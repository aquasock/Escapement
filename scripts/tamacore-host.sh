#!/usr/bin/env bash
# tamacore-host.sh - build and run the SPLB32 emulator on this machine.
#
#   ./scripts/tamacore-host.sh build
#   ./scripts/tamacore-host.sh run --rom Tamagotchi_v3_us_eu.bin --seconds 5
#   ./scripts/tamacore-host.sh frames PREFIX    # turn the PPMs into a contact sheet
#
# WHY THIS EXISTS. The Tamacore port needs the emulator's portable half running
# where iteration is cheap. A firmware cycle is a ~20 s flash plus a manual look
# at a 40-column screen; this is a build of a few files and a PPM you can open.
# It also gives a speed baseline on a machine with a known clock, so the
# ESP32-S3 figure later has something honest to be measured against rather than
# a bare number with no denominator.
#
# THE SOURCES ARE COMPILED IN PLACE, NOT COPIED. tools/tamacore/tmc_host.c is
# new; everything else is the Linux project's own emulator, compiled straight
# out of patches/harness so the two cannot drift. If a file here grows a
# firmware dependency, this build breaks first -- which is the point: the set of
# files that compile on a host IS the portable subset, stated as a build rule
# rather than as an opinion in a comment.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

OUT=${TMC_OUT:-build/tamacore-host}

SRCS=(
    tools/tamacore/tmc_host.c
    patches/harness/splb32/splb32.c
    patches/harness/splb32/splb32_sound.c
    patches/harness/splb32/ht24lc08.c
    patches/harness/splb32/con_v3_ir.c
    patches/harness/segmap-1x/segmap.c
    patches/harness/core0/tmc_core.c
)

# The dirty-row check is its own program: it links the core but no renderer
# output, and it returns a status, so it can gate a commit.
CHECK_SRCS=(
    tools/tamacore/check_dirty.c
    patches/harness/splb32/splb32.c
    patches/harness/splb32/splb32_sound.c
    patches/harness/splb32/ht24lc08.c
    patches/harness/splb32/con_v3_ir.c
    patches/harness/segmap-1x/segmap.c
    patches/harness/core0/tmc_core.c
)

INCS=(
    -Ipatches/harness/splb32
    -Ipatches/harness/segmap-1x
    -Ipatches/harness/core0
)

build() {
    mkdir -p "$OUT"
    echo "building $OUT/tmc_host"
    # -O2 because a -O0 number would be a lie about the emulator's speed;
    # -Wno-unused because these files are shared with a firmware build that has
    # its own flag set and the host compiler is stricter about a few of them.
    # gnu11 rather than c11: clock_gettime and CLOCK_MONOTONIC need
    # _POSIX_C_SOURCE under strict ISO C, and tightening the standard here would
    # be a portability claim this harness does not need to make.
    gcc -std=gnu11 -O2 -Wall -Wextra \
        -Wno-unused-parameter -Wno-unused-but-set-variable \
        "${INCS[@]}" -o "$OUT/tmc_host" "${SRCS[@]}" -lm
    echo "ok"
}

# Does the dirty-row range cover every pixel that changed? Exits non-zero if a
# changed pixel falls outside the reported range, which on the watch would show
# as one stale row.
check() {
    mkdir -p "$OUT"
    echo "building $OUT/check_dirty"
    gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter \
        "${INCS[@]}" -o "$OUT/check_dirty" "${CHECK_SRCS[@]}" -lm
    "$OUT/check_dirty" "$@"
}

# Why a beep per "audio event" never stopped: the emit counter counts square-wave
# toggles, not sounds. Links splb32_sound.c alone -- no CPU, no ROM -- because the
# question is arithmetic about that one model.
audio() {
    mkdir -p "$OUT"
    echo "building $OUT/check_audio"
    gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter \
        -Ipatches/harness/splb32 -o "$OUT/check_audio" \
        tools/tamacore/check_audio.c patches/harness/splb32/splb32_sound.c
    "$OUT/check_audio"
}

run() {
    [ -x "$OUT/tmc_host" ] || build
    "$OUT/tmc_host" "$@"
}

# A contact sheet helps when a frame looks wrong and the question is "wrong since
# when". ImageMagick is not assumed; if it is missing this says so and does
# nothing rather than failing the cycle.
frames() {
    local prefix=${1:-frame}
    command -v montage >/dev/null 2>&1 || {
        echo "montage not found (ImageMagick); the PPMs are in $prefix-*.ppm" >&2
        return 1
    }
    montage "$prefix"-*.ppm -tile 4x -geometry +2+2 "$prefix-sheet.png"
    echo "$prefix-sheet.png"
}

case "${1:-}" in
    build) build ;;
    check) shift; check "$@" ;;
    run) shift; run "$@" ;;
    frames) shift; frames "$@" ;;
    audio) audio ;;
    *)
        sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
esac
