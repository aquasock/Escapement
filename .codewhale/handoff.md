# Handoff — Escapement, 2026-11-09T10:31

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail lives in `.ai/core-reference.md` (35 records, twelve topics).
Project history lives in `.ai/core-log.md`. Do not re-derive what those hold.

## Current objective (verbatim)

Bring up the watch's Linux fully. Deferred by user decision: BLE media keys,
firmware-side BLE bond persistence, and any wake-from-sleep path. Do not start
those without asking.

## Reconstruct (in this order)

1. `git log --oneline -8` in `/run/media/vash/GIT/Escapement` — expect `main` at
   `fd5f252`, clean, fully pushed.
2. `.ai/core.md`, then `.ai/core-reference.md`. The reference is the authority
   on hardware facts; read the record, not just the index line.
3. `git -C build/esp-hosted/esp_hosted_ng log --oneline -5` and
   `git branch --show-current` — expect `ble-bond-wip`.

## State verified on hardware this session

| Check | Evidence |
|---|---|
| Boot, login, fork | login prompt; interactive shell |
| AMOLED console | `Console: switching to colour frame buffer device 40x25` |
| Display border | `co5300: frame painted rc=0`; black surround + 1 px red ring, seen on glass |
| PMU | `axp20x-battery` 100% / 4175000 uV / Full, `axp20x-usb` online 1, four IRQs |
| RTC | `rtc0` registers, hctosys seeds the clock |
| BLE keyboard | types into the AMOLED login |
| WiFi | 10.10.0.15, ~409 KB/s, DNS resolving |
| Userspace fb draw | SPLB32 emulator ran on the panel, 163 frames, exit 0 |

Live check: `./scripts/watch-telnet.py 'uptime; /etc/init.d/S48blekbd status'`

## Two traps that will cost you a session if missed

**The firmware tree must never be pushed.** `build/esp-hosted/esp_hosted_ng`
has origin `jcmvbkbc/esp-hosted` — upstream, not this project. Its commits are
marked local-only. Commit locally, never push.

**Back up the whole flash, not the kernel.** Before any kernel or firmware
change: `./scripts/watch-backup.sh` (16 MiB, ~3.5 min, refuses if the port is
busy). A kernel-only backup is not enough — `/etc` damage is what once made a
saved known-good kernel appear broken, and the recovery cost the display, the
WiFi credentials, the root password and the keyboard deployment.

Restore: `esptool ... write_flash 0 0x1000000 <file>`.
Backups in `~/twatch-backup/`. Use `working-2026-11-09-1017.bin`
(sha256 `639da94c...`, verified to contain the running kernel and firmware).
Do **not** restore `working-2026-11-09.bin` — it predates `fb_write`.

## Known accepted defect

Running firmware has `sm_bonding` on while bond restore is broken, so the
keyboard needs pairing mode after each reboot. Not a regression. Fix is either
`sm_bonding = 0` in `ble_hid.c` (back to silent re-pair per connect) or finish
the load path in `bond.c`.

## Next step

**Touch.** CST9217 at I2C 0x1A is fitted and identified but has no driver, so
there is no local tap or pointer input at all. Needs `CONFIG_INPUT_TOUCHSCREEN`,
a driver, and the XL9555 reset/interrupt lines before it can be verified.

Then console repaint speed — it has a concrete lead: the full-width band
transfer shape the CO5300 actually accepts, found while drawing the border.

## Tooling

- `./scripts/watch-telnet.py '<cmd>'` — shell on the watch over telnet. Prefer
  this to the serial console; the serial port is 115200 and fights printk.
- `./scripts/watch-exec.py '<cmd>'` — same over the USB console, needs
  `~/.config/escapement/watch-pass`.
- `./scripts/watch-backup.sh` — full flash backup with hash.
- `./scripts/build-userspace.sh` — cross-compile; `--selftest` runs the HID
  decoder test.
- **`export XTENSA_GNU_CONFIG=$PWD/build/toolchain/xtensa-dynconfig/esp32s3.so`
  before any cross-compile.** Unset, the compiler emits big-endian objects and
  the link fails with "file in wrong format", which reads as a broken toolchain.
- Kernel: `make ARCH=xtensa CROSS_COMPILE=xtensa-esp32s3-linux-uclibcfdpic-
  xipImage` in `build/kernel/linux-7.2.4`; flash to `0x140000` only.
- Check `space_for_vectors` in the firmware ELF against `CONFIG_VECTORS_ADDR`
  (currently `0x40380000`) after any firmware change that moves RAM use.

## Evidence base

- `.ai/core-reference.md` — hardware facts and ruled-out causes
- `.ai/core-log.md` — entry 1 is this handoff
- `docs/watch-recovery.md` — recovery doctrine
- `docs/bluetooth-keyboard.md` — the BLE host, five defects, protocol trade
- `docs/bluetooth-bond.md` — bond persistence, half built
- `docs/panel-geometry.md` — glass area, and the measured one-pixel offset
