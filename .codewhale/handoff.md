# Handoff — Escapement, 2026-11-10T07:55

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail lives in `.ai/core-reference.md` (57 records, fourteen topics).
Project history lives in `.ai/core-log.md` (19 entries; newest first — 19, 18,
17, then 1 through 16). Do not re-derive what those hold.

## Current objective (verbatim)

Bring up the watch's Linux fully.

Deferred by user decision — do not start these without asking: BLE media keys,
the Rii K06's touchpad, the Logitech keyboard, firmware-side BLE bond
persistence, any wake-from-sleep path, and moving the emulator to core 0.

## Reconstruct (in this order)

1. `git log --oneline -8` — `main` was at `99f3d13`, clean and fully pushed,
   before the commit carrying this file.
2. `.ai/core.md` (policy), then `.ai/core-reference.md` (hardware facts — open
   the record, not the index line), then `.ai/core-log.md` entries 19, 18, 17.
3. `git -C build/esp-hosted/esp_hosted_ng branch --show-current` — expect
   `ble-bond-wip`, five local-only commits.

## State verified on hardware — last checked 2026-11-10

Boot to login and fork; AMOLED console `40x25` in a 320x400 fb0 at (45,45);
`axp20x-battery` 100 % / 4175000 uV / Full and `axp20x-usb` online, four IRQs;
`rtc0` seeds the clock; Rii K06 pairs, types, and reconnects on its stored bond
after a reboot; WiFi 10.10.0.15 at ~409 KB/s; three touch bars paint, click and
emit BTN_0/1/2; the Tamagotchi runs at a true 1.00x from the flash-resident ROM
at 40 MHz with eight clean full-width bands.

**The watch is not connected right now.** No `/dev/ttyACM*` exists and
`10.10.0.15` is 100 % packet loss from a host on the same subnet. Nothing can
be built to it, deployed, or user-tested until it comes back.

Live check: `./scripts/watch-telnet.py 'uptime; /etc/init.d/S48blekbd status'`

## Two traps that will cost you a session if missed

**The firmware tree must never be pushed.** `build/esp-hosted/esp_hosted_ng`
has origin `jcmvbkbc/esp-hosted` — upstream, not this project. Commit locally,
never push.

**Back up the whole flash, not the kernel.** Before any kernel or firmware
change: `./scripts/watch-backup.sh` (16 MiB, ~3.5 min). A kernel-only backup is
not enough — `/etc` damage is what once made a saved known-good kernel look
broken, and the recovery cost the display, the WiFi credentials, the root
password and the keyboard deployment. Restore with `esptool ... write_flash 0
0x1000000 <file>`. Backups live in `~/twatch-backup/`; do not restore
`working-2026-11-09.bin`, which predates `fb_write`.

## Where the engineering actually stands

The display is not the bottleneck and the CPU is not the limit either — PSRAM
latency is. Linux runs on core 1, which has no data cache (`XCHAL_DCACHE_SIZE=0`,
no locality effect across three working sets) and no reachable internal SRAM, and
a dependent load costs 178.83 ns. The interpreter therefore pays roughly a dozen
PSRAM round trips per emulated instruction. Steady state the guest sleeps and the
emulator holds 1.00x; when the ROM computes, throughput collapses to ~0.38x,
which is the parked ~1.5 s time-screen wait seen from the other end.

**The open question is a count, not a theory.** EMU-005 is OPEN: the interpreter
spends ~3676 ns per emulated instruction against a 12.77 ns native yardstick, on
the order of 250 native instructions where a tuned core needs 30 to 80 — but that
figure is derived from timing and has never been counted. Count it first. Near
250 justifies the block-register interpreter (keep guest registers, flags and PC
in host locals across a decoded block; 2-3x). Nearer 80 means this port tops out
around 0.5x and the honest answer is the core-0 design or nothing.

Already excluded by measurement — do not re-tread: SPI clock (+13 %), DMA
(per-chunk cost is not on the critical path), frame rate (does not touch
instruction throughput), and `float` (not slow — it faults, because the kernel
does not enable the FPU coprocessor for user tasks).

## Open items, in the order they are worth doing

- The native-instruction count of EMU-005, then the interpreter restructure if it
  warrants it.
- LCD-009's tearing root cause: a long single-transfer window into the right
  strip splits diagonally, 2-row bands are clean, and the driver caps bands at
  two rows as a workaround. SPI clock is excluded as the explanation.
- The 4.2x disagreement between LCD-005's ~190 ms frame and LCD-010's 45.7 ms
  projection; re-measure through the real repaint path before quoting either.
- The top-bar status daemon — never started; needs no further kernel work.
- Three things must be re-installed by hand after any **full-image** flash:
  `/etc/TZ`, `S01hwclock` and `S50tbtns` (`scripts/watch-time.sh` covers the
  first two). A kernel-only flash leaves `/etc` alone.

## Tooling

- `./scripts/watch-telnet.py '<cmd>'` — shell on the watch over telnet. Prefer
  this to the serial console; the serial port is 115200 and fights printk.
- `./scripts/watch-exec.py '<cmd>'` — same over the USB console, needs
  `~/.config/escapement/watch-pass`.
- `./scripts/usb-send.py` / `./scripts/watch-deploy.sh` — deploys. USB streaming
  through one console session; netcat is unavailable on that channel.
- `./scripts/build-userspace.sh` — cross-compile; `--selftest` runs the HID
  decoder and touch-band tests.
- **`export XTENSA_GNU_CONFIG=$PWD/build/toolchain/xtensa-dynconfig/esp32s3.so`
  before any cross-compile.** Unset, the compiler emits big-endian objects and
  the link fails with "file in wrong format", which reads as a broken toolchain.
- Kernel: `make ARCH=xtensa CROSS_COMPILE=xtensa-esp32s3-linux-uclibcfdpic-`
  `xipImage` in `build/kernel/linux-7.2.4`; flash to `0x140000` only. Current
  image is 2,833,064 B, well clear of the 0x480000 ROM slot.
- Check `space_for_vectors` in the firmware ELF against `CONFIG_VECTORS_ADDR`
  (`0x40380000`) after any firmware change that moves RAM use.
- The emulator and its harness live outside this repo at `../TamaHarness`;
  `portable/` holds `splb32.c`, `tamafb.c`, `corebench.c` and the ROM.

## Evidence base

- `.ai/core-reference.md` — hardware facts and ruled-out causes
- `.ai/core-log.md` — entry-by-entry history, newest first
- `docs/tamagotchi.md` — the emulator, its measurements, its parked defect
- `docs/watch-recovery.md` — recovery doctrine
- `docs/panel-api.md`, `docs/panel-geometry.md`, `docs/panel-transfer-cost.md`
- `docs/bluetooth-keyboard.md`, `docs/bluetooth-bond.md`
