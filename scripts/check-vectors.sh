#!/usr/bin/env bash
# Verify that the kernel's exception vectors land inside the buffer the
# firmware reserves for them.
#
# The firmware reserves a 4 KiB IRAM buffer, space_for_vectors, and Linux
# copies its exception vectors there at boot: CONFIG_VECTORS_ADDR is defined,
# so MERGED_VECTORS is 0 and head.S relocates each vector to VECBASE. Linux
# reserves no memory for this -- it assumes the firmware left that space empty.
#
# The address is chosen by the firmware's LINKER, not by anyone here.
# space_for_vectors is __attribute__((aligned(8192))), so it sits on the next
# 8 KiB boundary after whatever IRAM content precedes it and moves a page
# whenever the firmware grows past one. Adding IRAM_ATTR code is enough.
#
# If the kernel points at the old page, Linux writes its vectors over live
# firmware code. The board then boots clean and dies later, as soon as
# something calls whatever got clobbered -- which reads as a mysterious hang
# rather than as an address mismatch.
#
# Nothing used to enforce this. scripts/build.sh compared the two, but it only
# builds and never flashes; scripts/build-kernel.sh grepped for a hardcoded
# literal, so it passed regardless of the firmware; and the flash route in
# .codewhale/handoff.md (ninja + esptool write_flash) ran neither.
#
#   ./scripts/check-vectors.sh        check; exit 1 and explain on mismatch
#   ./scripts/check-vectors.sh -v     also print both addresses when they agree
#
# Exit 0 means the pair is safe to flash together.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

VERBOSE=0
[ "${1:-}" = "-v" ] && VERBOSE=1

FW_ELF=${FW_ELF:-build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build/network_adapter.elf}
KCONF=${KCONF:-build/kernel/linux-7.2.4/.config}
KMAP=${KMAP:-build/kernel/linux-7.2.4/System.map}

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

# ---- the firmware side -----------------------------------------------------
# The symbol lives in the firmware ELF, so a firmware rebuild is what moves it.
# Without the ELF there is nothing to compare against, and guessing is how this
# went wrong, so refuse rather than pass.
[ -f "$FW_ELF" ] || die "no firmware ELF at $FW_ELF
  Build it first (see .codewhale/handoff.md), or pass FW_ELF=/path/to/network_adapter.elf.
  Refusing to certify a kernel against a firmware that is not here."

NM=""
for c in "$here"/build/idf-tools/tools/xtensa-esp32s3-elf/*/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-nm \
         "$(command -v xtensa-esp32s3-elf-nm || true)"; do
    [ -x "$c" ] && { NM=$c; break; }
done
[ -n "$NM" ] || die "no xtensa-esp32s3-elf-nm found. Run ./scripts/build.sh, or source an ESP-IDF export.sh."

# awk exits on the first match, which sends SIGPIPE to nm; under pipefail that
# would abort the script with 141 instead of reporting the result.
fw_raw=$({ "$NM" "$FW_ELF" 2>/dev/null || true; } | awk '$3=="space_for_vectors" {print $1; exit}')
[ -n "$fw_raw" ] || die "space_for_vectors not found in $FW_ELF.
  The firmware patch that reserves it (linux_boot.c) is missing from this build."

# ---- the kernel side -------------------------------------------------------
[ -f "$KCONF" ] || die "no kernel .config at $KCONF. Build the kernel first."
k_raw=$(sed -n 's/^CONFIG_VECTORS_ADDR=0x\([0-9a-fA-F]*\).*/\1/p' "$KCONF" | head -1)
[ -n "$k_raw" ] || die "CONFIG_VECTORS_ADDR is not set in $KCONF.
  Without it the kernel uses its own vector layout and does not need the
  firmware's buffer -- but this port's firmware reserves one, so that would be
  a change of design, not a fix."

# Normalise: the config may carry leading zeros or a different case.
norm() { printf '%08x' "0x$1"; }
fw=$(norm "$fw_raw")
k=$(norm "$k_raw")

if [ "$fw" != "$k" ]; then
    cat >&2 <<EOF
escapement: VECTORS ADDRESS MISMATCH -- do not flash this pair.

  firmware  space_for_vectors   = 0x$fw   (from $FW_ELF)
  kernel    CONFIG_VECTORS_ADDR = 0x$k   (from $KCONF)

Linux would copy its exception vectors to 0x$k and overwrite 41+ bytes of
whatever the firmware put there. The board boots and then dies later, with no
message that points at this.

The firmware's linker owns this address, so the kernel is what has to move:

  sed -i 's/^CONFIG_VECTORS_ADDR=.*/CONFIG_VECTORS_ADDR=0x$fw/' \\
      patches/kernel/config/devkit_c1_16m_linux.config
  ./scripts/build-kernel.sh

Then re-run this script. Do not hand-edit the built .config: the next build
overwrites it from the patch-carried config.
EOF
    exit 1
fi

# ---- the span check --------------------------------------------------------
# Equal addresses are necessary but not sufficient: the kernel's vectors must
# also FIT in the 4 KiB the firmware reserved. This catches a config that is
# plausible but wrong, which a string comparison cannot.
if [ -f "$KMAP" ]; then
    lo=$(awk '$3=="_WindowVectors_text_start" {print $1; exit}' "$KMAP")
    hi=$(awk '$3=="_exception_text_end" {print $1; exit}' "$KMAP")
    if [ -n "$lo" ] && [ -n "$hi" ]; then
        lo=$((16#$lo)); hi=$((16#$hi)); base=$((16#$k)); top=$((base + 4096))
        if [ "$lo" -lt "$base" ] || [ "$hi" -gt "$top" ]; then
            die "kernel vectors span 0x$(printf '%x' "$lo")..0x$(printf '%x' "$hi") \\
does not fit the firmware buffer 0x$k..0x$(printf '%x' "$top")."
        fi
        [ "$VERBOSE" = 1 ] && printf 'vectors span 0x%x..0x%x (%d bytes) inside 0x%x..0x%x\n' \
            "$lo" "$hi" "$((hi - lo))" "$base" "$top"
    fi
else
    [ "$VERBOSE" = 1 ] && printf 'no %s; skipped the span check (config addresses agree)\n' "$KMAP"
fi

[ "$VERBOSE" = 1 ] && printf 'vectors ok: firmware 0x%s == kernel 0x%s\n' "$fw" "$k"
exit 0
