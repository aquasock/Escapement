#!/usr/bin/env bash
# pmbench.sh - build and run the Pokemon Mini core benchmark.
#
#   ./scripts/pmbench.sh fetch
#   ./scripts/pmbench.sh build
#   ./scripts/pmbench.sh run [rom.min|--synthetic] [frames]
#   ./scripts/pmbench.sh compare DIR [frames]
#   ./scripts/pmbench.sh target        # cross-compile a STANDALONE binary
#
# `target` DOES NOT PRODUCE SOMETHING THE WATCH CAN RUN. It links against this
# project's crosstool-NG and uClibc, so the result is a Linux ELF, and the watch
# runs ESP-IDF with TinyDesk Shell, which has no ELF loader. That was tried on
# 2026-11-13 and refused at execution (core-log entry 65).
#
# The on-watch measurement is the `pmbench` shell command built from
# `components/pokemini` in the tinydesk fork -- see docs/pokemini-scoping.md
# section 6 for the figure it produced. `target` is kept only for the Linux image.
#
# WHY THIS EXISTS. Same standard as nesbench.sh and gbcbench.sh: a platform
# decision is a measurement. This one has a second purpose, which is the whole
# point of the exercise -- the CPU requirement is geniunely unknown on the
# watch, and the only honest way to find out is to run the core there.
#
# THE CORE IS THE PROJECT'S OWN VENDORING. tinytama already vendors PokeMini at
# a pinned revision with one 2-line patch (an instruction counter) recorded in
# core/pokemini/patches/. Rather than fork that provenance, this script uses a
# checkout if one is present and otherwise fetches the pinned upstream and
# applies the same patch. Set POKEMINI_SRC to point at an existing tree --
# /run/media/vash/GIT/tinytama/core/pokemini is the natural one on this machine.
#
# THE REQUIREMENT IS NOT 1 MIPS. 1 MIPS is the hardware ceiling (4 MHz / 4-cycle
# fetch). A game needs instructions-per-frame x 72 Hz, which for the twelve
# retail cartridges measured here is 0.22-0.36 MIPS. See docs/pokemini-scoping.md.
#
# -DPERFORMANCE IS NOT OPTIONAL. It inlines the PRC's memory access instead of
# going through call pointers: measured 77.8 -> 96.7 M steps/sec on the host, a
# 24% gain for 1,220 bytes of text.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

SRC=${POKEMINI_SRC:-third_party/pokemini}
OUT=${PMBENCH_OUT:-build/pmbench}
REPO=${PMBENCH_REPO:-https://github.com/pokemon-mini/PokeMini.git}
PIN=${PMBENCH_PIN:-8a64225e21307ee89b18feca0a429a8d2d43e6d1}

fetch() {
    if [ -d "$SRC" ]; then
        echo "already at $SRC"
        return 0
    fi

    # Prefer the tree this project already vendored: it carries the patch and
    # the audit. Fetching upstream and re-applying the patch is the fallback so
    # this stays reproducible from a clean machine.
    local local_copy=/run/media/vash/GIT/tinytama/core/pokemini

    if [ -d "$local_copy" ]; then
        echo "copying the vendored core from $local_copy"
        mkdir -p "$(dirname "$SRC")"
        cp -r "$local_copy" "$SRC"
        echo "pokemini at $SRC (from tinytama, revision $PIN)"
        return 0
    fi

    mkdir -p "$(dirname "$SRC")"
    echo "cloning $REPO at $PIN"
    git clone -q "$REPO" "$SRC"
    git -C "$SRC" checkout -q "$PIN"

    # The instruction counter, which the harness needs and upstream does not have.
    # Two lines: a global and an increment. Applying it here rather than shipping
    # a modified core keeps the provenance checking to one diff.
    python3 - "$SRC/MinxCPU_XX.c" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
old = "int MinxCPU_Exec(void)\n{\n"
new = "unsigned long g_exec_count = 0;\nint MinxCPU_Exec(void)\n{\n\tg_exec_count++;\n"
assert old in s, "MinxCPU_Exec not found -- upstream layout changed, re-check the patch"
open(p, "w").write(s.replace(old, new, 1))
PY
    echo "pokemini at $SRC (fetched, counter patch applied)"
}

build() {
    [ -d "$SRC" ] || { echo "no pokemini at $SRC -- run: $0 fetch" >&2; return 1; }

    mkdir -p "$OUT"
    echo "building $OUT/pmbench"

    # -O2 because an -O0 figure would be a lie about interpreter speed.
    # -w because this is upstream code with its own warnings.
    gcc -O2 -std=gnu11 -w -DPERFORMANCE \
        -I"$SRC" \
        -o "$OUT/pmbench" \
        tools/pmbench/bench_pokemini.c \
        $(find "$SRC" -maxdepth 1 -name '*.c') \
        -lm
    echo "ok"
}

run() {
    [ -x "$OUT/pmbench" ] || build
    "$OUT/pmbench" "$@" 2>&1 \
        | grep -E '^core |^realtime|^per frame|^rom |^frames |BENCHMARK INVALID'
}

compare() {
    local dir=${1:?usage: $0 compare DIR [frames]}
    local frames=${2:-8000}
    local f

    [ -x "$OUT/pmbench" ] || build

    # Long runs matter: the same binary reports 107 M steps/sec at 200 frames and
    # 194 M at 20,000, because a short run measures the host's frequency ramp
    # rather than the core. Hence the 8,000 default.
    for f in "$dir"/*.min; do
        [ -e "$f" ] || continue

        line=$("$OUT/pmbench" "$f" "$frames" 2>/dev/null | grep '^core ' || true)

        if [ -n "$line" ]; then
            echo "$line  $(basename "$f")"
        else
            echo "SKIP      $(basename "$f") (unsupported or bad cartridge)"
        fi
    done
}

# The on-watch binary. This is the measurement that decides the platform.
target() {
    local cc=${PMBENCH_CC:-$here/build/toolchain/crosstool-NG/builds/xtensa-esp32s3-linux-uclibcfdpic/bin/xtensa-esp32s3-linux-uclibcfdpic-gcc}

    [ -x "$cc" ] || { echo "no cross compiler at $cc" >&2; return 1; }
    [ -d "$SRC" ] || { echo "no pokemini at $SRC -- run: $0 fetch" >&2; return 1; }

    mkdir -p "$OUT"
    echo "building $OUT/pokemini_bench_target"

    # XTENSA_GNU_CONFIG selects the S3's little-endian dynconfig. Without it the
    # compiler emits BIG-endian objects and the link fails against the
    # little-endian uClibc sysroot with a misleading "file in wrong format".
    XTENSA_GNU_CONFIG=$(find "$here/build/toolchain" -name 'esp32s3*.so' | head -1) \
    "$cc" -O2 -DPERFORMANCE -w -static \
        -I"$SRC" \
        -o "$OUT/pokemini_bench_target" \
        tools/pmbench/bench_pokemini.c \
        $(find "$SRC" -maxdepth 1 -name '*.c') \
        -lm
    echo "ok -- this is a Linux ELF; it runs on the Escapement Linux image,"
    echo "     NOT on TinyDesk. The TinyDesk measurement is the pmbench command."
}

case "${1:-}" in
    fetch)   fetch ;;
    build)   build ;;
    run)     shift; run "$@" ;;
    compare) shift; compare "$@" ;;
    target)  target ;;
    *)
        sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
esac
