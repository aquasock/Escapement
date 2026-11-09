// SPDX-License-Identifier: GPL-2.0-only
//
// esp_log_pipe.c - /dev/esp-log: firmware diagnostics from the ESP32-S3.
//
// The USB-Serial-JTAG port carries ESP-IDF log output only until Linux claims
// it, about 1.75 s into boot. After that the firmware's own logs have nowhere to
// go, which is exactly when WiFi, the IPC link and the BLE stack start running.
//
// A second stream is needed rather than reusing /dev/esp-ble: that pipe is a
// question-and-answer dialogue between /usr/sbin/ble-wifi-setup and a phone, and
// a log line arriving mid-dialogue is consumed as an answer to the current
// prompt. Logs get their own device and their own queue.

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/skbuff.h>

#include "esp.h"
#include "utils.h"
#include "esp_api.h"

#define ESP_LOG_DEV_NAME	"esp-log"
#define ESP_LOG_MAX_QUEUE	64

static struct esp_adapter *log_adapter;
static struct sk_buff_head log_q;
static wait_queue_head_t log_wait;

/*
 * Deliberately not exclusive, unlike the provisioning pipe: reading logs must be
 * possible while something else is looking, and a log stream is safe to fan out
 * in the sense that each skb is handed to exactly one reader. Opening twice
 * simply means whichever read() runs first gets the next record.
 */
void esp_log_pipe_rx(struct sk_buff *skb)
{
	if (!skb)
		return;

	if (skb_queue_len(&log_q) >= ESP_LOG_MAX_QUEUE) {
		/* Drop the oldest: a stalled reader must not be able to stall the
		 * firmware by filling this queue. */
		dev_kfree_skb_any(skb_dequeue(&log_q));
	}

	skb_queue_tail(&log_q, skb);
	wake_up_interruptible(&log_wait);
}

static ssize_t esp_log_pipe_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct sk_buff *skb;
	size_t len;
	int ret;

	skb = skb_dequeue(&log_q);
	while (!skb) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(log_wait, !skb_queue_empty(&log_q));
		if (ret)
			return ret;

		skb = skb_dequeue(&log_q);
	}

	len = min(count, (size_t)skb->len);
	if (copy_to_user(buf, skb->data, len)) {
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	if (len < skb->len) {
		skb_pull(skb, len);
		skb_queue_head(&log_q, skb);
	} else {
		dev_kfree_skb_any(skb);
	}

	return len;
}

/* Writes are accepted and ignored: this direction has no consumer. */
static ssize_t esp_log_pipe_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	return count;
}

static __poll_t esp_log_pipe_poll(struct file *file, poll_table *wait)
{
	__poll_t mask = 0;

	poll_wait(file, &log_wait, wait);
	if (!skb_queue_empty(&log_q))
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

static const struct file_operations esp_log_pipe_fops = {
	.owner		= THIS_MODULE,
	.read		= esp_log_pipe_read,
	.write		= esp_log_pipe_write,
	.poll		= esp_log_pipe_poll,
};

static struct miscdevice esp_log_pipe_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= ESP_LOG_DEV_NAME,
	.fops	= &esp_log_pipe_fops,
	.mode	= 0600,
};

int esp_log_pipe_init(struct esp_adapter *adapter)
{
	int ret;

	log_adapter = adapter;
	skb_queue_head_init(&log_q);
	init_waitqueue_head(&log_wait);

	ret = misc_register(&esp_log_pipe_misc);
	if (ret) {
		esp_err("Failed to register /dev/%s: %d\n", ESP_LOG_DEV_NAME, ret);
		log_adapter = NULL;
		return ret;
	}

	esp_info("firmware log pipe at /dev/%s\n", ESP_LOG_DEV_NAME);
	return 0;
}

void esp_log_pipe_deinit(void)
{
	if (!log_adapter)
		return;

	misc_deregister(&esp_log_pipe_misc);
	skb_queue_purge(&log_q);
	log_adapter = NULL;
}
