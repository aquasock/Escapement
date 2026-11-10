# Handoff — Escapement, 2026-11-10T10:44

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

Canonical detail lives in `.ai/core-reference.md` (63 records, fourteen topics).
Project history lives in `.ai/core-log.md` (26 entries; newest first — 26, 25, 24, 23,
22, 21, 20, 19, 18, 17, then 1 through 16). Do not re-derive what those hold.

## Current objective (verbatim)

Bring up the watch's Linux fully.

Deferred by user decision — do not start these without asking: BLE media keys,
the Rii K06's touchpad, the Logitech keyboard, firmware-side BLE bond
persistence, any wake-from-sleep path, and moving the emulator to core 0.

## Reconstruct (in this order)

1. `git log --oneline -8` — `main` was at `a47de90`, clean and fully pushed,
   before the commit carrying this file.
2. `.ai/core.md` (policy), then `.ai/core-reference.md` (hardware facts — open
   the record, not the index line), then `.ai/core-log.md` entries 26, 25, 24.
3. `git -C build/esp-hosted/esp_hosted_ng branch --show-current` — expect
   `ble-bond-wip`, five local-only commits.

## State verified on hardware — last checked 2026-11-10

Boot to login and fork; AMOLED console `40x25` in a 320x400 fb0 at (45,45);
`axp20x-battery` 100 % / 4175000 uV / Full and `axp20x-usb` online, four IRQs;
`rtc0` seeds the clock; Rii K06 pairs, types, and reconnects on its stored bond
after a reboot; WiFi 10.10.0.15 at ~409 KB/s; three touch bars paint, click and
emit BTN_0/1/2; the Tamagotchi runs at a true 1.00x from the flash-resident ROM
at 40 MHz with eight clean full-width bands.

**The watch is on the 40 MHz kernel as of 2026-11-10T09:40.** `spi_hz` reads
`40000000`, `tbtns` and `blekbd` run, and `/home` survived because the kernel
was written alone to `0x140000`. Rebuilt from the tree and flashed; it differs
from the pre-loop kernel in 12 bytes, all build timestamp.

**There is now exactly one image worth restoring**, and it is qualified:

```
~/twatch-backup/working-2026-11-10-0940.bin    16 MiB, sha256 fd7e5bf9
~/twatch-backup/twatch-factory-16MB.bin        untouched stock state
```

Use the first. Do not hunt for others — the directory was pruned from 17 images
to those two on 2026-11-10, and every older one was 20 MHz.

**Restore command, corrected this session** (the previously documented form was
wrong and cost an hour):

```sh
PY=build/idf-tools/python_env/idf5.1_py3.14_env/bin/python
$PY -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    --after no_reset write_flash 0 ~/twatch-backup/working-2026-11-10-0940.bin
```

`--after no_reset` is not optional: without it esptool resets the board into the
app at the end, Linux claims the USB-Serial-JTAG port, and every later command
hangs at `Connecting...`. A *boot-looping* board needs no button because it
re-enters the ROM bootloader every cycle. See `docs/watch-recovery.md`.

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

**The interpreter is measured, at last.** EMU-005/006/007/008 now hold counts
rather than estimates, taken on hardware 2026-11-10:

```
454305 instructions  736660 steps  282355 sleep steps   204045 instr/s   4.49x
1.622 steps/instr    2.408 accesses/instr   4922 ns/instr = ~1181 cycles
rom 72.6%   ram 21.4%   memory = 2.6% of cost   interpreter stream = 97%
```

That settles the question entry 18 left open, in the useful direction. Memory is
2.6 percent of an emulated instruction's cost, instruction fetch is nearly free
(8x the code footprint costs 3.8 percent), and the other 97 percent is the
interpreter's own instruction stream — the order of **150 native instructions
per emulated instruction** against the 30-80 a tuned core needs. The
block-register rewrite is justified and has a baseline to be judged against.

**The ROM's location is now measured, and it is the biggest win available.**
ROM 204033 instr/s (4.49x) in the flash window against 273098 (6.01x) in PSRAM —
**+33.8%**, nine flash runs across three boots, two PSRAM samples on two boots,
identical instruction counts. The saving of 1239 ns per instruction matches the
1290 ns the ROM-access cost predicted, within 4 percent. So the ROM should move
to PSRAM; the cost is that a 512 KB contiguous block exists only briefly at boot
(1 MB at 0 min, 256 KB by 9 min, 128 KB by 32 min) and one allocation consumes
it — KRN-004 — which means the emulator must start from rcS.

**And the touch daemon costs a third of the emulator's speed.** Same boot, same
binary: 141k instr/s with `S50tbtns` running, 204k with it stopped. Worth fixing
by having it stand down while a fullscreen app owns the panel.

Those two together are larger and far cheaper than the block-register rewrite,
which should now be re-justified against a 6.01x baseline rather than 4.49x.

**How to run the instrument.** `./scripts/build-corebench.sh` builds four
variants; only `corebench-flash` (77 KB, ROM read through the window at
0x42480000) actually execs here. The 601 KB compiled-in build is refused by the
kernel as 565248 bytes not contiguous, and the file-based one cannot malloc
512 KB. `corebench-prof` prints the counters — never trust its wall clock, the
counters perturb it.

## Open items, in the order they are worth doing

- **`S51tamafb` from rcS** — the one blocking item. It makes the emulator start
  at boot instead of by hand, and if it starts early enough the 512 KB ROM
  allocation can succeed, which is the only route to the 33.8 percent. It must
  write `/var/run/tamafb.pid` or the `tbtns` stand-down will not trigger.
- LCD-009's tearing root cause; the 4.2x disagreement between LCD-005's 190 ms
  and LCD-010's 45.7 ms — re-measure through the real repaint path first.
- The parked ~1.5 s time-screen wait.
- The top-bar status daemon — never started.
- The 2026-11-10 boot loop is unexplained and its dump is gone. If it recurs,
  dump the flash *before* restoring anything.
- `/etc/TZ`, `S01hwclock` and `S50tbtns` must be re-installed by hand after any
  **full-image** flash (`scripts/watch-time.sh` covers the first two).

## Two settled results, so they are not re-litigated

**The ROM will not fit in PSRAM as a plain malloc.** malloc(524288) fails where
malloc(524000) succeeds — the boundary is the allocator header — and splitting
the ROM into two halves fails for the same reason. KRN-004. The front end's code
is deployed and correctly falls back with `rom: flash window`.

**`tbtns` costs 34.7 percent and it is NOT the poll rate.** Slowing 15 ms to
66 ms moved throughput 1.1 percent, so the cost is per-wakeup, not per-read.
Unexplained; profile before acting on it. The daemon now *slows* rather than
stops during gameplay, because `tamafb`'s only input is the device `tbtns`
injects — stopping it made the buttons dead. Both transitions are in
`/home/tbtns.log`.

## Tooling

- `./scripts/watch-telnet.py '<cmd>'` — shell on the watch over telnet. Prefer
  this to the serial console; the serial port is 115200 and fights printk.
- `./scripts/watch-exec.py '<cmd>'` — same over the USB console, needs
  `~/.config/escapement/watch-pass`.
- `./scripts/usb-send.py` / `./scripts/watch-deploy.sh` — deploys. USB streaming
  through one console session; netcat is unavailable on that channel.
- `./scripts/build-userspace.sh` — cross-compile; `--selftest` runs the HID
  decoder and touch-band tests.
- `./scripts/build-corebench.sh` — the emulator instrument; see above.
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
