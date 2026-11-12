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
