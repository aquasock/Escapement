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
  - USB         # the ESP32-S3 USB OTG port as a host, and devices attached to it
  - NETWORK     # esp-hosted WiFi over shmem
  - TOOLCHAIN   # crosstool-NG Xtensa/uClibc, ESP-IDF, esptool
  - EXTERNAL    # interface contracts owned by other projects
  - EMU         # the SPLB32 emulator: interpreter cost, PSRAM latency, core choice
  - IMU         # BHI260AP: host-loaded firmware, KLIO frames, injection, meta events
  - AUD         # audio playback, decoding and volume

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

- topic_id: SSH
  name: "wolfSSH server and SFTP"
  description: "The shell's SSH/SFTP server on port 22: the request sizes it will and will not answer, the single client slot, and the file-descriptor budget it shares with the SD card."

- topic_id: GPS
  name: "MIA-M10Q GNSS receiver"
  description: "The u-blox receiver on UART1: which pins, which rail, the NMEA sentence set it emits, and what its output proves about the antenna when there is no fix."

- topic_id: NFC
  name: "ST25R3916 NFC reader"
  description: "The reader on the shared SPI bus: the part fitted, which chip select and interrupt it uses, the PMU rail that powers it, the SPI framing it answers, and the vendor stack this project drives it with."

- topic_id: DSP
  name: "Framebuffer ownership and blitting"
  description: "Who may write the panel's framebuffer and how a bitmap reaches it: the painter's cell-diff model, exclusive ownership for an app that draws pixels, where frame-sized buffers can live, and what happens to the USB mirror when a picture owns the screen."

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

- topic_id: USB
  name: "ESP32-S3 USB host"
  description: "The OTG port acting as a host under ESP-IDF 5.3.1: the FIFO split that bounds endpoint sizes, the event loops the host library requires, and the devices measured on it."

- topic_id: MOD
  name: "Loadable modules in flash"
  description: "Running code from the `modules` flash partition on the ESP32-S3: mapping raw flash as instructions, calling across a partition boundary, and the API-table contract a blob uses to reach the firmware when it cannot link against it."

- topic_id: AUD
  name: "Audio playback, decoding and volume"
  description: "How a sample reaches the speaker or the DAC's analog output and what happens to it on the way: compressed-audio decoding on this CPU and the memory cost that decides which decoder fits, volume as a taper law rather than a percentage, the perceptual and hardware conventions behind those laws, and the resolution cost of attenuating in the samples."

- topic_id: IMU
  name: "BHI260AP IMU, KLIO and host data injection"
  description: "The Bosch BHI260AP on the shared I2C bus: the host-loaded firmware image and its container, the KLIO pattern-learning algorithm's frame layout and learning semantics, the data-injection command's payload rules and ids, the meta-event vocabulary, and the FIFO behaviour when injected blocks and physical sensors share one queue."
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
| Why is a panel or motor powered after a reboot but dark from a power cycle? | BRD | BRD-006 |
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
| Can a panel writer bound its wait for the SPI bus? | LCD | LCD-014 |
| Why does a bounded acquire fail with every push refused? | LCD | LCD-014 |
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
| How wide can content be in the rows above the framebuffer? | LCD | LCD-007 |
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
| How many satellites are in view, and why do two commands disagree? | GPS | GPS-006 |
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
| Where can codec or decoder blobs live so they cost no RAM? | FLS | FLS-004 |
| How does a module in the modules partition call the firmware? | MOD | MOD-001 |
| Why does a module still run but decode garbage after an app rebuild? | MOD | MOD-002 |
| How much does a littlefs partition actually hold? | FLS | FLS-004 |
| Why does NimBLE refuse to pair? | BLE | BLE-001 |
| Why does a report-map read kill the link? | BLE | BLE-002 |
| Boot protocol or report protocol? | BLE | BLE-003 |
| Why does the board reset-loop when Bluetooth scans? | BLE | BLE-004 |
| How can a PC reach the watch with no USB or WiFi? | BLE | BLE-008 |
| Why did the BLE console stream ~24 KB/s and show only the top 10 rows? | TOL | TOL-004 |
| Why does the host refuse an isochronous endpoint over 128 bytes? | USB | USB-001 |
| Why does an attached USB device never enumerate, or a transfer never complete? | USB | USB-002 |
| What does the USB-C DAC offer, and which alternate plays 16-bit stereo? | USB | USB-003 |
| Why does USB audio sound scratchy with every packet on time? | USB | USB-004 |
| Why does starting the USB stream from the file player panic? | USB | USB-005 |
| Why is the DAC silent when the watch says it is streaming? | USB | USB-006 |
| What should a volume control's taper law be? | AUD | AUD-001 |
| Which FLAC decoder fits on this board, and why? | AUD | AUD-002 |
| Why does I2S fail to allocate its DMA buffer, and why does the picture freeze when it does? | AUD | AUD-003 |
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
| Does a bigger data cache make the emulator faster? | EMU | EMU-022 |
| Does slot B actually make the Game Boy faster, and should the blitter move to the idle core? | EMU | EMU-023 |
| Why does deleting sdkconfig change the firmware? | TOL | TOL-008 |
| Why does a port's strong hook never run, while the firmware still builds and boots? | TOL | TOL-009 |
| Where can a large read-only blob live without RAM? | EXT | EXT-002 |
| Where does the IMU's firmware live, and in what container? | IMU | IMU-001 |
| What do the bytes in a KLIO output frame mean? | IMU | IMU-002 |
| What does injected data have to look like, and which ids does it take? | IMU | IMU-003 |
| How do I know KLIO actually learnt a pattern? | IMU | IMU-004 |
| What are the part's meta event codes, and can they be trusted? | IMU | IMU-005 |
| Why does the FIFO overflow while injecting? | IMU | IMU-006 |
| Why does KLIO stop emitting frames partway through a learning run? | IMU | IMU-007 |
| What unit is the IMU timestamp in, and how is it encoded? | IMU | IMU-008 |
| How is the injected gesture waveform built, and what was wrong with it? | IMU | IMU-009 |
| Can a gesture recorded from a hand be replayed through injection, and does it learn? | IMU | IMU-010 |
| Why does `read_pattern` fail against a pattern that is loaded and matching? Does a pattern survive a reboot? | IMU | IMU-011 |
| What configuration lets KLIO learn to 100 percent, and what breaks it? | IMU | IMU-012 |
| Which ttyACM device is the watch, and which is the TinyTang? | TOL | TOL-005 |
| Why is the console dead after an esptool read with --after no_reset? | TOL | TOL-006 |
| How do I open a Terminal on the watch from a host, reliably? | TOL | TOL-007 |
| Why does a fullscreen app flicker against the console? | EXT | EXT-003 |
| Why does pressing an on-glass app button crash the watch? | DISPLAY | DSP-009 |
| Why is there a solid white block in the framebuffer's top-left corner? | EXT | EXT-004 |
| What does a gnuboy savestate contain, and what does its loader not check? | EXT | EXT-005 |
| Why does the screen tear for one frame when a whole screen updates? | LCD | LCD-015 |

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
LCD-007: "The glass is 410x502 with 45-degree chamfers; the largest unclipped rectangle is 320x412 and fb0 is 320x400 at (45,45), leaving a 57-row strip below it that the touch buttons are painted into. The chamfers are three measured lines -- x+y=80 and (410-x)+y=90 at the top, x+(502-y)=82 at the bottom-left -- so the usable width in the rows ABOVE fb0 shrinks with the row: 261 px at row 10 against the 331 available by row 45, which is what bounds the status strip"
LCD-008: "45x45 windows are honoured, so the edge API needs no full-width fallback; and in RGB565 green is a 6-bit field, so RGB565(31,31,31) is pink (0xFBFF) and white needs 63 (0xFFFF)"
LCD-009: "A long single-transfer window to the right strip (45x356) tears into diagonal splits; the same rect in 2-row bands is clean. Not width, not X, not height -- EXPLAINED 2026-11-12: it is the ROW COUNT, the same defect as the TinyDesk port's leftover strips (DSP-004)"
LCD-010: "Transfers cost 11.42 us per 64-byte chunk with per-transfer fixed cost ~0, at 5.6 MB/s (56% of quad wire); the panel itself is only 1.6% of that, so the remaining 5 us/chunk is CPU FIFO work and DMA caps at about 2x"
LCD-011: "40 MHz is clean on the glass and is the driver's compiled-in default, but doubling the clock buys only 13 percent (115.15 ms to 100.85 ms per frame), which is the evidence that the transfer is not clock bound"

LCD-012: "SPI DMA is not usable here today: the GDMA out-link descriptor address field is 20 bits, so a descriptor must sit in internal DRAM below 1 MB, and Linux owns none of it -- the obstacle is allocation, not addressability (SOC-004)"
LCD-013: "A full 320x400 frame costs 93-95 ms at 40 MHz and 108.8 ms at 20 MHz through fbbench, so halving the clock costs only 15 percent and the transfer is neither wire bound nor MMIO bound: roughly 20 us of each 64-byte transaction is fixed engine turnaround"
LCD-014: "spi_device_acquire_bus() in ESP-IDF 5.3.1 requires portMAX_DELAY: a finite wait tripped SPI_CHECK and returned ESP_ERR_INVALID_ARG without touching the lock, so a pusher cannot bound its wait -- it must take the infinite acquire and check the result, which is what makes contention a skipped band and a hang something else entirely"
LCD-015: "The CO5300 TE line (GPIO 6) was ALREADY ENABLED by the driver -- display.c sends TEON 35h M=0 -- but never configured or read, so a whole-frame update went out as 18 unaligned 16-row bands and tore once per full-screen change. FIXED: rising-edge interrupt on GPIO 6 and ONE gate at the top of display_picture_rows() (never per band), bounded at 20 ms and failing open, taken from LilyGo's own driver. Confirmed on the glass"
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
TCH-011: "The margin buttons must be gated on display_pixels_claimed(), not on td_wm_app_owns_screen() -- the latter is false for an app that claims pixels without being a window manager window, so the Tamagotchi received the margins' desktop keys and pressing Enter mid-game typed into the hidden shell"
TCH-012: "The CST9217 block carries TWO points six bytes apart -- second at byte 7 (event 0x16), bytes 8-10 coordinates in the same encoding, byte 12 validity -- numbered by ARRIVAL order; idle is fifteen 0xFF bytes, not 0x00 or 0xAB; and lifting the first finger of a chord PROMOTES the survivor into the primary slot with event 0x16, which is why the press test must be an exact b[0] == 0x06 and not a nibble mask"
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

FLS-004: "The T-Watch Ultra firmware adds a 2 MB raw `modules` partition at 0xE00000 (subtype 0x40) for codec blobs meant to be mmap-ed as instructions, funded by shrinking `storage` from 9.88 MB to 7.87 MB; a littlefs partition's live contents must be read by MOUNTING it -- 4.2 MB of non-0xFF blocks held only 22 KB of files"
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
TOL-004: "ESP-IDF's xRingbufferSend() returns BaseType_t pdTRUE/pdFALSE, NOT a byte count, and on a RINGBUF_TYPE_BYTEBUF it is all-or-nothing; returning it as a count made every BLE console write report 1 byte, so the renderer resent shifted copies until the ring filled and then repainted the whole screen for ever"

EXT-001: "The SPLB32 Tamagotchi core runs at 4.05x realtime headless on this board, and 1.00x while painting 320x400 at 4 fps from the flash-resident ROM; the display path is the bottleneck"
EXT-002: "The linear flash window 0x42000000 is readable from userspace, so a 512 KB ROM at 0x42480000 costs zero RAM -- the fix for contiguity on NOMMU; file mmap costs 2x the file and MAP_SHARED fails ENODEV"
EXT-003: "A fullscreen app on fb0 flickers against fbcon because every printk repaints the console over it; console_loglevel decides it, and /etc/sysctl.conf sets kernel.printk = 1 4 1 7"
EXT-004: "TinyDesk's render layer SHOWS the terminal cursor and homes it when it leaves a full-screen screen (`ESC[?25h` ... `ESC[H`), so a cell-based panel driver ends up with `cursor_visible` true at cell (0,0); painting that cursor means INVERTING the cell's colours, which on a dark taskbar cell is a solid white one-character block in the top-left corner of the framebuffer"
EXT-005: "gnuboy's savestate is `SAVE_VERSION 0x107` written as a named-field header then five block runs -- 4 KiB of `hw.ioregs`/pal/OAM/wave, then WRAM, VRAM and cartridge RAM at 4096-byte granularity -- and `do_save_load()` validates the VERSION but never the CARTRIDGE: a `.st` from a different ROM loads silently, and its `fwrite`/`fread` are checked only for fewer than one 4096-byte block, so a truncated file or a full card is not reported"

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
EMU-017: "OPEN and superseded: a firmware task writing a PSRAM buffer from the heap put the board in a boot loop, and the overlap hypothesis this record proposes was NOT confirmed -- coherency was later proven working and the emulator moved to core 0 (EMU-004, docs/cores.md)"

NFC-001: "The NFC reader is an ST25R3916, not a PN532 and not on I2C: SPI on the shared bus (SCK 35, MOSI 34, MISO 33) with CS 4 and INT 5, powered by the AXP2101's DLDO1 (voltage 0x99, enable bit 7 of 0x90); on this unit register 0x3F reads 0x2A, which is base silicon rev 2 and not the 3916B"
NFC-002: "The ST25R3916 answers SPI mode 1 at up to 5 MHz with a one-byte command: write reg = (reg and 0x3F) then data, read reg = that or 0x40 then a dummy byte then data, FIFO load 0x80, FIFO read 0xBF, direct command 0xC0 or cmd; chip select must stay a plain GPIO because the vendor stack drives it by hand around each command"
NFC-003: "The vendor stack is ST RFAL plus the ST25R3916 driver plus NDEF, ~40k lines, and its RfalRfClass declares 50 virtuals with none defined, so it fails to link until they are marked pure; Arduino coupling is only types, GPIO, MIN/MAX and a Serial object, and attachInterrupt can stay a no-op because the stack falls back to reading the pin"
NFC-004: "This board has no capacitive card-presence circuit, so the reader must be powered and polling to see a tag; a Type 2 tag reads as `card A uid <hex>`, `sak 00 T2T`, and its NDEF message, and the LilyGo tag in hand carries the text record 'Hello LilyGo' rather than being blank"

GPS-001: "The GNSS receiver is a u-blox MIA-M10Q on UART1 at 38400 8N1, TX 43 / RX 44 / PPS 13 from the SoC's point of view, powered by the AXP2101's BLDO1 (voltage 0x96, enable bit 4 of 0x90) -- one register below the speaker's BLDO2 in both, so the wrong write lights the amplifier and leaves the receiver dark"
GPS-002: "The receiver emits GGA, RMC, GSA, GSV, GLL and VTG at 1 Hz with 0 checksum failures, and acquiring a UTC time from it proves ephemeris was decoded off the satellites -- a stronger statement about the antenna than a fix, and the evidence that separates 'no antenna' from 'no fix'"
GPS-003: "The receiver answers NMEA on this unit but NOT the UBX-MON-VER poll, so it never confirms its own model string; NMEA is the working interface and the UBX side (and constellation configuration) is untouched factory default"
GPS-004: "Indoors the receiver tracks 3 to 5 satellites with a real HDOP around 2.38 and no fix, because a 3D solution needs four; a fix needs sky view and is the one remaining question the bench cannot answer"
GPS-005: "A GPS time needs no fix: the receiver reports valid UTC at 3 satellites, so `gps time` sets the clock indoors; the value goes through settimeofday AND board_time_set_post, so the PCF85063A holds it and it survives a hard reset, and a GPS time applied from a stale capture is a wrong clock however right the source"
GPS-006: "GSV carries each constellation's own count of satellites IN VIEW, so a sky figure is the SUM across talkers (18-24 with four constellations) against GGA's count USED in the fix (0 without a fix); `gps raw` reports the MAX single constellation, so the shell and the app differ by design"
SSH-001: "The SFTP server answers a 16 KiB write but never answers 24 KiB or 32 KiB (paramiko's put uses 32 KiB and hangs), and answers a 4 KiB read but not 16 KiB, so uploads chunk at 16 KiB and downloads at 4 KiB: measured 28.9 KiB/s up and 74.0 KiB/s down"
SSH-002: "An abandoned SFTP session leaks its file descriptors unless WOLFSSH_STOREHANDLE is defined, because SFTP_FreeHandles() is compiled out and settings.h only sets that macro for WOLFSSH_FATFS, which is the other I/O path; with the macro on, 16 killed transfers leave the SD card writable where 5 used to exhaust its 5-slot VFS pool"
SSH-003: "The SSH server takes one client at a time on the accept loop's own task, so a stalled session holds the only slot and later attempts complete the TCP handshake then get no protocol banner; only `ssh restart` or a reboot clears it"
DSP-007: "A push is only complete when CS is asserted for the pixel stream, and reg_write_bus() leaves CS high on return -- so moving window programming inside the bus acquisition, which is what made two concurrent pushers safe, silently dropped every game frame while the SPI reported success, the timing accounted for every microsecond, and `nes status` showed a healthy frame rate; the glass was black and only looking at it found out"
DSP-008: "DSP-005's single-writer rule can be lifted for an app blit, but only with all three parts of it: a per-task transmit band instead of a shared s_tx, window programming inside the bus acquisition so each band is atomic, and a frame-buffer pool handed between renderer and blitter through queues -- with the blit callback taking a free buffer BEFORE publishing the finished one, or a dry pool pushes half-written frames"
DSP-001: "The panel's framebuffer has two writers -- the cell painter and any bitmap blit -- and they must be given exclusive ownership, because the painter decides what to redraw by comparing cells against its own record and a blit does not update that record; uncoordinated, the painter repaints the old cells over the picture at the next cursor blink or clock tick"
DSP-002: "A static RGB565 full-panel buffer does not fit: 320x416 costs 256 KB and the link fails with `region dram0_0_seg overflowed by 88896 bytes`, because everything static in the port's component is internal DRAM by construction and only libtinydesk, libtdsh and libmicropython are mapped to PSRAM; frame buffers must be heap allocations in PSRAM"
DSP-003: "The USB mirror stops describing the panel once a bitmap owns the screen, because the desktop's diff stream no longer matches the glass, so a diagnostic that exists only in that terminal is unreadable exactly when a picture is up; counters must also go to a file"
DSP-009: "`display_margin_label()` paints synchronously on the caller's task and takes the panel's SPI bus, so while an app owns the pixels only its BLIT task may call it -- from the touch callback it asserted at `spi_master.c:1310` (`spi_device_release_bus`) because the blit task was mid polling-transaction and `spi_device_acquire_bus()` had returned ESP_ERR_INVALID_STATE, which the port does not check; the callback records state and the blit task drains it, in BOTH branches because an idle cartridge produces no frames"
DSP-004: "The leftover strips are a transfer-height fault: `flush_rect` sized a transaction from `5120 / w`, so an 8-pixel column went out as one 640-row window and dropped bands while full-width 16-row flushes were always clean; capping the rows at 16 whatever the width fixes it, and this is LCD-009's unestablished cause"
DSP-005: "Only paint_task may touch flush_rect, s_tx or the SPI bus; calling it from the shell task produced a scrambled taskbar and a hung shell, because a second writer raced the painter over a buffer and bus that had never needed a lock"
DSP-006: "The button bars and key columns live OUTSIDE the framebuffer in controller coordinates and are painted once at init by paint_buttons()/paint_column(), not by the cell painter, so a full-screen app can clear s_fb without erasing the watch's controls -- but a framebuffer clear alone is not enough to take the screen, because a blit leaves the previous console text in the rest of s_fb and ownership then stops anything from overwriting it"
EMU-018: "Internal DRAM is scarce enough that 4,148 bytes of .bss starved the SSH server of its 65,536-byte floor (`insufficient internal RAM (62443 bytes free; need at least 65536)`), so the emulator's dirty-row tables are a refcounted PSRAM heap allocation and internal free went 62,443 to 66,599; a component that adds .bss can break a peripheral with nothing to do with it"
EMU-019: "tmc_button() writes the emulated CPU's pin state that tmc_run() reads, so it may only be called from the guest task: one instance is not reentrant, and the touch poll task runs on core 0 at the same priority as the unpinned guest task -- presses go through a queue drained between frames, not straight at the machine"
EMU-020: "splb32_audio_emits() counts square-wave TOGGLES, not sounds -- one held tone raises it thousands of times a second -- so a front end keyed on it plays continuously and falls silent only when the guest stops stepping; the envelope is the signal and the waveform is a clock, so splb32_audio_active() exists and callers fire on its rising edge"
EMU-021: "The emulated EEPROM is 1024 bytes (HT24LC08_SIZE) and the save image must match it exactly -- ht24lc08_save() memcpys HT24LC08_SIZE with no length argument, so TMC_EEPROM_BYTES at 512 wrote 512 bytes past the caller's buffer and the overflow surfaced three calls away as a tdsh `fclose` on a corrupted FILE*; _Static_assert ties the two together, and a 64 KB coredump partition carved from the unused ota_1 is how a panic gets a backtrace when the console is compiled out"
EMU-022: "A 64 KB data cache moved the Game Boy benchmark from 7915/7886 us to 7718/7850 us -- inside the 130 us run-to-run spread -- while costing 17 KB of internal heap and leaving no room for a shell to start, so the emulator is not data-cache bound"
OTA-001: "A/B firmware update works on this port and needs no repartitioning -- 2 x 3 MiB slots were already there with esp_ota_set_boot_partition and rollback wired; install to the inactive slot, restart, the trial confirms itself after 30 s, and installing again writes the other slot. A truncated image is refused at boot with `invalid segment length`, so a corrupt slot cannot brick the board. The built-in update feed is a 404, which is the real gap, not partition size"
NES-003: "The NES blit was 70 percent of the frame because it made three passes over external memory for one 5120-pixel internal band -- a 120 KB PSRAM scratch buffer, the 260 KB PSRAM framebuffer, and back -- and streaming the conversion straight into the transmit band took Zelda's title from 19 to 42 fps, which is close to this board's ceiling rather than halfway to 60"
PM-001: "Pokemon Mini's CPU requirement is 0.22-0.36 MIPS, not the 1 MIPS the original scope stated -- 1 MIPS is the 4 MHz / 4-cycle hardware CEILING, while a game needs instructions-per-frame x 72 Hz, and the twelve retail cartridges measured use 3,091 to 5,053 instructions of the 13,889-per-frame budget"
PM-002: "The display gate that was called Pokemon Mini's binding constraint was measured through 64-byte SPI chunks at 357 ns per pixel; the rebuilt transmit path moves 16,384-pixel transactions at 66.5 ns per pixel, so a 288x192 frame costs 3,675 us of a 13,889 us budget instead of 19,741 us, and the gate passes with 3.8x headroom"
PM-003: "The Pokemon Mini frame tick is 71.97 Hz (65 x 855 = 55,575 cycles at 4 MHz), and a cartridge draws at that divided by 2 to 12 with no divisor of 1, so 36 Hz is the fastest picture rate, 60 Hz does not exist, and running below 1.00x lowers a dithering game's dither rate proportionally"
GBC-001: "gnuboy is 1.54-1.79x faster than binjgb across four commercial GBC ROMs (Zelda LA DX, Pokemon Crystal, Mario Deluxe, Tetris DX -- CGB-only and CGB-compatible, three mappers) and emits GB_PIXEL_565_BE, the panel's own byte order, so a port needs no per-pixel conversion; but a GBC's CPU is 2.34x the NES's clock, so emulating it costs proportionally more, and at 2x scale the pixel count rises to 1.5x the NES's -- the picture gets much better and the frame rate probably gets slightly worse"
GBC-002: "Peanut-GB is the fastest portable Game Boy core by reputation and is DMG-only -- its CGB support is preliminary work in an issue, not in the tree -- so it is the obvious first choice for a Game Boy port and the wrong one for a Color port; and 160x144 divides 320 exactly, so GBC at 2x fills the watch's width with no side margins where the NES at 256x240 cannot reach 320 at any integer scale"
NES-001: "nofrendo is 1.96x InfoNES on the same ROM (394.9/392.6/828.2 vs 201.6/201.6/422.4 Mcyc/s) with render and palette conversion live, covers 98.2 percent of a 794-ROM collection, and needs NO retro-go -- nes/utils.h has an #ifdef RETRO_GO branch, so with it undefined logging is printf and CRC32 is (0); FCEUmm and QuickNES cannot be built at all without libretro.h and a frontend"
NES-002: "On the watch nofrendo runs at 21 fps, split 8,160us emulator and 6,166us blit against a 16,667us budget -- moving 61 KB in 6.1 ms is sixty times too slow because display_picture_rgb() copies into s_fb and flush_rect() byte-swaps into s_tx, making four passes over PSRAM where one would do; a full-frame path that converts straight into s_tx is the fix"
SOC-006: "tdsh's SSH server draws about 19,400 bytes of internal DRAM merely by running and another 29,100 while a session is connected, which is why its 65,536-byte admission floor was never the whole story -- the check happens before the server's own footprint exists"
SOC-005: "The ESP32-S3 Wi-Fi driver takes its RX and TX buffers from INTERNAL DMA-capable RAM, never PSRAM, at esp_wifi_init() whether or not a network is ever joined -- each is 1600 bytes and the IDF default pair (16 static RX + 16 static TX) holds 51,200 bytes for the life of the firmware, which on this board was 30,160 bytes more than the desktop needed"
BLE-007: "The operator's Xbox Wireless Controller is BLE HID, not Bluetooth Classic -- it connected to the ESP32-S3 on the first try -- and hid_mouse_parse() accepts its descriptor because a gamepad declares Generic Desktop X and Y for its left stick, which is why it moved the desktop cursor; the discriminator is the top-level collection usage (Game Pad 0x05, Joystick 0x04, Mouse 0x02, Keyboard 0x06) and the gamepad parser must be asked FIRST, or the mouse claims the device and the gamepad path is never reached"
BLE-008: "NimBLE can run central and peripheral at once, so the watch keeps its BLE keyboard AND offers a Nordic-UART serial console for a PC; GATT services must be registered BEFORE nimble_port_freertos_init (ble_svc_gap_init asserts SYSINIT_ASSERT_ACTIVE) and only advertising after the sync, and the name must be in the advertisement itself or a scan-by-name finds nothing"
USB-001: "ESP-IDF 5.3.1 splits the S3's one host FIFO by CONFIG_USB_HOST_HW_BUFFER_BIAS: at Balanced, periodic TX is OTG_DFIFO_DEPTH/8 = 32 words and the HCD refuses any isochronous OUT endpoint above 128 bytes (the Kconfig help says 192; the code and the board say 128); PERIODIC_OUT is required for USB audio and leaves IN endpoints at 128"
USB-002: "ESP-IDF 5.3.1's host needs TWO event loops: usb_host_lib_handle_events() or nothing enumerates, and usb_host_client_handle_events() or no transfer completes; control submits do not block, completion callbacks are called unconditionally (NULL is a jump to 0), and is_synchronous must be false"
USB-003: "The TTGK KM-HIFI-384KHZ DAC (3302:3366) enumerates on the S3 at full speed as UAC2: playback interface 1, ep 0x01 isochronous synchronous OUT, interval 1, alt 1 = 2 ch S16 mps 384, alt 2 = S24 576, alt 3 = S32 768; clock 9 via input terminal 1, RANGE 8000/16000/32000/44100/48000/96000 Hz; it played a 48 kHz S16 tone on both channels; a plug inserted or removed in its 3.5 mm jack makes it re-enumerate"
USB-004: "Isochronous OUT through IDF 5.3.1 at 16 packets per transfer puts a few-sample discontinuity at every transfer boundary (440 Hz tone sidebands at +/- n x 62.5 Hz, -36 dB) even with every packet full and on time; at 8 packets it is gone (sidebands -107 dB, THD+N -62 dB); mechanism unknown"
USB-005: "Claiming a USB interface allocates DMA descriptor lists and IDF 5.3.1 PANICS (hcd_dwc.c:1059 assert) when it cannot; the internal DMA pool is what runs out (open SD files and 4 KB task stacks both draw on it), so all USB audio DMA allocation happens once in `uac2 open` and the USB tasks now run on PSRAM stacks"
MOD-001: "A blob in the modules partition is mapped as INSTRUCTIONS (0x42000000..0x43ffffff) and called directly, but it must never be data-read through that pointer and must never map the partition twice; it reaches the firmware through ONE API table whose address the blob build derives from the app ELF, so a blob belongs to the build that produced it"
AUD-001: "A volume percentage is a loudness scale, not an amplitude, and the three conventions differ: -10 dB at half is the psychoacoustic half-loudness result, -18 dB at half is what a 15A audio-taper pot and VLC's cubic law both give, and linear-in-dB spends an even 3.6 dB per 10% over a 36 dB range; a power law is accurate at one point and inverts the feel"
AUD-002: "FLAC decoding fits because micro-flac (Apache-2.0) sizes one PSRAM working buffer from the file's own STREAMINFO and has Xtensa LPC assembly, costing 31.7 KB of flash; Rockbox's decoder cannot run here at all because it reserves ~190 KB of internal SRAM as static 7-channel buffers"
AUD-003: "An I2S descriptor buffer is capped at 4092 bytes by the driver and allocated one calloc each from the internal DMA pool, so four descriptors of 1024 stereo frames asks for 16,368 bytes at once; this board had 12,251 free with a 3,968 largest block, so the allocation failed and the channel never configured -- and because the display SPI bus draws on the same pool, a shortage shows up as silence with a picture or a frozen picture with sound"
USB-006: "VERIFIED: the USB-C DAC's playback FU MUTE refuses SET_CUR with a STALL, so its silent first stream cannot be unmuted from the host; a streaming alternate-setting cycle (dac_wake(), `uac2 wake`) re-arms it, and its hardware volume barely changes loudness on headphones"

IMU-001: "The BHI260AP runs firmware the HOST loads into its RAM: the image sits in `moddata` at 0xE10000 as a multi-segment container whose first segment loads at 0x124000, and its header carries a SHA-256 of the payload at offset 0x08; nothing about it is a fixed mask ROM program"
IMU-002: "A KLIO learning frame is 11 bytes; byte 1 is the pattern index (int8, -1 = none), byte 2 is progress 0..100 (100 learns a pattern), byte 3 is change_reason: 0 progressing, 1 interrupted by non-repetitive activity, 2 interrupted by no significant movement"
IMU-003: "Injected payloads must be a multiple of four bytes (0x7C ERR_INPUT_CHANNEL_NOT_WORD_ALIGNED) and are addressed by DEDICATED inject ids 55 (ACCEL_INJECT_ID) and 58 (GYRO_INJECT_ID), never by sensor ids; a malformed block returns 0xE2 ERR_SENSOR_DATA_INJECTION"
IMU-004: "KLIO learning succeeds only when bhy2_klio_read_pattern() returns BHY2_OK with a 157-byte pattern; rc -2 with the 244-byte buffer size filled in means no pattern exists, and the progress byte alone is not evidence of anything"
IMU-005: "bhy2_defs.h meta codes are 1,2,3,5..16,18,19,20 (11 = SENSOR_ERROR, 12 = FIFO_OVERFLOW, 20 = SPACER); with the physical sensors streaming, the meta callback also receives packets that are not meta events at all (observed lead byte 0xF5), so meta counts are only interpretable with those sensors off, where a run yields 12-15 packets all code 20"
IMU-006: "Accelerometer and gyroscope at 50 Hz alongside injected blocks at 100 Hz overflow the part's one FIFO: measured 42 FIFO_OVERFLOW and 30 SENSOR_ERROR events attributed to sensor 4, against 0 and 0 with both physical sensors off -- injection replaces their contribution, it does not supplement it"
IMU-007: "With a quiet FIFO and zero errors, KLIO climbs 20/40/60/80 in about 5 s, then returns to 0 with change_reason 1 and STOPS EMITTING FRAMES ENTIRELY (15 frames, then silence for 1400 more accepted blocks), and the peak is not reproducible between identical runs (80 then 20)"
IMU-008: "The part's timestamp tick is ~15.6 us (64130 ticks/second, MEASURED): a TS_LARGE_DELTA field is a DELTA that bhy2.c ADDS to a running 64-bit counter, not an absolute time, and the physical accelerometer at 50 Hz advances the parsed timestamp by exactly 1282-1283 ticks per 20 ms sample"
IMU-009: "The injected gesture rotates gravity in the y-z plane (ay=sin(theta), az=cos(theta)), so its angular velocity belongs on X: writing it into gy, as the waveform did from entry 73 to 76, makes every block assert a tilt about x and a spin about y at once -- a motion no wrist can produce"
IMU-010: "A hand gesture recorded at 100 Hz and replayed through injection five times gave peaks 20/60/40/40/20 with 3-27 frames and read_pattern rc -2 every time, while the same gesture performed live learns and yields a 157-byte pattern -- so injection delivers data the engine reads but the input content is not what separates the two paths"
IMU-011: "The LEARN slot that read_pattern reads and the recognition table write_pattern fills are separate: once a pattern is written and enabled the learn slot is consumed, so read_pattern returns -2 with driver status 3 (invalid pattern operation), and the recognition table is RAM the firmware upload wipes at every boot and any learning_reset"
IMU-012: "KLIO reaches progress 100 and reports a learnt pattern with accel and gyro at 100 Hz and the orientation sensors OFF for the duration; orientation streams at 50 Hz into the same single FIFO and that sharing is what produced the `meta 12 fifo overflow` of entries 73 and 74, so a learning pass must own the FIFO"
TOL-005: "The watch and the TinyTang both appear as ttyACM devices and swap numbers between reboots: the watch is 303a:1001 (Espressif USB JTAG/serial debug unit) and the TinyTang is ffff:5454 (TinyTang CDC), so identity must come from /dev/serial/by-id or udev, never from the ttyACM number"
TOL-006: "esptool's --after no_reset leaves the ESP32-S3 in its ROM bootloader, so the board's console emits nothing and every later command reads stale state until a --after hard_reset chip_id or a power cycle"
TOL-007: "The desktop Start menu remembers its last selection, so F10 then Enter is only deterministic after a reboot: the sequence that works is reboot, F10, Enter, and the window that appears must be verified to be `tdsh` before typing, or the keys land in whichever app was selected last"
TOL-008: "Deleting sdkconfig to force regeneration reverts every setting that lives only in it, silently: on this board that dropped CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH and cut the ISR stack from 2096 to 1536, which matters because the console is compiled out and the coredump is the only crash record"
EMU-023: "Slot B gives the Game Boy 6.6 percent of frame time and removes 334 us of preemption per frame; moving the blitter to the idle core 0 makes the guest 7.7 percent faster again but drops 14 percent of displayed frames, so it stays on the guest core"
TOL-009: "A weak definition in an object the link already includes satisfies every reference to that symbol, so the archive member holding a strong definition is never fetched; ESP-IDF archives port sources into libmain.a, so a port's strong override of a weak board hook is silently absent from the image until `-Wl,--undefined=<symbol>` forces the member out, and `nm` shows `W` instead of `T`"
BRD-006: "The XL9555 expander's CONFIG and OUTPUT registers survive an ESP32-S3 reset, so a peripheral it switches stays powered across a warm reboot and is unpowered on a cold boot; on a cold boot every expander pin is an input, which is why the panel lit up after a warm reset with no code raising its rail"
```

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
  statement: "The panel's addressable area is 410 by 502 pixels, windowed at column 22 because the controller's RAM is wider than the visible glass. Its corners are chamfered at 45 degrees rather than rounded, so the largest rectangle guaranteed to render in full is 320x412, being the panel inset by 45 pixels on each side. fb0 is set to 320x400 and placed at offset (45,45), giving fbcon 40 columns by 25 rows of the VGA 8x16 cell with no driver-side arithmetic. THE CHAMFER IS THREE MEASURED LINES AND NOT A RADIUS: x + y = 80 at the top-left, (410 - x) + y = 90 at the top-right, and x + (502 - y) = 82 at the bottom-left, read off a full-panel grid photographed against fiducials. A pixel renders only if all four hold, so the usable width DEPENDS ON THE ROW: at panel row 10 the glass spans x 70..330, which is 261 px inclusive, and by row 45 the cuts no longer bind at fb0's width (331 px available against the 320 fb0 uses)."
  consequence: "The area outside fb0 — 45 pixels wide, 45 above and 57 below — is not part of the framebuffer, so neither fbcon nor userspace can draw there. It exists only in the panel's own RAM and can only be painted by the driver at probe time, in panel coordinates. The 57-row strip below fb0 is what the three touch buttons are painted into. THE ROWS ABOVE fb0 ARE THE NARROWEST PART OF THE GLASS AND THE ONLY PART THAT IS: because the top edge sits inside the chamfer's leg, the usable width there is 261 px at row 10 and 243 px at row 1, against 331 px by row 45. Anything drawing in rows 0..44 -- the TinyDesk port's status strip is the only such case -- must size its content to the chamfer at the row it occupies, not to the panel's 410 columns and not to fb0's 320. Measured by a host harness that compiled the real font array and the real placement arithmetic and counted every ink pixel against the three lines: an 18-cell 288 px line at scale 2 loses 61 ink pixels to the cuts, a 16-cell 256 px line clears by only 4 px at its worst, and the shipped 224 px line clears by 21 px."
  sources:
    - "patches/kernel/display/final/co5300.c: PANEL_W/PANEL_H/PANEL_OFFSET_X/PANEL_SAFE_INSET and the FB_X/FB_Y derivation"
    - "docs/panel-geometry.md: the measurement method, the three chamfer-line readings, and the grid/outline experiments that established them"
    - "kernel log: 'Console: switching to colour frame buffer device 40x25' and 'co5300 spi0.0: 410x502 RGB565, fb0 registered'"
  verification: "Confirmed on hardware by the console geometry itself and by painting the surrounding border in panel coordinates, which only works if the pad between fb0 and the visible edge is real. Revised 2026-11-09 (core-log entry 3): FB_Y is 45, not the 46 this record previously stated. (502 - 412) / 2 is 45, and the driver's own probe line 'co5300: frame painted rc=0, ring cols 66/387 rows 44/445' only reconciles with FB_Y = 45, since those are FB_Y - 1 and FB_Y + FB_H. The running driver was always correct; this record and the README repeated a wrong comment. Revised 2026-11-15 (core-log entry 11): added the three measured chamfer-line equations and the consequence that the usable width above fb0 is row-dependent. The equations were already in docs/panel-geometry.md but this record had not carried them, so the port's status strip was written against the panel's 410 columns and an 18-cell line lost ink to both cuts. The row-by-row widths are arithmetic on the recorded lines, checked on the host; the strip's own placement was then confirmed on the glass by the operator."

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

- record_id: TCH-011
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-12
  title: "There are two notions of who owns the screen, and the margin gate asked the wrong one"
  statement: "The TinyDesk port's touch driver refused the thirteen margin buttons when an application owned the screen, asking `td_wm_app_owns_screen()`. That predicate is the desktop's own idea of ownership: it is true for a window manager window that has gone full screen. It is FALSE for an application that calls `display_pixels_claim()` directly without being a window manager window, which is what the Tamagotchi emulator does -- so the gate was inert for the one app that needed it, and a press on the bottom strip's Enter while the guest had the screen sent Enter to the shell underneath. The gate is now `display_pixels_claimed()`, the port's own flag, which is the same one the framebuffer painter honours."
  consequence: "Two ownership flags existed and only one of them described the glass. The painter's flag is the honest one, because it is the condition under which nothing else may write pixels; the window manager's flag describes a window's state and merely correlates with that. When two predicates both answer a question that sounds the same, the one that GATES THE WRITES is the one the rest of the system must agree with, and anything else is a second definition waiting to disagree. The observable failure was quiet in a characteristic way: the margins worked normally on the desktop and worked normally in a full-screen window, and failed only for an app that had learned the port's own API instead of the desktop's -- so every test that used the desktop's path passed. A refused press now logs `e<n> i<n> off` and does not buzz, so the suppression is visible rather than inferred from the absence of a key."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/touch.c: the gate in the press path, display_pixels_claimed() replacing td_wm_app_owns_screen()"
    - "tinydesk-twatch ports/twatchultra/main/display.c: display_pixels_claim(), display_pixels_claimed() and the paint task's use of the same flag"
    - "tinydesk-twatch ports/twatchultra/main/tama.c: display_pixels_claim(true) and the absence of any window manager involvement"
    - "DSP-001 for the ownership contract itself, and DSP-005 for the one-writer rule this gate protects"
  verification: "Verified on hardware 2026-11-12 by the operator, who asked for the margins to stop controlling the desktop and confirmed the result as perfect after a reboot from a fresh build; the guest ran and stopped cleanly across that cycle. The specific pre-fix fault -- Enter reaching the hidden shell -- was reasoned from the two predicates and their callers rather than caught in a log, and the fix is therefore verified by the requirement it was written for rather than by observing the bad state first."

- record_id: TCH-012
  kind: TOUCH
  topic_id: TCH
  status: VERIFIED
  verified_date: 2026-11-12
  title: "The report block carries two points six bytes apart, idle is 0xFF, and lifting the first finger promotes the second with event 0x16"
  statement: "Measured by dumping raw 15-byte report blocks to /sd/touch.txt while performing deliberate two-finger gestures. (1) A block carries two points at six-byte stride: point 1 has its event at byte 0 (0x06 for a press) and coordinates at bytes 1-3; point 2 has its event marker at byte 7 (0x16), coordinates at bytes 8-10 in the SAME 12-bit encoding as the first (x = (hi1 << 4) | (lo >> 4), y = (hi2 << 4) | (lo & 0x0F)), and validity 0xAB at byte 12. (2) An IDLE read is fifteen 0xFF bytes -- NOT the 0x00 or 0xAB the driver's own comments had claimed -- and is rejected today only because the finger count in byte 5 decodes to 127. (3) Points are numbered by ARRIVAL ORDER: the finger placed first stays point 1 for the whole chord. (4) LIFTING THE FIRST FINGER of a chord PROMOTES THE SURVIVOR into the primary slot, moving its coordinates to bytes 1-3 and giving it event code 0x16."
  consequence: "Two of these change code and one changes a test. The promotion in (4) is the dangerous one: 0x16 & 0x0F == 0x06, so a driver testing the nibble accepts a promoted second finger as a fresh primary press -- and since the survivor keeps its own coordinates, releasing an arming finger would fire whatever button the other finger was resting on, with any interlock already satisfied and gone. The press test is therefore an exact b[0] == 0x06. The 0xFF idle value in (2) means the driver's documented understanding of 'nothing there' was wrong even though its behaviour was right by accident, which is the kind of thing that breaks when someone later 'fixes' the finger-count check. And (3) plus the six-byte stride mean a consumer must treat a report as a SET of points, not as an ordered pair with a designated first -- which is exactly how the Tamagotchi's hold-to-arm interlock is written, so that it cannot invert when the arm finger happens to land second."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/touch.c: the second-point decode at bytes 7..12 and the exact-event press test"
    - "tinydesk-twatch ports/twatchultra/main/touch.c: touch_dump(), the raw-block capture"
    - "captures preserved at ~/twatch-backup/touch-captures/: touch2.txt (single finger on a bar), touch3.txt (reserved + bottom-left), touch7.txt (full chord with lift-order, md5 1718e025e60e7e5a7086c478cbe03032)"
    - "TCH-003 and TCH-008 for the single-point layout and the acknowledge rule these extend"
  verification: "Verified on hardware 2026-11-12 across four captures with four different gestures. The second point was confirmed independently twice: a second finger on the left column's TOP slot decoded to controller y 92 (slot 0), and on the bottom slot to y 432 (slot 4), which are different answers for different gestures rather than one pattern matched twice. The promotion was seen in the lift-order capture as a run of 226 blocks at n=1 carrying event 0x16 with the surviving finger's coordinates in the primary slot. Idle-as-0xFF was seen in every capture as runs of identical ff-blocks."

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
  consequence: "EXPLAINED 2026-11-12 by the leftover-strips fault in the TinyDesk port, which is the same defect seen from the other side. The cap was right and this record's own variables were the wrong ones to suspect: what a transfer cannot carry is ROWS, and every pattern here that tore was a tall single transfer while every pattern that was clean was banded. The wider records in DSP-004 carry the reasoning and the fix; this one keeps its measurements and now points there instead of leaving the cause open. The rule that holds on hardware is that a single transfer must carry few rows, so `CO5300_IOC_WRITE_RECT` caps every band at two rows and loops. That is invisible to callers -- userspace still hands over one rectangle -- and it is cheap here because these are status icons, but it is a workaround and not a fix: it would be wrong for continuous drawing, which is why `co5300_blit()` keeps its own uncapped sizing. The root cause is NOT established, but SPI clock margin is now excluded as the explanation for the reference firmware's own 45 MHz shearing: that signature is also what a write-pointer desync looks like, and the reference firmware carried the SPI_USR_COMMAND/SPI_USR_ADDR-resent-per-chunk bug that restarts the write pointer every 64 bytes. That bug is fixed here, and the panel has since been tested at 40 MHz with eight full-width bands in eight distinct colours, which came out clean and in order -- a desync cannot produce that. 40 MHz is now the compiled-in default. What remains unexplained is this record's own narrow-transfer fragility, which persists at the higher clock and is therefore not about the clock."
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

- record_id: EXT-004
  kind: EXTERNAL
  topic_id: EXT
  title: "TinyDesk shows and homes its cursor, so a cell-based panel driver paints a solid block at cell (0,0)"
  status: VERIFIED
  verified_date: 2026-11-24
  statement: "TinyDesk's render layer emits `ESC[?25l` when it enters a full-screen screen and `ESC[?7h ESC[?25h ESC[?1049l` when it leaves one, and the shell's editor emits `ESC[?25h ESC[0m ESC[2J ESC[H` -- show cursor, reset attributes, clear, home. `td_vterm_init()` also starts with `cursor_visible = true`. So after either path the terminal model holds `cursor_visible` true with the cursor at cell (0,0), which for a driver drawing the framebuffer from the cell grid is the framebuffer's top-left cell. Drawing such a cursor is not a glyph: the vterm renderer INVERTS the cell's foreground and background for that one cell."
  consequence: "A cell-based panel driver that honours `cursor_visible` therefore paints one inverted cell at the top-left, and on a dark taskbar cell that is a solid light block exactly one character in size. It is permanent rather than blinking because the paint loop keeps honouring the cursor once it stops moving, and it appears on a boot path where the desktop's first paint finds the cursor homed. The port does not want it anywhere -- the desktop is a window manager drawing cells and a terminal window's caret is the shell's own content -- so `display_cursor_draw()` gates the inversion and defaults to off (`ports/twatchultra/main/display.c`). A driver that DOES want a text caret must implement the inversion, not the U+2588 glyph: the two look the same on the glass and only one of them is what the terminal asked for."
  sources:
    - "tinydesk src/render.c:205 and :220, the `ESC[?25l` on entry and `ESC[?7h ESC[?25h ESC[?1049l` on exit"
    - "tinydesk third_party/tdsh/ports/esp_idf/components/tdsh/src/tdsh_nano.c:1200, editor_leave_screen()"
    - "tinydesk src/vterm.c:62, `vt->cursor_visible = true` in td_vterm_init()"
    - "tinydesk-twatch ports/twatchultra/main/display.c: paint_cell()'s inversion and display_cursor_draw()"
  verification: "Established by reading the four call sites and the framebuffer painter, then confirmed fixed on the glass 2026-11-24: the operator had reported a solid white one-character block at the framebuffer's top-left corner on every reboot into slot A, for a long time and predating recent work, and it was gone after the inversion was gated off (core-log entry 16). THE CONFIRMATION CAME FROM THE OPERATOR, NOT FROM THIS AGENT: the artifact needed the glass, and the USB mirror carries the terminal stream rather than framebuffer pixels, so it could not be observed from a host. The size, colour, position and permanence could all be predicted from the mechanism before the test, and all four matched."

- record_id: EXT-005
  kind: EXTERNAL
  topic_id: EXT
  title: "gnuboy savestates carry a format version but no cartridge identity, and short writes are not detected"
  status: VERIFIED
  verified_date: 2026-11-24
  statement: "gnuboy's `do_save_load()` writes `SAVE_VERSION` (0x107 in this tree) as the first field of a 53-field named header -- CPU registers, IME and the interrupt mask, timing, the mapper's bank state, the RTC block and the four sound channels -- and then four block runs: 1 block of the 4 KiB `buf` holding `hw.ioregs`, `hw.pal`, `hw.oam` and `hw.snd->wave`, then `hw.rambanks`, `hw.vbanks` and `cart.rambanks`, each at 4096-byte granularity with a block count of 8 or 2 (WRAM), 4 or 2 (VRAM) and `cart.ramsize * 2` depending on the hardware type. Loading checks only `sav_ver != SAVE_VERSION`; there is no cartridge identity anywhere in the file and no length check. Both directions test `fwrite`/`fread` only for a return below ONE block, so a write cut short after its first block returns success."
  consequence: "Two facts a caller has to design around rather than fix. A savestate file is ONLY meaningful for the cartridge that produced it, so a driver must key the filename on the ROM, which is what the watch port does with `/sd/saves/gbc/<rom>.st0` -- one file per cartridge, no catalogue. And a truncated or failed write can be reported as a clean save, so the only trustworthy evidence that a savestate is good is loading it, not the return code from saving it. NEITHER IS A LIMIT TO FIX IN THE VENDORED CORE unless the port grows a savestate slot list or a shared file: with the file keyed per cartridge the first cannot bite, and the second leaves a truncated file that the next load reads as a version-valid stub."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/gnuboy/gnuboy.c: SAVE_VERSION at line 741, do_save_load(), the sblock_t blocks[] table at 839, and the `fwrite < 1` / `fread < 1` tests"
    - "tinydesk-twatch ports/twatchultra/main/gb.c: s_state_path, and the guest-task request that calls these"
  verification: "Read from the vendored source on 2026-11-24 while wiring the Save and Load buttons to it (core-log entry 17). The save and load paths themselves were confirmed working on the glass by the operator in the same entry, so the format round-trips for one cartridge. THE LIMITS ARE STATED FROM CODE AND NOT DEMONSTRATED: no truncated write and no cross-cartridge load was exercised, so treat the two failure modes as the code's shape rather than as measured behaviour."

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

- record_id: BRD-006
  kind: BOARD
  topic_id: BRD
  title: "The XL9555 expander's direction and output registers survive an ESP32-S3 reset, so a peripheral it switches stays powered across a warm reboot and is dark on a cold boot"
  status: VERIFIED
  verified_date: 2026-11-23
  statement: "The XL9555 at I2C 0x20 switches the AMOLED's rail (port 0 bit 7, P07) and the DRV2605's (port 0 bit 6, P06) through its CONFIG and OUTPUT latches. Both registers keep their values across a reset of the ESP32-S3; the expander is not reset with the MCU. On a cold boot, from a power cycle, every expander pin is an INPUT, so neither rail is driven and both peripherals are unpowered until something configures the pin as an output. After a warm reset, the pins are still configured as outputs and the latches are still high, so both peripherals come up already powered."
  consequence: "This makes the panel's behaviour depend on how the board started, which is the opposite of what a driver should assume, and it produced a specifically misleading result on this project. A second boot personality that brought the display up without raising the panel switch showed a lit screen after a warm reboot and would have shown a black one after a power cycle -- so the success proved nothing about the cold path. Drive the pins explicitly at every bring-up whose correctness matters (power.c's board_display_power_on and board_haptic_power_on, and board_touch_reset_release for the touch controller's reset line, which is held in reset by the same expander) rather than inferring the rail state from what the glass shows. This record is separate from BRD-005 because it is a property of the expander and applies to every peripheral it gates, not to the haptic chip."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/power.c: xl_pin_set/xl_pin_high writing XL9555_CONFIG0 and OUTPUT0, and the note at board_touch_reset_release that 'on a cold boot the expander has every pin configured as an input'"
    - "BRD-005 for which bit gates the haptic rail, and patches/kernel/display/NOTES for the display's P07"
  verification: "Established on hardware 2026-11-23 (core-log entry 92) by a failure that named itself: slot B drew on the panel correctly after a warm reboot from the desktop while raising no rail at all, and the motor stayed silent although its rail was up -- so the display's success was the latch's doing, not the driver's. power.c's own comment states the cold-boot half, and the fix was to call board_display_power_on() unconditionally so the cold path is exercised rather than inherited."

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

- record_id: FLS-004
  kind: FLS
  topic_id: FLS
  status: VERIFIED
  verified_date: 2026-11-16
  title: "A 2 MB raw `modules` partition at 0xE00000 holds codec blobs to be mmap-ed as instructions, and a littlefs partition's live size is only knowable by mounting it"
  statement: "The T-Watch Ultra's 16 MiB flash is laid out as nvs 24K, phy_init 4K, ota_0 3M at 0x10000, ota_1 3008K at 0x310000, coredump 64K at 0x600000, otadata 8K at 0x610000, storage (littlefs) and, since this change, a 2 MB `modules` partition of type data and subtype 0x40 at 0xE00000. `storage` shrank from 0x9E0000 to 0x7E0000 to fund it, so it is now 7.87 MB (2016 blocks of 4096). The two OTA slots are unchanged. `modules` is raw: Xtensa executes from flash natively because every instruction ESP-IDF runs is XIP-mapped, so a blob there can be mapped with esp_partition_mmap() as SPI_FLASH_MMAP_INST and called, costing no internal SRAM to load -- which is the resource this board is short of. Subtype 0x40 is the first application-defined data subtype; ESP-IDF reserves 0x00-0x3F."
  consequence: "A codec or decoder that does not fit the app image has somewhere to live. The app had 27.5 KB of headroom in ota_1 and the FLAC decoder already cost 31.7 KB, so a second codec could not be added inline at all; all eleven Rockbox codecs are 704 KB against this partition's 2 MB. NOTHING LOADS FROM IT YET, and the open risk is real: a blob must be linked position-independently for this address and callable across the Xtensa windowed ABI, and neither has been demonstrated. The partition costs nothing while it is empty. THE SECOND LESSON IS ABOUT MEASUREMENT: a littlefs partition's live contents cannot be inferred from the raw flash. This one showed 4,303,700 non-erased bytes across 2,429 of 2,528 blocks, 1,151,524 of them above the new 7.87 MB boundary -- which read as 'shrinking would destroy 1.1 MB'. Mounting it showed the truth: 25 files and 11 directories, 22,244 bytes, 34 blocks. The rest was stale blocks awaiting garbage collection. Any future decision to shrink or reformat a littlefs partition must start from a mount, not from a byte census."
  sources:
    - "tinydesk-twatch ports/twatchultra/partitions.csv (the table, with the reasoning in comments)"
    - "ESP-IDF 5.3.1 components/esp_partition/include/esp_partition.h (esp_partition_mmap, subtype ranges)"
    - "measurements: `esptool read_flash 0x8000 0xc00` decoded with gen_esp32part.py, and littlefs-python 0.19.0 mounting the extracted storage partition"
  verification: "Verified on hardware 2026-11-16 (core-log entry 25). The live table was read back from flash and decoded, showing `storage,data,littlefs,0x620000,8064K` and `modules,data,64,0xe00000,2M`. The migration was done by extracting all 25 files from the old partition with littlefs-python, rebuilding a fresh image at 0x7E0000 with them, and flashing that; four files including etc/board.conf compared byte-for-byte identical afterwards, and the board booted with its board keys, shell history, Apps, Start-menu .tdlink files and Desktop intact. The modules region was read back and confirmed all 0xFF. The music player still decodes the same 352800-frame 44.1 kHz tone with 0 errors. A full 16 MiB backup of the new layout was taken and its app region verified byte-identical to the build."

- record_id: MOD-001
  kind: MOD
  topic_id: MOD
  status: VERIFIED
  verified_date: 2026-11-16
  title: "Code loads from the modules partition by mapping flash as INSTRUCTIONS and calling it, and reaches the firmware through one API table whose address the blob build derives from the app's ELF"
  statement: "A raw flash partition can be executed from on the ESP32-S3 because every instruction ESP-IDF runs is already XIP-mapped: esp_partition_mmap(p, 0, p->size, ESP_PARTITION_MMAP_INST, &base, &h) returns an address in the instruction window 0x42000000..0x43ffffff (measured: 0x42ae0000 for a 2 MB partition at 0xE00000), and a function pointer into it can be called across a partition boundary with the Xtensa windowed ABI intact. Three constraints are measured rather than assumed. FIRST, the mapped bytes must not be DATA-READ through that pointer: a load faults with exccause 3 (LoadStoreErrorCause) while the same bytes read through esp_partition_read() are fine, so a blob header is read with esp_partition_read() into a stack struct and never through the mapped address. SECOND, mapping the same partition twice -- once DATA, once INST -- does not give two windows: the second mapping returned 0x3cae0000, which is PSRAM, and the call faulted with exccause 2 (InstructionFetchErrorCause). THIRD, a blob has no writable state: it executes from read-only flash, its .bss cannot be zeroed because no loader runs, so link.ld discards .data and .bss and the module must keep no static buffers. Because a blob cannot resolve an external symbol, it reaches the firmware through ONE table of function pointers (modhost_t, ABI-versioned); the firmware publishes the table's address in its own RAM as g_modhost_ptr and the blob build reads that symbol's ADDRESS out of the app's ELF with nm, passing it as a compile-time constant. Measured drift: rebuilding the app moved it from 0x3c27b630 to 0x3c27b650 within one cycle."
  consequence: "Code that does not fit the app image has somewhere to run without spending internal SRAM, which is this board's scarce pool. A blob is tied to the firmware build that produced it, because the table's address is absolute; the Makefile derives it every time, so the binding cannot silently go stale, and a check target fails loudly when the app has not been built. The build must compile HOSTED -- libstdc++ headers refuse -ffreestanding -- and enforce freestanding-ness at link time with -nostdlib plus a shim; C and C++ flags must be separate because gcc rejects -std=gnu++17, -fno-rtti and -fno-threadsafe-statics on C input; and the published symbol needs __attribute__((used)) plus a real reference from the firmware, since --gc-sections collects a section nothing references and the blob needs the symbol's ADDRESS rather than its value. A blob should report errors upward rather than assert: the shim's __assert_func reports through the table and returns."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/modload.c, modhost.c, modhost.h; modules/Makefile, modules/link.ld, modules/src/modshim.c, modules/src/modtest.c"
    - "ESP-IDF 5.3.1 components/esp_partition/include/esp_partition.h (esp_partition_mmap, SPI_FLASH_MMAP_INST, esp_partition_read)"
    - "coredumps: exccause 3 excvaddr 0x42ae0014 (data load through an instruction mapping); exccause 2 excvaddr 0x3cae0028 (executing a second mapping that landed in PSRAM)"
  verification: "Verified on hardware 2026-11-16 (core-log entries 26 and 27). A 77-byte blob mapped at 0x42ae0000 and called returned 42, proving control transfer and the windowed ABI. A 730-byte blob then read the API table at the build-derived address, called malloc, memset, memcpy and free through it, wrote into a caller-owned PCM buffer, and returned 42, with the console showing 'mapped for execution at 0x42ae0000', 'entry +0x2c' and 'returned 42'. The two fault causes are from coredumps decoded against the matching ELF. Neither the table's enoughness for a real codec nor its cost has been demonstrated: only memory and logging are carried so far, and a decoder additionally needs its buffers supplied by the caller."

- record_id: MOD-002
  kind: MOD
  topic_id: MOD
  status: VERIFIED
  verified_date: 2026-11-16
  title: "The two mmap addresses a module is linked for are runtime-chosen and MOVE when the app image changes size, and a blob linked for a stale pair still executes while reading its constants from the wrong place"
  statement: "A module with both code and constants needs TWO mappings of TWO partitions, because the ESP32-S3's address windows are complementary: 0x42000000..0x43ffffff fetches instructions and faults on a data load (exccause 3), 0x3C000000..0x3DFFFFFF serves data loads and faults on a fetch (exccause 2), and neither can serve the same bytes. So `modules` at 0xE00000 holds .text mapped ESP_PARTITION_MMAP_INST and `moddata` at 0xF00000 holds .rodata mapped ESP_PARTITION_MMAP_DATA. BOTH BASE ADDRESSES ARE CHOSEN BY THE MMU AT BOOT AND BOTH CHANGE WHEN THE APP IMAGE CHANGES SIZE: measured, 0x42ae0000/0x3cbe0000 while the inline FLAC decoder was linked into the app, 0x42ad0000/0x3cbd0000 once it was removed. A blob linked for the stale pair is not merely out of date -- IT STILL EXECUTES, because the entry point is reached by offset from whatever instruction base is live, and only the constant reads go wrong, from 64 KB away in the data window. micro-flac compares the input's first four bytes against a MAGIC_BYTES table in .rodata, so the only symptom was FLAC_DECODER_ERROR_BAD_MAGIC_NUMBER (-5) on files that were perfectly good."
  consequence: "The link addresses must be derived at build time from what the board actually mapped, never typed, and the loader must refuse a mismatch rather than call into one. modload_header_t therefore carries linked_inst and linked_data (ABI 2), modload_prepare() compares them against the live mapping and leaves the reason in modload_why() for whichever command reports it -- 'module linked for inst 42ae0000 data 3cbd0000, mapped at 42ad0000 3cbd0000 -- rebuild it' -- instead of a generic 'no module mapped at boot'. modules/moddata.at holds the pair and the Makefile passes BOTH as --defsym: MODTEXT_AT for the . = in link.ld and MODDATA_AT for the .rodata address, so the text base is not pinned in three separate places. `make modaddr` refreshes the file by reading /root/modaddr.txt (which modload_prepare writes at every boot) out of the raw storage partition with esptool, avoiding the console, the display mirror and the line discipline entirely. ANY app change that alters the image size requires make modaddr and a module rebuild; the guard makes forgetting it a log line rather than a decoder reading garbage."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/modload.h (modload_header_t, MODLOAD_ABI_VERSION 2), main/modload.c (modload_prepare, modload_why)"
    - "tinydesk-twatch ports/twatchultra/modules/link.ld, Makefile, patch_header.py, read_modaddr.py, moddata.at"
    - "the two observed pairs: /root/modaddr.txt read on the board before and after the inline decoder was removed"
  verification: "Verified on hardware 2026-11-16 (core-log entry 30, fork commits e712466 and 791cc7b). Both moves observed directly: the board reported inst=0x42ae0000 data=0x3cbe0000 while the inline decoder was linked in and inst=0x42ad0000 data=0x3cbd0000 after it was removed, and a module linked for the first pair reported 'bad magic number' on two tones that decode cleanly when relinked. The guard was then verified by deliberately flashing a header claiming the old base: it refused, printed the exact mismatch, decoded nothing, and did not crash. After relinking, a 5:58 track decoded at 15792504 frames with 0 errors and 0 short writes, and a clean rebuild reproduced the flashed blob byte-for-byte."

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

- record_id: LCD-014
  kind: DISPLAY
  topic_id: LCD
  status: VERIFIED
  verified_date: 2026-11-21
  title: "spi_device_acquire_bus() accepts only portMAX_DELAY: a bounded wait is refused outright, not merely slow"
  statement: "In ESP-IDF 5.3.1 a device's bus acquisition cannot be bounded. `spi_device_acquire_bus()` in components/esp_driver_spi/src/gpspi/spi_master.c begins with two checks before it touches any lock: `SPI_CHECK(wait == portMAX_DELAY, \"acquire finite time not supported now.\", ESP_ERR_INVALID_ARG)` and `SPI_CHECK(!spi_bus_device_is_polling(device), \"Cannot acquire bus when a polling transaction is in progress.\", ESP_ERR_INVALID_STATE)`. So a timeout passed as a tick count does not wait and then give up -- it returns ESP_ERR_INVALID_ARG immediately, having never attempted `spi_bus_lock_acquire_start()`. The second check is the one that fires when the SAME device handle is mid-transaction: two tasks polling the same `spi_device_t` overlap, and the loser is told the bus is busy rather than being made to wait."
  consequence: "A writer on the panel bus that wants to survive contention cannot do it by asking for a bounded acquire; it must either hold portMAX_DELAY and accept an unbounded wait, or check the result of an infinite acquire and skip the band. The port's `push_pixels_raw_buf()` does the latter and counts refusals as `s_bus_misses`. The practical consequence for diagnosis is the sharper one: because a refused acquisition costs a band and returns, a task that instead never returns is blocked somewhere else, so contention and a hang are distinguishable only if something records which phase the pusher had reached. Measured evidence that the refusal is silent and cheap: swapping the port's infinite acquire for `pdMS_TO_TICKS(100)` turned a miss count of 29 into 43,680 over one comparable emulator session -- every acquisition failed, and the emulator kept running with a black picture rather than crashing."
  sources:
    - "ESP-IDF 5.3.1 components/esp_driver_spi/src/gpspi/spi_master.c: spi_device_acquire_bus(), the two SPI_CHECK lines at the top, and spi_device_release_bus() which asserts `ret == ESP_OK` on a bus this device never acquired"
    - "tinydesk-twatch ports/twatchultra/main/display.c: push_pixels_raw_buf(), BUS_PH_* phase markers and s_bus_misses"
  verification: "Established on this project's tools and hardware 2026-11-21 (core-log entry 89). A bounded acquire was built and flashed; every panel push then failed, driving the driver's miss counter from 29 to 43,680 across one session, which is what sent the investigation to the driver source and found the SPI_CHECK. Reverting to portMAX_DELAY restored 0 to 15 misses per session. The `ESP_ERR_INVALID_STATE` polling check is the same one already recorded as fatal in display.c's comment, where ignoring it and then calling release produced `assert failed: spi_device_release_bus spi_master.c:1330 (ret == ESP_OK)` and a reboot."
- record_id: LCD-015
  kind: DISPLAY
  topic_id: LCD
  status: VERIFIED
  verified_date: 2026-11-24
  title: "The panel's TE output was enabled and never read; gating a whole-picture push on it removes the torn frame"
  statement: "The CO5300 has three tearing-effect output modes, selected by two commands: TEON (35h) with parameter M, and STESL (44h) with a 16-bit line number N. M=0 gives Mode 1, V-blanking only; M=1 gives Mode 2, V-blanking plus H-blanking; N!=0 gives Mode 3, a single pulse at line N; and STESL with N=0 is defined to be equivalent to TEON with M=0. In Mode 1 -- the mode this port enables -- the line is HIGH for the V-porch, and the datasheet defines that interval as one where the display is NOT updated from the frame memory (tvdh), against tvdl for the visible period where it is. TE is active LOW only in Sleep In mode; rise and fall are specified at 15 ns or less; the output is unaffected by MADCTL bit ML; and STESL takes effect on the frame AFTER the one in progress. THE DRIVER ALREADY ENABLES THE OUTPUT: `display.c` sends {0x35, {0x00}} in its init table, which is TEON Mode 1. What it never does is configure GPIO 6 as an input or read it -- `gpio_config()` is called for LCD_CS and LCD_RST only, and the schematic shows LCD_TE going straight to IO6 with no series or pull resistor."
  consequence: "A whole-frame update therefore takes about one frame's push time end to end (roughly 8 ms at the measured full-frame cost), and that span is not aligned to the panel's own scanout. The top of the glass shows the new content while the bottom still shows the previous content, for the duration of one push: on the glass that reads as a single torn frame rather than a persistent fault. This is the mechanism behind the tearing reported on a diagnostic cartridge that redraws the whole screen on a cycle of 2.4 s; the 2.4 s belongs to the ROM's own test sequence and is not a firmware period -- no task, timer or retry in the port has that period. THE FIX IS OPEN ON BOTH SIDES, which is why this record states the contract rather than a recipe: the panel is already producing a Mode 1 pulse and nobody listens, and the remaining work is on the HOST side only. Three things must be settled before code is written. FIRST, THE PULSE WIDTH IS NOT DERIVABLE FROM THE DATASHEET: the VBP, VFP and VSW figures this would need are listed as \"TBD\" in its vertical timing table, so the blanking window has to be measured on the board rather than read. SECOND, Mode 1 (blank only) and Mode 3 (a pulse at a chosen line, STESL) are different tools: Mode 1 bounded by blanking only gives the porch, which at 60 Hz is a fraction of 16.7 ms, whereas Mode 3 can place the reference edge wherever the push actually wants to start. THIRD, GSL (45h) reads the CURRENT SCAN LINE and would let a writer track scanout instead of guessing ahead of it -- but this port treats the panel as write-only, so whether that read answers at all has to be established first. A naive edge wait is the wrong default: it can add up to one refresh period per frame, 16.7 ms, against a guest measured at 6.9 ms of work in a 16.7 ms budget. THE POLARITY AND THE SAFE SHAPE ARE NOW ESTABLISHED FROM LILYGO'S OWN DRIVER rather than inferred: LilyGoLib's `LilyGoDispInterface.cpp` configures the pin as a plain INPUT, attaches an interrupt on the RISING edge, and its push path DRAINS the semaphore (`while (xSemaphoreTake(te_semaphore, 0) == pdTRUE) {}`) before taking it with a 20 ms timeout -- just over one refresh -- and on timeout logs a warning and CARRIES ON with the push. Two properties of that pattern matter more than the gate itself: draining first is what makes the wait mean anything, because a queued give otherwise returns immediately, and failing open is what stops a missing edge from hanging the watch. Their variant header also confirms `DISP_TE (6)`, agreeing with the schematic. THE COST TURNED OUT NOT TO BE A PROBLEM: LilyGo's `_use_tearing_effect` defaults to FALSE and their factory example never calls `enableTearingEffect()`, which is a signal they did not consider it free, but on this port a single gate per picture was enough and the operator confirmed the tearing gone on the glass. THE IMPLEMENTATION IS IN `display.c` AND THE SHAPE IS THE PART WORTH CARRYING FORWARD: gate `display_picture_rows()` ONCE, at the top, never per band -- a per-band gate would wait up to a refresh eighteen times for one frame."
  sources:
    - "tinydesk-twatch ports/twatchultra/board.conf: `lcd.te = 6`"
    - "tinydesk-twatch ports/twatchultra/main/display.c: BAND_ROWS and s_tx, push_pixels_raw_buf(), and the absence of any LCD_TE reference"
    - "Escapement .ai/core-reference.md BRD-004: the panel's TE line is GPIO 6"
    - "Xinyuan-LilyGO/LilyGoLib, src/display/LilyGoDispInterface.cpp: disp_te_isr(), `attachInterrupt(te, disp_te_isr, RISING)`, and the drain-then-take wait in pushColorsDMA()/pushColors() with a 20 ms timeout"
    - "Xinyuan-LilyGO/LilyGoLib, variants/lilygo_twatch_ultra/pins_arduino.h: `#define DISP_TE (6)`"
    - "Xinyuan-LilyGO/LilyGoLib, src/display/LilyGoDispInterface.h: `_use_tearing_effect(false)` in the constructor and `enableTearingEffect()`"
  verification: "Established 2026-11-24 (core-log entry 18 and entry 19) by searching the port for LCD_TE and reading the band arithmetic, then reading the CO5300 datasheet (sections 5.3, 7.5.27-7.5.28, 7.5.35-7.5.36) and the T-Watch Ultra schematic, both supplied by the operator; then, on 2026-11-24, LilyGo's own driver in the LilyGoLib repository, reached from the product wiki page, which settles polarity (rising), the input mode, the drain-first ordering and the 20 ms fail-open timeout from working vendor code rather than from a timing diagram. REVISED 2026-11-24 (core-log entry 19): this record first said the TE output was never enabled. That was wrong and the correction is the useful part -- the driver sends TEON Mode 1 at init and has done all along, so the panel has been emitting a synchronisation pulse nobody reads. What is missing is host-side only: no gpio_config() for GPIO 6 and no reader. THE TORN FRAME ITSELF WAS SEEN BY THE OPERATOR, NOT BY THIS AGENT, and no measurement has yet correlated a push with a TE edge; the band-straddling explanation was the mechanism that fit the observation and the code. CONFIRMED ON THE GLASS 2026-11-24 (core-log entry 20): with the gate in place the operator reported the tearing gone, which is both the fix and the proof of the diagnosis -- the tear was the unaligned push and nothing else. The pin's own liveness was measured before that test rather than assumed: `gb status` reported `te: edges=1211` with no emulator running, about 60 edges a second against an idle clock, which is the refresh rate and not a floating pin picking up noise."
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

- record_id: EMU-018
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "Adding the emulator to the firmware starved the SSH server of internal RAM, so its row tables are a PSRAM heap allocation"
  statement: "Putting tmc_core into the TinyDesk firmware, with its dirty-row tables as file-static arrays, added 4,148 bytes of .bss to internal DRAM. The board then refused to start its SSH server: `ssh: insufficient internal RAM (62443 bytes free; need at least 65536 before start)`, against a hard floor of 65,536 bytes that tdsh checks before creating the server task (src/tdsh_ssh.c, ssh_internal_needed()). Moving the tables to a single refcounted heap allocation, freed when the last emulator instance goes, moved internal free from 62,443 to 66,599 bytes and `ssh start` succeeded immediately; `heap` then reported `minimum-free=59919`. The allocation lands in PSRAM rather than internal RAM because the port sets CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096, so an 8 KB request is served externally."
  consequence: "This is the second time this port has been bitten by the same shape and the first time it was diagnosed rather than fixed blind: a component's .bss is internal DRAM by construction (extram.lf maps only libtinydesk, libtdsh and libmicropython to PSRAM), so DSP-002's 88,896-byte overflow and this 4 KB starvation are the same scarce resource failing two different ways -- one loudly at link time, one silently at runtime in a completely unrelated subsystem. The consequence for anyone adding to this firmware is that internal DRAM must be budgeted before the build, not discovered by a peripheral refusing to start: the board idles around 66 KB free, tdsh wants 65,536 for SSH alone, and the failure reports the number it could not meet but never the component that took the last bytes. Anything an app-sized feature needs should be a heap allocation, and a feature that is only sometimes in use should give the memory back -- which the refcount does. The same cycle found the companion diagnostic fact recorded as DSP-003: with a bitmap owning the panel the USB mirror no longer describes the glass, so a guest that is running and a board that has crashed both look like a stale desktop from the host, and the only view that survives is a file on the card. `tama` therefore rewrites /sd/tama.txt every 64 frames with frames, blits, cycles and esp_reset_reason()."
  sources:
    - "ports/twatchultra/components/tamacore/tmc_core.c: struct tmc_tables, tables_acquire() and tables_release()"
    - "ports/twatchultra/main/tama.c: report(), which writes /sd/tama.txt"
    - "third_party/tdsh/ports/esp_idf/components/tdsh/src/tdsh_ssh.c: ssh_start() and its internal-RAM floor"
    - "ports/twatchultra/sdkconfig.defaults: CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096"
    - "DSP-002 for the same scarce resource failing at link time, and DSP-003 for why the diagnostics go to a file"
  verification: "Verified on hardware 2026-11-12 in three steps. The failure was observed as the exact refusal above with 62,443 bytes free. After the change `heap` reported free=66599 and `ssh start` returned without an error, with the SFTP server answering on 10.10.0.15:22 and a 524288-byte ROM transferred at 26.6 KiB/s with md5 3bd0c293351ebb2bb221e9c69cc1c83b matching. The claimed cause is confirmed rather than assumed because the 4,148-byte figure is the .bss the component's tables occupied before the change and 66,599 - 62,443 = 4,156 recovered, which is that figure plus alignment."

- record_id: EMU-019
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "A button press is a cross-task write to the emulated CPU, so it is queued rather than applied"
  statement: "`tmc_button()` writes the emulated CPU's pin state, which `tmc_run()` reads on every step, and `tmc_core.h` states that one instance is not reentrant. In the firmware the guest runs on an unpinned task at priority 3 while touch.c's poll task is pinned to core 0 at the same priority, so applying a press from the touch task would be a data race on two cores. Accepted presses now go into an eight-deep FreeRTOS queue and the guest task drains it at the top of each frame, before `tmc_run()`. Measured on the glass in one run: 31 presses, 0 drops, across 9,107 frames."
  consequence: "The emulator keeps its single-threaded contract instead of growing a lock, which matters because the contract is what makes the whole dirty-row design simple -- a lock around `tmc_run()` would serialise the emulator against itself for no benefit. Draining BEFORE the step rather than after is load-bearing and not a detail: the guest samples its pins at its own rate inside `tmc_run()`, so a press applied afterwards is one frame late, and a frame is 62 ms of guest time. The general lesson is that an emulator's input surface is part of its state, not a message to it. It also surfaced a latent instance of the same fault that had been in the tree since Phase 4: `tama press N` wrote pin state straight from the shell task, and it now takes the same queue -- which is what made the drain verifiable from the host before any finger touched the glass."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/tama.c: tama_enqueue_button(), the drain at the top of the guest loop, and cmd_tama's press path"
    - "tinydesk-twatch ports/twatchultra/main/touch.c: the poll task pinned to core 0, and the call into touch_app_button_fn"
    - "patches/harness/core0/tmc_core.h: 'one instance is not reentrant', and tmc_button()'s contract"
    - "tinydesk-twatch ports/twatchultra/main/touch.h for why the sink is registered for the life of the process"
  verification: "Verified on hardware 2026-11-12. The queue path was first exercised from the host with three `tama press`/`tama release` pairs, which the report recorded as `presses=3 drops=0` -- so the drain was proven before the touch path existed. The operator then played the pet from the strip on the glass and confirmed it works, and that run's report recorded `presses=31 drops=0` over 9,107 frames and 569 s of guest time. That the guest's own game logic responds correctly to those presses is the operator's report, not a measurement."

- record_id: EMU-020
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "The audio emit counter counts square-wave toggles, so a front end keyed on it plays one tone for ever"
  statement: "splb32_sound emulates the V3's PWM and tone channels. `splb32_sound_toggle()` is called on every driver timer overflow while the channel's envelope is non-zero, and it increments `emits` on each call -- so a single held tone raises the counter thousands of times a second. `splb32_sound_set_data()` also increments it on register writes. The firmware's first version treated any change in this counter as 'the guest made a sound' and raised a beep, so a BEL played back to back for as long as the guest made any noise, and went silent only while the emulator was paused -- because pausing stops stepping the guest and therefore stops the timer overflows that generate the toggles."
  consequence: "The waveform is a clock and the envelope is the signal, and confusing the two is the fault. `splb32_audio_active()` answers the question a front end actually has -- is either channel currently driving its speaker -- by testing the envelope and the last written level, and a caller fires on its rising edge to get one event per sound. Two alternative fixes are both wrong and both were ruled out by measurement: watching the LEVEL fails because toggle_state alternates on every call, so it would fire twice per period; and watching the counter's slope fails because the slope is the pitch. A 250 ms floor sits on top of the edge because a beep is 200 ms long, so two closer together overlap into noise rather than reading as two sounds. The diagnostic value of this record is the clue that identified it: PAUSING SILENCED IT, which points at the waveform's clock rather than at the guest's intent."
  sources:
    - "patches/harness/splb32/splb32_sound.c: splb32_sound_toggle(), which increments emits on every call"
    - "patches/harness/splb32/splb32.c: splb32_audio_active(), and splb32_audio_emits() which exposes the counter"
    - "patches/harness/core0/tmc_core.c: tmc_audio_active(), the portable wrapper the firmware uses"
    - "tinydesk-twatch ports/twatchultra/main/tama.c: the rising-edge test with its 250 ms floor"
  verification: "Verified on hardware 2026-11-12: the operator reported a BEL that never stopped and that ceased while paused, and confirmed it fixed after the change. The fault is stated as a number by tools/tamacore/check_audio.c, run with ./scripts/tamacore-host.sh audio: 'one 1000-toggle tone is one sound (1000 emits, envelope constant)'. The check was proved sensitive by removing the toggle_state inversion from splb32_sound.c, which makes it fail with 'level did not alternate across a toggle'."

- record_id: EMU-021
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "A save-image size constant 512 short of the EEPROM's 1024 overflowed the stack, and the crash appeared three calls away"
  statement: "The emulated serial EEPROM is a 24LC08: `HT24LC08_SIZE` is 1024. `ht24lc08_save()` is `memcpy(out, e->mem, HT24LC08_SIZE)` -- it takes no length and always writes the full 1024 bytes. `TMC_EEPROM_BYTES` was 512, and both `save_checkpoint()` in `main/tama.c` and `cmd_tama`'s save path declare `uint8_t save[TMC_EEPROM_BYTES]` on the stack, so every checkpoint wrote 512 bytes past its buffer. `tmc_eeprom_load()` read 512 bytes past its buffer on the way back, because `splb32_eeprom_load()` calls `ht24lc08_load(e, data, HT24LC08_SIZE)`."
  consequence: "The overflow's destination is the point. On the shell task the write ran UPWARD through `cmd_tama` into tdsh's `execute_segment` frame and corrupted the `FILE *redir_out` local, which is initialised to NULL and never assigned for a command with no redirection. The parser's `if (redir_out) fclose(redir_out);` then closed a garbage pointer and newlib asserted: `check_lock_nonzero locks.c:307 (lock != NULL && "Uninitialized lock used")`. So `tama stop` rebooted the watch, and the backtrace pointed at tdsh, three calls from the line that caused it. Three things follow. A stack overflow does not have to hit a guard to be fatal -- it can quietly rewrite a caller's state and kill the program later, somewhere unrelated. A wrong size constant is invisible when the API that consumes it takes no length, so the defence has to be a static assertion against the device's own size rather than care at the call site. And the Save BUTTON hid the bug: `apply_action` calls the same function on the guest task, whose 6144-byte stack absorbed the overflow, so the saved file was correct and the memory around it was not -- a passing Save means the file is right, not that the machine is."
  sources:
    - "patches/harness/core0/tmc_core.h: TMC_EEPROM_BYTES, now 1024 with the reasoning recorded"
    - "patches/harness/core0/tmc_core.c: _Static_assert(TMC_EEPROM_BYTES == HT24LC08_SIZE)"


    - "patches/harness/splb32/ht24lc08.c: ht24lc08_save() and HT24LC08_SIZE"
    - "tinydesk-twatch third_party/tdsh/src/core/tdsh_parser.c:1630, the fclose that received the corrupted pointer"
    - "captured backtrace: #5 _fclose_r(fp=0x6d305b1b), #6 fclose, #7 execute_segment, #8 tdsh_execute_line (line="tama stop")"
  verification: "Verified on hardware 2026-11-12. The backtrace came from a core dump, not from bisection: `ota_1` has never been used (no update feed on this board), so it was shrunk from 0x300000 to 0x2F0000 to free 64 KB at 0x600000 for a `coredump` partition, with `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` and `..._DATA_FORMAT_ELF`; `idf.py coredump-info` then named the assert and every frame. The partition and the config were reverted and the watch reflashed from a clean tree before the fix was accepted. After the fix, two complete start/stop cycles: `uptime` climbed 17 to 27 to 34 seconds with `stopped, screen released` printed each time and no reset. The static assertion was proved to work rather than assumed -- setting the constant back to 512 fails the build with 'save image size must match the emulated EEPROM'."

- record_id: EMU-022
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-21
  title: "A 64 KB data cache buys about 1.5 percent of the Game Boy's frame time and costs 17 KB of internal heap, so the data path is not cache bound"
  statement: "Doubling the ESP32-S3 data cache from CONFIG_ESP32S3_DATA_CACHE_32KB to _64KB, changing nothing else, moved the Game Boy CPU-only benchmark (Shantae, 3000 frames, rendering off) from 7915 and 7886 us a frame at 32 KB to 7718 and 7850 us at 64 KB. Run-to-run spread on a single build is about 130 us, so the two configurations are not distinguishable. The cost is measured on the same boot: heap_caps_get_free_size(MALLOC_CAP_INTERNAL) falls from 27467 to about 10020, and the heap's total size reported by `mem heap` falls from 259083 to 231279 -- 27804 bytes taken by the cache, of which about 17,000 is the difference in free space once the driver's other allocations settle. The 64 KB figure is not usable as-is: with it the desktop still ran but no shell could start until 4 KB more was freed by shrinking the statusbar task's stack, and the internal low-water mark fell to 2611 bytes."
  consequence: "This closes the last internal-RAM lever on the emulator, and it closes it the same way entry 87's IRAM placement did. Four changes have now been tried that spend something to buy frame time: jump tables cost nothing and gained 7 percent; moving the interpreter to IRAM gained nothing for 12 KB; moving VRAM to internal RAM crashed; a 64 KB data cache gains about 1.5 percent for 17 KB and leaves the shell unable to start. Internal RAM is not where this emulator's time is, and the likely reason is that the working set the cache could help -- VRAM plus CPU state -- already fits in 32 KB, while the 1 MB cartridge streams through either way. The practical use of the number is architectural: an emulator-only firmware profile would not go faster for having more cache, so any reason to split the application across two slots has to be a reason other than frame time."
  sources:
    - "ports/twatchultra/sdkconfig.defaults: CONFIG_ESP32S3_DATA_CACHE_32KB / _64KB"
    - "ports/twatchultra/main/gbcbench.c: run_us, mode=cpu, and /sd/diag/gbcbench.txt"
  verification: "Measured on hardware 2026-11-21 (core-log entry 90). Two runs at each cache size on the same ROM and frame count, with the result written to /sd/diag/gbcbench.txt because the panel cannot hold it. The heap figures are from `mem heap` and from the same file on the same boot as each run."

- record_id: OTA-001
  kind: EXTERNAL
  topic_id: OTA
  status: VERIFIED
  verified_date: 2026-11-12
  title: "A/B firmware update works here with no repartitioning, and a corrupt slot is refused at boot rather than booted"
  statement: "This port's partition table already carries two 3 MiB app slots (`ota_0` at 0x10000, `ota_1` at 0x310000) plus `otadata` at 0x610000, and `ports/esp_idf/app/ota_esp.c` already implements the whole ESP-IDF A/B path: `esp_ota_set_boot_partition()`, `esp_ota_mark_app_valid_cancel_rollback()`, `esp_ota_mark_app_invalid_rollback_and_reboot()`, an `ota` shell command (status, feed, check, install, cancel, restart, rollback) and the Update app. Measured on the watch 2026-11-12: `ota_0` running, `ota_1 is empty`; `ota install` reports `Version 0.4.0 is installed. Restart to use it.`; `ota restart` comes up `Running: ota_1 (on trial: confirms itself 30 s after start-up)`; the trial flag clears about 35 s later; a second install writes `ota_0` and a second restart returns to `Running: ota_0`. `otadata` then holds sequence 3 for one slot against 2 for the other."
  consequence: "The swap is real and needs no new partitions, which retires the premise that an 8 MiB repartition is required before firmware updates can be trusted. Two operational facts came out of doing it. First, `ota install` refuses to proceed on this board until `board save` has run, because 28 pin settings are compiled into this port's firmware and a generic image would start without them -- the guard is correct and running it also proves `/fs` is writable. Second, the transfer path that works is HTTP: SFTP stalls and leaves a 0-byte file (the single-client wolfSSH wedge, SSH-003), while `ota install http://host/file.bin` streams at about 52 KB/s. The rollback protection was verified against a REAL corrupt image rather than a contrived one: an install interrupted by a hard reset left `ota_1` with a valid descriptor and a truncated body, and the bootloader refused it with `E esp_image: invalid segment length 0xff9afffd`, staying on `ota_0`. That is the property the mechanism exists for, and the corruption was self-inflicted -- do not reset the board during an install. Revised 2026-11-23 (core-log entry 95): a revision note added earlier the same day claimed the trial NO LONGER CONFIRMS ITSELF on this build. THAT WAS WRONG AND IS WITHDRAWN. The measurement behind it was a reader fault: the otadata state was decoded with an enum map shifted by one, so ESP_OTA_IMG_VALID (0x2) printed as PENDING_VERIFY. Re-measured correctly, the trial behaves exactly as this record says -- a desktop confirmed drawing and left running 45 s reads back VALID, and OTA-001's 2026-11-12 figure stands. The enum values that must be used are NEW 0x0, PENDING_VERIFY 0x1, VALID 0x2, INVALID 0x3, ABORTED 0x4, UNDEFINED 0xFFFFFFFF. What the incident did establish is a separate operational fact, recorded here because this is the record that owns the A/B machinery: a reset inside the 30-second trial makes the bootloader mark the pending entry ABORTED and fall back to the OTHER valid image, so a slot switch followed by a quick reset lands in the old personality. That is rollback protection working as designed. Revised 2026-11-21 (core-log entry 43's repartition, confirmed by entry 90): the SLOT ADDRESSES in this record are stale. The table is now ota_0 at 0x20000 and ota_1 at 0x400000, each 0x3E0000, with otadata at 0x7F0000 and coredump at 0x7E0000 -- the mechanism and every behaviour measured here are unchanged, but the offsets quoted above belong to the pre-2026-11-17 layout."
  sources:
    - "tinydesk-twatch ports/twatchultra/partitions.csv: ota_0, ota_1, otadata"
    - "tinydesk-twatch ports/esp_idf/app/ota_esp.c: ota_install, ota_roll_back, ota_esp_boot_ok, cmd_ota"
    - "tinydesk-twatch third_party/tdsh/src/core/tdsh_parser.c for the shell surface, and apps/update.c for the UI"
    - "backup working-2026-11-12-2059.bin: both slots carrying valid 0.4.0 images, otadata seq 3 vs 2"
  verification: "Verified on hardware 2026-11-12 by the full round trip quoted in the statement, over HTTP from the development machine, with `ota status` read after each step. The corrupt-image refusal was observed directly in the install log. The built-in feed `https://tnydesk-project.github.io/install/update-desktop-twatchultra.json` was checked from the host and returns HTTP 404, so 'Check for official updates' has no feed on this board; that is recorded rather than assumed working. NOT verified: booting from a partition at any offset other than 0x10000, since no repartitioning has been done."

- record_id: NES-003
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-12
  title: "The NES blit spent 70 percent of the frame copying an image through two PSRAM buffers to reach a 10 KB internal one"
  statement: "`nes_blit()` converted the 272x240 index buffer into a 120 KB PSRAM scratch frame, called `display_picture_rgb()`, which copied that into the 260 KB PSRAM framebuffer, and then `flush_rect()` copied the same pixels straight back out into `s_tx`, the 10,240-byte internal transmit band. Each arc of that round trip was 120 KB or 260 KB crossing the PSRAM bus, and the framebuffer is far larger than the cache, so both were streams of cache misses. The panel push itself is only about 3,072 us for a full frame at 80 MHz QIO -- 61,440 pixels, 16 bits, 4 bits per clock -- so the copying dwarfed the transfer it existed to feed."
  consequence: "Streaming the conversion into the transmit band -- `display_picture_rows()` drives the band loop and asks a callback for one row, so the palette lookup writes where the push will read from -- removed the scratch buffer and both framebuffer passes. On Zelda's title screen, measured over 25-second runs: 19 fps to 42, dropped 531 to 6 per second, and the blit from 37,140 us to 8,647 us of a frame. Two further numbers set the ceiling and are worth as much as the win. First, moving the frame buffer out of PSRAM is nearly worthless: a static internal array measured 42 to 45 fps, saving 577 us of an 8,647 us blit, because the blit is dominated by the push and not by reading the buffer -- a prediction of several milliseconds that the measurement refused. Second, that push is NOT inefficient, which was the expectation and is wrong: instrumenting it separately showed 4,054 us per frame, of which 3,360 us is the transfer itself -- 1.10 times the 3,072 us bytes-and-clocks arithmetic, so the SPI is running at essentially full speed -- and about 700 us is the fifteen window programmings at 48 us each. The earlier 8,047 us figure was blit-minus-estimated-conversion, an inference presented as a measurement, and it made a 2.6x excess look like the next 5,000 us to win. There is no such excess. The arithmetic that closes the question, now on measured per-frame terms: interpreter and PPU 15,242 us, conversion 4,355 us, push 4,054 us, total 23,651 us. Even deleting the entire blit leaves 15,242 us, which is 65.6 fps, so 60 fps is reachable only by running the conversion and the push CONCURRENTLY with the emulator rather than after it -- not by making either of them cheaper, since both are already near their floors."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: display_picture_rows(), and flush_rect() above it for the old path"
    - "tinydesk-twatch ports/twatchultra/main/nes.c: nes_fill_row(), nes_blit(), and the NES_VIDBUF_STATIC switch whose comment carries both measurements"
    - "tinydesk-twatch ports/twatchultra/main/display.c:54 LCD_SPI_HZ, plus PIX_CHUNK and push_pixels_raw for the transfer overhead"
  verification: "Measured on the watch 2026-11-12. `nes status` after 25-second runs of /sd/zelda.nes read frames=1027 fps=42 dropped=9 emu=1010056us blit=380300us, against 19 fps from the previous cycle with the same command; the first streaming build read frames=1108 fps=42 dropped=6 emu=1010816us blit=381153us. The operator played the game and reported 'i feel 42fps. graphics look good.' That confirmation is what makes this VERIFIED rather than merely fast: the panel is write-only and a frame counter cannot say the image is correct, and the conversion now writes panel byte order from a pre-swapped palette, so a mistake there would have produced 42 fps of wrongly coloured picture. The internal-DRAM variant is recorded as measured and rejected, not shipped. Revised 2026-11-12 (core-log entry 61): the push was instrumented separately instead of inferred, and the 8,047 us figure with its claimed 2.6x excess was wrong -- the transfer runs at 1.10x theoretical and the push totals 4,054 us. The consequence changed accordingly: there is no waste to reclaim in the push, and the reachable ceiling is set by serialising blit work after emulation, not by the cost of either."

- record_id: PM-001
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-13
  title: "A Pokemon Mini game needs 0.22-0.36 MIPS, not 1 MIPS - the 1 MIPS figure is the hardware ceiling, not the demand"
  statement: "The S1C88 runs at 4.00 MHz with a 4-cycle instruction fetch, so the machine's ceiling is 1 MIPS and its frame budget at 72 Hz is 13,889 instructions. The scope document written on 2026-11-09 states the port's requirement as '1 MIPS sustained', which conflates that ceiling with what software asks for. Measured on the host across all twelve retail cartridges held, running their attract modes for 8,000 frames each with `-O2 -DPERFORMANCE`: 3,091 to 5,053 instructions per frame, or 22 to 36 percent of the budget. At 72 Hz that is 222,552 to 363,816 instructions per second actually required."
  consequence: "The target is three times less demanding than the original document assumed, which changes the port from 'needs a fast interpreter' to 'needs a modest one'. It also means a core that lands at 0.25 MIPS on the watch is already enough for the lightest cartridges, so the go/no-go threshold in the older document is pitched too high. Caveat recorded because it bounds the claim: these are attract modes, not played sessions, and input-driven scenes are heavier -- the 13,889 ceiling is hard by definition, so 36 percent is a measured floor of demand rather than a bound. Host interpreter throughput itself ranged 63.2 to 135.6 Mips across the twelve, so the per-frame instruction count and the host speed are separate measurements and should not be combined into a ratio."
  sources:
    - "tinytama docs/POKEMINI-CORE.md, the measurements table and the 13,889 budget"
    - "tinytama core/pokemini/patches/0001-exec-counter.patch, the 2-line instruction counter that makes the count possible"
    - "Escapement scripts/pmbench.sh and tools/pmbench/bench_pokemini.c, which reproduce the table"
  verification: "Measured 2026-11-13 by rebuilding the harness from source in this repository and running all twelve cartridges from the MiSTer set at 8,000 frames each. A synthetic 20,000-frame run reproduced at 82.0 and 86.0 Mips on two builds, so the harness is stable to about 5 percent. The core itself was copied from the tinytama vendoring rather than re-fetched, so it carries that project's audit and the same pinned revision 8a64225e."

- record_id: PM-002
  kind: DISPLAY
  topic_id: DISPLAY
  status: VERIFIED
  verified_date: 2026-11-13
  title: "Pokemon Mini's display gate was the stated blocker and is now 5.4x cheaper than when it was measured"
  statement: "tinytama's POKEMINI-DISPLAY-GATE.md concluded that the panel, not the CPU, was what would stop Pokemon Mini shipping, on the measurement `11.42 us per 64-byte chunk` for a CO5300 SPI transfer -- 357 ns per pixel, making a 288x192 frame cost 19,741 us and capping it at 51 fps against a 72 Hz budget of 13,889 us. That transfer was issued in 64-byte chunks. The rebuilt path built for the NES port issues 16,384-pixel transactions and was measured on the watch at 66.455 ns per pixel, 5.4 times cheaper."
  consequence: "Recomputed against the same 72 Hz budget, 288x192 costs 3,675 us -- 26 percent of the frame -- and 1x and 2x cost 408 us and 1,633 us. With the core-1 blit already built, the push overlaps emulation, so the display is now the comfortable part of this platform rather than its risk. The three conclusions of the original gate still hold and are simply no longer needed to make the port viable: per-transfer fixed cost is zero, DMA caps the gain at about 2x, and dirty rectangles are unnecessary. This is the second time the same lesson has paid: a measurement records the path it was taken through, and re-using a per-pixel figure after changing the transfer size produces an answer that is wrong by the ratio of the chunk sizes."
  sources:
    - "tinytama docs/POKEMINI-DISPLAY-GATE.md, the 11.42 us per 64-byte chunk fit and its 19.7 ms conclusion"
    - "tinydesk-twatch ports/twatchultra/main/display.c: PIX_CHUNK 16384 and push_pixels_raw()"
    - "this project's NES measurement: 4,083 us of push for 61,440 pixels, recorded in NES-003"
  verification: "The 66.455 ns per pixel figure is a measurement on the watch (4,083 us push for 61,440 pixels at 51 fps, 2026-11-13). The 357 ns figure is a measurement on the watch from 2026-11-09. The recomputation is arithmetic from those two and is labelled as such: the 3,675 us figure for 288x192 has not been measured on this panel, only derived from a per-pixel cost that has. It should be confirmed by the first real frame the port pushes."

- record_id: PM-003
  kind: DISPLAY
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-13
  title: "The Pokemon Mini's frame tick is 71.97 Hz and a cartridge draws at that divided by 2 to 12, so 36 Hz is the fastest possible picture rate and 60 Hz does not exist"
  statement: "Three different rates are involved and only the first is fixed. The machine tick is the PRC counter's own period: the counter runs from 0x01000000 to 0x42000000, a span of 65 x 2^24, and advances by 2^24/855 per CPU cycle, so a frame is exactly 65 x 855 = 55,575 cycles, which at 4.00 MHz is 71.9748 Hz. PokeMini's function is named MinxPRC_On72HzRefresh(2), i.e. 72 is a rounded label on a 71.97 Hz event. A cartridge draws at that tick divided by an integer selected through PRC rate control at register 0x81: the rate match value is (divisor - 1) << 4, giving divisors 2, 3, 4, 6, 8, 9 and 12, and there is no divisor of 1. The fastest picture rate is therefore 71.97 / 2 = 35.99 Hz. Pokemon Pinball Mini selects divisor 2, measured on the watch as `prc rate reg 0x19, match 0x10 -> draws every 2 ticks = 36.0 Hz`."
  consequence: "There is no 60 Hz target to hit, and '60 fps' is not a meaningful goal for this platform. The correct target is 1.00x of the 71.97 Hz tick, which makes every drawn frame land on the cartridge's own divisor and makes a dithering game alternate at the rate it was authored for. Running slower than 1.00x lowers the dither rate proportionally, which on Pokemon Pinball Mini is audible as a beat: at 0.765x the game dithered at 27.5 Hz instead of 36, and an fps figure alone cannot express that fault. This also fixes the per-frame cycle budget: 55,575 cycles, not the 55,555 that 4e6/72 gives, and using the latter runs the machine 0.036 percent fast."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/pokemini/MinxPRC.c, MinxPRC_Sync(): counter reset to 0x01000000 and end-of-frame at >= 0x42000000"
    - "tinydesk-twatch ports/twatchultra/components/pokemini/MinxPRC.h: MINX_PRCTIMERINC defined as (16777216/855)"
    - "tinydesk-twatch ports/twatchultra/components/pokemini/MinxPRC.c, MinxPRC_WriteReg() case 0x81: rate match 0x20/0x50/0x80/0xB0/0x10/0x30/0x50/0x70 commented /3 /6 /9 /12 /2 /4 /6 /8"
    - "tinydesk-twatch ports/twatchultra/main/pm.c: PM_FRAME_CYCLES 55575 and the `pm status` PRC-rate report"
    - "https://en.wikipedia.org/wiki/Pok%C3%A9mon_Mini (infobox: 'refresh rate of 30 frames per second'), which conflicts with the above"
  verification: "The 71.9748 Hz figure is arithmetic from the core's own constants, cross-checked on the watch: `pm status` reported 71.9 fps against a tick the port now derives as 71.97 Hz, with the frame timer started from 55,575 cycles. The 36 Hz drawn rate is a live measurement read from the cartridge's own register after it set it. The Wikipedia figure of 30 fps is recorded as a conflict rather than adopted: it cites a 2001 IGN preview, not a datasheet, and matches neither the tick nor any legal divisor of it. The primary technical source, pokemon-mini.net/documentation/prc/, could not be read on 2026-11-13 -- Cloudflare returned 403 to every request and the Wayback Machine rate-limited with 429 -- so the discrepancy remains open and should be settled from that page or a datasheet when it is reachable."

- record_id: GBC-001
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-13
  title: "gnuboy beats binjgb by 1.6x and emits panel-order pixels, but a GBC costs more to emulate than a NES"
  statement: "Four commercial Game Boy Color ROMs were run headless through two cores on the same host, 1,500 frames per run, best of three because run-to-run noise on this machine measures 7 to 12 percent. gnuboy (retro-go fork, GPL-2.0, 4,996 lines) read 9,308 / 13,560 / 10,989 / 13,620 fps for Zelda Link's Awakening DX, Pokemon Crystal, Super Mario Bros. Deluxe and Tetris DX. binjgb (MIT, 10,477 lines) read 5,684 / 7,578 / 6,683 / 8,848 on the same titles, and those figures already include the RGBA to RGB565 conversion a port using it would have to write. gnuboy is 1.54 to 1.79 times faster and is consistently so across CGB-only (0xC0) and CGB-compatible (0x80) cartridges and three different mappers."
  consequence: "gnuboy also offers `GB_PIXEL_565_BE`, which is 16-bit colour with the high byte first -- exactly the byte order this panel's push expects -- so a gnuboy port needs NO per-pixel conversion, where the NES port spends 4,355 us of a 23,651 us frame on a 256-entry palette lookup. That is a port-level advantage independent of the speed ratio. Against that, the arithmetic that decides whether GBC is worth doing: the NES port's interpreter is 15,242 us of a 16,639 us frame, so the watch sustains about 1.95 Mcyc/s of 6502 cycles, while a GBC needs 4.194 Mcyc/s of Z80 cycles. Normalised by instructions (Z80 at about 4 cycles each, 6502 at about 2.5) that is 1.46x the work, or 22,253 us per frame; normalised by raw cycles it is 2.34x, or 35,666 us. The blit, by contrast, gets cheaper only if the scale stays at 1x: at 2x the 92,160 transmitted pixels are 1.5x the NES's 61,440, costing about 6,124 us against the NES's 10,272 us, a saving that the emulation increase more than cancels. Using the core-1 overlap already built, the frame is max(emu, blit), so the projection is 45 fps at the instruction normalisation and 28 fps at the cycle one -- somewhat SLOWER than the 51 fps the NES achieves today. GBC is worth doing for the picture, not the speed."
  sources:
    - "scripts/gbcbench.sh and tools/gbcbench/bench_gnuboy.c, bench_binjgb.c -- both cores buildable from pinned commits"
    - "tinydesk-twatch ports/twatchultra/components/gnuboy is not yet vendored; the upstream is ducalex/retro-go at 4ced120, retro-core/components/gnuboy"
    - "docs/gbc-scoping.md carries the full table and the arithmetic"
  verification: "Measured on the host 2026-11-13, headless, no audio, no sleeping. Every run passed the harness's own validity guard: the video callback fired for at least 99 percent of requested frames (gnuboy legitimately skips the hook while the LCD is off, which is one boot frame in 1,200 for Zelda) and the frame buffer was proved non-empty on frame 0. The harness was proved able to fail while being built: omitting gnuboy_reset() segfaults inside the CPU, running the diagnostic scan per frame masked the format difference entirely, and freeing the ROM buffer after emulator_delete() crashed after the numbers had already been computed. The frame-rate projection is arithmetic from measured watch numbers and is NOT a measurement; it is labelled as an estimate with a 28 to 45 fps range because two normalisations disagree by 1.6x."

- record_id: GBC-002
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-13
  title: "The Game Boy's 160x144 divides 320 exactly, which the NES cannot, and the fastest portable GB core does not support Color"
  statement: "The watch framebuffer is 320x416. A Game Boy's 160x144 at 2x is 320x288: it fills the full width with no side margins and 69.2 percent of the framebuffer, with 64 rows of letterbox above and below. 3x does not fit at 480x432. The NES at 256x240 reaches only 46.2 percent with 32-pixel side margins, and cannot do better at any integer scale because 256 x 1.25 = 320 and 1.25 is not an integer."
  consequence: "This is the whole reason to prefer GBC over the parked NES despite the GBC costing more to emulate: the operator's complaint was that 320x240 is too small for the glass, and GBC at 2x answers it exactly while the NES cannot. The second half of the record is a trap for the obvious plan. Peanut-GB is the fast portable Game Boy core -- it runs full speed on an RP2040 -- and its README states plainly that only the original DMG is supported, with CGB support described as preliminary work tracked in an issue rather than present in the tree. So the core with the best speed reputation is the wrong choice for a Color port, and the honest candidates are gnuboy, binjgb, SameBoy, Gearboy and gambatte."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.h: DISPLAY_W 320, DISPLAY_H 416"
    - "Peanut-GB README.md, the 'Only the original Game Boy (DMG) is supported at this time' paragraph"
    - "docs/gbc-scoping.md section 1 and 2 for the full table"
  verification: "The geometry is arithmetic and exact. The Peanut-GB limitation was read from its own README on 2026-11-13; it is not a measurement of this repository's code and is recorded as a property of that upstream project. The candidate licences and GBC support were read from each project's repository metadata and source: gnuboy has GB_HW_CGB and GB_PALETTE_CGB in gnuboy.h, binjgb has CGB_FLAG handling and emulator_get_frame_buffer returning RGBA."

- record_id: NES-001
  kind: EMU
  topic_id: NES
  status: VERIFIED
  verified_date: 2026-11-12
  title: "nofrendo is 1.96x InfoNES and needs no retro-go; the libretro cores need a whole frontend"
  statement: "Four NES cores were evaluated for the watch. Built and benchmarked headless on x86 with the render path live and the palette-to-RGB565 conversion counted, 600 frames each: nofrendo 394.85 / 392.64 / 828.21 Mcyc-emulated per second on Balloon Fight / Contra / Crisis Force, against InfoNES 201.61 / 201.56 / 422.38 -- a ratio of 1.96 on all three. FCEUmm (`libretro-fceumm`) and QuickNES (`libretro/QuickNES_Core`) could not be compiled at all: the first file of FCEUmm's core fails on `#include <libretro.h>`, and both are libretro cores that need a frontend shim before they produce a frame. Compatibility was measured on the operator's collection: 794 ROMs, 14 unsupported (98.2 percent), misses being mapper gaps such as Karnov and Holy Diver, with 90 percent of the collection on mappers 0-4."
  consequence: "The choice of nofrendo rests on two measured facts rather than on reputation, and it reversed an earlier recommendation that had favoured InfoNES for its Apache-2.0 licence -- once licensing was stated to be flexible and compatibility and performance were named as what matters, the 1.96x settled it. The porting cost is also settled by the same experiment: FCEUmm's 482 mapper boards buy nothing if half the collection is mappers 0-4 and the core cannot be built without a frontend, and nofrendo's 58 mapper files cover 98.2 percent. Crucially the core needs no retro-go despite living inside that tree: `nes/utils.h` has an `#ifdef RETRO_GO` branch and with the macro undefined logging becomes `printf`, `IRAM_ATTR` becomes empty and `CRC32` becomes `(0)` -- verified by grepping every `rg_` symbol in the whole component, where the remainder (`prg_rom_banks`, `prg_ram`) turned out to be struct fields and not retro-go calls. The component compiles on Xtensa at 120.4 KB .text and 5.3 KB .bss across 69 objects, and `port.c` turns the no-retro-go property into a build failure if `RETRO_GO` is ever defined."
  sources:
    - "Escapement scripts/nesbench.sh and tools/nesbench/bench_nofrendo.c: the benchmark, pinned to retro-go 4ced120"
    - "Escapement ~/twatch-backup/nesbench/RESULTS.md: the recorded runs and the three false passes"
    - "tinydesk-twatch ports/twatchultra/components/nofrendo/: the vendored core, unmodified"
    - "tinydesk-twatch ports/twatchultra/components/nofrendo/port.c: the RETRO_GO guard and endianness assertions"
    - "retro-go components/retro-core/components/nofrendo/nes/utils.h: the #ifdef RETRO_GO branch"
  verification: "Benchmarked on x86 2026-11-12, best of three runs per ROM. Both cores were checked for a non-zero whole frame buffer before their numbers were accepted -- nofrendo 62,865/65,280 pixels with ppu ctrl0=90 ctrl1=1E, InfoNES 60,095/61,440 with R0=90 R1=1E -- because three earlier harness versions reported plausible speeds for emulators that never drew a pixel. Compatibility was measured by running all 794 ROMs through the harness and counting the ones that produced no result. The ESP32-S3 build is verified: it compiles clean, the RETRO_GO guard was proved to fail the build when the macro is defined, and the firmware binary is unchanged at 0x28ca20 because nothing references the component yet. NOT verified: the watch's actual frame rate. The x86 ratio does not transfer, because a 6502 interpreter is pointer-chasing and branch-heavy and Xtensa differs from x86 exactly there; the strongest evidence for full speed on the watch is that retro-go, esp32-nesemu and esplay ship nofrendo on a 240 MHz ESP32."

- record_id: NES-002
  kind: EMU
  topic_id: NES
  status: VERIFIED
  verified_date: 2026-11-12
  title: "nofrendo runs on the watch at 21 fps, and the blit costs sixty times what moving 61 KB should"
  statement: "Measured on the board with the emulator running, over one second: 21 fps, with 8,160 microseconds per frame inside the 6502 interpreter and 6,166 inside the blit, against a 16,667 microsecond budget at 60 Hz. The display is correct -- 1x letterboxed, 256x240 centred in 320x416, pixel-perfect -- and the operator played a game with the margin buttons. The blit is the anomaly: it moves 61,440 pixels and takes 6.1 ms, where a memcpy of 61 KB should be about 100 microseconds. The cause is architectural rather than a defect: `display_picture_rgb()` copies the frame into the PSRAM framebuffer `s_fb`, and `flush_rect()` then reads `s_fb`, byte-swaps into the internal `s_tx` buffer and pushes over QSPI. With the source also in PSRAM that is four passes over external memory per frame where one would do."
  consequence: "The emulator half is NOT the first thing to fix, which is the opposite of the intuitive reading of 'the NES is slow'. The blit is 37 percent of the frame for work that should be 0.6 percent, so a full-frame path that converts the palette indices straight into `s_tx` in flush order -- skipping `s_fb` and the intermediate converted row -- removes most of 6,166 microseconds before any work is done on the interpreter. That path also serves the Tamagotchi, which blits rows through the same function. The emulator's 8,160 microseconds is a separate problem: the host runs the same frame in 41 microseconds, so the S3 is roughly 200x slower on this workload, and the likely causes are the PPU writing 65 KB per frame into PSRAM and the ROM sitting in internal DRAM. Worth noting that neither figure could have been inferred from the host benchmark, which is why the split is reported by `nes status` rather than a single frame rate."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/nes.c: the emu/blit timers and the `nes status` line"
    - "tinydesk-twatch ports/twatchultra/main/display.c: display_picture_rgb() and flush_rect()"
    - "on the watch 2026-11-12: emu=1036293us blit=783085us over 127 frames in one second"
    - "NES-001 for the host benchmark these figures must not be confused with"
  verification: "Verified on hardware 2026-11-12. The operator launched /sd/game.nes (Balloon Fight), the picture appeared correctly, and they played it with the margin buttons -- so display, input and emulation are all confirmed correct. Throughput was measured by `nes status` after six seconds of running. NOT verified: any improvement, since no optimisation has been attempted yet. The host benchmark is unchanged at 142x realtime on the same ROM, which is host load rather than a regression."

- record_id: BLE-008
  kind: BLE
  topic_id: BLE
  status: VERIFIED
  verified_date: 2026-11-15
  title: "NimBLE runs central and peripheral together; the console service must register before the host starts, and its name must be in the advertisement"
  statement: "The ESP32-S3 under NimBLE supports the central and peripheral roles simultaneously, and this board uses both: as a CENTRAL it connects out to a BLE keyboard (bt_hid.c, BLE-003/BLE-007), and as a PERIPHERAL it now offers a serial console in the Nordic UART shape -- service 6E400001-B5A3-F393-E0A9-E50E24DCCA9E with a write characteristic (6E400002) in and a notify characteristic (6E400003) out, which is what off-the-shelf BLE serial clients already speak. THREE ORDERING FACTS MAKE OR BREAK IT, and each cost a boot on this board. NimBLE's service initialisers require sysinit to be active: ble_svc_gap_init() opens with SYSINIT_ASSERT_ACTIVE() and then SYSINIT_PANIC_ASSERT(rc == 0), so calling it from the host-sync callback panics with 'assert failed: ble_svc_gap_init ble_svc_gap.c:333'; registration belongs BETWEEN nimble_port_init() and nimble_port_freertos_init(), which is the only window that satisfies the assertion. Advertising, by contrast, cannot precede the sync, because the address is not known until then -- ble_hs_id_infer_auto() fails before it. And ble_svc_gap_device_name_set() sets the GAP characteristic's VALUE, which a client can read only AFTER connecting; it puts nothing in the advertising payload, so a scan filtered by name finds nothing at all. The name has to be set in struct ble_hs_adv_fields (name/name_len/name_is_complete) before ble_gap_adv_start()."
  consequence: "This is the only console path on this board that does not collide with the work it is meant to observe, and that is why it exists. The USB port is the console AND the OTG host and cannot be both: the USB-Serial-JTAG peripheral resets the chip when a device talks on the lines the host controller is driving -- esp_reset_reason() reports ESP_RST_USB, which maps from RESET_REASON_CORE_USB_UART/CORE_USB_JTAG, and that is a reset with no panic and therefore no coredump. WiFi is not a substitute because it does not fit: with the USB host stack linked in, internal RAM measured 41 KB free against the 65,536 bytes the SSH server refuses to start below. The BLE radio runs on core 0, on its own pins, and is already up for the keyboard. The transport plugs in as the port's ALTERNATE serial-2 stream, so the shared display layer needed three weak board hooks and a board key (display2.transport = ble) and learned nothing about Bluetooth. BLE-002's mbuf lesson applies to the notify path: one notification is kept in flight at a time, released by BLE_GAP_EVENT_NOTIFY_TX, so an unbounded stream cannot exhaust the pool and drop the link with HCI 0x16."
  sources:
    - "tinydesk-twatch port: main/bleuart.c (service, registration/start split, tx pacing), ports/esp_idf/app/bt_hid.c (board_ble_uart_register/on_sync, weak defaults), ports/esp_idf/app/hal_mux.c (serial-2 alternate transport)"
    - "ESP-IDF 5.3.1 examples/bluetooth/nimble/bleprph/main/gatt_svr.c: ble_svc_gap_init(), ble_svc_gatt_init(), ble_gatts_count_cfg(), ble_gatts_add_svcs() all called before nimble_port_freertos_init"
    - "components/bt/host/nimble/nimble/nimble/host/services/gap/src/ble_svc_gap.c: SYSINIT_ASSERT_ACTIVE() at ble_svc_gap_init(), line 325"
    - "on the watch: esp_reset_reason() reporting `usb (code 11)`, and heap_caps_get_free_size(MALLOC_CAP_INTERNAL) at 41 KB with the host stack linked"
  verification: "Verified on hardware 2026-11-15, end to end and from the PC: bluetoothctl scanned the watch as `TinyDesk` at 80:45:6B:46:DF:56, a bleak client connected, found both characteristics, and the desktop's terminal stream arrived as notifications -- 2,945 bytes of the start menu with its colours and box-drawing characters -- and `uptime` typed over the write characteristic came back '0 days 00:01:05'. The two ordering failures were both caught by coredump rather than by reasoning: 'assert failed: ble_svc_gap_init ble_svc_gap.c:333' with frames pointing at bleuart_start from on_sync, and before that an advertisement that ran correctly (started=1, tx_handle=18) and was still invisible to every scan-by-name. Revised 2026-11-15 (core-log entry 14): new record."


- record_id: SOC-006
  kind: SOC
  topic_id: SOC
  status: VERIFIED
  verified_date: 2026-11-12
  title: "SSH needs 65,536 bytes free to start and then takes 48,456 of them, so the admission floor is checked against a heap it is about to shrink"
  statement: "tdsh's SSH server allocates a private 65,536-byte heap in PSRAM (`SSH private free=0/65536` at start), which does not touch internal DRAM. Its TASK stack and per-connection buffers do come from internal DRAM, and they are not accounted for by the admission check, because that check runs before the task exists."
  consequence: "Four states measured back to back on one boot, each read twice and stable: fresh boot with no SSH server, 102,615 bytes free; SSH server running with no client connected, 82,379; SSH server with one session connected, 53,319; and the low-water mark while that session was active, 40,899. So the server costs 19,396 bytes for existing and a further 29,060 while a client is attached -- 48,456 in total, which is 74 percent of the 65,536 it demands before it will start. This explains the historical failures recorded in EMU-018 without contradicting them: a build with 62,443 bytes free was refused, correctly, but a build that passed the check by a few kilobytes was still only a session away from the floor. It also sets the shape of any future measurement -- 'heap' with no SSH, with SSH idle, and with a session attached are three different numbers, and quoting one as if it were the others is how the same board appears to have gained or lost 50 KB between two readings a minute apart. Quote the state alongside the number or the number means nothing."
  sources:
    - "third_party/tdsh/ports/esp_idf/components/tdsh/src/tdsh_ssh.c: the admission check and the 'before SSH server task creation' log line"
    - "watch: `heap` and `ssh start|stop|status` over the USB terminal, 2026-11-12"
  verification: "Measured on hardware 2026-11-12 on one boot, so no confound from uptime or prior allocations. WiFi up and SSH stopped: free=101775, largest=25600. After `ssh start`, no client: free=82379, largest=25600, reproduced three times identically. With one SSH session attached: free=53319, largest=31744, minimum-free=40899. After `ssh stop`: back to free=101775, so the 19,396 bytes are returned cleanly rather than leaked. A fresh boot with neither server nor connection read free=102615, the 840-byte difference from 101775 being the associated WiFi station's own small allocation. The panel is write-only, so nothing here is a claim about the glass."

- record_id: SOC-005
  kind: SOC
  topic_id: SOC
  status: VERIFIED
  verified_date: 2026-11-12
  title: "Wi-Fi's static buffers are 51 KB of internal DRAM that PSRAM cannot hold, and halving them returned 30 KB"
  statement: "`CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM` and `..._STATIC_TX_BUFFER_NUM` size two pools of 1600-byte buffers that the ESP32-S3 Wi-Fi driver allocates from internal DMA-capable RAM. They are taken at `esp_wifi_init()` and kept for the life of the firmware -- joining or leaving a network does not change them, and `network mode lan` only calls `esp_wifi_stop()`, so measuring the heap with Wi-Fi stopped shows no delta at all and is a misleading experiment. The IDF default pair, 16 + 16, is 51,200 bytes. This port had exactly that, against an idle internal heap of 72,475 bytes and a low-water mark of 35,211 -- below the 65,536 bytes tdsh's SSH server requires before it will create its task, so SSH and any large allocation were competing for the same thin margin."
  consequence: "Setting the pair to 8 + 8, with `RX_BA_WIN` following down to 8 because it must stay at or under 2 x `STATIC_RX_BUFFER_NUM`, and the dynamic pools (`DYNAMIC_RX_BUFFER_NUM` 32->16, `CACHE_TX_BUFFER_NUM` 32->16) halved with them, moved free internal from 72,475 to 102,635 bytes (+30,160) and the low-water mark from 35,211 to 95,955. The saving is larger than the 25,600 bytes the two static pools account for because the dynamic pools contribute their own internal allocation at init. Cost: measured, not assumed -- SFTP with the smaller buffers ran 17.9 KiB/s against 17.2 KiB/s before, md5 matching, and Wi-Fi re-associated on its own after a reboot with three bars. Eight 1600-byte buffers are still far more than a 17 KiB/s transfer can keep in flight. The same lesson as EMU-018 generalises: on this board INTERNAL DRAM is the scarce resource, and it is worth asking of every buffer which pool it is drawn from before asking whether it is needed at all. Disconnecting from the network frees none of it: with WiFi connected and with it disconnected the heap read 101,775 both times, because the buffers belong to the driver's initialisation and not to the connection -- and that is the test that decides when they are paid at all. A boot on which the driver is never initialised does not pay them: with `network autowifi off` set, so that nothing attempts a connect and `esp_wifi_init()` is never called, `heap` reads 143,275 with a 49,152 largest block, stable across repeats, against 101,775 on the same firmware with the driver initialised and the station disconnected. The 41,500-byte gap is the whole driver's initialised footprint, not just the two pools, so a session that does not need the network is worth more to internal DRAM than any tuning of the buffers inside it. Note what does NOT follow -- `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` must not be lowered to chase the same win, because `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y` means a lowered threshold moves task stacks into PSRAM, where a task that runs during a flash write dies when the cache is disabled."
  sources:
    - "ports/twatchultra/sdkconfig.defaults: the buffer block with this reasoning recorded beside it"
    - "ports/twatchultra/sdkconfig: CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=8, STATIC_TX=8, RX_BA_WIN=8, DYNAMIC_RX=16, CACHE_TX=16"
    - "third_party/tdsh/ports/esp_idf/components/tdsh/src/tdsh_wifi.c:324 esp_wifi_init, :824 esp_wifi_stop, :848 esp_wifi_deinit (only the deinit path releases buffers)"
  verification: "Revised 2026-11-12 (core-log entry 59): added the never-initialised state -- `autowifi off` with no connect attempt reads 143275/49152 stable, against 101775 with the driver initialised and disconnected, so the buffers are paid at init and a boot that never initialises the driver keeps 41500 bytes more than one that disconnects. Measured on hardware 2026-11-12. `heap` before: free=72475, largest=31744, minimum-free=35211. After the rebuild and flash: free=102635, largest=31744, minimum-free=95955. tdsh's own SSH log line then read `before SSH server task creation: internal free=102635 largest=31744 | SSH private free=0/65536`, so the server starts with room instead of at the edge. Wi-Fi came back by itself after the flash (ping 10.10.0.15 OK, `network status` Online: yes) and a 131,072-byte SFTP put at --chunk 4096 completed in 7.15 s (17.9 KiB/s) with md5 c5d6e009fd9c9bd89b6dea07d00a3226 matching both ends. Bluetooth unaffected: `bt status` shows the radio up, 40,980 bytes taken, both stored pairings present."

- record_id: BLE-007
  kind: BLE
  topic_id: BLE
  status: VERIFIED
  verified_date: 2026-11-12
  title: "An Xbox Wireless Controller is BLE HID here, and a gamepad is only distinguishable from a mouse by its top-level collection"
  statement: "The operator's Xbox Wireless Controller (model EP2-29929) connects to this board as a Bluetooth LE HID-over-GATT peripheral and delivers input reports; it paired first try once put in pairing mode. This contradicted an earlier assertion in this project that it was Bluetooth Classic BR/EDR -- the ESP32-S3 has no Classic radio at all, so that reading implied it could not connect. Its report descriptor is 283 bytes and decodes as: report ID 1, 16 bytes, left stick at bits 0 and 16 (16-bit each), triggers at 32, right stick at 64 and 80 (10-bit each), hat switch at 96 (4 bits), fifteen buttons at 104, and a consumer control at 120. The hat reports 1..8 clockwise from up with 0 (or 15) centred, which is NOT the 0-based range a desk-mouse-style descriptor uses."
  consequence: "Two things generalise beyond this controller. First, `hid_mouse_parse()` ACCEPTS a gamepad's descriptor: a gamepad declares Generic Desktop X and Y for its left stick, and X and Y are the only usages the mouse parser looks for. The controller was therefore claimed as a mouse and its stick moved the desktop cursor. The discriminator is the TOP-LEVEL COLLECTION -- Generic Desktop / Game Pad 0x05, or / Joystick 0x04, against / Mouse 0x02 and / Keyboard 0x06 -- and the gamepad parser must be asked BEFORE the mouse, because once the mouse has claimed a device the gamepad path is never reached. The ordering is the fix, not a detail. Second, and this is the more expensive lesson: an assertion about a device's protocol from its model generation was wrong, and only worked out because the operator said 'it is in pairing mode, try it'. Hardware claims from product families are not measurements."
  sources:
    - "tinydesk-twatch ports/esp_idf/app/hid_gamepad.c: the collection-usage discriminator and the field extraction"
    - "tinydesk-twatch ports/esp_idf/app/bt_hid.c: the classification order in ready()"
    - "tinydesk-twatch tests/xbox_btle.h: the 283-byte descriptor and ten raw reports captured from the device"
    - "tinydesk-twatch tests/test_gamepad.c: 27 checks against that real data"
    - "ESP-IDF 5.3.1 components/soc/esp32s3/.../Kconfig.soc_caps.in: no SOC_BT_CLASSIC_SUPPORTED on the S3"
  verification: "Verified on hardware 2026-11-12. The descriptor and reports were captured from the controller onto the SD card and read back over SFTP. With the gamepad parser asked first, `bt status` reports the device as `kbd=0 mouse=0 pad=1` and the mouse slot stays idle, where it previously showed `last: connected: mouse`. A 30-second sample decoded the hat as up/left/right and button bits 10 and 11 correctly, which proves the button decode path. NOT verified: the four face buttons' bit assignment, which rests on the press order recorded during capture -- they did not land inside the sampling window, and the mapping is one table edit away if a game disagrees."

- record_id: NFC-001
  kind: BOARD
  topic_id: NFC
  title: "The NFC reader is an ST25R3916 on the shared SPI bus, powered by DLDO1, and this unit is base silicon rev 2"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The reader on this board is an ST Microelectronics ST25R3916-family RF front end, not the PN532 that a T-Watch backplate once carried, and it is not on I2C at all. It sits on the second general-purpose SPI bus that the SD card and the LoRa radio share (SCK 35, MOSI 34, MISO 33) with chip select on GPIO 4 and interrupt on GPIO 5, and its supply is the AXP2101's DLDO1 -- voltage register 0x99 in 100 mV steps from 500 mV, enable bit 7 of 0x90. On this unit the identity register 0x3F reads 0x2A: the type field in bits 7:3 is 5, which is the ST25R3916 and not the 3916B that reads 6, with revision 2 in bits 2:0. Both readings are stable across separate transactions."
  consequence: "Three things follow, and each replaces a guess this project was carrying. The part is not the PN532 its earlier records hinted at, so no PN532 driver applies. The rail is DLDO1 and not ALDO1..4 or BLDO2, and it is the THIRD part on this board whose failure mode is to answer and do nothing with its supply off -- the haptic driver and the speaker amplifier are the other two -- so a reader that reads correctly and never sees a card is a rail fault before it is a protocol fault. And the two silicon types are not interchangeable at the analog layer: they carry different antenna tuning and different AWS settings in the vendor's configuration table, so a driver written against 0x30 would read this part's identity and still fail on a tag. The rail must also be raised before the first transaction, because DLDO1 is a switched regulator and not a tie to VBAT."
  sources:
    - "on the watch, TinyDesk port 2026-11-12: `nfc` reports `rail up`, `bus up cs 4 int 5`, `id 2a type ST25R3916 rev 2`, `io2 04`, `op 00`, `irq 0`"
    - "BRD-003 for the rail assignment, and BRD-004 for the shared SPI bus and its chip selects"
    - "LilyGoLib docs/hardware/lilygo-t-watch-ultra.md: the ST25R3916 identification and the CS/INT pin table"
    - "arduino-esp32 variants/lilygo_twatch_ultra/pins_arduino.h: NFC_CS 4, NFC_INT 5, and USING_ST25R3916"
    - "tinydesk-twatch port: main/nfc.c board_nfc_rail_on() and nfc_bus()"
  verification: "Verified on hardware 2026-11-12 in the TinyDesk port, on the glass: the `nfc` command printed the identity above and the SD card mounted at 119 GB and read a directory in the same session with the reader added to its bus, which is the coexistence evidence. The earlier 'absent' verdict this project carried predates the chip-select parking fix and is superseded by BRD-004."

- record_id: NFC-002
  kind: BOARD
  topic_id: NFC
  title: "The ST25R3916 speaks one-byte-command SPI mode 1, and its chip select must stay a plain GPIO"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The reader's host interface is SPI mode 1 (CPOL 0, CPHA 1), MSB first, up to 5 MHz, with a one-byte command that encodes the operation in its top bits: a register write sends `(reg & 0x3F)` followed by the data, a register read sends `((reg & 0x3F) | 0x40)` followed by one dummy byte and then returns the value on the second byte, a FIFO load sends 0x80 then data, a FIFO read sends 0xBF then collects data, and a direct command sends `0xC0 | cmd`. Registers in space B require 0xFB first. The vendor stack and this port both drive chip select by hand -- `digitalWrite(cs, LOW)` around the command byte and its data -- rather than handing the pin to the SPI peripheral."
  consequence: "The two-byte read and the hand-driven chip select are not stylistic and both cost a cycle if assumed otherwise. The chip cannot drive MISO while it is still receiving the command byte, so the first byte clocked back during a read is meaningless and the value is the second; a driver that takes the first byte reads a register's own address back and looks plausible. And a pin handed to the SPI driver as `spics_io_num` is routed to a peripheral output through the GPIO matrix, where `gpio_set_level()` has no effect, so the by-hand toggling the protocol needs silently stops working -- the symptom is a chip that never answers, indistinguishable from a dead part or a wrong mode. Read the identity register twice across separate transactions before believing it: 0x00 and 0xFF are what a floating MISO produces, so a single read cannot separate a missing chip from a missing line. This bus has MISO shared with the SD card and the LoRa radio, which is why that distinction matters here more than on a dedicated bus."
  sources:
    - "ST, ST25R3916 datasheet, and the SPI framing as implemented in ST's own driver, st25r3916_com.cpp"
    - "on the watch, TinyDesk port 2026-11-12: the same register read twice returned 0x2a both times and `io2 04` read back the byte that had been written"
    - "tinydesk-twatch port: main/nfc.c, the xfer()/nfc_read_reg()/nfc_write_reg() pair and the note on spics_io_num"
  verification: "Verified on hardware 2026-11-12: the identity read is stable, a write to IO_CONF2 0x01 reads back as written, and the SD card keeps mounting on the same bus with the reader's device added, which is what shows the mode and the hand-driven chip select are both right rather than merely plausible."

- record_id: NFC-003
  kind: EXTERNAL
  topic_id: NFC
  title: "The vendor NFC stack is 40k lines of Arduino code that does not link until its interface class is made abstract"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The portable NFC stack for this part is lewisxhe/ST25R3916-NFC-RFAL v1.0.1, which merges ST's ST25R3916 register driver, ST's RFAL abstraction layer and an NDEF reader/writer: about 40 700 lines across 33 .cpp and 35 .h files, carrying ST's SLA0052 / MIX MyLiberty licence rather than a permissive one. Its Arduino dependency is thin -- three `#include <Arduino.h>`, 25 `dev_spi->transfer` calls, 23 `digitalWrite`, 14 `delay`, 6 `millis`, and MIN/MAX -- so it can be built for ESP-IDF behind three small headers. Two defects must be handled before it runs: `RfalRfClass` in rfal_rf.h declares fifty virtual functions and defines none of them, so the link fails with an undefined reference to its vtable under any compiler that emits one, and the fix is to mark all fifty `= 0`; and `attachInterrupt()` can be a no-op, because the stack's fallback is `isr_pending || digitalRead(int_pin) == HIGH` and reads the line on every worker call regardless."
  consequence: "Porting the vendor stack rather than writing a driver is the cheaper and more honest path, but only because the coupling is this thin: the parts that are hard to get right and impossible to guess -- the analog configuration table with its per-silicon antenna tuning, and the ISO14443A short-frame and anticollision sequence -- are all inside the vendored code, and the parts this port has to supply are types and GPIO. Marking the interface pure is better than the link fix it replaces, because it converts a mangled-symbol failure at link time into a named-function failure at compile time, which is what a future re-vendor needs. The interrupt being unnecessary is worth keeping in view: it means the SPI bus is never touched from interrupt context, and the worker loop's 1 kHz rate is far inside the 106 kbit/s framing. Cost measured: 55 KB of flash on this port, against 517 KB free in its OTA slot, so the whole stack fits with room for the app work still to come. The stack must not be re-vendored without re-checking the base-class edit, because it is the one place this project's copy differs from upstream."
  sources:
    - "https://github.com/lewisxhe/ST25R3916-NFC-RFAL tag v1.0.1, vendored at tinydesk-twatch ports/twatchultra/components/st25r3916/rfal/"
    - "rfal_rf.h: RfalRfClass, the fifty virtual declarations, and the `= 0` edit this project made"
    - "rfal_rfst25r3916.cpp: isISRPending(), the digitalRead fallback that makes the interrupt optional"
    - "tinydesk-twatch port: components/st25r3916/include/{Arduino.h,SPI.h,Wire.h} and shim/arduino_shim.cpp"
  verification: "Verified on hardware 2026-11-12: the port links with the stack vendored, boots, and reads a real tag end to end (`card A uid 1dd30d9e1c1080`, `sak 00 T2T ndef 19 bytes`, decoded as the text record 'Hello LilyGo'). Build evidence: tinydesk.bin grew from 2,570,352 to 2,628,560 bytes, 55 KB, with no change to the panel, the SD card or the shell."

- record_id: NFC-004
  kind: BOARD
  topic_id: NFC
  title: "There is no card-presence circuit on this board, so the reader must be powered and polling to see a tag"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "LilyGo's own documentation for this board states that the ST25R3916 has no external capacitive card-presence detection circuit and that the reader must be powered to detect a card. Consistent with that, a scan that powers the rail, polls NFC-A/B/F/V in one-second discovery windows and holds the field on reads whatever is on the antenna: on 2026-11-12 a Type 2 tag held against the glass reported as `card A uid 1dd30d9e1c1080`, `sak 00 T2T ndef 19 bytes`, with the NDEF message `d1 01 0f 54 02 65 6e` followed by 'Hello LilyGo' -- a well-known Text record, 15 bytes of payload, language 'en'. The tag in hand is therefore NOT blank despite being described as one; it carries a factory-written text record, which is why the read produced a message rather than an empty answer."
  consequence: "The consequence for power is direct: there is no way to wait for a card cheaply, so detection means running the radio, and a reader left polling holds tens of milliamps of antenna current for as long as it runs. That is why this project scans on demand rather than in a background task, and why the field is dropped when a scan ends. The consequence for reading is that 'a tag is present' and 'a tag has content' are separate facts and both are worth printing: a Type 2 tag with no NDEF reports zero-length and is not an error, while the tag actually tested returned content, so the no-NDEF path remains written and UNEXERCISED. The record also retires the 'blank tag' premise this cycle started from -- a tag's UID, SAK and NDEF are three independent answers and only reading all three says what is on it."
  sources:
    - "LilyGoLib docs/hardware/lilygo-t-watch-ultra.md: 'The ST25R3916 NFC reader has no external capacitive card-presence detection circuit on this board. The reader must be powered to detect a card.'"
    - "on the watch, TinyDesk port 2026-11-12: `nfc scan 10 > /sd/nfc.txt` then `cat /sd/nfc.txt` gave the card line, the decoded record, the hex and `1 card`"
    - "tinydesk-twatch port: main/nfc_scan.cpp nfc_scan_step() and main/nfc.c print_ndef_text()/print_ndef_hex()"
  verification: "Verified on hardware 2026-11-12 with the operator holding the tag against the glass. Two independent scans four minutes apart returned the same UID, and the second run's output was written to /sd and read back, so the evidence is a durable record rather than a scrollback. Not yet exercised: a tag with no NDEF, any technology other than NFC-A Type 2, and writing."

- record_id: GPS-001
  kind: BOARD
  topic_id: GPS
  title: "The GNSS receiver is a u-blox MIA-M10Q on UART1, powered by BLDO1 one register below the speaker's rail"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The T-Watch Ultra carries a u-blox MIA-M10Q multi-constellation receiver on UART1 at 38400 baud, 8 data bits, no parity, one stop bit. From the SoC's point of view the pins are TX 43, RX 44 and PPS 13 -- which is the opposite of the module's, since the schematic labels GPS_TX for the line the module receives on. Its supply is the AXP2101's BLDO1: voltage register 0x96 in 100 mV steps from 500 mV, and enable bit 4 of 0x90. Nothing in this project raised that rail before 2026-11-12."
  consequence: "BLDO1 sits immediately below the speaker's BLDO2 in BOTH registers -- bit 4 versus bit 5 of 0x90, and 0x96 versus 0x97 for the voltage -- and the consequence of writing the wrong one is entirely silent: the amplifier comes up, the receiver stays dark, no register reports a fault, and the only symptom is an absence of satellites. That is the same silent-stub shape as the haptic driver with its expander bit low and the SD card with ALDO1 low, and it is why the rail is written read-modify-write and then read back rather than trusted. The module also needs roughly 300 ms after its rail before it will answer, because its own oscillator has to start and its firmware to boot; probing sooner returns silence that is indistinguishable from a wiring fault. And because the pin names are the SoC's, a harness with TX and RX crossed is silent in both directions and looks exactly like a dead module -- which is why a probe here must report how many BYTES arrived and not only whether a reply parsed."
  sources:
    - "BRD-003 for the BLDO1 assignment, and the AXP2101 register map this project carries in patches/kernel/axp2101"
    - "arduino-esp32 variants/lilygo_twatch_ultra/pins_arduino.h: GPS_TX 43, GPS_RX 44, GPS_PPS 13"
    - "LilyGoLib docs/hardware/lilygo-t-watch-ultra.md: MIA-M10Q and the same pin table"
    - "tinydesk-twatch port: main/power.c board_gnss_rail_on() and main/gps.c gps_open()"
  verification: "Verified on hardware 2026-11-12 in the TinyDesk port: `gps` reported `rail up` and `port 38400 8N1`, and 624 to 900 bytes arrived from the module in the first second, so both the rail and the pin direction are correct rather than merely plausible."

- record_id: GPS-002
  kind: BOARD
  topic_id: GPS
  title: "Acquiring UTC proves the antenna works, and is better evidence than a fix"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "With the rail up, the receiver emits GGA, RMC, GSA, GSV, GLL and VTG at 1 Hz: `gps raw 12` counted GGA 12, RMC 12, GSA 48, GSV 54, GLL 12, VTG 12 over twelve seconds with zero unclassified sentences, and a 60-second monitor parsed 120 sentences with zero checksum failures. During that monitor the receiver began reporting a real UTC time -- `20:28:41` -- together with s03 (three satellites used) and HDOP 2.38, while never reporting a fix. A GSV count reported 5 satellites visible."
  consequence: "A GNSS receiver cannot report UTC until it has decoded ephemeris from the satellites themselves, which takes roughly thirty seconds of continuous reception at usable signal strength. That makes an acquired clock a stronger statement about the antenna than a fix is: noise, a crossed pin and a parser bug cannot produce it. The two readings also separate three different faults that look identical on a fix display -- no antenna shows no satellites at all, an antenna with sky but insufficient geometry shows satellites and no fix, and a working receiver shows a fix -- and only the satellite count distinguishes the first from the second. HDOP is worth reading alongside it because the receiver reports 99.99 when it has nothing, so a plausible HDOP with zero satellites in the fix means the solution is being computed from a real though insufficient set. Three satellites is one short of the four a 3D fix requires, which is exactly what a windowless room supplies and what a fixed location cannot be blamed for."
  sources:
    - "on the watch, TinyDesk port 2026-11-12: `gps raw 12` and `gps monitor 60`, the latter written to /sd and read back"
    - "tinydesk-twatch port: main/nmea.c for the GGA/RMC decode and main/gps.c for the raw sentence counter"
    - "GPS-001 for the rail these readings depend on"
  verification: "Verified on hardware 2026-11-12, indoors, on a desk: the counts and the acquired UTC above were read off the panel and out of a file written to the SD card. NOT verified: a fix, which needs sky view, and the C/N0 of the satellites being tracked, which the port does not yet extract from GSV."

- record_id: GPS-003
  kind: EXTERNAL
  topic_id: GPS
  title: "The receiver answers NMEA but not the UBX-MON-VER poll, so it never names itself"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The module speaks NMEA continuously on its UART, and this project's probe sends a single UBX-MON-VER request (`B5 62 0A 04 00 00 0E 34`) expecting a reply carrying software version, hardware version and an extension list in which a u-blox module puts its own model as `OD=MIA-M10Q`. On this unit no such reply arrives within a one-second window, in two separate runs, while 567 and 624 bytes of NMEA arrived in the same windows. The UBX input side may be disabled in the module's configuration, or the reply may be missed by the framing, and neither was determined."
  consequence: "This matters for how much weight the model name can carry. The board's documentation and its Arduino variant both say MIA-M10Q, and the receiver's behaviour is consistent with that, but the module has not confirmed it in its own words and this project's records should not pretend otherwise. It also limits what can be configured later: every u-blox configuration command is UBX, so anything beyond reading NMEA -- changing the sentence rate, enabling or disabling constellations, setting a fixed mode -- depends on solving this first. The untried next steps, in order of cost, are enabling UBX input with UBX-CFG-PRT, polling again after the receiver has a fix (some modules answer only once they are running normally), and checking the reply framing against a longer window."
  sources:
    - "on the watch, TinyDesk port 2026-11-12: two runs of `gps` reporting `ubx no reply` with `heard 567 bytes` and `heard 624 bytes`"
    - "tinydesk-twatch port: main/gps.c, the MON_VER request and the Fletcher checksum in ubx_checksum_ok()"
    - "LilyGoLib examples/peripheral/GPSFullExample and src/gps/GPS.cpp checkUblox(), which sends the same request and reads the same OD= extension"
  verification: "Verified as a negative on hardware 2026-11-12: NMEA demonstrably flows (GPS-002) while the MON-VER reply does not arrive, which rules out a dead or unpowered module and leaves the UBX input path as the open question. Recorded as unexplained rather than explained away."

- record_id: GPS-004
  kind: BOARD
  topic_id: GPS
  title: "Indoors the receiver tracks a handful of satellites without a fix, and that is the bench's limit"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "Over a 60-second monitor on a desk with no window, the receiver reported three satellites used, a real HDOP of 2.38 that moved no more than 0.01 over the minute, an acquired UTC time, and no fix. An earlier twelve-second window showed five satellites visible in GSV. Fix quality stayed 0 throughout."
  consequence: "Three satellites is one short of the four a three-dimensional fix needs, which makes this a geometry limit and not a defect -- and it is worth stating so that a later cycle does not treat it as a bug to hunt. Two things follow for method. First, a fix needs sky view and is the only part of this work the bench cannot supply, so it has to be tested near a window or outdoors. Second, the port currently reads the satellite COUNT but not the carrier-to-noise ratio of each satellite, so it can say how many are heard and not how well; extracting C/N0 from GSV is the measurement that would say whether a marginal fix is a placement problem or a receiver one."
  sources:
    - "on the watch, TinyDesk port 2026-11-12: `gps monitor 60` written to /sd/m60.txt and read back"
    - "GPS-002 for the same run's UTC acquisition and sentence counts"
  verification: "Verified on hardware 2026-11-12, indoors. The no-fix result is a measurement of the location rather than of the receiver, and is recorded as such: the same code with the same antenna is expected to fix outdoors, which has not been tried."

- record_id: GPS-005
  kind: BOARD
  topic_id: GPS
  title: "GPS time needs no fix and reaches the watch clock through the RTC, so it survives a power cycle"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The receiver reports a valid UTC time once it has decoded ephemeris, which happens at three satellites and with no fix -- so a watch held up indoors for a few seconds can be given an accurate clock with no network. The port applies it with `board_time_apply()`, which is settimeofday followed by `board_time_set_post()`, the same two steps the shared app takes for SNTP and for Date & time; the second step writes the PCF85063A. Verified on the board: three corrections thirty seconds apart each reported the clock `was off +0 s`, `+1 s`, `+0 s`; the write logged `twatch-rtc: wrote 2026-11-12 20:34:54 UTC` and the epoch reported decoded back to that exact instant; and after a hard reset the shell read 13:46:16 MST against a host at 13:46:28."
  consequence: "Two design points follow and both are about where the time is applied rather than how it is computed. First, settimeofday alone is not enough on this board: it would leave the chip holding the previous value and the next boot, which seeds from the chip, would undo the correction -- so any future time source here must write through, and reusing the existing `board_time_set_post` hook is what keeps GPS, SNTP and the Date & time dialog from drifting apart. Second, THE TIME MUST BE APPLIED THE MOMENT IT IS VALID. A version that collected the UTC from the first good sentence and then waited out its listening window before setting the clock produced correction runs reporting `was off +26 s` and then `+1 s` a minute apart, which reads as a clock gaining twenty-five seconds in eighty and was entirely the stale value being applied late. An hour of apparent hardware drift was code. The way it was separated from real drift is worth reusing: the clock ran exactly 60 s across a `sleep 60` and 41 s across a 40 s monitor, which cleared the hardware, and a GPS-referenced delta before and after the apply is a cleaner instrument than comparing against a host whose command latency is unknown."
  sources:
    - "on the watch, TinyDesk port 2026-11-12: `gps time 20` three times in /sd/r1..r3.txt, and `date` after a hard reset in /sd/p.txt"
    - "tinydesk-twatch port: main/gps.c cmd_gps() settime branch, main/rtc.c board_time_apply(), main/nmea.c nmea_utc()/nmea_utc_raw()"
    - "ports/esp_idf/app/main.c time_set(), the sequence this mirrors for SNTP"
    - "Epoch cross-checked outside the firmware: 1794515694 decodes to 2026-11-12T20:34:54Z in Python"
  verification: "Verified on hardware 2026-11-12, indoors, end to end: the system clock, the RTC write-through and survival across a reset were each observed separately rather than inferred from one. The epoch arithmetic has 12 host checks including the 2024 leap day and both century cases, and a mutant dropping the century term fails 2100-12-31 by a day."

- record_id: GPS-006
  kind: BOARD
  topic_id: GPS
  title: "GSV carries a per-constellation count of satellites in view, so a sky figure is a SUM and the shell's is a MAX"
  status: VERIFIED
  verified_date: 2026-11-15
  statement: "GSV is `$<talker>GSV,<messages>,<this one>,<in view>,...` and repeats its constellation's count of satellites in view in every message of the series. A multi-GNSS receiver emits a separate series per constellation -- GP, GL, GA, GB on this unit -- so a receiver tracking four constellations can have eighteen or twenty-four satellites in view while no single one of them reports more than eight. Measured on the board outdoors with a fix: `sats_in_view` 18 to 24 against `sats_used` 12 and HDOP 0.95 to 1.07, and indoors with no fix 3 to 0 in view. THE COUNT IS ALSO NOT WHAT GGA CARRIES: GGA's satellite field is the number USED in a solution, which is zero whenever there is no fix however much sky the antenna can hear, and it is the reason a healthy indoor receiver reads as broken if that is the only number shown."
  consequence: "Three things follow for anything that displays a satellite figure. A sky count must be summed across constellations and not taken from one series, or a good multi-GNSS receiver is reported as seeing a handful. The sum needs a staleness rule, because a constellation that stops being reported would otherwise contribute its last count for ever -- the port drops a talker not heard within 400 sentences, about half a minute at this receiver's 12 sentences per second. And the two instruments on this board now disagree by design rather than by fault: `gps raw` reports the MAXIMUM single-constellation count in its `sats visible` line because that is what a per-sentence counter naturally produces, while `nmea_state_t::sats_in_view` and the GPS app report the SUM. A future reader comparing 3 from the shell against 18 from the app should not conclude that one of them is wrong. The distinction worth keeping on a display is in view against used: a receiver with satellites in view and no fix is working and needs sky, and one with neither has an antenna or receiver fault."
  sources:
    - "tinydesk-twatch port: main/nmea.c do_gsv() and NMEA_GSV_TALKERS/NMEA_GSV_STALE, main/gps.c raw_sink() and its `sats visible` max"
    - "NMEA 0183 GSV sentence layout: field 3 is the number of satellites in view, restated per message of the series"
    - "GPS-002 for the antenna evidence that GSV counts were first used for, and GPS-004 for the indoor 3-to-5-satellite figure"
  verification: "Read from the receiver on hardware 2026-11-15: `gps raw 6` on the board counted GGA 6, RMC 6, GSA 24, GSV 24, GLL 6, VTG 6 -- four GSV series per second, so four constellations -- while reporting 3 satellites visible indoors. The app then showed 18 in view and 12 used with a fix, and 24 in view minutes earlier, both accepted by the operator as normal for their sky. The sum, the staleness rule and the empty-field case have 12 host checks in test_nmea.c; the staleness test was confirmed able to fail by raising NMEA_GSV_STALE and watching it report 18 where 11 was right."

- record_id: SSH-001
  kind: NETWORK
  topic_id: SSH
  title: "SFTP answers 16 KiB writes and 4 KiB reads, and hangs beyond that"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The shell's wolfSSH/SFTP server on port 22 has a request-size ceiling and it differs by direction. Uploads: 4, 8 and 16 KiB chunks all complete at 19-31 KiB/s, while a 24 KiB file and a single 32 KiB write never finish. Downloads: 4 KiB reads run at 74.0 KiB/s while a 16 KiB read stalls. Measured end to end on a 233,600-byte text file, a 16 KiB upload chunk size gives 28.9 KiB/s and a 4 KiB download read size gives 74.0 KiB/s, with md5 matching both ways."
  consequence: "The practical trap is that paramiko's `SFTPClient.put()` uses a 32 KiB request, so ordinary tooling hangs on every file above about 16 KiB without any error -- it is a wedge and not a slow transfer, which is why it reads as 'SFTP is slow'. Reads and writes are not symmetric, so a single chunk size chosen from one direction fails the other; the first transfer helper written here used 16 KiB both ways and hung a download that had uploaded fine. The 74 KiB/s download against 28.9 KiB/s upload also shows the radio is not the limiting factor, so the ceiling is in the SSH layer and not in the network. tools/sftp-put.py encodes the measured sizes rather than a guess, and names them in its own header so a re-measurement has a baseline."
  sources:
    - "on the watch, TinyDesk port 2026-11-12: size sweep 1/4/8/16/24/32/64 KiB up and 4/8/16 KiB down, with md5 of a 233,600-byte file both ways"
    - "tinydesk-twatch tools/sftp-put.py: PUT_CHUNK 16 KiB, GET_CHUNK 4 KiB, and the note explaining why they differ"
  verification: "Verified on hardware 2026-11-12, repeatedly: 16 KiB up completes in 0.59 s and 24 KiB never returns, and the same 233 KB file transfers intact in both directions at the quoted rates. NOT established: why the ceiling exists -- the server's read/write buffer setup was not inspected, so the boundary is measured and the cause is open."

- record_id: SSH-002
  kind: NETWORK
  topic_id: SSH
  title: "An abandoned SFTP session leaks file descriptors unless WOLFSSH_STOREHANDLE is defined"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The SD card is mounted with max_files = 5 (tdsh_sdcard.c, SD_MAX_FILES) and SFTP files are opened through wolfSSH's stdio path, where WFOPEN maps to wfopen and therefore into ESP-IDF's VFS. wolfSSH closes those descriptors on the SFTP CLOSE request and again in SFTP_FreeHandles(), which wolfSSH_SFTP_free() calls at session teardown -- but that second path is compiled out unless WOLFSSH_STOREHANDLE is defined, and settings.h defines it automatically only for WOLFSSH_FATFS, the direct-FatFs path this port does not use. Defining the macro from the port's project_include.cmake fixes it without editing any source: sixteen deliberately killed transfers now leave the card writable, where five killed it."
  consequence: "Two things about this are worth carrying. First, the leak is invisible to a clean test: a session that ends normally ends with the client's own CLOSE requests, which free descriptors regardless of the macro, so only ABANDONED sessions leak -- and that is why the failure presented as a size limit, since the size limit is what causes the abandonment. Separating the two required noticing that eight 1 KiB puts in one session and a 233 KB transfer both left the card writable while a few killed clients did not. Second, exhausting the pool breaks the card for everything, not just for SFTP: the shell's own `echo x > /sd/f` returns 1 with `vfs_fat: open: no free file descriptors`, and `sd umount; sd mount` restores reads while only a reboot restores writes. The fix is one build definition rather than a source change because both halves of the mechanism are already present and gated on one macro, which matters here since wolfSSH lives in the tinydesk-shell submodule and is upstream's to edit. Raising SD_MAX_FILES would only move the cliff."
  sources:
    - "third_party/tdsh/third_party/wolfssh/src/wolfsftp.c: SFTP_AddHandleNode/SFTP_FreeHandles, the WOLFSSH_STOREHANDLE guards, and wolfSSH_SFTP_free()"
    - "third_party/tdsh/third_party/wolfssh/wolfssh/settings.h: STOREHANDLE defined only for WOLFSSH_FATFS"
    - "third_party/tdsh/ports/esp_idf/components/wolfssh_local/include/wolfssh/port.h: WFOPEN mapped to wfopen, so files go through VFS"
    - "third_party/tdsh/ports/esp_idf/components/tdsh/src/tdsh_sdcard.c: SD_MAX_FILES 5"
    - "tinydesk-twatch ports/twatchultra/main/project_include.cmake: the definition and its rationale"
  verification: "Verified on hardware 2026-11-12 with a controlled before/after: five killed transfers exhausted the pool before the change (shell write returned 1, `vfs_fat: open: no free file descriptors` in the log), and sixteen killed transfers left the card writable after it, with a clean 233 KB upload and download both md5-verified afterwards. The definition was confirmed to reach the sources by finding it on 1514 entries of compile_commands.json and both SFTP_AddHandleNode and SFTP_FreeHandles in the rebuilt wolfsftp.c.obj. Double-close was ruled out by reading the CLOSE path, which closes the descriptor and then removes the node."

- record_id: SSH-003
  kind: NETWORK
  topic_id: SSH
  title: "One stalled SFTP client denies the whole server, silently and until a restart"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The shell's SSH server accepts TDSH_SSH_MAX_CLIENTS = 1 and services the accepted client inside the accept loop's own task. When a client stalls -- which an oversized SFTP request does reliably -- the slot stays held, and every subsequent connection completes its TCP handshake in about 10 ms and then receives no SSH protocol banner at all, indefinitely. Nothing times the session out. `ssh restart` on the watch, or a reboot, clears it."
  consequence: "The failure mode is the expensive part: a TCP connect that succeeds followed by silence is indistinguishable from a dead server, so the natural next steps are to suspect the network, the port, or the credentials rather than the previous session. It also compounds an unrelated fault into a total outage -- the request-size wedge in SSH-001 would only cost one transfer if the server shed stalled clients. The diagnostic that settles it in seconds is to read the banner directly rather than opening an SSH client: a listening socket that never sends `SSH-2.0-wolfSSHv1.5.0` is a held slot, not a broken server. `tools/sftp-put.py` names that remedy in its connection-failure message for this reason, and `ssh status` reports the occupancy as `Active clients: N / 1`."
  sources:
    - "third_party/tdsh/ports/esp_idf/components/tdsh/src/tdsh_ssh.c: TDSH_SSH_MAX_CLIENTS, ssh_handle_client() called from the accept loop, and the client counter"
    - "on the watch, TinyDesk port 2026-11-12: repeated socket connects reading no banner until `ssh restart`"
  verification: "Verified on hardware 2026-11-12: after a wedged transfer, `socket.recv` returned nothing for 8 s while `ssh status` reported `Active clients: 1 / 1`, and the same check returned `SSH-2.0-wolfSSHv1.5.0` in 0.05 s immediately after `ssh restart`. The single-slot limit and the in-loop servicing are from the source, not inferred."

- record_id: DSP-007
  kind: DISPLAY
  topic_id: DISPLAY
  status: VERIFIED
  verified_date: 2026-11-13
  title: "Chip select is what makes a pixel push real, and it is invisible to every counter the driver keeps"
  statement: "`reg_write_bus()` asserts CS around each register write and leaves it HIGH when it returns, because the panel latches a command on the CS edge and holding it low across two register writes changes what it sees. A pixel stream therefore needs CS asserted again after the window programming, and the three margin painters do exactly that. The pixel-stream push did not: when window programming was moved inside the bus acquisition on 2026-11-13, the `gpio_set_level(LCD_CS, 0)` that had followed the separate `set_window()` call was consumed by the rewrite and not re-added."
  consequence: "Every game frame was transmitted with chip select de-asserted. The SPI shifted all the bytes, `spi_device_polling_transmit` returned ESP_OK for every band, the push counters recorded the full transfer time, and `nes status` reported 53 fps with one blit per frame and zero skips -- while the panel ignored the lot and the framebuffer stayed black. The margins remained painted, because their push sites kept their CS assertion, which is the clue that was available and not read. Two things follow. A display change is not verified until someone has looked at the glass: the counters describe what the driver did, never what the panel showed, and this is the same lesson the panel-is-write-only rule in this file has been teaching since the first framebuffer. And a driver rewrite that collapses two calls into one can drop a side effect that no compiler or test notices, because CS is a GPIO and not part of any interface the type system or the transfer API tracks."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: reg_write_bus(), push_pixels_raw_buf(), and the CS assertion with this reasoning beside it"
    - "tinydesk-twatch ports/twatchultra/main/display.c: the three margin painters, which kept their `gpio_set_level(LCD_CS, 0)` and stayed visible on the glass"
  verification: "Observed on the watch 2026-11-13. The operator reported 'the fb is completely black. i dont see zelda' while `nes status` read fps=50, dropped=0, blits=220, skipped=0 -- a full and apparently healthy picture by every counter. The CS assertion was restored and the same build then showed the game, confirmed by the operator: 'looks good. no tearing.' After the fix a 20-second run reported frames=11055, blits=11055, skipped=0, so the push was also provably one-to-one and not merely visible."

- record_id: DSP-008
  kind: DISPLAY
  topic_id: DISPLAY
  status: VERIFIED
  verified_date: 2026-11-13
  title: "DSP-005's single-writer rule can be lifted, but only by all three of its parts together"
  statement: "DSP-005 says only paint_task may touch `flush_rect`, `s_tx` and the SPI bus. Running an app blit on a second core needs that rule replaced by something weaker that is still safe, and three separate mechanisms are required: a per-task transmit band, because a shared `s_tx` lets a margin relabel overwrite a band the blitter is about to send; window programming inside the bus acquisition, because CASET/RASET are panel state and a second pusher's window between another's window and its pixels sends those pixels to the wrong rectangle; and a frame-buffer pool with ownership moved through queues, because the renderer must not write a buffer the blitter is reading."
  consequence: "Implemented as `s_tx_app` alongside `s_tx`, `reg_write_bus()`/`set_window_bus()` so a band is programmed and filled under one `spi_device_acquire_bus()`, and three PSRAM frame buffers handed between the emulator on core 0 and a blit task on core 1 through `s_free_q` and `s_filled_q`. The ordering inside the blit callback is the correctness argument and is easy to get backwards: a free buffer is taken BEFORE the finished one is published, so a dry pool skips the frame rather than letting the renderer keep writing a buffer that has just been queued. Zelda's title screen went 42 to 51 fps with 11,055 frames, 11,055 blits and 0 skipped. Three buffers rather than two, because with two the renderer would wait for the blitter and there would be nothing to overlap."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: s_tx_app, reg_write_bus(), set_window_bus(), push_pixels_buf()"
    - "tinydesk-twatch ports/twatchultra/main/nes.c: NES_VBUF_N, s_free_q, s_filled_q, nes_blit_task(), nes_blit()"
  verification: "Measured and confirmed on the watch 2026-11-13. 42 fps single-task with blit 8,360 us per frame, against 51 fps with the pool: frames=11055 blits=11055 skipped=0 over 20 seconds, dropped=36 (0.3 percent). Three consecutive start/stop cycles left uptime continuous with no hang, and `nes stop` returns the desktop whole because the blit task is waited for before its queues and buffers are released. The operator played the game and reported no tearing. Per-frame blit rose from 8,360 to 10,272 us (conversion 4,266 to 6,266) -- the two cores contend for PSRAM bandwidth -- and throughput still improved, so the contention costs less than the overlap gains."

- record_id: DSP-001
  kind: DISPLAY
  topic_id: DSP
  title: "The panel has two potential writers, and the cell painter will overpaint any bitmap unless given exclusive ownership"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The port's framebuffer is written by a paint task that decides what to redraw per terminal cell, comparing each cell against `s_shown` -- its own record of what it has already drawn -- and flushing only the rectangles that differ. A bitmap blit through `display_picture_rgb()` writes pixels directly and does not update that record, so after a blit the painter still believes the glass holds the old cells and repaints them at the next notification. On a real panel this appears as the console bleeding through the image in strips wherever a taskbar clock tick or cursor blink landed, carrying whatever the previous windows had drawn."
  consequence: "An app that draws pixels must claim the framebuffer for as long as it owns the screen, and release on exit: `display_pixels_claim(true)` stops the painter, and `display_pixels_claim(false)` invalidates `s_shown` so the next frame repaints every cell -- necessary because by then the glass holds a picture rather than the cells the painter was tracking. It is a mode and not a lock because there is never more than one writer either way, and making it a lock would suggest concurrency the design does not have. Measured while owned: 142 repaint triggers over 15 seconds and three pattern changes, of which none executed. On release the painter resumed and repainted the whole screen, which is what makes the desktop come back whole rather than showing the picture underneath."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: paint_task(), the s_shown comparison, and display_pixels_claim()"
    - "tinydesk-twatch ports/twatchultra/main/display.h: the claim/release contract"
    - "on the watch 2026-11-12: `pattern 0|1|2` then `pattern off`, with the paint counters read from /sd/pattern.txt"
  verification: "Verified on hardware 2026-11-12: the operator reported all three patterns holding with no console bleed after the change, against stripped bleed before it. The suppression is a measurement rather than an impression -- `painted` stayed at 631 across 15 s while `skip` climbed from 185 to 327, and on release `painted` rose to 656 with `skip` frozen."

- record_id: DSP-002
  kind: DISPLAY
  topic_id: DSP
  title: "A full-panel RGB565 buffer cannot be static: 256 KB overflows internal DRAM by 88,896 bytes"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "A 320x416 RGB565 frame is 266,240 bytes. Declared as a static array in the port's component it fails the link with `tinydesk.elf section .dram0_0.bss will not fit in region dram0_0_seg` and `region dram0_0_seg overflowed by 88896 bytes`. Everything static in `ports/twatchultra/main` is internal DRAM by construction: the port's `extram.lf` maps only `libtinydesk`, `libtdsh` and `libmicropython` to PSRAM, and internal DRAM on this board has roughly 47 KB free once WiFi and Bluetooth have taken their share. A heap allocation in PSRAM works."
  consequence: "This decides where any frame-sized buffer lives, and it applies to the Tamagocore port directly: the emulator's frame is 300x308 (184,800 bytes) and cannot be a static array either. It also explains a constraint that looks arbitrary from outside -- the panel's DMA source buffer, `s_tx`, IS static and must stay so, because DMA out of PSRAM goes through the cache and is markedly slower, and because it is only 10 KB. The rule is not 'never static' but 'nothing frame-sized static': small buffers where DMA needs them, frame-sized buffers on the heap. The alternative, a linker mapping for this component, is available but would put every small static in the component into PSRAM as well, including the ones that need to stay internal."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/extram.lf: the three archive mappings"
    - "tinydesk-twatch ports/twatchultra/main/display.c: the s_tx comment explaining why that one is static"
    - "build log 2026-11-12: `region dram0_0_seg overflowed by 88896 bytes`"
  verification: "Verified by build: the static form failed to link with the exact overflow figure, and moving the same buffer to `malloc()` linked and ran. The 47 KB internal-free figure is from the port's own System Monitor and the earlier shell sessions."

- record_id: DSP-003
  kind: DISPLAY
  topic_id: DSP
  title: "The USB mirror goes stale when a bitmap owns the panel, so instrumenting a picture needs a second channel"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The port mirrors the desktop to USB as a stream of terminal cells, which is how every screen-reading check in this project has worked. When a bitmap claims the panel and the painter stops, that stream stops describing the glass: the desktop's diff output no longer matches what is displayed, because what is displayed is no longer the desktop. A screen reader driven off the mirror then reports a stale or blank terminal while the panel holds a correct picture."
  consequence: "A diagnostic that exists only in the terminal is unreadable precisely when a picture is up, which is when it is most wanted. The fix used here is to write the same counters to a file on the SD card as well as printing them, and read them back over SFTP. The general rule is worth stating because it will recur: when the thing under test owns the output channel, the test needs a channel it does not own. Note the mirror is still correct for everything else -- it is the desktop that is stale, not the link."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/link_twatch.c: the mirror budget and the s_mirror_stale path"
    - "tinydesk-twatch ports/twatchultra/main/display.c: the paint_task suppression"
    - "tools/watch-screen.py: the reader this affects"
  verification: "Observed on hardware 2026-11-12: after `pattern 0` the screen reader returned a terminal frame that no longer updated while `pattern stats` still ran and reported the shell was alive. The counters read back from /sd/pattern.txt continued to advance correctly, which is what showed the panel and the shell were fine and only the mirror was stale."

- record_id: DSP-004
  kind: DISPLAY
  topic_id: DSP
  title: "The leftover strips are a transfer-height fault: cap the rows per write whatever the width"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "The desktop left thin horizontal strips of stale pixels behind after a window closed, reproducibly, and the fault was in how a partial update was sized for transfer. `flush_rect()` computed the rows per SPI transaction purely from its buffer, `band = 5120 / w`, so a full-width flush was 16 rows and an 8-pixel-wide column was 640 -- a narrow update went out as one very tall window. Full-width flushes were always clean; narrow ones dropped bands. Capping the height at 16 rows whatever the width fixes it, confirmed by normal use with no recovery command needed."
  consequence: "This is LCD-009 seen from the other side, and it settles that record's open question. LCD-009 measured a 45x356 single transfer tearing into diagonal splits while the same rectangle in 2-row bands was clean, excluded width, X and height one variable at a time, and concluded the cause was unestablished -- but its exclusions were of the wrong variables: what a transfer cannot carry is ROWS, and every tearing pattern there was tall while every clean one was banded, so the width sweep was not testing the dimension that mattered. The practical rule for any code drawing on this panel: size a transfer by its row count, not by the pixel count that happens to fit a buffer. The cost of the cap is small and worth stating because the code argued the other way -- a tall narrow flush becomes one transaction per 16 rows, which for an 8-pixel column is 26 instead of one, and two window-register writes per transaction at 40 MHz is noise against the pixels."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: flush_rect(), the band computation and the cap"
    - "LCD-009 for the original tearing measurement and LCD-010 for the per-transaction cost that makes the cap cheap"
    - "on the watch 2026-11-12: strips reproduced by opening and closing the Start menu, cleared by a full-width repaint, gone after the cap"
  verification: "Verified on hardware 2026-11-12 by the operator using the watch normally -- Start menu and Files opened and closed, no strips, no `repaint` needed. The diagnosis was established by instrument: a full-width repaint cleared the strips while the framebuffer was already proven correct, which placed the fault between the framebuffer and the panel and left transfer geometry as the variable."

- record_id: DSP-005
  kind: DISPLAY
  topic_id: DSP
  title: "Only the paint task may touch the flush path; a second caller scrambles the panel and hangs the shell"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "An instrument that repainted the screen by painting cells and calling `flush_rect()` directly from the shell's task produced a scrambled taskbar and a shell that never returned; the board had to be reset. `flush_rect()` writes the shared DMA source buffer `s_tx` and acquires the SPI bus, and `paint_task` does both, so the two raced over the same buffer and bus. No lock existed between them because until then nothing had ever called the flush path from a second task. Routing the request through the paint task -- set a flag, wake it, let it invalidate its drawn-cell record so the normal comparison repaints everything -- works and returns promptly."
  consequence: "One framebuffer, one painter, and everything that wants the screen changed goes THROUGH the painter rather than beside it. This is the same rule as the framebuffer-ownership fix in DSP-001 and it was learned twice in two cycles, which is the useful part of the record: the earlier fix was written, understood and then not applied to the next thing that drew. A symptom worth recognising is that a scrambled panel plus an unresponsive shell after a screen operation is a second-writer race, not a rendering fault -- the drawn pixels are irrelevant once a buffer and a bus are shared without coordination. THE RULE IS ABOUT THE TRANSMIT PATH AND NOT ABOUT paint_task BY NAME: once an app claims the pixels the transmit path belongs to that app's blit task, and the desktop's painter is the owner only while the desktop has the glass (DSP-009, which is the same fault a third time and cost a panic)."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: flush_rect(), s_tx, and the paint task's ownership of both"
    - "tinydesk-twatch ports/twatchultra/main/display.c: display_repaint_all() setting a request flag instead of drawing"
    - "on the watch 2026-11-12: the scrambled taskbar and hung shell, then the fix"
  verification: "Verified on hardware 2026-11-12: the direct-call version scrambled the panel and hung the shell (required a reset); the flag version runs, returns, and repaints correctly, with the shell confirming responsiveness by writing a file afterwards. Revised 2026-11-24 (core-log entry 14): the consequence read as though paint_task were the only writer that could ever exist, and it was applied that way to an app drawing on a claimed screen -- the port now has four legal painters (the desktop's paint task and one blit task per emulator) and the test is which task owns the transmit path, not which task is the painter. See DSP-009."
- record_id: DSP-009
  kind: DISPLAY
  topic_id: DSP
  title: "An app paints from its blit task: a margin relabel from the touch callback panics the board on the SPI bus"
  status: VERIFIED
  verified_date: 2026-11-24
  statement: "`display_margin_label()` repaints the whole margin column SYNCHRONOUSLY ON THE CALLER'S TASK: it writes `s_tx`, programs the window, pushes the bands, and calls `spi_device_acquire_bus()`/`spi_device_release_bus()` on the panel's SPI device. Called from the CST9217 touch callback while an emulator was blitting, the board panicked: `assert failed: spi_device_release_bus spi_master.c:1310 (0)`, with the coredump naming task `tdtouch` and the backtrace `display_margin_label()` -> `paint_column()` -> `spi_device_release_bus()`. The mechanism: the emulator's blit task held the same SPI device mid polling-transaction, so `spi_device_acquire_bus()` took the `SPI_CHECK` path, logged 'Cannot acquire bus when a polling transaction is in progress.' and returned `ESP_ERR_INVALID_STATE` without touching the lock; the port does not check that return, and the matching `spi_device_release_bus()` then hit `spi_bus_device_is_polling()` true and asserted."
  consequence: "The writer discipline is about the TRANSMIT PATH, not about a task name. While an app has claimed the pixels, the task that owns the transmit path is that app's blit task -- `gb_blit_task`, `nes_blit_task`, `pm_blit_task` -- and only it may call anything that paints. A touch callback may record state and return; it may not paint. The working pattern is the one the Tamagotchi already used (EMU-019): the callback sets state or queues an event and the frame/blit task applies it, so a dynamic label needs a one-bit latch the blit task drains. THE DRAIN BELONGS IN BOTH BRANCHES OF THE LOOP: an idle or paused cartridge produces no frames at all, so an idle-only or busy-only drain leaves the label stale exactly when a person is reaching for the control. STACK IS A REAL CONSTRAINT rather than a formality: these blit tasks run at 4 KiB and `paint_column()`'s frame is 0x130 = 304 bytes, which was measured from the crash's own task table (gbblit 704 used of 4,096) rather than assumed. Note also that the failure is DETERMINISTIC rather than intermittently racy: the assertion fires on the first press that finds the blit task mid-transaction, which is why the operator saw it immediately."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: display_margin_label(), paint_column(), s_tx, spi_device_acquire_bus() and spi_device_release_bus()"
    - "tinydesk-twatch ports/twatchultra/main/touch.c: deliver_app_touch() and the app touch callback contract"
    - "ESP-IDF 5.3.1 components/esp_driver_spi/src/gpspi/spi_master.c:1310 and spi_bus_device_is_polling() at 647-650"
    - "tinydesk-twatch ports/twatchultra/main/tama.c: enqueue_action() and the guest task's drain, the pattern already in use"
    - "the coredump at flash 0x7E0000 on the watch, 2026-11-24"
  verification: "Established from the board's own crash record rather than by reasoning: the coredump partition was read with esptool and decoded with espcoredump.py against the matching ELF, which named the faulting task, the assert, the exception cause and the backtrace. Fixed in the same cycle by moving every relabel into the emulator's blit task. The operator then pressed the button on the glass and passed it (core-log entry 14). WHAT THIS DOES NOT CLAIM: no coredump was read after the operator's test, so the record rests on their report plus the code being structurally incapable of the old path -- no `display_margin_label()` call remains in any touch callback -- and not on a second decoded panic record."
- record_id: DSP-006
  kind: DISPLAY
  topic_id: DSP
  title: "Claiming the screen is not owning it: a blit leaves the console in the framebuffer, and the margins survive a full clear because they live outside it"
  status: VERIFIED
  verified_date: 2026-11-12
  statement: "`display_pixels_claim(true)` stops the cell painter, and that was taken to mean an app had the screen. It does not. `display_picture_rgb()` copies only the rectangle it is given into `s_fb`, so every pixel the painter had already written outside that rectangle -- the shell's text, the window frame -- is still in the framebuffer when ownership begins, and with the painter suppressed nothing will ever overwrite it. The operator's report was that the Tamagotchi ran 'overlayed on the console'. The missing half is a full-framebuffer clear (`display_fill()`, 320x416 to 0x0000, pushed in bands) done after claiming and before the first blit."
  consequence: "Two things follow and the second is the one worth keeping. First, an app that owns the screen must clear it, not merely claim it -- the claim is about who writes next, and the clear is about what is already there. Second, the glass OUTSIDE the framebuffer needs no special handling at all, and this was verified in the code rather than assumed: the three bottom bars and the two side key columns are painted by `paint_buttons()` and `paint_column()` during `display_init()` in CONTROLLER coordinates (x 22..66 and 387..431 for the columns, y 461..501 for the strip), which are outside `s_fb`'s 320x416 window, and the cell painter never touches them. So a framebuffer clear cannot erase the watch's controls, and the margins keep working while an app owns the screen. That is the intended arrangement -- the margins are the watch's controls, not the desktop's -- and it is now recorded because it was previously true only by accident of where each piece of code draws. A follow-on question this raises and does not settle: the margins send desktop keys (F10, Esc, Tab, Enter) under ownership too, so pressing Enter mid-game types into a hidden shell rather than reaching the app."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/display.c: display_fill(), display_picture_rgb(), paint_buttons(), paint_column()"
    - "tinydesk-twatch ports/twatchultra/main/tama.c: tama_start() clearing after display_pixels_claim(true)"
    - "tinydesk-twatch ports/twatchultra/main/edges.h: the margin geometry in controller coordinates"
    - "DSP-001 for ownership, and DSP-005 for the single-writer rule the flush obeys"
  verification: "Verified on hardware 2026-11-12: the operator saw the pet over the console before the change, and reported the cleared screen as looking great after it. The build was clean with 0 warnings, binary 0x28b510, and a status read during the run showed frames=160 blits=20 at 10 s of guest time, so the clear did not disturb the 16 Hz cadence or the dirty-row skip ratio. The claim that the margins are outside s_fb is verified by reading the geometry constants and the init-time paint calls, not by a before/after photograph of the margins with an app up."

- record_id: TOL-004
  kind: TOOLCHAIN
  topic_id: TOL
  status: VERIFIED
  verified_date: 2026-11-15
  title: "xRingbufferSend() returns pdTRUE/pdFALSE, not a byte count, and is all-or-nothing on a byte buffer"
  statement: "ESP-IDF 5.3.1 declares BaseType_t xRingbufferSend(RingbufHandle_t, const void *pvItem, size_t xItemSize, TickType_t xTicksToWait) and documents its return as 'pdTRUE if succeeded, pdFALSE on time out or if the data is larger than the maximum permissible size of the buffer'. On a RINGBUF_TYPE_BYTEBUF the item is copied whole or not at all; xRingbufferGetCurFreeSize() reports how much would fit."
  consequence: "Any wrapper that promises a write() contract -- 'returns the number of bytes accepted' -- must translate the flag, not pass it through. bleuart_write() passed it through, so every successful write reported ONE byte to td_render_flush(), which resent out+1, out+2, ... each accepted in full until the 4 KB TX ring was full; the next write returned 0, the renderer dropped the frame, marked the link down and repainted the whole screen TD_LINK_RETRY_MS (500 ms) later, for ever. It presented as a continuous ~24 KB/s stream on an idle desktop, a screen that never showed below about row 10, corrupted rows, and a `fetch` that duplicated data -- all one fault. The fix sends min(len, free) and returns exactly that; the blocking variant returns len on pdTRUE."
  sources:
    - "ESP-IDF 5.3.1 components/esp_ringbuf/include/freertos/ringbuf.h:133-140, xRingbufferSend() and its @return"
    - "tinydesk-twatch ports/twatchultra/main/bleuart.c: bleuart_write(), bleuart_write_wait()"
    - "tinydesk-twatch src/render.c:26-44 td_render_flush(), src/td.c:623-635 link-down handling"
  verification: "Verified on hardware 2026-11-15 (core-log entry 17). Before the fix, one captured BLE repaint was exactly 4,090 bytes and decomposed byte for byte into a 1,024-byte render buffer followed by that buffer from offsets 1, 2 and 3; 118,610 bytes arrived in 5 s with nothing typed. After the fix, 240 bytes arrived in 5 idle seconds (frame markers only, as on USB) and a full redraw was 3,557 bytes rendering all 26 rows cleanly."

- record_id: USB-001
  kind: USB
  topic_id: USB
  status: VERIFIED
  verified_date: 2026-11-15
  title: "The host FIFO bias sets the largest endpoint the HCD accepts, and isochronous OUT is 128 bytes at the default"
  statement: "The ESP32-S3's DWC OTG controller has one data FIFO (OTG_DFIFO_DEPTH 256, soc/esp32s3/include/soc/usb_dwc_cfg.h:44; the line count actually used is read from GHWCFG3 at runtime) split into RX, non-periodic TX and periodic TX. usb_dwc_hal_set_fifo_bias() sizes the split from CONFIG_USB_HOST_HW_BUFFER_BIAS: at BALANCED, periodic TX = OTG_DFIFO_DEPTH/8 lines and non-periodic TX = OTG_DFIFO_DEPTH/4; at PERIODIC_OUT, RX = OTG_DFIFO_DEPTH/8 + 2, non-periodic TX = OTG_DFIFO_DEPTH/16 and periodic TX takes the remainder. usb_dwc_hal_get_mps_limits() turns lines into bytes (x4), and the HCD refuses to open a pipe whose wMaxPacketSize exceeds the limit for its direction and type. The Kconfig help quotes Balanced periodic OUT as 192 bytes and Periodic OUT as 600."
  consequence: "At the default the limit is 32 lines = 128 bytes, which is below every playback alternate a UAC2 DAC is likely to offer (48 kHz stereo 16-bit alone is 192 bytes per 1 ms frame), so USB audio REQUIRES CONFIG_USB_HOST_HW_BUFFER_BIAS_PERIODIC_OUT=y; it is set in ports/twatchultra/sdkconfig.defaults. The cost is IN endpoints capped at (RX lines - 2) x 4 = 128 bytes and non-periodic OUT at 64, which still covers HID keyboards and mice, control endpoint 0, and the DAC's 64-byte interrupt endpoint. The Kconfig help's 192 for Balanced disagrees with the code and with the board; the code is authoritative. This is an ESP-IDF configuration limit, not a USB or hardware limit."
  sources:
    - "ESP-IDF 5.3.1 components/hal/usb_dwc_hal.c:184-246, usb_dwc_hal_set_fifo_bias() and usb_dwc_hal_get_mps_limits()"
    - "ESP-IDF 5.3.1 components/usb/hcd_dwc.c:1640-1667, the MPS check and its 'EP MPS (%d) exceeds supported limit (%d)' message"
    - "ESP-IDF 5.3.1 components/usb/Kconfig:14-48, USB_HOST_HW_BUFFER_BIAS and its help text"
  verification: "Verified on hardware 2026-11-15 (core-log entry 17). At BALANCED, claiming the DAC's 768-byte alternate failed with 'E HCD DWC: EP MPS (768) exceeds supported limit (128)'. With PERIODIC_OUT the 384-byte alternate was claimed and streamed with errors=0 missed=0. The PERIODIC_OUT periodic-TX ceiling itself (600 per Kconfig, up to 824 computed if GHWCFG3 reports 256 lines) was not measured, because the 384-byte alternate was the one needed."

- record_id: USB-002
  kind: USB
  topic_id: USB
  status: VERIFIED
  verified_date: 2026-11-15
  title: "The host library and each client need their own event loop, and control transfers are asynchronous"
  statement: "ESP-IDF 5.3.1's USB Host Library processes device attach, enumeration and detach only inside usb_host_lib_handle_events(), and delivers a client's transfer completions and client events only inside usb_host_client_handle_events() (usb_host.h:487: 'On completion, the transfer's callback will be called from the client's usb_host_client_handle_events() function'). usb_host_transfer_submit_control() queues the URB and returns (usb_host.c:1446-1467); the completion callback is invoked unconditionally (usb_host.c:798). usb_host_client_config_t.is_synchronous is documented 'Set to false for now', and async.client_event_callback is invoked on every queued event whatever is_synchronous says (usb_host.c:809)."
  consequence: "A driver must run a task looping on usb_host_lib_handle_events() (freeing devices on NO_CLIENTS, as usb_hid.c does) and a task looping on usb_host_client_handle_events() for its client; without the first usb_host_device_addr_list_fill() stays empty with a device plugged in, and without the second no control transfer completes and no isochronous transfer is refilled. A synchronous-looking control helper must set a callback that signals the caller and wait on it -- never from the client task itself, which is the one delivering the completion. A NULL transfer callback or NULL client event callback is a jump to address zero the moment the event arrives. Separately observed: while the host is installed the S3's USB-Serial-JTAG device does not appear on a PC even with the PC cable plugged back in; a reboot restores it, which on this board is done over the BLE console (BLE-008) with `reboot`."
  sources:
    - "ESP-IDF 5.3.1 components/usb/include/usb/usb_host.h:118-125 (is_synchronous), 483-499 (transfer submit semantics)"
    - "ESP-IDF 5.3.1 components/usb/usb_host.c:667-668, 798, 809, 1446-1467"
    - "tinydesk-twatch ports/twatchultra/main/uac2.c: lib_task(), client_task(), ctrl()/ctrl_cb(); ports/esp_idf/app/usb_hid.c:153-162 usb_lib_task()"
  verification: "Verified on hardware 2026-11-15 (core-log entry 17). With no library task, `uac2` reported dev=0 with the DAC attached while `usbdiag` -- which runs the loop -- enumerated it at address 1. With both loops and the async ctrl(), `uac2 open` found the DAC on its first call, the RANGE and SET_CUR control requests completed, and isochronous transfers streamed for over 95 s with errors=0. The USB-Serial-JTAG absence was observed after `uac2 open`: no kernel enumeration from 16:47 until a `reboot` sent over BLE at 16:54, after which it enumerated immediately."

- record_id: USB-003
  kind: USB
  topic_id: USB
  status: VERIFIED
  verified_date: 2026-11-15
  title: "The TTGK KM-HIFI-384KHZ USB-C DAC as the S3 sees it"
  statement: "The DAC (USB id 3302:3366, product 'KM-HIFI-384KHZ', bcdUSB 0x0201, device class 239/2/1) enumerates on the ESP32-S3 host at FULL speed (12 Mbit/s), EP0 max 64, one configuration of 422 bytes. It is UAC2 (protocol 0x20). Playback is interface 1, isochronous synchronous OUT endpoint 0x01, bInterval 1, with three alternates: alt 1 = 2 channels, 2-byte subslot, 16 bits, wMaxPacketSize 384; alt 2 = 3-byte, 24 bits, 576; alt 3 = 4-byte, 32 bits, 768. Interface 2 is isochronous synchronous IN on 0x81 (192 and 288 byte alternates); interface 3 is an interrupt IN on 0x83, 64 bytes, interval 8. The playback alternate's terminal link is input terminal 1, whose clock source is entity 9; a SAM_FREQ RANGE request on clock 9 returns 8000, 16000, 32000, 44100, 48000 and 96000 Hz. Inserting or removing a plug in its 3.5 mm output jack makes it drop off the bus and re-enumerate."
  consequence: "The 384 in the product name is not a rate this host can use: at full speed the largest alternate is 768 bytes per 1 ms frame. 48 kHz stereo 16-bit (192 bytes per frame) uses alt 1, which needs the PERIODIC_OUT FIFO bias (USB-001). Being synchronous, it has no feedback endpoint; the host sends a fixed 192 bytes every frame. The configuration is 422 bytes, which is why CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE is 512. Entity requests address wIndex = (entity << 8) | AudioControl interface 0. The `usbdiag` log prints 'AC header: bcdADC' for AudioStreaming descriptors too, so its 0x0001 and 0x0501 values are not the device's audio class revision. Because a jack change is a USB disconnect, a driver must survive DEV_GONE in the middle of a stream; this project's first attempt at that panicked (core-log entry 18)."
  sources:
    - "on the watch, /sd/diag/usbdiag.log from `usbdiag 30`, 2026-11-15"
    - "on the watch, `uac2 open` log lines after the parser rewrite, 2026-11-15"
    - "USB Audio Device Class 2.0 spec 4.7.2.1 (clock source), 4.7.2.4 (input terminal), 4.9.2 (AS general), 5.2.2 (control request addressing); Frmts 2.0 2.3.1.6 (Type I format)"
  verification: "Verified on hardware 2026-11-15 (core-log entry 17): enumerated by `usbdiag` (speed, ids, endpoints, '# done: 1 device(s) enumerated in 30 s') and by `uac2 open` (per-alternate formats, clock 9, RANGE rates). `uac2 tone` with set_rate(48000) -> ESP_OK on alt 1 produced a 440 Hz tone the operator heard on both channels, described as scratchy; the scratch was traced to the host's transfer boundaries (USB-004), not the device. Revised 2026-11-15 (core-log entry 18): added the jack re-enumeration, inferred from the driver's own state rather than seen on a bus analyser -- the coredump of a panic after moving the jack shows forget_gone_device() running, which only happens once the client has received DEV_GONE or a NO_DEVICE transfer status, and it happened on each of three jack changes during a stream."

- record_id: USB-004
  kind: USB
  topic_id: USB
  status: VERIFIED
  verified_date: 2026-11-15
  title: "Isochronous OUT through the IDF 5.3.1 host glitches at every transfer boundary at 16 packets per transfer, and not at 8"
  statement: "Streaming 48 kHz stereo S16 to the USB-003 DAC as isochronous OUT transfers of 16 packets each (192 bytes per 1 ms packet, three transfers in flight, each resubmitted from its completion callback), the DAC's analog output carries a discontinuity once per transfer: a 440 Hz test tone captured on a PC line input shows sidebands at 440 +/- n x 62.5 Hz at -36 dB each, and fitting the phase in 1 ms windows shows a step of about 0.18 rad (about 3 samples) every 16 ms, a brief pull toward zero over 2-3 samples. It persists with the driver proven to hand the HCD full packets on time (short=0, underruns=0, errors=0, missed=0). At 8 packets per transfer, with nothing else changed, the operator heard the tone as clean."
  consequence: "The glitch is at the URB boundary in the host path or in the DAC\'s handling of it, not in buffering upstream of usb_host_transfer_submit(); it cannot be fixed by feeding the ring better, and a counter of SKIPPED packets does not see it. At 8 packets per transfer it is gone by measurement, and 8 is what uac2.c uses (UAC2_PKTS_PER_XFER). WHY the boundary costs samples at 16 and not at 8 is still unknown -- a frame left empty or late between URBs, or the DAC concealing a late packet, are candidates and neither is established -- so a future change to the transfer length should be measured again rather than assumed safe."
  sources:
    - "PC line-in captures of `uac2 tone`, 2026-11-15, analysed for phase steps (1 ms sine fits), local-fit residual and the 62.5 Hz sideband family"
    - "tinydesk-twatch ports/twatchultra/main/uac2.c: xfer_done(), UAC2_PKTS_PER_XFER, the `short` counter"
  verification: "Measured on hardware 2026-11-15 from the DAC\'s line output on a PC line-in. At 16 packets: 115 phase steps and sidebands -36.7..-36.2 dB before the buffering fix, and 172 steps and -36.6..-36.1 dB after it with short=0 underruns=0 -- unchanged, which placed the cause at the boundary. At 8 packets: 0 phase steps, sidebands -107..-109 dB, harmonics h2..h6 at -93 dB or lower, THD+N -62.4 dB at full level (core-log entry 19). Revised 2026-11-15 (core-log entry 19): INFERRED to VERIFIED, the 8-packet case measured."

- record_id: USB-005
  kind: USB
  topic_id: USB
  status: VERIFIED
  verified_date: 2026-11-16
  title: "Claiming an interface allocates DMA memory, IDF 5.3.1 asserts when it cannot, and the internal DMA pool is what runs out -- open SD files and internal task stacks both draw on it"
  statement: "usb_host_interface_claim() allocates a pipe for each endpoint of the alternate setting, and hcd_pipe_alloc() allocates that pipe's transfer-descriptor lists in DMA-capable internal RAM (hcd_dwc.c buffer_block_alloc, transfer_descriptor_list_alloc). When that allocation fails, IDF 5.3.1 does not return an error: transfer_descriptor_list_alloc() asserts (hcd_dwc.c:1059, 'ret == ESP_OK') and the chip panics. usb_host_transfer_alloc() also takes its data buffer from DMA-capable memory (usb_private.c urb_alloc -> esp_dma_capable_malloc), but returns ESP_ERR_NO_MEM when it cannot."
  consequence: "On this board DMA-capable internal RAM is the scarce pool, and the SD card's SPI driver and FATFS take DMA-capable buffers of their own once a file is open. So the same stream start that succeeds from the shell panics from a file player that has opened its file first. Every DMA allocation the USB audio stream needs -- its transfers and its interface claim -- is therefore made once, in `uac2 open`, before any file is opened, and held while the DAC is open; stop and rate changes only switch the alternate setting. Any future USB host code on this board should do the same: claim early, never claim from a path that has touched the SD card, and treat a failed claim as a panic risk rather than an error to handle. THE BETTER REMEDY IS TO NOT SPEND THE POOL AT ALL: with CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY enabled, xTaskCreateWithCaps(MALLOC_CAP_SPIRAM) moves a 4 KB task stack out of internal RAM entirely, and three such stacks (usb_lib, the uac2 client and the uac2 worker) were 12 KB of it. On this board that took the pool from dma free 6,359 / largest 4,096 to free 18,675 / largest 10,752, which is the difference between a claim that asserts and one that succeeds with room to spare. A task created that way must never itself call a flash or OTA API, because the flash cache is disabled during those operations and a PSRAM stack cannot be executing then."
  sources:
    - "ESP-IDF 5.3.1 components/usb/hcd_dwc.c:1059 transfer_descriptor_list_alloc, :1598 buffer_block_alloc, :1868 hcd_pipe_alloc"
    - "ESP-IDF 5.3.1 components/usb/usb_private.c urb_alloc (esp_dma_capable_malloc), usb_host.c:1385 usb_host_transfer_alloc"
    - "tinydesk-twatch ports/twatchultra/main/uac2.c: uac2_open() (allocate and claim once), UAC2_XFER_BUF_BYTES"
  verification: "Verified on hardware 2026-11-15 (core-log entry 19) by coredump: 'assert failed: transfer_descriptor_list_alloc hcd_dwc.c:1059 (ret == ESP_OK)' in task 'uac2play', stack hcd_pipe_alloc <- usbh_ep_alloc <- interface_claim <- usb_host_interface_claim <- uac2_start_rate(44100) <- play_task. The build before it, which claimed at start but allocated transfers there too, failed instead with 'transfer alloc: ESP_ERR_NO_MEM' from the player while `uac2 tone` from the shell started fine on the same boot. With everything allocated in `uac2 open`, clip10.wav played to the end with short=0 underruns=0. Revised 2026-11-16 (core-log entry 22): the SAME assert fired a third time, in task 'usb_lib' during enumeration (root_port_handle_events -> usbh_devs_add -> device_alloc -> hcd_pipe_alloc), which establishes that the shortage is general to the pool rather than specific to the claim, and that any DMA held at boot starves enumeration before a device is even present. The task-stack figures above are measured: with the three USB task stacks in internal RAM the OTG pool read dma free 6,359 / largest 4,096, and with them in PSRAM it read free 18,675 / largest 10,752 on the same board in the same mode."

- record_id: USB-006
  kind: USB
  topic_id: USB
  status: VERIFIED
  verified_date: 2026-11-16
  title: "The USB-003 DAC's playback Feature Unit MUTE refuses SET_CUR with a STALL, so the output stage cannot be unmuted from the host and is re-armed only by a streaming alternate-setting transition"
  statement: "The DAC's first stream after it is plugged in plays as silence while the watch reports a complete, gap-free stream (short=0, underruns=0, errors=0), and re-seating the headphone plug makes the same file audible. `uac2 fu set`, which issues SET_CUR on the playback Feature Unit's MUTE control, reports `ctrl 21/01 status 4` -- USB_TRANSFER_STATUS_STALL -- so that request is refused by the device and the host cannot clear it; the Volume control does answer SET_CUR across its RANGE of -18944..0 /256 dB (-74 to 0 dB) and reads back what was set, but on the operator's headphones -26 dB sounded like roughly 70 percent of full. Re-seating the plug is a jack-detect event inside the dongle, whose own firmware re-arms the output stage, which is why it works and why nothing on the host side does."
  consequence: "A SET_INTERFACE round trip -- alternate 0, then the streaming alternate -- re-arms the DAC's output stage, and a STALL on the mute control is reported and not fatal. `uac2` therefore cycles the alternate setting on every stream start (dac_wake()) and offers `uac2 wake` for the same thing by command, which gives the same recovery a headphone replug gave without touching the jack. Its hardware volume is still not a usable volume control on headphones; scaling the samples measured exact (-20.0 dB in every band at 10 percent) and is the better control."
  sources:
    - "on the watch, `uac2`, `uac2 fu set` and the Log Viewer during the silent and the audible runs, 2026-11-15 and 2026-11-16"
    - "operator listening reports, and PC line-in captures that came back silent while the watch reported a clean stream"
    - "tinydesk-twatch ports/twatchultra/main/uac2.c: dac_wake(), apply_volume(), fu_report()"
  verification: "Revised 2026-11-16 (core-log entry 22) from INFERRED to VERIFIED for the recovery and the refusal, still inferred for the device's internal state. Silent first plays were reproduced at 17:16, 17:31, 17:34, 19:15-19:17, 19:55 and 19:56 on 2026-11-15; on 2026-11-16 the operator stated the pattern plainly -- the first play after a plug is silent, and after a headphone replug the same file is audible. The STALL is measured, not inferred: `uac2 fu set` logged `W uac2: ctrl 21/01 status 4` and `E uac2: ctrl A1/01: no completion`. With dac_wake() in the start path, the first play after a plug was audible with `frames` climbing, `errors=0`, `underruns=0`, `short=0`, and no headphone replug. What the dongle's output stage actually latches cannot be observed from this side, so the mechanism remains behavioural."

- record_id: AUD-001
  kind: AUD
  topic_id: AUD
  status: VERIFIED
  verified_date: 2026-11-16
  title: "A volume percentage is a loudness scale, not an amplitude: -10 dB at half is a lab result, -18 dB at half is the hardware convention, and only linear-in-dB gives even steps"
  statement: "Three different laws are all defensible for mapping a volume percentage to a gain, and they disagree by up to 10 dB at the same dial position. The psychoacoustic result is that loudness roughly doubles every +10 dB, so HALF AS LOUD is -10 dB and the amplitude gain is (pct/100)^1.66096. The hardware convention comes from the audio-taper potentiometer: a 15A taper presents about 15 percent of its resistance at half rotation, which is -16.5 dB, and a 10A taper gives -20.0 dB at half. VLC's software mixer cubes the GAIN (modules/audio_output/volume.h, 'Cubic mapping from software volume to amplification factor', with a comment requiring every volume-capable plugin to match it), and its Qt dial spans more than unity -- qt-max-volume defaults to 125 with a range of 60..300 (modules/gui/qt/qt.cpp), and the plain slider sets maximum 200 -- so unity sits at 80 percent of travel and the top of the dial is OVERDRIVE. Over the sub-unity portion, VLC's cubic reaches -18.1 dB at the factor-0.5 point, agreeing with linear-in-dB over a 36 dB range, which puts 50 percent at -18.00 dB; the two arrive at the same midpoint by different routes. A fourth shape, linear amplitude (gain = pct/100), is what a wrong potentiometer produces: -6 dB at half, with all its attenuation crammed into the bottom of the travel."
  consequence: "For a percentage control a listener uses rather than measures, the midpoint should be about -18 dB, not -10 dB: the operator rejected a -10 dB-at-half power law by ear as 'inverted -- steep at the bottom and dead at the top' even though it measured exact to 0.02 dB at the DAC's line output, and that description is the correct reading of the SHAPE rather than of the accuracy. A power law is right at exactly one point of the travel and wrong on both sides of it: the -10 dB-at-half version spends 10 of its 33 dB in the first tenth of travel and 1.7 dB in the last, and VLC's cubic is more front-loaded still (18 dB in the first tenth, -60 dB by 10 percent, 2.7 dB in the last). Linear-in-dB over a 36 dB range spends an even 3.6 dB per 10 percent of travel, so no part of the dial is dead, one percent of travel is 0.36 dB which is under the ~1 dB the ear resolves, and the bottom of the travel is -36 dB -- still audible on headphones. VLC's overdrive cannot be copied on this board: 100 percent is already digital full scale, so any boost above it clips in the samples, and only the sub-unity portion of its law is usable. Attenuating in the samples also costs resolution at the bottom -- 10 percent gives gain 0.063, so a 16-bit full-scale sample becomes 2069 of 32767, about 11 effective bits -- which is a noise-floor matter, not a loudness one, and the reason a S24 or S32 alternate would be wanted before going deeper than 36 dB."
  sources:
    - "VLC source, modules/audio_output/volume.h (aout_SoftVolumeSet, 'Cubic mapping'), modules/gui/qt/qt.cpp (qt-max-volume default 125, range 60..300), modules/gui/qt/components/controller_widget.cpp (QSlider maximum 200), include/vlc_aout.h (AOUT_VOLUME_DEFAULT 256, AOUT_VOLUME_MAX 512)"
    - "audio-taper potentiometer resistance laws: 15A and 10A tapers, approximately 15 percent and 10 percent of resistance at half rotation"
    - "tinydesk-twatch ports/twatchultra/main/uac2.c: vol_db256(), apply_volume(), UAC2_VOL_RANGE_DB, dac_flat()"
  verification: "Verified on hardware 2026-11-16 (core-log entry 23) at the DAC's analog line output, captured on a PC line input with its capture gain at -15 dB and Line Boost at 0 dB -- both required, because at the default +20 dB boost the codec limited the signal and every reading above about 70 percent flattened to the same level, which produced a false 'the curve is wrong at the top' conclusion that cost a round trip. With the capture chain clean, a five-point sweep of the -10 dB-at-half power law matched the predicted curve to within 0.02 dB (100, 75, 50, 25 and 10 percent measured -28.61, -32.76, -38.61, -48.59 and -61.82 dBFS), so the law was implemented correctly and its rejection was a matter of shape, not error. The linear-in-dB replacement was verified in the firmware's own arithmetic: 3.60 dB per 10 percent of travel at every step, monotonic over 1..100, 50 percent at exactly -18.00 dB, and 100 percent at unity with the multiply skipped."

- record_id: AUD-002
  kind: AUD
  topic_id: AUD
  status: VERIFIED
  verified_date: 2026-11-16
  title: "A FLAC decoder fits on the ESP32-S3 only if its working set comes from PSRAM sized by the file: micro-flac costs 31.7 KB of flash, Rockbox's needs ~190 KB of internal SRAM"
  statement: "Three FLAC decoders were measured for this board, and the axis that decides is internal RAM, not licence or popularity. Rockbox lib/rbcodec/codecs/flac.c declares its channel buffers as `static int32_t decoded0..6[MAX_BLOCKSIZE]` with IBSS_ATTR, that is seven channels' worth in internal IRAM sized for the worst case whether or not the file needs it -- 7 x 4608 x 4 = 126 KB at Rockbox's default MAX_BLOCKSIZE of 4608, 229 KB at 8192 -- plus a 64 KB input buffer from `ci->request_buffer(&buff_size, MAX_FRAMESIZE+16)`. dr_libs dr_flac keeps a 4,512-byte decoder state (including a 4 KB input cache) and allocates one PCM buffer per block through DRFLAC_MALLOC, roughly 41 KB for a 4608-block stereo file, with no host layer. esphome-libs micro-flac (Apache-2.0, v0.3.0) keeps a ~290-byte decoder object and allocates one block-samples buffer of `max_block_size x channels x 4`, which is typically 16-64 KB, through heap_caps_malloc_prefer with PSRAM first; it also ships Xtensa assembly for LPC prediction restore (src/xtensa/lpc_32_xtensa.S and lpc_64_xtensa.S) selected by CONFIG_MICRO_FLAC_ENABLE_XTENSA_ASM, which defaults on for the ESP32-S3. The wider ecosystem points the same way: the most-used ESP32 audio library, schreibfaul1/ESP32-audioI2S (about 1,730 stars), is GPL-3.0 and uses its own purpose-built decoders with a documented limit of 24576-byte blocks and 1-2 channels, and requires PSRAM; no ESP32 project using dr_flac was found."
  consequence: "With roughly 18 KB of internal SRAM free while the desktop runs, a decoder that statically reserves 190 KB cannot run on this board at all -- Rockbox's would have to have its static arrays patched out of GPL source first, which is a different and larger job than vendoring it. The usable shape is a decoder whose working buffer is (a) sized from the file's STREAMINFO rather than from a worst case and (b) allocated in PSRAM, which is what micro-flac does and why it was chosen. Its measured cost on this board is 31.7 KB of FLASH -- the component's own .a is 76 KB because it also contains the encoder, which link-time garbage collection drops entirely, so a size read off the archive is misleading. A future codec should be judged the same way: measure in PSRAM, never in internal IRAM, and measure the linked image rather than the archive."
  sources:
    - "Rockbox lib/rbcodec/codecs/flac.c (decoded0..6 with IBSS_ATTR, request_buffer MAX_FRAMESIZE), lib/rbcodec/codecs/libffmpegFLAC/decoder.h (MAX_BLOCKSIZE 4608, MAX_FRAMESIZE 65536)"
    - "mackron/dr_libs dr_flac.h (drflac struct, DR_FLAC_BUFFER_SIZE 4096, drflac__resize_decoded_samples)"
    - "esphome-libs/micro-flac v0.3.0: include/micro_flac/flac_decoder.h, src/alloc.h, src/flac_decoder.cpp, src/xtensa/, Kconfig"
    - "schreibfaul1/ESP32-audioI2S: src/flac_decoder/flac_decoder.h (block and channel limits), README (PSRAM required)"
    - "tinydesk-twatch ports/twatchultra/main/musicdec.cpp, components/micro-flac/CMakeLists.txt and README.vendored.md"
  verification: "Measured on hardware 2026-11-16 (core-log entry 24). Flash: the real decode path was compiled for xtensa-esp32s3 into both an empty ESP-IDF app and the watch's firmware, and the image grew 31.7 KB in isolation (206,576 to 239,008 bytes) with 55 micro_flac symbols in the final ELF and no FLACEncoder symbol, confirming the encoder is dropped. RAM: sizeof(drflac) was measured at 4,512 bytes on the host; micro-flac's buffer arithmetic was checked against its source (max_block_size x channels x sizeof(int32_t) in decode_frame). Behaviour on the watch: 352800 frames for an 8 s 44.1 kHz stereo tone and 192000 for a 4 s 48 kHz one, both exact, and a 5:58 track played with 0 errors and 0 short writes while the frame counter advanced 548352 frames over an interval matching wall-clock time; internal RAM returned to its starting figure after the task ended. Rockbox's figures are read from its source rather than measured on this board, because its static reservation is what makes it unbuildable here."

- record_id: IMU-001
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "The BHI260AP's firmware is host-loaded into the part's RAM from the `moddata` partition, in a multi-segment container whose first segment lands at 0x124000"
  statement: "The BHI260AP is an ARC core running compiled firmware supplied by the host, not a fixed mask-ROM part: the image is stored in flash at 0xE10000 (the `moddata` partition plus 64 KB) and streamed into the part by `imu boot upload`. The container is multi-segment rather than a flat blob; a segment's load address is carried in the image rather than implied, and the image measured here loads its first segment at 0x124000. The header carries a SHA-256 of the payload at offset 0x08, and that check is real: one flipped payload byte is rejected."
  consequence: "A custom algorithm image is therefore a software problem, not a silicon one -- but the part verifies what it is given, so a modified payload must carry a matching digest, and two further fields at header offsets 0x2c and 0x44 remain unidentified and are enforced. The firmware lives in `components/imu/bhi260_klio.img` in the fork and is flashed separately from the app and the codec (`make imu`), sharing `moddata` with the codec's .rodata without overlapping because the codec occupies offset 0 and the IMU image starts 64 KB in."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/imu/bhi260_klio.img, ports/twatchultra/main/imu.c (cmd_imu_boot, firmware load path)"
    - "tinydesk-twatch ports/twatchultra/modules/Makefile (IMU_IMG, IMU_AT = 0xE10000)"
    - "Bosch BHI260AP firmware container layout, established by inspection of shipped images (core-log entries 62-65)"
  verification: "Verified on hardware 2026-11-17 and 2026-11-18 (core-log entries 62-65, 71-74): `imu boot upload` reports BOOTED, `chip 70 boot 38 err 00`, and 35 virtual sensors are enumerable. Multi-segment loading and the 0x124000 first-segment address were established by comparing shipped images; the digest at 0x08 was confirmed by flipping one payload byte and observing rejection. Two fields at 0x2c and 0x44 are enforced but not identified, and are recorded here as open rather than guessed."

- record_id: IMU-002
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "A KLIO learning frame is 11 bytes, and its progress, index and change_reason bytes have defined meanings: 100 learns a pattern, and an interruption is explained by the reason byte"
  statement: "Bosch's bhy2_klio_defs.h defines `bhy2_klio_sensor_frame_learning_t` as `uint8_t instance; int8_t index; uint8_t progress; uint8_t change_reason;` with these documented meanings: index -1 means no new learning has occurred and any value >= 0 means a pattern has been learnt and may be read; progress counts from 0 to 100 while learning and reaching 100 learns a new pattern, with the documented behaviour that 'if learning is interrupted, this progress will return to 0, and change reason will be set to indicate why'; change_reason is 0 = learning is progressing, 1 = learning was interrupted by a non-repetitive activity, 2 = learning was interrupted because no significant movement was detected. The frame as it arrives in the FIFO is 11 bytes, with the index at byte 1, progress at byte 2 and change_reason at byte 3."
  consequence: "A learning frame is self-describing, so a run can be judged from its frames rather than from a guessed threshold. It also means `progress` returning to 0 is the documented signal of an interruption and not a bug, and that the reason byte distinguishes the two interruptions Bosch describes -- non-repetitive motion from undetected motion -- which are different problems with different fixes. Any tool that reports progress alone throws away the field that says why."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/bhy2/include/bhy2_klio_defs.h (bhy2_klio_sensor_frame_learning_t, progress and change_reason doc comments)"
  verification: "Verified on hardware 2026-11-18 (core-log entries 73, 74). The 11-byte size is enforced by the tap in ports/twatchultra/main/imu.c, which rejects any other length after an earlier defect decoded 7-byte accelerometer frames as KLIO and produced impossible progress values such as 246. Byte offsets were confirmed against captured frames, and the documented reason vocabulary was observed directly: a run produced progress 20, 40, 60, 80 with reason 0, followed by a frame with progress 0 and reason 1."

- record_id: IMU-003
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "Data injection needs a word-aligned payload and the dedicated inject ids 55 and 58, not sensor ids; its two error codes are 0x7C for misalignment and 0xE2 for rejected content"
  statement: "The part's data-injection handler enforces two rules that are stated in Bosch's BHI360 SDK rather than in the BHI2xy SDK this port vendors. First, a payload whose length is not a multiple of four bytes is rejected with `BHI360_ERR_INPUT_CHANNEL_NOT_WORD_ALIGNED` = 0x7C, before the handler is reached. Second, the ids a payload carries are dedicated injection ids, not sensor ids: `dinject_defs.h` defines `ACCEL_INJECT_ID` = 55, `GYRO_INJECT_ID` = 58, `ACCEL_INJECT_EVENT_SIZE` = 7 and `GYRO_INJECT_EVENT_SIZE` = 7. A payload that reaches the handler and is refused returns `BHI360_ERR_SENSOR_DATA_INJECTION` = 0xE2. Bosch's own reference tool sends a 6-byte full timestamp as one injection call and then a 17-byte data block as another, and its SDK zero-pads an unaligned length to a multiple of four."
  consequence: "Both rules are invisible in the BHI2xy SDK this port uses, and each cost this project multiple cycles: every attempt before entry 67 sent an unaligned payload and read 0x7C as 'the part ignores injection', and every attempt before entry 71 used a sensor id and read 0xE2 as 'the content is wrong'. The instrumented command in ports/twatchultra/main/imu.c therefore pads every block and builds it from ids 55 and 58, and reports the error register per block so framing and content failures stay distinguishable."
  sources:
    - "Bosch BHI360 SensorAPI, source/bhi360_defs.h (BHI360_ERR_INPUT_CHANNEL_NOT_WORD_ALIGNED 0x7C, BHI360_ERR_SENSOR_DATA_INJECTION 0xE2)"
    - "Bosch BHI2xy SensorAPI, examples/swim/swim_data_injection and dinject_defs.h (ACCEL_INJECT_ID 55, GYRO_INJECT_ID 58, event size 7)"
    - "Bosch BHI2xy SensorAPI, bhy2_hif_exec_cmd_generic() (rounds to four bytes, zero-fills the tail)"
  verification: "Verified on hardware 2026-11-18 (core-log entries 67, 71): a block built from id 4 returns 0xE2 while the same block built from id 55 returns 0x00, six times in a row; and an unaligned 17-byte payload returns 0x7C where the same payload padded to 20 bytes reaches the handler. Every attempt to resume this work should start from these two constants rather than re-deriving them."

- record_id: IMU-004
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "A learnt KLIO pattern is proven by bhy2_klio_read_pattern() returning a 157-byte pattern; rc -2 means there is none, and the progress byte is not evidence"
  statement: "`bhy2_klio_read_pattern()` takes a caller buffer and a length in/out parameter. On success it returns BHY2_OK and reports a 157-byte pattern. When no pattern has been learnt it returns -2 (an invalid-parameter class error, as documented for 'trying to read a learnt pattern when no pattern has been learnt') and leaves the length unchanged at whatever the caller passed, which is why the size that comes back in a failed call is the buffer size and not a pattern size."
  consequence: "The 157-byte return is the only gate this project accepts for a learnt pattern, because the `progress` byte has been observed at 1, 20, 35, 43, 96 and 246 and cannot be trusted. Reading a pattern must also happen immediately after the engine announces one: any reset of the engine clears it, so a `klio save` issued after the fact reported 'invalid param' on a pattern that had just existed."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/bhy2/include/bhy2_klio.h (bhy2_klio_read_pattern), bhy2_klio_defs.h (invalid-pattern error)"
  verification: "Established as a negative result on hardware across core-log entries 71-74: read_pattern has returned rc -2 with a 244-byte buffer on every injected run, including runs with zero FIFO overflows and zero sensor errors, and has never returned 157 bytes from injected data. A 157-byte pattern HAS been read from a gesture trained by hand, which is what makes the gate meaningful rather than hypothetical."

- record_id: IMU-005
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "Meta event codes are 1,2,3,5-16,18,19,20; with the physical sensors streaming the meta callback also receives packets that are not meta events, so its counts are only interpretable with those sensors off"
  statement: "bhy2_defs.h defines the meta event ids as 1 FLUSH_COMPLETE, 2 SAMPLE_RATE_CHANGED, 3 POWER_MODE_CHANGED, 5 ALGORITHM_EVENTS, 6 SENSOR_STATUS, 7-9 the BSX step counters, 10 RESERVED1, 11 SENSOR_ERROR, 12 FIFO_OVERFLOW, 13 DYNAMIC_RANGE_CHANGED, 14 FIFO_WATERMARK, 15 RESERVED2, 16 INITIALIZED, 18 SENSOR_FRAMEWORK, 19 RESET and 20 SPACER. Meta packets arrive on two channels, the normal FIFO (id 254) and the wakeup FIFO (id 248), so both must be registered or a rejection looks like silence. Observed on this board: with the accelerometer and gyroscope switched off, a 30-second run produced 12-15 meta packets, every one of them code 20 (SPACER); with those sensors streaming, the same callback received thousands of packets whose first bytes were spread across 0-31 and included at least one packet that was not a meta event at all (first byte 0xF5, sensor byte 253)."
  consequence: "A meta-event count taken while the physical sensors are running is not a measurement of meta events, and an instrument built on one should either switch those sensors off or attribute packets by sensor id before counting. Bosch's own CLI activates only the algorithm during injection (`actse 114:100`), which is the configuration in which the meta channel behaves coherently on this board."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/bhy2/include/bhy2_defs.h (BHY2_META_EVENT_* values, BHY2_SYS_ID_META_EVENT 254, BHY2_SYS_ID_META_EVENT_WU 248)"
  verification: "Verified on hardware 2026-11-18 (core-log entry 74) by instrumenting the meta callback in ports/twatchultra/main/imu.c to count each code into a histogram and record the first four packets verbatim. Sensors off: 12 and 13 packets, all code 20. Sensors on: 4122 packets with codes 0 and 31 dominant, and a final packet of ev 245 / sensor 253, which is a sensor id rather than an event code. The verbatim packets captured are `14 01 00 f7` (code 20 = SPACER) and `10 67 17 00` (code 16 = INITIALIZED), both consistent with the SDK."

- record_id: IMU-006
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "Accelerometer and gyroscope at 50 Hz alongside 100 Hz injection overflow the part's single FIFO; with both switched off the overflows and sensor errors are zero"
  statement: "The injected blocks and the physical sensors' own samples share one FIFO on this part. Measured with the accelerometer and gyroscope configured at 50 Hz while 3000 injection blocks were sent at 100 Hz over 30 seconds: 42 `FIFO_OVERFLOW` and 30 `SENSOR_ERROR` meta events, attributed to sensor 4 (the accelerometer), with only 9 KLIO frames and a last error of 0xE2 on the data path. Measured with both physical sensors explicitly disabled and every other variable unchanged: 0 overflows, 0 sensor errors, 16 KLIO frames, and 12-15 meta packets in the whole run."
  consequence: "Injection replaces the physical sensors' contribution rather than supplementing it, so an injected experiment should disable both physical sensors -- which is also what Bosch's own CLI does. A drain-tightening change is not the fix for this and was the wrong next step before it was measured: the FIFO is being over-fed, not under-drained, and the port's own imu_poll() records two earlier attempts at a tighter drain that wedged the shell task."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/imu.c (cmd_imu_inject, `phys` argument; imu_poll; imu_drain_hard)"
    - "Bosch BHy2CLI flow: actse <sensor>:<rate> dmode s dinject <log> dmode n actse <sensor>:0 -- the algorithm is activated and the physical sensors are not"
  verification: "Verified on hardware 2026-11-18 (core-log entry 74) as a single-variable comparison: identical waveform (`imu inject 30 1 112 1` against `imu inject 30 1 112 0`), identical block count and rate, differing only in whether the accelerometer and gyroscope were configured. This supersedes the earlier and now void comparison recorded in entries 71 and 72, which ran while the frame tap was still decoding accelerometer packets as KLIO frames."

- record_id: IMU-007
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "With a clean FIFO, KLIO climbs 20/40/60/80 in about 5 s, then returns to 0 with change_reason 1 and stops emitting frames entirely; the peak is not reproducible between identical runs"
  statement: "Two identical 30-second runs of the designed gesture (a 1.5 s cycle of 60 percent swing and 40 percent rest, injected at 100 Hz into sensor 112 with both physical sensors disabled) produced the following. Run A: progress 0 until 9 s, then 20 at 10 s, 40 at 12 s, 60 at 14 s, 80 at 15 s, and a frame with progress 0 and change_reason 1 immediately after; 15 KLIO frames in total, after which the engine emitted no further frames for the remaining 1400 accepted blocks. Run B: the same progression through 80 at 15 s, 15 frames, and the same silence. Across these and the earlier sine runs, every completed run reported 12-16 frames and a frozen frame count for the second half. The recorded peak differed between two runs that should have been identical: 80 in one, 20 in another, with the same 3000 blocks, the same 15 frames, zero overflows and zero sensor errors in both, and 0 `change_reason 2` (no movement) frames in any run."
  consequence: "The interruption is the engine's own judgement, not transport: with the FIFO quiet and no errors, KLIO increments progress by 20 per repetition, reaches 80, and then decides the activity is non-repetitive (reason 1) and stops producing output rather than reporting stillness (reason 2 is never seen). Any further attempt should therefore vary the waveform's stillness and repetition boundary -- the two things reason 1 and reason 2 are about -- rather than the transport. The run-to-run variance in peak means a single run is not evidence about convergence, so a future cycle should record several runs per configuration."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/imu.c (cmd_imu_inject waveform, step_ms, klio_tap_cb, peer_progress, trace writer)"
    - "/fs/root/inject-<shape>-p<phys>.txt on the board, recovered off the `storage` partition with esptool"
  verification: "Verified on hardware 2026-11-18 (core-log entry 74). The operator watched one run live and described it before the trace was read -- 0 at first, a slow climb to 60 about halfway through the cycle, then a drop to 0 that held for the rest of the run -- and the trace recovered off the chip shows exactly that shape with the peak samples at 80. The frame count freezes at 15 in both runs and in the earlier sine runs, which is the observation that distinguishes 'the engine stopped talking' from 'the engine is still evaluating'."

- record_id: TOL-005
  kind: TOOLCHAIN
  topic_id: TOL
  status: VERIFIED
  verified_date: 2026-11-18
  title: "The watch and the TinyTang are both ttyACM devices and swap numbers between reboots; identify them by USB id, never by ttyACM number"
  statement: "On this workstation two unrelated boards enumerate as /dev/ttyACM devices. The watch's ESP32-S3 USB-Serial-JTAG presents as USB vendor 303a model 1001 (`Espressif USB JTAG/serial debug unit`, serial 80:45:6B:46:DF:54). The TinyTang presents as vendor ffff model 5454 (`TinyTang CDC`, serial 0.1.3). Which one is ttyACM0 depends on enumeration order and has been observed on either number."
  consequence: "Any host tool that opens a ttyACM path by number can write to the wrong board, and a write is not harmless: a terminal repaint sequence lands in the other board's shell. Tools must resolve the watch through /dev/serial/by-id or by checking the device's USB ids, and should refuse rather than guess. read_modaddr.py in the fork already resolves by-id for this reason; tools/watch-screen.py now does the same and refuses any device that is not the Espressif JTAG unit."
  sources:
    - "udevadm info -q property -n /dev/ttyACM0 and /dev/ttyACM1 on the operating workstation, 2026-11-18 (ID_VENDOR_ID/ID_MODEL_ID as above)"
    - "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00 and usb-TinyTang_TinyTang_CDC_0.1.3-if00"
  verification: "Verified on the workstation 2026-11-18 (core-log entry 74) after tools/watch-screen.py, which defaulted to /dev/ttyACM0, sent its Ctrl+L repaint to the TinyTang while the operator was flashing that board. The by-id resolution and the refuse-if-not-Espressif guard were then exercised against both devices: the guard rejects /dev/ttyACM0 (TinyTang) and accepts /dev/ttyACM1 (watch)."

- record_id: TOL-006
  kind: TOOLCHAIN
  topic_id: TOL
  status: VERIFIED
  verified_date: 2026-11-18
  title: "esptool --after no_reset leaves the ESP32-S3 in its ROM bootloader, so the console is silent and later reads are stale until a hard reset"
  statement: "An esptool operation on this board with `--after no_reset` leaves the chip in the ROM download loader rather than starting the application. In that state the USB-Serial-JTAG carries the loader, not the desktop, so reading the port returns nothing and any host tool that expects the application's terminal output sees an empty stream."
  consequence: "A sequence that reads flash and then wants to observe the running firmware must end with a hard reset -- `--after hard_reset chip_id`, or a power cycle -- before trusting anything the console reports. Without it a command typed into the board is silently discarded and the last thing written to flash is read back as if it were new, which is indistinguishable from a command that ran and produced no output."
  sources:
    - "esptool 4.13.0 `--after` option semantics (hard_reset | soft_reset | no_reset | no_reset_stub)"
  verification: "Verified on hardware 2026-11-18 (core-log entry 74): after `read_flash ... --after no_reset` the watch returned zero bytes on the serial port and two separate command sequences appeared to run without effect, while the traces recovered from flash were older than the runs being attempted. A `--after hard_reset chip_id` restored the desktop immediately."

- record_id: TOL-007
  kind: TOOLCHAIN
  topic_id: TOL
  status: VERIFIED
  verified_date: 2026-11-18
  title: "Opening a Terminal on the watch from a host is reboot, F10, Enter -- and the window must be verified to be tdsh, because the Start menu remembers its last selection"
  statement: "The desktop is driven over the USB mirror by sending escape sequences. F10 is `ESC [ 2 1 ~` and opens the Start menu; Enter launches the highlighted item. The Start menu does NOT reset its selection to the first item when it is closed and reopened -- it remembers what was last selected -- so F10 followed by Enter launches whichever application was chosen before, not necessarily Terminal. A fresh boot does start with Terminal selected."
  consequence: "Any host harness that needs a shell must either reboot the board first, making F10/Enter deterministic, or read the screen back and confirm that a `tdsh` window is present before sending a command. Failing to check is not a cosmetic problem: the typed command becomes keystrokes delivered to whatever application did open, which on one occasion started the Tamagotchi emulator and fed `imu inject` into it as game input. The escape sequence must also be written in ONE write rather than byte by byte, because the desktop's parser abandons a lone ESC after a few tens of milliseconds and every such key arrives as Escape followed by literal text."
  sources:
    - "tinydesk-twatch tools/watch-screen.py (Screen model, DECSC/DECRC handling, drain-before-send)"
    - "tinydesk-twatch ports/twatchultra/main/ (desktop key handling for F10 and Enter; Start menu selection persistence)"
  verification: "Verified on hardware 2026-11-18 (core-log entry 75): repeated attempts using F10 followed by Enter with no screen check launched the Tamagotchi emulator several times, and `imu inject 30 ...` was typed into it as game input. After a reboot the same F10/Enter sequence produced a `tdsh` window on the first attempt, confirmed by reading the screen back, and the injection runs then executed normally."

- record_id: TOL-008
  kind: TOOLCHAIN
  topic_id: TOL
  status: VERIFIED
  verified_date: 2026-11-21
  title: "Deleting sdkconfig to force regeneration silently drops settings that live only in that file, including the coredump partition and the ISR stack size"
  statement: "ESP-IDF regenerates `sdkconfig` from `sdkconfig.defaults` only when `sdkconfig` is absent, and the generated file then holds every value the project set at any time -- including ones set interactively with `idf.py menuconfig` or inherited from an earlier defaults file that no longer contains them. Removing `sdkconfig` to force a clean reconfiguration therefore reverts every such setting to its IDF default without any message. On this board that dropped `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` (to `CONFIG_ESP_COREDUMP_ENABLE_TO_NONE=y`) and `CONFIG_FREERTOS_ISR_STACKSIZE` from 2096 to 1536, and also reverted the NimBLE role set and dropped `CONFIG_FREERTOS_TASK_FUNCTION_WRAPPER`."
  consequence: "The coredump setting is the one that matters here: this firmware builds with `CONFIG_ESP_CONSOLE_NONE`, so a panic prints nothing and the coredump partition at 0x7E0000 is the only record a crash leaves (EMU-021 depends on it). A scratch experiment that deletes `sdkconfig` can therefore remove the project's only crash reporting and say nothing at all. The safe pattern is to copy `sdkconfig` aside before a configuration experiment and restore the copy afterwards rather than regenerate, and to confirm the restore by comparing the built image size against the pre-experiment build -- here the difference was 18 KB, which is what surfaced the loss. Values that must survive belong in `sdkconfig.defaults`, because that file is the only one that reproduces them."
  sources:
    - "ports/twatchultra/sdkconfig and sdkconfig.defaults: the two files' diff after a regeneration"
    - "ESP-IDF 5.3.1 build system: sdkconfig is generated from defaults only when it does not exist"
  verification: "Observed on this project's tools 2026-11-21 (core-log entry 90): after `rm -f sdkconfig && idf.py build` the generated file differed from the preserved copy in 40 lines, the app image came out 18432 bytes smaller, and the two settings above had flipped. Restoring the preserved copy rebuilt to the identical size and `mem heap` returned to 259083 total / 27467 free."

- record_id: EMU-023
  kind: EMU
  topic_id: EMU
  status: VERIFIED
  verified_date: 2026-11-23
  title: "Slot B gives the Game Boy 6.6 percent of frame time and removes 334 us of preemption per frame, and moving the blitter to the idle core costs 14 percent of displayed frames"
  statement: "Measured on the same cartridge (Zelda, The default ROM), 70 seconds per run, two to three runs per arm, reading average WORK wall time per frame, average CPU consumed by the guest, and their difference -- time the guest had work to do and was not on a processor -- plus the count of guest frames against frames actually pushed to the panel. With the desktop running: wall 11427 us, CPU 11096 us, lost 334 us, 29 of ~3900 frames over the console's 16.742 ms budget. In slot B with both emulator tasks on core 1: wall 10670 us, CPU 10695 us, lost 0 us, 7 frames over budget, and blits equal to guest frames with the frame pool never starved. Moving the gbblit task to the otherwise-idle core 0: wall 9836 us but blits 3621 against 4199 guest frames and 3749 pool-dry events, so 14 percent of produced frames never reached the glass."
  consequence: "Slot B is worth real time -- 6.6 percent per frame, and the elimination of all preemption, because on the desktop the guest shares its core with the desktop's own UI task at equal priority. The 334 us figure is what that contention costs and it is the number that justified the two-slot architecture. The BLITTER MUST STAY ON THE GUEST'S CORE even though the other core is idle: running the two concurrently makes the emulator itself faster and the picture worse, which is the trade nobody wants. The likely mechanism is PSRAM, since the framebuffer and the cartridge both live there and on one core the guest and blitter alternate rather than contending, but that is inference and not measured. The lesson worth generalising is that guest CPU time is not the target -- a change that improves the guest's own numbers while reducing blits below frames has made the product worse, so any future emulator timing must report blits against frames and the pool-dry count beside the timings."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/gb.c: the s_tot_* accumulators, gb_set_blit_core(), and the /sd/diag/gbtime.txt summary"
    - "tinydesk-twatch ports/twatchultra/main/slotb.c: gb_set_blit_core(1) with the measured reason recorded at the call"
    - "core-log entry 96 for the runs, and entries 87-90 for the earlier optimisation attempts this supersedes in method"
  verification: "Measured on hardware 2026-11-23 (core-log entry 96), operator-confirmed as playable in the slot-B arm. Repeatability was checked rather than assumed and the arms were tight: desktop wall 11421 and 11433 us, slot B core-1 wall 10672, 10674 and 10661 us, slot B core-0 wall 9851 and 9848 us. The counters that changed the conclusion were added after the first core-0 run appeared to be a large win on timings alone."

- record_id: IMU-008
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "The IMU timestamp is a delta added to a running counter, and its tick is about 15.59 us (64130 ticks per second), measured from the physical accelerometer"
  statement: "In the BHy2 FIFO, a `BHY2_SYS_ID_TS_LARGE_DELTA` (252) or `..._WU` (246) entry is followed by a 16-bit little-endian value, and bhy2.c's parser applies it as `*time_stamp += BHY2_LE2U16(buffer + pos + 1)` -- the field is a DELTA added to a running 64-bit per-FIFO counter, not an absolute timestamp. A small-delta entry (251/245) adds a single byte the same way, and `BHY2_SYS_ID_TS_FULL` (253) carries an absolute value with its own encoding. Consecutive parsed timestamps for the physical accelerometer configured at 50 Hz advanced by 1282, 1283, 1282, 1283, 1282, 1283, 1283, 1282, 1283, 1282, 1283 ticks across twelve samples, so 20 ms is 1283 ticks and the tick is 20 ms / 1283 = 15.59 us, i.e. 64130 ticks per second."
  consequence: "Injecting a timestamp means writing a DELTA per block, not an absolute time: at the 100 Hz injection rate used here that is 641 ticks per block, and the counter must be allowed to wrap modulo 2^16 because only the low 16 bits fit the field. Any value that is absolute, that resets to zero, or that is off in scale makes the engine's notion of elapsed time wrong, which matters for an algorithm that segments motion by duration. The unit is not documented in the BHy2 headers this port vendors, so it must be re-measured with `imu clock` if a firmware image or configured rate changes."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/bhy2/bhy2.c, parse_fifo: BHY2_SYS_ID_TS_LARGE_DELTA / TS_SMALL_DELTA / TS_FULL handling"
    - "tinydesk-twatch ports/twatchultra/components/bhy2/include/bhy2_defs.h (BHY2_SYS_ID_TS_LARGE_DELTA 252, _WU 246, TS_SMALL_DELTA 251, TS_FULL 253)"
    - "tinydesk-twatch ports/twatchultra/main/imu.c: on_accel timestamp capture and the `imu clock` subcommand"
  verification: "Measured on hardware 2026-11-18 (core-log entry 76) with `imu clock`, which configures ONLY the physical accelerometer at 50 Hz and reads the SDK's parsed timestamp for consecutive samples -- deliberately outside any injection, because the injected deltas are what is in question and an injected run would be circular. Ten consecutive intervals all reported 1282 or 1283 ticks, giving 64130 ticks/s. The consequence was then applied: the injected delta was changed from an absolute 10000-per-block counter that reset at 65000 to a 641-per-block delta with natural 16-bit wraparound. Four subsequent runs with the corrected delta peaked at 20/40/40/40, statistically unchanged from the 20/40/40 baseline, so the fix is correct and is not what blocks learning."


- record_id: IMU-009
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "The injected gesture waveform rotated gravity in the y-z plane but put the gyro rate on Y; the consistent axis is X"
  statement: "The gesture this port injects synthesises a tilt by rotating gravity in the y-z plane: the lateral component is `sin(theta) * 4096` and the vertical one `cos(theta) * 4096` in 1/4096 g, so the acceleration vector sweeps from +z toward +y as theta goes 0 to 1 radian. The angular velocity that produces that sweep is the derivative of the rotation, (dtheta, 0, 0) -- a rotation ABOUT X. The code wrote that rate into the gyro's Y component, with X held at zero."
  consequence: "Every injected block between the introduction of this waveform and entry 77 described two contradictory motions at once: an accelerometer implying a tilt about x and a gyroscope reporting a spin about y. That is not a motion a wrist can make, and an algorithm that decides whether successive movements are the same movement may reasonably reject it. `shape 4` in `imu inject` writes the rate to X and leaves the acceleration untouched, so the motion is self-consistent; the corrected axis does not raise the observed progress ceiling (both configurations peak at 40), so it is a correctness fix rather than the blocker."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/imu.c: cmd_imu_inject waveform, the ay/az/gy construction and shape 4"
    - "bhy2_defs.h / the BHI260AP FIFO sample format: accelerometer and gyroscope as three int16 per axis, 1/4096 g and 16.4 LSB per dps"
  verification: "Verified by inspection and on hardware 2026-11-18 (core-log entry 77). The physics is elementary: differentiating (0, sin theta, cos theta) with respect to theta gives (0, cos theta, -sin theta), which is the velocity field of a rotation about x, so omega = (dtheta, 0, 0). On the board, six alternating runs in one boot gave shape 0 peaks of 20, 40 and 40 and shape 4 peaks of 40, 40 and 40 -- both capped at 40, with zero FIFO overflows, zero sensor errors and read_pattern rc -2 throughout. Separately established on the host in the same entry: the existing float-phase loop already yields cycles that differ by at most 1 LSB of 4096 on 0.8 percent of samples, so cycle-to-cycle repeatability was never a plausible cause and no hardware test of it was run."

- record_id: IMU-010
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-18
  title: "A hand-trained gesture learns and reads back a 157-byte pattern, but the same gesture recorded and replayed through injection does not"
  statement: "The two paths to the KLIO engine are not equivalent on this firmware. Live: `klio learn` with the accelerometer and gyroscope at 100 Hz reaches progress 100 percent, reports 12 frames from the engine, announces a learnt pattern, and `klio detect` then reports `read_pattern: ok (0), 157 bytes`. Injected: the same gesture, recorded at 100 Hz by `imu record` (six int16 per sample -- ax, ay, az, gx, gy, gz, the part's own integers) and replayed by `imu inject ... 5` one sample per block through the same step-by-step path, was run five times and gave peaks of 20, 60, 40, 40 and 20 percent with 3, 27, 26, 23 and 12 frames; `read_pattern` returned rc -2 with a 244-byte buffer on all five. Every block was accepted with error 0x00 in both paths, and no FIFO overflow or sensor error occurred in any replay."
  consequence: "The waveform's content is not what separates the two paths, so further host-side waveform design is not supported by evidence. The differences that remain are enumerable: the sensor path uses the part's own hardware timestamping while injection supplies a delta the host reconstructs; the sensor path delivers samples as the part's sensors produce them while injection writes blocks the host composed; and the sensor path runs the engine against a free-running FIFO while injection drives it step-by-step. The step-by-step mode is the largest of the three and the only one not yet tested, and it is a single-variable change."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/imu.c: `imu record` (on_accel and on_gyro sinks, gesture.bin writer) and shape 5 (replay path)"
    - "tinydesk-twatch ports/twatchultra/main/klio.c: `klio learn` and `klio detect` (imu_stream_on at 100 Hz, on_klio, read_pattern)"
    - "Bosch BHy2CLI flow, which injects in step-by-step mode (dmode s ... dmode n)"
  verification: "Verified on hardware 2026-11-18 (core-log entry 78), both paths on the same image and the same board. The recording itself was validated off the chip rather than from the firmware's report: 3111 accelerometer and 3111 gyroscope samples for 3000 ticks, |a| = 0.97 g, all six axes swinging by thousands of counts, and the accelerometer and gyroscope mutually consistent -- the real gesture rotates gravity in the x-z plane with its rate on gy, which is a rotation about y, whereas the synthetic waveform rotated y-z (about x) while writing gy. The 60 percent peak is recorded as a single sample of a noisy distribution, not as an improvement: the same command produced 3 and 27 frames on different runs."

- record_id: IMU-011
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-20
  title: "The LEARN slot and the recognition table are separate, and a pattern survives only in the second"
  statement: "A learnt pattern lives in a LEARN slot that `bhy2_klio_read_pattern()` reads, and bhy2_klio.h documents that id as \"only 0 supported now\". Once the blob has been written into the RECOGNITION table with `bhy2_klio_write_pattern()` and enabled with `bhy2_klio_set_pattern_states()`, the learn slot is consumed: a later `read_pattern(0)` returns -2 (`BHY2_E_INVALID_PARAM`) with a driver status of 3, which bhy2_klio_defs.h names `KLIO_DRIVER_ERROR_INVALID_PATTERN_OPERATION` and describes as trying to read a learnt pattern when no pattern has been learnt. The recognition table itself is in the part's RAM and is destroyed by a firmware reload, because the host uploads that firmware at every start (IMU-001); it is also destroyed by any `bhy2_klio_set_state()` call that sets `learning_reset`, which bhy2_klio.h states \"removes any patterns loaded for recognition\". The written pattern is 157 bytes on this firmware, and `KLIO_PARAM_RECOGNITION_MAX_PATTERNS` reports 25 usable slots."
  consequence: "Two things follow, and both were measured rather than assumed. First, `read_pattern` CANNOT TEST a restored pattern, because a restored pattern never passes through the learn slot -- an instrument that begins with read_pattern reports \"no learnt pattern\" against a pattern that is loaded, enabled and matching, and `klio recognise` exists because of this. Second, persistence must keep the 157-byte blob outside the part and write it back after every firmware boot, which is what `klio_store.c` and `klio_after_boot()` do; and any future re-learn on this engine wipes the restored table, so a session that learns a new gesture must reload the store afterwards."
  sources:
    - "tinydesk-twatch ports/twatchultra/components/bhy2/include/bhy2_klio.h: read_pattern (\"only 0 supported now\"), write_pattern (written patterns are disabled until enabled), set_state (a reset removes patterns loaded for recognition)"
    - "tinydesk-twatch ports/twatchultra/components/bhy2/include/bhy2_klio_defs.h: KLIO_DRIVER_ERROR_INVALID_PATTERN_OPERATION = 3, and the parameter ids KLIO_PARAM_RECOGNITION_MAX_PATTERNS = 8 and KLIO_PARAM_PATTERN_BLOB_SIZE = 7 (ids, not values -- the values this part reports are 25 slots and 157 bytes)"
    - "tinydesk-twatch ports/twatchultra/main/klio_store.c and klio.c: klio_store_format header and klio_after_boot()"
  verification: "Verified on hardware 2026-11-20 (core-log entry 79). The learn slot's consumption was observed directly: after `klio save 0` succeeded, `klio detect` printed `driver status: 3` and `read_pattern: FAILED (-2), 244 bytes` while the same pattern was demonstrably live. The restore path was then proven in both directions -- a learned gesture, saved, rebooted and restored automatically (`restored 1 of 1 pattern(s) after boot`), matched with `klio recognise` reporting pattern 0 and its count climbing to 14.1 over 115 frames; and after `klio forget 0` recognition refused with nothing loaded, and a further reboot logged no restore. The part reported `max patterns 25` and `pattern blob size 157` in `klio status` on this image."

- record_id: IMU-012
  kind: IMU
  topic_id: IMU
  status: VERIFIED
  verified_date: 2026-11-20
  title: "KLIO learns to 100 percent with accel and gyro at 100 Hz and orientation off; orientation sharing the FIFO is what overflows it"
  statement: "A learning pass on this board reaches progress 100 and the engine reports a learnt pattern when the accelerometer and gyroscope are both configured at 100 Hz and the orientation sensors (DEVICE_ORI, DEVICE_ORI_WU, which the IMU window configures at 50 Hz to give its heading readout) are left OFF for the duration. The FIFO is emptied before the pass begins, the report watermark is set to 32 bytes, and the KLIO virtual sensor is subscribed BEFORE it is configured so the first frames are not missed. Under that configuration a hand-performed gesture reached 100 percent with no FIFO overflow and no sensor error. The orientation sensors are the difference: they stream at 50 Hz into the same one FIFO the injected and algorithm traffic uses, which is the mix that produced `meta 12 fifo overflow` in entries 73 and 74."
  consequence: "Anything that drives a learning pass must take the FIFO for itself for the duration rather than sharing it with a heading readout. That includes the IMU window, which is why its sensor setup defers to a running pass instead of reconfiguring the accelerometer to 50 Hz and turning orientation back on -- doing so mid-pass would both change the rate the engine expects and refill the FIFO with a second stream. Orientation comes back when the pass ends."
  sources:
    - "tinydesk-twatch ports/twatchultra/main/klio.c: klio_learn_begin (quiesce, accel 100 Hz, gyro 100 Hz, wmark 32, subscribe, then configure KLIO at 50 Hz) and learn_keep_pattern"
    - "tinydesk-twatch ports/twatchultra/main/imu.c: ops_start, which returns early while klio_learn_active(), and ori_enable"
    - "tinydesk-twatch ports/twatchultra/components/bhy2/include/bhy2_defs.h: BHY2_SENSOR_ID_DEVICE_ORI and _WU, BHY2_SENSOR_ID_KLIO"
  verification: "Verified on hardware 2026-11-20 (core-log entry 80). The operator pressed the IMU window's Learn button and performed the gesture; the bar filled and the window reported `learned and kept`, `klio stored` then reported slot 0 at 157 bytes, and after a reboot the pattern was restored automatically and matched over 88 frames from the engine. No FIFO overflow or sensor error appeared in that pass. The contrasting configuration -- orientation left streaming during algorithm work -- is the one entries 73 and 74 measured overflowing."

- record_id: AUD-003
  kind: AUD
  topic_id: AUD
  status: VERIFIED
  verified_date: 2026-11-20
  title: "An I2S DMA descriptor is capped at 4092 bytes, taken from the internal pool the display's SPI bus also draws on"
  statement: "In ESP-IDF 5.3.1's `esp_driver_i2s`, a descriptor's DMA buffer is capped by `I2S_DMA_BUFFER_MAX_SIZE` (4092 bytes): `i2s_get_buf_size()` computes `dma_frame_num * bytes_per_frame` and, above that ceiling, clamps the frame count and warns `dma frame num is out of dma buffer size, limited to N`. For stereo 16-bit that is 4 bytes a frame, so the practical maximum is 1023 frames a descriptor regardless of what is asked for. The buffers are allocated one `heap_caps_calloc` each, from `I2S_MEM_ALLOC_CAPS` (`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`) and `I2S_DMA_ALLOC_CAPS` (`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT`), and `i2s_std_set_slot()` frees and reallocates ALL of them whenever the computed buffer size changes -- which a mono/stereo mode change does. `i2s_alloc_dma_buffer` failing surfaces as `E i2s_common: ... allocate DMA buffer failed` followed by `E i2s_std: i2s_std_set_slot` and `i2s_channel_reconfigure`, and the channel is left unusable."
  consequence: "The descriptor count and frame count are a MEMORY BUDGET, not a latency tuning knob: four descriptors of 1024 frames is 16,368 bytes of internal DMA-capable RAM in one shot, and on this board with the emulator running that pool measured 12,251 bytes free with a largest contiguous block of 3,968, so the allocation could not succeed at all. This port therefore runs four descriptors of 256 frames (4,096 bytes total), which sets the ring at 1,024 frames -- 21 ms at 48 kHz, 23 ms at 43,691 Hz. THE DISPLAY COMPETES FOR THE SAME POOL: `spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO)` allocates from the same internal DMA heap, so a shortage presents as whichever subsystem loses the allocation -- an I2S failure gives a silent picture with sound, an SPI failure gives a frozen picture with sound. Any new consumer of internal DMA RAM on this board should be sized against `mem heap`'s DMA line rather than against the part's maximum."
  sources:
    - "ESP-IDF 5.3.1 components/esp_driver_i2s/i2s_common.c: I2S_DMA_BUFFER_MAX_SIZE clamp in i2s_get_buf_size(), i2s_alloc_dma_desc() and i2s_free_dma_desc()"
    - "ESP-IDF 5.3.1 components/esp_driver_i2s/i2s_std.c: i2s_std_set_slot() frees and reallocates the descriptor set when the buffer size changes"
    - "ESP-IDF 5.3.1 components/esp_driver_i2s/i2s_private.h: I2S_MEM_ALLOC_CAPS and I2S_DMA_ALLOC_CAPS"
    - "tinydesk-twatch ports/twatchultra/main/sound.c: sound_init() descriptor and frame counts, and set_std() which reconfigures the slot on every rate change"
  verification: "Verified on hardware 2026-11-20 (core-log entry 82). Two failing runs of the Game Boy emulator produced the four I2S lines above -- one with sound and a frozen picture, one with a moving picture and no sound -- and `mem heap` with the emulator running reported the DMA pool at 12,251 bytes free with a 3,968 largest block, which is smaller than a single clamped 4,092-byte buffer. After the frame count was reduced to 256, a cold boot reported no I2S lines at all, `gb status` showed `audio=on 43691Hz` where the channel could not be configured before, and the operator confirmed picture and sound both running. The shared consumer was checked rather than assumed: `music` on a 48 kHz tone still reports 192,000 frames, 0 errors, 0 short writes with the shorter ring."

- record_id: TOL-009
  kind: TOOLCHAIN
  topic_id: TOL
  status: VERIFIED
  verified_date: 2026-11-23
  title: "A weak definition that is already in the link satisfies every reference to it, so a port's strong override in an archive member is never fetched and is silently absent from the image"
  statement: "GNU ld's archive-member selection is driven only by references that are still undefined when the archive is scanned. A weak definition does not have to come from an archive to satisfy a reference: if the object that defines it is already included in the link, every call to the symbol binds to that weak definition, no undefined reference remains, and the linker never searches the archives for a strong one. ESP-IDF links a component's sources into a static library -- this port's into libmain.a -- so a `__attribute__((weak))` no-op written beside `app_main` in main.c suppresses a board's strong implementation in another translation unit of the same component. The link succeeds with no diagnostic of any kind."
  consequence: "This port deliberately uses weak board hooks so one shared application links against every board in the tree: `board_apps_register`, `board_imu_ops`, `board_maintenance_start`, and `board_startup_override`. A port that defines one strongly gets nothing unless the archive member is forced out, and the failure presents as the feature simply not working while the firmware builds, flashes and boots normally -- on this board that hid an entire boot personality. The mechanism is `target_link_options(<component> PUBLIC \"-Wl,--undefined=<a symbol only that member defines>\")`, or any call from an object that is always linked. Confirm a hook is live with `nm <elf> | grep <symbol>`: `T` is a linked strong definition and `W` is the weak fallback, so `W` means the port's code did not run. Apply the same check to any other weak board hook whose implementation lives outside main.c."
  sources:
    - "GNU binutils ld documentation, weak symbols and archive-member inclusion"
    - "tinydesk-twatch ports/esp_idf/app/main.c: __attribute__((weak)) board_startup_override stub, and the call in app_main()"
    - "tinydesk-twatch ports/twatchultra/main/CMakeLists.txt: target_link_options(... -Wl,--undefined=slotb_force_link)"
  verification: "Established on this project's tools 2026-11-23 (core-log entry 91). The first build containing ports/twatchultra/main/slotb.c linked `board_startup_override` as `W` at an address inside main.o and `slotb_mark` was absent from the ELF entirely; the board booted the desktop from the other OTA slot and no log, coredump or build warning mentioned slot B. Adding the `--undefined` link option changed the symbol to `T`, and slot B then ran and handed the boot back."

```

---

## 6. Record template

```yaml
- record_id: <TOPIC>-<NNN>
  kind: SOC | BOARD | DISPLAY | PMU | RTC | KERNEL | FLASH | JFFS2 | BLE | USB | NETWORK | TOOLCHAIN | EXTERNAL | EMU
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
last_reviewed: 2026-11-18
```

---

## 8. Retired record ids

| record_id | Folded into | Why |
|---|---|---|
| — | — | No records retired yet. |
