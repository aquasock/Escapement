/*
 * check_dirty.c - does the dirty-row range cover every pixel that changed?
 *
 * This exists because the optimisation it checks is the one thing in the port
 * that can be wrong without looking wrong. tmc_frame() reports a row range and
 * the firmware blits only that; if the range misses a row, the panel keeps
 * showing the previous frame there and the bug appears as a stale pixel in one
 * place -- which reads as a renderer fault, a blit fault, or a cache fault, and
 * is none of them.
 *
 * THE CHECK COMPARES AGAINST A FULL REPAINT, NOT AGAINST THE PREVIOUS FRAME.
 *
 * The first version compared each frame with the one before it and asked
 * whether every changed pixel fell inside the reported range. That check could
 * not fail: a range that omits rows simply never draws them, so those rows never
 * change, so nothing is "outside the range". A mutant that clamped the range to
 * rows 0-10 passed it -- 86,650 changed pixels, zero uncovered -- while the
 * panel would have shown two thirds of the picture frozen.
 *
 * So the reference is what a full repaint would produce. Every frame is drawn
 * twice: incrementally into the frame buffer the caller owns, and completely
 * into a reference buffer here. Any pixel where they disagree is a row the
 * incremental path failed to redraw, whatever the range said.
 *
 *   ./scripts/tamacore-host.sh check --rom Tamagotchi_v3_us_eu.bin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tmc_core.h"
#include "render.h"
#include "segmap.h"

static uint16_t fb[TMC_FRAME_W * TMC_FRAME_H];
static uint16_t ref[TMC_FRAME_W * TMC_FRAME_H];

int main(int argc, char **argv)
{
    const char *rom_path = argc > 1 ? argv[1]
                                    : "/run/media/vash/GIT/Tamagotchi_v3_us_eu.bin";
    int frames = 2000;

    static uint8_t rom[TMC_ROM_BYTES];

    FILE *f = fopen(rom_path, "rb");
    if (!f)
    {
        fprintf(stderr, "check_dirty: cannot open %s\n", rom_path);
        return 2;
    }
    if (fread(rom, 1, sizeof(rom), f) != sizeof(rom))
    {
        fprintf(stderr, "check_dirty: %s is not a %u-byte ROM\n",
                rom_path, (unsigned)TMC_ROM_BYTES);
        fclose(f);
        return 2;
    }
    fclose(f);

    tmc *m = NULL;
    if (tmc_new(rom, sizeof(rom), fb, &m) != TMC_OK)
    {
        fprintf(stderr, "check_dirty: tmc_new failed\n");
        return 2;
    }

    /* Past the ROM's start-up, so the first sampled frame is a real picture
     * rather than the blank it clears to. */
    tmc_run(m, TMC_CLOCK_HZ * 2u);

    int pushed = 0, mismatched = 0, pixels = 0;

    for (int n = 0; n < frames; n++)
    {
        tmc_run(m, TMC_FRAME_CYCLES);

        int lo = 0, hi = 0;
        int rows = tmc_frame(m, &lo, &hi);

        /*
         * The reference: what the frame buffer WOULD hold if every row had been
         * redrawn. Built independently of tmc_frame()'s range so it cannot
         * inherit its mistakes.
         */
        {
            lcd_fb r = { ref, TMC_FRAME_W, TMC_FRAME_H };

            for (int i = 0; i < TMC_FRAME_W * TMC_FRAME_H; i++)
                ref[i] = 0x0000;      /* the bezel colour, as tmc_core.c uses */
            lcd_draw_dots_rows(&r, tmc_dpram(m), TMC_PAD, TMC_PAD,
                               TMC_DOT, TMC_GAP, TMC_PAD, TMC_PAD + SEG_LCD_H);
        }

        for (int i = 0; i < TMC_FRAME_W * TMC_FRAME_H; i++)
        {
            if (fb[i] == ref[i])
                continue;

            pixels++;
            if (mismatched < 5)
                fprintf(stderr,
                        "MISS frame %d: (%d,%d) frame=%04x reference=%04x range [%d,%d)\n",
                        n, i % TMC_FRAME_W, i / TMC_FRAME_W, fb[i], ref[i], lo, hi);
            mismatched++;
        }

        if (rows > 0)
            pushed++;
    }

    printf("%d frames, %d pushed, %d pixels differ from a full repaint\n",
           frames, pushed, pixels);

    tmc_free(m);
    return mismatched ? 1 : 0;
}
