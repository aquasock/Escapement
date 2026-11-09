# Usable glass area — measured on hardware, 2026-11-08

The AMOLED is not a plain rectangle. Its four corners are cut, and until now
nobody had measured how much of the glass can actually be used. Everything here
was read off the panel itself; nothing is inferred from a datasheet.

## The answer

```
panel           410 x 502 px
inset           45 px on every side
usable area     320 x 412 px
corners         45-degree chamfers, leg ~85-90 px
```

## The corners are chamfers, not fillets

Worth stating plainly, because a radius model wasted two rounds of flashing:
these are **straight 45-degree cuts**, not circular corners. Fitting
`d = R(1 - 1/sqrt2)` to the measurement gives R > 400 px for this glass, which
is not physical — the formula simply does not describe the part.

For a 45-degree chamfer the inset law is `d = L/2`, where L is the leg measured
along each edge. Measured from where grid lines terminate:

```
top-left      x + y       = 80
top-right    (410-x) + y  = 90
bottom-left   x + (502-y) = 82
```

A 7x7 marker centred on the chamfer midpoint (leg/2) still clips slightly, so
the true leg is a little over 85. Symmetry across three corners is consistent;
the bottom-right reading was unreliable in the photograph and is assumed to
match.

## How it was measured

1. A full-panel grid — green lines every 20 px, red every 100 px — drawn edge to
   edge. Where the grid stops tells you the extent directly.
2. A photograph of the glass, with the red decade lines used as fiducials to fit
   a homography from panel space to photo space. Perspective across the frame is
   real: line spacing drifts 140 -> 130 px horizontally and 128 -> 145 px
   vertically, so a linear fit is not good enough.
3. The four corners cropped at full resolution and read directly.
4. Confirmation: a predicted outline — straight edges at inset 45, chamfers of
   leg 85, with markers at each chamfer midpoint — drawn and compared against the
   glass. **All four straight edges render completely; the corner markers still
   clip slightly.** That is the numbered result above.

## What fits

At the native cell of Tiny5 Mono (6 wide x 9 tall):

```
53 cols x 45 rows
53 * 6 = 318 px  (2 px spare of 320)
45 * 9 = 405 px  (7 px spare of 412)
```

3120 characters, no corner clipping.

For comparison, using the full glass (410x502) would give 68 x 55, but the four
extreme corner cells then sit inside the chamfer. That is only worth doing if
something later wants the extra 15 columns, and it costs four cells that a
terminal rarely reads.

## Caveats

  * The grid's first row and column (panel x=0, y=0) did not appear in the
    photograph while everything from 20 px inward did. That may just be the
    photograph, or it may be an addressing offset — the driver windows the glass
    at `PANEL_OFFSET_X = 22` because the controller's addressable area is wider
    than the visible one. Unresolved, and it does not affect the numbers above
    because they are measured from the visible glass inward.
  * The measurement is a photograph of an AMOLED through a camera. It is good to
    a few pixels, not to one.

## The console uses 320x400 of it

fbcon draws 40 columns by 25 rows at the VGA 8x16 cell (8 px wide in a 9 px
cell):

```
40 * 8  = 320 px   (fills the width exactly)
25 * 16 = 400 px   (12 px spare of 412)
```

The framebuffer is therefore 320x400, not 320x412, placed at (45, 46). Two
reasons: the round 400 leaves the vertical centring exact, and making fb0 the
console area rather than the whole glass means fbcon does the arithmetic and
the border strips stay outside the framebuffer, free for notifications.

The five fonts the kernel ships and what each would give here:

```
VGA8x16      8x16   ->  40 cols x 25 rows   <- chosen; the classic
6x10         6x10   ->  53 cols x 41 rows
6x8          6x8    ->  53 cols x 51 rows
ProFont6x11  6x11   ->  53 cols x 37 rows
7x14         7x14   ->  45 cols x 29 rows
MINI4x6      4x6    ->  80 cols x 68 rows   <- the only 80-column option
```

MINI4x6 reaches 80 columns (4 * 80 = 320) but 4 px glyphs are rough to read on
a 2-inch panel. 53 columns is the ceiling for any 6 px font, since 320 / 6 = 53
and the glass is 410 wide at most.
