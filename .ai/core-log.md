## 17 COMMIT Unreleased 2026-11-09T23:15:52-07:00

#### Coming From:

Unreleased 8e99aba

#### Purpose:

Make the Tamagotchi emulator keep real time on the watch. It ran, and it ran at
1.00x, but any burst of guest activity fell behind by enough to stretch a screen
transition to about 1.5 s.

#### Outcome:

The interpreter's cost was in the wrong arithmetic. The SPLB32 core follows its
reference implementation in keeping cycle and timer accumulators in `double`,
and this target has no hardware double: measured on the watch, a double add is
711 ns against 121 ns for the integer equivalent, every operation a soft-float
library call. `timers_clock()` runs once per emulated instruction and touches
three such counters, so the interpreter paid that for every instruction the
guest executed. Converting the accumulators to 64-bit fixed-point at
`CYC_SHIFT = 7` halves it -- emulator time per frame at 10 fps went from 50.8 ms
to 26.4 ms. The conversion is exact rather than approximate: the sleep tick is
4000000/32768 = 122.0703125 = 15625/128, so at 7 fractional bits it is the
integer 15625 and nothing is truncated, which is what the original comments
feared. Equivalence was proven, not assumed: against the unmodified core, 200M
cycles (50 emulated seconds) agree exactly on instrs, pc, registers, ram_fnv and
dpram_fnv, and a simulated button press matches to the tenth of a millisecond
including the 580.1 ms gap the ROM leaves before redrawing. That comparison
caught a real bug: `TM0L/TM0H/TM1L/TM1H` are guest-visible as raw 16-bit cycle
counters and need the fraction shifted off, or the firmware reads a value 128x
too large -- the trace agreed for 400k instructions and diverged at the first
timer poll. Two neighbouring options are traps and are now recorded as such:
`double` is slow, and `float` is not slow but fatal, because the ESP32-S3 FPU
exists and the compiler emits `add.s`, but the kernel does not enable the
coprocessor for user tasks so the process dies with SIGSEGV; making it usable
means coprocessor context switching on a NOMMU port with a patched `fork()`.
Alongside it, the panel SPI clock became a runtime knob after the reference
firmware's 45 MHz shearing was traced to the write-pointer bug rather than clock
margin: 40 MHz was tested with eight full-width bands in eight colours and came
out clean and in order, which a desynced write pointer cannot produce, so
`/etc/init.d/S51panelclk` now sets it at boot and survives a reboot. It bought
13%, not the 2x it was hoped for -- a full 320x400 frame went 115.15 ms to
100.85 ms -- which says the transfer is not clock bound. Two tooling bugs cost
as much time as the engineering and are fixed: `watch-exec.py` hardcoded
`/dev/ttyACM0` and went silently dead whenever the board re-enumerated as
`ttyACM1` after a replug, and `watch-deploy.sh` lost the last chunk of every USB
transfer because `fold` emits an unterminated final line that `while read`
discards, so a 23616-character file always arrived exactly 216 short while every
chunk reported success. Touch polling went from 60 ms to 15 ms after measuring a
15-byte read at 3957 us and the DRV2605 click at 1796 us. The user-visible
result is that the watch now runs the pet at a true 1.00x with headroom the
transition can borrow from.

#### Next Steps:

The ~1.5 s wait entering the time screen is parked by user decision and runs
fine otherwise; `docs/tamagotchi.md` and EMU-002 record what is established, what
is excluded by measurement, and the one unexplained figure -- panel time of
43-135 ms per frame during a stall against 9.5 ms in the benchmark. The next
measurement, if it is picked up, is to read the per-frame stall log against a
press on the fixed-point build; the press-to-paint instrumentation is already in
`tamafb.c` behind an argv flag. The larger standing item is making the emulator a
service: `/home/run-tama.sh` retries up to 8 times because `exec` is subject to
the contiguity rule, and starting it early from rcS would avoid the lottery
entirely. Anything touching the kernel still needs `./scripts/watch-backup.sh`
first -- the 16 MiB backup from this cycle is
`~/twatch-backup/working-2026-11-09-2212.bin`, sha256 `19105568...`.

#### Files Modified:

`patches/kernel/display/final/co5300.c`, `scripts/watch-deploy.sh`,
`scripts/watch-exec.py`, `userspace/tbtns.c`, `userspace/S51panelclk`,
`userspace/run-tama.sh`, `docs/tamagotchi.md`, `.ai/core-reference.md`

#### Status:

- Build: PASS (kernel and userspace)
- Deployment: PASS (kernel flashed and hash-verified; binaries deployed)
- User Test: PASS (40 MHz bands clean; emulator runs, buttons work)

---

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

## 6 COMMIT Unreleased 2026-11-09T13:23:48-07:00

#### Coming From:

Unreleased 8134396

#### Purpose:

Decide how userspace writes pixels into the reserved edge strips, closing the one question the geometry specification deliberately left open.

#### Outcome:

`docs/panel-api.md` settles the calling convention: one ioctl on `/dev/fb0`, not a new character device, sysfs or debugfs. The deciding reason is not elegance but `FLS-003` -- a new device node needs an mdev rule, and mdev rules are precisely the `/etc` state that a full-image flash destroys, so a node would have to be recreated by hand after every reflash while `/dev/fb0` already exists and is already openable. It is also the display's own interface rather than a third device that happens to share a panel, and the fbdev core forwards unrecognised ioctls to the driver, which was checked in the tree at `fb_chrdev.c:148` rather than assumed. The ioctl magic `'C'` was confirmed unused in this kernel, as was `'c'`, so there is no collision with another built-in driver. Two commands are defined: `CO5300_IOC_WRITE_RECT` taking panel coordinates, a size, a row stride and an RGB565 buffer, and `CO5300_IOC_GET_GEOM` returning the panel and fb0 rectangles so no userspace program hardcodes 45 or 410, which doubles as a feature test because an older kernel returns `-ENOTTY`. Four semantics are load-bearing rather than conventional. The driver rejects any rectangle intersecting fb0, which puts the "edges never disturb the console" guarantee in the driver instead of in a convention -- a userspace bug cannot paint over the console, and the check is four comparisons. Pixels are passed in host byte order and byte-swapped by the driver, because requiring callers to pre-swap has already produced a silent bug here. Degenerate rectangles return `-EINVAL` instead of drawing nothing, since `LCD-002` established that a window one pixel wide or tall reports success. And each call is one window and one transfer, split into bands by the driver if it exceeds the transmit buffer, which is the only proven way to cover more than one buffer. The document also specifies that the driver gains one mutex taken at every panel entry point -- the four fbcon ops and the ioctl -- because fbcon serialises itself with `console_lock` but that lock does not exclude a userspace ioctl, while the ioctl deliberately does not take `console_lock` to avoid a deadlock class for no added safety. Transfer costs were derived from the measured rate in `LCD-005` rather than estimated: 742 ns per byte gives about 3 ms for a 45x45 tile, 29 ms for a full-width 45-row band and 37 ms for the whole bottom strip, and two earlier estimates in the draft were corrected against that derivation. The chief implementation risk is stated rather than glossed: a 45x45 window shape is untested, since `LCD-002` only established that 1 px fails and 432x31 works, and nothing has established where between those the limit lies. Two fallbacks are recorded, full-width band composition from `p->fb` if narrow windows misbehave, and restricting to the top and bottom strips where no fb0 rows are involved. The cycle also closed the loop with the geometry specification, whose "Not decided here" section previously listed the API shape as open; it now records the decision and points at the new document, so the two specs do not disagree. No code was written and no hardware was touched: the watch was unplugged from USB for this cycle, running on battery, which is why no build or deployment stage applies. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 6 was validated against the template with six sections in canonical order, sequential numbering from 5, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

Implementation begins with the mutex and the two ioctls in `co5300.c`, which changes no fbcon behaviour, and the gate for everything after it is a diagnostic that paints a corner-marked pattern into every reserved region so the 45x45 window question is answered by looking at the glass. Argument validation -- bounds, fb0 overlap, degenerate size, stride -- is pure arithmetic and should be tested on the build host beside `scripts/tbtns-selftest.py` so a boundary error does not cost a flash. After that: a top-bar status daemon reading `/sys/class/power_supply` and the WiFi state, slot rendering driven by a config on `/home`, and finally button press and release art extending `tbtns`. Still open from the geometry document: where slot art lives and whether the partial top cells 1 and 7 are offered at all. The esp-hosted `process_rx_buf: bad if_type 6` console spam remains unaddressed and still pollutes the display during testing. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- docs/panel-api.md
- docs/panel-reservations.md
- README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 7 COMMIT Unreleased 2026-11-09T14:06:40-07:00

#### Coming From:

Unreleased 0d25388

#### Purpose:

Implement the panel API decided in docs/panel-api.md and prove on the glass that it draws where it says.

#### Outcome:

`co5300.c` now carries the mutex and the two ioctls from the specification, and the four reserved regions were painted and read back on the hardware. The panel mutex is taken at all five entry points -- fillrect, copyarea, imageblit, write and the ioctl -- so fbcon and userspace cannot interleave a window setup with another window's pixel stream, and the ioctl deliberately does not take console_lock to avoid inverting the fbcon lock order. `CO5300_IOC_GET_GEOM` answers from the running kernel with `panel 410x502, fb0 320x400 at (45,45), offset 22, abi 1`, so no userspace program has to hardcode geometry, and it doubles as a feature test because an older kernel returns ENOTTY. `CO5300_IOC_WRITE_RECT` takes panel coordinates and host-order RGB565 and byte-swaps in the driver. The argument validation was moved out into `co5300_rect.h` so it can be exercised on the build host, and `scripts/co5300-selftest.py` now drives 43 cases -- legal tiles and strips, degenerate sizes, reserved and flags misuse, a stride smaller than the width, out-of-panel bounds, and the fb0 overlap guard including the exact boundary rows and columns -- with all 14 side slots confirmed accepted; `build-kernel.sh` runs it as a build gate. The chief risk carried into this cycle was resolved in the useful direction: 45x45 windows are honoured, as are 45x315, 410x45 and 410x57, so the full-width band fallback in the spec is not needed. One defect in this cycle was the diagnostic rather than the driver, and it is worth recording because it produced a confident wrong answer: the test pattern's white markers were written as `RGB565(31, 31, 31)`, and in RGB565 green is a 6-bit field, so 31 is half brightness and the result is `0xFBFF` -- pink. The user reported all four edges pink and a pink sliver under the green bar, which read as a pixel-path fault; the arithmetic showed the API had been correct throughout and the tool had never drawn white. Colours are now named constants with a comment on the field widths, and a solid-primary mode exists so a region's colour can be read without interpreting stripes. A second defect was genuine but is in the test harness, not the driver: `panel-test -R` and `-1` segfaulted intermittently, roughly half of runs, with no output and no kernel oops in dmesg. It was bisected against rect size -- 45x45, 45x315, 410x45, 410x57 each ran 10 to 15 times with no failure, and the crashing modes then ran 24 consecutive times with no failure on the current build -- so it could not be reproduced and its cause is not established. It is recorded as an open, unreproduced fault rather than closed. The paint path itself is proven: the four regions land at the coordinates asked with fb0 untouched. Two operational facts surfaced. The esp-hosted link dropped mid-cycle with `process_rx_buf: bad if_type 6` and would not recover by restarting networking -- `ping` to its own gateway failed with the interface still addressed -- and only a reboot restored it; `scripts/watch-deploy.sh` gained a USB transport for exactly that case, sending chunked base64 over the console, because netcat is not available on that channel at all. The build was flashed to 0x140000 after a full 16 MiB backup, and the kernel booted with the console, PMU and RTC intact. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 7 was validated against the template with six sections in canonical order, sequential numbering from 6, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The API is implemented and proven, so the consumers can be built. The next cycle is the top-bar status daemon: read `/sys/class/power_supply` and the WiFi state and draw into the five fully usable cells with `CO5300_IOC_WRITE_RECT`, which needs no further kernel work. Two facts must be respected from that work: each strip is painted in two layers, a background over rows 89..444 and tile art over the 45x45 slots, or unpainted margins appear; and every colour constant must be checked against RGB565's field widths before a test result means anything. The intermittent segfault in `panel-test` is still open and unreproduced and should be chased before the harness is trusted for anything subtle. The esp-hosted `process_rx_buf: bad if_type 6` fault is now clearly worth its own cycle rather than being a nuisance: it drops the network hard enough that only a reboot recovers it, which costs a deploy channel mid-work. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- patches/kernel/display/final/co5300_rect.h
- userspace/panel-test.c
- scripts/co5300-selftest.py
- scripts/build-kernel.sh
- scripts/build-userspace.sh
- scripts/watch-deploy.sh
- docs/panel-api.md
- docs/panel-reservations.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 8 COMMIT Unreleased 2026-11-09T14:46:17-07:00

#### Coming From:

Unreleased 2cb6ffd

#### Purpose:

Diagnose the diagonal tearing the user reported in the right-hand strip and make panel writes land correctly there.

#### Outcome:

The tearing is real, is not residue, and is not explained by any geometric property of the rectangle. It is explained, empirically, by how much one transfer carries: `CO5300_IOC_WRITE_RECT` now sends every rectangle in bands of two rows, and the right strip draws cleanly. Diagnosing it took six test patterns, each varying one thing, and the two hypotheses carried in were both wrong. The first was width: a 45-wide window was thought to be near some limit, and the observation that 45x45 drew earlier made that unlikely, correctly. The second was X position, on the theory that controller columns 387..431 reach past the controller's RAM boundary inferred from `PANEL_OFFSET_X = 22`. Both were excluded by measurement rather than argument. Width is excluded because severity is non-monotonic: five stacked segments from the same origin gave 45 wide torn, 35 wide clean, and 25, 15 and 5 wide all torn. X position is excluded because nine 45x50 windows swept across the bottom strip at x = 0, 45, ... 360 were all clean, including one at controller columns 382..426 which overlaps the right strip almost entirely. Height is excluded because 45x71 segments tore too. Residue is excluded because the splits are visible in a single uniform fill, which cannot be produced by overlapping partial writes. What the measurements do show is that a single 45x356 transfer into the right strip returns as eight diagonal splits worst at the bottom, while the same rectangle as 178 windows of 45x2 is clean, repeatably, and that the left strip tolerates the single tall transfer the right does not. After the driver change, userspace sending the same single rectangle produced a clean uniform strip, so the cap is invisible to callers. Two things are recorded rather than glossed. The root cause is NOT established: this is a workaround that has held every time it was tried, not an explanation, and it is acceptable only because these are status icons that change on battery or link state -- it would be wrong for continuous drawing, which is why `co5300_blit()` keeps its own uncapped sizing. And the strongest remaining suspect is SPI clock margin, because the reference firmware recorded the identical signature at 45 MHz, "dark horizontal lines shearing slightly down to the right", cured by dropping to 20 MHz, while this project runs at 20 MHz and the signature persists for long narrow transfers. A separate defect in the diagnostic tool was found and fixed during the work: several test patterns painted partial rectangles of different colours into the same region, so anything landing inexactly left a neighbour's colour beside it and read as a tear. That produced two false reports from the user -- "all four sides pink" and "eight tears in a solid fill" were partly artefacts of the tool -- and the lesson recorded is that a diagnostic must repaint its whole region in one operation before its output is trusted. A second defect is open and unresolved: `panel-test -R` and `-1` segfaulted intermittently, and after bisecting rect size ten to fifteen times each with no failure, the modes ran 24 consecutive times clean, so it is unreproduced and its cause is not established. The build was flashed to 0x140000 after a full 16 MiB backup, the vectors address was verified at 0x40380000 before writing, and the kernel booted with the console, PMU and RTC intact. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 8 was validated against the template with six sections in canonical order, sequential numbering from 7, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The panel API is usable as specified: callers hand over one rectangle and the driver bands it. The next cycle is the top-bar status daemon, reading `/sys/class/power_supply` and the WiFi state and drawing into the five fully usable cells, which needs no further kernel work. Two experiments are worth their cost before the tearing is declared closed. First, drop the SPI clock to 10 MHz and repeat the single tall 45x356 transfer: if it comes back clean, the fault is signal margin rather than anything in the addressing, and the driver could then band less aggressively at a lower clock or keep the cap and raise it. Second, if that does not explain it, capture the tearing under a faster scope of instrument -- a full-width reference bar beside the strip in the same frame would show whether rows are lost or shifted. The intermittent `panel-test` segfault remains open and unreproduced. The esp-hosted `process_rx_buf: bad if_type 6` fault still drops the network hard enough to need a reboot and is still the cheapest general cycle. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- userspace/panel-test.c
- docs/panel-api.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 9 COMMIT Unreleased 2026-11-09T15:35:14-07:00

#### Coming From:

Unreleased 38f2f36

#### Purpose:

Answer whether the display path or the CPU limits the panel, for another project that needs the number to decide whether to port to this board.

#### Outcome:

A bench was added to the driver, `CO5300_IOC_BENCH`, which times the same pixel payload twice -- once with CS asserted so the panel receives it, and once with CS never asserted so the peripheral clocks the bytes out into nothing. The difference is the panel's contribution and the rest is the driver and CPU. Measured on hardware: 288x2 (18 chunks of 64 bytes) costs 197 us with CS and 193 us without, and 288x64 (576 chunks) costs 6572 us and 6470 us. Fitting those two points gives 11.42 us per 64-byte chunk and a per-transfer fixed cost of -8.6 us, which is zero within noise. The panel accounts for 0.18 to 0.22 us per chunk, about 1.6 percent. Throughput is 5.6 MB/s against 10 MB/s of quad-lane wire capacity at 20 MHz. Three conclusions follow and they are the reason the cycle was worth running. Per-transfer overhead is negligible, so a dirty-rectangle scheme pays no per-call penalty and splitting a frame into many small transfers is cheap in fixed cost -- which reverses the concern that a 2-row cap would multiply overhead by transfer count. The panel is excluded as a bottleneck outright: removing it changes the time by 1.6 percent. And the remaining 5 us per chunk is CPU work, 16 memcpy_toio writes into a 64-byte FIFO plus a length write and a cpu_relax() completion poll, so DMA caps the gain at about 2x rather than the 5-10x a FIFO-share estimate suggested. One contradiction is recorded rather than resolved: LCD-005 puts a 320x400 frame at about 190 ms while 4000 chunks at 11.42 us is 45.7 ms, a factor of 4.2, and nothing yet explains the difference -- LCD-005 predates the CS-handling fix and may have included work outside the transfer, so it should be re-measured through the real repaint path before either figure is quoted. The cycle also produced a lesson about the instrument. Bench runs that block leave the calling shell unable to execute anything, and on this NOMMU kernel the blocked process cannot be interrupted with Ctrl-C, so only a chip reset clears it; two such wedges cost several resets and a great deal of time before the pattern was recognised, and the fix is to launch a bench with nohup in the background so it can never hold the shell. During the work a genuine bug in the test tool was found and fixed: the getopt string listed `b` without a colon while `r` had one, so `-b` received no argument, `sscanf` was handed NULL and the tool died with SIGSEGV and no output. That is almost certainly the intermittent crash recorded as unreproduced in entries 7 and 8 -- same signature, same absence of a kernel oops, and the reason it correlated with newly added modes rather than with rect size. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 9 was validated against the template with six sections in canonical order, sequential numbering from 8, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The bench is a permanent diagnostic and `docs/panel-transfer-cost.md` records how to run it safely. Two follow-ups are worth their cost. First, re-measure the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005; until that is done the practical frame rate of the console is unknown and both published figures are suspect. Second, with per-transfer cost shown to be zero, the 2-row cap on `CO5300_IOC_WRITE_RECT` can be revisited on transfer-time grounds alone: the cap costs almost nothing in overhead, so it stays for correctness rather than cost, and the tearing root cause from LCD-009 is still open with SPI clock the remaining suspect. The next feature cycle is the top-bar status daemon, reading `/sys/class/power_supply` and the WiFi state, which needs no further kernel work. The esp-hosted `process_rx_buf: bad if_type 6` fault wedged the network three times in this session and each time only a chip reset cleared it, so it is now the most costly open defect in the project and should be scheduled. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- userspace/panel-test.c
- docs/panel-transfer-cost.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 10 COMMIT Unreleased 2026-11-09T16:53:31-07:00

#### Coming From:

Unreleased fb9ad02

#### Purpose:

Fix the `process_rx_buf: bad if_type 6` console spam and restore the firmware diagnostic channel it was hiding.

#### Outcome:

The cause was an interface enum that had drifted between firmware and kernel. esp-hosted tags every payload with an `if_type` byte and both sides test it against `ESP_MAX_IF`; upstream has six values and this project's firmware fork added a seventh, `ESP_LOG_IF`, for its own diagnostic stream, so the firmware's `ESP_MAX_IF` became 7 while the kernel's stayed 6. Every log frame therefore arrived tagged 6, failed `if_type >= ESP_MAX_IF` in `process_rx_buf()`, and was dropped -- which is the message that had been scrolling the console for days. The frames were not merely rejected, they had nowhere to go: `esp_log_pipe.c` was carried in `patches/kernel/display/final/` but `build-kernel.sh` never installed or compiled it, so `/dev/esp-log` did not exist on this board at all. `patches/kernel/0007` fixes both together, deliberately: the enum entry, the dispatch to `esp_log_pipe_rx()`, the init and deinit calls and the Makefile object, because accepting `ESP_LOG_IF` without a consumer would leave those skbs queued in `rx_q` forever, which is worse than dropping them. Verified on hardware: `/dev/esp-log` now exists as character device 10,257 and delivers real firmware lines such as `@LOG hid: scan 1 complete (connected 0)`, while `bad if_type` errors are zero through the first minutes of uptime against hundreds per boot before the change. The log stream is sparse and that is correct -- the firmware emitter is rate limited to 40 lines per second because, in its own words, a flood of these frames competes for the same shmem link the kernel is booting over. PMU, RTC, local time and WiFi were checked afterwards and are unaffected. Two additional defects surfaced while making the build honest. The escapement patch loop used to print `FAILED to apply` and carry on, which is the silent-loss pattern of TOL-003; it now dies, and on the first clean build that immediately caught two patches that had been doing nothing. One targets `new-files/.../devkit_c1_16m_linux.config`, a file in the port repository rather than the kernel tree, so it is now skipped explicitly as the port's to apply. The other, `0006-dts-side-buttons.patch`, had never applied and never needed to: every element it adds -- `gpio-keys`, `pusher_b_pins`, `pmic_irq_pins`, `KEY_ENTER` and the PMU interrupt -- is already present in the whole-file DTS that `build-kernel.sh` installs, checked one element at a time before it was deleted, and keeping a patch that can never apply is a trap for the next person. What is explicitly not claimed: that this fixes the wedge. The firmware's own source records that a flood of these frames "has already been shown to stop Linux reaching userspace", and that is consistent with what was observed -- ICMP answering throughout while a telnet login stalled, and the IPC ring advancing its read pointer even on a rejected frame so errors could not have stalled it. But that is a hypothesis supported by the evidence, not a demonstrated mechanism, and if the wedge recurs the next instrument is per-interface packet counts in the ring. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 10 was validated against the template with six sections in canonical order, sequential numbering from 9, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The firmware diagnostic channel is now a tool rather than a gap, and it should be used before anything else in the deferred list: `cat /dev/esp-log` shows what the BLE host and the shmem link are doing after handoff, which is exactly the period that was previously undiagnosable, and it is already reporting that the keyboard scans repeatedly and connects to nothing. That makes the accepted `sm_bonding` defect -- pairing needed after every reboot -- the obvious next cycle, because the log can now show whether the load path runs at all. The wedge needs a stability test across several hours of ordinary telnet use before it is called fixed; if it recurs, add per-interface counters where the ring is consumed. The tearing root cause in LCD-009 is still open with SPI clock the remaining suspect, and re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding. The top-bar status daemon has not been started. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/0007-wireless-esp32-ng-add-ESP_LOG_IF-and-log-pipe.patch
- patches/kernel/0006-dts-side-buttons.patch
- scripts/build-kernel.sh
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 11 COMMIT Unreleased 2026-11-09T17:11:25-07:00

#### Coming From:

Unreleased 6d3c8b3

#### Purpose:

Make the new Rii K06 combo keyboard type on the watch, replacing the Logitech it was developed against.

#### Outcome:

The keyboard now pairs, encrypts and types, and the defect that stopped it was a subscription rule rather than pairing. The K06 exposes the Boot Keyboard Input characteristic 0x2A22, so the firmware set its boot-input flag -- and nothing ever notifies there. Its boot-format reports arrive on report characteristic 0x001E instead, exactly as TinyTang's BLE-019 records for this same unit. The host's rule was to subscribe only to the boot input whenever one existed, on the reasoning that one CCCD is cheaper than six; that rule is wrong for a device which has the characteristic and never uses it, and it fails silently, because the link comes up and only the keys are missing. The fix follows TinyTang's own implementation, which subscribes to the boot input and every notifying 0x2A4D report together "so reports are seen whichever way the device actually sends them". Verified on hardware with the K06 in LE pairing mode: the log shows connect (0), security initiate (0), protocol mode -> boot (0), and subscriptions to handles 0x0015, 0x001a, 0x001e, 0x0025 and 0x002c where the old firmware subscribed to 0x0015 alone, and blekbd then decoded 8-byte boot reports into KEY_Y and KEY_F which the operator confirmed appear on the AMOLED console. The claim in BLE-003 that boot protocol costs "one subscription instead of six" was wrong and is corrected there, along with the fear that six subscriptions would exhaust the mbuf pool: that was six concurrent CCCD writes, and subscribe_next issues one and resumes from its completion callback. A second defect was introduced and caught in this same cycle, and it is recorded because it is a property of the board rather than a slip. To find out whether the K06 was advertising at all, a log line was added to the scan callback for every advertisement. That stopped the network interface coming up: fw_log sends each line as an IPC message over the same shmem link WiFi initialises over, the kernel reported Command[1] timed out and esp_add_network_ifaces failure, and there was no espsta0 and no /dev/esp-log, because the log pipe driver initialises after the step that failed. The failure was self-concealing -- the diagnostic channel died with the network, so the log could not report what killed it. It reproduced on two consecutive boots and was reverted before anything else was tried; the same firmware without the logging restored both immediately. On this board logging is traffic, and the emitter's 40 lines per second is a budget rather than a guarantee. The firmware change is committed locally in build/esp-hosted/esp_hosted_ng, which is a fork of upstream jcmvbkbc/esp-hosted and must never be pushed; the remote is untouched. README was also corrected where it claimed re-pairing was automatic: with sm_bonding enabled and the load path unfinished, the keyboard has to be put into pairing mode by hand after each reboot, which is a regression against the earlier silent re-pair and is now stated rather than glossed. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 11 was validated against the template with six sections in canonical order, sequential numbering from 10, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The bond is the obvious next cycle and it is now the only thing standing between this keyboard and being pleasant to use. `blekbd` reports `bond saved (332 bytes)` while the firmware's `bond_load()` is the unfinished half, so the key it stores is never restored and the keyboard must be re-paired by hand after every reboot -- a regression against the previous behaviour, not the original design. TinyTang has a working implementation for this exact problem on comparable silicon, `ports/bl616/tang_ble_bonds.c`: a versioned file with a magic, per-slot records covering the address, key size, flags, LTK and its rand/ediv, and a CRC-32 over the whole thing, which the firmware reads at boot and puts into the host's key table before devices reconnect. That is the reference to follow rather than a design from scratch, and documenting the Escapement equivalent in docs/bluetooth-bond.md as it is built is cheaper than reconstructing it later. Two smaller items are worth clearing first because they are cheap: the K06 also carries a touchpad, which its report map declares as report ID 3 with X, Y and wheel, and the consumer and system collections that would give media keys are declared too -- both are reachable now that every notifying report is subscribed, but neither is decoded. Separately, the tearing root cause in LCD-009 remains open with SPI clock the remaining suspect, and re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding. BLE media keys, firmware-side bond persistence itself and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 12 COMMIT Unreleased 2026-11-09T17:34:21-07:00

#### Coming From:

Unreleased b4600dc

#### Purpose:

Make the Rii K06 reconnect on its stored bond after a reboot, without the user putting it into pairing mode.

#### Outcome:

The bond now loads and the keyboard reconnects unattended, and the three defects that prevented it had been hiding each other for several sessions. The first was observability, exactly as `docs/bluetooth-bond.md` predicted: `bond_load()` runs from the NimBLE host-sync callback, before Linux starts, so every line it logged was dropped and there was no evidence whether the file had been read, rejected, or never opened. `fw_log_defer()` and `fw_log_flush()` now hold boot-time lines in a small fixed pool and release them when Linux first speaks on the pipe; the first flush printed the cause immediately, as `bond: no room to read etc (458752 bytes)`. The second was PSRAM. `map_bond_partition()` asked for `heap_caps_malloc(part->size, MALLOC_CAP_SPIRAM)`, and this firmware sets `CONFIG_SPIRAM_USE_MEMMAP`, which maps PSRAM as address space for the Linux kernel instead of registering it as a heap region -- so no allocation on this board can carry that capability and the call could never succeed. `bond_load()` therefore returned before it opened the file, which is why every earlier attempt to fix the load logic was revising code that never ran. It now allocates internal RAM and reads a bounded prefix, 16 KiB doubling until the file is found, rather than the whole 448 KiB partition that never fits and does not need to. The third was in the jffs2 reader: the node walk bounds-checked the header but then trusted `node.totlen`, so a truncated image read past the buffer. That was latent while the image was always whole, but bounded reads make a partial trailing node normal, so the check had to be added with them. Verified on hardware with the full cycle: flash, reboot, `bond: loaded 4 entries from ble-bond.bin (0 rejected)`, then a keypress with the keyboard NOT in pairing mode, after which it reconnected on the stored key and typed, with KEY_G decoded by blekbd. Two documentation errors were corrected in the same pass. README claimed re-pairing was automatic, and separately listed the bond as not done; both are now stated as working, with the forced split explained -- the firmware cannot write flash after Linux starts, so Linux writes the file and the firmware only reads it. And the media-keys limitation was stale: since the host now subscribes to every notifying input report, the consumer-page reports carrying volume and browser keys do reach Linux and blekbd has a consumer usage map for them, so what is actually missing is the report map, whose reader `map_step()` is never called, leaving the daemon on the boot-layout fallback. That is stated as untested rather than either working or absent. The firmware change is committed locally in build/esp-hosted/esp_hosted_ng, whose remote is upstream jcmvbkbc/esp-hosted and must never be pushed. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 12 was validated against the template with six sections in canonical order, sequential numbering from 11, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The Rii K06 is now working as intended: it pairs, types, and reconnects after a reboot on its stored bond. The next thing the user asked for is the same treatment for the Logitech keyboard, which should be mostly a matter of checking it against the changed subscription rule and confirming both devices can hold a bond at once -- `ENTRY_MAX` is 8 and the K06 uses 4, so a second device needs the count checked before anything is assumed. Two capabilities are now reachable and unbuilt: the K06's touchpad, which its report map declares as report ID 3 with X, Y and wheel, and media keys, which need the report map read -- `map_step()` exists but is not called -- chunked and serialised because that read is what exhausted the ATT buffers once before. Beyond Bluetooth, the tearing root cause in LCD-009 remains open with SPI clock the remaining suspect, and re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding. The top-bar status daemon has not been started.

#### Files Modified:

- docs/bluetooth-bond.md
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 13 COMMIT Unreleased 2026-11-09T17:51:08-07:00

#### Coming From:

Unreleased 7ac454d

#### Purpose:

Check whether the three touch buttons still work, after several firmware and kernel cycles had rebooted past the point where they were last tested.

#### Outcome:

They did not work, and the reason was not the driver. The daemon had only ever been started by hand with nohup during the cycle that built it, so nothing brought it back at boot: after the reboots of the last few cycles there was no `tbtns` process and no `touch-buttons` device. `userspace/S50tbtns` now installs to `/etc/init.d` and is started by rcS alongside the keyboard daemon, deployed with a sha256 check; `/etc` is jffs2 so it survives a reboot, and only a full-image flash removes it. With the service running, the buttons were then verified twice over. The daemon log recorded three presses in the order they were made with a matching release after each, decoding to bands 0, 1 and 2. And a raw read of `/dev/input/event3` during a second round of presses carried 192 bytes decoding to `type=1 code=256 value=1`, `SYN_REPORT`, `value=0`, then 257, then 258 -- so the events reach the kernel's input layer and not merely the daemon's log, on the firmware built earlier today. `/proc/bus/input/devices` reports `EV=3`, which is EV_SYN plus EV_KEY and nothing else: three on/off switches, no pointer, which is what these are meant to be. Two characteristics of the touch part are worth restating because they are visible in the log during any of this: `i2c i2c-0: sendbytes: NAK bailout` appears as the CST9217 intermittently refuses a read, which the daemon tolerates by retrying and by never treating a failed read as a release; and the press path has a known ceiling where a tap shorter than one poll interval can fall between samples, already recorded. Neither showed up in this test: three presses, three presses registered. No code changed in this cycle beyond the init script, so there was nothing to build. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 13 was validated against the template with six sections in canonical order, sequential numbering from 12, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

Nothing consumes the three buttons yet: they emit BTN_0, BTN_1 and BTN_2 and no daemon acts on them, so pressing them changes nothing on the glass. Wiring them to something is the obvious next use of them, and pressing should eventually repaint the bar with its pressed image, which needs the panel region write path already built for `CO5300_IOC_WRITE_RECT`. The user has designated the Rii K06 as the official input device and deferred both its touchpad and the Logitech keyboard, so those stay parked. One piece of housekeeping is worth doing before the next full-image flash: three things now have to be re-installed by hand after one -- `/etc/TZ`, `S01hwclock` and now `S50tbtns` -- plus the deployed binaries on `/home`, and consolidating them behind one installer would remove a step people forget. Beyond that, the tearing root cause in LCD-009 is still open with SPI clock the remaining suspect, re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding, and the top-bar status daemon has not been started.

#### Files Modified:

- userspace/S50tbtns
- README.md

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 14 COMMIT Unreleased 2026-11-09T19:08:00-07:00

#### Coming From:

Unreleased 97aafcc

#### Purpose:

Put the Tamagotchi V3 ROM in flash so the emulator starts at any uptime instead of only on a fresh boot.

#### Outcome:

The ROM now lives raw in flash at 0x480000 and is read from userspace through the kernel's linear flash window at 0x42480000, costing no RAM at all. The problem it solves is KRN-001: the emulator's ROM is 512 KB and NOMMU needs it contiguous, so a malloc succeeded on a fresh boot and failed once the heap fragmented, with an exec-time allocator error the program could not report -- the pet could be started exactly once per boot. Three candidate routes were tested rather than assumed. File mmap works but costs 1020 KB, twice the file, because NOMMU copies the mapping; MAP_SHARED fails with ENODEV. The linear flash window is readable from userspace and was verified by checksumming a 64 KB range through it. The third route was the answer, and it is recorded as EXT-002 because it is a property of this port rather than of the emulator. The ROM was extracted from rom_embed.c, checked against the sha256 that file declares, written with esptool, and then verified independently by reading it back through the window and comparing CRCs -- flash crc32 301F0E87 against file crc32 301F0E87 -- rather than trusting esptool's own hash. A guard was added to scripts/build-kernel.sh so the kernel can never grow into the slot: an xipImage that would reach 0x480000 is refused, because otherwise a larger kernel would be silently overwritten by the next ROM write and the pet would be subtly wrong rather than obviously broken. The guard was checked against four sizes including exactly-at-limit and one-byte-over. The front end needed three changes, all in TamaHarness's portable/tamafb.c and documented in docs/tamagotchi.md: ROM_FLASH_ADDR selects the flash ROM and skips the allocation; FB_W and FB_H became overridable because the defaults are SEG_W+32 by SEG_H+48, which is 324x348 and larger than fb0, so the program refused to draw; and the mmap write path was replaced with banded pwrite() because the CO5300 driver implements no mmap operation. Static, and deliberately without -DEMBED_ROM, which would compile the ROM into .bss and reintroduce the same contiguity problem -- bss is 35 KB this way and would be 547 KB otherwise. A second defect was found and fixed while getting the picture on the glass, and it was not the emulator: every printk mirrors to all consoles and fbcon is one of them, so each kernel message repainted console text over the frame and the next frame repainted back, which the operator saw as flicker between the console and the game. The trigger was constant because the touch daemon polls the CST9217 every 60 ms, the controller NAKs, and i2c-gpio prints a line per NAK at KERN_ERR. /etc/sysctl.conf now sets kernel.printk = 1 4 1 7 and this is recorded as EXT-003; dropping to 3 first did not help, which is how the message level was identified. Verified on hardware: the egg renders and wiggles, the operator confirmed it on the glass, and the emulator ran at 1.00x realtime with the largest free block down to 128 KB -- 16 frames in 4 s at 3833499 emulated cycles/s -- where the malloc build cannot start at all. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 14 was validated against the template with six sections in canonical order, sequential numbering from 13, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The emulator runs but nothing controls it: the three touch bars emit BTN_0, BTN_1 and BTN_2 and nothing consumes them, and splb32_pin(cpu, 0, mask, level) with PA bits 0x01/0x02/0x04 is the hook -- tamagui.c already drives exactly that from a keyboard, so the mapping is known and the work is reading event3 and holding the state across frames. After that it should become a service, S51tamafb, which the flash ROM now makes practical at any uptime; that is also the point at which the console loglevel should be raised and lowered by the app itself rather than left at emergencies-only for the whole session. The pwrite cost is still not separated from the render cost: LCD-005's ~190 ms per frame conflicts with LCD-010's 45.7 ms projection by 4.2x, and instrumenting render_band against the pwrite would settle which dominates before any frame-rate work is attempted. Beyond the emulator: the tearing root cause in LCD-009 is still open with SPI clock the remaining suspect, the top-bar status daemon has not been started, and the K06's touchpad and media keys remain reachable but unbuilt. The Logitech keyboard is parked by user decision now that the Rii K06 is the official input device.

#### Files Modified:

- scripts/build-kernel.sh
- docs/tamagotchi.md
- README.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 15 COMMIT Unreleased 2026-11-09T19:17:57-07:00

#### Coming From:

Unreleased 354e0ea

#### Purpose:

Give the three touch buttons a click, so a tap feels like a button rather than glass.

#### Outcome:

The DRV2605 haptic amplifier at I2C 0x5A now plays a Strong Click on every button press, and the operator confirmed it is felt. It was approached by reading the chip first rather than by following TinyPetz, which turned out to be the right order: TinyPetz enables the haptic rail at boot and never writes to the DRV2605 at all, so the vibration noted there was the chip's own power-on behaviour and gave no register sequence to copy. Three reads off this unit settled everything before a line was written -- STATUS 0x00 read 0xE0, whose DEVICE_ID bits 7:5 are 0b111 for a DRV2605 with DIAG_RESULT clear; FEEDBACK 0x1A read 0x36, whose bit 7 is 0 and therefore means an ERM motor rather than an LRA, which decides the effect library since 6 is LRA and 1 to 4 are ERM; and the XL9555 port 0 output read 0xff, confirming the expander bit that gates the motor's power is already high. Two things about this part fail silently and both are recorded as BRD-005: a chip whose expander bit is low answers every I2C transaction and never moves, and a library mismatched to the motor type buzzes weakly or not at all instead of erroring. The driver is userspace/haptic.c with haptic_init(), haptic_tap() and haptic_close(), kept separate from tbtns because the same three calls serve anything on this board wanting a click; it verifies the device ID before writing anything, so it refuses rather than sending effect IDs to a stranger at that address. The click is fired on press only and never on release, because a click on both ends of every tap reads as two events and feels like noise rather than feedback; the operator accepted it as shipped. On the board the daemon reports `haptic ready (DRV2605, ERM)`, and the failure path is non-fatal by design -- a board without the part, or a rail that never came up, logs `no haptic -- buttons will be silent` and the buttons keep working. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 15 was validated against the template with six sections in canonical order, sequential numbering from 14, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The buttons now press, emit BTN_0/1/2 and click, and nothing consumes the events, so wiring them to something is the next use of them. The obvious consumer is the Tamagotchi, whose core takes input through splb32_pin(cpu, 0, mask, level) on PA bits 0x01, 0x02 and 0x04 with level 1 for press and -1 for release -- tamagui.c already drives exactly that from a keyboard, so the mapping is known and the work is reading /dev/input/event3 each frame and holding the state across frames. A second consumer worth having is pressed and released imagery on the bars themselves, which needs nothing new: CO5300_IOC_WRITE_RECT already writes panel rectangles and the driver paints the bars, so a pressed state is a rectangle repaint. Two larger items remain open from earlier cycles: the pwrite-versus-render split for the emulator is still unmeasured, and LCD-009's tearing root cause is unresolved with SPI clock the remaining suspect. The top-bar status daemon has not been started. The Logitech keyboard, the K06's touchpad and BLE media keys are parked by user decision.

#### Files Modified:

- userspace/haptic.c
- userspace/haptic.h
- userspace/tbtns.c
- scripts/build-userspace.sh
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 16 COMMIT Unreleased 2026-11-09T19:31:21-07:00

#### Coming From:

Unreleased 3964fc8

#### Purpose:

Stop the console's blinking cursor showing through the Tamagotchi as a hole.

#### Outcome:

The cursor is now suppressed while the emulator runs and restored when it exits, and the operator confirmed the picture is clean. It was a second, independent source of console bleed-through rather than a variant of the first: `console_loglevel` silences `printk`, but fbcon blinks the cursor on its own timer and repaints that one cell each time, so it sailed straight through the sysctl fix from the previous cycle. The knob is `/sys/class/graphics/fbcon/cursor_blink`, which exists on this kernel and read 1. `tamafb` now saves the current value, writes 0 before its first frame, and writes the saved value back before it prints its results -- restoring rather than leaving it off, because a blinking cursor is correct at a shell prompt and turning it off for the session would be a regression disguised as a fix. Verified: `blink=0` while the process runs and `1` after it exits. Chasing it also turned up a real limit that constrains the next step, and it is recorded as a revision to KRN-001 rather than as a footnote. Backgrounding the emulator failed intermittently with SIGSEGV, and the cause was not the new cursor code: at a fragmented moment the largest free contiguous block had fallen to 256 KB, and the 222 KB static binary then started in the foreground every time while dying roughly one launch in three when backgrounded with `&`. So exec is subject to the same contiguity rule as any allocation, the margin is thin near the block size, and foreground and background are not equivalent. That matters directly for making the emulator a service: starting it early from rcS avoids the situation, starting it late does not. Getting the launch to succeed for the operator's check took a retry loop, and the earlier foreground runs at 1.00x are what showed the fault was in the launch rather than in the code. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 16 was validated against the template with six sections in canonical order, sequential numbering from 15, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The emulator is presentable now: it runs, it draws, the console stays out of the way, and the bars click. Two things remain before it is a game, and the first is small. Nothing consumes BTN_0/1/2, so the pet cannot be played; `splb32_pin(cpu, 0, mask, level)` on PA bits 0x01/0x02/0x04 with level 1 for press and -1 for release is the hook, and tamagui.c already drives exactly that from a keyboard, so the mapping is known and the work is reading /dev/input/event3 each frame and holding the state across frames. The second is making it a service, and that now has a constraint attached: the binary must be started early, from rcS, because exec is subject to the contiguity rule and the margin is thin -- the measurement above is the reason not to simply copy S50tbtns. Separately, pressed and released imagery on the bars would need nothing new, since CO5300_IOC_WRITE_RECT already writes panel rectangles. Larger items still open: the pwrite-versus-render split for the emulator is unmeasured, LCD-009's tearing root cause is unresolved with SPI clock the remaining suspect, and the top-bar status daemon has not been started. The Logitech keyboard, the K06's touchpad and BLE media keys are parked by user decision.

#### Files Modified:

- README.md
- docs/tamagotchi.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
