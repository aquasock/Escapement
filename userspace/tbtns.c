/*
 * tbtns.c - three touch buttons on the bottom strip of the watch.
 *
 * The CO5300 driver paints three coloured bars in the area below fb0 (rows
 * 445..501 of the 410x502 glass). This daemon turns a finger on those bars into
 * ordinary key events, so they behave exactly like the two side pushers: an
 * on/off input device, nothing more.
 *
 *     touch-buttons     BTN_0   left third    (red bar)
 *                       BTN_1   middle third (green bar)
 *                       BTN_2   right third  (blue bar)
 *
 * Why this is not a kernel touchscreen driver
 * -------------------------------------------
 * A kernel driver for a pointer reports coordinates, which is not what is
 * wanted here: three regions of about 135 px each, pressed with a fingertip
 * some 40 px across. Everything needed is already in the running kernel --
 * /dev/i2c-0 (CONFIG_I2C_CHARDEV), /dev/uinput (CONFIG_INPUT_UINPUT) and the
 * XL9555 as a gpiochip -- so this is a userspace program with no kernel config,
 * no device tree change and no rebuild to adjust a boundary.
 *
 * The part is a Hynitron CST9217 at I2C 0x1A. Its protocol came from TinyPetz
 * (firmware/bringup/main/touch.c), which established it on this same board, and
 * three of its findings are load-bearing here:
 *
 *   1. It answers at 0x1A. A second device answers at 0x5A, which is the
 *      DRV2605 haptic driver and has nothing to do with touch.
 *   2. Reads are a two-byte big-endian address (0xD000) followed by a report
 *      block, and the block must be ACKNOWLEDGED by writing the same address
 *      followed by 0xAB. Without the ack the controller keeps returning the
 *      same stale block, which reads as a phantom touch.
 *   3. A freshly reset part is in COMMAND mode: register reads succeed and
 *      return zeros, but it is not scanning. It has to be unlocked
 *      (0xD1 = 0x01, 0xD1 = 0x1E twice, read 0x0002 and expect 0x1E back) and
 *      then put in normal mode (0xD1 = 0x09) before any touch appears.
 *
 * The one thing this adds is a note on release.
 *
 * THE CONTROLLER HAS NO UP EVENT.
 *
 * It reports on a 66 ms cadence while a finger is down and then simply stops.
 * There is no lift report, and its finger-count byte is not trustworthy either
 * -- TinyPetz measured a count of 1 persisting sixteen seconds after the finger
 * was lifted. So "released" is a TIMEOUT, and it is the only thing that ends a
 * press. Get this wrong and the buttons stick on.
 *
 * The timeout is 200 ms: three report periods, long enough to survive a dropped
 * report on this bit-banged bus (dropped reads have been observed), short
 * enough that a tap releases about two frames after the finger leaves.
 *
 * Usage:
 *   tbtns              run
 *   tbtns -v           run and log every decoded report
 *   tbtns -n           decode and print, create no input device
 *   tbtns -k a,b,c     keycodes to emit (default BTN_0,BTN_1,BTN_2)
 *   tbtns -e 158,294   band boundaries in controller columns
 *   tbtns -y 445       first row treated as the button strip
 *   tbtns -s X,B       x correction: ctrl_x = raw_x / X + B
 *   tbtns -t MS        release timeout
 *   tbtns -o           one-shot: report the handshake and the first three
 *                      fresh reports, then exit. This is the calibration tool.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#define DEV_I2C		"/dev/i2c-0"
#define DEV_UINPUT	"/dev/uinput"

#define TOUCH_ADDR	0x1A
#define XL9555_ADDR	0x20

/* CST92xx registers, all two-byte addressed. */
#define REG_REPORT	0xD000
#define REG_MODE	0xD1
#define REG_UNLOCK	0x0002
#define REG_ACKBYTE	0xAB

#define MODE_CMD	0x01
#define MODE_UNLOCK	0x1E
#define MODE_NORMAL	0x09

#define MAX_FINGERS	2

/*
 * The report block is 15 bytes: two fingers of five bytes each, plus five.
 * Only the first seven are read out of it -- byte 0 the event, bytes 1..3 the
 * nibble-packed coordinates, byte 5 the finger count, byte 6 the freshness
 * marker -- but the TRANSFER must be the full 15.
 *
 * This was 7, on the reasoning that a shorter transfer is more reliable on this
 * bus, and that was wrong in a way that produced the worst symptom so far: the
 * acknowledge that advances the controller's report buffer is issued against
 * the block, so reading only 7 bytes and then acking left the buffer mid-block
 * and it never advanced. The visible effect was a touch that reported one
 * position, then nothing, then "released", for as long as a finger was held --
 * not because the controller had nothing to say, but because it was never told
 * that the previous report had been consumed.
 */
#define BLOCK_LEN	15		/* full vendor block; 7 are decoded */
#define DECODE_LEN	7

#define EVT_DOWN	0x06
#define EVT_UP		0x07

/* XL9555: port 1, bit 0 is the touch reset line (active low). */
#define XL9555_OUT1	0x03
#define XL9555_RST_BIT	0x01

/* ---- defaults, all overridable ------------------------------------------- */

/* Controller-column boundaries between buttons. These mirror BTN_EDGE1 and
 * BTN_EDGE2 in patches/kernel/display/final/co5300.c, so the band that is
 * painted is the band that is read. */
static int edge1 = 158;
static int edge2 = 294;

/*
 * First row treated as the button strip.
 *
 * The bar is painted from panel row 445 (FB_Y + FB_H), but the threshold is set
 * five rows HIGHER, at 440, and that gap is deliberate.
 *
 * The touch controller's Y axis is the least accurate of the two: TinyPetz
 * measured a least-squares fit with roughly 4.6 px RMS residual against a
 * fingertip, and this unit shows the same. Pressing near the top edge of a bar
 * routinely reports Y in the 441..444 range -- measured, not assumed: two taps
 * in a ten-tap run decoded to ctrl_y 442 and 443 and were discarded as "not in
 * the strip" while the finger was plainly on the bar.
 *
 * Five rows of tolerance costs at most the last five console rows being read as
 * a button press, and buys the property these buttons are actually for: the
 * boundary is a region, not a target. The bands are 135 px wide for the same
 * reason.
 */
static int y_strip = 440;

/* x correction from TinyPetz's four-point calibration of this board's panel:
 * ctrl_x = raw_x / 1.0871 + 22. The sensor's origin is the glass edge while the
 * controller's CASET origin is 22 columns earlier, which is what the offset is.
 * TinyPetz states the fit's own accuracy as a few pixels against a fingertip,
 * which is why the bands are 135 px wide and this needs no better. */
static double xs = 1.0871, xb = 22.0;
static double ys = 1.0515;

static int release_ms = 200;

/*
 * Gap between polls, in milliseconds.
 *
 * The controller reports on a 66 ms cadence while the glass is touched, so
 * polling near that cadence is the natural choice. Measured on hardware at ten
 * quick taps: nine readable reports and eight presses, the loss being a tap that
 * fell between polls. That is accepted rather than chased -- a deliberate press
 * always lands, and polling harder to catch a deliberate flick is not worth the
 * bus traffic on a device whose reads are already unreliable.
 */
#define POLL_MS 60

static int verbose, dry_run, oneshot;

/*
 * -C: classify a controller column and exit, before /dev/i2c-0 is touched.
 *
 * This exists so the band arithmetic can be tested on a build host, where the
 * panel and the touch controller are not. A boundary that is off by one is
 * invisible until a finger lands on it, and by then it has cost a flash.
 */
static int classify_only;
static int do_classify;

static int codes[3] = { BTN_0, BTN_1, BTN_2 };

/* ---- plumbing ------------------------------------------------------------ */

static void say(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static int i2c_fd = -1;

/*
 * Every transfer names its own address and no I2C_SLAVE ioctl is used at all.
 *
 * That is the whole fix for a round of EIO failures. I2C_SLAVE (or _FORCE)
 * binds the *file descriptor* to one client, and on this adapter that left
 * later I2C_RDWR transfers failing with EIO even though i2ctransfer -- which
 * does exactly this, one address per message -- worked from the shell every
 * time. Putting the address in each message also means the XL9555 can be
 * reached while pca953x owns it, without a force ioctl: the messages simply
 * address 0x20 for the length of the reset pulse.
 */
static int i2c_xfer(struct i2c_msg *msgs, int n)
{
	struct i2c_rdwr_ioctl_data msgset;

	msgset.msgs = msgs;
	msgset.nmsgs = n;

	if (ioctl(i2c_fd, I2C_RDWR, &msgset) < 0) {
		if (verbose)
			say("tbtns: I2C_RDWR addr %02X n=%d failed: %s",
			    msgs[0].addr, n, strerror(errno));
		return -1;
	}
	return 0;
}

static int i2c_write(int addr, const unsigned char *buf, int n)
{
	struct i2c_msg msg;

	msg.addr = addr;
	msg.flags = 0;
	msg.len = n;
	msg.buf = (unsigned char *)buf;

	return i2c_xfer(&msg, 1);
}

/* Two-byte address, then read back. */
static int i2c_read_reg(int addr, unsigned reg, unsigned char *buf, int n)
{
	unsigned char cmd[2] = { (unsigned char)(reg >> 8),
				 (unsigned char)(reg & 0xFF) };
	struct i2c_msg msgs[2];

	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = cmd;

	msgs[1].addr = addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = n;
	msgs[1].buf = buf;

	return i2c_xfer(msgs, 2);
}

/* ---- controller bring-up ------------------------------------------------- */

/*
 * Pulse the reset line through the XL9555.
 *
 * The expander is already bound as a gpiochip and leaves this bit high, so a
 * controller that has never been reset may still answer -- but it does not
 * reliably start reporting. LilyGo's init holds it low 20 ms and high 60 ms and
 * that is what is done here, straight through the I2C expander rather than
 * through gpiolib, so there is no dependency on libgpiod.
 */
static void hw_reset(void)
{
	unsigned char buf[2];
	unsigned char cur = 0;

	/* Read-modify-write so no other expander bit is disturbed: P06 and P07
	 * are the haptic and display enables. */
	if (i2c_read_reg(XL9555_ADDR, XL9555_OUT1, &cur, 1) < 0) {
		say("tbtns: cannot read XL9555; continuing without a reset");
		return;
	}

	/* {register, value} -- writing a bare value here would land it in the
	 * wrong register and silently disturb the haptic or display enable. */
	buf[0] = XL9555_OUT1;
	buf[1] = cur & ~XL9555_RST_BIT;
	if (i2c_write(XL9555_ADDR, buf, 2) < 0)
		return;
	usleep(20 * 1000);

	buf[0] = XL9555_OUT1;
	buf[1] = cur | XL9555_RST_BIT;
	if (i2c_write(XL9555_ADDR, buf, 2) < 0)
		return;
	usleep(60 * 1000);
}

static int touch_init(void)
{
	unsigned char w2[2];
	unsigned char u[4] = { 0 };
	int attempt, ok = 0;

	hw_reset();

	/* 1. command mode */
	w2[0] = REG_MODE; w2[1] = MODE_CMD;
	if (i2c_write(TOUCH_ADDR, w2, 2) < 0)
		return -1;

	/* 2. unlock, written twice as the vendor sequence does */
	w2[0] = REG_MODE; w2[1] = MODE_UNLOCK;
	for (attempt = 0; attempt < 2; attempt++) {
		if (i2c_write(TOUCH_ADDR, w2, 2) < 0)
			return -1;
		usleep(5 * 1000);
	}

	/* 3. it must acknowledge: byte 1 of 0x0002 reads back as 0x1E */
	for (attempt = 0; attempt < 3 && !ok; attempt++) {
		if (i2c_read_reg(TOUCH_ADDR, REG_UNLOCK, u, 4) == 0 && u[1] == MODE_UNLOCK)
			ok = 1;
		else
			usleep(10 * 1000);
	}

	if (!ok) {
		say("tbtns: controller did not acknowledge the unlock "
		    "(read %02X %02X %02X %02X); it will answer but never report",
		    u[0], u[1], u[2], u[3]);
		return -1;
	}

	/* 4. normal mode: start scanning */
	w2[0] = REG_MODE; w2[1] = MODE_NORMAL;
	if (i2c_write(TOUCH_ADDR, w2, 2) < 0)
		return -1;

	usleep(10 * 1000);
	say("tbtns: CST9217 up at 0x%02X, unlocked, scanning", TOUCH_ADDR);
	return 0;
}

/* ---- uinput -------------------------------------------------------------- */

static int ufd = -1;

static int uinput_open(void)
{
	struct uinput_setup usetup;
	int i;

	if (dry_run) {
		say("tbtns: dry run, no input device");
		return 0;
	}

	ufd = open(DEV_UINPUT, O_WRONLY | O_NONBLOCK);
	if (ufd < 0) {
		perror("open " DEV_UINPUT);
		return -1;
	}

	if (ioctl(ufd, UI_SET_EVBIT, EV_KEY) < 0 ||
	    ioctl(ufd, UI_SET_EVBIT, EV_SYN) < 0) {
		perror("UI_SET_EVBIT");
		goto fail;
	}

	/* Only the three buttons. Declaring the whole BTN_ range would make this
	 * look like a pointer or gamepad to anything that inspects the device,
	 * and these are exactly three on/off switches. */
	for (i = 0; i < 3; i++) {
		if (ioctl(ufd, UI_SET_KEYBIT, codes[i]) < 0) {
			perror("UI_SET_KEYBIT");
			goto fail;
		}
	}

	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_HOST;
	usetup.id.vendor = 0x1a1a;
	usetup.id.product = 0x0003;
	usetup.id.version = 1;
	snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "touch-buttons");

	if (ioctl(ufd, UI_DEV_SETUP, &usetup) < 0) {
		perror("UI_DEV_SETUP");
		goto fail;
	}
	if (ioctl(ufd, UI_DEV_CREATE) < 0) {
		perror("UI_DEV_CREATE");
		goto fail;
	}

	say("tbtns: input device created (\"touch-buttons\", BTN_0/1/2)");
	return 0;

fail:
	close(ufd);
	ufd = -1;
	return -1;
}

static void emit(int type, int code, int value)
{
	struct input_event ev;

	if (ufd < 0)
		return;

	memset(&ev, 0, sizeof(ev));
	gettimeofday(&ev.time, NULL);
	ev.type = type;
	ev.code = code;
	ev.value = value;

	if (write(ufd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev))
		perror("write uinput");
}

/* ---- decoding ------------------------------------------------------------ */

static long long now_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int clamp_ctrl(int v)
{
	if (v < 0)
		return 0;
	if (v > 431)
		return 431;
	return v;
}

/*
 * Which button a corrected controller column falls in.
 *
 * Boundaries are the painted ones, not thirds of the raw range: the bar under
 * the finger is the button that fires, which is the only property that matters
 * when the regions are this coarse.
 */
static int band_for(int ctrl_x)
{
	if (ctrl_x < edge1)
		return 0;
	if (ctrl_x < edge2)
		return 1;
	return 2;
}

/* Latched state. -1 means no button is down. */
static int held = -1;

/*
 * Read-outcome counters.
 *
 * These exist because on this bit-banged bus there are three different ways a
 * poll can tell you nothing, and only one of them is the sensor saying "no
 * finger". Without counting them apart, a run where every transfer NAKed looks
 * exactly like a run where the glass was never touched, and the two need
 * opposite fixes.
 *
 *   eio      the transfer itself failed
 *   blank    the read succeeded but every byte was 0xFF
 *   idle     a well-formed "no new report" block (0xAB in byte 0 and byte 6)
 *   fresh    a report carrying an event and coordinates
 *   ackfail  the read worked but the acknowledge that advances the buffer did not
 */
static unsigned long n_eio, n_blank, n_idle, n_fresh, n_ackfail;
static long long stats_at;

static void press(int b)
{
	if (held == b)
		return;
	if (held >= 0) {
		emit(EV_KEY, codes[held], 0);
		say("tbtns: release %d", held);
	}
	emit(EV_KEY, codes[b], 1);
	emit(EV_SYN, SYN_REPORT, 0);
	say("tbtns: press %d", b);
	held = b;
}

static void release(void)
{
	if (held < 0)
		return;
	emit(EV_KEY, codes[held], 0);
	emit(EV_SYN, SYN_REPORT, 0);
	say("tbtns: release %d", held);
	held = -1;
}

int main(int argc, char **argv)
{
	unsigned char blk[BLOCK_LEN];
	unsigned char ack[3] = { (unsigned char)(REG_REPORT >> 8),
				 (unsigned char)(REG_REPORT & 0xFF), REG_ACKBYTE };
	long long down_at = 0;
	int opt, fresh_seen = 0;

	while ((opt = getopt(argc, argv, "vnok:e:y:s:t:C:")) != -1) {
		switch (opt) {
		case 'v': verbose = 1; break;
		case 'n': dry_run = 1; break;
		case 'o': oneshot = 1; dry_run = 1; verbose = 1; break;
		case 'C': classify_only = atoi(optarg); do_classify = 1; break;
		case 'k':
			if (sscanf(optarg, "%d,%d,%d", &codes[0], &codes[1], &codes[2]) != 3) {
				say("tbtns: -k needs three comma-separated keycodes");
				return 2;
			}
			break;
		case 'e':
			if (sscanf(optarg, "%d,%d", &edge1, &edge2) != 2 || edge1 >= edge2) {
				say("tbtns: -e needs two increasing columns");
				return 2;
			}
			break;
		case 'y': y_strip = atoi(optarg); break;
		case 's':
			if (sscanf(optarg, "%lf,%lf", &xs, &xb) != 2 || xs == 0.0) {
				say("tbtns: -s needs scale,offset");
				return 2;
			}
			break;
		case 't': release_ms = atoi(optarg); break;
		default:
			say("tbtns: usage: tbtns [-v] [-n] [-o] [-k a,b,c] "
			    "[-e x1,x2] [-y row] [-s scale,offset] [-t ms]");
			return 2;
		}
	}

	/* Classify-only: no device access at all, so this runs on a build host. */
	if (do_classify) {
		int c = clamp_ctrl(classify_only);

		printf("%d\n", band_for(c));
		return 0;
	}

	i2c_fd = open(DEV_I2C, O_RDWR);
	if (i2c_fd < 0) {
		perror("open " DEV_I2C);
		return 1;
	}

	if (touch_init() < 0) {
		close(i2c_fd);
		return 1;
	}

	if (uinput_open() < 0) {
		close(i2c_fd);
		return 1;
	}

	say("tbtns: bands ctrl %d / %d, strip from row %d, timeout %d ms",
	    edge1, edge2, y_strip, release_ms);

	for (;;) {
		int fingers, event;
		int raw_x, raw_y, cx, cy;
		int is_fresh;

		/*
		 * One retry, but with a gap before it.
		 *
		 * The controller needs settling time between transactions on this
		 * bit-banged bus: a back-to-back second attempt against the same
		 * NAK fails the same way, which turns a transient into a
		 * permanent-looking failure. 5 ms is far below the 66 ms report
		 * cadence, so a retry costs nothing that matters.
		 */
		if (i2c_read_reg(TOUCH_ADDR, REG_REPORT, blk, BLOCK_LEN) < 0) {
			n_eio++;
			usleep(5 * 1000);
			if (i2c_read_reg(TOUCH_ADDR, REG_REPORT, blk, BLOCK_LEN) < 0) {
				n_eio++;
				/*
				 * A dropped read on this bit-banged bus is
				 * not news, and it is emphatically not a
				 * release: treating it as one would turn a
				 * busy bus into flickering buttons. Skip the
				 * poll and let the timeout decide.
				 */
				if (verbose)
					say("tbtns: read failed (skipped)");
				usleep(POLL_MS * 1000);
				continue;
			}
		}

		/*
		 * Acknowledge immediately, with nothing between the read and the
		 * ack. This is the vendor sequence and it is not cosmetic: the ack
		 * is what tells the controller the block has been consumed, and an
		 * ack that does not follow the read directly leaves the buffer
		 * unadvanced so the same report is handed back forever.
		 */
		if (i2c_write(TOUCH_ADDR, ack, 3) < 0)
			n_ackfail++;

		/*
		 * A fresh report is signalled by 0xAB in the last byte with
		 * something other than 0xAB or 0x00 in the first. Anything else
		 * means "no new report" and says nothing about the finger.
		 */
		is_fresh = !(blk[0] == REG_ACKBYTE || blk[6] != REG_ACKBYTE ||
			     blk[0] == 0x00);

		{
			/* Classify the quiet cases for the counters. */
			int i, all_ff = 1;

			for (i = 0; i < BLOCK_LEN; i++)
				if (blk[i] != 0xFF)
					all_ff = 0;

			if (is_fresh)
				n_fresh++;
			else if (all_ff)
				n_blank++;
			else
				n_idle++;
		}

		/*
		 * A periodic census in verbose mode. This is what turns
		 * "nothing happened" into a diagnosable statement: whether the
		 * bus is failing, the controller is muted, or it is genuinely
		 * seeing no finger.
		 */
		if (verbose && now_ms() - stats_at >= 2000) {
			stats_at = now_ms();
			say("tbtns: poll eio=%lu blank=%lu idle=%lu fresh=%lu ackfail=%lu",
			    n_eio, n_blank, n_idle, n_fresh, n_ackfail);
		}

		if (is_fresh) {
			fingers = blk[5] & 0x7F;
			event = blk[0] & 0x0F;

			raw_x = ((int)blk[1] << 4) | (blk[3] >> 4);
			raw_y = ((int)blk[2] << 4) | (blk[3] & 0x0F);

			cx = clamp_ctrl((int)(raw_x / xs + xb));
			cy = (int)(raw_y / ys);

			if (verbose)
				say("tbtns: report ev=%02X fingers=%d raw %d,%d "
				    "ctrl %d,%d%s", event, fingers, raw_x, raw_y,
				    cx, cy, cy >= y_strip ? " STRIP" : "");

			if (oneshot && ++fresh_seen >= 3)
				break;

			if (fingers == 0 || fingers > MAX_FINGERS) {
				release();
			} else if (event == EVT_UP) {
				release();
			} else {
				/*
				 * Any fresh report that is not a lift carries live
				 * coordinates and restarts the timeout: a down, a
				 * move, or a code this program does not name. That
				 * mirrors the vendor logic, which treats the
				 * catch-all case as "still holding, new position".
				 *
				 * Restarting the timer on every fresh report is what
				 * keeps a held finger held: the controller reports on
				 * a 66 ms cadence while the glass is touched, so a
				 * 200 ms timeout only fires when the reports really
				 * stop.
				 */
				if (cy >= y_strip)
					press(band_for(cx));
				else
					release();
				down_at = now_ms();
			}
		} else if (held >= 0 && now_ms() - down_at > release_ms) {
			/*
			 * The only release path that fires in practice: the
			 * controller went quiet. See the header.
			 */
			if (verbose)
				say("tbtns: timeout release");
			release();
		}

		usleep(POLL_MS * 1000);
	}

	release();
	if (ufd >= 0) {
		ioctl(ufd, UI_DEV_DESTROY);
		close(ufd);
	}
	close(i2c_fd);

	return 0;
}
