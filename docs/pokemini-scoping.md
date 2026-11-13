# Pokémon Mini on the T-Watch Ultra: scoping

The third candidate platform. Unlike Game Boy Color, most of the work already
exists: the core is vendored, a reference desktop build is playable, and the
display gate that used to block this platform has since been overtaken by the
NES blit work. What follows is what is measured, what is inherited, and the one
number still missing.

**Headline: the display gate that was the stated blocker now passes with 4x
headroom, and the CPU requirement was mis-stated by a factor of 3 in the
original scope. The one remaining unknown is Xtensa throughput, and a binary
built to answer it is sitting on disk.**

---

## 1. What already exists

This is not a greenfield scope. From `~/GIT/tinytama` (2026-11-09):

- **PokeMini vendored** at revision `8a64225e`, v0.61, **GPL-3.0**, 47 files in
  `core/pokemini/`. Only one upstream file modified, by a 2-line patch that adds
  an instruction counter.
- **A playable reference desktop build.** The operator played **twelve**
  cartridges on it, including two levels of Pokémon Pinball cleared. That covers
  BIOS handover, cartridge execution, PRC rendering, display refresh and input in
  one test.
- **Cross-compiled for Xtensa and size-measured**: `114,708 text, 14,984 bss`
  with `-O2 -DPERFORMANCE`. Every vendored file compiles clean.
- **A headless host harness** that runs synthetic carts or real `.min` files, and
  a prepared **static Xtensa binary** (`for-escapement/pokemini_bench_target`,
  348 KB) built specifically to measure the watch.
- **Three scope documents**: `POKEMINI-SCOPE.md`, `POKEMINI-CORE.md`,
  `POKEMINI-DISPLAY-GATE.md`, plus `POKEMINI-DESKTOP.md`.

That is roughly a week of work already banked. By comparison the GBC port would
start from nothing.

---

## 2. Hardware

| | |
|---|---|
| CPU | Seiko Epson **S1C88** ("Minx"), 8-bit with 16-bit ops |
| Clock | 4.00 MHz, 4-cycle fetch → 1 MIPS ceiling |
| RAM | 4 KB + 256 B I/O |
| ROM | up to 2 MB cartridge |
| Display | **96×64 monochrome**, 72 Hz, driven by the PRC chip |
| Input | D-pad, A, B, C, Power, shock sensor |
| Sound | 3-channel PWM + noise |

The display is not a framebuffer: the **PRC (Program Rendering Chip)** owns it
and games talk to the PRC. PokeMini models it, so this is inherited rather than
written.

---

## 3. The CPU requirement was overstated in the original scope

`POKEMINI-SCOPE.md` states the requirement as **1 MIPS sustained** (4 MHz ÷
4-cycle fetch). That is the hardware *ceiling*, not what a game needs. A game
needs `instructions per frame × 72 Hz`, and the frame budget is only 13,889
instructions.

Measured on the host, 8,000 frames per cartridge, `-O2 -DPERFORMANCE`:

| cartridge | interpreter | instructions/frame | of the 1 MIPS ceiling | **actually needed** |
|---|---|---|---|---|
| Pokémon Party Mini | 133.1 Mips | 5,053 | 36.4% | **363,816 ips** |
| Pokémon Zany Cards | 135.6 Mips | 4,562 | 32.8% | 328,464 ips |
| Pokémon Pinball Mini | 68.8 Mips | 3,574 | 25.7% | 257,328 ips |
| Pokémon Puzzle Collection | 65.4 Mips | 3,349 | 24.1% | 241,128 ips |
| synthetic (random opcodes) | 82.0 Mips | 3,302 | 23.8% | 237,744 ips |
| Pokémon Tetris | 65.9 Mips | 3,102 | 22.3% | 223,344 ips |
| Lunch Time / Snorlax's | 63.2 Mips | 3,091 | 22.3% | 222,552 ips |

So the target is **0.22–0.36 MIPS, not 1 MIPS** — three times less demanding than
the original document assumed. This matters because it is the difference between
"needs a fast core" and "needs a modest one".

Note the honest caveat the core document already makes: these are retail
cartridges running their *attract modes*, not played sessions. Input-driven
scenes are heavier. The 13,889 ceiling is hard by definition, so treat 36% as the
measured floor of demand rather than a bound.

---

## 4. The display gate now passes with 4x headroom

This is the material change since the platform was scoped. `POKEMINI-DISPLAY-
GATE.md` concluded that the panel was the binding constraint:

```
288x192 full frame:  19.7 ms  =  51 fps
fit: 11.42 us per 64-byte chunk  ->  357 ns per pixel
```

That measurement was made through **64-byte chunks**. The NES blit work of
2026-11-12/13 rebuilt that path around 16,384-pixel transactions and measured
**66.455 ns per pixel** — **5.4x cheaper**, verified on the glass at 51 fps.

Against a 72 Hz frame budget of 13,889 µs:

| zoom | pixels | old gate | our path | headroom |
|---|---|---|---|---|
| 1x 96×64 | 6,144 | 2,193 µs (456 fps) | **408 µs (2,449 fps)** | 34x |
| 2x 192×128 | 24,576 | 8,774 µs (114 fps) | **1,633 µs (612 fps)** | 8.5x |
| **3x 288×192** | 55,296 | 19,741 µs (**51 fps**) | **3,675 µs (272 fps)** | **3.8x** |

At 3x the push is **26% of a 72 Hz frame**, and with the core-1 blit already
built, emulation and push overlap. **The gate that was called the risk is now
the comfortable part.** The four conclusions the original gate document reached
still hold — per-transfer fixed cost is zero, DMA caps at ~2x, and dirty
rectangles are unnecessary — they simply were not needed once the transfer got
5.4x cheaper.

---

## 5. The honest picture problem

This is the one place Pokémon Mini scores worse than Game Boy Color, and it
should be stated plainly rather than buried.

The Mini is **96×64, a 3:2 landscape screen**, on a **320×416 portrait** panel.

| | pixels | share of 320x416 | fills width? |
|---|---|---|---|
| PM at 3x (max integer) | 288×192 = 55,296 | 41.5% | 90% |
| GBC at 2x | 320×288 = 92,160 | 69.2% | 100% |
| NES at 1x (parked) | 256×240 = 61,440 | 46.2% | 80% |

So **PM at 3x is smaller than the NES the operator just parked as too small**,
and much smaller than GBC. It wins on width and loses badly on height — 112 rows
of letterbox above and below. The aspect ratios are simply mismatched: a 3:2
game on a 0.77:1 screen.

This is not a reason to reject the platform — the games are different and the
port is cheap — but nobody should start it expecting a big picture.

---

## 6. What is still unknown

**Xtensa interpreter throughput on the watch. Everything else is measured.**

That single number decides the platform, and it needs to be ~0.25 Mips or better
for the lightest cartridges and ~0.4 Mips for the heaviest.

### The prepared binary cannot run on the watch as it is configured

The obvious route — copy `for-escapement/pokemini_bench_target` to the watch and
run it — **does not work, and the reason is not fixable by retrying.** That binary
is a **Linux** ELF: built by Escapement's own crosstool-NG (GCC 14.1.0) against
uClibc, with `_start` and `__libc_start_main` and no ESP-IDF runtime. The watch as
it stands runs **ESP-IDF v5.3.1 with TinyDesk Shell**, which is not Linux and has
no ELF loader — `tdsh` reports `command not found` and has no `chmod` or `exec`.

Attempted on 2026-11-13: WiFi on, `ssh start`, SFTP to `/sd` (392,204 bytes at
18.1 KiB/s, md5 matching), `chmod` absent, execution refused. File removed, radio
off again. The `for-escapement/` directory name was literal — it was built for
the Linux watch, not for this firmware.

So the measurement needs one of two routes, and they are not equivalent:

**A. Flash the Escapement Linux image and run it there.** This is what the binary
was built for and it needs no new code. It also replaces the TinyDesk firmware,
which is the platform the NES and GBC work lives on. Reversible: a verified 16 MiB
backup of the current firmware exists (`working-2026-11-13-0019.bin`). The number
this produces is the frame rate of Pokémon Mini **on Linux**, which is a different
question from the one the NES and GBC scopes answered.

**B. Build the core into TinyDesk as a `pmbench` command.** This costs real work —
vendoring `core/pokemini/` as a component and writing a shell command — but it
measures the platform the other emulator work actually targets, and it is step 1
of that port rather than a detour around it. Sizing is comfortable: 114,708 bytes
of text against 347 KB free in the app partition, and 14,984 bytes of bss against
143 KB of free internal DRAM.

**Recommend B**, because NES and GBC are both TinyDesk ports. A Pokémon Mini that
runs on a different operating system would be a third parallel platform, and the
recommendation would then depend on which OS the next session happens to have
flashed.

**Do not estimate this by scaling the host figure.** `POKEMINI-CORE.md` says so
directly — "the host figure cannot be scaled to the target by a ratio — any such
ratio would be invented". An attempt while writing this document produced
anything from 0.3 to 1.3 Mips depending on which anchor was chosen, which is the
same non-answer in three costumes.

## 7. Recommendation

**Run the prepared benchmark before deciding anything else.** It is one file
copy and one command, and it converts the last unknown into a number.

If it clears ~0.4 Mips, the platform is straightforward:

1. **Vendor `core/pokemini/` into the fork** as `components/pokemini`, GPL-3.0
   with attribution. 114 KB of text fits the app partition's 347 KB of free space
   with 225 KB to spare, so there is no size risk.
2. **Reuse the display path unchanged.** `display_picture_rows()` and the core-1
   blit already exist; a monochrome 96×64 frame needs a 2-entry palette, so the
   conversion is even cheaper than the NES's 256-entry one.
3. **3x integer scale**, 288×192, accepting the letterbox.
4. **Audio last**, as with every other platform here.
5. **The shock sensor** has no watch equivalent; a screen-tap gesture is the
   natural stand-in and is already modelled — the touch panel supports multi-touch
   gestures since the NES margin work.

If it lands below ~0.25 Mips, the platform does not ship and we say so rather
than porting something that runs slow. Either way the answer costs one transfer
and one command.

### Why this over GBC

Both are worth having and they are not substitutes — different games, different
feel. But as ports:

- **PM has a week of work banked** (core vendored, desktop reference playable on
  twelve cartridges, size measured, bench harness written, display gate already
  investigated). GBC starts from nothing.
- **PM's display cost is trivial**; GBC's is 1.5x the NES's because 2x upscaling
  sends 92,160 pixels.
- **GBC has the better picture**, and it is the one the operator asked for on
  those grounds.

If the goal is "a platform we can ship soon", PM. If the goal is "the glass
filled with the game", GBC.
