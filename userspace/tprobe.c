/*
 * tprobe.c - bring the CST9217 up and report what it says, on the watch.
 *
 * A throwaway diagnostic, not the driver. It exists because a shell loop over
 * telnet costs a fork, an exec and a network round trip per sample, which is
 * slow enough that the interesting states are missed and the loop times out
 * before printing anything.
 *
 * Everything here follows LilyGo's SensorLib (lewisxhe/SensorLib, MIT),
 * src/touch/TouchDrvCST92xx.cpp and Xinyuan-LilyGO/LilyGoLib
 * src/LilyGoWatchUltra.cpp initTouch(). Where this file differs from that
 * source it says so.
 *
 * Usage:  tprobe [-q N] [-t seconds]
 *   -q N        quiet: only print samples that carry a point
 *   -t seconds  how long to sample (default 20)
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define I2C_BUS		"/dev/i2c-0"

#define XL9555_ADDR	0x20
#define XL9555_OUT1	0x03
#define XL9555_CFG1	0x07
#define XL_TOUCH_RST	0x01	/* P08 on port 1 */

#define TP_ADDR		0x1a
#define TP_REG_READ	0xd000
#define TP_ACK		0xab
#define TP_BLOCK	15

static int fd = -1;

static void ms(int n)
{
	struct timespec ts = { n / 1000, (long)(n % 1000) * 1000000L };
	nanosleep(&ts, NULL);
}

/* One write of n bytes to addr. */
static int wr(int addr, const uint8_t *b, int n)
{
	uint8_t tmp[8];
	struct i2c_msg m = { .addr = addr, .flags = 0, .len = n, .buf = tmp };
	struct i2c_rdwr_ioctl_data d = { .msgs = &m, .nmsgs = 1 };

	memcpy(tmp, b, n);
	return ioctl(fd, I2C_RDWR, &d) < 0 ? -errno : 0;
}

/* Write wlen bytes, repeated start, read rlen bytes. One transaction. */
static int wr_rd(int addr, const uint8_t *w, int wlen, uint8_t *r, int rlen)
{
	struct i2c_msg m[2] = {
		{ .addr = addr, .flags = 0, .len = wlen, .buf = (uint8_t *)w },
		{ .addr = addr, .flags = I2C_M_RD, .len = rlen, .buf = r },
	};
	struct i2c_rdwr_ioctl_data d = { .msgs = m, .nmsgs = 2 };

	return ioctl(fd, I2C_RDWR, &d) < 0 ? -errno : 0;
}

/*
 * Read n bytes from a 16-bit register. LilyGo's readWordFromMem() does exactly
 * this: two address bytes out, repeated start, n bytes in.
 */
static int read_reg16(uint16_t reg, uint8_t *buf, int n)
{
	uint8_t a[2] = { reg >> 8, reg & 0xff };

	return wr_rd(TP_ADDR, a, 2, buf, n);
}

static void hexdump(const char *tag, const uint8_t *b, int n)
{
	int i;

	printf("%s", tag);
	for (i = 0; i < n; i++)
		printf(" %02x", b[i]);
	printf("\n");
}

/*
 * Reset through the XL9555.
 *
 * LilyGo's initTouch() pulses it low 20 ms and high 60 ms, and passes -1 as the
 * reset pin to the touch driver because the line is not a GPIO of the SoC at
 * all -- it is P08 of the expander. The expander's port 1 comes up as all
 * inputs, so P08 has to be made an output first; LilyGo does that in begin()
 * with pinMode(EXPANDS_TOUCH_RST, OUTPUT).
 *
 * The input/output registers are read first so that the display and haptic
 * enables on the same expander are preserved rather than overwritten.
 */
static int tp_reset(void)
{
	uint8_t cfg, out;
	uint8_t b[2];

	b[0] = XL9555_CFG1;
	if (wr_rd(XL9555_ADDR, b, 1, &cfg, 1) < 0)
		return -1;
	b[0] = XL9555_OUT1;
	if (wr_rd(XL9555_ADDR, b, 1, &out, 1) < 0)
		return -1;

	printf("expander before: cfg1=%02x out1=%02x\n", cfg, out);

	/* P08 output, keeping the rest as they are. */
	b[0] = XL9555_CFG1; b[1] = cfg & ~XL_TOUCH_RST;
	if (wr(XL9555_ADDR, b, 2) < 0)
		return -1;

	/* Assert, hold, release. */
	b[0] = XL9555_OUT1; b[1] = out & ~XL_TOUCH_RST;
	if (wr(XL9555_ADDR, b, 2) < 0)
		return -1;
	ms(20);
	b[0] = XL9555_OUT1; b[1] = out | XL_TOUCH_RST;
	if (wr(XL9555_ADDR, b, 2) < 0)
		return -1;
	ms(60);

	printf("expander after: cfg1=%02x out1=%02x\n",
	       cfg & ~XL_TOUCH_RST, out | XL_TOUCH_RST);
	return 0;
}

/*
 * getAttribute() from SensorLib: enter command mode, then read the checkcode,
 * the resolution, and the chip type. Each of the three must pass or the part is
 * not usable, and LilyGo fails init rather than carrying on hopefully.
 */
static int tp_attribute(void)
{
	uint8_t b[8];
	uint32_t checkcode, fw;
	uint16_t resx, resy, chip;
	uint8_t cmd[2] = { 0xd1, 0x01 };
	int i;

	if (wr(TP_ADDR, cmd, 2) < 0) {
		printf("command mode write failed\n");
		return -1;
	}
	ms(10);

	if (read_reg16(0xd1fc, b, 4) < 0) {
		printf("checkcode read failed\n");
		return -1;
	}
	checkcode = ((uint32_t)b[3] << 24 | (uint32_t)b[2] << 16 |
		     (uint32_t)b[1] << 8 | b[0]);
	hexdump("checkcode 0xd1fc:", b, 4);
	printf("  -> 0x%08x  %s\n", checkcode,
	       (checkcode & 0xffff0000) == 0xcaca0000 ? "OK" : "BAD");

	if (read_reg16(0xd1f8, b, 4) < 0) {
		printf("resolution read failed\n");
		return -1;
	}
	resx = (uint16_t)(b[1] << 8 | b[0]);
	resy = (uint16_t)(b[3] << 8 | b[2]);
	hexdump("resolution 0xd1f8:", b, 4);
	printf("  -> X=%u Y=%u\n", resx, resy);

	if (read_reg16(0xd204, b, 4) < 0) {
		printf("chip type read failed\n");
		return -1;
	}
	chip = (uint16_t)(b[3] << 8 | b[2]);
	hexdump("chip type 0xd204:", b, 4);
	printf("  -> 0x%04x %s, ProjectID 0x%04x\n", chip,
	       (chip == 0x9217 || chip == 0x9220) ? "OK" : "BAD",
	       (unsigned)(b[1] << 8 | b[0]));

	if (read_reg16(0xd208, b, 8) < 0) {
		printf("fw version read failed\n");
		return -1;
	}
	fw = ((uint32_t)b[3] << 24 | (uint32_t)b[2] << 16 |
	      (uint32_t)b[1] << 8 | b[0]);
	printf("fw version 0x%08x %s\n", fw,
	       fw == 0xa5a5a5a5 ? "(NO FIRMWARE)" : "");

	/*
	 * The one deliberate difference from SensorLib: it never leaves command
	 * mode, because its only setMode() caller is sleep(). Reports were
	 * observed as 15 x 0xff while in command mode, so normal mode is
	 * selected here and 0x0002 is read back. That is the handshake tbtns
	 * used; it is included because a report read in command mode returns
	 * nothing, not because LilyGo needs it.
	 */
	cmd[0] = 0xd1; cmd[1] = 0x09;
	if (wr(TP_ADDR, cmd, 2) < 0) {
		printf("normal mode write failed\n");
		return -1;
	}
	ms(10);
	for (i = 0; i < 3; i++) {
		if (read_reg16(0x0002, b, 2) == 0) {
			printf("mode readback 0x0002 = %02x %02x %s\n",
			       b[0], b[1], b[1] == 0x09 ? "OK" : "");
			break;
		}
		ms(100);
	}

	return 0;
}

/*
 * One report read, exactly as getTouchPoints() does it: 15 bytes from 0xD000,
 * then a separate 3-byte write of 0xD000 followed by 0xAB to acknowledge.
 *
 * The ack is not optional. Without it the controller keeps returning the same
 * block and a release is never seen.
 */
static int tp_report(uint8_t *blk)
{
	uint8_t ack[3] = { 0xd0, 0x00, TP_ACK };
	int err;

	err = read_reg16(TP_REG_READ, blk, TP_BLOCK);
	if (err) {
		snprintf((char *)blk, TP_BLOCK, "%d", err);
		return err;
	}
	/* Ack regardless: a read whose ack is skipped leaves the buffer stuck. */
	wr(TP_ADDR, ack, 3);
	return 0;
}

int main(int argc, char **argv)
{
	int quiet = 0, secs = 20, i, npoints = 0, nerr = 0, nreads = 0;
	unsigned long start, now;
	uint8_t blk[TP_BLOCK];

	while ((i = getopt(argc, argv, "q:t:")) != -1) {
		switch (i) {
		case 'q': quiet = atoi(optarg); break;
		case 't': secs = atoi(optarg); break;
		default: return 2;
		}
	}

	fd = open(I2C_BUS, O_RDWR);
	if (fd < 0) {
		perror(I2C_BUS);
		return 1;
	}

	if (tp_reset() < 0) {
		printf("expand reset failed: %s\n", strerror(errno));
		return 1;
	}
	if (tp_attribute() < 0)
		return 1;

	printf("--- sampling %d s\n", secs);
	start = (unsigned long)time(NULL);
	do {
		int err = tp_report(blk);
		nreads++;
		if (err) {
			nerr++;
			if (!quiet)
				printf("read error %d\n", err);
		} else if (blk[0] == TP_ACK) {
			/* No new data -- the documented "nothing to report". */
			if (!quiet)
				printf("no-data\n");
		} else {
			int n = blk[5] & 0x7f;
			/*
			 * A point only counts when its event nibble is 0x06.
			 *
			 * This is LilyGo's rule, and it is not cosmetic: the
			 * controller hands back blocks that carry a finger count and
			 * a valid marker 0xab while the event nibble is 0x00, and
			 * those are not presses. Counting on the finger count alone
			 * invented a phantom at raw (41,451) with nothing on the
			 * glass. getTouchPoints() checks `event == 0x06` for exactly
			 * this reason, and then clears the whole list if point 0's
			 * event is 0x00.
			 */
			if (n && n <= 2) {
				int f, live = 0;

				if (!quiet) {
					printf("blk n=%d valid=%s ev0=%02x:", n,
					       blk[6] == TP_ACK ? "y" : "n",
					       blk[0] & 0x0f);
					for (f = 0; f < TP_BLOCK; f++)
						printf(" %02x", blk[f]);
					printf("\n");
				}
				for (f = 0; f < n; f++) {
					uint8_t *p = blk + f * 5 + (f ? 2 : 0);
					unsigned x = (p[1] << 4) | (p[3] >> 4);
					unsigned y = (p[2] << 4) | (p[3] & 0x0f);
					unsigned ev = p[0] & 0x0f;

					if (ev != 0x06)
						continue;
					live++;
					printf("  TOUCH finger%d id=%u ev=%02x raw=(%u,%u)"
					       " panel=(%d,%d) fb=(%d,%d)\n",
					       f, p[0] >> 4, ev, x, y,
					       (int)x, (int)y,
					       (int)x - 45, (int)y - 45);
				}
				if (live)
					npoints++;
			}
		}
		now = (unsigned long)time(NULL);
	} while (now - start < (unsigned long)secs);

	printf("--- reads=%d errors=%d with_points=%d\n", nreads, nerr, npoints);
	close(fd);
	return 0;
}
