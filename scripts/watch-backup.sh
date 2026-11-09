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

[ -z "$(pgrep -f 'esptool.*read_flash' || true)" ] || die "a flash read is already running"

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

printf 'reading 16 MiB from %s -> %s\n' "$PORT" "$OUT"
"$PY" -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
    read_flash 0 0x1000000 "$OUT" 2>&1 | tr '\r' '\n' | tail -2

size=$(stat -c %s "$OUT")
[ "$size" = "16777216" ] || die "backup is $size bytes, expected 16777216"

(cd "$(dirname "$OUT")" && sha256sum "$(basename "$OUT")" > "$(basename "$OUT").sha256")
printf 'OK: %s\n' "$OUT"
cat "$(dirname "$OUT")/$(basename "$OUT").sha256"
