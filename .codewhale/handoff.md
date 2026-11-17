# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-18T01:45

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**Bosch ships a data-injection firmware for the BHI260AP and it boots on this
board — that is the door to feeding the AI engine synthetic data, and it was
found by looking in Bosch's API repo rather than SensorLib.** The IMU otherwise
works as before: accel at ~97 Hz, a posture vocabulary pinned by a host test, an
IMU window, and KLIO learning and recognising a gesture trained by hand. `music` plays FLAC on the built-in speaker with the
decoder out of the `modules` partition. Bluetooth has a controller slot and the
pad lives there. Both TinyDesk 0.5.2 fixes are in. NVS is 64 KB, the OTA slots
are equal, and **the app is at 0x20000, not 0x10000**. The flash is four equal
4 MiB regions; internal RAM is accounted by `mem`.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `dc4e424`
  (core-log **entry 57**). `.ai/core.md` is RESTRICTED — do not edit without an
  explicit request. Reference records live in `.ai/core-reference.md`.
- **`.ai/core.md`'s Purpose line is stale and deliberately untouched.** It says
  "Escapement is a port of linux". The project is TinyDesk; the Linux line
  closed 2026-11-11. Fixing it needs the operator's word, because that file may
  only be edited when asked.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `e48940e`.** Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream
  `tinydesk-project/tinydesk`).
- `/run/media/vash/ROCKBOX` is the operator's source backup. **Never write to it
  or unmount it** (`/dev/loop0p1`, image
  `/run/media/vash/USB_HDD/Backups/ROCKBOX_2026-08-01.img`).
- The watch's SD card is `/run/media/vash/TEST` when it is in the reader. It is
  in the watch now, so read paths from the board, not the PC.

## What is proven on hardware

```
music <file.flac>   |   music stop   |   music   (status)
modflac             alias of the above, kept because the logs use the name
decoder             micro-flac v0.3.0, in `modules` (26,685 B), .rodata in
                    `moddata` (1,976 B)
sink                built-in MAX98357A over I2S
```

- **A 5:58 real track:** `state done, 15792504 frames, 0 errors, 0 short
  writes`. 15,792,504 / 44,100 = 358.1 s, matching the track. Operator
  confirmed by ear: "sounds good."
- **The 44.1 kHz tone:** exactly 352800 frames, 78 decode calls.
- **The 48 kHz tone:** `rate 48000`, exactly 192000 frames = 4.000 s. The rate
  now comes from the file, not a constant.
- **Internal SRAM during playback: 20,707 B free; after: 33,727 B.** The PSRAM
  buffers are genuinely released.
- **Clean rebuild is reproducible:** `make clean && make flac` produced a blob
  byte-identical (sha256 `dd47bce9…`) to the flashed one.
- **The stale-link guard fires** — and again for real (entry 53): a blob linked
  for `0x42ad0000/0x3ccd0000` against a live `0x42ae0000/0x3cce0000` was refused
  with both pairs named, decoded nothing, did not crash.
- **After the entry-53 relink, re-verified end to end:** 44.1 kHz tone
  `state done, 352800 frames, 0 errors, 78 decode calls`; 48 kHz tone
  `state done, 192000 frames, 0 errors, rate 48000`. Operator heard it:
  "i heard the tone."

```
klio learn          train a gesture           progress 0..100, then an index
klio detect         five-step lifecycle       RECOGNISED pattern 0, count 5.1
klio / klio stop / klio reset
imu / imu accel / imu rate / imu scan / imu inject
```

- **KLIO recognises** (entry 52): a gesture trained by hand, then `klio detect`
  and the same gesture again gives `RECOGNISED pattern 0, count 5.1` then `5.5`,
  45 frames over one run.
- **Accelerometer:** `x -0.01 y -0.07 z +1.03` flat, ~97.4 Hz, confirmed two
  ways. Firmware boots in 3.26 s from `moddata+0x10000`.

## The trap that cost this cycle — READ BEFORE TOUCHING MODULES

**Both mmap addresses move when the app image changes size.**

```
                 modules (instruction)     moddata (data)
with inline dec     0x42ae0000              0x3cbe0000
after removing it   0x42ad0000              0x3cbd0000
after the quad table 0x42ad0000             0x3ccd0000   (entry 43)
```

The **instruction** base tracks the app image size; the **data** base tracks the
`moddata` partition's offset. Entry 43 moved the partition without moving the app,
so exactly one of the pair changed — a case neither earlier guard test covered.

A blob linked for the old pair **still executes** — the entry point is reached by
offset from whichever instruction base is live — and reads its constants 64 KB
away. Because micro-flac compares the input's first four bytes against
`MAGIC_BYTES` in `.rodata` (verified: `fLaC` at `.rodata+0x7b0`), the only
symptom was `FLAC_DECODER_ERROR_BAD_MAGIC_NUMBER` on files that were fine.

**IT BIT AGAIN on 2026-11-17 (entry 53), and the runtime guard is why it was
cheap.** Five app rebuilds for the IMU and KLIO work moved the instruction base
0x42ad0000 -> 0x42ae0000 while `moddata.at` still held entry 43's pair; `music`
then refused every file. The guard turned it into one status line naming both
pairs. **What failed was the repair path, not the guard** — see below.

Four mechanisms now catch it:
- `modload_header_t` carries `linked_inst` / `linked_data` / `linked_host`
  (ABI 3). `modload_prepare()` compares all three to the live values and refuses,
  leaving the reason in `modload_why()`.
- `modules/moddata.at` holds the pair plus **`app=<size>`**, the size of the app
  image they were measured against.
- **`check_addr_fresh.py` refuses to link a module when that size disagrees with
  the current build**, and `flac` depends on it through a phony target so it
  cannot be skipped. This is the mechanism that closes the five-rebuild gap.
- `make modaddr` re-measures by reading `/root/modaddr.txt` out of the raw
  storage partition with esptool — **and it now parses `partitions.csv` for the
  storage row.** It held `0x620000/0x7E0000` from before the four-region table,
  so it was reading 1.5 MiB below storage and could not repair anything. If you
  ever see it "find nothing", the geometry is the suspect.

**So: after ANY change that alters the app image size — `make modaddr`, rebuild
the module (`make flac` now refuses without a fresh measurement), flash all
three regions.** Neither the build nor the loader will let a stale blob through
silently now.

**What the guard does NOT cover:** `linked_host` moves on nearly every build
regardless of image size, and only the loader checks it. A build that changes
code without changing size could still ship a stale one — a runtime diagnostic,
not garbage. Reading the symbol out of the fresh ELF is the obvious tightening.

## Flash and build

```
app      esptool write_flash 0x20000  build/tinydesk.bin   <-- 0x20000, unchanged
.text    esptool write_flash 0xC00000  modules/build/flac.bin   <-- 0xC00000 since entry 43
.rodata  esptool write_flash 0xE00000  modules/build/flac.data.bin  <-- 0xE00000 since entry 43
```

The IDF environment does **not** come from `export.sh` — it fails on a missing
`riscv32-esp-elf-gdb`. Use:

```bash
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=/home/vash/esp/idf-tools/tools/xtensa-esp-elf/esp-13.2.0_20240530/xtensa-esp-elf/bin:$IDF_PYTHON_ENV_PATH/bin:$PATH
python $IDF_PATH/tools/idf.py -C ports/twatchultra -B ports/twatchultra/build build
```

App size: `0x2e3090`, 53 KB free in `ota_1`.

## INTERNAL RAM — measured 2026-11-17, with `mem`

The board runs with ~26 KB of internal heap free. Where it went, all measured
on the watch by the port's new `mem` command:

```
internal heap     263,651 total   236,928 used   26,680 free
largest FREE block 9,728          <-- the real constraint, see below
min free (boot)   15,823
IRAM/exec pool    9,352 free, largest 7,168, one region min-free 308
PSRAM             428,612 used of 8.2 MB
static .bss/.data 97,620 B, plus ~113 KB IRAM text
```

| Consumer | Bytes | How known |
| --- | --- | --- |
| Bluetooth (radio up) | **47,304** | measured with `bt.enable` 0 vs 1 across a reboot |
| `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL` | **32,768** | survives BT off; it is the IDF reserve pool |
| shell task stack | 24,576 | `TDSH_SHELL_TASK_STACK`, one block |
| everything else | ~132 KB | ~300 allocations, avg <1 KB |

**The 32 KB is not a leak.** `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768` (the
IDF default) withholds 32 KB of internal RAM from ordinary `malloc()` so that
task stacks and DMA can still succeed when internal memory is full. Its Kconfig:
"the memory in this pool is not given out when a normal malloc() is called".

**THE CONSTRAINT IS THE LARGEST BLOCK, NOT THE TOTAL.** 26 KB free but only
9,728 in one piece — which is exactly why `pm start` failed needing a 10,240
stack. Freeing more bytes does not fix that; allocating emulator stacks EARLY,
before the small allocations chop the heap up, would.

**Distribution of the 236,928 allocated bytes:** 2 blocks of 17-32 KB = 57,344
(24%); 19 blocks of 2-4 KB = 64,088 (27%); 5 blocks of 4-8 KB = 35,840 (15%);
19 of 1-2 KB = 30,192 (12%); 285 blocks ≤128 B = 13,608 (6%). So the mass is in
~45 blocks, not thousands of small ones.

**PSRAM placement is already right** — `s_vt` 36 KB, `games_database` 48 KB, the
panel frame buffer, emulator ROMs and frame buffers all live there.

## THE `mem` COMMAND (new, entry 42)

```
mem heap      per capability, with the lifetime low-water mark
mem regions   IDF's per-region list
mem tasks     per task: unused stack bytes + internal or PSRAM
mem blocks    size histogram of allocated blocks + the largest by address
```

Split into four **because the desktop's terminal is 24 usable rows and the
mirror carries only what is on the screen** — longer output scrolls away and
cannot be recovered from a capture. Each subcommand fits.

**Turning a slot's reconnect off does NOT keep the radio down across a boot**:
`bt off combo` + `bt off controller` + reboot still reported "Radio: up". Use
`board set bt.enable 0` for a no-Bluetooth baseline, and put it back afterwards.

## THE IMU: A BOSCH BHI260AP (entry 44)

```
part        Bosch BHI260AP "smart sensor"   (U7, on the schematic)
I2C         0x28   on the shared bus
interrupt   GPIO 8 (SENSOR_INT)
power       AXP2101 ALDO4 @ 1.8 V  <- nothing enabled it until entry 44
firmware    ~121 KB, uploaded to the part's RAM on FIRST USE
protocol    Bosch BHY2 SDK, vendored in components/bhy2 (BSD-3-Clause)
```

**The rail was the whole reason it was invisible.** An unpowered I2C device
does not NAK — it is absent — so the symptom is "this board has no IMU". Enable
ALDO4 (bit 3 of 0x90, 1.8 V = code 13 in 0x95) and 0x28 appears. Same shape as
the amplifier on BLDO2 and the card on ALDO1.

**It is not an accelerometer.** A Fuser core runs algorithms — orientation, step
counter, gestures — and reports through a FIFO. That is the prize: work the
ESP32 would otherwise do in the 26 KB it does not have.

**What works, verified:**
```
imu          rail up, chip id 0x70 checked, blob found at moddata+0x10000,
             123,696 bytes uploaded and booted in 3.26 s,
             kernel 23.103 read back, returns to the prompt
```

**ACCELERATION NOW READS** (entry 45): `imu accel` prints five live readings of
`x +0.01  y -0.07  z +1.00` g — a watch lying flat. `imu rate` and `imu scan`
also work.

```
imu accel   five live readings, ~1 s apart, then total samples
imu rate    samples per second over a 2 s window
imu scan    the 35 virtual sensor ids the KLIO firmware carries
imu stop    accelerometer off (firmware stays loaded)
```

**Four bugs stood between the boot and a sample**, all in the "nothing happens"
family — see entry 45. The one worth remembering above the others:
`bhy2_is_sensor_available()` reads a bitmap that only
`bhy2_update_virtual_sensor_list()` populates, so asking first reports *no
sensors at all*. That produced a confident, wrong verdict that the firmware had
no accelerometer.

**The rate IS settled: ~98 Hz**, confirmed two ways — 195 samples in 2002 ms and
**13.8 bytes per sample** (a packet is 13). The earlier 973 Hz was an
instrumentation artifact. Three runs read 97.4 / 98.8 / 97.8 Hz.

**THE HANG, AND THE RULE IT TEACHES.** `bhy2_get_and_process_fifo()` reads into
`buffer_size - read_length`; when the part holds more than the buffer that term
hits zero, the read returns nothing, `remain_length` never changes, and the SDK
spins **for ever** with the shell task inside it. Ctrl+C cannot reach it.

```
buffer      2048 B (1.5 s at 100 Hz)   -- 512 B was under half a second
drains      ONE per poll, never "cleverer"
and         STOP THE SENSOR when nothing is reading it
```

A part streaming into an undrained FIFO builds 13 B/sample — a minute is 78 KB —
and the next command to touch it wedges. **A command that can freeze the terminal
is worse than no command.**

**The window works** (entry 46): Start menu → IMU, reads `BHI260AP`,
`fw kernel 23.103`, `+0.01 -0.06 +1.00`, and a climbing sample count.
`td_imu_ops_t` is in `td_sysinfo.h`, reached through a **weak
`board_imu_ops()`** — a `#define` cannot cross the component boundary.

**THE AXES, MEASURED (entry 47).** The operator's rotation data fixes the frame:

```
flat                x +0.01  y -0.07  z +1.00    z+ out of the glass
tipped fwd 90 deg   x -1.00  y  0.00  z  0.00    gravity moved z -> x
```

So **y runs left-right across the case, x runs strap-to-strap, z comes out of
the glass.** Tipping forward is a rotation about y.

**IT IS ACCELERATION, NOT ORIENTATION** — and the reason the numbers feel wrong
is physical: **an accelerometer cannot see yaw.** Spin the watch flat on a table
and the readings do not change, because gravity did not. Heading needs a
magnetometer or the fusion quaternion; it is not a bug.

The window leads with the posture and shows **tilt in degrees** derived from
gravity — `pitch = atan2(-x, sqrt(y²+z²))`, `roll = atan2(y, sqrt(x²+z²))`.
Checked against the operator's own 90° rotation: +90° pitch, 0 roll.

**THE VOCABULARY (renamed in entry 49 — it describes what the WEARER did):**

```
FACE UP      FACE DOWN          flat, glass to the sky or the floor
TILT UP      TILT DOWN          x axis; TILT UP = tipped forward towards the strap
TILT LEFT    TILT RIGHT         y axis
TILTED                          no axis dominant (0.70 = cos 45°)
```

`EDGE UP/DOWN` and `SIDE UP/DOWN` are gone: geometrically true, useless to a
wearer. **`imu_posture()` lives in `ports/twatchultra/main/imu_posture.c`, has no
ESP-IDF dependencies, and is shared by the window and the shell** — so
`imu accel` printing `posture FACE UP` verifies the vocabulary without opening a
window. `tests/test_imu_posture.c` pins all of it (14 checks), including the
sign of x against the operator's measured 90° rotation.

**Heading still reads `--`.** Both 69 (on-change) and 70 (wakeup) are subscribed;
neither delivered a value yet. If they never do, question `on_ori()`'s payload
layout next — it assumes 4×int16 quaternion at 2^14 then 3×int16 Euler at 2^8,
taken from the driver, not yet checked against Bosch's documentation.

**Reopen fix is IN CODE BUT UNVERIFIED.** `start()` was called once behind a
`static bool started`; closing calls `stop()`, so the second open never
re-enabled the sensors. Now called every tick with a bounded retry. **Tap IMU,
close, tap again** — that is the test.

**A harness note that has now cost three sessions: driving the Start menu over
serial is the least reliable part of this workflow.** Type-ahead resets after
~2 s, so `i` `i` searches twice from the top and lands on Image Viewer both
times; an arrow count of 18 overshot into Users. **Use the operator's finger,
or a shell command.**

**What the KLIO firmware carries (35 ids):** 1 3 4 5 6 7 10 12 13 14 15 16 28 29
31 32 37 38 48 50 52 53 55 57 59 61 63 67 69 70 75 77 94 112 127.
The interesting ones are **61 pickup-gesture, 63 ar, 67 wrist-tilt-gesture,
69 device-orientation, 70 ori-wakeup, 75 stationary, 77 motion** — the on-chip
work this board cannot afford to do itself. Try `BHY2_SENSOR_ID_DEVICE_ORI`
(69) next; heading on a wrist beats three acceleration numbers.

**Flashing the firmware** (separate from `make flac`, they share `moddata`):

```
moddata   0xE00000  codec .rodata at offset 0     (1,976 B)
          0xE10000  IMU firmware at offset 0x10000
make imu                   # flashes components/imu/bhi260_klio.img to 0xE10000
components/imu/fetch.sh    # regenerates that image from SensorLib
```

**Three mistakes this cost, all now comments where they bit:**
1. A 136-byte HIF write buffer vs the uploader's 4,096-byte writes; the SDK maps
   any non-zero return to `BHY2_E_IO (-3)`, so it read as a bus fault.
2. Hand-rolling `..._partly()` uploaded all 123,696 B then booted with
   `HOST_FW_VERIFY_ERROR` (boot status 0x78). Use the whole-image call.
3. `bus_delay_us` flooring sub-ms to a whole tick (1 kHz) made handshakes crawl.
   `read_write_len = 0` is NOT the fix — it breaks the upload with (-2).

## KLIO — THE ON-CHIP LEARNING ENGINE (entry 50)

```
klio           algorithm version, state, 25 pattern slots, 157-byte blobs
klio learn     reset, enable learning, print progress 0..100 live
klio detect    enable recognition, print what it matches
klio reset     clear patterns
```

**LEARNING WORKS, verified four times on the glass.** Repeat a gesture and the
engine's own progress climbs 0/20/40/60/80/100%, then it assigns a pattern index.

**RECOGNITION WORKS (entry 52).** Train by hand, `klio detect`, perform it again:

```
RECOGNISED pattern 0, count 5.1
RECOGNISED pattern 0, count 5.5     45 frames over one run
```

**THE FIVE-STEP LIFECYCLE — the third step was missing for two cycles:**

```
1  learn                              engine signals a new pattern
2  read_pattern(0)   -> 157 bytes     pull it OUT of the engine
3  write_pattern(0)                   into the RECOGNITION table
4  set_pattern_states(ENABLE, {0})    a written pattern is disabled by default
5  recognition on, learning OFF, BOTH reset fields 0
```

**A learnt pattern lives INSIDE the algorithm, never in the host table** —
`bhy2_klio.h` on `read_pattern`: *"After learning of a new pattern has been
signaled by a sensor data event, call this function to retrieve the learnt
pattern."* Enabling an empty slot is why detect had nothing to match.

**The pattern id is 0, not the index the engine reports.** That index numbers the
engine's internal slot; the header says *"only 0 supported now"* for the
recognition table.

**FIFO overflow was hiding it too.** Setup is several blocking I2C calls while
accel+gyro stream 100 Hz ≈ 2.6 KB/s; `meta 12 fifo overflow` mid-setup drops KLIO
frames, and a dropped frame is indistinguishable from no match. Recognition runs
its inputs at **50 Hz**, watermark **32**, drains between setup steps, polls 5 ms.

**Five bugs total across the two cycles, four of them mine:**
1. **KLIO is an ALGORITHM, not a sensor** — it consumes accel + gyro.
2. **A learnt pattern is DISABLED** — needs `set_pattern_states()`.
3. **`recognition_reset = 1` DELETES the pattern** — name reads as action, means wipe.
4. The probe once lied: `learn_index` zero-inits to 0 and `>= 0` read as "learnt".
5. **A pattern must be read out and written back** before recognition sees it.

**Frame decode, verified against raw bytes:** `d[5]` = pattern index,
`d[6..9]` = little-endian float repetition count.

**Frame format, determined empirically** (the SDK does not document it): **11
bytes**, `{instance, learn_index, progress, change_reason}` then the recognition
block. `-1` = nothing new learnt, `0xff` = nothing recognised. One byte longer
than the two packed structs account for — log all 11, not the first 6.

**Patterns are volatile.** The firmware is uploaded to RAM each boot, so LEARN AND
DETECT MUST HAPPEN IN THE SAME BOOT.

## DATA INJECTION — THE ARCHITECTURE, AND IT IS AN ALGORITHM PLUS INJECTION (entry 57)

**A DataInject image is an algorithm + injection. It is NOT a way to fake an
accelerometer.** Bosch's example says so in its second line:

```c
#define CUSTOM_SENSOR_ID  BHY2_SENSOR_ID_SWIM     /* 114, an algorithm */
firmware/bhi260ap_swim/DataInject_nobsx.fw        /* a different folder */
```

Injected data replaces the **algorithm's** input; the algorithm runs on it.
Feeding a physical sensor's output is not what this family is for.

```
bhi260ap/DataInject.fw             95,960 B   boots   sensors 4 6      no algo
bhi260ap_swim/DataInject_nobsx.fw 104,700 B   boots   sensors 3 8 12 17 **114**
bhi260ap_klio/...                 123,696 B   boots   KLIO, no injection
```

**`components/imu/DataInject_SWIM.fw` boots here and carries algorithm 114.**
That is the operator's pipeline proven on this silicon — with SWIM, not KLIO.

**No public KLIO + DataInject image exists.** That is a toolchain build.

### Why every earlier result looked the way it did

On the plain DataInject image there was no algorithm to consume injected bytes.
The part accepted them and emitted **one event per injection** (199-299 for
200-300) — which reads as success — with every payload the constant
`00 08 00 01 00 00 00`, identically for ids 4, 55 and 76. The count was the part
pacing off host data; the values were its own, because nothing wanted ours.

### Three instrument bugs fixed (each had been costing hard resets)

1. **`bhy2.c parse_fifo()` spins for ever when `event_size == 0`** —
   `read_pos += 0` never advances: no I2C, no output, no crash, indistinguishable
   from a hang. **This is what wedged `imu feed`.** Now skips a byte.
2. **`imu raw` read a 4-byte header from DATA fifos**, which use 2. Only the
   status channel is 4. An empty FIFO reported "251 bytes waiting".
3. **`imu raw` capped partial FIFO reads**, desyncing the stream for ever after
   (a header then claimed 64,768 bytes). It refuses rather than reads short.

`imu feed <seconds> <hz> <amp> <mode> <event id>` now injects and round-trips
without wedging. Its payload comparison is what made the constant value visible.

## SUPERSEDED — entry 56's images, still correct as far as they go

## DATA INJECTION — SOLVED IN PRINCIPLE: BOSCH SHIPS A DIAGNOSTIC FIRMWARE (entry 56)

**Read `ports/twatchultra/components/imu/README.md` before touching this.** It
holds the images, the protocol and the negative that makes them trustworthy.

**Bosch ships `DataInject.fw` for the BHI260AP** — 95,960 B, in
`boschsensortec/BHI2xy_SensorAPI` under `firmware/bhi260ap/`. It is **not** in
SensorLib, which is where every earlier search started, which is why the part
looked permanently injection-less.

```
BHI260AP KLIO        123,696 B   booted        boot 38  err 00
BHI260AP DataInject   95,960 B   BOOTED        boot 38  err 00   sensors 4 6
BHI360  data_inject   99,736 B   VERIFY-ERROR  boot 78  err 14
```

The BHI360 row is the control: same code path, same container magic, rejected at
verification. Chip id **0x70 = BHI260**, **0x7A = BHI360**.

**The trade is unavoidable:** DataInject has only two virtual sensors (4, 6 — the
accelerometer), so the IMU window, the posture vocabulary and KLIO all stop
working while it is loaded. Choose per session.

### Bosch's injection sequence — step 2 explains every earlier failure

`examples/swim/swim_data_injection/swim_data_injection.c`:

```
1  bhy2_set_data_injection_mode(STEP_BY_STEP)
2  bhy2_set_virt_sensor_cfg(SENSOR_ID, rate, latency)   <-- CONFIGURE FIRST
3  drain the status FIFO
4  bhy2_inject_data(timestamp, 6)    full timestamp, its OWN call
5  bhy2_inject_data(event, 7)        then the event (id + 3 x int16)
6  drain the FIFO between injections
```

Injection supplies data **for a sensor**. With none configured nothing is
listening, so packets are accepted and dropped — exactly what entry 55 measured
from outside.

### Not finished

`imu feed <s> <hz> <amp>` implements the sequence, generates a sine and
round-trips it — but **wedges the shell on the drain**, and `imu raw` desyncs the
FIFO stream when capped short. Instrument bugs, not evidence. Bosch's loop sets
the async bit, drains against a generous buffer, then clears it.

## SUPERSEDED — entry 55's verdict, which was true only of the KLIO image

## DATA INJECTION — SETTLED: IT REACHES NOTHING (entry 55)

**The question is closed. Do not reopen it by guessing another framing.**

```
imu watch <seconds> <shape>     inject +7.32 g on Z, watch the accelerometer
```

A value gravity cannot produce, and a RANGE over thousands of samples so a single
hit would count. Sensor running throughout.

```
shape 4  driver ids 76/79   971 sent, 762 samples, z 0.89..0.92, 0 above 4 g
shape 1  floats+timestamp   728 sent, 610 samples, z 0.89..0.90, 0 above 4 g
shape 2  inject ids 55/58   777 sent, 611 samples, z 0.89..0.89, 0 above 4 g
```

The sensor keeps delivering, so injection does not suppress it — it simply does
nothing. **This negative is stronger than the KLIO ones**: KLIO reports a
summary that can stay at zero for a dozen reasons; the accelerometer reports the
sample itself, so this is about the data path and nothing else.

**The one explanation left is Bosch's own**: `dinject`'s help says *"Ensure that
the requisite application specific Data Injection firmware is loaded"*. A
different image — and it would REPLACE the KLIO image, not add to it. Getting it
means Bosch's toolchain. **That is a decision for the operator, not a next step.**

**If the goal is only to classify motion**, the ESP32-S3 itself is the honest
answer: 985 KB of app slot free, vector instructions, six channels already read
reliably at 97 Hz.

## HOW THE COMMAND PATH ACTUALLY BEHAVES (entry 54, still true)

**Entry 51's "inert" verdict was wrong, and the method was blind.** `bhy2_inject_data()`
ends at `bhy2_hif_exec_cmd_generic()`, which writes to `CHAN_CMD` and returns
`BHY2_OK` when the I2C write completes — it never reads a status response. So
`rc 0` meant "the bytes left the ESP32" and nothing more.

What the status channel actually says:

```
imu param 0900                        rc 0, 4 B      positive control: it answers
imu cmd ffee     unknown id           status 0x000f CMD ERR, `ee ff 05 00`  reason 05
imu cmd 0008     INJECT_DATA bare     status 0x000f CMD ERR, `08 00 01 00`  reason 01
imu cmd 0007 02000000  step-by-step   accepted silently
imu cmd 0008 <payload> after that     ACCEPTED SILENTLY
```

**So the command exists, parses, and does not error.** Two distinct refusal
reasons for two distinct failures, which is what makes reason 01 not "unknown
command".

**What is still true: accepted data does not reach the accelerometer.** Sensor
stopped, FIFO verified empty by a raw header read, a 7-byte frame (one-byte
sensor id `04`, z = +2.5 g) left the FIFO empty and the reading unchanged.

**The open question is where it goes** — most plausibly the ALGORITHM path
rather than the sensor outputs. That would be exactly the operator's use case:
run KLIO learning while injecting, see whether progress moves with nobody
touching the watch. **This is the next probe and it is small.**

### Commands for poking at the part

```
imu reg                 the documented registers, decoded
imu reg <hex>           one register
imu raw <reg> [len]     any register, under the SDK
imu cmd <id> [payload]  bare command id; then error reg + status channel
imu status              listen to the status channel alone
imu param <hex id>      read a parameter (the positive control)
imu inject [1..6]       one or all payload shapes, with status reporting
```

### Two defects found here

- **`bhy2_hif_set_inject_data_mode()` clears the async status bit and never
  restores it.** Every call leaves the part with async status off for the rest
  of the session (reg 0x06 read 0x00 where boot leaves 0x80). Restored
  explicitly after the sweep. An SDK defect, invisible until something needs it.
- **`imu raw` on a FIFO channel hangs the shell task** and needs a hard reset.
  The channels are not plain registers: read a 4-byte header (word + bytes
  remaining), then a second read of exactly that length. Fixed.

### The hazard that bit twice this cycle

**`imu accel` can block the shell task** — the sensor-configuration call blocks
and takes the terminal with it; two hard resets this session. It does not
reproduce every time. Prefer `imu rate`, which stops the sensor when it finishes.

## ENTRY 51's superseded note — kept for the record

**Six payload shapes, all accepted by the protocol, all inert.** Including the
**28-byte float frame** the algorithm itself consumes (`bhy2_klio_log_frame_t`:
timestamp + accel[3] + gyro[3]):

```
8B  id+int16          10B id+ts+int16     8B ACC_WU+int16
28B float frame       30B float + KLIO_LOG id
28B float in REAL_TIME mode instead of STEP_BY_STEP
```

All read `z +1.00` against an injected `+2.50 g`. **No `meta 11 SENSOR ERROR`
for any of them** — the command is acknowledged and never reaches the sensor
pipeline.

**So it is not a payload-shape question.** Either this build refuses host
injection regardless of shape, or the mechanism needs setup this port does not
do. Both are expensive. **Do not re-litigate the payload.**

**Two method lessons worth keeping:**
- **Test with the watch STILL.** The first run had the operator rocking it and
  the readings wandered (+0.65…+1.08), which looked like partial success. The
  still run gave +1.00 six times. **The pass condition is a specific impossible
  VALUE (+2.50), never "did the number change"** — that is the only reason the
  noisy run was not called a positive.
- **Verify the effect, never the acknowledgement.** The API returned success for
  every shape it ignored.

## RESTART CAUTION

The board went **silent twice** after a flash this cycle and needed
`esptool chip_id --after hard_reset`. No crash captured, did not recur on the same
build, trigger unknown. **A silent board after flashing is not a dead board.**

## THE FLASH: FOUR EQUAL 4 MiB REGIONS (entry 43, 2026-11-17)

```
region 1  0x000000 - 0x400000
  nvs        0x0009000    64K
  phy_init   0x0019000     4K
  ota_0      0x0020000  3968K   <- THE APP LIVES HERE (1005 KB free)
region 2  0x400000 - 0x800000
  ota_1      0x0400000  3968K
  coredump   0x07e0000    64K
  otadata    0x07f0000     8K
region 3  0x800000 - 0xC00000
  storage    0x0800000     4M   littlefs, mounted at /
region 4  0xC00000 - 0x1000000
  modules    0x0C00000     2M   .text,  MMAP_INST
  moddata    0x0E00000     2M   .rodata, MMAP_DATA
```

**The slots are 3968 KB, not 4 MiB** — the boot overhead sits inside region 1,
so both slots take its smaller size. 4+4+4+4 needs 176 KB more than the chip has.

**NVS is 64 KB** because 24 KB had 5 of 6 pages used and GC never stopped; when
it fills, `nvs_flash_init()` returns `NO_FREE_PAGES` and `app_main()` calls
`nvs_flash_erase()` — accounts, networks and bonds, silently.

**storage is 4 MiB** and held 11 dirs / 29 files / 23,047 B before the change.
Shrink it only by extract-and-rebuild, never truncation (entry 25).

**Flash offsets, and the one that will bite:** `0x20000` app, `0xC00000` module
text, `0xE00000` module data. The old module text offset `0xE00000` is now the
DATA partition — writing a blob there would put code in the rodata window.

**Two module-mechanism bugs were fixed here** (see MODULE LINKING below).

**Reading NVS properly** (the first attempt got nonsense): states are 2-bit
fields in the page's **bytes 32-63**, entries start at **64**, 126 per page.

**Reading NVS properly** (the first attempt got nonsense): states are 2-bit
fields in the page's **bytes 32-63**, entries start at **64**, 126 per page.

## TINYDESK 0.5.2 FIXES — both applied 2026-11-17

Taken from upstream by cherry-pick; both faults existed here.

- `03fc1b9` **Store**: `apps/store.c` divided `st.done * 100` by
  `s_apps[s_busy_app].size`, which is **0 for an app listed offline** — an
  offline double click panicked the board. `on_install()` also had no guard for
  a hidden Install button. Clean apply; only `docs/VALIDATION.md` conflicted and
  was left at our version (it is upstream's QA record).
- `73a3bd7` **descriptors 0-2**: we run `CONFIG_ESP_CONSOLE_NONE`, so nothing
  opened fds 0/1/2 and the first files opened got them — a log line went into
  the file, and a read-only one made littlefs assert. `hold_std_fds()` registers
  `/dev/tdnull` and opens it three times at the top of `app_main()`.
  **Checked first:** the USB mirror writes via `usb_serial_jtag_write_bytes()`
  (`link_usj.c`), never fd 1; and the shell replaces the global `stdout` with
  its own `funopen()` stream per task (`tdsh_bridge_esp.c`).

**The fix's own effect is NOT directly observed** — reproducing it needs a
failed Store download with WiFi. `cat /dev/tdnull` from the shell was
inconclusive: tdsh resolves through `tdsh_path_to_real()` and never reaches
IDF's VFS. The argument it works is from `components/vfs/vfs.c:80-106`, which
can only reject a length of 1, a length over `ESP_VFS_PATH_MAX`, a bad prefix,
or OOM.

## THE CONTROLLER SLOT — what it took, and the trap in it

```
TD_BT_SLOT_CONTROLLER = 3, TD_BT_SLOTS = 4
TD_BT_KIND_GAMEPAD    appearance 0x03C3 (joystick) or 0x03C4 (gamepad)
```

The Xbox pad was in the **keyboard** slot because a gamepad advertises HID and
no keyboard appearance, which made it an "other HID device" — offered to any
slot. `collect()` now withholds a GAMEPAD from every slot but the controller's,
so the trap is gone, not just moved.

**The silent hazard: `slots_load()` demanded `n == sizeof(s_slots)`.** Four
slots make the array bigger, so that check would fail and zero the blob — both
pairings gone, no error. It now accepts any whole number of slots (the new one
is appended, so the old blob is a prefix) and clears only the tail. Verified:
the watch still reports **2 stored in the board's NVS** after two flashes.

**Moving a device:** `ops_pair()` clears any OTHER slot holding the same
address and closes its connection, but keeps the bond — deleting it would force
the controller back into pairing mode. A saved slot holds an address and a
name, never a kind, so storage cannot identify the pad in the keyboard slot;
re-pairing it into the controller slot is what moves it.

**VERIFIED 2026-11-16 23:05, move and reboot both.** After the operator paired
the pad into the Controller slot: `controller ready, Xbox Wireless Controller
78:86:2E:BB:45:79 (267 reports)`, `reconnects by itself`, keyboard `not paired`.
After a **hard reset**: same arrangement, `2 stored in the board's NVS`, and
the pad **already reconnected on its own** — `ready (29 reports)` inside the
first half-minute. That exercises the 4-slot blob written by the move and read
back by `slots_load()` on the next boot.

**The report counter resets with the connection** (267 → 29) and stops climbing
while the pad is idle, because an idle gamepad sends nothing. Non-zero after a
boot proves input arrived; continuous input is `last:` in the window.

**NimBLE:** `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` 3 → 4. The board sets
`MEM_ALLOC_MODE_EXTERNAL`, so the extra connection is PSRAM, not internal RAM.

**Rows are counted, not assumed.** Client height on the watch, measured:
**23 rows** (26, less the taskbar, less frame+title), **22** with the title bar
set to Large. Slots are top-anchored at row 3; the scan section is anchored to
the bottom and gives up list rows (4 at 23, 3 at 22). `COL` went 11 → 12
because "Controller" is ten characters and read "Controlleridle".

`api_stable` failed on the two enums — the gate working. Re-recorded with
`--record`; the diff is two added lines. `TD_VERSION` stays 0.4.0 (semver rules
are from 1.0, and the text lives in six port CMakeLists).

## Driving the watch's console

**Reboot, wait ~22 s, F10, Enter, then type.** That is the whole recipe and it
has never changed. `/tmp/drive3.py` does it: it clears the screen before each
command, because the display mirror accumulates and old scroll reads as new
output.

**THE OPERATOR'S RULE, AND IT IS THE FIRST THING TO TRY: when in doubt, reboot,
F10, Enter — that always lands in the Terminal.** It works because the Start menu
opens with **Terminal already selected**, so a bare Enter after F10 is enough. No
screen inspection, no type-ahead, no arrow counting. This has now cost three
sessions:
- **type-ahead resets after ~2 s**, so `i` `i` searches twice *from the top* and
  lands on Image Viewer both times;
- an arrow count of 18 overshot into Users.

**Launching a specific app from serial is the least reliable part of this
workflow. Two reliable routes: the operator's finger, or a shell command.**

- **Ctrl+Q is `0x11` = XON** and is swallowed by the line discipline unless
  `IXON|IXOFF|IXANY` are cleared. Every serial helper must clear them.
- `tdsh` is **not bash**: no `wc`, `head -4` fails (`head <file> [-l lines]`),
  and **double quotes trip the parser** — `tdsh: unmatched quote or escape`. Use
  globs (`01*`) or unquoted paths. Glob results are **relative**, and the
  playback task does not inherit the shell's cwd, so `cd` is not a substitute
  for an absolute path.
- The status line is wrapped at 40 columns mid-word (`fra mes`). Strip all
  whitespace before matching.
- esptool leaves the port at its own baud and the console goes silent;
  `chip_id --after hard_reset` restores it.
- An orphaned helper holding `/dev/ttyACM1` makes every esptool call fail while
  the board looks healthy. `fuser /dev/ttyACM1` names it.

## Reading a backup's digest

**A digest read before a rebuild will not match one read after** — the app
descriptor's build time moves, and esptool appends an image digest computed over
it. Measured once: after deleting a dead header, the rebuild produced the same
size (0x2e3090) and differed from the flashed image in exactly 69 bytes — 4 for
the timestamp `16:45:02` -> `16:59:14`, the descriptor's ELF hash, and the
trailing 32-byte digest. Every byte in between was identical, so the board ran
the same code and was not reflashed. **Compare regions, never whole digests.**

Pre-quad images (all in the old layout, all superseded by entry 43):
`music-only-2026-11-16-1654.bin`, `music-module-2026-11-16-1637.bin`,
`modules-2026-11-16-1320.bin`, `baseline-2026-11-16.bin` (entry 23, PRE-FLAC —
do not restore expecting a working board).

## Documentation, as it now stands

Rewritten 2026-11-16 (core-log entry 31). The README describes TinyDesk on the
watch, with the Linux era in its own section marked as abandoned — not deleted,
because `docs/panel-geometry.md` is still the only measurement of the chamfers
and the current desktop is laid out inside it.

- **`docs/README.md` sorts every document into current and archive.** Three are
  current (`nes-accuracy`, `gbc-scoping`, `pokemini-scoping`); the other eleven
  are Linux-era. Two archive documents are still load-bearing:
  `panel-geometry.md` and `panel-transfer-cost.md`.
- **Live scripts:** `watch-backup.sh`, `tamacore-host.sh`, `tamacore-sync.sh`.
  Everything else in `scripts/` targets the Linux image. `watch-backup.sh` runs
  out of the *old* toolchain's Python at
  `build/idf-tools/python_env/idf5.1_py3.14_env` — it works, and it will break
  if `build/` is deleted.
- **This repository has no `LICENSE` file.** The old README claimed GPLv3; the
  firmware fork is MIT. Unresolved, recorded as a decision owed.

## START-MENU LAUNCHERS — one missing import, and both are versioned now

`py` is MicroPython with a `tinydesk` module **registered but not auto-imported**.
`/root/Apps/pm.py` called `tinydesk.shell(...)` without `import tinydesk`, so
every launch of **Pokémon Mini** from the Start menu printed a `NameError`
traceback. **`tama.py` already had the import and was never broken** — the two
failures looked identical from outside.

Both launchers now live in `scripts/apps/` (they were in no repository, and a
full-image flash would have lost them) with the install commands, and both use
the firmware's default ROM path instead of carrying a long path with spaces:

```
pm.py     import tinydesk / tinydesk.shell("pm start")
tama.py   import tinydesk / tinydesk.shell("tama")
```

Verified **from the Start menu** — the path that failed — at 71.8 fps.

**Navigating the menu:** `td_type_ahead` steps to the *next* match, so `p` selects
Pixels and a second `p` reaches Pokémon Mini; `t` reaches Task Manager before
Tamagotchi. `End` goes to the very bottom, which is the users section and
"Sign out", not the app list.

## GUEST STACKS BELONG ON PSRAM — a class, not two coincidences

`gb start` and then `pm start` both failed with **"no task"**. `GB` was 8 KB and
`PM_STACK` 10,240 B of internal DRAM, against a largest free internal block of
**9,728** with the desktop up and a Terminal open. Both moved to PSRAM with
`xTaskCreatePinnedToCoreWithCaps(..., MALLOC_CAP_SPIRAM)` and both delete with
`vTaskDeleteWithCaps()`.

Safe for a specific reason, not by luck: those tasks run the emulator and
nothing else — the ROM is loaded on the caller's task, buffers are already
PSRAM, and they open no file and map no partition, so they never disable the
cache a PSRAM stack lives behind.

**AND TWO WAYS THE REPAIR ITSELF WAS BROKEN, both found by entry 43:**

1. **The publish came after the refusal.** `modload_prepare()` wrote
   `/root/modaddr.txt` *after* the stale-link check, so a stale blob returned
   early and never wrote the file the build reads to make a fresh one — the guard
   made itself permanent. Publish now happens as soon as both mappings exist,
   before anything that can refuse.
2. **`moddata.at` was not a prerequisite of `flac.bin`.** Addresses are read at
   Makefile-parse time, so a changed file only relinks if make knows it is a
   dependency. Without that, `make modaddr && make flac` printed "gen_addr:
   unchanged" and shipped a blob for the old address. Now a prerequisite.

**Two instances means a class.** Still on internal stacks and worth a sweep:
`tama.c`, `nes.c`, `gbcbench.c`, `pmbench.c`. `nes.c` fits today, but that is a
measurement about one game and one ROM.

## Backups taken and verified

```
inject-swim-2026-11-17-1653.bin      16,777,216 B   THE CURRENT BOARD
   sha256 38ecbce7b445347b9f718ab1ab9d99a344b7247c426dd3bfee106828aef3cb9c
   app 0x20000 = 9103bec7ed8f344f, module text 0xC00000 = af5c5ca5d5f07345,
   module data, IMU fw -- all four byte-identical to the build.
   verify OK, board came back. Has the parse_fifo fix, the FIFO-header fix,
   `imu feed`, `imu boot`, `klio save`/`compare`. KLIO image loaded, music ok.

inject-settled-2026-11-17-1507.bin   STALE -- seven app reflashes behind
   sha256 a47f365950338e61e78f5987e8030781a9992fac887db3f26bfb7b134d71fe21
   app 0x20000 = fda18b2a0e661f79, module text 0xC00000 = 6da1645b4b962ccc,
   module data 0xE00000, IMU fw 0xE10000 -- all four byte-identical to the build.
   verify OK, board came back. Has `imu watch`, `klio inject` with the motion as
   arguments, and the trace-to-file behaviour.

imu-probe-2026-11-17-1401.bin       STALE -- six app reflashes behind
   sha256 b37985bd95084e9aab08b54bddd299e12e4175d9c79af29be217b8b68317c585
   app 0x20000, module text 0xC00000, module data 0xE00000, IMU fw 0xE10000
   -- all four byte-identical to the build. verify OK, board came back.
   KLIO learn + recognise, posture names, music working, and the entry-54
   instruments (imu reg / raw / cmd / status / param / inject).

klio-music-2026-11-17-1300.bin       STALE -- five app rebuilds behind
   sha256 a4aeea4a1038d740d7bdc1fa4b79e9927ec7aef5ded75313bc8627fe60405176
   Has no `imu cmd`, `imu raw` or `imu param`; predates the injection findings.

klio-2026-11-17-1247.bin             16,777,216 B   same app, STALE MODULES
   sha256 c850ae2c25a5e032b20981cc3ce6afe0ecbe2c23367a7b1931efefde1df87d3b
   Taken 40 minutes earlier; differs from the above ONLY in the two module
   regions (linked for 0x42ad0000/0x3ccd0000, so `music` refuses). Restoring it
   rolls `music` back to broken -- prefer the 1300 image.

imu-window-2026-11-17-1315.bin       16,777,216 B   superseded
   sha256 1325c673bb8dca1be00b98a1772f0b4dc79491b7c6423538f9090f8ddea97557
   predates entry 49 (old posture names) and the KLIO work.

quad4m-2026-11-17-1030.bin           16,777,216 B   quad layout, pre-IMU
   sha256 535776ecbcdaa0462cf98ce5c06db596bf3667c8eac00651d1f034c5664b8dac
   app 0x20000 = 987b7852   module text 0xC00000 = ea99cb70
   module data 0xE00000 = 9b47992b -- all three byte-identical to the build
   storage 0x800000, 4 MiB littlefs. 29 files, both pairings, account.

pre-quad-2026-11-17-1000.bin         16,777,216 B   THE PREVIOUS LAYOUT
   sha256 fa736ea629fd90db05bf5a4d947e917423916366b131d1a098ea3d64aaa897ca
   3008 KB slots, storage at 0x620000 (8 MB), modules at 0xE00000.
   The only image in that layout: the clean rollback, and where a coredump
   written before the change would be (0x600000).

nvs64-2026-11-17-0755.bin
   64 KB NVS, 8064 KB storage, modules at 0xE00000 -- superseded layouts.

pre-nvs64-2026-11-17-0740.bin
   app at 0x10000, nvs 24 KB. Two layouts back; keeps the 24 KB NVS blob.

pinball-fixed-menu-2026-11-16-2134.bin
   sha256 48dd838b1931ce4d4ca49bccf6e51630d14a65484c278cfae660dfde1d014e31
   old layout, predates both 0.5.2 fixes.
```

**Take a new one after any layout change** — an image in the old layout no
longer matches the partition table on the board, so restoring it is a rollback
rather than a backup.

## THE HARNESS BUG THAT MISLED ME FOR A WHOLE STRETCH

Escape sequences were written **one byte per 100 ms**, and the parser gives up on
a lone `ESC` after a few tens of milliseconds. So every `F10` arrived as
**Escape (which CLOSES the menu) followed by the literal text `[21~`**. That is
why menus kept closing, why one was found scrolled to the bottom, and why I
concluded several times that Enter did not work. **Enter always worked.**
One `os.write()` per sequence, and: F10 → wait 1.5 s → Enter → wait 3 s.

Also: **the display mirror is not a frame counter.** It sent 32 bytes per 2 s
while Pokemon Pinball was visibly running, because an emulator blits straight to
the framebuffer and never touches the text-cell model.

`gb start` (no argument; the ROM default is Zelda DX) failed with
**`gb: no guest task`** once the desktop was up with a Terminal open. The 8 KB
guest stack was internal DRAM, and internal free was 25,395 with the largest
block 9,728 — it does not fit. **Moved to PSRAM** via
`xTaskCreatePinnedToCoreWithCaps`, safe because that task only runs gnuboy: no
file, no partition map, so it never disables the cache a PSRAM stack lives
behind. Deleted with `vTaskDeleteWithCaps()`.

Verified: `running=1 frames=1068 blits=1066 dropped=0 audio=on 43691Hz`, save
restored from `/sd/saves/gbc/...`, operator played it and passed it.

Backup taken in entry 35, below, and verified region by region.

## THE TWO LEAKS THAT KILLED PLAYBACK — fixed, measured flat

The operator's "stopped after 4-5 songs with error: no amplifier" was memory,
not audio. Both leaks were found by counting allocations, and the numbers are
what identified them:

```
decoder working buffer  never freed — nothing called ~FLACDecoder()
                        41 KB PSRAM + 356 B internal PER TRACK
player task              deleted with vTaskDelete() instead of
                        vTaskDeleteWithCaps() — the caps-aware API's PSRAM
                        stack is not freed by the idle task
                        8,196 B PSRAM + 356 B internal PER TRACK
```

- **The module contract has a TEARDOWN now** (`MODLOAD_ABI_VERSION` 3): a call
  with `in == NULL, in_len == 0` destroys what the module built and frees what
  it allocated. `music_player.c` makes that call before freeing the state block.
- **`vTaskDeleteWithCaps(NULL)`**, not `vTaskDelete(NULL)`. `idf_additions.h`
  says so explicitly. `uac2.c` and `usbhost.c` also use `xTaskCreateWithCaps`
  and do not self-delete — safe today, but anyone adding a self-delete there
  must use the caps-aware call.
- **After both: PSRAM returns to 7,786,392 every track, internal delta 0,
  module 0/0.** Measured over eight consecutive tracks.

`MUSIC_UI_SELFTEST` (commented out in `main/CMakeLists.txt`) is the instrument:
it runs a series of tracks and writes PSRAM, internal RAM and the module's live
allocation count after each. `modhost.c` counts those allocations with
`heap_caps_get_allocated_size()`.

### THE THIRD MOVING ADDRESS — now checked

`g_modhost_ptr` moved four times in one afternoon: `0x3c26b8c8`, `0x3c26b99c`,
`0x3c26ba78`, `0x3c26bb50`. A blob holding a stale one does **not** fault; every
call returns `MODCALL_ERROR` and the only symptom is "decode error". The header
now carries `linked_host`, `modload_prepare()` compares it, and
`patch_header.py` reads it from `modaddr.h` at patch time (a parse-time Makefile
read produced `host=0x0` once, which the patcher now refuses).

### "no amplifier" is gone

`sound.c` records the exact failing call and its `esp_err_t`, so the message
now names disable/clock/slot/enable/allocation rather than one catch-all.

## The Music window — built, proven by file, NOT yet seen

`Music` is in the Start menu. A playlist of the SD card's tracks with a
Play/Stop/Prev/Next row, on TinyPhosphor's layout. It is a VIEW onto the one
playback task: `music_player.c` owns the amplifier, the shell's `music` command
and the window both drive it.

**Proven on the device by `MUSIC_UI_SELFTEST`** (commented out in
`main/CMakeLists.txt`; uncomment, flash, and read `/fs/root/musicboot.txt`):

```
window=1  tracks=74  dir=/sd/ROCKBOX/Audio
play=running:1 state:1 frames:253440 rate:44100 secs:5 err:0 short:0
stopped=running:0 state:0
after_next=running:0 sel=1
```

**The operator has now used it**: opened it, played a few songs, heard them.
The window works. What remains unproven is only that playback now survives many
tracks — the leaks are measurably gone, but the failure itself was never
deliberately reproduced, so read it as "cause removed".

**Music is no longer first in the Start menu.** A weak `board_apps_register()`
in `ports/esp_idf/app/main.c` runs after `td_apps_register_all()`, so port apps
register last and sit at the bottom of the menu, next to the user's emulator
`.tdlink` entries.

### THE HOST-TABLE ADDRESS IS A SECOND MOVING TARGET

Entry 30's guard compares the two **mmap bases** only. `g_modhost_ptr` — the
address the blob reads the API table from — moves on nearly every build:

```
0x3c26b8c8 -> 0x3c26b99c -> 0x3c26ba78 -> 0x3c26b9a4
```

A stale one does not fault; it surfaces as **"decode error"** in the status
line. `make modaddr && make flac` after EVERY app build, then flash all three
regions. To check before flashing:

```bash
xtensa-esp32s3-elf-nm ports/twatchultra/build/tinydesk.elf | grep g_modhost_ptr
grep MODHOST_PTR_AT ports/twatchultra/modules/build/modaddr.h
```

They must match. Extending `modload_prepare()` to check this too is owed.

### Driving the desktop: what actually works

- **The Start menu is open at boot**, and `Enter` does open the selected app.
  The screen mirror sends only CHANGED cells, so a frame captured immediately
  after Enter still shows the menu — do not read that as "the key did not land".
  A Terminal that looks absent may be open and focused.
- **A command typed into a just-opened Terminal is lost.** The pause must be
  long enough for the window to take focus, not merely for the frame to paint.
  Roughly: F10, wait ~1.5 s, Enter, wait ~2.5 s, then type.
- After `gb start` the panel shows pixels, so `gb status` written to the
  Terminal is invisible until the emulator stops. Check pixels-vs-text: a few
  hundred idle bytes means a picture.

### The console mirror cannot be trusted — use a file

It sends only CHANGED cells, attaches stale, and repeatedly replayed old
scroll as new output. It twice made `░` wallpaper look like a stuck shadow and
produced a wrong diagnosis. When a display or input question must be answered,
either write the answer to flash (`MUSIC_UI_SELFTEST`, `modload_selftest`) or
have the operator look. Enter and synthetic SGR mouse clicks on the Start menu
did NOT work from the harness; `Esc`, F10 and typed commands did.

## Next steps

1. **`modload` holds one blob.** A second codec needs a registry of blobs by
   name — that is the next structural piece, not another decoder.
2. **Ogg is the obvious companion** to FLAC; `MICRO_FLAC_DISABLE_OGG=1` is
   already set, and `micro-ogg-demuxer` (MIT) is already vendored.
3. **`modcall_t` still does not return the rate.** `modflac` reads the handle's
   `rate` word directly, which works and is documented, but a module that is not
   this one would need the field.
4. **`__assert_func`** routes through the API table and returns rather than
   halting. Reviewed, **never exercised**.
5. **No picker, queue, seek or progress display.** `music` takes a path.

6. **The app image is 3,059,616 B with 1,003,616 B (980 KiB) free** in its
   0x3E0000 slot — entry 43 bought that room. That is FLASH for the image, not
   RAM; both slots are 0x3E0000 so it is the budget either way.
7. **KLIO is a demonstration, not a feature.** Three things stand between it and
   something the operator would use, in the order they matter: **persistence**
   (the firmware is uploaded to RAM every boot, so a trained pattern is gone —
   `read_pattern` -> littlefs -> `write_pattern` fixes it and is the obvious next
   piece), **a UI** (it is a shell command), and **tuning** the four knobs
   `RECOGNITION_SENSITIVITY`, `RECOGNITION_RESPONSIVNESS`, `LEARNING_SPEED`,
   `LEARNING_SIMILARITY_THRESHOLD`.
8. **Finish the sine feeder — this is the live thread.** Bosch's DataInject
   firmware for the BHI260AP is downloaded, in the repo, verified to boot
   (`boot 38`, err `00`, sensors 4 and 6), and its injection protocol is
   documented in `components/imu/README.md`. What is missing is the drain:
   `imu feed` wedges the shell task, and `imu raw` desyncs the FIFO when capped.
   Fix those two instrument bugs and the operator's experiment — train a sine,
   measure `bhy2_klio_similarity_score()` against modulated variants — becomes
   two commands. `klio save` and `klio compare` are already built, never run.
   - **The trade:** DataInject has no KLIO and no gyro. Similarity scoring needs
     the KLIO image; feeding needs DataInject. Needing both in one session is
     the point at which Bosch's toolchain becomes the real answer.
9. **`check_addr_fresh.py` compares image size, not `linked_host`.** That symbol
   moves on nearly every build regardless of size, so a code-only rebuild could
   still ship a stale one — a runtime diagnostic now, but reading the symbol out
   of the fresh ELF is three lines.
10. **The module path has no flash step in the build.** `make modaddr && make
    flac` then flashing 0xC00000 and 0xE00000 is a manual sequence, and a manual
    sequence is what left it stale for five rebuilds. A `make flash-modules`
    would close the other half.
11. **Backups want pruning.** Two images from one hour differ only in the module
    regions and neither name says so; four layouts are still on disk. Keep the
    current layout plus one rollback, and say in one line what each holds.
12. **The IMU window's heading still reads `--`.** `DEVICE_ORI` (69) and
    `DEVICE_ORI_WU` (70) are both subscribed and neither has delivered a value.
    If a nudge does not produce one, question `on_ori()`'s payload layout — it
    assumes 4xint16 quaternion at 2^14 then 3xint16 Euler at 2^8, taken from the
    driver, not from Bosch's documentation.
13. **`update.url` is unset** in `board.example.conf` and the device's
   `board.conf`, so *Check for official updates* reads upstream's feed and would
   offer an image not built for this watch.
14. **`third_party/tdsh` is pinned behind its remote** (0b8ebee -> 53e8681 seen
   on this fetch; the shell itself stays at 0.5.1).
15. **A gamepad reaches the desktop input path like a keyboard now.** Whether
    the emulators exploit that — the NES already takes a BLE Xbox pad through
    `gamepad.c` — is separate work from pairing.
16. **Upstream invitation, from the author on issue 20:** the port is welcome
    upstream as `ports/twatchultra` if a PR is ever wanted.

## Out of scope by instruction

The **USB-C DAC path** stays out of the player until the speaker path was
working — it now is, so this is a candidate to revisit, but only if asked.
`uac2` remains as entry 23 left it.

## The rules, each from a coredump or a link error

- Instruction window `0x42xxxxxx` = fetch only; a data load faults `exccause 3`.
  Data window `0x3Cxxxxxx` = data only; a fetch faults `exccause 2`. They cannot
  serve the same bytes, so `.text` and `.rodata` need two partitions.
- A PSRAM-stack task may not map flash **and may not touch `/fs`** — both
  disable the cache PSRAM lives behind: `assert
  esp_task_stack_is_sane_cache_disabled()`. Map at boot only
  (`modload_prepare()`); other tasks call `modload_ready()`. Diagnostics go to
  `/sd`.
- A blob has no writable `.bss`; the firmware lends `g_modbss` (256 B) and
  `modload_zero_bss()` clears it.
- A `-D` flag does **not** invalidate object files. `gen_addr.py` generates
  `build/modaddr.h`; every module object depends on it.
- Build modules **hosted** (libstdc++ refuses `-ffreestanding`), enforce
  freestanding at link with `-nostdlib`, keep C and C++ flags separate.
- `objcopy -O binary` starts at the lowest address, which is `.rodata` in the
  data window — always use `--only-section`.

## Test audio

`/sd/media/audio/tone440_44100.flac`, `tone1000_48000.flac`, plus `clip10.wav`
and friends. Real tracks (74 FLACs) in `/sd/ROCKBOX/Audio/`.

## Coredump decode

```
esp_coredump info_corefile --core <f> --core-format raw --off 0x18 \
  --gdb /home/vash/esp/idf-tools/tools/xtensa-esp-elf-gdb/14.2_20240403/xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb \
  <matching>.elf
```

Copy the ELF out **before** the next build. A coredump whose SHA doesn't match
the app means it is stale — clear it with `erase_region 0x7E0000 0x10000` and see
whether a fresh one appears.
