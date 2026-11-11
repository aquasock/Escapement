## 59 COMMIT Unreleased 2026-11-11T10:40:00-07:00

#### Coming From:

Unreleased c9c8a45

#### Purpose:

Render the V3 display as what it actually is -- a 32x30 dot matrix plus ten status icons drawn as glyphs -- instead of quantising both onto one 49x49 grid.

#### Outcome:

The geometry is corrected and deployed. The root cause was that `gen_segmap.py` computed one bounding box over all 970 artwork segments and emitted a map at 49x49 *pixels*, which the renderer then multiplied by the dot pitch. Two consequences followed, and both were defects: the 49x49 figure was the whole artwork box (1219.7 units divided by the 25.1 unit pitch), not the LCD, so the matrix was only 32x30 of it; and the ten status icons were rasterised at 49px and then magnified six times, which is why they looked chunky rather than like the artwork's smooth glyphs. Measuring the artwork settled both: of 970 segments, exactly 960 are 22.5x22.5 unit squares forming a complete 32x30 grid with no gaps or strays, and exactly 10 are complex glyphs 120-150 units wide in two rows of five. `patches/harness/segmap-1x/gen_map.py` replaces the old generator and emits two coordinate spaces, because the two objects are not the same kind of thing: the matrix as a 960-entry table of packed byte/bit cells in DOT units, and the icons as full-resolution horizontal runs in LCD pixels. Rendered output was checked visually before any of it was wired up -- five smooth glyphs above, a clean 32x30 grid, five smooth glyphs below -- which is the check the old map would have failed and did not have. `render.h` now draws the matrix as DOT x DOT blocks on a (DOT+GAP) pitch and the icons as glyph runs, and still owns every pixel in the range so a segment turning off is erased rather than left showing the previous frame. `shmproto.h` carries the matrix as 32x30 dots and the LCD as 292x300 pixels, and `_Static_assert`s in `render.h` now fail the build if the generated map and the shared contract ever disagree -- they cannot include each other, the map being generated and the header being in another tree, so an unchecked pairing would have been silent. The dirty-row tables were rebuilt for the new model: they are keyed by drawable item rather than by byte, so a changed byte is XORed with its shadow and only items whose bit actually changed contribute, and the top icon row is tracked as a separate band from the matrix and bottom icons. The firmware went from 4 percent free to 8, because 5366 runs of const data were replaced by 120 bytes of matrix plus 620 icon runs. Booted and deployed: firmware and userspace both flashed, `check-vectors.sh` green, emulator running with frames advancing. **What is not verified, and this matters.** The operator has not yet confirmed the picture on the glass. The two-band dirty tracking is computed but not yet delivered to userspace -- `dirty_lo`/`dirty_hi` still publish the union of both bands, so core 1 still blits one range and the blit-side saving is not yet realised; the drawing side does benefit. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 58, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The glass check is the first thing, because everything else was verified off-device and the operator has not looked yet. Then the two-band blit: the bands are computed in `tamacore_task.c` as `lo1`/`hi1` for the top icon row and `lo2`/`hi2` for the rest, but only their union is published, so `struct shm_ctl` needs a second pair of dirty fields and `userspace/tamacore.c` needs to blit both ranges. That is a shared-structure change, so both halves must be flashed together and `check-vectors.sh` must pass, exactly as with this cycle. The frame is now 300x308 rather than 324x348, so a full repaint is smaller, but nobody has measured the new draw and blit cost against the old numbers -- `draw_max` read 67.8 ms on the first samples of a fresh boot, which includes the initial full paint and is not comparable to the 47 ms steady-state figure in entry 58; that wants a clean measurement. The `gen_map.py` script depends on `HARNESS` pointing at the TamaHarness checkout for its path parser, which is a build-time dependency on a tree outside this repository and should either be vendored or documented in `docs/`.

#### Files Modified:

- patches/harness/segmap-1x/gen_map.py
- patches/harness/segmap-1x/segmap.c
- patches/harness/segmap-1x/segmap.h
- patches/harness/core0/render.h
- patches/harness/core0/shmproto.h
- patches/harness/core0/tamacore_task.c
- userspace/tamacore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

