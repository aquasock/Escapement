#!/usr/bin/env python3
"""Build a frame file for `blekbd -f` and print what the decoder should produce.

The daemon's HID report-descriptor parser is the part most likely to be subtly
wrong, and it can be exercised without a keyboard in the room: record the
descriptor and a few reports as the firmware would frame them, then play them
back through exactly the decoder the device path uses.

    ./scripts/blekbd-selftest.py frames.bin
    /tmp/blekbd_host -f frames.bin -v

The descriptor below is modelled on a Logitech multi-function BLE keyboard: one
report ID for the keyboard array, one for the consumer/media page. It is not
copied from the real keyboard -- it is a shape that has the same features, so
that a parser bug shows up here rather than on the watch.
"""

import sys

# --- HID report descriptor ------------------------------------------------
#
# 05 01        Usage Page (Generic Desktop)
# 09 06        Usage (Keyboard)
# A1 01        Collection (Application)
# 85 01          Report ID (1)
# 05 07          Usage Page (Keyboard/Keypad)
# 19 E0          Usage Minimum (Left Control)
# 29 E7          Usage Maximum (Right GUI)
# 15 00          Logical Minimum (0)
# 25 01          Logical Maximum (1)
# 75 01          Report Size (1)
# 95 08          Report Count (8)
# 81 02          Input (Data, Variable, Absolute)      -- modifier bitfield
# 95 01          Report Count (1)
# 75 08          Report Size (8)
# 81 01          Input (Constant)                       -- reserved byte
# 95 06          Report Count (6)
# 75 08          Report Size (8)
# 15 00          Logical Minimum (0)
# 26 FF 00       Logical Maximum (255)
# 19 00          Usage Minimum (0)
# 2A FF 00       Usage Maximum (255)
# 81 00          Input (Data, Array, Absolute)         -- six key slots
# 05 08          Usage Page (LEDs)
# 19 01          Usage Minimum (Num Lock)
# 29 05          Usage Maximum (Kana)
# 95 05          Report Count (5)
# 75 01          Report Size (1)
# 91 02          Output (Data, Variable, Absolute)
# 95 01          Report Count (1)
# 75 03          Report Size (3)
# 91 01          Output (Constant)
# C0           End Collection
# 05 0C        Usage Page (Consumer)
# 09 01        Usage (Consumer Control)
# A1 01        Collection (Application)
# 85 02          Report ID (2)
# 15 00          Logical Minimum (0)
# 26 FF 03       Logical Maximum (1023)
# 19 00          Usage Minimum (0)
# 2A FF 03       Usage Maximum (1023)
# 75 10          Report Size (16)
# 95 01          Report Count (1)
# 81 00          Input (Data, Array, Absolute)
# C0           End Collection

DESCRIPTOR = bytes([
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08,
    0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x19, 0x00, 0x2A, 0xFF, 0x00,
    0x81, 0x00,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x95, 0x05, 0x75, 0x01,
    0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0xC0,
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x85, 0x02,
    0x15, 0x00, 0x26, 0xFF, 0x03,
    0x19, 0x00, 0x2A, 0xFF, 0x03,
    0x75, 0x10, 0x95, 0x01,
    0x81, 0x00,
    0xC0,
])

FRAME_MAGIC = 0xA5
FRAME_INPUT = 0x01
FRAME_MAP = 0x02
FRAME_STATE = 0x03

# (report bytes, what should happen)
CASES = [
    # 'a' down. Report ID 1, no modifiers, usage 0x04 in slot 1.
    (bytes([0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00]),
     "KEY_A down"),
    (bytes([0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]),
     "KEY_A up"),
    # Left shift + 'b'.
    (bytes([0x01, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00]),
     "KEY_LEFTSHIFT down, KEY_B down"),
    (bytes([0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]),
     "KEY_LEFTSHIFT up, KEY_B up"),
    # Enter, and a two-key chord in the array.
    (bytes([0x01, 0x00, 0x00, 0x28, 0xE1, 0x00, 0x00, 0x00, 0x00]),
     "KEY_ENTER down, KEY_LEFTSHIFT down"),
    (bytes([0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]),
     "KEY_ENTER up, KEY_LEFTSHIFT up"),
    # Consumer report ID 2: volume up (0x00E9), little endian.
    (bytes([0x02, 0xE9, 0x00]), "KEY_VOLUMEUP down"),
    (bytes([0x02, 0x00, 0x00]), "KEY_VOLUMEUP up"),
    # Consumer: play/pause (0x00CD).
    (bytes([0x02, 0xCD, 0x00]), "KEY_PLAYPAUSE down"),
    (bytes([0x02, 0x00, 0x00]), "KEY_PLAYPAUSE up"),
]


def frame(t, payload):
    if t == FRAME_INPUT:
        return bytes([FRAME_MAGIC, FRAME_INPUT, len(payload)]) + payload
    if t == FRAME_MAP:
        n = len(payload)
        return bytes([FRAME_MAGIC, FRAME_MAP, n & 0xFF, n >> 8]) + payload
    if t == FRAME_STATE:
        return bytes([FRAME_MAGIC, FRAME_STATE, payload])
    raise ValueError(t)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "frames.bin"

    data = b"".join([
        frame(FRAME_STATE, 1),
        frame(FRAME_MAP, DESCRIPTOR),
        frame(FRAME_STATE, 2),
    ] + [frame(FRAME_INPUT, r) for r, _ in CASES])

    with open(out, "wb") as f:
        f.write(data)

    print("wrote %s (%d bytes, descriptor %d bytes, %d reports)"
          % (out, len(data), len(DESCRIPTOR), len(CASES)))
    print("\nexpected decoding, in order:")
    for _, expect in CASES:
        print("  %s" % expect)


if __name__ == "__main__":
    main()
