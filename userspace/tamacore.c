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
 * Banded blit to /dev/fb0, with no large heap allocation anywhere.
 *
 * CO5300_IOC_WRITE_RECT is not available here and it is not a fallback that was
 * missed: the driver refuses any rectangle that intersects fb0, on purpose, so
 * a userspace bug cannot paint over the console. The game lives *inside* fb0,
 * so the only path onto the glass is fb_write(), which blits whole rows at full
 * width.
 *
 * That was fine at 1x: a 320x57 strip cost 17 ms, and a frame fit in one write.
 * At 4x the frame is 204 rows and one write would be 130,560 bytes -- measured
 * at 79 ms. At 16 fps that is 1264 ms of every second on core 1, so the frame is
 * written in four bands of 51 rows and only the bands that changed are sent.
 * One band is about 20 ms, which is what the 1x build paid for a whole frame.
 *
 * Why no 83 KB buffers. The obvious implementation keeps the whole frame plus a
 * shadow of what was sent, and 2 x 83,232 bytes does not allocate on this board:
 *
 *     tamacore: cannot allocate a 83232 byte frame: Cannot allocate memory
 *
 * with 1228 kB free and an order-6 block available, so it is the NOMMU
 * contiguity rule (KRN-001), not exhaustion. Rather than retry-and-hope, this
 * reads ONE BAND at a time from the window with pread and keeps only per-row
 * hashes of what was last written -- 54 KB of static buffers and no malloc.
 *
 * The comparison is against what was last WRITTEN, not against the other frame
 * buffer in the window. Core 0 double-buffers, so the other buffer is tempting,
 * but it holds a frame that may already be half-overwritten: if a row changed
 * and then changed back, comparing against that would call it unchanged and
 * leave the previous row on the glass.
 */
static uint16_t g_band[SHM_FB_W * SHM_BAND_ROWS];   /* 320 x 51, the strip  */
static uint16_t g_rows[SHM_BAND_ROWS * SHM_FRAME_W]; /* 51 x 204, the frame */
static uint32_t g_rowsum[SHM_FRAME_H];              /* per-row, last sent   */
static int g_rowsum_valid;

/* FNV-1a over one row of the frame. 408 bytes at 4x. */
static uint32_t row_hash(const uint16_t *row, int w)
{
	const uint8_t *p = (const uint8_t *)row;
	uint32_t h = 2166136261u;
	int i;

	for (i = 0; i < w * 2; i++)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

/* Build the 320-wide framebuffer strip for rows [y0, y0+rows) of the frame. */
static void compose_band(int y0, int rows)
{
	int y;

	for (y = 0; y < SHM_FB_W * rows; y++)
		g_band[y] = SHM_BEZEL;

	for (y = 0; y < rows; y++) {
		memcpy(&g_band[(size_t)y * SHM_FB_W + SHM_ORIGIN_X],
		       &g_rows[(size_t)y * SHM_FRAME_W],
		       SHM_FRAME_W * sizeof(uint16_t));
	}

	(void)y0;
}

/*
 * `force` sends every band regardless of the hashes. The bench needs it: with
 * an accurate shadow a repeated identical frame costs nothing after the first,
 * which would make the measurement meaningless.
 */
static int panel_blit(unsigned long frame_off, int force)
{
	int y0, failed = 0;

	for (y0 = 0; y0 < SHM_FRAME_H; y0 += SHM_BAND_ROWS) {
		int rows = SHM_FRAME_H - y0;
		int dirty = force, y;

		if (rows > SHM_BAND_ROWS)
			rows = SHM_BAND_ROWS;

		/* One pread per band: 20,808 bytes, a small part of the cost of
		 * writing the same rows to the panel. */
		if (shared_read(frame_off + (unsigned long)y0 * SHM_FRAME_W * 2,
				g_rows, (size_t)rows * SHM_FRAME_W * 2) != 0) {
			failed = 1;
			continue;
		}

		if (!g_rowsum_valid)
			dirty = 1;

		for (y = 0; y < rows && !dirty; y++)
			if (row_hash(&g_rows[(size_t)y * SHM_FRAME_W],
				     SHM_FRAME_W) != g_rowsum[y0 + y])
				dirty = 1;

		if (!dirty)
			continue;

		compose_band(y0, rows);

		/* fb rows are framebuffer rows, offset by where the frame sits */
		if (pwrite(g_fb, g_band, (size_t)rows * SHM_FB_W * 2,
			   (off_t)(SHM_ORIGIN_Y + y0) * SHM_FB_W * 2)
		    != (ssize_t)((size_t)rows * SHM_FB_W * 2)) {
			failed = 1;
			continue;
		}

		for (y = 0; y < rows; y++)
			g_rowsum[y0 + y] = row_hash(&g_rows[(size_t)y * SHM_FRAME_W],
						    SHM_FRAME_W);
		g_rowsum_valid = 1;
	}

	return failed ? -1 : 0;
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

/*
 * Frame checksum, over whatever core 0 last published.
 *
 * Used by -t to answer one question: did pressing a button change what the
 * guest is drawing? That is the only end-to-end evidence that the button path
 * works -- the control field moving proves nothing on its own, because a field
 * nobody reads moves just as readily as one that is read.
 */
static uint32_t frame_sum(uint32_t *idx_out)
{
	uint32_t idx, sum = 0;
	unsigned long off;
	int y0;

	if (shared_read(SHM_CTL_OFF + 68, &idx, sizeof(idx)) != 0)
		idx = 0;
	off = idx ? SHM_FRAME1_OFF : SHM_FRAME0_OFF;

	/* Band by band, so this needs no whole-frame buffer. */
	for (y0 = 0; y0 < SHM_FRAME_H; y0 += SHM_BAND_ROWS) {
		int rows = SHM_FRAME_H - y0;
		int y;

		if (rows > SHM_BAND_ROWS)
			rows = SHM_BAND_ROWS;

		if (shared_read(off + (unsigned long)y0 * SHM_FRAME_W * 2, g_rows,
				(size_t)rows * SHM_FRAME_W * 2) != 0)
			return 0;

		for (y = 0; y < rows; y++)
			sum ^= row_hash(&g_rows[(size_t)y * SHM_FRAME_W],
					SHM_FRAME_W);
	}
	sum ^= (uint32_t)off;

	if (idx_out)
		*idx_out = idx;
	return sum;
}

/* Reads core 0's applied-pin counter (line 3 of the control block). */
static uint32_t ctl_btn_events(void)
{
	uint32_t v = 0;

	shared_read(SHM_CTL_OFF + 108, &v, sizeof(v));
	return v;
}

static void set_buttons(uint32_t mask)
{
	shared_write(SHM_CTL_OFF + 44, &mask, sizeof(mask));
}

#define TAMACORE_VERSION "1.0"

static void usage(void)
{
	printf(
"usage: tamacore [seconds] [-q] [-t] [-b N]\n"
"\n"
"The Tamagotchi Connection V3, emulated on the watch's core 0. Frames are\n"
"blitted to /dev/fb0 by this process on core 1; the bars below the display\n"
"are the three buttons. Ctrl-C stops it and leaves the pet where it was.\n"
"\n"
"  seconds   run for that long, then exit and report (default: until Ctrl-C)\n"
"  -q        quiet: no startup line and no summary\n"
"  -t        self-test: press a button, check core 0 applied it, exit\n"
"  -b N      time N panel writes and exit (measures the blit path)\n"
"  -h        this help\n"
"  -V        version\n"
"\n"
"Needs /proc/esp-shared (CONFIG_ESP_SHARED in the kernel) and /home/rom.bin.\n"
"The save lives in /home/tama.sav and is only written when the pet changes it.\n");
}

int main(int argc, char **argv)
{
	struct shm_ctl ctl = { 0 };
	uint32_t mask = 0, sent_mask = 0;
	uint32_t last_seq = 0, last_eeprom = 0;
	unsigned long frames = 0;
	int quiet = 0, bench = 0, selftest = 0, i;
	double seconds = 0.0;
	time_t started;

	setvbuf(stdout, NULL, _IONBF, 0);

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-q") == 0) {
			quiet = 1;
		} else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
			bench = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-t") == 0) {
			selftest = 1;
		} else if (strcmp(argv[i], "-h") == 0 ||
			   strcmp(argv[i], "--help") == 0) {
			usage();
			return 0;
		} else if (strcmp(argv[i], "-V") == 0 ||
			   strcmp(argv[i], "--version") == 0) {
			printf("tamacore %s\n", TAMACORE_VERSION);
			return 0;
		} else if (argv[i][0] == '-' && argv[i][1]) {
			/*
			 * Refuse an unknown option rather than ignoring it.
			 *
			 * This used to fall through to "start the emulator and
			 * run until killed", so a typo like `-q` written `-Q`
			 * quietly took over the panel and the emulated machine
			 * instead of doing nothing. A command that can seize the
			 * display should be strict about what it is asked for.
			 */
			fprintf(stderr, "tamacore: unknown option '%s'\n",
				argv[i]);
			usage();
			return 2;
		} else {
			seconds = atof(argv[i]);
		}
	}

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

		t0 = now_us();
		for (n = 0; n < bench; n++) {
			if (panel_blit(SHM_FRAME0_OFF, 1) < 0) {
				fprintf(stderr, "tamacore: bench blit: %s\n",
					strerror(errno));
				return 1;
			}
		}
		t1 = now_us();
		printf("tamacore: %d full frames of %d bytes: %.3f ms each "
		       "(%d bands)\n", bench, (int)SHM_FRAME_BYTES,
		       (double)(t1 - t0) / 1000.0 / bench,
		       (int)((SHM_FRAME_H + SHM_BAND_ROWS - 1) / SHM_BAND_ROWS));
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
	 * Clear the stale fields on this side's line -- but NOT `session`.
	 *
	 * PSRAM is not zeroed at power-on, so a stale `buttons` word reads as a
	 * random mask and a stale `quit` stops the guest before it starts; both
	 * were observed, so these have to be cleared. Writing zeros over the
	 * whole 32-byte line also zeroes `session`, and that is a deadlock: the
	 * counter then only ever goes 0 -> 1, while core 0 remembers serving
	 * session 1 and waits forever for a number that never comes. The count
	 * has to be monotonic across boots to mean anything.
	 */
	{
		uint8_t zero[4] = { 0 };
		static const unsigned int stale[] = {
			offsetof(struct shm_ctl, rom_ready),
			offsetof(struct shm_ctl, eeprom_ready),
			offsetof(struct shm_ctl, eeprom_dirty),
			offsetof(struct shm_ctl, buttons),
			offsetof(struct shm_ctl, quit),
		};
		unsigned int i;

		for (i = 0; i < sizeof(stale) / sizeof(stale[0]); i++) {
			if (shared_write(SHM_CTL_OFF + stale[i], zero, sizeof(zero))) {
				note("could not clear the control line");
				return 1;
			}
		}
	}

	/* Order matters: the save image and the ROM are both in place before
	 * either ready flag goes up, and core 0 waits for both. */
	if (seed_eeprom() < 0)
		return 1;
	if (seed_rom() < 0)
		return 1;

	/*
	 * Raise the flags, then bump `session`. The counter is what tells core 0
	 * this is a new run rather than the previous one still standing -- it
	 * remembers the last session it served and starts a fresh machine for
	 * any other number. Signalling with a counter rather than by dropping
	 * and re-raising a flag removes the window in which both sides can be
	 * waiting on each other.
	 */
	{
		uint32_t session = 0;

		shared_read(SHM_CTL_OFF + (unsigned long)offsetof(struct shm_ctl, session),
			    &session, sizeof(session));

		if (set_field((unsigned long)offsetof(struct shm_ctl, eeprom_ready), 1) ||
		    set_field((unsigned long)offsetof(struct shm_ctl, rom_ready), 1) ||
		    set_field((unsigned long)offsetof(struct shm_ctl, session),
			      session + 1)) {
			note("could not raise the ready flags");
			return 1;
		}
	}

	if (!quiet)
		printf("tamacore: rom and save image handed over; waiting for core 0\n");

	started = time(NULL);

	/*
	 * -t: press the middle button and check that the picture changes.
	 *
	 * Waits for core 0 to be RUNNING first, because pressing into a machine
	 * that has not started says nothing. Three samples are taken: before, a
	 * moment after the press, and after the release, so "changed" cannot be
	 * satisfied by the frame counter moving on its own.
	 */
	if (selftest) {
		uint32_t before, during, after, pl, pd, pa;
		uint32_t events_before, events_after;
		int rc;
		int waited;

		for (waited = 0; waited < 400; waited++) {
			uint32_t st;

			if (shared_read(SHM_CTL_OFF + 8, &st, sizeof(st)) == 0 &&
			    st == SHM_STAGE_RUNNING)
				break;
			usleep(25000);
		}
		if (waited >= 400) {
			printf("tamacore: selftest: core 0 never reached RUNNING\n");
			return 1;
		}

		usleep(1500000);                /* let it finish booting */
		before = frame_sum(&pl);
		events_before = ctl_btn_events();
		set_buttons(SHM_BTN_MIDDLE);
		usleep(400000);
		set_buttons(0);
		usleep(1500000);
		during = frame_sum(&pd);
		events_after = ctl_btn_events();
		usleep(1500000);
		after = frame_sum(&pa);

		printf("tamacore: selftest: frame %08x -> %08x -> %08x "
		       "(idx %u/%u/%u)\n",
		       before, during, after, pl, pd, pa);
		printf("tamacore: selftest: pin changes applied by core 0: "
		       "%u -> %u\n", events_before, events_after);

		/*
		 * Two separate claims, and only one of them is a pass/fail.
		 *
		 * The pin counter is decisive: core 0 read the mask out of the
		 * window and drove the emulated inputs, which is the whole
		 * cross-core button path. The picture changing is reported but
		 * is NOT treated as proof, because this guest animates on its
		 * own -- an earlier version of this test passed on a pet that
		 * was merely blinking.
		 */
		if (events_after > events_before) {
			printf("tamacore: selftest: PASS -- core 0 applied the "
			       "press (picture %s)\n",
			       before != after ? "also changed" : "did not change");
			rc = 0;
		} else {
			printf("tamacore: selftest: FAIL -- core 0 never applied "
			       "the press\n");
			rc = 1;
		}

		set_field((unsigned long)offsetof(struct shm_ctl, quit), 1);
		return rc;
	}

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

			if (panel_blit(off, 0) < 0 && !quiet)
				fprintf(stderr, "tamacore: blit: %s\n",
					strerror(errno));
			frames++;
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
