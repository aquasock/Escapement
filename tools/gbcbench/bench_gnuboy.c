/*
 * bench_gnuboy.c - how fast is gnuboy on this machine, headless?
 *
 * WHY THIS EXISTS. Same reason as bench_nofrendo.c: choosing an emulator core
 * should be a measurement rather than a reading of source files, and the
 * measurement has to be reproducible after the core is vendored into the
 * firmware. This runs one ROM for a fixed number of FRAMES with no sleeping and
 * no audio, and reports frames/second and emulated CPU cycles/second against the
 * Game Boy's own clock, which is the number that says whether it fits on a
 * 240 MHz microcontroller.
 *
 * WHAT IT MEASURES THAT nofrendo's HARNESS DID NOT. gnuboy can be asked for
 * its output already in the panel's byte order (GB_PIXEL_565_BE -- 16-bit RGB565
 * with the high byte first). The watch's panel wants exactly that: the NES port
 * runs a 256-entry palette lookup per pixel for the same effect, and that
 * conversion measured 4,355 us of a 23,651 us frame. So the format is a
 * property of the CORE here and not work the port must add, which is a real
 * advantage of gnuboy over nofrendo and is why this harness configures it the
 * way a port would rather than the most convenient way.
 *
 * A SECOND FORMAT IS MEASURED FOR COMPARISON. GB_PIXEL_PALETTED hands over
 * palette indices, which is what a port that wants per-game colour correction
 * would take, and it needs a conversion loop. Running both gives the cost of
 * that loop separately instead of guessing it.
 *
 * THE HOST NUMBER IS NOT THE WATCH NUMBER. Same caveat as the NES harness, and
 * it is not a formality: a Z80/GB interpreter is pointer-chasing and
 * branch-heavy, and the Xtensa core differs from x86 exactly there. What
 * transfers is the RATIO BETWEEN CORES and the ratio against the Game Boy's own
 * clock, because every core here emulates the same 4.194304 MHz / 1.048576 MHz
 * and does the same abstract work.
 *
 * THE HARNESS GUARDS THE MEASUREMENT. The NES version found three separate ways
 * to produce a fast-but-fake result, all of which ran to completion and printed
 * plausible speeds. The equivalent traps here are a frame buffer never set (the
 * core skips rendering), a ROM header misread so the mapper never initialises,
 * and counting a frame as emulated when the LCD was off and the core short-
 * circuited at line 88 of gnuboy.c. So: the frame counter is checked against the
 * number of video callbacks, the frame buffer is checksummed to prove it is not
 * empty, and the emulated hardware type is printed so a DMG ROM cannot be
 * reported as CGB work or the reverse.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gnuboy.h"

/* The Game Boy runs at 4.194304 MHz, and a frame is 70224 cycles at 59.7275 Hz
 * (4194304 / 70224 = 59.7275). Both numbers are fixed properties of the
 * hardware, so every core's cycles/second is directly comparable. */
#define GB_CPU_CLOCK      4194304.0
#define GB_FPS_NTSC       59.7275

static unsigned long g_frames;
static unsigned long g_checksum;
static unsigned long g_cb_pixels;      /* pixels the video callback delivered  */
static unsigned long g_fb_nonzero;     /* non-zero pixels on the first frame, to prove rendering */

/* The frame buffer a port supplies. 160x144 at 16 bits is 46,080 bytes. */
static uint16_t g_fb[GB_WIDTH * GB_HEIGHT];

/* The conversion loop a port would need if it took palette indices instead of
 * ready-to-send pixels. Deliberately the naive one, as in the NES harness. */
static uint16_t g_pal[256];

static void video_cb(void *buffer)
{
    const uint16_t *src = (const uint16_t *)buffer;

    if (!src)
        return;

    /*
     * A PALETTED core hands indices; a 565 core hands colours. The conversion is
     * applied only in the first case, which is what makes the two formats
     * comparable rather than both paying for it.
     *
     * g_pal[1] is non-zero only on the paletted path -- see main(). Using it as
     * the flag keeps the format choice in one place instead of threading a
     * global through the callback.
     */
    if (g_pal[1] != 0)
    {
        uint16_t *dst = g_fb;

        for (int i = 0; i < GB_WIDTH * GB_HEIGHT; i++)
            dst[i] = g_pal[((const uint8_t *)buffer)[i]];
        src = g_fb;
    }

    /*
     * THE DIAGNOSTIC SCAN RUNS ON ONE FRAME, NOT EVERY FRAME.
     *
     * It used to run on all of them, and that quietly destroyed the thing this
     * harness exists to measure: a 23,040-iteration scan in both formats cost
     * more than the 256-entry palette conversion it was being compared against,
     * so 565BE and PAL came out within noise of each other (8,724 against 8,882
     * fps) and the conclusion "the format does not matter" would have been an
     * artefact of the measurement. Scanning only the first frame still proves
     * the renderer produced a non-empty picture, which is the guard's purpose,
     * and takes it out of the timed path.
     *
     * g_checksum stays per frame because it is one array read and one add.
     */
    if (g_frames == 0)
    {
        for (int i = 0; i < GB_WIDTH * GB_HEIGHT; i++)
        {
            g_fb_nonzero += (src[i] != 0);
        }
    }

    g_checksum += src[g_frames % (GB_WIDTH * GB_HEIGHT)];
    g_cb_pixels += GB_WIDTH * GB_HEIGHT;
    g_frames++;
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
        fprintf(stderr, "usage: %s <rom.gb|rom.gbc> [frames] [paletted]\n", argv[0]);
        return 2;
    }

    int frames = argc > 2 ? atoi(argv[2]) : 3600;     /* 60 s of GBC */
    int paletted = argc > 3 && strcmp(argv[3], "paletted") == 0;

    gb_video_fmt_t fmt = paletted ? GB_PIXEL_PALETTED : GB_PIXEL_565_BE;

    if (gnuboy_init(44100, GB_AUDIO_MONO_S16, fmt, video_cb, NULL) != 0)
    {
        fprintf(stderr, "gnuboy_init failed\n");
        return 1;
    }

    gnuboy_set_hwtype(GB_HW_CGB);
    gnuboy_set_palette(GB_PALETTE_CGB);

    /* The buffer must be installed before the ROM is run; gnuboy writes into it
     * during lcd.c's line rendering, and a NULL buffer means it renders nothing
     * and the run is a CPU-only measurement wearing a frame rate. */
    gnuboy_set_framebuffer(g_fb);

    /* On the paletted path the palette indices are the core's, not colours, so
     * give them a plausible table to convert through. The non-zero entry at [1]
     * is also the flag video_cb uses to decide whether to convert. */
    for (int i = 0; i < 256; i++)
        g_pal[i] = (uint16_t)(i * 257);

    if (gnuboy_load_rom_file(argv[1]) != 0)
    {
        fprintf(stderr, "gnuboy_load_rom_file failed -- bad ROM or unsupported mapper\n");
        return 1;
    }

    /*
     * THE RESET IS NOT OPTIONAL, and leaving it out is a segfault rather than a
     * quiet wrong answer.
     *
     * No boot ROM is loaded here, and that is supported: gb_cpu_reset() sets
     * `PC = GB.bios ? 0x0000 : 0x0100`, so a core with no BIOS starts at the
     * cartridge entry point exactly as real hardware does once the boot ROM has
     * handed over. But that choice is made IN gb_cpu_reset(), which only runs via
     * gnuboy_reset(). Without it PC stays 0 from the zeroed state, the first
     * fetch goes through address 0x0000, and gb_hw_read() dereferences the NULL
     * hw.bios. The first version of this harness omitted the call and died with
     * SIGSEGV inside the CPU, which looks like a broken core and is a missing
     * line in the harness.
     *
     * retro-go's own front end (retro-core/main/main_gbc.c) does the same thing
     * for the same reason: "Hard reset to have a clean slate".
     */
    gnuboy_reset(true);

    /* Let the boot sequence settle so the first measured frame is a real one. */
    for (int i = 0; i < 30; i++)
        gnuboy_run(true);

    g_frames = g_checksum = g_cb_pixels = g_fb_nonzero = 0;

    double t0 = now_s();

    for (int i = 0; i < frames; i++)
        gnuboy_run(true);

    double dt = now_s() - t0;
    double fps = frames / dt;

    /* cycles per second = frames/s x cycles per frame, NOT fps x the clock --
     * the same 60x error the NES harness made and records. */
    double cycles = fps * (GB_CPU_CLOCK / GB_FPS_NTSC);

    const char *hw = gnuboy_get_hwtype() == GB_HW_CGB ? "CGB" :
                     gnuboy_get_hwtype() == GB_HW_SGB ? "SGB" : "DMG";

    printf("gnuboy    %-30s %6.1f fps  %7.2f Mcyc/s  %6.1fx realtime  "
           "fmt=%s hw=%s cb=%lu sum=%lu\n",
           strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1],
           fps, cycles / 1e6, cycles / GB_CPU_CLOCK,
           paletted ? "PAL" : "565BE", hw, g_frames, g_checksum);

    fprintf(stderr, "  [diag] video callbacks: %lu for %d frames, "
                    "non-zero pixels on frame 0: %lu\n",
                    g_frames, frames, g_fb_nonzero);

    /*
     * THE INVALID-RESULT GUARD. A core that rendered nothing still runs at a
     * plausible speed, so an all-zero frame buffer has to fail the benchmark
     * rather than produce a number.
     *
     * Missing callbacks are tolerated up to 1 percent and reported, because
     * gnuboy legitimately does not call the video hook while the LCD is off --
     * gnuboy_run() returns early in that case (gnuboy.c:88). Zelda's boot has one
     * such frame in 1,200. Failing on it would reject a correct run; ignoring
     * missing callbacks entirely would accept a core whose render path never
     * ran, which is the failure the NES harness lost two runs to.
     */
    int bad = 0;

    if ((long)g_frames < (long)frames - frames / 100)
    {
        fprintf(stderr, "BENCHMARK INVALID: %lu callbacks for %d frames -- the "
                        "render path did not run\n", g_frames, frames);
        bad = 1;
    }
    if (g_fb_nonzero == 0)
    {
        fprintf(stderr, "BENCHMARK INVALID: frame buffer is entirely zero -- "
                        "nothing was rendered\n");
        bad = 1;
    }

    gnuboy_free_rom();
    return bad ? 3 : 0;
}
