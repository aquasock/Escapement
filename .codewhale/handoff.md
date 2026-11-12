# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-12T01:10

Linux on a watch was parked. The live work is a **TinyDesk port** at
`~/GIT/tinydesk-twatch`, branch `t-watch-ultra`. This file is orientation;
**verify before acting on it.**

## One line

**The desktop works: 40x26, three painted touch margins that buzz and beep, an
SD card, a battery line in System Monitor, and a backup that verifies.** What is
left is polish and a conversation with TinyDesk's author.

## Two repositories, both clean and pushed

- **Fork** `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD `ed3f88f`.
  `fork` remote = `aquasock/tinydesk` (push here). `origin` = upstream
  `tinydesk-project/tinydesk` — **never push there**.
- **Escapement** `~/GIT/Escapement`, branch `main`, HEAD `19ad9c4`.
  The Linux project, parked. Its last *code* commit is `8645a82`; everything
  after it is `.ai/core-log.md` entries.

## Permissions and limits

- Push only to `aquasock` repos. Never to `tinydesk-project/tinydesk`.
- `.ai/core.md` is RESTRICTED: read it, never edit it without being asked.
- No sub-agents, no goals (`.ai/core.md`).
- A `core-syntax.md` audit is owed after any `.ai/` change.
- `board.conf` is gitignored — the SD/audio wiring exists only on this machine.

## Verified on the glass tonight

- **Margins**: left column `F10 Esc F6 Super+D F11` (rotated, reads top-to-bottom,
  upright from the right-hand edge), bottom strip `Tab Enter Ctrl+Q`, right column
  reserved + four arrows. Dark grey faces, violet 2px border, terminal-green labels.
  Every button buzzes on press (DRV2605) and every bottom/side press fires a key.
- **Haptics**: `DRV2605 lib1`, confirmed felt.
- **Battery**: System Monitor shows `Battery: 100%  4.17 V full`, from the
  AXP2101 gauge. 4170 mV agrees with the recorded 4181 mV.
- **BEL / speaker**: `py -c "print(chr(7))"` in the Terminal beeps. Log shows
  `twatch-sound: beep 16000/16000 ok`.
- **Escape** no longer opens the Start menu on a bare desktop; Enter and F10 do.
- **SD card**: 29 GB and 128 GB FAT32 cards both mount at `/sd`, read and write,
  and appear in Files as `sd/`.

## Verified by tool output, not by eye

- Host suite **38 of 38**, including `test_twatch_edges` (1,536 checks) and
  `test_vterm`'s BEL tests (44 checks).
- Backup `~/twatch-backup/working-2026-11-12-0109.bin`, 16 MiB, sha256
  `721ff576…`, **`verify_flash` digest matched** — the board is held in the
  bootloader across read and compare, so the check is real.

## The hardware facts that cost the most

All in `.ai/core-reference.md`; the ones that matter:

- **The SD card's bus is shared with LoRa (CS 36) and NFC (CS 4).** Every other
  CS must be parked HIGH or it drives MISO and corrupts SD transactions. This
  is what "the second SPI bus never worked" actually was.
- **The touch reset is expander pin 8.** Pin 10 is `EXPANDS_SD_DET` — the card's
  insert switch — and this port used to drive it as an output.
- **Rails**: ALDO1 = SD (`0x92`), ALDO2 = display (`0x93`), BLDO2 = speaker
  (`0x97`), enables are bits 0/1/5 of `0x90`. Haptic rail is XL9555 P06.
- **Touch panel is not a pointer on the desktop**; a press on the framebuffer
  does nothing. A full-screen app gets it back via `td_wm_app_owns_screen()`.

## Method that found the bugs

**Sweep pure functions over their whole input space against a second
implementation.** `tests/test_twatch_edges.c` did that for the margin geometry
and caught three faults. And **render the shapes at 1:1 on the host before
flashing** — `display.c` cannot compile on a host, so this is the only check the
painter gets.

## Open, in order of cost

1. **`board.example.conf` still calls the SD keys unproven.** They now work. That
   note is wrong and a clean checkout reads it as current.
2. **`td_vterm_t.bell` is probably redundant** — `td_sysinfo_t.bell` is the path
   that works. Both exist as a debugging hedge. Revisit rather than defend.
3. **Three API additions for the author conversation**: `td_sysinfo_t.bell()`,
   `td_sysinfo_t.batteries()`, `td_vterm_t.bell`. All minor under `STABILITY.md`.
   `TD_VERSION` was deliberately **not** bumped — that is the author's call.
4. **The top margin** is the last unpainted region, inert as `EDGE_TOP`, meant
   for status icons.
5. **Deferred by the operator**: what the margins should do when an app owns the
   whole screen. Waiting for something to actually go full screen.
6. **Hot-plug is untested both ways.** `sd.cd` is not configured, so removal is
   detected by polling. Pin 10 is the insert switch and now free.
7. **Store installs to SD and the image cache** have never run against a card.
8. **Untested port overrides**: `TD_SIZE_POLL_MS=0`, `TD_ACK_TIMEOUT_MS=0`.
9. **RTC write path** compiles and is called, never exercised by setting a time
   and power-cycling.
10. **Pre-existing YAML defect** in `.ai/core-reference.md`: a code fence after
    the last record stops the sequence parsing as one document. Records can only
    be validated individually.

## Traps that will cost time if forgotten

- **`CONFIG_ESP_CONSOLE_NONE=y`** — `ESP_LOG` reaches **nothing** on USB. Log
  lines exist only in the desktop's 64-line Log Viewer ring. A missing boot line
  is not evidence of a fault. `board_log_rails()` exists because preinit lines
  are always scrolled away before anyone can read them.
- **`sdkconfig` is generated and gitignored.** A config change only there is
  lost on a clean checkout. Put it in `sdkconfig.defaults` and **delete
  `sdkconfig`**.
- **esptool 4.13 cannot chain commands.** `read_flash … verify_flash` is refused;
  it must be two invocations with `--after no_reset` then `--before no_reset`.
- **`screen /dev/ttyACM0` blocks flashing.** Check `fuser /dev/ttyACM0` first.
- **Native USB re-enumerates on reset**, so boot logs are lost. To read the
  desktop, send `Ctrl+L` (full repaint) and decode into a **fresh** screen model.
- **The shell drops the first character** typed into a freshly focused Terminal.
- **`td_vterm_init()` clears the struct** but preserves `reply` and now `bell`.
  Anything else installed there is wiped by `td_terminal_reset()`.

## Reading the desktop from the host

`/tmp/td/grid.py` is an ANSI screen model. The working pattern, used all night:

1. open `/dev/ttyACM0` at 115200
2. write keys (`\x1b[21~` = F10 for the Start menu, `\r` = Enter)
3. send `\x0c` (Ctrl+L) to force a full repaint
4. feed the bytes into a **new** `Screen(40,26)` and print

`/tmp/sd/drive.py` has this wrapped in a `Desk` class. **It lives in `/tmp`** —
worth moving into the repo.

## Build and flash

```sh
cd ~/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=$HOME/esp/esp-idf-5.3.1 IDF_TOOLS_PATH=$HOME/esp/idf-tools
. $IDF_PATH/export.sh && idf.py build && idf.py -p /dev/ttyACM0 flash
```

Host suite: `cmake --build /tmp/tdtest -j8 && cd /tmp/tdtest && ctest`

Backup: `cd ~/GIT/Escapement && ./scripts/watch-backup.sh`
(reads and verifies in one go; ~4 minutes + ~4 for the verify)

## Reference material read this session

- LilyGo's firmware: cloned to `/tmp/lilygolib` — `src/LilyGoWatchUltra.cpp`,
  `src/audio/AudioDevice.cpp`, and **`schematic/T-Watch Ultra V1.0 SCH
  25-07-24.pdf`** (page 5 has the amplifier).
- Their haptic driver: `lewisxhe/SensorLib`, cloned to `/tmp/sensorlib`.
- Pin map: `arduino-esp32/variants/lilygo_twatch_ultra/pins_arduino.h`.

## Log state

`.ai/core-log.md` has **14 entries** (the post-rollover log, opened 2026-11-11).
Entries 1–100 are archived at `archived_logs/core-log-2026-11-11-2223.tar.gz`.
Do not read other archives without the operator's approval.
