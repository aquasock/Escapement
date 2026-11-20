# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-20T12:35

Orientation, not authority. **Verify before acting on it.** The files, the flash
and the board are the fact; this file is a claim.

## One line

**Learnt gestures now survive a reboot, proven in both directions on the board.**
The AI-engine/injection line is **parked by operator decision** — do not resume
it without being asked. The work now is a **user-facing gesture feature**: a
button that starts learning, and a percent readout.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `baeb590`
  (core-log **entry 79**, reference **IMU-011**). Pushed, clean.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, at `72a0e7b`, pushed to `fork`, clean.** Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream, read-only).
- `.ai/core.md` is RESTRICTED. Do not edit without an explicit instruction.

## What entry 79 built (all verified on hardware)

- `ports/twatchultra/main/klio_store.c/.h` — versioned, CRC-32'd serialiser for
  the engine's 157-byte blobs. **Pure C, no ESP-IDF, no Bosch** so it is
  host-testable. Up to 32 slots, 16-byte reserved name field per record.
- Store file: `/fs/root/klio_patterns.bin` (littlefs). Temp-file + `rename()`.
- `klio_after_boot()` — declared in `imu.h`, **weak default in `imu.c`**, called
  from `imu_start()` right after the firmware boots. Restores every stored blob
  and enables it. Fully best-effort: a bad store logs and the sensor still works.
- Commands: `klio save <slot>` (now writes the file), `klio restore` (reloads
  file), `klio forget <slot>`, `klio stored`, **`klio recognise [secs]`**.
- `tests/test_klio_store.c` — 647 checks, plus `test_klio_store_asan`. Host
  suite is **46/46**.

## THE FACT THAT SHAPES THE UI (IMU-011)

`read_pattern` reads the **LEARN slot**. Once a pattern is written into the
**recognition table**, that slot is consumed and `read_pattern` returns `-2`
with driver status 3. So:

- **`klio detect` cannot test a saved pattern** — that is why `klio recognise`
  exists. Use `klio recognise` for anything persistence-related.
- **`klio learn` sets `learning_reset`, which wipes every restored pattern.**
  The Learn button must therefore save on completion and **reload the whole
  store** afterwards. This is not optional.

## Proven evidence (reproduce before trusting)

```
learn     progress 20/40/60/80/100, engine assigned pattern 2
save      read 157 bytes, slot 0 enabled, 1 pattern stored
reboot    "pattern store: 1 pattern(s)"
          "slot 0 (157 B): write ok, enable ok"
          "restored 1 of 1 pattern(s) after boot"      <- no operator action
recognise pattern 0, count to 14.1, 115 frames, MATCH
forget 0  store empty; recognise -> "nothing is loaded to recognise"
reboot    no restore line at all
```

Note the store is **empty right now** because the negative control ran
`klio forget 0`. The first Learn press has to be done from scratch.

## HOST PORTS — has gone wrong three times

Watch and TinyTang both enumerate as `/dev/ttyACM` and **swap numbers**.

- Watch: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00`
- Tang: `ffff:5454`. Watch is `303a:1001`. Never open a ttyACM by number.
- **`tools/watch-screen.py` only forces a repaint when NO keys are sent.** A
  capture that passes keys returns whatever was already on screen — this
  produced three stale reads in one session. To read a command's output, send
  the command, then call again with no keys.
- Don't `nohup ... &` a capture; it died with empty output and nothing reached
  the board. Run it in the foreground.
- A capture killed mid-flight leaves escape residue. Send `\x03`, then re-read.
- `esptool --after no_reset` leaves the board in the bootloader and silences the
  console. Finish every read with `--after hard_reset chip_id`.

## OPENING A TERMINAL

Reboot → F10 → Enter → **verify `tdsh` before typing** (TOL-007). After using
other apps the Start menu remembers its selection and F10+Enter launches *that*
app. This cost a run in this session: keys went to the desktop and opened the
Start menu instead of a prompt.

## Build environment (does not come from `export.sh`)

```sh
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=$(ls -d /home/vash/esp/idf-tools/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin|head -1):$IDF_PYTHON_ENV_PATH/bin:$PATH
python "$IDF_PATH/tools/idf.py" build
```

There is a **second** toolchain at `~/.espressif` — using it gives "configured
with a different python env". Use `/home/vash/esp/idf-tools`.

## Module relink — the trap that bit this session

App grew 3,084,240 → 3,090,000. The mmap pair (`inst=0x42ae0000`,
`data=0x3cce0000`) did **not** move, but the **API table did**:
linked `0x3c27fb44` vs live `0x3c280418`. With the stale blob `music` printed
"playing" and played nothing. After `make flac` (needs `IDF_PATH` set, it uses
`$(IDF_PATH)/components/xtensa/esp32s3/include`) the header reported
`host=0x3c280418` and audio worked: 192,000 frames, 0 errors, 0 short writes.

```sh
cd ports/twatchultra/modules
python3 read_modaddr.py --port <by-id> -o moddata.at   # ~2 min, reads storage
make flac                                              # needs IDF_PATH
esptool write_flash 0xC00000 build/flac.bin 0xE00000 build/flac.data.bin
```

**Always check the `host=` field in the patch_header output against the board's
live value.** Matching inst/data is not enough.

## Next steps, in order

1. **Build the gesture UI in `apps/imu.c`.** Learn button, percent readout,
   Stop. Window is 34x14 now; ~34x19 fits 26 rows. `td_button`, `td_progress`
   and `td_progress_set` all already exist and are used by sysmon/GPS/bluetooth.
2. **Make the learning state machine non-blocking in `klio.c`.** `klio learn`
   is a blocking shell loop; a window tick cannot call it. Extract
   begin/tick/end and **refactor `klio learn` to call them**, so the button and
   the command cannot drift.
3. Extend `td_imu_ops_t` in `include/tinydesk/td_sysinfo.h` (additive;
   `ports/esp_idf` returns NULL so other ports are unaffected).
4. Progress must be **honest**: it resets to 0 when the engine judges the next
   repetition unlike the last. `change_reason` 1 = interrupted by
   non-repetitive activity, 2 = no significant movement. Say so, or the bar
   reads as a bug.

## Open / unsolved

- `klio save` still emits `meta 12 fifo overflow` while reading the pattern. The
  stored blob demonstrably matched after reboot, but the pattern read *right
  after* that save returned `-2`. Whether the save's read is reliable under
  overflow is **not settled**.
- Store is one file, not per-user. No migration path if the format version
  changes (it refuses rather than migrates).
- Nothing yet acts on a recognised pattern.
- Injection line parked at entry 78: step-by-step vs normal mode untested.
- Two unidentified firmware header fields at 0x2c and 0x44 still gate custom
  firmware.
- Conformance debt: entries 66–73 use fenced code blocks in `Outcome`, which
  `core-syntax.md` forbids. Recorded in entry 74, deliberately unfixed.

## Standing facts

- Similarity metric from real motion: 1.000 / 0.795 / 0.475 / negative. Graded,
  reproducible, not touched by any of this.
- App at `0x20000`, slot `0x3E0000`, ~974 KB free.
- Internal DRAM is scarce: a few KB of `.bss` has starved a peripheral (EMU-018).
  The pattern store is a PSRAM heap allocation for this reason.
