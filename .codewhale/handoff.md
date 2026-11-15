# Handoff — Escapement / TinyDesk on T-Watch Ultra, 2026-11-15T16:15

Orientation, not authority. **Verify before acting on it.**

Session ended here: operator called a stop mid-cycle. The BLE console is alive and
one fix is built but **not flashed**. Watch state at handoff: connected over BLE,
shell open on the Terminal app, USB not enumerated.

## One line

**The reverse 4 KB read failed because `bleuart fetch` hung the shell task, not
because the link was bad — the fix is built, sha256 `9c7678de767ab384`, and has
never been flashed or tested.**

## THE FIRST THING TO KNOW

**Nothing from this cycle is on the watch.** `bleuart.c` was changed to stop
`fetch` wedging the shell, the build passed, and then the session stopped. The
watch is running the previous UAC2 build. The reverse transfer is therefore
still unproven: no byte has ever come back off the watch.

Do not read any earlier "it works" claim in this file as covering the reverse
direction. Writing is proven; reading is not.

## What actually happened

`bleuart fetch /root/x.txt` was sent and the shell never returned.

Evidence is a replay of the **complete** notify stream — 2.7 MB in `/tmp/c_all`,
no gap — which reconstructs the terminal as frozen at:

```
root@twatch:~# bleua...fetch /r
```

with no prompt ever following, and four later commands (`echo ZZZTEST`,
`head -c 200 /root/x.txt`, `sha256sum`, `cksum`) producing **not one byte**.

Cause: the fetch loop retried the non-blocking `bleuart_write()` every 5 ms with
no bound, while the display mirror filled the same 4 KB TX ring and the desktop
repainted faster than a connection interval drained it. Ring never emptied, loop
never exited, shell task never returned.

The fix (fork commit `8798688`):

- `s_fetching` flag — `board_serial2_alt_write()` returns early during a
  transfer, so the mirror **yields** the ring. Call site confirmed at
  `hal_mux.c:274` inside `serial2_write()`, display 2's mirror path.
- `bleuart_write_wait()` — blocks for ring space, 500 ms ceiling, and a transfer
  stalling five times in a row ends with `(client stopped reading)`.
  Retrying forever is what made a partial failure look like a dead watch.
- chunk 512 → 256, bounded pre-report drain.

## THE INSTRUMENT LESSON — this cost the cycle

**The display mirror sends only changed cells.** Transient output never appears
in a capture; persistent output always does. Measured: `ls -l`'s
`4000  x.txt` survived, `echo ZZZTEST` left no trace at all.

So a client that rebuilds the screen from its latest buffer **reports healthy
commands as failures**. Mine did exactly that, four times in a row, and sent me
hunting the link when the link was fine.

The operator's correction — *"you are not reading the screen properly, its all
flickery in minicom"* — was right and was the turning point.

**A client must keep the whole notify stream and replay it offline.** That is how
the wedge was found.

## Unresolved, and distinct from the above

**The argument echo arrived truncated** — `fetch /r` for `fetch /root/x.txt`. If
that is real and not a rendering artefact, `fopen` is being handed a mutilated
path, and the operator's "cannot open file" points the same way.

Test with a file in the current directory (`bleuart fetch x.txt`) so a long
absolute path cannot be the variable.

The absence of the `cannot open` text in my capture is **not** a contradiction:
output is non-blocking and lossy under burst (see entry 15), so an error printed
into a mirror-filled ring never reaches the client.

`md5sum`, `sha256sum` and `cksum` are all absent from BusyBox `tdsh`, so on-device
checksum verification needs a different tool.

## Do this first

1. Plug the watch into USB and confirm it enumerates. Pin by by-id, never by
   index:
   ```
   /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00
   ```
   At handoff only `usb-TinyTang_TinyTang_CDC_0.1.3-if00` was present.
2. Flash the build at **sha256 `9c7678de767ab384`**, `0x2dde80` bytes, 2% of the
   app partition free. Back up first
   (`./scripts/watch-backup.sh`; verify the sidecar from its own directory).
3. Run `bleuart fetch /root/x.txt` with a client that keeps its capture.
   **Acceptance: 100 intact `Lnn`+36×x lines = 4000 bytes.** Anything short
   leaves `(client stopped reading)` or a byte count to diagnose from.

If it passes, read is finally proven and entry 17 should say so. If it fails, the
truncation theory is next.

## Menu navigation — the recurring wound

**Never press Enter on a row whose highlight has not been read.** Blind Enter has
opened Task Manager on top of a working desktop twice, and once opened the Tama
emulator.

- `F10` = `\x1b[21~` opens Start.
- The selected row is `48;5;25`; neighbours are `48;5;238`. **Confirmed from the
  raw bytes** after several failed guesses: row `[3;1H` (Terminal) was the only
  row carrying `25`.
- On this desktop the launcher opens with Terminal already highlighted, so one
  `F10` then `\r` lands correctly — but read it first anyway.
- Task Manager closes with **Ctrl+Q** (`\x11`), not Escape.
- Prefer an already-open Terminal over driving the menu at all.

## Repositories

- **Fork** `/run/media/vash/GIT/tinydesk-twatch`, branch `t-watch-ultra`, HEAD
  **`7ee5dfe`** — clean, pushed to `fork` (`aquasock/tinydesk`).
  `origin` = `tinydesk-project/tinydesk` — **never push**.
  - `8798688` ble: the fetch fix.
  - `7ee5dfe` uac2: the playback driver, committed so it could not be lost.
- **Escapement** `/run/media/vash/GIT/Escapement`, branch `main` — log entry 16
  committed. 16 of 100 entries used.

## Permissions and limits

- Push only to `aquasock` repos. Never to `tinydesk-project/tinydesk`.
- `.ai/core.md` is RESTRICTED: read it, never edit it.
- **A `core-syntax.md` audit is owed after any `.ai/` change.** One ran at
  handoff: 16 entries, 0 problems, `.ai/core.md` unchanged.
- `board.conf` is gitignored. Watch password at `~/Desktop/passwd.txt` — **do not
  echo**.
- `archived_logs/*.tar.gz` not to be referenced without explicit approval.
- No sub-agents, no goals (project law in `.ai/core.md`).

## Live state on the PC

- `cons.service` (`systemd --user`, transient) runs `/tmp/cons.py` — **the link
  holder**. It connects once and presses nothing unprompted. Commands:
  append a line to `/tmp/c_cmd`; `#keys <secs> <hex>` sends raw bytes;
  `#raw <secs> <cmd>` captures; `#screen` writes `/tmp/c_screen`.
  **`#screen` is untrustworthy** — it rebuilds from one buffer. Read `/tmp/c_all`
  and replay it instead.
  Check `systemctl --user is-active cons`; stop with `systemctl --user stop cons`.
- `/tmp/replay.py` reconstructs the true screen from `/tmp/c_all` offline. Use it.
- `bleserial.service` was stopped. It creates `/tmp/ttyBLE` for minicom.
- Watch MAC `80:45:6B:46:DF:56`, advertises as **`TinyDesk`**. A connected device
  stops advertising, so prefer the known address over scanning. If BlueZ holds a
  stale link, `bluetoothctl disconnect 80:45:6B:46:DF:56`.
- Nordic UART: service `6e400001-…`, write `…0002`, notify `…0003`.

## The larger objective, untouched

**Does `uac2 tone` make sound?** The driver is written, flashed once, and has
never been exercised. It has run as far as `uac2 streaming=0 dev=0 clock=0`,
which proves it registers, not that it plays. This is the whole audio objective —
headphones over USB-C are the only viable path, because the built-in speaker is
unusable in public.

DAC: `3302:3366 KM-HIFI-384KHZ`, UAC 2.0 (`bcdADC 0x0204`), full speed,
isochronous **synchronous** OUT `ep 0x01`, `mps=768`, `interval=1`, alt 3 on
interface 1, no feedback endpoint. 48 kHz stereo S16 = 192 bytes/frame. The
"384 kHz" is its name, not a rate this driver selects.

Also owed, unrelated to audio: `usb_hid.c` still carries
`ESP_INTR_FLAG_LEVEL1`, which makes `usb_host_install()` fail
(`ESP_ERR_NOT_FOUND`) because WiFi and BLE hold the level-1 slots. Two-line fix,
and the USB keyboard path has never run.

## Still open from earlier

- Does the `coredump` partition stay? It has named faults and earned its place,
  but `ota_1` is shrunk to make room.
- WiFi vs BLE as default console. WiFi does not fit: 41 KB free internal with the
  USB host stack linked in, against the 65,536 the SSH server refuses below.
- Backlog: Pause/Reset/Save are labelled on all three emulators and send nothing;
  the Game Boy's right column has a drawn-but-dead third slot; the BLE controller
  needs reconnecting after every reboot; `pm.c` duplicates `gamepad.c`'s
  rasteriser; the NES owes battery-SRAM and its skip-path test.

## Build and flash

```sh
cd /run/media/vash/GIT/tinydesk-twatch/ports/twatchultra
export IDF_PATH=$HOME/esp/esp-idf-5.3.1 IDF_TOOLS_PATH=$HOME/esp/idf-tools
. $IDF_PATH/export.sh >/dev/null 2>&1
idf.py build
W=/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:45:6B:46:DF:54-if00
python -m esptool --chip esp32s3 -p "$W" -b 230400 --before default_reset \
  --after hard_reset write_flash --flash_mode dio --flash_size 16MB \
  --flash_freq 80m 0x10000 build/tinydesk.bin
```

App partition is at **2% free** — capacity is a real constraint now, and worth
addressing before the next feature lands.

`idf.py coredump-info` fails with `coredump SHA256(...) != app SHA256(...)` when
the dump predates a reflash; that dump is unreadable, so flash every fix before
capturing the next crash.

---

Status: **BLE write solid. BLE read unproven, fix built unflashed. Audio
unproven.** Next action is one flash and one command.
