/*
 * bench_nofrendo.c - how fast is nofrendo on this machine, headless?
 *
 * WHY THIS EXISTS. Three NES cores are candidates and the choice should be a
 * measurement, not a reading of source files. This runs one ROM for a fixed
 * number of FRAMES with no video output, no audio output and no sleeping, and
 * reports frames/second and emulated CPU cycles/second. The ratio against the
 * NES's own 1.789773 MHz tells you how many times faster than realtime the core
 * runs on this host, which is the number that decides whether it fits on a
 * 240 MHz microcontroller.
 *
 * WHAT IT DOES NOT MEASURE. This is an x86 host, not an ESP32-S3. A 6502
 * interpreter is pointer-chasing and branch-heavy, and the Xtensa core differs
 * from x86 in exactly the ways that matter there (no large out-of-order window,
 * slower unaligned access, different cache behaviour). So the absolute ratio here
 * does NOT transfer. What DOES transfer is the RATIO BETWEEN CORES -- if core A
 * is 2x core B on x86 it is likely to be roughly 2x on Xtensa, because they are
 * doing the same abstract work with the same kind of interpreter. That is what
 * makes this worth running before porting anything.
 *
 * The NES clock is the fixed point: every core emulates the same 1.789773 MHz,
 * so "cycles per second" from each is directly comparable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nes/nes.h"

/* nofrendo hands a finished PALETTE-INDEXED frame to this. Headless, so it only
 * has to exist -- but it is counted, because a core that calls blit every frame
 * is telling us its render path ran. */
static unsigned long g_blits;
static unsigned long long g_pixels;

/* The palette a port has to convert through, and the conversion loop itself.
 * Done here rather than skipped because "how fast is the emulator" is not the
 * question -- "how fast is the emulator plus the work of getting pixels onto a
 * panel" is. The loop is deliberately the naive one a first port would write. */
static uint16_t g_pal[256];
static uint16_t g_out[NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT];
static unsigned long g_checksum;

static void blit(uint8 *buf)
{
    uint16_t *dst = g_out;

    for (int y = 0; y < NES_SCREEN_HEIGHT; y++)
    {
        const uint8 *src = NES_SCREEN_GETPTR(buf, 0, y);

        for (int x = 0; x < NES_SCREEN_WIDTH; x++)
            *dst++ = g_pal[src[x]];
    }

    /* Touched so the compiler cannot delete the loop. */
    g_checksum += g_out[g_blits & 0x3FF];
    g_blits++;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <rom.nes> [frames]\n", argv[0]);
        return 2;
    }

    int frames = argc > 2 ? atoi(argv[2]) : 1800;   /* 30 s of NTSC */

    nes_t *nes = nes_init(SYS_NES_NTSC, 44100, true, NULL);

    if (!nes)
    {
        fprintf(stderr, "nes_init failed\n");
        return 1;
    }

    nes->blit_func = blit;

    /*
     * THE FRAME BUFFER IS THE CALLER'S, and without it nofrendo silently skips
     * rendering entirely: `draw = draw && nes.vidbuf != NULL` at the top of
     * nes_emulate(). The first run of this benchmark reported 24,715x realtime
     * and blits=0, which was not a fast emulator but a benchmark that had
     * switched off the PPU without saying so -- the CPU core alone, measured as
     * though it were the whole thing.
     *
     * The size is NES_SCREEN_PITCH (8 + 256 + 8) x NES_SCREEN_HEIGHT, one byte
     * per pixel: a palette INDEX, not a colour. Retro-Go converts that through a
     * 256-entry table in its own blit; that conversion is part of the cost being
     * measured below, and it is left to the harness so the number includes
     * everything a port would have to do.
     */
    static uint8 vidbuf[NES_SCREEN_PITCH * NES_SCREEN_HEIGHT];

    int rc = nes_loadfile(argv[1]);

    if (rc < 0)
    {
        fprintf(stderr, "nes_loadfile failed (%d) -- unsupported mapper or bad ROM\n", rc);
        return 1;
    }

    for (int i = 0; i < 256; i++)
        g_pal[i] = (uint16_t)(i * 257);

    /*
     * SET THE FRAME BUFFER *AFTER* LOADING, and the order is not cosmetic.
     * nes_loadfile() -> nes_reset() -> `nes.vidbuf = NULL`, so a buffer set
     * before the load is thrown away and the emulator renders nothing while
     * still reporting a plausible frame rate. The first version of this harness
     * did exactly that and reported 24,125x realtime with blits=0 -- the CPU
     * core alone, silently. Two runs were lost to it. If blits is not equal to
     * frames, the benchmark is not measuring what it claims.
     */
    nes_setvidbuf(vidbuf);

    /* One warm-up frame so allocations and the first render are not counted. */
    nes_emulate(true);

    double t0 = now_s();

    for (int i = 0; i < frames; i++)
        nes_emulate(true);

    double dt = now_s() - t0;

    double fps = frames / dt;

    /* CYCLES PER SECOND IS fps x CYCLES PER FRAME, not fps x the NES clock.
     * Those look similar and are not: one frame is cpu_clock / refresh_rate
     * cycles (29780 for NTSC), so multiplying fps by the 1.789 MHz clock
     * reported 13,133x realtime for a run that was really 218x. The figure was
     * wrong by a factor of 60 -- exactly the frame rate -- which is the kind of
     * error that reads as "plenty of headroom" no matter what the truth is. */
    double cycles_per_frame = NES_CPU_CLOCK_NTSC / 60.0988;
    double cycles = fps * cycles_per_frame;

    printf("nofrendo  %-34s %6.1f fps  %8.2f Mcyc/s  %7.1fx realtime  blits=%lu sum=%lu\n",
           strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1],
           fps, cycles / 1e6, cycles / NES_CPU_CLOCK_NTSC, g_blits, g_checksum);

    /* Same guard as the InfoNES harness: a frame buffer of zeros means the
     * render path did not run, whatever the frame counter says. */
    {
        unsigned long nz = 0;
        for (int i = 0; i < NES_SCREEN_PITCH * NES_SCREEN_HEIGHT; i++)
            if (vidbuf[i])
                nz++;
        fprintf(stderr, "  [diag] vidbuf non-zero: %lu / %d  ppu ctrl0=%02X ctrl1=%02X\n",
                nz, NES_SCREEN_PITCH * NES_SCREEN_HEIGHT,
                (unsigned)nes->ppu->ctrl0, (unsigned)nes->ppu->ctrl1);
    }

    if ((long)g_blits != frames + 1)
    {
        fprintf(stderr, "BENCHMARK INVALID: %lu blits for %d frames -- the render "
                        "path did not run every frame\n", g_blits, frames + 1);
        nes_shutdown();
        return 3;
    }

    nes_shutdown();
    return 0;
}
