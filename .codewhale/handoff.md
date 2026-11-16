# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-16T17:05

Orientation, not authority. **Verify before acting on it.** The files, the
coredump partition and the board are the fact; this file is a claim.

## One line

**`music` plays FLAC on the built-in speaker with the decoder running out of the
`modules` partition, and the inline decoder is deleted — 32 KB of app image
recovered. A 5:58 track decoded at 15792504 frames, 0 errors, 0 short writes.**

## Where things live

- Project-control repo: `/run/media/vash/GIT/Escapement`, `main`, at `bd4518b`
  (core-log **entry 30**). `.ai/core.md` is RESTRICTED — do not edit without an
  explicit request. Reference records live in `.ai/core-reference.md`.
- **Active code is the fork: `/run/media/vash/GIT/tinydesk-twatch`, branch
  `t-watch-ultra`, clean at `791cc7b`.** Push to `fork`
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
descriptor's build time moves.

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
