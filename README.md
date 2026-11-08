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

**Works: the RTC.** A PCF85063A at I2C 0x51 registers as `rtc0` and
`CONFIG_RTC_HCTOSYS` seeds the system clock before init:

```
rtc-pcf85063 0-0051: registered as rtc0
rtc-pcf85063 0-0051: setting system clock to 2026-11-09T02:45:18 UTC
i2c-gpio i2c0: using lines 579 (SDA) and 578 (SCL)     # pins 3 and 2
```

The stored value was UTC+8 — the factory set the chip to China Standard Time,
so it needed one correction. Reads and writes both work: the chip kept exact
time over a 60 s sample (02:52:05 → 02:53:05), and `scripts/rtc-writeback.sh`
closes the loop so the corrected clock is written back at shutdown.

```
time rate     60 s sample, 60 s apart, no drift
chip write    hwclock -w -u            (BusyBox: no --set, -s/-w/--systz only)
boot           hctosys seeds the kernel from the chip
shutdown       /etc/init.d/S01hwclock  writes it back
```

Verified by corrupting the chip on purpose: set to 20:30 while true UTC was
19:00, corrected the *system* clock only, rebooted — and the next boot reported
`setting system clock to 2026-11-08T19:01:00 UTC`. The chip had followed.

**Does not: display anything.** No CO5300 driver exists in the port, the
kernel has no framebuffer or VT, and core 0 does not bring up the PMU, so the
AMOLED is not even powered. This is a headless Linux in a watch, for now.

<details>
<summary>The short version of how it got there</summary>

The upstream release requires **octal** PSRAM. This watch has **quad**, so the
stock image aborts before Linux starts. One firmware config change fixes it.
`docs/bring-up.md` has the whole story, including the three traps that cost
time.

</details>

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
verification approach for a board with no serial console, and — eventually —
the display and input drivers.

## Hardware

LilyGo T-Watch Ultra:

```
SoC      ESP32-S3, 2x Xtensa LX7, 240 MHz
RAM      512 KiB internal + 8 MiB PSRAM, QUAD (not octal)
Flash    16 MiB QSPI, DIO, 80 MHz
Display  CO5300 AMOLED, 410x502, QSPI      -- no driver
Touch    CST9217, I2C 0x1A                 -- no driver
PMU      AXP2101          -- not brought up by the port
Expander XL9555           -- not brought up by the port
```

The relevant difference from the supported board: **quad PSRAM**. Everything
else being different is why there is no screen.

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

## Not done

- **No display.** Needs a CO5300 driver. The most promising shape given this
  port's architecture is core 0 drawing and handing finished framebuffers over
  the shared-memory channel that already carries wifi frames.
- **No input.** `CONFIG_INPUT` is off in the kernel config.
- **The RTC's stored time is UTC+8.** The chip counts correctly but the factory
  set it to China Standard Time. It is exactly 8 h ahead of true UTC, so it
  needs one `hwclock --set` to correct. Not a driver fault.
- **No console.** Enabling the `ttyGS3` getty means editing `/etc/inittab` on
  the jffs2 partition, which needs mtd-utils built from source.
- **No PMU or expander bring-up** on core 0, so no rails, no haptic, no battery.

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
