# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-18T13:22

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**A real, demonstrably learnable gesture, recorded and replayed through injection,
does not learn.** Live it reaches 100 percent and `klio detect` reads back a
157-byte pattern; replayed five times it peaked 20/60/40/40/20 and `read_pattern`
returned rc -2 every time. So **the input content is not what separates the two
paths** — five entries of waveform design were chasing the wrong thing. One
difference is left untested and it is a single-variable change: Bosch's
**step-by-step injection mode**.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `85e63f4`
  (core-log **entry 78**). Pushed.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `059feb3`, pushed to `fork`.** Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream
  `tinydesk-project/tinydesk`).
- `.ai/core.md` is RESTRICTED. Its Purpose line was corrected in entry 74 with
  explicit authorization; do not edit again without a new instruction.

## HOST PORTS — the thing that has already gone wrong twice

The watch and the **TinyTang** both enumerate as `/dev/ttyACM` and **swap numbers
between reboots**. Tang `ffff:5454`, watch `303a:1001`.

- Watch: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00`
- Never open a ttyACM path by number. `tools/watch-screen.py` resolves by-id and
  refuses anything that is not an Espressif JTAG unit.
- `esptool --after no_reset` leaves the board in its ROM bootloader and silences
  the console; that hard reset also **closes the Terminal**. Finish every read
  with `--after hard_reset chip_id`, then reopen the Terminal.
- The operator flashes the Tang from this same workstation. Check
  `ps -ef | grep tinytang` and `fuser /dev/ttyACM*` before touching a port.

## OPENING A TERMINAL — reboot → F10 → Enter → verify `tdsh`

The Start menu remembers its last selection, so F10+Enter after using other apps
launches *that* app. A fresh boot starts on Terminal. Always read the screen back
and confirm `tdsh` **before** typing; failing to do so once fed `imu inject` into
the Tamagotchi emulator as game input. A display-repaint can also leave escape
residue in the shell's input line (`tdsh: command not found: ^[[?2026himu`) — send
a `Ctrl+C` first if the prompt looks polluted. Records TOL-005/006/007.

## What entries 77 and 78 established

```
entry 77  cycle-to-cycle sameness is VOID (proved on the host, no flash spent:
          existing float-phase loop already differs by <=1 LSB of 4096 on 0.8% of
          samples). Real defect found instead: the synthetic waveform rotated
          gravity y-z (about X) but wrote the rate to gy. shape 4 fixes the axis;
          peaks unchanged.

entry 78  THE CONTROL: `klio learn` from hand motion -> 100%, 12 frames,
          `klio detect` -> read_pattern ok (0), 157 bytes. Injection has never
          done this.

          `imu record <seconds>` -> /root/gesture.bin, six int16 per sample
          (ax,ay,az,gx,gy,gz), raw ints, 100 Hz. 3111 accel + 3111 gyro samples
          for 3000 ticks. Validated off the chip: |a| = 0.97 g, all axes swinging.
          The real gesture rotates x-z with rate on gy (about y) -- self-
          consistent, unlike the synthetic waveform.

          shape 5 replays it. Five runs: peaks 20/60/40/40/20, frames
          3/27/26/23/12, read_pattern rc -2 ALL FIVE.
          The 60 is ONE SAMPLE of a noisy distribution, not an improvement.
```

Audio verified after the final module relink: 258048 frames, 0 errors, by ear.
`/root/gesture.bin` still holds a 30 s recording — no need to ask the operator to
perform again.

## Next steps, in order

1. **Test normal injection mode instead of step-by-step.** This is now the only
   unexplained difference between a path that learns and a path that does not.
   `imu inject` calls `imu_inject_begin(BHY2_STEP_BY_STEP_INJECTION)`; a variant
   using `BHY2_NORMAL_MODE` is a single-variable change. Bosch's CLI uses
   step-by-step, but every hand-trained success ran against a free-running FIFO.
2. **If that fails, close the line with an entry that says so.** Six entries have
   eliminated waveform, axis, amplitude, rate, timestamp, transport and finally the
   input data itself. A further failure means injection is not equivalent to the
   sensor path on this firmware.
3. Before closing, two cheap ones: report how many injected samples the engine
   actually consumes per KLIO frame (a ratio near zero would explain a lot), and
   try the same recording at a lower injection rate.

**Reading traces:** order recovered traces by **flash offset**, not filename — the
run counter restarts at boot and littlefs keeps stale copies.

## Instrument

`imu inject <seconds> <shape> <sensor> <phys>` and `imu clock`, in
`ports/twatchultra/main/imu.c`.

- `shape 0` gesture 0.9 s swing + 0.6 s rest (baseline); `shape 1` 1 Hz sine
  control; `shape 2` gesture with 1.5 s rest; `shape 3` table-replayed twin of
  shape 0 (built, never run — hypothesis void); `shape 4` shape 0 with the gyro on
  the axis the acceleration implies; `shape 5` replay `/root/gesture.bin`.
- `imu record <seconds>` writes `/root/gesture.bin` from the physical sensors at
  100 Hz; `imu clock` measures the timestamp unit.
- `phys 0` disables accel and gyro — **use this**; it is what Bosch's CLI does and
  it is the only configuration with zero overflows.
- `imu clock` measures the timestamp unit; re-run it if the firmware image or the
  configured rate changes.
- Read traces off the chip: `read_flash 0x800000 0x400000`, search `totals:` and
  `part clock`. littlefs keeps stale copies.

## Standing facts

- The similarity metric from **real motion** (1.000 / 0.795 / 0.475 / negative) is
  the project's measured result; nothing here touches it.
- After any app rebuild, module order is: build app → flash → boot → `make
  modaddr` → `make flac` → flash 0xC00000/0xE00000. A stale link affects `music`
  only, not `imu`.
- Build env does not come from `export.sh` (fails on missing
  `riscv32-esp-elf-gdb`). Set `IDF_PATH`, `IDF_TOOLS_PATH`, `IDF_PYTHON_ENV_PATH`,
  put `xtensa-esp-elf/bin` and the venv bin on `PATH`, call `python -m idf.py` /
  `python -m esptool` from that venv.
- App at `0x20000`; `otadata` cleared so it boots `ota_0`.

## Conformance debt

Entries 66–73 use fenced code blocks inside `Outcome`, which `.ai/core-syntax.md`
prohibits. Recorded in entry 74, deliberately **not** fixed — the operator's call.
Entries 74–76 are written to the strict template; use them as the model.
