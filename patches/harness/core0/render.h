/*
 * render.h - draw the LCD segment map into an RGB565 buffer, at any integer
 * scale, without a second copy of the map.
 *
 * This started as a copy of the harness's render.h, which draws one pixel per
 * LCD dot. It is carried here because the firmware needs a scale factor and the
 * harness file does not have one.
 *
 * Why the scale is applied here and not baked into a bigger map: a 4x map is
 * four times as many runs (5366 -> 21464), which is 126 KB of const data, and
 * that overflowed the 768 KB firmware partition by 57 KB. Scaling at draw time
 * costs nothing at all -- the map stays 32 KB and each run is drawn SEG_SCALE
 * times wider and SEG_SCALE rows taller.
 *
 * The result is exact either way. A run is a horizontal span of whole LCD dots,
 * so multiplying the coordinates by an integer scale lands every edge on an
 * exact pixel boundary: no resampling, no anti-aliasing, and no gap between
 * dots -- which is what "4x, no spacing pixels" means.
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

/*
 * Draw the LCD with its top-left corner at (ox, oy), each LCD dot as a
 * SEG_SCALE x SEG_SCALE block of pixels.
 *
 * The inner loop is written row-major rather than per-dot so that a lit run
 * costs one memset-like sweep per output row, not one call per dot. At 4x a
 * full-screen repaint touches 196*196 pixels and this is the hot path on
 * core 0.
 */
static inline void lcd_draw_scaled(const lcd_fb *fb, const uint8_t *dpram,
                                   int ox, int oy, int scale)
{
    int i;

    lcd_rect(fb, ox, oy, ox + SEG_W * scale, oy + SEG_H * scale, LCD_OFF);

    for (i = 0; i < SEG_COUNT; i++) {
        const seg_entry *e = &seg_map[i];
        const seg_run *r;

        if (!((dpram[e->byte] >> e->bit) & 1))
            continue;                       /* unlit: leave the background */

        r = &seg_runs[e->run_start];
        for (int k = 0; k < e->run_count; k++, r++) {
            int y0 = oy + r->y * scale;
            int x0 = ox + r->x0 * scale;
            int x1 = ox + r->x1 * scale;

            /* One horizontal span of whole dots, repeated down the block. */
            for (int dy = 0; dy < scale; dy++)
                lcd_rect(fb, x0, y0 + dy, x1, y0 + dy + 1, LCD_INK);
        }
    }
}

/* Convenience wrapper at the project's scale. */
#ifndef SEG_SCALE
#define SEG_SCALE 1
#endif

static inline void lcd_draw(const lcd_fb *fb, const uint8_t *dpram, int ox, int oy)
{
    lcd_draw_scaled(fb, dpram, ox, oy, SEG_SCALE);
}

#endif /* RENDER_H */
