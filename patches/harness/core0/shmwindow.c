/*
 * shmwindow.c - prove that core 0 can hand data to Linux through PSRAM.
 *
 * This is the third attempt. The first two failed the same way and the reason
 * is now known, so this one is built to avoid it rather than to be clever:
 *
 *  1. They logged through the log pipe. ble_hid.c records why that is fatal
 *     here -- a high-volume log hook "wedges this board a second after
 *     handoff, reproducibly, at the shmem probe". This file logs nothing.
 *     Every value it publishes goes into the shared window itself, so the
 *     Linux side is both the reader and the evidence.
 *
 *  2. They asked heap_caps for MALLOC_CAP_SPIRAM. The firmware has no PSRAM
 *     heap at all (CONFIG_SPIRAM_USE_MEMMAP=y, CONFIG_SPIRAM_USE_MALLOC unset),
 *     so both probes got NULL and returned before touching anything. This one
 *     writes to a fixed address that the boot map already provides.
 *
 * The address. Linux's memory node is 0x3d800000 + 0x700000, and the firmware
 * maps MMU entries 0x1f0..0x1ff onto PSRAM pages 0x70..0x7f, so the top
 * megabyte is mapped on both sides at 0x3df00000 and owned by neither. The
 * MMU table is global to both cores, so that is one mapping, not two.
 *
 * The gate. A task that writes 0x3df00000 *before* init_linux_mmap() would be
 * writing somewhere else -- the plain IDF PSRAM map puts that virtual address
 * on a different physical page. So the writer watches MMU entry 0x1f0 and
 * starts only once the value is (MMU_ACCESS_SPIRAM | page 0x70), which is the
 * exact state init_linux_mmap() leaves behind. No flag, no cross-file API, and
 * no window in which the task can write to the wrong page.
 *
 * What it writes. A header of constants, then an incrementing counter, flushed
 * with esp_cache_msync() every tick. Whether core 1 sees the counter move is
 * the coherency answer. Core 0 has a D-cache; core 1 reportedly has none, and
 * if the counter arrives this is the only thing that needed proving before the
 * emulator can move to core 0.
 */

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cache.h"
#include "hal/mmu_ll.h"
#include "soc/ext_mem_defs.h"

#include "shmwindow.h"

/* The window both cores see. 64-byte aligned: esp_cache_msync needs it. */
#define SHM_WINDOW_ADDR     0x3df00000u
#define SHM_WINDOW_MMU_ENT  0x1f0
#define SHM_WINDOW_PAGE     0x70

/* The MMU entry value init_linux_mmap() writes for the window's first page. */
#define SHM_MMU_EXPECTED    (MMU_ACCESS_SPIRAM | SHM_WINDOW_PAGE)

#define SHM_TICK_MS         100
#define SHM_FLUSH_BYTES     64

#define SHM_MAGIC           0x53484d57u   /* 'SHMW' */

struct shm_window {
	volatile uint32_t magic;
	volatile uint32_t seq;
	volatile uint32_t self;      /* the address the writer is using */
	volatile uint32_t mmu;       /* the entry value it waited for */
	volatile uint32_t pad[12];   /* to the 64-byte flush boundary */
};

static struct shm_window *const win = (struct shm_window *)SHM_WINDOW_ADDR;

static void shmwindow_task(void *arg)
{
	uint32_t waited = 0;

	(void)arg;

	/*
	 * Wait for Linux's map. Polling the MMU table rather than sleeping on a
	 * flag means there is no start-up ordering for anyone else to get wrong.
	 */
	while (mmu_ll_read_entry(0, SHM_WINDOW_MMU_ENT) != SHM_MMU_EXPECTED) {
		vTaskDelay(pdMS_TO_TICKS(10));
		if (++waited > 3000) {
			/* Half a minute and still not mapped: leave the window
			 * untouched and give up silently. A writer that never
			 * starts is a failed test, not a broken board. */
			vTaskDelete(NULL);
			return;
		}
	}

	win->magic = SHM_MAGIC;
	win->self  = (uint32_t)(uintptr_t)win;
	win->mmu   = mmu_ll_read_entry(0, SHM_WINDOW_MMU_ENT);
	win->seq   = 0;

	esp_cache_msync((void *)win, SHM_FLUSH_BYTES,
			ESP_CACHE_MSYNC_FLAG_INVALIDATE);

	for (;;) {
		win->seq = win->seq + 1;

		/*
		 * The whole point. Without this the counter lives in core 0's
		 * D-cache and core 1 reads whatever PSRAM held before.
		 */
		esp_cache_msync((void *)win, SHM_FLUSH_BYTES,
				ESP_CACHE_MSYNC_FLAG_INVALIDATE);

		vTaskDelay(pdMS_TO_TICKS(SHM_TICK_MS));
	}
}

void shmwindow_start(void)
{
	/* Pinned to core 0: core 1 is Linux. Priority 1 so it never competes
	 * with the radios. */
	xTaskCreatePinnedToCore(shmwindow_task, "shmwindow", 3072, NULL, 1,
				NULL, 0);
}
