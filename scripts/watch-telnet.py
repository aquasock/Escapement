#!/usr/bin/env python3
"""Run a shell command on the watch over telnet.

    ./scripts/watch-telnet.py 'uname -a'
    echo 'ls -l /home' | ./scripts/watch-telnet.py -

Why telnet and not the USB console: the serial port is 115200 baud and shared,
and every command there competes with kernel printk. Telnet is faster, does not
disturb anyone reading the console, and the board runs it on the LAN already.

The watch runs a BusyBox telnetd with no option negotiation worth speaking, but
the IAC bytes still arrive and have to be stripped or they land in the command
stream. Everything before the shell prompt is discarded once per connection.
"""

import os
import select
import socket
import sys
import time

HOST = os.environ.get("WATCH_HOST", "10.10.0.15")
PORT = int(os.environ.get("WATCH_TELNET_PORT", "23"))

IAC, DONT, DO, WONT, WILL, SB, SE = 255, 254, 253, 252, 251, 250, 240


def _read_password():
    pw = os.environ.get("WATCH_PASS")
    if pw:
        return pw
    path = os.environ.get(
        "WATCH_PASS_FILE",
        os.path.expanduser("~/.config/escapement/watch-pass"))
    try:
        with open(path) as f:
            return f.read().strip()
    except OSError:
        return ""


def strip_iac(buf):
    """Remove telnet option negotiation from a byte stream."""
    out = bytearray()
    i = 0
    n = len(buf)
    while i < n:
        b = buf[i]
        if b != IAC:
            out.append(b)
            i += 1
            continue
        if i + 1 >= n:
            break
        cmd = buf[i + 1]
        if cmd == IAC:			# escaped 0xFF
            out.append(IAC)
            i += 2
        elif cmd in (DO, DONT, WILL, WONT):
            i += 3			# command + option
        elif cmd == SB:
            j = buf.find(bytes([IAC, SE]), i)
            i = n if j < 0 else j + 2
        else:
            i += 2
    return bytes(out)


def recv_until(sock, needles, deadline):
    buf = b""
    while time.time() < deadline:
        r, _, _ = select.select([sock], [], [], 0.2)
        if not r:
            continue
        try:
            d = sock.recv(65536)
        except OSError:
            break
        if not d:
            break
        buf += d
        clean = strip_iac(buf)
        for n in needles:
            if n in clean:
                return clean, clean.split(n, 1)[0]
    return strip_iac(buf), strip_iac(buf)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd = sys.stdin.read() if sys.argv[1] == "-" else sys.argv[1]

    sock = socket.create_connection((HOST, PORT), timeout=10)
    sock.setblocking(False)
    try:
        # Log in. The board may already be at a prompt if a session is live,
        # so look for both shapes before typing anything.
        buf, _ = recv_until(sock, [b"login:", b"# "], time.time() + 6)

        if b"login:" in buf:
            sock.sendall(b"root\r")
            recv_until(sock, [b"assword:"], time.time() + 5)
            sock.sendall(_read_password().encode() + b"\r")
            recv_until(sock, [b"# "], time.time() + 8)
        elif b"# " not in buf:
            sock.sendall(b"\r")
            recv_until(sock, [b"# "], time.time() + 5)

        # The marker is built from a shell variable so the echoed command line
        # does not contain the literal text and cannot be mistaken for output.
        script = (b"m=__WT; printf '\\n%s_BEGIN%s\\n' \"$m\" \"$m\"; " +
                  cmd.encode() +
                  b"; printf '\\n%s_END%s\\n' \"$m\" \"$m\"\r")
        sock.sendall(script)

        out, _ = recv_until(sock, [b"__WT_END__"], time.time() + 180)

        # Answer any pager that wants a keypress, so output never stalls.
        while b"__WT_END__" not in out and time.time() < 2:
            break
    finally:
        sock.close()

    if b"__WT_BEGIN__" in out:
        out = out.split(b"__WT_BEGIN__", 1)[1]
    if b"__WT_END__" in out:
        out = out.split(b"__WT_END__", 1)[0]

    text = out.decode("utf-8", "replace").replace("\r\n", "\n").replace("\r", "")
    lines = text.split("\n")
    while lines and lines[0].strip() == "":
        lines.pop(0)
    print("\n".join(lines).rstrip("\n"))


if __name__ == "__main__":
    main()
