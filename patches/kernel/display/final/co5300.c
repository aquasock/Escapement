// co5300.c - the T-Watch Ultra's AMOLED panel, as an SPI client.
//
// This replaces an earlier version that programmed the SPI peripheral directly.
// That was the wrong shape: the kernel's own esp32-spi driver already handles
// clock gating, reset and register mapping, and it demonstrably binds to SPI3
// on this board:
//
//     esp32_spi 60025000.spi: probed: bus -1, clk 80000000 Hz
//
// Everything about the panel below is taken from LilyGo's own MIT-licensed
// driver (LilyGoLib, src/display/LilyGoDispInterface.cpp) rather than inferred:
//
//   host      SPI3_HOST for the T-Watch Ultra (SPI2 only for the LoRa Pager)
//   mode      SPI_MODE0, command_bits 8, address_bits 24, CS driven by hand
//   reset     HIGH 200 ms, LOW 300 ms, HIGH 200 ms
//   register  0x02 then a 24-bit address carrying the register byte at 23:16
//   pixels    0x32 then address 0x002C00, then the data stream, CS held
//
// spi_transfer cannot express separate command and address phases, so they are
// sent as one byte stream: 0x02, addr[23:16], addr[15:8], addr[7:0], then any
// parameters. The pixel stream uses the same shape with 0x32.
//
// The panel is write-only: it never acknowledges and never returns a byte. The
// only evidence anything worked is the glass -- and GPIO6, the tearing-effect
// output, which pulses once per refresh while the controller scans. That pin is
// the instrument this driver reports on.

#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/gpio/consumer.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "co5300"

/*
 * Panel API: writing rectangles into the strips around fb0.
 *
 * Decided and specified in docs/panel-api.md; this is the implementation.
 *
 * The shape of the decision, and why it is an ioctl on /dev/fb0 rather than a
 * new device node: fb0 already exists and is already openable by the programs
 * that need this. A new node would need an mdev rule, and mdev rules are exactly
 * the /etc state a full-image flash destroys (FLS-003) -- it would have to be
 * recreated by hand after every reflash. The fbdev core forwards unrecognised
 * ioctls here, so no core change is needed.
 */
#include <linux/ioctl.h>
#include <linux/ktime.h>
#include "co5300_rect.h"

/* The rect structs and the bounds check live in co5300_rect.h so the
 * arithmetic can be tested on a build host; see scripts/co5300-selftest.py. */
#define CO5300_IOC_WRITE_RECT \
	_IOW(CO5300_IOC_MAGIC, 1, struct co5300_rect)
#define CO5300_IOC_GET_GEOM \
	_IOR(CO5300_IOC_MAGIC, 2, struct co5300_geom)

/*
 * Diagnostic only: time the same pixel payload twice, once with CS asserted so
 * the panel receives it and once with CS never asserted so the peripheral
 * clocks the bytes out into nothing.
 *
 * The difference is the panel's contribution. If both are equal, the cost is
 * the driver and the CPU -- register pokes and the completion poll -- and DMA
 * would help. If the asserted run is much slower, the panel is gating the bus
 * and DMA would buy nothing, because the wire is not the constraint.
 *
 * This does not write visible pixels of any value: the buffer is the transmit
 * buffer as it stands, and a CS-high transfer is ignored by the controller.
 */
struct co5300_bench {
	__u16 w, h;	/* payload shape, panel pixels */
	__u16 iters;	/* repetitions, 1..64 */
	__u16 reserved;
	__u32 cs_us;	/* mean microseconds per iteration, CS asserted */
	__u32 nocs_us;	/* mean microseconds per iteration, CS not asserted */
	__u32 bytes;	/* payload bytes per iteration */
};

#define CO5300_IOC_BENCH \
	_IOWR(CO5300_IOC_MAGIC, 3, struct co5300_bench)

/*
 * Rows per transfer for CO5300_IOC_WRITE_RECT, capped hard.
 *
 * The transmit buffer could hold a whole frame, and the first version let a
 * rectangle go out as one op so long as it fit. On the hardware that tears: a
 * single 45x356 write to the right-hand strip came back as eight diagonal
 * splits, worst at the bottom. The same rectangle written as 178 windows of
 * 45x2 is clean, repeatedly.
 *
 * What is established, from the glass:
 *
 *   45x356 one op, right strip   torn
 *   45x71  one op, right strip   torn -- though a 35-wide segment happened to
 *                                come out clean, so width is not the rule
 *   45x2   per op, right strip   clean
 *   45x356 one op, left strip    clean
 *
 * So the rule is not width and not position: it is how much a single transfer
 * carries, with the right-hand strip tolerating less than the left. The ROOT
 * CAUSE IS NOT ESTABLISHED. This is a workaround that has held every time it
 * was tried, not an explanation.
 *
 * It is acceptable here because these are status icons that change on battery
 * or link state. It would not be acceptable for continuous drawing, and
 * co5300_blit() -- fbcon's path -- keeps its own uncapped sizing.
 *
 * The same signature is recorded for the reference firmware at 45 MHz ("dark
 * horizontal lines shearing slightly down to the right", cured by dropping to
 * 20 MHz), which is why SPI clock stays a suspect once the mechanism is worth
 * the cycles to find.
 */
#define CO5300_RECT_ROWS_PER_BAND	2

/*
 * SPI clock, as a runtime knob rather than a #define.
 *
 * Three things on this project collide over this number. The panel is
 * documented as running at 20 MHz in the reference firmware and that is
 * "proven", so it is the default. But 20 MHz caps a full-frame transfer at
 * about 45 ms, which is the difference between the emulator keeping real time
 * and not. And the evidence that 45 MHz was unsafe comes from the reference
 * firmware -- which also carried the SPI_USR_COMMAND/SPI_USR_ADDR-resent-per-
 * chunk bug that restarts the panel's write pointer every 64 bytes. That bug
 * is fixed here. A write-pointer desync and a clock-margin failure look the
 * same on the glass, so the 45 MHz verdict may have been about the bug.
 *
 * Making it writable at runtime means one flash answers the question: write a
 * value to /sys/module/co5300/parameters/spi_hz, force a repaint, and look.
 * Testing three clocks by rebuilding three kernels would cost three 16 MiB
 * backups and three boot cycles to learn the same thing.
 *
 * The setter has to reach the LIVE spi_device, not just the module variable:
 * a plain module_param() would store the new number and change nothing, since
 * the value actually used is spi->max_speed_hz. esp32_spi_exec_op() compares
 * that against the programmed speed on every op and reprograms the divider, so
 * assigning it here takes effect on the next transfer without re-probing.
 */
#define CO5300_SPI_HZ_DEFAULT	20000000u
#define CO5300_SPI_HZ_MIN	 1000000u
#define CO5300_SPI_HZ_MAX	80000000u

static unsigned int spi_hz = CO5300_SPI_HZ_DEFAULT;

/* Set in probe; the parameter setter needs it to reach the live device. */
static struct spi_device *co5300_spi_dev;

static int co5300_spi_hz_set(const char *val, const struct kernel_param *kp)
{
	u32 hz;
	int rc = kstrtou32(val, 0, &hz);

	if (rc)
		return rc;
	if (hz < CO5300_SPI_HZ_MIN || hz > CO5300_SPI_HZ_MAX)
		return -EINVAL;

	((unsigned int *)kp->arg)[0] = hz;

	/*
	 * Assigning max_speed_hz touches no hardware: it is the contract with the
	 * SPI core, and esp32_spi_exec_op() compares it against the programmed
	 * speed on every op and reprograms the divider when they differ. So the
	 * new clock takes effect at the next op -- which is a safe boundary --
	 * rather than in the middle of a pixel burst.
	 */
	if (co5300_spi_dev)
		co5300_spi_dev->max_speed_hz = hz;

	return 0;
}

static const struct kernel_param_ops co5300_spi_hz_ops = {
	.set = co5300_spi_hz_set,
	.get = param_get_uint,
};

module_param_cb(spi_hz, &co5300_spi_hz_ops, &spi_hz, 0644);
MODULE_PARM_DESC(spi_hz, "CO5300 SPI clock in Hz (default 20000000; try 30000000 and 40000000)");

#define PANEL_W 410
#define PANEL_H 502
#define PANEL_OFFSET_X 22

/*
 * The glass has 45-degree chamfered corners, not circular ones, so these are
 * the only rectangle dimensions that are guaranteed to render in full. Measured
 * on hardware; see docs/panel-geometry.md for the method and the numbers.
 *
 *    panel 410x502  -  inset 45 per side  =  320x412 usable
 */
#define PANEL_SAFE_INSET	45
#define PANEL_SAFE_W		(PANEL_W - 2 * PANEL_SAFE_INSET)	/* 320 */
#define PANEL_SAFE_H		(PANEL_H - 2 * PANEL_SAFE_INSET)	/* 412 */

/*
 * The framebuffer IS the console area, not the whole glass.
 *
 * That is the whole point of the split: fbcon lays a console across whatever
 * the framebuffer says it is, so making fb0 320x410 gives 40 columns by 25
 * rows at the 8x16 VGA cell with no driver-side arithmetic. The 45/46 px
 * border that the chamfered corners force stays outside fb0, free for
 * notifications and status strips.
 *
 * FB_X and FB_Y are where fb0 sits on the glass; co5300_blit() adds
 * PANEL_OFFSET_X on top for the controller's own column offset.
 */
#define FB_X		((PANEL_W - PANEL_SAFE_W) / 2)	/* 45 */
/*
 * 45, not 46: (502 - 412) / 2 is 45. An earlier comment here said 46 and the
 * project documentation repeated it, but the running driver is the authority --
 * the probe log prints the ring at "rows 44/445", and those are FB_Y-1 and
 * FB_Y+FB_H, which only reconcile with FB_Y = 45. fb0 therefore covers rows
 * 45..444 and the strip below it starts at 445.
 */
#define FB_Y		((PANEL_H - PANEL_SAFE_H) / 2)	/* 45 */
#define FB_W		PANEL_SAFE_W			/* 320 */
#define FB_H		400				/* 25 rows of 16 */
#define BPP 16

struct co5300 {
	struct spi_mem *mem;
	struct spi_device *spi;
	struct gpio_desc *cs;
	struct gpio_desc *rst;
	struct gpio_desc *te;

	struct fb_info *info;
	u16 *fb;
	size_t fb_bytes;

	/*
	 * Serialises every panel operation. The panel has no memory of its own and
	 * takes no commands out of order: two concurrent callers would interleave
	 * one window setup with another's pixel stream. fbcon serialises itself
	 * with console_lock, but that does not exclude a userspace ioctl, so this
	 * is what serialises fbcon against userspace. Taken at the entry points
	 * (the four fbcon ops and the ioctl) rather than inside the blit helpers,
	 * so a multi-band operation is one critical section.
	 */
	struct mutex lock;

	u16 *tx;
	size_t tx_len;
	/* spi-mem refuses buffers that live on the stack (spi_mem_check_op
	 * rejects non-DMA-able addresses), so every transfer buffer has to be
	 * heap or static. These two serve the window registers and the small
	 * init payloads. */
	u8 *small;
	u32 pseudo_palette[16];
};


// ---- pin routing -----------------------------------------------------------
//
// These pins have no SPI3 alternate function in IO_MUX, which is why
// SPICOMMON_BUSFLAG_GPIO_PINS is set in LilyGo's bus config: the signals go
// through the GPIO matrix. spi-esp32 does not do this itself (it expects
// pinctrl to have arranged the pads), so the board-specific routing lives here,
// done once at probe.
//
// Signal indices are SPI3's, not SPI2's:
//            SPI3  SPI2(FSPI)
//   CLK        4      101
//   D          1      103
//   Q          0      102
//   HD         2      104
//   WP         3      105

#define GPIO_MATRIX_BASE 0x60004000
#define IO_MUX_BASE      0x60009000
#define GPIO_OUT_SEL(n)  (0x554 + (n) * 4)

#define RDY_VAL          BIT(9)        /* FUN_IE  */
#define MCU_SEL_GPIO     (1 << 12)     /* bits 14:12 */
#define DRV_20MA         (2 << 10)     /* bits 11:10 */

#define SPICLK_OUT_IDX   4
#define SPID_OUT_IDX     1
#define SPIQ_OUT_IDX     0
#define SPIHD_OUT_IDX    2
#define SPIWP_OUT_IDX    3

#define PIN_SCK   40
#define PIN_D0    38
#define PIN_D1    39
#define PIN_D2    42
#define PIN_D3    45

/*
 * IO_MUX offsets are NOT pin*4 above GPIO21. Pins 37..45 are the JTAG and flash
 * pads, laid out by pad name: GPIO37 0x98, 38 0x9c, 39 0xa0 (MTCK), 40 0xa4
 * (MTDO), 41 0xa8 (MTDI), 42 0xac (MTMS), 45 0xb8.
 */
static int co5300_iomux_off(int pin)
{
	switch (pin) {
	case 37: return 0x98;
	case 38: return 0x9c;
	case 39: return 0xa0;
	case 40: return 0xa4;
	case 41: return 0xa8;
	case 42: return 0xac;
	case 45: return 0xb8;
	/*
	 * The generic case is 0x04 + pin*4, not pin*4. The old form pointed one
	 * pad low, so the input-enable meant for GPIO6 (TE) landed on GPIO5 and
	 * the tearing-effect pin read a constant 0 -- which this driver then
	 * reported as "controller NOT scanning".
	 */
	default: return 0x04 + pin * 4;
	}
}

static int co5300_route_pins(struct device *dev)
{
	void __iomem *iomux;

	iomux = devm_ioremap(dev, IO_MUX_BASE, 0x200);
	if (!iomux)
		return -ENOMEM;

	/*
	 * The five SPI signals are NOT routed here. esp32-spi owns them,
	 * because it also has to set the matrix output-enable bit and the GPIO
	 * enable for pins in the high bank -- and an earlier version of this
	 * function rewrote the same OUT_SEL registers without that bit,
	 * switching the pads' output drivers back off. The bus then clocked
	 * fine in the controller's own self-test and drove nothing at all on
	 * the glass.
	 *
	 * What is left is the three plain GPIOs.
	 *
	 * CS (41) and RST (37) need their pad mux set: GPIO41 defaults to
	 * MTDI, a JTAG signal, and CS would never move. TE (6) needs its input
	 * buffer on.
	 */
	iowrite32(MCU_SEL_GPIO | DRV_20MA, iomux + co5300_iomux_off(41));
	iowrite32(MCU_SEL_GPIO | DRV_20MA, iomux + co5300_iomux_off(37));
	iowrite32(MCU_SEL_GPIO | RDY_VAL, iomux + co5300_iomux_off(6));

	return 0;
}

// One spi-mem op, with chip select left entirely to the caller.
//
// Command and address are ALWAYS single-lane; only the pixel payload is quad.
//
// This is the single most expensive mistake in this driver. ESP-IDF's
// SPI_TRANS_MULTILINE_CMD/ADDR are ignored unless a SPI_TRANS_MODE_?IO flag is
// also set, and the working firmware sets neither for register writes and only
// MODE_QIO for the pixel payload. So the panel has never seen a quad command
// byte. Dumping the peripheral on the working firmware settles it:
//
//     SPI_CTRL_REG = 0x00340000     fcmd_quad and faddr_quad both clear
//
// We were writing 0x00000240 -- quad command, quad address, nothing else. The
// panel ignored every register write, which is why it never started scanning.
static int co5300_xfer(struct co5300 *p, u8 cmd, u8 cmd_w, u32 addr, u8 addr_w,
		       const void *out, size_t len, int dw)
{
	struct spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(cmd, cmd_w),
		SPI_MEM_OP_ADDR(3, addr, addr_w),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_NO_DATA);

	/* The DATA macros are brace initialisers, so the phase fields are set
	 * individually rather than assigned a compound literal. */
	if (len) {
		op.data.nbytes = len;
		op.data.buswidth = dw;
		op.data.dir = SPI_MEM_DATA_OUT;
		op.data.buf.out = out;
	}

	return spi_mem_exec_op(p->mem, &op);
}

// One op bracketed by its own chip-select pulse.
static int co5300_op(struct co5300 *p, u8 cmd, u32 addr,
		     const void *out, size_t len, int dw)
{
	int rc;

	/* CS is ACTIVE_LOW: gpiod value 1 means asserted (driven low). */
	if (p->cs)
		gpiod_set_value_cansleep(p->cs, 1);

	rc = co5300_xfer(p, cmd, 1, addr, 1, out, len, dw);

	if (p->cs)
		gpiod_set_value_cansleep(p->cs, 0);

	if (rc)
		pr_err("co5300: op cmd 0x%02X addr 0x%06X len %zu -> %d\n",
		       cmd, addr, len, rc);
	return rc;
}

// A register write: real command and address phases, single-line data.
//
// This used to go through co5300_send(), which is a plain spi_write of
// 0x02 + three address bytes + payload. That delivers the register number as
// *data*, not as an address phase, so the panel never saw a valid command --
// visible in the exec_op trace, where the first op to arrive was the 0x32
// RAMWR rather than the init table's 0xFE.
static int co5300_cmd(struct co5300 *p, u8 reg, const u8 *data, size_t len)
{
	/* Command and address on four lanes; register payload single-line. */
	return co5300_op(p, 0x02, (u32)reg << 8, data, len, 1);
}

// ---- init ------------------------------------------------------------------
//
// The same 11 entries the working ESP-IDF firmware uses, which in turn came
// from the factory image at file offset 0x130dd4.

static const struct {
	u8 op;
	u8 data[4];
	u8 len;		/* low 5 bits = parameter count, 0x80 = settle after */
} co5300_init[] = {
	{ 0xFE, { 0x00 }, 0x01 },
	{ 0xC4, { 0x80 }, 0x01 },
	{ 0x3A, { 0x55 }, 0x01 },	/* RGB565 */
	{ 0x35, { 0x00 }, 0x01 },
	{ 0x53, { 0x20 }, 0x01 },
	{ 0x63, { 0xFF }, 0x01 },
	{ 0x2A, { 0x00, 0x16, 0x01, 0xAF }, 0x04 },
	{ 0x2B, { 0x00, 0x00, 0x01, 0xF5 }, 0x04 },
	{ 0x11, { 0x00 }, 0x80 },	/* sleep out */
	{ 0x29, { 0x00 }, 0x80 },	/* display on */
	{ 0x51, { 0x00 }, 0x01 },	/* brightness; 0xFF follows */
};

static void co5300_init_panel(struct co5300 *p)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(co5300_init); i++) {
		u8 len = co5300_init[i].len & 0x1F;

		/* co5300_init[] is static, so its payload is not on the stack
		 * and spi-mem will accept it directly. */
		co5300_cmd(p, co5300_init[i].op, co5300_init[i].data, len);
		if (co5300_init[i].len & 0x80)
			msleep(120);
	}

	/*
	 * Brightness is not optional. The table sets 0x51 to 0x00 and the
	 * reference firmware then writes 0xFF separately; without it the panel
	 * is initialised, receiving pixels, and showing them on a black
	 * backplane -- which on an AMOLED looks exactly like a dead panel.
	 */
	p->small[0] = 0xFF;
	co5300_cmd(p, 0x51, p->small, 1);
}

// Is the controller scanning? GPIO6 pulses per refresh while it runs.
static int co5300_te_edges(struct co5300 *p, int ms)
{
	int last, edges = 0, i;

	if (!p->te)
		return -1;

	last = gpiod_get_value_cansleep(p->te);
	for (i = 0; i < ms * 20; i++) {
		int now = gpiod_get_value_cansleep(p->te);

		if (now != last) {
			edges++;
			last = now;
		}
		udelay(50);
	}
	return edges;
}

// Push one rectangle. Window once, then stream.
static void co5300_blit(struct co5300 *p, int x0, int y0, int x1, int y1)
{
	u8 *win = p->small;
	const u16 *src = p->fb;
	u16 *dst = p->tx;
	size_t npix = (size_t)(x1 - x0 + 1) * (y1 - y0 + 1);
	int x, y, rc;

	int px0 = FB_X + x0, px1 = FB_X + x1;
	int py0 = FB_Y + y0, py1 = FB_Y + y1;

	win[0] = (px0 + PANEL_OFFSET_X) >> 8;
	win[1] = (px0 + PANEL_OFFSET_X) & 0xFF;
	win[2] = (px1 + PANEL_OFFSET_X) >> 8;
	win[3] = (px1 + PANEL_OFFSET_X) & 0xFF;
	co5300_cmd(p, 0x2A, win, 4);

	win[0] = py0 >> 8; win[1] = py0 & 0xFF;
	win[2] = py1 >> 8; win[3] = py1 & 0xFF;
	co5300_cmd(p, 0x2B, win, 4);

	/*
	 * RAMWR as a plain command: 0x02, address 0x002C00, no payload.
	 *
	 * The working firmware sends this BEFORE the stream, and the 0x32
	 * pixel transfer does not replace it. This was simply missing here, so
	 * the controller was never put into memory-write mode at all.
	 */
	co5300_op(p, 0x02, 0x002C00, NULL, 0, 1);

	/*
	 * Byte-swap into the transmit buffer: the panel wants low byte first
	 * and the peripheral shifts the high byte out first. Without the swap
	 * every colour is rotated, and a flat fill still looks right -- so only
	 * a real image shows it.
	 */
	for (y = y0; y <= y1; y++)
		for (x = x0; x <= x1; x++)
			*dst++ = swab16(src[(size_t)y * FB_W + x]);

	/*
	 * The whole window in ONE op with CS held across all of it.
	 *
	 * The controller treats a CS edge as end-of-burst and silently discards
	 * the rest, so this must not be split per row -- that is what the
	 * previous version did, and it left CS deasserted before a single pixel
	 * had been sent.
	 *
	 * Command and address go out on ONE lane and the pixels on four, which
	 * is ESP-IDF's SPI_TRANS_MODE_QIO. It also means the command and
	 * address are sent once: esp32_spi's exec_op emits them with the first
	 * 64-byte chunk only.
	 */
	if (p->cs)
		gpiod_set_value_cansleep(p->cs, 1);

	rc = co5300_xfer(p, 0x32, 1, 0x002C00, 1, p->tx, npix * 2, 4);

	if (p->cs)
		gpiod_set_value_cansleep(p->cs, 0);

	if (rc)
		pr_err("co5300: pixel stream %zu bytes -> %d\n", npix * 2, rc);
}

/*
 * Send one band of pixels that are already in p->tx, in PANEL coordinates.
 *
 * This is the transfer half of the panel API, factored out so the caller owns
 * the decision of what goes in the buffer. It is the same window, RAMWR and
 * single-op stream as co5300_blit(), with panel coordinates instead of fb0
 * coordinates -- nothing here is offset by FB_X/FB_Y, and PANEL_OFFSET_X is
 * added because the controller's RAM starts 22 columns left of the visible
 * glass.
 *
 * The caller must have byte-swapped the pixels: the panel takes the low byte
 * first and the peripheral shifts the high byte out first (LCD-003).
 */
static int co5300_send_band_cs(struct co5300 *p, int x0, int y0, int x1, int y1,
			       bool assert_cs)
{
	u8 *win = p->small;
	size_t npix = (size_t)(x1 - x0 + 1) * (size_t)(y1 - y0 + 1);
	int rc;

	if (!p->tx || npix * 2 > p->tx_len)
		return -ENOMEM;

	win[0] = (x0 + PANEL_OFFSET_X) >> 8;
	win[1] = (x0 + PANEL_OFFSET_X) & 0xFF;
	win[2] = (x1 + PANEL_OFFSET_X) >> 8;
	win[3] = (x1 + PANEL_OFFSET_X) & 0xFF;
	co5300_cmd(p, 0x2A, win, 4);

	win[0] = y0 >> 8; win[1] = y0 & 0xFF;
	win[2] = y1 >> 8; win[3] = y1 & 0xFF;
	co5300_cmd(p, 0x2B, win, 4);

	co5300_op(p, 0x02, 0x002C00, NULL, 0, 1);

	if (p->cs && assert_cs)
		gpiod_set_value_cansleep(p->cs, 1);

	rc = co5300_xfer(p, 0x32, 1, 0x002C00, 1, p->tx, npix * 2, 4);

	if (p->cs && assert_cs)
		gpiod_set_value_cansleep(p->cs, 0);

	if (rc)
		pr_err("co5300: rect %zu bytes -> %d\n", npix * 2, rc);

	return rc;
}

static int co5300_send_band(struct co5300 *p, int x0, int y0, int x1, int y1)
{
	return co5300_send_band_cs(p, x0, y0, x1, y1, true);
}


// ---- panel-space drawing ---------------------------------------------------

#define RGB565(r, g, b)	((u16)((((r) & 0x1f) << 11) | \
				 (((g) & 0x3f) << 5) | ((b) & 0x1f)))
#define COL_BLACK	0x0000
#define COL_RED		RGB565(31, 0, 0)
#define COL_GREEN	RGB565(0, 63, 0)
#define COL_BLUE	RGB565(0, 0, 31)

/*
 * Three touch buttons across the bottom strip.
 *
 * The strip is everything the framebuffer does not cover below it:
 *
 *   panel 502 rows - fb0 at y 45..444 = 57 rows spare, y 445..501
 *
 * Split into thirds of the 410 px visible glass. The split is deliberately
 * coarse: the CST9217's reported Y is least accurate at the bottom of the
 * glass, so these are regions, not targets. A third is 137 px and the measured
 * Y error is a handful of pixels, so the boundaries are not where a finger can
 * be unlucky.
 *
 * Coordinates here are CONTROLLER columns, which is what the band loop indexes:
 * the controller's RAM starts PANEL_OFFSET_X (22) columns left of the visible
 * glass, so visible panel x maps to controller x + 22, and the visible glass
 * occupies controller columns 22..431.
 *
 *
 *   22 .............. 156  red
 *  157 ... 158         black separator (BTN_SEP)
 *  159 .............. 292  green
 *  293 ... 294         black separator (BTN_SEP)
 *  295 .............. 431  blue
 *
 * Which is 135 / 134 / 137 visible pixels per button, against a finger contact
 * patch of roughly 40 px. The separators are there so the three regions read as
 * three buttons rather than one striped bar.
 *
 * The strip is painted across the FULL controller width, not just the visible
 * 410, so that it also covers the red ring's bottom edge (drawn at y 445 from
 * controller columns 65..388). Otherwise the ring would leave two red dots
 * either side of the button row.
 *
 * The bottom corners of the glass are 45-degree chamfers, so the outermost sliver
 * of each end button is off the glass. Painting the full width is still correct:
 * the alternative leaves a dead corner that looks like a driver fault.
 */
#define BTN_Y0		(FB_Y + FB_H)		/* 445, first row below fb0 */
#define BTN_Y1		(PANEL_H - 1)		/* 501, last row of the glass */
#define BTN_CTRL_X0	0			/* full controller width */
#define BTN_CTRL_X1	(PANEL_OFFSET_X + PANEL_W - 1)	/* 431 */
#define BTN_SEP		2			/* black px between buttons */

/* Controller-column band for button n (0..2), for userspace to mirror. */
#define BTN_EDGE0	(PANEL_OFFSET_X)					/* 22  */
#define BTN_EDGE1	(BTN_EDGE0 + PANEL_W / 3)			/* 158 */
#define BTN_EDGE2	(BTN_EDGE0 + 2 * (PANEL_W / 3))		/* 294 */
#define BTN_EDGE3	(BTN_CTRL_X1)					/* 431 */

/*
 * Ring thickness, in pixels.
 *
 * One visible pixel on every edge, which is not one drawn pixel on every edge.
 * Measured on the glass, one pixel at a time:
 *
 *   uniform 1 px ring  -> right and bottom appeared, left and top did not
 *   uniform 2 px ring  -> right and bottom appeared 2 px, left and top 1 px
 *
 * So exactly one pixel is lost at the left and top edges and none at the right
 * and bottom -- consistent with the first row and column being swallowed, which
 * docs/panel-geometry.md recorded as unresolved when the glass was measured.
 *
 * Left and top are therefore drawn one pixel wider than right and bottom, so
 * that the same thickness survives. Every ring pixel is outside fb0, so the
 * console is never affected either way.
 */
#define RING_T		1
#define RING_LT		(RING_T + 1)

/*
 * Paint the whole panel: black, with a 1 px red outline where fb0 ends.
 *
 * Everything here goes out as FULL-WIDTH bands, and that is the point. An
 * earlier version drew the background with wide fills and then the outline with
 * four thin ones -- 322x1 across and 1x402 down -- and the outline never
 * appeared, while a 432x31 strip in the same place did. Every call returned 0.
 *
 * The controller does not reliably accept a window whose width or height is a
 * single pixel. Rather than find out exactly which dimension it objects to, the
 * outline is drawn inside the band loop: each transfer is a full-width band,
 * identical in shape to the background fill that demonstrably works, and the
 * red pixels are just values in the row buffer.
 */
static int co5300_paint_frame(struct co5300 *p, int l, int r, int t, int b)
{
	size_t width = (size_t)(PANEL_OFFSET_X + PANEL_W);
	size_t max_band, i;
	int y;

	if (!p->tx)
		return -ENOMEM;

	max_band = p->tx_len / (width * 2);
	if (max_band < 1)
		return -ENOMEM;

	for (y = 0; y < PANEL_H; y += (int)max_band) {
		u16 *row = p->tx;
		int y2 = y + (int)max_band - 1;
		size_t npx;
		u8 *win;
		int rc, yy;

		if (y2 > PANEL_H - 1)
			y2 = PANEL_H - 1;

		npx = width * (size_t)(y2 - y + 1);

		/* Background first, then the ring pixels on top of it. */
		for (i = 0; i < npx; i++)
			row[i] = swab16(COL_BLACK);

		for (yy = y; yy <= y2; yy++) {
			u16 *line = row + width * (size_t)(yy - y);
			int x;

			/*
			 * One visible pixel on every edge, which needs different
			 * numbers of drawn pixels per edge: the left and top each
			 * lose one to whatever swallows the first row and column,
			 * the right and bottom lose none. See RING_LT.
			 *
			 *   drawn            ->  visible
			 *   left/top  RING_LT     RING_LT - 1 = 1
			 *   right/bot RING_T      RING_T       = 1
			 */
			if (yy >= t - RING_LT + 1 && yy <= t) {
				for (x = l - RING_LT + 1;
				     x <= r + RING_T - 1; x++)
					line[x] = swab16(COL_RED);
			} else if (yy >= b && yy <= b + RING_T - 1) {
				for (x = l - RING_LT + 1;
				     x <= r + RING_T - 1; x++)
					line[x] = swab16(COL_RED);
			} else if (yy > t && yy < b) {
				for (x = l - RING_LT + 1; x <= l; x++)
					line[x] = swab16(COL_RED);
				for (x = r; x <= r + RING_T - 1; x++)
					line[x] = swab16(COL_RED);
			}

			/*
			 * Touch buttons, on top of everything: the strip sits below
			 * fb0, where the ring's bottom edge also is, so painting last
			 * is what keeps the row clean.
			 */
			if (yy >= BTN_Y0 && yy <= BTN_Y1) {
				for (x = BTN_CTRL_X0; x <= BTN_CTRL_X1; x++) {
					u16 c;

					if (x <= BTN_EDGE1 - BTN_SEP)
						c = COL_RED;
					else if (x <= BTN_EDGE1)
						c = COL_BLACK;
					else if (x <= BTN_EDGE2 - BTN_SEP)
						c = COL_GREEN;
					else if (x <= BTN_EDGE2)
						c = COL_BLACK;
					else
						c = COL_BLUE;

					line[x] = swab16(c);
				}
			}
		}

		win = p->small;
		win[0] = 0; win[1] = 0;
		win[2] = (u8)((width - 1) >> 8); win[3] = (u8)((width - 1) & 0xFF);
		co5300_cmd(p, 0x2A, win, 4);

		win[0] = (u8)(y >> 8); win[1] = (u8)(y & 0xFF);
		win[2] = (u8)(y2 >> 8); win[3] = (u8)(y2 & 0xFF);
		co5300_cmd(p, 0x2B, win, 4);

		co5300_op(p, 0x02, 0x002C00, NULL, 0, 1);

		if (p->cs)
			gpiod_set_value_cansleep(p->cs, 1);
		rc = co5300_xfer(p, 0x32, 1, 0x002C00, 1, row, npx * 2, 4);
		if (p->cs)
			gpiod_set_value_cansleep(p->cs, 0);
		if (rc)
			return rc;
	}

	return 0;
}


//
// The area outside fb0 is not part of the framebuffer, so neither fbcon nor
// userspace can put anything there: it exists only in the panel's own RAM.
// Anything drawn outside the console therefore has to be drawn here, in panel
// coordinates, at probe time.
//
// "Panel coordinates" means what goes into the 0x2A/0x2B window registers --
// the controller's RAM, not the visible glass. The init table windows the RAM
// at 22..431, so the visible 410 px sit at column 22 plus the visible x.


// ---- framebuffer -----------------------------------------------------------

/*
 * Every fbcon drawing primitive has to push to the panel as well as to memory.
 *
 * The cfb_* helpers write into the framebuffer and stop there, which is fine
 * for a display that scans memory out of its own RAM. This panel has no memory
 * of ours -- it is fed over SPI -- so a console drawn with bare cfb_* would
 * fill the buffer and never appear. Each op therefore blits the rectangle it
 * touched straight afterwards.
 *
 * This is slow: the blit path is one spi-mem op per call through a 64-byte
 * FIFO, so a full 40x25 repaint is thousands of register writes. It is correct
 * first; batching a dirty region comes later.
 */
static void co5300_flush(struct fb_info *info, const struct fb_fillrect *r)
{
	struct co5300 *p = info->par;
	int x0 = r->dx, y0 = r->dy;
	int x1 = r->dx + r->width - 1;
	int y1 = r->dy + r->height - 1;

	if (x1 < x0 || y1 < y0)
		return;
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > FB_W - 1) x1 = FB_W - 1;
	if (y1 > FB_H - 1) y1 = FB_H - 1;
	if (x1 < x0 || y1 < y0)
		return;

	co5300_blit(p, x0, y0, x1, y1);
}

static void co5300_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct co5300 *p = info->par;

	mutex_lock(&p->lock);
	cfb_fillrect(info, rect);
	co5300_flush(info, rect);
	mutex_unlock(&p->lock);
}

static void co5300_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	struct co5300 *p = info->par;
	struct fb_fillrect r = {
		.dx = area->dx, .dy = area->dy,
		.width = area->width, .height = area->height,
	};

	mutex_lock(&p->lock);
	cfb_copyarea(info, area);
	co5300_flush(info, &r);
	mutex_unlock(&p->lock);
}

static void co5300_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct co5300 *p = info->par;
	struct fb_fillrect r = {
		.dx = image->dx, .dy = image->dy,
		.width = image->width, .height = image->height,
	};

	mutex_lock(&p->lock);
	cfb_imageblit(info, image);
	co5300_flush(info, &r);
	mutex_unlock(&p->lock);
}

/*
 * Palette: black background, green text.
 *
 * fbcon asks for the standard 16 VGA colours and we hand back green in the two
 * entries ordinary text actually uses -- 7 (light grey, the default foreground)
 * and 15 (bright white, used for bold). Everything else is left as asked, so
 * `ls --color`, error messages and the rest still work.
 */
static int co5300_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			       u_int transp, struct fb_info *info)
{
	u32 v;

	if (regno > 255)
		return 1;

	if (regno == 7 || regno == 15) {
		/* bright green, slightly dimmer for the non-bold entry */
		red = 0x20;
		green = (regno == 15) ? 0xFF : 0xC0;
		blue = 0x20;
	}

	v = (red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3);
	((u32 *)info->pseudo_palette)[regno] = v;
	return 0;
}


/*
 * fb_write - let userspace draw on the panel.
 *
 * Without this there is no userspace path onto fb0 at all. The driver provides
 * only the accelerated ops (fillrect / copyarea / imageblit), which fbcon uses,
 * so text works -- but mmap() returns ENODEV (no fb_mmap) and write() hits
 * fb_WARN_ON_ONCE(!info->fbops->fb_write) and returns -1. A program that wants
 * to put its own pixels on the glass has no way to do it.
 *
 * fb_mmap alone would not be enough either: the panel has no memory of ours, it
 * is fed over SPI, so writing to the framebuffer only puts bytes in RAM. Each
 * write therefore copies into the framebuffer and then blits the rows it
 * touched, which is the only thing that reaches the glass.
 *
 * The whole frame cannot be pushed as one op from here (spi-mem wants a command
 * with its data), so this reuses co5300_blit(), which is already the path fbcon
 * takes.
 */
static ssize_t co5300_write(struct fb_info *info, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct co5300 *p = info->par;
	unsigned int total = info->fix.smem_len;
	unsigned int line = info->fix.line_length;
	unsigned int start, y0, y1;

	/*
	 * Everything here is 32-bit on purpose. The offsets are bounded by
	 * smem_len (256000), so 64-bit arithmetic buys nothing -- and on Xtensa
	 * a 64-bit divide pulls in __divdi3, which this kernel does not link.
	 * The first version used loff_t and failed to link at vmlinux.
	 */
	if (*ppos < 0)
		return -EINVAL;
	if ((loff_t)*ppos > (loff_t)total)
		return 0;

	start = (unsigned int)*ppos;

	if (count > total - start)
		count = total - start;
	if (!count)
		return 0;

	if (copy_from_user((u8 __force *)info->screen_base + start, buf, count))
		return -EFAULT;

	*ppos = start + count;

	y0 = start / line;
	y1 = (start + (unsigned int)count - 1) / line;
	if (y1 > FB_H - 1)
		y1 = FB_H - 1;

	mutex_lock(&p->lock);
	co5300_blit(p, 0, (int)y0, FB_W - 1, (int)y1);
	mutex_unlock(&p->lock);

	return count;
}

/*
 * CO5300_IOC_WRITE_RECT - put an RGB565 rectangle from userspace onto the glass
 * in panel coordinates, outside fb0.
 *
 * The overlap test against fb0 is not a politeness check. It is the mechanism
 * behind "edge updates never disturb the console": a userspace bug must not be
 * able to paint over the console, and this is four comparisons in the driver
 * rather than a convention that every future caller has to remember.
 *
 * Rects larger than the transmit buffer are split into row bands, each band a
 * complete window and a complete transfer, which is the same shape
 * co5300_paint_frame() uses and the only proven way to cover more than one
 * buffer (LCD-006).
 */
static int co5300_do_write_rect(struct fb_info *info, struct co5300_rect __user *urect)
{
	struct co5300 *p = info->par;
	struct co5300_rect r;
	size_t stride, band, rows, done;
	int rc = 0;

	if (copy_from_user(&r, urect, sizeof(r)))
		return -EFAULT;

	/*
	 * All argument validation lives in co5300_rect_check(), shared with the
	 * host-side test. It covers degenerate size, non-zero reserved/flags, a
	 * stride smaller than the width, out-of-panel bounds, and overlap with
	 * fb0 -- the last being the mechanism that stops userspace painting over
	 * the console.
	 */
	{
		struct co5300_limits L = {
			.panel_w = PANEL_W, .panel_h = PANEL_H,
			.fb_x = FB_X, .fb_y = FB_Y,
			.fb_w = FB_W, .fb_h = FB_H,
		};
		int v = co5300_rect_check(&r, &L);

		if (v)
			return v;
	}

	/*
	 * Rows per transfer: capped at CO5300_RECT_ROWS_PER_BAND, and further
	 * reduced only if a single row of this width would not fit the buffer.
	 */
	band = CO5300_RECT_ROWS_PER_BAND;
	if (band > p->tx_len / ((size_t)r.w * 2))
		band = p->tx_len / ((size_t)r.w * 2);
	if (band < 1)
		return -EINVAL;

	for (done = 0; done < r.h; done += rows) {
		size_t i;
		u16 *src = (u16 __user *)(uintptr_t)r.pixels;

		rows = r.h - done;
		if (rows > band)
			rows = band;

		/*
		 * Copy the band into the transmit buffer, byte-swapping each pixel
		 * on the way in. The panel takes the low byte of each RGB565 pixel
		 * first while the SPI peripheral shifts the high byte out first, so
		 * an unswapped stream rotates every colour -- invisible for black,
		 * wrong for everything else (LCD-003). Swapping here rather than
		 * asking callers to pre-swap is what stops that bug recurring.
		 *
		 * Rows are read individually against `stride`, so a caller can hand
		 * over a tile from a larger sprite sheet without repacking it.
		 */
		for (i = 0; i < rows; i++) {
			const u16 __user *line = src + (done + i) * stride;
			u16 *dst = p->tx + i * (size_t)r.w;
			size_t k;

			if (copy_from_user(dst, line,
					   (size_t)r.w * sizeof(u16))) {
				rc = -EFAULT;
				goto out;
			}
			for (k = 0; k < r.w; k++)
				dst[k] = swab16(dst[k]);
		}

		rc = co5300_send_band(p, (int)r.x, (int)r.y + (int)done,
				      (int)r.x + (int)r.w - 1,
				      (int)r.y + (int)done + (int)rows - 1);
		if (rc)
			goto out;
	}

out:
	return rc;
}

static int co5300_do_bench(struct co5300 *p, struct co5300_bench __user *ub)
{
	struct co5300_bench b;
	ktime_t t0;
	/*
	 * 32-bit accumulators on purpose. On Xtensa a 64-bit divide pulls in
	 * __divdi3, which this kernel does not link -- the same trap already
	 * documented in co5300_write(). Bounded: 64 iterations of at most a few
	 * hundred milliseconds each is well under 2^32 microseconds.
	 */
	u32 cs_total = 0, nocs_total = 0;
	size_t npix;
	int i, rc = 0;

	if (copy_from_user(&b, ub, sizeof(b)))
		return -EFAULT;

	if (b.w < 2 || b.h < 2 || b.iters < 1 || b.iters > 64)
		return -EINVAL;

	npix = (size_t)b.w * (size_t)b.h;
	if (!p->tx || npix * 2 > p->tx_len)
		return -EINVAL;

	/*
	 * Fill the transmit buffer with something non-trivial. A zero buffer is
	 * fine for timing, but a non-zero one rules out any chance the
	 * peripheral short-circuits on constant data.
	 */
	for (i = 0; i < (int)npix; i++)
		p->tx[i] = swab16((u16)(0x1234 + (i & 0xFF)));

	/* Warm up: first iteration pays for whatever is cold. */
	co5300_send_band_cs(p, 0, 0, b.w - 1, b.h - 1, true);

	for (i = 0; i < b.iters; i++) {
		t0 = ktime_get();
		co5300_send_band_cs(p, 0, 0, b.w - 1, b.h - 1, true);
		cs_total += (u32)ktime_to_us(ktime_sub(ktime_get(), t0));
	}

	for (i = 0; i < b.iters; i++) {
		t0 = ktime_get();
		co5300_send_band_cs(p, 0, 0, b.w - 1, b.h - 1, false);
		nocs_total += (u32)ktime_to_us(ktime_sub(ktime_get(), t0));
	}

	b.cs_us = cs_total / b.iters;
	b.nocs_us = nocs_total / b.iters;
	b.bytes = (u32)(npix * 2);

	pr_info("co5300 bench: %ux%u = %u bytes: cs=%u us, nocs=%u us, chunks=%zu\n",
		b.w, b.h, b.bytes, b.cs_us, b.nocs_us, npix * 2 / 64);

	if (copy_to_user(ub, &b, sizeof(b)))
		return -EFAULT;
	return rc;
}

static int co5300_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct co5300 *p = info->par;
	void __user *uarg = (void __user *)arg;
	int rc;

	switch (cmd) {
	case CO5300_IOC_GET_GEOM: {
		struct co5300_geom g;

		memset(&g, 0, sizeof(g));
		g.panel_w = PANEL_W;
		g.panel_h = PANEL_H;
		g.fb_x = FB_X;
		g.fb_y = FB_Y;
		g.fb_w = FB_W;
		g.fb_h = FB_H;
		g.offset_x = PANEL_OFFSET_X;
		g.version = CO5300_ABI_VERSION;

		if (copy_to_user(uarg, &g, sizeof(g)))
			return -EFAULT;
		return 0;
	}

	case CO5300_IOC_BENCH:
		mutex_lock(&p->lock);
		rc = co5300_do_bench(p, uarg);
		mutex_unlock(&p->lock);
		return rc;

	case CO5300_IOC_WRITE_RECT:
		/*
		 * Deliberately does NOT take console_lock. fbcon holds it for its
		 * own ops; taking it here would invert that ordering and add a
		 * deadlock class for no additional safety. The panel mutex is what
		 * serialises fbcon against userspace.
		 */
		mutex_lock(&p->lock);
		rc = co5300_do_write_rect(info, uarg);
		mutex_unlock(&p->lock);
		return rc;

	default:
		return -ENOTTY;
	}
}

static const struct fb_ops co5300_fb_ops = {
	.owner = THIS_MODULE,
	.fb_setcolreg = co5300_fb_setcolreg,
	.fb_fillrect = co5300_fillrect,
	.fb_copyarea = co5300_copyarea,
	.fb_imageblit = co5300_imageblit,
	.fb_write = co5300_write,
	.fb_ioctl = co5300_ioctl,
};

static int co5300_probe(struct spi_mem *mem)
{
	struct spi_device *spi = mem->spi;
	struct device *dev = &spi->dev;
	struct co5300 *p;
	struct fb_info *info;
	struct fb_var_screeninfo *var;
	int ret, edges;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	p->spi = spi;
	p->mem = mem;
	spi_set_drvdata(spi, p);

	/* Every panel operation is serialised against userspace by this. */
	mutex_init(&p->lock);

	if (co5300_route_pins(dev))
		return dev_err_probe(dev, -ENOMEM, "pin routing failed\n");

	/*
	 * Set the clock mode WITHOUT clobbering the data-width bits. spi->mode
	 * carries SPI_TX_QUAD / SPI_RX_QUAD from the DT's spi-tx-bus-width,
	 * and an earlier version did `spi->mode = SPI_MODE_0`, which silently
	 * cleared them -- after which every quad phase was rejected with
	 * -EOPNOTSUPP before reaching the controller.
	 */
	spi->mode &= ~(SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST);
	spi->bits_per_word = 8;
	spi->max_speed_hz = spi_hz;	/* default 20 MHz, see the module_param above */

	/* Publish the device so the parameter setter can retune it later. */
	co5300_spi_dev = spi;
	ret = spi_setup(spi);
	if (ret)
		return dev_err_probe(dev, ret, "spi_setup failed\n");

	/*
	 * Allocate the transfer scratch buffer BEFORE any command is sent.
	 * spi-mem rejects stack buffers, so init and window payloads go through
	 * this -- and an earlier version allocated it after init_panel(), which
	 * dereferenced a NULL and panicked the boot.
	 */
	p->small = kzalloc(16, GFP_KERNEL);
	if (!p->small)
		return -ENOMEM;

	{
		struct fwnode_handle *fwnode = of_fwnode_handle(dev->of_node);

		p->cs = fwnode_gpiod_get_index(fwnode, "lilygo,cs", 0,
					       GPIOD_OUT_LOW, "co5300-cs");
		if (IS_ERR(p->cs)) {
			pr_warn("co5300: no CS gpio (%ld), relying on hardware CS\n",
				PTR_ERR(p->cs));
			p->cs = NULL;
		}
	}
	p->rst = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	p->te = devm_gpiod_get_optional(dev, "te", GPIOD_IN);

	/* Reset: HIGH 200, LOW 300, HIGH 200 -- LilyGo's sequence exactly. */
	if (p->rst) {
		gpiod_set_value_cansleep(p->rst, 0);
		msleep(200);
		gpiod_set_value_cansleep(p->rst, 1);
		msleep(300);
		gpiod_set_value_cansleep(p->rst, 0);
		msleep(200);
	}

	pr_info("co5300: TE edges before init = %d\n", co5300_te_edges(p, 40));

	co5300_init_panel(p);

	edges = co5300_te_edges(p, 40);
	pr_info("co5300: TE edges after init = %d %s\n", edges,
		edges == 0 ? "(controller NOT scanning)" :
		edges < 0 ? "(no te gpio)" : "*** CONTROLLER IS SCANNING ***");

	p->fb_bytes = FB_W * FB_H * (BPP / 8);
	p->fb = vzalloc(p->fb_bytes);
	/*
	 * The transmit buffer is a whole frame, because the pixel stream has to
	 * go out as ONE spi-mem op: spi-mem refuses a data-only op with no
	 * command, so the frame cannot be continued across calls, and it cannot
	 * be split either because a CS edge ends the burst.
	 */
	p->tx_len = p->fb_bytes;
	p->tx = vzalloc(p->tx_len);
	if (!p->fb || !p->tx)
		return -ENOMEM;

	info = framebuffer_alloc(0, dev);
	if (!info)
		return -ENOMEM;
	p->info = info;
	info->par = p;
	info->pseudo_palette = p->pseudo_palette;

	info->screen_base = (char __iomem *)p->fb;
	info->screen_size = p->fb_bytes;
	info->fbops = &co5300_fb_ops;
	info->fix.smem_len = p->fb_bytes;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.line_length = FB_W * (BPP / 8);

	var = &info->var;
	var->xres = FB_W;
	var->yres = FB_H;
	var->xres_virtual = FB_W;
	var->yres_virtual = FB_H;
	var->bits_per_pixel = BPP;
	var->red.offset = 11; var->red.length = 5;
	var->green.offset = 5; var->green.length = 6;
	var->blue.offset = 0; var->blue.length = 5;

	ret = register_framebuffer(info);
	if (ret)
		return dev_err_probe(dev, ret, "register_framebuffer failed\n");

	/*
	 * Paint the border AFTER the framebuffer is registered.
	 *
	 * The border outside fb0 is not part of the framebuffer, so fbcon cannot
	 * reach it and userspace cannot either. Left alone it keeps whatever the
	 * panel powered up with -- a teal wash that reads as a broken display
	 * rather than as a border.
	 *
	 * Order matters and cost a round of debugging: the fill used to run before
	 * register_framebuffer() and the ring was lost, while the background
	 * survived. Registering first and drawing last means nothing can overwrite
	 * the ring afterwards -- fbcon only ever touches fb0.
	 */
	memset(p->fb, 0x00, p->fb_bytes);
	co5300_blit(p, 0, 0, FB_W - 1, FB_H - 1);

	{
		int l = PANEL_OFFSET_X + FB_X - 1;	/* 66, left of fb0  */
		int r = PANEL_OFFSET_X + FB_X + FB_W;	/* 387, right of fb0 */
		int t = FB_Y - 1;			/* 44, above fb0    */
		int b = FB_Y + FB_H;			/* 445, below fb0   */
		int rc = co5300_paint_frame(p, l, r, t, b);

		pr_info("co5300: frame painted rc=%d, ring cols %d/%d rows %d/%d\n",
			rc, l, r, t, b);
	}

	dev_info(dev, "%dx%d RGB565, fb0 registered\n", PANEL_W, PANEL_H);
	return 0;
}

static int co5300_remove(struct spi_mem *mem)
{
	struct co5300 *p = spi_get_drvdata(mem->spi);

	if (p->info) {
		unregister_framebuffer(p->info);
		framebuffer_release(p->info);
	}
	vfree(p->fb);
	vfree(p->tx);
	kfree(p->small);
	return 0;
}

static const struct of_device_id co5300_of_match[] = {
	{ .compatible = "lilygo,co5300" },
	{ }
};
MODULE_DEVICE_TABLE(of, co5300_of_match);

static struct spi_mem_driver co5300_driver = {
	.spidrv = {
		.probe = NULL,
		.driver = {
			.name = DRIVER_NAME,
			.of_match_table = co5300_of_match,
		},
	},
	.probe = co5300_probe,
	.remove = co5300_remove,
};
module_spi_mem_driver(co5300_driver);

MODULE_DESCRIPTION("CO5300 AMOLED panel on the T-Watch Ultra (SPI3)");
MODULE_LICENSE("GPL");
