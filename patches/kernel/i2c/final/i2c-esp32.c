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
#define I2C_INT_RAW		0x20
#define I2C_INT_CLR		0x24
#define I2C_INT_ENA		0x28
#define I2C_INT_STATUS		0x2c
/* SCL_SP_CONF is at 0x80, NOT 0x60, and its fields are scl_rst_slv_en at
 * bit 0 with scl_rst_slv_num at [5:1]. Both were wrong on the first attempt,
 * which is the third time a register layout has been guessed rather than read
 * out of soc/esp32s3/i2c_struct.h. */
#define I2C_SCL_SP_CONF		0x80
#define SCL_SP_RST_NUM		9	/* recovery clocks */
#define SCL_SP_RST_EN		BIT(0)
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
/*
 * The two SCL timeouts, which the first version of this driver did not test at
 * all. Bit 13 is the slave-stretch timeout -- a slave held SCL longer than the
 * TO register allows -- and bit 14 is the FSM sitting in a main state too long.
 * A touch controller preparing a report is exactly what produces these, and
 * ignoring them made a stretched bus look like a slow one: the transfer ran to
 * the driver's own per-transfer timeout and left the bus wedged.
 */
#define INT_SCL_ST_TO		BIT(13)
#define INT_SCL_MAIN_ST_TO	BIT(14)

/*
 * SR, from the field list in soc/esp32s3/i2c_struct.h:
 *   resp_rec [0], slave_rw [1], (reserved) [2], arb_lost [3], bus_busy [4],
 *   slave_addressed [5], rxfifo_cnt [13:8], txfifo_cnt [23:18].
 *
 * arb_lost and bus_busy were previously defined one bit low, which meant
 * SR_BUS_BUSY was actually reading arb_lost: a genuinely stuck bus never
 * triggered recovery, and the arbitration-lost path never fired either.
 */
#define SR_RESP_REC		BIT(0)		/* ACK received for last byte */
#define SR_ARB_LOST		BIT(3)
#define SR_BUS_BUSY		BIT(4)
/* rxfifo_cnt is bits [13:8]. An earlier version used [10:5], taken from a
 * truncated comment, and reported "56 of 7 bytes" for every read. */
#define SR_RXFIFO_CNT_MASK	GENMASK(13, 8)
#define SR_RXFIFO_CNT_SHIFT	8

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
/*
 * The ACK bits, per the ESP32-S3 TRM's I2C command structure:
 *   ack_en   [8]  -- check the slave's ACK and report the result
 *   ack_exp  [9]  -- the ACK value the slave is EXPECTED to return
 *   ack_val  [10] -- the value the MASTER drives on the last received byte
 *                    0 = ACK, 1 = NACK
 */
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
#define GPIO_IN_SEL_OFF(sig)	(0x154 + (sig) * 4)	/* FUNC0_IN_SEL_CFG */
#define GPIO_IN_SEL_GPIO	BIT(7)	/* take the input from the pad, not the matrix */
#define GPIO_PIN_OFF(pin)	(0x74 + (pin) * 4)	/* PIN0_REG */
#define GPIO_PIN_PAD_DRIVER	BIT(2)	/* open drain */

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
	void __iomem		*sys;		/* SYSTEM block, for gate + reset */
	bool			debug;
	/*
	 * A hard budget for the diagnostic lines below, rather than
	 * printk_ratelimit(). The hardware I2C is polled as often as 66 times a
	 * second by tbtns, which permanently exhausts the shared ratelimit
	 * state -- so every diagnostic in this driver went silent exactly when
	 * it mattered. A counter is bounded (the console mirrors to fbcon, so
	 * an unbounded dump costs a minute of boot) and cannot be starved.
	 */
	u32			dbg_budget;
	u32			ok_budget;
	bool			first_err;
};

/*
 * Two separate bounded budgets, because they answer different questions and
 * one must not starve the other. The success counter shows whether the bus is
 * carrying traffic; the error counter shows how it breaks. A single shared
 * counter was spent entirely during probe, which is exactly when nothing was
 * wrong.
 */
#define DBG(i2c, fmt, ...)					\
	do {							\
		if ((i2c)->dbg_budget) {				\
			(i2c)->dbg_budget--;				\
			dev_err((i2c)->dev, fmt, ##__VA_ARGS__);	\
		}						\
	} while (0)

#define DBG_OK(i2c, fmt, ...)					\
	do {							\
		if ((i2c)->ok_budget) {				\
			(i2c)->ok_budget--;				\
			dev_err((i2c)->dev, fmt, ##__VA_ARGS__);	\
		}						\
	} while (0)

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
	u32 div, sclk, half, wait_high, high, clk_conf, ctr, tout;

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

	/*
	 * Written to match i2c_ll_set_bus_timing() line for line, including
	 * where it does and does not subtract one. The HAL's comment explains
	 * the asymmetry: scl_high and scl_wait_high are deliberately not
	 * decremented because doing so makes the clock run slightly fast.
	 */
	esp32_i2c_write(i2c, I2C_SCL_LOW_PERIOD, half - 1);
	esp32_i2c_write(i2c, I2C_SCL_HIGH_PERIOD,
			FIELD_PREP(GENMASK(8, 0), high) |
			FIELD_PREP(GENMASK(15, 9), wait_high));
	esp32_i2c_write(i2c, I2C_SDA_HOLD, (half / 4) - 1);
	esp32_i2c_write(i2c, I2C_SDA_SAMPLE, (half / 2) - 1);
	esp32_i2c_write(i2c, I2C_SCL_START_HOLD, half - 1);
	esp32_i2c_write(i2c, I2C_SCL_RSTART_SETUP, half - 1);
	esp32_i2c_write(i2c, I2C_SCL_STOP_HOLD, half - 1);
	esp32_i2c_write(i2c, I2C_SCL_STOP_SETUP, half - 1);

	/* sclk_div_num is bits [7:0] -- corrected from [13:8], which is
	 * sclk_div_a and does fraction division. */
	clk_conf = esp32_i2c_read(i2c, I2C_CLK_CONF);
	clk_conf &= ~GENMASK(7, 0);
	clk_conf |= FIELD_PREP(GENMASK(7, 0), div - 1);
	esp32_i2c_write(i2c, I2C_CLK_CONF, clk_conf);

	/*
	 * The peripheral's own bus timeout, which the HAL always enables.
	 *
	 * Two corrections here, both read out of i2c_struct.h and
	 * i2c_ll_cal_bus_clk() rather than guessed:
	 *
	 *   time_out_en is bit 5, not bit 10. Writing bit 10 was writing a
	 *   reserved field, so the hardware timeout was never enabled -- which
	 *   is why INT_TIME_OUT never fired on a stretching bus and why the
	 *   driver sat on its own one-second timeout instead of failing fast
	 *   and recovering.
	 *
	 *   tout is derived from the bus timing, not a constant. ESP-IDF's
	 *   comment is "we set the timeout value to about 10 bus cycles" and
	 *   computes log2(5 * half_cycle) + 2. The field is 5 bits, so it is
	 *   clamped rather than allowed to wrap.
	 */
	tout = 32 - __builtin_clz(5 * half) + 2;
	if (tout > 31)
		tout = 31;
	if (tout < 1)
		tout = 1;
	esp32_i2c_write(i2c, I2C_TO,
			FIELD_PREP(GENMASK(4, 0), tout) | BIT(5) /* time_out_en */);

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

	/*
	 * The matching INPUT route. This is what the first version of this
	 * driver left out, and the symptom was subtle: the transfer started,
	 * consumed the address byte (txfifo_cnt fell from 3 to 2) and then sat
	 * there with int_status at zero, because the controller could not read
	 * SDA back and so never saw an ACK.
	 *
	 * ESP-IDF's i2c_set_pin() calls gpio_matrix_in() for exactly this.
	 * GPIO_SIGn_IN_SEL = 1 selects the pad directly.
	 */
	iowrite32(pin | GPIO_IN_SEL_GPIO, gpio + GPIO_IN_SEL_OFF(sig));

	/*
	 * I2C is open drain: the pad pulls low and a passive pull-up restores
	 * high. Without pad_driver the pad drives high actively, two devices
	 * fight, and the bus is not I2C at all.
	 */
	iowrite32(ioread32(gpio + GPIO_PIN_OFF(pin)) | GPIO_PIN_PAD_DRIVER,
		  gpio + GPIO_PIN_OFF(pin));

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
	dev_info(dev, "input sda<-sig%u:%08x scl<-sig%u:%08x pad_driver sda=%u scl=%u\n",
		 SIG_I2CEXT0_SDA, ioread32(gpio + GPIO_IN_SEL_OFF(SIG_I2CEXT0_SDA)),
		 SIG_I2CEXT0_SCL, ioread32(gpio + GPIO_IN_SEL_OFF(SIG_I2CEXT0_SCL)),
		 !!(ioread32(gpio + GPIO_PIN_OFF(sda)) & GPIO_PIN_PAD_DRIVER),
		 !!(ioread32(gpio + GPIO_PIN_OFF(scl)) & GPIO_PIN_PAD_DRIVER));

	i2c->have_pins = true;
}

static void esp32_i2c_reset_fifos(struct esp32_i2c *i2c)
{
	u32 v = esp32_i2c_read(i2c, I2C_FIFO_CONF);

	/* Thresholds at the HAL's defaults (11 and 4) rather than zero. Zero
	 * was an unverified assumption in the first version; matching the HAL
	 * removes one variable while the one-byte-read bug is chased. */
	v &= ~(GENMASK(4, 0) | GENMASK(9, 5));
	v |= FIELD_PREP(GENMASK(4, 0), 11) | FIELD_PREP(GENMASK(9, 5), 4);

	/* Pulse each reset separately, as i2c_ll_txfifo_rst()/rxfifo_rst() do.
	 * A single write setting both leaves the reset asserted. */
	esp32_i2c_write(i2c, I2C_FIFO_CONF, v & ~(FIFO_CONF_RX_RST | FIFO_CONF_TX_RST));
	esp32_i2c_write(i2c, I2C_FIFO_CONF, v | FIFO_CONF_TX_RST);
	esp32_i2c_write(i2c, I2C_FIFO_CONF, v & ~FIFO_CONF_TX_RST);
	esp32_i2c_write(i2c, I2C_FIFO_CONF, v | FIFO_CONF_RX_RST);
	esp32_i2c_write(i2c, I2C_FIFO_CONF, v & ~FIFO_CONF_RX_RST);
}

/*
 * Recovery, matching ESP-IDF's i2c_hw_fsm_reset() on ESP32-S3.
 *
 * ESP32-S3 leaves SOC_I2C_SUPPORT_HW_FSM_RST undefined, with this comment in
 * soc_caps.h: "FSM_RST only resets the FSM, not using it." So the one-bit
 * reset is not usable and ESP-IDF takes the other branch, which cycles the
 * whole peripheral:
 *
 *   clock off + reset asserted
 *   ... nine recovery clocks ...
 *   clock on  + reset released
 *   re-init master mode and FIFO mode
 *   disable and clear all interrupts
 *   RESTORE THE TIMING REGISTERS          <-- the step that was missing
 *   restore the filter
 *
 * Losing the timing restore is what broke this driver twice: after a gate
 * cycle every timing register is back at its default, so the next transfer
 * runs at a nonsense clock and the peripheral reports scl_main_st_to with
 * arb_lost -- a bus fault that looks electrical but is self-inflicted.
 *
 * The nine-clock sequence between the gate writes is ESP-IDF's
 * i2c_master_clear_bus(); on S3 that is i2c_ll_master_clr_bus(), one write to
 * SCL_SP_CONF, because SOC_I2C_SUPPORT_HW_CLR_BUS is 1 here.
 */
static void esp32_i2c_recover(struct esp32_i2c *i2c)
{
	void __iomem *sys = i2c->sys;

	/* Clock off and reset asserted: periph_ll_disable_clk_set_rst(). */
	if (sys) {
		iowrite32(ioread32(sys + 0x18) & ~BIT(7), sys + 0x18);
		iowrite32(ioread32(sys + 0x20) | BIT(7), sys + 0x20);
	}
	udelay(10);

	/* Nine clocks to unstick a slave holding SDA low, then a STOP. */
	esp32_i2c_write(i2c, I2C_SCL_SP_CONF,
			FIELD_PREP(GENMASK(5, 1), SCL_SP_RST_NUM) | SCL_SP_RST_EN);

	/*
	 * Latch it. conf_upgate is write-triggered and synchronises the
	 * configuration into the peripheral's own clock domain;
	 * i2c_ll_master_clr_bus() sets it immediately after programming
	 * scl_rst_slv_en and scl_rst_slv_num, and without it the nine pulses
	 * are written and never taken up.
	 *
	 * This is the third time in this project that a register written and
	 * never latched looked like working code: the timing registers had the
	 * same problem, and so did the transfer trigger. The consequence here
	 * was worse than a slowdown -- the bus clear had never actually run, so
	 * recovery could not free a held bus and every incident needed a power
	 * cycle instead.
	 */
	esp32_i2c_write(i2c, I2C_CTR,
			esp32_i2c_read(i2c, I2C_CTR) | CTR_CONF_UPDATE);

	/* Clock on and reset released: periph_ll_enable_clk_clear_rst(). */
	if (sys) {
		iowrite32(ioread32(sys + 0x18) | BIT(7), sys + 0x18);
		iowrite32(ioread32(sys + 0x20) & ~BIT(7), sys + 0x20);
	}
	udelay(50);

	/* Master mode, clock enable, pads driven (i2c_ll_master_init). */
	esp32_i2c_write(i2c, I2C_CTR, CTR_MS_MODE | CTR_CLK_EN |
				      CTR_SDA_FORCE_OUT | CTR_SCL_FORCE_OUT |
				      CTR_ARBITRATION_EN);

	/* FIFO mode, MSB first, and both FIFOs emptied. */
	esp32_i2c_write(i2c, I2C_FIFO_CONF, 0);
	esp32_i2c_reset_fifos(i2c);

	esp32_i2c_write(i2c, I2C_INT_ENA, 0);
	esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);

	/* THE MISSING STEP: without it the bus runs at default timing. */
	esp32_i2c_set_timing(i2c, i2c->bus_freq ? i2c->bus_freq : 100000);

	/*
	 * Restore the glitch filter too. i2c_hw_fsm_reset() saves and restores
	 * it across the reset, and this driver configures it at probe, so
	 * leaving it at the reset default after a recovery silently changes the
	 * bus' tolerance to noise for the rest of the boot.
	 */
	esp32_i2c_write(i2c, I2C_FILTER_CFG, 0);

	esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);

	DBG(i2c, "bus recovered (int_raw=%08x sr=%08x)\n",
	    esp32_i2c_read(i2c, I2C_INT_RAW),
	    esp32_i2c_read(i2c, I2C_SR));
}

/*
 * Make sure the bus is free before a transfer, by RESETTING it rather than
 * waiting for it.
 *
 * ESP-IDF's i2c_master_cmd_begin() does exactly this:
 *
 *     if (status == TIMEOUT || i2c_ll_is_bus_busy(dev))
 *             i2c_hw_fsm_reset(i2c_num);
 *
 * bus_busy does not clear on its own after a completed transfer on this
 * silicon, so spinning on it -- which this driver did -- guarantees a timeout
 * on every transfer after the first. That was the "one transfer per boot"
 * symptom: all three subsystems registered during probe and then every read
 * failed.
 */
static void esp32_i2c_wait_idle(struct esp32_i2c *i2c)
{
	if (esp32_i2c_read(i2c, I2C_SR) & SR_BUS_BUSY)
		esp32_i2c_recover(i2c);
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
	 * Configure, latch, then trigger -- three separate writes.
	 *
	 * conf_upgate and trans_start are both write-triggered and
	 * self-clearing, and combining them in one store lets the latch
	 * consume the trigger: measured, the FIFO kept every byte and
	 * int_raw stayed zero. i2c_ll_update() exists as its own step for
	 * exactly this reason.
	 */
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

	/*
	 * Poll INT_RAW, not INT_STATUS.
	 *
	 * INT_STATUS is INT_RAW masked by INT_ENA, and this driver runs the
	 * controller in polling mode with INT_ENA left at zero -- so INT_STATUS
	 * reads 0 for ever, no matter what the peripheral is doing. That made
	 * every transfer look like it never started, when in fact the FSM was
	 * clocking bytes out and the FIFO was draining.
	 */
	for (;;) {
		st = esp32_i2c_read(i2c, I2C_INT_RAW);

		/*
		 * NACK and arbitration-loss both leave the peripheral in a state
		 * that has to be cleaned up, and neither path used to do it.
		 *
		 * This is what made the bus wedged after touching the capacitive
		 * panel, and it is the same fault tbtns hit: the touch controller
		 * pulls on SDA/SCL while a read is in flight, the controller loses
		 * arbitration (SR bit 3) or sees a NACK, this function returned
		 * immediately with the interrupt cleared and no reset -- and every
		 * later transfer, to every device, failed identically. Recovery was
		 * only wired into the timeout paths, so the one class of failure
		 * that actually occurred was the one class that never recovered.
		 *
		 * ESP-IDF's i2c_master_cmd_begin() resets when the status is TIMEOUT
		 * *or the bus is still busy*, which catches all of these. The rule
		 * used here is the same and is deliberately general: after any
		 * failure, if the controller still thinks the bus is busy, reset it.
		 * A NACK from a device that is simply absent must not trigger a
		 * reset, and does not, because the bus is not busy in that case.
		 */
		if (st & (INT_NACK | INT_ARB_LOST)) {
			u32 sr = esp32_i2c_read(i2c, I2C_SR);

			DBG(i2c, "%s: int_raw=%08x sr=%08x busy=%u\n",
			    (st & INT_NACK) ? "NACK" : "ARB_LOST", st, sr,
			    !!(sr & SR_BUS_BUSY));

			esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
			if (sr & SR_BUS_BUSY)
				esp32_i2c_recover(i2c);

			return (st & INT_NACK) ? -ENXIO : -EAGAIN;
		}
		if (st & (INT_TIME_OUT | INT_SCL_ST_TO | INT_SCL_MAIN_ST_TO)) {
			/*
			 * A timeout of one of the three kinds the controller can
			 * report, which on this board means a slave is holding
			 * SCL (clock-stretching) and the transfer is not going to
			 * finish.
			 *
			 * Returning without resetting leaves the bus wedged for
			 * every later transfer: the FSM is idle, so SR_BUS_BUSY is
			 * 0 and esp32_i2c_wait_idle() never fires recovery, while
			 * each new transfer fails the same way. One bad transfer
			 * then costs the whole boot, which is exactly what was
			 * observed.
			 *
			 * ESP-IDF's i2c_master_cmd_begin() has the same branch and
			 * calls i2c_hw_fsm_reset() when the status is TIMEOUT or the
			 * bus is still busy. This driver was missing it.
			 */
			/*
			 * The first failure after boot, logged once and outside the
			 * budget. A wedged bus produces a storm of identical errors, so
			 * the only line worth having is the first one, with every
			 * register that could explain it.
			 */
			if (!i2c->first_err) {
				i2c->first_err = true;
				dev_err(i2c->dev,
					"FIRST SCL timeout: int_raw=%08x sr=%08x ctr=%08x clk_conf=%08x to=%08x fifo_conf=%08x\n",
					st, esp32_i2c_read(i2c, I2C_SR),
					esp32_i2c_read(i2c, I2C_CTR),
					esp32_i2c_read(i2c, I2C_CLK_CONF),
					esp32_i2c_read(i2c, I2C_TO),
					esp32_i2c_read(i2c, I2C_FIFO_CONF));
			}
			DBG(i2c,
			    "SCL timeout: int_raw=%08x sr=%08x fifo_conf=%08x\n",
			    st, esp32_i2c_read(i2c, I2C_SR),
			    esp32_i2c_read(i2c, I2C_FIFO_CONF));

			esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
			esp32_i2c_recover(i2c);
			return -ETIMEDOUT;
		}
		if (st & INT_TRANS_COMPLETE)
			break;

		if (time_after(jiffies, timeout)) {
			if (!i2c->first_err) {
				i2c->first_err = true;
				dev_err(i2c->dev,
					"FIRST spin timeout: int_raw=%08x sr=%08x ctr=%08x clk_conf=%08x to=%08x\n",
					st, esp32_i2c_read(i2c, I2C_SR),
					esp32_i2c_read(i2c, I2C_CTR),
					esp32_i2c_read(i2c, I2C_CLK_CONF),
					esp32_i2c_read(i2c, I2C_TO));
			}
			DBG(i2c, "spin timeout: int_raw=%08x sr=%08x ctr=%08x\n",
			    st, esp32_i2c_read(i2c, I2C_SR),
			    esp32_i2c_read(i2c, I2C_CTR));
			esp32_i2c_recover(i2c);
			esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	esp32_i2c_write(i2c, I2C_INT_CLR, 0xffffffff);

	/*
	 * Give the STOP condition time to finish on the wire.
	 *
	 * trans_complete fires when the command list has retired, which is
	 * slightly before the bus itself is idle. ESP-IDF does not wait -- it
	 * simply checks bus_busy at the start of the next transfer and resets
	 * the peripheral if it is still set. Waiting briefly here means that
	 * recovery is the exception rather than the norm.
	 *
	 * The one-bit FSM reset is deliberately NOT used: soc_caps.h says
	 * "FSM_RST only resets the FSM, not using it", and it is exactly what
	 * put this driver into a state where the next transfer ran at default
	 * timing.
	 */
	{
		unsigned long t0 = jiffies;

		while ((esp32_i2c_read(i2c, I2C_SR) & SR_BUS_BUSY) &&
		       !time_after(jiffies, t0 + usecs_to_jiffies(2000)))
			cpu_relax();
	}

	return 0;
}

static int esp32_i2c_transfer(struct esp32_i2c *i2c, u16 addr,
			      const u8 *wbuf, size_t wlen,
			      u8 *rbuf, size_t rlen)
{
	int n = 0, i, err;

	if (!wlen && !rlen)
		return -EINVAL;
	/*
	 * addr|W shares a command with the payload, so that command carries
	 * 1 + wlen bytes and wlen cannot exceed 254.
	 */
	if (wlen > 254 || rlen > 255)
		return -EINVAL;

	esp32_i2c_reset_fifos(i2c);

	esp32_i2c_write(i2c, I2C_COMD(n++), CMD_OP_CODE(OP_RSTART));

	/*
	 * addr|W and the register number are ONE WRITE command.
	 *
	 * A WRITE command streams byte_num bytes back to back, so there is
	 * nothing special about the address byte: it is simply the first byte
	 * of the command and the payload follows it. ESP-IDF's
	 * i2c_master_write() emits one command for any data_len > 1 for the
	 * same reason. The wire sequence is identical either way -- START,
	 * addr|W, reg, ... -- and only the command-register segmentation
	 * differs.
	 *
	 * An earlier version of this driver gave the address its own one-byte
	 * command and then concluded, from a build where merging hung, that
	 * merging was wrong. That build also set ack_exp on every command and
	 * had no GPIO input route, so no address was ever acknowledged and
	 * every shape hung. The merge is not a trick: it is what frees the
	 * command register that the split READ below needs, and a 16-bit
	 * register address needs exactly that one register.
	 */
	if (wlen) {
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_WRITE) | CMD_BYTE_NUM(1 + wlen) |
				CMD_ACK_EN);
	}

	if (rlen) {
		/* Repeated start: the register number and the read have to be
		 * one transaction, or another master could interleave. */
		if (wlen)
			esp32_i2c_write(i2c, I2C_COMD(n++),
					CMD_OP_CODE(OP_RSTART));
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_WRITE) | CMD_BYTE_NUM(1) |
				CMD_ACK_EN);
		/*
		 * The read is SPLIT into READ(N-1) with ACK plus READ(1) with
		 * NACK. This is what makes a multi-byte read work at all.
		 *
		 * ack_val is the value the master drives on the LAST received
		 * byte of the command it belongs to. A single READ command of N
		 * bytes with ack_val = 1 therefore NACKs the end of that command,
		 * the slave stops after the first byte, and the remaining N-1
		 * bytes are whatever the pull-ups supply -- 0xff. That was the
		 * entire multi-byte symptom: one real byte then 0xff to the end,
		 * with rxfifo_cnt correctly reporting N and so no short read to
		 * warn about.
		 *
		 * ESP-IDF has no single "read N, NACK the last" command either.
		 * i2c_master_read() with I2C_MASTER_LAST_NACK splits into
		 * READ(data_len - 1) with I2C_MASTER_ACK followed by a one-byte
		 * READ with I2C_MASTER_NACK. This is that split, one command
		 * each: READ(N-1) carries no ack_val (ACK), READ(1) carries
		 * ack_val=1 (NACK).
		 *
		 * Two earlier readings of this field were wrong, and both came
		 * from contaminated evidence rather than from i2c.c:
		 *   "ack_val=1 matches ESP-IDF" -- it matches only the final
		 *   one-byte command, never a multi-byte one;
		 *   "ack_val=0 works for 7 bytes, so use it" -- it did, because
		 *   the master then ACKed the final byte too and relied on
		 *   byte_num to cut the transfer short, which is not what the
		 *   protocol asks for and which hung on a one-byte read.
		 */
		if (rlen > 1)
			esp32_i2c_write(i2c, I2C_COMD(n++),
					CMD_OP_CODE(OP_READ) |
					CMD_BYTE_NUM(rlen - 1));
		esp32_i2c_write(i2c, I2C_COMD(n++),
				CMD_OP_CODE(OP_READ) | CMD_BYTE_NUM(1) |
				CMD_ACK_VAL);
	}

	/*
	 * SOC_I2C_CMD_REG_NUM is 8 and I2C_COMD(n) is a register set, so the
	 * END command must land at index 7 or lower. The worst case above is
	 * six commands consumed before STOP, so this cannot fire -- it is here
	 * so that a future change to the shape fails loudly instead of
	 * scribbling the END opcode over a timing register.
	 */
	if (WARN_ON_ONCE(n > 6))
		return -EINVAL;

	esp32_i2c_write(i2c, I2C_COMD(n++), CMD_OP_CODE(OP_STOP));
	esp32_i2c_write(i2c, I2C_COMD(n), CMD_OP_CODE(OP_END));

	/* FIFO bytes, in the order the commands above consume them. */
	if (wlen) {
		esp32_i2c_write(i2c, I2C_DATA, addr << 1);
		for (i = 0; i < wlen; i++)
			esp32_i2c_write(i2c, I2C_DATA, wbuf[i]);
	}
	if (rlen)
		esp32_i2c_write(i2c, I2C_DATA, (addr << 1) | 1);

	/*
	 * Rate limited on purpose. With the debug flag enabled this printed eight
	 * lines per transfer, and at boot that came to 241 lines spread over a
	 * hundred seconds -- on a board whose console is a 115200 serial port
	 * that also mirrors to the AMOLED, so the log itself was the slowest
	 * thing in the boot. A dump that can stall the machine is not a
	 * diagnostic.
	 */
	if (i2c->debug && printk_ratelimit()) {
		int k;

		for (k = 0; k <= n; k++)
			pr_info("i2c-esp32: comd[%d]=%08x\n", k,
				esp32_i2c_read(i2c, I2C_COMD(k)));
	}

	err = esp32_i2c_run(i2c);
	if (err)
		return err;

	if (rlen) {
		u32 sr = esp32_i2c_read(i2c, I2C_SR);
		u32 cnt = FIELD_GET(SR_RXFIFO_CNT_MASK, sr);

		if (i2c->debug && printk_ratelimit())
			pr_info("i2c-esp32: RX len=%zu sr=%08x cnt=%u\n",
				rlen, sr, cnt);
		/*
		 * Proof of life: shows whether a transfer completed at all, which
		 * is the difference between "the bus is wedged" and "the bus is
		 * fine and something above us is failing".
		 */
		DBG_OK(i2c, "ok addr=%02x wlen=%zu rlen=%zu cnt=%u sr=%08x\n",
		       addr, wlen, rlen, cnt, sr);
		if (cnt != rlen)
			DBG(i2c, "short read: %u of %zu (sr=%08x fifo_conf=%08x)\n",
			    cnt, rlen, sr, esp32_i2c_read(i2c, I2C_FIFO_CONF));
		for (i = 0; i < rlen && i < cnt; i++)
			rbuf[i] = esp32_i2c_read(i2c, I2C_DATA) & 0xff;
	}

	return 0;
}

static int esp32_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			  int num)
{
	struct esp32_i2c *i2c = i2c_get_adapdata(adap);
	int i, ret;

	mutex_lock(&i2c->lock);

	esp32_i2c_wait_idle(i2c);

	for (i = 0; i < num; i++) {
		struct i2c_msg *m = &msgs[i];

		/*
		 * The common register read arrives as two messages: WRITE the
		 * register number, then READ. They must be one command list
		 * with a repeated start between them, not two transactions.
		 */
		if ((i + 1 < num) && !(m->flags & I2C_M_RD) &&
		    (msgs[i + 1].flags & I2C_M_RD) &&
		    msgs[i + 1].addr == m->addr) {
			ret = esp32_i2c_transfer(i2c, m->addr, m->buf, m->len,
						 msgs[i + 1].buf,
						 msgs[i + 1].len);
			i++;
		} else if (m->flags & I2C_M_RD) {
			ret = esp32_i2c_transfer(i2c, m->addr, NULL, 0,
						 m->buf, m->len);
		} else {
			ret = esp32_i2c_transfer(i2c, m->addr, m->buf, m->len,
						 NULL, 0);
		}
		if (ret)
			goto out;
	}

	ret = num;
out:
	if (ret < 0) {
		/*
		 * Every failure, with the shape that produced it. The driver
		 * returns -ENXIO, -EAGAIN and -ETIMEDOUT from three different
		 * places, and the error a caller sees is the same -110 either
		 * way, so this is the only way to tell which branch fired.
		 */
		DBG(i2c, "xfer FAIL ret=%d num=%d addr=%02x flags=%02x len=%u\n",
		    ret, num, msgs[0].addr, msgs[0].flags, msgs[0].len);
	}
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
	i2c->debug = of_property_read_bool(pdev->dev.of_node, "espressif,debug");
	i2c->dbg_budget = 40;
	i2c->ok_budget = 30;

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

		i2c->sys = sys;

		dev_info(&pdev->dev,
			 "sys: perip_clk_en0=%08x perip_rst_en0=%08x (bit7=%u)\n",
			 ioread32(sys + 0x18), ioread32(sys + 0x20),
			 !!(ioread32(sys + 0x20) & BIT(7)));
	}

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
