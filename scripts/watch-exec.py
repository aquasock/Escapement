#!/usr/bin/env python3
"""Run a shell command on the watch over its USB console and print the output.

    ./scripts/watch-exec.py 'uname -a'
    echo 'ls -l /dev' | ./scripts/watch-exec.py -

The watch's console is a bash shell on ttyGS3, presented over the S3's
USB-Serial-JTAG as /dev/ttyACM0. There is no framing protocol, so this sends a
marker before and after the command and prints only what lands between them.

Assumes the shell is already logged in (root, `# ` prompt). If it is at
`login:`, that has to be handled out of band -- this tool does not guess
credentials.

The console is 40 columns wide, so command echo wraps; markers are built from a
shell variable so the echoed copy of the command does not contain the literal
marker text and cannot be mistaken for the real output.
"""

import os
import select
import sys
import time
import tty

def _find_port():
    """Locate the watch's USB console.

    Do NOT hardcode /dev/ttyACM0. Replugging the board, or anything that resets
    its USB peripheral, re-enumerates it as ttyACM1, ttyACM2 and so on -- and a
    script pointed at the old number talks to a device that no longer exists.
    The failure is not a clean error: opens succeed against the stale node often
    enough to look like a wedged link, commands come back empty, and the board
    appears frozen when it is fine. That cost real hours here.

    The by-id symlink is stable across re-enumeration because it is derived
    from the chip's MAC address, so follow that instead.
    """
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

    # Fall back to the first ttyACM that exists.
    for n in range(8):
        cand = "/dev/ttyACM%d" % n
        if os.path.exists(cand):
            return cand
    return "/dev/ttyACM0"


PORT = _find_port()

# The watch's root password is not kept in this repository. Provide it with
# WATCH_PASS, or in a file (default ~/.config/escapement/watch-pass, mode 600):
#
#     mkdir -p ~/.config/escapement
#     printf '%s' 'the-password' > ~/.config/escapement/watch-pass
#     chmod 600 ~/.config/escapement/watch-pass

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


PASSWD = _read_password()


def drain(fd, seconds=1.0):
    buf = b""
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                buf += os.read(fd, 65536)
            except BlockingIOError:
                pass
    return buf


def login(fd, timeout=60):
    """Get to a root shell, logging in if the console is at `login:`.

    After a flash the board spends ~10 s booting, and anything typed into the
    boot log is lost -- so this waits for a prompt to appear rather than
    assuming one is already there. A bare newline is enough to make bash (or the
    login program) reprint its prompt, which is how the current state is read
    without guessing.
    """
    # A previous command that was killed on the host side may still be running
    # on the watch and holding the console's foreground -- a `head -c` on
    # /dev/esp-log does exactly this. It swallows the newline that would
    # reprint the prompt, so the next command appears to hang. Interrupt first.
    write_all(fd, b"\x03")
    time.sleep(0.3)
    drain(fd, 0.4)
    write_all(fd, b"\r")

    # Decide from the TAIL of what arrived, never from the whole accumulated
    # buffer. After a session that left several prompts stacked, the old text
    # is still in the stream, so a plain `in buf` test can act on a `login:`
    # that has already been answered and type credentials into a live shell.
    def state(window):
        tail = window[-400:]
        if b"# " in tail:
            return "shell"
        if b"login:" in tail or b"assword" in tail:
            return "login"
        return "unknown"

    deadline = time.time() + timeout
    sent_user = False
    while time.time() < deadline:
        buf = drain(fd, 0.6)
        st = state(buf)

        if st == "shell":
            return True

        if st == "login":
            if b"assword" in buf[-400:]:
                if not PASSWD:
                    raise SystemExit(
                        "watch is at a login prompt and no password is "
                        "configured; set WATCH_PASS or "
                        "~/.config/escapement/watch-pass")
                write_all(fd, PASSWD.encode() + b"\r")
                sent_user = True
                # `login` sleeps after a bad password; allow for it.
                time.sleep(2.5)
            elif not sent_user:
                write_all(fd, b"root\r")
                sent_user = True
                time.sleep(1.0)
            else:
                # Sent credentials and got the prompt back: they were wrong,
                # or the login did not take. One more try, then give up.
                return False
        else:
            # Nothing recognisable yet. Nudge the console to reprint.
            write_all(fd, b"\r")

    return False


def write_all(fd, data, deadline=10.0):
    """Write everything to a non-blocking tty.

    A tty's input queue is about 4 KiB. os.write() on a non-blocking fd returns
    a short count once that fills, so a single write silently truncates any
    command longer than the buffer -- and the symptom is not an error but a
    command that never finishes, because the trailing end marker was cut off.
    """
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
    return len(view) == 0


def read_until(fd, needles, deadline):
    buf = b""
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], 0.1)
        if not r:
            continue
        try:
            d = os.read(fd, 65536)
        except BlockingIOError:
            continue
        if d:
            buf += d
            for n in needles:
                if n in buf:
                    return buf, buf.split(n, 1)[0]
    return buf, buf


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd = sys.stdin.read() if sys.argv[1] == "-" else sys.argv[1]

    fd = os.open(PORT, os.O_RDWR | os.O_NONBLOCK)
    try:
        tty.setraw(fd)
        # Get back to a known-good prompt. A previous interrupted line can
        # leave bash at its secondary prompt (`> '), where commands are
        # swallowed as continuation lines.
        time.sleep(0.2)
        while select.select([fd], [], [], 0)[0]:
            try:
                os.read(fd, 65536)
            except BlockingIOError:
                break
        # The return value matters. Ignoring it was a real defect: when login
        # did not take, the command below was typed into a `login:` prompt,
        # thrown away, and the tool then waited out its full 180 s timeout for
        # an end marker that could never appear -- which reads as "the board
        # hung" when nothing was wrong with the board at all.
        if not login(fd):
            sys.exit("watch-exec: could not reach a root shell on %s "
                     "(console answered, but no '# ' prompt appeared). "
                     "The board may be frozen, or logged out." % PORT)

        # The literal markers never appear in the echoed command line: the
        # echo carries "%s_BEGIN%s", the real output carries "__WX_BEGIN__".
        script = (
            b"m=__WX; printf '\\n%s_BEGIN%s\\n' \"$m\" \"$m\"; " +
            cmd.encode() +
            b"; printf '\\n%s_END%s\\n' \"$m\" \"$m\"\r")
        if not write_all(fd, script):
            sys.exit("watch-exec: could not write the command to the console")

        buf, _ = read_until(fd, [b"__WX_END__"], time.time() + 180)
        # Let the shell render its next prompt before we let go.
        read_until(fd, [b"# "], time.time() + 2)
    finally:
        os.close(fd)

    if b"__WX_BEGIN__" in buf:
        buf = buf.split(b"__WX_BEGIN__", 1)[1]
    if b"__WX_END__" in buf:
        buf = buf.split(b"__WX_END__", 1)[0]
    text = buf.decode("utf-8", "replace")
    text = text.replace("\r\n", "\n").replace("\r", "")
    # Drop the first bit of echoed command that followed the marker echo.
    lines = text.split("\n")
    # The echoed tail of the command sits before the first real output line;
    # strip the leading marker-newline artifacts.
    while lines and lines[0].strip() == "":
        lines.pop(0)
    print("\n".join(lines).rstrip("\n"))


if __name__ == "__main__":
    main()
