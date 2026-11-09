# Bluetooth keyboard — design, 2026-11-08

Goal: a BLE keyboard types into the watch's console.

The keyboard in question is a Logitech that speaks **Bluetooth LE only**. That
is fortunate, because the ESP32-S3 has no Bluetooth Classic at all — but it does
have a BLE radio, and so does the Logitech.

## Why the HID host lives in the firmware, not in Linux

The obvious approach — pass HCI up to Linux and let the kernel Bluetooth stack
handle it — does not work here:

  * **The kernel has no BLE HID host.** `CONFIG_BT_HIDP` is Bluetooth *Classic*
    HID. HID over GATT (HOGP) lives in BlueZ, userspace.
  * **BlueZ is not installable.** The rootfs is a read-only cramfs and there is
    no buildroot tree in this checkout, so the image cannot be rebuilt. BlueZ
    also wants D-Bus, and this is a NOMMU kernel whose `fork()` is a patch
    stack — running a modern userspace daemon tree is not on the table.

So the BLE host runs where the BLE stack already is: **on the ESP32-S3**, in the
firmware alongside `esp-hosted`. Key presses are decoded there and handed to
Linux as input events.

This is the same split TinyTang uses on its BL616 — pair on the MCU, feed the
console from there. Its `ports/bl616/tang_ble.c` is the reference, and it has
already been through the Logitech quirks.

## The transport already exists

Nothing new is needed to carry events from firmware to Linux. The port ships a
byte pipe in both directions, used today for WiFi provisioning:

```
firmware:  ble_prov_to_linux(data, len)      main/ble_prov.c
             -> interface_buffer_handle_t, if_type = ESP_BLE_PROV_IF
             -> IPC / shmem
Linux:     /dev/esp-ble                       esp_ble_prov.c
             queue 32 deep, 512-byte writes, poll() and read()
```

`ble_prov_to_linux()` is exactly the call a HID report handler needs.

## What has to be added

**Firmware.** `ble_prov.c` is currently a GATT *server*: it advertises as
`Esp32-Linux`, a phone writes WiFi credentials, and they arrive on `/dev/esp-ble`.
A keyboard needs the opposite role — a GATT *client* that initiates the
connection. Both can coexist in NimBLE.

**Roles were not even compiled in.** The target defaults had

```
# CONFIG_BT_NIMBLE_ROLE_CENTRAL is not set
# CONFIG_BT_NIMBLE_ROLE_OBSERVER is not set
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
```

Now:

```
CONFIG_BT_NIMBLE_ROLE_CENTRAL=y
CONFIG_BT_NIMBLE_ROLE_OBSERVER=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=2
```

Two connections because the keyboard and the provisioning peripheral are
separate links; one would drop the other.

Measured cost: the binary went from 678,464 to 683,504 bytes, so about 5 KiB.
Free space is 102,928 of 786,432 (13%). Cheap — the stack was already linked.

**Linux.** `CONFIG_INPUT_UINPUT`, plus a small daemon that reads `/dev/esp-ble`
and writes `input_event` structs to `/dev/uinput`. The kernel's VT keyboard
handler then picks it up: `kbd_match()` accepts any device with `EV_KEY` bits
below `BTN_MISC`, and a uinput device declaring ordinary key codes qualifies.
That is what makes keys reach `tty1` and any application, not just the shell.

**Deployment.** No buildroot, so the daemon cannot go into the cramfs. `/etc`
and `/home` are jffs2 and writable, `etc.jffs2` and `home.jffs2` are built as
separate images, and `rcS` runs every `/etc/init.d/S*`. A daemon placed on one
of those partitions and started from an init script is the path.

## The Logitech quirks, from TinyTang

Each of these cost TinyTang debugging time and is worth inheriting rather than
rediscovering:

  * **Discover unfiltered.** Filtering service discovery by HID UUID (0x1812)
    found nothing on the K950. Walk all attributes and pick the ones wanted.
  * **Subscribe by CCC, matched to the preceding value handle.** A CCC
    (0x2902) belongs to the characteristic whose value handle precedes it.
  * **Write Exit Suspend (0x01) to the HID Control Point (0x2A4C)** after
    subscribing. A device that thinks it is suspended sends no reports until
    told to resume. The K950 does not need it; an Xbox controller does. Written
    after subscribing so the device is listening when it resumes.
  * **Route by length, not by handle.** The K950 and M750 report on their report
    characteristics rather than their boot inputs, despite accepting boot
    protocol. An 8-byte report is a keyboard report whatever handle it arrives
    on; a boot mouse report is 3–4 bytes and never 8.
  * **Boot protocol is negotiated with `HID Protocol Mode` (0x2A4E)** and gives
    a fixed 8-byte keyboard report: modifiers, reserved, six key slots. Much
    simpler than report-descriptor parsing.

## Risks

  * **Radio coexistence.** BLE and WiFi share one radio on this chip, and
    `esp-hosted` is running WiFi for Linux. Both work, but BLE scan and pair are
    the fragile moments.
  * **Internal RAM.** BLE central plus a GATT client needs internal RAM, which
    is the scarce kind here. Not yet measured.
  * **Firmware changes are riskier than kernel changes.** A firmware that fails
    to boot leaves the watch looking dead, and the console is on the other side
    of it. `scripts/flash.sh --factory` is the recovery, and it is not a partial
    reflash.
  * **`ble_prov.c` is single-connection today.** It keys off one global
    `conn_handle`, which was fine for provisioning and is not fine for
    provisioning plus a keyboard.

## First attempt: the watch boot-looped, and was restored

The firmware side was written and did compile — `ble_hid.c` with the scan, pair,
HID discovery, subscribe and report-forward path, wired so that its sync ran from
`ble_prov.c`'s (there is exactly one NimBLE sync callback and `ble_prov` owns it).
It did not run.

First flash with it:

```
rst:0x8 (TG1WDT_SYS_RST)
```

A task-watchdog reset, about 4.4 s in, repeating. The kernel got as far as
`esp32s3-rsa: hardware RSA registered` and then the board reset.

Reducing the scan duty cycle from 100 % (window == interval, which starves the
idle task on a core shared with WiFi) to 50 % made no difference, so the duty
cycle was not the cause.

**Root cause is not yet established.** Two candidates, and they need separating:

  1. Enabling the central/observer roles at all, with a GATT client linked in.
  2. Scanning under radio coexistence with `esp-hosted`'s WiFi.

**The instant revert**: `build/esp-hosted` was reset to `cab5e98`, the firmware
rebuilt (678,464 bytes) and flashed, and the watch boots to `buildroot login:`
with no reset. The work is kept on branch `ble-hid-wip`.

## The real blocker: firmware logs are invisible after handoff

This is what made the failure hard to read, and it has to be fixed before the
next attempt.

The USB-Serial-JTAG port carries ESP-IDF logs until Linux claims it, at about
1.75 s. Every line after that is Linux printk. `app_main` brings the BLE stack up
*after* it starts Linux, so `ble_hid`'s own `ESP_LOGI` output goes to a port Linux
now owns -- it is lost, not delayed. Raising the log level to INFO made the early
boot verbose and revealed nothing about BLE, because the BLE code runs later.

So debugging the keyboard stack needs one of:

  * a log sink that survives handoff -- a second UART, or the `printf` that
    `esp-hosted` already uses for its own diagnostics;
  * starting the BLE host *before* `linux_boot`, so its logs land in the window
    that is still visible;
  * a status command the Linux side can invoke, which needs the keyboard up
    first -- circular;
  * a deliberately delayed Linux boot while bringing the keyboard up.

The second is the cheapest and also the most likely to be correct on its own
merits: the radio should be up before there is traffic to share it with.

## Next step

Bisect the two candidates with logs visible:

  1. Central/observer roles enabled, `ble_hid.c` not called at all. Does it boot?
  2. If yes, add the host init but no scan. Does it boot?
  3. Then add the scan.

Each answer costs one flash, and step 1 in particular says whether NimBLE's GATT
client simply does not fit alongside this port's WiFi.

## Log visibility: what was built, what it proved, what broke

The firmware could not be diagnosed after handoff because the USB-Serial-JTAG
port belongs to Linux from ~1.75 s. Three things were added to fix that.

**A `/dev/esp-log` device.** A new `ESP_LOG_IF` interface type in both the kernel
and firmware enums, a misc device and its own skb queue on the Linux side, and a
`ble_prov_log_nonblocking()` sender on the firmware side. Separate from
`/dev/esp-ble` because that pipe is a **question-and-answer dialogue** between
`/usr/sbin/ble-wifi-setup` and a phone: it writes prompts and `read_line`s the
answers, so a log line arriving mid-dialogue is consumed as an answer and fires
the provisioning menu. Sharing it was not an option.

**It worked.** Proof from the kernel:

```
esp32_wifi:esp_log_pipe_rx: esp-log: first frame received, 48 bytes
```

**Two real bugs came out of testing it.**

1. `skb_queue_tail+0xe` oops. The firmware logs as soon as its sink is installed,
   which is before the kernel driver finishes probing, so frames arrived while
   `log_q` was still zeroed and `skb_queue_tail` dereferenced a NULL `prev`. Fixed
   by setting `log_adapter` last in init and treating it as the ready flag.

2. A global `esp_log_set_vprintf` hook **wedges the board.** With the hook
   installed and logging at INFO the board stops at the shmem probe and resets on
   the core-0 task watchdog (`rst:0x7 TG0WDT_SYS_RST`), reproducibly. The first
   theory -- that writing to the UART after handoff blocks -- was wrong; gating
   that out did not help. What did help was not installing the global hook at all
   and letting each module log explicitly (`hid_log()` in `ble_hid.c`).

## Where it stopped

Two independent blockers remain, and they are separate:

  * The global log hook must not be used. Targeted senders only.
  * `ble_hid` compiled in still wedges the board at ~4.4 s, immediately after
    `Chipset=ESP32-S3 ID=09 detected over shmem IPC`, whether or not anything is
    logged. So the BLE central/GATT-client/scan path has its own problem,
    independent of logging.

The watch is running the known-good firmware (`cab5e98`, 678,464 bytes) and boots
to `buildroot login:`. The BLE work is on branch `ble-hid-wip` and in
`stash@{0}` of `build/esp-hosted`; the log-pipe work is committed.

## Next step

Bisect `ble_hid` with the targeted log path in place, one variable per flash:

  1. `ble_hid_configure()` only (security config + store), no host sync hook, no
     scan. Does it boot?
  2. Add `ble_hid_on_sync()` with `scan_start()` commented out.
  3. Add the scan.

That separates "the central role costs more RAM than is free" from "scanning
under radio coexistence wedges core 0", which are very different fixes.

## CORRECTION: the "advertising breaks WiFi" result was wrong

An earlier version of this section claimed an A/B showing that BLE advertising
took WiFi from 0% to 100% packet loss. **That conclusion was wrong and is
retracted.** The measurements were real; the attribution was not.

What was believed:

  * before reboot: 4 pings to the gateway, 0 received, plus `reason=2` deauths
  * after reboot:  4 pings, 4 received
  * therefore: the thing that changed was BLE advertising

What actually changed was the reboot. The firmware calls `ble_prov_init()`
unconditionally, so the watch advertises for its whole life, and it was
advertising during **both** measurements. Verified after the fact:

```
$ bluetoothctl --timeout 12 scan on
[NEW] Device 80:45:6B:46:DF:56 Esp32-Linux        <- advertising

$ ping -c10 -i0.3 10.10.0.15
10 packets transmitted, 10 received, 0% packet loss
rtt min/avg/max/mdev = 96.254/144.711/203.978/27.845 ms
```

So advertising and working WiFi coexist. The honest reading of the original
failure is a **stale association**: the link had stopped passing traffic, the AP
had deauthenticated it (`reason=2`, "previous authentication no longer valid"),
and the reboot re-established it. A plausible contributor is the repeated
`wifi scan` invocations run immediately before that failure -- scanning moves the
single radio off-channel and disrupts an associated STA link -- but that is a
hypothesis, not something measured.

**What survives from that investigation** is narrower but still useful:

  * ~150 ms average round-trip to the watch. High for a LAN, consistent with
    radio sharing, but functional.
  * Repeated `wifi scan` while associated is disruptive and should not be used
    as a connectivity test. Ping the gateway instead.
  * The lesson worth keeping: an A/B needs the variable confirmed, not assumed.
    Here the assumed variable (advertising) was never actually off.

## Superseded: the original (incorrect) analysis

This is the clearest result of the whole investigation, and it came from an A/B
that changed one variable.

**With BLE advertising running** (the `Esp32-Linux` provisioning peripheral,
which advertises forever and re-arms every 5 s):

```
watch -> gateway:   4 packets transmitted, 0 received, 100% packet loss
wpa_supplicant:     CTRL-EVENT-DISCONNECTED ... reason=2   (several times)
host -> watch:      15 packets transmitted, 0 received, 100% packet loss
```

`reason=2` is "previous authentication no longer valid" -- the AP dropping a
station that stopped answering, not the station leaving.

**After provisioning was configured**, so the watch started itself with
`BLE WiFi setup: not started (wifi already configured)` and nothing was
advertising:

```
watch -> gateway:   4 packets transmitted, 4 received, 0% packet loss
                    min/avg/max = 15.7 / 63.9 / 192.8 ms
host  -> watch:     5 packets transmitted, 5 received, 0% packet loss
                    min/avg/max = 177.7 / 187.6 / 196.0 ms
```

Same firmware, same radio, same AP, same watch. The only change is that the BLE
peripheral stopped advertising.

## What this means for the keyboard

BLE and WiFi share one radio on the ESP32-S3, on a core that also runs the
ESP-IDF idle task. A peripheral advertising every 5 s for the life of the device
is enough to starve an active WiFi STA link into 100% loss.

So the keyboard work cannot simply add "connect to a BLE keyboard" on top of
what exists. The provisioning peripheral has to stop advertising once WiFi is
configured -- which it now does, for exactly this reason, in the port's own
`S46blewifi` logic -- and anything that keeps BLE alive must coexist politely:

  * **Stop advertising once configured.** Already true, and load-bearing.
  * **Narrow scan windows.** 50 ms in every 100 ms, not window == interval.
    Continuous scan starves the idle task on a shared core.
  * **Expect degraded throughput, not a broken link.** A connected BLE keyboard
    at a sane connection interval costs the radio far less than advertising.
  * **Do not run peripheral and central roles at once unless measured.** The
    original plan had the watch as provisioning peripheral *and* HID central
    simultaneously. On this hardware that is a coexistence question first and a
    protocol question second.

## Also corrected

An earlier note in this file claimed "WiFi is up" on the strength of a `wifi
scan` returning 19 networks. That was wrong. A scan proves the radio and the
scan path work; it says nothing about the data path. The link was associated
with an IP address and dropping 100% of packets at the time.

# What it actually was, and what now works

*2026-11-09.* The firmware that wedged the board was never short of RAM and
never fighting WiFi. It was doing one thing wrong, and two more things had to
be right before a keyboard could work at all.

## 1. The wedge: an unbounded scan starves core 0's idle task

`ble_gap_disc(own_addr_type, BLE_HS_FOREVER, ...)` was called once at start-up
and never stopped. Every advertisement from every device in radio range was
delivered to the NimBLE host task, which ESP-IDF creates with
`xTaskCreatePinnedToCore(..., configMAX_PRIORITIES - 4, ..., NIMBLE_CORE)` —
`NIMBLE_CORE` is 0, the core that also runs the WiFi task and the FreeRTOS idle
task. Idle never ran again, and ten seconds later:

```
rst:0x7 (TG0WDT_SYS_RST)   /   esp_restart_noos at system_internal.c:158
```

**Proof that it was starvation, not memory or coexistence:** with
`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` off, the same firmware ran
indefinitely, `loadavg 0.19`, with the BLE stack up and the keyboard connected.

Two earlier readings of this same evidence were wrong and are superseded:

- *"The central role plus a GATT client does not fit."* It fits. Roles-only
  boots, and so does the full host.
- *"Scanning under coexistence with WiFi wedges the core."* Scanning at 100 %
  duty does. Scanning at 25 % duty in bounded windows does not, and WiFi stays
  up.

The fix is in `ble_hid.c`: a 30 ms window in a 120 ms interval, duplicate
filtering on, 15 s per hunt, then an 8 s gap. Measured cost of the whole BLE
host including the GATT client: 687,200 bytes of a 786,432-byte partition.

**One thing this hid for a long time: firmware panics are invisible.** The
ESP-IDF console is UART0, and nothing on this board is wired to UART0. A crash
in `app_main` therefore looks exactly like a silent reset loop. Enabling
`CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG` puts panics on the same
`/dev/ttyACM0` the shell is on — but it also stops Linux from reaching
userspace, because the firmware and the kernel both drive that peripheral.
Enable it in a throwaway build only.

## 2. The silent blocker: nothing initialised NimBLE's bond store

With the scan fixed the board booted, found a keyboard, connected — and then:

```
hid: connected, security initiate (8)          <- BLE_HS_ENOTSUP
hid: found 6 notifying report(s)
hid: protocol mode -> report (0)
hid: protocol mode write status 261            <- ATT 0x05, insufficient auth
hid: map read status 261
```

`ble_store_config_init()` is the application's responsibility in ESP-IDF; the
examples call it explicitly in `main()`. This firmware never did, and the store
callbacks were therefore NULL, so `ble_gap_security_initiate()` declined to
start pairing at all. Without pairing the link never encrypts, and a HID
keyboard will not hand over a single report on an unencrypted link.

## 3. Bonds cannot be persisted here, and that is a hardware limit

With `CONFIG_BT_NIMBLE_NVS_PERSIST=y` the board reset-looped at ~3.3 s. The
backtrace named it exactly:

```
abort() was called at PC 0x42023b2e on core 0
  esp_ipc_call_and_wait
  spi_flash_disable_interrupts_caches_and_other_cpu
  esp_mm ... esp_ota_get_running_partition
  esp_partition_write <- nvs::Page::writeItem <- nvs_set_blob
```

Any flash write from core 0 must park the other core to disable the flash
caches, and **the other core is running Linux**, so there is no FreeRTOS IPC
task to park. It aborts. There is no way around this from the firmware.

So bonds live in RAM and the keyboard re-pairs on each boot. Doing better means
handing the bond to Linux over the pipe, storing it on jffs2 in `/home`, and
feeding it back before the NimBLE host starts — the mechanism is
`ble_store_write_peer_sec()` on the firmware side. Not done yet.

## 4. Report protocol, not boot protocol

Boot protocol gives a fixed 8-byte keyboard report: letters, modifiers, nothing
else. Every media key, every vendor report, is invisible in boot mode. The host
now writes `1` to HID Protocol Mode (0x2A4E) and subscribes to every notifying
report characteristic — six of them on the keyboard tested — then reads the HID
Report Map (0x2A4B) and forwards it to Linux.

The firmware forwards reports **verbatim**. When a HID device uses report IDs
the ID is already the first payload byte, so the firmware needs no per-report
bookkeeping at all; it is the daemon's job to know what the bytes mean.

## 5. Choosing the right keyboard

A scan sees every HID device in range. Connecting to the first one found spent
the connect timeout on a neighbour at -92 dBm while the keyboard on the desk was
never tried. Scan results are now collected for the whole window; anything at
-60 dBm or better is connected immediately, otherwise the strongest candidate is
tried when the window closes, and anything below -80 dBm is not tried at all.

## The userspace side

`userspace/blekbd.c` parses the HID report descriptor and drives uinput. The
parser is the part most likely to be subtly wrong, so it is tested without a
keyboard: `scripts/build-userspace.sh --selftest` builds it for the host, feeds
it a recorded descriptor and report stream, and prints the keys it would inject.
Modifiers, chords, and Consumer-page media keys all decode correctly.

Two details the firmware/daemon split makes easy to get wrong:

- **Bit offsets shift by one byte** when report IDs are in use, because the
  report ID is part of the forwarded data but not part of the descriptor's bit
  accounting.
- **Array fields index through the logical minimum**, not from zero.

Both were caught by the self-test rather than on the watch.

## IT TYPES

*2026-11-09.* A Logitech BLE keyboard types into a shell on the watch's own
AMOLED console. Verified by typing `ls` and reading the command and its output
off the watch screen, with `key KEY_L down` / `KEY_S` / `KEY_ENTER` in the
daemon log and zero unmapped usages.

The thing that unlocked it was reading TinyTang's BL616 port instead of
continuing to hand-roll. Its `hid_setup_reports()` writes **0** to HID Protocol
Mode — boot protocol — and that single choice cascades into everything else:

    boot protocol  ->  8-byte report, no report ID, no report map to read
    report protocol ->  report-ID'd reports + a 934-byte map to interpret

Report protocol is what was implemented first, and it cannot work here. The map
is read over ATT, one 22-byte chunk per round trip at MTU 23; 43 trips, one mbuf
each, and the pool is exhausted. Every subscribe then fails with
`BLE_HS_ENOMEM (6)` and the keyboard drops the link (`HCI 0x16`) before one key
arrives. Boot protocol needs none of it.

**What boot protocol costs:** media/consumer keys. They only exist on the
consumer-page reports, which report protocol exposes. Typing was the
requirement, so this is the right way round. Adding them back means a second
report characteristic and the map read, done serially and with the mbuf budget
understood.

### The five defects between "connected" and "types"

Each was individually fatal, each looked like something else:

1. **Unbounded scan** starved core 0's idle task -> task watchdog reset loop.
   Bounded windows (30 ms in 120 ms, 15 s per hunt, 8 s gap).
2. **No bond store initialised.** `ble_store_config_init()` is the
   application's job; without it `ble_gap_security_initiate()` returns
   `ENOTSUP`, the link never encrypts, and a HID keyboard refuses every ATT
   operation with `0x05 insufficient authentication`.
3. **Report protocol** -> 934-byte map read -> mbuf exhaustion -> link dropped.
   Replaced with boot protocol.
4. **Descriptor walks bounded by the service end**, not the next
   characteristic, so all six reports returned the same CCCD (0x0045) and five
   were never really subscribed.
5. **The daemon created its uinput device only on the report-map path.** Boot
   protocol sends no map, so the device was never created, every decode was
   written to fd `-1`, and typing failed *silently*. This is the one that
   produced a false success: a `HELLO` file appeared and looked like proof, but
   it had been typed into `screen` over the serial console, not over Bluetooth.
   `/proc/bus/input/devices` showing only `gpio-keys` is what caught it.

Lesson worth keeping: **verify the input device exists, not just that a file
appeared.** A write to a closed fd is not an error anywhere downstream.

### Two more, found by testing rather than by the hardware

- **Bonding is off, deliberately.** This board cannot keep a bond — an NVS write
  aborts, because parking the other core for a flash-cache disable is
  impossible when that core runs Linux. With bonding on, the sequence across a
  reboot is: the keyboard remembers us, we have forgotten it, encryption fails,
  and the keyboard — bonded to a host that cannot resolve it — stops
  advertising until it is re-paired by hand. Bonding off still encrypts, which
  is all HID requires, and leaves nothing to go stale.
- **A tty input queue is ~4 KiB**, so a single write truncates long command
  lines; the symptom is a command that never finishes, not an error.

## Deployment: it survives a reboot

- `/home/blekbd` — the daemon (jffs2, writable; the rootfs is a read-only
  cramfs with no buildroot tree here).
- `/etc/init.d/S48blekbd` — starts it, after `S46blewifi` because both want
  `/dev/esp-ble` and the driver allows one opener. The daemon retries on
  `EBUSY` for up to 120 s rather than dying, which it used to do silently.

Verified by rebooting the watch: the daemon starts, the input device appears,
WiFi comes up with its address, and `loadavg 0.25`. The keyboard itself sleeps
when idle — pressing a key wakes and reconnects it, as with any BLE keyboard.

## Build and deploy

    ./scripts/build-userspace.sh              # cross-compile
    ./scripts/build-userspace.sh --selftest   # host test of the HID decoder
    ./scripts/blekbd-selftest.py frames.bin   # generate a frame fixture

The self-test feeds a recorded descriptor and report stream through exactly the
decode path the device uses, and prints the keys it would inject. It caught two
bugs before hardware: the one-byte bit-offset shift when report IDs are in use,
and array fields indexing through the logical minimum rather than from zero.
