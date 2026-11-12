## 78 COMMIT Unreleased 2026-11-12T05:40:00-07:00

#### Coming From:

Unreleased 252e3d4

#### Purpose:

Record MQTT's reflow, the worst case found by the survey, and the survey's full result.

#### Outcome:

At 62237ae on branch `t-watch-ultra`, `apps/mqtt.c` gives each field its own row with its button beside it on a narrow window. Upstream puts a 34-column field at x 8 with seven controls to the right of it -- Connect at 44, Login at 59, Config at 59, Subscribe at 44, Unsubscribe at 58, Publish at 44 and Retain at 56 -- and on a 40-column panel every one of them was invisible and unreachable while the window looked healthy, because the read-only topic list below renders correctly. That is the strongest argument in this line of work for checking an app mechanically rather than by eye, and the survey that found it is worth recording: grepping every app for `td_button`, `td_checkbox`, `td_label` and `td_textbox` calls whose x plus width reaches column 38, and skipping those that already branch on `td_win_client()`, leaves six apps broken and one clear ranking. MQTT is the worst with seven off-screen controls; `trash` has Refresh at x 38, its status label at 45 and "Delete after: never" running to column 40; `update` has a "Check for updates on start" caption 50 columns wide starting at column 1; `store` has "Install on the..." running to 39; and `users` and `settings` place controls and captions at columns 24 and 25 that overflow. Network, Modbus, Displays, Bluetooth, Files, Editor, Date & time and Remote Desktop are already gated. Three of the six -- update, store and users -- have captions longer than the whole screen, so stacking will not save them; they need shorter wording, which is different work from what the last five apps needed and should be planned for. The narrow MQTT form puts Login and Config on the bottom rows rather than competing for space, since they are settings rather than part of the publish flow, and the app's `on_draw` labels are gated by width with the status, subscription and column-heading lines moved below the six-row form. One error was made and caught: the column heading and the list's first row were both at row 8, so the list painted over its own heading; the list starts at 10 now so the heading at 9 stays visible. Verified on the glass: all seven controls, the status line, the subscriptions line and the list heading, read back over the USB link. The host suite is 37 of 37.

#### Next Steps:

`trash` is next and is mostly positioning rather than restructuring: Refresh and the status label need to move, and "Delete after: never" needs a shorter caption or its own row. Then `update`, `store`, `users` and `settings`, which need wording changes as well as layout. Seven apps have windows 60 to 78 columns wide but no control past column 38, so they may only have dead space on the right -- Log Viewer, Task Manager, Keyboard, Mouse, Image Viewer, Search and Pixels -- and are worth a look on the glass rather than a code change, since the cheapest test is whether the content uses the width it is given. The parked redraw fault from entry 74 is untouched; forcing `td_full_redraw()` with the garbage visible is still the experiment that would localise it. The remaining port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` are still untested against the current tree, and the three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, awaiting the vendor map. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- apps/mqtt.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 77 COMMIT Unreleased 2026-11-12T05:00:00-07:00

#### Coming From:

Unreleased 32eb2fa

#### Purpose:

Record the Modbus form's reflow, and a survey finding that says which of the remaining apps need the same work.

#### Outcome:

At cce4386 on branch `t-watch-ultra`, `apps/modbus.c` stacks its form on a narrow window. Upstream puts four controls on each of four 72-column rows -- Address at x 8 with Count at 22, Read at 28, Repeat at 37, the interval at 54 and an "ms" label at 63 -- so on a 40-column panel three controls, Read, Repeat and the interval, were off the glass entirely. The narrow form uses short INLINE labels rather than a caption row per field, because on 23 rows four captions would cost more than the fields themselves: "Dev 127.0.0.1  Unit 1" then the table button, "Addr 0  Cnt 10", the repeat row, and "Vals  [ Write ]". The app's own `on_draw` also draws labels at fixed rows and columns chosen for the wide form, and on a narrow window those landed on top of the fields -- the render read "0  10  t" and "[ ]uRepeat every 1000" -- so they are gated by width and the note and list heading move below the stacked form. Three placement errors were made and fixed inside this cycle, all the same kind: a label drawn at x 21 sat under a textbox that reached column 25, and the note's position was passed as its x argument rather than its y. The rule that emerged is worth carrying forward: widgets paint AFTER `on_draw`, so text drawn near a field must clear the field's whole rectangle rather than the label's own width. A survey done while answering a question about MQTT found that most of the remaining apps need this work and several look fine at a glance while being unusable: `apps/mqtt.c` asks for 76x20 with a 75-column minimum and puts Connect, Login, Config, Subscribe, Unsubscribe, Publish and Retain at columns 44 to 63, all of which are off a 38-column client, but the read-only topic list renders correctly so the window appears healthy. That is the case for checking each app for controls past column 38 rather than judging by how it looks. Verified on the glass: the whole Modbus form, the note, the list with its column heading and the TCP server row, read back over the USB link. The host suite is 37 of 37.

#### Next Steps:

MQTT needs the same treatment and is the worst case found so far -- seven controls at columns 44 to 63 -- so it should come next. The remaining apps are System Monitor, Task Manager, Log Viewer, Users and Settings, then Startup, Store, Remote Desktop and the Software Update window; Users and Settings will be harder than these because both build their content from a list of controls, so a narrow layout may need the list to scroll rather than the rows to stack. A cheap first filter for the rest: grep each app for `td_button` and `td_textbox` calls whose x is 38 or more, which finds the invisible controls without opening the window. The parked redraw fault from entry 74 is untouched; forcing `td_full_redraw()` with the garbage visible is still the experiment that would localise it. The remaining port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` are still untested against the current tree, and the three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, awaiting the vendor map. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- apps/modbus.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 76 COMMIT Unreleased 2026-11-12T04:10:00-07:00

#### Coming From:

Unreleased f8d0a31

#### Purpose:

Record the Displays app's reflow for the 40x26 grid, which had one defect worse than the ones the previous apps showed.

#### Outcome:

At a92edca on branch `t-watch-ultra`, `apps/displays.c` stacks its controls on a narrow window. The window asked for 74x27 with a 70-column minimum and placed its controls at columns 14, 17, 33 and 39; on a 40-column panel the client is 38 wide, so most of that column was off the glass. Two faults were worse than a truncation. The orientation button and the display pickers were laid out past the visible width, so one vanished entirely and the other overlapped its neighbour -- a control the user cannot see and cannot reach, rather than one that reads badly. And the app's two help lines were drawn at fixed rows 21 and 22, which on a 23-row client are the rows holding the message and the Close button, so the help was painted straight over them and the bottom of the window read "[ Close ]+Left / Right, or Other disp". The narrow layout gives each control a label row and a value row in reading order with the shorter captions "Auto (its size)" and "Landscape 180", drops the two long help lines entirely on the grounds that a keyboard hint is the least important thing on 23 rows, and trims the platform's status wording to whole words rather than mid-word. Two controls stay hidden and that is the app's own logic rather than a fault: orientation when the terminal reports no rotation capability, which this fixed panel does not, and the main-display checkbox when only one display exists. Wide windows keep upstream's layout unchanged. The pattern from entries 73 and 75 held again and is now the established way to do this work: measure `td_win_client()`, branch, and keep every height relative. The guard in `test_native_apps` -- launch every app, resize to its minimum, check that each focusable widget lies inside the client -- passed on the first build this time, having caught a real error in the previous app. Verified on the glass: the display diagram, all five visible controls, the rotation note and the Close button all inside a 25-row window, read back over the USB link. The host suite is 37 of 37.

#### Next Steps:

The apps left to check maximised on 40x26 are System Monitor, Task Manager, Log Viewer, Users, Startup, Store, Remote Desktop and the Software Update window; System Monitor and Task Manager are the likeliest to need work because both lay rows out by hand rather than deriving them. Two of the remaining apps will be harder than these three: Users and Settings build their content from a list of controls, so a narrow layout may need the list to scroll rather than the rows to stack. The parked redraw fault from entry 74 is untouched; forcing `td_full_redraw()` with the garbage visible is still the experiment that would localise it. The remaining port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` are still untested against the current tree, and the three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, awaiting the vendor map. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- apps/displays.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 75 COMMIT Unreleased 2026-11-12T03:15:00-07:00

#### Coming From:

Unreleased 61d5e9b

#### Purpose:

Record the Network app's reflow for the 40x26 grid, and the two app checks that passed either side of it.

#### Outcome:

At 5f0b79a on branch `t-watch-ultra`, `apps/network.c` picks its layout from the width of the client it was actually given. The window had asked for 68x23 with a 64-column minimum and placed buttons at columns 30, 47 and 52, so on a 40-column panel everything past column 38 was off the glass, and the four bottom buttons needed 43 columns with their captions. Wide windows (60 columns or more) are unchanged; narrow ones stack one item per row with short captions where the long ones do not fit -- "Telnet desktop" and "Display" instead of "...(port 23)" and "Displays...", and Scan/Join/Leave/Forget, which are 35 columns back to back. The Access point dialog had the same fault one level down, a 56-wide window with two 34-column text fields starting at column 11, six columns too wide once the window manager maximises it; those fields size themselves from the client now and its hint line was shortened so it no longer stops mid-sentence. Search, Editor, Terminal and Files had already been checked by the operator and passed with no change, and Files' layout was confirmed good even though the redraw fault recorded in entry 74 still affects it. Two mistakes were made inside this cycle and both were caught before delivery. The list height was computed from the client height at launch, which looked equivalent to the relative `-2` it replaced and was not: widgets keep the height they are built with, so the first resize pushed the list past the bottom of the client. `test_native_apps` found it, because it launches every app, resizes each to its minimum and checks that every focusable widget lies inside the client -- the first time in this project that guard has paid for itself, and worth remembering when the remaining apps are reflowed. The narrow button positions were also first spaced by intuition rather than arithmetic and overlapped, because a button is its caption plus four columns; the positions are computed now. A stray `fprintf` left in `td_msgbox` during the debugging in entry 74 was removed in the same commit. Verified on the glass: the Network window with its list and all four bottom buttons inside the frame, and the Access point dialog with both text fields and its Save/Cancel row, read back over the USB link. The host suite is 37 of 37.

#### Next Steps:

The apps still to check maximised on 40x26 are Settings, System Monitor, Task Manager, Log Viewer, Users, Startup, Store, Remote Desktop and the Software Update window, with System Monitor and Task Manager the likeliest to need work since both lay rows out by hand. Two patterns are now established and should be reused rather than reinvented: measure `td_win_client()` and branch, as the Network and Bluetooth windows do, and keep every list height relative so a resize cannot push it out of the client. The parked redraw fault from entry 74 is still parked; forcing `td_full_redraw()` with the garbage visible is the experiment that would localise it. The remaining port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` are still untested against the current tree, and the three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, awaiting the vendor map. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- apps/network.c
- src/widgets.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 74 COMMIT Unreleased 2026-11-12T02:30:00-07:00

#### Coming From:

Unreleased f9ffe9d

#### Purpose:

Record a redraw fault that is upstream of this port, two fixes found alongside it that do stand on their own, and a diagnostic effort that went wrong twice before it produced a useful answer.

#### Outcome:

The operator reported leftover pixels on dark screens, reproducible by opening Files and navigating into a directory with fewer entries than the last. It is not fixed and it is not in this port. What established that is a measurement rather than a theory: navigating `/root` into `Desktop` and recording every cursor-position sequence the desktop emits shows writes to rows 2 through 11 and NO writes at all to rows 12 through 25. The desktop never sends those cells, and its own front buffer therefore already believes they are blank, so its diff skips them for ever and nothing downstream can recover -- neither a full push of the framebuffer nor blanking the whole panel, which were both tried. The two port-side fixes that came out of the investigation at 1579667 do stand on their own. First, the cell the cursor leaves was never repainted: `paint_cell` inverts a cell when the cursor is on it while the cell's own colours are unchanged, so once the cursor moved away the comparison read `want == have && !cur`, skipped the cell, and left it inverted on the glass permanently; the cursor's position is remembered now and that cell is painted back exactly once. Second, the panel is wiped once at start-up -- the framebuffer is 320x416 inside a 410x502 glass so a margin is never written, and the controller's RAM is not cleared at reset, meaning whatever the previous firmware (a Linux framebuffer with a different geometry) left there survives; whether anything visible was in those margins is now known to be unproven, since the wipe did not clear the reported fault, and the comment on it says so. The diagnostic effort is worth recording because it failed twice in the same way. A 3-second reflush test never ran: the wait was `ulTaskNotifyTake(pdMS_TO_TICKS(3000))` and the desktop rewrites the clock every second, so the timeout branch was unreachable dead code, and the operator's answer to it described a test that had not executed. Then a mismatch counter was placed before painting, so it counted cells that were merely pending rather than stuck and reported 33 on a healthy framebuffer. Both were reported to the operator as evidence before the error was noticed. The measurement that finally worked -- recording which rows the desktop writes -- took one run and answered the question outright, and the lesson is that it should have been the first thing tried: it observes what the desktop actually sends instead of instrumenting our side of a path we had already measured as correct. An earlier conclusion in that sequence was also wrong and was retracted to the operator: a full push of `s_fb` that left the pixels in place was read as "the panel rejected them", when it meant `s_fb` held the same stale content the glass was showing. The issue is parked at the operator's request. Whether the fix belongs in the Files app, the list widget, or the window manager's fill is not yet known, and the one experiment that would separate those -- forcing `td_full_redraw()` while the garbage is on screen -- has not been run.

#### Next Steps:

When this is picked up: force `td_full_redraw()` with the garbage visible. If the strips clear, the front buffer was stale and the defect is where a buffer init loses a repaint (`apply_size` and `td_buffer_init` in `src/td.c`); if they stay, the composed back buffer never had the cells cleared, which puts it in the Files list, the list widget, or `draw_window`'s fill. Either way it is upstream code shared by every port, so it should be fixed there rather than worked around here. Separately: the remaining port overrides `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0` are still untested against the current tree; the rest of the apps still need checking maximised on 40x26, with Files, Editor, Settings and System Monitor the likely ones to need work; and the three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, awaiting the vendor map. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/twatchultra/main/display.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 73 COMMIT Unreleased 2026-11-12T01:40:00-07:00

#### Coming From:

Unreleased 7776aef

#### Purpose:

Record the first app reflowed for the 40x26 grid, and the accepted state of the Bluetooth and Terminal apps.

#### Outcome:

`apps/bluetooth.c` now lays each of its three slots out in three rows instead of four, at 2afeb88 on branch `t-watch-ultra`, and the operator accepted both it and the Terminal app on the glass. The four rows were not four things worth saying: the pairing state had a line to itself and then a blank one, so the state and the device name share a line now and "idle, not paired" reads as well inline as it did on two lines. The detail line moved to column 1, because it is a continuation rather than a value and the ten columns it gains are the difference between "connected: keyboard and mouse" fitting and being truncated to "connected: keyboard a"; with no last event to show, that row carries the paired address instead of staying blank. The Close button is gone -- the title bar's `[x]` closes the window and the initial focus is the first slot's Pair button, so Enter and the arrow keys still work -- which gave back another row for the device list. Four strings were longer than the 37 usable columns and were shortened rather than left to truncate: the scan prompt lost "near the board", the nothing-found line lost its clause about pairing mode, and the found-count line lost "(or double-click)", which the list's own Enter handling makes moot. The net effect on the scan view is the point of the exercise: it previously began at row 19 with its device list at 21 and its buttons below the bottom of a 25-row window, so it could not be reached at all, and it now fits -- header at 15, count at 16, five device rows at 17 to 21, buttons at 23 and status at 24. `test_bluetooth`'s coordinates follow the new stride, which is the third time that test has needed updating as the layout moved; the pattern is that it clicks fixed positions, so any layout change breaks it, and that is worth knowing before the next app is reflowed. Verified on the glass: the slot list with all three button groups fitting and the closing brackets intact, and a live scan listing five devices with every line inside the frame. The host suite is 37 of 37. The port README now states that apps open maximised and that F6 is the switcher, since neither was written down anywhere a reader of the port would find it.

#### Next Steps:

The remaining apps each need the same treatment, and Terminal has been checked and passed with no change needed -- it fills a maximised window, which is what it is for. The ones worth looking at next are the ones with their own layout arithmetic rather than a list or a form: Files, Editor, Settings and System Monitor all place rows by hand and were drawn for a taller window. Before that, the two port-level overrides are still untested against the current tree -- `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0`, each a one-line revert -- and the three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, with recorded edges that divide the strip into two equal zones plus a one-pixel third, so the vendor map settles that geometry before any code. The Mouse and Keyboard Bluetooth slots pair one device each and share the Combo slot's working path but have not been tried. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- apps/bluetooth.c
- tests/test_bluetooth.c
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 72 COMMIT Unreleased 2026-11-12T01:05:00-07:00

#### Coming From:

Unreleased 9c5b2f3

#### Purpose:

Record maximised app windows as the fork's baseline shape, accepted on the glass, and the correction of a false alarm that preceded it.

#### Outcome:

The fork at 315236e on branch `t-watch-ultra` carries upstream's `apps/bluetooth.c` unmodified and opens every app maximised instead, behind a build flag. The reasoning is that on a 40x26 panel an app asking for a desktop-sized window has most of its content below row 26 and off the glass -- the Bluetooth window's own `WIN_H` is 34 -- while a maximised window gets the desk area and needs no layout change at all: the frame and title bar stay, so it is still movable, resizable and closable, and the taskbar is untouched. `TD_OPEN_MAXIMIZED` is 0 by default so a board with room keeps the overlapping-window feel, and the port turns it on from `project_include.cmake`. Dialogs, tools and one new category are exempt: `TD_WIN_MODAL` because a message box two-thirds down the screen is still a message box rather than a page, `TD_WIN_NO_FOCUS` because tools such as the on-screen keyboard are placed by their own code, and `TD_WIN_OWN_SIZE` because a window whose size IS its meaning -- the display identifier popover, which paints a number in a 14x5 box beside the display it names -- would be destroyed by filling the screen. Both new constants are additive public API and are recorded in `api/tinydesk.api`, which `api_stable` requires and `STABILITY.md` permits in a minor version. The flag is applied before `clamp_rect`, so `restore_rect` still holds the caller's rect and an un-maximised window returns to where it asked to be. The full-screen reflow of `apps/bluetooth.c` was reverted first (b3f34a8) and its test edit with it, which the host suite confirmed carried nothing else: with both files restored from upstream the suite was 37 of 37. Three things about this cycle belong in the record rather than being quietly corrected. The reflow was replaced because maximised windows solve the same problem with a few lines in shared code instead of a relayout of a shared app, and a patch that fixes a defect on a narrow screen is easier to justify upstream than one that restyles a window. The taskbar cannot carry per-window buttons at 40 columns -- it is already full with `[Start]`, the user, the network tray and the clock, so the button loop breaks before placing the first one -- and switching between open apps is F6 or Alt+Tab, which is upstream's own mechanism; that answers the operator's concern about not being able to run two apps at once, since maximised windows stack and closing one reveals the next. And a false alarm is worth naming: after a flash was interrupted mid-write the device sat in esptool's stub, so every read returned zero bytes, and that was briefly reported as a broken build when the cause was the interrupted flash and the chip's state. The operator disproved it by touching the screen, which worked. Nothing had been lost -- the working changes were in a stash and were restored -- and the lesson is that "no output" is evidence about the link, not about the image.

#### Next Steps:

The remaining port-level overrides are still untested against the current tree: `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0`, both suspected of compensating for faults now fixed elsewhere, each a one-line revert. The apps still need going through one at a time to check they read well maximised on 40x26 -- maximising gives them the room, but whether each one's own layout uses it well has only been checked for the desktop and the Bluetooth window. The three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, with recorded edges that divide the strip into two equal zones plus a one-pixel third, so the vendor map settles the geometry before any code. The Mouse and Keyboard Bluetooth slots pair one device each and share the Combo slot's working path but have not been tried. When the fork is offered upstream, the shared-code changes now number six files and the maximised-window policy is among the most generally useful of them, since it is off by default and costs a board nothing unless it wants it. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- src/wm.c
- include/tinydesk/td_wm.h
- include/tinydesk/td_config.h
- api/tinydesk.api
- apps/displays.c
- ports/twatchultra/main/project_include.cmake

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 71 COMMIT Unreleased 2026-11-12T00:30:00-07:00

#### Coming From:

Unreleased c8a8fef

#### Purpose:

Record that the desktop's grid is now fixed at build time in both directions, and take an inventory of what the fork changes upstream.

#### Outcome:

The 40x26 grid is enforced at the right layer now. `ports/esp_idf/components/tinydesk/CMakeLists.txt` wrote its own `TD_MAX_COLS`/`TD_MAX_ROWS` unconditionally, so the port's declaration was discarded and the generous PSRAM defaults (256x96) applied; the fix is that a port which declares a grid now sets it, which is exactly the mechanism `td_config.h` documents. Clamping only the maximum was half a fix: the clamp in `src/td.c` is `clampi(reported, TD_MIN_COLS/TD_MIN_ROWS, TD_MAX_COLS/TD_MAX_ROWS)`, and the minimum was still the library's "smallest usable desktop" of 12 rows, so attaching a `screen` window smaller than the panel shrank the desktop -- reported by the operator as losing the bottom two rows of the glass and leaving a black band in the framebuffer. Reproduced by answering the size query with `24;40R`, which moved the taskbar to row 24. The component now pins the minimum to the declared grid as well, making the range degenerate, and `td_config.h` gains `#ifndef` guards on those two constants -- they were the only values in that file not overridable from the build system while its own header says any value can be. Verified on the watch by answering the size query deliberately wrong in each direction: `20;20R`, `24;40R` and `34;74R` all leave the taskbar on row 26, and the host suite is 37 of 37. The port also carried an earlier filter in `link_twatch.c` that removed the host terminal's cursor-position report from the USB input; the earlier entry justified it and this one supersedes that reasoning. It was the wrong layer -- it hid the size the terminal really has, when `td_stats` exposes `term_cols`/`term_rows` separately from `cols`/`rows` precisely so the difference between reported and in use stays visible -- and with the grid fixed it was unnecessary, so it is gone: about 120 lines and a byte-wise state machine that could swallow input if it ever mis-parsed a sequence. The fork now stands 19 commits above upstream v0.4.0. Six files of shared code are modified: `src/td.c` (the `TD_SIZE_POLL_MS` test moved to `#if`, which fixes a genuine `-Werror=type-limits` compile failure when the value is 0, since a run-time `unsigned >= 0` is always true), `include/tinydesk/td_config.h` (the two `#ifndef` guards), `ports/esp_idf/components/tinydesk/CMakeLists.txt` (the grid override described above), `apps/bluetooth.c` (full-screen layout computed from the client area), `src/widgets.c` (message boxes wrap to the screen instead of trusting caller line breaks) and `tests/test_bluetooth.c` (coordinates for the new layout). Everything else is new: `ports/twatchultra/`, about 3000 lines across sixteen files. Two build-level overrides remain in the port, `TD_SIZE_POLL_MS=0` and `TD_ACK_TIMEOUT_MS=0`, and both are now suspected of compensating for faults fixed elsewhere -- the first for a dirty cursor cell that the poll may no longer cause, the second for frame pacing that never switched off when the real lag was the mirror stall.

#### Next Steps:

Test the two remaining overrides against the current tree and either justify or remove them: `TD_SIZE_POLL_MS=0` should now be harmless to leave on but is probably redundant, and `TD_ACK_TIMEOUT_MS=0` needs measuring, because its stated justification was a round trip that costs a few bytes rather than lag. Then the apps, one at a time, reflowed for 40x26 -- and any app judged broken before entry 70 should be judged again, since the grid it was laid out against was not the one on the glass. The three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, and their recorded edges divide the strip into two equal zones plus a one-pixel third, so the vendor map settles that first. The Mouse and Keyboard slots pair one device each and share the Combo slot's working code path but have not been tried. When the fork is offered upstream, the four shared-code changes are the ones that need to stand on their own merits rather than on this port's needs, and `apps/bluetooth.c` is the most likely to be declined, since it changes a shared app's window shape. The Linux side stays parked; its last code commit is 8645a82 and everything after it in this repository is the project log.

#### Files Modified:

- ports/esp_idf/components/tinydesk/CMakeLists.txt
- include/tinydesk/td_config.h
- ports/twatchultra/CMakeLists.txt
- ports/twatchultra/main/link_twatch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 70 COMMIT Unreleased 2026-11-11T23:20:00-07:00

#### Coming From:

Unreleased 0e5149a

#### Purpose:

Record that the host terminal's size answer was reaching the window manager, and review the port's earlier patches against that finding.

#### Outcome:

The layout faults that had been treated as separate app bugs were one bug. The desktop asks its terminal for the size with `ESC[6n` and lays itself out for the answer, and two answers reached the link: the panel's, from `display.c`'s own `td_vterm` through the inject queue, and the host terminal's whenever one was attached over USB. `link_twatch.c`'s header comment stated the host's answer is dropped and `link_read` passed it straight through instead, so a `screen` session, which replies 74x34 by default, resized the whole window manager to 74 columns on a 40-column panel: windows measured themselves for 74, the panel clipped them, and text was cut mid-word with nothing to say so. That is what made the Bluetooth window and every message box look broken, and neither could be fixed properly while the grid the window manager believed in was not the grid on the glass. The filter now removes exactly `ESC[<rows>;<cols>R` from the USB side and nothing else -- the panel's answer arrives by the other path untouched, keys and mouse reports and pastes pass through unchanged, and a sequence that starts like a report but is not one is put back as literal text so no input can be swallowed. `td_msgbox` also wraps: it took each caller line at face value and measured the longest, which on a narrow screen clipped the text to "Forget the combo K06 BLE Keyboard? I", and it now re-wraps to the screen width, treating a single newline as a soft break and a blank line as a paragraph break, measuring the window from the wrapped result. Two faults were introduced and caught in that work, both worth recording because both looked reasonable: copying each source line into the `TD_TEXT_MAX` buffer before wrapping truncated anything over 48 columns, so the wrap faithfully wrapped what was left and a sentence lost its beginning; and the first filter could emit more bytes than it consumed, since an `ESC` pair expands when flushed, and that output was not bounded to the caller's buffer, so it overran `hal_mux`'s 64-byte buffer and took the screen down entirely. `test_bluetooth` asserted the old 72x34 layout -- columns 11, 23 and 31, rows 8 + 5i, scan row 30 -- and had been failing since the full-screen reflow rather than since today; its coordinates follow the computed layout now and the host suite is 37 of 37. Verified on the watch by answering the size query with 34;74R as a 74x34 terminal does: the desktop stayed 40x26 with the taskbar on row 26, and the Forget confirmation wrapped to complete sentences with its buttons on the last row. Reviewing the port's own earlier commits against this finding: the rectangle flush and the message-box wrapping are sound and stand on their own; the Bluetooth full-screen reflow is the right shape for a 40x26 panel but its commit message blames fixed constants for clipping that the size bug was causing; and `TD_SIZE_POLL_MS=0` is now doubtful, because its stated reason was a single dirty cursor cell while the poll may have been genuinely harmful only while two size answers were fighting over the grid.

#### Next Steps:

Two port-level overrides may now be compensating for faults that are fixed elsewhere and should be re-tested before they are kept: `TD_SIZE_POLL_MS=0`, whose justification was a dirty cursor cell, and `TD_ACK_TIMEOUT_MS=0`, whose justification was frame pacing never turning itself off. Both are one-file experiments with an obvious revert, and neither has been run against the current tree. The apps are the remaining work and are to be reflowed one at a time for the 40x26 grid, with the caveat that any app whose layout was judged broken before this entry should be judged again now, because the grid it was laid out against was not the one on the glass. The three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, and their recorded edges divide the strip into two equal zones plus a one-pixel third, so the vendor map is the place to settle that first. The Mouse and Keyboard slots pair one device each and share the Combo slot's working code path but have not been tried. The Linux side stays parked with its restore image intact: its last code commit is 8645a82 and everything after it in this repository is the project log, while all port code lives in the fork.

#### Files Modified:

- ports/twatchultra/main/link_twatch.c
- src/widgets.c
- tests/test_bluetooth.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 69 COMMIT Unreleased 2026-11-11T22:05:00-07:00

#### Coming From:

Unreleased da7f3b4

#### Purpose:

Record the cause of laggy pointer motion on Bluetooth input when nothing was draining the USB port, and the correction of a measurement that had been read wrong twice.

#### Outcome:

The symptom was precise and the operator supplied the discriminator: with a Bluetooth combo keyboard and mouse driving the watch, dragging the pointer was laggy whenever the watch was plugged into a PC with no terminal open, while the same desktop over a `screen` session was smooth. Only the input path and the state of the host differed, so the fault was at the link, not in the desktop's diff. It was `link_write` in `ports/twatchultra/main/link_twatch.c`, which wrote the USB mirror in 256-byte chunks with a 150 ms timeout each and then returned `len` regardless of what it had actually written; two faults in one function, a stall of up to about 450 ms per frame and a false report that every frame had been delivered. The gate that was meant to prevent this made it worse: `usb_serial_jtag_is_connected()` reports only that a USB HOST is present, by watching for SOF packets, and says nothing about whether anything is reading, so the mirror ran with no reader, the driver's 1 KB transmit ring stayed full, and every frame blocked the desktop while the panel already held a correct picture. The fix separates what the two outputs are for: `display_write()` always receives the whole buffer because the panel is local and authoritative and must never lose a byte, the mirror gets a fixed 15 ms budget per frame, and a mirror that cannot finish a frame is marked stale but deliberately not repaired, since a repaint requested there would cost a full compose and a full panel repaint on every frame while nobody is watching; the first frame that does get through completely is the proof that something is reading again, and that is what triggers the catch-up. `len` is still returned, because the mirror and the panel are the same call and a short count makes the renderer drop bytes the panel needs. Two failed attempts belong in the record because both looked reasonable: honouring the renderer's documented `write_failed` contract by returning a short count booted the board to a blank screen for exactly that reason, and a zero-wait write was too strict, because a single momentarily full ring dropped the frame and the identical retry then failed for ever. Both were caught by the screen coming back empty rather than by reasoning. The measurement discipline failed twice in this cycle and is worth naming: an instrument that logged large flushes was read while the Log Viewer was open, and the Log Viewer repainting itself produced the very full-screen rectangles that were then blamed on pointer motion, so the instrument measured its own observer; and separately a "cells" figure was counted from cursor-jump sequences rather than from characters, which made a whole-screen resync look like 25 changed cells. The operator's own report -- one large refresh on the first pointer move -- is what identified the resync correctly. Once the port was quiet the desktop was measured sending 30 to 49 single cells per frame during motion, never a full repaint, which is the diff doing its job. The operator confirmed the result as working perfectly with the mirror no longer blocking.

#### Next Steps:

The apps are the remaining work and are to be stepped through one at a time, reflowed for the 40x26 grid the way the Bluetooth window was, since several still lay themselves out for a screen larger than this board has. The three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, and their recorded edges divide the strip into two equal zones plus a one-pixel third, so the vendor map is the place to settle that before writing a fix. The Mouse and Keyboard slots pair one device each and share the Combo slot's working code path but have not been tried. Nothing outstanding is known about the display, touch, the USB link or Bluetooth. The Linux side stays parked at da7f3b4 with its restore image intact.

#### Files Modified:

- ports/twatchultra/main/link_twatch.c
- ports/twatchultra/board.example.conf

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 68 COMMIT Unreleased 2026-11-11T21:26:00-07:00

#### Coming From:

Unreleased 2d45777

#### Purpose:

Record that the panel renders one row taller than the measured rectangle, which is a geometry fact worth having before any later work assumes 320x400 is the hard boundary.

#### Outcome:

The measured unclipped rectangle for this glass is 320x400 at (45,45) -- 45 px inset on every side, which is what the 45-degree chamfers allow as a rectangle, and what the Linux driver used. That is now known to be conservative at the bottom edge. In `~/GIT/tinydesk-twatch` at 24ad3a2 on branch `t-watch-ultra`, the framebuffer is 320x416 and the desktop reports 40x26: one 16-pixel row added BELOW the measured rectangle, reaching glass row 461 where the measured bottom edge is 457. The last four pixels of that row are past the inset and are visible, because the chamfers cut the corners rather than the middle of a long edge. So the width is the hard limit and the height has slack -- which is the useful part of this record, since the reference library states the rectangle as a single fact and a later change could reasonably have taken 400 as a boundary not to cross. No code path needed altering: `DISPLAY_MAX_ROWS` and the terminal size both derive from `DISPLAY_H`, and the desktop asks the link for its size, so raising the height raised the grid. One wrong turn is worth recording too: an earlier attempt at this same experiment widened the framebuffer by one column each side instead -- 336 px and 42 columns, origin in to 37 -- and was built but never flashed, so nothing about the width was learned and no column experiment has been run. Verified on the glass: the log reads `tinydesk: desktop 40x26`, the taskbar moved down to row 26, and the Image Viewer icon that had been off the bottom filled the new row 25. The operator accepted it.

#### Next Steps:

The width has not been probed at all, and that is now the interesting direction: the same four pixels of slack probably exist at the left and right edges, so one extra column each side (336 px, 42 columns, origin 37) is the obvious next experiment and was built once without being flashed. The bottom row's slack is also unmeasured more precisely than "four pixels is enough", so if a later layout ever wants two more rows that is where the limit would be found. Separately: the three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, with the recorded edges dividing the strip into two equal zones plus a one-pixel third; the Mouse and Keyboard slots pair one device each and share the working Combo slot's code path but have not been tried. The Linux side stays parked at 2d45777 with its restore image intact.

#### Files Modified:

- ports/twatchultra/main/display.h
- ports/twatchultra/main/display.c
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 67 COMMIT Unreleased 2026-11-11T21:10:00-07:00

#### Coming From:

Unreleased bb7bf2e

#### Purpose:

Record the port's Bluetooth input path as verified on hardware, which was the last untested subsystem, and the two layout defects a real pairing exposed.

#### Outcome:

A Bluetooth combo keyboard-and-mouse paired from a scan into the Combo slot and drives the desktop, so the radio, the pairing path, the HID decode and NVS persistence are all exercised on the glass -- `td_hid.c`'s boot-format keyboard decoder and the mouse report map both, since a combo device sends both kinds of report down one link. This was the last subsystem the port had written but never run, and with it the port's entire input and output path is now verified end to end: panel, touch, USB link, and Bluetooth. The window's own status was the evidence rather than an inference -- `Pairings 1 stored in the board's NVS`, `Combo ready, K06 BLE Keyboard`, `last: connected: keyboard and mouse`, and `[ Off ]` reading as the reconnect toggle. Two layout defects appeared only once a slot was occupied, and both are fixed in the fork at 641f7bb. The first: a button is its caption plus four columns, so `Pair...`, `Off` and `Forget` with one column between them need thirty, and starting at the value column they ended at column 40 of a 40-column window -- cutting the closing bracket off `[ Forget ]`, on the one button a user reaches for when they want to let go of a device. The group now starts one column left and ends at 39. The second: detail lines were indented to the value column, spending eleven of forty columns on nothing, which is why a paired device read `paired: K06 BLE Keyboard D1:` with the address cut mid-octet; they start at column 1 now, and when the window is narrow the address is dropped rather than truncated, because the state and the device name identify it and the reconnecting flag is already carried by the button beside it. The app is shared with every other port, so it was built host-side under `-Werror` as well and is clean. The operator accepted the result on the glass.

#### Next Steps:

The Mouse and Keyboard slots pair one device each and are the same code path as the Combo slot that now works, but neither has been tried -- worth a pass only if there is a reason to use separate devices. The three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, and their geometry is unresolved because the recorded edges divide the strip into two equal zones plus a one-pixel third, so the vendor map is the place to settle that before a fix is written. Beyond those, the port's documented gaps are unchanged and deliberate: no Sixel or picture support, no sound, no haptics, no battery reporting, and SD hardware still unproven on this board. The Linux side stays parked at bb7bf2e with its restore image intact.

#### Files Modified:

- apps/bluetooth.c
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 66 COMMIT Unreleased 2026-11-11T20:52:00-07:00

#### Coming From:

Unreleased d7bb6b9

#### Purpose:

Record the port's partial-update rework and the Bluetooth window's reflow, both of which the operator accepted on the glass.

#### Outcome:

Two cycles are recorded together because they were driven back to back by the same acceptance test -- pointer inside the framebuffer and the Bluetooth window -- and the second could not be evaluated until the first stopped making every touch repaint. In `~/GIT/tinydesk-twatch` at f9b5270 on branch `t-watch-ultra`, the flush path was pushing 96 per cent of the framebuffer per pointer move. The cause was not the touch driver: `display.c` tracked the dirty region as runs of ROWS and then pushed each run at the full 320-pixel width. This desktop repaints the entire icon column on every pointer move, so 23 changed cells in one column formed a single contiguous run of rows and went out as a full-width band -- 122,880 pixels for a column that is 8 pixels wide. It now tracks the column extent of each run and pushes a rectangle, so the same move is 3,072 pixels, a 40-fold reduction; and the band height is sized to the rectangle so the column goes out as one transaction instead of 24, since at narrow widths the per-transaction cost of two window-register writes and taking the bus was exceeding the pixel data. Measured on the board with the flush timed and logged through the Log Viewer: 641 us average and 680 us maximum per flush in steady state, against roughly 16 ms for the same move beforehand. A second fault was made and caught inside that cycle: moving the byte-swap and DMA source buffer from PSRAM to a `MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA` allocation booted the board to a dark screen with no output at all, because with the Bluetooth controller holding internal RAM that request can fail and `CONFIG_ESP_CONSOLE_NONE` leaves nowhere to report it before the desktop link exists; it is a 10 KB static array now, internal by construction because `extram.lf` maps only libtinydesk, libtdsh and libmicropython to PSRAM. The Bluetooth window had a separate and simpler defect with a worse symptom: it opened as a floating 72x34 window on a 40x25 screen. The window manager clamps the RECTANGLE to the screen but cannot re-flow contents, so the device list and its buttons sat at rows 21 to 30, off the bottom of the glass -- which reads as a scan that finds your keyboard with nowhere to click it, and would read the same on any port with 25 or 30 rows, which is every ESP32 board here. It now opens full screen with every row derived from the client height the window manager actually returns, so drawing and widgets cannot disagree about where the bottom is; on 25 rows that leaves 14 for the three slots, then the scan header, status line, seven rows of device list, three buttons and Close. Text longer than the window is now shortened where it carries least meaning rather than truncated mid-value, and the window's status line moved from column 9 to 10 because "Close ]" occupies 0..8 and the two had been running together as "Close ]listening for devices". The app is shared with every other port, so it was also built host-side under `-Werror` and is clean there. Both results were accepted on the glass by the operator, who confirmed the picture and pointer inside the framebuffer as good and the reflowed window as good.

#### Next Steps:

Bluetooth pairing is still the one untested subsystem: a scan lists devices and they are now reachable, but no keyboard or mouse has been paired, so the HID decode in `hid_input.c` and the reconnect-on-boot path remain unexercised. The three capacitive bars under the glass still act on stretches of taskbar rather than three fixed cells, and their geometry is unresolved because the recorded edges (22, 158, 294) divide the strip into two equal zones plus a one-pixel third -- so the vendor map is the place to settle that before any fix is written. The Linux side stays parked at d7bb6b9 with its restore image intact.

#### Files Modified:

- apps/bluetooth.c
- ports/twatchultra/main/display.c
- ports/twatchultra/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 65 COMMIT Unreleased 2026-11-11T20:26:00-07:00

#### Coming From:

Unreleased e1dcf8c

#### Purpose:

Correct a mapping figure that entry 64 stated from memory rather than from arithmetic, before it is used as the basis for the next cycle.

#### Outcome:

Entry 64 described how the three bars under the glass map onto the desktop's taskbar and got the first of them wrong; this entry supersedes that figure. The claim was that bar 0 clamps to taskbar column 0 and that only bar 1 spreads. Computed directly from the constants this project recorded -- `CST92XX_BTN_Y0` 440 and edges 22, 158 and 294 (TCH-004), against `to_cell()` as it now stands with origin 45 and an 8x16 cell -- bar 0 occupies x 22..157 and therefore spans columns 0 through 14, not column 0 alone; bar 1 occupies x 158..293 and spans columns 14 through 31; and bar 2 is x 294 and nothing else, a single pixel column, because `cst92xx_button_at()` admits x up to and including 294 and then assigns 294 itself to the third bar. So all three bars act on stretches of the taskbar rather than on single cells, and the last one is effectively unreachable. The underlying conclusion in entry 64 stands unchanged -- the bars sit below the 320x400 framebuffer and mapping them through its grid is the wrong model -- but the numbers are now recorded as measured rather than as guessed, because the next cycle's fix depends on them and a wrong figure in this log is exactly the kind of thing a later session would build on. Nothing was delivered to the device and no firmware changed: this is a correction to the project record.

#### Next Steps:

Give the three bars their own fixed cells instead of deriving them through the framebuffer grid, and settle what the third bar actually covers before writing it, since the recorded edges leave it one pixel and that is more likely a transcription artefact in TCH-004 than the real geometry -- the vendor's own map for this strip is the place to check. Separately, Bluetooth pairing is still untested: the radio starts and the apps exist, but no keyboard or mouse has been paired.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 64 COMMIT Unreleased 2026-11-11T20:20:00-07:00

#### Coming From:

Unreleased 0267262

#### Purpose:

Record the operator's hardware acceptance of the port's input path, which entry 63 wrote as not run.

#### Outcome:

The operator confirmed on the glass that touch responds and that Wi-Fi associates with the credentials already stored on the device, and accepted the cycle. That completes the evidence entry 63 was missing and supersedes its `User Test: NOT RUN`; nothing in the delivered firmware changed. Touch was the one link left unproven, because entry 63 could show the driver reading and could show an injected SGR click opening the start menu, but could not show a finger on the glass -- the operator's tap is that evidence and it arrives without any further change to the driver. Wi-Fi remembering its credentials also closes the risk entry 63 raised about regenerating `sdkconfig`: deleting that file to make the new Bluetooth defaults apply could have discarded hand edits made to it, since it is generated and gitignored, and a fresh association from stored credentials proves the previous configuration carried nothing that this one lost. One behaviour the operator called out is worth recording as intended rather than as a fault: Wi-Fi does not associate at boot, and connecting from the Network app afterwards works, which is the operator's preference for this board. The Bluetooth radio is now in the build and its Mouse, Keyboard and Bluetooth apps appear in the start menu, so `bt_hid.c` is compiled and `bt_hid_start()` returns a table rather than the empty stub -- but no device has been paired, so the Bluetooth keyboard and mouse path remains written and enabled without having been exercised, and nothing in this entry should be read as claiming otherwise.

#### Next Steps:

Pair a Bluetooth keyboard and mouse from the desktop's Bluetooth app: the radio starts and the apps exist, but no device has ever been paired, so the HID decode in `hid_input.c` and the reconnect-on-boot path are untested on this board. Separately, the three capacitive bars under the glass are now reachable with working touch and their mapping deserves a decision: `to_cell()` converts a touch through the framebuffer's 320x400 grid, which is right for the screen but wrong for the bars, since they sit below it -- bar 0 clamps to taskbar column 0, but bar 1 spans columns 14 to 31 rather than landing on one cell, so pressing the middle bar acts on whichever taskbar column happens to be under the finger. Giving the bars their own three fixed cells would make them behave as three buttons. Boot-time Wi-Fi association stays off by the operator's choice.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 63 COMMIT Unreleased 2026-11-11T20:05:00-07:00

#### Coming From:

Unreleased 0e8ddba

#### Purpose:

Record the TinyDesk port's first hardware-verified input path, closed after the earlier entry called touch and the once-a-second flash open.

#### Outcome:

Both open faults are fixed in `~/GIT/tinydesk-twatch` at 084e1d7 on branch `t-watch-ultra`. The flash had two layers and the earlier fix never reached the code: `TD_SIZE_POLL_MS=0` was set twice, but `src/td.c` belongs to the shared `tinydesk` component, so a definition on the port's own component could not reach it, and `add_compile_definitions` at project scope runs after ESP-IDF registers components, so only the port's `main` inherited it. Counting the generated `compile_commands.json` proved it -- 19 translation units carried the define and `src/td.c` was not one of them -- and it is now set from `main/project_include.cmake`, which ESP-IDF evaluates before component registration. That exposed an upstream defect worth reporting: `TD_SIZE_POLL_MS=0` did not compile, because `td.c` tested the value as a run-time condition and then compared `unsigned >= 0`, which `-Werror=type-limits` rejects; written as `#if` it matches the file's own convention and the disabled build carries no dead branch. The repaint was also structural -- `paint_task` kept one span from the topmost changed row to the bottommost, so the size query, which parks the cursor in the bottom-right corner to measure the terminal, stretched the span over all 400 rows. Flushing contiguous runs fixed that, and `TD_ACK_TIMEOUT_MS=0` removed frame pacing, which never switched itself off because the port's own `td_vterm` answers `ESC[5n` and feeds the answer back as desktop input, so every frame paid a query, a flush and a reply for a round trip that controls nothing. On the wire in 15 idle seconds: zero `ESC[5n`, one `ESC[6n` (the one-time setup query), and idle frames carrying no cells. Touch failed for a different reason and was never going to work. The driver implemented the `0xD1=0x1E` unlock handshake this project retracted on 2026-11-10 (TCH-002 revision, TCH-007): those writes belong to SensorLib's `setMode()`, whose only caller is `sleep()`, and a working part reads back `0x00 0x09` at `0x0002`. Requiring `0x1E` made the driver refuse a healthy controller. Behind that sat the real fault, visible only after the retracted handshake was removed: the command-mode entry is a TWO-byte write (`0xD1 0x01`), while a register write carrying a value is three, and the port sent the two-byte command as three, addressing register `0x00D1` -- which the part accepts and ignores -- so command mode was never entered and the `0xD1FC` checkcode read returned normal-mode content. The part acked every transfer, so the failure surfaced as "bad checkcode" rather than a bus error and looked like absent hardware. Three more faults came out of comparing against the measured driver: the poll never sent the `0xD000=0xAB` acknowledge that advances the controller's buffer, it required byte 0 `== 0x06` exactly instead of the low nibble, and it never checked byte 6's `0xAB` validity marker. Bluetooth had never been in the build at all: `sdkconfig.defaults` carried no BT settings, so `CONFIG_BT_ENABLED` was unset, `bt_hid.c` compiled to its empty stub, and the Mouse, Keyboard and Bluetooth apps were absent from the start menu -- which is the quickest way to see it and is now the way to check it. NimBLE is enabled as a central with limits from `ports/nanoesp32`, its memory in PSRAM and soft coexistence with Wi-Fi, since the watch is meant to be driven by a Bluetooth keyboard and mouse and the glass is too small to type on. `to_cell()` also subtracted no origin, so every touch would have landed 45 px down and right with the right-hand fifth of the screen unreachable. Read from the Log Viewer over the USB link, the bring-up now passes and leaves evidence: `rst 8=1 10=1`, `CST9220 410x502`, `CST9217 up at 0x1A`, then `300 rd 0 rep` -- the poll running, which only starts if bring-up succeeded. The pointer path is proven end to end independently of the panel: an injected SGR click over the link opens the start menu, so what touch.c emits reaches the desktop. The radio is not a regression from the Bluetooth work: the Network app scans twelve networks, so Wi-Fi is functional and simply not associated.

#### Next Steps:

Two things need the operator's hands because neither can be simulated. First, a finger on the glass: the driver reads and reports but no press has been seen (`0 rep`), so the decode from controller coordinates to taskbar cells is the one link left unproven. Second, Bluetooth pairing from the desktop's Bluetooth app -- the radio starts and the apps exist, but no device has been paired. Separately, Wi-Fi is offline and the log shows no association attempt at all, which is either that this build does not auto-reconnect or that it needs credentials again; reconnecting through the Network app distinguishes those, and it is worth knowing which because `sdkconfig` was regenerated to enable Bluetooth and the previous file, which was generated and gitignored rather than tracked, carried whatever hand edits it had. The Linux side stays parked at 0e8ddba with its restore image intact.

#### Files Modified:

- ports/twatchultra/CMakeLists.txt
- ports/twatchultra/README.md
- ports/twatchultra/board.example.conf
- ports/twatchultra/sdkconfig.defaults
- ports/twatchultra/main/project_include.cmake
- ports/twatchultra/main/display.c
- ports/twatchultra/main/display.h
- ports/twatchultra/main/power.c
- ports/twatchultra/main/touch.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 62 COMMIT Unreleased 2026-11-11T12:24:00-07:00

#### Coming From:

Unreleased 8645a82

#### Purpose:

Record the TinyDesk port's first working state and what remains broken, because that work now lives in a separate repository and the log is the only place this project records it.

#### Outcome:

The port renders a desktop on the AMOLED and is not finished. The work is in `~/GIT/tinydesk-twatch`, a fork of TinyDesk v0.4.0 (commit fce4c5a) on branch `t-watch-ultra`, seven commits, with `ports/twatchultra/` written against the upstream v0.4.0 tree rather than the stale local `../tinydesk-work` checkout, which sits on a nanoesp32 branch well behind it. Getting the panel lit took four faults and every one of them failed silently. The AMOLED's rail is switched by XL9555 P07 and nothing else, and on a cold boot the expander has every pin configured as an input, so the panel was unpowered while a correct init sequence went into it. The QSPI framing was wrong twice before it was right: register writes need `SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR` on a device configured `command_bits = 8, address_bits = 24, spics_io_num = -1, SPI_DEVICE_HALFDUPLEX`, which was finally transcribed from LilyGo's own `LilyGoDispQSPI` rather than derived. The reset pulse is 200/300/200 ms and an earlier 20/120 ms left the controller held in reset. And the picture was offset until the framebuffer's true origin was applied: the 320x400 rectangle sits at (45,45) on the glass and the controller's RAM starts a further 22 columns left, so a pixel is written at x+45+22 and y+45. Colours were a separate defect in this project's own code: `td_vterm` stores colour INDICES and the port masked them with `& 0x0F`, which folded every 256-colour index into the wrong entry, so the desktop appeared in colours nobody chose while the same bytes rendered correctly in a PC terminal that expands the indices itself. `s_palette` is now the full xterm 256 collection. The operator confirmed the desktop looks right on the glass. **Two things remain broken and neither is disguised.** Touch does not work. The controller's reset is on the XL9555 expander rather than an SoC GPIO and nothing drove it; two sources disagree on the pin, LilyGo's hardware page saying expander GPIO10 and this project's TCH-005 saying P08, so the port now drives both and logs each rather than guessing, and touch still does not respond with that in place. And the display, and the USB mirror with it, flashes a full screen once a second. The capture isolates it exactly -- `ESC[?2026l ESC[5n ESC[7 ESC[999;999H ESC[6n ESC[8 ESC[?2026h` wrapped around every frame -- which is the desktop's terminal-size query, asked because a serial terminal cannot report a resize, and answered by moving the cursor to the bottom-right corner where the cells it touches repaint. `TD_SIZE_POLL_MS=0` was set twice, first on the port's own component and then at project scope via `add_compile_definitions`, and the capture still shows the query, so either the definition is not reaching `td.c`'s translation unit or something other than the poll interval is emitting it. That is unresolved and is stated as unresolved. One retraction belongs in the record: the repeated `ESC[6n` was called a stuck loop for several cycles, and it is not -- `TD_SIZE_POLL_MS` defaults to 1000, so one query a second is designed behaviour, and the fault is its visible cost on this panel rather than its existence. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 61, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Three things, in the order that unblocks the most. First, grep the port's generated `compile_commands.json` for `TD_SIZE_POLL_MS=0`: if it is absent the definition never reached the compiler and the fix is to find where the `tinydesk` component's flags are actually set, in `ports/esp_idf/app/app.cmake`; if it is present then the emit is in the frame path rather than the poll, and the place to look is `td_render_query_size`'s callers in `src/td.c` and whether a malformed or ignored resize reply leaves the desktop querying forever. Second, touch: the reset is now driven on both candidate expander pins, so the next failure is either the CST9217 not answering at 0x1A at all -- which `bring_up()` logs as the checkcode it read -- or the controller answering and the SGR mouse reports not reaching the desktop. Those are distinguishable from the log and should be distinguished before any more code is written; the CST9217 handshake in `touch.c` is transcribed from this project's measured TCH-007 sequence and has not been questioned. Third, the fork has to be pushed somewhere under `aquasock` before it is a contribution, and the systems table in the port README already states which subsystems are verified and which are not -- display verified, touch written but unverified, `/fs` and WiFi untested, SD hardware unproven on this board, and battery, RTC and haptics deliberately absent because TinyDesk has no consumer for them. The Linux side of this project is parked, not abandoned: `main` is at 8645a82 with the flash fix, the hatch yield and the display rework all committed and pushed, and a full restore image of the state before the port overwrote the partition table is at `~/twatch-backup/pre-tinydesk-2026-11-11-1130.bin`, sha256 cd0dd018.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 61 COMMIT Unreleased 2026-11-11T11:18:00-07:00

#### Coming From:

Unreleased fe6fd1e

#### Purpose:

Record the full-paint test switch as a documented setting rather than an untracked local edit, and note that enabling it was observed to destabilise the board, so the Linux work is left in its verified state before the project changes direction.

#### Outcome:

The switch exists and defaults off. `SHM_FULL_PAINT` in `tamacore_task.c` bypasses the dirty path entirely: with it at 1 every publish redraws and flushes the full LCD area regardless of how little changed, which is the honest worst case and the baseline any further display work should be judged against; with it at 0 behaviour is exactly as entry 60 left it and the shadow copy is still maintained, so toggling needs no other change. It is committed defaulting to 0 because turning it on was not free. A run with full paint enabled reset the board with reason 5, `ESP_RST_INT_WDT`, at least once. That is consistent with the increased frame cost driving the interrupt watchdog, but it was not characterised -- the run was abandoned when the operator decided to change direction -- so the comment records the observation and explicitly does not claim a mechanism, only that the setting has been seen to destabilise the board and that the default is therefore the verified-good path. The firmware was rebuilt and flashed after setting the default back to 0, so the source and the image on the watch agree, and the board was confirmed booting and stable at 38 s uptime on kernel #16. **This entry closes the Linux line of work, deliberately and not as a failure.** The operator's reason, recorded because it is the evidence any future decision should rest on: the board fights the OS on four separate counts and every one is a property of Linux on this SoC rather than a defect in anything built here. The kernel is XIP and fetches its instructions through the same shared cache that core 0 disables during a flash operation, so a flash write could kill core 1 outright (entries 52 to 54). Internal SRAM is directly addressable but no region of it is allocated to Linux, so the kernel has nowhere to run while that cache is off (SOC-004). NOMMU forces a patched fork stack and contiguous load segments. And core 0's idle task starves whenever the emulator falls behind the wall clock, which the task watchdog punishes with a reset (entry 57). Each of those cost a cycle or more to find and each is inherent to the arrangement rather than fixable by more work in the same direction. The plan is a fork and port of TinyDesk to the T-Watch Ultra: a desktop port contributed upstream to that project's author, then a console build in the shape of the current Linux front end for the Tama work. The reference material for that migration was gathered during this cycle and is recorded in Next Steps rather than thrown away. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 60, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The TinyDesk work starts from upstream `main` at v0.4.0, commit fce4c5a, not from the local checkout in `../tinydesk-work`, which sits on a `nanoesp32-node` branch well behind it. Two facts found while scoping are worth carrying forward because they contradict the obvious assumption. The operator's nanoesp32 port IS merged upstream, but as a desktop port only: it has no `main_console.c`, no `vt.c` and no `ili9340.c`, so the console mode is on the branch and not in main. And the template to copy is `ports/crowpanel7`, not nanoesp32: it is an ESP32-S3 with 8 MB PSRAM, an 800x480 LCD and a GT911 touchscreen, and it is the only upstream port that has both a display and a touch panel, which is what the watch has. Its shape is `display.c` at 1085 lines painting cell grids, `touch.c` at 223 translating the controller to SGR mouse sequences, `link_crowpanel.c` at 278, four bitmap fonts, and the usual CMakeLists, sdkconfig.defaults, board.example.conf, partitions.csv and extram.lf. The two contracts a new port implements are small: `include/tinydesk/td_vterm.h` defines a four-byte cell of char plus two colour bytes with a dirty flag, which a display port paints; and the touch contract is a single callback taking SGR mouse bytes, `bool touch_init(void (*input)(const char *, int))`. Three things differ from crowpanel7 and all three are already solved in this repository: PSRAM is QUAD where crowpanel7 sets octal, so the new port must set QUAD or it stops at boot (patches/0001-watch-quad-psram.patch, SOC-002); the panel is a CO5300 QSPI AMOLED rather than an RGB parallel LCD, and `patches/kernel/display/final/co5300.c` has the framing, window addressing and byte order; and the touch controller is a CST9217 rather than a GT911, with the unlock handshake and report block in `patches/kernel/touch/final/cst92xx.c`. Flash is 16 MB against crowpanel7's 4 MB, so the partitions have room. At the 8x16 font a 320x400 panel is 40x25 cells. The sequence agreed with the operator is the desktop port first, as the contribution to give upstream, then a console build in the shape of the current Linux front end for the Tama work. Nothing about the Tama emulator has been ported and no Tama code belongs in the upstream contribution.

#### Files Modified:

- patches/harness/core0/tamacore_task.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 60 COMMIT Unreleased 2026-11-11T10:51:00-07:00

#### Coming From:

Unreleased 759b222

#### Purpose:

Record the operator's acceptance of the corrected display, which entry 59 could not state because it was written before the board was looked at.

#### Outcome:

The operator reports that the display renders properly and accepts it, so the geometry correction in entry 59 is verified on the glass and the `User Test: NOT RUN` it recorded is now settled. This entry exists rather than an edit to entry 59 because a committed entry is settled history and the result arrived after the commit; the earlier entry is named here so the two are read together. What the operator confirmed is the thing the cycle set out to fix: the pet is a 32x30 dot matrix and the ten status icons render as glyphs rather than as quantised blocks, which is what the 49x49 map had been producing by rasterising them at 49 pixels and magnifying every map pixel six times. The fix is therefore confirmed at all three stages -- the two coordinate spaces emitted by `gen_map.py`, the renderer drawing matrix as dot blocks and icons as runs, and the shared contract in `shmproto.h` agreeing with the generated map under the static asserts. Two things stated as incomplete in entry 59 remain incomplete and are unaffected by this acceptance. The two-band dirty tracking is computed but not published: `lo1`/`hi1` for the top icon row and `lo2`/`hi2` for the matrix and bottom icons are tracked separately in `tamacore_task.c`, but `dirty_lo`/`dirty_hi` in `struct shm_ctl` still carry their union, so core 1 blits one range and the blit-side saving of an icon flip is not yet realised. And the draw cost after the change has not been measured against the pre-change figure: the 67.8 ms `draw_max` seen on the first samples of a fresh boot includes the initial full paint and is not comparable to the 47 ms steady-state number in entry 58, so no claim is made here about whether the smaller frame made drawing cheaper. Nothing else was changed by this cycle.

#### Next Steps:

The two-band blit is the next piece and is the only part of the icon optimisation still on the table, since the drawing side already resolves to per-item spans. It needs a second `(lo, hi)` pair in `struct shm_ctl` and `userspace/tamacore.c` taught to blit both ranges, which makes it a shared-structure change: both halves flashed together and `scripts/check-vectors.sh` passing first, exactly as with entries 54 and 59. Worth measuring before and after, because the benefit depends on how often the two bands are genuinely disjoint in practice -- the assumed case is a status icon toggling while the pet is idle, but the pet animates continuously, so the matrix band is usually dirty anyway and the saving may be smaller than the model suggests. A clean steady-state `draw_max` and blit measurement on the corrected geometry should come first, since that is the number any further display work would be judged against and entry 59 deliberately left it unclaimed. Beyond that the `HARNESS` build-time dependency in `patches/harness/segmap-1x/gen_map.py` is still an outside-tree reference that should be vendored or documented.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 59 COMMIT Unreleased 2026-11-11T10:40:00-07:00

#### Coming From:

Unreleased c9c8a45

#### Purpose:

Render the V3 display as what it actually is -- a 32x30 dot matrix plus ten status icons drawn as glyphs -- instead of quantising both onto one 49x49 grid.

#### Outcome:

The geometry is corrected and deployed. The root cause was that `gen_segmap.py` computed one bounding box over all 970 artwork segments and emitted a map at 49x49 *pixels*, which the renderer then multiplied by the dot pitch. Two consequences followed, and both were defects: the 49x49 figure was the whole artwork box (1219.7 units divided by the 25.1 unit pitch), not the LCD, so the matrix was only 32x30 of it; and the ten status icons were rasterised at 49px and then magnified six times, which is why they looked chunky rather than like the artwork's smooth glyphs. Measuring the artwork settled both: of 970 segments, exactly 960 are 22.5x22.5 unit squares forming a complete 32x30 grid with no gaps or strays, and exactly 10 are complex glyphs 120-150 units wide in two rows of five. `patches/harness/segmap-1x/gen_map.py` replaces the old generator and emits two coordinate spaces, because the two objects are not the same kind of thing: the matrix as a 960-entry table of packed byte/bit cells in DOT units, and the icons as full-resolution horizontal runs in LCD pixels. Rendered output was checked visually before any of it was wired up -- five smooth glyphs above, a clean 32x30 grid, five smooth glyphs below -- which is the check the old map would have failed and did not have. `render.h` now draws the matrix as DOT x DOT blocks on a (DOT+GAP) pitch and the icons as glyph runs, and still owns every pixel in the range so a segment turning off is erased rather than left showing the previous frame. `shmproto.h` carries the matrix as 32x30 dots and the LCD as 292x300 pixels, and `_Static_assert`s in `render.h` now fail the build if the generated map and the shared contract ever disagree -- they cannot include each other, the map being generated and the header being in another tree, so an unchecked pairing would have been silent. The dirty-row tables were rebuilt for the new model: they are keyed by drawable item rather than by byte, so a changed byte is XORed with its shadow and only items whose bit actually changed contribute, and the top icon row is tracked as a separate band from the matrix and bottom icons. The firmware went from 4 percent free to 8, because 5366 runs of const data were replaced by 120 bytes of matrix plus 620 icon runs. Booted and deployed: firmware and userspace both flashed, `check-vectors.sh` green, emulator running with frames advancing. **What is not verified, and this matters.** The operator has not yet confirmed the picture on the glass. The two-band dirty tracking is computed but not yet delivered to userspace -- `dirty_lo`/`dirty_hi` still publish the union of both bands, so core 1 still blits one range and the blit-side saving is not yet realised; the drawing side does benefit. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 58, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The glass check is the first thing, because everything else was verified off-device and the operator has not looked yet. Then the two-band blit: the bands are computed in `tamacore_task.c` as `lo1`/`hi1` for the top icon row and `lo2`/`hi2` for the rest, but only their union is published, so `struct shm_ctl` needs a second pair of dirty fields and `userspace/tamacore.c` needs to blit both ranges. That is a shared-structure change, so both halves must be flashed together and `check-vectors.sh` must pass, exactly as with this cycle. The frame is now 300x308 rather than 324x348, so a full repaint is smaller, but nobody has measured the new draw and blit cost against the old numbers -- `draw_max` read 67.8 ms on the first samples of a fresh boot, which includes the initial full paint and is not comparable to the 47 ms steady-state figure in entry 58; that wants a clean measurement. The `gen_map.py` script depends on `HARNESS` pointing at the TamaHarness checkout for its path parser, which is a build-time dependency on a tree outside this repository and should either be vendored or documented in `docs/`.

#### Files Modified:

- patches/harness/segmap-1x/gen_map.py
- patches/harness/segmap-1x/segmap.c
- patches/harness/segmap-1x/segmap.h
- patches/harness/core0/render.h
- patches/harness/core0/shmproto.h
- patches/harness/core0/tamacore_task.c
- userspace/tamacore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 58 COMMIT Unreleased 2026-11-11T10:27:00-07:00

#### Coming From:

Unreleased 4faadf7

#### Purpose:

Stop a single status-icon change from dirtying nearly the whole display, by resolving dirty rows per segment instead of per display byte, as the first step of correcting the LCD geometry.

#### Outcome:

The dirty-row resolution is fixed and measured; the geometry correction it was meant to accompany is not started, and this entry says so rather than implying otherwise. The old table was per BYTE: `build_byte_rows()` took a min/max over every segment sharing that byte, and the diff loop used the byte's whole span when it changed. That is wrong whenever one byte holds segments at opposite ends of the screen, and the ten status icons do exactly that -- bytes 0, 1, 5 and 6 each carry one icon from the top row and one from the bottom row -- so flipping a single two-state ON/OFF glyph dirtied very nearly the whole frame. It is not an icon-only problem either, because 970 segments share 512 bytes, so multi-segment bytes are the norm; the icons are simply where it is most visible. The fix resolves the diff per segment: a changed byte is XORed with its shadow to get the changed bits, only the segments whose bit actually changed contribute, and each contributes its own precomputed row span. Segments are grouped by byte with a counting sort built once per session, so a changed byte finds its segments without scanning all 970. Two details mattered for correctness rather than speed: the `fill[]` table is 1 KiB and this task runs on a 4 KiB stack, so it is static rather than automatic, and `byte_seg`/`fill` are `uint16_t` because 970 exceeds a byte. Measured on hardware after flashing: frames advance, `draw_max` fell from the 56-71 ms seen in the previous entries to 47 ms, and the board boots and runs normally with the pet playing. **What this cycle did not do, stated plainly because the operator asked for more than this.** The real complaint is that the V3 display is a 32x30 dot matrix plus 10 status icons, that our map is 49x49 because `gen_segmap.py` scales the whole artwork bounding box rather than the matrix, and that the icons are being rendered as quantised pixels inside that grid instead of as overlay glyphs. Investigation confirmed all of it from the artwork itself: parsing `tinytama/assets/Tamagotchi_v3.svg` gives 970 segment ids, of which exactly 960 are 22.5x22.5 unit squares forming a perfect 32x30 grid -- columns 0-31 and rows 0-29, 32 times 30 being 960 with no gaps and no strays -- and exactly 10 are complex glyphs 120-150 units wide in two rows of five. Our 49 is simply 1219.7 artwork units divided by the 25.1 unit dot pitch, i.e. the whole artwork box and not the LCD. None of the rendering or geometry changes were made: the generator, `segmap.h`, `SHM_LCD_DOTS_W/H`, `SHM_DOT`/`SHM_GAP` and both renderers are untouched, and the map is still 49x49 with the icons quantised into its outer rows. That work was deferred deliberately rather than half-done, because it spans the generator, the shared-memory contract and both renderers, and a partial change to that contract is exactly the class of fault that has cost this project several silent failures. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 57, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The geometry rework is the next cycle and the facts it needs are already measured, so it should not re-derive them. The plan is to split the generator's output into two maps: the 32x30 matrix as a bitmap of 960 bits, which is 120 bytes and which the existing `byte_bit` segment index already addresses directly, and the 10 icons as their own run-lists with their own bounding box. Rendering the matrix then becomes 960 bit tests expanded into DOT x DOT blocks, replacing 4800 per-segment run lookups, and the icons become overlay glyphs drawn at matched scale above and below the matrix, which is what the hardware does. The arithmetic for the chosen 5 px dot is a pet area of 160x150 where the frame is currently 293x293, so the frame shrinks and the per-frame work falls with it; the operator chose 5 px with a 1 px gap and icons as proper glyphs. The icons are two-state only, which this cycle's per-segment dirtying now exploits properly -- an icon flip dirties that icon's rows and nothing else -- and the same reasoning suggests going further: an icon's glyph pixels only change when its state changes, so a flip could repaint just that glyph's rows without re-running the matrix, provided the dirty band is computed from the two sub-rectangles rather than from a single lo/hi pair, which is the one structural limit of the current representation and should be addressed in the same cycle. This touches `shmproto.h`, so `scripts/check-vectors.sh` must pass and both halves must be flashed together; the firmware and userspace read `SHM_LCD_DOTS_W/H` and `SHM_DOT`/`SHM_GAP` from that header and currently agree only because the map came from `patches/harness/segmap-1x/`. A full backup before the first flash of that change is warranted.

#### Files Modified:

- patches/harness/core0/tamacore_task.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 57 COMMIT Unreleased 2026-11-11T10:15:00-07:00

#### Coming From:

Unreleased 0224375

#### Purpose:

Fix the freeze at the hatch, which turns out to be a task-watchdog starvation of core 0 rather than anything to do with flash, and repair `watch-exec.py`, which was misreporting healthy boards as hung.

#### Outcome:

The hatch works and the operator has confirmed it on the glass: the pet hatched, survived being named, and is walking around. **The diagnosis was wrong twice before it was right, and the instrument is what corrected it.** Entry 54's flash fix was real and holds -- `flash_max` measured 10.25 ms and `chunk_max` 2.13 ms during the failures, with no evidence of a cache window being involved -- but the reboot at the hatch was a different fault. Reading `reset_reason` at the correct offset (112, not 28 as this session had been reading it) gave **6, ESP_RST_TASK_WDT**, not 5, ESP_RST_INT_WDT. `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y` with `CONFIG_ESP_TASK_WDT_TIMEOUT_S=10` makes that a statement about core 0's idle task: it did not run for ten seconds. The pacing loop in `tamacore_task.c` had exactly one `vTaskDelay`, in the branch taken when the emulator is AHEAD of the wall clock. When the guest computes continuously the board cannot keep up, `cycles_done >= target` is never true, that branch is never reached, and a priority-1 task that never blocks starves priority-0 idle outright. Core 0's radio and IPC tasks starve with it, which is why the console went silent before the reset rather than at it -- the freeze and the reboot were one fault seen at two moments, not two faults. **The measurement was asked for before the fix and it changed the fix.** Instrumenting slice duration into three borrowed words of `shm_diag` gave `slice_us_max` of 21-33 ms and `slice_since_yield_us` climbing to 6,257,613 us at the moment of death. Slices were therefore fine and the slice cap was doing its job; what failed was that the loop never blocked. That pointed at a wall-clock yield rather than a per-slice one, which matters because yielding every slice would have cost about 17 percent of throughput and was not needed. The fix is `YIELD_INTERVAL_US` 100000: if 100 ms have passed since the last block, call `vTaskDelay(1)`. It costs one 10 ms tick per 100 ms only while catching up, and normal play never reaches it because it takes the ahead-of-clock branch every iteration. Verified after flashing: `slice_since_yield_us` now caps at 162147 us instead of growing without bound, `slice_us_max` is unchanged at 35 ms, frames keep advancing, uptime ran continuously to 238 s and beyond, and the hatch and naming both completed with the pet walking. **`watch-exec.py` was a real defect and cost this session hours.** Two bugs. It accumulated console output across the whole wait and tested for `# ` anywhere in it, so after repeated resets had left several prompts stacked it could act on a `login:` that had already been answered, or type credentials into a live shell; it now decides from the last 400 bytes only, tracks whether it has already sent a username, and gives up rather than looping. And `main()` ignored the return value of `login()`, so when login did not take it typed the command into a `login:` prompt and then waited out its full 180 s timeout for a marker that could never arrive -- which reads as "the board hung" when the board is healthy. It now exits with a specific message. Both are fixed and the tool was verified across four consecutive invocations. Recording the cost honestly: this session repeatedly blamed the board for what were tool failures, and the operator had to say so. `patches/harness/core0/0003-*.patch` was regenerated again and re-verified -- firmware half applies to a clean c5423bd and produces byte-identical files, esp-idf half byte-identical to the live submodule. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 56, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Two records and one document now state the wrong cause for the hatch freeze and should be corrected before anyone plans around them. `docs/tamagotchi.md` records the reboot mechanism as the shared-cache flash fault from entry 54, which is true of a different failure and not of this one, and it should gain the task-watchdog finding rather than have the flash finding removed -- both faults are real and both were measured. The reference is unaffected in its flash records but the watchdog relationship deserves its own record, since "core 0's idle task starves when the emulator cannot keep up" is a constraint on any future work that puts computation on core 0 and it cost three sessions to find. The slice instrumentation added to `shm_diag` is worth keeping rather than removing: it answers a question that will recur, the three words were already reserved, and it is how this fault was found. `watch-exec.py`'s login hardening has one known weakness that was not exercised -- a console with several stacked prompts can still resolve against the newest of them, which is correct, but the tool does not clear the backlog first, so an unusually deep stack could still confuse it; it has not been observed since the fix and is not worth more work unless it recurs. The 100 ms yield is a throughput trade made under duress and should be reconsidered once the emulator is otherwise stable: at the hatch it costs about 9 percent, and a smaller interval or a yield that only engages while behind for longer than a few hundred milliseconds would recover most of that, but it needs measurement rather than another guess.

#### Files Modified:

- patches/harness/core0/tamacore_task.c
- patches/harness/core0/shmproto.h
- patches/harness/core0/0003-flash-chunking-and-cache-instrument.patch
- scripts/watch-exec.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 56 COMMIT Unreleased 2026-11-11T10:00:00-07:00

#### Coming From:

Unreleased 6d14186

#### Purpose:

Remove the two stale rationales that entry 55 flagged as still present in the tree, and confirm the deliberate no-autostart decision is recorded rather than merely implemented.

#### Outcome:

Both stale rationales are corrected, a third was found alongside them, and the autostart decision turned out to need no fixing at all. `linux_flash.c` described chunking as the fix for the reboots, which entry 54 disproved: chunking bounds each uninterruptible window and is still worth keeping for that, but it did not stop the reboots, because a 1 KB write -- one chunk -- rebooted the board as reliably as 32 KB while the longest window measured 0.51 ms against an 800 ms watchdog. The comment now says what chunking does and states explicitly that it is not the reboot fix, records why the diagnosis was wrong, and names the staging buffer as the actual fix. `docs/tamagotchi.md` carried the same class of error in a different form: it said the reboot mechanism was not proven and was consistent with the phantom session, which was true when written and is now superseded. It now carries the established mechanism -- the cache is shared on this SoC so core 0 disables it for the whole chip, the kernel is XIP and fetches through it, and the fatal access was core 0 reading Linux's PSRAM source buffer inside the window -- with the measured evidence, plus two separate things that are not fixed. The text that chunking the writes was what stopped the reboot is replaced by the statement that chunking did not change it and why. The `ESP_RST_INT_WDT` reading is kept because it is a real reading of a real reset, and the heading that presented it as the explanation is removed; the paragraph now says plainly that whether the watchdog was the cause or a consequence is not established, and that an 800 ms watchdog cannot have expired because of a 0.51 ms window, so the two facts still need reconciling by something other than window duration. A third stale statement surfaced during this cycle: the doc's claim that the phantom session was what rebooted the board, which entry 54 replaced. It is now stated as having been the belief at the time, with the reboots stopping when it was fixed offered as the reason for that belief rather than as proof, and marked as at most a contributor; the paragraph that introduced the flash mechanism no longer calls it a second cause either, because what it actually is is a real bug that happened to be fixed alongside the real one. **The autostart question needed no fix.** `docs/tamagotchi.md` already states the decision and its reason -- autostarting would hand the emulator `/dev/fb0` and the emulated machine from power-on whether or not anyone wanted the pet running -- and `scripts/install-tamacore.sh` enforces it mechanically rather than by prose, checking for `/etc/init.d/S51tamacore` and reporting `autostart: none - runs only when typed`. What was genuinely missing was a log entry, which this one supplies; the decision itself is sound, recorded in the place a reader would look, and verified by tooling. Since `linux_flash.c` changed, the firmware was rebuilt and reflashed rather than left describing source it was not built from: the layout did not move, `check-vectors.sh` still reports the kernel's vectors spanning 0x40382000..0x40382916 inside the firmware's buffer, `space_for_vectors` stays at 0x40382000 and `flash_stage` at 0x3fca3eb0, and the board boots to a login prompt on kernel #16. The reboot fix was then re-verified on the rebuilt binary rather than assumed to carry over: a 1 KiB write to `/home` at 12 s into emulator play landed with the emulator confirmed running and uptime continuous from 143.66 to 160.63. `patches/harness/core0/0003-*.patch` was regenerated from the tree's own diff and both halves verified again -- the firmware hunk applies to a clean c5423bd and produces files byte-identical to the live tree, and the esp-idf hunk is byte-identical to the live submodule's diff. `scripts/sync-firmware.sh --check` reports the firmware tree matches this repo. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 55, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The stale-rationale sweep is complete for the two files named in entry 55 and for the third statement it turned up, but it was a targeted fix rather than a systematic one, and the same mistake has now been found four times in this session -- in `shmproto.h`, in `esc_instr.c`, in `linux_flash.c` and twice in `docs/tamagotchi.md` -- because each of those files was written from the best model available at the time and the model changed twice underneath them. Anything written before entry 54 that explains WHY a change was made should therefore be read as suspect even where its WHAT is still correct, and `docs/cores.md`, which records the coherency result and the emulator's move to core 0, is the obvious next place to check. The `ESP_RST_INT_WDT` question is now stated as open in two places and should stay that way until measured: the instrument committed at 2436f59 records the longest cache-off window and its caller and survives a reset, so a future freeze can be read rather than argued about, and if a reset reports reason 5 again the PREV record will say whether a flash window was involved at all. Nothing else is known to be outstanding from the cleanup list: the patch is verified reproducible, JFS-001 and its four dependants are revised, all 86 records carry an index line, and the log gap is closed by entry 55.

#### Files Modified:

- patches/harness/core0/0003-flash-chunking-and-cache-instrument.patch
- docs/tamagotchi.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 55 COMMIT Unreleased 2026-11-11T09:50:00-07:00

#### Coming From:

Unreleased d8b7ec0

#### Purpose:

Close the 23-commit gap between core-log entry 50 and entry 51, so a session following the recovery policy no longer finds the log describing a tree that stopped existing a day earlier.

#### Outcome:

This entry is a retrospective, not a cycle record, and it is written to be read as one: the 23 commits between de41dbc and 35010bf were landed without log entries, so what follows is reconstructed from the commits themselves, from `docs/cores.md`, and from the tracked handoff at `.codewhale/handoff.md`. Git holds the authoritative detail and these commit hashes are the way in, not a substitute for it. Nothing here is a measurement taken by this cycle, and no figure is stated that is not already written in one of those artefacts. **What those commits did.** They moved the Tamagotchi emulator from core 1 to core 0, which is the design EMU-004 had recorded and deferred. `399669b` gave Linux 7 MB of PSRAM and turned the top megabyte into a window both cores can reach, `2f0ec53` established that core 0's writes into that window arrive on core 1 and that core 1 needs no cache maintenance of its own, and `46e459e` then put the guest on core 0 with core 1 reduced to blitting. `docs/cores.md` records the measured result of that stage as 4,038,199 emulated cycles per second against a 4,000,000 target, 159 frames in 10 seconds, a worst frame 11 ms late and 17 ms per strip write. Four commits worked through a display problem: `c6db1b9` found the guest alive on core 0 with the fault being a single `+=` in the pacing loop, `7ef4cb5` made the handshake session-numbered so a second run starts rather than deadlocking, `4e27475` fixed a smear and three bugs it had been hiding, and `c258529` and `1edde10` changed the dot pitch, ending at 5 px dots with a 1 px gap and fbcon yielding the framebuffer. `b0d8950` and the handoff record the glass verification as buttons responding, menus good, and 1416 frames published and 1416 blitted over 90 seconds at about 15.6 fps with 8 ms worst jitter. `8e78e2d` made `tamacore` an ordinary command rather than a boot service, deliberately with no `/etc/init.d` entry. Then the reboots, which is where the gap's own account diverges from what is now known: `3e23b40` bounded cache operations to 16 rows, `7c51d0a` added a core-1 diagnostic record, `e1358fa` chunked flash operations on the belief that a long uninterruptible window was resetting the board, `32304ee` preserved the previous boot's diagnostic record across a reset, and `35010bf` wrote the handoff naming flash operations as the partly-fixed cause. Entry 54 later established that the real fault was core 0 reading a PSRAM write source while the shared cache was off, which is why chunking bounded the window and changed nothing. That is recorded here as a correction of the gap's own conclusion, not as a criticism of it -- it was the best available reading at the time and it is what pointed at flash at all. One thing the gap did leave correct and useful: `32304ee`'s preserved-record idea is the same idea entry 52 needed for the flash instrument, and it was already in the tree. **Why this entry exists in this shape.** The alternative was to write 23 entries, one per commit, in the canonical six-section form. That would have manufactured Outcome prose, Status lines and Files Modified lists for cycles nobody logged, and the status values in particular would be invented: no one can now say whether the user tested `c258529`. The reference library carries a rule against recording what has not been looked up, and the same reasoning applies here. The log is a ring buffer of cycles; the honest way to restore it is one entry that says where the detail lives and what it concluded, which is what this is. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 54, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The gap is closed as a pointer and not as a history, so the remaining question is whether anything in those 23 commits is still load-bearing and unrecorded. Two candidates are worth checking against the tree rather than against the log. `8e78e2d` made `tamacore` an ordinary command with no autostart, which means the emulator does not start itself and a user has to type it -- that is a deliberate decision that no log entry states, and it should be confirmed as still intended before anyone assumes the watch boots into the pet. And `e1358fa`'s chunking was justified by a diagnosis entry 54 disproved, though the code is still correct and still needed for bounding the window; the comment in `linux_flash.c` that explains chunking as the fix for the reboots should be re-read, because it now states the wrong reason for a right change and that is exactly the kind of stale rationale that misled this session about the instrument. Beyond those, the four cleanup items requested with this entry are now done: the regenerated patch was verified to apply cleanly to a clean `c5423bd` and to produce files byte-identical to the verified deployment, with the esp-idf half also confirmed to apply to a clean `cache_utils.c`; JFS-001 was revised because its claim that firmware must never write flash is contradicted by the project's own working code, and KRN-005's citation of it was narrowed to match; and the twenty records that lacked a fast lookup index line now have one, taking the file to 86 records with 86 index keys, no record without an index line, no index line without a record, every routing target resolving and all YAML parsing. Nothing is known to be left over from this list.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 54 COMMIT Unreleased 2026-11-11T09:35:00-07:00

#### Coming From:

Unreleased bae1b42

#### Purpose:

Test the lead left by entry 53, that the fatal access during a flash write is core 0 reading the PSRAM write source rather than core 1 failing to fetch instructions, by staging each chunk through internal DRAM.

#### Outcome:

The lead was right, and the reboot is fixed. The write source is Linux's buffer and therefore lives in PSRAM, and `linux_flash.c` called `esp_flash_write()` with the external-memory cache disabled, so core 0 read PSRAM inside a window where that read cannot work. Each 4096-byte chunk is now copied into a static internal-DRAM buffer with the cache still on, and `esp_flash_write()` reads the copy; the buffer was verified in the ELF at 0x3fca3eb0, so it is internal DRAM rather than PSRAM. **The result is unambiguous because the test was the same one that had never once passed.** Before this change a single 1 KiB write to `/home` with the emulator playing rebooted the board every time it was attempted, at `/home` 74 percent full and again at 47 percent full. After it, four consecutive 1 KiB writes at 12 s into emulator play all survived, five cache-disable windows each, with uptime continuous across the whole run; a 128 KiB write plus forty file create/delete cycles, which is erase-heavy jffs2 garbage-collection pressure, survived with 891 windows and uptime 369.34 to 410.97 unbroken; and nine rounds of 64 KiB write-and-delete at ten-second intervals over 100 s survived with 1860 windows and uptime 424.93 to 527.18 unbroken. The emulator was confirmed RUNNING in every run rather than assumed. So the fatal read was on core 0 all along, and the answer to entry 52's open question is that core 1 was never the one dying. That also explains two things that had been recorded as mysteries: chunking the writes to 4 KiB bounded the window but changed nothing, because the problem was never the window's length, and the longest window measured 0.51 ms against an 800 ms watchdog for the same reason. The interrupt-level theory was already excluded in entry 53 and stays excluded; the mechanism is a single illegal read, not a race. **Made reproducible rather than left on the watch.** `patches/harness/core0/0003-*.patch` was regenerated from the firmware tree's own `git diff` instead of being hand-edited, and both halves were verified by reverse-applying them with `git apply --check --reverse`, so the committed patch is now faithful to what is actually deployed; it previously described the chunking and the old instrument and had nothing about staging. `patches/harness/core0/esc_instr.c` was added to `scripts/sync-firmware.sh`'s copy list, which now reports the firmware tree matches this repo. `CONFIG_VECTORS_ADDR` moved to 0x40382000 in the patch-carried config, because the instrumented firmware grows IRAM and moves `space_for_vectors` a page up from the historical 0x40380000, and `scripts/check-vectors.sh` reports the kernel's vectors spanning 0x40382000..0x40382916 inside the firmware's 0x40382000..0x40383000 buffer -- the guard that entry 51 built is what makes this move checkable rather than remembered. A full 16 MiB backup of the working state was taken to `~/twatch-backup/working-2026-11-11-0925.bin`, sha256 `11081f3a`, and the board was verified booting to a login prompt after the read. One honest limit on the evidence: the first attempt at the decisive test returned no output because a background `&` broke the console framing of `watch-exec.py`, so the test was rebuilt as a script on the watch that writes its results to a file and can be re-run; the numbers above come from that version, and the earlier no-output run was not counted as a pass. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 53, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The reboot is fixed but only in the firmware tree, which is never pushed, so the fix currently lives on one watch and in one patch file; the first thing to do is confirm the regenerated patch applies cleanly to a fresh checkout of `c5423bd` and reproduces a booting, non-rebooting pair, because that is the only claim a future session can act on. Beyond that the reference still contains two records that this session disproved and did not revise: EMU-003 and EMU-004 were corrected for the internal-SRAM claim in entry 53, but KRN-005's statement that a flash write from core 0 aborts because the IPC never completes predates the finding that the abort comes from an illegal PSRAM read while the cache is off, and JFS-001 states the same aborts-forever conclusion; both should be re-read against this entry before anyone plans around them. The instrument now earns its keep: it survived several resets during this cycle and its PREV record read 250 calls with a 2534373-cycle maximum window, and since a write during play is safe it can be left enabled and read after any future freeze to say whether a cache-disable window was involved. Twenty records still lack a fast lookup index line -- TCH-005 through TCH-010, KRN-007 and KRN-008, HAP-001, RTC-004, PMU-004, LCD-012 and LCD-013, and EMU-011 through EMU-017 except EMU-012 -- which is mechanical work, not analysis, and has now been deferred twice. The 23 log entries between entry 50 and entry 51 remain unwritten.

#### Files Modified:

- patches/harness/core0/0003-flash-chunking-and-cache-instrument.patch
- scripts/sync-firmware.sh
- patches/kernel/config/devkit_c1_16m_linux.config

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 53 COMMIT Unreleased 2026-11-11T09:40:00-07:00

#### Coming From:

Unreleased 2436f59

#### Purpose:

Correct three reference records that wrongly claimed Linux has no reachable internal SRAM, and test the interrupt-level theory for why a flash write during emulator play still reboots the board.

#### Outcome:

The records were wrong and are corrected, and the interrupt-level theory is disproved. **The correction.** LCD-012, EMU-003 and EMU-004 all stated that this port has no internal SRAM the kernel can reach, and that record was used to conclude both that DMA was unimplementable and that the emulator had no escape from PSRAM latency. That is false, and the proof is already in the build: the kernel's System.map carries 42 symbols in 0x4038xxxx, including `_WindowVectors_text_start`, `_WindowOverflow4/8/12`, `_WindowUnderflow4/8/12` and `_exception_text`, and `CONFIG_VECTORS_ADDR` is 0x40380000 -- which is internal IRAM. Linux executes there every boot. The error was conflating MMU-mapped with reachable: external memory is reached through the MMU as 64 KiB pages in 0x3C000000..0x3DFFFFFF, while internal memory is directly addressed and needs no mapping at all, IRAM being 0x40370000..0x403E0000 and DRAM 0x3FC88000..0x3FD00000 per the SoC headers. What is actually true, and narrower, is that Linux owns none of internal DRAM: the System.map has no symbol in 0x3FCAxxxx, the device tree's memory node covers PSRAM only, and the firmware owns that region -- so the obstacle to putting kernel code or DMA descriptors in internal RAM is that nothing has allocated it, not that it cannot be addressed. The fix pattern already exists in `space_for_vectors`, where the firmware reserves 4 KiB of internal IRAM and the kernel is told the address. A new record, SOC-004, states all of this positively with its sources, and the three records were revised in place with revision notes naming this entry, as the reference's own rules require; SOC-004 is registered in the topic catalog, the fast lookup index and the routing table, and `last_reviewed` moved to 2026-11-11. Validation is structural: the file parses as six YAML blocks, 86 records with 86 unique ids, every record carries the ten required fields, no index line lacks a record, every routing target resolves, and all fences balance; SOC-004 itself has an index line, and the twenty records that still lack one are listed in Next Steps as a pre-existing defect rather than counted as fixed here. **The check.** The theory was that `local_irq_save()` on this core only masks interrupt levels at or below three -- it compiles to `rsil 3`, and `XCHAL_EXCM_LEVEL` is 3 with `XCHAL_NUM_INTLEVELS` 6, leaving levels 4, 5 and 6 unmasked, five interrupts living at level 4 or 5 including INT16 at 5 and INT24/25/28/30 at 4. If any of those were in use, its handler would run from flash inside the cache-off window and kill core 1 regardless of how IRAM-resident the wait loop is. The theory is dead: every interrupt the board actually raises is at level 1. `/proc/interrupts` shows the active set as IRQ 3 (acm), 4 (ipc), 5 (spi), 6 (xtensa timer), and the gpio children 7, 8 and 33, and resolving each through `xtensa_map_ext_irq` against `XCHAL_INTTYPE_MASK_EXTERN_EDGE|LEVEL` gives INT3, INT2, INT9, INT6 and INT10 -- all level 1, all masked by `rsil 3`. So no interrupt can fire in the window and the level hypothesis is excluded. One further fact fell out and it is load-bearing: `SOC_IDCACHE_PER_CORE` is defined for esp32 but not for esp32s3, so the cache is SHARED, and `spi_flash_disable_cache(cpuid)` disables it for the whole chip; the `other_cpuid` branch in `cache_utils.c` is compiled out, which is why core 1 dies rather than merely stalling. A candidate that this makes worth testing is stated as a lead and not a conclusion: the write source buffer is Linux's own, so it lives in PSRAM, and `esp_flash_write()` reads it as `cmd->data + done` in `linux_flash.c` while core 0 has the cache disabled -- if that read is what faults, the death is on core 0 rather than core 1. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 52, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The next experiment is to test whether the death is on core 0 rather than core 1, because that reverses where to look and the interrupt result leaves it as the strongest remaining candidate. The cheapest discriminating test is to make the firmware's IPC write path copy the incoming buffer into internal RAM before calling `esp_flash_write()`, so nothing on core 0 reads PSRAM while the cache is off; if the board then survives a write during play, the fault was core 0 reading the PSRAM source and the durable fix is that copy plus the existing chunking, which is a small change to `linux_flash.c` rather than a memory-layout rearrangement. If it still dies, core 0 is exonerated and the next place to look is the interaction between the flash window and the emulator's own PSRAM traffic, since 290 s of emulator with no writes is clean and a single 1 KiB write during play is fatal every time. Either way the instrument committed at 2436f59 is what makes the experiment readable: it records count, caller and the longest window in cycles, and it survives the reset, so a failed attempt now yields evidence instead of nothing. The reference still has twenty records without a fast lookup index line -- TCH-005 through TCH-010, KRN-007 and KRN-008, HAP-001, RTC-004, PMU-004, LCD-012 and LCD-013, and EMU-011 through EMU-017 except EMU-012 -- which is a pre-existing defect noted in the first audit of this session and left unfixed because it is mechanical work rather than analysis; SOC-004 is not among them. The 23 log entries between entry 50 and entry 51 are still unwritten.

#### Files Modified:

- .ai/core-reference.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 52 COMMIT Unreleased 2026-11-11T09:05:00-07:00

#### Coming From:

Unreleased e2930ae

#### Purpose:

Find why the board reboots during use, by giving the flash-path instrument a record that survives a reset and cannot itself stop the board booting, and then characterise the failure it reports.

#### Outcome:

The instrument was fixed, the failure was characterised, and a proposed cure was disproved on hardware. Three things are established and one is now known to be impossible. **The instrument's first version was itself the reason the board would not boot at commit c5423bd.** Bisection on hardware isolates it exactly: c5423bd with the instrument hangs, c5423bd without it boots, and 85d557b boots, so the app-side work in that commit is sound and the esp-idf half is not. The cause is that the instrument read a guard word at 0x3df70118 and stored at 0x3df70110 from inside `spi_flash_disable_interrupts_caches_and_other_cpu()`, which always runs with the external-memory cache disabled; PSRAM is reached THROUGH that cache, so the access cannot work. Its offsets were also wrong, 0x70110 being 16 bytes inside `SHM_DIAG_PREV_OFF` and therefore corrupting the crash snapshot it existed to help read. It also produced no diagnostic at all, because the firmware console is `CONFIG_ESP_CONSOLE_UART_DEFAULT` (UART0) and nothing reaches the USB port, so a boot that died before Linux printed nothing anywhere. The fixed instrument puts four counters in internal DRAM -- verified in the ELF at 0x3fca588c onward, never in PSRAM -- arms them at the end of `map_flash_and_go()` after `init_linux_mmap()`, and publishes them into `SHM_DIAG_FLASH_OFF` from a separate core-0 task, `esc_instr.c`, that does nothing until armed. The previous boot's record is copied into `SHM_DIAG_FLASH_PREV_OFF` at the one point in the firmware that can safely read the window, which is how a reading now survives the reset that makes it interesting; without that copy the counter restarts at zero every boot and any value read afterwards describes only the current boot. **What it then measured.** 25 cache-disable calls at boot with Linux idle, and zero during a 75 second emulator run and again during a 290 second run -- so the emulator alone does not disable the cache, and 290 seconds of play is stable with no writes. A single 1 KiB write to `/home` with the emulator running rebooted the board every time it was attempted, twice at `/home` 74 percent full and again at 47 percent full, which disproves the hypothesis that jffs2 garbage-collection pressure was the trigger and retires the plan to relieve it by deleting superseded emulators. The caller resolves through `xtensa-esp32s3-elf-addr2line` to `cache_disable()` in `spi_flash_os_func_app.c`, the ordinary `esp_flash_*` path reached from `linux_flash.c`'s IPC handler, not from the emulator. The longest single cache-off window measured 0.51 ms against an 800 ms interrupt watchdog, so the reboot was never a watchdog timeout and chunking the firmware writes to 4 KiB could not have helped. A 155 KB deploy accounted for 6065 of those calls, which is why an early uncorrected reading of 6070 looked like emulator traffic. **The disproved cure.** The mechanism was at first stated as core 1 executing from PSRAM without its own cache; that was wrong and the error matters. `CONFIG_XIP_KERNEL=y` with `_stext` at 0x42140000, so the kernel EXECUTES FROM FLASH, and kernel instructions are fetched through the same external-memory cache that core 0 disables. Core 1 therefore cannot fetch one instruction while the window is open, which is why a window of half a millisecond is fatal and why the watchdog threshold is irrelevant. The fix attempted was to make the entire IPC handshake interrupt-free and IRAM-resident by rewriting `map_esp32.c`'s `esp32_ipc_flash_io`, keeping only the second half in `.xiptext` as the port's patch series does. It compiled, `esp32_ipc_flash_wait` was confirmed in `.xiptext` at 0x4038091c, and the board still rebooted on the first 1 KiB write during play. The reason is that IRAM code is not sufficient: the wait loop's shared command block is a `kzalloc` from the PSRAM node, and the windowed ABI spills registers to a stack that is also PSRAM, so a loop that touches no memory is not writable. `/proc/iomem` settles it -- Linux has the flash window and PSRAM and no internal SRAM at all, so there is no address it can execute or read from while the cache is off. The driver was restored from the port's own patch series, 0017 plus 0053, after a hand revert corrupted it into duplicate definitions, and the kernel rebuilt and flashed so the board runs the port's original driver on kernel #15 with its matching firmware. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected and contains only this entry, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 51, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The durable fix needs internal SRAM mapped into Linux, and there is no way around that. Because the kernel executes from the flash window through the cache core 0 disables, and because `/proc/iomem` shows Linux owns no internal SRAM, any code or data the kernel must touch during a flash window has to live somewhere the cache does not mediate. That is the same infrastructure `space_for_vectors` already uses: reserve a few KiB of internal RAM in the firmware, map it in the MMU in `linux_ll_esp32s3.c`, describe it in the device tree, and put both the IRAM wait loop and the shared command block in it so the handshake touches nothing that the cache mediates. That work changes the firmware/kernel address contract that has already produced two silent failures on this project, so it should be done with a fresh full backup, on a branch, and with `scripts/check-vectors.sh` passing before any flash. Until then the honest interim measure is a policy rather than a fix: no flash I/O while the emulator runs. The save is already written only on exit, but `/etc`, `/home`, the dropbear keys and jffs2 garbage collection can all write at any time, so if the watch is to be usable that invariant needs enforcing rather than assuming. One measurement remains worth taking and is cheap: whether a write survives when the emulator is stopped but other userspace is busy, which separates "flash write is fatal whenever Linux is executing" from "flash write is fatal only under emulator load". The 23 missing log entries before entry 51 are still unwritten, and the reference library still records EMU-017 as an unverified overlap hypothesis when the coherency question has since been answered, so a fresh agent following the recovery policy reads a stale account of both.

#### Files Modified:

- patches/harness/core0/shmproto.h
- patches/harness/core0/esc_instr.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 51 COMMIT Unreleased 2026-11-11T07:45:00-07:00

#### Coming From:

Unreleased 35010bf

#### Purpose:

Replace three broken copies of the vectors-address check with one enforced check that runs on the build and flash paths, so a kernel and firmware that disagree can no longer be flashed as a pair.

#### Outcome:

The invariant now has a mechanism. `scripts/check-vectors.sh` compares `space_for_vectors` in the firmware ELF against `CONFIG_VECTORS_ADDR`, and compares the kernel's own vector span from `System.map` against the firmware's 4096-byte buffer, so equal addresses without room is also a failure. It replaced three regimes that each failed differently: `scripts/build-kernel.sh` grepped for the hardcoded literal `^CONFIG_VECTORS_ADDR=0x40380000`, which passes whatever the firmware does; `scripts/build.sh` compared the right two numbers but read upstream's seed config rather than the one the kernel is built from, and only builds, never flashes; and `scripts/flash.sh --app` had no check at all while writing core-0 firmware alone, which is the exact operation that strands the kernel already on the board. `scripts/flash.sh` now refuses through `check_pair`, `scripts/build-kernel.sh` delegates instead of matching a literal, `scripts/build.sh` checks the config the kernel actually consumes and deliberately omits `System.map` because a stale map would report a mismatch that the pending kernel build is about to fix, and `patches/0001-watch-quad-psram.patch` no longer instructs a future agent to confirm the literal 40380000. The underlying fact this makes checkable rather than rememberable: `space_for_vectors` is a 4096-byte, 8192-aligned IRAM buffer, so it sits on the next 8 KiB boundary after whatever IRAM precedes it and moves a page whenever firmware IRAM grows past one. That patch's own history records the trap, having raised the alignment from 4 KiB to 8 KiB to bring the symbol from 0x4037f000 up to 0x40380000, which only worked while IRAM ended below that boundary. The core-0 flash instrument added at commit 35010bf pushed IRAM past it and the symbol is now 0x40382000, and no alignment can move it back down because the preceding IRAM content stays where it is. Flashing the matched pair at 0x40382000 did not work, and the failure is reported rather than explained: the kernel was rebuilt to that address, `check-vectors.sh -v` confirmed its vectors spanned 0x40382000..0x40382916 inside the buffer 0x40382000..0x40383000, and both images were written with `Hash of data verified`, but the board then reached `entry 0x403c9880` and never started Linux, with no reset loop and the USB device staying enumerated. That is a second, independent fault in the same firmware commit, because that commit both added the instrument and grew IRAM and was never verified to boot on its own. The leading and unconfirmed explanation is that the instrument reads `ESC_DIAG_FLASH_LIVE` at `0x3df70118` on every cache-disabling flash operation, including boot-time ones before PSRAM is ready, and this board's PSRAM is not zeroed at power-on. The known-good pair was then restored from `working-2026-11-11-0005.bin`, `Hash of data verified` over all 16777216 bytes, and the board verified booting to `buildroot login:` with `co5300` painting at 3.78 s and ping answering. The kernel was also rebuilt at 0x40380000 through the new check and flashed alone, and that booted, which is what establishes the guard itself rather than the address as the working part. `CONFIG_VECTORS_ADDR` is therefore left at 0x40380000, the last pair verified to boot, with a comment recording that the firmware tree now produces 0x40382000 and that the guard will refuse it until it is proven, so the tree currently fails `scripts/build-kernel.sh` on purpose. One pre-existing defect is recorded here rather than repaired: entry 50 was written at commit de41dbc and 23 commits have landed since without entries, covering the whole core-0 emulator and shared-window effort, so the log does not describe the tree it ships with. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected and contains only this entry, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 50, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The firmware that does not boot is the next problem, and it should be approached as a firmware question rather than an address one, because the address is now verified correct and the board still refused. The cheapest discriminating test is to rebuild the firmware tree with the instrument neutralised so it writes nothing and reads nothing, flash it with the restored kernel, and see whether the board boots: if it does, the instrument's reads are the fault and it needs to be gated on a live flag that can only be set after the handoff rather than on a PSRAM word at power-on, and if it does not, the fault is something else in that commit and the instrument is exonerated. Only after a firmware boots in isolation should its `space_for_vectors` be read and `CONFIG_VECTORS_ADDR` moved to match, at which point the guard will permit the pair instead of refusing it. The flash-write suspicion from the previous session is not addressed by this cycle and remains open: a flash write from core 0 disables the external-memory cache for the whole chip, core 1 executes from PSRAM with no cache of its own, and the instrument that was built to count those events is in the firmware that will not boot, so the freezes remain unexplained until a firmware carrying a working instrument is on the board. The 23 missing log entries should be written before the next cycle, because a fresh agent following the recovery policy currently reads a log that stops before the core-0 work began and a handoff that describes it, and only the handoff is accurate.

#### Files Modified:

- scripts/check-vectors.sh
- scripts/build-kernel.sh
- scripts/build.sh
- scripts/flash.sh
- patches/kernel/config/devkit_c1_16m_linux.config
- patches/0001-watch-quad-psram.patch

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 50 COMMIT Unreleased 2026-11-10T18:53:16-07:00

#### Coming From:

Unreleased cb9e984

#### Purpose:

Test whether core 1 can read a PSRAM buffer that core 0 writes, which is the gate on the whole core-0 emulator architecture.

#### Outcome:

**The probe put the board in a boot loop, and the loop is the result worth having.** The firmware gained a task writing an incrementing counter into a 256-byte PSRAM buffer every 100 ms with `esp_cache_msync(..., ESP_CACHE_MSYNC_FLAG_INVALIDATE)`, and the kernel gained `CONFIG_DEVMEM` so Linux could read the same physical address. Flashed together, the console showed a clean Linux boot reaching `Chipset=ESP32-S3 ID=09 detected over shmem IPC` at 4.198 s and then `rst:0xc (RTC_SW_CPU_RST)`, repeatedly. Reverting the firmware change restored boot immediately, which isolates the cause to the probe and clears the kernel. The leading hypothesis, and it reframes the design rather than merely reporting a bug: **the two cores' PSRAM use may overlap.** `init_linux_mmap()` identity-maps all 8 MB of PSRAM into Linux's window (`CONFIG_LINUX_PSRAM_8M=y`, MMU entries 0x180..0x1ff covering `0x3d800000..0x3e000000`), while the firmware's own `heap_caps` PSRAM heap comes from the same silicon. If they overlap, a firmware task writing its heap corrupts memory Linux is running from, which produces exactly the observed signature -- a working boot until the writes begin, then a software reset. That would mean a shared display buffer cannot be an ordinary firmware heap allocation: it has to be a `reserved-memory` region carved out of Linux's map and handed to the firmware explicitly, and the firmware must not touch PSRAM heap once Linux is up. The measurement that settles it is cheap -- have the firmware print its PSRAM heap base and compare it against Linux's window before writing anything -- and it is the first thing the next attempt should do. Two things the cycle did establish. The firmware toolchain builds and flashes from this machine: the ESP gcc is at `build/idf-tools/tools/xtensa-esp32s3-elf/esp-12.2.0_20230208` rather than in `~/.espressif`, and an incremental `ninja` in the existing build directory works in about a minute. And `esp_cache_msync()` exists in this IDF with both writeback and invalidate flags, so the coherency primitive is available. Everything was reverted to the safe state before the cycle closed: the firmware change is out, `CONFIG_DEVMEM` is off again with the recorded reason still standing, the kernel rebuilt and reflashed so source and flash agree, and the watch was verified booting with PMU 100 percent, `rtc0` working, touch on IRQ 33 and `drv2605` bound. The probe source is kept at `patches/harness/core0/shmprobe.c` so the retry starts from it rather than from scratch. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 85 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 49, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The first step of the next attempt is not the probe but a print: have the firmware log the base and size of its PSRAM heap, and compare that number against Linux's `0x3d800000..0x3e000000` window. If they overlap, the architecture needs a `reserved-memory` node in the device tree (with `no-map` so Linux does not touch it) plus a fixed physical address the firmware can be compiled to use, and the ordinary PSRAM heap must be considered off-limits once Linux is up. If they do not overlap, then the boot loop has another cause -- a task or stack problem in the probe, an `esp_cache_msync` misuse, or a watchdog -- and that gets diagnosed before any further attempt. Either way the coherency question itself remains unanswered and the core-0 design remains unvalidated. Worth stating plainly: this is the first cycle in the session that made the hardware worse rather than better, it was recovered without a full restore, and the reason it was recoverable is that `working-2026-11-10-1756.bin` existed and the firmware app can be reflashed alone without touching `/etc` or `/home`. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/harness/core0/shmprobe.c
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 49 COMMIT Unreleased 2026-11-10T18:30:39-07:00

#### Coming From:

Unreleased cc5a7de

#### Purpose:

Price the remaining suspects in the interpreter before rewriting it, so the rewrite is aimed at something real.

#### Outcome:

The rewrite was not done, and the measurements are the reason: the interpreter is already at normal efficiency for its design, so micro-optimisation cannot deliver what the status menu needs. Three suspects were priced and all three are small. Removing the per-instruction coverage counter gave +0.8 percent. Compiling at `-O3` instead of `-O2` gave -0.5 percent, and 10 KB more code. Skipping the `timers_clock` call entirely -- a throwaway build that breaks the guest and is therefore a ceiling rather than a shipping option -- gave +7.7 percent, 517948 to 557764 instructions per second. What is left is `step_core` and `exec_op_sem`, and the arithmetic says there is not much there to take: at 463 native cycles per emulated instruction and EMU-007's measured 7.3 cycles per host operation, the interpreter executes roughly **63 host operations per emulated instruction**, which is the normal range for a switch-based 6502 interpreter rather than a bloated one. This is the third correction to the optimisation plan in one session and it inverts the recommendation. EMU-005's "3-8x available" was derived from 1181 cycles per emulated instruction -- the corebench figure, not this workload -- and at 464 cycles and ~63 host operations there is no 2x sitting in the interpreter. A register cache removing most host memory traffic might reach ~45 host operations, about **1.4x**, which would take the menus from 0.52x to roughly 0.7x rather than 1.00x. The reason micro-optimisation is capped is architectural and was already recorded in EMU-004 from the other direction: every host instruction costs about 7.3 cycles because core 1 has no I-cache and executes from PSRAM, and no change to the interpreter alters what a host instruction costs. The 4-7x is on core 0, which has cache and reachable internal SRAM, and that means running the emulation there and leaving core 1 to blit - a different kind of change from anything attempted here, still deferred by user decision, with its three recorded constraints: core 0 owns the WiFi link the console depends on and starving it trips the FreeRTOS watchdog, a flash write from core 0 after Linux is up aborts, and the firmware tree must never be pushed. Nothing was left in the tree or on the watch from the experiments: the probe build was removed from both, and `tamafb-1x` remains the deployed emulator. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 84 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 48, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

There are two honest paths and they should be chosen deliberately rather than drifted into. The first is to accept the emulator as it is: it plays at 1.00x, the panel costs 2 percent of frame time, the menus run at 0.52x, and every cheap lever has been measured and excluded. That is a complete and working state. The second is EMU-004's core-0 design, which is the only thing with the right magnitude - 4-7x from cache and internal SRAM - and which needs a display buffer shared between cores and a thin blitter left on core 1. It carries the three constraints recorded in EMU-004 and it is deferred by user decision, so it needs an explicit decision to start rather than an inference. A register cache is not worth a cycle on its own: 1.4x does not reach the 2x the menus need and would be paid for with a rewrite of a 970-case switch. If a host-instruction counter is ever wanted to close the last gap in the model, it belongs with the core-0 work rather than before it. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 48 COMMIT Unreleased 2026-11-10T18:23:33-07:00

#### Coming From:

Unreleased 19efd13

#### Purpose:

Establish where the emulator's time actually goes before rewriting the interpreter, and put the interpreter under version control so that rewrite is reversible.

#### Outcome:

The safety net is in and the cost model is corrected, and no rewrite was attempted -- deliberately, because the model had to be right first. `patches/harness/splb32/` now carries the interpreter and `scripts/build-tamafb.sh` builds from there instead of from `~/GIT/TamaHarness`, which has no `.git` at all; the vendored build reproduces the deployed binary's hash family exactly. That was the precondition for touching a 1337-line file whose failure modes are silent miscounted cycles rather than crashes. The only source change was putting the `opcode_hits` coverage counter behind `-DSPLB32_COVERAGE`: it was a PSRAM read-modify-write on every emulated instruction for a counter whose accessor is called from nowhere. It measured 516887 to 521199 instr/s, +0.8%, and the comment in the source says plainly that this is not a lever -- it was kept because it is free. Then the measurement work, which produced a correction worth more than the optimisation would have. Disassembling `splb32_run_cycles` shows **258 host instructions containing 86 loads and 30 stores — 116 host memory operations per emulated instruction**, and at 516887 instructions/s the real cost is 464 native cycles, so 116 operations at roughly 4 cycles each accounts for it exactly. That contradicts the model EMU-006 established, and the error was in what got counted: EMU-006's "memory is only 2.6 percent of the cost" is correct for GUEST bus accesses (2.408 per instruction) but never counted host accesses to the cpu struct. Memory traffic is the dominant cost after all, at about 4 cycles per transaction -- not the 12.4 of a dependent load, and not the 43 an older record claimed -- which means the bus pipelines well and **access count is the limit, not latency**. Two experiments that had appeared to argue against memory are explained by the same arithmetic and are now recorded as not being evidence: removing the coverage counter saved 3.8 cycles because 2 accesses of 116 is under 2 percent, and the benchmark's own run-to-run spread is about 0.6 percent, so that result was noise-adjacent and the conclusion drawn from it at the time was wrong; and `-O3` changed nothing because a better register allocator cannot remove accesses the source makes on a struct it cannot hold in registers across a 970-way switch. One further measurement-hygiene error is recorded against this cycle: 1181 native cycles per instruction is the corebench/flash-ROM figure and 464 is the tamafb/PSRAM figure, and they are not comparable because corebench runs a synthetic loop while tamafb runs the real ROM which sleeps most steps. **EMU-011 already states that rule and this cycle violated it while quoting it.** The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 83 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 47, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The register cache is the next change and its target is now specific rather than estimated: remove the large majority of the 116 host memory operations per emulated instruction by keeping `a`, `x`, `y`, `sp`, `pc` and the flags in host locals across a run, writing them back only when the guest actually touches memory or a handler needs the full struct. Two things make that worth doing carefully rather than quickly. The failure mode is silent miscounted cycles, not a crash, so every step needs the headless bench against 516887 instr/s and the status-menu check on hardware; and the figure to beat is the 464 cycles per instruction, not EMU-006's 1181, which belongs to a different benchmark. If the rewrite reaches roughly half the accesses it should land near 230 cycles and about 1.03M instructions/s, which is the 2x EMU-014 says the menus need. If it does not reach that, the honest conclusion is that 0.52x in menus is close to this chip's floor and the answer is to accept it or to move the emulator to core 0, which stays deferred. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/harness/splb32/splb32.c
- patches/harness/splb32/splb32.h
- scripts/build-tamafb.sh
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 47 COMMIT Unreleased 2026-11-10T18:01:56-07:00

#### Coming From:

Unreleased 0ffbddc

#### Purpose:

Capture a full 16 MiB backup of the state in which the emulator renders at the guest LCD's true 49x49 resolution, taken from a cold boot.

#### Outcome:

`~/twatch-backup/working-2026-11-10-1756.bin` is the current recoverable state, 16 MiB read at 627.0 kbit/s in 214.1 seconds, sha256 `a7fcf01083d83637161ec12877c731f86f6f6b83aaa981afa7cc808edf3b833b`. It is the first image carrying the 1:1 emulator, so it is the first that restores a watch able to play the Tamagotchi without the panel competing for the frame budget. The watch was rebooted first and every subsystem re-verified before the read, so this is a cold-boot state: touch interrupt driven at `idle 5000 ms, active 20 ms, release 250 ms`, `drv2605` bound and reporting `DRV2605 ready, ERM motor`, PMU capacity 100 with `usb_online` 1, `rtc0` at 00:56:51, fb0 at 320x400, and `/home/tamafb-1x` present at 255176 bytes. The image was verified three ways. The stored `.sha256` matches an independent `sha256sum`; the kernel extracted from `0x140000` is byte-identical to the built `xipImage` (`eb1e42ac…`); and the `/home` partition carries the jffs2 magic `85 19` at `0xcc0000`, confirming the partition is present and formatted with content. That third check replaced a check that did not work, and the failure is worth recording: a raw scan of the image for the deployed binary's hash finds nothing, because jffs2 compresses its contents. Absence from such a scan is not evidence that a file is missing, and a future session should not read it that way. The backup's trailing hard reset rebooted the watch, which was confirmed healthy afterwards: `drv2605` re-probed twice in the log, `cst92xx` re-registered on IRQ 33, and `/home/tamafb-1x` was still there. At the operator's instruction the tree was left uncommitted pending the interpreter work; this entry covers the backup only. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 46, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The interpreter is the next and only remaining performance problem, and this image is the restore point for that work: 1756 restores a watch whose emulator plays at 1.00x and slows to 0.52x in the status menu, so any interpreter change can be judged against a known baseline in four minutes. The target from EMU-014 is roughly a 2x improvement in instructions per second to make the menus comfortable and 4x to leave headroom, and EMU-005/006 locate the cost in the interpreter's own instruction stream at about 1181 native cycles per emulated instruction, 97 percent of it there rather than in memory access, with guest register fields reloaded per access across a 6796-instruction switch. The recommended first move -- keeping guest registers, flags and PC in host variables across a decoded block -- is userspace-only and carries no firmware risk. Nothing else about the display needs attention: the panel is 2 percent of frame time and should be left alone. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 46 COMMIT Unreleased 2026-11-10T17:54:48-07:00

#### Coming From:

Unreleased 3f6b19c

#### Purpose:

Render the guest LCD at its true resolution to take the panel out of the emulator's frame cost, after the operator suggested the pet is really only a few dozen pixels rather than the 292x300 the harness was painting.

#### Outcome:

The suggestion was right and the panel is finished. The harness renders the V3 dot-matrix artwork at a 5 px dot on a 6 px pitch, which produces a 292x300 LCD so the pet is readable at arm's length; the hardware's real information resolution is **49x49**, recovered by dividing that pitch out of the generated map. At 1:1 the per-frame cost fell from render 2.5 ms and panel 5.7 ms to render 0.6 ms and panel 0.5 ms, stalls in a 20 s idle run went from 29 to 0, and the panel's share of frame time went from 32 percent to about 2 percent. Bands sent per dirty frame fell from 6 of 10 to 1-2 of 10, because a 49x49 LCD occupies two bands of 40 rows. That last point is why the win is larger than the pixel ratio suggests: band masking only pays when the dirty area is small, so a smaller image and the masking reinforce each other. The trade is legibility -- the pet is small on a 320x400 glass -- and it is the same artwork at any integer multiple of the dot pitch (2x is 98x98, 3x is 147x147) if a larger size is wanted later.

One long-standing bug surfaced on the way, and the operator caught it on the glass. `tamafb.c` derives its framebuffer geometry from the segment size unless it is overridden: `FB_W = SEG_W + 32` and `FB_H = SEG_H + 48`, which is 324x348 for the 5 px map and 81x97 for the 1:1 map, neither of which is the watch's 320x400 fb0. The file's own comment says those defaults exist precisely to be overridden for a board like this one, and the watch build had never passed them. Every band was therefore written with a row stride 320 pixels narrower than the driver's 640-byte stride. At the 5 px scale the error was 4 px horizontal and 52 px vertical and read as a slightly off-centre picture that nobody questioned; at 49 px it was a full-width smear about 48 rows tall. Fixed by passing the real geometry, and the first attempt was wrong in a second way worth noting: `round(x0/6)` makes dots whose x0 is a multiple of 6 collapse to zero width, so every dot is now emitted as exactly one pixel wide at its pitch column.

The more important result is what the clean measurement showed once the noise was removed. A 22 s run on a freshly booted watch with nothing else running gave speed 1.00-1.01x during ordinary play and **0.52-0.79x sustained while the operator used the status menu**, with panel time flat at 8-37 ms/s throughout. The obstacle is not frame time but cycles per instruction: emu wall time per second rose from 137-150 ms to 800-945 ms while the panel share stayed tiny, and the run averaged 25 emulated cycles per instruction across 2913709 instructions. At 4M cycles/s the status menu therefore needs about 520k instructions/s against a measured headless ceiling of 513k, so it cannot keep up, while ordinary play needs only about 160k because the pet spends most steps asleep and a sleep step is cheap. That is why the slowdown feels disproportionate to what the guest is doing, and why the operator's instinct -- slowdowns whenever interacting with game menus, never when the screen is static -- was the correct diagnosis. The only lever left is interpreter cost per emulated instruction, which EMU-005 and EMU-006 put at about 1181 native cycles against the 30-80 a tuned core needs, 97 percent of it in the interpreter's own instruction stream. That is userspace work with no firmware risk.

A measurement trap is recorded with these numbers because it cost two cycles. `ns_emu` is wall time around `splb32_run_cycles()`, so anything else running on the watch -- including this session's own diagnostic commands -- is billed to `emu`. The same build logged 64 stalls while the watch was being driven from a shell and 2 stalls in a 22 s run left alone. The operator identified that before the log did, having noticed the game went laggy exactly while commands were being issued. Provenance was also fixed: `patches/harness/segmap-1x/` now carries the 1:1 map and `scripts/build-tamafb.sh` builds the binary from it, reproducing the deployed file byte for byte, because the emulator front end lives in `~/GIT/TamaHarness/portable` which is not a git repository and the binary would otherwise have no reproducible source. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 82 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 45, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The interpreter is now the only thing between this and full speed, and the target is quantified: the status menu needs about 520k instructions/s and the ceiling is 513k, so roughly a 2x improvement in instructions per second would make it comfortable at 1.00x and 4x would leave headroom. EMU-005's estimate of 3-8x being available from a better interpreter makes that plausible rather than certain. EMU-006 already says where to look: 1.622 interpreter steps per emulated instruction, 72.6 percent of bus accesses to ROM, and memory only 2.6 percent of the cost, so the work is in the interpreter's own instruction stream and specifically in the 6796-instruction switch that reloads guest register fields per access. The change EMU-005 recommends -- keeping guest registers, flags and PC in host variables across a decoded block instead of reloading them per access -- is the right first move and is userspace-only. A full 16 MiB backup should also be taken, since `working-2026-11-10-1703.bin` predates the chunk-loop change and this cycle, though neither affects the kernel: the deployed emulator is in `/home` and is a userspace file. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/harness/segmap-1x/segmap.c
- patches/harness/segmap-1x/segmap.h
- scripts/build-tamafb.sh
- .gitignore
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 45 COMMIT Unreleased 2026-11-10T17:30:23-07:00

#### Coming From:

Unreleased c1ff9d7

#### Purpose:

Cut the panel transfer cost to make full-frame refresh affordable, following the operator's report that the status screen runs at 0.3x with visible tearing.

#### Outcome:

The cost is now understood precisely, the fix is proven impossible by register width rather than by effort, and the optimisation attempted measured as a no-op that rules out the leading theory. Three things came out of it and only the first is an implementation. **DMA cannot be used from this port.** The GDMA out-link register stores only the low 20 bits of the first descriptor's address, so descriptors must live in internal DRAM at 0x3FC88000; Linux here has no internal SRAM mapping at all, its device tree memory being a single 8 MB block at 0x3D800000 covering PSRAM only. That is an addressing limit, not a contention one, so no driver work changes it. Two risks that had been raised around the same idea were checked and dissolved: the esp-hosted link on this board is shmem (`CONFIG_ESP_SHMEM_HOST_INTERFACE=y`), not SPI and not SDIO, so networking claims no SPI DMA path; and slave_bt.c's UHCI channels are never initialised because this build drives BT through the internal controller via NimBLE, which a GDMA occupancy dump at 2.1 s confirmed as all five channels zero. **The transfer is not wire bound.** Halving the SPI clock from 40 MHz to 20 MHz through the existing `spi_hz` parameter made a full frame 15 percent worse, not 100 percent: 93-95 ms becomes 108.8 ms. Wire time at 4 lanes and 40 MHz is about 3.2 us of the ~23 us each 64-byte transaction costs, so roughly 20 us per transaction is fixed engine turnaround outside the CPU's reach. **The MMIO reduction gained nothing, and that is the useful result.** Five of the roughly eight register writes per chunk were redundant on all but the last chunk -- a `SPI_USER` rewrite with an unchanged value, an unchanged `SPI_MS_DLEN`, and a `SPI_UPDATE` latch plus poll on each of 4000 chunks -- and removing them measured 93.3 ms against 94.8 ms, inside the noise. That rules out MMIO as the cost, which points the next attempt at fewer transactions or fewer bytes rather than faster ones, and it is recorded as a negative result on purpose because the change was kept. The operator confirmed the display was correct on the glass after it: console text legible, ring and bars intact, no tearing at rest. Two measurement traps cost real time and are now written down. `dd if=/dev/zero of=/dev/fb0` cannot measure this: a `dd` that touches nothing costs 430-440 ms on this board, so a 25 KB write and a 256 KB write both 'take' about 440 ms and the numbers describe process startup. Deriving a per-band figure from the emulator's stall log gives 9.3 ms from one frame and 25 ms from another, a 2.7x spread, which is why `userspace/fbbench.c` was written -- one process, CLOCK_MONOTONIC, no fork noise -- and committed with the emulator's other diagnostics. A self-matching bug in `watch-backup.sh` was also fixed: its in-flight guard used `pgrep -f 'esptool.*read_flash'`, which matches the calling shell's own command line when that line mentions esptool, so the script refused to run with the port perfectly free; the pattern is now `[e]sptool` and cannot match itself. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 80 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 44, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Fewer bytes, not faster bytes, is the only lever left on the panel, and there are two concrete offers. The first is to confine blits to the guest LCD area: the emulator's LCD is 292x300 (175 KB) inside a 320x400 framebuffer (256 KB), so painting only the LCD region would cut bytes and transactions by about 32 percent with no new hardware risk. The second is the `swab16` copy inside `co5300_blit`, which loops over every pixel with a multiply per index while holding the ioctl, and has never been timed separately from the transfer it feeds -- it is counted inside the 93 ms measured, and is plausibly 10 to 20 ms of it. Either is worth measuring before it is believed, using `fbbench`. The stalls themselves remain open and are not a throughput problem: EMU-002's status-screen frames decompose as emu 49 percent, panel 32 percent and render 15 percent against a 50 ms budget at 20 fps, which is why better paint alone moves them from 153 ms to perhaps 65 ms rather than eliminating them. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/display/final/spi-esp32.c
- scripts/watch-backup.sh
- scripts/build-userspace.sh
- userspace/fbbench.c
- .gitignore
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 44 COMMIT Unreleased 2026-11-10T16:46:42-07:00

#### Coming From:

Unreleased 17099e0

#### Purpose:

Restore the three touch bars after the touch daemon was retired, and settle whether the emulator is faster without it.

#### Outcome:

Both, and the second was the point. Retiring `tbtns` stranded three things it owned, discovered one at a time as each stopped working: the haptic call, then the `touch-buttons` input device, then the bar-to-button mapping itself. All three are now in the kernel. `cst92xx` decodes the strip below fb0 into three buttons and registers a second input device named exactly `touch-buttons` carrying BTN_0, BTN_1 and BTN_2 -- the same name and codes the retired daemon used, chosen so `tamafb` needs no change and no consumer written against the old daemon breaks. The band mirrors `co5300_paint_frame()`'s painted geometry, y >= 440 split at x = 22, 158 and 294, with two details measured rather than assumed: the Y threshold is 440 and not the painted 445 because taps decode a few pixels high near the bottom of the glass (TCH-004), and a press on a bar deliberately reports no ABS_X/ABS_Y, because the strip is outside fb0 by construction so a coordinate there would only invite a consumer to clamp it in. Sliding along the strip changes button without firing a second click, so a drag across three bars is one press rather than three. With that working the operator reports the emulator running **materially faster**, which is EMU-010's predicted 30 percent arriving now that nothing polls the controller on the CPU the emulator runs on. This closes the trade the documentation had recorded as "you can have buttons or 34.7 percent, not both": that trade only existed because the bars were decoded by a userspace poller competing with the emulator, and on the touch interrupt they cost nothing. The stale passages were superseded rather than deleted -- the reasoning about why stopping the daemon killed the buttons is still the reason the mapping was hard to get right, so it is kept with a note that its conclusion no longer applies. One honest limit on this cycle's evidence: the emulator's log confirms it found the device (`buttons: /dev/input from touch-buttons` rather than its "buttons will do nothing" error) and the operator confirmed all three bars respond, but no press line was captured in the log, so the evidence that a press traverses the new path is the operator's report plus the device registration rather than a recorded event. Nothing was left running that could capture it, and the emulator is still up for the next cycle. The stall log also shows 29 stalls still occurring, 14 of them with `dirty 0xFC`, which is the time-screen transition of EMU-002 and unrelated to throughput; it is deliberately left for the next cycle at the operator's request.

#### Next Steps:

The stalls are the next discussion, and this cycle's numbers sharpen what they are: 29 stalls in an otherwise faster emulator, 14 of them on `dirty 0xFC` and the longest early ones at 286 and 211 ms wall with panel time at 104 and 41 ms. That is the EMU-002 shape -- the guest stops changing display RAM, the loop falls behind, and the panel figure during a stall runs an order of magnitude above its 9.5 ms benchmark. EMU-002 names the next measurement: read the stall log against a press. The watch is in the right state for it -- touch is interrupt driven, the bars work, and `tamafb` writes the stall log -- so it needs only a press on the middle bar while the log is tailed. Beyond that, a full 16 MiB backup should be taken, since `working-2026-11-10-1621.bin` predates the haptic driver and the bar mapping. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/touch/final/cst92xx.c
- docs/tamagotchi.md
- docs/panel-reservations.md
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 43 COMMIT Unreleased 2026-11-10T16:36:09-07:00

#### Coming From:

Unreleased a253f61

#### Purpose:

Benchmark the V3 emulator against the real ROM rather than a synthetic workload, and settle the claim that the panel stops keeping up at four frames per second.

#### Outcome:

The emulator has six to seven times the headroom it needs and the four-frame ceiling was wrong. Measured with `tamafb` running the real V3 ROM, flat out and headless, after a reboot so the 512 KB PSRAM block was fresh: **513765 instructions/s with the ROM in PSRAM** (29.67 M cycles/s, 7.42x realtime) and **408300 with it in the flash window** (23.58 M cycles/s, 5.90x). Both arms retired exactly 692707 instructions over 40000000 emulated cycles, 57.74 cycles per instruction, and every run landed within 0.6 percent across two boots each, so the comparison is like for like. With the display running at 10 fps and paced by the `speed_pct` argument the loop held 1.01x at 100 percent, 1.50x at 150 percent and **2.01x at 200 percent**, the tool's ceiling, using about half its 100 ms frame budget at the top of that range. Frame rate is not a limit either: 1.00-1.01x held at 2, 4, 8 and 16 fps. A 20 s run at 10 fps drew 200 frames, repainted 40 and logged no stalls. The documentation previously stated "4 fps gives ~1.0x realtime; the panel is the limit", which was true of an earlier build with the touch daemon competing for the CPU and is now false; it has been replaced rather than left to mislead. The more useful consequence is what this rules out. Since the emulator is paced to 1.00x and has six times the headroom to honour it, neither the display refresh rate nor the ROM's location is what makes the Tamagotchi feel slow, and optimising either would buy nothing for realtime play. The remaining slow path is the specific guest stall in EMU-002 -- 580 ms of ROM computation with no display-RAM change -- which is untouched by this cycle and remains the only open emulator question. This cycle also records a measurement trap that has now bitten twice. The new figures do not reproduce the 204033 and 273098 instructions/s previously recorded for the emulator, and **neither set is wrong**: those came from `corebench`, which runs a synthetic workload, while these come from `tamafb`, which runs the real ROM and spends more of its steps asleep -- and a sleep step is cheap. The PSRAM delta differs for the same reason, +25.6 percent here against the recorded +33.8, because the gain depends on what fraction of the workload is ROM reads. The corollary is written into both the record and the document: compare `corebench` against `corebench` and `tamafb` against `tamafb`, never across the two. A second smaller trap was confirmed in passing: the 512 KB PSRAM allocation succeeds exactly once per boot, so any multi-run sweep silently measures the flash-window arm after the first run unless it reboots between arms -- which is why the bench numbers were taken one per boot and the reason is stated with them.

#### Next Steps:

The only remaining emulator question is EMU-002, and this cycle narrows what it is. It is not throughput, not the panel, not the frame rate and not the ROM's location, which leaves the guest's own 580 ms of computation during the time-screen transition. EMU-002 names the next measurement precisely: read the stall log against a press on the fixed-point build to see what the guest is executing in that window. That is now cheap to run -- touch is interrupt driven, the stall log already exists in `tamafb`, and a press on the middle bar is the only input needed -- but it requires a physical press, so it is a cycle for when the operator is available. Worth noting that the unexplained part is the **panel figure**: 43-135 ms per frame during a stall against 9.5 ms in the benchmark, a tenfold gap with no established cause, which the no-stall result above does not explain either. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- docs/tamagotchi.md
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 42 COMMIT Unreleased 2026-11-10T16:26:05-07:00

#### Coming From:

Unreleased 614c633

#### Purpose:

Capture a full 16 MiB backup of the first state in which every subsystem on the I2C bus has been verified, taken from a cold boot.

#### Outcome:

`~/twatch-backup/working-2026-11-10-1621.bin` is the current recoverable state, 16 MiB read at 628.6 kbit/s in 213.5 seconds, sha256 `464b04138ada0baaec2822dbd6aa708194d76d7a3bf2b21d003aa2c2050dfa2f`. It replaces `working-2026-11-10-1533.bin`, which was three cycles stale: it predated the 5 s idle poll, the RTC and PMU interrupt verification, and the haptic driver. Nothing about those cycles was risky, but a saved image that no longer matches what ships stops being a useful fallback, and this session has already spent an afternoon on a backup that silently contained the wrong kernel. The watch was rebooted first and every subsystem re-verified before the read, so this is a cold-boot image: touch interrupt driven with `idle 5000 ms, active 20 ms, release 250 ms`, `drv2605` bound and reporting `DRV2605 ready, ERM motor`, PMU capacity 100 with `usb_online` 1, `rtc0` at 23:21:05, and fb0 at 320x400. The image was verified four ways rather than by the script's own OK line. The stored `.sha256` matches an independent `sha256sum`; the 2860624-byte kernel extracted from `0x140000` is byte-identical to the built `xipImage` (`862d0a6c…`); the image carries all five markers -- `hynitron,cst92xx`, `ti,drv2605` with `DRV2605 ready, %s motor`, the interrupt-path string `idle %u ms, active %d ms, release %d ms, %s`, and the I2C recovery string `int_raw=%08x sr=%08x busy=%u`; and the running watch reports `7.2.4-forkbank-gf134bd8126fa-dirty`, the same build string the image carries. The image grew from 2860408 to 2860624 bytes against the previous cycle, and that 216-byte delta is itself a check that the haptic driver is in the build rather than only in the source -- a small delta was worth confirming precisely because it looked too small for a new driver, and `drv2605_tap` was found as a global symbol in `System.map` before the flash was trusted. The backup's trailing hard reset rebooted the watch, which was confirmed healthy afterwards: `drv2605` re-probed, `cst92xx` re-registered on IRQ 33, and `rtc0` read 23:25:45. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 41, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The I2C bus is now closed: every device on it either has a working driver or a proven reason not to, and this image is the first that captures all of that at once. The largest remaining unknown is the second SPI bus, and it is unstarted ground rather than a gap to be filled. BRD-004 records that the bus on SCK 35 / MOSI 34 / MISO 33 has never been proven to transfer a byte; the NFC probe never received a reply, with MISO clamped low by something external; `CONFIG_MMC` is absent so the SD card could not mount even if the bus worked; and the earlier loopback attempt that would have settled it was itself blocked by a pad-driving bug. The honest next step is therefore to prove the bus in isolation -- a loopback, or one readable register from one device -- before wiring SD, LoRa and NFC drivers to it, because three drivers built on an unvalidated bus turn one unknown into four. The smaller open item remains the absent-address `-ETIMEDOUT` versus `-ENXIO` defect in `i2cdetect`, which affects no working path. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 41 COMMIT Unreleased 2026-11-10T16:15:16-07:00

#### Coming From:

Unreleased 913f9e3

#### Purpose:

Restore haptic feedback on touch, which went silent when the touch daemon was retired, by moving the DRV2605 sequence from a now-uncalled userspace library into the kernel.

#### Outcome:

The click is back, and this was a regression this project caused rather than a missing feature. `userspace/haptic.c` held the DRV2605 sequence and its only caller was the retired touch daemon, so when that daemon was removed the library stayed behind with no callers and the motor went silent while the file remained in the tree looking maintained. The sequence is now `patches/kernel/haptic/final/drv2605.c`, an `i2c_driver` exporting `drv2605_tap()`, which the touch driver calls on the press transition. The order is the one established by measurement: MODE 0x01 = 0x00 for internal trigger, RTP 0x02 = 0x00, LIBRARY 0x03 = 0x01 for the TS2200 A ERM set (library 6 is for LRA parts and buzzes weakly here), WAVESEQ1 0x04 = effect 1 (Strong Click 100%), WAVESEQ2 0x05 = 0x00 as the terminator, then GO 0x0C = 0x01. The sequence is rewritten on every tap rather than once at probe, because writing GO with a stale sequence replays whatever was in the wave registers, which is how a tap ends up buzzing for a second. One gating detail matters and was carried over deliberately: the controller re-reports a held finger about every 66 ms, so the click fires on the `!touching` to `touching` transition only -- clicking per report would buzz continuously instead of tapping. The driver identifies the part by STATUS bits 7:5 and refuses to write effect IDs to anything else, and it exposes a `tap` sysfs attribute so the motor can be exercised without a touch. Verified on hardware in three stages, each separating a different layer: the sysfs trigger fired eight times produced eight distinct felt clicks, proving the sequence and motor without involving touch at all; tapping the screen five times then produced five felt clicks, proving the touch-to-driver-to-motor chain end to end; and `/proc/interrupts` advanced 100 interrupts on IRQ 33 over that window, proving the events came through the interrupt path rather than the idle poll. `userspace/haptic.c` and `haptic.h` were deleted, since an uncalled copy of a sequence that the kernel now owns is exactly the sort of thing that gets edited by mistake later, and `scripts/build-userspace.sh` still builds its three remaining binaries cleanly without them. The README's haptic section was rewritten to describe the kernel driver and to record why the sequence lives there. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 76 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 40, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

A full 16 MiB backup should be taken; `working-2026-11-10-1533.bin` predates the 5 s idle period, the RTC and PMU verification and this driver, so it is several cycles stale even though it still restores a working watch. With touch, both PMU edges, the RTC alarm path and haptics all verified, the I2C story is closed: every device on the bus either has a working driver or a proven reason not to. That leaves the second SPI bus as the largest remaining unknown, and it is unstarted ground rather than a gap to fill -- BRD-004 records that it has never been proven to transfer a byte, the NFC probe never received a reply with MISO clamped low, and `CONFIG_MMC` is absent so the SD card cannot mount even if the bus works. Proving the bus itself, with a loopback or a single readable register from one device, is the honest next step there rather than wiring three drivers to an unvalidated bus. The smaller open item remains the absent-address `-ETIMEDOUT` versus `-ENXIO` defect in `i2cdetect`. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/haptic/final/drv2605.c
- patches/kernel/haptic/final/drv2605.h
- patches/kernel/touch/final/cst92xx.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- patches/kernel/config/devkit_c1_16m_linux.config
- scripts/build-kernel.sh
- README.md
- userspace/haptic.c (removed)
- userspace/haptic.h (removed)

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 40 COMMIT Unreleased 2026-11-10T15:57:37-07:00

#### Coming From:

Unreleased f287cad

#### Purpose:

Find out whether the RTC's and PMU's interrupts, mapped but with permanently zero counts, were broken or simply quiet.

#### Outcome:

Both work, and neither was ever broken. The zero counts meant no event had occurred, not that the path was dead -- a reading this project had carried as a suspicion about the hardware and had never tested by generating an event. The method in both cases was to drive the device rather than to read its counter. For the RTC that meant arming an alarm through `/sys/class/rtc/rtc0/wakealarm` for 20 s ahead, which advanced `7: esp32xx-gpio 1 pcf85063` from 0 to 1; the PCF85063A's Control_2 register reads 0x07 at rest, with bit 7 (AIE, alarm interrupt enable) and bit 5 (TIE, timer enable) both clear, so the INT pin cannot assert until something arms it, and nothing on this watch schedules an alarm. For the PMU it meant a quick tap of Pusher A -- the power key, which is not a GPIO at all and reaches Linux only through `axp20x-pek` -- which advanced `8: esp32xx-gpio 7 axp2101_irq_chip` from 0 to 2 and both PEK sub-sources from 0 to 1, `axp20x-pek-dbr` (rising, press) and `axp20x-pek-dbf` (falling, release). The AXP2101's pending register confirmed it at `00 08 00`, bank 1 bit 3, PEK_SHORT. Two sub-interrupts from one GPIO line and one press is the expected shape: the top-level handler services each pending source on the PMU. The AXP2101's enabled mask explains its quiet default too -- registers 0x40..0x42 read `00 c3 00`, so only bits 0, 1, 6 and 7 of bank 1 are unmasked, which is exactly the four sub-IRQs the driver registers (PEK rising, PEK falling, VBUS removal, VBUS plug-in) and nothing else. One correction to the record: the device tree carried a comment above the PMU node warning that "GPIO7 has no pull-up here, and a LEVEL_LOW IRQ on it storms and hangs the boot", which contradicted the node beneath it and is now demonstrably wrong -- `pmic_irq_pins` does give pin 7 a weak pull-up, the interrupt is connected and level-low, and two presses later the kernel was healthy with both edges serviced. The comment was replaced rather than left in place, because a comment telling the next reader that a working thing is dangerous is worse than no comment. The DTS change alters no generated output -- the compiled DTB is byte-identical -- so it required no redeployment on its own account; the kernel was reflashed anyway so that the running build string matches the committed source exactly rather than merely behaving the same. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 75 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 39, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

A full 16 MiB backup should be taken; `working-2026-11-10-1533.bin` predates both the 5 s idle period and this cycle, though this cycle changed no executable behaviour so the image remains a faithful restore of a working watch. The remaining open item on this thread is the absent-address `-ETIMEDOUT` versus `-ENXIO` defect, which makes `i2cdetect` report timeouts for empty addresses and affects no working path; it is small and self-contained. With touch, both PMU edges and the RTC alarm path now verified end to end, every peripheral on the bus has a proven interrupt or a proven reason to be quiet, which closes the interrupt work that began with the touch driver. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/display/final/esp32s3-devkit-c1.dts
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 39 COMMIT Unreleased 2026-11-10T15:47:12-07:00

#### Coming From:

Unreleased 5b23d84

#### Purpose:

Prove that the touch interrupt, and not the safety-net poll, is what discovers a touch, and ship the idle period that makes the interrupt's benefit real.

#### Outcome:

The interrupt is doing the work, and that is now a measurement rather than an assumption. The previous cycle left one claim unproven: the driver was interrupt driven with a 250 ms idle fallback, and at that value a broken interrupt would still look like working touch that was merely sluggish -- the same class of silent failure that hid behind the full interrupt descriptor table for a whole session. The test was designed so the poll could not be credited. `hynitron,idle-poll-ms = <5000>` was set in the device tree, and the driver already reads that property, so the change was one DTS line and one build. Against a 5 s idle poll, a tap that is pressed and released within a fraction of a second is usually sampled by the poll only after the finger has gone, at which point the controller reports nothing: a dead interrupt would miss most or all taps. Eight deliberate taps one second apart produced **eight presses and eight releases**, 48 events in the captured stream, with coordinates clustering in one region of the glass (x 158..191, y 208..252) and `/proc/interrupts` advancing to 255 on IRQ 33. The poll could not have produced that result. The value was therefore kept rather than reverted, because it is both proven and useful: idle bus traffic is about 2 reads per second instead of the ~50 an unconditional 20 ms poll generated. The idle poll is deliberately retained at 5 s as a safety net rather than removed -- if the INT line ever stops asserting, touch degrades to slow instead of disappearing, and a slow tap is recoverable where a dead screen is not. No rebuild was needed beyond the DTS change: the flashed kernel was verified to match the built image by build string, and the running driver reports `idle 5000 ms, active 20 ms, release 250 ms, interrupt driven`. One environmental distraction is worth recording so it is not mistaken for a regression later: partway through verification the watch stopped answering on the network while remaining perfectly healthy on every other measure -- interface UP, correct address, gateway ARP present, IPC interrupt count climbing, RTC and PMU fine. The cause was not the kernel: the operator reported that the watch had paired with a nearby BLE mouse, and WiFi was being disrupted by the BLE activity. All verification from that point was done over the USB console instead. The wiring had been reached over WiFi only; the console proved the kernel was never at fault. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 73 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 38, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

A full 16 MiB backup should be taken, since `working-2026-11-10-1533.bin` predates the 5 s idle period; the difference is behavioural rather than structural, so the existing image restores a working watch with a 250 ms idle poll, but the saved state should match what ships. After that the next item is the one this work uncovered rather than fixed: the RTC's and PMU's interrupts on GPIO 1 and GPIO 7 are mapped and register, yet show zero counts in `/proc/interrupts`. That was previously attributed to the hardware and now deserves re-testing on its own terms, because the interrupt descriptor table turned out to be the cause of the last unexplained interrupt failure and the same class of problem has not been excluded for these two. The smaller open item is the absent-address `-ETIMEDOUT` versus `-ENXIO` defect, which still makes `i2cdetect` report timeouts for empty addresses and affects no working path. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/display/final/esp32s3-devkit-c1.dts
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 38 COMMIT Unreleased 2026-11-10T15:38:11-07:00

#### Coming From:

Unreleased de0dfda

#### Purpose:

Capture a full 16 MiB backup of the first state with interrupt-driven touch and the enlarged interrupt descriptor table, from a cold boot.

#### Outcome:

`~/twatch-backup/working-2026-11-10-1533.bin` is the current recoverable state, 16 MiB read at 628.6 kbit/s in 213.5 seconds, sha256 `d1b87c3089ddfed46f5bca2193546c242fb9d692489be3c4172a0da176b3da9d`. It closes the gap left at the end of the previous cycle, where `working-2026-11-10-1444.bin` had working touch but polling only and predated both the interrupt work and the `NR_IRQS` increase. The watch was rebooted first and every subsystem re-verified before the read, so this is a cold-boot image: `cst92xx` bound and reported `irq: client=0 gpiod_to_irq=33` followed by `interrupt driven`, `/proc/interrupts` carried `33: esp32xx-gpio 12 cst92xx`, the PMU read 100, `rtc0` read 22:33:07 against a seven-byte register read of `0x07 0x33 0x22`, fb0 was 320x400, WiFi was at 10.10.0.15 and the BLE keyboard was running. The image was verified four ways rather than by the script's own OK line. The stored `.sha256` matches an independent `sha256sum`; the 2860368-byte kernel extracted from `0x140000` is byte-identical to the built `xipImage` (`dd3f424d…`); the image contains `hynitron,cst92xx`, the interrupt-path format string `idle %u ms, active %d ms, release %d ms, %s` and the earlier I2C recovery format string `%s: int_raw=%08x sr=%08x busy=%u`, so the touch driver, the interrupt work and the bus fix are all present; and the running watch reports `7.2.4-forkbank-g5c05e1d68be7-dirty`, the same build string the image carries. The image is also 2,860,368 bytes against the previous cycle's 2,841,808, the difference being the enlarged interrupt descriptor array -- a size change that is itself evidence the `NR_IRQS` fix is in the build rather than merely in the source. The backup's trailing hard reset rebooted the watch, which was confirmed healthy afterwards: `cst92xx` bound again on IRQ 33 and `rtc0` reading 22:37:44. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 37, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The kernel is now cheap to experiment on: a failed attempt costs a four-minute restore from 1533 rather than a session. Three candidates, in the order they are worth doing. The first is the idle-poll experiment the driver was built for: `hynitron,idle-poll-ms` already exists as a property, and raising it to something like 5000 ms would make the interrupt demonstrably the sole source of idle discovery, which is the last unproven claim in touch. The second is the RTC and PMU interrupts, which are mapped and show zero counts in `/proc/interrupts` and were previously blamed on the hardware; now that the descriptor table has room and the mapping mechanism is understood, that explanation deserves to be re-tested rather than inherited. The third is the absent-address `-ETIMEDOUT` versus `-ENXIO` defect, which still makes `i2cdetect` report timeouts for empty addresses; it affects no working path and is the smallest of the three. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 37 COMMIT Unreleased 2026-11-10T15:29:30-07:00

#### Coming From:

Unreleased 8eebe06

#### Purpose:

Drive the touch controller from its interrupt instead of polling it, and in doing so find out why no GPIO interrupt on this board had ever worked.

#### Outcome:

Touch is interrupt driven, and the reason it could not be is a port defect that had been silently costing every driver its interrupt since the beginning. The intended change was small: `cst92xx` now requests TP_INT as a GPIO with `int-gpios = <&gpio0 12 GPIO_ACTIVE_LOW>`, turns it into an IRQ with `gpiod_to_irq()` rather than through an `interrupts` property, and keeps polling at two rates -- 20 ms while a finger is down and 250 ms when idle as a safety net -- so that a wrong polarity or a lost pull-up degrades to latency rather than to no touch at all. What blocked it was that `gpiod_to_irq()` returned `-ENXIO`. Mapping a spread of lines in one boot settled the shape of the fault: lines 1 and 7 returned IRQs 7 and 8 while lines 0, 6, 12, 16 and 17 all returned `-ENXIO`, so the discriminator was not the line but whether something had already claimed it. That pointed away from the GPIO controller and at the allocation itself. The cause is in `arch/xtensa/include/asm/irq.h`: with `CONFIG_SPARSE_IRQ` unavailable on xtensa, the descriptor table is sized `NR_IRQS = XCHAL_NUM_INTERRUPTS + PLATFORM_NR_IRQS + 1`, which on this core is 32 + 0 + 1 = 33. That is right for peripherals hanging directly off the CPU's interrupt lines and wrong for this SoC, where the GPIO controller maps one Linux IRQ per GPIO line a driver asks for and the AXP2101 adds a virtual domain on top. The table was full. `irq_create_mapping()` returned ENOSPC, `gpiod_to_irq()` turned it into `-ENXIO`, and every driver that treats its interrupt as optional fell back to polling and worked -- so the visible symptom was "this board's GPIO interrupts do not work" rather than "the table is full". It also explains a detail that had been recorded but not understood: both of the interrupts that do exist, the RTC's and the PMU's, show zero counts in `/proc/interrupts`, because mapping a line is not the same as its parent firing. Two false leads were pursued and discarded on evidence rather than assumption, both worth recording. The first was that `gpio-esp32.c` lacks a `xlate` callback; it does not need one, because gpiolib supplies `gpiochip_domain_ops` with `irq_domain_xlate_twothreecell`, which accepts two cells, and the RTC's line mapping proves it works. The second was `CONFIG_PLATFORM_NR_IRQS`, which would have been the natural knob; it is a hidden symbol with no prompt, so `olddefconfig` recomputes it straight back to 0, and the config check in the build script caught that rather than letting it through. The fix is therefore a larger table in `patches/kernel/irq/final/irq.h`: `EXTRA_NR_IRQS 128`, about 18 KB of image and roughly 24 KB of `.bss` against 7.8 MiB of RAM, carried whole because nothing else in the tree touches that header and a patch would have to track it forever. Verified on hardware: `gpiod_to_irq()` now returns IRQ 33 -- the first number past the old limit -- the driver reports "interrupt driven", `/proc/interrupts` gains `33: esp32xx-gpio 12 cst92xx`, and 331 interrupts accumulated across three taps while a captured event stream showed three presses, three releases and smooth coordinate tracks including `(174,254)` through `(214,393)` and `(258,428)` through `(291,441)`. Idle rate measured at 20 interrupts in 10 seconds, against the ~50 reads per second the previous unconditional 20 ms poll generated. The bus, RTC, PMU, panel, WiFi and BLE keyboard all remained healthy throughout, with the RTC reading 22:29:09 against a seven-byte register read of `0x09 0x29 0x22`. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all YAML blocks in the reference parse and all 72 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 36, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

A full 16 MiB backup should be taken: `working-2026-11-10-1444.bin` contains working touch but predates the interrupt work, so no saved image has interrupt-driven touch or the enlarged interrupt table. The interrupt is now the wake-up path rather than the only path, which is the safe shape, so the next question for touch is not whether it works but whether the 250 ms idle poll can be raised much higher, making the interrupt demonstrably the sole source of idle discovery; the driver already takes `hynitron,idle-poll-ms` as a property for exactly that experiment, and it was not run in this cycle. Two smaller open items remain. The absent-address `-ETIMEDOUT` versus `-ENXIO` defect still makes `i2cdetect` report timeouts for empty addresses. And the RTC's and PMU's own interrupts, now that the table has room and the mapping is understood, are worth revisiting: both are mapped and neither has ever fired, which was previously attributed to the hardware and may simply be the same class of problem. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/touch/final/cst92xx.c
- patches/kernel/irq/final/irq.h
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- scripts/build-kernel.sh
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 36 COMMIT Unreleased 2026-11-10T14:48:04-07:00

#### Coming From:

Unreleased 6af4934

#### Purpose:

Capture a full 16 MiB backup of the first state in which touch works on the hardware I2C bus, taken from a cold boot rather than a warm one.

#### Outcome:

`~/twatch-backup/working-2026-11-10-1444.bin` is the current recoverable state, 16 MiB read at 628.8 kbit/s in 213.5 seconds, sha256 `1a2637f5f9c781fe3dc0a19476fbb47550d019a10f4b428658e17d1fccd17c5a`. It is the first backup that contains working touch over the hardware bus, and it closes the gap noted at the end of the previous cycle, where no saved image had either the I2C recovery fix or `cst92xx`. The watch was rebooted first and the subsystems were re-verified before the read, so the image is a cold-boot state rather than a warm one: `cst92xx` bound as `CST9220 at 0x1a, 410x502, fw 0x01000003` and registered `cst92xx-touch` as input2, `rtc0` read 21:44:04 against a seven-byte register read of `0x05 0x44` (21:44:05), WiFi was at 10.10.0.15 and the BLE keyboard was running. The image was verified four ways rather than by the script's own OK line, because a backup that silently contains the wrong kernel is worse than none and this project has already lost a session to exactly that. The stored `.sha256` matches an independent `sha256sum`; the 2841808-byte kernel extracted from `0x140000` is byte-identical to the built `xipImage` (`d47e2cc3…`), proving the right kernel is in the right slot; the image contains the `hynitron,cst92xx` compatible string and the recovery format string `%s: int_raw=%08x sr=%08x busy=%u` together with the `ARB_LOST` literal, proving both the touch driver and the I2C fix are inside it; and the running watch reports `7.2.4-forkbank-g0a3083746891-dirty`, the same build string the image carries. One detail worth recording for the next person who greps an image for evidence: the recovery log line does not contain the literal `NACK:`, because it is formatted from `%s` with `"NACK"` or `"ARB_LOST"` selected at the call site, so searching for `NACK:` in a kernel image returns nothing even when the code is present. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 35, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The next kernel change now costs a four-minute restore rather than a session, which matters because the remaining touch work is the one part likeliest to need iteration. That work is interrupt-driven operation: `TP_INT` is GPIO 12 and LilyGo attaches a falling-edge handler to it, which would replace the 20 ms poll and reduce both bus traffic and the NACK rate. It was deferred deliberately again, because the polling path is what proved the protocol and an unverified interrupt would mix two unknowns. A second candidate, smaller and independent, is the absent-address `-ETIMEDOUT` versus `-ENXIO` defect, which still makes `i2cdetect` report timeouts for empty addresses; it is noted in the driver as an open item and does not affect any working path. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 35 COMMIT Unreleased 2026-11-10T14:40:11-07:00

#### Coming From:

Unreleased 4b83f00

#### Purpose:

Replace the retired userspace touch daemon with a kernel input driver, written from LilyGo's own source rather than from the earlier register map, and fix the I2C recovery defect that had been wedging the whole bus.

#### Outcome:

Touch works as a kernel input driver, and the bus fault that had been blocking it is fixed. The driver is `patches/kernel/touch/final/cst92xx.c`, a polling `i2c_driver` that registers an `INPUT_PROP_DIRECT` input device with `ABS_X`, `ABS_Y` and `BTN_TOUCH`. It was written by reading LilyGo's source in full rather than by extending the previous guesswork: `lewisxhe/SensorLib` `src/touch/TouchDrvCST92xx.cpp` (MIT) and `Xinyuan-LilyGO/LilyGoLib` `src/LilyGoWatchUltra.cpp` `initTouch()`. Both were fetched from GitHub and are cited in TCH-005 through TCH-008. The most important discovery is that the reset line is not an SoC GPIO at all -- it is P08 of the XL9555 expander, whose port 1 configuration reads 0xFF at boot, so the line was an input and nothing had ever reset the part; the driver takes it through `devm_gpiod_get_optional` with a `reset-gpios` phandle so probe waits for the expander. The official bring-up is four attribute reads after entering command mode -- checkcode at 0xD1FC requiring `(code & 0xFFFF0000) == 0xCACA0000`, resolution at 0xD1F8, chip type at 0xD204 requiring 0x9217 or 0x9220, and firmware at 0xD208 rejecting 0xA5A5A5A5 -- and probe fails on any mismatch instead of continuing hopefully. Measured on hardware: checkcode 0xcaca4e20, resolution 410x502, chip type 0x9220 with ProjectID 0x5a67, firmware 0x01000003. That resolution equals the panel exactly, so raw controller coordinates are panel coordinates with no swap, mirroring or scale, and no calibration constants are needed; the 320x400 fb0 window is the 45 px inset rectangle, so consumers subtract 45 and clamp. Three protocol details came out of the same reading and each had already caused a wrong result: only event 0x06 is a press, so a non-zero finger count with event 0x00 invents a phantom (observed at raw (41,451) with nothing touching); byte 6 must be 0xAB for a block to be valid; and the part emits no release event at all, so release is inferred from a 250 ms gap, about four of its ~66 ms report periods. The `0xD1 = 0x1E` "unlock" handshake that earlier work treated as required is not part of official bring-up -- it lives in `setMode()`, whose only caller is `sleep()` -- and TCH-002 was revised in place to say so. The second and larger finding is a bus defect that predated this work: `esp32_i2c_run()` returned `-ENXIO` on NACK and `-EAGAIN` on arbitration loss after clearing the interrupt, without calling recovery, which was wired only into the timeout branches. Since a polled capacitive controller produces exactly those outcomes, one such failure left `SR.bus_busy` set with the FSM idle, the pre-transfer check never fired, and every later transfer to every device failed until a reboot -- this is what wedged the bus every time touch was read, and what the old daemon hit. Recovery is now on those paths, gated on the bus actually being busy so an absent device still NACKs cheaply, and `esp32_i2c_transfer()` propagates the real error instead of collapsing everything to `-ETIMEDOUT`. Verified on hardware: a 10 s run of 1687 report reads produced 188 errors, 11 genuine NACKs each logged as `NACK: int_raw=00008610 sr=5600c010 busy=1` followed by `bus recovered`, and RTC, PMU and expander all still answered afterwards. The driver then bound as `cst92xx 0-001a: CST9220 at 0x1a, 410x502, fw 0x01000003`, registered `cst92xx-touch` as input2, and captured real events: `ABS_X=258 ABS_Y=309 BTN_TOUCH=1` then a smooth drag through `(259,305) (262,299) (267,289) (280,270)` and a `BTN_TOUCH=0` release. The user confirmed the coordinates were correct for the area touched. `userspace/tprobe.c` is kept as the diagnostic that established all of this, and it is the only binary in this tree built static: with about 7.8 MB of RAM and no swap, a fresh dynamic process fails at exec with "can't load library 'libc.so.0'" or segfaults under the loader. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, all six YAML blocks in the reference parse, all 70 records carry the ten required fields with unique ids, and this entry was validated against the template with six sections in canonical order, sequential numbering from 34, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Interrupt-driven operation is the obvious improvement and is now the main thing left in touch: `TP_INT` is GPIO 12 and LilyGo attaches a falling-edge handler to it, which would replace the 20 ms poll and cut both bus traffic and the NACK rate. It was deliberately not attempted in this cycle because the polling path is what proves the protocol and an unverified interrupt would have mixed two unknowns. A full 16 MiB backup should be taken: `working-2026-11-10-1353.bin` predates both the I2C recovery fix and the touch driver, so no saved image contains a watch with working touch on the hardware bus. Beyond touch, the deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/touch/final/cst92xx.c
- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- patches/kernel/config/devkit_c1_16m_linux.config
- scripts/build-kernel.sh
- scripts/build-userspace.sh
- userspace/tprobe.c
- .gitignore
- .ai/core-reference.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 34 COMMIT Unreleased 2026-11-10T13:58:39-07:00

#### Coming From:

Unreleased 1d56b39

#### Purpose:

Capture a full 16 MiB backup of the working hardware-I2C state, because no saved image contained the multi-byte fix and the previous newest backup predated it.

#### Outcome:

`~/twatch-backup/working-2026-11-10-1353.bin` is now the current recoverable state, 16 MiB read at 583 kbit/s in 230 seconds, sha256 `ee2133e0389c4ca6910df2f272c44755f22a7709004a80771b2c2e711e801309`. It was verified three independent ways rather than by accepting the script's own OK line, because a backup that silently contains the wrong kernel is worse than no backup at all and this project has already lost a session to exactly that. The stored `.sha256` was checked against a fresh `sha256sum` of the file; the 2841584-byte kernel extracted from the image at offset `0x140000` is byte-identical to the built `xipImage` (`a79b73d0…`), which proves the correct kernel is in the correct slot; and the running watch reports `Linux version 7.2.4-forkbank-ga623647af724-dirty`, the same build string the image contains, so the saved kernel is the one that has been tested rather than a stale neighbour. The image also carries the fix markers — the `FIRST SCL timeout` diagnostic string is present — and both `espressif,esp32-i2c` and `i2c-gpio` compatible strings, which is expected: `i2c-gpio` is still compiled in, but the live device tree selects the hardware driver and only `60013000.i2c` exists as a platform device, so the bit-banged path is present but unused. The backup's trailing hard reset rebooted the watch, which was confirmed back to a working state afterwards: hardware I2C bound, a seven-byte RTC read returning `0x21 0x58 0x20 0x10 0x02 0x11 0x26` = 20:58:21, and PMU capacity 100. The two images that matter for recovery are now clearly distinguished: 1353 is the working hardware-I2C watch without touch, and 1053 remains the only image that restores a fully functional watch including touch, on the older bit-banged bus. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 33, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The next kernel change starts from a verified image, so a failed attempt now costs a 4-minute restore instead of the whole session. That cycle is the kernel input driver that replaces `tbtns`: the bus is proven, the split-READ shape and command budget that make a 16-bit-addressed 15-byte read possible are established, and the two remaining unknowns are the CST9217's initialisation sequence and releasing its reset line through the XL9555. The absent-address `-ETIMEDOUT` versus `-ENXIO` defect belongs in the same cycle, so `i2cdetect` stops reporting timeouts for empty addresses. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 33 COMMIT Unreleased 2026-11-10T13:52:28-07:00

#### Coming From:

Unreleased 23c918a

#### Purpose:

Verify that the multi-byte I2C fix and the retirement of `tbtns` both survive a cold boot, since the previous cycle's evidence was all taken within one uptime.

#### Outcome:

Both hold across a reboot, and the bus came up clean without intervention. This mattered because every earlier finding in this work was confounded by the same weakness: a warm bus that had already been probed into a working state cannot distinguish a fix that persists from one that merely got lucky on the first boot. The watch was rebooted from the shell and checked from zero uptime. PMU published capacity 100 / 4175000 uV / Full with `usb_online` 1, `rtc0` read 20:50:12 and `hwclock -r` succeeded, and a seven-byte read of the RTC at register `0x04` returned `0x18 0x50 0x20 0x10 0x02 0x11 0x26` -- seven distinct non-`0xff` bytes decoding to the exact BCD time 20:50:18, which is the precise shape that was broken before and returned `0x57 ff ff ff ff ff ff` on every device for the whole session. A 60-operation soak followed, fifteen rounds over all four devices, and produced zero failures with zero kernel errors; a second check after another minute still read `0x52 0x51 0x20 0x10 0x02 0x11 0x26` = 20:51:52 against a clock reading 20:51:52. `tbtns` stayed gone: moving it to `/etc/disabled/` survives a reboot because `rcS` globs `S??*` and the file is no longer under `init.d`, so there is no repeat of the wedge and no manual step needed after a boot. Load average behaved as expected, 1.16 shortly after boot decaying to 0.42 within two minutes, which is ordinary boot spin and not a fault -- the earlier session had misread a load of 1.4 as a problem before establishing that it decays. WiFi came up at 10.10.0.15, the BLE keyboard is running, and fb0 is 320x400. Touch reads `0xff 0xff` at register `0x00` instead of its `0xca` checkcode, which is correct and expected: no driver is loading, and nothing releases the CST9217's reset line through the XL9555. Nothing was changed in this cycle, so no rebuild or reflash was required and the deployed kernel is the one verified. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 32, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Take a full 16 MiB backup of this state, because none of the four saved images matches it: `working-2026-11-10-1251.bin` predates the multi-byte fix and still carries the chatty driver, and `working-2026-11-10-1053.bin` is the older `i2c-gpio` build that restores touch but on the bit-banged bus. A clean image of the working hardware-I2C state is the one thing this session produced that is not otherwise recoverable. After that the next cycle is the kernel input driver to replace `tbtns`: the bus is proven, the split-READ shape and command budget that make a 16-bit-addressed 15-byte read possible are established, and the two remaining unknowns are the CST9217's initialisation sequence and releasing its reset line through the XL9555. The absent-address `-ETIMEDOUT` versus `-ENXIO` defect is worth folding into that same cycle so `i2cdetect` stops reporting timeouts for empty addresses. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 32 COMMIT Unreleased 2026-11-10T13:47:00-07:00

#### Coming From:

Unreleased 7e5efdc

#### Purpose:

Fix the multi-byte I2C read that returned one real byte followed by `0xff`, by reading ESP-IDF's `i2c_master_read()` instead of guessing at the ACK field, and retire `tbtns` after establishing that it was the one thing wedging the bus.

#### Outcome:

The multi-byte read works and the bus is stable, and the fault was never in the ACK field it had been blamed on. ESP-IDF never issues a single `READ(N)` with `LAST_NACK`: `i2c_master_read()` splits into `READ(data_len - 1)` with `I2C_MASTER_ACK` followed by a one-byte `READ` with `I2C_MASTER_NACK`, because `ack_val` is the level the master drives on the last byte of *its own command*, so one `READ(N)` with `ack_val = 1` NACKs after the first byte and the slave stops transmitting while the master keeps clocking the pull-ups. That is exactly `0x57 ff ff ff ff ff ff`, and `rxfifo_cnt` correctly reported `N`, which is why no short-read warning was ever logged and why the symptom could not be attributed from outside. The driver's code comment asserting that `ack_val = 1` "matches ESP-IDF" was a misreading of the same function and is corrected. Making that split fit took the command budget to exactly its limit, `seven real commands plus END at index 7` for a 16-bit register address plus a 15-byte read, so the address byte was merged into the register write, which is the same bytes on the wire and is what `i2c_master_write()` does for any `data_len > 1`. Three further defects fell out of the same reading and each was independently sufficient to keep the bus unreliable: `time_out_en` is bit 5 of `I2C_TO` and the driver wrote bit 10, a reserved field, so the hardware timeout had never been enabled and a stretched bus could only be caught by the driver's own one-second spin; `recover()` programmed `SCL_SP_CONF` without pulsing `conf_upgate`, so the nine-clock bus clear had never actually run and recovery could not free a held bus, which is why every earlier wedge required a power cycle instead of self-recovering; and `tout` was the constant 16 rather than `i2c_ll_cal_bus_clk()`'s derived value. The driver now also tests `INT_SCL_ST_TO` (bit 13) and `INT_SCL_MAIN_ST_TO` (bit 14), which it had never tested at all, and recovers on either. Verified on hardware: a seven-byte RTC read returns seven distinct real bytes that decode as the correct time in BCD (`0x52 0x45 0x20 0x10 0x02 0x11 0x26` = 20:45:52 on 2026-11-10), `hwclock -r` succeeds where it previously failed with `EINVAL`, and twenty operations across all four devices produced zero errors. `tbtns` was then retired by user decision, because the bus wedge was traced to its 15-byte touch read and a new touch driver is planned: it is stopped and moved to `/etc/disabled/`, its source, init script and selftest are removed from the tree, and it is recoverable from `1c37c50`. With it gone every remaining subsystem verifies clean — PMU capacity 100 / 4176000 uV / Full / usb online 1, RTC ticking and readable, expander `0xff 0xff`, haptic `0xe0`, panel bound at 320x400 16 bpp and painting, WiFi at 10.10.0.15, BLE keyboard running, load average down from 1.09 to 0.28 — and `dmesg` records zero `xfer FAIL`, zero `SCL timeout` and zero `short read`. One defect is knowingly left open and is not a regression: a transfer to an absent address returns `-ETIMEDOUT` rather than `-ENXIO`, so `i2cdetect` reports `--` from a timeout instead of a fast NACK. It costs scan speed and clean probe semantics, nothing else. The diagnostics added while chasing this are bounded counters rather than `printk_ratelimit()`, because `tbtns` polling at 66 Hz permanently exhausted the shared ratelimit state and silenced every log line exactly when it mattered, and because this board's console mirrors to fbcon so an unbounded dump costs a minute of boot. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 31, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The hardware I2C bus is working and is the default, so the next cycle is a real touch driver to replace `tbtns`, and it should be a kernel input driver rather than a userspace daemon: the CST9217 needs its reset line released through the XL9555 first, which is why the device currently answers `0xff 0xff` at register `0x00` instead of its `0xca` checkcode, and the 15-byte report block must be read whole because the acknowledge that advances the controller's buffer is issued against the block. The split-READ shape and the command budget that made that read possible are now proven, so the remaining work is initialisation sequence and input plumbing, not bus debugging. Worth fixing in the same pass or before it: absent addresses should return `-ENXIO` from the NACK interrupt instead of running to the SCL timeout, which would make `i2cdetect` immediate and give every driver's probe path correct semantics. A fresh full 16 MiB backup should be taken of this state, because the newest saved image `working-2026-11-10-1251.bin` predates the multi-byte fix and still carries the chatty driver; this cycle deliberately did not take one. The deferred items are unchanged and none should be started without asking: BLE media keys, the Rii K06 touchpad and Logitech keyboard, firmware-side BLE bond persistence, wake-from-sleep, and moving the Tamagotchi emulator to core 0.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- scripts/build-userspace.sh
- scripts/watch-deploy.sh
- userspace/haptic.c
- .gitignore
- userspace/tbtns.c (removed)
- userspace/S50tbtns (removed)
- scripts/tbtns-selftest.py (removed)

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 31 COMMIT Unreleased 2026-11-10T13:05:12-07:00

#### Coming From:

Unreleased 224f585

#### Purpose:

Find what was adding about a minute to boot, after the user noticed something called i2c-esp32 busy for a long time at startup.

#### Outcome:

It was this project's own debug logging, and the flag had been left on. The `espressif,debug` property added to the device tree during entry 29's command-word diagnosis was never removed, and with it every I2C transfer printed all eight command registers. At boot that came to 241 log lines spread over 100 seconds, which is what the user saw: not a program, and not a timeout, but the driver talking about itself. The time was not the serial output either -- it was that this board's console mirrors to fbcon, so each of those 241 lines forced a repaint of the 320x400 panel over SPI, and 241 repaints is most of a minute. Only two driver timeouts occurred in the same boot, about 2 s total, so the transfer path was never the cost. Two changes were made rather than one. The flag is gone from the device tree, and the dump is now behind `printk_ratelimit()` so that even with it deliberately enabled it cannot flood the console again; the second matters more, because a diagnostic that can stall the machine is worse than the fault it is hunting. Verified on hardware after flashing: the log dropped from 241 lines to 6, those six are the legitimate one-time probe messages (clock rate, the two pin routes, the input route, the system registers, and the master registration), and all of them land between 4.4 s and 6.6 s, so the driver finishes probing in about two seconds. The subsystems were unaffected -- `rtc0` still present and reporting, `axp20x-battery` capacity 100. The user confirmed the boot is now much faster. Worth recording as a pattern rather than an incident: this is the second time in one session that a diagnostic caused a worse symptom than the bug it was instrumenting, the other being the bus recovery that generated the arb_lost fault it was meant to cure. Both had the same shape -- instrumentation whose cost was never bounded. Note for whoever restores a backup: `working-2026-11-10-1251.bin` predates this fix and therefore still carries the chatty driver, so a restore from it will boot slowly until this change is reflashed. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 30, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The remaining I2C work is unchanged and is the touch controller's 16-bit register address, documented at the end of `docs/cores.md`: fold the address into the data WRITE or split the transfer, since a 16-bit read needs one more command register than `SOC_I2C_CMD_REG_NUM` allows. Until that lands `tbtns` cannot run and the watch has no local input. A fresh full backup should be taken once boots are confirmed fast and that fix is in, so the saved image does not carry the chatty driver; this cycle deliberately did not take one.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 30 COMMIT Unreleased 2026-11-10T13:01:47-07:00

#### Coming From:

Unreleased be88103

#### Purpose:

Finish the hardware I2C driver by reading ESP-IDF's source rather than by flashing, after entry 29 left the bus working for one transfer and failing thereafter.

#### Outcome:

The hardware I2C bus now works, sustained, and the two defects that made it unfixable by experiment were both one-line errors in a header field list. Reading `i2c_struct.h` and `soc_caps.h` rather than flashing found them: `bus_busy` is bit 4, not bit 3, and bit 3 is `arb_lost`, so with the define one low the driver's `SR_BUS_BUSY` was reading the wrong flag entirely -- a genuinely stuck bus never triggered recovery and the arbitration-lost path never fired, which is the whole "one transfer per boot" symptom. And recovery must restore the timing registers: ESP32-S3 leaves `SOC_I2C_SUPPORT_HW_FSM_RST` undefined with the comment "FSM_RST only resets the FSM, not using it", so ESP-IDF cycles the clock gate and then re-writes every timing register, and skipping that restore left the peripheral at default timing so the next transfer reported `scl_main_st_to` with `arb_lost` -- an electrical-looking fault that was self-inflicted. A third correction followed: the one-bit `fsm_rst` is not usable on this chip at all and is gone, and `ack_val = 1` (NACK the last byte) is correct for every read, matching ESP-IDF's `I2C_MASTER_LAST_NACK`; the earlier "use 0 for multi-byte" rule had been derived from a test that also carried the address-phase bug, so the evidence was contaminated. Verified on hardware and sustained: capacity 100, voltage_now 4178000 uV, status Full, usb online 1, `rtc0` readable and ticking from 19:42:50 to 19:42:58, a credible `i2cdetect` in which only 1a, 20, 34 and 51 answer, and ten consecutive reads with no failure. A full 16 MiB backup of exactly that state was taken and hash-verified, `working-2026-11-10-1251.bin`, sha256 `04cfee94`, confirmed to carry the 40 MHz marker, the `espressif,esp32-i2c` compatible string and no `i2c-gpio`. One gap remains and it is precisely characterised: the CST9217 needs a 16-bit register address and that transfer shape hangs, with the failure state unambiguous -- `int_raw=0x200` (trans_start only), `txfifo_cnt=3` (bytes never sent), `resp_rec=1` (the address WAS acknowledged), `bus_busy=0` -- so the controller sends the address, receives its ACK and never chains to the next command. Only the touch controller is affected; the PMU and RTC use one-byte addresses. The strongest lead is the command-register budget, since a 16-bit read needs 8 command registers and `SOC_I2C_CMD_REG_NUM` is 8. Two observations about the board's behaviour were also settled rather than left as suspicions: the load average of 1.4 shortly after boot decays to 0.48 and the CPU measures 4 percent busy with 96 idle, so it was stale boot-time spin from probe failures and not a live fault; and the RTC and PMU reads that failed while a flash read was in progress recovered on their own, so that was transient contention rather than a regression. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, KRN-006 was updated in place to record the resolution, and this entry was validated against the template with six sections in canonical order, sequential numbering from 29, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The touch controller needs one change, and it is small enough that it should not cost another dozen builds: a 16-bit register read currently emits 7 commands plus END, one more than `SOC_I2C_CMD_REG_NUM`, so either fold the address into the data WRITE or split the transfer in two. Until that lands, `tbtns` cannot run and the watch has no local input, which is the only thing standing between this and full function. Everything else on the I2C bus is working and backed up, so the next cycle should be that one fix and nothing else. Beyond the bus, the deferred items are unchanged: the Tamagotchi front end still wants a `S51tamafb` from rcS, and LCD-009's tearing and the LCD-005 versus LCD-010 disagreement remain open.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 29 COMMIT Unreleased 2026-11-10T11:56:24-07:00

#### Coming From:

Unreleased 8e19197

#### Purpose:

Take the suggestion to read how LilyGo and ESP-IDF actually drive this peripheral, rather than continuing to reconstruct the sequence from headers, after entry 28 left the driver unable to start a transfer.

#### Outcome:

The method was the fix, and the bus now works: reading ESP-IDF's own `components/driver/i2c/i2c.c` instead of its headers found five further defects that eight header-based flash cycles had not, and with them the peripheral carries real traffic. `i2cdetect` returns a credible scan in which only `1a`, `20`, `34` and `51` answer and every other address is absent, single-byte reads return correct values (`i2cget 0x51 0x04` gives `0x07`, `i2cget 0x34 0xa4` gives `0x4c`), `rtc-pcf85063` registers as `rtc0`, the AXP2101 probes and publishes `axp20x-battery` and `axp20x-usb`, and `axp20x-pek` appears as an input device. Nine defects were found in total, and they are worth listing because each one alone would have been enough to stop the driver: completion was polled on a command's `done` bit which END never sets; `INT_STATUS` is masked by `INT_ENA`, which is zero in polling mode, so it reads zero for ever and `INT_RAW` is the register to poll; the clock gate is closed at boot in `SYSTEM_PERIP_CLK_EN0` bit 7; the peripheral is held in reset in `SYSTEM_PERIP_RST_EN0` bit 7, which is KRN-006 and the expensive one; the timing needs `scl_wait_high_period` and the HAL does not subtract one from `scl_high`; `sclk_div_num` is bits [7:0] and not [13:8]; the GPIO matrix needed the INPUT route as well as the output one, without which the controller could never read SDA and never saw an ACK, and `pad_driver` had to be set for open drain; the ACK flags are per-opcode, with RESTART and STOP taking none, WRITE taking `ack_en` and READ taking `ack_val` only, where this driver had been setting `ack_exp` everywhere and so expecting NACKs; and `rxfifo_cnt` is bits [13:8] rather than the [10:5] a truncated comment in `i2c_struct.h` suggests, which is the second time that header has misled this work. What the cycle demonstrates beyond the bus itself is a method: the headers are incomplete and in at least two places wrong, `i2c_struct.h`'s opcode comment contradicts the HAL and its field comments are truncated, and the driver source that ships with the hardware is the authority. Eight flash cycles were spent before that was tried. What still fails is multi-byte reads: they return the first byte and then `0xff`, so `i2ctransfer -y -f 0 w1@0x51 0x04 r3` gives `0x07 0xff 0xff`. Single-byte reads are correct, which is why the PMU probes fully and the RTC registers but `hctosys` cannot read its seven-byte block and `pca953x` cannot read the expander. The command words were dumped from the running controller and are correct -- RESTART `00003000`, WRITE 1 byte `00000901`, READ 3 bytes `00001c03`, STOP `00001000`, END `00002000` -- so the encoding is not the fault and the fault is in the RX path, where the FIFO only ever holds one byte. The board is left bootable with a partly working bus: PMU and RTC present, expander and touch not, and `tbtns` not running. The backup at `~/twatch-backup/working-2026-11-10-1053.bin` still restores a fully working watch. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates and KRN-006 was revised in place with its verification extended, and this entry was validated against the template with six sections in canonical order, sequential numbering from 28, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

One more attempt at the multi-byte read, then restore regardless of the outcome. The command encoding is proven correct by dump, so the next check is the RX side: how many bytes the FIFO actually holds after a read (dump `SR` and the `rxfifo_cnt` field), whether the drain loop is reading `I2C_DATA` correctly for FIFO mode, and whether ESP-IDF's `i2c_ll_read_rxfifo()` call sites reveal a step this driver is missing. If that does not settle it in one flash, restore `~/twatch-backup/working-2026-11-10-1053.bin`: a watch with a working bit-banged bus is worth more than a watch with a half-working hardware one, and everything learned here is preserved in the driver source, KRN-006 and `docs/cores.md` for the next attempt. Beyond the driver, the deferred items are unchanged and none of them should be started until the bus question is closed either way.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 28 COMMIT Unreleased 2026-11-10T11:38:52-07:00

#### Coming From:

Unreleased f32af28

#### Purpose:

Write the hardware I2C driver that entry 27 identified as the real fix for a third of core 1 being spent bit-banging the bus.

#### Outcome:

The driver is written, builds, links and probes, and it does not work yet; the board is currently running with no I2C at all. What it did establish is worth more than the failure. Four genuine defects were found and fixed, each of which would have blocked any future attempt: completion was being polled on a command's `done` bit, which the END command never sets, and the real signals are `int_status.trans_complete` and `int_status.nack`; the timing setup omitted `scl_wait_high_period`, which shares the `SCL_HIGH_PERIOD` register in bits 15:9, without which the bus cannot run at all; the peripheral's clock gate is closed at boot, `SYSTEM_PERIP_CLK_EN0` at `0x600c0018` bit 7 reading 0, and nothing on this board had ever opened it because the firmware does not use I2C and the bit-banged bus needed no peripheral clock; and the peripheral is held in reset at boot, `SYSTEM_PERIP_RST_EN0` at `0x600c0020` bit 7 reading 1 in a register worth `0x9eff1fb8`. That last one is the finding that matters and it is now KRN-006: while the reset is held, every write to the controller is silently discarded while reads still return data and the TX FIFO still accepts bytes, so `CTR` reads back `0x20b` -- the power-on default with master mode and clock enable clear -- immediately after a successful write. That signature imitates three different faults in sequence, a bad command encoding, a wrong completion signal and a dead bus, which is why diagnosing it took eight build-and-flash cycles. Releasing and pulsing the reset makes `CTR` hold `0x313` as written, which proves master mode and clock enable now genuinely take effect, and the clock reports 80 MHz. What remains is that `trans_start` does not start the sequencer: `int_status` stays zero and the TX FIFO keeps its six bytes. Once the reset was out of the way, register offsets were checked against the member order in `i2c_struct.h`, command opcodes against the HAL rather than the stale comment in that header, pin routing read back as `out_sel` naming signals 90 and 89, and the write ordering of `conf_upgate` and `trans_start` split and flushed and rejoined, and none of it moved the result. The honest reading is that the count of excluded causes now exceeds what a single wrong bit can explain, so the model of the block is wrong somewhere rather than one value being off, and the next step is to read ESP-IDF's own `components/driver/i2c/i2c.c` master transfer routine line by line instead of continuing to reconstruct it from headers. That is a change of method, not another guess, and it is the method that should have been used first. The board boots and runs throughout with the console and USB console working, but the PMU, RTC, expander and touch controller all fail to bind, so the watch is degraded; the rollback is `~/twatch-backup/working-2026-11-10-1053.bin`, taken before any of this, which restores `i2c-gpio` and a fully functional watch. `docs/cores.md` now carries an appendix recording what works, what was excluded, and this exact next step, so a future attempt starts from evidence. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates at 65 records with 65 index keys and no orphans in either direction, and this entry was validated against the template with six sections in canonical order, sequential numbering from 27, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Restore `~/twatch-backup/working-2026-11-10-1053.bin` so the watch has working I2C again: the driver is not close enough to justify leaving the PMU, RTC, expander and touch controller unbound. The driver source stays in the tree at `patches/kernel/i2c/final/i2c-esp32.c` with every excluded hypothesis recorded in its comments, and the DTS switch is a single node that reverting is a two-line change for. Then read ESP-IDF's `i2c.c` master path and compare its start sequence against this driver's before flashing anything again; the trigger, not the configuration, is what is unproven, and eight cycles of header reconstruction have earned a different approach.

#### Files Modified:

- patches/kernel/i2c/final/i2c-esp32.c
- patches/kernel/display/final/esp32s3-devkit-c1.dts
- patches/kernel/config/devkit_c1_16m_linux.config
- scripts/build-kernel.sh
- docs/cores.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 27 COMMIT Unreleased 2026-11-10T11:04:36-07:00

#### Coming From:

Unreleased 1c37c50

#### Purpose:

Settle, from the port's own sources and the vendor's firmware, whether the RTOS on core 0 should own the hardware with a fast link to Linux, before any driver work is committed to.

#### Outcome:

The answer is no, and it is not a judgement call: there is no channel to build it on. The port documents its own split in `build/Linux-on-esp32-S3/ARCHITECTURE.md`, and the firmware confirms it in one function. Core 0 runs ESP-IDF and FreeRTOS and owns the WiFi radio, whose driver is a closed binary that only runs under ESP-IDF. Core 1 runs Linux natively, and `linux_boot.c` starts it as `xTaskCreatePinnedToCore(linux_task, ..., 1)` where `linux_task()` is `map_flash_and_go(); esp_restart();` -- Linux is a FreeRTOS task pinned to core 1 that never returns, and `esp_restart()` is the path taken only if it ever does. So the split is forced by one blob rather than chosen as a design principle, and everything except the radio -- panel, PMU, RTC, touch, haptics, USB, filesystem -- is already owned directly by Linux through ordinary drivers. The link between the cores carries a network interface, Bluetooth, and typed custom interfaces, of which this project added one for firmware logs; it carries no I2C, SPI or GPIO RPC and has no generic peripheral channel. LilyGo was checked as asked, and its architecture answers a different question: `Xinyuan-LilyGO/LilyGoLib` runs FreeRTOS on both cores with no OS boundary and no IPC, so there is nothing to offload to, and its touch code reaches the controller through a direct `Wire` call from whatever task is running. Two facts from it are still useful. It pins the NES emulator and its audio task to core 0 alongside the radios, which shows core 0 has headroom for compute in a single address space but says nothing about moving I/O. And `LilyGoWatchUltra.cpp` calls `Wire.begin(SDA, SCL)` with `setClock(1000000)` for the touch panel, which confirms from the vendor that the hardware I2C peripheral works on this board's pins at a megahertz against the 100 kHz we bit-bang. Meshtastic has no T-Watch Ultra target and MeshCore has no variant for it either, so neither adds a third data point. The proposal fails on five grounds already recorded in this project, and they are now written down together rather than scattered: no channel exists and the shmem link is contended enough that one extra log line stopped WiFi bring-up; every transaction would become a round trip that the deliberate 15 ms touch poll cannot absorb; core 0 cannot write flash; core 0 is the radio core and a polling server would contend with WiFi and BLE under a watchdog that resets the chip; and every device driver already exists and works on Linux, so moving the bus means rewriting them as firmware in a tree that must never be pushed. The distinction worth keeping is that computation may cross the boundary while I/O should not, because computation batches into a shared buffer and I/O needs a round trip per transaction, which is why EMU-004's emulator-on-core-0 design remains the one legitimate offload and stays deferred. The bit-banged I2C is therefore a missing driver and not a placement mistake: the fix is to write `i2c-esp32.c`, which removes the CPU cost instead of moving it and gains 4 to 10x on the bus as well. A system-wide CPU sample was taken to size the problem rather than argue about it, and it is worth stating with its disagreement intact: `/proc/stat` over five seconds shows 26.0 percent of the one core busy with 22.9 percent of it in kernel time, while `tbtns` alone measured 34.6 percent in kernel time earlier in the session -- the two do not reconcile, and the likely cause is that NAK-driven console traffic varies with load, so the honest reading is that kernel time from the bus is somewhere in the twenties to mid-thirties percent and is the dominant consumer either way. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates at 64 records with 64 index keys and no orphans in either direction, and this entry was validated against the template with six sections in canonical order, sequential numbering from 26, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Write the hardware I2C driver. The design is now constrained by evidence rather than preference: it lives in `patches/kernel/i2c/final/i2c-esp32.c` and installs the same way `spi-esp32.c` does, it programs `I2C0` at `0x60013000` from the register map in the local ESP-IDF `i2c_ll.h` and `i2c_reg.h`, and it routes GPIO3 and GPIO2 through the GPIO matrix using the `I2CEXT0_SDA_OUT_IDX` and `I2CEXT0_SCL_OUT_IDX` signals that `i2c_periph.c` names. The first milestone is deliberately small and reversible: `i2cdetect -y 0` must see all six devices at 100 kHz with the DTS switched over, before interrupts or 400 kHz are attempted. That switch is the risky edit, because PMU, RTC, haptic and touch all move to the new driver at once, so it wants the backup taken at 10:57 and the console watched rather than assumed. The core-0 question is answered and closed; the emulator-offload question remains open under EMU-004 and should not be reopened on the strength of this cycle's finding about compute headroom, which was inferred from a single-address-space design and does not transfer to ours.

#### Files Modified:

- docs/cores.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: NOT RUN

---

## 26 COMMIT Unreleased 2026-11-10T10:43:07-07:00

#### Coming From:

Unreleased a47de90

#### Purpose:

Undo a regression introduced in entry 25, where the touch daemon stopped polling during gameplay and took the only input path with it.

#### Outcome:

The user asked whether the buttons still worked after entry 25 paused the daemon, and the answer was no. `tamafb` reads `BTN_0`, `BTN_1` and `BTN_2` from the uinput device that `tbtns` creates and from nothing else, so stopping the daemon stopped the pet from being playable: the previous cycle had optimised away the thing the buttons exist for. The daemon now slows to 66 ms while a fullscreen app owns the panel instead of stopping, which matches the controller's own 66 ms report cadence, and both transitions are verified in the log on hardware, `tbtns: slowing to 66 ms for tamafb` and `tbtns: back to 15 ms, tamafb has exited`, with `tamafb` confirming `buttons: /dev/input from touch-buttons` and polling it during the run. The measurement attached to the fix contradicts the explanation given for it in entry 25, and that is the more useful half of this cycle. Slowing the poll from 15 ms to 66 ms -- quartering the read rate -- moved throughput by 1.1 percent, 132679 against 131226 instructions/s. So the 34.7 percent the daemon costs is not the poll cadence, even though the arithmetic appeared to predict it: 3957 us per read every 15 ms is a 26 percent duty cycle, which matches the measured loss closely enough to have been convincing. It was still wrong. Whatever is expensive happens once per wakeup rather than once per read, and the candidates are the scheduler cost of the `i2c-gpio` kernel thread for each transaction, the console traffic behind every NAK, or the syscall round trip; none of that is established and profiling is the next step if the number ever matters. The practical conclusion is stated plainly in `docs/tamagotchi.md` rather than buried: you can have working buttons or 34.7 percent of the emulator, not both, and the buttons win because a watch whose pet cannot be played is not the goal. The speed belongs to starting the emulator at boot, where the ROM may fit in PSRAM and recover 33.8 percent without touching the daemon at all. Two measurement caveats are worth recording rather than smoothed: the A/B above is two runs per arm rather than the three used elsewhere, because the board kept interfering, so treat 1.1 percent as close to the noise floor and the direction as the only firm claim; and three attempts at a cleaner A/B failed for tooling reasons, not reasons of substance -- a stand-in process built by copying `/bin/sleep` segfaulted because busybox dispatches on `argv[0]` and `tamafb` is not an applet name, `/home` reached 97 percent full when that copy landed there at 855 KB, and a long telnet command line was truncated before it could run. A purpose-built 6 KB idle binary fixed it, and a subdirectory was needed because `comm` is the basename. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 25, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The emulator thread's one blocking item is unchanged and now carries both remaining wins: a `S51tamafb` started from `rcS`, which makes the emulator start at boot, may let the 512 KB ROM allocation happen while an order-7 block still exists, and must write `/var/run/tamafb.pid` or not even the 66 ms slow-down will engage. The daemon's 34.7 percent is worth profiling only if it becomes the thing standing in the way; until then the honest position is that it is unexplained and the buttons are worth more than it. Beyond the emulator, the display items are unchanged: LCD-009's tearing root cause, and the 4.2x disagreement between LCD-005 and LCD-010. The boot loop from entry 23 remains unexplained with its dump pruned.

#### Files Modified:

- userspace/tbtns.c
- docs/tamagotchi.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 25 COMMIT Unreleased 2026-11-10T10:26:48-07:00

#### Coming From:

Unreleased 109f9b5

#### Purpose:

Recover the third of the emulator's throughput that the touch daemon was taking, without giving up the tap latency the daemon exists for.

#### Outcome:

`tbtns` now stands down while a fullscreen app owns the panel, and both transitions are verified in the daemon log on hardware: `tbtns: standing down for tamafb` when the app starts and `tbtns: resuming, tamafb has exited` when it stops. The cost being recovered was measured the same boot on the same binary, 134496 instructions/s with the daemon polling against 206091 with it stopped, so the daemon takes 34.7 percent, or 53 percent more throughput without it. The arithmetic says why rather than leaving it a mystery: one touch read costs about 3957 us on this bit-banged I2C bus and the loop polls every 15 ms, so roughly a quarter of the core goes into waiting out bus traffic that is almost always about to report nothing new. Both constants are correct for their purpose, so the fix is not to poll slower but to stop polling while something else needs the core. The daemon reads `/var/run/tamafb.pid`, which `run-tama.sh` already writes, and validates that pid against `/proc/<pid>/comm` before believing it. That validation earned its place twice: a stale file after a crash would otherwise disable the buttons permanently and silently, which is worse than the bug being fixed, and during testing a launch wrapped in a shell had `comm` of `bash` rather than `tamafb` and was correctly ignored. Standing down also forces a release before yielding, so a finger held at the moment the app starts cannot leave a key stuck, because nothing below runs to time it out. The fallback interval is 200 ms, long enough to cost nothing and short enough that resuming is not noticeable. The band-arithmetic selftest still passes. The same cycle closed the PSRAM question from entry 24 in the negative and, in doing so, corrected two of my own errors. The patch is deployed and does exactly what it should when the allocation fails, printing `rom: 512 KB PSRAM allocation refused ... falling back to flash, 33.8% slower` followed by `rom: flash window`, and the run was clean at 4,130,898 emulated cycles/s and 1.03x; so the fallback path, which had never once been exercised, is now proven. But it will never take the PSRAM arm as things stand. A malloc ladder on the board gives malloc(524288) FAIL and malloc(524000) OK, so the boundary lies in that 288-byte window and it is the allocator's own header: a request equal to the largest free block leaves nothing over for the header itself. Splitting the ROM, which was the user's suggestion and a good one to test rather than argue about, makes it worse: malloc(262144) twice produced one success and one failure, because each block pays its own header out of the same budget. Two corrections follow for the records. My reading of /proc/buddyinfo had been against the wrong end of the line and the block it called order 8 is order 7, 512 KB; the line has eleven fields, orders 0 to 10, and nothing larger than 512 KB has ever been free in these readings. And an attempt at fixing this by allocating 1024 bytes less than the ROM while still reading the full ROM would have overrun the buffer; it was caught before building, the source was restored from the original plus the patch, and the rebuild came back byte-identical at sha256 7447986c, so nothing unsafe was ever deployed. KRN-004 and `docs/tamagotchi.md` were revised to record the boundary, the failed split, and the correction. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference still validates at 63 records with 63 index keys, and this entry was validated against the template with six sections in canonical order, sequential numbering from 24, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The emulator thread now has one clear blocking item: a `S51tamafb` started from `rcS`, which would both make the emulator start at boot rather than by hand and, if it runs early enough, let the 512 KB ROM allocation happen while an order-7 block still exists, which is the only route to the 33.8 percent that entry 22 measured. Nothing else in that thread is worth doing first, and the two cheap wins are now one delivered and one blocked on that service. Beyond the emulator, the display items are unchanged and both are diagnosis rather than design: LCD-009's tearing root cause, still open with SPI clock excluded, and the 4.2x disagreement between LCD-005's 190 ms frame and LCD-010's 45.7 ms projection, which needs a re-measurement through the real repaint path before either figure is quoted again. The boot loop from entry 23 remains unexplained and its dump was pruned in entry 24, so a recurrence has to be captured fresh.

#### Files Modified:

- userspace/tbtns.c
- docs/tamagotchi.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 24 COMMIT Unreleased 2026-11-10T09:47:12-07:00

#### Coming From:

Unreleased b81ac0f

#### Purpose:

Re-establish the 40 MHz panel state that the restore had rolled back, and leave exactly one qualified image so a recovery no longer has to guess which backup is the right one.

#### Outcome:

The 40 MHz state is back on the board and for the first time there is one image that can be restored without a decision. The kernel was rebuilt from the tree, `build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage`, 2833064 bytes, and written alone to `0x140000`, which is the same kernel-only operation that produced the state on the night of 2026-11-09; it reported `Hash of data verified` in 23.1 seconds and the board came up with `spi_hz` reading 40000000 and both `tbtns` and `blekbd` running, with `/home`, `/etc` and the ROM slot untouched. Two pre-flight checks were made first and both passed: `space_for_vectors` in the firmware ELF is `0x40380000`, matching `CONFIG_VECTORS_ADDR`, so no firmware rebuild or vectors realignment was needed, and the image ends at `0x3F3AA8`, clear of the ROM slot at `0x480000` by 561 KB. The rebuilt kernel differs from the one that was running this morning in exactly 12 bytes, both clusters being the build timestamp, `#1 Tue Nov 10 00:22:04 MST 2026` against `#2 Tue Nov 10 09:37:47 MST 2026`, so it is the same code and not a new build risk. A full 16 MiB backup was then taken to `~/twatch-backup/working-2026-11-10-0940.bin`, sha256 `fd7e5bf9`, and verified three ways rather than by exit status: the hash checks with `sha256sum -c`, the `spi_hz` marker is present in the image, and the kernel region is byte-identical to the `xipImage` that was flashed, with the ROM slot still reading `A5 5F 10 49`. The backup directory was then pruned from 17 images and 259 MB to two images and 33 MB. This cycle also settles why the last three attempts at recovery kept going wrong. There was never a qualified 40 MHz image: the 40 MHz default landed in commit `3feff4f` at 00:29 on 2026-11-10, while the newest full backup, `working-2026-11-10-0023.bin`, was written at 00:23, six minutes earlier, exactly as entry 18 had warned. Every image on the machine was old, which is why restoring any of them returned the panel to 20 MHz and why the only 40 MHz image in existence was the boot-looping dump. All 16 images were checked for the `spi_hz` marker rather than by filename, and that check is the one that should have been run before the first restore. Two of my own errors are recorded because they cost the user time: the looping dump was described as the user's working state and restored, when it was a dump of a device that was already failing, and a `find` used to locate images compared a full-flash dump against a bare kernel image at the same offset, which is meaningless for the bare image. The pruned set keeps `working-2026-11-10-0940.bin` and `twatch-factory-16MB.bin`. The factory image was kept against the stated plan of deleting everything but the qualified image, because checking it showed its `factory` and `rootfs` partitions do not match the qualified backup: the backup carries this project's firmware while that file is the untouched stock state, so it is the one file in the directory that cannot be reproduced. Everything deleted was either reproducible from the qualified image, reproducible from the repository, or a known-bad dump. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected and contains only this entry, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 23, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The board is on the 40 MHz kernel with the qualified image saved, so the emulator work can resume where entry 22 left it. Two changes are pending and both are small: redeploy `tamafb` from `patches/harness/0001-tamafb-rom-in-psram.patch` so the ROM-in-PSRAM arm reaches the hardware, confirming it prints `rom: PSRAM` on a fresh boot and `rom: flash window` once the 512 KB block is gone, which is the fallback that has still never been exercised; and make `tbtns` stand down while the emulator owns the panel, worth about 30 percent for no memory at all. The boot loop from entry 23 remains unexplained and is now uninvestigable from the image side, because the dump it was diagnosed from was deleted as part of this cycle's pruning; if it recurs the first action is a fresh dump before anything is restored. The panel and display items are unchanged: LCD-009's tearing root cause, and the 4.2x disagreement between LCD-005 and LCD-010.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 23 COMMIT Unreleased 2026-11-10T09:20:41-07:00

#### Coming From:

Unreleased 736e11c

#### Purpose:

Restore the watch from its full flash backup after it entered a boot loop, and record what the recovery cost so the next one is cheaper.

#### Outcome:

The board is recovered and verified on hardware, but the cause of the loop is not established and this entry says so rather than guessing. The failure appeared as an endless reset at exactly `Starting touch buttons:` on every cycle, with `rst:0xc (RTC_SW_CPU_RST)` and `Saved PC:0x403777fd`, while the USB console was still readable and userspace answered nothing; it followed a `sync; reboot` issued after a session in which the WiFi link had already wedged and a 255 KB `tamafb` had been deployed. Before overwriting anything the failing image was dumped to `~/twatch-backup/broken-2026-11-10-0858.bin`, which allows the cause to be bounded even though it cannot be named. Compared against the restored backup, `etc` is byte-identical and the ROM slot at `0x480000` is byte-identical, so this was not `/etc` damage, which is the failure mode the recovery doctrine warns about, and not a damaged ROM. The two partitions that differ are `linux` and `home`. The `linux` difference is fully explained and is not a fault: the running kernel is byte-identical to this repository's `build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage`, the 40 MHz build, while the backup's kernel is the older 20 MHz one, exactly as entry 18 recorded when it warned that restoring this backup returns the panel to 20 MHz. The `home` difference is jffs2, which is log-structured and changes on any write. So nothing in persistent storage accounts for the loop, which leaves a transient runtime or firmware condition as the remaining explanation and no instrument yet pointed at it. The restore itself took three attempts and the failures were the useful part. The documented command, `write_flash 0 0x1000000 <file>`, is wrong for esptool 4.13: `write_flash` takes `<address> <filename>` pairs with no length argument, so `0x1000000` was read as a second filename and esptool died with `[Errno 2] No such file or directory: '0x1000000'` before touching flash. A second attempt ran without `--after no_reset` and reported success, but its output was not captured and the reset esptool performs by default had already booted the board out of download mode, after which Linux owned the USB-Serial-JTAG port (SOC-003) and every later command sat at `Connecting...` forever; that is why holding the board in boot mode did not appear to help, because the lockout came from the previous command's own reset. The write that worked was `--after no_reset write_flash 0 <backup>` against a board held in boot mode, and it reported `Hash of data verified` over all 16777216 bytes in 122.9 seconds, which is the only evidence that counts. `docs/watch-recovery.md` now carries the corrected command and both traps. The measurement work from the previous cycle was preserved rather than lost with the reflash: the Tamagotchi front end's PSRAM change lives outside this repository in `../TamaHarness`, so it was captured as `patches/harness/0001-tamafb-rom-in-psram.patch`, checked by applying it to a scratch checkout of the original and confirming the result is byte-identical to the working tree. The board came back on the older kernel with `buildroot login:` on the console, ping answering, `tbtns` and `blekbd` both running, and a 1 MB contiguous block free. Two consequences are stated rather than left implicit: the deployed `tamafb` is again the old 249112-byte flash-window build because the backup predates the deploy, so the 33.8 percent is not on the board until it is rebuilt and redeployed from the preserved patch; and the panel is back at 20 MHz with no `spi_hz` parameter in `/sys/module/co5300`, so the 40 MHz kernel has to be reflashed if that clock is wanted. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 22, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Three things, in order. Reflash the 40 MHz kernel to `0x140000` from `build/kernel/linux-7.2.4/arch/xtensa/boot/xipImage`, which is verified byte-identical to what was running before the loop, because the restore rolled it back to 20 MHz. Then rebuild and redeploy `tamafb` from `patches/harness/0001-tamafb-rom-in-psram.patch` so the ROM-in-PSRAM arm is actually on the board, and confirm it prints `rom: PSRAM` on a fresh boot and `rom: flash window` once the block is gone, which is the fallback path this cycle never got to exercise. The boot loop itself is still unexplained and should not be considered closed: the next occurrence should be caught with the flash dumped before any restore, as this one was, and with the console captured across a whole cycle, because the evidence that would convict a cause is what happens in the last second before `rst:0xc`, which this cycle saw only as the reset itself. A full-image backup should be taken before the kernel reflash. `scripts/watch-backup.sh` was checked against the same esptool and is correct as it stands, because `read_flash` takes three arguments -- address, size, filename -- and the script passes all three, so the defect was confined to the documented write command and not to the tooling.

#### Files Modified:

- docs/watch-recovery.md
- userspace/memlat.c
- patches/harness/0001-tamafb-rom-in-psram.patch

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 22 COMMIT Unreleased 2026-11-10T08:37:52-07:00

#### Coming From:

Unreleased 199dc64

#### Purpose:

Test the ROM-location lead entry 21 left open, so the emulator's ROM has a measured home instead of a predicted one.

#### Outcome:

The lead was right and the effect is larger than predicted. With the ROM read from the memory-mapped flash window the emulator sustains 204033 instructions/s and 4.49x realtime; with the ROM in PSRAM it sustains 273098 instructions/s and 6.01x, a gain of 33.8 percent, which is 1239 ns saved per emulated instruction, 4901 ns down to 3662 and 1176 native cycles down to 879. Nine flash runs across three boots gave 203008 to 205065 with no overlap against the PSRAM figure, and the PSRAM arm was sampled twice on two separate boots at 273115 and 273080. Both arms retired exactly 454305 instructions at 88.05 emulated cycles per instruction, so the same work was done both times and only the memory differed. The mechanism is confirmed rather than merely observed: the ROM is 72.6 percent of the 2.408 bus accesses an instruction makes, 1.748 of them, and a flash-window read costs 738 ns, which predicts 1290 ns saved; the measured saving is 1239 ns, within 4 percent, so the interpreter was indeed stalling close to full dependent latency on most of its ROM reads. The trade is recorded rather than resolved: the flash window always works at any uptime and costs 33.8 percent every time, while PSRAM buys 6.01x but depends on a 512 KB contiguous block that exists only briefly after boot, measured at 1 MB at 0 minutes, 256 KB by 9 minutes and 128 KB by 32 minutes, with one such allocation consuming it for the rest of the session. That is now KRN-004, and it is why a S51tamafb has to start from rcS. Getting a trustworthy number took correcting two things that would otherwise have produced a wrong one. The first is that the flash baseline was initially read as 141k against entry 21's 204k on what looked like a regression after a reboot, and the cause was not the reboot: the touch daemon was running, it had not been running during entry 21's measurement, and stopping it restored 204k on the same boot, which makes the daemon's cost a finding in its own right at about 30 percent of throughput because it polls the CST9217 over the bit-banged bus every 60 ms and each poll NAKs and prints at KERN_ERR. The second is that the PSRAM arm can only be sampled once per boot, so the first single reading of 273k was re-taken on a fresh boot before being believed, and the two agree to 0.01 percent. With tbtns restored to running and the board left on the flash ROM, both services are up and the flash baseline is intact. The reference was updated to match: EMU-008's consequence now records the lead as closed and quantified, EMU-009 holds the A/B and EMU-010 the daemon cost, KRN-004 records the allocation window, and the index and routing table were extended to 63 records. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates at 63 records with 63 index keys, no orphan in either direction, every routing target resolving and all fences balanced, and this entry was validated against the template with six sections in canonical order, sequential numbering from 21, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The ROM should move to PSRAM, and it is the cheapest large win left: 33.8 percent for a small change to the Tamagotchi front end rather than an interpreter rewrite, with the constraint that the allocation must happen at boot, which the still-unbuilt S51tamafb was already going to do. Alongside it, and worth a comparable amount for no memory at all, the touch daemon should stand down while a fullscreen app owns the panel, which is the same coordination the console loglevel already needs. Those two are worth more together than the block-register rewrite that entry 21 recommended, and they are far smaller, so they should come first and the rewrite should be re-justified against a 6.01x baseline rather than the 4.49x one. The push of entry 21 was completed at the start of this cycle, `main` is at 199dc64 on origin, and this cycle's commit follows it.

#### Files Modified:

- docs/tamagotchi.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: NOT RUN

---

## 21 COMMIT Unreleased 2026-11-10T08:23:11-07:00

#### Coming From:

Unreleased b02dfb7

#### Purpose:

Take the interpreter measurement entry 18 left open, so the decision between a block-register rewrite and the core-0 design rests on counts instead of arithmetic on timings.

#### Outcome:

The emulator now has a measured decomposition and a baseline to be judged against. Running the real V3 ROM for 40,000,011 emulated cycles on the watch with the prof counters enabled retires 454305 instructions in 736660 interpreter steps and 282355 sleep steps, performing 1032471 reads and 61692 writes, so one emulated instruction costs 1.622 steps and 2.408 guest bus accesses; the steps figure is exact rather than approximate because 736660 minus 282355 is 454305, which is to say the interpreter takes exactly one step per opcode and the extra 0.622 is sleep ticks. Wall clock on the plain build is 2.236 s, three consecutive runs giving 204045, 203945 and 203488 instructions/s at 4.48 to 4.49x realtime, so an emulated instruction costs 4922 ns, about 1181 native cycles at 240 MHz. That answers the question the count was for and it answers it in the useful direction: memory is 2.408 accesses at a measured 53 ns, about 128 ns or 2.6 percent of the total, so 97 percent of the cost is the interpreter's own instruction stream, which is exactly what a block-register rewrite removes. Dividing the residual by the cost of a simple dependent operation puts the interpreter at the order of 150 native instructions per emulated instruction, between the 30-80 a tuned core needs and the 250 entry 18 had derived, so the headroom is real and it is in userspace. Two supporting measurements close the obvious objections. Instruction fetch is not the cost even though there is no I-cache: three tests running identical arithmetic and identical dependent table loads over 64 bytes, about 1 KB and about 8 KB of code gave 56.83, 30.52 and 31.69 ns per operation, so expanding the footprint eightfold costs 3.8 percent and a rewrite need not fear growing its code. And the ROM's location is a genuine lead: a dependent read from the linear flash window costs 738.04 ns over a 256-byte window and 757.05 ns over 16 KB against 53.13 to 55.67 ns for the same arithmetic on PSRAM, a control that isolates the window at a factor of 14, and since the ROM is 72.6 percent of the emulator's accesses, moving it into PSRAM is worth a predicted fifth to a quarter of throughput. That is stated as a lead and not a result because it could not be A/B'd: a 512 KB contiguous block does not exist once the board has been up a few minutes, measured at 256 KB largest free block with the allocation failing as 'cannot allocate 524288 bytes for the ROM', so it needs a fresh boot. Getting this far required fixing the instrument three times, and all three are worth recording. The 601 KB compiled-in build does not exec at all, refused with 'nommu: Allocation of length 565248 from process 207 (corebench) failed', which also corrects entry 18's ~575 KB estimate and shows the requirement falls on a loaded segment because the ROM lands in .rodata rather than .bss; the build script gained a third variant, corebench-flash, which reads the ROM through the window at 0x42480000 and allocates nothing, and that is the one that ran. The file-based variant had always been failing on a bug rather than on the board, asking malloc for the file length plus a second full ROM copy, 1052672 bytes for a 512 KB image, which is the defect the harness's own prepared diff describes; it is fixed to allocate exactly n bytes and to report the failure instead of returning silently. And two of the measurements were initially wrong in ways that would have produced confident nonsense: the code-footprint benchmark reported 0.00 ns per operation for its middle variant because the compiler folded a chain whose data and trip counts were visible, fixed by seeding the table from the clock and routing the bounds through a volatile, and the dependent-load figure of 178.83 ns carried in EMU-003 turned out to come from a run of the original memlat.c that was crashing on this NOMMU heap, so the true PSRAM latency is 50.6 to 53.4 ns. The reference was corrected to match: EMU-003 and EMU-005 revised in place with their errors stated, and EMU-006, EMU-007 and EMU-008 added for the decomposition, the fetch result and the flash-window cost, with the index and routing table extended. Emulation is deterministic, which gave a free correctness check: every build reports 454305 instructions and 88.05 cycles per instruction, matching the host build exactly, and romverify confirmed the ROM in the slot first with crc32 301F0E87 and first bytes A5 5F 10 49. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, the reference validates as 60 records with 60 index keys, no orphans in either direction, every routing target resolving and all fences balanced, and this entry was validated against the template with six sections in canonical order, sequential numbering from 20, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

Two things are now worth doing and one of them is cheap. The cheap one is the ROM-location A/B: on the next fresh boot, before anything else allocates, deploy corebench-rom with the ROM file and run it against corebench-flash on the same boot, which either confirms the predicted fifth to a quarter of throughput or kills the lead, and the largest free block at boot needs to be read first because the run is only meaningful if it is at least 512 KB. The other is the block-register interpreter, which is now justified by measurement rather than by estimate -- 97 percent of the cost is the interpreter's own instruction stream, 150 native instructions per emulated instruction against a target of 30-80, and fetch is nearly free so a larger code body is not a risk -- and it has a baseline to be judged against at 204045 instructions/s and 4.49x. Neither is started. Beyond the interpreter, the parked ~1.5 s time-screen wait remains the same problem from the other end, and the display items are unchanged: LCD-009's tearing root cause, and the 4.2x disagreement between LCD-005 and LCD-010.

#### Files Modified:

- userspace/memlat.c
- userspace/codecost.c
- scripts/build-corebench.sh
- docs/tamagotchi.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 20 COMMIT Unreleased 2026-11-10T07:56:53-07:00

#### Coming From:

Unreleased f7803d2

#### Purpose:

Make corebench, the only instrument that runs the SPLB32 interpreter with no panel behind it, a one-command build and deploy so the interpreter cost that entry 18 left unmeasured can be taken on the next hardware session.

#### Outcome:

`scripts/build-corebench.sh` now cross-compiles the harness's corebench from `../TamaHarness/portable` in two shapes, with the same non-optional `XTENSA_GNU_CONFIG` export as the other userspace builds, and prints each artifact's largest `PT_LOAD` because that segment is what exec must satisfy contiguously on NOMMU. The compiled-in variant is 601,780 bytes with a 562,700-byte load, and the ROM lands in `.rodata` rather than `.bss`, which corrects the ~575 KB figure entry 18 estimated before the build existed; the small variant is 78,364 bytes and takes the ROM as a path argument, `malloc()`ing 512 KB instead. Both were rebuilt from scratch after the first attempt and came back byte-identical, and the host build runs, reporting 454,305 instructions over 40,000,011 emulated cycles at 88.05 emulated cycles per instruction, which matches the shape recorded in TamaHarness's RESULTS-3.md and is the available correctness signal for the sources. One defect was introduced and fixed inside the cycle: the script omitted `rom_embed.c` from the compiled-in variant's source list, and the cross linker did not merely report the undefined `rom_embed_len` but then segfaulted on a BFD assertion, which is worth knowing because it looks like a broken toolchain rather than a missing file. The finding that matters more than the script is negative, and it changes the plan: corebench cannot count native instructions, which is exactly what EMU-005 needs. This board has no retired-instruction counter -- the Xtensa exposes `CCOUNT` for cycles and there is no PMU on the ESP32-S3 -- and the obvious host substitute is also closed, because `perf_event_paranoid` is 4 with no `CAP_PERFMON` so `perf stat -e instructions` refuses, and valgrind is not installed. Native cycles per emulated instruction are measurable and measured, 3676 ns at 240 MHz being about 880 cycles, but the instruction count inside them is not, because with no cache and PSRAM stalls the IPC is low and varies with what the guest executes. `docs/tamagotchi.md` now carries a section stating all of that and naming the two real routes to a count, static disassembly weighted by a measured opcode histogram or a `qemu-xtensa` run with instruction counting, along with the cheaper alternative: skip the count, do the block-register rewrite, and measure instructions/s against this baseline, which answers whether the 2-3x exists without first proving the mechanism. Deployment and user test did not run because the watch is not connected. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected and contains only this log entry, `.ai/core.md` was not modified, and this entry was validated against the template with six sections in canonical order, sequential numbering from 19, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, and Status values within the allowed set.

#### Next Steps:

The build is ready and the measurement is one command once the watch answers on 10.10.0.15 or enumerates as `/dev/ttyACM*`: deploy on a fresh boot, because both variants need a large contiguous block and the largest free block is 256 KB after a few minutes of uptime against about 850 KB at boot. The user needs to choose how the interpreter question is settled, since the count cannot be taken as originally planned: weighted static disassembly and a `qemu-xtensa` icount run both cost real work, while the block-register rewrite measured against the new baseline costs the same effort as the optimisation itself and answers the question that matters. Until that is decided, nothing else in the emulator thread should start, and the parked ~1.5 s time-screen wait remains the same problem from the other end.

#### Files Modified:

- scripts/build-corebench.sh
- docs/tamagotchi.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 19 COMMIT Unreleased 2026-11-10T07:54:49-07:00

#### Coming From:

Unreleased 99f3d13

#### Purpose:

Repair the `.ai` control plane, which had drifted far enough that an agent following the recovery policy could not reach the newest engineering records or trust the tracked handoff.

#### Outcome:

Three defects of the same kind were found and closed. Six records -- EMU-001 through EMU-005 and LCD-011 -- had been appended after section 8 instead of into section 5, so the file's own records section ended at EXT-001 while five emulator records and the 40 MHz panel record sat below the retired-id table. `EMU` was used as a `kind` and a `topic_id` on those six records but was absent from `record_kinds`, from the section 2 topic catalog and from the template's kind list, so the file declared thirteen kinds and used fourteen. And section 4's fast lookup index, which is where the policy says to look first, carried no `EMU` line and no `LCD-011` line at all, which meant the seven routing rows pointing at EMU resolved to keys that did not exist; section 3 routing itself was already correct. The six records were moved into section 5 in their existing order with their text unchanged, `EMU` was registered in all three places, `LCD-011` and the five `EMU` lines were added to the index, `last_reviewed` moved to 2026-11-10 and the file's purpose line now names the emulator. Verification is structural rather than eyeballed: 57 records, 57 unique ids, 57 index keys, no record without an index line and no index line without a record, every routing target resolving to a record, all fourteen kinds used also declared, all twelve code fences balanced, and all 57 records inside section 5. The tracked handoff at `.codewhale/handoff.md` was by then seventeen entries stale -- it was written from entry 1 and still claimed `main` at `fd5f252` with 35 records across twelve topics, its "next step" was touch input (delivered in entries 3, 4 and 13) and its "known accepted defect" was the `sm_bonding` pairing failure (fixed in entry 12) -- and because it is tracked deliberately so a clean clone can find the baton, a fresh agent would have started from a plan already completed; it has been rewritten against entries 17 and 18. Two facts about this cycle: the watch was not reachable, with no `/dev/ttyACM*` present and 100 percent packet loss to 10.10.0.15 from a host holding 10.10.0.42 on the same subnet, so no build, deployment or user test applied; and the active log's entries 18 and 17 had been prepended in that order ahead of 1 through 16, so this entry was prepended in turn rather than reordering settled history. The core-syntax audit passed: `.ai/core.md` and `.ai/core-syntax.md` were re-read, the complete `.ai/` diff was inspected, `.ai/core.md` was not modified, this entry was validated against the template with six sections in canonical order, sequential numbering from 18, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, `Files Modified` written as None because the cycle changed only project-control metadata, and Status values within the allowed set.

#### Next Steps:

The road to the records is clear, so the measurement entry 18 left open is the work: EMU-005 is still OPEN because the native-instruction count per emulated step is derived from timing and has never been counted, and the decision between the block-register interpreter and the core-0 design turns on whether that figure is near 250 or nearer 80. Counting it needs the watch, which is currently unpowered or unplugged, so nothing on hardware can proceed until it answers on 10.10.0.15 or enumerates as `/dev/ttyACM*`; a dynamic `corebench` build prepared for that moment is the next cycle. After the count, the parked ~1.5 s time-screen wait is the same problem seen from the other end and needs no separate work.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 18 COMMIT Unreleased 2026-11-10T02:00:00-07:00

#### Coming From:

Unreleased 5b05745

#### Purpose:

Find what actually limits the Tamagotchi emulator on this board, after two
sessions of optimising the wrong thing.

#### Outcome:

The limit is the port, not the chip, and the evidence now says so three
independent ways. First, per-second telemetry taken across a real menu press on
hardware: the emulator's own cost goes from 245-290 ms per second while the pet
idles to 900-1140 ms while a menu is open, while the panel figure FALLS from
50-110 ms to 18-28 ms. The display was never involved; a 580 ms ROM transition
becomes about 1.5 s because the guest gets a third of the real time it needs.
Second, the arithmetic: 3.68 us per emulated instruction measured against a
12.77 ns native yardstick, and 26-38 ns for any single memory access. Third, the
mechanism: the core configuration the kernel and toolchain are built against
declares XCHAL_DCACHE_SIZE=0, XCHAL_ICACHE_SIZE=0 and XCHAL_HAVE_MMU=0, so there
is no cache at all -- confirmed from the other direction by a three-working-set
pointer chase on the watch (4 KB 70.30 ns, 256 KB 68.72 ns, second buffer
78.93 ns) which shows no locality effect whatsoever, and impossible if a cache
were operating. The device tree gives Linux exactly one memory region,
memory@0 at 0x3d800000 for 8 MB of PSRAM, so there is no internal SRAM to move
the hot state into either. Both memory-level escape routes are closed.

The architectural consequence is the interesting result and it is new: both cores
are identical Xtensa LX7 at 240 MHz, but core 0 -- the firmware side -- is the
one with cache enabled and internal SRAM reachable, while Linux runs on core 1
alone (CONFIG_HAVE_SMP is not set and XCHAL_HAVE_EXCLUSIVE=0, so SMP is not
supportable). The emulator is therefore running on the one core of two that has
no cache. Moving emulation to core 0 and leaving core 1 to blit is the plausible
full-speed design, and it is deliberately NOT started: it needs firmware work in
the tree that must never be pushed, core 0 owns the WiFi link this project's
console depends on, and starving it trips the FreeRTOS watchdog (BLE-004).
The recommended next step is smaller and safer: the interpreter retires roughly
250 native instructions per emulated instruction against the 30-80 a tuned core
needs, so keeping guest registers, flags and PC in host variables across a
decoded block is a 2-3x lever entirely in userspace. That figure is derived from
timing and has NOT been counted -- two attempts were defeated by deploys failing
for lack of memory, and counting it is the first task for whoever continues.

Delivered and verified along the way: USB deploys fixed (scripts/usb-send.py
streams through one console session, 249 KB in 12 s against the previous 370
separate logins taking about 18 minutes, 4/4 large and 4/4 small); the
interpreter converted to fixed-point after measuring doubles at 711 ns against
121 ns for integers, proven equivalent over 200M cycles and across a button
press; 40 MHz qualified on the panel with eight full-width bands and made the
driver's compiled-in default, buying 13 percent rather than the 2x hoped for,
which is itself the evidence that transfers are not clock bound; and the
emulator given EEPROM save persistence so a pet survives a restart.

Two mistakes worth recording because they cost real time. An interrupted 16 MiB
restore wiped /home down to five files, which looked like a boot loop; the
signature is a half-written jffs2 partition, and the fix is to let the restore
run to completion rather than killing it, with nothing else holding the port.
And a boot loop I initially blamed on the 40 MHz kernel was almost certainly the
interrupted flash itself -- the same clock had booted and been played for a long
session beforehand. esptool also needs --after no_reset and a modest baud when
the board is reset-looping, or it cannot sync.

#### Next Steps:

Count native instructions retired per emulated step before writing any
interpreter restructure; EMU-005 records that this is derived and unmeasured.
The decomposition benchmark exists at userspace/../TamaHarness/portable but needs
a build that fits -- the watch has about 850 KB available and a 700 KB static
binary will not run, so build it dynamically linked (575 KB) and clear the
scratch binaries out of /home first. If the count is near 250, do the
block-register interpreter for the 2-3x; if it is nearer 80, this port genuinely
tops out around 0.5x and the honest answer is the core-0 design or nothing.
After that, the parked ~1.5 s time-screen issue is the same problem seen from the
other end and needs no separate work. The watch is on the 40 MHz kernel with
spi_hz=40000000 as the compiled default, /home intact, and no emulator running.
Newest backup is ~/twatch-backup/working-2026-11-10-0023.bin, sha256 9c60af8a --
it predates the 40 MHz kernel, so restoring it returns the panel to 20 MHz and
the clock has to be reflashed afterwards.

#### Files Modified:

`.ai/core-reference.md`, `docs/tamagotchi.md`, `userspace/memlat.c`,
`scripts/usb-send.py`, `scripts/watch-deploy.sh`, `scripts/watch-exec.py`,
`patches/kernel/display/final/co5300.c`, `userspace/tbtns.c`,
`userspace/run-tama.sh`, `.gitignore`

#### Status:

- Build: PASS (kernel 40 MHz default; userspace binaries)
- Deployment: PASS (kernel flashed and verified; /home restored and intact)
- User Test: PASS (menus measured on hardware; pet hatched and playable)

---

## 17 COMMIT Unreleased 2026-11-09T23:15:52-07:00

#### Coming From:

Unreleased 8e99aba

#### Purpose:

Make the Tamagotchi emulator keep real time on the watch. It ran, and it ran at
1.00x, but any burst of guest activity fell behind by enough to stretch a screen
transition to about 1.5 s.

#### Outcome:

The interpreter's cost was in the wrong arithmetic. The SPLB32 core follows its
reference implementation in keeping cycle and timer accumulators in `double`,
and this target has no hardware double: measured on the watch, a double add is
711 ns against 121 ns for the integer equivalent, every operation a soft-float
library call. `timers_clock()` runs once per emulated instruction and touches
three such counters, so the interpreter paid that for every instruction the
guest executed. Converting the accumulators to 64-bit fixed-point at
`CYC_SHIFT = 7` halves it -- emulator time per frame at 10 fps went from 50.8 ms
to 26.4 ms. The conversion is exact rather than approximate: the sleep tick is
4000000/32768 = 122.0703125 = 15625/128, so at 7 fractional bits it is the
integer 15625 and nothing is truncated, which is what the original comments
feared. Equivalence was proven, not assumed: against the unmodified core, 200M
cycles (50 emulated seconds) agree exactly on instrs, pc, registers, ram_fnv and
dpram_fnv, and a simulated button press matches to the tenth of a millisecond
including the 580.1 ms gap the ROM leaves before redrawing. That comparison
caught a real bug: `TM0L/TM0H/TM1L/TM1H` are guest-visible as raw 16-bit cycle
counters and need the fraction shifted off, or the firmware reads a value 128x
too large -- the trace agreed for 400k instructions and diverged at the first
timer poll. Two neighbouring options are traps and are now recorded as such:
`double` is slow, and `float` is not slow but fatal, because the ESP32-S3 FPU
exists and the compiler emits `add.s`, but the kernel does not enable the
coprocessor for user tasks so the process dies with SIGSEGV; making it usable
means coprocessor context switching on a NOMMU port with a patched `fork()`.
Alongside it, the panel SPI clock became a runtime knob after the reference
firmware's 45 MHz shearing was traced to the write-pointer bug rather than clock
margin: 40 MHz was tested with eight full-width bands in eight colours and came
out clean and in order, which a desynced write pointer cannot produce, so it is
now the driver's compiled-in default. It was briefly a boot script poking sysfs,
which was backwards -- a qualified constant is a default, not a startup action --
and that script has been deleted; the module parameter remains only as an escape
hatch for a panel revision that does shear. The clock bought
13%, not the 2x it was hoped for -- a full 320x400 frame went 115.15 ms to
100.85 ms -- which says the transfer is not clock bound. Two tooling bugs cost
as much time as the engineering and are fixed: `watch-exec.py` hardcoded
`/dev/ttyACM0` and went silently dead whenever the board re-enumerated as
`ttyACM1` after a replug, and `watch-deploy.sh` lost the last chunk of every USB
transfer because `fold` emits an unterminated final line that `while read`
discards, so a 23616-character file always arrived exactly 216 short while every
chunk reported success. Touch polling went from 60 ms to 15 ms after measuring a
15-byte read at 3957 us and the DRV2605 click at 1796 us. The user-visible
result is that the watch now runs the pet at a true 1.00x with headroom the
transition can borrow from.

#### Next Steps:

The ~1.5 s wait entering the time screen is parked by user decision and runs
fine otherwise; `docs/tamagotchi.md` and EMU-002 record what is established, what
is excluded by measurement, and the one unexplained figure -- panel time of
43-135 ms per frame during a stall against 9.5 ms in the benchmark. The next
measurement, if it is picked up, is to read the per-frame stall log against a
press on the fixed-point build; the press-to-paint instrumentation is already in
`tamafb.c` behind an argv flag. The larger standing item is making the emulator a
service: `/home/run-tama.sh` retries up to 8 times because `exec` is subject to
the contiguity rule, and starting it early from rcS would avoid the lottery
entirely. Anything touching the kernel still needs `./scripts/watch-backup.sh`
first -- the 16 MiB backup from this cycle is
`~/twatch-backup/working-2026-11-09-2212.bin`, sha256 `19105568...`.

#### Files Modified:

`patches/kernel/display/final/co5300.c`, `scripts/watch-deploy.sh`,
`scripts/watch-exec.py`, `userspace/tbtns.c`, `scripts/usb-send.py`,
`userspace/run-tama.sh`, `docs/tamagotchi.md`, `.ai/core-reference.md`

#### Status:

- Build: PASS (kernel and userspace)
- Deployment: PASS (kernel flashed and hash-verified; binaries deployed)
- User Test: PASS (40 MHz bands clean; emulator runs, buttons work)

---

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

## 3 COMMIT Unreleased 2026-11-09T12:55:56-07:00

#### Coming From:

Unreleased f32fb12

#### Purpose:

Give the watch local touch input: three buttons painted in the strip below fb0, each reporting as an ordinary on/off key like the two side pushers.

#### Outcome:

The strip below the console is now red, green and blue thirds painted full width by `co5300_paint_frame()` inside its existing full-band loop, so each transfer keeps the shape the controller is known to honour, and each third reports as `BTN_0`, `BTN_1` and `BTN_2` on a uinput device named `touch-buttons`. Verified on hardware 2026-11-09: thirteen presses in the order green, red, blue, green decoded to the matching bands with a release after each, and the raw kernel stream read back from `/dev/input/event3` as `type=1 code=256 value=1`, `SYN_REPORT`, `value=0` for red, then 257 for green and 258 for blue. The input half is a userspace daemon, `userspace/tbtns.c`, not a kernel driver, and that was the deciding design call: the running kernel already provides `/dev/i2c-0`, `/dev/uinput` and the XL9555 as a gpiochip, so there is no `CONFIG_INPUT_TOUCHSCREEN`, no device-tree node, and moving a band boundary is a command-line flag instead of a rebuild and a flash. Only the painting required a kernel change, and only because the strip lies outside fb0. Getting there meant establishing the controller from scratch, since no CST9217 driver exists in 7.2.4 and the tree's only Hynitron support matches `hynitron,cst340`: the part is at I2C 0x1A and `0x5A` is the DRV2605 haptic driver, which an early single-register probe had wrongly identified as touch -- the discriminator is the CST92xx unlock handshake, not a register read. A freshly reset part sits in command mode answering transactions and reporting nothing, so it needs `0xD1=0x01`, `0xD1=0x1E` twice, byte 1 of `0x0002` reading back `0x1E`, then `0xD1=0x09`. And the freshness test that matters is the one mainline's own CST3xx driver uses: a block whose byte 0 is `0xAB` is not a report, which is why an untouched screen can otherwise present a frozen block as a phantom press. Three findings are recorded rather than smoothed over. Reads of this device on the bit-banged bus fail often -- roughly two in eight, against forty consecutive clean reads from the PCF85063A and the AXP2101 on the same pins at the same clock -- so a failed read skips the poll and lets the release timeout decide, because treating it as a release would make the buttons flicker. A very quick tap can therefore fall between polls and be missed; a deliberate press always lands, and the user confirmed the buttons work. And `tbtns` cannot pulse the controller's hardware reset, because `pca953x` owns 0x20 and a plain `I2C_SLAVE` returns `EBUSY`; the software bring-up has worked on every boot but the hardware path is unused. Two diagnostics were built rather than eyeballed: `scripts/tbtns-selftest.py` drives a `-C` classify mode on the build host to check the band arithmetic, which is the only part of this that can be wrong invisibly, and `scripts/watch-deploy.sh` wraps the netcat deploy after three separate hand-run transfer failures -- a backgrounded `nc` dying of SIGHUP, a port probe being consumed as the transfer, and `ETXTBSY` from moving a running binary. The cycle also corrected a project fact: `FB_Y` is 45, not the 46 that README and LCD-007 both stated, which the driver's own probe line `ring cols 66/387 rows 44/445` settles and which the buttons inherit by deriving from the macro. An unrelated observation is logged but not chased: esp-hosted prints `process_rx_buf: bad if_type 6` to the console every few seconds, which is what scrolled past the user during testing and briefly looked like evidence that touch was working. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 3 was validated against the template with six sections in canonical order, sequential numbering from 2, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The three buttons are functional and the user accepted them. The next improvement to this work, if it is wanted, is the missed fast tap: polling at roughly 60 ms with an unreliable ack means a quick flick can fall between samples, and the two candidate fixes are a shorter poll interval or retrying the ack until it succeeds rather than once. After that, the next capability gap remains the console repaint ceiling of about 5 fps, which has a concrete lead in LCD-005 -- fewer, larger transfers through a persistent RAMWR window. The `process_rx_buf: bad if_type 6` console spam from esp-hosted is worth a cycle of its own because it currently pollutes the display during any test. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- userspace/tbtns.c
- scripts/build-userspace.sh
- scripts/tbtns-selftest.py
- scripts/watch-deploy.sh
- .gitignore
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 4 COMMIT Unreleased 2026-11-09T13:11:15-07:00

#### Coming From:

Unreleased d786239

#### Purpose:

Fix the buttons' delayed detect, where a finger held on a bar produced one position and then silence until it was lifted.

#### Outcome:

The freeze was in this project's reader, not the hardware, and the user's suspicion -- that it was the way the sensor is read rather than how often -- was correct. `tbtns` read seven bytes of the controller's fifteen-byte report block and then acknowledged, on the reasoning that a shorter transfer is more reliable on this bus. The acknowledge that advances the controller's buffer is issued against the block, so acking after a short read left the buffer mid-block and it never advanced: the first report was taken, then nothing, then the release timeout fired while the finger was still down. The fix is the vendor sequence exactly -- read all fifteen bytes, acknowledge immediately with nothing between the read and the ack -- which is what TinyPetz's `touch.c` does and what this project had deviated from in two ways at once. Verified on hardware: holding the green bar and sliding across all three now produces a continuous coordinate stream (ctrl_x 224 to 78 to 198 to 325) with a press and release firing at each band transition instead of one press followed by silence. The correction rewrites an earlier conclusion rather than adding to it: the "2 in 8 reads succeed" failure rate recorded for this part was mostly this bug. Measured over 20 s with the full block, transfer failures are about 2 percent (6 in roughly 314 polls) with zero ack failures, and the all-0xFF blocks that dominate the census are the controller answering "nothing new" rather than a failed transfer. A second, independent defect surfaced in the same test and was fixed by loosening rather than tightening: taps landing on a bar decode to panel Y in the 441..444 range while the bar is painted from row 445, so two taps in a ten-tap run were discarded as outside the strip. The threshold is now 440, five rows of tolerance, which matches the user's original intent that these be regions rather than targets and is consistent with TinyPetz's measured 4.6 px RMS Y residual. What remains is accepted, not fixed: at ten quick taps the run produced nine readable reports and eight presses, the loss being a tap shorter than one poll interval. The user chose to keep the working behaviour rather than chase that, and the poll interval is left at the 60 ms cadence that matches the controller's own report rate. The shipped binary was confirmed byte-identical to the build that was tested by comparing its sha256 against the earlier deployment, and the band arithmetic selftest still passes. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 4 was validated against the template with six sections in canonical order, sequential numbering from 3, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The buttons are accepted as they are and no further work is owed on them. If the missed fast tap ever matters, the lever is the poll interval, which is deliberately left where it is. The next capability gap remains the console repaint ceiling of about 5 fps, with a concrete lead in LCD-005: fewer, larger transfers through a persistent RAMWR window. The esp-hosted `process_rx_buf: bad if_type 6` console spam is still unaddressed and still pollutes the display during testing, so it is the cheapest next cycle. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- userspace/tbtns.c
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 5 COMMIT Unreleased 2026-11-09T13:21:27-07:00

#### Coming From:

Unreleased fe9de87

#### Purpose:

Reserve the four edge strips around fb0 with one specification, so that the top status bar, the side app slots and the bottom buttons all read their geometry from a single agreed document instead of each inventing its own.

#### Outcome:

`docs/panel-reservations.md` now fixes the layout. The module is 45 px, which is the largest square that fits a side strip without touching fb0 and therefore costs no console columns; 48 was considered and rejected because it would have forced fb0 from 320 to 304 px and cost two of the forty columns. Measured geometry gives a 45 px left strip, a 45 px right strip, 45 rows above fb0 and 57 below it, and the strips are trapezoids rather than rectangles because the glass corners are 45-degree chamfers. The two sides are not symmetric: the left strip is full width from row 80 but the right only from row 89, because the top-right chamfer leg is 10 px longer than the top-left, so the slot grid is laid out on the intersection of the two, rows 89 to 420, and centred within it. Seven 45x45 slots per side at rows 97..411 use 315 of 332 available rows with the 17 spare split 8 above and 9 below, and every one of the fourteen slots was checked cell by cell against the chamfer inequalities and is 100 percent unclipped. The top bar was given the full width including the corners, by explicit decision, and 410 is not divisible by 45, so the nine top cells leave a 5 px orphan at x 405..409 which sits inside the top-right chamfer and is invisible anyway; wasting that space to keep one module everywhere was the deliberate trade. Cell-by-cell visibility across the top bar came out at 2, 69, 100, 100, 100, 100, 100, 61 and 1 percent, which is the concrete statement that the bar has five fully usable cells and four that are clipped, and that clipping is accepted rather than treated as a fault. The bottom strip stays the three buttons as they are, at panel columns 0..134, 137..270 and 273..409, and is deliberately not on the 45 module because it is fixed chrome rather than a slot grid. The central design point recorded is that all four edges are one operation -- write RGB565 into a named rectangle in panel coordinates, outside fb0 -- and that the bottom bar is already that operation, painted at probe time rather than on demand. Non-interference with the console is shown to be structural rather than a matter of discipline: `co5300_blit()` sets its column window to `FB_X + x0 .. FB_X + x1`, so fbcon physically cannot address a panel column outside fb0, which was checked in the source rather than assumed. The document also carries forward the three defects that any new drawing path must respect, since all three reported success while drawing nothing: pixels must be byte-swapped, a degenerate window is not honoured, and a rectangle must be one transfer. No code was written and no hardware was touched -- this cycle is a specification, so Build, Deployment and User Test do not apply. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 5 was validated against the template with six sections in canonical order, sequential numbering from 4, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The specification needs an implementation, and it deliberately leaves the API shape open: ioctl, sysfs node or a small character device has to be settled first because it fixes the userspace contract, and the top bar's drawing should be a daemon for consistency with `blekbd` and `tbtns`. Whatever is chosen must begin with a structured test pattern -- distinct colours per slot and a different colour in each corner of each slot -- because a flat fill proves nothing about addressing on a write-only panel, and this project has already had three drawing paths report success while drawing nothing. The bottom bar's press and release artwork is the last consumer to wire up, once the press path can repaint a region. The esp-hosted `process_rx_buf: bad if_type 6` console spam remains open and is still the cheapest next cycle because it pollutes the display during testing. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- docs/panel-reservations.md
- README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 6 COMMIT Unreleased 2026-11-09T13:23:48-07:00

#### Coming From:

Unreleased 8134396

#### Purpose:

Decide how userspace writes pixels into the reserved edge strips, closing the one question the geometry specification deliberately left open.

#### Outcome:

`docs/panel-api.md` settles the calling convention: one ioctl on `/dev/fb0`, not a new character device, sysfs or debugfs. The deciding reason is not elegance but `FLS-003` -- a new device node needs an mdev rule, and mdev rules are precisely the `/etc` state that a full-image flash destroys, so a node would have to be recreated by hand after every reflash while `/dev/fb0` already exists and is already openable. It is also the display's own interface rather than a third device that happens to share a panel, and the fbdev core forwards unrecognised ioctls to the driver, which was checked in the tree at `fb_chrdev.c:148` rather than assumed. The ioctl magic `'C'` was confirmed unused in this kernel, as was `'c'`, so there is no collision with another built-in driver. Two commands are defined: `CO5300_IOC_WRITE_RECT` taking panel coordinates, a size, a row stride and an RGB565 buffer, and `CO5300_IOC_GET_GEOM` returning the panel and fb0 rectangles so no userspace program hardcodes 45 or 410, which doubles as a feature test because an older kernel returns `-ENOTTY`. Four semantics are load-bearing rather than conventional. The driver rejects any rectangle intersecting fb0, which puts the "edges never disturb the console" guarantee in the driver instead of in a convention -- a userspace bug cannot paint over the console, and the check is four comparisons. Pixels are passed in host byte order and byte-swapped by the driver, because requiring callers to pre-swap has already produced a silent bug here. Degenerate rectangles return `-EINVAL` instead of drawing nothing, since `LCD-002` established that a window one pixel wide or tall reports success. And each call is one window and one transfer, split into bands by the driver if it exceeds the transmit buffer, which is the only proven way to cover more than one buffer. The document also specifies that the driver gains one mutex taken at every panel entry point -- the four fbcon ops and the ioctl -- because fbcon serialises itself with `console_lock` but that lock does not exclude a userspace ioctl, while the ioctl deliberately does not take `console_lock` to avoid a deadlock class for no added safety. Transfer costs were derived from the measured rate in `LCD-005` rather than estimated: 742 ns per byte gives about 3 ms for a 45x45 tile, 29 ms for a full-width 45-row band and 37 ms for the whole bottom strip, and two earlier estimates in the draft were corrected against that derivation. The chief implementation risk is stated rather than glossed: a 45x45 window shape is untested, since `LCD-002` only established that 1 px fails and 432x31 works, and nothing has established where between those the limit lies. Two fallbacks are recorded, full-width band composition from `p->fb` if narrow windows misbehave, and restricting to the top and bottom strips where no fb0 rows are involved. The cycle also closed the loop with the geometry specification, whose "Not decided here" section previously listed the API shape as open; it now records the decision and points at the new document, so the two specs do not disagree. No code was written and no hardware was touched: the watch was unplugged from USB for this cycle, running on battery, which is why no build or deployment stage applies. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 6 was validated against the template with six sections in canonical order, sequential numbering from 5, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

Implementation begins with the mutex and the two ioctls in `co5300.c`, which changes no fbcon behaviour, and the gate for everything after it is a diagnostic that paints a corner-marked pattern into every reserved region so the 45x45 window question is answered by looking at the glass. Argument validation -- bounds, fb0 overlap, degenerate size, stride -- is pure arithmetic and should be tested on the build host beside `scripts/tbtns-selftest.py` so a boundary error does not cost a flash. After that: a top-bar status daemon reading `/sys/class/power_supply` and the WiFi state, slot rendering driven by a config on `/home`, and finally button press and release art extending `tbtns`. Still open from the geometry document: where slot art lives and whether the partial top cells 1 and 7 are offered at all. The esp-hosted `process_rx_buf: bad if_type 6` console spam remains unaddressed and still pollutes the display during testing. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- docs/panel-api.md
- docs/panel-reservations.md
- README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 7 COMMIT Unreleased 2026-11-09T14:06:40-07:00

#### Coming From:

Unreleased 0d25388

#### Purpose:

Implement the panel API decided in docs/panel-api.md and prove on the glass that it draws where it says.

#### Outcome:

`co5300.c` now carries the mutex and the two ioctls from the specification, and the four reserved regions were painted and read back on the hardware. The panel mutex is taken at all five entry points -- fillrect, copyarea, imageblit, write and the ioctl -- so fbcon and userspace cannot interleave a window setup with another window's pixel stream, and the ioctl deliberately does not take console_lock to avoid inverting the fbcon lock order. `CO5300_IOC_GET_GEOM` answers from the running kernel with `panel 410x502, fb0 320x400 at (45,45), offset 22, abi 1`, so no userspace program has to hardcode geometry, and it doubles as a feature test because an older kernel returns ENOTTY. `CO5300_IOC_WRITE_RECT` takes panel coordinates and host-order RGB565 and byte-swaps in the driver. The argument validation was moved out into `co5300_rect.h` so it can be exercised on the build host, and `scripts/co5300-selftest.py` now drives 43 cases -- legal tiles and strips, degenerate sizes, reserved and flags misuse, a stride smaller than the width, out-of-panel bounds, and the fb0 overlap guard including the exact boundary rows and columns -- with all 14 side slots confirmed accepted; `build-kernel.sh` runs it as a build gate. The chief risk carried into this cycle was resolved in the useful direction: 45x45 windows are honoured, as are 45x315, 410x45 and 410x57, so the full-width band fallback in the spec is not needed. One defect in this cycle was the diagnostic rather than the driver, and it is worth recording because it produced a confident wrong answer: the test pattern's white markers were written as `RGB565(31, 31, 31)`, and in RGB565 green is a 6-bit field, so 31 is half brightness and the result is `0xFBFF` -- pink. The user reported all four edges pink and a pink sliver under the green bar, which read as a pixel-path fault; the arithmetic showed the API had been correct throughout and the tool had never drawn white. Colours are now named constants with a comment on the field widths, and a solid-primary mode exists so a region's colour can be read without interpreting stripes. A second defect was genuine but is in the test harness, not the driver: `panel-test -R` and `-1` segfaulted intermittently, roughly half of runs, with no output and no kernel oops in dmesg. It was bisected against rect size -- 45x45, 45x315, 410x45, 410x57 each ran 10 to 15 times with no failure, and the crashing modes then ran 24 consecutive times with no failure on the current build -- so it could not be reproduced and its cause is not established. It is recorded as an open, unreproduced fault rather than closed. The paint path itself is proven: the four regions land at the coordinates asked with fb0 untouched. Two operational facts surfaced. The esp-hosted link dropped mid-cycle with `process_rx_buf: bad if_type 6` and would not recover by restarting networking -- `ping` to its own gateway failed with the interface still addressed -- and only a reboot restored it; `scripts/watch-deploy.sh` gained a USB transport for exactly that case, sending chunked base64 over the console, because netcat is not available on that channel at all. The build was flashed to 0x140000 after a full 16 MiB backup, and the kernel booted with the console, PMU and RTC intact. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 7 was validated against the template with six sections in canonical order, sequential numbering from 6, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The API is implemented and proven, so the consumers can be built. The next cycle is the top-bar status daemon: read `/sys/class/power_supply` and the WiFi state and draw into the five fully usable cells with `CO5300_IOC_WRITE_RECT`, which needs no further kernel work. Two facts must be respected from that work: each strip is painted in two layers, a background over rows 89..444 and tile art over the 45x45 slots, or unpainted margins appear; and every colour constant must be checked against RGB565's field widths before a test result means anything. The intermittent segfault in `panel-test` is still open and unreproduced and should be chased before the harness is trusted for anything subtle. The esp-hosted `process_rx_buf: bad if_type 6` fault is now clearly worth its own cycle rather than being a nuisance: it drops the network hard enough that only a reboot recovers it, which costs a deploy channel mid-work. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- patches/kernel/display/final/co5300_rect.h
- userspace/panel-test.c
- scripts/co5300-selftest.py
- scripts/build-kernel.sh
- scripts/build-userspace.sh
- scripts/watch-deploy.sh
- docs/panel-api.md
- docs/panel-reservations.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 8 COMMIT Unreleased 2026-11-09T14:46:17-07:00

#### Coming From:

Unreleased 2cb6ffd

#### Purpose:

Diagnose the diagonal tearing the user reported in the right-hand strip and make panel writes land correctly there.

#### Outcome:

The tearing is real, is not residue, and is not explained by any geometric property of the rectangle. It is explained, empirically, by how much one transfer carries: `CO5300_IOC_WRITE_RECT` now sends every rectangle in bands of two rows, and the right strip draws cleanly. Diagnosing it took six test patterns, each varying one thing, and the two hypotheses carried in were both wrong. The first was width: a 45-wide window was thought to be near some limit, and the observation that 45x45 drew earlier made that unlikely, correctly. The second was X position, on the theory that controller columns 387..431 reach past the controller's RAM boundary inferred from `PANEL_OFFSET_X = 22`. Both were excluded by measurement rather than argument. Width is excluded because severity is non-monotonic: five stacked segments from the same origin gave 45 wide torn, 35 wide clean, and 25, 15 and 5 wide all torn. X position is excluded because nine 45x50 windows swept across the bottom strip at x = 0, 45, ... 360 were all clean, including one at controller columns 382..426 which overlaps the right strip almost entirely. Height is excluded because 45x71 segments tore too. Residue is excluded because the splits are visible in a single uniform fill, which cannot be produced by overlapping partial writes. What the measurements do show is that a single 45x356 transfer into the right strip returns as eight diagonal splits worst at the bottom, while the same rectangle as 178 windows of 45x2 is clean, repeatably, and that the left strip tolerates the single tall transfer the right does not. After the driver change, userspace sending the same single rectangle produced a clean uniform strip, so the cap is invisible to callers. Two things are recorded rather than glossed. The root cause is NOT established: this is a workaround that has held every time it was tried, not an explanation, and it is acceptable only because these are status icons that change on battery or link state -- it would be wrong for continuous drawing, which is why `co5300_blit()` keeps its own uncapped sizing. And the strongest remaining suspect is SPI clock margin, because the reference firmware recorded the identical signature at 45 MHz, "dark horizontal lines shearing slightly down to the right", cured by dropping to 20 MHz, while this project runs at 20 MHz and the signature persists for long narrow transfers. A separate defect in the diagnostic tool was found and fixed during the work: several test patterns painted partial rectangles of different colours into the same region, so anything landing inexactly left a neighbour's colour beside it and read as a tear. That produced two false reports from the user -- "all four sides pink" and "eight tears in a solid fill" were partly artefacts of the tool -- and the lesson recorded is that a diagnostic must repaint its whole region in one operation before its output is trusted. A second defect is open and unresolved: `panel-test -R` and `-1` segfaulted intermittently, and after bisecting rect size ten to fifteen times each with no failure, the modes ran 24 consecutive times clean, so it is unreproduced and its cause is not established. The build was flashed to 0x140000 after a full 16 MiB backup, the vectors address was verified at 0x40380000 before writing, and the kernel booted with the console, PMU and RTC intact. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 8 was validated against the template with six sections in canonical order, sequential numbering from 7, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The panel API is usable as specified: callers hand over one rectangle and the driver bands it. The next cycle is the top-bar status daemon, reading `/sys/class/power_supply` and the WiFi state and drawing into the five fully usable cells, which needs no further kernel work. Two experiments are worth their cost before the tearing is declared closed. First, drop the SPI clock to 10 MHz and repeat the single tall 45x356 transfer: if it comes back clean, the fault is signal margin rather than anything in the addressing, and the driver could then band less aggressively at a lower clock or keep the cap and raise it. Second, if that does not explain it, capture the tearing under a faster scope of instrument -- a full-width reference bar beside the strip in the same frame would show whether rows are lost or shifted. The intermittent `panel-test` segfault remains open and unreproduced. The esp-hosted `process_rx_buf: bad if_type 6` fault still drops the network hard enough to need a reboot and is still the cheapest general cycle. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- userspace/panel-test.c
- docs/panel-api.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 9 COMMIT Unreleased 2026-11-09T15:35:14-07:00

#### Coming From:

Unreleased 38f2f36

#### Purpose:

Answer whether the display path or the CPU limits the panel, for another project that needs the number to decide whether to port to this board.

#### Outcome:

A bench was added to the driver, `CO5300_IOC_BENCH`, which times the same pixel payload twice -- once with CS asserted so the panel receives it, and once with CS never asserted so the peripheral clocks the bytes out into nothing. The difference is the panel's contribution and the rest is the driver and CPU. Measured on hardware: 288x2 (18 chunks of 64 bytes) costs 197 us with CS and 193 us without, and 288x64 (576 chunks) costs 6572 us and 6470 us. Fitting those two points gives 11.42 us per 64-byte chunk and a per-transfer fixed cost of -8.6 us, which is zero within noise. The panel accounts for 0.18 to 0.22 us per chunk, about 1.6 percent. Throughput is 5.6 MB/s against 10 MB/s of quad-lane wire capacity at 20 MHz. Three conclusions follow and they are the reason the cycle was worth running. Per-transfer overhead is negligible, so a dirty-rectangle scheme pays no per-call penalty and splitting a frame into many small transfers is cheap in fixed cost -- which reverses the concern that a 2-row cap would multiply overhead by transfer count. The panel is excluded as a bottleneck outright: removing it changes the time by 1.6 percent. And the remaining 5 us per chunk is CPU work, 16 memcpy_toio writes into a 64-byte FIFO plus a length write and a cpu_relax() completion poll, so DMA caps the gain at about 2x rather than the 5-10x a FIFO-share estimate suggested. One contradiction is recorded rather than resolved: LCD-005 puts a 320x400 frame at about 190 ms while 4000 chunks at 11.42 us is 45.7 ms, a factor of 4.2, and nothing yet explains the difference -- LCD-005 predates the CS-handling fix and may have included work outside the transfer, so it should be re-measured through the real repaint path before either figure is quoted. The cycle also produced a lesson about the instrument. Bench runs that block leave the calling shell unable to execute anything, and on this NOMMU kernel the blocked process cannot be interrupted with Ctrl-C, so only a chip reset clears it; two such wedges cost several resets and a great deal of time before the pattern was recognised, and the fix is to launch a bench with nohup in the background so it can never hold the shell. During the work a genuine bug in the test tool was found and fixed: the getopt string listed `b` without a colon while `r` had one, so `-b` received no argument, `sscanf` was handed NULL and the tool died with SIGSEGV and no output. That is almost certainly the intermittent crash recorded as unreproduced in entries 7 and 8 -- same signature, same absence of a kernel oops, and the reason it correlated with newly added modes rather than with rect size. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 9 was validated against the template with six sections in canonical order, sequential numbering from 8, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The bench is a permanent diagnostic and `docs/panel-transfer-cost.md` records how to run it safely. Two follow-ups are worth their cost. First, re-measure the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005; until that is done the practical frame rate of the console is unknown and both published figures are suspect. Second, with per-transfer cost shown to be zero, the 2-row cap on `CO5300_IOC_WRITE_RECT` can be revisited on transfer-time grounds alone: the cap costs almost nothing in overhead, so it stays for correctness rather than cost, and the tearing root cause from LCD-009 is still open with SPI clock the remaining suspect. The next feature cycle is the top-bar status daemon, reading `/sys/class/power_supply` and the WiFi state, which needs no further kernel work. The esp-hosted `process_rx_buf: bad if_type 6` fault wedged the network three times in this session and each time only a chip reset cleared it, so it is now the most costly open defect in the project and should be scheduled. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/display/final/co5300.c
- userspace/panel-test.c
- docs/panel-transfer-cost.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 10 COMMIT Unreleased 2026-11-09T16:53:31-07:00

#### Coming From:

Unreleased fb9ad02

#### Purpose:

Fix the `process_rx_buf: bad if_type 6` console spam and restore the firmware diagnostic channel it was hiding.

#### Outcome:

The cause was an interface enum that had drifted between firmware and kernel. esp-hosted tags every payload with an `if_type` byte and both sides test it against `ESP_MAX_IF`; upstream has six values and this project's firmware fork added a seventh, `ESP_LOG_IF`, for its own diagnostic stream, so the firmware's `ESP_MAX_IF` became 7 while the kernel's stayed 6. Every log frame therefore arrived tagged 6, failed `if_type >= ESP_MAX_IF` in `process_rx_buf()`, and was dropped -- which is the message that had been scrolling the console for days. The frames were not merely rejected, they had nowhere to go: `esp_log_pipe.c` was carried in `patches/kernel/display/final/` but `build-kernel.sh` never installed or compiled it, so `/dev/esp-log` did not exist on this board at all. `patches/kernel/0007` fixes both together, deliberately: the enum entry, the dispatch to `esp_log_pipe_rx()`, the init and deinit calls and the Makefile object, because accepting `ESP_LOG_IF` without a consumer would leave those skbs queued in `rx_q` forever, which is worse than dropping them. Verified on hardware: `/dev/esp-log` now exists as character device 10,257 and delivers real firmware lines such as `@LOG hid: scan 1 complete (connected 0)`, while `bad if_type` errors are zero through the first minutes of uptime against hundreds per boot before the change. The log stream is sparse and that is correct -- the firmware emitter is rate limited to 40 lines per second because, in its own words, a flood of these frames competes for the same shmem link the kernel is booting over. PMU, RTC, local time and WiFi were checked afterwards and are unaffected. Two additional defects surfaced while making the build honest. The escapement patch loop used to print `FAILED to apply` and carry on, which is the silent-loss pattern of TOL-003; it now dies, and on the first clean build that immediately caught two patches that had been doing nothing. One targets `new-files/.../devkit_c1_16m_linux.config`, a file in the port repository rather than the kernel tree, so it is now skipped explicitly as the port's to apply. The other, `0006-dts-side-buttons.patch`, had never applied and never needed to: every element it adds -- `gpio-keys`, `pusher_b_pins`, `pmic_irq_pins`, `KEY_ENTER` and the PMU interrupt -- is already present in the whole-file DTS that `build-kernel.sh` installs, checked one element at a time before it was deleted, and keeping a patch that can never apply is a trap for the next person. What is explicitly not claimed: that this fixes the wedge. The firmware's own source records that a flood of these frames "has already been shown to stop Linux reaching userspace", and that is consistent with what was observed -- ICMP answering throughout while a telnet login stalled, and the IPC ring advancing its read pointer even on a rejected frame so errors could not have stalled it. But that is a hypothesis supported by the evidence, not a demonstrated mechanism, and if the wedge recurs the next instrument is per-interface packet counts in the ring. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 10 was validated against the template with six sections in canonical order, sequential numbering from 9, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The firmware diagnostic channel is now a tool rather than a gap, and it should be used before anything else in the deferred list: `cat /dev/esp-log` shows what the BLE host and the shmem link are doing after handoff, which is exactly the period that was previously undiagnosable, and it is already reporting that the keyboard scans repeatedly and connects to nothing. That makes the accepted `sm_bonding` defect -- pairing needed after every reboot -- the obvious next cycle, because the log can now show whether the load path runs at all. The wedge needs a stability test across several hours of ordinary telnet use before it is called fixed; if it recurs, add per-interface counters where the ring is consumed. The tearing root cause in LCD-009 is still open with SPI clock the remaining suspect, and re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding. The top-bar status daemon has not been started. BLE media keys, firmware-side bond persistence and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- patches/kernel/0007-wireless-esp32-ng-add-ESP_LOG_IF-and-log-pipe.patch
- patches/kernel/0006-dts-side-buttons.patch
- scripts/build-kernel.sh
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 11 COMMIT Unreleased 2026-11-09T17:11:25-07:00

#### Coming From:

Unreleased 6d3c8b3

#### Purpose:

Make the new Rii K06 combo keyboard type on the watch, replacing the Logitech it was developed against.

#### Outcome:

The keyboard now pairs, encrypts and types, and the defect that stopped it was a subscription rule rather than pairing. The K06 exposes the Boot Keyboard Input characteristic 0x2A22, so the firmware set its boot-input flag -- and nothing ever notifies there. Its boot-format reports arrive on report characteristic 0x001E instead, exactly as TinyTang's BLE-019 records for this same unit. The host's rule was to subscribe only to the boot input whenever one existed, on the reasoning that one CCCD is cheaper than six; that rule is wrong for a device which has the characteristic and never uses it, and it fails silently, because the link comes up and only the keys are missing. The fix follows TinyTang's own implementation, which subscribes to the boot input and every notifying 0x2A4D report together "so reports are seen whichever way the device actually sends them". Verified on hardware with the K06 in LE pairing mode: the log shows connect (0), security initiate (0), protocol mode -> boot (0), and subscriptions to handles 0x0015, 0x001a, 0x001e, 0x0025 and 0x002c where the old firmware subscribed to 0x0015 alone, and blekbd then decoded 8-byte boot reports into KEY_Y and KEY_F which the operator confirmed appear on the AMOLED console. The claim in BLE-003 that boot protocol costs "one subscription instead of six" was wrong and is corrected there, along with the fear that six subscriptions would exhaust the mbuf pool: that was six concurrent CCCD writes, and subscribe_next issues one and resumes from its completion callback. A second defect was introduced and caught in this same cycle, and it is recorded because it is a property of the board rather than a slip. To find out whether the K06 was advertising at all, a log line was added to the scan callback for every advertisement. That stopped the network interface coming up: fw_log sends each line as an IPC message over the same shmem link WiFi initialises over, the kernel reported Command[1] timed out and esp_add_network_ifaces failure, and there was no espsta0 and no /dev/esp-log, because the log pipe driver initialises after the step that failed. The failure was self-concealing -- the diagnostic channel died with the network, so the log could not report what killed it. It reproduced on two consecutive boots and was reverted before anything else was tried; the same firmware without the logging restored both immediately. On this board logging is traffic, and the emitter's 40 lines per second is a budget rather than a guarantee. The firmware change is committed locally in build/esp-hosted/esp_hosted_ng, which is a fork of upstream jcmvbkbc/esp-hosted and must never be pushed; the remote is untouched. README was also corrected where it claimed re-pairing was automatic: with sm_bonding enabled and the load path unfinished, the keyboard has to be put into pairing mode by hand after each reboot, which is a regression against the earlier silent re-pair and is now stated rather than glossed. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 11 was validated against the template with six sections in canonical order, sequential numbering from 10, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The bond is the obvious next cycle and it is now the only thing standing between this keyboard and being pleasant to use. `blekbd` reports `bond saved (332 bytes)` while the firmware's `bond_load()` is the unfinished half, so the key it stores is never restored and the keyboard must be re-paired by hand after every reboot -- a regression against the previous behaviour, not the original design. TinyTang has a working implementation for this exact problem on comparable silicon, `ports/bl616/tang_ble_bonds.c`: a versioned file with a magic, per-slot records covering the address, key size, flags, LTK and its rand/ediv, and a CRC-32 over the whole thing, which the firmware reads at boot and puts into the host's key table before devices reconnect. That is the reference to follow rather than a design from scratch, and documenting the Escapement equivalent in docs/bluetooth-bond.md as it is built is cheaper than reconstructing it later. Two smaller items are worth clearing first because they are cheap: the K06 also carries a touchpad, which its report map declares as report ID 3 with X, Y and wheel, and the consumer and system collections that would give media keys are declared too -- both are reachable now that every notifying report is subscribed, but neither is decoded. Separately, the tearing root cause in LCD-009 remains open with SPI clock the remaining suspect, and re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding. BLE media keys, firmware-side bond persistence itself and any wake-from-sleep path remain deferred by user decision.

#### Files Modified:

- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 12 COMMIT Unreleased 2026-11-09T17:34:21-07:00

#### Coming From:

Unreleased b4600dc

#### Purpose:

Make the Rii K06 reconnect on its stored bond after a reboot, without the user putting it into pairing mode.

#### Outcome:

The bond now loads and the keyboard reconnects unattended, and the three defects that prevented it had been hiding each other for several sessions. The first was observability, exactly as `docs/bluetooth-bond.md` predicted: `bond_load()` runs from the NimBLE host-sync callback, before Linux starts, so every line it logged was dropped and there was no evidence whether the file had been read, rejected, or never opened. `fw_log_defer()` and `fw_log_flush()` now hold boot-time lines in a small fixed pool and release them when Linux first speaks on the pipe; the first flush printed the cause immediately, as `bond: no room to read etc (458752 bytes)`. The second was PSRAM. `map_bond_partition()` asked for `heap_caps_malloc(part->size, MALLOC_CAP_SPIRAM)`, and this firmware sets `CONFIG_SPIRAM_USE_MEMMAP`, which maps PSRAM as address space for the Linux kernel instead of registering it as a heap region -- so no allocation on this board can carry that capability and the call could never succeed. `bond_load()` therefore returned before it opened the file, which is why every earlier attempt to fix the load logic was revising code that never ran. It now allocates internal RAM and reads a bounded prefix, 16 KiB doubling until the file is found, rather than the whole 448 KiB partition that never fits and does not need to. The third was in the jffs2 reader: the node walk bounds-checked the header but then trusted `node.totlen`, so a truncated image read past the buffer. That was latent while the image was always whole, but bounded reads make a partial trailing node normal, so the check had to be added with them. Verified on hardware with the full cycle: flash, reboot, `bond: loaded 4 entries from ble-bond.bin (0 rejected)`, then a keypress with the keyboard NOT in pairing mode, after which it reconnected on the stored key and typed, with KEY_G decoded by blekbd. Two documentation errors were corrected in the same pass. README claimed re-pairing was automatic, and separately listed the bond as not done; both are now stated as working, with the forced split explained -- the firmware cannot write flash after Linux starts, so Linux writes the file and the firmware only reads it. And the media-keys limitation was stale: since the host now subscribes to every notifying input report, the consumer-page reports carrying volume and browser keys do reach Linux and blekbd has a consumer usage map for them, so what is actually missing is the report map, whose reader `map_step()` is never called, leaving the daemon on the boot-layout fallback. That is stated as untested rather than either working or absent. The firmware change is committed locally in build/esp-hosted/esp_hosted_ng, whose remote is upstream jcmvbkbc/esp-hosted and must never be pushed. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 12 was validated against the template with six sections in canonical order, sequential numbering from 11, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The Rii K06 is now working as intended: it pairs, types, and reconnects after a reboot on its stored bond. The next thing the user asked for is the same treatment for the Logitech keyboard, which should be mostly a matter of checking it against the changed subscription rule and confirming both devices can hold a bond at once -- `ENTRY_MAX` is 8 and the K06 uses 4, so a second device needs the count checked before anything is assumed. Two capabilities are now reachable and unbuilt: the K06's touchpad, which its report map declares as report ID 3 with X, Y and wheel, and media keys, which need the report map read -- `map_step()` exists but is not called -- chunked and serialised because that read is what exhausted the ATT buffers once before. Beyond Bluetooth, the tearing root cause in LCD-009 remains open with SPI clock the remaining suspect, and re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding. The top-bar status daemon has not been started.

#### Files Modified:

- docs/bluetooth-bond.md
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 13 COMMIT Unreleased 2026-11-09T17:51:08-07:00

#### Coming From:

Unreleased 7ac454d

#### Purpose:

Check whether the three touch buttons still work, after several firmware and kernel cycles had rebooted past the point where they were last tested.

#### Outcome:

They did not work, and the reason was not the driver. The daemon had only ever been started by hand with nohup during the cycle that built it, so nothing brought it back at boot: after the reboots of the last few cycles there was no `tbtns` process and no `touch-buttons` device. `userspace/S50tbtns` now installs to `/etc/init.d` and is started by rcS alongside the keyboard daemon, deployed with a sha256 check; `/etc` is jffs2 so it survives a reboot, and only a full-image flash removes it. With the service running, the buttons were then verified twice over. The daemon log recorded three presses in the order they were made with a matching release after each, decoding to bands 0, 1 and 2. And a raw read of `/dev/input/event3` during a second round of presses carried 192 bytes decoding to `type=1 code=256 value=1`, `SYN_REPORT`, `value=0`, then 257, then 258 -- so the events reach the kernel's input layer and not merely the daemon's log, on the firmware built earlier today. `/proc/bus/input/devices` reports `EV=3`, which is EV_SYN plus EV_KEY and nothing else: three on/off switches, no pointer, which is what these are meant to be. Two characteristics of the touch part are worth restating because they are visible in the log during any of this: `i2c i2c-0: sendbytes: NAK bailout` appears as the CST9217 intermittently refuses a read, which the daemon tolerates by retrying and by never treating a failed read as a release; and the press path has a known ceiling where a tap shorter than one poll interval can fall between samples, already recorded. Neither showed up in this test: three presses, three presses registered. No code changed in this cycle beyond the init script, so there was nothing to build. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 13 was validated against the template with six sections in canonical order, sequential numbering from 12, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

Nothing consumes the three buttons yet: they emit BTN_0, BTN_1 and BTN_2 and no daemon acts on them, so pressing them changes nothing on the glass. Wiring them to something is the obvious next use of them, and pressing should eventually repaint the bar with its pressed image, which needs the panel region write path already built for `CO5300_IOC_WRITE_RECT`. The user has designated the Rii K06 as the official input device and deferred both its touchpad and the Logitech keyboard, so those stay parked. One piece of housekeeping is worth doing before the next full-image flash: three things now have to be re-installed by hand after one -- `/etc/TZ`, `S01hwclock` and now `S50tbtns` -- plus the deployed binaries on `/home`, and consolidating them behind one installer would remove a step people forget. Beyond that, the tearing root cause in LCD-009 is still open with SPI clock the remaining suspect, re-measuring the 320x400 frame through the real repaint path to settle the 4.2x disagreement with LCD-005 is still outstanding, and the top-bar status daemon has not been started.

#### Files Modified:

- userspace/S50tbtns
- README.md

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 14 COMMIT Unreleased 2026-11-09T19:08:00-07:00

#### Coming From:

Unreleased 97aafcc

#### Purpose:

Put the Tamagotchi V3 ROM in flash so the emulator starts at any uptime instead of only on a fresh boot.

#### Outcome:

The ROM now lives raw in flash at 0x480000 and is read from userspace through the kernel's linear flash window at 0x42480000, costing no RAM at all. The problem it solves is KRN-001: the emulator's ROM is 512 KB and NOMMU needs it contiguous, so a malloc succeeded on a fresh boot and failed once the heap fragmented, with an exec-time allocator error the program could not report -- the pet could be started exactly once per boot. Three candidate routes were tested rather than assumed. File mmap works but costs 1020 KB, twice the file, because NOMMU copies the mapping; MAP_SHARED fails with ENODEV. The linear flash window is readable from userspace and was verified by checksumming a 64 KB range through it. The third route was the answer, and it is recorded as EXT-002 because it is a property of this port rather than of the emulator. The ROM was extracted from rom_embed.c, checked against the sha256 that file declares, written with esptool, and then verified independently by reading it back through the window and comparing CRCs -- flash crc32 301F0E87 against file crc32 301F0E87 -- rather than trusting esptool's own hash. A guard was added to scripts/build-kernel.sh so the kernel can never grow into the slot: an xipImage that would reach 0x480000 is refused, because otherwise a larger kernel would be silently overwritten by the next ROM write and the pet would be subtly wrong rather than obviously broken. The guard was checked against four sizes including exactly-at-limit and one-byte-over. The front end needed three changes, all in TamaHarness's portable/tamafb.c and documented in docs/tamagotchi.md: ROM_FLASH_ADDR selects the flash ROM and skips the allocation; FB_W and FB_H became overridable because the defaults are SEG_W+32 by SEG_H+48, which is 324x348 and larger than fb0, so the program refused to draw; and the mmap write path was replaced with banded pwrite() because the CO5300 driver implements no mmap operation. Static, and deliberately without -DEMBED_ROM, which would compile the ROM into .bss and reintroduce the same contiguity problem -- bss is 35 KB this way and would be 547 KB otherwise. A second defect was found and fixed while getting the picture on the glass, and it was not the emulator: every printk mirrors to all consoles and fbcon is one of them, so each kernel message repainted console text over the frame and the next frame repainted back, which the operator saw as flicker between the console and the game. The trigger was constant because the touch daemon polls the CST9217 every 60 ms, the controller NAKs, and i2c-gpio prints a line per NAK at KERN_ERR. /etc/sysctl.conf now sets kernel.printk = 1 4 1 7 and this is recorded as EXT-003; dropping to 3 first did not help, which is how the message level was identified. Verified on hardware: the egg renders and wiggles, the operator confirmed it on the glass, and the emulator ran at 1.00x realtime with the largest free block down to 128 KB -- 16 frames in 4 s at 3833499 emulated cycles/s -- where the malloc build cannot start at all. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 14 was validated against the template with six sections in canonical order, sequential numbering from 13, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The emulator runs but nothing controls it: the three touch bars emit BTN_0, BTN_1 and BTN_2 and nothing consumes them, and splb32_pin(cpu, 0, mask, level) with PA bits 0x01/0x02/0x04 is the hook -- tamagui.c already drives exactly that from a keyboard, so the mapping is known and the work is reading event3 and holding the state across frames. After that it should become a service, S51tamafb, which the flash ROM now makes practical at any uptime; that is also the point at which the console loglevel should be raised and lowered by the app itself rather than left at emergencies-only for the whole session. The pwrite cost is still not separated from the render cost: LCD-005's ~190 ms per frame conflicts with LCD-010's 45.7 ms projection by 4.2x, and instrumenting render_band against the pwrite would settle which dominates before any frame-rate work is attempted. Beyond the emulator: the tearing root cause in LCD-009 is still open with SPI clock the remaining suspect, the top-bar status daemon has not been started, and the K06's touchpad and media keys remain reachable but unbuilt. The Logitech keyboard is parked by user decision now that the Rii K06 is the official input device.

#### Files Modified:

- scripts/build-kernel.sh
- docs/tamagotchi.md
- README.md
- .gitignore

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 15 COMMIT Unreleased 2026-11-09T19:17:57-07:00

#### Coming From:

Unreleased 354e0ea

#### Purpose:

Give the three touch buttons a click, so a tap feels like a button rather than glass.

#### Outcome:

The DRV2605 haptic amplifier at I2C 0x5A now plays a Strong Click on every button press, and the operator confirmed it is felt. It was approached by reading the chip first rather than by following TinyPetz, which turned out to be the right order: TinyPetz enables the haptic rail at boot and never writes to the DRV2605 at all, so the vibration noted there was the chip's own power-on behaviour and gave no register sequence to copy. Three reads off this unit settled everything before a line was written -- STATUS 0x00 read 0xE0, whose DEVICE_ID bits 7:5 are 0b111 for a DRV2605 with DIAG_RESULT clear; FEEDBACK 0x1A read 0x36, whose bit 7 is 0 and therefore means an ERM motor rather than an LRA, which decides the effect library since 6 is LRA and 1 to 4 are ERM; and the XL9555 port 0 output read 0xff, confirming the expander bit that gates the motor's power is already high. Two things about this part fail silently and both are recorded as BRD-005: a chip whose expander bit is low answers every I2C transaction and never moves, and a library mismatched to the motor type buzzes weakly or not at all instead of erroring. The driver is userspace/haptic.c with haptic_init(), haptic_tap() and haptic_close(), kept separate from tbtns because the same three calls serve anything on this board wanting a click; it verifies the device ID before writing anything, so it refuses rather than sending effect IDs to a stranger at that address. The click is fired on press only and never on release, because a click on both ends of every tap reads as two events and feels like noise rather than feedback; the operator accepted it as shipped. On the board the daemon reports `haptic ready (DRV2605, ERM)`, and the failure path is non-fatal by design -- a board without the part, or a rail that never came up, logs `no haptic -- buttons will be silent` and the buttons keep working. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 15 was validated against the template with six sections in canonical order, sequential numbering from 14, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The buttons now press, emit BTN_0/1/2 and click, and nothing consumes the events, so wiring them to something is the next use of them. The obvious consumer is the Tamagotchi, whose core takes input through splb32_pin(cpu, 0, mask, level) on PA bits 0x01, 0x02 and 0x04 with level 1 for press and -1 for release -- tamagui.c already drives exactly that from a keyboard, so the mapping is known and the work is reading /dev/input/event3 each frame and holding the state across frames. A second consumer worth having is pressed and released imagery on the bars themselves, which needs nothing new: CO5300_IOC_WRITE_RECT already writes panel rectangles and the driver paints the bars, so a pressed state is a rectangle repaint. Two larger items remain open from earlier cycles: the pwrite-versus-render split for the emulator is still unmeasured, and LCD-009's tearing root cause is unresolved with SPI clock the remaining suspect. The top-bar status daemon has not been started. The Logitech keyboard, the K06's touchpad and BLE media keys are parked by user decision.

#### Files Modified:

- userspace/haptic.c
- userspace/haptic.h
- userspace/tbtns.c
- scripts/build-userspace.sh
- README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 16 COMMIT Unreleased 2026-11-09T19:31:21-07:00

#### Coming From:

Unreleased 3964fc8

#### Purpose:

Stop the console's blinking cursor showing through the Tamagotchi as a hole.

#### Outcome:

The cursor is now suppressed while the emulator runs and restored when it exits, and the operator confirmed the picture is clean. It was a second, independent source of console bleed-through rather than a variant of the first: `console_loglevel` silences `printk`, but fbcon blinks the cursor on its own timer and repaints that one cell each time, so it sailed straight through the sysctl fix from the previous cycle. The knob is `/sys/class/graphics/fbcon/cursor_blink`, which exists on this kernel and read 1. `tamafb` now saves the current value, writes 0 before its first frame, and writes the saved value back before it prints its results -- restoring rather than leaving it off, because a blinking cursor is correct at a shell prompt and turning it off for the session would be a regression disguised as a fix. Verified: `blink=0` while the process runs and `1` after it exits. Chasing it also turned up a real limit that constrains the next step, and it is recorded as a revision to KRN-001 rather than as a footnote. Backgrounding the emulator failed intermittently with SIGSEGV, and the cause was not the new cursor code: at a fragmented moment the largest free contiguous block had fallen to 256 KB, and the 222 KB static binary then started in the foreground every time while dying roughly one launch in three when backgrounded with `&`. So exec is subject to the same contiguity rule as any allocation, the margin is thin near the block size, and foreground and background are not equivalent. That matters directly for making the emulator a service: starting it early from rcS avoids the situation, starting it late does not. Getting the launch to succeed for the operator's check took a retry loop, and the earlier foreground runs at 1.00x are what showed the fault was in the launch rather than in the code. The core-syntax audit passed: the `.ai/` diff was inspected, `.ai/core.md` was not modified, entry 16 was validated against the template with six sections in canonical order, sequential numbering from 15, an ISO-8601 timestamp carrying the America/Phoenix offset, prose-only Outcome and Next Steps, eligible Files Modified, and Status values within the allowed set.

#### Next Steps:

The emulator is presentable now: it runs, it draws, the console stays out of the way, and the bars click. Two things remain before it is a game, and the first is small. Nothing consumes BTN_0/1/2, so the pet cannot be played; `splb32_pin(cpu, 0, mask, level)` on PA bits 0x01/0x02/0x04 with level 1 for press and -1 for release is the hook, and tamagui.c already drives exactly that from a keyboard, so the mapping is known and the work is reading /dev/input/event3 each frame and holding the state across frames. The second is making it a service, and that now has a constraint attached: the binary must be started early, from rcS, because exec is subject to the contiguity rule and the margin is thin -- the measurement above is the reason not to simply copy S50tbtns. Separately, pressed and released imagery on the bars would need nothing new, since CO5300_IOC_WRITE_RECT already writes panel rectangles. Larger items still open: the pwrite-versus-render split for the emulator is unmeasured, LCD-009's tearing root cause is unresolved with SPI clock the remaining suspect, and the top-bar status daemon has not been started. The Logitech keyboard, the K06's touchpad and BLE media keys are parked by user decision.

#### Files Modified:

- README.md
- docs/tamagotchi.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
