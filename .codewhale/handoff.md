# Handoff — Escapement, 2026-11-10T16:37

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail lives in `.ai/core-reference.md` (77 records, fourteen topics).
Project history lives in `.ai/core-log.md` (43 entries; newest first — 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33,
31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, then 1 through 16).
Do not re-derive what those hold.

## Current objective (verbatim)

Bring up the watch's Linux fully.

Deferred by user decision — do not start these without asking: BLE media keys,
the Rii K06's touchpad, the Logitech keyboard, firmware-side BLE bond
persistence, any wake-from-sleep path, and moving the emulator to core 0.

## Reconstruct (in this order)

1. `git log --oneline -8` — `main` was at `a253f61`, clean and fully pushed,
   before the commit carrying this file.
2. `.ai/core.md` (policy), then `.ai/core-reference.md` (hardware facts — open
   the record, not the index line), then `.ai/core-log.md` entries 43, 42, 41.
3. `git -C build/esp-hosted/esp_hosted_ng branch --show-current` — expect
   `ble-bond-wip`, five local-only commits.

## State verified on hardware 2026-11-10 16:16

```
PMU        capacity 100 / Full / usb online 1; both PEK edges fire (PMU-004)
RTC        hwclock -r works; alarm path fires IRQ 1 (RTC-004)
panel      co5300 bound, fb0 320x400 @16 bpp
TOUCH      cst92xx-touch input2, CST9220 410x502, IRQ 33, interrupt driven
           idle poll 5000 ms, active 20 ms, release 250 ms
HAPTIC     drv2605 bound at 0x5a, ERM; click on every press (HAP-001)
BLE kbd    running            WiFi  flaky — see the BLE trap below
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

Its replacement is **`patches/kernel/touch/final/cst92xx.c`**, an `i2c_driver`
registering `cst92xx-touch` with `ABS_X`/`ABS_Y`/`BTN_TOUCH` and
`INPUT_PROP_DIRECT`. Verified on hardware 2026-11-10: bound as
`CST9220 at 0x1a, 410x502, fw 0x01000003`, registered as `input2`, and captured
three presses and three releases with smooth coordinate tracks.

**Interrupt driven** on TP_INT (GPIO 12, falling edge, `int-gpios` +
`gpiod_to_irq()`, INPUT_PULLUP). Polling is kept at two rates as a safety net:
**20 ms while touched, 5000 ms idle**. That is deliberate — the interrupt wakes
it, it does not have to be the only path, so a wrong polarity degrades to
latency instead of losing touch.

**The interrupt is proven, not assumed** (TCH-010). With the idle poll at 5 s,
a dead interrupt would usually sample after the finger had gone and miss the
tap, so the poll cannot be credited. Eight taps one second apart gave **8
presses and 8 releases**, 255 interrupts on IRQ 33. Idle bus traffic is ~2
reads/s against the ~50 an unconditional 20 ms poll produced. The 5 s poll is
retained rather than removed: if INT ever stops asserting, touch goes slow
rather than dead.

The driver was written from LilyGo's own source, not from a guess — see TCH-005
through TCH-010 in the reference. The five things that matter:

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
- **INT is GPIO 12, and the pull-up is part of the contract** — the controller
  drives it low and releases it, so without the pull the line floats.

`0xD1 = 0x1E` is **not** part of official bring-up — it lives in `setMode()`,
whose only caller is `sleep()`. TCH-002 was corrected in place.

**Each press clicks.** `patches/kernel/haptic/final/drv2605.c` drives the
DRV2605 at 0x5A and `cst92xx` calls `drv2605_tap()` on the press transition
only — a held finger re-reports every ~66 ms, so per-report clicking would buzz
instead of tap. Test it without touching: `echo 1 > /sys/bus/i2c/devices/0-005a/tap`.

This was a **regression this project caused**: the sequence lived in
`userspace/haptic.c`, whose only caller was the retired `tbtns` daemon, so the
motor went silent while the file sat in the tree looking maintained. Now in the
kernel, and the dead library is deleted (HAP-001).

## Why no GPIO interrupt on this board ever worked

**`NR_IRQS` was 33 and the board ran out.** `arch/xtensa/include/asm/irq.h`
sizes the descriptor table `XCHAL_NUM_INTERRUPTS (32) + PLATFORM_NR_IRQS (0) +
1`, and `CONFIG_SPARSE_IRQ` is not selectable on xtensa. That is fine for
peripherals on the CPU's own lines and wrong here, where the GPIO controller
maps one Linux IRQ per line a driver asks for and the AXP2101 adds a virtual
domain on top.

The failure is silent: `irq_create_mapping()` gives ENOSPC, `gpiod_to_irq()`
turns that into `-ENXIO`, and every driver with an optional interrupt falls back
to polling and works. It reads as "this board's GPIO interrupts don't work".
The tell is that it is **line-independent** — probing seven lines gave irq 7 and
8 for the two already claimed and `-ENXIO` for the other five, so prior use was
the discriminator, not the line.

Fixed in **`patches/kernel/irq/final/irq.h`** with `EXTRA_NR_IRQS 128`
(~18 KB image, ~24 KB `.bss`). `CONFIG_PLATFORM_NR_IRQS` is **not** the fix —
it is a hidden symbol and `olddefconfig` recomputes it to 0; the build's config
check catches that.

Two distractions already ruled out: `gpio-esp32.c` needs no `xlate` (gpiolib
supplies `irq_domain_xlate_twothreecell`, and the RTC's line mapping proves it);
and no GPIO line is otherwise invalid.

**Settled 2026-11-10: the RTC and PMU interrupts both work.** Their zero counts
meant *no event had occurred*, not a dead path — and testing it took generating
an event rather than reading the counter. Arming `wakealarm` 20 s ahead moved
`7: esp32xx-gpio 1 pcf85063` from 0 to 1; a tap of **Pusher A** (the power key,
which is not a GPIO and reaches Linux only via `axp20x-pek`) moved
`8: esp32xx-gpio 7 axp2101_irq_chip` to 2 with both `axp20x-pek-dbr` and
`-dbf` at 1 — press *and* release. See RTC-004 and PMU-004.

Their quiet defaults are by design, which is why the zero looked alarming:
PCF85063A Control_2 = `0x07` clears AIE (bit 7), so INT cannot assert until an
alarm is armed and nothing schedules one; AXP2101 mask `0x40..0x42 = 00 c3 00`
unmasks only the four sub-IRQs the driver actually registers.

**The DTS `pmic@34` comment claiming LEVEL_LOW on GPIO 7 “storms and hangs the
boot” was wrong and has been removed** — pin 7 has a pull-up in `pmic_irq_pins`,
the interrupt is connected, and the kernel boots and services presses normally.

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

## The emulator — measured, and faster than the docs said

`tamafb` runs the real V3 ROM and has a bench mode. Headless, flat out, with a
fresh PSRAM block (one reboot per measurement — see below):

```
ROM in PSRAM         513765 instr/s   29.67 M cyc/s   7.42x realtime
ROM in flash         408300 instr/s   23.58 M cyc/s   5.90x
```

With the display at 10 fps and paced by `speed_pct` (argv 6): 1.01x at 100%,
1.50x at 150%, **2.01x at 200%** — the tool's ceiling, at about half the 100 ms
frame budget. Holds 1.00–1.01x at 2/4/8/16 fps. A 20 s run logged **no stalls**.

**Do not compare these with the 204k/273k figures in the reference.** Those are
`corebench`, which runs a synthetic workload; these are the real ROM, which
spends more steps asleep and sleep steps are cheap. Both number sets are right.
Compare corebench to corebench, tamafb to tamafb, never across. See EMU-011.

The old claim "4 fps gives ~1.0x; the panel is the limit" is **retired** — it
was true with the touch daemon competing and is false now.

**What is left: EMU-002**, the ~1.5 s time-screen entry. Not throughput, not the
panel, not the frame rate, not the ROM's location — those are all excluded now.
It is 580 ms of guest computation plus an unexplained 10x panel figure (43–135 ms
per stall frame against 9.5 ms benchmarked). EMU-002 names the next measurement:
read the stall log against a press. Needs a physical press, so wait for the
operator.

## One known open defect, not a regression

A transfer to an **absent address** returns `-ETIMEDOUT` instead of `-ENXIO`, so
`i2cdetect` shows `--` from a timeout rather than a fast NACK. It costs scan
time and clean probe semantics and nothing else. Fix it in the touch cycle.

## Backups — read the directory, do not trust this file

**`working-2026-11-10-1621.bin` is the current, complete state.** Taken
2026-11-10 16:24, 16 MiB, sha256 `464b0413…`, **after a reboot** so it is a
cold-boot state. It is the first image with every verified subsystem: touch
interrupt-driven at a 5 s idle poll, haptics in the kernel, the enlarged
interrupt table, and the I2C recovery fix.

Verified rather than trusted:

- the stored `.sha256` matches an independent `sha256sum` of the file;
- the 2,860,624-byte kernel extracted at `0x140000` is byte-identical to the
  built `xipImage` (`862d0a6c…`);
- the image carries every marker: `hynitron,cst92xx`, `ti,drv2605` and
  `DRV2605 ready, %s motor`, the interrupt-path string
  `idle %u ms, active %d ms, release %d ms, %s`, and the I2C recovery string
  `int_raw=%08x sr=%08x busy=%u`;
- the running watch reports `7.2.4-forkbank-gf134bd8126fa-dirty`, the same
  build string the image carries.

Verify a new backup the same four ways. Two checks are worth naming because
they caught real surprises: the **image size delta** is evidence a config or
table change actually landed (2860624 vs 2860408 was the haptic driver), and a
**grep for a format string does not match a fully-formatted message** — the
recovery line has no literal `NACK:` because `%s` supplies it at the call site.

```
~/twatch-backup/working-2026-11-10-1621.bin   464b0413  CURRENT — touch
                                                        interrupt-driven, haptics,
                                                        enlarged IRQ table
~/twatch-backup/working-2026-11-10-1533.bin   d1b87c30  interrupt-driven touch,
                                                        250 ms idle, no haptics
~/twatch-backup/working-2026-11-10-1444.bin   1a2637f5  touch by polling only
~/twatch-backup/working-2026-11-10-1353.bin   ee2133e0  multi-byte fixed, NO touch
~/twatch-backup/working-2026-11-10-1251.bin   04cfee94  no touch, chatty boot
~/twatch-backup/working-2026-11-10-1053.bin   b0f31a2d  i2c-gpio — full function
                                                        incl. old touch daemon
~/twatch-backup/working-2026-11-10-0940.bin   fd7e5bf9  i2c-gpio, 40 MHz
~/twatch-backup/twatch-factory-16MB.bin       471c9a21  stock
```

**1621 is the one to restore.** 1053 remains the fallback if the hardware-I2C
path is ever suspected: it is the bit-banged build with the old userspace touch
daemon, fully functional for its time. Restore command:
but polling only) and is worth keeping for one more cycle as the A/B for the
interrupt work. 1053 remains the fallback if the hardware-I2C path is ever
suspected: it is the bit-banged build with the old userspace touch daemon, fully
functional for its time. Restore command:

```sh
PY=build/idf-tools/python_env/idf5.1_py3.14_env/bin/python
$PY -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    --after no_reset write_flash 0 ~/twatch-backup/<image>.bin
```

`--after no_reset` is not optional: without it esptool resets into the app,
Linux claims the USB-Serial-JTAG port, and later commands hang at
`Connecting...`.

## Three traps that cost a session each

**WiFi dying is usually the BLE radio, not the kernel.** The watch has paired
with a nearby BLE mouse at least once, and the BLE activity took WiFi down with
it: the interface stayed UP with the right address, gateway ARP present, IPC
interrupt climbing, RTC and PMU fine — and not a single packet passed, not even
to the watch's own gateway. Every kernel-side indicator said healthy. Do not
debug this as a network or driver fault: check `/run/wpa_supplicant.log` for a
recent reassociation and ask whether anything BLE is nearby. **Use
`./scripts/watch-exec.py` (USB console) for verification when WiFi is suspect**
— it does not share the radio and it proved the kernel was never at fault.
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
