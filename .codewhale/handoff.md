# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-17T07:30

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**`music` plays FLAC on the built-in speaker with the decoder running out of the
`modules` partition. Bluetooth has a controller slot and the pad lives there.
Both TinyDesk 0.5.2 fixes are in — the Store divide-by-zero and the held
descriptors 0-2. NVS is 24 KB and running with one free page of the six.**

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `7f3ce38`
  (core-log **entry 40**). `.ai/core.md` is RESTRICTED — do not edit without an
  explicit request. Reference records live in `.ai/core-reference.md`.
- **`.ai/core.md`'s Purpose line is stale and deliberately untouched.** It says
  "Escapement is a port of linux". The project is TinyDesk; the Linux line
  closed 2026-11-11. Fixing it needs the operator's word, because that file may
  only be edited when asked.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `44f87fa`.** Push to `fork`
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
app      esptool write_flash 0x10000  build/tinydesk.bin
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

## NVS IS THE NEXT REAL CONSTRAINT — measured 2026-11-17

```
region     24 KB, six 4 KB pages, at 0x9000 0x6000
in use     5 pages full or active, 1 uninitialised
entries    420 of 756 slots written, 200 erased
churn      page sequence numbers 66, 100, 101, 102 -- GC runs continuously
payload    ~8-11 KB live once duplicates awaiting collection are discounted
```

What is in it: **ns10** NimBLE bonds (the keyboard and the controller),
**ns11** this port's own slot blob (136 bytes = 4 × 34, confirming the
four-slot format is on the board), **ns1** shell accounts, **ns7** WiFi station,
**ns8** 1.9 KB of regenerable RF calibration, **ns12** SSH host key, **ns5** AP.

**The failure it is heading for is silent.** When no free page is left,
`nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` and `app_main()`
answers with `nvs_flash_erase()` (`ports/esp_idf/app/main.c:909`) — accounts,
networks and Bluetooth bonds, with no message. Every other port here uses
0x10000; only `crowpanel7` matches our 0x6000.

**The change is designed and NOT applied** (it erases state, so it needs the
operator's word). Nothing above 0x600000 moves:

```
nvs       0x9000   0x10000    phy_init  0x19000  0x1000
ota_0     0x1A000  0x2F0000   ota_1     0x310000 0x2F0000
coredump / otadata / storage / modules / moddata -- unchanged
```

Cost: the new region covers 0xF000-0x19000, which holds `phy_init` and the
app's first 36 KB, so NVS takes the erase path and both pairings must be redone.
Avoidable in principle by writing the existing 24 KB blob back into an erased
region (leaving ten clean pages), but that is a plan, not a measurement.
An OTA cannot change the partition table: one serial flash with an erase.

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
pinball-fixed-menu-2026-11-16-2134.bin  16,777,216 B   CURRENT
   sha256 48dd838b1931ce4d4ca49bccf6e51630d14a65484c278cfae660dfde1d014e31
   app 0x10000 = b6f1ce70   module text 0xE00000 = 2ef7b97a
   module rodata 0xF00000 = ff665897 -- all three byte-identical to the build
   verify OK (digest matched) with the board held in the stub, board came back
   HOLDS THE FIXED LAUNCHERS: storage extracted at 0x620000 and mounted with
   littlefs-python shows /root/Apps/pm.py and tama.py both with
   `import tinydesk` -- entry 36's device-side edit is in the image.
pinball-gb-music-2026-11-16-1952.bin    16,777,216 B
   sha256 e7cd9c68d9274fa7
   same three regions; PREDATES the pm.py fix (littlefs only)
```

**A full-image flash is the one thing `scripts/apps/` cannot survive**, so this
backup is the only image that carries the working launchers. Untested: restoring
it. `esptool write_flash 0 0x1000000` is ~6 minutes and has never been run here.

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

6. **The app image is at 2% free** (0x2e3e60 in a 3 MB OTA slot). The next
   feature needing real space has to trade something out.
7. **A gamepad reaches the desktop input path like a keyboard now.** Whether the
   emulators exploit that — the NES already takes a BLE Xbox pad through
   `gamepad.c` — is separate work from pairing, and not needed for the slot.

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
