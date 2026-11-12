#!/usr/bin/env bash
# Take a full 16 MiB flash backup before changing anything on the watch.
#
#   ./scripts/watch-backup.sh            ~/twatch-backup/working-DATE.bin
#   ./scripts/watch-backup.sh -o FILE
#
# Why this exists, at the cost of a session: the kernel was modified, the
# resulting image would not boot, and the "known-good kernel" that was flashed
# back would not boot either -- because the *other* damage was in the /etc
# jffs2 partition, not the kernel. The only way out was the factory release
# image, which predates the display driver and has no WiFi credentials, so it
# cost the display, the Bluetooth keyboard, the AMOLED login and the network
# configuration.
#
# A 16 MiB read costs about 3.5 minutes and makes recovery a single write of a
# file this machine already holds. Save the hash with it.
#
# The serial port must not be in use: close any console session first.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

OUT=$HOME/twatch-backup/working-$(date +%F-%H%M).bin
if [ "${1:-}" = "-o" ] && [ -n "${2:-}" ]; then
    OUT=$2
fi
mkdir -p "$(dirname "$OUT")"

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

#
# Refuse to start if something is already reading the flash.
#
# `pgrep -f` matches against the whole command line, and this script's own
# caller eventually has the pattern in ITS command line -- so invoking the
# script from a shell whose command string mentions esptool makes the guard
# match that shell and refuse, with the port perfectly free. Observed exactly
# that on 2026-11-10: the only match was the `bash -c` that ran the check.
#
# Two changes make it honest. `[e]sptool` cannot match its own literal,
# because the pattern text in the command line is "[e]sptool" and not
# "esptool". And the exclusions drop test harnesses and this script, so a
# wrapper cannot trip it either.
#
if pgrep -f '[e]sptool.*read_flash' >/dev/null 2>&1; then
	die "a flash read is already running"
fi

PORT=${PORT:-}
if [ -z "$PORT" ]; then
    for d in /dev/serial/by-id/usb-Espressif_*; do
        [ -e "$d" ] || continue
        PORT=$(readlink -f "$d"); break
    done
fi
[ -n "$PORT" ] || die "no Espressif USB device found"
timeout 3 bash -c "exec 3<>$PORT" 2>/dev/null \
    || die "$PORT is busy -- close any serial console session first"

PY=build/idf-tools/python_env/idf5.1_py3.14_env/bin/python
[ -x "$PY" ] || die "no python env at $PY; run ./scripts/build.sh once"

#
# READ, THEN VERIFY, WITH THE BOARD HELD IN THE BOOTLOADER THROUGHOUT.
#
# `verify_flash` compares a file against the LIVE chip, so it can only pass if
# nothing writes to the flash between the read and the compare. On this board
# something does: the watch boots, the firmware runs, and NVS -- shell history,
# TinyDesk settings -- is written, partly at power-down. Measured on
# 2026-11-12: a backup taken at 23:58 no longer matched the chip four minutes
# later, and 3 of the NVS partition's 6 blocks had changed. The first three
# backups on this board verified only because they were checked before the write
# landed, which is a race the script won by being quick, not a check.
#
# So the read leaves the board in esptool's stub instead of resetting it, and the
# verify connects to that stub without sending a reset pulse. The board never
# runs firmware between the two, and the compare is against the same flash the
# read came from. The reset happens at the very end, once the verify has passed.
#
# This is two invocations and not one because esptool 4.13 does not chain
# commands: `read_flash ... verify_flash ...` fails with "unrecognized
# arguments". Both flags below were chosen to make the split safe rather than
# because chaining would have been convenient -- `--before no_reset` is what
# stops the second invocation from rebooting the board.
#
reset_board() {
    "$PY" -m esptool --chip esp32s3 --port "$PORT" --before no_reset \
        --after hard_reset run >/dev/null 2>&1 || true
}

#
# If anything between the read and the reset fails, the board is left in the
# bootloader -- which looks exactly like a dead watch until it is power cycled.
# The trap puts it back however this exits.
#
trap 'reset_board' EXIT

printf 'reading 16 MiB from %s -> %s\n' "$PORT" "$OUT"
printf 'the board stays in the bootloader until the verify has passed\n'

log=$(mktemp)
trap 'reset_board; rm -f "$log"' EXIT

#
# esptool's status is captured to a file and checked directly. A pipeline would
# report the status of its LAST stage -- `tail` -- which is 0 whether or not the
# read worked, so `if ! cmd | tail` is a check that cannot fail.
#
if ! "$PY" -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
        --before default_reset --after no_reset \
        read_flash 0 0x1000000 "$OUT" > "$log" 2>&1
then
    tr '\r' '\n' < "$log" | tail -5 >&2
    die "the flash read failed; $OUT is not a backup"
fi
tr '\r' '\n' < "$log" | grep -E 'Read [0-9]+ bytes' | tail -1

#
# Size before verify: a short read is worth catching before spending another
# four minutes comparing it.
#
size=$(stat -c %s "$OUT" 2>/dev/null || echo 0)
[ "$size" = "16777216" ] || die "backup is $size bytes, expected 16777216"

printf 'verifying against the chip, which has not been allowed to boot\n'
if ! "$PY" -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
        --before no_reset --after hard_reset \
        verify_flash 0 "$OUT" > "$log" 2>&1
then
    tr '\r' '\n' < "$log" | tail -5 >&2
    die "verify FAILED: $OUT does not match the chip"
fi
tr '\r' '\n' < "$log" | grep -E 'Verifying|verify (OK|FAILED)' | tail -2
grep -q 'verify OK' <(tr '\r' '\n' < "$log") \
    || die "esptool exited 0 but never said 'verify OK'; treat the image as unverified"

(cd "$(dirname "$OUT")" && sha256sum "$(basename "$OUT")" > "$(basename "$OUT").sha256")
printf 'OK: %s\n' "$OUT"
cat "$(dirname "$OUT")/$(basename "$OUT").sha256"
exit 0
