# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-24T21:15

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**ENTRY 18 IS CLOSED, WITH THE TEARING UNDERSTOOD AND UNFIXED.** The reported
"stutter" is a **torn frame**, not a stall: a whole-screen update goes out as 18
unaligned 16-row bands and straddles a refresh. The **TE line (GPIO 6) is
configured in `board.conf` and never read by the driver** — that is the fix, and it
is the next cycle. The watch runs entry 17's firmware, unchanged.

**Latest shipped behaviour:** Hold reveals Save/Load (savestates); Pause/Reset are
blank and inert; the volume steps on the third left slot when idle; the terminal
cursor is not painted.

**Still open from before:** the original "rom not found" (entries 11–12) is
closed on the operator's verdict, **not** on a demonstrated cause — the
successful run overwrote the record that would have said which fault it was.

## Standing invariants that cost a cycle each

- **The TEARING is an unaligned push, and TE is unused.** A whole-frame update
  goes out as `BAND_ROWS`-sized transactions (18 of them for a Game Boy frame, 320
  wide) and that span is not aligned to the panel scanout, so a full-screen change
  straddles a refresh. The panel's TE line is **GPIO 6**, in `board.conf`, and
  `LCD_TE` appears **zero times** in the driver. See **LCD-015** before touching it:
  read the polarity/pulse contract first, and measure what a gate costs (up to one
  16.7 ms refresh per frame).
- **While an app owns the pixels, ONLY ITS BLIT TASK MAY PAINT.** `gb_blit_task`,
  `nes_blit_task`, `pm_blit_task`. A touch callback may record state and return.
  `display_margin_label()` paints synchronously on the caller's task and takes
  the SPI bus, so calling it from a touch callback asserts at
  `spi_master.c:1310` (`spi_device_release_bus`) because the blit task is mid
  polling-transaction and `spi_device_acquire_bus()`'s `ESP_ERR_INVALID_STATE` is
  unchecked. This is DSP-005 violated a third time — see **DSP-009**.
- **A dynamic label needs a latch drained in BOTH loop branches.** An idle or
  paused cartridge produces no frames; a running one always has a frame waiting
  and never reaches the idle branch. One branch only = a label that sits stale.
- **A control that SHARES A SLOT with another control branches on state, and the
  slot's hit test is collected unconditionally.** `gb_app_touch()` collects
  `save_pt`/`load_pt` before it knows whether Hold is down, then splits after the
  loop. The third left slot is the volume when idle and Load during a hold.
- **A model of the code is not the code.** The host simulation for entry 17 passed
  while the shipped line was wrong, because it was transcribed from what the code
  was SUPPOSED to do. For a handler whose inputs were rewired, grep each local for
  an assignment besides its declaration — mechanical, not behavioural.
- **A column repaint covers the glass, not just the framebuffer, and it is
  painted LATER than the app's own controls.** So repaint only the slots whose
  words changed: `display_margin_labels(col, texts, last_slot)` stops the window
  at `last_slot`'s bottom row. Repainting the full column and then redrawing the
  controls is correct and *visible* — a clip and a repaint on every press, which
  the operator reported. `_Static_assert`s in `gb.c` pin A, B and the d-pad below
  the word region.
- **The terminal cursor must not be painted.** `display_cursor_draw()` defaults
  off. The desktop shows and homes the cursor leaving a full-screen screen, so a
  cell-based driver would invert cell (0,0) into a solid white block — EXT-004.
  A real text caret is the INVERSION, never the U+2588 glyph.
- **The `0x1B000` failure record sector is LAST-WRITE-WINS.** If the "rom not
  found" symptom returns, read `tools/watch-slotbrec.py` BEFORE playing anything
  that works, or the evidence is gone.
- **Blanks are drawn but not live.** Safe today because `slotb_run()` claims the
  pixels before the picker appears, so `edge_press()` is unreachable. Any new
  owner of the glass must claim it.
- **No `display_fill()` in a redraw path** — it pushes the whole panel and the
  caller paints over it: a visible flash and double the panel time. See entry 13.
- **The picker's bands tile the framebuffer under a `_Static_assert`.** With the
  fill gone, an unclaimed row is a permanent stripe, not a blink.

## What changed, entries 8–18 (all pushed)

Read the entry, not this list, before trusting a claim.

- **Entry 8 (fork `8904b22`)**: the picker's `..  (back)` row is gone; the bar
  right of Enter is **"Up"**.
- **Entry 9 (fork `bbbbb9e`)**: `slotb_app_go(sel, direct)` owns every exit
  decision; `gb_last_error()` names the real reason a start refused.
- **Entry 10 (fork `d1f2bc0`)**: every margin slot drawn; Up at a console root
  returns to the console list with the current console highlighted.
- **Entry 11 (fork `e15e01b`)**: console screen had been borrowing `list_dir()`'s
  label array; fixed, plus the `0x1B000` failure record and its reader.
- **Entry 12 (fork `75e8a00`)**: the reader compared magic as bytes and rejected
  a good record; fixed.
- **Entry 13 (fork `8dff372`)**: the picker no longer paints a black frame before
  every repaint — a move repaints two rows plus the footer.
- **Entry 14 (fork `3a28bb6`, supersedes the crashing `f9fa9a6`)**: Mute/25/50/75/
  Max volume on the third left button of Game Boy, NES and Pokemon Mini; and the
  panic that version caused, fixed.
- **Entry 15 (fork `c1a5975`, supersedes its own `27a7d3d`)**: Game Boy's
  top-right slot is Hold — blanks the other five column words and makes them
  inert while pressed. Two wrong versions first (A clipped, then A visibly
  clipped-and-redrawn); the shipped one stops the repaint above the cluster.
- **Entry 16 (fork `eef4294`)**: the white block was the terminal's block cursor;
  `display_cursor_draw()` gates it off by default. EXT-004 records the contract.
- **Entry 17 (fork `74d9ed0`, supersedes `3e75636` and `4240979`)**: Pause/Reset
  blanked; Hold reveals **Save**/top and **Load**/third as savestates
  (`gnuboy_save_state`/`load_state`, `<rom>.st0`). The first build broke the
  volume button — a local left declared and never assigned — fixed in `74d9ed0`.
- **Entry 18 (no fork change)**: diagnostic-only. The tearing is an unaligned
  18-band push; the TE line is configured and unused. LCD-015 records it. The
  instrumentation built for this was over-done, froze the emulator twice, and is
  **stashed, not deployed**.

## Where things live

- Project control: `/run/media/vash/GIT/Escapement`, `main`, **log at entry 16**.
- Active code: `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`,
  HEAD `74d9ed0`, pushed to `fork`. Push to `fork` (`aquasock/tinydesk`) only —
  **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

```
Fork        /run/media/vash/GIT/tinydesk-twatch  branch t-watch-ultra  HEAD 74d9ed0
Project     /run/media/vash/GIT/Escapement       branch main          log entry 18
Port        /dev/ttyACM0  (also /dev/serial/by-id/usb-Espressif_...-if00)
Build       3,112,944 B  sha256 1311014e9a7f2c8a1481ba48cb1f6605364ab8fd4272874859c929d5ef1eaadc
Module      flac.bin 26,725 B sha256 dfe1815408b1f9290c071032768265615628a8411027c2367aa2763197e74f9b
            flac.data.bin 1,976 B sha256 4eccdad2da2a42f3deb7b79b156cf346f81a034401c1d66b386bf2e036587c11
            host=0x3c2819e4  inst=0x42ae0000  data=0x3cce0000  app=3112944
            (matches the deployed clean build -- checked, not assumed)
Backup      ~/twatch-backup/working-2026-11-24-2115-clean-noinstrument.bin
sha256      4260436980ce49bda313a887d52bd6dcf058fcde4d176feec518d988d043e338
            (whole 16 MiB; both app slots AND the blob verified inside)
SUPERSEDED  working-2026-11-24-1320-volume-button.bin — holds the CRASHING
            f9fa9a6 build (volume painted from the touch task). Do not restore.
            working-2026-11-24-1350-volume-button-fixed.bin,
            working-2026-11-24-1745-hold-volume-cursor.bin and
            working-2026-11-24-1900-saveload-volfix.bin — right for their
            cycles, superseded by entry 18's clean rebuild.
DO NOT USE  working-2026-11-23-2230-upbar-nomenu.bin.STALE-MODULE
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
   covers display and margin rules only. `gbpick.c` and the emulator margin
   wiring run on the board alone.
6. **A panic is diagnosable — read the coredump, do not theorise.** The console
   is compiled out (`CONFIG_ESP_CONSOLE_NONE`), so a crash leaves no serial
   output; the 64 KB coredump partition at `0x7E0000` is the only record, and
   `espcoredump.py` decodes it into the task name, the assert string and the
   backtrace. Entry 14's crash was named this way in one read instead of being
   guessed at. Recipe below. **It is last-write-wins like the slot B record:**
   read it before rebooting into anything else if a fault matters.

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
- **The USB mirror cannot show a panel artifact.** It carries the terminal stream,
  not framebuffer pixels. A defect the operator can see at a cell may be invisible
  to `tools/watch-screen.py` and to every host check — the cursor block of entry 16
  was. Ask what the size, colour, position and permanence are, then look for the
  mechanism that produces exactly those four.
- **A panic reading `assert failed: <fn> <file>:<line>` means a driver contract
  was broken, not a bug in the driver.** Check the task in the coredump's task
  table against which task is allowed to call that function.
- **Before closing a cycle, check the commit CONTAINS what it calls.**
  `git show HEAD:<file> | grep <symbol>` for the declaration as well as the use;
  a commit that calls a function it does not declare will not build from a clean
  checkout, and the tree can look clean while that is true. It happened once here
  (`65301a4`) and needed `--force-with-lease` to repair.

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

1. **THE TEARING — gate the first band of a frame on a TE edge.** Understood, not
   fixed. Read the CO5300's tearing-effect contract for polarity, pulse width and
   whether it follows scanout; then gate and compare the glass. Settle the cost
   question FIRST: a TE wait can add up to 16.7 ms per frame against a guest using
   6.9 ms of a 16.7 ms budget, so it is affordable but not free.
2. **The stall-log experiment is stashed, not deployed** (`stash@{0}` on the fork,
   "stall instrumentation (not deployed)"). Do not restore it as-is: its freeze
   watchdog writes to the card **from the blit task**, and that task owns the panel,
   which is the leading candidate for the two freezes seen while it was running. Any
   future instrument must write from a task that does not own the panel, and must
   not rely on a stop-time dump — a hang prevents stopping.
3. **A savestate gives NO feedback on the glass.** The column words are identical
   before and after, so the only evidence is the 150–250 ms stutter and a wearer
   cannot tell a save from a missed press. A brief acknowledgement in the pressed
   slot is the obvious next cycle, and it should be built on a report.
4. **The other left-column buttons still have no duty.** On Game Boy they are now
   BLANK (blanked in entry 17) — honest but unfinished. The NES still draws
   "Pause" and "Reset" and handles neither, and the Pokemon Mini's "Exit" and
   "Save" buzz and do nothing. Use the Hold button's shape: hit test in the
   callback, act via a latch the blit task drains.
5. **The savestate stall (150–250 ms) is accepted, not solved.** A background
   write or a smaller state would fix it; the button is not the lever.
6. **The picker's start-up repaints each left-column label at full height**, about
   ten full-height pushes before the first frame. Pre-existing, visible only as
   launch latency, and `paint_column_range()` is the tool for it.
7. **The volume is only tested on Game Boy.** The code path is shared; the test
   is not. Whether the NES and Pokemon Mini steps sound as even is unverified.
8. **Startup audio burst**: 99% of drops are two episodes (sec 0 = 7,177;
   sec 14 = 3,915). The recorded theory ("stream opens before ROM load") is
   **wrong**: `sound_stream_open()` is near line 1490, `gnuboy_load_rom()` near
   1427. Likely the I2S ring draining. Instrument: the per-second `gbhist` record.
   **Note:** streams now start Muted, so the burst is inaudible unless the volume
   is raised first — which may itself change what `gbhist` shows.
9. **The desktop's two emulator entries are still `.tdlink` → `.py`** and open a
   Terminal — the same noise entries 5–6 removed from slot B.
10. **Picker rescans the card on every launch** — five taps to a cartridge. A
   remembered selection is the cheapest remaining win, and the `retail` level can
   be collapsed to one tap.
11. **A failed cartridge returns to the desktop, not the picker** — costly for
   someone sampling 1,143 ROMs. Needs a remembered directory plus a retry, and
   should be built on a report.
12. **Tamagotchi is the only emulator not in slot B's menu.**
13. **`lcd.boot_pattern = 1` in `board.conf` re-enables the RGB bring-up bands** —
    implemented, default off, **never exercised**. One line and a reboot to test.
14. **The `0x1A000` trail and the `0x1B000` record are both owed removal** once
    slot B runs a few cycles without an unexplained failure.
15. **Open question, the operator's to decide:** `spi_device_acquire_bus()`'s
    return is unchecked in `paint_column()`, so a second-writer mistake panics
    instead of skipping a repaint. Loud is what made entry 14 a one-read
    diagnosis; checking it trades that for resilience.
16. **`core-syntax.md` still names the wrong target** — "deployed to the Tang"
    and "the active Tang-Phosphor log". The formatting rules are sound, the
    device name is not. Needs the operator's word because it is a `.ai/` file.

**Walked and closed: do not re-open these on the strength of this file.** The exit
path and stage 8 RETURN (entry 12), the console labels, the blank margins,
Up-at-root, the Up bar (entries 10–12), the picker's redraw (entry 13), the
volume button on Game Boy (entry 14), the Hold button (entry 15), the white block
in the framebuffer's corner (entry 16), Save/Load plus the blanked placeholder slots
(entry 17), and the diagnosis that the "stutter" is a torn frame (entry 18).

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

## Reading a panic (the only crash record this build has)

The console is compiled out, so a crash writes to the coredump partition and
nowhere else. This is the exact sequence that named entry 14's fault:

```sh
cd /run/media/vash/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=$IDF_PYTHON_ENV_PATH/bin:$(ls -d /home/vash/esp/idf-tools/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin|head -1):\
/home/vash/esp/idf-tools/tools/xtensa-esp-elf-gdb/14.2_20240403/xtensa-esp-elf-gdb/bin:$PATH

python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default_reset \
       --after no_reset read_flash 0x7E0000 0x10000 /tmp/coredump.bin

# -t raw is required: the partition holds a raw core dump, not the ELF variant.
# The ELF must be the build that is RUNNING, or every symbol is wrong.
python $IDF_PATH/components/espcoredump/espcoredump.py info_corefile \
       -t raw -c /tmp/coredump.bin build/tinydesk.elf > /tmp/cd.txt 2>&1
grep -E "Crashed task|Panic reason|#[0-9]" /tmp/cd.txt | head -20
```

Read the **task table** too — `STACK USED/FREE` per task is how stack headroom
gets measured instead of guessed. The dump is overwritten by the next panic.

## Card

`/run/media/vash/ESCAPEMENT` when in a PC — `media/gbc/retail` (543),
`media/gb/retail` (600), bucket cap 61 in `tools/gbcstage.py`, `media/nes/retail`,
`media/pokemini/retail`, `media/tama/rom.bin`. ROM defaults resolve
**retail → homebrew → flat root** (`GB_ROM_HB`, `NES_ROM_HB`, `PM_ROM_HB`).
