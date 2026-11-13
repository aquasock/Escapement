# Pokémon Mini on the T-Watch Ultra: scoping

The third candidate platform. Unlike Game Boy Color, most of the work already
exists: the core is vendored, a reference desktop build is playable, and the
display gate that used to block this platform has since been overtaken by the
NES blit work. What follows is what is measured, what is inherited, and what the
watch itself reported when asked.

**Headline: eleven of the twelve cartridges run at 0.998-0.999x of real time on
the watch, and one does not. Pokemon Pinball Mini plays at 71.9 fps against the
machine's 71.97 Hz tick with the display pushed from the second core; Pokemon
Puzzle Collection drifts to 0.971x because its instructions are the most
expensive in the set at 3.34 us each. The platform works and is not uniformly at
full speed, and the difference is the interpreter's time per instruction rather
than how much work a game asks for.**

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

## 6. The measurement, taken on the watch

**The unknown is closed: the watch retires 383,000 S1C88 instructions per second
while emulating a Pokemon Mini, which is 1.61x realtime on a worst-case
cartridge and 1.06x the heaviest requirement in the band.**

Route B was built and run. The prepared Linux binary described below was never
usable, and nothing was gained by trying harder with it; what answered the
question was `components/pokemini` (the core vendored into the fork as a
component) plus `main/pmbench.c` (a shell command). Both are in
`~/GIT/tinydesk-twatch` at `ports/twatchultra/`.

### What was measured

Run on 2026-11-13 on the watch itself, with the radio off, on core 0 at priority
5 — the same core the NES port emulates on, so the figures are comparable.

- 2,000 frames of 55,555 emulated cycles each, i.e. 27.8 seconds of emulated
  Pokemon Mini time per run.
- A 32 KB cartridge generated in memory from a fixed xorshift32 seed, which is
  the largest a PM cartridge gets and therefore the worst case for the ROM read
  path. No commercial ROM is used or distributed.
- Four runs: **382,982 / 382,951 / 383,075 / 383,102 instructions per second**.
  17.24 s each. The spread is 0.04%, so the noise that makes single runs
  untrustworthy elsewhere is not present here.

### The cartridge must live in PSRAM, and the board says so

`pmbench` measures the cartridge in internal SRAM and in PSRAM because it does
not copy the image — `PokeMini_SetMINMem()` keeps the caller's pointer, and the
S1C88 fetches instructions out of the cartridge, so where it lives dominates the
result. The internal case was **refused for lack of a contiguous block**:

```
Internal RAM: free=117475, largest=31744, minimum-free=100591 bytes
```

31,744 bytes is the largest free block against a 32,768-byte request: it misses
by exactly 1 KB with 117 KB free in total. So a maximally-sized cartridge cannot
be internal on this board, which settles the choice — a real port puts the ROM in
PSRAM, and the 383,000 figure is that configuration rather than a pessimistic
one that could be better. It also means the internal-SRAM-versus-PSRAM penalty is
not measurable at 32 KB and is left unmeasured rather than guessed.

### Measured on a real cartridge: the port reaches full speed

Pokemon Pinball Mini (USA, Europe) was run on the watch with a retail BIOS dump.
The measurements below are in the order they were taken, because each one found
the next problem and the last one was not where the port was looking.

- **Emulation alone, unpaced: 82.4 fps** (`pm bench`), i.e. 12.1 ms of a 13.9 ms
  frame, 3,809 instructions per frame at 3.19 us each.
- **With the display, sequentially: 54.3 fps**, from emulation 13.0 ms + blit
  5.4 ms = 18.4 ms. The blit had been 9.9 ms until the row expansion was cached:
  a 3x vertical scale reads each source row three times, and the first callback
  re-derived the source column per destination pixel, so a 6,144-dot picture cost
  55,296 divisions a frame. Caching one expanded row took 9.9 ms to 5.3 ms.
- **Final: 0.999x of real time**, 71.9 fps against the machine's 71.97 Hz tick,
  `missed=0` over 6,295 frames and 2 missed ticks in 20,249 over 281 s. Per frame:
  emulation 13.2 ms overlapped with a 5.6 ms core-1 push, 13.2 ms against a
  13.89 ms budget.

**The caveat in the previous version of this section was in the wrong
direction.** It guessed that real cartridges would be cheaper per instruction
than the synthetic mix, because random 12-bit words hit disproportionately many
slow paths. They are not: synthetic instructions measure 2.61 us and Pinball's
measure 3.19 us, 22 percent dearer. So the synthetic figure is optimistic per
instruction, and the real margins are these.

**Three changes produced the last 30 percent, and the third was not the one
that was planned.**

Overlapping the blit onto core 1 was the planned change and it moved 0.765x to
0.983x. It costs what it costs: emulation rose from 12.8 to 13.8 ms because the
pusher and the cartridge now share the PSRAM bus, which is the same contention
the NES port measured. Paying 1.0 ms to remove 5.6 ms from the critical path is
still the right trade.

Skipping frames identical to the one already pushed removed about half the
pushes -- `pushes=3070 identical=3225` -- because Pinball draws at the PRC's
rate, once every 2 ticks, and the loop was pushing on every tick. A 6 KB
`memcmp` against 5.7 ms of SPI, and it is also what the real panel does: it only
changes when the PRC copies.

**The pacing was the remaining cap, and it read as an emulator fault.** After
those two changes the frame cost 13.2 ms against a 13.89 ms budget, yet the
measured speed was still 0.981x. The tell was `missed=33` of 1,837 ticks -- 1.8
percent, matching the shortfall exactly. A binary semaphore holds one pending
give, so when emulation overran slightly the next two ticks collapsed into one
and the second was lost for good: every overrun silently deleted emulated time.
A counting semaphore with a depth of 8 lets the loop drain a backlog and catch
up, and the figure went to 0.996x and then 0.999x. Nothing in the emulator
changed.

### What the machine's frame rate actually is

**The Pokemon Mini is not a 60 Hz machine and its picture rate is not its tick
rate.** Three rates matter and only the first is fixed:

- **The tick is 71.97 Hz.** The PRC counter runs `0x01000000` to `0x42000000`, a
  span of 65 x 2^24, advancing 2^24/855 per CPU cycle, so a frame is exactly
  65 x 855 = 55,575 cycles. At 4 MHz that is 71.9748 Hz. The core's own function
  is named `MinxPRC_On72HzRefresh`; the 72 is a rounded label on a 71.97 Hz
  event. An earlier version of this port used 4e6/72 = 55,555 cycles, which ran
  the machine 0.036 percent fast.
- **A cartridge draws at the tick divided by 2 to 12**, chosen through PRC rate
  control (register 0x81). The match value is `(divisor-1) << 4`, so the divisor
  is `match >> 4 + 1`, and **there is no divisor of 1** -- the fastest a game can
  draw is 36 Hz. Measured for Pinball: `prc rate reg 0x19, match 0x10 -> draws
  every 2 ticks = 36.0 Hz`.
- **Wikipedia's infobox says 30 fps**, citing a 2001 IGN preview rather than a
  datasheet. It matches neither 71.97 nor 36 and is not used here. The technical
  source (`pokemon-mini.net/documentation/prc/`) was unreachable during this
  work -- Cloudflare blocks it and the Wayback Machine rate-limited the request --
  so the discrepancy is recorded rather than resolved.

This matters for a dithering game: alternating-pixel greyscale works because one
drawn frame is one field, so the dither alternates at the drawn rate. Pinball
dithers at 36 Hz on hardware, which means at 0.765x it dithered at 27.5 Hz and
beat against nothing in particular -- an audible cadence that reads as "the frame
rate is wrong" and was in fact the machine running slow.

### All twelve cartridges, swept

Every cartridge in the set was run on the watch: `pm bench` for 400 frames
unpaced to measure emulation, then `pm start` for 15 s paced to measure the real
loop. Radio off throughout, so the figures are comparable with each other.

| Cartridge | bench fps | instr/frame | us each | emu ms | speed | PRC | drawn Hz |
|---|---|---|---|---|---|---|---|
| Lunch Time | 89.3 | 3,483 | 3.22 | 11.8 | 0.998x | /2 | 36 |
| Party Mini - Baseline Judge | 90.6 | 4,486 | 2.46 | 11.9 | 0.998x | /3 | 24 |
| Party Mini - Chansey's Dribble | 90.9 | 4,486 | 2.45 | 11.9 | 0.998x | /3 | 24 |
| Party Mini - Ricochet Dribble | 90.9 | 4,486 | 2.45 | 11.9 | 0.998x | /3 | 24 |
| Party Mini - Rocket Start | 90.6 | 4,486 | 2.46 | 11.9 | 0.998x | /3 | 24 |
| Party Mini - Slowking's Judge | 90.9 | 4,486 | 2.45 | 11.9 | 0.998x | /3 | 24 |
| Party Mini | 91.1 | 4,489 | 2.45 | 11.1 | 0.998x | /3 | 24 |
| Pinball Mini | 83.1 | 3,811 | 3.16 | 13.0 | 0.998x | /2 | 36 |
| **Puzzle Collection** | **81.3** | **3,678** | **3.34** | **14.1** | **0.971x** | /2 | 36 |
| Tetris | 89.7 | 3,484 | 3.20 | 11.2 | 0.999x | /2 | 36 |
| Zany Cards | 103.0 | 4,382 | 2.22 | 9.7 | 0.999x | /2 | 36 |
| Snorlax's Lunch Time | 88.6 | 3,483 | 3.24 | 11.8 | 0.998x | /2 | 36 |

**Eleven of twelve run at full speed. Puzzle Collection does not**, and it is the
one that matters for honesty: over a 75-second run it drifts to **0.971x** with
emulation at **14.1 ms against the 13.89 ms budget**, 145 ticks behind and never
recovering. Its instruction count is the second lowest in the set (3,678 per
frame) but its instructions are the most expensive at **3.34 us**, so the cost is
in the interpreter's time per instruction rather than in how much work the game
asks for. A 15-second sample read 0.992x, which is why the short sweep would have
called it a pass; the longer run is the number to believe.

**This corrects the estimate in the previous version of this document.** Entry 67
projected 16.1 ms for the heaviest cartridge by scaling the host survey's 5,053
instructions per frame. No cartridge here exceeds 4,489, and the real worst case
is Puzzle Collection at 14.1 ms from expensive instructions rather than numerous
ones. The estimate was pessimistic and pointed at the wrong mechanism, though it
reached the right conclusion: the port is not uniformly at full speed.

Heap was read after every cycle and returns to 110,363 bytes each time, against
110,675 at boot: a 312-byte one-time cost, flat across twelve start/stop cycles,
so there is no per-run leak.

**Two faults in the harness cost more than the firmware did, and both are worth
recording because they produced confident wrong answers.**

`/dev/null` does not exist in tdsh's filesystem jail, so `pm stop > /dev/null`
failed with a redirection error and never executed. The first sweep therefore
left cartridge one running for its entire duration, and every later cartridge's
`pm bench` was refused with "stop the emulator first" while its `pm status`
reported cartridge one. Eleven of the twelve rows in that run were the same
measurement. A redirection failure is a command that does not run, which is a
different fault from a command that runs and fails.

The board also went silent once during that first sweep and needed an esptool
reset. The console capture held no panic. After the wrecked sweep internal RAM
read 26,919 bytes with the radio up, against 70,031 at a clean boot, and the SSH
server refused to start below its 65,536-byte floor -- consistent with one
emulator left running plus WiFi, but not established. It did not reproduce in
eight consecutive start/stop cycles or in the corrected sweep, and is recorded as
an observation rather than a cause.

### Cost, measured rather than estimated

`libpokemini.a` is 14,564 bytes of `.bss` and 63,692 bytes of flash (58,556 text
+ 5,136 rodata), against a scoping estimate of 114,708 text and 14,984 bss. The
app partition still has 284,848 bytes free, so the estimate's conclusion holds
with more room than it claimed.

### The prepared binary cannot run on the watch as it is configured

Kept because it cost a cycle and would otherwise be retried. The obvious route —
copy `for-escapement/pokemini_bench_target` to the watch and run it — **does not
work, and the reason is not fixable by retrying.** That binary is a **Linux** ELF:
built by Escapement's own crosstool-NG (GCC 14.1.0) against uClibc, with `_start`
and `__libc_start_main` and no ESP-IDF runtime. The watch runs **ESP-IDF v5.3.1
with TinyDesk Shell**, which is not Linux and has no ELF loader — `tdsh` reports
`command not found` and has no `chmod` or `exec`.

Attempted on 2026-11-13: WiFi on, `ssh start`, SFTP to `/sd` (392,204 bytes at
18.1 KiB/s, md5 matching), `chmod` absent, execution refused. File removed, radio
off again. The `for-escapement/` directory name was literal — it was built for
the Linux watch, not for this firmware.

**Do not estimate the target figure by scaling the host one.** `POKEMINI-CORE.md`
says so directly — "the host figure cannot be scaled to the target by a ratio —
any such ratio would be invented". An attempt while writing this document
produced anything from 0.3 to 1.3 Mips depending on which anchor was chosen,
which is the same non-answer in three costumes.

## 7. Recommendation

**The benchmark has been run and it clears the requirement.** What follows is
therefore a build plan rather than a conditional. The one reservation is the
instruction-count caveat in section 6: the margin is thin against heavy
cartridges, so the first hardware run of a real cartridge decides whether the
interpreter needs work before the port is worth finishing.

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
