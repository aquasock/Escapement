/*
 * tmc_host.c - the Tamagotchi emulator, running on a PC.
 *
 * PHASE 1 AND NOW THE FIRST PART OF PHASE 2. It drives tmc_core.c, the same
 * portable core the firmware component will use, so what runs here and what
 * runs on the watch are one implementation rather than two that agree today.
 * The harness supplies only what a host has to supply: a file for the ROM, a
 * buffer for the frame, a file for the save image, and a clock.
 *
 * Nothing here knows about PSRAM, cache lines, core affinity or a control block,
 * because tmc_core.c does not.
 *
 * Build and run (from the repository root):
 *
 *   ./scripts/tamacore-host.sh build
 *   ./scripts/tamacore-host.sh run --rom Tamagotchi_v3_us_eu.bin --seconds 30
 *
 * Output: <prefix>-NNNN.ppm frames, a line of numbers, and optionally the frame
 * as text for a terminal.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmc_core.h"
#include "render.h"
#include "segmap.h"

static uint16_t s_fb[TMC_FRAME_W * TMC_FRAME_H];

/* --- output -------------------------------------------------------------- */

/*
 * A frame as text, for a terminal.
 *
 * POSITION DECIDES, NOT COLOUR. LCD_INK and the bezel are both 0x0000, so a
 * renderer that skips "the bezel colour" skips the ink with it and reports a
 * blank screen for a working emulator. Inside the LCD rectangle a zero pixel is
 * INK; outside it, it is the surround. One character per 2x4 block.
 */
static void print_ascii(void)
{
    for (int y = 0; y < TMC_FRAME_H; y += 4)
    {
        for (int x = 0; x < TMC_FRAME_W; x += 2)
        {
            int ink = 0, lit = 0;

            for (int dy = 0; dy < 4 && y + dy < TMC_FRAME_H; dy++)
                for (int dx = 0; dx < 2 && x + dx < TMC_FRAME_W; dx++)
                {
                    int fy = y + dy, fx = x + dx;
                    int inside = fy >= TMC_PAD && fy < TMC_PAD + SEG_LCD_H &&
                                 fx >= TMC_PAD && fx < TMC_PAD + SEG_LCD_W;

                    if (!inside)
                        continue;

                    uint16_t p = s_fb[(size_t)fy * TMC_FRAME_W + fx];

                    if (p == LCD_INK)
                        ink++;
                    else
                        lit++;
                }

            if (ink + lit == 0)
                putchar(' ');
            else if (ink >= 4)
                putchar('#');
            else if (ink >= 2)
                putchar('+');
            else if (ink == 1)
                putchar('`');
            else
                putchar('.');
        }
        putchar('\n');
    }
}

static int write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;

    fprintf(f, "P6\n%d %d\n255\n", TMC_FRAME_W, TMC_FRAME_H);

    for (int i = 0; i < TMC_FRAME_W * TMC_FRAME_H; i++)
    {
        uint16_t p = s_fb[i];
        unsigned r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
        unsigned char rgb[3] = {
            (unsigned char)((r << 3) | (r >> 2)),
            (unsigned char)((g << 2) | (g >> 4)),
            (unsigned char)((b << 3) | (b >> 2)),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: tmc_host --rom FILE [--seconds N] [--prefix P]\n"
            "                [--frames N] [--buttons MASK] [--eeprom FILE]\n"
            "                [--save FILE] [--ascii N] [--no-ppm] [--quiet]\n"
            "  --seconds  guest seconds to run (default 5)\n"
            "  --frames   stop after this many frames (overrides --seconds)\n"
            "  --buttons  button mask held throughout, hex: 1, 2 or 4\n"
            "  --eeprom   load a save image before starting\n"
            "  --save     write the save image out when finished\n"
            "  --ascii    print every Nth frame as text (0 = never)\n");
}

static uint8_t *read_file(const char *path, long *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0)
    {
        fclose(f);
        return NULL;
    }

    uint8_t *buf = malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len)
    {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    *len_out = len;
    return buf;
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL, *prefix = "frame";
    const char *eeprom_path = NULL, *save_path = NULL;
    double seconds = 5.0;
    long max_frames = -1;
    unsigned buttons = 0;
    int quiet = 0, ascii_every = 0, want_ppm = 1;

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc)
            rom_path = argv[++i];
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
            seconds = atof(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            max_frames = atol(argv[++i]);
        else if (!strcmp(argv[i], "--prefix") && i + 1 < argc)
            prefix = argv[++i];
        else if (!strcmp(argv[i], "--buttons") && i + 1 < argc)
            buttons = (unsigned)strtoul(argv[++i], NULL, 16);
        else if (!strcmp(argv[i], "--eeprom") && i + 1 < argc)
            eeprom_path = argv[++i];
        else if (!strcmp(argv[i], "--save") && i + 1 < argc)
            save_path = argv[++i];
        else if (!strcmp(argv[i], "--ascii") && i + 1 < argc)
            ascii_every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-ppm"))
            want_ppm = 0;
        else if (!strcmp(argv[i], "--quiet"))
            quiet = 1;
        else
        {
            usage();
            return 2;
        }
    }

    if (!rom_path)
    {
        usage();
        return 2;
    }

    long rom_len = 0;
    uint8_t *rom = read_file(rom_path, &rom_len);
    if (!rom)
    {
        fprintf(stderr, "tmc_host: cannot read %s\n", rom_path);
        return 1;
    }

    tmc *m = NULL;
    tmc_err err = tmc_new(rom, (size_t)rom_len, s_fb, &m);

    if (err != TMC_OK)
    {
        /* The signature case is worth naming: a 512 KiB file of the wrong
         * thing is the most likely mistake, and "wrong ROM" is a much better
         * message than a blank screen. */
        fprintf(stderr, "tmc_host: %s\n",
                err == TMC_ERR_SIGNATURE ? "not a V3 ROM (bad signature)" :
                err == TMC_ERR_MEMORY    ? "out of memory" : "bad argument");
        free(rom);
        return 1;
    }

    if (eeprom_path)
    {
        long len = 0;
        uint8_t *img = read_file(eeprom_path, &len);

        if (img && tmc_eeprom_load(m, img, (size_t)len))
        {
            if (!quiet)
                fprintf(stderr, "tmc_host: save image loaded (%ld bytes)\n", len);
        }
        else
        {
            /* Not fatal: a missing or wrong-sized save is a fresh Tamagotchi,
             * which is a legitimate state and much better than refusing to
             * run. */
            fprintf(stderr, "tmc_host: %s is not a %d-byte save; starting fresh\n",
                    eeprom_path, TMC_EEPROM_BYTES);
        }
        free(img);
    }

    /* Two seconds of guest time before the first frame, as the firmware does:
     * the ROM's start-up clears display RAM over that span and painting before
     * it finishes shows whatever was left in it. */
    tmc_run(m, TMC_CLOCK_HZ * 2u);

    const long total_frames = max_frames > 0 ? max_frames
                                             : (long)(seconds * TMC_FRAME_HZ);

    if (!quiet)
        fprintf(stderr, "tmc_host: rom %ld bytes, %ld frames, frame %dx%d\n",
                rom_len, total_frames, TMC_FRAME_W, TMC_FRAME_H);

    double t0 = now_s();
    char path[512];
    int pushed = 0, skipped = 0;

    for (long n = 0; n < total_frames; n++)
    {
        /* Buttons are held for the whole run here. The firmware sets them from
         * the touch margins every iteration; tmc_button() is the same call. */
        for (int b = 0; b < 3; b++)
            tmc_button(m, b, (buttons & (1u << b)) != 0);

        /* A constant delta in; the absolute cycle count out. Passing a growing
         * target here makes every step larger than the last -- one guest second
         * took a minute of host time before this was right. */
        (void)tmc_run(m, TMC_FRAME_CYCLES);

        int y_lo = 0, y_hi = 0;
        int rows = tmc_frame(m, &y_lo, &y_hi);

        if (rows > 0)
            pushed++;
        else
            skipped++;

        if (want_ppm)
        {
            snprintf(path, sizeof(path), "%s-%04ld.ppm", prefix, n);
            if (write_ppm(path) != 0)
            {
                fprintf(stderr, "tmc_host: cannot write %s\n", path);
                break;
            }
        }

        if (ascii_every > 0 && (n % ascii_every == 0 || n == total_frames - 1))
        {
            printf("--- frame %ld (rows %d-%d) ---\n", n, y_lo, y_hi);
            print_ascii();
            fflush(stdout);
        }
    }

    double elapsed = now_s() - t0;
    uint64_t total_cycles = tmc_cycles(m);

    if (!quiet)
    {
        printf("frames      %ld\n", total_frames);
        printf("guest cycles %llu (%.2f s of guest time)\n",
               (unsigned long long)total_cycles, (double)total_cycles / TMC_CLOCK_HZ);
        printf("host seconds %.3f\n", elapsed);
        printf("pushed %d, skipped %d (%.0f%% of frames had nothing to draw)\n",
               pushed, skipped,
               100.0 * skipped / (pushed + skipped ? pushed + skipped : 1));
        printf("emulated MHz %.2f\n",
               elapsed > 0 ? (double)total_cycles / elapsed / 1e6 : 0.0);
        printf("guest x realtime %.2f\n",
               elapsed > 0 ? ((double)total_frames / TMC_FRAME_HZ) / elapsed : 0.0);
    }

    if (save_path)
    {
        uint8_t img[TMC_EEPROM_BYTES];
        FILE *f;

        tmc_eeprom_save(m, img);
        f = fopen(save_path, "wb");
        if (f)
        {
            fwrite(img, 1, sizeof(img), f);
            fclose(f);
            if (!quiet)
                fprintf(stderr, "tmc_host: save image written to %s\n", save_path);
        }
        else
        {
            fprintf(stderr, "tmc_host: cannot write %s\n", save_path);
        }
    }

    tmc_free(m);
    free(rom);
    return 0;
}
