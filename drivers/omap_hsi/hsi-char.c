/*
 * hsi-char.c
 *
 * HSI character device driver, implements the character device
 * interface.
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Author: Andras Domokos <andras.domokos@nokia.com>
 * Author: Sebastien JAN <s-jan@ti.com>
 * Author: Djamil ELAIDI <d-elaidi@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <asm/mach-types.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/hsi_driver_if.h>
#include <linux/hsi_char.h>

#include <plat/omap_hsi.h>

#include "hsi-char.h"

#define DRIVER_VERSION  "0.3"
#define HSI_CHAR_DEVICE_NAME  "hsi_char"

static unsigned int port = 2;
module_param(port, uint, S_IRUGO);
MODULE_PARM_DESC(port, "HSI port to be probed");

static unsigned int num_channels = 1;
static unsigned int channels_map[HSI_MAX_CHAR_DEVS] = { 1 };
module_param_array(channels_map, uint, &num_channels, S_IRUGO);
MODULE_PARM_DESC(channels_map, "HSI channels to be probed");

struct char_queue {
	struct list_head list;
	u32 *data;
	unsigned int count;
};

struct hsi_char {
	unsigned int opened;
	int poll_event;
	struct list_head rx_queue;
	struct list_head tx_queue;
	spinlock_t lock;	/* Serialize access to driver data and API */
	struct fasync_struct *async_queue;
	wait_queue_head_t rx_wait;
	wait_queue_head_t tx_wait;
	wait_queue_head_t poll_wait;
};

struct hsi_char_priv {
	struct class *class;	/* HSI char class during class_create */
	dev_t dev;		/* Device region's dev_t */
	struct cdev cdev;	/* character device entry */
	char dev_name[sizeof(HSI_CHAR_DEVICE_NAME) + 1];
};

static struct hsi_char hsi_char_data[HSI_MAX_CHAR_DEVS];

void if_hsi_notify(int ch, struct hsi_event *ev)
{
	struct char_queue *entry;

	pr_debug("%s, ev = {0x%x, 0x%p, %u}\n", __func__, ev->event, ev->data,
		 ev->count);

	spin_lock(&hsi_char_data[ch].lock);

	if (!hsi_char_data[ch].opened) {
		pr_debug("%s, device not opened\n!", __func__);
		spin_unlock(&hsi_char_data[ch].lock);
		return;
	}

	switch (HSI_EV_TYPE(ev->event)) {
	case HSI_EV_IN:
		entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
		if (!entry) {
			pr_err("HSI-CHAR: entry allocation failed.\n");
			spin_unlock(&hsi_char_data[ch].lock);
			return;
		}
		entry->data = ev->data;
		entry->count = ev->count;
		list_add_tail(&entry->list, &hsi_char_data[ch].rx_queue);
		spin_unlock(&hsi_char_data[ch].lock);
		pr_debug("%s, HSI_EV_IN\n", __func__);
		wake_up_interruptible(&hsi_char_data[ch].rx_wait);
		break;
	case HSI_EV_OUT:
		entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
		if (!entry) {
			pr_err("HSI-CHAR: entry allocation failed.\n");
			spin_unlock(&hsi_char_data[ch].lock);
			return;
		}
		entry->data = ev->data;
		entry->count = ev->count;
		hsi_char_data[ch].poll_event |= (POLLOUT | POLLWRNORM);
		list_add_tail(&entry->list, &hsi_char_data[ch].tx_queue);
		spin_unlock(&hsi_char_data[ch].lock);
		pr_debug("%s, HSI_EV_OUT\n", __func__);
		wake_up_interruptible(&hsi_char_data[ch].tx_wait);
		break;
	case HSI_EV_EXCEP:
		hsi_char_data[ch].poll_event |= POLLPRI;
		spin_unlock(&hsi_char_data[ch].lock);
		pr_debug("%s, HSI_EV_EXCEP\n", __func__);
		wake_up_interruptible(&hsi_char_data[ch].poll_wait);
		break;
	case HSI_EV_AVAIL:
		hsi_char_data[ch].poll_event |= (POLLIN | POLLRDNORM);
		spin_unlock(&hsi_char_data[ch].lock);
		pr_debug("%s, HSI_EV_AVAIL\n", __func__);
		wake_up_interruptible(&hsi_char_data[ch].poll_wait);
		break;
	default:
		spin_unlock(&hsi_char_data[ch].lock);
		break;
	}
}

static int hsi_char_fasync(int fd, struct file *file, int on)
{
	int ch = (int)file->private_data;
	if (fasync_helper(fd, file, on, &hsi_char_data[ch].async_queue) >= 0)
		return 0;
	else
		return -EIO;
}

static unsigned int hsi_char_poll(struct file *file, poll_table *wait)
{
	int ch = (int)file->private_data;
	unsigned int ret = 0;

	/*printk(KERN_DEBUG "%s\n", __func__); */

	poll_wait(file, &hsi_char_data[ch].poll_wait, wait);
	poll_wait(file, &hsi_char_data[ch].tx_wait, wait);
	spin_lock_bh(&hsi_char_data[ch].lock);
	ret = hsi_char_data[ch].poll_event;
	spin_unlock_bh(&hsi_char_data[ch].lock);

	pr_debug("%s, ret = 0x%x\n", __func__, ret);
	return ret;
}

static ssize_t hsi_char_read(struct file *file, char __user *buf,
			     size_t count, loff_t *ppos)
{
	int ch = (int)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	u32 *data;
	unsigned int data_len;
	struct char_queue *entry;
	ssize_t ret;

	/*printk(KERN_DEBUG "%s, count = %d\n", __func__, count); */

	/* only 32bit data is supported for now */
	if ((count < 4) || (count & 3))
		return -EINVAL;

	data = kmalloc(count, GFP_ATOMIC);

	ret = if_hsi_read(ch, data, count);
	if (ret < 0) {
		kfree(data);
		goto out2;
	}

	spin_lock_bh(&hsi_char_data[ch].lock);
	add_wait_queue(&hsi_char_data[ch].rx_wait, &wait);
	spin_unlock_bh(&hsi_char_data[ch].lock);

	for (;;) {
		data = NULL;
		data_len = 0;

		set_current_state(TASK_INTERRUPTIBLE);

		spin_lock_bh(&hsi_char_data[ch].lock);
		if (!list_empty(&hsi_char_data[ch].rx_queue)) {
			entry = list_entry(hsi_char_data[ch].rx_queue.next,
					   struct char_queue, list);
			data = entry->data;
			data_len = entry->count;
			list_del(&entry->list);
			kfree(entry);
		}
		spin_unlock_bh(&hsi_char_data[ch].lock);

		pr_debug("%s, data = 0x%p, data_len = %d\n",
			 __func__, data, data_len);

		if (data_len) {
			pr_debug("%s, RX finished\n", __func__);
			spin_lock_bh(&hsi_char_data[ch].lock);
			hsi_char_data[ch].poll_event &= ~(POLLIN | POLLRDNORM);
			spin_unlock_bh(&hsi_char_data[ch].lock);
			if_hsi_poll(ch);
			break;
		} else if (file->f_flags & O_NONBLOCK) {
			pr_debug("%s, O_NONBLOCK\n", __func__);
			ret = -EAGAIN;
			goto out;
		} else if (signal_pending(current)) {
			pr_debug("%s, ERESTARTSYS\n", __func__);
			ret = -EAGAIN;
			if_hsi_cancel_read(ch);
			/* goto out; */
			break;
		}

		/*printk(KERN_DEBUG "%s, going to sleep...\n", __func__); */
		schedule();
		/*printk(KERN_DEBUG "%s, woke up\n", __func__); */
	}

	if (data_len) {
		ret = copy_to_user((void __user *)buf, data, data_len);
		if (!ret)
			ret = data_len;
	}

	kfree(data);

out:
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&hsi_char_data[ch].rx_wait, &wait);

out2:
	/*printk(KERN_DEBUG "%s, ret = %d\n", __func__, ret); */
	return ret;
}

static ssize_t hsi_char_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	int ch = (int)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	u32 *data;
	unsigned int data_len = 0;
	struct char_queue *entry;
	ssize_t ret;

	/*printk(KERN_DEBUG "%s, count = %d\n", __func__, count); */

	/* only 32bit data is supported for now */
	if ((count < 4) || (count & 3))
		return -EINVAL;

	data = kmalloc(count, GFP_ATOMIC);
	if (!data) {
		WARN_ON(1);
		return -ENOMEM;
	}
	if (copy_from_user(data, (void __user *)buf, count)) {
		ret = -EFAULT;
		kfree(data);
		goto out2;
	} else {
		ret = count;
	}

	/* Write callback can occur very fast in response to hsi_write()    */
	/* and may cause a race condition between hsi_char_write() and      */
	/* if_hsi_notify(). This can cause the poll_event flag to be set by */
	/* if_hsi_notify() then cleared by hsi_char_write(), remaining to 0 */
	/* forever. Clear the flag ahead of hsi_write to avoid this.        */
	spin_lock_bh(&hsi_char_data[ch].lock);
	hsi_char_data[ch].poll_event &= ~(POLLOUT | POLLWRNORM);
	add_wait_queue(&hsi_char_data[ch].tx_wait, &wait);
	spin_unlock_bh(&hsi_char_data[ch].lock);

	ret = if_hsi_write(ch, data, count);
	if (ret < 0) {
		kfree(data);
		spin_lock_bh(&hsi_char_data[ch].lock);
		hsi_char_data[ch].poll_event |= (POLLOUT | POLLWRNORM);
		spin_unlock_bh(&hsi_char_data[ch].lock);
		remove_wait_queue(&hsi_char_data[ch].tx_wait, &wait);
		goto out2;
	}

	for (;;) {
		data = NULL;
		data_len = 0;

		set_current_state(TASK_INTERRUPTIBLE);

		spin_lock_bh(&hsi_char_data[ch].lock);
		if (!list_empty(&hsi_char_data[ch].tx_queue)) {
			entry = list_entry(hsi_char_data[ch].tx_queue.next,
					   struct char_queue, list);
			data = entry->data;
			data_len = entry->count;
			list_del(&entry->list);
			kfree(entry);
		}
		spin_unlock_bh(&hsi_char_data[ch].lock);

		if (data_len) {
			pr_debug("%s, TX finished\n", __func__);
			ret = data_len;
			break;
		} else if (file->f_flags & O_NONBLOCK) {
			pr_debug("%s, O_NONBLOCK\n", __func__);
			ret = -EAGAIN;
			goto out;
		} else if (signal_pending(current)) {
			pr_debug("%s, ERESTARTSYS\n", __func__);
			ret = -ERESTARTSYS;
			goto out;
		}

		/*printk(KERN_DEBUG "%s, going to sleep...\n", __func__); */
		schedule();
		/*printk(KERN_DEBUG "%s, woke up\n", __func__); */
	}

	kfree(data);

out:
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&hsi_char_data[ch].tx_wait, &wait);

out2:
	/*printk(KERN_DEBUG "%s, ret = %d\n", __func__, ret); */
	return ret;
}

static long  hsi_char_ioctl(struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	int ch = (int)file->private_data;
	unsigned int state;
	size_t size;
	unsigned long fclock;
	struct hsi_rx_config rx_cfg;
	struct hsi_tx_config tx_cfg;
	int ret = 0;

	pr_debug("%s, ch = %d, cmd = 0x%08x\n", __func__, ch, cmd);

	switch (cmd) {
	case CS_SEND_BREAK:
		if_hsi_send_break(ch);
		break;
	case CS_FLUSH_RX:
		if_hsi_flush_rx(ch, &size);
		if (copy_to_user((void __user *)arg, &size, sizeof(size)))
			ret = -EFAULT;
		break;
	case CS_FLUSH_TX:
		if_hsi_flush_tx(ch, &size);
		if (copy_to_user((void __user *)arg, &size, sizeof(size)))
			ret = -EFAULT;
		break;
	case CS_SET_ACWAKELINE:
		if (copy_from_user(&state, (void __user *)arg, sizeof(state)))
			ret = -EFAULT;
		else
			if_hsi_set_acwakeline(ch, state);
		break;
	case CS_GET_ACWAKELINE:
		if_hsi_get_acwakeline(ch, &state);
		if (copy_to_user((void __user *)arg, &state, sizeof(state)))
			ret = -EFAULT;
		break;
	case CS_SET_WAKE_RX_3WIRES_MODE:
		if (copy_from_user(&state, (void __user *)arg, sizeof(state)))
			ret = -EFAULT;
		else
			if_hsi_set_wake_rx_3wires_mode(ch, state);
		break;
	case CS_GET_CAWAKELINE:
		if_hsi_get_cawakeline(ch, &state);
		if (copy_to_user((void __user *)arg, &state, sizeof(state)))
			ret = -EFAULT;
		break;
	case CS_SET_RX:
		if (copy_from_user(&rx_cfg, (void __user *)arg, sizeof(rx_cfg)))
			ret = -EFAULT;
		else
			ret = if_hsi_set_rx(ch, &rx_cfg);
		break;
	case CS_GET_RX:
		if_hsi_get_rx(ch, &rx_cfg);
		if (copy_to_user((void __user *)arg, &rx_cfg, sizeof(rx_cfg)))
			ret = -EFAULT;
		break;
	case CS_SET_TX:
		if (copy_from_user(&tx_cfg, (void __user *)arg, sizeof(tx_cfg)))
			ret = -EFAULT;
		else
			ret = if_hsi_set_tx(ch, &tx_cfg);
		break;
	case CS_GET_TX:
		if_hsi_get_tx(ch, &tx_cfg);
		if (copy_to_user((void __user *)arg, &tx_cfg, sizeof(tx_cfg)))
			ret = -EFAULT;
		break;
	case CS_SW_RESET:
		if_hsi_sw_reset(ch);
		break;
	case CS_GET_FIFO_OCCUPANCY:
		if_hsi_get_fifo_occupancy(ch, &size);
		if (copy_to_user((void __user *)arg, &size, sizeof(size)))
			ret = -EFAULT;
		break;
	case CS_SET_HI_SPEED:
		if (copy_from_user(&state, (void __user *)arg, sizeof(state)))
			ret = -EFAULT;
		else
			if_hsi_set_hi_speed(ch, state);
		break;
	case CS_GET_SPEED:
		if_hsi_get_speed(ch, &fclock);
		if (copy_to_user((void __user *)arg, &fclock, sizeof(fclock)))
			ret = -EFAULT;
		break;
	case CS_SET_CLK_FORCE_ON:
		if_hsi_set_clk_forced_on(ch);
		break;
	case CS_SET_CLK_DYNAMIC:
		if_hsi_set_clk_dynamic(ch);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static int hsi_char_open(struct inode *inode, struct file *file)
{
	int ret = 0, ch = iminor(inode);
	int i;

	for (i = 0; i < HSI_MAX_CHAR_DEVS; i++)
		if ((channels_map[i] - 1) == ch)
			break;

	if (i == HSI_MAX_CHAR_DEVS) {
		pr_err("HSI char open: Channel %d not found\n", ch);
		return -ENODEV;
	}

	pr_debug("HSI char open: opening channel %d\n", ch);

	spin_lock_bh(&hsi_char_data[ch].lock);

	if (hsi_char_data[ch].opened) {
		spin_unlock_bh(&hsi_char_data[ch].lock);
		pr_err("HSI char open: Channel %d already opened\n", ch);
		return -EBUSY;
	}

	file->private_data = (void *)ch;
	hsi_char_data[ch].opened++;
	hsi_char_data[ch].poll_event = (POLLOUT | POLLWRNORM);
	spin_unlock_bh(&hsi_char_data[ch].lock);

	ret = if_hsi_start(ch);

	return ret;
}

static int hsi_char_release(struct inode *inode, struct file *file)
{
	int ch = (int)file->private_data;
	struct char_queue *entry;
	struct list_head *cursor, *next;

	pr_debug("%s, ch = %d\n", __func__, ch);

	if_hsi_stop(ch);
	spin_lock_bh(&hsi_char_data[ch].lock);
	hsi_char_data[ch].opened--;

	if (!list_empty(&hsi_char_data[ch].rx_queue)) {
		list_for_each_safe(cursor, next, &hsi_char_data[ch].rx_queue) {
			entry = list_entry(cursor, struct char_queue, list);
			list_del(&entry->list);
			kfree(entry);
		}
	}

	if (!list_empty(&hsi_char_data[ch].tx_queue)) {
		list_for_each_safe(cursor, next, &hsi_char_data[ch].tx_queue) {
			entry = list_entry(cursor, struct char_queue, list);
			list_del(&entry->list);
			kfree(entry);
		}
	}

	spin_unlock_bh(&hsi_char_data[ch].lock);

	return 0;
}

static const struct file_operations hsi_char_fops = {
	.owner = THIS_MODULE,
	.read = hsi_char_read,
	.write = hsi_char_write,
	.poll = hsi_char_poll,
	.unlocked_ioctl = hsi_char_ioctl,
	.open = hsi_char_open,
	.release = hsi_char_release,
	.fasync = hsi_char_fasync,
};

static int hsi_char_probe(struct platform_device *pdev)
{
	int i;
	unsigned ret;
	struct hsi_char_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct hsi_char_priv *priv;

	pr_info("HSI character device version " DRIVER_VERSION "\n");
	pr_info("hsi_char: %d channels mapped\n", pdata->num_channels);

	for (i = 0; i < HSI_MAX_CHAR_DEVS; i++) {
		init_waitqueue_head(&hsi_char_data[i].rx_wait);
		init_waitqueue_head(&hsi_char_data[i].tx_wait);
		init_waitqueue_head(&hsi_char_data[i].poll_wait);
		spin_lock_init(&hsi_char_data[i].lock);
		hsi_char_data[i].opened = 0;
		INIT_LIST_HEAD(&hsi_char_data[i].rx_queue);
		INIT_LIST_HEAD(&hsi_char_data[i].tx_queue);
	}

	priv = kzalloc(sizeof(struct hsi_char_priv), GFP_KERNEL);
	if (!priv) {
		pr_err("%s: Failed to allocate private data.\n", __func__);
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, priv);

	strncpy(priv->dev_name, HSI_CHAR_DEVICE_NAME,
			sizeof(priv->dev_name) - 1);

	pr_debug("%s, devname = %s\n", __func__, priv->dev_name);

	ret = if_hsi_init(pdata->port, pdata->channels_map,
			  pdata->num_channels);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Failed to init HSI core: %d.\n", __func__, ret);
		goto rollback_1;
	}

	ret = alloc_chrdev_region(&priv->dev, 0, HSI_MAX_CHAR_DEVS,
				  priv->dev_name);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Failed to register char device: %d\n", __func__,
		       ret);
		goto rollback_2;
	}
	pr_debug("%s: Allocated major device number %d", __func__,
		 MAJOR(priv->dev));

	priv->class = class_create(THIS_MODULE, priv->dev_name);
	if (IS_ERR(priv->class)) {
		ret = PTR_ERR(priv->class);
		pr_err("%s: Failed to create class: %d\n", __func__, ret);
		goto rollback_3;
	}

	cdev_init(&priv->cdev, &hsi_char_fops);
	ret = cdev_add(&priv->cdev, priv->dev, HSI_MAX_CHAR_DEVS);
	if (ret < 0) {
		pr_err("%s: Failed to add char device: %d\n", __func__, ret);
		goto rollback_4;
	}

	for (i = 0; i < HSI_MAX_CHAR_DEVS; i++) {
		pr_debug("%s: channels_map[%d] = %d\n", __func__,
			i, channels_map[i]);
		if ((int)(channels_map[i] - 1) >= 0) {
			ret = PTR_ERR(device_create(priv->class, NULL,
					MKDEV(MAJOR(priv->dev),
					channels_map[i] - 1),
					NULL, "hsi%d" ,
					channels_map[i] - 1));
			if (IS_ERR_VALUE(ret)) {
				pr_err("%s: Failed to create device hsi%d: %d\n"
					, __func__, channels_map[i] - 1, ret);
				goto rollback_5;
			}
			pr_info("hsi_char: HSI device created /dev/hsi%d\n",
				channels_map[i] - 1);
		}
	}
	return 0;

rollback_5:
	for (--i; i >= 0; --i)
		if ((int)(channels_map[i] - 1) >= 0)
			device_destroy(priv->class, MKDEV(MAJOR(priv->dev),
				       channels_map[i] - 1));
	cdev_del(&priv->cdev);
rollback_4:
	class_destroy(priv->class);
rollback_3:
	unregister_chrdev_region(priv->dev, HSI_MAX_CHAR_DEVS);
rollback_2:
	if_hsi_exit();
rollback_1:
	kfree(priv);
	return ret;
}

static int hsi_char_remove(struct platform_device *pdev)
{
	struct hsi_char_priv *priv = dev_get_drvdata(&pdev->dev);
	int i;

	for (i = 0; i < HSI_MAX_CHAR_DEVS; i++)
		if ((int)(channels_map[i] - 1) >= 0)
			device_destroy(priv->class, MKDEV(MAJOR(priv->dev),
				channels_map[i] - 1));
	cdev_del(&priv->cdev);
	class_destroy(priv->class);
	unregister_chrdev_region(priv->dev, HSI_MAX_CHAR_DEVS);
	if_hsi_exit();
	kfree(priv);
	return 0;
}

static struct platform_driver hsi_char_plat_drv = {
	.probe = hsi_char_probe,
	.remove = hsi_char_remove,
	.driver = {
		.name = "hsi_char",
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_OMAP_HSI_CHAR_MODULE
static void hsi_char_plat_dev_release(struct device *dev)
{
	/* Dummy function to fix the warning: "Device 'hsi_char.0' does not */
	/* have a release() function, it is broken and must be fixed." */
	return;
}

static struct hsi_char_platform_data hsi_char_platform_data;
static struct platform_device hsi_char_plat_dev = {
	.name = "hsi_char",
	.dev = {
		.platform_data = &hsi_char_platform_data,
		.release = hsi_char_plat_dev_release,
	}
};
#endif /* CONFIG_OMAP_HSI_CHAR_MODULE */

static int __init hsi_char_init(void)
{
	int result;

	result = platform_driver_register(&hsi_char_plat_drv);
	if (result) {
		pr_err("%s: Could not register hsi_char platform driver: %d.\n",
		       __func__, result);
		goto err;
	}
	pr_info("hsi_char: platform driver registered\n");
#ifdef CONFIG_OMAP_HSI_CHAR_MODULE
	hsi_char_platform_data.port = port;
	hsi_char_platform_data.num_channels = num_channels;
	memcpy(hsi_char_platform_data.channels_map, channels_map,
	       sizeof(channels_map));
	result = platform_device_register(&hsi_char_plat_dev);
	if (result) {
		pr_err("%s: Could not register hsi_char platform device, unregistering platform driver : %d.\n",
		       __func__, result);
		platform_driver_unregister(&hsi_char_plat_drv);
		goto err;
	}
	pr_info("hsi_char: platform device registered\n");
#endif /* CONFIG_OMAP_HSI_CHAR_MODULE */
err:
	return result;
}

static void __exit hsi_char_exit(void)
{
#ifdef CONFIG_OMAP_HSI_CHAR_MODULE
	platform_device_unregister(&hsi_char_plat_dev);
	pr_info("hsi_char: platform device unregistered\n");
#endif /* CONFIG_OMAP_HSI_CHAR_MODULE */
	platform_driver_unregister(&hsi_char_plat_drv);
	pr_info("hsi_char: platform driver unregistered\n");
}

MODULE_AUTHOR("Andras Domokos <andras.domokos@nokia.com>");
MODULE_AUTHOR("Sebastien Jan <s-jan@ti.com> / Texas Instruments");
MODULE_AUTHOR("Djamil Elaidi <d-elaidi@ti.com> / Texas Instruments");
MODULE_DESCRIPTION("HSI character device");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(hsi_char_init);
module_exit(hsi_char_exit);
