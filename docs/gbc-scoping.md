# Game Boy Color on the T-Watch Ultra: scoping

A measurement, not a reading of source files. Two cores were built and run
headless over four commercial ROMs; the screen-fit arithmetic and the watch
projection are computed from numbers this project already measured on the
hardware. Everything here is reproducible from `scripts/gbcbench.sh`.

**The headline is not what we expected:** the picture gets much better and the
frame rate probably gets slightly worse.

---

## 1. Why GBC at all

The NES is parked because 256x240 is too small for a 320x416 glass. The NES
renders at 1x (integer scaling is not available: 256 x 1.25 = 320, so any larger
integer scale overflows) and occupies 46% of the framebuffer, with 32-pixel
side margins.

The Game Boy's 160x144 is a *lower* resolution that fits this screen *better*,
because 160 divides 320 exactly:

| | pixels | scale | on screen | share of fb |
|---|---|---|---|---|
| NES | 256x240 | 1x | 256 wide | 46.2% |
| GBC | 160x144 | 1x | 160 wide | 17.3% |
| **GBC** | **160x144** | **2x** | **320x288** | **69.2%** |

At 2x the Game Boy fills the width exactly — no side margins at all — and 69% of
the framebuffer, with 64 rows of letterbox above and below. That is the reason to
want this port: a picture that uses the glass instead of floating in it.

3x does not fit (480x432 against a 320x416 framebuffer).

---

## 2. The candidates

| core | language | licence | GBC? | notes |
|---|---|---|---|---|
| **gnuboy** (retro-go fork) | C | GPL-2.0 | **yes**, `GB_HW_CGB` | 4,996 lines. Already vendored by the same tree the NES core came from, at the same pin. |
| **binjgb** | C | MIT | yes | 10,477 lines. Accuracy-first; passes suites gnuboy does not. RGBA output only. |
| SameBoy | C | Expat | yes | The accuracy reference. Much larger; not built here. |
| Gearboy | C++ | GPL-3.0 | yes | C++ front end; heavier to integrate into this C firmware. |
| gambatte (libretro) | C++ | GPL-2.0 | yes | Needs `libretro.h` and a front end, as FCEUmm/QuickNES did for NES. |
| Peanut-GB | C | — | **NO** | **DMG only.** Its speed reputation does not apply to Color. Rules it out. |
| gnuboy (upstream) | C | GPL-2.0 | some | retro-go's fork is the one already in this project's supply chain. |

Two things worth recording because they were assumptions that did not survive
contact:

- **Peanut-GB is the fastest portable Game Boy core and it is DMG-only.** Its
  README says CGB support is "preliminary work" in an issue, not in the tree. It
  is the obvious first choice by reputation and it is the wrong choice for this
  port.
- **The licence question does not discriminate here.** The operator is flexible
  on licence and cares about compatibility and performance, so GPL-2.0 (gnuboy)
  is acceptable — which matters, because it also happens to be the fastest.

---

## 3. Measured speed

Host: this machine, x86-64. Harness: `tools/gbcbench/`, headless, no audio, no
sleeping. 1,500 frames per run, best of 3 (the machine has ~7-12% run-to-run
noise, which is larger than any difference the harness would otherwise report).

| ROM | CGB | mapper | gnuboy fps | binjgb fps | gnuboy advantage |
|---|---|---|---|---|---|
| Zelda: Link's Awakening DX | 0x80 compatible | MBC5 | 9,308 | 5,684 | 1.64x |
| Pokemon Crystal | 0xC0 only | MBC3+RTC | 13,560 | 7,578 | 1.79x |
| Super Mario Bros. Deluxe | 0xC0 only | MBC5 | 10,989 | 6,683 | 1.64x |
| Tetris DX | 0x80 compatible | MBC1 | 13,620 | 8,848 | 1.54x |

**gnuboy is consistently 1.54-1.79x faster than binjgb** across CGB-only and
CGB-compatible titles and three different mappers. It also emits `GB_PIXEL_565_BE`
— 16-bit colour with the high byte first — which is exactly the watch panel's
byte order, so a gnuboy port needs **no per-pixel conversion at all**. binjgb
only produces RGBA, and its figures above already include the RGBA→RGB565
conversion a port would have to write.

### Harness traps found while building it

Recorded because each produced a plausible number that was wrong, which is the
same class of failure `nesbench.sh` documents four of:

1. **Omitting `gnuboy_reset()` after loading** segfaults inside the CPU. gnuboy
   supports having no boot ROM — `gb_cpu_reset()` sets `PC = GB.bios ? 0 : 0x100`
   — but that decision is only made *in* the reset. Without it PC stays 0 and the
   first fetch dereferences a NULL BIOS pointer.
2. **Running the diagnostic scan on every frame** destroyed the format
   comparison: a 23,040-iteration loop cost more than the 256-entry palette
   conversion it was meant to measure, so 565BE and PAL came out within noise of
   each other and "the format does not matter" would have been an artefact. The
   scan now runs on one frame.
3. **Freeing the ROM buffer after `emulator_delete()`** double-freed and crashed
   *after* the numbers had been computed — the worst shape, because the result
   prints and only the exit status is wrong.
4. **The noise floor is 7-12%**, larger than the differences being compared. Every
   figure above is best-of-3 for that reason, and a single run would not have
   justified any conclusion.

---

## 4. Projection onto the watch

This is arithmetic from measured watch numbers, not a measurement. It should be
treated as an estimate with a stated range, and the only way to settle it is to
port the core and run it.

**Emulation cost.** The NES port's interpreter is 15,242 µs of a 16,639 µs frame,
so the watch sustains ~1.95 Mcyc/s of 6502 cycles. A GBC needs 4.194 Mcyc/s of Z80
cycles. Two normalisations bracket the answer:

- by **instructions** (Z80 ~4 cycles/instr, 6502 ~2.5): GBC needs 1.46x the
  instruction throughput → **22,253 µs/frame**
- by **cycles**: GBC needs 2.34x → **35,666 µs/frame**

**Blit cost.** Measured NES push is 4,083 µs for 61,440 pixels = 66.5 ns/pixel,
and it scales with pixel count. GBC at 2x is 92,160 pixels → **~6,124 µs**, with
no conversion. Note that 2x upscaling puts the pixel count *above* the NES's
1.5x, because "fewer source pixels" and "fewer transmitted pixels" are different
questions.

**Frame time**, using the core-1 overlap already built (frame = max(emu, blit)):

| normalisation | emulation | blit | frame | fps |
|---|---|---|---|---|
| instructions | 22,253 µs | 6,124 µs | 22,253 µs | **45** |
| cycles | 35,666 µs | 6,124 µs | 35,666 µs | **28** |

**Against the NES's current 51 fps.** The Game Boy's CPU is far more expensive to
emulate than the NES's — 2.34x the clock — and 2x upscaling puts the pixel count
back above the NES's. The blit side gets much cheaper (10,272 → ~6,124 µs, no
palette conversion), but emulation dominates, so **the likely outcome is 28-45
fps, i.e. somewhat slower than the NES runs today.**

---

## 5. What this means for the decision

The honest trade:

- **The picture is unambiguously better.** 320x288 filling the full width and 69%
  of the framebuffer, versus 256x240 in 46% with side margins. That is the
  operator's actual complaint about the NES, and GBC fixes it exactly.
- **The frame rate probably gets worse, not better.** 28-45 fps estimated against
  51 measured. The Game Boy is a faster machine than the NES and costs
  proportionally more to emulate.
- **The port is smaller.** gnuboy is 4,996 lines against nofrendo's larger tree,
  its API is far cleaner (init / load / reset / run, six calls), it emits pixels in
  the panel's own byte order, and the display driver does not need to change at
  all — `display_picture_rows()` and the core-1 blit already exist and were built
  for exactly this.

So GBC is worth doing for the picture, not for the speed. If the operator wants
both, the lever is the emulator: 28-45 fps means the Z80 interpreter is the
bottleneck, and the NES cycle shows that display work is already near its floor.

### Recommended next steps, in order

1. **Port gnuboy, not binjgb.** 1.6x faster, no pixel conversion, cleaner API,
   and already in this project's supply chain at a pin we control. Keep binjgb as
   the accuracy reference if a game misbehaves.
2. **Measure on the watch before optimising anything.** The 28-45 fps band is wide
   enough that the first hardware run decides the plan: at 45 there is nothing to
   do, at 28 the interpreter needs work.
3. **Use 2x scale from the first build.** 1x is worse than the NES — 17% of the
   framebuffer — and would not answer the operator's complaint.
4. **Reuse the display path unchanged.** `display_picture_rows()` takes a row
   callback; gnuboy renders internally, so the port converts its frame into the
   transmit band through the same entry point the NES uses. No driver work.
5. **Audio last**, as with the NES. gnuboy's APU needs an external
   implementation (`minigb_apu` is the usual one); the NES port has no audio
   either, so this is not a regression.
