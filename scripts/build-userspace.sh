#!/usr/bin/env bash
# Build the userspace side of the BLE keyboard: userspace/blekbd.
#
#   ./scripts/build-userspace.sh            build
#   ./scripts/build-userspace.sh --selftest build for the host and run the
#                                           report-descriptor test
#
# The binary is deployed to the watch's writable jffs2 partition (/home),
# because the rootfs is a read-only cramfs and there is no buildroot tree in
# this checkout to rebuild it:
#
#   nc -l -p 5556 > /home/blekbd.new        # on the watch
#   nc <watch-ip> 5556 < userspace/blekbd   # here
#   mv /home/blekbd.new /home/blekbd; chmod +x /home/blekbd
#
# scripts/build.sh's toolchain check does the same thing for the kernel; see
# docs/toolchain.md for why the environment variable below is not optional.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

TC=build/toolchain/crosstool-NG/builds/xtensa-esp32s3-linux-uclibcfdpic
CC=$TC/bin/xtensa-esp32s3-linux-uclibcfdpic-gcc
DYN=build/toolchain/xtensa-dynconfig/esp32s3.so

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

[ -x "$CC" ] || die "no cross compiler at $CC -- run ./scripts/build-kernel.sh once to build the toolchain"
[ -f "$DYN" ] || die "no $DYN -- the compiler needs it, see docs/toolchain.md"

# NOT optional. Without XTENSA_GNU_CONFIG the compiler falls back to a generic
# big-endian core: it emits MSB objects, and the link then fails against the
# little-endian uClibc in the sysroot with "file in wrong format". The error
# names libgcc_s.so.1 or libc.so.1 and says nothing about endianness, so it
# reads as a broken toolchain rather than a missing environment variable.
export XTENSA_GNU_CONFIG=$PWD/$DYN

case "${1:-}" in
--selftest)
    say() { printf '\n=== %s\n' "$*"; }
    say "host build"
    cc -O1 -Wall -Wextra -o /tmp/blekbd_host userspace/blekbd.c
    say "generate frames"
    python3 scripts/blekbd-selftest.py /tmp/blekbd-frames.bin
    say "decode"
    /tmp/blekbd_host -f /tmp/blekbd-frames.bin -v
    ;;
*)
    "$CC" -Os -Wall -Wextra -o userspace/blekbd userspace/blekbd.c
    ls -l userspace/blekbd
    printf '\ndeploy it to the watch; see the header of this script.\n'
    ;;
esac
