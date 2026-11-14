# NES accuracy baseline — AccuracyCoin on the T-Watch Ultra

First measurement of *correctness* rather than speed on this port. Recorded
2026-11-14 against the image from core-log entry 94 (sha256 `2e71a9fd…`), which
added the `ppu.vaddr_inc` initialisation without which the ROM would not render
at all.

The ROM is `AccuracyCoin.nes` (40,976 bytes, iNES, mapper 0, PRG 32K, CHR 8K),
run from `/sd`. Reference behaviour confirmed on `fceux 2.6.5` on the
development machine before the watch was measured.

## Result

**14 of 22 pages completed. 68 pass, 35 fail, of 103 tests. The suite crashes on
page 15.**

On real hardware, for comparison, AccuracyCoin reports 100% pass on a
cycle-accurate NES implementation. Every failure below is a place this port
deviates from the console, and each one is listed because a future cycle
should not have to rediscover which.

## Per-page results

Column 2 is the pass/fail split; the notes name the individual failing tests.

| Page | Group | P | F |
|---|---|---|---|
| 1 | CPU Behavior | 5 | 4 |
| 2 | Addressing Mode Wraparound | 4 | 2 |
| 3 | Unofficial Instructions: SLO | 7 | 0 |
| 4 | Unofficial Instructions: RLA | 7 | 0 |
| 5 | Unofficial Instructions: SRE | 7 | 0 |
| 6 | Unofficial Instructions: RRA | 7 | 0 |
| 7 | Unofficial Instructions: \*AX (SAX/LAX) | 10 | 0 |
| 8 | Unofficial Instructions: DCP | 7 | 0 |
| 9 | Unofficial Instructions: ISC | 7 | 0 |
| 10 | Unofficial Instructions: SH\* | 1 | 5 |
| 11 | Unofficial Immediates | 6 | 2 |
| 12 | CPU Interrupts | 0 | 3 |
| 13 | APU Registers and DMA Tests | 0 | 10 |
| 14 | APU Tests | 0 | 9 |

## Failing tests

### Page 1 — CPU Behavior

- PASS: ROM is not writable
- PASS: RAM Mirroring
- PASS: PC Wraparound
- PASS: The Decimal Flag
- PASS: The B Flag
- **FAIL: Dummy read cycles**
- **FAIL: Dummy write cycles**
- **FAIL: Open Bus**
- **FAIL: All NOP instructions** (ROM footer: `NOP MUST DUMMY READ`)

### Page 2 — Addressing Mode Wraparound

- PASS: Absolute Indexed
- PASS: Zero Page Indexed
- PASS: Indirect
- **FAIL: Indirect, X**
- **FAIL: Indirect, Y**
- PASS: Relative

### Page 10 — Unofficial Instructions: SH\*

- **FAIL: SHA INDIRECT, Y** (`$93`)
- **FAIL: SHA ABSOLUTE, Y** (`$9F`)
- **FAIL: SHS ABSOLUTE, Y** (`$9B`)
- **FAIL: SHY ABSOLUTE, X** (`$9C`)
- **FAIL: SHX ABSOLUTE, Y** (`$9E`)
- PASS: LAE ABSOLUTE, Y (`$BB`)

ROM footer reads `SHA BEHAVIOR 3` and `SHS BEHAVIOR 3`, meaning the ROM detected
the unstable-store family behaving as its third documented variant. These
opcodes AND the high byte of the address with the value on real hardware and are
genuinely undefined across cartridge revisions; failing them is expected of any
emulator that implements one fixed behaviour.

### Page 11 — Unofficial Immediates

- **FAIL: ANC IMMEDIATE** (`$0B`)
- **FAIL: ANC IMMEDIATE** (`$2B`)
- PASS: ASR (`$4B`), ARR (`$6B`), ANE (`$8B`), LXA (`$AB`), AXS (`$CB`),
  SBC (`$EB`)

ROM footer reads `ANE MAGIC = SEE` and `LXA MAGIC = SEE`, so those two depend on
the "magic" constant and were accepted on that basis.

### Page 12 — CPU Interrupts

- **FAIL: Interrupt Flag Latency**
- **FAIL: NMI Overlap BRK**
- **FAIL: NMI Overlap IRQ**

### Page 13 — APU Registers and DMA Tests

All ten fail:

- DMA + OPEN BUS
- DMA + `$2002` READ
- DMA + `$2007` READ
- DMA + `$2007` WRITE
- DMA + `$4015` READ
- DMA + `$4016` READ
- DMC DMA BUS CONFLICTS
- DMC DMA + OAM DMA
- EXPLICIT DMA ABORT
- IMPLICIT DMA ABORT

### Page 14 — APU Tests

All nine fail:

- Length Counter
- Length Table
- Frame Counter IRQ
- Frame Counter 4-Step
- Frame Counter 5-Step
- Delta Modulation Channel
- APU Register Activation
- **Controller Strobing**
- **Controller Clocking**

Note the last two: they test `$4016` strobe and shift-register behaviour, which
is the interface the port's own gamepad and on-glass input read. Zelda's input
works, so the deviation is in exact timing rather than in the basic path.

## What the failure profile says

Every passing group is an **opcode implementation**; every failing group is a
**timing or bus-accuracy** behaviour. That is the expected signature of a
compact emulator: nofrendo implements the instruction set including the
undocumented opcodes, and does not model dummy reads, open bus, DMA cycles, or
sub-instruction interrupt latency.

Three consequences worth carrying forward:

1. **The undocumented opcodes are correct.** 52 of 52 across pages 3 to 9. That
   is a stronger result than expected and should not be second-guessed.
2. **APU accuracy is absent by design, not by accident.** The port explicitly
   disables the frame IRQ (`apu_write(APU_FRAME_IRQ, 0x80)` with a source note
   saying nesdev calls for 0 but it "seems to work better disabled"). Page 14's
   failures are that decision showing up in a test.
3. **DMA is not modelled at all.** Page 13's ten failures plus page 1's dummy
   cycles are the same missing capability. This is also the most plausible
   connection to the audio dropout parked in core-log entry 88: a sound path with
   no DMA model has no way to account for CPU-vs-DAC scheduling.

## Known unknowns

- **The page 15 crash is unexplained and uninvestigated.** Whatever group sits on
  page 15, the emulator does not survive it. That is a separate fault from the
  accuracy results above and wants its own cycle; the other 8 pages are also
  unmeasured as a consequence.
- **Pages are 1-indexed as the ROM displays them.** The page count shown is 22.
- **No automated capture exists.** These results were read from photographs of
  the glass. A future cycle that wants to track accuracy over time would need the
  test names and results keyed out mechanically, which the ROM supports through
  its own on-screen output only.
