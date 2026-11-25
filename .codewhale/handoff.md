# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-25T11:45

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**NOTHING IS OUTSTANDING ON THE GLASS.** Entry 31 retired the NES and the
card-side Tamagotchi launcher from the menus and the operator passed it -- *"pass"*.
Log at entry 31; fork `0cc6c8f` pushed; a verified 16 MiB backup of that exact
build exists.

**THE FIRST THING TO FIX IS A FALSEHOOD IN THIS REPOSITORY.** `README.md`'s status
block still lists `emulators    NES, Game Boy / GBC, Pokemon Mini, Tamagotchi V3`.
The NES does not ship. That was reported and deliberately left alone because it is
outside what the operator asked for -- it is a one-line change and it should be
the next commit.

**The NES is backburnered, not deleted.** `nes.c` is untouched and `nes` still
runs from a Terminal in the desktop; `nes status` was checked on the board after
the flash. What went is the OFFER: `s_app_nes` in `link_twatch.c` and the NES row
in `slotb.c`'s `s_apps[]`. To revive it, put those two back and nothing else.
`nes_install_touch_hook()` is still installed at slot B init and still declines
unless the emulator is running, so that side needs no change either.

**Removing a row from `s_apps[]` changes two menus at once, by design.** That
table is what slot B's menu draws AND what `slotb play <key>` resolves through, so
an app there cannot be launchable-but-invisible or visible-but-unlaunchable. Do
not remove a row from one surface only.

**A tdsh quirk that makes a status check lie.** `rm` does not accept `-f`: the flag
is read as a filename, so `rm -f X` prints `rm '-f': No such file or directory`
and returns 1 -- and deletes X anyway. Use plain `rm X` and verify with `cat`.

**Latest shipped behaviour:** all three consoles pick their cartridge from the same
navigator. Game Boy and Pokemon Mini reveal Save/Load savestates on Hold. The
Tamagotchi also reveals Save/Load on Hold — into its own `tama.sav`, not a
savestate — with Save at the top of the left column and Load in the third. Its right
column reads Hold / blank / sound level / rate / Pause. The terminal cursor is not
painted.

**Still open from before:** the original "rom not found" (entries 11–12) is
closed on the operator's verdict, **not** on a demonstrated cause — the
successful run overwrote the record that would have said which fault it was.

**Known stale document:** `ports/twatchultra/README.md` still describes the
Tamagotchi's margins as `Pause, Save, Load, Speed, Sound` / `Hold, Shot, blank,
blank, Reset`. That is two layout cycles old. It is named as the next cycle in
entry 28; do not trust it until it is fixed.

## Standing invariants that cost a cycle each

- **A picker path needs a 256-byte buffer, and a short one fails SILENTLY.** `strncpy`
  caps and NUL-terminates, so an over-long path becomes a well-formed path to the
  wrong file, which reads as "no rom at ..." and gets blamed on the card. `tama.c`
  had 64 until entry 25 because its default path is only 22 characters — the default
  is exactly what hides the bug. `gb.c`, `pm.c` and `slotb.c` all use 256.
- **A touch consumer must not be welded to a shell command.** `tama_register_command()`
  registered Tama's consumer, and slot B never calls it because slot B has no shell —
  so the app would have run perfectly and been unpressable. The split is
  `*_install_touch_hook()` (gb, nes, pm, tama), called together in `slotb_run()`.
- **THE PICKER PAINTS LABEL OVERRIDES, AND `paint_margins()` CONSULTS OVERRIDES
  BEFORE THE APP TABLES.** So every slot `gbpick_run_cfg()` touched keeps the
  picker's word — PgUp, PgDn, the two arrows, Enter and Up — unless the app
  overwrites it. An app that overrides only the slots whose words change will come
  up wearing the picker's furniture. **`display_margins_set(DISPLAY_MARGINS_APP)`
  will NOT fix it either**: the picker already left the driver in APP mode, so the
  call returns on its own `mode == s_margin_mode` guard without painting. The
  per-slot or per-column writes are what repaint. This made the Tamagotchi look
  like it had no controls at all (entry 26).
- **An app that writes both columns in full every paint does not need a separate
  clear** — and clearing first costs an extra full-height paint per column and
  FLASHES the table's words for one frame. `paint_column_state()` overwrites every
  override by construction. The strip is the exception: only `display_margin_btn_label()`
  writes it, so it still has to be cleared explicitly on start and on stop.
- **A hidden word must be an explicit `" "`, never a `NULL`.** A `NULL` clears the
  override and falls back to the shared table — showing exactly the word being
  hidden. Both `gb.c` and `tama.c` depend on this for their Hold-reveal Save/Load.
- **A slot index is named in THREE independent places, and the host test is the
  only thing holding them together.** The label (`left_label_app()` / `right_label_app()`
  in `edges.h`), the action (`s_map` in `tama_ui.c`, or the emulator's own handler)
  and the painter (`paint_column_state()` / `pm_paint_state()`). Moving a button means
  moving all three plus `tests/test_tama_ui.c`'s slot indexes. `left_label_app()` is
  the SHARED app-mode table, not any one app's: Game Boy, Pokemon Mini and the picker
  each overwrite every left slot on entry, which is the only reason a change there
  cannot leak into them. Check that before editing it.
- **The Hold latch is drained ABOVE the pause branch, in both branches.** A paused
  guest produces no frames, and a pause is precisely when a player reaches for Save.
  A busy-only or idle-only drain leaves the held state unpainted for as long as the
  pause lasts (DSP-009, third application).
- **There is ONE cartridge picker, configured per console.** `pk_cfg_t` in
  `gbpick.h` holds what is console-specific (extensions, roots, title);
  `gbpick_run_cfg()` is the navigator. **Do not add a second picker or special-case
  inside it** — the whole point is that paging and touch cannot drift between
  consoles. `gbpick_run()` is a wrapper over `pk_gb`.
- **Pokemon Mini's control geometry is the Game Boy's, deliberately copied, not
  derived.** `PM_DPAD_*` and `PM_PAD_*` are absolute numbers matching `GB_*`;
  `s_pad_pos[2]` holds A and B. It fits because PM's picture is 192 rows against
  GB's 288, NOT because the glass differs. If you change either emulator's layout,
  decide whether the other follows — they are the same on purpose.
- **PM's margin words come from STATE, not a table.** `pm_paint_state()` is the one
  description of both columns in both held states; the old `PM_LEFT`/`PM_RIGHT` static
  tables are gone. Add a state there, not an override beside it.
- **PM's Hold gates C too.** C is a real `MINX_KEY_C` button and it is blanked AND
  inert while Hold is down — a blank button that fires is the phantom-button fault.
  The gate is the same `s_hold` the painter reads, so drawn and live cannot drift.
- **Both savestates have the same shape and the same trap.** Serviced by the GUEST
  task between frames, never the touch callback: a torn savestate is silent, where
  the same mistake with the panel panicked (DSP-009). **PokeMini's core leaves the
  ROM check to the caller** — `CheckSSFile` copies the stored path out and compares
  nothing, and `LoadSSFile` validates nothing — see **EXT-006**. gnuboy's has the
  same gap; see **EXT-005**.
- **PM's label table is NOT the Game Boy's.** PM has a real C button on
  `MINX_KEY_C`; GB has no C. Copying the labels would remove a working control.
- **Whole-picture pushes are TE-gated; NOTHING ELSE IS.** `display_picture_rows()`
  waits for one TE edge before its first band. **Do not add a second gate**, and in
  particular never gate `push_pixels_buf()` — that is per band, so a frame would wait
  up to a refresh eighteen times. The gate fails open at 20 ms by design: a frozen
  watch is worse than a torn frame. **If you see tearing again, read the `te:` line in
  `gb status` before theorising** — `timeout` climbing means the pin or ISR stopped
  delivering. LCD-015 has the contract (rising edge, drain before taking).
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

## What changed, entries 8–27 (all pushed)

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
- **Entry 19 (no fork change)**: read the CO5300 datasheet and schematic. TE has
  three modes; polarity is rising; LCD-015 corrected — the driver already sent
  TEON, and only the host side was missing.
- **Entry 20 (fork `e5fe7aa`)**: TE sync. Rising-edge ISR on GPIO 6, one gate per
  picture in `display_picture_rows()`, drained then taken with a 20 ms fail-open
  bound. Tear gone. `gb status` gained a `te:` line.
- **Entry 21 (fork `0407271`)**: the picker became a config-driven navigator;
  `pk_gb` and `pk_pm` are tables, `slotb_start_pm` gives Pokemon Mini a chooser.
- **Entry 22 (fork `fbaf1f2`)**: Pokemon Mini's d-pad upright and its whole
  control geometry copied from the Game Boy. The 45° transform is deleted, not
  neutralised.
- **Entry 23 (fork `26e62b3`)**: PM's Hold/Save/Load. Five dead words removed,
  held state is a real gate (C included), Save/Load are savestates via the
  core's own calls. EXT-006 records that the core leaves the ROM check to the
  caller.
- **Entry 24 (fork `262e26c`)**: PgUp/PgDn above the arrows; menu icons are a
  motif; names shortened to GBC / NES / PokéMini. The `(B)` suffix went with the
  card-side `.tdlink` that it disambiguated.
- **Entry 25 (fork `672004e`)**: Tamagotchi picker as the fourth config; slot B
  row and desktop entry `●☼ Tama V3`. Fixed tama.c's 64-byte ROM path and split
  `tama_install_touch_hook()` out of its shell-command registration.
- **Entry 26 (fork `9941946`)**: the picker's words off the Tamagotchi's margins,
  and both columns relaid out — right is Hold/blank/Sound/rate/Pause, left is
  Save and Load. Pause and Speed moved; Shot, Reset and Exit lost their slots.
- **Entry 27 (fork `8db2c31`)**: Save and Load revealed by Hold. Built, flashed,
  slots verified, host test green; **user test passed in the session after**, and
  the verdict is recorded in entry 28.
- **Entry 31 (fork `0cc6c8f`)**: the NES and the card-side `Tamagotchi V2-3`
  launcher off both menus. NES code kept, reachable by the `nes` shell command.
  The `.tdlink` was deleted on the device, not in this repository.
- **Entry 30 (fork `7d28941`)**: the Tamagotchi's Sound on the shared volume
  control -- boots muted, steps up, wraps. Deleted its private stepper; added
  `sound_volume_step()` and `sound_volume_muted()` to `sound.c`.
- **Entry 29 (fork `257be10`, supersedes `41949bf`)**: the Tamagotchi's up-time
  counter in the top strip, in guest seconds derived from the emulator's cycle
  count. Load persists it in a sidecar `/sd/saves/tama/tama.age`. Fixed two
  things on the way: the strip never existed in slot B, and a refused panel band
  was reported as painted.
- **Entry 28 (fork `94ac276`)**: the Tamagotchi's Save moved from left slot 1 to
  slot 0 — Save / blank / Load, matching Game Boy and Pokemon Mini. Three tables
  moved together (`left_label_app()`, `tama_ui.c`'s map, `paint_column_state()`).
  Image size unchanged, so `check_addr_fresh.py` passed and no module reflash.

## Where things live

- Project control: `/run/media/vash/GIT/Escapement`, `main`, **log at entry 28**.
- Active code: `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`,
  HEAD `0cc6c8f`, pushed to `fork`. Push to `fork` (`aquasock/tinydesk`) only —
  **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.
- **The flash holds entry 29's build**, and a full backup of it exists:
  `~/twatch-backup/working-2026-11-25-1145-tama-uptime.bin`, 16,777,216 B,
  sha256 `be09e1f877aa3df3e7ce193f2e07ccbef3dfbceb6685e95ad5614d41f1d36f13`,
  verified against the chip with the board held in the bootloader, and both OTA
  slots inside it hash to `f30f3ef7…` -- the entry 29 build. The previous
  backup, `working-2026-11-25-0910-tama-save-top.bin`, holds entry 28.

```
Fork        /run/media/vash/GIT/tinydesk-twatch  branch t-watch-ultra  HEAD 0cc6c8f
Project     /run/media/vash/GIT/Escapement       branch main          log entry 31
Port        /dev/ttyACM0  (also /dev/serial/by-id/usb-Espressif_...-if00)
Build       3,125,120 B  sha256 a14c6d11b57a32977de75caac0848bffa8ac8c5b9f4532f8eaf4f9509cd82081
Module      flac.bin 26,725 B sha256 947fc4eb86d878fc318f1e3b514ac309587744d47df338a384f765f417b58ff6
            flac.data.bin 1,976 B sha256 4eccdad2da2a42f3deb7b79b156cf346f81a034401c1d66b386bf2e036587c11
            host=0x3c281d78  inst=0x42ae0000  data=0x3cce0000  app=3125120
            (host moves with almost every build; re-check, then rebuild)
Backup      ~/twatch-backup/working-2026-11-25-1330-nes-retired.bin
sha256      6d541a5fd4f97f2322d4bdc1536b2ed74c9814d5d805d88012b1c891d222aad6
            Holds ENTRY 31's build (slots hash a14c6d11...), verified against the
            chip. The previous backup holds entry 30.
            (whole 16 MiB; both app slots and flac.bin verified inside)
SUPERSEDED  working-2026-11-24-1320-volume-button.bin — holds the CRASHING
            f9fa9a6 build (volume painted from the touch task). Do not restore.
            working-2026-11-24-1350-volume-button-fixed.bin,
            working-2026-11-24-1745-hold-volume-cursor.bin and
            working-2026-11-24-1900-saveload-volfix.bin,
            working-2026-11-24-2115-clean-noinstrument.bin,
            working-2026-11-24-2135-te-sync.bin,
            working-2026-11-24-2215-pm-picker-ui.bin,
            working-2026-11-24-2250-pm-hold-saveload.bin,
            working-2026-11-25-0030-menu-icons-names.bin and
            working-2026-11-25-0045-tama-picker.bin — right for their cycles,
            superseded by entry 26.
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

1. **VERIFY ENTRY 27 ON THE GLASS — it is the only shipped change with no user
   verdict.** Run the Tamagotchi and check four things, in order of how easily they
   fail silently: Save and Load are ABSENT when the emulator opens and appear while
   Hold is held; they disappear on release rather than sticking; they still appear
   and disappear while the game is PAUSED (the branch the latch was moved above);
   and an unarmed press of the left slots after a release does nothing and is
   silent. If the words STICK, the fault is in the drain or the transition test. If
   they FLASH "Save" on open, something paints before `s_hold` is assigned.
2. **The Tamagotchi has TWO Start menu entries**, and the card-side one is the
   noise: `●☼ Tama V3` is native (entry 25), and `py Tamagotchi V2-3` is a `.tdlink`
   to `/root/Apps/tama.py` that opens a Terminal and echoes its command first. This
   is the same duplicate the `(B)` suffix once marked on Pokemon Mini, which entry 24
   resolved by deleting the `.tdlink` rather than by adding a suffix. Delete
   `/root/Start menu/Tamagotchi V2-3.tdlink` (a shell command on the littlefs, not a
   commit) and update `scripts/apps/README.md`, which still documents it as live.
3. **`/root/Apps/pm.py` is orphaned** — nothing points at it. Left in place
   deliberately; removing it would also mean updating `scripts/apps/README.md`.
4. **The Tamagotchi's Shot, Reset and Exit have no margin slot** since entry 26's
   layout, so they are reachable only from the shell (`tama do shot`, `do reset`);
   Exit is also what Pusher A does. With Save and Load now on the left under Hold,
   the blank at right 1 is the only thing separating the arm from the sound level and
   may be worth using.
5. **The Tamagotchi's checkpoint is a single shared `/sd/saves/tama/tama.sav`**,
   unlike Game Boy's and Pokemon Mini's per-cartridge paths keyed on the ROM name.
   Correct with one ROM; the moment the picker offers a second it becomes the
   cross-cartridge collision of EXT-005/EXT-006.
6. **A short Tama ROM cannot say why it was refused.** `read_rom()` requires exactly
   `TMC_ROM_BYTES` and there is no `why` hook, so a truncated dump shows slot B's
   generic "did not start".
7. **Watch the `te:` line in `gb status` for a while.** The gate aligns the START of
   a picture and does not bound its LENGTH, so a returning tear would mean the band
   run itself has outgrown one refresh — the answer then is fewer, larger transfers,
   not better alignment. A climbing `timeout` count with `edges` static means the pin
   or the ISR has stopped.
8. **Pokemon Mini has no `why` hook**, so a refused start still cannot say its
   reason the way `gb_last_error()` does. That is the one piece of margin
   behaviour that still does not match the Game Boy's.
9. **The cross-cartridge savestate refusal is unexercised** on PM: the comparison
   exists because the core does not make it (EXT-006), not because a wrong state
   was seen loading. Two cartridges and one deliberate mismatch would be the
   first evidence the refusal path itself works.
10. **The stall-log experiment is stashed, not deployed** (`stash@{0}` on the fork,
   "stall instrumentation (not deployed)"). Do not restore it as-is: its freeze
   watchdog writes to the card **from the blit task**, and that task owns the panel,
   which is the leading candidate for the two freezes seen while it was running. Any
   future instrument must write from a task that does not own the panel, and must
   not rely on a stop-time dump — a hang prevents stopping.
11. **A savestate gives NO feedback on the glass.** The column words are identical
   before and after, so the only evidence is the 150–250 ms stutter and a wearer
   cannot tell a save from a missed press. A brief acknowledgement in the pressed
   slot is the obvious next cycle, and it should be built on a report.
12. **The other left-column buttons still have no duty.** On Game Boy they are now
   BLANK (blanked in entry 17) — honest but unfinished. The NES still draws
   "Pause" and "Reset" and handles neither, and the Pokemon Mini's "Exit" and
   "Save" buzz and do nothing. Use the Hold button's shape: hit test in the
   callback, act via a latch the blit task drains.
13. **The savestate stall (150–250 ms) is accepted, not solved.** A background
   write or a smaller state would fix it; the button is not the lever.
14. **The picker's start-up repaints each left-column label at full height**, about
   ten full-height pushes before the first frame. Pre-existing, visible only as
   launch latency, and `paint_column_range()` is the tool for it.
15. **The volume is only tested on Game Boy.** The code path is shared; the test
   is not. Whether the NES and Pokemon Mini steps sound as even is unverified.
16. **Startup audio burst**: 99% of drops are two episodes (sec 0 = 7,177;
   sec 14 = 3,915). The recorded theory ("stream opens before ROM load") is
   **wrong**: `sound_stream_open()` is near line 1490, `gnuboy_load_rom()` near
   1427. Likely the I2S ring draining. Instrument: the per-second `gbhist` record.
   **Note:** streams now start Muted, so the burst is inaudible unless the volume
   is raised first — which may itself change what `gbhist` shows.
17. **The desktop's two emulator entries are still `.tdlink` → `.py`** and open a
   Terminal — the same noise entries 5–6 removed from slot B.
18. **Picker rescans the card on every launch** — five taps to a cartridge. A
   remembered selection is the cheapest remaining win, and the `retail` level can
   be collapsed to one tap.
19. **A failed cartridge returns to the desktop, not the picker** — costly for
   someone sampling 1,143 ROMs. Needs a remembered directory plus a retry, and
   should be built on a report.
20. **The Tamagotchi's save is a single shared `/sd/saves/tama/tama.sav`**, unlike
    Game Boy's and Pokemon Mini's per-cartridge paths keyed on the ROM name. Correct
    with one ROM; the moment the picker offers a second it becomes the cross-cartridge
    collision of EXT-005/EXT-006, so per-ROM naming turns into a correctness
    requirement rather than tidiness.
21. **A short Tama ROM cannot say why it was refused.** `read_rom()` requires exactly
    `TMC_ROM_BYTES`, and there is no `why` hook, so a truncated dump shows slot B's
    generic "did not start" — the same gap Pokemon Mini has.
22. **`lcd.boot_pattern = 1` in `board.conf` re-enables the RGB bring-up bands** —
    implemented, default off, **never exercised**. One line and a reboot to test.
23. **The `0x1A000` trail and the `0x1B000` record are both owed removal** once
    slot B runs a few cycles without an unexplained failure.
24. **Open question, the operator's to decide:** `spi_device_acquire_bus()`'s
    return is unchecked in `paint_column()`, so a second-writer mistake panics
    instead of skipping a repaint. Loud is what made entry 14 a one-read
    diagnosis; checking it trades that for resilience.
25. **`core-syntax.md` still names the wrong target** — "deployed to the Tang"
    and "the active Tang-Phosphor log". The formatting rules are sound, the
    device name is not. Needs the operator's word because it is a `.ai/` file.

**Walked and closed: do not re-open these on the strength of this file.**
The Tamagotchi being the only emulator missing from slot B (entry 25). The exit
path and stage 8 RETURN (entry 12), the console labels, the blank margins,
Up-at-root, the Up bar (entries 10–12), the picker's redraw (entry 13), the
volume button on Game Boy (entry 14), the Hold button (entry 15), the white block
in the framebuffer's corner (entry 16), Save/Load plus the blanked placeholder slots
(entry 17), the "stutter" being a torn frame (entry 18), the TE fix itself
(entry 20), the Pokemon Mini picker (entry 21), its controls (entry 22), and its
Hold/Save/Load (entry 23).

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
