#!/usr/bin/env python3
"""usb-send.py - stream a file to the watch over the USB console, in one session.

    ./scripts/usb-send.py <local-file> <remote-path>

Why this exists
---------------
`watch-deploy.sh --usb` used to send the file as base64 in 900-character
chunks, one `watch-exec.py` per chunk. Each of those invocations opens the
port, interrupts whatever is running, logs in and waits for a prompt -- about
three seconds of handshake for 675 bytes of payload. A 249 KB binary is 370
chunks, which is roughly eighteen minutes of handshaking for a transfer that
should take seconds. Every such deploy was written off as "the console hangs"
when it had simply never been given long enough.

The console itself is fine. Measured over eight consecutive runs, watch-exec.py
succeeded every time; the defect was the per-chunk session, not the transport.

What this does instead: log in once, then hand the shell a heredoc. Bash reads
the heredoc body from the same input stream it reads commands from, so the whole
file streams through a single session with no round trips at all.

Details that matter
-------------------
* `stty -echo` first. The console echoes every byte typed at it, so without
  this the terminal spends the transfer echoing 332 KB back at us over a
  115200-baud link -- slower than the transfer itself, and it fills our own
  receive buffer.
* The heredoc terminator must not appear in base64 output. Base64 is
  [A-Za-z0-9+/=], so a terminator containing an underscore or a dot is
  structurally impossible to collide with.
* Writes are driven by `select` on writability, because the tty input queue is
  a few KB and the remote side consumes it at its own pace. A blind write loop
  truncates silently -- which is exactly how the old chunked path lost its
  final chunk.
* The hash is compared on the host, not trusted from the watch's output.
"""

import os
import select
import sys
import time
import tty

# Must not be a legal base64 substring; underscores cannot occur in base64.
TERMINATOR = "ESC_EOF_7f3a91c4"
# Built from a shell variable at send time so the echoed command line reads
# "${m}_DONE" while the actual output reads "ESC_DONE". Without that split the
# marker appears in the echo before the command has run, read_until matches
# instantly, and the hash is never seen -- the same trap as a heredoc end tag.
END_MARKER = "ESC_DONE"

DEFAULT_PASS_FILE = os.path.expanduser("~/.config/escapement/watch-pass")


def find_port():
    """Follow the by-id symlink so a re-enumerated ttyACM number cannot bite."""
    override = os.environ.get("WATCH_PORT")
    if override:
        return override

    by_id = "/dev/serial/by-id"
    try:
        for name in sorted(os.listdir(by_id)):
            if name.startswith("usb-Espressif"):
                return os.path.realpath(os.path.join(by_id, name))
    except OSError:
        pass

    for n in range(8):
        cand = "/dev/ttyACM%d" % n
        if os.path.exists(cand):
            return cand
    raise SystemExit("usb-send: no Espressif serial device found")


def password():
    pw = os.environ.get("WATCH_PASS")
    if pw:
        return pw
    path = os.environ.get("WATCH_PASS_FILE", DEFAULT_PASS_FILE)
    try:
        with open(path) as f:
            return f.read().strip()
    except OSError:
        raise SystemExit("usb-send: no password; set WATCH_PASS_FILE")


def drain(fd, seconds):
    buf = b""
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if not r:
            continue
        try:
            d = os.read(fd, 65536)
        except OSError:
            break
        if d:
            buf += d
    return buf


def write_all(fd, data, deadline=120.0):
    """Write all of `data`, honouring backpressure. Returns bytes written."""
    view = memoryview(data)
    end = time.time() + deadline
    while view and time.time() < end:
        try:
            n = os.write(fd, view)
        except BlockingIOError:
            n = 0
        if n:
            view = view[n:]
        else:
            select.select([], [fd], [], 0.05)
    return len(data) - len(view)


def read_until(fd, needle, deadline):
    buf = b""
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], 0.1)
        if not r:
            continue
        try:
            d = os.read(fd, 65536)
        except OSError:
            break
        if d:
            buf += d
            if needle in buf:
                return buf
    return buf


def login(fd, pw, timeout=20.0):
    """Get a root shell. Returns True on success."""
    # An interrupted host-side session can leave a foreground process holding
    # the console; Ctrl-C clears it.
    write_all(fd, b"\x03", 5.0)
    time.sleep(0.3)
    drain(fd, 0.4)
    write_all(fd, b"\r", 5.0)

    buf = read_until(fd, b"# ", time.time() + timeout)
    if b"# " in buf:
        return True
    if b"login:" not in buf:
        return False

    write_all(fd, b"root\r", 5.0)
    time.sleep(0.8)
    buf = drain(fd, 1.5)
    if b"assword" not in buf:
        return False

    write_all(fd, pw.encode() + b"\r", 5.0)
    buf = read_until(fd, b"# ", time.time() + 12)
    return b"# " in buf


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "rb") as f:
        raw = f.read()

    # Wrap at 900 so a line fits comfortably inside the tty input queue even
    # when the reader is momentarily behind.
    import base64
    b64 = base64.b64encode(raw)
    lines = [b64[i:i + 900] for i in range(0, len(b64), 900)]

    import hashlib
    want = hashlib.sha256(raw).hexdigest()

    print("usb-send: %s -> %s (%d bytes, %d b64 lines, sha256 %s...)"
          % (src, dst, len(raw), len(lines), want[:12]))

    port = find_port()
    pw = password()
    print("usb-send: port %s" % port)

    fd = os.open(port, os.O_RDWR | os.O_NONBLOCK)
    try:
        tty.setraw(fd)
        time.sleep(0.2)
        drain(fd, 0.3)
        if not login(fd, pw):
            raise SystemExit("usb-send: could not get a root shell on %s" % port)

        # Echo off before any bulk data: otherwise the console spends the
        # transfer reading back its own echo over a 115200-baud link.
        write_all(fd, b"stty -echo; rm -f %s.b64\r" % dst.encode(), 10.0)
        read_until(fd, b"# ", time.time() + 10)

        # Stream the payload straight into `base64 -d`, which reads its stdin
        # from the same tty this session is writing to.
        #
        # NOT a heredoc. A heredoc makes *bash* buffer the entire body before
        # handing it over, and 332 KB of base64 is exactly the kind of
        # single allocation that fails on a NOMMU target with a fragmented
        # heap -- the measured symptom is the shell dying outright and the
        # console going completely silent, which reads as "USB is broken".
        # `base64 -d` consumes a chunk at a time and never holds the payload.
        # `head -c N` bounds the read by byte count, so base64 gets EOF from
        # the closed pipe and never needs a ^D from the tty.
        #
        # ^D was the bug. It is a line-discipline signal, not data: if it
        # arrives while bash is still at its prompt rather than inside
        # `base64 -d`, bash treats it as end-of-input and the login shell exits.
        # For a 249 KB binary there is always a backlog queued, so the ^D lands
        # inside base64 and the transfer succeeds -- measured 6/6. For a 28-byte
        # script there is no backlog, the ^D races ahead of the command, and the
        # shell dies roughly every time. Fixing the signal is better than
        # damping the race with a sleep.
        # Count the line terminators too. Each line goes out as base64 text
        # plus CRLF, and `head -c` counts every byte on the wire, so a limit of
        # len(b64) stops one byte short per line -- 370 bytes short here, which
        # truncates the tail of the payload and yields a wrong hash rather than
        # an error. base64 -d ignores the CR and LF, so they only need to be
        # counted, not removed.
        wire_len = len(b64) + 2 * len(lines)
        write_all(fd, b"head -c %d | base64 -d > %s.new\r"
                  % (wire_len, dst.encode()), 15.0)

        sent = 0
        for line in lines:
            n = write_all(fd, line + b"\r\n", 30.0)
            if n != len(line) + 2:
                raise SystemExit("usb-send: short write at %d of %d bytes"
                                 % (sent, len(b64)))
            sent += n

        # No ^D: `head -c` stops on its own byte count. Echo is off, so the
        # only thing that comes back is the prompt once the pipeline finishes.
        read_until(fd, b"# ", time.time() + 45)

        write_all(fd, b"stty echo\r", 10.0)
        read_until(fd, b"# ", time.time() + 10)

        # Drain first. The prompt that ends the `base64 -d` wait may still be
        # sitting unread, and read_until("# ") would then match it instantly --
        # returning before sha256sum had printed anything, so the hash came back
        # None even though the transfer was perfect. That is why this failed
        # about one run in three.
        drain(fd, 0.5)

        # A unique end marker, not the prompt: the prompt is not unique.
        # The marker is assembled from $m so the echoed command line does not
        # contain it -- see the END_MARKER comment above.
        cmds = (b"m=%s; chmod +x %s.new && "
                b"mv %s.new %s && "
                b"sha256sum %s; echo ${m}_DONE\r"
                % (END_MARKER.encode(), dst.encode(), dst.encode(),
                   dst.encode(), dst.encode()))
        write_all(fd, cmds, 10.0)

        buf = read_until(fd, (END_MARKER + "_DONE").encode(),
                         time.time() + 60)
        text = buf.decode("utf-8", "replace")

        have = None
        for tok in text.split():
            if len(tok) == 64 and all(c in "0123456789abcdef" for c in tok):
                have = tok
        if have != want:
            raise SystemExit("usb-send: hash mismatch\n  want %s\n  got  %s"
                             % (want, have))
        print("usb-send: verified, sha256 matches")
    finally:
        # Always give the terminal its echo back. Leaving it off makes the next
        # session look dead, and the failure then gets blamed on the transport.
        try:
            os.write(fd, b"\x03")
            time.sleep(0.2)
            os.write(fd, b"\rstty echo\r")
        except OSError:
            pass
        os.close(fd)


if __name__ == "__main__":
    main()
