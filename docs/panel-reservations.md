# Panel reservations — the four edges around fb0

Status: **specification, not yet implemented.** Agreed 2026-11-09. Everything
here is derived from measured panel geometry (`docs/panel-geometry.md`,
`docs/panel-reservations.md` uses the same constants) and from the driver's own
layout in `patches/kernel/display/final/co5300.c`.

This document reserves space. It does not describe code that exists yet, apart
from the bottom bar, which is already painted and already reports input.

## Why this exists

The framebuffer is the console, not the glass. That leaves four strips around it
that nothing owns. Left alone they are black waste; the point of this
specification is to give them a purpose and a grid, so that every later feature
draws into a known rectangle instead of inventing its own.

The alternative — four independent features each with their own geometry — was
rejected. They are one operation: **write RGB565 pixels into a named rectangle
in panel coordinates, outside fb0.** The bottom bar is already exactly that
operation; it happens to be painted once at probe time rather than on demand.

## Geometry

Measured constants, not assumptions:

```
panel            410 x 502
fb0                x  45 .. 364    (320 wide)
                   y  45 .. 444    (400 tall, 25 rows of the 8x16 cell)
left strip         x   0 ..  44    (45 wide)
right strip        x 365 .. 409    (45 wide)
top strip          y   0 ..  44    (45 tall)
bottom strip       y 445 .. 501    (57 tall)
```

The corners are 45-degree chamfers with measured legs: top-left `x + y >= 80`,
top-right `(410 - x) + y >= 90`, bottom-left `x + (502 - y) >= 82`. The
bottom-right reading was unreliable when the glass was measured and is assumed
to match bottom-left.

## The grid

One module, **45 px**, used for every edge. It is the largest square that fits a
side strip without touching the framebuffer, so it needs no change to fb0 and
no loss of console columns.

### Side strips: 7 slots each, 45 x 45

The two sides are not symmetric — the left strip is full-width from row 80, the
right only from row 89, because the top-right chamfer leg is 10 px longer. Tiles
are therefore laid out on the **intersection**, rows 89..420 (332 rows), centred
within it so the two sides line up with each other:

```
slot 0   y  97 .. 141        slot 4   y 277 .. 321
slot 1   y 142 .. 186        slot 5   y 322 .. 366
slot 2   y 187 .. 231        slot 6   y 367 .. 411
slot 3   y 232 .. 276

left slots   x   0 ..  44
right slots  x 365 .. 409
```

7 tiles use 315 of the 332 available rows; the 17 leftover rows are split 8
above and 9 below. **All fourteen slots are 100 % unclipped** — checked cell by
cell against the chamfer inequalities above.

Slots tile edge to edge with no gutter. Icon artwork carries its own padding,
which is the usual convention and avoids a background sliver between tiles.

### Top strip: 9 cells, 5 px orphan

The top bar owns the full width, `y 0 .. 44`, including the corners.

410 is not divisible by 45: `410 = 9 x 45 + 5`. The five remaining pixels sit at
`x 405 .. 409`, in the top-right chamfer, where they are invisible regardless.
That orphan is accepted — wasting edge space to keep one module everywhere is
the deliberate trade.

The chamfers take a large bite out of the outer cells. Measured visibility of
each 45 x 45 top cell:

```
cell 0   x   0 ..  44     2 %      unusable
cell 1   x  45 ..  89    69 %
cell 2   x  90 .. 134   100 %
cell 3   x 135 .. 179   100 %
cell 4   x 180 .. 224   100 %
cell 5   x 225 .. 269   100 %
cell 6   x 270 .. 314   100 %
cell 7   x 315 .. 359    61 %
cell 8   x 360 .. 404     1 %      unusable
```

So the top bar really has **five fully usable cells** (2..6) plus two partial
ones. System status belongs in cells 2..6. Cells 0, 1, 7 and 8 may be painted;
what lands there is clipped, and that is the accepted behaviour, not a fault.

### Bottom strip: 3 buttons, 57 rows

Unchanged and already implemented. Three regions at full width, painted from
panel row 445 (`FB_Y + FB_H`) to 501, with boundaries at panel `x 0..134`,
`137..270`, `273..409` and 2 px black separators. They are not on the 45 module
and do not need to be: the bottom bar is fixed chrome, not a slot grid.

## Ownership

| Region | Owner | Contents | Update rate |
|---|---|---|---|
| top, `y 0..44` | system | battery, wifi, clock, notifications | seconds |
| left, `x 0..44` | apps | 7 slots, app-chosen icons | slow |
| right, `x 365..409` | apps | 7 slots, app-chosen icons | slow |
| bottom, `y 445..501` | system | 3 buttons, press and release art | on press only |
| fb0 | console | everything else | fbcon |

The top bar owns `y 0..44` across the **full** width, including the corners the
side strips would otherwise reach. A status bar that stops short of the glass
edge looks broken; one that is clipped by the chamfer looks like the hardware it
is.

Corners are therefore unambiguous: top bar takes `y 0..44`, bottom bar takes
`y 445..501`, and the side strips are bounded to `y 97..411` by their slot grid
in any case.

## Non-interference is structural

The requirement is that edge updates never disturb the console. That is already
guaranteed and does not depend on discipline:

`co5300_blit()` sets its column window to `FB_X + x0 .. FB_X + x1` and its row
window to `FB_Y + y0 .. FB_Y + y1`. It cannot address a panel column outside
fb0. fbcon drawing into the framebuffer physically cannot touch a strip.

The converse does not hold and must be respected: an edge write **is** a panel
operation and owns the SPI bus for its duration. A large edge update can delay a
console repaint. Keep edge updates small and infrequent — which is what status
icons and slow app badges are.

## One mechanism

Everything above is one operation, specified once:

```
write a rectangle  (panel_x, panel_y, w, h)  from an RGB565 buffer
```

- Coordinates are **panel** coordinates, not framebuffer coordinates — the same
  space `co5300_paint_frame()` already uses when it draws the ring and the bars.
- Pixels are byte-swapped on the way out (`swab16`), because the panel takes the
  low byte first. A path that omits this draws nothing visible for any non-black
  colour; see LCD-003.
- The window must not be degenerate. A 1 px wide or 1 px tall window returns
  success and draws nothing (LCD-002). Edge rectangles are all far larger than
  that, but the driver should reject a degenerate request rather than silently
  accepting it.
- The whole rectangle must be one transfer. The controller treats a CS edge as
  end-of-burst (LCD-006).

Consumers of that one operation:

1. the bottom bar (exists — probe-time paint, `co5300_paint_frame()`)
2. the top status bar
3. left and right app slots
4. button press / release imagery

## Update discipline

- **Edges never touch fb0.** No filp, no mmap, no `write()` to `/dev/fb0`.
- **Slow and idempotent.** A status icon changes on battery or link state, not on
  a timer. An update that would draw the same pixels should be skipped.
- **No bus monopolising.** One edge write at a time; the console shares the bus.

## Not decided here

- ~~The API's exact shape.~~ Decided in `docs/panel-api.md`: a single ioctl on
  `/dev/fb0`, because that node already exists and a new one would need an mdev
  rule, which is exactly the `/etc` state a full-image flash destroys.
- How a slot's art is supplied and where it is stored (`/home` is writable
  jffs2; a slot config belongs there). Still open; the API document deliberately
  keeps the driver ignorant of slot semantics.
- Whether the top bar is drawn by a daemon or by the kernel. A daemon is
  consistent with `blekbd` and `tbtns`, and it is the intended shape.
- Whether partial top cells (1 and 7) are offered at all.

## Verification

A region write cannot be verified from a return code — the panel is write-only
and never acknowledges (LCD-002, LCD-003, LCD-004 all reported success while
drawing nothing). Every claim about these strips must come from looking at the
glass.

The implementation cycle should therefore start with a diagnostic that paints a
**known, structured** pattern into each region: distinct colours per slot, and a
different colour in each corner of each slot so that a shifted or clipped window
is visible. A flat fill proves nothing about addressing.

## Relationship to existing records

`.ai/core-reference.md` holds external facts — what the hardware is. This file
holds a project **design decision** about how space is allocated, which is why it
lives in `docs/`. The geometry it depends on is recorded there as LCD-007 and
LCD-004.
