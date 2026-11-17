## 56 COMMIT Unreleased 2026-11-17T23:30:00-07:00

#### Coming From:

Unreleased f6d7152

#### Purpose:

Follow the operator's own reasoning to its conclusion: "i dont think boshe ever made what i want but if they did, it would be a diag tool to test the ai engine in the imu. the engineers need some way to make sure its working properly and only synthetic, repeatable data can do that." They were right, and entries 51, 54 and 55 had all been searching in the wrong place.

#### Outcome:

BOSCH SHIPS DATA INJECTION FIRMWARE FOR THE BHI260AP AND IT BOOTS ON THIS BOARD. `firmware/bhi260ap/DataInject.fw`, 95,960 bytes, and `DataInject_nobsx.fw`, 94,460 bytes, from `boschsensortec/BHI2xy_SensorAPI`. They are not in SensorLib, which this port took its KLIO image from, and SensorLib is where every earlier search started -- so the part looked permanently incapable of injection on the strength of a third-party Arduino library's contents. THE MEASUREMENT, WITH ITS CONTROL: the BHI260AP KLIO image (123,696 B) gives `boot 38`, err `00`; the BHI260AP DataInject image (95,960 B) also gives `boot 38`, err `00` and boots in 2,473 ms; the BHI360 data-inject image (99,736 B) gives `boot 78` with err `14`, which is HOST_FW_VERIFY_ERROR. THE BHI360 ATTEMPT IS WHAT MAKES THE RESULT WORTH TRUSTING, because it exercises the same code path, the same container magic (0x662B) and the same silicon and comes out different -- the part received that image in full, ran verification and rejected it. The distinguishing fact is chip id: 0x70 is a BHI260, 0x7A is a BHI360, and this board reports 0x70. A NEW INSTRUMENT WAS NEEDED TO SEE ANY OF THIS: `imu boot [upload]` brings the rail, bus and SDK up and reports the part's own boot status WITHOUT uploading firmware, which is the only way to inspect a part whose firmware was refused -- every other command gates on a successful start, so the one case that needed diagnosis was the one case that could not be reached. `boot 38` is iface-ready + verify-done + no-flash; `boot 78` adds VERIFY-ERROR. THE INJECTION PROTOCOL WAS THEN FOUND IN BOSCH'S OWN EXAMPLE, `examples/swim/swim_data_injection`, AND ONE STEP EXPLAINS EVERY EARLIER FAILURE: THE VIRTUAL SENSOR MUST BE CONFIGURED FIRST, `bhy2_set_virt_sensor_cfg(SENSOR_ID, rate, latency)`, BEFORE ANYTHING IS INJECTED. Injection supplies data FOR a sensor; with no sensor configured nothing is listening and the packets are accepted and dropped, which is precisely what entry 55 measured from the outside with +7.32 g on Z and zero samples above 4 g. The rest of the sequence is: step-by-step mode, drain the status FIFO, inject a SIX-BYTE FULL TIMESTAMP as its own call, then inject the event, then drain the FIFO between injections. A 6-byte timestamp as a separate injection is the second thing every earlier attempt lacked and the reason the event would have had nowhere to sit in time. THE TRADE IS REAL AND MUST BE STATED: the DataInject image presents only TWO virtual sensors, ids 4 and 6 -- the accelerometer and its wake-up variant. No gyro, no orientation, no KLIO. The IMU window, the posture vocabulary and the KLIO experiments all stop working while it is loaded, and there is no image that combines KLIO learning with injection on this silicon. That is a choice per session, not a capability. WHAT IS NOT DONE: `imu feed <seconds> <hz> <amp>` implements Bosch's sequence and generates a sine with the host, injects it and round-trips the samples back through the part's parser -- but it WEDGES THE SHELL TASK on the drain, and `imu raw` on a FIFO channel desyncs the stream whenever the read is capped short of the length the header states. Both are instrument bugs and neither is evidence about the part. The board was returned to the KLIO image and verified whole: IMU boots to kernel 23.103, `music` decodes 352800 frames with 0 errors and 78 decode calls. BOTH THE FIRMWARE IMAGES AND THE PROTOCOL ARE NOW RECORDED IN `ports/twatchultra/components/imu/README.md`, including the 12-byte FUMI wrapper the port requires, the esptool commands in both directions, and the BHI360 negative -- so the next session does not have to rediscover any of it.

#### Next Steps:

FINISH THE FEEDER, AND IT IS SMALL: the wedge is in the drain, not in the injection. Bosch's own loop sets the async status bit, calls `bhy2_get_and_process_fifo` against a generously sized work buffer, then clears the bit; this port has a documented history of that exact call spinning for ever when the FIFO is larger than the buffer, so the buffer and the bound both need to be right rather than optimistic. The `imu raw` capping behaviour should be removed entirely: a partial FIFO read leaves the stream desynchronised for every later read, which is why the counts jumped from 251 to a nonsense 64,768. ONCE THE SINE ROUND-TRIPS, THE OPERATOR'S ACTUAL EXPERIMENT IS TWO COMMANDS: train on a reference sine, then measure `bhy2_klio_similarity_score()` against modulated variants, which already returns a float where "close to 1.0 means the same exercise and negative means very different" -- that is the 90%-sure figure and its decay, and `klio save <slot>` and `klio compare <ref> <slots>` are already built and waiting for it. NOTE THAT KLIO IS NOT IN THE DATAINJECT IMAGE, so the similarity experiment needs the KLIO image and the feeding needs DataInject: if both are required in one session, that is the point at which Bosch's toolchain becomes the real answer. THE FULL SEQUENCE TO TEST FIRST, in order: flash DataInject, `imu boot upload`, `imu cmd 0007 02000000` for step-by-step, configure the accelerometer with `bhy2_set_virt_sensor_cfg`, then timestamp-then-event injection with a full drain between. THE BOARD IS ON THE KLIO IMAGE AND FULLY WORKING as of this entry, which is where it should stay until the feeder is proven. UNCHANGED AND STILL OPEN: KLIO patterns do not survive a reboot, KLIO has no UI and none of its four knobs are tuned, the similarity commands are built but never run against a real pattern, the guest stacks are internal DRAM, `update.url` is unset, and `.ai/core.md`'s Purpose line still calls this a port of Linux.

#### Files Modified:

None in this repository; the fork carries the work at f28ad6a (ports/twatchultra/main/imu.c, components/imu/DataInject.fw, DataInject_nobsx.fw, README.md).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A (firmware identified and booted; the experiment it exists for is not finished)

---

## 55 COMMIT Unreleased 2026-11-17T21:30:00-07:00

#### Coming From:

Unreleased 6b6775b

#### Purpose:

Answer the open question entry 54 left -- where accepted injected data goes -- with the cheapest test that could settle it rather than the most elaborate: "do the cheap check."

#### Outcome:

INJECTED DATA DOES NOT REACH ANY HOST-VISIBLE PATH, AND THIS IS NOW A MEASUREMENT RATHER THAN AN ABSENCE. The test does not depend on framing a payload correctly, which is what every earlier attempt did: inject a value gravity cannot produce -- +7.32 g on Z, four times a hand's weight -- and watch the accelerometer's own virtual-sensor output for it while the physical sensor runs throughout. The success test is a RANGE over thousands of samples, so a single injected sample landing anywhere in the window is enough to declare victory. THREE FRAMINGS, ALL NEGATIVE, ALL THE SAME: with driver ids 76/79, 971 frames sent and 762 sensor samples during, z stayed 0.89..0.92 g with 0 samples above 4 g; with the float-plus-timestamp form, 728 sent and 610 samples, z 0.89..0.90; with inject ids 55/58, 777 sent and 611 samples, z 0.89..0.89. THE SENSOR KEEPS DELIVERING THROUGHOUT, which is a second finding: step-by-step injection does not suppress the physical sensors, it simply does nothing to them. Not one sample in nearly two thousand came within half of the injected value. THIS NEGATIVE IS STRONGER THAN THE KLIO ONES and the reason is structural. KLIO reports a SUMMARY -- progress, an index -- and a summary can stay at zero for a dozen reasons: the rate is wrong, the threshold is wrong, the segmentation does not see a repetition, the algorithm is not subscribed. The accelerometer reports the SAMPLE ITSELF. So a negative here is about the data path and nothing else, and it closes the question that entries 51 and 54 could only narrow. THE WAVEFORM BUG IN THE PREVIOUS EXPERIMENT, found by the operator rather than by me, is recorded because it was a real error: "what synthetic data are you feeding it? is it smooth like sin waves?" It was -- a pure sine, `theta = amp*sin(wt)`, perfectly periodic with no noise, no variance and no repetition boundary, which is a stimulus with the one structure a segmentation-based classifier needs surgically removed. It was replaced with a gesture: 60 percent swing and 40 percent deliberate rest per cycle, a 30 percent attack and 70 percent decay with raised-cosine ease on each, small jitter on amplitude and period, noise on every sample, and a cross-axis component. That still produced no learning, so the generator was not the whole story -- but the operator's question was the right question and the sine would have made any positive result an artifact. BOSCH'S OWN PROTOCOL WAS FOUND AND IS NOW IN THIS PORT, which matters for anything built later even though it did not change the outcome: BHy2CLI's `dinject.c` injects a stream of ORDINARY FIFO EVENTS concatenated into one buffer, one-byte event id then that event's data, using INJECT ids 55 (accel) and 58 (gyro) or DRIVER ids 76 (corrected accel) and 79 (corrected gyro) -- not the ordinary sensor ids 4 and 13 that every earlier attempt used. Bosch's tool switches with `dmode s`, injects, then `dmode n`, and reads the status once after switching with the comment "Fix for mode switching not occurring cleanly". All of that is now implemented here. WHICH LEAVES ONE EXPLANATION STANDING, and it is Bosch's own words: the help text for `dinject` says "Ensure that the requisite application specific Data Injection firmware is loaded and the sensor is configured prior to executing Data Injection." A DIFFERENT FIRMWARE IMAGE. The 123,696-byte KLIO image this port uploads is the gesture/learning variant; the injection-capable variant is a separate build from Bosch's toolchain, and swapping it would trade KLIO learning for injection rather than adding to it. That is a much larger piece of work than anything attempted in entries 51, 54 or 55 and it should not be started without the operator saying so explicitly. AN EARLIER 20 PERCENT PROGRESS READING IS RECORDED AS NOISE. One KLIO run reached 20 percent once, which is exactly one repetition of the five the engine wants and looked like a real signature. It did not reproduce in four subsequent attempts including controls on the same build, so it is called noise here rather than a lead. INSTRUMENT DEFECTS FIXED THIS CYCLE, both of which had been hiding results: the display mirror sends ONLY CHANGED CELLS and the console capture applies them to an empty grid, so a scrolling transcript came back interleaved and unreadable -- measured as `progress I20%APframesH626accel` -- and every run now writes its trace to `/root/klio.txt` with only the verdict on screen; and the per-frame KLIO byte trace was at INFO, printing 50 lines a second and scrolling the answer away, now behind `klio trace on`. `imu accel`'s blocking configuration call took the terminal down twice more this cycle and needed hard resets both times. THREE NEW INSTRUMENTS: `klio inject <shape> [seconds] [hz] [amp] [rate] [wave]`, which is the synthetic-motion run with the motion as arguments so a sweep costs a command rather than a reflash; `imu watch <seconds> <shape>`, the virtual-sensor check above; and `imu inject begin/frame/end` exported in imu.h so other code can feed the part without duplicating the framing. NOTHING REGRESSED: six app rebuilds and six reflashes this session, each followed by `make modaddr` and a module rebuild, and the entry-53 link guard plus `check_addr_fresh.py` refused nothing because the sequence was followed. `music` last verified at 352800 frames, 0 errors, 78 decode calls.

#### Next Steps:

THE OPERATOR'S WILD IDEA IS OUT OF REACH ON THIS FIRMWARE and they should be told plainly, with the evidence: injection is implemented, accepts a payload, and reaches nothing. Three experiments across two days and five framings agree. THE REMAINING ROUTE IS THE ONE BOSCH NAMES -- an application-specific Data Injection firmware image -- and its cost is: obtain the image (Bosch's Development Desktop or Application Board toolchain, a registered download), accept that it replaces the KLIO image rather than supplementing it, and re-derive whatever this port does with the KLIOfirmware. That is a decision for the operator, not a next step to take. IF THE GOAL IS ONLY TO CLASSIFY MOTION and not specifically to prove the part can, the ESP32-S3 ITSELF IS THE OBVIOUS ANSWER: 985 KB of app slot free since entry 43, hardware vector instructions, and a host-side pipeline would use the six channels this port already reads reliably at 97 Hz. That is a different demo and an honest one. KLIO ITSELF IS STILL A WORKING DEMONSTRATION and nothing here broke it: learn by hand, `klio detect`, recognise by hand, verified in entry 52. What it still lacks is unchanged -- persistence across reboot, a UI, and any tuning of its four knobs. THE BACKUP IS DUE: the last one is `imu-probe-2026-11-17-1401.bin` and six app reflashes have happened since. THE INSTRUMENT LIST IS NOW LONG ENOUGH TO NEED DOCUMENTING IN ONE PLACE in the handoff rather than accumulating command by command: `imu`, `imu accel`, `imu rate`, `imu reg [addr]`, `imu raw <reg> [len]`, `imu cmd <id> [payload]`, `imu status`, `imu param <id>`, `imu inject [1..6]`, `imu watch <s> <shape>`, `klio`, `klio learn`, `klio detect`, `klio stop`, `klio reset`, `klio trace`, `klio inject <shape> [s] [hz] [amp] [rate] [wave]`.

#### Files Modified:

None in this repository; the fork carries the work at 539facd (ports/twatchultra/main/imu.c, main/imu.h, main/klio.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A (a measurement; the operator read the result)

---

## 54 COMMIT Unreleased 2026-11-17T19:30:00-07:00

#### Coming From:

Unreleased e5adf45

#### Purpose:

Answer the operator's question -- "its 6 channels of data coming straight from the imu and we cant intercept or redirect that anywhere else unless we build our own custom code for the IMUs AI engine?" -- by testing it rather than reasoning about it: "do the experiment. i want to dig around in this chip a bit."

#### Outcome:

INJECTION IS IMPLEMENTED ON THIS FIRMWARE. ENTRY 51 WAS WRONG, AND THE HOLE IN ITS METHOD IS NOW NAMED. It concluded that host data injection is dead from six payload shapes that all returned 0 from `bhy2_set_data_injection_mode()` and `bhy2_inject_data()`. That return code means nothing: `bhy2_inject_data()` calls `bhy2_hif_exec_cmd()`, which calls `bhy2_hif_exec_cmd_generic()`, which writes the packet to `BHY2_REG_CHAN_CMD` and returns `BHY2_OK` as soon as the I2C write completes. IT NEVER WAITS FOR THE STATUS RESPONSE AND NEVER READS IT. So "accepted" has always meant "the bytes left the ESP32", and the verdict that the path is inert was reached without once asking the part what it thought of the payload. THE PART ANSWERS ON A SEPARATE CHANNEL, and `bhy2_defs.h` names the vocabulary: `BHY2_STATUS_INJECT_SENSOR_CONF_REQ` (0x04), which would mean "tell me which sensor this data is for", and `BHY2_STATUS_CMD_ERR` (0x0f), which would mean "rejected". Reading that channel is the experiment. THE SEQUENCE IS BOSCH'S OWN, taken from `bhy2_hif_get_parameter()` -- the only path in the SDK that reads a response: clear the async status bit BEFORE the command goes out, wait for `BHY2_IST_MASK_STATUS`, then pull the status FIFO. The order matters and it is the reason nothing had been seen before: with the async bit set, a response is delivered down the asynchronous channel and a synchronous read finds an empty FIFO. THE TIMEOUT ALSO HAD TO BE OURS. `bhy2_read_status()` waits 1000 x 10 ms = ten seconds for a status that may never arrive, inside the shell's own task, and a call that cannot be interrupted takes the terminal with it -- so the interrupt-status register is polled with a short budget first and `read_status()` is only called once it says something is waiting. THREE INSTRUMENTS WERE BUILT, EACH ABLE TO DISAGREE WITH THE OTHER TWO, and that is what makes the result worth having. `imu param 0900` is the positive control: rc 0, 4 bytes returned, so the reading path can produce data and silence is not the instrument failing. `imu cmd ffee` sends a command id that cannot exist and got `status 0x000f CMD ERR` with the payload `ee ff 05 00` -- reason 05, and the offending id echoed back. `imu cmd 0008` sends `BHY2_CMD_INJECT_DATA` bare and got `status 0x000f CMD ERR` with reason 01. TWO DIFFERENT REASONS FOR TWO DIFFERENT FAILURES, so reason 01 is not "unknown command". And then the decisive step: with step-by-step mode set first via `imu cmd 0007 02000000`, THE SAME INJECT_DATA COMMAND WITH A PAYLOAD IS ACCEPTED IN SILENCE. The command is in the firmware. It parses. It does not error. WHAT IS STILL TRUE, AND IT IS THE OPERATOR'S ACTUAL CONSTRAINT: accepted data does not reach the accelerometer. With the sensor stopped and the FIFO verified empty by a raw header read, a 7-byte frame carrying a one-byte sensor id (04, the corrected accelerometer) and z = +2.5 g left the FIFO empty and the reading unchanged. The old sweep used a TWO-byte sensor id where the BHy2 FIFO format uses one, which is a real defect in the old payloads, but it cannot be the whole explanation because the command reports success with the one-byte form too. So the honest statement is narrower than either earlier claim: injection is present, accepts data, and does not appear on the sensor data path. Whether the injected stream goes to the ALGORITHMS instead of the sensor outputs -- which would be exactly what the operator wants, since the whole idea is to feed a classifier -- is the open question and is now the obvious next probe. TWO DEFECTS FOUND AND FIXED, BOTH OF WHICH COULD HAVE BITTEN LATER. `bhy2_hif_set_inject_data_mode()`, the SDK's own helper behind `bhy2_set_data_injection_mode()`, reads the host interface control register, CLEARS the async status bit, sends the mode command and NEVER RESTORES IT. Every call, including the `BHY2_NORMAL_MODE` one that ends each attempt, leaves the part with asynchronous status reporting switched off for the rest of the session -- measured, reg 0x06 read 0x00 after a sweep where boot leaves it at 0x80. That is an SDK defect rather than this driver's, it is invisible until something depends on async status, and it is now restored explicitly. And `imu raw` ON A FIFO CHANNEL HUNG THE SHELL TASK AND COST A HARD RESET: the channels are not plain registers, Bosch reads a four-byte header first (a 16-bit word and a 16-bit count of bytes remaining) and only then a second read of exactly that length, so a blind 16-byte read against an empty FIFO leaves the transaction unfinished. Fixed to header-then-payload, after which an empty FIFO reports "nothing queued" and a full one dumps bytes. NOTHING REGRESSED: `music` still decodes the 44.1 kHz tone at exactly 352800 frames, 0 errors, 0 short writes and 78 decode calls after five app rebuilds and five reflashes in this session, which is also the fifth demonstration that the entry-53 link guard plus `make modaddr` keeps the module correct across image growth.

#### Next Steps:

THE OPEN QUESTION IS WHERE ACCEPTED INJECTED DATA GOES. The next probe is whether it reaches the ALGORITHMS rather than the sensor outputs -- run KLIO learning while injecting a synthetic motion stream and see whether the learning progress moves without anyone moving the watch. That is a small experiment with a large payoff, because it is precisely the operator's use case and it does not require a new firmware or Bosch's toolchain. If it works, the architecture is: host generates x,y,z, part classifies, ESP32 only moves bytes. If it does not, the remaining route to feeding the AI engine from a file really is our own code on the Fuser core, and the operator should hear that plainly before planning around it. THE PAYLOAD FORMAT IS STILL A GUESS. Three shapes are ruled out and none is confirmed; a shape that is accepted silently and a shape that is accepted and ignored are indistinguishable from the status channel alone, so the test has to measure the effect and not the acceptance. THE `imu accel` BLOCKING HAZARD IS UNCHANGED AND WAS HIT TWICE THIS CYCLE: the sensor-configuration call blocks inside the shell's own task and takes the terminal with it, needing a hard reset. It did not reproduce every time and no crash was captured. Until the configure path is made interruptible, treat `imu accel` as the risky command and prefer `imu rate`, which stops the sensor when it finishes. NEW COMMANDS FOR POKING: `imu reg`, `imu reg <addr>`, `imu raw <reg> [len]`, `imu cmd <id> [payload]`, `imu status`, `imu param <id>`, `imu inject [1..6]`. None of them is a feature; all of them are instruments that earn their place by having been wrong once. THE NEWEST BACKUP IS `imu-probe-2026-11-17-1401.bin`, sha256 `b37985bd95084e9aab08b54bddd299e12e4175d9c79af29be217b8b68317c585`, verify OK with all four code regions byte-identical to the build (app `824232e38c0e852e`, module text `941fa73df34f6bec`, module data `4eccdad2da2a42f3`, IMU firmware `7723591a1047dfe4`). It supersedes `klio-music-2026-11-17-1300.bin`, which is now five app rebuilds stale. UNCHANGED AND STILL OPEN: KLIO patterns do not survive a reboot, KLIO has no UI and none of its four knobs are tuned, the IMU window's heading still reads `--`, the guest stacks are internal DRAM, `update.url` is unset, and `.ai/core.md`'s Purpose line still calls this a port of Linux.

#### Files Modified:

None in this repository; the fork carries the work at e7571a5 (ports/twatchultra/main/imu.c, ports/twatchultra/modules/moddata.at).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A (an instrument, not a feature -- the operator asked to dig, not to ship)

---

## 53 COMMIT Unreleased 2026-11-17T13:55:00-07:00

#### Coming From:

Unreleased 473a988

#### Purpose:

Take a full backup of the working board and record it, as the operator asked: "do a full backup, then log commit and push." The backup was taken -- and checking it against the build found `music` broken on the board.

#### Outcome:

THE BACKUP IS GOOD AND THE BOARD WAS NOT. `klio-music-2026-11-17-1300.bin`, 16,777,216 bytes, sha256 `a4aeea4a1038d740d7bdc1fa4b79e9927ec7aef5ded75313bc8627fe60405176`, `verify OK (digest matched)` with the board held in the stub across read and verify; all four code regions compared byte-identical to the build afterwards -- app at 0x20000, module text at 0xC00000, module data at 0xE00000, IMU firmware at 0xE10000. It replaced `klio-2026-11-17-1247.bin`, taken forty minutes earlier and also verified. Both were taken before anything failed, so neither is a backup of a broken board: `music` is a module, and the app image it lives outside of never changed. THE BOARD WAS CARRYING A STALE MODULE LINK, and it was found by asking the board instead of assuming. `music` on a 44.1 kHz tone answered `state error, 0 frames` with the loader's own words: `module linked for inst 42ad0000 data 3ccd0000 host 3c26c0a4, live 42ae0000 3cce0000 3c27d4d0`. The app had been rebuilt five times for the IMU and KLIO work since entry 43 set the four-region table, and every one of those rebuilds moved the instruction base up 64 KiB while moddata.at still held entry 43's pair. THE RUNTIME GUARD DID EXACTLY WHAT IT WAS BUILT FOR and that is the part worth stating: the blob did not run, nothing read garbage, and the failure arrived as one line naming both pairs and telling the reader what to type. MOD-002's guard converted what used to be a silent `FLAC_DECODER_ERROR_BAD_MAGIC_NUMBER` on files that were fine into a diagnosis. WHAT ACTUALLY FAILED WAS THE REPAIR PATH, and it failed for a reason this project has recorded before in a different costume. `make modaddr` exists precisely to re-measure the pair, and `read_modaddr.py` still carried `STORAGE_OFF = 0x620000` and `STORAGE_LEN = 0x7E0000` -- the geometry from BEFORE entry 43. So the one tool for fixing a stale link read 8,064 KiB starting 1.5 MiB below where storage begins, on a board whose storage is 4 MiB at 0x800000. It found no `inst=`/`data=` pair where there was one and could not have found the right one anywhere. It now parses `partitions.csv` for the `storage` row rather than holding a copy of two of its fields, because the table is already the single source of truth for where a partition lives and a second copy of a field is a copy that drifts. THE DEEPER FAULT IS AN ORDERING ONE and it got a mechanism rather than a reminder: moddata.at is measured on the BOARD while the app is built on the HOST, and nothing connected the two, so an app rebuilt for its own reasons left the measurement behind and `make flac` linked against it without a word. moddata.at now records `app=<size>`, the size of the image its addresses were measured against, and the new `check_addr_fresh.py` refuses to link a module when that disagrees with the current `build/tinydesk.bin`. `flac` depends on it through a phony target, so the check runs on every module build and cannot be skipped by a target that was already up to date. The APP IMAGE SIZE is what is compared, not a timestamp, because the size is what is observed to move the bases -- a timestamp would also fire on a rebuild that changed only a comment, and a guard that cries wolf on clean builds is a guard that gets deleted. Three paths were exercised before it was trusted: a mismatched size refused with both numbers `(2985168)` vs `(3054112)`, a moddata.at with no `app=` line refused with instructions, and the true measurement passed. All three were run through `make flac` as well as directly, and the stale one produced `make: *** [Makefile:123: addrcheck] Error 1`. VERIFIED ON THE GLASS AFTER RELINK AND FLASH of both regions: the 44.1 kHz tone gave `state done, 352800 frames, 0 errors, 0 short writes`, `magic 464c4143` and 78 decode calls, which is the baseline recorded when this was first proven; the 48 kHz tone gave `state done, 192000 frames, 0 errors`, `rate 48000`, 43 decode calls; and the operator confirmed the 48 kHz tone by ear, "i heard the tone", which is the only check that proves the sink is fed rather than that the counter moved. THE RELINKED BLOB IS BYTE-IDENTICAL TO THE PREVIOUS GOOD ONE, sha256 `da4adcdb994655db`, and that is the correct result rather than a wasted build: only the addresses in the header changed, and reproducing the same text is evidence that nothing else did.

#### Next Steps:

THE GUARD COVERS THE CASE THAT BIT, NOT EVERY CASE. It compares app image size, which is what moved the bases on 2026-11-17; it does not compare `linked_host`, and that address moves on nearly every build regardless of size -- `0x3c26b8c8`, `0x3c26b99c`, `0x3c26ba78`, `0x3c26bb50` across four rebuilds in one afternoon, per modload.h. The loader still checks it at runtime, so the failure is still a diagnostic and not a decoder reading garbage, but a build that changed nothing but code inside an unchanged image size could still ship a stale `linked_host`. Reading the host symbol out of the freshly built ELF and comparing it is the same three lines and is the obvious next tightening. THE MODULE PATH HAS NO FLASH STEP IN THE BUILD: relinking and flashing both regions is a manual sequence in the handoff, and it was a manual sequence that left this stale for five rebuilds. A `make flash-modules` that writes 0xC00000 and 0xE00000 and verifies would remove the second half of the same hazard. THE BACKUP STORY IS NOW AMBIGUOUS BY ACCUMULATION: there are two images from this hour (`klio-2026-11-17-1247.bin` and `klio-music-2026-11-17-1300.bin`) whose only difference is the module regions, and neither name says so. `quad4m-2026-11-17-1030.bin`, `pre-quad-2026-11-17-1000.bin`, `nvs64-2026-11-17-0755.bin` and `pre-nvs64-2026-11-17-0740.bin` are four layouts deep and all superseded. Pruning to the current layout plus one rollback is owed, and every image that remains should say in one line what it holds. UNCHANGED AND STILL OPEN: KLIO patterns do not survive a reboot (the firmware is uploaded to RAM at every start), KLIO has no UI and none of its four knobs are tuned, host-side data injection is inert on this firmware (entry 51), the IMU window's heading still reads `--` because neither `DEVICE_ORI` nor `DEVICE_ORI_WU` has delivered a value, the guest stacks are internal DRAM, and `.ai/core.md`'s Purpose line still calls this project a port of Linux.

#### Files Modified:

None in this repository; the fork carries the work at 259513c (ports/twatchultra/modules/{Makefile,moddata.at,read_modaddr.py,check_addr_fresh.py}).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS (the operator heard the 48 kHz tone)

---

## 52 COMMIT Unreleased 2026-11-17T16:05:00-07:00

#### Coming From:

Unreleased 1b50d77

#### Purpose:

Get KLIO recognition working, which entry 50 left as the one open technical problem: learning worked and detection produced no frames at all.

#### Outcome:

IT WORKS, AND THE WHOLE LOOP IS PROVEN ON THE GLASS: train a gesture by hand, run `klio detect`, perform it again, and the engine answers. `RECOGNISED pattern 0, count 5.1` then `RECOGNISED pattern 0, count 5.5`, with 45 KLIO frames over one run. A classifier on the Fuser core now learns a motion from the operator's hand and recognises fuzzy repetitions of it, and the ESP32 does nothing but move bytes. THE MISSING STEP WAS WRITTEN IN THE HEADER I HAD ALREADY READ. `bhy2_klio.h`, on `read_pattern`: "After learning of a new pattern has been signaled by a sensor data event, CALL THIS FUNCTION TO RETRIEVE THE LEARNT PATTERN." Learning leaves the pattern INSIDE the algorithm; it never enters the host's recognition table, so `set_pattern_states(ENABLE)` was switching on an empty slot and the engine had nothing to compare against. That single omission explains every symptom entry 50 recorded -- zero frames with learning off, and frames that kept learning when it was on -- because in neither case was there a pattern in the table being searched. THE REAL LIFECYCLE IS FIVE STEPS and all five are now in order, each printing its own result so a future failure names itself: learn (the engine signals a new pattern); `read_pattern(0)` (157 bytes into host memory); `write_pattern(0)` (into the recognition table); `set_pattern_states(ENABLE, {0})` (a written pattern is disabled by default); then recognition on with learning OFF and BOTH reset fields 0 (the reset fields delete the table). THE PATTERN ID IS 0, NOT THE INDEX THE ENGINE REPORTS. The index announced while learning numbers the engine's internal slot; the header says "only 0 supported now" for the recognition table, which is why enabling index 4 -- the number the engine had just given -- could never have worked. SECOND FIX, AND IT WAS HIDING THE FIRST: THE FIFO OVERFLOW WAS DROPPING THE FRAMES. Setup is several blocking I2C calls -- read, write, enable -- during which nothing drains, while accel and gyro streamed at 100 Hz each, about 2.6 KB a second. That produced `meta 12 fifo overflow` in the middle of setup, and a KLIO frame dropped during recognition is INDISTINGUISHABLE FROM NO MATCH. Recognition now runs its inputs at 50 Hz rather than 100, sets a 32-byte watermark so interrupts come early, drains between each setup step, and polls every 5 ms instead of 10. The rate matters less than it sounds: KLIO works on motion envelopes, not sample-accurate waveforms. THE FRAME WAS DECODED AGAINST THE RAW BYTES rather than trusted from the struct layout: `d[5]` is the pattern index and `d[6..9]` is a little-endian float repetition count, confirmed on three consecutive frames reading 4.752, 4.858 and 4.962. That is the check that makes the count a measurement rather than a plausible number -- the previous cycle's own printed values (5.1, 5.5) came from later frames in the same stream, so the decode had to be verified independently of what the display showed.

#### Next Steps:

THE HONEST STATUS IS THAT THIS IS A WORKING DEMONSTRATION AND NOT YET A FEATURE. What it proves: the part learns a motion, stores a 157-byte fingerprint, and matches fuzzy repetitions of it, entirely on-chip. What it does not yet have: a way to keep a pattern across a reboot (the firmware is uploaded to RAM at every start, so the pattern table is rebuilt empty -- persisting it means reading the pattern out and storing it on littlefs, which the API supports via `read_pattern`/`write_pattern` and is the obvious next piece), any UI (it is a shell command), and any tuning of the four knobs that decide how forgiving the match is -- `RECOGNITION_SENSITIVITY`, `RECOGNITION_RESPONSIVNESS`, `LEARNING_SPEED` and `LEARNING_SIMILARITY_THRESHOLD`. THE OPERATOR'S ORIGINAL AMBITION REMAINS OUT OF REACH ON THIS FIRMWARE: feeding arbitrary IMU data in so the classifier can be trained from a file was tested across six payload shapes in entry 51 and the path is inert. So the demo is the version with the sensors as the input, which is the version that works. NOTHING REGRESSED: the app is 0x2e9a20 with 1,000 KB free, both Bluetooth pairings present, the four-region layout from entry 43 stands, `music` decodes through the module, and `imu`, `imu accel`, `imu rate`, `imu scan` and `klio` (status/learn/detect/stop/reset) all work. The posture vocabulary from entry 49 is unchanged and still pinned by its host test. Carried forward unchanged: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar needs `total_samples`, `modload` holds one blob, the guest stacks are internal DRAM, `.ai/core.md`'s Purpose line still calls this a port of Linux, and the newest backup `imu-window-2026-11-17-1315.bin` is now four changes stale.

#### Files Modified:

None in this repository; the fork carries the work at a8e64d4 (ports/twatchultra/main/klio.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS (the operator saw recognition fire on the glass)

---

## 51 COMMIT Unreleased 2026-11-17T15:20:00-07:00

#### Coming From:

Unreleased 4393aa4

#### Purpose:

Test whether the BHI260AP accepts arbitrary IMU data from the host, which entry 50 left open: the earlier 8-byte attempt was accepted and ineffective, and the leading hypothesis was that the payload had the wrong shape.

#### Outcome:

THE HYPOTHESIS IS DISPROVED, AND THE ANSWER IS CLEANER THAN "WRONG PAYLOAD". Six shapes were tried in one run, including the 28-byte float frame the algorithm itself consumes -- timestamp followed by accel[3] and gyro[3], straight from `bhy2_klio_log_frame_t` -- and every one of them was accepted by the protocol and did nothing. `8B id+i16`, `10B id+ts+i16`, `8B ACC_WU+i16`, `28B float frame`, `30B float frame behind a KLIO_LOG id`, and the float frame in REAL_TIME_INJECTION rather than STEP_BY_STEP. All read exactly `z +1.00` afterwards, against an injected `+2.50 g`, which is a value gravity cannot physically produce. THE ABSENCE OF ERRORS IS THE FINDING: not one `meta 11 (SENSOR ERROR)` appeared. A payload that was parsed and rejected raises that event, and a payload that was parsed and accepted moves the reading; neither happened for any shape, so the command is acknowledged at the protocol layer and never reaches the sensor pipeline at all. That makes this no longer a payload-shape question. Either this firmware build refuses host injection whatever the shape, or the mechanism needs setup the port does not perform -- and both are more expensive than the demo is worth today. THE MEASUREMENT WAS ALMOST INVALIDATED BY THE OPERATOR'S HANDS, and the recovery is worth recording because it is a lesson about instruments rather than about chips. The first run produced readings that WANDERED -- z between +0.65 and +1.08 across the six shapes -- and that looked like partial success, or at least like something happening. It was the operator rocking the watch during the test. A re-run with the watch perfectly still gave `+1.00` six times, identical, which is the clean result. The success criterion had been designed for exactly this hazard -- a specific impossible value, `+2.50`, rather than "did it change" -- and that is the only reason the noisy run could not be mistaken for a positive. A test whose pass condition is "the number moved" would have called that a success six times. SECOND-ORDER LESSON: the injection probe reported `accepted` from the API's return code for a shape that did nothing, which is precisely why the check measures the READING and not the return code. An API that returns success for an operation it ignored is common enough that the pattern is worth keeping: verify the effect, never the acknowledgement. WHAT THIS MEANS FOR THE OPERATOR'S IDEA, stated plainly because the idea is good and the answer is not the one hoped for. Treating the BHI260AP as a general-purpose accelerator is sound in hardware terms -- the Fuser core runs whatever algorithm the firmware blob carries -- but two things block the demo. First, KLIO is one specific algorithm compiled into that blob; running a different one needs Bosch's toolchain and a different image, not a parameter. Second, feeding it data from the host does not work on this build. So the demo as described -- stream 28-byte samples in, get fuzzy pattern matching out -- cannot be built on this firmware. What CAN be built is the same idea with the sensors as the input: the part genuinely learns a repeated motion and genuinely recognises fuzzy variants of it, which is the operator's concept with the input stage the part insists on.

#### Next Steps:

RECOGNITION IS STILL THE OPEN TECHNICAL PROBLEM and is now the only one worth pursuing on this part: `klio learn` works (verified four times), `klio detect` gives zero frames with learning off and keeps learning with it on. The next session should stop guessing at injection and finish that instead, because it is the difference between a working demo and a curiosity. THE DEMO THAT IS ACTUALLY AVAILABLE should be described honestly to the operator: a gesture trained by hand and recognised by hand, with the fuzzy matching and the on-chip classifier that make the idea interesting, and without host-supplied data. IF HOST INJECTION EVER MATTERS ENOUGH, the remaining avenues are a different firmware image that enables it, or Bosch's own sensor-fusion toolchain, and both are large. NOTHING REGRESSED: the app is 0x2e9740 with 1,000 KB free, both Bluetooth pairings present, the four-region layout from entry 43 stands, `music` decodes through the module, and `imu`, `imu accel`, `imu rate`, `imu scan` and `klio` all work. The posture vocabulary from entry 49 is unchanged and still pinned by its host test. THE SILENT-BOARD CAUTION FROM ENTRY 50 STANDS and was not seen again this cycle: the board needed no `esptool chip_id` reset during these runs. Carried forward unchanged: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar needs `total_samples`, `modload` holds one blob, the guest stacks are internal DRAM, `.ai/core.md`'s Purpose line still calls this a port of Linux, and the newest backup `imu-window-2026-11-17-1315.bin` is now three changes stale.

#### Files Modified:

None in this repository; the fork carries the work at 86eb891 (ports/twatchultra/main/imu.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS (injection tested and cleanly negative; the operator's rocking was identified as the cause of the ambiguous first run)

---

## 50 COMMIT Unreleased 2026-11-17T14:40:00-07:00

#### Coming From:

Unreleased 2490285

#### Purpose:

Investigate the BHI260AP's on-chip learning engine, which the operator suspected and was right about, and determine whether arbitrary IMU data can be fed to it.

#### Outcome:

KLIO IS A REAL ON-DEVICE LEARNING CLASSIFIER AND IT LEARNED A GESTURE ON THE GLASS. It is not pattern matching against a fixed list: the wearer repeats a motion, the engine reports progress 0 to 100 percent as it accumulates repetitions, and at 100 it announces a new pattern index. VERIFIED four times on the hardware, patterns 0 through 4, with the progress trace visible each run. `klio status` reports algorithm version 3.0.2 (Sep 26 2021), 25 pattern slots and a 157-byte pattern blob; `klio learn`, `klio detect` and `klio reset` drive it. THE FRAME FORMAT WAS DETERMINED EMPIRICALLY because the SDK does not document it: 11 bytes beginning {instance, learn_index, progress, change_reason} followed by the recognition block, with -1 meaning "nothing new learnt" and 0xff meaning "nothing recognised". That is one byte longer than the two packed structs in `bhy2_klio_defs.h` account for, which is why every byte is now logged rather than the first six. RECOGNITION DOES NOT WORK YET AND THIS ENTRY DOES NOT CLAIM IT DOES. With learning off and a pattern enabled the engine emits NO frames at all -- not frames saying "no match", but silence -- and with learning on it emits frames while continuing to learn, with the recognition index never leaving 0xff. Four bugs were found on the way and three were mine. First, KLIO is an ALGORITHM, not a sensor: enabling it without the accelerometer and gyroscope gave zero frames for a full minute of a human waving at it, because it had nothing to learn from. Second, a learnt pattern is DISABLED and must be enabled with `bhy2_klio_set_pattern_states()` before recognition will use it. Third, and this is the one that produced a sensor error, `recognition_reset = 1` DELETES THE PATTERN: the header says "Writing 1 to the reset fields resets all internal algorithm state and removes any patterns loaded for recognition", and the flag's name reads as an action when it means a wipe. Fourth, the probe itself lied once -- `learn_index` zero-initialises to 0 and the success test `>= 0` announced a learnt pattern in under a second with no gesture, which is the kind of confident false positive a diagnostic must never produce. ARBITRARY DATA INJECTION: THE COMMANDS EXIST AND ARE ACCEPTED, BUT DO NOT TAKE EFFECT. `bhy2_set_data_injection_mode()` and `bhy2_inject_data()` both return 0, and the SDK exposes three modes including real-time and step-by-step injection, so the protocol path is real. But there is NO payload struct anywhere in `bhy2_defs.h`, and injecting +2.50 g on z -- a value gravity cannot physically produce -- leaves the reading at the real sensor's +1.03 g. So the command is accepted and ignored. TWO HYPOTHESES ARE WORTH THE NEXT SESSION, in order: that the payload needs a timestamp field or a different header order than the datasheet prose suggests, and that this firmware variant needs the injection command issued per-sample in step-by-step mode rather than once. IF THAT IS SOLVED, the consequence is large: a classifier on the Fuser core could be trained from a FILE -- from recorded motion, or from synthesised motion that never happened -- rather than only from a hand. That is the piece of this the operator is interested in.

#### Next Steps:

RECOGNITION IS THE OPEN PROBLEM and the next session should start there, with the frame log already in place. The most likely explanations, in order: that recognition frames are only emitted when a pattern actually matches, so silence is a NEGATIVE result and the gesture needs to be more repeatable than the training was; that the pattern must be written back with `bhy2_klio_write_pattern()` to be usable rather than living only in the engine's volatile state; and that a boot wipes the pattern table, which it almost certainly does since the firmware is uploaded to RAM on every start -- so learn and detect MUST happen in the same boot, which the current probe does. THE OPERATOR'S WILD IDEA WAS NOT DISCLOSED, but the injection question was asked in preparation for it, and the answer so far is "the API says yes, the implementation says no". THE OPERATOR ALSO ASKED TO STOP ADDING FEATURES AND TEST FIRST, so this entry is an instrument and nothing else: `klio` is a diagnostic command, its only destructive operation is `learn` (which resets the pattern table deliberately), and nothing was built on top of it. A RESTART CAUTION: the board went silent twice after a flash in this cycle and needed an `esptool chip_id --after hard_reset` before it would talk. No crash was captured, it did not recur on the same build, and the trigger is unknown -- but a silent board after flashing is not necessarily a dead board, and the reset is one command. NOTHING REGRESSED: the app is 0x2e9570 with 1,000 KB free, both Bluetooth pairings present, the four-region layout from entry 43 stands, `music` decodes through the module, `imu`/`imu accel`/`imu rate` all work, and the posture vocabulary from entry 49 is unchanged and still pinned by its host test. Carried forward unchanged: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar needs `total_samples`, `modload` holds one blob, the guest stacks are internal DRAM, `.ai/core.md`'s Purpose line still calls this a port of Linux, and the newest backup `imu-window-2026-11-17-1315.bin` is now two changes stale.

#### Files Modified:

None in this repository; the fork carries the work at a6f0913 (ports/twatchultra/main/klio.c|h, imu.c|h, link_twatch.c, main/CMakeLists.txt).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PARTIAL (learning verified on the glass by the operator; recognition not working)

---

## 49 COMMIT Unreleased 2026-11-17T13:55:00-07:00

#### Coming From:

Unreleased 9f8285a

#### Purpose:

Rename the posture states to describe what the wearer does rather than which edge of the case is low, at the operator's request.

#### Outcome:

EDGE DOWN BECAME TILT UP, EDGE UP BECAME TILT DOWN, SIDE DOWN BECAME TILT LEFT, SIDE UP BECAME TILT RIGHT; FACE UP and FACE DOWN were left alone because they were already right. THE OLD NAMES WERE GEOMETRICALLY TRUE AND USELESS, which is the whole reason for the change: nobody turns their wrist and thinks about which edge of a rectangle is lowest. TILT UP is what the person did -- they tilted the watch up to look at it -- and that is the operator's own phrasing. The axis signs come from measurement rather than from a datasheet: tipping the watch forward towards the top strap 90 degrees reads x -1.00 and must read TILT UP, which is the same data point entry 47 recorded and now has a second job. THE NAMING RULE MOVED OUT OF THE WINDOW INTO ITS OWN FILE, and that is the structural part of this entry. It was a static function in `apps/imu.c`, which meant the only way to check it was to hold a watch at an angle and look; it is now `ports/twatchultra/main/imu_posture.c`, a file with NO ESP-IDF dependencies, for two reasons. It is a pure function of three floats that decides the only text a person reads, so it belongs in a host test that can walk it exhaustively -- and it did not have one, which is how the wrong names survived a release. And it is now SHARED: `imu_posture()` is what the shell's `imu accel` prints and what the window shows, so the two cannot drift apart on what the watch is doing, and the mapping can be checked over the console rather than through the Start menu. THAT LAST PROPERTY EARNED ITS KEEP IMMEDIATELY. The old names could only be verified by opening a window, and driving the Start menu over serial has now cost three sessions; the new vocabulary prints from the shell, so `imu accel` reading `posture FACE UP` on a watch lying flat is the whole check. A HOST TEST NOW PINS THE VOCABULARY: `tests/test_imu_posture.c`, 14 checks, covering the two flat states, the four tilts, the operator's own two measurements exactly as reported (flat `+0.01 -0.07 +1.00` giving FACE UP, tipped forward `-1.00 0.00 0.00` giving TILT UP), the threshold boundary at 0.70 either side, a diagonal watch reading TILTED rather than being forced into the nearest name, and the no-reading case. Its reason for existing is worth stating: a sign flip in this function would otherwise be noticed only by a person turning their wrist and being confused, which is precisely how the wrong names got shipped. THE API GATE FIRED AND WAS SATISFIED BY RECORDING rather than silenced -- `td_imu_t`, `td_imu_ops_t`, the `imu` field in `td_sysinfo_t` and `td_imu_register` in `td_apps.h` are all additions, four lines in the snapshot. The full host suite is now 44 tests and all pass. A build error cost one cycle and is worth recording as a caution about editing: removing the function by slicing from one marker to the next left a DUPLICATED `on_draw()` and a stale comment block, because the slice boundaries were guessed rather than read. The fix was to print the region and cut precisely.

#### Next Steps:

THE NEWEST BACKUP IS NOW STALE: `imu-window-2026-11-17-1315.bin` predates this rename, so the image on the board is newer than the newest backup. Take another before the next risky change; this one was a string table and a file move, so the risk was low and the operator did not ask for one. HEADING REMAINS THE ONE DASHED FIELD on the window and is the next real piece of work: both orientation sensors are subscribed and neither has delivered, so if a nudge of the watch produces nothing, `on_ori()`'s payload layout is what to question -- it assumes four int16 quaternion at 2^14 followed by three int16 Euler at 2^8, taken from the driver rather than from Bosch's documentation. THE GESTURE SENSORS ARE UNCHANGED IN REACH AND STILL THE PRIZE: 61 pickup-gesture, 67 wrist-tilt-gesture, 75 stationary and 77 motion all run on the Fuser core. ONE THING THE POSTURE WORK MAKES EASIER AND IS WORTH NOTICING: a wrist-raise-to-wake needs exactly the vocabulary this entry just put under test -- "is the watch being raised" is a posture question, so the naming is no longer decoration but the beginning of a feature. NOTHING REGRESSED: the app is 0x2e7e90 with 1,000 KB free, both Bluetooth pairings are present, the four-region layout from entry 43 stands, `music` still decodes through the module, and `imu`, `imu accel` and `imu rate` all work. Carried forward unchanged from entries 40 to 48: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, the guest stacks are still internal DRAM, the first IMU start blocks the desktop for about 3.3 s while the firmware uploads, and `.ai/core.md`'s Purpose line still calls this a port of Linux.

#### Files Modified:

None in this repository; the fork carries the change at f14626e (apps/imu.c, ports/twatchultra/main/imu_posture.c, imu.c, imu.h, main/CMakeLists.txt, tests/test_imu_posture.c, CMakeLists.txt, include/tinydesk/td_sysinfo.h, api/tinydesk.api).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PARTIAL (FACE UP confirmed over the console; the four tilt names are pinned by a host test but have not been seen on the glass since the rename)

---

## 48 COMMIT Unreleased 2026-11-17T13:30:00-07:00

#### Coming From:

Unreleased cfcb43c

#### Purpose:

Record the operator's verdict on the IMU window, verify the reopen fix that entry 47 left unproven, and take the backup for the current layout.

#### Outcome:

THE IMU WINDOW IS ACCEPTED BY THE OPERATOR: "face up, edge down states work perfect." That is the user test entry 47 could not supply, and it is the reading that matters -- the window now says in one word which way the watch is pointing, and the words are correct when the watch is turned. THE REOPEN FIX WAS VERIFIED AT THE COMMAND LEVEL, which is as far as this harness can reach. Three consecutive `imu accel` runs on one boot each produced live readings -- `x -0.01 y -0.07 z +1.03` flat, fifteen, then twenty-nine, then forty-two samples -- and each run stops the sensor on the way out, so every one of those runs began with a `start()` after a `stop()`. That is the same path the window's reopen takes, and the bug entry 47 fixed was purely that the window called it once and never again. The window-level test itself still needs a tap: close IMU, open it, and confirm it streams. A CORRECTION TO MY OWN WORK: entry 47's status line said the reopen fix was unexercised, and it still is at the window level, so this entry does not upgrade that claim -- it narrows what is untested from "start after stop" to "the window re-invoking start", which are not the same thing even though they share a code path. THE OPERATOR'S NAVIGATION RULE IS NOW THE FIRST THING TO TRY AND BELONGS IN THE HARNESS NOTES PROPERLY, because it has cost three sessions: WHEN IN DOUBT, REBOOT, F10, ENTER -- that always lands in the Terminal. It is exact, it needs no inspection of the screen, and it removes the two failures that kept appearing: type-ahead resetting after about two seconds so that `i` `i` searches twice from the top and lands on Image Viewer both times, and an arrow count overshooting into whichever entry happened to be there. The rule works because the Start menu opens with Terminal as the first entry already selected, so a bare Enter after F10 is enough. THE BACKUP IS TAKEN AND ITS THREE CODE REGIONS VERIFY. `imu-window-2026-11-17-1315.bin`, 16,777,216 bytes, sha256 `1325c673bb8dca1b`, read at 633.5 kbit/s with the board held in the stub and `verify OK (digest matched)`, board back up afterwards. The app at 0x20000, the module text at 0xC00000 and the module data at 0xE00000 each compare byte-identical to the build. A NOTE ON THE VERIFICATION ITSELF, because it nearly produced a false failure: the first comparison used a hardcoded app length of 3,060,160 bytes and reported MISMATCH, which is exactly what a real regression would look like. The build is 3,046,880 bytes. A hardcoded length in a verification command is a stale constant waiting to lie, and the check was redone reading the size from the file. That is worth keeping: verify by comparing against the artifact, not against a remembered number.

#### Next Steps:

VERIFY THE WINDOW REOPEN with a tap -- open IMU, close it, open it again, and confirm the readings return rather than the window sitting on "waiting for the first sample". HEADING REMAINS THE ONE DASHED FIELD and is the next real piece of work: both orientation sensors are subscribed, neither has delivered, and if a nudge of the watch does not produce a value then `on_ori()`'s payload layout is the thing to question -- it assumes four int16 quaternion at 2^14 followed by three int16 Euler at 2^8, taken from the driver rather than from Bosch's documentation. THE GESTURE SENSORS ARE UNCHANGED IN REACH AND STILL THE PRIZE: 61 pickup-gesture, 67 wrist-tilt-gesture, 75 stationary and 77 motion all run on the Fuser core, so a wrist-raise-to-wake or a shake costs the ESP32 nothing. NOTHING REGRESSED. The app is 0x2e7de0 with 1,000 KB free in its slot, both Bluetooth pairings are present, the four-region layout from entry 43 stands, `music` still decodes through the module, and `imu`, `imu accel` and `imu rate` all work. Carried forward unchanged from entries 40 to 47: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, the guest stacks are still internal DRAM, the first IMU start blocks the desktop for about 3.3 s while the firmware uploads, and `.ai/core.md`'s Purpose line still calls this a port of Linux.

#### Files Modified:

None. This entry records a verification, a backup and the navigation rule.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS (the operator confirms FACE UP and EDGE DOWN behave correctly)

---

## 47 COMMIT Unreleased 2026-11-17T13:00:00-07:00

#### Coming From:

Unreleased 3033f26

#### Purpose:

The operator rotated the watch and reported that the readings changed with rotation but not as they expected, and concluded the part was reporting pitch, roll and yaw.

#### Outcome:

THE OPERATOR'S CONCLUSION WAS THE OTHER WAY ROUND, AND THE CORRECTION IS THE SUBSTANCE OF THIS ENTRY. The window was showing ACCELERATION -- three numbers saying which way is down in the watch's own axes -- not orientation angles. Their evidence was exact and useful: flat, the window read `+0.01 -0.07 +1.00`; tipped forward 90 degrees toward the top strap, `-1.00 0.00 0.00`. That is gravity moving from z to x, a clean quarter turn about the axis that stayed at 0.00, which is y. So y runs left to right across the case, x runs strap to strap, and z comes out of the glass. THE THING THAT MAKES THE ORIGINAL NUMBERS FEEL WRONG IS PHYSICAL AND WORTH WRITING DOWN IN THE UI RATHER THAN A COMMENT: an accelerometer cannot see yawn. Rotation about the gravity axis leaves gravity unchanged, so a watch spun flat on a table reads identically however far it is turned, and no amount of shaking changes that. Only a magnetometer or a fusion algorithm can report heading. THE WINDOW NOW LEADS WITH THE ANSWER IN ONE WORD -- FACE UP, FACE DOWN, EDGE UP, EDGE DOWN, SIDE UP, SIDE DOWN, or TILTED -- chosen by whichever axis the gravity vector points along, with 0.70 (cosine 45 degrees) as the threshold so the name follows the dominant axis rather than merely the largest. TILT IN DEGREES IS DERIVED FROM GRAVITY and shown beside it: `pitch = atan2(-x, sqrt(y^2 + z^2))` and `roll = atan2(y, sqrt(x^2 + z^2))`. This is the rotation reading a person expects and it needs no magnetometer, so a board with only an accelerometer can still show it -- which is the honest answer to what the operator asked for. The arithmetic was checked against their own measurement rather than asserted: x -1.00 with y and z at zero gives exactly +90 degrees pitch and 0 roll. BOTH ORIENTATION SENSORS ARE NOW SUBSCRIBED. Sensor 69 (DEVICE_ORI) reports only ON CHANGE, so a watch lying still produced nothing and heading showed `--` for ever; 70 (DEVICE_ORI_WU) is the wakeup variant and reports continuously. Heading still reads `--` in this session because neither had delivered a value before the session ended, and the window does not call it a compass: without a magnetometer the fusion yaw is relative to power-on and drifts. A REAL BUG WAS FOUND AND FIXED BY TRYING TO REOPEN THE WINDOW. `start()` was called once behind a `static bool started` flag, but closing the window calls `stop()` by design -- the sensor is off when nothing reads it -- so the second open left the firmware running, the sensors off, and the window waiting for a sample that could never arrive. `start()` is idempotent and cheap when already streaming, so it is now called every tick, with a bounded retry so an absent part cannot re-attempt a three-second firmware upload repeatedly. THAT FIX IS NOT VERIFIED ON THE GLASS and this entry does not claim otherwise: reaching the Start-menu entry needs navigation that the session's harness kept getting wrong -- type-ahead resets after about two seconds, so `i` `i` searches twice from the top and lands on Image Viewer both times, and an arrow-count of eighteen overshot into Users. The reasoning is sound and the change is small, but it has not been seen working. A HARNESS NOTE WORTH KEEPING: the reliable way to open a specific app is the operator's finger, or the Terminal with a command; driving the Start menu from a serial capture is the least reliable part of this whole workflow and has now cost time in three separate sessions.

#### Next Steps:

VERIFY THE REOPEN, by tapping IMU, closing it, and tapping it again -- if it streams on the second open the fix is good, and if it hangs on "waiting for the first sample" the retry bound needs looking at. THEN HEADING: it is the one field still dashed. Sensor 70 is the one likely to deliver, and a nudge of the watch should produce a first value; if neither 69 nor 70 ever reports, the payload layout in `on_ori()` is the next thing to question rather than the subscription -- it assumes quaternion as four int16 scaled by 2^14 followed by three int16 Euler angles scaled by 2^8, which came from the driver's own reading of the protocol and has not been checked against Bosch's documentation. THE GESTURE SENSORS REMAIN THE PRIZE and are unchanged in reach: 61 pickup-gesture, 67 wrist-tilt-gesture, 75 stationary and 77 motion all run on the Fuser core, so a wrist-raise-to-wake costs the ESP32 nothing. NOTHING REGRESSED: the app is 0x2e7de0 with 1,000 KB free, both Bluetooth pairings are present, the layout from entry 43 stands, `music` still decodes through the module, and `imu`, `imu accel` and `imu rate` all still work. Carried forward unchanged from entries 40 to 46: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, the guest stacks are still internal DRAM, the first IMU start blocks the desktop for about 3.3 s while the firmware uploads, and `.ai/core.md`'s Purpose line still calls this a port of Linux.

#### Files Modified:

None in this repository; the fork carries the work at 60e6fbc (apps/imu.c, include/tinydesk/td_sysinfo.h, ports/twatchultra/main/imu.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PARTIAL (the window was seen showing FACE UP, tilt in degrees and live axes; the reopen fix has not been exercised)

---

## 46 COMMIT Unreleased 2026-11-17T12:30:00-07:00

#### Coming From:

Unreleased 63eea97

#### Purpose:

Settle the sample-rate question entry 45 left open, then give the IMU a desktop app the way GPS has one.

#### Outcome:

THE RATE IS HONEST AT ABOUT 98 Hz AND THE EARLIER 973 Hz WAS AN ARTIFACT OF THE INSTRUMENT, NOT OF THE PART. Two independent measures agree: 195 samples in 2,002 ms, and 13.8 bytes per sample across 307 drains. An accelerometer packet is six bytes of payload plus a sensor id and a timestamp, so thirteen bytes is exactly one packet -- one packet, one sample, one callback. Three consecutive runs after the fix read 97.4, 98.8 and 97.8 Hz against a configured 100. The 973 Hz figure came from a build whose sampling loop was consuming more packets than the part was producing, which is the sort of thing a counter shows and a byte count disproves; that is why both were measured. THE REAL DEFECT THAT CYCLE FOUND IS A HANG, AND IT IS NOW UNDERSTOOD RATHER THAN WORKED AROUND. `bhy2_get_and_process_fifo()` reads into `buffer_size - read_length`, so when the part holds more data than the work buffer that term reaches zero, the read returns nothing, `remain_length` never changes, and the SDK spins for ever with the shell task inside it -- Ctrl+C cannot reach a loop that is not checking for it. The work buffer was 512 bytes, which is thirty-nine samples, less than half a second at 100 Hz. TWO ATTEMPTS TO BE CLEVERER MADE IT WORSE, and both are recorded in the code so they are not repeated. Gating the drain on a FIFO-length read let a backlog accumulate until the next drain could not finish. Draining in bounded passes still handed the SDK a FIFO larger than the buffer, so the same zero-length read came around one layer down. WHAT ACTUALLY FIXES IT IS THREE THINGS: a 2 KB buffer, which is 1.5 s of headroom; one drain per poll, which is the shape that was verified; and -- the part that matters -- turning the sensor OFF when nothing is reading it. A part left streaming into an undrained FIFO builds thirteen bytes per sample, a minute of which is 78 KB, and the next command to touch it is the one that wedges. A command that can freeze the terminal is worse than no command, so the one-shot commands now stop the sensor on the way out and the window stops it on close. THE UI IS BUILT AND HAS BEEN SEEN WORKING ON THE GLASS: `td_imu_ops_t` in `td_sysinfo.h` shaped like `td_gps_ops_t`, where start and stop are a pair and `latest` is the polling read; and `apps/imu.c`, a window that opens from the Start menu immediately after GPS and reads `BHI260AP`, `fw kernel 23.103`, `+0.01 -0.06 +1.00` and a climbing sample count -- a watch lying flat, with gravity on z and the other two axes near zero. The sample count is shown on purpose: a reading that has stopped updating looks exactly like a reading from a watch that is perfectly still, and the counter is the only thing that separates the two. THE PORT IS REACHED THROUGH A WEAK `board_imu_ops()`, the same pattern as `board_apps_register()`, because a `#define` in the port's own CMakeLists cannot reach `ports/esp_idf/app/main.c` and one build was lost to trying. TWO MORE BUGS THE WINDOW FOUND, both presenting as "nothing happens". `start()` booted the part but never enabled the sensors, so the window sat on "waiting for the first sample" with the firmware running and nothing being produced. And `latest()` did not drain the FIFO, so 50 Hz of readings piled up unread behind a live sensor; the contract says the port does the work, so the drain belongs in `latest()`. ORIENTATION IS WIRED BUT NOT YET PRODUCING. `BHY2_SENSOR_ID_DEVICE_ORI` (69) is in the firmware's sensor list and the driver subscribes to it and parses quaternion plus Euler, but heading, pitch and roll still read `--` on the glass while acceleration works. That is the one visible gap in the window.

#### Next Steps:

ORIENTATION IS THE NEXT REAL PIECE OF WORK and it is narrow: sensor 69 reports on change, so a watch sitting still may legitimately produce nothing -- the first thing to try is the wakeup variant (70, `DEVICE_ORI_WU`) or a motion nudge, before assuming the parser is wrong. The payload layout assumed here is quaternion as four int16 scaled by 2^14 followed by three int16 Euler angles scaled by 2^8, which is worth checking against the part's documentation rather than trusting the arithmetic in `on_ori()`. THE GESTURE SENSORS ARE THE PRIZE AND ARE NOW WITHIN REACH: 61 pickup-gesture, 67 wrist-tilt-gesture, 75 stationary, 77 motion are all carried by this firmware and all run on the Fuser core, so a wrist-raise-to-wake or a shake-to-do-something costs the ESP32 nothing. THE OPERATIONAL RULES THIS CYCLE PRODUCED are worth keeping beside the module ones, because they are the same shape: never let a sensor stream into a FIFO nobody drains; a command that can block the shell task is a bug regardless of whether the data is correct; and a counter alone cannot tell "still" from "dead" without a byte count next to it. NOTHING REGRESSED: the app is 0x2e7c20 with 1,000 KB free, both Bluetooth pairings are present, the layout from entry 43 stands, `music` still decodes through the module, and `imu accel` and `imu rate` both still work. Carried forward unchanged from entries 40 to 45: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, the guest stacks are still internal DRAM, and `.ai/core.md`'s Purpose line still calls this a port of Linux. THE FIRST START OF THE IMU BLOCKS THE DESKTOP FOR ABOUT 3.3 SECONDS while the firmware uploads, because the window calls `start()` from its tick. It happens once per boot and was accepted rather than solved; moving it to a task is the fix if it ever becomes annoying.

#### Files Modified:

None in this repository; the fork carries the work at 83d4a58 (apps/imu.c, apps/apps.c, apps/td_apps.h, include/tinydesk/td_sysinfo.h, ports/esp_idf/app/main.c, ports/esp_idf/components/tinydesk/CMakeLists.txt, ports/twatchultra/main/imu.c|h, main/CMakeLists.txt, CMakeLists.txt).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS (window opens and shows live acceleration on the glass; orientation not yet producing)

---

## 45 COMMIT Unreleased 2026-11-17T11:40:00-07:00

#### Coming From:

Unreleased 03ebad7

#### Purpose:

Fix the blocking sensor-configuration call that entry 44 left open, and get the BHI260AP actually reporting acceleration.

#### Outcome:

ACCELERATION READS, AND THE VALUES ARE RIGHT. `imu accel` prints five live readings of `x +0.01  y -0.07  z +1.00` in g -- a watch lying flat, which is the one reading gravity makes checkable without moving anything. The command returns to the prompt. FOUR BUGS STOOD BETWEEN THE BOOT THAT WORKED AND A SAMPLE, and each of them presented as "nothing happens", which is why they cost as much as they did. FIRST, `bhy2_update_virtual_sensor_list()` was never called. That function fills `dev->event_size[]`, which is how the FIFO parser knows how long a packet for a given sensor is; `bhy2_get_virt_sensor_list()`, which was called, fills only the presence bitmap. Without the sizes the parser cannot frame a packet, so it dispatches nothing: the sensor is configured, the part streams, the drain returns OK, and zero samples reach the callbacks. SECOND, the availability check lied in a way that sent a whole cycle down the wrong path. `bhy2_is_sensor_available()` reads a bitmap that only the list call populates, so asking before that reports "no sensors at all" -- and the log then said, in a confident engineering voice, that the KLIO firmware had no accelerometer. It has 35 virtual sensors and id 4 is among them. The lesson is the one this project keeps relearning: an absence in one instrument is not evidence about a mechanism, and a "no" from an API is only as good as what populated the thing it reads. THIRD, a FIFO gate that returned early unless the interrupt status showed the WAKEUP bits did two kinds of harm at once -- accel data arrives on the NON-wakeup FIFO (0x18), and the part raises interrupts on a watermark, so a gate trusting a stale status bit let the FIFO fill until it overflowed. It was written to guard against a stall that turned out to be a different bug (the missing async-status-channel control, fixed in entry 44's follow-up), and one bug does not justify a filter that discards data: `bhy2_get_and_process_fifo()` already reads the status itself. FOURTH, the burst loop in the command never refreshed the sample counter it was waiting on, so it always timed out and printed a single line -- while `imu rate`, measuring the same sensor, counted 195 samples in 2,000 ms. A stale loop variable is not evidence about the device. THE REPORTING CADENCE ALSO NEEDED CONFIGURING rather than accepting: with the defaults the part delivers about 26 samples once a second, which is useless for a window redrawing at 4 Hz and leaves the FIFO on the edge of overflowing, so the driver now sets a 20 ms report latency and a 32-byte wakeup watermark. THE SAMPLE RATE IS NOT YET TRUSTWORTHY AND THIS ENTRY WILL NOT CLAIM IT IS. With latency 0 and default watermark the measurement was 97.5 Hz; after the latency and watermark change it measures 973 Hz, three times in a row, against a configured 100 Hz, with a `meta 12 fifo overflow` warning and five consecutive readings identical to two decimal places. Those three facts together suggest the drain is re-reading data it has already delivered rather than the part sampling ten times faster, but that is a hypothesis and not a measurement. What IS established is that real samples arrive with the correct value, which is enough to build on and not enough to size a buffer with. `imu` now takes `accel`, `rate`, `scan` and `stop`, dispatched by `strcmp` rather than by first letter -- "stop" and "scan" share a first letter, and a first-letter dispatch had silently sent `imu scan` to the status branch for one build. `imu scan` prints all 35 sensor ids the loaded KLIO firmware carries, which is worth keeping: 61 pickup-gesture, 63 ar, 67 wrist-tilt-gesture, 69 device-orientation, 70 device-orientation-wakeup, 75 stationary-detect, 77 motion-detect, alongside the expected accelerometer and gyroscope families. That is the wearable firmware, and the gesture and orientation sensors are the interesting half -- they are exactly the on-chip work this board cannot afford to do in its 26 KB of internal RAM.

#### Next Steps:

THE RATE NEEDS PINNING BEFORE ANY WINDOW DEPENDS ON IT, and the way to do that is to look at what the drain actually consumes rather than at the counter: log `bytes_read` and `bytes_remain` from `bhy2_hif_get_fifo` for a few polls, and compare the byte count against the packet size the SDK expects. If the byte counts match the sample count, the part really is oversampling and the configured rate is not being honoured; if they do not, the drain is re-reading. The fifo-overflow warning is the second signal and should disappear once the first is understood. THE UI IS THEN SHORT AND WAS ALREADY SPECIFIED: `td_imu_ops_t` in `td_sysinfo.h` shaped like `td_bt_ops_t` and `td_gps_ops_t`, with a poll-latest accessor rather than a callback, because a 4 Hz window wants the newest value and does not care about the ones it missed; then the GPS-style window. THE GESTURE AND ORIENTATION SENSORS ARE THE REAL PRIZE and should be tried as soon as the plumbing is trusted -- `BHY2_SENSOR_ID_DEVICE_ORI` (69) gives a heading and pitch directly, which is more useful on a wrist than three acceleration numbers and costs the ESP32 nothing. NOTHING REGRESSED: the app is 0x2e7650 with 1,003 KB free, both Bluetooth pairings are present, the layout from entry 43 stands, and `music` still decodes through the module. Carried forward unchanged from entries 40 to 44: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, the guest stacks are still internal DRAM, and `.ai/core.md`'s Purpose line still calls this a port of Linux. The board is left with the IMU firmware flashed, `imu accel` and `imu rate` both working, and the accelerometer configured.

#### Files Modified:

None in this repository; the fork carries the work at 20fc05a (ports/twatchultra/main/imu.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS (acceleration reads with correct gravity; the rate figure is flagged as untrustworthy rather than accepted)

---

## 44 COMMIT Unreleased 2026-11-17T10:10:00-07:00

#### Coming From:

Unreleased 1c7d664

#### Purpose:

Bring up the watch's IMU, which turned out to be a Bosch BHI260AP and had never been powered, and get as far as a firmware upload and a boot.

#### Outcome:

THE PART WAS INVISIBLE FOR ONE REASON AND IT WAS THE POWER RAIL. The BHI260AP sits at I2C 0x28 with its interrupt on GPIO 8, and AXP2101 ALDO4 feeds it at 1.8 V -- and nothing in this port had ever enabled ALDO4, whose enable is bit 3 of register 0x90 and whose voltage is register 0x95 at 500 mV plus 100 mV steps. Before the change a bus scan found only 0x1a 0x20 0x34 0x51 0x5a; enabling ALDO4 from MicroPython at runtime made 0x28 appear at once, and chip id register 0x2B then read 0x70, which is BHI260_CHIP_ID. After the change, with no runtime poking, a plain reboot scans six devices. AN UNPOWERED I2C DEVICE DOES NOT NAK -- it is simply absent, so the symptom reads as "this board has no IMU" rather than "the rail is down", which is the same shape as the amplifier on BLDO2 and the card on ALDO1. The voltage is 1.8 V and not 3.3, taken from the vendor's own rail table in LilyGoLib; the schematic was checked to confirm the part is fitted (U7). THIS IS NOT AN ACCELEROMETER WITH A REGISTER MAP. It is a Fuser core running Bosch algorithms -- orientation, step counter, gestures, wake-on-motion -- reported through a FIFO over the BHY2 protocol. Getting a sample means uploading ~121 KB of Bosch firmware into its RAM and booting it. Bosch's BHY2 SDK is vendored into components/bhy2 from SensorLib (BSD-3-Clause, licence recorded beside it, one SensorLib-only include deleted); the linker strips the parts nothing calls, so the whole SDK costs the app 5.6 KB rather than the 5,656 source lines it looks like. THE FIRMWARE IS LAZY AND LIVES IN `moddata`, NOT THE APP IMAGE. 121 KB is 12 percent of the app slot that entry 43 had just freed to 1005 KB, and an upload costs seconds at start-up. So the image is a blob at moddata+0x10000, streamed with esp_partition_read() on first use, 64 KB clear of the codec's .rodata at offset 0 so `make flac` and `make imu` may be flashed in either order. It is reproducible: components/imu/fetch.sh regenerates it from SensorLib. VERIFIED ON THE WATCH: the rail is raised (`PMU aldo4_v=0d ldo_en=bf`), the chip id is checked before anything is uploaded, the blob is found and its magic validated, 123,696 bytes upload and boot in 3.26 s, and the kernel version reads back as 23.103 -- a real Bosch firmware version, which is the proof that the part is executing our image rather than sitting in boot ROM. `imu` returns to the prompt. WHAT DOES NOT WORK YET, AND IT IS THE SAMPLE PATH: `imu accel` blocks inside bhy2_set_virt_sensor_cfg and takes the shell task with it, so Ctrl+C cannot reach it. Rather than leave a command that hangs the terminal, the default `imu` was made to report status only and the sample path is opt-in by name. THREE MISTAKES COST THIS CYCLE AND EACH IS NOW A COMMENT WHERE IT BIT. First, the HIF write buffer was 136 bytes and the uploader hands over 4,096-byte writes; the SDK turns any non-zero return into BHY2_E_IO (-3), so "your buffer is too small" presented as a bus fault and the upload "failed at 4096" after succeeding at 0. Second, a hand-rolled loop over bhy2_upload_firmware_to_ram_partly() uploaded all 123,696 bytes and then booted with HOST_FW_VERIFY_ERROR set -- boot status 0x78 -- because the chunk offsets it passed were not what the protocol wanted; the SDK's whole-image call sequences it correctly and the image is resident in PSRAM for the duration, which is 121 KB of a 7.7 MB pool and freed immediately. Third, bus_delay_us floored every sub-millisecond delay to a whole FreeRTOS tick, which at 1 kHz turned the SDK's microsecond handshakes into something very slow. A FOURTH DEAD END WAS TESTED AND REVERTED rather than left in: read_write_len = 0 makes the upload fail with BHY2_E_INVALID_PARAM (-2), because the uploader chunks the image by that value and 0 means no chunking. 64 is load-bearing in both directions and the comment says so.

#### Next Steps:

THE ONE OPEN BUG IS NARROW AND NAMED: bhy2_set_virt_sensor_cfg blocks. The next session should instrument that single call -- a log line before and after, and an I2C timeout audit -- rather than re-deriving the bring-up, which is now solid. Two hypotheses are worth testing in order: that the parameter-write path expects a different transfer length than the 64 that uploads the firmware, and that the part needs the HIF control set (`bhy2_set_host_intf_ctrl(BHY2_HIF_CTRL_ASYNC_STATUS_CHANNEL)`) before any virtual sensor is configured -- the vendor sets it immediately after boot and before cfg, and this driver does not. ONCE SAMPLES FLOW, the rest is short and was already planned: `td_imu_ops_t` in `td_sysinfo.h` shaped like `td_bt_ops_t` and `td_gps_ops_t`, then the GPS-style window app showing orientation and tilt live. NOTE THAT THE INTERESTING PART IS THE ON-CHIP ALGORITHMS and not the raw axes: the KLIO firmware carries orientation and step counting, which is work the ESP32 would otherwise do in the 26 KB of internal RAM this board is short of -- worth reaching for rather than settling for an accelerometer readout. THE IMU COMMAND IS NOT IN THE HEARTBEAT, so nothing regressed: entry 43's layout stands, both Bluetooth pairings are still present, and the app is 0x2e6800 with 1,004 KB free in the slot. Everything carried from entries 40 to 43 is unchanged: `update.url` is unset, `third_party/tdsh` is pinned, the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, the guest stacks are still internal DRAM, and `.ai/core.md`'s Purpose line still calls this a port of Linux. The board is left with the firmware flashed at 0xE10000 and a working `imu` status command.

#### Files Modified:

None in this repository; the fork carries the work at d81ed1b (ports/twatchultra/main/imu.c|h, power.c|h, components/bhy2/**, components/imu/**, modules/Makefile, main/CMakeLists.txt, link_twatch.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PARTIAL (part identified, powered, uploaded and booted on the hardware; acceleration not yet read)

---

## 43 COMMIT Unreleased 2026-11-17T10:45:00-07:00

#### Coming From:

Unreleased a7c2eba

#### Purpose:

Split the watch's 16 MB into four equal 4 MiB regions -- two app slots, a 4 MB filesystem and 4 MB of executable codec space -- because the 46 KB of app headroom left by entry 41 was the difference between "an emulator does not fit" and "two would".

#### Outcome:

IT IS DONE AND THE BOARD RUNS. Four regions on 4 MiB boundaries: region 1 (0x000000) holds the bootloader, partition table, 64 KB NVS, phy_init and ota_0; region 2 (0x400000) holds ota_1 and puts coredump and otadata in its tail; region 3 (0x800000) is the 4 MiB littlefs; region 4 (0xC00000) is `modules` 2 MB plus `moddata` 2 MB. APP HEADROOM WENT FROM 46,016 BYTES TO 1,029,056 -- 25 percent of the slot -- and the module partitions doubled against a 26,725-byte decoder. The final table was read back off the board's own flash at 0x8000 rather than taken from the CSV. THE ONE PLACE AN EVEN SPLIT CANNOT BE LITERAL, and it is worth being exact about because it was the whole design question: the bootloader, partition table, NVS and phy_init have to sit inside region 1, so region 1 has 176 KB less usable space than the other three, and an update writes whichever slot is not running, so both slots must be the same size. That size is the smaller one: 0x3E0000 each. Four 4 MiB slots would need 176 KB more flash than the chip has; the arithmetic was checked before anything was written, and the first proposal did overflow. The 56 KB left in region 2 after coredump and otadata is simply unused. THE HISTORICAL NOTE THE OPERATOR REMEMBERED IS SLIGHTLY OFF IN A WAY THAT MATTERS FOR READING OLD ENTRIES: the slots were 3 MB (0x300000) from the first commit and were never reduced below that. `ota_1` was trimmed to 3008 KB when the 64 KB coredump partition was carved out of its tail, and entry 41 equalised the two at 3008 KB rather than shrinking anything. So this change takes 3008 KB to 3968 KB, not 3 MB to 4 MB. THE RISKY HALF WAS `storage`, AND IT WAS DONE THE ONLY SAFE WAY. The live partition held 11 directories, 29 files and 23,047 bytes -- the two .py launchers, both .tdlink shortcuts, `/etc/board.conf`, the shell's history and settings, the account's Desktop, and a scattering of test files. The migration read every file out of the old image, formatted a fresh 4 MiB littlefs, wrote the same tree back, then MOUNTED the result and compared a SHA-256 of all 29 files: 0 missing, 0 extra, 0 digest mismatches. It was never truncated. That distinction is the whole of entry 25, where a byte census said the shrinking partition held 1.1 MB of real data and mounting it showed 22 KB -- the lesson being that a byte census is not a filesystem census, and the safe operation is extract-and-rebuild. NVS DID NOT MOVE, so the account and both Bluetooth pairings survived with no re-pairing: `bt` still lists 2 pairings with the K06 keyboard and the Xbox controller in their slots. THE ACCEPTANCE TEST WAS THE USER-VISIBLE ONE RATHER THAN A COUNTER: Pokemon Pinball was launched FROM THE START MENU, which walks the whole chain that the layout could have broken -- the .tdlink shortcut, the Python launcher, `tinydesk.shell`, and the emulator task -- and ran at 71.8 fps, 3487 frames, missed=0, for 48.5 s before being stopped from the shell. `music` decoded the 440 Hz tone at 211968 frames with 0 errors. TWO REAL BUGS WERE FOUND BY THE CHANGE, both in the module address mechanism and both previously unreachable. FIRST, `modload_prepare()` published `/root/modaddr.txt` AFTER the stale-link check, so a stale blob returned early and never wrote the very file the build reads to make a fresh one -- the guard made itself permanent, `make modaddr` cheerfully re-read the old pair, and the build regenerated the same wrong blob. That is a deadlock that could only appear the first time the data window moved, and this change moved it (0x3cbd0000 to 0x3ccd0000). The publish now happens as soon as both mappings exist, before anything that can refuse, so even a blob so broken the header check rejects it still publishes the addresses that would fix it. SECOND, `moddata.at` was not a prerequisite of `flac.bin`: the addresses are read at Makefile-parse time, so a changed file only relinks if make knows the blob depends on it. Without that, `make modaddr && make flac` updated the file, printed "gen_addr: unchanged", and silently shipped a blob linked for the old address -- the same class of stale-address failure the generated header was built to prevent, one layer down. Both bugs were caught by the loader's own guard doing exactly what it was designed to do: it refused with the linked-versus-live pair in the message instead of decoding garbage, which turned a two-hour mystery into one line. A SMALL HONESTY ITEM NOT WORTH CHASING NOW: after `music stop` tears the module down, a subsequent `music` status prints a garbage number for `in-calls` (2779096533, i.e. 0xa5a5a5a5-ish) because the handle is being read after it was freed. It is cosmetic, it does not affect playback, and it predates this change.

#### Next Steps:

THE SPACE QUESTION IS ANSWERED AND THE NEXT ONE IS UNBLOCKED: with 1005 KB free, a Game Boy-class emulator (~37 KB) fits four times over and even a NES-class core (~57 KB) fits, where before neither did. The larger prize is still the 998 KB of `modules` space now doubled to 4 MB, which needs the module ABI extension -- a blob can be called but cannot call back for input or audio, and its writable state has nowhere to live because `moddata` is flash -- so that remains real work rather than a build flag. THE OPERATIONAL RULE THIS CHANGE CREATES, and it is sharper than entry 41's: **the app is still at 0x20000 but the modules are now at 0xC00000 and 0xE00000**, and a hand-written esptool line using 0xE00000 for the module text would write codec bytes into the rodata partition. `make flash` and `idf.py flash` read the build's own args and are safe. ALSO: `coredump` moved from 0x600000 to 0x7E0000, so a panic trace written before this table is at an address the new one does not read -- if a crash is being chased across this change, read the old offset from the pre-change backup. Everything entry 42 listed about RAM stands and is unaffected: Bluetooth is still 47 KB, the 32 KB reserve is still held, and the largest free internal block is still 9,728 -- which is the constraint that decides whether a new emulator can START, and it has not changed. The guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM and still compete for it. `update.url` is still unset, `third_party/tdsh` is still pinned behind its remote, the launchers still have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, and `.ai/core.md`'s Purpose line still calls this a port of Linux. The newest backup before this change, `pre-quad-2026-11-17-1000.bin`, is the only image in the previous layout and therefore the only clean rollback.

#### Files Modified:

None in this repository; the fork carries the change at 93abe97 (ports/twatchultra/partitions.csv, README.md, main/modload.c, modules/Makefile, modules/moddata.at).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 42 COMMIT Unreleased 2026-11-17T09:20:00-07:00

#### Coming From:

Unreleased ff2af3b

#### Purpose:

Find what is consuming internal SRAM on the watch, because the board runs with a few tens of KB free and nothing on it said where the rest went.

#### Outcome:

THE ANSWER, MEASURED RATHER THAN ASSERTED. Internal heap is 263,651 bytes in total, of which 236,928 is allocated and 26,680 is free. The biggest single identified consumer is BLUETOOTH at 47,304 bytes, established by turning it off in `board.conf`, rebooting, and re-running the same measurement -- 189,624 bytes used with the radio down, 236,928 with it up, and 62 fewer allocated blocks. That is a causal comparison, not a correlation: same firmware, same boot, one config key changed. It also refines the firmware's own number, which reports 44,288 as the difference in free internal heap across `nimble_port_init()`; the block-walk sees 47,304, the difference being blocks that moved rather than appeared. The second is the SHELL'S TASK STACK at 24,576 bytes (`TDSH_SHELL_TASK_STACK`), visible as one block at a known address. The third is the 32 KB block that survives Bluetooth being off, and it is `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768` -- an ESP-IDF setting, at its default, that withholds 32 KB of internal RAM from ordinary `malloc()` so that task stacks and DMA allocations can still succeed when internal memory is otherwise full. IDF's own Kconfig says so: "the memory in this pool is not given out when a normal malloc() is called", and "FreeRTOS stacks are forced to internal memory, they will also use this memory pool". The rest is about 300 small allocations averaging under a kilobyte, which is what ESP-IDF's components look like; there is no fourth hog. THE BINDING CONSTRAINT IS NOT THE FREE TOTAL, and this is the number worth keeping. The largest free internal block is 9,728 bytes. That is why `pm start` failed with "no task" asking for a 10,240-byte stack while 26 KB was free: the pool is fragmented, so the question "is there room" has a different answer from "is there 10 KB in one piece". Two more numbers of the same kind: the IRAM-capable pool is at 9,352 free with 7,168 in its largest block and a lifetime low of 7,912 -- and one of its regions reports a minimum free of 308 bytes, which is close enough to exhaustion to matter for anything that allocates IRAM at runtime. THE DISTRIBUTION MATTERS MORE THAN THE TOTAL. Of the 236,928 allocated bytes: 57,344 (24 percent) is two blocks of 17-32 KB; 64,088 (27 percent) is nineteen blocks of 2-4 KB; 35,840 (15 percent) is five blocks of 4-8 KB; 30,192 (12 percent) is nineteen blocks of 1-2 KB; and 285 blocks of 128 bytes or less hold only 13,608 bytes between them, 6 percent. So the memory is NOT spread thin over many small things -- the small things are nearly free, and the mass is in about 45 blocks. THE INSTRUMENT IS PART OF THE RESULT. `mem` was written for this and is now in the port: `mem heap` (per capability, with the lifetime low-water mark), `mem regions` (IDF's per-region list, because a pool that is really three pools fragments differently from one), `mem tasks` (every task's unused stack bytes AND whether the stack is in internal SRAM or PSRAM -- the second is the column that matters, since a task on a PSRAM stack costs the scarce pool nothing), and `mem blocks` (a size histogram of allocated blocks via `heap_caps_walk()`, plus the largest blocks by address so one can be matched to a task's stack base or a driver's buffer). It is split into four subcommands because the desktop's terminal is 24 usable rows and the display mirror carries only what is ON the screen: output longer than the screen scrolls away and cannot be recovered from a capture, which cost two build cycles to learn. NOTHING HERE ALLOCATES: the command runs on the shell's task, which may have a PSRAM stack, so it uses a fixed task table and touches no file system. STATIC INTERNAL RAM WAS CHECKED SEPARATELY and is 97.6 KB of `.dram0.bss` plus `.dram0.data`, with 113 KB more of IRAM text -- and an attempt to attribute it per component from the linker map produced nonsense (three times the real total) because the map lists each input section more than once; that was abandoned rather than reported. What the symbol table does say is that it is flat: the largest single internal static object is 10,240 bytes (`s_tx` and `s_tx_app`, the USB link buffers), then 8,192 (`PM_RAM`), then a long tail. The heavy buffers are already in PSRAM where they belong -- `s_vt` 36 KB, `games_database` 48 KB, the display frame buffer, the emulator ROMs and frame buffers -- so `extram.lf` is doing its job and the PSRAM pool shows 428 KB used of 8.2 MB. A BY-PRODUCT WORTH RECORDING: turning a slot's reconnect off with `bt off <slot>` does NOT keep the radio down across a boot. Both slots were set off, the board was rebooted, and `bt` still reported "Radio: up, the stack took 44288 bytes", so something other than the reconnect path starts Bluetooth at boot. The baseline for this entry was therefore taken by disabling `bt.enable` in `board.conf` instead. Both slots were restored to "reconnects by itself" afterwards and the radio is back up.

#### Next Steps:

THE ACTIONABLE ITEMS ARE NOW SPECIFIC, and all four are tuning rather than redesign. FIRST, the 32 KB reserve is a knob: `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL` can go to 0 or to a smaller number, and the cost of getting it wrong is that a task stack or DMA allocation fails when internal RAM is otherwise full -- which is precisely the failure `pm start` already hits. Lowering it would make the headline free figure look better while making that failure MORE likely, so it should not be touched without a reason. SECOND, Bluetooth's 47 KB is the honest price of the keyboard and controller, and the only lever is `bt.enable`; it is on by default here because the watch has no other keyboard. THIRD, the fragmentation is the real defect: 26 KB free but 9.7 KB in the largest block is why a 10 KB stack cannot start, and the fix for that is to allocate the emulator stacks EARLY (before the small allocations chop up the heap) or to give them a dedicated region, not to free more memory. FOURTH, the IRAM low-water of 308 bytes in one region is the risk that has not bitten yet -- anything added that allocates IRAM at runtime should be measured with `mem regions` first. The instrument should be used before and after any such change, and it is worth noting that the app image is now 0x2e4c40 with only 0xB3C0 (46,016 bytes, 44.9 KB) free in the smaller OTA slot, so the next feature has to trade something out. Everything entry 41 listed stands: `update.url` is unset, `third_party/tdsh` is pinned behind its remote, the guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM (which is now a measured, not theoretical, problem -- they compete for the same 26 KB), the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, and `.ai/core.md`'s Purpose line still calls this a port of Linux. The backup `nvs64-2026-11-17-0755.bin` predates the `mem` command, so the image on the board is newer than the newest backup.

#### Files Modified:

None in this repository; the fork carries the instrument at 1e0a653 (ports/twatchultra/main/memdiag.c, memdiag.h, CMakeLists.txt, link_twatch.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A (a diagnostic, exercised on the watch by the operator's request)

---

## 41 COMMIT Unreleased 2026-11-17T08:00:00-07:00

#### Coming From:

Unreleased 705199e

#### Purpose:

Take the NVS change the author advised and entry 40 measured: grow NVS from 24 KB to 64 KB and equalise the two OTA slots, on the operator's explicit word and with the re-pairing accepted.

#### Outcome:

THE CHANGE IS DONE, THE BOARD RUNS, AND IT COST NOTHING THAT MATTERED. NVS is 64 KB, both OTA slots are 0x2F0000, and the only thing above 0x600000 that could have made this expensive -- `storage`, `modules`, `moddata` -- did not move, so there was no littlefs migration and no module relink. The final table, read back off the board's own flash at 0x8000 rather than taken from the CSV: `nvs 0x9000 64K`, `phy_init 0x19000 4K`, `ota_0 0x20000 3008K`, `ota_1 0x310000 3008K`, `coredump 0x600000 64K`, `otadata 0x610000 8K`, `storage 0x620000 8064K`, `modules 0xE00000 1M`, `moddata 0xF00000 1M`. THE RE-PAIRING THAT WAS BUDGETED FOR DID NOT HAVE TO HAPPEN, and the reason is worth keeping: the old 24 KB NVS blob was in the pre-change backup, the region starts at the same offset 0x9000 in both layouts, so it was written back into the front of the new 64 KB region instead of being wiped. The remaining 40 KB stayed erased and NVS claimed it on the next mount, which is visible in the page states -- the eleven appended pages all read 0xFFFFFFFF and the six original pages KEPT THEIR SEQUENCE NUMBERS 66/100/101/102/4 across the reboot, which can only happen if the mount succeeded and rewrote them. Result: 6 pages used and 10 free of 16, against 5 used and 1 free before. The account, the WiFi credentials and BOTH Bluetooth bonds survived and were confirmed on the glass: `bt` reports `2 stored in the board's NVS` with the K06 keyboard in combo and the Xbox pad in the controller slot, exactly as entry 39 left them. TWO MISTAKES WERE MADE AND FIXED IN THIS CYCLE, BOTH MINE. The first was arithmetic: `ota_0` was first placed at 0x1A000, which is where `phy_init` ends, and `0x1A000 + 0x2F0000 = 0x30A000` left a 24 KB hole before `ota_1` at 0x310000. A checker that walked the table and printed every gap caught it before anything was flashed. The correct offsets for two 0x2F0000 slots that end on the coredump boundary are 0x20000 and 0x310000, which leaves 24 KB unused at 0x1A000-0x20000 -- deliberate, and documented in the CSV, because the app offset stays 64 KB aligned. ESP-IDF asks only for 4 KB there without secure boot (`gen_esp32part.py:111`) and the bootloader maps from the aligned base below the image, so this is a convention bought for 24 KB of 16 MB, not a requirement. The second was a build-cache trap: after editing `partitions.csv`, `idf.py build` regenerated the app but `flash_args` still said `0x10000`, and writing that would have flashed the app straight over the new 64 KB NVS. The generated `partition-table.bin` was still the OLD table even though ninja listed the CSV as a prerequisite. `idf.py reconfigure` -- preceded by deleting the stale `partition-table.bin` -- produced the new table and the app offset moved to 0x20000. **The lesson is that a CSV edit is not a table change until `partition-table.bin` says so, and that file is what to read rather than the CSV.** THE DESTRUCTIVE STEP WAS SCOPED, NOT GLOBAL. `erase_flash` would have taken `storage` with it, which holds the launchers, the shell's config and the user's files; `erase_region 0x9000 0x11000` erased exactly the new NVS plus `phy_init` and nothing else. RF calibration in `phy_init` was regenerated by the next boot, which is what it is for -- ns8 still reports `cal_data`, `cal_mac` and `cal_version` after the reboot. THE NVS RESTORE WAS PROVEN AT THE FLASH LEVEL BEFORE THE DESKTOP WAS TRUSTED. The 64 KB region was read back and compared: the first 24 KB is byte-identical to the backup's, and the page states are 6 used plus 10 uninitialised. That mattered because the sign-in screen appeared once during this cycle -- the user saw it and started to create an account -- and the point of the restore was to make that unnecessary rather than to be argued about. The mount is what made it moot. NOTHING ELSE REGRESSED. The app boots from its new offset; `music /sd/media/audio/tone440_44100.flac` reports `state playing, 198144 frames, 0 errors, 0 short writes` with handle magic `464c4143`, so the module was accepted by the loader at the unchanged pair -- a stronger check than comparing symbols, since `modload_prepare()` refuses a stale pair and this one ran; `cat /root/Apps/pm.py` returns the corrected launcher, so littlefs and the user state in it are intact; and `bt` lists both slots. THE BACKUP FOR THE NEW LAYOUT WAS TAKEN AND ITS REGIONS VERIFIED. `nvs64-2026-11-17-0755.bin`, 16,777,216 bytes, sha256 `542396eb749c1c34dae4480a2c87b3a39a5ecfdd54a6551c9f7bfc98daae47d8`, verified against the chip with the board held in the stub and the board back up afterwards. The app at 0x20000, the module text at 0xE00000 and its rodata at 0xF00000 each compare byte-identical to the build. The pre-change image `pre-nvs64-2026-11-17-0740.bin` is kept because it is the only image in the OLD layout, and therefore the only way back to it.

#### Next Steps:

THE ONE THING THAT MUST NOT BE FORGOTTEN IS THAT THE APP IS NO LONGER AT 0x10000. Any hand-written esptool line that says `write_flash 0x10000 build/tinydesk.bin` now writes 64 KB of app image into NVS, which is exactly the failure this entry's own build-cache trap nearly caused. The correct single-region flash is `write_flash 0x20000 .../tinydesk.bin`, and `idf.py flash` reads `flash_args` and is safe. The Escapement repository's `scripts/flash.sh` still says 0x10000, but it flashes the Linux-era kernel and is archive, so it is correctly left alone rather than corrected. TWO AUTHOR ITEMS REMAIN from entry 40 and neither was touched here: `update.url` is unset, so Check for official updates still reads upstream's feed and would offer an image not built for this watch, and `third_party/tdsh` is pinned behind its remote. THE APP IMAGE BUDGET DID NOT CHANGE, only its honesty: 0x2e3fc0 against 0x2F0000 is 48.1 KB free, and before this change the same number was the limit while the table appeared to promise 3 MB. Two upstream bug fixes cost 352 bytes and that trend continues, so the partition change has removed the false headroom rather than created real room -- if a feature needs more than 48 KB, something has to be traded out. Everything entry 40 recorded about the code stands: the descriptor fix's own effect is still only argued from `vfs.c` and not observed, the guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM, the launchers have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, and `.ai/core.md`'s Purpose line still calls this a port of Linux. A SMALL OPERATIONAL NOTE: the watch stayed in the bootloader after several esptool reads because they use `--after no_reset`, and the display mirror then produced nothing at all -- which reads as a dead board and is not one. `esptool ... chip_id --after hard_reset` brings it back, and the mirror is only worth reading after that.

#### Files Modified:

None in this repository; the fork carries the change at 4ca882e (ports/twatchultra/partitions.csv, ports/twatchultra/README.md).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 40 COMMIT Unreleased 2026-11-17T07:30:00-07:00

#### Coming From:

Unreleased 7f3ce38

#### Purpose:

Read the TinyDesk author's reply to issue 20, act on what applies to this port, and take the NVS measurement that entry 39's session could not because the watch had been unplugged.

#### Outcome:

THE AUTHOR ANSWERED THE QUESTION THAT WAS ASKED AND THREE THAT WERE NOT. schikani on 2026-11-17: the partition scheme is FINE -- Software Update only asks ESP-IDF for the other update slot via `esp_ota_get_next_update_partition()` and refuses before writing if the image does not fit, so moving `otadata`, `coredump`, `storage`, `modules` and `moddata` is all safe, and TinyDesk ignores partitions it does not know. All it needs is a littlefs partition labelled `storage`, plus `otadata` and `nvs`. He also confirmed the two judgements already made here: emulators belong in C and not in `.tdapp` (which is MicroPython, though it can carry `.mpy` native code), and the XIP codec partition is "exactly the right instinct". He endorsed the name Escapement over TTWatch on the grounds that "TTWatch reads like a LilyGo product name", and offered the door that matters for later: "If you want the port upstream one day as `ports/twatchultra`, a PR is welcome." THE MEASUREMENT THAT ENTRY 39 COULD NOT TAKE WAS TAKEN, and it says his first warning is real rather than theoretical. NVS here is 24 KB, six 4 KB pages, and THE BOARD IS RUNNING WITH FIVE OF THEM FULL OR ACTIVE AND ONE UNINITIALISED: 420 of 756 entry slots written, 200 erased. The page sequence numbers are 66, 100, 101 and 102, which is the count of times each page has been recycled, so garbage collection is not occasional here -- it is continuous, with a single free page to work in. The failure it is heading for is specific and quiet: when no free page is left, `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES`, and `app_main()` answers with `nvs_flash_erase()` (`ports/esp_idf/app/main.c:909`), which is every account, every saved network and every Bluetooth bond, with no message to anyone. Every other ESP32 port in this tree gives NVS 0x10000; only `crowpanel7` matches this port's 0x6000. WHAT IS IN THERE, decoded with a parser written against ESP-IDF's own nvs_page.cpp rather than guessed -- the first attempt used the entry's first byte as its state and produced nonsense, because the states are 2-bit fields in the page's bytes 32 to 63 and the entries start at 64: ns10 is NimBLE's bond store (`peer_sec_1`, `our_sec_1`, `local_irk_1`, 8 entries), ns11 is this port's own Bluetooth slot blob at 136 bytes, ns1 is the shell's accounts (`db`, `access`, `proof`), ns7 is the WiFi station credentials (31 entries), ns8 is 1.9 KB OF REGENERABLE RF CALIBRATION (`cal_data` twice, `cal_mac`, `cal_version`), ns12 is the SSH host key and ns5 holds the access point configuration. Live payload is roughly 8 to 11 KB once duplicates still awaiting collection are discounted, so it FITS -- but it fits with one page of headroom, and the 1.9 KB of calibration is the kind of content that should not be competing for space with things that cannot be regenerated. The 136 bytes is also a useful confirmation of entry 38 from the other direction: `saved_slot_t` is 34 bytes, so four slots is exactly 4 x 34, which means the four-slot blob is on the board in the new format and was read back by `slots_load()` after both reboots. BOTH 0.5.2 FIXES APPLIED, and both faults were present here rather than theoretical. The Store divide by zero was verbatim at `apps/store.c:898`: `(int)((uint64_t)st.done * 100 / s_apps[s_busy_app].size)`, where `size` is 0 for an app listed offline, so an offline double click panicked the board -- and `on_install()` had no guard for a hidden Install button. Taken from upstream `03fc1b9`, which applied to `store.c` without a conflict; only `docs/VALIDATION.md` conflicted, and that file was left at this tree's version because it is upstream's QA record and not this port's. The descriptor fix is the one that matters most here and is taken from upstream `73a3bd7`: this port runs `CONFIG_ESP_CONSOLE_NONE`, so nothing ever opened descriptors 0, 1 and 2, and the first files opened got them, which meant a log line written through stdout landed inside whatever file held 1 and a read-only one made littlefs assert. `hold_std_fds()` now registers `/dev/tdnull` and opens it three times at the top of `app_main()`. Two things were checked before trusting it: the USB console mirror writes through `usb_serial_jtag_write_bytes()` in `link_usj.c` and never through fd 1, and the shell's Terminal replaces the global `stdout` with its own `funopen()` stream per task in `tdsh_bridge_esp.c`, so holding fd 1 cannot take the shell's output. THE MODULE HAD TO BE RELINKED AND THE ADDRESS REALLY DID MOVE THIS TIME. The app grew 352 bytes to 0x2e3fc0, `g_modhost_ptr` went 0x3c26bb00 to 0x3c26bbb0, and the blob was rebuilt against the regenerated header before flashing -- the mmap pair did not move, staying 0x42ad0000 and 0x3cbd0000, which is why `make modaddr` alone would not have been enough and `make flac` was needed too. Verified on the watch after flashing all three regions: the desktop renders, the USB mirror still delivers full repaints, the shell answers, `music` reports `playing, 211968 frames, 0 errors, 0 short writes` with handle magic `464c4143` and rate 44100, and `bt` still reports `2 stored in the board's NVS` with the keyboard in combo and the controller in its own slot.

#### Next Steps:

THE ONE THING NOT VERIFIED IS THE DESCRIPTOR FIX ITSELF, and the reason is worth recording rather than glossing. What is verified is that it broke nothing. What is not verified is that descriptors 0 to 2 are actually held, because the reproduction needs a failed download in the Store with WiFi up, and the cheap probe tried instead was inconclusive: `cat /dev/tdnull` from the shell printed `No such file or directory`, but tdsh resolves paths through `tdsh_path_to_real()` into its own filesystem layer and never reaches ESP-IDF's VFS, so that failure says nothing about the registration. The argument that it works is from IDF's source rather than from hardware -- `esp_vfs_register()` in `components/vfs/vfs.c:80-106` can only reject a length of 1, a length over `ESP_VFS_PATH_MAX`, a prefix not starting with `/` or ending with one, or an out-of-memory allocation, and `/dev/tdnull` at 11 characters at the top of `app_main()` trips none of them. If certainty is wanted, a ten-line diagnostic that records the result in a global and writes it to `/root/` once littlefs is up would settle it in one flash cycle. THE PARTITION CHANGE IS MEASURED, DESIGNED AND DELIBERATELY NOT DONE, because it destroys state and needs the operator's word. Growing `nvs` from 0x6000 to 0x10000 and equalising both OTA slots at 0x2F0000 fits with nothing above 0x600000 moving: `nvs` 0x9000 0x10000, `phy_init` 0x19000 0x1000, `ota_0` 0x1A000 0x2F0000, `ota_1` 0x310000 0x2F0000, and `coredump`, `otadata`, `storage`, `modules` and `moddata` all keep their addresses, so there is no littlefs migration and no module relink. The cost is that the new 64 KB NVS region would cover 0xF000 to 0x19000, which today holds `phy_init` and the first 36 KB of the app image, so NVS would find garbage pages and take the erase path: the keyboard and the controller would both need re-pairing. That can probably be avoided by writing the existing 24 KB blob back into the erased region before first boot, leaving ten clean pages for the stack to grow into, but that is a plan and not a measurement and should be proven rather than promised. It also cannot be done by OTA at all -- an update cannot change the partition table, so it is one serial flash with an erase. TWO SMALLER ITEMS STAND: `update.url` is unset in both `board.example.conf` and the device's `board.conf`, so Check for official updates would read upstream's feed and offer an image that is not built for this watch; and the `third_party/tdsh` submodule is pinned behind its own remote, which moved 0b8ebee to 53e8681 during this fetch while the shell stays at 0.5.1, so whether to follow it is a separate decision. Everything entry 38 and 39 listed is unchanged: guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM, the launchers and `.tdlink` files still have no installer, the Music progress bar still needs `total_samples`, `modload` still holds one blob, and `.ai/core.md`'s Purpose line still calls this a port of Linux. THE APP IMAGE IS NOW AT 48.1 KB FREE in the smaller 3 MB OTA slot, down from 48.4 KB, which is the trend worth watching: two upstream bug fixes cost 352 bytes, and the partition change is the only thing that buys real headroom back.

#### Files Modified:

None in this repository; the fork carries both fixes at 44f87fa (apps/store.c, ports/esp_idf/app/main.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PARTIAL (no regression verified on the watch; the descriptor fix's own effect needs a failed download to observe)

---

## 39 COMMIT Unreleased 2026-11-16T23:05:00-07:00

#### Coming From:

Unreleased 98a0b06

#### Purpose:

Check entry 38 on the watch, whose one unverified item was the move itself: a controller paired in the keyboard slot being re-paired into the controller slot, and whether that move survives a reboot.

#### Outcome:

THE MOVE WORKED AND IT SURVIVED A REBOOT, which was the part still open. The operator paired the pad into the Controller slot from the window, and `bt` then read `controller ready, Xbox Wireless Controller 78:86:2E:BB:45:79 (267 reports)`, `paired with Xbox Wireless Controller 78:86:2E:BB:45:79, reconnects by itself`, with the window showing `Controller  ready, Xbox Wireless Controller` and `last: connected:` and the keyboard slot reduced to `idle, not paired`. So the duplicate-slot clear in `ops_pair()` did what it was written to do -- the old slot was emptied, its connection closed, and the bond kept, since the pad came back without being put into pairing mode a second time. THE REBOOT IS THE TEST THAT MATTERS AND IT PASSED: after a hard reset the board reported the same arrangement, `2 stored in the board's NVS`, the keyboard still empty, and the controller not merely remembered but ALREADY RECONNECTED on its own -- `controller ready, ... (29 reports)` within the first half-minute of boot. That exercises the 4-slot NVS blob in both directions at once: written in the new wider format by the move, and read back by `slots_load()` on the next boot, which is the function that would have silently zeroed everything had it kept demanding the old 3-slot size. THE REPORT COUNTER IS THE PROOF OF A LIVE LINK RATHER THAN A REMEMBERED ONE, and it needs a caveat to be honest: it read 267 before the reboot and 29 afterwards, having reset with the connection, and then stopped climbing -- because an idle gamepad sends no reports. Its being greater than zero after a boot shows input arrived; whether it keeps arriving as the pad is used is the operator's to see, and the window's `last:` line is where that shows up. THE WINDOW FITS WITH FOUR SLOTS. Captured on the glass: values start in the column after "Controller" rather than against it, the three paired slots show their buttons and the empty keyboard slot shows only `[ Pair... ]`, and the message line `Xbox Wireless Controlle is ready` sits on row 24 with the frame closing on 25 -- so the recounting of rows in entry 38 was right about the 25-row desktop rather than the 26 first assumed.

#### Next Steps:

Nothing is outstanding from entry 38. Two things carry forward unchanged from entries 37 and 38 and are worth keeping in view rather than repeating in full: the app image has 0xC1A0 bytes free in a 3 MB OTA slot, so the next feature that needs real space has to trade something out, and the guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM, which has already failed twice for that reason. The controller slot opens one new question and no work: a gamepad now reaches the desktop's input path the same way a keyboard does, and whether the emulators take advantage of it -- the NES and Pokemon Mini draw their own d-pads and the NES already accepts a BLE Xbox pad through `gamepad.c` -- is a separate piece of work from pairing. Nothing about that is needed for the slot to be correct.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 38 COMMIT Unreleased 2026-11-16T22:45:00-07:00

#### Coming From:

Unreleased fa9b6bd

#### Purpose:

Give the Xbox controller a slot of its own, because it was sitting in the keyboard slot and taking the keyboard's place, and make the Bluetooth window fit four slots on a watch whose screen is 40 by 26.

#### Outcome:

THE CONTROLLER WAS NOT IN THE WRONG SLOT BY MISTAKE. A gamepad advertises the HID service but no keyboard appearance, so the scan list's rule -- "its own kind first, then other HID devices" -- made it an ordinary candidate for the keyboard slot, and pairing it there took the keyboard's place for as long as it was connected. That is why the fix adds a slot AND withholds a gamepad from the other three: adding the slot alone would have left the same trap one tap away. Appearance 0x03C3 (joystick) and 0x03C4 (gamepad) now map to a new TD_BT_KIND_GAMEPAD in both the NimBLE port and the Bluedroid one, and `collect()` skips any GAMEPAD whose slot is not the controller's. THE BUG THAT WOULD HAVE COST THE MOST WAS SILENT: the slot list is one NVS blob, and `slots_load()` required it to be exactly sizeof(s_slots). Going from three slots to four makes the array bigger, so the check would have failed, the blob would have been zeroed, and the first boot of the new firmware would have reported an empty slot list with no error anywhere -- a keyboard and a controller quietly forgotten. It now accepts any whole number of slots and clears only the tail, which works because the new slot was appended and the old blob is therefore a prefix. Verified on the watch rather than argued: the board reported "2 stored in the board's NVS" on the first boot after flashing, and still reported it after a second flash and reboot, with the K06 keyboard's address and name intact. THE MOVE PATH EXISTS SO THE OPERATOR DOES NOT HAVE TO FORGET ANYTHING. Pairing a device that is already stored in another slot now clears that slot and closes its connection, while deliberately keeping the bond in NimBLE's store -- the bond is what the new slot is about to use, and deleting it would force the controller back into pairing mode. A saved slot holds an address and a name and never the kind of device, so storage alone cannot identify the controller sitting in the keyboard slot; re-pairing it into the controller slot is what moves it, and that is a few taps because the controller is already a known device. FOUR SLOTS DID NOT FIT THE ROWS THE WINDOW ASSUMED. The window asks for 34 rows and is clamped: the desktop is 26 and the taskbar takes one, so the client is 23 rows with the default title bar and 22 with it set to Large -- measured on the glass, not calculated. Four slots at three rows each overflowed that, and the scan section, which is the reason anyone opens the window, would have been pushed off. The slots stay top-anchored and the scan section is now anchored to the BOTTOM, taking whatever rows are left (four at 23, three at 22) rather than sitting at fixed rows. A second, smaller defect was visible in the first capture of the new window: the row read "Controlleridle, not paired", because the heading column started its value at 11 and "Controller" is ten characters. Values now start at 12. THE API GATE FIRED AND WAS SATISFIED RATHER THAN SILENCED. `api_stable` failed on the two changed enums, which is the mechanism working; the snapshot was re-recorded with the tool's own `--record`, and the diff is two lines, both additions -- one enum member in each of TD_BT_SLOT_* and TD_BT_KIND_*. TD_VERSION was left at 0.4.0: the semver rules in STABILITY.md apply from 1.0, and the version text lives in six port CMakeLists plus README and RELEASE_NOTES, which is a release decision rather than part of this change. NIMBLE'S CONNECTION LIMIT HAD TO MOVE AND IT IS CHEAPER THAN IT LOOKS. CONFIG_BT_NIMBLE_MAX_CONNECTIONS goes 3 to 4, because four slots can each hold a live device; the board already sets CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL, so the stack's memory including the per-connection state comes from PSRAM and no internal SRAM is spent -- which matters on the one board where internal RAM is the scarce pool. Verified in build/config/sdkconfig.h rather than assumed from the file edit. TESTS NOW RUN AT THE SIZE THE GLASS REALLY IS. test_bluetooth drives the window at 80x45 by default, which is roomier than the watch and would pass with a layout that never fits; it now closes that window, rebuilds the terminal at 40x26, checks the client is 23 rows, presses the Controller slot's Pair button there, and then repeats at the Large title bar size (22 rows). That last phase also proves the button is reachable rather than merely drawn, since pressing it starts a scan. On hardware the scan section was captured rendering in its real place: label on row 17, "Listening... 4 s" on 18, the list on 19 to 22, Pair, Scan again and Cancel on 23, and the message on 24 with the frame closing on 25 -- one row clear of the bottom. THE MODULE HAD TO BE RELINKED, and the address did not move, which is worth recording because it is the opposite of the last two times. The app grew by 496 bytes with the first build and the build after the comment fixes was byte-for-byte the same size, so g_modhost_ptr stayed at 0x3c26bb00 and the mmap pair stayed 0x42ad0000 and 0x3cbd0000; `make modaddr` was still run and still reported the same pair, the module was rebuilt against the regenerated header, and all three regions were flashed. `music` then played the 440 Hz tone at "129024 frames, 0 errors, 0 short writes" with the handle reporting rate 44100, so the relinked blob is accepted and decodes. One environment trap cost a few minutes and is worth writing down: the module Makefile needs IDF_PATH and the xtensa toolchain on PATH as well as IDF_PYTHON_ENV_PATH, because read_modaddr.py finds esptool through the venv while the compile needs IDF_PATH/components/xtensa/esp32s3/include for xtensa/config/core-matmap.h.

#### Next Steps:

THE ONE THING NOT VERIFIED ON HARDWARE IS THE MOVE ITSELF, and it is the step the operator has to take: pair the Xbox controller into the Controller slot once. It is currently paired in the keyboard slot with reconnecting off ("paired with Xbox Wireless Controller, 78:86:2E:BB:45:79, off"), so the sequence is Bluetooth, Controller, Pair..., put the pad in pairing mode, pick it, Pair -- and the keyboard slot should then read "not paired" while the controller slot holds it. The code that does the move is reviewed and its per-installation NVS behaviour is verified, but the move has not been observed, because it needs the physical controller awake and in pairing mode. A second thing follows from the first and is a decision rather than a fix: the keyboard slot still offers non-gamepad HID devices, including keyboards, so a keyboard could be paired there instead of in the combo slot; that permissiveness is the same escape hatch that `bt pair <slot> <device>` relies on for devices that never announce their kind, and tightening it would break that case. Everything entry 37 listed stands unchanged: guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM, the launchers and `.tdlink` files still have no installer, a progress bar for Music still needs `total_samples` in the module handle, `modload` still holds exactly one blob, and `.ai/core.md`'s Purpose line still says the project is a port of Linux. The app image is at 0x2e3e60 with 0xC1A0 bytes free in a 3 MB OTA slot -- 2 percent -- so the next feature that needs real space will have to trade something out rather than grow.

#### Files Modified:

None in this repository; the fork carries the change at 869cbfe (apps/bluetooth.c, include/tinydesk/td_sysinfo.h, ports/common/td_proto_cmds.c, ports/esp_idf/app/bt_hid.c, ports/esp_idf/app/bt_hid_bluedroid.c, ports/twatchultra/sdkconfig.defaults, api/tinydesk.api, tests/test_bluetooth.c).

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PARTIAL (four slots and both old pairings verified on the watch; moving the controller into its slot needs the operator's hands)

---

## 37 COMMIT Unreleased 2026-11-16T21:40:00-07:00

#### Coming From:

Unreleased bee1216

#### Purpose:

Take the full 16 MiB backup owed after entry 36's device-side fix, so the corrected Start-menu launchers are captured in an image that can be restored rather than only existing as text in a repository.

#### Outcome:

THE BACKUP EXISTS AND IS VERIFIED AGAINST THE CHIP. `pinball-fixed-menu-2026-11-16-2134.bin`, 16,777,216 bytes, sha256 `48dd838b1931ce4d4ca49bccf6e51630d14a65484c278cfae660dfde1d014e31`. The read took 230.3 seconds at 582.7 kbit/s and the verify reported `verify OK (digest matched)` with the board held in esptool's stub between the two, so the comparison was against the same flash the read came from rather than against a board that had rebooted and written NVS in between. The board was reset afterwards and came back: the script's own liveness check printed `board is up`. ALL THREE CODE REGIONS WERE COMPARED AGAINST THE BUILD THAT PRODUCED THEM, because entry 20 and entry 25 both record that a digest read at one moment will not match one read at another once the app descriptor's build time moves: the app at 0x10000 is `b6f1ce70`, the module's text at 0xE00000 is `2ef7b97a`, and its rodata at 0xF00000 is `ff665897`, each byte-identical to `build/tinydesk.bin`, `modules/build/flac.bin` and `modules/build/flac.data.bin`. THE REASON THIS BACKUP WAS WORTH FOUR MINUTES WAS THEN CHECKED RATHER THAN ASSUMED. The point was not the firmware -- that was already safe in git at `5021959` -- but the littlefs state, which a full-image flash destroys. The `storage` partition was extracted from the image at 0x620000 and mounted with littlefs-python at block_size 4096 and block_count 2016, and both launchers were read back out: `/root/Apps/pm.py` holds `import tinydesk` then `tinydesk.shell("pm start")`, and `/root/Apps/tama.py` holds `import tinydesk` then `tinydesk.shell("tama")`. That is entry 36's fix, in the image, restorable. So the two launchers now exist in three independent places -- the device, this backup, and `scripts/apps/` -- where before this cycle they existed in one. THE FIX HAS STILL NOT BEEN SEEN TO SURVIVE A FULL-IMAGE RESTORE, which is the only thing three copies buy and therefore the only thing left untested: the restore path is `esptool write_flash 0 0x1000000`, and it is untested here for the same reason it is rarely tested, that a 16 MiB write costs about six minutes and ends with a board in an unknown state. The claim this entry makes is narrower and it is exact: the bytes are in the file and the file matches the chip.

#### Next Steps:

NOTHING IN THE FORK NEEDED COMMITTING, which is itself the finding: the flashed app was already at `5021959` and the only change since entry 35 was on the device's littlefs, so `git status` is clean on both repositories and this backup's distinct content is entirely user state. The untested restore is the first thing to do when a full-image flash is next wanted for another reason, rather than as an experiment of its own. Everything entry 36 listed still stands: guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM and two of them have already failed for exactly that reason, the launchers and the `.tdlink` files still have no installer beyond the documented commands in `scripts/apps/README.md`, a progress bar for the Music window still needs `total_samples` in the module handle, `modload` still holds exactly one blob, and `.ai/core.md`'s Purpose line still says the project is a port of Linux. The one item this cycle moved is the backup itself, which was the last outstanding artifact in the sequence that started at entry 32.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 36 COMMIT Unreleased 2026-11-16T20:11:21-07:00

#### Coming From:

Unreleased 2fbf347

#### Purpose:

Fix the Start-menu entries that launch the Pokémon Mini and Tamagotchi emulators, because one of them printed a Python traceback instead of starting the game.

#### Outcome:

THE POKEMON MINI ENTRY WAS MISSING ONE LINE, AND THE TAMAGOTCHI ENTRY WAS NOT BROKEN AT ALL. `py` on this port is MicroPython with a `tinydesk` module that is REGISTERED BUT NOT AUTO-IMPORTED (`ports/common/python/td_py_mod.c` registers it as an ordinary module; nothing injects it into globals). `/root/Apps/pm.py` called `tinydesk.shell(...)` on line 1 without `import tinydesk`, so every launch from the Start menu raised `NameError: name 'tinydesk' isn't defined`. `/root/Apps/tama.py` already began with `import tinydesk` and worked, so the belief that both had the same bug was half right: the failure looked identical from outside, and the difference was a single line. Both were verified FROM THE START MENU rather than from the shell, which is the distinction that matters here -- `py /root/Apps/pm.py` succeeding would not have proven the menu path works, and the menu path is the one that failed. THE FIX SIMPLIFIES AS IT REPAIRS. The old line carried the whole ROM path with spaces, commas and parentheses inside a Python string: one typo away from broken, and awkward to re-enter over a console. It is now `tinydesk.shell("pm start")`, which uses the firmware's own `PM_ROM_DEFAULT` -- and that default was verified in the previous cycle to be exactly that Pokémon Pinball cartridge. That also makes the two launchers symmetrical, since `tama.py` already relied on `TAMA_ROM_DEFAULT` the same way. THE FILES HAD NO HOME IN ANY REPOSITORY and that is now corrected: they live on the device's littlefs, which survives app flashes but is not created by a full-image flash, so they could not be restored after one. `scripts/apps/` now holds both, with a README stating the rule (both lines, in this order), the exact `NameError` so the symptom is searchable, why neither passes an argument, and install commands that were RUN VERBATIM and then checked by reading both files back. A METHOD NOTE THAT COST TIME AND IS WORTH KEEPING: writing the files needed `echo` with single quotes around a Python string containing double quotes, and `tdsh` keeps single quotes literal, so `echo 'tinydesk.shell("pm start")' >> file` survives intact. Testing it on a scratch file first (`/tmp/t1.py`) before touching the real one is what made the write safe. THE HARNESS LESSON FROM ENTRY 35 WAS APPLIED AND IT WORKED: escape sequences written in one `write()` each, so `F10` opens the menu rather than arriving as Escape followed by literal text. Navigating to the user's own entries needed type-ahead rather than arrow counting -- 'p' selects Pixels and a second 'p' reaches Pokémon Mini, because `td_type_ahead` steps to the next match; End reaches the very bottom, which is the users section and "Sign out", not the app list.

#### Next Steps:

THE OLD ROM PATH IS STILL IN THE TRACE, AS EVIDENCE AND NOT AS A CONFIGURATION. `/root/Apps/pm.py` previously named `/sd/media/min/homebrew/Pokemon Pinball Mini (USA, Europe).min` explicitly, and that is now implicit in `PM_ROM_DEFAULT`; if the card's layout ever changes, the launcher follows the firmware rather than drifting from it. The larger outstanding item is unchanged and is the one entry 35 named: guest stacks in `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` are still internal DRAM, and two of them have already failed for exactly that reason. A second, smaller thing is now visible and worth a decision rather than a fix: the `.tdlink` files and these Python launchers are user state on littlefs with no installer, so a full-image flash still loses them even though they are now reproducible from `scripts/apps/`. Whether that becomes a deploy script or stays a documented manual step is the operator's call, and the README in that directory is written to serve either. Nothing else regressed: Pokémon Mini was relaunched from the Start menu at the end of this cycle and reported 71.8 fps with missed=0, and the Tamagotchi was started and stopped from the menu in the same session.

#### Files Modified:

- scripts/apps/pm.py
- scripts/apps/tama.py
- scripts/apps/README.md

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 35 COMMIT Unreleased 2026-11-16T19:56:37-07:00

#### Coming From:

Unreleased 5021959

#### Purpose:

Launch Pokemon Pinball on the Pokemon Mini emulator, and take the 16 MiB backup owed from entry 34.

#### Outcome:

POKEMON PINBALL RUNS, AND IT NEEDED THE SAME FIX THE GAME BOY DID. `pm start` with no argument is the whole command: the default ROM is `/sd/media/min/homebrew/Pokemon Pinball Mini (USA, Europe).min`, resolved through `sd_media()`. The first attempt printed `pm: no task` -- `PM_STACK` is 10,240 bytes of internal DRAM and the largest internal block measured 9,728 with the desktop up and a Terminal open, so the allocation could not succeed. The identical shape as entry 34, one commit later, which is the useful part: this is a CLASS of failure rather than two coincidences, and the third emulator to gain a task would have hit it too. The task moved to PSRAM with `xTaskCreatePinnedToCoreWithCaps(..., MALLOC_CAP_SPIRAM)` and its exit became `vTaskDeleteWithCaps()`. The safety argument is the same one and is worth keeping specific rather than general: this task runs PokeMini and nothing else, `load_rom()` happens on the caller's task before it starts, the emulator's state and frame buffers are PSRAM, and the save path snapshots here and writes elsewhere -- so the task opens no file and maps no partition, and never disables the cache a PSRAM stack lives behind. Verified on hardware: `pm: running /sd/media/min/homebrew/Pokemon Pinball Mini (USA, Europe).min (524288 bytes)` and the operator confirmed it on the glass. THE MAIN-BRANCH HARNESS BUG OF THIS WHOLE STRETCH WAS FOUND HERE AND IS WORTH RECORDING. Escape sequences were being written one byte per 100 ms, and the desktop's parser gives up on a lone `ESC` after a few tens of milliseconds -- so every "F10" was arriving as Escape (which CLOSES the menu) followed by the literal text `[21~`. That is why the Start menu kept closing instead of opening, why menus were found scrolled to the bottom, and why several earlier conclusions about Enter not working were wrong: Enter works, and the menu was being dismissed before it could be used. The fix is one write per sequence. TWO SMALLER HARNESS FACTS CONFIRMED RATHER THAN ASSUMED: the watch re-enumerates between `ttyACM0` and `ttyACM1` and `usb-...80:45:6B:46:DF:54-if00` identifies it while `/dev/ttyACM0` was the TinyTang CDC; and the display mirror is not a frame counter -- it sent 32 bytes per two-second window while Pokemon Pinball was visibly running, because an emulator blits straight to the framebuffer and never touches the text-cell model, so mirror traffic says nothing about whether a picture is animating. THE OWED BACKUP WAS TAKEN. `pinball-gb-music-2026-11-16-1952.bin`, 16,777,216 bytes, sha256 `e7cd9c68d9274fa7`, and its three code regions were each compared byte-for-byte against the build that produced them: the app at 0x10000 (`b6f1ce70`), the module's text at 0xE00000 (`2ef7b97a`) and its rodata at 0xF00000 (`ff665897`) all match. That closes entry 34's outstanding item.

#### Next Steps:

THE CLASS IS THE THING TO ACT ON, not these two instances. Two emulators have now failed with "no task" because a guest stack was internal DRAM on a board whose internal RAM is the scarce pool, and every other guest task in the port is a candidate: `tama.c`, `nes.c`, `gbcbench.c` and `pmbench.c` should be swept for `xTaskCreatePinnedToCore` with a large stack, and the rule written down is that a task which runs an emulator and touches no file belongs on PSRAM. `nes.c` at least already runs on this board, so its allocations fit today -- but that is a measurement about one game with one ROM, not a guarantee for the next one, and the sweep is cheap. The remaining emulator gaps are unchanged: the Game Boy and Pokemon Mini ports leave their two margin columns unassigned, and the NES is still about 21 fps because its blit makes four passes over PSRAM. Beyond the emulators, three items stand from entries 32 to 34 -- a progress bar needs `total_samples` in the module's handle, `modload` still holds exactly one blob, and `uac2.c` and `usbhost.c` create caps-aware tasks that do not self-delete today but would need `vTaskDeleteWithCaps()` if they ever did.

#### Files Modified:

- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 34 COMMIT Unreleased 2026-11-16T19:46:24-07:00

#### Coming From:

Unreleased 0644fef

#### Purpose:

Boot the Game Boy Color emulator with Zelda running at the operator's request, which turned up a start failure that had nothing to do with the emulator.

#### Outcome:

ZELDA RUNS, AND IT COULD NOT BE STARTED FROM A SHELL UNTIL ONE LINE CHANGED. The operator asked for the GBC emulator with Zelda, and the ROM is the firmware's own default -- `Legend of Zelda, The - Link's Awakening DX (USA, Europe) (Rev 2) (SGB Enhanced) (GB Compatible).gbc` under `/sd/media/gbc/homebrew/`, resolved through `sd_media()` with `/sd/game.gbc` as the flat fallback -- so `gb start` with no argument is the whole command. The first attempt printed `gb: no guest task` and ran nothing. THE CAUSE WAS INTERNAL DRAM AND THE MESSAGE WAS HONEST. The guest task's stack was 8 KB of internal DRAM, and with the desktop up and a Terminal open internal free measured 25,395 bytes with the largest block 9,728 -- an 8 KB stack plus its TCB does not fit, so `xTaskCreatePinnedToCore()` returned an error and the code said so. PSRAM had 7.7 MB free. THE FIX IS THE RIGHT ONE FOR A REASON WORTH STATING RATHER THAN A TRADE. The guest task runs gnuboy and nothing else: the cartridge and both frame buffers are already PSRAM allocations, it opens no file and maps no partition, so it never disables the cache that a PSRAM stack lives behind -- the constraint that forced `modload_prepare()` and every `/fs` read onto specific tasks does not apply to it. `xTaskCreatePinnedToCoreWithCaps(..., MALLOC_CAP_SPIRAM)` moves the stack where the memory actually is, and the delete became `vTaskDeleteWithCaps()` to match, because plain `vTaskDelete()` would leak the PSRAM stack exactly as the music player did in entry 33. The save task, which does write to `/fs`, is left on internal RAM deliberately. VERIFIED ON HARDWARE: `gb start` loads the cartridge, reports `gnuboy_load_sram: Loaded SRAM bank 0..2` and `gb: save restored from /sd/saves/gbc/...sav`, and `gb status` then reads `running=1 frames=1068 blits=1066 dropped=0 audio=on 43691Hz` with the panel showing pixels rather than text. The operator played it and passed it. TWO SMALLER THINGS WERE LEARNED ABOUT THE HARNESS AND ARE RECORDED BECAUSE THEY MISLED ME HERE. The Start menu is open at boot and `Enter` does open the selected app -- but the screen mirror only sends CHANGED cells, so the frame captured immediately after Enter still showed the menu and I twice concluded the key had not landed when the Terminal was in fact open and focused. And a command typed into a Terminal that has just opened is lost: the pause has to be long enough for the window to take focus, not merely for the frame to paint. The menu itself was confirmed to have changed as intended in entry 33 -- `>> Music` now appears at the bottom of the list, immediately before the separator and the user's own `.tdlink` entries. NO BACKUP WAS TAKEN THIS CYCLE AND THAT IS OWED: the board re-enumerated to `/dev/ttyACM0` and a 16 MiB read needs a reset, which would have ended a game the operator was playing. The previous image `music-ui-2026-11-16-1921.bin` (sha256 `e7bc1c62`) holds everything except this one app-image change, and that change is a single call in `gb.c` reproducible from `0644fef`.

#### Next Steps:

A 16 MiB backup is owed at the next convenient reset, since this cycle's image is on the board but only the previous one is archived. Beyond that, three things stand from entries 32 and 33 and none is affected by this change: the progress bar needs `total_samples` in the module's handle, the module loader still holds exactly one blob, and `uac2.c` and `usbhost.c` create tasks with `xTaskCreateWithCaps()` that do not self-delete today but would need `vTaskDeleteWithCaps()` if they ever did. The Game Boy's own known gaps are unchanged and unchanged by this: the two margin columns are unassigned in the emulator ports, and the NES remains at roughly 21 fps because of its blit. One thing this cycle makes worth checking when convenient: `gb start` now spends PSRAM on a stack that it did not before, and the measurement that would close the loop is internal free with the emulator running -- which was 25,395 before the change and should be unchanged after, since the stack is no longer internal.

#### Files Modified:

- ports/twatchultra/main/gb.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 33 COMMIT Unreleased 2026-11-16T19:25:27-07:00

#### Coming From:

Unreleased 69cb2c9

#### Purpose:

Fix the player dying after four or five tracks with "error: no amplifier", and move the Music window out of the first Start-menu slot.

#### Outcome:

THE FAILURE WAS TWO LEAKS AND NEITHER WAS THE AMPLIFIER. The message named a symptom: "no amplifier" was the catch-all for five different faults inside `sound_stream_open()`, and the cause was memory rather than audio. Counting allocations instead of guessing found both. FIRST, THE DECODER'S WORKING BUFFER WAS NEVER FREED. micro-flac keeps it until `~FLACDecoder()`, and nothing ever called the destructor -- the player freed the state BLOCK it owns and not what lived inside it -- which cost 41 KB of PSRAM and 356 bytes of internal RAM per track. The module contract now has a TEARDOWN: a call with `in == NULL` asks the module to destroy what it constructed in the caller's block and release everything it took from the host table, and that is `MODLOAD_ABI_VERSION` 3 so a blob built for 2 is refused rather than allowed to leak silently. SECOND, THE PLAYER TASK WAS DELETED THE WRONG WAY. `idf_additions.h` states plainly that a task created with `xTaskCreateWithCaps()` "must only be deleted using `vTaskDeleteWithCaps()`"; plain `vTaskDelete()` hands the task to the idle task, which frees the TCB it knows about but not the PSRAM stack the caps-aware API allocated. That was exactly 8,196 bytes of PSRAM and 356 bytes of internal RAM per track -- the numbers matched the leak too well to be a coincidence, and they only became visible after the module's own allocations were counted separately and came back clean. AFTER BOTH, THE HEAP IS FLAT: over eight consecutive tracks PSRAM returns to the same byte (7,786,392), the internal delta is 0, and the module reports 0/0 live allocations after every track. THE INSTRUMENT WAS THE POINT. `modhost.c` now counts what a module asks for and gives back, using `heap_caps_get_allocated_size()` to recover a block's size at free time, and `MUSIC_UI_SELFTEST` plays a run of tracks and writes PSRAM, internal RAM and the module's live count after each one. That division is what separated the two leaks: the module's number went to zero after the teardown fix while the process still lost 8,196 a track, and the remainder was the task. A THIRD MOVING ADDRESS IS NOW CHECKED. `g_modhost_ptr` -- the API table's address -- moved four times in one afternoon (0x3c26b8c8, 0x3c26b99c, 0x3c26ba78, 0x3c26bb50) and a blob holding a stale one does not fault; every call returns `MODCALL_ERROR` and the only symptom is "decode error". Entry 30's guard compared the two mmap bases and nothing else, which is why that failure stayed silent. The blob header now records `linked_host`, `modload_prepare()` compares it against the live symbol, and `patch_header.py` reads it out of `modaddr.h` at PATCH time rather than trusting a value the Makefile captured at parse time -- a stale parse-time read produced a blob claiming `host=0x0` on the first attempt, which the patcher now refuses outright rather than writing. AND "NO AMPLIFIER" NO LONGER COVERS FIVE FAULTS: `sound.c` records the exact call and its `esp_err_t` name, the player reports that string, so the next occurrence says whether disable, clock, slot, enable or the PSRAM allocation failed. THE MENU MOVED WITH A HOOK RATHER THAN A HACK. Registration order is Start-menu order, and a board registers during init, before the desktop's own apps -- which is why Music was first. A weak `board_apps_register()` in the shared app runs after `td_apps_register_all()`, every other board links unchanged, and the port's own apps now land at the end of the app list immediately before the separator and the user's emulator `.tdlink` entries, which is where the operator asked for it. Verified on the device: window=1, 74 tracks, playback real-time at err=0 over eight tracks, and the app, module and all three linked addresses (inst, data, host) agreeing with the running image. A full 16 MiB backup was taken and its app and module regions confirmed byte-identical to the build.

#### Next Steps:

THE OPERATOR SHOULD PLAY SEVERAL TRACKS AGAIN, and the thing to watch is whether the run now goes past four or five: the two leaks are gone and measured flat, but the operator's failure was never reproduced deliberately, so the honest position is that the cause is removed rather than that the symptom was observed disappearing. If it recurs, the message will now name the failing call instead of saying "no amplifier", which makes the next step a specific one rather than another hunt. TWO THINGS REMAIN OWED IN THIS AREA. A progress bar needs `total_samples` from STREAMINFO, which means a field in `flacmod.cpp`'s 32-byte handle -- an ABI change of the same kind the teardown just made, and the moment to do it is the next time that struct is touched. And `uac2.c` and `usbhost.c` also create tasks with `xTaskCreateWithCaps()`; they do not self-delete today, so they do not have this leak, but anyone adding a self-delete to either must use `vTaskDeleteWithCaps()`. Beyond music, the module loader still holds exactly one blob, so a second codec remains the structural piece.

#### Files Modified:

- ports/twatchultra/main/music_player.c
- ports/twatchultra/main/music_ui.c
- ports/twatchultra/main/sound.c
- ports/twatchultra/main/sound.h
- ports/twatchultra/main/modhost.c
- ports/twatchultra/main/modhost.h
- ports/twatchultra/main/modload.c
- ports/twatchultra/main/modload.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/modules/src/flacmod.cpp
- ports/twatchultra/modules/src/modtest.c
- ports/twatchultra/modules/Makefile
- ports/twatchultra/modules/patch_header.py
- ports/esp_idf/app/main.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 32 COMMIT Unreleased 2026-11-16T18:25:46-07:00

#### Coming From:

Unreleased df9de5b

#### Purpose:

Give the FLAC player a window: a playlist of tracks from the SD card with a transport row, laid out on TinyPhosphor's design, so music can be chosen and played from the desktop instead of by typing a path at the shell.

#### Outcome:

THE MUSIC WINDOW EXISTS AND ITS PLAYER PATH IS PROVEN ON THE DEVICE. `Music` appears in the Start menu and opens a 38x20 window shaped like TinyPhosphor's player, shrunk: a folder line, the library with a scrollbar, then now-playing, elapsed time and a Play/Stop/Prev/Next row anchored to the bottom edge so a resize grows the list and leaves the transport where it was. `modflac.c` became `music_player.c` and grew a playlist -- `music_tracks_scan()` reads a directory into a PSRAM name pool, because 256 names of 72 bytes is 18 KB that would be internal DRAM as a static array, the same mistake that once pushed `ssh start` under its 65,536-byte floor; and the list is SORTED, because readdir order on FAT is the order the files were copied, so a card written by a ripper lists 01, 10, 11, 02 and a playlist is read by a person. The window is a view, never a second player: it calls `music_play_index()` and reads `music_status_get()` twice a second, while the shell's `music` command drives the same task, so the amplifier cannot be held twice. THE EVIDENCE IS A FILE, NOT THE SCREEN, AND THAT IS THE LESSON OF THE CYCLE. The display mirror sends only CHANGED cells and attached stale or incomplete repeatedly -- twice I read its `░` wallpaper as a stuck shadow and drew a wrong conclusion from it -- so a gated `MUSIC_UI_SELFTEST` drives `launch()` and then the very handlers the buttons call, and writes the result to `/fs/root/musicboot.txt` for esptool to read. It reported `window=1`, `tracks=74`, `first=01 - Bohemian Rhapsody.flac`, `dir=/sd/ROCKBOX/Audio`, `play=running:1 state:1 idx:0 frames:253440 rate:44100 secs:5 err:0 short:0`, `now=01 - Bohemian Rhapsody.flac`, `stopped=running:0 state:0`, `after_next=running:0 sel=1`. 253,440 frames in 5.75 s is real time with no decode errors, and Next moving the selection without starting audio is the designed behaviour for a transport pressed while nothing plays. TWO REAL DEFECTS SURFACED, BOTH FOUND BY THAT FILE. Status reported `rate 0` during playback, because the rate is only copied out of the caller-owned state block when the task ENDS -- so the UI's clock would have sat at 00:00 for a whole track; it now reads the live handle. And the module was linked against a stale `g_modhost_ptr` (`0x3c26b8c8` against the app's `0x3c26b99c`, then `0x3c26ba78`, `0x3c26b9a4` -- it moves on almost every build), surfacing only as "decode error" in the status line. THE GUARD FROM ENTRY 30 DOES NOT COVER THIS: `modload_prepare()` compares the two mmap bases and nothing else, so a blob whose host-table pointer is stale is accepted and fails at the first API call. The fix each time is `make modaddr` then `make flac` after every app build, which is now written into the README's build section with the order that matters -- build, flash, boot, modaddr, make flac, flash. A THIRD, SMALLER FIX: the status line printed `00:00 playing 0 Hz` for the few hundred milliseconds before STREAMINFO parses, which is a number that is not true; the rate field is now omitted until it is known. THE SHIPPED IMAGE IS VERIFIED COHERENT: the app region read back off the chip is byte-identical to the build (sha256 `8dbcd979cc464731`), and the blob carries 9 references to the app's live `g_modhost_ptr` at `0x3c26b9a4`. WHAT IS NOT VERIFIED IS THE PART THE OPERATOR HAS TO DO: nobody has yet seen the window on the glass or heard a track started by the Play button. The self-test proves the handlers work on the device, not that the pixels are right or that the buttons are where a thumb lands.

#### Next Steps:

THE OPERATOR'S LOOK IS THE REMAINING STEP and everything else waits on it: open Music from Start, confirm the list and the transport row are legible at 40 columns, tap a track and Play, hear it, tap Stop. If the layout reads badly the fix is geometry (the window is 38x20 of a 40x26 desktop) rather than plumbing. Two things are known to be missing from the window on purpose and are the obvious next additions: a PROGRESS BAR, which needs `total_samples` from STREAMINFO and therefore a field added to `flacmod.cpp`'s 32-byte handle -- an ABI change and a module rebuild, which is why it was deferred rather than forgotten; and VOLUME, which this player has no notion of at all, because the I2S path writes samples straight to the amplifier. The inline `music` command remains the fallback if the window is ever wrong, and `MUSIC_UI_SELFTEST` stays in the tree commented out in CMakeLists as the instrument to reach for when a later display question cannot be answered by looking. Beyond the window, the module loader still holds exactly one blob, so a second codec remains the structural piece rather than another decoder.

#### Files Modified:

- ports/twatchultra/main/music_player.c
- ports/twatchultra/main/music_player.h
- ports/twatchultra/main/music_ui.c
- ports/twatchultra/main/music_ui.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 31 COMMIT Unreleased 2026-11-16T17:25:50-07:00

#### Coming From:

Unreleased a043a9d

#### Purpose:

Make the documentation say what the project is rather than what it was, because the front page still opened with "Linux on a T-Watch Ultra" for a line of work closed on 2026-11-11.

#### Outcome:

THE README NOW OPENS WITH THE DESKTOP, NOT THE KERNEL. `README.md` described a NOMMU Linux 7.2.4 that boots to a bash login, which is accurate history and no longer the project: the operator's direction is that the Linux line is finished and TinyDesk is where the work continues. The new front page says what runs on the watch today, and gives the Linux era its own section that states plainly why it was abandoned -- too heavy, because a kernel plus a rootfs plus a userspace on 8 MB of PSRAM leaves nothing over -- with the closing commit named (`8645a82`, 2026-11-11) so the claim is checkable rather than a matter of tone. THE ARCHIVE IS LABELLED RATHER THAN DELETED, which is the part that needed judgement. `patches/`, `userspace/` and `probes/` remain, because `docs/panel-geometry.md` is still the only measurement of where the chamfers are and the current desktop is laid out inside that rectangle, and because a future reader is owed the reason a whole approach was dropped instead of a silent gap. A NEW `docs/README.md` SORTS EVERY DOCUMENT INTO CURRENT AND ARCHIVE, which is the piece that was actually missing: fourteen documents sit in a flat directory where three describe the running system and eleven describe code that no longer runs, and nothing in the filenames says which. It also resolves the scripts, and found that the split is not what the dates suggest -- `watch-backup.sh`, `tamacore-host.sh` and `tamacore-sync.sh` are live while every `build-*.sh` is not, and `watch-backup.sh` works only because it reaches into the old toolchain's Python at `build/idf-tools/python_env/idf5.1_py3.14_env`, which still exists and carries esptool 4.13; that dependency is now written down before somebody deletes `build/`. THREE CLAIMS IN THE OLD TEXT WERE NOT TRUE AND ARE CORRECTED RATHER THAN RE-WORDED. It asserted GPLv3 for this repository "matching upstream", but there is no LICENSE file here at all and the firmware now lives in an MIT fork, so the page records the gap as a decision owed instead of inheriting a licence that has nothing behind it. Its "no sound, no PMU" line in the fork's port README described a moment several cycles past and was contradicted by the PMU row twenty lines below it, and `partitions.csv` still called the `modules` partition UNPROVEN with nothing loading from it while a FLAC decoder plays a 5:58 track out of it. Two stray pipes were also splitting the Pictures and Tamacore rows of that table into phantom extra columns. The build section gained the two things that cost time this week: the IDF environment that `export.sh` cannot produce on this machine, because it fails on a missing `riscv32-esp-elf-gdb` and leaves `idf.py` off the path, and THE MODULE REBUILD ORDER, which is not a preference but a consequence of the MMU choosing both addresses at boot -- build the app, flash it, let it boot, `make modaddr`, `make flac`, flash -- with the failure it prevents spelled out as the bad-magic error on good files that entry 30 recorded. Every link, anchor and referenced path in both files was checked against the tree, all fourteen documents are listed with none orphaned and none absent, and the table column counts were verified after editing. THE RESTRICTED FILE IS THE REMAINING STALE DOCUMENT AND WAS NOT TOUCHED: `core.md`'s Purpose still reads "Escapement is a port of linux to the Lilygo T Watch Ultra", which is the same falsehood on the most authoritative page in the project, and it is left alone because `core.md` may only be edited when asked. That is raised with the operator rather than quietly fixed.

#### Next Steps:

THE PURPOSE LINE IN `core.md` IS THE ONE THING OWED and it needs the operator's word, not an agent's judgement, because editing that file is explicitly restricted; it should say that this is a TinyDesk port for the T-Watch Ultra, with the Linux port recorded as the abandoned first attempt. Beyond that the documentation has a gap the README now points at without filling: there is no document describing the port's architecture as it stands -- the core split between the desktop, the shell, the apps and the board half, plus where the flash partitions divide the job -- and `docs/cores.md` describes the Linux arrangement in enough detail that a reader could mistake it for current. Writing that is a cycle of its own and belongs beside the fork's port README rather than duplicating it. The Linux-era documents could each gain a one-line header saying they are archive, which `docs/README.md` currently does in one place instead of eleven; that is cheap and worth doing when one of them is next opened for any other reason. The fork's own top-level `README.md` is still upstream's, describing TinyDesk generally and not this project, and a fork is entitled to say what it changed. Finally, `core-log.md` stands at thirty-one entries against the hundred-entry ring, so no archive is due, but the pattern of entries 25 to 31 -- one structural change per cycle, each with a hardware measurement behind it -- is what the next cycle should keep.

#### Files Modified:

- README.md
- docs/README.md
- ports/twatchultra/README.md
- ports/twatchultra/partitions.csv

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: NOT RUN

---

## 30 COMMIT Unreleased 2026-11-16T16:57:59-07:00

#### Coming From:

Unreleased 791cc7b

#### Purpose:

Finish the exercise the modules partition was created for: delete the inline FLAC decoder and recover the app-image space it held, proving the module path can carry the only decoder rather than a spare one.

#### Outcome:

THE INLINE DECODER IS GONE AND `music` STILL PLAYS. One player, two names: `music` is the user-facing command and `modflac` survives as its alias, both dispatching to the loop that decodes through the `modules` partition. Deleting `music.c`, `musicdec.cpp`, `musicdec.h`, the `micro-flac` requirement and its `REQUIRES` entry took the app from 0x2ead40 to 0x2e3000 and free space in ota_1 from 21 KB to 53 KB -- 32 KB recovered against the 31.7 KB the component was measured to cost. THE TRACK THAT PROVES IT IS THE OPERATOR'S, not a tone: a 5:58 FLAC through `music` gave `state done, 15792504 frames, 0 errors, 0 short writes`, verdict clean, and the operator confirmed by ear that it sounded good. 15,792,504 frames is 358.1 s at 44.1 kHz, matching the track. heap came home: internal SRAM 20,707 bytes free DURING playback, 33,727 after, so the PSRAM buffers are genuinely released. A clean `make clean && make flac` reproduces the flashed blob byte-for-byte (sha256 dd47bce9), so the module build is deterministic. TWO BUGS CAME OUT OF USING IT, AND THE SECOND IS THE ONE WORTH REMEMBERING. The rate was hard-coded at 44100 in `play_task` while the module had already parsed the real value into its handle -- the fix is a single read of `w[2]`, and verified: `tone1000_48000` reports `rate 48000` and exactly 192000 frames, so a 48 kHz file no longer plays sharp by a semitone and a half. THE SECOND IS THE MAPPING MOVES WHEN THE APP IMAGE DOES. Removing the inline decoder shifted the instruction window 0x42ae0000 -> 0x42ad0000 and the data window 0x3cbe0000 -> 0x3cbd0000. `moddata.at` still held the old pair, the blob was linked for it, AND THE BLOB STILL EXECUTED -- entry reached by offset from whatever base is live -- then read its `.rodata` 64 KB off. The only symptom was `last -5`, `FLAC_DECODER_ERROR_BAD_MAGIC_NUMBER`, on two files that were perfectly good, because micro-flac compares the input against a MAGIC_BYTES table living in that misdirected `.rodata`. It looks exactly like a corrupt file and is not one. THE FIX IS IN THREE PARTS. The module header now carries `linked_inst` and `linked_data` (ABI 2), `modload_prepare()` compares them against the live mapping and refuses a mismatch instead of calling into it, and `modload_why()` retains the reason so the report is `module linked for inst 42ae0000 data 3cbd0000, mapped at 42ad0000 3cbd0000 -- rebuild it` rather than "no module mapped at boot", which was true and useless. That guard was verified by deliberately flashing a header claiming the old base: it refused, reported the exact mismatch, and decoded nothing. `patch_header.py` and `link.ld` now take BOTH bases from `moddata.at` instead of pinning the text base in three separate places, and `make modaddr` reproduces the typed addresses by reading `/root/modaddr.txt` out of the storage partition with esptool -- no F10, no Enter, no display mirror, which are the three things that have each cost a cycle here. Two smaller corrections: `modtest` needed the same `--only-section` split as the real blob, because `.rodata` links into the data window at a LOWER address than `.text` so a plain `objcopy -O binary` started the file at `.rodata` and the header was not at offset 0 (`bad magic 0x75646f6d` -- the letters of the word "module" from a string literal); and the status command reported `handle: magic 00000000 rate 0` for the whole duration of a working track, because the handle was only copied out when the task ended and the block was freed, so it now reads the live block while one is playing.

#### Next Steps:

THE SECOND CODEC IS NOW CHEAP, which was the point of all of this. The decoder occupies 26,685 bytes of the 1 MB `modules` partition and 1,976 of the 1 MB `moddata`, against 704 KB for all eleven Rockbox codecs, so the arithmetic says a full codec set fits with room to spare. Doing it means a second blob, a second `make` target and a loader that can hold more than one entry point -- `modload` currently maps one partition and reads one header, so a registry of blobs by name is the next structural piece rather than another decoder. The Ogg container is the obvious companion to FLAC, and `MICRO_FLAC_DISABLE_OGG=1` is already set for the FLAC build, so an Opus or Vorbis path would reuse the partition without touching what works. THREE SMALLER ITEMS STAND. The module hard-codes nothing about rate now, but `modcall_t` still does not return it -- `modflac` reads the handle's `rate` word directly, which works and is documented, but a module that is not this one would need the field. `__assert_func` routes through the API table and returns rather than halting, and that path has still only been reviewed, never exercised by a failing assert. And the app has no picker, no queue, no seek and no progress display: `music` takes a path and plays it, which is the smallest thing that answers "does the decoder reach the speaker" and is no longer the whole answer the project wants.

#### Files Modified:

- ports/twatchultra/main/modflac.c
- ports/twatchultra/main/modload.c
- ports/twatchultra/main/modload.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/music.c
- ports/twatchultra/main/musicdec.cpp
- ports/twatchultra/main/musicdec.h
- ports/twatchultra/modules/Makefile
- ports/twatchultra/modules/link.ld
- ports/twatchultra/modules/moddata.at
- ports/twatchultra/modules/patch_header.py
- ports/twatchultra/modules/read_modaddr.py
- ports/twatchultra/modules/src/flacmod.cpp
- ports/twatchultra/modules/src/modtest.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 29 COMMIT Unreleased 2026-11-16T16:08:24-07:00

#### Coming From:

Unreleased 6c38889

#### Purpose:

Finish the codec-module path so a FLAC decoder running out of the `modules` partition plays audio, proving that the partition can carry code and not merely execute a test.

#### Outcome:

IT PLAYS. The operator heard the 440 Hz tone on the watch's speaker, decoded by code running out of the `modules` partition -- fetched from flash by XIP, called across a partition boundary, never linked into the app. THE NUMBERS ARE EXACT: `state done, 352800 frames, 78 calls, verdict 0` with `handle: magic 464c4143 ready 1 rate 44100 ch 2 bits 16`. 352,800 frames is exactly 8 seconds at 44.1 kHz, so every sample was decoded; magic `464c4143` is "FLAC", which says the API table reached the module; rate, channel count and bit depth say STREAMINFO parsed correctly; and verdict 0 is a clean end of stream. THE LAST BLOCKER WAS A BUILD-SYSTEM BUG AND ITS SYMPTOM WAS SILENCE. A blob was linked against `0x3c27b8fc` while the firmware published `0x3c27ba74`, so the module's `host()` read garbage and every call returned MODCALL_ERROR -- with no crash, no log and no clue except a status line reading `verdict -1`. The cause is that a `-D` FLAG DOES NOT MAKE AN OBJECT DEPEND ON ITS VALUE: rebuilding the app changed the define, make saw unchanged prerequisites and reused the stale `flacmod.o`, and the blob was linked against an address the firmware no longer used. The fix is structural rather than a corrected number: `modules/gen_addr.py` reads `g_modhost_ptr` and `g_modbss` out of the app ELF into `build/modaddr.h`, every module object depends on that header, and a moved address therefore forces a rebuild. Verified by disassembly before flashing -- the linked entry loads `l32r a8, ... (3c27ba74)`, the address the firmware actually publishes. THE SPLIT INTO TWO PARTITIONS IS WHAT MADE `.rodata` POSSIBLE: `modules` at 0xE00000 for `.text` mapped as INSTRUCTIONS, `moddata` at 0xF00000 for `.rodata` mapped as DATA, because the two mmap windows are complementary and cannot serve the same bytes. The loader maps both at boot and publishes the addresses it was given in `/root/modaddr.txt`; `modules/moddata.at` carries them and the Makefile passes them as `--defsym` so `.rodata` links at whatever the MMU handed out rather than at a number typed in by hand. Three further mechanisms carried over from the work that found them: the lent bss `g_modbss`, which the module's `link.ld` claims for `.bss` because a library's function-local statics still need zeroed writable RAM and a blob cannot zero its own; `FLAC_MALLOC`/`FLAC_FREE` redirected to the host table through `flacmalloc.h`, `-include`d because the override is a NAME and every translation unit needs the declaration; and `abs`/`labs` plus both LPC assembly widths supplied by the shim and libgcc. THE HARNESS COST MORE THAN THE CODE AND THE RECIPE IS NOW KNOWN. Ctrl+Q is `0x11`, which is XON: with `IXON` set the line discipline swallows it as flow control and it never reaches the desktop, so "close the focused window" silently did nothing through dozens of attempts. With `IXON` cleared the sequence is simply reboot, wait for boot, F10, Enter, then type. Two further traps: the USB console goes silent after esptool and a fresh `chip_id --after hard_reset` restores it, and an orphaned helper holding `/dev/ttyACM1` makes every esptool invocation fail with a serial exception while the board looks fine -- `fuser` on the port named it immediately once it was suspected. Build PASS at 0x2eace0 bytes with micro-flac STILL LINKED INLINE for the `music` command, so nothing regressed and the 31.7 KB has not yet been recovered; that is the next step. Reference MOD-001 gained the corrections from this cycle.

#### Next Steps:

THE PRIZE IS NOW IN REACH: micro-flac is still compiled into the app for the `music` command, so deleting `musicdec.cpp`, the `micro-flac` component requirement and the `REQUIRES` entry should recover the 31.7 KB that prompted the whole exercise -- leaving `modflac` as the only FLAC path, and the app image with roughly 56 KB of headroom instead of 24 KB. Before deleting, `modflac` should be tested on a REAL track rather than a tone, because a tone exercises no joint-stereo, no variable block sizes and no wasted-bits handling, all of which the decoder path takes. The module's rate is currently hard-coded to 44100 at stream start because `modcall_t` does not yet report it back; a 48 kHz file would play at the wrong pitch, and adding the rate to the returned state is small. Beyond that the module mechanism can carry the rest of the codec set: all eleven Rockbox codecs are 704 KB against 1.8 MB of free space across the two partitions, and the 28 KB this decoder occupies shows the arithmetic works. Three smaller items stand. The `__assert_func` stub reports through the table and returns rather than halting, but that path has still only been reviewed, never exercised. The `modflac` diagnostic still writes to `/sd` rather than `/fs`, because a PSRAM-stack task may not touch littlefs -- that is a rule now, recorded in the reference, not a workaround. And `music` has no seek, queue, resume or app window, and the DAC path stays out of the player by the operator's instruction.

#### Files Modified:

- ports/twatchultra/modules/gen_addr.py
- ports/twatchultra/modules/Makefile
- ports/twatchultra/main/modflac.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 28 COMMIT Unreleased 2026-11-16T15:08:54-07:00

#### Coming From:

Unreleased 77d46ff

#### Purpose:

Move the FLAC decoder out of the app image and into the `modules` partition, to recover the 31.7 KB it costs inline on a board with 24 KB of headroom.

#### Outcome:

THE DECODER BUILDS AND RUNS AS A 28,660-BYTE MODULE AND IS CALLED SUCCESSFULLY, but it does not yet decode: it faults reading its own constant tables. THE COREDUMP IS THE EVIDENCE AND THE FINDING: fault `LoadStoreErrorCause` at `0x42ae6ff0`, which lies inside the module's own `.rodata` (`0x42ae683c` + `0x7b8`), with the trace `play_task` -> module entry -> `0x42ae231f` inside the blob. So the module's code EXECUTES and the boundary works, and what fails is a DATA READ of the module's own read-only data through the instruction mapping. THIS IS THE THIRD VARIANT OF ONE RULE AND IT COMPLETES THE PICTURE: the instruction window at 0x42000000..0x43ffffff serves instruction fetches and nothing else — a header load faulted at `0x42ae0014` in entry 26, and a CRC-table load faulted at `0x42ae6ff0` here, both `exccause 3`; and the data window at 0x3Cxxxxxx is its complement, serving data and faulting on instruction fetch (`exccause 2`, entry 27). THE TWO MAPPINGS ARE COMPLEMENTARY AND CANNOT COEXIST FOR THE SAME BYTES, so a codec blob needs TWO REGIONS — one mapped INST for code and one mapped DATA for `.rodata` — which means a second partition rather than one, a partition-table change and not a code change. That is the natural next step and it is cheap, because the pattern and the loader already exist. WHAT WAS BUILT AND IS KEPT. The module compiles from the vendored decoder with the encoder excluded and Ogg off, and links `-nostdlib` against `link.ld`. `FLAC_MALLOC` and `FLAC_FREE` are redirected to the host table through `flacmalloc.h`, because micro-flac's override is a NAME and every translation unit therefore needs the declaration — a plain `-D` left the decoder's own sources with an undeclared identifier. `abs()` and `labs()` are supplied by the shim because `lpc.cpp` calls them. Both LPC assembly widths are linked because the decoder references the 64-bit path whether or not a file needs it. A LENT BSS is new and general: real library code carries function-local statics — micro-flac's `get_metadata_blocks()` holds a static empty vector, 20 bytes plus a guard variable — and a blob cannot zero its own `.bss`, so the firmware publishes `g_modbss` (256 bytes), the module's `link.ld` places `.bss` there via `--defsym`, and `modload_zero_bss()` clears it before the first call; the address is derived from the ELF exactly as the API table's is. A SECOND COREDUMP FIXED A REAL TRAP: mapping the partition from a PSRAM-stack task panics with `assert failed: spi_flash_disable_interrupts_caches_and_other_cpu (esp_task_stack_is_sane_cache_disabled())`, because `esp_partition_mmap()` disables the flash cache and PSRAM is reached through that cache; `modload_prepare()` now maps and reads the header ONCE at boot from the boot task, whose stack is internal, and every PSRAM-stack task reads the cached result. That is the same rule entry 26 recorded about PSRAM stacks and flash APIs, arriving from a new direction. The app image was NOT reduced this cycle: micro-flac is still linked inline for the `music` command, so headroom fell to 22 KB rather than rising to 56 KB, and that stays true until the module path replaces the inline one. Build PASS at 0x2ea9b0 bytes.

#### Next Steps:

THE NEXT STEP IS A SECOND PARTITION, and the shape is already clear. A codec blob needs `.text` mapped as INSTRUCTIONS and `.rodata` mapped as DATA, at two addresses, in two regions; the module's linker script then places sections into both, and the loader maps both at boot from the boot task. That is one more entry in `partitions.csv`, one more `esp_partition_mmap(MAP_DATA)` call, and a `--defsym` for the data address, and the 9.87 MB `storage` or the 2 MB `modules` has room without touching the OTA slots. UNTIL THEN the inline micro-flac stays, so nothing regresses: `music` still plays, and `modflac` exists as the module path under test. Once decoding works through the module, deleting `musicdec.cpp` and the component requirement frees 31.7 KB, which is the point of the whole exercise. Three things to carry: the `__assert_func` stub in the shim reports through the table and returns rather than halting, so a decoder error surfaces as a return code — but that path has not been exercised, only reviewed; the module's entry signature grew a `state`/`state_size` pair so the caller lends the decoder object a home, and the decoder handle is padded to 32 bytes so the placement-new lands 8-byte aligned, which is worth checking if odd faults appear; and the diagnostic the module writes to `/fs/root/modflac.txt` did not appear on the last run, which is unexplained and should be understood before trusting any future module diagnostic — the crash happened first, so this is probably just the crash, but it is not proven.

#### Files Modified:

- ports/twatchultra/main/modflac.c
- ports/twatchultra/main/modflac.h
- ports/twatchultra/main/modload.c
- ports/twatchultra/main/modload.h
- ports/twatchultra/main/modhost.c
- ports/twatchultra/main/modhost.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/modules/Makefile
- ports/twatchultra/modules/link.ld
- ports/twatchultra/modules/src/modshim.c
- ports/twatchultra/modules/src/flacmalloc.h
- ports/twatchultra/modules/src/flacmod.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 27 COMMIT Unreleased 2026-11-16T14:28:35-07:00

#### Coming From:

Unreleased c901c46

#### Purpose:

Give a module in the `modules` partition a way to call back into the firmware, so a codec blob can allocate memory and report errors instead of being isolated in read-only flash.

#### Outcome:

A MODULE CAN NOW REACH THE FIRMWARE THROUGH AN API TABLE, and the round trip is proven on hardware: a 730-byte blob named `modapi` reads the table, calls `malloc`, `memset`, `memcpy` and `free` THROUGH it, writes into a caller-owned PCM buffer, and returns 42 when every step worked. The console output is the evidence: `'modules' 2097152 B mapped for execution at 0x42ae0000`, `blob: 'modapi' abi 1 size 730 entry +0x2c`, `modtest: calling 0x42ae002c(&call)`, `modtest: returned 42`. THE DESIGN IS ONE TABLE RATHER THAN BOUND SYMBOLS, and the reason is worth keeping: binding each function with `--defsym` is simpler, but every blob then breaks the moment the app is rebuilt, which defeats the purpose of a separately updatable codec partition. One table means ONE address has to be right instead of one per libc function. THE ADDRESS IS DERIVED AT BUILD TIME, NOT TYPED. A blob executes from read-only flash and cannot hold a pointer, so the firmware publishes `g_modhost_ptr` in its own RAM and `modules/Makefile` reads that symbol's ADDRESS out of the app's ELF with `nm`, passing it as `-DMODHOST_PTR_AT`; the blob dereferences it once. That this was not theoretical showed immediately: rebuilding the app moved the symbol from 0x3c27b630 to 0x3c27b650 and the blob build picked the new value up automatically. THE CONTRACT IS THAT A BLOB BELONGS TO THE FIRMWARE BUILD THAT PRODUCED IT, which is honest rather than unfortunate — and the `check` target fails loudly when the app has not been built or the symbol is missing, instead of linking a blob that dereferences garbage. FOUR BUILD FACTS COST TIME AND ARE NOW IN THE MAKEFILE WITH REASONS. First, the blob must be compiled HOSTED, not `-ffreestanding`: micro-flac's decoder includes `<vector>` and the libstdc++ headers refuse freestanding mode outright, so freestanding-ness is enforced where it belongs, at link time with `-nostdlib` and by the shim. Second, C and C++ flags must be separate, because gcc rejects `-std=gnu++17`, `-fno-rtti` and `-fno-threadsafe-statics` on C input and the build treats warnings as errors. Third, the shim CANNOT USE STATIC BUFFERS: the first `__assert_func` built its message in `static char msg[120]` and failed to link with "`.bss` referenced in section `.text`", because a blob has no writable bss — the partition is read-only flash and there is no loader to zero one — which is the same constraint the earlier `link.ld` already encoded by discarding `.data` and `.bss`. Fourth, `g_modhost_ptr` needs `__attribute__((used))` AND a real reference from the firmware, because `--gc-sections` collects a section nothing references, and a blob build needs the symbol's ADDRESS rather than its value. What this does NOT prove is a codec: the table carries only memory and logging, and a decoder additionally needs its buffers handed in and a way to report errors upward. The measurement from the previous cycle — micro-flac as a blob is 26,257 bytes of text and 20 bytes of bss — says the remaining work is plumbing rather than discovery. Build PASS at 0x2e9e60 bytes, flashed app-only to 0x10000 and the blob to 0xE00000, both at 115200 after a 460800 attempt failed to connect. Reference MOD-001 was added for the module mechanism.

#### Next Steps:

THE NEXT STEP IS A REAL DECODER IN THE PARTITION, and four things about it are now known. It must reach memory only through the table, because it cannot call libc; the shim already routes `malloc`, `free`, `operator new` and `operator delete`, so micro-flac should link with little more than its buffers being caller-supplied. It must hand back a status rather than assert, which the `modcall_t` shape provides and the `__assert_func` stub supports by reporting and returning instead of halting. Its 20 bytes of bss still have to be dealt with — the simplest honest route is to have the caller allocate that block and pass it in, rather than pinning a fixed address in the app's RAM, which would be another build-time constant to keep in step. And its link address must be re-measured after any partition-table change; `modules/link.ld` hard-codes 0x42ae0000, which `modtest` reports. The immediate goal is to free the 31.7 KB micro-flac costs inline in the app, where only 24 KB of headroom remains. Three smaller items stand: the API table has no allocator-with-caps entry beyond `malloc_prefer_psram`, so a decoder wanting internal RAM specifically would need one; there is no version negotiation beyond a single `abi_version` integer, which is enough now and will not be if the table grows; and `music` still plays only inline and knows nothing about modules, so switching it over is the integration point. The DAC path stays out of the player by the operator's instruction, and the `music` command lacks seek, queue, resume and an app window.

#### Files Modified:

- ports/twatchultra/main/modhost.c
- ports/twatchultra/main/modhost.h
- ports/twatchultra/main/modload.c
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/modules/Makefile
- ports/twatchultra/modules/src/modshim.c
- ports/twatchultra/modules/src/modtest.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 26 COMMIT Unreleased 2026-11-16T13:50:14-07:00

#### Coming From:

Unreleased ff8cb49

#### Purpose:

Prove that code stored in the `modules` flash partition can be mapped as instructions and called, before any codec is ported to live there.

#### Outcome:

IT WORKS: a 77-byte blob at 0xE00000 is mapped with `esp_partition_mmap(ESP_PARTITION_MMAP_INST)` at **0x42ae0000**, called through a function pointer, and returns **42** for 6 x 7 -- which proves the arguments arrived across the boundary, a loop ran, an internal function call inside the blob survived, and the heap figures the caller passed were read correctly. The load costs ZERO internal SRAM, which is the whole reason the design exists. The result record read back from flash is `STLR`, inst 0x42ae0000, entry 0x42ae0028, called 1, heap free 58531 largest 31744, returned 42. TWO COREDUMPS TAUGHT THE DESIGN AND BOTH WOULD HAVE BEEN GUESSED WRONG. First, reading the blob header THROUGH the instruction-mapped pointer faults: `exccause 3 (LoadStoreErrorCause), excvaddr 0x42ae0014`, in `memcpy` reached from printf formatting the header's name string, with the magic word at offset 0 reading fine and the string at +0x10 not. Flash instructions live on the instruction bus at 0x42000000..0x43ffffff and a data load there is not a supported access, however it happens to behave in a given case. Second, mapping the SAME partition TWICE -- once DATA, once INST -- does not give two windows: the second mapping came back at `0x3cae0000`, which is PSRAM address space, and the call faulted with `exccause 2 (InstructionFetchErrorCause), excvaddr 0x3cae0028`. The design is therefore exactly ONE mapping as INSTRUCTIONS for execution only, with the header read by `esp_partition_read()` into a stack struct -- a normal flash read through the data path that cannot fault on the instruction bus. THE REPORTING PATH FAILED TWICE FOR UNRELATED REASONS AND IS WORTH RECORDING because it cost most of the cycle's time. The display mirror sends only CHANGED cells, so a one-line result is unreadable without a forced repaint, and a restored Log Viewer window holds focus at boot and eats the keystrokes meant for the Terminal; and `esp_partition_write()` refuses a partition that is currently mapped, so writing the verdict into `modules` itself needs the unmap first. The result now goes both to a flash sector and to a littlefs file, and a boot-time diagnostic (`MODTEST_AT_BOOT`) runs the experiment once so no console is in the loop at all. An arithmetic slip also cost a round: the result sector is at partition offset 0x1FF000, so absolute 0xFFF000, and it was read at 0xEFF000 twice before that was noticed. THE BLOB IS SMALL ON PURPOSE: `modules/src/modtest.c` touches no libc, has no initialised global, and calls nothing outside itself, because each of those would need a relocation the blob cannot resolve, and the point was to fail for the interesting reason if it failed at all. `modules/Makefile` builds it `-nostdlib` against `modules/link.ld`, which places it at the MEASURED address and discards `.data` and `.bss` because a blob has no loader to populate them; `patch_header.py` fills in size and entry offset from the linked image so a stale offset cannot jump into the middle of an instruction. THE ADDRESS IS A BUILD-TIME CONSTANT THAT MUST BE RE-MEASURED if the partition table moves, and the partition now holds both the experiment and its own result, so a codec blob must not be placed at the 0x1FF000 sector. Build PASS at 0x2e9f30 bytes, flashed app-only to 0x10000 with `Hash of data verified` at 115200 after a 460800 attempt reported "the chip stopped responding", and the result read back with esptool rather than through any console.

#### Next Steps:

THE ROOM IS NOW PROVEN AND EMPTY, so the next task is to put something real in it: a second codec, most likely an MP3 decoder from Rockbox's set, built as a module rather than linked into the app. Three things about that are now known and constrain it. A blob must not use libc, initialised globals, or any external symbol, so a decoder written against its own allocator and file API will need a shim layer inside the blob and a way to hand it a buffer from the caller -- the experiment passed a heap figure, but a real blob needs the buffer itself. The link address must be re-measured after any partition-table change, and `modules/link.ld` hard-codes what `modtest` reports. And the result sector at partition offset 0x1FF000 is now used by the diagnostic, so blob placement must avoid it or the diagnostic must move. The boot-time diagnostic (`MODTEST_AT_BOOT` in the port's CMakeLists) should be removed once it stops being useful, since it runs an experiment on every boot; it is kept for now because it makes the test reproducible without any console. Four smaller items stand: the `music` command still has no seek, queue, resume or app window; `dac_wake()` remains a mitigation rather than an understanding; the DAC path stays out of the player by the operator's instruction; and the modules partition holds 2 MB of which all eleven Rockbox codecs would use 704 KB, so there is room for the whole set if the port succeeds.

#### Files Modified:

- ports/twatchultra/main/modload.c
- ports/twatchultra/main/modload.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/modules/Makefile
- ports/twatchultra/modules/link.ld
- ports/twatchultra/modules/patch_header.py
- ports/twatchultra/modules/src/modtest.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 25 COMMIT Unreleased 2026-11-16T13:24:38-07:00

#### Coming From:

Unreleased 14398a9

#### Purpose:

Take a full backup of the working FLAC-player firmware, then add a partition for codec blobs that costs no RAM, funded by shrinking the littlefs partition without losing a byte.

#### Outcome:

THE FLASH NOW CARRIES A 2 MB RAW `modules` PARTITION AT 0xE00000 and `storage` fell from 9.88 MB to 7.87 MB to fund it; both 3 MB OTA slots are untouched, and every file that was in the littlefs partition is still there, verified byte-for-byte. TWO BACKUPS WERE TAKEN, because the first became obsolete the moment the layout changed. `music-2026-11-16-1259.bin` captures the pre-change layout, 16,777,216 bytes, sha256 `7a308611b75adaad6546c833282b18b7feefb20d9b63a0d33774bc725d6c0a45`, with its app region verified byte-identical to the build then on the board. `modules-2026-11-16-1320.bin` captures the new layout, sha256 `34a9d33688e18e0940bf2f0ef7642143f8e0b871f3d3a2a040cbc4e86064262b`, checked with `sha256sum -c`, and its app region at 0x10000 verified byte-identical to the build at `7af3e9c2aa9cae89`. That digest moved from `40aa57945e448dcd` when the app was rebuilt for the new partition table, which changed only the app descriptor's build time -- the phenomenon entry 20 recorded, and the reason a digest read before a rebuild must not be compared against one read after. THE MIGRATION IS THE PART WORTH CARRYING, because the first measurement was wrong in a way that would have destroyed data. Counting non-0xFF bytes in the storage partition reported 4,303,700 bytes across 2,429 of 2,528 blocks, with 1,151,524 of them ABOVE the new 7.87 MB boundary, which reads as "shrinking this loses 1.1 MB". Mounting the partition with littlefs-python 0.19.0 gave the truth: 25 files and 11 directories, 22,244 bytes, 34 blocks in use. The rest was stale blocks awaiting garbage collection. A raw byte census is not a measure of what a filesystem holds; only a mount is. So the migration was done by extraction rather than by truncation: all 25 files were read out, a fresh littlefs image was built at 0x7E0000 and populated, and that image was flashed. Four files including `etc/board.conf` compared byte-identical afterwards, and the board came up with its board keys intact (`usb.host = 0`, `usb.dac = 1`, `bt.enable = 1` and the rest), its shell history, its Apps, its Start-menu `.tdlink` files and its Desktop. The music player still decodes the same 352800-frame 44.1 kHz tone with 0 errors and 0 short writes after the change, which is the evidence that the app, the filesystem and the SD card all still agree. THE PARTITION ITSELF IS UNPROVEN AND IS RECORDED AS SUCH. Xtensa executes from flash natively -- every instruction ESP-IDF runs is XIP-mapped -- so a blob at 0xE00000 can be mapped with `esp_partition_mmap()` as instructions and called, costing no internal SRAM, which is the resource this board lacks. All eleven Rockbox codecs are 704 KB against the partition's 2 MB. But nothing loads from it yet: a blob must be linked position-independently for that address and callable across the Xtensa windowed ABI, and neither has been demonstrated. The region was read back and confirmed all 0xFF. Subtype 0x40 is the first application-defined data subtype, chosen because ESP-IDF reserves 0x00-0x3F. The live table was read back from flash and decoded to confirm it, rather than trusted from the build directory. Reference FLS-004 was added with the layout, the address, the mmap reasoning and the mount-not-census rule. The required core-syntax audit was run before commit and passed, with `.ai/core.md` unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched.

#### Next Steps:

THE MODULES PARTITION IS A ROOM WITH NOTHING IN IT, and proving it works is the next task: build one small position-independent blob, map it with `esp_partition_mmap()`, and call it, before any codec is linked for it. If the Xtensa windowed ABI turns out to be unusable for cross-partition calls the design is dead and the room should be given back to `storage`, so the experiment should be cheap and early rather than after a codec port. The alternative still on the table is trimming the app image instead -- MicroPython, wolfSSH and wolfSSL dominate it -- which would need no new mechanism at all. Three smaller items stand. The `music` command plays a file and nothing else: no seek, no queue, no resume, no Ogg-FLAC test, and no app window, which was deliberate because the smallest thing that answers "does a decoded sample reach the speaker" was the goal. `dac_wake()` is a mitigation, not an understanding, and the DAC path stays out of the player by the operator's instruction. And the storage partition is now 7.87 MB holding 136 KB, so the space freed is real; if a future codec needs more than 2 MB the modules partition can grow at storage's expense without touching the code.

#### Files Modified:

- ports/twatchultra/partitions.csv

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 24 COMMIT Unreleased 2026-11-16T12:44:22-07:00

#### Coming From:

Unreleased 800f32c

#### Purpose:

Give the watch a music player that decodes FLAC on its own CPU and plays it through the built-in speaker, choosing the decoder on measured memory cost rather than reputation.

#### Outcome:

FLAC PLAYS ON THE WATCH FROM THE SD CARD OUT OF THE BUILT-IN SPEAKER, decoded on the S3 itself, with 0 errors and 0 short writes; the operator heard a 5:58 44.1 kHz stereo track through the MAX98357A. The decoder chosen is esphome-libs/micro-flac v0.3.0 (Apache-2.0), vendored unmodified in `ports/twatchultra/components/micro-flac` with its micro-ogg-demuxer submodule (MIT) and a `README.vendored.md` recording provenance. THE DECISION WAS MADE BY MEASURING THREE CANDIDATES, AND MEMORY WAS THE DECIDING AXIS. Rockbox's flac.c reserves 7 x MAX_BLOCKSIZE x 4 bytes of channel buffer -- 126 KB at its 4608 default and 229 KB at 8192 -- as STATIC IBSS_ATTR arrays because it supports multichannel FLAC, plus a 64 KB MAX_FRAMESIZE input buffer, which is about 190 KB of internal SRAM held regardless of the file; this board has roughly 18 KB free with the desktop running, so Rockbox cannot run here at all without patching the statics out of its own GPL sources. dr_flac is 41 KB of relocatable heap and free of any host layer, but it is generic desktop C with no ESP32 tuning and no project found using it on this platform. micro-flac sizes ONE working buffer from the file's own STREAMINFO (32 KB for a 4096-block stereo track), allocates it through heap_caps_malloc_prefer with PSRAM first, and ships Xtensa assembly for the LPC prediction restore loop. The ecosystem check is the part worth carrying: the 1,730-star standard for this task, schreibfaul1/ESP32-audioI2S, is GPL-3.0 and uses its own purpose-built ESP32 decoders with a documented 24576-byte block limit and 1-2 channel restriction, so the established practice on this chip is to write a decoder for the chip rather than to port a desktop one. THE LICENCE QUESTION RESOLVED THE OTHER WAY FROM WHAT WAS ASSUMED, and the correction is recorded because it changes what is available later: this firmware ALREADY ships wolfSSH and wolfSSL under GPLv3 and is therefore distributed as GPLv3 as a whole, so Rockbox's GPL-2.0-or-later would have cost nothing legally and the argument against it had to be made on memory alone. Measured on the board, micro-flac costs 31.7 KB of flash -- obtained by building the real decode path for xtensa-esp32s3 and diffing the image against an empty app, not by reading its README -- while its own archive is 76 KB because it contains the encoder, which link-time garbage collection drops entirely. WHAT WAS BUILT. `sound.c` gained `sound_stream_write_stereo_wait()`: the existing `sound_stream_write()` drops samples on a full ring by design because its caller is an emulator frame loop with a millisecond of slack, and a player is the opposite case, so the new call blocks on the I2S DMA, which paces the decoder to the output rate for free and lets the ring absorb SD stalls. `musicdec.cpp` confines the whole of micro-flac's C++ surface behind the C API in `musicdec.h`, because the port is C and the decoder is touched only through its documented streaming API. `music.c` is the player task: read a 16 KB window, decode what it yields, write the PCM, carry the partial frame to the front of the next window; it runs on a PSRAM stack and never touches flash. Verified in numbers: 352800 frames for an 8 s 44.1 kHz tone and 192000 for a 4 s 48 kHz one, both exact; a 5:58 track at 0 errors and 0 short writes with the counter advancing 548352 frames over an interval that matched wall-clock time; and internal RAM back to its starting figure after the task ends, so the allocate-free cycle does not leak. The build was 0x2e9450 bytes, 63,472 more than the baseline, leaving 27.5 KB of OTA headroom -- enough for no further decoder of this size, which is why the `modules` partition design is the next structural question rather than another inline component. The SD card was seeded with the two test tones in `/sd/media/audio/` and 74 real FLAC files totalling 1.8 GB in `/sd/ROCKBOX/Audio/`, a path reserved for the wider codec set. Build PASS, flashed app-only to 0x10000 with `Hash of data verified`; no backup was taken this cycle because the partition layout is unchanged and the baseline image of entry 23 still restores the previous app. The required core-syntax audit was run before commit and passed, with `.ai/core.md` unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched.

#### Next Steps:

THE NEXT STRUCTURAL QUESTION IS WHERE A THIRD DECODER WOULD LIVE, and the answer is not the app image: 27.5 KB of OTA headroom remains and micro-flac alone cost 31.7 KB, so a second codec of that size does not fit. The `modules` partition design recorded in this cycle's planning is the candidate -- a raw flash region memory-mapped as instructions through esp_partition_mmap(), which costs zero RAM because Xtensa executes from flash, with `storage` shrunk from 9.88 MB to fund it while the two 3 MB OTA slots stay as they are. That design is unproven here: the ABI match between Rockbox-style codec blobs and the Xtensa windowed ABI is the real research risk, and nothing about it has been measured. The cheaper step before it is to measure how much of the app image is actually needed, since 27.5 KB is thin for any further work at all. Three smaller items stand. `music` is a shell command with no app window, deliberately, because the smallest thing that answers "does a decoded sample reach the speaker" was the goal; a Start-menu player needs a picker, a progress line and a stop button. The player has no seek, no resume and no queue, and it refuses Ogg-FLAC in practice pending a test of the path that micro-flac enables but which has never been exercised. And the DAC remains out of scope by the operator's instruction: the built-in speaker is proven, and `uac2` playback stays as entry 23 left it.

#### Files Modified:

- ports/twatchultra/main/music.c
- ports/twatchultra/main/music.h
- ports/twatchultra/main/musicdec.cpp
- ports/twatchultra/main/musicdec.h
- ports/twatchultra/main/sound.c
- ports/twatchultra/main/sound.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/components/micro-flac
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 23 COMMIT Unreleased 2026-11-16T11:31:12-07:00

#### Coming From:

Unreleased d55b06d

#### Purpose:

Replace the DAC's unusable hardware volume with a sample-scaling control whose taper follows how loudness is actually heard, and establish a backed-up baseline once the operator accepted it.

#### Outcome:

THE VOLUME IS NOW OURS, IT IS UNIFORM IN dB, AND WAV PLAYBACK OVER THE USB-C DAC IS QUALIFIED AS THE PROJECT BASELINE. The DAC's own Feature Unit was already proven to misreport itself in entry 22; this cycle measured it across its whole range before removing it: 10 percent reads -20.00 dB, 25 percent -12.04, 50 percent -6.01 and 100 percent 0.00, every value echoed exactly by GET_CUR, while the output did not follow and nothing changed audibly until past the halfway point. So the Feature Unit is now held at 0 dB and unmuted once at open and again after every dac_wake(), and all attenuation happens in the samples through s_gain_q15. THE TAPER TOOK THREE ATTEMPTS AND THE REJECTIONS ARE THE RESULT. The first was the psychoacoustic law, gain = (pct/100)^1.66096, putting half-loudness (-10 dB) at 50 percent of travel; measured at the DAC's line output it was correct to 0.02 dB across a five-point sweep (100, 75, 50, 25 and 10 percent reading -28.61, -32.76, -38.61, -48.59 and -61.82 dBFS against predicted values within 0.02 of each), and the operator rejected it by ear as inverted, steep at the bottom and dead at the top. That reading was right and is a statement about SHAPE, not accuracy: a power law is exact at one point of the travel and wrong either side of it, spending 10 of its 33 dB in the first tenth and 1.7 dB in the last. VLC was then taken as the trusted reference and read from source rather than memory: modules/audio_output/volume.h cubes the GAIN with a comment requiring every volume-capable plugin to match it, and its Qt dial is not a 0..1 control, because qt-max-volume defaults to 125 with a range of 60..300 and the plain slider sets maximum 200, so unity sits at 80 percent of travel and the last fifth is overdrive. That produced two useful facts and one hard constraint: VLC and a linear-in-dB curve agree at the midpoint, -18.1 dB against -18.00 dB, which is the figure the operator remembered from consumer gear; cubic is MORE front-loaded than the rejected power law; and the overdrive cannot be copied here, because this board's 100 percent is already digital full scale so a boost would clip in the samples, leaving only the bottom-heavy sub-unity portion of VLC's law usable. The shipped law is therefore linear in dB over 36 dB, gain = 10^(dB/20) with dB = -36 * (1 - pct/100), and it was accepted by ear on a live stream stepped 100 to 75 to 50 to 25 to 10 percent while a track played, reporting even 9 dB per quarter-turn with no glitch, gap or click on any change. Verified in the firmware's arithmetic: 3.60 dB per 10 percent of travel at every step, strictly monotonic over 1..100, 50 percent at exactly -18.00 dB, and unity at 100 percent with the multiply skipped entirely. The play task is also the last one on an internal stack and now takes its 4 KB from PSRAM, matching usb_lib, the uac2 client and the uac2 worker, which is 16 KB of internal SRAM returned across this cycle and the last. TWO INSTRUMENT LESSONS COST TIME AND ARE RECORDED IN THE REFERENCE. The PC's capture chain has a +20 dB Line Boost and an AGC; at their defaults the codec limited the signal, every reading above about 70 percent flattened to the same level, and the resulting false 'the curve is wrong at the top' conclusion sent the work in the wrong direction until the boost was zeroed and the capture gain set to -15 dB, after which the noise floor dropped to -91 dBFS and the readings became trustworthy. And a BLE session left holding a link across a watch reset reports 'Service Discovery has not been performed yet' for every command while the board is healthy, so the session must be torn down and reopened rather than reused. Sixteen workarounds and dead ends are not repeated here because the surviving design is recorded in the code comments beside the reason for each rejection. Build PASS at 0x2e1460 bytes with 2 percent of the app partition free, sha256 `abaa8528e1b35c5d`, flashed app-only to 0x10000 with `Hash of data verified`. A full 16 MiB backup was then taken as `baseline-2026-11-16.bin` in the operator's backup directory, 16,777,216 bytes, sha256 `1ee127d64d06e4c8112edfaa1926b8c4005984e1ca965bf7beccbafd01c46818`, checked with `sha256sum -c`, and confirmed to hold the flashed app: `ota_0` inside it, read back out at offset 0x10000 for 0x2e1460 bytes, hashes to `abaa8528e1b35c5d`. The operator accepted the audio by ear and declared wav playback over the USB-C DAC qualified, which is what this baseline records. Reference AUD-001 was added with a new AUD topic and its catalog, routing and index entries, holding the three competing taper laws, VLC's cubic and overdrive, and the capture-chain trap.

#### Next Steps:

CYCLE 2'S OTHER HALF IS ROUTING, which this cycle did not touch: the emulators and `uac2 play` both feed one sink and there is no selector between them, so a track and a running core would compete for the same ring. Cycle 3 remains the emulators' resampler, since gnuboy emits 43,691 Hz and `uac2` refuses a rate the clock's RANGE does not list rather than converting it. Three smaller items stand and none is blocking: the build now takes its task stacks from PSRAM, so any future task added to this port should state why it may or may not do the same, and none of those tasks may call a flash or OTA API because the cache is disabled during those operations; attenuating in the samples costs resolution at the bottom of the travel, 10 percent giving gain 0.063 so a 16-bit full-scale sample becomes 2069 of 32767, about 11 effective bits, which is a noise-floor matter and not a loudness one, and is the reason the DAC's S24 (alt 2) and S32 (alt 3) alternates would be wanted before pushing the range past 36 dB; and the DAC's output stage still latches a state the host cannot clear, with dac_wake() reproducing the lever a headphone replug pulls rather than establishing what the dongle latches. The BLE shell input path still drops multi-byte writes, and `bleuart fetch` is still in the build and still untested now that TOL-004's write path is correct.

#### Files Modified:

- ports/twatchultra/main/uac2.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 22 COMMIT Unreleased 2026-11-16T09:53:32-07:00

#### Coming From:

Unreleased 66cac80

#### Purpose:

Make audio actually play on the USB-C DAC in OTG host mode: fix the boot loop that returned when the DAC was attached, and fix the silent first stream.

#### Outcome:

AUDIO PLAYS ON THE DAC IN OTG MODE, ON THE FIRST STREAM AFTER A PLUG, WITH NO HEADPHONE REPLUG, and the boot loop is gone; the operator confirmed sound on headphones with `uac2 play` reporting `frames` climbing, `short=0`, `underruns=0`, `errors=0` at 44.1 kHz. THE ROOT CAUSE WAS THE INTERNAL DMA POOL, NOT THE ORDERING OR RESERVATION OF ANY ALLOCATION. Claiming the streaming interface needs two 512-byte, 512-ALIGNED descriptor lists and the small `calloc`s IDF interleaves between them (pipe_t 96 bytes, the channel object and block headers 20 each, measured from DWARF), so each list must skip up to 511 bytes for alignment and a single contiguous region of about 1.6 KB is required -- not two 512-byte holes. In OTG mode the pool could not supply that: the coredump showed `assert failed: transfer_descriptor_list_alloc hcd_dwc.c:1059 (ret == ESP_OK)` and the refusal that preceded it read `dma free=5215 largest=2432`, plenty of bytes and no region of that shape. Three separate attempts to work around it failed ON HARDWARE and are recorded so they are not repeated: allocating the stream's transfers before the claim failed because they consumed the pool; a reserve freed immediately before the claim failed because the claim's own two `calloc()`s took the freed hole first, leaving nothing aligned; the same reserve held across the claim wasted 1024 bytes the claim could not use; and a 2048-byte block taken at boot STARVED ENUMERATION, which the coredump named as task `usb_lib` at `usb_lib -> usb_host_lib_handle_events -> hub_process -> root_port_handle_events -> usbh_devs_add -> device_alloc -> hcd_pipe_alloc -> transfer_descriptor_list_alloc`, proving the shortage is general to the pool rather than particular to the claim. THE FIX IS TO STOP SPENDING THE POOL: `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` is already enabled for this board, so `usb_lib`, the uac2 client task and the uac2 worker task are now created with `xTaskCreateWithCaps(MALLOC_CAP_SPIRAM)`, returning 12 KB of internal RAM. Measured on the same board in the same OTG mode: the pool read `dma=6359/4096 int=13887/7168` before and `dma=18675/10752 int=26203/10752` after, and the claim then succeeded with `last open: dma free=17543 largest=8704`. None of those three tasks touches flash, which is the constraint a PSRAM stack carries, and OTA runs on the shell's internal-stack task. With the pool fixed, every workaround was deleted: there is no reserve, no boot hold and no special ordering, only a check that refuses with a logged reason rather than asserting when the pool is ever too small, and a bounded retry of 3 attempts on a 2 s step so a fault retrying cannot fix can never become a loop. THE SILENT FIRST STREAM IS A SECOND, UNRELATED DEFECT AND IT IS NOW FIXED TOO. `uac2 fu set` reports `ctrl 21/01 status 4` -- USB_TRANSFER_STATUS_STALL -- so the DAC REFUSES the Feature Unit's MUTE SET_CUR and the host cannot unmute it; a headphone replug worked because it is a jack-detect event inside the dongle whose own firmware re-arms the output stage. A SET_INTERFACE round trip reproduces that lever: `uac2_start_rate()` now calls `dac_wake()`, which selects alternate 0, returns to the streaming alternate and re-asserts the volume, and `uac2 wake` exposes the same recovery by command. THE CYCLE ALSO CLOSED THE HALF-OPEN STATE that entry 21 left: `s_opened` (set only at the successful end) now defines an open, `open_fail()` releases the claim, closes the device and clears the state on every failure after the device is opened, transfers and the PSRAM ring are kept because freeing transfers on a teardown path is entry 18's panic, an open mutex serialises the worker against a typed `uac2 open`, the ring drain in `uac2_start_rate()` returns ESP_ERR_INVALID_STATE through `note_start_err` unless `s_opened && s_claimed && s_ring`, and the automatic open waits on `board_shell_task_ready()` -- called after `xTaskCreate` has taken the shell's stack -- rather than on the hold's release, which would race the shell for the freed block. Build PASS at 0x2e0f20 bytes with 2 percent of the app partition free, sha256 `d2231d74ddb02044`, flashed app-only to 0x10000 with `Hash of data verified`. Two instrument lessons are worth carrying: the ELF of a crashed image must be copied out BEFORE the next build or its coredump cannot be decoded because symbol addresses move, and after the watch resets, a BLE client left holding the old link reports failures for every command while the board is healthy, so the session must be torn down and reopened rather than reused. Reference USB-005 and USB-006 were both revised from this cycle, USB-006 from INFERRED to VERIFIED for the refusal and the recovery. The required core-syntax audit was run before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched, this entry carrying all six sections in template order and Status values from the permitted set.

#### Next Steps:

CYCLE 1 IS COMPLETE AND CYCLE 2 IS ROUTING AND A GLOBAL VOLUME. `uac2 vol` currently writes the DAC's own Feature Unit Volume, which is what USB-006 says is not a usable control on headphones, so volume should move back to scaling samples through `sound_stream_*` with the DAC held at 0 dB, in fixed decibel steps that match hearing, confirmed by ear and by a line-in capture with the PC input near 5 percent. Routing is the other half: the emulators and `uac2 play` both feed one sink and there is no selector. Three smaller items stand. The DAC's output stage latches a state the host cannot clear and `dac_wake()` is a mitigation reproducing the lever a headphone replug pulls, not an established understanding of what the dongle latches, so the next time a first stream is silent check whether the alternate-setting cycle ran rather than assuming it did. The BLE shell input path still drops multi-byte writes, which needs its own cycle. And `bleuart fetch` is still in the build and still untested now that TOL-004's write path is correct. Cycle 3 remains the emulators' resampler, because gnuboy emits 43,691 Hz and the DAC takes discrete rates -- `uac2` refuses a rate the clock's RANGE does not list rather than converting it, which is correct and is why the resampler is needed.

#### Files Modified:

- ports/twatchultra/main/uac2.c
- ports/twatchultra/main/usbhost.c
- ports/twatchultra/main/usbhost.h
- ports/esp_idf/app/tdsh_bridge_esp.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 21 COMMIT Unreleased 2026-11-15T22:43:12-07:00

#### Coming From:

Unreleased fadfe11

#### Purpose:

Fix the reset loop that began when a USB-C DAC was plugged into the watch in OTG host mode, so the board boots and stays up with the DAC attached.

#### Outcome:

THE BOOT LOOP WAS OURS AND IS FIXED, PROVEN BY COREDUMP AND CONFIRMED ON HARDWARE, through fork commit `1f8edc2` on `t-watch-ultra`. Plugging the DAC in reset the board every few seconds, with the desktop briefly visible each time, and only unplugging it stopped the cycle -- a physical escape, because the reset lands before any command can run. The coredump partition named it in task `usb_lib`, decoded with `esp-coredump` against `tinydesk.elf`: `assert failed: transfer_descriptor_list_alloc hcd_dwc.c:1059 (ret == ESP_OK)`, reached by `hub.c root_port_handle_events` into `usbh_devs_add`, `device_alloc`, `hcd_pipe_alloc`, `buffer_block_alloc`. The fault is in ENUMERATION, not in the interface claim that entry 18 and reference USB-005 describe, and enumeration's descriptor list is only 24 bytes, so it failed for exhaustion and not for fragmentation: `uac2_auto_start()` was taking three 1536-byte transfers and a 2 x 512 reserve, 5632 bytes of DMA-capable internal RAM, at boot before any device had enumerated, while `usbhost.c` was also holding 25600 bytes for the shell's stack. The allocation now happens in `uac2_open()` -- once a device is present, after enumeration and still before any file is opened on the SD card, which is the constraint USB-005 actually needs -- and the unplug path no longer re-takes the reserve, since holding it across a re-enumeration is the same fault one step later. The boot heap log became `pre-USB dma=free/largest int=free/largest`, DMA first and 36 columns, because the Log Viewer keeps 128 lines and cuts each at 38 and the previous wording lost exactly these figures 3 s into boot. Build PASS at 0x2e07a0 bytes with 2 percent of the app partition free, sha256 `d69daa80dc4d22ba`, flashed app-only to 0x10000 with `Hash of data verified`, and the operator confirmed the board boots with the DAC attached; uptime was then read three times over BLE at 1:40 and climbing, which is what distinguishes a fixed boot from a loop. In OTG mode the shell comes up, internal RAM reads free 12323, largest 7168, minimum-free 4039, and the DAC enumerates as `dev=1 clock=9 iface=1 alt=1 ep=0x01 mps=384`. TWO DEFECTS REMAIN AND NEITHER IS THE BOOT LOOP. Audio still does not play. The automatic open at boot fails with `transfer alloc: ESP_ERR_NO_MEM` at 3338 ms because it runs while the shell's 25600-byte hold is still taken, while the same `uac2 open` typed from the shell afterwards succeeds, so that is an ordering fault and not a capacity one. And `uac2 play /sd/media/audio/clip10.wav` RESETS the board: the coredump names `assert failed: xRingbufferReceiveUpTo ringbuf.c:1170 (pxRingbuffer && pxItemSize)` in task `uac2play`, at `uac2.c:1260` called from `uac2_start_rate(44100)` in `play_task`, with the ring handle NULL because the failed boot-time open had already set `s_dev` and returned before allocating transfers or creating the ring, after which `uac2_open()`'s `if (s_dev) return true` reported `open ok` without completing the open. Over BLE, multi-byte writes to the shell are dropped while single bytes arrive, so remote shell work is one byte at a time; escape sequences sent as one burst do reach the desktop, so the loss is in the shell's input path and is distinct from TOL-004's write side. Entry 20's account of the board was wrong and is superseded here: the image read back before this cycle's flash, archived as `app-2026-11-15-2158-5814b472.bin` with sha256 `5814b472cea69ab2e18eccf26cc16711d97f8f4507ee81a9f5f62778b47fb3b1`, already contained the shell-hole fix, since it carries both `shell hole released` and the new `cannot create the shell task (%u-byte stack; internal largest %u)` string, and a naive byte comparison of it against the committed build reports 71 percent difference from a roughly 232-byte layout shift rather than from different source. A full 16 MiB backup was taken as `working-2026-11-15-2231.bin`, 16,777,216 bytes, sha256 `fa5906277247b91ea3e70f8aeb53b5f4e7553f04e460169fcfee435765873166`, checked with `sha256sum -c` from its own directory and confirmed to hold the flashed app: `ota_0` inside it is byte-identical to the build at `d69daa80dc4d22ba`. `esptool verify_flash` against the chip then reported `verify FAILED (digest mismatch)`, and the cause is understood rather than hand-waved: a second full read differs from the backup in exactly two 64 KiB blocks, 0x620000 and 0xfb0000, both inside the littlefs `storage` partition which the running firmware writes, while the app and coredump partitions are identical in both reads, so the backup is a faithful image of its moment and the mismatch is live filesystem churn between the read and the verify. The required core-syntax audit was run before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched, this entry carrying all six sections in template order, prose in Purpose, Outcome and Next Steps, and Status values from the permitted set; no reference record changed, because the failures are this project's own code paths rather than new external facts.

#### Next Steps:

AUDIO IS STILL BROKEN AND THE NEXT CYCLE IS THE HALF-OPEN STATE, which is one coherent fix with two parts. First, `uac2_open()` must not treat a set `s_dev` as a completed open: either clear `s_dev` when the open fails after setting it, or track the opened state separately, so a second call cannot report `open ok` with no transfers and no ring. Second, the ring drain in `uac2_start_rate` must not assume `s_ring` exists -- an assert inside IDF is a reset, and this one was reachable from an ordinary `uac2 play`. Until both land, do not run `uac2 play` on a boot whose automatic open failed, because it reboots the board. After that, the boot-time ordering fault: the automatic open runs while the shell's 25600-byte hold is still taken and fails at `transfer alloc`, so either defer the automatic open until `board_shell_task_prepare()` has released the hold, or shrink what the automatic path reserves. Acceptance for the cycle is `uac2 play /sd/media/audio/clip10.wav` in OTG mode with no typed `uac2 open`, reporting `short=0 underruns=0` and heard on headphones. Two smaller items stand: the BLE shell input path drops multi-byte writes, which needs its own cycle and a client that sends byte-at-a-time until it is fixed, and `bleuart fetch` is still in the build and still untested now that TOL-004's write path is correct. The operator's three-cycle plan is unchanged -- this was still cycle 1, so cycle 2 is routing and a global volume through `sound_stream_*` scaling samples rather than using the DAC's weak hardware volume (USB-006), and cycle 3 is the emulators' resampler. The boot loop's lesson is worth carrying: an assert inside IDF is a reset with no log and no console, so a panic in the USB host path is only ever visible in the coredump partition, and a device that kills the board at enumeration cannot be diagnosed from the glass.

#### Files Modified:

- ports/twatchultra/main/uac2.c
- ports/twatchultra/main/usbhost.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 20 COMMIT Unreleased 2026-11-15T20:43:53-07:00

#### Coming From:

Unreleased 26175d7

#### Purpose:

Integrate USB audio into the desktop with a USB page in Settings that switches the USB-C port between Normal and OTG host mode and enables a USB-C DAC in host mode, as the first of three planned cycles, and hand off at the operator's request before it was finished.

#### Outcome:

STOPPED FOR A HANDOFF WITH THE LAST FIX BUILT AND NOT FLASHED, through fork commit `872eb4b` on `t-watch-ultra`. THE OPERATOR'S DESIGN, agreed before any code: one device-wide volume, a restart to change the USB mode, audio routed to the DAC automatically when it is attached and the speaker otherwise, delivered in three cycles. This is the first: the mode page, one owner for the host stack, and the DAC opening by itself. Routing and a global volume through the `sound_stream_*` sink are the second, and a resampler for the emulators the third. WHAT WAS BUILT. Settings gained a `USB...` button that opens `apps/usbset.c`. The page shows the port's role, Normal or OTG host, and a "USB-C DAC" checkbox that is drawn in the theme's dim colour and refuses presses while Normal is selected, since the widget set has no disabled state. The page reaches the board only through a new `td_usb_ops_t` in `td_sysinfo`, installed by a weak `board_usb_ops()`, so other boards show no page. The choices are saved as the board keys `usb.host` and `usb.dac`, device-wide, and applied at restart, and the page shows saved against running with a Restart button when they differ. `usbhost.c` is now the only code that installs the host stack and runs the library loop, and `uac2.c` uses it. With both keys set, boot starts the DAC driver in automatic mode: it takes the stream's transfers and a DMA reserve for the interface claim before any SD file is open, the reserve being two 512-byte, 512-aligned blocks, the shape of the HCD's isochronous descriptor lists (USB-005), and a worker opens the DAC when it is plugged in and forgets it when it goes. WHAT WORKED ON HARDWARE, by the operator and over BLE. The page rendered and behaved: OTG host and the DAC ticked, `Running: OTG host`. Normal to OTG and back by restart both behaved, the PC losing the watch and then regaining it at 20:36 as `ttyACM1`. In OTG mode the DAC opened by itself, logged at 3.3 s after boot, with the page reporting "USB-C DAC: KM-HIFI-384KHZ" and no `uac2 open` typed. WHAT FAILED: THE SHELL DOES NOT START IN OTG MODE. The Log Viewer, which needs no shell, showed `tdsh_bridge: cannot create the shell task` at 2919 ms. The shell's task is created at boot, just after this port's init, with a 24 KB internal-RAM stack (`TDSH_SHELL_TASK_STACK`), and the USB setup had taken that RAM first. In earlier cycles `uac2 open` ran after the shell already existed, which is why this never showed. A 25 KB block held across the USB setup and released at its end did not help, because the telnet server started at 2910 ms, between the release and the shell, and almost certainly split the hole. That is an inference: the Log Viewer keeps only 128 lines and cuts each at 38 columns, so the heap figures logged during boot were lost. The fix, which holds the block until a new weak hook `board_shell_task_prepare()` that the shell bridge calls immediately before creating the task, is built and not deployed. Build PASS for that last build at 0x2e07f0 bytes with 2 percent of the app partition free, sha256 fe0516cac4cec10d. The watch is RUNNING the previous build, sha256 1e9aa43698e8225c, flashed with `Hash of data verified`, IN OTG MODE with `usb.host = 1` and `usb.dac = 1`, so it is invisible to the PC and has no working shell. THE ONLY WAY BACK TO NORMAL IS THE USB PAGE ON THE GLASS, Settings, `USB...`, Normal, `Restart now`, because `board set usb.host 0` needs the shell; the page can also be driven over BLE with xterm mouse clicks, as was done to open it. Host suite 43 of 43: the API snapshot was re-recorded as an additive change, and `board_keys` passes again after `display2.transport`, unrecorded since entry 14, was recorded and described in the README with `usb.host` and `usb.dac`. No backup was taken this cycle; `working-2026-11-15-2002.bin` from entry 19 holds the last passed firmware. The required core-syntax audit was run before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched; no reference record changed, because the shell's stack size and boot order are this project's own facts.

#### Next Steps:

FIRST, RETURN THE WATCH TO NORMAL MODE through Settings, `USB...`, Normal, `Restart now`, and flash `fe0516cac4cec10d`, rebuilding from `872eb4b` if needed. Then switch to OTG with the DAC, restart, and check that the Terminal gives a prompt. If it does, run `heap` and record the free and largest internal figures in OTG mode, then `uac2 play /sd/media/audio/clip10.wav` without `uac2 open`, which is the acceptance test of this cycle. If the shell still fails, read the Log Viewer before anything else: `usbhost: before USB`, `after USB` and `shell hole released` each log the internal free, largest and DMA-largest figures. If even holding the block until the shell is created leaves too little for the telnet server or another task, shrink the USB tasks' 4 KB stacks or create the shell task before the USB setup, rather than putting stacks in PSRAM, which SOC-005 warns against. The Log Viewer's 128-line limit lost this boot's heap lines, so a future diagnostic of boot memory should log late, or to the SD card, rather than rely on it. After acceptance, cycle 2 is the router and the global volume through `sound_stream_*`, scaling the samples rather than using the DAC's weak hardware volume (USB-006), and cycle 3 is the emulators' resampler. The open items of entries 17 to 19 stand.

#### Files Modified:

- CMakeLists.txt
- README.md
- api/board_keys.txt
- api/tinydesk.api
- apps/settings.c
- apps/td_apps.h
- apps/usbset.c
- include/tinydesk/td_sysinfo.h
- ports/esp_idf/app/main.c
- ports/esp_idf/app/tdsh_bridge_esp.c
- ports/esp_idf/components/tinydesk/CMakeLists.txt
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/uac2.c
- ports/twatchultra/main/uac2.h
- ports/twatchultra/main/usbhost.c
- ports/twatchultra/main/usbhost.h

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 19 COMMIT Unreleased 2026-11-15T20:07:47-07:00

#### Coming From:

Unreleased a4eb902

#### Purpose:

Play an audio file from the SD card on headphones through the USB-C DAC, which the speaker could already do.

#### Outcome:

The operator passed it: `clip10.wav` played on headphones with "perfect quality" through fork commit `e051111` on `t-watch-ultra`, with `short=0 underruns=0` for the whole clip and the slowest SD read at 12 to 63 ms against a 500 ms ring. `uac2 play <file.wav>` streams 16-bit PCM from the card on its own task at the FILE's rate when the DAC's clock offers it, so a 44.1 kHz file needs no resampler: packets alternate 44 and 45 frames, sized from a running frame count so the average is exact, and IDF packs variable packets back to back (`hcd_dwc.c` `_buffer_fill_isoc`). The ring is about 500 ms in PSRAM. GETTING THERE TOOK FOUR FAULTS, EACH NAMED BY AN INSTRUMENT AND NOT BY READING. The player's log lines reached nothing, because only the shell task's output reaches the Terminal window, so its first failure was silent; its messages and every start failure are now kept and printed by `uac2`, and a failed start undoes what it took, which it did not, so one failure had made every later claim fail. That record named `transfer alloc: ESP_ERR_NO_MEM` from the player while `uac2 tone` started the same stream from the shell on the same boot; after the transfers were pre-allocated, the coredump named the deeper cause: `assert failed: transfer_descriptor_list_alloc hcd_dwc.c:1059`, IDF 5.3.1 panicking inside `usb_host_interface_claim()` when it cannot allocate the pipe's DMA descriptor lists, because an open SD file has already taken DMA-capable RAM (USB-005). Every DMA allocation the stream needs, its three 1,536-byte transfers and the interface claim, now happens once in `uac2 open` before any file is opened and is held while the DAC is open; stop and rate changes only switch the alternate setting, and streams are capped at 48 kHz. That removed every free from the unplug path, and the unplug recovery that panicked in entry 18 then worked once on hardware: re-seating the headphone plug made the DAC re-enumerate, and `uac2 open` recovered without a reboot. THE OPERATOR'S "DISTORTED, HISS, POPS, NO BASS" WAS NOT THE STREAM. It was measured three ways. A 440 Hz tone from the DAC's line output had no phase steps, sidebands at -107 dB and THD+N -62 dB at 8 packets per transfer, which confirms entry 18's change by measurement (USB-004 is now VERIFIED). The music captured at 10 and 100 percent software volume differed by exactly 20.0 dB in every band from 0 to 20 kHz. And the line output of `clip10.wav` matched the source file, copied off the card, to -50 dB from 300 Hz to 3 kHz once the two clocks were aligned to a fraction of a sample. The source plays well on the PC. What was wrong was DAC state: on several boots it played a clean, unmuted stream as silence, and re-seating the plug fixed it (USB-006, inferred). VOLUME IS NOT SETTLED. `uac2 vol` was first a sample scale, then, after the operator heard hiss, the DAC's own Feature Unit 2 Volume (range -74 to 0 dB, read back correctly); but on headphones 5 percent, -26 dB, sounded like about 70 percent, so the DAC's control is weak and the scale, which measured exact, is likely the better control after all. Diagnostics added: `uac2 fu [set]` reads and writes the DAC's mute and volume, `uac2 ev` lists short or empty transfers with whether the reader was inside `fread()`, and the play result reports the slowest read. Build PASS at 0x2dfbe0 bytes with 2 percent of the app partition free, sha256 b8f4bc165f13a15a, deployed by app-only flash with `Hash of data verified`. Ten builds were flashed during the cycle; this is the one passed. A full image was archived with the board held in the bootloader across read and verify: `working-2026-11-15-2002.bin`, 16,777,216 bytes, sha256 `3c243d792c50b6d7009b1008c5189898607e68aca8e5243279a08138f0b8f80e`, `verify OK (digest matched)`, its sidecar re-checked from its own directory, `ota_0` inside it byte-identical to the build, and the board up afterwards. The PC's line-in was set to 5 percent for captures and restored to 86 percent at the end, which is ALSA `Capture` 63 and `Line Boost` 2. The required core-syntax audit was run before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched; USB-004 was revised to VERIFIED and USB-005 and USB-006 added with index and routing rows, 137 records with no duplicate ids.

#### Next Steps:

VOLUME FIRST, AND IT IS SMALL. Put `uac2 vol` back on scaling the samples with the DAC held at 0 dB, using steps of a fixed number of decibels so the curve matches hearing, as the Tamagotchi's volume already does, and confirm it both by ear and with a line-in capture. A capture needs the PC's line-in near 5 percent, because at its normal 86 percent the DAC clips it. THE DAC'S SILENT STATE IS UNDERSTOOD ONLY AS A WORKAROUND: re-seating the headphone plug and running `uac2 open` recovers it, but what puts it there is not established, so the next time it happens note whether the headphones went in before or after the watch powered the DAC. `full.wav`, about 4 minutes 49 seconds, has never been played; it is the test of SD reads over a long track. A file at a rate the DAC does not list is refused rather than converted, and the emulators still need a resampler to reach the DAC. The watch's USB stays in host mode after any audio command, so a flash needs a reboot first. `bleuart fetch`, `usb_hid.c`'s `ESP_INTR_FLAG_LEVEL1`, the `coredump` partition and entry 1's wrong fork path stand as entry 17 left them.

#### Files Modified:

- ports/twatchultra/main/uac2.c
- ports/twatchultra/main/uac2.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 18 COMMIT Unreleased 2026-11-15T17:37:51-07:00

#### Coming From:

Unreleased c47a6ed

#### Purpose:

Remove the scratch the operator heard in entry 17's DAC test tone, measuring it from the DAC's line output on the PC rather than judging it by ear alone.

#### Outcome:

The operator passed the tone as perfect on headphones, through fork commit `4267dca` on `t-watch-ultra`, but the cause is only half established and the cycle shipped one known defect. THE MEASUREMENT CAME FIRST. With the DAC's output on the PC's line-in, a 440 Hz capture once the PC's capture gain had been lowered enough not to clip showed the scratch as a discontinuity every 16 ms: sidebands at 440 plus or minus multiples of 62.5 Hz at -36 dB each, and a phase step of about 0.18 rad, about 3 samples, every 16 ms -- one per 16-packet isochronous transfer. TWO REAL BUFFERING FAULTS WERE FOUND AND FIXED, AND THEY WERE NOT THE SCRATCH. `xfer_done()` read the ring once, and a byte ring stops at its wrap point, so fills came back short and were zero-padded without being counted, because only an empty ring incremented `underruns`; it now reads across the wrap and counts every short fill as `short`. `uac2 tone` paced itself with a fixed 2 ms delay and ran slightly behind the DAC; it now blocks on ring space through a new `uac2_write_wait()`, and `uac2_write()` no longer returns `pdTRUE` as a byte count (TOL-004). The proof that these were not the cause is the next capture: with the watch reporting `short=0 underruns=0` the sidebands were unchanged at -36 dB and the phase steps were still there. So the glitch is at the transfer boundary in the host path, recorded as USB-004 with status INFERRED because its mechanism is unknown. Halving the transfer to 8 packets, one constant and nothing else, gave a tone the operator heard as perfect. That result rests on ears alone: every line-in capture on the 8-packet build came back as digital silence while the watch reported a clean stream, so the 125 Hz sideband spacing that would prove the per-transfer mechanism was never measured. THE UNPLUG RECOVERY ADDED THIS CYCLE CRASHES, AND IT SHIPPED. A capture at 17:04 was silent because the DAC had dropped off the bus mid-stream, with status 7 `NO_DEVICE`, and the driver kept writing into a dead stream; `forget_gone_device()` was added so the next `uac2 open` would recover. The coredump of the third black-screen hang names it: `assert failed: tlsf_free tlsf.c:1201 (!block_is_free(block) && "block already marked as free")`, in `usb_host_transfer_free()` called from `forget_gone_device()` at `uac2.c:610`, freeing `s_xfer[1]` after `s_xfer[0]` freed cleanly. It is reached because inserting or removing the DAC's 3.5 mm plug makes the DAC re-enumerate (USB-003), which three jack changes during a stream each turned into a panic on the next `uac2` command; the core holds only task stacks, so whether this is a double free or a damaged heap header is not established. A `reboot` sent over BLE at 17:05 while the stream was stuck on the dead device also left the watch not booting until the operator power-cycled it. Builds: sha256 bf2d56cc69cfa873 with the buffering fixes, flashed and confirmed by `verify OK (digest matched)`; then sha256 41bdc420d0ccb3cd with 8 packets per transfer, flashed with `Hash of data verified`, which is the one passed. A rebuild after a comment-only edit differs from it in 68 bytes, all in the app descriptor's build time and the trailing image hash. The PC's line-in was set to 5 percent for the captures and restored to 86 percent, which is ALSA `Capture` 63 and `Line Boost` 2. No 16 MiB backup was taken. The required core-syntax audit was run before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched; USB-003 was revised and USB-004 added with its index and routing rows.

#### Next Steps:

THE UNPLUG CRASH IS FIRST, because a jack change is an ordinary user action and it takes the watch down. Until it is fixed, do not move the DAC's plug while a stream is open. The approach proposed and not yet approved is to stop freeing transfers on that path: allocate the three isochronous transfers once, reuse them on reopen against the new device handle, and log each transfer's pointer, buffer and status at DEV_GONE and at reopen, so that heap damage, if that is what this is, shows up in a log rather than as another silent panic. Test it by moving the jack mid-stream, then `uac2 open` and `uac2 tone`. Next, USB-004 needs its measurement: a line-in capture at 8 packets, with the capture's peak checked before any analysis, which is the step that would have caught the empty captures at once. Two of them were empty while the PC detected a plug in its line-in jack, so the DAC end of that cable is suspect. Then the audio objective proper: a resampler from each emulator's rate to 48 kHz, gnuboy's 43,691 Hz first. `bleuart fetch`, `usb_hid.c`'s `ESP_INTR_FLAG_LEVEL1`, the `coredump` partition, the owed backup and entry 1's wrong fork path stand as entry 17 left them.

#### Files Modified:

- ports/twatchultra/main/uac2.c
- ports/twatchultra/main/uac2.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 17 COMMIT Unreleased 2026-11-15T17:01:08-07:00

#### Coming From:

Unreleased 8f5a81a

#### Purpose:

Make the BLE console a usable serial terminal and get the USB-C DAC to play sound, after both had been chased through custom clients and screen scraping without a measurement of either.

#### Outcome:

Both landed on hardware and the operator passed the tone: a 440 Hz sine out of the TTGK DAC on both channels, described as scratchy, through fork commit `e437aaf` on `t-watch-ultra`, pushed to `aquasock/tinydesk`. ENTRY 16'S DIAGNOSIS IS SUPERSEDED: there was no shell wedge and no truncated argument. The `fetch /r` it read as a mutilated path was the 40-column Terminal window clipping the line, repeated 3,483 times in its capture, and the operator had in fact seen 4 KB move both ways. THE BLE STREAM WAS ONE BUG IN ONE RETURN VALUE (TOL-004). `bleuart_write()` returned `xRingbufferSend()`'s `pdTRUE` as a byte count, so every write reported one byte and `td_render_flush()` resent the buffer shifted by one byte at a time until the 4 KB ring filled; the failing write then made the renderer drop the frame and repaint the whole screen 500 ms later, for ever. A captured repaint decomposed byte for byte into one 1,024-byte render buffer plus its copies from offsets 1, 2 and 3, 4,090 bytes, and that single fault explained the 24 KB/s idle stream, a screen that never arrived below row 10, the corrupted rows and `fetch`'s duplication. After the fix an idle link carries 240 bytes in 5 s against 118,610 before, and a full redraw is 3,557 bytes rendering all 26 rows. THE DAC NEEDED FOUR THINGS AND NONE WAS ITS FAULT. `usbdiag 30` showed first that the hardware path works: the dongle enumerates at full speed as UAC2 with playback alternates of 384, 576 and 768 bytes (USB-003), and the watch stayed up with the host running. `uac2` could not see it because no task ran `usb_host_lib_handle_events()`, and it could not have streamed or completed a control request because no task ran `usb_host_client_handle_events()`, its control helper passed a NULL callback to a submit that does not block, and the client was registered synchronous against IDF's own instruction (USB-002); those are now two event tasks and a semaphore-waited `ctrl()`. With that fixed, claiming the endpoint failed with `EP MPS (768) exceeds supported limit (128)`, which is IDF's default FIFO split rather than hardware (USB-001), so `CONFIG_USB_HOST_HW_BUFFER_BIAS_PERIODIC_OUT=y` went into `sdkconfig.defaults`. And the descriptor parser read UAC2 with UAC1 offsets: it rejected the 8-byte clock source with a `>= 9` test, misread format descriptors, chose the last alternate whatever its format, and had `set_rate()`'s `wIndex` bytes swapped. It now records every alternate, picks alt 1 (2 ch S16, 384 bytes), follows terminal 1 to clock 9, lists 8 to 96 kHz with a RANGE request and sets 48 kHz with `ESP_OK`; the stream then ran over 95 s with `errors=0 missed=0`. THE METHOD IS THE OTHER RESULT. The watch's USB port is a plain serial line driven with `stty`, `cat` and `printf`, `ESC[5000~` is TinyDesk's own full-redraw request and F10 opens Start; BLE now goes through the stock `ble-serial` tool installed in `~/.venvs/ble-serial`, which presents `/tmp/ttyBLE` for minicom, after a transient `cons.service` left by an earlier session had been auto-reconnecting and was stopped. One self-inflicted reset is recorded: a pyserial open that cleared DTR then RTS passed through RTS-high DTR-low and reset the chip. Build PASS at 0x2de5e0 bytes with 2 percent of the app partition free, sha256 aecb5ed4c98f3a51, deployed by app-only flash to 0x10000 with `Hash of data verified`; a rebuild after comment-only header edits differs from the flashed image only in the app descriptor's build time and the trailing image hash, 67 bytes and no code. The earlier build with only the BLE fix and the event loops, sha256 dff4d72e6b247a6e, was flashed and tested first. No 16 MiB backup was taken this cycle. The required core-syntax audit was run before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched; the reference gained the USB topic and kind, TOL-004 and USB-001 to USB-003 with index and routing rows, and holds 134 records with no duplicate ids.

#### Next Steps:

THE TONE IS SCRATCHY AND THAT IS THE NEXT THING TO MEASURE. `uac2 tone` fills the ring in 96-frame slices with a fixed 2 ms delay rather than against ring space, and the underrun counter rose while it played, so gaps of silence inside the tone are the first suspect; count underruns across a tone alone, then pace the generator by `uac2_write()`'s accepted count, before suspecting the device. After that, real audio needs a resampler from each emulator's rate to 48 kHz, gnuboy's 43,691 Hz first. `uac2 stop` and a second `uac2 open` after a stop are unexercised, and `usbdiag` cannot run once `uac2` has installed the host until a reboot. The watch's USB stays in host mode after any audio test and only `reboot`, sent over BLE, returns it to the PC for a flash. `ble-serial` does not reconnect after a watch reboot and leaves a stale `/tmp/ttyBLE` when killed; `ble-autoconnect` is its companion if that becomes tiresome. `bleuart fetch` from `8798688` is in the build and untested now that its write path is correct. `usb_hid.c` still carries `ESP_INTR_FLAG_LEVEL1`, the `coredump` partition still holds, a full-flash backup is owed, and entry 1's fork path is wrong: the fork is `/run/media/vash/GIT/tinydesk-twatch`, not `~/GIT`. The wider backlog is as entry 15 left it.

#### Files Modified:

- ports/twatchultra/main/bleuart.c
- ports/twatchultra/main/uac2.c
- ports/twatchultra/sdkconfig.defaults

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 16 COMMIT Unreleased 2026-11-15T16:05:00-07:00

#### Coming From:

Unreleased 9c2093a

#### Purpose:

Complete the half of BLE file transfer that entry 15 left lossy by reading a 4 KB file back off the watch, and find out why the read kept failing.

#### Outcome:

THE READ FAILED, AND THE FAILURE WAS NOT IN THE READ PATH: `bleuart fetch` WEDGED THE SHELL TASK, so every command typed after it was swallowed by a shell that never returned to a prompt. The operator reported "cannot open file" and then a console that answered nothing, which reads as a dead link; it was a live link in front of a stopped shell. The evidence is a replay of the complete notify stream -- 2.7 MB captured without gap -- which reconstructs the terminal as frozen at `root@twatch:~# bleua...fetch /r` with no prompt ever following, while four later commands (`echo ZZZTEST`, `head -c 200 /root/x.txt`, `sha256sum`, `ck` via `cksum`) produced not one byte of output. THE CAUSE IS CONTENTION FOR ONE 4 KB RING. `bleuart fetch` looped calling the non-blocking `bleuart_write()` and retried every 5 ms with no bound, while the display mirror writes through `board_serial2_alt_write()` into that same ring and the desktop repaints far faster than a connection interval drains it. The ring therefore never emptied, the loop never exited, and the shell task -- which is where `fetch` runs -- spun forever. THE FIX GIVES EACH WRITER THE BEHAVIOUR IT NEEDS. A `s_fetching` flag makes `board_serial2_alt_write()` return early during a transfer, so the mirror yields the ring instead of fighting for it; the call site was confirmed rather than assumed, with `hal_mux.c:274` reading `return s_s2_open ? board_serial2_alt_write(buf, len) : 0` inside `serial2_write()`, which is display 2's mirror path. A new `bleuart_write_wait()` blocks for ring space with a 500 ms ceiling, and a transfer that stalls five times in a row now ENDS with `(client stopped reading)` rather than hanging the only channel that could report it -- the previous loop's shape, "retry forever", is precisely what made a partial failure indistinguishable from a dead watch. The chunk was also halved to 256 bytes and the pre-report drain made bounded. TWO THINGS ABOUT THE INSTRUMENTS ARE WORTH CARRYING. First, MY OWN RENDERER WAS THE BUG: it rebuilt the screen from only the most recent capture buffer, so it displayed stale text and reported four consecutive successful commands as failures, which is where the wasted round trips came from; replaying the full stream offline fixed the reading and immediately named the wedge. Second, THE MIRROR SENDS ONLY CHANGED CELLS, so transient output is never in the capture while persistent output always is -- `ls -l`'s `4000  x.txt` survived and `echo ZZZTEST` left no trace at all -- which is exactly the flicker the operator described and the reason a capture-based reading cannot be trusted as a test of whether a command ran. The operator's "you are not reading the screen properly, its all flickery" was correct and was the turning point. The absence of the `cannot open` text in my capture is explained by the same mechanism and is not a contradiction: output is non-blocking and lossy under burst per entry 15, so an error printed into a mirror-filled ring never reaches the client. BusyBox `tdsh` also has no `md5sum`, no `sha256sum` and no `cksum`, so on-device checksum verification needs a different tool. Build PASS at 0x2dde80 bytes with 2 percent of the app partition free, sha256 9c7678de767ab384, and the new strings were confirmed present in the image; DEPLOYMENT DID NOT RUN because the watch was not enumerated on USB at the end of the cycle, so the fix is built and unflashed, and the reverse transfer is therefore still unproven -- no byte has yet come back off the watch. The fork also carries UAC2 driver work that predates this cycle and was committed separately at handoff (`7ee5dfe`) so it could not be lost, not because it was exercised -- it remains flashed once and never tested against the DAC. The required core-syntax audit was run against this entry before commit and PASSED, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched: the entry carries all six sections in template order, its Status values are drawn from the permitted set, it ends with a rule, and the log now holds 16 of its 100 permitted entries.

#### Next Steps:

THE IMMEDIATE ACTION IS ONE FLASH AND ONE COMMAND. The watch must be on USB (`/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00`; only the TinyTang board was present at handoff), then flash the build at sha256 9c7678de767ab384, and only then run `bleuart fetch /root/x.txt` with a client that keeps its capture. Acceptance is 100 intact `Lnn`+36x lines, which is 4000 bytes; anything short leaves `(client stopped reading)` or a byte count to diagnose from. A SECOND ISSUE IS UNRESOLVED AND SHOULD NOT BE CONFUSED WITH THE FIRST: the argument echo arrived truncated in the capture (`fetch /r` for `fetch /root/x.txt`), and if that truncation is real rather than a rendering artefact then `fopen` is being handed a mutilated path -- the operator's "cannot open file" points the same way. Test with a file in the current directory (`bleuart fetch x.txt`) so a long absolute path cannot be the variable. THE CLIENT SHOULD ALSO STOP GUESSING AT THE SCREEN: it must keep the whole notify stream and replay it offline, never rebuild from a single buffer, and it must not press Enter on a menu row whose highlight has not been read -- the highlight is `48;5;25` on the selected row and `48;5;238` on its neighbours, which was confirmed from the raw bytes after Task Manager had already been opened twice by a blind Enter. A structured read path is the real answer to the reverse direction: a length-prefixed or CRC-terminated frame would let the client verify a transfer without the file ever being rendered, which is the cheap version of what entry 15 proposed. The larger objective is unchanged and untouched: the UAC2 driver is written, flashed once and never exercised, so `uac2 tone` against the TTGK KM-HIFI-384KHZ is still the thing that decides whether headphones work on this board, and `usb_hid.c` still carries `ESP_INTR_FLAG_LEVEL1` so the USB keyboard path cannot start. The `coredump` partition still holds and should be revisited deliberately. The wider backlog is as entry 15 left it.

#### Files Modified:

- ports/twatchultra/main/bleuart.c

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 15 COMMIT Unreleased 2026-11-15T12:42:41-07:00

#### Coming From:

Unreleased f43d7ef

#### Purpose:

Make the Bluetooth console usable as a data channel, because it was silently losing bytes when a file was written through it.

#### Outcome:

File transfer over the BLE console is now byte-exact on both filesystems -- 1000 of 1000 bytes and 4000 of 4000, into littlefs and onto the SD card, each verified with `ls -l` read back through the same console -- and the defect that made it lossy was NOT in the Bluetooth layer. THE FIRST MEASUREMENT NAMED THE WRONG SUSPECT AND THE INSTRUMENT CLEARED IT. A 1000-byte transfer into littlefs arrived as 760 bytes, then 800, then 920, varying only with how fast the client sent. The BLE layer's own counters were then extended to report both what it accepted and what it handed to the reader, and they read `in=1853 out=1853 refused=0`: every byte that entered the ring left it. The loss was downstream. THE ACTUAL DROPPER WAS THE SHELL BRIDGE, and its size explains why nobody had seen it: `tdsh_bridge_esp.c` held a 256-byte input stream buffer, about six typed command lines, and passed it to `xStreamBufferSend()` with a zero timeout. That call returns a short count when the buffer is full and the caller ignored the count, so a file arriving at a few hundred bytes a second simply discarded everything past the first 256. A keyboard cannot fill 256 bytes between two frames; a file transfer fills it continuously. IT IS NOW 2048 BYTES AND THE WRITE WAITS. The wait is in bounded slices with a total budget, so the whole chain acquires backpressure rather than one component absorbing it: the shell drains, the bridge blocks briefly, the desktop's key handling slows, fewer bytes leave the transport, and the transport's own flow control tells the far end to pause. The BLE side completes that chain in two ways -- a full ring now REFUSES the write with ATT 0x11 instead of swallowing it, and XON/XOFF watermarks at three-quarters and one-quarter of the ring pause a client that honours them. The refusal was observed working: a test client that predated the flow control crashed with `BleakGATTProtocolError INSUFFICIENT_RESOURCE (17)`, which is the new refusal arriving where silent loss used to be, and once the client honoured XOFF and retried, the transfer came through exact. TWO CHANGES WERE TRIED, FOUND WRONG, AND REMOVED RATHER THAN LEFT IN. Growing `TD_EVENT_QUEUE_SIZE` from 32 to 128 was a plausible theory -- every input byte becomes one event and a 40-byte command line is 40 events -- and it changed nothing measurable, so it was reverted rather than shipped carrying a rationale that did not hold; the CMake comment asserting it was the cause would have misled the next reader. And blocking on the OUTPUT path was tried and made things dramatically worse: it stalls the renderer, which then repaints, and reading a 4000-byte file back produced 17,700 bytes of screen traffic with not one of the file's 100 line markers intact. Output therefore stays non-blocking and lossy under burst, which is the right trade -- a lost output byte shortens a diagnostic, where a lost input byte corrupts a file quietly. WHAT REMAINS IMPERFECT AND IS RECORDED RATHER THAN HIDDEN: reading a large file back over the console is still lossy, because the notify path is throttled by connection interval and the ring can overflow; `cat` of a 4000-byte file returned roughly 3.4 KB. Writing is solid, reading is best-effort, and the asymmetry is deliberate. A client that ignores both the ATT refusal and XON/XOFF can still lose data, which is stated in the header. The BLE connection also proved slow to recover after repeated client churn -- a killed client leaves the watch believing it is connected until supervision timeout, during which it does not advertise. Build PASS at 0x2db650 bytes with 3 percent of the app partition free, sha256 28aed4e0e70944d9; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the board was rebooted to a clean desktop and a full 16 MiB image archived with it held in the bootloader across read and verify: `working-2026-11-15-1238.bin`, 16,777,216 bytes, sha256 `526e48a9ecb9cb8f51a404c86f8d08af9cea593448f88e423265612e9b37f133`, `verify OK (digest matched)`, its sidecar re-checked from its own directory, `ota_0` inside it compared byte-identical to the build. The operator's requirement was that file transfer be solid, and the acceptance is the byte counts above rather than a counter. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched; no reference record changed, because this is a defect in this project's own bridge rather than an external fact.

#### Next Steps:

READING A FILE BACK OVER THE CONSOLE IS THE OWED HALF, and it needs a different approach from writing: the notify path is throttled by connection interval, so `cat` of a 4 KB file returned about 3.4 KB. Options, none yet tried: raise the ATT MTU above 256 so a notification carries more, shorten the connection interval by requesting different parameters, or stop using `cat` for retrieval and have the watch compute a checksum the client can compare -- which would make transfer verification cheap without needing the bytes back at all. THE CONNECTION-RECOVERY BEHAVIOUR SHOULD ALSO BE HANDLED: a client that dies without disconnecting leaves the watch advertising nothing until supervision timeout, which reads as a dead console; either a shorter supervision timeout or an explicit `bleuart drop` that terminates a stale connection would make the failure self-limiting. The `usbdiag` command and the DAC it was written for remain untested -- that work was interrupted by this defect and is still the larger objective, with the enumerated device (TTGK KM-HIFI-384KHZ 3302:3366, UAC 2.0, isochronous synchronous OUT) waiting to be tried now that the console can watch it. `usb_hid.c` still carries `ESP_INTR_FLAG_LEVEL1`, so the USB keyboard path remains broken. The `coredump` partition stays for now: it has named three faults in one session and cost one flash of a resized `ota_1`, and should be reverted deliberately rather than drifting. The wider backlog is unchanged: Pause, Reset and Save are labelled on all three emulators and send nothing, the Game Boy's right column has a third slot that is drawn furniture and could carry `gb save`, the BLE controller must be reconnected after every reboot, `pm.c` still duplicates the rasteriser `gamepad.c` owns, the NES owes battery-SRAM and its skip-path test, and the status strip still owes the battery and the year against entry 11's measurement.

#### Files Modified:

- ports/esp_idf/app/tdsh_bridge_esp.c
- ports/twatchultra/main/bleuart.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 14 COMMIT Unreleased 2026-11-15T11:50:33-07:00

#### Coming From:

Unreleased 6c37673

#### Purpose:

Give the watch a console that works while its USB port is a host, because every existing way in collides with the work it is meant to observe.

#### Outcome:

The watch now offers a serial console over Bluetooth LE, verified end to end from the development machine: `bluetoothctl` scans it as `TinyDesk`, a client connects, and `uptime` typed over BLE returns `0 days 00:01:05`. THE REASON IS THAT EVERY OTHER PATH FAILS IN A SPECIFIC MEASURED WAY. USB is the console and the OTG host at once and cannot be both: the USB-Serial-JTAG peripheral resets the chip when a device talks on the lines the host controller is driving, which `esp_reset_reason()` reports as `usb (code 11)` -- a reset with no panic and therefore no coredump, which is why three earlier runs ended with an empty log and no explanation. WiFi is not a substitute because it does not fit: with the USB host stack linked in, internal RAM measured 41 KB free against the 65,536 the SSH server refuses to start below. The BLE radio is on core 0, on its own pins, and already running for the keyboard, so it is the one channel that touches nothing else. The service is Nordic's UART shape -- 6E400001 with a write characteristic in and a notify out -- so ordinary BLE serial clients work without a custom app, and it plugs in as the port's ALTERNATE serial-2 transport, which is why the shared display layer learned nothing: `hal_mux.c` gained three weak board hooks and the key `display2.transport = ble`. THE SESSION'S REAL PRODUCT IS THREE BUGS, AND NONE WAS FOUND BY READING CODE. The first was in shared display code and predates this work entirely: `spi_device_acquire_bus()` was called unchecked in `push_pixels_raw_buf()`, and the subsequent `spi_device_release_bus()` asserts when the bus was never held, so the paint task and the status strip's task contending for one SPI device handle was a live panic -- a reboot in zero seconds with the console compiled out, which the operator saw as a flash. It was named by a coredump after three wrong guesses: `assert failed: spi_device_release_bus spi_master.c:1330`, with frames `push_pixels_raw_buf -> push_pixels_raw -> push_pixels`. The second made USB host impossible on this board: the host was installed with `ESP_INTR_FLAG_LEVEL1`, the level-1 slots are taken by WiFi and BLE, and `usb_host_install()` therefore returned `ESP_ERR_NOT_FOUND` -- which is not a return value anywhere in its own path, but is exactly what `esp_intr_alloc()` returns when no slot matches. `LEVEL3` works, and the same flag in `usb_hid.c` means the USB keyboard and mouse feature documented for this port has never run here at all, because `usb.host` was never set. The third was mine and is recorded because it is a general NimBLE rule rather than a local slip: GATT services must be registered between `nimble_port_init()` and `nimble_port_freertos_init()`, because `ble_svc_gap_init()` opens with `SYSINIT_ASSERT_ACTIVE()` and panics otherwise; only advertising belongs after the sync callback, because the address is not known before it. A fourth, smaller one: `ble_svc_gap_device_name_set()` sets a characteristic a client can only read AFTER connecting, so the name has to be placed in the advertisement itself or a scan-by-name finds nothing while the service is running correctly -- observed as `started=1 tx_handle=18` beside a scan that saw no such device. THE METHOD IS THE PART TO CARRY: every one of those was found by an instrument, not by reasoning, and the two that mattered most came from the coredump partition the reference already documented (EMU-021) -- 64 KB carved from the unused `ota_1`, `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`, ELF format, decoded with `idf.py coredump-info`. A `usbdiag` command was also added for the USB host work this was in service of; it logs the reset reason at the start of each run so that a run ending without its `# done` line still says why, and it is how `ESP_RST_USB` was identified. Build PASS at 0x2db4f0 bytes with 3 percent of the app partition free, sha256 521d33ac0b989049; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the board was rebooted to a clean desktop and a full 16 MiB image archived with it held in the bootloader across read and verify: `working-2026-11-15-1146.bin`, 16,777,216 bytes, sha256 `b9dd507c1a8a01b43193641e941ea3ad8e583b70feb674f2d2409f048960cd46`, `verify OK (digest matched)`, its sidecar re-checked from its own directory, and `ota_0` inside it compared byte-identical to the build. THE PARTITION TABLE CHANGED in this cycle and that is why the backup mattered: `ota_1` shrank from 0x300000 to 0x2F0000 to free 64 KB at 0x600000 for `coredump`, which is a layout change rather than an app change, and the previous image `working-2026-11-15-1138.bin` still restores the old one. The operator's test of the console was a PC-side client typing a shell command and reading the answer, not a counter. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched; BLE-008 was added to the reference with its index entry and routing row, and the file holds 130 records with no duplicate ids. AN HONEST NOTE ON SCOPE: the DAC this was all in service of has still not been tested. What this cycle bought is the ability to test it -- the USB port can now be a host while the console stays reachable, which no earlier attempt could do.

#### Next Steps:

THE USB AUDIO WORK IS UNSTARTED AND NOW POSSIBLE. `usbdiag` needs one run with the DAC attached through the splitter, watched over BLE, to answer the two questions the coredump work kept interrupting: whether the dongle (TTGK KM-HIFI-384KHZ, 3302:3366, UAC 2.0, bcdADC 2.00, 422-byte configuration descriptor, isochronous synchronous OUT endpoint) enumerates at all, and at what speed -- the ESP32-S3's OTG port is full speed only, IDF's host documentation says so, and a high-speed-only device cannot come up on it. If it does enumerate, the driver question is separate and already scoped: Espressif's `usb_host_uac` is UAC 1.0 only through 1.5.0, so a UAC2 device means either the community `usb_host_uac2` (which calls only public `usb` component API and whose `>=5.4` floor looks like convention rather than a requirement) or a purpose-built pump using the isochronous support that `hcd_dwc.c` has in 5.3.1. Either way a rate converter is unavoidable, because gnuboy emits 43,691 Hz and a DAC takes discrete rates. TWO THINGS SHOULD BE DECIDED BEFORE MORE USB WORK. The `coredump` partition and the shrunk `ota_1` are a diagnostic configuration, not a permanent one, and should either be kept deliberately with the cost stated or reverted in a cycle of their own. And `usb_hid.c` still carries `ESP_INTR_FLAG_LEVEL1`, so the USB keyboard and mouse path stays broken until that one flag is changed -- it is a two-line fix that makes a documented feature real, and it is unrelated to audio. THE BLE CONSOLE HAS KNOWN LIMITS: it is one connection, it drops output rather than blocking when the ring fills, it requires `bt.enable = 1` because it rides the radio `bt_hid.c` starts, and it is selected with `display2.transport = ble` which means display 2 is no longer available for a UART. The `bleuart` command reports `started`, the connection handle, the TX value handle and both ring levels, and can restart advertising; it exists because a console that cannot be seen is not a console. The wider backlog is unchanged: Pause, Reset and Save are labelled on all three emulators and send nothing, the Game Boy's right column has a third slot that is drawn furniture and could carry `gb save`, the BLE controller must be reconnected after every reboot, `pm.c` still duplicates the rasteriser `gamepad.c` owns, the NES owes battery-SRAM and its skip-path test and is parked with `docs/nes-accuracy.md` as its baseline, and the status strip still owes the battery and the year against entry 11's 446 ink pixel measurement.

#### Files Modified:

- ports/esp_idf/app/bt_hid.c
- ports/esp_idf/app/hal_mux.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/bleuart.c
- ports/twatchultra/main/bleuart.h
- ports/twatchultra/main/display.c
- ports/twatchultra/main/gps.c
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/usbdiag.c
- ports/twatchultra/main/usbdiag.h
- ports/twatchultra/partitions.csv
- ports/twatchultra/sdkconfig.defaults

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 13 COMMIT Unreleased 2026-11-15T09:32:01-07:00

#### Coming From:

Unreleased 50fb568

#### Purpose:

Put the GPS receiver's clock into the GPS app, so the watch can be given the time from the sky without a shell or a network.

#### Outcome:

The GPS window gained a `Set clock` button, the operator pressed it and it reported `Clock set to 16:25:41 UTC.`, and the point of the cycle is that the button reuses a proven path rather than adding a second one. IT CALLS THE SAME TWO STEPS THE `gps time` COMMAND DOES, through `board_time_apply()`: settimeofday followed by `board_time_set_post()`, and the second step is what writes the PCF85063A. That distinction is the whole correctness of the feature and it is already recorded -- entry 12's predecessor and GPS-005 both establish that settimeofday alone leaves the chip holding the old value, and that the next boot seeds FROM the chip, so the correction would be undone at the next power cycle without ever showing a fault at the moment it was applied. A clock button that appeared to work and silently failed a reboot would be worse than no button, which is why the existing verified path was reused rather than a convenience call written beside it. THE SEAM GAINED ONE OPTIONAL FIELD. `td_gps_ops_t` is now `start`, `stop`, `status`, `set_time`; the app asks whether `set_time` exists and draws no button when it does not, so a board with a receiver and no clock to set shows nothing dead. The port's implementation copies the parser state out under the lock before converting, because the pump task owns `nmea_state_t` and the conversion reads eight of its fields -- 200 bytes inside a critical section, rather than computing an epoch in the pump task on every 100 ms tick for a button pressed monthly. The conversion goes through `nmea_utc()`, so this project's 2020..2100 plausibility floor applies and a receiver that has a date but no usable year cannot set the watch to 1970. THE RESULT IS REPORTED AS A WORD AND THAT IS DELIBERATE: a button that does nothing visible cannot be told from one whose press was missed, and this is the only control in the window that changes something outside it. The failure branch matters more than the success branch, because the common case for the first half minute after opening is a receiver that has no UTC yet, and it has to say `No time from the receiver yet.` rather than appear to have worked. The message and the button sit on the client's last two rows addressed NEGATIVELY, so they follow the window when it is resized -- the fault the Network app's list hit when it computed those rows from the height it was given at launch, and the window manager's own tests caught. WHAT WAS AND WAS NOT VERIFIED IS WORTH SEPARATING, because the app's `UTC:` line is the RECEIVER's clock and not the watch's. The operator's report and a read of the mirror together establish that the button renders, that its press produced `Clock set to 16:25:41 UTC.`, that the message persisted across a repaint, and that the receiver's own time then read 16:26:30 against a host at 16:26:34 -- a four second gap that is read latency. The operator checked the bezel against the window directly and reported the strip at 9:27 against the app's 16:27, exactly seven hours apart, which is the MST7 offset this board carries and means the two clocks agree. WHAT WAS NOT OBSERVED THIS CYCLE IS THE SYSTEM CLOCK OR THE RTC ITSELF: the watch's clock was already correct before the press, because the strip read 9:07 local against a true 16:07 UTC, so there was no error to correct and no before-and-after delta to show. The evidence for the write is therefore that the code path is the one GPS-005 verified end to end, not a fresh measurement of it, and the decisive test that was offered and not taken is a reboot, where hctosys re-seeds the system clock FROM the chip and would show the write had landed. Build PASS at 0x2cc240 bytes with 7 percent of the app partition free, sha256 e7b5b43983ebc6d8; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the host suite passed 43 of 43 and the API snapshot was re-recorded with a one-line additive diff, the ops struct gaining its trailing field; the board was rebooted and a full 16 MiB image archived with it held in the bootloader across read and verify: `working-2026-11-15-0927.bin`, 16,777,216 bytes, sha256 `df41e30db7a68d52c0030e31da266c9c1743c3cae1ec0006a78ec122e443df6d`, `verify OK (digest matched)`, its sidecar re-checked from its own directory, `ota_0` inside it compared byte-identical to the build. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched -- no reference record changed, because the button adds a caller to a device fact already recorded as GPS-005 rather than establishing a new one. A NOTE ON METHOD, since it happened twice in one session: an attempt to read the watch's clock from the host before the press produced a stale composite of the Start menu in the USB mirror and nothing useful, which is DSP-003 again. The operator read the bezel and answered the question in one line.

#### Next Steps:

A REBOOT IS THE ONE MEASUREMENT THIS CYCLE OWED AND DID NOT TAKE, and it is cheap: hctosys seeds the system clock from the PCF85063A before init, so a reboot with no network and no receiver would show whether `board_time_set_post()` actually landed. It was offered at the operator's direction to defer rather than refused, and a future cycle that touches time handling should take it before trusting the button further. THE RECEIVER'S OWN CONFIGURATION REMAINS UNTOUCHED and is the larger question: GPS-003 records that this unit answers NMEA but never the UBX identification poll, so the constellation set, the sentence rates and the power mode are factory default and cannot be changed from the watch. Anything wanting those -- a lower NMEA rate to cut the 12 sentences a second this task parses, a constellation subset to cut power, or a backup mode -- has to establish the UBX side first, and that is the next real investigation about this part rather than a UI task. The GPS window still shows one coordinate format regardless of the Settings date and clock preferences, offers no diagnostic view of talker counts (so `gps raw` remains the only way to see them, and the two sides are exclusive), and drops the receiver on close but not on an ordered shutdown. The wider backlog is unchanged: Pause, Reset and Save are labelled on all three emulators and send nothing, the Game Boy's right column has a third slot that is drawn furniture and could carry `gb save`, the BLE controller must be reconnected after every reboot, `pm.c` still duplicates the rasteriser `gamepad.c` owns, the NES owes battery-SRAM and its skip-path test and is parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase, and no DMG cartridge has ever been launched so the `GB_HW_CGB` compatibility path remains unproven. THE STATUS STRIP STILL OWES THE BATTERY AND THE YEAR, and entry 11's 446 ink pixel measurement is what any attempt to restore them has to answer.

#### Files Modified:

- api/tinydesk.api
- apps/gps.c
- include/tinydesk/td_sysinfo.h
- ports/twatchultra/main/gps.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 12 COMMIT Unreleased 2026-11-15T09:16:58-07:00

#### Coming From:

Unreleased 69c8d55

#### Purpose:

Add a GPS application that shows the user the receiver's fix state, position and satellite counts, since the receiver is qualified but only reachable from the shell.

#### Outcome:

The GPS app is in the start menu, the operator ran it and accepted it, and the cycle's real work was a portable seam rather than a window. THE APP CANNOT INCLUDE THE DRIVER, and that constraint set the shape of the change. `apps/` is shared by every port, so the app may not reach into `ports/twatchultra/main/gps.h`; Network reaches its platform through `td_sysinfo()->net` and this now does the same through a new `td_gps_ops_t`. The interface is three calls -- `start`, `stop`, `status` -- and the pair is deliberate: THE RECEIVER IS POWERED ONLY WHILE THE WINDOW IS OPEN, by operator decision, so opening the window is what raises the rail and closing it is what drops it. A GNSS receiver draws tens of milliamps and a watch has none to spare for a window nobody is looking at. The cost is that the first reading after opening is empty and a cold fix takes minutes, which is why the window renders `Satellites: 0 in view` within a second rather than appearing blank. The position is carried as `int32` in 1e-7 degrees rather than as a double, which keeps the shared struct free of floating point and loses nothing: the receiver sends four decimal places of arc-minute, about 1.7e-6 degrees, so a seventh decimal place is already more resolution than the data has. THE LINE THAT MAKES THE APP WORTH HAVING IS `Satellites: N in view, M used`, and the difference is the whole design. GGA's satellite field is the number USED in a solution, which is zero whenever there is no fix however much sky the antenna can hear -- and zero is also what a disconnected antenna reads. A receiver indoors is working perfectly and cannot fix, so a display showing only the used count reports a healthy receiver as broken. In view separates the two, which is the distinction GPS-002 already needed for `gps raw`. Parsing it needed a new sentence: GSV was counted and ignored before, and it is now decoded for its satellite count only. THE COUNT IS A SUM AND NEEDS A STALENESS RULE, both recorded in GPS-006. GSV restates each constellation's own count in every message of its series, so a multi-GNSS receiver emits four series and a sky figure is their total -- 18 to 24 on this board against 12 used. A constellation that stops being reported would otherwise contribute its last count for ever, so a talker not heard within 400 sentences, about half a minute at this receiver's 12 sentences per second, is dropped from the sum. This also explains a disagreement that would otherwise read as a bug: `gps raw` reports the MAXIMUM single-constellation count because that is what a per-sentence counter naturally produces, so the shell said `sats visible 3` while the app showed 18, and neither is wrong. TWO PRE-EXISTING RED TESTS WERE FOUND AND ONE WAS FIXED. `api_stable` failed because the new types are public API, which is the test working: the snapshot was re-recorded and the diff is additive only -- three new declarations plus one field appended to `td_sysinfo_t`. `board_keys` was ALREADY failing at `HEAD` and is unrelated to this cycle: commit `45b761e` added the `ssh.autostart` board key without recording it, so the key was recorded and described in the README's board-setting list, which is what that tool asks for. The suite is now 43 of 43. A FIELD WAS RENAMED BECAUSE ITS NAME LIED: `nmea_state_t::hdop_x10` held hundredths, so 92 meant 0.92, and the new shared struct would have carried the same trap into a second place; it is `hdop_x100` at all six sites, with the reason written beside it. THE RECEIVER IS EXCLUSIVE, which is a state the driver did not previously have to consider: the shell command and the app would each parse half of one UART's sentences and both would look faulty, so each refuses while the other holds the port and says which. Build PASS at 0x2cc090 bytes with 7 percent of the app partition free, sha256 1a1ade9b2fd84f63; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; host suite PASS 43 of 43 with the new GSV checks at 12 and confirmed able to fail by raising the staleness bound and watching the stale case report 18 where 11 was right; the board was rebooted and a full 16 MiB image archived with it held in the bootloader across read and verify: `working-2026-11-15-0912.bin`, 16,777,216 bytes, sha256 `910ccd311147b7f37750e5fce3350e60a266de4481aaf2ce179234786dd2b534`, `verify OK (digest matched)`, `ota_0` inside it compared byte-identical to the build. The operator's acceptance was on the glass with a real fix: `Fix: GPS`, `33.29387 N 111.88092 W`, `18 in view, 12 used`, HDOP 0.95, 375 m, UTC ticking. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched; GPS-006 was added to the reference, its index and the routing table. A NOTE ON METHOD WORTH KEEPING: an attempt was made to open the app from the host by parsing the start menu's colours and pressing Down repeatedly, which cost three rewrites of a screen model and never worked, because the desktop sends only changed cells and the model drifted. The operator opened the app with a tap in one second. That instrument should not have been built.

#### Next Steps:

THE APP SHOWS THE RECEIVER, AND THE RECEIVER'S OWN CONFIGURATION IS UNTOUCHED: GPS-003 records that this unit answers NMEA but not the UBX identification poll, so the constellation set is factory default and there is no way to select or disable talkers from the watch. Anything wanting NMEA rate changes, constellation control or a power mode has to establish the UBX side first, which is the next real question about this part rather than a UI task. The app does not yet offer `gps time`, which sets the clock from the receiver and is proven to survive a hard reset through the RTC (GPS-005); a button for it is small and would give the watch a clock source that needs no network. The window's own limits are recorded rather than measured-good: it is 38 columns by 12 rows at a fixed position, it shows one coordinate format regardless of the Settings date and clock preferences, and the receiver is dropped on close but not on shutdown, where the rail state is whatever the PMU decides. The exclusivity rule means the shell command cannot be used while the window is open, which is correct but is also the only way to see raw sentences; a diagnostic view inside the app that dumped talker counts would remove the need to close it. The wider backlog is unchanged: Pause, Reset and Save are labelled on all three emulators and send nothing, the Game Boy's right column has a third slot that is drawn furniture and could carry `gb save`, the BLE controller must be reconnected after every reboot, `pm.c` still duplicates the rasteriser `gamepad.c` owns, the NES owes battery-SRAM and its skip-path test and is parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase, and no DMG cartridge has ever been launched so the `GB_HW_CGB` compatibility path remains unproven. THE STATUS STRIP STILL OWES THE BATTERY AND THE YEAR, and the 446 ink pixel measurement in entry 11 is what any attempt to restore them has to answer.

#### Files Modified:

- CMakeLists.txt
- README.md
- api/board_keys.txt
- api/tinydesk.api
- apps/apps.c
- apps/gps.c
- apps/td_apps.h
- include/tinydesk/td_sysinfo.h
- ports/esp_idf/app/main.c
- ports/esp_idf/components/tinydesk/CMakeLists.txt
- ports/twatchultra/main/gps.c
- ports/twatchultra/main/gps.h
- ports/twatchultra/main/nmea.c
- ports/twatchultra/main/nmea.h
- tests/test_nmea.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 11 COMMIT Unreleased 2026-11-15T08:04:47-07:00

#### Coming From:

Unreleased 95b3198

#### Purpose:

Add the date and the battery to the always-visible status strip, so the operator can see more than the time without leaving an emulator.

#### Outcome:

The strip now reads the date and the clock, the operator accepted it on the glass, and the cycle cost two rejected layouts and two real defects before it did. THE FIRST LAYOUT WAS BUILT AND REJECTED ON THE GLASS, and the reason it had to exist is the measurement that governs the whole feature: the strip lives in glass rows 1..43, which is ABOVE the framebuffer, and `docs/panel-geometry.md` measures the corners as straight 45-degree cuts at `x + y = 80` and `(410 - x) + y = 90`. At row 10, where a scale-2 glyph block first puts ink down, the glass therefore spans only x 70..330 -- 261 px inclusive, not the 410 the file's own header comment had claimed. A host harness that compiled this file's real font array and its real placement arithmetic, then counted every ink pixel against those two lines, put numbers on every candidate: the full date with the battery and a 12-hour clock clips 446 ink px at a uniform scale 2, and the full date with the clock alone still clips 121. What shipped first was a compromise that fitted -- the clock at scale 2 and the date and battery at scale 1 -- and the operator's verdict was that it did not really fit, because three sizes on one line read as three separate strips sharing a row rather than one strip. THE SECOND LAYOUT IS THE CYCLE, and it removes the year and the battery by operator direction so that every field returns to the single scale this strip has ever been accepted at: `15-11   7:57 AM`, 224 px of the 261 available, clearing the chamfer by 24 px on the left and 21 on the right. Both fields still come from the Settings app through `td_clock_prefs()`, which is what the operator asked for and what the first version already did -- the date order renders `15-11`, `11-15` or `11/15` and the 12-hour form is padded with `%2d` to a fixed eight cells so an unpadded hour cannot shift the date 16 px at 10 o'clock every morning. TWO DEFECTS WERE SHIPPED AND CAUGHT, and both are recorded because the second was invisible to the build and to the operator's first report. The first was a double-applied scale: the font cell was defined as `8 * SB_SCALE` and each field's width was then multiplied by its scale again, so every width doubled, `sb_push_field()` refused the field against the buffer limit, and the strip silently never drew at all while the build passed and the flash verified. The strip's own `statusbar` command is what saw it, reporting `left=-16` and `pushes=0`; the constants are now split into an unscaled `SB_FONT_W`/`SB_FONT_H` and a derived `SB_GLYPH_W`/`SB_GLYPH_H`, with the reason written above them. The second was reported as `15-11--` and was NOT the formatter emitting dashes: the strip is centred as a group, so a field's x depends on the other field's width, and the placeholder clock `--:--` is five cells against the real clock's eight, so when the timezone resolved at boot the date moved 24 px left and each field was pushed only over its own new rectangle -- leaving the old placeholder's dashes where nothing would ever overwrite them. A host harness reproduced it exactly, 28 stale pixels in one character cell at x 168..183, before any code changed. The cure is that the line is now pushed as ONE rectangle covering the union of where it is and where it was, so vacated pixels are cleared by the same transfer that draws the new ones, which also makes each band a single window and a single pixel stream; the diagnostic gained the painted extent (`painted x 88..311 (w=224)`) because this fault class was otherwise unobservable. Build PASS at 0x2cb410 bytes with 7 percent of the app partition free, sha256 dbacc50a7d14d4ec; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the board was rebooted and a full 16 MiB image archived with it held in the bootloader across read and verify: `working-2026-11-15-0808.bin`, 16,777,216 bytes, sha256 `8d9766bc341a40b5d3c6196ac09d308737404c19b77c36cbcf327485b96bc12d`, `verify OK (digest matched)`, `ota_0` inside it compared byte-identical to the build. THE ARTIFACT WAS REBUILT AND REFLASHED AFTER THE OPERATOR'S PASS, and the reason is recorded rather than hidden: the comment above SB_GLASS_CX had said the glass at row 10 is 260 px wide where the inclusive count is 261, the same span-versus-count slip this entry's own prose carried, and correcting it also corrected `docs`-facing figures in the reference. A binary compare against the build the operator had tested showed the two differ in 70 bytes, all of them the app descriptor's build stamp and the trailing image SHA-256, and in no byte of code -- so the firmware on the glass is functionally the one that passed, the first image (`working-2026-11-15-0759.bin`, sha256 `c63393d9101c4889d3817bd93a4b19807762cf58ec7b06fddfed300d971b1674`) is retained, and the second was taken so the archive still matches the deployed build rather than leaving a comment fix as the one unflashed difference in the tree. TWO FLASH ATTEMPTS WERE KILLED MID-WRITE by another process on the development machine that globbed and opened every `/dev/ttyACM*` in a loop, which presents as `device reports readiness to read but returned no data` and leaves a partial image; the chip answered `read_mac` in the bootloader each time and a clean reflash succeeded, and the port was pinned by `/dev/serial/by-id` rather than by index afterwards because a TinyTang board and a SIPEED debugger both appeared on this machine during the cycle. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` and `.ai/core-reference.md` the only `.ai/` files touched; LCD-007 in the reference was revised to carry the three measured chamfer-line equations it had omitted, which is the fact this cycle was missing when it was first written.

#### Next Steps:

THE BATTERY AND THE YEAR ARE OWED AND THE ARITHMETIC IS WHY THEY ARE NOT HERE, so a future attempt has to answer the 446 px measurement rather than rediscover it: the strip has 261 px at its binding row, and the year is four more cells of the date plus the battery's four, which cannot both be added without shrinking the clock, moving a field to a second row, or accepting the mixed-scale layout the operator rejected on the glass. The `statusbar` command reports `date`, `clock`, `utc`, `tz`, `have`, `fmt`, `12h`, its push count, the painted extent and the driver's total, and should stay until the content is settled. THE STRIP SHOWS DASHES FOR UP TO THIRTY SECONDS AFTER A REBOOT and that is `get_tz()` in `main.c` caching a pre-login failure, not the strip: entry 10 records why an immediate retry was tried there and reverted, so the cure has to be a first-read that does not depend on `td_session_home()` being populated, and it is unfixed. The 12-hour clock's `%2d` padding keeps the geometry still but widens the gap to its left at single-digit hours, which the operator has not complained about and may; centring the text inside its fixed box is the change if it reads as sloppy. The wider backlog is unchanged: Pause, Reset and Save are labelled on all three emulators and send nothing, the Game Boy's right column has a third slot that is drawn furniture and could carry `gb save`, the BLE controller must be reconnected after every reboot, `pm.c` still duplicates the rasteriser `gamepad.c` owns, the NES owes battery-SRAM and its skip-path test and is parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase, and no DMG cartridge has ever been launched so the `GB_HW_CGB` compatibility path remains unproven.

#### Files Modified:

- ports/twatchultra/main/statusbar.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 10 COMMIT Unreleased 2026-11-15T01:01:00-07:00

#### Coming From:

Unreleased bf5b5c3

#### Purpose:

Add a permanently visible status strip in the 44 glass rows above the framebuffer, showing the time where no emulator can cover it, and fix the defects that surfaced while doing it.

#### Outcome:

The strip works, the operator accepted it, and getting there exposed a stack overflow that had been rebooting the watch every thirty-three seconds while wearing the appearance of a display bug. THE STRIP ITSELF IS SMALL AND THE BUDGET IS THE DESIGN: it occupies rows 1 to 43, because row 0 is swallowed by the controller (LCD-004, measured with a 2 px ring) and row 44 already belongs to `paint_column()`, which programs its window one row up as a sacrificial row for the same reason. Inside that band an 8x16 glyph at scale 2 is 32 rows tall, leaving five above and six below, and the text is 80 px wide against the glass's 410, so the clock sits centred with 165 px of margin either side. The whole persistent cost is one 1,280-byte band buffer. IT NEEDED A NEW PUSH PATH, AND THAT PATH IS A DELIBERATE EXCEPTION: `display_panel_rows()` refuses unless an app owns the pixels, which is exactly backwards for furniture that must survive an emulator, so `display_panel_push()` was added that ignores ownership. It is safe without a lock for three reasons that already existed in the file rather than being promised now -- `push_pixels_raw_buf()` holds the SPI bus across the window write and the pixel stream so each band is atomic, the caller supplies its own buffer so there is no `s_tx_app` race with `gamepad_paint()`, and it touches no framebuffer, no `s_vt` and no cell record. THE STACK OVERFLOW IS THE CYCLE AND IT IS WORTH THE WHOLE ENTRY. The task was created with 3,072 bytes on the reasoning that it does almost nothing -- one `snprintf` and a pixel push a minute. It panicked instead: `***ERROR*** A stack overflow in task statusbar has been detected.` The push is not cheap; `display_panel_push()` reaches `push_pixels_raw_buf()`, which puts an `spi_transaction_ext_t` and a bus acquisition on the stack and calls the SPI driver's polling transmit, and that path alone is deeper than three kilobytes. 8,192 is double the failing size, which is the cheap side of the trade because a once-a-minute task cannot make 5 KB matter and a stack overflow is a reboot rather than an error. THE FAILURE WAS INVISIBLE AND THAT IS THE REAL LESSON. This firmware builds with `CONFIG_ESP_CONSOLE_NONE=y` and `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y`, so a panic prints nowhere and reboots in zero seconds. What the operator saw was the screen flashing, and then the strip's clock reverting from local time to UTC after a minute or two -- which read as two writers fighting over one value. It was one writer and a board restarting underneath it: every reboot re-ran the first-timezone read in `link_init()`, before the desktop had logged in, so `td_session_home()` was still empty, the path was `/.tdsh_tz`, and the read failed. THE DIAGNOSIS TOOK THREE EXPERIMENTS AND THE ORDER IS THE POINT. Watching the serial port for bursts over 200 seconds found six, at 33-second intervals, which is not a repaint -- it is a boot loop. Disabling the strip entirely produced zero bursts in 100 seconds, which named the culprit. Re-enabling the console and rebuilding captured four words of panic text that named the mechanism. Only then was the stack changed. BEFORE THAT, TWO CHANGES HAD BEEN MADE ON A HYPOTHESIS AND BOTH WERE WRONG, and they are recorded because the pattern matters more than either fix. `get_tz()` was first changed to retry immediately after a failure, and then again to report failures honestly; the retry version was reverted because it turned one failed file open into an open per tick forever, on a function that sits on the taskbar's paint path. The honest-failure half was kept and is a genuine defect fixed: `get_tz()` used to return TRUE while handing back 0, so "I could not find your time zone" and "your time zone is UTC" arrived as the same answer, and anything displaying the offset showed UTC and called it success. `*seconds` is still filled in on failure so existing callers are unaffected, but a caller that wants to distinguish the two cases now can. The strip additionally refuses to substitute zero for an unknown offset: it shows `--:--`, the same rule this project applies to the RTC, and retries every five seconds until it has a value and once a minute after. THE PATTERN IS THE THIRD INSTANCE IN ONE SESSION and it is named here for the next agent: `paint_column`'s swapped chrome index, the `rot_k` comment that claimed 65536 was upright, and now `get_tz` on a guess. Each was a change to shared code made before the instrument that would have named the fault. The two that went well were both measured first -- the arrow orientation and the glyph size were confirmed by compiling the real font array and the real placement arithmetic on the host and printing pixels before any flash. Build PASS at 0x2cad80 bytes with 7 percent of the app partition free, sha256 a43a6a96d898ab17; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the board then ran 180 seconds with the production console configuration and produced no reboots where the previous build produced six in 200. The board was rebooted and a full 16 MiB image archived with it held in the bootloader across read and verify: `working-2026-11-15-0057.bin`, 16,777,216 bytes, sha256 `7399c009036422cd5d0e5126f6fc1be101212ec9614696bce0d521e8099a8c15`, `verify OK (digest matched)`, `ota_0` inside it byte-identical to the build. The FIRST backup attempt at 00:53 read to 16 percent and failed with `Corrupt data, expected 0x1000 bytes but received 0xfc3 bytes`; the script refused to call it a backup and left no file behind, and the retry after a reboot and a check for competing access succeeded, so the corrupt read is attributed to port contention rather than to the image. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched.

#### Next Steps:

THE STRIP SHOWS THE TIME ONLY. The operator's stated intent was time, connectivity and anything else that has to survive an emulator, so the next duties are the obvious ones: network state from `td_sysinfo()->net`, battery level, and whatever else is worth a permanent pixel. Each is a text field in the same band and a `putc` against the same buffer, so the layout is the work rather than the plumbing. A PANIC WITH THE CONSOLE OFF IS STILL INDISTINGUISHABLE FROM A DISPLAY FAULT, and that cost most of this cycle; `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT` with `CONFIG_ESP_CONSOLE_NONE` should either print to something, be caught by a restart counter, or be documented where an agent will find it before chasing a flash. The strip's own `statusbar` command reports `text`, `utc`, `tz`, `home`, the tz file path, its push count and the driver's total push count, and it was what proved the timezone defect; it should be kept until the strip's content is settled. `display_panel_push()` is the first push path that ignores `display_pixels_claim()` and any second user of it deserves the same reasoning written down. No other cartridge has been run in the Game Boy port, so the DMG path is still unproven. The wider backlog is unchanged: Pause, Reset and Save on all three emulators, the right column's third slot being drawn furniture that could carry `gb save`, the BLE controller reconnecting after every reboot, `pm.c` still duplicating the rasteriser `gamepad.c` owns, the NES owing battery-SRAM and its skip-path test, and the NES parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase.

#### Files Modified:

- ports/esp_idf/app/main.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/statusbar.c
- ports/twatchultra/main/statusbar.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 9 COMMIT Unreleased 2026-11-14T23:32:00-07:00

#### Coming From:

Unreleased 9de59f7

#### Purpose:

Fix the four arrow glyphs on the desktop's right column, which pointed the wrong way and appeared in the wrong order, double their size, and rename two labels in the System Monitor.

#### Outcome:

The arrows now read left, right, up, down from the top and are twice the size, and the operator accepted both; the System Monitor's two memory rows are renamed. THE ARROW FAULT WAS ONE ROTATION WEARING TWO SYMPTOMS, and the operator's report described both without knowing they were the same thing: the order looked wrong AND the glyphs looked wrong. The label table has always listed `U+2190`, `U+2192`, `U+2191`, `U+2193` -- left, right, up, down -- in slot order, so the table was never the problem. `paint_column()` draws every label through `label_bit()`, which for a rotated column transposes the 8x16 bitmap so the glyph's rows become the pixel's columns; that is exactly right for a word, because "Super+D" has to stand on end to fit a 41 px face, and exactly wrong for an arrow, because rotating an arrow does not make it stand on end, it makes it point somewhere else. Measured against the transposition: leftward maps to cell-down so it appears as a down arrow, rightward to cell-up so it appears as up, upward to cell-left so it appears as left, and downward to cell-right so it appears as right. Top to bottom the column therefore read down, up, left, right -- which is precisely what the operator saw. THE OBVIOUS FIX IS THE WRONG ONE, which is why this is worth an entry: swapping the table entries to compensate would have made the table a lie about a rendering bug, and the next person to read it would have been misled in exactly the way this project has been misled by the `rot_k` comment that claimed 65536 was upright. The fix belongs in the renderer, and it is a rotation argument rather than new code, because `label_bit()` already implemented `LABEL_UPRIGHT` in full: a label that is a single character above U+007F -- the four arrows and nothing else in any column -- is drawn upright whatever the column does with its words. IT TOOK TWO ATTEMPTS AND THE FIRST DID NOTHING AT ALL. That version tested `label_is_glyph(label[s].cp, label[s].n)` as an argument to the very `label_place()` call that fills those fields, so it read uninitialized stack, returned false almost always, and produced a build that looked identical on the glass -- which the operator reported, because a comment claiming a fix is not a fix. The build succeeded and reported no warnings both times. The correction classifies from the STRING, which exists before the call, and the classifier was then checked against every label in the tree: all four arrows return upright, all nine words stay rotated, and a two-arrow string correctly stays rotated. A HOST SIMULATION IS NOW THE STANDARD PRACTICE FOR THIS KIND OF CHANGE and it is what should have been done first: the actual font arrays and the actual `label_place()`/`label_bit()` arithmetic, compiled on the development machine, printing the resulting pixels. That output is what confirmed the order and the orientation before the second flash, and the same harness confirmed the doubled arrows afterwards. THE DOUBLING IS A SCALE PARAMETER OF `label_place()`, NOT A FIELD SET AFTERWARDS, for the reason the first attempt taught: a value written after a call that already read it is a bug that compiles and ships. `label_bit()` divides the pixel offset by the scale before the cell arithmetic, so each source pixel becomes a 2x2 block -- edges stay on whole pixels and stroke weight is unchanged, where a fractional or bilinear scale of an 8x16 bitmap reads blurred. At scale 1 both functions reduce term for term to what was there before, so the bottom strip's degree circle and every word are untouched. A doubled arrow is 16 by 32 in a face of about 41 by 61, so it fits with room and remains centred on its button's axis rather than growing off one edge. THE SYSTEM MONITOR RENAMED TWO ROWS at the operator's direction: `Internal RAM` became `Internal SRAM` and `  peak use` became `SRAM Peak Use`. The first is a correctness point rather than a preference, because the reading sits one line above `PSRAM` and two rows labelled RAM and PSRAM read as the same pool at different speeds, where SRAM against PSRAM reads as the distinct pools they are with different pressure and different failure modes. Both new strings are exactly 13 characters, the width the label already had, so the progress bars at column 14 and the figures at 33 do not move; the second row lost its two leading spaces because it was indented as a continuation of the row above and is now a full label. THE IMAGE WAS GREPPED RATHER THAN ASSUMED. The binary came back byte-identical in size to the previous build, which was suspicious enough to check: both new strings are present in it and the old indented one is absent, and the identical size is string-table alignment absorbing the difference. `Internal RAM` does still appear twice in the image, and both were traced to terminal messages rather than the app -- `tdsh_commands_espidf.c`'s `free` output and `tdsh_ssh.c`'s start-up report -- so the app label changed and the two command outputs did not, which is recorded in Next Steps as an inconsistency rather than left to be discovered. Build PASS at 0x2ca430 bytes with 7 percent of the app partition free, sha256 e42dff3dbf34d77e; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the board was rebooted and came back up. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched.

#### Next Steps:

This cycle took no 16 MiB backup, so the last verified image remains `working-2026-11-14-2259.bin` from entry 8. The System Monitor now says "Internal SRAM" while the `free` command in `tdsh_commands_espidf.c` and the SSH server's start-up report in `tdsh_ssh.c` both still say "Internal RAM" for the same pool, which is the mild form of a label disagreeing with its neighbour rather than a lie -- two strings, when it is wanted. `label_bit()` has only ever been exercised at scale 1 and 2; scale 3 would give a 24 by 48 arrow and still fit the vertical face, and any non-integer scale is unsupported by construction. The doubled glyph is a 2x2 block scale, so a future font with anti-aliased edges would not render through it unchanged. The wider backlog is unchanged: Pause, Reset and Save on all three emulators, the right column's third slot being drawn furniture that could carry `gb save`, the BLE controller reconnecting after every reboot, `pm.c` still duplicating the rasteriser `gamepad.c` owns, the NES owing battery-SRAM and its skip-path test, and the NES parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase.

#### Files Modified:

- apps/sysmon.c
- ports/twatchultra/main/display.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 8 COMMIT Unreleased 2026-11-14T23:03:00-07:00

#### Coming From:

Unreleased 46d4906

#### Purpose:

Turn the Game Boy's d-pad upright and widen its touch area, move the A button, and fix the display fault that had silently swapped the two side columns' chrome state since the margin painter was written.

#### Outcome:

The operator accepted the result on the glass and this cycle also produced the first real fault found in shared display code rather than in one app. THE D-PAD BECAME UPRIGHT, AND THAT WAS NOT A CONSTANT CHANGE. `gamepad_t` carried a single `rot_k` documented as "16.16 cos of the rotation, 65536 is upright", but the transform was `u = ((dx + dy) * rot_k) >> 16` and `v = ((dy - dx) * rot_k) >> 16` -- both expressions ARE a 45-degree rotation, so the constant could only scale the diamond. Setting 65536 drew a smaller diamond, not a cross, and the comment asserting otherwise had been wrong since the module was factored out of `pm.c`. A real rotation needs both terms, so the layout gained `rot_s`: `u = dx*rot_k + dy*rot_s`, `v = dy*rot_k - dx*rot_s`. At 45 degrees the cosine and sine are equal, so the new expression collapses to the old one bit for bit and the NES and Pokemon Mini are unchanged -- but both now state `rot_s = 46341` EXPLICITLY, because the field defaults to zero and a zero sine with a 65536 cosine is an upright pad. That default is the reason the two existing layouts were edited in this cycle; leaving them alone would have silently un-rotated both. `gamepad_hit()` had the same assumption baked in -- it compared |dx+dy| against |dy-dx|, the diamond's own frame -- so an upright painted cross would have been hit-tested as a diamond, with painter and hit test disagreeing about which arm is where. It now decides direction from `cu = dx*rot_k + dy*rot_s` and `cv = dy*rot_k - dx*rot_s` before the shift, which at 45 degrees is the identical comparison, and its tie rule `dx == 0` generalises to `cu == cv`, again exactly equivalent. THE TOUCH AREA WAS TRIED AS A RECTANGLE AND REJECTED. A box running from the glass's left edge to the disc's former right reach and from ten rows below the picture to the glass's bottom was implemented, drawn and measured -- 197 by 163 -- and the operator's verdict was that the circle was better, so the box was removed rather than left as an option. The disc came back with `dpad_hit_r` set to 94 against a default of 78, which is a 20 percent wider RADIUS rather than a 20 percent larger area: a thumb's reach is a length, so the linear reading is the one meant. The box's removal was complete -- the field, the predicate, the hit-test branch, the diagnostic that outlined it, and the two radius accessors that existed only for that diagnostic all left the tree, and the build shrank from 0x2ca850 to 0x2ca360, which is how the dead code was confirmed gone rather than merely switched off. A `.dpad_hit_r` of 94 costs ten rows of picture: the disc's top is framebuffer 278 and the guest's last row is 287, so a narrow band along the bottom of the screen can register Up. That is the same trade the circles have always made and it is recorded rather than hidden. THE FAULT WORTH THE ENTRY is in `display.c`, not in the Game Boy port. `paint_column()` read the chrome state with its indexes swapped -- `s_nochrome[desk_right ? 1 : 0][s]`, so the LEFT column consulted the RIGHT column's state -- while `display_margin_chrome()` wrote `[col][index]`, `display_margin_slot_live()` read `[col][index]`, and `s_lab_dy[desk_right ? 0 : 1]` four lines away in the same function used the documented convention. For the Pokemon Mini nothing changed, because its two bare-slot tables are identical. For the NES and the Game Boy it meant painting and liveness disagreed about which slots were bare, which is exactly the label/duty mismatch `edges.h` exists to prevent. IT PRESENTED AS TWO SYMPTOMS THAT LOOKED LIKE SEPARATE BUGS, AN HOUR APART, AND BOTH WERE THE SAME SWAP. First the operator reported that the outline around the left column's Save had disappeared when the Game Boy's controls were drawn on the glass: the left column was reading the right column's state, the Game Boy blanks its right slots 2 to 4, so left slot 2 read `true` and was painted bare. Correcting the index then made the RIGHT column's third button lose its box, because that box had only ever been drawn BY the bug -- `gb.c` had asked for that slot to be bare all along, and the swap was quietly overriding the request by reading the left column's slot 2, which carries Save and a box. So the fix for the second symptom was to request the box explicitly in `gb.c`, with a comment saying it is furniture rather than a duty: the Game Boy has only two margin controls, Start and Select, the third slot keeps the column reading as the same three-button stack as the other two emulators, and `pad_from_frame()` still answers only for slots 0 and 1 so a box that does nothing cannot fire. The general lesson is the one this project keeps re-learning from a different direction: two callers agreeing by accident is indistinguishable from two callers agreeing by design, and the giveaway was a neighbouring line in the same function using the opposite convention. ALSO IN THIS CYCLE: the A circle moved four pixels down at the operator's direction, to framebuffer y 296, and a `gb mark on|off` diagnostic was built that outlined every touch-sensitive region -- the d-pad's accept disc, both circles' 1.5x discs, and the two live column slots -- compositing the guest picture underneath so the rings did not punch holes in the game, and staying clear of the LCD-002 trap by using three-pixel bars rather than one-pixel rules. It did its job, the operator confirmed the regions were where intended, and it was removed at their direction once it had answered the question. A cross-emulator check followed the shared change, and the FIRST ATTEMPT AT IT WAS WORTHLESS AND IS RECORDED AS SUCH: it filtered output for matching words without clearing the screen first, so it matched a stale `gb status` line already on the glass and printed the same figures for all five commands. The second attempt cleared the console before each command and showed the Pokemon Mini, the NES and the Game Boy each starting and stopping, with the Game Boy reloading all four SRAM banks afterwards. Build PASS at 0x2ca390 bytes with 7 percent of the app partition free, sha256 5de6e41cb9bc7006; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the board was rebooted and a full 16 MiB image archived with it held in the bootloader across read and verify: `working-2026-11-14-2259.bin`, 16,777,216 bytes, sha256 `9d6aa2e120aea39abae65bcacd8c8ed1edec4e5bd347d136ca6c3829fd2b3faa`, `verify OK (digest matched)`, `ota_0` inside it compared byte-identical to the build. The required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched.

#### Next Steps:

The right column's third slot is drawn and does nothing, which is honest but is also an opportunity: Save is labelled on the left column and sends nothing, and this port has a working save path behind it, so wiring that slot to `gb save` would give the column a third real duty and retire a piece of furniture at the same time. `gb mark on|off` was removed after it answered its question; anyone wanting to re-derive the touch geometry should note that it composited the guest picture underneath and used three-pixel bars for the LCD-002 reason, so it is a rebuild rather than a flag. The d-pad's 20 percent radius reaches ten rows into the picture and A's disc reaches fourteen, and both are accepted rather than measured-good; if either steals a press in play, `GB_DPAD_HIT_R` and `GB_BTN_HIT_D` are one value each. The upright transform is new and only the Game Boy uses it, so `rot_s` has never been exercised at an angle other than 0 or 45 degrees -- a layout asking for 20 degrees would be the first real test of the general case. No cartridge other than Link's Awakening DX has been run, so the DMG compatibility path of the `GB_HW_CGB` machine remains unproven. The wider backlog is unchanged: Pause, Reset and Save on all three emulators, the BLE controller reconnecting after every reboot, `pm.c` still duplicating the rasteriser `gamepad.c` owns, the NES owing battery-SRAM and its skip-path test, and the NES parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/gamepad.c
- ports/twatchultra/main/gamepad.h
- ports/twatchultra/main/gb.c
- ports/twatchultra/main/nes.c
- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 7 COMMIT Unreleased 2026-11-14T22:21:00-07:00

#### Coming From:

Unreleased 4403a01

#### Purpose:

Give the Game Boy port battery-backed save RAM that survives leaving the emulator, and the drawn d-pad and action buttons the other two emulators carry, so a cartridge can be played rather than looked at.

#### Outcome:

Both landed and the operator confirmed them on the glass: "i ran around and saved. input works good" and then, after stop and restart, "its there. it loaded just fine. pass." SAVE RAM NEEDED NO NEW EMULATION, ONLY A PLACE AND A MOMENT: `gnuboy_load_sram()` and `gnuboy_save_sram()` were already in the fork and unused, reading and writing the eight kilobyte banks the mapper exposes plus the MBC3 time block. What the port supplies is a path derived from the ROM's own name into `saves/gbc/` on the card, so one cartridge gets one file and renaming a ROM abandons its save, and a load placed AFTER `gnuboy_reset(true)` -- which is not a detail but the whole correctness of the feature, because that call does `memset(cart.rambanks, 0xff, ...)` and zeroes `cart.rtc`, so a load placed before it is read from the card, reported as restored, and then erased. THE FIRST VERSION WORKED AND WAS WRONG, WHICH IS WHY IT SHIPPED FOR A WHILE: it called `gnuboy_save_sram()` inline from the guest task, and measurement put that at 127 ms for 32 KB on a 16.7 ms frame. The cause is not in this repository's emulator code -- `tdsh_sdcard.c` mounts the card over SPI at `SD_MAX_KHZ` 10 MHz rather than through SDMMC, which lands the achieved throughput near 270 KB/s -- and against the 93 ms audio ring a 127 ms stall drops seven frames and underruns the speaker every time a game saves. So the work was split at the point where the expensive part begins: `gnuboy_sram_snapshot()` copies the banks into one of two PSRAM slots in 2 ms on the guest task, which is the only code that ever reads them, and a separate task does `fopen`/`fwrite`/`fclose`. The accessor was added to the CORE rather than reimplemented in the port deliberately, so the snapshot and the file writer cannot drift apart and produce saves the loader cannot read. TWO BUGS IN THAT REDESIGN, BOTH FOUND BY MEASUREMENT RATHER THAN BY READING. FIRST, THE SNAPSHOT DID NOT CLEAR THE DIRTY FLAG, because only `gnuboy_save_sram()` did, so every save re-fired at the next opportunity and `gb stop` wrote the file a second time -- which is how it was noticed, from a `gb: saved` line on a shutdown that had nothing left to save. The flag is now cleared at the moment of the copy, in the thread that owns the banks, so anything the cartridge stores during the 127 ms transfer correctly marks it dirty again for the next save; clearing after the write completed would have silently dropped whatever landed in between. A failed write calls the new `gnuboy_sram_mark_dirty()` so it is retried instead of forgotten. SECOND, THE WRITER TASK AT PRIORITY 3 WAS STARVED BY THE BLITTER AT 4 and the same 32 KB took 243 ms instead of 127, doubling the freeze for no gain; at priority 6 the write finishes in 127 ms and the blitter is the one that waits, which costs painted frames rather than emulated ones. Final figures: `gb save` reports `saves=1 write=127ms snap=2ms`, the counter stays at 1 across twenty-five idle seconds, idle frame rate is 59.7 fps, and the file on the card is 32,768 bytes, exactly four banks. THE CONTROLS COPIED THE POKEMON MINI'S FURNITURE BUT COULD NOT COPY ITS SIZES, and the arithmetic is the reason. `gamepad.c`'s cross paints about 0.471 of its frame each way once rotated, so the NES's 217-row band takes a 168 px frame and two 84 px circles with a 32 px gap, 200 rows in all. This port's picture is 288 rows tall because it scales a 144-line screen 2x, leaving 169 rows below it -- less than the NES cluster itself. Measured against the band, 148 px paints about 142 rows and leaves 13 clear above and below, and two 74 px circles with the gap cut to 12 need 160 of the 169. At the NES's 168 the frame touches the picture; two 84 px circles do not fit at all. HORIZONTALLY NOTHING MOVED: the d-pad's painted tip still lands on the left column's axis and A and B sit at framebuffer 236 and 208 on all three emulators, so a thumb that has learned one has learned the others. The column now carries only what has no drawn equivalent, Start and Select, with every slot below them and the whole bottom strip blanked and `display_margin_chrome()` off, because blank glass that fires a button is a fault this project has reported twice. Chrome is restored on `gb stop` as well -- that state lives inside the display driver and both the app and desktop paint paths read it, so a slot left bare would return as a desktop button with no box. ONE COMPROMISE IS RECORDED RATHER THAN HIDDEN: A's 1.5x touch disc, 111 px centred at framebuffer y 329, reaches fourteen rows into the bottom of the picture, so a press at the very bottom of the screen can register as A. The NES accepted the same character of spill at five rows from the same disc; it is larger here because the band is tighter, and the operator has not reported being bothered by it. The d-pad's disc is clean, landing at framebuffer y 294..450 entirely below the picture. Build PASS at 0x2ca340 bytes with 7 percent of the app partition free, sha256 e931d40ccf52ad23; deployment PASS by app-only flash to 0x10000 with `Hash of data verified`; the required core-syntax audit was run against this entry before commit and passed, with `.ai/core.md` confirmed unchanged and `.ai/core-log.md` the only `.ai/` file touched.

#### Next Steps:

This cycle took no reboot and no 16 MiB backup, so the last verified image remains `working-2026-11-14-2128.bin` from entry 6 and the firmware here is committed and rebuildable but not archived. The save has been proven across `gb stop` and `gb start`, which covers leaving the emulator but not losing power, and a full reboot is the test that separates those. A's touch disc reaching into the picture is the one known wart and shrinking `GB_BTN_HIT_D` from 111 to about 90 is the fix if it ever proves annoying in play, at the cost of thumb comfort. Autosave is a fixed twenty seconds after the cartridge marks RAM dirty, which suits a player who saves and puts the watch down and is arbitrary for every other case; the cartridge gives no signal that a save is finished, so a longer window or a save-on-idle are the alternatives. No cartridge other than Link's Awakening DX has been run -- no other Game Boy Color title and no DMG cartridge at all -- so the DMG compatibility path of the `GB_HW_CGB` machine remains unproven and is the larger of the two unknowns. The left column reads Pause, Reset and Save and sends nothing, the same debt the NES and Pokemon Mini carry and the same disagreement `edges.h` exists to prevent. The wider backlog is unchanged: Pause, Reset and Save on all three emulators, the BLE controller reconnecting after every reboot, `pm.c` still duplicating the rasteriser `gamepad.c` owns, the NES owing battery-SRAM and its skip-path test, and the NES parked undecided with `docs/nes-accuracy.md` as its baseline and its page 15 crash as the first thing to chase.

#### Files Modified:

- ports/twatchultra/components/gnuboy/gnuboy.c
- ports/twatchultra/components/gnuboy/gnuboy.h
- ports/twatchultra/main/gb.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

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
