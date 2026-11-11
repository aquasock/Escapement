/*
 * fbbench.c - what does a framebuffer write actually cost, in process.
 *
 * Written because every cheap way to measure this on this board is wrong:
 *
 *   - `dd if=/dev/zero of=/dev/fb0` measures process startup, not the write.
 *     Measured control: a `dd` that touches nothing costs 430-440 ms on this
 *     board, so a 25 KB write and a 256 KB write both "take" ~440 ms and the
 *     numbers say nothing about the panel.
 *   - Deriving the cost from tamafb's stall log gives 9.3 ms per band from one
 *     frame and 25 ms per band from another, a 2.7x spread that no amount of
 *     arithmetic resolves.
 *
 * So: one process, N writes, timed with a monotonic clock, reported in ms.
 *
 * The question it answers is whether refreshing the whole framebuffer every
 * frame is affordable, which decides whether tamafb's dirty-band masking can
 * be retired.
 *
 * Usage: fbbench [-n iters] [-b bands]
 *   -n iters   full-frame writes to time (default 20)
 *   -b bands   band height in rows (default 40, matching tamafb's BAND_H)
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define FB_W 320
#define FB_H 400

static uint16_t frame[FB_W * FB_H];

static double now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	int iters = 20, band_rows = 40;
	int fd, i, b, opt;
	double t0, t1, total;

	while ((opt = getopt(argc, argv, "n:b:")) != -1) {
		switch (opt) {
		case 'n': iters = atoi(optarg); break;
		case 'b': band_rows = atoi(optarg); break;
		default: return 2;
		}
	}

	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0) {
		perror("/dev/fb0");
		return 1;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) || ioctl(fd, FBIOGET_VSCREENINFO, &var)) {
		perror("ioctl");
		return 1;
	}

	if (var.bits_per_pixel != 16 || var.xres < FB_W || var.yres < FB_H) {
		fprintf(stderr, "fb %ux%u %ubpp is not usable\n",
			var.xres, var.yres, var.bits_per_pixel);
		return 1;
	}

	printf("fb %ux%u %ubpp line %u\n", var.xres, var.yres,
	       var.bits_per_pixel, fix.line_length);

	/* Something visibly different from a cleared screen, so a write is real. */
	for (i = 0; i < FB_W * FB_H; i++)
		frame[i] = (uint16_t)((i * 7) & 0xFFFF);

	/* Full frame, one pwrite, exactly what a whole-screen refresh costs. */
	total = 0;
	for (i = 0; i < iters; i++) {
		t0 = now_ms();
		if (pwrite(fd, frame, sizeof(frame), 0) != (ssize_t)sizeof(frame)) {
			perror("pwrite full");
			return 1;
		}
		t1 = now_ms();
		total += t1 - t0;
	}
	printf("full frame  %zu B   %6.1f ms  (%d writes)\n",
	       sizeof(frame), total / iters, iters);

	/* Per band, to see whether cost is linear in bytes or per call. */
	{
		int bands = (FB_H + band_rows - 1) / band_rows;
		int rows, by0;
		size_t bytes;
		double per = 0;

		total = 0;
		for (i = 0; i < iters; i++) {
			for (b = 0; b < bands; b++) {
				by0 = b * band_rows;
				rows = FB_H - by0;
				if (rows > band_rows)
					rows = band_rows;
				bytes = (size_t)FB_W * rows * 2;

				t0 = now_ms();
				if (pwrite(fd, &frame[by0 * FB_W], bytes,
					   (off_t)by0 * fix.line_length) != (ssize_t)bytes) {
					perror("pwrite band");
					return 1;
				}
				t1 = now_ms();
				total += t1 - t0;
			}
		}
		per = total / ((double)iters * bands);
		printf("banded      %d x %d rows  %5.2f ms/band  %.1f ms/frame\n",
		       bands, band_rows, per, per * bands);
	}

	close(fd);
	return 0;
}
