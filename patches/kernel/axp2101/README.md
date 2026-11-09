# AXP2101 support: backport of a pending upstream series

Not ours. This is Olivier Louvignes' series *"Add support for the X-Powers
AXP2101 PMIC"*, v1, posted 2026-10-23:

  https://patchwork.kernel.org/series/1192024/
  submitters: Olivier Louvignes <olivier@mgcrea.io>

Series as posted: 7 patches (4 code, 3 dt-bindings). Patchwork only ingested
5 of the 7; 2/7 (mfd) and 3/7 (regulator) were retrieved from the marc.info
archive. Patch 4/7, a dt-binding only, was not recovered and is not needed to
build.

## Why it is needed

Linux has no `axp2101` variant: not in this kernel, and not in mainline as of
this writing. `drivers/mfd/axp20x-i2c.c` knows axp152/192/202/209/221/223/
313a/323/717/803/806/15060 only. The AXP2101 is the watch's PMU, so without
this series it cannot be driven at all — there is nothing to enable.

## Local change

`0002-mfd-axp20x-add-axp2101.patch` does not apply as posted: its context
expects `regmap_set_bits()` in `axp20x_power_off()`, where this kernel has
`regmap_write()`. Adapted to `regmap_write()`.

That is safe rather than a behavioural change: in both existing cases
(`AXP313A_ID`/`AXP323_ID` and the default) the series sets
`shutdown_bit = AXP20X_OFF`, which is exactly the value the old code wrote, so
the two forms agree for every pre-existing variant. For the AXP2101 it becomes
a write of bit 0 to `COMM_CFG` (0x10), which is what this board's own
ESP-IDF firmware does to power off.

If upstream later settles this function differently, drop the local change and
rebase on the merged series.

## Status

Rebased onto Linux 7.2.4 + this project's patches. Verified only as far as
"the six patches apply and the kernel builds" — hardware behaviour is recorded
in docs/bringup notes.

## Two additions are whole files, not patches

`final/` holds `axp20x.c`, `axp20x_battery.c` and `axp20x.h`, installed by
`scripts/build-kernel.sh` after the patch stack. They carry:

- `POWER_SUPPLY_PROP_CAPACITY` and `VOLTAGE_NOW` for the AXP2101 battery, read
  straight from registers `0xa4` (percent, bits 6:0) and `0x34`/`0x35` (14-bit,
  already mV). There is no IIO fallback for this variant.
- the `axp20x-pek` cell in `axp2101_cells[]`, with the two edge interrupts, so
  the top-left pusher becomes an input device instead of being silently absent.

They were patches (0009, 0010) until both turned out to be broken in ways that
only showed up as a *silently missing driver*:

- **0009 had hunk line counts that did not match its body.** `git apply`
  rejected it as corrupt, and because the loop wrapped the call in `|| true`
  the failure was invisible. The kernel built, booted, and reported no battery.
- **0010's diff headers carried a timestamp suffix** and a path missing the
  `drivers/` prefix, so `git apply` looked for `mfd/axp20x.c` and did not find
  it. Also silent.

The lesson is in `scripts/build-kernel.sh` now: the loop reports failures
instead of swallowing them, and the recursive glob means nothing under a
subdirectory is skipped again.

## Verified on hardware, 2026-11-09

```
/sys/class/power_supply/    axp20x-battery  axp20x-usb
capacity                    100
voltage_now                 4175000        uV
status                      Full
usb online                  1

/proc/interrupts
  8:  esp32xx-gpio       7  axp2101_irq_chip
 17:  axp2101_irq_chip   8  axp20x-pek-dbr
 18:  axp2101_irq_chip   9  axp20x-pek-dbf
 23:  axp2101_irq_chip  14  axp20x-usb-power-supply
 24:  axp2101_irq_chip  15  axp20x-usb-power-supply

input: axp20x-pek as /devices/platform/i2c0/i2c-0/0-0034/axp20x-pek/input/input1
```

`voltage_now` needed its units fixed once: the register is mV and the property
is uV, and the first version reported 4175 for a 4175 mV battery.

`CONFIG_AXP20X_POWER` also had to be enabled -- it is what registers
`axp20x-usb`, and it was off.
