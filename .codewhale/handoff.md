# Handoff — Escapement, 2026-11-10T13:59

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail lives in `.ai/core-reference.md` (63 records, fourteen topics).
Project history lives in `.ai/core-log.md` (34 entries; newest first — 34, 33, 32,
31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, then 1 through 16).
Do not re-derive what those hold.

## Current objective (verbatim)

Bring up the watch's Linux fully.

Deferred by user decision — do not start these without asking: BLE media keys,
the Rii K06's touchpad, the Logitech keyboard, firmware-side BLE bond
persistence, any wake-from-sleep path, and moving the emulator to core 0.

## Reconstruct (in this order)

1. `git log --oneline -8` — `main` was at `1d56b39`, clean and fully pushed,
   before the commit carrying this file.
2. `.ai/core.md` (policy), then `.ai/core-reference.md` (hardware facts — open
   the record, not the index line), then `.ai/core-log.md` entries 34, 33, 32.
3. `git -C build/esp-hosted/esp_hosted_ng branch --show-current` — expect
   `ble-bond-wip`, five local-only commits.

## State verified on hardware 2026-11-10 13:52 — **after a cold reboot**

This is the post-reboot check, not a warm one, so it proves persistence:

```
PMU        capacity 100 / voltage_now 4175000 uV / Full / usb online 1
RTC        20:51:52, hwclock -r works (used to fail EINVAL)
0x51 read  w1 0x04 r7 -> 0x52 0x51 0x20 0x10 0x02 0x11 0x26 = 20:51:52 UTC
PMU 2-byte 0x64 0x00    expander 0x20: 0xff 0xff    haptic 0x5a: 0xe0 0x00
panel      co5300 bound, fb0 320x400 @16 bpp
WiFi       10.10.0.15            BLE keyboard  running
soak       60 ops over all four devices: 0 fails, 0 kernel errors
load       1.16 -> 0.42 in two minutes (ordinary boot spin, it decays)
```

Seven distinct non-`0xff` bytes decoding to the exact RTC time is the shape
that was broken for the whole session (`0x57 ff ff ff ff ff ff` on every
device), so this is the fix confirmed, not a lucky boot.

**The hardware I2C bus works and is the default.** Multi-byte reads are fixed —
see `docs/cores.md` for the whole proof. In short: `ack_val` is the master's
level on the last byte of *its own command*, so `READ(N)` with `ack_val=1` NACKs
after byte one; ESP-IDF splits into `READ(N-1)` ACK + `READ(1)` NACK and now so
does this driver. Three more defects went with it: `time_out_en` is bit 5 of
`I2C_TO` (driver wrote bit 10, reserved), the nine-clock bus clear was never
latched (`conf_upgate` missing, so recovery could never free a held bus — that
is why every earlier wedge needed a power cycle), and `INT_SCL_ST_TO` /
`INT_SCL_MAIN_ST_TO` were never tested at all.

**What the old docs got wrong, so nobody chases it again:** the
command-register budget was never the problem. A 1-byte-address read and a
2-byte-address read need the same number of commands. That lead is withdrawn.

## Touch — retired, replaced next

`tbtns` is **gone by user decision**, not broken-and-parked. It is stopped,
moved to `/etc/disabled/` on the watch, and deleted from the tree (recoverable
from `1c37c50`). It was the thing wedging the bus: its 15-byte touch read
started the failure storm, and with it removed the bus has been clean.

Next cycle is a **kernel input driver** to replace it. Two facts already
established for whoever writes it:

- The CST9217 needs its reset line released through the XL9555 first. That is
  why, with `tbtns` gone, it returns `0xff 0xff` at register `0x00` instead of
  its `0xca` checkcode.
- The 15-byte report block must be read **whole**. The acknowledge that advances
  the controller's internal buffer is issued against the block; reading part of
  it leaves the buffer mid-block and it never advances.

The split-READ shape and the command budget that make that read possible are
proven. The remaining work is initialisation sequence and input plumbing, not
bus debugging.

## One known open defect, not a regression

A transfer to an **absent address** returns `-ETIMEDOUT` instead of `-ENXIO`, so
`i2cdetect` shows `--` from a timeout rather than a fast NACK. It costs scan
time and clean probe semantics and nothing else. Fix it in the touch cycle.

## Backups — read the directory, do not trust this file

**`working-2026-11-10-1353.bin` is the current state and is verified.** Taken
2026-11-10 13:57, 16 MiB, sha256 `ee2133e0…`. Verified three ways rather than
by the script's own OK: the stored hash matches an independent `sha256sum`, the
kernel extracted from the image at `0x140000` is byte-identical to the built
`xipImage` (`a79b73d0…`), and the running watch reports the same build string
`7.2.4-forkbank-ga623647af724-dirty`.

```
~/twatch-backup/working-2026-11-10-1353.bin   ee2133e0  CURRENT — hardware I2C,
                                                        multi-byte fixed, no touch
~/twatch-backup/working-2026-11-10-1251.bin   04cfee94  hardware I2C, no touch,
                                                        chatty boot, pre-fix
~/twatch-backup/working-2026-11-10-1053.bin   b0f31a2d  i2c-gpio — full function
                                                        incl. touch
~/twatch-backup/working-2026-11-10-0940.bin   fd7e5bf9  i2c-gpio, 40 MHz
~/twatch-backup/twatch-factory-16MB.bin       471c9a21  stock
```

The 1353 image does not contain touch, because `tbtns` is retired. If touch is
needed before the new driver lands, restore 1053: it is the fully functional
watch, on the bit-banged bus. Note both images contain the **kernel** at
`0x140000` and a working `i2c-gpio` driver compiled in, but only 1353 has the
fixed hardware driver selected by the device tree. Restore command:

```sh
PY=build/idf-tools/python_env/idf5.1_py3.14_env/bin/python
$PY -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    --after no_reset write_flash 0 ~/twatch-backup/<image>.bin
```

`--after no_reset` is not optional: without it esptool resets into the app,
Linux claims the USB-Serial-JTAG port, and later commands hang at
`Connecting...`.

## Two traps that cost a session each

**Diagnostics must bound their own cost.** This bit twice. An unbounded
`espressif,debug` dump cost a minute of boot because the console mirrors to
fbcon over SPI. Then `printk_ratelimit()` was used for the next round — and it
is *shared state*, so `tbtns` polling at 66 Hz exhausted it permanently and
every diagnostic went silent exactly when it mattered
(`37 callbacks suppressed`). The driver now uses small per-category counters.
Anything you add must be bounded by construction.

**A kernel rebuild is not reproducible from a dirty tree.** The kernel tree is a
post-build overlay, so `./scripts/build-kernel.sh` fails at patch 0001 with
"patch did not apply". Use `./scripts/build-kernel.sh --clean`; it takes ~45 s
and is the only honest build.

## Tooling notes

- `./scripts/watch-telnet.py '<cmd>'` — shell on the watch. Prefer to serial.
- Flash a kernel alone: `--after no_reset write_flash 0x140000 <xipImage>`, then
  reset with `--after hard_reset read_mac`. Kernel-only leaves `/etc` and
  `/home` intact.
- `/dev/mem` does not exist on this image, so there is no `devmem` escape hatch —
  the driver has to report its own register state.
- `export XTENSA_GNU_CONFIG=$PWD/build/toolchain/xtensa-dynconfig/esp32s3.so`
  before any manual cross-compile, or you get a misleading "wrong format" error.
