#!/usr/bin/env bash
# Build tamacore, core 1's half of the Tamagotchi.
#
# The guest itself runs on core 0 now (firmware tamacore_task.c). What is left
# on this side is the blitter and the buttons, which is why this binary is small
# and has no emulator in it at all -- splb32, the segmap and the renderer all
# live in the firmware image.
#
# The two halves share one header, patches/harness/core0/shmproto.h, so the
# window layout cannot drift between them. The firmware includes the same file
# by copying it into its main/ directory at build time.
#
# Usage: ./scripts/build-tamacore.sh [outfile]
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

TC=build/toolchain/crosstool-NG/builds/xtensa-esp32s3-linux-uclibcfdpic
CC=$TC/bin/xtensa-esp32s3-linux-uclibcfdpic-gcc
OUT=${1:-userspace/tamacore}

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

[ -x "$CC" ] || die "no cross compiler at $CC"

# NOT optional: without it the compiler emits big-endian objects.
export XTENSA_GNU_CONFIG=$here/build/toolchain/xtensa-dynconfig/esp32s3.so

"$CC" -O2 -Wall -Wextra -static \
    -I"$here/patches/harness/core0" \
    -o "$OUT" "$here/userspace/tamacore.c"

ls -l "$OUT"
printf '\ndeploy with ./scripts/watch-deploy.sh %s\n' "$(basename "$OUT")"
