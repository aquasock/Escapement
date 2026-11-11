#!/usr/bin/env bash
# Copy the emulator sources this repo owns into the esp-hosted firmware tree.
#
# The firmware tree is a fork of jcmvbkbc/esp-hosted and is NEVER pushed, so it
# cannot be where anything is maintained. But the emulator on core 0 has to be
# compiled into that image, and it is built from sources that live here:
#
#   patches/harness/splb32/     the interpreter and its peripherals
#   patches/harness/segmap-1x/  the 49x49 segment map, one pixel per LCD dot
#   patches/harness/core0/      the window contract and the core-0 task
#
# So they are copied in rather than duplicated. Edit them here, run this, build.
# Running it twice is harmless; it only copies.
#
# Usage: ./scripts/sync-firmware.sh [--check]
#   --check  report drift and exit non-zero instead of copying
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

FW=build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main
HARNESS=${HARNESS:-/run/media/vash/GIT/TamaHarness/portable}
CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

[ -d "$FW" ] || die "$FW not found. Run ./scripts/build.sh first."

# repo-relative source -> firmware filename
SOURCES=(
    "patches/harness/splb32/splb32.c"
    "patches/harness/splb32/splb32.h"
    "patches/harness/splb32/ht24lc08.c"
    "patches/harness/splb32/ht24lc08.h"
    "patches/harness/splb32/con_v3_ir.c"
    "patches/harness/splb32/con_v3_ir.h"
    "patches/harness/splb32/splb32_sound.c"
    "patches/harness/splb32/splb32_sound.h"
    "patches/harness/segmap-1x/segmap.c"
    "patches/harness/segmap-1x/segmap.h"
    "patches/harness/core0/render.h"
    "patches/harness/core0/shmproto.h"
    "patches/harness/core0/tamacore_task.c"
    "patches/harness/core0/esc_instr.c"
)

# render.h is the segment-map renderer, shared with the desktop front end and
# not modified here, so it comes straight from the harness checkout. It is
# copied rather than edited: any change to it should be made upstream and
# vendored deliberately, not drift in quietly.
EXTERNAL=()

drift=0
for src in "${SOURCES[@]}"; do
    name=$(basename "$src")
    [ -f "$src" ] || die "missing $src"
    if [ -f "$FW/$name" ] && cmp -s "$src" "$FW/$name"; then
        continue
    fi
    drift=1
    if [ "$CHECK" = 0 ]; then
        cp "$src" "$FW/$name"
        printf '  copied %s\n' "$name"
    else
        printf '  differs %s\n' "$name"
    fi
done

for src in "${EXTERNAL[@]}"; do
    name=$(basename "$src")
    [ -f "$src" ] || die "missing $src (set HARNESS=...)"
    if [ -f "$FW/$name" ] && cmp -s "$src" "$FW/$name"; then
        continue
    fi
    drift=1
    if [ "$CHECK" = 0 ]; then
        cp "$src" "$FW/$name"
        printf '  copied %s (from %s)\n' "$name" "$HARNESS"
    else
        printf '  differs %s\n' "$name"
    fi
done

if [ "$CHECK" = 1 ]; then
    [ "$drift" = 0 ] || die "firmware tree is out of step; run without --check to fix"
    printf '\nfirmware tree matches this repo\n'
else
    printf '\nsynced. build with:\n'
    printf '  export IDF_TOOLS_PATH=$PWD/build/idf-tools\n'
    printf '  source %s/../../esp-idf/export.sh\n' "$FW"
    printf '  ninja -C %s/../build\n' "$FW"
fi
