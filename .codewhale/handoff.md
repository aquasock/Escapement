# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-15T22:45

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**The boot loop is fixed and hardware-confirmed; audio still does not play, and
`uac2 play` currently resets the board from a half-open state.**

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, branch `main`, clean at
  `9415d47` (log entry 21). Core files: `.ai/core.md` (RESTRICTED — do not edit
  without an explicit request), `.ai/core-log.md`, `.ai/core-reference.md`,
  `.ai/core-syntax.md`.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `1f8edc2`, pushed to `fork`
  (`aquasock/tinydesk`).** Never push `origin` (upstream
  `tinydesk-project/tinydesk`). Entry 1's `~/GIT/tinydesk-twatch` path is still
  wrong; it is `/run/media/vash/GIT/`.
- Firmware on the board: sha256 `d69daa80dc4d22ba`, size 0x2e07a0, flashed
  app-only at 0x10000 with `Hash of data verified`.
- Backups in `~/twatch-backup/`:
  - `working-2026-11-15-2231.bin` — full 16 MiB, sha256 `fa5906277247b91e`,
    `sha256sum -c` OK, `ota_0` inside byte-identical to the flashed build.
    `esptool verify_flash` against the chip reported a digest mismatch; the
    cause is known — two 64 KiB blocks (0x620000, 0xfb0000) inside the littlefs
    `storage` partition changed between the read and the verify because the
    running firmware writes there. App and coredump partitions were identical
    across two reads. Not corruption.
  - `app-2026-11-15-2158-5814b472.bin` — the app that was on the board before
    this cycle (build 20:37:06).

## What this cycle established

- **The boot loop was ours.** Coredump, task `usb_lib`:
  `assert failed: transfer_descriptor_list_alloc hcd_dwc.c:1059 (ret == ESP_OK)`
  via `hub.c root_port_handle_events → usbh_devs_add → device_alloc →
  hcd_pipe_alloc`. **Enumeration**, not the interface claim. Its descriptor list
  is 24 bytes, so it failed from exhaustion: `uac2_auto_start()` took 5,632
  bytes of DMA at boot plus the 25,600-byte shell hold.
- Fix: allocation moved into `uac2_open()` (after enumeration, still before any
  SD file opens — what USB-005 actually needs). The unplug path no longer
  re-takes the reserve.
- **Entry 20 was wrong** about the board: the image read back already carried
  the shell-hole fix. A naive byte diff of two builds of the same source reports
  ~71% difference from a ~232-byte layout shift.
- Boot heap log is now `pre-USB dma=free/largest int=free/largest` — DMA first,
  36 columns, because the Log Viewer cuts at 38 and keeps 128 lines.

## Two defects remaining — a DAC in OTG mode

1. **The automatic open at boot fails**: `transfer alloc: ESP_ERR_NO_MEM` at
   3338 ms, because it runs while the 25,600-byte shell hold is still taken. The
   same `uac2 open` typed from the shell afterwards succeeds. Ordering, not
   capacity.
2. **`uac2 play` resets the board.** Coredump task `uac2play`:
   `assert failed: xRingbufferReceiveUpTo ringbuf.c:1170 (pxRingbuffer && pxItemSize)`
   at `uac2.c:1260`, from `uac2_start_rate(44100)` in `play_task`, ring NULL.
   Chain: the failed boot-time open set `s_dev` and returned before allocating
   transfers or creating the ring; `uac2_open()`'s `if (s_dev) return true` then
   reported `open ok` without completing the open. Do **not** run `uac2 play` on
   a boot whose automatic open failed — it reboots the watch.

## Do this first

Fix the half-open state as one coherent change: clear `s_dev` (or track opened
state separately) when an open fails after setting it, and make the ring drain in
`uac2_start_rate` intolerant of a NULL `s_ring` — an assert inside IDF is a reset
with no log. Then gate the automatic open until `board_shell_task_prepare()` has
released the hold. Acceptance is `uac2 play /sd/media/audio/clip10.wav` in OTG
mode with no typed `uac2 open`, reporting `short=0 underruns=0`, heard on
headphones.

## Instrument notes — these cost the cycle

- **A panic in the USB host path is invisible everywhere except the coredump
  partition.** No console, no Log Viewer, no BLE: the reset lands first. Read
  0x600000 + 0x10000 and decode with `esp-coredump info_corefile --core <f>
  --core-format raw --off 0x18 --gdb <xtensa-esp32s3-elf-gdb> tinydesk.elf`.
  The decoder needs GDB; `export.sh` fails on a missing `riscv32-esp-elf-gdb`, so
  build with `IDF_TOOLS_PATH=/home/vash/esp/idf-tools` and
  `IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env`.
- **BLE is the only remote channel in OTG mode, and it drops multi-byte writes
  to the shell** — single bytes arrive and a 4-byte escape sequence to the
  desktop arrives, but `heap\r` as one write produces nothing. Type
  byte-at-a-time. The viewer that reads the mirror must keep the whole stream:
  the mirror sends only changed cells, so transient output is never in a capture.
- `ble-serial` must be restarted after any watch reset and loses `/tmp/ttyBLE`
  when it drops. Because the shell is reachable over BLE, the OTG key can always
  be cleared with `board set usb.host 0` + `reboot`.
- Never press Enter on a Start-menu row without reading the highlight first:
  selected rows paint with `48;5;25`. Read raw bytes, not the stripped grid.

## Owed

`bleuart fetch` is still in the build and still untested now that TOL-004's
write path is correct. USB-006's silent-DAC state has no established cause.
A reference record for the IDF assert-on-NULL-handle family (cf. TOL-004) was
considered and deliberately not added this cycle — it may be worth one.

## Plan unchanged

This was still cycle 1 of the operator's three: cycle 2 is routing plus a global
volume by scaling samples through `sound_stream_*` (the DAC's own volume is not
usable on headphones — USB-006), cycle 3 is the emulators' resampler.
