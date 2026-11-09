# Panel API — writing the reserved strips

Status: **specification, not implemented.** Agreed 2026-11-09. This decides the
one question `docs/panel-reservations.md` deliberately left open: how userspace
puts pixels into the strips around fb0.

Read `docs/panel-reservations.md` first for the geometry. This document only
adds the calling convention.

## The decision

**One ioctl on `/dev/fb0`.**

Not a new character device, not sysfs, not debugfs. Reasons, in order of weight:

1. `/dev/fb0` already exists and is already openable by the programs that need
   this. A new node means a mdev rule, and mdev rules are exactly the kind of
   `/etc` state that a full-image flash destroys (`FLS-003`).
2. It is the display's own interface. The strips are part of the display, not a
   separate device that happens to share a panel.
3. The fbdev core forwards unrecognised ioctls to the driver's `fb_ioctl`
   (`drivers/video/fbdev/core/fb_chrdev.c:148`), so this needs no core change.

```
#define CO5300_IOC_MAGIC 'C'
```

`'C'` and `'c'` are both unused in this kernel tree (checked), so there is no
collision with any other built-in driver.

## The call

```c
struct co5300_rect {
	__u16 x, y;      /* top-left, PANEL coordinates */
	__u16 w, h;      /* size in pixels, w >= 2 and h >= 2 */
	__u16 stride;    /* pixels per row of the source, 0 means w */
	__u16 reserved;  /* must be 0 */
	__u32 flags;     /* must be 0 */
	__u64 pixels;    /* user pointer, __u16 RGB565, host byte order */
};

#define CO5300_IOC_WRITE_RECT _IOW(CO5300_IOC_MAGIC, 1, struct co5300_rect)
```

```c
struct co5300_geom {
	__u16 panel_w, panel_h;   /* 410, 502 */
	__u16 fb_x, fb_y;         /* 45, 45 */
	__u16 fb_w, fb_h;         /* 320, 400 */
	__u16 offset_x;           /* 22, the controller's column offset */
	__u16 version;            /* CO5300_ABI_VERSION */
	__u32 reserved;
};

#define CO5300_IOC_GET_GEOM    _IOR(CO5300_IOC_MAGIC, 2, struct co5300_geom)
#define CO5300_ABI_VERSION     1
```

`GET_GEOM` exists so no userspace program hardcodes 45 or 410. If the panel or
fb0 geometry ever moves, one query follows it. It also gives a feature test:
an older kernel returns `-ENOTTY`.

## Semantics

### Coordinates are panel coordinates

`x` and `y` are positions on the glass, exactly the space
`co5300_paint_frame()` already uses when it draws the ring and the bars. Not
framebuffer coordinates — nothing here is relative to fb0.

The driver adds `PANEL_OFFSET_X` (22) to the column addresses internally,
because the controller's RAM is wider than the visible glass. Callers never see
that offset, and are never expected to.

### The write is confined to the reserved strips

**The driver rejects any rectangle that intersects fb0.** This is the mechanism
behind "edges never disturb the console", and it belongs in the driver rather
than in a convention: a userspace bug must not be able to paint over the
console, and the check is four comparisons.

This is why `GET_GEOM` returns the fb0 rectangle: a caller can tell, before
calling, whether what it intends is legal.

### One rectangle is one address window, sent in short bands

Each call opens its own address window and streams the rectangle before
releasing CS (`LCD-006`: a CS edge ends the burst, and `spi-mem` will not
continue a pixel stream across calls).

Every rectangle is split into bands of **two rows**, regardless of buffer space,
because longer transfers tear on this hardware — see "Transfer size" below. The
split is invisible to the caller: userspace hands over one rectangle and one
buffer, and the driver issues the bands.

### Pixels are RGB565 in host byte order

The caller passes normal `__u16` RGB565 values. The driver byte-swaps on the way
out (`swab16`), because the panel takes the low byte first (`LCD-003`).

This is a deliberate placement of responsibility. The alternative — requiring
callers to pre-swap — has already produced a bug in this project: a fill path
that omitted the swap drew nothing, and it looked correct for as long as it was
only ever asked to draw black.

### Degenerate rectangles are refused

`w < 2` or `h < 2` returns `-EINVAL` rather than drawing nothing (`LCD-002`: a
window one pixel wide or tall is not honoured and reports success). Failing
loudly at the boundary is worth more than accepting a request that silently does
nothing.

### `stride` allows a sub-rectangle of a larger image

`stride` is the source's row pitch in pixels; `0` means `w`. This lets a caller
keep one sprite sheet in memory and blit a tile out of it without repacking.
The first pixel of the rectangle is `pixels`, and row *n* begins at
`pixels + n * stride`.

## Errors

| errno | Meaning |
|---|---|
| `-ENOTTY` | not a `CO5300_IOC_*` command |
| `-EINVAL` | degenerate `w`/`h`, `stride < w`, non-zero `reserved` or `flags`, out-of-panel bounds, or overlap with fb0 |
| `-EFAULT` | `pixels` is not readable for `h * stride` elements |
| `-ENOMEM` | could not allocate a temporary buffer |
| `-EIO` | the SPI transfer failed |

`-EINVAL` and `-EIO` are distinguishable on purpose: a rejected request is a
caller bug, a failed transfer is the display path.

## Locking

The panel has no memory of its own and takes no commands out of order. Two
concurrent panel operations would interleave a window setup with another
window's pixel stream.

So the driver gains **one mutex**, taken at the top of every entry point that
touches the panel:

- `co5300_fillrect`, `co5300_copyarea`, `co5300_imageblit`, `co5300_write`
- the new `co5300_ioctl`

Taken at the entry points, not inside `co5300_blit` or the transfer helpers, so
that a multi-band operation is one critical section and cannot be relocked
underneath itself.

fbcon already serialises its own calls with `console_lock`; this mutex is what
serialises fbcon against userspace. The ioctl deliberately does **not** take
`console_lock` — doing so from an ioctl risks a class of deadlock for no
additional safety.

## Concurrency with the console: what the caller owes

The driver serialises. It does not schedule. An edge write holds the bus for its
duration, so during a large update the console's next repaint waits.

Keep edge updates small and infrequent. A 45x45 tile is 4050 bytes and costs
about 3 ms at the rate measured in `LCD-005` (190 ms for a 256000-byte frame,
which is 742 ns per byte). For comparison, a full-width 45-row band is about
29 ms and the whole bottom strip about 37 ms. This is a design constraint on the
daemons, not something the driver can enforce.

## What this API does not do

Deliberately out of scope, so the primitive stays one primitive:

- **No readback.** The panel is write-only and never acknowledges. There is
  nothing to read.
- **No slot semantics.** The driver does not know what a "slot" is, does not
  store icons, and does not track which slot holds what. Rectangles only.
- **No art management.** Where icons live and how they are loaded is a
  userspace concern; `/home` is writable jffs2 and is the obvious home.
- **No dimming, no alpha, no partial updates.** RGB565 rectangles.
- **No press/release handling.** `tbtns` already owns reading the buttons; it
  gets the new paint primitive for free once this exists.
- **No double buffering.** Every call is a direct panel write.

## Transfer size: a cap that works, and a cause that is unknown

`CO5300_IOC_WRITE_RECT` sends every rectangle in bands of **two rows**, even
though the transmit buffer could hold a whole frame.

That is not caution. A single 45x356 transfer into the right-hand strip comes
back as eight diagonal splits, worst at the bottom, and the splits show even in
a uniform fill — so whole rows did not receive the write rather than the panel
being caught mid-scan. The same rectangle sent as 178 windows of 45x2 is clean,
repeatedly.

What was ruled out, one variable per test pattern, all measured on the glass:

```
width       not it -- non-monotonic. From one origin, stacked:
            45 wide torn, 35 wide CLEAN, 25, 15 and 5 wide all torn
X position  not it -- nine 45x50 windows swept across the bottom strip at
            x = 0..360 were all clean, including controller columns 382..426,
            which overlap the right strip's 387..431
height      not it -- the 45x71 segments tore as well
residue     not it -- the splits are visible in a single solid fill
```

And the left strip tolerates the same 45x356 single transfer that the right one
does not.

**The root cause is not established.** This is a workaround that has held every
time it was tried, not an explanation. It is acceptable here because these are
status icons that change on battery or link state; it would be wrong for
continuous drawing, which is why `co5300_blit()` — fbcon's path — keeps its own
uncapped sizing.

The strongest remaining suspect is **SPI clock margin**. `NOTES.md` in
`patches/kernel/display/` records the identical signature at 45 MHz in the
reference firmware — "dark horizontal lines shearing slightly down to the
right", cured by dropping to 20 MHz. We run at 20 MHz and the same signature is
still marginal for long narrow transfers, which suggests 20 MHz is closer to the
edge than believed. That is one experiment worth running before the mechanism
is declared mysterious.

## Implementation risk, stated plainly

**Resolved: 45x45 windows are honoured.** This section originally said the shape
was untested, because `LCD-002` only established that a 1 px window draws
nothing while a 432x31 strip draws. Implementation measured it: 45x45, 45x315,
410x45 and 410x57 all draw correctly, verified on the glass. The full-width band
fallback below is therefore not needed, and the reserved regions can be written
directly. It is kept documented because it remains the answer if a future panel
revision behaves differently.

The reverse risk did materialise, in the test tool rather than the driver: a
pattern whose "white" markers were `RGB565(31, 31, 31)` drew pink, because green
is a 6-bit field and 31 is half brightness. That was read as a driver fault on
the glass before the arithmetic was checked. Any colour constant in a drawing
test must be checked against the field widths first (`LCD-008`).

Fallbacks if narrow windows ever misbehave, in order of preference:

1. **Full-width band composition.** Build the band in RAM from `p->fb` plus the
   tile, so every transfer is the 432-wide shape that is already proven. Costs
   about 29 ms instead of 3 ms per 45-row band, which is irrelevant for icons
   that change on battery or link state.
2. **Restrict to rows outside fb0.** Top and bottom strips span no fb0 rows at
   all, so a band there needs no fb0 composition.

The implementation cycle must therefore start by painting a **structured** test
pattern — a distinct colour per tile, and a different colour in each corner of
each tile, so a shifted or clipped window is visible. A flat fill proves nothing
(`LCD-003`, `LCD-004`, and three earlier drawing paths that reported success
while drawing nothing).

## Sequence of work

1. Add the mutex and the two ioctls to `co5300.c`. No behaviour change to fbcon.
2. A diagnostic that paints a corner-marked pattern into every reserved region.
   Look at the glass. This is the gate for everything after it.
3. A status daemon for the top bar, reading `/sys/class/power_supply` and the
   WiFi state, drawing into the five fully usable cells.
4. Slot rendering for the side strips, driven by a config on `/home`.
5. Button press/release art, extending `tbtns` to repaint the bottom bar.

## Verification

Two layers, and they are not interchangeable:

- **Host**: the argument validation (bounds, fb0 overlap, degenerate size,
  stride) is pure arithmetic and belongs in a selftest beside
  `scripts/tbtns-selftest.py`, so a boundary error does not cost a flash.
- **Hardware**: whether the rectangle actually landed on the glass. Only the
  glass can answer that.

## Relationship to the other documents

- `docs/panel-reservations.md` — what the space is and who owns it.
- `docs/panel-geometry.md` — how the numbers were measured.
- `.ai/core-reference.md` — the hardware facts both depend on: `LCD-002`,
  `LCD-003`, `LCD-004`, `LCD-006`, `LCD-007`.
