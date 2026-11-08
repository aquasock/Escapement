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

## How this was verified (no console exists)

The kernel's bootargs are `console=ttyGS3 console=ttyS0,115200n8`. This watch has
no UART bridge, and `ttyGS3` is the S3's USB-Serial-JTAG peripheral — the same
one the ROM is already using. After handoff the host never re-enumerates, and
`/dev/ttyACM1` delivers 0 bytes. Enabling the firmware's own USJ console
(`CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y`) made it worse by holding the
peripheral; it was reverted.

So the state was read over **JTAG** (openocd, `board/esp32s3-builtin.cfg`),
sampling core 1's PC after boot:

```
11  PC = 0x422f89aa     -> flash offset 0x1b89aa: `waiti 0` / `retw.n` = the idle loop
 1  PC = 0x42646edd     -> flash offset 0x646edd, inside rootfs.cramfs
```

Two conclusions, and the reasoning matters:

- The PC **varies**, so the core is not wedged; it is idle and taking
  interrupts for most of its time, which is what a booted system looks like.
- The odd sample lands **inside rootfs.cramfs**, which this port executes in
  place. Executing from the rootfs means **userspace was running.**

Caveat: an earlier draft of this note claimed the identical PC across samples
was a wedge. That was wrong — openocd's `init` resets both cores, so a repeated
PC can just be reset state. The variation test above is the one that means
something.

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

## If a console is wanted later

The reliable route is to enable the `ttyGS3` getty by editing `/etc/inittab` on
the 448 KiB jffs2 `etc` partition (offset `0xd0000`) — `usb-console on` does it
at runtime, but needs a shell, which is the thing we lack.

That needs `mkfs.jffs2`/`jffs2dump` from mtd-utils, which is not installed and
cannot be installed because `sudo` is blocked by the no-new-privs flag. It has
to be built from source into a prefix. Several hours for no new information
about whether Linux runs — only for a login.

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
