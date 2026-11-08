# LilyGo's own driver: open source, MIT, and exactly how they do it

`Xinyuan-LilyGO/LilyGoLib` is **MIT licensed**. The T-Watch Ultra's panel is
driven by `src/display/LilyGoDispInterface.cpp` (`LilyGoDispQSPI`). This is the
reference that should have been read first, and everything below is taken from
it directly rather than inferred.

## Which SPI host

```c
#ifdef ARDUINO_T_LORA_PAGER
#define SPI_DEV_HOST_ID     SPI2_HOST
#else
#define SPI_DEV_HOST_ID     SPI3_HOST
#endif
```

**The T-Watch Ultra uses SPI3**, not SPI2. This matters because SPI3's GPIO
matrix signal indices are entirely different from SPI2's:

```
                     SPI3        SPI2 (FSPI)
SPICLK_OUT_IDX        4            101
SPID_OUT_IDX          1            103
SPIQ_OUT_IDX          0            102
SPIQ_IN_IDX           0            102
SPIHD_OUT_IDX         2            104
SPIWP_OUT_IDX         3            105
```

Routing SPI2 signals while expecting an SPI3 peripheral produces exactly the
symptom seen throughout: every transaction completes and nothing reaches the
panel.

## Bus and device configuration

```c
spi_bus_config_t spi_config = {
    .data0_io_num = d0,   // 38
    .data1_io_num = d1,   // 39
    .sclk_io_num  = sck,  // 40
    .data2_io_num = d2,   // 42
    .data3_io_num = d3,   // 45
    .data4..7_io_num = -1,
    .max_transfer_sz = 0x40000 + 8,
    .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
};

spi_device_interface_config_t dev_config = {
    .command_bits = 8,
    .address_bits = 24,
    .mode = SPI_MODE0,
    .clock_speed_hz = freq_Mhz * 1000 * 1000,
    .spics_io_num = -1,          // CS driven by hand
    .flags = SPI_DEVICE_HALFDUPLEX,
    .queue_size = 17,
};
spi_bus_initialize(SPI_DEV_HOST_ID, &spi_config, SPI_DMA_CH_AUTO);
```

`SPICOMMON_BUSFLAG_GPIO_PINS` confirms matrix routing is required, which matches
what the IO_MUX inspection showed: these pins have no SPI alternate function.

They use **DMA**, not FIFO polling.

## Reset sequence

```c
pinMode(rst, OUTPUT);
digitalWrite(rst, HIGH);  delay(200);
digitalWrite(rst, LOW);   delay(300);
digitalWrite(rst, HIGH);  delay(200);
```

Starts **high**, pulses low for **300 ms**, releases. The Linux driver asserted
first and held 20 ms, which is a different waveform.

## Register write

```c
t.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
t.cmd   = 0x02;
t.addr  = cmd << 8;
t.length = 8 * length;
```

CS low around it, then high. `0x02` is the single-line write opcode.

## Pixel stream

```c
digitalWrite(_cs, LOW);
// first transfer
t.flags = SPI_TRANS_MODE_QIO;
t.cmd   = 0x32;
t.addr  = 0x002C00;
t.length = chunk_size * 16;      // chunk_size in PIXELS
// continuation transfers
t.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
          SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
t.command_bits = 0; t.address_bits = 0; t.dummy_bits = 0;
```
Chunks of 16384 pixels. CS released only after the whole stream.

So `0x32` is the quad write opcode and `0x002C00` carries RAMWR in the address
phase — which is what the Linux driver eventually converged on, independently.

## What this means for the Linux driver

Switching `co5300.c` to SPI3 (`0x60025000`) and the SPI3 signal indices was
done, and reading SPI3's registers back returned **all zeros** — including the
clock gate register. So SPI3 is not reachable at that address as things stand:
either its peripheral clock is not ungated (`SYSTEM_SPI3_CLK_EN`, bit 16 of
`SYSTEM_PERIP_CLK_EN0` at `0x600C0018`, default enabled) or the block is held
in reset by the kernel's clock/reset drivers, which the panel driver bypasses.

## The recommendation, unchanged and now better supported

**Stop hand-programming the peripheral.** Extend `drivers/spi/spi-esp32.c`,
which already handles clock gating, reset, pin routing and transfer framing, and
which is currently used by nothing on this board. Everything above — host
selection, mode, bit widths, phases, DMA — is exactly what that driver and
`spi_transfer` already express, with the one exception of the QIO data phase,
which it currently refuses.

That is a smaller, better-understood change than continuing to reproduce
`spi_master` by hand, and it is the shape LilyGo's own driver has.

---

## Attempt: switch to SPI3 and ungate it

Done, and it did not work:

```
co5300: system clk_en0=6101e007 rst_en0=9efe1ff8
```

Decoding those: **SPI3 clock enabled (bit 16 = 1), SPI3 reset released (bit 16
of the reset register = 0)** — the writes took effect. And yet:

```
before-selftest:  cmd=00000000 ctrl=00000000 clock=00000000 user=00000000 ...
after-selftest:   cmd=00000000 ctrl=00000000 clock=00000000 user=00000000 ...
after-first-cmds: cmd=00000000 ctrl=00000000 clock=00000000 user=00000000 ...
```

**Every SPI3 register still reads zero**, including the ones the driver writes,
while the self-test still reports "transaction completes" — which can only mean
the completion bit is reading zero too.

So reads from `0x60025000` are not landing, even though the DT declares SPI3
there, the clock is ungated and the block is out of reset. Possibilities:

  * the base is wrong for this silicon (the dtsi was written by the port, not
    verified against an S3 with SPI3 in use)
  * the `ioremap` in this driver is not providing a live mapping for that block
    the way `devm_platform_ioremap_resource` would
  * SPI3 needs an APB-side gate the SYSTEM register does not cover

Note `clk_en0` bit 6 (SPI2) reads **0** while SPI2 demonstrably works for the
flash, so this register is not simply "1 = enabled" per peripheral and the
decode above may itself be wrong.

## The recommendation, restated and now with evidence

**Write `co5300` as a normal SPI client of `spi-esp32.c`, not as a platform
driver that pokes registers.** That driver already does `devm_clk_get`,
`clk_prepare_enable`, `reset_control_deassert` and `devm_platform_ioremap_resource`,
so clock, reset and mapping are correct by construction rather than by my hand.
On SPI2 it demonstrably works; on SPI3 it would settle in one boot whether the
block is reachable at all.

What then has to be added to `spi-esp32.c`:

  * `tx_nbits == SPI_NBITS_QUAD` support (it currently `WARN_ON`s and proceeds
    single-bit) — the `SPI_FWRITE_QUAD` path, roughly 10 lines in
    `esp32_spi_prepare_transfer`
  * a way to express command and address phases. `spi_transfer` cannot, but
    `spi-mem` (`spi_mem_op` with `cmd`, `addr`, `dummy`, `data` and per-phase
    bus widths) expresses this protocol exactly, and is the idiomatic answer.

That is the work. It is a few hundred lines rather than a few dozen, but every
one of the last fifteen flash cycles has been me re-deriving what
`spi-esp32.c` and `spi-mem` already do correctly.

---

## Rewritten as an SPI client of spi-esp32 — and what that settled

The panel driver is now a normal `spi_driver` under `&spi3`, not a platform
driver poking registers. That produced two firm results.

**1. SPI3 works, and only through the kernel driver.**

```
esp32_spi 60025000.spi: probed: bus -1, clk 80000000 Hz
```

`spi-esp32` binds to SPI3 and gets an 80 MHz clock. The earlier platform driver
reading zeros from `0x60025000` was therefore a mapping or ordering fault in
*that* driver, not an unreachable block. Its probe does `devm_clk_get`,
`clk_prepare_enable`, `reset_control_deassert` and
`devm_platform_ioremap_resource` in the right order; hand-rolling those was the
mistake.

**2. The panel still does not initialise.**

```
co5300: TE edges before init = 0
co5300: TE edges after init = 0 (controller NOT scanning)
co5300 spi0.0: 410x502 RGB565, fb0 registered
```

Transactions complete, `fb0` registers, and GPIO6 never pulses.

Two real bugs fixed on the way, both worth keeping:

  * **D2/D3 were routed to SPID.** Quad data is four *different* signals —
    D0=SPID(1), D1=SPIQ(0), D2=SPIWP(3), D3=SPIHD(2). Routing D2 and D3 to
    SPID put the same bit on three wires.
  * **IO_MUX offsets above GPIO21 are not `pin*4`.** GPIO37=0x98, 38=0x9c,
    39=0xa0 (MTCK), 40=0xa4 (MTDO), 41=0xa8 (MTDI), 42=0xac (MTMS), 45=0xb8.

## The one remaining difference, and it is structural

LilyGo's register write sets:

```c
t.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
t.cmd   = 0x02;
t.addr  = cmd << 8;
```

**The command and address phases go out on multiple lines**, while the data
phase stays single. That is not something `spi_transfer` can express, and it is
what the current driver cannot reproduce: it sends `0x02, addr[23:16],
addr[15:8], addr[7:0], data` as one byte stream, all single-line.

A CO5300 that expects the register number delivered on four lanes will see
nothing it recognises from that, which is consistent with TE never starting.

**So the next step is `spi-mem`, and it is now the only step.** `struct
spi_mem_op` models command, address, dummy and data as separate phases with
independent bus widths (`op.cmd.buswidth = 4`, `op.addr.buswidth = 4`,
`op.data.buswidth = 1` for registers and `4` for pixels). That is precisely
this protocol. What is needed:

  1. `CONFIG_SPI_MEM=y`
  2. `spi_controller_mem_ops` on `spi-esp32`: a minimal `exec_op` that maps
     `spi_mem_op` onto the peripheral's command/address/data phases with
     `SPI_FCMD_QUAD` / `SPI_FADDR_QUAD` and `SPI_FWRITE_QUAD` set per phase
  3. `co5300` rewritten to call `spi_mem_exec_op` instead of `spi_write`

That is a bounded, well-specified piece of work — unlike everything since the
first flash cycle, which has been discovering these facts one boot at a time.
