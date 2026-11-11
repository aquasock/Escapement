#!/usr/bin/env python3
"""
gen_map.py - generate the V3 LCD map: a 32x30 dot matrix plus 10 icon glyphs.

Why this exists, and what it replaces
-------------------------------------
The previous map was generated at 49x49 *pixels* and the renderer multiplied
every map pixel by the dot pitch. That worked for the matrix, but it meant the
10 status icons were rasterised at 49px and then blown up six times, so they
came out as chunky blocky shapes instead of the smooth glyphs the hardware
draws. It also forced the matrix to share one coordinate space with the icons,
which is why an icon flip used to dirty most of the screen.

The artwork says the two things are not the same kind of object:

  * 960 segments are exactly 22.5 x 22.5 units, forming a complete 32 x 30
    grid. That is the pet. It is a dot matrix, so it is stored as a bitmap and
    drawn as DOT x DOT blocks.
  * 10 segments are complex glyphs 120-150 units wide, in two rows of five.
    Those are the status icons. They are two-state -- lit or not -- so they are
    stored as full-resolution run lists and drawn as glyphs.

Getting the resolution right is the whole point: the icons are now rasterised
at the LCD's true pixel size, so they are sharp, and each is a small table that
can be repainted without touching the matrix.

  python3 gen_map.py <Tamagotchi_v3.svg> segmap.c segmap.h

Coordinate spaces
-----------------
The matrix is in DOT units: cell (col, row) with 0 <= col < 32, 0 <= row < 30.
The icons are in PIXEL units relative to the LCD's top-left corner, because
they are not on the dot grid and quantising them is exactly the defect this
fixes. The renderer scales dot cells by (DOT + GAP) and draws icon runs as-is.
"""

import os
import re
import sys

# Import the path parser rather than reimplementing it. It handles M/L/H/V/C,
# and a naive x,y alternation over the raw `d` attribute produces garbage --
# that mistake showed up as a grid of lines across the display.
_HARNESS = os.environ.get("HARNESS", "/run/media/vash/GIT/TamaHarness/portable")
sys.path.insert(0, _HARNESS)
import importlib.util
_spec = importlib.util.spec_from_file_location("gen_segmap", os.path.join(_HARNESS, "gen_segmap.py"))
_g = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_g)

DOT_PX = int(os.environ.get("SEG_DOT", "5"))
GAP_PX = int(os.environ.get("SEG_GAP", "1"))

# Artwork units, measured from the SVG.
DOT_UNITS = 22.5
PITCH_UNITS = 25.1

MATRIX_W, MATRIX_H = 32, 30


def main(argv):
    svg_path, out_c, out_h = argv[1], argv[2], argv[3]
    svg = open(svg_path, encoding="utf-8").read()

    segs = _g.extract_segments(svg)
    parsed = {k: [poly for d in ds for poly in _g.parse_path(d)]
              for k, ds in segs.items()}

    # Classify by geometry. A dot is a ~22.5-unit square; an icon is much
    # larger in at least one dimension. This is the same test gen_segmap.py
    # uses, and on this artwork it separates them cleanly: 960 and 10.
    dots, icons = {}, {}
    for k, polys in parsed.items():
        pts = [p for poly in polys for p in poly]
        if not pts:
            continue
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        w, h = max(xs) - min(xs), max(ys) - min(ys)
        if 15 <= w <= 30 and 15 <= h <= 30:
            dots[k] = (min(xs), min(ys))
        else:
            icons[k] = polys

    if len(dots) != MATRIX_W * MATRIX_H:
        print("expected %d dots, found %d" % (MATRIX_W * MATRIX_H, len(dots)),
              file=sys.stderr)

    # ---- matrix grid -------------------------------------------------------
    dx0 = min(v[0] for v in dots.values())
    dy0 = min(v[1] for v in dots.values())

    cell = {}                                    # (col, row) -> (byte, bit)
    for k, (x, y) in dots.items():
        col = int(round((x - dx0) / PITCH_UNITS))
        row = int(round((y - dy0) / PITCH_UNITS))
        if not (0 <= col < MATRIX_W and 0 <= row < MATRIX_H):
            print("dot %s out of grid: %d,%d" % (k, col, row), file=sys.stderr)
            continue
        cell[(col, row)] = k

    missing = [(c, r) for r in range(MATRIX_H) for c in range(MATRIX_W)
               if (c, r) not in cell]
    if missing:
        print("WARNING: %d grid cells have no dot: %s"
              % (len(missing), missing[:8]), file=sys.stderr)

    # ---- geometry in pixels ------------------------------------------------
    # The matrix keeps the dot pitch. The icons keep their TRUE proportions,
    # scaled by the same factor so the whole LCD matches the artwork.
    scale = float(DOT_PX + GAP_PX) / PITCH_UNITS
    pitch = DOT_PX + GAP_PX

    # Whole-artwork bounding box, in artwork units, then in pixels.
    all_pts = [p for polys in parsed.values() for poly in polys for p in poly]
    ax0 = min(p[0] for p in all_pts)
    ay0 = min(p[1] for p in all_pts)
    lcd_w = int(round((max(p[0] for p in all_pts) - ax0) * scale))
    lcd_h = int(round((max(p[1] for p in all_pts) - ay0) * scale))

    # Pixel origin of matrix cell (0,0).
    mx = int(round((dx0 - ax0) * scale))
    my = int(round((dy0 - ay0) * scale))

    # ---- icons -------------------------------------------------------------
    icon_entries = []
    icon_runs = []

    def raster_icon(polys):
        """Flatten an icon to horizontal runs at LCD pixel resolution."""
        out = []
        polys_px = [[( (x - ax0) * scale, (y - ay0) * scale ) for (x, y) in poly]
                    for poly in polys]
        ys = [p[1] for poly in polys_px for p in poly]
        if not ys:
            return out
        y_lo, y_hi = max(0, int(min(ys))), min(lcd_h - 1, int(max(ys)) + 1)
        for y in range(y_lo, y_hi + 1):
            xs = []
            # Scanline against every edge of every flattened polygon.
            for poly in polys_px:
                n = len(poly)
                for i in range(n):
                    x1, y1 = poly[i]
                    x2, y2 = poly[(i + 1) % n]
                    if y1 == y2:
                        continue
                    if (y1 <= y + 0.5 < y2) or (y2 <= y + 0.5 < y1):
                        xs.append(x1 + (y + 0.5 - y1) * (x2 - x1) / (y2 - y1))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                a, b = int(round(xs[i])), int(round(xs[i + 1]))
                if b > a:
                    out.append((y, a, b))
        return out

    for k in sorted(icons):
        runs = raster_icon(icons[k])
        if not runs:
            continue
        icon_entries.append((k[0], k[1], len(icon_runs), len(runs)))
        icon_runs.extend(runs)

    # ---------------- emit --------------------------------------------------
    with open(out_h, "w") as f:
        f.write("/* Generated by gen_map.py - do not edit.\n")
        f.write(" * %dx%d dot matrix + %d icon glyphs, LCD %dx%d px, dot %d gap %d.\n"
                % (MATRIX_W, MATRIX_H, len(icon_entries), lcd_w, lcd_h, DOT_PX, GAP_PX))
        f.write(" * Matrix cells are DOT units; icon runs are LCD pixels. */\n\n")
        f.write("#ifndef SEGMAP_H\n#define SEGMAP_H\n#include <stdint.h>\n\n")
        f.write("#define SEG_MATRIX_W  %d\n#define SEG_MATRIX_H  %d\n" % (MATRIX_W, MATRIX_H))
        f.write("#define SEG_LCD_W     %d\n#define SEG_LCD_H     %d\n" % (lcd_w, lcd_h))
        f.write("#define SEG_DOT       %d\n#define SEG_GAP       %d\n" % (DOT_PX, GAP_PX))
        f.write("#define SEG_MATRIX_OX %d\n#define SEG_MATRIX_OY %d\n\n" % (mx, my))
        f.write("#define SEG_ICON_COUNT %d\n#define SEG_ICON_RUN_COUNT %d\n\n"
                % (len(icon_entries), len(icon_runs)))
        f.write("/* One cell per dot, packed (byte << 8) | bit, in the pet's dpram. */\n")
        f.write("extern const uint16_t seg_matrix[SEG_MATRIX_W * SEG_MATRIX_H];\n\n")
        f.write("/* An icon is two-state: its bit is either lit or not. */\n")
        f.write("typedef struct { uint8_t byte, bit; uint16_t run_start, run_count; } seg_icon;\n")
        f.write("typedef struct { uint16_t y, x0, x1; } seg_run;\n\n")
        f.write("extern const seg_icon seg_icons[SEG_ICON_COUNT];\n")
        f.write("extern const seg_run  seg_icon_runs[SEG_ICON_RUN_COUNT];\n\n")
        f.write("#endif\n")

    with open(out_c, "w") as f:
        f.write("/* Generated by gen_map.py - do not edit. */\n")
        f.write('#include "segmap.h"\n\n')
        f.write("const uint16_t seg_matrix[SEG_MATRIX_W * SEG_MATRIX_H] = {\n")
        for r in range(MATRIX_H):
            row = []
            for c in range(MATRIX_W):
                b, n = cell.get((c, r), (0xFF, 0))
                row.append("0x%04X" % ((b << 8) | n))
            f.write("  " + ", ".join(row) + ",\n")
        f.write("};\n\n")
        f.write("const seg_icon seg_icons[SEG_ICON_COUNT] = {\n")
        for (b, n, start, count) in icon_entries:
            f.write("  {%3d, %d, %5d, %4d},\n" % (b, n, start, count))
        f.write("};\n\n")
        f.write("const seg_run seg_icon_runs[SEG_ICON_RUN_COUNT] = {\n")
        for (y, x0, x1) in icon_runs:
            f.write("  {%3d, %3d, %3d},\n" % (y, x0, x1))
        f.write("};\n")

    print("matrix %dx%d (%d cells), %d icons in %d runs, LCD %dx%d px, dot %d gap %d"
          % (MATRIX_W, MATRIX_H, len(cell), len(icon_entries), len(icon_runs),
             lcd_w, lcd_h, DOT_PX, GAP_PX))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
