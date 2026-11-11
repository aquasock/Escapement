/*
 * shmproto.h - the contract between core 0 and core 1.
 *
 * The window is the top megabyte of PSRAM, 0x3df00000..0x3e000000, mapped on
 * both cores by init_linux_mmap() and excluded from Linux's memory node by the
 * DTS. Neither side allocates it; both sides know its address. See
 * docs/cores.md.
 *
 *   offset      size        owner
 *   0x00000     4 KB        control block, both sides, named fields only
 *   0x01000   104 KB        frame A, core 0 writes
 *   0x1B000   104 KB        frame B, core 0 writes
 *   0x35000     4 KB        EEPROM image, core 1 seeds, both update
 *   0x80000   512 KB        ROM, core 1 copies once, core 0 reads only
 *
 * The frame slots are sized from the frame itself, and the static asserts below
 * check that. They exist because an earlier layout put the frames 4 KB apart
 * while a frame was 6498 bytes, so drawing frame B overwrote the tail of frame A
 * and the EEPROM image with it -- which read back as 18 c6 18 c6, pixel colour
 * 0xC618. The frame is now 204x204 (a 4x LCD plus surround) and 83,232 bytes,
 * so the slot is 0x1A000 and the EEPROM moved clear of it.
 *
 * Rule that keeps this simple: each field has one writer. The two sides never
 * read-modify-write the same word, so nothing here needs a lock.
 *
 * The handshake, and why it carries a session number. Core 1 copies the ROM in,
 * raises rom_ready and eeprom_ready, and bumps `session`. Core 0 waits for both
 * flags and for a session it has not already served.
 *
 * Without the session counter this deadlocked: core 0 clears its own lines at
 * the start of every run, and an earlier version cleared the whole block --
 * wiping flags core 1 had already raised. Both sides then waited forever, with
 * stage stuck at WAIT_ROM and no frames. The counter removes the ordering
 * question entirely instead of adding another flag to get wrong.
 *
 * Coherency: core 0 has a D-cache and core 1 has none, so core 0 calls
 * esp_cache_msync() after publishing and core 1 needs nothing. That was
 * measured rather than assumed -- see docs/cores.md.
 */

#ifndef SHMPROTO_H
#define SHMPROTO_H

#include <stdint.h>

#define SHM_BASE            0x3df00000u
#define SHM_SIZE            0x00100000u

#define SHM_CTL_OFF         0x00000u
#define SHM_FRAME0_OFF      0x01000u
#define SHM_FRAME1_OFF      0x30000u
#define SHM_EEPROM_OFF      0x60000u
#define SHM_ROM_OFF         0x80000u

/*
 * Diagnostics, in the free gap between the save image and the ROM. It gets its
 * own area rather than sharing the control block's spare words because it is
 * written on a different cadence (every phase of every frame) and, crucially,
 * because PSRAM SURVIVES A RESET: `phase` is the only record of what core 0 was
 * doing when the board went down, and it has to be somewhere nothing else
 * clears.
 */
#define SHM_DIAG_OFF        0x70000u
/*
 * A copy of the diagnostics as they were at the START of this boot, i.e. the
 * state the previous boot died in.
 *
 * Zeroing the maxima for the new boot is necessary -- they are maxima, and
 * PSRAM is not cleared at power-on -- but doing it in place destroys exactly
 * the evidence a crash leaves behind. That happened twice this session before
 * the copy existed. The record is snapshotted here first, then cleared.
 */
#define SHM_DIAG_PREV_OFF   (SHM_DIAG_OFF + 0x100u)
#define SHM_DIAG_MAGIC      0x44494147u     /* 'DIAG' */
#define SHM_DIAG_VERSION    1u

/*
 * Flash-path instrument: does anything on core 0 disable the caches after
 * Linux starts, and who. That is the question the freezes turn on.
 *
 * The mechanism, established 2026-11-11 and worth stating accurately because an
 * earlier version of this comment had it wrong: CONFIG_XIP_KERNEL=y and _stext
 * is at 0x42140000, so the kernel EXECUTES FROM FLASH. A flash operation on
 * core 0 turns the external-memory cache off for the whole chip, and kernel
 * instructions are fetched through that cache. Core 1 cannot execute a single
 * kernel instruction during the window, so it does not slow down or lose
 * data -- it dies. Linux has only the flash window and the PSRAM window in
 * /proc/iomem, with no internal SRAM at all, so there is nowhere to run that
 * the cache does not mediate.
 *
 * The counters are NOT kept here and this area is NOT written by the
 * instrument. The instrument runs inside
 * spi_flash_disable_interrupts_caches_and_other_cpu(), which is called during
 * boot before this window is mapped and always with the cache off, so a store
 * here is invalid. An earlier version did store here and the board stopped
 * booting with no message at all. The counters live in internal RAM instead
 * and a core-0 task copies them into this area once the window is usable.
 *
 * A full page past SHM_DIAG_PREV_OFF on purpose: the first attempt used
 * 0x70110, which is 16 bytes INTO the crash snapshot, so it corrupted the one
 * record that outlives a reset.
 */
#define SHM_DIAG_FLASH_OFF  (SHM_DIAG_OFF + 0x200u)
#define SHM_DIAG_FLASH_MAGIC 0x464c5348u    /* 'H','S','L','F' -> "FLSH" */
/*
 * The same record as it was at the START of this boot, i.e. the reading taken
 * when the previous boot died. Same reasoning as SHM_DIAG_PREV_OFF: the live
 * record is re-based to zero every boot, so without a copy the one number that
 * matters -- how many flash operations had happened when the board went down --
 * is destroyed by the reboot that made it interesting.
 */
#define SHM_DIAG_FLASH_PREV_OFF (SHM_DIAG_OFF + 0x240u)

struct shm_flash_instr {
	volatile uint32_t magic;    /* SHM_DIAG_FLASH_MAGIC once publishing */
	volatile uint32_t calls;    /* cache-disable calls since arming */
	volatile uint32_t ret;      /* caller's return address, most recent */
	volatile uint32_t cpu;      /* core that made it (always expected 0) */
	volatile uint32_t armed;    /* 1 once core 0 armed it at the jump */
	/*
	 * Longest single window with interrupts disabled and the cache off,
	 * in CPU cycles: CONFIG_ESP_INT_WDT_TIMEOUT_MS is 800, and at 240 MHz
	 * that is 192000000 cycles. A value approaching it is the reset.
	 * Measured across spi_flash_disable/enable with the cycle counter,
	 * which is a register read and legal with the cache off.
	 */
	volatile uint32_t max_cyc;
	volatile uint32_t last_cyc;
	volatile uint32_t reserved[1];
};

_Static_assert(sizeof(struct shm_flash_instr) == 32,
	       "the flash instrument record must be one cache line");

/*
 * Core 0 publishes only the rows it changed, and core 1 blits only those.
 *
 * This is not an optimisation for its own sake. A 5-dot frame is 181,202 bytes,
 * and flushing it costs a cache writeback plus invalidate inside a critical
 * section -- interrupts disabled -- for the whole range, 16 times a second.
 * Measured, that was only 2.1 ms, so it is not a stall on its own; but ESP-IDF
 * has a documented cache *livelock* mode where cache maintenance on one core
 * contends with external-memory traffic on the other, and the interrupt
 * watchdog is the thing that catches it. Flushing a few hundred bytes instead of
 * 181 KB removes that exposure by ~100x, and it also stops core 1 spending
 * ~90 ms a frame re-blitting rows that did not change.
 *
 * dirty_lo..dirty_hi is one contiguous range rather than a band bitmap: the
 * panel write is one fb_write() over whole rows, so a contiguous range is
 * exactly the transfer shape, and coalescing scattered changes into one range
 * costs a little extra drawing instead of a second transfer.
 */

/*
 * Rows per draw+flush chunk. A full 5x5 frame is 181,216 bytes, and
 * esp_cache_msync() disables interrupts on core 0 for the whole range it is
 * given. 16 rows is 9,632 bytes -- short enough that no single critical section
 * can reach the interrupt watchdog's 800 ms, however much of the screen changed.
 * See the long comment in tamacore_task.c for why this is the fix and not an
 * optimisation.
 */
#define SHM_PAINT_ROWS      16

/* What core 1 is in the middle of. Same purpose, same survives-a-reset trick. */
enum shm_c1_phase {
	SHM_C1_IDLE = 0,
	SHM_C1_READ = 1,        /* pread from the window */
	SHM_C1_PWRITE = 2,      /* writing rows to /dev/fb0 */
	SHM_C1_EEPROM = 3,      /* persisting the save image */
	SHM_C1_WAIT = 4,        /* sleeping between polls */
};

/* What core 0 is in the middle of. Left behind by a reset. */
enum shm_phase {
	SHM_PHASE_IDLE = 0,
	SHM_PHASE_DRAW = 1,
	SHM_PHASE_WRITEBACK = 2,
	SHM_PHASE_INVALIDATE = 3,
};

/*
 * Two writers, so two cache lines, and the separation is not cosmetic.
 *
 * Core 0 has a D-cache and core 1 does not. When core 0 flushes its line it
 * writes back whatever ITS cache holds; if core 1's fields shared that line,
 * every flush would silently restore core 0's stale copy over core 1's fresh
 * heartbeat. The one-writer-per-line rule is what makes that impossible.
 *
 * Line 0 + line 1 (64 bytes): core 0 only.
 * Line 2 (32 bytes):          core 1 only.
 */
struct shm_diag {
	/* line 0 -- core 0 */
	volatile uint32_t magic;
	volatile uint32_t version;
	volatile uint32_t phase;        /* survives a reset: where core 0 died */
	volatile uint32_t frames;

	volatile uint32_t draw_us;      /* last frame, each phase */
	volatile uint32_t wb_us;
	volatile uint32_t inv_us;
	volatile uint32_t gap_us;       /* since the previous publish */

	/* line 1 -- core 0 */
	volatile uint32_t draw_max;     /* worst since boot */
	volatile uint32_t wb_max;
	volatile uint32_t inv_max;
	volatile uint32_t gap_max;
	volatile uint32_t total_max;    /* whole publish, worst */
	volatile uint32_t chunk_max;    /* longest SINGLE esp_cache_msync */

	/*
	 * Flash work, recorded by the firmware's IPC handler because a flash
	 * operation is the other thing on this board that runs with interrupts
	 * off. Written by core 0, in core 0's line.
	 */
	volatile uint32_t flash_max_us;   /* longest single esp_flash_* call */
	volatile uint32_t flash_last;     /* last op: code<<24 | size>>12 */

	/* line 2 -- core 1 */
	volatile uint32_t c1_beat;      /* ++ every loop iteration */
	volatile uint32_t c1_phase;     /* SHM_C1_* below: where core 1 died */
	volatile uint32_t c1_blit_max;  /* longest single pwrite, us */
	volatile uint32_t c1_last_us;   /* duration of the last pwrite, us */
	volatile uint32_t c1_frames;    /* frames this side has blitted */
	/*
	 * Slice instrumentation, borrowed from the reserved words. A slice is one
	 * splb32_run_cycles() call in the behind-the-clock path. The question it
	 * answers is whether the task watchdog fires because individual slices
	 * are long, or because the loop spins through many short ones without
	 * ever yielding -- those need different fixes.
	 */
	volatile uint32_t slice_us_max;   /* longest single slice, us */
	volatile uint32_t slice_us_last;  /* most recent slice, us */
	volatile uint32_t slice_since_yield_us; /* longest run with no vTaskDelay */
};

_Static_assert(sizeof(struct shm_diag) == 96,
	       "diagnostics must be exactly three cache lines");
#define SHM_DIAG_C1_OFF     64u    /* byte offset of core 1's line */
#define SHM_DIAG_OWN_BYTES  64u    /* how much core 0 publishes */


/*
 * Where the frame sits inside the watch's 320x400 framebuffer, and how big the
 * LCD itself is. These come first because the frame macros are derived from
 * them; defining SHM_FRAME_W in terms of a SHM_LCD_W declared later would not
 * compile, and ordering them wrongly is exactly the kind of mistake the
 * static asserts below are there to catch.
 */
/*
 * The V3 display is a 32x30 dot matrix plus 10 status icons. Those are two
 * different kinds of object and are rendered differently (see render.h): the
 * matrix is drawn as SHM_DOT x SHM_DOT blocks on a +(SHM_GAP) pitch, and the
 * icons are drawn as full-resolution glyphs.
 *
 * The whole LCD is therefore NOT dots*SHM_DOT: the icon rows extend past the
 * matrix on both sides and above and below it. SEG_LCD_W/H in segmap.h is the
 * authoritative size, and these two numbers must match it. They matched 49x49
 * before, which was the whole artwork box divided by the dot pitch -- the
 * matrix was only 32x30 of that, and the icons were being quantised onto the
 * same grid, which is why they came out chunky.
 *
 * SET SHM_DOT AND SHM_GAP TO MATCH segmap.h. A mismatch does not fail loudly:
 * the frame is the size these macros say, and the renderer would draw past it
 * or leave a margin. Both are silent.
 */
#define SHM_DOT             5
#define SHM_GAP             1
#define SHM_LCD_DOTS_W      32      /* matrix columns, NOT the LCD width */
#define SHM_LCD_DOTS_H      30      /* matrix rows,    NOT the LCD height */
/*
 * The LCD is the artwork at the chosen dot pitch: 1219.7 x 1254.6 units at
 * 6/25.1 px per unit is 292x300, and that is what segmap.h reports as
 * SEG_LCD_W/H. Kept as literals rather than derived here because this header
 * cannot include segmap.h (different trees), so the pairing is checked by a
 * static assert in render.h instead.
 */
#define SHM_LCD_W           292
#define SHM_LCD_H           300
#define SHM_FB_W            320
#define SHM_FB_H            400
#define SHM_FRAME_PAD       4           /* LCD surround, in pixels */
#define SHM_ORIGIN_X        ((SHM_FB_W - SHM_LCD_W) / 2 - SHM_FRAME_PAD)
#define SHM_ORIGIN_Y        ((SHM_FB_H - SHM_LCD_H) / 2 - SHM_FRAME_PAD)

/*
 * The frame is the LCD area plus its surround, so core 1 blits a rectangle and
 * never has to reason about the border. The stride is rounded up to a multiple
 * of the 32-byte cache line so a flush covers whole lines.
 *
 * At 4x the LCD is 196x196 and a frame is 83,232 bytes -- 12.8x the 1x frame,
 * which is why the blitter writes it in bands (see SHM_BAND_ROWS). A full-width
 * strip of 204 rows costs about 62 ms at the measured 2.1 MB/s, and 16 fps
 * would need 995 ms of every second on core 1. Four bands of 51 rows cost 16 ms
 * each, and only the bands that actually changed are written.
 */
#define SHM_FRAME_W         (SHM_LCD_W + 2 * SHM_FRAME_PAD)
#define SHM_FRAME_H         (SHM_LCD_H + 2 * SHM_FRAME_PAD)
#define SHM_FRAME_STRIDE    (((SHM_FRAME_W * SHM_FRAME_H * 2) + 31) / 32 * 32)
#define SHM_FRAME_BYTES     (SHM_FRAME_W * SHM_FRAME_H * 2)

/*
 * Rows per blit band. 51 gives exactly four bands for a 204-row frame, so a
 * full repaint is four writes and a typical idle repaint is one.
 */
#define SHM_BAND_ROWS       ((SHM_FRAME_H + 3) / 4)

#define SHM_EEPROM_BYTES    1024
#define SHM_ROM_BYTES        524288u     /* 512 KB, the V3 ROM */

/* The bug that produced a full-width smear: a frame larger than its slot. */
_Static_assert(SHM_FRAME_STRIDE <= (SHM_FRAME1_OFF - SHM_FRAME0_OFF),
	       "a frame does not fit in its slot");
_Static_assert(SHM_EEPROM_OFF >= SHM_FRAME1_OFF + SHM_FRAME_STRIDE,
	       "the EEPROM image overlaps frame B");
_Static_assert(SHM_ORIGIN_X >= 0 && SHM_ORIGIN_X + SHM_FRAME_W <= SHM_FB_W,
	       "the frame does not fit across the framebuffer");
_Static_assert(SHM_ORIGIN_Y >= 0 && SHM_ORIGIN_Y + SHM_FRAME_H <= SHM_FB_H,
	       "the frame does not fit down the framebuffer");
_Static_assert(SHM_BAND_ROWS * 4 >= SHM_FRAME_H, "four bands must cover a frame");
_Static_assert(SHM_LCD_W <= SHM_FB_W && SHM_LCD_H <= SHM_FB_H,
	       "the LCD does not fit on the glass");
_Static_assert(SHM_EEPROM_OFF + SHM_EEPROM_BYTES <= SHM_ROM_OFF,
	       "the EEPROM image overlaps the ROM");

/*
 * Colours. The bezel is deliberately NOT the LCD background colour: with both
 * at 0xC618 the whole strip read as one flat light band with no visible LCD.
 * Bezel black matches the console background, so only the LCD area shows.
 */
#define SHM_LCD_INK         0x0000
#define SHM_LCD_OFF         0xC618
#define SHM_BEZEL           0x0000

#define SHM_MAGIC           0x414d4154u     /* 'TAMA' */
#define SHM_VERSION         1u

/* Buttons, active high in the bitmask, matching splb32's PA bits. */
#define SHM_BTN_LEFT        0x01
#define SHM_BTN_MIDDLE      0x02
#define SHM_BTN_RIGHT       0x04

/* Where core 0 has got to. Published even when it never reaches RUNNING, which
 * is the whole point: this design cannot log at the handoff (the log pipe
 * wedges the board there), so progress has to be visible in the window. */
enum shm_stage {
	SHM_STAGE_IDLE = 0,     /* task not started */
	SHM_STAGE_WAIT_ROM,     /* waiting for core 1 to copy the ROM */
	SHM_STAGE_WAIT_EEPROM,  /* ROM present, waiting for the save image */
	SHM_STAGE_RUNNING,      /* stepping the guest */
	SHM_STAGE_ERROR,        /* see `err` */
};

/* `err` values when stage is SHM_STAGE_ERROR. */
enum shm_err {
	SHM_ERR_NONE = 0,
	SHM_ERR_CPU_ALLOC,      /* splb32_new() failed */
	SHM_ERR_ROM_SIGNATURE,  /* the bytes at SHM_ROM_OFF are not a V3 ROM */
};

/*
 * The control block. Exactly four 32-byte cache lines so a whole-block flush is
 * two cache-line operations per side and no field ever shares a line with a
 * field the other core writes.
 */
struct shm_ctl {
	/* line 0: identity, written once by core 0 */
	volatile uint32_t magic;
	volatile uint32_t version;
	volatile uint32_t stage;
	volatile uint32_t err;
	volatile uint32_t frame_pad;    /* SHM_FRAME_PAD, so core 1 can check */
	volatile uint32_t rom_bytes;
	volatile uint32_t reserved0[2];

	/* line 1: core 1 writes, core 0 reads */
	volatile uint32_t rom_ready;
	volatile uint32_t eeprom_ready;
	volatile uint32_t eeprom_dirty; /* core 1 has newer EEPROM bytes */
	volatile uint32_t buttons;
	volatile uint32_t quit;
	volatile uint32_t session;      /* bumped once per core 1 start */
	volatile uint32_t reserved1[2];

	/* line 2: core 0 writes, core 1 reads */
	volatile uint32_t frame_seq;
	volatile uint32_t frame_idx;
	volatile uint32_t frames;
	volatile uint32_t guest_hz;     /* emulated cycles/s, measured */
	volatile uint32_t guest_cycles; /* low 32 bits, wraps */
	volatile uint32_t jitter_ms;    /* worst frame lateness since boot */
	volatile uint32_t eeprom_seq;   /* bumped when the EEPROM image changes */
	volatile uint32_t reserved2;

	/* line 3: core 0 diagnostics, core 1 reads */
	volatile uint32_t guest_pc;     /* where the guest is executing */
	volatile uint32_t dpram_lit;    /* nonzero bytes in display RAM */
	volatile uint32_t instrs_lo;    /* opcodes retired, low 32 bits */
	volatile uint32_t btn_events;   /* pin changes applied from `buttons` */
	volatile uint32_t reset_reason; /* esp_reset_reason() for THIS boot */
	volatile uint32_t boot_count;   /* firmware boots, increments per reset */
	volatile uint32_t dirty_lo;     /* first frame row core 0 changed */
	volatile uint32_t dirty_hi;     /* one past the last changed row */
};

#endif /* SHMPROTO_H */
