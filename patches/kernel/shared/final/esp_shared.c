// SPDX-License-Identifier: GPL-2.0-only
/*
 * esp_shared.c - read-only view of the PSRAM megabyte both cores share.
 *
 * The window is 0x3df00000..0x3e000000: excluded from the DTS memory node, so
 * the kernel never allocates from it, but still mapped in the MMU table, which
 * is global to both cores. Core 0 writes into it; this is how core 1 reads it.
 *
 * This is the coherency experiment, not a device driver. Core 0 has a D-cache
 * and core 1 reportedly has none; if a counter written on core 0 arrives here
 * and moves, the shared-buffer design in docs/cores.md is sound and the
 * emulator can move to core 0.
 *
 * /dev/mem would do the same job in one line, and it is off on purpose: on
 * NOMMU there is no STRICT_DEVMEM to bound it and it would reach core 0's own
 * memory. This exposes one read-only window instead, which is the smallest
 * thing that answers the question.
 *
 *   dd if=/proc/esp-shared bs=32 count=1 2>/dev/null | od -An -tu4
 *
 * The `invalidate` parameter exists to separate two failures that look the
 * same from userspace. With it on (the default) every read drops core 1's
 * cache lines for the range first, so a moving counter proves core 0's
 * esp_cache_msync() reaches memory. With it off, a frozen counter proves core
 * 1 is serving stale cache lines of its own, which is a different problem with
 * a different fix.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <asm/cacheflush.h>

/* Both cores map this at the same address; see init_linux_mmap(). */
#define ESP_SHARED_BASE		0x3df00000UL
#define ESP_SHARED_SIZE		0x00100000UL

/* The S3's data-cache line is 32 bytes; align every operation to it. */
#define ESP_SHARED_LINE		32UL

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

	if (off >= ESP_SHARED_SIZE)
		return 0;

	if (count > ESP_SHARED_SIZE - off)
		count = ESP_SHARED_SIZE - off;

	src = (void *)(ESP_SHARED_BASE + off);

	if (invalidate) {
		lo = (unsigned long)src & ~(ESP_SHARED_LINE - 1);
		hi = ALIGN((unsigned long)src + count, ESP_SHARED_LINE);
		__invalidate_dcache_range(lo, hi - lo);
	}

	if (copy_to_user(ubuf, src, count))
		return -EFAULT;

	*ppos += count;
	return count;
}

static const struct proc_ops esp_shared_proc_ops = {
	.proc_read	= esp_shared_read,
	.proc_lseek	= default_llseek,
};

static struct proc_dir_entry *esp_shared_entry;

static int __init esp_shared_init(void)
{
	/*
	 * 0444: readable by anyone, writable by no one. This is a window into
	 * memory neither the kernel nor userspace owns, so there is nothing
	 * here to write to.
	 */
	esp_shared_entry = proc_create("esp-shared", 0444, NULL,
				       &esp_shared_proc_ops);
	if (!esp_shared_entry)
		return -ENOMEM;

	pr_info("esp_shared: 0x%08lx..0x%08lx exposed read-only at /proc/esp-shared\n",
		ESP_SHARED_BASE, ESP_SHARED_BASE + ESP_SHARED_SIZE);
	return 0;
}

static void __exit esp_shared_exit(void)
{
	proc_remove(esp_shared_entry);
}

device_initcall(esp_shared_init);
module_exit(esp_shared_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Read-only view of the PSRAM window shared with core 0");
