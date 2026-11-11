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
 *   0x01000     8 KB        frame A, core 0 writes
 *   0x02000     8 KB        frame B, core 0 writes
 *   0x03000     4 KB        EEPROM image, core 1 seeds, both update
 *   0x80000     512 KB      ROM, core 1 copies once, core 0 reads only
 *
 * Rule that keeps this simple: each field has one writer. The two sides never
 * read-modify-write the same word, so nothing here needs a lock.
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
#define SHM_FRAME1_OFF      0x02000u
#define SHM_EEPROM_OFF      0x03000u
#define SHM_ROM_OFF         0x80000u

/*
 * The frame is the LCD area plus its 4 px surround, so core 1 blits one
 * rectangle and never has to reason about the border. 57x57 RGB565 is 6498
 * bytes; the stride is rounded up to a multiple of the 32-byte cache line so a
 * flush covers whole lines.
 */
#define SHM_FRAME_W         57
#define SHM_FRAME_H         57
#define SHM_FRAME_STRIDE    6528u       /* 57*57*2 = 6498, padded to 32 */
#define SHM_FRAME_BYTES     (SHM_FRAME_W * SHM_FRAME_H * 2)
#define SHM_FRAME_PAD       4           /* LCD surround, in pixels */

/* Where the frame sits inside the watch's 320x400 framebuffer. */
#define SHM_LCD_W           49
#define SHM_LCD_H           49
#define SHM_FB_W            320
#define SHM_FB_H            400
#define SHM_ORIGIN_X        ((SHM_FB_W - SHM_LCD_W) / 2 - SHM_FRAME_PAD)
#define SHM_ORIGIN_Y        ((SHM_FB_H - SHM_LCD_H) / 2 - SHM_FRAME_PAD)

/* Colours, matching the front end this replaces. */
#define SHM_LCD_INK         0x0000
#define SHM_LCD_OFF         0xC618

#define SHM_EEPROM_BYTES    1024
#define SHM_ROM_BYTES        524288u     /* 512 KB, the V3 ROM */

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
	volatile uint32_t reserved1[3];

	/* line 2: core 0 writes, core 1 reads */
	volatile uint32_t frame_seq;
	volatile uint32_t frame_idx;
	volatile uint32_t frames;
	volatile uint32_t guest_hz;     /* emulated cycles/s, measured */
	volatile uint32_t guest_cycles; /* low 32 bits, wraps */
	volatile uint32_t jitter_ms;    /* worst frame lateness since boot */
	volatile uint32_t eeprom_seq;   /* bumped when the EEPROM image changes */
	volatile uint32_t reserved2;

	/* line 3: spare, and keeps the block cache-line sized */
	volatile uint32_t reserved3[8];
};

#endif /* SHMPROTO_H */
