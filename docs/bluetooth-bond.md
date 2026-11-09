# Keeping the BLE bond across a reboot

**Status: half done.** Export works. Load does not. The watch does not
currently restore a pairing at boot, so the keyboard needs pairing mode after a
reboot. Nothing here is enabled on the watch — the running firmware is from
`ble-bond-wip` and is not in a state to ship.

## Why not NVS

The obvious place for a bond is NVS. It cannot be used on this board: an NVS
write from core 0 reaches
`spi_flash_disable_interrupts_caches_and_other_cpu()`, which parks the other
core with `esp_ipc_call_and_wait()` — and the other core is running Linux, so
there is no FreeRTOS IPC task to park. It aborts:

```
abort() was called at PC 0x42023b2e on core 0
  esp_ipc_call_and_wait
  spi_flash_disable_interrupts_caches_and_other_cpu
  esp_partition_write <- nvs::Page::writeItem <- nvs_set_blob
```

So the firmware must never write flash after Linux starts. The split is forced:

- **Linux writes** `/etc/ble-bond.bin` (jffs2) when the firmware hands over a new
  bond.
- **Firmware reads** it at boot, before the radio scans.

## What works

**The firmware can read a file Linux wrote.** Verified rather than assumed: the
reader was compiled for the host and run against a real 448 KiB partition image
dumped from the watch. A 256-byte mostly-zero blob — the shape of a bond, and
the kind of content jffs2 *does* compress — read back byte-exact
(`BONDMAGIC` + 240 zero bytes + `ENDMARK`). That matters because
`tiny_jffs2_reader.c` refuses compressed nodes outright:

```c
/* compression is not supported ATM */
if (read.version_compr > read.version_min)
        return -1;
```

Small files are stored uncompressed, so the guard did not bite. `/etc` and
`/home` are mounted `compr=none` anyway, so it cannot start to.

**Export works.** After a fresh pairing the daemon wrote 170 bytes:

```
0000000 45 53 43 42 01 50 02 00   ESCB, version 1, sec_size 0x50, 2 entries
0000008 02 01 44 da 81 a7 88 db   peer sec, keyboard DB:88:A7:81:DA:44
0000016 10 00 00 00 ...           key set
```

8 + 2 × 81 = 170. Magic, size check, both key sets, correct address.

## What does not work

The load at boot. After rebooting with that 170-byte file in place, the
keyboard woke, failed to reconnect, and went back to sleep — behaving exactly
as if the watch had never stored anything.

**The blocker is observability, not the protocol.** `bond_load()` runs from the
NimBLE host-sync callback, which fires long before Linux is reading
`/dev/esp-log`. Every `fw_log()` from it is dropped. So there is no evidence
either way: the file may have been read and rejected, loaded and then ignored,
or never read at all.

The next step is therefore instrumentation — buffer the boot-time bond lines and
flush them once the pipe is live, or park a result code somewhere the daemon can
read — not another attempt at the load logic.

## Three findings worth keeping

**1. `init_linux_mmap()` runs too late for firmware use.** It extends the linear
flash window (so the whole 16 MiB is readable at `0x42000000`) inside
`map_flash_and_go()`, immediately before jumping to Linux. Firmware reading
`0x42xx0000` before that is reading unmapped memory. The first version of
`bond_load()` did exactly that and the board went into a core-1 watchdog reset
loop:

```
rst:0x8 (TG1WDT_SYS_RST)   Saved PC:0x42002b3b (panic_handler)
```

with no console output at all, because the fault is an address that does not
exist yet. `esp_partition_read()` is the API that works at any point after
partition init.

**2. `ENC_CHANGE` fires before the keys are written.** The security manager
writes the bond in `ble_sm_store_keys()` (`ble_sm.c`), at pairing completion —
after key distribution. Exporting from the `ENC_CHANGE` handler found an empty
store and logged `bond: nothing to save` on a link that had just paired
successfully. A 1.5 s delayed callout is what produced the 170-byte file.

**3. Packed structs and Xtensa.** The bond blob is a wire format, so it is
packed. Taking the address of a packed member and handing it to the security
manager is unaligned access; copy to an aligned local first.

## Is it worth finishing?

It buys a fraction of a second per reconnect. With `sm_bonding = 0` the keyboard
already reconnects unattended after a reboot — that was verified, and it is the
configuration the watch shipped with. Bonding is only worth having if the load
works, because a keyboard bonded to a host that has lost the key stops
advertising until it is re-paired by hand; that is the failure mode the
`-wip` branch currently leaves you in.
