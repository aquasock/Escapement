# red-screen

A single ESP-IDF program that turns the panel red. No Linux, no Petz, no
framebuffer. It exists to answer one question in isolation:

    does the CO5300 on this watch light up at all?

**It does.** Verified on hardware 2026-11-08: the screen fills completely red.

## What it proves

Power, the XL9555 display gate, panel init, and the entire QSPI path are all
correct and working. Anything failing under Linux is therefore a driver
problem, not hardware.

It reuses `TinyPetz/firmware/bringup/main/{board,panel}.c` unchanged, so the
only new code is the 90-line `red_screen.c`.

## Geometry, and the bug that cost a round trip

`panel.h` names its constants transposed:

    PANEL_NATIVE_W  502     <- this is the ROW count
    PANEL_NATIVE_H  410     <- this is the COLUMN count

The glass is 410 columns by 502 rows. Trusting the names produced a window from
column 22 to 523 — 25 columns wider than the panel — with both dimensions
swapped, and the panel correctly drew nothing.

The values to use are the ones TinyPetz itself passes from `main.c`:

    WIN_X0 22, WIN_X1 431, PANEL_COLS 410, PANEL_ROWS 502

## The exact sequence that works

```
write_command(0x2A, caset, 4)      CS low -> cmd+addr+4 bytes -> CS high
write_command(0x2B, raset, 4)      CS low -> cmd+addr+4 bytes -> CS high
write_command(0x2C, NULL, 0)       CS low -> cmd+addr, no data -> CS high
panel_stream_begin()               CS LOW, HELD for everything that follows
  row 0        QIO, cmd=0x32, addr=0x002C00, 410 pixels
  rows 1..501  QIO, bare data, no command, no address
panel_stream_end()                 CS HIGH
```

Three details that matter and are easy to miss:

1. **Both 0x2C and 0x32 are sent.** 0x2C is a plain single-line RAMWR that
   opens the stream; 0x32 with address 0x002C00 is the first *pixel* transfer.
   Sending only 0x32, or sending it with no data, does not open a stream.

2. **CS stays low across all 502 rows.** The controller treats a CS edge as
   end-of-burst and silently discards the rest, returning success. Toggling CS
   per row gives one row of image.

3. **Brightness after init.** The init table leaves 0x51 at 0x00, which is a
   black backplane. `panel_brightness_raw(0xFF)` follows, and
   `panel_ctrl_raw(0x20)` sets BCTRL in 0x53 so brightness takes effect. A
   panel told to be black is indistinguishable from a dead one.

## Build and flash

```sh
source <esp-idf>/export.sh
idf.py set-target esp32s3 && idf.py build
esptool.py --chip esp32s3 --port /dev/ttyACM1 --baud 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x10000 build/red-screen.bin
```
