/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drv2605.h - TI DRV2605 haptic driver, kernel side.
 *
 * The chip sits at I2C 0x5A on this board and drives an ERM motor. This
 * header exists so a consumer -- currently the touch driver -- can ask for a
 * click without either knowing the other's registers.
 */

#ifndef _LINUX_DRV2605_H
#define _LINUX_DRV2605_H

#if IS_ENABLED(CONFIG_INPUT_DRV2605)
void drv2605_tap(void);
#else
static inline void drv2605_tap(void) { }
#endif

#endif /* _LINUX_DRV2605_H */
