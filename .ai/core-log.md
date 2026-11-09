## 1 COMMIT Unreleased 2026-11-09T10:31:52-07:00

#### Coming From:

Unreleased fd5f252

#### Purpose:

Record the state of the project at a session handoff, so a fresh agent can continue from evidence rather than reconstruct the bring-up.

#### Outcome:

The watch is fully up on real Linux 7.2.4 and every subsystem brought up so far is verified on hardware, not inferred: the kernel boots to userspace, logs in and forks; the AMOLED renders a 40x25 text console inside a 320x400 framebuffer with a black border and a 1 px red outline painted by the driver; the AXP2101 publishes both `axp20x-battery` (100 percent, 4175000 uV, Full) and `axp20x-usb` (online 1) with four registered interrupts; the PCF85063A seeds the system clock at boot; a Logitech BLE keyboard types into a login prompt on the watch face; and WiFi carries real traffic at about 409 KB/s with DNS resolving. Userspace can now draw on the panel, which required adding `fb_write` to the CO5300 driver, and an external SPLB32 Tamagotchi emulator has been measured running on the glass as an independent check of that path. Two facts about the current deployment matter most to whoever picks this up. First, the repository is clean and fully pushed at `fd5f252` on `main`, but the firmware lives in a separate git tree at `build/esp-hosted/esp_hosted_ng` on branch `ble-bond-wip` with local-only commits that must never be pushed, because its origin is the upstream `jcmvbkbc/esp-hosted` repository rather than this project; the running firmware is that branch's build, byte-identical to the one captured in the current backup. Second, that firmware has `sm_bonding` enabled while bond loading is broken, so the keyboard stores a key the watch cannot restore and must be put back into pairing mode after every reboot; this is a known and accepted defect, not a new fault, and the fix is either to set `sm_bonding = 0` (reverts to silent re-pairing on each connect) or to finish the load path. A full 16 MiB flash backup exists at `/home/vash/twatch-backup/working-2026-11-09-1017.bin` (sha256 `639da94c...`), verified to contain the running kernel and firmware byte-for-byte, alongside the untouched factory image; an older `working-2026-11-09.bin` predates `fb_write` and should not be restored. The previous cycle's own result is folded in here: `.ai/core-reference.md` now holds 35 records across twelve topics covering everything this project had to establish about the SoC, board, panel, PMU, kernel constraints, flash layout, jffs2, BLE, networking and toolchain, including the negative results that each cost a session and would otherwise be re-litigated. No build, deployment or hardware test applies to this cycle, which changed documentation only.

#### Next Steps:

Refresh the live state before acting on any of this, since the handoff is orientation and not law: confirm the watch answers on 10.10.0.15 with `./scripts/watch-telnet.py 'uptime'`, and take a fresh full flash backup with `./scripts/watch-backup.sh` if the kernel or firmware is to change, because a kernel-only backup does not protect against the `/etc` damage that made a previously saved kernel appear broken. The next engineering work is touch: the CST9217 at I2C 0x1A is fitted and identified but has no driver at all, so the watch currently has no local pointer or tap input, and it needs `CONFIG_INPUT_TOUCHSCREEN`, a driver, and the XL9555 reset and interrupt lines before it can be verified. After that, console repaint speed is the next largest gap, and it now has a concrete lead because the full-width band transfer shape the controller actually accepts was established while drawing the border. The BLE media keys, firmware-side bond persistence, and any wake-from-sleep path remain deferred by user decision and should not be started without asking.

#### Files Modified:

None.

#### Status:

- Build: NOT RUN
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 2 COMMIT Unreleased 2026-11-09T11:25:20-07:00

#### Coming From:

Unreleased 2118b3c

#### Purpose:

Fix the watch's real time: the console displayed UTC because the image carries no timezone data, and the documented shutdown write-back to the PCF85063A did not exist on the board.

#### Outcome:

The cycle began by testing the standing claim that the RTC held UTC+8 and needed a correction, and found it false: the chip held true UTC to within two seconds of the host clock, so the factory correction had been applied and stuck, and the README bullet asserting otherwise was stale. The visible defect was a different one -- the image ships no `/etc/TZ`, no `/etc/localtime` and no `/usr/share/zoneinfo`, so `date` printed `17:46 UTC +0000` while the clock was right and the console was seven hours off the wall. Writing the POSIX string `MST7` into `/etc/TZ` fixed it, confirmed on hardware with `TZ` unset: `2026-11-09T10:47:07 MST -0700` against a true UTC of 17:47:07. `MST7` rather than `MST7MDT` because America/Phoenix has no DST. A second gap was that `/etc/init.d/S01hwclock`, documented as running at shutdown, was absent from the board; the cause is `/etc/init.d/S03keepconfig`, which carries exactly seven named files plus the dropbear keys across a full-image flash, so any other `/etc` addition is lost by one. A full 16 MiB backup was taken first as instructed (`working-2026-11-09-1053.bin`, sha256 `1e89a99b...`), which proved worth the three minutes because the read hard-reset the board mid-cycle. Both files are now repo artifacts installed by `scripts/watch-time.sh`, which base64s each over telnet or the USB console and checks the sha256 on the watch before an atomic rename, replacing `scripts/rtc-writeback.sh`, which built the init script from a heredoc and could not be re-run safely. The finding that changed the approved plan was `/etc/init.d/S01clock`, shipped in the upstream image: it writes the epoch to `/home/.clock` at shutdown and overwrites the system clock with that note at boot whenever the note is newer, which is after `hctosys` has seeded from the chip, so the note beat the chip. Measured rather than assumed -- a note set 90 minutes ahead moved the clock from 18:12:58 to 19:42:56 against a true 18:12:59 -- and the user chose to disable it, since `/sys/class/rtc/` contains only `rtc0` at 0x51 and if the chip dies the time should say so. Disabling it by renaming would have been a false fix: `rcS` and `rcK` both iterate `S??*`, which is S plus exactly two characters plus anything, so `S01clock.disabled` still matches and still runs; it is moved to `/etc/disabled/S01clock` instead, and `/home/.clock` is deleted. Verification on hardware: a skewed system clock written back at shutdown and adopted by the next boot's `hctosys`, then after the disable a shutdown-and-reboot cycle left `/home/.clock` absent -- `rcK` recreates it if the script is still in the glob -- with `uptime` 54 s and the seed at true UTC. The user confirmed the AMOLED console reads local time and that it survives reboots. Two documentation errors were corrected in the same commit: the stale UTC+8 bullet in README, and `docs/bring-up.md` stating that `date -s "@epoch"` does not work on this shell, which it does, established by setting the clock to 2001 with it. One anomaly could not be explained and is recorded as open rather than smoothed: in one reboot the system clock read true UTC although `hctosys` had seeded a value 300 s fast, and neither `S01clock` nor any other mechanism was found that steps time backwards, so the cause is unknown and bounded to that single boot. The core-syntax audit passed: the `.ai/` diff was inspected, `core.md` was not modified, entry 2 was validated against the template with six sections in order, sequential numbering from 1, ISO-8601 timestamp with the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

Nothing remains on this cycle. The next engineering work is still touch input, which is larger than entry 1 described: the CST9217 at I2C 0x1A has no device tree node, `CONFIG_INPUT_TOUCHSCREEN` is unset, no CST9217 driver exists in 7.2.4 (the tree's only Hynitron support matches `hynitron,cst340`), and there is no vendor register map in `build/` -- so it needs a sourced register map, a driver, and the XL9555 reset and interrupt lines before it can be verified. The console repaint work has a concrete lead in LCD-005. The missing contiguous-allocation anomaly above should be closed with a controlled stamp test if it is worth a cycle. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- userspace/TZ
- userspace/S01hwclock
- scripts/watch-time.sh
- scripts/rtc-writeback.sh
- README.md
- docs/bring-up.md

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 3 COMMIT Unreleased 2026-11-09T12:55:56-07:00

#### Coming From:

Unreleased f32fb12

#### Purpose:

Give the watch local touch input: three buttons painted in the strip below fb0, each reporting as an ordinary on/off key like the two side pushers.

#### Outcome:

The strip below the console is now red, green and blue thirds painted full width by `co5300_paint_frame()` inside its existing full-band loop, so each transfer keeps the shape the controller is known to honour, and each third reports as `BTN_0`, `BTN_1` and `BTN_2` on a uinput device named `touch-buttons`. Verified on hardware 2026-11-09: thirteen presses in the order green, red, blue, green decoded to the matching bands with a release after each, and the raw kernel stream read back from `/dev/input/event3` as `type=1 code=256 value=1`, `SYN_REPORT`, `value=0` for red, then 257 for green and 258 for blue. The input half is a userspace daemon, `userspace/tbtns.c`, not a kernel driver, and that was the deciding design call: the running kernel already provides `/dev/i2c-0`, `/dev/uinput` and the XL9555 as a gpiochip, so there is no `CONFIG_INPUT_TOUCHSCREEN`, no device-tree node, and moving a band boundary is a command-line flag instead of a rebuild and a flash. Only the painting required a kernel change, and only because the strip lies outside fb0. Getting there meant establishing the controller from scratch, since no CST9217 driver exists in 7.2.4 and the tree's only Hynitron support matches `hynitron,cst340`: the part is at I2C 0x1A and `0x5A` is the DRV2605 haptic driver, which an early single-register probe had wrongly identified as touch -- the discriminator is the CST92xx unlock handshake, not a register read. A freshly reset part sits in command mode answering transactions and reporting nothing, so it needs `0xD1=0x01`, `0xD1=0x1E` twice, byte 1 of `0x0002` reading back `0x1E`, then `0xD1=0x09`. And the freshness test that matters is the one mainline's own CST3xx driver uses: a block whose byte 0 is `0xAB` is not a report, which is why an untouched screen can otherwise present a frozen block as a phantom press. Three findings are recorded rather than smoothed over. Reads of this device on the bit-banged bus fail often -- roughly two in eight, against forty consecutive clean reads from the PCF85063A and the AXP2101 on the same pins at the same clock -- so a failed read skips the poll and lets the release timeout decide, because treating it as a release would make the buttons flicker. A very quick tap can therefore fall between polls and be missed; a deliberate press always lands, and the user confirmed the buttons work. And `tbtns` cannot pulse the controller's hardware reset, because `pca953x` owns 0x20 and a plain `I2C_SLAVE` returns `EBUSY`; the software bring-up has worked on every boot but the hardware path is unused. Two diagnostics were built rather than eyeballed: `scripts/tbtns-selftest.py` drives a `-C` classify mode on the build host to check the band arithmetic, which is the only part of this that can be wrong invisibly, and `scripts/watch-deploy.sh` wraps the netcat deploy after three separate hand-run transfer failures -- a backgrounded `nc` dying of SIGHUP, a port probe being consumed as the transfer, and `ETXTBSY` from moving a running binary. The cycle also corrected a project fact: `FB_Y` is 45, not the 46 that README and LCD-007 both stated, which the driver's own probe line `ring cols 66/387 rows 44/445` settles and which the buttons inherit by deriving from the macro. An unrelated observation is logged but not chased: esp-hosted prints `process_rx_buf: bad if_type 6` to the console every few seconds, which is what scrolled past the user during testing and briefly looked like evidence that touch was working. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 3 was validated against the template with six sections in canonical order, sequential numbering from 2, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The three buttons are functional and the user accepted them. The next improvement to this work, if it is wanted, is the missed fast tap: polling at roughly 60 ms with an unreliable ack means a quick flick can fall between samples, and the two candidate fixes are a shorter poll interval or retrying the ack until it succeeds rather than once. After that, the next capability gap remains the console repaint ceiling of about 5 fps, which has a concrete lead in LCD-005 -- fewer, larger transfers through a persistent RAMWR window. The `process_rx_buf: bad if_type 6` console spam from esp-hosted is worth a cycle of its own because it currently pollutes the display during any test. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- userspace/tbtns.c
- scripts/build-userspace.sh
- scripts/tbtns-selftest.py
- scripts/watch-deploy.sh
- .gitignore
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 4 COMMIT Unreleased 2026-11-09T13:11:15-07:00

#### Coming From:

Unreleased d786239

#### Purpose:

Fix the buttons' delayed detect, where a finger held on a bar produced one position and then silence until it was lifted.

#### Outcome:

The freeze was in this project's reader, not the hardware, and the user's suspicion -- that it was the way the sensor is read rather than how often -- was correct. `tbtns` read seven bytes of the controller's fifteen-byte report block and then acknowledged, on the reasoning that a shorter transfer is more reliable on this bus. The acknowledge that advances the controller's buffer is issued against the block, so acking after a short read left the buffer mid-block and it never advanced: the first report was taken, then nothing, then the release timeout fired while the finger was still down. The fix is the vendor sequence exactly -- read all fifteen bytes, acknowledge immediately with nothing between the read and the ack -- which is what TinyPetz's `touch.c` does and what this project had deviated from in two ways at once. Verified on hardware: holding the green bar and sliding across all three now produces a continuous coordinate stream (ctrl_x 224 to 78 to 198 to 325) with a press and release firing at each band transition instead of one press followed by silence. The correction rewrites an earlier conclusion rather than adding to it: the "2 in 8 reads succeed" failure rate recorded for this part was mostly this bug. Measured over 20 s with the full block, transfer failures are about 2 percent (6 in roughly 314 polls) with zero ack failures, and the all-0xFF blocks that dominate the census are the controller answering "nothing new" rather than a failed transfer. A second, independent defect surfaced in the same test and was fixed by loosening rather than tightening: taps landing on a bar decode to panel Y in the 441..444 range while the bar is painted from row 445, so two taps in a ten-tap run were discarded as outside the strip. The threshold is now 440, five rows of tolerance, which matches the user's original intent that these be regions rather than targets and is consistent with TinyPetz's measured 4.6 px RMS Y residual. What remains is accepted, not fixed: at ten quick taps the run produced nine readable reports and eight presses, the loss being a tap shorter than one poll interval. The user chose to keep the working behaviour rather than chase that, and the poll interval is left at the 60 ms cadence that matches the controller's own report rate. The shipped binary was confirmed byte-identical to the build that was tested by comparing its sha256 against the earlier deployment, and the band arithmetic selftest still passes. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 4 was validated against the template with six sections in canonical order, sequential numbering from 3, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The buttons are accepted as they are and no further work is owed on them. If the missed fast tap ever matters, the lever is the poll interval, which is deliberately left where it is. The next capability gap remains the console repaint ceiling of about 5 fps, with a concrete lead in LCD-005: fewer, larger transfers through a persistent RAMWR window. The esp-hosted `process_rx_buf: bad if_type 6` console spam is still unaddressed and still pollutes the display during testing, so it is the cheapest next cycle. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- userspace/tbtns.c
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 5 COMMIT Unreleased 2026-11-09T13:21:27-07:00

#### Coming From:

Unreleased fe9de87

#### Purpose:

Reserve the four edge strips around fb0 with one specification, so that the top status bar, the side app slots and the bottom buttons all read their geometry from a single agreed document instead of each inventing its own.

#### Outcome:

`docs/panel-reservations.md` now fixes the layout. The module is 45 px, which is the largest square that fits a side strip without touching fb0 and therefore costs no console columns; 48 was considered and rejected because it would have forced fb0 from 320 to 304 px and cost two of the forty columns. Measured geometry gives a 45 px left strip, a 45 px right strip, 45 rows above fb0 and 57 below it, and the strips are trapezoids rather than rectangles because the glass corners are 45-degree chamfers. The two sides are not symmetric: the left strip is full width from row 80 but the right only from row 89, because the top-right chamfer leg is 10 px longer than the top-left, so the slot grid is laid out on the intersection of the two, rows 89 to 420, and centred within it. Seven 45x45 slots per side at rows 97..411 use 315 of 332 available rows with the 17 spare split 8 above and 9 below, and every one of the fourteen slots was checked cell by cell against the chamfer inequalities and is 100 percent unclipped. The top bar was given the full width including the corners, by explicit decision, and 410 is not divisible by 45, so the nine top cells leave a 5 px orphan at x 405..409 which sits inside the top-right chamfer and is invisible anyway; wasting that space to keep one module everywhere was the deliberate trade. Cell-by-cell visibility across the top bar came out at 2, 69, 100, 100, 100, 100, 100, 61 and 1 percent, which is the concrete statement that the bar has five fully usable cells and four that are clipped, and that clipping is accepted rather than treated as a fault. The bottom strip stays the three buttons as they are, at panel columns 0..134, 137..270 and 273..409, and is deliberately not on the 45 module because it is fixed chrome rather than a slot grid. The central design point recorded is that all four edges are one operation -- write RGB565 into a named rectangle in panel coordinates, outside fb0 -- and that the bottom bar is already that operation, painted at probe time rather than on demand. Non-interference with the console is shown to be structural rather than a matter of discipline: `co5300_blit()` sets its column window to `FB_X + x0 .. FB_X + x1`, so fbcon physically cannot address a panel column outside fb0, which was checked in the source rather than assumed. The document also carries forward the three defects that any new drawing path must respect, since all three reported success while drawing nothing: pixels must be byte-swapped, a degenerate window is not honoured, and a rectangle must be one transfer. No code was written and no hardware was touched -- this cycle is a specification, so Build, Deployment and User Test do not apply. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 5 was validated against the template with six sections in canonical order, sequential numbering from 4, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The specification needs an implementation, and it deliberately leaves the API shape open: ioctl, sysfs node or a small character device has to be settled first because it fixes the userspace contract, and the top bar's drawing should be a daemon for consistency with `blekbd` and `tbtns`. Whatever is chosen must begin with a structured test pattern -- distinct colours per slot and a different colour in each corner of each slot -- because a flat fill proves nothing about addressing on a write-only panel, and this project has already had three drawing paths report success while drawing nothing. The bottom bar's press and release artwork is the last consumer to wire up, once the press path can repaint a region. The esp-hosted `process_rx_buf: bad if_type 6` console spam remains open and is still the cheapest next cycle because it pollutes the display during testing. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- docs/panel-reservations.md
- README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
