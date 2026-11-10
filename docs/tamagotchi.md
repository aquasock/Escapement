# The Tamagotchi on the watch

Status: **works, verified on hardware 2026-11-09.** The V3 egg runs on the
AMOLED, wiggles, and survives a fragmented heap.

The emulator itself is not this project's work. It lives in
`/run/media/vash/GIT/TamaHarness/` — a bit-exact, fuzz-clean SPLB32 core
executing the real 512 KB V3 ROM, with its correctness established by its
authors. This document records what it takes to run it *here*.

## The shape

```
ROM            512 KB, raw in flash at 0x480000
               read from userspace at 0x42480000, costing zero RAM
tamafb         cross-compiled, ~220 KB static, bss ~35 KB
display        320x400 RGB565 via banded pwrite() to /dev/fb0
rate           4 fps gives ~1.0x realtime; the panel is the limit
```

## Why the ROM lives in flash

The obvious approach — `malloc` 512 KB — works on a fresh boot and then stops
working. This is KRN-001: NOMMU has no overcommit, so the ROM must be one
contiguous run of free memory, and after a few minutes of uptime no 512 KB run
exists. The failure is an exec-time allocator error, not something the program
can report:

```
cannot allocate 524288 bytes for the ROM -- the largest free
contiguous block is too small; reboot and try again
```

Measured on the watch: with the largest free block down to **128 KB**, the
malloc build cannot start; the flash build runs at **1.00x realtime**.

The alternative the kernel already provides is the linear flash window.
`init_linux_mmap()` extends the MMU table so the whole 16 MiB is readable at
`0x42000000` before jumping to Linux (KRN-002), and that window is still valid
from userspace afterwards — verified by reading the kernel partition and
checksumming a 64 KB range through it.

Two things that were tried and are **not** the answer:

- **File mmap** (`MAP_PRIVATE` on `/home/rom.bin`) works but costs **1020 KB**,
  twice the file, because NOMMU copies the mapping rather than sharing pages.
  `MAP_SHARED` fails with `ENODEV`.
- **`MAP_POPULATE` / demand paging** does not help: the copy happens either way.

So the ROM is written to flash once and read through the window.

## The slot, and the guard that keeps it safe

```
linux partition   0x140000 .. 0x540000    4096 KB
kernel            0x140000 .. 0x3F3A88    2833032 bytes
ROM slot          0x480000 .. 0x500000    512 KB, erased before use
spare after ROM   0x500000 .. 0x540000    256 KB
```

561 KB of headroom above the current kernel. That is enough to be comfortable
and close enough to be dangerous, so `scripts/build-kernel.sh` **refuses to
build an `xipImage` that would reach 0x480000**:

```
xipImage would reach the ROM slot at 0x480000 (limit 3407872 bytes)
```

Without that guard, a kernel that grew past the slot would be silently
overwritten by the next ROM write, and the pet would be subtly wrong rather
than obviously broken.

Writing the ROM:

```sh
esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x480000 rom.bin
```

Verified by reading back through the window and comparing CRCs against the
file, not by trusting esptool's hash alone:

```
flash  0x42480000   A5 5F 10 49   crc32 301F0E87
file   rom.bin                     crc32 301F0E87
```

## Building it

The front end comes from TamaHarness and needs three changes for this board,
all of which are now in `portable/tamafb.c`:

```sh
CC=build/toolchain/crosstool-NG/builds/xtensa-esp32s3-linux-uclibcfdpic/bin/\
xtensa-esp32s3-linux-uclibcfdpic-gcc
export XTENSA_GNU_CONFIG=$PWD/build/toolchain/xtensa-dynconfig/esp32s3.so

$CC -O2 -Wall -Wextra -static \
    -DFB_W=320 -DFB_H=400 \
    -DROM_FLASH_ADDR=0x42480000 \
    -o tamafb splb32.c ht24lc08.c con_v3_ir.c splb32_sound.c \
             tamafb.c segmap.c
```

- **`ROM_FLASH_ADDR`** selects the flash-window ROM and skips the allocation
  entirely. Without it the build falls back to `rom.bin` in the cwd.
- **`FB_W` / `FB_H`** must be 320x400. The defaults are `SEG_W+32` by
  `SEG_H+48` = 324x348, which is larger than fb0 and the program refuses to
  draw. The 292x300 segment area still centres inside 320x400.
- **`-static`** and no `-DEMBED_ROM`. Embedding the ROM compiles it into `.bss`
  and reintroduces exactly the contiguity problem this is avoiding: `bss` goes
  from 35 KB to 547 KB.

## Two display facts that bite

**Do not `mmap` `/dev/fb0`.** The CO5300 driver implements no mmap operation,
so `MAP_SHARED` returns `ENODEV` (LCD-001). Use `pwrite()` per band — that is
`fb_write`, which the driver does implement, and one call per band matches the
driver's one-window-per-transfer shape.

**The console fights the game, twice over.** Every `printk` mirrors to all consoles and
fbcon is one of them, so a kernel message repaints console text over the frame
and the next frame repaints back — a visible flicker. On this board the trigger
was constant: the touch daemon polls the CST9217, it NAKs, `i2c-gpio` prints a
line per NAK. `/etc/sysctl.conf` now sets `kernel.printk = 1 4 1 7` so routine
messages do not reach the panel. Raise it for a boot with
`echo 7 4 1 7 > /proc/sys/kernel/printk` when debugging.

Loglevel does **not** cover the second source: fbcon blinks the console cursor on
a timer and repaints that cell each time, which shows on a fullscreen app as a
small blinking hole. `tamafb` writes 0 to
`/sys/class/graphics/fbcon/cursor_blink` at start and restores the previous value
on exit — restoring rather than leaving it off, because a blinking cursor is
correct at a shell prompt.

## Measured

On the flash build, 320x400, 4 fps:

```
frames drawn      16       (4 s run)
realtime factor   1.00x    (target 4,000,000 cycles/s)
```

The core alone reaches 4.05x headless, so the panel bounds it — consistent with
LCD-005 and LCD-010. At 4 fps the egg hatches in roughly real time.

## Still to do

- **Buttons are not wired.** The three touch bars emit `BTN_0/1/2` and nothing
  consumes them; `splb32_pin(cpu, 0, mask, level)` with PA bits `0x01/0x02/0x04`
  is the hook, and `tamagui.c` already does exactly this from a keyboard.
- **Not a service.** It is launched by hand. The flash ROM removed the 512 KB
  allocation, but a `S51tamafb` needs care for a second reason: the binary is
  222 KB static and *exec* is subject to the same contiguity rule (KRN-001).
  Measured at a fragmented moment with the largest free block at 256 KB, the
  same binary started in the foreground every time and died with SIGSEGV about
  one launch in three when backgrounded with `&`. Starting it early, from rcS,
  avoids this; starting it late does not.
- **The `pwrite` cost is unmeasured separately from rendering.** LCD-005's
  ~190 ms per frame conflicts with LCD-010's 45.7 ms projection by 4.2x, and
  instrumenting `render_band` against the `pwrite` would settle which dominates.
