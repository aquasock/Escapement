// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * i2c-esp32.c -- I2C master driver for the ESP32-S3's hardware I2C controller.
 *
 * Why this exists: the T-Watch Ultra ran its whole peripheral set on i2c-gpio,
 * a bit-banged bus, because this port had no driver for the hardware
 * controller. That was correct as an interim and wrong as a destination --
 * measured on the watch 2026-11-10, driving the touch controller over the
 * bit-banged bus cost 34.6 percent of core 1 in *kernel* time (173 of 182
 * jiffies in sys, 9 in user), because every clock edge is a GPIO toggle taken
 * in software. The bus is also 100 kHz, where the hardware does 400 kHz or
 * more.
 *
 * The controller itself is unremarkable: a command list, a byte FIFO, and
 * timing registers. This driver programs it in polling mode deliberately --
 * no interrupts in the first version. That keeps the risky part small: if the
 * bus does not move, the fault is in the command encoding or the pin routing
 * and there is nothing else to blame. Interrupt-driven operation is a later
 * change once the bus is known to work.
 *
 * Register reference, all from the local ESP-IDF tree:
 *   soc/esp32s3/i2c_periph.c        -- GPIO matrix signals, IRQ source
 *   soc/esp32s3/i2c_reg.h           -- register offsets
 *   soc/esp32s3/i2c_struct.h        -- bitfield layouts
 *   hal/esp32s3/include/hal/i2c_ll.h -- command encoding and timing maths
 *   soc/esp32s3/ld/esp32s3.peripherals.ld -- I2C0 = 0x60013000
 *
 * One trap worth recording: the comment in i2c_struct.h says the opcodes are
 * "0: RSTART; 1: WRITE; 2: READ; 3: STOP; 4: END", which is wrong. The HAL
 * that actually drives this silicon uses RSTART = 6, WRITE = 1, READ = 3,
 * STOP = 2, END = 4. Trusting the comment would produce a driver that runs
 * the command list to completion while never generating a start condition --
 * a silent no-op, which is this project's least favourite failure mode.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#define ESP32_I2C_DRV_NAME	"i2c-esp32"

/* Register offsets, from soc/esp32s3/i2c_reg.h. */
#define I2C_SCL_LOW_PERIOD	0x00
#define I2C_CTR			0x04
#define I2C_SR			0x08
#define I2C_TO			0x0c
#define I2C_SLAVE_ADDR		0x10
#define I2C_FIFO_ST		0x14
#define I2C_FIFO_CONF		0x18
#define I2C_DATA		0x1c
#define I2C_SDA_HOLD		0x30
#define I2C_SDA_SAMPLE		0x34
#define I2C_SCL_HIGH_PERIOD	0x38
#define I2C_SCL_START_HOLD	0x40
#define I2C_SCL_RSTART_SETUP	0x44
#define I2C_SCL_STOP_HOLD	0x48
#define I2C_SCL_STOP_SETUP	0x4c
#define I2C_FILTER_CFG		0x50
#define I2C_INT_CLR		0x24
#define I2C_INT_STATUS		0x2c
#define I2C_CLK_CONF		0x54
#define I2C_COMD(n)		(0x58 + (n) * 4)	/* 8 commands */

/* CTR, from i2c_struct.h (bitpos in the comment). */
#define CTR_SDA_FORCE_OUT	BIT(0)
#define CTR_SCL_FORCE_OUT	BIT(1)
#define CTR_MS_MODE		BIT(4)
#define CTR_TRANS_START		BIT(5)
#define CTR_TX_LSB_FIRST	BIT(6)
#define CTR_RX_LSB_FIRST	BIT(7)
#define CTR_CLK_EN		BIT(8)
#define CTR_ARBITRATION_EN	BIT(9)
#define CTR_FSM_RST		BIT(10)
#define CTR_CONF_UPDATE		BIT(11)

/* INT_STATUS / INT_CLR, from i2c_struct.h int_status_reg_t bit order. */
#define INT_END_DETECT		BIT(3)
#define INT_BYTE_TRANS_DONE	BIT(4)
#define INT_ARB_LOST		BIT(5)
#define INT_TRANS_COMPLETE	BIT(7)
#define INT_TIME_OUT		BIT(8)
#define INT_TRANS_START		BIT(9)
#define INT_NACK		BIT(10)

/* SR. */
#define SR_RESP_REC		BIT(0)		/* ACK received for last byte */
#define SR_ARB_LOST		BIT(2)
#define SR_BUS_BUSY		BIT(3)
#define SR_RXFIFO_CNT_MASK	GENMASK(10, 5)
#define SR_RXFIFO_CNT_SHIFT	5

/* FIFO_CONF. */
#define FIFO_CONF_RX_THRHD(val)	FIELD_PREP(GENMASK(4, 0), (val))
#define FIFO_CONF_TX_THRHD(val)	FIELD_PREP(GENMASK(9, 5), (val))
#define FIFO_CONF_RX_RST	BIT(12)
#define FIFO_CONF_TX_RST	BIT(13)

/*
 * Command word. Layout from i2c_ll_hw_cmd_t in hal/esp32s3/i2c_ll.h:
 *   byte_num [7:0], ack_en [8], ack_exp [9], ack_val [10], op_code [13:11].
 */
#define CMD_BYTE_NUM(n)		FIELD_PREP(GENMASK(7, 0), (n))
#define CMD_ACK_EN		BIT(8)
#define CMD_ACK_EXP		BIT(9)
#define CMD_ACK_VAL		BIT(10)
#define CMD_OP_CODE(op)		FIELD_PREP(GENMASK(13, 11), (op))
#define CMD_DONE		BIT(31)

/* Opcodes from i2c_ll.h -- NOT the stale comment in i2c_struct.h. */
#define OP_RSTART		6
#define OP_WRITE		1
#define OP_READ			3
#define OP_STOP			2
#define OP_END			4

/* GPIO matrix, reused from the SPI driver's approach. */
#define ESP32_IO_MUX_BASE	0x60009000
#define ESP32_GPIO_BASE		0x60004000
#define IO_MUX_OFF(pin)		(0x04 + (pin) * 4)
#define GPIO_OUT_SEL_OFF(pin)	(0x554 + (pin) * 4)
#define GPIO_ENABLE_W1TS_OFF	0x24
#define GPIO_ENABLE1_W1TS_OFF	0x2c
#define IO_MUX_MCU_SEL_MASK	GENMASK(14, 12)
#define IO_MUX_MCU_SEL_GPIO	BIT(12)
#define IO_MUX_FUN_DRV_MASK	GENMASK(11, 10)
#define IO_MUX_FUN_DRV_20MA	BIT(11)
#define IO_MUX_FUN_IE		BIT(9)
#define GPIO_FUNC_OEN_SEL	BIT(10)

/* I2C0's GPIO matrix signals, from soc/esp32s3/gpio_sig_map.h. */
#define SIG_I2CEXT0_SCL		89
#define SIG_I2CEXT0_SDA		90

/* I2C0's interrupt source, from soc/esp32s3/i2c_periph.c. */
#define ESP32_I2C1_PHYS		0x60027000

#define ESP32_I2C_TIMEOUT_MS	1000

struct esp32_i2c {
	void __iomem		*base;
	struct device		*dev;
	struct i2c_adapter	adap;
	struct clk		*clk;
	u32			bus_freq;
	struct mutex		lock;		/* one transaction at a time */
	bool			have_pins;
};

static inline u32 esp32_i2c_read(struct esp32_i2c *i2c, u32 off)
{
	return ioread32(i2c->base + off);
}

static inline void esp32_i2c_write(struct esp32_i2c *i2c, u32 off, u32 val)
{
	iowrite32(val, i2c->base + off);
}

/*
 * Program the SCL timing for `freq`, following i2c_ll_cal_bus_clk().
 *
 * The controller divides a source clock twice: sclk_div_num first, then a
 * half-cycle counter. For 100 kHz off an 80 MHz source the first divider is 1
 * and the half cycle is 400 counts, which is why the arithmetic here is
 * unexciting -- but it has to be right for 400 kHz too, so it is computed
 * rather than hard-coded.
 */
static void esp32_i2c_set_timing(struct esp32_i2c *i2c, u32 freq)
{
	u32 src = i2c->clk ? clk_get_rate(i2c->clk) : 80000000;
	u32 div, sclk, half, wait_high, high, clk_conf, ctr;

	if (!src)
		src = 80000000;

	/*
	 * These are i2c_ll_cal_bus_clk()/i2c_ll_set_scl_timing() from
	 * hal/esp32s3/include/hal/i2c_ll.h, reproduced rather than
	 * approximate. The first version of this function set scl_high_period
	 * and scl_low_period only, left scl_wait_high_period at zero, and the
	 * bus never moved: transactions timed out with the TX FIFO still
	 * holding every byte, which looks like dead hardware and is not.
	 */
	div  = src / (freq * 1024) + 1;
	sclk = src / div;
	half = sclk / freq / 2;
	if (!half)
		half = 1;

	/* Below 80 kHz a large wait_high makes the clock run fast; above it,
	 * half a cycle minus two. Both branches come from the HAL. */
	wait_high = (freq >= 80000) ? (half / 2 - 2) : (half / 4);
	if (wait_high < 1)
		wait_high = 1;
	high = half - wait_high;
	if (high <= wait_high)
		high = wait_high + 1;

	esp32_i2c_write(i2c, I2C_SCL_LOW_PERIOD, half - 1);
	/* scl_high_period [8:0] and scl_wait_high_period [15:9] share a reg. */
	esp32_i2c_write(i2c, I2C_SCL_HIGH_PERIOD,
			FIELD_PREP(GENMASK(8, 0), high - 1) |
			FIELD_PREP(GENMASK(15, 9), wait_high));
	esp32_i2c_write(i2c, I2C_SDA_HOLD, half / 4);
	esp32_i2c_write(i2c, I2C_SDA_SAMPLE, half / 2);
	esp32_i2c_write(i2c, I2C_SCL_START_HOLD, half);
	esp32_i2c_write(i2c, I2C_SCL_RSTART_SETUP, half);
	esp32_i2c_write(i2c, I2C_SCL_STOP_HOLD, half);
	esp32_i2c_write(i2c, I2C_SCL_STOP_SETUP, half);

	/* sclk_div_num is bits [13:8]; the rest of clk_conf stays as it was. */
	clk_conf = esp32_i2c_read(i2c, I2C_CLK_CONF);
	clk_conf &= ~GENMASK(13, 8);
	clk_conf |= FIELD_PREP(GENMASK(13, 8), div - 1);
	esp32_i2c_write(i2c, I2C_CLK_CONF, clk_conf);

	/*
	 * conf_upgate latches the timing registers into the peripheral's own
	 * clock domain. i2c_ll_update() exists for exactly this, and without
	 * it the values above are written and never take effect.
	 */
	ctr = esp32_i2c_read(i2c, I2C_CTR);
	esp32_i2c_write(i2c, I2C_CTR, ctr | CTR_CONF_UPDATE);

	i2c->bus_freq = freq;
}

/*
 * Route SDA/SCL through the GPIO matrix.
 *
 * The pad's pull-up, drive strength and input-enable come from the pinctrl
 * group in the device tree (FUN0_20MA_IE_WPU), which the pinctrl-single driver
 * has already applied by the time we probe. What is missing without this
 * function is the output signal: until GPIO_OUT_SEL names I2CEXT0_SDA/SCL, the
 * pad is an input and the controller drives nothing.
 */
static void esp32_i2c_route_pin(void __iomem *iopad, void __iomem *gpio,
				u32 pin, u16 sig)
{
	u32 v;

	v = ioread32(iopad + IO_MUX_OFF(pin));
	v &= ~(IO_MUX_MCU_SEL_MASK | IO_MUX_FUN_DRV_MASK);
	v |= IO_MUX_MCU_SEL_GPIO | IO_MUX_FUN_DRV_20MA | IO_MUX_FUN_IE;
	iowrite32(v, iopad + IO_MUX_OFF(pin));

	iowrite32(GPIO_FUNC_OEN_SEL | sig, gpio + GPIO_OUT_SEL_OFF(pin));

	if (pin < 32)
		iowrite32(BIT(pin), gpio + GPIO_ENABLE_W1TS_OFF);
	else
		iowrite32(BIT(pin - 32), gpio + GPIO_ENABLE1_W1TS_OFF);
}

static void esp32_i2c_route_pins(struct esp32_i2c *i2c)
{
	struct device *dev = i2c->dev;
	struct device_node *np = dev->of_node;
	void __iomem *iopad, *gpio;
	u32 sda, scl;

	if (of_property_read_u32(np, "espressif,sda-pin", &sda) ||
	    of_property_read_u32(np, "espressif,scl-pin", &scl)) {
		dev_warn(dev, "no sda-pin/scl-pin; assuming pinctrl did the routing\n");
		return;
	}

	iopad = devm_ioremap(dev, ESP32_IO_MUX_BASE, 0x1000);
	gpio = devm_ioremap(dev, ESP32_GPIO_BASE, 0x1000);
	if (!iopad || !gpio) {
		dev_warn(dev, "cannot map IO_MUX/GPIO matrix, pins unrouted\n");
		return;
	}

	esp32_i2c_route_pin(iopad, gpio, sda, SIG_I2CEXT0_SDA);
	esp32_i2c_route_pin(iopad, gpio, scl, SIG_I2CEXT0_SCL);

	/* Read back, because a silent write here is exactly the failure that
	 * costs an evening: the driver reports success and the bus is dead. */
	dev_info(dev, "route sda pin %u -> sig %u: iomux=%08x out_sel=%08x\n",
		 sda, SIG_I2CEXT0_SDA,
		 ioread32(iopad + IO_MUX_OFF(sda)),
		 ioread32(gpio + GPIO_OUT_SEL_OFF(sda)));
	dev_info(dev, "route scl pin %u -> sig %u: iomux=%08x out_sel=%08x\n",
		 scl, SIG_I2CEXT0_SCL,
		 ioread32(iopad + IO_MUX_OFF(scl)),
		 ioread32(gpio + GPIO_OUT_SEL_OFF(scl)));

	i2c->have_pins = true;
}

static void esp32_i2c_reset_fifos(struct esp32_i2c *i2c)
{
	esp32_i2c_write(i2c, I2C_FIFO_CONF,
			FIFO_CONF_RX_THRHD(0) | FIFO_CONF_TX_THRHD(0) |
			FIFO_CONF_RX_RST | FIFO_CONF_TX_RST);
	esp32_i2c_write(i2c, I2C_FIFO_CONF,
			FIFO_CONF_RX_THRHD(0) | FIFO_CONF_TX_THRHD(0));
}

static void esp32_i2c_fsm_reset(struct esp32_i2c *i2c)
{
	u32 ctr = esp32_i2c_read(i2c, I2C_CTR);

	esp32_i2c_write(i2c, I2C_CTR, ctr | CTR_FSM_RST);
	esp32_i2c_write(i2c, I2C_CTR, ctr & ~CTR_FSM_RST);
}

static int esp32_i2c_wait_idle(struct esp32_i2c *i2c)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(ESP32_I2C_TIMEOUT_MS);

	while (esp32_i2c_read(i2c, I2C_SR) & SR_BUS_BUSY) {
		if (time_after(jiffies, timeout)) {
			dev_err(i2c->dev, "bus still busy after %d ms\n",
				ESP32_I2C_TIMEOUT_MS);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}
	return 0;
}

/*
 * Run a prepared command list and wait for the controller to finish.
 *
 * Completion is read from INT_STATUS rather than from a command's `done` bit.
 * The first version of this driver polled `done` on the END command, which
 * does not set it, so every transaction ran to the driver's timeout with the
 * bytes still sitting in the TX FIFO -- indistinguishable from dead hardware.
 * ESP-IDF's own HAL reads the event out of INT_STATUS, so that is what this
 * does: trans_complete means the list retired, nack means a device did not
 * answer, time_out means the bus held SCL too long.
 */
static int esp32_i2c_run(struct esp32_i2c *i2c)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(ESP32_I2C_TIMEOUT_MS);
	u32 ctr, st;

	/* Stale status from a previous transfer would be read as this one's. */
	esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);

	/*
	 * Master mode, clock enable, pad drive and the transfer trigger all go
	 * in ONE write.
	 *
	 * Setting CTR earlier in probe() did not take: the register read back
	 * as 0x20b, the hardware reset default, with ms_mode (bit 4) and
	 * clk_en (bit 8) both clear. In slave mode trans_start is a no-op, so
	 * every transaction timed out with the bytes still queued in the TX
	 * FIFO (sr=0000c000, txfifo_cnt=6) and int_status stayed zero -- which
	 * reads exactly like dead hardware.
	 *
	 * Writing the whole configuration together with trans_start leaves no
	 * window for whatever clears it between probe and use.
	 */
	/* Configure, latch, then trigger -- in that order, in three writes.
	 * The trigger bit applied in the same store as the configuration is
	 * not acted on: measured, the FIFO kept its bytes and int_status
	 * stayed zero. conf_upgate is what moves the configuration into the
	 * peripheral's clock domain, and ESP-IDF's i2c_ll_update() exists for
	 * exactly this step. */
	ctr = CTR_MS_MODE | CTR_CLK_EN | CTR_SDA_FORCE_OUT |
	      CTR_SCL_FORCE_OUT | CTR_ARBITRATION_EN;
	esp32_i2c_write(i2c, I2C_CTR, ctr);
	/* Latch on its own. Both conf_upgate and trans_start are write-triggered
	 * and self-clearing; combining them in one store lets the latch consume
	 * the trigger, which is what the previous attempt did. */
	esp32_i2c_write(i2c, I2C_CTR, ctr | CTR_CONF_UPDATE);
	(void)esp32_i2c_read(i2c, I2C_CTR);	/* flush the write */
	esp32_i2c_write(i2c, I2C_CTR, ctr | CTR_TRANS_START);
	(void)esp32_i2c_read(i2c, I2C_CTR);

	for (;;) {
		st = esp32_i2c_read(i2c, I2C_INT_STATUS);

		if (st & INT_NACK) {
			esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
			return -ENXIO;
		}
		if (st & INT_ARB_LOST) {
			esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
			return -EAGAIN;
		}
		if (st & (INT_TRANS_COMPLETE | INT_TIME_OUT))
			break;

		if (time_after(jiffies, timeout)) {
			dev_err_ratelimited(i2c->dev,
				"I2C timeout: int_status=%08x sr=%08x ctr=%08x\n",
				st, esp32_i2c_read(i2c, I2C_SR),
				esp32_i2c_read(i2c, I2C_CTR));
			esp32_i2c_fsm_reset(i2c);
			esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	if (st & INT_TIME_OUT) {
		esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
		return -ETIMEDOUT;
	}

	esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
	return 0;
}

static int esp32_i2c_xfer_msg(struct esp32_i2c *i2c, struct i2c_msg *msg)
{
	u32 addr = msg->addr << 1;
	int n = 0, i;

	if (msg->len == 0 || msg->len > 255)
		return -EINVAL;

	esp32_i2c_reset_fifos(i2c);

	if (msg->flags & I2C_M_RD) {
		/* [RSTART] [WRITE addr|1, 1 byte] [READ len] [STOP] [END] */
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_RSTART) | CMD_ACK_EN);
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_WRITE) | CMD_BYTE_NUM(1) |
				CMD_ACK_EN | CMD_ACK_EXP);
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_READ) | CMD_BYTE_NUM(msg->len) |
				CMD_ACK_EN | CMD_ACK_EXP | CMD_ACK_VAL);
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_STOP) | CMD_ACK_EN);

		esp32_i2c_write(i2c, I2C_DATA, addr | 1);
	} else {
		/* [RSTART] [WRITE addr, len] [STOP] [END] */
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_RSTART) | CMD_ACK_EN);
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_WRITE) | CMD_BYTE_NUM(msg->len) |
				CMD_ACK_EN | CMD_ACK_EXP);
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_STOP) | CMD_ACK_EN);

		esp32_i2c_write(i2c, I2C_DATA, addr);
		for (i = 0; i < msg->len; i++)
			esp32_i2c_write(i2c, I2C_DATA, msg->buf[i]);
	}

	/* END terminates the list; without it the controller keeps executing
	 * whatever is left in the command registers. */
	esp32_i2c_write(i2c, I2C_COMD(n), CMD_OP_CODE(OP_END));

	if (esp32_i2c_run(i2c))
		return -ETIMEDOUT;

	if (msg->flags & I2C_M_RD) {
		u32 cnt = FIELD_GET(SR_RXFIFO_CNT_MASK,
				    esp32_i2c_read(i2c, I2C_SR));

		if (cnt != msg->len) {
			dev_warn_ratelimited(i2c->dev,
					     "short read: %u of %u bytes\n",
					     cnt, msg->len);
		}
		for (i = 0; i < msg->len && i < cnt; i++)
			msg->buf[i] = esp32_i2c_read(i2c, I2C_DATA) & 0xff;
	}

	return 0;
}

static int esp32_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			  int num)
{
	struct esp32_i2c *i2c = i2c_get_adapdata(adap);
	int i, ret;

	mutex_lock(&i2c->lock);

	ret = esp32_i2c_wait_idle(i2c);
	if (ret)
		goto out;

	for (i = 0; i < num; i++) {
		ret = esp32_i2c_xfer_msg(i2c, &msgs[i]);
		if (ret)
			goto out;
	}

	ret = num;
out:
	mutex_unlock(&i2c->lock);
	return ret;
}

static u32 esp32_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm esp32_i2c_algo = {
	.master_xfer	= esp32_i2c_xfer,
	.functionality	= esp32_i2c_func,
};

static int esp32_i2c_probe(struct platform_device *pdev)
{
	struct esp32_i2c *i2c;
	struct resource *res;
	u32 freq = 100000;
	u32 ctr;
	int ret;

	i2c = devm_kzalloc(&pdev->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c->dev = &pdev->dev;
	mutex_init(&i2c->lock);

	i2c->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(i2c->base))
		return PTR_ERR(i2c->base);

	/*
	 * The clock does two jobs: it gates the peripheral at all, and it sets
	 * the rate the dividers are computed from.
	 *
	 * The gate is the important one. SYSTEM_PERIP_CLK_EN0 bit 7 defaults
	 * to 0 and nothing on this board had ever set it -- the firmware does
	 * not use I2C and i2c-gpio needs no peripheral clock. Without the gate
	 * open, every register write to the controller is discarded and the
	 * peripheral answers as if freshly reset (ctr = 0x20b, master mode
	 * clear), while the driver's writes appear to succeed. That cost two
	 * build cycles to find, so it is enabled explicitly and its rate is
	 * logged rather than assumed.
	 */
	i2c->clk = devm_clk_get_optional(&pdev->dev, NULL);
	if (IS_ERR(i2c->clk)) {
		dev_warn(&pdev->dev, "no clock node, assuming 80 MHz and relying on the gate already being open\n");
		i2c->clk = NULL;
	} else if (i2c->clk) {
		ret = clk_prepare_enable(i2c->clk);
		if (ret) {
			dev_err(&pdev->dev, "cannot enable clock: %d\n", ret);
			return ret;
		}
		dev_info(&pdev->dev, "clock rate %lu Hz\n",
			 clk_get_rate(i2c->clk));
	}

	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &freq);

	/* Master mode, clock enabled, pads held by the controller. Exactly
	 * i2c_ll_master_init()'s CTR value. */
	ctr = CTR_MS_MODE | CTR_CLK_EN | CTR_SDA_FORCE_OUT |
	      CTR_SCL_FORCE_OUT | CTR_ARBITRATION_EN;
	esp32_i2c_write(i2c, I2C_CTR, ctr);

	{
		/*
		 * Deassert this peripheral's reset, then pulse it.
		 *
		 * SYSTEM_PERIP_RST_EN0 (0x600c0020) bit 7 is I2C_EXT0's reset and
		 * it comes out of the bootloader SET -- measured on this board as
		 * 0x9eff1fb8, bit 7 = 1. While it is held, the register file
		 * stays at its power-on defaults and every write is discarded:
		 * CTR wrote 0x31b and read back 0x20b, master mode and clock
		 * enable both clear, so trans_start did nothing and every
		 * transaction ran to timeout with the bytes still in the FIFO.
		 * Reads keep working throughout, and the FIFO even accepts data,
		 * which is what made this look like a command-encoding bug for
		 * two build cycles.
		 *
		 * The clock gate (bit 7 of PERIP_CLK_EN0, offset 0x18) was the
		 * other half of the same story; the DTS now enables it via
		 * i2c0_clk, and both are asserted here so the driver does not
		 * depend on the bootloader's opinion.
		 */
		void __iomem *sys = devm_ioremap(&pdev->dev, 0x600c0000, 0x100);

		if (!sys) {
			dev_err(&pdev->dev, "cannot map the system block\n");
			return -ENOMEM;
		}

		/* clock gate on */
		iowrite32(ioread32(sys + 0x18) | BIT(7), sys + 0x18);
		/* reset released */
		iowrite32(ioread32(sys + 0x20) & ~BIT(7), sys + 0x20);
		/* pulse it so the block starts from a known state */
		iowrite32(ioread32(sys + 0x20) | BIT(7), sys + 0x20);
		udelay(10);
		iowrite32(ioread32(sys + 0x20) & ~BIT(7), sys + 0x20);
		udelay(10);

		dev_info(&pdev->dev,
			 "sys: perip_clk_en0=%08x perip_rst_en0=%08x (bit7=%u)\n",
			 ioread32(sys + 0x18), ioread32(sys + 0x20),
			 !!(ioread32(sys + 0x20) & BIT(7)));
	}

	/* No timeout of our own inside the peripheral; the driver bounds it. */
	esp32_i2c_write(i2c, I2C_TO, 0xfffff);
	esp32_i2c_write(i2c, I2C_FILTER_CFG, 0);
	esp32_i2c_write(i2c, I2C_FIFO_CONF,
			FIFO_CONF_RX_THRHD(0) | FIFO_CONF_TX_THRHD(0));

	esp32_i2c_set_timing(i2c, freq);
	esp32_i2c_route_pins(i2c);

	i2c_set_adapdata(&i2c->adap, i2c);
	i2c->adap.owner		= THIS_MODULE;
	i2c->adap.dev.parent	= &pdev->dev;
	i2c->adap.dev.of_node	= pdev->dev.of_node;
	i2c->adap.algo		= &esp32_i2c_algo;
	i2c->adap.timeout	= msecs_to_jiffies(ESP32_I2C_TIMEOUT_MS);
	strscpy(i2c->adap.name, ESP32_I2C_DRV_NAME, sizeof(i2c->adap.name));

	platform_set_drvdata(pdev, i2c);

	ret = i2c_add_adapter(&i2c->adap);
	if (ret) {
		dev_err(&pdev->dev, "cannot register adapter: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev,
		 "ESP32-S3 I2C master at %pR, %u Hz, ctr=%08x clk_conf=%08x\n",
		 res, i2c->bus_freq,
		 esp32_i2c_read(i2c, I2C_CTR),
		 esp32_i2c_read(i2c, I2C_CLK_CONF));

	return 0;
}

static void esp32_i2c_remove(struct platform_device *pdev)
{
	struct esp32_i2c *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adap);
	if (i2c->clk)
		clk_disable_unprepare(i2c->clk);
}

static const struct of_device_id esp32_i2c_of_match[] = {
	{ .compatible = "espressif,esp32-i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, esp32_i2c_of_match);

static struct platform_driver esp32_i2c_driver = {
	.probe		= esp32_i2c_probe,
	.remove		= esp32_i2c_remove,
	.driver		= {
		.name	= ESP32_I2C_DRV_NAME,
		.of_match_table = esp32_i2c_of_match,
	},
};
module_platform_driver(esp32_i2c_driver);

MODULE_DESCRIPTION("ESP32-S3 hardware I2C master");
MODULE_LICENSE("GPL");
