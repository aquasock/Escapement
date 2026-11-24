#!/usr/bin/env python3
"""Populate the card's ROM tree from a MiSTer games collection.

    gbcstage.py --dry-run
    gbcstage.py --go

WHAT THIS SOLVES. The MiSTer card stores cartridges as one .zip per game, and
the emulator cannot read a zip -- it maps a ROM buffer and executes from it. So
each archive is opened, its single .gb/.gbc entry is extracted, and the ROM is
placed by its OWN extension rather than by which MiSTer folder it came from:
the operator warned that both collections hold a mixture, and a .gb sitting in
the GBC folder is exactly the case that would otherwise land in the wrong tree.

BUCKETS. The card keeps cartridges in alphabetical folders, and the bucket size
is not cosmetic: the picker holds at most 61 ROMs per list, so a folder of 543
cannot be browsed. Each first character gets its own bucket, and any group over
the cap is split into contiguous chunks named for the 2-character prefix of
their first and last entries -- the same shape the operator's existing tree had.

SKIPPED, deliberately: the Palettes/ directories (SGB and Extra colour tables,
not cartridges), mister-boot.gb (the MiSTer's own boot ROM) and boot1.rom.
"""

import argparse
import os
import shutil
import sys
import zipfile

SOURCES = [
    ("/run/media/vash/MiSTer_Data/games/GBC", ".gbc"),
    ("/run/media/vash/MiSTer_Data/games/Gameboy", ".gb"),
]

DEST = {
    ".gbc": "/run/media/vash/ESCAPEMENT/media/gbc/retail",
    ".gb": "/run/media/vash/ESCAPEMENT/media/gb/retail",
}

CAP = 61                      # the picker's list limit, PK_MAX_ROM is 64
SKIP_NAMES = {"mister-boot.gb", "boot1.rom"}


def collect():
    """Return [(zip_path, member, rom_name, ext)] for every cartridge."""
    found = []

    for src, _hint in SOURCES:
        if not os.path.isdir(src):
            print(f"missing source: {src}", file=sys.stderr)
            continue

        for entry in sorted(os.listdir(src)):
            path = os.path.join(src, entry)
            low = entry.lower()

            if not entry.lower().endswith(".zip"):
                continue
            if entry in SKIP_NAMES:
                continue

            try:
                with zipfile.ZipFile(path) as z:
                    roms = [n for n in z.namelist()
                            if n.lower().endswith((".gb", ".gbc", ".gbx"))
                            and not n.startswith("__MACOSX")]
            except (zipfile.BadZipFile, OSError) as e:
                print(f"  unreadable: {entry}: {e}", file=sys.stderr)
                continue

            if not roms:
                print(f"  no cartridge inside: {entry}", file=sys.stderr)
                continue
            if len(roms) > 1:
                print(f"  {len(roms)} entries, taking the first: {entry}", file=sys.stderr)

            name = os.path.basename(roms[0])
            ext = os.path.splitext(name)[1].lower()
            found.append((path, roms[0], name, ext))

    return found


def prefix(name, n=2):
    """Two characters of the title, for naming a split bucket."""
    s = name.strip()
    return s[:n] if len(s) >= n else s


def bucket_name(names):
    """A bucket's folder name: one character, or first2-last2 when split."""
    if not names:
        return "?"
    if len(names) == 1:
        return prefix(names[0], 1).upper()

    return f"{prefix(names[0])}-{prefix(names[-1])}"


def plan(found):
    """Group cartridges into (ext, bucket_foldername) -> [(zip, member, name)]."""
    by_ext = {}

    for item in found:
        by_ext.setdefault(item[3], []).append(item)

    placement = []

    for ext, items in sorted(by_ext.items()):
        items.sort(key=lambda t: t[2].lower())

        groups = {}
        for it in items:
            key = it[2][:1].upper()

            # A title starting with punctuation -- a "(Hack)" prefix, say --
#             gets its own visible bucket rather than a folder named "(" , which
#             reads as damage and sorts before the digits.
            if not key.isalnum():
                key = "#"

            groups.setdefault(key, []).append(it)

        for key, group in sorted(groups.items()):
            if len(group) <= CAP:
                placement.append((ext, key if not key.isdigit() else key, group))
                continue

            parts = (len(group) + CAP - 1) // CAP
            size = (len(group) + parts - 1) // parts

            for i in range(0, len(group), size):
                chunk = group[i:i + size]
                placement.append((ext, bucket_name([c[2] for c in chunk]), chunk))

    return placement


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--go", action="store_true")
    args = ap.parse_args()

    if not args.dry_run and not args.go:
        ap.error("choose --dry-run or --go")

    found = collect()
    print(f"cartridges found: {len(found)}")

    placement = plan(found)
    total = sum(len(p[2]) for p in placement)

    print(f"buckets: {len(placement)} covering {total} cartridges")
    print()

    for ext, bucket, items in sorted(placement, key=lambda p: (p[0], p[1])):
        print(f"  {ext:5} {bucket:<10} {len(items):3}   "
              f"{os.path.basename(items[0][2])[:44]}")

    over = [p for p in placement if len(p[2]) > CAP]
    if over:
        print(f"\nBUCKETS OVER CAP: {len(over)}")

    if args.dry_run:
        print("\ndry run: nothing written")
        return 0

    written = skipped = 0

    for ext, bucket, items in placement:
        out_dir = os.path.join(DEST[ext], bucket)
        os.makedirs(out_dir, exist_ok=True)

        for zip_path, member, name, _e in items:
            out = os.path.join(out_dir, name)

            if os.path.exists(out) and os.path.getsize(out) > 0:
                skipped += 1
                continue

            with zipfile.ZipFile(zip_path) as z, z.open(member) as src:
                with open(out, "wb") as dst:
                    shutil.copyfileobj(src, dst)

            written += 1

    print(f"\nwritten {written}, already present {skipped}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
