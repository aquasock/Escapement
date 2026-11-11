/*
 * Publish the flash-path instrument's counters into the shared window.
 *
 * The instrument itself lives in esp-idf's spi_flash/cache_utils.c, because
 * spi_flash_disable_interrupts_caches_and_other_cpu() is the only place that
 * catches EVERY cache-disabling flash operation, including ones this project
 * does not own. It has to stay there and it has to stay tiny.
 *
 * WHY THE COUNTERS ARE NOT IN THE WINDOW
 *
 * Both PSRAM and the flash window are reached THROUGH the external-memory
 * cache on this chip -- /proc/iomem shows Linux has those two regions and no
 * internal SRAM. That function is always called with that cache disabled, so a
 * store to either one from inside it is not merely invalid, it cannot work. An earlier version of this
 * instrument guarded itself on a word at 0x3df70118 and stored at 0x3df70110;
 * the board stopped booting to Linux entirely, with no message anywhere,
 * because the firmware console is CONFIG_ESP_CONSOLE_UART_DEFAULT (UART0) and
 * nothing reaches the USB port. Two separate bugs were in that: a PSRAM access
 * from a context that cannot make one, and an offset (0x70110) that sits
 * inside SHM_DIAG_PREV_OFF, corrupting the crash snapshot.
 *
 * So the instrument only bumps variables in internal DRAM, and this task
 * copies them into the window where the cache is on and the access is legal.
 * The copy of the PREVIOUS boot's reading is made in linux_boot.c, the one
 * place that can guarantee the window is mapped first.
 *
 * WHAT THE NUMBERS MEAN
 *
 *   calls == 0   nothing on core 0 has disabled the caches since Linux started.
 *                A freeze under those conditions is not a flash operation, and
 *                the shmem/IPC path is the next suspect.
 *   calls > 0    something is doing flash work while core 1 executes. Map `ret`
 *                with xtensa-esp32s3-elf-addr2line to name the caller.
 *
 * MEASURED 2026-11-11: 25 calls at boot with Linux idle and 0 calls during a
 * 75 s and a 290 s emulator run. The calls that matter come from jffs2 writes:
 * a single 1 KiB write to /home with the emulator running rebooted the board
 * every time it was tried, at /home 74 percent full AND at 47 percent full.
 * The caller resolves to cache_disable() in spi_flash_os_func_app.c, i.e. the
 * ordinary esp_flash_* path reached from linux_flash.c's IPC handler, not the
 * emulator itself.
 */

#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cache.h"

#include "shmproto.h"

/* Defined in esp-idf's spi_flash/cache_utils.c, in internal DRAM. */
extern volatile uint32_t g_esc_flash_calls;
extern volatile uint32_t g_esc_flash_ret;
extern volatile uint32_t g_esc_flash_cpu;
extern volatile uint32_t g_esc_flash_armed;
extern volatile uint32_t g_esc_flash_max_cyc;
extern volatile uint32_t g_esc_flash_last_cyc;

static struct shm_flash_instr *const instr =
	(struct shm_flash_instr *)(SHM_BASE + SHM_DIAG_FLASH_OFF);
static void esc_instr_task(void *arg)
{
	(void)arg;

	for (;;) {
		if (g_esc_flash_armed) {
			instr->calls = g_esc_flash_calls;
			instr->ret = g_esc_flash_ret;
			instr->cpu = g_esc_flash_cpu;
			instr->max_cyc = g_esc_flash_max_cyc;
			instr->last_cyc = g_esc_flash_last_cyc;
			instr->armed = 1;
			instr->magic = SHM_DIAG_FLASH_MAGIC;

			/*
			 * Invalidate, not writeback: core 1 reads this, and
			 * core 1 has no data cache of its own, so the only
			 * requirement is that core 0's stores reach PSRAM
			 * rather than sitting in core 0's D-cache.
			 */
			esp_cache_msync((void *)instr, sizeof(*instr),
					ESP_CACHE_MSYNC_FLAG_INVALIDATE);
		}

		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

void esc_instr_start(void)
{
	/* Core 0, low priority: this is a 250 ms poll of four words and must
	 * never compete with the radios or the emulator. */
	xTaskCreatePinnedToCore(esc_instr_task, "escinstr", 3072, NULL, 1,
				NULL, 0);
}
