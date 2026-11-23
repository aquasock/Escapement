# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-23T09:45

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**Slot B exists, works, and is confirmed on the glass** — one image in both OTA
slots, `ota_1` boots a bare personality that starts no shell, desktop, WiFi or
SSH (entry 91). Next increment is the emulator payload on that proven base.

**Background is now verified and complete for the current build** — the gap
entries 89 and 90 left is closed.

## WORKING WITH THE OPERATOR ON THE WATCH — read this first

**When the operator says they are testing, send no commands.** Not status, not
`ls`, not a read "to check". Boot what they ask for, then stop.

**NEVER reset the board mid-cycle with an instrument.** On 2026-11-23 a
`otatool read_otadata` run three seconds after `switch_ota_partition` reset the
board and restarted slot B mid-sequence. Wait out the cycle (≈30 s) before
touching the port.

**`esptool ... --after no_reset` leaves the board in the bootloader (TOL-006).**
Every marker read must be followed by a real reset:
`esptool ... --before default_reset --after hard_reset run`.

**THE USB MIRROR IS DEAD WHILE A PICTURE OWNS THE PANEL (DSP-003).** A blocking
host read during a picture hangs for ever. Nothing read during a picture is
trustworthy.

**Do not pipe a long run through `tail`.** It buffers to exit.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, entry 91
  (reference **TOL-009** added).
- Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `188f262`, pushed to `fork`. Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

## SLOT B — how it works (entry 91)

One image, flashed to **both** `ota_0` (0x20000) and `ota_1` (0x400000). The
slot the bootloader picked decides the personality.

- **Seam:** `board_startup_override()` in `ports/esp_idf/app/main.c`, **weak**,
  called after `nvs_flash_init()` and before the netif/shell/desktop. Returns
  true → `app_main` stops there.
- **Implementation:** `ports/twatchultra/main/slotb.c`, strong, returns false on
  `ota_0` so the desktop path is unchanged.
- **Entry:** `ota rollback` in a shell, or
  `otatool switch_ota_partition --name ota_1`.
- **Exit:** `esp_ota_set_boot_partition(ota_0)` + `esp_restart()`.
- **Slot B brings up its own power rails** (`board_power_rails_preinit()`)
  because the override runs before `app_main` reaches them, and the AMOLED is
  behind a switched regulator.

### THE TRAP THAT COST THIS CYCLE — read before adding any board hook

A **weak definition in an already-included object satisfies every reference**,
so the linker never fetches the strong one from `libmain.a`. The module was
absent from the image and the firmware built, flashed and booted normally.
`nm <elf> | grep board_startup_override` showed **`W`** instead of `T`.

Fixed by `target_link_options(... PUBLIC "-Wl,--undefined=slotb_force_link")`.
**Any new weak board hook needs the same treatment and the same `nm` check.**
See TOL-009.

## Diagnostics in flash padding (interim — REMOVE BEFORE SHIPPING)

```
0x1A000  slot B stage trail   (only written when slot B runs)
0x1B000  which slot booted     (EVERY boot — a desktop-side flash write)
```

Stage codes: 1 ENTER, 2 RAILS, 3 DISPLAY, 4 PAINT, 5 HOLD, 6 RETURN,
7 SETBOOT_FAILED, 99 DISPLAY_FAILED. 200+subtype = boot-slot mark
(0x10 ota_0, 0x11 ota_1). Read both with one contiguous read:

```sh
esptool -p <by-id> -b 921600 --before default_reset --after no_reset \
        read_flash 0x1A000 0x2000 /tmp/mark.bin
# then reset for real:  ... --before default_reset --after hard_reset run
```

**`slotb_boot_mark()` must be deleted before this is shipping code** — it
erases and writes a flash sector on every desktop boot.

## Proving sequence (what "pass" looked like)

Green → white square (through `display_panel_push()`) → blue → desktop returns.
Each phase 4 s. Operator confirmed 2026-11-23.

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

**Flash both slots after any build:**
```sh
esptool -p <by-id> -b 921600 --before default_reset --after hard_reset \
        write_flash 0x20000 build/tinydesk.bin 0x400000 build/tinydesk.bin
```
Add `0x7F0000 build/ota_data_initial.bin` to force the boot back to `ota_0`.

**Expect a relink whenever the image size changes.** Check first:
```sh
xtensa-esp32s3-elf-nm build/tinydesk.elf | grep -E ' g_modhost_ptr| g_modbss$'
grep -E 'MODHOST_PTR_AT|MODBSS_AT' modules/build/modaddr.h
```
If they differ (they did this cycle: `host=0x3c280db8`):
```sh
cd ports/twatchultra/modules
python3 read_modaddr.py --port <by-id> -o moddata.at   # ~4 min, reads storage raw
make flac                                              # prints host=0x...
esptool write_flash 0xC00000 build/flac.bin 0xE00000 build/flac.data.bin
```

**NEVER `rm sdkconfig`** — see TOL-008. Copy aside, compare after build.

## Current board state

- Runs entry-91 build, `ota_0`, seq 5 (desktop is the boot target).
- `ota_1` holds the same image at seq 4 — one `ota rollback` away.
- App 3,099,152 bytes; module `host=0x3c280db8`, MMU `0x42ae0000`/`0x3cce0000`.
- `music` relinked and flashed; verified only by hash, not played since.

## Backup

`~/twatch-backup/working-2026-11-23-0910-entry90.bin` — 16 MiB, verified
against the chip, sha256
`f716a49a57115abf79dce35ec129b1a52769189d66268beb47ec9d9f0b792aa0`.
Contains the **entry-90** build (3,098,048 B), not the slot-B build.
**Take a fresh backup once slot B is validated**, since the current flash is
not in any verified image.

## Next steps, in order

1. **Emulator payload in slot B** — gnuboy on one core, blitter on the other,
   touch/gamepad input, audio, Pusher A to return (reuse `gb_app_key()`).
2. **Delete `slotb_boot_mark()`** — interim instrument, writes flash every boot.
3. **Mount the SD card in slot B** — no `td_fs_stdio_redirect` there; needed for
   cartridges and `/sd/diag/`.
4. **Prove rollback protection** — configured but never exercised. A deliberate
   crash before `slotb_confirm()` is the cheap test.
5. Still open: frame-pool stall (parked, `/sd/diag/gbwedge.txt`), startup audio
   burst (~5,100 samples), Pusher A for NES/PM, always-on gestures, `klio save`
   overflow, injection parked at 78, DMG unproven, savestate over-read,
   firmware header fields 0x2c/0x44.
6. Operator decisions outstanding: statusbar stack (5120 vs 8192), two-slot
   rationale now reframed as fit/power/boot-time.

## Standing facts

- Internal RAM is the binding constraint; the optimisation line is closed four
  ways (entries 87–90).
- I2S DMA and the display's SPI bus share one internal pool (AUD-003).
- Slot is 0x3E0000; 24% free at 3.1 MB.
- **Back up the working tree before destructive experiments.**
