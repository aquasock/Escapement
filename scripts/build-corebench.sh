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
#   userspace/corebench-flash ROM read from the memory-mapped flash window at
#                            0x42480000, so it allocates nothing at all. ~78 KB
#                            to deploy and no contiguity requirement beyond its
#                            own load segment. THIS IS THE ONE THAT RUNS: on
#                            2026-11-10 the 601 KB build failed to exec with
#                            'nommu: Allocation of length 565248 ... failed',
#                            the largest free block being 512 KB.
#
#   userspace/corebench-rom  No ROM inside and no flash window. ~78 KB, needs
#                            `corebench rom.bin`, and malloc()s the file.
#
# Both the embedded and the file variants need a large contiguous block, one at
# exec time and the other at malloc time, which is why the flash window exists:
# this is NOMMU (KRN-001) and a 512 KB block survives only while memory is
# unfragmented. EXT-002 is the record behind the window.
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

build corebench       rom_embed.c -DEMBED_ROM
build corebench-flash ""         -DROM_FLASH_ADDR=0x42480000
build corebench-prof  ""         -DROM_FLASH_ADDR=0x42480000 -DSPLB32_PROF
build corebench-rom   ""

printf '\nBuilt. The flash variant allocates no ROM at all and is the one to use:\n\n'
printf '    ./scripts/watch-deploy.sh corebench-flash\n'
printf '    ./scripts/watch-telnet.py "/home/corebench-flash --cycles 40000000"\n\n'
printf 'It reads the ROM at 0x42480000, which is flash offset 0x480000. Verify\n'
printf 'that slot still holds the ROM before trusting a measurement from it.\n'
printf 'The other two need a large contiguous block (KRN-001), which on this board\n'
printf 'means a fresh boot, and the 601 KB one may not fit at all.\n\n'
printf 'corebench-prof counts interpreter steps and guest bus accesses per retired\n'
printf 'instruction. Its wall-clock figures are PERTURBED by the counters -- use it\n'
printf 'for counts only, and corebench-flash for timing.\n'
