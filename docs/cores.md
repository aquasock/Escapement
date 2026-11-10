# Where code runs, and why

The ESP32-S3 has two Xtensa LX7 cores at 240 MHz. This document fixes what runs
on each, and — more importantly — what may *not* be moved across, with the
evidence for each decision. It exists because the split looks arbitrary from
outside and gets re-proposed every few sessions.

Two proposals come up repeatedly and both are answered here:

1. "Put the RTOS in charge of hardware and let Linux do software, with a fast
   link between them."
2. "Linux is only using one core; use the other one for the work."

---

## What the port actually does

The upstream port this project builds on documents its own split in
`build/Linux-on-esp32-S3/ARCHITECTURE.md`, and the firmware source confirms it:

```
Core 0 — ESP-IDF / FreeRTOS ("network_adapter")
  owns the WiFi radio (closed binary blob, ESP-IDF only)
  NimBLE (BLE HID host, BLE provisioning byte pipe)
  esp-hosted shmem link
  our firmware additions: bond load, HID report handling, log pipe

Core 1 — Linux 7.2.4 (native Xtensa, not emulated)
  everything else: the CO5300 panel, GPIO, I2C, USB, the filesystem
```

`linux_boot.c` makes the boundary concrete and blunt:

```c
xTaskCreatePinnedToCore(linux_task, "linux_task", 4096, NULL, 5, NULL, 1);

static void linux_task(void *p)
{
        map_flash_and_go();
        esp_restart();          /* only reached if Linux ever returns */
}
```

**Linux is a FreeRTOS task pinned to core 1 that never yields.** It is not a
separate system with a driver protocol; once it starts, core 1 is gone from
FreeRTOS's point of view. `esp_restart()` is the "Linux exited" path.

**The split is not a design principle. It is forced by one binary blob.** The
WiFi driver is closed and only runs under ESP-IDF, so the radio has to live on
core 0. Everything else — the panel, the PMU, the RTC, the touch controller,
the haptics — Linux owns directly, through its own drivers, over its own MMIO.
That is what this port *is*.

### What the link between them can and cannot do

`esp-hosted` carries, and only carries:

- a network interface (`espsta0`)
- Bluetooth (the HID host runs in firmware)
- custom typed interfaces, of which this project added one, `ESP_LOG_IF`, for
  firmware logs

There is **no generic peripheral channel**. No I2C RPC, no SPI RPC, no GPIO
RPC. Adding one would mean inventing a protocol over a link that has already
proved fragile: NET-003 records that a mismatch in one enum silently dropped
every log frame, and BLE-005 records that logging one line per BLE
advertisement — mere traffic — stopped the WiFi interface from coming up at
all. The link is a narrow, contended resource, not a bus.

---

## What LilyGo does

`Xinyuan-LilyGO/LilyGoLib` is the official library for this board and its
factory firmware is the most complete example of the hardware being driven.

**LilyGo runs FreeRTOS on both cores. There is no OS boundary and no IPC.**
It is one address space, one scheduler, one build:

- core 1 (or whichever `ARDUINO_RUNNING_CORE` selects): `setup()`/`loop()`, the
  LVGL UI, the touch and display refresh
- core 0: WiFi and BLE, plus everything heavy they pin there explicitly —
  the **NES emulator** (`xTaskCreatePinnedToCore(nes_task, ..., 0)`), its audio
  task, the LoRaWAN join task, the BLE HID helper
- and where a task must not disturb the UI, they pin it to the *other* core
  explicitly: `screen_web_server.cpp` uses `ARDUINO_RUNNING_CORE == 0 ? 1 : 0`

Two things follow from that, and both matter:

**1. They do not run a hardware server either.** There is nothing to offload
*to* — the UI task and the I2C calls are in the same process. When LilyGo's
touch code calls `Wire.beginTransmission(0x1A)`, that is a direct peripheral
access from whatever task happens to be running. Their architecture says
nothing about whether a hardware server is a good idea, because it never faced
the question.

**2. They are happy to put compute on core 0 with the radios.** The NES
emulator — the most demanding thing in their firmware — is pinned to core 0
alongside WiFi and BLE. That is evidence core 0 has headroom for *compute*,
but it is not evidence for moving *peripherals* there: their emulator shares an
address space with the drivers, and ours would not.

---

## Decision: Linux owns the peripherals. Fix the driver, do not move the core.

The proposal to put hardware behind the RTOS fails on five independent
grounds, each of which is recorded elsewhere in this project:

1. **There is no channel to build it on.** `esp-hosted` has no peripheral RPC.
   Building one means a new protocol over shmem, and BLE-005 shows that a
   single extra log line can break WiFi bring-up on that link.

2. **Latency, in the direction that hurts.** Every I2C transaction would
   become a round trip: syscall → shmem enqueue → core 0 wake → peripheral →
   response → shmem → wake. The touch controller is polled at 15 ms and the
   whole point of that number is tap responsiveness. Putting an IPC hop and
   another core's scheduling in the path trades a measured constant for an
   unmeasured variable.

3. **Core 0 cannot write flash.** JFS-001: a flash write from core 0 aborts,
   because parking core 1 never completes when core 1 is running Linux. Any
   peripheral needing persistent state inherits that constraint.

4. **Core 0 is the radio core, not spare capacity.** BLE-004: an unbounded BLE
   scan starves core 0's idle task and trips the watchdog
   (`rst:0x7 TG0WDT_SYS_RST`). It is already the core that runs WiFi, BLE, the
   shmem link and our HID/bond code. A polling I2C server would contend with
   all of it, and the failure mode is a chip reset.

5. **The drivers already exist and work.** `axp20x`, `pcf85063`,
   `pca953x`, `co5300`, `gpio-esp32`, `spi-esp32` all bind and function on
   Linux today. Moving the bus behind an RPC means rewriting all of them as
   firmware — in a tree that must never be pushed (its origin is upstream
   `jcmvbkbc/esp-hosted`, not this project).

The bit-banged I2C is a real problem and this is not a defence of it. It is a
problem **because there is no driver for the hardware I2C peripheral**, not
because it runs on the wrong core. The fix is to write that driver, which
removes the CPU cost entirely rather than moving it. LilyGo's own firmware
confirms the peripheral works on our pins at 400 kHz–1 MHz against the 100 kHz
we bit-bang, so there is 4–10x on the bus as well as the CPU savings.

---

## The one place core 0 offload would be right

**Compute, not peripherals.** EMU-004 records the argument: Linux runs on the
core with no data cache and no reachable internal SRAM, at ~53 ns per dependent
PSRAM load, while core 0 owns the cache. Moving the Tamagotchi *emulator* to
core 0 and leaving a thin blitter on core 1 is the only full-speed design for
that workload.

That is a different proposal from this document's subject and it stays
deferred, for the reasons EMU-004 lists — core-0 watchdog risk, the flash-write
constraint, and an unpushable firmware tree. The distinction worth keeping:
**computation may cross the boundary; I/O should not**, because computation can
be batched into a shared buffer while I/O needs a round trip per transaction.

---

## Summary

- Core 0 runs the radios because a closed blob leaves no choice. It is not a
  hardware server and there is no channel to make it one.
- Core 1 runs Linux and owns every peripheral directly, through ordinary
  drivers. That is the port's design, inherited, not invented here.
- LilyGo runs FreeRTOS on both cores with no OS boundary, so its split answers
  a different question. Its one transferable lesson is that the hardware I2C
  works — and is faster than what we use.
- Bit-banged I2C is a missing driver, not a placement mistake. Write
  `i2c-esp32.c`.

---

## Appendix: the hardware I2C attempt, and where it stopped

Written after a session that spent eight build-and-flash cycles on it, so the
next attempt starts from evidence rather than from scratch.

**What works.** `patches/kernel/i2c/final/i2c-esp32.c` is a polling-mode
`i2c_algorithm` for I2C0 at `0x60013000`. The bus is live: `i2cdetect` returns a
credible scan (only `1a`, `20`, `34` and `51` answer, everything else absent),
single-byte reads return real data, `rtc-pcf85063` registers as `rtc0`, the
AXP2101 probes and publishes `axp20x-battery` and `axp20x-usb`, and
`axp20x-pek` appears as an input device.

**Nine defects found on the way**, every one of them by reading ESP-IDF's own
`components/driver/i2c/i2c.c` rather than its headers. That is the method that
works here; eight earlier flash cycles reconstructing from headers got nowhere.

1. **Completion polled on `command_done`,** which END never sets. Real signals
   are `int_status.trans_complete` / `.nack`.
2. **`INT_STATUS` is masked by `INT_ENA`,** which is zero in polling mode, so
   status reads 0 for ever. Poll `INT_RAW` instead.
3. **The clock gate is closed at boot** — `SYSTEM_PERIP_CLK_EN0` (0x600c0018)
   bit 7 = 0. Provided as `i2c0_clk` in the DTS, a `gpio-gate-clock` over
   `clk_gpio0` bit 7, the same mechanism `spi2_clk` already used.
4. **The peripheral is held in reset** — `SYSTEM_PERIP_RST_EN0` (0x600c0020)
   bit 7 = 1 (register `0x9eff1fb8`). While held, writes are discarded, reads
   work and the FIFO still accepts bytes, so `CTR` reads back `0x20b`. KRN-006.
5. **Timing needs `scl_wait_high_period`** (bits [15:9] of `SCL_HIGH_PERIOD`),
   and the HAL does *not* subtract one from `scl_high`.
6. **`sclk_div_num` is bits [7:0]**, not [13:8] — [13:8] is `sclk_div_a`, the
   fractional divider.
7. **The GPIO matrix needs the INPUT route too.** Only the output signal was
   set, so the controller could never read SDA and never saw an ACK. Also
   `pad_driver` must be set for open drain, or the pads drive high actively.
8. **The ACK flags are per-opcode.** RESTART and STOP take none; WRITE takes
   `ack_en`; READ takes `ack_val` only. `ack_exp` means "expect a NACK" and
   setting it everywhere breaks address ACKs.
9. **`rxfifo_cnt` is bits [13:8]**, not [10:5] — a truncated comment in
   `i2c_struct.h` says otherwise and has now misled this driver twice.

**What works now, verified on hardware and sustained.** The bus carries real
traffic and does not wedge:

```
capacity     100
voltage_now  4178000 uV
status       Full
usb online   1
rtc0         19:42:50 -> 19:42:58   (readable and ticking)
```

`i2cdetect` returns a credible scan (1a, 20, 34, 51 answer). Ten consecutive
reads with no failure. PMU, RTC and expander all bind and their sysfs values are
live. There is a verified 16 MiB backup of this state,
`working-2026-11-10-1251.bin`.

**The two bugs that made this unfixable by flashing** were both found in
`i2c_struct.h`'s field list after a dozen failed builds, and both are one line:

1. **`bus_busy` is bit 4, not bit 3.** Bit 3 is `arb_lost`. With the define one
   low, `SR_BUS_BUSY` was reading the wrong flag entirely: a genuinely stuck bus
   never triggered recovery, and the arbitration-lost path never fired. That is
   the whole "one transfer per boot" bug.
2. **Recovery must restore the timing registers.** ESP32-S3 leaves
   `SOC_I2C_SUPPORT_HW_FSM_RST` undefined (*"FSM_RST only resets the FSM, not
   using it"*), so ESP-IDF cycles the clock gate and then re-writes every timing
   register. Skipping that restore leaves the peripheral at default timing, so
   the next transfer reports `scl_main_st_to` with `arb_lost` — a bus fault that
   looks electrical and is self-inflicted.

A third correction: the one-bit `fsm_rst` is not usable on this chip at all and
was removed. And `ack_val = 1` (NACK the last byte) is correct for every read,
matching ESP-IDF's `I2C_MASTER_LAST_NACK`; the earlier "0 for multi-byte" rule
came from a test that also had the address-phase bug, so the evidence was
contaminated.

**What still fails: 16-bit register addresses.** The CST9217 touch controller
needs a two-byte register address, and that transfer shape hangs:

```
i2ctransfer w1@0x1a 0x00 r4   ->  data (8-bit address, works)
i2ctransfer w2@0x1a 0x00 0x02 r1  ->  times out
```

The register state at failure is unambiguous — `int_raw=0x200` (trans_start
only, nothing completed), `txfifo_cnt=3` (bytes never sent), `resp_rec=1` (the
address WAS acknowledged), `bus_busy=0` (bus idle). So the controller sends the
address, gets its ACK, and never chains to the next command. This affects only
the touch controller; the PMU and RTC use one-byte addresses and work.

The strongest lead is the command-register budget: a 16-bit register read needs
8 command registers and `SOC_I2C_CMD_REG_NUM` is 8, so the list may be one over
what the sequencer will chain. Either merge the address into the data WRITE, or
split the transfer.

**The board state this leaves.** Running the hardware-I2C kernel: PMU, RTC and
expander all bind and their values are live; the touch controller does not,
because of the 16-bit addressing gap above, so `tbtns` is not running. The
verified backup of exactly this state is
`~/twatch-backup/working-2026-11-10-1251.bin`. The previous image,
`working-2026-11-10-1053.bin`, is the `i2c-gpio` build and restores a fully
functional watch including touch.
