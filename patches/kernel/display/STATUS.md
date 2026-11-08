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
