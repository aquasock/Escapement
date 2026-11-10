// SPDX-License-Identifier: GPL-2.0
/*
 * drv2605.c - TI DRV2605 ERM/LRA haptic driver for the T-Watch Ultra.
 *
 * The chip is at I2C 0x5A and drives an ERM motor. Its rail is gated by the
 * XL9555 expander at 0x20 port 0 bit 6, so a chip with that bit low answers
 * every transaction and never moves.
 *
 * Why this exists as a kernel driver: the click used to be produced by a
 * userspace library (userspace/haptic.c) whose only caller was the touch
 * daemon. When that daemon was retired the library survived with no callers,
 * and the motor went silent -- a regression, not a deliberate removal. Putting
 * the sequence in the kernel means the click survives whatever userspace does,
 * which is what a button press wants.
 *
 * The sequence, in this order, and the order matters:
 *
 *   MODE     0x01 = 0x00   internal trigger, not PWM/RTP/audio
 *   RTP      0x02 = 0x00
 *   LIBRARY  0x03 = 0x01   TS2200 A, the ERM effect set -- library 6 is for
 *                          LRA motors and buzzes weakly on an ERM
 *   WAVESEQ1 0x04 = effect ID
 *   WAVESEQ2 0x05 = 0x00   the sequence is terminated by a zero, so a single
 *                          effect needs this written explicitly
 *   GO       0x0C = 0x01
 *
 * Writing GO with a stale sequence replays whatever was there before, which is
 * how a "tap" ends up buzzing for a second; hence the sequence is rewritten on
 * every tap rather than once at probe.
 *
 * Nothing is polled. The chip plays the effect and stops.
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include <linux/drv2605.h>

#define DRV2605_DRV_NAME	"drv2605"

#define DRV2605_REG_STATUS	0x00
#define DRV2605_REG_MODE	0x01
#define DRV2605_REG_RTP		0x02
#define DRV2605_REG_LIBRARY	0x03
#define DRV2605_REG_WAVESEQ1	0x04
#define DRV2605_REG_WAVESEQ2	0x05
#define DRV2605_REG_GO		0x0c

#define DRV2605_MODE_INTERNAL	0x00
#define DRV2605_LIBRARY_ERM_A	0x01
#define DRV2605_EFFECT_STRONG	0x01	/* Strong Click, 100% */

#define DRV2605_DEVICE_ID_MASK	GENMASK(7, 5)
#define DRV2605_DEVICE_ID		0x07

struct drv2605 {
	struct i2c_client	*client;
	struct mutex		lock;
};

/*
 * One chip on this board, so one pointer. The consumer is a different driver
 * and has no way to look it up, which is why this is a module-level rather
 * than a per-device handle.
 */
static struct drv2605 *drv2605_dev;

static int drv2605_write(struct drv2605 *h, u8 reg, u8 val)
{
	u8 buf[2] = { reg, val };

	return i2c_master_send(h->client, buf, sizeof(buf));
}

/**
 * drv2605_tap() - play a single short click.
 *
 * Safe to call from any context that can sleep, and safe to call before the
 * chip has probed: it returns quietly if there is no device. Callers treat the
 * click as optional feedback, never as something whose failure matters.
 */
void drv2605_tap(void)
{
	struct drv2605 *h = READ_ONCE(drv2605_dev);

	if (!h)
		return;

	mutex_lock(&h->lock);
	drv2605_write(h, DRV2605_REG_WAVESEQ1, DRV2605_EFFECT_STRONG);
	drv2605_write(h, DRV2605_REG_WAVESEQ2, 0x00);
	drv2605_write(h, DRV2605_REG_GO, 0x01);
	mutex_unlock(&h->lock);
}
EXPORT_SYMBOL_GPL(drv2605_tap);

/* Manual trigger, so the chip can be exercised without a touch. */
static ssize_t tap_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	drv2605_tap();
	return count;
}
static DEVICE_ATTR_WO(tap);

static int drv2605_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct drv2605 *h;
	u8 status;
	int ret;

	h = devm_kzalloc(dev, sizeof(*h), GFP_KERNEL);
	if (!h)
		return -ENOMEM;

	h->client = client;
	mutex_init(&h->lock);
	i2c_set_clientdata(client, h);

	ret = i2c_smbus_read_byte_data(client, DRV2605_REG_STATUS);
	if (ret < 0)
		return dev_err_probe(dev, ret, "no answer at 0x%02x\n",
				     client->addr);
	status = ret;

	/*
	 * DEVICE_ID is bits 7:5. Anything else on this address is not the part
	 * this driver knows how to drive, so refuse rather than write effect
	 * IDs into a stranger.
	 */
	if (FIELD_GET(DRV2605_DEVICE_ID_MASK, status) != DRV2605_DEVICE_ID)
		return dev_err_probe(dev, -ENODEV,
				     "not a DRV2605 (status 0x%02x)\n", status);

	ret = i2c_smbus_read_byte_data(client, 0x1a);	/* FEEDBACK */
	if (ret >= 0)
		dev_info(dev, "DRV2605 ready, %s motor\n",
			 (ret & BIT(7)) ? "LRA" : "ERM");

	ret = drv2605_write(h, DRV2605_REG_MODE, DRV2605_MODE_INTERNAL);
	if (ret < 0)
		return ret;
	ret = drv2605_write(h, DRV2605_REG_RTP, 0x00);
	if (ret < 0)
		return ret;
	ret = drv2605_write(h, DRV2605_REG_LIBRARY, DRV2605_LIBRARY_ERM_A);
	if (ret < 0)
		return ret;

	WRITE_ONCE(drv2605_dev, h);

	ret = device_create_file(dev, &dev_attr_tap);
	if (ret)
		dev_warn(dev, "no tap attribute: %d\n", ret);

	dev_info(dev, "library %d, effect %d on tap\n",
		 DRV2605_LIBRARY_ERM_A, DRV2605_EFFECT_STRONG);

	return 0;
}

static void drv2605_remove(struct i2c_client *client)
{
	struct drv2605 *h = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_tap);
	WRITE_ONCE(drv2605_dev, NULL);
	/* Best effort: leave the motor still. */
	drv2605_write(h, DRV2605_REG_GO, 0x00);
}

static const struct of_device_id drv2605_of_match[] = {
	{ .compatible = "ti,drv2605" },
	{ }
};
MODULE_DEVICE_TABLE(of, drv2605_of_match);

static struct i2c_driver drv2605_driver = {
	.driver = {
		.name = DRV2605_DRV_NAME,
		.of_match_table = drv2605_of_match,
	},
	.probe = drv2605_probe,
	.remove = drv2605_remove,
};
module_i2c_driver(drv2605_driver);

MODULE_DESCRIPTION("TI DRV2605 haptic driver");
MODULE_LICENSE("GPL");
