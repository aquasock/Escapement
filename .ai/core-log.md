## 1 COMMIT Unreleased 2026-11-09T10:31:52-07:00

#### Coming From:

Unreleased fd5f252

#### Purpose:

Record the state of the project at a session handoff, so a fresh agent can continue from evidence rather than reconstruct the bring-up.

#### Outcome:

The watch is fully up on real Linux 7.2.4 and every subsystem brought up so far is verified on hardware, not inferred: the kernel boots to userspace, logs in and forks; the AMOLED renders a 40x25 text console inside a 320x400 framebuffer with a black border and a 1 px red outline painted by the driver; the AXP2101 publishes both `axp20x-battery` (100 percent, 4175000 uV, Full) and `axp20x-usb` (online 1) with four registered interrupts; the PCF85063A seeds the system clock at boot; a Logitech BLE keyboard types into a login prompt on the watch face; and WiFi carries real traffic at about 409 KB/s with DNS resolving. Userspace can now draw on the panel, which required adding `fb_write` to the CO5300 driver, and an external SPLB32 Tamagotchi emulator has been measured running on the glass as an independent check of that path. Two facts about the current deployment matter most to whoever picks this up. First, the repository is clean and fully pushed at `fd5f252` on `main`, but the firmware lives in a separate git tree at `build/esp-hosted/esp_hosted_ng` on branch `ble-bond-wip` with local-only commits that must never be pushed, because its origin is the upstream `jcmvbkbc/esp-hosted` repository rather than this project; the running firmware is that branch's build, byte-identical to the one captured in the current backup. Second, that firmware has `sm_bonding` enabled while bond loading is broken, so the keyboard stores a key the watch cannot restore and must be put back into pairing mode after every reboot; this is a known and accepted defect, not a new fault, and the fix is either to set `sm_bonding = 0` (reverts to silent re-pairing on each connect) or to finish the load path. A full 16 MiB flash backup exists at `/home/vash/twatch-backup/working-2026-11-09-1017.bin` (sha256 `639da94c...`), verified to contain the running kernel and firmware byte-for-byte, alongside the untouched factory image; an older `working-2026-11-09.bin` predates `fb_write` and should not be restored. The previous cycle's own result is folded in here: `.ai/core-reference.md` now holds 35 records across twelve topics covering everything this project had to establish about the SoC, board, panel, PMU, kernel constraints, flash layout, jffs2, BLE, networking and toolchain, including the negative results that each cost a session and would otherwise be re-litigated. No build, deployment or hardware test applies to this cycle, which changed documentation only.

#### Next Steps:

Refresh the live state before acting on any of this, since the handoff is orientation and not law: confirm the watch answers on 10.10.0.15 with `./scripts/watch-telnet.py 'uptime'`, and take a fresh full flash backup with `./scripts/watch-backup.sh` if the kernel or firmware is to change, because a kernel-only backup does not protect against the `/etc` damage that made a previously saved kernel appear broken. The next engineering work is touch: the CST9217 at I2C 0x1A is fitted and identified but has no driver at all, so the watch currently has no local pointer or tap input, and it needs `CONFIG_INPUT_TOUCHSCREEN`, a driver, and the XL9555 reset and interrupt lines before it can be verified. After that, console repaint speed is the next largest gap, and it now has a concrete lead because the full-width band transfer shape the controller actually accepts was established while drawing the border. The BLE media keys, firmware-side bond persistence, and any wake-from-sleep path remain deferred by user decision and should not be started without asking.

#### Files Modified:

None.

#### Status:

- Build: NOT RUN
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 2 COMMIT Unreleased 2026-11-09T11:25:20-07:00

#### Coming From:

Unreleased 2118b3c

#### Purpose:

Fix the watch's real time: the console displayed UTC because the image carries no timezone data, and the documented shutdown write-back to the PCF85063A did not exist on the board.

#### Outcome:

The cycle began by testing the standing claim that the RTC held UTC+8 and needed a correction, and found it false: the chip held true UTC to within two seconds of the host clock, so the factory correction had been applied and stuck, and the README bullet asserting otherwise was stale. The visible defect was a different one -- the image ships no `/etc/TZ`, no `/etc/localtime` and no `/usr/share/zoneinfo`, so `date` printed `17:46 UTC +0000` while the clock was right and the console was seven hours off the wall. Writing the POSIX string `MST7` into `/etc/TZ` fixed it, confirmed on hardware with `TZ` unset: `2026-11-09T10:47:07 MST -0700` against a true UTC of 17:47:07. `MST7` rather than `MST7MDT` because America/Phoenix has no DST. A second gap was that `/etc/init.d/S01hwclock`, documented as running at shutdown, was absent from the board; the cause is `/etc/init.d/S03keepconfig`, which carries exactly seven named files plus the dropbear keys across a full-image flash, so any other `/etc` addition is lost by one. A full 16 MiB backup was taken first as instructed (`working-2026-11-09-1053.bin`, sha256 `1e89a99b...`), which proved worth the three minutes because the read hard-reset the board mid-cycle. Both files are now repo artifacts installed by `scripts/watch-time.sh`, which base64s each over telnet or the USB console and checks the sha256 on the watch before an atomic rename, replacing `scripts/rtc-writeback.sh`, which built the init script from a heredoc and could not be re-run safely. The finding that changed the approved plan was `/etc/init.d/S01clock`, shipped in the upstream image: it writes the epoch to `/home/.clock` at shutdown and overwrites the system clock with that note at boot whenever the note is newer, which is after `hctosys` has seeded from the chip, so the note beat the chip. Measured rather than assumed -- a note set 90 minutes ahead moved the clock from 18:12:58 to 19:42:56 against a true 18:12:59 -- and the user chose to disable it, since `/sys/class/rtc/` contains only `rtc0` at 0x51 and if the chip dies the time should say so. Disabling it by renaming would have been a false fix: `rcS` and `rcK` both iterate `S??*`, which is S plus exactly two characters plus anything, so `S01clock.disabled` still matches and still runs; it is moved to `/etc/disabled/S01clock` instead, and `/home/.clock` is deleted. Verification on hardware: a skewed system clock written back at shutdown and adopted by the next boot's `hctosys`, then after the disable a shutdown-and-reboot cycle left `/home/.clock` absent -- `rcK` recreates it if the script is still in the glob -- with `uptime` 54 s and the seed at true UTC. The user confirmed the AMOLED console reads local time and that it survives reboots. Two documentation errors were corrected in the same commit: the stale UTC+8 bullet in README, and `docs/bring-up.md` stating that `date -s "@epoch"` does not work on this shell, which it does, established by setting the clock to 2001 with it. One anomaly could not be explained and is recorded as open rather than smoothed: in one reboot the system clock read true UTC although `hctosys` had seeded a value 300 s fast, and neither `S01clock` nor any other mechanism was found that steps time backwards, so the cause is unknown and bounded to that single boot. The core-syntax audit passed: the `.ai/` diff was inspected, `core.md` was not modified, entry 2 was validated against the template with six sections in order, sequential numbering from 1, ISO-8601 timestamp with the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

Nothing remains on this cycle. The next engineering work is still touch input, which is larger than entry 1 described: the CST9217 at I2C 0x1A has no device tree node, `CONFIG_INPUT_TOUCHSCREEN` is unset, no CST9217 driver exists in 7.2.4 (the tree's only Hynitron support matches `hynitron,cst340`), and there is no vendor register map in `build/` -- so it needs a sourced register map, a driver, and the XL9555 reset and interrupt lines before it can be verified. The console repaint work has a concrete lead in LCD-005. The missing contiguous-allocation anomaly above should be closed with a controlled stamp test if it is worth a cycle. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- userspace/TZ
- userspace/S01hwclock
- scripts/watch-time.sh
- scripts/rtc-writeback.sh
- README.md
- docs/bring-up.md

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---
