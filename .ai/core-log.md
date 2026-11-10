## 37 COMMIT Unreleased 2026-11-10T15:29:30-07:00

#### Coming From:

Unreleased 8eebe06

#### Purpose:

Drive the touch controller from its interrupt instead of polling it, and in doing so find out why no GPIO interrupt on this board had ever worked.

#### Outcome:

Touch is interrupt driven, and the reason it could not be is a port defect that had been silently costing every driver its interrupt since the beginning. The intended change was small: `cst92xx` now requests TP_INT as a GPIO with `int-gpios = <&gpio0 12 GPIO_ACTIVE_LOW>`, turns it into an IRQ with `gpiod_to_irq()` rather than through an `interrupts` property, and keeps polling at two rates -- 20 ms while a finger is down and 250 ms when idle as a safety net -- so that a wrong polarity or a lost pull-up degrades to latency rather than to no touch at all. What blocked it was that `gpiod_to_irq()` returned `-ENXIO`. Mapping a spread of lines in one boot settled the shape of the fault: lines 1 and 7 returned IRQs 7 and 8 while lines 0, 6, 12, 16 and 17 all returned `-ENXIO`, so the discriminator was not the line but whether something had already claimed it. That pointed away from the GPIO controller and at the allocation itself. The cause is in `arch/xtensa/include/asm/irq.h`: with `CONFIG_SPARSE_IRQ` unavailable on xtensa, the descriptor table is sized `NR_IRQS = XCHAL_NUM_INTERRUPTS + PLATFORM_NR_IRQS + 1`, which on this core is 32 + 0 + 1 = 33. That is right for peripherals hanging directly off the CPU's interrupt lines and wrong for this SoC, where the GPIO controller maps one Linux IRQ per GPIO line a driver asks for and the AXP2101 adds a virtual domain on top. The table was full. `irq_create_mapping()` returned ENOSPC, `gpiod_to_irq()` turned it into `-ENXIO`, and every driver that treats its interrupt as optional fell back to polling and worked -- so the visible symptom was "this board's GPIO interrupts do not work" rather than "the table is full". It also explains a detail that had been recorded but not understood: both of the interrupts that do exist, the RTC's and the PMU's, show zero counts in `/proc/interrupts`, because mapping a line is not the same as its parent firing. Two false leads were pursued and discarded on evidence rather than assumption, both worth recording. The first was that `gpio-esp32.c` lacks a `xlate` callback; it does not need one, because gpiolib supplies `gpiochip_domain_ops` with `irq_domain_xlate_twothreecell`, which accepts two cells, and the RTC's line mapping proves it works. The second was `CONFIG_PLATFORM_NR_IRQS`, which would have been the natural knob; it is a hidden symbol with no prompt, so `olddefconfig` recomputes it straight back to 0, and the config check in the build script caught that rather than letting it through. The fix is therefore a larger table in `patches/kernel/irq/final/irq.h`: `EXTRA_NR_IRQS 128`, about 18 KB of image and roughly 24 KB of `.bss` against 7.8 MiB of RAM, carried whole because nothing else in the tree touches that header and a patch would have to track it forever. Verified on hardware: `gpiod_to_irq()` now returns IRQ 33 -- the first number past the old limit -- the driver reports "interrupt driven", `/proc/interrupts` gains `33: esp32xx-gpio 12 cst92xx`, and 331 interrupts accumulated across three taps while a captured event stream showed three presses, three releases and smooth coordinate tracks including `(174,254)` through `(214,393)` and `(258,428)` through `(291,441)`. Idle rate measured at 20 interrupts in 10 seconds, against the ~50 reads per second the previous unconditional 20 ms poll generated. The bus, RTC, PMU, panel, WiFi and BLE keyboard all remained healthy throughout, with the RTC reading 22:29:09 against a seven-byte register read of `0x09 0x29 0x22`. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 72 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 36, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

A full 16 MiB backup should be taken: `working-2026-11-10-1444.bin` contains working touch but predates the interrupt work, so no saved image has interrupt-driven touch or the enlarged interrupt table. The interrupt is now the wake-up path rather than the only path, which is the safe shape, so the next question for touch is not whether it works but whether the 250 ms idle poll can be raised much higher, making the interrupt demonstrably the sole source of idle discovery; the driver already takes `hynitron,idle-poll-ms` as a property for exactly that experiment, and it was not run in this cycle. Two smaller open items remain. The absent-address `-ETIMEDOUT` versus `-ENXIO` defect still makes `i2cdetect` report timeouts for empty addresses. And the RTC's and PMU's own interrupts, now that the table has room and the mapping is understood, are worth revisiting: both are mapped and neither has ever fired, which was previously attributed to the hardware and may simply be the same class of problem. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/touch/final/cst92xx.c
- patches/kernel/irq/final/irq.h
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- scripts/build-kernel.sh
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 36 COMMIT Unreleased 2026-11-10T14:48:04-07:00

#### Coming From:

Unreleased 6af4934

#### Purpose:

Capture a full 16 MiB backup of the first state in which touch works on the hardware I2C bus, taken from a cold boot rather than a warm one.

#### Outcome:

`~/twatch-backup/working-2026-11-10-1444.bin` is the current recoverable state, 16 MiB read at 628.8 kbit/s in 213.5 seconds, sha256 `1a2637f5f9c781fe3dc0a19476fbb47550d019a10f4b428658e17d1fccd17c5a`. It is the first backup that contains working touch over the hardware bus, and it closes the gap noted at the end of the previous cycle, where no saved image had either the I2C recovery fix or `cst92xx`. The watch was rebooted first and the subsystems were re-verified before the read, so the image is a cold-boot state rather than a warm one: `cst92xx` bound as `CST9220 at 0x1a, 410x502, fw 0x01000003` and registered `cst92xx-touch` as input2, `rtc0` read 21:44:04 against a seven-byte register read of `0x05 0x44` (21:44:05), WiFi was at 10.10.0.15 and the BLE keyboard was running. The image was verified four ways rather than by the script's own OK line, because a backup that silently contains the wrong kernel is worse than none and this project has already lost a session to exactly that. The stored `.sha256` matches an independent `sha256sum`; the 2841808-byte kernel extracted from `0x140000` is byte-identical to the built `xipImage` (`d47e2cc3…`), proving the right kernel is in the right slot; the image contains the `hynitron,cst92xx` compatible string and the recovery format string `%s: int_raw=%08x sr=%08x busy=%u` together with the `ARB_LOST` literal, proving both the touch driver and the I2C fix are inside it; and the running watch reports `7.2.4-forkbank-g0a3083746891-dirty`, the same build string the image carries. One detail worth recording for the next person who greps an image for evidence: the recovery log line does not contain the literal `NACK:`, because it is formatted from `%s` with `"NACK"` or `"ARB_LOST"` selected at the call site, so searching for `NACK:` in a kernel image returns nothing even when the code is present. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 35, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The next kernel change now costs a four-minute restore rather than a session, which matters because the remaining touch work is the one part likeliest to need iteration. That work is interrupt-driven operation: `TP_INT` is GPIO 12 and LilyGo attaches a falling-edge handler to it, which would replace the 20 ms poll and reduce both bus traffic and the NACK rate. It was deferred deliberately again, because the polling path is what proved the protocol and an unverified interrupt would mix two unknowns. A second candidate, smaller and independent, is the absent-address `-ETIMEDOUT` versus `-ENXIO` defect, which still makes `i2cdetect` report timeouts for empty addresses; it is noted in the driver as an open item and does not affect any working path. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 35 COMMIT Unreleased 2026-11-10T14:40:11-07:00

#### Coming From:

Unreleased 4b83f00

#### Purpose:

Replace the retired userspace touch daemon with a kernel input driver, written from LilyGo's own source rather than from the earlier register map, and fix the I2C recovery defect that had been wedging the whole bus.

#### Outcome:

Touch works as a kernel input driver, and the bus fault that had been blocking it is fixed. The driver is `patches/kernel/touch/final/cst92xx.c`, a polling `i2c_driver` that registers an `INPUT_PROP_DIRECT` input device with `ABS_X`, `ABS_Y` and `BTN_TOUCH`. It was written by reading LilyGo's source in full rather than by extending the previous guesswork: `lewisxhe/SensorLib` `src/touch/TouchDrvCST92xx.cpp` (MIT) and `Xinyuan-LilyGO/LilyGoLib` `src/LilyGoWatchUltra.cpp` `initTouch()`. Both were fetched from GitHub and are cited in TCH-005 through TCH-008. The most important discovery is that the reset line is not an SoC GPIO at all -- it is P08 of the XL9555 expander, whose port 1 configuration reads 0xFF at boot, so the line was an input and nothing had ever reset the part; the driver takes it through `devm_gpiod_get_optional` with a `reset-gpios` phandle so probe waits for the expander. The official bring-up is four attribute reads after entering command mode -- checkcode at 0xD1FC requiring `(code & 0xFFFF0000) == 0xCACA0000`, resolution at 0xD1F8, chip type at 0xD204 requiring 0x9217 or 0x9220, and firmware at 0xD208 rejecting 0xA5A5A5A5 -- and probe fails on any mismatch instead of continuing hopefully. Measured on hardware: checkcode 0xcaca4e20, resolution 410x502, chip type 0x9220 with ProjectID 0x5a67, firmware 0x01000003. That resolution equals the panel exactly, so raw controller coordinates are panel coordinates with no swap, mirroring or scale, and no calibration constants are needed; the 320x400 fb0 window is the 45 px inset rectangle, so consumers subtract 45 and clamp. Three protocol details came out of the same reading and each had already caused a wrong result: only event 0x06 is a press, so a non-zero finger count with event 0x00 invents a phantom (observed at raw (41,451) with nothing touching); byte 6 must be 0xAB for a block to be valid; and the part emits no release event at all, so release is inferred from a 250 ms gap, about four of its ~66 ms report periods. The `0xD1 = 0x1E` "unlock" handshake that earlier work treated as required is not part of official bring-up -- it lives in `setMode()`, whose only caller is `sleep()` -- and TCH-002 was revised in place to say so. The second and larger finding is a bus defect that predated this work: `esp32_i2c_run()` returned `-ENXIO` on NACK and `-EAGAIN` on arbitration loss after clearing the interrupt, without calling recovery, which was wired only into the timeout branches. Since a polled capacitive controller produces exactly those outcomes, one such failure left `SR.bus_busy` set with the FSM idle, the pre-transfer check never fired, and every later transfer to every device failed until a reboot -- this is what wedged the bus every time touch was read, and what the old daemon hit. Recovery is now on those paths, gated on the bus actually being busy so an absent device still NACKs cheaply, and `esp32_i2c_transfer()` propagates the real error instead of collapsing everything to `-ETIMEDOUT`. Verified on hardware: a 10 s run of 1687 report reads produced 188 errors, 11 genuine NACKs each logged as `NACK: int_raw=00008610 sr=5600c010 busy=1` followed by `bus recovered`, and RTC, PMU and expander all still answered afterwards. The driver then bound as `cst92xx 0-001a: CST9220 at 0x1a, 410x502, fw 0x01000003`, registered `cst92xx-touch` as input2, and captured real events: `ABS_X=258 ABS_Y=309 BTN_TOUCH=1` then a smooth drag through `(259,305) (262,299) (267,289) (280,270)` and a `BTN_TOUCH=0` release. The user confirmed the coordinates were correct for the area touched. `userspace/tprobe.c` is kept as the diagnostic that established all of this, and it is the only binary in this tree built static: with about 7.8 MB of RAM and no swap, a fresh dynamic process fails at exec with "can't load library 'libc.so.0'" or segfaults under the loader. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all six YAML blocks in the reference parse, all 70 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 34, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Interrupt-driven operation is the obvious improvement and is now the main thing left in touch: `TP_INT` is GPIO 12 and LilyGo attaches a falling-edge handler to it, which would replace the 20 ms poll and cut both bus traffic and the NACK rate. It was deliberately not attempted in this cycle because the polling path is what proves the protocol and an unverified interrupt would have mixed two unknowns. A full 16 MiB backup should be taken: `working-2026-11-10-1353.bin` predates both the I2C recovery fix and the touch driver, so no saved image contains a watch with working touch on the hardware bus. Beyond touch, the deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/touch/final/cst92xx.c
- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- patches/kernel/config/devkit_c1_16m_linux.config
- scripts/build-kernel.sh
- scripts/build-userspace.sh
- userspace/tprobe.c
- .gitignore
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 34 COMMIT Unreleased 2026-11-10T13:58:39-07:00

#### Coming From:

Unreleased 1d56b39

#### Purpose:

Capture a full 16 MiB backup of the working hardware-I2C state, because no saved image contained the multi-byte fix and the previous newest backup predated it.

#### Outcome:

`~/twatch-backup/working-2026-11-10-1353.bin` is now the current recoverable state, 16 MiB read at 583 kbit/s in 230 seconds, sha256 `ee2133e0389c4ca6910df2f272c44755f22a7709004a80771b2c2e711e801309`. It was verified three independent ways rather than by accepting the script's own OK line, because a backup that silently contains the wrong kernel is worse than no backup at all and this project has already lost a session to exactly that. The stored `.sha256` was checked against a fresh `sha256sum` of the file; the 2841584-byte kernel extracted from the image at offset `0x140000` is byte-identical to the built `xipImage` (`a79b73d0…`), which proves the correct kernel is in the correct slot; and the running watch reports `Linux version 7.2.4-forkbank-ga623647af724-dirty`, the same build string the image contains, so the saved kernel is the one that has been tested rather than a stale neighbour. The image also carries the fix markers — the `FIRST SCL timeout` diagnostic string is present — and both `espressif,esp32-i2c` and `i2c-gpio` compatible strings, which is expected: `i2c-gpio` is still compiled in, but the live device tree selects the hardware driver and only `60013000.i2c` exists as a platform device, so the bit-banged path is present but unused. The backup's trailing hard reset rebooted the watch, which was confirmed back to a working state afterwards: hardware I2C bound, a seven-byte RTC read returning `0x21 0x58 0x20 0x10 0x02 0x11 0x26` = 20:58:21, and PMU capacity 100. The two images that matter for recovery are now clearly distinguished: 1353 is the working hardware-I2C watch without touch, and 1053 remains the only image that restores a fully functional watch including touch, on the older bit-banged bus. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 33, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The next kernel change starts from a verified image, so a failed attempt now costs a 4-minute restore instead of the whole session. That cycle is the kernel input driver that replaces `tbtns`: the bus is proven, the split-READ shape and command budget that make a 16-bit-addressed 15-byte read possible are established, and the two remaining unknowns are the CST9217's initialisation sequence and releasing its reset line through the XL9555. The absent-address `-ETIMEDOUT` versus `-ENXIO` defect belongs in the same cycle, so `i2cdetect` stops reporting timeouts for empty addresses. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 33 COMMIT Unreleased 2026-11-10T13:52:28-07:00

#### Coming From:

Unreleased 23c918a

#### Purpose:

Verify that the multi-byte I2C fix and the retirement of `tbtns` both survive a cold boot, since the previous cycle's evidence was all taken within one uptime.

#### Outcome:

Both hold across a reboot, and the bus came up clean without intervention. This mattered because every earlier finding in this work was confounded by the same weakness: a warm bus that had already been probed into a working state cannot distinguish a fix that persists from one that merely got lucky on the first boot. The watch was rebooted from the shell and checked from zero uptime. PMU published capacity 100 / 4175000 uV / Full with `usb_online` 1, `rtc0` read 20:50:12 and `hwclock -r` succeeded, and a seven-byte read of the RTC at register `0x04` returned `0x18 0x50 0x20 0x10 0x02 0x11 0x26` -- seven distinct non-`0xff` bytes decoding to the exact BCD time 20:50:18, which is the precise shape that was broken before and returned `0x57 ff ff ff ff ff ff` on every device for the whole session. A 60-operation soak followed, fifteen rounds over all four devices, and produced zero failures with zero kernel errors; a second check after another minute still read `0x52 0x51 0x20 0x10 0x02 0x11 0x26` = 20:51:52 against a clock reading 20:51:52. `tbtns` stayed gone: moving it to `/etc/disabled/` survives a reboot because `rcS` globs `S??*` and the file is no longer under `init.d`, so there is no repeat of the wedge and no manual step needed after a boot. Load average behaved as expected, 1.16 shortly after boot decaying to 0.42 within two minutes, which is ordinary boot spin and not a fault -- the earlier session had misread a load of 1.4 as a problem before establishing that it decays. WiFi came up at 10.10.0.15, the BLE keyboard is running, and fb0 is 320x400. Touch reads `0xff 0xff` at register `0x00` instead of its `0xca` checkcode, which is correct and expected: no driver is loading, and nothing releases the CST9217's reset line through the XL9555. Nothing was changed in this cycle, so no rebuild or reflash was required and the deployed kernel is the one verified. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 32, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Take a full 16 MiB backup of this state, because none of the four saved images matches it: `working-2026-11-10-1251.bin` predates the multi-byte fix and still carries the chatty driver, and `working-2026-11-10-1053.bin` is the older `i2c-gpio` build that restores touch but on the bit-banged bus. A clean image of the working hardware-I2C state is the one thing this session produced that is not otherwise recoverable. After that the next cycle is the kernel input driver to replace `tbtns`: the bus is proven, the split-READ shape and command budget that make a 16-bit-addressed 15-byte read possible are established, and the two remaining unknowns are the CST9217's initialisation sequence and releasing its reset line through the XL9555. The absent-address `-ETIMEDOUT` versus `-ENXIO` defect is worth folding into that same cycle so `i2cdetect` stops reporting timeouts for empty addresses. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 32 COMMIT Unreleased 2026-11-10T13:47:00-07:00

#### Coming From:

Unreleased 7e5efdc

#### Purpose:

Fix the multi-byte I2C read that returned one real byte followed by `0xff`, by reading ESP-IDF's `i2c_master_read()` instead of guessing at the ACK field, and retire `tbtns` after establishing that it was the one thing wedging the bus.

#### Outcome:

The multi-byte read works and the bus is stable, and the fault was never in the ACK field it had been blamed on. ESP-IDF never issues a single `READ(N)` with `LAST_NACK`: `i2c_master_read()` splits into `READ(data_len - 1)` with `I2C_MASTER_ACK` followed by a one-byte `READ` with `I2C_MASTER_NACK`, because `ack_val` is the level the master drives on the last byte of *its own command*, so one `READ(N)` with `ack_val = 1` NACKs after the first byte and the slave stops transmitting while the master keeps clocking the pull-ups. That is exactly `0x57 ff ff ff ff ff ff`, and `rxfifo_cnt` correctly reported `N`, which is why no short-read warning was ever logged and why the symptom could not be attributed from outside. The driver's code comment asserting that `ack_val = 1` "matches ESP-IDF" was a misreading of the same function and is corrected. Making that split fit took the command budget to exactly its limit, `seven real commands plus END at index 7` for a 16-bit register address plus a 15-byte read, so the address byte was merged into the register write, which is the same bytes on the wire and is what `i2c_master_write()` does for any `data_len > 1`. Three further defects fell out of the same reading and each was independently sufficient to keep the bus unreliable: `time_out_en` is bit 5 of `I2C_TO` and the driver wrote bit 10, a reserved field, so the hardware timeout had never been enabled and a stretched bus could only be caught by the driver's own one-second spin; `recover()` programmed `SCL_SP_CONF` without pulsing `conf_upgate`, so the nine-clock bus clear had never actually run and recovery could not free a held bus, which is why every earlier wedge required a power cycle instead of self-recovering; and `tout` was the constant 16 rather than `i2c_ll_cal_bus_clk()`'s derived value. The driver now also tests `INT_SCL_ST_TO` (bit 13) and `INT_SCL_MAIN_ST_TO` (bit 14), which it had never tested at all, and recovers on either. Verified on hardware: a seven-byte RTC read returns seven distinct real bytes that decode as the correct time in BCD (`0x52 0x45 0x20 0x10 0x02 0x11 0x26` = 20:45:52 on 2026-11-10), `hwclock -r` succeeds where it previously failed with `EINVAL`, and twenty operations across all four devices produced zero errors. `tbtns` was then retired by user decision, because the bus wedge was traced to its 15-byte touch read and a new touch driver is planned: it is stopped and moved to `/etc/disabled/`, its source, init script and selftest are removed from the tree, and it is recoverable from `1c37c50`. With it gone every remaining subsystem verifies clean — PMU capacity 100 / 4176000 uV / Full / usb online 1, RTC ticking and readable, expander `0xff 0xff`, haptic `0xe0`, panel bound at 320x400 16 bpp and painting, WiFi at 10.10.0.15, BLE keyboard running, load average down from 1.09 to 0.28 — and `dmesg` records zero `xfer FAIL`, zero `SCL timeout` and zero `short read`. One defect is knowingly left open and is not a regression: a transfer to an absent address returns `-ETIMEDOUT` rather than `-ENXIO`, so `i2cdetect` reports `--` from a timeout instead of a fast NACK. It costs scan speed and clean probe semantics, nothing else. The diagnostics added while chasing this are bounded counters rather than `printk_ratelimit()`, because `tbtns` polling at 66 Hz permanently exhausted the shared ratelimit state and silenced every log line exactly when it mattered, and because this board's console mirrors to fbcon so an unbounded dump costs a minute of boot. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 31, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The hardware I2C bus is working and is the default, so the next cycle is a real touch driver to replace `tbtns`, and it should be a kernel input driver rather than a userspace daemon: the CST9217 needs its reset line released through the XL9555 first, which is why the device currently answers `0xff 0xff` at register `0x00` instead of its `0xca` checkcode, and the 15-byte report block must be read whole because the acknowledge that advances the controller's buffer is issued against the block. The split-READ shape and the command budget that made that read possible are now proven, so the remaining work is initialisation sequence and input plumbing, not bus debugging. Worth fixing in the same pass or before it: absent addresses should return `-ENXIO` from the NACK interrupt instead of running to the SCL timeout, which would make `i2cdetect` immediate and give every driver's probe path correct semantics. A fresh full 16 MiB backup should be taken of this state, because the newest saved image `working-2026-11-10-1251.bin` predates the multi-byte fix and still carries the chatty driver; this cycle deliberately did not take one. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- scripts/build-userspace.sh
- scripts/watch-deploy.sh
- userspace/haptic.c
- .gitignore
- userspace/tbtns.c (removed)
- userspace/S50tbtns (removed)
- scripts/tbtns-selftest.py (removed)

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 31 COMMIT Unreleased 2026-11-10T13:05:12-07:00

#### Coming From:

Unreleased 224f585

#### Purpose:

Find what was adding about a minute to boot, after the user noticed something called i2c-esp32 busy for a long time at startup.

#### Outcome:

It was this project's own debug logging, and the flag had been left on. The `espressif,debug` property added to the device tree during entry 29's command-word diagnosis was never removed, and with it every I2C transfer printed all eight command registers. At boot that came to 241 log lines spread over 100 seconds, which is what the user saw: not a program, and not a timeout, but the driver talking about itself. The time was not the serial output either -- it was that this board's console mirrors to fbcon, so each of those 241 lines forced a repaint of the 320x400 panel over SPI, and 241 repaints is most of a minute. Only two driver timeouts occurred in the same boot, about 2 s total, so the transfer path was never the cost. Two changes were made rather than one. The flag is gone from the device tree, and the dump is now behind `printk_ratelimit()` so that even with it deliberately enabled it cannot flood the console again; the second matters more, because a diagnostic that can stall the machine is worse than the fault it is hunting. Verified on hardware after flashing: the log dropped from 241 lines to 6, those six are the legitimate one-time probe messages (clock rate, the two pin routes, the input route, the system registers, and the master registration), and all of them land between 4.4 s and 6.6 s, so the driver finishes probing in about two seconds. The subsystems were unaffected -- `rtc0` still present and reporting, `axp20x-battery` capacity 100. The user confirmed the boot is now much faster. Worth recording as a pattern rather than an incident: this is the second time in one session that a diagnostic caused a worse symptom than the bug it was instrumenting, the other being the bus recovery that generated the arb_lost fault it was meant to cure. Both had the same shape -- instrumentation whose cost was never bounded. Note for whoever restores a backup: `working-2026-11-10-1251.bin` predates this fix and therefore still carries the chatty driver, so a restore from it will boot slowly until this change is reflashed. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 30, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The remaining I2C work is unchanged and is the touch controller's 16-bit register address, documented at the end of `docs/cores.md`: fold the address into the data WRITE or split the transfer, since a 16-bit read needs one more command register than `SOC_I2C_CMD_REG_NUM` allows. Until that lands `tbtns` cannot run and the watch has no local input. A fresh full backup should be taken once boots are confirmed fast and that fix is in, so the saved image does not carry the chatty driver; this cycle deliberately did not take one.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 30 COMMIT Unreleased 2026-11-10T13:01:47-07:00

#### Coming From:

Unreleased be88103

#### Purpose:

Finish the hardware I2C driver by reading ESP-IDF's source rather than by flashing, after entry 29 left the bus working for one transfer and failing thereafter.

#### Outcome:

The hardware I2C bus now works, sustained, and the two defects that made it unfixable by experiment were both one-line errors in a header field list. Reading `i2c_struct.h` and `soc_caps.h` rather than flashing found them: `bus_busy` is bit 4, not bit 3, and bit 3 is `arb_lost`, so with the define one low the driver's `SR_BUS_BUSY` was reading the wrong flag entirely -- a genuinely stuck bus never triggered recovery and the arbitration-lost path never fired, which is the whole "one transfer per boot" symptom. And recovery must restore the timing registers: ESP32-S3 leaves `SOC_I2C_SUPPORT_HW_FSM_RST` undefined with the comment "FSM_RST only resets the FSM, not using it", so ESP-IDF cycles the clock gate and then re-writes every timing register, and skipping that restore left the peripheral at default timing so the next transfer reported `scl_main_st_to` with `arb_lost` -- an electrical-looking fault that was self-inflicted. A third correction followed: the one-bit `fsm_rst` is not usable on this chip at all and is gone, and `ack_val = 1` (NACK the last byte) is correct for every read, matching ESP-IDF's `I2C_MASTER_LAST_NACK`; the earlier "use 0 for multi-byte" rule had been derived from a test that also carried the address-phase bug, so the evidence was contaminated. Verified on hardware and sustained: capacity 100, voltage_now 4178000 uV, status Full, usb online 1, `rtc0` readable and ticking from 19:42:50 to 19:42:58, a credible `i2cdetect` in which only 1a, 20, 34 and 51 answer, and ten consecutive reads with no failure. A full 16 MiB backup of exactly that state was taken and hash-verified, `working-2026-11-10-1251.bin`, sha256 `04cfee94`, confirmed to carry the 40 MHz marker, the `espressif,esp32-i2c` compatible string and no `i2c-gpio`. One gap remains and it is precisely characterised: the CST9217 needs a 16-bit register address and that transfer shape hangs, with the failure state unambiguous -- `int_raw=0x200` (trans_start only), `txfifo_cnt=3` (bytes never sent), `resp_rec=1` (the address WAS acknowledged), `bus_busy=0` -- so the controller sends the address, receives its ACK and never chains to the next command. Only the touch controller is affected; the PMU and RTC use one-byte addresses. The strongest lead is the command-register budget, since a 16-bit read needs 8 command registers and `SOC_I2C_CMD_REG_NUM` is 8. Two observations about the board's behaviour were also settled rather than left as suspicions: the load average of 1.4 shortly after boot decays to 0.48 and the CPU measures 4 percent busy with 96 idle, so it was stale boot-time spin from probe failures and not a live fault; and the RTC and PMU reads that failed while a flash read was in progress recovered on their own, so that was transient contention rather than a regression. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, KRN-006 was updated in place to record the resolution, and this entry was validated against the template with six sections in canonical order, sequential numbering from 29, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The touch controller needs one change, and it is small enough that it should not cost another dozen builds: a 16-bit register read currently emits 7 commands plus END, one more than `SOC_I2C_CMD_REG_NUM`, so either fold the address into the data WRITE or split the transfer in two. Until that lands, `tbtns` cannot run and the watch has no local input, which is the only thing standing between this and full function. Everything else on the I2C bus is working and backed up, so the next cycle should be that one fix and nothing else. Beyond the bus, the deferred items are unchanged: the Tamagotchi front end still wants a `S51tamafb` from rcS, and LCD-009's tearing and the LCD-005 versus LCD-010 disagreement remain open.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 29 COMMIT Unreleased 2026-11-10T11:56:24-07:00

#### Coming From:

Unreleased 8e19197

#### Purpose:

Take the suggestion to read how LilyGo and ESP-IDF actually drive this peripheral, rather than continuing to reconstruct the sequence from headers, after entry 28 left the driver unable to start a transfer.

#### Outcome:

The method was the fix, and the bus now works: reading ESP-IDF's own `components/driver/i2c/i2c.c` instead of its headers found five further defects that eight header-based flash cycles had not, and with them the peripheral carries real traffic. `i2cdetect` returns a credible scan in which only `1a`, `20`, `34` and `51` answer and every other address is absent, single-byte reads return correct values (`i2cget 0x51 0x04` gives `0x07`, `i2cget 0x34 0xa4` gives `0x4c`), `rtc-pcf85063` registers as `rtc0`, the AXP2101 probes and publishes `axp20x-battery` and `axp20x-usb`, and `axp20x-pek` appears as an input device. Nine defects were found in total, and they are worth listing because each one alone would have been enough to stop the driver: completion was polled on a command's `done` bit which END never sets; `INT_STATUS` is masked by `INT_ENA`, which is zero in polling mode, so it reads zero for ever and `INT_RAW` is the register to poll; the clock gate is closed at boot in `SYSTEM_PERIP_CLK_EN0` bit 7; the peripheral is held in reset in `SYSTEM_PERIP_RST_EN0` bit 7, which is KRN-006 and the expensive one; the timing needs `scl_wait_high_period` and the HAL does not subtract one from `scl_high`; `sclk_div_num` is bits [7:0] and not [13:8]; the GPIO matrix needed the INPUT route as well as the output one, without which the controller could never read SDA and never saw an ACK, and `pad_driver` had to be set for open drain; the ACK flags are per-opcode, with RESTART and STOP taking none, WRITE taking `ack_en` and READ taking `ack_val` only, where this driver had been setting `ack_exp` everywhere and so expecting NACKs; and `rxfifo_cnt` is bits [13:8] rather than the [10:5] a truncated comment in `i2c_struct.h` suggests, which is the second time that header has misled this work. What the cycle demonstrates beyond the bus itself is a method: the headers are incomplete and in at least two places wrong, `i2c_struct.h`'s opcode comment contradicts the HAL and its field comments are truncated, and the driver source that ships with the hardware is the authority. Eight flash cycles were spent before that was tried. What still fails is multi-byte reads: they return the first byte and then `0xff`, so `i2ctransfer -y -f 0 w1@0x51 0x04 r3` gives `0x07 0xff 0xff`. Single-byte reads are correct, which is why the PMU probes fully and the RTC registers but `hctosys` cannot read its seven-byte block and `pca953x` cannot read the expander. The command words were dumped from the running controller and are correct -- RESTART `00003000`, WRITE 1 byte `00000901`, READ 3 bytes `00001c03`, STOP `00001000`, END `00002000` -- so the encoding is not the fault and the fault is in the RX path, where the FIFO only ever holds one byte. The board is left bootable with a partly working bus: PMU and RTC present, expander and touch not, and `tbtns` not running. The backup at `~/twatch-backup/working-2026-11-10-1053.bin` still restores a fully working watch. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates and KRN-006 was revised in place with its verification extended, and this entry was validated against the template with six sections in canonical order, sequential numbering from 28, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

One more attempt at the multi-byte read, then restore regardless of the outcome. The command encoding is proven correct by dump, so the next check is the RX side: how many bytes the FIFO actually holds after a read (dump `SR` and the `rxfifo_cnt` field), whether the drain loop is reading `I2C_DATA` correctly for FIFO mode, and whether ESP-IDF's `i2c_ll_read_rxfifo()` call sites reveal a step this driver is missing. If that does not settle it in one flash, restore `~/twatch-backup/working-2026-11-10-1053.bin`: a watch with a working bit-banged bus is worth more than a watch with a half-working hardware one, and everything learned here is preserved in the driver source, KRN-006 and `docs/cores.md` for the next attempt. Beyond the driver, the deferred items are unchanged and none of them should be started until the bus question is closed either way.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 28 COMMIT Unreleased 2026-11-10T11:38:52-07:00

#### Coming From:

Unreleased f32af28

#### Purpose:

Write the hardware I2C driver that entry 27 identified as the real fix for a third of core 1 being spent bit-banging the bus.

#### Outcome:

The driver is written, builds, links and probes, and it does not work yet; the board is currently running with no I2C at all. What it did establish is worth more than the failure. Four genuine defects were found and fixed, each of which would have blocked any future attempt: completion was being polled on a command's `done` bit, which the END command never sets, and the real signals are `int_status.trans_complete` and `int_status.nack`; the timing setup omitted `scl_wait_high_period`, which shares the `SCL_HIGH_PERIOD` register in bits 15:9, without which the bus cannot run at all; the peripheral's clock gate is closed at boot, `SYSTEM_PERIP_CLK_EN0` at `0x600c0018` bit 7 reading 0, and nothing on this board had ever opened it because the firmware does not use I2C and the bit-banged bus needed no peripheral clock; and the peripheral is held in reset at boot, `SYSTEM_PERIP_RST_EN0` at `0x600c0020` bit 7 reading 1 in a register worth `0x9eff1fb8`. That last one is the finding that matters and it is now KRN-006: while the reset is held, every write to the controller is silently discarded while reads still return data and the TX FIFO still accepts bytes, so `CTR` reads back `0x20b` -- the power-on default with master mode and clock enable clear -- immediately after a successful write. That signature imitates three different faults in sequence, a bad command encoding, a wrong completion signal and a dead bus, which is why diagnosing it took eight build-and-flash cycles. Releasing and pulsing the reset makes `CTR` hold `0x313` as written, which proves master mode and clock enable now genuinely take effect, and the clock reports 80 MHz. What remains is that `trans_start` does not start the sequencer: `int_status` stays zero and the TX FIFO keeps its six bytes. Once the reset was out of the way, register offsets were checked against the member order in `i2c_struct.h`, command opcodes against the HAL rather than the stale comment in that header, pin routing read back as `out_sel` naming signals 90 and 89, and the write ordering of `conf_upgate` and `trans_start` split and flushed and rejoined, and none of it moved the result. The honest reading is that the count of excluded causes now exceeds what a single wrong bit can explain, so the model of the block is wrong somewhere rather than one value being off, and the next step is to read ESP-IDF's own `components/driver/i2c/i2c.c` master transfer routine line by line instead of continuing to reconstruct it from headers. That is a change of method, not another guess, and it is the method that should have been used first. The board boots and runs throughout with the console and USB console working, but the PMU, RTC, expander and touch controller all fail to bind, so the watch is degraded; the rollback is `~/twatch-backup/working-2026-11-10-1053.bin`, taken before any of this, which restores `i2c-gpio` and a fully functional watch. `docs/cores.md` now carries an appendix recording what works, what was excluded, and this exact next step, so a future attempt starts from evidence. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates at 65 records with 65 index keys and no orphans in either direction, and this entry was validated against the template with six sections in canonical order, sequential numbering from 27, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Restore `~/twatch-backup/working-2026-11-10-1053.bin` so the watch has working I2C again: the driver is not close enough to justify leaving the PMU, RTC, expander and touch controller unbound. The driver source stays in the tree at `patches/kernel/i2c/final/i2c-esp32.c` with every excluded hypothesis recorded in its comments, and the DTS switch is a single node that reverting is a two-line change for. Then read ESP-IDF's `i2c.c` master path and compare its start sequence against this driver's before flashing anything again; the trigger, not the configuration, is what is unproven, and eight cycles of header reconstruction have earned a different approach.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- patches/kernel/config/devkit_c1_16m_linux.config
- scripts/build-kernel.sh
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 27 COMMIT Unreleased 2026-11-10T11:04:36-07:00

#### Coming From:

Unreleased 1c37c50

#### Purpose:

Settle, from the port's own sources and the vendor's firmware, whether the RTOS on core 0 should own the hardware with a fast link to Linux, before any driver work is committed to.

#### Outcome:

The answer is no, and it is not a judgement call: there is no channel to build it on. The port documents its own split in `build/Linux-on-esp32-S3/ARCHITECTURE.md`, and the firmware confirms it in one function. Core 0 runs ESP-IDF and FreeRTOS and owns the WiFi radio, whose driver is a closed binary that only runs under ESP-IDF. Core 1 runs Linux natively, and `linux_boot.c` starts it as `xTaskCreatePinnedToCore(linux_task, ..., 1)` where `linux_task()` is `map_flash_and_go(); esp_restart();` -- Linux is a FreeRTOS task pinned to core 1 that never returns, and `esp_restart()` is the path taken only if it ever does. So the split is forced by one blob rather than chosen as a design principle, and everything except the radio -- panel, PMU, RTC, touch, haptics, USB, filesystem -- is already owned directly by Linux through ordinary drivers. The link between the cores carries a network interface, Bluetooth, and typed custom interfaces, of which this project added one for firmware logs; it carries no I2C, SPI or GPIO RPC and has no generic peripheral channel. LilyGo was checked as asked, and its architecture answers a different question: `Xinyuan-LilyGO/LilyGoLib` runs FreeRTOS on both cores with no OS boundary and no IPC, so there is nothing to offload to, and its touch code reaches the controller through a direct `Wire` call from whatever task is running. Two facts from it are still useful. It pins the NES emulator and its audio task to core 0 alongside the radios, which shows core 0 has headroom for compute in a single address space but says nothing about moving I/O. And `LilyGoWatchUltra.cpp` calls `Wire.begin(SDA, SCL)` with `setClock(1000000)` for the touch panel, which confirms from the vendor that the hardware I2C peripheral works on this board's pins at a megahertz against the 100 kHz we bit-bang. Meshtastic has no T-Watch Ultra target and MeshCore has no variant for it either, so neither adds a third data point. The proposal fails on five grounds already recorded in this project, and they are now written down together rather than scattered: no channel exists and the shmem link is contended enough that one extra log line stopped WiFi bring-up; every transaction would become a round trip that the deliberate 15 ms touch poll cannot absorb; core 0 cannot write flash; core 0 is the radio core and a polling server would contend with WiFi and BLE under a watchdog that resets the chip; and every device driver already exists and works on Linux, so moving the bus means rewriting them as firmware in a tree that must never be pushed. The distinction worth keeping is that computation may cross the boundary while I/O should not, because computation batches into a shared buffer and I/O needs a round trip per transaction, which is why EMU-004's emulator-on-core-0 design remains the one legitimate offload and stays deferred. The bit-banged I2C is therefore a missing driver and not a placement mistake: the fix is to write `i2c-esp32.c`, which removes the CPU cost instead of moving it and gains 4 to 10x on the bus as well. A system-wide CPU sample was taken to size the problem rather than argue about it, and it is worth stating with its disagreement intact: `/proc/stat` over five seconds shows 26.0 percent of the one core busy with 22.9 percent of it in kernel time, while `tbtns` alone measured 34.6 percent in kernel time earlier in the session -- the two do not reconcile, and the likely cause is that NAK-driven console traffic varies with load, so the honest reading is that kernel time from the bus is somewhere in the twenties to mid-thirties percent and is the dominant consumer either way. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates at 64 records with 64 index keys and no orphans in either direction, and this entry was validated against the template with six sections in canonical order, sequential numbering from 26, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Write the hardware I2C driver. The design is now constrained by evidence rather than preference: it lives in `patches/kernel/i2c/final/i2c-esp32.c` and installs the same way `spi-esp32.c` does, it programs `I2C0` at `0x60013000` from the register map in the local ESP-IDF `i2c_ll.h` and `i2c_reg.h`, and it routes GPIO3 and GPIO2 through the GPIO matrix using the `I2CEXT0_SDA_OUT_IDX` and `I2CEXT0_SCL_OUT_IDX` signals that `i2c_periph.c` names. The first milestone is deliberately small and reversible: `i2cdetect -y 0` must see all six devices at 100 kHz with the DTS switched over, before interrupts or 400 kHz are attempted. That switch is the risky edit, because PMU, RTC, haptic and touch all move to the new driver at once, so it wants the backup taken at 10:57 and the console watched rather than assumed. The core-0 question is answered and closed; the emulator-offload question remains open under EMU-004 and should not be reopened on the strength of this cycle's finding about compute headroom, which was inferred from a single-address-space design and does not transfer to ours.

#### Files Modified:

- docs/cores.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: NOT RUN

---

## 26 COMMIT Unreleased 2026-11-10T10:43:07-07:00

#### Coming From:

Unreleased a47de90

#### Purpose:

Undo a regression introduced in entry 25, where the touch daemon stopped polling during gameplay and took the only input path with it.

#### Outcome:

The user asked whether the buttons still worked after entry 25 paused the daemon, and the answer was no. `tamafb` reads `BTN_0`, `BTN_1` and `BTN_2` from the uinput device that `tbtns` creates and from nothing else, so stopping the daemon stopped the pet from being playable: the previous cycle had optimised away the thing the buttons exist for. The daemon now slows to 66 ms while a fullscreen app owns the panel instead of stopping, which matches the controller's own 66 ms report cadence, and both transitions are verified in the log on hardware, `tbtns: slowing to 66 ms for tamafb` and `tbtns: back to 15 ms, tamafb has exited`, with `tamafb` confirming `buttons: /dev/input from touch-buttons` and polling it during the run. The measurement attached to the fix contradicts the explanation given for it in entry 25, and that is the more useful half of this cycle. Slowing the poll from 15 ms to 66 ms -- quartering the read rate -- moved throughput by 1.1 percent, 132679 against 131226 instructions/s. So the 34.7 percent the daemon costs is not the poll cadence, even though the arithmetic appeared to predict it: 3957 us per read every 15 ms is a 26 percent duty cycle, which matches the measured loss closely enough to have been convincing. It was still wrong. Whatever is expensive happens once per wakeup rather than once per read, and the candidates are the scheduler cost of the `i2c-gpio` kernel thread for each transaction, the console traffic behind every NAK, or the syscall round trip; none of that is established and profiling is the next step if the number ever matters. The practical conclusion is stated plainly in `docs/tamagotchi.md` rather than buried: you can have working buttons or 34.7 percent of the emulator, not both, and the buttons win because a watch whose pet cannot be played is not the goal. The speed belongs to starting the emulator at boot, where the ROM may fit in PSRAM and recover 33.8 percent without touching the daemon at all. Two measurement caveats are worth recording rather than smoothed: the A/B above is two runs per arm rather than the three used elsewhere, because the board kept interfering, so treat 1.1 percent as close to the noise floor and the direction as the only firm claim; and three attempts at a cleaner A/B failed for tooling reasons, not reasons of substance -- a stand-in process built by copying `/bin/sleep` segfaulted because busybox dispatches on `argv[0]` and `tamafb` is not an applet name, `/home` reached 97 percent full when that copy landed there at 855 KB, and a long telnet command line was truncated before it could run. A purpose-built 6 KB idle binary fixed it, and a subdirectory was needed because `comm` is the basename. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 25, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The emulator thread's one blocking item is unchanged and now carries both remaining wins: a `S51tamafb` started from `rcS`, which makes the emulator start at boot, may let the 512 KB ROM allocation happen while an order-7 block still exists, and must write `/var/run/tamafb.pid` or not even the 66 ms slow-down will engage. The daemon's 34.7 percent is worth profiling only if it becomes the thing standing in the way; until then the honest position is that it is unexplained and the buttons are worth more than it. Beyond the emulator, the display items are unchanged: LCD-009's tearing root cause, and the 4.2x disagreement between LCD-005 and LCD-010. The boot loop from entry 23 remains unexplained with its dump pruned.

#### Files Modified:

- userspace/tbtns.c
- docs/tamagotchi.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 25 COMMIT Unreleased 2026-11-10T10:26:48-07:00

#### Coming From:

Unreleased 109f9b5

#### Purpose:

Recover the third of the emulator's throughput that the touch daemon was taking, without giving up the tap latency the daemon exists for.

#### Outcome:

`tbtns` now stands down while a fullscreen app owns the panel, and both transitions are verified in the daemon log on hardware: `tbtns: standing down for tamafb` when the app starts and `tbtns: resuming, tamafb has exited` when it stops. The cost being recovered was measured the same boot on the same binary, 134496 instructions/s with the daemon polling against 206091 with it stopped, so the daemon takes 34.7 percent, or 53 percent more throughput without it. The arithmetic says why rather than leaving it a mystery: one touch read costs about 3957 us on this bit-banged I2C bus and the loop polls every 15 ms, so roughly a quarter of the core goes into waiting out bus traffic that is almost always about to report nothing new. Both constants are correct for their purpose, so the fix is not to poll slower but to stop polling while something else needs the core. The daemon reads `/var/run/tamafb.pid`, which `run-tama.sh` already writes, and validates that pid against `/proc/<pid>/comm` before believing it. That validation earned its place twice: a stale file after a crash would otherwise disable the buttons permanently and silently, which is worse than the bug being fixed, and during testing a launch wrapped in a shell had `comm` of `bash` rather than `tamafb` and was correctly ignored. Standing down also forces a release before yielding, so a finger held at the moment the app starts cannot leave a key stuck, because nothing below runs to time it out. The fallback interval is 200 ms, long enough to cost nothing and short enough that resuming is not noticeable. The band-arithmetic selftest still passes. The same cycle closed the PSRAM question from entry 24 in the negative and, in doing so, corrected two of my own errors. The patch is deployed and does exactly what it should when the allocation fails, printing `rom: 512 KB PSRAM allocation refused ... falling back to flash, 33.8% slower` followed by `rom: flash window`, and the run was clean at 4,130,898 emulated cycles/s and 1.03x; so the fallback path, which had never once been exercised, is now proven. But it will never take the PSRAM arm as things stand. A malloc ladder on the board gives malloc(524288) FAIL and malloc(524000) OK, so the boundary lies in that 288-byte window and it is the allocator's own header: a request equal to the largest free block leaves nothing over for the header itself. Splitting the ROM, which was the user's suggestion and a good one to test rather than argue about, makes it worse: malloc(262144) twice produced one success and one failure, because each block pays its own header out of the same budget. Two corrections follow for the records. My reading of /proc/buddyinfo had been against the wrong end of the line and the block it called order 8 is order 7, 512 KB; the line has eleven fields, orders 0 to 10, and nothing larger than 512 KB has ever been free in these readings. And an attempt at fixing this by allocating 1024 bytes less than the ROM while still reading the full ROM would have overrun the buffer; it was caught before building, the source was restored from the original plus the patch, and the rebuild came back byte-identical at sha256 7447986c, so nothing unsafe was ever deployed. KRN-004 and `docs/tamagotchi.md` were revised to record the boundary, the failed split, and the correction. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference still validates at 63 records with 63 index keys, and this entry was validated against the template with six sections in canonical order, sequential numbering from 24, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The emulator thread now has one clear blocking item: a `S51tamafb` started from `rcS`, which would both make the emulator start at boot rather than by hand and, if it runs early enough, let the 512 KB ROM allocation happen while an order-7 block still exists, which is the only route to the 33.8 percent that entry 22 measured. Nothing else in that thread is worth doing first, and the two cheap wins are now one delivered and one blocked on that service. Beyond the emulator, the display items are unchanged and both are diagnosis rather than design: LCD-009's tearing root cause, still open with SPI clock excluded, and the 4.2x disagreement between LCD-005's 190 ms frame and LCD-010's 45.7 ms projection, which needs a re-measurement through the real repaint path before either figure is quoted again. The boot loop from entry 23 remains unexplained and its dump was pruned in entry 24, so a recurrence has to be captured fresh.

#### Files Modified:

- userspace/tbtns.c
- docs/tamagotchi.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 24 COMMIT Unreleased 2026-11-10T09:47:12-07:00

#### Coming From:

Unreleased b81ac0f

#### Purpose:

Re-establish the 40 MHz panel state that the restore had rolled back, and leave exactly one qualified image so a recovery no longer has to guess which backup is the right one.

#### Outcome:

The 40 MHz state is back on the board and for the first time there is one image that can be restored without a decision. The kernel was rebuilt from the tree, `build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage`, 2833064 bytes, and written alone to `0x140000`, which is the same kernel-only operation that produced the state on the night of 2026-11-09; it reported `Hash of data verified` in 23.1 seconds and the board came up with `spi_hz` reading 40000000 and both `tbtns` and `blekbd` running, with `/home`, `/etc` and the ROM slot untouched. Two pre-flight checks were made first and both passed: `space_for_vectors` in the firmware ELF is `0x40380000`, matching `CONFIG_VECTORS_ADDR`, so no firmware rebuild or vectors realignment was needed, and the image ends at `0x3F3AA8`, clear of the ROM slot at `0x480000` by 561 KB. The rebuilt kernel differs from the one that was running this morning in exactly 12 bytes, both clusters being the build timestamp, `#1 Tue Nov 10 00:22:04 MST 2026` against `#2 Tue Nov 10 09:37:47 MST 2026`, so it is the same code and not a new build risk. A full 16 MiB backup was then taken to `~/twatch-backup/working-2026-11-10-0940.bin`, sha256 `fd7e5bf9`, and verified three ways rather than by exit status: the hash checks with `sha256sum -c`, the `spi_hz` marker is present in the image, and the kernel region is byte-identical to the `xipImage` that was flashed, with the ROM slot still reading `A5 5F 10 49`. The backup directory was then pruned from 17 images and 259 MB to two images and 33 MB. This cycle also settles why the last three attempts at recovery kept going wrong. There was never a qualified 40 MHz image: the 40 MHz default landed in commit `3feff4f` at 00:29 on 2026-11-10, while the newest full backup, `working-2026-11-10-0023.bin`, was written at 00:23, six minutes earlier, exactly as entry 18 had warned. Every image on the machine was old, which is why restoring any of them returned the panel to 20 MHz and why the only 40 MHz image in existence was the boot-looping dump. All 16 images were checked for the `spi_hz` marker rather than by filename, and that check is the one that should have been run before the first restore. Two of my own errors are recorded because they cost the user time: the looping dump was described as the user's working state and restored, when it was a dump of a device that was already failing, and a `find` used to locate images compared a full-flash dump against a bare kernel image at the same offset, which is meaningless for the bare image. The pruned set keeps `working-2026-11-10-0940.bin` and `twatch-factory-16MB.bin`. The factory image was kept against the stated plan of deleting everything but the qualified image, because checking it showed its `factory` and `rootfs` partitions do not match the qualified backup: the backup carries this project's firmware while that file is the untouched stock state, so it is the one file in the directory that cannot be reproduced. Everything deleted was either reproducible from the qualified image, reproducible from the repository, or a known-bad dump. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected and contains only this entry, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 23, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The board is on the 40 MHz kernel with the qualified image saved, so the emulator work can resume where entry 22 left it. Two changes are pending and both are small: redeploy `tamafb` from `patches/harness/0001-tamafb-rom-in-psram.patch` so the ROM-in-PSRAM arm reaches the hardware, confirming it prints `rom: PSRAM` on a fresh boot and `rom: flash window` once the 512 KB block is gone, which is the fallback that has still never been exercised; and make `tbtns` stand down while the emulator owns the panel, worth about 30 percent for no memory at all. The boot loop from entry 23 remains unexplained and is now uninvestigable from the image side, because the dump it was diagnosed from was deleted as part of this cycle's pruning; if it recurs the first action is a fresh dump before anything is restored. The panel and display items are unchanged: LCD-009's tearing root cause, and the 4.2x disagreement between LCD-005 and LCD-010.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 23 COMMIT Unreleased 2026-11-10T09:20:41-07:00

#### Coming From:

Unreleased 736e11c

#### Purpose:

Restore the watch from its full flash backup after it entered a boot loop, and record what the recovery cost so the next one is cheaper.

#### Outcome:

The board is recovered and verified on hardware, but the cause of the loop is not established and this entry says so rather than guessing. The failure appeared as an endless reset at exactly `Starting touch buttons:` on every cycle, with `rst:0xc (RTC_SW_CPU_RST)` and `Saved PC:0x403777fd`, while the USB console was still readable and userspace answered nothing; it followed a `sync; reboot` issued after a session in which the WiFi link had already wedged and a 255 KB `tamafb` had been deployed. Before overwriting anything the failing image was dumped to `~/twatch-backup/broken-2026-11-10-0858.bin`, which allows the cause to be bounded even though it cannot be named. Compared against the restored backup, `etc` is byte-identical and the ROM slot at `0x480000` is byte-identical, so this was not `/etc` damage, which is the failure mode the recovery doctrine warns about, and not a damaged ROM. The two partitions that differ are `linux` and `home`. The `linux` difference is fully explained and is not a fault: the running kernel is byte-identical to this repository's `build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage`, the 40 MHz build, while the backup's kernel is the older 20 MHz one, exactly as entry 18 recorded when it warned that restoring this backup returns the panel to 20 MHz. The `home` difference is jffs2, which is log-structured and changes on any write. So nothing in persistent storage accounts for the loop, which leaves a transient runtime or firmware condition as the remaining explanation and no instrument yet pointed at it. The restore itself took three attempts and the failures were the useful part. The documented command, `write_flash 0 0x1000000 <file>`, is wrong for esptool 4.13: `write_flash` takes `<address> <filename>` pairs with no length argument, so `0x1000000` was read as a second filename and esptool died with `[Errno 2] No such file or directory: '0x1000000'` before touching flash. A second attempt ran without `--after no_reset` and reported success, but its output was not captured and the reset esptool performs by default had already booted the board out of download mode, after which Linux owned the USB-Serial-JTAG port (SOC-003) and every later command sat at `Connecting...` forever; that is why holding the board in boot mode did not appear to help, because the lockout came from the previous command's own reset. The write that worked was `--after no_reset write_flash 0 <backup>` against a board held in boot mode, and it reported `Hash of data verified` over all 16777216 bytes in 122.9 seconds, which is the only evidence that counts. `docs/watch-recovery.md` now carries the corrected command and both traps. The measurement work from the previous cycle was preserved rather than lost with the reflash: the Tamagotchi front end's PSRAM change lives outside this repository in `../TamaHarness`, so it was captured as `patches/harness/0001-tamafb-rom-in-psram.patch`, checked by applying it to a scratch checkout of the original and confirming the result is byte-identical to the working tree. The board came back on the older kernel with `buildroot login:` on the console, ping answering, `tbtns` and `blekbd` both running, and a 1 MB contiguous block free. Two consequences are stated rather than left implicit: the deployed `tamafb` is again the old 249112-byte flash-window build because the backup predates the deploy, so the 33.8 percent is not on the board until it is rebuilt and redeployed from the preserved patch; and the panel is back at 20 MHz with no `spi_hz` parameter in `/sys/module/co5300`, so the 40 MHz kernel has to be reflashed if that clock is wanted. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 22, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Three things, in order. Reflash the 40 MHz kernel to `0x140000` from `build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage`, which is verified byte-identical to what was running before the loop, because the restore rolled it back to 20 MHz. Then rebuild and redeploy `tamafb` from `patches/harness/0001-tamafb-rom-in-psram.patch` so the ROM-in-PSRAM arm is actually on the board, and confirm it prints `rom: PSRAM` on a fresh boot and `rom: flash window` once the block is gone, which is the fallback path this cycle never got to exercise. The boot loop itself is still unexplained and should not be considered closed: the next occurrence should be caught with the flash dumped before any restore, as this one was, and with the console captured across a whole cycle, because the evidence that would convict a cause is what happens in the last second before `rst:0xc`, which this cycle saw only as the reset itself. A full-image backup should be taken before the kernel reflash. `scripts/watch-backup.sh` was checked against the same esptool and is correct as it stands, because `read_flash` takes three arguments -- address, size, filename -- and the script passes all three, so the defect was confined to the documented write command and not to the tooling.

#### Files Modified:

- docs/watch-recovery.md
- userspace/memlat.c
- patches/harness/0001-tamafb-rom-in-psram.patch

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 22 COMMIT Unreleased 2026-11-10T08:37:52-07:00

#### Coming From:

Unreleased 199dc64

#### Purpose:

Test the ROM-location lead entry 21 left open, so the emulator's ROM has a measured home instead of a predicted one.

#### Outcome:

The lead was right and the effect is larger than predicted. With the ROM read from the memory-mapped flash window the emulator sustains 204033 instructions/s and 4.49x realtime; with the ROM in PSRAM it sustains 273098 instructions/s and 6.01x, a gain of 33.8 percent, which is 1239 ns saved per emulated instruction, 4901 ns down to 3662 and 1176 native cycles down to 879. Nine flash runs across three boots gave 203008 to 205065 with no overlap against the PSRAM figure, and the PSRAM arm was sampled twice on two separate boots at 273115 and 273080. Both arms retired exactly 454305 instructions at 88.05 emulated cycles per instruction, so the same work was done both times and only the memory differed. The mechanism is confirmed rather than merely observed: the ROM is 72.6 percent of the 2.408 bus accesses an instruction makes, 1.748 of them, and a flash-window read costs 738 ns, which predicts 1290 ns saved; the measured saving is 1239 ns, within 4 percent, so the interpreter was indeed stalling close to full dependent latency on most of its ROM reads. The trade is recorded rather than resolved: the flash window always works at any uptime and costs 33.8 percent every time, while PSRAM buys 6.01x but depends on a 512 KB contiguous block that exists only briefly after boot, measured at 1 MB at 0 minutes, 256 KB by 9 minutes and 128 KB by 32 minutes, with one such allocation consuming it for the rest of the session. That is now KRN-004, and it is why a S51tamafb has to start from rcS. Getting a trustworthy number took correcting two things that would otherwise have produced a wrong one. The first is that the flash baseline was initially read as 141k against entry 21's 204k on what looked like a regression after a reboot, and the cause was not the reboot: the touch daemon was running, it had not been running during entry 21's measurement, and stopping it restored 204k on the same boot, which makes the daemon's cost a finding in its own right at about 30 percent of throughput because it polls the CST9217 over the bit-banged bus every 60 ms and each poll NAKs and prints at KERN_ERR. The second is that the PSRAM arm can only be sampled once per boot, so the first single reading of 273k was re-taken on a fresh boot before being believed, and the two agree to 0.01 percent. With tbtns restored to running and the board left on the flash ROM, both services are up and the flash baseline is intact. The reference was updated to match: EMU-008's consequence now records the lead as closed and quantified, EMU-009 holds the A/B and EMU-010 the daemon cost, KRN-004 records the allocation window, and the index and routing table were extended to 63 records. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates at 63 records with 63 index keys, no orphan in either direction, every routing target resolving and all fences balanced, and this entry was validated against the template with six sections in canonical order, sequential numbering from 21, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The ROM should move to PSRAM, and it is the cheapest large win left: 33.8 percent for a small change to the Tamagotchi front end rather than an interpreter rewrite, with the constraint that the allocation must happen at boot, which the still-unbuilt S51tamafb was already going to do. Alongside it, and worth a comparable amount for no memory at all, the touch daemon should stand down while a fullscreen app owns the panel, which is the same coordination the console loglevel already needs. Those two are worth more together than the block-register rewrite that entry 21 recommended, and they are far smaller, so they should come first and the rewrite should be re-justified against a 6.01x baseline rather than the 4.49x one. The push of entry 21 was completed at the start of this cycle, `main` is at 199dc64 on origin, and this cycle's commit follows it.

#### Files Modified:

- docs/tamagotchi.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: NOT RUN

---

## 21 COMMIT Unreleased 2026-11-10T08:23:11-07:00

#### Coming From:

Unreleased b02dfb7

#### Purpose:

Take the interpreter measurement entry 18 left open, so the decision between a block-register rewrite and the core-0 design rests on counts instead of arithmetic on timings.

#### Outcome:

The emulator now has a measured decomposition and a baseline to be judged against. Running the real V3 ROM for 40,000,011 emulated cycles on the watch with the prof counters enabled retires 454305 instructions in 736660 interpreter steps and 282355 sleep steps, performing 1032471 reads and 61692 writes, so one emulated instruction costs 1.622 steps and 2.408 guest bus accesses; the steps figure is exact rather than approximate because 736660 minus 282355 is 454305, which is to say the interpreter takes exactly one step per opcode and the extra 0.622 is sleep ticks. Wall clock on the plain build is 2.236 s, three consecutive runs giving 204045, 203945 and 203488 instructions/s at 4.48 to 4.49x realtime, so an emulated instruction costs 4922 ns, about 1181 native cycles at 240 MHz. That answers the question the count was for and it answers it in the useful direction: memory is 2.408 accesses at a measured 53 ns, about 128 ns or 2.6 percent of the total, so 97 percent of the cost is the interpreter's own instruction stream, which is exactly what a block-register rewrite removes. Dividing the residual by the cost of a simple dependent operation puts the interpreter at the order of 150 native instructions per emulated instruction, between the 30-80 a tuned core needs and the 250 entry 18 had derived, so the headroom is real and it is in userspace. Two supporting measurements close the obvious objections. Instruction fetch is not the cost even though there is no I-cache: three tests running identical arithmetic and identical dependent table loads over 64 bytes, about 1 KB and about 8 KB of code gave 56.83, 30.52 and 31.69 ns per operation, so expanding the footprint eightfold costs 3.8 percent and a rewrite need not fear growing its code. And the ROM's location is a genuine lead: a dependent read from the linear flash window costs 738.04 ns over a 256-byte window and 757.05 ns over 16 KB against 53.13 to 55.67 ns for the same arithmetic on PSRAM, a control that isolates the window at a factor of 14, and since the ROM is 72.6 percent of the emulator's accesses, moving it into PSRAM is worth a predicted fifth to a quarter of throughput. That is stated as a lead and not a result because it could not be A/B'd: a 512 KB contiguous block does not exist once the board has been up a few minutes, measured at 256 KB largest free block with the allocation failing as 'cannot allocate 524288 bytes for the ROM', so it needs a fresh boot. Getting this far required fixing the instrument three times, and all three are worth recording. The 601 KB compiled-in build does not exec at all, refused with 'nommu: Allocation of length 565248 from process 207 (corebench) failed', which also corrects entry 18's ~575 KB estimate and shows the requirement falls on a loaded segment because the ROM lands in .rodata rather than .bss; the build script gained a third variant, corebench-flash, which reads the ROM through the window at 0x42480000 and allocates nothing, and that is the one that ran. The file-based variant had always been failing on a bug rather than on the board, asking malloc for the file length plus a second full ROM copy, 1052672 bytes for a 512 KB image, which is the defect the harness's own prepared diff describes; it is fixed to allocate exactly n bytes and to report the failure instead of returning silently. And two of the measurements were initially wrong in ways that would have produced confident nonsense: the code-footprint benchmark reported 0.00 ns per operation for its middle variant because the compiler folded a chain whose data and trip counts were visible, fixed by seeding the table from the clock and routing the bounds through a volatile, and the dependent-load figure of 178.83 ns carried in EMU-003 turned out to come from a run of the original memlat.c that was crashing on this NOMMU heap, so the true PSRAM latency is 50.6 to 53.4 ns. The reference was corrected to match: EMU-003 and EMU-005 revised in place with their errors stated, and EMU-006, EMU-007 and EMU-008 added for the decomposition, the fetch result and the flash-window cost, with the index and routing table extended. Emulation is deterministic, which gave a free correctness check: every build reports 454305 instructions and 88.05 cycles per instruction, matching the host build exactly, and romverify confirmed the ROM in the slot first with crc32 301F0E87 and first bytes A5 5F 10 49. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates as 60 records with 60 index keys, no orphans in either direction, every routing target resolving and all fences balanced, and this entry was validated against the template with six sections in canonical order, sequential numbering from 20, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Two things are now worth doing and one of them is cheap. The cheap one is the ROM-location A/B: on the next fresh boot, before anything else allocates, deploy corebench-rom with the ROM file and run it against corebench-flash on the same boot, which either confirms the predicted fifth to a quarter of throughput or kills the lead, and the largest free block at boot needs to be read first because the run is only meaningful if it is at least 512 KB. The other is the block-register interpreter, which is now justified by measurement rather than by estimate -- 97 percent of the cost is the interpreter's own instruction stream, 150 native instructions per emulated instruction against a target of 30-80, and fetch is nearly free so a larger code body is not a risk -- and it has a baseline to be judged against at 204045 instructions/s and 4.49x. Neither is started. Beyond the interpreter, the parked ~1.5 s time-screen wait remains the same problem from the other end, and the display items are unchanged: LCD-009's tearing root cause, and the 4.2x disagreement between LCD-005 and LCD-010.

#### Files Modified:

- userspace/memlat.c
- userspace/codecost.c
- scripts/build-corebench.sh
- docs/tamagotchi.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 20 COMMIT Unreleased 2026-11-10T07:56:53-07:00

#### Coming From:

Unreleased f7803d2

#### Purpose:

Make corebench, the only instrument that runs the SPLB32 interpreter with no panel behind it, a one-command build and deploy so the interpreter cost that entry 18 left unmeasured can be taken on the next hardware session.

#### Outcome:

`scripts/build-corebench.sh` now cross-compiles the harness's corebench from `../TamaHarness/portable` in two shapes, with the same non-optional `XTENSA_GNU_CONFIG` export as the other userspace builds, and prints each artifact's largest `PT_LOAD` because that segment is what exec must satisfy contiguously on NOMMU. The compiled-in variant is 601,780 bytes with a 562,700-byte load, and the ROM lands in `.rodata` rather than `.bss`, which corrects the ~575 KB figure entry 18 estimated before the build existed; the small variant is 78,364 bytes and takes the ROM as a path argument, `malloc()`ing 512 KB instead. Both were rebuilt from scratch after the first attempt and came back byte-identical, and the host build runs, reporting 454,305 instructions over 40,000,011 emulated cycles at 88.05 emulated cycles per instruction, which matches the shape recorded in TamaHarness's RESULTS-3.md and is the available correctness signal for the sources. One defect was introduced and fixed inside the cycle: the script omitted `rom_embed.c` from the compiled-in variant's source list, and the cross linker did not merely report the undefined `rom_embed_len` but then segfaulted on a BFD assertion, which is worth knowing because it looks like a broken toolchain rather than a missing file. The finding that matters more than the script is negative, and it changes the plan: corebench cannot count native instructions, which is exactly what EMU-005 needs. This board has no retired-instruction counter -- the Xtensa exposes `CCOUNT` for cycles and there is no PMU on the ESP32-S3 -- and the obvious host substitute is also closed, because `perf_event_paranoid` is 4 with no `CAP_PERFMON` so `perf stat -e instructions` refuses, and valgrind is not installed. Native cycles per emulated instruction are measurable and measured, 3676 ns at 240 MHz being about 880 cycles, but the instruction count inside them is not, because with no cache and PSRAM stalls the IPC is low and varies with what the guest executes. `docs/tamagotchi.md` now carries a section stating all of that and naming the two real routes to a count, static disassembly weighted by a measured opcode histogram or a `qemu-xtensa` run with instruction counting, along with the cheaper alternative: skip the count, do the block-register rewrite, and measure instructions/s against this baseline, which answers whether the 2-3x exists without first proving the mechanism. Deployment and user test did not run because the watch is not connected. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected and contains only this log entry, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 19, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The build is ready and the measurement is one command once the watch answers on 10.10.0.15 or enumerates as `/dev/ttyACM*`: deploy on a fresh boot, because both variants need a large contiguous block and the largest free block is 256 KB after a few minutes of uptime against about 850 KB at boot. The user needs to choose how the interpreter question is settled, since the count cannot be taken as originally planned: weighted static disassembly and a `qemu-xtensa` icount run both cost real work, while the block-register rewrite measured against the new baseline costs the same effort as the optimisation itself and answers the question that matters. Until that is decided, nothing else in the emulator thread should start, and the parked ~1.5 s time-screen wait remains the same problem from the other end.

#### Files Modified:

- scripts/build-corebench.sh
- docs/tamagotchi.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 19 COMMIT Unreleased 2026-11-10T07:54:49-07:00

#### Coming From:

Unreleased 99f3d13

#### Purpose:

Repair the `.ai` control plane, which had drifted far enough that an agent following the recovery policy could not reach the newest engineering records or trust the tracked handoff.

#### Outcome:

Three defects of the same kind were found and closed. Six records -- EMU-001 through EMU-005 and LCD-011 -- had been appended after section 8 instead of into section 5, so the file's own records section ended at EXT-001 while five emulator records and the 40 MHz panel record sat below the retired-id table. `EMU` was used as a `kind` and a `topic_id` on those six records but was absent from `record_kinds`, from the section 2 topic catalog and from the template's kind list, so the file declared thirteen kinds and used fourteen. And section 4's fast lookup index, which is where the policy says to look first, carried no `EMU` line and no `LCD-011` line at all, which meant the seven routing rows pointing at EMU resolved to keys that did not exist; section 3 routing itself was already correct. The six records were moved into section 5 in their existing order with their text unchanged, `EMU` was registered in all three places, `LCD-011` and the five `EMU` lines were added to the index, `last_reviewed` moved to 2026-11-10 and the file's purpose line now names the emulator. Verification is structural rather than eyeballed: 57 records, 57 unique ids, 57 index keys, no record without an index line and no index line without a record, every routing target resolving to a record, all fourteen kinds used also declared, all twelve code fences balanced, and all 57 records inside section 5. The tracked handoff at `.codewhale/handoff.md` was by then seventeen entries stale -- it was written from entry 1 and still claimed `main` at `fd5f252` with 35 records across twelve topics, its "next step" was touch input (delivered in entries 3, 4 and 13) and its "known accepted defect" was the `sm_bonding` pairing failure (fixed in entry 12) -- and because it is tracked deliberately so a clean clone can find the baton, a fresh agent would have started from a plan already completed; it has been rewritten against entries 17 and 18. Two facts about this cycle: the watch was not reachable, with no `/dev/ttyACM*` present and 100 percent packet loss to 10.10.0.15 from a host holding 10.10.0.42 on the same subnet, so no build, deployment or user test applied; and the active log's entries 18 and 17 had been prepended in that order ahead of 1 through 16, so this entry was prepended in turn rather than reordering settled history. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, this entry was validated against the template with six sections in canonical order, sequential numbering from 18, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, `Files Modified` written as None because the cycle changed only project-control metadata, and Status values within the allowed set.

#### Next Steps:

The road to the records is clear, so the measurement entry 18 left open is the work: EMU-005 is still OPEN because the native-instruction count per emulated step is derived from timing and has never been counted, and the decision between the block-register interpreter and the core-0 design turns on whether that figure is near 250 or nearer 80. Counting it needs the watch, which is currently unpowered or unplugged, so nothing on hardware can proceed until it answers on 10.10.0.15 or enumerates as `/dev/ttyACM*`; a dynamic `corebench` build prepared for that moment is the next cycle. After the count, the parked ~1.5 s time-screen wait is the same problem seen from the other end and needs no separate work.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 18 COMMIT Unreleased 2026-11-10T02:00:00-07:00

#### Coming From:

Unreleased 5b05745

#### Purpose:

Find what actually limits the Tamagotchi emulator on this board, after two
sessions of optimising the wrong thing.

#### Outcome:

The limit is the port, not the chip, and the evidence now says so three
independent ways. First, per-second telemetry taken across a real menu press on
hardware: the emulator's own cost goes from 245-290 ms per second while the pet
idles to 900-1140 ms while a menu is open, while the panel figure FALLS from
50-110 ms to 18-28 ms. The display was never involved; a 580 ms ROM transition
becomes about 1.5 s because the guest gets a third of the real time it needs.
Second, the arithmetic: 3.68 us per emulated instruction measured against a
12.77 ns native yardstick, and 26-38 ns for any single memory access. Third, the
mechanism: the core configuration the kernel and toolchain are built against
declares XCHAL_DCACHE_SIZE=0, XCHAL_ICACHE_SIZE=0 and XCHAL_HAVE_MMU=0, so there
is no cache at all -- confirmed from the other direction by a three-working-set
pointer chase on the watch (4 KB 70.30 ns, 256 KB 68.72 ns, second buffer
78.93 ns) which shows no locality effect whatsoever, and impossible if a cache
were operating. The device tree gives Linux exactly one memory region,
memory@0 at 0x3d800000 for 8 MB of PSRAM, so there is no internal SRAM to move
the hot state into either. Both memory-level escape routes are closed.

The architectural consequence is the interesting result and it is new: both cores
are identical Xtensa LX7 at 240 MHz, but core 0 -- the firmware side -- is the
one with cache enabled and internal SRAM reachable, while Linux runs on core 1
alone (CONFIG_HAVE_SMP is not set and XCHAL_HAVE_EXCLUSIVE=0, so SMP is not
supportable). The emulator is therefore running on the one core of two that has
no cache. Moving emulation to core 0 and leaving core 1 to blit is the plausible
full-speed design, and it is deliberately NOT started: it needs firmware work in
the tree that must never be pushed, core 0 owns the WiFi link this project's
console depends on, and starving it trips the FreeRTOS watchdog (BLE-004).
The recommended next step is smaller and safer: the interpreter retires roughly
250 native instructions per emulated instruction against the 30-80 a tuned core
needs, so keeping guest registers, flags and PC in host variables across a
decoded block is a 2-3x lever entirely in userspace. That figure is derived from
timing and has NOT been counted -- two attempts were defeated by deploys failing
for lack of memory, and counting it is the first task for whoever continues.

Delivered and verified along the way: USB deploys fixed (scripts/usb-send.py
streams through one console session, 249 KB in 12 s against the previous 370
separate logins taking about 18 minutes, 4/4 large and 4/4 small); the
interpreter converted to fixed-point after measuring doubles at 711 ns against
121 ns for integers, proven equivalent over 200M cycles and across a button
press; 40 MHz qualified on the panel with eight full-width bands and made the
driver's compiled-in default, buying 13 percent rather than the 2x hoped for,
which is itself the evidence that transfers are not clock bound; and the
emulator given EEPROM save persistence so a pet survives a restart.

Two mistakes worth recording because they cost real time. An interrupted 16 MiB
restore wiped /home down to five files, which looked like a boot loop; the
signature is a half-written jffs2 partition, and the fix is to let the restore
run to completion rather than killing it, with nothing else holding the port.
And a boot loop I initially blamed on the 40 MHz kernel was almost certainly the
interrupted flash itself -- the same clock had booted and been played for a long
session beforehand. esptool also needs --after no_reset and a modest baud when
the board is reset-looping, or it cannot sync.

#### Next Steps:

Count native instructions retired per emulated step before writing any
interpreter restructure; EMU-005 records that this is derived and unmeasured.
The decomposition benchmark exists at userspace/../TamaHarness/portable but needs
a build that fits -- the watch has about 850 KB available and a 700 KB static
binary will not run, so build it dynamically linked (575 KB) and clear the
scratch binaries out of /home first. If the count is near 250, do the
block-register interpreter for the 2-3x; if it is nearer 80, this port genuinely
tops out around 0.5x and the honest answer is the core-0 design or nothing.
After that, the parked ~1.5 s time-screen issue is the same problem seen from the
other end and needs no separate work. The watch is on the 40 MHz kernel with
spi_hz=40000000 as the compiled default, /home intact, and no emulator running.
Newest backup is ~/twatch-backup/working-2026-11-10-0023.bin, sha256 9c60af8a --
it predates the 40 MHz kernel, so restoring it returns the panel to 20 MHz and
the clock has to be reflashed afterwards.

#### Files Modified:

`.ai/core-reference.md`, `docs/tamagotchi.md`, `userspace/memlat.c`,
`scripts/usb-send.py`, `scripts/watch-deploy.sh`, `scripts/watch-exec.py`,
`patches/kernel/display/final/co5300.c`, `userspace/tbtns.c`,
`userspace/run-tama.sh`, `.gitignore`

#### Status:

- Build: PASS (kernel 40 MHz default; userspace binaries)
- Deployment: PASS (kernel flashed and verified; /home restored and intact)
- User Test: PASS (menus measured on hardware; pet hatched and playable)

---

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
out clean and in order, which a desynced write pointer cannot produce, so it is
now the driver's compiled-in default. It was briefly a boot script poking sysfs,
which was backwards -- a qualified constant is a default, not a startup action --
and that script has been deleted; the module parameter remains only as an escape
hatch for a panel revision that does shear. The clock bought
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
`scripts/watch-exec.py`, `userspace/tbtns.c`, `scripts/usb-send.py`,
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
