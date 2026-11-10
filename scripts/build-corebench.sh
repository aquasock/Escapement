#!/usr/bin/env bash
# Cross-compile TamaHarness's corebench for the watch.
#
#   ./scripts/build-corebench.sh            build both variants
#   ./scripts/build-corebench.sh --selftest also build and run it on the host
#
# Why this exists at all: corebench is the only program that runs the SPLB32
# interpreter with no panel behind it, so it is the instrument for measuring the
# interpreter itself. Entry 18 of .ai/core-log.md left the interpreter's cost
# unmeasured (EMU-005 is OPEN) and the decision between a block-register rewrite
# and the core-0 design turns on that number. corebench has never been deployed
# from a script; it was built by hand twice and both deploys failed for want of
# memory, which is why the two variants below exist and why their sizes matter.
#
# The two variants, and why both are built:
#
#   userspace/corebench      ROM compiled in (-DEMBED_ROM). One file to deploy,
#                            takes no arguments. The ROM lands in .rodata, so
#                            the binary is ~600 KB and the kernel must find one
#                            contiguous run that size at exec time.
#
#   userspace/corebench-rom  No ROM inside. ~78 KB, needs `corebench rom.bin`,
#                            and malloc()s 512 KB contiguously at startup.
#
# Both need a large contiguous block, because this is NOMMU (KRN-001): the one
# at exec time, the other at malloc time. A fresh boot is the only reliable
# moment for either -- the largest free block was measured at 256 KB after a
# few minutes of uptime, against ~850 KB at boot.
#
# Source lives outside this repository, in the TamaHarness checkout beside it.
# Override the location with TAMAHARNESS=/path/to/TamaHarness/portable.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

TAMA=${TAMAHARNESS:-$here/../TamaHarness/portable}
TC=$here/build/toolchain/crosstool-NG/builds/xtensa-esp32s3-linux-uclibcfdpic/bin
CC=$TC/xtensa-esp32s3-linux-uclibcfdpic-gcc
SIZE=$TC/xtensa-esp32s3-linux-uclibcfdpic-size
READELF=$TC/xtensa-esp32s3-linux-uclibcfdpic-readelf
DYN=$here/build/toolchain/xtensa-dynconfig/esp32s3.so

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

[ -x "$CC" ] || die "no cross compiler at $CC -- run ./scripts/build-kernel.sh once to build the toolchain"
[ -f "$DYN" ] || die "no $DYN -- the compiler needs it, see docs/toolchain.md"
[ -d "$TAMA" ] || die "no TamaHarness at $TAMA -- set TAMAHARNESS=/path/to/TamaHarness/portable"

# NOT optional; see the note in scripts/build-userspace.sh and TOL-001.
export XTENSA_GNU_CONFIG=$DYN

# corebench itself; splb32_sound and con_v3_ir are linked because splb32.c
# calls into them, ht24lc08 because the EEPROM model is part of the bus.
SRCS="splb32.c ht24lc08.c con_v3_ir.c splb32_sound.c corebench.c"
CFLAGS="-O2 -Wall -Wextra"

cd "$TAMA"
for f in $SRCS; do [ -f "$f" ] || die "missing source $TAMA/$f"; done

if [ "${1:-}" = "--selftest" ]; then
    printf '\n=== host build and run (checks the sources still compile and work)\n'
    cc $CFLAGS -DEMBED_ROM -o /tmp/corebench_host \
        $SRCS rom_embed.c
    /tmp/corebench_host --cycles 40000000 | sed 's/^/    /'
fi

# Dynamic, matching what the other userspace binaries do (TOL-002). -static is
# not used here: the static build is over 700 KB and would not fit.
build() {
    local out=$1 extra=$2; shift 2
    printf '\n=== %s\n' "$out"
    "$CC" $CFLAGS "$@" -o "$here/userspace/$out" $SRCS $extra
    local bytes
    bytes=$(stat -c %s "$here/userspace/$out")
    printf '    size    %s bytes\n' "$bytes"
    # The largest PT_LOAD is what exec has to satisfy contiguously; report it
    # rather than leaving the number to be discovered on the watch.
    "$READELF" -l "$here/userspace/$out" \
        | awk '$1=="LOAD" { printf "    LOAD    filesz %s  memsz %s\n", $5, $6 }'
}

build corebench     rom_embed.c -DEMBED_ROM
build corebench-rom ""

printf '\nBoth built. Deploy and run on a FRESH boot -- exec and malloc are both\n'
printf 'subject to the contiguity rule (KRN-001), and the largest free block is\n'
printf 'largest at boot:\n\n'
printf '    ./scripts/watch-deploy.sh corebench\n'
printf '    ./scripts/watch-telnet.py "/home/corebench"\n\n'
printf 'or the small one, which needs the ROM as a file:\n\n'
printf '    ./scripts/watch-deploy.sh corebench-rom\n'
printf '    ./scripts/watch-telnet.py "/home/corebench-rom /home/rom.bin"\n'
