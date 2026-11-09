/*
 * blekbd.c - turn Bluetooth LE HID reports from the ESP32 into input events.
 *
 * The firmware with ble_hid.c finds a BLE keyboard, pairs, subscribes to its
 * input reports and forwards them here over /dev/esp-ble. This daemon parses
 * the keyboard's HID report descriptor once, then decodes each report and
 * injects key events through /dev/uinput, which is what makes them reach the
 * virtual terminal.
 *
 * Why a descriptor parser instead of a fixed layout
 * ------------------------------------------------
 * The obvious shortcut -- assume the 8-byte boot keyboard report -- works for
 * basic typing and nothing else. A real Logitech keyboard sends several report
 * IDs: one for the keyboard array, one for consumer/media keys, sometimes a
 * vendor one for battery and Fn-lock. Which byte means what is described in the
 * HID report descriptor and nowhere else. Parsing it is the only way to get the
 * media keys, and it makes the daemon work with keyboards this was never tested
 * against.
 *
 * Frames from the firmware (see ble_hid.c):
 *
 *     0xA5 0x01 <len> <data...>        input report, verbatim
 *     0xA5 0x02 <lo> <hi> <map...>     HID report descriptor
 *     0xA5 0x03 <state>                link state
 *
 * A report that uses report IDs carries the ID as its first byte, so reports go
 * straight from the notification to the decoder.
 *
 * Design notes for this target:
 *   - NOMMU: one process, no fork, no threads.
 *   - The pipe is exclusive, so this owns it and issues the scan request.
 *   - uinput can only be created once the descriptor is known, so the input
 *     device appears when the first report map arrives.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#define DEV_PIPE	"/dev/esp-ble"
#define BOND_PATH	"/etc/ble-bond.bin"
#define DEV_UINPUT	"/dev/uinput"

#define FRAME_MAGIC	0xA5
#define FRAME_INPUT	0x01
#define FRAME_MAP	0x02
#define FRAME_STATE	0x03
#define FRAME_BOND	0x04

#define STATE_DISCONNECTED	0
#define STATE_CONNECTED		1
#define STATE_READY		2

#define MAP_MAX		1024
#define BOOT_KBD_LEN	8
#define MAX_FIELDS	256
#define MAX_USAGES	16

/* The ordinary keyboard keycode block. BTN_* starts at 0x100. */
#define KEY_MAX_ORD	255

/* ------------------------------------------------------------------ *
 * Logging
 * ------------------------------------------------------------------ */

static int verbose;
static int dry_run;		/* -n: decode and report, do not create a uinput device */
static const char *frame_file;	/* -f: play frames from a file instead of the pipe */

static void say(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

#define dbg(...) do { if (verbose) say(__VA_ARGS__); } while (0)


/*
 * The cross toolchain's <linux/input.h> predates most of these keycodes -- it is
 * a uClibc-era header, not the kernel's current one. The kernel running on the
 * watch is 7.x and knows them all, so the values below are what matter; the
 * fixed names just let this compile. Values taken from the kernel's
 * input-event-codes.h.
 */
#ifndef KEY_MUTE
#define KEY_MUTE 113
#define KEY_HELP 138
#define KEY_MENU 139
#define KEY_AGAIN 129
#define KEY_UNDO 131
#define KEY_COPY 133
#define KEY_PASTE 135
#define KEY_FIND 136
#define KEY_CUT 137
#define KEY_CALC 140
#define KEY_WWW 150
#define KEY_MAIL 155
#define KEY_COMPUTER 157
#define KEY_BACK 158
#define KEY_FORWARD 159
#define KEY_EJECTCD 161
#define KEY_NEXTSONG 163
#define KEY_PLAYPAUSE 164
#define KEY_PREVIOUSSONG 165
#define KEY_STOPCD 166
#define KEY_PHONE 169
#define KEY_CONFIG 171
#define KEY_HOMEPAGE 172
#define KEY_REFRESH 173
#define KEY_SEARCH 217
#define KEY_CANCEL 223
#define KEY_MEDIA 226
#define KEY_MICMUTE 248
#define KEY_SELECT 0x161
#define KEY_INFO 0x166
#define KEY_FAVORITES 0x16c
#define KEY_ZOOMIN 0x1a2
#define KEY_ZOOMOUT 0x1a3
#endif

/* Present in the kernel but not in the old header. */
#ifndef KEY_SYSRQ
#define KEY_SYSRQ 99
#endif
#ifndef KEY_SCROLLLOCK
#define KEY_SCROLLLOCK 70
#endif

/* ------------------------------------------------------------------ *
 * HID usage -> Linux keycode
 *
 * Only two pages matter for a keyboard: Keyboard/Keypad (0x07) and Consumer
 * (0x0C), which is where media and browser keys live.
 * ------------------------------------------------------------------ */

static unsigned short hid_kbd_to_key[256] = {
	[0x04] = KEY_A, [0x05] = KEY_B, [0x06] = KEY_C, [0x07] = KEY_D,
	[0x08] = KEY_E, [0x09] = KEY_F, [0x0A] = KEY_G, [0x0B] = KEY_H,
	[0x0C] = KEY_I, [0x0D] = KEY_J, [0x0E] = KEY_K, [0x0F] = KEY_L,
	[0x10] = KEY_M, [0x11] = KEY_N, [0x12] = KEY_O, [0x13] = KEY_P,
	[0x14] = KEY_Q, [0x15] = KEY_R, [0x16] = KEY_S, [0x17] = KEY_T,
	[0x18] = KEY_U, [0x19] = KEY_V, [0x1A] = KEY_W, [0x1B] = KEY_X,
	[0x1C] = KEY_Y, [0x1D] = KEY_Z,

	[0x1E] = KEY_1, [0x1F] = KEY_2, [0x20] = KEY_3, [0x21] = KEY_4,
	[0x22] = KEY_5, [0x23] = KEY_6, [0x24] = KEY_7, [0x25] = KEY_8,
	[0x26] = KEY_9, [0x27] = KEY_0,

	[0x28] = KEY_ENTER,     [0x29] = KEY_ESC,       [0x2A] = KEY_BACKSPACE,
	[0x2B] = KEY_TAB,       [0x2C] = KEY_SPACE,     [0x2D] = KEY_MINUS,
	[0x2E] = KEY_EQUAL,     [0x2F] = KEY_LEFTBRACE, [0x30] = KEY_RIGHTBRACE,
	[0x31] = KEY_BACKSLASH, [0x32] = KEY_BACKSLASH, [0x33] = KEY_SEMICOLON,
	[0x34] = KEY_APOSTROPHE,[0x35] = KEY_GRAVE,     [0x36] = KEY_COMMA,
	[0x37] = KEY_DOT,       [0x38] = KEY_SLASH,     [0x39] = KEY_CAPSLOCK,

	[0x3A] = KEY_F1,  [0x3B] = KEY_F2,  [0x3C] = KEY_F3,  [0x3D] = KEY_F4,
	[0x3E] = KEY_F5,  [0x3F] = KEY_F6,  [0x40] = KEY_F7,  [0x41] = KEY_F8,
	[0x42] = KEY_F9,  [0x43] = KEY_F10, [0x44] = KEY_F11, [0x45] = KEY_F12,

	[0x46] = KEY_SYSRQ, [0x47] = KEY_SCROLLLOCK, [0x48] = KEY_PAUSE,
	[0x49] = KEY_INSERT, [0x4A] = KEY_HOME,     [0x4B] = KEY_PAGEUP,
	[0x4C] = KEY_DELETE, [0x4D] = KEY_END,      [0x4E] = KEY_PAGEDOWN,
	[0x4F] = KEY_RIGHT,  [0x50] = KEY_LEFT,     [0x51] = KEY_DOWN,
	[0x52] = KEY_UP,

	[0x53] = KEY_NUMLOCK,   [0x54] = KEY_KPSLASH, [0x55] = KEY_KPASTERISK,
	[0x56] = KEY_KPMINUS,   [0x57] = KEY_KPPLUS,  [0x58] = KEY_KPENTER,
	[0x59] = KEY_KP1, [0x5A] = KEY_KP2, [0x5B] = KEY_KP3, [0x5C] = KEY_KP4,
	[0x5D] = KEY_KP5, [0x5E] = KEY_KP6, [0x5F] = KEY_KP7, [0x60] = KEY_KP8,
	[0x61] = KEY_KP9, [0x62] = KEY_KP0, [0x63] = KEY_KPDOT,

	[0x64] = KEY_102ND, [0x65] = KEY_COMPOSE,
	[0x67] = KEY_KPEQUAL,
	[0x68] = KEY_F13, [0x69] = KEY_F14, [0x6A] = KEY_F15, [0x6B] = KEY_F16,
	[0x6C] = KEY_F17, [0x6D] = KEY_F18, [0x6E] = KEY_F19, [0x6F] = KEY_F20,
	[0x70] = KEY_F21, [0x71] = KEY_F22, [0x72] = KEY_F23, [0x73] = KEY_F24,

	[0x75] = KEY_HELP, [0x76] = KEY_MENU,
	[0x77] = KEY_SELECT,  [0x79] = KEY_AGAIN,
	[0x7A] = KEY_UNDO,    [0x7B] = KEY_CUT,  [0x7C] = KEY_COPY,
	[0x7D] = KEY_PASTE,   [0x7E] = KEY_FIND, [0x7F] = KEY_MUTE,
	[0x80] = KEY_VOLUMEUP, [0x81] = KEY_VOLUMEDOWN,
	[0x85] = KEY_KPCOMMA,
	[0x87] = KEY_RO, [0x88] = KEY_KATAKANAHIRAGANA, [0x89] = KEY_YEN,
	[0x8A] = KEY_HENKAN, [0x8B] = KEY_KATAKANA, [0x8C] = KEY_HIRAGANA,
	[0x8D] = KEY_ZENKAKUHANKAKU, [0x8E] = KEY_KATAKANAHIRAGANA,
	[0x90] = KEY_HANGEUL, [0x91] = KEY_HANJA,

	[0xB6] = KEY_KPLEFTPAREN, [0xB7] = KEY_KPRIGHTPAREN,
	[0xE0] = KEY_LEFTCTRL,  [0xE1] = KEY_LEFTSHIFT,
	[0xE2] = KEY_LEFTALT,   [0xE3] = KEY_LEFTMETA,
	[0xE4] = KEY_RIGHTCTRL, [0xE5] = KEY_RIGHTSHIFT,
	[0xE6] = KEY_RIGHTALT,  [0xE7] = KEY_RIGHTMETA,
};

struct usage_key {
	unsigned short usage;
	unsigned short key;
};

/* The Consumer page. Not exhaustive -- it does not need to be. An unknown
 * usage costs nothing and is logged, so it can be added when one shows up. */
static const struct usage_key consumer_map[] = {
	{ 0x00B5, KEY_NEXTSONG },
	{ 0x00B6, KEY_PREVIOUSSONG },
	{ 0x00B7, KEY_STOPCD },
	{ 0x00B8, KEY_EJECTCD },
	{ 0x00CD, KEY_PLAYPAUSE },
	{ 0x00E2, KEY_MUTE },
	{ 0x00E5, KEY_SEARCH },
	{ 0x00E9, KEY_VOLUMEUP },
	{ 0x00EA, KEY_VOLUMEDOWN },
	{ 0x00E3, KEY_HOMEPAGE },
	{ 0x00E4, KEY_BACK },
	{ 0x00E6, KEY_HELP },
	{ 0x00E7, KEY_HELP },
	{ 0x0221, KEY_SEARCH },
	{ 0x0223, KEY_HOMEPAGE },
	{ 0x0224, KEY_BACK },
	{ 0x0225, KEY_FORWARD },
	{ 0x0226, KEY_STOP },
	{ 0x0227, KEY_REFRESH },
	{ 0x022A, KEY_FAVORITES },
	{ 0x0183, KEY_MEDIA },
	{ 0x018A, KEY_MAIL },
	{ 0x0192, KEY_CALC },
	{ 0x0194, KEY_COMPUTER },
	{ 0x0169, KEY_PHONE },
	{ 0x016F, KEY_CONFIG },
	{ 0x0171, KEY_INFO },
	{ 0x01A7, KEY_MICMUTE },
	{ 0x0186, KEY_WWW },
	{ 0x0189, KEY_WWW },
	{ 0x0170, KEY_WWW },
	{ 0x0196, KEY_WWW },
	{ 0x0198, KEY_STOP },
	{ 0x0152, KEY_CANCEL },
	{ 0x0126, KEY_ZOOMOUT },
	{ 0x0127, KEY_ZOOMIN },
};

static unsigned short usage_to_key(unsigned short page, unsigned short usage)
{
	unsigned int i;

	if (page == 0x07)
		return usage < 256 ? hid_kbd_to_key[usage] : 0;

	if (page == 0x0C) {
		for (i = 0; i < sizeof(consumer_map) / sizeof(consumer_map[0]);
		     i++)
			if (consumer_map[i].usage == usage)
				return consumer_map[i].key;
	}

	return 0;
}

/* ------------------------------------------------------------------ *
 * Report descriptor model
 * ------------------------------------------------------------------ */

struct field {
	uint8_t  report_id;
	uint8_t  type;		/* 0 = input, 1 = output, 2 = feature */
	uint16_t bit_offset;
	uint16_t bit_size;
	uint16_t count;
	uint8_t  is_array;
	uint8_t  is_constant;
	uint16_t usage_page;
	int32_t  logical_min;
	uint16_t usages[MAX_USAGES];	/* variable fields */
	uint16_t usage_min, usage_max;	/* array fields */
};

static struct field fields[MAX_FIELDS];
static int nfields;
static int have_map;

static unsigned int item_u32(const uint8_t *p, int size)
{
	unsigned int v = 0;

	if (size >= 1) v |= p[0];
	if (size >= 2) v |= (unsigned int)p[1] << 8;
	if (size >= 4) v |= (unsigned int)p[2] << 16 |
			     (unsigned int)p[3] << 24;
	return v;
}

static int32_t item_s32(const uint8_t *p, int size)
{
	unsigned int v = item_u32(p, size);

	if (size == 1 && (v & 0x80))   return (int32_t)(v | 0xFFFFFF00u);
	if (size == 2 && (v & 0x8000)) return (int32_t)(v | 0xFFFF0000u);
	return (int32_t)v;
}

struct global_state {
	uint16_t usage_page;
	int32_t  logical_min, logical_max;
	uint16_t report_size, report_count;
	uint8_t  report_id;
	uint32_t input_bit[256];
	uint32_t output_bit[256];
	uint32_t feature_bit[256];
};

static void parse_report_map(const uint8_t *d, int len)
{
	struct global_state s, stack[4];
	uint16_t local_usages[MAX_USAGES];
	int n_local = 0;
	uint16_t local_usage_min = 0, local_usage_max = 0;
	int have_range = 0;
	uint16_t local_page = 0;
	int depth = 0;
	int i = 0;

	memset(&s, 0, sizeof(s));
	memset(stack, 0, sizeof(stack));
	memset(fields, 0, sizeof(fields));
	nfields = 0;

	while (i < len) {
		uint8_t prefix = d[i];
		int size, type, tag;
		const uint8_t *arg;
		unsigned int u;

		if (prefix == 0xFE) {		/* long item */
			if (i + 3 > len)
				break;
			i += 3 + d[i + 1];
			continue;
		}

		size = prefix & 0x03;
		if (size == 3)
			size = 4;
		type = (prefix >> 2) & 0x03;
		tag  = prefix >> 4;

		if (i + 1 + size > len)
			break;
		arg = d + i + 1;
		u = item_u32(arg, size);

		if (type == 0) {
			/* Main item */
			if (tag == 0x8 || tag == 0x9 || tag == 0xB) {
				uint32_t *cursor;
				int ftype;

				if (tag == 0x8) {
					cursor = &s.input_bit[s.report_id];
					ftype = 0;
				} else if (tag == 0x9) {
					cursor = &s.output_bit[s.report_id];
					ftype = 1;
				} else {
					cursor = &s.feature_bit[s.report_id];
					ftype = 2;
				}

				if (nfields < MAX_FIELDS && s.report_count &&
				    s.report_size) {
					struct field *f = &fields[nfields];
					int is_const = u & 0x01;
					int is_var   = u & 0x02;

					f->report_id   = s.report_id;
					f->type        = ftype;
					f->bit_offset  = (uint16_t)*cursor;
					f->bit_size    = s.report_size;
					f->count       = s.report_count;
					f->is_constant = is_const;
					f->is_array    = !is_var && !is_const;
					f->logical_min = s.logical_min;
					f->usage_page  = local_page ?
							 local_page :
							 s.usage_page;

					if (is_var) {
						int k;
						for (k = 0; k < s.report_count &&
						     k < MAX_USAGES; k++) {
							if (k < n_local)
								f->usages[k] =
								  local_usages[k];
							else if (have_range)
								f->usages[k] =
								  local_usage_min + k;
							else if (n_local)
								f->usages[k] =
								  local_usages[0];
						}
					} else if (have_range) {
						f->usage_min = local_usage_min;
						f->usage_max = local_usage_max;
					} else if (n_local) {
						f->usage_min = local_usages[0];
						f->usage_max = local_usages[0];
					}

					/* Constants carry padding and the report-ID
					 * byte itself; they consume bits but map to
					 * nothing. */
					if (!is_const)
						nfields++;
				}

				*cursor += (uint32_t)s.report_size *
					   s.report_count;
			}

			n_local = 0;
			have_range = 0;
			local_page = 0;

		} else if (type == 1) {
			/* Global item */
			switch (tag) {
			case 0x0: s.usage_page = u; break;
			case 0x1: s.logical_min = item_s32(arg, size); break;
			case 0x2: s.logical_max = item_s32(arg, size); break;
			case 0x7: s.report_size = u; break;
			case 0x8: s.report_id = u & 0xFF; break;
			case 0x9: s.report_count = u; break;
			case 0xA: if (depth < 4) stack[depth++] = s; break;
			case 0xB: if (depth > 0) s = stack[--depth]; break;
			default: break;
			}

		} else if (type == 2) {
			/* Local item */
			switch (tag) {
			case 0x0:	/* Usage */
				/* A 4-byte Usage carries the page in its high
				 * half; anything else uses the current page. */
				if (size == 4) {
					local_page = (u >> 16) & 0xFFFF;
					if (n_local < MAX_USAGES)
						local_usages[n_local++] =
							u & 0xFFFF;
				} else if (n_local < MAX_USAGES) {
					local_usages[n_local++] = u & 0xFFFF;
				}
				break;
			case 0x1:	/* Usage Minimum */
				local_usage_min = u & 0xFFFF;
				have_range = 1;
				break;
			case 0x2:	/* Usage Maximum */
				local_usage_max = u & 0xFFFF;
				have_range = 1;
				break;
			default:
				break;
			}
		}

		i += 1 + size;
	}

	say("blekbd: report descriptor parsed: %d field(s)", nfields);

	if (verbose) {
		int off;

		for (off = 0; off < len; off += 16) {
			char line[100];
			int n = 0, k;

			n += snprintf(line + n, sizeof(line) - n,
				      "blekbd:   %04x:", off);
			for (k = 0; k < 16 && off + k < len; k++)
				n += snprintf(line + n, sizeof(line) - n,
					      " %02x", d[off + k]);
			say("%s", line);
		}
	}

	if (verbose) {
		for (i = 0; i < nfields; i++) {
			struct field *f = &fields[i];

			say("  field %d: id=%u type=%u off=%u size=%u "
			    "count=%u %s page=0x%02x usages=0x%04x..0x%04x",
			    i, f->report_id, f->type, f->bit_offset,
			    f->bit_size, f->count,
			    f->is_array ? "array" : "var",
			    f->usage_page, f->usage_min, f->usage_max);
		}
	}
}

/* ------------------------------------------------------------------ *
 * uinput
 * ------------------------------------------------------------------ */

static int ufd = -1;

static int uinput_open(void)
{
	struct uinput_setup usetup;
	int fd, i;

	if (dry_run) {
		say("blekbd: dry run, no uinput device");
		return 0;
	}

	fd = open(DEV_UINPUT, O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open " DEV_UINPUT);
		return -1;
	}

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
	    ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0 ||
	    ioctl(fd, UI_SET_EVBIT, EV_REP) < 0) {
		perror("UI_SET_EVBIT");
		close(fd);
		return -1;
	}

	/*
	 * Declare the whole ordinary keyboard range rather than exactly the keys
	 * the descriptor mentions. It costs one page of bits and it cannot be
	 * wrong: a missing bit silently swallows a key, and the mistake only
	 * shows up later as "that key does nothing". 1..255 is the keyboard
	 * block; BTN_* begins at 0x100, and declaring those would make this look
	 * like a mouse or joystick to anything that inspects the device.
	 */
	for (i = 1; i <= KEY_MAX_ORD; i++)
		ioctl(fd, UI_SET_KEYBIT, i);

	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_BLUETOOTH;
	usetup.id.vendor  = 0x046d;	/* Logitech */
	usetup.id.product = 0xb342;
	usetup.id.version = 1;
	snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "BLE Keyboard");

	if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0) {
		perror("UI_DEV_SETUP");
		close(fd);
		return -1;
	}
	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		perror("UI_DEV_CREATE");
		close(fd);
		return -1;
	}

	say("blekbd: input device created (/dev/input, \"BLE Keyboard\")");
	return fd;
}

static void emit(int fd, int type, int code, int value)
{
	struct input_event ev;

	memset(&ev, 0, sizeof(ev));
	gettimeofday(&ev.time, NULL);
	ev.type = type;
	ev.code = code;
	ev.value = value;

	if (write(fd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev))
		perror("write uinput");
}

/* ------------------------------------------------------------------ *
 * report -> events
 * ------------------------------------------------------------------ */

/* Keys currently down. HID reports carry the whole held set every time, so a
 * key that was present and is now absent has been released. */
static unsigned char held[256];

/* A readable name for the keys this project cares about, so -n reports what
 * would be typed rather than a number. */
static const char *key_name(unsigned short code)
{
	static const struct { unsigned short code; const char *name; } t[] = {
		{ KEY_A, "KEY_A" }, { KEY_B, "KEY_B" }, { KEY_C, "KEY_C" },
		{ KEY_D, "KEY_D" }, { KEY_E, "KEY_E" }, { KEY_F, "KEY_F" },
		{ KEY_G, "KEY_G" }, { KEY_H, "KEY_H" }, { KEY_I, "KEY_I" },
		{ KEY_J, "KEY_J" }, { KEY_K, "KEY_K" }, { KEY_L, "KEY_L" },
		{ KEY_M, "KEY_M" }, { KEY_N, "KEY_N" }, { KEY_O, "KEY_O" },
		{ KEY_P, "KEY_P" }, { KEY_Q, "KEY_Q" }, { KEY_R, "KEY_R" },
		{ KEY_S, "KEY_S" }, { KEY_T, "KEY_T" }, { KEY_U, "KEY_U" },
		{ KEY_V, "KEY_V" }, { KEY_W, "KEY_W" }, { KEY_X, "KEY_X" },
		{ KEY_Y, "KEY_Y" }, { KEY_Z, "KEY_Z" },
		{ KEY_ENTER, "KEY_ENTER" }, { KEY_ESC, "KEY_ESC" },
		{ KEY_BACKSPACE, "KEY_BACKSPACE" }, { KEY_TAB, "KEY_TAB" },
		{ KEY_SPACE, "KEY_SPACE" },
		{ KEY_LEFTSHIFT, "KEY_LEFTSHIFT" },
		{ KEY_RIGHTSHIFT, "KEY_RIGHTSHIFT" },
		{ KEY_LEFTCTRL, "KEY_LEFTCTRL" }, { KEY_LEFTALT, "KEY_LEFTALT" },
		{ KEY_LEFTMETA, "KEY_LEFTMETA" },
		{ KEY_VOLUMEUP, "KEY_VOLUMEUP" },
		{ KEY_VOLUMEDOWN, "KEY_VOLUMEDOWN" },
		{ KEY_MUTE, "KEY_MUTE" }, { KEY_PLAYPAUSE, "KEY_PLAYPAUSE" },
		{ KEY_NEXTSONG, "KEY_NEXTSONG" },
		{ KEY_PREVIOUSSONG, "KEY_PREVIOUSSONG" },
	};
	unsigned int i;

	for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
		if (t[i].code == code)
			return t[i].name;
	return NULL;
}

static void set_key(unsigned short code, int down)
{
	const char *name;

	if (!code || code > KEY_MAX_ORD)
		return;
	if (!!held[code] == !!down)
		return;

	held[code] = down ? 1 : 0;
	name = key_name(code);
	/* Logged whenever -v is on, not only in dry-run: this line is the only
	 * direct evidence that a decoded key reached the input device. */
	if (verbose)
		say("blekbd:   key %s %s", name ? name : "(unnamed)",
		    down ? "down" : "up");
	if (ufd > 0)
		emit(ufd, EV_KEY, code, down ? 1 : 0);
}

/* Read `size` bits starting at bit `offset`, least significant bit first. */
static uint32_t bits_at(const uint8_t *data, int len, int offset, int size)
{
	uint32_t v = 0;
	int i;

	if (size <= 0 || size > 32)
		return 0;

	for (i = 0; i < size; i++) {
		int o = offset + i;
		int byte = o >> 3;
		int bit  = o & 7;

		if (byte >= len)
			break;
		if (data[byte] & (1 << bit))
			v |= 1u << i;
	}

	return v;
}

static void handle_report(const uint8_t *data, int len)
{
	unsigned char want[256];
	uint8_t report_id = 0;
	int uses_ids = 0;
	int bias = 0;
	int i;

	if (len < 1)
		return;

	for (i = 0; i < nfields; i++)
		if (fields[i].report_id) {
			uses_ids = 1;
			break;
		}

	/* If any report in the descriptor has an ID, every report on the wire
	 * starts with it. Taken from the model, not guessed from a length. */
	if (uses_ids) {
		report_id = data[0];
		/*
		 * Bit offsets in the descriptor are counted from the start of the
		 * report PAYLOAD, but the bytes handed in here include the report
		 * ID as data[0] -- the firmware forwards the notification
		 * verbatim. Everything shifts by one byte.
		 */
		bias = 8;
	}

	memset(want, 0, sizeof(want));

	/*
	 * Fallback: the standard boot keyboard layout, used when the report map
	 * did not arrive or did not describe this report.
	 *
	 * The firmware fetches only the first part of the descriptor -- enough
	 * for the keyboard collection, which is all typing needs -- and that read
	 * can be cut short. Rather than make typing depend on a 934-byte ATT
	 * transfer over a shared radio, decode the one layout that is fixed by
	 * the HID specification: byte 0 modifiers, byte 1 reserved, then six
	 * usage slots. With report IDs in use the ID is byte 0 and the layout
	 * shifts by one.
	 */
	if (nfields == 0 || len == BOOT_KBD_LEN) {
		int base = 0;
		int nslots;

		if (len == BOOT_KBD_LEN) {
			/* Boot protocol: no report ID, the layout is fixed. */
			base = 0;
		} else if (len == BOOT_KBD_LEN + 1 && data[0] == 1) {
			/* Same layout behind a report ID, seen when a device
			 * ignores boot protocol. */
			base = 1;
		} else {
			return;		/* not a keyboard report */
		}

		nslots = len - base - 2;
		if (nslots > 6)
			nslots = 6;

		for (i = 0; i < 8; i++)
			if (data[base] & (1 << i))
				want[hid_kbd_to_key[0xE0 + i]] = 1;

		for (i = 0; i < nslots; i++) {
			unsigned short k = hid_kbd_to_key[data[base + 2 + i]];

			if (k)
				want[k] = 1;
		}

		goto emit;
	}

	for (i = 0; i < nfields; i++) {
		struct field *f = &fields[i];
		int k;

		if (f->type != 0 || f->report_id != report_id)
			continue;

		if (f->is_array) {
			for (k = 0; k < f->count; k++) {
				int off = f->bit_offset + bias +
					  k * f->bit_size;
				uint32_t v = bits_at(data, len, off,
						     f->bit_size);
				int32_t usage;
				unsigned short key;

				if (!v)
					continue;	/* 0 = empty slot */

				/* Array entries index into
				 * [usage_min .. usage_max], offset by the
				 * logical minimum. */
				usage = (int32_t)v - f->logical_min +
					(int32_t)f->usage_min;

				if (usage <= 0 || usage > 0xFFFF)
					continue;

				key = usage_to_key(f->usage_page,
						   (unsigned short)usage);
				if (!key) {
					dbg("blekbd: unmapped usage "
					    "page=0x%02x usage=0x%04x",
					    f->usage_page, (unsigned)usage);
					continue;
				}
				if (key <= KEY_MAX_ORD)
					want[key] = 1;
			}
		} else {
			/* Variable: control k carries usages[k]. */
			for (k = 0; k < f->count && k < MAX_USAGES; k++) {
				int off = f->bit_offset + bias +
					  k * f->bit_size;
				uint32_t v = bits_at(data, len, off,
						     f->bit_size);
				unsigned short key;

				if (!v)
					continue;

				key = usage_to_key(f->usage_page, f->usages[k]);
				if (!key)
					continue;
				if (key <= KEY_MAX_ORD)
					want[key] = 1;
			}
		}
	}

emit:
	/* Releases first, then presses. */
	for (i = 0; i < 256; i++)
		if (held[i] && !want[i])
			set_key(i, 0);
	for (i = 0; i < 256; i++)
		if (!held[i] && want[i])
			set_key(i, 1);

	if (ufd > 0)
		emit(ufd, EV_SYN, SYN_REPORT, 0);

	if (verbose) {
		char b[240];
		int n = 0;

		n += snprintf(b + n, sizeof(b) - n,
			      "blekbd: report id=%u len=%d:",
			      report_id, len);
		for (i = 0; i < len && n < (int)sizeof(b) - 4; i++)
			n += snprintf(b + n, sizeof(b) - n, " %02x", data[i]);
		say("%s", b);
	}
}

/* ------------------------------------------------------------------ *
 * main
 * ------------------------------------------------------------------ */

/*
 * Persist the bond the firmware just handed us.
 *
 * Written to a temporary file and renamed, because a partially written bond is
 * worse than none: the firmware would load half a key and fail to authenticate
 * on every subsequent boot. rename() on jffs2 is atomic enough for that.
 */
static void save_bond(const unsigned char *p, int len)
{
	char tmp[] = BOND_PATH ".tmp";
	int fd;

	fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0) {
		perror(tmp);
		return;
	}

	if (write(fd, p, len) != len) {
		perror("write bond");
		close(fd);
		unlink(tmp);
		return;
	}

	if (fsync(fd) < 0)
		perror("fsync bond");
	close(fd);

	if (rename(tmp, BOND_PATH) < 0) {
		perror("rename bond");
		unlink(tmp);
		return;
	}

	say("blekbd: bond saved (%d bytes)", len);
}

static unsigned char rx_buf[2048];
static size_t rx_have;

/*
 * Consume every complete frame sitting in rx_buf.
 *
 * Split out of main so the same code can be driven from a recorded frame file
 * (see -f) -- which is how the descriptor parser gets tested without a keyboard
 * in the room.
 */
static void consume_frames(void)
{
	size_t pos = 0;

	while (pos < rx_have) {
		size_t avail = rx_have - pos;

		if (rx_buf[pos] != FRAME_MAGIC) {
			/* Provisioning text or noise. */
			pos++;
			continue;
		}
		if (avail < 2)
			break;

		if (rx_buf[pos + 1] == FRAME_INPUT) {
			int len;

			if (avail < 3)
				break;
			len = rx_buf[pos + 2];
			if (avail < (size_t)(3 + len))
				break;

			/* The input device is created on the first report, not on
			 * the report map.
			 *
			 * It used to be created in the map path only, which silently
			 * stopped working the moment the firmware switched to boot
			 * protocol -- boot protocol sends no map, so ufd stayed -1
			 * and every decode was handed to a device that did not
			 * exist. Reports arrived, keys were parsed, and nothing was
			 * ever typed. A missing uinput fd is not an error anywhere
			 * downstream, which is why it failed invisibly.
			 */
			if (ufd < 0) {
				ufd = uinput_open();
				if (ufd < 0)
					say("blekbd: cannot create the input "
					    "device; keys will not be typed");
			}

			handle_report(rx_buf + pos + 3, len);
			pos += 3 + len;

		} else if (rx_buf[pos + 1] == FRAME_MAP) {
			int len;

			if (avail < 4)
				break;
			len = rx_buf[pos + 2] | (rx_buf[pos + 3] << 8);
			if (avail < (size_t)(4 + len))
				break;

			if (len <= MAP_MAX) {
				parse_report_map(rx_buf + pos + 4, len);
				have_map = 1;
				if (ufd < 0)
					ufd = uinput_open();
			} else {
				say("blekbd: report map too large (%d bytes)",
				    len);
			}
			pos += 4 + len;

		} else if (rx_buf[pos + 1] == FRAME_BOND) {
			int len;

			if (avail < 4)
				break;
			len = rx_buf[pos + 2] | (rx_buf[pos + 3] << 8);
			if (avail < (size_t)(4 + len))
				break;

			save_bond(rx_buf + pos + 4, len);
			pos += 4 + len;

		} else if (rx_buf[pos + 1] == FRAME_STATE) {
			if (avail < 3)
				break;
			switch (rx_buf[pos + 2]) {
			case STATE_CONNECTED:
				say("blekbd: keyboard connected");
				break;
			case STATE_READY:
				say("blekbd: keyboard ready");
				break;
			case STATE_DISCONNECTED:
				say("blekbd: keyboard disconnected");
				if (ufd > 0)
					memset(held, 0, sizeof(held));
				break;
			}
			pos += 3;

		} else {
			pos++;
		}
	}

	if (pos) {
		memmove(rx_buf, rx_buf + pos, rx_have - pos);
		rx_have -= pos;
	}
}

int main(int argc, char **argv)
{
	int pfd;
	int i;
	time_t last_req = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v"))
			verbose = 1;
		else if (!strcmp(argv[i], "-n"))
			dry_run = 1;
		else if (!strcmp(argv[i], "-f") && i + 1 < argc)
			frame_file = argv[++i];
	}

	if (frame_file) {
		/* Playback: feed recorded frames through exactly the decoder the
		 * device path uses, print what would be typed, and exit. */
		int fd = open(frame_file, O_RDONLY);
		ssize_t n;

		if (fd < 0) {
			perror(frame_file);
			return 1;
		}
		dry_run = 1;
		while ((n = read(fd, rx_buf + rx_have,
				 sizeof(rx_buf) - rx_have)) > 0) {
			rx_have += n;
			consume_frames();
		}
		close(fd);
		say("blekbd: playback complete");
		return 0;
	}

	/*
	 * The pipe takes a single reader, and something else legitimately holds it
	 * for a while: /usr/sbin/ble-wifi-setup runs the WiFi provisioning dialog
	 * on the same device. At boot, S46blewifi starts before this and has to be
	 * allowed to finish or give up.
	 *
	 * So retry instead of exiting. Exiting on EBUSY looked acceptable and was
	 * not: the daemon was started once, lost the race, and the keyboard stayed
	 * dead for the whole session with no sign of why.
	 */
	{
		int waited = 0;

		for (;;) {
			pfd = open(DEV_PIPE, O_RDWR);
			if (pfd >= 0)
				break;

			if (errno != EBUSY || waited >= 120) {
				perror("open " DEV_PIPE);
				say("Is the firmware running, and is something "
				    "else (ble-wifi-setup) holding the pipe?");
				return 1;
			}

			if (!waited)
				say("blekbd: %s busy, waiting for it to be "
				    "released", DEV_PIPE);
			sleep(2);
			waited += 2;
		}

		if (waited)
			say("blekbd: got %s after %d s", DEV_PIPE, waited);
	}

	say("blekbd: reading %s", DEV_PIPE);

	/* Create the input device up front as well. Waiting for the first
	 * report would work, but a device that exists before the keyboard is
	 * even connected is one less thing between a keypress and a character,
	 * and it makes "is it there?" answerable at any time. */
	if (ufd < 0) {
		ufd = uinput_open();
		if (ufd < 0)
			say("blekbd: %s unavailable; keys will be decoded but "
			    "not typed", DEV_UINPUT);
	}

	/* Ask the firmware for a discovery window now, and again periodically
	 * while no keyboard is attached -- the firmware's scans are bounded. */
	if (write(pfd, "scan", 4) != 4 && errno != EAGAIN)
		perror("write scan");
	last_req = time(NULL);

	for (;;) {
		struct timeval tv;
		fd_set rfds;
		ssize_t n;

		/*
		 * The retry is timed here rather than with poll(): the kernel's
		 * esp_ble_prov_poll() returns EPOLLOUT unconditionally, so a
		 * readiness test comes back immediately for ever and a timeout
		 * on it never fires.
		 */
		gettimeofday(&tv, NULL);
		if (tv.tv_sec - last_req >= 20) {
			last_req = tv.tv_sec;
			if (write(pfd, "scan", 4) != 4 && errno != EAGAIN)
				perror("write scan");
		}

		FD_ZERO(&rfds);
		FD_SET(pfd, &rfds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if (select(pfd + 1, &rfds, NULL, NULL, &tv) <= 0)
			continue;

		n = read(pfd, rx_buf + rx_have, sizeof(rx_buf) - rx_have);
		if (n < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			perror("read pipe");
			break;
		}
		if (n == 0)
			continue;

		rx_have += n;
		consume_frames();

		if (rx_have >= sizeof(rx_buf))
			rx_have = 0;	/* pathological: resynchronise */
	}

	if (ufd > 0) {
		ioctl(ufd, UI_DEV_DESTROY);
		close(ufd);
	}
	close(pfd);
	return 0;
}
