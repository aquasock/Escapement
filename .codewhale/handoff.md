# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-20T16:05

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**A learnt gesture now opens an app, and the wearer switches detection on or
off — both survive a reboot.** Press Learn, gesture, pick an app, tick `detect`,
gesture again and the app opens. The AI-engine/injection line is **parked by
operator decision** — do not resume it without being asked.

**Detection only runs while the IMU window is open.** That is a stated limit the
operator accepted, not a bug: `src/wm.c` has no lock, so a background task
launching an app would be a data race. See "Always-on detection" below.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `161b1b6`
  (core-log **entry 81**; reference **IMU-011** and **IMU-012**). Pushed, clean.
- Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `badaff7`, pushed to `fork`, clean. Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

## What entry 81 added

- `apps/imu.c` — a **Gesture opens** section: app button, picker over the real
  registry (`td_app_count`/`td_app_get`/`td_app_launch`), and a `detect`
  checkbox. Window is 25 rows with both sections.
- `klio_store.c/h` — **format v2**: name field 24 bytes (v1's 16 was one byte too
  short for "On-screen keyboard"), and **two more flag bits**:
  `ENABLED` (engine state) vs `ARMED` (wearer's switch). V1 files still load and
  are rewritten as v2 on the next save.
- `klio.c` — `klio_arm`/`klio_disarm`/`klio_slot_app`/`klio_slot_armed`/
  `klio_slot_set`/`klio_detect_tick`. The boot restore enables **only armed**
  patterns and explicitly disables the rest.
- `imu.c` port — the six mapping ops on `s_imu_ops`.
- Tests: store suite **713 checks** (+ASan/UBSan), window suite **92 checks**
  walking the real picker.

### THE BUG THAT COST A CYCLE

`klio_store_put()` carries the armed bit over from the record it replaces — right
for the learn path (re-training must not forget the mapping), **wrong for
settings**. The settings write went through `put()`, so arming was overwritten
with the old value before the file was touched, and the checkbox (redrawn from
the store every tick) snapped back. Now split: **`put()` keeps the mapping,
`replace()` writes the record as given**, and `klio_slot_set()` uses `replace()`.

Why the tests missed it: the window test used a **fake port** and never reached
the store; the store test only exercised `put()`. Both gaps are now closed.

## THE TWO RULES THAT SHAPE ANY FURTHER WORK (IMU-011, IMU-012)

1. **`klio learn` sets `learning_reset`, which wipes every restored pattern.**
   A completed learn must save and reload the whole store — `learn_keep_pattern()`
   does exactly that. Do not "simplify" it away.
2. **A learning pass must own the FIFO.** Accel+gyro at 100 Hz, orientation OFF.
   Orientation streams at 50 Hz into the same FIFO and that sharing is what
   overflowed it in entries 73/74. `ops_start` defers to the pass for this reason.

## Proven evidence (reproduce before trusting)

```
learn     press Learn, gesture, bar fills, "learned and kept"
stored    klio stored -> slot 0, 157 bytes, enabled, watching, Editor
reboot    "restored 1 of 1 pattern(s) after boot, 1 watching"
gesture   IMU window open + detect ticked -> the mapped app opens
box       stays checked (this was the bug; see below)
audio     192000 frames, 0 errors, 0 short writes
```

The store holds **one** gesture (slot 0). Learning another overwrites it; the
store and picker already support 25.

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

## Always-on detection (the obvious next feature)

Wanted: a gesture opens an app from the desktop, with the IMU window closed.

**The blocker is real and mechanical.** `src/wm.c` contains **zero** semaphores
and zero critical sections, and `td_event_push()` in `src/event.c` is an
unlocked ring buffer. A background FreeRTOS task calling `td_app_launch` — or any
`td_win_*` function — is a data race on the global window array, not a slow
path. Detection therefore lives in the window tick today, which is the same
context a click arrives from.

Two honest routes, both a change to **upstream** rather than to the port, so
each needs its own decision:

1. A lock around the event queue in `src/event.c`, letting a task post an event
   that the main loop turns into an action. Smallest surface; touches two
   functions.
2. A marshalling hook in the WM that a task can call. Cleaner boundary, bigger
   upstream change.

A background task also costs power permanently: the part streams at 100 Hz for
the watch's whole uptime instead of only while a window is open. That trade
should be made deliberately, not as a side effect.

## Next steps, in order

1. **Decide on always-on detection** (above) — it is the difference between a
   demo and a feature.
2. **`klio save` overflow is now more visible.** The save happens automatically
   when a learn completes, so "learned and kept" can be shown for a pattern read
   during a `meta 12 fifo overflow`. It survived every observation, but one
   instrumented run counting overflow events against successful reads would
   settle whether that is luck.
3. **The pass has a silent 60 s ceiling** with no countdown.
4. **More than one gesture.** The store and picker already handle a collection
   (25 slots, name + switch each); the window just has to learn to choose a slot.

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
