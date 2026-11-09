#!/usr/bin/env python3
"""Check tbtns's button-band arithmetic against the painted geometry.

    python3 scripts/tbtns-selftest.py /tmp/tbtns_host

The three bands are the only decision in tbtns that can be wrong in a way
nobody notices until a finger is on the glass, and testing that on the watch
costs a deployment per attempt. So tbtns carries a `-C <column>` mode that
classifies a controller column and exits before opening /dev/i2c-0, and this
drives it on whatever machine is to hand.

The expected boundaries are not hardcoded twice. They are recomputed here from
the same geometry the kernel driver uses (PANEL_W, PANEL_OFFSET_X, PANEL_W/3),
and the test then asserts:

  * every column in 0..431 lands in a band (no gaps),
  * band 1 is contiguous and sits between the other two,
  * the first and last visible columns reach bands 0 and 2,
  * the classifier agrees with the driver's own band edges,
  * the edges are within half a fingertip of a true third, which is the
    property that makes the buttons usable when the touch Y axis is imprecise.

Exits non-zero on the first disagreement.
"""

import subprocess
import sys

# Mirrors patches/kernel/display/final/co5300.c.
PANEL_W = 410
PANEL_H = 502
PANEL_OFFSET_X = 22
BTN_CTRL_X1 = PANEL_OFFSET_X + PANEL_W - 1  # 431

EDGE1 = PANEL_OFFSET_X + PANEL_W // 3            # 158
EDGE2 = PANEL_OFFSET_X + 2 * (PANEL_W // 3)      # 294

# A fingertip is roughly 40 px across on this glass (TinyPetz measurement). An
# edge inside that distance of a true third means the band is where the user
# would expect from looking at the bars.
FINGERTIP = 20


def classify(binary, column):
    out = subprocess.run([binary, "-C", str(column)],
                         capture_output=True, text=True, timeout=10)
    if out.returncode != 0:
        raise SystemExit(f"tbtns -C {column} exited {out.returncode}: {out.stderr}")
    text = out.stdout.strip()
    if not text.isdigit():
        raise SystemExit(f"tbtns -C {column} printed {text!r}, expected a digit")
    return int(text)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    binary = sys.argv[1]

    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    # Every controller column classifies, and the sequence is a staircase of
    # exactly three steps. A gap or a wobble would mean an edge constant is off.
    seen = [classify(binary, c) for c in range(0, BTN_CTRL_X1 + 1)]
    check(sorted(set(seen)) == [0, 1, 2],
          f"expected bands 0,1,2 over 0..{BTN_CTRL_X1}, got {sorted(set(seen))}")

    # Contiguous and ordered: no column has a lower band than the one before it.
    check(all(b >= a for a, b in zip(seen, seen[1:])),
          "band index decreases as the column increases")

    # The first and last visible columns land in the outer bands, so no part of
    # the painted glass is dead.
    check(classify(binary, PANEL_OFFSET_X) == 0,
          "leftmost visible column does not reach band 0")
    check(classify(binary, BTN_CTRL_X1) == 2,
          "rightmost visible column does not reach band 2")

    # The classifier agrees with the driver's edges exactly.
    check(classify(binary, EDGE1 - 1) == 0, f"column {EDGE1 - 1} should be band 0")
    check(classify(binary, EDGE1) == 1, f"column {EDGE1} should be band 1")
    check(classify(binary, EDGE2 - 1) == 1, f"column {EDGE2 - 1} should be band 1")
    check(classify(binary, EDGE2) == 2, f"column {EDGE2} should be band 2")

    # Each edge is close to a true third of the visible glass, which is what
    # makes the regions feel even under a finger.
    third1 = PANEL_OFFSET_X + PANEL_W / 3
    third2 = PANEL_OFFSET_X + 2 * PANEL_W / 3
    check(abs(EDGE1 - third1) <= FINGERTIP,
          f"edge 1 ({EDGE1}) is more than {FINGERTIP} px from a true third ({third1:.1f})")
    check(abs(EDGE2 - third2) <= FINGERTIP,
          f"edge 2 ({EDGE2}) is more than {FINGERTIP} px from a true third ({third2:.1f})")

    # Report the widths rather than asserting them: the exact split depends on
    # integer division and the separators, and only the shape matters.
    widths = [seen.count(b) for b in (0, 1, 2)]
    print(f"tbtns bands: edges {EDGE1}/{EDGE2} over controller columns "
          f"0..{BTN_CTRL_X1}, widths {widths}")

    if failures:
        print("FAIL")
        for f in failures:
            print(f"  - {f}")
        return 1

    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
