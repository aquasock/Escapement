# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-23T23:25

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**THE PICKER DRAWS THE WHOLE MARGIN FRAME AND UP LEAVES THE CONSOLE** (entry 10).
Every margin slot is drawn; unused ones are blank and inert. Up at a console's
root returns to the Game Boy / Game Boy Color list, with the console you came from
highlighted, so switching console costs a press.

**Awaiting one walked test** (owed since entry 10): Start menu → Game Boy Color →
enter `retail` → Up (console root) → Up (console list, GBC highlighted) → Down,
Enter (Game Boy) → confirm the margins look complete on every screen.

**Blanks are drawn but not live.** Safe today because `slotb_run()` claims the
pixels before the picker appears, so `edge_press()` (the only path that turns a
margin press into a key) is unreachable. **Any new owner of the glass must claim
it**, or a blank slot will type into a shell slot B does not have.

## What changed this cycle

- **Picker (entry 8, fork `8904b22`)**: the `..  (back)` row is gone; the
  bottom-strip bar right of Enter — Ctrl+Q on the desktop, meaningless in slot B —
  is **"Up"** and climbs one directory. Drawn only where a parent exists; blanked
  and inert at the console root. **Operator confirmed on the glass.**
- **Slot B launch path (entry 9, fork `bbbbb9e`)**: `slotb_app_go(sel, direct)`
  holds every exit decision; a launch with an NVS key runs one app and never
  draws the menu. `slotb_app_t` gained a `why` hook.
- **Modules (fork `09f680e`)**: the app grew to 3,108,096 B, so
  `modules/check_addr_fresh.py` refused the link. `make modaddr` re-read the
  board: mmap bases unchanged (0x42ae0000 / 0x3cce0000), **API table moved
  0x3c2817e0 → 0x3c2818d8**. Blob + moddata reflashed, read back identical.

## Where things live

- Project control: `/run/media/vash/GIT/Escapement`, `main`, **log at entry 10**.
- Active code: `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`,
  HEAD `d1f2bc0`, pushed to `fork`. Push to `fork` (`aquasock/tinydesk`) only —
  **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

```
Fork        /run/media/vash/GIT/tinydesk-twatch  branch t-watch-ultra  HEAD d1f2bc0
Project     /run/media/vash/GIT/Escapement       branch main          log entry 10
Port        /dev/ttyACM0  (also /dev/serial/by-id/usb-Espressif_...-if00)
Build       3,108,192 B  sha256 88431e74feee2ee2b51011c6653e4c420f6b19b83422c17e12f1f659e7cd01f0
Module      flac.bin 26,725 B sha256 23753524bebb76942668b32580cbd8749177f13da481ad67840878991dcb3ba0
            flac.data.bin 1,976 B sha256 4eccdad2da2a42f3deb7b79b156cf346f81a034401c1d66b386bf2e036587c11
            host=0x3c2818d8  inst=0x42ae0000  data=0x3cce0000  app=3108192
Backup      ~/twatch-backup/working-2026-11-23-2315-fullframe-consoleup.bin
sha256      8ca339c50cfc4f2c1c4f3d286098308284b33488860a686484bc8df2692784f2
            (both slots AND both module regions verified byte-identical)
DO NOT USE  working-2026-11-23-2230-upbar-nomenu.bin.STALE-MODULE
            (app slots right, codec blob stale — quarantined, not deleted)
Layout      ota_0 0x20000, ota_1 0x400000 (0x3E0000 each), coredump 0x7E0000,
            otadata 0x7F0000, storage 0x800000, modules 0xC00000, moddata 0xE00000
```

## THE TRAPS THIS CYCLE RE-TAUGHT

1. **The IDF project root is `ports/twatchultra`, not the repo root.** The repo
   root holds a separate host CMake tree (`build/`, Unix Makefiles, ~40 host
   tests). Running `idf.py build` from the root fails on a generator mismatch;
   recovering that tree cost a reconfigure (and it works — `test_twatch_edges`).
2. **A backup taken between flashes is not a backup.** The first 16 MiB read of
   the cycle landed after the app was written and before the codec, so it holds
   an app needing `host=0x3c2818d8` beside a blob linked for `0x3c2817e0`. Take
   the backup AFTER the last region of a cycle is flashed.
3. **The image size changing moves the module API address**, always. The gate is
   `python3 modules/check_addr_fresh.py ../build/tinydesk.bin moddata.at` — run it
   before flashing, and `make modaddr && make flac` when it refuses.

## Instrument traps that have cost real time

- `esptool ... --after hard_reset run` does **not** reset this board out of the
  bootloader. `otatool` (`read_otadata`, `switch_ota_partition`) does.
- Screen captures interleave with keystrokes; `ls` can come back blank or
  garbled. Re-check with a different command shape. Four wrong conclusions came
  from this.
- A rebuilt image is **never** byte-identical (build date embedded).
- `otadata` enum: NEW 0x0, PENDING_VERIFY 0x1, **VALID 0x2**, INVALID 0x3,
  ABORTED 0x4, UNDEFINED 0xFFFFFFFF.
- A reset inside the 30 s confirm window rolls back to the other slot; slot B's
  exit erases otadata for that reason.
- **When the operator says they are testing, send no commands.** The USB mirror
  is dead while a picture owns the panel (DSP-003).

## Slot B stage trail at flash 0x1A000 (magic "SLBS")

1 ENTER, 2 RAILS, 3 DISPLAY, 4 BOARDCFG, 5 SD, 6 EMU_START, 7 EMU_STOP,
8 RETURN, 9 SETBOOT_FAILED, **10 MENU** (out of band — written every time the app
menu is drawn, so a menu found on the glass can be read rather than guessed at).
FAIL codes 90..93 for display, board config, SD and no-ROM.

## Open, in the order I would take them

1. **Walk the exit path** (owed by entry 9).
2. **The desktop's two emulator entries are still `.tdlink` → `.py`** and open a
   Terminal — the same noise entries 5–6 removed from slot B.
3. **Startup audio burst**: 99% of drops are two episodes (sec 0 = 7,177;
   sec 14 = 3,915). The recorded theory ("stream opens before ROM load") is
   **wrong**: `sound_stream_open()` is near line 1490, `gnuboy_load_rom()` near
   1427. Likely the I2S ring draining. Instrument: the per-second `gbhist` record.
4. Picker rescans the card on every launch (4 taps for 1,143 cartridges) and the
   `retail` provenance level is the only populated one.
5. A failed cartridge returns to the desktop, not the picker — right by
   instruction, costly for someone sampling 1,143 ROMs. Needs a remembered
   directory in `gbpick_run()` plus a retry, and should be built on a report.
6. Tamagotchi is the only emulator not in slot B's menu.

## Build / flash

```sh
cd /run/media/vash/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=$(ls -d /home/vash/esp/idf-tools/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin|head -1):$IDF_PYTHON_ENV_PATH/bin:$PATH
python "$IDF_PATH/tools/idf.py" build

esptool.py -p /dev/ttyACM0 -b 921600 --before default_reset --after no_reset \
        write_flash 0x20000 build/tinydesk.bin 0x400000 build/tinydesk.bin
python $IDF_PATH/components/app_update/otatool.py -p /dev/ttyACM0 erase_otadata
```

Module, only when the image SIZE changes (the gate says so):

```sh
cd modules && python3 check_addr_fresh.py ../build/tinydesk.bin moddata.at
make modaddr && make flac
esptool.py -p /dev/ttyACM0 write_flash 0xC00000 build/flac.bin 0xE00000 build/flac.data.bin
```

## Card

`/run/media/vash/ESCAPEMENT` when in a PC — `media/gbc/retail` (543),
`media/gb/retail` (600), bucket cap 61 in `tools/gbcstage.py`, `media/nes/retail`,
`media/pokemini/retail`, `media/tama/rom.bin`. ROM defaults resolve
**retail → homebrew → flat root** (`GB_ROM_HB`, `NES_ROM_HB`, `PM_ROM_HB`).
