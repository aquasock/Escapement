# Handoff — Escapement, 2026-11-10T20:05

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail: `.ai/core-reference.md` (open the record, not the index line),
`.ai/core-log.md` (newest first), `docs/cores.md`, `docs/tamagotchi.md`.

## Current objective

The Tamagotchi emulator on **core 0**. Guest on the cached core, thin blitter on
core 1, frames through a shared PSRAM window. Approved by the user
("approved. lets do the move") and implemented this cycle.

## State on the watch, verified 2026-11-10 ~20:03

```
firmware   tamacore_task.c + splb32 + segmap compiled in, started pre-handoff
kernel     7 MB Linux + 1 MB shared window, CONFIG_ESP_SHARED, /proc/esp-shared
userspace  /home/tamacore deployed (blitter), /home/rom.bin, /home/tama.sav
watch      boots, zero Oops, PMU 100, rtc0 reading, uptime healthy
```

Measured on hardware, 10 s run:

```
guest rate      4,038,199 emulated cycles/s   (target 4,000,000)
frames          159 published                 (16 fps, exact)
worst lateness  11 ms
strip blit      17.0-17.4 ms per 320x57 write
```

## What works

- Core 0 runs SPLB32 at realtime with the ROM read from the window.
- Frames are published into the window on guest time, not wall time.
- Core 1 reads them and blits a full-width 320x57 strip via `fb_write()`.
- The window, the coherency and the handshake are all proven. Coherency was
  measured two ways: the counter advanced with and without the reader's
  invalidate, which means core 1 has no data cache and core 0's msync suffices.
- Boot is stable. The boot-loop, frozen-block, stale-flag and one-shot bugs are
  fixed and written up in `docs/tamagotchi.md`.

## Not verified — the open work

1. **The glass.** The emulated display region hashed identically over two
   seconds during one run, so the pet was not visibly animating in that sample.
   Could be an idle guest, could be frames not landing. Needs eyes.
2. **Buttons.** Never tested end to end since the move. `buttons` in the control
   block read 0 in every sample taken. Needs a press.
3. **Frame delivery.** One run blitted 77 of 159 published frames. The write is
   17 ms, so this is overruns (10 ms pacing tick against a 21 ms poll), not
   transfer cost. Fix by publishing on a tick boundary and polling tighter.
4. **No mutual exclusion.** Two `tamacore` instances will race for the window.

## Next actions, in order

1. Run `/home/tamacore 60` on the watch and watch the glass. If it is blank or
   frozen, dump frames A and B and compare against the emulated dpram.
2. Press the bars. If nothing happens, check that `buttons` moves in
   `/proc/esp-shared` at offset 44 before suspecting the guest.
3. Only then tune delivery. Do not optimise a path that has not been seen.

## Reconstruct

1. `git log --oneline -5` — `main` at `46e459e`, pushed to aquasock/Escapement.
2. `git -C build/esp-hosted/esp_hosted_ng log --oneline -3` — `d52e0c9`, local
   only. **That tree is never pushed** (origin `jcmvbkbc/esp-hosted`).
3. `./scripts/sync-firmware.sh --check` — should say the firmware tree matches.

## Build and flash

```sh
# firmware (core 0)
export IDF_TOOLS_PATH=$PWD/build/idf-tools
source build/esp-hosted/esp_hosted_ng/esp/esp_driver/esp-idf/export.sh
./scripts/sync-firmware.sh
ninja -C build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build

# kernel (core 1)
./scripts/build-kernel.sh        # or incremental make in build/kernel/linux-7.2.4

# blitter
./scripts/build-tamacore.sh

# flash app + kernel together, then deploy userspace
PY=build/idf-tools/python_env/idf5.1_py3.14_env/bin/python
$PY -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 --after no_reset \
    write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x10000 build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build/network_adapter.bin \
    0x140000 build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage
./scripts/watch-deploy.sh tamacore
```

`--after no_reset` is mandatory. **Check `space_for_vectors` before flashing** —
`0x40380000` is the expected value and it moves if internal RAM use changes.

## Traps that cost real time here

- **Never log from the firmware around the handoff.** The log pipe wedges the
  board there. Publish into the window instead.
- **Never write the window before the MMU gate.** Entry `0x1f0` must read
  `MMU_ACCESS_SPIRAM | 0x70` first. Two boot loops came from skipping this.
- **`esp_cache_msync(..., 0)` is not enough** for core 0's writes to reach core
  1; use `ESP_CACHE_MSYNC_FLAG_INVALIDATE`.
- **PSRAM is not zeroed at power-on.** Each side initialises its own line of the
  control block.
- **CO5300_IOC_WRITE_RECT refuses anything inside fb0** by design. Inside fb0
  the only path is `write()` to `/dev/fb0`.
- **The firmware has no PSRAM heap** (`SPIRAM_USE_MEMMAP`, not `USE_MALLOC`), so
  `heap_caps` returns NULL. Fixed window addresses only.
- **No flash writes from core 0** after Linux is up. Core 1 persists the save.
- WiFi is flaky when a nearby BLE mouse interferes, and no kernel indicator
  shows it. Prefer `./scripts/watch-exec.py`.

## Reference

- Repo `/run/media/vash/GIT/Escapement`, `main` clean at `46e459e`
- Window `0x3df00000..0x3e000000`; ctl `0x0`, frames `0x1000`/`0x2000`,
  EEPROM `0x3000`, ROM `0x80000`. Contract: `patches/harness/core0/shmproto.h`
- Control block offsets: stage 8, rom_ready 32, eeprom_ready 36, buttons 44,
  quit 48, frame_seq 64, frame_idx 68, frames 72, guest_hz 76, jitter_ms 84
- Backups in `~/twatch-backup/`: `working-2026-11-10-1756.bin` (last known-good
  full image), `kernel-7M-2026-11-10-1906.bin`, `app-7M-2026-11-10-1906.bin`
- Key numbers: 4.0 MHz guest clock · 16 fps · 17 ms strip · 51.8 ns dependent
  PSRAM load · 1181 cycles is corebench/flash and NOT comparable
- Deferred, do not start without asking: BLE media keys, Rii K06 touchpad,
  Logitech keyboard, firmware BLE bond persistence, wake-from-sleep
