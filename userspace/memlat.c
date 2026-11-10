/*
 * memlat.c - dependent-load latency on this board, for each memory the
 *            emulator actually touches.
 *
 * The point is to price one guest bus access, because that is what decides
 * whether the SPLB32 interpreter is bound by memory or by its own native work.
 * Three memories matter and they are not the same hardware:
 *
 *   PSRAM, small working set    the emulator's cpu state (about 3 KB)
 *   PSRAM, 64 KB heap           guest RAM, and the interpreter's own code
 *   flash window at 0x42480000  the ROM, read straight out of SPI flash through
 *                               the linear window the kernel maps (EXT-002)
 *
 * The ROM is 72.6 percent of all guest accesses measured by corebench's counter
 * build, so if a flash-window read costs materially more than a PSRAM read then
 * the "memory is a small share of the cost" arithmetic changes.
 *
 * Every test is a dependent chain: the address of each load comes from the
 * value of the previous load, so nothing can be overlapped or prefetched and
 * the number is a true load-to-use latency.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROM_WINDOW 0x42480000u   /* linear flash window, ROM at offset 0x480000 */

/* Keeps the accumulator observable so the compiler cannot drop the loop. */
static volatile uint32_t sink;

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* Classic pointer chase: b[] holds a single cycle, so every load's address is
 * the previous load's value. Latency only, no prefetch benefit. */
static double chase(uint8_t *b, size_t n, long iters) {
    volatile uint8_t s = 0;
    size_t i = 0;
    for (size_t k = 0; k < n; k++) b[k] = (uint8_t)((k * 7 + 1) % n);
    double t0 = now_s();
    for (long k = 0; k < iters; k++) { i = b[i]; s = (uint8_t)(s + i); }
    double t1 = now_s();
    return (t1 - t0) * 1e9 / iters;
}

/* Flash is read-only, so a cycle cannot be written into it. Instead each step
 * loads a byte from flash and derives the next index from it, which keeps the
 * chain dependent while staying inside a window of 2^bits bytes. */
static double chase_flash(const volatile uint8_t *rom, unsigned bits, long iters) {
    uint32_t mask = (1u << bits) - 1u;
    uint32_t i = 0;
    uint32_t s = 0;
    double t0 = now_s();
    for (long k = 0; k < iters; k++) {
        i = (i * 33u + rom[i & mask] + 1u) & mask;
        s += i;
    }
    double t1 = now_s();
    sink = s;
    return (t1 - t0) * 1e9 / iters;
}

/* Control for chase_flash: byte-for-byte the same arithmetic and the same
 * working-set sizes, but the buffer is ordinary PSRAM. Whatever this reports is
 * the arithmetic's cost, so chase_flash minus this is the flash window's cost
 * and nothing else. Without the control the flash number is unfalsifiable. */
static double chase_ctl(uint8_t *buf, unsigned bits, long iters) {
    uint32_t mask = (1u << bits) - 1u;
    uint32_t i = 0;
    uint32_t s = 0;
    double t0 = now_s();
    for (long k = 0; k < iters; k++) {
        i = (i * 33u + buf[i & mask] + 1u) & mask;
        s += i;
    }
    double t1 = now_s();
    sink = s;
    return (t1 - t0) * 1e9 / iters;
}

/* Sequential streaming read: walk the region in order, summing bytes. This is
 * the pattern a fetch-ahead burst engine likes, and it is the complement of the
 * dependent chain above. Comparing the two separates "the bus is wide and
 * bursting" from "the latency to a random address is long", which are different
 * questions and are easy to conflate. */
static double stream_psram(uint8_t *b, size_t n, long iters) {
    uint32_t s = 0;
    double t0 = now_s();
    for (long k = 0; k < iters; k++) {
        for (size_t i = 0; i < n; i++) s += b[i];
    }
    double t1 = now_s();
    sink = s;
    return (t1 - t0) * 1e9 / (double)(iters * (long)n);
}

static double stream_flash(const volatile uint8_t *rom, size_t n, long iters) {
    uint32_t s = 0;
    double t0 = now_s();
    for (long k = 0; k < iters; k++) {
        for (size_t i = 0; i < n; i++) s += rom[i];
    }
    double t1 = now_s();
    sink = s;
    return (t1 - t0) * 1e9 / (double)(iters * (long)n);
}

int main(void) {
    static uint8_t small[3000];              /* static: same segment as the cpu struct */
    uint8_t *big = malloc(1 << 16);
    if (!big) { printf("no heap\n"); return 1; }
    memset(big, 0, 1 << 16);

    /* Flush after every line: a fault later in the run would otherwise lose the
     * whole buffer, which is exactly how this instrument first failed -- a
     * SIGSEGV with no output at all and no way to tell which test died. */
    printf("dependent load,   3 KB static PSRAM : %.2f ns\n", chase(small, sizeof small, 5000000));
    fflush(stdout);
    printf("dependent load,  64 KB heap   PSRAM : %.2f ns\n", chase(big, 1 << 16, 2000000));
    fflush(stdout);

    const volatile uint8_t *rom = (const volatile uint8_t *)ROM_WINDOW;
    /* Cheap validity check before timing anything: a flash window that is not
     * mapped faults here rather than producing a meaningless number. The ROM's
     * first four bytes are A5 5F 10 49 and its crc32 is 301F0E87 (EXT-002), so
     * an all-zero read means the window or the slot is not there. */
    printf("probing flash window 0x%08X ...\n", ROM_WINDOW);
    fflush(stdout);
    if (rom[0] == 0 && rom[1] == 0 && rom[2] == 0 && rom[3] == 0) {
        printf("flash window 0x%08X reads zero -- not mapped, or ROM slot empty; "
               "skipping flash tests\n", ROM_WINDOW);
        return 0;
    }
    printf("flash window 0x%08X first 4 bytes  : %02X %02X %02X %02X\n",
           ROM_WINDOW, rom[0], rom[1], rom[2], rom[3]);
    fflush(stdout);
    static uint8_t ctl[1 << 16];
    for (size_t k = 0; k < sizeof ctl; k++) ctl[k] = (uint8_t)(k * 7 + 1);
    printf("control,  same arithmetic on PSRAM   : %.2f ns  (256 B)\n", chase_ctl(ctl, 8, 5000000));
    fflush(stdout);
    printf("dependent load, 256 B flash window  : %.2f ns\n", chase_flash(rom, 8, 5000000));
    fflush(stdout);
    printf("control,  same arithmetic on PSRAM   : %.2f ns  (16 KB)\n", chase_ctl(ctl, 14, 2000000));
    fflush(stdout);
    printf("dependent load,  16 KB flash window : %.2f ns\n", chase_flash(rom, 14, 2000000));
    fflush(stdout);

    /* Byte-at-a-time streaming over the same 16 KB, for each memory. */
    printf("\nsequential streaming, 16 KB:\n");
    fflush(stdout);
    printf("  PSRAM   : %6.2f ns/byte\n", stream_psram(ctl, 16384, 2000));
    fflush(stdout);
    printf("  flash   : %6.2f ns/byte\n", stream_flash(rom, 16384, 2000));
    fflush(stdout);
    return 0;
}
