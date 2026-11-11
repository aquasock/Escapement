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
 * Where the frame sits inside the watch's 320x400 framebuffer, and how big the
 * LCD itself is. These come first because the frame macros are derived from
 * them; defining SHM_FRAME_W in terms of a SHM_LCD_W declared later would not
 * compile, and ordering them wrongly is exactly the kind of mistake the
 * static asserts below are there to catch.
 */
/*
 * Each real LCD dot is a SHM_DOT x SHM_DOT block of pixels, separated by
 * SHM_GAP pixels. 5 and 1 is the harness's original look: the dot matrix is
 * visible as a grid, which is what makes the pet readable across a room.
 *
 * At 5/1 an LCD of 49x49 dots is 293x293 pixels -- 49 dots of 5 px plus 48 gaps
 * of 1 px, because the grid ends with a dot rather than a margin. That fits the
 * 320-pixel width with 27 px to spare, which is where the side of the surround
 * comes from.
 *
 * SET BOTH TO THE SAME VALUE for the true-hardware look (gap 0), and the image
 * shrinks accordingly. The blitter measures its bandwidth from SHM_FRAME_BYTES,
 * so nothing downstream has to know which was chosen.
 */
#define SHM_DOT             5
#define SHM_GAP             1
#define SHM_LCD_DOTS_W      49
#define SHM_LCD_DOTS_H      49
#define SHM_LCD_W           (SHM_LCD_DOTS_W * SHM_DOT + \
			     (SHM_LCD_DOTS_W - 1) * SHM_GAP)
#define SHM_LCD_H           (SHM_LCD_DOTS_H * SHM_DOT + \
			     (SHM_LCD_DOTS_H - 1) * SHM_GAP)
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
	volatile uint32_t reserved3[2];
};

#endif /* SHMPROTO_H */
