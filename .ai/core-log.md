## 4 COMMIT Unreleased 2026-11-11T22:46:47-07:00

#### Coming From:

Unreleased 0510262

#### Purpose:

Capture a full 16 MiB backup of the state the operator adopted as the new baseline, in which all three margins are painted, labelled and live.

#### Outcome:

`~/twatch-backup/working-2026-11-11-2242.bin` is the current recoverable state, 16 MiB read at 584.2 kbit/s in 229.8 seconds, sha256 `d731a1f90bcc300260a48e5c427c31339a59579ccf4bb367363c3b7b1b60b856`. It carries TinyDesk at 6c32d28 on branch `t-watch-ultra`, the build in which the left column reads F10, Esc, F6, Super+D and F11 stood on end and upright from the right-hand edge, the bottom strip reads Tab, Enter and Ctrl+Q, and the right column carries a reserved slot and four arrows, all of them dark grey faces with a violet border and terminal-green labels. The read was taken with `scripts/watch-backup.sh` rather than by hand, which is the first time that script has been used since entry 96 noted it was skipped; its 921600 baud took the read from 406 seconds to 230 and its own size and hash checks ran as written. What the script does not do is the check that matters most, so it was run afterwards: `esptool verify_flash` read the chip back a second time and compared it against the file, digest matched, which is a stronger statement than a stored hash because it says the file equals the flash rather than that the file equals itself. Three further checks confirmed the image holds the current build rather than an older one. The 2,534,800-byte app region at 0x10000 is byte-identical to the `tinydesk.bin` this machine built, both hashing to `69df4a19`. The bootloader and the app both carry the 0xe9 image magic, so neither region is a truncated or empty read. The partition table parses as nvs at 0x9000, phy_init at 0xf000, ota_0 and ota_1 at 0x10000 and 0x310000, otadata at 0x610000 and storage at 0x620000, and the storage partition holds 1,988 non-empty 4K blocks, so the desktop's own filesystem came with it and not just the firmware. The board was confirmed running after the read's trailing hard reset: the USB port produced the desktop's own terminal escape sequences, which is the application drawing rather than the ROM talking. Nothing in this repository changed; the image is a recovery artifact, so Files Modified is None by the syntax's own rule about generated binaries, and the build and deployment stages are N/A because this cycle deployed nothing. One note for the next backup rather than a change made here: `verify_flash` belongs inside `scripts/watch-backup.sh`, where it would cost one more pass and turn the strongest available check from something a person remembers into something the tool guarantees.

#### Next Steps:

The backup now matches what ships, so the baseline is safe and the next cycle can take a risk. The most valuable thing outstanding is unchanged: the sweep that walks every point of the glass and compares the hit test against an independent implementation in controller coordinates has caught three faults across two cycles and one more in its own reference, and it still lives in a temporary directory. It belongs in the port as a real test, and it is pure arithmetic so it needs no hardware. `scripts/watch-backup.sh` should gain the `verify_flash` step described above. The top margin is the last unpainted region, inert as `EDGE_TOP` and intended for status icons. Two questions still want a decision rather than code: Esc on a bare desktop opens the Start menu, which was sensible while Enter and Esc were a keyboard pair for the icon grid and is odd now that Esc means cancel, and the margin buttons stay painted while a full-screen application owns the screen where they are no longer buttons. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` still untested against the current tree, and the RTC write path written but not exercised. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 3 COMMIT Unreleased 2026-11-11T22:39:41-07:00

#### Coming From:

Unreleased 2b2182e

#### Purpose:

Record the left column's labels turning through 180 degrees so they are upright when the watch is read from the opposite edge.

#### Outcome:

At 6c32d28 on branch `t-watch-ultra`, the left column's labels are rotated clockwise rather than counter-clockwise: they read top to bottom, with each character upright with respect to the right-hand edge of the watch instead of the left. That is the same column seen from the other side rather than a different arrangement, and the operator asked for exactly that after seeing the first version on the glass. The implementation change is small but it corrected a modelling mistake: rotation was a bool, which can say whether a label is stood on end but cannot say which way it is facing, and those are two different quarter turns that read in opposite directions. It is now a three-state enum of upright, counter-clockwise and clockwise, so the direction is a property of the label rather than something a reader has to infer from a flag. Because the two rotations are exact 180-degree opposites, the clockwise mapping is the counter-clockwise one turned around: the first character moves from the bottom to the top, the glyph's top edge moves from the left to the right, and the leftmost bit moves from the last row of the character's band to the first. Nothing about the fit changed, because a rotation preserves the bounding box's dimensions -- Super+D still needs 56 of the 61 rows and there are still five to spare. Two checks were run and both are exact rather than approximate, which is the point: differing from an upright label rotated 90 degrees clockwise by zero pixels, and differing from the previous counter-clockwise mapping rotated 180 degrees by zero pixels, across all five labels. The first of those checks failed on its first run with about a hundred differences per label, and the fault was in the check rather than the code: the reference rotation mapped a source pixel to the wrong destination, using an offset on one axis where it needed a swap of both. Deriving it properly -- a point at (sx, sy) lands at (height minus one minus sy, sx) after a quarter turn clockwise -- made all five agree exactly. That is worth recording as the third time in this sequence that a verification harness was wrong before the code was, which is its own pattern: a second implementation is only useful if its own derivation is checked, and the cheap way to check it is to predict what a deliberate error in the first one would look like. Verified at 1:1 on the host before flashing: all five labels fit their faces with Super+D five rows spare, no glyph pixel on a border, none inside a rounded corner, no missing glyphs. Host suite 37 of 37, four sections hash-verified. The operator's look on the glass is the acceptance test and had not been given when this entry was written.

#### Next Steps:

The margins are finished as far as appearance goes. The top margin is the last unpainted region, still inert as `EDGE_TOP` and intended for status icons. The sweep that has now caught three faults across two cycles, and a fourth in the harness itself, still lives only in a temporary directory and is the highest-value thing to land as a real test in the port. A fresh 16 MiB backup is overdue: `working-2026-11-11-2149.bin` predates five cycles of margin work and none of them touched anything below the display layer, but a backup that no longer matches what ships stops being a fallback. Before that, `scripts/watch-backup.sh` should gain the `verify_flash` step that the hand-rolled backup used, since reading the chip back and comparing digests is a stronger statement than trusting a stored hash and costs one more pass. Two questions still want a decision rather than code: Esc on a bare desktop opens the Start menu, and the margin buttons stay painted while a full-screen application owns the screen. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` still untested against the current tree, and the RTC write path written but not exercised. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/edges.h
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 2 COMMIT Unreleased 2026-11-11T22:33:49-07:00

#### Coming From:

Unreleased 69ed9dd

#### Purpose:

Record the left column's five buttons being labelled with their key names, stood on end, and every label taking the terminal's green.

#### Outcome:

At 8b4c4fe on branch `t-watch-ultra`, the left column reads F10, Esc, F6, Super+D and F11, rotated a quarter turn counter-clockwise so each reads bottom to top, and every label on the glass is now the terminal's ANSI green rather than white. The rotation is not a style preference and the arithmetic is why: a column's usable face is 41 px across, and `Super+D` is seven characters at eight pixels, 56 px wide, so it cannot be laid out horizontally at all. Stood on end it needs 56 of the 61 rows a slot offers between the corner radii and 16 of the 41 columns, which is five rows to spare rather than the one pixel `Right` had before the arrows replaced it. The direction is counter-clockwise specifically, so that turning the watch a quarter turn clockwise makes the labels read left to right along what becomes the top row -- the column is designed to be read that way and the other direction would have made every label upside down in that orientation. Implementing the rotation touched more than the table. A label's bounding box swaps its dimensions when rotated, so the centring in `label_place()` swaps with it, and `label_bit()` gained a second mapping that runs backwards through the font table: the glyph's own rows become the pixel's columns and its columns become the pixel's rows, with the first character at the bottom so the string reads upward. The green is read from the port's own ANSI palette at index 2 rather than written as an RGB565 constant -- `0x0660`, which is `{0,205,0}` -- so a label is the same colour the terminal draws when a program asks for green text, and it follows the palette instead of drifting from it. One mistake was made and caught during this cycle and is worth recording because the build caught it rather than a person: the edit that replaced the old empty `left_label()` was anchored by slicing between two comment strings, and the slice swallowed `TOP_Y1` and all four slot-arithmetic functions with it, so the build failed with implicit-declaration errors on `col_slot_at` and `btn_slot_at`. The fix was to restore the file from git and re-apply a single anchored replacement, and the resulting diff touches only the label table. The lesson is that slicing a file between two markers is only safe when the content between them is exactly what is being replaced, and here it was not. Verified at 1:1 on the host before flashing: all five labels fit their faces, Super+D with five rows to spare, no glyph pixel on a border, none inside a rounded corner, and no missing glyphs. Host suite 37 of 37, four sections hash-verified, and all five label strings confirmed present in the built image. The operator's look on the glass is the acceptance test and had not been given when this entry was written.

#### Next Steps:

The three margins are now all painted and all labelled, so the appearance work is done and what remains is behaviour. The top margin is the last unpainted region, classified in `edges.h` as `EDGE_TOP` and inert, intended for status icons. The sweep that caught three faults across two cycles still lives only in a temporary directory and belongs in the port as a real test, which is now the highest-value outstanding item because every margin change since has been verified by a throwaway copy of it. Two questions want a decision rather than code: Esc on a bare desktop opens the Start menu, and the margin buttons stay painted while a full-screen application owns the screen. If the ANSI green reads too dim on the dark grey face, index 10 is the bright green the terminal uses for bold text and it is one constant away in `LABEL_ANSI`. Also open: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` still untested against the current tree, the RTC write path written but not exercised, and `scripts/watch-backup.sh` still lacking the `verify_flash` step. The backup at `working-2026-11-11-2149.bin` predates four cycles of margin work, so a fresh one is due before anything risky. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/twatchultra/main/display.c
- ports/twatchultra/main/edges.h
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 1 COMMIT Unreleased 2026-11-11T22:23:33-07:00

#### Coming From:

Unreleased 4c4170b

#### Purpose:

Open a new active log after the hundredth entry, recording the archive that holds the previous one and the state of the project at the handoff.

#### Outcome:

The active log reached its hundred-entry limit and was archived to `archived_logs/core-log-2026-11-11-2223.tar.gz`, 136352 bytes, holding all hundred entries of entries 1 to 100. The archive was checked by extracting it again and hashing the result: `d6ff703a2e1ab52993b155398e051f14f0f926961f0d00242acb071bba4cecfe`, which is the hash of the log as it stood before archiving, and the extracted file still counts a hundred top-level entries. This entry is the handoff and is numbered 1, so numbering continues from here. The project state at the handoff is as follows, and everything in it should be verified against the working tree rather than trusted, because a handoff is a claim and the files are the fact. Escapement, the Linux project, is parked: its last code commit is 8645a82 and everything after it in the log is project log only. The T-Watch Ultra work moved to a standalone fork at `~/GIT/tinydesk-twatch`, branch `t-watch-ultra`, which is a port of TinyDesk v0.4.0 and stands 55 commits above that tag; the remote to push to is `fork`, which is `aquasock/tinydesk`, and `origin` is the upstream project and must never be pushed to. The fork's latest commit is 1890bf8. On the glass the port has a 40 by 26 desktop at 320x416 on a 410x502 glass, every app reflowed to that grid with the narrow-layout work list empty in `tests/test_native_apps.c`, the RTC seeding the system clock at boot, and three painted margins that are live touch targets: five buttons down the left column at F10, Esc, F6, Super+D and F11; three across the bottom strip at Tab, Enter and Ctrl+Q; and five down the right column, a reserved slot followed by the four arrow keys. All three margins are dark grey faces with a violet two-pixel border and white labels, the bottom strip's labels being key names and the right column's being arrow glyphs. The touch panel is deliberately not a pointer on the desktop -- a press on the framebuffer does nothing, so the margins are the whole of the input -- while an application that owns the whole screen gets the panel back as an absolute pointer, which is what `td_wm_app_owns_screen()` asks. The geometry of all three margins lives in `ports/twatchultra/main/edges.h` and is shared by the painter and the hit test for a reason the archived log records four times over: when the two were separate they drifted, and the result was a column that did nothing and a strip shifted by twenty-two pixels while every line of code looked correct. A full 16 MiB image of this state is at `~/twatch-backup/working-2026-11-11-2149.bin`, sha256 `74009d2955ccd84dc301b12f376037218da4300cbeb9ae5e611eec4b3684e0e6`, verified by reading the chip back and comparing digests rather than by trusting the stored hash. Status fields are N/A because the rollover itself built and deployed nothing; it is a maintenance action on project metadata, not an engineering cycle.

#### Next Steps:

The equipment for the next several cycles already exists and does not need rebuilding. The left column is the last margin without labels, and it needs only a table: its five duties are all four characters or fewer and the UTF-8 label path already handles them. The top margin is the last unpainted region, classified in `edges.h` as `EDGE_TOP` and inert, intended for status icons. The sweep written in the archived log -- which walks every point of the glass and compares the hit test against an implementation written independently in controller coordinates -- caught three separate faults across two cycles and still lives only in a temporary directory, so it belongs in the port as a real test. Four engineering items remain open and none is urgent: the parked redraw fault where a dark screen keeps leftover pixels because the desktop stops sending rows twelve to twenty-five, the two port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` which are still untested against the current tree, the RTC write path which compiles and is called but has never been exercised by setting a time and power-cycling, and `scripts/watch-backup.sh`, which lacks the `verify_flash` step that the last backup used and so has a weaker check than it should. Two questions want a decision rather than code: Esc on a bare desktop opens the Start menu, which was sensible while Enter and Esc were a keyboard pair for the icon grid and is odd now that Esc means cancel, and the margin buttons stay painted while a full-screen application owns the screen where they are no longer buttons. The archived log also records deviations from this syntax that were left alone because rewriting settled history is prohibited -- entries 17 and 18 carried annotated status labels and fourteen earlier entries listed `.ai/` files under Files Modified -- and they remain in the archive rather than in the active log. Method worth carrying forward, because it is what the last several cycles actually ran on: measure rather than reason about the hardware, and where a fault is arithmetic, sweep the whole input space against a second implementation instead of probing points on the glass. Three of the last five faults were found that way, and two of them -- the coordinate space and a guessed font absence -- were mistakes in reasoning that a four-line check would have caught.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
