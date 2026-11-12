# ESCAPEMENT TECHNICAL REFERENCE

> **Project:** Escapement
> **Purpose:** The authoritative lookup for externally defined facts this project depends on: the T-Watch Ultra's wiring and fitted parts, the ESP32-S3's memory and boot coupling, the CO5300 panel's controller behaviour and fbdev contract, the AXP2101 power management, the Xtensa/uClibc toolchain, the external projects embedded here, and the SPLB32 emulator measured on this board.
> **Authority:** Per `.ai/core.md`, consult this file before looking anything up online, and open the record behind any hardware fact before acting on it. A record is authoritative to the extent of its `status` and cited `sources`. When a better or newer source is found online, add or revise a record and notify the user.

This file records what an outside source (a datasheet, schematic, vendor SDK, vendor tool, or external repository) says, plus hardware observations that confirm or refute it. It does not record project history, build results, or discussion; those belong in `core-log.md`. It does not record project design choices unless an external fact forces them.

Some records are negative results — facts about what this hardware *cannot* do, or an open defect's ruled-out causes. They are recorded because each cost a session to establish and each would otherwise be re-litigated: see LCD-002, LCD-003, LCD-004, KRN-002 and BLE-004.

---

## 1. AI operating rules

```yaml
schema_version: 1

record_kinds:
  - SOC         # ESP32-S3 silicon: memory, PSRAM, cores, USB-Serial-JTAG
  - BOARD       # T-Watch Ultra: pins, fitted parts, I2C, power rails
  - DISPLAY     # CO5300 AMOLED controller and the fbdev driver contract
  - TOUCH       # CST9217 capacitive controller: address, unlock handshake, report block
  - PMU         # AXP2101 power management: supplies, registers, interrupts
  - RTC         # PCF85063A real-time clock
  - KERNEL      # the Linux port: NOMMU, fork, vectors, cached windows
  - FLASH       # 16 MiB partition layout and image formats
  - JFFS2       # the firmware's jffs2 reader and Linux-written files
  - BLE         # ESP32-S3 NimBLE as a HID host
  - NETWORK     # esp-hosted WiFi over shmem
  - TOOLCHAIN   # crosstool-NG Xtensa/uClibc, ESP-IDF, esptool
  - EXTERNAL    # interface contracts owned by other projects
  - EMU         # the SPLB32 emulator: interpreter cost, PSRAM latency, core choice

statuses:
  VERIFIED:   "Primary source cited AND confirmed on this project's hardware or tools."
  SOURCED:    "Primary source cited; not yet confirmed on this project's hardware or tools."
  INFERRED:   "Derived from secondary sources, reverse inspection, or consistent observation; no primary source. Flag before relying on it."

rules:
  - "One fact or one tightly coupled fact group per record."
  - "Every record cites at least one source with enough detail to re-check it: repository path plus commit, file, symbol, register name, or tool version."
  - "Separate what the source says (statement) from what it means for this project (consequence)."
  - "Diagnostic or implementation limits chosen by this project are never recorded as standard or device limits."
  - "A record holds what is true now. When a fact changes or proves wrong, edit the record in place: revise statement and consequence, update verified_date and sources, and end verification with 'Revised <date> (core-log entry N): <what changed>'. Git and core-log.md keep the history; this file does not."
  - "Before acting on a record, check that the code paths and symbols it cites still exist; fix the record in the same cycle if they do not."
  - "Never reuse a record_id. When a record is folded into another, or split, move its id to the retired-id table in section 8 with the record that now holds its facts, after checking that every fact still true has been carried across."
  - "An INFERRED record must say how it was inferred, in verification."
  - "A negative result is a fact. Record what the hardware cannot do with the same rigour as what it can, and say how many attempts established it. An open defect may be recorded as its established facts and ruled-out causes, with any lead labelled as a lead."
  - "When an online lookup was needed because this file had no answer, add the finding here with the existing syntax, then notify the user if the source is newer or more valid than a record already here."
  - "Use technical names (ESP32-S3, CO5300, AXP2101, PCF85063A). The user's conversational names are not used in this file."
  - "The panel is write-only and never acknowledges. Any record about what appears on the glass must say in verification that the conclusion came from looking at the screen, and must not be upgraded to VERIFIED on the strength of a return code."
```

---

## 2. Topic catalog

Topic IDs are the `record_id` prefix. An entry reserves a name; it does not claim coverage.

```yaml
- topic_id: SOC
  name: "ESP32-S3"
  description: "The SoC: internal RAM, PSRAM and its mode, the two cores, and the USB-Serial-JTAG console that is handed to Linux."

- topic_id: BRD
  name: "T-Watch Ultra board"
  description: "Board wiring and fitted parts: the I2C bus and its addresses, the PMU interrupt line, the SPI buses, and the power rail map."

- topic_id: LCD
  name: "CO5300 AMOLED and fbdev"
  description: "The panel's controller: SPI framing, window addressing, the byte order the pixel stream needs, which windows it honours, and what the fbdev driver does and does not implement for userspace."

- topic_id: TCH
  name: "CST9217 capacitive touch"
  description: "The touch controller at I2C 0x1A: its address against its neighbours, the unlock handshake that gets it scanning, the report block layout and freshness marker, and the measured read reliability on the bit-banged bus."

- topic_id: PMU
  name: "AXP2101 power management"
  description: "The PMU at I2C 0x34: the register addresses for capacity and voltage, the interrupt line and its required pull-up, the MFD cell set, and the supplies it publishes."

- topic_id: RTC
  name: "PCF85063A real-time clock"
  description: "The RTC at I2C 0x51: registration, the hctosys seeding path, and the write-back at shutdown."

- topic_id: KRN
  name: "Linux port constraints"
  description: "What the NOMMU port forces on every program and driver: contiguous load segments, the fork patch stack, the vectors address coupling, and the linear flash window's activation point."

- topic_id: FLS
  name: "16 MiB flash layout"
  description: "The partition table, where the kernel image lives and in what form, and what the release image does and does not contain."

- topic_id: JFS
  name: "jffs2 and the firmware's reader"
  description: "The writable partitions, the firmware's read-only jffs2 reader, its compression refusal, and the one-way rule that firmware never writes flash after Linux starts."

- topic_id: BLE
  name: "Bluetooth LE HID host"
  description: "The ESP32-S3's BLE radio under NimBLE: why the HID host runs in firmware, boot protocol versus report protocol, the mbuf budget, and the scan discipline the shared core requires."

- topic_id: NET
  name: "esp-hosted WiFi"
  description: "WiFi as delivered by core 0 over shmem: the interface, what the data path is worth, and which operations disrupt an associated link."

- topic_id: TOL
  name: "Toolchain behaviour"
  description: "crosstool-NG Xtensa/uClibc, ESP-IDF's build and flash tools, and the environment and endianness traps that produce misleading link errors."

- topic_id: EXT
  name: "External projects"
  description: "Interface contracts owned by other projects embedded in or measured by this one."

- topic_id: EMU
  name: "SPLB32 Tamagotchi emulator"
  description: "The interpreter measured on this board: what one emulated instruction costs, why PSRAM latency and the absence of a cache set the floor, and which of the two cores the work runs on."
```

---

## 3. Active routing

| Question | Consult first | Fast records |
|---|---|---|
| What SoC, RAM and PSRAM mode does this watch have? | SOC | SOC-001 |
| Why does the stock upstream image abort before Linux starts? | SOC | SOC-002 |
| Where is the console, and when does Linux take it? | SOC | SOC-003 |
| Can Linux reach internal SRAM, or is it MMU-only? | SOC | SOC-004 |
| Why can Linux not keep code in internal RAM? | SOC | SOC-004 |
| Can a DMA descriptor live in internal DRAM? | SOC | SOC-004 |
| Which I2C bus carries the peripherals, on which pins, at which addresses? | BRD | BRD-001 |
| Which PMU interrupt pin is it, and why did it storm? | BRD | BRD-002 |
| What does the power rail map look like? | BRD | BRD-003 |
| Which SPI bus is the panel on, and does the second one work? | BRD | BRD-004 |
| Where is the haptic driver, and why does it buzz nothing? | BRD | BRD-005 |
| Where is the touch controller, and what is at 0x5A? | TCH | TCH-001 |
| Why does the touch controller answer but never report? | TCH | TCH-002 |
| Why does an untouched screen read as a phantom press? | TCH | TCH-003 |
| Why does a held touch freeze, then report a release? | TCH | TCH-003 |
| Why is the emulator's interpreter so slow? | EMU | EMU-001 |
| Can the emulator use float instead of double? | EMU | EMU-001 |
| Why does entering the time screen take 1.5 s? | EMU | EMU-002 |
| Is 40 MHz safe on this panel? | LCD | LCD-011 |
| How much does raising the SPI clock buy? | LCD | LCD-011 |
| Can the panel transfer use DMA to go faster? | LCD | LCD-012 |
| What actually limits a framebuffer write on this board? | LCD | LCD-013 |
| Why is the emulator slow when the ROM computes? | EMU | EMU-003 |
| How fast is PSRAM access on this watch? | EMU | EMU-003 |
| Why not use the other core? | EMU | EMU-004 |
| Is the interpreter doing more work than it needs to? | EMU | EMU-005 |
| How many steps and memory accesses does one emulated instruction cost? | EMU | EMU-006 |
| Is instruction fetch the cost, with no I-cache? | EMU | EMU-007 |
| How fast is a read from the flash window, compared with PSRAM? | EMU | EMU-008 |
| Should the emulator's ROM live in flash or PSRAM? | EMU | EMU-009 |
| Why is the emulator slower with the touch buttons running? | EMU | EMU-010 |
| How fast does the emulator actually run the real ROM? | EMU | EMU-011 |
| What resolution is the Tamagotchi LCD really, and does rendering it 1:1 help? | EMU | EMU-013 |
| Why does the status menu run at half speed when play is fine? | EMU | EMU-014 |
| Where do the 464 cycles per emulated instruction actually go? | EMU | EMU-015 |
| Is there 2x left in the interpreter, or is 0.52x near the floor? | EMU | EMU-016 |
| Did the core-0 shared-memory probe work? | EMU | EMU-017 |
| Why did the emulator speed up when the touch daemon was retired? | EMU | EMU-012 |
| Why does a 512 KB allocation work once and then fail? | KERNEL | KRN-004 |
| Why not put the hardware behind the RTOS on core 0? | KERNEL | KRN-005 |
| Why is the hardware I2C that slow? | KERNEL | KRN-006 |
| Why does the I2C controller ignore every register write? | KERNEL | KRN-006 |
| Why does one bad I2C transfer kill the whole bus until reboot? | KERNEL | KRN-007 |
| Why can no driver map a GPIO interrupt on this board? | KERNEL | KRN-008 |
| Is Linux really only using one core? | KERNEL | KRN-005 |
| Why are taps at the top of a bar ignored? | TCH | TCH-004 |
| Why does the touch controller answer but never report a touch? | TOUCH | TCH-005 |
| Which way round are the touch axes, and do they need calibrating? | TOUCH | TCH-006 |
| What is the official sequence to bring the CST9217 up? | TOUCH | TCH-007 |
| Why does a finger count of 1 not mean a finger is down? | TOUCH | TCH-008 |
| Which pin is the touch interrupt, and does it help? | TOUCH | TCH-009 |
| Is the interrupt really doing the work, or is the poll? | TOUCH | TCH-010 |
| Can userspace draw on /dev/fb0? | LCD | LCD-001 |
| Why does a thin fill draw nothing while a thick one works? | LCD | LCD-002 |
| Does a 45x45 window draw, or is it too narrow? | LCD | LCD-008 |
| Why does a tall strip tear into diagonal splits? | LCD | LCD-009 |
| What does a panel transfer cost per byte? | LCD | LCD-010 |
| Is the panel or the CPU the display bottleneck? | LCD | LCD-010 |
| Why did a white test pattern come out pink? | LCD | LCD-008 |
| Why does a coloured fill report success and change nothing? | LCD | LCD-003 |
| Why is one pixel missing from the left and top edges? | LCD | LCD-004 |
| How fast can a frame be pushed to the panel? | LCD | LCD-005 |
| Why can a whole frame not be one transfer? | LCD | LCD-006 |
| How is the panel's geometry related to fb0's? | LCD | LCD-007 |
| What registers give battery capacity and voltage, and in what units? | PMU | PMU-001 |
| Why did the PMU work, then stop, after a rebuild? | PMU | PMU-002 |
| Which config option publishes axp20x-usb? | PMU | PMU-003 |
| Does the PMU interrupt work, and does LEVEL_LOW really storm? | PMU | PMU-004 |
| What do the charge registers mean, and is this board charging correctly? | PMU | PMU-005 |
| Is a battery thermistor fitted, and does it read the battery or the chip? | PMU | PMU-006 |
| What NTC part is on the TS pin, and how do millivolts become degrees? | PMU | PMU-007 |
| Why did the click disappear when the touch daemon was retired? | HAPTIC | HAP-001 |
| Where does the RTC's time come from and go to? | RTC | RTC-001 |
| Does the RTC interrupt actually work, or is it dead? | RTC | RTC-004 |
| Why does `date` print UTC when the clock is right? | RTC | RTC-002 |
| Why did the clock jump to last shutdown's time at boot? | RTC | RTC-003 |
| What survives a full-image flash in /etc? | FLS | FLS-003 |
| Can the firmware persist anything to flash after Linux starts? | JFS | JFS-001 |
| Can the firmware read a file Linux wrote? | JFS | JFS-002 |
| Why does a program fail to exec with free memory remaining? | KRN | KRN-001 |
| Why does a large static array break a program? | KRN | KRN-001 |
| Why does reading 0x42xx0000 from firmware crash the board? | KRN | KRN-002 |
| Why must the kernel be rebuilt when firmware RAM use changes? | KRN | KRN-003 |
| Where does the kernel image live in flash, and in what form? | FLS | FLS-001 |
| What does the release image contain, and what is missing from it? | FLS | FLS-002 |
| Why does a full image flash cost the display and the credentials? | FLS | FLS-002 |
| Why does NimBLE refuse to pair? | BLE | BLE-001 |
| Why does a report-map read kill the link? | BLE | BLE-002 |
| Boot protocol or report protocol? | BLE | BLE-003 |
| Why does the board reset-loop when Bluetooth scans? | BLE | BLE-004 |
| Why does an unattended reboot miss the keyboard entirely? | BLE | BLE-004 |
| Why does the keyboard connect but type nothing? | BLE | BLE-003 |
| Why does a saved bond never come back at boot? | BLE | BLE-006 |
| Can firmware allocate scratch space in PSRAM? | BLE | BLE-006 |
| Why does logging break the WiFi interface? | BLE | BLE-005 |
| What is WiFi's real throughput and latency? | NET | NET-001 |
| Is `wifi scan` a safe connectivity test? | NET | NET-002 |
| Why does the console fill with `bad if_type 6`? | NET | NET-003 |
| Where do firmware logs go after handoff? | NET | NET-003 |
| Why does the cross compiler emit objects the linker rejects? | TOL | TOL-001 |
| Which toolchain is it, and what does it produce? | TOL | TOL-002 |
| Why does a kernel patch silently go missing? | TOL | TOL-003 |
| What can an emulator measured on this board achieve? | EXT | EXT-001 |
| Where can a large read-only blob live without RAM? | EXT | EXT-002 |
| Why does a fullscreen app flicker against the console? | EXT | EXT-003 |

---

## 4. Fast lookup index

```yaml
SOC-001: "ESP32-S3: 512 KiB internal SRAM, 8 MiB PSRAM fitted as QUAD (not octal), 16 MiB flash, two Xtensa LX7 cores at 240 MHz; core 0 runs firmware, core 1 runs Linux"
SOC-002: "The stock upstream release image aborts before Linux with 'octal_psram: PSRAM ID read error' because this watch's PSRAM is quad; this project's firmware must follow any full-image flash"
SOC-003: "The only console is the ESP32-S3's USB-Serial-JTAG, presented as /dev/ttyACM0; core 0 owns it until Linux claims it about 1.75 s into boot, after which firmware output written there is lost"
SOC-004: "Internal SRAM is directly addressed, not MMU-mapped: IRAM 0x40370000..0x403E0000 and DRAM 0x3FC88000..0x3FD00000 need no MMU entry, and Linux already executes in 4 KiB of IRAM at 0x40380000 (42 System.map symbols); it owns no internal DRAM only because nothing has allocated it"

BRD-001: "All peripherals are on one bit-banged I2C bus (i2c-gpio, SDA pin 3, SCL pin 2, kernel lines 579/578) at 100 kHz: AXP2101 0x34, XL9555 0x20, PCF85063A 0x51, DRV2605 0x5A, CST9217 0x1A"
BRD-002: "The AXP2101 interrupt is GPIO7, open drain, active low, and MUST have input-enable plus an internal pull-up (PIN(7) FUN0_20MA_IE_WPU); a level IRQ on the unpulled pin storms, hangs the boot and starves the bit-banged I2C bus"
BRD-003: "Rail map: ALDO1 SD card, ALDO2 display, ALDO3 LoRa, ALDO4 sensor, BLDO1 GNSS, BLDO2 speaker, DLDO1 NFC, LDO1 GNSS backup; the display rail is additionally gated by the XL9555 at 0x20"
BRD-004: "The panel is on SPI2 (SCK 40, D0-D3 38/39/42/45, CS 41 driven by hand, RST 37, TE 6); a second general-purpose SPI bus exists on SCK 35 / MOSI 34 / MISO 33 with SD CS 21 and LoRa CS 36, but it has never been proven to transfer a byte"
BRD-005: "The DRV2605 haptic driver is at I2C 0x5A, identified by STATUS 0x00 = 0xE0 (DEVICE_ID 0b111); FEEDBACK 0x1A = 0x36 means an ERM motor, so the effect library is 1-4 and not the LRA library 6; its rail is XL9555 port0 bit 6"

LCD-001: "fb0 implements only fb_setcolreg, fb_fillrect, fb_copyarea and fb_imageblit; with no fb_write and no fb_mmap, nothing in userspace can draw on /dev/fb0"
LCD-002: "The CO5300 will not reliably honour a window one pixel wide or one pixel tall; four 1 px fills returned success and drew nothing, while a 432x31 fill in the same place appeared"
LCD-003: "Pixel data must be byte-swapped (swab16) because the panel takes the low byte first; black is identical in either byte order, so a broken fill looks like a working one"
LCD-004: "One pixel is lost at the left and top edges of the visible glass and none at the right and bottom, so equal drawn thickness is not equal visible thickness"
LCD-005: "A 320x400 RGB565 frame costs about 190 ms in SPI transfers, roughly 19 ms per 40-row band; frame rate is bounded by the display path, not the CPU, giving about 5 fps practical ceiling"
LCD-006: "A whole frame cannot be a single transfer: spi-mem refuses a data-only op with no command, and the controller treats a CS edge as end-of-burst"
LCD-007: "The glass is 410x502 with 45-degree chamfers; the largest unclipped rectangle is 320x412 and fb0 is 320x400 at (45,45), leaving a 57-row strip below it that the touch buttons are painted into"
LCD-008: "45x45 windows are honoured, so the edge API needs no full-width fallback; and in RGB565 green is a 6-bit field, so RGB565(31,31,31) is pink (0xFBFF) and white needs 63 (0xFFFF)"
LCD-009: "A long single-transfer window to the right strip (45x356) tears into diagonal splits; the same rect in 2-row bands is clean. Not width, not X, not height -- the cap is a workaround and the root cause is unestablished, with SPI clock the remaining suspect"
LCD-010: "Transfers cost 11.42 us per 64-byte chunk with per-transfer fixed cost ~0, at 5.6 MB/s (56% of quad wire); the panel itself is only 1.6% of that, so the remaining 5 us/chunk is CPU FIFO work and DMA caps at about 2x"
LCD-011: "40 MHz is clean on the glass and is the driver's compiled-in default, but doubling the clock buys only 13 percent (115.15 ms to 100.85 ms per frame), which is the evidence that the transfer is not clock bound"

LCD-012: "SPI DMA is not usable here today: the GDMA out-link descriptor address field is 20 bits, so a descriptor must sit in internal DRAM below 1 MB, and Linux owns none of it -- the obstacle is allocation, not addressability (SOC-004)"
LCD-013: "A full 320x400 frame costs 93-95 ms at 40 MHz and 108.8 ms at 20 MHz through fbbench, so halving the clock costs only 15 percent and the transfer is neither wire bound nor MMIO bound: roughly 20 us of each 64-byte transaction is fixed engine turnaround"
TCH-001: "The touch controller is a CST9217 at I2C 0x1A (register 0x0000 reads 0xCA, the family checkcode); 0x5A is the DRV2605 haptic driver, whose register 0x00 reads 0xE0 (DEVICE_ID bits), and only 0x1A completes the CST92xx unlock handshake"
TCH-002: "A reset CST9217 is in command mode and reports nothing: write 0xD1=0x01, then 0xD1=0x1E twice, require byte 1 of a read of 0x0002 to be 0x1E, then write 0xD1=0x09 for normal mode"
TCH-003: "Report block at 0xD000 is 15 bytes and the transfer must be all 15 even though only 7 are decoded; byte 0 = 0x06 for a down event and 0xAB means no new report; x=(b1<<4)|(b3>>4), y=(b2<<4)|(b3&0x0F); a short read leaves the ack mid-block so a held touch freezes"
TCH-004: "The strip threshold must sit above the painted bar edge: taps on a bar decode to panel Y a few px high near the bottom (measured 442..443 against a bar painted from 445), so the threshold is set to 440"

TCH-005: "The CST9217 reset is P08 of the XL9555 expander at 0x20 (port 1 bit 0), not an SoC GPIO, and port 1 comes up all inputs with the line held high, so nothing drives reset at boot"
TCH-006: "The CST9217 reports 410x502 in 0xD1F8, exactly PANEL_W x PANEL_H, so controller coordinates are panel coordinates: no axis swap, no mirroring, no scaling"
TCH-007: "LilyGo's own bring-up is reset, 0xD1=0x01 command mode, then attribute reads of 0xD1FC (checkcode 0xCACA____), 0xD1F8 (resolution) and 0xD204 (chip type); it does not use the 0xD1=0x1E unlock handshake recorded in TCH-002"
TCH-008: "In the 15-byte report block only event 0x06 is a press, byte 6 must be 0xAB or the block is invalid, the finger count is byte 5 bits 6:0 and more than 2 is invalid, and the part emits no release event at all -- a release must be inferred"
TCH-009: "TP_INT is GPIO 12, active low, and the pull-up is part of the contract because the controller only pulls the line down and releases it; interrupt-driven touch cuts idle bus traffic about 8x"
TCH-010: "The interrupt, not the poll, discovers a touch: with a 5 s idle poll, eight taps one second apart produced eight presses and eight releases and 255 interrupts on IRQ 33"
PMU-001: "AXP2101 capacity is register 0xa4 bits 6:0 (0x64 = 100, above 100 = not settled); voltage is registers 0x34/0x35, 14 bits, already in mV, and the power_supply property is uV so multiply by 1000"
PMU-002: "The AXP2101 has no mainline driver, so it lives in patches/kernel/axp2101/; a non-recursive patch glob plus '|| true' silently dropped the series and a rebuild produced a kernel with no PMU driver and no error"
PMU-003: "CONFIG_AXP20X_POWER publishes /sys/class/power_supply/axp20x-usb and is off by default; its absence is silent because axp20x-battery still appears"

PMU-004: "The AXP2101 interrupt on GPIO 7 works, and its earlier zero count meant no event had happened rather than a broken line: a tap of Pusher A advanced IRQ 8 from 0 to 2 and both axp20x-usb-power-supply sub-sources from 0 to 1"
PMU-005: "Charge registers are 0x18 enable, 0x30 NTC, 0x61 precharge, 0x62 fast charge, 0x63 termination, 0x64 CV; this board defaults to 4200 mV, 500 mA and 125/125 mA, already correct for its 1100 mAh cell, whose limits are below 500 mA and 0.5 C"
PMU-006: "A battery thermistor IS fitted on the TS pin despite the schematic marking it NC: TS tracks temperature in both directions and reads the CELL, not the SoC, so during charging the die held 35.8 C while TS fell 11 mV. Absolute calibration is not established, so it is reported in millivolts"
PMU-007: "The TS NTC is 10 kohm at 25 C between TS and GND, driven by a selectable 20/40/50/60 uA current source (reg50H[1:0]); the AXP2101 datasheet tabulates temperature, resistance and ADC code from -20 to 60 C at 50 uA. This watch reads reg50H = 0x07, i.e. 60 uA not the 50 uA default, so a conversion must scale by the current it finds. reg50H[4] = 0 also means TS gates the charger"
RTC-001: "The PCF85063A at I2C 0x51 registers as rtc0 and, with CONFIG_RTC_HCTOSYS, seeds the system clock before init; its interrupt shares GPIO bank 0 with the PMU line"
RTC-002: "The image ships no /etc/TZ, /etc/localtime or /usr/share/zoneinfo, so `date` prints UTC even when the clock is right; /etc/TZ containing MST7 (UTC-7 all year, no DST) is the whole fix for uClibc-ng"
RTC-003: "The upstream image's /etc/init.d/S01clock saves the epoch to /home/.clock at shutdown and overwrites the RTC-seeded clock at boot; the board has one RTC (rtc0 at 0x51), so it is a second mechanism, not a second clock, and this project disables it by moving it out of /etc/init.d because S??* still matches .disabled"

RTC-004: "The PCF85063A interrupt on GPIO 1 works; its zero count meant no alarm had ever been armed, not a dead line -- arming one through /sys/class/rtc/rtc0/wakealarm advanced IRQ 7 from 0 to 1"
FLS-003: "A full-image flash rewrites /etc and S03keepconfig carries back only seven named files plus dropbear keys, so project-added /etc files must be re-installed by scripts/watch-time.sh after one; a kernel-only flash leaves /etc alone"

KRN-001: "NOMMU with no overcommit: every ELF load segment and every large allocation must be CONTIGUOUS, so 'nommu: Allocation of length N failed' is a contiguity failure, not an out-of-memory one, and a big static array blocks exec rather than failing at run time"
KRN-002: "init_linux_mmap() extends the linear flash window over the whole 16 MiB only inside map_flash_and_go(), immediately before the jump to Linux; firmware dereferencing 0x42xx0000 earlier reads unmapped memory and reset-loops with no output"
KRN-006: "The ESP32-S3 I2C peripheral boots with its clock gate CLOSED (SYSTEM_PERIP_CLK_EN0 bit 7 = 0) and its reset HELD (SYSTEM_PERIP_RST_EN0 bit 7 = 1), so every register write is discarded while reads and FIFO writes still succeed, which presents as a peripheral stuck at CTR=0x20b with master mode clear"

KRN-007: "An I2C NACK or arbitration loss must reset the ESP32-S3 controller: recovery was wired only into the timeout branches, so a NACK leaving SR.bus_busy set wedged every later transfer to every device until reboot"
KRN-008: "The Xtensa IRQ descriptor table is 33 entries (XCHAL_NUM_INTERRUPTS 32 + PLATFORM_NR_IRQS 0 + 1) because CONFIG_SPARSE_IRQ is unavailable, and the board exhausts it, so a later driver cannot map a GPIO interrupt at all"
KRN-005: "Core 0 runs the radios because the WiFi driver is a closed blob only ESP-IDF can run; Linux is a FreeRTOS task pinned to core 1 that never returns, and esp-hosted carries network, Bluetooth and typed custom interfaces but no I2C/SPI/GPIO RPC -- so there is no channel for an RTOS hardware server, and I/O stays on Linux"
KRN-004: "The largest contiguous block is 512 KB (order 7), and exactly 524288 bytes is the worst size to request: malloc(524288) fails where malloc(524000) succeeds because the allocator header does not fit, and splitting the ROM into two halves fails for the same reason; one such allocation also consumes the block for the rest of the session"
KRN-003: "The kernel is linked against a fixed address in the firmware's RAM (CONFIG_VECTORS_ADDR) which is the linker symbol space_for_vectors; moving it via a firmware change gives a kernel that boots and then dies silently at 'Run /sbin/init'"

FLS-001: "16 MiB layout: nvs 0xa000/0x5000, phy_init 0xf000/0x1000, factory 0x10000/0xc0000, etc 0xd0000/0x70000, linux 0x140000/0x400000, rootfs 0x540000/0x780000, home 0xcc0000/0x340000; xipImage is stored RAW at 0x140000 and can be written alone"
FLS-002: "The release image is not this project's state: its kernel has no CO5300 driver, its firmware requires octal PSRAM, and its /etc has no WiFi credentials; flashing it costs the display, the login, and the credentials"

JFS-001: "A firmware flash write aborts only when it issues the inter-core park, which g_spi_flash_skip_ipc skips -- linux_flash.c uses that to serve Linux's writes; the real constraint is that the write path must touch no PSRAM and no flash-resident code while the shared cache is off, which internal-DRAM staging satisfies"
JFS-002: "The firmware's jffs2 reader can read files Linux wrote, including compressible ones, verified byte-exact against a dumped partition; it returns -1 for any compressed node, so mounts should carry compr=none"

HAP-001: "The tap click lives in the kernel as drv2605_tap() called by the touch driver on each press transition, because the userspace library that made it died with its only caller"
BLE-001: "NimBLE's store is the application's job: without ble_store_config_init() ble_gap_security_initiate() returns BLE_HS_ENOTSUP (8), the link never encrypts, and a HID keyboard refuses every ATT operation with 0x05"
BLE-002: "At MTU 23 an ATT read returns 22 bytes, so a 934-byte report map costs 43 round trips and one mbuf each; the pool defaults to 12 blocks, empties, and the keyboard drops the link with HCI 0x16"
BLE-003: "HID boot protocol (0 to 0x2A4E) gives a fixed 8-byte report with no report ID and no report map; report protocol needs the map and is what exhausted the ATT buffers"
BLE-004: "A continuous scan starves core 0's idle task and trips the task watchdog (rst:0x7 TG0WDT_SYS_RST); and an idle BLE keyboard advertises only for a few seconds, so an 8 s scan gap is deaf 35% of the time and can miss it with no log trace"
BLE-005: "Firmware logging rides the same shmem link as WiFi bring-up, so one log line per BLE advertisement stops the interface coming up with `Command[1] timed out` -- logging is traffic on this board, and the failure also kills the log channel that would report it"
BLE-006: "PSRAM is mmap, not a heap (CONFIG_SPIRAM_USE_MEMMAP), so MALLOC_CAP_SPIRAM allocations always fail; and fw_log from a pre-Linux path is dropped, so boot-time diagnostics must be deferred with fw_log_defer/fw_log_flush"

NET-001: "WiFi delivers about 409 KB/s bulk and 30-50 ms to the gateway over esp-hosted on a shared shmem link and one core"
NET-002: "`wifi scan` proves the radio and scan path only, not the data path; repeated scans while associated disrupt the link, so ping the gateway and use the board's own `wifi connect` helper"
NET-003: "The firmware fork added ESP_LOG_IF = 6 for its diagnostics, so its ESP_MAX_IF is 7 against the kernel's 6; a mismatch makes every log frame fail if_type >= ESP_MAX_IF and print `bad if_type 6`. Matching the enum alone is not enough -- the frame needs a consumer or it queues forever, so enum and /dev/esp-log driver belong in one patch"

TOL-001: "Without XTENSA_GNU_CONFIG pointing at esp32s3.so the compiler emits BIG-ENDIAN objects and the link fails with 'file in wrong format' naming libc or libgcc, which reads as a broken toolchain"
TOL-002: "crosstool-NG xtensa-esp32s3-linux-uclibcfdpic, GCC 14.1.0, uClibc-ng; binaries are dynamically linked to libc.so.0 with interpreter /lib/ld-uClibc.so.0, and -static needs -mfdpic"
TOL-003: "A patch loop globbing patches/kernel/*.patch does not recurse, and 'git apply || true' hides the failure; both are how entire patch series disappeared from a build without an error"

EXT-001: "The SPLB32 Tamagotchi core runs at 4.05x realtime headless on this board, and 1.00x while painting 320x400 at 4 fps from the flash-resident ROM; the display path is the bottleneck"
EXT-002: "The linear flash window 0x42000000 is readable from userspace, so a 512 KB ROM at 0x42480000 costs zero RAM -- the fix for contiguity on NOMMU; file mmap costs 2x the file and MAP_SHARED fails ENODEV"
EXT-003: "A fullscreen app on fb0 flickers against fbcon because every printk repaints the console over it; console_loglevel decides it, and /etc/sysctl.conf sets kernel.printk = 1 4 1 7"

EMU-001: "Doubles cost 711 ns against 121 ns for the integer equivalent, so SPLB32's accumulators are 64-bit fixed-point at CYC_SHIFT=7; float is not slow but fatal, because the kernel does not enable the FPU coprocessor for user tasks and the process dies with SIGSEGV"
EMU-002: "Entering the time screen takes about 1.5 s because the guest computes for 580.1 ms of emulated time and emulated time then runs at about a third of real; parked by user decision, with 43-135 ms per frame during the stall still unexplained"
EMU-003: "A dependent load from PSRAM costs 178.83 ns, about 43 cycles, and there is no cache at all (XCHAL_DCACHE_SIZE=0, no locality effect across three working sets), so the roughly 12 PSRAM round trips per emulated step bound the emulator at 265k instructions/s"
EMU-004: "Both cores are LX7 at 240 MHz, but Linux runs on core 1, which has no cache and no reachable internal SRAM, while core 0 has both; SMP is not supportable (CONFIG_HAVE_SMP unset, XCHAL_HAVE_EXCLUSIVE=0), so moving emulation to core 0 is the only full-speed design and it is deliberately not started"
EMU-005: "The interpreter spends about 3676 ns per emulated instruction against a 12.77 ns native yardstick, on the order of 250 native instructions against the 30 to 80 a tuned core needs; the count is derived from timing and NOT measured, and counting it is the first task"
EMU-006: "One emulated instruction is 1.622 interpreter steps (exactly one per opcode plus sleep ticks), 2.408 guest bus accesses and about 1181 native cycles; 72.6 percent of the accesses are ROM, and memory is only ~2.6 percent of the cost, so the interpreter's own instruction stream is what to attack"
EMU-007: "Instruction fetch from PSRAM is not the cost: identical arithmetic over 1 KB of code costs 30.5 ns/op and over 8 KB costs 31.7 ns/op, so growing the interpreter's code body is nearly free"
EMU-008: "A dependent read from the linear flash window costs about 738 ns against 53 ns for PSRAM, a factor of 14 that does not depend on working-set size, and the ROM is 72.6 percent of the emulator's bus accesses, so the ROM's location is worth testing -- a 512 KB contiguous PSRAM block needs a fresh boot"
EMU-009: "Putting the ROM in PSRAM instead of the flash window takes the emulator from 204033 to 273098 instructions/s, 4.49x to 6.01x, a gain of 33.8 percent; the 1239 ns saved per instruction matches the 1290 ns the ROM-access prediction implies to within 4 percent"
EMU-010: "The touch daemon costs the emulator about 30 percent of its throughput -- 141k instructions/s with S50tbtns running against 204k with it stopped, on the same boot, same binary"

EMU-011: "The emulator has 5.9-7.4x realtime headroom headless (513765 instr/s with the ROM in PSRAM, 408300 from the flash window) and holds 1.00x at 2/4/8/16 fps, so neither the panel nor the interpreter limits realtime play; corebench and tamafb figures are not comparable across benchmarks"
EMU-012: "Retiring the touch daemon gave back the 30 percent EMU-010 predicted, and the three bars still work because cst92xx decodes them into a touch-buttons input device in the kernel instead of a userspace poller"
EMU-013: "The guest LCD is really 49x49 dots; rendering it 1:1 instead of at a 5 px dot on a 6 px pitch drops per-frame cost from render 2.5 ms plus panel 5.7 ms to 0.6 and 0.5 ms, and stalls in a 20 s idle run from 29 to 0"
EMU-014: "The status menu runs at 0.52x because it needs about 520k instructions/s against a 513k ceiling, not because of the display: cycles per instruction collapse toward 1 while panel time stays at 8-37 ms/s"
EMU-015: "An emulated instruction executes 258 host instructions containing 86 loads and 30 stores -- 116 host memory operations -- and at 464 cycles per instruction those accesses at about 4 cycles each account for the cost almost exactly"
EMU-016: "The interpreter is already at normal efficiency: coverage counter +0.8 percent, -O3 -0.5 percent, timers_clock removal +7.7 percent, so about 1.4x is left in userspace and only core 0 has the 4-7x"
EMU-017: "OPEN and superseded: a firmware task writing a PSRAM buffer from the heap put the board in a boot loop, and the overlap hypothesis this record proposes was NOT confirmed -- coherency was later proven working and the emulator moved to core 0 (EMU-004, docs/cores.md)"```

---

## 5. Records

```yaml
- record_id: SOC-001
  kind: SOC
  topic_id: SOC
  title: "ESP32-S3: 512 KiB internal SRAM, 8 MiB QUAD PSRAM, 16 MiB flash, two LX7 cores"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The T-Watch Ultra carries an ESP32-S3 with 512 KiB of internal SRAM, 8 MiB of PSRAM fitted in QUAD mode, and 16 MiB of QSPI flash accessed in DIO at 80 MHz. It has two Xtensa LX7 cores at 240 MHz. Core 0 runs the ESP-IDF firmware and its radios; core 1 runs Linux."
  consequence: "The split is the architecture of this project: firmware is 'core 0' and Linux is 'core 1' throughout. Anything the firmware does that needs the other core to cooperate must account for that core running Linux rather than a FreeRTOS task loop — which is why any firmware flash write has to skip the inter-core call rather than park Linux (JFS-001)."
  sources:
    - "README.md, Hardware section (SoC/RAM/Flash table)"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/sdkconfig.defaults.esp32s3.16m8r: CONFIG_SPIRAM_MODE_QUAD=y, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y"
  verification: "Read from the boot log and the running system; `free -m` on the watch reports 8 MiB total with about 2 MiB available at idle, consistent with the documented figures."

- record_id: SOC-002
  kind: SOC
  topic_id: SOC
  title: "The stock upstream image aborts before Linux because it requires OCTAL PSRAM and this watch is QUAD"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The upstream release firmware is built for OCTAL PSRAM. On this watch's quad-PSRAM part it aborts during startup with 'octal_psram: PSRAM ID read error: 0x00000000, PSRAM chip not found or not supported, or wrong PSRAM line mode', and Linux never starts."
  consequence: "Any full-image flash of the release image must be followed immediately by this project's own firmware at 0x10000, or the board resets forever with nothing on the console. The partition table does not change the PSRAM configuration; only the firmware binary does."
  sources:
    - "esptool full-image flash of images/linux-esp32s3-native-full.bin, then the boot log: 'E (173) octal_psram: PSRAM ID read error: 0x00000000'"
    - "sdkconfig.defaults.esp32s3.16m8r: CONFIG_SPIRAM_MODE_QUAD=y, CONFIG_LINUX_PSRAM_8M=y"
  verification: "Reproduced on hardware in both directions: the release image alone reset-looped at the PSRAM probe (94 resets in a 40 s capture); the same image with this project's firmware written at 0x10000 booted to a login prompt."

- record_id: SOC-003
  kind: SOC
  topic_id: SOC
  title: "The only console is the USB-Serial-JTAG port, and core 0 loses it to Linux early in boot"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The board has no UART wired to anything. Its console is the ESP32-S3's USB-Serial-JTAG peripheral, presented to a host as /dev/ttyACM0 at 115200 baud. ESP-IDF writes its log there until Linux claims the port; the kernel then takes it over and all output after that is printk. Firmware writes to the port after handoff can block on a peripheral Linux now owns."
  consequence: "Firmware panics after handoff go only to UART0, which nothing is wired to, so a crash in app_main looks exactly like a silent reset loop. Enabling CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG puts panics on the visible port, but it also stops Linux reaching userspace because both sides then drive the same peripheral — a throwaway-build instrument only. Routine firmware diagnostics travel over /dev/esp-log instead."
  sources:
    - "app_main.c and linux_boot.c: the boot handoff and the USB-Serial-JTAG claim"
    - "sdkconfig.defaults.esp32s3.16m8r: CONFIG_ESP_CONSOLE_UART_DEFAULT=y, CONFIG_ESP_CONSOLE_SECONDARY_NONE=y with the reasoning recorded"
    - "docs/bluetooth-keyboard.md: the 'firmware panics are invisible' section"
  verification: "Observed repeatedly on hardware: boot output on /dev/ttyACM0 stops being firmware text once printk takes the port. Setting the secondary console reproduced a panic backtrace on the same port and also prevented Linux from reaching userspace."

- record_id: SOC-004
  kind: SOC
  topic_id: SOC
  title: "Internal SRAM is directly addressed, not MMU-mapped, and Linux already executes in it"
  status: VERIFIED
  verified_date: 2026-11-11
  statement: "The ESP32-S3 memory map has two halves with different access rules. External memory -- flash at SOC_IROM_LOW 0x42000000 and PSRAM -- is reached through the MMU, whose 256 entries at DR_REG_MMU_TABLE map 64 KiB pages into the 0x3C000000..0x3DFFFFFF window; that is the mapping `init_linux_mmap()` programs. Internal memory is not behind the MMU at all and needs no entry: instruction RAM is SOC_IRAM_LOW..SOC_IRAM_HIGH = 0x40370000..0x403E0000 and data RAM is SOC_DRAM_LOW..SOC_DRAM_HIGH = 0x3FC88000..0x3FD00000, both readable and executable in place at those physical addresses by either core. Linux on this port already occupies part of internal IRAM: the built System.map carries 42 symbols in 0x4038xxxx, including _WindowVectors_text_start, _WindowOverflow4/8/12, _WindowUnderflow4/8/12, _KernelExceptionVector and _exception_text, and CONFIG_VECTORS_ADDR points at 0x40380000, the 4 KiB buffer the firmware reserves as space_for_vectors. Linux allocates nothing from internal DRAM -- the System.map has no symbol in 0x3FCAxxxx -- because the device tree's memory node covers PSRAM only and the firmware owns that region."
  consequence: "This corrects a load-bearing error in three earlier records (LCD-012, EMU-003, EMU-004), each of which said Linux had no reachable internal SRAM. The truth is narrower and more useful: internal SRAM is reachable and already used, but only the 4 KiB the firmware deliberately hands over for vectors, and no general allocation path exists for the rest. Two things follow. Anything the kernel must execute or read while the external-memory cache is off -- during a flash operation, when core 1 cannot fetch from either flash or PSRAM -- has to live in internal memory, and the only proven way to get it there today is the space_for_vectors pattern: reserve it in the firmware, pass the address, and let the kernel place code and data at it. And a DMA descriptor can legitimately live in internal DRAM at 0x3FC88000, but only if the firmware carves that region out and the kernel is told about it; nothing today does. The distinction to keep is MMU-mapped versus directly addressed, not present versus absent."
  sources:
    - "components/soc/esp32s3/include/soc/soc.h: SOC_DROM_LOW 0x3C000000, SOC_IROM_LOW 0x42000000, SOC_IRAM_LOW 0x40370000, SOC_IRAM_HIGH 0x403E0000, SOC_DRAM_LOW 0x3FC88000, SOC_DRAM_HIGH 0x3FD00000"
    - "build/kernel/linux-7.2.4/System.map: 42 symbols at 0x4038xxxx including _WindowVectors_text_start and _exception_text_end; none at 0x3FCAxxxx"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/linux_ll_esp32s3.c: init_linux_mmap() programs only MMU entries 0x100..0x1ff, i.e. external pages"
    - "patches/kernel/display/final/esp32s3-devkit-c1.dts: memory@0 covers 0x3d800000 only"
  verification: "Read from the built artefacts and the SoC headers, then confirmed on hardware 2026-11-11: /proc/iomem lists the flash window and the peripheral registers and no 0x3FC8/0x4038 region, because internal SRAM is not a resource any driver registers -- while /proc/kallsyms and the System.map show the kernel's own window and exception vectors executing at 0x40380000. That the vectors work at all is proved by the board booting. Revised the three records that asserted internal SRAM was unreachable: see LCD-012, EMU-003 and EMU-004."

- record_id: BRD-001
  kind: BOARD
  topic_id: BRD
  title: "One bit-banged I2C bus at 100 kHz carries every peripheral: AXP2101 0x34, XL9555 0x20, PCF85063A 0x51, DRV2605 0x5A, CST9217 0x1A"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The board's peripherals share a single I2C bus implemented with i2c-gpio rather than a hardware controller: SDA on pin 3 and SCL on pin 2, which the kernel reports as GPIO lines 579 and 578. Addresses are the AXP2101 PMU at 0x34, the XL9555 GPIO expander at 0x20, the PCF85063A RTC at 0x51, the DRV2605 haptic driver at 0x5A, and the CST9217 touch controller at 0x1A."
  consequence: "Because the bus is bit-banged in a kernel thread, anything that starves that thread takes the whole peripheral set down at once — which is what an interrupt storm on the PMU line did (BRD-002), producing I2C NAKs that looked like an I2C fault. Hardware I2C would not have fixed it."
  sources:
    - "patches/kernel/0001-dts-watch-i2c-pins-and-rtc.patch, 0004-dts-xl9555-gpio-expander.patch"
    - "kernel log: 'i2c-gpio i2c0: using lines 579 (SDA) and 578 (SCL)'"
    - "README.md, Hardware table"
  verification: "Present in every boot log on hardware. `ls /sys/bus/i2c/devices/` reports 0-0020, 0-0034 and 0-0051; the touch controller at 0x1A is fitted but has no driver."

- record_id: BRD-002
  kind: BOARD
  topic_id: BRD
  title: "The AXP2101 interrupt is GPIO7, open drain and active low, and needs input-enable plus a pull-up or it storms"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The PMU's IRQ line reaches the SoC on GPIO7 as an open-drain, active-low signal. The board's pinctrl block configures pull-ups for the I2C pins only, so GPIO7 floats unless it is configured. Declaring it as `interrupts = <7 IRQ_TYPE_LEVEL_LOW>` without a pull-up makes the floating line read as permanently asserted and the handler re-fires without end."
  consequence: "The storm hung the boot at 'AXP20x variant AXP2101 found' and never reached init, with the console silent and the CPU idle between storms. It also starved the bit-banged I2C bus, so the same run produced 'i2c-0: sendbytes: NAK bailout' and a failed RTC probe — a false I2C diagnosis. The fix is PIN(7) FUN0_20MA_IE_WPU on the pin, after which the PMU's interrupt chip and the RTC's interrupt both resolve."
  sources:
    - "patches/kernel/0003-dts-axp2101-pmu.patch: the 'No interrupt node, deliberately' note and the pmic_irq_pins pinctrl group"
    - "patches/kernel/display/final/esp32s3-devkit-c1.dts: pinctrl-0 = <&pmic_irq_pins>; interrupts = <7 IRQ_TYPE_LEVEL_LOW>"
    - "docs/bring-up.md: 'Blocker 1: a level IRQ on an unpulled pin hangs the boot'"
  verification: "Both states observed on hardware. Without the pull-up the boot hung before init; with it, /proc/interrupts shows line 8 'esp32xx-gpio 7 axp2101_irq_chip' alongside the RTC on line 7."

- record_id: BRD-003
  kind: BOARD
  topic_id: BRD
  title: "The AXP2101's regulators feed specific board loads, and the panel has its own switchable rail"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "From LilyGo's hardware page for the T-Watch Ultra, the AXP2101's rails are assigned as: ALDO1 to the SD card, ALDO2 to the display, ALDO3 to the LoRa radio, ALDO4 to the sensor, BLDO1 to the GNSS receiver, BLDO2 to the speaker, DLDO1 to the NFC reader, and LDO1 to GNSS backup. The display's power is additionally gated by the XL9555 expander at I2C 0x20."
  consequence: "The panel is not powered until both the AXP2101 rail and the expander bit are correct, so a dark panel has at least three possible causes — SPI, the expander bit, or the rail — and they must be separated in that order. The expander's power-control write is what the DTS regulator node drives."
  sources:
    - "patches/kernel/display/SECOND-SPI-BUS.md: the power map quoted from LilyGo's hardware page"
    - "patches/kernel/0005-dts-display-power-regulator.patch"
  verification: "Read from the vendor page as transcribed in this repository. Not independently measured: the rail voltages were not probed."

- record_id: BRD-004
  kind: BOARD
  topic_id: BRD
  title: "The panel owns SPI2; the second SPI bus carries SD and LoRa and is proven to transfer data, by mounting a card from ESP-IDF"
  status: VERIFIED
  verified_date: 2026-11-11
  statement: "The CO5300 panel is on a dedicated QSPI bus: SCK 40, D0-D3 on 38/39/42/45, CS driven by hand on GPIO41, RST on 37, TE on 6. A separate general-purpose SPI bus exists on SCK 35, MOSI 34, MISO 33, with SD CS on 21, LoRa CS on 36 (RST 47, BUSY 48, INT 14) and NFC CS on 4 (INT 5). That second bus works: ESP-IDF mounted both a 29 GB and a 128 GB FAT32 card on it and read and wrote files."
  consequence: "The earlier verdict that this bus had never moved a byte was measured on Linux's spi-mem stack and does not carry over. Two things had to be right, and neither is about the bus itself. First, EVERY OTHER CHIP SELECT ON THE BUS MUST BE PARKED HIGH: MISO is shared, so a CS floating low leaves that device driving the line and corrupts every transaction, which presents as a handshake failure. Driving LoRa CS 36 and NFC CS 4 high before use turned a card that would not answer into one that mounted. Second, the card's supply is the PMU's ALDO1 (BRD-003), and an unpowered card answers nothing, so the rail has to be up before the first mount attempt. While on Linux the kernel still has no MMC support at all (CONFIG_MMC is not set), so none of this helps the parked kernel."
  sources:
    - "patches/kernel/display/SECOND-SPI-BUS.md"
    - "build/kernel/linux-7.2.4/.config: '# CONFIG_MMC is not set'"
    - "patches/kernel/display/final/esp32s3-devkit-c1.dts for the panel pin set"
    - "tinydesk-twatch port: power.c board_sd_rail_on/board_power_rails_preinit and the shared-spi CS parking"
    - "arduino-esp32 variants/lilygo_twatch_ultra/pins_arduino.h for SD_CS 21 and the shared LORA_CS 36 / NFC_CS 4"
  verification: "Verified on hardware 2026-11-11 in the TinyDesk port rather than Linux: a 29 GB FAT32 card (SDHC/SDXC, verified separately on a PC as FAT32 with an MBR partition table) reported itself with `sd` as mounted at /sd, accepted a file written from the shell and read it back byte-exact, and appeared in the desktop's Files app as the folder sd through the port's existing redirect. The failure sequence is the evidence for the ordering: with nothing parked, sdmmc_init_sd_if_cond returned 0x108 (no valid response); with the CS lines parked and ALDO1 raised, the same mount succeeded. Capacity is not limited to 32 GB despite LilyGo documenting 'up to 32 GB (FAT32)': a 128 GB FAT32 card (119.2 GiB, 32 KB clusters, 3.9M clusters) mounted unchanged and wrote files, with no code or configuration difference from the 29 GB card. The real constraint is the FILESYSTEM and not the size -- FatFs in ESP-IDF 5.3 is built with FF_FS_EXFAT hardcoded to 0 and no Kconfig option, so an exFAT card cannot mount at any capacity, while 32 GB is only the size above which cards and Windows' own format tool default to exFAT. The limits are far away: sdmmc's capacity is uint32 sectors and FF_LBA64 is 0, both giving a 2.2 TB ceiling, and FAT32's cluster ceiling is 2^28 at which 32 KB clusters reach about 8.8 TB. The drive reporting path is 64-bit throughout (td_drive_t.total/free, tdsh_sdcard_space), so nothing truncates at 4 GiB either."

- record_id: LCD-001
  kind: DISPLAY
  topic_id: LCD
  title: "fb0 has no fb_write and no fb_mmap, so nothing in userspace can draw on the panel"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The CO5300 framebuffer driver originally provided only .fb_setcolreg, .fb_fillrect, .fb_copyarea and .fb_imageblit. fbcon uses those accelerated operations, which is why the text console works. There was no .fb_write and no .fb_mmap, so write() to /dev/fb0 triggered fb_WARN_ON_ONCE(!info->fbops->fb_write) in the fbdev core and returned -1, and mmap() returned ENODEV."
  consequence: "Userspace had no path onto the panel at all: a program could open /dev/fb0, query its geometry successfully, and then be unable to put a single pixel on the glass. This project added co5300_write, which copies into the framebuffer and then blits the rows it touched. fb_mmap alone would not have sufficed, because the panel has no memory of its own and is fed over SPI — writing the framebuffer only puts bytes in RAM."
  sources:
    - "patches/kernel/display/final/co5300.c: the fb_ops table and the added co5300_write"
    - "kernel log on hardware: 'fb0: fb_WARN_ON_ONCE(!info->fbops->fb_write)' with a full backtrace, and 'fb write: -1 of 256000' from the program"
  verification: "Confirmed by failure then by success on hardware: after adding co5300_write the same program wrote 163 frames over 30 s, every band accepted, exit status 0 — and the image was seen on the glass."

- record_id: LCD-002
  kind: DISPLAY
  topic_id: LCD
  title: "The CO5300 will not reliably draw a window one pixel wide or one pixel tall"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A fill issued as four thin rectangles — 322x1 across and 1x402 down — produced nothing on the glass, while every call returned success (rc=0). A 432x31 fill of the same colour, through the same function, in the same region, appeared immediately."
  consequence: "Anything drawn as a 1 px rule, border, separator or cursor must not be a 1 px window. Draw it inside a larger transfer instead: build the thin feature into the pixel buffer of a normal full-width band. This driver's blit routine already warned that a CS edge ends the burst (LCD-006); the additional constraint is that a degenerate window is not honoured at all. The failure is silent, so a return code proves nothing."
  sources:
    - "patches/kernel/display/final/co5300.c: co5300_paint_frame(), which draws the ring inside the band loop for exactly this reason"
    - "no datasheet statement located; established entirely by experiment on this panel"
  verification: "Established on hardware over four flash cycles: 1 px fills drew nothing (twice), a 432x31 strip drew, and moving the ring into the full-width band loop drew. The panel is write-only and never acknowledges, so the conclusion rests on looking at the screen."

- record_id: LCD-003
  kind: DISPLAY
  topic_id: LCD
  title: "Pixel data must be byte-swapped, and a black fill hides the mistake"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The panel takes the low byte of each RGB565 pixel first and the SPI peripheral shifts the high byte out first, so every pixel value written into a transfer buffer must be passed through swab16() before transmission. The driver's blit path has always done this; a new fill path that omitted it drew nothing visible while reporting success."
  consequence: "Black (0x0000) is identical in either byte order, so a fill path with the wrong byte order looks correct as long as it is only ever asked to draw black. Any coloured fill exposes it. When adding a new drawing path, test it with a colour that is not black."
  sources:
    - "patches/kernel/display/final/co5300.c: the comment on the swap loop in co5300_blit(), 'the panel wants low byte first and the peripheral shifts the high byte out first'"
  verification: "The blit path's swap predates this work and is proven by the eight-band test in README.md. The omission was shown on hardware: a black background drew while a red ring through the same unswapped path did not."

- record_id: LCD-004
  kind: DISPLAY
  topic_id: LCD
  title: "One pixel is lost at the left and top edges of the visible glass and none at the right and bottom"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Drawing a rectangle of uniform thickness around fb0 and counting what survives on the glass gives: with a 1 px ring, the right and bottom edges appeared and the left and top did not; with a 2 px ring, the right and bottom appeared 2 px thick and the left and top 1 px thick. Exactly one pixel is consumed at the left and top edges and none at the right and bottom."
  consequence: "Equal drawn thickness does not produce equal visible thickness. The driver compensates by drawing the left and top one pixel wider than the right and bottom, so a 1 px line is visible on all four sides. This is also the answer to the unresolved note in docs/panel-geometry.md, where the first row and column did not appear in the measurement photographs."
  sources:
    - "measured on this panel; no datasheet statement found"
    - "docs/panel-geometry.md, 'The first row and column are swallowed — measured'"
  verification: "Measured on hardware by counting visible pixels at three thicknesses, one flash each. Not a colour or byte-order effect: the same call, colour and byte order produced different visible thickness on different edges."

- record_id: LCD-005
  kind: DISPLAY
  topic_id: LCD
  title: "A 320x400 frame costs about 190 ms in SPI transfers, so the display bounds the achievable frame rate"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Pushing 320x400 RGB565 to the panel by band blits costs roughly 190 ms per frame, about 19 ms per 40-row band. Measured across frame rates on an emulator workload: 1.67x realtime at 2 fps, 1.04x at 4 fps, 0.60x at 8 fps, 0.32x at 16 fps and 0.17x at 32 fps, against 4.05x with no display. Subtracting emulation time leaves an essentially constant ~190 ms of transfer per frame."
  consequence: "Frame rate is bounded by the SPI path, not the CPU. About 5 fps is the practical ceiling while painting this panel; above that the CPU spends nearly all its time waiting on transfers. Raising it needs fewer, larger transfers — a persistent RAMWR window in the driver — rather than faster computation. The blit path is one spi-mem op per call through a 64-byte FIFO, with no DMA and no dirty-region tracking."
  sources:
    - "measured on hardware with the SPLB32 emulator's sweep harness; results in TamaHarness/for-escapement/RESULTS-3.md and tamabench-on-watch.txt"
    - "patches/kernel/display/final/co5300.c: the blit path's own note on the 64-byte FIFO and the absence of DMA"
  verification: "Five configurations measured in one process on one boot, with emulation time subtracted using the separately measured headless rate of 16.2 M emulated cycles/s."

- record_id: LCD-006
  kind: DISPLAY
  topic_id: LCD
  title: "A frame cannot be sent as one transfer: spi-mem needs a command, and a CS edge ends the burst"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Pixel data reaches the panel through SPI_MEM with a command and an address phase per transfer. spi-mem refuses an operation that carries data with no command, so a pixel stream cannot be continued across calls; and the controller treats a CS edge as end-of-burst and silently discards whatever follows, so a transfer cannot be split mid-stream either."
  consequence: "Every rectangle to be drawn must be one indivisible transfer, and a full-panel fill must be split into bands small enough to fit a transmit buffer while each band remains a complete operation. This is why the driver allocates a full-frame transmit buffer for blits and why the background painter works in bands."
  sources:
    - "patches/kernel/display/final/co5300.c: the comment on the single-op pixel stream in co5300_blit(), 'The controller treats a CS edge as end-of-burst and silently discards the rest'"
  verification: "From the driver's own analysis of its earlier failures, consistent with the observed behaviour of every fill attempted on hardware this session."

- record_id: LCD-007
  kind: DISPLAY
  topic_id: LCD
  title: "The glass is 410x502 with 45-degree chamfers; the largest unclipped rectangle is 320x412 and fb0 is 320x400 at (45,45)"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The panel's addressable area is 410 by 502 pixels, windowed at column 22 because the controller's RAM is wider than the visible glass. Its corners are chamfered at 45 degrees rather than rounded, so the largest rectangle guaranteed to render in full is 320x412, being the panel inset by 45 pixels on each side. fb0 is set to 320x400 and placed at offset (45,45), giving fbcon 40 columns by 25 rows of the VGA 8x16 cell with no driver-side arithmetic."
  consequence: "The area outside fb0 — 45 pixels wide, 45 above and 57 below — is not part of the framebuffer, so neither fbcon nor userspace can draw there. It exists only in the panel's own RAM and can only be painted by the driver at probe time, in panel coordinates. The 57-row strip below fb0 is what the three touch buttons are painted into."
  sources:
    - "patches/kernel/display/final/co5300.c: PANEL_W/PANEL_H/PANEL_OFFSET_X/PANEL_SAFE_INSET and the FB_X/FB_Y derivation"
    - "docs/panel-geometry.md: the measurement method and numbers"
    - "kernel log: 'Console: switching to colour frame buffer device 40x25' and 'co5300 spi0.0: 410x502 RGB565, fb0 registered'"
  verification: "Confirmed on hardware by the console geometry itself and by painting the surrounding border in panel coordinates, which only works if the pad between fb0 and the visible edge is real. Revised 2026-11-09 (core-log entry 3): FB_Y is 45, not the 46 this record previously stated. (502 - 412) / 2 is 45, and the driver's own probe line 'co5300: frame painted rc=0, ring cols 66/387 rows 44/445' only reconciles with FB_Y = 45, since those are FB_Y - 1 and FB_Y + FB_H. The running driver was always correct; this record and the README repeated a wrong comment."

- record_id: TCH-001
  kind: TOUCH
  topic_id: TCH
  title: "The touch controller is a CST9217 at I2C 0x1A; 0x5A on the same bus is the DRV2605 haptic driver"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The capacitive controller answers at I2C address 0x1A on the shared bit-banged bus. Register 0x0000 reads 0xCA, the CST92xx checkcode prefix (the family's checkcode format is 0xCACA____). A second device on the same bus answers at 0x5A, and its register 0x00 reads 0xE0 -- bits 7:5 of that byte are the DRV2605 DEVICE_ID field, which is 0b111 for that part. 0x5A is therefore the haptic driver and has nothing to do with touch, even though the vendor header for the CST92xx family lists 0x5A as its address."
  consequence: "Probing 0x5A for touch yields a device that acknowledges and returns plausible-looking bytes, which is how an earlier attempt concluded the touch part was at 0x5A. The discriminator is the unlock handshake, not a single register read: a CST92xx acknowledges the handshake by returning 0x1E in byte 1 of a read of 0x0002, and 0x5A does not. Any future probe of this bus should use that handshake rather than a register read."
  sources:
    - "on the watch: `i2ctransfer -y 0 w2@0x1a 0x00 0x02 r4` returns 0x07 0x1e 0x00 0x00; the same operation at 0x5A returns 0x00 0x00 0x01 0x01"
    - "TinyPetz firmware/bringup/main/touch.c: the note that the part answers at 0x1A and that another device at 0x5A was being mistaken for it"
    - "BRD-001 for the bus and the other addresses on it"
  verification: "Verified on hardware: the 0x1A handshake succeeded on every attempt while 0x5A never returned 0x1E in byte 1. An earlier single-register read of 0x5A returned 0xE0 and was wrongly read as a touch answer; the DRV2605 DEVICE_ID field is what it actually is."

- record_id: TCH-002
  kind: TOUCH
  topic_id: TCH
  title: "A freshly reset CST9217 is in command mode and must be unlocked and put in normal mode before it reports anything"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "After reset the controller sits in command mode, where register reads succeed but it is not scanning the glass. Reading 0x0002 after writing 0xD1 = 0x09 returns 0x00 0x09, the mode byte confirming normal mode. NOTE, revised 2026-11-10: an earlier version of this record stated that a 0xD1 = 0x1E double-write 'unlock' was a required step, with 0x1E in byte 1 of 0x0002 as its acknowledgement. That is not part of the official bring-up. LilyGo's SensorLib never issues it during initialisation -- the 0x1E writes live in setMode(), whose only caller is sleep() -- and the probe that writes the 0x1E sequence then reads 0x0002 and gets 0x00 0x09 on a working part, not 0x1E. See TCH-007 for the sequence that is actually authoritative."
  consequence: "Skipping the mode selection produces a controller that answers every transaction perfectly and never reports a touch, which looks exactly like a working sensor nobody has touched -- a report read in command mode returns 15 bytes of 0xFF. The mode write should be confirmed by reading 0x0002 back rather than assumed, because a controller in the wrong state ignores it silently. The 0xD1 = 0x1E writes are harmless but must not be treated as the acknowledgement of anything: looking for 0x1E in 0x0002 turns a correct part into an apparent failure."
  sources:
    - "lewisxhe/SensorLib src/touch/TouchDrvCST92xx.cpp getAttribute() and setMode(), read in full on 2026-11-10"
    - "on the watch: the sequence issued with i2ctransfer, then `i2ctransfer -y 0 w2@0x1a 0x00 0x02 r4` returning 0x00 0x09 0x00 0x00"
    - "TCH-007 for the authoritative bring-up sequence"
  verification: "Verified on hardware by running the sequence over /dev/i2c-0 with i2ctransfer and observing the 0x09 mode read, then reading a live down report. Corrected 2026-11-10 against LilyGo's source; the 0x1E step was never required and the 0x09 readback is the check that matters."

- record_id: TCH-003
  kind: TOUCH
  topic_id: TCH
  title: "A touch report starts with 0x06, a 0xAB in byte 0 means no report, and the read must be the full 15-byte block"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The report block at 0xD000 is fifteen bytes -- two fingers of five each plus five -- of which the first seven carry everything used: byte 0 the event (0x06 down), bytes 1..3 nibble-packed 12-bit coordinates as x = (b1 << 4) | (b3 >> 4) and y = (b2 << 4) | (b3 & 0x0F), byte 5 the finger count and byte 6 the freshness marker 0xAB. A block whose byte 0 is 0xAB is not a report at all: mainline's CST3xx driver rejects exactly that, on buf[6] != 0xAB || buf[0] == 0xAB. An idle controller returns all-0xFF, which is the same 'nothing new' answer in a different shape. Critically, the TRANSFER must be the full fifteen bytes even though only seven are decoded: the acknowledge that advances the controller's buffer is issued against the block, so acking after a short read leaves the buffer mid-block and it never advances."
  consequence: "Three separate mistakes each cost a round. Treating 0xAB-prefixed blocks as fresh produces phantom presses from an untouched screen, because the block still contains plausible coordinates. Treating a failed read as a release produces flickering buttons, so a failed transfer must skip the poll and let the release timeout decide. And reading seven bytes instead of fifteen freezes a held touch: the first report is taken, then the buffer stops advancing, so a finger held on the glass yields one position, then nothing, then a spurious release. That last one is the defect that made the buttons feel like they had a slow detect, and it was in this project's reader, not the hardware."
  sources:
    - "build/kernel/linux-7.2.4/drivers/input/touchscreen/hynitron_cstxxx.c: CST3XX_TOUCH_DATA_CHK_VAL 0xab, and cst3xx_read_touch() rejecting on buf[6] != 0xab || buf[0] == 0xab"
    - "TinyPetz firmware/bringup/main/touch.c: CST92XX_BLOCK_LEN (MAX_FINGERS * 5 + 5) = 15, the read of the full block, and ack16() issued immediately after it"
    - "on the watch: /home/tbtns.log census counts, which separate transfer failures from idle answers"
  verification: "Verified on hardware: with a finger held on the middle bar the block read 0x06 0x0a 0x1e 0xb6 0x00 0x01 0xab on all 25 samples, decoding to raw x=171 y=486. Revised 2026-11-09 (core-log entry 4): the earlier '2 in 8 reads succeed' figure was measured against the seven-byte transfer and was therefore partly this project's own bug, not a property of the part. With the full fifteen-byte block and the ack issued immediately, a 20 s sample showed 6 transfer failures in about 314 polls, roughly 2 percent, with zero ack failures; the remaining all-0xFF blocks are the controller answering 'nothing new'. A frozen held-touch disappeared at the same time."

- record_id: TCH-004
  kind: TOUCH
  topic_id: TCH
  title: "The strip threshold must sit above the painted bar edge, because the touch Y axis reads a few pixels high near the bottom"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The buttons are painted from panel row 445, but taps landing on a bar routinely decode to panel Y in the 441..444 range. In a ten-tap run against a threshold of 445, two taps that were plainly on a bar decoded to ctrl_y 442 and 443 and were discarded as outside the strip."
  consequence: "A strip threshold set exactly at the painted edge loses taps at the top of the bar. This project sets it to 440, five rows of tolerance, which costs at most the last few console rows being read as a press and buys the property the buttons exist for: they are regions, not targets. The error is consistent with the touch controller's own calibration residual, which TinyPetz measured at roughly 4.6 px RMS on Y, and it is worst at the extremes of the glass."
  sources:
    - "userspace/tbtns.c: the y_strip default and the note explaining the five-row gap"
    - "TinyPetz firmware/bringup/main/touch.c: TOUCH_CAL_YS and the stated fit residual"
  verification: "Measured on hardware: in a ten-tap run with the threshold at 445, two taps produced no press and their decoded values were ctrl_y 442 and 443 while both x values were solidly inside a band. Re-running with the threshold at 440, the same blind spot did not recur."

- record_id: TCH-005
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The CST9217 reset line is P08 of the XL9555 expander, and port 1 of that expander comes up as all inputs, so nothing drives reset at boot"
  statement: "The touch controller's reset is not an SoC GPIO. It is P08 of the XL9555 at I2C 0x20 -- expander port 1, bit 0. Port 1's configuration register reads 0xFF at boot (all inputs) and the output register reads 0xFF, so the line is an input held high by the pin's default and no driver ever pulses it."
  consequence: "A controller whose reset has never been driven answers on the bus and returns 15 bytes of 0xFF from the report register, which reads as a dead part or a bad transfer rather than as 'never initialised'. The expander's configuration register must be made an output for bit 0 (0xFF -> 0xFE) before reset can be pulsed, and the pulse is low 20 ms followed by high 60 ms. The kernel driver does this through devm_gpiod_get_optional(\"reset\") with a reset-gpios phandle to the expander, so probe waits for the expander instead of writing to an undriven line."
  sources:
    - "Xinyuan-LilyGO/LilyGoLib src/LilyGoWatchUltra.cpp initTouch(): io.digitalWrite(EXPANDS_TOUCH_RST, LOW); delay(20); ... HIGH; delay(60); and setPins(-1, TP_INT), the -1 being the reset pin because it is not an SoC GPIO"
    - "on the watch: `i2ctransfer -y -f 0 w1@0x20 0x07 r2` returning 0xfe 0xff after programming bit 0, and 0xff 0xff before it"
  verification: "Verified on hardware 2026-11-10: with port 1 configured as all inputs the report register returned 15 x 0xFF; after setting bit 0 of the configuration register to output and pulsing reset low 20 ms then high 60 ms, the checkcode, resolution and chip type all read correctly and the part reported touches."

- record_id: TCH-006
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The CST9217 reports 410x502 and those are panel coordinates directly: no axis swap, no mirroring, no scale"
  statement: "Reading the resolution register 0xD1F8 returns four bytes, X then Y, little-endian: 0x9a 0x01 0xf6 0x01 is X=410 and Y=502. That is exactly PANEL_W x PANEL_H, so controller coordinates are panel coordinates. A touch at raw (82,419) reports panel (82,419)."
  consequence: "No calibration constants are needed and none should be introduced. The fb0 window is a 320x400 rectangle inset 45 px per side on a 502 px-wide panel, so consumers subtract FB_X/FB_Y (45, 45) to get framebuffer coordinates and clamp to 0..319 / 0..399. Clamping is required rather than optional: a press on the border ring outside fb0 legitimately decodes to a negative or out-of-range value -- measured fb=(8,408) for a press below the window -- and must not be reported to fb0 consumers as if it were inside. The driver publishes the full 0..410 / 0..502 range so that consumers can tell the difference between the border and the window."
  sources:
    - "on the watch: `i2ctransfer -y -f 0 w2@0x1a 0xD1 0xF8 r4` -> 9a 01 f6 01"
    - "lewisxhe/SensorLib src/touch/TouchDrvCST92xx.cpp getAttribute(): resolutionX = buffer[1]<<8|buffer[0], resolutionY = buffer[3]<<8|buffer[2]"
    - "LCD records for PANEL_W = 410 and PANEL_H = 502 and the 45 px safe inset"
  verification: "Verified on hardware 2026-11-10 against the panel geometry: the reported resolution equals the panel dimensions, and a captured drag advanced through raw (258,309) (262,299) (267,289) (280,270), a smooth path with no swap or mirror applied."

- record_id: TCH-007
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The official bring-up is four attribute reads in command mode, and it does not use a 0xD1=0x1E unlock handshake"
  statement: "LilyGo's own driver, SensorLib TouchDrvCST92xx::getAttribute(), brings the part up as: reset; delay 30 ms; write 0xD1=0x01 (command mode); delay 10 ms; read 0xD1FC for the 4-byte checkcode and require (checkcode & 0xFFFF0000) == 0xCACA0000; read 0xD1F8 for resolution; read 0xD204 for the chip type and require 0x9217 or 0x9220; read 0xD208 for an 8-byte firmware version and reject 0xA5A5A5A5. There is no 0xD1=0x1E unlock step anywhere in that path."
  consequence: "The 0xD1=0x1E double-write recorded earlier as required bring-up is part of setMode(), whose only caller in the official driver is sleep(). It was never part of initialisation. Running it is harmless but it is not what makes the part report, and treating it as the acknowledgement led to probing for 0x1E in register 0x0002 when the authoritative checks are the checkcode, chip type and firmware reads. Separately, a report read taken while the part is still in command mode returns 15 bytes of 0xFF, so normal mode (0xD1=0x09) with a 0x0002 readback of 0x09 is still worth doing before trusting a report -- but as a state check, not as an unlock."
  sources:
    - "lewisxhe/SensorLib src/touch/TouchDrvCST92xx.cpp getAttribute() and setMode(); the latter's only caller in that file is sleep()"
    - "on the watch: 0xD1=0x01 then 0xD1FC -> 20 4e ca ca, 0xD1F8 -> 9a 01 f6 01, 0xD204 -> 67 5a 20 92 (chip 0x9220, ProjectID 0x5a67), 0xD208 -> fw 0x01000003, and mode readback 0x0002 -> 00 09"
  verification: "Verified on hardware 2026-11-10: the four attribute reads returned a valid checkcode, 410x502, chip type 0x9220 and firmware 0x01000003, and the part then reported touches. The kernel driver performs these checks and fails probe on any mismatch rather than continuing hopefully."

- record_id: TCH-008
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-10
  title: "Only event 0x06 is a press, byte 6 must be 0xAB for the block to be valid, and the part emits no release event at all"
  statement: "A report block is 15 bytes at 0xD000: five header bytes then two 5-byte finger records, the second record starting at offset 7. Byte 6 is a validity marker that must be 0xAB. Byte 5's low seven bits are the finger count; more than 2 is invalid. A finger record is event in bits 3:0 and id in bits 7:4 of its first byte, then x = (b1<<4)|(b3>>4) and y = (b2<<4)|(b3&0x0F). Only event 0x06 counts as a press. There is no release event: event 0x07 exists in the protocol but the part does not send it when a finger lifts, it simply stops reporting."
  consequence: "A non-zero finger count with event 0x00 is not a touch. Accepting it invents a phantom press at a stale position -- observed as raw (41,451) with nothing on the glass. Release must be synthesised from a gap in reports, which is why the driver has a 250 ms timeout, set to about four of the controller's ~66 ms report periods so that one dropped report does not break a drag. Every report read must also be acknowledged with a separate write of 0xD0 0x00 0xAB; skipping the ack leaves the buffer un-advanced and the same block is returned for ever, so a release is never seen."
  sources:
    - "lewisxhe/SensorLib src/touch/TouchDrvCST92xx.cpp getTouchPoints(): the buffer[6] != 0xAB reject, the 0x7F mask on buffer[5], the per-finger decode, and `if (event == 0x06)`"
    - "on the watch: a block 00 02 1c 93 00 01 ab 00 00 00 3c 05 ab 00 00 with event 0x00 and nothing touching, and live presses returning 0x06"
  verification: "Verified on hardware 2026-11-10: with the event check absent the probe counted a phantom point while nothing was touching; with it present the count was zero. Presses then produced event 0x06 blocks that decoded to a smooth path, and the kernel input driver emitted BTN_TOUCH 1 and 0 correctly with ABS_X/ABS_Y advancing between them."

- record_id: KRN-008
  kind: KERNEL
  topic_id: KRN
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The kernel's interrupt descriptor table is 33 entries and the board exhausts it, so later drivers cannot map a GPIO interrupt at all"
  statement: "With CONFIG_SPARSE_IRQ unavailable on xtensa, arch/xtensa/include/asm/irq.h sizes the descriptor table as NR_IRQS = XCHAL_NUM_INTERRUPTS + PLATFORM_NR_IRQS + 1. XCHAL_NUM_INTERRUPTS is 32 on this core and PLATFORM_NR_IRQS is 0, so the table holds 33. That is enough for peripherals hanging directly off the CPU's interrupt lines, and not enough for a SoC whose GPIO controller is itself an interrupt controller mapping one Linux IRQ per GPIO line a driver requests, with a virtual domain (the AXP2101's) on top. Measured on this board: lines 1 and 7 mapped (the RTC and the PMU, which probed while slots were free) while every later request returned -ENXIO."
  consequence: "The failure is silent and thoroughly misleading. irq_create_mapping() returns ENOSPC, gpiod_to_irq() turns that into -ENXIO, and a driver that treats its interrupt as optional falls back to polling and works -- so the visible symptom is 'this board's GPIO interrupts do not work' rather than 'the table is full'. It is also line-independent, which is the clue worth remembering: probing seven different lines gave irq 7 and 8 for the two already mapped and -ENXIO for the other five, so the discriminator was prior use, not the line. Both of the interrupts that do exist have zero counts in /proc/interrupts, because mapping a line is not enough -- the parent has to fire, and their parents never do on this board. The fix is a larger table: 128 extra descriptors, about 18 KB of image, in patches/kernel/irq/final/irq.h. CONFIG_PLATFORM_NR_IRQS cannot be used because it is a hidden symbol and olddefconfig recomputes it back to 0."
  sources:
    - "arch/xtensa/include/asm/irq.h: NR_IRQS = XTENSA_NR_IRQS + PLATFORM_NR_IRQS + 1, with XTENSA_NR_IRQS = XCHAL_NUM_INTERRUPTS"
    - "arch/xtensa/Kconfig: config PLATFORM_NR_IRQS is a plain int with no prompt, so it cannot be set from a defconfig"
    - "patches/kernel/irq/final/irq.h: EXTRA_NR_IRQS 128"
    - "on the watch, before the fix: `line 0 -> irq -6`, `line 1 -> irq 7`, `line 6 -> irq -6`, `line 7 -> irq 8`, `line 12 -> irq -6`, `line 16 -> irq -6`, `line 17 -> irq -6`"
  verification: "Verified on hardware 2026-11-10. Before the fix, gpiod_to_irq() on the touch controller's INT returned -ENXIO and the driver reported 'polling only'. After it, the same call returned irq 33 -- the first number past the old 33-entry limit -- the driver reported 'interrupt driven', and /proc/interrupts gained `33: esp32xx-gpio 12 cst92xx`, which then accumulated 351 interrupts across three taps while the bus stayed healthy."

- record_id: TCH-009
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The CST9217 INT line is GPIO 12, and driving touch from it cuts idle bus traffic about 8x"
  statement: "TP_INT is GPIO 12 on this board, active low, pulsed when the controller has a report. LilyGo configures it as INPUT_PULLUP and attaches a FALLING-edge handler. The pull-up is part of the contract: the controller drives the line low and releases it, so without the pull the line floats and the edge either never arrives or arrives continuously. In the kernel driver the line is requested with `int-gpios` and turned into an interrupt with gpiod_to_irq() rather than through an `interrupts` property, which is the idiom for input devices and also lets the pin's electrical configuration be set explicitly."
  consequence: "The interrupt is used to wake the driver, and polling is kept at two rates rather than removed: 20 ms while a finger is down, so a drag keeps up with the controller's ~66 ms reports, and 250 ms when idle as a safety net. That structure is deliberate. Making the interrupt the only wake-up would mean a wrong polarity or a lost pull-up silently loses touch entirely, whereas a slow poll degrades to latency. This shape is now implemented and measured on two different stacks and the two idle figures do NOT agree, which is worth knowing before either is quoted. In the Linux kernel driver idle interrupts settled at about 2 per second, against the ~50 reads per second that the previous unconditional 20 ms poll produced. In the TinyDesk port the idle interrupt count is exactly ZERO: `irq1488` was unchanged across 77 s of an untouched glass, while the port read the report block 4 times a second on its idle timer. So neither figure should be generalised -- 2 per second is what the kernel driver saw, 0 is what ESP-IDF sees on this same board, and the difference is unexplained. Two candidates are recorded rather than chosen: the kernel driver's own polling produces some I2C activity the part answers with a pulse, or the two stacks leave the controller in different internal states. Anyone quoting an idle interrupt rate for this board should say which stack measured it."
  sources:
    - "Xinyuan-LilyGO/LilyGoLib src/LilyGoWatchUltra.cpp initTouch(): pinMode(TP_INT, INPUT_PULLUP); attachInterrupt(TP_INT, ..., FALLING)"
    - "patches/kernel/touch/final/cst92xx.c: cst92xx_irq(), the idle/active periods, and cst92xx_service() shared by both paths"
    - "KRN-008 for why the interrupt could not be mapped until the descriptor table was enlarged"
    - "tinydesk-twatch ports/twatchultra/main/touch.c: the same two-rate structure on ESP-IDF, with the ISR only notifying the task"
  verification: "Verified on hardware 2026-11-10: IRQ 33 registered against cst92xx, 331 interrupts accumulated during three taps, and a captured event stream showed three presses and three releases with smooth coordinate tracks. Idle rate measured at 20 interrupts over 10 seconds. The bus, RTC, PMU, panel, WiFi and BLE keyboard all remained healthy throughout. Revised 2026-11-12 (core-log entry 18): the ESP-IDF port implemented the same structure and measured the idle interrupt count at zero rather than 2 per second, over 77 s of untouched glass. The pin, the polarity, the pull-up, the falling edge and the two poll rates are all confirmed a second time on a different stack; only the idle figure differs, and this record no longer presents one number as the board's behaviour."

- record_id: TCH-010
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The touch interrupt, not the poll, is what discovers a touch: with a 5 s idle poll every tap still registers"
  statement: "With `hynitron,idle-poll-ms = <5000>` the driver reports `idle 5000 ms, active 20 ms, release 250 ms, interrupt driven`, and eight deliberate taps one second apart produced eight presses and eight releases in the captured input event stream, for 255 interrupts on IRQ 33."
  consequence: "This is what makes the interrupt a proven mechanism rather than an assumed one. The test is designed so the poll cannot be credited: an idle poll of 5 s against a tap that is pressed and released within a fraction of a second means a dead interrupt would usually sample the controller after the finger had already gone, so it would report nothing -- missing most or all of the taps. Eight out of eight registered, which the poll could not have produced. The long idle period is therefore safe to ship, and it is what makes the interrupt's benefit real rather than nominal: idle bus traffic is about 2 reads per second instead of the ~50 an unconditional 20 ms poll generated. The idle poll is deliberately kept rather than removed, at 5 s, as a safety net: if the INT line ever stops asserting, touch degrades to slow instead of disappearing, and a slow tap is a recoverable failure while a dead screen is not."
  sources:
    - "patches/kernel/display/final/esp32s3-devkit-c1.dts: the hynitron,idle-poll-ms = <5000> property and the note on why 5 s rather than 250 ms"
    - "patches/kernel/touch/final/cst92xx.c: device_property_read_u32(dev, \"hynitron,idle-poll-ms\", ...) and the idle/active period selection in cst92xx_irq() and cst92xx_poll()"
    - "TCH-009 for the interrupt itself and KRN-008 for why it could not be mapped until the descriptor table was enlarged"
  verification: "Verified on hardware 2026-11-10: kernel reported idle 5000 ms and interrupt driven; a capture of /dev/input/event2 across eight taps contained 48 events decoding to 8 presses and 8 releases with coordinates clustering in one region of the glass (x 158..191, y 208..252), while /proc/interrupts advanced to 255 on IRQ 33."

- record_id: LCD-008
  kind: DISPLAY
  topic_id: LCD
  title: "A 45x45 window is honoured, and an RGB565 colour needs 63 in the green field to be white"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "LCD-002 established that a window one pixel wide or tall draws nothing while a 432x31 strip draws, leaving the threshold between those unmeasured. It is now measured at the useful end: 45x45 windows draw correctly, and so do 45x315, 410x45 and 410x57. Separately, in RGB565 the red and blue fields are 5 bits (0-31) but green is 6 bits (0-63), so RGB565(31, 31, 31) is full red, HALF green and full blue -- which is pink, not white. 0xFBFF, not 0xFFFF."
  consequence: "The narrow-window limit does not bite the edge API: 45 px tiles are fine, so the full-width band fallback in docs/panel-api.md is not needed and the reserved regions can be written directly. The colour trap is the dangerous one, because it produces a plausible-looking wrong answer rather than a failure. A test pattern built with RGB565(31, 31, 31) for its white markers drew pink, and that was read as a driver fault on the glass before the arithmetic was checked -- the API was correct throughout. Any colour constant in a drawing test must be checked against the field widths before the result means anything."
  sources:
    - "userspace/panel-test.c: the WHITE/BLACK/RED/GREEN/BLUE constants and the note on why green needs 63"
    - "patches/kernel/display/final/co5300.c: CO5300_IOC_WRITE_RECT, which is what was exercised"
    - "LCD-002 for the degenerate-window result this extends"
  verification: "Verified on the glass, which is the only admissible evidence for this panel. Solid primaries drawn one per region read back as red on the left, green on the right and blue at the bottom, with the top strip white only after the green field was corrected to 63 -- it read pink before. Tile, tall-strip and full-width rectangles all drew at the coordinates requested, with fb0 untouched."

- record_id: LCD-009
  kind: DISPLAY
  topic_id: LCD
  title: "A long single-transfer window to the right-hand strip tears into diagonal splits; 2-row bands do not"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A single 45x356 transfer into the right-hand strip returns as eight diagonal splits, worst at the bottom, and the splits are visible even in a uniform fill -- so whole rows did not receive the write rather than being torn mid-scan. The same rectangle sent as 178 windows of 45x2 is clean, repeatably. The failure is not explained by width, by X position, or by window height. Width is excluded because the behaviour is non-monotonic: with five stacked segments from the same origin, 45 wide tore, 35 wide was clean, and 25, 15 and 5 wide all tore. X position is excluded because nine 45x50 windows swept across the bottom strip at x = 0, 45, ... 360 were all clean, including one at controller columns 382..426, overlapping the right strip's 387..431. Height is excluded because the 45x71 segments also tore. The left strip tolerates the same 45x356 single transfer that the right strip does not."
  consequence: "The rule that holds on hardware is that a single transfer must carry few rows, so `CO5300_IOC_WRITE_RECT` caps every band at two rows and loops. That is invisible to callers -- userspace still hands over one rectangle -- and it is cheap here because these are status icons, but it is a workaround and not a fix: it would be wrong for continuous drawing, which is why `co5300_blit()` keeps its own uncapped sizing. The root cause is NOT established, but SPI clock margin is now excluded as the explanation for the reference firmware's own 45 MHz shearing: that signature is also what a write-pointer desync looks like, and the reference firmware carried the SPI_USR_COMMAND/SPI_USR_ADDR-resent-per-chunk bug that restarts the write pointer every 64 bytes. That bug is fixed here, and the panel has since been tested at 40 MHz with eight full-width bands in eight distinct colours, which came out clean and in order -- a desync cannot produce that. 40 MHz is now the compiled-in default. What remains unexplained is this record's own narrow-transfer fragility, which persists at the higher clock and is therefore not about the clock."
  sources:
    - "patches/kernel/display/final/co5300.c: CO5300_RECT_ROWS_PER_BAND and the record of what is and is not established"
    - "userspace/panel-test.c: the -D, -T, -W and -C modes that produced these measurements"
    - "patches/kernel/display/NOTES provenance on the 45 MHz shearing at 20 MHz in the reference firmware"
  verification: "Measured on the glass across six test patterns, changing one variable per pattern. The decisive pair: one 45x356 transfer tears into eight diagonal splits; the identical rectangle in 2-row bands is clean, confirmed repeatedly. After the driver cap was applied, userspace sending the same single rectangle produced a clean uniform strip."

- record_id: LCD-010
  kind: DISPLAY
  topic_id: LCD
  title: "Panel transfers cost 11.4 us per 64-byte chunk with no per-transfer fixed cost, and the panel itself is only 1.6 percent of that"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Timing the same pixel payload twice with `CO5300_IOC_BENCH` -- once with CS asserted so the panel receives it, once with CS never asserted so the peripheral clocks the bytes out into nothing -- separates the panel's contribution from the driver's. A 288x2 payload (18 chunks of 64 bytes) costs 197 us with CS and 193 us without; a 288x64 payload (576 chunks) costs 6572 us and 6470 us. Fitting those two points gives 11.42 us per 64-byte chunk and a per-transfer fixed cost of -8.6 us, which is zero within noise. The panel therefore accounts for 0.18 to 0.22 us per chunk, about 1.6 percent of transfer time. Throughput is 5.6 MB/s against 10 MB/s of quad-lane wire capacity at 20 MHz, so 56 percent."
  consequence: "Two things follow for anyone deciding what to optimise. Per-transfer overhead is not worth designing around: command, address, CS and the completion poll together cost less than noise, so a dirty-rectangle scheme pays no per-call penalty and splitting a frame into many small transfers is cheap in fixed cost. And the remaining 5 us per chunk is CPU work -- 16 memcpy_toio writes into a 64-byte FIFO, a length register write, and a cpu_relax() completion poll -- which is what DMA would remove, capping the gain at about 2x rather than the 5-10x a FIFO-share estimate suggested. The panel is excluded: removing it entirely from the circuit changes the time by 1.6 percent."
  sources:
    - "patches/kernel/display/final/co5300.c: CO5300_IOC_BENCH, which performs the CS-asserted and CS-not-asserted runs"
    - "docs/panel-transfer-cost.md: the measurements, the fit, and the reproduction command"
    - "patches/kernel/display/final/spi-esp32.c: the FIFO-only exec_op that the per-chunk cost comes from"
  verification: "Measured on hardware 2026-11-09 with the transmitted buffer, not a shared-memory estimate. The contradictory figure is recorded rather than resolved: LCD-005 puts a 320x400 frame at about 190 ms, while 4000 chunks at 11.42 us is 45.7 ms, a factor of 4.2 apart. Those do not reconcile and the difference is not explained; LCD-005 predates the CS-handling fix and may have included non-transfer work. LCD-005 should be re-measured through the real repaint path before either number is quoted."

- record_id: BLE-005
  kind: BLE
  topic_id: BLE
  title: "Firmware logging shares the shmem link with wifi bring-up, so logging per advertisement breaks the network"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "`fw_log` sends every line to Linux as an IPC message on the same shmem link the kernel uses for WiFi, and it does so from core 0 while the kernel is still initialising. Adding a `hid_log` call to the BLE scan callback -- one line per advertisement, which in a normal radio environment is dozens per second -- stopped the network interface from coming up: the kernel reported `wait_and_decode_cmd_resp: Command[1] timed out`, then `esp_add_card: esp_add_network_ifaces(adapter) failure, ret: -1`, and no `espsta0`, and no `/dev/esp-log` either, because `esp_log_pipe_init()` runs after the step that failed. Reverting the logging restored both on the next boot."
  consequence: "On this board logging is traffic, not observation. The firmware emitter is rate limited to 40 lines per second precisely because of this, and even that is a budget rather than a guarantee: what matters is how much of the link the lines occupy while wifi is being brought up. Any diagnostic added to the scan or connection path must be either rare by construction or gated so it cannot fire during interface initialisation. The failure is also self-concealing -- the log channel dies with the network, so the log cannot report the fault that killed it."
  sources:
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/log_pipe.c: LOG_MAX_PER_SEC 40 and the note that a flood 'has already been shown to stop Linux reaching userspace'"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/ble_prov.c: ble_prov_log_nonblocking(), which enqueues one IPC message per line"
    - "kernel log on hardware: Command[1] timed out, esp_add_network_ifaces failure, network interface init failed"
  verification: "Reproduced twice on hardware 2026-11-09: the firmware with per-advertisement logging produced no espsta0 and no /dev/esp-log on two consecutive boots, and the same firmware with the logging removed restored both immediately. This is the same mechanism as NET-003."

- record_id: BLE-006
  kind: BLE
  topic_id: BLE
  title: "PSRAM is not a heap on this firmware, and boot-time firmware logs need deferring to be seen"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Two facts that together explain why bond persistence appeared to do nothing for several sessions. First, the firmware is built with `CONFIG_SPIRAM_USE_MEMMAP=y`, so the 8 MiB PSRAM is mapped as address space for the Linux kernel rather than registered as a heap region: `heap_caps_malloc(n, MALLOC_CAP_SPIRAM)` can never succeed on this board, whatever n is. Second, `fw_log()` sends over the shmem pipe and the reader is Linux, so any call made from a path that runs before Linux starts — the NimBLE host-sync callback is the one that matters — is dropped with no trace."
  consequence: "`bond_load()` combined both: it tried to copy the whole 448 KiB etc partition into PSRAM, failed, and returned before opening the file, while its diagnostic was emitted into a pipe nobody was reading. Every previous attempt to fix the load logic was therefore working on a path that never ran. The fixes are a bounded prefix read into internal RAM (16 KiB doubling until the file is found) and `fw_log_defer()` / `fw_log_flush()`, a small fixed pool of boot-time lines released when Linux first speaks. Both are general: any firmware code that needs large scratch space at boot must use internal RAM, and any diagnostic in a pre-Linux path must be deferred or it will not be seen."
  sources:
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/sdkconfig: CONFIG_SPIRAM=y, CONFIG_SPIRAM_MODE_QUAD=y, CONFIG_SPIRAM_USE_MEMMAP=y"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/bond.c: map_bond_partition() and bond_load()"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/log_pipe.c: fw_log_defer() and fw_log_flush()"
    - "docs/bluetooth-bond.md: what works, and the three defects in the order they were found"
  verification: "Verified on hardware 2026-11-09. Before the change the only visible symptom was a bond file that was written and never restored; after deferring the logs, the first boot printed `bond: no room to read etc (458752 bytes)` and named the cause. With bounded internal-RAM reads the load reports `bond: loaded 4 entries from ble-bond.bin (0 rejected)`, and the Rii K06 then reconnected on the stored key without pairing mode and typed."

- record_id: EXT-002
  kind: EXTERNAL
  topic_id: EXT
  title: "The linear flash window at 0x42000000 is readable from userspace, so a ROM there costs no RAM"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "`init_linux_mmap()` extends the SoC's MMU table so the whole 16 MiB of flash is addressable at `0x42000000` before the jump to Linux (KRN-002). That window remains valid once Linux is running, and userspace can read it directly: reading 64 KB through it returned real data and no fault. A 512 KB ROM written raw to flash at `0x480000` is therefore readable at `0x42480000`, and a program that points at it allocates nothing."
  consequence: "This is the answer to the contiguous-allocation problem for ROM images and any other large read-only blob (KRN-001). `malloc` of 512 KB succeeds on a fresh boot and fails once the heap fragments; a flash-window pointer works at any uptime. It matters because the failure mode is an exec-time allocator error that the program cannot report. The slot must be protected: `scripts/build-kernel.sh` now refuses an `xipImage` that would reach `0x480000`, because otherwise a larger kernel would be silently overwritten by the next ROM write rather than failing."
  sources:
    - "linux_ll_esp32s3.c: init_linux_mmap() and map_flash_and_go()"
    - "on the watch: a userspace read of 0x42000000 returned data, and 0x42480000 read back a 512 KB ROM with crc32 301F0E87 matching the file"
    - "scripts/build-kernel.sh: the ROM slot guard"
  verification: "Verified on hardware 2026-11-09. The window was read from userspace and checksummed against the file it held, byte for byte. File mmap was tested as an alternative and rejected: MAP_PRIVATE on the same file costs 1020 KB of RAM, twice the file, while MAP_SHARED fails with ENODEV. The watch ran the emulator at 1.00x realtime with the largest free block at 128 KB, where a 512 KB malloc cannot succeed."

- record_id: EXT-003
  kind: EXTERNAL
  topic_id: EXT
  title: "A fullscreen app on fb0 flickers against the console, because every printk repaints fbcon"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Every `printk` is mirrored to all registered consoles, and on this board fbcon on the CO5300 is one of them. A program drawing to `/dev/fb0` and the console repainting over it therefore take turns on the same panel, which on the glass is a flicker between the app and console text. The trigger here was continuous: the touch daemon polls the CST9217 at I2C 0x1A every 60 ms, the controller NAKs intermittently, and `i2c-gpio` prints `sendbytes: NAK bailout` for each one at KERN_ERR."
  consequence: "`console_loglevel` decides whether this happens at all, so the fix is a sysctl rather than a driver change: `/etc/sysctl.conf` now sets `kernel.printk = 1 4 1 7`, and emergencies only reach the panel. Any fullscreen app on this board needs that, or needs to raise and lower the level itself around its run. It hides the NAK message rather than stopping the NAKs, which are a property of the touch controller (TCH-003). There is a second, independent source that loglevel does nothing about: fbcon blinks the console cursor on a timer and repaints that one cell each time, which lands on a fullscreen app as a small blinking hole. The knob is `/sys/class/graphics/fbcon/cursor_blink`; a fullscreen app should save it, write 0, and restore on exit rather than disabling the cursor for the whole session, because a blinking cursor is correct at a shell prompt."
  sources:
    - "on the watch: `/proc/sys/kernel/printk` reading 7 4 1 7 while the emulator ran, with NAK lines arriving continuously"
    - "docs/tamagotchi.md: the flicker and the sysctl"
  verification: "Observed on hardware 2026-11-09 with the Tamagotchi emulator running: the operator reported the console taking over the frame each time it printed. Dropping the level to 1 stopped it immediately and the game held the panel. The message level was identified by trying 3 first, which did not help because NAK bailout is KERN_ERR."

- record_id: BRD-005
  kind: BOARD
  topic_id: BRD
  title: "The DRV2605 haptic amplifier is at I2C 0x5A behind an XL9555-gated rail, and this unit drives an ERM motor"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A TI DRV2605 haptic driver sits at I2C 0x5A on the shared bit-banged bus. Reading its STATUS register (0x00) gives 0xE0: DEVICE_ID bits 7:5 = 0b111 identifies a DRV2605, and DIAG_RESULT bit 3 is 0, meaning the diagnostics passed. Reading FEEDBACK (0x1A) gives 0x36, whose bit 7 is 0, so the fitted motor is ERM rather than LRA -- which decides the effect library, since library 6 is for LRA and the ERM sets are 1 to 4. The chip's supply is gated by the XL9555 expander at 0x20, port 0 bit 6, which is high at boot. The chip powers up in internal-trigger mode with no library selected and an empty wave sequence, so writing GO (0x0C) does nothing until a library (0x03) and a sequence (WAVESEQ1 0x04, WAVESEQ2 0x05, terminated by 0) have been written."
  consequence: "Two independent things have to be right before a click happens, and both fail silently: the expander bit gates the motor's power, so a chip with the bit low answers every I2C transaction and never moves; and the library must match the motor type, so an ERM given the LRA library buzzes weakly or not at all rather than erroring. Use effect IDs from the selected library -- 1 is Strong Click 100 percent, which is what a button should feel like -- and write the sequence before GO, because GO replays whatever sequence is already loaded. The click belongs on press only; a release click doubles every tap and reads as noise."
  sources:
    - "on the watch: `i2cget -y -f 0 0x5a 0x00` = 0xe0, `0x1a` = 0x36, `0x01` = 0x00, `i2cget -f 0 0x20 0x02` = 0xff"
    - "patches/kernel/display/NOTES provenance and TinyPetz firmware/bringup/main/board.c: EXPANDS_DRV_EN = 6, the expander bit that enables the haptic rail"
    - "userspace/haptic.c: the driver, with the register map and the library choice recorded"
  verification: "Verified on hardware 2026-11-09: the registers were read as above before any code was written, haptic_init() then reported `haptic ready (DRV2605, ERM)` in the daemon log, and the operator confirmed the click is felt when a bar is tapped. TinyPetz was checked first and does not drive the chip at all -- it only raises the expander bit at boot -- so the effect was established here from the datasheet IDs rather than copied."

- record_id: HAP-001
  kind: HAPTIC
  topic_id: HAP
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The tap click belongs in the kernel, because the userspace library that made it died with its only caller"
  statement: "The DRV2605's Strong Click is now produced by a kernel driver, `patches/kernel/haptic/final/drv2605.c`, exposing `drv2605_tap()`, which the touch driver calls on each press transition. The sequence is MODE 0x01 = 0x00 (internal trigger), RTP 0x02 = 0x00, LIBRARY 0x03 = 0x01 (TS2200 A, the ERM set), WAVESEQ1 0x04 = 0x01 (Strong Click 100%), WAVESEQ2 0x05 = 0x00 (the zero that terminates the sequence), then GO 0x0C = 0x01. The sequence is rewritten on every tap rather than once at probe."
  consequence: "This is a regression repair, and the shape of the regression is the lesson. The click used to come from `userspace/haptic.c`, a library whose only caller was the touch daemon; when that daemon was retired the library stayed behind with no callers, so the motor went silent while the file remained in the tree looking maintained. Moving the sequence into the kernel puts it where the button is, so it survives whatever userspace does. Two details are load-bearing and both were inherited from measurement, not from a datasheet: library 6 is for LRA motors and buzzes weakly on this ERM part, and writing GO with a stale sequence replays whatever was in the wave registers, which is how a \"tap\" ends up buzzing for a second. The click must also fire on the press transition only -- the controller re-reports a held finger about every 66 ms, so clicking per report gives a continuous buzz instead of a tap."
  sources:
    - "patches/kernel/haptic/final/drv2605.c: the register sequence, the DEVICE_ID check on STATUS bits 7:5, and drv2605_tap()"
    - "patches/kernel/touch/final/cst92xx.c: the call gated on `if (!ts->touching)` inside cst92xx_report_touch()"
    - "BRD-005 for the device ID, the ERM-vs-LRA decision from FEEDBACK 0x36, and the XL9555 port 0 bit 6 rail"
  verification: "Verified on hardware 2026-11-10. The driver bound and logged 'DRV2605 ready, ERM motor' followed by 'library 1, effect 1 on tap'. Firing the sysfs trigger eight times produced eight distinct felt clicks, confirming the sequence and the motor independently of touch. Tapping the screen five times then produced five felt clicks, and /proc/interrupts advanced 100 interrupts on IRQ 33 over the same window, confirming the touch-to-driver-to-motor path end to end."

- record_id: PMU-001
  kind: PMU
  topic_id: PMU
  title: "AXP2101 capacity is register 0xa4 bits 6:0 and voltage is 0x34/0x35 in mV, while the property is in uV"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "State of charge is register 0xa4, bits 6:0, so 0x64 reads as 100 percent; bit 7 is a validity flag on some revisions and a value above 100 means the gauge has not settled. Battery voltage is a 14-bit value across register 0x34 (high, bits 13:8) and 0x35 (low), already scaled to millivolts: 0x10 0x55 reads as 4181 mV. The power_supply voltage_now property is in microvolts, so the register value must be multiplied by 1000."
  consequence: "There is no IIO fallback for this variant, because drivers/iio/adc/axp20x_adc.c has no AXP2101 channel map, so both values must be read straight from the registers. Reporting the raw millivolt figure in a microvolt property yields 4175 for a 4175 mV battery, which any consumer will read as 4 mV."
  sources:
    - "patches/kernel/axp2101/final/axp20x_battery.c: the CAPACITY and VOLTAGE_NOW cases"
    - "patches/kernel/axp2101/final/axp20x.h: AXP2101_BATT_VOLTAGE_H 0x34, AXP2101_BATT_VOLTAGE_L 0x35, AXP2101_BATT_PERCENT_DATA 0xa4"
  verification: "Measured on hardware: capacity 100, voltage_now 4175000 uV, status Full, usb online 1. The units bug was caught by reading the property back and seeing 4175 where 4175000 was expected."

- record_id: PMU-002
  kind: PMU
  topic_id: PMU
  title: "The AXP2101 has no mainline driver, and a non-recursive patch glob silently dropped this project's entire series"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "No AXP2101 driver exists in mainline Linux, so this project carries a pending series plus local fixes under patches/kernel/axp2101/. The kernel build applied patches with the glob `patches/kernel/*.patch`, which does not recurse into that subdirectory, and wrapped each application in 'git apply ... || true', which discarded the failure. A rebuild therefore produced a kernel with no AXP2101 MFD, regulator or battery driver and reported success; the device tree still described the PMU, and the only trace of it was the compatible strings, which nothing bound to."
  consequence: "A missing driver is silent: /sys/class/power_supply is simply empty while the DTB still names the parts. Any claim that a peripheral works must be checked against the running system, not against the device tree or the build log. The build now recurses one level and reports a patch that fails to apply instead of swallowing it."
  sources:
    - "scripts/build-kernel.sh: the patch loop and its comment recording both silent losses"
    - "patches/kernel/axp2101/README.md"
  verification: "Found on hardware: /sys/class/power_supply empty and no axp2101_irq_chip in /proc/interrupts despite README claiming the PMU worked; `grep -c axp2101 drivers/mfd/axp20x.c` returned 0 in the built tree. After fixing the glob and enabling CONFIG_AXP20X_POWER, both supplies and four interrupts appeared."

- record_id: PMU-003
  kind: PMU
  topic_id: PMU
  title: "CONFIG_AXP20X_POWER is what publishes axp20x-usb, and it is off by default"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The USB power-supply driver for the AXP20x family is gated on CONFIG_AXP20X_POWER, which depends on MFD_AXP20X and IIO and is not set in a minimal configuration. With it off, the PMU binds and publishes axp20x-battery, but /sys/class/power_supply/axp20x-usb does not exist. Enabling it registers the supply and its two VBUS interrupts."
  consequence: "Its absence is silent, because the battery supply still appears and nothing looks wrong. The usb supply is what makes VBUS hotplug observable at all, so any work on charging state or wake-on-power depends on this option being set."
  sources:
    - "drivers/power/supply/Kconfig: config AXP20X_POWER depends on MFD_AXP20X and IIO"
    - "patches/kernel/config/devkit_c1_16m_linux.config: CONFIG_AXP20X_POWER=y with the reason recorded"
  verification: "On hardware: axp20x-usb absent before enabling the option, present with online=1 after, and /proc/interrupts then showed lines 23 and 24 named axp20x-usb-power-supply."

- record_id: RTC-001
  kind: RTC
  topic_id: RTC
  title: "The PCF85063A registers as rtc0 on the same bit-banged bus and seeds the system clock at boot"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A PCF85063A at I2C address 0x51 registers as rtc0 and, with CONFIG_RTC_HCTOSYS set, its stored time seeds the kernel's clock before init runs. The kernel logs 'rtc-pcf85063 0-0051: registered as rtc0' followed by 'setting system clock to ... UTC'. Its interrupt resolves to the same GPIO bank as the PMU line, appearing as 'esp32xx-gpio 1 pcf85063'."
  consequence: "The clock is available to userspace from the first second of boot rather than waiting for a network time source. This project writes the system clock back to the chip at shutdown, so a correction made during a session survives the next boot. The factory had set the chip to UTC+8, and that correction has been applied: the chip now holds true UTC, confirmed against the host clock to within two seconds."
  sources:
    - "patches/kernel/0001-dts-watch-i2c-pins-and-rtc.patch"
    - "boot log on hardware: 'rtc-pcf85063 0-0051: registered as rtc0' and the hctosys line"
    - "README.md, RTC section"
  verification: "Observed in every boot log. The write-back loop was verified twice: first by corrupting the chip on purpose and confirming the next boot adopted the corrected system clock, and again on 2026-11-09 by skewing the system clock +300 s, rebooting with no manual write, and finding the chip holding the skewed value (18:06:36 against true 18:01:38) while hctosys then seeded from it. Revised 2026-11-09 (core-log entry 2): the chip now holds true UTC, so the standing UTC+8 correction is no longer outstanding."

- record_id: RTC-002
  kind: RTC
  topic_id: RTC
  title: "The image ships no timezone data; /etc/TZ containing MST7 is what makes `date` read local time"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The image contains no `/etc/TZ`, no `/etc/localtime` and no `/usr/share/zoneinfo`. The kernel's stored UTC can be exactly right while `date` still prints UTC, because nothing tells userspace what offset to apply. Placing the POSIX TZ string `MST7` in `/etc/TZ` is sufficient: uClibc-ng reads that file whenever `TZ` is unset, which is the state of the console's shells. `MST7` means UTC-7 for the whole year with no DST, which is America/Phoenix; `MST7MDT` would apply a one-hour DST offset and be wrong for all of summer."
  consequence: "This is a separate defect from the clock being correct. A watch whose chip holds perfect UTC still shows the wrong time on the glass until `/etc/TZ` exists. The file must contain the TZ string and nothing else -- neither uClibc-ng nor glibc parses comments there, so a comment becomes part of the zone name. It is installed by `scripts/watch-time.sh` and is unlisted in `S03keepconfig`, so a full-image flash loses it."
  sources:
    - "userspace/TZ (repo artifact: the single line `MST7`)"
    - "scripts/watch-time.sh: the install path and the MST7 reasoning"
    - "on the watch: `ls /etc/TZ /etc/localtime /usr/share/zoneinfo` all report No such file or directory"
  verification: "Verified on hardware: with `TZ` unset and `/etc/TZ` absent, `date` printed '2026-11-09T17:46:20 UTC +0000'; after writing `MST7`, the same command printed '2026-11-09T10:47:07 MST -0700' against a true UTC of 17:47:07."

- record_id: RTC-003
  kind: RTC
  topic_id: RTC
  title: "The upstream image ships a stamp pseudo-clock (S01clock) that can overwrite the RTC-seeded time at boot"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "`/etc/init.d/S01clock` ships in the upstream image and is not a clock. At shutdown it writes the epoch to `/home/.clock`; at boot it overwrites the system clock with that note whenever the note is newer than the current clock. It sorts after `hctosys`, which seeds from the RTC about 5.4 s into boot, so the note has the last word. `/sys/class/rtc/` contains `rtc0` and nothing else, resolving to `platform/i2c0/i2c-0/0-0051/rtc/rtc0` -- the board has exactly one timekeeper, so this is a second mechanism, not a second clock. It also costs one jffs2 write per shutdown."
  consequence: "Left enabled, boot time can come from the previous shutdown rather than from the chip, which masks a dead or drifting RTC and silently discards any correction NTP made during the last session. This project disables it so the PCF85063A is the single authority; `scripts/watch-time.sh` moves it to `/etc/disabled/S01clock` and deletes `/home/.clock`. Disabling it is deliberately not a rename: `rcS` and `rcK` both iterate `for i in $(ls /etc/init.d/S??*)`, and that pattern is S, exactly two characters, then anything, so `S01clock.disabled` and `S01clock.off` both still match and still run. Only a file outside `/etc/init.d` is actually stopped."
  sources:
    - "on the watch: `cat /etc/init.d/S01clock`, and `cat /etc/init.d/rcS` / `rcK` for the invocation loop"
    - "`ls /etc/init.d/S??*` behaviour checked in a scratch directory: S01clock, S01clock.disabled, S01clock.off and S20foo all matched"
    - "scripts/watch-time.sh: disable_stamp_clock()"
  verification: "Verified on hardware: with `/home/.clock` set 90 minutes ahead, `/etc/init.d/S01clock start` printed 'Clock restored to 2026-11-09 19:42:56 UTC' and the clock moved from 18:12:58 to 19:42:56 against a true 18:12:59. After the disable, a shutdown-and-reboot cycle left `/home/.clock` absent -- `rcK` would have recreated it had the script still been in the glob -- with `uptime` 54 s and hctosys seeding true UTC."

- record_id: RTC-004
  kind: RTC
  topic_id: RTC
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The RTC's interrupt path works; its zero count meant no alarm had ever been armed, not a broken line"
  statement: "The PCF85063A's interrupt is GPIO 1 on the esp32 GPIO controller, active low, and it maps and fires correctly. Arming an alarm through `/sys/class/rtc/rtc0/wakealarm` and waiting for it advanced `7: esp32xx-gpio 1 pcf85063` from 0 to 1. Control_2 (0x01) reads 0x07 at rest, in which bit 7 (AIE, alarm interrupt enable) and bit 5 (TIE, timer interrupt enable) are both clear -- so the INT pin cannot assert until an alarm is armed."
  consequence: "This record exists to stop the zero being misread. A permanently-zero count in `/proc/interrupts` looks like a dead interrupt, and that is how it was previously interpreted, but for this part the default state is genuinely 'no interrupt source enabled': nothing schedules an alarm on this watch, so the line is silent by design rather than by fault. Note that 0x07 also clears bit 5, the STOP bit, so the oscillator is running -- the two facts share a register and only one of them is about interrupts. Anyone re-checking this in future should arm an alarm rather than reading the count and concluding anything."
  sources:
    - "on the watch: `i2ctransfer -y -f 0 w1@0x51 0x00 r3` -> 0x00 0x07 0xa2, and Control_2 bit 7 AIE = 0, bit 5 TIE = 0"
    - "on the watch: `echo <epoch> > /sys/class/rtc/rtc0/wakealarm`, then `grep pcf85063 /proc/interrupts` advancing 0 -> 1"
    - "drivers/rtc/rtc-pcf85063.c: pcf85063_rtc_set_alarm() writes the alarm registers then sets PCF85063_CTRL2_AIE only when alrm->enabled"
  verification: "Verified on hardware 2026-11-10: with no alarm armed the count sat at 0 through many boots; arming wakealarm 20 s ahead produced `7: 1 esp32xx-gpio 1 pcf85063`, proving alarm, AIE, the INT pin, the GPIO controller, the chained handler and the driver all work together. No code change was needed -- the path was never broken."

- record_id: PMU-004
  kind: PMU
  topic_id: PMU
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The PMU interrupt path works, and its previous zero count was simply 'no event had happened'"
  statement: "The AXP2101's interrupt is GPIO 7, active low, and it fires correctly. A quick tap of Pusher A -- the power key, which is not a GPIO and reaches Linux only through `axp20x-pek` -- advanced `8: esp32xx-gpio 7 axp2101_irq_chip` from 0 to 2 and both of its sub-sources from 0 to 1: `axp20x-pek-dbr` (0x11, rising edge = press) and `axp20x-pek-dbf` (0x12, falling = release). The PMU's pending-status register confirmed it, reading `00 08 00` with bank 1 bit 3 set, which is PEK_SHORT."
  consequence: "Two things follow, one of which corrects an earlier reading of this board. First, the interrupt was never broken; the count was zero because nothing had asserted it, and the AXP2101's default enable mask leaves only the PEK edges and VBUS plug/removal unmasked (0x40..0x42 read `00 c3 00`, exactly the four sub-IRQs the driver registers), so a quiet period is the expected state. Second, the DTS comment warning that a LEVEL_LOW interrupt on GPIO 7 'storms and hangs the boot' is now demonstrably wrong: pin 7 is configured with a weak pull-up in `pmic_irq_pins` and driven level-low, and two presses later the kernel is healthy with the handlers serviced. The comment also contradicts its own node, which does declare the interrupt. Whatever originally hung was not this configuration as it now stands."
  sources:
    - "on the watch: `grep axp /proc/interrupts` showing 8 -> 2, 17 -> 1, 18 -> 1 after one Pusher A tap"
    - "on the watch: `i2ctransfer -y -f 0 w1@0x34 0x48 r3` -> 00 08 00 (bank 1 bit 3, PEK_SHORT)"
    - "on the watch: `i2ctransfer -y -f 0 w1@0x34 0x40 r3` -> 00 c3 00, the enabled mask 0x41 = bits 0,1,6,7"
    - "patches/kernel/axp2101/final/axp20x.c: axp2101_regmap_irqs[] and axp2101_regmap_irq_chip's unmask_base = AXP2101_IRQ0_EN"
    - "patches/kernel/display/final/esp32s3-devkit-c1.dts: the pmic@34 node and its stale warning comment"
  verification: "Verified on hardware 2026-11-10. Before: all five PMU-related counts 0 and status 00 00 00. After one tap of Pusher A: IRQ 8 at 2, both PEK sub-IRQs at 1, status bank 1 bit 3 set. The press/release pair appearing separately confirms both edges are delivered, not just one, and the kernel remained healthy throughout."


- record_id: PMU-005
  kind: PMU
  topic_id: PMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "AXP2101 charge registers, and this board's power-on charge defaults are already correct for its 1100 mAh cell"
  statement: "The AXP2101's charging is configured through six registers, decoded here from lewisxhe/SensorLib's AXP2101Charger, which is the driver LilyGo's own firmware uses. 0x18 is the module enable, where bit 1 enables the cell charger and bit 3 the fuel gauge; 0x30 bit 1 enables TS pin measurement for an NTC battery thermistor; 0x61 holds the precharge current in the low nibble at 25 mA per step; 0x62 holds the fast-charge current in bits 4:0, where a code of 8 or less means code x 25 mA and anything above 8 means 200 + (code - 8) x 100 mA; 0x63 holds the termination current in the low nibble at 25 mA per step; and 0x64 bits 2:0 select the full-charge voltage from six fixed steps, 0 = 5000 mV, 1 = 4000, 2 = 4100, 3 = 4200, 4 = 4350, 5 = 4400, with 6 and 7 undefined. On this watch, measured 2026-11-12 and never written by any code in the TinyDesk port: 0x18 = 0x0a, 0x30 = 0xff, 0x61 = 0x05, 0x62 = 0x0b, 0x63 = 0x05, 0x64 = 0x03. That decodes as cell charging and the gauge both enabled, NTC sensing enabled, 125 mA precharge, 500 mA fast charge, 125 mA termination and a 4200 mV full-charge voltage."
  consequence: "THE CHARGER'S OWN EVENT BITS ARE ON BANK 2 AND ARE NOT ON BY DEFAULT. Status/enable registers 0x48..0x4a (bank 0 = 0x48, bank 1 = 0x49, bank 2 = 0x4a, each with an enable partner at 0x40..0x42) carry one bit per source, and bank 2 bit 3 is CHARG while bit 4 is CHARG_DONE. The measured enable mask for bank 2 on this board was 0x5f after this project enabled those two bits, which is what makes them observed rather than documented. They matter because bank 1's VBUS bits say only that a cable is attached, while CHARG says the cell is actually taking current and CHARG_DONE says it has finished -- three different states a single cable event cannot distinguish, and the difference between a working charger and one that is plugged into nothing. Nothing needs to be written, and that is a result rather than an omission. LilyGo documents this board's pack as 1100 mAh at 3.7 V, warns to keep the charging current below 500 mA so the PMU does not overheat, and warns not to exceed 0.5 C, which for 1100 mAh is 550 mA. The measured defaults sit inside all three: 500 mA is 0.45 C, 4200 mV is the standard full-charge voltage for a 3.7 V Li-ion cell rather than the 4350 or 4400 settings that belong to high-voltage cells, and 125 mA termination is about 11 percent of capacity, which is the usual shape. The TinyDesk port's charge.c therefore only READS these registers at boot, logs them, and warns if any value drifts from the expected one, so a future change that breaks charging cannot pass unnoticed. It deliberately does not write a charge voltage: choosing one means reading a cell's spec, and setting 4350 or 4400 on this pack would overcharge it."
  sources:
    - "lewisxhe/SensorLib src/pmic/xpowers/axp2101/AXP2101Charger.cpp: setPreChargeCurrent, setFastChargeCurrent, setTerminationCurrent, setChargeVoltage, and the two decode scales"
    - "lewisxhe/SensorLib src/pmic/xpowers/axp2101/AXP2101Regs.hpp: IPRECHG_SET 0x61, ICC_SETTING 0x62, ITERM_CTRL 0x63, CV_CHG_VOLTAGE 0x64"
    - "lewisxhe/SensorLib src/pmic/xpowers/axp2101/AXP2101Core.cpp: CELL_CHARGE and GAUGE as bits 1 and 3 of MODULE_EN 0x18, TS_MEASURE as bit 1 of 0x30"
    - "Xinyuan-LilyGO/LilyGoLib docs/hardware/lilygo-t-watch-ultra.md: 1100 mAh (4.07 Wh), 3.7 V, 'use a charging current below 500 mA', 'do not charge at a rate greater than 0.5 C'"
    - "tinydesk-twatch ports/twatchultra/main/charge.c: charge_report(), which decodes the same six registers and checks them against these values"
  verification: "Measured on the watch 2026-11-12 by the ESP-IDF port, which has never written a charge register: the boot log reads `180a 30ff 6105`, `620b 6315 6403` and `cv4200 ic500`, and System Monitor simultaneously reports `Battery:100%  4.17 V full`, so the state decode agrees with the configuration decode. The decoder was checked against the encoder before it was trusted: a host harness round-tripped all 40 fast-charge steps from 25 to 1000 mA and all six voltage steps through SensorLib's own encoding, 46 checks with no failures. A vendor defect was found in the process and is recorded because copying it would be silent: LilyGoLib's LilyGoWatchUltra.cpp calls `setChargeVoltage(4288)`, and SensorLib accepts only 5000, 4000, 4100, 4200, 4350 and 4400, so 4288 falls through to `default: return false` and the register keeps its power-on value. A project that copied that call would believe it had set a voltage it had not set. Their `setFastChargeCurrent(512)` is harmless for a different reason: 512 encodes to the same register code as 500 mA Revised 2026-11-12 (core-log entry 26): the charger's own start and done events are bank 2 bits 3 and 4, off by default; enabling them produced `chg done`, then `vbus out`, then `vbus in` and `chg go` 15 ms later, then `chg done` again ten seconds after, against a cell already near full."


- record_id: PMU-006
  kind: PMU
  topic_id: PMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "A battery thermistor IS fitted on the TS pin, and it reads the cell rather than the SoC"
  statement: "The AXP2101's TS pin is the battery's NTC input, not an ambient sensor: it exists so the charger can refuse to charge a cell that is too hot or too cold. It is read as a voltage at 14 bits and 0.5 mV per step across registers 0x36/0x37, with the pin's mode set by register 0x50, which reads 0x07 here -- the value the vendor driver writes when external NTC measurement is enabled. The board's schematic marks that position `NC NTC`, which reads as not-populated. THAT MARKING IS WRONG FOR THIS UNIT: the pin tracks temperature. Four states of one watch, measured in order: TS 475 mV with the die at 37.8 C; 527 mV after the watch had cooled to a die temperature of 32.8 C; then a monotonic fall from 526 to 515 mV across 260 seconds of charging while the die sensor held at exactly 35.8 C in every one of those samples; and 511 mV after a reboot. Cooling raises the reading, warming lowers it, and both directions were observed."
  consequence: "Two things follow, and the second is the one worth keeping. The pad is populated, so the charger's temperature protection has something real to act on, and a schematic marking of NC must not be read as evidence that a part is absent -- the same lesson the README's `/fs` and SD rows taught on this same day. And THE TS PIN DOES NOT MEASURE THE SoC: during the charging run the die sat at exactly 35.8 C for 260 seconds while TS fell 11 mV, because the cell warms under charge current and is a separate thermal body with far more mass than the SoC. Read TS for the battery, read the SoC's own sensor for the chip, and never use one as a proxy for the other -- they diverge by several degrees whenever the load or the charger changes. What is NOT established BY THIS RECORD is an absolute calibration, and it is now supplied elsewhere: PMU-007 gives the datasheet's NTC specification and its full temperature table, and explains why the excitation current must be read rather than assumed. This record's own earlier arithmetic was wrong on that point -- it guessed 80 uA from a plausible-looking fit, whereas the part reports 60 uA in reg50H[1:0], so the 2 to 10 mV per degree sensitivity quoted here was comparing two bodies that were never at the same temperature. The vendored driver does not decode it either: it returns TS volts and names the field `BAT_TEMPERATURE`. So this project reports raw millivolts and does not invent a temperature."
  sources:
    - "on the watch: `ts ctl07` with `ts 475mV`, `527mV`, `515mV` and `511mV` across four states, logged by ports/twatchultra/main/charge.c charge_ts_report()"
    - "lewisxhe/SensorLib src/pmic/xpowers/axp2101/AXP2101Adc.cpp: BAT_TEMPERATURE reads 0x36/0x37 and returns raw volts via TS_STEP 0.5; enableChannels() writes 0x50 with 0x07 for external NTC"
    - "lewisxhe/SensorLib src/pmic/xpowers/axp2101/AXP2101Regs.hpp: TS_VOLT_H 0x36, TS_VOLT_L 0x37, TS_PIN_CTRL 0x50"
    - "Xinyuan-LilyGO/LilyGoLib schematic T-Watch Ultra V1.0 SCH 25-07-24, AXP2101 symbol: the TS pin against a `NC NTC` marking"
    - "PMU-005 for the charge registers this sits alongside"
  verification: "Measured on hardware 2026-11-12. Cooling: the die went 37.8 to 32.8 C and TS moved 475 to 527 mV. Warming: over 260 s of charging TS read 526,526,526,525,525,524,524,523,523,523,523,522,522,522,522,521,520,520,520,519,519,518,518,518,517,517,516,515 while the die held a constant 35.8 in every sample -- which rules out a fixed divider, an empty pad and PMU reference drift at once, because the part's own reference sits at the die temperature and that did not move. A fifth reading after a reboot gave 511 mV. The decode is deliberately left in millivolts: no beta was assumed, because a plausible number derived from a guessed part number is the exact failure this file records in LCD-008 and PMU-001. Revised 2026-11-12 (core-log entry 30): the part data was found, so the decode no longer has to stay in millivolts indefinitely -- see PMU-007. The guess of an 80 uA excitation current in this record was wrong: reg50H reads 0x07, which is 60 uA, and the same millivolts therefore decode to 28.6-30.3 C rather than the 23.7-25.4 C a 50 uA assumption would give. The thermistor conclusion itself is unaffected and was reached before any of this."

- record_id: PMU-007
  kind: PMU
  topic_id: PMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "The TS thermistor is a 10 kohm NTC on a current source, and the datasheet gives the full temperature table"
  statement: "The AXP2101 datasheet (X-Powers, V1.4, section 6.7.4.4 and Table 6-3) specifies the battery-temperature input exactly. The NTC sits BETWEEN TS AND GND, nominally 10 kohms at 25 C; the part sources a constant current through it and converts the resulting voltage with its ADC. The current is selected by reg50H[1:0]: 00 = 20 uA, 01 = 40 uA, 10 = 50 uA, 11 = 60 uA, with a power-on default of 10b (50 uA); reg50H[3:2] enables the source (00 off, 01 on when the TS ADC channel is enabled, 10 on only while the TS channel converts, 11 always on) and reg50H[4] selects whether TS feeds the charger (0) or is a fixed external input (1). The datasheet's worked example is the Mitsubishi TH11-3H103F at 50 uA, tabulated from -20 C to 60 C as resistance, detected voltage and ADC code: -20 C 63.00k/3.150V/0x189, -10 C 40.26k/2.013V/0xFBA, 0 C 26.49k/1.325V/0xA5A, 10 C 17.78k/0.889V/0x6F2, 20 C 12.07k/0.604V/0x4B8, 25 C 10.00k/0.500V/0x3E8, 30 C 8.320k/0.416V/0x340, 40 C 5.839k/0.292V/0x248, 50 C 4.171k/0.209V/0x1A2, 60 C 3.032k/0.152V/0x130. The ADC is 0.5 mV per count, so 0x3E8 = 1000 counts = 500 mV at 25 C, which is what this project's decoder already computes. THE SAME CURRENT SOURCE THAT PRODUCES A MEASUREMENT ALSO GATES THE CHARGER: with reg50H[4] = 0 the part pauses charging when TS leaves the window set by reg54H/reg55H (defaults 0x29 and 0x58, about 0 C and 55 C) and pauses the boost outside reg56H/reg57H (defaults 0x3E and 0x4C, about -10 C and 60 C), resuming automatically on return."
  consequence: "This is the part data the port was missing, and it makes TS millivolts convertible to degrees without guessing a beta. Two things must be read rather than assumed, though. The current source is NOT necessarily at its power-on default: this watch reads reg50H = 0x07, meaning 60 uA rather than the documented 50 uA default, with reg50H[4] = 0 so TS genuinely gates the charger. Whether that 60 uA came from an eFuse, which the datasheet lists as this register's reset source, or from firmware that ran before this one, the PMU stays powered from the cell and keeps the value -- so the conversion must scale by the current it actually finds, not by the table's 50 uA. At 60 uA the port's readings of 494 to 527 mV decode to 30.3 to 28.6 C, against die temperatures of 33 to 38 C in the same sessions, which is the expected ordering: the SoC runs warmer than the cell. A conversion that assumed 50 uA would report those same readings as 25.4 to 23.7 C, four to five degrees cooler. The second consequence is a safety one that must not be broken by accident: because reg50H[4] = 0, a wrong TS reading does not merely mis-report, it moves the charger's temperature window. Writing reg50H to change the current or to declare TS unused would change when the part refuses to charge, which is why this project reads the register and does not write it."
  sources:
    - "XPowersLib datasheet/AXP2101_Datasheet_V1.4_en.pdf: section 6.7.4.4 'Battery temperature protection', Table 6-3, and register 0x50 in 6.13.2.46; registers 0x52-0x57 for the JEITA thresholds"
    - "on the watch: reg50H reads 0x07 (`ts ctl07` at every boot) and reg36H/reg37H readings of 494-527 mV"
    - "lewisxhe/XPowersLib src/XPowersAXP2101.hpp getTsTemperature(): the same 0.5 mV scaling, a 50 uA assumption, and the TH11-3H103F Steinhart-Hart coefficients"
    - "PMU-006 for the measurement that established a thermistor is fitted"
  verification: "Read from the vendor datasheet bundled in XPowersLib, then checked against this board. The decoder was validated against the datasheet's own table rather than against itself: interpolating Table 6-3 returns exactly 25.0 C for 500 mV and 30.0 C for 416 mV, the two tabulated points it is asked about, so the table is internally consistent at its stated 50 uA. The current-source setting was read from the part (0x07) rather than assumed, which is what turns the same millivolts into 28.6-30.3 C at 60 uA against 23.7-25.4 C at 50 uA. The Steinhart-Hart coefficients published in XPowersLib were checked and NOT used: they put 10.00k at 22.4 C where the datasheet's own table puts it at 25 C, so they disagree with the primary source by about 2.6 C and the table wins."

- record_id: KRN-001
  kind: KERNEL
  topic_id: KRN
  title: "The kernel is NOMMU: every load segment and every large allocation must be contiguous"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "This port has no MMU and no overcommit. An ELF's load segments must each be satisfied by one contiguous run of free memory, and there is no page table to scatter them; a program whose static data is large simply fails to exec. The kernel reports the failure as 'nommu: Allocation of length N from process PID (name) failed' followed by a Mem-Info dump, where N is the segment or request size."
  consequence: "A large static array is part of the load segment, so it blocks exec rather than failing at run time — which is why a 512 KB ROM compiled into .bss produced a binary that could not start once memory had fragmented, with no message the program itself could print. Moving the payload to a runtime malloc turns an uninstrumentable exec failure into a reportable one. Plentiful free memory does not help: what matters is the largest contiguous block, which after a few minutes of uptime settles well below the peak."
  sources:
    - "kernel log on hardware: 'nommu: Allocation of length 1052672 from process 222 (corebench) failed' and 'Allocation of length 569344 ... (tamafb) failed'"
    - "TamaHarness/for-escapement/RESULTS-3.md: the workarounds and the fragmentation cliff"
  verification: "Reproduced repeatedly on hardware: a statically embedded 512 KB ROM failed to exec after a few minutes of uptime and succeeded immediately after a reboot; the same payload read into a runtime allocation started, and then reported its own failure when the allocation could not be met. Revised 2026-11-09 (core-log entry 16): the cliff is not limited to large payloads. A 222 KB static binary started reliably when the largest free block was 1 MB, and once it fell to 256 KB the same binary started in the foreground every time but died with SIGSEGV roughly one launch in three when backgrounded with `&` -- so exec itself becomes marginal near the block size, and foreground and background are not equivalent."

- record_id: KRN-002
  kind: KERNEL
  topic_id: KRN
  title: "The linear flash window at 0x42000000 covers the whole 16 MiB only immediately before the jump to Linux"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "init_linux_mmap() in this port's low-level code extends the SoC's MMU table so the entire flash contents are addressable at 0x42000000. It is called from map_flash_and_go(), which runs inside the task that jumps to Linux at the end of app_main. Firmware code that executes earlier and dereferences 0x42xx0000 is reading addresses that are not mapped."
  consequence: "Reading unmapped memory there does not produce a clean fault: the board entered a core-1 watchdog reset loop with the saved PC in panic_handler and no console output at all, indistinguishable from a board that will not boot. Startup-time firmware that needs to read a partition must use esp_partition_read(), which works at any point after partition initialisation, rather than the linear window."
  sources:
    - "linux_ll_esp32s3.c: init_linux_mmap() and its MMU-table walk"
    - "linux_boot.c: map_flash_and_go() as the only caller"
    - "boot log on hardware: 'rst:0x8 (TG1WDT_SYS_RST) Saved PC:0x42002b3b' resolving to panic_handler"
  verification: "Caused and then avoided on hardware: an early read through the linear window produced the reset loop; the same read via esp_partition_read() worked and the board booted normally."

- record_id: KRN-003
  kind: KERNEL
  topic_id: KRN
  title: "The kernel is linked against space_for_vectors in the firmware's RAM, so changing firmware size moves the address it expects"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The firmware reserves a 4 KiB buffer for the exception vectors Linux will use, and the linker places that symbol, space_for_vectors, in the firmware's internal RAM. The kernel is configured with CONFIG_VECTORS_ADDR set to that address, currently 0x40380000. Anything that changes how much internal RAM the firmware uses — a new component, a bigger static buffer, a different cache configuration — can move the symbol."
  consequence: "If the two disagree, Linux boots perfectly and then goes silent at 'Run /sbin/init', because it writes its vectors into the wrong memory. There is no panic and no message. Build-time checks compare the firmware ELF's symbol against the kernel config and refuse a mismatch: scripts/build.sh for the firmware, and a grep in scripts/build-kernel.sh for the kernel. CONFIG_KERNEL_LOAD_ADDRESS is coupled the same way to the linux partition offset, being 0x42140000 for a partition at 0x140000."
  sources:
    - "linux_boot.c: space_for_vectors and its alignment"
    - "patches/kernel/config/devkit_c1_16m_linux.config: CONFIG_VECTORS_ADDR=0x40380000, CONFIG_KERNEL_LOAD_ADDRESS=0x42140000"
    - "scripts/build.sh: the vectors-address check before flashing"
  verification: "The two agree in the current build: the firmware ELF reports space_for_vectors at 0x40380000 and the kernel config expects 0x40380000. Re-checked on every flash this session; it never moved."

- record_id: KRN-004
  kind: KERNEL
  topic_id: KRN
  title: "A 512 KB contiguous block exists only briefly after boot, and one allocation consumes it for the rest of the session"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "On a freshly booted board /proc/buddyinfo reports a free order-8 block, 1 MB contiguous. Within about a minute of boot it may already be gone: at nine minutes of uptime the largest free block was 256 KB, and at thirty-two minutes it was 128 KB. A single 512 KB allocation consumes the large block permanently, so the same program that succeeds once fails immediately afterwards with 'cannot allocate 524288 bytes for the ROM', even though `free` still reports over 2 MB available."
  consequence: "Anything needing 512 KB contiguous gets exactly one attempt per boot, and only if it runs early. This is what makes the emulator's ROM a boot-time problem rather than a runtime one: putting the ROM in PSRAM is worth 33.8 percent (EMU-009) but only if the allocation happens before the block fragments, which is the concrete reason a S51tamafb must start from rcS and not on demand. Read /proc/buddyinfo before assuming a large allocation can be met -- `free` answers a different question and will say there is plenty."
  sources:
    - "on the watch: cat /proc/buddyinfo at 0, 1, 9 and 32 minutes of uptime"
    - "kernel log: 'nommu: Allocation of length 1052672 from process 234 (corebench-rom) failed' and the later 'cannot allocate 524288 bytes for the ROM' from the same binary"
    - "KRN-001 for the contiguity rule itself"
  verification: "Measured on hardware 2026-11-10 across four boots, reading the buddy list directly rather than inferring from free memory. The one-shot behaviour was observed three times: an allocation that succeeded on a fresh boot failed on every subsequent attempt in the same session. Revised 2026-11-10 (core-log entry 25): exactly 524288 bytes is the worst size to request on this board. Probed directly with a malloc ladder on a board whose buddy list showed a free order-7 block: malloc(524288) fails and malloc(524000) succeeds, so the boundary lies in that 288-byte window and it is the allocator's own header -- a request equal to the largest block leaves nothing over for it. Splitting the ROM does not help: malloc(262144) twice yields one success and one failure, because each block pays its own header out of the same budget. The earlier order-8 claim in this record was also wrong: the buddyinfo line has eleven fields, orders 0 to 10, and the readings that were called order 8 were order 7, 512 KB. Nothing larger than 512 KB was ever free."


- record_id: KRN-005
  kind: KERNEL
  topic_id: KRN
  title: "The core split is forced by the closed WiFi blob, not a design choice, and esp-hosted has no peripheral channel"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "The upstream port documents its own architecture in `build/Linux-on-esp32-S3/ARCHITECTURE.md`: core 0 runs ESP-IDF/FreeRTOS and owns the WiFi radio, whose driver is a closed binary that only runs under ESP-IDF; core 1 runs Linux natively, not emulated. The firmware source makes the boundary explicit -- `linux_boot.c` calls `xTaskCreatePinnedToCore(linux_task, \"linux_task\", 4096, NULL, 5, NULL, 1)`, and `linux_task()` is `map_flash_and_go(); esp_restart();`, so Linux is a FreeRTOS task pinned to core 1 that never returns. Everything other than the radio -- panel, PMU, RTC, touch, haptics, filesystem, USB -- is owned directly by Linux through its own drivers and MMIO. The esp-hosted link between the cores carries a network interface, Bluetooth, and typed custom interfaces (this project added ESP_LOG_IF for firmware logs); it has no I2C, SPI or GPIO RPC, and no generic peripheral channel."
  consequence: "This settles a recurring proposal to put hardware behind the RTOS and let Linux do software. It fails on five grounds, each already recorded here: there is no channel to build it on (NET-003, BLE-005 -- the link is contended enough that one extra log line stopped WiFi bring-up); every I2C transaction would become a round trip that the 15 ms touch poll cannot absorb; core 0's flash path has to skip the inter-core park and must not touch PSRAM while the shared cache is off (JFS-001); core 0 is the radio core and a polling server would contend with WiFi and BLE under a watchdog (BLE-004); and every device driver already exists and works on Linux, so moving the bus would mean rewriting them as firmware in a tree that must never be pushed. The bit-banged I2C is a missing hardware driver, not a placement mistake, and the fix is to write one. The distinction to keep is that computation may cross the boundary -- EMU-004's emulator-to-core-0 design batches work into a shared buffer -- while I/O should not, because I/O needs a round trip per transaction. See docs/cores.md."
  sources:
    - "build/Linux-on-esp32-S3/ARCHITECTURE.md: 'The big picture: two cores, two operating systems, one chip' and the boot flow"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/linux_boot.c: the xTaskCreatePinnedToCore(..., 1) call and linux_task()"
    - "Xinyuan-LilyGO/LilyGoLib examples/factory/ui_nes.cpp, screen_web_server.cpp, LilyGoWatchUltra.cpp: how the official firmware uses both cores"
  verification: "Read from the firmware and port sources in this workspace, not inferred: ARCHITECTURE.md states the split, and linux_boot.c shows the pinning call and the never-returning task body. The absence of a peripheral RPC was established by reading the esp-hosted interface set, which carries network, Bluetooth and the project's own ESP_LOG_IF and nothing else. LilyGo's usage was read from its published examples: it runs FreeRTOS on both cores with no OS boundary, pins the NES emulator and its audio task to core 0 alongside the radios, and pins the web server to the core that ARDUINO_RUNNING_CORE is not; its board file calls Wire.begin(SDA, SCL) with setClock(1000000) for touch, which confirms the hardware I2C peripheral works on this board's pins. Revised 2026-11-11 (core-log entry 54): the fourth of the five grounds said core 0 cannot write flash at all, citing JFS-001. It can -- linux_flash.c performs Linux's flash writes by skipping the inter-core park -- so the ground is restated as the narrower constraint JFS-001 now records. The other four grounds are unchanged and the argument still stands."

- record_id: KRN-006
  kind: KERNEL
  topic_id: KRN
  title: "The hardware I2C block boots gated and held in reset, and ignores writes while answering reads"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "The ESP32-S3's I2C_EXT0 peripheral is not usable from a cold start without two writes to the SYSTEM block at 0x600c0000. SYSTEM_PERIP_CLK_EN0 (offset 0x18) bit 7 gates its clock and reads 0 at boot; SYSTEM_PERIP_RST_EN0 (offset 0x20) bit 7 holds it in reset and reads 1, measured on this board as 0x9eff1fb8. While the reset bit is set the peripheral's register file stays at its power-on defaults and writes to it are discarded, but reads still return data and the TX FIFO still accepts bytes. The observable signature is CTR reading 0x20b (the reset default: master mode and clock enable both clear) immediately after a successful write, with sr showing txfifo_cnt holding the queued bytes and int_status reading 0."
  consequence: "This cost eight build-and-flash cycles to find because the failure imitates three other faults in sequence: a bad command encoding, a wrong completion signal, and a dead bus. Anyone porting a peripheral here should check the SYSTEM block first, not last: gate and reset are per-peripheral bits in two registers at a fixed address, they are cheap to read, and a held reset makes every driver-side hypothesis unfalsifiable. The existing i2c-gpio bus needed neither because it drives GPIOs directly, and the firmware does not use I2C at all, so nothing on this board had ever opened the gate. The same two registers govern every other peripheral, and the port already relied on this without saying so: spi2_clk in esp32s3.dtsi is a gpio-gate-clock over clk_gpio0 bit 6, which is exactly SYSTEM_SPI2_CLK_EN."
  sources:
    - "esp-idf soc/esp32s3/include/soc/system_reg.h: SYSTEM_PERIP_CLK_EN0_REG 0x18, SYSTEM_I2C_EXT0_CLK_EN BIT(7), SYSTEM_PERIP_RST_EN0_REG 0x20, SYSTEM_I2C_EXT0_RST BIT(7)"
    - "esp-idf hal/esp32s3/include/hal/clk_gate_ll.h: periph_ll_get_clk_en_mask and periph_ll_get_rst_en_mask map PERIPH_I2C0_MODULE to those bits"
    - "arch/xtensa/boot/dts/esp32s3.dtsi: spi2_clk as gpio-gate-clock over clk_gpio0 bit 6, matching SYSTEM_SPI2_CLK_EN"
  verification: "RESOLVED 2026-11-10 (core-log entry 31): the two blockers were a wrong status bit and a missing timing restore, and with both fixed the bus carries sustained traffic -- capacity 100, voltage 4178000 uV, status Full, usb online 1, rtc0 readable and ticking, ten consecutive reads with no failure, and a credible i2cdetect. Written up in docs/cores.md. Measured on the watch 2026-11-10 through the driver's own probe logging. Before the fix: perip_clk_en0=0x6100e0c7 (bit 7 clear) and perip_rst_en0=0x9eff1fb8 (bit 7 set), and a direct write of 0x31b to CTR read back 0x20b. After gating, releasing and pulsing the reset: perip_rst_en0=0x9eff1f38 with bit 7 clear, and the same CTR write read back correctly, so master mode and clock enable genuinely took effect. Revised 2026-11-10 (core-log entry 29): with the gate open, the reset released, the GPIO input route added and the ACK flags corrected, the bus carries real traffic -- i2cdetect returns a credible scan with only 1a, 20, 34 and 51 answering, single-byte reads return correct data, rtc-pcf85063 registers as rtc0 and the AXP2101 probes and publishes both supplies. Multi-byte reads still return only their first byte, so the transfer path is proven for one byte and not for N."

- record_id: FLS-001
  kind: FLASH
  topic_id: FLS
  title: "16 MiB partition layout, and the kernel image is stored raw at 0x140000"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The custom partition table is: nvs at 0xa000 length 0x5000, phy_init at 0xf000 length 0x1000, factory at 0x10000 length 0xc0000, etc at 0xd0000 length 0x70000, linux at 0x140000 length 0x400000, rootfs at 0x540000 length 0x780000, home at 0xcc0000 length 0x340000. The kernel image, arch/xtensa/boot/xipImage, is stored raw at 0x140000 with no container or header: the first 4096 bytes of flash at that offset are byte-identical to the file."
  consequence: "A kernel change is a single write of about 2.8 MiB to 0x140000 and does not disturb the firmware, rootfs or the writable partitions. A full-image write of 16 MiB is only needed to change the partition table or restore the release state — and it rewrites /etc and /home, so it costs the WiFi credentials, the root password and anything deployed there."
  sources:
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/partition_table.esp32s3.16m8r"
    - "esptool read of 0x140000 compared byte for byte against arch/xtensa/boot/xipImage"
  verification: "Confirmed on hardware: a 4 KiB read from 0x140000 matched the first 4 KiB of xipImage exactly, and writing xipImage alone to 0x140000 repeatedly produced a booting kernel with new driver changes."

- record_id: KRN-007
  kind: KERNEL
  topic_id: KRN
  status: VERIFIED
  verified_date: 2026-11-10
  title: "An I2C NACK or arbitration loss must reset the ESP32-S3 controller, or one bad transfer wedges the whole bus until reboot"
  statement: "The driver's polling loop returned -ENXIO on INT_NACK and -EAGAIN on INT_ARB_LOST after clearing the interrupt, without calling the recovery path. Recovery was wired only into the timeout branches. On this board those two outcomes are exactly what a capacitive controller produces while it is being polled, and either one can leave SR.bus_busy set. Once bus_busy is set with the FSM idle, the pre-transfer check never fires recovery either, so every later transfer to every device fails identically and the bus stays unusable until a reboot or power cycle."
  consequence: "This is the fault that wedged the bus whenever the touch controller was read, and it is the same fault the earlier userspace touch daemon hit: repeated reads produced a storm of failures against all four devices with no recovery attempted. Recovery belongs on every failure path, not just the timeout ones, and the safe general rule is ESP-IDF's: after any failure, if the controller still reports the bus busy, reset it. A NACK from a genuinely absent device does not trigger a reset because the bus is not busy in that case, which is why an i2cdetect scan stays cheap. Related: esp32_i2c_transfer() also collapsed every failure to -ETIMEDOUT, so a NACK and a timeout were indistinguishable from userspace and the log said nothing useful; it now propagates the real error."
  sources:
    - "patches/kernel/i2c/final/i2c-esp32.c esp32_i2c_run(), esp32_i2c_recover(), and the ERRNO_NOTE"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/esp-idf/components/driver/i2c/i2c.c i2c_master_cmd_begin(): resets the FSM when the status is TIMEOUT *or the bus is still busy*"
    - "KRN-006 for the clock gate and reset state this depends on"
  verification: "Verified on hardware 2026-11-10. Before the fix, a handful of reads from the touch controller left the bus dead for every device. After it, a 10 s run of 1687 report reads produced 188 errors and the bus was still healthy afterwards, with the log showing `NACK: int_raw=00008610 sr=5600c010 busy=1` followed by `bus recovered (int_raw=00000000 sr=0000c000)` for each of 11 genuine NACKs, and the RTC, PMU and expander all still answering."

- record_id: FLS-002
  kind: FLASH
  topic_id: FLS
  title: "The upstream release image is not this project's state: no display driver, octal PSRAM, and no credentials"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "images/linux-esp32s3-native-full.bin is the upstream release. Its kernel region contains no CO5300 strings at all, so the panel has no driver and the screen stays dark. Its firmware requires octal PSRAM and aborts on this quad-PSRAM board. Its /etc carries the factory root password and no WiFi credentials."
  consequence: "Flashing it is a recovery step with a cost, not a restoration: it loses the display, the Bluetooth keyboard daemon, the login on the AMOLED, the WiFi configuration and the root password. It must be followed by this project's firmware at 0x10000. A full flash backup taken beforehand is the only way to get those back exactly, and a backup of the kernel alone is not sufficient — /etc damage is what makes a saved kernel appear broken."
  sources:
    - "images/linux-esp32s3-native-full.bin: kernel region probed for 'co5300' returning 0"
    - "boot log after flashing it: 'octal_psram: PSRAM ID read error'"
    - "docs/watch-recovery.md"
  verification: "Established by doing it on hardware and paying the full cost, then recovering by writing this project's kernel to 0x140000."

- record_id: FLS-003
  kind: FLASH
  topic_id: FLS
  title: "A full-image flash rewrites /etc, and S03keepconfig carries back only seven named files plus the dropbear keys"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A full-image flash rewrites the `/etc` jffs2 partition from the image. `/etc/init.d/S03keepconfig` restores a fixed list afterwards -- `wpa_supplicant.conf`, `shadow`, `passwd`, `group`, `inetd.conf`, `remote-login`, `remote-login.conf`, and the four `dropbear/dropbear_*_host_key` files -- from a copy it keeps in `/home/.etc-backup`, and only when `/etc` carries no generation stamp. Nothing outside that list survives; every other file added to `/etc` is lost. A kernel-only write to 0x140000 does not touch `/etc` at all."
  consequence: "Any project-added `/etc` file -- `/etc/TZ`, `/etc/init.d/S01hwclock`, a deployed init script -- must be re-installed after a full-image flash, but not after a kernel flash. This is how a shutdown write-back hook that had been verified on hardware came to be missing from the board: the script's existence was documented while the file itself had been removed by a reflash. `scripts/watch-time.sh` is idempotent and re-installs both files, so the cure is one command rather than remembering."
  sources:
    - "on the watch: `cat /etc/init.d/S03keepconfig` -- the FILES and KEYS lists, and the stamp gate"
    - "FLS-001 for the partition layout and the kernel-only write path"
  verification: "Verified on hardware: `/etc/TZ` written by hand and `/etc/init.d/S01hwclock` installed both survived a plain reboot, while `/etc/init.d/S01hwclock` was absent from the board at the start of this cycle despite being documented as running at shutdown. The surviving-file list is read directly from the script's own `FILES` and `KEYS` variables, not inferred."

- record_id: JFS-001
  kind: JFFS2
  topic_id: JFS
  title: "A firmware flash write through the default path aborts because it tries to park core 1; the project bypasses that, and the real constraint is the shared cache"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "spi_flash_disable_interrupts_caches_and_other_cpu() parks the other core before disabling caches, using esp_ipc_call() and then waiting for the other side to acknowledge. Core 1 runs Linux, not a FreeRTOS task loop, so that acknowledgement never arrives and the call aborts with 'abort() was called at PC 0x42023b2e on core 0'. This happens only when the inter-core call is actually issued. The firmware can set g_spi_flash_skip_ipc, which skips the call entirely, and linux_flash.c does exactly that around every esp_flash_* call for precisely this reason. With the flag set the abort does not occur and the flash operation proceeds."
  consequence: "Two separate facts had been collapsed into one, and the collapse is what this record now undoes. The abort is specific to the default path that parks core 1, and g_spi_flash_skip_ipc avoids it, so firmware flash writes are not forbidden -- they are what the IPC flash service in linux_flash.c exists to perform. What is genuinely constrained is what the write path may TOUCH while the caches are off: the cache is shared on this chip (SOC_IDCACHE_PER_CORE is undefined for esp32s3), so disabling it on core 0 disables it for core 1, whose kernel is XIP and fetches instructions through it. Core 0 must therefore not read PSRAM or flash-resident code inside that window either. linux_flash.c now copies each chunk into a static internal-DRAM buffer before calling esp_flash_write(), which satisfies that, and a 1 KiB write during emulator play went from a reliable reboot to four consecutive survivals (core-log entry 54). NVS through the default path is still unusable, because nvs_set_blob does not set the flag, so any firmware persistence that must survive a reboot still belongs to Linux on jffs2."
  sources:
    - "backtrace on hardware: abort <- esp_ipc_call_and_wait <- spi_flash_disable_interrupts_caches_and_other_cpu <- esp_partition_write <- nvs::Page::writeItem <- nvs_set_blob"
    - "docs/bluetooth-bond.md: 'Bonds cannot be persisted here, and that is a hardware limit'"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/linux_flash.c: g_spi_flash_skip_ipc set around every esp_flash_* call, and the internal-DRAM staging buffer"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/esp-idf/components/spi_flash/cache_utils.c: the g_spi_flash_skip_ipc test around esp_ipc_call()"
    - "SOC-004 for the shared cache and why core 1 cannot execute during the window"
  verification: "Reproduced on hardware by enabling CONFIG_BT_NIMBLE_NVS_PERSIST: the board reset-looped at about 3.3 s with the backtrace above. Disabling it restored normal boot. Revised 2026-11-11 (core-log entry 54): the title and statement said firmware must never write flash after Linux starts, and that is contradicted by this project's own working code -- linux_flash.c performs exactly those writes by setting g_spi_flash_skip_ipc to skip the inter-core call that aborts. The constraint is on what the write path touches while the caches are off, not on writing at all."

- record_id: JFS-002
  kind: JFFS2
  topic_id: JFS
  title: "The firmware's jffs2 reader can read files Linux wrote, but refuses compressed nodes"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "tiny_jffs2_reader.c walks jffs2 nodes from the start of a partition and can resolve a file written by Linux on the mounted filesystem, not only one present in the flashed image. A 256-byte blob written on the watch — mostly zeroes, content jffs2 does compress — read back byte-exact through this reader, both head and tail. The reader nonetheless returns -1 for any node whose compression flag is set: 'compression is not supported ATM'."
  consequence: "Small files survive because jffs2 stores them uncompressed, so the refusal does not bite in practice, but mounting with compr=none makes it impossible for Linux to write something the firmware cannot read. This is the mechanism that would let Linux persist a BLE bond for the firmware to load at boot."
  sources:
    - "tiny_jffs2_reader.c: the version_compr check and its comment"
    - "/etc/fstab on the watch: compr=none on the etc and home mounts"
  verification: "Verified off-hardware against a real 448 KiB partition image dumped from the watch, with the reader compiled for the host: a 256-byte blob of BONDMAGIC + zeroes + ENDMARK read back with the expected 8 non-zero bytes at the expected offsets."

- record_id: BLE-001
  kind: BLE
  topic_id: BLE
  title: "NimBLE's bond store is the application's job; without it pairing is refused with ENOTSUP"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "In ESP-IDF, ble_store_config_init() installs the security manager's store callbacks and is called explicitly by the application before the host starts. Without it the callbacks remain NULL, ble_gap_security_initiate() returns BLE_HS_ENOTSUP (8), and no pairing procedure is begun. The link therefore never encrypts, and a HID keyboard — which requires an encrypted link before it will deliver reports — refuses every ATT operation with 0x05, insufficient authentication."
  consequence: "The symptom appears two layers from the cause: the keyboard connects and is discovered, then every characteristic write fails with an authentication error, which reads as a keyboard quirk. ble_store_config_init() must be called in the same place as the ble_hs_cfg.sm_* configuration, before nimble_port_init()."
  sources:
    - "ESP-IDF examples, e.g. examples/bluetooth/nimble/bleprph/main/main.c: the explicit ble_store_config_init() call and its 'XXX Need to have template for store' comment"
    - "host/src/ble_store.c: ble_store_read/write return BLE_HS_ENOTSUP when the callback is NULL"
  verification: "Diagnosed on hardware from the sequence 'security initiate (8)' followed by 'protocol mode write status 261', where 261 is BLE_HS_ERR_ATT_BASE + 0x05. Adding the call produced 'encryption on (0)' and working key reports."

- record_id: BLE-002
  kind: BLE
  topic_id: BLE
  title: "At MTU 23 an ATT read returns 22 bytes, so a large report map exhausts the mbuf pool and drops the link"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The default ATT MTU is 23, so a single read returns at most 22 bytes. A Logitech keyboard's HID report map is 934 bytes, which needs 43 round trips, and each outstanding GATT client request holds one mbuf from a pool that defaults to 12 blocks. The pool empties, subsequent requests fail with BLE_HS_ENOMEM (6), and the keyboard eventually drops the link with HCI status 0x16."
  consequence: "Reading a report map is not a small operation on this stack. Either raise CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT, or avoid needing the map at all — which is what boot protocol does (BLE-003). Raising the pool to 24 blocks was not sufficient on its own because the requests were also issued concurrently; serialising them one completion at a time is equally necessary."
  sources:
    - "sdkconfig.defaults.esp32s3.16m8r: CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT raised 12 to 24, with the reasoning recorded"
    - "host/src/ble_att.c and ble_gattc.c for the MTU-1 payload and per-request mbuf behaviour"
  verification: "Observed on hardware: 'report map read at N could not start (6)' repeated, then 'disconnected (reason 534)'. After switching to boot protocol the map is never read and the link stays up."

- record_id: BLE-003
  kind: BLE
  topic_id: BLE
  title: "Boot protocol gives a fixed 8-byte keyboard report; report protocol needs the map and cost the link"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Writing 0 to HID Protocol Mode (0x2A4E) selects boot protocol, which defines exactly one keyboard report shape: byte 0 modifiers, byte 1 reserved, bytes 2-7 six HID usage slots with 0 meaning empty. No report ID is present and no report map is needed. Writing 1 selects report protocol, in which the device sends every report it has behind a report ID and the host must read the Report Map (0x2A4B) to interpret any of it."
  consequence: "Boot protocol is the correct choice for typing on this board, and the report map is never read, which is what keeps BLE-002 from recurring. The cost is that consumer-page keys — volume, media, browser — never arrive, because they exist only on the report-protocol consumer collection. Two things about subscriptions follow, and the second cost a cycle. A keyboard that accepts boot protocol may still send its 8-byte report on a report characteristic rather than the boot input, so reports must be routed by length, not by handle. And subscribing ONLY to the boot input because it exists is wrong: a device can expose Boot Keyboard Input 0x2A22 and never notify on it, sending on a report characteristic instead. The correct rule is to subscribe to the boot input and to every notifying input report, which is what TinyTang does and what this project now does. The old rule looked cheaper and was simply silent for such a device."
  sources:
    - "HID over GATT Profile, HID Protocol Mode 0x2A4E and Boot Keyboard Input Report 0x2A22"
    - "TinyTang ports/bl616/tang_ble.c hid_setup_reports(): writes boot_protocol = 0, subscribes to the boot input and every 0x2A4D report, and routes by length"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/ble_hid.c: subscribe_next(), after the fix"
  verification: "Verified on hardware by both routes: report protocol produced the mbuf drain and dropped link of BLE-002; boot protocol produced 'protocol mode -> boot (0)' and working key reports on the AMOLED console. Revised 2026-11-09 (core-log entry 11): the 'one subscription instead of six' claim was wrong and is what made the Rii K06 type nothing. Measured with the K06 in LE mode: the old rule subscribed handle 0x0015 only (CCC 0x0016) and no report ever arrived there; subscribing to 0x0015, 0x001a, 0x001e, 0x0025 and 0x002c produced live 8-byte reports on 0x001E and decoded keystrokes. Six serialised subscriptions do not exhaust the mbuf pool -- that was six concurrent ones."

- record_id: BLE-004
  kind: BLE
  topic_id: BLE
  title: "Continuous scanning starves core 0's idle task, and a long scan gap can miss a sleeping keyboard entirely"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "ble_gap_disc() with a duration of BLE_HS_FOREVER delivers every advertisement in range to the NimBLE host task, which ESP-IDF pins to core 0 at configMAX_PRIORITIES-4 — the same core as the FreeRTOS idle task and the WiFi task. Idle then never runs and the task watchdog resets the chip about ten seconds later with 'rst:0x7 (TG0WDT_SYS_RST)' and esp_restart_noos in the backtrace. Separately, an idle BLE keyboard advertises for only a few seconds after a key is pressed, so a scan that pauses for eight seconds between fifteen-second windows is deaf 35 percent of the time and can miss a wake altogether with no log trace."
  consequence: "Scanning must be bounded in duration, low in duty cycle and with only a short gap. 30 ms of window in a 120 ms interval is 25 percent duty, leaving the idle task three quarters of the wall clock; a 500 ms gap after each 15 s window keeps coverage near 97 percent. The duty cycle is what protects the idle task, not the gap — an eight-second gap buys nothing and costs coverage."
  sources:
    - "ESP-IDF nimble_port_freertos.c: xTaskCreatePinnedToCore(host_task, 'nimble_host', NIMBLE_HS_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, &host_task_h, NIMBLE_CORE)"
    - "kernel log on hardware: 'rst:0x7 (TG0WDT_SYS_RST)', backtrace resolving to esp_restart_noos"
  verification: "Bisected on hardware: with the idle-task watchdog check disabled the same firmware ran indefinitely at load 0.19 with BLE up, proving starvation rather than a memory or coexistence fault; with bounded scanning and the watchdog fully armed it also runs indefinitely. The gap defect was then found by a user pressing keys on a sleeping keyboard and observing no connection attempt at all."

- record_id: NET-001
  kind: NETWORK
  topic_id: NET
  title: "WiFi is delivered by core 0 over shmem at about 409 KB/s and 30-50 ms to the gateway"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "Linux's WiFi interface is espsta0, backed by esp-hosted on core 0 communicating over a shared-memory IPC link rather than a hardware network device. Measured on this board: bulk transfer of 3 MB from host to watch completed in 7.34 s, about 409 KB/s; round trip to the gateway averaged 46 ms over 10 large pings with no loss; round trip to 1.1.1.1 averaged 33 ms; DNS through the gateway resolved normally."
  consequence: "It is a working data path, not merely an association. The throughput is modest because a single core arbitrates WiFi, the IPC link and its own work, and it competes with the BLE radio on the same silicon. Expectations of WiFi-speed transfers over this link should be scaled accordingly."
  sources:
    - "sdkconfig.defaults.esp32s3.16m8r: CONFIG_ESP_SHMEM_HOST_INTERFACE=y"
    - "measured on hardware 2026-11-09"
  verification: "Measured on hardware: nc bulk transfer timing, a 10-packet ping to the gateway, a ping to 1.1.1.1, and a DNS lookup, all after a clean boot."

- record_id: NET-002
  kind: NETWORK
  topic_id: NET
  title: "`wifi scan` proves the radio works, not the data path, and disrupts an associated link"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A scan returning networks demonstrates only that the radio and scan path function; it says nothing about whether packets are passing. This link has been observed associated with a valid address while dropping 100 percent of traffic. Repeated `wifi scan` invocations immediately before such a failure move the single shared radio off-channel and are a plausible contributor, though that specific attribution was never isolated."
  consequence: "Ping the gateway when testing connectivity, not scan. The board provides its own `wifi connect \"SSID\" \"passphrase\"` helper, which writes /etc/wpa_supplicant.conf and brings the link up; hand-writing wpa_passphrase output and starting wpa_supplicant manually is unnecessary and was the source of a wasted detour."
  sources:
    - "docs/bluetooth-keyboard.md: the retracted advertising/WiFi analysis and what survived from it"
    - "usr/sbin/ble-wifi-setup: uses `wifi connect` and `wifi scan --tsv`"
  verification: "The distinction was learned on hardware and is recorded in the retraction: an A/B that appeared to blame BLE advertising was actually measuring a reboot, because advertising was running in both arms."

- record_id: NET-003
  kind: NETWORK
  topic_id: NET
  title: "The firmware fork adds ESP_LOG_IF as interface 6, so the kernel's enum must match or every log frame is dropped"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The esp-hosted payload header carries an `if_type` byte naming the interface a frame belongs to, and both sides test it against `ESP_MAX_IF`. Upstream has six values, 0 through 5, with `ESP_MAX_IF` = 6. This project's firmware fork added a seventh, `ESP_LOG_IF`, for its own diagnostic stream, which makes its `ESP_MAX_IF` = 7. With the kernel's enum left at six, every firmware log frame arrives tagged 6, fails `if_type >= ESP_MAX_IF` in `process_rx_buf()`, and is dropped -- printing `process_rx_buf: bad if_type 6` to the console. The kernel accepted the frames from the moment the enum matched: `/dev/esp-log` appears and delivers lines such as `@LOG hid: scan 1 complete (connected 0)`."
  consequence: "The interface enum is a shared ABI between firmware and kernel and nothing enforces it; a one-entry divergence silently discards a whole stream. The match has to be made on both sides or the failure is invisible apart from console spam. Two further facts from this: the frames were not merely rejected but never had a consumer, because `esp_log_pipe.c` was carried in the tree but never compiled, so `/dev/esp-log` did not exist at all; and accepting the type without a consumer would be worse than rejecting it, because those skbs would queue in `rx_q` forever. The enum entry and the driver must land together."
  sources:
    - "patches/kernel/0007-wireless-esp32-ng-add-ESP_LOG_IF-and-log-pipe.patch: the enum entry, the dispatch and the driver wiring"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/include/adapter.h: the fork's enum with ESP_LOG_IF"
    - "drivers/net/wireless/espressif/esp32-ng/esp_shmem.c: process_rx_buf() and its if_type >= ESP_MAX_IF test"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/log_pipe.c: the emitter, which records that a flood of these frames competes for the same shmem link the kernel boots over"
  verification: "Verified on hardware 2026-11-09: before the change the console carried `process_rx_buf: bad if_type 6` hundreds of times per boot and `/dev/esp-log` did not exist; after it, zero such errors in the first minutes and `/dev/esp-log` exists and delivers real firmware lines. The enum divergence itself was established by reading both headers side by side."

- record_id: TOL-001
  kind: TOOLCHAIN
  topic_id: TOL
  title: "Without XTENSA_GNU_CONFIG the compiler silently emits big-endian objects and the link fails misleadingly"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The Xtensa compiler reads its core description from the file named by the XTENSA_GNU_CONFIG environment variable at run time. Unset, it falls back to a generic core that is big-endian, so it emits ELFDATA2MSB objects while the uClibc in the sysroot is little-endian. The link then fails with 'error adding symbols: file in wrong format', naming libgcc_s.so.1 first and libc.so.1 next; with static linking it reaches 'crt1.o: compiled for a little endian system and target is big endian'."
  consequence: "None of those messages mentions the missing variable, and -mlittle-endian is not a recognised option because the endianness comes from the core description rather than a flag, so the toolchain reads as broken. It is not. XTENSA_GNU_CONFIG must point at build/toolchain/xtensa-dynconfig/esp32s3.so; readelf -h on a test object must report little endian before any link is trusted."
  sources:
    - "docs/toolchain.md: section 5, 'XTENSA_GNU_CONFIG is not optional, and its absence looks like a broken linker'"
    - "scripts/build-userspace.sh: the export and the reasoning"
  verification: "Reproduced and fixed on the host: objects compiled without the variable reported '2's complement, big endian' and every link failed; setting it to esp32s3.so produced little-endian objects and a working binary."

- record_id: TOL-002
  kind: TOOLCHAIN
  topic_id: TOL
  title: "crosstool-NG Xtensa/uClibc GCC 14.1.0 produces dynamically linked binaries against libc.so.0"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The cross toolchain is crosstool-NG's xtensa-esp32s3-linux-uclibcfdpic sample: GCC 14.1.0, binutils 2.42.50, uClibc-ng. Binaries it produces are ELF 32-bit LSB for the Tensilica Xtensa processor, dynamically linked with interpreter /lib/ld-uClibc.so.0 and a NEEDED entry for libc.so.0. The multilib is fdpic."
  consequence: "Dynamic linking is the default and the simplest path here; -static requires -mfdpic and additional care, and a plain -static produced objects the linker rejected. Both the interpreter and libc.so.0 are present on the watch, so an ordinary dynamic build runs without deploying any libraries."
  sources:
    - "docs/toolchain.md: the pinned revisions and the sample name"
    - "readelf -h and readelf -d on binaries built for the watch"
  verification: "Toolchain version and the resulting ELF headers read on the host; the binaries run on the watch, which is the operative proof."

- record_id: TOL-003
  kind: TOOLCHAIN
  topic_id: TOL
  title: "A non-recursive patch glob plus `|| true` is how whole patch series disappear without an error"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "In a shell loop over `patches/kernel/*.patch`, the glob matches only files directly in that directory. Patches under a subdirectory such as patches/kernel/axp2101/ are never seen, and wrapping the apply in 'git apply ... || true' discards the failure. Both defects were present in this project's kernel build, and each removed a patch series from the build without any message: first a glob of 0001-* that skipped the patch registering the display driver, then the non-recursive form that skipped the entire AXP2101 series."
  consequence: "A build can succeed, boot, and be missing drivers, with the device tree still describing hardware nothing binds to. Patch loops must enumerate every subdirectory deliberately and must report a patch that neither applies nor is already applied. Where a file is edited faster than a patch can track it, a whole-file copy installed after the patch stack is the more honest carrier."
  sources:
    - "scripts/build-kernel.sh: the loop and the comment recording both incidents"
    - "patches/kernel/axp2101/README.md"
  verification: "Diagnosed on hardware by finding /sys/class/power_supply empty while the DTB declared a PMU: grep -c axp2101 drivers/mfd/axp20x.c returned 0 in the built tree. Fixed by making the glob recursive and reporting failures."

- record_id: EXT-001
  kind: EXTERNAL
  topic_id: EXT
  title: "The SPLB32 Tamagotchi core reaches 4.05x realtime headless and about 1.04x while painting 4 fps"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The TamaHarness portable SPLB32 core, executing the real 512 KB Tamagotchi Connection V3 ROM, runs on this watch at 4.05x real time with no display (16.2 million emulated cycles per second against a 4 MHz target), and at 1.04x while also painting the panel at 4 frames per second. Its correctness is established separately by its authors: instruction traces and register-file hashes identical to a Python reference over 10 million instructions, all 151 opcodes checked individually, and 1200 fuzzed programs with no divergence."
  consequence: "The emulator is fast enough for its purpose on this hardware and the panel is what limits it (LCD-005), so display work benefits this workload more than CPU work would. Two contiguity constraints (KRN-001) must be respected to run it at all: the ROM cannot live in .bss, and a full frame cannot be one allocation."
  sources:
    - "TamaHarness for-escapement/RESULTS-3.md and tamabench-on-watch.txt"
    - "TamaHarness portable VALIDATION.md and BUGS.md for the correctness claims"
  verification: "All figures measured on hardware in a single process on one boot, with frame cost derived by subtracting the separately measured headless rate. The rendered output was read off the panel and confirmed to be the expected egg sprite. Revised 2026-11-09 (core-log entry 14): re-measured with the ROM read from flash rather than allocated, at 320x400 and 4 fps, giving 1.00x realtime -- 16 frames in 4 s at 3833499 emulated cycles/s. Earlier figures used the desktop front end's 324x348 geometry and a malloc'd ROM."

- record_id: EMU-001
  kind: EMU
  topic_id: EMU
  title: "The SPLB32 interpreter must use fixed-point, because doubles are soft-float and floats fault"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "On this kernel a double operation costs 711 ns and the integer equivalent 121 ns, because the Xtensa has no hardware double and every double add is a soft-float library call (__adddf3 in the binary). SPLB32's reference implementation keeps its cycle and timer accumulators in double, and timers_clock() runs once per emulated instruction touching three of them. Converting those to 64-bit fixed-point with CYC_SHIFT=7 fractional bits halves the interpreter's cost: measured on the watch, emulator time per frame at 10 fps fell from 50.8 ms to 26.4 ms. The conversion is exact, not approximate: the sleep tick is 4000000/32768 = 122.0703125 = 15625/128, so at 7 fractional bits it is the integer 15625 and nothing is truncated."
  consequence: "Fixed-point is the only option, and the two alternatives are traps. Single-precision float is not slow -- it faults: the compiler happily emits add.s/mul.s because the ESP32-S3 FPU exists, but the kernel does not enable the coprocessor for user tasks, so executing them raises a coprocessor-disabled fault and the process dies with SIGSEGV (exit 139, reproduced). Making float usable means coprocessor context switching on a NOMMU port with a patched fork(), which is far more risk than using integers. Equivalence of the fixed-point core was proven rather than assumed: over 200M cycles the original and converted cores agree exactly on instrs, pc, registers, ram_fnv and dpram_fnv, and a button-press run matches to the tenth of a millisecond. One bug was caught by that comparison and would otherwise have shipped: TM0L/TM0H/TM1L/TM1H are guest-visible as raw 16-bit cycle counters and need the fraction shifted off, or the firmware reads a value 128x too large."
  sources:
    - "TamaHarness/portable/splb32.c: timers_clock(), splb32_step(), the SFR_TM0L/TM0H/TM1L/TM1H read cases"
    - "TamaHarness/portable/splb32.h: CYC_SHIFT, CYC_ONE"
    - "TamaHarness/portable/con_v3_ir.c: the IR model's counters, converted to the same scale"
    - "docs/tamagotchi.md: the measurements and the equivalence runs"
  verification: "Measured on hardware 2026-11-09. Double 711 ns vs int 121 ns; emu per frame 50.8 -> 26.4 ms; float arithmetic segfaults. Equivalence checked on the host against the unmodified core at 40M and 200M cycles and across a simulated button press."

- record_id: EMU-002
  kind: EMU
  topic_id: EMU
  title: "Entering the Tamagotchi time screen takes about 1.5 s, and the cause is known but not fixed"
  status: OPEN
  verified_date: 2026-11-09
  statement: "Pressing the middle touch bar takes roughly 1.5 s before the clock screen appears, and during that time the pet stops animating. The ROM is responsible for 580.1 ms of it: measured on the host in emulated cycles, the guest makes no change to display RAM for 580.1 ms after a press, ending at +620.1 ms. On the host that window runs at 211x realtime, so the ROM's own work is not where the time goes there. On the watch, frames during the transition take 196-399 ms against a 100 ms budget, with emu 123-240 ms and panel 43-135 ms, so the loop falls behind and emulated time stretches -- which is how 580 ms becomes about 1.5 s."
  consequence: "Parked by decision on 2026-11-09: the emulator runs well otherwise and the remaining work is not worth the risk right now. Do not re-tread the paths already excluded by measurement: touch polling (15-byte read 3957 us, DRV2605 click 1796 us, polling now 15 ms at 8% of throughput), the paint path (band masking already skips 62% of transfers, and mid-frame painting was reverted because it tore the picture), and SPI clock (40 MHz buys 13%, not 2x, because the transfer is not clock bound). The unexplained observation is the panel figure: 43-135 ms per frame during a stall against 9.5 ms in the benchmark, a 10x gap with no established cause. The next measurement, if this is picked up, is to read the stall log against a press on the fixed-point build to see what the guest is executing in those 580 ms."
  sources:
    - "docs/tamagotchi.md: 'Parked: the ~1.5 s wait entering the time screen'"
    - "TamaHarness/portable/tamafb.c: the per-frame stall log and the press-to-paint timestamps"
  verification: "The 580.1 ms ROM gap is measured on the host and reproducible. The 1.5 s on hardware is reported by the user and consistent with the measured stall frames, but the exact breakdown was never captured on device -- the press-to-paint instrumentation was deployed and never read against a press."

- record_id: LCD-011
  kind: DISPLAY
  topic_id: LCD
  title: "40 MHz is safe on this panel, and doubling the SPI clock buys only 13 percent"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A full 320x400 RGB565 frame costs 115.15 ms to push at 20 MHz and 100.85 ms at 40 MHz, measured back to back on hardware through /dev/fb0 pwrite, with the 20 MHz figure reproduced after the 40 MHz run (116.19 ms) to rule out drift. 40 MHz is clean on the glass: eight full-width bands in eight distinct colours came out as eight equal horizontal bands in the correct order, which a desynced write pointer cannot produce because it repeats the first rows instead of reaching the bottom of the screen. 40 MHz is now the driver's compiled-in default."
  consequence: "The transfer is not clock bound, so raising the clock is not the lever it looks like. Wire speed at 20 MHz quad is 10 MB/s, which is 25 ms for 250 KB, against 115 ms measured -- 4.6x off wire speed. The time is in the 64-byte FIFO chunk loop in esp32_spi_exec_op and in the per-pixel swab16 pass, not in shifting bits, so DMA is the change that matters and the clock is only worth taking because it is nearly free. It also settles the reference firmware's 45 MHz shearing, which was a write-pointer desync from the command/address-resent-per-chunk bug rather than a clock-margin limit; that bug is fixed here and the same clock is clean. This does NOT explain LCD-009's narrow-transfer fragility, which persists at 40 MHz and so is not a clock effect."
  sources:
    - "userspace/fbtime.c: paints eight bands and times repeated full-frame pwrites"
    - "patches/kernel/display/final/co5300.c: CO5300_SPI_HZ_DEFAULT, and the spi_hz module parameter that made the comparison a single-boot test"
  verification: "Measured on hardware 2026-11-09 by the user's own eyes on the glass for the band order and the absence of shear, and by wall-clock timing for the two frame costs. The SPI frequency was changed at runtime through /sys/module/co5300/parameters/spi_hz, which assigns spi->max_speed_hz so the divider is reprogrammed at the next exec_op."


- record_id: LCD-012
  kind: LCD
  topic_id: LCD
  status: VERIFIED
  verified_date: 2026-11-10
  title: "SPI DMA is not usable from this port today: the GDMA descriptor address field is 20 bits and Linux owns no internal DRAM"
  statement: "The ESP32-S3 GDMA out-link register stores only the low 20 bits of the first descriptor's address (`out.link.addr : 20` in gdma_struct.h), so a descriptor must live in the low 1 MB of the address space, which on this chip is internal DRAM at 0x3FC88000. Linux on this port owns no internal DRAM: the device tree's memory node is a single block at 0x3D800000 of 8 MB, the whole of PSRAM, so a descriptor allocated through the kernel allocator lands in PSRAM and outside the 20-bit range. This is not an MMU limit -- internal SRAM is directly addressed and needs no mapping (SOC-004) -- it is an allocation limit: nobody has carved a region of 0x3FC88000 out of the firmware's heap and handed it to Linux."
  consequence: "Replacing the 4000 CPU-mode FIFO chunks per frame with a handful of DMA transfers is not possible on this port, and no amount of driver work changes it. This is worth recording because the idea is attractive and will be proposed again: the transfer is 83 percent per-transaction overhead, so fewer transactions is the obvious fix, and DMA is the obvious way to get them. It cannot be done without giving Linux a region of internal DRAM, which is a firmware-plus-kernel layout change of a different order -- and the pattern for it already exists in space_for_vectors, where the firmware reserves 4 KiB of internal IRAM and the kernel is told the address (SOC-004). Two related facts, both checked so they are not re-investigated: the esp-hosted link on this board is shmem (CONFIG_ESP_SHMEM_HOST_INTERFACE=y), not SPI and not SDIO, so networking claims no SPI DMA path and the usual worry about fighting the WiFi blob for a GDMA channel does not apply; and the firmware's UHCI DMA channels in slave_bt.c are never initialised because this build drives BT through the internal controller via NimBLE, so a GDMA channel dump taken at 2.1 s showed all five channels zero. None of that helps, because the limit is addressing, not contention."
  sources:
    - "components/soc/esp32s3/include/soc/gdma_struct.h: `uint32_t addr : 20;` with the comment that it stores the 20 least significant bits of the first outlink descriptor's address"
    - "components/soc/esp32s3/include/soc/gdma_reg.h: GDMA_OUT_LINK_CH0_REG, OUTLINK_ADDR bitpos [19:0]"
    - "components/hal/include/hal/dma_types.h: dma_descriptor_t, 12 bytes, owner bit, max 4095 bytes per descriptor"
    - "the device tree memory node: a single 8 MB block at 0x3D800000, with no internal DRAM region; SOC-004 for why that is an allocation fact rather than an addressability one"
    - "components/esp_hw_support/dma/gdma.c: `esp_ptr_internal(user_data)` checks, the same constraint expressed as a runtime test in ESP-IDF"
  verification: "Established by reading the register definitions and the device tree, then confirmed empirically: a GDMA occupancy dump added to esp32_spi_probe read all five channels as zero at 2.1 s, before the shmem link came up at 4.2 s. No DMA code was written, because the descriptor address range makes it unimplementable rather than merely risky. Revised 2026-11-11 (core-log entry 53): this record said Linux had no internal SRAM mapping at all and that DMA was therefore unimplementable. That overstated it. Internal SRAM is directly addressed and needs no MMU entry, and the kernel already executes at 0x40380000, so the obstacle is that no internal DRAM is allocated to Linux rather than that none can be reached. See SOC-004."

- record_id: LCD-013
  kind: LCD
  topic_id: LCD
  status: VERIFIED
  verified_date: 2026-11-10
  title: "A framebuffer write costs 94 ms per full frame and is not wire bound: half the SPI clock buys only 15 percent"
  statement: "Measured in-process on /dev/fb0 with fbbench, which is the only honest instrument available: a full 320x400 RGB565 frame (256000 bytes) costs 93-95 ms at 40 MHz, and 108.8 ms at 20 MHz. Halving the SPI clock costs only 15 percent, so the transfer is not limited by shifting bits. Banded writes cost the same in total: 10 bands of 40 rows cost 10.0 ms each, 100 ms per frame."
  consequence: "The cost is per-transaction turnaround, not bytes and not MMIO. 256000 bytes over a 64-byte FIFO is 4000 transactions at about 23 us each, of which wire time at 4 lanes and 40 MHz is about 3.2 us -- so roughly 20 us of every transaction is fixed overhead that lives in the SPI engine, outside the CPU's reach, which is exactly why halving the peripheral clock barely moved it. A follow-up optimisation that removed 5 of the 8 MMIO register writes per chunk (a redundant SPI_USER rewrite, a redundant SPI_MS_DLEN rewrite, and a SPI_UPDATE latch with its poll loop on every one of 4000 chunks) measured as a no-op: 94.8 ms before, 93.3 ms after. That result is kept deliberately, because it rules out MMIO as the cost and points the next attempt at fewer transactions or fewer bytes rather than faster ones. Two measurement traps are recorded with it: `dd if=/dev/zero of=/dev/fb0` is useless here because a dd that touches nothing costs 430-440 ms on this board, so 25 KB and 256 KB both 'take' about 440 ms; and deriving a per-band figure from the emulator's stall log gives 9.3 ms from one frame and 25 ms from another, a 2.7x spread that cannot be resolved by arithmetic."
  sources:
    - "userspace/fbbench.c: one process, CLOCK_MONOTONIC, full-frame and banded timings"
    - "on the watch: ./fbbench -n 12 at 40 MHz and at 20 MHz, the clock set through /sys/module/co5300/parameters/spi_hz"
    - "patches/kernel/display/final/spi-esp32.c: the 64-byte chunk loop in esp32_spi_exec_op"
    - "LCD-011 for the earlier 100.85 ms figure this reproduces independently, and LCD-010 for where the time was thought to be"
  verification: "Measured on hardware 2026-11-10, after the operator confirmed the display was correct on the glass following the chunk-loop change. Consistent across runs: 93.3, 94.8, 94.0 and 94.8 ms for a full frame at 40 MHz across three boots; 108.8 ms at 20 MHz; banded 9.99-10.30 ms per band."
- record_id: EMU-003
  kind: EMU
  topic_id: EMU
  title: "PSRAM costs 179 ns per dependent load, and that is what bounds the emulator"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "A dependent-load chain over a 3 KB working set costs 178.83 ns per load on this watch, measured with a pointer-chase over a static array. At the reported 240 MHz that is about 43 CPU cycles for a single load. The SPLB32 emulator keeps its entire machine state -- 2968 bytes -- in a heap allocation at 0x3ddb0008, which is PSRAM, and its static .bss lands at 0x3dc708f0, which is also PSRAM. The interpreter therefore performs roughly a dozen PSRAM round trips per emulated instruction. Measured consequence: 387212 steps/s and 265k instructions/s, about 620 CPU cycles per emulated instruction, and 57.74 emulated cycles per instruction retired."
  consequence: "This, not the panel, is what limits the emulator, and it explains every symptom seen on hardware. Steady state the guest sleeps and cycles are nearly free, so the emulator reports about 1.00x real time; when the ROM actually computes -- opening a menu, drawing a text screen -- the sleep disappears, steps go up, and throughput collapses to about 0.38x, which stretches a 580 ms screen transition to about 1.5 s. It also explains why the fixed-point conversion helped more than its arithmetic share: removing doubles removed dependent loads, not just soft-float calls. Two consequences for anyone optimising further. First, the levers already tried and excluded by measurement stay excluded -- SPI clock bought 13 percent, DMA would remove a per-chunk cost that is not on the critical path, and frame rate does not touch instruction throughput. Second, the only lever with the right magnitude is getting the hot state into internal SRAM: internal memory is on-die and the state is under 3 KB, so the per-access cost there would be a few cycles instead of 43. That is not available today, but the reason is an allocation fact rather than an addressability one: the device tree declares exactly one memory region, memory@0 at 0x3d800000 for 8 MB of PSRAM, and the firmware owns all of internal DRAM, so the kernel has nothing to allocate from there. Internal SRAM is directly addressed and needs no MMU entry, and the kernel already executes in 4 KiB of it (SOC-004). So PSRAM latency is the floor for this port as it stands, and raising it means reserving internal DRAM for Linux the way space_for_vectors reserves internal IRAM. What remains is to reduce the NUMBER of dependent loads rather than their cost. Revised 2026-11-10 (core-log entry 21): the per-access cost in this consequence is overstated -- the load is about 53 ns, or 13 cycles, not 43 -- and the number of loads per emulated instruction is now measured at 2.408 rather than estimated at a dozen (EMU-006). That arithmetic is what actually leaves room: memory is a minority of the ~1181 native cycles an emulated instruction costs, so most of the time is the interpreter's own instruction stream, which a block-register rewrite can cut."
  sources:
    - "userspace/memlat.c: the pointer-chase timing, deployed and run on the watch"
    - "TamaHarness/portable/tamafb.c: the bench path that prints instr/s, cycles/instr, and the address of the cpu state"
    - "TamaHarness/portable/splb32.c: SPLB32_PROF counters for steps, sleep steps and IRQs"
  verification: "Measured on hardware 2026-11-10. 178.83 ns dependent load; cpu state at 0x3ddb0008 and .bss at 0x3dc708f0, both in the PSRAM window; 265k instr/s and 387k steps/s from the on-device bench. The cache question is now settled and the answer is that there is no cache: the core configuration the kernel and toolchain are built against declares XCHAL_DCACHE_SIZE=0 and XCHAL_ICACHE_SIZE=0, with XCHAL_HAVE_MMU=0 and no MMU/MPU/TLB support at all. A three-working-set pointer-chase (4 KB, 256 KB, and a second 256 KB buffer) confirms it from the other direction: 70.30, 68.72 and 78.93 ns -- no locality effect whatsoever, which is impossible if a data cache were operating. So CONFIG_MEMMAP_CACHEATTR is irrelevant here; there is nothing behind it to configure. Note also that CONFIG_MEMMAP_CACHEATTR's PSRAM nibble is 2, which the Kconfig help defines differently per MMA type, so reading it as 'bypass' would have been the wrong conclusion reached from the wrong evidence. Revised 2026-11-11 (core-log entry 53): the claim that both escape routes were closed and that all internal SRAM belonged to the firmware and ROM was wrong in the direction that matters -- the kernel already runs code at 0x40380000, so internal SRAM is reachable and in use. The real limit is that no internal DRAM is allocated to Linux. See SOC-004. Revised 2026-11-10 (core-log entry 21): the load figure was wrong. The dependent-load latency of PSRAM on this board is 50.6 to 53.4 ns, not 178.83 ns, and the 178.83 figure came from a run of the original memlat.c that was crashing: that program malloc()s 1 MB and the second, 64 KB test faulted, which on this NOMMU heap happens intermittently, so the number in this record was taken from a process that did not complete. Re-measured with a control that performs identical arithmetic on PSRAM, 52.8/53.4/53.1 ns and 55.7/57.0 ns, reproducible across runs. The 'roughly a dozen PSRAM round trips per emulated instruction' is also now measured rather than assumed: 2.408 guest bus accesses per retired instruction, of which 72.6 percent are ROM reads (EMU-006). The consequence of both corrections is that memory access is a much smaller share of the interpreter's cost than this record claimed, which makes the userspace lever larger, not smaller."

- record_id: EMU-004
  kind: EMU
  topic_id: EMU
  title: "Core 0 has the cache; Linux runs on the core that does not"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "Both ESP32-S3 cores are identical Xtensa LX7 at 240 MHz, but they are not equivalent for compute. Cache on this chip is enabled and owned by the firmware, which is how ESP-IDF runs from flash-mapped memory at usable speed; the core configuration the Linux kernel and toolchain are built against declares XCHAL_DCACHE_SIZE=0 and XCHAL_ICACHE_SIZE=0 (EMU-003). Core 0 runs the ESP-IDF firmware and its radios and is idle apart from network and esp-hosted traffic. Linux runs on core 1 alone: CONFIG_HAVE_SMP is not set in the built config, and the LX7 config declares XCHAL_HAVE_EXCLUSIVE=0, so the atomic exclusive operations an SMP kernel needs to synchronise are absent -- SMP is not merely unconfigured, it is not supportable on this core."
  consequence: "This is the architectural reason the SPLB32 emulator is slow, and it is a different answer from every optimisation attempted this session. The emulator runs on the one core of two that has no cache, at roughly 30 ns per random dependent PSRAM access, and it has no internal SRAM to put its hot state in because the kernel allocates nothing from there -- not because internal SRAM is unreachable (SOC-004). The same interpreter on core 0 would run with cache and with the firmware's internal SRAM available to it. The design that follows is to move the emulation to core 0 -- cached, fast -- have it write its 292x300 display into a shared buffer, and leave core 1 running a thin program that blits that buffer to fb0, so compute happens on the cached core and only drawing happens on the core that owns SPI. That is not a small change and it must be weighed against three recorded constraints: core 0 owns the WiFi link this project's console depends on, and starving it trips the FreeRTOS task watchdog (BLE-004, rst:0x7 TG0WDT_SYS_RST); core 0's flash path must skip the inter-core park meant for Linux and must not touch PSRAM while the shared cache is off (JFS-001); and the firmware tree must never be pushed, its origin being upstream jcmvbkbc/esp-hosted rather than this project."
  sources:
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build/network_adapter.map: iram0_0_seg 0x40374000 size 0x58700, dram0_0_seg 0x3fc88000 size 0x54700, _heap_start 0x3fca68f0"
    - "build/kernel/linux-7.2.4/.config: # CONFIG_HAVE_SMP is not set"
    - "build/toolchain/xtensa-dynconfig/esp32s3.so: XCHAL_DCACHE_SIZE=0, XCHAL_ICACHE_SIZE=0, XCHAL_HAVE_EXCLUSIVE=0"
  verification: "Revised 2026-11-11 (core-log entry 53): 'no reachable internal SRAM' was wrong. Internal SRAM is directly addressed and the kernel already executes in 4 KiB of it at 0x40380000; what core 1 lacks is any allocation of the rest. See SOC-004, which also records how to give it some. Config and core facts read from the built artefacts rather than assumed. The claim that core 0's cache is enabled is inferred from ESP-IDF's normal operation, not measured on this unit -- the firmware does not report its cache configuration, so if the emulator is moved to core 0 that inference should be confirmed by measurement before the design is committed to."

- record_id: EMU-005
  kind: EMU
  topic_id: EMU
  title: "The interpreter retires far more native instructions per emulated instruction than it needs to"
  status: OPEN
  verified_date: 2026-11-10
  statement: "Per emulated instruction the interpreter spends about 3676 ns, against a measured yardstick of 12.77 ns for a simple native operation (a dependent-free add chain, implying roughly 78 million simple operations per second). That is on the order of 250 native instructions per emulated instruction once the memory accesses within it are costed at their measured 26-38 ns rather than at the arithmetic rate. A tuned 6502 interpreter retires one guest instruction in 30 to 80 native instructions, so this core is running 3 to 8 times heavier than it needs to."
  consequence: "Reducing native work per emulated instruction is the only optimisation with the right magnitude that does not require firmware changes, and it is the recommended next step over moving the emulator to core 0 (EMU-004), which is larger and riskier. The concrete shape: keep the guest's registers, flags and program counter in host variables across a decoded block instead of reloading each field from PSRAM per access, and use compact table-driven or computed-goto dispatch so that handlers are not spread across a 6796-instruction switch on a core with no I-cache. Expected 2-3x, which would take the menu from 0.38x to roughly 0.9-1.15x. NOT YET MEASURED: the native-instruction count per emulated step. Two attempts to run the decomposition benchmark on hardware were defeated by a deploys that failed for lack of memory, so the 250 figure is derived from timing, not counted. Counting it is the first task for whoever picks this up."
  sources:
    - "TamaHarness/portable/splb32.c: exec_op_sem, read8/write8, splb32_step"
    - "docs/tamagotchi.md: 'Why the emulator crawls when the ROM computes'"
  verification: "The 3676 ns per step and the 12.77 ns yardstick are measured on hardware. The native-instruction count is derived, not counted -- explicitly unresolved. Of 393 instructions sampled from exec_op_sem, 39 are memory operations (10 percent), which bounds how much of the cost can be memory access within the handler itself. Revised 2026-11-10 (core-log entry 21): the interpreter's shape is now counted rather than derived, by corebench built with SPLB32_PROF. One retired guest instruction costs 1.622 interpreter steps -- exactly one opcode step, since 736660 steps minus 282355 sleep steps is 454305, the instruction count to the digit -- and 2.408 guest bus accesses. An emulated instruction costs 4922 ns on the standalone corebench, about 1181 native cycles at 240 MHz. Subtracting the measured memory cost (2.408 accesses at about 53 ns is roughly 128 ns, or 2.6 percent) leaves the native work at the order of 150 simple operations per emulated instruction, in the same range as the derived 250 and still 2 to 5 times the 30-80 a tuned core needs. The rewrite therefore still has room, and memory is not what is eating it."
- record_id: EMU-006
  kind: EMU
  topic_id: EMU
  title: "One emulated instruction costs 1.622 interpreter steps, 2.408 guest bus accesses and about 1181 native cycles"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "Running the real V3 ROM for 40000011 emulated cycles on corebench, with the prof counters enabled, retires 454305 instructions in 736660 interpreter steps and 282355 sleep steps, performing 1032471 reads and 61692 writes. That is 1.622 steps and 2.408 bus accesses per retired instruction. The steps figure is exact rather than approximate: 736660 minus 282355 is 454305, so the interpreter takes exactly one step per opcode and the extra 0.622 per instruction is the sleep ticks between them. The accesses split as 72.6 percent ROM, 21.4 percent RAM, 0.3 percent SFR and no DPRAM at all. Wall-clock on the same workload without the counters is 2.236 s, so one emulated instruction costs 4922 ns, about 1181 cycles at 240 MHz, and the run sustains 204045 instructions/s and 4.49x realtime against the 4 MHz target."
  consequence: "This is the decomposition the earlier records could only infer, and it changes what is worth optimising. Memory is not the cost: 2.408 accesses at the measured 53 ns is about 128 ns, roughly 2.6 percent of the 4922 ns an instruction costs, so the remaining 97 percent is the interpreter's own instruction stream. A block-register rewrite attacks exactly that and is therefore the right next lever, while reducing accesses would be chasing 3 percent. It also gives the rewrite a baseline to be judged against, which did not exist before: 204045 instructions/s, 4.49x realtime, 1181 cycles per emulated instruction. The 72.6 percent ROM share matters separately and is the subject of EMU-008."
  sources:
    - "TamaHarness/portable/corebench.c: the counter report added under SPLB32_PROF, which subtracts a warm-up snapshot because the counters are process-global and nothing resets them"
    - "TamaHarness/portable/splb32.h and splb32.c: splb32_prof_steps, splb32_prof_sleep_steps, splb32_prof_reads/writes and the four address-class counters"
    - "scripts/build-corebench.sh: the corebench-prof target"
  verification: "Measured on hardware 2026-11-10 with corebench-prof, deployed to /home and run twice with identical counts. The ROM was verified in the flash slot first: romverify reported crc32 301F0E87 and first bytes A5 5F 10 49, matching EXT-002, and the identical instruction count of 454305 against the host build confirms the ROM actually read is the right one. The 4922 ns per instruction comes from the plain build in three consecutive runs, 204045, 203945 and 203488 instructions/s, because the counters perturb timing and must not be used for it."

- record_id: EMU-007
  kind: EMU
  topic_id: EMU
  title: "Instruction fetch from PSRAM is not the interpreter's cost; expanding code 8x costs about 4 percent"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "With XCHAL_ICACHE_SIZE=0 every instruction the interpreter fetches comes from PSRAM, which raised the possibility that the fetch itself, rather than the interpreter's arithmetic, is what an emulated instruction costs. Measured with three tests performing identical arithmetic and identical dependent table loads and differing only in code footprint: a 64-byte loop costs 56.83 ns per operation, the same body unrolled 16 times over about 1 KB costs 30.52 ns, and a 64-case switch of about 8 KB entered in a jumping order costs 31.69 ns. Going from about 1 KB of code to about 8 KB costs 3.8 percent."
  consequence: "Instruction fetch is covered well enough that a larger code footprint is nearly free, so an interpreter rewrite does not have to fear growing its code -- which matters, because the block-register design in EMU-005 is a bigger and less cache-friendly body than the current 6796-instruction switch. It also rules out fetch as the explanation for the per-instruction cost, leaving the amount of arithmetic the interpreter does, which is what EMU-006's 97 percent points at. The small-loop figure being the slowest is the expected artifact of loop overhead against a long unrolled run, not a footprint effect."
  sources:
    - "userspace/codecost.c: the three-variant footprint benchmark and its control design"
  verification: "Measured on hardware 2026-11-10, twice, with per-op costs reproducing to within 1 percent (56.83/30.52/31.69 and 56.74/30.45/32.19). The table is seeded from the clock and the trip counts pass through a volatile, because a first version with compile-time-visible data and bounds was folded by the compiler and reported 0.00 ns/op for the middle variant -- a reminder that a benchmark which can be optimised away will be."

- record_id: EMU-008
  kind: EMU
  topic_id: EMU
  title: "A read from the linear flash window costs 738 ns against 53 ns for PSRAM, and the ROM is 72.6 percent of the emulator's bus accesses"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "The linear flash window is readable from userspace (EXT-002), but it is not fast. A dependent load costs 50.6 to 53.4 ns from PSRAM and 737.23 to 757.05 ns from the flash window at 0x42480000, a factor of about 14, and the figure does not depend on the working set: 738.04 ns over a 256-byte window and 757.05 ns over 16 KB. The control that isolates it runs byte-identical arithmetic over a PSRAM buffer and reports 53.13 to 55.67 ns, so the difference is the window and not the benchmark. Separately, 72.6 percent of the emulator's guest bus accesses are ROM reads (EMU-006), which is what makes this matter."
  consequence: "The ROM's location has a real price that EXT-002 did not state, because 'costs no RAM' is true and 'costs nothing' is not. At 2.408 accesses per instruction and 72.6 percent of them ROM reads, the ROM contributes on the order of 1.75 accesses at 738 ns if every one cost the full dependent latency, which is 1290 ns of the 4922 ns an emulated instruction costs -- an upper bound, because the interpreter does not stall on every ROM read the way a pointer chase does. The effect was predicted at a fifth to a quarter of throughput and has since been tested: moving the ROM into PSRAM buys 33.8 percent, more than predicted, and the measured saving of 1239 ns per emulated instruction matches the 1290 ns the prediction implies to within 4 percent (EMU-009). So the ROM's location is no longer a lead, it is a known and quantified cost. Testing it requires a fresh boot, because a 512 KB contiguous block does not survive long -- see KRN-004 for how short that window is."
  sources:
    - "userspace/memlat.c: chase() over PSRAM and chase_flash() over the window, with chase_ctl() as the same arithmetic on PSRAM"
    - "TamaHarness/portable/corebench.c: the SPLB32_PROF address-class counters"
    - "EXT-002 for the window itself and KRN-001 for why the PSRAM alternative is hard"
  verification: "Measured on hardware 2026-11-10 across two runs of the same binary and reproduced to within 2 percent. The window's identity was confirmed before timing it: romverify read crc32 301F0E87 and first bytes A5 5F 10 49 at 0x42480000, so the accesses being priced are real flash reads of the real ROM and not a zero page."

- record_id: EMU-009
  kind: EMU
  topic_id: EMU
  title: "Putting the ROM in PSRAM instead of the flash window buys 33.8 percent, and the saving matches the predicted ROM cost"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "The same workload -- 40,000,011 emulated cycles of the real V3 ROM, 454305 instructions -- runs at 204033 instructions/s and 4.49x realtime with the ROM read from the memory-mapped flash window, and at 273098 instructions/s and 6.01x with the ROM in PSRAM. That is a gain of 33.8 percent, or 1239 ns saved per emulated instruction: 4901 ns down to 3662 ns, that is 1176 native cycles down to 879. Nine runs of the flash variant across three boots gave 203008 to 205065 instructions/s with no overlap against the PSRAM figure, and two PSRAM runs on two separate boots gave 273115 and 273080."
  consequence: "This closes the lead EMU-008 opened and it does so with the mechanism confirmed rather than merely observed. The prediction was that the ROM's 1.748 accesses per instruction, at 738 ns each, would account for 1290 ns; the measured saving is 1239 ns, within 4 percent, which says the interpreter was indeed stalling close to full dependent latency on most of its ROM reads. Three things follow. The emulator should read its ROM from PSRAM, not from the flash window -- 6.01x instead of 4.49x is most of what the block-register rewrite was expected to buy, for far less work. That reintroduces the 512 KB contiguous allocation, which is why the emulator must start early from rcS while the block still exists (KRN-004). And the flash window should not be used for anything performance-sensitive again: costing no RAM is not the same as costing nothing."
  sources:
    - "TamaHarness/portable/corebench.c: the ROM_FLASH_ADDR path against the malloc'd-file path, differing only in where the ROM lives"
    - "scripts/build-corebench.sh: the corebench-flash and corebench-rom targets"
    - "EMU-006 for the access counts and EMU-008 for the per-access cost"
  verification: "Measured on hardware 2026-11-10 across three boots. Both arms ran with the touch daemon stopped, because it costs about 30 percent on its own (EMU-010), and the flash runs were interleaved before and after the PSRAM run to rule out drift. The determinism check held throughout: every run on both arms retired exactly 454305 instructions at 88.05 emulated cycles per instruction, so the two arms executed identical work and only the memory differed."

- record_id: EMU-010
  kind: EMU
  topic_id: EMU
  title: "The touch daemon costs the emulator about 30 percent of its throughput"
  status: VERIFIED
  verified_date: 2026-11-10
  statement: "With /home/tbtns running, corebench-flash sustains 140491 to 142949 instructions/s on a board at load average 1.37. With the daemon stopped, the same binary on the same boot sustains 202019 to 203950, and the load average falls to 0.59. That is a difference of about 30 percent, or 42 percent more throughput without it."
  consequence: "Two services that must coexist on this one core are currently fighting, and the emulator loses a third of its speed to a button poller. The daemon polls the CST9217 every 60 ms over the bit-banged I2C bus, and each poll is a 15-byte transfer that frequently NAKs and makes i2c-gpio print at KERN_ERR, so the cost is not the 60 ms cadence but the bus work and the console traffic behind it. Anything measuring the emulator must stop the daemon first or it will benchmark the contention, which is exactly what happened when this was first noticed: the same binary read 204k before a reboot and 141k after, and the difference was the daemon, not the reboot. The fix to consider is to have the daemon stand down while a fullscreen app owns the panel, which is the same coordination the console loglevel already needs (EXT-003)."
  sources:
    - "on the watch: /etc/init.d/S50tbtns stop, then the same binary re-run, same boot"
    - "userspace/tbtns.c: the poll interval and the read-acknowledge sequence"
    - "TCH-003 for why the polls NAK"
  verification: "Measured on hardware 2026-11-10 as a same-boot A/B: three runs with the daemon running at 140491/141968/142949, then four runs with it stopped at 203123/202019/203950/204642, with the binary hash unchanged and the instruction count identical at 454305. The effect was then confirmed indirectly on two further boots, where the flash baseline was 204k only once the daemon had been stopped."

- record_id: EMU-011
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The emulator has 5.9-7.4x realtime headroom and holds 1.00x at every frame rate tested, so neither the panel nor the interpreter limits it"
  statement: "Measured with tamafb running the real V3 ROM after a reboot so the 512 KB PSRAM block was fresh, flat out and headless: 513765 instructions/s with the ROM in PSRAM (29.67 M cycles/s, 7.42x realtime) and 408300 with the ROM in the flash window (23.58 M cycles/s, 5.90x). Both arms retired exactly 692707 instructions over 40000000 emulated cycles, 57.74 cycles per instruction, with every run within 0.6 percent across two boots each. With the display running at 10 fps and paced by the speed_pct argument, the loop held 1.01x at 100 percent, 1.50x at 150 percent and 2.01x at 200 percent -- the tool's ceiling -- using about half its 100 ms frame budget at the top of that range. Frame rate is not a limit either: 1.00-1.01x held at 2, 4, 8 and 16 fps. A 20 s run at 10 fps drew 200 frames, repainted 40 and logged no stalls."
  consequence: "This retires the claim that four frames per second is where the panel stops keeping up. The emulator is paced to 1.00x by design and has between six and seven times the headroom needed to honour that, so for realtime play the display refresh rate and the ROM's location are not bottlenecks -- a high frame rate costs correctness nothing and only shortens how long a change takes to appear. That is worth knowing before optimising anything: the remaining slow path is not throughput but the specific guest stall recorded in EMU-002, where the ROM computes for 580 ms without touching display RAM. The second consequence is a measurement-hygiene point that has now bitten twice: these figures do not reproduce the 204033 and 273098 instructions/s recorded for corebench, and neither the newer numbers nor the older ones are wrong. corebench runs a synthetic workload and tamafb runs the real ROM, which spends more of its steps asleep, and a sleep step is cheap; comparability also breaks between the two because the PSRAM gain depends on what fraction of the workload is ROM reads, which is why the delta here is +25.6 percent against corebench's +33.8. Compare corebench against corebench and tamafb against tamafb, never across."
  sources:
    - "TamaHarness/portable/tamafb.c: the bench mode at argv[4], which runs 40000000 emulated cycles flat out, and the speed_pct pacing at argv[6]"
    - "on the watch: ./tamafb 10 4 0 1 (bench), ./tamafb 6 <fps> 0 0 (display sweep), ./tamafb 12 10 0 0 0 <speed_pct>"
    - "KRN-004 for why the 512 KB PSRAM block is a one-shot allocation per boot and why later runs fall back to flash"
    - "EMU-002 for the stall that is still open, and EMU-009 for the corebench PSRAM figure this deliberately does not overwrite"
  verification: "Measured on hardware 2026-11-10 with the touch daemon retired, so no polling competes with the run. Bench: PSRAM 511711 and 513765 on two separate boots, flash 409532 and 407079 on one boot, both arms retiring an identical 692707 instructions. Display sweep at 2/4/8/16 fps held 1.00-1.01x. Speed ramp at 100/150/200 percent held 1.01/1.50/2.01x with 120 frames drawn at each. A 20 s, 10 fps run logged zero stalls."


- record_id: EMU-013
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The guest LCD is really 49x49 dots; rendering it 1:1 instead of at a 5 px dot pitch removes the panel from the emulator's cost entirely"
  statement: "The harness renders the V3 dot-matrix artwork at a 5 px dot on a 6 px pitch, which yields a 292x300 LCD so the pet is readable at arm's length. The hardware's real information resolution is 49x49 -- the artwork's dot grid, recovered by dividing the 6 px pitch out of the generated map. Rendering at 1:1, one pixel per dot, changed the per-frame cost from render 2.5 ms and panel 5.7 ms to render 0.6 ms and panel 0.5 ms, took stalls in a 20 s idle run from 29 to 0, and took the panel's share of frame time from 32 percent to about 2 percent. Bands sent per dirty frame fell from 6 of 10 to 1-2 of 10, because a 49x49 LCD only occupies two bands of 40 rows."
  consequence: "The panel is no longer part of the emulator's cost problem and should not be optimised further. This also explains a design point that had been open: band masking only helps if the dirty area is small, so shrinking the rendered image and the masking reinforce each other rather than being separate wins. The trade is legibility -- 49x49 is small on a 320x400 glass -- and it is the same artwork at an integer multiple of the dot pitch if a larger size is wanted later: 2x is 98x98, 3x is 147x147, 4x is 196x196. One bug was found on the way, and it predates this change: tamafb.c derives its framebuffer geometry from the segment size (`FB_W = SEG_W + 32`, `FB_H = SEG_H + 48`) unless overridden, which is 324x348 for the 5 px map and 81x97 for the 1:1 map, neither of which is the watch's 320x400 fb0. The file's own comment says those defaults exist to be overridden for exactly this reason and the watch build never passed them, so every band was written with a row stride 320 pixels narrower than the driver's 640-byte stride. At the 5 px scale the error was 4 px horizontal and 52 px vertical and read as a slightly off-centre picture; at 49 px it was a full-width smear roughly 48 rows tall. The operator caught it on the glass. Fixed by passing -DFB_W=320 -DFB_H=400 through scripts/build-tamafb.sh."
  sources:
    - "TamaHarness/portable/gen_segmap.py: SEG_DOT=5, SEG_GAP=1 default, PITCH_UNITS = 25.1, and the fitting table that names 292x300 as the largest pixel-exact size"
    - "TamaHarness/portable/segmap.c: 5366 runs, of which 4860 are exactly 5 px wide -- the 5-on-6 pitch this divides out"
    - "patches/harness/segmap-1x/: the 1:1 map, SEG_W and SEG_H both 49"
    - "scripts/build-tamafb.sh: the build that passes the real framebuffer geometry, which reproduces the deployed binary byte for byte"
    - "on the watch: ./tamafb-1x, telemetry showing panel 8-37 ms/s and render 0.6 ms per frame"
  verification: "Verified on hardware 2026-11-10. The 1:1 build reports `fb: 320x400, 16 bpp, line 640`, drew correctly on the glass after the framebuffer-geometry fix, and in a 20 s run with no interaction logged zero stalls against 29 for the 5 px build. Telemetry over a 22 s run measured panel 8-37 ms per second, repaint 1-4 frames per second, 1-7 bands per repaint."

- record_id: EMU-014
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The status menu runs at 0.52x because it needs about 520k instructions/s against a 513k ceiling, not because of the display"
  statement: "With the panel reduced to about 2 percent of frame time, a clean 22 s run on an otherwise idle watch measured speed 1.00-1.01x during ordinary play and 0.52-0.79x while the operator used the status menu. The distinguishing number is not frame time but cycles per instruction: emu wall time per second went from 137-150 ms during play to 800-945 ms during the menus while panel time stayed at 8-37 ms, and the run averaged 25 emulated cycles per instruction over 2913709 instructions and 73466045 cycles."
  consequence: "This closes the search. The interpreter is saturated, not the display: at 4M cycles/s the status menu needs roughly 520k instructions/s and the measured headless ceiling is 513k, so it cannot keep up, while ordinary play needs only about 160k because the pet spends most steps asleep and a sleep step is cheap. It also explains why the slowdown feels disproportionate to what the guest is doing -- the guest is doing arithmetic, cycles per instruction collapse towards 1, and instruction demand rises fourfold against a fixed ceiling. The only lever left is the interpreter's cost per emulated instruction, which EMU-005/006 put at about 1181 native cycles against the 30-80 a tuned 6502 core needs, with 97 percent of it in the interpreter's own instruction stream rather than in memory access. It is a userspace change with no firmware risk. One measurement trap belongs with these numbers: ns_emu is wall time around splb32_run_cycles(), so anything else running on the watch -- including the operator's own diagnostic commands -- is billed to 'emu'. The same build logged 64 stalls while the watch was being driven from a shell and 2 stalls in a 22 s run left alone, so a stall count taken during instrumented work describes the instrument, not the emulator."
  sources:
    - "on the watch: ./tamafb-1x 22 20 0 0 1, telemetry lines `tel  8.4s speed 1.01x emu 137ms panel 18ms` through the transition to `tel 20.8s speed 0.52x emu 945ms panel 16ms`"
    - "EMU-011 for the 513765 instructions/s ceiling measured headless on the same ROM"
    - "EMU-005 and EMU-006 for the per-instruction cost and where it lives"
    - "TamaHarness/portable/tamafb.c: ns_emu is wall time around splb32_run_cycles()"
  verification: "Measured on hardware 2026-11-10 on a freshly booted watch with no other work running: 2 stalls in 22 s, speed 1.01x during play rising to 0.52x sustained in the status menu, panel unchanged at 8-37 ms/s throughout. The operator independently confirmed the pattern -- slowdowns whenever interacting with game menus, not when the screen is static."


- record_id: EMU-015
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-10
  title: "An emulated instruction executes 116 host memory operations, and that is where the 464 cycles go"
  statement: "Disassembling splb32_run_cycles, which is step_core inlined into the run loop, gives 258 host instructions per emulated instruction containing 86 loads and 30 stores -- 116 host memory operations. On the tamafb build with the ROM in PSRAM the emulator sustains 516887 instructions/s at 240 MHz, which is 464 native cycles per emulated instruction. 116 operations at roughly 4 cycles each accounts for that 464 almost exactly."
  consequence: "This corrects the model EMU-006 established, and the error was in what got counted rather than in the arithmetic. EMU-006's 'memory is only 2.6 percent of the cost' is true of GUEST bus accesses -- 2.408 per instruction at 53 ns is about 128 ns of 4922 -- but it does not count host accesses to the cpu struct, and those are 116 per instruction. So memory traffic is the dominant cost after all, at a transaction cost of about 4 cycles rather than the 12.4 of a dependent load or the 43 an earlier record claimed. The distinction matters for what to do next: the bus pipelines back-to-back transactions well, so dependent latency is not the limit and access COUNT is. Two cheap experiments that appeared to argue against memory are explained by this and should not be read as evidence against it. Removing a per-instruction PSRAM read-modify-write saved 3.8 cycles, 1.9 per access, because 2 accesses of 116 is under 2 percent; the benchmark's own run-to-run spread is about 0.6 percent, so that measurement was noise-adjacent and the conclusion drawn from it at the time was wrong. Compiling at -O3 instead of -O2 changed nothing because a better register allocator cannot remove accesses the source performs on a struct it cannot keep in registers across a 970-way switch. The lever is therefore a register cache that keeps a, x, y, sp, pc and the flags in host locals across a run, which should remove the large majority of the 116."
  sources:
    - "patches/harness/splb32/splb32.c: step_core and splb32_run_cycles"
    - "objdump of splb32.o: 258 instructions, 86 loads, 30 stores inside splb32_run_cycles"
    - "userspace/memlat.c on the watch: dependent PSRAM load 51.80 ns, sequential stream 32.88 ns/byte"
    - "EMU-006 for the guest-access share this extends, and EMU-011 for the cross-benchmark trap described below"
  verification: "Measured on hardware 2026-11-10: 516887 instructions/s from tamafb's bench against a 4 MHz guest, and the instruction mix counted by disassembly of the same source. A measurement-hygiene error is recorded with this: 1181 native cycles per instruction is the corebench/flash-ROM figure (EMU-006) and 464 is the tamafb/PSRAM figure, and they are not comparable because corebench runs a synthetic loop while tamafb runs the real ROM which sleeps most steps. Both figures are right for their own workload. EMU-011 already states this rule and this cycle violated it while quoting it, which is why the rule is repeated here."


- record_id: EMU-016
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-10
  title: "The interpreter is already at normal efficiency: micro-optimisation has about 1.4x left, and only core 0 has the 4-7x"
  statement: "Three suspects were priced by measurement and all three are small: removing the per-instruction coverage counter gave +0.8 percent, compiling at -O3 instead of -O2 gave -0.5 percent, and skipping the timers_clock call entirely -- which breaks the guest and is a ceiling, not a shipping change -- gave +7.7 percent (517948 to 557764 instructions/s). What remains is step_core and exec_op_sem. At 463 native cycles per emulated instruction and EMU-007's measured 30.5 ns (7.3 cycles) per host operation in this code, the interpreter executes roughly 63 host operations per emulated instruction, which is the normal range for a switch-based 6502 interpreter."
  consequence: "This changes the recommendation, and it is the third correction to the optimisation plan in one session. EMU-005's '3-8x available' was derived from 1181 cycles per emulated instruction, which is the corebench figure and not this workload; at 464 cycles and ~63 host operations the design is near the practical floor for its own shape. A register cache that removes most of the host memory traffic could plausibly reach ~45 host operations, which is about 1.4x -- helpful but short of the 2x EMU-014 says the status menu needs, so the menus would improve to something like 0.7x rather than reaching 1.00x. The reason micro-optimisation is capped this way is architectural and already recorded: every host instruction costs about 7.3 cycles because core 1 has no I-cache and executes from PSRAM, and no amount of work on the interpreter changes what a host instruction costs. EMU-004 states the same conclusion from the other direction -- the fix is to run the emulation on core 0, which has cache and reachable internal SRAM, and leave core 1 to blit. That converts 7.3 cycles per host operation into roughly 1-2, which is the 4-7x, and it is a different kind of change from anything attempted here. It stays deferred by user decision, with its three recorded constraints: core 0 owns the WiFi link the console depends on and starving it trips the FreeRTOS watchdog (BLE-004), core 0's flash path must skip the inter-core park meant for Linux and must not touch PSRAM while the shared cache is off (JFS-001), and the firmware tree must never be pushed."
  sources:
    - "on the watch: tamafb bench at 517948 instr/s with timers, 557764 with the timers_clock call compiled out via a throwaway -DSPLB_NOTIMERS build"
    - "EMU-015 for the 464-cycle figure and the 116 host memory operations, EMU-007 for 30.5 ns per host operation over 1 KB of code"
    - "EMU-014 for the ~520k instructions/s the status menu needs, EMU-005 for the 1181-cycle basis this corrects"
    - "EMU-004 for the core-0-design consequence and its three constraints"
  verification: "Measured on hardware 2026-11-10 across four boots, one binary per boot because the 512 KB ROM allocation is a one-shot per boot: baseline 516887 and 517948, coverage-guarded 521199, -O3 514700, timers compiled out 557764. The throwaway build was removed from the watch and the tree afterwards. The 63-host-operations figure is derived from the measured 464 cycles and EMU-007's measured per-operation cost, not counted directly -- a host-instruction counter would settle it, and is not needed to support the conclusion, which is that the remaining suspects are small and the per-instruction cost is architectural."


- record_id: EMU-017
  kind: EMU
  topic_id: EMU
  status: OPEN
  verified_date: 2026-11-10
  title: "A firmware task writing PSRAM put the board in a boot loop, and the leading cause is that both cores' PSRAM use overlaps"
  statement: "A probe was added to the firmware: a 256-byte buffer from `heap_caps_aligned_alloc(64, ..., MALLOC_CAP_SPIRAM)` written with an incrementing counter every 100 ms and flushed with `esp_cache_msync(..., ESP_CACHE_MSYNC_FLAG_INVALIDATE)`, intended to test whether core 1 can read what core 0 writes. Flashing it produced a boot loop: the console showed a normal Linux boot reaching `Chipset=ESP32-S3 ID=09 detected over shmem IPC` at 4.198 s and then `rst:0xc (RTC_SW_CPU_RST)`, repeatedly. Reverting the firmware change restored normal boot immediately, so the probe was the cause and the kernel was not involved."
  consequence: "The experiment did not answer the cache-coherency question because it did not survive long enough to be read, and the failure is itself the important result. The leading hypothesis, not yet confirmed, is address-space collision: `init_linux_mmap()` identity-maps the whole 8 MB of PSRAM into Linux's range (`CONFIG_LINUX_PSRAM_8M=y`, MMU entries 0x180..0x1ff for `0x3d800000..0x3e000000`), and the firmware's own PSRAM heap is allocated from the same silicon. If the two overlap, a firmware task writing its heap corrupts memory Linux is actively using, which produces exactly this signature: a working boot until the writes begin, then a software reset. If that is right it reframes the whole core-0 design -- a shared display buffer cannot be an ordinary firmware heap allocation, it has to be a region carved out of Linux's memory map by a `reserved-memory` node and handed to the firmware explicitly, and the firmware must not use PSRAM heap at all once Linux is up. The measurement that would settle it is to have the firmware print its PSRAM heap base and compare it against Linux's 0x3d800000 window before writing anything. Two things this cycle did establish: the firmware toolchain builds and flashes from this machine (the ESP gcc lives at build/idf-tools/tools/xtensa-esp32s3-elf/esp-12.2.0_20230208, not in ~/.espressif, and an incremental `ninja` in the existing build dir works), and `esp_cache_msync()` exists in this IDF with writeback and invalidate flags. The kernel-side half of the experiment was also built and reverted: `CONFIG_DEVMEM` was temporarily enabled to let Linux read arbitrary physical memory, and the recorded reason for it being off still stands, so it was turned back off rather than left as a standing security regression."
  sources:
    - "patches/harness/core0/shmprobe.c: the probe as written, kept for the retry"
    - "build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/main/linux_ll_esp32s3.c: init_linux_mmap() and CONFIG_LINUX_PSRAM_8M's identity mapping"
    - "console capture during the loop: a clean boot to 4.198 s then rst:0xc (RTC_SW_CPU_RST)"
    - "patches/kernel/config/devkit_c1_16m_linux.config: the recorded reason CONFIG_DEVMEM is off"
  verification: "Observed on hardware 2026-11-10: the boot loop reproduced and then stopped exactly when the firmware change was reverted, which isolates it to the probe. The overlap hypothesis is NOT verified -- it is the leading explanation and the next measurement, listed above, would confirm or refute it. After the revert the watch boots normally with PMU 100 percent, rtc0 working, touch on IRQ 33 and drv2605 bound, and /dev/mem is absent again."

- record_id: EMU-012
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-10
  title: "Retiring the touch daemon gave the emulator the 30 percent EMU-010 predicted, and the bars still work because they moved into the kernel"
  statement: "The three touch bars below fb0 are decoded in `cst92xx`, which registers a second input device named exactly `touch-buttons` carrying BTN_0, BTN_1 and BTN_2, and fires `drv2605_tap()` on a press that starts on a bar. The band is y >= 440 split at x = 22, 158 and 294, mirroring `co5300_paint_frame()`'s painted geometry. With that in place the retired userspace daemon is no longer needed for input at all, and the operator reports the emulator running materially faster with the bars working."
  consequence: "This closes the trade recorded in docs/tamagotchi.md as 'you can have buttons or 34.7 percent, not both'. That trade existed only because the bars were decoded by a userspace poller competing with the emulator for the CPU; on the touch interrupt they cost nothing and the 34.7 percent stays. It also completes a pattern that ran through this whole session: retiring the daemon stranded three separate things it owned -- the bar-to-button mapping, the `touch-buttons` input device, and the haptic call -- and each was found only when something visibly stopped working. All three now live in the kernel, which is where a button's behaviour belongs. Two details of the mapping are measured rather than assumed: the Y threshold is 440 and not 445 because taps decode a few pixels high near the bottom of the glass (TCH-004), and a press on a bar deliberately does not report ABS_X/ABS_Y, because the strip is outside fb0 by construction and a coordinate there would only invite a consumer to clamp it in. Sliding along the strip changes button without firing a second click, so a drag across three bars is one press, not three."
  sources:
    - "patches/kernel/touch/final/cst92xx.c: cst92xx_button_at(), the touch-buttons input device, and the routing in cst92xx_report_touch()"
    - "patches/kernel/display/final/co5300.c: BTN_Y0, BTN_EDGE0..2 and BTN_SEP, the painted geometry this mirrors"
    - "TamaHarness/portable/tamafb.c: BTN_DEV_NAME \"touch-buttons\" and the BTN_0..BTN_2 read, the contract the device name satisfies"
    - "TCH-004 for the 440 threshold, and EMU-010 for the 30 percent the daemon cost"
  verification: "Verified on hardware 2026-11-10. /proc/bus/input/devices lists a fourth device, Name=\"touch-buttons\", as event3 alongside cst92xx-touch on event2. The emulator's own log confirms `buttons: /dev/input from touch-buttons` rather than its 'buttons will do nothing' error, and the operator confirmed the pet responds to all three bars and that the emulator is materially faster than with the daemon running."
```

---

## 6. Record template

```yaml
- record_id: <TOPIC>-<NNN>
  kind: SOC | BOARD | DISPLAY | PMU | RTC | KERNEL | FLASH | JFFS2 | BLE | NETWORK | TOOLCHAIN | EXTERNAL | EMU
  topic_id: <TOPIC>
  title: "Short noun phrase"
  status: VERIFIED | SOURCED | INFERRED
  verified_date: YYYY-MM-DD
  statement: "What the source says, with exact values and units"
  consequence: "What it means for this project, or where it is implemented"
  sources:
    - "Document or repository, revision/commit/path/symbol, URL"
  verification: "How and when it was confirmed, or why it is not yet confirmed; then any 'Revised <date> (core-log entry N): ...' notes"
```

---

## 7. Maintenance boundary

- Keep external facts, their sources, and their verification status here.
- Keep build results, failures, rationale, and chronology in `core-log.md`; cite a log entry number in `verification` instead of repeating it.
- Do not add speculative records for topics that have not been looked up. An unanswered question stays out of this file until a source answers it.
- Keep this file current rather than historical: when a record's source is replaced by a newer document or tool version, or its fact changes, revise the record in place and let git and `core-log.md` carry the history (section 1).
- **The panel is write-only and never acknowledges.** Records about what appears on the glass are admissible only when their verification says the observation came from looking at the screen. A return code of 0 from a display operation is not evidence that anything was drawn, and three separate display defects in this project reported success while drawing nothing.
- Records about the SoC/firmware boundary should say which core they concern: core 0 runs ESP-IDF and its radios, core 1 runs Linux, and anything requiring the two to cooperate must account for that split.
- A `core-syntax.md` audit is required whenever this file changes, per `.ai/core.md`.

```yaml
last_reviewed: 2026-11-12
```

---

## 8. Retired record ids

| record_id | Folded into | Why |
|---|---|---|
| — | — | No records retired yet. |
