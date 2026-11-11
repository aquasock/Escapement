#!/usr/bin/env bash
# Build tamafb at the guest LCD's true resolution (1 pixel per LCD dot).
#
# The emulator front end lives in the sibling tree
# ~/GIT/TamaHarness/portable, which is NOT a git repository. This script and
# the segment map beside it exist so the binary this project deploys can be
# reproduced from THIS repo without depending on that tree's working state.
#
# Why a custom map at all: the harness renders the V3 dot-matrix art at a 5 px
# dot on a 6 px pitch, giving a 292x300 LCD so the pet is readable across the
# room. At that size a dirty frame touches 6 of 10 bands and the panel transfer
# is 34 percent of the emulator's frame time. At the hardware's real resolution
# -- 49x49 -- a frame touches 1-2 bands and the panel drops to about 2 percent.
# The pet is tiny on the glass. That is the trade, and it was taken knowingly.
#
# Why the explicit FB_W/FB_H: tamafb.c defaults them to SEG_W+32 by SEG_H+48,
# which is 81x97 for this map and NOT the watch's 320x400 fb0. The file's own
# comment says those defaults exist to be overridden by a board whose
# framebuffer is a different size; the watch build never passed them, so every
# band was written with a 162-byte row stride into a 640-byte framebuffer. At
# the 5 px scale that error was 4 px horizontal and 52 px vertical and looked
# merely slightly off; at 49 px it was a full-width smear. Caught on the glass,
# fixed by passing the real geometry.
#
# Usage: ./scripts/build-tamafb.sh [outfile]
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

TC=build/toolchain/crosstool-NG/builds/xtensa-esp32s3-linux-uclibcfdpic
CC=$TC/bin/xtensa-esp32s3-linux-uclibcfdpic-gcc
H=${HARNESS:-/run/media/vash/GIT/TamaHarness/portable}
OUT=${1:-userspace/tamafb-1x}

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

[ -x "$CC" ] || die "no cross compiler at $CC"
[ -d "$H" ] || die "harness sources not found at $H (set HARNESS=...)"
for f in splb32.c ht24lc08.c con_v3_ir.c splb32_sound.c tamafb.c; do
    [ -f "$H/$f" ] || die "missing $H/$f"
done

# NOT optional: without it the compiler emits big-endian objects.
export XTENSA_GNU_CONFIG=$here/build/toolchain/xtensa-dynconfig/esp32s3.so

# -DFB_W/-DFB_H are the point of this script; see the comment above.
# No -DEMBED_ROM: the deployed binary reads /home/rom.bin, which keeps it at
# 255 KB instead of 779 KB (the ROM is a 512 KB blob that must be one
# contiguous allocation, and it is loaded at runtime anyway).
# splb32.c comes from THIS repo, not from the harness tree. The interpreter is
# under active optimisation and TamaHarness is not a git repository, so taking
# it from there would mean every change is unversioned and unrevertible. The
# vendored copy in patches/harness/splb32/ is the one that builds, and the only
# thing still read from $HARNESS is the front end and its peripherals.
"$CC" -O2 -Wall -Wextra -static \
    -DFB_W=320 -DFB_H=400 \
    -I"$here/patches/harness/segmap-1x" \
    -I"$here/patches/harness/splb32" -I"$H" \
    -o "$OUT" \
    "$here/patches/harness/splb32/splb32.c" \
    "$H/ht24lc08.c" "$H/con_v3_ir.c" "$H/splb32_sound.c" \
    "$H/tamafb.c" "$here/patches/harness/segmap-1x/segmap.c"

ls -l "$OUT"
printf '\ndeploy with ./scripts/watch-deploy.sh %s\n' "$(basename "$OUT")"
