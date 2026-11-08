# Escapement

Linux on a T-Watch Ultra. Real Linux 7.2.4 on the ESP32-S3's own Xtensa
cores — not emulated — running headless on a watch.

Named for the part of a mechanical watch that turns stored energy into
regulated time, which is roughly what a NOMMU kernel on 8 MB of PSRAM and one
core does.

## Status

**Works: Linux boots to userspace on the watch.** Verified 2026-11-08 over
JTAG, not by console (there is no console — see below).

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

There is no console, and this is the part that needs explaining.

The kernel's bootargs are `console=ttyGS3 console=ttyS0,115200n8`. The watch
has no UART bridge, so `ttyS0` is unreachable. `ttyGS3` is the S3's
USB-Serial-JTAG peripheral — the same one the ROM uses — and after handoff the
host never re-enumerates the device, so `/dev/ttyACM1` delivers nothing.

What does work is **JTAG**. `scripts/probe.sh` attaches openocd and samples
core 1's program counter after boot:

```
11  PC = 0x422f89aa     idle loop (`waiti 0` / `retw.n`)
 1  PC = 0x42646edd     inside rootfs.cramfs
```

The variation proves the core is live rather than wedged, and a PC inside the
rootfs proves userspace was executing — that port XIPs userspace straight from
cramfs. That is the whole evidence base for "it booted", and it is honest:
a repeated PC alone proves nothing, because openocd's `init` resets both cores.

## Not done

- **No display.** Needs a CO5300 driver. The most promising shape given this
  port's architecture is core 0 drawing and handing finished framebuffers over
  the shared-memory channel that already carries wifi frames.
- **No input.** `CONFIG_INPUT` is off in the kernel config.
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
