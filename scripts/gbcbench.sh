#!/usr/bin/env bash
# gbcbench.sh - build and run the Game Boy / Game Boy Color core benchmark.
#
#   ./scripts/gbcbench.sh fetch          # clone the gnuboy this was built against
#   ./scripts/gbcbench.sh build
#   ./scripts/gbcbench.sh run ROM [frames]
#   ./scripts/gbcbench.sh compare DIR [frames]
#
# WHY THIS EXISTS. Same reason as nesbench.sh: choosing an emulator core is a
# measurement, not a reading of source files, and the measurement has to be
# repeatable after the core is vendored into the firmware.
#
# THE FORMAT IS THE POINT. gnuboy can hand over pixels already in the panel's
# byte order (GB_PIXEL_565_BE). The watch's panel wants exactly that, so a port
# using gnuboy needs NO per-pixel conversion -- where the NES port spends 4,355
# us of a 23,651 us frame on a 256-entry palette lookup. The 565BE figure is what
# a port would actually get; the paletted figure is measured alongside it so the
# cost of the conversion is known rather than assumed.
#
# THE HOST NUMBER IS NOT THE WATCH NUMBER. A GB interpreter is pointer-chasing
# and branch-heavy and the Xtensa core differs from x86 exactly there. What
# transfers is the ratio BETWEEN CORES and the ratio against the Game Boy's own
# 4.194304 MHz, because every core emulates the same clock and does the same
# abstract work.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

SRC=${GBCBENCH_SRC:-third_party/gnuboy}
OUT=${GBCBENCH_OUT:-build/gbcbench}
REPO=${GBCBENCH_REPO:-https://github.com/ducalex/retro-go.git}
SUBDIR=retro-core/components/gnuboy

# The same upstream commit the NES core is vendored from, so both benchmarks
# describe one tree.
PIN=${GBCBENCH_PIN:-4ced120}

# The second core, for the speed/accuracy comparison. It lives in its own
# repository, so it has its own fetch target and its own pin.
BINJGB_SRC=${GBCBENCH_BINJGB_SRC:-third_party/binjgb}
BINJGB_REPO=${GBCBENCH_BINJGB_REPO:-https://github.com/binji/binjgb.git}
BINJGB_PIN=${GBCBENCH_BINJGB_PIN:-1662111}

fetch() {
    if [ -d "$SRC" ]; then
        echo "already at $SRC"
        return 0
    fi
    mkdir -p "$(dirname "$SRC")"
    echo "cloning $REPO at $PIN"
    git clone --filter=blob:none --no-checkout "$REPO" "$SRC.tmp"
    git -C "$SRC.tmp" sparse-checkout init --cone
    git -C "$SRC.tmp" sparse-checkout set "$SUBDIR"
    git -C "$SRC.tmp" checkout "$PIN"
    mv "$SRC.tmp/$SUBDIR" "$SRC"
    rm -rf "$SRC.tmp"
    echo "gnuboy at $SRC"
}

fetch_binjgb() {
    if [ -d "$BINJGB_SRC" ]; then
        echo "already at $BINJGB_SRC"
        return 0
    fi
    mkdir -p "$(dirname "$BINJGB_SRC")"
    echo "cloning $BINJGB_REPO at $BINJGB_PIN"
    git clone -q "$BINJGB_REPO" "$BINJGB_SRC"
    git -C "$BINJGB_SRC" checkout -q "$BINJGB_PIN"
    echo "binjgb at $BINJGB_SRC"
}

# binjgb is built from its own tree with two files excluded: src/binjgb.c and
# src/tester.c each define main(), and the harness defines its own.
build_binjgb() {
    [ -d "$BINJGB_SRC" ] || { echo "no binjgb at $BINJGB_SRC -- run: $0 fetch-binjgb" >&2; return 1; }

    mkdir -p "$OUT"
    echo "building $OUT/bench_binjgb"

    gcc -O2 -std=gnu11 -w \
        -I"$BINJGB_SRC/src" \
        -o "$OUT/bench_binjgb" \
        tools/gbcbench/bench_binjgb.c \
        $(find "$BINJGB_SRC/src" -maxdepth 1 -name '*.c' \
          ! -name 'binjgb.c' ! -name 'tester.c' \
          ! -name 'host*.c' ! -name 'emulator-debug.c') \
        -lm
    echo "ok"
}

build() {
    [ -d "$SRC" ] || { echo "no gnuboy at $SRC -- run: $0 fetch" >&2; return 1; }

    mkdir -p "$OUT"
    echo "building $OUT/bench_gnuboy"

    # -O2 because an -O0 figure would be a lie about interpreter speed.
    # -w because this is upstream code with its own warnings.
    gcc -O2 -std=gnu11 -w \
        -I"$SRC" \
        -o "$OUT/bench_gnuboy" \
        tools/gbcbench/bench_gnuboy.c \
        $(find "$SRC" -maxdepth 1 -name '*.c') \
        -lm
    echo "ok"
}

# Only the summary line is kept. the core's own MESSAGE_INFO chatter goes to
# stdout and would otherwise bury the result -- the same reason nesbench.sh
# greps for '^nofrendo '.
run() {
    [ -x "$OUT/bench_gnuboy" ] || build
    "$OUT/bench_gnuboy" "$@" 2>&1 | grep -E '^gnuboy |BENCHMARK INVALID'
}

compare() {
    local dir=${1:?usage: $0 compare DIR [frames]}
    local frames=${2:-1200}
    local f

    [ -x "$OUT/bench_gnuboy" ] || build

    for f in "$dir"/*.gb "$dir"/*.gbc; do
        [ -e "$f" ] || continue

        for fmt in 565be paletted; do
            line=$("$OUT/bench_gnuboy" "$f" "$frames" $fmt 2>/dev/null \
                   | grep '^gnuboy ' || true)

            if [ -n "$line" ]; then
                echo "$line"
            else
                echo "SKIP      $(basename "$f") [$fmt] (unsupported mapper or bad ROM)"
            fi
        done
    done
}

case "${1:-}" in
    fetch)        fetch ;;
    fetch-binjgb) fetch_binjgb ;;
    build)        build ;;
    build-binjgb) build_binjgb ;;
    run)     shift; run "$@" ;;
    compare) shift; compare "$@" ;;
    *)
        sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
esac
