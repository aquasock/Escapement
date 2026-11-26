#!/usr/bin/env python3
"""Turn cached OpenTopoMap tiles into a tiled map set for the watch.

    maps_tiles.py --list                        # what the cache holds, by zoom
    maps_tiles.py --zoom 15 --cols 8 --rows 8   # write an 8x8 block
    maps_tiles.py --zoom 15 --origin 6174,13141  # a specific block

WHAT IT WRITES, and why each part exists:

    <out>/t_<x>_<y>.tmap    one 256x256 RGB565 tile, header version 2, carrying
                            its OWN grid coordinates so the watch can prove it
                            opened the tile its filename claims.
    <out>/start.txt         two integers: the lowest tile coordinate present.

The app opens <dir>/t_<x>_<y>.tmap for the four tiles of its 2x2 square and slides
by one tile per arrow press. There is no index file and no pack: four files are
opened per slide, two of them already in memory, and a missing neighbour names
itself in the app's error rather than showing as a black quadrant with no reason.

WHY NO PALETTE YET. RGB565 is 128 KB a tile; 8bpp against a 256-entry palette is
64 KB, and TinyMaps itself stores 4 or 8 bits. That is the obvious next change and
it is deliberately not in this one: the point of the first tiled build is to find
out whether walking a map with four arrow presses feels right, and a format change
would confound that with a colour question if it went wrong.

The tile cache is OpenTopoMap's, fetched by TinyMaps' own tool. Nothing here
redistributes it: the tiles stay in a private cache and the set is written to the
operator's own card.
"""

import argparse
import glob
import os
import struct
import sys
import zlib

TILE = 256
MAGIC = b"TMAP"
VERSION = 2
FORMAT_RGB565 = 1
HEADER = 24
CACHE = os.path.expanduser("~/.cache/tinymaps/tiles")


def cached(cache, z):
    """{(x, y)} of cached tiles at one zoom."""
    out = set()
    for p in glob.glob(os.path.join(cache, "%d_*.png" % z)):
        base = os.path.basename(p)[:-4]
        try:
            _, x, y = base.split("_")
        except ValueError:
            continue
        out.add((int(x), int(y)))
    return out


def to_rgb565(im):
    """Logical little-endian RGB565: 5 bits red, 6 green, 5 blue.

    Truncating with >> rather than rounding is the conventional mapping and it is
    what the port's palette tables do, so a map pixel and an emulator pixel of the
    same colour land on the same 16-bit value."""
    rgb = im.tobytes()
    out = bytearray(len(rgb) // 3 * 2)
    o = 0
    for i in range(0, len(rgb), 3):
        r, g, b = rgb[i], rgb[i + 1], rgb[i + 2]
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out[o] = v & 0xFF
        out[o + 1] = v >> 8
        o += 2
    return bytes(out)


def write_tile(tiles, z, x, y, out):
    from PIL import Image

    src = os.path.join(tiles, "%d_%d_%d.png" % (z, x, y))

    if not os.path.isfile(src):
        return False

    with Image.open(src) as im:
        if im.size != (TILE, TILE):
            sys.exit("maps_tiles: %s is %dx%d, expected %dx%d"
                     % (src, im.size[0], im.size[1], TILE, TILE))
        px = to_rgb565(im.convert("RGB"))

    assert len(px) == TILE * TILE * 2, len(px)

    # Built piece by piece rather than from one format string, because the layout
    # has a four-byte magic followed by 16-bit fields -- so a single "<4sHHHH..." 
    # string needs padding care, and a format string that has to be reasoned about
    # is one that will be wrong once. 24 bytes: magic, version, format, w, h,
    # crc32, tile x, tile y.
    hdr = (MAGIC
           + struct.pack("<HHHH", VERSION, FORMAT_RGB565, TILE, TILE)
           + struct.pack("<I", zlib.crc32(px) & 0xFFFFFFFF)
           + struct.pack("<i", x)
           + struct.pack("<i", y))
    assert len(hdr) == HEADER, len(hdr)

    path = os.path.join(out, "t_%d_%d.tmap" % (x, y))
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(hdr)
        f.write(px)
    os.replace(tmp, path)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=CACHE)
    ap.add_argument("--out", default=".")
    ap.add_argument("--zoom", type=int, default=15)
    ap.add_argument("--cols", type=int, default=8)
    ap.add_argument("--rows", type=int, default=8)
    ap.add_argument("--origin", help="top-left tile as X,Y; default is the "
                                     "lowest cached tile that has a full block")
    ap.add_argument("--list", action="store_true", help="report the cache and exit")
    args = ap.parse_args()

    have = cached(args.cache, args.zoom)

    if not have:
        sys.exit("maps_tiles: no z%d tiles in %s" % (args.zoom, args.cache))

    xs = [t[0] for t in have]
    ys = [t[1] for t in have]

    if args.list:
        print("z%d: %d tiles cached, x %d..%d (%d wide), y %d..%d (%d tall)"
              % (args.zoom, len(have), min(xs), max(xs), max(xs) - min(xs) + 1,
                 min(ys), max(ys), max(ys) - min(ys) + 1))
        print("block sizes that fit entirely inside the cache:")
        for n in range(2, 20):
            fits = any(all((x + i, y + j) in have
                           for i in range(n) for j in range(n))
                       for (x, y) in have)
            if fits:
                print("  %dx%d = %d tiles = %.1f MB" % (n, n, n * n, n * n * 128 / 1024))
        return

    if args.origin:
        try:
            ox, oy = (int(v) for v in args.origin.split(","))
        except ValueError:
            sys.exit("maps_tiles: --origin wants X,Y")

        need = [(ox + i, oy + j) for j in range(args.rows) for i in range(args.cols)]
        missing = [t for t in need if t not in have]

        if missing:
            sys.exit("maps_tiles: %d tiles not cached at that origin, first: %d,%d"
                     % (len(missing), missing[0][0], missing[0][1]))
    else:
        # The lowest cached tile that has a full block to its right and below, so
        # the set does not run off the edge of the download on its first press.
        best = None
        for (x, y) in sorted(have):
            if all((x + i, y + j) in have
                   for j in range(args.rows) for i in range(args.cols)):
                best = (x, y)
                break

        if best is None:
            sys.exit("maps_tiles: no cached block of %dx%d exists; try --list"
                     % (args.cols, args.rows))
        ox, oy = best

    os.makedirs(args.out, exist_ok=True)

    written = 0
    for j in range(args.rows):
        for i in range(args.cols):
            if write_tile(args.cache, args.zoom, ox + i, oy + j, args.out):
                written += 1

    # start.txt is the app's default. Without it `maps` refuses with "no start tile
    # in the set" rather than opening a tile at (0,0), which is in the Atlantic.
    with open(os.path.join(args.out, "start.txt"), "w") as f:
        f.write("%d %d\n" % (ox, oy))

    print("wrote %d tiles from z%d at origin %d,%d into %s"
          % (written, args.zoom, ox, oy, args.out))
    print("  %.1f MB of tiles; start.txt says %d %d"
          % (written * 128 / 1024, ox, oy))
    print("  the watch walks this block with up to %d presses across and %d down"
          % (args.cols - 1, args.rows - 1))


if __name__ == "__main__":
    main()
