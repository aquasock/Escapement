# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-15T09:20

Orientation, not authority. **Verify before acting on it.**

Session ended here: operator's verdict on the GPS app was "works great! ui looks good. pass.",
watch still connected with the app open. Log entry 12 is committed and pushed.

## One line

**There is a GPS app: fix state, position, satellite counts, HDOP, altitude and UTC, with
the receiver powered only while its window is open. It runs at a real fix — 18 in view, 12
used, HDOP 0.95 — and the number that makes it useful is satellites IN VIEW, because
satellites USED is zero both without a fix and without an antenna.**

## Repositories

- **Fork** `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD **`7dce0b6`**
  — clean, pushed. `fork` = `aquasock/tinydesk` (push here). `origin` = upstream
  `tinydesk-project/tinydesk` — **never push**.
- **Escapement** `/run/media/vash/GIT/Escapement`, branch `main`, HEAD **`055a1c8`** — clean,
  pushed. Log entry 12 is current; 12 of 100 entries used.

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

`sha256 1a1ade9b2fd84f63`, `0x2cc090` bytes, 7% partition free, **console OFF**
(`CONFIG_ESP_CONSOLE_NONE=y`).

Archived as `~/twatch-backup/working-2026-11-15-0912.bin`, 16,777,216 bytes,
sha256 `910ccd311147b7f37750e5fce3350e60a266de4481aaf2ce179234786dd2b534`,
`verify OK (digest matched)`, `ota_0` byte-identical to the build.

Older images are retained and are safe rollback targets: `working-2026-11-15-0808.bin`
(the status strip with the date) and `working-2026-11-15-0759.bin` (the strip as tested
before a comment fix; differs in 70 bytes, none of them code).

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
`display pushes` incremented. The GPS app is the opposite case and a reminder that this is
per-feature: it draws into the framebuffer, so the mirror reads it perfectly (that is how
`Fix: GPS / 33.29387 N / 18 in view, 12 used / HDOP 0.95` was confirmed) — but only while
no emulator owns the screen.

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

## The GPS app (`apps/gps.c` + the port's `gps.c`)

- **In the start menu as `GPS`**, between Bluetooth and Store. Tap it; no shell needed.
- **Reaches the receiver through `td_gps_ops_t`** in `td_sysinfo.h`, not the port header:
  `apps/` is shared by every board. `start()` / `stop()` / `status()`.
- **The receiver is powered only while the window is open** — operator's decision. `start()`
  on open raises the rail, `stop()` on close drops it. The window is therefore empty for
  the first second and a cold fix takes minutes; satellites in view appear first.
- **`Satellites: N in view, M used`** is the line that matters. GGA's field counts satellites
  USED, which is **zero** both without a fix and without an antenna -- so a healthy receiver
  indoors reads as broken if that is all you show. GSV carries IN VIEW. See GPS-006.
- **The sky figure is a SUM across constellations** with a staleness rule (a talker not heard
  in 400 sentences is dropped). `gps raw` reports the **MAX** single constellation, so the
  shell and the app disagree **by design** — 3 vs 18 is not a bug.
- **The receiver is exclusive.** The shell command and the app cannot both parse one UART;
  each refuses while the other holds it.
- **Position crosses the seam as `int32` in 1e-7 degrees**, not a double. `td_sysinfo.h` has
  no floating point anywhere and this keeps it that way.
- **Offered next:** a `gps time` button. The path exists and is proven to survive a hard
  reset through the RTC (GPS-005); it is only unwired to the UI.

## The status strip (`ports/twatchultra/main/statusbar.c`)

- **Shows `15-11   8:07 AM`** — date then clock, both at scale 2, 224 px centred on x=200
  (the *chamfered* centre; the glass centre is 205). Rows 6..37.
- **Both fields come from Settings** via `td_clock_prefs()`: date order renders `15-11`,
  `11-15` or `11/15`; the 12-hour form is padded `%2d` to a fixed 8 cells.
- **Rows 1..43.** Row 0 is swallowed (LCD-004); row 44 belongs to `paint_column()`.
- **Stack 8192.** 3072 overflowed; the push reaches the SPI driver's polling transmit.
- **Pushes on the minute index**, and on a text change (settings edit, midnight).
- **The line is pushed as ONE rectangle** covering the union of where it is and where it
  was, so a field that moved is erased. See the stale-pixel defect below.
- **`statusbar` command** reports `date`, `clock`, `utc`, `tz`, `have`, `fmt`, `12h`, push
  count, `left=`, `painted x a..b (w=N)`, `home`, tz file path. Keep it.

### The two defects that cycle cost, both still instructive

**A double-applied scale.** The font cell was defined as `8 * SB_SCALE` and each width
multiplied by scale again, so every width doubled, the push was refused against the buffer
limit, and **the strip silently never drew** — build clean, flash verified. `statusbar`
showed `left=-16`, `pushes=0`. Constants are now `SB_FONT_W/H` (unscaled) and
`SB_GLYPH_W/H` (derived). Never fold a scale into a cell constant and apply it again.

**Stale placeholder pixels.** Reported as `15-11--`, and it was **not** the formatter: the
strip centres as a group, so a field's x depends on the other field's width. The placeholder
clock `--:--` is 5 cells against the real clock's 8, so resolving the timezone moved the date
24 px and the old dashes sat where nothing overwrote them (28 stale pixels, reproduced on the
host). Fixed by the union push.

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

- **A `gps time` button on the GPS app.** The path exists, is proven, and survives a hard
  reset through the RTC (GPS-005); it is only unwired to the UI. Small, and it would give the
  watch a clock source that needs no network.
- **The receiver's own configuration is untouched.** GPS-003: this unit answers NMEA but not
  the UBX poll, so constellation set, sentence rates and power modes are factory default and
  cannot be changed from the watch. Anything wanting those has to establish the UBX side
  first — that is the next real question about this part, not a UI task.
- **Battery and the year on the strip are owed, and the arithmetic is the reason they are
  absent.** The 446 px measurement above is what any future attempt has to answer: both need
  shrinking the clock, a second row, or the mixed-scale layout the operator rejected.
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

**DO NOT SCRIPT START-MENU NAVIGATION. ASK THE OPERATOR TO TAP.** This session burned three
rewrites of a colour-tracking screen model trying to open an app from the host, and never got
it: the desktop sends only CHANGED cells, so an accumulated model drifts until the selected
row reads as a blend of two entries. The operator opened the app with one tap in one second.
The same trap bit earlier when a bare Enter to the Start menu launched Tamagotchi — the menu
**remembers its selection across opens**, so Enter does not mean "the top item". Type into a
shell that already exists; do not press Enter on a menu you have not read.

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
