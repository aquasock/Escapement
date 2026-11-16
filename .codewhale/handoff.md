# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-16T10:00

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**Audio works on the DAC in OTG mode — first play after a plug, no headphone
replug — and the boot loop is gone. Cycle 1 is done and committed. Cycle 2 is
routing plus a global volume.**

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, branch `main`, at the
  commit that carries core-log **entry 22**. Core files: `.ai/core.md`
  (RESTRICTED), `.ai/core-log.md` (22/100 entries, newest first),
  `.ai/core-reference.md`, `.ai/core-syntax.md`.
- Active code: `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`,
  base `1f8edc2`. Push to `fork` (`aquasock/tinydesk`) only — never `origin`.
- Firmware on the board: **sha256 `d2231d74ddb02044`, size 0x2e0f20**, flashed
  app-only to 0x10000, `Hash of data verified`.
- **`/dev/ttyACM0` is a TinyTang CDC, not the watch.** The watch is the
  Espressif interface (`303a:1001`), normally `/dev/ttyACM1`.
- Crashed-build ELFs are in `/tmp` and must be copied out **before** the next
  build or the coredumps cannot be decoded: `tinydesk-000f3b3d.elf`,
  `tinydesk-fix1.elf`, `tinydesk-fix4.elf`, `tinydesk-fix6.elf` (current).

## What fixed it — one root cause, two bugs

**The internal DMA pool was too small, and three 4 KB task stacks were why.**

- `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y` is already set for this board.
- `usb_lib`, the uac2 client task and the uac2 worker are now created with
  `xTaskCreateWithCaps(MALLOC_CAP_SPIRAM)` — 12 KB returned.
- Measured, same board, same OTG mode:
  `dma=6359/4096 int=13887/7168` → `dma=18675/10752 int=26203/10752`.
- The claim then succeeded at `dma free=17543 largest=8704`. The refusal that
  looped read `free=5215 largest=2432`.
- None of those three tasks may call a flash/OTA API: a PSRAM stack cannot run
  while the flash cache is disabled. OTA runs on the shell's internal stack.

**The silent first stream was a separate defect.** The DAC **STALLs** the
playback Feature Unit's MUTE SET_CUR (`ctrl 21/01 status 4`), so the host cannot
unmute it. A headphone replug worked because it is a jack-detect inside the
dongle. `dac_wake()` now cycles alt 0 → streaming alt and re-asserts volume on
every start, and `uac2 wake` does it by command. Reference USB-006 moved from
INFERRED to VERIFIED.

## Four workarounds that failed — do not retry them

All were tried on hardware and each died differently:

1. Transfers allocated before the claim — they consumed the pool.
2. A reserve freed immediately before the claim — the claim's own two
   `calloc()`s (pipe_t 96, channel 20, headers 20) take the freed hole first, so
   the 512-aligned lists find nothing.
3. That reserve held *across* the claim — 1 KB the claim cannot use.
4. A 2 KB block held **from boot** — starves enumeration. Coredump, task
   `usb_lib`: `root_port_handle_events → usbh_devs_add → device_alloc →
   hcd_pipe_alloc → transfer_descriptor_list_alloc → assert`. **Any DMA held at
   boot is a bug on this board**, whatever its size.

## What to check next session

- Cycle 2: routing, and volume back to scaling samples through
  `sound_stream_*` with the DAC at 0 dB in fixed dB steps (USB-006 says the
  hardware volume is not usable on headphones).
- `dac_wake()` is a mitigation reproducing the replug's lever, not an
  established understanding of what the dongle latches. If a first stream is
  ever silent again, check that the alt cycle ran before assuming it did.
- The DAC has twice dropped off the bus after a headphone replug mid-stream
  (`last=7`, NO_DEVICE). Same dongle instability family as USB-006; not fixed.

## Instrument notes — these each cost real time

- **A panic in the USB host path is visible only in the coredump partition.**
  Read 0x600000 + 0x10000 and decode with `esp-coredump info_corefile --core
  <f> --core-format raw --off 0x18 --gdb <xtensa-esp32s3-elf-gdb> <elf>`. Build
  with `IDF_TOOLS_PATH=/home/vash/esp/idf-tools
  IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env`.
- **The watch's USB-C is either the PC's console or the OTG host, never both.**
  OTG tests are BLE-only. `board set usb.host 0` + `reboot` returns the port.
- **BLE session: `/tmp/blesess.py` holds ONE connection** (`exec` it as a
  background task). Commands go through the FIFO `/tmp/wt.cmd`; capture lands in
  `/tmp/wt.raw`. Text is typed 120 ms per character; a line starting `!` is
  written as one escaped byte string (`!\x1b[21~` = F10, `!\x11` = Ctrl+Q,
  `!\r` = Enter). **After a watch reset the session must be torn down and
  restarted** — the old handle reports `SENDFAIL: Service Discovery has not been
  performed yet` for every command while the board is healthy. That was a
  harness fault, now fixed to re-raise so the loop reconnects.
- `bluetoothctl disconnect 80:45:6B:46:DF:56` clears a stale link. The watch
  stops advertising while connected, so a scan showing nothing is normal.
- **The USB CDC carries the display mirror, not ESP_LOG.** After a reset only
  the Start menu is drawn; F10 opens it, and Terminal is the top row.
- **Never press Enter on a Start-menu row without reading the highlight.** Read
  SGR `48;5;25` on the selected row (`/tmp/sel.py` does this). A blind Enter
  opened Counter twice.
- **The mirror sends only changed cells**, so a capture started mid-session
  cannot show pre-existing text — `ESC[5000~` forces a full repaint.

## Owed

`bleuart fetch` still untested now that TOL-004 is correct. The BLE shell input
path still drops multi-byte writes. Cycle 3 remains the emulators' resampler
(gnuboy emits 43,691 Hz; `uac2` refuses unlisted rates rather than converting).
