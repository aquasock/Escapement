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
#include "esp_system.h"
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

/* Largest slice of guest time run between yields: 20 ms. Small enough that a
 * button press and a frame deadline are never missed by more than that. */
#define SLICE_CYCLES        (SPLB32_CLOCK_HZ / 50u)

/* Two seconds of guest time in one call, before the first frame. This is how
 * the userspace front end starts (tamafb.c), and without it the first core-0
 * build came up blank: the paced loop alone never let the ROM finish booting. */
#define BOOT_CYCLES         (SPLB32_CLOCK_HZ * 2u)

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
	 * Clear OUR lines only -- 0, 2 and 3 -- never line 1, which is core 1's.
	 *
	 * PSRAM is not zeroed at power-on, so a stale `frames` counter and a
	 * stale button mask both read as live values and this has to clear
	 * something. But clearing the whole block is a race: core 1 can raise
	 * rom_ready between core 0 reading it and core 0 wiping it, and then
	 * both sides wait forever. That is exactly what happened -- stage stuck
	 * at WAIT_ROM, no frames, an emulator that looked dead.
	 *
	 * Each side initialising its own line is the rule everywhere else in
	 * this structure, and the handshake below makes it sufficient.
	 */
	/*
	 * Clear line 0 and line 2, and the per-session fields of line 3 -- but
	 * preserve reset_reason and boot_count.
	 *
	 * Those two describe the boot rather than the session, and clearing them
	 * here is what made the reset reason read as 0 every time it was
	 * checked: the value was correct for a moment and then wiped by the
	 * first session's initialisation. The INT_WDT readings that did survive
	 * came from a build that wrote the reason after this clear.
	 */
	memset((void *)ctl, 0, 32);                     /* line 0 */
	memset((void *)((uint8_t *)ctl + 64), 0, 32);   /* line 2 */

	ctl->guest_pc = 0;
	ctl->dpram_lit = 0;
	ctl->instrs_lo = 0;
	ctl->btn_events = 0;

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

		if (now != was) {
			splb32_pin(cpu, 0, bit[i], now ? 1 : -1);
			ctl->btn_events = ctl->btn_events + 1;
		}
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
static void tamacore_session(uint32_t served, uint32_t *served_out)
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

	/* Wait for core 1 to announce a new session with the ROM and the save
	 * image in place. Both flags are set before `session` moves, so seeing a
	 * new session implies both are real. */
	for (;;) {
		ctl_read_peer_line();
		if (ctl->rom_ready && ctl->eeprom_ready &&
		    ctl->session != served)
			break;
		if (ctl->quit)
			goto out;
		vTaskDelay(pdMS_TO_TICKS(20));
	}

	*served_out = ctl->session;

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

	/*
	 * Pacing, rewritten after the first paced version starved the guest.
	 *
	 * The old loop computed a wall-clock deadline in microseconds and either
	 * ran a 4 ms slice or slept a whole 10 ms tick, so the guest advanced in
	 * bursts that did not add up to real time and never finished booting:
	 * ~105k opcodes retired in ten seconds, display RAM untouched. The boot
	 * burst below now happens first and fills the screen, and the steady state
	 * is one tick of guest time per tick of wall time.
	 *
	 * One step per vTaskDelay(1) is the whole trick. It cannot starve the
	 * core-0 idle task -- which the task watchdog watches, and which a busy
	 * loop would have tripped -- and the guest's clock and the wall clock
	 * advance together at 100 Hz.
	 */
	cycles_done = splb32_run_cycles(cpu, BOOT_CYCLES);   /* absolute */
	last_eeprom_us = esp_timer_get_time();

	/* Pretend the burst already took its two seconds of wall time, so the
	 * measured rate is the guest's real rate rather than an artefact of
	 * starting the clock after the burst. */
	t0_us = esp_timer_get_time() -
		(int64_t)(cycles_done * 1000000ull / SPLB32_CLOCK_HZ);
	last_hz_us = 0;
	last_hz_cycles = cycles_done;
	next_frame_cycle = cycles_done + FRAME_CYCLES;

	for (;;) {
		int64_t now_us;
		uint64_t target;

		ctl_read_peer_line();
		if (ctl->quit)
			break;

		buttons = ctl->buttons;

		/*
		 * Absolute pacing: how many guest cycles *should* have elapsed by
		 * now, and run up to that. Self-correcting, so tick jitter cannot
		 * accumulate.
		 *
		 * splb32_run_cycles() returns the machine's absolute cycle count,
		 * not the number it just added. The first version wrote
		 * `cycles_done += splb32_run_cycles(...)`, which inflated the
		 * counter toward 2^64, made the "caught up" test always true, and
		 * starved the guest to ~105k opcodes in ten seconds while reporting
		 * a healthy 4 MHz. That single += is why the screen stayed blank.
		 */
		now_us = esp_timer_get_time() - t0_us;
		target = (uint64_t)SPLB32_CLOCK_HZ * (uint64_t)now_us / 1000000u;

		if (cycles_done >= target) {
			vTaskDelay(1);          /* ahead of the wall clock: yield */
			continue;
		}

		{
			uint64_t want = target - cycles_done;

			if (want > SLICE_CYCLES)
				want = SLICE_CYCLES;

			cycles_done = splb32_run_cycles(cpu, want);
		}

		apply_buttons(cpu, buttons, &applied);

		/* Publish on guest time, not wall time: the frame rate follows the
		 * machine, so a starved guest shows up as jitter rather than as a
		 * number that looks fine. */
		if (cycles_done >= next_frame_cycle) {
			const uint8_t *dp = splb32_dpram(cpu);
			splb32_state st;
			uint32_t lit = 0;
			int64_t deadline_us;
			int64_t late_ms;
			int n;

			draw_frame(frame[idx], dp);
			esp_cache_msync(frame[idx], SHM_FRAME_STRIDE,
					ESP_CACHE_MSYNC_FLAG_INVALIDATE);

			for (n = 0; n < 0x200; n++)
				if (dp[n])
					lit++;

			splb32_snapshot(cpu, &st);

			ctl->guest_pc = st.pc;
			ctl->instrs_lo = st.instrs;
			ctl->dpram_lit = lit;
			ctl->frame_idx = idx;
			ctl->frames = ctl->frames + 1;
			ctl->guest_cycles = (uint32_t)cycles_done;

			deadline_us = (int64_t)(next_frame_cycle * 1000000ull /
						SPLB32_CLOCK_HZ);
			late_ms = (now_us - deadline_us) / 1000;
			if (late_ms > (int64_t)ctl->jitter_ms)
				ctl->jitter_ms = (uint32_t)late_ms;

			if (now_us > last_hz_us) {
				uint64_t d = cycles_done - last_hz_cycles;

				ctl->guest_hz = (uint32_t)(d * 1000000ull /
							   (uint64_t)(now_us - last_hz_us));
				last_hz_us = now_us;
				last_hz_cycles = cycles_done;
			}

			ctl->frame_seq = ctl->frame_seq + 1;  /* last: readers gate on it */
			ctl_publish();

			idx ^= 1;

			/* If the guest fell more than a frame behind, skip the frames
			 * it missed rather than publishing a backlog. */
			do {
				next_frame_cycle += FRAME_CYCLES;
			} while (next_frame_cycle <= cycles_done);
		}

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

/*
 * Once per boot, before any session: record why the previous boot ended.
 *
 * esp_reset_reason() describes the reset that got us here, so without this a
 * reboot is a mystery -- the console log has scrolled past, Linux's dmesg
 * starts empty, and a task watchdog, an abort(), a brownout and a stack
 * overflow all look identical from the outside. Values are esp_reset_reason_t:
 * 1 = power-on, 3 = software, 4 = panic, 5 = interrupt watchdog,
 * 6 = task watchdog, 7 = other watchdog, 9 = brownout.
 *
 * `boots` is the count of real boots, kept in PSRAM so it survives the reset.
 * It is deliberately NOT incremented per session: an earlier version put it
 * inside the session loop, and a spinning outer loop made it climb like a boot
 * loop that was not happening.
 */
static void ctl_boot_once(void)
{
	uint32_t boots = ctl->boot_count;

	ctl->reset_reason = (uint32_t)esp_reset_reason();
	ctl->boot_count = boots + 1;

	esp_cache_msync((void *)((uint8_t *)ctl + 96), 32,
			ESP_CACHE_MSYNC_FLAG_INVALIDATE);
}

static void tamacore_task(void *arg)
{
	uint32_t served;

	(void)arg;

	/* Wait for the window before touching anything, then take the boot
	 * record. `served` starts at whatever session is already published, so
	 * core 0 only begins a machine when core 1 actually asks for one --
	 * a stale count from the previous boot is not a request. */
	while (!window_mapped())
		vTaskDelay(pdMS_TO_TICKS(10));

	ctl_boot_once();

	esp_cache_msync((void *)((uint8_t *)ctl + 32), 32,
			ESP_CACHE_MSYNC_FLAG_INVALIDATE);
	served = ctl->session;

	/*
	 * One session per core 1 run, numbered rather than flagged.
	 *
	 * Remembering which session was already served is what lets this restart
	 * cleanly: a new tamacore on core 1 bumps the counter, and core 0 starts
	 * a fresh machine. No flag has to be dropped and re-raised, so there is
	 * no window in which both sides can be waiting on each other.
	 */
	for (;;) {
		tamacore_session(served, &served);
		vTaskDelay(pdMS_TO_TICKS(50));
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
