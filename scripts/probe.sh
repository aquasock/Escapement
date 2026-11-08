#!/usr/bin/env bash
# Watch Linux run, over JTAG, on a board with no serial console.
#
#   ./scripts/probe.sh            sample core 1's PC a dozen times
#   ./scripts/probe.sh --ram      also dump internal DRAM and grep for strings
#
# Why this exists: the kernel's console is ttyGS3, the S3's USB-Serial-JTAG
# peripheral. After handoff the host never re-enumerates it, so nothing arrives
# on /dev/ttyACM*. ttyS0 is unreachable because the watch has no UART bridge.
# JTAG is the only window, and it is enough to tell "running" from "wedged".
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

B=build
OCD=$(ls -d "$B"/idf-tools/tools/openocd-esp32/*/openocd-esp32/bin/openocd 2>/dev/null | head -1)
SCR=$(ls -d "$B"/idf-tools/tools/openocd-esp32/*/openocd-esp32/share/openocd/scripts 2>/dev/null | head -1)
[ -x "${OCD:-}" ] || { echo "escapement: openocd not found under build/idf-tools" >&2; exit 1; }

KERNEL_BASE=0x42000000   # XIP: kernel and rootfs are addressed here
KERNEL_OFF=0x140000
ROOTFS_OFF=0x540000
ROOTFS_END=0xcc0000

samples=12
settle=20
mode=${1:-}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/probe.tcl" <<EOF
init
targets esp32s3.cpu1
halt
resume
sleep ${settle}000
for {set i 0} {\$i < $samples} {incr i} {
  halt
  set pc [reg pc]
  echo "SAMPLE \$pc"
  resume
  sleep 400
}
EOF

if [ "$mode" = "--ram" ]; then
    cat >> "$tmp/probe.tcl" <<EOF
halt
for {set i 0} {\$i < 15} {incr i} {
  set a [expr {0x3FC88000 + \$i*0x8000}]
  set f [format "$tmp/ram%02d.bin" \$i]
  dump_image \$f \$a 0x8000
}
EOF
fi

cat >> "$tmp/probe.tcl" <<'EOF'
echo "PROBEDONE"
shutdown
EOF

echo "=== sampling core 1 (waits ${settle}s for boot)"
# `reg pc` returns "pc (/32): 0x422f89aa", not a bare number, so the address
# has to be pulled out of the line rather than used as a value.
"$OCD" -s "$SCR" -f board/esp32s3-builtin.cfg -f "$tmp/probe.tcl" 2>&1 \
    | grep 'SAMPLE' | grep -oE '0x[0-9a-f]+' > "$tmp/pcs"

if [ ! -s "$tmp/pcs" ]; then
    echo "no samples. Is the watch plugged in? (lsusb | grep 303a)" >&2
    exit 1
fi

echo
echo "=== where core 1 was, over $samples samples"
sort "$tmp/pcs" | uniq -c | sort -rn | while read -r n pc; do
    where="?"
    # Arithmetic context, not `test`: `[ x -ge 0x140000 ]` rejects hex, and
    # these bounds read better in hex.
    if (( pc >= KERNEL_BASE + KERNEL_OFF && pc < KERNEL_BASE + KERNEL_OFF + 0x400000 )); then
        where=$(printf 'kernel+XIP (flash 0x%x)' $(( pc - KERNEL_BASE )))
    elif (( pc >= KERNEL_BASE + ROOTFS_OFF && pc < KERNEL_BASE + ROOTFS_END )); then
        where="ROOTFS -- userspace is executing"
    fi
    printf '%3d  %s  %s\n' "$n" "$pc" "$where"
done

echo
echo "read it like this:"
echo "  - more than one distinct PC  -> the core is live, not wedged"
echo "  - any PC inside the rootfs   -> userspace ran"
echo "  - one PC only, over 12 polls -> either idle, or reset by openocd's init"
echo "    (a lone repeated PC proves nothing -- init resets both cores)"

if [ "$mode" = "--ram" ]; then
    echo
    echo "=== internal DRAM"
    cat "$tmp"/ram*.bin > "$tmp/ram.bin" 2>/dev/null || true
    if [ -s "$tmp/ram.bin" ]; then
        printf '%d bytes dumped\n' "$(stat -c %s "$tmp/ram.bin")"
        for s in "Linux version" "cramfs" "espsta0" "Kernel panic"; do
            printf '  %-14s %s\n' "$s" "$(grep -c -a "$s" "$tmp/ram.bin" || true)"
        done
        echo "note: PSRAM (0x3d800000) is not readable over this JTAG path, and"
        echo "      the kernel's log buffer lives there, not here."
    else
        echo "dump failed"
    fi
fi
