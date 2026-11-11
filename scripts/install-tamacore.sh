#!/usr/bin/env bash
# Install tamacore as an ordinary program: type `tamacore` on the watch.
#
#   ./scripts/install-tamacore.sh           install and verify
#   ./scripts/install-tamacore.sh --check   report only, change nothing
#   WATCH_TRANSPORT=usb ./scripts/install-tamacore.sh
#
# This is deliberately NOT a boot service. There is no /etc/init.d entry and no
# rcS hook: the emulator is an application, so it runs when you type its name
# and stops with Ctrl-C. Starting it at boot would also mean it owned /dev/fb0
# and the emulated machine from power-on whether or not anyone wanted the pet
# running.
#
# Two things make `tamacore` a command rather than a path:
#
#   1. the binary        userspace/tamacore              -> /home/tamacore  0755
#   2. /home on PATH     userspace/profile.d-home-path.sh
#                        -> /etc/profile.d/home-path.sh  0644
#
# Why there and not /usr/bin: / is cramfs and read-only, so /usr/bin cannot take
# a copy or a symlink. /home is jffs2 and writable, and it is where deploys
# already put binaries. /etc is jffs2 too, and a profile.d snippet is exactly
# what it is for -- the image already extends PATH that way for curl's CA bundle.
#
# Why every remote command here is a single line: bash echoes a continued
# command once per physical line and reprints its prompt each time, so a
# multi-line remote command buries its own output. watch-time.sh hit this first.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

HOST=${WATCH_HOST:-10.10.0.15}
BIN_SRC=userspace/tamacore
PROFILE_SRC=userspace/profile.d-home-path.sh
PROFILE_DST=/etc/profile.d/home-path.sh
CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

die() { printf 'escapement: %s\n' "$*" >&2; exit 1; }

[ -f "$BIN_SRC" ]     || die "missing $BIN_SRC -- run ./scripts/build-tamacore.sh"
[ -f "$PROFILE_SRC" ] || die "missing $PROFILE_SRC"

# --- transport -----------------------------------------------------------------
transport=${WATCH_TRANSPORT:-auto}
if [ "$transport" = auto ]; then
	if timeout 5 bash -c "exec 3<>/dev/tcp/$HOST/23" 2>/dev/null; then
		transport=telnet
	else
		transport=usb
	fi
fi

case "$transport" in
telnet)
	remote() { ./scripts/watch-telnet.py "$1"; }
	deploy() { ./scripts/watch-deploy.sh --telnet "$1"; }
	;;
usb)
	remote() { ./scripts/watch-exec.py "$1"; }
	deploy() { ./scripts/watch-deploy.sh --usb "$1"; }
	;;
*)
	die "WATCH_TRANSPORT must be telnet, usb or auto"
	;;
esac

# --- install --------------------------------------------------------------------
if [ "$CHECK" = 1 ]; then
	printf 'checking only, transport %s\n' "$transport"
else
	printf 'installing tamacore, transport %s\n' "$transport"

	deploy tamacore || die "deploy failed"

	# The PATH entry: base64 over the shell channel, sha256 checked ON THE
	# WATCH before the move. A short write here is silent and expensive --
	# /etc/profile sources every profile.d snippet at login, so a truncated
	# one gives every future shell a broken PATH line.
	want=$(sha256sum "$PROFILE_SRC" | cut -d' ' -f1)
	b64=$(base64 -w0 "$PROFILE_SRC")
	out=$(remote "mkdir -p /etc/profile.d; printf '%s' '$b64' | base64 -d > $PROFILE_DST.new; chmod 0644 $PROFILE_DST.new; sha256sum $PROFILE_DST.new")
	have=$(printf '%s\n' "$out" | grep -oE '[0-9a-f]{64}' | tail -1)
	[ "$have" = "$want" ] \
		|| die "profile.d file did not arrive intact (want ${want:0:12}, have ${have:0:12})"
	remote "mv $PROFILE_DST.new $PROFILE_DST"
	printf 'installed /home/tamacore and %s\n' "$PROFILE_DST"
fi

# --- verify ---------------------------------------------------------------------
# A login shell, because that is what sources /etc/profile and therefore the
# profile.d entry. An interactive shell that is already open will not see the
# new PATH until it is replaced -- so "tamacore: not found" in an old terminal
# is the expected result, and not a failed install.
printf '\n'
remote "printf 'binary:     '; ls -l /home/tamacore 2>/dev/null | cut -c1-10; printf 'path file:  '; [ -f $PROFILE_DST ] && echo present || echo MISSING; printf 'login PATH: '; sh -lc 'echo \$PATH'; printf 'resolves:   '; sh -lc 'command -v tamacore || echo NOT-ON-PATH'; printf 'version:    '; sh -lc 'tamacore --version 2>&1'; printf 'help:       '; sh -lc 'tamacore -h 2>&1 | head -1'; printf 'autostart:  '; [ -e /etc/init.d/S51tamacore ] && echo 'PRESENT (unexpected)' || echo 'none - runs only when typed'"
