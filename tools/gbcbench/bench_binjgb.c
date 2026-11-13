/*
 * bench_binjgb.c - how fast is binjgb on this machine, headless?
 *
 * WHY A SECOND CORE. The NES scoping compared nofrendo against InfoNES rather
 * than measuring one core and calling it a decision, and the same standard
 * applies here. gnuboy is the speed candidate; binjgb is the accuracy candidate
 * (it is the reference implementation many test suites are written against and
 * it passes blargg/mooneye suites that gnuboy does not). The point of measuring
 * both is the trade the port would be making: how much speed accuracy costs.
 *
 * WHAT THIS ONE PAYS THAT gnuboy's HARNESS DID NOT. binjgb produces RGBA -- four
 * bytes per pixel -- so a port must convert to the panel's RGB565 before pushing
 * anything. gnuboy can emit RGB565 in the panel's byte order directly. That
 * difference is real work and it is included here rather than skipped, because
 * "how fast is the emulator" is not the question; "how fast is the emulator plus
 * the work of getting pixels onto this panel" is.
 *
 * Ticks are the Game Boy's own clock: 4.194304 MHz, 70224 per frame.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "emulator.h"

#define GB_CPU_CLOCK      4194304.0
#define GB_FPS_NTSC       59.7275
#define TICKS_PER_FRAME   ((Ticks)70224)

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
        fprintf(stderr, "usage: %s <rom.gb|rom.gbc> [frames]\n", argv[0]);
        return 2;
    }

    int frames = argc > 2 ? atoi(argv[2]) : 3600;

    FileData rom = {0};

    if (!SUCCESS(file_read(argv[1], &rom)) || !rom.data)
    {
        fprintf(stderr, "could not read %s\n", argv[1]);
        return 1;
    }

    EmulatorInit init = {
        .rom = rom,
        .audio_frequency = 44100,
        .audio_frames = 1024,
        .random_seed = 1,
        .builtin_palette = 0,
        .force_dmg = FALSE,
        .cgb_color_curve = CGB_COLOR_CURVE_NONE,
    };

    /* Read the CGB flag from the cartridge header, since binjgb has no
     * accessor for the mode it chose. 0x80 and 0xC0 both mean the cartridge
     * supports Color; binjgb runs those in CGB mode. */
    const int cgb_flag = rom.size > 0x143 && (rom.data[0x143] & 0x80) != 0;

    Emulator *gb = emulator_new(&init);

    if (!gb)
    {
        fprintf(stderr, "emulator_new failed -- bad or unsupported ROM\n");
        free(rom.data);
        return 1;
    }

    /* The RGBA frame binjgb produces, and the RGB565 the panel would take. */
    static uint16_t out[SCREEN_WIDTH * SCREEN_HEIGHT];
    unsigned long blits = 0, nonzero = 0;
    unsigned long long sum = 0;

    /* Settle the boot sequence before measuring. */
    for (int i = 0; i < 30; i++)
        emulator_run_until(gb, (Ticks)(i + 1) * TICKS_PER_FRAME);

    Ticks base = emulator_get_ticks(gb);

    double t0 = now_s();

    for (int i = 0; i < frames; i++)
    {
        emulator_run_until(gb, base + (Ticks)(i + 1) * TICKS_PER_FRAME);

        FrameBuffer *fb = emulator_get_frame_buffer(gb);

        if (!fb)
            continue;

        /* RGBA8888 -> RGB565, which is the conversion a port must perform.
         * RGBA here is one packed u32 in 0xAARRGGBB order -- see the RGBA_WHITE
         * and RGBA_BLACK constants in emulator.h -- not a four-field struct. */
        for (int p = 0; p < SCREEN_WIDTH * SCREEN_HEIGHT; p++)
        {
            uint32_t c = (*fb)[p];
            out[p] = (uint16_t)((((c >> 16) & 0xFF) >> 3) << 11 |
                                (((c >> 8) & 0xFF) >> 2) << 5 |
                                ((c & 0xFF) >> 3));
        }

        if (blits == 0)
            for (int p = 0; p < SCREEN_WIDTH * SCREEN_HEIGHT; p++)
                nonzero += (out[p] != 0);

        sum += out[blits % (SCREEN_WIDTH * SCREEN_HEIGHT)];
        blits++;
    }

    double dt = now_s() - t0;
    double fps = frames / dt;
    double cycles = fps * (GB_CPU_CLOCK / GB_FPS_NTSC);

    printf("binjgb    %-30s %6.1f fps  %7.2f Mcyc/s  %6.1fx realtime  "
           "fmt=RGBA2_565 hw=%s cb=%lu sum=%llu\n",
           strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1],
           fps, cycles / 1e6, cycles / GB_CPU_CLOCK,
           (cgb_flag ? "CGB" : "DMG"), blits, sum);

    fprintf(stderr, "  [diag] blits=%lu for %d frames, non-zero pixels on frame 0: %lu\n",
            blits, frames, nonzero);

    int bad = 0;

    if ((long)blits < (long)frames - frames / 100)
    {
        fprintf(stderr, "BENCHMARK INVALID: %lu blits for %d frames\n", blits, frames);
        bad = 1;
    }
    if (nonzero == 0)
    {
        fprintf(stderr, "BENCHMARK INVALID: frame buffer is entirely zero\n");
        bad = 1;
    }

    /*
     * The ROM buffer is NOT freed here. emulator_delete() owns everything it was
     * handed in EmulatorInit, and freeing rom.data as well was a double free that
     * crashed after the measurement had already completed -- which is the worst
     * shape a bug can take in a benchmark, because the numbers print and only the
     * exit status is wrong. The summary line was lost to stdout buffering, so the
     * crash also hid the result it had already computed.
     */
    emulator_delete(gb);
    return bad ? 3 : 0;
}
