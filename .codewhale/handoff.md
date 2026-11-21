# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-20T23:00

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**The GBC emulator runs clean** — Zelda boots, saves restore, audio is clean by
ear — after entry 82 fixed one DMA defect that had been showing up as two
different faults, and entry 83 widened the ring that fix had over-shrunk.
**A learnt gesture opens an app, and the wearer switches detection on or off —
both survive a reboot.** The AI-engine/injection line is **parked by operator
decision** — do not resume it without being asked.

**Detection only runs while the IMU window is open.** That is a stated limit the
operator accepted, not a bug: `src/wm.c` has no lock, so a background task
launching an app would be a data race. See "Always-on detection" below.

## WORKING WITH THE OPERATOR ON THE WATCH — read this first

**When the operator says they are testing, send no commands.** Not status, not
`ls`, not a read "to check". Reading the screen did not cause the freezes, but
running commands did take the board away from them mid-session, and entry 82
records that cost. Boot what they ask for, then stop.

**The USB mirror goes stale once a picture owns the panel** (DSP-003). Two
`gb status` reads returned identical counters and reported `emu=63755us` against
a 6127us baseline — the numbers were meaningless. Do not quote frame rates read
from the mirror while an emulator runs; ask the operator.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `e2c5180`
  (core-log **entry 88**; reference **IMU-011**, **IMU-012**, **AUD-003**).
  Pushed, clean.
- Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `1664a04`, pushed to `fork`, clean. Push to `fork`
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

## The GBC emulator (entries 82-86 — working)

**Margin columns.** Right column, top to bottom: **empty, Select, Start** (entries
85). Left: Pause, Reset, Save — labelled, sending nothing. Only slots 1 and 2 of
the right column are live, and there are **three** places that must agree: the pad
mapping in `pad_from_frame()`, the haptic liveness predicate in `gb_app_touch()`,
and the labels in the margin setup. Moving a slot means moving all three; a label
that lived in two places once is how a swapped chrome index went unnoticed.

`gb start` boots Link's Awakening DX, save restores, audio plays. Verified on
hardware with the operator confirming picture and sound.

**The one thing that can break it, and did twice under two different names:**
the I2S DMA allocation. Four descriptors × 1024 frames stereo = 16,368 bytes
from `MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA`; the driver caps a descriptor at 4092
bytes and allocates each separately. This board had **12,251 free with a 3,968
largest block**, so it could not allocate at all. Now 256 frames a descriptor
(1,024 bytes each). Ring is 21 ms at 48 kHz instead of 85 ms.

**AUDIO RING: A CEILING, NOT A DEPTH (entries 82-86).** `dma_desc_num` and
`dma_frame_num` are a memory budget shared with the **display's SPI bus**, and
three things matter: buffer size (a 2,926 B frame should fit one descriptor),
ring depth (total bytes vs stalls), and queue depth (`desc_num - 1`, the
driver's completion queue).

**The hard limit is the display.** ~24,951 B are free for audio + display
together. `4 x 1023` = 16,368 B leaves 8,583 and **freezes the picture**;
`8 x 320` = 10,240 B leaves 14,711 and is fine. `ring_try` in `sound.c` now
stops at `3 x 900` (10,800 B, 61.8 ms) and lists fallbacks down to `4 x 256`,
because free DMA varies with what else runs. `gb status` prints `ring=` so a
drop rate always says which geometry produced it.

**The display's SPI bus draws on the same pool.** That is why the same defect
appeared as a frozen picture with sound on one run and a moving picture in
silence on another. If either symptom returns, run `mem heap` and read the DMA
line before suspecting anything else. See AUD-003.

Saving: the writer task is on a **PSRAM** stack now, because it writes the SD
card over SDSPI and never touches internal flash. If it ever starts touching
internal flash, it must go back to internal DRAM — that condition is written at
the allocation.

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

## Latest backup

`~/twatch-backup/working-2026-11-20-2241-jtables.bin`, 16,777,216 bytes, sha256
`ccc95be23156a9669ff38483c0d73425257a3b59d9b516be6924f550b62c1ca7`. Verified
against the chip; `ota_0` identical to build `0x2f3770` (3,094,384 B); module
link `host=0x3c28080c` still valid. Supersedes the audio-prio image below.

`~/twatch-backup/working-2026-11-20-2124-audio-prio.bin`, 16,777,216 bytes, sha256
`eaa795bd8b802cd2ffda66709e640e75dc753a383db153ea3713d3487179220a`. Verified
against the chip; `ota_0` identical to build `0x2f3c30`; module relinked to
`host=0x3c280804`. Supersedes `working-2026-11-20-1857-gbc-dma.bin`
(the older entry below is kept for provenance).

`~/twatch-backup/working-2026-11-20-1857-gbc-dma.bin`, 16,777,216 bytes, sha256
`8bb3bc4f615c714a2246b70ecb23ef37f7675cc729bd0acc96810930b20d4ab2`. Verified
against the chip (digest matched) and `ota_0` inside it is byte-identical to the
build. Holds both audio DMA changes.

## The GBC frame budget (entry 87 — measured)

Normal frames 11 ms, transition frames 40-53 ms, budget 16.742 ms. The port
holds 58.9 fps against 59.73; 1.3% of frames miss, all during transitions.

**The split, measured with a temporary profiler (since reverted):**
- **render is CONSTANT 5-10 ms** — `lcd_renderline()` runs 144×/frame always
- **interpreter goes 6 ms idle → 17-32 ms** on a transition

So opcode/interpreter work is the lever, not rendering.

**Landed:** jump tables on `cpu.c` only (`-fjump-tables -ftree-switch-conversion`
in the component's CMakeLists — a build flag, no source edit). 5,008 → 4,649 µs,
**+7%**.

**Rejected, with numbers:**
- IRAM placement — moved the interpreter to real IRAM (0x4037f…) and gained
  nothing (4,649 → 4,602, noise) while costing 12 KB internal RAM. The 32 KB
  i-cache already covered the 14.5 KB interpreter.
- Blit priority above the painter — **hangs the picture**; two blocking writers
  on one SPI bus cannot be ordered by priority. See entry 86.

**VRAM to internal DRAM — TRIED, CRASHES (entry 88).** Two banks of 8 KB,
internal-first. Terminal died twice during `gbcbench` with no error text;
reverting restored it at 4,552 µs immediately. Not separated whether it was the
16 KB leaving a shared pool or a defect in the two-bank rewrite — both were
reverted together. **If anyone retries: keep the rewrite, put both banks in
PSRAM.** If that runs, it is memory pressure and the question is closed.

**THE OPTIMISATION LINE IS CLOSED.** Three locality changes, three answers:

```
jump tables        +7%, free          KEPT
interpreter→IRAM   no gain, 12 KB     reverted
VRAM→internal      crashes            reverted
```

Internal RAM is the binding constraint. The interpreter is at this board's
floor: 1.72x realtime with rendering, 58.9 fps of 59.73.

**Known defect taken by the revert:** the savestate block in `gnuboy.c` passes
a 16 KB buffer with a count of 4 (a 32 KB walk). Nothing calls it, but a future
savestate feature inherits it — fix when next touching the core.

## NEXT CYCLE — DESIGNED, NOT BUILT: Pusher A closes the emulator

**The operator's request:** a single tap of the watch's physical Pusher A closes
the running emulator and returns to the desktop.

**This was researched and planned but NO CODE WAS WRITTEN.** The tree is clean at
`1664a04` and entry 88 is the last entry. Anyone picking this up starts from the
plan below, not from a partial change.

### What already exists

`ports/twatchultra/main/pwrkey.c` already owns Pusher A. It is **not a GPIO** —
it reaches the SoC as the AXP2101's PEK input, reported on PMU IRQ GPIO 7, with
the internal pull-up and write-1-to-clear status discipline that BRD-002 says is
mandatory. On a short press (`IRQ1_PEK_SHORT`, 0x08, at line 158) it does:

```c
s_presses++;
send_key();        // line 90: s_input(KEY_ENTER, 1) — always "\r"
haptic_tap();
```

`pwrkey_init(pwrkey_input_fn)` is the only entry point; `pwrkey.pin = -1`
disables it deliberately and returns true.

### The plan

1. **`pwrkey.h` — add an app-key hook**, modelled on `touch.h`'s
   `touch_set_app_touch()`, which documents the exact reason it is set once at
   init rather than per app: *"a registration that is torn down with the app has
   a window where the driver holds a pointer to something that has freed
   itself."*

   ```c
   /* Called on a short press before the key is injected. True = consumed. */
   void pwrkey_set_app_key(bool (*fn)(void *user), void *user);
   ```

2. **`pwrkey.c` — consult it first**, in the `IRQ1_PEK_SHORT` branch before
   `send_key()`; if consumed, still `haptic_tap()` but do not inject.

3. **`gb.c` — register at init**, alongside `touch_add_app_touch(gb_app_touch)`
   at line 1759:

   ```c
   static bool gb_app_key(void *user) {
       if (!gb_running()) return false;   /* not ours: Enter passes through */
       gb_stop();                          /* saves SRAM, releases the screen */
       return true;
   }
   ```

4. **Scope: GBC only.** NES and Pokemon Mini are two lines each but have their
   own save paths and behaviour the operator has not tested. Extend after GBC is
   confirmed on the glass.

### Two things the next agent must check

**Threading.** The hook runs on `pwrkey_task` (core 0, priority 3) and
`gb_stop()` waits for the guest and blitter to exit — up to ~100 ms. `gb stop`
from the shell makes the same call from a task, so this is not new, but after
entry 86's blit-priority hang it deserves a measurement rather than an
assumption. Measure tap-to-desktop latency and record it.

**A saved game.** `gb_stop()` saves SRAM, so an accidental tap is annoying rather
than destructive. Verify that explicitly — it is the difference between a
feature and a way to lose a session.

### Also open from entry 88

The savestate block in `gnuboy.c` passes a 16 KB buffer with a count of 4 (a
32 KB walk). Nothing calls it, so no user can hit it, but a future savestate
feature inherits it. Fix when the core is next touched.

## Next steps, in order

1. **Pusher A closes the emulator** — planned above, no code written. GBC first.
2. **The startup audio burst.** ~5,100 samples dropped during cartridge load
   before steady state, which is larger than anything left in steady state. The
   shape of the fix is to open the audio stream *after* the ROM loads rather than
   before — a change to the port, not the core. *(The per-second histogram this
   list used to ask for was built in entries 86-87 and lives at
   `/sd/diag/gbhist.txt`; steady-state drops are now 0.014%.)*
3. **Decide on always-on gesture detection** (above) — the difference between a
   demo and a feature, and it needs an upstream change.
4. **`klio save` overflow.** The save is automatic when a learn completes, so
   "learned and kept" can be shown for a pattern read during a
   `meta 12 fifo overflow`. It survived every observation, but one instrumented
   run counting overflow events against successful reads would settle it.
5. **The learn pass has a silent 60 s ceiling** with no countdown.
6. **More than one gesture.** The store and picker already handle a collection
   (25 slots, name + switch each); the window just has to choose a slot.

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
