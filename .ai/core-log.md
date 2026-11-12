## 34 COMMIT Unreleased 2026-11-12T11:57:24-07:00

#### Coming From:

Unreleased 78d4546

#### Purpose:

Capture a full 16 MiB backup of the build that plays WAV files, and verify the streaming player survived a whole track.

#### Outcome:

`~/twatch-backup/working-2026-11-12-1152.bin` is the current recoverable state, 16 MiB read at 577.4 kbit/s in 232.4 seconds, sha256 `b90249b86d96d7efd43c2c865ebaf445fb12c4de22d0178199ec520f281eba16`, verify OK against the chip with the board held in the stub across read and compare, and the stored `.sha256` re-checked afterwards. The 2,570,352-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `5c81df44`, and the strings that identify this build are present -- `play <file.wav> | play stop`, `play %luHz`, `open: %s` and `truncated header` among them -- so the image is the one with the player rather than an earlier one. The partition table parses as nvs, phy_init, ota_0, ota_1, otadata and storage at the offsets `partitions.csv` declares, both image magics are 0xe9, and the littlefs partition holds 2,004 written blocks of 2,528 against the previous backup's 1,999, which is the five blocks of session writes this run produced. THE PLAYER WAS VERIFIED END TO END BEFORE THE BACKUP, and the numbers are the interesting part: the log reads `play 44100Hz` at 135110 ms and `stop` at 424264 ms, which is 289.15 seconds against a file of 289.133 seconds. That is the whole track, streamed, stopping itself at the end rather than being cut short -- 48 MB read from an SD card on a 10 MHz bus shared with the LoRa radio and the NFC reader, and the operator reported it sounded perfect with no drops. The resource state afterwards is consistent with a long read that did its work properly: 65 KB of internal RAM free, and the cell at 34.4 C against a die at 45.8 C. That last pair is worth recording separately, because it is the thermistor conversion being corroborated by unexpected evidence -- five minutes of continuous decode and SPI traffic warmed the board, and the cell and the SoC both rose while staying in the order PMU-006 established, which neither would have done if the conversion were reading the SoC or the wrong current. THE SCRIPT'S RESET WORKED ON THE FIRST ATTEMPT AGAIN, which is now the second consecutive backup to do so, against three in a row that had needed a fallback before entry 28 found the cause. The board was then confirmed running independently of the script's own report, by reading its desktop over USB at 11:57. Nothing was built or flashed this cycle. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 34, Files Modified reads `None.` because the cycle generated a recovery artifact outside the repository and changed no tracked file, and Status uses `N/A` for build and deployment because nothing was compiled or written to the chip.

#### Next Steps:

Audio is finished as far as the port is concerned: a file streams from the card for its full length without a drop, and the three formats a normal converter produces all play. What it does not have is a way in from the desktop, since `play` is a Terminal command -- a Start-menu extra for a fixed file, or a small app that lists the card and plays a selection, are the two shapes, and neither is needed to call the work done. Two limits are worth carrying so they are not mistaken for faults later: there is no pause, seek or volume, and the sampling formats are 16-bit PCM, one or two channels, 8 to 96 kHz -- 24-bit, float and multichannel are refused with their numbers logged rather than played wrong. Beyond audio, the queue is unchanged and nothing is blocked: the taskbar edge is settable and left defaulting to bottom at the operator's direction; the thermistor reads in degrees with a host-tested conversion; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; the display's tearing-effect line is configured and unread; the screen reader lives in the fork under `tools/`; and the unbuilt outboard parts are as they were -- the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card and the image cache, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 33 COMMIT Unreleased 2026-11-12T11:49:19-07:00

#### Coming From:

Unreleased ad05986

#### Purpose:

Play a WAV file through the watch's speaker, which the port had never done despite having working I2S output.

#### Outcome:

At `c881b3c` on branch `t-watch-ultra`, `play /sd/full.wav` streams a 4 minute 49 second, 48 MB, 44.1 kHz stereo track off the SD card while the desktop keeps running, and it was confirmed by ear in three steps: a 3-second mono 16 kHz clip, a 5-second stereo 44.1 kHz clip, and then the whole track. THE PREMISE THIS STARTED FROM WAS WRONG AND THE CORRECTION IS WORTH RECORDING. The request assumed the port already played a WAV file for its tone; it did not. `sound.c` generated a 1 kHz square wave in `beep_samples()` and there was no RIFF parser anywhere in the tree, so this was a new feature rather than a reconfiguration of existing playback. What did already work was the I2S output itself, which is the easy half and is why the feature came together quickly once the parser existed. TWO NEW FILES, BOTH SPLIT OUT FOR TESTABILITY. `wav.c` walks the RIFF chunk list rather than assuming a canonical 44-byte header, and that decided the outcome on the very first real file: ffmpeg's output reports `data_offset` 192 because it writes a LIST metadata chunk, so a fixed-offset reader would have played 148 bytes of metadata as audio and sounded merely wrong rather than failing. The test states the same chunk layouts independently -- a LIST between fmt and data, an odd-length chunk with its pad byte, a `WAVE_FORMAT_EXTENSIBLE` fmt longer than 16 bytes whose real format lives in the sub-format GUID, and a data length exceeding the file -- and TWO NEGATIVE CONTROLS CONFIRM IT CAN FAIL: dropping the odd-chunk pad byte fails one assertion, and a fixed-offset reader fails six. PLAYBACK IS STREAMED AND NEVER LOADED, which is forced rather than stylistic: the track is six times the whole littlefs partition and PSRAM is shared with the desktop. 4 KiB reads against four 1024-frame DMA buffers is 93 ms of queued audio against under 25 ms of file I/O, roughly a 4x margin, and no stutter was audible across a five-minute read from an SD card on a 10 MHz bus shared with the LoRa radio and the NFC reader. It handles mono and stereo and reconfigures the I2S clock from the file's own header, restoring the beep's 16 kHz afterwards; mono is duplicated into both slots because the MAX98357A averages (L+R)/2 in its default SD mode and a mono-left stream would play at half volume, the same trap `sound_init()` already records for the beep. The rate change is audible rather than merely correct in a register: the log shows `play 16000Hz` then `play 44100Hz`, and the operator reported the second `sounded better`. THREE BUGS WERE CAUGHT BEFORE THIS CODE RAN AND ONE AFTER. Before: the path was handed to the task as a pointer to the caller's stack, so a command's argv would be freed before the task read it; a partial I2S reconfiguration was not undone on failure; and `wav.c` was missing from the build list, which the linker caught rather than review. After, and the one worth carrying: the first attempts logged only `no file`, which made a wrong path, an unreadable file and an unmounted filesystem indistinguishable and cost several rounds of guessing. `strerror(errno)` turned that into `open: No such file or directory` immediately, which is how the real path was found -- the shell's `~` is `/fs/root` in VFS terms, not `/home/root` and not `/`. A diagnostic that says what failed without saying why is a diagnostic that invites guessing, and this cycle spent several rounds proving it. A FLAW IN THE TEST SETUP IS RECORDED TOO: `test_wav.c` `#include`s `wav.c`, so CMake had no dependency on it, ninja reported `no work to do`, and a negative control silently tested a stale build. Fixed with `OBJECT_DEPENDS` on both include-style tests and verified by touching the source and watching the object rebuild; without that, a control can appear to fail for the right reason while checking nothing. The SD card route is worth recording as method: SFTP on this watch manages about 3 KiB/s, which is four hours for this track and never completed even a 96 KiB clip in the time allowed, so the card was written from a host card reader and every file verified by md5 against its source before the watch saw it. `api/tinydesk.api` is UNCHANGED at 701 declarations, because the player is port-internal -- unlike the taskbar edge and the thermistor temperature, this needs no author conversation. The host suite is 40 of 40, the firmware builds clean under `-Werror`, and only the app region at 0x10000 was written with the hash verified. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 33, and Status uses `PASS` for the user test because the operator confirmed the audio on the glass.

#### Next Steps:

What is left on audio is a decision rather than a repair: the player is a shell command, so it belongs to the Terminal and cannot be reached from the desktop, and the two shapes worth considering are a Start-menu extra that plays a fixed file and a small app that lists what is on the card and plays a selection. Neither is needed to call this done. Two limitations are worth stating plainly rather than letting them look like bugs later. There is no pause, no seek and no volume control: `play` starts a file, `play stop` ends it, and the only way to change the track is to stop and start again. And the sampling formats are deliberately narrow -- 16-bit PCM only, one or two channels, 8 to 96 kHz -- which covers everything a normal converter produces and silently refuses 24-bit, float and multichannel rather than playing them wrong; the log says `fmt%d %db %dch %lu` when it refuses, so the numbers are there to be read. A fresh backup is due, since `working-2026-11-12-1045.bin` predates the player entirely. Everything else is unchanged and unblocked: the thermistor reads in degrees and its conversion has three negative controls; the taskbar edge is settable and left defaulting to bottom at the operator's direction; both pushers work and both send Enter; the display's tearing-effect line is configured and unread; the screen reader lives in the fork under `tools/`; and the unbuilt outboard parts are as they were -- the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card and the image cache, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

- ports/twatchultra/main/wav.c
- ports/twatchultra/main/wav.h
- ports/twatchultra/main/sound.c
- ports/twatchultra/main/sound.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- tests/test_wav.c
- CMakeLists.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 32 COMMIT Unreleased 2026-11-12T10:50:49-07:00

#### Coming From:

Unreleased a06a444

#### Purpose:

Capture a full 16 MiB backup of the build in which the battery thermistor reads in degrees.

#### Outcome:

`~/twatch-backup/working-2026-11-12-1045.bin` is the current recoverable state, 16 MiB read at 578.8 kbit/s in 231.9 seconds, sha256 `efeb1ef224b696ab6daae270075e818a42c7b29f72ea003db93e6881079bd758`, verify OK against the chip with the board held in the stub across read and compare, and the stored `.sha256` re-checked afterwards. The 2,565,696-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `118a305c`, so the image is the firmware that was running. The strings that identify this build are all present -- `ntc%s%d.%d`, `ts %dmV`, `ts ctl%02x`, `cv%d ic%d`, `chg go`, `vbus in`, `i%lu r%lu`, `irq %d up`, `p%d enter` and `Taskbar at the top` -- which matters more than usual here because one of them, `ntc%s%d.%d`, exists only in the build that converts the thermistor to degrees; a backup taken one cycle earlier would have carried the millivolt format instead. Two candidate strings were absent on first inspection and neither was a fault: `ts_tenths_c` is a function name rather than a string literal and does not survive into the binary that way, and `ts 494mV` embeds a value that only exists at run time. Checking the format strings instead of the rendered text is the correct instrument, and the first attempt was the wrong one. The partition table parses as nvs at 0x9000, phy_init at 0xf000, ota_0 at 0x10000, ota_1 at 0x310000, otadata at 0x610000 and storage at 0x620000, matching `partitions.csv`; both image magics are 0xe9; and the littlefs partition holds 1,999 written blocks of 2,528 against the previous backup's 1,995, which is the four blocks of shell history and settings writes this session produced. THE SCRIPT'S RESET WORKED ON THE FIRST ATTEMPT, which is the first time that can be said: it printed `resetting the board`, `waiting for the board to come back`, `board is up`, with no fallback used, against three consecutive earlier backups that had all needed a second attempt before entry 28 found the cause. The board was then confirmed running independently of the script's own report, by reading its desktop over USB at 10:50, and the thermistor conversion was confirmed live on that fresh boot rather than only on the build's first run: `Battery:100%  4.17 V full ntc29.4` with the die at 35.8 C, which is the same ordering and the same range as entry 31's 30.0 C against 36.8 C. Nothing was built or flashed this cycle. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 32, Files Modified reads `None.` because the cycle generated a recovery artifact outside the repository and changed no tracked file, and Status uses `N/A` for build and deployment because nothing was compiled or written to the chip.

#### Next Steps:

The backup now matches what ships, so the next risky change has a fallback: `working-2026-11-12-1045.bin` carries the thermistor conversion, the taskbar option, both pushers, the charge handling and the touch interrupt. The cheapest useful work left on the thermistor is logging its temperature alongside each `chg go` and `chg done`, which needs no new part data and gives a temperature history from events the driver already handles -- and it would place a reading at the moment charging starts, which is when the charger's thermal window between roughly 0 and 55 C actually matters. One observation is still owed on the current source: `reg50H` reads 60 uA where the datasheet's default is 50, and a battery disconnect would show whether that is an eFuse value or something a previous firmware wrote. The conversion follows either, because it reads the register rather than assuming, but nobody has watched it change. Everything else is unchanged and unblocked: the taskbar-at-top option is verified and left defaulting to bottom at the operator's direction, both pushers work and both send Enter, audio is a beep with no playback path, the display's tearing-effect line is configured and unread, the screen reader lives in the fork under `tools/`, and the unbuilt outboard parts are as they were -- the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card and the image cache, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 31 COMMIT Unreleased 2026-11-12T10:42:47-07:00

#### Coming From:

Unreleased af3d266

#### Purpose:

Convert the battery thermistor's voltage into a real temperature, using the part data found in the previous cycle.

#### Outcome:

At `3bff944` on branch `t-watch-ultra`, System Monitor's battery line reads `Battery:100%  4.18 V full ntc30.0` where it read `ntc494`, and the watch confirms it live: the cell at 30.0 C with the die at 36.8 C, which is both the ordering `PMU-006` established and the value `PMU-007`'s table predicts for that voltage. THE ARITHMETIC IS IN ITS OWN FILE, `ports/twatchultra/main/ts_convert.c`, WITH NO ESP-IDF IN IT, and that is the whole reason the change is shaped this way: the conversion is the one part of the thermistor feature that can be wrong in a plausible way, and leaving it beside the I2C calls would have put it beyond the reach of any test. It is now compiled and swept on a host like any other pure function. The table is reproduced from Table 6-3 of the AXP2101 datasheet rather than curve-fitted, because the Steinhart-Hart coefficients published in public drivers disagree with the vendor's own numbers by about 2.6 C; and the current source is READ from reg50H[1:0] rather than assumed, because this board reports 60 uA where the table assumes 50. The reading is scaled to the table's 50 uA before interpolating, which is exact rather than approximate -- the thermistor's resistance does not depend on the current and the voltage across a resistor is proportional to it -- and skipping that step makes the battery look 4.6 C colder, which is precisely what the test's negative control measures. A TEST WAS WRITTEN THAT COULD ACTUALLY FAIL, and three negative controls each trip a different assertion: dropping the current scaling fails the 60/40/20 uA round-trips; mistyping one table row in the source from 348 to 358 fails at 357 against 350, which is the exact fault the test exists for because a 0.7 C error of that kind reads as plausible in every other way; and moving the absent-reading guard after the mode guard reports a missing reading as NOT_TEMP instead of NONE. The test also restates the datasheet's resistance and ADC columns independently of the implementation, so a transcription slip is caught against a second opinion rather than against a copy of itself, and it holds the board's own recorded readings from `PMU-006` and `PMU-007` -- 494 and 527 mV at 60 uA -- to the values those records state. Two faults were found while writing the code and both are recorded because neither was visible in review: the absent-reading guard originally sat after the mode guard, so a register that did not answer would have been reported as "read, but not a temperature"; and the fault code `0x2000` would have scaled to 4096 mV, above every row of the table, and read as an implausibly cold battery instead of as an open pin. `td_battery_t` gains `ntc_tenths_c` at the end of the struct plus the `TD_NTC_NONE` and `TD_NTC_NOT_TEMP` sentinels, and `api/tinydesk.api` is re-recorded at 701 declarations with three additions and nothing removed -- the minor shape `STABILITY.md` sanctions, with `TD_VERSION` still untouched because that is the author's call. The voltage field STAYS rather than being replaced: a board without the part data can report it honestly, System Monitor falls back to it when a board fills in only the voltage, and `test_native_apps` now covers all three paths -- temperature, voltage fallback, and neither -- which meant updating that test rather than leaving it green against a contract that had changed. The boot log deliberately still prints raw millivolts (`ts 494mV`) because that is the diagnostic and a derived temperature cannot be checked against itself. The full host suite is 39 of 39 with the new test included, the firmware builds clean under `-Werror`, and only the app region at 0x10000 was written with the hash verified. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 31, and Status uses `PASS` for the user test because the feature was verified on the glass.

#### Next Steps:

The thermistor is now calibrated and visible, which closes the measurement work that began when the schematic marked the position `NC`; what is left of it is small. Logging the TS temperature alongside each `chg go` and `chg done` would give a temperature history for free from events the driver already handles, and it would put a reading next to the moment charging starts, which is the one time the charger's thermal window matters -- and the window itself is now known to be real, since `PMU-007` records that `reg50H[4] = 0` means this reading gates the charger between roughly 0 and 55 C. Separately, the current source sitting at 60 uA rather than its documented 50 uA default deserves one deliberate observation across a battery disconnect: if it is an eFuse value it will persist, and if a previous firmware wrote it then a disconnect restores 50 uA, and the conversion follows it because it reads rather than assumes -- but nobody has watched that happen. A fresh backup is now due and is the most valuable outstanding action, since `working-2026-11-12-1010.bin` predates this build. Everything else is unchanged and unblocked: the taskbar-at-top option is verified at all five points and left defaulting to bottom at the operator's direction, both pushers work and both send Enter, audio is a beep with no playback path, the display's tearing-effect line is configured and unread, the screen reader now lives in the fork under `tools/`, and the unbuilt outboard parts are as they were -- the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

- ports/twatchultra/main/ts_convert.c
- ports/twatchultra/main/ts_convert.h
- ports/twatchultra/main/charge.c
- ports/twatchultra/main/charge.h
- ports/twatchultra/main/power.c
- ports/twatchultra/main/CMakeLists.txt
- include/tinydesk/td_sysinfo.h
- apps/sysmon.c
- tests/test_ts_convert.c
- tests/test_native_apps.c
- api/tinydesk.api
- CMakeLists.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 30 COMMIT Unreleased 2026-11-12T10:35:44-07:00

#### Coming From:

Unreleased a735d57

#### Purpose:

Find the battery thermistor's part data, which the previous two cycles had concluded was unavailable.

#### Outcome:

The part data exists and is authoritative, and the search that found it also corrected two of this project's own claims. `PMU-007` now records it: the NTC sits between the AXP2101's TS pin and ground, nominally 10 kohms at 25 C, driven by a constant current selected in reg50H[1:0] as 20, 40, 50 or 60 uA; the AXP2101 datasheet (X-Powers V1.4, section 6.7.4.4 and Table 6-3) works the whole thing through with the Mitsubishi TH11-3H103F reference part and tabulates resistance, detected voltage and ADC code from -20 to 60 C, with 10.00 kohms and 0.500 V and ADC 0x3E8 at 25 C at 50 uA. It was found in the datasheet bundled inside lewisxhe/XPowersLib rather than anywhere in LilyGo's own tree: LilyGo publish the pack as 1100 mAh at 3.7 V and say nothing about its thermistor, and the board schematic marks the position `NC NTC` and gives no value at all. Rendering the schematic at 600 dpi did settle the topology even without a part number: the TS pin reaches the battery connector P2 through the net named NTC, whose pin 2 is the pack's thermistor contact, with R8 drawn from that net to ground. The datasheet explains R8 -- it is the 10 kohm pull-down the part wants when a pack has no thermistor -- so the `NC` in the drawing most likely means that fallback resistor is not fitted while the pack's NTC is, which is consistent with the measurement and with the schematic text being the only evidence that ever suggested otherwise. TWO CORRECTIONS FOLLOW, and both are this project's rather than the vendor's. The excitation current is NOT the 50 uA the datasheet's table assumes: reg50H reads 0x07 on this watch, which is 60 uA in reg50H[1:0] against a documented power-on default of 50, with reg50H[4] = 0 so the TS input genuinely gates the charger. `PMU-006` had guessed 80 uA by fitting a curve to two readings, and that guess was wrong; the same millivolts therefore decode to 28.6 to 30.3 C rather than the 23.7 to 25.4 C a 50 uA assumption would have produced. Since the current source is not at its default, a conversion must read it rather than assume it, which is now the recorded consequence rather than an inferred one. And the Steinhart-Hart coefficients published in XPowersLib were checked and deliberately not used: they place 10.00 kohms at 22.4 C where the datasheet's own table places it at 25 C, a disagreement of about 2.6 C, so the table wins. The decoder was validated against that table rather than against itself -- interpolating Table 6-3 returns exactly 25.0 C for 500 mV and 30.0 C for 416 mV, the two tabulated points it is asked about -- and the table is self-consistent at its stated 50 uA, which is why scaling by the actual current is the right correction rather than a fudge. A SAFETY FACT IS NOW ON RECORD that was not before: because reg50H[4] = 0, the TS reading does not merely report a temperature, it moves the charger's window. The part pauses charging when TS leaves reg54H/reg55H (defaults about 0 C and 55 C) and pauses the boost outside reg56H/reg57H (about -10 C and 60 C), so writing reg50H to change the current, or to declare TS unused, would change when this board refuses to charge. That is the reason the port reads that register and does not write it. `PMU-006` was revised in place rather than left standing, since its calibration caveat is now answered and its 80 uA arithmetic was wrong; the thermistor conclusion it reached was not affected, because that was established from the direction of travel and not from any assumed coefficient. Nothing was built or flashed. The required `core-syntax.md` audit was run against the whole `.ai/` diff: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 30, Files Modified reads `None.` because the cycle changed only project-control files, the index and routing table gained their entries, and all 89 records parse individually with no duplicate ids.

#### Next Steps:

The conversion this unlocks is the next code change and it is small: replace the millivolt display with a temperature, computed from Table 6-3 by interpolation and scaled by the current actually found in reg50H[1:0] rather than by the table's 50 uA. The shape worth using is the table rather than the Steinhart coefficients, because the table is the primary source and the coefficients available in the wild disagree with it by 2.6 C. What that change should NOT do is write reg50H, including to force the current to 50 uA so the table applies directly: reg50H[4] = 0 means that register decides when this board will refuse to charge, and changing it to simplify arithmetic would trade a safety behaviour for a convenience. Two smaller items belong with it. The `NC` annotation on the schematic should be recorded as meaning the unused pull-down rather than an absent thermistor, since that reading is now supported by both the datasheet's description of R8 and by the measurement. And the current source being off its default is worth watching across a real power cycle: if it is an eFuse value it will persist, and if a previous firmware wrote it then a battery disconnect will restore 50 uA and the conversion must follow it rather than break, which the read-don't-assume rule already covers but which nobody has yet confirmed on hardware. Everything else is unchanged and unblocked: the taskbar-at-top option is verified at all five points and left defaulting to bottom at the operator's direction, the pushers work and both send Enter, audio is a beep with no playback path, the display's tearing-effect line is configured and unread, and the unbuilt outboard parts are as they were, with the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`. A fresh backup is due once the conversion lands, since `working-2026-11-12-1010.bin` predates it.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 29 COMMIT Unreleased 2026-11-12T10:27:17-07:00

#### Coming From:

Unreleased 8835330

#### Purpose:

Verify the taskbar-at-top option on the glass and close the ten-cycle-old item of the screen reader living in a temporary directory.

#### Outcome:

All five checks passed on the watch, and the cycle's real finding was in the instrument rather than in the feature. The taskbar moved to the top edge -- `[Start] root Offline 10:23` on row 1 with row 26 now plain desktop; the Start menu opened at row 2 hanging DOWNWARD from the bar rather than upward from the bottom; a maximised System Monitor took rows 2 to 26 with its top border immediately below the bar, so `desk_of()` is moving the origin as intended; and with desktop icons enabled for the test the first icon row landed at row 3, visible rather than behind the bar, which is the `slot_y()` offset working. Persistence was checked by a real reboot, not by assuming the settings write landed: uptime afterwards read 22 seconds and the bar was still at the top, so the settings-blob byte survives. THE SCREEN READER LIED AND THE OPERATOR CORRECTED IT. Reading the desktop back over USB reported twenty-six blank rows with no taskbar anywhere, and that was written up as a missing bar before the operator said it looked fine on the glass -- which it did. The cause is in `tools/watch-screen.py`: the desktop brackets its cursor-position query with save (ESC 7) and restore (ESC 8) around a jump to 999;999, and the model ignored both, so it was left parked at row 999 and wrote every glyph off-screen. A desktop drawn correctly and a desktop drawn nowhere are indistinguishable when the reader is the only witness, which is the same trap as a panel that returns success and draws nothing -- and this project has that one recorded three times already for the display. Fixed, and POSITIONS ARE CLAMPED as well, because 999;999 is a legitimate size query and must not poison every later write. A SECOND FAULT was found while fixing the first: a fixed-duration drain is not enough, because bytes from the previous frame are still in flight and parsing a partial frame reports a stale screen; `read_screen()` now drains until the line has been quiet for 150 ms and sends Ctrl+L itself when asked for a plain read, so the default path is the one that works rather than the one that needs remembering. THE TOOL NOW LIVES IN THE FORK at `tools/watch-screen.py` with both lessons in its docstring, which closes an item carried since entry 16 and rewritten or reused in ten consecutive cycles; it takes keys and a wait as arguments and self-repaints, so the next cycle reads the glass with a working instrument rather than rebuilding one. Two state changes were made on the watch for these tests and both are recorded: desktop icons were enabled to check the icon-grid offset and have been turned back off, restoring the operator's configuration, and the Settings window was closed with the setting saved. Nothing was built or flashed -- the firmware under test is `bd0c819`, the build the operator has been using since the NTC row was added. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 29, Files Modified lists one deterministic tool and no `.ai/` path or generated artifact, and Status uses `PASS` for the user test because the operator reported the taskbar looking right on the glass.

#### Next Steps:

The lesson worth carrying out of this cycle is not about the taskbar: an instrument that reports a blank screen is indistinguishable from a feature that draws nothing, and the only reason this was caught is that a person looked at the hardware. Any future cycle that concludes something is MISSING on the basis of the USB reader alone should say so as a reading rather than as a fact, and should check the reader against a known-good screen first -- which is now cheap, because `tools/watch-screen.py` is in the tree and its failure modes are documented. What remains on the watch is the thermistor calibration, needing the NTC's nominal resistance and beta and the charger's excitation current; logging the TS voltage alongside each `chg go` and `chg done` would give a temperature history for free in the meantime. Both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would argue for, and now that the bar can be at either edge there is a small layout question worth a decision rather than a default: with the bar at the top, its row sits directly under the glass's inert top margin, which reads better than the bar being at the bottom with the `Tab`/`Enter`/`Ctrl+Q` strip immediately below it -- so the port should probably ship `td_wm_set_taskbar_top(true)` rather than leaving the operator to find the checkbox. Audio is a beep with no playback path, and the display's tearing-effect line is configured and unread. The unbuilt outboard parts are as they were: the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card and the image cache, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

- tools/watch-screen.py

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 28 COMMIT Unreleased 2026-11-12T10:15:13-07:00

#### Coming From:

Unreleased e3d8baa

#### Purpose:

Capture a full 16 MiB backup of the state the operator accepted, and find the root cause of the silent reset that had now recurred three times.

#### Outcome:

`~/twatch-backup/working-2026-11-12-1010.bin` is the current recoverable state, 16 MiB read at 576.6 kbit/s in 232.8 seconds, sha256 `ad82e52ca5d3fc29cbbdd0ca72574fd56fe33dfb329d5d49e7b484c6e37ba3ab`, verify OK against the chip with the board held in the stub across read and compare, and the stored `.sha256` re-checked afterwards. The 2,565,216-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `ab7bbd12`, so the image holds the firmware that was running rather than an older build; the strings this session's work introduced are all present -- `ntc%d`, `Taskbar at the top`, `chg go`, `vbus out`, `ts %dmV`, `twatch-pwrkey` and `twatch-pusher` -- both image magics are 0xe9, and the partition table parses as nvs, phy_init, ota_0, ota_1, otadata and storage at the offsets `partitions.csv` declares. The littlefs partition holds 1,995 written blocks of 2,528 against the previous backup's 1,993, which is the two blocks of NVS and settings writes this session produced. THE SCRIPT'S RESET WAS FINALLY DIAGNOSED RATHER THAN RECOVERED FROM. Three consecutive backups had printed `board stayed silent` and then brought the board back on a second attempt, so the fault was visible, the artifact was always sound, and the cause stayed hidden behind a recovery path that worked. It is `--before no_reset`, which is wrong in both directions, and both were measured. From the stub it CONNECTS AND EXITS 0 while performing no effective reset, because this board is on a native USB-JTAG port where RTS is not wired to EN -- the pulse is not felt, the stub keeps running and the desktop never appears, so the failure is silent and looks like the board's fault. From a running board it cannot even connect, failing its sync with `Invalid head of packet` because the desktop is speaking raw VT100. `--before default_reset` works from either state, so `reset_board` now uses that and nothing else, and `verify_flash` stopped asking for `--after hard_reset`, which was the same broken pulse from the same stub and bought nothing. The tail is simpler as a result -- reset, check, retry once, warn -- instead of relying on a fallback that had become the normal path. PROVEN END TO END RATHER THAN ARGUED: the run after the change printed `resetting the board`, `waiting for the board to come back` and `board is up`, which is the first attempt with no fallback used, against three consecutive earlier runs that had all needed the second. An isolated harness had already confirmed the fixed function returns 0 and reboots a running board -- `Uptime: 00:00:17` after the call -- and that isolation is what showed the first attempt had been reporting success without doing anything, since a `no_reset` connection to a stub returns 0 whether or not the board wakes. Two further backups were taken during the investigation, at 0959 and 1005, and both are sound and verified; 1010 is the one to restore because it is the first taken with the reset working, and the earlier two are kept rather than deleted since a backup is cheap and a deleted one is not recoverable. Nothing was built or flashed this cycle. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 28, Files Modified lists one deterministic tool and no `.ai/` path or generated artifact, and Status uses `N/A` for build and deployment because this cycle changed a script and read the flash without building anything.

#### Next Steps:

The backup situation is now correct and the script can be trusted to say whether the board came back, which closes an item that had been carried forward since entry 7 and had been "fixed" twice without the root cause being found -- a warning worth carrying: both earlier fixes made the symptom survivable and neither made it absent, and the thing that finally found it was an isolated harness rather than reading the script again. What remains on the watch is the thermistor calibration, which needs the NTC's nominal resistance and beta and the charger's excitation current; logging the TS voltage alongside each `chg go` and `chg done` would give a temperature history for free in the meantime, from events the driver already handles. The taskbar-at-top option is flashed and still waiting on the operator's four checks -- the bar moves, the Start menu hangs downward from it, a maximised window does not hide under it, and the desktop's top row of icons is not behind it -- followed by a reboot to confirm the setting persists. Both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would argue for. Audio is a beep with no playback path, and the display's tearing-effect line is configured and unread. The screen model that reads the Log Viewer over USB is in a temporary directory for the tenth cycle and belongs in the fork under a tools directory; it has now been rebuilt or reused in every cycle this session and moved nowhere. The unbuilt outboard parts are as they were: the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card and the image cache, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

- scripts/watch-backup.sh

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 27 COMMIT Unreleased 2026-11-12T09:58:30-07:00

#### Coming From:

Unreleased f1c9da7

#### Purpose:

Put the battery thermistor's voltage on System Monitor, where the cell's temperature can be watched rather than only logged at boot.

#### Outcome:

At `bd0c819` on branch `t-watch-ultra`, System Monitor's battery row reads `Battery:100%  4.17 V full ntc494`, and the reading moves with the cell -- `ntc497` on one read and `ntc494` a few minutes later as it cooled. It sits on the battery line rather than a row of its own because it is the same subject and a 26-row screen cannot spare a row for seven characters; the whole line is 32 of the 38 columns the window shows at this board's grid. MILLIVOLTS AND NOT DEGREES, deliberately: turning the TS voltage into a temperature needs the NTC's nominal resistance and beta plus the excitation current the charger applies, none of which is known here, and PMU-006 records why a plausible number derived from a guessed part number is worse than no number at all. `td_battery_t` gains `ntc_millivolts` appended at the end of the struct, which is the minor change `STABILITY.md` sanctions, and `api/tinydesk.api` was re-recorded at 699 declarations with that one struct line changed and nothing removed; a board with no thermistor reports 0 and its battery line is byte-identical to before, so the addition is free for every other port. `charge_ts_mv()` is new and is the single decoder for registers 0x36/0x37, with `charge_ts_report()` now calling it instead of duplicating the shift -- so the boot line and the twice-a-second tick physically cannot disagree -- and it logs nothing and does nothing else, because a diagnostic per call at that rate would bury the 64-line ring, which is the fault this port has already recorded twice. THE FINDING WORTH CARRYING IS THAT THE SUITE PASSED WITHOUT TESTING ANY OF THIS. `check_controls()` audits only FOCUSABLE widgets and these are labels, so the 40x26 layout pass said nothing about whether the line fits; and nothing in the suite supplied a battery at all, so the thermistor branch had never been rendered by any test -- a green suite that would have stayed green whatever that code did. `tests/test_native_apps.c` now fakes a battery with and without a thermistor and checks both the text and the width at this board's grid, and TWO NEGATIVE CONTROLS EACH FAIL A DIFFERENT ASSERTION rather than the same one twice: formatting no `ntc` at all fails the text check, and a longer label fails the width check at 38 columns, which was verified separately after the first attempt tripped the text assertion and proved nothing about width. Verified on the board rather than only in the host tests: the firmware builds clean under `-Werror`, the app region was written with the hash verified, and System Monitor was read back over USB showing the line with a thermistor value that changed between reads, so the tick path is live rather than a boot-time snapshot. The host suite is 38 of 38. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 27, and Status uses `PASS` for the user test because the operator reported the feature working.

#### Next Steps:

The thermistor is now visible but still uncalibrated, and that is the next real step rather than a cosmetic one: with the NTC's nominal resistance and beta and the charger's excitation current, `ntc_millivolts` becomes a battery temperature, and then the charger's own temperature protection becomes reportable instead of merely active. Two cheaper things are worth doing first because they need no new part data. Logging the TS voltage alongside each `chg go` and `chg done` would give a temperature history for free from events the driver already handles, and it would put a thermistor reading next to the moment charging starts -- which is the one time the charger's thermal protection matters. And the battery percentage is still unverified above 100, which only draining the watch can settle and which would now be corroborated by the cell temperature falling as the pack empties. A fresh backup remains due and is overdue: `working-2026-11-12-0859.bin` predates the whole charge cycle, both pusher builds' successors, the taskbar option and this row. Everything else is unchanged and unblocked: the taskbar-at-top option is flashed and waiting on the operator's four checks; both pushers send Enter with F10 on the BOOT pusher the shape I would argue for; audio is a beep with no playback path; the display's tearing-effect line is configured and unread; the screen model that reads the Log Viewer over USB is in a temporary directory for the ninth cycle and belongs in the fork under `tools/`; the unbuilt outboard parts are as they were, with the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code; and the standing queue is otherwise as it was, including the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

- include/tinydesk/td_sysinfo.h
- apps/sysmon.c
- api/tinydesk.api
- tests/test_native_apps.c
- ports/twatchultra/main/charge.c
- ports/twatchultra/main/charge.h
- ports/twatchultra/main/power.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 26 COMMIT Unreleased 2026-11-12T09:50:21-07:00

#### Coming From:

Unreleased c9b839e

#### Purpose:

Make the taskbar's edge a settable option, and settle whether the board has a battery thermistor and what it actually measures.

#### Outcome:

At `ae4e38d` on branch `t-watch-ultra` the taskbar can sit on either edge, and the thermistor question is answered: one IS fitted, it reads the cell rather than the SoC, and the schematic marking `NC NTC` is wrong for this unit. THE TASKBAR IS SETTABLE RATHER THAN FIXED because the right edge is a property of the display and not of the desktop -- a 26-row watch reads better with the bar at the top -- while a 25-row terminal has no opinion, so `td_wm_set_taskbar_top()` defaults to false and every existing port is unchanged, and the desktop keeps the same number of rows either way. The work is mostly one helper: five places independently computed `main_bottom() - taskbar_h()`, and moving the bar creates five chances to disagree about where it is, which is the failure mode this file already records once as a bar drawn in one place and hit-tested in another -- presenting as a dead button rather than a geometry bug. The painter, the hit test, the clock click, the picture-cover rectangle and the Start menu anchor all ask `taskbar_y()` now. Three sites needed more than a coordinate: `desk_of()` must move the origin down for a top bar or a maximised window starts underneath it; `slot_y()` must shift the icon grid or its first row lands behind the bar, drawn and hit-tested correctly but invisible; and `menu_open_at()` clamped to `(0, desk - h)`, which assumed the desktop began at screen row 0 and was true only for a bottom bar on a single display. Settings gains a checkbox and the choice persists in the settings blob as a byte after `desk`, with old blobs zero-filled past what was read so an existing user keeps the bottom bar. `tests/test_wm.c` gains `test_taskbar_top`, pinning where the bar is drawn, where it is hit-tested and where the desktop's rows begin, and THREE NEGATIVE CONTROLS EACH FAIL A DIFFERENT ASSERTION: making the painter ignore the setting, making the hit test ignore it, and dropping the `desk_of()` offset. `api/tinydesk.api` re-recorded at 699 declarations with two added and none removed, the minor shape `STABILITY.md` sanctions, and `TD_VERSION` deliberately not bumped because that is the author's call. THE THERMISTOR WAS SETTLED BY WATCHING IT CHANGE RATHER THAN BY READING A VOLTAGE. The AXP2101's TS pin is the battery's NTC input and it reads 474-475 mV on a warm watch, which on its own proves nothing: a fixed divider and a leaking floating pad both produce a plausible number, and the schematic marks the position `NC NTC`. What settled it was a temperature excursion and the direction of travel. After the watch cooled, the die sensor fell from 37.8 to 32.8 C and TS rose from 475 to 527 mV; then, charging, TS fell monotonically from 526 to 515 mV across 260 seconds while the die held at exactly 35.8 C in all twenty-seven samples. That single run rules out three explanations at once -- a fixed divider would not move, an empty pad would be noisy rather than smooth, and the PMU's own ADC reference sits at the die temperature, which did not move. A fifth reading after a reboot gave 511 mV. So the pad is populated and the schematic marking should not be trusted, which is the third document today to be contradicted by the hardware after the README's `/fs` row and the SD card note. THE SECOND FINDING IS THE ONE WORTH KEEPING: during that run the SoC sat at exactly 35.8 C while the cell warmed and TS fell, because the cell is a separate thermal body with far more mass. The die sensor is therefore not a proxy for the battery and the battery thermistor is not a proxy for the chip -- they diverge by several degrees whenever the load or the charger changes, and this cycle watched them do it. What is NOT established is an absolute calibration: turning TS millivolts into degrees needs the NTC's nominal resistance, beta and the part's excitation current, none of which is known, and the observed sensitivity of roughly 2 to 10 mV per degree against the ~19 a 10k/B3950 cell at 80 uA would predict is explained by the cell and the SoC being at different temperatures rather than by a measured beta. So `charge.c` reports raw millivolts and deliberately invents no temperature, because a plausible number from a guessed part number is the failure this project records in LCD-008 and PMU-001. TWO MISTAKES OF MINE ARE RECORDED RATHER THAN SMOOTHED OVER. First, entry 25 claimed `PMU-005` had been revised with the bank-2 charge bits and the observed sequence, and while the consequence was revised the `verification` note never landed -- a `str.replace()` whose anchor did not match, silently returning the string unchanged, with the script printing success regardless. The note is in `PMU-005` now and this entry is where it belongs, since entry 25 is settled history and is not rewritten. Second, one flash in this cycle wrote a STALE binary: `idf.py build 2>&1 | grep ... && esptool ...` let a failed build continue to the flash, because a pipeline reports the status of its last stage and `grep` succeeded by finding the error text. That is the same trap `scripts/watch-backup.sh` documents at length and I walked into it anyway; the fix used here is to redirect to a file and check the real exit status, which is what the rest of this cycle then did. `PMU-006` was added to `core-reference.md` with the four readings, the direction of travel, the ruled-out explanations and the calibration caveat, and every one of the 88 records parses individually with no duplicate ids. Build is clean under `-Werror`, the host suite is 38 of 38 including the three negative controls, and only the app region at 0x10000 was written. User Test is `NOT RUN` because the taskbar option has been flashed but not yet toggled by the operator.

#### Next Steps:

The taskbar option is ready and needs the operator: open Settings, tick `Taskbar at the top`, and confirm the bar moves, that the Start menu hangs downward from it, that a maximised window does not hide under it, and that the desktop's top row of icons is not behind it -- those are the four things the code was written to get right and the test pins. Then reboot and confirm the setting survives, which exercises the settings-blob byte. For the thermistor, the remaining work is calibration and it needs a source this project does not have: the NTC's nominal resistance and beta, and the excitation current the AXP2101 applies to the TS pin. With those, TS millivolts become a real battery temperature and the charger's temperature protection becomes reportable rather than merely active. Absent them, the honest state is what exists now -- the raw voltage on screen at boot, which is enough to tell a working thermistor from a missing one and not enough to say how hot the cell is. Two other things are worth doing while the cell's temperature is observable at all: charging could log the TS voltage alongside each `chg go` and `chg done` rather than only at boot, which would give a temperature history for free from events the driver already handles; and the battery percentage is still unverified above 100 percent, which only draining the watch can settle and which would now be corroborated by a falling cell temperature. A fresh backup is due, since this build carries the taskbar option and `working-2026-11-12-0859.bin` predates both it and the charge work before it. Everything else is unchanged and unblocked: both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would argue for; audio is a beep with no playback path; the display's tearing-effect line is configured and unread; the screen model that reads the Log Viewer over USB is in a temporary directory for the eighth cycle and belongs in the fork under a tools directory; the unbuilt outboard parts are as they were, with the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code; and the standing queue is otherwise as it was, including the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, the unexercised RTC write path, and the pre-existing YAML fence defect in `core-reference.md`.

#### Files Modified:

- src/wm.c
- include/tinydesk/td_wm.h
- apps/settings.c
- tests/test_wm.c
- api/tinydesk.api
- ports/twatchultra/main/charge.c
- ports/twatchultra/main/charge.h
- ports/twatchultra/main/link_twatch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 25 COMMIT Unreleased 2026-11-12T09:22:30-07:00

#### Coming From:

Unreleased 41d3c92

#### Purpose:

Make the watch react to the charger rather than merely observe the cable, and put the die temperature on the screen instead of letting the window cut it off.

#### Outcome:

At `3a71c6d` on branch `t-watch-ultra`, the AXP2101's own charge start and finish events are enabled and handled, and System Monitor shows the chip's temperature on a row of its own. THE CHARGE WORK STARTED FROM A DISTINCTION THE PORT COULD NOT MAKE. `vbus in` reported that a cable had arrived, which is not the same as charging: a cable in a dead charger, a cell that never starts taking current, and a healthy charge all look identical if only VBUS is watched. The part reports three separate things and the port was listening to one of them. Bank 1 of the status block carries the VBUS edges at bits 7 and 6, and bank 2 carries CHARG at bit 3 and CHARG_DONE at bit 4, with enable partners at 0x40..0x42; the measured bank 2 enable was 0x5f and the two charge bits were off. They are enabled now and VERIFIED BY READBACK rather than by the write returning OK -- `irq2 5f c1` where the c1 means both bits are in the register -- because this project has already been taught that lesson once, when the speaker rail's log line printed the values read before the write and an earlier claim that it confirmed the rail was wrong on that basis. THE HANDLING MOVED OUT OF pwrkey.c. The button driver owns the pin and is the thing that clears the status, so it keeps that job and hands its three status bytes to `charge_on_irq()`; what a VBUS or charger edge MEANS now lives in charge.c with the rest of the charging knowledge, and pwrkey.c lost its own copy of the VBUS bits so the two cannot drift. The handler logs transitions only -- `vbus in`, `vbus out`, `chg go`, `chg done` -- comparing against what it last saw, so a bouncing charger cannot repeat a line, and the boot state is fed through the same path so the log starts from what the part reports rather than from unknown and the first line is never a removal with no matching arrival. ORDERING IS LOAD-BEARING AND WAS REASONED ABOUT BEFORE THE FIRST FLASH: `charge_watch_enable()` runs before `pwrkey_init()`, because enabling a source can make a status bit pending and `pwrkey_init()` is what clears the status and refuses to arm while the line reads low -- arming first would have left that guard looking at a line the very next call was about to pull down. THE RESULT IS THE SEQUENCE THAT PROVES THE DISTINCTION IS REAL, and it is better evidence than the cycle expected. The operator unplugged and replugged the watch, and the log reads `chg done` at 186711 ms while the cable was still attached, `vbus out` at 188450, `vbus in` at 192482, `chg go` at 192497 and `chg done` again at 202616. So the charger finished its cycle before the cable came out, started again fifteen milliseconds after it went back in, and completed ten seconds later against a cell that was already near full -- which is a working charge controller doing exactly what the three bits say it should, and it also corrects a prediction this session made in advance: a full cell does NOT necessarily skip CHARG, it charges briefly and finishes. THE DIE TEMPERATURE WAS ALREADY READ AND ALREADY BROKEN ON THE GLASS. `chip_temp` existed in the shared ESP-IDF app and System Monitor called it, but the value shared the Tasks line and the window clipped it, so the screen showed `Tasks: 18  CPU: 240 MHz  Chip` with no number after it -- worse than not showing it at all, because it reads as a reading of nothing. It has its own row now, created only when the sensor actually answers, probed once in `launch()` because that question decides the layout rather than the reading; the window grows by that row and `on_close()` clears the new pointer, which is the use-after-free shape this file records from the battery row. Live on the watch: `Chip: 37.8 °C`, a plausible idle temperature for this part with the panel lit. Both changes build clean under `-Werror` and the host suite is 38 of 38. `PMU-005` was revised in place to carry the bank 2 bits and the observed sequence, since it is the same subject and a second record for two more bits of the same register block would have split one fact in two. The required `core-syntax.md` audit was run against the whole `.ai/` diff: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 25, and the reference change is a revision inside one record with all 87 records still parsing and no duplicate ids.

#### Next Steps:

A fresh backup is due and is the next action with real value, since this build carries `charge.c`'s transition handling and the System Monitor temperature row and `working-2026-11-12-0859.bin` predates both. What charging leaves open is smaller than it was and none of it is blocked: the cell's NTC thermistor is still unconfirmed -- TS measurement is enabled and reads 0xff in the enable byte, but a missing thermistor produces a plausible-looking temperature rather than an error, so the check is to read the TS ADC channel and compare it against a value the die sensor can corroborate, which is now easy because that sensor works; and the reported capacity is still unverified beyond 100 percent, which only draining the watch can settle. Both are measurements rather than code. Two design questions remain open by the operator's choice: both pushers send Enter and the useful shape is probably F10 on the BOOT pusher, since it is global in the window manager and works with nothing focused; and what the desktop should do with the charge transitions now that it can see them, which so far is a log line and nothing more -- refreshing the battery row at the moment a charge starts would be the smallest visible version of that. Everything else is unchanged and unblocked: audio is a beep with no playback path, the display's tearing-effect line is configured and unread, the screen model that reads the Log Viewer over USB is in a temporary directory for the seventh cycle and belongs in the fork under a tools directory, and the unbuilt outboard parts are as they were, with the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam is parked with its finding recorded, the top margin is the last unpainted region, the full-screen margin question waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched.

#### Files Modified:

- ports/twatchultra/main/charge.c
- ports/twatchultra/main/charge.h
- ports/twatchultra/main/pwrkey.c
- ports/twatchultra/main/link_twatch.c
- apps/sysmon.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 24 COMMIT Unreleased 2026-11-12T09:11:48-07:00

#### Coming From:

Unreleased d9cd6ef

#### Purpose:

Establish what the AXP2101 is doing about charging, which this port had never read or configured, and turn the answer into a check that runs at every boot.

#### Outcome:

At `ab74901` on branch `t-watch-ultra`, `ports/twatchultra/main/charge.c` reads the AXP2101's six charge registers at boot, decodes them, logs them on three short lines and warns if any value has drifted from what it should be -- and writes nothing, because the measurement says nothing needs writing. The cycle began as configuration work and correctly turned into measurement: charging is the one subsystem on this board where a wrong write damages hardware that is not easily replaced, and the obvious source for what to write disagreed with itself. The port has never written a charge register, and the part's power-on values are already right for the cell. Read off the watch: `0x18=0x0a` with the cell charger at bit 1 and the fuel gauge at bit 3 both set, `0x30=0xff` with NTC temperature sensing on, `0x61=0x05`, `0x62=0x0b`, `0x63=0x05`, `0x64=0x03`; those decode as 125 mA precharge, 500 mA fast charge, 125 mA termination and a 4200 mV full-charge voltage. LilyGo documents this board's pack as 1100 mAh at 3.7 V, warns to keep the charging current below 500 mA so the PMU does not overheat, and warns against exceeding 0.5 C, which for 1100 mAh is 550 mA; the defaults sit inside all three at 0.45 C, and 4200 mV is the standard full-charge voltage for a 3.7 V Li-ion cell rather than the 4350 or 4400 steps that belong to high-voltage cells. So the deliverable is a check rather than a setting, which is the stronger of the two: `charge_report()` logs `cv4200 ic500` and stays silent unless a value moves, so a future change that breaks charging cannot pass unnoticed, whereas a comment saying the defaults are correct would still be true the day they stopped being true. It deliberately does not write a voltage, because choosing one means reading a cell's spec and writing 4350 or 4400 on this pack would overcharge it. A VENDOR DEFECT WAS FOUND AND IS THE REASON COPYING WAS RULED OUT. LilyGoLib's Ultra init calls `setChargeVoltage(4288)`, and SensorLib's `setChargeVoltage()` accepts only 5000, 4000, 4100, 4200, 4350 and 4400 -- 4288 falls through to `default: return false`, so the call does nothing and the register keeps its power-on value while the source reads as though it asks for 4288. Their `setFastChargeCurrent(512)` is harmless for a different reason: 512 encodes to the same register code as 500 mA. A project copying either call would have believed it had configured charging. THE DECODER WAS CHECKED AGAINST THE ENCODER BEFORE IT WAS TRUSTED, since a wrong decode would make the safety check wrong in exactly the direction that matters: a host harness round-tripped all 40 fast-charge steps from 25 to 1000 mA and all six voltage steps through SensorLib's own encoding -- 46 checks, no failures -- and confirmed that 4288 does not encode at all. On the board the boot prints `180a 30ff 6105`, `620b 6315 6403` and `cv4200 ic500` with no charge warning, while System Monitor simultaneously reports `Battery:100%  4.17 V full`, so the state decode and the configuration decode agree. Nothing was flashed beyond the app region at 0x10000 and the write was hash-verified. `PMU-005` was added to `core-reference.md` with the register map, the measured values, the cell's limits and the vendor defect, because this is an external fact the file did not hold and `.ai/core.md` requires such findings to be recorded with the existing syntax; every one of the 87 records still parses individually against the template with no duplicate ids, and the pre-existing YAML fence defect in the fast-lookup index is unchanged -- it fails identically at `HEAD`, which was checked rather than assumed. The required `core-syntax.md` audit was run against the whole `.ai/` diff: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 24, and the reference change is insertion-only.

#### Next Steps:

The measurement leaves a real question that this cycle deliberately did not answer, and it is the one worth answering next: nothing in this port REACTS to charging. The pwrkey driver reports `vbus in` and `vbus out` and System Monitor reads the charge state on its own tick, so the display is correct, but no code acts on the transition -- nothing refreshes the battery line at the moment the cable goes in, and nothing records that a charge cycle happened. That is small and worth doing because it is the difference between a number on a screen and a device that knows it is charging. Two questions about the cell remain genuinely open and are deliberately left open: whether this pack has an NTC thermistor actually fitted, since the PMU's TS measurement is enabled but a missing thermistor reads as a plausible temperature, and whether the reported capacity is trustworthy, since PMU-001 records that a freshly powered gauge reports above 100 percent until it settles and every reading so far has been at 100. The first is answerable by reading the TS ADC channel and comparing against a plausible ambient; the second by discharging the watch and watching the gauge fall. Everything else is unchanged and unblocked. Both pushers work and both send Enter, which remains a placeholder: F10 on the BOOT pusher is the shape I would argue for, because it is global in the window manager and works with nothing focused. Audio remains a beep with no playback path, and the display's tearing-effect line is configured and unread. The screen model that reads the Log Viewer over USB is in a temporary directory, now for the sixth cycle, and belongs in the fork under a tools directory. The unbuilt outboard parts are as they were: the BHI260AP at 0x28 answering with no driver, the LoRa radio needing ALDO3 and XL9555 P05 before it is powered, the NFC reader needing DLDO1, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam is parked with its finding recorded, the top margin is the last unpainted region, the full-screen margin question waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched. A fresh backup is due once this build is accepted, since it carries `charge.c` and `working-2026-11-12-0859.bin` predates it.

#### Files Modified:

- ports/twatchultra/main/charge.c
- ports/twatchultra/main/charge.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 23 COMMIT Unreleased 2026-11-12T09:05:20-07:00

#### Coming From:

Unreleased 9843f44

#### Purpose:

Make `scripts/watch-backup.sh` report whether the watch came back after its reset, which it twice failed to do while claiming success, and take a backup of the build both pushers now run on.

#### Outcome:

`scripts/watch-backup.sh` now ends by confirming the board is running, and the backup it was extended to protect is `~/twatch-backup/working-2026-11-12-0859.bin`, 16 MiB read at 576.7 kbit/s in 232.7 seconds, sha256 `87feb3989f2183c197200c9f38a10c1b84496eb910d0c7482e72d55bed6dce03`, verify OK against the chip with the board held in the stub across read and compare. THE DEFECT WAS THAT BOTH RESETS FAIL QUIETLY AND NOTHING CHECKED THE RESULT. The verify's `--after hard_reset` leaves the board in the stub when it does not take, and `reset_board` -- written before this cycle -- runs `--before no_reset` against a board that by then is no longer in a stub, so its own failure also lands in the `|| true` at the end of the trap. On entry 15 and again on entry 20 that produced a script exiting 0 while the watch sat silent on both USB and WiFi, which from the outside is indistinguishable from a brick and cost a session to diagnose each time. The tail now opens the port, reads for up to twelve seconds, and treats any byte as alive, because a booting desktop repaints the whole screen and one byte is therefore enough; if nothing arrives it performs a full `--before default_reset --after hard_reset run` and checks again; and if the board is still silent it says so in three lines and states that the backup itself is sound, because the compare against the chip passed before any reset was attempted. Reporting rather than dying is deliberate: a silent board needs a power cycle, it does not invalidate the artifact, and making the script exit non-zero would train its caller to ignore it. THE FAILURE REPRODUCED ON THE FIRST REAL RUN AND THE FIX RECOVERED IT, which is the only evidence worth having here: the script printed `board stayed silent; resetting it again` and then `board is up after a second reset`, so this was not a hypothetical being guarded against but an observed fault being handled. The probe itself was checked in both directions before it was trusted, since a liveness check that cannot fail is worthless: against the live port it returned 128 bytes and exit 0, and against a silent pty it returned zero bytes and exit 1. The backup was then verified independently of the script's own claim. The stored `.sha256` re-checks; the 2,562,688-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `886f9481`; the strings unique to this build -- `twatch-pwrkey`, `pwrkey.pin`, `line low, not armed` and `vbus in` -- are all present in the image, so it is the build carrying the power key and not a snapshot from before it; and the littlefs partition holds 1,993 written blocks of 2,528, identical to the 0820 backup, so the desktop's own files did not move. The board was then confirmed running independently of the script's report, by reading its desktop over USB and finding the taskbar at `09:04`. Nothing was built or flashed this cycle: the firmware on the watch is `715c7b4`, the build the operator accepted, and the backup is a read of it. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 23, Coming From names the pre-cycle `HEAD`, Files Modified lists one deterministic tool and no `.ai/` path or generated artifact, and Status uses `N/A` for the three stages because this cycle changed a script and read the flash without building or deploying anything.

#### Next Steps:

Charging is the largest thing this port can now reach and still does not: `vbus in` and `vbus out` have been observable since entry 22, System Monitor already reads the AXP2101's charge state, and nothing acts on either -- no charge current is set, no termination is configured, and the desktop cannot show whether the watch is charging beyond the state the gauge reports. The next cycle should decide what a `vbus in` event should do rather than leave the reporting without a consumer, and the honest first question is whether the part's default charging behaviour is already correct for this cell, which is answerable by reading the AXP2101's charge registers and the cell's spec before writing anything. Two pushers now exist and both send Enter, which is a placeholder rather than a design: the useful question is whether they should differ, and the shape I would argue for is F10 on the BOOT pusher because it is global in the window manager and works with nothing focused, with the power key left as Enter or given a dedicated meaning once one exists. Everything else is unchanged and unblocked. Audio remains a beep with no playback path, and the display's tearing-effect line is configured in `board.conf` and read by nothing. The screen model that reads the Log Viewer over USB is still in a temporary directory, now for the fifth cycle, and belongs in the fork under a tools directory -- this cycle used it again and again rebuilt nothing of it. The unbuilt outboard parts are as they were: the BHI260AP at 0x28 answering with no driver, the LoRa radio needing ALDO3 and XL9555 P05 before it is powered, the NFC reader needing DLDO1 with its earlier absence verdict still suspect, and the GNSS and microphone with no code and, for the microphone, no evidence that a part is fitted. The standing queue is otherwise as it was: the bell seam is parked with its finding recorded, the top margin is the last unpainted region, the full-screen margin question waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched.

#### Files Modified:

- scripts/watch-backup.sh

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 22 COMMIT Unreleased 2026-11-12T08:58:25-07:00

#### Coming From:

Unreleased b54fc6b

#### Purpose:

Make Pusher A, the watch's power key, reach the desktop, which means driving the AXP2101's interrupt line for the first time in this port.

#### Outcome:

At `715c7b4` on branch `t-watch-ultra`, `ports/twatchultra/main/pwrkey.c` configures GPIO 7 as a pulled-up input with a falling-edge interrupt, reads and clears the AXP2101's interrupt status on every wake, and sends one key per short press into the same `inject()` queue everything else uses. Pusher A is not a GPIO at all: it is the PMU's PEK input and reaches the SoC only as the PMU's own interrupt, `PMU_IRQ` on GPIO 7 -- confirmed in the schematic net list as `PMU_IRQ------IO7` and already used by this project's Linux port as `interrupts = <7 IRQ_TYPE_LEVEL_LOW>` -- so this is an I2C driver with a GPIO wake-up rather than a button driver. THE FACT THAT DECIDES THE WHOLE DESIGN IS THAT THE LINE STAYS LOW UNTIL THE STATUS IS CLEARED. The AXP2101 asserts IRQ while any enabled source is pending, and the pending bits at 0x48..0x4a are write-1-to-clear, so a handler that wakes, reads and returns without clearing is called again immediately and for ever. Every wake here reads all three status bytes and writes them straight back, which is the same thing the PMU's own driver does through regmap-irq with `ack_base` at the status register. That is also why the pull-up is part of the contract rather than a nicety: the line is open drain and the board carries no external resistor, so the internal one is the only thing holding it high, exactly as Linux supplies `PIN(7) FUN0_20MA_IE_WPU`. THE GUARD EARNED ITS PLACE ON THE FIRST BOOT. Before arming, anything pending is cleared and the line is then read; if it is still low the driver declines to arm and says `line low, not armed`, because a lost button is a nuisance while an interrupt that never returns is an unbootable watch. On this boot it printed `st 008030` -- bank 1 bit 7, VBUS_PLUGIN, pending precisely because USB was attached -- cleared it, and armed cleanly. Without that clear the VBUS bit would have held the line low and a falling-edge handler would have stormed, which is the failure BRD-002 records hanging a boot on the Linux side; the guard is the mechanism that makes that history unable to repeat rather than a note about it. A SHORT PRESS AND NOT THE EDGE, for a reason the part forces: the PMU reports both edges and a separate short-press event, and acting on the falling edge would fire on touch but would also fire during a long hold, which on this part is the hardware power-off gesture. The short-press event fires once, on release, with no bounce and no repeat while held; its cost is that the action lands on release, which is what every device's power key does. The measured default enable mask is `0xc3` -- both edges plus VBUS -- so this adds the one bit the part leaves off. VBUS plug and removal arrive on the same interrupt and are logged as `vbus in` and `vbus out` without being acted on, and are the only path this port has ever had to notice the charger; a long press is logged as `key long` and not intercepted, so a shutdown explains itself in the log rather than looking like a crash. `pwrkey.pin` is the board key, 7 by default and -1 to turn the key off without disabling VBUS reporting, and the `board_keys` check caught it as it caught the other two. Verified on the board: `twatch-pwrkey: st 008030` with the pending VBUS bit, `gpio: GPIO[7]| InputEn: 1`, and `twatch-pwrkey: p7 enter` with no storm and the desktop up, so the guard passed with real evidence rather than trivially; and the operator short-pressed twice, which produced `k1` and `k2`, buzzed each time, and then opened Files with a third press logged as `k3` -- three presses, three consecutive lines, no duplicates and no repeats while held, which is the evidence that the short-press event and not the edge is what fires. The one thing checked afterwards and found wanting was my own verification: a read of the desktop to confirm Files was open found it bare, because the operator had already closed it, so that particular confirmation rests on their report and not on a capture -- the log lines are the machine evidence and they are consistent with three presses. Build is clean under `-Werror`, the host suite is 38 of 38 including the board-keys gate, and only the app region at 0x10000 was written with the hash verified. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 22, Coming From names the pre-cycle `HEAD`, Files Modified lists only port files and no `.ai/` path or generated artifact, Status uses allowed values, and the active log holds 22 entries against the 100-entry limit.

#### Next Steps:

A fresh backup is now overdue and is the only item with real value, because two firmware writes have landed since `working-2026-11-12-0820.bin` was taken -- the BOOT pusher and the power key -- and neither is in that image. After it, what this driver makes possible is larger than what it currently does: VBUS plug and removal are now observable for the first time, and charging remains read-but-never-configured, so the natural next step is to act on `vbus in` by starting a charge and to surface the state System Monitor already reads. The key itself remains Enter while the operator decides what the two pushers should mean between them; now that both work, the useful question is whether they should differ -- with the BOOT pusher free to be something else, F10 for the launcher is the shape that makes a physical home button, and Esc is the one to avoid on a bare desktop because it goes to the focused window first and would do nothing. Elsewhere nothing is blocked. Audio remains a beep with no playback path, and the display's tearing-effect line is configured in `board.conf` and read by nothing. Two smaller items persist: the screen model that reads the Log Viewer over USB is in a temporary directory for the fifth cycle running and belongs in the fork under a tools directory, and `scripts/watch-backup.sh` still does not confirm the board came back after its trailing reset, which has been silent twice and is hidden behind `|| true` in the trap -- and both pushers now give that script a cheap way to check, since a key injected at start-up would appear in the log. The standing queue is otherwise as it was: the bell seam is parked with its finding recorded, the top margin is the last unpainted region, the full-screen margin question waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched. The unbuilt outboard parts are unchanged: the BHI260AP at 0x28 answering with no driver, the LoRa radio needing ALDO3 and XL9555 P05 before it is powered, the NFC reader needing DLDO1 with its earlier absence verdict still suspect, and the GNSS and microphone with no code and, for the microphone, no evidence that a part is fitted.

#### Files Modified:

- ports/twatchultra/main/pwrkey.c
- ports/twatchultra/main/pwrkey.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/power.c
- ports/twatchultra/main/power.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/board.example.conf
- api/board_keys.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 21 COMMIT Unreleased 2026-11-12T08:42:04-07:00

#### Coming From:

Unreleased 3cfb371

#### Purpose:

Make the BOOT pusher a key, so a physical side button on the watch reaches the desktop for the first time.

#### Outcome:

At `4e3a8b4` on branch `t-watch-ultra`, `ports/twatchultra/main/pusher.c` configures GPIO 0 as an input with the internal pull-up and a falling-edge interrupt, and sends one key per press into the same `inject()` queue the margin buttons use. The board has two pushers and neither reached the desktop before this: Pusher A is the power key, which is not a GPIO at all -- it reaches the SoC only through the AXP2101's PEK interrupt on GPIO 7 -- and was left alone, while Pusher B is a plain switch on GPIO 0 with a 10K pull-up on the board, declared by LilyGoLib as `Button2 bootButton = Button2(0)` and pinned with `pinMode(0, INPUT)`. The key is Enter for now because the operator has not decided what the button should do, and Enter acts on nearly anything focused, so a press cannot be silently swallowed while that question is open; changing it is one constant and one table entry. THE SHAPE IS DELIBERATELY THE TOUCH INTERRUPT'S: configure the pin, install a falling-edge handler, have the handler notify a task that does the work. That gives the port one mechanism for "a pin tells a task something happened" rather than two, and it means the ISR takes no lock, allocates nothing and calls nothing into the desktop, with the notification coalescing harmlessly. Because the key goes in through `inject()`, a pusher press is indistinguishable from the same key on a Bluetooth keyboard and nothing below the port knows the button exists. TWO PROPERTIES OF THE PIN WERE HANDLED RATHER THAN DISCOVERED, and both would otherwise have looked like faults. GPIO 0 is a strapping pin: holding the pusher while the watch powers on puts the ESP32-S3 into serial download mode, which is the pin's designed function and not something firmware can override. And esptool's resets drive IO0 low, so a flash can leave an edge pending that no finger caused -- the task therefore settles for 250 ms and drops any stale notification before it listens, and the level check rejects a pin that is already high again; the settle sits in the task rather than in `pusher_init()` so it stays off the desktop's startup path. ONE REAL DEFECT WAS CAUGHT ON THE GLASS AND NOT IN REVIEW. `touch_init()` installed the GPIO ISR service first, so the pusher's own call hit `GPIO_CHECK`'s "GPIO isr service already installed", which logs at ERROR level, and the first flashed build carried an `E` line at every boot for a condition that was exactly what was wanted. `board_gpio_isr_init()` in `power.c` is now idempotent and cached, so any driver may call it and only the first call does anything, and both `touch.c` and `pusher.c` go through it; that also removes an ordering question that would have been silent, because the second caller no longer needs to know it is second. `pusher.pin` is the board key, defaulting to 0, with -1 turning the button off as the way to tell a press from here apart from one on the panel, and the `board_keys` check caught the new key on the first build exactly as it did for `touch.int`. Verified end to end on the board rather than by the buzz, which matters because `send_key()`, `haptic_tap()` and the log line all run in the same task: `gpio: GPIO[0]| InputEn: 1` and `twatch-pusher: p0 enter` appear at 3067 ms with no gpio error anywhere in the boot, and the two pins share one ISR service; eight presses produced eight `twatch-pusher: p<n>` lines numbered consecutively with no duplicates, so the debounce holds and a single press is a single event; and the operator opened the Start menu and pressed the pusher, which launched Files -- which the desktop was then read back to confirm, showing `/root` with `Apps/`, `Desktop/` and `Start menu/`. That last check is the one that proves the key arrived rather than merely that the press was detected, and it is why it was run separately. Build is clean under `-Werror`, the host suite is 38 of 38 including the `board_keys` gate, and only the app region at 0x10000 was written, with the write hash-verified. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 21, Coming From names the pre-cycle `HEAD`, Files Modified lists only port files and no `.ai/` path or generated artifact, Status uses allowed values, and the active log holds 21 entries against the 100-entry limit.

#### Next Steps:

What the pusher should send is now a design question rather than a wiring one, and the operator has deliberately left it open -- the summary of what is available is worth stating so the decision can be made against it: F10 is global in the window manager and works with nothing focused, with a window focused, and over a full-screen application, which makes it the natural "home"; Esc is the cancel key everywhere else but goes to the focused window first, so on a bare desktop it does nothing; and Enter, the current mapping, activates whatever is focused. Everything else is unchanged and unblocked. Pusher A remains unbuilt and is the more valuable of the two to add next, because it needs the AXP2101 PEK interrupt on GPIO 7 that this port does not install and that same interrupt is what VBUS plug and removal would need for charge-state handling -- so one piece of work there would give a working power button and the basis for reporting charging. Audio remains a beep with no playback path, charging is read but never configured, and the display's tearing-effect line is configured in `board.conf` and read by nothing. Two smaller items still belong to earlier work: the screen model that reads the Log Viewer over USB is in a temporary directory for the fourth time and belongs in the fork under a tools directory, and `scripts/watch-backup.sh` does not confirm the board returned after its trailing reset, which has now been silent twice and is hidden behind `|| true` in its trap. The standing queue is otherwise as it was: the bell seam is parked with its finding recorded, the top margin is the last unpainted region, the full-screen margin question waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched. The unbuilt outboard parts are unchanged: the BHI260AP at 0x28 answering with no driver, the LoRa radio needing ALDO3 and XL9555 P05 before it is powered, the NFC reader needing DLDO1 with its earlier absence verdict still suspect, and the GNSS and microphone with no code and, for the microphone, no evidence that a part is fitted. A fresh backup is due, since this build is the first to carry the pusher and `working-2026-11-12-0820.bin` predates it.

#### Files Modified:

- ports/twatchultra/main/pusher.c
- ports/twatchultra/main/pusher.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/touch.c
- ports/twatchultra/main/power.c
- ports/twatchultra/main/power.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/board.example.conf
- api/board_keys.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 20 COMMIT Unreleased 2026-11-12T08:30:20-07:00

#### Coming From:

Unreleased eeddfda

#### Purpose:

Capture a full 16 MiB backup of the state the operator accepted, and correct the WiFi row that implied the link returns at boot when it does not.

#### Outcome:

`~/twatch-backup/working-2026-11-12-0820.bin` is the current recoverable state, 16 MiB read at 577.6 kbit/s in 232.4 seconds, sha256 `17437dde4efc015e159c953ec0ad48011eca954d3f4750d8eca23ca4c928f1e4`, and it is verified twice over rather than once. `scripts/watch-backup.sh` reported `verify OK (digest matched)` because it holds the board in esptool's stub from the first byte of the read to the last byte of the compare, so the comparison is against the same flash the read came from; and the stored `.sha256` was re-checked afterwards against the file. Four further checks establish that the image holds the firmware that is actually running rather than an older build, and the first of them is the one that matters: the 2,560,368-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `144f0f20`. Both image magics are 0xe9, so neither region is a truncated read. The partition table parses as nvs at 0x9000, phy_init at 0xf000, ota_0 at 0x10000, ota_1 at 0x310000 and storage at 0x620000, matching `partitions.csv`. And the four strings that are unique to the logging fix are present in the flash image -- `i%lu r%lu`, `irq %d up` and `no irq, poll only` -- while `irq%lu rd%lu p%lu`, the format the previous build carried, is absent, which is a check that the image is this evening's work and not a snapshot taken before it. A first attempt at the app-region comparison used the byte count of the earlier build and reported a mismatch; the fault was the length passed to `dd`, not the image, and it is recorded because a size-sensitive comparison that silently reads the wrong number of bytes is exactly the kind of check that can pass by accident in the other direction. The storage partition holds 1,993 non-empty 4K blocks out of 2,528, identical to the previous backup, so the desktop's own littlefs content is unchanged, and that figure also identifies an earlier attempt to count those blocks as wrong: a first pass read only one block because it sampled the partition with a shell loop against the wrong stride, and the python recount that respects the 4096-byte block size is the number quoted. ONE THING DID NOT GO TO PLAN AND HAS NOW HAPPENED TWICE. The script's trailing reset -- both the `--after hard_reset` on the verify and the `reset_board` trap -- did not bring the board back, and both of the board's paths were silent afterwards: no bytes on USB and no answer to ping on 10.10.0.15, which together look like a dead watch. An explicit `esptool ... run` and then a `flash_id` proved the chip was healthy and merely sitting in the stub, and the operator reported it running again after the board rebooted. Entry 15 recorded the same signature on the previous backup, so this is a property of this board's USB-Serial-JTAG stub or of the script's final reset rather than a one-off, and the trap hides it because `reset_board` ends in `|| true`. The correct conclusion is that the backup is sound -- the verify passed before either reset was attempted -- and that the script should confirm the board came back rather than assume it. THE WIFI CORRECTION came out of the same session and is the other half of this cycle. The row written in entry 19 said credentials "live in `/fs/etc` and survive a reboot", which is true and reads as "the watch returns online", and the operator corrected it: the watch boots Offline by design. The source agrees -- `s_wifi_autoconnect` is initialised false in the shell's network policy, and the taskbar said `Offline` after the reset while the same build had answered on 10.10.0.15 an hour earlier. So the row now separates the two facts a reader actually needs: saved credentials persist, and the link does not come up until `network autowifi on` is set, which is the difference between "the link is broken" and "the link was never asked for", and the first thing to check before suspecting the radio. At `abfbaac` the row also carries the command that turns it on, and a first version of that sentence named `network wifi auto on`, which is not a command -- reading `tdsh_network.c` showed it is `network autowifi`, and the invented form was corrected before the commit rather than after. Nothing was built or flashed this cycle: the firmware running on the watch is still `dbebb69`, the build the operator accepted, and the backup is a read of it. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 20, Coming From names the pre-cycle `HEAD`, Files Modified lists one port file and no `.ai/` path or generated artifact, Status uses allowed values, and the active log holds 20 entries against the 100-entry limit.

#### Next Steps:

`scripts/watch-backup.sh` should prove the board came back instead of assuming it, because this is the second consecutive backup that left it silent on both USB and WiFi with only an explicit reset recovering it, and the failure is invisible from the script's own status because `reset_board` ends in `|| true`. The cheap shape is to wait for USB output after the reset and report `board did not return on its own -- reset it` as a warning rather than silently exiting 0, which costs seconds and turns a confusing dead-watch moment into a message. Everything else is unchanged and unblocked. The next device with a real result available is Pusher B on GPIO 0, which is already proven readable from the earlier SPI probe and needs a handler rather than a driver; Pusher A follows because it reaches the SoC only through the AXP2101 PEK interrupt on GPIO 7, which this port does not install, and that same interrupt is what VBUS plug and removal would need for charge-state handling. Audio remains a beep with no playback path, charging is read but never configured, and the display's tearing-effect line is configured in `board.conf` and read by nothing. Two smaller items still belong to the touch work: the screen model that reads the Log Viewer over USB is in a temporary directory for the third time and belongs in the fork under a tools directory, and the boot-time OTA check writes four lines of `esp-tls` and `HTTP_CLIENT` noise on an offline watch at every boot, which is now worth quieting because the ring is worth reading. The standing queue is otherwise as it was: the bell seam is parked with its finding recorded, the top margin is the last unpainted region, the full-screen margin question waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched. The unbuilt outboard parts are unchanged: the BHI260AP at 0x28 answering with no driver, the LoRa radio needing ALDO3 and XL9555 P05 before it is powered, the NFC reader needing DLDO1 with its earlier absence verdict still suspect, and the GNSS and microphone with no code and, for the microphone, no evidence that a part is fitted.

#### Files Modified:

- ports/twatchultra/README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 19 COMMIT Unreleased 2026-11-12T08:20:05-07:00

#### Coming From:

Unreleased b3c4222

#### Purpose:

Correct three rows of the port's subsystem table that described `/fs`, WiFi and the PMU as unbuilt or barely built when all three are proven working.

#### Outcome:

At `d9a8bf9` on branch `t-watch-ultra`, `/fs`, WiFi and Battery / PMU in the subsystem table of `ports/twatchultra/README.md` now say what was measured rather than what was assumed. The cycle began as an audit of which of the board's peripherals remain unbuilt, and the audit found that one entry on the operator's own list was already finished: WiFi was reported untested because this repository contains no WiFi code, which is a true statement about the port and a false one about the board, since the radio is ESP-IDF's and needs nothing here. That was an inference from a code search presented as a finding, and the operator corrected it by giving the watch's address. Verified from the host: twelve pings to 10.10.0.15 all returned, min 5.152 ms, average 34.284, max 64.400, mdev 22.010, so the link carries traffic and is not merely associated; telnet on port 23 reaches `tinydesk - log in with your TinyDesk account` followed by `login:` and `Password:`; and port 22 completes an ECDSA P-256 key exchange against `SSH-2.0-wolfSSHv1.5.0` with the SFTP client landing on a server that identifies itself as `tdsh-idf SSH/SFTP`. Verified from the watch's own side rather than only from the host: `ssh status` reports `SSH/SFTP server: running`, port 22, a per-device host key, `Active clients: 0 / 1`, and `wolfSSL allocator: private heap in PSRAM` with a 65536-byte private heap. The taskbar changed from `Offline` to `Wi-Fi` with signal bars over the same session, which is the desktop's own reading of a link the host could already reach. What is not verified is a login, because that needs the account password and guessing at one is not a test; so SFTP file transfer and the telnet desktop handover are confirmed at the protocol and banner level and no further, and the row says so. `/fs` was checked the same way rather than inferred from the component being linked: `ls /` in the Terminal returns `etc home root sd shared tmp`, which is the littlefs partition as the shell's own root, and a file written to `/tmp` read back byte-exact and was then removed -- so the earlier note that it "should work" is now a measurement. Battery / PMU had claimed that no TinyDesk UI consumes the PMU and that the AXP2101 raises only ALDO2, and both were false: `power.c` raises ALDO1 for the card, ALDO2 for the display and BLDO2 for the speaker, and System Monitor draws the fuel gauge. That row now also records what the port does NOT do -- charging is never configured, because the only PMU writes are rail voltages and enable bits, so the part charges on its own defaults -- which is the distinction a reader needs and the old row had backwards. Two things were deliberately not changed. `NET-001`'s throughput figures of about 409 KB/s and 30 to 50 ms to the gateway were measured on the Escapement Linux build over esp-hosted, not on this port, and the 34 ms average seen here is a different stack that happens to land in the same range, so the record stands and the coincidence is noted rather than folded into it. And no code was touched: the three rows are documentation, nothing was built, deployed or flashed, and the firmware running on the watch is still `dbebb69`, which is also the build the operator accepted. The required `core-syntax.md` audit was run against the whole `.ai/` diff, which is this entry alone: `core.md` was untouched, the entry follows the six-section template and the canonical order, its number is the highest present plus one at 19, Coming From names the pre-cycle `HEAD`, Files Modified lists one port file and no `.ai/` path or generated artifact, Status uses allowed values only, and the active log holds 19 entries against the 100-entry limit.

#### Next Steps:

A fresh backup is now the outstanding action and the only one with real value, because four commits have landed since `working-2026-11-12-0109.bin` was written -- the SD note, the touch interrupt, its logging fix and these corrections -- and two of those changed the firmware image, so the backup no longer describes what runs. After it, the remaining unbuilt peripherals are the ones the audit listed and none is blocked: audio is a beep and nothing else, with no playback path, no mixer and no file source; charging is read but never configured; Pusher A is the power key and reaches the SoC only through the AXP2101 PEK interrupt on GPIO 7, which this port does not install, so the physical power button does nothing; Pusher B is GPIO 0, proven readable during the old SPI probe and read by no code here; and the display's tearing-effect line is configured in `board.conf` and read by nothing, so the painter free-runs. Of those, Pusher B is the cheapest real result because the pin is already proven and needs a handler rather than a driver, and Pusher A is the natural follow-on because the same PMU interrupt that gives the power key also gives VBUS plug and removal, which is what charge-state handling would need. Two smaller items belong to the touch work rather than to a new subject: the screen model that reads the Log Viewer over USB is in a temporary directory for the third time and belongs in the fork under a tools directory, and the boot-time OTA check writes four lines of `esp-tls` and `HTTP_CLIENT` errors on an offline watch that bury the ring at every boot and are worth quieting now that the ring is worth reading. The standing queue is otherwise unchanged: the bell seam stays parked with its finding recorded, the top margin is the last unpainted region, what the margins do under a full-screen application waits for something to go full screen, Store installs to the card and the image cache have never run against a card, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` remain untested against the current tree, the RTC write path has never been exercised by setting a time and power-cycling, and the pre-existing YAML fence defect in `core-reference.md` is untouched. The unbuilt outboard parts are as they were: the BHI260AP at 0x28 answering on the bus with no driver, the LoRa radio needing ALDO3 and XL9555 P05 before it is powered, the NFC reader needing DLDO1 with its earlier absence verdict still suspect, and the GNSS and microphone with no code and, for the microphone, no evidence in this repository that a part is fitted at all.

#### Files Modified:

- ports/twatchultra/README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

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
