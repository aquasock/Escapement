/*
 * co5300_rect.h - the panel API's argument validation, in one testable place.
 *
 * The structs and the bounds check live here rather than inside co5300.c so
 * that the arithmetic can be compiled and exercised on a build host, where the
 * panel and the watch are not. Every rejection here is a rule from
 * docs/panel-api.md, and a boundary that is off by one would otherwise only be
 * discovered by a flash and a look at the glass.
 *
 * Nothing in this file depends on the kernel except the __u16/__u64 type names,
 * which the host test satisfies with stdint typedefs.
 */
#ifndef _CO5300_RECT_H
#define _CO5300_RECT_H

#ifndef CO5300_HOSTTEST
#include <linux/types.h>
#else
#include <stdint.h>
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
#endif

#define CO5300_IOC_MAGIC	'C'	/* unused elsewhere in this tree */
#define CO5300_ABI_VERSION	1

struct co5300_rect {
	__u16 x, y;	/* top-left, PANEL coordinates */
	__u16 w, h;	/* size in pixels; w and h must both be >= 2 */
	__u16 stride;	/* source row pitch in pixels; 0 means w */
	__u16 reserved;	/* must be 0 */
	__u32 flags;	/* must be 0 */
	__u64 pixels;	/* user pointer, __u16 RGB565, host byte order */
};

struct co5300_geom {
	__u16 panel_w, panel_h;
	__u16 fb_x, fb_y;
	__u16 fb_w, fb_h;
	__u16 offset_x;	/* the controller's column offset (22) */
	__u16 version;
	__u32 reserved;
};

/* Geometry, passed in so the host test can vary it. */
struct co5300_limits {
	int panel_w, panel_h;
	int fb_x, fb_y, fb_w, fb_h;
};

/*
 * Returns 0 if the rectangle is writable, or a negative errno.
 *
 * The rules, in the order docs/panel-api.md lists them:
 *
 *   - w and h must both be >= 2. A window one pixel wide or tall is not
 *     honoured by the controller and reports success (LCD-002), so refusing it
 *     turns a silent no-op into an error the caller can see.
 *   - reserved and flags must be zero, so a future ABI change that starts using
 *     them is caught by old kernels rather than ignored.
 *   - stride must be >= w. A smaller stride would read back over the row just
 *     written.
 *   - the rectangle must lie inside the panel.
 *   - the rectangle must not intersect fb0. This is the mechanism behind
 *     "edges never disturb the console": a userspace bug must not be able to
 *     paint over the console.
 */
static inline int co5300_rect_check(const struct co5300_rect *r,
				    const struct co5300_limits *L)
{
	int x0, y0, x1, y1;
	int stride;

	if (r->w < 2 || r->h < 2)
		return -22;	/* -EINVAL */
	if (r->reserved || r->flags)
		return -22;

	stride = r->stride ? r->stride : r->w;
	if (stride < (int)r->w)
		return -22;

	x0 = r->x;
	y0 = r->y;
	x1 = (int)r->x + (int)r->w - 1;
	y1 = (int)r->y + (int)r->h - 1;

	if (x0 < 0 || y0 < 0 || x1 >= L->panel_w || y1 >= L->panel_h)
		return -22;

	/*
	 * Two rectangles do not overlap if either is wholly to one side of the
	 * other on any axis. Rejecting the complement of that is exactly the
	 * fb0 guard, and it is inclusive on both ends because these are pixel
	 * coordinates, not half-open ranges.
	 */
	{
		int fx0 = L->fb_x, fy0 = L->fb_y;
		int fx1 = L->fb_x + L->fb_w - 1;
		int fy1 = L->fb_y + L->fb_h - 1;

		int separate = (x1 < fx0) || (x0 > fx1) ||
			       (y1 < fy0) || (y0 > fy1);

		if (!separate)
			return -22;
	}

	return 0;
}

#endif /* _CO5300_RECT_H */
