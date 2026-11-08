# CO5300 display driver — work in progress, does not work yet

The watch currently runs the kernel from before this work. Do not flash the
display kernel: it hangs the boot.

## Why a platform driver and not an SPI client

The CO5300 protocol is an 8-bit command phase, a 24-bit address phase and a
4-bit data phase, chip select held low across all three. `spi_transfer` has no
representation for that, so a normal SPI client driver cannot build these
transactions. `drivers/spi/spi-esp32.c` in this tree is also hard-limited:

```c
mode = max_t(unsigned int, t->rx_nbits, t->tx_nbits);
WARN_ON(mode != SPI_NBITS_SINGLE);
```

It advertises quad in `mode_bits` but warns and proceeds single-bit. So the
driver programs the SPI2 registers directly, which is what the ESP-IDF
firmware does too.

## What exists

`co5300.c` — platform driver, ~500 lines: SPI2 register access, the init table
transcribed from the working firmware, an fbdev framebuffer, and a blit path.
Builds clean and links. Config and DT are in place:

    panel@60024000 { compatible = "lilygo,co5300"; ... }

## Where it fails

```
co5300: SPI transaction never completed
```

Every transaction times out waiting for `SPI_USR` to clear. The first version
had no bound on that wait and hung the boot with no message at all — which is
why the wait is now capped and reports.

Two things are already fixed and worth keeping:

  * **The SPI clock gate.** Without
    `SPI_CLK_GATE_REG = SPI_CLK_EN | SPI_MST_CLK_ACTIVE | SPI_MST_CLK_SEL`
    the peripheral never clocks. Omitting it produced the silent hang.
  * **A bounded wait**, so a misprogrammed peripheral fails loudly instead of
    spinning inside probe.

What is still wrong is the transaction setup itself — most likely the
`SPI_CTRL_REG` quad bits (`SPI_FCMD_QUAD`/`SPI_FADDR_QUAD`) or the way the
command and address phases are armed relative to `SPI_USER_REG`. Those bit
positions were taken from ESP-IDF's `spi_ll.h`, not verified against this
kernel's own header, and a wrong bit there would look exactly like this.

## The next step, concretely

Do not debug the full panel path. First write a **minimal single transaction**
that does one 8-bit command with no data and polls a status bit the peripheral
must report — or better, use the existing working `esp32-spi` driver to prove
the registers are reachable at all by making the panel respond to a simple
software-reset command over 1-bit SPI. Once one transaction completes, the rest
is arithmetic.

## Reference material, already extracted

From `TinyPetz/firmware/bringup/main/panel.c`, which works on this hardware:

    CS 41, SCK 40, D0 38, D1 39, D2 42, D3 45, RST 37, TE 6
    0x2A = 00 16 01 AF      columns 22..431
    0x2B = 00 00 01 F5      rows 0..501
    0x3A = 55               RGB565
    init: 11 commands, extracted from the factory image at 0x130dd4
    CS low for the whole stream; RAMWR once per window; RGB565 byte-swapped

That firmware is the ground truth. Its own notes warn that a failed transfer
returns ESP_OK silently, so on ESP-IDF the same bug would have looked like a
blank panel rather than an error.

---

## Second pass: six bugs found, panel still dark

The driver now builds, boots, registers `fb0`, runs every transaction without
error — and the panel does not light. The controller is not scanning:
GPIO6 (the tearing-effect pin) shows **0 edges** before and after init, which
from the bring-up notes separates "panel dead" from "panel alive but fed bad
pixels". It is the former.

Bugs found and fixed this pass, in the order they surfaced. Each was invisible
from the kernel side, which is why the diagnostics were worth writing.

**1. Every SPI_CTRL quad bit was wrong.** `fcmd_quad` I had at 11, actual 9;
`faddr_quad` 14 vs 6; `faddr_dual` 13 vs 5; `fread_quad` 20 vs 15. Transcribed
from memory of ESP-IDF's helpers instead of decoded from `spi_struct.h`. The
kernel's own `spi-esp32.c` had `q_pol`/`d_pol`/`hold_pol` right, which is
exactly why the others looked trustworthy.

**2. FIFO overrun.** `FIFO_WORDS 64` — the FIFO is `W0..W15`, sixteen 32-bit
words. The pixel loop wrote one 16-bit pixel per slot at a 2-byte stride, ran
past the end, and overwrote the control registers at 0xe0/0xe8. The register
dump caught it: `clock` read `0xffff` where `0x3043` had been written.

**3. No pin routing at all.** The panel's pins have no SPI2 alternate function
in IO_MUX — GPIO38's only SPI function is `FSPIWP` at sel 2, and
39/40/42/45 expose nothing but plain GPIO. The devkit's pins 10–13 *do* have
SPI2 at sel 4, which is where the copied `FUN_SEL(4)` came from. Everything
must go through the GPIO matrix.

**4. The SPI clock gate.** Without
`SPI_CLK_GATE_REG = CLK_EN | MST_CLK_ACTIVE | MST_CLK_SEL` the peripheral never
clocks, `SPI_USR` never clears, and the first version spun forever inside probe
with no message at all.

**5. Register data phase width.** The reference sends command parameters
single-bit and only pixels quad — it sets `MULTILINE_CMD|MULTILINE_ADDR` but not
`MODE_QIO` on a register write. Sending parameters quad puts a wrong value in
every register. Also: the pixel stream opens with command **0x32** and address
**0x002C00**, not 0x2C — 0x02 is single-line write, 0x32 is the quad write.

**6. `MCU_SEL` not shifted.** `MCU_SEL_GPIO` was `1` instead of `1 << 12`, so
every panel pad stayed in its IO_MUX default function (MTDO stayed MTDO) while
the matrix output selector pointed the SPI signal at it. Readback made it
obvious: `iomux[40]` read back `0x8401` — precisely the value written, with
`MCU_SEL` still 0.

After fix 6 the readback is `iomux[40]=0x9400 outsel[40]=0x65` — MCU_SEL is
GPIO and the output selector names FSPICLK. Correct, and the panel is still
dark.

## What is left

Two candidates, neither yet tested:

  * **Display power may not actually be asserted.** The rail is XL9555 P07,
    driven through `regulator-fixed`. The regulator framework reports
    `enabled`, but that only proves `regulator_enable()` was called — not that
    `pca953x` made P07 an *output* (the expander powers up all-inputs, and an
    output-register write to a pin still configured as input does nothing).
    This is the single most likely remaining cause and the cheapest to check.

  * **Reset may not be toggling.** `iomux[37]` reads `0x0a00`: input-enable
    set, `MCU_SEL` 0. For most S3 pads sel 0 is the GPIO default so this is
    probably fine, but it has not been confirmed that GPIO37 actually moves.

## The next concrete step

Stop guessing at the SPI protocol; it is now built to match the reference
line for line. Determine whether the panel is powered:

    i2cget -y -f 0 0x20 0x06     # config: bit 7 must be 0 (output)
    i2cget -y -f 0 0x20 0x02     # output: bit 7 must be 1 (high)

If `cfg0` bit 7 is 1, the expander never made P07 an output and the panel has
had no power this whole time — which would explain every symptom, including a
silent one, and would mean `regulator-fixed` needs its gpio requested with an
explicit direction rather than relying on the framework.

---

## Third pass: everything matches the reference and it is still dark

Verified after seven fixes:

    iomux[40]=0x9400  outsel[40]=0x65   SCK  -> FSPICLK, GPIO, OE
    iomux[38]=0x9400  outsel[38]=0x67   D0   -> FSPID,   GPIO, OE
    iomux[41]=0x9400                    CS   -> GPIO, OE (default was MTDI!)
    iomux[37]=0x9400                    RST  -> GPIO, OE

    display rail: cfg0=0x3f (P07 an output), out0=0xff (P07 driven high)

and the init table, reset sequence, command/address widths and CS handling now
match `TinyPetz/firmware/bringup/main/panel.c` line for line:

    .command_bits = 8, .address_bits = 24, .spics_io_num = -1
    flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR
    cmd = 0x02, addr = opcode << 8
    RST low 30 ms, high 150 ms
    the same 11-entry table

TE (GPIO6) still reads 0 edges.

## Two honest possibilities

1. The SPI transaction is still wrong in a way I cannot see from the kernel
   side. Every write returns success and the completion bit clears, but
   "completes" only means the peripheral finished shifting; it says nothing
   about whether the panel understood.

2. The TE measurement itself is wrong, in which case "0 edges" is not evidence
   of anything and the panel may be initialising fine while the pixel path is
   broken. GPIO6 as a plain input should work, but that has not been proven
   independently.

## The decisive next test

Do not iterate further on the driver blind. Instead:

1. Flash the ESP-IDF firmware (`TinyPetz`) back and confirm the screen still
   works. That takes two minutes and re-establishes that the hardware, the
   panel and the power are fine -- everything after this point is then known to
   be a software difference, not a dying panel.
2. Write a small ESP-IDF program that uses IDF's own SPI driver with the same
   pin set, rather than hand-programmed registers. If the panel works through
   IDF's SPI on those pins, then the pin routing is right and the fault is in
   my register programming. If it does not, the fault is in the pin map or
   power, and the kernel driver was never the problem.

That is the shortest path to knowing which half is wrong, and it needs no more
guesswork.
