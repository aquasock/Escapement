# CO5300 display driver — the panel is scanning

State as of 2026-11-08: **working.** Verified on hardware — eight bands, correct
order, full glass.

```
co5300: TE edges after init = 4 *** CONTROLLER IS SCANNING ***
co5300 spi0.0: 410x502 RGB565, fb0 registered
```

TE edges on GPIO6 are the panel's own proof that its internal scan engine is
running, which only happens if it accepted the init table. Before this session
that counter read 0.

Row addressing is verified, and that needed a pattern rather than a flat fill:
eight horizontal bands came out as eight correct bands in the right order. White
first only told us the panel emits — it cannot distinguish 502 addressed rows
from row 0 repeated, which is a failure this project had already hit once.

## The five bugs, in the order they were found

Each one alone produced "every write succeeds, nothing happens".

**1. `exec_op` ignored `op->data.dir`.** `buf.in` and `buf.out` are a union, so
an output transfer aliased its own source as the read-back buffer, and exec_op
then copied the FIFO back over the caller's data. For the init table — in
read-only XIP flash — that is an immediate store fault:

```
Unhandled load/store exception in kernel
pc: 42327f45   s8i a6, a5, 0        <- store byte
excvaddr: 42342729 = co5300_init+1  <- into the const table
```

**2. The bus clock was never programmed on the spi-mem path.**
`SPI_CLOCK_REG` was written only from `prepare_transfer`, which exec_op never
calls. Split out into `esp32_spi_set_clock()` and called from both.

**3. Command and address were sent on four lanes.** ESP-IDF's
`SPI_TRANS_MULTILINE_CMD`/`MULTILINE_ADDR` are ignored unless a
`SPI_TRANS_MODE_?IO` flag is also set, and the working firmware sets neither for
register writes. Dumping its peripheral settles it:

```
firmware  ctrl=00340000     fcmd_quad and faddr_quad both clear
ours was  ctrl=00000240     quad command, quad address, nothing else
```

Command and address are now single-lane everywhere; only the pixel payload is
quad.

**4. The pins were never routed.** Neither SPI bus on this SoC reaches the
watch's panel pins through IO_MUX: SPI3 has no IO_MUX pins at all, and FSPI's
are GPIO 9..14. The panel is on 38..45, so it needs the GPIO matrix — which
`esp32_spi` did not implement. The pad stayed in its reset function and carried
nothing.

Two details that cost extra time here:

  * The signal indices in the older notes (`SPICLK 4, SPID 1, SPIQ 0, SPIHD 2,
    SPIWP 3`) belong to the **flash/cache peripheral**, not SPI3. SPI3 is
    66..70; FSPI is 101..105.
  * Pins 32 and above have their own enable register. Writing `BIT(pin & 31)`
    to `GPIO_ENABLE_W1TS` enabled GPIO 6, 7, 8, 10 and 13 — unrelated pads —
    while the panel pins stayed undriven. High-bank pins need
    `GPIO_ENABLE1_W1TS`.

**5. co5300 undid the routing, and the TE pad was off by one.** After the fix
above the bus clocked (proved by a self-test reading the clock pad's own input
buffer: 912 edges), and the panel still did not respond — because
`co5300_route_pins()` rewrote the same `OUT_SEL` registers *without* the
output-enable bit, switching the pads back off. Removed; `esp32_spi` owns those
pins now.

The TE pad masked the whole thing: `iomux_off()` fell back to `pin * 4`, so the
input-enable meant for GPIO6 landed on GPIO5. TE read a constant 0 and the
driver reported "controller NOT scanning" whether or not it was. The generic
case is `0x04 + pin*4`.

## Instruments worth keeping

**The clock self-test** in `esp32_spi` (`esp32_spi_clk_selftest`) answers "does
the peripheral reach the pad" without any device answering. A pad's input buffer
reads its own driven level, so it counts transitions on the clock pin during a
2 MHz transfer:

```
clk pin 40: 912 edges in 962 samples, usr_cleared=1 dma_raw=00001000
            -- SPI clock REACHES the pad
```

It must run **after** `esp32_spi_init()`. Called earlier it reports a stall,
because that function is what enables the peripheral's clock gate.

**The register dump** in `exec_op` prints the same fields as the SPIDUMP block in
TinyPetz `main/panel.c`. Diffing the two is the fastest way to find a
misprogrammed field, and it is what identified bug 3.

## Bus choice: SPI2, not SPI3

The panel is on SPI2. LilyGo's own library uses SPI3 for this board, and the
earlier work followed it, but the ESP-IDF firmware that provably drives this
glass uses SPI2, and both buses behave identically once routing is correct. SPI2
is the one whose working register values can be diffed against.

## Still open

  * No double buffering, no partial blit: `co5300_blit` sends the whole window as
    one spi-mem op, which is why the transmit buffer is a full frame.
  * `esp32_spi_exec_op` is still FIFO-only, 64 bytes per chunk, no DMA. A frame
    is 6432 chunks of register pokes.
  * Brightness is fixed at 0xFF after init.

## A flat fill proves nothing

From this project's original bring-up notes, and worth repeating because it is
the next test to run: a solid colour renders identically whether the panel
received 502 rows or received row 0 five hundred times. Any test meant to
validate row addressing needs vertical structure — bands, a border, a gradient
along y.
