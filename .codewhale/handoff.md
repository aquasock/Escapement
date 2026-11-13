# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-13T00:55

Orientation, not authority. Verify before acting on it.

## One line

**NES: Zelda's title 19 -> 51 fps.** Blit now runs on core 1 while the emulator
runs on core 0 (`64ccc7d`). frames=11055 blits=11055 skipped=0 over 20 s,
dropped 36 (0.3%). Operator: "no tearing... mostly full speed."

**DSP-005's single-writer rule is now lifted, in three parts — do not remove one.**
`s_tx_app` (own transmit band), `reg_write_bus()`/`set_window_bus()` (window
inside the bus acquisition), and a 3-buffer pool handed through queues. The
callback takes a free buffer BEFORE publishing the finished one; invert that and
a dry pool pushes half-written frames. DSP-008.

**A DISPLAY CHANGE IS NOT VERIFIED UNTIL SOMEONE LOOKS AT THE GLASS.** This cycle
shipped a build that reported 53 fps, 0 skips, correct timings — and was pure
black, because the CS assertion after window programming was consumed by the
rewrite. Every counter said healthy. The operator found it. DSP-007.

**51, not 60:** a frame is now 19,608 us against 15,242 us of interpreter+PPU.
The remaining gap is the emulator, not the display.

Tried and rejected, with numbers in the code (`NES_VIDBUF_STATIC`, default 0):
static internal vidbuf = 45 fps, +65,800 B permanently, breaks SSH with WiFi on.
Saved 577 us, not the predicted milliseconds — the blit is push-bound.

**Backup `working-2026-11-12-2307.bin`**, 16 MiB, sha256 `bdf7215d…`,
`verify OK (digest matched)` with the board held in the stub across read and
compare. App region at `0x10000` is **byte-identical** to `build/tinydesk.bin` at
`352c4c3` (md5 `c50472f3…`), so it is a record of that commit. Supersedes
`working-2026-11-12-2235.bin`.

**A never-connected boot is a fourth heap state, and the best one: 143,275 free**
(49,152 largest), stable. With `autowifi off` nothing calls `esp_wifi_init()`, so
the static pools are never allocated — 41,500 better than "connected then
disconnected" (101,775), and more than the 30,160 Entry 57's buffer halving won.
SOC-005 revised with this.

Do NOT chase the same win by lowering `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`:
`CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y`, so it would move task stacks into
PSRAM and kill any task that runs during a flash write.

**The `tama stop` reboot is FIXED** (`99e42ed`). It was `TMC_EEPROM_BYTES` 512
against the EEPROM's real 1024: every save wrote 512 bytes past a stack buffer,
through `cmd_tama` into tdsh's parser frame, and it died as a `fclose` on a
corrupted FILE* three calls away. `_Static_assert` now ties the constant to
`HT24LC08_SIZE`. Verified: two start/stop cycles, uptime 17 -> 27 -> 34 s.

**GAMES PLAY, WITH A REAL CONTROLLER.** Super Mario Bros and Zelda both ran from
`/sd` and the operator confirmed **A, B, START, LEFT, RIGHT** by playing them —
that closes the face-button question, which had rested on press order.
**18-19 fps**, and the operator says it feels like 19. Controls: D-pad = move,
A/B = face buttons, Menu = START, View = SELECT.

**Two gaps to know before a game night:** nofrendo keeps battery SRAM in RAM
only, so **a Zelda save does not survive a reset**; and at 19 fps the action is
sluggish — the blit is 37% of the frame.

**The NES PLAYS on the watch.** ROM at `/sd/game.nes`, launched with `nes`.
1x letterboxed, pixel-perfect, and the operator played a game with the margin
buttons. **21 fps, not 60**, and the split names the cause:

    8,160 us  6502 interpreter  (49% of the 16,667 us budget)
    6,166 us  blit              (37%)  <- should be ~100 us

The blit is the anomaly: `display_picture_rgb()` copies into the PSRAM `s_fb`
and `flush_rect()` byte-swaps into `s_tx`, so each frame makes four passes over
external memory. **A full-frame path converting straight into `s_tx` is the next
change** — it also serves the Tamagotchi. Optimising the emulator is the WRONG
first move.

**The Tamagotchi is a playable app with seven controls.** Full-screen, 16 Hz,
dirty rows only. Bottom strip = the pet's three buttons. Left column = Pause,
Save, Load, Speed, Sound; right = Hold (the arm), Shot, blank, blank, Reset.
Save/Load/Reset need Hold down. Sound, pause, speed, screenshot and the strip all
verified by the operator. What is left is a Start menu entry, and the bottom
strip's `°` label is now stale. Next lever: launch it as an app, not `tama`.

## Repositories, both clean and pushed

- **Fork** `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD `3032daa`.
  `fork` remote = `aquasock/tinydesk` (push here). `origin` = upstream
  `tinydesk-project/tinydesk` — **never push there**.
- **Escapement** `~/GIT/Escapement`, branch `main`, HEAD `a39b122`. Log entry 56
  is the current cycle; BLE-007 (gamepad), NES-002 (fps split), NES-001 are newest.

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
- **Backup `working-2026-11-12-2059.bin`, 16 MiB, sha256 `e180f582…`,
  `verify OK`**. Post-OTA-test state: BOTH app slots hold valid 0.4.0 images,
  `otadata` seq 3 vs 2 pointing at `ota_0`.
- **A/B firmware update is proven** (no code changed): install to the inactive
  slot → restart → `Running: ota_1 (on trial)` → confirms after 30 s → install
  again → `ota_0`. A truncated image is refused at boot, not booted.
- The built-in update feed (`…/update-desktop-twatchultra.json`) is **HTTP 404**.
  The mechanism is complete and has no feed — that is the real gap.
- Host suite **42/42**, including `test_tama_ui` (206 checks; the interlock fails
  70/59/59 ways when broken) and `test_twatch_edges` (1,567).
- `./scripts/tamacore-host.sh audio` → `one 1000-toggle tone is one sound`.
- `./scripts/tamacore-host.sh check` → `2000 frames, 250 pushed, 0 pixels differ`.
- `./scripts/tamacore-sync.sh check` → `in sync (13 files)`.

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
6. **A waveform is a clock; the envelope is the signal.** `splb32_audio_emits()`
   counts square-wave toggles, thousands a second for one held tone. Gating a
   beep on it plays continuously and stops only when paused. Use
   `tmc_audio_active()` and its rising edge (EMU-020).
7. **Lifting the first finger of a chord promotes the survivor** into the primary
   slot with event `0x16`, low nibble `0x06`. The press test must be an exact
   `b[0] == 0x06` or a stray finger fires at its own position with the interlock
   already gone (TCH-012).
8. **A rebuild is never byte-identical** — the app descriptor embeds the build
   time and ELF hash. Compare the code region, not the image.
9. **Never cast `char[N][M]` to `const char **`.** That crashed the board once:
   element 1 read mid-row and the painter dereferenced garbage.

## Open, in order of cost

1. **Touch to the guest.** Touches on the framebuffer mapped to the three
   buttons, behind the ownership test `touch.c` already makes. The obvious
   Phase 5 and nothing else is blocked on it.
2. **Start menu entry.** `tama` is a shell command; an app that claims the screen
   is a shape the desktop already supports (`display_pixels_claim`).
3. **`tama stop`'s one-second wait** and the `tama start <rom>` argument path are
   written but unexercised.
4. **Untested under load:** whether the guest holds 16 Hz with the painter active.
10. **NES: SRAM persistence.** Smallest change with the largest effect — a Zelda
    file cannot be saved today. The Tamagotchi already writes its 1024-byte
    EEPROM to `/sd` on stop and loads on start; copy that pattern.
11. **NES: make the blit cheap.** A full-frame RGB565 push that converts into
    `s_tx` in flush order and skips `s_fb`. Measurable on the host first — the
    cost is a memcpy and a byte swap, not emulation. Then re-measure the split;
    a 500 us blit may make the emulator half irrelevant.
11. **NES: SELECT is unmapped.** A few games need it to choose a mode. The left
    column has three free slots.
12. **sound.c's beep buffer is now per-call PSRAM** (was 16,000 B of .bss). That
    fix is independent of the NES and got internal free 60,143 → 76,147.
11. **Save and Load need re-testing** — their earlier pass happened while every
    press corrupted 512 bytes around the saved data. The file was right; the
    machine was not.
11. **Add a boot log** (`/sd/boot.txt`, reset reason + monotonic ms per boot) if
    "did it reboot" is ever ambiguous again. A reloaded Terminal cannot answer it.
12. **No Start menu entry.** `tama` is typed into the shell.
    `display_pixels_claim()` is all an app needs to take the screen — this is the
    next real lever.
11. **The strip's `°` is a stale placeholder.** Chosen when the bars were inert;
    they play the pet now. No short word fits 41 px — the operator's decision.
12. **Two right-column slots are blank on purpose.** An unused button that does
    nothing looks broken; empty glass is honest.
13. **`touchdump` should be kept.** It is how every multi-touch fact above was
    obtained. Captures are preserved in `~/twatch-backup/touch-captures/`.
14. **No way to reach the seven controls without launching `tama`.** The columns
    mean desktop keys on the desktop and app actions under an app. A second
    screen-claiming app will force that decision.
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
