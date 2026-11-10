/*
 * haptic.c - tap feedback on the DRV2605.
 *
 * This is a library, not a program: a caller invokes haptic_tap() when a
 * button goes down. It is kept separate because the same three calls serve
 * anything on this board that wants a click.
 *
 * The chip
 * --------
 * A TI DRV2605 at I2C 0x5A, driving an ERM motor. Two things have to be right
 * before it will do anything, and both are easy to miss:
 *
 *   - The motor's power rail is gated by the XL9555 expander at 0x20, port 0
 *     bit 6. TinyPetz enables it at boot along with the display rail; if that
 *     bit is low the chip answers on I2C perfectly and buzzes never.
 *   - The DRV2605 boots into internal-trigger mode with no library selected and
 *     an empty wave sequence, so writing GO does nothing until a library and a
 *     sequence have been written.
 *
 * Identification, read off this unit rather than assumed:
 *
 *     reg 0x00 STATUS   0xE0   DEVICE_ID bits 7:5 = 0b111 -> DRV2605
 *     reg 0x1A FEEDBACK 0x36   bit 7 = 0 -> ERM, not LRA
 *
 * Which matters because the library is motor-specific: library 6 is for LRA
 * and the ERM libraries are 1..4. Selecting the wrong one on an ERM gives a
 * weak or silent buzz rather than an error.
 *
 * Triggering
 * ----------
 * Internal trigger mode: write the effect IDs into WAVESEQ1 (0x04) and
 * WAVESEQ2 (0x05), then write 1 to GO (0x0C). The sequence ends at ID 0, so a
 * single effect needs WAVESEQ2 = 0. Nothing is polled; the chip plays and
 * stops.
 */

#include "haptic.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <unistd.h>

#define DEV_I2C		"/dev/i2c-0"
#define DRV_ADDR	0x5A

/* Register map, the ones used here. */
#define REG_STATUS	0x00
#define REG_MODE	0x01
#define REG_RTP		0x02
#define REG_LIBRARY	0x03
#define REG_WAVESEQ1	0x04
#define REG_WAVESEQ2	0x05
#define REG_GO		0x0C
#define REG_FEEDBACK	0x1A

#define MODE_INTERNAL	0x00	/* internal trigger, not PWM/RTP/audio */
#define LIBRARY_ERM_A	0x01	/* TS2200 A: the ERM effect set */

/*
 * Effect IDs from library 1. These are index values into the library, not
 * amplitudes, so they are fixed by the datasheet rather than tuned here.
 *
 *   1  Strong Click 100%      crisp, what a phone button feels like
 *   4  Sharp Click 100%       sharper and shorter
 *   10 Double Click 100%      two pulses
 *   47 Buzz 1 100%            long, for an alarm rather than a tap
 */
#define EFFECT_STRONG_CLICK	1

static int fd = -1;
static int ready;

static int wr(uint8_t reg, uint8_t val)
{
	struct i2c_msg msg;
	uint8_t buf[2] = { reg, val };

	msg.addr = DRV_ADDR;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	struct i2c_rdwr_ioctl_data d = { .msgs = &msg, .nmsgs = 1 };

	return ioctl(fd, I2C_RDWR, &d) < 0 ? -1 : 0;
}

static int rd(uint8_t reg, uint8_t *val)
{
	struct i2c_msg msgs[2];
	uint8_t r = reg;

	msgs[0].addr = DRV_ADDR;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &r;

	msgs[1].addr = DRV_ADDR;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = val;

	struct i2c_rdwr_ioctl_data d = { .msgs = msgs, .nmsgs = 2 };

	return ioctl(fd, I2C_RDWR, &d) < 0 ? -1 : 0;
}

int haptic_init(void)
{
	uint8_t status = 0;

	fd = open(DEV_I2C, O_RDWR);
	if (fd < 0)
		return -1;

	if (rd(REG_STATUS, &status) < 0) {
		close(fd);
		fd = -1;
		return -1;
	}

	/* Bits 7:5 are DEVICE_ID; 0b111 is the DRV2605. Anything else on this
	 * address is not the part this code knows how to drive, so refuse
	 * rather than write effect IDs into a stranger. */
	if (((status >> 5) & 0x7) != 0x7) {
		close(fd);
		fd = -1;
		return -1;
	}

	if (wr(REG_MODE, MODE_INTERNAL) < 0 ||
	    wr(REG_RTP, 0x00) < 0 ||
	    wr(REG_LIBRARY, LIBRARY_ERM_A) < 0) {
		close(fd);
		fd = -1;
		return -1;
	}

	ready = 1;
	return 0;
}

void haptic_tap(void)
{
	if (!ready || fd < 0)
		return;

	/* Sequence first, then GO. Writing GO with a stale sequence replays
	 * whatever was there before, which is how a "tap" ends up buzzing for
	 * two seconds. */
	if (wr(REG_WAVESEQ1, EFFECT_STRONG_CLICK) < 0)
		return;
	if (wr(REG_WAVESEQ2, 0x00) < 0)
		return;
	(void)wr(REG_GO, 0x01);
}

void haptic_close(void)
{
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
	ready = 0;
}
