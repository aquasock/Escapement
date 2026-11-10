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

## The core is fixed-point, and that doubled its speed

The reference implementation keeps its cycle and timer accumulators in
`double`. On this watch that is expensive: measured on hardware, a double add
costs **711 ns** against **121 ns** for the integer equivalent, because the
Xtensa has no hardware double and every operation is a soft-float library call.
`timers_clock()` runs once per emulated instruction and touches three such
counters, so the interpreter paid that for every instruction the guest ran.

The conversion is exact rather than approximate, which is why it is safe. The
sleep tick is `CLOCK_HZ / SUB_CLOCK = 4000000 / 32768 = 122.0703125`, which is
precisely `15625/128`. At `CYC_SHIFT = 7` fractional bits that value is the
integer 15625 and nothing is truncated, so the drift the original comments warn
about cannot occur.

Measured on the watch, per frame at 10 fps:

```
before (double)   emu 50.8 ms
after  (fixed)    emu 26.4 ms
```

Equivalence was proven, not assumed. Against the original core, over 200M
cycles (50 emulated seconds):

```
instrs      1790467 -> 1790467
pc             8239 -> 8239
ram_fnv    41DA6499 -> 41DA6499
dpram_fnv  EEE59CD3 -> EEE59CD3
```

and a button-press run matched to the tenth of a millisecond, including the
580.1 ms gap the ROM leaves before it redraws.

One bug was caught this way. `TM0L/TM0H/TM1L/TM1H` are read by the firmware as
raw 16-bit cycle counters, so they need the fraction shifted off; leaving them
in fixed-point units made the guest read a value 128x too large. The trace
agreed for the first 400k instructions and diverged at the first timer poll.

Two things that do **not** work and should not be retried:

- **`double` is soft-float.** 711 ns per operation.
- **`float` is worse than slow -- it faults.** The ESP32-S3 has a
  single-precision FPU and the compiler emits `add.s`/`sub.s` for it, but this
  kernel does not enable the coprocessor for user tasks, so executing those
  instructions raises a coprocessor-disabled fault and the process dies with
  SIGSEGV. Making float usable means coprocessor context switching on a NOMMU
  port with a patched `fork()`, which is a much larger risk than using integers.

## Why the emulator crawls when the ROM computes

Measured on the watch: **a dependent load from PSRAM costs 178.83 ns**, about
43 CPU cycles at 240 MHz. The emulator's whole machine state is 2968 bytes and
it lives in PSRAM (heap at `0x3ddb0008`, `.bss` at `0x3dc708f0`), so the
interpreter makes roughly a dozen PSRAM round trips per emulated instruction.

That produces 387k steps/s and ~620 CPU cycles per emulated instruction, and it
explains every symptom:

```
idle pet        speed 1.00x   emu 245-290 ms/s   panel 50-110 ms/s
menu / text     speed 0.38x   emu 900-1140 ms/s  panel  18-28 ms/s
```

When the guest sleeps, cycles are nearly free, so the emulator keeps real time.
When the ROM actually computes -- opening a menu, drawing a text screen -- the
sleep disappears and throughput collapses. The panel figure *falls* during the
slow stretch, which is what rules the display out as the cause.

This is also why the fixed-point conversion helped: it removed dependent loads,
not just soft-float calls.

Everything already excluded stays excluded -- SPI clock bought 13 percent, DMA
targets a cost that is not on this critical path, and frame rate does not touch
instruction throughput. The only lever with the right magnitude is getting the
hot state into internal SRAM: under 3 KB, on-die, a few cycles per access
instead of 43. That is a memory-placement question for this port, the same class
of constraint as the ROM's 512 KB contiguity limit.

## Parked: the ~1.5 s wait entering the time screen

Still open, by decision, because it runs fine otherwise.

What is established:

- The ROM itself goes **silent for 580.1 ms** after a press while it rebuilds
  the clock screen. Measured on the host in emulated cycles, so it is a
  property of the ROM and not of this port.
- The host runs that window at 211x realtime, so the ROM's own cost is not the
  problem there.
- On the watch the same windows show single frames of `emu 123-240 ms` and
  `panel 43-135 ms` against a 100 ms frame budget, so the loop falls behind and
  emulated time stretches. 580 ms of ROM at the observed slowdown plus paint is
  the right order for 1.5 s.
- The panel figure is 10x its own benchmark (9.5 ms). **Unexplained.**

Ruled out by measurement, so nobody re-treads it:

- Touch polling. A 15-byte read is 3957 us and the DRV2605 click 1796 us;
  polling is now 15 ms and costs 8% of the emulator's throughput.
- The paint path. Band masking already skips 62% of transfers; mid-frame
  painting was tried and reverted because it tore the picture.
- SPI clock. 40 MHz gives 13%, not 2x -- the transfer is not clock bound.

Next thing to measure, if it is picked up again: what the guest is actually
doing in those 580 ms. The stall log splits the time but has not yet been read
against a press on the fixed-point build.

## Still to do

- **Not a service.** It is launched by hand. The flash ROM removed the 512 KB
  allocation, but a `S51tamafb` needs care for a second reason: the binary is
  249 KB static and *exec* is subject to the same contiguity rule (KRN-001).
  Measured at a fragmented moment, the same binary started in the foreground
  every time and died with SIGSEGV about one launch in three when backgrounded
  with `&`. Starting it early, from rcS, avoids this; starting it late does not.
  `/home/run-tama.sh` retries up to 8 times, which works but is luck.
