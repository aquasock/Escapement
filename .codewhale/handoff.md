# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-12T17:05

Orientation, not authority. Verify before acting on it.

## One line

**The Tamagotchi is playable on the watch.** Full-screen, 16 Hz, dirty rows only,
and the three bottom bars are its three buttons (31 presses, 0 drops, one
session). The two side columns are still inert and logged `off`; a Start menu
entry does not exist, so it is launched by typing `tama`. Next is the columns,
then the menu entry.

## Repositories, both clean and pushed

- **Fork** `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD `e076670`.
  `fork` remote = `aquasock/tinydesk` (push here). `origin` = upstream
  `tinydesk-project/tinydesk` — **never push there**.
- **Escapement** `~/GIT/Escapement`, branch `main`, HEAD `8f63618`. Log entry 49
  is the current cycle; EMU-019, TCH-011, DSP-006 and EMU-018 are the durable findings.

## Permissions and limits

- Push only to `aquasock` repos. Never to `tinydesk-project/tinydesk`.
- `.ai/core.md` is RESTRICTED: read it, never edit it without being asked.
- No sub-agents, no goals (`.ai/core.md`).
- A `core-syntax.md` audit is owed after any `.ai/` change.
- `board.conf` is gitignored — the SD/audio wiring exists only on this machine.
- Watch password lives at `~/Desktop/passwd.txt`; do not echo it into transcripts
  or commits. `~/.config/escapement/watch-pass` is a **different** credential and
  does not authenticate.

## Verified on the glass this cycle

- **The pet owns the screen.** `tama` clears the framebuffer to black first, so the
  pet is not drawn over the console; the margin buttons stay painted and live.
  Operator confirmed by eye, twice ("looks great. pass").
- **Four runs, no crash:** 549 frames / 69 blits, 358 / 45, 255 / 32, and then
  2,337 / 293 over a 139-second session left alone — `reset=11` afterwards, so a
  long run is as stable as a short one. That is the host harness's 88% frame-skip
  rate arriving on the hardware.
- **`tama stop` returns the desktop whole** and uptime continues unbroken — so the
  earlier "it crashed" reading was the stale mirror, not a reset.
- **SFTP still works** after the internal-RAM fix: ROM transferred 524,288 B at
  26.6 KiB/s, md5 `3bd0c293351ebb2bb221e9c69cc1c83b` both ways.

## Verified by tool output

- Firmware binary `0x28b460`, 15% of the app partition free, **0 warnings**.
- `./scripts/tamacore-host.sh check` → `2000 frames, 250 pushed, 0 pixels differ
  from a full repaint`, RC 0.
- `./scripts/tamacore-sync.sh check` → `in sync (13 files)`, and proven able to
  fail: a one-line edit to a copy gave `DIFFERS segmap.h`, RC 1.
- Fork host suite **41/41**.
- **Backup `working-2026-11-12-1737.bin`, 16 MiB, sha256 `64c58067…`,
  `verify OK`** (digest matched with the board held in the bootloader across read
  and compare). Covers everything through `a17059a`: the app region at `0x10000`
  is byte-identical to the build (md5 `f2a91ffbbf2f090719aaa4c8e1783a70`).
- `tests/test_twatch_edges` **1,567 checks, 0 failures**, including
  `test_margin_labels`; proved able to fail (3 failures on a wrong code point, 6
  on an empty label). Host suite **41/41**.
- The button queue drained correctly from the host before any finger touched the
  glass: three `tama press`/`release` pairs → `presses=3 drops=0`.
- **No backup since `e076670`** — `working-2026-11-12-1737.bin` covers `a17059a`.

## The two things this cycle cost

1. **4,148 bytes of .bss broke SSH.** The emulator's dirty-row tables pushed
   internal RAM under tdsh's 65,536-byte floor and `ssh start` refused. Internal
   DRAM is the board's scarcest resource: it idles near 66 KB free and SSH alone
   wants 65,536, so **do not add static arrays to this firmware without running
   `heap` afterwards.** Anything app-sized goes in a PSRAM heap allocation, and a
   feature that is only sometimes in use gives the memory back (refcount).
2. **The mirror cannot see a guest.** Once a bitmap owns the panel the USB mirror
   stops describing the glass, so a running emulator and a dead board are
   identical from the host. `/sd/tama.txt` is rewritten every 64 frames with
   frames, blits, cycles and `esp_reset_reason()`, and is the only view that
   survives (DSP-003, EMU-018).
3. **Claiming the screen is not owning it.** A blit writes one rectangle; the
   painter's earlier output stays everywhere else and nothing overwrites it once
   ownership suppresses the painter. An app must `display_fill()` after claiming
   (DSP-006). The margin bars need no such handling -- they are painted once at
   init in controller coordinates, outside `s_fb`, so a framebuffer clear cannot
   reach them.
4. **Two ownership predicates existed and the margin gate asked the wrong one.**
   `td_wm_app_owns_screen()` is false for an app that claims pixels without being
   a WM window -- the Tamagotchi -- so the gate was inert for the one app that
   needed it. Gate on `display_pixels_claimed()` (TCH-011). Every desktop-path
   test passes either way; only an app using the port's own API can catch it.
5. **An emulator's input surface is part of its state, not a message to it.**
   `tmc_button()` writes pin state that `tmc_run()` reads, from a different task
   on a different core. Presses go through a queue drained BEFORE each step --
   after the step makes every press a frame late (EMU-019).

## Open, in order of cost

1. **Touch to the guest.** Touches on the framebuffer mapped to the three
   buttons, behind the ownership test `touch.c` already makes. The obvious
   Phase 5 and nothing else is blocked on it.
2. **Start menu entry.** `tama` is a shell command; an app that claims the screen
   is a shape the desktop already supports (`display_pixels_claim`).
3. **`tama stop`'s one-second wait** and the `tama start <rom>` argument path are
   written but unexercised.
4. **Untested under load:** whether the guest holds 16 Hz with the painter active.
6. **The two columns are inert under an app, and they are next.** Five buttons
   a side, declined by the handler and logged `off`. Wiring them means extending
   the same queue with a wider event and deciding what they do.
7. **The strip's ° is now a stale placeholder.** Chosen when the bars were inert;
   they play the pet now. No short word fits 41 px, so this wants a decision.
8. **No Start menu entry.** `tama` is typed into the shell. `display_pixels_claim()`
   is all an app needs to take the screen.
5. **`board.example.conf` still calls the SD keys unproven.** They work. Stale.
6. **`td_vterm_t.bell` is probably redundant** — `td_sysinfo_t.bell` is the path
   that works. Both exist as a debugging hedge. Revisit rather than defend.
7. **Three API additions for the author conversation**: `td_sysinfo_t.bell()`,
   `td_sysinfo_t.batteries()`, `td_vterm_t.bell`. Minor under `STABILITY.md`.
   `TD_VERSION` was deliberately **not** bumped — that is the author's call.
8. **The top margin** is the last unpainted region, inert as `EDGE_TOP`, meant for
   status icons. And what the margins should do when an app owns the screen is
   still deferred by the operator.
9. **Carried over, unchanged:** NFC parked with the re-read storm deliberately
   kept; GPS tracks satellites and sets the clock (UBX unexplored, no fix
   indoors); LoRa needs ALDO3 and XL9555 P05; the BHI260AP at 0x28 answers and is
   undriven; the microphone has no code; hot-plug and Store installs untested.
10. **Pre-existing YAML defect** in `.ai/core-reference.md`: a code fence after
    the last record stops the sequence parsing as one document, so records can
    only be validated individually.

## How to drive the watch

```sh
cd ~/GIT/tinydesk-twatch
python3 tools/watch-screen.py                      # repaint + read the desktop
python3 tools/watch-screen.py $'\x1b[21~\r' 3.0    # F10 then Enter -> Terminal
python3 tools/watch-screen.py $'tama\r' 3.0
python3 tools/watch-screen.py $'tama stop\r' 3.0
./tools/sftp-put.py put FILE /sd/FILE ; get /sd/FILE FILE ; ls /sd
```

`ssh start` needs ≥65,536 bytes of internal RAM free; `heap` says where it stands.
One client at a time — a wedged session needs `ssh restart`. `tama` writes
`/sd/tama.txt` every 64 frames; read it over SFTP when the mirror is stale.

## Traps that will cost time if forgotten

- **`CONFIG_ESP_CONSOLE_NONE=y`** — `ESP_LOG` reaches **nothing** on USB. Lines
  exist only in the desktop's 64-line Log Viewer ring, and that is 40 columns and
  truncates. A missing boot line is not evidence of a fault.
- **`sdkconfig` is generated and gitignored.** A change only there is lost on a
  clean checkout. Put it in `sdkconfig.defaults` and **delete `sdkconfig`**.
- **esptool 4.13 cannot chain commands.** `read_flash … verify_flash` must be two
  invocations with `--after no_reset` then `--before no_reset`.
- **`screen /dev/ttyACM0` blocks flashing.** Check `fuser /dev/ttyACM0` first.
- **Native USB re-enumerates on reset**, so boot logs are lost.
- **The shell drops the first character** typed into a freshly focused Terminal.
- **A pipeline through `grep` reports grep's status**, not the command's.
- **Only paint_task may touch `flush_rect`/`s_tx`/the SPI bus** (DSP-005), and an
  app drawing pixels must `display_pixels_claim(true)` first (DSP-001).

## Method that keeps finding the bugs

- **Sweep pure functions over their whole input space against a second
  implementation.** `tests/test_twatch_edges.c` caught three margin faults.
- **A check that cannot fail is not a check.** Prove every new gate fails on a
  deliberate mutation — the sync script and `check_dirty` were both proved this
  way, and the first `check_dirty` could not fail and was replaced.
- **Render on the host before flashing** where the code allows it; `display.c`
  cannot compile on a host, so on-glass is the only check the painter gets.

## Log state

`.ai/core-log.md` holds the post-rollover log; entries 1–100 are archived at
`archived_logs/core-log-2026-11-11-2223.tar.gz`. Do not read other archives
without the operator's approval.
