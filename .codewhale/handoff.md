# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-15T01:05

Orientation, not authority. **Verify before acting on it.**

Session ended here: operator signing off for the night, **watch disconnected from USB**.
The status strip is shipped and accepted; the operator said they will test it further tonight.

## One line

**A permanent clock now lives in the 44 glass rows above the framebuffer, where no
emulator can cover it. Getting it there exposed a stack overflow that had been rebooting
the watch every 33 seconds while looking exactly like a display flash.**

## Repositories

- **Fork** `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD **`11025d6`** — clean, pushed.
  `fork` = `aquasock/tinydesk` (push here). `origin` = upstream `tinydesk-project/tinydesk`
  — **never push**.
- **Escapement** `~/GIT/Escapement`, branch `main`, HEAD **`b2f40ce`** — clean, pushed.
  Log entry 10 is current; 10 of 100 entries used.

## Permissions and limits

- Push only to `aquasock` repos. Never to `tinydesk-project/tinydesk`.
- `.ai/core.md` is RESTRICTED: read it, never edit it.
- **A `core-syntax.md` audit is owed after any `.ai/` change** — one ran at handoff: 10 entries, 0 problems.
- `board.conf` is gitignored. Watch password at `~/Desktop/passwd.txt` — **do not echo**.
- `archived_logs/*.tar.gz` not to be referenced without explicit approval.
- No sub-agents, no goals (project law in `.ai/core.md`).

## Current firmware

`sha256 a43a6a96d898ab17`, 0x2cad80 bytes, 7% partition free, **console OFF**
(`CONFIG_ESP_CONSOLE_NONE=y` restored after the diagnostic build).

Archived as `~/twatch-backup/working-2026-11-15-0057.bin`, 16,777,216 bytes,
sha256 `7399c009036422cd5d0e5126f6fc1be101212ec9614696bce0d521e8099a8c15`,
`verify OK (digest matched)`, `ota_0` byte-identical to the build.

## THE THING TO KNOW BEFORE CHASING A DISPLAY BUG

**A panic with the console off is indistinguishable from a display fault.** This
firmware builds with `CONFIG_ESP_CONSOLE_NONE=y` and
`CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y`, so a panic prints nowhere and reboots in
zero seconds. A crash looks like a flash. This cost most of a cycle.

To see a panic: set `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` and unset
`CONFIG_ESP_CONSOLE_NONE` in `ports/twatchultra/sdkconfig`, rebuild, flash, and read
the port. Four words ("stack overflow in task statusbar") ended a two-hour hunt.
**Restore both afterwards.**

Count boot bursts to detect a reboot loop from the host: a boot emits ~1.2 KB at once
and renders to nothing (raw boot log, not VT100). Sample `len(drain(...))` per second
and flag anything over 400 bytes.

## The status strip (`ports/twatchultra/main/statusbar.c`)

- **Rows 1..43.** Row 0 is swallowed by the controller (LCD-004); row 44 belongs to
  `paint_column()`, which uses it as a sacrificial row for the same reason.
- **HH:MM at scale 2** — 32 rows of glyph centred in 43, 80 px wide against 410 of glass,
  165 px margin each side. Total persistent cost: one 1,280-byte band buffer.
- **Stack 8192.** 3072 overflowed. The push reaches the SPI driver's polling transmit.
- **Pushes only when the minute index changes**, not when the string differs.
- **Timezone cached in the task**: 5 s retry while unknown, 60 s once known. Shows
  `--:--` rather than substituting UTC.
- **`display_panel_push()`** in `display.c` is a new export that **ignores
  `display_pixels_claim()`** — the first push path that does. Safe without a lock for
  three existing reasons: `push_pixels_raw_buf()` holds the bus across window and pixels,
  the caller brings its own buffer (no `s_tx_app` race with `gamepad_paint()`), and it
  touches no framebuffer or cell record. **A second user of it deserves the same reasoning
  written down.**
- **`statusbar` shell command** reports `text`, `utc`, `tz`, `home`, the tz file path, its
  push count and the driver's total. That command proved the timezone defect; keep it
  until the strip's content is settled.

## `get_tz()` — the defect that was fixed, and the one that was reverted

In `ports/esp_idf/app/main.c`. It **used to return `true` while handing back `0`**, so
"I could not find your time zone" and "your time zone is UTC" arrived as the same answer.
It now reports a failed read as a failure, and still fills `*seconds` so callers that
ignore the return value keep the last known offset.

**Do not add an immediate retry on failure.** A version that did turned one failed file
open into an open per tick forever, because the function sits on the taskbar's paint path.
The 30-second window is deliberate.

## The three lessons this session cost most

**1. Measure before editing shared code.** Three changes were made on a hypothesis and all
three were wrong or unnecessary: `paint_column`'s chrome index (fixed a real bug but the
first diagnosis was wrong), the `rot_k` comment, and `get_tz`'s retry. The two changes that
went well were both measured first — the arrow orientation and the glyph size were
confirmed by compiling the real font array and placement arithmetic on the host and
printing pixels before any flash.

**2. Build the instrument that can see the result.** The arrow bug took two rounds because
the first fix read `label[s].cp` before the call that fills it — uninitialized stack, build
clean, identical picture. A host simulation settled it in one run. Do that first.

**3. An absence in one instrument is not evidence.** A cross-emulator smoke test in this
session filtered output without clearing the screen and matched a stale status line, so it
printed identical figures for five different commands and verified nothing. Clear state
before every probe.

## Game Boy port — where it stands

Runs Link's Awakening DX at **59.5 fps, 0.9969× real time, zero audio drops**. Battery
saves work and survive `gb stop` / `gb start`. Drawn d-pad (upright, radius 94) and A/B
circles. `gb` / `gb start [rom]` / `gb stop` / `gb save` / `gb rsave` / `gb status`.

**Unproven:** no DMG cartridge has ever been launched, so the `GB_HW_CGB` compatibility
path is untested. A save has not survived a power cut — only `gb stop`/`gb start`.

## Owed / next

- **Extend the strip**: network state from `td_sysinfo()->net`, battery level. Each is a
  text field in the same band; the layout is the work, not the plumbing.
- **Pause, Reset and Save** are labelled on all three emulators and send nothing.
- **The right column's third slot** is drawn furniture on the GB port; it could carry `gb save`.
- **BLE controller** must be reconnected after every reboot.
- **`pm.c`** still duplicates the rasteriser `gamepad.c` owns.
- **NES**: parked undecided, owes battery-SRAM and a skip-path test,
  `docs/nes-accuracy.md` is the baseline, page 15 crash is the first thing to chase.

## Reference

**Ports:** watch `/dev/ttyACM0`; a TinyTang board sits on `/dev/ttyACM1` — do not confuse them.

**Build and flash**
```sh
cd /run/media/vash/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=$HOME/esp/esp-idf-5.3.1 IDF_TOOLS_PATH=$HOME/esp/idf-tools
. $IDF_PATH/export.sh >/dev/null 2>&1
idf.py build
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before default_reset --after no_reset write_flash --flash_mode dio \
  --flash_size 16MB --flash_freq 80m 0x10000 build/tinydesk.bin
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before default_reset --after hard_reset read_mac
```

**Backup:** `./scripts/watch-backup.sh` from Escapement. One attempt in this session read to
16% and failed with `Corrupt data` — the script correctly refused it and left no file. The
retry after a reboot succeeded. **Check for competing access to `/dev/ttyACM0` if a read
fails**, and reboot the watch before retrying.

**Key paths**
- `/root/.tdsh_tz` holds the UTC offset in seconds (`-25200` = UTC-7). `/fs/root` is the
  same place seen through the VFS root.
- `/root` is littlefs in flash; `/sd` is the card.
- `tdsh` has no `wc`, `sed`, `head`, `grep`. Terminal wraps at 40 columns — read the whole screen.
- Start menu opens with `\x1b[21~` (F10); Terminal is first. **The selection persists across
  opens**, so navigate deliberately rather than assuming the top item.
- `/tmp/watch-screen.py` — the VT100 parser used for console interaction.

**Not committed:** nothing. Both trees clean.
