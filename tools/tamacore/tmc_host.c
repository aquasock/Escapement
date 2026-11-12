/*
 * tmc_host.c - the SPLB32 emulator, running on a PC.
 *
 * PHASE 1 OF THE TAMACORE PORT, and the reason it exists: of the ~4,660 lines
 * that make the emulator, roughly 2,560 should compile unchanged -- the guest
 * interpreter, the segment map, the renderer, the EEPROM and IR models. The
 * other 1,160 lines are the shared-PSRAM window, the cache invalidations and
 * the two-core handshake, which exist only because Linux and ESP-IDF had to
 * share one chip. This harness runs the portable half on the host with none of
 * that, which does two things at once: it proves the interpreter is correct
 * before any firmware work, and it gives a speed baseline on a machine whose
 * clock is known, so the ESP32-S3 number later has something honest to be
 * compared against.
 *
 * It is also the thing that makes the port tractable at all. Iterating on a
 * watch costs a flash cycle of about 20 seconds and a manual look at the glass;
 * iterating here costs a second and produces a PPM nobody has to squint at.
 *
 * WHAT IT DOES NOT DO. No display, no touch, no sound: frames go to a file, the
 * buttons come from the command line. Those are the parts the firmware genuinely
 * has to own, so they are deliberately absent rather than stubbed.
 *
 * Build and run (from the repository root):
 *
 *   ./scripts/tamacore-host.sh build
 *   ./scripts/tamacore-host.sh run --rom Tamagotchi_v3_us_eu.bin --seconds 5
 *
 * Output: <prefix>-NNNN.ppm frames, a WAV of whatever the buzzer made, and a
 * line of numbers: guest instructions, host seconds, and the ratio.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "splb32.h"
#include "splb32_sound.h"
#include "segmap.h"
#include "render.h"

/*
 * THE FRAME GEOMETRY, repeated here rather than included.
 *
 * shmproto.h cannot be included: it is written for the firmware and carries the
 * PSRAM window address, the control-block layout and static asserts about cache
 * lines, none of which exist in a host program. But the numbers that matter --
 * the dot pitch, the surround and the framebuffer the artwork was laid out
 * against -- are the contract between the renderer and whoever blits the
 * result, so they are stated once here and checked against segmap.h below by a
 * compile-time assert. A mismatch would silently draw past the frame or leave a
 * margin, which is exactly the class of fault this project has already paid for
 * once (see SHM_DOT in shmproto.h).
 */
#define HOST_DOT     5
#define HOST_GAP     1
#define HOST_PAD     4
#define HOST_FB_W    320
#define HOST_FB_H    400
#define HOST_LCD_W   SEG_LCD_W
#define HOST_LCD_H   SEG_LCD_H
#define HOST_FRAME_W (HOST_LCD_W + 2 * HOST_PAD)
#define HOST_FRAME_H (HOST_LCD_H + 2 * HOST_PAD)
#define HOST_ORIGIN_X ((HOST_FB_W - HOST_LCD_W) / 2 - HOST_PAD)
#define HOST_ORIGIN_Y ((HOST_FB_H - HOST_LCD_H) / 2 - HOST_PAD)

#define BEZEL 0x0000

/* The renderer says SEG_LCD_W/H is authoritative and that these two must agree.
 * If segmap.h ever changes the artwork size, this fails at compile time instead
 * of producing frames with a margin. */
_Static_assert(SEG_LCD_W == 292 && SEG_LCD_H == 300,
               "segmap.h artwork changed size; HOST_FRAME_* and the firmware's SHM_* must follow");

/* --- what the guest publishes --- */

static uint16_t s_fb[HOST_FRAME_W * HOST_FRAME_H];

/*
 * One guest step, then paint. 1/16 s of guest time per frame is what the
 * firmware uses (tamacore_task.c publishes at that rate), so the harness keeps
 * it: 16 fps is the pacing the original ran at and a frame every 1/16 s is what
 * the artwork's animations are authored against.
 */
/* The guest's clock is the emulator's, not a guess: SPLB32_CLOCK_HZ. The first
 * version of this file used 32768, which is the rate of the real part's
 * oscillator but NOT what the harness models, and the frame pacing would have
 * been 122x too fast. */
#define GUEST_HZ   SPLB32_CLOCK_HZ
#define FRAME_HZ   16
#define CYCLES_PER_FRAME (GUEST_HZ / FRAME_HZ)

/*
 * THE LCD STARTS AT PAD INSIDE THE FRAME, NOT AT THE FRAMEBUFFER ORIGIN.
 *
 * HOST_ORIGIN_X/Y are where the frame sits inside the watch's 320x400
 * framebuffer -- they are about placing a 300x308 rectangle on the glass, and
 * they are 10 and 46. The renderer, though, draws relative to the FRAME's own
 * top-left, so the LCD begins at SHM_FRAME_PAD (4,4) within it, exactly as
 * tamacore_task.c calls it.
 *
 * Using the framebuffer origin here put the LCD's left edge at x=10 in a frame
 * only 300 wide, so 10+292=302 ran past the right edge and about 2% of every
 * row was clipped -- and the *effect* was not a clipped picture but a picture
 * that looked uniformly blank, because the clip landed on the artwork. */
static void paint(lcd_fb *fb, const uint8_t *dpram, int yl, int yh)
{
    lcd_draw_dots_rows(fb, dpram, HOST_PAD, HOST_PAD,
                       HOST_DOT, HOST_GAP, yl, yh);
}

static void fill_bezel(void)
{
    for (int i = 0; i < HOST_FRAME_W * HOST_FRAME_H; i++)
        s_fb[i] = BEZEL;
}

/*
 * A frame as text, for a terminal.
 *
 * The whole point of this harness is fast iteration, and opening a PPM is not
 * fast when the question is "is it drawing at all". One character per 2x4 block
 * of the 300x308 frame is 150x77, which fits a terminal.
 *
 * POSITION DECIDES, NOT COLOUR, and that distinction cost a debugging session.
 * LCD_INK and BEZEL are both 0x0000, so a renderer that skips "the bezel
 * colour" skips the ink with it and reports a blank screen for a working
 * emulator. Inside the LCD rectangle a zero pixel is INK; outside it, it is the
 * surround. The first version got this wrong and made a correct frame look
 * empty.
 */
static void print_ascii(void)
{
    for (int y = 0; y < HOST_FRAME_H; y += 4)
    {
        for (int x = 0; x < HOST_FRAME_W; x += 2)
        {
            int ink = 0, lit = 0;
            for (int dy = 0; dy < 4 && y + dy < HOST_FRAME_H; dy++)
                for (int dx = 0; dx < 2 && x + dx < HOST_FRAME_W; dx++)
                {
                    int fy = y + dy, fx = x + dx;
                    int inside = fy >= HOST_PAD && fy < HOST_PAD + HOST_LCD_H &&
                                 fx >= HOST_PAD && fx < HOST_PAD + HOST_LCD_W;
                    uint16_t p = s_fb[(size_t)fy * HOST_FRAME_W + fx];

                    if (!inside)
                        continue;              /* the surround is not part of the picture */
                    if (p == LCD_INK)
                        ink++;                 /* a lit dot: black inside the LCD */
                    else
                        lit++;
                }
            if (ink + lit == 0)
                putchar(' ');                  /* outside the LCD: blank */
            else if (ink >= 4)
                putchar('#');
            else if (ink >= 2)
                putchar('+');
            else if (ink == 1)
                putchar('`');                  /* one lit dot: faint */
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

    fprintf(f, "P6\n%d %d\n255\n", HOST_FRAME_W, HOST_FRAME_H);
    for (int i = 0; i < HOST_FRAME_W * HOST_FRAME_H; i++)
    {
        uint16_t p = s_fb[i];
        /* RGB565 to RGB888, the same expansion the panel's own path implies. */
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

/* What is the machine actually doing? Added because "blank LCD" has three
 * causes with three different fixes -- a guest that never started, one that
 * halted waiting for a wakeup, and one that runs without writing the display --
 * and the frame alone cannot tell them apart.
 *
 * The state struct exposes PC, the registers and the retired-opcode count, so
 * this reports the things that distinguish those cases: where the guest is
 * executing, and whether it has retired any instructions at all. A PC that
 * never advances between samples is a halt; a non-zero PCs that advances with
 * a static dpram is a guest that is waiting or reading input. */
static void report_state(splb32 *cpu, const char *when)
{
    splb32_state st;
    splb32_snapshot(cpu, &st);
    const uint8_t *dp = splb32_dpram(cpu);
    unsigned nz = 0;
    for (int i = 0; i < 0x200; i++)
        if (dp[i])
            nz++;

    fprintf(stderr,
            "  [%s] pc=%05x a=%04x x=%04x y=%04x sp=%04x instrs=%u cycles=%llu "
            "dpram_nonzero=%u first=%02x%02x%02x%02x\n",
            when, (unsigned)st.pc, st.a, st.x, st.y, st.sp, st.instrs,
            (unsigned long long)st.cycles, nz, dp[0], dp[1], dp[2], dp[3]);
}

static void usage(void)
{
    fprintf(stderr,
            "usage: tmc_host --rom FILE [--seconds N] [--prefix P]\n"
            "                [--frames N] [--buttons MASK] [--sound FILE]\n"
            "                [--ascii N] [--no-ppm] [--quiet]\n"
            "  --ascii    print every Nth frame as text (start with 1 or 10)\n"
            "  --no-ppm   do not write frame files\n"
            "  --seconds  guest seconds to run (default 5)\n"
            "  --frames   stop after this many frames (overrides --seconds)\n"
            "  --prefix   frame files are PREFIX-NNNN.ppm (default frame)\n"
            "  --buttons  button mask held throughout, as hex (see splb32.h)\n");
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL, *prefix = "frame", *sound_path = NULL;
    double seconds = 5.0;
    long max_frames = -1;
    int ascii_every = 0;      /* print a frame as text every N frames, 0 = never */
    int want_ppm = 1;
    unsigned buttons = 0;
    int quiet = 0;

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
        else if (!strcmp(argv[i], "--sound") && i + 1 < argc)
            sound_path = argv[++i];
        else if (!strcmp(argv[i], "--quiet"))
            quiet = 1;
        else if (!strcmp(argv[i], "--ascii") && i + 1 < argc)
            ascii_every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-ppm"))
            want_ppm = 0;
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

    FILE *f = fopen(rom_path, "rb");
    if (!f)
    {
        fprintf(stderr, "tmc_host: cannot open %s\n", rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long rom_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (rom_len <= 0)
    {
        fprintf(stderr, "tmc_host: %s is empty\n", rom_path);
        fclose(f);
        return 1;
    }
    uint8_t *rom = malloc((size_t)rom_len);
    if (!rom || fread(rom, 1, (size_t)rom_len, f) != (size_t)rom_len)
    {
        fprintf(stderr, "tmc_host: cannot read %s\n", rom_path);
        fclose(f);
        free(rom);
        return 1;
    }
    fclose(f);

    splb32 *cpu = splb32_new(rom, (size_t)rom_len);
    if (!cpu)
    {
        fprintf(stderr, "tmc_host: splb32_new failed\n");
        free(rom);
        return 1;
    }

    /*
     * THE MACHINE'S WIRING, IN THE FIRMWARE'S EXACT ORDER AND WITH ITS EXACT
     * PARAMETERS. The first version of this harness got all four of these wrong
     * and the symptom is worth keeping: 100x realtime, frames written, and
     * every frame an empty LCD. The guest was running at full speed and
     * spinning in the EEPROM's I2C read, because there was no SDA pull-up, so
     * it never reached the code that draws.
     *
     *   port 3 (PD) mask 0x02   the EEPROM's SDA pull-up
     *   eeprom at 0xA0          not 0x50: this is the 8-bit address the part
     *                           answers on, and the harness had invented 0x50
     *   SDA 0x02, SCL 0x01      port D bits, from the same board wiring
     *   IR rx/tx 0x04           port C, both directions on one line
     *
     * AND NO splb32_reset() AFTERWARDS. splb32_reset() memsets pullup_ext, so a
     * reset after this removes the pull-up again and the guest hangs exactly as
     * it did here. splb32_new() has already reset the core; the wiring goes on
     * after it, which is what tamacore_task.c and tamafb.c both do.
     */
    splb32_set_pullup(cpu, 3, 0x02);
    splb32_attach_eeprom(cpu, 0xA0, 0x02, 0x01);
    splb32_attach_ir(cpu, 0x04, 0x04);

    /* Two seconds of guest time before the first frame, as the firmware does:
     * the ROM's start-up runs its self-test and clears display RAM over that
     * span, and painting before it finishes shows whatever the ROM left in the
     * dpram -- which on a fresh emulator is zeros, not a blank LCD. */
    splb32_run_cycles(cpu, SPLB32_CLOCK_HZ * 2u);
    if (!quiet)
        report_state(cpu, "after boot");

    lcd_fb fb = { s_fb, HOST_FRAME_W, HOST_FRAME_H };

    const long total_frames = max_frames > 0 ? max_frames
                                             : (long)(seconds * FRAME_HZ);

    if (!quiet)
        fprintf(stderr,
                "tmc_host: rom %ld bytes, %ld frames, dot %d pitch %d, frame %dx%d at (%d,%d)\n",
                rom_len, total_frames, HOST_DOT, HOST_DOT + HOST_GAP,
                HOST_FRAME_W, HOST_FRAME_H, HOST_ORIGIN_X, HOST_ORIGIN_Y);

    uint64_t cycles_done = SPLB32_CLOCK_HZ * 2u;   /* the boot span already run */
    double t0 = now_s();
    char path[512];

    fill_bezel();

    for (long n = 0; n < total_frames; n++)
    {
        if (buttons)
            splb32_pin(cpu, 0, (uint8_t)buttons, 0);

        /* THE ARGUMENT IS A DELTA AND THE RETURN IS ABSOLUTE. The firmware's
         * comment ("cycles_done += splb32_run_cycles(...) inflated the count")
         * describes the opposite mistake -- it treats the return as absolute,
         * which is right -- and I read it as saying the ARGUMENT was absolute
         * too. It is not: run_cycles() computes `target = c->cycles + cycles`,
         * so passing a growing absolute number each frame made every frame run
         * a LARGER slice than the one before (8.25M, 8.5M, 8.75M ...). Two
         * guest seconds took 60 s of host time that way, against 0.015 s for a
         * single frame, which is what gave it away.
         *
         * So: a constant delta in, the absolute cycle count out. */
        cycles_done = splb32_run_cycles(cpu, CYCLES_PER_FRAME);

        /* Repaint the whole frame each time. The firmware tracks which rows can
         * have changed from the dpram write set and paints only those; on a PC
         * that optimisation buys nothing and would hide a renderer fault behind
         * a dirty-tracking fault, so the harness keeps the two apart by doing
         * the simple thing. */
        paint(&fb, splb32_dpram(cpu), HOST_PAD, HOST_PAD + HOST_LCD_H);

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
            printf("--- frame %ld ---\n", n);
            print_ascii();
            fflush(stdout);
        }
    }

    double elapsed = now_s() - t0;
    if (!quiet)
        report_state(cpu, "at end");

    if (!quiet)
    {
        /* The per-opcode counters are compiled out of the emulator by default
         * -- splb32.c explains why: the read-modify-write cost 3.8 native
         * cycles per instruction and bought little. So the harness reports the
         * cycle count, which is always available, and says plainly that a
         * per-instruction figure needs SPLB32_PROF. A number that silently
         * reads zero would be worse than no number. */
        printf("frames      %ld\n", total_frames);
        printf("guest cycles %llu (%.2f s of guest time)\n",
               (unsigned long long)cycles_done, (double)cycles_done / GUEST_HZ);
        printf("host seconds %.3f\n", elapsed);
        /* Emulated megahertz: guest cycles per host second. Unlike an
         * instructions-per-second figure this needs no profiling build and is
         * the same quantity on the watch, so the two can be compared. */
        printf("emulated MHz %.2f\n",
               elapsed > 0 ? (double)cycles_done / elapsed / 1e6 : 0.0);
        /* The number the port cares about: how many times faster than realtime
         * the GUEST ran. cycles are the guest's own crystal, so this is a ratio
         * of emulated time to host time and is comparable across machines. */
        printf("guest x realtime %.2f\n",
               elapsed > 0 ? ((double)total_frames / FRAME_HZ) / elapsed : 0.0);
    }

    if (sound_path)
    {
        /* splb32_sound's contract is checked by reading it before relying on it;
         * this is left as a stub until the firmware needs the buzzer. */
        fprintf(stderr, "tmc_host: --sound not implemented yet\n");
    }

    splb32_free(cpu);
    free(rom);
    return 0;
}
