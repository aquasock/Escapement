#!/usr/bin/env python3
"""Read firmware diagnostics from /dev/esp-ble on the watch.

    ./scripts/watch-logs.py            keep reading and printing
    ./scripts/watch-logs.py --raw      hex only, no decoding

Why this exists: the USB-Serial-JTAG port carries ESP-IDF log output only until
Linux claims it at about 1.75 s into boot. Everything after that is Linux printk,
and the firmware's own logs go nowhere -- which is precisely when the interesting
parts (WiFi, the IPC link, the BLE stack) are running. The firmware therefore also
writes its log lines down the esp-ble pipe, and this script decodes them.

Frames, as produced by the firmware:

    "@LOG <text>\n"                     log line (readable with plain cat)
    0xA5 0x01 <8 bytes>                  HID boot keyboard report
    anything else                        provisioning text, printed as-is

Run it on the watch itself, over the USB console:

    ./watch-logs.py
"""

import argparse
import sys

LOG_PREFIX = b"@LOG "
HID_MAGIC = 0xA5
TYPE_LOG = 0x01
TYPE_HID = 0x01

DEV = "/dev/esp-ble"


def decode_hid(report):
    """Turn an 8-byte boot keyboard report into something readable."""
    mods, _reserved = report[0], report[1]
    keys = [k for k in report[2:8] if k]
    names = []
    for bit, name in enumerate(("LCtrl", "LShift", "LAlt", "LMeta",
                                "RCtrl", "RShift", "RAlt", "RMeta")):
        if mods & (1 << bit):
            names.append(name)
    return "mods=%s keys=%s" % ("+".join(names) or "-", keys or "-")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw", action="store_true", help="hex dump, no decoding")
    ap.add_argument("--dev", default=DEV)
    args = ap.parse_args()

    try:
        f = open(args.dev, "rb", buffering=0)
    except OSError as e:
        sys.exit("%s: %s\nIs the kernel driver loaded and the pipe idle?"
                 % (args.dev, e))

    buf = b""
    while True:
        chunk = f.read(256)
        if not chunk:
            continue
        buf += chunk

        while True:
            if args.raw:
                sys.stdout.write(buf.hex(" ") + "\n")
                buf = b""
                break

            if len(buf) < 2:
                break

            # Log lines are printable: "@LOG <text>\n".
            if buf.startswith(LOG_PREFIX):
                end = buf.find(b"\n")
                if end < 0:
                    break
                sys.stdout.write(buf[len(LOG_PREFIX):end].decode(
                    "utf-8", "replace") + "\n")
                sys.stdout.flush()
                buf = buf[end + 1:]
                continue

            if magic == HID_MAGIC and len(buf) >= 10:
                sys.stdout.write("[hid] %s\n" % decode_hid(buf[2:10]))
                sys.stdout.flush()
                buf = buf[10:]
                continue

            # Not a frame we know: print a byte and resynchronise, so
            # provisioning text is visible rather than eating the stream.
            sys.stdout.write(buf[:1].decode("utf-8", "replace"))
            sys.stdout.flush()
            buf = buf[1:]


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
