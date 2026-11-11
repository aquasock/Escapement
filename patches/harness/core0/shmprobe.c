/*
 * shmprobe.c - prove that core 0 can hand data to Linux through PSRAM.
 *
 * This exists to answer one question before committing to the core-0
 * architecture in EMU-004: when the firmware writes pixels into a PSRAM buffer,
 * can Linux read them, and does the cache get in the way?
 *
 * The concern is specific and worth stating. Core 0 has a data cache; core 1 has
 * none. If the firmware writes a frame, those writes land in core 0's cache and
 * are not necessarily in memory that core 1 will read, so the blitter could
 * show a stale frame -- intermittently, and only under some timings. That is the
 * failure mode that would make the whole design unworkable, so it gets tested
 * first, with a counter rather than a framebuffer.
 *
 * What this does: allocate a small buffer in PSRAM, write an increasing counter
 * into it forever, and flush it to memory after every write. The address is
 * logged once so Linux can read the same bytes.
 *
 * Deliberately a SEPARATE file rather than edits scattered through app_main,
 * because this is a throwaway experiment and removing it should be one deleted
 * file plus one call.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_log.h"

#define SHM_PROBE_WORDS 64          /* 256 bytes: small, and cache-line aligned */
#define SHM_PROBE_TICK_MS 100       /* slow enough to read by hand */

static const char *TAG = "shmprobe";

/* 8-byte aligned; esp_cache_msync needs the address to be line aligned. */
static uint32_t *probe_buf;
static volatile uint32_t probe_seq;

static void shmprobe_task(void *arg)
{
    (void)arg;

    for (;;) {
        probe_seq++;

        probe_buf[0] = probe_seq;
        probe_buf[1] = 0x5A5A0000u | (probe_seq & 0xFFFF);
        probe_buf[2] = (uint32_t)(uintptr_t)probe_buf;

        /*
         * The whole point. Without this the counter lives in core 0's cache and
         * core 1 reads whatever was in PSRAM before.
         *
         * ESP_CACHE_MSYNC_FLAG_INVALIDATE is included as well as the writeback:
         * the default action writes dirty lines to memory, and the flag also
         * invalidates them so the next read by this core comes from memory
         * rather than from a stale line. For a one-way producer that is the
         * conservative choice.
         */
        esp_cache_msync(probe_buf, SHM_PROBE_WORDS * sizeof(uint32_t),
                        ESP_CACHE_MSYNC_FLAG_INVALIDATE);

        vTaskDelay(pdMS_TO_TICKS(SHM_PROBE_TICK_MS));
    }
}

/*
 * Called from app_main after the log pipe is up, so the address this prints is
 * readable on the Linux side over the existing ESP_LOG_IF channel.
 */
void shmprobe_init(void)
{
    probe_buf = heap_caps_aligned_alloc(64,
                                       SHM_PROBE_WORDS * sizeof(uint32_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!probe_buf) {
        ESP_LOGE(TAG, "no PSRAM for the probe buffer");
        return;
    }

    memset(probe_buf, 0, SHM_PROBE_WORDS * sizeof(uint32_t));

    ESP_LOGW(TAG, "probe buffer at %p (0x%08x), %d words, tick %d ms",
             probe_buf, (unsigned)(uintptr_t)probe_buf,
             SHM_PROBE_WORDS, SHM_PROBE_TICK_MS);

    xTaskCreate(shmprobe_task, "shmprobe", 3072, NULL, 3, NULL);
}
