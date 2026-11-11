/*
 * tamacore_task.c - the Tamagotchi runs on core 0.
 *
 * Core 1 has no data cache and executes from PSRAM, which is why the guest
 * costs ~464 host cycles per emulated instruction there and the status menu
 * limped at 0.52x. Core 0 has a cache. So the guest moves here and core 1 keeps
 * the job it is actually good at: pushing pixels to the panel.
 *
 * The two halves meet in the PSRAM window (shmproto.h). Core 1 copies the ROM
 * in once and sets rom_ready; this task then owns the machine and publishes a
 * frame whenever it has emulated 1/16 s of guest time.
 *
 * Three constraints, each learned the hard way in this project:
 *
 *   - No logging. A high-volume log hook through the log pipe wedges the board
 *     a second after handoff (ble_hid.c records the measurement). Progress is
 *     published in the control block instead, so a failure is still visible.
 *   - No PSRAM allocation. The firmware has no PSRAM heap, so heap_caps returns
 *     NULL. Everything here uses fixed window addresses or internal RAM.
 *   - No flash writes. A flash write from core 0 after Linux is up aborts.
 *     Nothing here touches flash; core 1 persists the save image.
 *
 * The guest must not fall behind. If it is starved by the radios it does not
 * skip emulated time, it runs late -- pacing below always chases the wall clock,
 * and `jitter_ms` records the worst lateness so the cost is visible rather than
 * felt.
 */

#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cache.h"
#include "esp_timer.h"
#include "hal/mmu_ll.h"
#include "soc/ext_mem_defs.h"

#include "splb32.h"
#include "segmap.h"
#include "render.h"
#include "shmproto.h"

/* 16 fps: the real hardware redraws its LCD at about 32 Hz and the guest only
 * produces new pixels at that rate, so 16 halves the transport with nothing
 * visible lost. */
#define TAMACORE_FPS        16u
#define FRAME_CYCLES        (SPLB32_CLOCK_HZ / TAMACORE_FPS)

/* 4 ms of guest time per sub-step. Small enough that a short tap still lands
 * between two of them, so a press cannot be born and die inside one step. */
#define STEP_CYCLES         (SPLB32_CLOCK_HZ / 250u)

#define EEPROM_PUBLISH_US   2000000                      /* 2 s */

_Static_assert(sizeof(struct shm_ctl) == 128, "control block must be four cache lines");
_Static_assert(SHM_FRAME_STRIDE >= SHM_FRAME_BYTES, "frame stride must hold a whole frame");

/*
 * The gate, and it is not optional.
 *
 * Before init_linux_mmap() repoints them, MMU entries 0x1f0..0x1ff belong to
 * the firmware's own PSRAM, so 0x3df00000 is a DIFFERENT physical page and a
 * write there corrupts the firmware. Doing it anyway is what put this board in
 * a boot loop -- rst:0xc at the handoff -- because the task's first statement
 * touched the control block.
 *
 * So nothing in the window is read or written until entry 0x1f0 reads back
 * exactly what init_linux_mmap() leaves: PSRAM, page 0x70.
 */
#define SHM_WINDOW_MMU_ENT	0x1f0
#define SHM_WINDOW_PAGE		0x70
#define SHM_MMU_EXPECTED	(MMU_ACCESS_SPIRAM | SHM_WINDOW_PAGE)

static int window_mapped(void)
{
	return mmu_ll_read_entry(0, SHM_WINDOW_MMU_ENT) == SHM_MMU_EXPECTED;
}

static struct shm_ctl *const ctl = (struct shm_ctl *)SHM_BASE;
static uint16_t *const frame[2] = {
	(uint16_t *)(SHM_BASE + SHM_FRAME0_OFF),
	(uint16_t *)(SHM_BASE + SHM_FRAME1_OFF),
};
static uint8_t *const rom_image = (uint8_t *)(SHM_BASE + SHM_ROM_OFF);
static uint8_t *const eeprom_image = (uint8_t *)(SHM_BASE + SHM_EEPROM_OFF);

/* Line 1 belongs to core 1: drop our cached copy before reading it, or a button
 * press could stay invisible for as long as the line lives in our D-cache. */
static inline void ctl_read_peer_line(void)
{
	esp_cache_msync((void *)((uint8_t *)ctl + 32), 32,
			ESP_CACHE_MSYNC_FLAG_INVALIDATE);
}

/*
 * Lines 0 and 2 belong to us. Flush them before core 1 reads them.
 *
 * INVALIDATE as well as writeback, and that is not belt-and-braces: with flags
 * of 0 the writes stayed in core 0's D-cache and core 1 read whatever the
 * previous boot had left in PSRAM, so the control block looked frozen. The
 * earlier one-way probe used this flag and was visible, which is the evidence.
 * Invalidating our own lines costs a refill on the next read and nothing else.
 */
static inline void ctl_publish(void)
{
	esp_cache_msync((void *)ctl, 32, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
	esp_cache_msync((void *)((uint8_t *)ctl + 64), 32,
			ESP_CACHE_MSYNC_FLAG_INVALIDATE);
}

/*
 * Zero every field this side owns, explicitly.
 *
 * PSRAM is not zeroed at power-on, so a field nobody writes reads as whatever
 * the last boot left there. That is not theoretical: a stale `frames` counter
 * and a stale `buttons` mask both showed up on the first run, and the second
 * would have driven phantom button presses into the guest.
 */
static void ctl_init_own(void)
{
	/*
	 * Take ownership of the whole block first, both sides' lines included.
	 *
	 * That is safe because the handshake is ordered around it: core 1 waits
	 * for `magic` before it touches anything here, so at this moment the
	 * block is nobody else's. And it is necessary, because PSRAM survives a
	 * reset -- stale ready flags and a stale `quit` from the previous boot
	 * once convinced the guest it had already been told to stop, and it
	 * exited before emulating anything.
	 */
	memset((void *)ctl, 0, sizeof(*ctl));

	ctl->magic = SHM_MAGIC;
	ctl->version = SHM_VERSION;
	ctl->stage = SHM_STAGE_WAIT_ROM;
	ctl->err = SHM_ERR_NONE;
	ctl->frame_pad = SHM_FRAME_PAD;
	ctl->rom_bytes = SHM_ROM_BYTES;
	ctl->reserved0[0] = 0;
	ctl->reserved0[1] = 0;

	ctl->frame_seq = 0;
	ctl->frame_idx = 0;
	ctl->frames = 0;
	ctl->guest_hz = 0;
	ctl->guest_cycles = 0;
	ctl->jitter_ms = 0;
	ctl->eeprom_seq = 0;
	ctl->reserved2 = 0;

	ctl_publish();
}

static void draw_frame(uint16_t *px, const uint8_t *dpram)
{
	lcd_fb fb = { px, SHM_FRAME_W, SHM_FRAME_H };
	int i;

	/*
	 * Bezel first, in a colour distinct from the LCD background. Filling
	 * this with SHM_LCD_OFF made the surround and the LCD the same shade,
	 * so a blank screen and a screen full of pixels looked alike -- and
	 * because core 1 blits a full-width strip, the surround appeared as a
	 * flat band across the whole framebuffer. Black surrounds match the
	 * console background, so only the LCD itself is visible.
	 */
	for (i = 0; i < SHM_FRAME_W * SHM_FRAME_H; i++)
		px[i] = SHM_BEZEL;

	lcd_draw(&fb, dpram, SHM_FRAME_PAD, SHM_FRAME_PAD);
}

static void apply_buttons(splb32 *cpu, uint32_t mask, uint32_t *applied)
{
	static const uint8_t bit[3] = { 0x01, 0x02, 0x04 };
	int i;

	if (mask == *applied)
		return;

	for (i = 0; i < 3; i++) {
		int now = (mask & bit[i]) != 0;
		int was = (*applied & bit[i]) != 0;

		if (now != was)
			splb32_pin(cpu, 0, bit[i], now ? 1 : -1);
	}

	*applied = mask;
}

/*
 * Publish the save image, but only when the guest has actually changed it.
 *
 * The first version wrote it unconditionally every two seconds. On a guest that
 * was not writing its EEPROM yet, that replaced a real /home/tama.sav with 1 KB
 * of zeros — destroying the save, and doing it silently. A shadow copy of what
 * was seeded is enough to tell the difference.
 */
static uint8_t eeprom_shadow[1024];

static void publish_eeprom(splb32 *cpu)
{
	uint8_t buf[1024];

	if (splb32_eeprom_save(cpu, buf) != 0)
		return;

	if (memcmp(buf, eeprom_shadow, sizeof(buf)) == 0)
		return;                 /* unchanged: leave the file alone */

	memcpy(eeprom_shadow, buf, sizeof(buf));
	memcpy(eeprom_image, buf, sizeof(buf));
	esp_cache_msync(eeprom_image, sizeof(buf), ESP_CACHE_MSYNC_FLAG_INVALIDATE);
	ctl->eeprom_seq = ctl->eeprom_seq + 1;
	ctl_publish();
}

/*
 * One complete session: take the window, wait for the handshake, run the guest
 * until core 1 asks it to stop, hand the save image back.
 *
 * Returns so the task can start again. It used to vTaskDelete() itself here,
 * which made the whole thing one-shot: after the first session ended the task
 * was gone until the next reboot, and every later run of the blitter read a
 * stale control block and reported the previous run's numbers. Restarting is
 * also what makes iterating on this possible without a flash each time.
 */
static void tamacore_session(void)
{
	splb32 *cpu = NULL;
	uint64_t cycles_done = 0;
	uint64_t next_frame_cycle = FRAME_CYCLES;
	int64_t t0_us, last_eeprom_us, last_hz_us = 0;
	uint64_t last_hz_cycles = 0;
	uint32_t idx = 0;
	uint32_t buttons = 0, applied = 0;

	/* Nothing below may touch the window until the boot map has given way to
	 * Linux's. See the gate comment above. */
	while (!window_mapped())
		vTaskDelay(pdMS_TO_TICKS(10));

	/* Line 1 is core 1's and is zeroed by core 1 before it raises a ready
	 * flag, so reading it here cannot see power-on garbage. */
	ctl_init_own();
	ctl->stage = SHM_STAGE_WAIT_ROM;
	ctl_publish();

	/* Wait for core 1 to put the ROM and the save image in place. Both are
	 * announced separately and in that order, so this cannot see half of
	 * either. */
	for (;;) {
		ctl_read_peer_line();
		if (ctl->rom_ready && ctl->eeprom_ready)
			break;
		if (ctl->quit)
			goto out;
		vTaskDelay(pdMS_TO_TICKS(20));
	}

	if (rom_image[0] != 0xA5 || rom_image[1] != 0x5F ||
	    rom_image[2] != 0x10 || rom_image[3] != 0x49) {
		ctl->stage = SHM_STAGE_ERROR;
		ctl->err = SHM_ERR_ROM_SIGNATURE;
		ctl_publish();
		goto out;
	}

	cpu = splb32_new(rom_image, SHM_ROM_BYTES);
	if (!cpu) {
		ctl->stage = SHM_STAGE_ERROR;
		ctl->err = SHM_ERR_CPU_ALLOC;
		ctl_publish();
		goto out;
	}

	/*
	 * The machine's own wiring, in this exact order and no reset afterwards.
	 *
	 * splb32_reset() memsets pullup_ext, so calling it after set_pullup()
	 * silently removes the EEPROM's SDA pull-up and the ROM then hangs in the
	 * I2C read -- which looked like a guest that ran at full speed and never
	 * drew anything, because it did: it was spinning. splb32_new() has
	 * already reset the core, so the wiring goes on afterwards, exactly as
	 * the userspace front end does it (tamafb.c).
	 */
	splb32_set_pullup(cpu, 3, 0x02);
	splb32_attach_eeprom(cpu, 0xA0, 0x02, 0x01);
	splb32_eeprom_load(cpu, eeprom_image);
	splb32_attach_ir(cpu, 0x04, 0x04);
	memcpy(eeprom_shadow, eeprom_image, sizeof(eeprom_shadow));

	ctl->stage = SHM_STAGE_RUNNING;
	ctl_publish();

	t0_us = esp_timer_get_time();
	last_eeprom_us = t0_us;

	for (;;) {
		int64_t now_us;
		uint64_t target;

		ctl_read_peer_line();
		if (ctl->quit)
			break;

		buttons = ctl->buttons;

		now_us = esp_timer_get_time() - t0_us;
		target = (uint64_t)SPLB32_CLOCK_HZ * (uint64_t)now_us / 1000000u;

		if (cycles_done + STEP_CYCLES > target) {
			/* Caught up: yield. The tick is 10 ms, so the guest
			 * runs in short bursts between delays and the absolute
			 * deadline above keeps the average exact. */
			vTaskDelay(1);
			continue;
		}

		cycles_done += splb32_run_cycles(cpu, STEP_CYCLES);
		apply_buttons(cpu, buttons, &applied);

		/* Publish on guest time, not wall time: the frame rate then
		 * follows the machine, and a starved guest shows up as jitter
		 * rather than as a lie. */
		while (cycles_done >= next_frame_cycle) {
			int64_t deadline_us;
			int64_t late_ms;

			draw_frame(frame[idx], splb32_dpram(cpu));
			esp_cache_msync(frame[idx], SHM_FRAME_STRIDE,
					ESP_CACHE_MSYNC_FLAG_INVALIDATE);

			ctl->frame_idx = idx;
			ctl->frames = ctl->frames + 1;
			ctl->guest_cycles = (uint32_t)cycles_done;
			{
				/* Diagnostics: is the guest executing, and does
				 * it ever write display RAM? A blank LCD with a
				 * moving PC means it is spinning, not drawing. */
				const uint8_t *dp = splb32_dpram(cpu);
				uint32_t lit = 0;
				int n;

				for (n = 0; n < 0x200; n++)
					if (dp[n])
						lit++;

				{
					splb32_state st;

					splb32_snapshot(cpu, &st);
					ctl->guest_pc = st.pc;
					ctl->instrs_lo = st.instrs;
				}
				ctl->dpram_lit = lit;
			}
			ctl->frame_seq = ctl->frame_seq + 1;  /* last: readers gate on it */

			deadline_us = (int64_t)(next_frame_cycle * 1000000ull /
						SPLB32_CLOCK_HZ);
			late_ms = ((esp_timer_get_time() - t0_us) - deadline_us) / 1000;
			if (late_ms > (int64_t)ctl->jitter_ms)
				ctl->jitter_ms = (uint32_t)late_ms;

			if (now_us > last_hz_us) {
				uint64_t d = cycles_done - last_hz_cycles;

				ctl->guest_hz = (uint32_t)(d * 1000000ull /
							   (uint64_t)(now_us - last_hz_us));
			}

			ctl_publish();

			idx ^= 1;
			next_frame_cycle += FRAME_CYCLES;
		}

		if (now_us > last_hz_us) {
			last_hz_us = now_us;
			last_hz_cycles = cycles_done;
		}

		/* Republish the save image every couple of seconds. 1 KB is
		 * nothing, and doing it unconditionally means neither side has
		 * to ask the other's permission to persist. */
		if (esp_timer_get_time() - last_eeprom_us > EEPROM_PUBLISH_US) {
			publish_eeprom(cpu);
			last_eeprom_us = esp_timer_get_time();
		}
	}

out:
	/* Hand the final save image over before going quiet. */
	if (cpu)
		publish_eeprom(cpu);

	/* Keep an error visible: an ERROR stage that turns into IDLE on the way
	 * out is a failure nobody can see. */
	if (ctl->stage != SHM_STAGE_ERROR)
		ctl->stage = SHM_STAGE_IDLE;
	ctl_publish();

	splb32_free(cpu);
}

static void tamacore_task(void *arg)
{
	(void)arg;

	for (;;) {
		tamacore_session();

		/* Do not treat the flags still standing from the session that
		 * just ended as a new handshake. Wait for core 1 to drop them,
		 * which it does when it starts again. */
		for (;;) {
			ctl_read_peer_line();
			if (!ctl->rom_ready && !ctl->eeprom_ready)
				break;
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

void tamacore_start(void)
{
	/* Core 0, priority 1: above idle so it is scheduled, below the radio
	 * tasks so a WiFi burst always wins. At 16 fps the guest needs a
	 * fraction of this core, and the pacing above turns any starvation into
	 * measurable jitter rather than silent slowdown. */
	BaseType_t ok;

	ok = xTaskCreatePinnedToCore(tamacore_task, "tamacore", 8192, NULL, 1,
				     NULL, 0);
	if (ok != pdPASS)
		ok = xTaskCreatePinnedToCore(tamacore_task, "tamacore", 4096, NULL,
					     1, NULL, 0);
	(void)ok;
}
