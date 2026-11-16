## One line

**FLAC plays on the built-in speaker (inline decoder, verified). The `modules`
partition runs code and the API table works. A FLAC decoder builds as a module
and is CALLED, but has never been seen to decode a sample — the last blocker was
a stale address, now fixed and NOT yet re-tested.**

## READ THIS BEFORE TOUCHING THE TEST PATH

Most of the last cycle went into a harness, not the code. The traps, in order of
how much they cost:

**1. Ctrl+Q is 0x11 = XON.** With `IXON` set on the tty the line discipline
swallows it as flow control and it never reaches the desktop, so "close the
focused window" silently does nothing. Every serial helper MUST do:

```python
a[0] &= ~(termios.IXON | termios.IXOFF | termios.IXANY)
a[3] &= ~(termios.ECHO | termios.ICANON | termios.ISIG)
```

**2. The USB console goes silent after esptool.** A fresh
`esptool ... chip_id --before default_reset --after hard_reset` restores it.
esptool also leaves the port at its own baud; the console needs 115200.

**3. The desktop restores a focused window at boot** and eats keystrokes meant
for Terminal. F10 is a TOGGLE — pressing it when the Start menu is already open
closes it. **At boot the menu is open with Terminal selected**, so the reliable
sequence is simply: **send Enter, then type the command.**

**4. Parse the screen properly.** The mirror sends `ESC[r;cH` + SGR runs. A
selected row is the text between an SGR containing `48;5;25` and the next ESC.
`/tmp/raw.py` dumps the escape stream — that is the most useful tool here
because it shows the whole screen when a window repaints.

## THE BUG THAT ENDED THE CYCLE, AND ITS FIX

A blob was linked against `0x3c27b8fc` while the firmware's table had moved to
`0x3c27ba74`. The module's `host()` read garbage, every call returned
`MODCALL_ERROR`, and the ONLY symptom was `verdict -1` — no crash, no log.

**Cause: a `-D` flag does not make an object depend on its VALUE.** Rebuilding
the app changed the define; make saw unchanged prerequisites and reused the
stale `.o`.

**Fix: a generated header.** `modules/gen_addr.py` reads `g_modhost_ptr` and
`g_modbss` from the app ELF into `build/modaddr.h`, and every module object
depends on it, so a moved address forces a rebuild. Verified: the linked blob's
entry now loads `l32r a8, ... (3c27ba74)`.

**This is not re-tested on hardware.** That is the single next action.

## Two more rules from coredumps

- **A PSRAM-stack task may not map flash** — `esp_partition_mmap()` disables the
  cache PSRAM lives behind: `assert esp_task_stack_is_sane_cache_disabled()`.
  `modload_prepare()` runs once from the boot task; other tasks only *ask*
  (`modload_ready()`). A retry that can panic is not a retry.
- **A PSRAM-stack task may not touch `/fs` either.** littlefs is in SPI flash,
  so `fopen()` calls `esp_flash_read()`, which disables the same cache. The
  warning is wider than the mapper. `/sd` is fine (its own bus).

## Backups — take one after any layout change, the old ones stop matching

```
modules-2026-11-16-1320.bin   16,777,216 B
   sha256 34a9d33688e18e0940bf2f0ef7642143f8e0b871f3d3a2a040cbc4e86064262b
   THE CURRENT LAYOUT. App at 0x10000 = 7af3e9c2aa9cae89 (verified).
   sha256sum -c clean.

music-2026-11-16-1259.bin     pre-modules layout, kept for the entry 24 build
   sha256 7a308611b75adaad6546c833282b18b7feefb20d9b63a0d33774bc725d6c0a45

baseline-2026-11-16.bin       entry 23 build, PRE-FLAC. Do not restore over
                              the current layout expecting a working board.
```

**A digest read before a rebuild will not match one read after** — the app
descriptor's build time moves, as entry 20 and entry 25 both record. Compare
app regions against the build that was actually flashed.

## The partition table, live on the board

```
nvs       0x9000    24K
phy_init  0xf000     4K
ota_0     0x10000    3M      app
ota_1     0x310000 3008K     app (OTA target, the real size budget)
coredump  0x600000   64K
otadata   0x610000    8K
storage   0x620000 8064K     littlefs at / (NOT /fs)
modules   0xe00000    2M     data, subtype 0x40 — raw, all 0xFF
```

`storage` is mounted at **`/`** (`/etc`, `/home`, `/root`, `/sd`, `/shared`,
`/tmp`), not `/fs`. `/sd` is the SD card.

## The modules partition — what it is for and what is missing

Xtensa executes from flash natively (everything ESP-IDF runs is XIP-mapped), so
a blob at 0xE00000 can be `esp_partition_mmap()`-ed as **instructions** and
called, costing **no internal SRAM** — the pool this board is short of. 2 MB
against 704 KB for all eleven Rockbox codecs.

**Nothing loads from it.** Before porting anything, prove one tiny
position-independent blob can be mapped and called across the Xtensa windowed
ABI. If that fails, give the room back to `storage`; the alternative is trimming
the app image (MicroPython, wolfSSH, wolfSSL dominate it).

## THE LESSON THAT COST A SCARE: a byte census is not a filesystem census

Counting non-0xFF bytes said `storage` held 4.2 MB with 1.1 MB above the new
boundary — "shrinking destroys data". Mounting it with `littlefs-python` showed
**25 files, 11 dirs, 22,244 bytes, 34 blocks**. The rest was stale blocks
awaiting GC.

**To measure a littlefs partition, mount it:**

```bash
/tmp/lfsvenv/bin/python -c "
import littlefs
ctx = littlefs.UserContextFile('/tmp/storage.img')
fs = littlefs.LittleFS(ctx, mount=False, block_size=4096, block_count=N)
fs.mount()   # mount=False is essential: the ctor FORMATS on failure otherwise
"
```

Extract the partition from a backup image at 0x620000. Migration was done by
extraction and rebuild, never truncation.

## What plays, and with what

```
command    music <file.flac>   |   music stop   |   music   (status)
decoder    micro-flac v0.3.0, Apache-2.0, vendored unmodified
           ports/twatchultra/components/micro-flac (+ micro-ogg-demuxer, MIT)
sink       built-in MAX98357A over I2S, via sound_stream_write_stereo_wait()
```

Measured: 352800 frames for an 8 s 44.1 kHz tone, 192000 for a 4 s 48 kHz one,
both exact; a 5:58 track at `0 errors, 0 short writes` with the frame counter
advancing in step with wall-clock time; heap back to its starting figure after
the task ends.

**The DAC is out of scope by the operator's instruction** — the speaker is
proven, `uac2` stays as entry 23 left it. Do not re-add the DAC path to the
player without being asked.

## The decoder decision, and why it is not the obvious one

```
                          Rockbox flac.c      dr_flac       micro-flac
internal SRAM, static     190 KB (7ch x       0             0
                          4608 x 4 + 64 KB)
working buffer            static, worst-case  heap, 41 KB   heap, file-sized,
                                                             PSRAM-first
Xtensa LPC assembly       no                  no            yes
flash                     ~14 KB (RISC-V)      13 KB src     31.7 KB LINKED
licence                   GPL-2.0-or-later    MIT-0/PD      Apache-2.0
```

**The disqualifier is the 190 KB of static internal SRAM**, not the licence —
this board has ~18 KB free with the desktop running.

**A correction worth remembering:** the firmware **already ships wolfSSH and
wolfSSL under GPLv3**, so it is GPLv3 as a whole already. Rockbox's GPL would
have cost nothing legally. Don't repeat my earlier claim that GPL is the reason
to avoid it; memory is the reason.

**How to measure a future codec:** build the real decode path and diff the image
against an empty app. micro-flac's `.a` is 76 KB but only **31.7 KB** survives
link-time GC, because the archive contains an encoder nobody calls. A size read
off the archive is wrong by 2.4x.

## THE BASELINE IS STALE — read this before restoring anything

`~/twatch-backup/baseline-2026-11-16.bin` restores the **entry 23** firmware
(`abaa8528e1b35c5d`, no FLAC player). The board now runs a newer build and **no
backup of it exists yet**. Taking one is owed.

## Memory, as it stands

```
flash     app 0x2e9450 (3,083,344 B)   27.5 KB free in ota_1   <- thin
internal  ~18 KB free during playback; the player's own buffers are PSRAM
PSRAM     7.4 MB free — the resource nobody is spending
```

## The baseline, on disk

- Full 16 MiB image: `~/twatch-backup/baseline-2026-11-16.bin`, 16,777,216
  bytes, sha256 `1ee127d64d06e4c8112edfaa1926b8c4005984e1ca965bf7beccbafd01c46818`,
  `sha256sum -c` OK. **Verified to hold the flashed app**: bytes at 0x10000 for
  0x2e1460 hash to `abaa8528e1b35c5d`.
- Firmware on the board: sha256 `abaa8528e1b35c5d`, size 0x2e1460, app-only at
  0x10000, `Hash of data verified`. 2% of the app partition free.
- Because the backup and the source match, restoring this image restores this
  exact commit.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at the commit
  carrying core-log **entry 23** (23/100 entries).
- Active code: `/run/media/vash/GIT/tinydesk-twatch`, `t-watch-ultra`. Push to
  `fork` (`aquasock/tinydesk`) only — never `origin`.
- **`/dev/ttyACM0` is a TinyTang CDC, not the watch.** The watch is
  `303a:1001`, normally `/dev/ttyACM1`.
- Crashed-build ELFs in `/tmp`: `tinydesk-000f3b3d.elf`, `-fix1`, `-fix4`,
  `-fix6`, `-vol2`, `-lindb`, `-baseline.elf` (current). Copy the ELF out
  **before** the next build or its coredump cannot be decoded.

## The volume control, and why it is linear in dB

The DAC's Feature Unit is held at 0 dB and unmuted; **all attenuation is in the
samples**. Measured proof it had to be that way: the FU accepts and echoes every
value (`10% → -20.00 dB`, read back exactly) while the output does not follow.

Shipped law: **linear in dB over 36 dB**, `dB = -36 * (1 - pct/100)`. 3.60 dB per
10% of travel, 50% at exactly −18.00 dB, unity at 100% with the multiply
skipped. Accepted by ear on a live stream (100→75→50→25→10) with no glitch.

Rejected first attempt, for the reason a future agent will be tempted to retry
it: `gain = pct^1.66096` (the "half-loud at −10 dB" psychoacoustic rule) measured
**exact to 0.02 dB** at the line output and still felt wrong — a power law is
right at one point of travel and inverted either side of it.

VLC, read from source: it **cubes the gain** (`modules/audio_output/volume.h`)
and its dial goes to 125% with unity at 80% of travel, so its top fifth is
overdrive. Its midpoint agrees with linear-in-dB (−18.1 vs −18.0 dB) but its
shape is *more* bottom-heavy than the rejected curve, and its overdrive cannot be
copied because 100% here is already digital full scale. Recorded as **AUD-001**.

## Internal RAM — the other theme

The scarce pool is 512 KB internal SRAM. Recovered this cycle and the last:

- three USB task stacks (usb_lib, uac2 client, uac2 worker) 12 KB → PSRAM
- the uac2 play task 4 KB → PSRAM

**Rule those tasks keep:** a PSRAM stack must never be executing while the flash
cache is disabled, so no such task may call a flash or OTA API. OTA runs on the
shell's internal-stack task for that reason.

## Instrument traps that cost real time

- **The PC capture chain lies.** `Line Boost` was at **+20 dB** and there is an
  AGC; at defaults the codec limited everything above ~70% and all readings
  flattened, producing a false "the top of the curve is wrong". Zero Line Boost
  (`amixer -c 0 sset 'Line Boost' 0`) and set capture to −15 dB
  (`amixer -c 0 sset 'Capture' 3`); the noise floor then reads −91 dBFS.
- **A BLE session held across a watch reset** reports `SENDFAIL: Service
  Discovery has not been performed yet` for every command while the board is
  fine. Tear it down and reopen.
- **One BLE connection per BATCH, not per command.** `/tmp/blectl.py <cmdfile>`
  does this: one command per line, `!` prefix sends an escaped byte string
  (`!\x1b[21~` F10, `!\r` Enter, `!\x11` Ctrl+Q). The watch stops advertising
  while connected, so per-command connects churn the link visibly.
- **Never press Enter on a Start-menu row without reading the highlight**
  (`48;5;25`). A blind Enter opened Counter twice this session.
- **The mirror sends only changed cells**; `ESC[5000~` forces a repaint.

## Next

1. **Cycle 2's other half: routing.** The emulators and `uac2 play` share one
   sink with no selector.
2. **Cycle 3: the emulators' resampler** — gnuboy emits 43,691 Hz and `uac2`
   refuses unlisted rates rather than converting.
3. Optional: S24/S32 alternates if the bottom of the volume travel ever needs
   more resolution than ~11 effective bits.
4. Owed from before: BLE shell input drops multi-byte writes; `bleuart fetch`
   untested; `dac_wake()` is a mitigation, not an understanding.
