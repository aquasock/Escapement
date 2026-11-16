# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-16T12:50

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**FLAC plays on the watch from the SD card out of the built-in speaker, decoded
on the S3 itself — 0 errors, 0 short writes, real time. `.wav` over the USB-C DAC
is still the qualified baseline. The next structural question is where a second
codec would live, because the app image has 27.5 KB left and the first one cost
31.7 KB.**

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
