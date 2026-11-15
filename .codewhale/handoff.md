# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-15T08:20

Orientation, not authority. **Verify before acting on it.**

Session ended here: operator's verdict on the strip was "looks great! pass.", watch still
connected. Log entry 11 is committed and pushed.

## One line

**The strip now shows the date beside the clock at one scale, and getting there proved the
glass above the framebuffer is only 261 px wide — not the 410 the code had assumed.**

## Repositories

- **Fork** `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD **`20f1c08`**
  — clean, pushed. `fork` = `aquasock/tinydesk` (push here). `origin` = upstream
  `tinydesk-project/tinydesk` — **never push**.
- **Escapement** `/run/media/vash/GIT/Escapement`, branch `main`, HEAD **`517cba6`** — clean,
  pushed. Log entry 11 is current; 11 of 100 entries used.

**The handoff's old `~/GIT/...` paths are wrong** — `HOME=/home/vash` has no `GIT`
directory; both checkouts live under `/run/media/vash/GIT/`.

## Permissions and limits

- Push only to `aquasock` repos. Never to `tinydesk-project/tinydesk`.
- `.ai/core.md` is RESTRICTED: read it, never edit it.
- **A `core-syntax.md` audit is owed after any `.ai/` change** — one ran for entry 11: 11
  entries, 0 problems, `.ai/core.md` confirmed unchanged.
- `board.conf` is gitignored. Watch password at `~/Desktop/passwd.txt` — **do not echo**.
- `archived_logs/*.tar.gz` not to be referenced without explicit approval.
- No sub-agents, no goals (project law in `.ai/core.md`).
- `.codewhale/handoff.md` is tracked deliberately (see `.gitignore` line 36) and the
  previous session ended with a `handoff:` commit. Update and commit it.

## Current firmware

`sha256 dbacc50a7d14d4ec`, `0x2cb410` bytes, 7% partition free, **console OFF**
(`CONFIG_ESP_CONSOLE_NONE=y`).

Archived as `~/twatch-backup/working-2026-11-15-0808.bin`, 16,777,216 bytes,
sha256 `8d9766bc341a40b5d3c6196ac09d308737404c19b77c36cbcf327485b96bc12d`,
`verify OK (digest matched)`, `ota_0` byte-identical to the build.

The image from the build the operator actually tested is also retained:
`working-2026-11-15-0759.bin`, sha256 `c63393d9101c4889…`. It differs from the current one
in 70 bytes — the app descriptor's build stamp and the trailing image SHA-256, no code —
so either is a safe rollback target.

## TWO THINGS TO KNOW BEFORE CHASING A VISUAL BUG

**1. A panic with the console off is indistinguishable from a display fault.** This firmware
builds with `CONFIG_ESP_CONSOLE_NONE=y` and `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y`, so a
panic prints nowhere and reboots in zero seconds. A crash looks like a flash. To see one:
set `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, unset `CONFIG_ESP_CONSOLE_NONE`, rebuild,
flash, read the port. Four words ("stack overflow in task statusbar") ended a two-hour hunt
in the previous session. **Restore both afterwards.**

**2. The bezel is invisible over USB.** The strip is drawn outside the framebuffer, so
`tools/watch-screen.py` will never show it and the USB mirror cannot prove anything about
it. **The `statusbar` shell command is the only instrument** — it has now caught two
defects the build and the flash both passed. Do not conclude the strip works because
`display pushes` incremented.

## THE MEASUREMENT THAT SETS THE WIDTH BUDGET

The glass is **not 410 px wide above the framebuffer.** `docs/panel-geometry.md` measures
the corners as straight 45-degree chamfers:

```
top-left     x + y         = 80
top-right   (410 - x) + y  = 90
bottom-left  x + (502 - y) = 82
```

At row 10, where a scale-2 glyph block starting at row 6 first puts ink down, the usable
span is **x 70..330 = 261 px inclusive**. By row 45 the cuts stop binding (331 px). Anything
drawn in rows 0..44 — only the status strip does — must be sized to the chamfer at its own
row. This is now recorded in **LCD-007**.

Measured on the host, with the real font array and the real placement arithmetic:

- 18-cell 288 px line at scale 2 ……… **61 ink px clipped**
- full date (10 cells) + clock + battery at scale 2 … **446 ink px clipped**
- 16-cell 256 px line ………………… clears by only **4 px** at its worst
- the shipped 224 px line ………………… clears by **21 px**

**Do the host harness first.** `/tmp/sb_sim.c`-style: compile `font8x16.c` and the
placement arithmetic, count ink against the three chamfer lines, emit a PPM/PNG. It settled
every layout question this session in one run and cost no flashes.

## The status strip (`ports/twatchultra/main/statusbar.c`)

- **Shows `15-11   8:07 AM`** — date, then clock. 224 px, centred on x=200 (the *chamfered*
  centre; the glass centre is 205). Rows 6..37.
- **Both fields come from Settings** via `td_clock_prefs()`: date order renders `15-11`,
  `11-15` or `11/15`; 12-hour form is padded with `%2d` to a fixed 8 cells so an unpadded
  hour cannot shift the date 16 px.
- **Rows 1..43.** Row 0 is swallowed (LCD-004); row 44 belongs to `paint_column()`, which
  uses it as a sacrificial row for the same reason.
- **Stack 8192.** 3072 overflowed; the push reaches the SPI driver's polling transmit.
- **Pushes on the minute index**, and on a text change (settings edit, midnight, battery).
- **Timezone cached in the task**: 5 s retry while unknown, 60 s once known.
- **The line is pushed as ONE rectangle** covering the union of where it is and where it
  was — see the stale-pixel defect below. Each band is one window and one pixel stream.
- **`statusbar` command** reports `date`, `clock`, `utc`, `tz`, `have`, `fmt`, `12h`, push
  count, `left=`, `painted x a..b (w=N)`, `home`, and the tz file path. Keep it.

### The two defects this cycle, both worth remembering

**A double-applied scale.** The font cell was defined as `8 * SB_SCALE` and each width was
then multiplied by scale again, so every width doubled, the push was refused against the
buffer limit, and **the strip silently never drew** — build clean, flash verified,
`Hash of data verified`. `statusbar` showed `left=-16`, `pushes=0`. Constants are now
`SB_FONT_W/H` (unscaled 8x16) and `SB_GLYPH_W/H` (derived). Never fold a scale into a cell
constant and apply it again.

**Stale placeholder pixels.** Reported as `15-11--`. It was **not** the formatter: the strip
is centred as a group, so a field's x depends on the other field's width. The placeholder
clock `--:--` is 5 cells against the real clock's 8, so resolving the timezone moved the
date 24 px and the old dashes sat where nothing overwrote them (28 stale pixels at
x 168..183, reproduced on the host). Fixed by the union push.

## `get_tz()` — fixed, reverted, and one wart left unfixed

In `ports/esp_idf/app/main.c`. It **used to return `true` while handing back `0`**, so
"I could not find your time zone" and "your time zone is UTC" arrived as the same answer. It
now reports a failed read as a failure and still fills `*seconds`.

**Do not add an immediate retry on failure.** A version that did turned one failed file open
into an open per tick forever, because the function sits on the taskbar's paint path.

**Still unfixed:** the strip shows `--/--  --:--` for up to **30 s after a reboot**, because
`get_tz()` caches the pre-login failure for its whole window. The cure has to be a first read
that does not depend on `td_session_home()` being populated — not a retry.

## Owed / next

- **Battery and the year are owed, and the arithmetic is the reason they are absent.** The
  446 px measurement above is what any future attempt has to answer: both need shrinking the
  clock, a second row, or the mixed-scale layout the operator rejected on the glass.
- **The 12-hour clock's `%2d` padding** keeps geometry still but widens the gap at
  single-digit hours. Uncomplained-of; centring inside its fixed box is the change if wanted.
- **Pause, Reset and Save** are labelled on all three emulators and send nothing.
- **The right column's third slot** is drawn furniture on the GB port; it could carry `gb save`.
- **BLE controller** must be reconnected after every reboot.
- **`pm.c`** still duplicates the rasteriser `gamepad.c` owns.
- **NES**: parked undecided, owes battery-SRAM and a skip-path test,
  `docs/nes-accuracy.md` is the baseline, page 15 crash is the first thing to chase.
- **No DMG cartridge** has ever launched, so `GB_HW_CGB`'s compatibility path is unproven.

## Reference

**Ports — PIN BY USB IDENTITY, NOT BY INDEX.** During this session a TinyTang board took
`ttyACM0` and the watch landed on `ttyACM1`, then a SIPEED debugger appeared as
`ttyUSB0/1`. Indices moved more than once.

```sh
ls -l /dev/serial/by-id/            # watch: usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00
WATCH=/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00
```

**A second agent on this machine ran a loop that globbed and opened every `/dev/ttyACM*`**,
which killed two flashes mid-write (`device reports readiness to read but returned no data`)
and leaves a partial image. The operator told it to back off. **Check for competing opens
before flashing:**

```sh
for d in /dev/ttyACM*; do echo -n "$d: "; lsof $d 2>/dev/null | awk 'NR>1{print $1}' | tr '\n' ' '; echo; done
```

**Build and flash** (460800 failed twice; 230400 has been reliable)

```sh
cd /run/media/vash/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=$HOME/esp/esp-idf-5.3.1 IDF_TOOLS_PATH=$HOME/esp/idf-tools
. $IDF_PATH/export.sh >/dev/null 2>&1
idf.py build
python -m esptool --chip esp32s3 -p "$WATCH" -b 230400 \
  --before default_reset --after hard_reset write_flash --flash_mode dio \
  --flash_size 16MB --flash_freq 80m 0x10000 build/tinydesk.bin
```

**Backup:** `./scripts/watch-backup.sh` from Escapement. It auto-detects the watch by
`/dev/serial/by-id/usb-Espressif_*`, takes ~4 min, holds the board in the bootloader across
read and verify, and refuses to call a short read a backup. Writes
`~/twatch-backup/working-DATE-HHMM.bin` plus a `.sha256` sidecar — **check the sidecar from
its own directory** (`sha256sum -c` uses the relative filename inside it).

**Console reads:** `tools/watch-screen.py` defaults to `/dev/ttyACM0`. Override the default:

```python
m.open_port.__defaults__ = ('<resolved port>',)
```

**Key paths**
- `/root/.tdsh_tz` holds the UTC offset in seconds (`-25200` = UTC-7). `/fs/root` is the
  same place seen through the VFS root.
- `/root` is littlefs in flash; `/sd` is the card.
- `tdsh` has no `wc`, `sed`, `head`, `grep`. Terminal wraps at 40 columns — read the whole screen.
- Start menu opens with `\x1b[21~` (F10); Terminal is first. **The selection persists across
  opens**, so navigate deliberately rather than assuming the top item.
- System Monitor shows internal SRAM free/peak; this session it read 88 KB free, 71 KB peak,
  against the 65,536-byte floor the SSH server needs.

**Not committed:** nothing. Both trees clean, both remotes match.
