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
