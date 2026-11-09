#!/usr/bin/env bash
# Install the watch's time state: /etc/TZ and /etc/init.d/S01hwclock.
#
#   ./scripts/watch-time.sh           install both, verify, report
#   ./scripts/watch-time.sh --check   report only, change nothing
#
# Why this exists: the RTC chip holds correct UTC, seeded into the kernel by
# CONFIG_RTC_HCTOSYS at boot, but the image ships no timezone data at all --
# no /etc/TZ, no /etc/localtime, no /usr/share/zoneinfo -- so `date` on the
# watch prints UTC and the console clock is seven hours off the wall. And the
# shutdown write-back that README.md describes did not exist on the board:
# /etc is rewritten on every image flash, and only the seven files listed in
# /etc/init.d/S03keepconfig (plus the dropbear keys) survive one. Anything else
# added to /etc has to be re-installed, which is what this script does.
#
# Two files, carried here as artifacts rather than pasted into a heredoc:
#
#   userspace/TZ           -> /etc/TZ                 (0644)
#   userspace/S01hwclock   -> /etc/init.d/S01hwclock  (0755)
#
# It also DISABLES the upstream image's /home/.clock mechanism, by decision:
# the PCF85063A is the single clock authority on this board. See the
# disable_stamp_clock function for the reasoning and for why "rename it to
# .disabled" does not work.
#
# /etc/TZ holds the POSIX TZ string and nothing else. Neither uClibc-ng nor
# glibc parses comments in that file, so a comment there would become part of
# the timezone name. America/Phoenix is MST7: UTC-7 all year, no DST. NOT
# MST7MDT, which would be an hour wrong for the whole of every summer.
#
# The transfer is base64 over the shell channel, and the sha256 is checked ON
# THE WATCH before the file is moved into place. A truncated write here is
# silent and expensive: a short /etc/TZ gives a wrong timezone, and a short
# init script fails at shutdown when nothing is watching the console to notice.
#
# Transport: telnet if the LAN is up, else the USB console. Both go through the
# existing scripts/watch-telnet.py and scripts/watch-exec.py rather than
# speaking either protocol here.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

HOST=${WATCH_HOST:-10.10.0.15}
TZ_SRC=userspace/TZ
INIT_SRC=userspace/S01hwclock
TZ_DST=/etc/TZ
INIT_DST=/etc/init.d/S01hwclock

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }
say() { printf '%s\n' "$*"; }

# bash on the watch turns on bracketed paste, and its prompt carries colour, so
# raw session output arrives peppered with ESC sequences. Strip them for display
# only -- never before a value is compared.
strip_ansi() { sed 's/\x1b\[[0-9;?]*[a-zA-Z]//g'; }

[ -f "$TZ_SRC" ]   || die "missing $TZ_SRC"
[ -f "$INIT_SRC" ] || die "missing $INIT_SRC"

# --- transport -----------------------------------------------------------------
# A closed port answers fast; a powered-down watch times out. Either way the
# USB console is the fallback, so this only decides which path to try first.
transport=${WATCH_TRANSPORT:-auto}
if [ "$transport" = auto ]; then
	if timeout 3 bash -c "exec 3<>/dev/tcp/$HOST/23" 2>/dev/null; then
		transport=telnet
	else
		transport=usb
	fi
fi

case "$transport" in
telnet) remote() { ./scripts/watch-telnet.py "$1"; } ;;
usb)    remote() { ./scripts/watch-exec.py "$1"; } ;;
*)      die "WATCH_TRANSPORT must be telnet, usb or auto" ;;
esac

# One line, single quotes: bash echoes a continued command once per physical
# line and reprints its prompt each time, which buries the values being read.
#
# Both hwclock flags are printed on purpose. BusyBox `-u` means "the chip holds
# UTC" and then prints the result in LOCAL time; `-l` means "the chip holds
# local time" and prints the stored value unconverted. Read alone, either one
# looks like a wrong clock -- `-u` reads seven hours behind because it converts.
# The stored value must match `date -u`; that is the check that means anything.
report_state() {
	remote 'echo "TZ file:        $(cat /etc/TZ 2>/dev/null || echo MISSING)"; echo "local time:     $(env -u TZ date "+%Y-%m-%dT%H:%M:%S %Z %z")"; echo "UTC:            $(date -u "+%Y-%m-%dT%H:%M:%S")"; echo "chip stored:    $(hwclock -r -l 2>&1 | cut -c1-24)   (must equal UTC)"; echo "chip as local:  $(hwclock -r -u 2>&1 | cut -c1-24)   (must equal local)"; /etc/init.d/S01hwclock status 2>/dev/null || echo "init script: MISSING"; if [ -e /etc/init.d/S01clock ]; then echo "S01clock:       ACTIVE (upstream stamp clock -- disabled by this project)"; elif [ -e /etc/disabled/S01clock ]; then echo "S01clock:       disabled"; else echo "S01clock:       absent"; fi; echo "/home/.clock:   $(cat /home/.clock 2>/dev/null || echo absent)"'
}

# --- check mode ----------------------------------------------------------------
if [ "${1:-}" = "--check" ]; then
	say "transport: $transport"
	report_state | strip_ansi | sed 's/^/  /'
	say ""
	say "Host UTC for comparison: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
	exit 0
fi

[ $# -eq 0 ] || die "unknown argument: $1 (only --check is accepted)"

# --- install -------------------------------------------------------------------
# Write to <dst>.new on the same filesystem, verify, then rename. A rename
# within one jffs2 mount is atomic, so a failure leaves the previous file
# intact rather than half-written.
install_file() {
	local src=$1 dst=$2 mode=$3
	local want have b64

	want=$(sha256sum "$src" | cut -d' ' -f1)
	b64=$(base64 -w0 < "$src")

	# The token makes the check unambiguous: the echoed command line contains
	# the literal text '$(sha256sum', never a hash, so only real output matches.
	have=$(remote "printf '%s' '$b64' | base64 -d > $dst.new && chmod $mode $dst.new && echo ESCHASH \$(sha256sum $dst.new)" \
	       | grep -oE 'ESCHASH [0-9a-f]{64}' | awk '{print $2}' | tail -1 || true)

	if [ "$have" != "$want" ]; then
		die "$dst: transfer did not verify (want ${want:0:12}, got ${have:0:12}); nothing installed"
	fi

	remote "mv $dst.new $dst" >/dev/null
	say "installed $dst  ($mode, sha256 ${want:0:12}...)"
}

say "transport: $transport"
install_file "$TZ_SRC"   "$TZ_DST"   644
install_file "$INIT_SRC" "$INIT_DST" 755

# --- disable the upstream stamp clock ------------------------------------------
# /etc/init.d/S01clock ships in the upstream image. It writes the epoch to
# /home/.clock at shutdown and, at boot, overwrites the system clock with that
# note whenever the note is newer than the current clock. It runs after hctosys
# has already seeded from the RTC, so the note beats the chip: setting it +90
# minutes moved the clock from 18:12 to 19:42 against a true 18:12:59.
#
# Disabled by project decision. The PCF85063A is the only clock on this board --
# /sys/class/rtc/ contains rtc0 and nothing else -- so it is the single
# authority. If the chip dies, the time is wrong and that is worth seeing; a
# stale note from the last shutdown is not a clock, it is last shutdown's guess.
# The script also costs one jffs2 write per shutdown, which is wear a watch does
# not need.
#
# It is MOVED OUT of /etc/init.d rather than renamed with a .disabled suffix.
# rcS and rcK glob `S??*` -- S, exactly two characters, then anything -- so
# S01clock.disabled and S01clock.off still match and still run. Verified in a
# scratch directory: all three names are matched. Only leaving the directory
# actually stops it.
#
# /home/.clock is deleted as well: with no reader left it is just a wrong number
# on disk, and it would be mistaken for state by whoever looks next.
disable_stamp_clock() {
	local result
	result=$(remote 'if [ -e /etc/init.d/S01clock ]; then mkdir -p /etc/disabled && mv /etc/init.d/S01clock /etc/disabled/S01clock && echo MOVED; elif [ -e /etc/disabled/S01clock ]; then echo ALREADY; else echo ABSENT; fi; rm -f /home/.clock' \
	         | grep -oE 'MOVED|ALREADY|ABSENT' | tail -1 || true)

	case $result in
	MOVED)   say "disabled upstream stamp clock (S01clock -> /etc/disabled/S01clock)" ;;
	ALREADY) say "upstream stamp clock already disabled" ;;
	ABSENT)  say "upstream stamp clock not present on this image" ;;
	*)       die "could not determine the state of /etc/init.d/S01clock" ;;
	esac

	# The move is only half the claim. Confirm nothing is left in the glob
	# that rcS and rcK iterate, or 'disabled' would be a report about a file
	# that still runs.
	remote 'ls /etc/init.d/S01clock 2>/dev/null | grep -q . && echo FOUND || echo GONE' \
		| grep -q GONE || die "S01clock is still in /etc/init.d; it would still run"
}

# --- confirm -------------------------------------------------------------------
# Installing is not working. The value in /etc/TZ has to change what `date`
# prints, and it has to do so with TZ unset, which is how the console's shells
# run. The chip is read in UTC and compared against the host below by eye.
say ""
disable_stamp_clock
say ""
report_state | strip_ansi | sed 's/^/  /'
say ""
say "Host UTC for comparison: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
