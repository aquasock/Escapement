# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-23T10:20

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**The Game Boy runs in slot B with no desktop behind it** — no shell, no status
strip, no WiFi, no SSH; picture, UI, audio, video, touch, haptics and Pusher A
all confirmed on the glass (entry 92). The watch is a game console one
`ota rollback` away, and the desktop is the default boot.

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

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, entry 92
  (reference **BRD-006** added this cycle; **TOL-009** in entry 91).
- Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `af4a9cd`, pushed to `fork`. Push to `fork`
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
  `esp_ota_set_boot_partition(ota_0)` → restart.
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
2. **Registering a hook does not start a driver.** `touch_add_app_touch()` and
   `pwrkey_set_app_key()` install into a poll loop that `touch_init()` /
   `pwrkey_init()` create. Haptics need BOTH the rail (`board_haptic_power_on()`)
   AND `haptic_init()` (which writes the library and wave sequence). Three dead
   peripherals came from this one mistake.

### Board fact behind the "panel worked by luck" (BRD-006)

The **XL9555's CONFIG and OUTPUT registers survive an ESP32-S3 reset.** After a
warm reboot the panel and motor rails are still driven; on a **cold boot every
expander pin is an input**, so nothing is powered. Never infer a rail from what
the glass shows — drive the pins explicitly.

## Diagnostics in flash padding (INTERIM — FIRST THING TO REMOVE)

```
0x1A000  slot B stage trail   (only when slot B runs)
0x1B000  which slot booted    (EVERY boot — a desktop-side flash write)
```

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

## Current board state

- Runs entry-92 build (3,100,032 B), **desktop on ota_0 by default**; slot B is
  entered deliberately and both slots hold the same image.
- Module `host=0x3c280f40`, MMU `0x42ae0000`/`0x3cce0000`.
- ROM is hard-coded (`GB_ROM_DEFAULT`, the Zelda GBC cartridge).

## Backup

`~/twatch-backup/working-2026-11-23-1700-slotb-emu.bin` — 16 MiB, verified
against the chip, sha256
`9307ab83a140f34f39534ede2807c2ed8c0e066a6ace186a5759970a7fcdbdc8`.
Both `ota_0` and `ota_1` regions confirmed byte-identical to the build.

## Next steps, in order

1. **Delete `slotb_boot_mark()`** — writes a flash sector on every desktop boot.
2. **Prove the rollback net** — `slotb_confirm()` works; a slot B that dies
   *before* confirming has never been tested. A deliberate crash is the test.
3. **Cartridge selection in slot B** — the SD card is mounted, so a picker or a
   remembered choice is now available.
4. **Measure the frame-time claim** — slot B was argued from 8–13 ms of polling
   SPI leaving the guest's core. Compare a slot B `gbhist` against the same ROM
   under the desktop; it has never been checked against a run.
5. Remove the 0x1A000/0x1B000 markers with (1).
6. Open: frame-pool stall (parked), startup audio burst (~5,100 samples),
   Pusher A for NES/PM in the desktop, always-on gestures, `klio save` overflow,
   injection parked at 78, DMG unproven, savestate over-read, header 0x2c/0x44.

## Standing facts

- Internal RAM is the binding constraint; the optimisation line is closed four
  ways (entries 87–90).
- I2S DMA and the display's SPI bus share one internal pool (AUD-003).
- Slot is 0x3E0000; 24% free at 3.1 MB.
- **Back up the working tree before destructive experiments.**
