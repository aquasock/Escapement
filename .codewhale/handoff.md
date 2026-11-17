# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-17T09:20

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**`music` plays FLAC on the built-in speaker with the decoder running out of the
`modules` partition. Bluetooth has a controller slot and the pad lives there.
Both TinyDesk 0.5.2 fixes are in. NVS is 64 KB, the OTA slots are equal, and
**the app is at 0x20000, not 0x10000**. Internal RAM is now accounted: `mem`
says what holds it and what the real constraint is.

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `ff2af3b`
  (core-log **entry 42**). `.ai/core.md` is RESTRICTED — do not edit without an
  explicit request. Reference records live in `.ai/core-reference.md`.
- **`.ai/core.md`'s Purpose line is stale and deliberately untouched.** It says
  "Escapement is a port of linux". The project is TinyDesk; the Linux line
  closed 2026-11-11. Fixing it needs the operator's word, because that file may
  only be edited when asked.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `1e0a653`.** Push to `fork`
  (`aquasock/tinydesk`) only — **never** `origin` (upstream
  `tinydesk-project/tinydesk`).
- `/run/media/vash/ROCKBOX` is the operator's source backup. **Never write to it
  or unmount it** (`/dev/loop0p1`, image
  `/run/media/vash/USB_HDD/Backups/ROCKBOX_2026-08-01.img`).
- The watch's SD card is `/run/media/vash/TEST` when it is in the reader. It is
  in the watch now, so read paths from the board, not the PC.

## What is proven on hardware

```
music <file.flac>   |   music stop   |   music   (status)
modflac             alias of the above, kept because the logs use the name
decoder             micro-flac v0.3.0, in `modules` (26,685 B), .rodata in
                    `moddata` (1,976 B)
sink                built-in MAX98357A over I2S
```

- **A 5:58 real track:** `state done, 15792504 frames, 0 errors, 0 short
  writes`. 15,792,504 / 44,100 = 358.1 s, matching the track. Operator
  confirmed by ear: "sounds good."
- **The 44.1 kHz tone:** exactly 352800 frames, 78 decode calls.
- **The 48 kHz tone:** `rate 48000`, exactly 192000 frames = 4.000 s. The rate
  now comes from the file, not a constant.
- **Internal SRAM during playback: 20,707 B free; after: 33,727 B.** The PSRAM
  buffers are genuinely released.
- **Clean rebuild is reproducible:** `make clean && make flac` produced a blob
  byte-identical (sha256 `dd47bce9…`) to the flashed one.
- **The stale-link guard fires:** a header deliberately claiming the old base
  was refused with the exact mismatch, decoded nothing, did not crash.

## The trap that cost this cycle — READ BEFORE TOUCHING MODULES

**Both mmap addresses move when the app image changes size.**

```
                 modules (instruction)     moddata (data)
with inline dec     0x42ae0000              0x3cbe0000
after removing it   0x42ad0000              0x3cbd0000
```

A blob linked for the old pair **still executes** — the entry point is reached by
offset from whichever instruction base is live — and reads its constants 64 KB
away. Because micro-flac compares the input's first four bytes against
`MAGIC_BYTES` in `.rodata` (verified: `fLaC` at `.rodata+0x7b0`), the only
symptom was `FLAC_DECODER_ERROR_BAD_MAGIC_NUMBER` on files that were fine.

Three mechanisms now catch it:
- `modload_header_t` carries `linked_inst` / `linked_data` (ABI 2).
- `modload_prepare()` compares them to the live mapping and refuses, leaving the
  reason in `modload_why()`.
- `modules/moddata.at` holds the pair; `make modaddr` refreshes it by reading
  `/root/modaddr.txt` out of the raw storage partition with esptool.

**So: after ANY change that alters the app image size — `make modaddr`, rebuild
the module, flash all three regions.** The guard makes forgetting it a log line.

## Flash and build

```
app      esptool write_flash 0x20000  build/tinydesk.bin   <-- 0x20000 since entry 41
.text    esptool write_flash 0xE00000  modules/build/flac.bin
.rodata  esptool write_flash 0xF00000  modules/build/flac.data.bin
```

The IDF environment does **not** come from `export.sh` — it fails on a missing
`riscv32-esp-elf-gdb`. Use:

```bash
export IDF_PATH=/home/vash/esp/esp-idf-5.3.1
export IDF_TOOLS_PATH=/home/vash/esp/idf-tools
export IDF_PYTHON_ENV_PATH=/home/vash/esp/idf-tools/python_env/idf5.3_py3.14_env
export PATH=/home/vash/esp/idf-tools/tools/xtensa-esp-elf/esp-13.2.0_20240530/xtensa-esp-elf/bin:$IDF_PYTHON_ENV_PATH/bin:$PATH
python $IDF_PATH/tools/idf.py -C ports/twatchultra -B ports/twatchultra/build build
```

App size: `0x2e3090`, 53 KB free in `ota_1`.

## INTERNAL RAM — measured 2026-11-17, with `mem`

The board runs with ~26 KB of internal heap free. Where it went, all measured
on the watch by the port's new `mem` command:

```
internal heap     263,651 total   236,928 used   26,680 free
largest FREE block 9,728          <-- the real constraint, see below
min free (boot)   15,823
IRAM/exec pool    9,352 free, largest 7,168, one region min-free 308
PSRAM             428,612 used of 8.2 MB
static .bss/.data 97,620 B, plus ~113 KB IRAM text
```

| Consumer | Bytes | How known |
| --- | --- | --- |
| Bluetooth (radio up) | **47,304** | measured with `bt.enable` 0 vs 1 across a reboot |
| `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL` | **32,768** | survives BT off; it is the IDF reserve pool |
| shell task stack | 24,576 | `TDSH_SHELL_TASK_STACK`, one block |
| everything else | ~132 KB | ~300 allocations, avg <1 KB |

**The 32 KB is not a leak.** `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768` (the
IDF default) withholds 32 KB of internal RAM from ordinary `malloc()` so that
task stacks and DMA can still succeed when internal memory is full. Its Kconfig:
"the memory in this pool is not given out when a normal malloc() is called".

**THE CONSTRAINT IS THE LARGEST BLOCK, NOT THE TOTAL.** 26 KB free but only
9,728 in one piece — which is exactly why `pm start` failed needing a 10,240
stack. Freeing more bytes does not fix that; allocating emulator stacks EARLY,
before the small allocations chop the heap up, would.

**Distribution of the 236,928 allocated bytes:** 2 blocks of 17-32 KB = 57,344
(24%); 19 blocks of 2-4 KB = 64,088 (27%); 5 blocks of 4-8 KB = 35,840 (15%);
19 of 1-2 KB = 30,192 (12%); 285 blocks ≤128 B = 13,608 (6%). So the mass is in
~45 blocks, not thousands of small ones.

**PSRAM placement is already right** — `s_vt` 36 KB, `games_database` 48 KB, the
panel frame buffer, emulator ROMs and frame buffers all live there.

## THE `mem` COMMAND (new, entry 42)

```
mem heap      per capability, with the lifetime low-water mark
mem regions   IDF's per-region list
mem tasks     per task: unused stack bytes + internal or PSRAM
mem blocks    size histogram of allocated blocks + the largest by address
```

Split into four **because the desktop's terminal is 24 usable rows and the
mirror carries only what is on the screen** — longer output scrolls away and
cannot be recovered from a capture. Each subcommand fits.

**Turning a slot's reconnect off does NOT keep the radio down across a boot**:
`bt off combo` + `bt off controller` + reboot still reported "Radio: up". Use
`board set bt.enable 0` for a no-Bluetooth baseline, and put it back afterwards.

## NVS: DONE — 64 KB since 2026-11-17 (entry 41)

The 24 KB region was measured and replaced. Before:

```
24 KB, six pages: 5 full or active, 1 free
420 of 756 entry slots written, page sequence numbers 66/100/101/102
```

That is garbage collection with one page to work in, and the failure it heads
for is silent — `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` and
`app_main()` answers with `nvs_flash_erase()` (`ports/esp_idf/app/main.c:909`):
accounts, networks and bonds, with no message.

Now: **64 KB, 6 pages used and 10 free of 16.** The live on-board table:

```
nvs        0x9000    64K      phy_init   0x19000   4K
ota_0      0x20000   3008K    ota_1      0x310000  3008K
coredump   0x600000  64K      otadata    0x610000  8K
storage    0x620000  8064K    modules    0xE00000  1M
moddata    0xF00000  1M
```

**Nothing above 0x600000 moved**, so no littlefs migration and no module relink.
The 24 KB at 0x1A000-0x20000 is deliberately unused: two 3008 KB slots ending on
the coredump boundary start at 0x20000 and 0x310000.

**The old 24 KB blob was restored, not wiped**, so the account, the WiFi
credentials and both pairings survived. It can be redone from
`~/twatch-backup/pre-nvs64-2026-11-17-0740.bin` at 0x9000 if ever needed again.

**TWO TRAPS THIS CYCLE PRODUCED, both worth avoiding:**
- **A CSV edit is not a table change until `partition-table.bin` says so.**
  After editing `partitions.csv`, `idf.py build` still wrote `0x10000` into
  `flash_args` and left the OLD `partition-table.bin` in place, despite ninja
  listing the CSV as a prerequisite. Writing that would have put 64 KB of app
  image into NVS. `idf.py reconfigure` (after deleting the stale
  `partition-table.bin`) fixed it. **Read the .bin, not the CSV.**
- **`erase_flash` would have destroyed `storage`** — the launchers, the shell
  config, the user's files. The scoped form is
  `erase_region 0x9000 0x11000` (new NVS + `phy_init` only).

**Reading NVS properly** (the first attempt got nonsense): states are 2-bit
fields in the page's **bytes 32-63**, entries start at **64**, 126 per page.

**Reading NVS properly** (the first attempt got nonsense): states are 2-bit
fields in the page's **bytes 32-63**, entries start at **64**, 126 per page.

## TINYDESK 0.5.2 FIXES — both applied 2026-11-17

Taken from upstream by cherry-pick; both faults existed here.

- `03fc1b9` **Store**: `apps/store.c` divided `st.done * 100` by
  `s_apps[s_busy_app].size`, which is **0 for an app listed offline** — an
  offline double click panicked the board. `on_install()` also had no guard for
  a hidden Install button. Clean apply; only `docs/VALIDATION.md` conflicted and
  was left at our version (it is upstream's QA record).
- `73a3bd7` **descriptors 0-2**: we run `CONFIG_ESP_CONSOLE_NONE`, so nothing
  opened fds 0/1/2 and the first files opened got them — a log line went into
  the file, and a read-only one made littlefs assert. `hold_std_fds()` registers
  `/dev/tdnull` and opens it three times at the top of `app_main()`.
  **Checked first:** the USB mirror writes via `usb_serial_jtag_write_bytes()`
  (`link_usj.c`), never fd 1; and the shell replaces the global `stdout` with
  its own `funopen()` stream per task (`tdsh_bridge_esp.c`).

**The fix's own effect is NOT directly observed** — reproducing it needs a
failed Store download with WiFi. `cat /dev/tdnull` from the shell was
inconclusive: tdsh resolves through `tdsh_path_to_real()` and never reaches
IDF's VFS. The argument it works is from `components/vfs/vfs.c:80-106`, which
can only reject a length of 1, a length over `ESP_VFS_PATH_MAX`, a bad prefix,
or OOM.

## THE CONTROLLER SLOT — what it took, and the trap in it

```
TD_BT_SLOT_CONTROLLER = 3, TD_BT_SLOTS = 4
TD_BT_KIND_GAMEPAD    appearance 0x03C3 (joystick) or 0x03C4 (gamepad)
```

The Xbox pad was in the **keyboard** slot because a gamepad advertises HID and
no keyboard appearance, which made it an "other HID device" — offered to any
slot. `collect()` now withholds a GAMEPAD from every slot but the controller's,
so the trap is gone, not just moved.

**The silent hazard: `slots_load()` demanded `n == sizeof(s_slots)`.** Four
slots make the array bigger, so that check would fail and zero the blob — both
pairings gone, no error. It now accepts any whole number of slots (the new one
is appended, so the old blob is a prefix) and clears only the tail. Verified:
the watch still reports **2 stored in the board's NVS** after two flashes.

**Moving a device:** `ops_pair()` clears any OTHER slot holding the same
address and closes its connection, but keeps the bond — deleting it would force
the controller back into pairing mode. A saved slot holds an address and a
name, never a kind, so storage cannot identify the pad in the keyboard slot;
re-pairing it into the controller slot is what moves it.

**VERIFIED 2026-11-16 23:05, move and reboot both.** After the operator paired
the pad into the Controller slot: `controller ready, Xbox Wireless Controller
78:86:2E:BB:45:79 (267 reports)`, `reconnects by itself`, keyboard `not paired`.
After a **hard reset**: same arrangement, `2 stored in the board's NVS`, and
the pad **already reconnected on its own** — `ready (29 reports)` inside the
first half-minute. That exercises the 4-slot blob written by the move and read
back by `slots_load()` on the next boot.

**The report counter resets with the connection** (267 → 29) and stops climbing
while the pad is idle, because an idle gamepad sends nothing. Non-zero after a
boot proves input arrived; continuous input is `last:` in the window.

**NimBLE:** `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` 3 → 4. The board sets
`MEM_ALLOC_MODE_EXTERNAL`, so the extra connection is PSRAM, not internal RAM.

**Rows are counted, not assumed.** Client height on the watch, measured:
**23 rows** (26, less the taskbar, less frame+title), **22** with the title bar
set to Large. Slots are top-anchored at row 3; the scan section is anchored to
the bottom and gives up list rows (4 at 23, 3 at 22). `COL` went 11 → 12
because "Controller" is ten characters and read "Controlleridle".

`api_stable` failed on the two enums — the gate working. Re-recorded with
`--record`; the diff is two added lines. `TD_VERSION` stays 0.4.0 (semver rules
are from 1.0, and the text lives in six port CMakeLists).

## Driving the watch's console

**Reboot, wait ~22 s, F10, Enter, then type.** That is the whole recipe and it
has never changed. `/tmp/drive3.py` does it: it clears the screen before each
command, because the display mirror accumulates and old scroll reads as new
output.

- **Ctrl+Q is `0x11` = XON** and is swallowed by the line discipline unless
  `IXON|IXOFF|IXANY` are cleared. Every serial helper must clear them.
- `tdsh` is **not bash**: no `wc`, `head -4` fails (`head <file> [-l lines]`),
  and **double quotes trip the parser** — `tdsh: unmatched quote or escape`. Use
  globs (`01*`) or unquoted paths. Glob results are **relative**, and the
  playback task does not inherit the shell's cwd, so `cd` is not a substitute
  for an absolute path.
- The status line is wrapped at 40 columns mid-word (`fra mes`). Strip all
  whitespace before matching.
- esptool leaves the port at its own baud and the console goes silent;
  `chip_id --after hard_reset` restores it.
- An orphaned helper holding `/dev/ttyACM1` makes every esptool call fail while
  the board looks healthy. `fuser /dev/ttyACM1` names it.

## Backups

```
music-only-2026-11-16-1654.bin   16,777,216 B   THE CURRENT LAYOUT
   sha256 8308350bb31ff4e6e966a3d652f24a2b4852e4203966bf56b5a28b60b865dd68
   app / module .text / module .rodata all verified byte-identical to the build.
music-module-2026-11-16-1637.bin  the same app plus the pre-guard module
modules-2026-11-16-1320.bin       pre-`music` layout (inline decoder present)
baseline-2026-11-16.bin           entry 23, PRE-FLAC — do not restore expecting
                                  a working board
```

**A digest read before a rebuild will not match one read after** — the app
descriptor's build time moves, and esptool appends an image digest computed over
it. Measured on this change: after deleting the dead `main/music.h`, the rebuild
produced the same size (0x2e3090) and differed from the flashed image in exactly
69 bytes — 4 for the timestamp `16:45:02` -> `16:59:14`, the descriptor's ELF
hash, and the trailing 32-byte digest. Every byte in between was identical, so
the board runs the same code and was not reflashed. Compare regions, never whole
digests.

## Documentation, as it now stands

Rewritten 2026-11-16 (core-log entry 31). The README describes TinyDesk on the
watch, with the Linux era in its own section marked as abandoned — not deleted,
because `docs/panel-geometry.md` is still the only measurement of the chamfers
and the current desktop is laid out inside it.

- **`docs/README.md` sorts every document into current and archive.** Three are
  current (`nes-accuracy`, `gbc-scoping`, `pokemini-scoping`); the other eleven
  are Linux-era. Two archive documents are still load-bearing:
  `panel-geometry.md` and `panel-transfer-cost.md`.
- **Live scripts:** `watch-backup.sh`, `tamacore-host.sh`, `tamacore-sync.sh`.
  Everything else in `scripts/` targets the Linux image. `watch-backup.sh` runs
  out of the *old* toolchain's Python at
  `build/idf-tools/python_env/idf5.1_py3.14_env` — it works, and it will break
  if `build/` is deleted.
- **This repository has no `LICENSE` file.** The old README claimed GPLv3; the
  firmware fork is MIT. Unresolved, recorded as a decision owed.

## START-MENU LAUNCHERS — one missing import, and both are versioned now

`py` is MicroPython with a `tinydesk` module **registered but not auto-imported**.
`/root/Apps/pm.py` called `tinydesk.shell(...)` without `import tinydesk`, so
every launch of **Pokémon Mini** from the Start menu printed a `NameError`
traceback. **`tama.py` already had the import and was never broken** — the two
failures looked identical from outside.

Both launchers now live in `scripts/apps/` (they were in no repository, and a
full-image flash would have lost them) with the install commands, and both use
the firmware's default ROM path instead of carrying a long path with spaces:

```
pm.py     import tinydesk / tinydesk.shell("pm start")
tama.py   import tinydesk / tinydesk.shell("tama")
```

Verified **from the Start menu** — the path that failed — at 71.8 fps.

**Navigating the menu:** `td_type_ahead` steps to the *next* match, so `p` selects
Pixels and a second `p` reaches Pokémon Mini; `t` reaches Task Manager before
Tamagotchi. `End` goes to the very bottom, which is the users section and
"Sign out", not the app list.

## GUEST STACKS BELONG ON PSRAM — a class, not two coincidences

`gb start` and then `pm start` both failed with **"no task"**. `GB` was 8 KB and
`PM_STACK` 10,240 B of internal DRAM, against a largest free internal block of
**9,728** with the desktop up and a Terminal open. Both moved to PSRAM with
`xTaskCreatePinnedToCoreWithCaps(..., MALLOC_CAP_SPIRAM)` and both delete with
`vTaskDeleteWithCaps()`.

Safe for a specific reason, not by luck: those tasks run the emulator and
nothing else — the ROM is loaded on the caller's task, buffers are already
PSRAM, and they open no file and map no partition, so they never disable the
cache a PSRAM stack lives behind.

**Two instances means a class.** Still on internal stacks and worth a sweep:
`tama.c`, `nes.c`, `gbcbench.c`, `pmbench.c`. `nes.c` fits today, but that is a
measurement about one game and one ROM.

## Backups taken and verified

```
nvs64-2026-11-17-0755.bin            16,777,216 B   CURRENT LAYOUT
   sha256 542396eb749c1c34dae4480a2c87b3a39a5ecfdd54a6551c9f7bfc98daae47d8
   app 0x20000 = 2822d342   module text 0xE00000 = 741413b9
   module rodata 0xF00000 = ff665897 -- all three byte-identical to the build
   verify OK with the board held in the stub; board came back
   NVS 64 KB layout, both pairings and the account present.

pre-nvs64-2026-11-17-0740.bin        16,777,216 B   THE OLD LAYOUT
   sha256 26e67e981c636711a136800a490c66696a157ddd927c8e9eaaec8a1d9e95096b
   app at 0x10000, nvs 24 KB. The only image in the OLD layout, so the only
   way back to it -- and where the 24 KB NVS blob lives if it is wanted again.

pinball-fixed-menu-2026-11-16-2134.bin
   sha256 48dd838b1931ce4d4ca49bccf6e51630d14a65484c278cfae660dfde1d014e31
   old layout, predates both 0.5.2 fixes.
```

**Take a new one after any layout change** — an image in the old layout no
longer matches the partition table on the board, so restoring it is a rollback
rather than a backup.

## THE HARNESS BUG THAT MISLED ME FOR A WHOLE STRETCH

Escape sequences were written **one byte per 100 ms**, and the parser gives up on
a lone `ESC` after a few tens of milliseconds. So every `F10` arrived as
**Escape (which CLOSES the menu) followed by the literal text `[21~`**. That is
why menus kept closing, why one was found scrolled to the bottom, and why I
concluded several times that Enter did not work. **Enter always worked.**
One `os.write()` per sequence, and: F10 → wait 1.5 s → Enter → wait 3 s.

Also: **the display mirror is not a frame counter.** It sent 32 bytes per 2 s
while Pokemon Pinball was visibly running, because an emulator blits straight to
the framebuffer and never touches the text-cell model.

`gb start` (no argument; the ROM default is Zelda DX) failed with
**`gb: no guest task`** once the desktop was up with a Terminal open. The 8 KB
guest stack was internal DRAM, and internal free was 25,395 with the largest
block 9,728 — it does not fit. **Moved to PSRAM** via
`xTaskCreatePinnedToCoreWithCaps`, safe because that task only runs gnuboy: no
file, no partition map, so it never disables the cache a PSRAM stack lives
behind. Deleted with `vTaskDeleteWithCaps()`.

Verified: `running=1 frames=1068 blits=1066 dropped=0 audio=on 43691Hz`, save
restored from `/sd/saves/gbc/...`, operator played it and passed it.

Backup taken in entry 35, below, and verified region by region.

## THE TWO LEAKS THAT KILLED PLAYBACK — fixed, measured flat

The operator's "stopped after 4-5 songs with error: no amplifier" was memory,
not audio. Both leaks were found by counting allocations, and the numbers are
what identified them:

```
decoder working buffer  never freed — nothing called ~FLACDecoder()
                        41 KB PSRAM + 356 B internal PER TRACK
player task              deleted with vTaskDelete() instead of
                        vTaskDeleteWithCaps() — the caps-aware API's PSRAM
                        stack is not freed by the idle task
                        8,196 B PSRAM + 356 B internal PER TRACK
```

- **The module contract has a TEARDOWN now** (`MODLOAD_ABI_VERSION` 3): a call
  with `in == NULL, in_len == 0` destroys what the module built and frees what
  it allocated. `music_player.c` makes that call before freeing the state block.
- **`vTaskDeleteWithCaps(NULL)`**, not `vTaskDelete(NULL)`. `idf_additions.h`
  says so explicitly. `uac2.c` and `usbhost.c` also use `xTaskCreateWithCaps`
  and do not self-delete — safe today, but anyone adding a self-delete there
  must use the caps-aware call.
- **After both: PSRAM returns to 7,786,392 every track, internal delta 0,
  module 0/0.** Measured over eight consecutive tracks.

`MUSIC_UI_SELFTEST` (commented out in `main/CMakeLists.txt`) is the instrument:
it runs a series of tracks and writes PSRAM, internal RAM and the module's live
allocation count after each. `modhost.c` counts those allocations with
`heap_caps_get_allocated_size()`.

### THE THIRD MOVING ADDRESS — now checked

`g_modhost_ptr` moved four times in one afternoon: `0x3c26b8c8`, `0x3c26b99c`,
`0x3c26ba78`, `0x3c26bb50`. A blob holding a stale one does **not** fault; every
call returns `MODCALL_ERROR` and the only symptom is "decode error". The header
now carries `linked_host`, `modload_prepare()` compares it, and
`patch_header.py` reads it from `modaddr.h` at patch time (a parse-time Makefile
read produced `host=0x0` once, which the patcher now refuses).

### "no amplifier" is gone

`sound.c` records the exact failing call and its `esp_err_t`, so the message
now names disable/clock/slot/enable/allocation rather than one catch-all.

## The Music window — built, proven by file, NOT yet seen

`Music` is in the Start menu. A playlist of the SD card's tracks with a
Play/Stop/Prev/Next row, on TinyPhosphor's layout. It is a VIEW onto the one
playback task: `music_player.c` owns the amplifier, the shell's `music` command
and the window both drive it.

**Proven on the device by `MUSIC_UI_SELFTEST`** (commented out in
`main/CMakeLists.txt`; uncomment, flash, and read `/fs/root/musicboot.txt`):

```
window=1  tracks=74  dir=/sd/ROCKBOX/Audio
play=running:1 state:1 frames:253440 rate:44100 secs:5 err:0 short:0
stopped=running:0 state:0
after_next=running:0 sel=1
```

**The operator has now used it**: opened it, played a few songs, heard them.
The window works. What remains unproven is only that playback now survives many
tracks — the leaks are measurably gone, but the failure itself was never
deliberately reproduced, so read it as "cause removed".

**Music is no longer first in the Start menu.** A weak `board_apps_register()`
in `ports/esp_idf/app/main.c` runs after `td_apps_register_all()`, so port apps
register last and sit at the bottom of the menu, next to the user's emulator
`.tdlink` entries.

### THE HOST-TABLE ADDRESS IS A SECOND MOVING TARGET

Entry 30's guard compares the two **mmap bases** only. `g_modhost_ptr` — the
address the blob reads the API table from — moves on nearly every build:

```
0x3c26b8c8 -> 0x3c26b99c -> 0x3c26ba78 -> 0x3c26b9a4
```

A stale one does not fault; it surfaces as **"decode error"** in the status
line. `make modaddr && make flac` after EVERY app build, then flash all three
regions. To check before flashing:

```bash
xtensa-esp32s3-elf-nm ports/twatchultra/build/tinydesk.elf | grep g_modhost_ptr
grep MODHOST_PTR_AT ports/twatchultra/modules/build/modaddr.h
```

They must match. Extending `modload_prepare()` to check this too is owed.

### Driving the desktop: what actually works

- **The Start menu is open at boot**, and `Enter` does open the selected app.
  The screen mirror sends only CHANGED cells, so a frame captured immediately
  after Enter still shows the menu — do not read that as "the key did not land".
  A Terminal that looks absent may be open and focused.
- **A command typed into a just-opened Terminal is lost.** The pause must be
  long enough for the window to take focus, not merely for the frame to paint.
  Roughly: F10, wait ~1.5 s, Enter, wait ~2.5 s, then type.
- After `gb start` the panel shows pixels, so `gb status` written to the
  Terminal is invisible until the emulator stops. Check pixels-vs-text: a few
  hundred idle bytes means a picture.

### The console mirror cannot be trusted — use a file

It sends only CHANGED cells, attaches stale, and repeatedly replayed old
scroll as new output. It twice made `░` wallpaper look like a stuck shadow and
produced a wrong diagnosis. When a display or input question must be answered,
either write the answer to flash (`MUSIC_UI_SELFTEST`, `modload_selftest`) or
have the operator look. Enter and synthetic SGR mouse clicks on the Start menu
did NOT work from the harness; `Esc`, F10 and typed commands did.

## Next steps

1. **`modload` holds one blob.** A second codec needs a registry of blobs by
   name — that is the next structural piece, not another decoder.
2. **Ogg is the obvious companion** to FLAC; `MICRO_FLAC_DISABLE_OGG=1` is
   already set, and `micro-ogg-demuxer` (MIT) is already vendored.
3. **`modcall_t` still does not return the rate.** `modflac` reads the handle's
   `rate` word directly, which works and is documented, but a module that is not
   this one would need the field.
4. **`__assert_func`** routes through the API table and returns rather than
   halting. Reviewed, **never exercised**.
5. **No picker, queue, seek or progress display.** `music` takes a path.

6. **The app image is at 0x2e4c40, 46,016 B (44.9 KB, 1.5%) free** in the OTA
   slot. That is FLASH for the image, not RAM. Both slots are 0x2F0000 so this
   is the budget either way. There is also a **24,576 B gap at
   0x1A000-0x20000** that is unused on purpose (the app offset is kept 64 KB
   aligned); dropping that alignment to 0x1A000 would make 70,592 B available,
   and ESP-IDF only asks for 4 KB there without secure boot.
7. **Decide the NVS question** (see the NVS section): grow to 64 KB now and
   re-pair, or wait until something forces a full-image flash anyway.
8. **`update.url` is unset** in `board.example.conf` and the device's
   `board.conf`, so *Check for official updates* reads upstream's feed and would
   offer an image not built for this watch.
9. **`third_party/tdsh` is pinned behind its remote** (0b8ebee -> 53e8681 seen
   on this fetch; the shell itself stays at 0.5.1).
10. **A gamepad reaches the desktop input path like a keyboard now.** Whether
    the emulators exploit that — the NES already takes a BLE Xbox pad through
    `gamepad.c` — is separate work from pairing.
11. **Upstream invitation, from the author on issue 20:** the port is welcome
    upstream as `ports/twatchultra` if a PR is ever wanted.

## Out of scope by instruction

The **USB-C DAC path** stays out of the player until the speaker path was
working — it now is, so this is a candidate to revisit, but only if asked.
`uac2` remains as entry 23 left it.

## The rules, each from a coredump or a link error

- Instruction window `0x42xxxxxx` = fetch only; a data load faults `exccause 3`.
  Data window `0x3Cxxxxxx` = data only; a fetch faults `exccause 2`. They cannot
  serve the same bytes, so `.text` and `.rodata` need two partitions.
- A PSRAM-stack task may not map flash **and may not touch `/fs`** — both
  disable the cache PSRAM lives behind: `assert
  esp_task_stack_is_sane_cache_disabled()`. Map at boot only
  (`modload_prepare()`); other tasks call `modload_ready()`. Diagnostics go to
  `/sd`.
- A blob has no writable `.bss`; the firmware lends `g_modbss` (256 B) and
  `modload_zero_bss()` clears it.
- A `-D` flag does **not** invalidate object files. `gen_addr.py` generates
  `build/modaddr.h`; every module object depends on it.
- Build modules **hosted** (libstdc++ refuses `-ffreestanding`), enforce
  freestanding at link with `-nostdlib`, keep C and C++ flags separate.
- `objcopy -O binary` starts at the lowest address, which is `.rodata` in the
  data window — always use `--only-section`.

## Test audio

`/sd/media/audio/tone440_44100.flac`, `tone1000_48000.flac`, plus `clip10.wav`
and friends. Real tracks (74 FLACs) in `/sd/ROCKBOX/Audio/`.

## Coredump decode

```
esp_coredump info_corefile --core <f> --core-format raw --off 0x18 \
  --gdb /home/vash/esp/idf-tools/tools/xtensa-esp-elf-gdb/14.2_20240403/xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb \
  <matching>.elf
```

Copy the ELF out **before** the next build. A coredump whose SHA doesn't match
the app means it is stale — clear it with `erase_region 0x600000 0x10000` and see
whether a fresh one appears.
