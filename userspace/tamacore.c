/*
 * tamacore.c - core 1's half of the Tamagotchi: the blitter and the buttons.
 *
 * The guest runs on core 0 now (firmware tamacore_task.c) because core 0 has a
 * data cache and core 1 does not. What is left for Linux is the part it is
 * genuinely good at: pushing pixels to a panel and reading input devices.
 *
 * Responsibilities, in the order they matter:
 *
 *   1. Load the ROM and the save image into the shared PSRAM window, once, then
 *      tell core 0 it may start. Core 0 cannot read the filesystem and must not
 *      allocate PSRAM, so this is the only way it gets a ROM.
 *   2. Read frames out of the window at whatever rate core 0 publishes them and
 *      put them on the glass with a single write() of a full-width strip. No
 *      bands, no dirty tracking -- all of that moved to the side with a cache,
 *      and what is left is one transfer per frame.
 *   3. Push button state the other way, and persist the save image when core 0
 *      says it has changed.
 *
 * The window layout and every field here come from shmproto.h, which the
 * firmware includes too. There is one contract, not two copies of one.
 *
 * A note on why this exists at all: the emulator used to run here and managed
 * 1.01x in play but 0.52x once the status menu started filling the screen,
 * because every emulated instruction costs ~464 cycles from uncached PSRAM.
 * No interpreter change can fix that. Moving the work to the core with a cache
 * can. See docs/cores.md and docs/tamagotchi.md.
 */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "shmproto.h"

/* ---- fixed paths --------------------------------------------------------- */
#define ROM_PATH	"/home/rom.bin"
#define SAVE_PATH	"/home/tama.sav"
#define BTN_DEV_NAME	"touch-buttons"

/* How often to look for buttons and frames. 4 ms keeps a tap from landing
 * between samples without spinning; the guest itself runs at 16 fps. */
#define POLL_US		4000

static int g_shared = -1;
static int g_fb = -1;
static int g_btn = -1;
static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static uint64_t now_us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

static void note(const char *msg)
{
	/* stderr, not the kernel log: this is a userspace program and the kernel
	 * log is where driver complaints belong. */
	fprintf(stderr, "tamacore: %s\n", msg);
}

static int shared_write(unsigned long off, const void *buf, size_t len)
{
	ssize_t n = pwrite(g_shared, buf, len, (off_t)off);

	return (n == (ssize_t)len) ? 0 : -1;
}

static int shared_read(unsigned long off, void *buf, size_t len)
{
	ssize_t n = pread(g_shared, buf, len, (off_t)off);

	return (n == (ssize_t)len) ? 0 : -1;
}

/* ---- input --------------------------------------------------------------- */

static int input_open(void)
{
	char path[64], name[64];
	int i;

	for (i = 0; i < 32; i++) {
		FILE *f;

		snprintf(path, sizeof(path),
			 "/sys/class/input/event%d/device/name", i);
		f = fopen(path, "r");
		if (!f)
			continue;
		if (fgets(name, sizeof(name), f)) {
			name[strcspn(name, "\n")] = 0;
			if (strcmp(name, BTN_DEV_NAME) == 0) {
				fclose(f);
				snprintf(path, sizeof(path), "/dev/input/event%d", i);
				return open(path, O_RDONLY | O_NONBLOCK);
			}
		}
		fclose(f);
	}
	return -1;
}

/* Drain the queue into a button bitmask. Returns the number of events applied. */
static int input_poll(uint32_t *mask)
{
	struct input_event ev;
	int n = 0;

	if (g_btn < 0)
		return 0;

	while (read(g_btn, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
		uint32_t bit;

		if (ev.type != EV_KEY || ev.code < BTN_0 || ev.code > BTN_2)
			continue;

		bit = 1u << (ev.code - BTN_0);
		if (ev.value)
			*mask |= bit;
		else
			*mask &= ~bit;
		n++;
	}
	return n;
}

/* ---- panel --------------------------------------------------------------- */

/*
 * One full-width strip per frame, written with write() to /dev/fb0.
 *
 * CO5300_IOC_WRITE_RECT is not available here and it is not a fallback that was
 * missed: the driver refuses any rectangle that intersects fb0, on purpose, so
 * that a userspace bug cannot paint over the console. The game lives *inside*
 * fb0, so the only path onto the glass is fb_write().
 *
 * fb_write() blits every row it touches, full width, in one transfer. So the
 * frame is composed into a 320-wide strip and written once: one pwrite, one
 * band, which is also the transfer shape the panel handles best. The strip is
 * the LCD plus its bezel; text above and below it is left alone.
 */
static uint16_t g_strip[SHM_FB_W * SHM_FRAME_H];

static int panel_blit(const uint16_t *tile)
{
	int y;

	/* Bezel across the strip, then the tile in the middle of it. */
	for (y = 0; y < SHM_FB_W * SHM_FRAME_H; y++)
		g_strip[y] = SHM_LCD_OFF;

	for (y = 0; y < SHM_FRAME_H; y++)
		memcpy(&g_strip[(size_t)y * SHM_FB_W + SHM_ORIGIN_X],
		       &tile[(size_t)y * SHM_FRAME_W],
		       SHM_FRAME_W * sizeof(uint16_t));

	return pwrite(g_fb, g_strip, sizeof(g_strip),
		      (off_t)SHM_ORIGIN_Y * SHM_FB_W * 2) == (ssize_t)sizeof(g_strip)
		       ? 0 : -1;
}

/* ---- startup ------------------------------------------------------------- */

/*
 * Put the ROM in the window. This is the reason core 0 needs no filesystem and
 * no PSRAM heap: 512 KB copied once, at a fixed address, from the same file the
 * userspace emulator used to read.
 */
static int seed_rom(void)
{
	static uint8_t chunk[16384];
	unsigned long off = 0;
	int fd = open(ROM_PATH, O_RDONLY);

	if (fd < 0) {
		fprintf(stderr, "tamacore: %s: %s\n", ROM_PATH, strerror(errno));
		return -1;
	}

	while (off < SHM_ROM_BYTES) {
		size_t want = sizeof(chunk);
		ssize_t n;

		if (want > SHM_ROM_BYTES - off)
			want = SHM_ROM_BYTES - off;

		n = read(fd, chunk, want);
		if (n <= 0) {
			close(fd);
			note("rom is short; refusing to hand core 0 a partial image");
			return -1;
		}

		if (shared_write(SHM_ROM_OFF + off, chunk, (size_t)n) != 0) {
			close(fd);
			note("write to the ROM window failed");
			return -1;
		}
		off += (unsigned long)n;
	}

	close(fd);

	/* The signature is core 0's to check, before it starts the machine: it
	 * reads the window, so it sees the same bytes this just wrote. */
	return 0;
}

static int seed_eeprom(void)
{
	uint8_t buf[SHM_EEPROM_BYTES];
	FILE *f;
	int have = 0;

	memset(buf, 0, sizeof(buf));
	f = fopen(SAVE_PATH, "rb");
	if (f) {
		have = (fread(buf, 1, sizeof(buf), f) == sizeof(buf));
		fclose(f);
	}

	if (shared_write(SHM_EEPROM_OFF, buf, sizeof(buf)) != 0) {
		note("write to the save image failed");
		return -1;
	}

	return have ? 1 : 0;
}

static int persist_eeprom(void)
{
	uint8_t buf[SHM_EEPROM_BYTES];
	FILE *f;
	int ret = 0;

	if (shared_read(SHM_EEPROM_OFF, buf, sizeof(buf)) != 0)
		return -1;

	f = fopen(SAVE_PATH, "wb");
	if (!f)
		return -1;
	if (fwrite(buf, 1, sizeof(buf), f) != sizeof(buf))
		ret = -1;
	fclose(f);
	return ret;
}

/* Set one 32-bit control field. The control block is written field by field so
 * this side never replaces a word the other side owns. */
static int set_field(unsigned long off, uint32_t value)
{
	return shared_write(off, &value, sizeof(value));
}

int main(int argc, char **argv)
{
	struct shm_ctl ctl = { 0 };
	uint16_t frame[SHM_FRAME_W * SHM_FRAME_H];
	uint32_t mask = 0, sent_mask = 0;
	uint32_t last_seq = 0, last_eeprom = 0;
	unsigned long frames = 0;
	int quiet = 0, bench = 0, i;
	double seconds = 0.0;
	time_t started;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-q") == 0)
			quiet = 1;
		else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
			bench = atoi(argv[++i]);
		else if (argv[i][0] != '-')
			seconds = atof(argv[i]);
	}

	/* Unbuffered, because this program is normally run with its output on a
	 * pipe or redirected, and a buffered summary that never flushes is how a
	 * working run looked like a silent hang. */
	setvbuf(stdout, NULL, _IONBF, 0);

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	g_shared = open("/proc/esp-shared", O_RDWR);
	if (g_shared < 0) {
		fprintf(stderr, "tamacore: /proc/esp-shared: %s\n", strerror(errno));
		fprintf(stderr, "tamacore: is the esp_shared driver built in? "
			"(CONFIG_ESP_SHARED)\n");
		return 1;
	}

	g_fb = open("/dev/fb0", O_RDWR);
	if (g_fb < 0) {
		fprintf(stderr, "tamacore: /dev/fb0: %s\n", strerror(errno));
		return 1;
	}

	g_btn = input_open();
	if (g_btn < 0)
		note("no touch-buttons device; the bars will do nothing");

	/*
	 * -b N: time N strip writes and stop.
	 *
	 * The panel is write-only and never acknowledges, so the only honest way
	 * to know what a frame costs is to write one repeatedly and time it. This
	 * exists because the first working build blitted 77 of 159 published
	 * frames, and the number that matters is the one measured here rather
	 * than inferred from a ratio.
	 */
	if (bench > 0) {
		uint64_t t0, t1;
		int n;

		memset(g_strip, 0, sizeof(g_strip));
		t0 = now_us();
		for (n = 0; n < bench; n++) {
			if (panel_blit(g_strip) < 0) {
				fprintf(stderr, "tamacore: bench blit: %s\n",
					strerror(errno));
				return 1;
			}
		}
		t1 = now_us();
		printf("tamacore: %d strip writes of %zu bytes: %.3f ms each\n",
		       bench, sizeof(g_strip), (double)(t1 - t0) / 1000.0 / bench);
		return 0;
	}

	/*
	 * Wait for core 0 to take ownership of the control block.
	 *
	 * The window survives a reset, so nothing here can be trusted until the
	 * guest has cleared it and said who it is. Ordering the handshake around
	 * that one marker is what makes stale flags from a previous boot
	 * impossible to mistake for a handshake.
	 */
	{
		int waited;

		for (waited = 0; waited < 500; waited++) {
			if (shared_read(SHM_CTL_OFF, &ctl, sizeof(ctl)) == 0 &&
			    ctl.magic == SHM_MAGIC && ctl.version == SHM_VERSION)
				break;
			usleep(20000);
		}
		if (ctl.magic != SHM_MAGIC || ctl.version != SHM_VERSION) {
			note("core 0 never came up; is the firmware running?");
			return 1;
		}
	}

	/*
	 * Zero the control line this side owns -- the ready flags, the button
	 * mask, quit -- before setting any of it.
	 *
	 * PSRAM is not zeroed at power-on. A stale `buttons` word from a
	 * previous boot reads as a random mask, and core 0 would apply it as
	 * phantom button presses; a stale `quit` would stop the guest before it
	 * started. Both were observed. Each side therefore initialises its own
	 * line, which keeps the one-writer rule and needs no lock.
	 */
	{
		uint8_t zero[32] = { 0 };

		if (shared_write(SHM_CTL_OFF + 32, zero, sizeof(zero)) != 0) {
			note("could not clear the control line");
			return 1;
		}
	}

	/* Order matters: the save image and the ROM are both in place before
	 * either ready flag goes up, and core 0 waits for both. */
	if (seed_eeprom() < 0)
		return 1;
	if (seed_rom() < 0)
		return 1;

	if (set_field((unsigned long)offsetof(struct shm_ctl, eeprom_ready), 1) ||
	    set_field((unsigned long)offsetof(struct shm_ctl, rom_ready), 1)) {
		note("could not raise the ready flags");
		return 1;
	}

	if (!quiet)
		printf("tamacore: rom and save image handed over; waiting for core 0\n");

	started = time(NULL);

	for (;;) {
		uint32_t stage;
		int changed;

		if (g_stop)
			break;

		if (seconds > 0.0 && difftime(time(NULL), started) >= seconds)
			break;

		changed = input_poll(&mask);
		if (changed && mask != sent_mask) {
			set_field((unsigned long)offsetof(struct shm_ctl, buttons), mask);
			sent_mask = mask;
		}

		if (shared_read(SHM_CTL_OFF, &ctl, sizeof(ctl)) != 0) {
			note("control block read failed");
			break;
		}

		stage = ctl.stage;
		if (stage == SHM_STAGE_ERROR) {
			fprintf(stderr, "tamacore: core 0 reported error %u\n",
				(unsigned)ctl.err);
			break;
		}

		if (ctl.frame_seq != last_seq) {
			unsigned long off = ctl.frame_idx ? SHM_FRAME1_OFF
							  : SHM_FRAME0_OFF;

			if (shared_read(off, frame, sizeof(frame)) == 0) {
				if (panel_blit(frame) < 0 && !quiet)
					fprintf(stderr, "tamacore: blit: %s\n",
						strerror(errno));
				frames++;
			}
			last_seq = ctl.frame_seq;
		}

		if (ctl.eeprom_seq != last_eeprom) {
			persist_eeprom();
			last_eeprom = ctl.eeprom_seq;
		}

		usleep(POLL_US);
	}

	set_field((unsigned long)offsetof(struct shm_ctl, quit), 1);

	if (!quiet) {
		printf("tamacore: %lu frames blitted of %u published, "
		       "core 0 ran %u Hz, worst frame late by %u ms\n",
		       frames, (unsigned)ctl.frames, (unsigned)ctl.guest_hz,
		       (unsigned)ctl.jitter_ms);
		if (ctl.stage != SHM_STAGE_RUNNING)
			printf("tamacore: core 0 ended in stage %u (error %u)\n",
			       (unsigned)ctl.stage, (unsigned)ctl.err);
	}

	return 0;
}
