# The second SPI bus, and what the probe actually established

## The bus

From LilyGo's hardware page for the T-Watch Ultra, the board has a
general-purpose SPI bus **entirely separate** from the display's QSPI pins:

```
SCK 35   MOSI 34   MISO 33
SD   CS 21
NFC  CS  4,  INT 5        ST25R3916
LoRa CS 36,  RST 47,  BUSY 48,  INT 14   SX1262 or SX1280
```

The panel is on SCK 40, D0–D3 38/39/42/45, CS 41, RST 37, TE 6 and shares
nothing with these.

## Power map (from the same page)

```
DC1        ESP32-S3       ALDO1  SD card        BLDO1  GNSS
ALDO2      Display        ALDO3  LoRa          BLDO2  Speaker
ALDO4      Sensor         DLDO1  NFC           LDO1   GNSS backup
```

## What the probe established

`spi-probe.c` reads the ST25R3916 chip-identity register. It never got a
reply, but the diagnostics narrowed it down completely:

```
regulator enable -> 0, state enabled          DLDO1 (NFC) is genuinely on
np        MISO=0  IRQ=0  (iomux33=0x00001200)
pull-up   MISO=0  IRQ=0  (iomux33=0x00001300)  pad config correct
CONTROL gpio0 (known-good input), pull-up = 1  method proven sound
IC_IDENTITY with pull-up -> 0x00               no reply
```

The **control** is the important line. GPIO0 is Pusher B, already proven to work
as an input; the identical code path pulls it high. So the pad configuration is
not at fault, and GPIO33 really is being held low by something external.

A bug of mine in the way of that conclusion, worth recording: the first version
routed MISO with the output helper, enabling the pad's output driver so the
ESP32 drove its own MISO low and fought its own pull-up. That looked exactly
like "an unpowered chip holds the line low".

## What this means

  * The GPIO-matrix routing and IO_MUX programming **work** — proven by the
    control.
  * The NFC rail is on and it still does not answer, with MISO clamped low.

So: the ST25R3916 is absent on this unit, is on different pins than documented,
or the line is loaded by the SD socket. None of those is a kernel problem.

**The remaining honest gap:** none of this proves SPI itself transfers data.
Every target on the bus either does not answer or is not fitted. To prove the
peripheral works without relying on any external part, the thing to try is a
**peripheral loopback** — configure SPI2 to shift a byte out and read it back
with MOSI internally routed to MISO, or bridge the two pins with a wire and
read the same buffer back.

## Correction to an earlier claim

An earlier version of this note said LilyGo's documentation had the touch reset
on the wrong pin, because their table says "Expander GPIO10" while `board.h`
drives `EXPANDS_TOUCH_RST = 8`.

**That was my mistake.** Their table numbers expander pins Pxy-style, so
"GPIO10" means P1_0, which is linear gpio 8. Both sources agree; I misread the
convention and then wrote it up as a documentation error.

---

## Loopback attempt: inconclusive because the test itself is broken

The plan was sound and needs no external part: route the peripheral's data-out
signal onto the MISO pad, so whatever is shifted out comes straight back in.

```
loopback tx 0xA5 rx 0x00  differ
loopback tx 0x5A rx 0x00  differ
loopback tx 0xFF rx 0x00  differ
loopback tx 0x00 rx 0x00  MATCH      <- only the zero pattern "passes"
LOOPBACK 1/5 -> partial
```

But the sanity check that precedes it invalidates that result:

```
pad33 driven high reads 1
pad33 driven low  reads 1     <- should read 0
```

**A pad that cannot be driven low cannot be driven at all.** Since the loopback
depends on driving that pad from the SPI signal, "rx is always 0" is a property
of the broken pad control, not evidence about the peripheral.

## What is still true

  * The watch's second SPI bus exists and is documented (pins above).
  * Pad *input* configuration works: with an internal pull-up, a known-good
    input (GPIO0) reads 1 through the identical code path.
  * DLDO1 (NFC) enables successfully.
  * The NFC does not answer, and MISO on that bus reads low whether pulled up
    or not.

## What is not established

Whether SPI2 shifts bits at all. No device has ever answered, and the one test
that would not need a device is blocked by the pad-driving defect above.

## Where to go next, concretely

The `GPIO_OUT_W1TS`/`GPIO_OUT_W1TC` writes in `spi-probe.c` are not taking
effect for this pin. Either the offsets are wrong for this SoC revision or the
enable/enable-clear ordering is. Fix that first — a pad that cannot be driven
low means every output test in this driver, including the loopback, is
worthless.

The simplest external check, if a wire is ever convenient: bridge GPIO34 (MOSI)
to GPIO33 (MISO) and run the loopback again. That removes the pad-driving
question from the equation entirely, because the pad is then only ever an input.
