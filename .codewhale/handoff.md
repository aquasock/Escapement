# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-24T11:50

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**THE PICKER'S FLASH IS CLOSED — ON THE OPERATOR'S VERDICT** (entry 13). Every
selector move called `draw_screen()`, which opened with `display_fill(0x0000)` —
a full-panel push of black, ~100 ms — and then painted 23 bands back on top of
those same pixels. ~190 ms per press, the first half of it a black frame. Fixed
by making the black part of the content and repainting three bands per move.
Operator: *"works perfect. pass."*

**Still open from before:** the original "rom not found" (entries 11–12) is
closed on the operator's verdict, **not** on a demonstrated cause — the
successful run overwrote the record that would have said which fault it was.

## Standing invariants that cost a cycle each

- **The `0x1B000` failure record sector is LAST-WRITE-WINS.** If the "rom not
  found" symptom returns, read `tools/watch-slotbrec.py` BEFORE playing anything
  that works, or the evidence is gone.
- **Blanks are drawn but not live.** Safe today because `slotb_run()` claims the
  pixels before the picker appears, so `edge_press()` (the only path that turns a
  margin press into a key) is unreachable. **Any new owner of the glass must
  claim it**, or a blank slot types into a shell slot B does not have.
- **No `display_fill()` in a redraw path.** It pushes the whole panel and then
  the caller paints over it, which is a visible flash and double the panel time.
  Paint the bands; gate the fill on `paint_black()`-style background bands. The
  picker's `gbpick_message()` keeps its fill on purpose — it is a terminal screen.
- **The picker's bands tile the framebuffer under a `_Static_assert`.** With the
  fill gone, an unclaimed row is a permanent stripe, not a blink. Do not widen
  `PK_ROWS` or move `PK_LIST_Y` without re-checking it.

## What changed, entries 8–13 (all pushed)

Read the entry, not this list, before trusting a claim.

- **Entry 8 (fork `8904b22`)**: the picker's `..  (back)` row is gone; the bar
  right of Enter is **"Up"**. Operator confirmed.
- **Entry 9 (fork `bbbbb9e`)**: `slotb_app_go(sel, direct)` owns every exit
  decision, so a desktop shortcut can never draw the app menu. `gb_last_error()`
  names the real reason a start refused.
- **Entry 10 (fork `d1f2bc0`)**: every margin slot is drawn (unused ones blank);
  Up at a console root returns to the console list with the current console
  highlighted, so switching console costs a press.
- **Entry 11 (fork `e15e01b`)**: the console screen had been borrowing
  `list_dir()`'s label array and drew leftover folder names; fixed with dedicated
  storage, plus a guard that snaps `s_cwd` back under `root_base`. Added the
  `0x1B000` failure record and `tools/watch-slotbrec.py`.
- **Entry 12 (fork `75e8a00`)**: the reader had the magic byte order wrong and
  rejected a good record; fixed. Operator passed the whole area.
- **Entry 13 (fork `8dff372`)**: the picker no longer paints a black frame before
  every repaint. A move repaints the two changed rows plus the footer (~11 ms);
  a page change repaints the body. Operator passed it.

## Where things live

- Project control: `/run/media/vash/GIT/Escapement`, `main`, **log at entry 13**.
- Active code: `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`,
  HEAD `8dff372`, pushed to `fork`. Push to `fork` (`aquasock/tinydesk`) only —
  **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

```
Fork        /run/media/vash/GIT/tinydesk-twatch  branch t-watch-ultra  HEAD 8dff372
Project     /run/media/vash/GIT/Escapement       branch main          log entry 13
Port        /dev/ttyACM0  (also /dev/serial/by-id/usb-Espressif_...-if00)
Build       3,108,912 B  sha256 48af5daa106c562af996e364d6c6a357d4c93d6bb47504684802075aab8acc6e
Module      flac.bin unchanged this cycle — NOT reflashed, NOT re-tested
            0xC00000 / 0xE00000 hold the blob from the 3,108,672-byte app
            host=0x3c28193c (g_modhost_ptr) inst=0x42ae0000 data=0x3cce0000
            app=3108912  <-- all three checked against the new ELF, none moved
Backup      ~/twatch-backup/working-2026-11-24-1145-picker-noredraw-flash.bin
sha256      d848982012187ff1eecea51a5b4357c3a12f68d9b111782ded1a8affbb2534d4
            (whole 16 MiB; both app slots verified byte-identical to the build)
DO NOT USE  working-2026-11-23-2230-upbar-nomenu.bin.STALE-MODULE
            (app slots right, codec blob stale — quarantined, not deleted)
Layout      ota_0 0x20000, ota_1 0x400000 (0x3E0000 each), coredump 0x7E0000,
            otadata 0x7F0000, storage 0x800000, modules 0xC00000, moddata 0xE00000
```

## THE TRAPS THIS AREA RE-TAUGHT

1. **The IDF project root is `ports/twatchultra`, not the repo root.** The repo
   root holds a separate host CMake tree (`build/`, Unix Makefiles, ~40 host
   tests). Running `idf.py build` from the root fails on a generator mismatch.
2. **`export.sh` fails on this machine** — it wants `riscv32-esp-elf-gdb`, which
   is not installed, and returns non-zero before exporting anything. Export the
   environment by hand instead; the recipe below works and is what entry 13 used.
3. **A backup taken between flashes is not a backup.** Take it AFTER the last
   region of a cycle is flashed, and verify the app slots inside it rather than
   trusting the command that wrote it.
4. **A size change *usually* moves the module addresses — check, do not assume.**
   The gate is `python3 modules/check_addr_fresh.py ../build/tinydesk.bin
   moddata.at`, and it only compares sizes. The three addresses a blob depends on
   are `inst`, `data` (both in `moddata.at`) and `g_modhost_ptr` (regenerated by
   `gen_addr.py ../build/tinydesk.elf build/modaddr.h`). Entry 13 grew the image
   by 240 bytes and none of the three moved.
5. **A host test passing is not evidence about the glass.** `test_twatch_edges`
   covers display and margin rules only. `gbpick.c` runs on the board alone.

## Instrument traps that have cost real time

- `esptool ... --after hard_reset run` does **not** reset this board out of the
  bootloader. `otatool` (`read_otadata`, `switch_ota_partition`) does.
- Screen captures interleave with keystrokes; `ls` can come back blank or
  garbled. Re-check with a different command shape. Four wrong conclusions came
  from this.
- A rebuilt image is **never** byte-identical (build date embedded) — so compare
  a rebuild against its own read-back, not against last cycle's hash.
- `otadata` enum: NEW 0x0, PENDING_VERIFY 0x1, **VALID 0x2**, INVALID 0x3,
  ABORTED 0x4, UNDEFINED 0xFFFFFFFF.
- A reset inside the 30 s confirm window rolls back to the other slot; slot B's
  exit, and this project's flash procedure, erase otadata for that reason.
- **When the operator says they are testing, send no commands.** The USB mirror
  is dead while a picture owns the panel (DSP-003).

## Slot B stage trail at flash 0x1A000 (magic "SLBS")

1 ENTER, 2 RAILS, 3 DISPLAY, 4 BOARDCFG, 5 SD, 6 EMU_START, 7 EMU_STOP,
8 RETURN, 9 SETBOOT_FAILED, **10 MENU** (out of band — written every time the app
menu is drawn, so a menu found on the glass can be read rather than guessed at).
FAIL codes 90..93 for display, board config, SD and no-ROM.

## Slot B failure record at flash 0x1B000 (magic "SBRC")

```c
uint32 magic; uint32 uptime_ms; char why[48]; char path[256];
```
Written by `slotb_record()` — picker's 5 refusals, the cartridge handed over
(`why="pick"`), and `gb_start()`'s refusals with `gb_last_path()`. Read:

```sh
cd /run/media/vash/GIT/tinydesk-twatch && tools/watch-slotbrec.py
```

Old `SLBS` content there is pre-entry-93 boot mark debris, not a record.

**THE MAGIC IS A uint32, SO ITS BYTES ARE `CRBS`, NOT `SBRC`.** The first reader
compared bytes against `b"SBRC"` and reported "no record" on a good one. Compare
the number. Any new reader of this sector has the same trap.

## Open, in the order I would take them

1. **Startup audio burst**: 99% of drops are two episodes (sec 0 = 7,177;
   sec 14 = 3,915). The recorded theory ("stream opens before ROM load") is
   **wrong**: `sound_stream_open()` is near line 1490, `gnuboy_load_rom()` near
   1427. Likely the I2S ring draining. Instrument: the per-second `gbhist` record.
2. **The desktop's two emulator entries are still `.tdlink` → `.py`** and open a
   Terminal — the same noise entries 5–6 removed from slot B. Loudest remaining
   rough edge on the glass.
3. **Picker rescans the card on every launch** — five taps to a cartridge now that
   a console level sits above the tree. A remembered selection is the cheapest
   remaining win. The `retail` provenance level can also be collapsed to one tap.
4. **A failed cartridge returns to the desktop, not the picker** — right by
   instruction, costly for someone sampling 1,143 ROMs. Needs a remembered
   directory in `gbpick_run()` plus a retry, and should be built on a report.
5. **Tamagotchi is the only emulator not in slot B's menu.**
6. **`lcd.boot_pattern = 1` in `board.conf` re-enables the RGB bring-up bands** —
   implemented, default off, **never exercised**. One line and a reboot to test.
7. **The `0x1A000` trail and the `0x1B000` record are both owed removal** once
   slot B runs a few cycles without an unexplained failure. Neither is read at
   runtime. The record sector is last-write-wins; a ring of eight would keep a
   session's worth of failures.

**Walked and closed: do not re-open these on the strength of this file.** The exit
path and stage 8 RETURN (entry 12), the console labels, the blank margins,
Up-at-root, the Up bar (entries 10–12), and the picker's redraw (entry 13).

## Build / flash

```sh
cd /run/media/vash/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export ESP_ROM_ELF_DIR=/home/vash/esp/idf-tools/tools/esp-rom-elfs/20240305
export PATH=$IDF_PYTHON_ENV_PATH/bin:$(ls -d /home/vash/esp/idf-tools/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin|head -1):$IDF_PATH/tools:$PATH
python $IDF_PATH/tools/idf.py build

# both slots, then read them back and compare, then clear otadata
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default_reset \
       --after no_reset write_flash 0x20000 build/tinydesk.bin 0x400000 build/tinydesk.bin
python -m esptool ... read_flash 0x20000 <size> /tmp/a.bin     # and 0x400000
cmp build/tinydesk.bin /tmp/a.bin && cmp build/tinydesk.bin /tmp/b.bin
python -m esptool ... erase_region 0x7F0000 0x2000
python -m esptool ... --after hard_reset chip_id                # leaves bootloader
```

Module, only when `inst`, `data` or `g_modhost_ptr` has moved:

```sh
cd modules
python3 check_addr_fresh.py ../build/tinydesk.bin moddata.at
python3 gen_addr.py ../build/tinydesk.elf build/modaddr.h   # compare to the old one
python3 read_modaddr.py --port /dev/ttyACM0 -o moddata.at   # needs a booted board
make flac
```

## Card

`/run/media/vash/ESCAPEMENT` when in a PC — `media/gbc/retail` (543),
`media/gb/retail` (600), bucket cap 61 in `tools/gbcstage.py`, `media/nes/retail`,
`media/pokemini/retail`, `media/tama/rom.bin`. ROM defaults resolve
**retail → homebrew → flat root** (`GB_ROM_HB`, `NES_ROM_HB`, `PM_ROM_HB`).
