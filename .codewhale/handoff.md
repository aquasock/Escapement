# Handoff — Escapement, 2026-11-11T01:45

Linux 7.2.4 on a T-Watch Ultra (ESP32-S3). Real Linux on the watch's own Xtensa
cores, not emulated. This file is orientation; verify before acting on it.

## Where things stand, in one line

**The emulator works and is fast. The board reboots or freezes during long runs
with it, and that is unresolved.** Everything else is done.

Verified on the glass, 2026-11-11 ~00:30:

```
buttons      tamacore -t: pin changes applied by core 0: 0 -> 2, PASS
menus        operator: status is good (the old 0.52x is gone)
frames       1416 published, 1416 blitted over 90 s (~15.6 fps)
guest rate   3,998,834 emulated cycles/s against a 4,000,000 target
jitter       8 ms worst frame
```

Graphics are currently **5 px dots with a 1 px gap** (293x293 LCD, 301x301
frame). `SHM_DOT`/`SHM_GAP` in `shmproto.h` are the only knobs; set the gap to 0
for the true-hardware look and the frame size follows.

## The open bug — read this before anything else

Symptom: with `tamacore` running, the board **freezes** (Linux stops answering,
screen stays) or **reboots** (`ESP_RST_INT_WDT`, reason 5) within a few minutes.
The operator reports it most reliably **when the pet hatches** and when opening
the **status screen / menus** — the moments with large display updates.

What has been **ruled out**, by measurement:

- Not memory. NOMMU Linux has no OOM killer; 256 s idle showed 1328 kB free.
- Not the host console. It reboots with no USB traffic and with telnet banned.
- Not the dirty-frame cache flush alone: bounded to 16-row chunks, longest
  single `esp_cache_msync` measured at **1.6 ms** against an 800 ms watchdog
  (`SHM_PAINT_ROWS`).
- Not core 0 starving. The diag block's `phase` field, which survives a reset,
  read **IDLE** after a freeze — core 0 was healthy, with a 116 ms worst gap
  between frames. **Core 1 is what dies.**
- Not core 1 dying inside the blit: `c1_phase` also read IDLE, and `c1_beat`
  had stopped at a plausible value (the heartbeat was ticking until the end).

What **has** been established, and is a real fix:

**A flash write reboots this board.** Reproduced with the emulator stopped:
`write 1 KB to /home` survives; `write 32 KB to /home` reboots with reason 5.
Fixed by chunking (below), after which 32/64/128 KB writes all complete and the
longest single flash call measures **55.6 ms**.

**A flash write *while the emulator runs* is the fatal combination.** Core 0's
flash path turns the external-memory cache off for the whole chip, and this
kernel executes from PSRAM with **no cache of its own** — so core 1 dies if it
executes anything during that window. A flash write alone survives (core 1 is
parked waiting for the response); the emulator alone survives; together they
reset it. Hatching a pet did exactly that: the save changed, so the loop wrote
it mid-play. **`tamacore` no longer writes the save while playing — only once on
exit.**

**Still unexplained, and the next thing to chase:** the board still freezes
during long runs. The suspicion is a cache-disabling flash operation taken by
some core-0 task that is *not* `linux_flash.c`. To answer it, an instrument was
added at the only place that catches every caller —
`spi_flash_disable_interrupts_caches_and_other_cpu()` in esp-idf — armed at the
Linux handoff. **It was flashed but never read after a freeze.** That is step 1.

### Step 1: read the flash instrument

After a freeze, reset (PSRAM survives; the firmware snapshots the dying record
into `SHM_DIAG_PREV_OFF` before clearing) and read:

```sh
cd /run/media/vash/GIT/Escapement
./scripts/watch-exec.py 'dd if=/proc/esp-shared bs=4 skip=114756 count=3 2>/dev/null | od -An -tu4'
```

Offsets (`0x70110/0x70114/0x70118`, i.e. dword 114756 onward):

- **call count** since Linux came up
- **caller return address** of the most recent call
- **marker** `0x45534331` ("ESC1") once armed

A **non-zero count that grows while the emulator runs** is the answer: something
is writing flash during play. Map the return address with
`xtensa-esp32s3-elf-addr2line -e build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build/network_adapter.elf <addr>`.
A **zero count** rules flash out entirely and the death is elsewhere on core 1 —
in which case go after the shmem/IPC path next.

### Step 2: candidates if the count is non-zero

- `/etc` and `/home` are jffs2. Anything that touches them during play: the save
  (already stopped mid-play), dropbear keys, the BLE bond (`bond.c` — the
  firmware only *reads* it, but Linux writing it is a writer).
- **Treat "no flash I/O while the emulator runs" as an invariant**, not a
  workaround.

## What was fixed this session

- **Flash chunking** (`linux_flash.c`, `patches/harness/core0/0003-*`): requests
  split into 4096-byte pieces with the skip-ipc flag dropped between them, so
  each uninterruptible window is bounded however large the write is.
- **Save written only on exit** (`userspace/tamacore.c`) — see above. Cost, and
  it is real: progress since launch is lost if the watch dies or the battery is
  pulled.
- **Cache operations bounded to 16 rows** (`SHM_PAINT_ROWS`), which took the
  longest single cache operation from one 181 KB flush to 1.6 ms, and let core 0
  publish only the rows it changed (`dirty_lo`/`dirty_hi`) and skip frames where
  display RAM did not change at all.
- **The i2c chatter** (`patches/kernel/i2c/final/i2c-esp32.c`): the driver read
  `espressif,debug` and then ignored it in the `DBG`/`DBG_OK` macros, so every
  boot printed 30 `ok addr=..` lines. Both conditions are now required. 39 lines
  → 1. Register read-back dumps demoted to `dev_dbg`; re-enable with
  `echo 'file i2c-esp32.c +p' > /sys/kernel/debug/dynamic_debug/control`.
- **fbcon yields the framebuffer** — `tamacore` unbinds `/sys/class/vtconsole`
  and restores it on every exit path. That was the flickering black bands.
- **`tamacore` is an ordinary program**, not a boot service:
  `./scripts/install-tamacore.sh` puts it in `/home` and adds `/home` to `PATH`
  via `/etc/profile.d/home-path.sh`. Type `tamacore`, Ctrl-C stops it. There is
  deliberately no `/etc/init.d` entry.

## Traps that cost real time — do not repeat these

- **`/home/vash/Desktop/passwd.txt` is the watch password.** It must live at
  `~/.config/escapement/watch-pass` (mode 600) or every `watch-exec.py` /
  `usb-send.py` sits at `login:` forever. That cost most of an evening.
- **Cancelled console commands leave `python3` holding `/dev/ttyACM0`.** Kill
  them (`pkill -f scripts/watch-exec.py`) or every later deploy hangs. Check
  with `fuser /dev/ttyACM0`.
- **Do not use telnet.** The user has ruled it out; USB works.
- **`--after no_reset` leaves the chip in the bootloader.** Follow every flash
  with `--after hard_reset read_mac` or the board appears dead.
- **PSRAM is not zeroed at power-on.** Any field read as a *maximum* must be
  cleared or it holds `0x55555555` and no real value can ever beat it. Three
  separate instruments lied this way. Clear once per **boot**, never per
  session — clearing per session destroyed the crash record twice.
- **`esp_cache_msync` rejects a size that is not a whole number of 32-byte
  lines**, and the error is easy to ignore. Any struct passed to it must satisfy
  a static assert.
- **Core 0 has a D-cache, core 1 has none.** Any field both sides write must sit
  in its own cache line or core 0's writeback restores its stale copy over
  core 1's fresh data.

## Reconstruct

1. `git log --oneline -6` — `main` at `32304ee`, clean and fully pushed to
   `aquasock/Escapement`.
2. `git -C build/esp-hosted/esp_hosted_ng log --oneline -2` — local only. **That
   tree is never pushed** (origin `jcmvbkbc/esp-hosted`), and its nested
   `esp-idf` submodule carries the flash instrument.
3. `./scripts/sync-firmware.sh --check` — should say the firmware tree matches.

## Build and flash

```sh
export IDF_TOOLS_PATH=$PWD/build/idf-tools
source build/esp-hosted/esp_hosted_ng/esp/esp_driver/esp-idf/export.sh
./scripts/sync-firmware.sh
ninja -C build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build

./scripts/build-tamacore.sh          # core 1 half
./scripts/build-kernel.sh            # or incremental make in build/kernel/linux-7.2.4

PY=build/idf-tools/python_env/idf5.1_py3.14_env/bin/python
$PY -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 --after no_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x10000 build/esp-hosted/esp_hosted_ng/esp/esp_driver/network_adapter/build/network_adapter.bin \
  0x140000 build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage
$PY -m esptool --chip esp32s3 --port /dev/ttyACM0 --after hard_reset read_mac

./scripts/watch-deploy.sh --usb tamacore
```

`space_for_vectors` must equal `0x40380000`; it moves if firmware internal-RAM
use changes.

## Reference

- Window `0x3df00000..0x3e000000`. ctl `0x0`, frames `0x1000`/`0x30000`, EEPROM
  `0x60000`, ROM `0x80000`, diag `0x70000`, diag PREV copy `0x70100`,
  flash instrument `0x70110`.
- Control block: stage 8, rom_ready 32, eeprom_ready 36, buttons 44, quit 48,
  session 52, frame_seq 64, frame_idx 68, frames 72, guest_hz 76,
  dirty_lo 96, dirty_hi 100, reset_reason 112, boot_count 116.
  Contract: `patches/harness/core0/shmproto.h`.
- Diag block (dword index from `114688`): magic 0, version 1, phase 2, frames 3,
  draw_us 4, gap_us 7, draw_max 8, gap_max 11, total_max 12, chunk_max 13,
  flash_max 14, flash_last 15, c1_beat 16, c1_phase 17, c1_blit_max 18,
  c1_last_us 19, c1_frames 20. PREV copy at dword `114752`.
- Backups in `~/twatch-backup/`: `working-2026-11-11-0005.bin` (sha256
  `0248b818…`, current, taken after the 5x5 build), `working-2026-11-10-2239.bin`
  (pre-4x), `twatch-factory-16MB.bin`.
- Numbers: 4.0 MHz guest · 16 fps · LCD 293x293 · frame 301x301 / 181,202 B ·
  chunk 16 rows → 9,632 B · strip 192,640 B · longest cache op 1.6 ms · longest
  flash call 55.6 ms · longest pwrite 122 ms.
- Deferred, do not start without asking: BLE media keys, Rii K06 touchpad,
  Logitech keyboard, firmware BLE bond persistence, wake-from-sleep.

## Housekeeping left undone

`/home` on the watch is 74% full, and 384 KB of that is a leftover `frame.ppm`
debug capture plus two obsolete emulators (`tamafb`, `tamafb-1x`, ~510 KB) that
`tamacore` replaced. Deleting them would cut jffs2 garbage-collection pressure —
which matters because GC is itself a flash operation, i.e. exactly the class of
thing that seems to trigger the reboots. **Ask the user before deleting.**
