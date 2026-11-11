// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>

#define ESP32_SPI_DRIVER_NAME	"esp32_spi"

#define SPI_CMD_REG		0x00
#define SPI_UPDATE			BIT(23)
#define SPI_USR				BIT(24)

#define SPI_ADDR_REG		0x04

#define SPI_CTRL_REG		0x08
#define SPI_Q_POL			BIT(18)
#define SPI_D_POL			BIT(19)
#define SPI_HOLD_POL			BIT(20)
#define SPI_WP_POL			BIT(21)
#define SPI_RD_BIT_ORDER		GENMASK(24, 23)
#define SPI_WR_BIT_ORDER		GENMASK(26, 25)
/*
 * Per-phase line-width selectors. Decoded from the esp32s3 spi_struct.h field
 * order and cross-checked against spi_reg.h: fcmd_quad is bit 9, faddr_quad
 * bit 6. Getting these wrong is what made an earlier hand-written driver
 * deliver nothing while reporting success.
 */
#define SPI_FCMD_DUAL			BIT(8)
#define SPI_FCMD_QUAD			BIT(9)
#define SPI_FADDR_DUAL			BIT(5)
#define SPI_FADDR_QUAD			BIT(6)

#define SPI_CLOCK_REG		0x0c
#define SPI_CLKCNT_L			GENMASK(5, 0)
#define SPI_CLKCNT_H			GENMASK(11, 6)
#define SPI_CLKCNT_N			GENMASK(17, 12)
#define SPI_CLKDIV_PRE			GENMASK(21, 18)
#define SPI_CLK_EQU_SYSCLK		BIT(31)

#define SPI_USER_REG		0x10
#define SPI_DOUTDIN			BIT(0)
#define SPI_QPI_MODE			BIT(3)
#define SPI_OPI_MODE			BIT(4)
#define SPI_CS_HOLD			BIT(6)
#define SPI_CS_SETUP			BIT(7)
#define SPI_CK_OUT_EDGE			BIT(9)
#define SPI_FWRITE_DUAL			BIT(12)
#define SPI_FWRITE_QUAD			BIT(13)
#define SPI_FWRITE_OCT			BIT(14)
#define SPI_USR_MOSI			BIT(27)
#define SPI_USR_MISO			BIT(28)
#define SPI_USR_DUMMY			BIT(29)
#define SPI_USR_ADDR			BIT(30)
#define SPI_USR_COMMAND			BIT(31)

#define SPI_USER1_REG		0x14
#define SPI_CS_HOLD_TIME		GENMASK(26, 22)
#define SPI_CS_SETUP_TIME		GENMASK(21, 17)
/*
 * Address and dummy-cycle lengths live in USER1. Used by the spi-mem path;
 * without them the peripheral has no command or address phase at all, which is
 * why this driver could only ever shift data.
 */
#define SPI_USR_ADDR_BITLEN		GENMASK(31, 27)
#define SPI_USR_DUMMY_CYCLELEN		GENMASK(15, 8)

#define SPI_USER2_REG		0x18
#define SPI_USR_COMMAND_VALUE		GENMASK(15, 0)
#define SPI_USR_COMMAND_BITLEN		GENMASK(31, 28)



#define SPI_MS_DLEN_REG		0x1c
#define SPI_MS_DATA_BITLEN		GENMASK(17, 0)

#define SPI_MISC_REG		0x20
#define SPI_CS_DIS			GENMASK(5, 0)
#define SPI_CK_DIS			BIT(6)
#define SPI_MASTER_CS_POL		GENMASK(12, 7)
#define SPI_CK_IDLE_EDGE		BIT(29)
#define SPI_CS_KEEP_ACTIVE		BIT(30)

#define SPI_DIN_MODE_REG	0x24
#define SPI_DIN_NUM_REG		0x28
#define SPI_DOUT_MODE_REG	0x2c
#define SPI_DMA_CONF_REG	0x30
#define SPI_DMA_RX_ENA			BIT(27)
#define SPI_DMA_TX_ENA			BIT(28)
#define SPI_RX_AFIFO_RST		BIT(29)
#define SPI_BUF_AFIFO_RST		BIT(30)
#define SPI_DMA_AFIFO_RST		BIT(31)

#define SPI_DMA_INT_ENA_REG	0x34
#define SPI_DMA_INT_CLR_REG	0x38
#define SPI_DMA_INT_RAW_REG	0x3c
#define SPI_DMA_INT_ST_REG	0x40
#define SPI_TRANS_DONE_INT		BIT(12)

#define ESP32_SPI_FIFO_SIZE	64
#define SPI_W0_REG		0x98

/*
 * GDMA, at 0x6003F000, with 5 channel pairs on this chip.
 *
 * Worth knowing before using it: the GDMA block is shared with the firmware on
 * core 0, and the firmware is a closed WiFi blob we cannot inspect. The Linux
 * panel driver wants one TX channel to replace 4000 CPU-mode FIFO chunks per
 * frame with a handful of DMA transfers, but claiming a channel the blob is
 * already using would corrupt its traffic -- and the failure would appear as
 * intermittent WiFi, not as a panel bug.
 *
 * What the firmware source does say, and it is reassuring: the esp-hosted
 * transport on this board is shmem (CONFIG_ESP_SHMEM_HOST_INTERFACE=y), not SPI
 * and not SDIO, so networking claims no SPI DMA path at all; and slave_bt.c's
 * UHCI channels are never initialised because this build uses the internal BT
 * controller through NimBLE. That leaves the blob's own internal use as the
 * only unknown, which is what the dump below is for.
 */
#define ESP32_GDMA_BASE		0x6003f000
#define ESP32_GDMA_CHANS	5
#define GDMA_CH_STRIDE		0xc0
#define GDMA_IN_CONF0(n)	(0x00 + (n) * GDMA_CH_STRIDE)
#define GDMA_IN_LINK(n)		(0x20 + (n) * GDMA_CH_STRIDE)
#define GDMA_IN_PERI_SEL(n)	(0x48 + (n) * GDMA_CH_STRIDE)
#define GDMA_OUT_CONF0(n)	(0x60 + (n) * GDMA_CH_STRIDE)
#define GDMA_OUT_LINK(n)		(0x80 + (n) * GDMA_CH_STRIDE)
#define GDMA_OUT_PERI_SEL(n)	(0xa8 + (n) * GDMA_CH_STRIDE)

#define SPI_SLAVE_REG		0xe0
#define SPI_SLAVE_MODE			BIT(26)
#define SPI_SOFT_RESET			BIT(27)

#define SPI_CLK_GATE_REG	0xe8
#define SPI_CLK_EN			BIT(0)
#define SPI_MST_CLK_ACTIVE		BIT(1)
#define SPI_MST_CLK_SEL			BIT(2)


struct esp32_spi {
	void __iomem *regs;		/* virt. address of control registers */
	struct clk *clk;		/* bus clock */
	u32 speed_hz;			/* bus speed configured in SPI_CLOCK_REG */
	phys_addr_t phys;		/* peripheral base, to pick signal indices */
	void __iomem *iopad;		/* IO_MUX, for the clock self-test */
	void __iomem *gpio;		/* GPIO matrix, ditto */
	u32 clk_pin;
	bool have_clk;
	struct completion done;		/* wake-up from interrupt */
};

/*
 * GPIO matrix routing.
 *
 * Neither SPI bus on this SoC can reach the watch's panel pins through IO_MUX.
 * SPI3 has no IO_MUX pins at all ("SPI3 have no iomux pins" in soc/spi_pins.h),
 * and SPI2's are GPIO 9..14. The panel is on 38..45, so ESP-IDF routes it
 * through the GPIO matrix -- and this driver has to do the same, because a pad
 * left in its reset IO_MUX function carries nothing. That is why every register
 * write completed, every status said OK, and the panel stayed dark.
 *
 * Signal indices differ per instance and are NOT interchangeable: 0..4 belong
 * to the flash/cache peripheral, 101..105 to FSPI, 66..70 to SPI3.
 */
#define ESP32_IO_MUX_BASE	0x60009000
#define ESP32_GPIO_BASE		0x60004000

/* IO_MUX is 0x04 + pin*4 for every pin, including the JTAG pads 39..42. */
#define IO_MUX_OFF(pin)		(0x04 + (pin) * 4)
#define GPIO_OUT_SEL_OFF(pin)	(0x554 + (pin) * 4)
/*
 * GPIO is split into two 32-bit banks and each has its own enable registers.
 * Pins 38..45 are all in the high bank, so a write to the low one enables
 * GPIO 6, 7, 8, 10, 13 instead -- pads that belong to something else -- while
 * the panel pins stay undriven.
 */
#define GPIO_ENABLE_W1TS_OFF	0x24
#define GPIO_ENABLE1_W1TS_OFF	0x30

/*
 * IO_MUX fields, from soc/io_mux_reg.h: FUN_DRV is 11:10 (0=5mA 1=10mA
 * 2=20mA 3=40mA), FUN_IE is bit 9, MCU_SEL is 14:12. There is no output-enable
 * bit here at all -- the pad's OE comes from the GPIO matrix.
 */
#define IO_MUX_FUN_IE		BIT(9)
#define IO_MUX_FUN_DRV_MASK	GENMASK(11, 10)
#define IO_MUX_FUN_DRV_20MA	(2 << 10)
#define IO_MUX_MCU_SEL_MASK	GENMASK(14, 12)
#define IO_MUX_MCU_SEL_GPIO	(1 << 12)

/* Output enable comes from GPIO_ENABLE rather than the peripheral signal. */
#define GPIO_FUNC_OEN_SEL	BIT(10)

struct esp32_spi_sig {
	u16 clk, d, q, hd, wp;
};

/* Indexed by peripheral instance: SPI2 is FSPI, SPI3 has its own numbering. */
static const struct esp32_spi_sig esp32_spi_sig[] = {
	[2] = { 101, 103, 102, 104, 105 },
	[3] = { 66, 68, 67, 69, 70 },
};

static void esp32_spi_write(struct esp32_spi *spi, int offset, u32 value)
{
	iowrite32(value, spi->regs + offset);
}

static u32 esp32_spi_read(struct esp32_spi *spi, int offset)
{
	return ioread32(spi->regs + offset);
}

static void esp32_spi_init(struct esp32_spi *spi)
{
	esp32_spi_write(spi, SPI_CLK_GATE_REG,
			SPI_CLK_EN |
			SPI_MST_CLK_ACTIVE |
			SPI_MST_CLK_SEL);
	esp32_spi_write(spi, SPI_SLAVE_REG, SPI_SOFT_RESET);
	esp32_spi_write(spi, SPI_SLAVE_REG, 0);
	esp32_spi_write(spi, SPI_DMA_INT_ENA_REG, 0);
	esp32_spi_write(spi, SPI_DMA_INT_CLR_REG, SPI_TRANS_DONE_INT);
	esp32_spi_write(spi, SPI_DIN_MODE_REG, 0);
	esp32_spi_write(spi, SPI_DOUT_MODE_REG, 0);
	esp32_spi_write(spi, SPI_USER2_REG, 0);
	esp32_spi_write(spi, SPI_DMA_CONF_REG,
			SPI_RX_AFIFO_RST |
			SPI_BUF_AFIFO_RST);
}

static int
esp32_spi_prepare_message(struct spi_controller *host, struct spi_message *msg)
{
	struct esp32_spi *spi = spi_controller_get_devdata(host);
	struct spi_device *device = msg->spi;
	u8 cs = spi_get_chipselect(device, 0);
	u32 cr;

	/* SPI_USER1_REG */
	cr = FIELD_PREP(SPI_CS_HOLD_TIME, 1) | FIELD_PREP(SPI_CS_SETUP_TIME, 1);
	esp32_spi_write(spi, SPI_USER1_REG, cr);

	/* SPI_CTRL_REG */
	cr = 0;

	if (device->mode & SPI_LSB_FIRST)
		cr |= SPI_RD_BIT_ORDER | SPI_WR_BIT_ORDER;

	esp32_spi_write(spi, SPI_CTRL_REG, cr);

	/* SPI_MISC_REG */
	cr = //SPI_CS_KEEP_ACTIVE |
		//FIELD_PREP(SPI_MASTER_CS_POL, BIT(cs)) |
		(SPI_CS_DIS ^ BIT(cs));

	if (device->mode & SPI_CPOL)
		cr |= SPI_CK_IDLE_EDGE;

	esp32_spi_write(spi, SPI_MISC_REG, cr);
	return 0;
}

static void esp32_spi_set_cs(struct spi_device *device, bool is_high)
{
	struct esp32_spi *spi = spi_controller_get_devdata(device->controller);
	u8 cs = spi_get_chipselect(device, 0);
	u32 cr = is_high ? 0 : SPI_CS_KEEP_ACTIVE;

	/* SPI_MISC_REG */
	cr |= (SPI_CS_DIS ^ BIT(cs));

	if (device->mode & SPI_CS_HIGH)
		cr |= FIELD_PREP(SPI_MASTER_CS_POL, BIT(cs));

	if (device->mode & SPI_CPOL)
		cr |= SPI_CK_IDLE_EDGE;

	esp32_spi_write(spi, SPI_MISC_REG, cr);
}

static int esp32_spi_wait_bit(struct esp32_spi *spi, u32 bit)
{
	unsigned long timeout = jiffies + HZ;

	while (esp32_spi_read(spi, SPI_CMD_REG) & bit) {
		if (time_after(jiffies, timeout)) {
			pr_warn("%s: timeout waiting for 0x%08x\n", __func__, bit);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}
	return 0;
}

// Program the bus clock for `speed_hz`.
//
// This lives on its own because the spi-mem path needs it too, and it used to
// be reachable only from prepare_transfer -- so exec_op ran with whatever the
// clock register happened to hold, which is not a valid divider. The panel
// then saw a bit stream at some arbitrary rate and ignored every command.
static void esp32_spi_set_clock(struct esp32_spi *spi, u32 speed_hz)
{
	u32 spi_clock = clk_get_rate(spi->clk);
	u32 clkcnt = DIV_ROUND_UP(spi_clock, speed_hz) - 1;
	u32 clkdiv = 1;
	u32 cr;

	if (clkcnt && !FIELD_FIT(SPI_CLKCNT_N, clkcnt)) {
		clkdiv = speed_hz / FIELD_MAX(SPI_CLKCNT_N);
		clkcnt = DIV_ROUND_UP(spi_clock, speed_hz * clkdiv) - 1;
	}
	if (!clkcnt)
		clkcnt = 1;

	cr = FIELD_PREP(SPI_CLKDIV_PRE, clkdiv - 1) |
	     FIELD_PREP(SPI_CLKCNT_N, clkcnt) |
	     FIELD_PREP(SPI_CLKCNT_L, clkcnt) |
	     FIELD_PREP(SPI_CLKCNT_H, (clkcnt + 1) / 2 - 1);

	esp32_spi_write(spi, SPI_CLOCK_REG, cr);
	spi->speed_hz = speed_hz;
}

static void esp32_spi_prepare_transfer(struct esp32_spi *spi,
				       struct spi_device *device,
				       struct spi_transfer *t,
				       unsigned int n_words)
{
	u32 cr;
	unsigned int mode;

	/* SPI_CLOCK_REG */
	if (t->speed_hz != spi->speed_hz)
		esp32_spi_set_clock(spi, t->speed_hz);

	/* SPI_MS_DLEN_REG */
	WARN_ON_ONCE(t->bits_per_word == 0);
	esp32_spi_write(spi, SPI_MS_DLEN_REG,
			FIELD_PREP(SPI_MS_DATA_BITLEN,
				   t->bits_per_word * n_words - 1));

	/* SPI_USER_REG */
	cr = SPI_CS_HOLD | SPI_CS_SETUP | SPI_DOUTDIN;

	if ((device->mode & (SPI_CPOL | SPI_CPHA)) == SPI_CPOL ||
	    (device->mode & (SPI_CPOL | SPI_CPHA)) == SPI_CPHA)
		cr |= SPI_CK_OUT_EDGE;

	if (t->tx_buf)
		cr |= SPI_USR_MOSI;
	if (t->rx_buf)
		cr |= SPI_USR_MISO;

	mode = max_t(unsigned int, t->rx_nbits, t->tx_nbits);
	WARN_ON(mode != SPI_NBITS_SINGLE);

	esp32_spi_write(spi, SPI_USER_REG, cr);
}


/*
 * spi-mem support: command, address and data as separate phases with their own
 * line widths.
 *
 * This exists because the CO5300 panel on the T-Watch Ultra speaks a protocol
 * spi_transfer cannot express -- an 8-bit command, a 24-bit address and a data
 * phase, where command and address travel on four lanes and the data phase may
 * be one lane (registers) or four (pixels). LilyGo's own ESP-IDF driver builds
 * exactly that with SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR for the
 * command/address and SPI_TRANS_MODE_QIO only on the pixel payload.
 *
 * spi_mem_op already models it; this maps the phases onto the peripheral.
 */
static int esp32_spi_start_transfer(struct esp32_spi *spi);
static int esp32_spi_wait(struct esp32_spi *spi);

static int esp32_spi_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	struct esp32_spi *spi = spi_controller_get_devdata(mem->spi->controller);
	u32 cr = SPI_CS_HOLD | SPI_CS_SETUP;
	/*
	 * Idle levels for the extra data lines, copied from the working
	 * ESP-IDF configuration (SPI_CTRL_REG reads 0x00340000 there: q_pol,
	 * hold_pol and wp_pol high, d_pol low). HOLD# and WP# must sit
	 * deasserted high on a QSPI device, and these lines are shared with
	 * the pixel data phase.
	 */
	u32 ctrl = SPI_Q_POL | SPI_HOLD_POL | SPI_WP_POL;
	u32 addr = 0;
	u32 pdma_len = 0;
	/*
	 * buf.in and buf.out are a union, so reading both unconditionally makes
	 * an output transfer alias its own source as the read-back buffer -- and
	 * exec_op then copies the FIFO back over the caller's data. For the init
	 * table, which lives in read-only XIP flash, that is an immediate store
	 * fault at the first byte.
	 */
	const u8 *tx = op->data.dir == SPI_MEM_DATA_OUT ? op->data.buf.out : NULL;
	u8 *rx = op->data.dir == SPI_MEM_DATA_IN ? op->data.buf.in : NULL;
	unsigned int remaining = op->data.nbytes;

	{
		static bool once;

		if (!once) {
			once = true;
			dev_info(mem->spi->controller->dev.parent,
				 "exec_op reached: cmd 0x%02X b/w %u, addr b/w %u, data b/w %u len %u\n",
				 op->cmd.opcode, op->cmd.buswidth,
				 op->addr.buswidth, op->data.buswidth,
				 op->data.nbytes);
		}
	}

	/*
	 * The bus clock is programmed per transfer on the spi_transfer path.
	 * exec_op has to do it itself, or the peripheral shifts at whatever
	 * divider the probe left behind -- which is not a valid clock at all.
	 */
	if (mem->spi->max_speed_hz != spi->speed_hz)
		esp32_spi_set_clock(spi, mem->spi->max_speed_hz);

	/*
	 * Mode 0, and every hardware chip select disabled. CS is a plain GPIO
	 * the panel driver toggles, so the peripheral must not drive CS0 -- a
	 * CS edge would end the pixel burst.
	 */
	esp32_spi_write(spi, SPI_MISC_REG, SPI_CS_DIS);

	/* Line width for the command phase. */
	if (op->cmd.buswidth == 4)
		ctrl |= SPI_FCMD_QUAD;
	else if (op->cmd.buswidth == 2)
		ctrl |= SPI_FCMD_DUAL;

	if (op->addr.nbytes) {
		if (op->addr.buswidth == 4)
			ctrl |= SPI_FADDR_QUAD;
		else if (op->addr.buswidth == 2)
			ctrl |= SPI_FADDR_DUAL;
		/* Address goes in the top of the register, as the hardware wants. */
		if (op->addr.nbytes == 3)
			addr = op->addr.val << 8;
		else if (op->addr.nbytes == 4)
			addr = op->addr.val;
		else
			addr = op->addr.val;
	}

	if (op->data.nbytes) {
		if (op->data.buswidth == 4)
			cr |= SPI_FWRITE_QUAD;
		if (op->data.dir == SPI_MEM_DATA_IN)
			cr |= SPI_USR_MISO;
		else
			cr |= SPI_USR_MOSI;
		pdma_len = op->data.nbytes * 8 - 1;
		(void)tx;
		(void)rx;
	}

	if (op->cmd.nbytes)
		cr |= SPI_USR_COMMAND;
	if (op->addr.nbytes)
		cr |= SPI_USR_ADDR;
	if (op->dummy.nbytes)
		cr |= SPI_USR_DUMMY;

	esp32_spi_write(spi, SPI_CTRL_REG, ctrl);
	esp32_spi_write(spi, SPI_USER_REG, cr);

	if (op->cmd.nbytes)
		esp32_spi_write(spi, SPI_USER2_REG,
				FIELD_PREP(SPI_USR_COMMAND_VALUE, op->cmd.opcode) |
				FIELD_PREP(SPI_USR_COMMAND_BITLEN,
					   op->cmd.nbytes * 8 - 1));
	else
		esp32_spi_write(spi, SPI_USER2_REG, 0);

	esp32_spi_write(spi, SPI_USER1_REG,
			FIELD_PREP(SPI_USR_ADDR_BITLEN,
				   op->addr.nbytes ? op->addr.nbytes * 8 - 1 : 0) |
			FIELD_PREP(SPI_USR_DUMMY_CYCLELEN,
				   op->dummy.nbytes ? op->dummy.nbytes * 8 - 1 : 0));

	esp32_spi_write(spi, SPI_ADDR_REG, addr);
	esp32_spi_write(spi, SPI_MS_DLEN_REG,
			FIELD_PREP(SPI_MS_DATA_BITLEN, pdma_len));

	/*
	 * One-time register dump, in the same layout as the SPIDUMP block in
	 * TinyPetz main/panel.c. That firmware provably works, so diffing the
	 * two is the fastest way to find a misprogrammed field.
	 */
	{
		static bool dumped;

		if (!dumped) {
			dumped = true;
			dev_info(mem->spi->controller->dev.parent,
				 "regs cmd=%08x ctrl=%08x clock=%08x user=%08x u1=%08x u2=%08x dlen=%08x addr=%08x misc=%08x\n",
				 esp32_spi_read(spi, SPI_CMD_REG),
				 esp32_spi_read(spi, SPI_CTRL_REG),
				 esp32_spi_read(spi, SPI_CLOCK_REG),
				 esp32_spi_read(spi, SPI_USER_REG),
				 esp32_spi_read(spi, SPI_USER1_REG),
				 esp32_spi_read(spi, SPI_USER2_REG),
				 esp32_spi_read(spi, SPI_MS_DLEN_REG),
				 esp32_spi_read(spi, SPI_ADDR_REG),
				 esp32_spi_read(spi, SPI_MISC_REG));
		}
	}

	/*
	 * FIFO path only: this driver does not use DMA for host.io yet.
	 *
	 * The command and address phases belong to the FIRST chunk alone. Every
	 * later chunk is a continuation of the same burst and must go out as
	 * bare data, exactly as ESP-IDF does with VARIABLE_CMD/VARIABLE_ADDR at
	 * zero bits. Leaving SPI_USR_COMMAND/SPI_USR_ADDR set re-sends them on
	 * every 64-byte chunk: for the CO5300 that restarts the write pointer
	 * at the window origin 8000 times per frame, and the panel receives
	 * nothing but the first 32 pixels over and over.
	 */
	bool first = true;
	/*
	 * Per-chunk MMIO was the whole cost of a frame, not the wire.
	 *
	 * Measured on this board: a full 256000-byte frame takes 94.8 ms at
	 * 40 MHz and 108.8 ms at 20 MHz -- only 15 percent worse for half the
	 * clock, so the transfer is not wire bound. Wire time for a 64-byte
	 * chunk at 4 lanes and 40 MHz is 3.2 us, against 23.7 us measured per
	 * chunk, so about 20 us of every chunk was register work.
	 *
	 * Three of those register writes were redundant on all but a couple of
	 * chunks per transfer:
	 *
	 *   - SPI_USER was rewritten for every chunk after the first, with the
	 *     same value every time. It only changes once, when the command
	 *     and address bits are cleared for the continuation.
	 *   - SPI_MS_DLEN was rewritten for every chunk although only the last
	 *     partial chunk differs.
	 *   - SPI_CMD=SPI_UPDATE, plus a poll waiting for it to clear, ran per
	 *     chunk. It only has to be done when a config register changed.
	 *
	 * Steady state is now: fill the FIFO, write SPI_USR, poll until done.
	 */
	bool need_update = true;	/* config written above is not latched */
	unsigned int last_n = 0;

	while (remaining) {
		unsigned int n = min(remaining, ESP32_SPI_FIFO_SIZE);
		u32 n32 = n & -4;
		u32 r32 = n - n32;

		if (!first && (cr & (SPI_USR_COMMAND | SPI_USR_ADDR |
				     SPI_USR_DUMMY))) {
			cr &= ~(SPI_USR_COMMAND | SPI_USR_ADDR | SPI_USR_DUMMY);
			esp32_spi_write(spi, SPI_USER_REG, cr);
			need_update = true;
		}

		if (n != last_n) {
			esp32_spi_write(spi, SPI_MS_DLEN_REG,
					FIELD_PREP(SPI_MS_DATA_BITLEN, n * 8 - 1));
			last_n = n;
			need_update = true;
		}

		if (tx) {
			if (n32)
				memcpy_toio(spi->regs + SPI_W0_REG, tx, n32);
			if (r32) {
				u32 w = 0;

				memcpy(&w, tx + n32, r32);
				memcpy_toio(spi->regs + SPI_W0_REG + n32, &w, 4);
			}
			tx += n;
		}

		if (need_update) {
			esp32_spi_write(spi, SPI_CMD_REG, SPI_UPDATE);
			if (esp32_spi_wait_bit(spi, SPI_UPDATE))
				return -ETIMEDOUT;
			need_update = false;
		}
		esp32_spi_write(spi, SPI_CMD_REG, SPI_USR);
		if (esp32_spi_wait_bit(spi, SPI_USR))
			return -ETIMEDOUT;

		if (rx) {
			if (n32)
				memcpy_fromio(rx, spi->regs + SPI_W0_REG, n32);
			if (r32) {
				u32 w;

				memcpy_fromio(&w, spi->regs + SPI_W0_REG + n32, 4);
				memcpy(rx + n32, &w, r32);
			}
			rx += n;
		}
		remaining -= n;
		first = false;
	}

	/*
	 * The completion interrupt only needs clearing once per transfer, not
	 * once per chunk. INT_ENA is zero in polling mode so this is a latched
	 * raw bit; clearing it here keeps the next transfer's detection honest
	 * without an extra read and write on every chunk.
	 */
	if (op->data.nbytes)
		esp32_spi_write(spi, SPI_DMA_INT_CLR_REG, SPI_TRANS_DONE_INT);

	if (!op->data.nbytes) {
		if (esp32_spi_start_transfer(spi) || esp32_spi_wait(spi))
			return -ETIMEDOUT;
	}

	return 0;
}

static const struct spi_controller_mem_ops esp32_spi_mem_ops = {
	.exec_op = esp32_spi_exec_op,
};

static int esp32_spi_start_transfer(struct esp32_spi *spi)
{
	/* SPI_CMD_REG */
	esp32_spi_write(spi, SPI_CMD_REG, SPI_UPDATE);
	if (esp32_spi_wait_bit(spi, SPI_UPDATE))
		return -ETIMEDOUT;
	esp32_spi_write(spi, SPI_CMD_REG, SPI_USR);
	return 0;
}

static irqreturn_t esp32_spi_irq(int irq, void *dev_id)
{
	struct esp32_spi *spi = dev_id;
	u32 status = esp32_spi_read(spi, SPI_DMA_INT_ST_REG);

	if (status) {
		esp32_spi_write(spi, SPI_DMA_INT_ENA_REG, 0);
		complete(&spi->done);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int esp32_spi_wait(struct esp32_spi *spi)
{
	if (esp32_spi_wait_bit(spi, SPI_USR))
		return -ETIMEDOUT;
	if (esp32_spi_read(spi, SPI_DMA_INT_RAW_REG) & SPI_TRANS_DONE_INT) {
		esp32_spi_write(spi, SPI_DMA_INT_CLR_REG, SPI_TRANS_DONE_INT);
	} else {
		WARN_ONCE(1, "wait complete, but no SPI_TRANS_DONE_INT\n");
	}
	return 0;
#if 0
		reinit_completion(&spi->done);
		esp32_spi_write(spi, SPI_DMA_INT_ENA_REG, SPI_TRANS_DONE_INT);
		wait_for_completion(&spi->done);
		esp32_spi_write(spi, SPI_DMA_INT_CLR_REG, SPI_TRANS_DONE_INT);
#endif
}

static int esp32_spi_transfer_one(struct spi_controller *host,
				  struct spi_device *device,
				  struct spi_transfer *t)
{
	struct esp32_spi *spi = spi_controller_get_devdata(host);
	const u8 *tx_ptr = t->tx_buf;
	u8 *rx_ptr = t->rx_buf;
	unsigned int remaining_words = t->len;

	while (remaining_words) {
		unsigned int n_words = min(remaining_words, ESP32_SPI_FIFO_SIZE);
		u32 n32 = n_words & -4;
		u32 r32 = n_words - n32;

		esp32_spi_prepare_transfer(spi, device, t, n_words);

		if (n32)
			memcpy_toio(spi->regs + SPI_W0_REG, tx_ptr, n32);
		if (r32) {
			u32 w = 0;

			memcpy(&w, tx_ptr + n32, r32);
			memcpy_toio(spi->regs + SPI_W0_REG + n32, &w, 4);
		}
		tx_ptr += n_words;

		if (esp32_spi_start_transfer(spi) || esp32_spi_wait(spi))
			return -ETIMEDOUT;

		if (rx_ptr) {
			if (n32)
				memcpy_fromio(rx_ptr, spi->regs + SPI_W0_REG, n32);
			if (r32) {
				u32 w;

				memcpy_fromio(&w, spi->regs + SPI_W0_REG + n32, 4);
				memcpy(rx_ptr + n32, &w, r32);
			}
			rx_ptr += n_words;
		}

		remaining_words -= n_words;
	}
	return 0;
}

/*
 * Does the SPI clock actually reach the pad?
 *
 * Nothing on this bus ever answers, so a completed transaction proves only that
 * the peripheral thinks it shifted bits. A pad's input buffer reads its own
 * driven level, though, so the clock can be observed from software: run one
 * slow transfer and count transitions on the clock pin while it runs.
 *
 * 100 kHz over 512 bits is ~5 ms of continuous clocking, which is long enough
 * for a polling loop to sample the pin many times per bit period.
 */
static void esp32_spi_clk_selftest(struct esp32_spi *spi, struct device *dev,
				   void __iomem *iopad, void __iomem *gpio,
				   u32 clkpin)
{
	bool bank1 = clkpin >= 32;
	u32 in_off = bank1 ? 0x40 : 0x3c;
	u32 bit = BIT(bank1 ? clkpin - 32 : clkpin);
	unsigned long deadline;
	u32 prev, edges = 0, samples = 0, cmd_seen;
	bool usr_cleared = false;
	int i;

	/* Let the pad's input buffer see its own output. */
	iowrite32(ioread32(iopad + IO_MUX_OFF(clkpin)) | IO_MUX_FUN_IE,
		  iopad + IO_MUX_OFF(clkpin));

	/*
	 * 2 MHz, not 100 kHz: SPI_CLKDIV_PRE is four bits wide, so the slowest
	 * representable clock is about 78 kHz and anything lower silently wraps.
	 * 2 MHz divides exactly (clkcnt = 39) and 512 bits still lasts 256 us.
	 */
	esp32_spi_set_clock(spi, 2000000);

	esp32_spi_write(spi, SPI_CTRL_REG, 0);
	esp32_spi_write(spi, SPI_MISC_REG, SPI_CS_DIS);
	esp32_spi_write(spi, SPI_USER1_REG, 0);
	esp32_spi_write(spi, SPI_USER2_REG, 0);
	esp32_spi_write(spi, SPI_USER_REG, SPI_USR_MOSI);
	esp32_spi_write(spi, SPI_MS_DLEN_REG,
			FIELD_PREP(SPI_MS_DATA_BITLEN, 64 * 8 - 1));
	for (i = 0; i < 64; i += 4)
		esp32_spi_write(spi, SPI_W0_REG + i, 0xA5A5A5A5);

	prev = ioread32(gpio + in_off);
	if (esp32_spi_start_transfer(spi)) {
		dev_warn(dev, "clk self-test: transfer did not start\n");
		return;
	}

	deadline = jiffies + msecs_to_jiffies(50);
	while (esp32_spi_read(spi, SPI_CMD_REG) & SPI_USR) {
		u32 now = ioread32(gpio + in_off);

		if ((now ^ prev) & bit)
			edges++;
		prev = now;
		samples++;
		if (time_after(jiffies, deadline))
			break;
	}
	usr_cleared = !(esp32_spi_read(spi, SPI_CMD_REG) & SPI_USR);
	cmd_seen = esp32_spi_read(spi, SPI_CMD_REG);

	dev_info(dev, "clk pin %u: %u edges in %u samples, usr_cleared=%d cmd=%08x dma_raw=%08x -- %s\n",
		 clkpin, edges, samples, usr_cleared, cmd_seen,
		 esp32_spi_read(spi, SPI_DMA_INT_RAW_REG),
		 edges > 2 ? "SPI clock REACHES the pad" : "NO clock on the pad");
}

/*
 * Route one pad: put it in plain-GPIO mode, point its matrix output selector at
 * `sig`, and enable the pad's output driver.
 */
static void esp32_spi_route_pin(void __iomem *iopad, void __iomem *gpio,
				u32 pin, u16 sig)
{
	u32 v;

	v = ioread32(iopad + IO_MUX_OFF(pin));
	v &= ~(IO_MUX_MCU_SEL_MASK | IO_MUX_FUN_DRV_MASK | IO_MUX_FUN_IE);
	v |= IO_MUX_MCU_SEL_GPIO | IO_MUX_FUN_DRV_20MA;
	iowrite32(v, iopad + IO_MUX_OFF(pin));

	/* Output enable taken from GPIO_ENABLE, not from a peripheral signal:
	 * during phases the peripheral leaves idle the pad still has to hold
	 * its level, and on a quad bus those idle levels are what HOLD#/WP#
	 * read. */
	iowrite32(GPIO_FUNC_OEN_SEL | sig, gpio + GPIO_OUT_SEL_OFF(pin));

	if (pin < 32)
		iowrite32(BIT(pin), gpio + GPIO_ENABLE_W1TS_OFF);
	else
		iowrite32(BIT(pin - 32), gpio + GPIO_ENABLE1_W1TS_OFF);
}

/*
 * Read the pin assignment from the controller node and route it.
 *
 * Entirely optional: a board whose signals do reach IO_MUX needs none of this,
 * and a node without these properties behaves exactly as before.
 */
static void esp32_spi_route_pins(struct esp32_spi *spi, struct device *dev)
{
	static const char * const names[] = {
		"espressif,clk-pin", "espressif,d0-pin", "espressif,d1-pin",
		"espressif,d2-pin", "espressif,d3-pin",
	};
	const struct esp32_spi_sig *sig;
	void __iomem *iopad, *gpio;
	unsigned int inst;
	u32 pin, clk_pin = 0;
	bool have_clk = false;
	int i;

	inst = (spi->phys == 0x60025000) ? 3 : 2;
	sig = &esp32_spi_sig[inst];

	{
		u16 idx[5] = { sig->clk, sig->d, sig->q, sig->wp, sig->hd };

		iopad = devm_ioremap(dev, ESP32_IO_MUX_BASE, 0x1000);
		gpio = devm_ioremap(dev, ESP32_GPIO_BASE, 0x1000);
		if (!iopad || !gpio) {
			dev_warn(dev, "cannot map IO_MUX/GPIO matrix, pins unrouted\n");
			return;
		}

		for (i = 0; i < ARRAY_SIZE(names); i++) {
			if (of_property_read_u32(dev->of_node, names[i], &pin))
				continue;
			esp32_spi_route_pin(iopad, gpio, pin, idx[i]);
			if (i == 0) {
				clk_pin = pin;
				have_clk = true;
			}
		}

		/* Read back what landed: a silent write here is exactly the
		 * failure mode that costs an evening. */
		for (i = 0; i < ARRAY_SIZE(names); i++) {
			if (of_property_read_u32(dev->of_node, names[i], &pin))
				continue;
			dev_info(dev, "route %s pin %u -> sig %u: iomux=%08x out_sel=%08x enable=%08x\n",
				 names[i], pin, idx[i],
				 ioread32(iopad + IO_MUX_OFF(pin)),
				 ioread32(gpio + GPIO_OUT_SEL_OFF(pin)),
				 ioread32(gpio + (pin < 32 ? 0x20 : 0x2c)));
		}

		spi->iopad = iopad;
		spi->gpio = gpio;
		spi->have_clk = have_clk;
		spi->clk_pin = clk_pin;
	}
}

static int esp32_spi_probe(struct platform_device *pdev)
{
	struct esp32_spi *spi;
	int ret, irq;
	struct spi_controller *host;
	struct reset_control *rst;
	struct device_node *np = pdev->dev.of_node;
	u32 cs_num;

	host = spi_alloc_host(&pdev->dev, sizeof(struct esp32_spi));
	if (!host) {
		dev_err(&pdev->dev, "out of memory\n");
		return -ENOMEM;
	}

	spi = spi_controller_get_devdata(host);
	init_completion(&spi->done);
	platform_set_drvdata(pdev, host);

	ret = of_property_read_u32(np, "spi-num-chipselects", &cs_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to get spi-num-chipselects\n");
		goto put_host;
	}
	host->num_chipselect = cs_num;

	{
		struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

		if (res)
			spi->phys = res->start;
	}

	spi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(spi->regs)) {
		ret = PTR_ERR(spi->regs);
		goto put_host;
	}

	spi->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(spi->clk)) {
		dev_err(&pdev->dev, "Unable to find bus clock\n");
		ret = PTR_ERR(spi->clk);
		goto put_host;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto put_host;
	}

	/* Spin up the bus clock before hitting registers */
	ret = clk_prepare_enable(spi->clk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable bus clock\n");
		goto put_host;
	}
	rst = devm_reset_control_get(&pdev->dev, "reset");
	if (!IS_ERR(rst))
		reset_control_deassert(rst);

	esp32_spi_route_pins(spi, &pdev->dev);

	/* Define our host */
	host->dev.of_node = np;
	host->max_speed_hz = clk_get_rate(spi->clk);

	// Escapement: this driver had no probe logging at all, which made
	// "did it bind?" unanswerable from the console.
	dev_info(&pdev->dev, "probed: bus %d, clk %u Hz\n",
		 pdev->id, host->max_speed_hz);

	/*
	 * One-time GDMA occupancy dump.
	 *
	 * Diagnostic, not configuration: nothing is written and no channel is
	 * claimed. A channel the firmware has configured will show a non-zero
	 * LINK register or a non-zero PERI_SEL; a free one reads zero in all
	 * four. Printed once so a single boot answers the question, because the
	 * answer decides whether the panel driver may use DMA at all.
	 */
	{
		void __iomem *gd = devm_ioremap(&pdev->dev, ESP32_GDMA_BASE, 0x400);
		int n;

		if (!gd) {
			dev_warn(&pdev->dev, "cannot map GDMA at %08x\n",
				 ESP32_GDMA_BASE);
		} else {
			for (n = 0; n < ESP32_GDMA_CHANS; n++)
				dev_info(&pdev->dev,
					 "gdma ch%d: in_conf0=%08x in_link=%08x in_sel=%08x out_conf0=%08x out_link=%08x out_sel=%08x\n",
					 n,
					 ioread32(gd + GDMA_IN_CONF0(n)),
					 ioread32(gd + GDMA_IN_LINK(n)),
					 ioread32(gd + GDMA_IN_PERI_SEL(n)),
					 ioread32(gd + GDMA_OUT_CONF0(n)),
					 ioread32(gd + GDMA_OUT_LINK(n)),
					 ioread32(gd + GDMA_OUT_PERI_SEL(n)));
		}
	}
	host->bus_num = pdev->id;
	host->mode_bits = SPI_CPHA | SPI_CPOL |
		SPI_CS_HIGH | SPI_LSB_FIRST |
		SPI_TX_DUAL | SPI_TX_QUAD |
		SPI_RX_DUAL | SPI_RX_QUAD;
	host->bits_per_word_mask = SPI_BPW_RANGE_MASK(1, 32);
	host->mem_ops = &esp32_spi_mem_ops;
	host->prepare_message = esp32_spi_prepare_message;
	host->set_cs = esp32_spi_set_cs;
	host->transfer_one = esp32_spi_transfer_one;

	pdev->dev.dma_mask = NULL;
	/* Configure the SPI host hardware */
	esp32_spi_init(spi);

	/*
	 * Only meaningful from here: esp32_spi_init() is what enables the
	 * peripheral's clock gate, and until it runs every transaction stalls
	 * with SPI_USR never clearing -- a silent hang, exactly as recorded in
	 * this project's bring-up notes.
	 */
	if (spi->have_clk)
		esp32_spi_clk_selftest(spi, &pdev->dev, spi->iopad, spi->gpio,
				       spi->clk_pin);

	/* Register for SPI Interrupt */
	ret = devm_request_irq(&pdev->dev, irq, esp32_spi_irq, 0,
			       dev_name(&pdev->dev), spi);
	if (ret) {
		dev_err(&pdev->dev, "Unable to bind to interrupt\n");
		goto disable_clk;
	}

	ret = devm_spi_register_controller(&pdev->dev, host);
	if (ret < 0) {
		dev_err(&pdev->dev, "spi_register_host failed\n");
		goto disable_clk;
	}

	return 0;

disable_clk:
	clk_disable_unprepare(spi->clk);
put_host:
	spi_controller_put(host);

	return ret;
}

static void esp32_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *host = platform_get_drvdata(pdev);
	struct esp32_spi *spi = spi_controller_get_devdata(host);

	esp32_spi_write(spi, SPI_DMA_INT_ENA_REG, 0);
	clk_disable_unprepare(spi->clk);
}

static int esp32_spi_suspend(struct device *dev)
{
	struct spi_controller *host = dev_get_drvdata(dev);
	struct esp32_spi *spi = spi_controller_get_devdata(host);
	int ret;

	ret = spi_controller_suspend(host);
	if (ret)
		return ret;

	esp32_spi_write(spi, SPI_DMA_INT_ENA_REG, 0);
	clk_disable_unprepare(spi->clk);

	return ret;
}

static int esp32_spi_resume(struct device *dev)
{
	struct spi_controller *host = dev_get_drvdata(dev);
	struct esp32_spi *spi = spi_controller_get_devdata(host);
	int ret;

	ret = clk_prepare_enable(spi->clk);
	if (ret)
		return ret;
	ret = spi_controller_resume(host);
	if (ret)
		clk_disable_unprepare(spi->clk);

	return ret;
}

static DEFINE_SIMPLE_DEV_PM_OPS(esp32_spi_pm_ops,
				esp32_spi_suspend, esp32_spi_resume);


static const struct of_device_id esp32_spi_of_match[] = {
	{ .compatible = "esp,esp32s3-spi", },
	{}
};
MODULE_DEVICE_TABLE(of, esp32_spi_of_match);

static struct platform_driver esp32_spi_driver = {
	.probe = esp32_spi_probe,
	.remove = esp32_spi_remove,
	.driver = {
		.name = ESP32_SPI_DRIVER_NAME,
		.pm = &esp32_spi_pm_ops,
		.of_match_table = esp32_spi_of_match,
	},
};
module_platform_driver(esp32_spi_driver);

MODULE_AUTHOR("Max Filippov <jcmvbkbc@gmail.com>");
MODULE_DESCRIPTION("ESP32 SPI driver");
MODULE_LICENSE("GPL");
