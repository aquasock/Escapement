# Changing the watch without losing it

Written after a session where it was lost, and what it cost.

## The rule

**Take a full flash backup before changing firmware or kernel.** It is 16 MiB
over the USB port, about three and a half minutes:

```sh
./scripts/watch-backup.sh                 # -> ~/twatch-backup/working-DATE.bin
```

Recovery then costs one write of a file this machine already holds:

```sh
PY=build/idf-tools/python_env/idf5.1_py3.14_env/bin/python
$PY -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    --after no_reset write_flash 0 ~/twatch-backup/working-DATE.bin
```

**Two details in that command are load-bearing, and both were learned by
getting them wrong on 2026-11-10.**

`write_flash` takes `<address> <filename>` pairs; there is no length argument.
The form previously documented here — `write_flash 0 0x1000000 <file>` — looks
like it means "write 16 MiB at offset 0", but esptool 4.13 reads `0x1000000` as
a second filename and dies with `[Errno 2] No such file or directory:
'0x1000000'` before touching the flash. The 16 MiB in the backup filename is the
length; the command does not take one.

**`--after no_reset` is not optional when you need more than one command in a
session.** By default esptool ends by resetting the board, which boots it into
the app. Linux then claims the USB-Serial-JTAG port (SOC-003) and esptool can no
longer drive it, so the next command sits at `Connecting...` forever. Every
successful esptool command therefore locks itself out unless it ends with
`--after no_reset`. Holding the board in boot mode does not help by itself —
the reset at the end of the *previous* command is what does the damage.

Confirm the write before you trust it. esptool prints `Hash of data verified`
when the data it read back matches what it wrote, which is the only proof that
the 16 MiB landed; a write whose output was not captured is not a verified
write. After that, `--after hard_reset chip_id` (or a power cycle) starts the
board, and a buildroot login prompt on the console is the real end of the
recovery.

## Why a kernel backup is not enough

The session that taught this lesson started with a kernel change (`fb_write` on
the CO5300 driver, so userspace could draw on the panel). The rebuilt kernel
hung after `Running sysctl: OK`, so the previous kernel — which had been saved
with `cp` and was known to boot — was flashed back.

**It hung too.** That is the important part: the second failure was not the
kernel. Something else had been damaged, and the only other thing that had been
touched was `/etc`, on jffs2, where a new init script had been added while
`S03keepconfig` was managing that partition's generation.

The only remaining route was `images/linux-esp32s3-native-full.bin`, the
release image. It booted — and cost:

- **the display**, because the release image's kernel predates the CO5300
  driver entirely (zero `co5300` strings in its kernel)
- **the Bluetooth keyboard**, because `/home` was rewritten
- **the AMOLED login**, for the same reason
- **WiFi**, because `/etc/wpa_supplicant.conf` was rewritten
- **the root password**, which reset to the buildroot default

Every one of those was recoverable, and none of it needed to happen. A single
`read_flash 0 0x1000000` beforehand would have made it a 16 MiB restore.

## Userspace drawing on the panel

`fb_write` is implemented in the CO5300 driver (`co5300_write`), so userspace can
put pixels on the glass. Without it there is no userspace path at all: the driver
provides only `fb_setcolreg`, `fb_fillrect`, `fb_copyarea` and `fb_imageblit`,
which fbcon uses, so text works while `mmap()` returns ENODEV and `write()` hits
`fb_WARN_ON_ONCE(!info->fbops->fb_write)`.

`fb_mmap` alone would not be enough either — the panel has no memory of ours and
is fed over SPI, so fb writing into the framebuffer only puts bytes in RAM.
`co5300_write` copies and then blits the rows it touched.

Two things to know when driving it from a program:

- **Write in bands.** A 320x400 RGB565 frame is 256 KB, and on NOMMU that must
  be one *contiguous* allocation; after a few minutes of uptime the largest free
  block is smaller than that. Bands of ~40 rows (25 KB) always fit, and each
  band is a complete `fb_write` call.
- **Do not put the payload in `.bss`.** A large static array is part of the ELF
  load segment, so the program fails to *exec* when memory is fragmented —
  `nommu: Allocation of length 569344 ... failed` is the load segment, not a
  runtime allocation. The 512 KB Tamagotchi ROM hits this; it has to be read
  from a file into a runtime allocation.

Each `pwrite` is a synchronous SPI blit, so frame rate is bounded by the display
path, not the CPU: 4 fps leaves the emulator at ~1.1x realtime, 30 fps drops it
to 0.18x because the writes dominate.

## What the release image is, and is not

`images/linux-esp32s3-native-full.bin` is the upstream release. It is **not**
this project's state:

- its kernel has **no CO5300 display driver** — the panel stays dark
- its firmware requires **octal PSRAM**, so on this quad-PSRAM watch it
  panics before Linux starts (`octal_psram: PSRAM ID read error`)

Flashing it therefore needs the firmware written immediately afterwards:

```sh
./scripts/flash.sh --image     # release image, 16 MiB
./scripts/flash.sh --app       # quad-PSRAM firmware, 0x10000
```

and even then the display stays dark until a kernel with the driver is
written to the `linux` partition at `0x140000`.

## The kernel partition is separate

```
linux   0x140000  4 MiB
```

`xipImage` is stored **raw** there — verified byte-for-byte against flash — so a
kernel change is one write, not a full reflash:

```sh
esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x140000 build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage
```

**Check `space_for_vectors` first.** The kernel is linked against an address in
the firmware's RAM; if the firmware changed, the address moved and a mismatched
kernel corrupts memory instead of booting (`scripts/build.sh` verifies this).

## Getting back in when the network is gone

The release image has no WiFi credentials. Two routes:

- **Serial console**, 115200 on `/dev/ttyACM0`. Login `root`; the release image
  uses the buildroot default and then forces a change.
- **The board's own tool**, once at a shell: `wifi connect "SSID" "passphrase"`.
  It writes `/etc/wpa_supplicant.conf` and brings the link up — do not hand-write
  `wpa_passphrase` output, which is what went wrong when this was tried.

Telnet is on by default after the first-boot dialog, which is how the
deployment gets restored without fighting the serial console:

```sh
./scripts/watch-telnet.py 'uname -a'
```

## Restoring the userspace deployment

`/home` and `/etc` are rewritten by a full image flash. Everything needed to put
them back is in this repo:

| Watch path | From |
|---|---|
| `/home/blekbd` | `userspace/blekbd` (built by `scripts/build-userspace.sh`) |
| `/home/tty1-getty.sh` | `userspace/tty1-getty.sh` |
| `/etc/init.d/S48blekbd` | `userspace/S48blekbd` |
| `/etc/init.d/S49tty1shell` | `userspace/S49tty1shell` |

and `/etc/fstab` needs `compr=none` on both jffs2 mounts, so a bond file the
firmware reads back is never compressed.
