#!/usr/bin/env bash
# Close the loop on the RTC: write the system clock back at shutdown.
#
# CONFIG_RTC_HCTOSYS seeds the kernel from the PCF85063A at boot, but nothing
# closes the other direction. If NTP corrects the clock during a session, the
# chip keeps the value it had, so the error is there again on the next boot.
#
# Verified on hardware: the chip was deliberately set to 20:30 while true UTC
# was 19:00. The system clock was corrected, the watch rebooted with no manual
# `hwclock -w`, and the next boot reported
#
#     rtc-pcf85063 0-0051: setting system clock to 2026-11-08T19:01:00 UTC
#
# i.e. the chip had followed the system clock down.
#
# Two details that are easy to get wrong:
#
#   * BusyBox hwclock has no --set. It is -s / -w / --systz only, and
#     `date -s` takes a literal "YYYY-MM-DD hh:mm:ss", not @epoch.
#   * -u is not optional. Without it hwclock reads /var/lib/hwclock/adjtime to
#     decide whether the chip holds UTC or local time; that file does not exist
#     here, so it guesses.
#
# Usage: ./scripts/rtc-writeback.sh [--remove]
set -euo pipefail

PORT=${PORT:-}
[ -n "$PORT" ] || { for d in /dev/serial/by-id/usb-Espressif_*; do [ -e "$d" ] && PORT=$(readlink -f "$d") && break; done; }
[ -n "$PORT" ] || { echo "no Espressif device; set PORT=" >&2; exit 1; }

REMOVE=${1:-}

python3 - "$PORT" "$REMOVE" <<'PY'
import sys, time, serial, re

port, remove = sys.argv[1], (len(sys.argv) > 2 and sys.argv[2] == "--remove")
s = serial.Serial(port, 115200, timeout=0.3, write_timeout=3)

def drain(t=1.5):
    out = b""; end = time.time() + t
    while time.time() < end:
        d = s.read(4096)
        if d: out += d
    return re.sub(r"\x1b\[[0-9;?]*[a-zA-Z]", "", out.decode("utf-8", "replace"))

def run(line):
    s.write((line + "\n").encode()); s.flush(); time.sleep(0.2)

drain()

if remove:
    run("rm -f /etc/init.d/S01hwclock")
    run("ls /etc/init.d/S01hwclock 2>&1")
    print(drain())
    sys.exit(0)

# NOTE: no tabs anywhere in the heredoc body. This is pasted into an
# interactive bash, and a literal TAB triggers filename completion.
script = r'''cat > /etc/init.d/S01hwclock <<'EOS'
#!/bin/sh
# Escapement: write the system clock back to the PCF85063A at shutdown.
#
# hctosys seeds the kernel from the RTC at boot, but nothing closed the loop:
# after NTP corrected the clock the chip kept the old value, so the error came
# back on the next boot. rcK calls this with 'stop' after every other init
# script (it runs S??* in reverse), late enough for NTP and before umount.
#
# -u because the chip holds UTC. Without it hwclock consults
# /var/lib/hwclock/adjtime to decide, and that file does not exist here.
case "$1" in
stop)
    hwclock -w -u 2>/dev/null || :
    ;;
esac
EOS
chmod +x /etc/init.d/S01hwclock
'''
for line in script.splitlines():
    run(line)
run("chmod +x /etc/init.d/S01hwclock")
run("ls -l /etc/init.d/S01hwclock")
out = drain(2.5)
print(out[-600:])
ok = "S01hwclock" in out and " No such file" not in out
print("\ninstalled" if ok else "\nNOT CONFIRMED -- is a shell open and logged in?")
sys.exit(0 if ok else 1)
PY
