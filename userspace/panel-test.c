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
	int clear = 0, ruler_mode = 0, solid_mode = 0, bands_mode = 0, edge_mode = 0, clean_mode = 0, stacked_mode = 0, sweep_mode = 0, tall_mode = 0, rows_mode = 0, raw_mode = 0, opt, bad = 0, i;
	int raw[4] = {0};

	while ((opt = getopt(argc, argv, "gcnR1BECSWTDr:")) != -1) {
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
		case 'B': bands_mode = 1; break;
		case 'E': edge_mode = 1; break;
		case 'C': clean_mode = 1; break;
		case 'S': stacked_mode = 1; break;
		case 'W': sweep_mode = 1; break;
		case 'T': tall_mode = 1; break;
		case 'D': rows_mode = 1; break;
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

	if (rows_mode) {
		/*
		 * Write the right strip two rows at a time instead of all 356 at once.
		 *
		 * A shear inside a solid fill is invisible, so the diagonal splits the
		 * user sees in a single-colour bar mean some rows did not receive the
		 * write at all. That happens when the controller's write pointer does
		 * not return to the window's left edge on each new row: each row lands
		 * further right, and after enough rows the content falls off the end.
		 *
		 * A 45x2 window leaves that pointer almost no room to accumulate. If
		 * the result is a clean bar, the per-row advance inside a multi-row
		 * window is the bug, and the driver can work around it by sending one
		 * window per row pair. If it is still torn, the pointer is not the
		 * problem and the fault is somewhere I have not looked.
		 *
		 * 178 windows of 45x2, painted bottom-up so any residue from the
		 * previous test is overwritten last.
		 */
		int y;

		printf("rows: right strip as 178 windows of 45x2\n");

		/* Left control: same construction, known-clean side. */
		for (y = 444; y >= 89; y -= 2) {
			int y0 = (y - 1 < 89) ? 89 : y - 1;

			bad |= paint(0, y0, 45, y - y0 + 1, WHITE, 0, 0) < 0;
		}

		for (y = 444; y >= 89; y -= 2) {
			int y0 = (y - 1 < 89) ? 89 : y - 1;

			bad |= paint(365, y0, 45, y - y0 + 1, GREEN, 0, 0) < 0;
		}

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: left white, right green, both in 2-row windows\n");
		return 0;
	}

	if (tall_mode) {
		/*
		 * Width is the only variable.
		 *
		 * -1 paints the right strip as ONE solid 45x356 rectangle and it came
		 * out as eight diagonal tears. A uniform fill cannot be torn by
		 * residue, so the controller is receiving rows at a drifting X --
		 * which is what a row overrunning its RAM line looks like. The error
		 * accumulates downward, matching "the eighth tear is the worst".
		 *
		 * The right strip is split into five vertical segments, each 71 rows
		 * tall and each painted with a different WIDTH from the same origin
		 * x=365. Nothing else varies.
		 *
		 *   seg 0  y  89..159   width 45   white
		 *   seg 1  y 160..230   width 35   red
		 *   seg 2  y 231..301   width 25   green
		 *   seg 3  y 302..372   width 15   blue
		 *   seg 4  y 373..443   width  5   yellow
		 *
		 * If the narrow segments are clean, the wrap point is between them
		 * and is knowable. If all five tear, width is not the variable and
		 * the fault is the segment's position rather than its size.
		 */
		static const uint16_t TC[5] = {
			WHITE, RED, GREEN, BLUE, RGB565(31, 63, 0),
		};
		static const int TW[5] = { 45, 35, 25, 15, 5 };
		int k;

		printf("tall: five stacked segments, widths 45/35/25/15/5\n");

		/* Left control: five solid segments, full 45 width, same rows. */
		for (k = 0; k < 5; k++) {
			int y = 89 + k * 71;
			int h = (k == 4) ? 71 : 71;

			bad |= paint(0, y, 45, h, TC[k], 0, 0) < 0;
		}

		for (k = 0; k < 5; k++) {
			int y = 89 + k * 71;

			/* black backing so the unpainted remainder is unambiguous */
			bad |= paint(365, y, 45, 71, BLACK, 0, 0) < 0;
			bad |= paint(365, y, TW[k], 71, TC[k], 0, 0) < 0;
			printf("  seg %d y %d..%d width %d\n", k, y, y + 70, TW[k]);
		}

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: right strip, widths 45 35 25 15 5 top to bottom\n");
		return 0;
	}

	if (sweep_mode) {
		/*
		 * Where does a 45-wide window start to tear?
		 *
		 * Every other test has compared only the two places a 45-wide window
		 * can go -- the far left and the far right -- so X and everything
		 * correlated with X were inseparable. The bottom strip is outside fb0
		 * and spans the full width, so the same 45x50 window can be placed at
		 * nine different X positions across it. Nothing else varies.
		 *
		 *   black background over the whole strip (sliced, so the background
		 *   itself is not the thing under test)
		 *   nine white 45x50 blocks at x = 0, 45, 90, ... 360
		 *
		 * Whichever blocks come out torn name the X range that breaks.
		 */
		int x;

		printf("sweep: 45x50 blocks at x = 0..360 step 45 in the bottom strip\n");

		/* Background in small slices to keep each transfer tiny. */
		for (x = 0; x < 410; x += 50) {
			int w = (x + 50 > 410) ? (410 - x) : 50;

			bad |= paint(x, 445, w, 57, BLACK, 0, 0) < 0;
		}

		for (x = 0; x + 45 <= 410; x += 45) {
			bad |= paint(x, 449, 45, 50, WHITE, 0, 0) < 0;
			printf("  block x %d..%d (ctrl %d..%d)\n",
			       x, x + 44, x + 22, x + 66);
		}

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: nine white blocks across the bottom strip\n");
		return 0;
	}

	if (stacked_mode) {
		/*
		 * Same picture as -C, but the strip background is written as many
		 * short rectangles instead of one tall one.
		 *
		 * -C paints each strip as a single 45x356 rect -- 32040 bytes in ONE
		 * spi transfer -- and the result was clean at the top and torn at the
		 * bottom with un-written rows showing the previous test's colours.
		 * That is what a truncated transfer looks like: the rows near the
		 * start land, and everything after the truncation keeps old data.
		 *
		 * If stacking short rects fixes it, the window height (or the total
		 * byte count of one op) is the limit and the driver must band large
		 * windows. If it does not, height is not the variable.
		 *
		 * Each slice is 45 wide and 44 tall: 3960 bytes, comfortably small.
		 */
		int y, k;

		printf("stacked: strips written as 9 slices of 45x44\n");
		bad |= paint(0, 0, 410, 45, WHITE, 0, 0) < 0;
		bad |= paint(0, 445, 410, 57, BLUE, 0, 0) < 0;

		for (y = 89, k = 0; y < 445; y += 44, k++) {
			int h = (y + 44 > 445) ? (445 - y) : 44;

			bad |= paint(0, y, 45, h, WHITE, 0, 0) < 0;
			bad |= paint(365, y, 45, h, WHITE, 0, 0) < 0;
		}

		/* One block each side, well inside the stacked area. */
		bad |= paint(0, 240, 45, 50, RED, 0, 0) < 0;
		bad |= paint(365, 240, 45, 50, RED, 0, 0) < 0;

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: same picture as -C, background in slices\n");
		return 0;
	}

	if (clean_mode) {
		/*
		 * Clean slate: one uniform fill over each whole strip, then a single
		 * contrasting block in the middle of each.
		 *
		 * Every earlier test drew many partial rectangles of different
		 * colours into the same region, so anything that landed inexactly
		 * left a neighbour's colour beside it -- and the result reads as a
		 * tear or a gap that may be nothing of the kind. On a uniform
		 * background there is exactly one thing to look at: is the block a
		 * rectangle, and are its edges parallel to the screen.
		 */
		printf("clean: uniform strip, one block in the middle\n");

		/* Whole strips, one colour each. */
		bad |= paint(0, 89, 45, 356, WHITE, 0, 0) < 0;
		bad |= paint(365, 89, 45, 356, WHITE, 0, 0) < 0;

		/* One block, same place on both sides, nothing else in the strip. */
		bad |= paint(0, 240, 45, 50, RED, 0, 0) < 0;
		bad |= paint(365, 240, 45, 50, RED, 0, 0) < 0;

		/* And a second, higher up, so a vertical drift shows as a difference
		 * in where the two blocks sit relative to each other. */
		bad |= paint(0, 120, 45, 50, BLUE, 0, 0) < 0;
		bad |= paint(365, 120, 45, 50, BLUE, 0, 0) < 0;

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: white strips, blue block at y120, red block at y240\n");
		return 0;
	}

	if (edge_mode) {
		/*
		 * Find where the right edge breaks.
		 *
		 * Every band starts at panel x 365 (controller column 387) and ends
		 * at a different x1, so the only variable is how far toward the
		 * right the window reaches. The left strip carries the same colours
		 * at the full 45 width as a control, because it is known clean.
		 *
		 * If the tear disappears past some x1, that is the controller's RAM
		 * boundary and PANEL_OFFSET_X = 22 was the wrong inference. If every
		 * band is torn regardless, the right edge is not the variable.
		 *
		 *   band  colour   panel x1   controller x1   width
		 *    0    red        409          431           45
		 *    1    yellow     405          427           41
		 *    2    green      401          423           37
		 *    3    cyan       397          419           33
		 *    4    blue       393          415           29
		 *    5    magenta    389          411           25
		 *    6    white      385          407           21
		 *    7    red        381          403           17
		 */
		static const uint16_t ECOL[8] = {
			RED, RGB565(31, 63, 0), GREEN, RGB565(0, 63, 31),
			BLUE, RGB565(31, 0, 31), WHITE, RED,
		};
		int k;

		printf("edge: right strip x0=365, x1 varies; left is the control\n");

		bad |= paint(0, 0, 410, 45, WHITE, 0, 0) < 0;
		bad |= paint(0, 445, 410, 57, BLUE, 0, 0) < 0;

		for (k = 0; k < 8; k++) {
			int y = 89 + k * 44;
			int x1 = 409 - k * 4;

			/* control: full width on the left */
			bad |= paint(0, y, 45, 44, ECOL[k], 0, 0) < 0;
			/* variable: right edge pulled in */
			bad |= paint(365, y, x1 - 365 + 1, 44, ECOL[k], 0, 0) < 0;
			printf("  band %d y %d..%d  right x1 %d (ctrl %d)\n",
			       k, y, y + 43, x1, x1 + 22);
		}

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: top-down on the right, x1 = 409,405,401,397,393,389,385,381\n");
		return 0;
	}

	if (bands_mode) {
		/*
		 * The decisive test: IDENTICAL structured content on both side
		 * strips. Same width, same height, same band heights, same colours.
		 * The only difference is X position -- controller columns 22..66 on
		 * the left against 387..431 on the right.
		 *
		 * If the left is a clean stack of rectangles and the right is
		 * sheared, the fault is positional and nothing else. If both shear,
		 * it is the 45-wide window shape and position is irrelevant.
		 *
		 * A solid fill could never show this: a shear inside a solid colour
		 * is invisible.
		 */
		static const uint16_t BAND[9] = {
			RED, RGB565(31, 63, 0), GREEN, RGB565(0, 63, 31),
			BLUE, RGB565(31, 0, 31), WHITE, RED, RGB565(31, 63, 0),
		};
		int k;

		printf("bands: identical content, left x0..44 vs right x365..409\n");

		/* Full-width reference bars first, so any vertical offset in the
		 * strips can be measured against them. */
		bad |= paint(0, 0, 410, 45, WHITE, 0, 0) < 0;
		bad |= paint(0, 445, 410, 57, BLUE, 0, 0) < 0;

		for (k = 0; k < 9; k++) {
			int y = 89 + k * 40;

			/* Each band is one rect per side: 45 wide, 40 tall. */
			bad |= paint(0, y, 45, 40, BAND[k], 0, 0) < 0;
			bad |= paint(365, y, 45, 40, BAND[k], 0, 0) < 0;
		}

		close(ffd);
		if (bad) {
			printf("FAIL: a region was rejected\n");
			return 1;
		}
		printf("painted: compare left and right -- they should be identical\n");
		return 0;
	}

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
