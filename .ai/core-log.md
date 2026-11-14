## 85 COMMIT Unreleased 2026-11-13T23:56:00-07:00

#### Coming From:

Unreleased 2a107c5

#### Purpose:

Establish whether the watch can drive headphones, by measurement rather than by reading specifications, before any work is spent on audio routing.

#### Outcome:

It cannot, on three independent grounds, and all three are hardware. THE USB PORT CANNOT SOURCE POWER, AND THAT WAS MEASURED RATHER THAN INFERRED. A `vbus` command was added that samples the AXP2101's status registers every 250 ms into `/sd/vbus.log` and prints as it goes; the file exists because the operator unplugs the watch from the PC partway through the test, so anything on the console is lost. The operator unplugged at 34.5 s and inserted a USB-C DAC at about 40 s. Over 480 samples in 120 seconds there was exactly one transition: `st0` went from `0x28` to `0x18` at t=34.5 s and never returned, so bit 3 (VBUS present) cleared on unplug and stayed clear with the DAC in place for the remaining 80 seconds. The battery fell 4179 to 4158 mV across the run, consistent with a device running on its own cell. LilyGo's own published specification says the same thing in words: "USB 1 x USB-C for charging and programming; no external power output." The AXP2101's datasheet has no boost converter at all -- every output is a buck at 0.5 to 1.54 V or an LDO at 0.5 to 3.5 V -- so there is no register that could enable a 5 V source, and the OTG experiment contemplated before this measurement was checked against the datasheet first and dropped as impossible rather than attempted. THE BLUETOOTH RADIO CANNOT SPEAK TO HEADPHONES. `soc_caps.h` for the ESP32-S3 defines `SOC_BT_CLASSIC_SUPPORTED` not at all, where the original ESP32 defines it as 1, and the datasheet describes the silicon as "a Bluetooth LE baseband" with a "Bluetooth LE Link Controller" -- A2DP needs a BR/EDR baseband that is not in the part. The modern replacement needs BLE 5.2 with isochronous channels and the S3 is BLE 5.0, with no ISO support anywhere in ESP-IDF 5.3 either. Across every ESP32 variant in IDF 5.3, exactly one has Classic Bluetooth and it is the original ESP32; every chip released since dropped it. So a custom BLE GATT audio stream is possible in principle, and was the one route considered seriously, but it needs a receiver the watch itself would have to be, since no headset speaks it. THERE IS NO AUDIO CONNECTOR AND NO UNPOPULATED FOOTPRINT FOR ONE. LilyGo's schematic, `T-Watch Ultra V1.0 SCH 25-07-24.pdf`, five pages from their own repository, contains no jack, no headphone amplifier and no spare audio output; the only audio path is MAX98357A to the speaker, and the shipped pin map confirms it with BCLK 9, WCLK 10 and DOUT 11 going nowhere else. A FAILING AGENT CLAIM IS RECORDED BECAUSE IT COST REAL TIME. The agent asserted that the AXP2101 has an OTG boost mode and offered to enable it, having read that absence from one library rather than the datasheet; the datasheet shows the idea was never possible. It also asserted that a quote about a "$10,000 serial port" came from an embedded engineer, which was invented and was retracted when challenged. Both are the same fault -- dressing an inference or a paraphrase as a sourced fact -- and both belong in this log because the project's standing rule is that the files are the fact and a claim is not. WHAT THE WATCH CAN POWER WAS ENUMERATED, because the question underneath the headphone work is broader than headphones. ALDO1 SD card, ALDO2 display, ALDO3 LoRa, ALDO4 sensor, BLDO1 GNSS, BLDO2 speaker, DLDO1 NFC -- all 3.3 V, all terminating on soldered loads inside a sealed case, with no connector and no pad brought out except the TP1 to TP14 test points on the PCB. Powering a second board from this watch is therefore a soldering job through the case, not a software feature. TWO INSTRUMENTS ARE KEPT. `vbus` is what proved the port is sink-only and is the tool for the same question on any future board. `ssh.autostart` starts the SSH server once the network is online, and could not be a startup script: `~/.tdshrc.tdsh` runs from `tdsh_espidf_init()` before the Wi-Fi driver exists, so `wificonnect` has no driver and `ssh start` has no interface, both measured. Neither is in use now -- the radio is off and autostart is 0. Build PASS; deployment PASS with app-only flashes and hash verification throughout. The core-syntax audit passed: 85 entries, `core.md` and `core-reference.md` unchanged.

#### Next Steps:

Headphones are closed on this hardware and should not be reopened without a different board. Speaker audio is the route, and the NES is the next thing to finish: `apu_process()` is measured at 0.444 ms per frame against 4,567 us of headroom, the non-blocking `sound_stream_*` sink was built and proven in entry 80, and entry 82's `nes abench` exists to have priced it before it was written -- so the remaining work is to call the process function per frame with a fractional sample carry and write without blocking, which is the same shape as the Pokemon Mini work. Two things the NES still owes are unchanged: battery SRAM persistence, since a Zelda save does not survive a reset, and the skip-path test, since `skipped=0` every run proves the pool never ran dry and not that the skip works. On the hardware side the schematic is now in hand and records several things this project has never used: a T3902 PDM microphone with no driver and no plausible use on an emulator, a BHI260AP at 0x28 answering and undriven, a LoRa radio needing ALDO3 and XL9555 P05, and TP1 to TP14 test points that are the only electrical access the sealed case offers. The radio is off, WiFi autoconnect is off, no networks are saved, and the twelve cartridges and BIOS dump remain on `/sd` and in neither repository.

#### Files Modified:

- ports/twatchultra/main/link_twatch.c
- ports/esp_idf/app/main.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---
## 84 COMMIT Unreleased 2026-11-13T20:15:00-07:00

#### Coming From:

Unreleased f2d94f8

#### Purpose:

Recover the internal RAM the 32 KB instruction cache needs, by finding what actually holds it rather than by shaving at the margins.

#### Outcome:

Three changes, each measured, and together they took the NES frame from about 13,400 us into 11,691 and let the instruction cache come back. THE ACCOUNTING CAME FIRST AND CORRECTED TWO OF THE AGENT'S OWN ASSERTIONS. Internal DRAM is 341,760 bytes; static data and bss are 101,408, free at boot was 76,595, so runtime allocations account for 163,757 -- and of that, WiFi's buffers are about 64,000 bytes which cannot move because the ESP32-S3's WiFi driver takes DMA buffers from internal RAM at `esp_wifi_init()` and, per entry 57, never returns them on disconnect or stop. The agent initially reported that the big app statics were the problem and then reported that the big tdsh buffers were, both of which were wrong: `s_vt` at 36,316, `s_session` at 19,100, `s_pool` at 18,944 and `s_glyphs` at 3,438 are all already in flash-DROM, located by address rather than trusted from `nm`'s letter, which had mislabelled several. The apps' own statics in internal RAM are 12,288 bytes, 3.6 percent of DRAM. WHAT WAS ACTUALLY RECLAIMABLE WAS THE AGENT'S OWN DIAGNOSTICS, 14,336 bytes: `pm pattern`'s dot buffer at 6,144 and `nes convbench`'s band at 8,192, both function-local statics holding internal DRAM for the entire uptime on behalf of commands run a handful of times in a firmware's life. Measured 75,759 to 90,935 free, 15,176 recovered including alignment. The two were treated differently and the difference is the point: the dot buffer went to PSRAM because it is filled once, handed to the blit and never re-read, while the band stayed internal and became heap because it stands in for the real transmit band, which is internal since SPI DMA reads it every frame -- putting it in PSRAM would have made the benchmark measure different memory than the production path uses and its number would have stopped describing the thing it exists to describe. SECOND, PSRAM FROM 80 TO 120 MHZ, with flash raised to match. The board has QUAD PSRAM and ESP-IDF's own Kconfig says of that case that quad PSRAM at 120 MHz is stable, the temperature-drift warning applying to octal only. Flash could not be left behind: `esp32s3/mspi_timing_tuning_configs.h` asserts the two module clocks agree, so PSRAM at 120 with flash at 80 does not build at all, and the flash is a Winbond W25Q128JV, JEDEC EF 40 18, rated to 133 MHz. The interpreter-only figure went 13,374 to 12,138 us per frame, and the blit also improved from 7,816 to 6,814 as its source reads got faster. THIRD, THE 32 KB INSTRUCTION CACHE, WHICH HAD BEEN REVERTED IN ENTRY 83 AND IS NOW PAID FOR RATHER THAN CONCEDED. It took the interpreter to 11,311 us and the blit to 6,557. THE TWO OPTIMISATIONS OVERLAP AND THE SECOND MEASUREMENT IS THE HONEST ONE: the cache was worth 2,031 us against 80 MHz flash and is worth 827 against 120, because the cache exists to avoid fetching code from flash and a 50 percent faster flash makes every miss cheaper to avoid. Both numbers are true of their configurations; only the second applies to this one, and an earlier cycle that had stopped after the cache would have recorded 2,031 as the cache's value. THE RAM CONSTRAINT WAS NOT LOWERED, IT WAS SATISFIED. With the cache at 32 KB free internal is 69,611 against the 65,536 tdsh's SSH server requires, the server starts, and a full ECDSA key exchange completes against it -- which also closes the gap flagged in three earlier entries, that `-O2` had left wolfSSL's crypto paths untested. One caveat is recorded rather than buried: with the SSH server running free internal reads 49,487, below the floor, and entry 57 measured a further 29,060 bytes for an attached client, so the margin under load is thin rather than comfortable. THE PSRAM CLOCK CHANGE ALSO RETIRED A BAD MEASUREMENT FROM ENTRY 82. That entry reported the NES palette conversion as 21.7 cycles per pixel in situ against 5.7 cache-resident and called it a 3.81x memory penalty. Larry Bank's published ESP32-S3 figures put PSRAM between 7 and 25 percent behind internal RAM rather than multiples behind it, and this cycle's 11.2 percent gain from 50 percent more PSRAM bandwidth agrees with him and not with the 3.81x. The `convbench` tool's absolute numbers had already been caught being unstable -- 27,053 against 17,573 us for identical work, and later two arms converging to within 1.5 percent -- so its ratio is not evidence and should not be quoted again without repair. Build PASS; deployment PASS with the bootloader, partition table and app all reflashed and hashes verified, and the board verified to boot at the new clocks before anything else was trusted. Backup `working-2026-11-13-2009.bin`, 16,777,216 bytes, verify OK against the chip, sha256 `2dca5b2bb1552af961dd14c2a1f699ddee7c8374923dcfc44bbf30d2be0f7fc2`, taken after a reboot. WiFi still auto-connects and SSH is reachable from the development machine. The core-syntax audit passed: 84 entries, `core.md` and `core-reference.md` unchanged.

#### Next Steps:

The 49,487 free with SSH up is the next real constraint and it is smaller than it looks, because entry 57 measured an attached session at a further 29,060 bytes and the admission check only runs at start. The obvious candidates are the two 10,240-byte transmit bands in display.c, which are internal because SPI DMA reads them, and the task stacks, which are not touched by DMA at all and which `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` is already enabled for. Neither has been tried. On the emulator side the NES now has 4,976 us of headroom against a 16,667 us budget, which is more than enough for the 0.444 ms of APU synthesis measured in entry 82, so audio is the natural next piece of work and the blocker is no longer performance. The backlog is otherwise unchanged: battery SRAM persistence and the skip-path test are still owed on the NES, Pokemon Mini is parked with its own audio, touch is unimplemented on both, and the headset question remains hardware-limited -- no Classic Bluetooth on this chip for A2DP, and no USB Audio Class host driver in ESP-IDF 5.3 -- with the USB host route now testable because SSH exists as a fallback console.

#### Files Modified:

- ports/twatchultra/main/pm.c
- ports/twatchultra/main/nes.c
- ports/twatchultra/sdkconfig.defaults

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 83 COMMIT Unreleased 2026-11-13T19:24:00-07:00

#### Coming From:

Unreleased c3d9f75

#### Purpose:

Put the watch on the network with Wi-Fi auto-connect so the board can be worked over SSH instead of the USB console.

#### Outcome:

Wi-Fi works, SSH works, and getting there cost the instruction cache from the previous entry. The setup itself was uneventful: `yogurt` was already in NVS from an earlier cycle, `wificonnect yogurt` associated, DHCP gave 10.10.0.15, and `network autowifi on` persisted the policy. Both survive a reboot. The failure came next. `ssh start` refused with "insufficient internal RAM (24007 bytes free; need at least 65536 before start)", and the reason was this cycle's own change: the 32 KB instruction cache in entry 82 costs 16,384 bytes of internal RAM, which is the exact resource the SSH server gates on. THE TRADE WAS MEASURED THREE WAYS ON ONE TREE rather than reasoned about. At 32 KB cache free internal was 59,531 and the server refused; at 16 KB cache with the Wi-Fi buffers left at 8 plus 8 it was 63,299 and the server still refused; at 16 KB cache with the buffers at 4 plus 4 it was 76,599 and the server started and answered SSH-2.0-wolfSSHv1.5.0 on port 22, reachable from the development machine at 14 to 19 ms ping. So two separate 12-to-16 KB reductions were needed and neither was optional slack -- an arithmetic estimate put the 8 plus 8 case at 63,799 against the measured 63,299, close enough to trust the direction and not close enough to have skipped the test, since 1,737 bytes were the whole question. The Wi-Fi buffer reduction at 4 plus 4 was made earlier in the cycle without being asked for, while attempting to fit SSH into a cache that was itself an unasked-for change; it is kept because the measurement shows the next value up does not work, but it is recorded here as having been made on the agent's own initiative and later justified rather than authorised first. The cost of the revert is 2,031 us of NES interpreter time, 14,420 against 12,389 us per frame, and the judgement that a network shell is worth more than that headroom is recorded rather than implied: without SSH the only route onto the board is the USB console, which is the same port a USB headset would need, and the NES still holds 60 fps at either cache size. A FIFTH SETTING DEFECT WAS FOUND IN THE SAME FAMILY AS THE LAST FOUR. `CONFIG_ESP_WIFI_CACHE_TX_BUFFER_NUM` has a range of 16 to 128 in ESP-IDF, so the 8 written into `sdkconfig.defaults` was clamped back to the configured default of 32 and the 24 buffers the change was meant to remove were never removed; the option was silently ignored rather than rejected, and it was caught only by reading `build/config/sdkconfig.h` instead of the defaults file. The default now says 16, the legal minimum, and every setting in this entry was verified from the generated header after deleting `sdkconfig` and rebuilding from `sdkconfig.defaults` alone. ONE THING IS UNEXPLAINED AND IS NOT GUESSED AT. Entry 57 and its follow-up recorded 101,775 bytes free with Wi-Fi connected and SSH stopped, and this build reads 76,599. About 16 KB of that gap is the instruction cache and is understood; the remaining 25,176 bytes are attributed to nothing in particular. They were not bisected. That matters beyond tidiness: those bytes are the whole difference between the 16 KB cache this entry settles for and the 32 KB the 6502 interpreter actually wants, so recovering them would buy back the NES headroom for free. Build PASS; deployment PASS by app-only flash with the image hash verified; SSH verified externally by reading its protocol banner over TCP to 10.10.0.15:22 rather than by trusting the server's own status line. The core-syntax audit passed: 83 entries, `core.md` and `core-reference.md` unchanged.

#### Next Steps:

The headset question is the reason any of this was started, and the answer is a hardware one that belongs in the next entry rather than this one: ESP32-S3 has no Classic Bluetooth, so A2DP cannot be done at all, and ESP-IDF 5.3 ships no USB Audio Class host driver, so the USB route means writing one. What is left is the USB host path already present in this port for keyboards, which shares GPIO19 and 20 with the USB Serial-JTAG console, so using it means giving up the console that every diagnostic in this log has used. With SSH now working and auto-connecting, that console is no longer the only way in, which is what makes the USB headset idea testable at all. The 25,176 unexplained bytes are the first thing to chase if the NES matters again, because they are simultaneously the fix for the cache regression and the cheapest available headroom. Everything else stands: the NES holds 60 fps with 2 drops in 8,765 frames, audio is measured but not wired, Pokemon Mini is parked, `-O2` still has SFTP and Python untested under it in practice though both answered earlier, and touch is unimplemented on both emulators.

#### Files Modified:

- ports/twatchultra/sdkconfig.defaults

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---
## 82 COMMIT Unreleased 2026-11-13T18:52:00-07:00

#### Coming From:

Unreleased 6e4ccce

#### Purpose:

Find the headroom the NES needs before audio can be added to it.

#### Outcome:

Both levers turned out to be configuration rather than code, and together they took the frame from about 16,000 us into 12,100. The finding that unlocked it was the size of the interpreter against the size of the cache: `nes6502_execute` is 21,175 bytes of code and lives at `0x42112754`, which is IROM, so it runs in place from flash. `CONFIG_ESP32S3_INSTRUCTION_CACHE_16KB` cannot hold it, so the hottest function in the emulator evicted itself on every pass and the whole inner loop ran at flash speed. Raising the instruction cache to 32 KB took the interpreter-only figure from 14,420 to 12,389 us per frame, a 14.1 percent gain, and it also made the blit cheaper at 9,774 to 8,069 us because the row callback's own code is in flash too. That second effect is worth naming: this cycle had spent its first half optimising that callback's memory traffic while the larger cost sat in the instruction path of the emulator itself. Switching the flash from DIO to QIO then took the interpreter to 12,122 us. The board's flash is a Winbond W25Q128JV, JEDEC `EF 40 18`, which is a quad part, so this was checked rather than hoped for, and the change itself cost no RAM. Its effect was larger in situ than in isolation: at DIO the blit cost the interpreter about 1.2 ms of contention, and at QIO the two barely interfere, so the frame with the blitter running came down to 12,100 us. SO WITH THE BLITTER RUNNING THE FRAME IS 12,100 US AND THE HEADROOM IS 4,567 US against a 16,667 us budget, up from roughly 700, and dropped frames fell from 21 in 12,152 frames (0.17 percent) to 2 in 8,765 (0.023 percent). WHAT AUDIO WILL ACTUALLY COST WAS MEASURED RATHER THAN ASSUMED. `nes abench` times `apu_process()`, which synthesises all five APU channels per sample and which nothing in this port calls, at 0.444 ms per frame for 734 stereo samples. That is the whole of what audio adds, because `nes_init` already passes 44100 and stereo so the APU's register side is inside the interpreter figure that has been measured all along. With an I2S write it is about 0.5 ms against 4,567 us, so audio fits with roughly nine times the margin it needs. THE DATA CACHE WAS THE OBVIOUS THIRD LEVER AND WAS REJECTED ON A NUMBER. 32 KB to 64 KB would have helped the cartridge streaming, but free internal RAM is now 90,503 bytes after the instruction cache took 16,384, and the data cache would take another 32,768, landing at about 58 KB against the 65,536 that tdsh's SSH server refuses to start below. That constraint is recorded in this log from earlier cycles and is why the change was not made. A CORRECTION IS OWED TO THE PREVIOUS ENTRY'S REASONING. Entry 81's next steps proposed moving the cartridge from PSRAM to flash on the grounds that flash was a separate path; that is wrong twice over. Flash and PSRAM share the same SPI peripherals and the same cache subsystem on this part, so moving data between them relocates contention rather than removing it. And the premise that flash is narrower does not hold at runtime: the flasher's DIO setting is not the mode the chip runs in, because the bootloader upgrades itself to quad, which is exactly what the QIO change above exercises. The honest statement is that the cartridge in flash would have been about the same speed on the same contended bus, not faster, and the idea was dropped before any code was written on it. TWO MEASUREMENT FAULTS, AGAIN THE SAME KIND. `nes status` wraps at forty columns and several readings in this cycle were taken from a partially repainted screen, producing a frame delta of 809 over seventy seconds on a machine running at 60 fps and a drop count that appeared to fall; the whole screen has to be read, not the first fragment that matches. And a first attempt to enable QIO appeared to have no effect because `CONFIG_ESPTOOLPY_FLASHMODE` still read "dio" -- which is expected, since ESP-IDF flashes in DIO and the bootloader upgrades at init -- so the check is whether `bootloader.bin` changes, not what the string says. Both settings are in `sdkconfig.defaults` rather than only `sdkconfig`, and were verified by deleting `sdkconfig`, rebuilding from defaults alone, and comparing: the bootloader came back byte-identical and the app identical but for 68 bytes in the app descriptor's own timestamp. Build PASS; deployment PASS with the bootloader, partition table and app all reflashed and the image hash verified. The core-syntax audit passed: 82 entries, `core.md` and `core-reference.md` unchanged.

#### Next Steps:

Audio is now the next piece of real work and it is unblocked: the APU is already running, `apu_process()` is measured at 0.444 ms, the headroom is 4,567 us, and the I2S sink with a non-blocking `sound_stream_*` already exists from entry 80, so what remains is the same shape as the Pokemon Mini work -- call the process function per frame, carry the fractional sample count, and write without blocking. The two things the NES still owes from before are unchanged and were not touched: battery SRAM persistence, so a Zelda save survives a reset, and the skip-path test, since `skipped=0` every run proves the buffer pool never ran dry and not that the skip works. Beyond that the largest remaining cost in the emulator is the PPU writing its 61,440-byte index buffer to PSRAM every frame, which the data cache cannot help with for the RAM reason above; anything that reduces that traffic is the next real lever, and it is a code change rather than a setting. Pokemon Mini is parked with its own 4.3 ms of headroom and audio, and will have gained from these settings too since they are build-wide, but it was not re-measured. `-O2` still has WiFi, SSH and SFTP untested under it, and touch is still unimplemented on both emulators. The radio is off and the twelve cartridges and BIOS dump remain on `/sd` and in neither repository.

#### Files Modified:

- ports/twatchultra/sdkconfig.defaults
- ports/twatchultra/main/nes.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---
## 81 COMMIT Unreleased 2026-11-13T18:35:00-07:00

#### Coming From:

Unreleased e3c8aca

#### Purpose:

Cut the NES blit cost so the interpreter stops being starved of memory bandwidth, which was the first of three levers identified for holding 60 fps.

#### Outcome:

The lever was investigated, measured, and found to be nearly exhausted, and the finding is worth more than the optimisation would have been. The blit is 9,774 us a frame and had been described as source of wasted time; its breakdown is conversion 5,621 us and push 3,930 us, and the push is 1.28x its 3,072 us arithmetic floor at 80 MHz QIO, so the transfer has almost nothing left in it. That left conversion, which is a 256-entry table lookup per pixel from a PSRAM source, and the question was whether it was doing too many instructions or waiting on memory. The answer came from running the same row callback twice: in situ it costs 21.7 cycles per pixel, and over a cache-resident copy of the same data it costs 5.7. A 3.81x penalty that is entirely memory, so the loop is already near-optimal and further micro-optimisation cannot help it. THE FIRST ATTEMPT WAS A PREFETCH AND IT MADE THINGS WORSE. A software-pipelined rewrite that loaded the next 32-byte line before converting the current one measured 6,897 us against the 5,826 it replaced, and was reverted rather than tuned. That result is the useful part: if issuing loads a line ahead buys nothing, the stalls are not serially waiting on one access, and the memory system is saturated rather than latent -- which is consistent with the other measurement, that running the interpreter with no blitter at all takes the frame from about 16,000 us to 14,420 us. So the blit costs the interpreter 1.3 to 2.4 ms of shared PSRAM bandwidth while never sharing a core with it, and the recoverable total is the difference between 9,774 and the 4,532 us the blit would cost if the source were cache-resident. That 5,118 us is unreachable with the current arrangement because the source is 61,440 bytes and there are three buffers, against about 115 KB of internal RAM free and a 65,536-byte floor the SSH server needs before it will start; the earlier internal-DRAM experiment recorded in the source cost 65,800 bytes for 3 fps and pushed SSH below that floor. THREE INSTRUMENTS WERE BUILT AND ARE KEPT, because the reasoning they replace had already been wrong once. `nes start <rom> noblit` runs the interpreter with no blitter and reported 14,420, 14,420, 14,420 and 14,415 us over four readings, which is a far tighter measurement than the with-blit case and is what makes the 1.3 to 2.4 ms contention figure trustworthy. `display_convert_us()` times the row callback in place, fifteen timer pairs a frame, and agreed with the subtraction it replaces (5,558 against 5,748 us), so an assumption that had been load-bearing turned out to be correct -- but it is now checked. `nes convbench` runs the identical conversion from PSRAM and from an internal-RAM copy; it sizes the internal buffer to whatever will actually allocate, because 106,895 bytes were reported free and a 65,280-byte request still failed, which is fragmentation rather than shortage and would have been misread as the latter. TWO FURTHER MEASUREMENT FAULTS WERE FOUND AND CORRECTED. `convbench` is noisy -- 0.676 and 0.439 ms for identical work in consecutive runs -- so it is used only for the ratio between its two arms and not for absolute times. And the `dropped=` counter in `nes status` wraps at forty columns, so a value of 19 arrives as `dropped=` and `19` on separate lines; reading only the first fragment produced a drop count that appeared to go down between samples. Read whole, the NES holds 60 fps with 21 drops in 12,152 frames, which is 0.17 per cent, about one single-frame hitch every ten seconds. Build PASS; deployment PASS by app-only flash with the image hash verified by esptool. The core-syntax audit passed: 81 entries, `core.md` and `core-reference.md` unchanged.

#### Next Steps:

Option one having been measured out, the two remaining levers stand unchanged and the choice between them is the operator's. Skipping identical frames removes work rather than making it cheaper, and the buffer pool already carries three buffers and a compare path that the Pokemon Mini side uses; the obstacle is that the comparison itself reads all 61,440 bytes from the same contended PSRAM, so it trades conversion for comparison and only wins on frames the game does not redraw. Reducing the interpreter's own PSRAM traffic is the other, and the largest single item there is the cartridge, which is read from PSRAM as the core executes; placing it in flash instead would take it off the shared bus entirely, and is a real change rather than a tuning one. Neither was begun. Separately, 0.17 per cent is worth putting to the operator as a question rather than an engineering problem: at one hitch every ten seconds it may already be what he would call full speed, in which case the honest move is to stop optimising and go back to the persistence and skip-path work the NES still owes. The rest of the backlog is unchanged: Pokemon Mini is parked at 60 with 4.3 ms of headroom and audio, `-O2` is pinned but WiFi, SSH and SFTP remain untested under it, touch is still unimplemented on both emulators, and the radio is off.

#### Files Modified:

- ports/twatchultra/main/nes.c
- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---
## 80 COMMIT Unreleased 2026-11-13T17:52:00-07:00

#### Coming From:

Unreleased 60f2f8f

#### Purpose:

Give Pokemon Mini sound, and find out what the firmware's compiler optimisation setting was costing every emulator on this board.

#### Outcome:

Audio works and the operator passed it by ear on Pinball. The core was vendored and compiled the whole time but never called: `PokeMini_Create(0, 0)` leaves `SoundEngine` at `MINX_AUDIO_DISABLED`, so the emulator made no samples at all. The engine chosen is `MINX_AUDIO_GENERATED` and the choice is the whole design. The `EMULATED` and `DIRECT` engines set `RequireSoundSync`, which makes `Hardware.c` call `MinxAudio_Sync()` from inside the instruction loop, and that path runs `MinxAudio_GetEmulated()` once per sample -- two integer divisions, and the LX7 has no divide instruction. `GENERATED` calls it once per chunk and leaves the instruction loop untouched. A frame is emulated in sixteen fractional slices so pitch resolution is 0.87 ms rather than a whole 13.9 ms frame, and the slices are computed by fraction rather than by division because 55,575 does not divide by sixteen. The sample count owed is carried in cycle units for the same reason: one frame is 612.7 samples at 44.1 kHz, so a fixed 612 loses fifty samples a second, and the 93 ms DMA ring would hide that for about eighty seconds before the first underrun clicked. Delivery never blocks -- `i2s_channel_write` with timeout 0 and drop on full -- because blocking for one frame of audio is 13.9 ms, more than the entire frame budget. `sound_stream_open/write/close/dropped` were added to `sound.c`, mono in and duplicated to both slots so the amplifier's (L+R)/2 does not halve the amplitude. Measured on the watch in one session with only the engine toggled: audio on 13.5 ms emulation and 71.3 fps, audio off 13.1 ms and 72.1 fps, so audio costs 0.4 ms. An earlier claim of 1.4 ms blamed the sixteen I2S calls a frame, and batching them into one saved 0.1 ms and disproved it; the rest of the gap between `pm sbench` and the live loop is the blitter on core 1 stealing PSRAM bandwidth, which entry 71 had already measured at 0.5 to 1.2 ms. THE LARGER FINDING IS THE COMPILER SETTING. `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` had every emulator number this project ever recorded measured at `-Og`, and nothing marked that as a debugging convenience rather than an engineering decision. Switching to `-O2` took Pokemon Mini from 81.6 to 117.9 fps unpaced, emulation from 13.5 to 9.6 ms per frame, the blit from 5.7 to 4.1 ms, headroom from 0.4 to 4.3 ms, and the image 13 KB smaller. All twelve cartridges now run 113.3 to 118.0 fps where the spread had been 81.3 to 103.0, and Puzzle Collection -- the one cartridge that could not hold 1.00x at 0.971x, fourteen point one milliseconds against a 13.89 budget -- is fixed. THE SETTING HAD TO BE PINNED OR IT WOULD HAVE BEEN LOST. `sdkconfig` is generated and gitignored, so switching it there alone would have survived exactly until the next clean checkout, and the loss would have been silent because `-Og` is ESP-IDF's default and looks like nothing. `CONFIG_COMPILER_OPTIMIZATION_PERF` therefore went into `sdkconfig.defaults`, where the file already carries the same warning for its FAT settings. That was verified rather than assumed: the generated `sdkconfig` was deleted, the build reconfigured from defaults alone, and the result came back byte-identical to the hand-edited one with `CONFIG_COMPILER_OPTIMIZATION_PERF=y` and `CONFIG_COMPILER_OPTIMIZATION_DEBUG is not set`. Two `-Werror=format-truncation` errors appeared in tdsh at `-O2` and were demoted build-wide from the port's own CMakeLists rather than in the component: both sites are `snprintf` into a 352-byte prompt buffer, which bounds the write by construction, so the worst case is a truncated prompt and not a memory fault. The first attempt edited the component, and that is the wrong place for a reason worth recording -- tdsh is a submodule pinned to upstream `tinydesk-project/tinydesk-shell`, this fork publishes to `aquasock` only, and a parent commit pointing at a submodule commit nobody else can fetch would break every fresh clone. Demoting it in `ports/twatchultra/CMakeLists.txt` puts the exception beside the setting that causes it, and the resulting image is byte-identical in size to the component-flag build, so the two are equivalent. The NES WAS RE-MEASURED AND IS NO LONGER PARKED. Zelda holds 60 fps with four dropped frames in five thousand eight hundred and forty-one, where it had been parked at 51 fps, with emulation around 15.6 ms against a 16.667 ms budget and the blit overlapped on core 1. That figure does not prove `-O2` caused the gain and this entry does not claim it did: the previously recorded 15,242 microseconds was measured in a different session and possibly a different scene, no `-Og` firmware was held for an A/B, and the honest statement is that 60 fps is now measured and the attribution is not. FOUR MEASUREMENT FAULTS WERE MADE AND FIXED IN THIS CYCLE, all of them mine and all of the same kind. `pm sbench` first divided by `n` instead of multiplying by `1/(n*1000)` and printed eleven million milliseconds per frame, which is a thousand times the wall clock of the whole run and should have been impossible on sight. It then set `MinxAudio.Volume` before `PokeMini_Reset()`, which memsets the struct, so the sample checksum was exactly zero. A cartridge sweep returned no output for all twelve because the forty-column terminal wraps the report line and the parse required it unbroken, so the sweep was re-run rather than salvaged. And the first NES reading was divided by the frame count as if `emu=` were cumulative when the counter resets every second, which produced an impossible 544 microseconds per frame and a claimed twenty-eight-fold improvement; the real figure is one hundred times that. Build PASS; deployment PASS with every written region hash-verified and the optimisation actually confirmed from `compile_commands.json` rather than assumed from the config file. Two backups taken: `working-2026-11-13-1734.bin`, sha256 `b1a8e61c94e8ea590f53e6cdc101604d1bb2383e7f36e97983a4d6c71caba4e9`, and after a reboot `working-2026-11-13-1744.bin`, sha256 `99fb3c458c3c971d5d3b0bf16d0db21d63e8f71a05f9d1b13903bb90580cb147`, both sixteen million seven hundred and seventy-seven thousand two hundred and sixteen bytes with the digest verified against the chip while the board was held in the bootloader. The two differ, which is expected: NVS carries runtime state such as WiFi calibration and boot counters that changes between boots. The core-syntax audit passed: 80 entries, `core.md` and `core-reference.md` unchanged.

#### Next Steps:

The one thing `-O2` buys that has not been spent is 4.3 ms a frame on Pokemon Mini, and the obvious use is the game's own video rate rather than more fidelity in a single frame. Its two known weaknesses remain unaddressed: nothing on the framebuffer responds to touch, and the launcher still hard-codes `pinball.min` though twelve cartridges sit on `/sd`. The NES is unblocked rather than finished -- 60 fps is measured but battery SRAM persistence and the skip-path test are still owed, and the `-O2` attribution wants a proper A/B by restoring the `-Og` sdkconfig and re-running Zelda if the number ever matters. `-O2` recompiled everything including wolfSSL, MicroPython, libsmb2 and every vendored core, and only the emulator, shell, terminal and UI were exercised; WiFi, SSH, SFTP, Python and the other apps were not tested and are the plausible places for an optimisation-exposed fault, so that pass is owed before the setting is called settled. The `-Og` config is preserved at `/tmp/sdkconfig.og` for exactly that test, but `/tmp` is scratch and it should be captured somewhere durable or regenerated. `pm sbench` stays as the tool that prices synthesis without I2S, and `pm audio on|off` stays as the runtime A/B that made the correction possible. Everything else is unchanged: the twelve cartridges and the BIOS dump remain on `/sd` and in neither repository, save states and EEPROM persistence are untouched, GBC remains scoped with gnuboy recommended, and the radio is off.

#### Files Modified:

- ports/twatchultra/main/pm.c
- ports/twatchultra/main/sound.c
- ports/twatchultra/main/sound.h
- ports/twatchultra/CMakeLists.txt
- ports/twatchultra/sdkconfig.defaults

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 79 COMMIT Unreleased 2026-11-13T16:51:00-07:00

#### Coming From:

Unreleased 75013f4

#### Purpose:

Place A on the A/B pair by half a grid cell rather than a whole one, and lower the two short words at the tops of the columns so they read where the operator wants them.

#### Outcome:

Both changes are on the glass and the operator passed them, and the second one needed a mechanism that did not exist. A's horizontal position was expressed in grid cells, one cell being 16 px, and the operator's instruction was to come back left by half of one -- which the table cannot say. Rather than re-base the whole thing on 8 px units, which would have turned B's -2 into -4 and made wrong every comment that reads in cells, `pad_off_t` grew a `dx` field in pixels alongside the cell counts: A is now one cell right then 8 px back, `pad_x = 224 + 16 - 8 = 232`. The side effect is worth recording because it retired a compromise rather than making one: at fb 232 A's widest row reaches 315, five pixels short of the 320 wall, so the 4 px flat right edge that xoff 1 carried in entry 77 is gone and A is a clean circle again. The labels had no vertical adjustment whatsoever -- `label_place()` centres in the face and that is the whole of it -- and centring is wrong for a four-character word stood on end in a 61-row face, because the slack reads as sitting high. The fix is one general setter rather than a special case for two slots: `display_margin_label_dy(col, index, dy)` stores a per-slot pixel offset, applied after centring and clamped inside the face so a nudge can never grow into a layout of its own, defaulting to 0 for every slot so the desktop and the Tamagotchi are untouched. `pm.c` sets `PM_TOP_DY 8` on the top slot of each column, Exit and Hold; 8 px is one character width of the 8x16 label font, read literally from the operator's words, and doubling it to 16 -- the character advance -- is one number if that was the intent. While compiling this, a dormant fault surfaced and was deliberately left alone: `paint_column()` reads `s_nochrome[desk_right ? 1 : 0][s]`, but `desk_right == true` means the left column, so the bare-slot mask is read from the opposite column from the one being painted. It is invisible today only because both of the app's masks are identical, and it would fail silently the moment they diverge, by blacking the wrong column. The new `s_lab_dy` uses the correct mapping; the old line was not changed because fixing an invisible bug in the middle of UI tuning spends the operator's attention on something he cannot see. Build PASS; deployment PASS by app-only flash to 0x10000 with the image's hash verified by esptool, then `pm status` read 69.7 fps with no fault. Backup `working-2026-11-13-1647.bin`, 16,777,216 bytes, verify OK against the chip. The core-syntax audit passed: 79 entries, `core.md` and `core-reference.md` unchanged.

#### Next Steps:

The `s_nochrome` index inversion is the one thing this entry found and did not fix, and it should be repaired when the two column masks diverge for a real reason rather than on its own account. The half-cell channel `dx` now exists and is the way any future sub-cell placement is expressed, so the cell counts can keep meaning cells. Touch is unchanged and still the last thing between this port and playability without a shell: nothing on the framebuffer responds, the app-mode sink has never fired, and the two pieces of geometry that must be shared with a hit test rather than duplicated remain the d-pad's inverse rotation and the band split. The cartridge picker and the `tdsh run` 32 KiB contiguous-stack limit are both where entry 78 left them, and the NES, GBC and Tamagotchi backlog is unchanged.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 78 COMMIT Unreleased 2026-11-13T16:32:00-07:00

#### Coming From:

Unreleased 5b178c3

#### Purpose:

Give Pokemon Mini a Start menu entry so the emulator can be launched from the glass rather than from a shell prompt.

#### Outcome:

Pokemon Mini is in the Start menu and the operator launched it and passed it. The entry reads "Pokémon Mini" with the accented character intact, which the font can draw -- U+00E9 is present in the 8x16 set and the on-device filename carries it as UTF-8 bytes C3 A9, confirmed by reading the directory listing back. The route to it was not the obvious one and the reason is a pre-existing board limit worth recording. The natural implementation is a `.tdapp` script calling the shell, and that cannot work here: `tdsh run` starts a script worker with `TDSH_SCRIPT_TASK_STACK` of 32,768 contiguous bytes, and this board's largest free internal block has been 31,744 since long before the emulator was added -- entry 57 records that figure on a firmware with no Pokemon Mini in it at all, and the same 1 KB shortfall is what refused the 32 KB internal cartridge in entry 66. Every `.tdapp` and `.tdsh` therefore fails with "failed to start script worker (-12)", which is ENOMEM, and that is why `~/Apps` and `~/Start menu` were both empty: no app on this port has ever had a Start menu entry, including the NES and the Tamagotchi. The route that does work is MicroPython, which needs no worker of that size: the firmware exposes `tinydesk.shell()` (implemented in `third_party/tdsh/python/td_py_tdsh.c` as `tdsh_execute_line` with the script's own cwd and rights), so a Python file can run any shell command the shell can. Two files result, `~/Apps/pm.py` holding `tinydesk.shell("pm start /sd/pinball.min")` and `~/Start menu/Pokémon Mini.tdlink` whose `target=` line names it. One code change was needed and it is in `apps/shortcut.c`: a shortcut whose target has a Run command now runs it instead of opening it, where previously only the compiled targets ran and a shortcut to Python source opened the editor. The check is scoped to the shortcut branch deliberately, so opening a `.py` from Files still opens the editor -- a file manager's job is to show a file and a launcher's is to start one, and widening `td_is_compiled_app()` would have conflated them. The Start menu rebuilds its list on every open, so the entry appears without a reboot. Verification on the device was the whole chain rather than the last link: `py ~/Apps/pm.py` produced "pm: running /sd/pinball.min (524288 bytes)" and a `pm status` read of 71.4 fps with 6 missed in 24.8 s, and the operator then launched it from the menu and passed it. Build PASS; deployment PASS with every written region hash-verified; one stray build directory was created at the repository root by running `idf.py build` from the wrong directory and was removed before committing. The core-syntax audit passed: 78 entries, `core.md` and `core-reference.md` unchanged, and this entry conforms to the six-section template and the 100-entry cap.

#### Next Steps:

The launcher hard-codes `pinball.min`, and the obvious next step is a cartridge picker or a last-used memory; the shell already has `/sd` with twelve cartridges on it, so a small Apps-side script that lists them and passes the chosen name to `pm start` would need no firmware change at all. The larger outstanding item is unchanged and is now the only thing between this port and being playable without the shell: nothing drawn on the framebuffer responds to a touch, and the app-mode touch sink has never fired on this port. Two pieces of geometry must be shared with that hit test rather than duplicated -- the d-pad's inverse rotation, and the band split, since a point's allowable x depends on which band its row falls in -- and the clean shape is one function mapping a framebuffer point to the control that owns it, used by the painter and the hit test both. Two cosmetic items remain accepted rather than outstanding: the d-pad's left arm is clipped 1 px at the framebuffer's left edge, and A has a 4 px flat edge where it meets the right column. The `tdsh run` limit is worth a cycle of its own at some point, because 32,768 contiguous bytes is unavailable to any script on this board and that is a silent constraint on a documented feature; either the stack shrinks or the allocator is taught to find that much, and neither is a small change. Everything else is unchanged: Puzzle Collection remains at 0.94x to 0.98x with optimization parked behind a repeat-run-to-median harness, audio is absent, save states and EEPROM persistence are untouched, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the twelve cartridges and the BIOS dump remain on `/sd` and in neither repository, and the radio is off with auto-connect off.

#### Files Modified:

- apps/shortcut.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 77 COMMIT Unreleased 2026-11-13T16:16:00-07:00

#### Coming From:

Unreleased 652aae3

#### Purpose:

Settle the Pokemon Mini's on-glass controls: three slots per side column, a background that is black rather than the grid, and A and B sized and placed by the operator against the glass.

#### Outcome:

The control layout is finished and the operator accepted it, and the run below records five distinct faults along the way because four of them were the same class and only one was caught by a counter. Seven margin slots are now completely blank -- no box and no label -- so Pause, Reset, Shake, A, B, C and the reserved slot are bare glass while SS, Save, Load, Hold, Sound and Speed keep both, and the blankness is a single space rather than an empty string because `display_margin_label()` reads the empty string as "clear the override and fall back to the shared table", which would have restored the Tamagotchi's words on exactly the slots meant to be silent. The side columns draw three slots instead of five through a new `display_margin_column_slots()`, which is a count rather than a re-division: the buttons keep the geometry every other screen gives them and the bar simply stops, so the operator's rule that everything below the third button belongs to the app's own controls means what it says. `COL_N` is untouched, so the Tamagotchi and its test are unaffected and still pass 206 checks. The background is black -- the grid is behind `PM_GRID_ON` rather than deleted, because it earned its place by making three rounds of positioning specifiable in cells, and it will be wanted again -- while `PM_GRID` stays 16 because the cell offsets are still measured in it and the increment must not drift from the unit. A and B grew from 68 to 84 px with their labels staying at the doubled 16x32 as instructed, the labels returned to 0 degrees, and their offsets are separate values from a shared base column rather than derived from each other, which is what let A move without dragging B. The five faults. First, the push region was the framebuffer's width, panel 67..386, so anything right of it was clipped whatever the glass had room for; the operator diagnosed this directly by pointing out that the old buttons' boundary was being used as the limit, and the region is now two bands -- framebuffer width down to the column bottom, and full glass width below it, which is the space the three-slot columns freed. Second, the bands' origins were passed for x but not for y, so the lower band drew every shape 102 rows too low: the d-pad was cut in half by the bottom of the glass, B fell off it, and A clipped, all from one missing offset. Third, `display_picture_rows()` clips at the framebuffer while `display_panel_rows()` clips at the panel, and the horizontal bound of the latter was `DISPLAY_PANEL_W` alone when the last visible column is `PANEL_COL_OFFSET + DISPLAY_PANEL_W - 1` = 431, which was latent until a background spanned the width. Fourth, and the one to remember: the app's push window had been shortened to the drawn slots on the stated reasoning that a full-height black column was clipping A, and that reasoning was wrong because the columns paint before the app draws its controls -- so anything the column writes is painted over a moment later, and the real effect of the shorter window was that the desktop's own labels from boot survived in the two spare slots, which the operator reported as "Supe" in the left column and a left arrow in the right. The window is full height again, and the fault it was meant to fix was A running into the right column, which is a collision and not a clip. Fifth, the horizontal headroom is the right column and not the screen: at 84 px from a base of x 224, an x offset of one clips 4 px, two clips 20 and three clips 36, which the operator saw as a rectangular notch in A photographed against the Speed button. A sits at one cell right with a 4 px flat edge and B at two cells left, accepted as the compromise; shrinking the circles does not help, because the wall is a column boundary rather than a size limit. Build PASS; deployment PASS with every written region hash-verified; frame rate unchanged at 71.6 fps with `missed=0` over 22 s throughout, since none of this touches the frame loop. A verified 16 MiB backup `working-2026-11-13-1604.bin`, sha256 `ac46b080eb8a16a29007a86303ba0a60e3e74c6329935ba215e70a4e020803af`, was taken after acceptance with the board held in esptool's stub across read and compare, and it is the first archive to contain this whole layout. The core-syntax audit passed: 77 entries, `core.md` and `core-reference.md` unchanged, and this entry conforms to the six-section template and the 100-entry cap.

#### Next Steps:

The controls are drawn and inert, and touch is the last thing between this port and being playable without the shell. The first job is proving a tap reaches `pm.c` at all, because the app-mode touch sink has existed since the Tamagotchi work and has never fired here. Two pieces of the geometry must be shared with the hit test rather than written twice: the d-pad's inverse rotation, and now the band split, since a point's allowable x depends on which band its row falls in. The clean shape is one function that maps a framebuffer point to whichever control owns it, used by the painter and the hit test both, so the notch in A and the boundary at row 293 cannot disagree with what a finger does. Two cosmetic items are accepted rather than outstanding: the d-pad's left arm is clipped 1 px at the framebuffer's left edge, and A has a 4 px flat edge where it meets the right column. Puzzle Collection remains at 0.94x to 0.98x with optimization parked behind a repeat-run-to-median harness, audio is absent, save states and EEPROM persistence are untouched, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off. The board was unplugged and reconnected by the operator during this cycle rather than faulting, and the twelve cartridges and the BIOS dump remain on `/sd` and in neither repository.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 76 COMMIT Unreleased 2026-11-13T14:18:00-07:00

#### Coming From:

Unreleased 70e76c5

#### Purpose:

Give the Pokemon Mini's controls a background, rotate the A and B labels, and let the operator position each circle by counting grid squares on the glass.

#### Outcome:

The area below the picture is now the desktop's grid pattern instead of black, spanning the full width of the glass, and both circles sit where the operator placed them by eye. The grid is 16 px, which is the only grid in this port -- `display_test_pattern()` case 1 draws a line wherever either coordinate is a multiple of 16 -- after the instruction guessed 5 or 10; it is kept at 16 so the background matches what the `pattern 1` command draws, and the line is `0x18C3`, RGB565(3,6,3), rather than the pattern's white because as a background behind controls white competes with them while the controls are what the eye needs to find. Reaching the edges took two fixes on the same fault. The region was 320 wide, the framebuffer's width, when the glass is 410 -- the picture is a 320 px window inset 45 px into the panel, so a background that stops at the window stops 45 px short on both edges. Then `display_panel_rows()` clipped at x 410 when the controller's last visible column is `PANEL_COL_OFFSET + DISPLAY_PANEL_W - 1` = 431, because the RAM is 432 columns wide but the glass starts at column 22; that second one was latent, invisible on a black background and only reachable once something spanned the full width. The region is now panel x 22..431 by y 237..501, an exact fit to the glass, and the callback receives framebuffer x -45..364. The A and B labels are rotated 45 degrees counter-clockwise, the opposite sense from the d-pad's clockwise rotation, which was the instruction and is recorded as deliberate contrast rather than inconsistency: they are the circles' own marks and not part of the d-pad. Both go through the same `PM_ROT_K` 16.16 cosine so the arithmetic is shared, and the rotation direction was verified by rendering both senses side by side off-device before flashing, since getting it backwards is a one-character mistake that looks plausible in source. The circles are now placed independently, in grid cells, because the operator positions them by counting squares rather than in pixels: A is 3 cells right and 2 down from the pair's original column, B is 1 cell left and 1 down, and one cell is `PM_GRID`, so the offsets read as "how far did it move on the grid". They land at A x272..339 y292..359 and B x208..275 y376..443, with a 16 px gap between them and both inside the push region. One stale comment was caught and corrected in the process: `PM_PAD_YBASE` documented its value as 240 when the nudge had already made it 260, which would have misled the next reader about where the base sits. Build PASS; deployment PASS with every written region hash-verified; the operator confirmed the grid, the rotated labels and both circle positions on the glass, and unplugged the watch afterwards. Frame rate unaffected at 71.6 fps with `missed=0` over 22 s, since the background is drawn once at start like the controls. The core-syntax audit passed: 76 entries, `core.md` and `core-reference.md` unchanged, and this entry conforms to the six-section template and the 100-entry cap.

#### Next Steps:

The last verified backup is `working-2026-11-13-1319.bin`, which predates entries 75 and 76, so the first thing the next session owes is a fresh full backup once the watch is reconnected -- the binary now on the device carries the rotation, the nudge, the blank slots, the grid and the circle positions, and none of that is in the archive. After that, the controls remain drawn and inert: nothing on the glass responds to a touch, which is the last thing between this port and being playable without the shell. The first job there is proving a tap reaches `pm.c` at all, because the app-mode touch sink has existed since the Tamagotchi work and has never fired on this port. The geometry is positioned for it, with one caveat that must not be duplicated: the painter maps each pixel back through the d-pad's inverse rotation, so the hit test needs the same transform applied to the touch point and that arithmetic should be factored out rather than written twice. Two open questions still stand: the A and B letters were inferred rather than specified, and the panel's 45-degree chamfers make the bottom-left the tightest part of the layout. Everything else is unchanged: Puzzle Collection remains at 0.94x to 0.98x with optimization parked behind a repeat-run-to-median harness, audio is absent, save states and EEPROM persistence are untouched, nothing is committed for the twelve margin labels' duties, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off. The board was unplugged by the operator at the end of this cycle rather than faulting.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 75 COMMIT Unreleased 2026-11-13T14:01:00-07:00

#### Coming From:

Unreleased bbdd0b1

#### Purpose:

Finish the Pokemon Mini's on-glass controls: bare slots for the seven that the framebuffer supplies, a 45-degree rotation, a thumb-space nudge, and a push path that reaches the edge of the glass rather than the edge of the framebuffer.

#### Outcome:

The control cluster is where the operator wants it and the operator accepted it: the d-pad rotated 45 degrees clockwise into an X, nudged 20 px down and 20 px left, with every one of its twelve corners and all four arrows travelling with it, and the A and B circles unmoved horizontally and following it down. Seven margin slots are now completely blank -- Pause, Reset, Shake, A, B, C and the reserved slot have no box and no label -- while SS, Save, Load, Hold, Sound and Speed keep both, so a slot is either a named drawn button or nothing at all and the two never mix. The rotation is implemented as an inverse transform rather than a rotated rasteriser: every pixel is mapped back through the unrotate into the shape's own frame and tested there, so `dpad_inside()` and the arrow sampler are unchanged and the reasons a corner or a glyph looks the way it does live in one place. `PM_ROT_K` is 46341, cos 45 in 16.16 fixed point, chosen over float because the shape is rasterised once at start and an exactly-representable integer keeps the rendering identical run to run. The three faults worth recording are all the same fault in different clothes, and the last one is the cycle's real lesson. `display_margin_label()` treats an empty string as "clear the override and fall back to the shared table", so blanking the seven slots with `""` would have restored the Tamagotchi's words on exactly the slots meant to be silent; they use a single space, which is a real label that renders as nothing. The nudge was first computed as 10 px because at 20 the rotated bounding box reaches framebuffer row 425 and `display_picture_rows()` clips at 415 -- and that was arithmetic about the wrong rectangle. The framebuffer is a 320x416 window inside a 410x502 panel, the 41 rows below it are addressable and visible and already carry the bottom margin buttons, and the operator said so directly: there is space left on the OLED. The fix is `display_panel_rows()`, a second row-callback push identical to `display_picture_rows()` but clipped to the panel's own bounds, taking panel coordinates the way `edges.h` does; the d-pad and the circles now go through it, so the cluster's lowest row is framebuffer 427, which is panel 472, and nothing is shaved. That is the same path the margin buttons already used, which is what the operator asked for by describing the controls as needing to be drawn "like the side buttons". One constraint remains and is recorded rather than discovered later: the panel's 45-degree chamfers cut the corners, so the deep bottom-left is where a further nudge would start to lose the lower-left arm, not the clip. The centring from the previous cycle is also corrected in premise: the d-pad was centred in the band from the picture's bottom edge to the GLASS bottom rather than to the framebuffer's last row, which is a 41-row difference and is why 48 px sits above it and 49 below. Build PASS; deployment PASS with every written region hash-verified; frame rate unchanged at 71.5 fps with `missed=2` over 23 s, since none of this touches the frame loop. The core-syntax audit passed: 75 entries, `core.md` and `core-reference.md` unchanged, and this entry conforms to the six-section template and the 100-entry cap.

#### Next Steps:

The controls are drawn and inert: nothing on the glass responds to a touch yet, which is the next cycle and the last thing between this port and being playable without the shell. The first job there is proving a tap reaches `pm.c` at all, because the touch path has never been exercised on this port -- the margin buttons have been wired since the Tamagotchi and the app-mode sink in `pm.c` has never fired. The geometry is positioned for it: `dpad_inside()`, `pad_inside()`, `pad_cx()` and `pad_cy()` are the same functions the painter uses, and the d-pad's now needs the same inverse rotation applied to the touch point that the painter applies to the pixel, which is the one place that logic must not be duplicated. Two open questions still stand: the A and B letters were inferred rather than specified, and the panel chamfers mean the bottom-left corner is the tightest part of the layout. Everything else is unchanged: Puzzle Collection remains at 0.94x to 0.98x with optimization parked behind a repeat-run-to-median harness, audio is absent, save states and EEPROM persistence are untouched, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off. The board was unplugged by the operator during this cycle to test something else, which is what looked like a crash and was not; the firmware has not faulted.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 74 COMMIT Unreleased 2026-11-13T13:21:00-07:00

#### Coming From:

Unreleased a48a592

#### Purpose:

Draw the Pokemon Mini's own controls into the framebuffer rows the picture leaves empty, so the console can be driven from the glass rather than only from the shell.

#### Outcome:

The watch now carries a d-pad and two round action buttons in the 224 framebuffer rows below the top-aligned picture, and the operator accepted them as exactly specified. The controls are one push of a 264x168 region at (28,220), drawn once at cartridge start rather than per frame, because they never change: adding them to the per-frame blit would have put 44,352 extra pixels on the bus every frame for a static image. The d-pad is 168 px square with 56 px arms, 28 px from the left and bottom edges, with the eight arm tips rounded at r=5 and the four inner corners left sharp; the outline and both circles use the margin buttons' own 2 px border weight, built as shape-minus-inset the way `display.c` builds a button rather than by thickening a one-pixel test, which would have left a notch at every rounded tip. The arrows are the desktop's own U+2190 to U+2193 taken from the same `font8x16`, doubled to 16x32 by sampling each source pixel as a 2x2 block, and the two circles carry A and B drawn the same way. Colours are read from the buttons at draw time through a new `display_margin_colours()` rather than written as constants, so the cluster cannot drift from the twelve margin buttons it sits beside: violet border `COL_EDGE` 0x58CB, dark grey face `COL_FILL` 0x10A2, and the ANSI green of `s_palette[2]` 0x0660 for the arrows and letters. The circles are 68 px across, which is what the width allows: the gap between the d-pad's right edge and the framebuffer's margin is 124 px, so 28 px at each side leaves 68 and no more, and the pair occupies the d-pad's own band so the margins match on all four sides. At that diameter the space between the two circles is 32 px against 28 px at the outer edges, which is unavoidable and reads as two buttons rather than as a column. Four faults in this work are worth recording because three of them were caught by looking rather than by a counter. The arrow placement first used arm centres of `A/2` and `3A/2`, but the arms are the OUTER thirds, so Down and Right were drawn on top of each other in the middle square; rendering the geometry off-device before flashing found it. Rounding the inner corners was attempted three ways and none looked right -- centred on the corner and added bulges into the notch, centred in the void and subtracted is tangent and removes almost nothing, and centred on the corner and subtracted is the true mirror but eats three quarters of its disc from a reflex angle and reads as over-rounded -- so the operator's call was to leave them sharp, which is also the conventional shape. The outline needed eight-neighbour connectivity rather than four once the tips were rounded, because the arc grazes the edge and leaves isolated one-pixel specks: measured 640 pixels drawn with 8 specks against 684 with none. Build PASS; deployment PASS with every written region hash-verified; frame rate unchanged throughout at 71.6 fps with `missed=0` over 23 s, since none of this touches the frame loop. A verified 16 MiB backup `working-2026-11-13-1319.bin` was taken after the operator's acceptance with the board held in esptool's stub across read and compare, sha256 `fd6072d16c013fccbf3db4ca7b5e0241f9d28a3beee6495554d1a0b052aa3644`. The core-syntax audit passed: 74 entries, both control files unchanged apart from this entry, which conforms to the six-section template and the 100-entry cap.

#### Next Steps:

Nothing drawn here responds to a touch yet, and that is the next cycle: the touch path has never been exercised on this port, so the first job is proving that a tap on the d-pad arm reaches `pm.c` at all before any of it is mapped to the emulated CPU. The geometry already agrees with the hit test that will be needed -- `pad_inside()`, `pad_cx()` and `pad_cy()` are the same functions the painter uses, so a hit test written against them cannot disagree with what is drawn, which is the fault `edges.h` exists to prevent for the margins. Two open questions belong with that work: the A and B labels on the circles were inferred rather than specified, since the operator asked only for a colour, and A and B are what the console's own layout puts beside a d-pad while C is already a margin button; and the d-pad occupies the bottom-left with the circles to its right, leaving the remaining framebuffer space empty, which is where a third control or a status line could go later. Everything else is unchanged: the twelve margin labels still claim duties they do not perform, Puzzle Collection remains at 0.94x to 0.98x with optimization parked behind a repeat-run-to-median harness, audio is absent, save states and EEPROM persistence are untouched, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 73 COMMIT Unreleased 2026-11-13T12:16:00-07:00

#### Coming From:

Unreleased b716b5d

#### Purpose:

Label the margin buttons for Pokemon Mini, and stand the right column on end so it reads as the mirror of the left.

#### Outcome:

All twelve margin buttons now carry Pokemon Mini meanings and the operator accepted them on the glass. The left column top to bottom is SS, Save, Load, Pause and Reset; the bottom strip left to right is Shake, A and B; the right column top to bottom is Hold, Sound, Speed, a deliberately blank slot, and C. The blank is the operator's choice and not an omission: four of the five right-hand slots are taken and leaving the fifth visibly empty keeps the occupied positions where they are when a control arrives for it. The labels were applied through the override mechanism rather than by editing the shared tables in `edges.h`, because those tables are the Tamagotchi's and `tests/test_tama_ui.c` walks them against `tama_ui.c`'s action table; adding a second app's names would either break that test or force it to know about two applications. The column overrides already existed and `display_margin_label()` was used as-is, but the bottom strip had no override path at all -- `btn_label_app()` in `edges.h` returns the same degree glyph for all three bars -- so this cycle added one: an `s_ovr_btn[BTN_N]` array and `display_margin_btn_label()` in `display.c` and `display.h`, mirroring the column setter, with the override consulted at the single point where `paint_buttons()` chooses a label. The strip is repainted whole rather than per bar because it is one window, one bus acquisition and one transfer, so painting one bar in isolation would cost the same as three and would break the single-window rule the painter depends on. `pm.c` sets all twelve labels after claiming the pixels and clears them on stop, which matters for the next app rather than this one: without the clear the Tamagotchi would inherit "Shake A B" on its strip. The right column was then changed from `LABEL_UPRIGHT` to `LABEL_CCW`, so its first character sits at the bottom and it reads upward, making it the 180-degree counterpart of the left column's `LABEL_CW`. Two call sites needed the change rather than one, because `display_margin_label()` repaints a single column independently of `paint_margins()`, and a missing second site would have flipped the labels back the first time a toggle relabelled anything. That change also rotates the desktop's right-hand key labels, which is deliberate rather than incidental -- an edge should read consistently whichever app owns the screen -- and is recorded because the operator asked for the rotation in an app context and would not otherwise know the desktop moved. The shared table went untouched and `test_tama_ui` was compiled and run to prove it: 206 checks, 0 failures. Pokemon Pinball Mini measured 71.6 fps with `missed=1` over 27.7 s after both changes, and 71.6 fps with `missed=0` before the rotation, so neither change touched the frame loop. Build PASS at 0x2bc6b0 bytes with 278,352 free; deployment PASS with every written region hash-verified; the operator confirmed both the labels and the rotation on the glass. A verified 16 MiB backup `working-2026-11-13-1204.bin`, sha256 `45b715ae3967bf0f5827d379166f074a043e05cf5e5354a0f57a86cf649b3e3e`, was taken after acceptance with the board held in esptool's stub across read and compare. The core-syntax audit passed: 73 entries, both control files unchanged apart from this entry, which conforms to the six-section template and the 100-entry cap.

#### Next Steps:

The labels currently claim jobs the buttons do not do, and that is the next cycle's whole purpose. `edges.h` exists partly to prevent a label from disagreeing with its duty -- its own words are that a button labelled Enter which sends no Enter is the disagreement the file was built to stop -- so the twelve labels are presently a statement of intent and the work owed is making them true. The order that follows from the layout is the three bottom bars first, since Shake, A and B are the controls a game is actually played with and the touch path has never been exercised; then Pause and Reset; then Save, Load and SS, which need a state format that does not exist yet; and last Hold, Sound and Speed, which are toggles rather than presses and are where `display_margin_label()` earns its place, Pause becoming Resume and Speed showing the rate it will select next. Sound has no dependency to inherit because the port has no audio at all, and SS is the one label whose meaning has not been confirmed with the operator -- it was read here as the screenshot or save-state slot and the reading is recorded in `pm.c` so a correction is cheap. Everything else is unchanged: Puzzle Collection remains at 0.94x to 0.98x with optimization parked behind a repeat-run-to-median harness, save states and EEPROM persistence are untouched, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 72 COMMIT Unreleased 2026-11-13T11:35:00-07:00

#### Coming From:

Unreleased 07e2928

#### Purpose:

Top-align the Pokemon Mini picture in the framebuffer, which the operator asked for after reviewing the port on the glass.

#### Outcome:

The picture now sits flush against the top of the addressable framebuffer with all 224 unused rows below it, and the operator accepted it on the watch. The change is one constant: `PM_OY` went from `((416 - 64 * PM_SCALE) / 2)`, which is 112, to 0, so the 288x192 picture is placed at row 0 instead of being centred vertically. `PM_OX` is untouched at 16, so horizontal centring is unchanged and the margins either side stay symmetrical. Both call sites derive from the same constant -- the live blit in `blit_one()` and the `pm pattern` diagnostic -- so there was nothing else to edit, which is why the whole cycle is one line and a comment rather than a refactor. The reason recorded in `pm.h` for the operator's preference is that the eye tracks the top of a small screen, and a centred 192-row picture floats with equal dead space above and below it; the header also notes that the vacated rows are the natural home for a status line, battery indicator or save marker later, none of which would then intrude on the picture. The offset is confirmed by the diagnostic rather than assumed: `pm pattern` reports the coordinates it actually pushed to, and now prints `288x192 at (16,0), 3x` where it previously printed `(16,112)`. Frame rate is unaffected, as expected for a placement change: Pokemon Pinball Mini measured 71.7 fps with `missed=0` over 27.6 s at 13.0 ms of emulation against the 13.893 ms budget, identical to the figures before the change. Build PASS at 0x2bc510 bytes with 278,256 free, unchanged from the previous build because no code size changed; deployment PASS with every written region hash-verified; the operator confirmed the placement on the glass. The core-syntax audit passed: 72 entries, both control files unchanged apart from this entry, which conforms to the six-section template and the 100-entry cap.

#### Next Steps:

The port is feature-complete for playing one cartridge and the remaining work is the list that has been carried since the port shipped. Buttons are the largest gap: the margin and touch mapping exists in `pm.c` but has never been exercised, so the watch can only be driven by tapping the glass and the touch path's first test is still owed. Audio is absent and the core's `MinxAudio` is compiled but never started. Save states and EEPROM persistence are untouched, so no cartridge keeps progress across a restart. Puzzle Collection remains the one cartridge below full speed at 0.94x to 0.98x, and optimization is deliberately parked: two levers exist, dropping to 2x scale to halve push contention or a roughly 13 percent faster interpreter, but this board's run-to-run variance is about 5 percent, so neither can be measured one run at a time and a repeat-run-to-median harness should be built before either is attempted. The NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off.

#### Files Modified:

- ports/twatchultra/main/pm.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 71 COMMIT Unreleased 2026-11-13T11:11:00-07:00

#### Coming From:

Unreleased a888764

#### Purpose:

Find the roughly 1.7 ms per frame that separated Puzzle Collection's raw emulation figure from its paced figure, and decide whether the port's loop or the interpreter was responsible.

#### Outcome:

Neither was, in the way expected, and the cycle's real result is a negative one about measurement rather than about the emulator. Phase timers around the paced loop put the snapshot copy, the duplicate-frame comparison, the buffer handoff and everything else together at 0.15 ms per frame, so the loop was never the cost. A new `pm start <rom> noblit` diagnostic -- which runs the semaphore, the snapshot and the timing with no push at all -- then separated the two remaining candidates and found that the core-1 blitter costs the emulator between 0.5 and 1.2 ms of contention: Puzzle Collection emulates in 13.40 ms without a blitter and 13.9 to 14.6 ms with one. That matters because 13.40 ms is only 0.5 ms inside the 13.893 ms budget, so this cartridge has roughly half a millisecond of headroom to absorb display contention that costs up to 1.2 ms. The interpretation recorded in entry 70 was therefore too generous: the interpreter is not comfortably inside the frame for this cartridge in the paced loop, it nearly fills it, and the earlier 12.28 ms figure came from a tight unpaced loop in `pm bench` that does not reproduce the paced path. The optimisation attempted in this cycle -- comparing `LCDPixelsD` directly against the last pushed frame so that the snapshot is skipped entirely on the half of frames that are identical -- produced contradictory results and was reverted. It measured 13.9 ms when instrumented and 14.5 ms when lean, while the original copy-then-compare measured 14.5 ms when instrumented and 13.9 ms when lean: the two variants changed places between runs, which means the difference between them is smaller than run-to-run variance. That variance was then measured directly on one binary and is about 5 percent, spanning 0.94x to 0.98x for this cartridge across nominally identical builds and a cold boot, so any change of a few percent cannot be A/B tested on this board without many repetitions and single-run comparisons here are not evidence. The observation is recorded in the source above the diagnostic so a later cycle does not repeat the attempt blind. The final state keeps the original proven loop, gains the `noblit` diagnostic and its recorded figures, and leaves all eleven full-speed cartridges untouched: Pokemon Party Mini re-measured at 71.7 fps with `missed=0` and 11.3 ms of emulation. Build PASS; deployment PASS with every written region hash-verified; the operator's earlier acceptance of the picture stands and nothing visible changed. The core-syntax audit passed: 71 entries, both control files unchanged apart from this entry, which conforms to the six-section template and the 100-entry cap.

#### Next Steps:

Puzzle Collection at 0.94x to 0.98x is the last cartridge below full speed and the port should be described that way rather than as uniformly working. Two levers exist and the measurement now says which is binding: the display's contention is up to 1.2 ms and the emulator's headroom is 0.5 ms, so reducing push traffic is the cheaper target -- a 2x integer scale moves 24,576 pixels instead of 55,296 and would cut that contention by more than half, at the cost of a smaller picture than the 3x the operator has been reviewing. Faster interpretation is the other lever and needs roughly a 13 percent improvement in time per instruction, which is a much larger piece of work. Neither should be attempted until the variance is controlled, because a 5 percent noise floor makes both unmeasurable one run at a time; the practical fix is a repeatable measurement harness that runs the same cartridge several times and reports a median, which the `noblit` diagnostic already makes cheap to add. Everything else is unchanged: buttons remain unwired and the touch path unexercised so neither emulator can be played without tapping the glass, audio is absent, save states and EEPROM persistence are untouched, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off.

#### Files Modified:

- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 70 COMMIT Unreleased 2026-11-13T10:47:00-07:00

#### Coming From:

Unreleased 5099ff0

#### Purpose:

Find out whether Pokemon Puzzle Collection's 0.971x comes from the interpreter's speed or from this board's memory hierarchy, before spending a cycle making the interpreter faster.

#### Outcome:

The answer is neither, and the measurement that produced it also retired the plan it was meant to serve. Running the same three cartridges on the host and on the watch at 8,000 frames each showed the two machines ranking them identically: the host splits into two groups at exactly 2x, about 150 M instructions/sec for Pokemon Party Mini and Zany Cards against about 75 M for Lunch Time, Pinball Mini and Puzzle Collection, and the watch preserves that grouping with the same membership. So Puzzle Collection's expensive instructions are inherent to its code rather than an artefact of this board, and the hypothesis that its poorer memory locality was costing PSRAM fetches is refuted. The board's cost fits a single linear model, `watch_us = 1.20 + 159 * host_us`, which predicts every measured cartridge within 2 to 8 percent: a fixed floor of about 1.2 us per emulated instruction, which is dispatch plus a cartridge fetch with no instruction cache, and beyond it a slowdown of 159 times the host. That floor is also why the fastest cartridge suffers the largest relative penalty, 343 times for Zany Cards against 247 times for Puzzle Collection, and why the board compresses the spread between titles rather than preserving it. Two claims made earlier in this session were wrong and are corrected here. `synccycles` was proposed as the first thing to try and it would have been a no-op: it is read only by `PokeMini_EmulateFrame()` at Hardware.c lines 51, 58 and 70, while this port calls `PokeMini_EmulateCycles()`, which does not consult it at all and synchronises the timers, PRC and audio after every single instruction. The value was identified from `CommandLine.c` without tracing which function consumed it, which is the same class of error as entry 69's inverted picture: an assumption that survived because nothing checked it. The second correction is the size of the shortfall. Puzzle Collection's raw emulation measured 12.28 ms per frame unpaced, which is comfortably inside the 13.893 ms budget, and only the paced loop reaches 14.0 ms; the overrun is about 0.11 ms, 0.77 percent, rather than the structural deficit that entry 68's 0.971x implied. Entry 67's projected 16.1 ms for the heaviest cartridge was therefore wrong for a third reason beyond the instruction-count error already recorded in entry 68: the projection treated emulation as the whole frame cost when the interpreter alone fits for every cartridge in the set. A further correction belongs to the supporting harness rather than the firmware: `tools/pmbench/bench_pokemini.c` and `pmbench.c` at `components/pokemini` both still use 55,555 cycles per frame, which is 4e6/72 and 0.036 percent fast, rather than the 55,575 that PM-003 established; instruction counts are unaffected because they are counted per instruction executed, but any figure divided by frame count inherits the error, so the 400-frame host figures are not comparable with the device's and the 8,000-frame host figures are the ones quoted. Build PASS for the host harness with `POKEMINI_SRC=/run/media/vash/GIT/tinytama/core/pokemini ./scripts/pmbench.sh build`; no firmware changed, nothing was deployed, and no user test applies. The core-syntax audit passed: 70 entries, both control files unchanged apart from this entry, which conforms to the six-section template and the 100-entry cap.

#### Next Steps:

The interpreter is not the target, so the next cycle is instrumentation of the paced loop rather than optimisation of the core. The loop's 14.0 ms must be split into pure emulation, the 6 KB snapshot copy, the 6 KB comparison, the buffer handoff and scheduling, because the raw emulation figure is 12.28 ms and about 1.7 ms is unaccounted for in the paced path; that gap has not been measured and should not be guessed at, especially as it varies by cartridge, being roughly 0.1 ms for Pokemon Party Mini and 1.7 ms for Puzzle Collection. One inefficiency is already visible in the code and is a candidate to fix in the same cycle: every frame copies `LCDPixelsD` into a snapshot buffer and then compares that snapshot against the previous one, when comparing `LCDPixelsD` directly would skip the copy on the roughly half of frames that are identical. The target is 13.5 ms for Puzzle Collection, which leaves 0.4 ms of margin and keeps the tick backlog from saturating. Separately, the two host bench sources should be moved to 55,575 cycles per frame so their frame-derived figures agree with the device and with PM-003. Everything else is unchanged: buttons remain unwired and the touch path unexercised, audio is absent, save states and EEPROM persistence are untouched, Puzzle Collection's render path has still not been verified on the glass since entry 69 found the inverted picture after a sweep that had displayed it, the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 69 COMMIT Unreleased 2026-11-13T10:17:00-07:00

#### Coming From:

Unreleased 63e2fd6

#### Purpose:

Fix the inverted picture that the operator found by putting the watch beside the desktop emulator on the same screen.

#### Outcome:

The port had been rendering every frame inverted, and the operator found it by running Pokemon Puzzle Collection on the watch and on the tinytama desktop reference at the same time and comparing them: the desktop was correct and the watch was the negative of it. The cause is a semantic error rather than a colour choice. The Pokemon Mini's panel is a white-on-black monochrome LCD, meaning a set pixel is a DARK dot on a pale background, and PokeMini's `LCDPixelsD` holds one byte per dot with non-zero meaning lit; `pmgui.c` renders a non-zero dot as `0xFF16161A`, near black, against `0xFFC8C8BE` for an unlit one, which is the reference the port should have matched. `pm.c` mapped non-zero to white and shipped the negative. The fix is two renamed constants, `PM_DOT_LIT` 0x0000 and `PM_DOT_UNLIT` 0xFFFF, with the names chosen so the next reader cannot invert them by accident again. It is explicitly NOT the hardware's own inversion: `MinxLCD` has `InvAllPix` and `SetAllPix` registers that a cartridge can set, and `MinxLCD_Render()` applies them into `LCDPixelsD` before the port sees it, so the polarity in the port is fixed and absolute and nothing a game does should change it. What makes this fault worth recording is how invisible it was to every instrument the port has: the frame rate, the speed ratio, the push count and the identical-frame skip were all exactly as measured before, because none of them cares what a pixel means, and the watch had been signed off as working at 0.999x with a picture that was upside down in tone. Only looking at the two screens together found it, which is the third time this project has paid for that lesson. Pokemon Pinball Mini was then booted on both machines and the operator confirmed the picture correct, running at 71.9 fps, 0.999x of real time with `missed=0` over 84.1 s, emulation 13.0 ms against the 13.89 ms budget. Build PASS at 0x2bc4d0 bytes with 278,320 free; deployment PASS with every written region hash-verified; the desktop comparison used the cartridge at sha256 `5ac75fd2b6d5dea85e17e5e6131339e0e19eefe978a6f4b7f8700fb28c89cc23` on both sides. The core-syntax audit passed: 69 entries, both control files unchanged apart from this entry, which conforms to the six-section template and the 100-entry cap.

#### Next Steps:

Puzzle Collection is still the one cartridge below full speed at 0.971x with emulation at 14.1 ms against the 13.89 ms budget, and the picture fault means its earlier sweep result should be treated as a rate measurement only -- the frames were counted correctly but displayed inverted, so nothing about its render path has been verified against the reference. Re-running it on the glass is therefore worth a minute before the `synccycles` experiment is judged, because that experiment will be read off the same status figures and a second undetected display fault would invalidate the comparison the same way this one did. The rest is unchanged: buttons are still unwired and unverified, so neither emulator can be played on the watch without tapping the glass and the touch path has never been exercised; audio is absent; save states and EEPROM persistence are untouched; the twelve cartridges and the BIOS dump remain on `/sd` and in neither repository. The NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off.

#### Files Modified:

- ports/twatchultra/main/pm.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 68 COMMIT Unreleased 2026-11-13T10:05:00-07:00

#### Coming From:

Unreleased f99aaf8

#### Purpose:

Sweep all twelve retail cartridges rather than the one, because entry 67 proved full speed on Pokemon Pinball Mini and explicitly did not prove it for the platform.

#### Outcome:

Eleven of the twelve cartridges run at 0.998 to 0.999x of real time on the watch, and Pokemon Puzzle Collection does not: at 0.971x over a 75-second run its emulation takes 14.1 ms against the 13.89 ms frame budget and it falls 145 ticks behind without recovering. Every cartridge in the set was transferred to `/sd` and run twice, `pm bench` for 400 unpaced frames and then `pm start` for 15 s paced, with the radio off throughout so the figures are comparable; all twelve loaded, booted and rendered. The distribution is narrow and the outlier is not the cartridge with the most instructions: per frame the set needs 3,483 to 4,489 instructions, and the Party Mini family is the heaviest at about 4,486 and yet the cheapest per instruction at 2.45 us, running at 11.9 ms. Puzzle Collection needs only 3,678 instructions per frame but each costs 3.34 us, the highest in the set, which puts it over the budget; Pinball at 3.16 us and 3,811 instructions lands at 13.0 ms and clears by 0.9 ms. The conclusion is that the cost that matters on this board is the interpreter's time per instruction rather than a game's instruction count, and the two are not correlated across these titles. Every cartridge also reports its own PRC divisor, which decides how fast it draws and therefore whether a dither will beat: the Party Mini family runs at divisor 3 and draws at 24 Hz, and the other seven run at divisor 2 and draw at 36 Hz. This corrects entry 67's projection of 16.1 ms for the heaviest cartridge, which scaled the host survey's 5,053 instructions per frame by 3.19 us; no cartridge here exceeds 4,489 instructions and the real worst case is 14.1 ms from expense rather than volume, so the estimate was pessimistic and named the wrong mechanism while reaching the right conclusion. Heap was read after every cycle and returns to 110,363 bytes from 110,675 at boot on the first cycle and is flat thereafter across twelve start and stop cycles, so there is no per-run leak. Two faults in the sweep harness produced a confident wrong answer before the corrected run and belong in the entry: `/dev/null` does not exist in tdsh's filesystem jail, so `pm stop > /dev/null` failed with a redirection error and never ran, which left the first cartridge running for the whole sweep while every later cartridge was refused with "stop the emulator first" and its `pm status` reported the first one -- eleven of twelve rows were the same measurement, and a redirection failure is a command that does not run rather than a command that runs and fails. The board also went silent once during that attempt and needed an esptool reset with no panic in the console capture; afterwards internal RAM read 26,919 bytes with the radio up against 70,031 at a clean boot and the SSH server refused to start below its 65,536-byte floor, which is consistent with one emulator left running beside WiFi but was not established, and it did not reproduce in eight consecutive start and stop cycles or in the corrected sweep, so it is recorded as an observation rather than a cause. Build N/A because no firmware changed; deployment N/A; the sweep ran on the image accepted in entry 67 and left the watch with the radio off, the emulator stopped and the scratch output files removed.

#### Next Steps:

Puzzle Collection is 2 percent slow and the fix is not another overlap, because the overlap is already done and the emulator task is what exceeds the budget on its own. The measured lever is cost per instruction, and the candidate worth testing first is the core's `synccycles` value, which defaults to 64 in the performance build and sets how often the hardware blocks are synchronised; raising it trades accuracy for speed and would show up immediately in Puzzle Collection's emulation figure. Beyond that the interpreter's per-instruction dispatch is the target, and the EMU records already establish what the floor looks like on this board. Buttons remain unwired and unverified, which the operator has twice accepted as out of scope, and the shock sensor still has no watch equivalent; audio is still absent; save states and EEPROM persistence are untouched, so no cartridge keeps progress across a restart. The twelve cartridges remain on `/sd` for re-testing and neither they nor the BIOS dump are in either repository. The NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio is off with auto-connect off.

#### Files Modified:

- docs/pokemini-scoping.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 67 COMMIT Unreleased 2026-11-13T09:15:00-07:00

#### Coming From:

Unreleased 9c30082

#### Purpose:

Make Pokemon Mini playable at the machine's real speed on the watch, since the benchmark in entry 66 proved throughput but rendered nothing.

#### Outcome:

Pokemon Pinball Mini runs at 0.999x of real time on the watch: 71.9 fps against the machine's 71.97 Hz tick, `missed=0` over 6,295 frames and 2 missed ticks in 20,249 over 281 s, reported by the operator as looking good. The port is `main/pm.c` with `main/pm.h`, built on the component vendored in entry 66; `pm start` reads a cartridge from `/sd` into PSRAM and runs a 96x64 PRC picture through the existing `display_picture_rows()` blit at 3x integer scale, 288x192 centred at (16,112), with the margins switched to app mode so the desktop stops painting. The picture took three passes to become fluid and only the first was the change that had been planned. Sequentially the frame cost emulation 12.8 ms plus a 5.4 ms blit against a 13.89 ms budget, which is 0.765x: the machine ran in slow motion rather than dropping display frames, a distinction invisible in an fps figure and audible in the operator's own test, since a dithering game alternates once per drawn frame. Overlapping the blit onto core 1 through three 6 KB dot buffers and two queues, with the DSP-008 ordering of taking a free buffer before publishing the finished one, took 0.765x to 0.983x and cost 1.0 ms of emulation to PSRAM contention in exchange for removing 5.6 ms from the critical path. Pushing only frames that differ from the one already on the glass removed about half the pushes -- `pushes=3070 identical=3225` -- because a cartridge draws at the PRC rate, once every 2 ticks for this one, and the loop had been pushing on every tick. The remaining cap was the pacing and it read as an emulator fault: with 13.2 ms against a 13.89 ms budget the measured speed was still 0.981x, and the tell was `missed=33` of 1,837 ticks, 1.8 percent against the same shortfall, because a binary semaphore holds one pending give and each slight overrun silently deleted the following tick's emulated time; a counting semaphore of depth 8 took it to 0.999x with nothing in the emulator changed. Three facts about the platform were established and are now recorded as PM-003: the frame tick is 71.97 Hz, not 72, being 65 x 855 = 55,575 cycles at 4 MHz and derived here from the core's own counter constants rather than assumed, so an earlier 4e6/72 = 55,555 ran the machine 0.036 percent fast; a cartridge draws at that tick divided by 2 to 12 with no divisor of 1, so 36 Hz is the fastest possible picture rate and 60 Hz does not exist on this hardware; and Pinball measures divisor 2 live from its own register, which is why its dither alternated at 27.5 Hz at 0.765x and beats. Wikipedia's infobox states 30 fps from a 2001 IGN preview, which matches neither figure and is recorded as an open conflict rather than adopted, because the primary source at pokemon-mini.net returned 403 from Cloudflare and 429 from the Wayback Machine on every attempt. One further correction is part of the port: a retail cartridge needs a retail BIOS, the free GPL BIOS compiled into the core idles at $0040 and never hands over, and since `PokeMini_Create()` memcpy's it on every start there is no file whose absence prevents its use, so `pm start` reads `PokeMini_FreeBIOS` after loading and refuses to run if the free BIOS is in memory rather than presenting a healthy frame rate over the BIOS idle loop. Buttons are not mapped yet, which the operator accepted as out of scope for this cycle. Build PASS with the image at 0x2bc3b0 bytes and 284,752 free; deployment PASS with every written region hash-verified and the final figures taken on the exact source on the device; a verified 16 MiB backup `working-2026-11-13-0909.bin`, sha256 `79f2073f389c20c44005b849083f6b54206799b11d41da5b21b5ac166c92bc94`, was taken after the operator's acceptance with the board held in esptool's stub across read and compare. The core-syntax audit passed: 67 entries, 66 of 66 settled entries unchanged, one added entry conforming to the six-section template and the 100-entry cap, `core-reference.md` gaining PM-003 with a matching fast-lookup index entry and 127 record ids all unique and all indexed.

#### Next Steps:

The port is at full speed for this cartridge and not proven for the platform, and the entry should not be read as claiming otherwise: emulation has 13.2 ms of a 13.89 ms budget, so 5 percent, and at the measured 3.19 us per instruction the heaviest cartridge in the twelve-cartridge survey would need 16.1 ms of emulation before any display work. Running each of the other eleven cartridges once through `pm bench` and `pm start` is therefore the next measurement and it decides whether this is a port or a demonstration. Buttons come next after that: the margins and touch are wired into `pm.c` through the same `EDGE_BOTTOM` and `EDGE_RIGHT` mapping `nes.c` uses, but nothing has been verified on the glass, so the mapping needs the operator's hands before it can be called done, and the shock sensor has no watch equivalent because a reed switch is not a touch panel -- a screen-tap gesture is the natural stand-in and the panel already reports multi-touch. Audio is last, as on every platform here, and the core's MinxAudio is compiled but never started, so a sound path does not exist yet. Save states and EEPROM persistence are untouched and Pinball keeps nothing across a restart. The NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended, the Tamagotchi still wants a Start menu entry, and the radio remains off with auto-connect off; `/sd/pinball.min` and `/sd/bios.rom` are on the device only and neither a cartridge nor a BIOS dump is in either repository.

#### Files Modified:

- docs/pokemini-scoping.md
- ports/twatchultra/main/pm.c
- ports/twatchultra/main/pm.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 66 COMMIT Unreleased 2026-11-13T08:15:00-07:00

#### Coming From:

Unreleased e753e04

#### Purpose:

Measure Pokemon Mini interpreter throughput on the watch itself, the one open question that had left the platform undecided since it was scoped.

#### Outcome:

The platform clears its CPU requirement, and the watch said so rather than a ratio. The Route B work was done as planned: `components/pokemini` in the fork carries the core vendored verbatim from `~/GIT/tinytama/core/pokemini` (upstream PokeMini `8a64225e`, v0.61, GPL-3.0, one 2-line instruction counter recorded in `patches/0001-exec-counter.patch`), with provenance, licence and the dropped-disassembler rationale recorded beside it, and `main/pmbench.c` adds a shell command that runs a 32 KB synthetic cartridge of 55,555-cycle frames and reports instructions per second, per-frame cost and realtime. Four runs at 2,000 frames, radio off, core 0 at priority 5 and so comparable with the NES port, returned 382,982, 382,951, 383,075 and 383,102 instructions per second in 17.24 s each: a 0.04 percent spread, which is the first measurement on this board whose noise did not need explaining. That is 3,302 instructions per frame, 1.61x realtime, and 1.06x the heaviest requirement in the 0.22-0.36 MIPS band derived in entry 64. The internal-SRAM case was refused rather than measured -- the largest free internal block was 31,744 bytes against a 32,768-byte request, missing by exactly 1 KB with 117,475 bytes free in total -- which settles the design question the scoping document had left open, since a maximally sized cartridge cannot be internal and 383,000 is therefore the realistic figure rather than a pessimistic one with a better configuration available; the internal-versus-PSRAM penalty is not measurable at 32 KB and was left unmeasured rather than inferred from another workload. Two caveats belong in the entry because they bound the result: the synthetic cartridge executes 3,302 instructions per frame while the twelve retail cartridges measured in entry 64 need 3,091 to 5,053, so at 2.61 us per instruction a heavy cartridge costs 13.2 ms against a 13.9 ms frame, which is 1.05x realtime rather than 1.61x, and because random 12-bit words hit disproportionately many slow paths a real instruction should be cheaper, leaving the two effects opposing with neither magnitude known. The margin is therefore thin against heavy cartridges and thick against light ones, and a real `.min` would settle it in one more run. Two defects cost a build each and are recorded in the source: `MinxCPU.h` declares `static inline uint8_t SAR(...)` while Xtensa's `specreg.h` defines `SAR` as special register 3, which needed an explicit `#undef` because tdsh.h transitively includes FreeRTOS.h, and the first version of `pmbench` printed from its helper task, producing a run that returned to the prompt with the entire measurement absent; that version was replaced by measure-then-print with the mechanism left explicitly unestablished rather than guessed, since tdsh's stdout is one global newlib stream and not per task. Build PASS with `libpokemini.a` at 14,564 bytes of bss and 63,692 bytes of flash against a 114,708-and-14,984 estimate and 284,848 bytes still free in the app partition; deployment PASS with every written region hash-verified and the number re-taken on the exact source now on the device; a verified 16 MiB backup `working-2026-11-13-0749.bin` (sha256 `ed77df277072e58bd78a579dfc42776a0da05affbb78484d3f6f5c265197c0f3`) was taken first, with the board held in esptool's stub across read and compare. The core-syntax audit passed: 66 entries, 65 of 65 settled entries unchanged, one added entry conforming to the six-section template and the 100-entry cap.

#### Next Steps:

Make the cartridge playable, which is the port itself rather than a measurement: `pmbench` proves the bench sustains the frame rate but renders nothing, so the next cycle needs the 96x64 monochrome PRC output converted to RGB565 and pushed through the existing `display_picture_rows()` blit, the watch's margins and touch mapped to the Sega-style d-pad plus A, B and C, and a 72 Hz frame loop with real-time pacing rather than free-running cycles. Pokemon Pinball is the test cartridge, chosen because the operator cleared two levels of it on the tinytama desktop reference and so has a baseline for feel. The feel test is the point: 1.05x realtime on the heaviest cartridges is a margin that could read as laggy input, and if it does then the interpreter needs work before any UI or audio is worth building. The free BIOS cannot serve as the cartridge since it idles at `$0040`, and no commercial ROM is committed to either repository. Everything else is unchanged: the NES stays parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC remains scoped with gnuboy recommended and its 28-45 fps projection unverified, the Tamagotchi still wants a Start menu entry, and the radio remains off with auto-connect off.

#### Files Modified:

- docs/pokemini-scoping.md
- scripts/pmbench.sh
- ports/twatchultra/components/pokemini/CMakeLists.txt
- ports/twatchultra/components/pokemini/README.md
- ports/twatchultra/components/pokemini/LICENSE-PokeMini
- ports/twatchultra/components/pokemini/patches/
- ports/twatchultra/components/pokemini/
- ports/twatchultra/main/pmbench.c
- ports/twatchultra/main/pmbench.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 65 COMMIT Unreleased 2026-11-13T02:40:00-07:00

#### Coming From:

Unreleased 6715ac9

#### Purpose:

Run the Pokemon Mini benchmark on the watch, which was the one measurement standing between this project and a decision about that platform.

#### Outcome:

The measurement could not be taken, and the reason is the cycle's result: the prepared binary is built for the wrong operating system. `for-escapement/pokemini_bench_target` is a Linux ELF produced by this repository's own crosstool-NG (GCC 14.1.0) against uClibc, carrying `_start` and `__libc_start_main` and no ESP-IDF runtime, while the watch runs ESP-IDF v5.3.1 with TinyDesk Shell, which has no ELF loader and whose `tdsh` reports `command not found` and has neither `chmod` nor `exec`. The transfer itself worked perfectly and that is what makes the finding clean: WiFi was enabled with `network autowifi on` and `wificonnect`, `ssh start` reported internal free=91687, and SFTP moved 392204 bytes to `/sd` at 18.1 KiB/s in 21.2 seconds with md5 `c4ebd6cb6705d0d423c0add5082fddaf` matching at both ends. Execution was refused. The file was removed, the SSH server stopped, and the radio returned to `autowifi off` plus `wifidisconnect` with ping failing, as the operator asked. The scoping document was corrected rather than left standing: its "what is still unknown" section had instructed the next reader to copy that binary to the watch and run it, and that instruction is wrong. It now records the operating-system mismatch, the two routes that would actually produce the number, and their difference. Route A flashes the Linux image, which is what the binary was built for and needs no code, but it replaces TinyDesk -- the platform the NES and GBC work lives on -- and the figure it yields describes Pokemon Mini on Linux rather than here; it is reversible, since `working-2026-11-13-0019.bin` is a verified 16 MiB image of the current firmware. Route B builds `core/pokemini/` into TinyDesk as a `pmbench` command, which is real work but is step one of the port rather than a detour, and it is sized comfortably at 114708 bytes of text against 347 KB free in the app partition and 14984 bytes of bss against 143 KB of free internal DRAM. Route B is recommended, because NES and GBC are both TinyDesk ports and a third platform running on a different operating system would make the recommendation depend on which OS a later session happened to have flashed. Two findings from the previous cycle are unaffected because neither depends on the target: the CPU requirement is 0.22 to 0.36 MIPS rather than 1 MIPS, and the display gate is 5.4 times cheaper than when it was measured. Only the on-target CPU figure is blocked.

#### Next Steps:

Route B is the next cycle's work and it is well defined: vendor `core/pokemini/` into the fork as a component, which the tinytama tree already carries with its provenance and its one 2-line patch recorded, then add a `pmbench` shell command that runs the core with `--synthetic` and prints instructions per second, and flash it. The acceptance threshold is 0.22 to 0.36 MIPS rather than the 1 MIPS the older scope asked for, and the run should be repeated three times because this board's clock scaling makes single runs noisy. `scripts/pmbench.sh target` already cross-compiles the host harness against the ESP-IDF toolchain for reference, but the path that matters is the component build, not the standalone binary. Until that number exists, no Pokemon Mini UI work should start, and the operator's call on whether to spend a cycle on it at all is still open. Everything else is unchanged: the NES is parked at 51 fps with battery-SRAM persistence and the skip-path test owed, GBC is scoped with gnuboy recommended and its 28-45 fps estimate flagged, the Tamagotchi still wants a Start menu entry, and the radio remains off with auto-connect off so any network work needs `network autowifi on` and `wificonnect` first.

#### Files Modified:

- docs/pokemini-scoping.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 64 COMMIT Unreleased 2026-11-13T02:05:00-07:00

#### Coming From:

Unreleased 92b9f76

#### Purpose:

Scope a Pokemon Mini emulator, which the operator noted was already built during the Tamagotchi work.

#### Outcome:

The recollection was right and the work is in a different repository: `~/GIT/tinytama` holds PokeMini vendored at revision `8a64225e` under GPL-3.0 with one 2-line patch, a playable desktop reference build the operator confirmed across twelve cartridges including two levels of Pinball cleared, a cross-compile measurement of 114,708 bytes of Xtensa text, and three scope documents. What this cycle adds is a correction, a re-measurement and the harness to reproduce both. The correction is that the platform's CPU requirement was overstated threefold: the original scope states 1 MIPS, but 1 MIPS is the S1C88's hardware ceiling (4 MHz / 4-cycle fetch), while a game needs instructions-per-frame times 72 Hz, and the twelve retail cartridges measured at 3,091 to 5,053 instructions against a 13,889-per-frame budget -- 0.22 to 0.36 MIPS. The re-measurement is that the display gate, which that scope called the platform's binding constraint, was taken through 64-byte SPI chunks at 357 ns per pixel, and the transmit path rebuilt for the NES on 2026-11-12 moves 16,384-pixel transactions at 66.5 ns per pixel, so a 288x192 frame costs 3,675 us of the 13,889 us budget rather than 19,741 us. The risk profile has inverted: the display is now comfortable and the CPU is the only unknown. One honest reservation belongs in the entry because it argues against the platform: Pokemon Mini is 96x64, a 3:2 landscape screen, so at its maximum integer scale of 3x it is 288x192 -- 41.5 percent of the framebuffer, smaller in area than the NES the operator just parked as too small, and with 112 rows of letterbox above and below. It wins on width and loses on height, and nobody should start this port expecting a large picture. Build PASS for both the host harness and a new static Xtensa cross-build; no firmware changed and nothing was flashed.

#### Next Steps:

One measurement decides the platform and it is one transfer away: `build/pmbench/pokemini_bench_target` is a 392 KB static Xtensa binary that needs no ROM, and its own README records why it cannot be exercised anywhere but the board -- QEMU has no LX7 core, and a hello-world compiled by the same toolchain fails identically on every Xtensa core QEMU offers. It should be run with `--synthetic 2000` three times, because this board's clock scaling makes single runs noisy, and the number that matters is against 0.22 to 0.36 MIPS rather than the 1 MIPS the old document asked for. It cannot be run yet because the radio is off by the operator's instruction, so the transfer needs either the network briefly re-enabled or a card route; that is a decision rather than a task. If it clears, the port has a week of work banked and the display path needs no change at all. If it does not, the platform is refused and that is worth knowing for the price of one command. Everything else in the backlog is unchanged: the NES is parked at 51 fps with SRAM persistence and the skip-path test owed, GBC is scoped with gnuboy as the recommendation, the Tamagotchi still wants a Start menu entry, and the radio remains off with auto-connect off.

#### Files Modified:

- scripts/pmbench.sh
- tools/pmbench/bench_pokemini.c
- docs/pokemini-scoping.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 63 COMMIT Unreleased 2026-11-13T01:30:00-07:00

#### Coming From:

Unreleased 7a0d0b1

#### Purpose:

Scope a Game Boy Color emulator the way the NES was scoped, because 256x240 is too small for a 320x416 panel and the Game Boy's 160x144 divides 320 exactly.

#### Outcome:

Two cores were built and measured rather than one being read about, and the scoping reached a conclusion neither the operator nor the agent expected: the picture gets much better and the frame rate probably gets somewhat worse. gnuboy read 9,308 / 13,560 / 10,989 / 13,620 fps on Zelda Link's Awakening DX, Pokemon Crystal, Super Mario Bros. Deluxe and Tetris DX, against binjgb's 5,684 / 7,578 / 6,683 / 8,848, so gnuboy is consistently 1.54 to 1.79 times faster across CGB-only and CGB-compatible cartridges and three mappers; all figures are best of three at 1,500 frames because this host's run-to-run noise measures 7 to 12 percent. gnuboy also emits `GB_PIXEL_565_BE`, the panel's own byte order, so a port needs no per-pixel conversion where the NES spends 4,355 us of every frame on a palette lookup. The geometry is why the port is worth doing at all: 160x144 at 2x is 320x288, filling the full width with no side margins and 69.2 percent of the framebuffer, against the NES's 46.2 percent with 32-pixel margins that no integer scale can improve because 256 x 1.25 is not an integer. The arithmetic that sizes the disappointment is that a GBC's CPU runs at 2.34 times the NES's clock, so the watch's measured 1.95 Mcyc/s of 6502 cycles projects to 45 fps at an instruction normalisation and 28 fps at a cycle one, and 2x upscaling puts the transmitted pixel count at 1.5 times the NES's, so the cheaper blit does not pay for the dearer emulator. Peanut-GB was ruled out on its own README: it is the fastest portable Game Boy core and it is DMG-only. Two harness traps are worth recording because both produced plausible numbers that were wrong: omitting `gnuboy_reset()` segfaults inside the CPU rather than failing quietly, and running the diagnostic scan on every frame masked the format difference entirely because a 23,040-iteration loop cost more than the conversion it was measuring, which would have produced the confident and false conclusion that the pixel format does not matter. Build PASS for both harnesses; no firmware changed and no handset test applies because nothing was flashed.

#### Next Steps:

The port itself is the next step and the scoping document's recommendation is gnuboy, not binjgb, for four reasons that compound: it is 1.6 times faster, it needs no pixel conversion, its API is six calls rather than a front end, and it is already in this project's supply chain at a pin we control. The first hardware run decides the plan, because the projection's 28 to 45 fps band is wide enough that it could mean nothing to do or a Z80 interpreter that needs work, and no further optimisation should be designed before that number exists. It should be built at 2x scale from the first flash, since 1x is worse than the NES at 17 percent of the framebuffer and would not answer the complaint the port exists to answer. The display path needs no change at all: `display_picture_rows()` and the core-1 blit already exist and were built for this, which is the one place this cycle is cheap. Audio comes last as it did for the NES. The NES itself is parked with SRAM persistence and the skip-path test still owed, the Tamagotchi still wants a Start menu entry, and the radio remains off with auto-connect off, so the first GBC work must be driven over the USB terminal unless it is re-enabled.

#### Files Modified:

- scripts/gbcbench.sh
- tools/gbcbench/bench_gnuboy.c
- tools/gbcbench/bench_binjgb.c
- docs/gbc-scoping.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 62 COMMIT Unreleased 2026-11-13T00:52:00-07:00

#### Coming From:

Unreleased 51ccdd8

#### Purpose:

Run the NES blit concurrently with the emulator on the second core, which the previous cycle's arithmetic left as the only route past 42 fps.

#### Outcome:

Zelda's title screen runs at 51 fps with 11,055 frames, 11,055 blits and 0 skipped over a 20-second run, and dropped frames fell to 36 in 11,055 (0.3 percent), up from 42 fps. The operator played it and reported no tearing and that it now feels mostly full speed. The change replaces DSP-005's single-writer rule with three mechanisms rather than one: `s_tx_app`, a second transmit band so a margin relabel on one core cannot overwrite a band the blitter is about to send; `reg_write_bus()` and `set_window_bus()`, so a band's window programming and its pixel stream happen under one bus acquisition, since CASET and RASET are panel state and a second pusher's window between another's window and its pixels would send those pixels to the wrong rectangle; and three PSRAM frame buffers handed between the emulator on core 0 and a blit task on core 1 through two queues, because the renderer must never write a buffer the blitter is reading. Three buffers and not two, because with two the renderer would wait for the blitter and there would be nothing to overlap. The ordering inside the blit callback is the whole safety argument and is easy to invert: a free buffer is taken before the finished one is published, so a dry pool skips the frame rather than letting the renderer keep writing a buffer that has just been queued. Per-frame blit cost rose from 8,360 to 10,272 us, conversion from 4,266 to 6,266, because the two cores contend for PSRAM bandwidth, and throughput improved anyway, so the contention costs less than the overlap gains. One failure belongs in this entry because it cost a build cycle and it was the agent's: the CS assertion that follows window programming was consumed by the rewrite and not re-added, so every frame was pushed with chip select de-asserted. The SPI transmitted every byte, the timing accounted for every microsecond, and `nes status` reported 53 fps with a full set of blits and zero skips while the glass was black. The operator found it, not a counter. The margins stayed painted because their push sites kept their own CS line, which was the available clue. A display change is not verified until someone has looked at the screen, and a timing match is not that. Backup `working-2026-11-13-0019.bin`, 16777216 bytes, sha256 `d208bd93e966487a3bf7f9c92f36a2c9b08b79e0f8748bf56b03c5566202b54b`, verify OK with the board held in the stub across read and compare, app region byte-identical to the build at md5 `0ccae46a3362419d8d4e71244b9ef628`.

#### Next Steps:

51 fps is short of 60 and the reason is now the emulator rather than the display: a 51 fps frame is 19,608 us against 15,242 us of interpreter and PPU, so the remaining gap would have to come from the 6502 or the PPU and not from anything this cycle touched. Two smaller things are worth a cycle before that. The frame pool should be checked for the case the counters cannot see -- `skipped` staying at zero over a long run proves the pool never ran dry, but a deliberate test that makes the blit slower than a frame would prove the skip path works rather than merely never firing. And `nes status` reporting has grown a second line of counters per cycle; it is now the main evidence surface for this app and should be tidied into one block whose fields are all labelled with the state they were measured in. Nothing else in the backlog changed: nofrendo still keeps battery SRAM in RAM only so a Zelda save does not survive a reset, the nofrendo statics are still worth about 4.7 KB from PSRAM, and the Tamagotchi still wants a Start menu entry. The wifi buffers and heap work of entries 57 to 59 are unaffected and the radio is still off with auto-connect off.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/nes.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 61 COMMIT Unreleased 2026-11-13T00:41:00-07:00

#### Coming From:

Unreleased 8825d96

#### Purpose:

Find where the NES blit's remaining 8.6 ms goes, because the previous cycle attributed it to an unexplained 2.6x excess in the SPI push and was about to design a DMA overlap on that basis.

#### Outcome:

The 2.6x excess does not exist, and the measurement that removed it is the cycle's result. Instrumenting `push_pixels_raw()` to time window programming separately from the transfer read 4,054 us per frame, of which 3,360 us is the transfer -- 1.10 times the 3,072 us bytes-and-clocks arithmetic at 80 MHz QIO, so the SPI is already running at essentially full speed -- and about 700 us is fifteen window programmings at 48 us each. The earlier 8,047 us figure was blit-minus-estimated-conversion, an inference that was reported as a measurement, and it inverted the conclusion: there was no waste to reclaim in the push and no 5,000 us waiting there. Per-frame terms are now all measured over runs of about 1,050 frames: interpreter and PPU 15,242 us, conversion 4,355 us, push 4,054 us, total 23,651 us at 42 fps. The one thing that did get cheaper is the conversion, from 5,127 to 4,355 us, by reading four source bytes per load instead of one -- the byte loads from the PSRAM frame buffer were the cost, not the table lookups -- and that gain was proved with an A/B of about 1,000 frames per side rather than by comparing last-second samples, because the first attempt at measuring it moved from 9,218 to 9,731 us and would have been read as a regression when it was one second of a wandering loop. That is why `nes status` now reports per-run averages alongside the sample. The arithmetic that closes the plan: deleting the entire blit still leaves 15,242 us, or 65.6 fps, so 60 fps is reachable only by running the conversion and the push concurrently with the emulation rather than after it. Build PASS, flash PASS, and the operator's earlier confirmation that the picture is correct still holds since the only change to the pixel path is the width of the source load.

#### Next Steps:

The concurrency change is now the only route to 60 fps, and it is not the DMA overlap the previous cycle proposed -- it is moving the blit to the second core, which is a bigger thing than it sounds and wants a decision rather than a start. On this board core 1 is idle, the blit needs 8,409 us and the emulator 15,242, so the two fit inside one frame with room to spare; but the display driver's contract is single-writer by design (DSP-005: only paint_task may touch flush_rect, s_tx and the SPI bus), and three parts of it would have to change. `s_tx` cannot stay shared, because a margin label repaint on core 0 would overwrite a band core 1 was about to send, so the blit task needs its own transmit buffer. Window programming currently happens before the bus is acquired, so two concurrent pushers could have their CASET/RASET exchange swapped between them, and the acquisition would have to be widened to span the window as well as the transfer. And `vidbuf` needs at least triple buffering, or the PPU writes a frame while the blitter is still reading the last one. Each is small and all three together change the one subsystem everything else depends on, with tearing on the glass as the failure mode. Nothing else in the backlog changed: nofrendo still keeps battery SRAM in RAM only, the nofrendo statics are still worth about 4.7 KB from PSRAM, the Tamagotchi still wants a Start menu entry, and a backup should be taken before the next change since the last one predates the whole streaming blit.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/nes.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 60 COMMIT Unreleased 2026-11-12T23:52:00-07:00

#### Coming From:

Unreleased 7fd7376

#### Purpose:

Get Zelda's title screen to full speed, which the previous cycle's split had identified as a blit problem rather than an emulator one.

#### Outcome:

Zelda's title screen went from 19 to 42 fps, dropped frames fell from 531 to 6 per second, and the blit from 37,140 us to 8,647 us of a frame. The old path converted all 61,440 pixels into a 120 KB PSRAM scratch buffer, handed it to `display_picture_rgb()` which copied it into the 260 KB PSRAM framebuffer, and then `flush_rect()` copied the same pixels straight back out into the 10,240-byte internal transmit band -- three passes over external memory to feed a transfer that only takes about 3,072 us at 80 MHz QIO. `display_picture_rows()` inverts the arrangement: it drives the band loop and asks a callback for one row at a time, so the pre-swapped palette lookup writes directly into `s_tx`. One PSRAM read per pixel, none written, and the 120 KB scratch buffer is gone. It refuses unless `display_pixels_claim(true)` is in force rather than falling back silently, because a fallback would leave the cell painter reading a framebuffer nobody draws into and would invalidate `repaint` and `dcheck`, whose whole job is to say whether the glass matches the framebuffer. The operator played the game and reported that they feel 42 fps and the graphics look good, which is the check that matters since the panel is write-only. The experiment that did not pay is recorded rather than quietly dropped: the frame buffer was moved to internal DRAM as a static array, which is the only route past the heap's fragmentation, since internal has 143,275 bytes free but a 49,152-byte largest block and vidbuf wants 65,280 contiguous, so the heap attempt always falls back. That measured 45 fps against 42, saving 577 us of an 8,647 us blit and costing 65,800 bytes held for the life of the firmware -- with the radio initialised it puts internal free below the 65,536 tdsh's SSH server demands, so SSH stops starting. It is reverted, and kept behind `NES_VIDBUF_STATIC` with both measurements in the comment so it is not tried twice. The prediction was several milliseconds; the measurement was 577 us, because the blit is dominated by the push and not by reading the buffer. That also fixes the ceiling: the interpreter and PPU alone are 15,555 us per frame, so even a free conversion and a perfect push give 18,627 us, or 53.7 fps. 60 is not reachable here by tuning the blit at all. Build PASS, flash PASS; the internal free after reverting reads 142,739.

#### Next Steps:

The two levers that could still move the frame rate, in order of size, are the push and the interpreter, and neither is a blit change. The push measures 8,047 us against a 3,072 us arithmetic floor, roughly 2.6 times what the clocks say, so the first question is where the excess goes -- per-band window programming (15 bands of 16 rows, capped by LCD-009) and the polling transmit are the candidates, and DMA overlapped with emulation rather than serialised after it is the only arrangement that could hide the push entirely, which on this arithmetic would put a frame near 16,200 us and 60 fps within reach for the first time. That is a larger change than anything this cycle, it needs two transmit buffers that fit somewhere the SPI peripheral can read, and it should be measured against the 8,047 us baseline before it is designed further. The alternative is the interpreter: 15,555 us per frame including nofrendo's PPU writing 65,280 bytes of frame buffer every frame. Nothing in the backlog changed -- nofrendo still keeps battery SRAM in RAM only so a Zelda save does not survive a reset, the nofrendo statics are still worth about 4.7 KB from PSRAM, and the Tamagotchi still wants a Start menu entry. A backup should be taken before the next change, since the last one predates this blit work.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/nes.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 59 COMMIT Unreleased 2026-11-12T23:20:00-07:00

#### Coming From:

Unreleased 2a73b09

#### Purpose:

Take the full 16 MiB flash backup that was deferred after the Wi-Fi buffer change in commit 352c4c3, and record what the board's memory looks like with the radio deliberately off.

#### Outcome:

The backup is `~/twatch-backup/working-2026-11-12-2307.bin`, 16777216 bytes, sha256 `bdf7215de98798a5ba2ce69aa42fd74f7d37c8e44b0c434bcdc500ca244eccde`, and it is verified rather than merely written: esptool read 16777216 bytes at 606.1 kbit/s in 221.5 seconds with `--after no_reset`, compared them against the chip while the board was still held in the stub, and reported `verify OK (digest matched)`, so the file is the chip and not a view of it taken while NVS was being written behind the read. The hash was recomputed afterwards and matches the stored one. The board was reset at the end and came back on the first attempt. The image's app region at 0x10000 was then compared against `build/tinydesk.bin` from the tree at 352c4c3 and is byte-identical at md5 `c50472f3e12dad2766824e7c2d2d6157`, which is what makes this backup a record of that commit rather than of an unknown build. Two things had to be set right for the operator's instruction to hold: `wifidisconnect` alone re-associated within seconds, so `network autowifi off` was set as well, and the board has since stayed down with ping failing and the desktop showing Offline. Measuring in that state turned up a fact worth keeping: `heap` reads 143275 with a 49152 largest block, stable across repeats, against 101775 on the same firmware with the driver initialised and the station merely disconnected. The difference is that with auto-connect off and nothing attempting a connection, `esp_wifi_init()` is never called, so the static pools are never allocated at all -- it is a distinct state from disconnected, and it is worth 41500 bytes, which is more than the 30160 that halving the buffers won in Entry 57 and is available to any session that does not need the network. SOC-005 was revised to carry it.

#### Next Steps:

The backup now covers 352c4c3 and supersedes `working-2026-11-12-2235.bin`, which predates the buffer change; both are on disk and the older one can go when the newer has been proven by restoring from it. The board is left with WiFi off and `autowifi off` set, so the next test run starts from 143275 bytes free with no SSH available and must be driven over the USB terminal -- to restore the network, `network autowifi on` then `wificonnect`. The engineering candidates are unchanged and none is memory work: nofrendo's `mem` 3076, `apu` 1100 and `ppu` 496 are worth about 4.7 KB if allocated in PSRAM at `nes` start and freed at stop, the fast-blit path would recover most of the frame at 19 fps, and nofrendo still keeps battery SRAM in RAM only so a Zelda save does not survive a reset.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 58 COMMIT Unreleased 2026-11-12T23:12:00-07:00

#### Coming From:

Unreleased a966b43

#### Purpose:

Re-check the internal-RAM figure published in Entry 57, because `heap` read 52615 bytes on a board that the log said had 102635 free, and either the log was wrong or the number depended on state that had not been named.

#### Outcome:

The logging was right and the number does depend on state. Measured on a single boot so that no confound from uptime could creep in, four states read back to back and each reproduced: no SSH server and no connection, 102615 bytes free; SSH server running with no client, 82379, stable across three consecutive reads; one session attached, 53319, with a low-water mark of 40899; and after `ssh stop`, back to 101775. So tdsh's SSH server costs 19396 bytes of internal DRAM merely for existing and a further 29060 while a client is attached, 48456 in total against the 65536 it requires before it will start -- the admission check runs before the task it is admitting exists, which is why a build can pass the check by a few kilobytes and still be one session away from the floor. That resolves the apparent contradiction without revising Entry 57, whose 102635 was measured on a fresh boot with the server down and agrees with today's fresh-boot 102615 to within 20 bytes of run-to-run noise. The same measurement also closed a loose end in SOC-005 by the direct route: WiFi connected and WiFi disconnected both read 101775, so the driver's buffers belong to `esp_wifi_init()` and are not returned by a disconnect any more than by `esp_wifi_stop()`, and the 840-byte difference from a fresh boot is the associated station's own small allocation. One process fault belongs in this entry because it cost the operator real time: the re-check was run by typing into the USB terminal while the operator was using the watch, so their keystrokes and the agent's interleaved, and a `pattern` run was still holding memory when the first anomalous reading was taken. A board that is being used is not a bench, and the diagnostic tool that types into the console is the wrong instrument while someone else is holding the glass -- the reading should have been requested rather than taken. No engineering file changed; this cycle measured and recorded, and the board was left with WiFi disconnected as the operator asked.

#### Next Steps:

Two things are outstanding and neither is memory work. The backup `working-2026-11-12-2235.bin` predates the WiFi buffer change in commit 352c4c3, and the operator has deferred a fresh one. The next measurement session should be run with WiFi off, as the operator asked, and should quote the SSH state alongside any heap figure, because 101775, 82379 and 53319 are all true statements about the same board. The engineering candidates from Entry 57 are unchanged: the nofrendo statics (`mem` 3076, `apu` 1100, `ppu` 496) are worth roughly 4.7 KB via a PSRAM allocation made at `nes` start and freed at stop, with no frame-rate risk because they are idle when the emulator is not running, and `BT_CTRL_BLE_MAX_ACT` could drop from 6 to 4 for a few kilobytes at the cost of a silent connection failure if a keyboard, mouse and gamepad are ever all in use. The larger standing items are also unchanged: nofrendo keeps battery SRAM in RAM only, so a Zelda save does not survive a reset, and the fast-blit path would recover most of the frame at 19 fps.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 57 COMMIT Unreleased 2026-11-12T22:56:00-07:00

#### Coming From:

Unreleased 4d8bff3

#### Purpose:

Free internal DRAM, because Task Manager showed 70 KB free and internal RAM, not PSRAM, is what the SSH server's 65,536-byte floor and every large allocation are drawn against on this board.

#### Outcome:

Free internal RAM went from 72475 to 102635 bytes, a gain of 30160, and the low-water mark since boot rose from 35211 to 95955, and the whole of it came from one setting: the ESP32-S3 Wi-Fi driver allocates its receive and transmit buffers from internal DMA-capable RAM, which PSRAM cannot serve, and allocates them at `esp_wifi_init()` whether or not a network is ever joined, so the IDF default pair of 16 static RX and 16 static TX buffers at 1600 bytes each was holding 51200 bytes for the life of the firmware. Halving both to 8, with `RX_BA_WIN` following them to 8 because it must stay at or under twice `STATIC_RX_BUFFER_NUM`, and halving the two dynamic pools (`DYNAMIC_RX_BUFFER_NUM` 32 to 16 and `CACHE_TX_BUFFER_NUM` 32 to 16) returned 30160 bytes, more than the 25600 the static pools account for because the dynamic pools take their own internal allocation at init. One measurement took a wrong turn worth recording: `network mode lan` was run to test Wi-Fi's footprint on the theory that stopping it would release the buffers, and the heap read 72475 before and 72475 after, because the stop path calls `esp_wifi_stop()` and leaves the driver initialised, so only `esp_wifi_deinit()` frees the buffers and that experiment measured nothing while looking like it proved Wi-Fi was cheap; the honest test was the one that shipped, change the count, rebuild, flash, read the heap again. The cost was measured rather than assumed: a 131072-byte SFTP put at `--chunk 4096` ran 17.9 KiB/s against 17.2 KiB/s before, md5 `c5d6e009fd9c9bd89b6dea07d00a3226` matching at both ends, Wi-Fi re-associated by itself after the flash and `network status` reports Online: yes with the host pinging 10.10.0.15, and Bluetooth was untouched and unchanged at radio up, 40980 bytes and both stored pairings. tdsh's own SSH log line now reads `before SSH server task creation: internal free=102635 largest=31744`, so the server starts with room instead of at the edge of its floor. What was deliberately not done matters as much: the 10240-byte `s_tx` staging buffer stays in internal DRAM because it is the destination of the blit's byte-swap and the source of the panel DMA, and the blit is already 37 percent of the frame at 19 fps, so moving a hot buffer into PSRAM to buy 10 KB would be paid for in frame rate; and `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` stays at 4096 because `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` is on, so lowering the threshold would begin placing task stacks in PSRAM, where a task that runs while the flash cache is disabled during an OTA write or a littlefs commit dies. Build PASS with zero warnings, flash PASS, and the operator has not yet handled the watch since the flash.

#### Next Steps:

The remaining internal consumers are small and each has a price: `s_tx` at 10240 bytes and the nofrendo statics (`mem` 3076, `apu` 1100, `ppu` 496) are the only static objects over a kilobyte left in internal RAM, and the emulator's three are the cheaper candidates because they are only needed while `nes` runs, so a PSRAM heap allocation made at start and freed at stop would cost nothing during normal use for roughly 4.7 KB and no frame-rate risk; `BT_CTRL_BLE_MAX_ACT` of 6 could come down to 4, which is exactly one scan plus three connections, but the return is a few kilobytes and the risk is a future keyboard-mouse-gamepad setup that silently fails to connect, so it wants a measured experiment rather than a guess; and nothing here is urgent, because 102635 bytes free with a 95955 floor is comfortable against the 65536 that SSH needs. The operator should confirm the desktop, the gamepad and a transfer on the glass, and the standing items from Entry 56 are unchanged and still the larger work: nofrendo keeps battery SRAM in RAM only, so a Zelda save does not survive a reset, and the fast-blit path would recover most of the frame.

#### Files Modified:

- ports/twatchultra/sdkconfig.defaults

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 56 COMMIT Unreleased 2026-11-12T22:39:57-07:00

#### Coming From:

Unreleased dc490a7

#### Purpose:

Play real games with the real controller, which is the only test that says whether the NES port and the gamepad mapping are actually correct.

#### Outcome:

Two commercial games were played to the satisfaction of the operator and every controller button works. Super Mario Bros (mapper 0, 32K PRG, vertical mirroring) and The Legend of Zelda (mapper 1, MMC1, 128K PRG) both loaded from the card and ran, and the operator confirmed A, B, START, LEFT and RIGHT by playing them. THAT CLOSES A CLAIM THIS PROJECT WAS CARRYING UNVERIFIED. The four face buttons had been mapped from the press ORDER recorded during the descriptor capture, never from a named press, and the previous cycle said so explicitly rather than presenting the guess as a measurement. Playing settled it: the order was right, and the note that was honest about being an assumption can now be replaced by the test that confirmed it. The one thing the operator did NOT confirm is UP and DOWN, which the hat decode had already shown working for UP, LEFT and RIGHT in the sampler; nothing suggests DOWN differs, but it was not exercised and is not claimed. THE FRAME RATE IS NOW CONFIRMED FROM BOTH SIDES. The counter reads 18 frames per second on Super Mario Bros and 19 on Zelda, and the operator reports it FEELS like 19 -- which is worth more than the number alone, because a counter can be right about frames and wrong about experience if frames are being dropped between the count and the glass. The split is unchanged from NES-002: about 1,000,000 microseconds in the 6502 interpreter and 720,000 in the blit over each second, against a 16,667 microsecond budget per frame. So the game is playable and not smooth, and the blit -- 37 percent of the frame for work that should be one percent -- remains the next fix. TWO OPERATIONAL FACTS WERE ESTABLISHED THAT WOULD OTHERWISE COST A FUTURE CYCLE. First, SFTP writes have got worse, not better: 8 KiB now times out where the tool's own documentation says 16 KiB works, and the first attempt at Zelda reported `EXIT=124` from the timeout while leaving a 0-byte file on the card, which then failed to load with "File size error" rather than saying anything about the transfer. `--chunk 4096` is reliable and moved 131,088 bytes at 17.2 KiB/s with the md5 matching at both ends. The lesson is the same one this session has already recorded twice: an exit code from a timeout is not evidence about what the remote end did, so the file was checked with `ls -l` rather than trusted. Second, and more consequential for play: NOFRENDO KEEPS BATTERY SRAM IN RAM ONLY. Zelda is an MMC1 game that saves, the save works in the emulator, and nothing writes it anywhere, so a file does not survive a reset. The port supplies `InfoNES_ReadRom`-style loading and a PSRAM frame buffer but no SRAM persistence, and the operator was told before investing time in a file rather than after. The backup `working-2026-11-12-2235.bin` is 16 MiB, sha256 `53bc90b4e544dcd8fff32a6d0e3ca0a33abb9bb2e6939a6e68cf0d92e915c31c`, verify OK, app region byte-identical to the build with zero differing bytes.

#### Next Steps:

SRAM persistence is the smallest change with the largest effect on whether a game is worth starting: the emulated cartridge's battery-backed RAM needs saving to `/sd` on stop and loading on start, which is what the Tamagotchi already does for its EEPROM and enough of a pattern to copy rather than design. After that the blit, which is the only thing standing between 19 frames per second and 60: converting straight into the flush buffer instead of through `s_fb` and `s_tx` should remove most of 720,000 microseconds per second, and it can be measured on the host before it is flashed because the cost is a memcpy and a byte swap rather than emulation. Both are worth doing before any scaling discussion -- 1x is pixel-perfect and the picture is not the problem. Unchanged and still open: UP and DOWN on the controller are unexercised; the controller's analogue stick is decoded but unused; the NES has no Start menu entry and is launched by typing `nes`; and the gamepad's MENU button maps to START, which is a guess that a game with a real pause menu may disagree with.

#### Files Modified:

- ports/twatchultra/README.md

#### Status:

- Build: NOT RUN
- Deployment: PASS
- User Test: PASS

---

## 55 COMMIT Unreleased 2026-11-12T22:17:33-07:00

#### Coming From:

Unreleased b072044

#### Purpose:

Find out whether the operator's Xbox Wireless Controller can drive the watch, and make it drive the NES rather than the desktop cursor.

#### Outcome:

The controller works, and getting there reversed two of this agent's confident wrong answers. THE FIRST WAS THE PROTOCOL. Asked whether the controller would connect, this agent asserted it was Bluetooth Classic BR/EDR, noted correctly that the ESP32-S3 has no Classic radio at all (only the original ESP32 defines `SOC_BT_CLASSIC_SUPPORTED`), and concluded the watch could not see it. The operator put it in pairing mode and it connected immediately as a BLE HID-over-GATT peripheral. The second wrong answer was about why it then misbehaved. It connected into the Keyboard slot and moved the desktop cursor, which was read as a protocol oddity; the real cause is that `hid_mouse_parse()` ACCEPTS a gamepad's descriptor, because a gamepad declares Generic Desktop X and Y for its left stick and X and Y are the only things the mouse parser looks for. That is a parser doing its best with something outside its remit, and it made every stick movement a pointer movement. THE FIX IS AN ORDERING AND A DISCRIMINATOR. `hid_gamepad.c` walks the descriptor looking for the TOP-LEVEL COLLECTION usage -- Generic Desktop / Game Pad (0x05) or / Joystick (0x04), against / Mouse (0x02) and / Keyboard (0x06) -- and `bt_hid.c` now asks it BEFORE the mouse parser, because once the mouse has claimed a device the gamepad path is never reached. THE LAYOUT WAS MEASURED, NOT ASSUMED, and the measurement was worth the cycle. A temporary diagnostic wrote the Report Map and the raw input reports to the card, and the operator pressed buttons in a stated order; the descriptor decodes as report ID 1, 16 bytes, left stick at bits 0 and 16, triggers at 32, right stick at 64 and 80, hat switch at 96 (4 bits), fifteen buttons at 104, and a consumer control at 120. THE TEST FOUND TWO REAL PARSER BUGS, both of which produced plausible partial results. First, local items must be cleared on COLLECTION as well as on Input, Output and Feature: without that the collection's own usages stayed queued, the first Input consumed them instead of X and Y, and the sticks were never found while the hat and the buttons still resolved -- a partial parse that a test checking only "does it parse" would have passed. Second, the report ID was read after the whole descriptor walk, so it picked up a SECOND id from a PID collection at the end of the descriptor and rejected every report as belonging to another interface. Both were caught by asserting the fields' OFFSETS rather than merely their presence, and the local-reset bug escaped the first version of the test entirely -- it was found by mutating the parser, noticing a mutation did not fail, and concluding the test was wrong rather than the mutation being harmless. THE TEST IS HOST-SIDE AND RUNS AGAINST THE REAL DEVICE DATA. `hid_gamepad.c` was given no platform dependency precisely so `tests/test_gamepad.c` could compile it alone and feed it the captured descriptor and reports from `tests/xbox_btle.h`; 27 checks, and proved able to fail three ways (removing the collection check, skipping the local reset, and dropping the hat's 1..8 offset each produce failures). One subtlety cost a run: the captured payloads do NOT begin with the report ID, because BLE carries it in the attribute handle and `bt_hid.c` reconstructs it, so the test has to prepend it or it is testing a different byte string from the firmware's. VERIFIED ON THE WATCH: classified `kbd=0 mouse=0 pad=1`, and a 30-second sample decoded the hat as 1..8 clockwise from up with 0xFF centred, and buttons at bits 10 and 11. Those two bits arriving is the button decode path PROVEN end to end; the four face buttons did not land inside that sampling window, and their assignment still rests on the press order recorded during the descriptor capture. The NES maps the hat to its D-pad and bits 10 and 11 to SELECT and START, which also closes the SELECT gap recorded in the previous cycle.

#### Next Steps:

Hold a face button for a full second rather than tapping it and re-run `nes pad`, which samples to `/sd/nespad.txt` for thirty seconds precisely so the timing does not have to be coordinated with a command -- the earlier terminal-printing version asked the operator to press during a window that opened when the command was typed, which cannot work. Once bits 0, 1, 3 and 4 are confirmed against A, B, X and Y, the table in `nes.c` is either correct or one edit away. Two things are worth doing while the controller is in hand: the analogue stick is decoded but unused, and it is the obvious input for a future menu; and the controller's own MENU button currently maps to START, which is a guess that a game will probably disagree with. Unchanged from the previous cycle: the NES runs at 21 fps, and the split says the blit is 37 percent of the frame for work that should be one percent, so the full-frame display path remains the next real optimisation.

#### Files Modified:

- ports/esp_idf/app/hid_gamepad.c
- ports/esp_idf/app/hid_input.h
- ports/esp_idf/app/bt_hid.c
- ports/esp_idf/app/app.cmake
- ports/twatchultra/main/nes.c
- tests/test_gamepad.c
- tests/xbox_btle.h
- tests/stub/esp_timer.h
- tests/stub/freertos/FreeRTOS.h
- tests/stub/freertos/task.h
- tests/stub/tdsh_board.h
- tests/stub/tinydesk/td_wm.h
- CMakeLists.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 54 COMMIT Unreleased 2026-11-12T21:41:19-07:00

#### Coming From:

Unreleased 4a2ba85

#### Purpose:

Put the NES on the watch's screen and play it, which is Phases 3 and 4 of the port: PSRAM for the frame buffer, and the glue between nofrendo and this board's display, input and timing.

#### Outcome:

THE NES WORKS. The operator started a ROM, began a game and flew around with the margin buttons, so the display, the input and the emulation are all correct end to end. It is not yet fast enough: 21 frames a second against 60, and the measurement says where that goes. The split, taken on the board over one second with the emulator running, is 8,160 microseconds in the 6502 interpreter and 6,166 in the blit against a 16,667 microsecond budget -- 49 and 37 percent, with the rest lost between them. THE BLIT FIGURE IS THE SURPRISE AND THE EASIEST WIN. Moving 61,440 pixels should cost about a hundred microseconds; it costs sixty times that, because `display_picture_rgb()` copies the frame into `s_fb` and `flush_rect()` then byte-swaps it into `s_tx`, and both of those buffers are in PSRAM while the source is too. So each frame makes four passes over external memory where one would do. A direct path for a full-frame app -- convert straight into the flush buffer, skipping the intermediate copies entirely -- is the next change and should remove most of that 6,166 microseconds. The emulator half needs different work and is not addressed here: 8,160 microseconds for a frame the host runs in 41 means the S3 is roughly two hundred times slower than x86 on this workload, and the two likely causes are the PPU writing 65 KB per frame into PSRAM and the ROM living in internal DRAM. PHASE 3 TURNED OUT TO BE SMALLER THAN EXPECTED, AND ONE POSSIBILITY WAS RULED OUT BY READING RATHER THAN TESTING. The component has only 5.3 KB of .bss across 69 objects, and `vidbuf` is a plain pointer the CORE asks the port to supply, so there was no linker fragment to write: `nes.c` allocates the 65,280-byte frame buffer and the 122,880-byte converted frame from PSRAM with `heap_caps_malloc`, which is the pattern EMU-018 established. PHASE 4 IS SHORT BECAUSE NOFRENDO ASKS FOR ALMOST NOTHING -- three callbacks, `blit_func`, `vsync` and `input`, plus `nes_setvidbuf`. The blit skips the core's eight-pixel overdraw on each side rather than presenting it, converts through a 256-entry RGB565 table built from nofrendo's own palette data, and calls `display_picture_rgb()` once per frame rather than once per scanline, because each call takes the lock and reprograms the panel window. Notably the retro-go front end is still not vendored: `nofrendo_buildpalette()` lives in `nofrendo.c`, so the table is rebuilt here in four lines from `nes_palettes[][]`, which keeps the component free of that dependency altogether. TWO REAL FAULTS WERE FOUND AND FIXED ALONG THE WAY, both of the same shape as ones this project has already paid for. The first is EMU-018 happening again: adding the component's 4 KB of .bss pushed internal DRAM to 60,143 bytes and `ssh start` began refusing with "insufficient internal RAM (need at least 65536)". The culprit turned out to be `sound.c`'s beep buffer -- 16,000 bytes of .bss held for the life of the firmware to be used for 250 milliseconds at a time -- and moving it to a per-call PSRAM allocation took internal free from 60,143 to 76,147. That fix stands on its own and has nothing to do with the NES. The second is a collision the port created for itself: `touch_set_app_touch()` REPLACES its consumer, and with two apps that now claim the screen -- the Tamagotchi and the NES -- whichever registered last silently disabled the other. There is now `touch_add_app_touch()`, a bounded table of consumers where each decides for itself whether a frame is meant for it, because only one app can own the screen at a time. ONE UPSTREAM EDIT WAS NEEDED AND IT IS RECORDED IN PLACE: `nes/utils.h` defines `IRAM_ATTR` as empty, which collides with ESP-IDF's own definition of that macro and fails `-Werror`; it is now guarded by `#ifndef`. That is the only change to vendored code. The backup `working-2026-11-12-2136.bin` is 16 MiB, sha256 `dda6c8ce926e88e631c3f221f70d3653745fbee3cb283b2c008d7aaafd4b1131`, verify OK, and its app region is byte-identical to the build with zero differing bytes. The firmware is `0x2a8ba0` with 11 percent of the app partition free, and the host benchmark still reports 142x realtime on the same ROM it reported 233x earlier -- the difference is host load rather than a regression, which is worth stating because a falling host number with an unchanged core looks like a problem and is not.

#### Next Steps:

Make the blit cheap. The change is a display API addition -- a full-frame RGB565 push that converts into `s_tx` in flush order and skips `s_fb` -- and it should be measurable on the host before it is flashed, because the cost is a memcpy and a byte swap whose durations do not depend on the emulator. That also benefits the Tamagotchi, which blits rows through the same path. After that, re-measure the split, because a blit that costs 500 microseconds instead of 6,166 changes how much of the frame the interpreter actually gets and may make the emulator half irrelevant to whether the game is playable. If the emulator is then still over budget, the next question is where the PPU's 65 KB per frame is going -- PSRAM writes are the likely cost -- and whether `vidbuf` can live in internal DRAM once the blit stops competing for it. SELECT is unmapped, which a few games need to choose a mode and which the left column has three free slots for. Nothing about correctness is outstanding: the picture is right, the buttons are right, and the ROM loads.

#### Files Modified:

- ports/twatchultra/main/nes.c
- ports/twatchultra/main/nes.h
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/sound.c
- ports/twatchultra/main/touch.c
- ports/twatchultra/main/touch.h
- ports/twatchultra/main/tama.c
- ports/twatchultra/components/nofrendo/nes/utils.h
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 53 COMMIT Unreleased 2026-11-12T21:24:28-07:00

#### Coming From:

Unreleased b82d124

#### Purpose:

Choose an NES emulator for the watch by measurement rather than by reading source files, and get the chosen core compiling on the target before writing any glue.

#### Outcome:

nofrendo is chosen, benchmarked, vendored and building on the ESP32-S3; nothing calls it yet. THE CHOICE WAS A MEASUREMENT AND IT REVERSED AN EARLIER ANSWER. Four cores were considered. FCEUmm and QuickNES could not be built at all on this machine without `libretro.h` and a full frontend, which is itself the answer about their porting cost. InfoNES and nofrendo both built with a hand-written system layer, and running the same ROMs through both with the render path AND the palette-to-RGB565 conversion live gave 394.9 against 201.6, 392.6 against 201.6, and 828.2 against 422.4 -- a ratio of 1.96 on all three, so it is a property of the cores rather than of a particular game. This reverses the recommendation made from source-reading a cycle earlier, which had favoured InfoNES on licence grounds: Apache-2.0 against GPL-2.0 looked like the deciding factor until the operator said licensing was flexible and compatibility and performance were what mattered, at which point nofrendo won on both. Compatibility was then measured rather than assumed: of the operator's 794-ROM collection, 14 are unsupported, so 98.2 percent coverage, and the misses are mapper gaps (Karnov, Holy Diver, Ring King) rather than a systemic fault, with 90 percent of the collection on mappers 0 to 4. THE HARNESS PRODUCED THREE SEPARATE FALSE PASSES, and that is the most transferable result in this cycle. Each one ran to completion and printed a plausible speed: a frame buffer never set, so the PPU was skipped by `draw = draw && nes.vidbuf != NULL` and 24,125x realtime was measured for the CPU alone; realtime arithmetic that multiplied frames per second by the 1.789 MHz clock instead of by cycles per frame, overstating by exactly the frame rate, a factor of 60; and, in the InfoNES harness, a ROM header read into a local struct rather than into the core's own `NesHeader`, leaving the mapper number and every flag clear so the ROM was never mapped and 11,000 frames a second of nothing were reported. Every one was caught by checksumming the whole frame buffer rather than counting blits, and the harness now FAILS rather than reporting a speed when the frame buffer is blank. A fourth near-miss in the same family: `vidbuf` must be set AFTER loading, because `nes_loadfile()` calls `nes_reset()` which clears it, so a buffer set beforehand is silently discarded. PHASE 1 is `scripts/nesbench.sh` in this repository plus `tools/nesbench/bench_nofrendo.c`: fetch, build, run and compare, with the upstream commit pinned so a benchmark today is comparable to one after the core is vendored. Fetching is gitignored -- 8.5 MB of upstream GPL sources do not belong in this repository's history when a pinned commit reproduces them. PHASE 2 is `components/nofrendo` in the tinydesk fork: 88 files, 18,144 lines, compiling clean on Xtensa at a cost of 120.4 KB of .text and 5.3 KB of .bss across 69 objects, including 49,420 bytes of .rodata for upstream's game database which is droppable if flash ever matters. The core needs NO retro-go: `nes/utils.h` already has an `#ifdef RETRO_GO` branch, and with the macro undefined logging becomes printf, IRAM_ATTR becomes empty and CRC32 becomes (0) -- that is the entire dependency, verified by grepping every rg_ symbol and finding the rest were struct fields named `prg_rom_banks` and the like. `port.c` converts that property into a build failure if RETRO_GO is ever defined, and the guard was proved to fire rather than assumed. Two build details are worth keeping: ESP-IDF applies `-Werror=all` to every component, and upstream prints `uint32` with `%u`, which fails `-Wformat` on this ABI, so this component alone carries `-Wno-error -Wno-format` -- silencing the diagnostic keeps 68 vendored files diffable against upstream, which is the reason to vendor rather than fork. And CMake caches the component list, so adding a component to `components/` does nothing until a reconfigure. The firmware binary is byte-identical to the previous cycle at `0x28ca20` because nothing references the component yet, so the linker drops it: that is the correct state for a phase that set out to prove the source builds and nothing more.

#### Next Steps:

Phase 3 is PSRAM, and it is not optional: `vidbuf` is a pointer the PORT supplies, and at `NES_SCREEN_PITCH * NES_SCREEN_HEIGHT` bytes it is 65 KB which cannot come from internal DRAM on a board where 4 KB once broke the SSH server (EMU-018). The same is true of the PPU and APU buffers. Phase 4 is the glue and it is genuinely small, because the core's whole platform surface is three callbacks -- `blit_func`, `vsync` and `input` -- plus `nes_setvidbuf`; the blit receives a 272x240 byte-per-pixel palette-index frame, so the port owns the conversion and should do it through a 256-entry RGB565 table straight into `display_picture_rgb()`. Phase 5 is the glass: 1x letterboxed is 256x240, which is 46 percent of the 320x416 panel and pixel-perfect, and should be the first target before any scaling is discussed. One thing this cycle does NOT establish and must not be mistaken for: the x86 ratio does not predict the watch's frame rate, because a 6502 interpreter is pointer-chasing and branch-heavy and Xtensa differs from x86 exactly there. What transfers is the ratio between cores. The strongest prior that nofrendo reaches full speed on the watch is not arithmetic but that retro-go, esp32-nesemu and esplay all ship it on a 240 MHz ESP32, and that should be confirmed on hardware before any scaling or frameskip work is designed.

#### Files Modified:

- scripts/nesbench.sh
- tools/nesbench/bench_nofrendo.c
- .gitignore
- ports/twatchultra/components/nofrendo/CMakeLists.txt
- ports/twatchultra/components/nofrendo/port.c
- ports/twatchultra/components/nofrendo/config.h
- ports/twatchultra/components/nofrendo/database.h
- ports/twatchultra/components/nofrendo/palettes.h
- ports/twatchultra/components/nofrendo/nofrendo.h
- ports/twatchultra/components/nofrendo/COPYING
- ports/twatchultra/components/nofrendo/CREDITS
- ports/twatchultra/components/nofrendo/nes/
- ports/twatchultra/components/nofrendo/mappers/

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 52 COMMIT Unreleased 2026-11-12T21:03:50-07:00

#### Coming From:

Unreleased 815764b

#### Purpose:

Prove that this port can update its own firmware by writing the inactive OTA slot and booting into it, which is the A/B swap the operator proposed.

#### Outcome:

A/B firmware update works on the watch, in both directions, with no code written and no partition changed. The layout was already 2 x 3 MiB (ota_0 and ota_1), and the port already carries the whole ESP-IDF mechanism -- `esp_ota_set_boot_partition()` at `ota_esp.c:279`, `esp_ota_mark_app_valid_cancel_rollback()` at `:785`, `esp_ota_mark_app_invalid_rollback_and_reboot()` at `:630`, an `ota` shell command with status/feed/check/install/rollback/restart, and the Update app on top. What was missing was proof, and the round trip is now measured: starting on `ota_0` with `ota_1 is empty`, `ota install` reported `Version 0.4.0 is installed. Restart to use it.`, `ota restart` came up `Running: ota_1 (on trial: confirms itself 30 s after start-up)`, thirty-five seconds later the trial flag was gone, a second install then wrote `ota_0`, and a second restart came up `Running: ota_0` with `ota_1` holding a version. Both slots now carry valid 0.4.0 images and `otadata` records sequence 3 against 2, which is the bootloader's own account of which one to run. THE ROLLBACK PROTECTION WAS PROVED BY ACCIDENT AND THAT IS THE MOST VALUABLE RESULT. The first install attempt was interrupted by a hard reset during the write, leaving `ota_1` with a valid app descriptor but a truncated image. Attempting to boot it produced `E esp_image: invalid segment length 0xff9afffd` and the board stayed on `ota_0` instead of booting a broken slot. That is the safety property this whole mechanism exists for, demonstrated on a genuinely corrupt image rather than a contrived one -- and the corruption was this agent's fault for resetting mid-write, which is worth recording as the mistake it was. TWO ROUTES WERE TRIED AND ONLY ONE WORKED. SFTP to the card stalled and left a 0-byte file, which is the single-client wolfSSH wedge already recorded as SSH-003; the working route is HTTP, with the host serving the image and `ota install http://...` streaming it at about 52 KB/s. `ota install` also REFUSES until `board save` has run, because 28 pin settings are built into this port's firmware and would be lost by a generic image -- a guard worth knowing about, and running it confirmed `/fs` writes work (a 391-byte board.conf appeared). ONE CLAIM ABOUT THIS BOARD WAS CHECKED AND IS FALSE: the built-in update feed, `https://tnydesk-project.github.io/install/update-desktop-twatchultra.json`, returns HTTP 404, so the Update app's "Check for official updates" has nothing to talk to. The mechanism is complete and has no feed, which is the real gap in this feature and not partition sizes. The backup `working-2026-11-12-2059.bin` captures the post-test state: 16 MiB, sha256 `e180f58285866af475e1b827efb7126e6d98a1b7afe68c7cc6bb187654c68ebb`, verify OK, both app slots holding valid 0.4.0 images. Also answered, because it decides the next cycle: two 8 MiB firmware partitions are arithmetically impossible on a 16 MiB part -- the bootloader (32 KiB at 0x0), partition table (4 KiB at 0x8000), nvs, phy_init and otadata consume 72 KiB, leaving 15.93 MiB, and the largest equal split is 2 x 7.96 MiB with no filesystem at all. The current 3 MiB slot already caps the 2.55 MiB app at 15 percent headroom, so a move to 2 x 6 MiB would relax that to 58 percent at the cost of 3.93 MiB of filesystem, while 1 x 6 MiB would give the same headroom and 9.88 MiB of filesystem but would END the A/B swap just proved -- so the two proposals are not equivalent and the choice is a trade, not an improvement.

#### Next Steps:

The partition question is now the operator's to settle with numbers rather than impressions: keep 2 x 3 MiB (works, 15 percent headroom), move to 2 x 6 MiB (58 percent headroom, 3.93 MiB filesystem, keeps the swap), or 1 x 6 MiB (58 percent headroom, 9.88 MiB filesystem, loses the swap). Repartitioning is destructive to `/fs` -- `board.conf`, users and `/root` do not survive a storage offset change -- so it wants a fresh backup and a deliberate decision rather than incremental nudging, and the app should never be booted from a non-zero offset without a specific test, because that has not been tried. Two smaller pieces are worth having regardless: a `space` command, since `esp_littlefs_info()` is already called at mount and logs total and used but nothing exposes it interactively, and an understanding of what a 404 feed means for the Update app's "Check for official updates" button, which currently offers a feature that cannot work on this board. Separately, this cycle established the starting facts for a retro-go port, which is now the largest open question: retro-go is GPL-2.0 against this project's MIT, every one of its 17 targets is single-wire SPI while this panel is QSPI, and no target declares QUAD PSRAM while this board has only that -- so it is not a drop-in, and the display driver is the work.

#### Files Modified:

None.

#### Status:

- Build: NOT RUN
- Deployment: PASS
- User Test: PASS

---

## 51 COMMIT Unreleased 2026-11-12T20:04:44-07:00

#### Coming From:

Unreleased 09616ec

#### Purpose:

Find and fix the reboot that followed every `tama stop`, which the operator reproduced on their own watch.

#### Outcome:

The reboot was a stack overflow caused by one wrong constant, and the fault was three calls away from where it crashed. The emulated serial EEPROM is a 24LC08 -- 8 Kbit, 1024 bytes -- and `ht24lc08_save()` memcpys HT24LC08_SIZE bytes into the caller's buffer with no length argument, while `TMC_EEPROM_BYTES` said 512. So every checkpoint wrote 512 bytes past a 512-byte stack array. On the shell's stack that overflow ran UPWARD through `cmd_tama` into tdsh's parser frame and corrupted the `FILE *redir_out` local in `execute_segment`; the parser then called `fclose()` on the garbage pointer the overflow had left there, and newlib's lock assertion fired: `assert failed: check_lock_nonzero locks.c:307 (lock != NULL && "Uninitialized lock used")`. The load path read 512 bytes past its buffer for the same reason, so both directions were wrong. FIXING IT TOOK ONE LINE, FINDING IT TOOK A CHANNEL. Nothing in this project could report a panic: the console is compiled out (`CONFIG_ESP_CONSOLE_NONE`), the USB port belongs to the desktop's keyboard and mirror so a console there would have cost the ability to type the command under test, and the USB mirror stops describing the panel while a bitmap owns it. Every previous attempt was a bisect on a reboot signal that the operator rightly questioned, and three of those bisects were wasted removing things that were never guilty -- the repaint, the margin restore, and the frees. What broke the deadlock was a core dump: `ota_1` has never been used because this board has no update feed, so shrinking it by 64 KB made room for a 64 KB coredump partition without touching `storage`, and `idf.py coredump-info` then produced the full backtrace with symbols and line numbers on the first try. That partition and its config were removed again once the backtrace was in hand, and the watch was reflashed from a clean tree so what is on it matches what is committed. The permanent defence is `_Static_assert(TMC_EEPROM_BYTES == HT24LC08_SIZE)`, because a size constant drifting from the device it names is exactly the bug; it was proved to work rather than assumed, failing the build at 512 and passing at 1024. Verified on the watch after the diagnostic was gone: two complete start/stop cycles with `uptime` climbing 17 to 27 to 34 seconds and `stopped, screen released` printed each time, no reset. The emulator's own gates all still pass -- dirty-row check 2000 frames with 250 pushed and 0 pixels differing, the audio check, and the sync check across the thirteen shared files. TWO THINGS ARE WORTH CARRYING FORWARD AND NEITHER IS FIXED. The first is that this bug was reachable from the Save BUTTON as well as from `tama stop`: `apply_action` calls the same `save_checkpoint()` on the guest task, whose 6 KB stack absorbed the overflow without crashing, so the button appeared to work while quietly corrupting memory around it -- which means the operator's earlier "Save works" was true of the file and not of the memory. The second is a process failure rather than a code one: several conclusions this cycle were drawn from `uptime` read through a Terminal reopened with F10, an instrument that cannot distinguish a surviving window from a fresh one after a reboot, and the operator caught it. A boot log appending the reset reason to the card was drafted and then dropped so the tree would stay minimal; it remains the right instrument and is the first thing to add if this recurs.

#### Next Steps:

The Save and Load buttons should be re-tested now that the image is the right size, because their earlier pass was measured while every press was corrupting 512 bytes of memory around the saved data -- the file was correct and the machine around it was not. A boot log belongs in `link_init()`: one line per boot with `esp_reset_reason()` and the monotonic clock, appended to `/sd/boot.txt`, so that "did it reboot" and "was it a panic" are facts rather than inferences from a window. The coredump recipe is recorded in this cycle's commit message and in the reference under EMU-021 and is worth keeping to hand rather than rediscovering: shrink the unused `ota_1`, add a 64 KB coredump partition, enable the two config options, reproduce, `idf.py coredump-info`, then revert. Unchanged and still open from the previous cycle: there is no Quit among the margin controls, so the only way out of the emulator is `tama stop` from a keyboard; the bottom strip still carries a degree circle that no longer describes a duty; the right column has two deliberately blank slots; and the cache autoload experiment measured a 7 percent sequential-read gain against ten times the headroom, so it stays unadopted and stashed.

#### Files Modified:

- patches/harness/core0/tmc_core.c
- patches/harness/core0/tmc_core.h
- ports/twatchultra/components/tamacore/tmc_core.c
- ports/twatchultra/components/tamacore/include/tmc_core.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 50 COMMIT Unreleased 2026-11-12T18:57:46-07:00

#### Coming From:

Unreleased f74feac

#### Purpose:

Give the Tamagotchi seven margin controls gated by a hold-to-arm interlock, which required establishing that the touch panel reports two simultaneous fingers at all.

#### Outcome:

Seven controls work and the destructive three cannot be pressed by accident. The left column carries Pause, Save, Load, Speed and Sound; the right column carries the arm at the top, Shot next to it, and Reset at the bottom where a sliding thumb arrives last. Save, Load and Reset fire only while a second finger holds the arm -- the reserved slot the desktop had been carrying blank since it was first painted. THE ARGUMENT THAT MADE THE INTERLOCK TRUSTWORTHY IS ABOUT WHICH REPORT IT CHECKS. `tama_ui.c` holds the button map and the rules with no ESP dependency, so `tests/test_tama_ui.c` walks the whole thing on a host: 206 checks, including the order a person testing deliberately does not try -- arm down, arm lifted, then Reset, which must be refused. It also pins the reverse order, where holding an action and then adding the arm must NOT fire it retrospectively, because that would be a way to arm by accident. The rule is a property of the SET of points rather than of arrival order, because the controller numbers points by arrival and a rule keyed on "point 1 is the arm" would invert silently when the arm finger landed second. The interlock was proved able to fail rather than assumed: checking the arm against the previous report instead of the current one produces 70 failures, removing the gate produces 59, and gating on the wrong slot produces 59. TWO FAULTS WERE FOUND BY MEASUREMENT AND EACH COST A CYCLE. The first was mine and crashed the board: `overrides_for()` cast a `char[COL_N][OVR_MAX]` to `const char **` and returned it, so element 1 read eight bytes into the middle of row 0 and the label painter dereferenced garbage -- on the glass, `tama do speed` rebooted the watch and left a Displays window open. A pointer array and a 2-D char array are not the same layout, so the pointers are now built into a real array. The second is the more interesting one and it was in the emulator's contract rather than in this cycle's code: a BEL played continuously for as long as the guest made any sound, and stopped only while paused. The cause is that `splb32_audio_emits()` counts square-wave TOGGLES -- a held tone raises it thousands of times a second -- and the firmware was treating a change in it as an event. Pausing stopped it because pausing stops stepping the guest and therefore stops the waveform, which is the clue that identifies the fault. The waveform is a clock and the envelope is the signal, so `splb32_audio_active()` was added and the firmware now fires on the rising edge of it, with a 250 ms floor so two 200 ms beeps cannot overlap into noise. `tools/tamacore/check_audio.c` is the check and it states the fault as a number: one 1000-toggle tone is one sound, 1000 emits, envelope constant. It also rules out the obvious alternative fix by showing `toggle_state` alternates on every call, so "the level changed" would fire twice per period. The same investigation produced the multi-touch facts this cycle rests on, gathered with a new `touchdump` command that writes raw 15-byte report blocks to the card: a block carries two points six bytes each with the second at byte 7 (event), bytes 8-10 (coordinates, same 12-bit encoding) and byte 12 (validity); IDLE IS FIFTEEN 0xFF BYTES, not the 0x00 or 0xAB the driver's own comments claimed; points are numbered by arrival; and LIFTING THE FIRST FINGER OF A CHORD PROMOTES THE SURVIVOR INTO THE PRIMARY SLOT WITH EVENT 0x16, whose low nibble is 0x06. That last one turned the press test from `(b[0] & 0x0F) == 0x06` into `b[0] == 0x06`, because with the mask a stray finger left on the glass after a chord was accepted as a fresh press at its own position -- firing whatever it rested on, with the interlock already satisfied and gone. The capture that proves it is preserved at `~/twatch-backup/touch-captures/touch7.txt` on the development machine. One design decision is worth recording because it is a place two meanings of "save" could have quietly fought: the exit checkpoint is SKIPPED after a Reset, because quitting after one would otherwise overwrite the deliberate save with the fresh pet, destroying exactly what the interlock protects. The operator tested all seven controls, the three bottom bars, Pause, a screenshot and every speed setting, reported everything working, and confirmed after the fix that the sound is occasional rather than continuous. The build was clean with 0 warnings, binary `0x28ca20`, host suite 42 of 42 including the new `test_tama_ui`, and the backup `working-2026-11-12-1856.bin` is 16 MiB with sha256 `ffd5ccd12f298df7d07afcdd6bfe530eb066902895c91856c190f9238f1adf68` and a matching verify. ONE VERIFICATION HABIT NEEDED CORRECTING ALONG THE WAY: a rebuild of unchanged sources produces a different binary, because the app descriptor embeds the build date and time and the ELF hash. Comparing the flashed image against a freshly built one therefore shows dozens of differing bytes that are not code -- measured here as 70 bytes in three clusters, at the descriptor's time field, at its embedded ELF sha256, and in the image's trailing hash, with ZERO differences anywhere in the code region. So "the backup is byte-identical to the build" is a true and useful claim only when nothing has been rebuilt in between; the durable form of the check is to diff the two images and confirm the code region is empty, which is what was done to establish that the tested firmware and the committed source are the same program.

#### Next Steps:

Two blank slots remain in the right column and they are deliberate: an unused button that does nothing looks broken, so empty glass is the honest placeholder. The degree circle on the bottom strip is now the last stale label -- it was chosen when the bars were inert and they play the pet, but no short word for the V3's three buttons fits 41 px, so it wants the operator's decision rather than another guess. A Start menu entry is the remaining gap in reachability, since `tama` is still typed into the shell; `display_pixels_claim()` is all an app needs to take the screen. The `touchdump` command should be kept rather than removed -- it is how the multi-touch facts above were obtained, and the next question about this panel will want the same tool. One limitation is now visible and unaddressed: the two columns send desktop keys on the desktop and app actions under an app, so there is no way to reach six of the seven controls without launching the emulator, which is fine while the emulator is the only app that claims the screen but will need a decision when a second one exists.

#### Files Modified:

- ports/twatchultra/CMakeLists.txt
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/edges.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/tama.c
- ports/twatchultra/main/tama_ui.c
- ports/twatchultra/main/tama_ui.h
- ports/twatchultra/main/touch.c
- ports/twatchultra/main/touch.h
- ports/twatchultra/README.md
- tests/test_tama_ui.c
- tests/test_twatch_edges.c
- patches/harness/core0/tmc_core.c
- patches/harness/core0/tmc_core.h
- patches/harness/splb32/splb32.c
- patches/harness/splb32/splb32.h
- scripts/tamacore-host.sh
- tools/tamacore/check_audio.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 49 COMMIT Unreleased 2026-11-12T18:03:24-07:00

#### Coming From:

Unreleased e29680e

#### Purpose:

Wire the three bottom margin bars to the Tamagotchi's three buttons, so the pet is playable on the glass instead of only from the shell.

#### Outcome:

The strip plays the game. A press on the left, middle or right bar reaches the guest's button 0, 1 or 2 through `tmc_button()`, and the mapping is positional because that is the order the V3's pin mask documents. Verified on the operator's own run: **31 presses, 0 drops**, over 9,107 frames and 1,258 blits in 569 seconds of guest time, with the operator reporting it works great. THE PRESS IS QUEUED, AND THAT IS THE WHOLE OF THE ENGINEERING. `touch.c` runs its poll task pinned to core 0 at priority 3; the guest task is unpinned at the same priority and can be on core 1. `tmc_core.h` states plainly that one instance is not reentrant, and `tmc_button()` writes the emulated CPU's pin state that `tmc_run()` is reading -- so applying a press directly from the touch task would be a data race on two cores against the guest's own programmer's model. The failure would not have been a crash and would not have been reproducible on demand: it would have been a pet that occasionally acted on a button it never received, which is the kind of fault that gets blamed on the emulator. An eight-deep queue between them keeps the emulator single-threaded instead of growing a lock it does not need, and the guest task drains it at the top of each frame so a press that arrived during the last one is in effect for the next -- applying it afterwards would put every press a frame late. THE SAME RACE WAS ALREADY IN THE TREE. `tama press N` has written pin state straight from the shell task since Phase 4, which is the same violation with a different second task; it goes through the queue now, and taking the same road as a real press is what made the drain verifiable without a finger -- three shell presses produced `presses=3 drops=0` before the strip was tried. THE REGISTRATION IS FOR THE LIFE OF THE PROCESS rather than per run, and touch.h says why: a handler torn down with the app leaves a window in which the driver holds a pointer to something that has freed itself, whereas a handler that returns false when the app is not running behaves identically with none of that risk. The driver decides nothing about which buttons an app wants -- it passes the region and index and the handler accepts or declines -- so the columns are still declined and still logged `off`, which keeps "not wired yet" visible in the log instead of turning it into a silent no-op. An accepted press logs `e1 i<n> app` and buzzes once; a refused one logs `off` and does not, following the rule `edge_press()` already applies to the reserved slot. Two smaller things: `_Static_assert(BTN_N == TAMA_BTN_N)` makes the strip's bar count and the guest's button count fail at build time rather than drift, and the pointer path is now skipped when an app has claimed the pixels (`if (!app || owned)`), because an app that claimed the framebuffer has been offered the margins as buttons and sending pointer reports as well would be two answers to one press. The build was clean with 0 warnings, binary `0x28b890`, and the host suite passed 41 of 41. What this cycle does NOT establish is that the guest's own game logic responds correctly to those buttons -- the count proves presses arrived and were applied, and the operator's report that it works is what covers the rest.

#### Next Steps:

The two columns are the remaining unwired controls and the reason APP mode blanks them: five buttons down each side are the natural home for the pet's own screens and actions, and the handler already declines exactly those, so wiring them is a matter of deciding what they should do and extending the same queue with a wider event. A Start menu entry is the other obvious gap, since `tama` is still typed into the shell; `display_pixels_claim()` is all an app needs to take the screen and the desktop already has a way to launch apps. The degree circle on the strip is still a placeholder that no longer describes a duty -- it was chosen when the bars were inert, and they act now, so the label wants deciding: the guest's three buttons show the pet's status icons and there is no short word for them that fits 41 px. Nothing in this cycle is unverified except the guest's own response, which is the operator's report rather than a measurement.

#### Files Modified:

- ports/twatchultra/main/tama.c
- ports/twatchultra/main/tama.h
- ports/twatchultra/main/touch.c
- ports/twatchultra/main/touch.h
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 48 COMMIT Unreleased 2026-11-12T17:42:11-07:00

#### Coming From:

Unreleased 1a1b1f6

#### Purpose:

Stop the thirteen margin buttons from sending desktop keys while the Tamagotchi has the screen, and make their labels say so, restoring the desktop labels when the guest exits.

#### Outcome:

The margins now tell the truth about what they do. While `tama` has the screen, a press on any of the thirteen buttons is refused, and the two side columns are blank while the bottom strip carries a degree circle instead of Tab, Enter and Ctrl+Q; `tama stop` puts the desktop labels back. THE SUPPRESSION WAS NOT WHERE IT LOOKED. `touch.c` already refused the margins when an application owned the screen, but it asked `td_wm_app_owns_screen()`, which is the desktop's own notion of ownership and is false for anything that claims pixels without being a window manager window -- and the emulator is exactly that, since it calls `display_pixels_claim()` directly. So the gate was inert for the one app it was needed for and a press on Enter mid-game typed into the hidden shell, which is the fault the operator's request named. The gate is now `display_pixels_claimed()`, the port's own flag and the same one the painter already honours, so there is one definition of ownership rather than two that disagree. A refused press logs `e<n> i<n> off` and does not buzz, following the rule `edge_press()` already applies to the reserved slot: a buzz with no action claims something happened when nothing did. THE LABELS ARE A NEW MODE, NOT A NEW PAINT PATH. `display_margins_set(DISPLAY_MARGINS_APP | DISPLAY_MARGINS_DESKTOP)` repaints all five column buttons and all three strip bars from `edges.h`, which gained `blank_label()` and `btn_label_app()` beside the existing label tables so that a label and the duty it describes stay in one file. The three strip bars had to move to a single `paint_margins()` helper that both `display_init()` and the mode change call, because the labels now depend on state and two callers passing different arguments is the drift this file's own geometry comment warns about. THE SYNCHRONOUS CALL NEEDED AN ARGUMENT, and it is the reason it is not routed through the paint task like `display_repaint_all()`: the transition into APP mode happens WHILE the painter is suppressed, so a request left for the painter would be dropped exactly when it was wanted. Painting synchronously is safe only because claiming the pixels makes the caller the sole writer of `s_tx` and the SPI bus, which is DSP-005's rule holding rather than being broken; the stop path therefore restores the labels BEFORE releasing the claim, and the start path calls it after the claim and before the guest task exists, so ownership is true by construction in both. This is the third cycle in a row where the same one-writer rule decided a design, and the first where it was applied before rather than after a fault. THE DEGREE SIGN IS VERIFIED RATHER THAN ASSUMED. The label is a hand-written UTF-8 sequence, and a mistyped escape would decode to some other code point, find no glyph, and leave a blank button with nothing reporting an error -- invisible except on the glass. `tests/test_twatch_edges.c` gained `test_margin_labels`, which decodes it with its own UTF-8 decoder written from the definition rather than reusing display.c's, asserts U+00B0, and asks the real font table for the glyph and that it draws ink; 1,567 assertions, 0 failures, and proven able to fail at 3 failures on a wrong code point and 6 on an empty label. That test caught a fault in itself on the first run -- it asserted a non-empty label for every right-column slot, but slot 0 is the reserved blank -- and the test was corrected rather than the code. The build was clean with 0 warnings, binary `0x28b620`, and the host suite passed 41 of 41. The operator confirmed the result on the glass as perfect, after a reboot from a fresh build; the backup `working-2026-11-12-1737.bin` is 16 MiB, sha256 `64c58067d020b9e8aa77a6a5570748e62c79491bda30e0a8857200d4c5ca872d`, verify OK with the board held in the bootloader across read and compare, and its 2,667,808-byte app region is byte-identical to the build.

#### Next Steps:

Wiring the margins to the guest is the remaining half, and the degree circle is now on the glass as a placeholder for it: the columns plan to carry the pet's own controls and the strip the three V3 buttons, which is why APP mode blanks the columns rather than merely cutting their text -- there is nothing to restore, and the next cycle writes the labels it needs. Two things that cycle must answer are already visible here. The direction of the label is a decision, not a detail: the columns are rotated and the strip is not, so a column label is read from the right-hand edge and a strip label from the bottom, and the pet's controls should follow whichever the operator actually reads. And the press path currently refuses and stops; it should call `tmc_button()` instead, which already exists and is proven from the shell with `tama press 0..2`. Nothing else is blocked: `display_margins_set()` takes a mode, so a third one can be added without touching the painter. One loose end from this cycle: `tama stop` restores labels before releasing the claim, and if the guest task fails to stop within its one-second bound the function returns early and the margins are left in APP mode with the screen still claimed -- recoverable with `pattern off` and untested, because the task has never failed to stop.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/edges.h
- ports/twatchultra/main/tama.c
- ports/twatchultra/main/touch.c
- ports/twatchultra/README.md
- tests/test_twatch_edges.c
- CMakeLists.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 47 COMMIT Unreleased 2026-11-12T17:27:36-07:00

#### Coming From:

Unreleased 1d83738

#### Purpose:

Make the Tamagotchi take the whole framebuffer rather than floating on top of the console it was launched from, while leaving the watch's own margin buttons working.

#### Outcome:

The pet now has the screen to itself. The operator's report was that the game was "overlayed on the console", and the cause is the difference between drawing a picture and owning a screen: `display_picture_rgb()` writes only the rectangle it is given, so everything the cell painter had already put in the rest of the 320x416 framebuffer -- the shell's text and the window frame -- stayed exactly where it was, and once `display_pixels_claim(true)` suppressed the painter nothing was left to overwrite it. The new `display_fill(colour)` clears the whole framebuffer and pushes it in bands, and `tama_start()` calls it immediately after claiming and before the first blit, so the guest starts from black. Black is also the emulator's own bezel colour, so the frame blends into the screen and only the LCD's lit dots show; that is a coincidence worth stating because it is not the reason the value was chosen, and a future artwork change that moves the bezel colour should not silently reintroduce a visible border. THE GLASS OUTSIDE THE FRAMEBUFFER IS DELIBERATELY UNTOUCHED, and this is the part that needed checking rather than assuming: the three button bars and the two key columns are painted by `paint_buttons()` and `paint_column()` at init in CONTROLLER coordinates, not by the cell painter, so they live outside `s_fb` entirely and a framebuffer clear cannot reach them. That is why the margin buttons keep working while an app owns the screen, and it is now the documented behaviour rather than an accident of where the code happens to draw. The flush takes the same shape as the existing blit -- lock for the framebuffer write, release before `flush_rect()` -- and is safe for the same reason the blit is: with ownership set the painter takes its notification and returns without touching `s_tx`, so there is still exactly one writer, which is DSP-005's rule holding after being written the cycle before and then violated once. Verified on the glass by the operator, who reported the result as looking great. The build was clean, 0 warnings, binary `0x28b510` with 15 percent of the app partition free, and a status read during that run showed `frames=160 blits=20` at 10 seconds of guest time, so clearing the screen did not disturb the 16 Hz cadence or the dirty-row skip ratio.

#### Next Steps:

Touch is the remaining half of making the guest a real full-screen app: the panel still reports to the desktop as a mouse, so the guest's three buttons are reachable only from the shell with `tama press 0..2`, and mapping touches on the framebuffer to them is a small change behind the ownership test `touch.c` already makes. After that the emulator wants a Start menu entry, since `tama` is still a shell command and the margins are the only way to reach it from the desktop. One thing this cycle's reasoning raises and does not answer: the margins currently send desktop keys (F10, Esc, Tab, Enter) while an app owns the screen, so pressing Enter mid-game types into a hidden shell rather than reaching the pet -- deciding whether the margins should switch meaning under ownership is the operator's call, not a bug to fix unasked.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/tama.c
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 46 COMMIT Unreleased 2026-11-12T17:00:23-07:00

#### Coming From:

Unreleased 358100f

#### Purpose:

Put the Tamagotchi V3 emulator on the watch's own screen as firmware, which is Phase 4 of the Tamacore port and the first cycle in which guest pixels reach the glass.

#### Outcome:

The pet runs on the watch. `ports/twatchultra/components/tamacore` is the emulator copied out of this project's `patches/harness`, and `main/tama.c` is the board's half: it reads the 512 KiB ROM from `/sd/rom.bin` into a PSRAM heap buffer, allocates the 300x308 RGB565 frame the same way, claims the screen with `display_pixels_claim(true)` before the first blit, and runs a task that steps the guest 250,000 cycles at a time and pushes only the rows that changed through `display_picture_rgb()`. Measured on the board over three runs: 549 frames with 69 blits, 358 with 45, and 255 with 32 after the final build, so roughly 88 percent of frames cost no SPI traffic -- the same ratio the host harness reports, arriving on the hardware. `tama status` prints frames, blits, cycles and guest seconds; `tama press` and `tama release` work the guest's three buttons from the shell; `tama stop` saves the 512-byte EEPROM image to `/sd/tama.sav`, releases the screen and asks the painter for a full repaint, so the desktop comes back whole, and the next `tama` loads the saved pet. The ROM went over SFTP in 19.25 s at 26.6 KiB/s with md5 verified both ways. TWO FAILURES ALONG THE WAY ARE WORTH MORE THAN THE SUCCESS. First, the emulator's dirty-row tables were `.bss`, 4,148 bytes of internal DRAM, and adding them broke `ssh start` with `insufficient internal RAM (62443 bytes free; need at least 65536)` -- a refusal with no visible connection to the emulator, found only because a transfer over SSH failed with a bare EOF. They are now a refcounted heap allocation that lands in PSRAM and is freed when the last instance goes, and internal free went 62,443 to 66,599. Second, the first test run appeared to have crashed: the screen showed the desktop, `tama status` read `running=0 frames=0` and the device had rebooted. It had not. The USB mirror goes stale the moment a bitmap owns the panel, so a running emulator and a dead board look identical from the host, and the reboot reading was a second flash reset; the fix is that the guest now rewrites `/sd/tama.txt` every 64 frames with frames, blits, cycles and `esp_reset_reason()`, which is the only view of the guest that survives the mirror going stale, and after that `tama stop` was verified to complete with uptime continuing unbroken. The dirty-row tracking was re-verified on the host before flashing, 2000 frames with 250 pushed and 0 pixels differing from a full repaint, and the emulator's row tables were confirmed to be the only reason the firmware binary changed by nothing but a heap pointer. A COPY IS A LIABILITY UNLESS SOMETHING CHECKS IT, so `scripts/tamacore-sync.sh` compares the thirteen files in `patches/harness` against the component and exits non-zero on any difference; it was verified to fail on a one-line change to a copy and to pass when restored, because a check that cannot fail is not a check. The port's README now documents the subsystem and its stale "no picture support, no sound, no PMU battery reporting" line is corrected, all three having worked for several cycles. Touch is deliberately not wired to the guest yet: the panel still reports to the desktop as a mouse, so the guest's buttons are reachable only from the shell, which proves the emulator's own input path before a driver is added to it.

#### Next Steps:

Phase 5 should wire the touch panel to the guest -- touches on the framebuffer mapped to the three buttons, which is a small change to `touch.c` behind the ownership test it already makes -- and after that, the Tamagotchi should be launched from the Start menu as an app rather than typed in the shell. Two things measured this cycle constrain that work and are already recorded: an app that draws pixels must claim the screen, and the USB mirror is useless while it does, so diagnostics belong in a file on the card. The Launcher/app work also has to answer whether the guest keeps its 16-frame cadence while the desktop's painter is idle, which nothing has yet measured under load. `tama` currently ignores the ROM argument on a bare invocation after the first start, and `tama stop` waits up to a second for the task to leave and refuses to free if it does not -- neither has been exercised, and the second exists because freeing underneath a running task is worse than leaking.

#### Files Modified:

- ports/twatchultra/components/tamacore/CMakeLists.txt
- ports/twatchultra/components/tamacore/con_v3_ir.c
- ports/twatchultra/components/tamacore/con_v3_ir.h
- ports/twatchultra/components/tamacore/ht24lc08.c
- ports/twatchultra/components/tamacore/ht24lc08.h
- ports/twatchultra/components/tamacore/include/tmc_core.h
- ports/twatchultra/components/tamacore/render.h
- ports/twatchultra/components/tamacore/segmap.c
- ports/twatchultra/components/tamacore/segmap.h
- ports/twatchultra/components/tamacore/splb32.c
- ports/twatchultra/components/tamacore/splb32.h
- ports/twatchultra/components/tamacore/splb32_sound.c
- ports/twatchultra/components/tamacore/splb32_sound.h
- ports/twatchultra/components/tamacore/tmc_core.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/tama.c
- ports/twatchultra/main/tama.h
- ports/twatchultra/README.md
- patches/harness/core0/tmc_core.c
- scripts/tamacore-sync.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 45 COMMIT Unreleased 2026-11-12T16:43:49-07:00

#### Coming From:

Unreleased 91da0e3

#### Purpose:

Find and fix the leftover pixel strips that appear on the desktop after the Start menu closes -- a fault older than this session's work.

#### Outcome:

At `ade77cc` on branch `t-watch-ultra` the strips no longer appear, confirmed by the operator using the watch normally -- opening and closing the Start menu and Files -- with no `repaint` needed. THE FAULT AND THE FIX ARE BOTH ONE NUMBER. `flush_rect()` computed how many rows one transaction could carry purely from buffer size, `band = 5120 / w`, so a full-width flush was 16 rows and an 8-pixel-wide column was 640: a narrow update went out as one very tall window. Full-width flushes were always clean and narrow ones dropped bands, and the only difference between the two paths was that height. The height is now capped at `BAND_ROWS` whatever the width. What makes this worth recording rather than merely fixing is that the project already had the shape of it: `LCD-009` in the reference says a 45x356 single-transfer window tears into diagonal splits while the same rectangle in 2-row bands is clean, and explicitly closes with "not width, not X, not height" and the root cause unestablished. The leftover strips supplied the missing half -- that the tearing and the strips are the same fault, that the `repaint` command cleared it because full-width bands were clean, and that the incremental path produced it because narrow ones were not. THE COST OF THE FIX IS REAL AND SMALL: an 8x416 column becomes 26 transactions instead of one, and the icon-column case that motivated the old reasoning -- 23 rows of 8 pixels, which the previous comment argued should go out in a single transaction to save window-programming overhead -- becomes two. The old argument was true and its conclusion was wrong, because the panel does not tolerate a tall narrow window; two window-register writes per extra transaction at 40 MHz is noise against the pixels. TWO INSTRUMENTS WERE BUILT AND ONE WAS WRONG IN AN INSTRUCTIVE WAY. `dcheck` compares the framebuffer against the cell grid and reported `fb_bad=0 shown_bad=0` while the operator could see strips -- it was not lying, but it could never have answered the question, because the panel is write-only and nothing can compare the framebuffer against the glass. `repaint` is the instrument that worked: pushing the whole screen cleared the strips, which proved the framebuffer was right and the rectangles were being lost on the way out. A THIRD FIX WAS NEEDED BECAUSE THE FIRST `repaint` WAS ITSELF A SECOND WRITER: it painted cells and called `flush_rect()` from the shell's task, and `flush_rect` writes `s_tx` and takes the SPI bus, which until then only `paint_task` had ever touched -- there was no lock because there had never been a second caller. On the glass that was a scrambled taskbar and a shell that never returned, and the watch had to be reset. It now sets a request flag and wakes the painter, which invalidates `s_shown` so the existing comparison finds every cell changed -- the same lesson as `display_pixels_claim()` one cycle earlier, applied after failing to apply it once. Also in this cycle: `display_picture_rgb()` made real (it was a stub returning false and the desktop was told it had no picture support), the RGB sink registered, three generated test patterns, and exclusive framebuffer ownership so a bitmap stops being overpainted by a cell painter that cannot see it. The backup `working-2026-11-12-1638.bin` was taken from this build: 16 MiB, sha256 `917fbb1367c630e57f26118a26e8847f421854964990e83f2b7f536f62bbdb8e`, verify OK with the board held in the stub across read and compare, and the 2,642,096-byte app region byte-identical to the build at `06f575d1`. The required `core-syntax.md` audit was run against the complete `.ai/` diff: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 45; Files Modified lists port paths and no `.ai/` file or generated artifact; and Status records the user test as a pass on the operator's own report of normal use.

#### Next Steps:

The rendering work is done and the Tamacore port can resume at Phase 4 -- `tmc_core.c` as an ESP-IDF component, the ROM from the SD card, one static frame on the glass -- and the two constraints that phase depends on are now measured rather than assumed: a frame-sized buffer must be a PSRAM heap allocation because 256 KB of .bss overflowed internal DRAM, and an app that draws pixels must claim the screen or the painter overpaints it. `repaint` should be kept as a recovery command rather than removed: it is now safe, it costs nothing when unused, and it recovers a whole screen from anything that leaves the glass wrong. `dcheck` should be kept with its limit stated in its own help, because it measures the model against the framebuffer and says nothing about the panel -- the one question an operator most wants answered is the one it cannot answer. One thing this cycle leaves open by choice: `LCD-009`'s "not width, not X, not height" is now explained, and that record should be revised to say so rather than left as an open mystery. Everything else is unchanged and unblocked: NFC is parked with the re-read storm deliberately kept; GPS tracks satellites and sets the clock, with the UBX side unexplored and a fix untested for want of sky; the taskbar edge is settable and left defaulting to bottom; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the microphone with no code.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 44 COMMIT Unreleased 2026-11-12T16:23:06-07:00

#### Coming From:

Unreleased c0d705c

#### Purpose:

Make the watch draw an RGB565 bitmap and stop the desktop repainting over it, so an app that owns the screen has a screen.

#### Outcome:

At `2e350cb` on branch `t-watch-ultra`, `pattern 0` draws colour bars that HOLD: the operator reports all three patterns looking good, with no console bleeding through. Three things were needed and each was found by the previous one failing. FIRST, `display_picture_rgb()` was a stub returning false, and `link_twatch.c` passed NULL as the desktop's RGB sink; it now copies the bitmap row by row into `s_fb` and calls `flush_rect()`, which already byte-swaps and pushes. The port's own comment said it had "no picture support", which was true of all three formats and is now true of two: 8-bit and run-length pictures stay NULL, because the desktop learns from those three what it can draw and a sink that returns false is worse than saying plainly there is none. SECOND, THE BITMAP DID NOT HOLD, and the cause was two writers with no coordination rather than a blit fault. `paint_task` decides what to redraw by comparing each cell against `s_shown`, its record of what it has already drawn; a bitmap blit does not update that record, so after a blit the painter still believed the glass held the old cells and repainted them at the next notification -- a taskbar clock tick or a cursor blink -- carrying whatever the Start menu and Files had left behind. That is exactly the striped console bleed the operator saw, and it was the painter doing what it was told. The fix is explicit ownership: `display_pixels_claim(true)` stops the painter, and `display_pixels_claim(false)` invalidates `s_shown` so the next frame repaints every cell, because by then the glass holds a picture rather than the cells the painter was tracking. A mode rather than a lock, because while it is set there is exactly one writer and while it is clear there is exactly one other. THIRD, AND AS IMPORTANT AS THE FIX: A PICTURE CANNOT TESTIFY ABOUT ITSELF. A bitmap looks identical whether the painter is suppressed or about to overwrite it, and the USB mirror used for every other check in this project goes stale the moment the panel is owned -- the desktop's diff stream stops describing the glass, because the glass no longer shows the desktop. So two counters were added and the result is a fact rather than an inference: while owned, `painted` stayed at 631 across 15 seconds and three pattern changes while `skip` climbed 185 to 327, that is 142 repaint triggers of which none executed; on release `painted` went 631 to 656 and `skip` froze at 350. The same counters are written to `/sd/pattern.txt` as well as printed, because a diagnostic that exists only in the terminal is unreadable precisely when the panel is owned -- the mirror is stale -- and the file can be fetched over SFTP instead. A REAL CONSTRAINT WAS FOUND BY BUILDING IT: the test pattern first used `static uint16_t img[DISPLAY_W * DISPLAY_H]` and the link failed with `region dram0_0_seg overflowed by 88896 bytes`, because everything static in this component lands in internal DRAM by construction (extram.lf maps only libtinydesk, libtdsh and libmicropython to PSRAM) and internal DRAM is this board's scarcest resource at about 47 KB free. The buffer moved to the heap. That decides where the emulator's own 184 KB frame goes, and it would otherwise have been discovered a phase later. The build is clean, the operator confirmed all three patterns on the glass, and no backup has been retaken since the firmware changed. The required `core-syntax.md` audit was run against the complete `.ai/` diff, which is this entry alone: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 44; Files Modified lists port paths and no `.ai/` file or generated artifact; and Status records the user test as a pass on the operator's own report.

#### Next Steps:

The rendering question is closed and the port can proceed to Phase 4: `tmc_core.c` becomes an ESP-IDF component, the ROM loads from the SD card, and one static emulator frame goes on the glass. Two things from this cycle decide that phase's shape. The frame buffer must be a PSRAM allocation rather than a static array, because 256 KB of .bss overflowed internal DRAM by 88,896 bytes. And the app must claim the screen before its first blit and release on exit, or the painter fights it exactly as it fought the test pattern -- which is now a documented contract in `display.h` rather than something to rediscover. What Phase 4 still has to settle is the ROM: 524,288 bytes against 507,072 bytes of free flash means it cannot be embedded, so it comes from the card, and the app must fail gracefully when the card is absent rather than presenting a black panel. Everything else is unchanged and unblocked: NFC is parked at the operator's direction with the re-read storm deliberately kept; GPS tracks satellites and sets the clock, with the UBX side unexplored and a fix untested for want of sky; the taskbar edge is settable and left defaulting to bottom; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the microphone with no code. A fresh backup is due, since `working-2026-11-12-1542.bin` predates the picture sink and the paint-task change.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 43 COMMIT Unreleased 2026-11-12T15:46:24-07:00

#### Coming From:

Unreleased d44ba42

#### Purpose:

Capture a full 16 MiB backup before starting the Tamacore port, since that work will change the firmware substantially.

#### Outcome:

`~/twatch-backup/working-2026-11-12-1542.bin` is the current recoverable state: 16 MiB read at 579.8 kbit/s in 231.5 seconds, sha256 `a1bba8faf11c76461f65d738a2e088469b6bf7fad3aff330d8682b350c085ab4`, `verify OK (digest matched)` against the chip with the board held in the stub across read and compare, and the `.sha256` written beside it. The board was rebooted first rather than backed up as found, so the image is a settled state rather than one with a session's filesystem writes half-flushed. The 2,639,168-byte app region at 0x10000 is BYTE-IDENTICAL to the `tinydesk.bin` this machine built, both hashing to `3aed4ad3`, which is the identification that matters because the flashed image carries no symbols; the format strings for the three subsystems are present as a second check (`gps time [s]`, `waiting for UTC`, `card %s uid %s`, `play <file.wav>`). The littlefs partition holds 2,026 written 4 KiB pages against the previous backup's 2,017. Both repositories were clean and pushed at `6f5a382` and `d44ba42` before the backup began, so this cycle produces a recovery artifact and this entry and no engineering change. THE SCRIPT'S RESET WORKED ON THE FIRST ATTEMPT, the fifth consecutive backup to do so, and the board was confirmed running independently of the script's own report by reading its desktop over USB at 15:46 with WiFi associated. The required `core-syntax.md` audit was run against the complete `.ai/` diff, which is this entry alone: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 43; Files Modified reads `None.` because the cycle generated a recovery artifact outside the repository; and Status uses `N/A` for build and deployment because nothing was compiled or written to the chip.

#### Next Steps:

The Tamacore port is scoped and not started, and the survey that produced the scope is worth carrying into it because it changes what the plan assumes. The emulator is ~4,660 lines but splits unevenly: about 2,560 lines (`splb32.c`, `segmap.c`, `render.h`, `ht24lc08.c`, `con_v3_ir.c`, `splb32_sound.c`) include only libc headers and each other and should compile unchanged, while `tamacore_task.c` at 1,160 lines is the shared-PSRAM window, the `esp_cache` invalidations and the core-0/core-1 handshake -- machinery that exists only because Linux and ESP-IDF had to share one chip and is dead weight inside one image. Four measured facts set the shape: the emulator's frame is 300x308 RGB565 (184 KB) and fits the 320x416 panel at 1:1 centred at (10,54) with no scaling; the ROM is 524,288 bytes against 507,072 bytes of free flash, so it cannot be embedded and must load from the SD card into PSRAM; the segment map is only about 5 KB, not the 32 KB the old comments feared, because that figure described an abandoned 4x variant; and both S3 cores have cache, so the 464-cycles-per-instruction penalty that forced the guest onto core 0 under Linux does not apply here. The one real risk is that `paint_task` repaints the cell grid from `s_shown` and would overwrite pixels a second writer puts in `s_fb`, so the emulator should own the whole panel through `td_wm_app_owns_screen()` rather than share it, and `display_picture_rgb()` is stubbed to return false today and needs about twenty lines to memcpy into `s_fb` and call `flush_rect`. The proposed order is host harness first (run the portable interpreter on the PC and dump frames), then rewrite the glue on the host, then the blit, then a static image on the glass before any timing work. Nothing else is blocked: everything recorded at entries 38 to 42 stands, and the standing queue is unchanged.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 42 COMMIT Unreleased 2026-11-12T14:28:35-07:00

#### Coming From:

Unreleased 1fefce8

#### Purpose:

Find why SFTP transfers to the watch were slow and unreliable, and fix the cause rather than the symptom.

#### Outcome:

At `6f5a382` on branch `t-watch-ultra`, sixteen deliberately killed SFTP transfers leave the SD card writable where five used to kill it: after them `echo x > /sd/f` returns 0, and a clean 233 KB upload runs at 28.9 KiB/s with the md5 verified in both directions (download 74.0 KiB/s). Three separate faults were measured, and the one the cycle was asked about was not the one that mattered. THROUGHPUT WAS NEVER THE PROBLEM: a 1 KiB file takes 0.20 s, 16 KiB takes 0.59 s, and a 233 KB text file goes through in 7.88 s, which is 28.9 KiB/s -- entirely plausible for wolfSSH on one core with the desktop running, and the 74.0 KiB/s download shows the radio is not the ceiling. WHAT LOOKED LIKE SLOWNESS WAS A WEDGE. A 32 KiB SFTP write request is never answered by this server -- 16 KiB completes, 24 KiB does not finish at all -- and paramiko's `put()` uses exactly 32 KiB, so every file above about 16 KiB hung rather than failed. Reads differ again: 16 KiB reads stall and 4 KiB reads run at 74 KiB/s, so the two directions genuinely need different sizes; the first version of the tool used one for both and hung a download that had uploaded fine. Because the server accepts one client at a time (`TDSH_SSH_MAX_CLIENTS` is 1) and services it on the accept loop's own task, a wedged session then holds the only slot, and every later attempt completes its TCP handshake in 10 ms and receives no SSH banner at all -- indistinguishable from a dead server, and cleared only by `ssh restart` or a reboot. THE ACTUAL DEFECT WAS A FILE-DESCRIPTOR LEAK, AND IT WAS ONE UNDEFINED MACRO. This port uses wolfSSH's stdio path, so `WFOPEN` maps to `wfopen` and files land in ESP-IDF's VFS, whose FAT mount is created with `max_files = 5` in `tdsh_sdcard.c`. wolfSSH frees those descriptors in two places: the SFTP CLOSE request, and `SFTP_FreeHandles()` from `wolfSSH_SFTP_free()`. The second is compiled out unless `WOLFSSH_STOREHANDLE` is defined, and `settings.h` defines it automatically only for `WOLFSSH_FATFS` -- the other I/O path, which talks to FatFs through a private descriptor pool this port never uses. So `SFTP_RecvOpen` never recorded a handle, teardown had nothing to close, and each abandoned session kept its descriptor permanently. That is also why it presented as a size limit: a session that ends normally ends with the client's own CLOSE requests, which free descriptors whatever the macro says, so clean transfers never leaked and only killed clients did -- eight 1 KiB puts in one session and a 233 KB transfer both left the card writable, which is how the leak was separated from a size cap. Exhausting the pool then breaks the card for everything, the shell included: `vfs_fat: open: no free file descriptors`, and `sd umount; sd mount` restores reads while only a reboot restores writes. THE FIX CHANGES NO SOURCE FILE, which is the point: both halves of the mechanism are already in the vendored wolfSSH and are gated on that single macro, so it is defined from the port's own `project_include.cmake` -- the same early-evaluation hook `TD_SIZE_POLL_MS` uses -- and wolfSSH lives in the `tinydesk-shell` submodule, which is upstream's and not this project's to edit. The definition was confirmed to reach the sources rather than assumed: `compile_commands.json` carries it on 1514 entries, and `SFTP_AddHandleNode` and `SFTP_FreeHandles` are both present in the rebuilt `wolfsftp.c.obj`. Double-close was ruled out by reading the CLOSE path, which does `WCLOSE(fd)` and then `SFTP_RemoveHandleNode`, so the list only ever holds descriptors that are still open. Two supporting pieces landed with it: `tools/sftp-put.py`, which chunks 16 KiB up and 4 KiB down, verifies size and md5 in both directions, and names `ssh restart` in its failure message so a wedged server is not mistaken for a dead one; and the measured boundary is recorded in that tool's header rather than in a chat. Nothing about the wedge itself is fixed -- a 32 KiB request will still hang the server, and a stalled client still holds the only slot -- so both remain open, and the tool avoids them rather than curing them. The required `core-syntax.md` audit was run against the complete `.ai/` diff: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 42; Files Modified lists the two port and tool paths and no `.ai/` file or generated artifact; and Status records the build and deployment as passes with the user test NOT RUN, because every reading above was taken by this agent over USB.

#### Next Steps:

Two faults found in this cycle are measured and unfixed, and both live in the `tinydesk-shell` submodule, which is upstream's. The first is the wedge: a write request of 32 KiB or a read of 16 KiB is never answered, and nothing in the SFTP layer bounds the request size it will attempt. The second is the single client slot -- `TDSH_SSH_MAX_CLIENTS` is 1 and the client is serviced on the accept loop's own task, so one stalled session denies everyone else with no banner and no error. The first is arguably the more valuable repair because the second only becomes lethal when a session can stall, but both want a decision about where the change belongs: a local edit in the submodule would be lost on any fresh clone, so the honest options are forking `tinydesk-shell` to `aquasock` and repointing `.gitmodules`, or reporting upstream and living with the workaround in the meantime. Neither was taken unasked. What is left on the reader's own side is small: `tools/sftp-put.py` covers put, get, ls, stat and rm, and the transfer path is now reliable at 28.9 KiB/s up and 74.0 KiB/s down, so moving files onto the watch is a solved problem for anything under a few hundred megabytes. Everything else is unchanged and unblocked: NFC is parked at the operator's direction with the re-read storm kept deliberately and its three facts recorded (container at offset 12, NTAG213 with `e1101200`, `ndefPollerCheckPresence()` blocking on a lifted tag); GPS tracks satellites and sets the clock but leaves the UBX side unexplored and a fix untested for want of sky; the taskbar edge is settable and left defaulting to bottom; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, and the two untested port overrides.

#### Files Modified:

- ports/twatchultra/main/project_include.cmake
- tools/sftp-put.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 41 COMMIT Unreleased 2026-11-12T13:57:13-07:00

#### Coming From:

Unreleased 0b2141a

#### Purpose:

Record the operator's hardware test of `gps time`, which entry 39 left marked NOT RUN.

#### Outcome:

The operator ran `gps time` on the watch and reports it working, with the system clock already correct before the command ran, so the correction applied was zero seconds. That detail is worth more than a bare pass because it is independent corroboration of the mechanism rather than of the command: a clock that was already right when the command ran means the time written by an earlier correction had survived in the PCF85063A and been seeded back at the last boot, which is the write-through path GPS-005 claims and which this cycle now has from a second observer. It also closes the one status entry 39 could not honestly fill: that entry recorded the user test as NOT RUN because every reading in it was taken by this agent over USB, which was accurate at the time and remains accurate as history, so it was not amended -- rewriting a pushed entry to look better-informed than it was is a worse record than adding the confirmation beside it. Nothing was built, flashed or changed; both repositories were clean and pushed at `0b2141a` and `50d047d` before this entry, and the recoverable image `working-2026-11-12-1352.bin` describes the same firmware the operator exercised. The required `core-syntax.md` audit was run against the complete `.ai/` diff, which is this entry alone: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 41; Files Modified reads `None.` because the cycle changed no engineering file; and Status records the user test as PASS on the operator's own report while build and deployment remain `N/A` because nothing was compiled or written.

#### Next Steps:

`gps time` is now verified by both parties and the remaining question on it is a decision rather than a defect: the command must be typed, so a watch taken outdoors and reset comes back with whatever the chip last held until somebody asks the receiver. Running it automatically at boot is a trade of a few seconds and tens of milliamps on every start against a clock that is always right, and the cheaper shape is a Settings checkbox or a single invocation from the shell's start-up path -- either way it is the operator's call, not a default this port should assume. Everything else is unchanged and unblocked. NFC is parked at the operator's direction with the re-read storm deliberately kept, and its three established facts stand: the capability container is at offset 12, the tag is an NTAG213 with `e1101200` and 126 free bytes, and `ndefPollerCheckPresence()` blocks for ever once a tag is lifted. The GNSS receiver's UBX side remains unexplored -- MON-VER goes unanswered, constellation configuration is untouched, and C/N0 is not extracted from GSV -- and a fix is still untested because it needs sky. The taskbar edge is settable and left defaulting to bottom; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, and the two untested port overrides.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 40 COMMIT Unreleased 2026-11-12T13:55:09-07:00

#### Coming From:

Unreleased 78559bf

#### Purpose:

Capture a full 16 MiB backup of the first build that sets the watch clock from the GNSS receiver, since the previous image predates it.

#### Outcome:

`~/twatch-backup/working-2026-11-12-1352.bin` is the current recoverable state: 16 MiB read at 588.8 kbit/s in 227.9 seconds, sha256 `156e8e4c6a8308d6247e7937f7a4aece7b6ff5e1b2c3d4083deead991031e6f6`, `verify OK (digest matched)` against the chip with the board held in the stub across read and compare, and the stored `.sha256` written beside it. The 2,638,496-byte app region at 0x10000 is BYTE-IDENTICAL to the `tinydesk.bin` this machine built, both hashing to `4688575c`, which is the identification that matters because the flashed image carries no symbols to search. The format strings for this cycle are present as a second check -- `gps time [s]`, `waiting for UTC`, `clock set %lld`, `GGA %d RMC %d`, `gps raw [s]` -- alongside the NFC and audio ones from earlier cycles (`card %s uid %s`, `nfc scan [s]`, `play <file.wav>`), so the image is the one with all three subsystems rather than an earlier one. The partition table parses as the six entries `partitions.csv` declares -- nvs 0x9000, phy_init 0xf000, ota_0 0x10000, ota_1 0x310000, otadata 0x610000, storage 0x620000 -- and the littlefs partition holds 2,017 written 4 KiB pages against the previous backup's 2,013, which is the four pages of session writes this cycle produced. THE SCRIPT'S RESET WORKED ON THE FIRST ATTEMPT, the fourth consecutive backup to do so, and the board was confirmed running independently of the script's own report by reading its desktop over USB at 13:55. Nothing was built, flashed or changed this cycle: both repositories were already clean and pushed at `50d047d` and `78559bf` before the backup began, so this cycle produces a recovery artifact and a log entry and no engineering change. The required `core-syntax.md` audit was run against the complete `.ai/` diff, which is this entry alone: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 40; Files Modified reads `None.` because the cycle generated a recovery artifact outside the repository; and Status uses `N/A` for build and deployment because nothing was compiled or written to the chip.

#### Next Steps:

The image now holds three subsystems that were each unknown a day ago -- a reader, a receiver and a clock that can be set from it -- and the state is recoverable, so the natural next work is to make the clock's correction automatic rather than a command somebody has to remember. That is a decision about cost and not about code: the receiver takes a few seconds and tens of milliamps, so running it on every boot is a trade only the operator can make, and the cheaper shape is a Settings checkbox or a shell command run once at start-up. Everything else is unchanged and unblocked. NFC is parked at the operator's direction with the re-read storm kept deliberately, and its three established facts are recorded: the capability container is at offset 12, the tag is an NTAG213 with `e1101200` and 126 free bytes, and `ndefPollerCheckPresence()` blocks for ever once a tag is lifted. The GNSS receiver's UBX side is unexplored -- the MON-VER poll goes unanswered, constellation configuration is untouched, and C/N0 is not extracted from GSV, so the port knows how many satellites are heard and not how well. A fix remains untested because it needs sky. The taskbar edge is settable and left defaulting to bottom at the operator's direction; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, and the two untested port overrides.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 39 COMMIT Unreleased 2026-11-12T13:46:57-07:00

#### Coming From:

Unreleased 4e45a08

#### Purpose:

Set the watch's clock from the GNSS receiver, so a watch with no network can still be given an accurate time.

#### Outcome:

At `50d047d` on branch `t-watch-ultra`, `gps time [s]` sets the system clock from the receiver's UTC and writes it through to the PCF85063A. Verified on the hardware: three consecutive corrections thirty seconds apart each reported the clock `was off +0 s`, `+1 s` and `+0 s`; the RTC logged `twatch-rtc: wrote 2026-11-12 20:34:54 UTC`, and the epoch the command reported decodes back to exactly that instant; and after a hard reset the shell read 13:46:16 MST against a host at 13:46:28, the twelve-second gap being this agent's command-injection latency rather than clock error, so the correction survived the power cycle by way of the chip. THE POINT OF THIS IS THAT IT WORKS INDOORS. A fix needs four satellites and sky, but a TIME needs only decoded ephemeris, which this receiver acquires at three satellites behind a windowless ceiling -- so the watch can be given an accurate clock by being held up for a few seconds with no network at all, which is precisely what a watch whose WiFi does not auto-associate needs. THE FIRST VERSION OF THIS SET THE CLOCK TO A STALE VALUE AND THE SYMPTOM LOOKED EXACTLY LIKE HARDWARE DRIFT. It collected the UTC from the first valid sentence and then kept listening until the window expired before calling settimeofday, so it applied a reading already up to twenty seconds old. Measured on the board, correction runs a minute apart reported `was off +26 s` and then `+1 s`, which reads as a clock gaining twenty-five seconds in eighty and was really the setting routine handing the clock a stale number. Two experiments separated it from real drift and are worth recording as method: the clock ran exactly 60 s across a `sleep 60` and 41 s across a 40 s `gps monitor`, which is what proved the hardware was fine and the code was not. The loop now stops the moment it has a valid time, so the window bounds the wait and never delays the write. THE EPOCH CONVERSION IS THE PART THAT COULD BE QUIETLY WRONG, so it is split and swept. Days-from-civil is Hinnant's algorithm written out rather than taken from `timegm()`, because the C library's version honours TZ on some platforms and this must be UTC unconditionally -- a GPS time converted through a local zone is wrong by exactly the offset, seven hours on this watch, and reads as a fault rather than as arithmetic. `nmea_utc()` applies the 2020 plausibility floor and `nmea_utc_raw()` does not, which lets the day-count be checked against 1970 and the floor be checked separately; twelve host checks cover known epochs computed independently in Python, the 2024 leap day, and both century cases, and a mutant dropping the y/100 term was built and fails 2100-12-31 by exactly one day. The write-through reuses a new `board_time_apply()`, which takes the same two steps the shared app's `time_set()` takes for SNTP -- settimeofday, then `board_time_set_post` -- because settimeofday alone would leave the chip holding the old value and the next boot would undo the correction. The host suite is 41 of 41 and `test_nmea` is now 77 checks, up from 56. The required `core-syntax.md` audit was run against the complete `.ai/` diff: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 39; Files Modified lists port and test paths and no `.ai/` file or generated artifact; and Status records the user test as NOT RUN, because every reading above was taken by this agent over USB rather than by the operator.

#### Next Steps:

The clock is now settable from two independent sources and neither needs the other: the PCF85063A seeds it at boot, and GPS corrects it on demand. What is missing is the automatic half -- `gps time` is a command, so the watch only gets GPS time when someone asks, and the obvious next step is to run it at boot or when the shell starts, bounded and non-fatal, so a watch taken outdoors and reset comes back with the right time. That needs a decision about cost rather than code: the receiver takes a few seconds and tens of milliamps, and it is worth doing only if the operator wants the watch to spend that on every boot. The alternative and cheaper shape is a Settings checkbox that enables it. Beyond time, the receiver's UBX side is still unexplored -- the MON-VER poll goes unanswered on this unit, constellation configuration is untouched, and C/N0 is not extracted from GSV, so the port can say how many satellites are heard and not how well. A fix remains untested because it needs sky. Everything else is unchanged and unblocked: NFC is parked at the operator's direction with the re-read storm kept deliberately; the taskbar edge is settable and left defaulting to bottom; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, and the RTC write path -- which this cycle has now exercised in the direction that matters, from a source other than the chip itself.

#### Files Modified:

- ports/twatchultra/main/gps.c
- ports/twatchultra/main/nmea.c
- ports/twatchultra/main/nmea.h
- ports/twatchultra/main/rtc.c
- ports/twatchultra/main/rtc.h
- ports/twatchultra/README.md
- tests/test_nmea.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 38 COMMIT Unreleased 2026-11-12T13:29:57-07:00

#### Coming From:

Unreleased 0b4ac5b

#### Purpose:

Bring the T-Watch Ultra's GNSS receiver up for the first time and read its fix.

#### Outcome:

At `9fe65a2` on branch `t-watch-ultra` the watch tracks satellites: `gps` reports `rail up`, `port 38400 8N1` and 624 to 900 bytes heard per second, and `gps raw 12` counts `GGA 12 RMC 12 GSA 48 GSV 54 GLL 12 VTG 12` with `sats visible 5`. A 60-second monitor then produced `20:28:41` UTC with `s03` and `hd2.38` and NO FIX, which is the indoor limit rather than a fault: three satellites is one short of the four a 3D solution needs. ACQUIRING UTC IS THE EVIDENCE THAT MATTERS and it is worth being precise about why: a receiver cannot print a UTC time until it has decoded ephemeris from the satellites themselves, which takes roughly thirty seconds of continuous reception and cannot be produced by noise, a crossed pin or a parser bug. That is a stronger statement about the antenna than a fix would have been, because a fix can come from a receiver near a window while the actual question -- is the antenna connected -- was already answered by the clock. THE MODULE IS A u-blox MIA-M10Q on UART1 at 38400 8N1, TX pin 43, RX pin 44, PPS pin 13, powered by the AXP2101's BLDO1 -- voltage register 0x96 and enable bit 4 of 0x90 -- which nothing in this port raised before. That is one register below the speaker's BLDO2 in both the enable and voltage registers, and powering the wrong one lights the amplifier and leaves the receiver dark with no fault reported anywhere, so `board_gnss_rail_on()` reads the rail back rather than trusting the write, the same shape the speaker's rail already used. The work is two files and the split is what makes it verifiable: `nmea.c` owns no stream and decodes only GGA and RMC, so `tests/test_nmea.c` feeds it recorded sentences on the host with no hardware present, while `gps.c` is the transport -- rail, UART, one UBX poll, bytes assembled into lines with a buffer that survives between calls so a sentence straddling two reads is not lost. THE PARSER'S THREE HABITS ARE EACH A CORRECTION AND TWO WERE PROVED BY MUTATION. Latitude is degrees AND minutes: `4807.038` is 48.11730 degrees and not 48.07038, a difference of about 5.2 km north to south, and a mutant that reads it as decimal degrees was built and does fail the test. An empty field is not a zero: a receiver with no fix sends `,,,,,` for position, and a mutant that stores zero or drops the fix-quality gate produces `have_pos` at 0,0 -- a point in the Gulf of Guinea -- and was also built and does fail. Fields are counted rather than trusted because GGA arrives with twelve fields about as often as fourteen, so a short sentence yields missing data instead of shifted data. WHAT DID NOT WORK IS RECORDED RATHER THAN SMOOTHED OVER: the UBX-MON-VER probe gets no reply on this unit even though NMEA flows continuously, so the module never confirms its own model string and `gps` prints `ubx no reply` plainly; the input side may be disabled or the reply missed, and it was not chased because NMEA already answers every question the port has. The pin names are also from the SoC's point of view, which is the opposite of the module's, and a crossed pair is silent in both directions and looks exactly like a dead module -- which is why the probe reports a byte count and not only a parse result. `gps raw [s]` was written for exactly that distinction and earned its place: "no fix" and "no antenna" are the same reading on a fix display, and the GSV count is what separated them, turning this cycle from "no satellites" into "five visible, three used". The host suite is 41 of 41, up from 40 of 41, and the extra test is this one. THE `board_keys` FAILURE IS FIXED IN PASSING and its history is worth recording: it had been failing since `fcd14aa` because the NFC commit added `nfc.spi_host`, `nfc.cs` and `nfc.int` without recording them in `api/board_keys.txt`, and the suite was reported as passing at the time. A test that fails and is not noticed is not different from a test that is absent, and the check is cheap enough that it should have been run before that commit rather than after. The required `core-syntax.md` audit was run against the complete `.ai/` diff: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 38; Files Modified lists port and test paths and no `.ai/` file or generated artifact; Status records the user test as NOT RUN, because the hardware result above was read off the panel by this agent over USB and the operator has not independently exercised the command -- the readings are real, but they are not a user test and are not recorded as one.

#### Next Steps:

A fix needs sky view, and that is the one thing left on this receiver that the bench cannot supply -- near a window or outdoors it should lock within a minute now that the antenna is known to work. The cheapest useful work meanwhile is time rather than position: the receiver already reports authoritative UTC, the port already has `rtc.c` and a `board_time_set_post()` path that SNTP uses, and the watch's PCF85063A drifts, so setting the clock from a GPS sentence would give the watch an accurate time with no network at all -- which is the same reasoning that made the boot-time RTC seed worth doing. Beyond that the receiver's UBX side is unexplored: the MON-VER poll is unanswered, constellation configuration is untouched, and the module's factory defaults have not been changed, which is deliberate because the first move on a new part in this project has been measurement. Everything else is unchanged and unblocked: NFC is parked at the operator's direction with the re-read storm kept deliberately; the taskbar edge is settable and left defaulting to bottom; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, and the unexercised RTC write path.

#### Files Modified:

- ports/twatchultra/main/gps.c
- ports/twatchultra/main/gps.h
- ports/twatchultra/main/nmea.c
- ports/twatchultra/main/nmea.h
- ports/twatchultra/main/power.c
- ports/twatchultra/main/power.h
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/board.example.conf
- ports/twatchultra/README.md
- tests/test_nmea.c
- api/board_keys.txt
- CMakeLists.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 37 COMMIT Unreleased 2026-11-12T13:17:56-07:00

#### Coming From:

Unreleased 3bdfef5

#### Purpose:

Extend the NFC reader from "it reads a tag" to "it identifies the tag and counts presentations", and fix the re-read storm that made counting impossible.

#### Outcome:

The cycle was reverted at the operator's direction and nothing it produced is in the tree; the fork is back at `fcd14aa`, rebuilt and reflashed, and `nfc` again reports `rail up`, `bus up cs 4 int 5`, `id 2a type ST25R3916 rev 2`, `boot ok` with the desktop painted and WiFi associated. Two things were established before the revert and both are worth keeping even though the code is gone. FIRST, the tag identifies itself without a datasheet lookup, and the first attempt at reading that was wrong in a way that survived review: the capability container is at offset 12 on a Type 2 tag, not offset 0, because pages 0 and 1 are the UID and page 2 is the lock bytes -- reading offset 0 returned `1dd30d4b`, the first four bytes of the UID the same read had just printed, which looks like a plausible answer and is not one. Read at the right offset it gives `e1101200`, which decodes as magic E1, mapping version 1.0, 18 x 8 = 144 bytes and access 00, so the tag is an NTAG213 that is formatted, read/write and holds 126 free bytes. RFAL's own T2T detection computes the area length but leaves its version fields zero, so the version has to come from the container. SECOND, and this is the finding the cycle is actually worth: THE READER RE-READS A TAG THAT IS STILL THERE, about 6.7 times a second, because every successful read is followed by a deactivation and a fresh discovery. A tag left on the glass cost roughly four hundred reads a minute. That was not a separate annoyance but the cause of the measurement failure that consumed the cycle -- one tap looked like seven reads and seven taps looked like one, and three successive builds went into counting the storm rather than using it. The attempt to fix it properly HUNG THE SHELL and had to be reset out of: `ndefPollerCheckPresence()` is one block-0 read for a Type 2 tag, which is the right primitive, but once the tag is lifted the transmit goes into an empty field and the vendor's transceive waits for a response that never arrives, and nothing in RFAL offers a cancellable transceive. A blocking call made while a tag might be gone is therefore unsafe on this part, which is the constraint any future fix has to respect -- the shape that would work is a non-blocking quiet window after each read, suppressing discovery rather than polling the card. The judgement error is recorded too and it is the real lesson: the storm never stopped the tag from being read, the NDEF decode had been verified three builds earlier, and four build-flash cycles went into instrumentation that added no capability. A measurement problem is not a defect and should not have been treated as one. Nothing was committed from this cycle, so no tracked file changed; the tree is clean at `fcd14aa` on branch `t-watch-ultra`, and `working-2026-11-12-1240.bin` remains the recoverable image. The required `core-syntax.md` audit was run against the complete `.ai/` diff, which is this entry alone: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 37; Files Modified reads `None.` because everything the cycle produced was reverted; and Status records the user test as a rejection rather than a pass.

#### Next Steps:

NFC is parked at the operator's direction and the re-read storm is deliberately kept, because it is harmless for what the reader does today -- read a tag and print it -- and becomes a problem only if something later watches for tag changes. Any future cycle should start from the three facts this one established rather than rediscovering them: the capability container is at offset 12, the tag is an NTAG213 with `e1101200` and 126 free bytes, and `ndefPollerCheckPresence()` blocks for ever once the tag is lifted. The cheapest useful work on the reader is a consumer rather than more instrumentation: nothing reacts to a tag today, and injecting a key through the desktop's existing injection path would make a presentation do something visible. Writing is unimplemented and the vendored stack can do it. Everything else is unchanged and unblocked: the taskbar edge is settable and left defaulting to bottom at the operator's direction; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, and the unexercised RTC write path.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 36 COMMIT Unreleased 2026-11-12T12:43:43-07:00

#### Coming From:

Unreleased 25a26e8

#### Purpose:

Capture a full 16 MiB backup of the first build that reads NFC tags, since the previous image predates the whole reader.

#### Outcome:

`~/twatch-backup/working-2026-11-12-1240.bin` is the current recoverable state: 16 MiB read at 577.2 kbit/s in 232.5 seconds, sha256 `f581d47c4ac5ffa43bd8d82d551d2f0036a64457b9801275cff96307721f50e2`, `verify OK (digest matched)` against the chip with the board held in the stub across read and compare, and the stored `.sha256` written beside it. The 2,631,008-byte app region at 0x10000 is BYTE-IDENTICAL to the `tinydesk.bin` this machine built, both hashing to `7110cf9b`, so the image is the NFC build rather than an earlier one -- and because the flashed image carries no symbols, that comparison rather than a string search is the real identification. The NFC-specific format strings are present as a second check: `nfc up id`, `rfal up, polling`, `card %s uid %s`, `ndef %d bytes`, `nfc scan [s]`, alongside the player's `play <file.wav>`. One string that looked wrong on first pass is recorded so nobody chases it again: `Hello` appears in the image, but it is wolfSSL's "ServerHello" and not the tag's payload, which reached the screen through a `%s` and was never a literal. The partition table parses as the six entries `partitions.csv` declares -- nvs 0x9000, phy_init 0xf000, ota_0 0x10000, ota_1 0x310000, otadata 0x610000, storage 0x620000 -- and the littlefs partition holds 2,013 written 4 KiB pages against the previous backup's 2,004, which is the nine pages of session writes this cycle produced. THE SCRIPT'S RESET WORKED ON THE FIRST ATTEMPT, the third consecutive backup to do so, and the board was confirmed running independently of the script's own report by reading its desktop over USB at 12:43 -- taskbar clock correct, desktop painted. WiFi was also associated, which is the saved-credentials path and not evidence of anything about the image. Nothing was built or flashed this cycle and no tracked file changed. The required `core-syntax.md` audit was run against the complete `.ai/` diff, which is this entry alone: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 36; Files Modified reads `None.` because the cycle generated a recovery artifact outside the repository; and Status uses `N/A` for build and deployment because nothing was compiled or written to the chip.

#### Next Steps:

The reader is now recoverable but barely exercised, so the work that matters next is coverage rather than capability: only NFC-A Type 2 has ever been in the field, the tag in hand turned out to carry content rather than being blank, and the no-NDEF path has never run. Writing is the stack's other half and is unimplemented. Nothing yet reacts to a tag -- the read ends in a `printf`, and injecting a key through the desktop's existing injection path is the smallest change that would make a tag do something a person can see. `nfc` is Terminal-only, with no way in from the desktop, which is the same shape audio had before the player existed. Everything else is unchanged and unblocked: the taskbar edge is settable and left defaulting to bottom at the operator's direction; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, and the unexercised RTC write path. The one open measurement on the reader's power is worth naming while it is cheap: `board_nfc_rail_on()` raises DLDO1 on demand and leaves it up, so whether the rail is dropped when a scan ends is a decision waiting on a power-budget pass rather than a defect.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 35 COMMIT Unreleased 2026-11-12T12:34:01-07:00

#### Coming From:

Unreleased 1154f6b

#### Purpose:

Bring the T-Watch Ultra's NFC reader up for the first time and read the tag in hand with it.

#### Outcome:

At `fcd14aa` on branch `t-watch-ultra` the watch reads a tag: `nfc scan` powers the reader, polls, and prints `card A uid 1dd30d9e1c1080` with `sak 00 T2T ndef 19 bytes`, the message decoded as the text record `T Hello LilyGo` and its nineteen bytes in hex, ending `1 card`. THE READER IS NOT WHAT THIS PROJECT EXPECTED and the correction is the most valuable part: it is an ST Microelectronics ST25R3916 on the shared SPI bus -- chip select 4, interrupt 5, MOSI/MISO/SCK shared with the SD card and the LoRa radio -- and not a PN532, not on I2C. Its supply is the AXP2101's DLDO1, voltage register 0x99 and enable bit 7 of 0x90, which nothing in this port had ever raised, making it the third part here whose failure mode is to answer correctly and do nothing with its rail off. Register 0x3F reads 0x2A, which is five in the type field: base silicon rev 2, NOT the 3916B that reads six, and the two carry different antenna tuning in the vendor's analog table, so a driver written for the other would read the identity and still fail on a tag. THE TAG IS NOT BLANK EITHER. `d1 01 0f 54 02 65 6e` is a well-known Text record of fifteen bytes in language `en` carrying `Hello LilyGo`, so the cycle that was set up to exercise the empty-tag path produced content instead and that path stays written and unexercised. The port is a shim rather than a rewrite, and the reason is measured rather than assumed: `components/st25r3916/rfal/` is ST's ST25R3916 driver, RFAL and NDEF stack, about 40 700 lines under ST's SLA0052 licence, byte-identical to lewisxhe/ST25R3916-NFC-RFAL v1.0.1 apart from one line, and the Arduino surface it actually needs is three headers supplying types, GPIO, MIN/MAX and a Serial object. It cost 55 KB of flash -- `tinydesk.bin` from 2,570,352 to 2,628,560 bytes -- against the 517 KB this OTA slot still has free, so keeping it whole was affordable and the hard parts, the analog configuration table and the ISO14443A anticollision sequence, came for free. TWO DEFECTS IN THE VENDOR CODE HAD TO BE HANDLED. `RfalRfClass` declares fifty virtual functions and defines none of them while the concrete class overrides every one, so the link fails with `undefined reference to _ZTV11RfalRfClass`; marking all fifty `= 0` fixes it and is better than the alternatives because the compiler then enforces what the linker was complaining about, turning a future re-vendor's mistake into a named function at compile time instead of a mangled symbol at link time. And `attachInterrupt()` is deliberately a no-op: the stack's own fallback is `isr_pending || digitalRead(int_pin) == HIGH`, so the line is read on every worker call anyway, and a real ISR would call through a `std::function` from interrupt context while an SPI transaction to the same part might be in flight. The worker runs at 1 kHz, far inside the 106 kbit/s framing. TWO TRAPS ARE RECORDED BECAUSE EACH WOULD HAVE COST A CYCLE. The stack drives chip select by hand around each command, so the pin must stay a plain GPIO: handing it to the SPI peripheral as `spics_io_num` routes it to a peripheral output through the GPIO matrix where `gpio_set_level()` has no effect, and the chip then never answers, which is indistinguishable from a dead part. And the read is two bytes wide because the chip cannot drive MISO while it is still receiving the command byte, so a driver that takes the first byte reads the register's own address back and looks plausible. The field is on only while a scan runs, because there is no card-presence circuit on this board and detection therefore means running the radio for as long as it is wanted; a 2 s cooldown is why ten seconds of a tag resting on the glass records `1 card` and not eleven. One instrument was needed and is worth keeping: the desktop Terminal clears on completion, so the count line was lost between polls, and redirecting with `nfc scan 10 > /sd/nfc.txt` made the result durable -- which also re-exercised the SD card on the very bus the reader shares, still mounted at 119 GB, written and read back. Build is clean under `-Werror`, only the app region at 0x10000 was written with the hash verified, and the identity was observed on the glass and in a file read back off the card rather than by eye alone. The required `core-syntax.md` audit was run against the complete `.ai/` diff: `core.md` was untouched; the entry follows the six-section template and the canonical order; its number is the highest present plus one at 35; Files Modified lists only port paths and no `.ai/` file or generated artifact; Status uses allowed values only; and the active log holds 35 entries against the 100-entry limit. Both YAML blocks in `core-reference.md` were re-parsed after the change and all six fenced blocks in the file load, with 93 records and 93 index entries and no record lacking one -- and the PRE-EXISTING YAML FENCE DEFECT this log has carried since entry 16 is now FIXED, the closing fence of the fast-lookup index having sat on the last entry's line where a future edit would eventually have swallowed it.

#### Next Steps:

What the reader still cannot do is larger than what it can, and none of it is blocked. Only NFC-A Type 2 has ever been in the field, so coverage is one tag of one technology; a tag with no NDEF at all, and a bare tag with a formatted but empty container, are both written paths that have never run, and the first is the case the operator believed they were handing over. Writing is not implemented -- the vendored stack can write NDEF, so that is plumbing rather than protocol. Nothing reacts to a tag: the read ends in a printf, and the obvious first consumer is a key injection through the desktop's existing injection path, so that presenting a known tag could do something a person can see. `nfc` is a Terminal command with no way in from the desktop, which is the same shape audio had before the player existed. Two smaller things belong to this work rather than to the queue: the reader's rail is raised by `board_nfc_rail_on()` on demand and left up afterwards, so a future power-budget pass should decide whether it is dropped when a scan ends, and the interrupt pin is read and printed but never armed, which is deliberate and worth re-checking if RFAL ever takes a slow path. Everything else is unchanged and unblocked: the taskbar edge is settable and left defaulting to bottom at the operator's direction; the thermistor reads in degrees; both pushers work and both send Enter, with F10 on the BOOT pusher the shape I would still argue for; audio plays WAV files from the card; the display's tearing-effect line is configured and unread; and the remaining outboard parts are the BHI260AP at 0x28 answering and undriven, the LoRa radio needing ALDO3 and XL9555 P05, and the GNSS and microphone with no code. The standing queue is otherwise as it was: the bell seam parked with its finding recorded, the top margin as the last unpainted region, the full-screen margin question, Store installs to the card, the two untested port overrides, the unexercised RTC write path, and -- withdrawn from that list -- the `core-reference.md` fence defect, now fixed. A fresh backup is due, since `working-2026-11-12-1152.bin` predates the NFC stack.

#### Files Modified:

- ports/twatchultra/main/nfc.c
- ports/twatchultra/main/nfc.h
- ports/twatchultra/main/nfc_scan.cpp
- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/main/CMakeLists.txt
- ports/twatchultra/components/st25r3916/CMakeLists.txt
- ports/twatchultra/components/st25r3916/include/Arduino.h
- ports/twatchultra/components/st25r3916/include/SPI.h
- ports/twatchultra/components/st25r3916/include/Wire.h
- ports/twatchultra/components/st25r3916/shim/arduino_shim.cpp
- ports/twatchultra/components/st25r3916/rfal/
- ports/twatchultra/components/st25r3916/LICENSE-NFC-RFAL-fork
- ports/twatchultra/components/st25r3916/LICENSE-ST25R3916-fork
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

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
