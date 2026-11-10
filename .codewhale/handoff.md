# Handoff — Escapement, 2026-11-10T14:49

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail lives in `.ai/core-reference.md` (70 records, fourteen topics).
Project history lives in `.ai/core-log.md` (36 entries; newest first — 36, 35, 34, 33,
31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, then 1 through 16).
Do not re-derive what those hold.

## Current objective (verbatim)

Bring up the watch's Linux fully.

Deferred by user decision — do not start these without asking: BLE media keys,
the Rii K06's touchpad, the Logitech keyboard, firmware-side BLE bond
persistence, any wake-from-sleep path, and moving the emulator to core 0.

## Reconstruct (in this order)

1. `git log --oneline -8` — `main` was at `6af4934`, clean and fully pushed,
   before the commit carrying this file.
2. `.ai/core.md` (policy), then `.ai/core-reference.md` (hardware facts — open
   the record, not the index line), then `.ai/core-log.md` entries 36, 35, 34.
3. `git -C build/esp-hosted/esp_hosted_ng branch --show-current` — expect
   `ble-bond-wip`, five local-only commits.

## State verified on hardware 2026-11-10 14:39

```
PMU        capacity 100 / voltage_now / Full / usb online 1
RTC        21:38:55, hwclock -r works (used to fail EINVAL)
0x51 read  w1 0x04 r7 -> 0x55 0x38 0x21 0x10 0x02 0x11 0x26 = 21:38:55 UTC
PMU 2-byte 0x64 0x00    expander 0x20: 0xff 0xff    haptic 0x5a: 0xe0 0x00
panel      co5300 bound, fb0 320x400 @16 bpp
TOUCH      cst92xx-touch as input2, CST9220 410x502 fw 0x01000003
WiFi       10.10.0.15            BLE keyboard  running
i2c errs   12 (touch-poll NACKs, all recovered)
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

## Touch — a kernel input driver, working

`tbtns` is **gone by user decision**, not broken-and-parked. It is stopped,
moved to `/etc/disabled/` on the watch, and deleted from the tree (recoverable
from `1c37c50`).

Its replacement is **`patches/kernel/touch/final/cst92xx.c`**, a polling
`i2c_driver` registering `cst92xx-touch` with `ABS_X`/`ABS_Y`/`BTN_TOUCH` and
`INPUT_PROP_DIRECT`. Verified on hardware 2026-11-10: bound as
`CST9220 at 0x1a, 410x502, fw 0x01000003`, registered as `input2`, and captured
real events including a drag and a release.

The driver was written from LilyGo's own source, not from a guess — see TCH-005
through TCH-008 in the reference. The four things that matter:

- **Reset is XL9555 P08, not an SoC GPIO.** Port 1 reads `0xFF` at boot (all
  inputs), so nothing had ever reset the part. The driver gets it as
  `reset-gpios = <&xl9555 8 GPIO_ACTIVE_LOW>`; probe waits for the expander.
- **Coordinates are panel coordinates.** `0xD1F8` reports 410x502, exactly the
  panel, so no calibration and no axis swap. fb0 is the 45 px inset rectangle:
  consumers subtract 45 and **clamp** (a border press legitimately decodes
  outside it — measured `fb=(8,408)`).
- **Only event `0x06` is a press**, and byte 6 must be `0xAB`. A non-zero finger
  count with event `0x00` is not a touch; accepting it invents a phantom at
  `(41,451)`.
- **There is no release event.** The part just stops reporting, so release is a
  250 ms gap (~4 of its ~66 ms report periods).

`0xD1 = 0x1E` is **not** part of official bring-up — it lives in `setMode()`,
whose only caller is `sleep()`. TCH-002 was corrected in place.

Next improvement is interrupt-driven polling on `TP_INT` (GPIO 12, falling
edge). Not attempted yet: the poll is what proved the protocol, and adding an
unverified interrupt would have mixed two unknowns.

## The bus defect that was blocking touch

`esp32_i2c_run()` returned `-ENXIO` on NACK and `-EAGAIN` on arbitration loss
**without calling recovery** — recovery was wired only into the timeout paths.
A polled capacitive controller produces exactly those outcomes, so one such
failure left `SR.bus_busy` set, the pre-transfer check never fired, and every
later transfer to every device failed until reboot. That is what wedged the bus
every time touch was read, and what the old daemon hit. Recovery is now on both
paths, gated on the bus actually being busy so an absent device still NACKs
cheaply. Verified: 1687 report reads, 11 genuine NACKs, 11 recoveries, bus still
healthy afterwards.

## One known open defect, not a regression

A transfer to an **absent address** returns `-ETIMEDOUT` instead of `-ENXIO`, so
`i2cdetect` shows `--` from a timeout rather than a fast NACK. It costs scan
time and clean probe semantics and nothing else. Fix it in the touch cycle.

## Backups — read the directory, do not trust this file

**`working-2026-11-10-1444.bin` is the current, complete state.** Taken
2026-11-10 14:47, 16 MiB, sha256 `1a2637f5…`. It is the first backup that
contains working touch on the hardware bus, and it was verified rather than
trusted:

- the stored `.sha256` matches an independent `sha256sum` of the file;
- the 2,841,808-byte kernel extracted at offset `0x140000` is byte-identical to
the built `xipImage` (`d47e2cc3…`), so the right kernel is in the right slot;
- the image carries the `hynitron,cst92xx` compatible and the recovery format
  string `%s: int_raw=%08x sr=%08x busy=%u` plus the `ARB_LOST` literal, so
  both the touch driver and the I2C fix are in it;
- it was taken **after a reboot**, so it is a cold-boot state rather than a
  warm one.

```
~/twatch-backup/working-2026-11-10-1444.bin   1a2637f5  CURRENT — hardware I2C,
                                                        multi-byte fixed, TOUCH WORKS
~/twatch-backup/working-2026-11-10-1353.bin   ee2133e0  hardware I2C, multi-byte
                                                        fixed, NO touch
~/twatch-backup/working-2026-11-10-1251.bin   04cfee94  hardware I2C, no touch,
                                                        chatty boot, pre-fix
~/twatch-backup/working-2026-11-10-1053.bin   b0f31a2d  i2c-gpio — full function
                                                        incl. old touch daemon
~/twatch-backup/working-2026-11-10-0940.bin   fd7e5bf9  i2c-gpio, 40 MHz
~/twatch-backup/twatch-factory-16MB.bin       471c9a21  stock
```

1444 is the one to restore. 1053 remains the fallback if the hardware-I2C path
is ever suspected: it is the bit-banged build with the old userspace touch
daemon, fully functional for its time. Restore command:

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
