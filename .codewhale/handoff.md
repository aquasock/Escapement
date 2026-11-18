# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-18T12:35

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**Transport, sensors, waveform timing, rest duration and the timestamp are now all
eliminated.** Entry 76 found and fixed a real defect — the injected timestamp was
an absolute counter in a field the part *adds* as a delta, wrong by 15.6× and
resetting every seventh block — and correcting it left the result statistically
unchanged (peaks 20/40/40/40 vs a 20/40/40 baseline). The only untested variable
left is **cycle-to-cycle sameness of the injected waveform**, and before that,
characterising the 20-to-80 peak spread.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `3d6d38a`
  (core-log **entry 76**). Pushed.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `d1e3a30`, pushed to `fork`.** Push to `fork`
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

## What entry 76 established

```
The 7-byte packets entry 75 flagged: across 39 stored traces they appear ONLY in
phys-1 runs, and in NO phys-0 run. The suspected double-feed does not exist;
entry 75 carried that claim forward without re-observing it.

imu clock (physical accel only, outside injection):
  consecutive parsed timestamps +1282,1283,1282,1283,... ticks per 20 ms sample
  => 64130 ticks/s, 1 tick = 15.59 us, 10 ms block = 641 ticks

the injected field is a DELTA that bhy2.c ADDS; the old code wrote an absolute ms
counter (10000/block) and zeroed it at 65000 => 15.6x fast + sawtooth every 7th block
fixed: 641 ticks/block, natural 16-bit wraparound, no reset

after the fix, four runs: peaks 20/40/40/40, frames 15/25/22/16
pre-fix baseline:        peaks 20/40/40,   frames  9/21/22
=> the fix is correct on its own merits and is NOT what blocks learning
```

Audio re-verified after the module relink: 290304 frames, 0 errors, 0 short
writes, confirmed by ear.

## Next steps, in order

1. **Characterise the peak spread first.** The same command on the same image has
   peaked at 80 (entry 74), 40 and 20. An effect smaller than that spread cannot
   be seen. Run one configuration six or more times and report the distribution.
   Traces are named `inject-<shape>-p<phys>-r<run>.txt`, but the run counter
   restarts at boot — order recovered traces by **flash offset**, not by name.
2. **Cycle-to-cycle sameness.** Generate the waveform ONCE into a table and replay
   it exactly, so every cycle is bit-identical. Today it is recomputed per block
   from `fmodf(t, period)` on a 10 ms grid, which does not land on the same
   samples each cycle.
3. If that changes nothing, the waveform's *shape* is the problem — next try a
   single-axis swing.

## Instrument

`imu inject <seconds> <shape> <sensor> <phys>` and `imu clock`, in
`ports/twatchultra/main/imu.c`.

- `shape 0` gesture 0.9 s swing + 0.6 s rest; `shape 1` 1 Hz sine control;
  `shape 2` gesture with 1.5 s rest; other values refused with a usage line.
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
