# Escapement

A desktop on a wristwatch. **[TinyDesk](https://github.com/tinydesk-project/tinydesk)**
runs on a LilyGo **T-Watch Ultra** — the ESP32-S3's own cores, its own AMOLED,
its own touch panel — with no PC and no terminal emulator in between. The
desktop is drawn on the watch and the watch is what you touch.

40x26 cells of overlapping, draggable windows on a 410x502 AMOLED, driven by a
finger, a Bluetooth keyboard, or a real Xbox controller. A shell in a Terminal
window. Real emulators. Music out of the built-in speaker.

Named for the part of a mechanical watch that turns stored energy into
regulated time: a small mechanism doing a precise job with almost nothing to
work with.

---

## Status

**Works, and this is the shipping configuration.** Verified on hardware
2026-11-16.

```
desktop      40x26 at 320x416 on the AMOLED, touch and margins live
shell        TinyDesk Shell in a Terminal window, plus telnet and SSH/SFTP
input        BLE keyboard and mouse, BLE gamepad, USB keyboard, the margins
audio        FLAC from the SD card out of the built-in speaker
emulators    Game Boy / GBC, Pokemon Mini, Tamagotchi V3
radio        WiFi (telnet 23, SSH 22), BLE, NFC card reads, GNSS
power        AXP2101 gauge and charging, and an RTC chip, so the clock is right
             with no network
```

The NES is deliberately not in that list: see **The NES** under [What it
does](#what-it-does). The app image is `0x2FAF80` bytes (2.98 MiB) against a
`0x3E0000` OTA slot, leaving 916 KB free, so there is room to keep going.

## How it got here — and that Linux is not coming back

This project started as **Linux on the watch**: a real, not-emulated Linux
7.2.4 running on the ESP32-S3's Xtensa cores, booting to userspace and to a
bash login on the USB console. That genuinely worked, and there is a lot of
hard-won material about it in this repository.

**It was too heavy.** A NOMMU kernel on 8 MB of PSRAM, plus a rootfs, plus a
userspace, leaves almost nothing over — and the display, touch and power drivers
all had to be written from scratch to get that far. TinyDesk does the same job
by not needing an operating system at all: it is a portable C11 desktop drawn
with ANSI escape sequences, and this port simply gives it the watch's own panel
and touch panel as its terminal.

So the Linux line is **closed**, by decision, on 2026-11-11 (`8645a82`,
"close the Linux line of work"). The kernel patches, the rootfs scripts and the
probes remain in this repository as an archive — they are how the board's
hardware was mapped, and several measurements in them are still the only
measurements of this glass. **Nothing in this repository's `patches/`,
`userspace/`, `probes/` or `scripts/build-*.sh` is part of what runs on the
watch today.** See [What is in this repository](#what-is-in-this-repository).

## Where the code lives

The working code is a **fork of TinyDesk**, not this repository:

```
active code   /run/media/vash/GIT/tinydesk-twatch   branch t-watch-ultra
              remote: fork = aquasock/tinydesk      (the upstream is read-only)
this repo     /run/media/vash/GIT/Escapement        branch main
              project control: the decision log, the hardware reference,
              and the Linux-era archive
```

`ports/twatchultra/` in the fork is the whole port. Its `README.md` is the
authoritative subsystem-by-subsystem record and is far more detailed than this
file; read it before changing anything.

TinyDesk itself is MIT-licensed and not this project's work. What this project
contributes is the watch port, the measurements, and the parts of the board
nothing else drives.

## The watch

LilyGo T-Watch Ultra. The relevant difference from the boards TinyDesk already
supported is **quad PSRAM, not octal** — and on this port that is a setting that
either boots or produces a dead board with no explanation.

```
SoC      ESP32-S3, 2x Xtensa LX7, 240 MHz
RAM      512 KiB internal + 8 MiB PSRAM, QUAD
Flash    16 MiB QSPI, DIO, 80 MHz
Display  CO5300 AMOLED, 410x502, four-lane QSPI, write-only, no scanout
Touch    CST9217, I2C 0x1A, interrupt on GPIO 12
PMU      AXP2101, I2C 0x34 — display rail ALDO2, speaker BLDO2, GNSS BLDO1,
                             NFC DLDO1, SD ALDO1
Expander XL9555, I2C 0x20 — panel power P07, haptics P06
RTC      PCF85063A, I2C 0x51
Haptic   DRV2605, I2C 0x5A, ERM motor
Audio    MAX98357A I2S amplifier — BCLK 9, WS 10, DOUT 11
NFC      ST25R3916 on the second SPI bus (not a PN532, not on I2C)
GNSS     u-blox MIA-M10Q, UART1 38400 8N1, TX 43 / RX 44, PPS 13
```

Every peripheral except the panel shares one I2C bus, which is why the touch
address is spelled out in `board.example.conf` rather than probed.

### The flash layout

The stock layout is replaced. `ports/twatchultra/partitions.csv` in the fork is
the authority; the shape is two OTA slots (3 MB and 3008 KB), a 7.9 MB littlefs
mounted at `/`, and two 1 MB raw partitions for codec blobs:

```
nvs       0x9000    24K
phy_init  0xf000     4K
ota_0     0x10000    3M     app (the running image, 0x2e3090 used)
ota_1     0x310000 3008K    the A/B slot and the real size budget
coredump  0x600000   64K     panic backtraces — the only way a panic reports
otadata   0x610000    8K
storage   0x620000 8064K    littlefs, mounted at /
modules   0xe00000    1M     code, mapped as instructions
moddata   0xf00000    1M     constants, mapped as data
```

`modules` and `moddata` exist because the app image could not hold a second
codec: one FLAC decoder cost 31.7 KB of a 27 KB headroom. Code in them runs
straight from flash by XIP, costing no internal SRAM to load — which is this
board's scarce pool. They are two partitions, not one, because the ESP32-S3's
memory-mapped flash windows are complementary: the instruction window cannot
serve a data load, and the data window cannot be executed.

## What it does

**The desktop.** 40 columns by 26 rows of 8x16 cells, drawn into a 320x416
window inside the 410x502 glass — the corners are 45-degree chamfers, so the
usable rectangle is smaller than the panel. Only changed cells are pushed, as
rectangles rather than row runs; the difference between the two was 96% of a
framebuffer per mouse move.

**Input.** A BLE keyboard and mouse pair from the Bluetooth app and survive a
reboot in NVS. A BLE Xbox controller works for the emulators, discriminated by
its HID top-level collection so it is not mistaken for a mouse. There is also a
USB keyboard on the watch's own port, and the painted margins: a left column of
keyboard actions, a bottom strip of Tab/Enter/back, and a right column of
arrows. Those margins are live touch targets whose geometry lives in one header
(`edges.h`) shared by the painter and the hit test, so a button cannot drift
from what it does.

**A shell.** TinyDesk Shell in a Terminal window, reachable over USB serial,
telnet on 23, or SSH/SFTP on 22. `tdsh` is its own shell — not bash.

**Emulators.** Game Boy and Game Boy Color (gnuboy), Pokemon Mini, and the
Tamagotchi Connection V3. The Game Boy core measures 116 fps against a 59.7 fps
target, nearly twice real time. Which core to vendor was decided by running them
on the watch, not by reading source — `docs/gbc-scoping.md`,
`docs/pokemini-scoping.md` and `docs/nes-accuracy.md` are those measurements.

**The NES.** No longer offered anywhere, and the reason is the glass rather than
the emulator. nofrendo runs at about 21 fps with the frame split evenly between
the 6502 interpreter and the blit, and it is playable — but 320x240 of console
output on a 410x502 wrist display is not something a wearer can read, and a menu
that offers it is a promise this port should not make. Both Start menu entries
were retired (`0cc6c8f`, decision-log entry 31). **The code is kept**: `nes.c`,
`components/nofrendo` and `docs/nes-accuracy.md` are untouched, `nes` still runs
from a Terminal in the desktop, and reviving it is two menu registrations — the
row in slot B's app table and the `s_app_nes` entry in the desktop's app list.

**Music.** `music <file.flac>` decodes FLAC from the SD card out of the
built-in speaker, with the decoder running out of the `modules` partition. A
5:58 track measured 15,792,504 frames with zero errors and zero short writes,
and internal SRAM at 20,707 bytes free during playback. The decoder is
micro-flac; its rate comes from the file, so 48 kHz plays at 48 kHz.

**Radio.** WiFi gives telnet and an SSH server. NFC reads cards. GNSS tracks
satellites and can set the watch's clock from them — which works indoors,
because a time needs ephemeris and not a fix. BLE is both the input path and an
alternate console.

**Slot B, a second personality from the same image.** The same firmware is
flashed into both OTA slots, and which slot the bootloader picked decides what
runs: `ota_0` is the desktop, `ota_1` is a bare emulator personality with no
desktop, no shell, no WiFi and no SSH — just the three emulators and the display,
touch, audio and card drivers they need. Choosing an emulator from the Start
menu reboots into it, so the wearer sees black and then a game, never a boot
sequence. That is worth a boot because it hands back the watch's scarcest
resources: no `esp_wifi_init()`, no SSH heap floor, no littlefs mount, and no
desktop painter or shell stacks competing with the emulator for a core. Pusher A
backs out — to slot B's own menu when that is where the wearer came from, and to
the desktop otherwise — and a slot B that dies before an emulator starts is
rolled back to the desktop by the bootloader on the next reset, so it cannot
brick the watch. The whole of it — the boot path, the app table, the
peripheral bring-up order and the flash instruments it needs because it has no
console — is in the fork's `ports/twatchultra/README.md`.

## Building

The port is an ESP-IDF project and needs **ESP-IDF 5.3.1** plus an
`xtensa-esp32s3` toolchain. Everything else is in the fork.

```sh
cd /run/media/vash/GIT/tinydesk-twatch/ports/twatchultra
idf.py set-target esp32s3
idf.py build
```

There is no baud argument: the link is the S3's own USB Serial/JTAG, and line
speed is ignored.

Two gotchas worth knowing before the first build:

- **The IDF environment here does not come from `export.sh`.** It fails on a
  missing `riscv32-esp-elf-gdb` component and leaves `idf.py` off the path. Set
  the variables directly:

  ```sh
  export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
  export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
  export IDF_PYTHON_ENV_PATH=$IDF_TOOLS_PATH/python_env/idf5.3_py3.14_env
  export PATH=/home/vash/esp/idf-tools/tools/xtensa-esp-elf/esp-13.2.0_20240530/xtensa-esp-elf/bin:$IDF_PYTHON_ENV_PATH/bin:$PATH
  ```

- **`CONFIG_SPIRAM_MODE_OCT` is the dead-board trap.** This SoC has quad
  PSRAM; copy an octal target's config and the board stops before anything can
  report it. `sdkconfig.defaults` sets `CONFIG_SPIRAM_MODE_QUAD=y`.

### Building the audio module

The FLAC decoder is not in the app image. It is a blob in flash, and it must be
rebuilt whenever the app image changes size — the mmap addresses it links
against move when it does.

```sh
cd ports/twatchultra/modules
make modaddr     # re-read the mapping addresses off the board
make flac        # -> build/flac.bin + build/flac.data.bin
```

**The order is not optional.** The addresses are chosen by the MMU at boot, so
`make modaddr` reads a fact that only exists after the new app has run once:

```
build the app -> flash it -> let it boot -> make modaddr -> make flac -> flash
```

A blob built for the wrong addresses still *executes* and then reads its
constants from the wrong place. The loader refuses that mismatch now and says
which addresses disagree, but the fix is to rebuild, and the symptom it prevents
was a decoder reporting "bad magic number" on files that were fine.

## Flashing

App only, which is the normal case:

```sh
esptool --chip esp32s3 -p /dev/ttyACM1 -b 921600 \
        --before default_reset --after hard_reset \
        write_flash 0x10000 build/tinydesk.bin
```

With the audio module, three regions:

```sh
esptool ... write_flash 0x10000 build/tinydesk.bin \
                        0xE00000 modules/build/flac.bin \
                        0xF00000 modules/build/flac.data.bin
```

Read the flash out before flashing a board that holds something you cannot
regenerate — a full 16 MiB image is the only recovery that restores the
partition layout as well as the code.

## Driving it

USB serial, 115200. The console is a desktop, not a getty, so getting to a
shell is a keystroke sequence rather than a login:

```
reboot  ->  wait for boot  ->  F10  ->  Enter  ->  type
```

F10 opens Start with Terminal selected; Enter opens it. That is the whole
recipe, and it has not changed.

Traps that cost real sessions:

- **The display mirror sends only changed cells.** A capture attaches with an
  incomplete screen. The full-redraw request is `ESC[5000~`.
- **The mirror is 40 columns and wraps mid-word.** A status line arrives as
  `fra mes`; strip whitespace before parsing it.
- **`tdsh` is not bash.** No `wc`, `head` takes `-l` not `-n`, and double
  quotes trip its parser (`unmatched quote or escape`). Use globs or bare paths.
- **Ctrl+Q is `0x11` = XON.** With `IXON` set on the host tty the line
  discipline swallows it, so "close the focused window" silently does nothing.
  Any serial helper must clear `IXON|IXOFF|IXANY`.
- **esptool leaves the console silent and at its own baud.** A fresh
  `chip_id --after hard_reset` brings it back.
- **An orphaned helper holding the port** makes every esptool call fail while
  the board looks healthy. `fuser /dev/ttyACM1` names it.

## What is in this repository

This repository is **project control**, not the firmware.

```
README.md            this file
.ai/core.md          project law (RESTRICTED)
.ai/core-log.md      the decision log — every cycle, what broke and why
.ai/core-reference.md  hardware and defect records, indexed and question-shaped
docs/                measurements; see docs/README.md for what is current
patches/             LINUX-ERA ARCHIVE — kernel and firmware patches
userspace/           LINUX-ERA ARCHIVE — the rootfs-side daemons
probes/              LINUX-ERA ARCHIVE — bare-metal ESP-IDF probes
scripts/             mixed: see docs/README.md
third_party/         vendored emulator and core sources
tools/               host-side benchmark harnesses for choosing cores
```

The Linux-era directories are kept because they contain the only measurements
of parts of this board — the panel geometry and its chamfers, the expander
register map, the touch handshake — and because a future reader is owed the
reason the line was abandoned rather than a silent gap.

A few things in `scripts/` are still live: `watch-backup.sh` (the full 16 MiB
read, which is the recovery artefact), `tamacore-host.sh` (runs the Tamagotchi
core on a host and checks its audio envelope) and `tamacore-sync.sh` (fails when
the fork's copy of the core drifts from this one). The benchmark harnesses under
`tools/` remain the record of how the emulator cores were chosen, though the
on-watch measurement is now a shell command (`gbcbench`, `pmbench`) rather than
a Linux binary. `docs/README.md` sorts every document and script into current
and archive.

**The decision log is the real documentation.** If you want to know why
something is the way it is, `.ai/core-log.md` records it along with what was
tried first and why that failed. It is long because it is honest.

## Not done

- **No picker, queue or seek for music.** `music` takes a path and plays it.
- **One codec.** FLAC only. The partition layout has room for eleven Rockbox
  codecs; nothing else is ported, and the loader holds a single blob.
- **The NES is backburnered, not shipping.** It runs at about 21 fps with the
  blit a large share of the frame, and a direct path from the core's palette
  output into the flush buffer is the known fix — but the blocking problem is
  that 320x240 is illegible at wrist distance, so no speed work is planned.
- **No NES save persistence** — nofrendo keeps battery saves in RAM only. Moot
  while the emulator is off the menus.
- **The margins are wired per emulator, and the blanks are honest.** Game Boy,
  Pokemon Mini and the Tamagotchi each fill the slots they use and leave the rest
  drawn-but-empty rather than advertising a duty they do not have. The NES still
  draws `Pause` and `Reset` with no handler, and `SELECT` is unmapped, which a few
  games need — both moot while it is off the menus.
- **Bluetooth media keys are untested.** The consumer-page reports arrive; the
  report map is not parsed, so they cannot be decoded.
- **No suspend.** The PMU interrupt works and nothing acts on it, so the watch
  never sleeps. That is a power-management project, not a driver gap.
- **The battery gauge has no model** — a percentage and a voltage, no
  time-to-empty.
- **No OTA feed for this board.** The image is marked so a UART board's update
  cannot replace it, but nothing updates it over the air yet.
- **The USB-C DAC path is not in the player.** `uac2` — the watch's USB port as
  an OTG host driving a USB audio DAC — is built and qualified, and it was left
  out of `music` on purpose: the built-in speaker was the thing to prove first
  and it is proven. Re-adding it is a routing change, not a port.
- **SD is FAT32 only.** FatFs in ESP-IDF 5.3 is built without exFAT.
- **`__assert_func` in a codec module returns instead of halting.** Reviewed,
  never exercised.
- **The LoRa radio, microphone and BHI260AP** are on the board and undriven.

## Recovery

The watch always takes a full image back, whatever is on it: hold **BOOT** while
resetting (or replug while holding it) and the ESP32-S3's mask-ROM downloader
answers, because that path is in ROM and no firmware can disable it.

Keep a full 16 MiB image. This port replaces the stock partition layout, so
recovering means writing the layout back too, not just the app:

```sh
./scripts/watch-backup.sh -o ~/twatch-backup/working.bin   # and keep the hash
```

**Write several regions in one esptool invocation.** The command resets the
board when it finishes, the app then takes the USB Serial/JTAG port for the
desktop, and the next esptool call has to wait for it to go quiet again — so the
app plus both audio blobs belong in a single `write_flash`.

`docs/watch-recovery.md` records how the watch was lost once and what it cost:
a kernel that had been saved because it booted was flashed back and **hung too**,
because the damage was in `/etc` rather than in the kernel. That document is
written for the Linux line, but its rule is the one that still applies — a full
image, not a saved kernel.

## Licence

The TinyDesk fork is **MIT**, matching upstream; its `LICENSE` is in the fork.
The vendored cores keep their own licences: nofrendo, gnuboy, micro-flac
(Apache-2.0), ST's ST25R3916 stack.

**This repository has no licence file.** The previous version of this README
claimed GPLv3 "matching upstream", which was true of the Linux distribution it
was built on, but no `LICENSE` was ever added here and the firmware now lives in
an MIT fork. Someone should decide what this repository is licensed under rather
than inherit a claim that has nothing behind it. Until then, treat the decision
log and reference as unlicensed rather than as GPLv3.

TinyDesk is not this project's work; see
[the upstream project](https://github.com/tinydesk-project/tinydesk).
