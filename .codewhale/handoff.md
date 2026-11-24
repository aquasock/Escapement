# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-23T21:05

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**THE CARD NOW HOLDS 1,143 CARTRIDGES IN ALPHABETICAL BUCKETS** (entry 2):
543 GBC in `media/gbc/retail`, 600 DMG in `media/gb/retail`, ≤61 per bucket.
Population is scripted: `tools/gbcstage.py --dry-run` then `--go`.

**The picker NAVIGATES** — console → provenance → bucket → cartridge, folders
with a trailing slash, `..` to go up. `PK_MAX_ROM` is 64, so the bucket cap of 61
is what makes every screen fit.

**THE EMULATOR IS FULLY VERIFIED END TO END** (entry 4). The operator confirmed
Pusher A exits from BOTH the cartridge picker and a running game, and ran a `.gb`
cartridge — the first DMG launch on this board, so hardware-type detection
(header byte 0x143) works. Both items owed since entry 1 are now closed.

**Rows carry no console prefix** — "GBC "/"GB  " cost 4 of 38 usable characters
(10.5%) repeating what the navigation already said. The console is in the TITLE
once: `Game Boy Color - retail`, `Game Boy Color - A`.

**Still open, low priority:** the `retail` provenance level costs a tap and is
the only directory with contents (a single-child skip would save a press); and
the picker rescans the card every launch — 4 taps for 1,143 cartridges.

**Card paths moved again**, so the defaults now resolve retail → homebrew → flat:
`gbc/retail`, `nes/retail`, `pokemini/` (was `min/`), `tama/` unchanged.

**SLOT B BOOTS WITH NO COLOUR FLASH** (entry 6). `display_init()` draws RGB
bring-up bands and then blacks the panel ~100 ms later; the desktop hides them
under its first paint, slot B did not. `display_boot_pattern(false)` turns them
off for slot B only — the instrument stays available for the desktop and any
future panel fault (default ON).

**SLOT B APPS ARE NATIVE APPS NOW** (entry 5)**A `.tdlink` pointing at a `.py` CANNOT avoid the Terminal** — `td_script_run()`
→ `td_terminal_run()` is its only open path. That is why the old launchers showed
all that, and why they were deleted (they are also wiped by any full flash, being
on the card's littlefs).

**The desktop's OWN emulator entries (Pokemon Mini, Tamagotchi) ARE STILL SCRIPTS**
and still open a Terminal. Left alone deliberately; same treatment applies if the
noise bothers.

## WORKING WITH THE OPERATOR ON THE WATCH — read this first

**When the operator says they are testing, send no commands.** Not status, not
`ls`, not a read "to check". Boot what they ask for, then stop.

**NEVER reset the board mid-cycle with an instrument.** A `otatool read_otadata`
run three seconds after `switch_ota_partition` restarted slot B mid-sequence.

**`esptool ... --after no_reset` leaves the board in the bootloader (TOL-006).**
Follow every marker read with:
`esptool ... --before default_reset --after hard_reset run`.

**THE USB MIRROR IS DEAD WHILE A PICTURE OWNS THE PANEL (DSP-003).** A blocking
host read during a picture hangs for ever.

**Do not pipe a long run through `tail`.** It buffers to exit.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, **log at entry 6**
  (previous 99 entries archived — see below)
  (reference **BRD-006** in entry 92; **TOL-009** in entry 91).
- Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `b2d5de0`, pushed to `fork`. Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

## SLOT B — how it works (entries 91–92)

One image in **both** `ota_0` (0x20000) and `ota_1` (0x400000). The slot the
bootloader picked decides the personality.

- **Seam:** `board_startup_override(const char *board_conf)` in
  `ports/esp_idf/app/main.c`, **weak**, called after `nvs_flash_init()` and
  before netif/shell/desktop. Returns true → `app_main` stops there.
- **Implementation:** `ports/twatchultra/main/slotb.c`, strong, returns false on
  `ota_0`.
- **Entry:** `ota rollback` in a shell, or
  `otatool switch_ota_partition --name ota_1`.
- **Exit:** Pusher A → flag → boot task calls `gb_stop()` (SRAM saved) →
  **erase otadata** → restart. Erase, not `set_boot_partition`: see below.
- **Entry:** Start menu "Game Boy Color" → `slotb start`; or `slotb status`.
- **Picker:** `gbpick.c` scans `/sd/media/gbc` (2 deep), then `/sd` (2 deep);
  right column ↓/↑ (index 4/3), bottom strip Enter (index 1). Other ten margin
  buttons are blanked. Picker is also the failure report (text on the glass).
- **Emulator:** the port's own `gb.c`, **not a copy**. `gb_install_touch_hook()`
  and `gb_install_key_hook()` let it run without its shell command.

### ORDER THAT MATTERS in `slotb_run()`

`board_power_rails_preinit()` → `display_init(NULL)` → `display_pixels_claim(true)`
→ **`slotb_peripherals()`** → board.conf → SD mount → touch hook → `pwrkey_init`
→ `gb_start(NULL)` → `slotb_confirm()` → wait for flag → `gb_stop()` → return.

### THE TWO TRAPS THAT COST THIS WORK — read before changing anything

1. **A weak definition in an already-included object satisfies every reference**,
   so the linker never fetches the strong one from `libmain.a`. The module is
   absent from the image and the firmware still builds, flashes and boots.
   `nm <elf> | grep board_startup_override` must show **`T`**, not `W`.
   Fixed by `-Wl,--undefined=slotb_force_link`. See **TOL-009**.
2. **A touch handler must fire on the press EDGE, not per report.** The CST9217
   re-reports a held finger every ~66 ms (TCH records the interval; HAP-001 uses
   it for the click). Acting per report moved the picker 5–8 rows per tap, and
   would have made one tap select *and* commit a ROM. Track prev-state; a release
   frame (n == 0) clears it.
3. **Registering a hook does not start a driver.** `touch_add_app_touch()` and
   `pwrkey_set_app_key()` install into a poll loop that `touch_init()` /
   `pwrkey_init()` create. Haptics need BOTH the rail (`board_haptic_power_on()`)
   AND `haptic_init()` (which writes the library and wave sequence). Three dead
   peripherals came from this one mistake.

### Board fact behind the "panel worked by luck" (BRD-006)

The **XL9555's CONFIG and OUTPUT registers survive an ESP32-S3 reset.** After a
warm reboot the panel and motor rails are still driven; on a **cold boot every
expander pin is an input**, so nothing is powered. Never infer a rail from what
the glass shows — drive the pins explicitly.

## Diagnostics in flash padding

```
0x1A000  slot B stage trail   (only when slot B runs)
0x1B000  RETIRED — was a per-boot write; removed in entry 93. Stale bytes may remain.
```

The desktop's per-boot mark is **gone**. The 0x1A000 trail stays as insurance
against a hang (it distinguishes "stopped here" from "never started"); remove it
once several sessions pass without one.

Stages: 1 ENTER, 2 RAILS, 3 DISPLAY, 4 BOARDCFG, 5 SD, 6 EMU STARTED, 7 EMU
STOPPED, 8 RETURN, 9 SETBOOT FAILED; 90 DISPLAY, 91 BOARDCFG, 92 SD, 93 NO ROM.
200+subtype = boot slot (0x10 ota_0, 0x11 ota_1).

```sh
esptool -p <by-id> -b 921600 --before default_reset --after no_reset \
        read_flash 0x1A000 0x2000 /tmp/mark.bin
# then: ... --before default_reset --after hard_reset run
```

**Delete `slotb_boot_mark()` first** — it erases a flash sector on every desktop
boot.

## Build, flash, relink

```sh
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=$(ls -d /home/vash/esp/idf-tools/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin|head -1):$IDF_PYTHON_ENV_PATH/bin:$PATH
python "$IDF_PATH/tools/idf.py" build
```

Port: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00`
(watch is `303a:1001`; the TinyTang also enumerates as ttyACM — never open by number).

**Flash BOTH slots after every build** (slot B is what makes the feature work):
```sh
esptool -p <by-id> -b 921600 --before default_reset --after hard_reset \
        write_flash 0x20000 build/tinydesk.bin 0x400000 build/tinydesk.bin
```
Add `0x7F0000 build/ota_data_initial.bin` to force the boot back to `ota_0`.

**ESP-IDF EMBEDS THE BUILD DATE AND TIME**, so *every* rebuild changes the image
even for a comment-only edit. A rebuilt binary is never byte-identical — reflash
if you want the backup and the board to match the source.

**Relink the module whenever the image SIZE changes:**
```sh
xtensa-esp32s3-elf-nm build/tinydesk.elf | grep -E ' g_modhost_ptr| g_modbss$'
grep -E 'MODHOST_PTR_AT|MODBSS_AT' modules/build/modaddr.h
# if they differ:
cd ports/twatchultra/modules
python3 read_modaddr.py --port <by-id> -o moddata.at   # ~4 min
make flac                                              # prints host=0x...
esptool write_flash 0xC00000 build/flac.bin 0xE00000 build/flac.data.bin
```

**NEVER `rm sdkconfig`** — see TOL-008. Copy aside, compare after build.

**`esptool ... --before default_reset --after hard_reset run` DOES NOT RESET THIS
BOARD OUT OF THE BOOTLOADER.** It exits 0 and the board stays in the stub, so
anything you then "measure" was measured with nothing running. This invalidated
most of one session's tests. **Use `otatool` for switching and resetting** — it
resets reliably; `esptool` reads with `--after no_reset` leave the board parked.

## Current board state

- Runs entry-6 build (3,107,376 B), desktop on `ota_0`; both slots hold the same
  image. otadata holds a bootloader-written pending entry for `ota_0` (expected).
- Module `host=0x3c2817cc`, MMU `0x42ae0000`/`0x3cce0000`.
- Cartridge chosen per boot from the picker (no remembered selection yet).

## Backup

`~/twatch-backup/working-2026-11-23-2100-nobands.bin` — 16 MiB, verified
against the chip, sha256
`0831c3dff0ed06a78dfeb7ca88e48e180a6ce11e2317a32e1911b086325a23bb`.
Both slots byte-identical to the build; both FLAC blobs current. Supersedes
`working-2026-11-23-1245-launcher.bin`.
Both `ota_0` and `ota_1` regions confirmed byte-identical to the build, FLAC blobs
current. Supersedes `working-2026-11-23-1830-slotb-picker.bin` (entry 93).

## Next steps, in order

1. **Settle the rollback anomaly** — reset-within-30-s returning to slot B.
   Test properly: BOTH slots with valid otadata entries, reset inside the window,
   `otatool` output VISIBLE. Probable fix: confirm the desktop's image on boot
   after a local switch instead of after 30 s — but that is the desktop's OTA
   policy and needs operator approval.
2. **Remember the last cartridge** — the picker rescans every boot.
3. **Measure the frame-time claim** — compare a slot B `gbhist` against the same
   ROM under the desktop. Never checked against a run.
4. Remove the 0x1A000 trail once several sessions pass without a hang.
5. **Launcher files are NOT in the image** — `/root/Apps/gbslotb.py` and
   `Start menu/Game Boy Color.tdlink` live on littlefs `storage`. A full-image
   flash or factory restore removes them; sources are in `scripts/apps/`.
6. Open: frame-pool stall (parked), startup audio burst (~5,100 samples),
   Pusher A for NES/PM in the desktop, always-on gestures, `klio save` overflow,
   injection parked at 78, DMG unproven, savestate over-read, header 0x2c/0x44.

## Standing facts

- Internal RAM is the binding constraint; the optimisation line is closed four
  ways (entries 87–90).
- I2S DMA and the display's SPI bus share one internal pool (AUD-003).
- Slot is 0x3E0000; 24% free at 3.1 MB.
- **Back up the working tree before destructive experiments.**
