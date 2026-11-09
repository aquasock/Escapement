#!/usr/bin/env python3
"""Check the panel API's argument validation on the build host.

    python3 scripts/co5300-selftest.py

The rules live in drivers/video/fbdev/co5300_rect.h so they can be compiled off
the watch, and this drives them through a tiny C harness. Every rejection here
is a rule from docs/panel-api.md; a boundary that is off by one would otherwise
cost a flash and a look at the glass to discover.

Geometry under test matches the real panel (see docs/panel-reservations.md):

    panel        410 x 502
    fb0          x 45..364, y 45..444
    left strip   x  0.. 44      top    y  0.. 44
    right strip  x 365..409     bottom y 445..501
    side slots   45x45 at x 0..44 / 365..409, y 97..411 in steps of 45
"""

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
HEADER = os.path.join(ROOT, "patches/kernel/display/final/co5300_rect.h")

# The panel, from docs/panel-reservations.md.
PANEL_W, PANEL_H = 410, 502
FB_X, FB_Y, FB_W, FB_H = 45, 45, 320, 400

HARNESS = r"""
#define CO5300_HOSTTEST 1
#include <stdio.h>
#include <stdlib.h>
#include "co5300_rect.h"

/* Emits "idx result" lines; the Python side owns the expectations. */
int main(void)
{
	static const struct co5300_limits L = {
		.panel_w = 410, .panel_h = 502,
		.fb_x = 45, .fb_y = 45, .fb_w = 320, .fb_h = 400,
	};
	int x, y, w, h, stride, reserved, flags;

	while (scanf("%d %d %d %d %d %d %d", &x, &y, &w, &h,
		     &stride, &reserved, &flags) == 7) {
		struct co5300_rect r;
		r.x = (__u16)x; r.y = (__u16)y;
		r.w = (__u16)w; r.h = (__u16)h;
		r.stride = (__u16)stride;
		r.reserved = (__u16)reserved;
		r.flags = (__u32)flags;
		r.pixels = 0;
		printf("%d\n", co5300_rect_check(&r, &L));
	}
	return 0;
}
"""


def build_harness(tmpdir):
    src = os.path.join(tmpdir, "h.c")
    exe = os.path.join(tmpdir, "h")
    with open(src, "w") as f:
        f.write(HARNESS)
    cc = os.environ.get("CC", "cc")
    subprocess.run([cc, "-O1", "-Wall", "-Wextra",
                    "-I", os.path.join(ROOT, "patches/kernel/display/final"),
                    "-o", exe, src], check=True)
    return exe


def ask(exe, rects):
    payload = "\n".join(" ".join(str(v) for v in r) for r in rects) + "\n"
    out = subprocess.run([exe], input=payload, capture_output=True,
                         text=True, check=True)
    return [int(line) for line in out.stdout.split()]


def main():
    if not os.path.exists(HEADER):
        raise SystemExit(f"missing {HEADER}")

    cases = []   # (description, rect, should_pass)

    def case(desc, x, y, w, h, stride=0, reserved=0, flags=0, ok=True):
        cases.append((desc, (x, y, w, h, stride, reserved, flags), ok))

    # --- legal: the regions the spec defines -------------------------------
    case("left slot 0", 0, 97, 45, 45)
    case("left slot 6", 0, 367, 45, 45)
    case("right slot 0", 365, 97, 45, 45)
    case("right slot 6", 365, 367, 45, 45)
    case("top bar full width", 0, 0, 410, 45)
    case("top cell 2", 90, 0, 45, 45)
    case("bottom bar full width", 0, 445, 410, 57)
    case("1 px gap above fb0", 0, 44, 410, 1, ok=False)   # h < 2
    case("strip touching fb0 exactly", 0, 0, 45, 45)      # x1=44 < fb_x=45
    case("right strip abuts fb0", 365, 100, 45, 45)
    case("above fb0, full width", 0, 0, 410, 44)          # y1=43 < 45
    case("below fb0", 0, 445, 410, 50)
    case("stride larger than width", 0, 97, 45, 45, stride=64)

    # --- illegal: degenerate ----------------------------------------------
    case("width 1", 0, 97, 1, 45, ok=False)
    case("height 1", 0, 97, 45, 1, ok=False)
    case("both 1", 0, 97, 1, 1, ok=False)
    case("width 0", 0, 97, 0, 45, ok=False)
    case("height 0", 0, 97, 45, 0, ok=False)

    # --- illegal: reserved / flags ----------------------------------------
    case("reserved set", 0, 97, 45, 45, reserved=1, ok=False)
    case("flags set", 0, 97, 45, 45, flags=1, ok=False)

    # --- illegal: stride ---------------------------------------------------
    case("stride smaller than width", 0, 97, 45, 45, stride=44, ok=False)

    # --- illegal: bounds ---------------------------------------------------
    case("x at panel edge", 409, 97, 2, 45, ok=False)
    case("y at panel edge", 0, 501, 45, 2, ok=False)
    case("w spills right", 400, 97, 45, 45, ok=False)
    case("h spills bottom", 0, 490, 45, 45, ok=False)
    case("entirely off panel right", 411, 97, 45, 45, ok=False)

    # --- illegal: overlap with fb0 ----------------------------------------
    case("inside fb0", 100, 100, 45, 45, ok=False)
    case("one column into fb0 from left", 44, 200, 45, 45, ok=False)
    case("one column into fb0 from right", 320, 200, 45, 45, ok=False)
    case("one row into fb0 from above", 100, 44, 45, 45, ok=False)
    case("one row into fb0 from below", 100, 400, 45, 45, ok=False)
    case("top strip reaching into fb0", 0, 20, 410, 30, ok=False)
    case("full panel", 0, 0, 410, 502, ok=False)

    # --- boundaries that must be exactly right ----------------------------
    # fb0 occupies y 45..444; the last legal row above it starts at y=0 with
    # h=45 (y1=44), and the first legal row below starts at y=445.
    case("last row above fb0, exact", 100, 0, 45, 45)      # y1 = 44
    case("first row below fb0, exact", 100, 445, 45, 45)   # y0 = 445
    case("left of fb0, exact", 0, 200, 45, 45)             # x1 = 44
    case("right of fb0, exact", 365, 200, 45, 45)          # x0 = 365
    # fb0 is x 45..364, so x=364 with w=2 overlaps; x=365 does not.
    case("x=364 w=2 overlaps", 364, 200, 2, 45, ok=False)
    case("x=365 w=2 clear", 365, 200, 2, 45)
    # fb0 rows are 45..444, so y=444 h=2 overlaps and y=445 does not.
    case("y=444 h=2 overlaps", 100, 444, 45, 2, ok=False)
    case("y=445 h=2 clear", 100, 445, 45, 2)
    # Above: y=43 h=2 -> y1=44, clear. y=44 h=2 -> y1=45, overlaps.
    case("y=43 h=2 clear", 100, 43, 45, 2)
    case("y=44 h=2 overlaps", 100, 44, 45, 2, ok=False)

    grid_fail = []
    with tempfile.TemporaryDirectory() as tmp:
        exe = build_harness(tmp)
        results = ask(exe, [c[1] for c in cases])

        # Coverage sanity: every slot the spec defines must be legal.
        grid = [(x0, y0, 45, 45, 0, 0, 0)
                for y0 in (97 + 45 * i for i in range(7))
                for x0 in (0, 365)]
        for rect, rc in zip(grid, ask(exe, grid)):
            if rc != 0:
                grid_fail.append((rect, rc))

    failures = []
    for (desc, rect, should_pass), rc in zip(cases, results):
        passed = rc == 0
        if passed != should_pass:
            failures.append(
                f"{desc}: rect {rect} -> {'accepted' if passed else f'rejected ({rc})'}, "
                f"expected {'accepted' if should_pass else 'rejected'}")

    total = len(cases)
    print(f"co5300 rect validation: {total - len(failures)}/{total} cases agree")
    if grid_fail:
        print(f"grid: {len(grid_fail)} slot(s) rejected unexpectedly: {grid_fail[:4]}")
    else:
        print("grid: all 14 side slots accepted")

    if failures or grid_fail:
        print("FAIL")
        for f in failures:
            print("  - " + f)
        return 1

    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
