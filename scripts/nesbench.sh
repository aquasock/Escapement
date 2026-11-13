#!/usr/bin/env bash
# nesbench.sh - build and run the NES core benchmark on this machine.
#
#   ./scripts/nesbench.sh fetch          # clone the nofrendo this was built against
#   ./scripts/nesbench.sh build
#   ./scripts/nesbench.sh run ROM.nes [frames]
#   ./scripts/nesbench.sh compare DIR    # one line per ROM in a directory
#
# WHY THIS EXISTS. Choosing an NES core is a measurement, not a reading of source
# files, and the measurement has to be repeatable after the core is vendored into
# the firmware. This builds nofrendo out of a checkout and runs it headless for a
# fixed number of frames with the render path and the palette conversion both
# live, so the number includes everything a port would have to do.
#
# THE HARNESS ALSO GUARDS THE MEASUREMENT, which matters more than the number.
# Three separate ways of getting a fast-but-fake result were found while writing
# it -- a frame buffer never set, realtime arithmetic off by the frame rate, and
# a ROM header read into a local so the mapper never initialised. All three ran
# to completion and printed plausible speeds. Every one was caught by checksuming
# the whole frame buffer, so `run` FAILS when the frame buffer is empty rather
# than reporting a speed for a machine that never drew anything.
#
# THE HOST NUMBER IS NOT THE WATCH NUMBER. A 6502 interpreter is pointer-chasing
# and branch-heavy, and the Xtensa core differs from x86 exactly there, so the
# absolute ratio here does not transfer. What transfers is the ratio BETWEEN
# CORES, because they do the same abstract work. Treat this as a regression gate
# and a core-comparison tool, not as a prediction of the frame rate.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

SRC=${NESBENCH_SRC:-$here/third_party/nofrendo}
OUT=${NESBENCH_OUT:-build/nesbench}
REPO=${NESBENCH_REPO:-https://github.com/ducalex/retro-go.git}

# The upstream commit the firmware component is vendored from. Pinning it is what
# makes a benchmark run today comparable to one run after the component exists.
#
# The core lives inside retro-go's tree rather than in a repository of its own,
# which is why this fetches the whole thing and uses a subdirectory. The clone is
# sparse in effect -- only retro-core/components/nofrendo is ever read.
PIN=${NESBENCH_PIN:-4ced120}

fetch() {
    if [ -d "$SRC" ]; then
        echo "already at $SRC"
        return 0
    fi
    mkdir -p "$(dirname "$SRC")"
    echo "cloning $REPO at $PIN"
    git clone --filter=blob:none --no-checkout "$REPO" "$SRC.tmp"
    git -C "$SRC.tmp" sparse-checkout init --cone
    git -C "$SRC.tmp" sparse-checkout set retro-core/components/nofrendo
    git -C "$SRC.tmp" checkout "$PIN"
    mv "$SRC.tmp/retro-core/components/nofrendo" "$SRC"
    rm -rf "$SRC.tmp"
    echo "nofrendo at $SRC"
}

build() {
    [ -d "$SRC" ] || { echo "no nofrendo at $SRC -- run: $0 fetch" >&2; return 1; }

    mkdir -p "$OUT"
    echo "building $OUT/bench_nofrendo"

    # -O2 because an -O0 figure would be a lie about interpreter speed.
    # -w because this is upstream code with its own warnings; the port does not
    # own them and silencing them here keeps the output to one line.
    # nofrendo.c is excluded: it is the retro-go front end, and the whole point
    # is that the core builds without it.
    gcc -O2 -std=gnu11 -w \
        -I"$SRC" -I"$SRC/nes" \
        -o "$OUT/bench_nofrendo" \
        tools/nesbench/bench_nofrendo.c \
        $(find "$SRC" -name '*.c' ! -name 'nofrendo.c') \
        -lm
    echo "ok"
}

run() {
    [ -x "$OUT/bench_nofrendo" ] || build
    "$OUT/bench_nofrendo" "$@"
}

compare() {
    local dir=${1:?usage: $0 compare DIR [frames]}
    local frames=${2:-600}
    local f

    [ -x "$OUT/bench_nofrendo" ] || build

    for f in "$dir"/*.nes; do
        [ -e "$f" ] || continue
        # nofrendo's own startup chatter goes to stdout, so only the summary
        # line is kept -- the one starting with the core's name. A ROM the core
        # cannot map produces no such line and is reported as SKIP rather than
        # being counted as a zero-speed result.
        line=$("$OUT/bench_nofrendo" "$f" "$frames" 2>/dev/null | grep '^nofrendo ' || true)

        if [ -n "$line" ]; then
            echo "$line"
        else
            echo "SKIP     $(basename "$f") (unsupported mapper or bad ROM)"
        fi
    done
}

case "${1:-}" in
    fetch)   fetch ;;
    build)   build ;;
    run)     shift; run "$@" ;;
    compare) shift; compare "$@" ;;
    *)
        sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
esac
