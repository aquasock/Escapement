# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-16T11:31

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**`.wav` playback over the USB-C DAC is QUALIFIED and a full backup exists — this
is the project baseline. Audio plays on the first stream after a plug, the volume
is uniform in dB, and the boot loop is gone. Cycle 2's other half (routing) is
not started.**

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
