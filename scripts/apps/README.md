# Board app launchers

The two Python files here are the targets of this watch's Start-menu entries.
They live on the device at `/root/Apps/`, on the littlefs `storage` partition,
and **they are not part of the firmware image** — a full-image flash or a fresh
install does not create them. These copies exist so they can be restored.

```
pm.py     -> /root/Apps/pm.py     Pokemon Mini  (Start menu: "Pokémon Mini")
tama.py   -> /root/Apps/tama.py   Tamagotchi V3 (Start menu: "Tamagotchi V2-3")
```

Each is paired with a shortcut file the desktop reads:

```
/root/Start menu/Pokémon Mini.tdlink      target=/root/Apps/pm.py
/root/Start menu/Tamagotchi V2-3.tdlink   target=/root/Apps/tama.py
```

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
