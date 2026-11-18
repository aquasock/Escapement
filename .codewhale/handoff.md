# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-18T12:52

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**Stop synthesising.** Five entries have eliminated every physical property of the
injected signal that can be checked — framing, alignment, inject ids, FIFO, rate,
timestamp unit and encoding, rest duration, cycle repeatability, and as of entry 77
the gyro axis (it was on Y; the acceleration it accompanies implies rotation about
X). The engine still counts one repetition, reports 20, and resets with
`change_reason 1`. **The next experiment is to replay a gesture recorded from a real
hand** — the only way left to separate "injection cannot drive this engine" from
"the synthesised waveform is still wrong".

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `f9dd08c`
  (core-log **entry 77**). Pushed.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `b6d5b44`, pushed to `fork`.** Push to `fork`
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

## What entries 76 and 77 established

```
entry 76  the injected TS field is a DELTA bhy2.c ADDS; the code wrote an absolute
          counter and zeroed it at 65000. `imu clock` measured the unit from the
          physical accelerometer alone: 1282-1283 ticks per 20 ms => 64130 ticks/s,
          641 per 10 ms block. Fixed; peaks unchanged (20/40/40/40).

entry 77  cycle-to-cycle sameness is VOID -- proved on the host, no flash spent:
          the existing float-phase loop already differs by at most 1 LSB of 4096
          on 0.8 percent of samples. shape 3 (table replay) exists but was never run.

          real defect found instead: the acceleration rotates gravity in the y-z
          plane, which is a rotation ABOUT X, but the gyro rate was written to gy.
          Every block from entry 73 to 76 asserted a tilt about x AND a spin about
          y at once. shape 4 fixes the axis and changes nothing else.

          six alternating runs: shape 0 peaks 20/40/40, shape 4 peaks 40/40/40.
          Both cap at 40. The axis fix is correct and is not the ceiling.
```

Audio re-verified after the last module relink: 253440 frames, 0 errors.

## Next steps, in order

1. **Replay a recorded hand-trained gesture.** A hand-trained gesture already learns
   and recognises — that is the project's measured result. Record accel+gyro at
   100 Hz during one, then replay those exact samples through injection. If a
   real, learnable recording also fails to produce a 157-byte pattern, injection is
   not equivalent to the sensor path and no waveform design will fix it: a firm
   negative result and the honest end of this line. If it succeeds, synthesis was
   the problem. Needs `imu record <seconds>` writing samples to a file, and a shape
   that replays a file instead of generating one.
2. **Characterise the spread.** The same command on the same image has peaked at 80,
   40 and 20. No claim smaller than that spread can be read. Run one configuration
   six-plus times and report the distribution.
3. Traps when reading traces: the run counter restarts at boot, so order recovered
   traces by **flash offset**, not by filename; littlefs keeps stale copies.

## Instrument

`imu inject <seconds> <shape> <sensor> <phys>` and `imu clock`, in
`ports/twatchultra/main/imu.c`.

- `shape 0` gesture 0.9 s swing + 0.6 s rest (baseline); `shape 1` 1 Hz sine
  control; `shape 2` gesture with 1.5 s rest; `shape 3` table-replayed twin of
  shape 0 (built, never run — hypothesis void); `shape 4` shape 0 with the gyro
  on the axis the acceleration implies. Other values refused with a usage line.
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
