## 6 COMMIT Unreleased 2026-11-14T21:35:00-07:00

#### Coming From:

Unreleased 18cc0c1

#### Purpose:

Reboot the watch and take a verified full-flash backup of the audio build, which entry 5 recorded as owing but not done.

#### Outcome:

The board was rebooted and a full 16 MiB image archive was taken and verified, so entry 5's statement that no backup had been made is superseded and a future session should not act on it. ENTRY 5 UNDERSTATED THE PROJECT STATE BECAUSE IT WAS WRITTEN BEFORE THE OPERATOR ASKED FOR THE BACKUP, and its `Next Steps` says the cycle did not take the usual reboot and 16 MiB backup and that the last verified image therefore remains `working-2026-11-14-2027.bin`. That was accurate when written and is no longer accurate, and it is corrected here rather than edited in place because a committed entry is settled history. THE REBOOT USED `--before default_reset --after hard_reset`, which the backup script documents as the only form that works from both a running board and one in esptool's stub on this native USB-JTAG port; the board re-enumerated and emitted 1,193 bytes of VT100 within the first seconds, which is the evidence that it came back. THE IMAGE IS `working-2026-11-14-2128.bin`, 16,777,216 bytes exactly, sha256 `24d2366080d576f745dc5027cd2934d40b3e84ef6687c43910d978745abb8b3c`, with the sidecar re-checked afterwards by `sha256sum -c` rather than trusted from its own creation. The read took 228.6 seconds at 587 kbit/s and `verify_flash` reported `verify OK (digest matched)` against the chip, with the board held in the bootloader across both so nothing could write NVS between the read and the compare -- which is the race `watch-backup.sh` exists to close, and the reason a backup that verified four minutes late was found to have three changed blocks on 2026-11-12. The read was confirmed complete at the expected size before the compare was attempted, so a short read could not have been verified as a good image. THE IMAGE CONTAINS THE BUILD THAT WAS FLASHED, ESTABLISHED FROM THE IMAGE ITSELF RATHER THAN ASSUMED: the partition table parsed out of offset 0x8000 lists `ota_0` at 0x10000 with size 0x300000, and the 2,921,856 bytes at that offset hash to `ea4433b490fd4178`, which is `build/tinydesk.bin` from the audio cycle byte for byte. So the archived image is the Game Boy Color port with sound, not a pre-audio image. The script also confirmed the board alive after its own reset and the peripheral port answered afterwards, so this cycle did not repeat the failure entries 15 and 20 record, where a board was left silent while the script exited 0. WHAT THE IMAGE DOES NOT HOLD is the SD card: the ROMs, the captures, the saves directory and the card's layout are device state outside the 16 MiB flash and are described only by the `README.md` at the card's root. The required core-syntax audit was run before this commit and passed: `.ai/core.md` was confirmed unchanged, `.ai/core-log.md` is the only `.ai/` file touched, and this entry validates against the template with six sections in order, a sequential number, a `Coming From` naming the pre-cycle `HEAD` rather than this commit, prose Outcome and Next Steps with no nested lists, and three Status lines using only allowed values.

#### Next Steps:

SRAM persistence remains the owed item on this port: `gnuboy_load_sram` and `gnuboy_save_sram` exist in the core and are unused, so a Zelda save does not survive `gb stop`, and the card's `saves/` directory is empty and waiting for it. No cartridge other than Link's Awakening DX has been run -- no other Game Boy Color title and no DMG cartridge at all -- so the DMG compatibility path of the `GB_HW_CGB` machine is unproven and is the larger of the two unknowns. The left column reads Pause, Reset and Save on this port and sends nothing, the same debt the NES and Pokemon Mini carry and the same disagreement `edges.h` exists to prevent. The wider backlog is unchanged: Pause, Reset and Save on all three emulators, the BLE controller reconnecting after every reboot, `pm.c` still duplicating the rasteriser `gamepad.c` owns, the NES owing battery-SRAM and its skip-path test, and the NES parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 5 COMMIT Unreleased 2026-11-14T21:23:00-07:00

#### Coming From:

Unreleased 547d232

#### Purpose:

Put Game Boy and Game Boy Color on the watch, because the NES's 256x240 at 1x is 46 percent of the framebuffer and cannot be scaled on this glass, and give the new port sound.

#### Outcome:

It runs at 59.54 fps and the operator's verdict on the sound was "looks and sounds perfect to me." THE PICTURE ARGUMENT WAS SETTLED BY ARITHMETIC BEFORE ANY CODE: 160 divides 320 exactly, so a Game Boy at 2x fills the glass edge to edge at 320x288 -- 69 percent of the framebuffer against the NES's 46 -- with no side margins, where 3x needs 480 columns and does not fit. That is the whole reason to prefer this console here, and it means the margins become the entire controller rather than the leftover strip below a drawn cluster. THE PERFORMANCE ARGUMENT WAS SETTLED BY MEASUREMENT BEFORE THE PORT EXISTED, AND IT OVERTURNED THE PROJECT'S OWN SCOPING DOCUMENT. `docs/gbc-scoping.md` projected 28 to 45 fps and recommended against the attempt; `gbcbench run` on Pokemon Crystal reports `frames=600 run_us=8615 fps=116.1 realtime=1.943`, which is nearly twice real time. The projection was wrong because it normalised a gnuboy figure through the NES interpreter as measured at `-Og`, and the firmware has since gained `-O2`, 120 MHz PSRAM and flash, and the 32 KB instruction cache. The measurement is the reason this port was written rather than argued about, and the scoping document now carries a note saying so. IN THE PORT ITSELF: 59.0 fps, 0.987x real time, emulation 13,768 us and blit 7,887 us. Those two run on separate cores and the guest does not wait for the blit, so throughput is the larger of them rather than their sum, which is why a pair of figures that add to more than a frame still holds full speed. The blit is cheap here in a way the NES's is not because gnuboy's `GB_PIXEL_565_BE` already writes the panel's byte order, so there is no palette lookup and no byte swap where the NES spends 4,355 us a frame turning indices into colours -- and that is against 92,160 pixels where the NES pushes 61,440. TWO BUGS IN THE PORT, BOTH MINE, AND THE SECOND IS THE ONE WORTH REMEMBERING. The first was a pointer overwritten by an index: `xQueueReceive(s_free_q, &s_render_target, 0)` copied a four-byte slot number over a four-byte pointer, so the render target became address 0x1 and the guest wrote its first frame into low memory. That was the hard fault the operator reported, and it printed nothing because this firmware builds with `CONFIG_ESP_CONSOLE_NONE`. THE SECOND FROZE THE PICTURE WITHOUT STOPPING THE GUEST: a defensive `if (slot < 0) return;` bailed out of `gb_video_cb` before the buffer reacquire, and a dry pool produces exactly "no render target," so the callback could never take a buffer back. Emulation then ran with no framebuffer at all, dropped to 8.5 ms a frame, and the glass kept showing the last frame ever pushed -- `frames` advanced 2,671 while `blits` advanced 1. It looked like a working emulator displaying a still picture, which is precisely the trap this project already has a rule about from the other side: DSP-007 says a display change is not verified until someone looks at the glass, and this is the converse, that the glass looking right is not evidence the pipeline is alive. Only the counters distinguished the two. The callback now always reacquires, the guest loop recovers independently, and neither path alone can strand the pool. AUDIO TURNED OUT TO ALREADY BE THERE, AND THREE DOCUMENTS SAID OTHERWISE. This port's header, the handoff notes and the scoping document all stated that `gnuboy`'s APU needed an external sample generator, naming `minigb_apu`, so audio was recorded as future work. It is not true of this fork: `components/gnuboy/sound.c` is a complete four-channel chip -- two squares, the 4-bit wave channel, noise, envelopes, sweep, length counters, NR50 master volume and NR51 panning -- synthesising straight into a host buffer. What was missing was a buffer and a callback, about twenty lines. AND THE SYNTHESIS WAS ALREADY BEING PAID FOR: `gb_sound_emulate()` runs from `gb_hw_vblank()` on every frame no matter what the host does with the output, and the `if (!output_buf) continue;` guard skips only the stores, not the channel arithmetic above them. So enabling audio cost the writes and the DMA copy and nothing else -- measured frame time did not move. THE RATE IS THE ONE REAL TRAP AND IT IS A ROUNDING BUG WITH A SEVEN-MINUTE FUSE. gnuboy does not resample. `gb_sound_reset()` sets `snd.rate = round(2^21 / samplerate)` and emits exactly one sample every `snd.rate` double-speed cycles, so the requested rate only selects the delivered one. Asking for 44,100 lands on 48, which delivers 2^21/48 = 43,690.67 Hz. Opening the sink at 44,100 would then drain 9.3 samples a second faster than the guest fills it; the DMA holds 4,092 frames, so the first click arrives roughly seven minutes in and presents as "the sound goes bad after a while." That is the same shape as the 0.7-sample-per-frame loss `pm.c` documents for the Pokemon Mini. Both the request and the clock now derive from one constant, `GB_AUDIO_HZ` 43,691, which makes the rounded divisor, the delivered rate and the I2S clock agree to 0.008 percent. A frame is 731.5 samples because 35,112 double-speed cycles divide by 48 with a remainder; the core carries that remainder across frames so 731 and 732 alternate and the average is exact, and the buffer only has to exceed the larger. Measured after wiring: `frames` 3,063 to 4,603 over 25.86 s is 1,540 frames, 59.54 fps, 0.9969x real time, `emu=9857us`, `blit=7874us`, zero audio drops in the window. The drop counter reads 4,802 at start and does not move afterwards, which is the ring filling from empty rather than a leak: 4,802 is one 4,092-frame ring plus about one frame, and a steady 62/s loss would have shown roughly 1,240 in the window. The `-Wall -Wextra -Werror` build also exercised an include collision worth recording: the gnuboy component ships its own `sound.h` and puts its directory on the include path, and a quoted include searches the including file's own directory first, so `main/gb.c` resolves to `main/sound.h` -- the amplifier's sink -- rather than the core's APU header. Confirmed from ninja's dependency list rather than assumed, because the two are different things with the same name. Build PASS at 0x2c9580 bytes with 7 percent of the app partition free, sha256 ea4433b490fd4178; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the operator confirmed the picture fills the glass and the sound is correct. The required core-syntax audit was run against this entry before commit and passed after correcting three failures in its first draft: the Coming From field had been written with the fork commit this cycle produced rather than the `547d232` it started from, which is the resulting-hash prohibition; a vendored directory had been listed as one annotated path instead of the files it contains; and the audit result itself was absent, which this sentence supplies. `.ai/core.md` was confirmed unchanged and no other file in `.ai/` was touched.

#### Next Steps:

SRAM persistence is the owed item now that sound is closed: `gnuboy_load_sram` and `gnuboy_save_sram` exist in the core and are unused, so a Zelda save does not survive `gb stop`, and `saves/` on the card is empty and waiting for it. Only Link's Awakening DX has been run -- no other GBC cartridge, and no DMG cartridge at all, so the DMG compatibility path of the `GB_HW_CGB` machine is unproven. The left column reads Pause, Reset and Save and sends nothing, which is the same debt the NES and Pokemon Mini carry and the same disagreement `edges.h` exists to prevent. This cycle did not take the usual reboot and 16 MiB backup, so the last verified image remains `working-2026-11-14-2027.bin`; the firmware here is committed and rebuildable but is not archived. The wider backlog is unchanged: Pause, Reset and Save on all three emulators, the BLE controller reconnecting after every reboot, `pm.c` still duplicating the rasteriser `gamepad.c` owns, the NES owing battery-SRAM and its skip-path test, and the NES parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase.

#### Files Modified:

- ports/twatchultra/components/gnuboy/CMakeLists.txt
- ports/twatchultra/components/gnuboy/COPYING
- ports/twatchultra/components/gnuboy/CREDITS
- ports/twatchultra/components/gnuboy/README.md
- ports/twatchultra/components/gnuboy/cpu.c
- ports/twatchultra/components/gnuboy/cpu.h
- ports/twatchultra/components/gnuboy/gnuboy.c
- ports/twatchultra/components/gnuboy/gnuboy.h
- ports/twatchultra/components/gnuboy/hw.c
- ports/twatchultra/components/gnuboy/hw.h
- ports/twatchultra/components/gnuboy/lcd.c
- ports/twatchultra/components/gnuboy/lcd.h
- ports/twatchultra/components/gnuboy/sound.c
- ports/twatchultra/components/gnuboy/sound.h
- ports/twatchultra/components/gnuboy/tables.h
- ports/twatchultra/main/gb.c
- ports/twatchultra/main/gb.h
- ports/twatchultra/main/gbcbench.c
- ports/twatchultra/main/gbcbench.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 4 COMMIT Unreleased 2026-11-14T20:31:00-07:00

#### Coming From:

Unreleased de50078

#### Purpose:

Make the Pokemon Mini's drawn controls respond to touch, and reorganise the watch's SD card into the layout the TinyTang release cards use.

#### Outcome:

Both landed and the operator accepted them, and the control work cost three rounds because two of the faults were in mapping written from memory rather than read from the table beside it. THE CONTROL WORK USED THE NES METHOD: PM's existing drawn geometry is expressed as a `gamepad_t` -- its nudge-left d-pad position, its `PM_ROT_K` rotation, its own `pad_cx()` and `pad_cy()` rather than re-derived literals -- and `gamepad_hit()` answers from it, so the painter and the hit test read one set of numbers. The touchable area is a DISC OF FOUR QUADRANTS and not the drawn cross, for the reason measured on the NES: the arms paint about 72 percent of the disc that bounds them, so a thumb sliding between two arms falls into a dead wedge. It matters more here, because this d-pad is nudged 20 px left and its disc reaches framebuffer -8, overlapping the left column -- which is why the precedence rule is per point and drawn controls win. THREE BUGS, ALL FOUND ON A TEST CARTRIDGE THAT COUNTS PRESSES, which is what `refurb.min` exists for. FIRST, C WAS MAPPED TO THE WRONG PLACE: it went to the bottom strip's third bar, which is bare glass with no box and no label, while the drawn C in the right column's third slot buzzed and did nothing. The labels in `pm.c` say `PM_RIGHT = { "Hold", "Sound", "C", ... }` and `PM_BOTTOM` is blank in all three; the mapping was written from a recollection of the old layout instead of read from the table beside it. SECOND, AND WORSE, THE MAPPING NEVER CHECKED LIVENESS: `pad_from_frame()` read margin slots without asking `display_margin_slot_live()`, so a slot that is not drawn could still fire. That predicate exists precisely to stop this and the NES path uses it, so the omission was a missing line rather than a missing idea. THIRD, THE DRAWN CONTROLS WERE MISSING FROM THE HAPTIC REPORT: `pm_app_touch()` reported consumption only for margin slots, so a press on the d-pad or a circle moved the pet in silence while C buzzed. The stale comment above that loop made the fault look intentional -- it said nothing hit-tested the framebuffer yet, which was true when written and stopped being true one cycle later. One predicate now answers for both the pad state and the feedback, so a control cannot respond without also being felt. THE CARD WAS REORGANISED INTO THE TINYTANG LAYOUT: media by console with homebrew and diagnostics separated, bios, saves by console, states, shots and diag in their own directories, and the 159 diagnostic text files and 12 synthetic benchmark carts deleted. 205 entries became 31. The Pokemon Mini set was replaced with the named cartridges extracted from the MiSTer card, after verifying that `c01` to `c12` and `pinball.min` were byte-identical duplicates of them, so nothing was lost in the swap; `boot.rom` there was byte-identical to `bios.rom` so no second BIOS was taken. Three zero-byte `smbpart` files from a failed split test and an empty `TinyDesk` directory were found and removed. THE FIRMWARE HALF WAS NOT OPTIONAL, because every ROM path was a compile-time constant. `sdpaths.c` adds `sd_media()`, which tries the organised path and falls back to the flat one so an older card still boots, and `sd_out()`, which routes diagnostics to `/sd/diag` and screenshots to `/sd/shots` and creates the directory on demand -- the absence of which is how the card's root became 205 entries. While wiring it, `snprintf(path, 32, sd_out(...), n)` was found to be using a PATH as a printf FORMAT, working only because the `%lu` happened to ride along inside the filename; any card path containing a percent would have broken it, and it is now two explicit calls. ONE THING IS LEFT OWED AND VISIBLE: PM's left column reads Exit, Save and Load, and nothing handles them -- `pm_app_touch()` only reports haptics there. Those labels describe duties the buttons do not perform, which is the exact disagreement `edges.h` was built to prevent, and it is the same shape as the Tamagotchi's Exit. Build PASS at 0x2bf370 bytes with 8 percent of the app partition free; deployment PASS by app-only flash to 0x10000 with `Hash of data verified` and the image read back off the chip and compared byte-identical (sha256 cfd1bdaa10c9395d); the card was restructured on the development machine and mounted by the watch at `/sd` with the new tree listing correctly, and `/root/Apps/pm.py` in flash was rewritten because it still named `/sd/pinball.min`. The board was rebooted and a full 16 MiB backup taken with it held in the bootloader across read and verify: `working-2026-11-14-2027.bin`, 16,777,216 bytes, sha256 `99328d8ea91d6c733d29daa4966d870fc784249d202f45d76ab3ee44175a328e`, `verify OK (digest matched)`, `ota_0` inside it compared byte-identical to the build. The image archive holds the firmware but NOT the card: the card is device state and its layout is described in the README written at its root.

#### Next Steps:

PM's left column needs its three duties implemented, and Exit is the one that matters: Save and Load need a state format the core does not have, where Exit is a few lines and matches what the Tamagotchi already does. That should be its own cycle rather than folded into control work. The card layout should be captured by a script under `scripts/` that rebuilds it, since it is currently described only by a README on the card and by this entry -- and the same script should write the `/root/Apps` launchers for the Pokemon Mini and the Tamagotchi, which live in flash and survive an app flash but not a full erase. The d-pad's C is at its drawn size; nothing has been tuned by thumb, and `pad_hit_d` is one field if the circles want the NES's 1.5x. The wider backlog is unchanged: Pause, Reset and Save send nothing while an app owns the screen on the NES, the BLE controller needs reconnecting after every reboot, `pm.c` still duplicates the rasteriser `gamepad.c` owns, and the NES owes battery-SRAM persistence, the skip-path test and its page 15 crash.

#### Files Modified:

- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/nes.c
- ports/twatchultra/main/nes.h
- ports/twatchultra/main/pm.c
- ports/twatchultra/main/pm.h
- ports/twatchultra/main/sdpaths.c
- ports/twatchultra/main/sdpaths.h
- ports/twatchultra/main/tama.c
- ports/twatchultra/main/tama.h
- ports/twatchultra/main/touch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 3 COMMIT Unreleased 2026-11-14T18:49:00-07:00

#### Coming From:

Unreleased b22afc2

#### Purpose:

Finish the Tamagotchi V3 emulator's interface: give it the guest's own sampled sound with a volume control, make the margin buttons speak with haptics, widen the bottom strip's targets, and add an Exit button and a Start menu entry so the emulator can be left and returned to without a shell.

#### Outcome:

Five changes in one stretch, all on the glass and all accepted by the operator, and two of them cost a wrong diagnosis that is recorded because the reasoning is the useful part. THE SOUND WAS A BELL, NOT THE PET. The port called `sound_beep()`, which is the terminal's BEL -- a fixed 1 kHz square for 200 ms, fired on the rising edge of "the guest is making a noise". The core had been modelling the real thing all along: `splb32_sound` holds a PWM channel and a tone channel whose levels flip on driver timer overflows, and `tmc_audio_sample()` now reads that mix into the same `sound_stream_*` sink the NES and Pokemon Mini use. THE FIRST ATTEMPT RAN THE GUEST 41 PERCENT FAST and the arithmetic explains it exactly. Sampling a waveform that toggles inside `tmc_run()` needs about one sample per period, so the first version called `tmc_run()` a thousand times a frame -- and every call overshoots by up to one instruction, because `splb32_run_cycles()` cannot stop mid-instruction. One call per frame overshoots once and nobody notices; a thousand overshoot a thousand instructions. Measured at 352,700 cycles per frame against 250,000. The fix moved the loop into the core as `tmc_run_audio()`, one stepping loop against one target that emits samples as it crosses their boundaries, and the rate came back to 250,009 -- a ratio of 1.000. The lesson is worth more than the fix: a change that multiplies a call site by a thousand multiplies whatever that call gets wrong by a thousand too. VOLUME CYCLES 100, 75, 50, 25 AND MUTE, and the first version sounded wrong in a way that is arithmetic rather than taste. Scaling the samples linearly gave steps of 2.50, 3.52 and 6.02 dB, so full and three-quarters were barely distinguishable while the drop to a quarter was obvious -- reported on the glass precisely so. The ear is logarithmic, so each step is now a fixed 4 dB and the amplitude follows from it: gains of 1000, 631, 398 and 251 thousandths, with adjacent ratios all 0.631. The label shows the current level rather than the next one, deliberately unlike Speed: a rate is momentary and the button's only meaning is "change it", where a volume is a standing property of what you are hearing. HAPTICS FOLLOW WHAT WOULD ACTUALLY FIRE, and this took two attempts in opposite directions. The driver buzzes when a consumer reports it consumed the touch, and the app reported consumption only when an action was emitted -- so the arm, which is a modifier with no action by design, was silent, which is the one press that most needs confirming. The first fix made every mapped slot buzz regardless, on the reasoning that a physical button clicks whether or not the machine acts on it. That is true of a real button and false here: a gated slot unarmed is not a button that fails, it is a button that is not armed, and the interlock exists so that an unarmed press is not a press. The operator heard Save vibrate with nothing held and rejected it. The rule is now three cases -- the arm always, ungated actions always, gated actions only while armed -- and it is a predicate in the UI layer beside the arm's own definition rather than a condition in the driver. A SECOND FINGER IS A SECOND CLICK, which the chord case needed: the driver buzzed only on the report where the glass went from untouched to touched, so holding the arm with one thumb and pressing Save with another never buzzed for the second. It now also fires when the frame carries more points than the previous one. THE BOTTOM STRIP'S TARGETS ARE TWICE ITS DRAWN HEIGHT, at the operator's request, which makes each about square -- 82 tall against 107 wide instead of 41 against 107. It can only grow upward, because the strip's last row is the last row of the glass, and that put the new area at framebuffer rows 375 to 415. THE FIRST VERSION GATED THAT ON SCREEN OWNERSHIP AND WOULD HAVE BEEN WRONG: the Pokemon Mini's d-pad occupies framebuffer rows 267 to 429, right inside the new area, and that emulator does not hit-test its drawn controls at all, so a finger on its d-pad would have fired Shake, A and B. So it is opt-in per app -- `display_margin_strip_tall()`, called by the Tamagotchi alone, cleared automatically when the desktop takes the glass back. RESET MOVED UP ONE SLOT to fill the gap Sound left, so the right column is Hold, Shot, Sound, Reset, Exit with no hole; the cost is that Reset is now two slots from the arm rather than four, which weakens the original "furthest from a sliding thumb" argument and is a deliberate trade for a tidier column. EXIT IS GATED LIKE RESET and ends the session without saving, because the checkpoint on the card is what Save last put there and an exit that silently saved would make the Save button meaningless. Making it work required splitting `tama_stop()` into a request half and an idempotent `tama_teardown()`, because the guest task now ends runs itself and was previously deleting itself without freeing the machine or returning the screen. Teardown is claimed under a critical section rather than flag-tested, since both paths can race and freeing the machine underneath the other is worse than leaking. The Start menu entry is `Tamagotchi V2-3.tdlink` pointing at `/root/Apps/tama.py`, which is the same MicroPython shim the Pokemon Mini uses because `tdsh run` cannot allocate the 32 KiB contiguous stack an app script needs; the requested name was "Tamagotchi V2/3" and a slash cannot appear in a filename, so it is written V2-3. Build PASS at 0x2bf070 bytes with 8 percent of the app partition free; deployment PASS by app-only flash to 0x10000 with `Hash of data verified` and the image read back off the chip and compared byte-identical (sha256 0b531f0cc9743ef3); the host suites both pass, `test_tama_ui` at 236 checks and `test_twatch_edges` at 1591, the latter now sweeping the glass in BOTH strip states and asserting the exact point count for each region in each. The board was rebooted and a full 16 MiB backup taken with it held in the bootloader across read and verify: `working-2026-11-14-1845.bin`, 16,777,216 bytes, sha256 `7f541e0df481eb81bffd37ce1fc4eb67262a874622740cd8ce0e858a01722e9f`, `verify OK (digest matched)`, `ota_0` inside it compared byte-identical to the build. The watch enumerated on ttyACM0 with a second ESP32 board on ttyACM1; `/dev/serial/by-id` names the JTAG port by MAC, so the two are distinguishable and were checked rather than assumed.

#### Next Steps:

Two device-side files are still outside version control and should be captured by a script under `scripts/` if they are to survive a full erase: `/root/Apps/tama.py` and `/root/Start menu/Tamagotchi V2-3.tdlink`. The Pokemon Mini's equivalents were created the same way and have the same gap, so one script covering both would close it. The emulator now has sampled audio where the Pokemon Mini slices its frame sixteen ways and the NES synthesises once per frame -- three emulators, three approaches, and the Tamagotchi's is the only sample-accurate one, which is worth remembering when the audio dropout parked at entry 88 is reopened: `tmc_run_audio()` is a working example of stepping a guest in slices sized to the sample period. Its limit is that `splb32` toggles sound levels rather than producing samples, so the waveform is a square built from those toggles and not a filtered analogue output. What remains owed on the Tamagotchi is small: it still has no drawn controls on the framebuffer, relying on the margins, where the NES now uses `gamepad.c`. The wider backlog is unchanged -- Pause, Reset and Save send nothing while an app owns the screen on the NES, the BLE controller needs reconnecting after every reboot, `pm.c` still duplicates the rasteriser `gamepad.c` owns, and the NES owes battery-SRAM persistence, the skip-path test and its page 15 crash.

#### Files Modified:

- ports/twatchultra/components/tamacore/include/tmc_core.h
- ports/twatchultra/components/tamacore/tmc_core.c
- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/edges.h
- ports/twatchultra/main/tama.c
- ports/twatchultra/main/tama_ui.c
- ports/twatchultra/main/tama_ui.h
- ports/twatchultra/main/touch.c
- tests/test_tama_ui.c
- tests/test_twatch_edges.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 2 COMMIT Unreleased 2026-11-14T16:19:00-07:00

#### Coming From:

Unreleased b33ea1f

#### Purpose:

Fix the leftover pixels the Tamagotchi V3 emulator left behind a moving pet, which the operator reported as thin horizontal trails across the glass.

#### Outcome:

The fix is eight rows of padding on each side of every dirty band, and it was found by measurement after five instruments, two of which were built wrong. THE FRAME BUFFER WAS NEVER THE PROBLEM, and establishing that took a tool that could see the difference: `draw_rows()` regenerates the whole band it is given, from the guest's DPRAM, immediately before each push, so what reaches the panel always agrees with a fresh render and the frame buffer can never be blamed from outside. The first marker drawn for this purpose proved the point by being invisible -- four 1 px red rules, which this project's own LCD-002 records as a degenerate window the CO5300 does not honour and reports success for anyway. At 3 px the boxes appeared, and the operator could then see that the leftovers sit just outside the pushed bands, at the pet's bottom and at the inner edges of both icon rows. A band log confirmed only rows 196 to 243 were ever pushed while a pet animated, and the dirty-row tables were checked against the real `segmap.c` data and found complete, with no byte collisions between the 960 matrix cells and the ten icon bits. A full repaint cleared everything and the trails reappeared as the pet moved, which isolated the fault to the incremental path rather than to the guest's state or the renderer. THE FIX WORKS BY REDUNDANCY, NOT BY CORRECTING AN ADDRESS. Band heights are 35, 41 and 47, and `flush_rect()` caps a transaction at 17 rows across this width, so a 47-row band is three separate transfers; a row lost at one of those boundaries is never rewritten by the band it belonged to. Widening the band means the next band that touches the area rewrites it. 8 is a dot row plus its gap, one full cell of slack each side, which is the smallest value with a physical meaning here rather than a number tuned until the symptom stopped. The operator confirmed 0 reproduces and 8 is clean, and 8 is now `TAMA_PAD_DEFAULT` rather than a runtime setting, because a fix that only applies when someone types `tama pad 8` is not shipped. The cost is transfer time only and it is small: a 47-row band becomes 63 of a 308-row frame, pushed at the guest's own 16 Hz rather than per frame. TWO INSTRUMENTS WERE BUILT THAT COULD NOT FAIL, AND BOTH ARE RECORDED BECAUSE THAT IS THE CYCLE'S REAL COST. The first drew its red boxes as 1 px rules, so every call returned success and nothing appeared -- LCD-002 already said so. The second, `tmc_stale()`, rendered the whole frame from DPRAM into a scratch buffer and compared it against `s_frame`, which sounds exactly like a stale-pixel detector and is not: because `draw_rows()` rewrites the entire pushed band from the same truth, `s_frame == truth` is an invariant, so it reported zero over 45 seconds of a visibly broken screen. It has been removed rather than left in the tree looking useful. The general lesson is the one this stretch keeps re-teaching: an instrument has to be able to disagree with you, and checking that it can is cheaper than the round trip that discovers it cannot. WHAT IS KEPT is what earned its place: `tama pad N` to A/B the fix on the glass without a flash, `tama redraw` to force a full regeneration and push, `tama mark on|off` for the 3 px band boxes, and `tama bands on|off` which logs every pushed band's row range to `/sd/tamaband.txt`. Build PASS at 0x2bea40 bytes with 9 percent of the app partition free; deployment PASS by app-only flash to 0x10000 with `Hash of data verified` and the image read back off the chip and compared byte-identical (sha256 9ecf6d80c0088876); the operator confirmed no leftover pixels behind a moving pet. The board was then rebooted to a clean desktop and a full 16 MiB backup taken with it held in the bootloader across read and verify: `working-2026-11-14-1614.bin`, 16,777,216 bytes, sha256 `b8eadc87813b9f50fc3800c28433682d778c624089d363cc9b0e33cc0eed87b0`, `verify OK (digest matched)`, `ota_0` inside it compared byte-identical to the build.

#### Next Steps:

The padding is a redundancy fix rather than a diagnosis and that distinction is worth carrying: nothing here establishes WHY a row is lost at a transfer boundary, only that repeating the rows recovers it. If the loss ever becomes visible elsewhere -- in a full-screen app blit, or in the NES path which pushes differently -- the boundary behaviour of `flush_rect()` and the 17-row transaction cap are what to examine, and `tama pad 0` is the reproduction. The Tamagotchi has no sample-based audio, using the older `sound_beep()` event path where the NES and Pokemon Mini use the shared `sound_stream_*` sink, so unifying it is available work. The emulator also has no drawn controls on the framebuffer, relying on the margin buttons and their hold-to-arm interlock, where the NES now draws its own d-pad and circles -- `gamepad.c` exists and could serve it. Three items carried from the rollover handoff remain unchanged: Pause, Reset and Save send nothing while an app owns the screen, the BLE controller needs reconnecting after every reboot, and `pm.c` still duplicates the rasteriser `gamepad.c` owns. The NES is parked with `docs/nes-accuracy.md` as its baseline, and its page 15 crash is the first thing to chase whenever that thread resumes.

#### Files Modified:

- ports/twatchultra/main/tama.c
- ports/twatchultra/components/tamacore/tmc_core.c
- ports/twatchultra/components/tamacore/include/tmc_core.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 1 COMMIT Unreleased 2026-11-14T13:40:00-07:00

#### Coming From:

Unreleased 3a447b9

#### Purpose:

Open a new active log after the ninety-sixth entry, recording the archive that holds the closed one and the state of the project at the handoff, so the next session starts from a verified position rather than a recollection.

#### Outcome:

The closed log was archived to `archived_logs/core-log-2026-11-14-1340.tar.gz`, 160,994 bytes, holding all ninety-six entries of entries 1 to 96. It was checked by extracting it again and hashing the result: `10b437956d22d4deeee4ad1d0742fbfc8ff5a080149e6fb608c9abd2a9088476`, which is the hash of the log as it stood before archiving, at 520,645 bytes, and the extracted file still counts ninety-six top-level entries. THE PREVIOUS ROLLOVER'S ARCHIVE IS MISSING and that is recorded here rather than quietly fixed: the log just closed states in its own entry 1 that the preceding hundred entries were archived to `archived_logs/core-log-2026-11-11-2223.tar.gz`, and `archived_logs` was empty when this rollover began, with no tracking history for it in git at all. The likely cause is that the archive was never committed and was lost with a working copy, which is a directory `.gitignore` does not cover. This rollover commits its own archive for that reason, and a future session that wants the 2026-11-11 archive should look to the user rather than assume it is recoverable here. THE PROJECT STATE, and every claim below should be verified against the working tree, because the files are the fact and a handoff is a claim. Escapement, the Linux project, is parked: its last code commit is `8645a82`, "core0: record the full-paint switch, and close the Linux line of work", and everything after it in the closed log is project metadata only. The T-Watch Ultra work lives in a separate fork at `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, 132 commits above tag `v0.4.0`, HEAD `ae231d7`; the remote to push to is `fork`, which is `aquasock/tinydesk`, and `origin` is `tinydesk-project/tinydesk`, the upstream, which must never be pushed to. The fork was clean at handoff and this repository's last commit before this entry was `3a447b9`. The firmware on the watch is the build at sha256 `2e71a9fdd86878d3`, archived as `~/twatch-backup/working-2026-11-14-1331.bin`, 16,777,216 bytes, sha256 `2645ce4a0aa0a34b9ec430e40490ec5af2fd7c354e1866be5e965ed5b32d7d31`, verified against the chip with the board held in the bootloader and its `ota_0` compared byte-identical to the build. THE FOUR SHIPPED SETTINGS ARE THE FIRST THING TO KNOW because none of them is obvious and each was measured: `CONFIG_COMPILER_OPTIMIZATION_PERF` in place of the `-Og` default, which took Pokemon Mini from 81.6 to 117.9 fps unpaced; the 32 KB instruction cache because the 6502 interpreter is 21,175 bytes and cannot fit a 16 KB cache; PSRAM and flash both at 120 MHz, which the SoC asserts must agree; and WiFi buffers at 4 plus 4 with the cache pool at 16, which is the floor that lets the SSH server start. All four are in `sdkconfig.defaults` and all four were verified by deleting `sdkconfig` and rebuilding from defaults alone. WHAT WORKS ON THE GLASS: a 40 by 26 desktop at 320x416 on a 410x502 glass, every app reflowed to that grid; three painted margins that are live touch targets with all their geometry in `ports/twatchultra/main/edges.h`, shared by the painter and the hit test; the RTC seeding the clock at boot; the PMU reporting battery and USB; the Tamagotchi emulator; and two console emulators, the NES and the Pokemon Mini, both with sound through the same non-blocking `sound_stream_*` sink. THE NES IS THE MOST DEVELOPED AND WAS PARKED AT THE OPERATOR'S DIRECTION IMMEDIATELY BEFORE THIS HANDOFF. It runs Zelda at 61 fps with zero audio drops, draws its own d-pad and A and B circles below a top-aligned picture, answers touch through a disc-shaped four-quadrant hit test with 1.5x circle targets, and takes a BLE Xbox controller including the four diagonals. Entry 94's `ppu.vaddr_inc` fix was the last real bug: the field was never initialised, so any cartridge that filled its nametable before writing `$2000` wrote every byte to one address, which is why AccuracyCoin rendered as a field of identical rings. AccuracyCoin now boots and the baseline is captured in `docs/nes-accuracy.md`: 14 of 22 pages, 68 pass, 35 fail, then a crash on page 15. The failure profile is the useful part -- every passing group is an opcode implementation and every failing group is timing or bus accuracy, with the undocumented instruction set at 52 of 52. THREE THINGS ABOUT THIS PROJECT'S METHOD ARE WORTH CARRYING, because they were learned expensively and the closed log records each several times. A display or input change is not verified until someone looks at the glass or touches it: every counter can read healthy over a black screen, and the operator's eyes found faults that no instrument did. An absence in one instrument is not evidence about a mechanism: this stretch produced three separate wrong diagnoses from a counter that was measuring the instrument rather than the board, and the fix each time was to build an instrument that does not perturb what it measures. And a claim that a value was never initialised, or that a hardware behaviour cannot occur, is worth checking against the source rather than against intuition -- the `vaddr_inc` bug and the diagonal-hat bug were both a comment in the code asserting something false.

#### Next Steps:

Work has moved off the NES and the backlog is what this handoff hands over, in the order the closed log ranked it. Pause, Reset and Save are drawn and labelled on the emulator's left column and send nothing, because the driver's owned-app branch skips `edge_press()` entirely; they are the app's own controls, so they belong in `nes_app_touch()` rather than in a key sent to a hidden shell, and Pause and Reset are each a few lines while Save needs the SRAM persistence that is still owed. The BLE controller must be reconnected after every reboot because a saved bond does not come back at boot, which costs a manual step on every reset. The Pokemon Mini draws a d-pad and two circles but nothing hit-tests them, and `gamepad_hit()` is ready for it with its layout already expressed as a `gamepad_t`. `pm.c` still duplicates the rasteriser that `gamepad.c` owns, which is a consolidation owed rather than a fault. The NES owes battery-SRAM persistence, so a Zelda save survives a reset, and the skip-path test, since `skipped=0` proves the buffer pool never ran dry and not that the skip logic works. The NES accuracy work splits into three groups that should not be attempted together: modelling DMA and dummy cycles is the largest and has a second reason to exist, because it is the most plausible link to the audio dropout parked at entry 88 of the closed log; indirect addressing wraparound is small and self-contained; and sub-instruction interrupt latency is the hardest and least valuable. The page 15 crash is a separate fault from any accuracy result and costs 8 unmeasured pages. Headphones are closed on this hardware on three measured grounds and should not be reopened without a different board. The radio is off unless a cycle turned it on and left it, so check `network autowifi` before assuming SSH is reachable, and note that the touch controller's interrupt has never been confirmed to fire on a press on this stack. CAPACITY: this log holds 100 entries and the next rollover falls due after entry 100. Archive it under `archived_logs` with a timestamped name, verify by extracting and hashing, clear the log, and add the handoff as entry 1 -- and commit the archive, since the previous one was lost precisely because it was not.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
