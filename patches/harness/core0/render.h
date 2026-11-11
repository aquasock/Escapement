/*
 * render.h - draw the LCD segment map into an RGB565 buffer.
 *
 * Two modes, and the difference matters:
 *
 *   lcd_draw_scaled()   every LCD dot is a DOT x DOT block with no gap. This is
 *                       the true hardware look, and at DOT=4 the pet fills a
 *                       quarter of the screen. It is what "4x, no spacing
 *                       pixels" means.
 *
 *   lcd_draw_dots()     every LCD dot is a DOT x DOT block separated by GAP
 *                       pixels, so the dot matrix is visible as a grid. This is
 *                       the harness's original look: readable across a room
 *                       because the eye is given structure to latch onto, at the
 *                       cost of a larger image for the same information.
 *
 * Both take the dot grid size from the segment map (SEG_W x SEG_H dots), so
 * there is one artwork and one map. Scaling in the renderer rather than
 * rebuilding the map is not just cheaper -- a 4x map was 126 KB of const data
 * and overflowed the 768 KB firmware partition, while the map itself is 32 KB
 * whatever the on-screen size.
 *
 * Do not go back to bounding boxes. The artwork uses H/V path commands and
 * cubic curves, so a naive box over the raw coordinates produces garbage --
 * that mistake showed up as a grid of lines across the display.
 */

#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "segmap.h"

#define LCD_INK   0x0000
#define LCD_OFF   0xC618

typedef struct {
    uint16_t *px;
    int w, h;
} lcd_fb;

static inline void lcd_put(const lcd_fb *fb, int x, int y, uint16_t c)
{
    if (x >= 0 && x < fb->w && y >= 0 && y < fb->h)
        fb->px[y * fb->w + x] = c;
}

static inline void lcd_rect(const lcd_fb *fb, int x0, int y0, int x1, int y1, uint16_t c)
{
    int y;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->w) x1 = fb->w;
    if (y1 > fb->h) y1 = fb->h;
    if (x0 >= x1 || y0 >= y1)
        return;

    for (y = y0; y < y1; y++) {
        uint16_t *row = fb->px + (size_t)y * fb->w;

        for (int x = x0; x < x1; x++)
            row[x] = c;
    }
}

/* Pixels across the dot grid: DOT pixels per dot, GAP between neighbours. */
#define LCD_DOTS_W(dot, gap) (SEG_W * (dot) + (SEG_W - 1) * (gap))
#define LCD_DOTS_H(dot, gap) (SEG_H * (dot) + (SEG_H - 1) * (gap))

/*
 * Draw the LCD with its top-left corner at (ox, oy), each LCD dot as a DOT x DOT
 * block with GAP pixels between dots.
 *
 * The gap is applied between dots and not around the outside, so an image of W
 * dots is W*dot + (W-1)*gap wide -- the grid ends with a dot, not with a margin.
 * That is what makes the block read as a dot matrix rather than as a waffle.
 */
static inline void lcd_draw_dots(const lcd_fb *fb, const uint8_t *dpram,
                                 int ox, int oy, int dot, int gap)
{
    const int pitch = dot + gap;
    const int lcd_w = LCD_DOTS_W(dot, gap);
    const int lcd_h = LCD_DOTS_H(dot, gap);
    int i;

    lcd_rect(fb, ox, oy, ox + lcd_w, oy + lcd_h, LCD_OFF);

    for (i = 0; i < SEG_COUNT; i++) {
        const seg_entry *e = &seg_map[i];
        const seg_run *r;

        if (!((dpram[e->byte] >> e->bit) & 1))
            continue;                       /* unlit: leave the background */

        r = &seg_runs[e->run_start];
        for (int k = 0; k < e->run_count; k++, r++) {
            int y0 = oy + r->y * pitch;

            for (int dy = 0; dy < dot; dy++) {
                int yy = y0 + dy;

                for (int x = r->x0; x < r->x1; x++) {
                    int xx = ox + x * pitch;

                    lcd_rect(fb, xx, yy, xx + dot, yy + 1, LCD_INK);
                }
            }
        }
    }
}

/* True-hardware look: dot blocks touching, no grid. */
static inline void lcd_draw_scaled(const lcd_fb *fb, const uint8_t *dpram,
                                   int ox, int oy, int scale)
{
    lcd_draw_dots(fb, dpram, ox, oy, scale, 0);
}

#endif /* RENDER_H */
