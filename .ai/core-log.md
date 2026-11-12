## 18 COMMIT Unreleased 2026-11-12T08:11:20-07:00

#### Coming From:

Unreleased 7420750

#### Purpose:

Record the operator's acceptance of the CST9217 interrupt path and fix the logging defect that testing it exposed.

#### Outcome:

Entry 17 left the interrupt as `User Test: NOT RUN` because a press cannot be faked from a desk, and the operator has now tested it at `bb339d1` and reported every margin button working, a drag tracking and releasing correctly, and no regression anywhere on the desktop. Entry 17 is superseded on that one field only; it was accurate when written and is left otherwise as settled history. The interrupt itself is confirmed doing the work rather than merely being registered, which is the distinction entry 17 could not establish: the periodic counter read `irq1488` after the pressing session, and `e2 i2` appears in the ring, which is `EDGE_BOTTOM` index 2 -- Ctrl+Q, the last of the three bottom buttons, so reported presses run through the same hit test the margins always used. A second reading 77 s later, with the glass untouched, still read `irq1488`, so the line is not spuriously asserting and the 250 ms idle fallback is what is keeping the driver awake, not the interrupt. Testing also found a defect this cycle introduced, and it is worth recording because it defeated the instrument rather than the feature: `prs` logged on every press report, and the controller re-reports a held finger about every 66 ms, so the two-second hold in the test wrote roughly thirty lines into the Log Viewer's 64-line ring and the drag emptied it completely. The window showed 1540 lines and the counters the run was meant to be read from had scrolled out of it -- the diagnostic destroyed itself while being used, which is the third time this file records that shape of fault in `touch.c` and the first where the cause was volume rather than the 40-column truncation. It logs the transition only now, which is the report that carries the position a press started at, and holding and dragging are silent. The periodic line was clipped as well: `irq1488 rd3`, with the read count cut off, because the tag, a five-digit millisecond stamp and two growing numbers do not fit in 38 columns, so it is `i%lu r%lu` and fits, and the press counter went with it since the `prs` lines count presses now that there is one per touch. Both fixes were flashed and the operator re-tested on the new build, confirming one `prs` entry per touch with every button still working, and the line now reads `i0 r300` at rest -- four reads a second, unclipped, against the fifty the fixed poll produced. `TCH-009` was revised rather than left as it stood, because it claimed idle interrupts settle at about two per second and this board measures zero: `core-reference.md` now records that the two figures come from two different stacks, that the kernel driver's 2 per second and this port's 0 do not agree, and that the difference is unexplained with two candidates named rather than a cause invented. The pin, polarity, pull-up, falling edge and both poll rates are confirmed a second time on a second stack; only the idle figure was generalised wrongly. The firmware builds clean under `-Werror`, the host suite is 38 of 38, only the app region at 0x10000 was written with the hash verified, and the required `core-syntax.md` audit was run against the whole `.ai/` diff: `core.md` was not touched, the entry conforms to the six-section template and the canonical order, its number is the highest present plus one at 18, and every one of the 86 records in `core-reference.md` was re-parsed individually against the record template with all nine mandatory keys present and no duplicate ids, which is also the check that the revision to TCH-009 did not break the sequence.

#### Next Steps:

A fresh backup is now due and is the most valuable next action, because three firmware writes have landed since `working-2026-11-12-0109.bin` was taken -- the SD note, the interrupt, and the logging fix -- and that backup no longer describes what runs. Nothing else in this cycle is blocked. The screen model written to read the Log Viewer over USB is in a temporary directory again, which the handoff has now flagged twice; it belongs in the fork under a tools directory, and the next cycle should move it before the method is rediscovered a third time. Two smaller items belong with the touch work rather than to a new subject: `touch.int` is read as a board key while `i2c.sda`, `i2c.scl` and `touch.addr` beside it are still hardcoded, so the board configuration is half honest about this port's pins and either all of them should be read or the example should say which are inert; and the OTA check the desktop performs at boot fails on an offline watch with four lines of `esp-tls` and `HTTP_CLIENT` errors that are noise, not a fault, and bury the Log Viewer at every boot -- worth quieting or deferring now that the interrupt has made the ring worth reading. Otherwise the standing queue is unchanged: the bell seam stays parked at the operator's direction with its finding recorded, the top margin is the last unpainted region, what the margins do under a full-screen application waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched. The unbuilt devices are as they were: the BHI260AP at 0x28 answering with no driver, the LoRa radio needing ALDO3 and XL9555 P05 before it is powered, the NFC reader needing DLDO1 with its earlier absence verdict still suspect, and the GNSS, microphone and both pushers with no code at all.

#### Files Modified:

- ports/twatchultra/main/touch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 17 COMMIT Unreleased 2026-11-12T07:53:10-07:00

#### Coming From:

Unreleased aa489e4

#### Purpose:

Drive the CST9217 from its interrupt line instead of a fixed 20 ms poll, so touch stops occupying the shared I2C bus fifty times a second.

#### Outcome:

At `bb339d1` on branch `t-watch-ultra`, `ports/twatchultra/main/touch.c` configures GPIO 12 as an input with the internal pull-up and a falling-edge interrupt, and the touch task waits on a task notification that the ISR gives instead of sleeping for a fixed period. The bus cost was the point: the driver read the report block every 20 ms for ever, about fifty transactions a second on a bus it shares with the AXP2101, the XL9555, the PCF85063A and the DRV2605, and the `touch.int` key that named the unused line had been sitting in the board configuration since the port was written. The shape came from LilyGo rather than from invention, and reading their source changed the design: `initTouch()` in LilyGoLib sets `pinMode(TP_INT, INPUT_PULLUP)` and attaches a `FALLING` handler that only sets an event-group bit, and the I2C transfer happens in the consumer -- in their case LVGL's indev callback, which asks the flag before reading -- while SensorLib's `getTouchPoints()` never touches the interrupt pin at all. So the interrupt is a gate on the read rather than a driver of it, and the pull-up is part of the contract because the part only pulls the line down and releases it. Polling was therefore kept rather than removed, at the two rates TCH-009 measured in the kernel driver: 20 ms while a finger is down, so a drag keeps up with the controller's ~66 ms report period even if the part stops pulsing mid-touch, and 250 ms when idle, which costs almost nothing and means a dead INT line degrades touch to sluggish instead of losing it. THE RELEASE TIMEOUT HAD TO BE FIXED FIRST and would otherwise have shipped broken: `release_if_idle()` counted poll iterations against `RELEASE_MS / POLL_MS`, arithmetic that holds only while every iteration takes exactly one fixed period, so the 250 ms idle sleep would have made a release take three seconds and an interrupt-woken loop would have tied the release to the wake rate rather than to 250 ms. It now measures wall time from the arrival of the last press report. Two smaller corrections came out of the work itself. `touch.int` is now read as a board key, and the `board_keys` check failed on the first build with `+ touch.int (new: describe it, then --record)`, which is the mechanism doing its job -- the key is recorded in `api/board_keys.txt` and described in `board.example.conf`, and `-1` forces polling alone, which is the control for the whole change. And the bring-up line was rewritten to lead with the mode after the first flash proved it unreadable: it said `CST9217 up at 0x1A irq 12`, the Log Viewer clipped it to `CST9217 up at`, and the words lost were exactly the ones that say whether the interrupt path is running. It reads `irq 12 up` now, and the periodic line leads with the interrupt count for the same reason -- the third time this file has recorded a diagnostic defeated by a 40-column truncation, and it was defeated again on the first attempt. Verified on the board by reading the desktop over USB rather than trusting the build: `gpio: GPIO[12]| InputEn: 1` at 3088 ms and `twatch-touch: irq 12 up` at 3089 prove the pin configuration, the ISR service and the handler all took, with no `no irq, poll only` warning anywhere in the ring; the first idle blocks arrive 253 and 250 ms apart, and the counter line reads `irq0 rd300 p0` at 78.3 s, which is four reads a second against the fifty the old fixed poll produced, with no spurious interrupt while nothing is touching the glass. What this cycle does NOT establish is that a falling edge actually fires on a finger, because that needs someone to touch the watch; the interrupt could be wired and still never see a press, and every failure mode that would look identical from here -- wrong polarity, a pin the expander is still holding, an edge that arrives while the task is mid-read -- is distinguished only by a real press. The host suite is 38 of 38 including the `board_keys` check that gated the new key, the firmware builds clean under `-Werror`, and only the app region at 0x10000 was written, with the write hash-verified, so the bootloader, the partition table and `/etc` were untouched and the `working-2026-11-12-0109.bin` backup still describes the firmware that was replaced. A host tool was written to make the on-device verification possible at all -- a small ANSI screen model that injects keys over USB and decodes the desktop's mirror into a grid, which is what read the Log Viewer and is what the handoff had recorded as living only in a temporary directory.

#### Next Steps:

The operator needs to press the margins and the glass and report three things: that the buttons still fire, that a drag inside a full-screen application still tracks, and that the Log Viewer's periodic line shows a non-zero `irq` count after a press. If `irq` stays at zero across touches while `rd` climbs, the line is miswired or the wrong edge, and `touch.int = -1` is the immediate control: if touch then behaves identically, the interrupt was never doing the work and the poll was carrying it. If `irq` climbs but touch stutters, the suspect is the interaction between the notification and the read rather than the wiring. The screen model belongs in the fork under a tools directory so the next cycle can read the glass without rebuilding it, which is the item the handoff already flagged; it was written in a temporary directory again for this cycle. Also carried forward from this work rather than from the queue: `touch.int` is now consumed but `i2c.sda`, `i2c.scl` and `touch.addr` are still hardcoded next to it, so the board configuration is half honest about this port's pins and either all three should be read or the example should say which are inert. Otherwise the standing queue is unchanged and none of it is blocked: the bell seam is parked at the operator's direction, with the finding recorded that `td_vterm_t.bell` and `td_sysinfo_t.bell` are layers rather than duplicates and that the real hedge is `display_set_bell()` wiring the renderer's mirror; the top margin is still the last unpainted region; what the margins do under a full-screen application waits for something to go full screen; Store installs to the card and the image cache at `/sd/TinyDesk/cache` have never run against a card; `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree; the RTC write path has never been exercised by setting a time and power-cycling; and the pre-existing YAML defect in `core-reference.md` is untouched. TCH-009 should be revised to record what this cycle measured about the interrupt: its statement that idle interrupts settle at about two per second is not what this board does, where the count is zero until something is touched, so either the earlier figure described the kernel driver's own bus activity or something changed. The unbuilt devices remain as they were -- the BHI260AP at 0x28 is fitted and answering with no driver, the LoRa radio needs both ALDO3 and XL9555 P05 before it is even powered, the NFC reader needs DLDO1 and its earlier "absent" verdict is suspect because it predates the chip-select parking fix, and the GNSS, microphone and both pushers have no code at all.

#### Files Modified:

- ports/twatchultra/main/touch.c
- ports/twatchultra/board.example.conf
- ports/twatchultra/README.md
- api/board_keys.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 16 COMMIT Unreleased 2026-11-12T07:36:02-07:00

#### Coming From:

Unreleased 12b3f2a

#### Purpose:

Correct the SD card block in `ports/twatchultra/board.example.conf`, which still told a clean checkout that the card was unproven when the bus, the rail and two cards had each been shown to work.

#### Outcome:

At `e703575` on branch `t-watch-ultra`, the SD block in `ports/twatchultra/board.example.conf` no longer says the hardware is unproven. It had claimed three things that were true when written and have been false since entry 10: that the second SPI bus had never been proven to transfer a byte, that the card had never enumerated, and that `power.c` raised only the display's ALDO2 -- where entry 10 recorded `board_power_rails_preinit()` raising ALDO1 and a 29 GB card mounting, writing a file that read back byte-exact and appearing in Files, and entry 11 recorded the same for a 128 GB card. The note carried more weight than its place in a commented-out block suggests: `ports/esp_idf/app/app.cmake` copies `board.example.conf` into the firmware as the built-in configuration whenever `board.conf` is absent, which is the state of every clean checkout, so the wrong text shipped inside generic builds and the shell's own `board` command served it back on the device. What replaced it is what a reader needs before enabling the keys: that MISO is shared with the LoRa radio on CS 36 and the NFC reader on CS 4 and both chip selects must be parked high or they drive the line and corrupt every transaction; that FAT32 is the requirement rather than a size limit, because ESP-IDF 5.3 builds FatFs with `FF_FS_EXFAT` hardcoded off; that the keys stay commented because a card is optional and not because the answer is in doubt, since with them set and no card fitted the watcher re-probes the shared bus every 5 s; and that hot-plug is untested in either direction. Reading the code for that last item produced the one correction worth more than the rest: `sd.cd` looks like the way to stop polling for card presence, and the insert switch is expander pin 10, so `sd.cd = 10` is the line someone would write next, and it is wrong twice -- `sd.cd` is read with `gpio_get_level()` on an ESP32-S3 GPIO while the switch is XL9555 port 1 bit 2 and needs expander support `tdsh_sdcard.c` does not have, and SoC GPIO10 is this port's I2S word clock. The README's subsystem table carried the same "unproven" claim and was corrected in the same commit, because leaving one of the two would have made the tree contradict the file it points at. This is a documentation cycle, so nothing was built, deployed or flashed, and it was verified mechanically rather than by inspection: the example now parses through the shell's own `tdsh_board_parse` to exactly the same 19 settings with the same values and no bad line, and the non-comment bytes of the file hash identically before and after, so the configuration a generic build embeds is unchanged; the file is pure ASCII and no line approaches the parser's buffer limit, and comment text is stripped before anything is parsed. `board.conf` was deliberately left alone -- it is gitignored, so its copy of the stale header is outside the repository change, and it is the file this machine actually builds from. The required `core-syntax.md` audit was run against the complete `.ai/` diff, which is this entry alone: `core.md` was not touched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 16, Coming From names the pre-cycle `HEAD`, Files Modified lists only the two port files and no `.ai/` path or generated artifact, Status uses allowed values only, and the active log holds 16 entries against the 100-entry limit, so no conformance failure was carried forward.

#### Next Steps:

The next item in the queue is the bell seam: `apps/terminal.c`'s `vt_bell()` installs `td_vterm_t.bell` on the Terminal's own emulator and calls `td_sysinfo_t.bell`, which reaches the port's `board_bell()` and `sound_bell()`, so the two hooks are layers rather than duplicates and the handoff's note that `td_vterm_t.bell` is redundant is wrong; the hedge that does exist is `link_twatch.c`'s `display_set_bell(sound_bell, NULL)`, which wires the renderer's mirror vterm as well, and whether that can double-ring should be measured before it is kept or deleted. After that the three API additions go to TinyDesk's author as one written summary, with `TD_VERSION` left at 0.4.0 because the bump is the author's decision. Two follow-ups belong with this work rather than to a new cycle: this machine's `board.conf` still carries the old header comment and should be brought into line the next time it is edited, and card-detect against the expander is a change to `third_party/tdsh` rather than to this port, so it is an upstream proposal and not a local edit. Otherwise the standing queue is unchanged and none of it is blocked: the top margin is the last unpainted region and is inert as `EDGE_TOP`; what the margins should do while an application owns the whole screen stays deferred until something goes full screen; Store installs to the card and the image cache at `/sd/TinyDesk/cache` have never run against a card at any capacity; the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree; the RTC write path compiles and is called but has never been exercised by setting a time and power-cycling; and the pre-existing YAML defect in `core-reference.md`, where a code fence after the last record stops the sequence parsing as one document, is untouched. No fresh backup is needed yet -- nothing has flashed since `working-2026-11-12-0109.bin` was taken and verified -- but the next cycle that changes firmware should take one first.

#### Files Modified:

- ports/twatchultra/board.example.conf
- ports/twatchultra/README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 15 COMMIT Unreleased 2026-11-12T01:14:51-07:00

#### Coming From:

Unreleased 19ad9c4

#### Purpose:

Capture a full backup of the state at the end of this run of work, and write the handoff a fresh session needs to continue.

#### Outcome:

`~/twatch-backup/working-2026-11-12-0109.bin` is the current recoverable state, 16 MiB read at 575.7 kbit/s in 233.1 seconds, sha256 `721ff576453cc232c245f2dfcb9985d61da956772230ccae15709fb319fabc81`, and it is the first backup whose integrity check is a real one: `verify_flash` reported digest matched, because `scripts/watch-backup.sh` now holds the board in the bootloader from the first byte of the read to the last byte of the compare. Three further checks confirm the image is the current build rather than an older one: the 2,559,808-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `b2247146`; both image magics are 0xe9; and the storage partition holds 1,993 non-empty 4K blocks, five more than the last backup, which is the NVS writes this evening's session produced. The read's trailing reset left the board quiet enough that the first probe found no output, and an explicit `esptool run` brought the desktop back -- worth recording because "no output on USB" is normal on this build for the reason entry 14 gives, and the distinction between that and a board stuck in the bootloader is whether a reset produces terminal sequences. `.codewhale/handoff.md` was rewritten for a fresh agent, replacing a file that still described the parked Linux project. It states the two repositories and which remote may be pushed to, the restrictions from core.md, what was verified on the glass tonight, what was verified only by tool output, the hardware facts that cost the most to learn, the method that found the bugs, ten open items in order of cost, and the traps that will cost time -- among them that ESP_LOG reaches nothing on USB, that sdkconfig is generated and a change only there is lost, that esptool 4.13 cannot chain commands, that the shell drops the first character typed into a freshly focused Terminal, and that the host screen reader lives only in /tmp. It also records the three API additions this fork now carries so that the TinyDesk author conversation starts from a list rather than a recollection. Nothing in either repository changed except that handoff file and this entry; the build and deployment fields are N/A because this cycle flashed nothing, and the user test field records the operator's instruction that the run was finished for the night rather than a hardware acceptance of anything new.

#### Next Steps:

The ten open items in the handoff are the work queue and the first three are the ones a fresh session should take: correcting `board.example.conf`, which still calls the SD keys unproven when they work; deciding whether `td_vterm_t.bell` should be kept now that `td_sysinfo_t.bell` is the path that actually works, since both exist only as a debugging hedge; and taking the three API additions to TinyDesk's author together rather than one at a time, remembering that `TD_VERSION` was deliberately not bumped because that is the author's call. After those, the top margin is the last unpainted region and is meant for status icons. Two things are untested rather than unknown and should be said that way: the SD card's hot-plug has never been exercised in either direction, because `sd.cd` is not configured and removal is detected by polling the card rather than by the insert switch, and the Store's install-to-SD path and the image cache have never run against a card at any capacity. The deferred question of what the margins should do while a full-screen application owns the screen waits for something to actually go full screen. Also carried forward: `scripts/watch-backup.sh` now verifies properly but the backup itself is only as good as the moment it was taken, so the next risky change wants a fresh one; the RTC write path remains written but unexercised; and the pre-existing YAML defect in `.ai/core-reference.md`, where a code fence after the last record stops the sequence parsing as one document, is untouched. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- .codewhale/handoff.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 14 COMMIT Unreleased 2026-11-12T01:08:34-07:00

#### Coming From:

Unreleased 4a16a2d

#### Purpose:

Record BEL reaching the speaker, and the three separate faults that stood between a working hook and a sound.

#### Outcome:

At ed3f88f on branch `t-watch-ultra`, `py -c "print(chr(7))"` in the Terminal beeps. The operator heard it and the log records `twatch-sound: beep 16000/16000 ok`, so the sample count and the write result are both confirmed rather than inferred. Getting there took three cycles and three distinct faults, and only the first was in the amplifier code. THE TERMINAL HAS ITS OWN vterm. `apps/terminal.c` keeps a static `td_vterm_t` and initialises it itself; the renderer's `s_vt` in `display.c` is a different object, the mirror. A bell callback installed on the renderer's copy is therefore never called for terminal output, which is exactly what happened in entry 12: the hook was installed, a host test proved the emulator fires it, and the watch stayed silent because `apps/terminal.c` never knew. The port's hook is a `bell()` member on `td_sysinfo_t` now, so any app's emulator can ring it without knowing what hardware is behind it -- the same shape as `drives()`, `batteries()` and the weak board hooks. `td_vterm_init()` DISCARDED THE CALLBACK. It preserves `reply` and `reply_user` across the `memset` and did not preserve `bell`, and `td_terminal_reset()` re-initialises on every session change, so the callback would have worked until the first session switch and then silently stopped -- a fault that would have looked intermittent and been blamed on anything but this. Both callbacks are configuration rather than content, and both survive a re-init now; `test_bell` gained an assertion for it whose negative control takes the fix back out and fails exactly once. THE BELL INSIDE A CSI SEQUENCE RINGS, and that was right all along. My first version of the test asserted it should be silent, failed, and the code was correct: ECMA-48 makes BEL a control function executed wherever it appears, and xterm rings for it mid-sequence too. Recorded because the tempting response to a failing test is to change the code, and here that would have broken working behaviour to satisfy a wrong expectation. Two smaller things were fixed on the way. The I2S write timeout was 200 ms, which sat exactly on the boundary of a 200 ms tone with four DMA buffers, so it could fail or truncate by timing alone; it is 1000 ms now and the write reports how many bytes of how many it accepted. And the temporary probe that beeped on a margin button press -- added in entry 13 to test the amplifier with the shell out of the way -- is removed, because it proved the hardware and has no business buzzing on every press now that the real path works. The hardware questions the earlier cycles could not settle were settled by reading LilyGo's schematic rather than by code: BCLK IO9, LRCLK IO10 and DIN IO11 match this port, SD_MODE is pulled to SPK_VDD through R43 560K so the amplifier is enabled in hardware rather than by a GPIO, GAIN is set by R13, and BLDO2 feeds SPK_VDD. The rail is verified by readback, `bldo2 3300mV en37`, where bit 5 of 0x37 is the enable. One self-correction is worth keeping: the emission log used to print the registers read BEFORE the write, so it reported what the rail had been and said nothing about whether the write landed, and an earlier claim that it confirmed the rail was wrong on that basis.

#### Next Steps:

Audio is proven end to end and can be parked at the operator's direction, with the state to resume from being this: BEL works, sampled playback does not exist, and the desktop has no audio interface beyond the new `bell()` hook, which is a minor API addition `STABILITY.md` sanctions and which the author should be told about. The three API changes this fork now carries -- the `td_vterm_t.bell` callback, `batteries()`, and the `bell()` member on `td_sysinfo_t` -- are a single conversation with TinyDesk's author, and the `td_vterm_t.bell` callback is arguably redundant now that `td_sysinfo_t.bell` exists and is the path that works; keeping both was a hedge during debugging and is worth revisiting rather than defending. Remaining work is otherwise unchanged and unblocked: the top margin is the last unpainted region; the deferred question of what the margins should do under a full-screen application waits for something to go full screen; `board.example.conf` still calls the SD keys unproven when they work; and the pre-existing YAML defect in `.ai/core-reference.md`, where a code fence after the last record stops the sequence parsing as one document, remains unfixed. A fresh backup was taken at 0009 and verifies, but four cycles of firmware change have landed since -- haptics, BEL, the battery line and the backup-script fix -- so another is due before anything risky. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- src/vterm.c
- apps/terminal.c
- include/tinydesk/td_sysinfo.h
- tests/test_vterm.c
- ports/esp_idf/app/main.c
- ports/twatchultra/main/sound.c
- ports/twatchultra/main/sound.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/touch.c
- api/tinydesk.api

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 13 COMMIT Unreleased 2026-11-12T00:32:52-07:00

#### Coming From:

Unreleased 098627b

#### Purpose:

Record the battery line in System Monitor and the AXP2101 fuel gauge behind it, which is the first time the TinyDesk port has read the PMU for anything but rail setup.

#### Outcome:

At ecc6b30 on branch `t-watch-ultra`, System Monitor shows `Battery: 100%  4.17 V full` on the watch, stable across three reads three seconds apart. The 4170 mV agrees with PMU-001's measured 4181 mV on this board, which is the check that the register decode is right rather than merely plausible. TinyDesk had no battery support of any kind, verified four ways before anything was written: nothing in `td_sysinfo_t`, nothing in `api/tinydesk.api`, the taskbar tray draws only the network indicator and the clock, and System Monitor had no battery row. The shape added follows the drives accessor, which is the same problem: an optional array plus a count, cheap enough to call twice a second, and a board with no gauge returns zero so the app says "n/a" rather than the desktop having to know what is fitted. `td_sysinfo_t` gains `batteries()`, a new `td_battery_t` carries label, percent, millivolts and state, and `TD_MAX_BATTERIES` joins the other compile-time limits. Two decisions are worth more than the code. THE PORT OVERRIDES A WEAK HOOK rather than System Monitor reaching into a port: `board_battery_read()` joins `board_power_rails_preinit()` and `board_time_set_post()` in the shared ESP-IDF app as a weak function the board replaces, so the desktop stays ignorant of the chip. AND A GAUGE ABOVE 100 IS REPORTED AS UNKNOWN, not as a percentage: PMU-001 records that a freshly powered AXP2101 reports more than 100 until its fuel gauge settles, and the kernel driver returns -ENODATA for exactly that reason. The port passes -1 and the app prints `--%`, because a wrong number on a battery is worse than no number, and a battery indicator is the one place a plausible-looking lie is most likely to be believed. The registers are taken from the driver this project carries in `patches/kernel/axp2101` rather than from a datasheet: capacity at 0xa4 bits 6:0, voltage at 0x34 bits 5:0 above 0x35 giving 14 bits already in millivolts, charge state at 0x01 bits 2:0 with precharge, CC and CV all meaning charging and done meaning full, and battery presence at 0x00 bit 3. The row sits above the memory bars rather than below the statistics, because on a 26-row screen power left is what a watch user looks for first, and the window grows by one row only when a battery is actually present. Two bugs were caught while writing it and both are worth naming because neither would have shown up on the glass. The percent was formatted straight from an unbounded `int`, which `-Werror=format-truncation` refused; it is now clamped to 0 to 100 with anything out of range treated as unknown, which is the correct behaviour as well as the compiling one. And `on_close()` left every widget pointer set, so a tick arriving after the window closed but before `launch()` ran again would have written through freed widgets -- a use-after-free that the desktop's 500 ms tick makes unlikely rather than impossible, which is the kind that survives for months. The API snapshot was re-recorded at 697 declarations, three added and nothing removed, which is the minor shape `STABILITY.md` sanctions; `TD_VERSION` is still not bumped for the same reason as last cycle. On the question that prompted this: nothing in the audio failure is explained by the battery manager. The rail bits the amplifier depends on were checked against the kernel driver's own descriptors -- ALDO1 BIT(0), ALDO2 BIT(1) and BLDO2 BIT(5) of 0x90, with voltages at 0x92, 0x93 and 0x97 -- and they are correct, so the battery work neither implicates nor exonerates the PMU as the cause of the silence.

#### Next Steps:

The obvious next reading is the one this cycle made possible and did not take: log the full LDO enable byte at boot alongside the rails, so the BLDO2 bit can be seen rather than inferred, and take a battery reading while on battery rather than on USB, since every reading so far has been at 100 per cent and full. If the amplifier's silence is eventually traced to the rail, that byte is where it will show. Audio itself stays parked at the operator's direction and the state to resume from is recorded in entry 12: the build with Philips framing, stereo and a 200 ms tone was flashed and reported nothing audible, and the three remaining candidates are the rail, the I2S format against LilyGo's ALL_RIGHT, and the amplifier not being fitted. For the TinyDesk author, this cycle adds a second item to the conversation entry 12 opened: the `bell` callback is one minor API change and `batteries()` is another, and both would be better raised together than one at a time, with the note that `batteries()` deliberately copies `drives()` so the author has one pattern to judge rather than two. Also open and unchanged: the top margin is the last unpainted region; the deferred question of what the margins should do under a full-screen application waits for something to go full screen; `board.example.conf` still calls the SD keys unproven when they now work; and the pre-existing YAML defect in `.ai/core-reference.md`, where a code fence after the last record stops the sequence parsing as one document, remains unfixed. A fresh backup was taken at 0009 and verifies; this cycle and the audio one before it have changed the firmware since, so another is due before anything risky. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- apps/sysmon.c
- include/tinydesk/td_sysinfo.h
- include/tinydesk/td_config.h
- ports/esp_idf/app/main.c
- ports/twatchultra/main/power.c
- ports/twatchultra/main/power.h
- ports/twatchultra/main/sound.c
- api/tinydesk.api

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 12 COMMIT Unreleased 2026-11-12T00:19:17-07:00

#### Coming From:

Unreleased 95591b2

#### Purpose:

Record the speaker probe: a BEL from the terminal driving the MAX98357A, which is the first time anything on this project has touched that amplifier.

#### Outcome:

At dd73478 on branch `t-watch-ultra`, `py -c "print(chr(7))"` in the Terminal emits a BEL that reaches the speaker amplifier, and the whole software chain reports success: the boot log carries `PMU bldo2_v=1...`, `twatch-sound: i2s 16000Hz b...` and `link_twatch: sound on`, with no write failure from the beep. Whether a sound is actually audible is not something this session can establish; that is the operator's to report. The port's shape came entirely from LilyGo, because theirs is the only code that has ever driven this part: `AudioOutputDev::begin()` in LilyGoLib's `src/audio/AudioDevice.cpp` gives I2S master TX, 16-bit samples, four DMA buffers of 1024, `tx_desc_auto_clear`, and pins bclk 9, ws 10, dout 11, and `LilyGoWatchUltra.cpp` raises the supply in its PMU bring-up as `setVoltage(CH_BLDO2, 3300)` and `enable(CH_BLDO2, true)` beside the display's ALDO2 and the card's ALDO1. BRD-003 independently records BLDO2 as the speaker's rail, so the two agree. Two departures were necessary. LilyGo has no tone generator at all -- their audio path plays WAV files -- so a square-wave generator was written: 1 kHz for 80 ms, sixteen samples per period at 16 kHz, which is what an IBM PC speaker was and needs no filter or table. And LilyGo uses the legacy `i2s_driver_install()`; ESP-IDF 5.3.1 keeps `driver/i2s.h` only under `components/driver/deprecated`, so this uses `i2s_std` with `i2s_new_channel`, `i2s_channel_init_std_mode` and `i2s_channel_enable`, with the same settings and a mono slot where LilyGo used ALL_RIGHT. THE BEL HOOK IS THE ONE SHARED-CODE CHANGE IN THIS CYCLE, and it was made deliberately rather than by scanning bytes in the port. `td_vterm_t` gains an optional `bell(void *)` callback beside the existing `dcs`, and `vterm.c` calls it for a BEL in the ground state only. That distinction is load-bearing: an OSC string is terminated by BEL, so a terminal that rang on every 0x07 would buzz every time an application set its window title, which is a bug that would have looked like a spurious beep under load. The alternative -- a shadow scanner in the port that tracked whether it was inside a sequence -- would have duplicated parser state that this project has already watched drift apart twice, and the callback is the same shape the renderer already uses for DCS. It is a struct member appended at the end, which `STABILITY.md` classes as a minor change, so `api/tinydesk.api` is re-recorded and the suite is green at 38 of 38. `TD_VERSION` was NOT bumped: this fork would be claiming an upstream release number and that is the author's decision, not this port's. The ordering lesson from the last three cycles repeated once more and was applied this time before the failure rather than after it: `board_speaker_rail_on()` runs before the I2S channel is created, because a class-D amplifier with its supply down clocks in every sample and outputs silence, which is indistinguishable from a wrong I2S configuration. Where this differs from the haptic and SD cases is that here the symptom would have been "no sound" in a system that has never made sound, so nothing would have been suspected at all.

#### Next Steps:

The operator needs to run `py -c "print(chr(7))"` in the Terminal and say whether a beep is heard. If it is, the hardware is proven and the question becomes what the desktop wants from it, which the operator is already taking to TinyDesk's author: the BEL callback is the whole of what this port offers, and sampled audio would be a new interface rather than a port task, since `td_sysinfo_t` has no sound ops and the public headers have nothing audio-shaped. If it is not heard, the three candidates in order are the rail (the log line reports `bldo2_v` and the enable byte, so it is checkable rather than guessed), the I2S format (a mono slot against LilyGo's ALL_RIGHT is the one deliberate difference, and it is a one-line change), and the amplifier's own SD pin mode, which sets its channel behaviour and is not wired here -- LilyGo leaves it unconfigured too, so if that is the fault it is a shared one. One structural consequence of the shared-code change is worth recording for the author conversation: adding a member to `td_vterm_t` is a minor API bump by this tree's own policy, so the BEL callback cannot land in a 1.x release without one, and an alternative shape the author might prefer -- a function rather than a struct member, or a hook on `td_sysinfo_t` -- would avoid that. Everything else outstanding is unchanged: a fresh backup was taken at 0009 tonight and its `verify_flash` now passes because the script holds the board in the bootloader across the read and the compare; the top margin remains the last unpainted region; the deferred question of what the margins should do under a full-screen application waits for something to go full screen; `board.example.conf` still calls the SD keys unproven and is now wrong; and the pre-existing YAML defect in `.ai/core-reference.md`, where a code fence after the last record stops the sequence parsing as one document, remains. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/twatchultra/main/sound.c
- ports/twatchultra/main/sound.h
- ports/twatchultra/main/power.c
- ports/twatchultra/main/power.h
- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- include/tinydesk/td_vterm.h
- src/vterm.c
- api/tinydesk.api

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 11 COMMIT Unreleased 2026-11-11T23:57:09-07:00

#### Coming From:

Unreleased 12e7d06

#### Purpose:

Record a 128 GB card mounting, which settles the capacity question that LilyGo's documentation leaves at 32 GB.

#### Outcome:

A 128 GB FAT32 card mounts in the TinyDesk port and works, with no change to the code or the configuration: `sd` reports `SM128`, SDHC/SDXC, 119 GB with 119 GB free, a file written from the shell read back byte-exact, a second file of 494 bytes written and listed, and the desktop's Files app shows both alongside `TinyDesk`. The 29 GB card that preceded it mounted through the same path. So the answer to whether this port can use more than 32 GB is yes, and the reasons the question arises at all are three conventions stacked on top of each other rather than any limit in the stack. THE FILESYSTEM IS THE ONLY REAL CONSTRAINT. ESP-IDF 5.3 builds FatFs with `FF_FS_EXFAT` hardcoded to 0 in `ffconf.h` and exposes no Kconfig symbol for it, so an exFAT volume cannot mount at any capacity; FAT32 is the requirement and the size is not. The 32 GB figure comes from the SD Association naming 2 to 32 GB as SDHC and above that as SDXC, from cards above 32 GB usually shipping exFAT, and from Windows refusing to format FAT32 above 32 GB -- the last being a limitation of their format tool, which is why `mkfs.vfat` on Linux produced the 128 GB card tested here. THE HEADROOM IS ORDERS OF MAGNITUDE. `sdmmc` holds capacity as uint32 sectors and `FF_LBA64` is 0, which together put the ceiling at 2.2 TB; the card used 250 million sectors of the 4.3 billion available. FAT32's cluster ceiling is 2^28 minus one, and at the 32 KB clusters this card has, that is about 8.8 TB against the 3.9 million clusters in use, a factor of 68. Nothing in the reporting path truncates either: `td_drive_t.total` and `free`, `tdsh_sdcard_space()` and the shell's `print_size()` are all 64 bit. All of that was worked out before the test by reading the stack rather than assumed, and it is recorded in BRD-004 along with the result, but the arithmetic is the weaker half of the evidence -- the same kind of reasoning once said the second SPI bus worked, and it took two bugs to make that true, which is why the card was actually put in. One observation from the test rather than from the design: the shell's `write` command truncates a long argument line at roughly 500 bytes, so it is not a way to check large-file writes, and it should not be read as a filesystem limit. Larger writes remain unexercised, as do Store installs to the card and the image cache at `/sd/TinyDesk/cache`. Verified on the glass: mount, write, read back, listing, and Files navigation, then the test files removed and the card left with only `TinyDesk`. This cycle changed no code; its only repository change is the BRD-004 record, which now states the capacity result and keeps the limits beside it.

#### Next Steps:

The card works at both sizes, so what remains is exercising the things that use it rather than the card itself. In order of value: Store installs to the SD location and the `tde`/`tdapp` image cache at `/sd/TinyDesk/cache`, neither of which has run against a real card at any capacity; and a genuinely large file, since the only writes tested were under a kilobyte and the shell's `write` truncates its own argument line at about 500 bytes, so it cannot be the instrument. Hot-plug is still untested in either direction and is the weakest part of this feature: `sd.cd` is not configured, so removal is detected by polling the card rather than by the insert switch, and the switch is expander pin 10, which is free now that the bogus touch-reset hedge is gone -- wiring `sd.cd` to it would be better than polling and is a small change. Two bookkeeping items are now overdue. A fresh 16 MiB backup, since the last one predates the haptics, the glass sweep, the Escape change and the SD work. And `board.example.conf` still calls the SD keys unproven, which is now false; `board.conf` is gitignored, so a clean checkout builds without SD and reads that stale note as current. Also open from before: `verify_flash` still lives outside `scripts/watch-backup.sh`; the top margin is the last unpainted region; the deferred question of what the margins should do under a full-screen application waits for something to go full screen; and the pre-existing YAML defect in `.ai/core-reference.md` where a code fence after the last record stops the sequence parsing as one document. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 10 COMMIT Unreleased 2026-11-11T23:49:20-07:00

#### Coming From:

Unreleased 8843136

#### Purpose:

Record the SD card working in the TinyDesk port, which answers the open hardware question that BRD-004 had carried since 2026-11-09.

#### Outcome:

At 2d80ea5 on branch `t-watch-ultra`, a 29 GB FAT32 card mounts at `/sd` in the TinyDesk port, reads and writes files, and appears in the desktop's Files app as the folder `sd`. The last of those needed no new code: the shell already had `td_fs_stdio_redirect`, which serves `/fs/sd` from `/sd` while the card is mounted, and register it alongside a removable drive that reports real capacity. So the work was entirely in getting the card to mount at all, and two of the three things that had to be right were wrong. THE SHARED SPI BUS WAS NEVER PARKED, and this is the finding worth keeping: MISO, MOSI and SCLK are shared with the LoRa radio on CS 36 and the NFC reader on CS 4, neither of which this port had ever driven, so either could float low and leave its device driving MISO. That corrupts every transaction and presents as a handshake failure rather than as a bus that does not work. LilyGo's own `initShareSPIPins()` drives those pins high before using the bus; this port had no reason to before there was anything on the second bus. Parking them moved the failure from `sdmmc_init_sd_if_cond returned 0x108`, where the card does not answer at all, to `failed to mount card (13)`, where the card has answered and the filesystem is not recognised -- which is the evidence that it mattered rather than a plausible-sounding guess. THE TOUCH RESET WAS DRIVING THE CARD-DETECT PIN. `board_touch_reset_release()` hedged between two sources by driving expander pins 8 and 10, on the reasoning that a wrong pin is an unused pin. It is not: the board's own Arduino variant defines `EXPANDS_TOUCH_RST` as 8 and `EXPANDS_SD_DET` as 10, so the hedge was driving the SD card's insert switch as an output and fighting a physical switch. Only pin 8 is driven now, and the hedge's premise is recorded as false rather than deleted. Third, ALDO1: BRD-003 assigns it to the card and LilyGo raises it to 3300 mV beside the display's ALDO2, so `board_sd_rail_on()` does the same. It has to run before the shell's SD watcher, which mounts from a task rather than during init, so there is a new weak `board_power_rails_preinit()` in the shared app called from `app_main`; the port's own `link_init()` runs later, from the UI task, and would have been too late. FATFS also gained long file names and UTF-8 to match every other port, and that had to go in `sdkconfig.defaults` with the generated `sdkconfig` deleted, because a change made only in `sdkconfig` vanishes on a clean checkout with no diff to blame. Two things about method are worth carrying. The first: three separate diagnostic instruments were needed before the fault was visible, and one of them had to be built. `ESP_LOG` reaches nothing on USB in this build (`CONFIG_ESP_CONSOLE_NONE=y`), so the boot log exists only in the desktop's 64-line ring, and every diagnostic this port had written at preinit was scrolled away before anyone could read it. `board_log_rails()` now reads ALDO1 and ALDO2 back and logs them from `link_init`, which is late enough to survive and still early enough to precede the SD watcher's first attempt; it reported `ald1 1c`, which is 28, which is 3300 mV. A diagnostic nobody can read is not a diagnostic. The second: I was wrong twice about the cause and each time the card was cleared as a suspect on evidence rather than by argument. The first hypothesis was exFAT, because a 32 GB card often ships that way and ESP-IDF compiles FatFs with `FF_FS_EXFAT` hardcoded to 0 and no Kconfig option; mounting it on a PC showed FAT32 with an MBR partition table, so that was wrong. The second was the sector size, `CONFIG_FATFS_SECTOR_4096`, which I suspected would reject a 512-byte-sector card; reading `ffconf.h` showed `FF_MIN_SS` is the minimum of the SD and wear-levelling sizes, so 512 stays valid and that was wrong too. BRD-004 has been rewritten rather than left stale: its verdict that the second bus had never moved a byte was measured on Linux's `spi-mem` stack and does not carry over, and the record now states what was proven, on which stack, and what the two prerequisites are. Verified end to end on the glass: `sd` reports `mounted at /sd`, `SDABC`, SDHC/SDXC, 29 GB with 29 GB free; a file written from the shell read back byte-exact; Files navigated to its root shows `sd/` and opening it shows `TinyDesk/` and the probe file; the probe file was then removed and the card left with only `TinyDesk`. Host suite 38 of 38, four sections hash-verified on flash.

#### Next Steps:

The card works, so the next question is what to put on it rather than whether it can hold anything. The Store already installs packages to the card when the user picks it, and `tde`/`tdapp` caches images at `/sd/TinyDesk/cache`; neither has been exercised against a real card yet, and they are the natural first users. The `sd.*` board keys are absent from `api/board_keys.txt` because the checker skips `third_party/`, so they are outside the documented board-config surface -- worth recording if this port is ever offered upstream. Hot-plug is untested in both directions: the card was present at boot, and `sd.cd` is not configured on this port, so removal is detected by polling the card rather than by the insert switch. Note that the insert switch is expander pin 10 and the port does not currently read it, which means hot-plug would work better with `sd.cd` wired to the expander than with the polling path. Two limitations of the state rather than of the code are worth carrying: `board.conf` is not built in because it is gitignored, so a clean checkout of this port will build without SD enabled and the shipped `board.example.conf` still calls those keys unproven -- that note is now wrong and should be updated when the feature is adopted rather than probed. And the pre-existing YAML defect in `.ai/core-reference.md` remains: a code fence after the last record stops the sequence parsing as one document, so the records can only be validated individually. That predates this cycle and is not corrected here. The other open items are unchanged: a fresh 16 MiB backup is due, `verify_flash` still lives outside `scripts/watch-backup.sh`, the top margin is the last unpainted region, and the deferred question of what the margins should do under a full-screen application waits for something to go full screen. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/esp_idf/app/main.c
- ports/twatchultra/main/power.c
- ports/twatchultra/main/power.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/sdkconfig.defaults

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 9 COMMIT Unreleased 2026-11-11T23:15:08-07:00

#### Coming From:

Unreleased f66dd22

#### Purpose:

Capture a full 16 MiB backup of the state the operator accepted, in which haptics work and the glass sweep is a committed test.

#### Outcome:

`~/twatch-backup/working-2026-11-11-2310.bin` is the current recoverable state, 16 MiB read at 583.1 kbit/s in 230.2 seconds, sha256 `39b6ac8b3dbbff4c57049420ba62ae4a0ece5b86d4ea4e3580f25069e6e3a956`. It carries TinyDesk at dc9414e on branch `t-watch-ultra`, the build in which the three margins are painted, labelled and live, the DRV2605 buzzes on a press, Escape no longer opens the Start menu on a bare desktop, and the glass sweep exists as `tests/test_twatch_edges.c` rather than as a program in a temporary directory. The read used `scripts/watch-backup.sh`, which is now its second consecutive use after the hand-rolled read in entry 96 that the log recorded as a mistake not to repeat. Four checks confirm the image is the current build rather than an older one, and the order matters because the strongest is not the one the script does. `esptool verify_flash` read the chip back a second time and compared it against the file with the digest matched, which states that the file equals the flash rather than that the file equals itself. The `.sha256` written by the script verifies. The 2,535,920-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `80511df1`. Both image magics are 0xe9, so neither the bootloader nor the app region is a truncated read, and the partition table parses as nvs at 0x9000, phy_init at 0xf000, ota_0 and ota_1 at 0x10000 and 0x310000, otadata at 0x610000 and storage at 0x620000. The storage partition holds 1,988 non-empty 4K blocks, unchanged from the previous two backups, so the desktop's own filesystem came with the firmware and nothing in it has moved. The board was confirmed running after the read's trailing hard reset by the USB port producing the desktop's own terminal escape sequences rather than ROM output. Nothing in this repository changed; the image is a recovery artifact outside it, so Files Modified is None by the syntax's own rule about generated binaries, and Build and Deployment are N/A because this cycle built and flashed nothing. One limitation is worth stating plainly rather than leaving implied: the backup is taken after the flash and not after a cold boot with every subsystem re-read, so it records the running state rather than a freshly initialised one. That is the same shape as the two previous backups and it is adequate for recovery, because the flash contents do not depend on how the board got there; what it does not capture is any diagnostic evidence about a cold-boot sequence, and none was needed. `verify_flash` still lives outside `scripts/watch-backup.sh` and is still something a person has to remember, which entry 7 named as the next thing to fix and which this cycle did not fix.

#### Next Steps:

The immediate work is a small one that has now been deferred twice: putting `verify_flash` inside `scripts/watch-backup.sh`, where it costs one more pass over the flash and turns the strongest available check from a habit into a guarantee. Everything else outstanding is unchanged and unblocked. The top margin is the last unpainted region, inert as `EDGE_TOP` and intended for status icons. The operator deferred the question of whether the three margins should stop being painted while a full-screen application owns the screen, on the grounds that no application in this tree goes full screen today; that decision is recorded in entry 8 with both options and the reasoning, and it should be reopened when something first goes full screen rather than kept indefinitely. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` untested against the current tree, and the RTC write path written but never exercised by setting a time and power-cycling. Now that the sweep is a committed test, the port has a regression net for the margins that did not exist before this run of work, so the next margin change can be attempted with a check behind it rather than only with a render at 1:1; the painter's own shapes remain outside that net, since `display.c` cannot compile on a host. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 8 COMMIT Unreleased 2026-11-11T23:09:37-07:00

#### Coming From:

Unreleased 675e39c

#### Purpose:

Record Escape no longer opening the Start menu on a bare desktop, and the operator's decision to defer the margin-visibility question rather than settle it now.

#### Outcome:

At dc9414e on branch `t-watch-ultra`, `src/wm.c` opens the Start menu on a bare desktop for Enter alone; Escape no longer does. The pair was a keyboard affordance that made sense when a keyboard was the only way to reach the launcher, and it stopped making sense once this port gave Escape a labelled button in the left column and put a Start button in the same column: a key whose entire meaning is "get me out" was opening the launcher, while Escape is already the cancel key everywhere else in the window manager, closing dialogs and backing out of menus. The path that reaches the changed line is worth stating because it is not obvious from either side. With no window focused, `dispatch_key()` offers the key to `desktop_key()` first, and that function returns false immediately when desktop icons are off -- which is this watch's state, since the operator switched them off -- so control fell through to a line that toggled the menu for either key. With icons on, `desktop_key()` claims Escape first to clear the icon selection, which was and remains sensible. So the fault only appeared in the configuration this board actually runs, which is why it survived until a button was bound to the key. Nothing was lost by the change: Enter still opens the menu on a bare desktop, F10 opens it from anywhere and is what the left column's top button sends, the taskbar has its Start button, and a desktop icon can still be run from the keyboard. Tested rather than assumed: `test_bare_desktop_escape` in `tests/test_wm.c` fails three assertions when the old Enter-or-Escape behaviour is restored and passes with the fix, and it checks that F10 still toggles the menu as well, since F10 is the key the top-left button sends and a change here could quietly have taken that away. The operator also decided the second open question, on whether the three margins should stop being painted while a full-screen application owns the screen, and chose to defer it: no application in this tree goes full screen today, so the choice is theoretical until one does, and the honest thing is to leave it recorded rather than write code no test can exercise. Verified: host suite 38 of 38 with the new test, and the firmware built and flashed with four sections hash-verified, since this change is in shared code rather than in a test.

#### Next Steps:

The operator should confirm on the glass that the Escape button now does nothing on a bare desktop while F10 and Enter still open the launcher; that is a two-press check and the only part of this cycle not yet observed. Everything else outstanding is unchanged. A fresh 16 MiB backup is due, since `working-2026-11-11-2242.bin` now predates the haptic driver, the glass-sweep test and this fix. `scripts/watch-backup.sh` should gain the `verify_flash` step for the same reason it did last time, and the top margin remains the last unpainted region, inert as `EDGE_TOP` and intended for status icons. The deferred margin-visibility question is recorded rather than dropped: when an application first goes full screen, the choice is to blank the margins on the transition and restore them on exit, which is truthful because pressable-looking dead buttons are worse than blank glass, or to keep them painted and working, which risks feeding stray F10, Escape and F6 to an application that does not understand them. Note that every app in this tree opens maximised rather than full screen, and `td_wm_app_owns_screen()` returns false for maximised, so the margins work correctly in normal use and the question only arises for something genuinely full screen. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` untested against the current tree, and the RTC write path written but never exercised. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- src/wm.c
- tests/test_wm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 7 COMMIT Unreleased 2026-11-11T23:03:40-07:00

#### Coming From:

Unreleased 58901aa

#### Purpose:

Record the glass sweep becoming a committed test in the port rather than a program in a temporary directory.

#### Outcome:

At a53520b on branch `t-watch-ultra`, `tests/test_twatch_edges.c` walks all 205,820 points of the visible glass in panel coordinates and checks the margin hit test against a reference classifier, with 1536 assertions and no failures. The reason it belongs in the tree is that three faults in this subject matter reached the glass and every one was arithmetic that a desk could have found: the hit test comparing panel coordinates against controller coordinates, the inverse of the slot formula dividing by the span instead of inverting the floor, and the two columns being compared as an offset rather than a mirror. The design decision that makes the test worth having is where the reference comes from. A reference built from `edges.h`'s own macros would prove only that the code equals itself, so the reference restates the measured hardware instead -- a 410x502 glass, a 320x416 framebuffer inset 45 pixels on each side, and panel RAM 22 columns wider than the visible glass -- and derives everything else from those four facts. The code is then checked against the result, which means the macros can be wrong and the test will say so. The slot boundaries are likewise written as a table of the layout, 45/128/211/294/377/461 for the column and 67/173/280/387 for the strip, rather than as a formula the code already contains; the uneven distribution of the leftover rows and columns is visible on the glass, so it is stated rather than recomputed. Coverage is the geometry constants against the facts, the slot boundaries forward and backward, the exact inverse across every row and column of every slot, no index jumping as a finger moves, clamping at the ends, the panel-to-controller conversion stated as its own named test, and a 180-degree mirror between the two columns. Three negative controls were run and each reintroduces a fault that actually shipped, which is the only evidence that a test is not vacuous. Removing the coordinate conversion produces 30,250 region disagreements with each column keeping a claim on only 9,568 of its 18,720 points, 51 per cent dead, and that is the fault the operator reported as the left column not registering at all. Replacing the inverse with the obvious division produces 442 index disagreements and is caught first by the boundary round-trip. Shifting the right column's bounds by two pixels produces 922 region disagreements. The tree was restored after each and the final run is clean. One limitation is recorded rather than papered over: the test covers `edges.h` and not `display.c`, because the painter needs ESP-IDF's SPI headers and cannot be compiled on a host. The painter and the hit test share `edges.h` for the numbers, so the fault class this test is aimed at -- two copies of the geometry drifting apart -- is covered, but a fault inside the painter's own shape arithmetic is not. Verified: host suite now 38 of 38 including the new test. Deployment and User Test are N/A rather than PASS because nothing in the firmware changed -- this cycle added a test and its CMake entry and flashed nothing -- and the syntax does not let an agent-run check stand in for a hardware acceptance.

#### Next Steps:

The two items that were outstanding alongside this one are unchanged and both are small: a fresh 16 MiB backup, since `working-2026-11-11-2242.bin` now predates the haptic driver and this test; and the `verify_flash` step inside `scripts/watch-backup.sh`, which would turn the strongest available check from something a person remembers into something the tool guarantees. The top margin is still the last unpainted region and inert as `EDGE_TOP`, intended for status icons. Two questions still want decisions rather than code: Esc on a bare desktop opens the Start menu, and the margin buttons stay painted while a full-screen application owns the screen where they are no longer buttons. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` untested against the current tree, and the RTC write path written but never exercised. Worth noting for whoever picks up the painter next: because this test cannot reach `display.c`, a change to the shapes should still be rendered at 1:1 on the host before flashing, which is what the last four cycles did and what caught the reversed notch test and the wrong mirror formula. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- tests/test_twatch_edges.c
- CMakeLists.txt

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 6 COMMIT Unreleased 2026-11-11T23:00:19-07:00

#### Coming From:

Unreleased cbd38fd

#### Purpose:

Record the operator's acceptance of the haptic feedback, which entry 5 left as NOT RUN because the buzz had not been felt when it was written.

#### Outcome:

The operator reports that the margin buttons buzz and that it works perfectly, and accepted it without asking for a change. Entry 5 is therefore superseded on one point only: its User Test field reads NOT RUN, which was accurate when it was written and is left that way because a committed entry is settled history and the syntax requires later results to be recorded in the next entry rather than appended to the old one. Everything else in entry 5 stands as written, including the three failure modes it predicted and their distinguishing log lines. The acceptance also settles the two questions that entry 5 left open for the operator to answer. The chip is present and configured, since a missing DRV2605 or a failed rail would have logged its own line instead of buzzing. The effect is at the right strength for a button, since the operator described it as working rather than as weak or as a double buzz, so neither the effect ID within library 1 nor RATED_VOLTAGE and the overdrive clamp need touching; those remain unset, as they are in LilyGo's own driver, and are the knobs for a later cycle if the feel ever needs changing. Nothing in either repository changed for this cycle, so Files Modified is None, and the build and deployment fields are N/A because nothing was built or flashed. One thing this confirms beyond the feature itself: the bring-up ordering is correct, meaning the XL9555 P06 rail is genuinely raised before the chip is probed. That ordering was written from LilyGo's source and from BRD-005's warning that a chip with its supply off still answers I2C, which meant the failure would have been silent and looked like a dead motor; a working buzz is the evidence that the warning was heeded rather than merely quoted.

#### Next Steps:

A fresh 16 MiB backup is now the right next move, since `working-2026-11-11-2242.bin` predates both this driver and the two cycles before it. After that the remaining work is unchanged and none of it is blocked: the top margin is the last unpainted region and still inert as `EDGE_TOP`, intended for status icons; the glass sweep that has caught three faults plus one in its own reference should be landed as a real test rather than left in a temporary directory; and `scripts/watch-backup.sh` should gain the `verify_flash` step that the last two hand-checked backups used, so the strongest available check becomes something the tool guarantees rather than something a person remembers. Two questions still want decisions rather than code: Esc on a bare desktop opens the Start menu, which was sensible while Enter and Esc were a keyboard pair for the icon grid and is odd now that Esc means cancel, and the margin buttons stay painted while a full-screen application owns the screen where they are no longer buttons. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` untested against the current tree, and the RTC write path written but never exercised by setting a time and power-cycling. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 5 COMMIT Unreleased 2026-11-11T22:58:10-07:00

#### Coming From:

Unreleased 9099e29

#### Purpose:

Record the DRV2605 haptic driver in the TinyDesk port, written from LilyGo's own source so that a press on a margin button can be felt.

#### Outcome:

At a1b6640 on branch `t-watch-ultra`, `ports/twatchultra/main/haptic.c` drives the DRV2605 at I2C 0x5A and `edge_press()` fires one Strong Click after sending the key. The click fires only when a key was actually sent, so the reserved right-column slot and the inert top margin stay silent: buzzing for a no-op would say something happened when nothing did. It fires after the key rather than before, so the click coincides with the action instead of announcing it. The sequence was read from LilyGo's source rather than inferred from the datasheet or copied from the Linux driver, and doing so settled the one design question the Linux version had left open. LilyGoLib's `LilyGoWatchUltra.cpp` splits the work in two: `initDrv()` calls `selectLibrary(1)`, `setMode(HapticMode::INTERNAL_TRIGGER)`, `setWaveform(0, effect)`, `setWaveform(1, 0)` and `run()`; `vibrator()` then only rewrites the two waveform slots and fires GO. So the library and the trigger mode are set once and a tap is three register writes, not six. This port does the same, where the Linux driver rewrites the whole sequence per tap — both work, and LilyGo's split is the better shape because library and mode are configuration while the waveform is payload. The underlying register class is `lewisxhe/SensorLib`, `src/haptic/HapticDriver_DRV2605.cpp`, where the relevant writes are 0x03 for the library, 0x01 = 0x00 for internal trigger, 0x04 plus the slot index for a waveform, and 1 to 0x0C for GO. Two silent failures were handled rather than assumed, and both are cases where the hardware looks dead while reporting healthy. The first is the effect library: this board's FEEDBACK register at 0x1A reads 0x36 whose bit 7 is 0, so the motor is ERM and the library must be 1, not 6 — an ERM given the LRA library buzzes weakly or not at all with no error anywhere. The driver reads that register back at init and logs what it found rather than trusting the constant. The second is the rail, and it is the one that reads as broken hardware: the chip answers every I2C transaction with its supply switched off, so a driver that only probes it reports success and then nothing moves. `board_haptic_power_on()` now raises XL9555 P06 before the probe, a bit confirmed from two independent sources — LilyGoLib's `POWER_HAPTIC_DRIVER` case driving `EXPANDS_DRV_EN`, and this project's own reference citing the same value from TinyPetz — and unlike the touch reset line there was no disagreement to hedge. Bring-up logs which of the three outcomes happened: rail failed, no chip at 0x5A, or haptics on. One limitation shaped how far this can be verified from a desk rather than a finger: `CONFIG_ESP_CONSOLE_NONE=y` in this build, so `ESP_LOG` reaches nothing on USB and the bring-up line exists only in the desktop's Log Viewer ring. A reset over native USB re-enumerates and the early lines are dropped anyway, which is why an attempt to capture the boot log produced no haptic line at all — not a fault, and worth recording so the next session does not read that silence as a failure. Verified: host suite 37 of 37, four sections hash-verified on flash, and the built image carries `board_haptic_power_on`, `haptic_init` and `haptic_tap` with their diagnostic strings. The buzz itself is the operator's to report.

#### Next Steps:

The operator needs to press a margin button and say whether it is felt, and if it is not, the three outcomes are distinguishable from the Log Viewer's bring-up line rather than by guessing. If it is felt but too weak, the knobs are the effect ID within library 1 — LilyGo exposes `setHapticEffects` for exactly that and defaults to 1 as this port does — and after that RATED_VOLTAGE and the overdrive clamp, which neither LilyGo nor this port sets yet. If it is felt but fires twice per press, the cause would be the press being emitted twice rather than the effect being long, since the driver fires only on the transition into a press. Beyond haptics, the top margin is still the last unpainted region and inert as `EDGE_TOP`; the glass sweep that has caught three faults plus one in its own reference still lives only in a temporary directory; `scripts/watch-backup.sh` still lacks the `verify_flash` step; and the two open questions still want decisions rather than code, namely that Esc on a bare desktop opens the Start menu and that the margin buttons stay painted while a full-screen application owns the screen. Also still open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` untested against the current tree, and the RTC write path written but not exercised. The backup at `working-2026-11-11-2242.bin` predates this cycle, so a fresh one is due once the buzz is confirmed. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/twatchultra/main/haptic.c
- ports/twatchultra/main/haptic.h
- ports/twatchultra/main/touch.c
- ports/twatchultra/main/power.c
- ports/twatchultra/main/power.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 4 COMMIT Unreleased 2026-11-11T22:46:47-07:00

#### Coming From:

Unreleased 0510262

#### Purpose:

Capture a full 16 MiB backup of the state the operator adopted as the new baseline, in which all three margins are painted, labelled and live.

#### Outcome:

`~/twatch-backup/working-2026-11-11-2242.bin` is the current recoverable state, 16 MiB read at 584.2 kbit/s in 229.8 seconds, sha256 `d731a1f90bcc300260a48e5c427c31339a59579ccf4bb367363c3b7b1b60b856`. It carries TinyDesk at 6c32d28 on branch `t-watch-ultra`, the build in which the left column reads F10, Esc, F6, Super+D and F11 stood on end and upright from the right-hand edge, the bottom strip reads Tab, Enter and Ctrl+Q, and the right column carries a reserved slot and four arrows, all of them dark grey faces with a violet border and terminal-green labels. The read was taken with `scripts/watch-backup.sh` rather than by hand, which is the first time that script has been used since entry 96 noted it was skipped; its 921600 baud took the read from 406 seconds to 230 and its own size and hash checks ran as written. What the script does not do is the check that matters most, so it was run afterwards: `esptool verify_flash` read the chip back a second time and compared it against the file, digest matched, which is a stronger statement than a stored hash because it says the file equals the flash rather than that the file equals itself. Three further checks confirmed the image holds the current build rather than an older one. The 2,534,800-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `69df4a19`. The bootloader and the app both carry the 0xe9 image magic, so neither region is a truncated or empty read. The partition table parses as nvs at 0x9000, phy_init at 0xf000, ota_0 and ota_1 at 0x10000 and 0x310000, otadata at 0x610000 and storage at 0x620000, and the storage partition holds 1,988 non-empty 4K blocks, so the desktop's own filesystem came with it and not just the firmware. The board was confirmed running after the read's trailing hard reset: the USB port produced the desktop's own terminal escape sequences, which is the application drawing rather than the ROM talking. Nothing in this repository changed; the image is a recovery artifact, so Files Modified is None by the syntax's own rule about generated binaries, and the build and deployment stages are N/A because this cycle deployed nothing. One note for the next backup rather than a change made here: `verify_flash` belongs inside `scripts/watch-backup.sh`, where it would cost one more pass and turn the strongest available check from something a person remembers into something the tool guarantees.

#### Next Steps:

The backup now matches what ships, so the baseline is safe and the next cycle can take a risk. The most valuable thing outstanding is unchanged: the sweep that walks every point of the glass and compares the hit test against an independent implementation in controller coordinates has caught three faults across two cycles and one more in its own reference, and it still lives in a temporary directory. It belongs in the port as a real test, and it is pure arithmetic so it needs no hardware. `scripts/watch-backup.sh` should gain the `verify_flash` step described above. The top margin is the last unpainted region, inert as `EDGE_TOP` and intended for status icons. Two questions still want a decision rather than code: Esc on a bare desktop opens the Start menu, which was sensible while Enter and Esc were a keyboard pair for the icon grid and is odd now that Esc means cancel, and the margin buttons stay painted while a full-screen application owns the screen where they are no longer buttons. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` still untested against the current tree, and the RTC write path written but not exercised. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 3 COMMIT Unreleased 2026-11-11T22:39:41-07:00

#### Coming From:

Unreleased 2b2182e

#### Purpose:

Record the left column's labels turning through 180 degrees so they are upright when the watch is read from the opposite edge.

#### Outcome:

At 6c32d28 on branch `t-watch-ultra`, the left column's labels are rotated clockwise rather than counter-clockwise: they read top to bottom, with each character upright with respect to the right-hand edge of the watch instead of the left. That is the same column seen from the other side rather than a different arrangement, and the operator asked for exactly that after seeing the first version on the glass. The implementation change is small but it corrected a modelling mistake: rotation was a bool, which can say whether a label is stood on end but cannot say which way it is facing, and those are two different quarter turns that read in opposite directions. It is now a three-state enum of upright, counter-clockwise and clockwise, so the direction is a property of the label rather than something a reader has to infer from a flag. Because the two rotations are exact 180-degree opposites, the clockwise mapping is the counter-clockwise one turned around: the first character moves from the bottom to the top, the glyph's top edge moves from the left to the right, and the leftmost bit moves from the last row of the character's band to the first. Nothing about the fit changed, because a rotation preserves the bounding box's dimensions -- Super+D still needs 56 of the 61 rows and there are still five to spare. Two checks were run and both are exact rather than approximate, which is the point: differing from an upright label rotated 90 degrees clockwise by zero pixels, and differing from the previous counter-clockwise mapping rotated 180 degrees by zero pixels, across all five labels. The first of those checks failed on its first run with about a hundred differences per label, and the fault was in the check rather than the code: the reference rotation mapped a source pixel to the wrong destination, using an offset on one axis where it needed a swap of both. Deriving it properly -- a point at (sx, sy) lands at (height minus one minus sy, sx) after a quarter turn clockwise -- made all five agree exactly. That is worth recording as the third time in this sequence that a verification harness was wrong before the code was, which is its own pattern: a second implementation is only useful if its own derivation is checked, and the cheap way to check it is to predict what a deliberate error in the first one would look like. Verified at 1:1 on the host before flashing: all five labels fit their faces with Super+D five rows spare, no glyph pixel on a border, none inside a rounded corner, no missing glyphs. Host suite 37 of 37, four sections hash-verified. The operator's look on the glass is the acceptance test and had not been given when this entry was written.

#### Next Steps:

The margins are finished as far as appearance goes. The top margin is the last unpainted region, still inert as `EDGE_TOP` and intended for status icons. The sweep that has now caught three faults across two cycles, and a fourth in the harness itself, still lives only in a temporary directory and is the highest-value thing to land as a real test in the port. A fresh 16 MiB backup is overdue: `working-2026-11-11-2149.bin` predates five cycles of margin work and none of them touched anything below the display layer, but a backup that no longer matches what ships stops being a fallback. Before that, `scripts/watch-backup.sh` should gain the `verify_flash` step that the hand-rolled backup used, since reading the chip back and comparing digests is a stronger statement than trusting a stored hash and costs one more pass. Two questions still want a decision rather than code: Esc on a bare desktop opens the Start menu, and the margin buttons stay painted while a full-screen application owns the screen. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` still untested against the current tree, and the RTC write path written but not exercised. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/edges.h
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 2 COMMIT Unreleased 2026-11-11T22:33:49-07:00

#### Coming From:

Unreleased 69ed9dd

#### Purpose:

Record the left column's five buttons being labelled with their key names, stood on end, and every label taking the terminal's green.

#### Outcome:

At 8b4c4fe on branch `t-watch-ultra`, the left column reads F10, Esc, F6, Super+D and F11, rotated a quarter turn counter-clockwise so each reads bottom to top, and every label on the glass is now the terminal's ANSI green rather than white. The rotation is not a style preference and the arithmetic is why: a column's usable face is 41 px across, and `Super+D` is seven characters at eight pixels, 56 px wide, so it cannot be laid out horizontally at all. Stood on end it needs 56 of the 61 rows a slot offers between the corner radii and 16 of the 41 columns, which is five rows to spare rather than the one pixel `Right` had before the arrows replaced it. The direction is counter-clockwise specifically, so that turning the watch a quarter turn clockwise makes the labels read left to right along what becomes the top row -- the column is designed to be read that way and the other direction would have made every label upside down in that orientation. Implementing the rotation touched more than the table. A label's bounding box swaps its dimensions when rotated, so the centring in `label_place()` swaps with it, and `label_bit()` gained a second mapping that runs backwards through the font table: the glyph's own rows become the pixel's columns and its columns become the pixel's rows, with the first character at the bottom so the string reads upward. The green is read from the port's own ANSI palette at index 2 rather than written as an RGB565 constant -- `0x0660`, which is `{0,205,0}` -- so a label is the same colour the terminal draws when a program asks for green text, and it follows the palette instead of drifting from it. One mistake was made and caught during this cycle and is worth recording because the build caught it rather than a person: the edit that replaced the old empty `left_label()` was anchored by slicing between two comment strings, and the slice swallowed `TOP_Y1` and all four slot-arithmetic functions with it, so the build failed with implicit-declaration errors on `col_slot_at` and `btn_slot_at`. The fix was to restore the file from git and re-apply a single anchored replacement, and the resulting diff touches only the label table. The lesson is that slicing a file between two markers is only safe when the content between them is exactly what is being replaced, and here it was not. Verified at 1:1 on the host before flashing: all five labels fit their faces, Super+D with five rows to spare, no glyph pixel on a border, none inside a rounded corner, and no missing glyphs. Host suite 37 of 37, four sections hash-verified, and all five label strings confirmed present in the built image. The operator's look on the glass is the acceptance test and had not been given when this entry was written.

#### Next Steps:

The three margins are now all painted and all labelled, so the appearance work is done and what remains is behaviour. The top margin is the last unpainted region, classified in `edges.h` as `EDGE_TOP` and inert, intended for status icons. The sweep that caught three faults across two cycles still lives only in a temporary directory and belongs in the port as a real test, which is now the highest-value outstanding item because every margin change since has been verified by a throwaway copy of it. Two questions want a decision rather than code: Esc on a bare desktop opens the Start menu, and the margin buttons stay painted while a full-screen application owns the screen. If the ANSI green reads too dim on the dark grey face, index 10 is the bright green the terminal uses for bold text and it is one constant away in `LABEL_ANSI`. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` still untested against the current tree, the RTC write path written but not exercised, and `scripts/watch-backup.sh` still lacking the `verify_flash` step. The backup at `working-2026-11-11-2149.bin` predates four cycles of margin work, so a fresh one is due before anything risky. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/edges.h
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 1 COMMIT Unreleased 2026-11-11T22:23:33-07:00

#### Coming From:

Unreleased 4c4170b

#### Purpose:

Open a new active log after the hundredth entry, recording the archive that holds the previous one and the state of the project at the handoff.

#### Outcome:

The active log reached its hundred-entry limit and was archived to `archived_logs/core-log-2026-11-11-2223.tar.gz`, 136352 bytes, holding all hundred entries of entries 1 to 100. The archive was checked by extracting it again and hashing the result: `d6ff703a2e1ab52993b155398e051f14f0f926961f0d00242acb071bba4cecfe`, which is the hash of the log as it stood before archiving, and the extracted file still counts a hundred top-level entries. This entry is the handoff and is numbered 1, so numbering continues from here. The project state at the handoff is as follows, and everything in it should be verified against the working tree rather than trusted, because a handoff is a claim and the files are the fact. Escapement, the Linux project, is parked: its last code commit is 8645a82 and everything after it in the log is project log only. The T-Watch Ultra work moved to a standalone fork at `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, which is a port of TinyDesk v0.4.0 and stands 55 commits above that tag; the remote to push to is `fork`, which is `aquasock/tinydesk`, and `origin` is the upstream project and must never be pushed to. The fork's latest commit is 1890bf8. On the glass the port has a 40 by 26 desktop at 320x416 on a 410x502 glass, every app reflowed to that grid with the narrow-layout work list empty in `tests/test_native_apps.c`, the RTC seeding the system clock at boot, and three painted margins that are live touch targets: five buttons down the left column at F10, Esc, F6, Super+D and F11; three across the bottom strip at Tab, Enter and Ctrl+Q; and five down the right column, a reserved slot followed by the four arrow keys. All three margins are dark grey faces with a violet two-pixel border and white labels, the bottom strip's labels being key names and the right column's being arrow glyphs. The touch panel is deliberately not a pointer on the desktop -- a press on the framebuffer does nothing, so the margins are the whole of the input -- while an application that owns the whole screen gets the panel back as an absolute pointer, which is what `td_wm_app_owns_screen()` asks. The geometry of all three margins lives in `ports/twatchultra/main/edges.h` and is shared by the painter and the hit test for a reason the archived log records four times over: when the two were separate they drifted, and the result was a column that did nothing and a strip shifted by twenty-two pixels while every line of code looked correct. A full 16 MiB image of this state is at `~/twatch-backup/working-2026-11-11-2149.bin`, sha256 `74009d2955ccd84dc301b12f376037218da4300cbeb9ae5e611eec4b3684e0e6`, verified by reading the chip back and comparing digests rather than by trusting the stored hash. Status fields are N/A because the rollover itself built and deployed nothing; it is a maintenance action on project metadata, not an engineering cycle.

#### Next Steps:

The equipment for the next several cycles already exists and does not need rebuilding. The left column is the last margin without labels, and it needs only a table: its five duties are all four characters or fewer and the UTF-8 label path already handles them. The top margin is the last unpainted region, classified in `edges.h` as `EDGE_TOP` and inert, intended for status icons. The sweep written in the archived log -- which walks every point of the glass and compares the hit test against an implementation written independently in controller coordinates -- caught three separate faults across two cycles and still lives only in a temporary directory, so it belongs in the port as a real test. Four engineering items remain open and none is urgent: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` which are still untested against the current tree, the RTC write path which compiles and is called but has never been exercised by setting a time and power-cycling, and `scripts/watch-backup.sh`, which lacks the `verify_flash` step that the last backup used and so has a weaker check than it should. Two questions want a decision rather than code: Esc on a bare desktop opens the Start menu, which was sensible while Enter and Esc were a keyboard pair for the icon grid and is odd now that Esc means cancel, and the margin buttons stay painted while a full-screen application owns the screen where they are no longer buttons. The archived log also records deviations from this syntax that were left alone because rewriting settled history is prohibited -- entries 17 and 18 carried annotated status labels and fourteen earlier entries listed `.ai/` files under Files Modified -- and they remain in the archive rather than in the active log. Method worth carrying forward, because it is what the last several cycles actually ran on: measure rather than reason about the hardware, and where a fault is arithmetic, sweep the whole input space against a second implementation instead of probing points on the glass. Three of the last five faults were found that way, and two of them -- the coordinate space and a guessed font absence -- were mistakes in reasoning that a four-line check would have caught.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
