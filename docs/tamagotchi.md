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

**Read this together with the next section:** the flash window is the reliable
choice, not the fast one. It costs 33.8 percent against a PSRAM-resident ROM,
measured 2026-11-10. The history below explains why it was chosen and is still
accurate about the mechanism; the recommendation has since changed.

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

## The ROM's location is worth 33.8 percent, and it is in flash

**Measured 2026-11-10, and it changes the recommendation.** Reading the ROM
through the flash window works and costs no RAM, but it is not fast:

```
ROM in flash window   204033 instr/s   4.49x   4901 ns per emulated instruction
ROM in PSRAM          273098 instr/s   6.01x   3662 ns per emulated instruction
                                  +33.8%,  1239 ns saved per instruction
```

That is nine runs of the flash build across three boots (203008 to 205065, no
overlap with the PSRAM figure) against two PSRAM runs on two separate boots
(273115 and 273080). Both arms retired exactly 454305 instructions at 88.05
emulated cycles each, so the same work was done both times and only the memory
differs.

The mechanism is confirmed rather than merely observed. The ROM is 72.6 percent
of the emulator's 2.408 bus accesses per instruction — 1.748 of them — and a
dependent read from the flash window costs 738 ns, so the prediction was
1.748 × 738 ≈ **1290 ns**. The measured saving is **1239 ns**, within 4 percent.
The interpreter really was stalling close to full latency on most of its ROM
reads.

### So why is the ROM in flash at all

Because putting it in PSRAM is a one-shot bet, and that has not changed. The
512 KB contiguous run exists at boot and is gone quickly: 1 MB at 0 minutes,
256 KB by 9 minutes, 128 KB by 32 minutes, and a single such allocation consumes
it for the rest of the session (KRN-004). A malloc build succeeds exactly once
per boot, and `free` reports plenty of memory the whole time, which is what makes
the failure so misleading.

So the honest position is a trade, not a bug:

- **In flash** — always runs, any uptime, no contiguity requirement. Costs 33.8
  percent, every time the emulator runs.
- **In PSRAM** — 6.01x instead of 4.49x, but only if the allocation happens
  before the heap fragments, which means starting the emulator early from rcS
  rather than on demand.

The second is the better emulator and the more fragile service. Since a
`S51tamafb` was already going to have to start early for its own reasons
(KRN-001), the fragility it adds is not new — and 33.8 percent is most of what
the block-register rewrite was expected to buy, for a fraction of the work.

Two approaches this section used to call settled and that remain excluded:
**file mmap** (`MAP_PRIVATE` on `/home/rom.bin`) costs 1020 KB, twice the file,
because NOMMU copies the mapping rather than sharing pages, and `MAP_SHARED`
fails with `ENODEV`; and **`MAP_POPULATE` / demand paging** does not help because
the copy happens either way.

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
instead of 43. Both obvious escape routes are closed on this port. The core configuration the
kernel is built against declares `XCHAL_DCACHE_SIZE=0` and `XCHAL_ICACHE_SIZE=0`
-- there is no cache, so nothing to enable and `CONFIG_MEMMAP_CACHEATTR` is
irrelevant. And the device tree declares exactly one memory region, `memory@0`
at `0x3d800000` for 8 MB of PSRAM, so there is no internal SRAM to move the hot
state into either.

Confirmed from the other direction with a three-working-set pointer chase:

```
  4 KB (fits any cache)   70.30 ns
256 KB (exceeds cache)    68.72 ns
second 256 KB buffer      78.93 ns
```

No locality effect at all, which is impossible if a cache were operating.

So PSRAM latency is a hard floor. The lever that remains is reducing the
*number* of dependent loads rather than their cost: ~37 per emulated step
against the dozen an interpreter needs with decode state held in locals across a
block. That is 2-3x, in userspace, with no coherency risk.

## The architectural answer: the other core has the cache

Both ESP32-S3 cores are identical Xtensa LX7 at 240 MHz, but they are not
equivalent for compute. Cache is enabled and owned by the firmware side, which
is how ESP-IDF runs from flash-mapped memory at usable speed; the core
configuration the Linux kernel is built against declares `XCHAL_DCACHE_SIZE=0`
(EMU-003). Linux runs on core 1 alone -- `CONFIG_HAVE_SMP` is not set and the
LX7 config declares `XCHAL_HAVE_EXCLUSIVE=0`, so SMP is not supportable.

So the emulator is running on the one core of two that has no cache. Core 0,
running nothing but WiFi and the esp-hosted link, is idle and cached.

The design that follows is to emulate on core 0 and draw on core 1: core 0
writes its 292x300 display into a shared buffer, and a thin Linux program blits
that to fb0. Compute lands on the cached core; only drawing lands on the core
that owns SPI.

**Not started, deliberately.** It needs firmware work in the tree that must
never be pushed (its origin is upstream), core 0 owns the WiFi link this
project's console depends on, and starving core 0 trips the FreeRTOS watchdog
(`BLE-004`, `rst:0x7 TG0WDT_SYS_RST`). Recorded as EMU-004.

## The smaller lever, and the one to try first

The interpreter spends the order of 150 native instructions per emulated
instruction (measured 2026-11-10, see below), against the 30-80 a tuned 6502 core
needs and 97 percent of it in its own instruction stream rather than in memory
access. Keeping guest registers, flags and PC in host variables across a decoded
block -- instead of reloading each field per access, in handlers spread across a
6796-instruction switch -- is therefore the right lever, in userspace, with no
firmware risk. Recorded as EMU-005 and EMU-006.

## Measuring the interpreter: corebench, measured

`corebench` runs the SPLB32 core with nothing behind it -- no panel, no
framebuffer, no touch polling -- so it prices the interpreter and nothing else.
It is built by script, because hand-built deploys kept failing for want of
memory:

```sh
./scripts/build-corebench.sh              # three variants
./scripts/build-corebench.sh --selftest   # and run it on the host first
```

There are three now, and which one runs is not a preference:

- `corebench-flash` (77 KB) reads the ROM from the memory-mapped flash window at
  `0x42480000` and allocates nothing. **This is the one that works.** It is what
  produced every number below.
- `corebench` (601 KB) compiles the ROM in. It does not exec on this board: the
  kernel refuses `nommu: Allocation of length 565248 from process 207
  (corebench) failed`, the largest free block being 512 KB. This corrects entry
  18's estimate of ~575 KB, and shows the requirement is on a loaded segment
  because the ROM lands in `.rodata`, not `.bss`.
- `corebench-rom` (78 KB) takes the ROM as a path and `malloc()`s it. Same
  512 KB problem at malloc time; on a board that has been up a while it reports
  `cannot allocate 524288 bytes for the ROM`. Needs a fresh boot.

### What an emulated instruction costs

Measured on hardware 2026-11-10, 40,000,011 emulated cycles, three consecutive
runs of the plain build:

```
instructions      454305      (identical every run -- deterministic)
avg cycles/instr  88.05
instructions/s    204045 / 203945 / 203488
realtime factor   4.48-4.49x  (target 4,000,000 cycles/s)
```

and from the counter build, whose counts are exact:

```
steps             736660      sleep steps 282355   irqs 731
steps/instr       1.622       <- 736660 - 282355 = 454305, to the digit
reads             1032471     writes 61692
accesses/instr    2.408
rom 72.6%   ram 21.4%   dpram 0.0%   sfr 0.3%
```

So one emulated instruction is **exactly one interpreter step plus the sleep
ticks around it**, 2.4 guest bus accesses, and 4922 ns of wall clock -- about
1181 native cycles at 240 MHz.

### What that means for the rewrite

Memory is not the cost. 2.408 accesses at the measured 53 ns is about 128 ns,
**2.6 percent** of the 4922 ns. The other 97 percent is the interpreter's own
instruction stream, which is precisely what keeping guest registers, flags and PC
in host locals across a decoded block removes. Dividing the residual by the
measured cost of a simple dependent operation puts the interpreter at the order
of **150 native instructions per emulated instruction**, against the 30-80 a
tuned core needs and the 250 that entry 18 derived by a cruder route. The
headroom is real and it is in userspace.

Two supporting measurements close off the obvious objections:

- **Fetch is not the problem.** With `XCHAL_ICACHE_SIZE=0` every fetched
  instruction comes from PSRAM, which looked like it might be the whole cost.
  It is not: identical arithmetic over ~1 KB of code costs 30.5 ns/op and over
  ~8 KB costs 31.7 ns/op, 3.8 percent apart. A rewrite need not fear growing its
  code body.
- **The ROM's location was the biggest single win available.** A dependent read
  from the flash window costs 738 ns against 53 ns for PSRAM, a factor of 14, and
  the ROM is 72.6 percent of the accesses. The prediction was 1290 ns of the
  4922; the measured saving from moving the ROM to PSRAM is 1239 ns, and the
  throughput gain is 33.8 percent. See "The ROM's location is worth 33.8
  percent" above -- it is measured now, not a lead.

What the prof counters cannot give is the native instruction count itself: the
boards offers no retired-instruction counter (`CCOUNT` is cycles only), and the
host has `perf_event_paranoid` at 4 with no `CAP_PERFMON`, so `perf stat -e
instructions` refuses. The ~150 above is derived from cycles and the measured
cost of an operation, not counted. It is enough to justify the rewrite, which is
the question that matters; a rewrite measured against the 204k instructions/s
baseline above settles it definitively.

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

## The touch daemon costs it a third of its speed, and now stands down

Measured 2026-11-10, same boot, same binary:

```
S50tbtns polling   134,496 instructions/s
S50tbtns stopped   206,091 instructions/s
```

**34.7 percent**, or 53 percent more throughput without it. The arithmetic says
why rather than it being a mystery: one touch read costs about 3957 us on this
bit-banged I2C bus (TCH-003) and the daemon polls every 15 ms, so roughly a
quarter of the core's time goes into waiting out bus traffic that is nearly
always going to report "nothing new".

Both of those constants are right for what they were chosen for — 15 ms buys tap
latency and 3957 us is the hardware — so the fix is not to poll slower. **The
daemon now stands down while a fullscreen app owns the panel.** It reads
`/var/run/tamafb.pid`, which `run-tama.sh` already writes, validates that pid
against `/proc/<pid>/comm`, and sleeps in a 200 ms loop while a process named
`tamafb` is alive.

The validation is not paranoia. Pids are reused and the file survives a crash, so
trusting it would let a stale entry turn the buttons off permanently and
silently — a worse bug than the one being fixed. It also caught a real case
during testing: a launch wrapped in a shell has `comm` = `bash`, not `tamafb`,
and is correctly ignored.

Standing down forces a release first, so a finger down at the moment the app
starts cannot leave a key stuck, since nothing below runs to time it out.

Verified on hardware, both transitions in the daemon log:

```
tbtns: standing down for tamafb
tbtns: resuming, tamafb has exited
```

Two things to keep in mind. Anything *measuring* the emulator must account for
this: the same binary read 204k before a reboot and 141k after, and the
difference was the daemon, not the reboot, which produced a wrong conclusion
before it was caught. And a run of `tamafb` started by hand without a pidfile
does not trigger the stand-down — only `run-tama.sh`, or anything else that
writes the file, does.

## Still to do

- **Not a service.** It is launched by hand. A `S51tamafb` now has a stronger
  reason to start early than it did: if the ROM moves to PSRAM for the 33.8
  percent, the 512 KB allocation must happen before the heap fragments, which is
  minutes (KRN-004). The binary is also 249 KB static and *exec* is subject to
  the same contiguity rule (KRN-001): measured at a fragmented moment, the same
  binary started in the foreground every time and died with SIGSEGV about one
  launch in three when backgrounded with `&`. Starting it early, from rcS,
  avoids both problems at once. `/home/run-tama.sh` retries up to 8 times, which
  works but is luck.
- **Move the ROM to PSRAM — tried, and it does not fit.** The code is written
  and deployed (`patches/harness/0001-tamafb-rom-in-psram.patch`), and it
  correctly falls back with `rom: flash window`. It never takes the PSRAM arm
  because the allocation is refused: `malloc(524288)` fails while
  `malloc(524000)` succeeds, and splitting the ROM in two does not help --
  `malloc(262144)` twice gives one success and one failure, since each block
  pays its own header out of the same budget. Getting the 33.8 percent needs
  the allocation to happen while an order-8 block still exists, which means a
  `S51tamafb` started from `rcS`. See KRN-004.
- **`tbtns` standing down is done.** Worth 34.7 percent, deployed and verified;
  see the section above. A `S51tamafb` should write `/var/run/tamafb.pid` the
  same way `run-tama.sh` does or the stand-down will not trigger.
