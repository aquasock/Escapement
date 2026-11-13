# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-13T02:45

Orientation, not authority. **Verify before acting on it.**

Session ended here: operator signing off for the night. Everything is committed,
pushed, and the radio is **off**.

## One line

**Three emulator platforms assessed in one session; one shipped and improved, two
scoped. NES is parked at 51 fps because the screen is too small. GBC would fill
the screen but run slower. Pokémon Mini has a week of work banked, a much better
performance case, and an unknown that is now one build away.**

## Repositories

- **Fork** `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD **`64ccc7d`** —
  clean, pushed. `fork` = `aquasock/tinydesk` (push here). `origin` = upstream
  `tinydesk-project/tinydesk` — **never push**.
- **Escapement** `~/GIT/Escapement`, branch `main`, HEAD **`fa82052`** — clean,
  pushed. Log entry 65 is current.
- **tinytama** `~/GIT/tinytama` — the PokeMini work lives here, not in Escapement.

## Permissions and limits

- Push only to `aquasock` repos. Never to `tinydesk-project/tinydesk`.
- `.ai/core.md` is RESTRICTED: read it, never edit it.
- **A `core-syntax.md` audit is owed after any `.ai/` change** — one was run at
  handoff: 65 entries, 0 problems, 127 unique record ids.
- `board.conf` is gitignored. Watch password at `~/Desktop/passwd.txt` — **do not
  echo into transcripts or commits**.

## NES — shipped, parked at 51 fps

`nes start /sd/zelda.nes`. Operator played it: **"looks good. no tearing... mostly
full speed."**

Journey this session: **19 → 42 → 51 fps**, dropped 531/s → 36 in 11,055 frames.

- **19 → 42**: streamed the blit straight into the transmit band. It had been
  making three passes over PSRAM to reach a 10 KB internal buffer.
- **42 → 51**: blit on core 1, emulator on core 0, three-buffer pool, two queues.
- Per-frame now: interpreter 15,242 + convert 4,266 + push 4,004 µs.

**Two hard limits, both measured, do not re-litigate:**

- **The push is NOT wasteful** — 3,360 µs transfer against a 3,072 µs arithmetic
  floor at 80 MHz QIO, i.e. **1.10×**. An earlier claim of a "2.6× excess" was
  blit-minus-estimated-conversion, an inference reported as a measurement. It
  pointed at a DMA overlap with nothing to reclaim. **NES-003 revised.**
- **60 fps is not reachable by tuning the blit.** Deleting the entire blit leaves
  15,242 µs = 65.6 fps, so 60 needs concurrency plus the interpreter. See "Open".

**Owed on NES:** battery SRAM persistence (a Zelda save does not survive a reset)
and the skip-path test (`skipped=0` proves the pool never ran dry, not that the
skip works).

## The three lessons this session cost the most

**1. A display change is not verified until someone looks at the glass.**
A build shipped reporting 53 fps, 0 skips, correct timings — **and pure black**.
The CS assertion after window programming was consumed by a rewrite. Every counter
said healthy. The operator found it. The margins stayed painted because their push
sites kept their own CS line — the clue was on the screen and unread. **DSP-007.**

**2. Do not type into the board while the operator is using it.** A heap re-check
was run by sending keystrokes into a terminal in use; their input and ours
interleaved, and a `pattern` test was still holding memory in the anomalous
reading. Ask for the reading or wait. A board in use is not a bench.

**3. A heap number is meaningless without the state it was measured in.**
Three true figures for one board: **102,615** (no SSH), **82,379** (SSH running,
no client), **53,319** (session attached). SSH costs 48,456 of the 65,536 it
demands before starting. Quoting one as another is how a board appears to gain
50 KB in a minute. **SOC-006.**

## Current machine state

- **WiFi is OFF** — `network autowifi off` **and** `wifidisconnect`. Autoconnect
  alone was not enough; disconnect alone re-associated within seconds. To restore:
  `network autowifi on`, then `wificonnect`. **No SSH while it is off.**
- Internal DRAM: **~143,275 free**, largest block 49,152.
- **Backup `working-2026-11-13-0019.bin`**, 16 MiB, sha256 `d208bd93…`,
  `verify OK (digest matched)` with the board held in the stub across read and
  compare. App region byte-identical to the build (md5 `0ccae46a…`). Covers
  `4f646f3` **plus the CS fix**, but **NOT** `d4e2798` (the conversion
  optimization) or `64ccc7d` (core-1 blit) — **a fresh backup is owed.**

## Open, in order of value

1. **Pokémon Mini route B** — build `core/pokemini/` into TinyDesk as a `pmbench`
   command and take the on-target CPU number. Details below.
2. **NES 60 fps** — the interpreter. Every cheaper-blit avenue is exhausted.
3. **NES SRAM persistence** — smallest change, largest effect on whether a file is
   worth starting.
4. **GBC port** (gnuboy) — if the operator wants the glass filled.
5. Tamagotchi Start menu entry; a fresh backup.

## Pokémon Mini — the state of play

**Most of the work exists in `~/GIT/tinytama`, not here:** PokeMini vendored at
`8a64225e` (GPL-3.0, 47 files, one 2-line patch), a desktop reference build the
operator played across **twelve cartridges** including two levels of Pinball
cleared, 114,708 bytes of Xtensa text, three scope documents.

**Two corrections to that old work, both measured this session** (PM-001, PM-002):

- **The requirement is 0.22–0.36 MIPS, not 1 MIPS.** 1 MIPS is the 4 MHz /
  4-cycle *ceiling*. Twelve cartridges measure 3,091–5,053 instructions against a
  13,889-per-frame budget.
- **The display gate is 5.4× cheaper than when it was taken.** That gate used
  64-byte chunks (357 ns/px); our path uses 16,384-pixel transactions
  (66.5 ns/px). 288×192 costs 3,675 µs of a 13,889 µs budget, not 19,741 µs.
  **The stated blocker is now the comfortable part.**

**The one unknown, and the trap around it.**
`for-escapement/pokemini_bench_target` **cannot run on the watch.** It is a
**Linux** ELF (crosstool-NG, uClibc, has `_start`/`__libc_start_main`); the watch
is **ESP-IDF 5.3.1** with no ELF loader and no `chmod`. Attempted tonight: SFTP
392,204 bytes at 18.1 KiB/s, md5 matching, then `command not found`. The binary is
fine; it targets a different OS.

**Two routes:**

- **A** — flash the Linux image and run it there. Needs no code, but replaces
  TinyDesk (where the NES/GBC work lives) and answers a different question.
  Reversible via the 00:19 backup.
- **B — recommended** — vendor `core/pokemini/` into the fork and add a `pmbench`
  shell command. Real work, but it is **step one of the port**, and it measures
  the platform the other emulators target. Sizing is comfortable: 114,708 text
  against 347 KB free in the app partition; 14,984 bss against 143 KB internal.

**Do not scale the host figure by a ratio.** `POKEMINI-CORE.md` says so directly;
an attempt produced 0.3–1.3 MIPS depending on the anchor. Build it and run it.

**Against PM, plainly:** 96×64 is 3:2 *landscape* on a *portrait* panel. At max
integer scale (3×) it is 288×192 = **41.5% of the framebuffer — smaller in area
than the NES the operator just parked as too small**, with 112 rows of letterbox.

## GBC — scoped, gnuboy recommended

`docs/gbc-scoping.md`. Measured over four commercial ROMs, best of 3 (host noise
7–12%): gnuboy 9,308–13,620 fps vs binjgb 5,684–8,848 — **1.54–1.79× faster**, and
it emits `GB_PIXEL_565_BE` (the panel's byte order) so **no pixel conversion**.

**The picture is the reason:** 160×144 at 2× = **320×288, full width, 69% of the
framebuffer**, no side margins. NES reaches 46% and cannot improve — 256 × 1.25
is not an integer.

**The warning:** a GBC CPU is 2.34× the NES clock, so the projection is
**28–45 fps, likely slower than the NES's 51**. Cheaper blit does not pay for
dearer emulator. GBC is for the picture, not the speed.

**Peanut-GB is DMG-only** — the fastest portable GB core is wrong for Color.
Do not start there.

## Build / flash / drive

```sh
cd ~/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=$HOME/esp/esp-idf-5.3.1 IDF_TOOLS_PATH=$HOME/esp/idf-tools
. $IDF_PATH/export.sh >/dev/null 2>&1
idf.py build > /tmp/b.log 2>&1; echo "BUILD=$?"
idf.py -p /dev/ttyACM0 -b 460800 flash
```

**Driving the watch with the radio off — USB terminal only:**

```sh
python3 /tmp/wsend.py 'cmd' 4.0                 # type + Enter, raw reply
python3 /tmp/wsend.py --raw '\x1b[21~\r' 3.0    # F10 -> Terminal
python3 tools/watch-screen.py                   # parse into a 40x26 grid
```

`/tmp/wsend.py` and `/tmp/wrun.py` are **scratch and will be lost** — recreate if
needed. `wrun.py` needs SSH, so it is unavailable while the radio is off.

**Benchmarks:**

```sh
./scripts/nesbench.sh build && ./scripts/nesbench.sh run ROM 600
./scripts/gbcbench.sh fetch && build && build-binjgb && compare DIR
POKEMINI_SRC=/run/media/vash/GIT/tinytama/core/pokemini ./scripts/pmbench.sh build
./scripts/pmbench.sh compare /tmp/pmini 8000
```

## Traps that cost builds

- `CONFIG_ESP_CONSOLE_NONE=y` — `ESP_LOG` reaches nothing on USB.
- The USB mirror goes **stale** once a bitmap owns the panel. Diagnostics to `/sd`.
- `sdkconfig` is generated and gitignored; put changes in `sdkconfig.defaults`
  **and delete `sdkconfig`**.
- A pipeline through `grep`/`tail` reports **that command's** status.
- Compare the **code region**, not the whole image, after a rebuild.
- **SFTP: use `--chunk 4096`.** 8 KiB times out; a timeout leaves a **0-byte
  file**, so check `ls -l` and never trust `EXIT=124`.
- Only `paint_task` and now **the core-1 blit task** may touch the display. The
  single-writer rule is lifted but **only by all three parts together** — DSP-008.
  Invert the blit callback's order (take a free buffer *before* publishing the
  finished one) and a dry pool pushes half-written frames.
- `tdsh` has **no `chmod`, no `exec`, no ELF loader**. It cannot run host binaries.
- QEMU has **no LX7 core**; a hello-world from the same toolchain fails identically
  on every Xtensa core it offers. Xtensa measurements happen on the board or not
  at all.

## Verification left owed

- **A fresh 16 MiB backup** — the last one predates `d4e2798` and `64ccc7d`.
- `./scripts/tamacore-host.sh check --rom /run/media/vash/GIT/Tamagotchi_v3_us_eu.bin`
  → expect `2000 frames, 250 pushed, 0 pixels differ`, RC 0.
- `./scripts/tamacore-sync.sh check` → expect `in sync (13 files)`.
- Fork host suite — 43/43 at last run; `/tmp` may be cleared, rebuild with
  `cmake -B /tmp/tdtest -S . && cmake --build /tmp/tdtest -j8`.
