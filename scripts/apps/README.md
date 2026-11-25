# Board app launchers

**NEITHER OF THESE IS A START-MENU ENTRY ANY MORE, and the folder on the device
is now empty.** Both files are kept here as the source of record for scripts that
still exist at `/root/Apps/` on the littlefs `storage` partition — they are **not
part of the firmware image**, so a full-image flash or a fresh install does not
create them, and these copies exist so they can be restored.

```
pm.py     -> /root/Apps/pm.py     Pokemon Mini   -- orphaned, no menu entry
tama.py   -> /root/Apps/tama.py   Tamagotchi V3  -- orphaned, no menu entry
```

WHAT HAPPENED TO THE ENTRIES, in order, because the two went for different
reasons and only one of them was about the script:

```
/root/Start menu/Pokémon Mini.tdlink      removed earlier; slot B's picker
                                          does this better, without a Terminal
/root/Start menu/Tamagotchi V2-3.tdlink   removed 2026-11-25; the native
                                          "●☼ Tama V3" entry replaces it
```

A `.tdlink` to a `.py` is opened by `td_script_run()`, whose only path is
`td_terminal_run()` — so every launch put a Terminal on the glass, echoed the
command and printed a line before the game started. The native entries in
`board_apps_register()` do not: the panel simply goes black until the emulator is
up. That is the same reasoning that retired the three slot B launchers below.

BOTH SCRIPTS ARE NOW ORPHANED RATHER THAN WRONG. `pm.py` lost its entry at the
operator's direction and was deliberately left on the card because deleting it
was not asked for; `tama.py` lost its entry last. Neither is called by anything,
both still work from a shell (`py /root/Apps/tama.py`), and neither should be
treated as dead: they are the fallback if a native entry ever misbehaves.

## The slot B launchers are NOT scripts any more

Three files used to live here — `gbslotb.py`, `nesslotb.py`, `pmslotb.py`, each
with a `.tdlink` beside it — and they were deleted on 2026-11-23. They have been
replaced by **native apps registered in `board_apps_register()`** in the port's
`link_twatch.c`, calling `slotb_launch("<key>")`.

THEY COULD NOT STAY SCRIPTS, for two reasons that are both structural:

  - A `.tdlink` pointing at a `.py` is opened by `td_script_run()`, whose only
    path is `td_terminal_run()` — so every launch put a Terminal on the glass,
    echoed the command, ran the script and printed "rebooting into ota_1"
    before the reboot. A native app draws nothing and the panel simply goes
    black until slot B is up.
  - They lived on the card's littlefs, so any full-image flash or factory
    restore deleted them, and this file had to carry the `echo` lines that
    recreated them. Native apps are in the image.

Nothing in this directory should be recreated for slot B. To add a slot B app,
add a row to the table in `slotb.c` and an entry in `board_apps_register()`.

`pm` and `tama` start an emulator INSIDE the desktop. The slot B entries are a
different kind of thing: they reboot the watch into a personality with no
desktop at all, and they are native apps now (see above).

The command they call — `slotb play <key>` from a shell, `slotb_launch(key)` from
the native app — stores the key in NVS, sets `ota_1` as the boot partition and
restarts. Slot B reads the key and starts that app directly, skipping its menu,
so a menu entry that says "NES" lands in the NES, and pressing Pusher A there
returns to the DESKTOP, because that is where the wearer came from.
`slotb start` with no app clears the key and shows slot B's own menu instead, and
apps launched from that menu return to it so another game is one tap away.

## Why these are so short, and what they must not lose

`py` is MicroPython with a `tinydesk` module registered but **not auto-imported**.
A launcher that calls `tinydesk.shell(...)` without `import tinydesk` first fails
with:

```
Traceback (most recent call last):
  File "/root/Apps/pm.py", line 1, in <module>
NameError: name 'tinydesk' isn't defined
```

That is exactly what happened: `pm.py` shipped without the import and every
launch from the Start menu printed that traceback instead of starting the game.
`tama.py` had it and worked all along. **Both lines are required, in this order.**

Neither passes an argument, on purpose. `pm start` and `tama` with no argument
use the firmware's own default ROM paths (`PM_ROM_DEFAULT`,
`TAMA_ROM_DEFAULT`), so the launcher does not carry a long path with spaces,
commas and parentheses — which is one more thing to get wrong and one more
thing that has to be re-typed correctly to install it.

## Installing

There is no installer script; write them over the shell. `tdsh` keeps single
quotes literal, so a double-quoted Python argument survives:

```
echo 'import tinydesk' > "/root/Apps/pm.py"
echo 'tinydesk.shell("pm start")' >> "/root/Apps/pm.py"

echo 'import tinydesk' > "/root/Apps/tama.py"
echo 'tinydesk.shell("tama")' >> "/root/Apps/tama.py"
```

Then check both with `cat`, and launch one from the Start menu — the menu is the
path that failed, so `py` succeeding is not sufficient evidence on its own.

One harness note if you are driving this over the serial console: write each
escape sequence in a **single** `write()`. The desktop's parser gives up on a
lone `ESC` after a few tens of milliseconds, so a byte-at-a-time sender turns
`F10` into `Escape` (which *closes* the menu) followed by the literal text
`[21~`.

A second note, about deleting files there: **`tdsh`'s `rm` does not take `-f`.**
It reads the flag as a filename, prints `rm '-f': No such file or directory`, and
returns 1 — but it still removes the real operand, so a `rm -f X; echo $?` looks
like a failure while having done the deletion. Verify with `cat` afterwards
rather than trusting the status, and write plain `rm X`.
