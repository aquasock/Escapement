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
