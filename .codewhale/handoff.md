# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-18T12:20

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**Rest duration is not the variable.** Six runs, three of each shape, alternating:
the baseline gesture (0.6 s rest) and a lengthened one (1.5 s rest) are
indistinguishable — peaks 20/40/40 for both — with zero FIFO overflows and zero
sensor errors in all six, and `change_reason` 2 never observed once. **Progress
reaches 20 and resets to 0 on the very next frame**, so the engine counts one
repetition and judges the following one different. The next question is
cycle-to-cycle sameness of the injected waveform, not its timing.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `54b88ac`
  (core-log **entry 75**). Pushed.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean, in sync with `fork`.** Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream
  `tinydesk-project/tinydesk`).
- `.ai/core.md` is RESTRICTED. Its Purpose line was corrected in entry 74 with
  explicit authorization. Do not edit it again without a new instruction.

## HOST PORTS — the one thing that has already gone wrong

The watch and the **TinyTang** both enumerate as `/dev/ttyACM` and **swap numbers
between reboots**. Tang `ffff:5454`, watch `303a:1001`.

- Watch: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00`
- Never open a ttyACM path by number. `tools/watch-screen.py` resolves by-id and
  refuses anything that is not an Espressif JTAG unit. Records TOL-005/007.
- `esptool --after no_reset` leaves the board in its ROM bootloader and silences
  the console. Finish every read with `--after hard_reset chip_id`. Record TOL-006.
- **That hard reset closes the Terminal**, so a run sent immediately afterwards
  goes to the desktop and silently does nothing.

## OPENING A TERMINAL ON THE WATCH — the recipe, because this cost a cycle

**reboot → F10 → Enter → verify `tdsh` appears**, in that order.

The Start menu remembers its last selection, so F10+Enter after a session of
opening other apps launches *that* app. Several runs in entry 75's cycle launched
the Tamagotchi emulator and typed `imu inject` into it as game input. A fresh
boot starts with Terminal selected, which is why the reboot makes it
deterministic. Always read the screen back and check for `tdsh` before typing.
Record TOL-007.

## What entry 75 measured

```
six runs, shape 0 (0.6 s rest) vs shape 2 (1.5 s rest), phys 0, 3000 blocks each:
  shape 0: peak 20, 40, 40   frames  9, 21, 22
  shape 2: peak 20, 40, 40   frames  6, 22, 22
  every run: 0 fifo overflow, 0 sensor error, read_pattern rc -2 / 244 B
  change_reason 2 (no movement): never observed in any of the six

trajectory, every run: progress 20 on the frame after one repetition,
then back to 0 on the NEXT frame, repeatedly through the run.
```

Entry 74's runs peaked at 80 and 20; these six never exceeded 40. Same image,
same command. Six runs is too few to characterise that spread — say so rather
than explaining it.

## Next steps, in order

1. **Are the 7-byte packets on sensor 112 the injected blocks coming back?**
   The tap records 7-byte packets interleaved with its 11-byte KLIO frames even
   with both physical sensors off. If those are the injected samples, the engine
   is fed each sample twice and no waveform will ever be learned. Cheapest check,
   and it can make item 2 moot.
2. **Cycle-to-cycle sameness.** Make consecutive cycles bit-identical — e.g. a
   repeatable table rather than recomputing from `fmodf(t, period)` on a 10 ms
   grid, which does not land on the same samples each cycle — or simplify to a
   single-axis swing so the only cycle-to-cycle difference is numerical noise.
3. **Characterise the peak spread** before any convergence claim: several runs per
   configuration, and report the range.

## Instrument

`imu inject <seconds> <shape> <sensor> <phys>` in `ports/twatchultra/main/imu.c`.

- `shape 0` gesture, 0.9 s swing + 0.6 s rest (baseline); `shape 1` 1 Hz sine
  control; `shape 2` gesture, 0.9 s swing + 1.5 s rest. Other values are refused
  with a usage line.
- `phys 0` disables accel and gyro — **this is the configuration to use**; Bosch's
  own CLI activates only the algorithm. `phys 1` keeps the old default.
- Each run writes `/fs/root/inject-<shape>-p<phys>-r<run>.txt`; the run counter
  restarts at boot. Read it off the chip with `read_flash 0x800000 0x400000` and
  search for `totals:` — littlefs keeps stale copies, so match the header line.

## Standing facts

- The similarity metric from **real motion** (1.000 / 0.795 / 0.475 / negative) is
  the project's measured result and nothing here touches it.
- After any app rebuild, the module must be rebuilt in order: build app → flash →
  boot → `make modaddr` → `make flac` → flash 0xC00000/0xE00000.
- Build environment does not come from `export.sh` (fails on a missing
  `riscv32-esp-elf-gdb`). Set `IDF_PATH`, `IDF_TOOLS_PATH`, `IDF_PYTHON_ENV_PATH`,
  put `xtensa-esp-elf/bin` and the venv bin on `PATH`, and call `python -m idf.py`
  / `python -m esptool` from that venv.
- App is at `0x20000`; `otadata` was cleared so it boots `ota_0`.

## Conformance debt

Entries 66–73 use fenced code blocks inside `Outcome`, which `.ai/core-syntax.md`
prohibits. Recorded in entry 74, deliberately **not** fixed — reformatting settled
history is the operator's decision. Entries 74 and 75 are written to the strict
template; use them as the model.
