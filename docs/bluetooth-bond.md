# Keeping the BLE bond across a reboot

**Status: works, verified on hardware 2026-11-09.** Export writes
`/etc/ble-bond.bin`, the firmware reads it at boot and restores the keys, and
the Rii K06 reconnects on the stored key without being put into pairing mode:

```
bond: loaded 4 entries from ble-bond.bin (0 rejected)
```

Getting there meant finding three defects, each of which hid the next. They are
recorded below in the order they were uncovered, because the diagnostic lesson
is the transferable part: the load had been "failing to load" for several
sessions when in fact it was never reaching the file.

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

## What was wrong with the load, and how it was found

After rebooting with a valid file in place the keyboard woke, failed to
reconnect, and went back to sleep — behaving exactly as if the watch had never
stored anything. Three defects, each hiding the next.

**1. It never read the file, and said so to nobody.** `bond_load()` runs from the
NimBLE host-sync callback, which fires before Linux starts, so every `fw_log()`
from it was dropped. The doc called this an observability problem and it was
right: the fix was `fw_log_defer()` / `fw_log_flush()`, a small fixed pool of
boot-time lines released when Linux first speaks on the pipe. The very first
flush printed the cause:

```
bond: no room to read etc (458752 bytes)
```

**2. PSRAM is not a heap here.** `map_bond_partition()` asked for
`heap_caps_malloc(part->size, MALLOC_CAP_SPIRAM)`. This firmware sets
`CONFIG_SPIRAM_USE_MEMMAP`, so PSRAM is mapped as address space for the kernel
and is not registered as a heap region — no allocation on this board can carry
`MALLOC_CAP_SPIRAM`, and that call could never succeed. It now allocates
internal RAM and reads a **bounded prefix**: 16 KiB, doubling until the file is
found. The whole 448 KiB partition never fits alongside everything else at boot
and does not need to, because jffs2 appends and this partition holds a handful
of small files.

**3. The reader trusted a node's own length.** `jffs2_walk()` bounds-checked the
node header but then called the callback with `node.totlen` bytes, so an image
truncated mid-node read past the buffer. That was latent while the image was
always whole; bounded reads make a partial trailing node normal, so the check
had to be added with them.

**The blocker was observability, and that is worth keeping as a lesson.**
`bond_load()` runs from the NimBLE host-sync callback, which fires long before
Linux is reading `/dev/esp-log`, so every `fw_log()` from it was dropped — and
there was no evidence either way: the file may have been read and rejected,
loaded and then ignored, or never read at all. Several sessions were spent
revising the load logic on that basis. The instrumentation came first in the
end, and it answered the question in a single boot. When a path cannot report
its own failure, build the reporting before touching the logic.

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

## Is it worth having?

It is no longer a question: the load works, so `sm_bonding = 1` is safe to keep.
The argument for finishing it was that a bond which saves but does not load is
worse than no bond at all — a keyboard bonded to a host that has lost the key
stops advertising until it is re-paired by hand, which is exactly the failure
mode `ble-bond-wip` left the watch in. That is fixed, and the keyboard now
reconnects on the stored key after a reboot without pairing mode.

What it buys is small — a fraction of a second per reconnect, and no manual
re-pairing — but it is the difference between a keyboard that works after every
boot and one that needs the user to hold a key combination each time.

Still open, and separate: the slot layout is a single file holding every key
set, with `ENTRY_MAX` 8 and the K06 using 4. If a second device is ever paired
alongside it, check that count before assuming both fit.
