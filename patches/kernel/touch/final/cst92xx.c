// SPDX-License-Identifier: GPL-2.0
/*
 * cst92xx.c - Hynitron CST9217 / CST9220 capacitive touch controller.
 *
 * The T-Watch Ultra fits a CST9217 at I2C 0x1A. There is no mainline driver
 * for it and the part's protocol is not publicly documented, so this is
 * derived from LilyGo's own source rather than from a datasheet:
 *
 *   lewisxhe/SensorLib, src/touch/TouchDrvCST92xx.cpp  (MIT)
 *   Xinyuan-LilyGO/LilyGoLib, src/LilyGoWatchUltra.cpp initTouch()
 *
 * Both were read in full. Where this driver deliberately differs from them,
 * the comment says so and says why; those differences are the result of
 * measurements on this board, not of preference.
 *
 * The five things that are easy to get wrong, all of which cost time here:
 *
 *   1. The reset line is NOT a SoC GPIO. It is P08 of the XL9555 expander at
 *      0x20, and the expander's port 1 comes up as all inputs, so nothing
 *      drives it until it is programmed as an output. A controller that has
 *      never been reset does not report, and does not complain either.
 *
 *   2. Reports live at a 16-bit address, 0xD000. The read is a two-byte
 *      address followed by a repeated start and a 15-byte block, and each
 *      read must be acknowledged with a separate three-byte write of
 *      0xD0 0x00 0xAB. Skipping the ack leaves the controller's buffer
 *      mid-block and it stops advancing.
 *
 *   3. 15 bytes is not a suggestion. The block is two 5-byte finger records
 *      plus 5 header bytes, and the ack that advances the buffer is issued
 *      against the whole block. A short read wedges the buffer.
 *
 *   4. A finger count alone does not mean a press. The controller returns
 *      blocks with a non-zero count and a valid marker while the event nibble
 *      is 0x00, and treating those as presses invents phantom touches at
 *      full-scale. Only event 0x06 is a press.
 *
 *   5. There is no release event. Event 0x07 exists in the protocol but the
 *      part does not emit it when a finger lifts -- it simply stops sending
 *      reports. Release is therefore inferred from a gap, which is why
 *      TOUCH_RELEASE_MS exists and why it is tuned rather than guessed.
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define CST92XX_DRV_NAME	"cst92xx"

#define CST92XX_REG_REPORT	0xd000
#define CST92XX_ACK		0xab
#define CST92XX_BLOCK_LEN	15
#define CST92XX_MAX_FINGERS	2
#define CST92XX_EVENT_DOWN	0x06

/* Command-mode entry and the attribute registers, from getAttribute(). */
#define CST92XX_REG_CMD		0xd101
#define CST92XX_REG_CHECKCODE	0xd1fc
#define CST92XX_REG_RESOLUTION	0xd1f8
#define CST92XX_REG_CHIPTYPE	0xd204
#define CST92XX_REG_FWVERSION	0xd208
#define CST92XX_REG_MODE		0xd109	/* normal mode */

#define CST92XX_CHIP_9217	0x9217
#define CST92XX_CHIP_9220	0x9220
#define CST92XX_CHECKCODE_MASK	0xffff0000
#define CST92XX_CHECKCODE_ID	0xcaca0000

/*
 * Polling periods.
 *
 * The controller produces a report roughly every 66 ms while a finger is down,
 * so polling faster than that only reads the same block again -- and every
 * redundant read costs a bus transaction and, on this part, a fair share of
 * NACKs. 20 ms is comfortably under one report period, which keeps the worst
 * case latency at a fifth of a frame, while producing about a third of the
 * traffic that a 5 ms poll would.
 *
 * This is now the period used ONLY while a finger is down. When nothing is
 * touching, the driver parks and lets the interrupt wake it, so an idle watch
 * generates no bus traffic at all instead of fifty reads a second. The
 * interrupt is on TP_INT, GPIO 12, falling edge, exactly as LilyGo attaches it
 * (`pinMode(TP_INT, INPUT_PULLUP); attachInterrupt(TP_INT, ..., FALLING)`).
 *
 * The idle period below is a safety net, not the normal path. If the interrupt
 * never fires -- wrong polarity, a pin that lost its pull-up, a controller
 * revision that drives INT differently -- the driver degrades to polling this
 * slowly rather than losing touch entirely. 250 ms is four report periods, so
 * a tap still registers, just later; and a DT property can raise it to prove
 * the interrupt is doing the work (see cst92xx_probe).
 */
#define CST92XX_POLL_MS		20
#define CST92XX_IDLE_POLL_MS	250

/*
 * How long a press survives without a fresh report.
 *
 * Measured: the controller reports about every 66 ms while touched. Three
 * report periods is the smallest value that survives one dropped report, and
 * this is set to four because the cost of being slightly late is a few
 * milliseconds of extra drag, while the cost of being early is a touch that
 * stutters mid-drag.
 */
#define CST92XX_RELEASE_MS	250

struct cst92xx {
	struct i2c_client	*client;
	struct input_dev	*input;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*int_gpio;
	struct delayed_work	poll;
	struct mutex		lock;		/* one transfer at a time */
	u16			max_x;
	u16			max_y;
	bool			touching;
	unsigned long		last_report;
	int			irq;
	u32			idle_poll_ms;
	u32			irq_count;	/* diagnostics: reports seen from the IRQ */
};

/*
 * Read n bytes from a 16-bit register: two address bytes, repeated start, n
 * bytes back. This maps onto one command list in the ESP32 I2C driver, which
 * is what makes the 15-byte report read fit in the controller's eight command
 * registers.
 */
static int cst92xx_read_reg(struct cst92xx *ts, u16 reg, u8 *buf, size_t len)
{
	u8 addr[2] = { reg >> 8, reg & 0xff };
	struct i2c_msg msgs[2] = {
		{ .addr = ts->client->addr, .flags = 0,
		  .len = sizeof(addr), .buf = addr },
		{ .addr = ts->client->addr, .flags = I2C_M_RD,
		  .len = len, .buf = buf },
	};

	return i2c_transfer(ts->client->adapter, msgs, ARRAY_SIZE(msgs));
}

static int cst92xx_write_cmd(struct cst92xx *ts, u16 reg)
{
	u8 cmd[2] = { reg >> 8, reg & 0xff };

	return i2c_master_send(ts->client, cmd, sizeof(cmd));
}

/*
 * Release the controller from reset through the expander.
 *
 * LilyGo pulses it low for 20 ms and high for 60 ms, and passes -1 as the
 * touch driver's reset pin because the line is not an SoC GPIO at all. The
 * timings are theirs; the expander's own output register is handled by the
 * gpio consumer API rather than by hand, so a failed reset is reported here
 * instead of silently leaving the part un-initialised.
 */
static int cst92xx_reset(struct cst92xx *ts)
{
	gpiod_set_value_cansleep(ts->reset_gpio, 1);	/* assert, active low */
	msleep(20);
	gpiod_set_value_cansleep(ts->reset_gpio, 0);	/* release */
	msleep(60);

	return 0;
}

/*
 * getAttribute(), in the same order and with the same checks.
 *
 * LilyGo fails init if any of these disagree, and that is the right behaviour:
 * a controller with bad firmware or an unexpected part number will accept
 * writes and never report, and a probe that continues hopefully turns that
 * into a phantom-input bug later.
 */
static int cst92xx_init_chip(struct cst92xx *ts)
{
	struct device *dev = &ts->client->dev;
	u8 buf[8];
	u32 checkcode, fw;
	u16 chiptype;
	int ret;

	ret = cst92xx_write_cmd(ts, CST92XX_REG_CMD);
	if (ret < 0)
		return ret;
	msleep(10);

	ret = cst92xx_read_reg(ts, CST92XX_REG_CHECKCODE, buf, 4);
	if (ret < 0)
		return ret;
	checkcode = (u32)buf[3] << 24 | (u32)buf[2] << 16 |
		    (u32)buf[1] << 8 | buf[0];
	if ((checkcode & CST92XX_CHECKCODE_MASK) != CST92XX_CHECKCODE_ID) {
		dev_err(dev, "bad checkcode 0x%08x\n", checkcode);
		return -ENODEV;
	}

	ret = cst92xx_read_reg(ts, CST92XX_REG_RESOLUTION, buf, 4);
	if (ret < 0)
		return ret;
	ts->max_x = (u16)(buf[1] << 8 | buf[0]);
	ts->max_y = (u16)(buf[3] << 8 | buf[2]);

	ret = cst92xx_read_reg(ts, CST92XX_REG_CHIPTYPE, buf, 4);
	if (ret < 0)
		return ret;
	chiptype = (u16)(buf[3] << 8 | buf[2]);
	if (chiptype != CST92XX_CHIP_9217 && chiptype != CST92XX_CHIP_9220) {
		dev_err(dev, "unexpected chip type 0x%04x\n", chiptype);
		return -ENODEV;
	}

	/* Firmware check: 0xa5a5a5a5 means the part has none loaded. */
	ret = cst92xx_read_reg(ts, CST92XX_REG_FWVERSION, buf, 8);
	if (ret < 0)
		return ret;
	fw = (u32)buf[3] << 24 | (u32)buf[2] << 16 | (u32)buf[1] << 8 | buf[0];
	if (fw == 0xa5a5a5a5) {
		dev_err(dev, "no firmware in the controller\n");
		return -ENODEV;
	}

	/*
	 * Normal mode. LilyGo does not do this -- its only setMode() caller is
	 * sleep() -- but a report read taken while the part is in command mode
	 * returns 15 bytes of 0xff, and a read taken straight after the command
	 * write can return a stale attribute block. Selecting normal mode and
	 * reading 0x0002 back is the handshake that confirms the part actually
	 * changed state rather than merely accepting the write.
	 */
	ret = cst92xx_write_cmd(ts, CST92XX_REG_MODE);
	if (ret < 0)
		return ret;
	msleep(10);

	ret = cst92xx_read_reg(ts, 0x0002, buf, 2);
	if (ret < 0)
		return ret;
	if (buf[1] != 0x09)
		dev_warn(dev, "mode readback 0x%02x, expected 0x09\n", buf[1]);

	dev_info(dev, "CST%04x at 0x%02x, %ux%u, fw 0x%08x\n",
		 chiptype, ts->client->addr, ts->max_x, ts->max_y, fw);

	return 0;
}

/*
 * One report.
 *
 * The ack is sent whether or not the read succeeded. A read whose ack is
 * skipped leaves the controller's buffer un-advanced, which is worse than an
 * extra write: the next read returns the same block and a release is never
 * seen.
 */
static int cst92xx_read_report(struct cst92xx *ts, u8 *blk)
{
	u8 ack[3] = { CST92XX_REG_REPORT >> 8, CST92XX_REG_REPORT & 0xff,
		      CST92XX_ACK };
	int ret;

	ret = cst92xx_read_reg(ts, CST92XX_REG_REPORT, blk, CST92XX_BLOCK_LEN);
	i2c_master_send(ts->client, ack, sizeof(ack));

	return ret < 0 ? ret : 0;
}

static void cst92xx_report_touch(struct cst92xx *ts, u8 *blk)
{
	struct input_dev *input = ts->input;
	u8 *p = blk;		/* finger 0 is at offset 0 */
	unsigned int x, y;

	x = (p[1] << 4) | (p[3] >> 4);
	y = (p[2] << 4) | (p[3] & 0x0f);

	if (x > ts->max_x)
		x = ts->max_x;
	if (y > ts->max_y)
		y = ts->max_y;

	input_report_abs(input, ABS_X, x);
	input_report_abs(input, ABS_Y, y);
	input_report_key(input, BTN_TOUCH, 1);
	input_sync(input);

	ts->touching = true;
	ts->last_report = jiffies;
}

static void cst92xx_report_release(struct cst92xx *ts)
{
	input_report_key(ts->input, BTN_TOUCH, 0);
	input_sync(ts->input);
	ts->touching = false;
}

/*
 * One read-and-report cycle, shared by the poll and the interrupt path.
 *
 * Returns 1 if a point is currently held, 0 otherwise, so the caller can pick
 * the next wake-up period. Must be called with ts->lock held.
 */
static int cst92xx_service(struct cst92xx *ts)
{
	u8 blk[CST92XX_BLOCK_LEN];
	int ret, points;

	ret = cst92xx_read_report(ts, blk);
	if (!ret) {
		/*
		 * A failed read is not a release. The controller NACKs while it
		 * has nothing to hand over, and those gaps sit between reports --
		 * treating them as a lift would make every drag stutter. The
		 * release timer below is what ends a touch.
		 */
		if (blk[0] != CST92XX_ACK && blk[0] != 0x00) {
			/*
			 * blk[6] is the block's own validity marker. LilyGo
			 * requires 0xAB and returns no points otherwise, which
			 * rejects blocks that carry a plausible finger count but
			 * are not a report.
			 */
			points = blk[5] & 0x7f;
			/*
			 * Only event 0x06 is a press. A non-zero finger count with
			 * event 0x00 is not a touch: counting it invents a phantom
			 * at full-scale.
			 */
			if (blk[6] == CST92XX_ACK && points > 0 &&
			    points <= CST92XX_MAX_FINGERS &&
			    (blk[0] & 0x0f) == CST92XX_EVENT_DOWN)
				cst92xx_report_touch(ts, blk);
		}
	}

	/* Release by elapsed time, since the part emits no release event. */
	if (ts->touching &&
	    time_after(jiffies, ts->last_report +
		       msecs_to_jiffies(CST92XX_RELEASE_MS))) {
		cst92xx_report_release(ts);
		return 0;
	}

	return ts->touching;
}

/*
 * Interrupt: TP_INT is active low and the controller pulses it when it has a
 * report. The handler is threaded because reading the report is an I2C
 * transfer, which sleeps.
 *
 * IRQF_ONESHOT keeps the line masked until this function returns, so a
 * controller that holds INT low until it is read cannot re-enter here.
 */
static irqreturn_t cst92xx_irq(int irq, void *data)
{
	struct cst92xx *ts = data;
	int holding;

	mutex_lock(&ts->lock);
	ts->irq_count++;
	holding = cst92xx_service(ts);
	mutex_unlock(&ts->lock);

	/* Keep the fast poll running while a finger is down. */
	mod_delayed_work(system_wq, &ts->poll,
			 msecs_to_jiffies(holding ? CST92XX_POLL_MS
						  : ts->idle_poll_ms));

	return IRQ_HANDLED;
}

static void cst92xx_poll(struct work_struct *work)
{
	struct cst92xx *ts = container_of(work, struct cst92xx, poll.work);
	int holding;

	mutex_lock(&ts->lock);
	holding = cst92xx_service(ts);
	mutex_unlock(&ts->lock);

	mod_delayed_work(system_wq, &ts->poll,
			 msecs_to_jiffies(holding ? CST92XX_POLL_MS
						  : ts->idle_poll_ms));
}

static int cst92xx_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct cst92xx *ts;
	int ret, irq;

	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->client = client;
	mutex_init(&ts->lock);
	i2c_set_clientdata(client, ts);

	ts->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ts->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ts->reset_gpio),
				     "cannot get the reset GPIO\n");

	/*
	 * INPUT_PULLUP, as LilyGo does: the controller drives INT low and
	 * releases it, so the line needs the pull to return high. Without it the
	 * pin floats and the edge either never arrives or arrives constantly.
	 */
	ts->int_gpio = devm_gpiod_get_optional(dev, "int", GPIOD_IN);
	if (IS_ERR(ts->int_gpio))
		return dev_err_probe(dev, PTR_ERR(ts->int_gpio),
				     "cannot get the INT GPIO\n");

	ts->input = devm_input_allocate_device(dev);
	if (!ts->input)
		return -ENOMEM;

	ts->input->name = "cst92xx-touch";
	ts->input->id.bustype = BUS_I2C;
	ts->input->dev.parent = dev;

	__set_bit(INPUT_PROP_DIRECT, ts->input->propbit);
	input_set_abs_params(ts->input, ABS_X, 0, 4095, 0, 0);
	input_set_abs_params(ts->input, ABS_Y, 0, 4095, 0, 0);
	input_set_capability(ts->input, EV_KEY, BTN_TOUCH);
	input_set_capability(ts->input, EV_ABS, ABS_X);
	input_set_capability(ts->input, EV_ABS, ABS_Y);

	ret = cst92xx_reset(ts);
	if (ret)
		return ret;

	ret = cst92xx_init_chip(ts);
	if (ret)
		return ret;

	/* Now that the part's own resolution is known, publish it. */
	input_set_abs_params(ts->input, ABS_X, 0, ts->max_x, 0, 0);
	input_set_abs_params(ts->input, ABS_Y, 0, ts->max_y, 0, 0);

	ret = input_register_device(ts->input);
	if (ret)
		return dev_err_probe(dev, ret, "cannot register input device\n");

	/*
	 * The idle period is a property so the interrupt can be proved rather
	 * than assumed: setting it to a large value makes the slow poll
	 * effectively never fire, so touch working at all is then evidence the
	 * interrupt is doing the work. Default 250 ms.
	 */
	if (device_property_read_u32(dev, "hynitron,idle-poll-ms",
				     &ts->idle_poll_ms))
		ts->idle_poll_ms = CST92XX_IDLE_POLL_MS;
	if (ts->idle_poll_ms < CST92XX_POLL_MS)
		ts->idle_poll_ms = CST92XX_POLL_MS;

	INIT_DELAYED_WORK(&ts->poll, cst92xx_poll);

	/*
	 * The interrupt is optional on purpose. If it cannot be acquired -- no
	 * interrupt in the device tree, or a conflict -- the driver keeps
	 * working on the poll above rather than failing probe. A watch whose
	 * touch is slower is better than a watch with no touch at all.
	 *
	 * client->irq is normally filled in by the i2c core from the device
	 * tree, but on this kernel it arrives as 0, so the mapping is done
	 * here as well. Both values are logged: if the core ever starts
	 * providing it, the second lookup simply does not happen.
	 */
	/*
	 * The interrupt comes from the GPIO, not from the DT `interrupts`
	 * property. gpiod_to_irq() asks the gpiochip to map its own line, which
	 * is the idiom every input driver uses and which also gives control of
	 * the pin's electrical configuration -- LilyGo sets INPUT_PULLUP before
	 * attaching the handler, and the pull is part of the contract rather
	 * than a default worth inheriting.
	 *
	 * This replaces an earlier attempt that read the DT interrupt specifier
	 * directly. That path parsed correctly (hwirq 12, falling edge, parent
	 * gpio0) but the mapping step returned 0, so no virq was ever produced,
	 * while the RTC's line 1 mapped fine through the same domain. Going
	 * through the gpiochip avoids whatever that asymmetry is and is the
	 * supported interface besides.
	 */
	irq = -ENOENT;
	if (ts->int_gpio)
		irq = gpiod_to_irq(ts->int_gpio);

	if (irq == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	dev_info(dev, "irq: client=%d gpiod_to_irq=%d\n", client->irq, irq);

	ts->irq = -ENOENT;
	if (irq > 0) {
		ret = devm_request_threaded_irq(dev, irq, NULL, cst92xx_irq,
						IRQF_ONESHOT |
						IRQF_TRIGGER_FALLING |
						IRQF_NO_AUTOEN,
						CST92XX_DRV_NAME, ts);
		if (ret) {
			dev_warn(dev, "no interrupt (%d), polling only\n", ret);
		} else {
			ts->irq = irq;
			enable_irq(ts->irq);
		}
	}

	/*
	 * Start the poll rather than waiting for an interrupt. That way the
	 * first report is seen even if the controller asserted INT before this
	 * driver existed, and a dead interrupt line is invisible in practice.
	 */
	mod_delayed_work(system_wq, &ts->poll, 0);

	dev_info(dev, "idle %u ms, active %d ms, release %d ms, %s\n",
		ts->idle_poll_ms, CST92XX_POLL_MS, CST92XX_RELEASE_MS,
		ts->irq > 0 ? "interrupt driven" : "polling only");

	return 0;
}

static void cst92xx_remove(struct i2c_client *client)
{
	struct cst92xx *ts = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&ts->poll);
}

static const struct of_device_id cst92xx_of_match[] = {
	{ .compatible = "hynitron,cst92xx" },
	{ }
};
MODULE_DEVICE_TABLE(of, cst92xx_of_match);

static struct i2c_driver cst92xx_driver = {
	.driver = {
		.name = CST92XX_DRV_NAME,
		.of_match_table = cst92xx_of_match,
	},
	.probe = cst92xx_probe,
	.remove = cst92xx_remove,
};
module_i2c_driver(cst92xx_driver);

MODULE_DESCRIPTION("Hynitron CST9217/CST9220 capacitive touch controller");
MODULE_LICENSE("GPL");
