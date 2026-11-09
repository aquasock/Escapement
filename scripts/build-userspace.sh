#!/usr/bin/env bash
# Build the userspace programs: userspace/blekbd and userspace/tbtns.
#
#   ./scripts/build-userspace.sh            build both
#   ./scripts/build-userspace.sh --selftest build for the host and run the
#                                           report-descriptor and touch-band
#                                           tests
#
# The binaries are deployed to the watch's writable jffs2 partition (/home),
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
    cc -O1 -Wall -Wextra -o /tmp/tbtns_host userspace/tbtns.c
    say "generate frames"
    python3 scripts/blekbd-selftest.py /tmp/blekbd-frames.bin
    say "decode"
    /tmp/blekbd_host -f /tmp/blekbd-frames.bin -v
    say "touch bands"
    python3 scripts/tbtns-selftest.py /tmp/tbtns_host
    ;;
*)
    "$CC" -Os -Wall -Wextra -o userspace/blekbd userspace/blekbd.c
    "$CC" -Os -Wall -Wextra -o userspace/tbtns userspace/tbtns.c
    "$CC" -Os -Wall -Wextra -o userspace/panel-test userspace/panel-test.c
    ls -l userspace/blekbd userspace/tbtns userspace/panel-test
    printf '\ndeploy them to the watch; see the header of this script.\n'
    ;;
esac
