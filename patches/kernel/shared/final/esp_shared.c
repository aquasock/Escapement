// SPDX-License-Identifier: GPL-2.0-only
/*
 * esp_shared.c - the PSRAM megabyte both cores share, as a character device.
 *
 * The window is 0x3df00000..0x3e000000: excluded from the DTS memory node, so
 * the kernel never allocates from it, but still mapped in the MMU table, which
 * is global to both cores. Neither core has to remap it; both know the address.
 * See docs/cores.md and patches/harness/core0/shmproto.h, which is the contract
 * this file implements.
 *
 * Layout, and who owns what:
 *
 *   0x00000  4 KB    control block, named fields, one writer per field
 *   0x01000  8 KB    frame A, core 0 writes
 *   0x02000  8 KB    frame B, core 0 writes
 *   0x03000  4 KB    save image (1 KB), core 1 seeds, core 0 updates
 *   0x80000  512 KB  ROM, core 1 copies once, core 0 reads only
 *
 * This is a small driver on purpose. /dev/mem would do the same job in one line
 * and it stays off: on NOMMU there is no STRICT_DEVMEM to bound it and it would
 * reach core 0's own memory. This exposes one window, read/write, with the two
 * frame buffers refused to writes because core 0 is drawing into them.
 *
 * No cache maintenance on either side of the copy: core 1 has no data cache, so
 * its stores are already in memory, and core 0 flushes its own writes before it
 * publishes a sequence number.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <asm/cacheflush.h>

/* Mirrors patches/harness/core0/shmproto.h. Kept as literals rather than
 * included: this is a kernel header environment and that file is shared with
 * the firmware. The self-check below catches drift. */
#define SHM_BASE	0x3df00000UL
#define SHM_SIZE	0x00100000UL
#define SHM_FRAME0	0x00001000UL
#define SHM_FRAME1	0x0001b000UL
#define SHM_EEPROM	0x00035000UL
#define SHM_ROM		0x00080000UL

/*
 * A frame at 4x is 204x204 RGB565, which is 83,232 bytes.
 *
 * The first layout put the frames one page apart while a frame was 6498 bytes,
 * so every write to frame B ran past its slot and landed on the EEPROM image --
 * which then read back 18 c6 18 c6, i.e. pixel data. The frame is now 12.8x
 * larger and the slots grew with it. These asserts check the arithmetic against
 * the frame size rather than against a guess; the same numbers are asserted in
 * shmproto.h, which is the head of the contract.
 */
#define SHM_FRAME_BYTES	83232UL
#define SHM_FRAME_STRIDE	83232UL		/* padded to the 32-byte line */

_Static_assert(SHM_FRAME1 - SHM_FRAME0 >= SHM_FRAME_STRIDE,
	       "a frame does not fit in its slot");
_Static_assert(SHM_EEPROM - SHM_FRAME1 >= SHM_FRAME_STRIDE,
	       "the EEPROM image overlaps frame B");
_Static_assert(SHM_EEPROM + 1024UL <= SHM_ROM,
	       "the EEPROM image overlaps the ROM");
_Static_assert(SHM_BASE + SHM_ROM + 0x80000UL == SHM_BASE + SHM_SIZE,
	       "ROM must end at the window end");

/* The S3's data-cache line is 32 bytes; align every read operation to it. */
#define SHM_LINE	32UL

static bool invalidate = true;
module_param(invalidate, bool, 0644);
MODULE_PARM_DESC(invalidate,
		 "invalidate core 1's data cache over each read (default: on)");

static ssize_t esp_shared_read(struct file *file, char __user *ubuf,
			       size_t count, loff_t *ppos)
{
	unsigned long off = *ppos;
	unsigned long lo, hi;
	void *src;

	if (off >= SHM_SIZE)
		return 0;

	if (count > SHM_SIZE - off)
		count = SHM_SIZE - off;

	src = (void *)(SHM_BASE + off);

	if (invalidate) {
		lo = (unsigned long)src & ~(SHM_LINE - 1);
		hi = ALIGN((unsigned long)src + count, SHM_LINE);
		__invalidate_dcache_range(lo, hi - lo);
	}

	if (copy_to_user(ubuf, src, count))
		return -EFAULT;

	*ppos += count;
	return count;
}

/*
 * Core 1 seeds the ROM and the save image and pushes button state; core 0
 * publishes frames. Writes are therefore allowed everywhere except the two
 * frame buffers, which are the only range core 0 is actively rewriting. That
 * is one rule, checked by address, and it is the only way core 1 could corrupt
 * a frame that is being read out.
 */
static bool writable(unsigned long off, unsigned long count)
{
	unsigned long end = off + count;        /* first byte past the write */

	if (end > SHM_FRAME0 && off < SHM_FRAME0 + SHM_FRAME_STRIDE)
		return false;
	if (end > SHM_FRAME1 && off < SHM_FRAME1 + SHM_FRAME_STRIDE)
		return false;
	return true;
}

static ssize_t esp_shared_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	unsigned long off = *ppos;
	void *dst;

	if (off >= SHM_SIZE)
		return -EINVAL;

	if (count > SHM_SIZE - off)
		count = SHM_SIZE - off;

	if (!count)
		return 0;

	if (!writable(off, count))
		return -EPERM;

	dst = (void *)(SHM_BASE + off);

	if (copy_from_user(dst, ubuf, count))
		return -EFAULT;

	*ppos += count;
	return count;
}

static const struct proc_ops esp_shared_proc_ops = {
	.proc_read	= esp_shared_read,
	.proc_write	= esp_shared_write,
	.proc_lseek	= default_llseek,
};

static struct proc_dir_entry *esp_shared_entry;

static int __init esp_shared_init(void)
{
	/* 0644: the console user seeds the ROM and drives the buttons; nobody
	 * else needs it. The frame buffers stay unreadable to writes regardless
	 * of this mode, by address. */
	esp_shared_entry = proc_create("esp-shared", 0644, NULL,
				       &esp_shared_proc_ops);
	if (!esp_shared_entry)
		return -ENOMEM;

	pr_info("esp_shared: 0x%08lx..0x%08lx exposed at /proc/esp-shared "
		"(frames 0x%lx..0x%lx write-protected, ROM at 0x%lx)\n",
		SHM_BASE, SHM_BASE + SHM_SIZE,
		SHM_FRAME0, SHM_FRAME1 + SHM_FRAME_STRIDE, SHM_ROM);
	return 0;
}

static void __exit esp_shared_exit(void)
{
	proc_remove(esp_shared_entry);
}

device_initcall(esp_shared_init);
module_exit(esp_shared_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("The PSRAM window shared between core 0 and core 1");
