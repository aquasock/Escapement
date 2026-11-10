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
