/*
 * haptic.h - tap feedback on the DRV2605 haptic driver.
 *
 * haptic_init() opens /dev/i2c-0, checks that a DRV2605 really answers at 0x5A,
 * and puts it in internal-trigger mode with the ERM effect library selected.
 * haptic_tap() plays one short click. Both return without doing anything if
 * init failed, so a caller does not have to test the return value unless it
 * wants to report the absence.
 */
#ifndef ESPACEMENT_HAPTIC_H
#define ESPACEMENT_HAPTIC_H

int  haptic_init(void);	/* 0 = ready, -1 = absent or not a DRV2605 */
void haptic_tap(void);	/* one short click; no-op if not ready */
void haptic_close(void);

#endif
