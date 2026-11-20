# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-20T12:50

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**The gesture feature is on the watch and works from the glass.** Press Learn,
perform a gesture, watch the bar, get "learned and kept", and it survives a
reboot. The AI-engine/injection line is **parked by operator decision** — do not
resume it without being asked. **Nothing yet acts on a recognised gesture** —
that is the next decision, not the next task.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `bb5e599`
  (core-log **entry 80**, reference **IMU-011** and **IMU-012**). Pushed, clean.
- Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `c42f825`, pushed to `fork`, clean. Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

## What entry 80 built

- `apps/imu.c` — gesture section in the IMU window: Learn/Stop button (one
  button, two states), progress bar, status line. Window is 19 rows with the
  section, 14 without.
- `ports/twatchultra/main/klio.c` — `klio_learn_begin/tick/cancel` and
  `klio_learn_active`. **`klio learn` was refactored onto the same calls** so
  the command and the button cannot drift apart.
- `ports/twatchultra/main/imu.c` — `s_imu_ops` gains `learn_begin`/`learn_cancel`;
  `ops_latest` drives the pass; **`ops_start` returns early while a pass runs**,
  or it would reconfigure accel to 50 Hz and switch orientation back on mid-pass.
- `include/tinydesk/td_sysinfo.h` — five fields on `td_imu_t`, two function
  pointers on `td_imu_ops_t`. All at the END of the structs. `api/tinydesk.api`
  re-recorded with `third_party/tdsh/tools/check_api.py --record`.
- `tests/test_imu_app.c` — 57 checks, mutation-tested 4/4.

## THE TWO RULES THAT SHAPE ANY FURTHER WORK (IMU-011, IMU-012)

1. **`klio learn` sets `learning_reset`, which wipes every restored pattern.**
   A completed learn must save and reload the whole store — `learn_keep_pattern()`
   does exactly that. Do not "simplify" it away.
2. **A learning pass must own the FIFO.** Accel+gyro at 100 Hz, orientation OFF.
   Orientation streams at 50 Hz into the same FIFO and that sharing is what
   overflowed it in entries 73/74. `ops_start` defers to the pass for this reason.

## Proven evidence (reproduce before trusting)

```
window    opened from Start menu, read on the glass, nothing clipped
press     button -> Stop, bar 0, "repeat the same motion"
gesture   bar fills, then "learned and kept"
stored    klio stored -> slot 0, 157 bytes, enabled
reboot    "restored 1 of 1 pattern(s) after boot"    <- no operator action
recognise MATCH, 88 frames, count to 12.9
```

The store holds **one** pattern (slot 0). Learning another overwrites it.

## HOST PORTS — has gone wrong three times

Watch and TinyTang both enumerate as `/dev/ttyACM` and **swap numbers**.

- Watch: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00`
- Tang: `ffff:5454`. Watch is `303a:1001`. Never open a ttyACM by number.
- **`tools/watch-screen.py` only forces a repaint when NO keys are sent.** A
  capture that passes keys returns whatever was already on screen. To read a
  command's output: send the command, then call again with no keys.
- Don't background a capture (`nohup ... &`); it died with empty output.
- A capture killed mid-flight leaves escape residue — send `\x03`, re-read.
- Scrollback does not survive a repaint, so a long `music` diagnostic gets
  truncated and cannot be re-read. Prefer a short check or read it twice.
- `esptool --after no_reset` leaves the board in the bootloader. Finish with
  `--after hard_reset chip_id`.

## OPENING A TERMINAL / LAUNCHING AN APP

Reboot → F10 → Enter → **verify `tdsh` before typing** (TOL-007). To launch IMU
from the Start menu it is the **19th** entry: send `\x1b[B` eighteen times, then
`\r`. Verify by reading the title bar.

## Build environment (does not come from `export.sh`)

```sh
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=$(ls -d /home/vash/esp/idf-tools/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin|head -1):$IDF_PYTHON_ENV_PATH/bin:$PATH
python "$IDF_PATH/tools/idf.py" build
```

A second toolchain sits at `~/.espressif`; using it gives "configured with a
different python env". Use `/home/vash/esp/idf-tools`.

## Module relink — has bitten twice, expect it every time the app grows

```sh
cd ports/twatchultra/modules
python3 read_modaddr.py --port <by-id> -o moddata.at   # ~2 min, reads storage
make flac                                              # needs IDF_PATH
esptool write_flash 0xC00000 build/flac.bin 0xE00000 build/flac.data.bin
```

The mmap pair (`inst=0x42ae0000`, `data=0x3cce0000`) has been stable, but the
**API table moves with every app rebuild**: `0x3c27fb44` → `0x3c280418` →
`0x3c280658`. Check the `host=` field that `make flac` prints. Inst/data
matching is not sufficient. Symptom of a stale blob: `music` prints "playing"
and plays nothing.

## Next steps, in order

1. **Decide what a gesture should DO.** The engine reports "pattern 0, count N"
   and nothing consumes it. Launch an app, control volume, wake the screen — the
   mechanism is cheap because `klio recognise` already shows the stream, and the
   port could expose recognition the way it now exposes learning.
2. **One pattern is not a collection.** Slot 0 is overwritten on every learn.
   The store already supports the part's 25 slots and has a reserved 16-byte
   name field per record, so this is UI work (a chooser, or a slot on the
   button), not a format change.
3. **The pass has a silent 60 s ceiling.** No countdown; a wearer trying past it
   only sees "no pattern -- try again".

## Open / unsolved

- `klio save` still emits `meta 12 fifo overflow` while reading. The button's
  save produced a blob that matched after reboot — one observation, not a
  characterisation of reliability under overflow.
- Injection parked at entry 78 (step-by-step vs normal mode untested).
- Store is one file, not per-user. Older format versions are refused, not migrated.
- Two unidentified firmware header fields at 0x2c and 0x44 gate custom firmware.
- Conformance debt: entries 66–73 use fenced code blocks in `Outcome`; recorded
  in entry 74, deliberately unfixed by operator decision.

## Standing facts

- Similarity metric from real motion: 1.000 / 0.795 / 0.475 / negative.
- App at `0x20000`, slot `0x3E0000`, 3,092,880 bytes, ~971 KB free.
- Internal DRAM is scarce (EMU-018); the pattern store is a PSRAM allocation.
- **A mutation sweep once reverted uncommitted work via `git checkout
  apps/imu.c`.** Back up the working tree before destructive experiments.
