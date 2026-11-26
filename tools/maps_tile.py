#!/usr/bin/env python3
"""Cut a 320x416 map image for the watch out of OpenTopoMap tiles.

    maps_tile.py --preview                     # write a PNG and look at it
    maps_tile.py --out /run/media/vash/CARD/media/maps/tile.tmap
    maps_tile.py --tile 15/6182/13149 --preview

WHAT THIS IS FOR, AND WHAT IT DELIBERATELY IS NOT. The TinyMaps project on the
Tang is a hard-real-time FPGA renderer -- a line engine, its own SDRAM
controller, TMDS out at 1280x720. None of that fits a wristwatch. What does
carry over is its data model and one rule: tiles are 256x256 OpenTopoMap PNGs,
and "nothing is resampled -- every pixel kept is the tile's own".

So this tool crops. It never scales. The watch's framebuffer is 320x416 and a
tile is 256x256, which means the screen cannot hold a whole tile at native size;
the honest answer is to take a 320x416 WINDOW of the tile mosaic at 1:1 rather
than to shrink a tile to fit. What lands on the glass is then real map pixels,
which is the only way to judge whether a map is legible on a two-inch screen.

FILE FORMAT (little-endian), 16-byte header then pixels:

    0    u32  magic   'TMAP'
    4    u16  version 1
    6    u16  format  1 = RGB565, LOGICAL byte order
    8    u16  width
    10   u16  height
    12   u32  CRC-32 over the pixel bytes

THE CRC IS NOT DECORATION. A card read that comes back short is a real fault on
this board (the SSH record notes 16 KiB/4 KiB limits, and the picker paid for a
truncated read as "did not start"). Without a checksum a short read shows as
garbage on the glass with nothing to say why.

WHY "LOGICAL" BYTE ORDER MATTERS. display_picture_rgb() copies into the
framebuffer and flush_rect() swaps each 16-bit word on the way out, because the
panel wants the high byte first. So the file stores a plain little-endian
uint16 per pixel and the driver does the swap exactly once. Storing panel order
here would be swapped twice and come out looking like noise.
"""

import argparse
import os
import struct
import sys
import zlib

TILE = 256                      # OpenTopoMap tile edge, px
FB_W, FB_H = 320, 416           # the watch's framebuffer
CACHE = os.path.expanduser("~/.cache/tinymaps/tiles")
MAGIC = b"TMAP"
VERSION = 1
FORMAT_RGB565 = 1
HEADER = 16


def tile_path(cache, z, x, y):
    return os.path.join(cache, "%d_%d_%d.png" % (z, x, y))


def have(cache, z, x, y):
    return os.path.isfile(tile_path(cache, z, x, y))


def mosaic(cache, z, x0, y0, nx, ny):
    """Tiles (x0..x0+nx-1, y0..y0+ny-1) as one RGB image, or an error naming
    exactly which tiles are missing. Naming them beats 'cannot build': the fix
    is to fetch those files, and a bare failure sends you looking for a bug."""
    from PIL import Image

    missing = [(x, y) for y in range(y0, y0 + ny) for x in range(x0, x0 + nx)
               if not have(cache, z, x, y)]

    if missing:
        sys.exit("maps_tile: %d tile(s) not cached under %s:\n%s\n"
                 "  fetch them with TinyMaps' own tool:  "
                 "tools/maps_pack.py fetch   (in the TinyMaps repository)"
                 % (len(missing), cache,
                    "\n".join("    %d/%d/%d" % (z, x, y) for x, y in missing[:12])))

    im = Image.new("RGB", (nx * TILE, ny * TILE))
    for j in range(ny):
        for i in range(nx):
            with Image.open(tile_path(cache, z, x0 + i, y0 + j)) as t:
                im.paste(t.convert("RGB"), (i * TILE, j * TILE))
    return im


def to_rgb565(im):
    """Logical little-endian RGB565: 5 bits red high, then 6 green, then 5 blue.

    Truncating with >> rather than rounding is the conventional mapping and it is
    what the rest of this port's palette tables do, so a map pixel and an
    emulator pixel of the same colour land on the same 16-bit value."""
    # tobytes() rather than getdata(): Pillow 12 deprecates the latter, and this
    # walks 133,120 pixels, so the flat buffer is both the faster path and the
    # one that will still exist.
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=CACHE, help="tile cache directory")
    ap.add_argument("--zoom", type=int, default=15,
                    help="tile zoom; the watch default is 15 (4.78 m/px)")
    ap.add_argument("--tile", help="centre tile as Z/X/Y; default is a tile in "
                                   "the middle of the cache")
    ap.add_argument("--out", help="write the .tmap image here")
    ap.add_argument("--preview", help="write a PNG of exactly the crop, to LOOK at")
    args = ap.parse_args()

    from PIL import Image

    # Choose a centre tile. Without --tile, take one near the middle of what is
    # cached, so the window has neighbours on every side and the tool does not
    # fail on a tile at the edge of a download.
    if args.tile:
        try:
            z, cx, cy = (int(v) for v in args.tile.split("/"))
        except ValueError:
            sys.exit("maps_tile: --tile wants Z/X/Y, e.g. 15/6182/13149")
    else:
        z = args.zoom
        pts = []
        for name in os.listdir(args.cache):
            if not name.endswith(".png"):
                continue
            try:
                tz, tx, ty = (int(v) for v in name[:-4].split("_"))
            except ValueError:
                continue
            if tz == z:
                pts.append((tx, ty))
        if not pts:
            sys.exit("maps_tile: no z%d tiles in %s" % (z, args.cache))
        # Pick the cached tile CLOSEST TO THE CENTRE OF WHAT IS CACHED that also
        # has every neighbour the window needs. Taking the plain median landed
        # on the edge of the download and asked for a tile one row outside it,
        # which is a failure the tool can avoid rather than report.
        have_set = set(pts)
        mx = sum(p[0] for p in pts) / len(pts)
        my = sum(p[1] for p in pts) / len(pts)

        def window_for(t):
            tx, ty = t
            left = tx * TILE + TILE // 2 - FB_W // 2
            top = ty * TILE + TILE // 2 - FB_H // 2
            return (left // TILE, top // TILE,
                    (left + FB_W - 1) // TILE, (top + FB_H - 1) // TILE)

        def complete(t):
            a, b, c, d = window_for(t)
            return all((x, y) in have_set
                       for y in range(b, d + 1) for x in range(a, c + 1))

        good = [t for t in pts if complete(t)]
        if not good:
            sys.exit("maps_tile: no cached z%d tile has a full %dx%d window "
                     "around it; cache more tiles first" % (z, FB_W, FB_H))
        cx, cy = min(good, key=lambda t: (t[0] - mx) ** 2 + (t[1] - my) ** 2)

    print("centre tile %d/%d/%d" % (z, cx, cy))

    # The window is centred on the CENTRE TILE'S CENTRE, so the crop is
    # reproducible from a tile name and does not depend on the cache contents.
    centre_x = cx * TILE + TILE // 2
    centre_y = cy * TILE + TILE // 2
    left = centre_x - FB_W // 2
    top = centre_y - FB_H // 2

    # Tiles needed to cover left..left+FB_W and top..top+FB_H. At most
    # ceil(320/256)+1 = 3 across and ceil(416/256)+1 = 4 down; the +1 is for a
    # window that straddles a tile boundary.
    tx0 = left // TILE
    ty0 = top // TILE
    tx1 = (left + FB_W - 1) // TILE
    ty1 = (top + FB_H - 1) // TILE
    nx, ny = tx1 - tx0 + 1, ty1 - ty0 + 1
    print("window  %dx%d at (%d,%d) of the mosaic; tiles x %d..%d y %d..%d (%dx%d)"
          % (FB_W, FB_H, left, top, tx0, tx1, ty0, ty1, nx, ny))

    big = mosaic(args.cache, z, tx0, ty0, nx, ny)

    # Crop in mosaic coordinates. This is a pure integer offset -- no resampling,
    # which is the whole point.
    ox = left - tx0 * TILE
    oy = top - ty0 * TILE
    crop = big.crop((ox, oy, ox + FB_W, oy + FB_H))
    assert crop.size == (FB_W, FB_H), crop.size

    if args.preview:
        crop.save(args.preview)
        print("preview %s" % args.preview)

    if args.out:
        px = to_rgb565(crop)
        assert len(px) == FB_W * FB_H * 2, len(px)
        hdr = struct.pack("<4sHHHHI", MAGIC, VERSION, FORMAT_RGB565,
                          FB_W, FB_H, zlib.crc32(px) & 0xFFFFFFFF)
        assert len(hdr) == HEADER, len(hdr)
        tmp = args.out + ".tmp"
        with open(tmp, "wb") as f:
            f.write(hdr)
            f.write(px)
        os.replace(tmp, args.out)
        st = os.stat(args.out)
        print("wrote %s  %d bytes (header %d + %d pixels)  crc %08x"
              % (args.out, st.st_size, HEADER, FB_W * FB_H,
                 zlib.crc32(px) & 0xFFFFFFFF))
        if st.st_size != HEADER + FB_W * FB_H * 2:
            sys.exit("maps_tile: short write")

    if not args.out and not args.preview:
        print("nothing to do: pass --out and/or --preview")


if __name__ == "__main__":
    main()
