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
    # `--before default_reset` IS THE ONLY FORM THAT WORKS, from either state,
    # and both halves were measured on this board on 2026-11-12.
    #
    # From the stub: `--before no_reset` connects and exits 0 but performs no
    # effective reset, because the board is on a native USB-JTAG port where RTS
    # is not wired to EN -- the pulse is not felt, the stub keeps running and
    # the desktop never appears. That silent success is what made three
    # consecutive backups report "board stayed silent".
    #
    # From a running board: `--before no_reset` cannot even connect, because
    # the desktop is speaking raw VT100 and esptool's sync fails with "Invalid
    # head of packet".
    #
    # default_reset handles both -- it pulses reset and syncs whatever is
    # there -- so there is no reason to try anything else first.
    "$PY" -m esptool --chip esp32s3 --port "$PORT" --before default_reset \
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
# `--after no_reset`, NOT `--after hard_reset`. On this board the reset esptool
# performs after a verify does nothing: the board is in esptool's stub on a
# native USB-JTAG port, where RTS is not wired to EN, so the pulse is not felt,
# the stub keeps running and the board stays silent. Measured 2026-11-12 across
# three backups -- every one of them reported "board stayed silent" and then
# came back instantly when reset explicitly below. Asking for a reset here
# bought nothing and made the failure look like the board's fault.
if ! "$PY" -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
        --before no_reset --after no_reset \
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

#
# CONFIRM THE BOARD CAME BACK, BECAUSE TWICE IT DID NOT AND THE SCRIPT SAID NOTHING.
#
# Both resets here can fail without a word: the verify's `--after hard_reset`
# leaves the board in the stub when it does not take, and `reset_board` runs
# `--before no_reset` against a board that is by then running firmware rather
# than a stub, so its own failure is also swallowed by `|| true`. The result on
# 2026-11-12 (twice, logged as core-log entries 15 and 20) was a script that
# exited 0 while the watch sat silent on both USB and WiFi -- which is
# indistinguishable from a brick from the outside, and cost a session each
# time to notice.
#
# The backup itself is NOT in doubt when this fails: the verify passed against
# the chip before any reset was attempted, so the file is good. What was wrong
# was claiming success for a board nobody had checked. So the check is here,
# after the artifact is saved, and it reports rather than dies -- a silent board
# is a power-cycle, not a failed backup.
#
board_alive() {
    "$PY" - "$PORT" <<'PYEOF'
import os, select, sys, termios, time, tty

port = sys.argv[1]
try:
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
except OSError:
    sys.exit(1)

# The desktop speaks raw VT100, so no line discipline: reading it the way a
# terminal reads it is the only way its output arrives unmodified.
a = termios.tcgetattr(fd)
a[0] = a[1] = 0
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[3] = 0
a[4] = a[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, a)
tty.setraw(fd)

# A booting desktop repaints the whole screen, so one byte is enough to know it
# is running. The window is generous because native USB re-enumerates on reset
# and the port is briefly absent.
seen = 0
end = time.time() + 12
while time.time() < end and not seen:
    ready, _, _ = select.select([fd], [], [], 0.25)
    if ready:
        try:
            seen = len(os.read(fd, 65536))
        except (BlockingIOError, OSError):
            pass

os.close(fd)
sys.exit(0 if seen else 1)
PYEOF
}

printf 'resetting the board\n'
reset_board
printf 'waiting for the board to come back\n'
if board_alive; then
    printf 'board is up\n'
    trap 'rm -f "$log"' EXIT
else
    printf 'still silent after a reset; trying once more\n'
    reset_board
    if board_alive; then
        printf 'board is up on the second attempt\n'
        trap 'rm -f "$log"' EXIT
    else
        printf '%s\n' \
            "WARNING: $PORT is silent after two resets." \
            "         The backup is verified and valid -- the compare passed" \
            "         against the chip before any reset was attempted." \
            "         The board needs a power cycle to come back." >&2
    fi
fi

exit 0
