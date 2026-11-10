# Handoff — Escapement, 2026-11-10T13:50

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail lives in `.ai/core-reference.md` (63 records, fourteen topics).
Project history lives in `.ai/core-log.md` (32 entries; newest first — 32, 31,
30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, then 1 through 16).
Do not re-derive what those hold.

## Current objective (verbatim)

Bring up the watch's Linux fully.

Deferred by user decision — do not start these without asking: BLE media keys,
the Rii K06's touchpad, the Logitech keyboard, firmware-side BLE bond
persistence, any wake-from-sleep path, and moving the emulator to core 0.

## Reconstruct (in this order)

1. `git log --oneline -8` — `main` was at `7e5efdc`, clean and fully pushed,
   before the commit carrying this file.
2. `.ai/core.md` (policy), then `.ai/core-reference.md` (hardware facts — open
   the record, not the index line), then `.ai/core-log.md` entries 32, 31, 30.
3. `git -C build/esp-hosted/esp_hosted_ng branch --show-current` — expect
   `ble-bond-wip`, five local-only commits.

## State verified on hardware 2026-11-10 13:47

Everything below was measured on the running watch, not inferred:

```
PMU        capacity 100 / voltage_now 4176000 uV / Full / usb online 1
RTC        rtc0 ticking, hwclock -r works (used to fail EINVAL)
0x51 read  w1 0x04 r7 -> 0x52 0x45 0x20 0x10 0x02 0x11 0x26 = 20:45:52 UTC
expander   0x20: 0xff 0xff        haptic 0x5a: 0xe0
panel      co5300 bound, fb0 320x400 @16 bpp, painting works
WiFi       10.10.0.15            BLE keyboard  running
i2c errors dmesg: 0 xfer FAIL, 0 SCL timeout, 0 short read
load       0.28 (was 1.09 with tbtns running)
```

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

**No backup matches the current state.** The newest two both predate the
multi-byte fix, and the one before that is the old bit-banged build:

```
~/twatch-backup/working-2026-11-10-1251.bin   hardware I2C, no touch, chatty boot
~/twatch-backup/working-2026-11-10-1053.bin   i2c-gpio — full function incl. touch
~/twatch-backup/working-2026-11-10-0940.bin   i2c-gpio, 40 MHz
~/twatch-backup/twatch-factory-16MB.bin       stock
```

Take a full backup (`./scripts/watch-backup.sh`, 16 MiB, ~3.5 min) **before the
next kernel change**. If touch is needed before then, restore 1053: it is the
fully functional watch. Restore command:

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
