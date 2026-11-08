# Linux on the T-Watch Ultra — findings, 2026-11-08

Result: **Linux boots and runs to userspace on this watch.** It is headless and
has no display. Getting it there needed one firmware change (quad PSRAM) and no
kernel change. Verified over JTAG, not by console — see "How this was verified".

## What was done

Stock release image from
[paulneja/Linux-on-esp32-S3](https://github.com/paulneja/Linux-on-esp32-S3)
**0.9.1** (`linux-esp32s3-native-full.bin`, sha256
`120d4cdd50f2bf300302407355eca9234f0d750bd4241d41a51c58e5e77a72b7`, matches the
published `SHA256SUMS`) was flashed whole at `0x0`.

### It fails immediately, in the app, not the bootloader

```
ESP-ROM:esp32s3-20210327
E (173) octal_psram: PSRAM ID read error: 0x00000000, PSRAM chip not found
E (173) cpu_start: Failed to init external RAM!
ESP-ROM:esp32s3-20210327      <- reboot loop
```

Before this project existed, the exact same signature was already recorded in
TinyPetz's `NOTES.md` -- where it had been read as 'PSRAM is unusable on this
board' and left there for two sessions.
The cause is the project's requirement of **octal** PSRAM (N16R8); this board is
**quad** (LilyGo document 8 MB QSPI PSRAM, and the display firmware in TinyPetz
runs with `CONFIG_SPIRAM_MODE_QUAD=y`).

**The bootloader is not the problem.** Our rebuild produced a `bootloader.bin`
byte-identical to the release's (`56eaabc9…`), and the partition table is
byte-identical too (`179d3538…`). `cpu_start: Failed to init external RAM` is an
app-stage tag. So the fix belongs in the app and only the app needs reflashing.

### The fix: build core 0 with quad PSRAM

Two lines in `new-files/esp-hosted/network_adapter/sdkconfig.defaults.esp32s3.16m8r`:

```diff
-# CONFIG_SPIRAM_MODE_QUAD is not set
-CONFIG_SPIRAM_MODE_OCT=y
+# CONFIG_SPIRAM_MODE_OCT is not set
+CONFIG_SPIRAM_MODE_QUAD=y
```

Three things that will bite anyone repeating this:

1. **`esp_driver/CMakeLists.txt` runs `git reset --hard` on every cmake call.**
   Commit local changes or they vanish. This is documented in the project and it
   is not theoretical.
2. **The patched wifi libs must be installed by hand.** `esp_driver/lib/esp32s3/*.a`
   carry jcmvbkbc's additions (`esp_wifi_connect_internal`,
   `esp_wifi_issue_auth_internal`, `esp_wifi_issue_assoc_internal`). Skipping the
   `file(COPY …)` step in that CMakeLists gives a link error, not a wifi error.
   Confirmed with `nm`: stock `libnet80211.a` has 0 hits, patched has 1.
3. **The vectors symbol moves, and the kernel has its address compiled in.**
   `space_for_vectors` is a 4 KiB IRAM buffer declared 4 KiB-aligned. Quad mode
   grew IRAM by <4 KiB, so it rounded *down* a page to `0x4037f000` while the
   kernel expects `CONFIG_VECTORS_ADDR=0x40380000`. Declaring it
   `aligned(8192)` rounds it *up* to the right page, which avoids rebuilding the
   kernel. Verified with `nm` on both variants.

Build tree: `build/`, reproduced by `scripts/build.sh` -- project at `0.9.1`,
`esp-hosted` at `e2f19ed`, IDF fork at `24394052` (v5.1.1) with its own
toolchain in `build/idf-tools/`, so no other ESP-IDF install on the machine is
touched.

Flashed `0x10000` only:

```sh
esptool.py --chip esp32s3 --port /dev/ttyACM1 --baud 921600 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB 0x10000 build/network_adapter.bin
```

### It boots

```
pp rom version: e7ae62f
net80211 rom version: e7ae62f
MMU: first_free_mmu=10 (firmware uses entries 0..9)
MMU: kernel e0x14=0x0014 rootfs e0x54=0x0054
linux ptr = 0x42140000
vectors ptr = 0x40380000
```

`0x42140000` is the kernel at flash offset `0x140000`, and `0x40380000` is what
the kernel was built to expect. Single boot, no resets over 75 s.

## The RTC: what works, and the two BusyBox traps

The PCF85063A at I2C 0x51 is a real clock on this board — separate from the
AXP2101, backed by VBACKUP, and counting while the main battery is flat.

Verified end to end:

```
rate        02:52:05 -> 02:53:05 over 60 s wall time, no drift
boot        rtc-pcf85063 0-0051: setting system clock to ... UTC   (hctosys)
write       hwclock -w -u
shutdown    /etc/init.d/S01hwclock, called by rcK
```

The write-back hook exists because `hctosys` only goes one way. Without it, an
NTP correction during a session is lost and the old value returns at the next
boot. `rcK` runs `S??*` in **reverse** order, so `S01hwclock` runs last —
after anything that might touch the clock, before umount.

Verified by deliberately breaking it: chip set to 20:30 while true UTC was
19:00, system clock corrected, reboot with no manual `hwclock -w`, and the
next boot reported `setting system clock to 2026-11-08T19:01:00 UTC`.

### Trap 1: this is BusyBox hwclock, not util-linux

```
$ hwclock --set --date "..." --utc
hwclock: unrecognized option `--set'
```

There is no way to write an arbitrary time to the chip. BusyBox offers only:

```
-s        RTC -> system
-w        system -> RTC
--systz   set the in-kernel timezone
-u / -l   the chip holds UTC / local time
```

So the sequence is always **set the system clock, then push it down**:

```sh
date -u -s "2026-11-08 18:56:00"
hwclock -w -u
```

### Trap 2: `-u` is not optional

With neither `-u` nor `-l`, BusyBox `hwclock` reads
`/var/lib/hwclock/adjtime` to decide what the chip means. That file does not
exist here, so it is guessing. Always pass `-u`: the chip holds UTC, and
timezone is userspace's business.

Corollary: `date -s` takes a literal `YYYY-MM-DD hh:mm:ss`. There is no
`@epoch` form and no `+format`, so `date -s "@$saved"` — which the shipped
`S01clock` uses — does not work on this shell.

### The factory set it to China time

The chip arrived holding UTC+8. LilyGo are in Shenzhen; the RTC is battery
backed and was set to local time at the factory with no timezone attached.
`hctosys` then copied that number in as if it were UTC, so the kernel clock was
also 8 hours fast. One correction fixes it, and the hook keeps it fixed.

### Editing a script over the console: never send tabs

Writing the init script through an interactive bash mangled it — a literal TAB
in the heredoc body triggered readline filename completion, which inserted
`README.txt` into the file. Indent such scripts with spaces, or the shell
edits them for you.

## Building the kernel: three patch stacks, not one

The release kernel is Linux 7.2.4 plus **three separate stacks**, and the third
is easy to miss because the first two produce a kernel that builds and boots:

1. the port's 62 board patches (`new-files/board/.../patches/linux`)
2. Escapement's own (`patches/kernel`)
3. the **fork backend** (`experiments/mmu-poc/fork`, nine patches)

Missing stack 3 is not a compile error. The kernel builds, boots, brings up the
RTC, and reaches a login prompt — and then falls over the moment an interactive
shell starts:

```
Caught unhandled exception in 'bash' (pid = 194, pc = 0x3df30388)
EXCCAUSE is 2
bash could not fork with 4432 KiB free; switching this session to dash.
```

That was the cost of building with only stacks 1 and 2. NOMMU has no MMU to give
a child its own address space, so the memory-banking patches **are** the fork
implementation; without them `fork()` cannot work at all. Busybox `sh` hides it
because it uses `vfork`; bash does not, so the failure appears exactly at login.

The tell is the version string:

```
7.2.4-forkbank          <- correct
7.2.4-g<hash>-dirty     <- fork backend missing
```

`build-kernel.sh` sets `CONFIG_XTENSA_NOMMU_FORK=y` and
`CONFIG_LOCALVERSION="-forkbank"` and refuses to finish unless the built image
reports the `-forkbank` string.

### One config deviation worth knowing

`CONFIG_RTC_CLASS=y` pulls in `CONFIG_RTC_NVMEM` (it defaults to `RTC_CLASS`),
which selects the whole NVMEM framework. The board seed ships `CONFIG_NVMEM`
off and the PCF85063A uses none of it, so both are pinned off. The project's
own `check-kernel-config.py` catches this class of drift and is run as a build
gate.

## The console: two fixes that look like something else

There IS a console — `ttyGS3`, the S3's USB-Serial-JTAG — and the port you
flash is the port that prints. Two faults made it look like there wasn't one.

**`quiet`.** The dts bootargs carry it, dropping the log level to 4 so almost
nothing prints. A kernel with every message suppressed is indistinguishable
from a kernel that never started. This cost two sessions of JTAG program-counter
sampling to answer a question the kernel would have answered for free.

**Console order.** `console=ttyGS3 console=ttyS0,115200n8` — and `/dev/console`
binds to the *last* console=. So `/dev/console` was `ttyS0`, the UART this board
does not have. printk mirrors to every registered console, which is why the log
still appeared on USB, but init's stdin/stdout went to the dead UART. A root
shell with `init=/bin/sh` was running and reading from hardware that was not
connected: output appeared, keystrokes went nowhere, and host writes blocked
with `SerialTimeoutException` because the OUT endpoint had no consumer.

The diagnostic that settled it was process of elimination backed by a fact:
flashing works over the same USB interface, so host-to-device data is accepted
by the hardware. The failure had to be above it. Reordering to
`console=ttyS0,115200n8 console=ttyGS3` produced a shell immediately.

That ordering is also what makes the shipped `/etc/inittab` work untouched — it
already contains `console::respawn:/sbin/getty -L console 0 vt100`, and with
ttyGS3 last, `/dev/console` *is* the USB port.

### What this replaced

Upstream documents `usb-console on` to enable a `ttyGS3` getty, needed on their
devkit because there `ttyS0` is last and `/dev/console` is the UART. It is at
`/usr/sbin/usb-console` here and works, but it is unnecessary once the console
order is right, and enabling it would put two gettys on the same device.

An earlier version of this note claimed a repeated PC across samples was a
wedge. That was wrong: openocd's `init` resets both cores, so a lone repeated
PC proves nothing. The variation test is the one that means something, and it
is what `scripts/probe.sh` reports.

## What is NOT possible

**No display, and no path to one without real work.**

- No driver for the CO5300. Searching the project for `co5300`, `framebuffer`,
  `ili9341`/`st7789`/`gc9a01` returns nothing.
- The kernel config has no `CONFIG_FB`, no DRM, and `# CONFIG_VT is not set`, so
  there is not even a text console concept.
- `# CONFIG_INPUT is not set`, so no touch and no buttons.
- Core 0's firmware has no AXP2101 or XL9555 bring-up, so the AMOLED is not even
  powered. That init is in `firmware/bringup/main/board.c` and is the piece that
  does not exist on their side.

PSRAM cannot be read over this JTAG path, so the printk ring buffer was not
recoverable either way.

## If a console login is wanted later

Already satisfied: the stock `inittab` getty serves `/dev/console`, which is
`ttyGS3` once the console order is right. No partition editing, no mtd-utils.

`/usr/sbin/usb-console on` exists and would add a dedicated `ttyGS3` getty, but
with the reordering it is redundant and would put two gettys on one device.

## Recovery

Unchanged and re-verified this session:

```
factory image  16 MiB, read off the watch before any of this was flashed
sha256 471c9a21a6d53e5cdf6dfb01949fcc2dd4f420fe1a3738631f73b600419ac2b0
```

`scripts/flash.sh --factory` writes it back. That hash is this watch's; a
different unit needs its own dump.

BOOT + replug always reaches mask-ROM download mode. The bootloader and
partition table we built match the release byte-for-byte, so flashing the
TinyPetz app back at `0x10000` alone is also not sufficient: the Linux image
replaced the partition layout's contents from `0xd0000` onward. Use
`scripts/flash.sh --factory`.
