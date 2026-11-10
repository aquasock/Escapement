# Escapement

Linux on a T-Watch Ultra. Real Linux 7.2.4 on the ESP32-S3's own Xtensa
cores — not emulated — running headless on a watch.

Named for the part of a mechanical watch that turns stored energy into
regulated time, which is roughly what a NOMMU kernel on 8 MB of PSRAM and one
core does.

## Status

**Works: Linux boots to userspace, logs in, and forks.**
Verified on hardware 2026-11-08. `buildroot login:` appears on the USB console,
root authenticates, and an interactive bash session starts — which is the real
test, because bash forks and this port's `fork()` is a patch stack, not an MMU.

**Works: the PMU.** The AXP2101 at I2C 0x34 loads and reports the battery:

```
capacity    = 100
voltage_now = 4182000      uV
status      = Full
```

No Linux driver for the AXP2101 exists in mainline — not here, not upstream — so
`patches/kernel/axp2101/` carries the pending series plus three local fixes.

**Both supplies register, and the interrupt works:**

```
/sys/class/power_supply/    axp20x-battery  axp20x-usb
capacity                    100
voltage_now                 4175000        uV
status                      Full
usb online                  1

/proc/interrupts
  8:  esp32xx-gpio       7  axp2101_irq_chip
 17:  axp2101_irq_chip   8  axp20x-pek-dbr
 18:  axp2101_irq_chip   9  axp20x-pek-dbf
 23:  axp2101_irq_chip  14  axp20x-usb-power-supply
 24:  axp2101_irq_chip  15  axp20x-usb-power-supply
```

`axp20x-pek` also appears as an input device, so the top-left pusher reaches
Linux.

**Works: the RTC.** A PCF85063A at I2C 0x51 registers as `rtc0` and
`CONFIG_RTC_HCTOSYS` seeds the system clock before init:

```
rtc-pcf85063 0-0051: registered as rtc0
rtc-pcf85063 0-0051: setting system clock to 2026-11-09T02:45:18 UTC
i2c-gpio i2c0: using lines 579 (SDA) and 578 (SCL)     # pins 3 and 2
```

The stored value was UTC+8 — the factory set the chip to China Standard Time,
so it needed one correction. Reads and writes both work: the chip kept exact
time over a 60 s sample (02:52:05 → 02:53:05), and the shutdown write-back
closes the loop so a corrected clock is pushed down to the chip.

```
time rate     60 s sample, 60 s apart, no drift
chip write    hwclock -w -u            (BusyBox: no --set, -s/-w/--systz only)
boot           hctosys seeds the kernel from the chip
shutdown       /etc/init.d/S01hwclock  writes it back, from rcK
```

**The image ships no timezone data at all.** No `/etc/TZ`, no `/etc/localtime`,
no `/usr/share/zoneinfo` — so out of the box `date` prints UTC and the console
clock is seven hours off the wall. `/etc/TZ` holding the POSIX string `MST7`
is enough: uClibc-ng reads it whenever `TZ` is unset, which is how the console's
shells run. America/Phoenix is `MST7`, UTC-7 all year with no DST — not
`MST7MDT`, which is an hour wrong every summer.

`scripts/watch-time.sh` installs both files and verifies them. It has to be
re-run after any **full-image** flash, because `/etc` is rewritten from the
image and `/etc/init.d/S03keepconfig` only carries seven named files across
(`wpa_supplicant.conf`, `shadow`, `passwd`, `group`, `inetd.conf`,
`remote-login`, `remote-login.conf`, plus the dropbear keys). A kernel-only
flash leaves `/etc` alone and needs no re-install.

The script also disables the upstream image's `/etc/init.d/S01clock`. That is not
a second clock — `/sys/class/rtc/` holds `rtc0` and nothing else — but a note:
it saves the epoch to `/home/.clock` at shutdown and writes it back over the
system clock at boot whenever the note is newer, which is *after* `hctosys` has
seeded from the chip. Set 90 minutes ahead, it moved the clock to 19:42 against
a true 18:12. Disabled by decision: the PCF85063A is the single authority, and
if it dies then the time is wrong and should say so. It is moved to
`/etc/disabled/S01clock` rather than renamed, because `rcS` and `rcK` glob
`S??*` and `S01clock.disabled` would still run.

Verified by corrupting the chip on purpose: set to 20:30 while true UTC was
19:00, corrected the *system* clock only, rebooted — and the next boot reported
`setting system clock to 2026-11-08T19:01:00 UTC`. The chip had followed.

**Works: the Tamagotchi V3.** The real 512 KB Tamagotchi Connection V3 ROM runs
on the AMOLED at about 1.0x realtime — the egg wiggles and hatches. The core is
not this project's work: it is TamaHarness, a bit-exact SPLB32 emulator whose
correctness is established separately. What this project contributes is a place
to run it: the ROM lives **raw in flash** at `0x480000` and is read through the
kernel's linear flash window at `0x42480000`, so it costs no RAM at all and
starts at any uptime. That matters because a 512 KB `malloc` succeeds on a fresh
boot and fails once the heap fragments — see `docs/tamagotchi.md`.

**Works: a text console on the AMOLED.** Black background, green text, blinking
cursor, 40 columns by 25 rows:

```
Console: switching to colour frame buffer device 40x25
```

That is the VGA 8x16 font — the IBM PC text font, 8 px wide in a 9 px cell —
which is what "the official console font" means in practice. The kernel picks
it by default.

The framebuffer is **320x400**, not the whole glass, placed at offset (45, 45).
The panel's corners are 45-degree chamfers, so the largest unclipped rectangle
is 320x412 (`docs/panel-geometry.md` has the measurement). Keeping fb0 to the
console area means fbcon does the column/row arithmetic itself and the 45 px
border stays outside the framebuffer, reserved for notification strips. That
leaves 57 rows below fb0, which is where the touch buttons live.

The four strips around fb0 are reserved by a single specification,
`docs/panel-reservations.md`: a 45 px module, a top status bar that owns the full
width, seven 45x45 app slots down each side, and the three buttons across the
bottom. It is a spec rather than an implementation — only the bottom bar is
painted today — but it fixes the geometry every later feature reads from. The
calling convention for writing into those strips is a second spec,
`docs/panel-api.md`: one ioctl on `/dev/fb0` that takes RGB565 pixels and a
panel-coordinate rectangle, and refuses any rectangle that would touch the
console.

**Works: three touch buttons below the console.** The bottom strip outside fb0
is painted red | green | blue, full width, and each third is a real input
device:

```
/sys/class/input/inputN/name    touch-buttons
/dev/input/event3               EV_KEY 256 (BTN_0) | 257 (BTN_1) | 258 (BTN_2)
```

Verified on hardware 2026-11-09 by pressing each bar and reading the raw event
stream back: `type=1 code=256 value=1`, `SYN_REPORT`, `value=0` for red, the
same with 257 for green and 258 for blue, 13 presses and 13 releases in the
order they were made. They behave exactly like the two side pushers — three
on/off switches, no pointer.

This is a **userspace** daemon (`userspace/tbtns.c`), not a kernel driver. The
running kernel already provides everything needed — `/dev/i2c-0`, `/dev/uinput`,
and the XL9555 as a gpiochip — so there is no `CONFIG_INPUT_TOUCHSCREEN`, no
device-tree touch node, and changing a band boundary is a flag rather than a
rebuild and a flash. The bars are drawn by the driver (`co5300_paint_frame()`),
because the strip is outside fb0 and only the driver can paint there.

It runs as `S50tbtns` in `/etc/init.d`, started by rcS like the keyboard daemon.
That script is the reason the buttons survive a reboot: the daemon works
perfectly but had only ever been launched by hand, so every reboot since it was
written quietly lost it. Deploy it with `scripts/watch-deploy.sh` after a
full-image flash, since `/etc` is rewritten by one.

Three things about the CST9217 at I2C 0x1A that cost time and are worth knowing:
`0x5A` on the same bus is the DRV2605 haptic driver, not touch; the report block
must start with `0x06`, where a `0xAB` in byte 0 means "no new report" and will
otherwise read as a phantom press at a fixed coordinate; and the block must be
read **whole**, all fifteen bytes. That last one is subtle and was the worst bug
here: the acknowledge that advances the controller's buffer is issued against
the block, so reading seven bytes of it left the buffer mid-block and a finger
held on the glass produced one position, then silence, then a spurious release.

Known limitation: the controller reports on a 66 ms cadence and `tbtns` polls at
the same rate, so a tap shorter than one interval can fall between samples —
nine readable reports from ten quick taps. A deliberate press always lands.

**Works: the display.** The CO5300 AMOLED is a `spi_mem` client on SPI2 with an
fbdev framebuffer:

```
co5300: TE edges after init = 4 *** CONTROLLER IS SCANNING ***
co5300 spi0.0: 410x502 RGB565, fb0 registered
```

Verified on hardware 2026-11-08 by drawing **eight horizontal bands** — white,
red, green, blue, yellow, magenta, cyan, grey — and getting eight correct bands
in the right order. The pattern matters: a flat fill renders identically whether
the panel received all 502 rows or row 0 five hundred times, so only a pattern
that varies along y can prove row addressing. That trap cost this project time
once already; see `patches/kernel/display/STATUS.md` for the five driver bugs
and the two instruments built to find them.

<details>
<summary>The short version of how it got there</summary>

The upstream release requires **octal** PSRAM. This watch has **quad**, so the
stock image aborts before Linux starts. One firmware config change fixes it.
`docs/bring-up.md` has the whole story, including the three traps that cost
time.

</details>

**Works: a Bluetooth LE keyboard types into the console.** The ESP32-S3 finds
a Logitech BLE keyboard, pairs, and forwards its boot-protocol reports to
Linux, where a small daemon turns them into input events. Typing `ls` on the
keyboard runs `ls` in a shell on the AMOLED — verified on hardware 2026-11-09 by
reading the command and its output off the watch's own screen.

The HID host runs in the firmware, not in Linux: the kernel's `CONFIG_BT_HIDP`
is Bluetooth *Classic* only, HID-over-GATT lives in BlueZ, and BlueZ cannot run
on this NOMMU board.

**The pairing survives a reboot.** The firmware cannot write flash after Linux
starts (an NVS write parks the other core, which is running Linux, and aborts),
so the split is forced: Linux writes `/etc/ble-bond.bin` when a pairing
completes, and the firmware reads it at boot and restores the keys before it
scans. The keyboard then reconnects on the stored key without being put into
pairing mode. `docs/bluetooth-bond.md` records the three defects that hid each
other before this worked — the last of them was that the reader was trying to
copy a 448 KiB partition into PSRAM that this firmware does not expose as a heap.

**The current keyboard is a Rii K06 combo**, in its Bluetooth LE mode. It
pairs, encrypts, and types. It exposed one more defect worth knowing about,
because the failure looked like "connected but no keys" and the cause was a
subscription rule: the K06 *exposes* the Boot Keyboard Input characteristic
`0x2A22` and never sends anything on it — its boot-format reports arrive on
report characteristic `0x001E` instead. The host used to subscribe **only** to
the boot input when one existed, reasoning that one CCCD is cheaper than six.
That is wrong for this device and silent when it fails. The rule now is the one
TinyTang uses on the same hardware: subscribe to the boot input *and* every
notifying input report, so reports are seen whichever way the device sends them.

```
hid: connect (0)
hid: connected, security initiate (0)
hid: protocol mode -> boot (0)
hid: subscribed handle 0x0015, 0x001a, 0x001e, 0x0025, 0x002c
```

Getting there meant five separate defects, each fatal on its own, and the last
one failed *silently* in a way that looked like success. The whole story,
including the false positive and why boot protocol rather than report protocol
is the only workable choice here, is in `docs/bluetooth-keyboard.md`.

## Attribution

This project is watch bring-up and hard-won configuration, not a kernel port.
The port is other people's work and the credit is theirs:

- **[paulneja/Linux-on-esp32-S3](https://github.com/paulneja/Linux-on-esp32-S3)**
  — the distribution: userspace, fork backend, build system, releases.
  Escapement is built on release 0.9.1.
- **[jcmvbkbc](https://github.com/jcmvbkbc)** (Max Filippov) — the Xtensa Linux
  port, `esp-hosted`, and the two-core AMP arrangement this depends on.
- Espressif — ESP-IDF, and the closed WiFi blob that forces the two-core split
  in the first place.

What is here: the hardware delta that makes it run on *this* board, the
verification approach for a board with no serial console, and the display
driver.

## Hardware

LilyGo T-Watch Ultra:

```
SoC      ESP32-S3, 2x Xtensa LX7, 240 MHz
RAM      512 KiB internal + 8 MiB PSRAM, QUAD (not octal)
Flash    16 MiB QSPI, DIO, 80 MHz
Display  CO5300 AMOLED, 410x502, QSPI      works (fb0 + console, SPI2)
         usable 320x412 (45-degree chamfered corners)
Touch    CST9217, I2C 0x1A                 -- no driver
PMU      AXP2101, I2C 0x34                 works (battery)
Expander XL9555, I2C 0x20                  works (display power)
RTC      PCF85063A, I2C 0x51               works (rtc0)
Haptic   DRV2605, I2C 0x5A                 works (effect fires)
```

The relevant difference from the supported board: **quad PSRAM** — that is the
one thing that stopped the stock image booting at all.

## Building

Needs git, cmake, ninja, python3, and about 6 GB of disk for the toolchain.
`scripts/build.sh` does the whole path.

```sh
./scripts/build.sh            # fetch pinned sources, patch, build core 0
```

The script pins to what 0.9.1 was built from — project `0.9.1`, `esp-hosted`
`e2f19ed`, jcmvbkbc's IDF fork `24394052` (v5.1.1) — and installs that fork's
toolchain into `build/idf-tools/` so it never touches any other ESP-IDF
install on the machine.

Two things it handles that are easy to get wrong by hand:

- `esp_driver/CMakeLists.txt` runs `git reset --hard` on every cmake
  invocation, so the patches have to be committed locally or they vanish.
- The patched wifi libraries in `esp_driver/lib/esp32s3/` must be copied over
  IDF's stock ones. They carry `esp_wifi_connect_internal` and friends; without
  them the link fails, and the error names wifi symbols rather than the real
  cause.

## Flashing

Bootloader, partition table and the kernel/rootfs come from the 0.9.1 release
image. Only the app changes, so only the app gets flashed:

```sh
./scripts/flash.sh            # writes the 16 MiB image once
./scripts/flash.sh --app      # after a firmware rebuild: 0x10000 only
```

## Watching it run

The console is `ttyGS3` — the S3's own USB-Serial-JTAG peripheral — so the port
you flash is the port that prints:

```sh
screen /dev/ttyACM1 115200
```

Getting there took two fixes, both in `patches/0002`, and both worth knowing
because each one looks like something else:

1. **`quiet` was suppressing everything.** The dts bootargs carry it, dropping
   the console log level to 4. On a board whose only output path is USB, that
   turns a booting kernel into silence — indistinguishable from a kernel that
   never started. Two sessions were spent sampling core 1's program counter over
   JTAG to guess whether Linux was alive, while the kernel was talking the whole
   time.

2. **`/dev/console` was bound to the wrong device.** With
   `console=ttyGS3 console=ttyS0,115200n8`, `/dev/console` binds to the *last*
   one — `ttyS0`, a UART this board does not have. printk mirrors to every
   registered console, so the log appeared on USB while init's stdin/stdout went
   to the dead UART. A shell was running and reading from hardware that was not
   connected. Putting `ttyGS3` last fixes it, and is also what makes the stock
   `/etc/inittab` work with no editing: its `console::respawn:...getty` line
   then lands on the USB port.

### One thing to fix after the first boot: Enter

This board's console comes up with `ICRNL` **off**. Your terminal sends Enter as
CR; canonical-mode reads need NL. With the translation off, the line discipline
never sees a line ending, so any prompt that turns echo off — `login`, `passwd`,
anything using `getpass` — accepts keystrokes and then waits forever on a line
that can never end. Interactive shells hide it, because readline accepts CR
directly; password prompts do not. The symptom is a console that looks dead at
exactly the moments you most need it.

Confirmed on hardware:

```
$ stty -a
speed 9600 baud; line = 0;            <- termios never properly initialised
... -inlcr -igncr -icrnl ixon ixoff   <- the culprit
```

Two ways to work around it, and one to fix it:

- **Workaround, no setup:** press `Ctrl-J` (LF) instead of Enter at a prompt.
- **Fix for the current session:** `stty icrnl`
- **Fix permanently:** put it in an init script, which is what
  `scripts/console-fix.sh` writes:

```sh
#!/bin/sh
stty icrnl < /dev/console 2>/dev/null
```

Save as `/etc/init.d/S01termios`, `chmod +x`, reboot. `rcS` runs it before init
starts the getty, so the login prompt gets the corrected flags. Verified: after
a cold boot, typing `root` and pressing Enter reaches `Password:` instead of
hanging.

This lives on the writable `etc` partition, so an image update from upstream
will not carry it. Re-run it after one.

Upstream documents `usb-console on` as the way to enable this, which is needed
on their devkit because there `ttyS0` is last and `/dev/console` is the UART.
This board has no UART to fall back to, so the ordering fix is the whole job.

`scripts/probe.sh` still exists for when the console is unusable: it samples
core 1's program counter over JTAG. A PC inside `rootfs.cramfs` proves userspace
ran; a single repeated PC proves nothing, because openocd's `init` resets both
cores.

**Firmware diagnostics are on `/dev/esp-log`.** Linux takes the USB console at
about 1.75 s into boot, so every ESP-IDF log line after that goes to a port
Linux owns — which means nothing in the firmware was diagnosable exactly when
WiFi, the IPC link and the BLE stack are running. The firmware tags those lines
and sends them over the same shmem link as a separate interface, and the kernel
delivers them here:

```sh
cat /dev/esp-log
@LOG hid: scan 1 complete (connected 0)
@LOG hid: scan requested by Linux
```

This only started working after an interface-enum mismatch was fixed: the
firmware fork added `ESP_LOG_IF` as interface 6, the kernel's enum stopped at 6,
and every log frame was being dropped as `bad if_type 6`. The driver that reads
the stream was also never compiled, so the device did not exist. Both are
`patches/kernel/0007`.

## Not done

- **The console has no local input of its own.** The two side pushers emit
  keycodes (`KEY_POWER`, `KEY_ENTER`) but nothing consumes them, and the three
  touch buttons below the console emit `BTN_0/1/2` for the same reason. Typing is
  via the Bluetooth keyboard, or the USB port.
- **BLE media keys are untested, and probably close.** The host now subscribes
  to every notifying input report, not just the boot input, so the consumer-page
  reports that carry volume and browser keys do reach Linux, and `blekbd` has a
  consumer usage map to decode them. What is missing is the report map: the
  firmware's `map_step()` is not called, so the daemon falls back to the boot
  layout and cannot tell a report ID from a keycode. Reading the map is what
  exhausted the ATT buffers once before (BLE-002), so if it is added it has to
  be chunked and serialised.
- **The console is slow to repaint.** Every fbcon drawing op goes straight to
  the panel as one spi-mem op through a 64-byte FIFO, with no DMA and no dirty
  region tracking. Correct first; fast later.
- **Display is write-only and synchronous.** Every blit sends the whole window
  as one spi-mem op through a 64-byte FIFO, with no DMA and no double buffering.
  It works; it is not fast.
- **Wake-from-sleep is not wired to anything.** The PMU interrupt works and the
  VBUS hotplug interrupts are registered, but nothing acts on them yet: there is
  no suspend path, so the watch does not sleep at all. That is a power
  management project, not a driver gap.
- **The PMU's fuel gauge is not driven.** `capacity` and `voltage_now` come from
  the registers directly (see `patches/kernel/axp2101/0009`); there is no
  battery model, so there is no time-to-empty and no charge-cycle estimate.
- **The RTC's stored time has been corrected once.** The factory set the chip
  to China Standard Time, exactly 8 h ahead of true UTC. It has since been
  corrected and the chip now holds true UTC, verified against the host clock.
  Not a driver fault, and no longer outstanding.
- **`/etc` does not survive a full-image flash.** The image rewrites it, and
  `/etc/init.d/S03keepconfig` carries back only a named list. The timezone file
  and the RTC write-back script have to be re-installed with
  `./scripts/watch-time.sh` after one. Kernel-only flashes are unaffected.
- **A very quick tap can be missed.** The controller reports on a 66 ms cadence
  and `tbtns` polls at the same rate, so a tap shorter than one poll interval
  can fall between samples. Measured at ten quick taps: nine readable reports
  and eight presses. A deliberate press always lands, and the poll interval is
  left alone rather than tightened, because this part's reads are already the
  weak link.
- **Read the block whole, or a held touch freezes.** This cost a round and is
  worth stating plainly: the acknowledge that advances the controller's report
  buffer is issued against the block, so reading seven bytes of a fifteen-byte
  block left the buffer mid-block. The symptom was a finger held on the glass
  producing one position, then silence, then a spurious release. With the full
  block the transfer failure rate is about 2 percent, not the 75 percent an
  earlier seven-byte reader measured — that figure was the bug, not the part.
- **`tbtns` cannot pulse the controller's hardware reset.** The XL9555 at 0x20
  is owned by `pca953x`, so a plain `I2C_SLAVE` on that address returns `EBUSY`
  and the reset is skipped; the daemon brings the part up in software instead.
  It has worked on every boot so far, but the hardware path is unused.
- **The buttons have no names yet** beyond the kernel keycodes they emit:
  Pusher A (top left) is `KEY_POWER`, Pusher B (bottom left) is `KEY_ENTER`.

## Recovery

The watch always takes a full factory image back. BOOT + replug reaches
mask-ROM download mode regardless of what is on flash.

`scripts/flash.sh --factory` writes it back, and refuses to unless the file is
16 MiB and its hash matches:

```
sha256  471c9a21a6d53e5cdf6dfb01949fcc2dd4f420fe1a3738631f73b600419ac2b0
```

That hash is *this* watch's image. Yours will differ — read it off before
flashing anything (`esptool.py read_flash 0 0x1000000 factory.bin`) and point
`FACTORY` and `FACTORY_SHA` at it.

If it goes wrong badly, that is the way back — not a partial reflash, since
this image replaces the whole partition layout.

## Licence

Escapement's own files: GPLv3, matching upstream. The kernel, rootfs and
firmware it builds come from their own sources under their own licences; see
upstream's `NOTICE` for the full list.
