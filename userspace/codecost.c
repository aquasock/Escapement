/*
 * codecost.c - does instruction fetch from PSRAM, with no I-cache, dominate?
 *
 * This exists to settle one question that decides whether the interpreter
 * rewrite in EMU-005 is worth doing. The interpreter executes a large switch,
 * thousands of instructions of code, and this board has XCHAL_ICACHE_SIZE=0
 * (EMU-003): every instruction fetched comes from PSRAM through the MMU window.
 * So an emulated instruction could be expensive for either of two reasons:
 *
 *   (a) the interpreter retires many native instructions, as EMU-005 supposes,
 *       which a block-register rewrite would cut; or
 *   (b) each native instruction is cheap but its own FETCH costs a PSRAM
 *       access, which a block-register rewrite would not fix, because the code
 *       still has to be fetched.
 *
 * All three tests perform exactly the same arithmetic on exactly the same data:
 * a dependent chain `acc = acc * 31 + tbl[...]`, one runtime table load and one
 * multiply-add per unit of work. They differ only in how much code that work is
 * spread across:
 *
 *   small    a loop, a few dozen bytes of code
 *   medium   the same body unrolled 16 times, ~0.5 KB
 *   large    a 256-case switch, ~64 KB, cases entered in a jumping order --
 *            the shape of the interpreter's opcode dispatch
 *
 * The table load is deliberate and is the control. It makes the chain unfoldable
 * (a compiler cannot constant-fold a value it cannot see) and it puts an
 * identical dependent PSRAM access inside every variant, so any difference in
 * cost per operation is attributable to code footprint and nothing else. A
 * first version of this benchmark used only constants and gcc collapsed the
 * whole chain to a closed form, reporting 0.06 ns/op.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile uint32_t sink;
static uint32_t tbl[256];

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

#define OP(n) acc = acc * 31u + tbl[(n) & 255]

/* (1) Small footprint. */
static volatile long vbounds;   /* defeats compile-time trip-count reasoning */

static double bench_small(long ops) {
    uint32_t acc = 1;
    vbounds = ops;
    double t0 = now_s();
    for (long i = 0; i < vbounds; i++) OP(acc);
    double t1 = now_s();
    sink = acc;
    return (t1 - t0) * 1e9 / (double)ops;
}

/* (2) Medium: same work, 16 copies of the body, so the fetch stream walks ~16x
 * more code than the loop did. */
static double bench_medium(long ops) {
    uint32_t acc = 1;
    vbounds = ops / 16;
    double t0 = now_s();
    for (long i = 0; i < vbounds; i++) {
        OP(acc); OP(acc); OP(acc); OP(acc);
        OP(acc); OP(acc); OP(acc); OP(acc);
        OP(acc); OP(acc); OP(acc); OP(acc);
        OP(acc); OP(acc); OP(acc); OP(acc);
    }
    double t1 = now_s();
    sink = acc;
    return (t1 - t0) * 1e9 / (double)ops;
}

/* (3) Large: 256 cases of 32 operations each, entered in an order that jumps
 * around the body, which is what an opcode switch does. */
#define CHAIN 32
static uint32_t big_switch(uint32_t which, uint32_t acc) {
    switch (which & 63) {   /* only 64 cases are defined */
#define C(n) \
    case n: OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); \
            OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); \
            OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); \
            OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); OP(acc); \
            break;
    C(0) C(1) C(2) C(3) C(4) C(5) C(6) C(7)
    C(8) C(9) C(10) C(11) C(12) C(13) C(14) C(15)
    C(16) C(17) C(18) C(19) C(20) C(21) C(22) C(23)
    C(24) C(25) C(26) C(27) C(28) C(29) C(30) C(31)
    C(32) C(33) C(34) C(35) C(36) C(37) C(38) C(39)
    C(40) C(41) C(42) C(43) C(44) C(45) C(46) C(47)
    C(48) C(49) C(50) C(51) C(52) C(53) C(54) C(55)
    C(56) C(57) C(58) C(59) C(60) C(61) C(62) C(63)
#undef C
    default: OP(acc); break;
    }
    return acc;
}

static double bench_large(long ops) {
    uint32_t acc = 1, s = 0;
    vbounds = ops / CHAIN;
    double t0 = now_s();
    for (long i = 0; i < vbounds; i++) {
        s = s * 1103515245u + 12345u;
        acc = big_switch(s >> 16, acc);
    }
    double t1 = now_s();
    sink = acc + s;
    return (t1 - t0) * 1e9 / (double)ops;
}

int main(void) {
    const long OPS = 20000000;
    /*
     * Seed from the clock. An earlier version wrote tbl[i] = i*7+1 in a visible
     * loop, which gcc propagated through the whole translation unit: the medium
     * variant folded to a closed form and reported 0.00 ns/op. A seed the
     * compiler cannot predict makes every variant do real, dependent work.
     */
    struct timespec seed;
    clock_gettime(CLOCK_MONOTONIC, &seed);
    uint32_t s0 = (uint32_t)seed.tv_nsec | 1u;
    for (int i = 0; i < 256; i++) {
        s0 = s0 * 1103515245u + 12345u;
        tbl[i] = (s0 >> 16) | 1u;
    }
    sink = tbl[0];

    printf("code footprint vs cost per arithmetic op\n");
    fflush(stdout);
    printf("  small   (loop,      ~64 B of code) : %6.2f ns/op\n", bench_small(OPS));
    fflush(stdout);
    printf("  medium  (16x unroll, ~1 KB)        : %6.2f ns/op\n", bench_medium(OPS));
    fflush(stdout);
    printf("  large   (64 cases, jumps, ~8 KB)   : %6.2f ns/op\n", bench_large(OPS));
    fflush(stdout);
    printf("\nIdentical arithmetic and identical table loads in all three; only the\n");
    printf("code footprint differs. A flat profile means instruction fetch is covered\n");
    printf("by fetch-ahead and the interpreter's cost is its own arithmetic (EMU-005),\n");
    printf("so a block-register rewrite can win. A profile that climbs with footprint\n");
    printf("means the fetch is the cost and the rewrite cannot remove it.\n");
    return 0;
}
