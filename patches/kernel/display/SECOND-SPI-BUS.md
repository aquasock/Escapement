# The watch has a second, independent SPI bus

This is the most useful thing to come out of the display work, and it should
have been found first. From LilyGo's hardware page for the T-Watch Ultra:

```
Second SPI bus (separate from the panel)
  SCK    35
  MOSI   34
  MISO   33
  SD   CS 21
  NFC  CS  4,  INT 5      ST25R3916
  LoRa CS 36, RESET 47, BUSY 48, INT 14   SX1262 or SX1280
```

The display is on its own QSPI pins (SCK 40, D0–D3 38/39/42/45, CS 41, RST 37,
TE 6) and shares nothing with these. So the panel's bus and a general-purpose
SPI bus are independent, and the second one can be tested without disturbing
the panel at all.

## Why it matters

The CO5300 is **write-only**. It never acknowledges, never returns a byte, and
reports no error. A driver can run every transaction to completion against a
panel that is not listening — which is exactly what happened across eight
register-level fixes. There was no feedback signal anywhere.

Every device on the second bus answers. Any of them would have told us within
minutes whether SPI worked at all.

## First attempt: the SX1262

`spi-probe.c` resets the radio and reads `GetStatus` (0xC0). Result:

```
BUSY before reset = 0
BUSY after reset:  high=0 low=200  -> stuck low
GetStatus returned 0x00  -> no response
second read 0x00
```

Interpretation, and its limits:

  * MISO reads a **stable 0x00**, not 0xFF and not noise. A floating line
    would not do that, so something is holding it.
  * BUSY never moves, which for an SX1262 means it is not running.

Power is the likely cause. The radio is enabled by XL9555 P05, and `cfg0`
reads `0x3f` — bits 0..5 are **inputs**, so nothing was driving it. A
`gpio-hog` on line 5 was added to drive it high; the probe still got no
answer, but it is not confirmed the hog ran before the probe at 1.35 s.

**And there is a bigger caveat:** LilyGo list the radio as "SX1262 **or**
SX1280", i.e. a variant option. This unit may not have a LoRa module fitted at
all, in which case the test is meaningless and the `0x00` is just an idle line.

## What to test instead

Two better candidates on the same bus:

  * **ST25R3916 NFC** (CS 4). A standard SPI register device with a chip-ID
    read, and it is listed as standard equipment rather than an option. Its
    interrupt shares nothing with the panel.
  * **The SD socket** (CS 21). `mmc_spi` is in-tree, and a card identification
    either works or does not — no ambiguity. Requires enabling `CONFIG_BLOCK`
    and `MMC`/`MMC_SPI`, which this kernel currently lacks entirely.

Either would settle "does SPI2 work" definitively.

## The architectural lesson

Do not hand-program the SPI peripheral for the panel. Extend
`drivers/spi/spi-esp32.c` — it already owns clock gating, DMA, pin routing and
transfer framing correctly, and it is exercised by nobody on this board today.
The bespoke register driver in `co5300.c` was the wrong shape from the start,
and the lack of a feedback signal is what made the wrong shape expensive.
