# ESCAPEMENT TECHNICAL REFERENCE

> **Project:** Escapement
> **Purpose:** The authoritative lookup for externally defined facts this project depends on: the T-Watch Ultra's wiring and fitted parts, the ESP32-S3's memory and boot coupling, the CO5300 panel's controller behaviour and fbdev contract, the AXP2101 power management, the Xtensa/uClibc toolchain, and the external projects embedded here.
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
```

---

## 3. Active routing

| Question | Consult first | Fast records |
|---|---|---|
| What SoC, RAM and PSRAM mode does this watch have? | SOC | SOC-001 |
| Why does the stock upstream image abort before Linux starts? | SOC | SOC-002 |
| Where is the console, and when does Linux take it? | SOC | SOC-003 |
| Which I2C bus carries the peripherals, on which pins, at which addresses? | BRD | BRD-001 |
| Which PMU interrupt pin is it, and why did it storm? | BRD | BRD-002 |
| What does the power rail map look like? | BRD | BRD-003 |
| Which SPI bus is the panel on, and what else exists? | BRD | BRD-004 |
| Where is the touch controller, and what is at 0x5A? | TCH | TCH-001 |
| Why does the touch controller answer but never report? | TCH | TCH-002 |
| Why does an untouched screen read as a phantom press? | TCH | TCH-003 |
| Why does a held touch freeze, then report a release? | TCH | TCH-003 |
| Why are taps at the top of a bar ignored? | TCH | TCH-004 |
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
| Where does the RTC's time come from and go to? | RTC | RTC-001 |
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

BRD-001: "All peripherals are on one bit-banged I2C bus (i2c-gpio, SDA pin 3, SCL pin 2, kernel lines 579/578) at 100 kHz: AXP2101 0x34, XL9555 0x20, PCF85063A 0x51, DRV2605 0x5A, CST9217 0x1A"
BRD-002: "The AXP2101 interrupt is GPIO7, open drain, active low, and MUST have input-enable plus an internal pull-up (PIN(7) FUN0_20MA_IE_WPU); a level IRQ on the unpulled pin storms, hangs the boot and starves the bit-banged I2C bus"
BRD-003: "Rail map: ALDO1 SD card, ALDO2 display, ALDO3 LoRa, ALDO4 sensor, BLDO1 GNSS, BLDO2 speaker, DLDO1 NFC, LDO1 GNSS backup; the display rail is additionally gated by the XL9555 at 0x20"
BRD-004: "The panel is on SPI2 (SCK 40, D0-D3 38/39/42/45, CS 41 driven by hand, RST 37, TE 6); a second general-purpose SPI bus exists on SCK 35 / MOSI 34 / MISO 33 with SD CS 21 and LoRa CS 36, but it has never been proven to transfer a byte"

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
TCH-001: "The touch controller is a CST9217 at I2C 0x1A (register 0x0000 reads 0xCA, the family checkcode); 0x5A is the DRV2605 haptic driver, whose register 0x00 reads 0xE0 (DEVICE_ID bits), and only 0x1A completes the CST92xx unlock handshake"
TCH-002: "A reset CST9217 is in command mode and reports nothing: write 0xD1=0x01, then 0xD1=0x1E twice, require byte 1 of a read of 0x0002 to be 0x1E, then write 0xD1=0x09 for normal mode"
TCH-003: "Report block at 0xD000 is 15 bytes and the transfer must be all 15 even though only 7 are decoded; byte 0 = 0x06 for a down event and 0xAB means no new report; x=(b1<<4)|(b3>>4), y=(b2<<4)|(b3&0x0F); a short read leaves the ack mid-block so a held touch freezes"
TCH-004: "The strip threshold must sit above the painted bar edge: taps on a bar decode to panel Y a few px high near the bottom (measured 442..443 against a bar painted from 445), so the threshold is set to 440"

PMU-001: "AXP2101 capacity is register 0xa4 bits 6:0 (0x64 = 100, above 100 = not settled); voltage is registers 0x34/0x35, 14 bits, already in mV, and the power_supply property is uV so multiply by 1000"
PMU-002: "The AXP2101 has no mainline driver, so it lives in patches/kernel/axp2101/; a non-recursive patch glob plus '|| true' silently dropped the series and a rebuild produced a kernel with no PMU driver and no error"
PMU-003: "CONFIG_AXP20X_POWER publishes /sys/class/power_supply/axp20x-usb and is off by default; its absence is silent because axp20x-battery still appears"

RTC-001: "The PCF85063A at I2C 0x51 registers as rtc0 and, with CONFIG_RTC_HCTOSYS, seeds the system clock before init; its interrupt shares GPIO bank 0 with the PMU line"
RTC-002: "The image ships no /etc/TZ, /etc/localtime or /usr/share/zoneinfo, so `date` prints UTC even when the clock is right; /etc/TZ containing MST7 (UTC-7 all year, no DST) is the whole fix for uClibc-ng"
RTC-003: "The upstream image's /etc/init.d/S01clock saves the epoch to /home/.clock at shutdown and overwrites the RTC-seeded clock at boot; the board has one RTC (rtc0 at 0x51), so it is a second mechanism, not a second clock, and this project disables it by moving it out of /etc/init.d because S??* still matches .disabled"
FLS-003: "A full-image flash rewrites /etc and S03keepconfig carries back only seven named files plus dropbear keys, so project-added /etc files must be re-installed by scripts/watch-time.sh after one; a kernel-only flash leaves /etc alone"

KRN-001: "NOMMU with no overcommit: every ELF load segment and every large allocation must be CONTIGUOUS, so 'nommu: Allocation of length N failed' is a contiguity failure, not an out-of-memory one, and a big static array blocks exec rather than failing at run time"
KRN-002: "init_linux_mmap() extends the linear flash window over the whole 16 MiB only inside map_flash_and_go(), immediately before the jump to Linux; firmware dereferencing 0x42xx0000 earlier reads unmapped memory and reset-loops with no output"
KRN-003: "The kernel is linked against a fixed address in the firmware's RAM (CONFIG_VECTORS_ADDR) which is the linker symbol space_for_vectors; moving it via a firmware change gives a kernel that boots and then dies silently at 'Run /sbin/init'"

FLS-001: "16 MiB layout: nvs 0xa000/0x5000, phy_init 0xf000/0x1000, factory 0x10000/0xc0000, etc 0xd0000/0x70000, linux 0x140000/0x400000, rootfs 0x540000/0x780000, home 0xcc0000/0x340000; xipImage is stored RAW at 0x140000 and can be written alone"
FLS-002: "The release image is not this project's state: its kernel has no CO5300 driver, its firmware requires octal PSRAM, and its /etc has no WiFi credentials; flashing it costs the display, the login, and the credentials"

JFS-001: "Firmware must never write flash after Linux starts: an NVS write reaches spi_flash_disable_interrupts_caches_and_other_cpu(), which parks the other core, and that core is running Linux, so it aborts in esp_ipc_call_and_wait"
JFS-002: "The firmware's jffs2 reader can read files Linux wrote, including compressible ones, verified byte-exact against a dumped partition; it returns -1 for any compressed node, so mounts should carry compr=none"

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
  consequence: "The split is the architecture of this project: firmware is 'core 0' and Linux is 'core 1' throughout. Anything the firmware does that needs the other core to cooperate must account for that core running Linux rather than a FreeRTOS task loop — which is why the firmware cannot write flash once Linux is up (JFS-001)."
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
  title: "The panel owns SPI2; a second general-purpose SPI bus carries SD and LoRa but has never been proven to transfer data"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "The CO5300 panel is on a dedicated QSPI bus: SCK 40, D0-D3 on 38/39/42/45, CS driven by hand on GPIO41, RST on 37, TE on 6. A separate general-purpose SPI bus exists on SCK 35, MOSI 34, MISO 33, with SD CS on 21, LoRa CS on 36 (RST 47, BUSY 48, INT 14) and NFC CS on 4 (INT 5)."
  consequence: "Work on the second bus is unstarted and unproven: the NFC probe never received a reply, with MISO clamped low by something external, and the loopback test that would have proven the bus could move a byte was itself blocked by a pad-driving bug. The SD card and LoRa therefore rest on unverified foundations, and the kernel has no MMC support at all (CONFIG_MMC is not set)."
  sources:
    - "patches/kernel/display/SECOND-SPI-BUS.md"
    - "build/kernel/linux-7.2.4/.config: '# CONFIG_MMC is not set'"
    - "patches/kernel/display/final/esp32s3-devkit-c1.dts for the panel pin set"
  verification: "Panel pins confirmed working by the display itself. Second bus: read from the probe notes, which record the negative result and the never-completed loopback; not re-exercised this session."

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
  statement: "After reset the controller sits in command mode, where register reads succeed and return zeros but it is not scanning the glass. It has to be brought up in this order: write 0xD1 = 0x01 (command mode), write 0xD1 = 0x1E twice (unlock), read 0x0002 for four bytes and require byte 1 to be 0x1E as an acknowledgement, then write 0xD1 = 0x09 (normal mode). Reading 0x0002 after the sequence returns 0x00 0x09, which is the mode byte confirming normal mode."
  consequence: "Skipping any of this produces a controller that answers every transaction perfectly and never reports a touch, which looks exactly like a working sensor nobody has touched. The acknowledgement read must be checked rather than assumed, because a controller that did not unlock ignores the mode write silently."
  sources:
    - "TinyPetz firmware/bringup/main/touch.c: touch_init() and its setMode sequence, following LilyGo TouchDrvCST92xx in SensorLib"
    - "on the watch: the sequence issued with i2ctransfer, then `i2ctransfer -y 0 w2@0x1a 0x00 0x02 r4` returning 0x00 0x09 0x00 0x00"
  verification: "Verified on hardware by running the sequence over /dev/i2c-0 with i2ctransfer and observing the acknowledgement and the 0x09 mode read, then reading a live down report from the button strip."

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
  consequence: "The rule that holds on hardware is that a single transfer must carry few rows, so `CO5300_IOC_WRITE_RECT` caps every band at two rows and loops. That is invisible to callers -- userspace still hands over one rectangle -- and it is cheap here because these are status icons, but it is a workaround and not a fix: it would be wrong for continuous drawing, which is why `co5300_blit()` keeps its own uncapped sizing. The root cause is NOT established. The strongest remaining suspect is SPI clock margin, because the reference firmware recorded the identical signature at 45 MHz -- 'dark horizontal lines shearing slightly down to the right' -- cured by dropping to 20 MHz, and this project runs at 20 MHz where that signature is still marginal for long narrow transfers."
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
  consequence: "`console_loglevel` decides whether this happens at all, so the fix is a sysctl rather than a driver change: `/etc/sysctl.conf` now sets `kernel.printk = 1 4 1 7`, and emergencies only reach the panel. Any fullscreen app on this board needs that, or needs to raise and lower the level itself around its run. It hides the NAK message rather than stopping the NAKs, which are a property of the touch controller (TCH-003)."
  sources:
    - "on the watch: `/proc/sys/kernel/printk` reading 7 4 1 7 while the emulator ran, with NAK lines arriving continuously"
    - "docs/tamagotchi.md: the flicker and the sysctl"
  verification: "Observed on hardware 2026-11-09 with the Tamagotchi emulator running: the operator reported the console taking over the frame each time it printed. Dropping the level to 1 stopped it immediately and the game held the panel. The message level was identified by trying 3 first, which did not help because NAK bailout is KERN_ERR."

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
  verification: "Reproduced repeatedly on hardware: a statically embedded 512 KB ROM failed to exec after a few minutes of uptime and succeeded immediately after a reboot; the same payload read into a runtime allocation started, and then reported its own failure when the allocation could not be met."

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
  title: "Firmware must never write flash after Linux starts, because parking core 1 aborts"
  status: VERIFIED
  verified_date: 2026-11-09
  statement: "A flash write from core 0 takes spi_flash_disable_interrupts_caches_and_other_cpu(), which must park the other core first. Core 1 is running Linux, not a FreeRTOS task loop, so the inter-core call never completes and the firmware aborts: esp_ipc_call_and_wait, called from the cache-disable path, reaches abort() with 'abort() was called at PC 0x42023b2e on core 0'."
  consequence: "Any firmware persistence must be read-only. Bonds, settings or state the firmware needs are written by Linux to a jffs2 partition and read by the firmware at boot; the firmware cannot write them back. NVS is therefore unusable for anything the firmware must update at run time, even though it is otherwise the natural place."
  sources:
    - "backtrace on hardware: abort <- esp_ipc_call_and_wait <- spi_flash_disable_interrupts_caches_and_other_cpu <- esp_partition_write <- nvs::Page::writeItem <- nvs_set_blob"
    - "docs/bluetooth-bond.md: 'Bonds cannot be persisted here, and that is a hardware limit'"
  verification: "Reproduced on hardware by enabling CONFIG_BT_NIMBLE_NVS_PERSIST: the board reset-looped at about 3.3 s with the backtrace above. Disabling it restored normal boot."

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
```

---

## 6. Record template

```yaml
- record_id: <TOPIC>-<NNN>
  kind: SOC | BOARD | DISPLAY | PMU | RTC | KERNEL | FLASH | JFFS2 | BLE | NETWORK | TOOLCHAIN | EXTERNAL
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
last_reviewed: 2026-11-09
```

---

## 8. Retired record ids

| record_id | Folded into | Why |
|---|---|---|
| — | — | No records retired yet. |
