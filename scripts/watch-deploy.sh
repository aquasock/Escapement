#!/usr/bin/env bash
# Deploy a userspace binary to the watch's /home, verified by sha256.
#
#   ./scripts/watch-deploy.sh tbtns
#   ./scripts/watch-deploy.sh blekbd
#
# /home is the writable jffs2 partition; the rootfs is a read-only cramfs, so
# this is where binaries live. Deployment is netcat because the shell channels
# both choke on a 24 KB base64 argument: watch-telnet.py and watch-exec.py hand
# the command to an interactive shell, and a line that long is either truncated
# by the tty input queue or rejected outright.
#
# The details that matter, each of which failed once:
#
#   * nc must be in the FOREGROUND of the telnet session. Backgrounded, it dies
#     of SIGHUP the moment watch-telnet.py closes the socket, and the transfer
#     fails with "Connection refused".
#   * Do not probe the port to see whether it is listening: the probe IS a
#     connection, nc treats it as the whole transfer, and the result is a
#     zero-byte file that looks like a silent transfer failure.
#   * Moving the received file over a path that is currently executing gives
#     ETXTBSY. The old process is stopped first, and the receiver is given a
#     moment to exit and release its descriptor.
#
# Exit status is non-zero if the hash on the watch does not match the local file.
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

HOST=${WATCH_HOST:-10.10.0.15}
NAME=${1:-}
[ -n "$NAME" ] || { echo "usage: $0 <binary in userspace/>" >&2; exit 2; }

SRC=userspace/$NAME
[ -f "$SRC" ] || { echo "escapement: no $SRC -- run ./scripts/build-userspace.sh" >&2; exit 1; }

want=$(sha256sum "$SRC" | cut -d' ' -f1)
port=$(( 5560 + RANDOM % 200 ))
tmp=$(mktemp)

cleanup() { rm -f "$tmp"; }
trap cleanup EXIT

echo "deploying $NAME -> /home/$NAME (port $port, sha256 ${want:0:12}...)"

# Foreground nc inside a long-lived session: the shell stays up for as long as
# nc does, which is until the host side closes the connection.
timeout 60 ./scripts/watch-telnet.py \
    "rm -f /home/$NAME.new; nc -l -p $port > /home/$NAME.new; sha256sum /home/$NAME.new" \
    > "$tmp" 2>&1 &
sess=$!

sleep 5
if ! timeout 30 bash -c "cat '$SRC' > /dev/tcp/$HOST/$port"; then
    echo "escapement: could not send to $HOST:$port" >&2
    kill "$sess" 2>/dev/null || true
    exit 1
fi

wait "$sess" 2>/dev/null || true

# Read the hash back from the watch rather than trusting the transfer session's
# own output, which also carries banner and prompt noise.
timeout 30 ./scripts/watch-telnet.py "killall nc 2>/dev/null; sleep 1; mv /home/$NAME.new /home/$NAME && chmod +x /home/$NAME && sha256sum /home/$NAME" > "$tmp" 2>&1

have=$(grep -oE '^[0-9a-f]{64}' "$tmp" | tail -1)

if [ "$have" != "$want" ]; then
    echo "escapement: hash mismatch on the watch" >&2
    echo "  want $want" >&2
    echo "  got  ${have:-<none>}" >&2
    sed 's/^/  /' "$tmp" | tail -5 >&2
    exit 1
fi

echo "deployed /home/$NAME, sha256 verified"
