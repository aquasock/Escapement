#!/usr/bin/env bash
# Fix Enter on the watch's console, over the console.
#
# This board comes up with ICRNL off, so Enter (CR) never becomes a line ending
# and every echo-off prompt -- login, passwd, anything using getpass -- waits
# forever on a line that cannot end. Interactive shells hide it because readline
# accepts CR directly. Ctrl-J works around it by hand.
#
# Run this once with a shell on the console, either from a UART or by using
# Ctrl-J to submit the lines:
#
#     ./scripts/console-fix.sh                 # /dev/serial/by-id/... 
#     PORT=/dev/ttyUSB0 ./scripts/console-fix.sh
#
# It writes /etc/init.d/S01termios on the writable etc partition, so the fix
# survives a reboot. An image update from upstream will not carry it; re-run.
set -euo pipefail

PORT=${PORT:-}
if [ -z "$PORT" ]; then
    for d in /dev/serial/by-id/usb-Espressif_*; do
        [ -e "$d" ] && PORT=$(readlink -f "$d") && break
    done
fi
[ -n "$PORT" ] || { echo "no Espressif device; set PORT=" >&2; exit 1; }

python3 - "$PORT" <<'PY'
import sys, time, glob
try:
    import serial
except ImportError:
    sys.exit("needs pyserial (pip install pyserial, or use the IDF env's python)")

port = sys.argv[1]
s = serial.Serial(port, 115200, timeout=0.3, write_timeout=3)

def drain(t):
    out = b""
    end = time.time() + t
    while time.time() < end:
        d = s.read(4096)
        if d:
            out += d
    return out.decode("utf-8", "replace")

drain(0.5)
# Send whole lines terminated with CR: ICRNL is set by this very script, so
# CR works only once the first stty has run. Use LF for the first lines to be
# safe, which works whether or not ICRNL is on.
lines = [
    "stty icrnl",
    "cat > /etc/init.d/S01termios <<'EOS'",
    "#!/bin/sh",
    "# Enter arrives as CR. Without ICRNL the line discipline never sees a",
    "# NEWLINE, so login/passwd wait forever on a line that cannot end.",
    "stty icrnl < /dev/console 2>/dev/null",
    "EOS",
    "chmod +x /etc/init.d/S01termios",
    "cat /etc/init.d/S01termios",
]
for line in lines:
    s.write((line + "\n").encode())
    s.flush()
    time.sleep(0.15)

out = drain(3)
print(out[-700:])
ok = "stty icrnl < /dev/console" in out
print("\nwrote /etc/init.d/S01termios" if ok else "\nNOT CONFIRMED -- is a shell open?")
s.close()
sys.exit(0 if ok else 1)
PY
