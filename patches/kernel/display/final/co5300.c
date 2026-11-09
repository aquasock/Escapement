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
#include <linux/property.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>
#include <linux/io.h>
#include <linux/vmalloc.h>

#define DRIVER_NAME "co5300"

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
#define FB_Y		((PANEL_H - PANEL_SAFE_H) / 2)	/* 46 */
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
	cfb_fillrect(info, rect);
	co5300_flush(info, rect);
}

static void co5300_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	struct fb_fillrect r = {
		.dx = area->dx, .dy = area->dy,
		.width = area->width, .height = area->height,
	};

	cfb_copyarea(info, area);
	co5300_flush(info, &r);
}

static void co5300_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct fb_fillrect r = {
		.dx = image->dx, .dy = image->dy,
		.width = image->width, .height = image->height,
	};

	cfb_imageblit(info, image);
	co5300_flush(info, &r);
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

static const struct fb_ops co5300_fb_ops = {
	.owner = THIS_MODULE,
	.fb_setcolreg = co5300_fb_setcolreg,
	.fb_fillrect = co5300_fillrect,
	.fb_copyarea = co5300_copyarea,
	.fb_imageblit = co5300_imageblit,
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
	spi->max_speed_hz = 20000000;	/* proven on this panel */
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
	 * Clear the whole glass to black, then let fbcon draw into fb0.
	 *
	 * The border outside fb0 is not part of the framebuffer, so it has to be
	 * cleared here or a previous image stays visible in the strips that are
	 * being kept for notifications. It is cleared as a full-panel window
	 * rather than through the fb, which knows nothing about it.
	 */
	{
		u16 *blank = vzalloc((size_t)PANEL_W * 2);
		u8 *win = p->small;
		int y;

		if (blank) {
			for (y = 0; y < PANEL_H; y++) {
				win[0] = (PANEL_OFFSET_X) >> 8;
				win[1] = (PANEL_OFFSET_X) & 0xFF;
				win[2] = (PANEL_OFFSET_X + PANEL_W - 1) >> 8;
				win[3] = (PANEL_OFFSET_X + PANEL_W - 1) & 0xFF;
				co5300_cmd(p, 0x2A, win, 4);

				win[0] = y >> 8; win[1] = y & 0xFF;
				win[2] = y >> 8; win[3] = y & 0xFF;
				co5300_cmd(p, 0x2B, win, 4);

				co5300_op(p, 0x02, 0x002C00, NULL, 0, 1);

				if (p->cs)
					gpiod_set_value_cansleep(p->cs, 1);
				co5300_xfer(p, 0x32, 1, 0x002C00, 1, blank,
					    (size_t)PANEL_W * 2, 4);
				if (p->cs)
					gpiod_set_value_cansleep(p->cs, 0);
			}
			vfree(blank);
		}
	}

	memset(p->fb, 0x00, p->fb_bytes);
	co5300_blit(p, 0, 0, FB_W - 1, FB_H - 1);

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
