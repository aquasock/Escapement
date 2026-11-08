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
