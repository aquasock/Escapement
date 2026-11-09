/*
 * panel-test.c - prove that CO5300_IOC_WRITE_RECT lands where it says.
 *
 * This is the gate for the panel API. The panel is write-only and never
 * acknowledges, so a rectangle that lands in the wrong place, or is clipped, or
 * is dropped entirely, all look identical from the return code. The only
 * evidence is the glass. What this tool does is make a wrong result *visible*:
 *
 *   - a different fill colour per region, so "nothing drew" and "the wrong
 *     region drew" are distinguishable at a glance;
 *   - four corner markers per tile, in a second colour, so a shifted window,
 *     a clipped window or a mirrored window all show up as a missing corner
 *     rather than as a plausible square;
 *   - a centre dot, so a tile that drew only its border is caught.
 *
 * A flat fill would prove nothing. Three drawing paths in this project have
 * already reported success while drawing nothing (LCD-002, LCD-003, LCD-004).
 *
 * Layout painted (from docs/panel-reservations.md):
 *
 *   top bar      y   0.. 44, full width, 9 cells
 *   left slots   x   0.. 44, 7 tiles at y 97..411 step 45
 *   right slots  x 365..409, same rows
 *   bottom bar   y 445..501, full width, 3 thirds
 *
 * Usage:
 *   panel-test -g            print geometry from the kernel and exit
 *   panel-test               paint the pattern
 *   panel-test -c            clear all reserved regions to black
 *   panel-test -n            dry run: report what would be drawn
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEV_FB		"/dev/fb0"

#define CO5300_IOC_MAGIC	'C'
#define CO5300_ABI_VERSION	1

struct co5300_rect {
	uint16_t x, y, w, h, stride, reserved;
	uint32_t flags;
	uint64_t pixels;
};

struct co5300_geom {
	uint16_t panel_w, panel_h;
	uint16_t fb_x, fb_y;
	uint16_t fb_w, fb_h;
	uint16_t offset_x;
	uint16_t version;
	uint32_t reserved;
};

#define CO5300_IOC_WRITE_RECT \
	_IOW(CO5300_IOC_MAGIC, 1, struct co5300_rect)
#define CO5300_IOC_GET_GEOM \
	_IOR(CO5300_IOC_MAGIC, 2, struct co5300_geom)

#define RGB565(r, g, b) \
	((uint16_t)((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F)))

/*
 * Named colours, because getting the green width wrong is invisible until it
 * is on the glass. Red and blue are 5-bit (0..31) but green is 6-bit (0..63),
 * so WHITE is full red, HALF green and full blue -- which is pink,
 * not white. Every "white" in the first version of this tool was pink for
 * exactly that reason, and the tests it produced were read as a driver fault.
 */
#define WHITE		RGB565(31, 63, 31)
#define BLACK		RGB565(0, 0, 0)
#define RED		RGB565(31, 0, 0)
#define GREEN		RGB565(0, 63, 0)
#define BLUE		RGB565(0, 0, 31)

static int ffd = -1;
static struct co5300_geom geom;
static int dry_run;

static void die(const char *what)
{
	perror(what);
	exit(1);
}

/*
 * Paint one rectangle. The buffer is built here rather than being a static
 * array: on NOMMU a large .bss blocks exec outright (KRN-001), and the largest
 * region here is the full-width bottom bar.
 */
static int paint(int x, int y, int w, int h, uint16_t fill,
		 uint16_t corner, uint16_t dot)
{
	struct co5300_rect r;
	uint16_t *px;
	int i, j;

	if (dry_run) {
		printf("  would paint %dx%d at (%d,%d) fill %04X corner %04X dot %04X\n",
		       w, h, x, y, fill, corner, dot);
		return 0;
	}

	px = malloc((size_t)w * h * sizeof(*px));
	if (!px) {
		fprintf(stderr, "panel-test: out of memory for %dx%d\n", w, h);
		return -1;
	}

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
			px[j * w + i] = fill;

	/*
	 * Corner markers: a 3x3 block inset 2 px from each corner. Inset rather
	 * than flush so that a window shifted by one or two pixels still shows
	 * all four corners -- a flush marker would leave the picture ambiguous
	 * between "shifted" and "clipped".
	 */
	if (w >= 10 && h >= 10) {
		static const int cx[4] = { 2, -1, 2, -1 };
		static const int cy[4] = { 2, 2, -1, -1 };

		for (int c = 0; c < 4; c++) {
			int bx = (cx[c] >= 0) ? cx[c] : w + cx[c] - 2;
			int by = (cy[c] >= 0) ? cy[c] : h + cy[c] - 2;

			for (j = 0; j < 3; j++)
				for (i = 0; i < 3; i++)
					px[(by + j) * w + (bx + i)] = corner;
		}
	}

	/* Centre dot, so a tile that drew only its edge is caught. */
	if (dot && w >= 6 && h >= 6) {
		int mx = w / 2, my = h / 2;

		for (j = -1; j <= 1; j++)
			for (i = -1; i <= 1; i++)
				px[(my + j) * w + (mx + i)] = dot;
	}

	memset(&r, 0, sizeof(r));
	r.x = (uint16_t)x;
	r.y = (uint16_t)y;
	r.w = (uint16_t)w;
	r.h = (uint16_t)h;
	r.stride = 0;
	r.pixels = (uint64_t)(uintptr_t)px;

	if (ioctl(ffd, CO5300_IOC_WRITE_RECT, &r) < 0) {
		int e = errno;

		fprintf(stderr, "panel-test: %dx%d at (%d,%d) -> %s\n",
			w, h, x, y, strerror(e));
		free(px);
		return -1;
	}

	free(px);
	return 0;
}

/* Distinct palettes so adjacent regions are never the same colour. */
/*
 * Seven fills chosen to be distinguishable at 45 px through a camera, with no
 * two adjacent entries close in hue. Green and cyan use the full 6-bit field;
 * using 31 there yields half-brightness tones that read as muddy.
 */
static const uint16_t SLOT_FILL[7] = {
	RED,				/* red     */
	RGB565(31, 63, 0),		/* yellow  */
	GREEN,				/* green   */
	RGB565(0, 63, 31),		/* cyan    */
	BLUE,				/* blue    */
	RGB565(31, 0, 31),		/* magenta */
	WHITE,				/* white   */
};
static const uint16_t SLOT_CORNER = BLACK;	/* contrast on all seven */
static const uint16_t SLOT_DOT = WHITE;


/*
 * Ruler mode: draw each reserved region as ONE transfer, with the whole picture
 * built inside that one buffer.
 *
 * This exists to separate two explanations for the diagonal tear seen on the
 * right strip. If the picture is still sheared when it is one transfer, the
 * addressing is wrong. If the shear disappears, it was the transfer shape or
 * the scan timing, and the fix is in how writes are issued rather than in the
 * coordinates.
 *
 * The picture also makes the corner markers unmissable: 8x8 blocks inset 4 px,
 * in white on every tile, plus a full-width white rule at the first and last
 * row of the region so a vertically shifted tile breaks the rule visibly.
 *
 *   row 0 and row h-1        white rule, full width
 *   vertical stripes         every (w / 10) px, alternating bright colours
 *   tile corner markers      8x8 white, inset 4 px from each tile corner
 */
static int ruler(int x, int y, int w, int h, int ntiles)
{
	struct co5300_rect r;
	uint16_t *px;
	int i, j, t;

	fprintf(stderr, "ruler: enter %dx%d at (%d,%d)\n", w, h, x, y);

	px = malloc((size_t)w * h * sizeof(*px));
	if (!px) {
		fprintf(stderr, "panel-test: out of memory for %dx%d\n", w, h);
		return -1;
	}

	fprintf(stderr, "ruler: malloc ok\n");

	/* Background: 10 vertical stripes of alternating bright colours. */
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			int band = (i * 10) / w;

			px[j * w + i] = SLOT_FILL[band % 7];
		}
	}

	/* White rules on the first and last rows: a shifted tile breaks these. */
	for (i = 0; i < w; i++) {
		px[i] = WHITE;
		px[(h - 1) * w + i] = WHITE;
	}

	/* 8x8 white markers at the four corners of each tile. */
	if (ntiles > 0) {
		int th = h / ntiles;

		for (t = 0; t < ntiles; t++) {
			int ty0 = t * th;
			int pts[4][2] = {
				{ 4, ty0 + 4 }, { w - 12, ty0 + 4 },
				{ 4, ty0 + th - 12 }, { w - 12, ty0 + th - 12 },
			};
			int c;

			for (c = 0; c < 4; c++) {
				for (j = 0; j < 8; j++) {
					int yy = pts[c][1] + j;

					if (yy < 0 || yy >= h)
						continue;
					for (i = 0; i < 8; i++) {
						int xx = pts[c][0] + i;

						if (xx < 0 || xx >= w)
							continue;
						px[yy * w + xx] = WHITE;
					}
				}
			}
		}
	}

	fprintf(stderr, "ruler: buffer built\n");

	memset(&r, 0, sizeof(r));
	r.x = (uint16_t)x;
	r.y = (uint16_t)y;
	r.w = (uint16_t)w;
	r.h = (uint16_t)h;
	r.pixels = (uint64_t)(uintptr_t)px;

	fprintf(stderr, "ruler: calling ioctl\n");
	if (ioctl(ffd, CO5300_IOC_WRITE_RECT, &r) < 0) {
		fprintf(stderr, "panel-test: ruler %dx%d at (%d,%d) -> %s\n",
			w, h, x, y, strerror(errno));
		free(px);
		return -1;
	}
	free(px);
	return 0;
}

int main(int argc, char **argv)
{
	int clear = 0, ruler_mode = 0, solid_mode = 0, raw_mode = 0, opt, bad = 0, i;
	int raw[4] = {0};

	while ((opt = getopt(argc, argv, "gcnR1r:")) != -1) {
		switch (opt) {
		case 'g': {
			struct co5300_geom g;

			ffd = open(DEV_FB, O_RDWR);
			if (ffd < 0)
				die("open " DEV_FB);
			memset(&g, 0, sizeof(g));
			if (ioctl(ffd, CO5300_IOC_GET_GEOM, &g) < 0)
				die("CO5300_IOC_GET_GEOM");
			printf("panel   %ux%u\n", g.panel_w, g.panel_h);
			printf("fb0     %ux%u at (%u,%u)\n",
			       g.fb_w, g.fb_h, g.fb_x, g.fb_y);
			printf("offset  %u\n", g.offset_x);
			printf("abi     %u\n", g.version);
			close(ffd);
			return 0;
		}
		case 'c': clear = 1; break;
		case 'n': dry_run = 1; break;
		case 'R': ruler_mode = 1; break;
		case '1': solid_mode = 1; break;
		case 'r': {
			/* -r x,y,w,h : paint one black rect. For bisecting. */
			if (sscanf(optarg, "%d,%d,%d,%d", &raw[0], &raw[1],
				   &raw[2], &raw[3]) != 4) {
				fprintf(stderr, "usage: -r x,y,w,h\n");
				return 2;
			}
			raw_mode = 1;
			break;
		}
		default:
			fprintf(stderr,
				"usage: panel-test [-g] [-c] [-n]\n");
			return 2;
		}
	}

	ffd = open(DEV_FB, O_RDWR);
	if (ffd < 0)
		die("open " DEV_FB);

	memset(&geom, 0, sizeof(geom));
	if (ioctl(ffd, CO5300_IOC_GET_GEOM, &geom) < 0) {
		fprintf(stderr,
			"panel-test: this kernel has no CO5300_IOC_GET_GEOM "
			"(errno %d). The panel API is not in the running kernel.\n",
			errno);
		return 1;
	}

	printf("panel %ux%u, fb0 %ux%u at (%u,%u)\n",
	       geom.panel_w, geom.panel_h, geom.fb_w, geom.fb_h,
	       geom.fb_x, geom.fb_y);

	if (raw_mode) {
		int rc;

		printf("raw %dx%d at (%d,%d)\n", raw[2], raw[3], raw[0], raw[1]);
		fflush(stdout);
		rc = paint(raw[0], raw[1], raw[2], raw[3], BLACK, 0, 0);
		close(ffd);
		return rc < 0 ? 1 : 0;
	}

	if (solid_mode) {
		/*
		 * One pure colour per region and nothing else. No stripes, no
		 * markers, no rules: this is the test that cannot be misread.
		 * If the left strip is not plainly RED, the pixel path is wrong and
		 * every conclusion drawn from a stripey picture is worthless.
		 */
		printf("solid: top WHITE, left RED, right GREEN, bottom BLUE\n");

		/*
		 * Full strip extents, not just the slot grid. The slots are 45x45 at
		 * y 97..411, but the strip itself runs from row 89 (where both sides
		 * are full width) down to row 444, where fb0 ends. Painting only the
		 * slots leaves unpainted residue in those margins, which is what the
		 * first version of this test did and it read as a driver fault.
		 */
		bad |= paint(0, 0, 410, 45, WHITE, 0, 0) < 0;
		bad |= paint(0, 89, 45, 356, RED, 0, 0) < 0;      /* y 89..444 */
		bad |= paint(365, 89, 45, 356, GREEN, 0, 0) < 0;
		bad |= paint(0, 445, 410, 57, BLUE, 0, 0) < 0;
		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: expect top=white left=red right=green bottom=blue\n");
		return 0;
	}

	if (ruler_mode) {
		/*
		 * One transfer per region. Top and bottom are full width, which is
		 * the transfer shape already proven; the sides are one tall band
		 * each, which is the shape under test.
		 */
		printf("ruler: top strip as one 410x45 write\n");
		bad |= ruler(0, 0, 410, 45, 0) < 0;

		printf("ruler: left strip as one 45x315 write\n");
		bad |= ruler(0, 97, 45, 315, 7) < 0;

		printf("ruler: right strip as one 45x315 write\n");
		bad |= ruler(365, 97, 45, 315, 7) < 0;

		printf("ruler: bottom strip as one 410x57 write\n");
		bad |= ruler(0, 445, 410, 57, 3) < 0;

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted; look at the glass\n");
		return 0;
	}

	if (clear) {
		printf("clearing reserved regions\n");
		bad |= paint(0, 0, 410, 45, 0, 0, 0) < 0;
		bad |= paint(0, 97, 45, 315, 0, 0, 0) < 0;
		bad |= paint(365, 97, 45, 315, 0, 0, 0) < 0;
		bad |= paint(0, 445, 410, 57, 0, 0, 0) < 0;
		close(ffd);
		return bad ? 1 : 0;
	}

	/* --- top bar: 9 cells of 45, full width ------------------------------ */
	printf("top bar\n");
	for (i = 0; i < 9; i++) {
		uint16_t fill = SLOT_FILL[i % 7];

		if (paint(i * 45, 0, 45, 45, fill,
			  SLOT_CORNER, SLOT_DOT) < 0)
			bad = 1;
	}
	/* The 5 px orphan at x 405..409, painted white so its fate is visible. */
	if (paint(405, 0, 5, 45, WHITE, 0, 0) < 0)
		bad = 1;

	/* --- side slots: 7 per side ----------------------------------------- */
	printf("side slots\n");
	for (i = 0; i < 7; i++) {
		int y = 97 + i * 45;

		if (paint(0, y, 45, 45, SLOT_FILL[i],
			  SLOT_CORNER, SLOT_DOT) < 0)
			bad = 1;
		if (paint(365, y, 45, 45, SLOT_FILL[i],
			  SLOT_CORNER, SLOT_DOT) < 0)
			bad = 1;
	}

	/* --- bottom bar: three thirds, distinct and obvious ------------------ */
	printf("bottom bar\n");
	if (paint(0, 445, 135, 57, RGB565(31, 0, 0),
		  SLOT_CORNER, SLOT_DOT) < 0)
		bad = 1;
	if (paint(137, 445, 134, 57, RGB565(0, 31, 0),
		  SLOT_CORNER, SLOT_DOT) < 0)
		bad = 1;
	if (paint(273, 445, 137, 57, RGB565(0, 0, 31),
		  SLOT_CORNER, SLOT_DOT) < 0)
		bad = 1;

	close(ffd);

	if (bad) {
		printf("FAIL: something was rejected; see the messages above\n");
		return 1;
	}

	printf("painted; now look at the glass. A return code proves nothing.\n");
	return 0;
}
