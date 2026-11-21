# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-21T11:00

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**Pusher A closes the Game Boy emulator** — one short press, desktop back in
29–40 ms, SRAM saved first — built, flashed, and confirmed by the operator on
the glass (entry 89). **An intermittent blitter stall is real and is PARKED by
operator decision**, instrumented so its next occurrence names its own cause.
The AI-engine/injection line stays parked at entry 78.

## WORKING WITH THE OPERATOR ON THE WATCH — read this first

**When the operator says they are testing, send no commands.** Not status, not
`ls`, not a read "to check". Boot what they ask for, then stop.

**THE USB MIRROR IS DEAD WHILE A PICTURE OWNS THE PANEL (DSP-003), and this
cost a session on 2026-11-21.** A host polling `gb status` during a run got
nothing, and the read blocked for nine minutes with 28 ms of CPU until killed.
Nothing read from the host during emulation is trustworthy, and a blocking read
will hang. Instruments for anything that happens during play must write to the
card (`/sd/diag/`), not to the console and not to the mirror.

**Do not background a capture, and do not pipe a long run through `tail`.**
`tail` buffers to exit, so a 23-minute sweep shows nothing for 23 minutes.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at the entry
  89 commit (core-log **entry 89**; reference **LCD-014** added).
- Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `78a8c4c`, pushed to `fork`. Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

## Pusher A (entry 89, working)

`pwrkey.c` owns Pusher A — the AXP2101's PEK input on PMU IRQ GPIO 7, **not a
GPIO**. A short press was always Enter; it is now offered to an app hook first:

```c
void pwrkey_set_app_key(pwrkey_app_key_fn fn, void *user);  /* set ONCE, at init */
```

Set once for the reason `touch_set_app_touch()` documents — a registration torn
down with the app leaves the driver holding a freed pointer. `gb.c` registers
`gb_app_key()`, which claims the press only while a guest runs, calls `gb_stop()`
(SRAM saved first) and returns true; otherwise it returns false and Enter passes
through. The press buzzes either way. **GBC only** — NES and Pokemon Mini keep
Enter until this is proven.

**Never print from that hook.** The rule in `pmbench.c` is measured: a foreign
task's stdout does not survive the desktop's terminal capture. `gb_stop()` therefore
records `s_last_stop` / `s_last_stop_ms`, and the shell prints them from
`gb stop` and `gb status`.

### The defect it exposed

`gb_blit_task` never cleared `s_blit_task`, so `gb_stop()`'s "wait for the
blitter to leave" loop could never observe the exit and always ran its full
100 × 10 ms. **Fixed** by clearing the handle before `vTaskDelete(NULL)`.
Stop time 1020 ms → 29–40 ms. If you touch that wait again, confirm the loops
still terminate on the handle and not on the timeout.

## THE STALL — parked, with the evidence (entry 89)

Roughly 1 session in 4–10, blits stop mid-session while the guest keeps running
(`frames=2602 blits=168`; also `2414 of 2598`). The stop then waits its full
second and reports `blit=1000ms`.

**Eliminated by measurement, do not re-litigate without new evidence:**

- **The panel bus.** Phase markers (`BUS_PH_*` in `display.c`) read **DONE** at
  every wedge — the last push completed normally.
- **The status strip's minute tick.** It was 17.7 s clear of one wedge. (This
  was the operator's hypothesis; it was tested, and it does not fit.)
- **`display_picture_rows()` blocking.** It returns early when pixels are
  unclaimed.
- **A bounded bus acquire as a mitigation.** `spi_device_acquire_bus()` in
  ESP-IDF 5.3.1 requires `portMAX_DELAY`; a finite wait trips
  `SPI_CHECK` and returns `ESP_ERR_INVALID_ARG` without touching the lock. A
  100 ms timeout took the miss count 29 → 43,680 in one build. See **LCD-014**.

**What a wedge shows:** task state `eBlocked`, phase `DONE`, pool
`free=0 filled=0`. `GB_VBUF_N` is **2** and a starved guest publishes nothing,
so one lost buffer can stop the picture for the rest of the session. That is a
lead, not a diagnosis.

**First thing to check next:** whether a buffer can fail to return to
`s_free_q` when `s_filled_q` is full (`s_dropped` counts it but does not fix it).
`gb status` prints `pool: free= filled= starved=` for exactly this comparison.

**The next wedge writes `/sd/diag/gbwedge.txt` by itself** — task state, push
phase and age, ms since last blit, pool occupancy, ms since the strip's push.
Read it after the session; do not poll for it.

**Teardown is hardened:** if the blitter will not exit, the frame buffers and
queues are deliberately **leaked, not freed**, `gb status` says so, and
`gb_start()` refuses until a reboot. Do not "fix" that leak without solving the
exit first.

## Build, flash, relink

```sh
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=$(ls -d /home/vash/esp/idf-tools/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin|head -1):$IDF_PYTHON_ENV_PATH/bin:$PATH
python "$IDF_PATH/tools/idf.py" build
```

Port: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00`
(watch is `303a:1001`; the TinyTang also enumerates as ttyACM — never open by number).

**Expect a relink whenever the image size changes.** `read_modaddr.py` needs
`IDF_PYTHON_ENV_PATH` exported or it fails with "No module named esptool".

```sh
cd ports/twatchultra/modules
python3 read_modaddr.py --port <by-id> -o moddata.at   # ~2 min
make flac                                              # prints host=0x...
esptool write_flash 0xC00000 build/flac.bin 0xE00000 build/flac.data.bin
```

Skip the board read when the size is unchanged: compare
`xtensa-esp32s3-elf-nm build/tinydesk.elf` `g_modhost_ptr` / `g_modbss` against
`modules/build/modaddr.h`. That check was correct twice this session.

## Reading the watch from the host

```sh
tools/watch-screen.py                    # repaint and print
tools/watch-screen.py 'gb status\r' 3    # send keys, then read
```

- Only forces a repaint when **no** keys are sent; a capture that passes keys
  returns whatever was on the window already.
- A capture killed mid-flight leaves escape residue — send `\x03`, re-read.
- Scrollback does not survive a repaint; a long output gets truncated.
- **A blocking serial read hangs for ever once a picture owns the panel.**

## Backup

`~/twatch-backup/working-2026-11-21-1100-pusherA.bin`, 16,777,216 bytes,
sha256 `9fe4f9574c2c7d37e96e172064081cea689911c779a8f9cdb16557197797c18e`.
Verified against the chip; `ota_0` byte-identical to `build/tinydesk.bin`
(3,097,360 B); module `host=0x3c280b28`. Supersedes
`working-2026-11-20-2241-jtables.bin`.

## Next steps, in order

1. **Catch the stall** by playing normally; read `/sd/diag/gbwedge.txt`.
2. **The startup audio burst** (~5,100 samples dropped during cartridge load).
   Shape of the fix: open the audio stream *after* the ROM loads.
3. **Extend Pusher A to NES and Pokemon Mini** — two lines each, once GBC is
   confirmed (it is) and their save paths are checked.
4. **Always-on gesture detection** — needs an upstream lock in `src/wm.c`.
5. **`klio save` overflow**, the learn pass's silent 60 s ceiling, >1 gesture.

## Open / unsolved

- The blitter stall (above).
- Savestate over-read in `gnuboy.c`: a 16 KB buffer passed with a count of 4
  (a 32 KB walk). Nothing calls it; fix when the core is next touched.
- Injection parked at entry 78. Store is one file, not per-user.
- DMG compatibility unproven — no DMG cartridge has been launched.
- Two unidentified firmware header fields at 0x2c and 0x44 gate custom firmware.
- Conformance debt: entries 66–73 use fenced code blocks in `Outcome`; recorded
  in entry 74, deliberately unfixed by operator decision.

## Standing facts

- Internal RAM is the binding constraint; the optimisation line is closed
  (entry 88). Three locality changes, three answers: jump tables +7% (kept),
  interpreter→IRAM no gain (reverted), VRAM→internal crashes (reverted).
- I2S DMA and the display's SPI bus share one internal pool (AUD-003).
- App at `0x20000`, slot `0x3E0000`, 3,097,360 bytes, ~966 KB free.
- **Back up the working tree before destructive experiments** — a mutation
  sweep once reverted uncommitted work via `git checkout`.
