// red_screen.c - the smallest possible thing that turns the panel red.
//
// No Linux, no ragdoll, no gait, no framebuffer. This exists to answer one
// question and nothing else:
//
//     does the CO5300 on this watch light up at all?
//
// Everything it needs is already proven: board.c brings up the AXP2101 rails
// and the XL9555 expander (which gates display power on P07), and panel.c is
// the driver that draws the cat at 15 fps. Neither is modified. This program
// only calls them, in order, and then fills the glass with red.
//
// Why red and not white: white is what TinyPetz already shows at boot, so it
// proves nothing new. Red exercises different bits in every pixel, and a
// byte-order fault turns it blue -- which is the specific failure that once
// painted a tan cat pink.
//
// Why it loops rather than exiting: a bare `return` from app_main on ESP-IDF
// can drop into a panic or a reboot, and a reboot loop looks exactly like a
// dead panel. It repaints slowly and stays put.

#include <stdio.h>

#include "board.h"
#include "colour.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "panel.h"

static const char *TAG = "red";

#define BUS_MHZ 20

/*
 * The visible glass, in panel coordinates.
 *
 * NOT taken from PANEL_NATIVE_W/H. Those two are transposed relative to how
 * panel_fill_window wants them -- panel.h says PANEL_NATIVE_W 502 and
 * PANEL_NATIVE_H 410, while the glass is 410 columns by 502 rows. Using them
 * directly produced a window from column 22 to 523 (25 columns past the panel)
 * with the two dimensions swapped, and the panel drew nothing at all.
 *
 * These are the values TinyPetz itself passes, from its main.c: WIN_X0 22,
 * WIN_X1 431, PANEL_COLS 410, PANEL_ROWS 502.
 */
#define VIS_X0 22
#define VIS_X1 431
#define VIS_Y0 0
#define VIS_Y1 501
#define VIS_COLS 410
#define VIS_ROWS 502

void app_main(void)
{
    esp_err_t err;
    int frame = 0;

    ESP_LOGW(TAG, "=== red screen ===");

    // ---- power ------------------------------------------------------------
    // The AXP2101 rails and the XL9555 expander. The expander's P07 gates
    // display power; without it the panel is unpowered and shows nothing.
    ESP_ERROR_CHECK(board_i2c_init());
    err = board_pmu_init();
    ESP_LOGW(TAG, "pmu:     %s", esp_err_to_name(err));
    err = board_expander_init();
    ESP_LOGW(TAG, "expander:%s", esp_err_to_name(err));
    vTaskDelay(pdMS_TO_TICKS(100));

    // ---- panel ------------------------------------------------------------
    err = panel_init(BUS_MHZ);
    ESP_LOGW(TAG, "panel:   %s", esp_err_to_name(err));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel init failed - the panel will stay dark");
        // Keep running so the console stays alive and the failure is visible
        // rather than looking like a crash.
        for (;;)
            vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // TinyPetz does all three of these after init, and the brightness one is
    // not optional: the init table leaves 0x51 at 0x00, which is a black
    // backplane. A panel told to be black is indistinguishable from a dead one.
    panel_ctrl_raw(0x20);
    panel_brightness_raw(0xFF);
    panel_display_on();

    ESP_LOGW(TAG, "panel:   %d cols x %d rows, waving at %d MHz",
             VIS_COLS, VIS_ROWS, BUS_MHZ);

    // ---- red --------------------------------------------------------------
    const uint16_t red = RGB565(255, 0, 0);
    ESP_LOGW(TAG, "filling %d..%d x %d..%d with RGB565 0x%04X",
             VIS_X0, VIS_X1, VIS_Y0, VIS_Y1, red);

    for (;;) {
        int64_t t0 = esp_timer_get_time();

        err = panel_fill_window(VIS_X0, VIS_Y0, VIS_X1, VIS_Y1,
                                VIS_COLS, VIS_ROWS, red);

        int64_t ms = (esp_timer_get_time() - t0) / 1000;

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fill %d failed: %s", frame, esp_err_to_name(err));
        } else if ((frame % 4) == 0) {
            // Repeat occasionally rather than once: if the panel was not ready
            // for the first write, a later one still lands.
            ESP_LOGW(TAG, "fill %d: %lld ms", frame, ms);
        }

        frame++;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
