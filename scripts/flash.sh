#!/usr/bin/env bash
# Flash a T-Watch Ultra for Escapement.
#
#   ./scripts/flash.sh                 write the 16 MiB release image (once)
#   ./scripts/flash.sh --app           write only core-0 firmware at 0x10000
#   ./scripts/flash.sh --factory       restore the watch's factory image
#
# The full image replaces the whole partition layout, so a partial reflash back
# to something else is not possible -- --factory writes all 16 MiB.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

PORT=${PORT:-}
# Where this watch's own factory image lives. Yours will be somewhere else:
# read it off the watch before flashing anything (esptool read_flash 0 0x1000000),
# or point FACTORY at wherever you keep it.
FACTORY=${FACTORY:-$HOME/twatch-backup/twatch-factory-16MB.bin}
FACTORY_SHA=${FACTORY_SHA:-471c9a21a6d53e5cdf6dfb01949fcc2dd4f420fe1a3738631f73b600419ac2b0}
APP=build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build/network_adapter.bin

say() { printf '\n=== %s\n' "$*"; }
die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

# The watch is an Espressif USB-Serial-JTAG device (303a:1001). Do not guess a
# port number: /dev/ttyACM0 may well be some other board.
if [ -z "$PORT" ]; then
    for d in /dev/serial/by-id/usb-Espressif_*; do
        [ -e "$d" ] || continue
        PORT=$(readlink -f "$d"); break
    done
fi
[ -n "$PORT" ] || die "no Espressif USB device found. lsusb | grep 303a"
[ -e "$PORT" ] || die "$PORT does not exist"

# A serial reader holding the port makes esptool fail in confusing ways.
if pgrep -af "cat $PORT|readplain.py|readser.py" >/dev/null 2>&1; then
    die "something is holding $PORT. Kill it first."
fi

ESPTOOL=$(command -v esptool.py || command -v esptool || true)
ESPTOOL_PY=$(command -v python3 || true)
# Fall back to the toolchain build.sh installed, so flashing works straight
# after a build without sourcing export.sh in the caller's shell. Run it as a
# module rather than by path: the venv's console scripts keep an absolute
# shebang, which breaks the moment the build tree is moved.
if [ -z "$ESPTOOL" ]; then
    for py in build/idf-tools/python_env/*/bin/python; do
        if [ -x "$py" ] && "$py" -c 'import esptool' 2>/dev/null; then
            ESPTOOL_PY=$py; ESPTOOL=-m; break
        fi
    done
fi
[ -n "$ESPTOOL" ] || die "no esptool. Run ./scripts/build.sh, or source an ESP-IDF export.sh"

if [ "$ESPTOOL" = "-m" ]; then
    esptool_cmd() { "$ESPTOOL_PY" -m esptool "$@"; }
else
    esptool_cmd() { "$ESPTOOL" "$@"; }
fi

flash() {
    esptool_cmd --chip esp32s3 --port "$PORT" --baud 921600 \
        write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB "$@"
}

case "${1:---image}" in
--app)
    [ -f "$APP" ] || die "$APP missing. Run ./scripts/build.sh"
    say "app only (0x10000) -> $PORT"
    flash 0x10000 "$APP"
    ;;
--image)
    IMG=${2:-images/linux-esp32s3-native-full.bin}
    [ -f "$IMG" ] || die "$IMG missing"
    [ "$(stat -c %s "$IMG")" = 16777216 ] || die "$IMG is not 16 MiB"
    say "full image -> $PORT  (this replaces everything)"
    flash 0x0 "$IMG"
    ;;
--factory)
    [ -f "$FACTORY" ] || die "no factory image at $FACTORY (set FACTORY=/path/to/factory.bin; FACTORY_SHA too if it is a different watch)"
    say "verifying factory image"
    echo "$FACTORY_SHA  $FACTORY" | sha256sum -c - || die "factory image hash mismatch"
    say "FACTORY image -> $PORT  (restores the watch)"
    flash 0x0 "$FACTORY"
    ;;
-h|--help)
    sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'
    ;;
*)
    die "unknown option: $1"
    ;;
esac

say "done"
printf 'esptool often reports a bogus exit status; trust "Hash of data verified".\n'
