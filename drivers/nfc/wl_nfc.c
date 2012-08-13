/*
 *  NFC Char Device Driver for Texas Instrument's Connectivity Chip.
 *
 *  Copyright (C) 2009 Texas Instruments
 *  Written by Ilan Elias (ilane@ti.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define pr_fmt(fmt)    "(nfcdrv) %s: " fmt, __func__
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/ti_wilink_st.h>

/* Macros for defining NFC dev name and read register time out */
#define NFC_DRV_DEVICE_NAME			"nfc"
#define NFC_DRV_NUM_DEVICES			1
#define NFC_DRV_READ_TIMEOUT		10000	/* 10 sec*/
#define NFC_DRV_REGISTER_TIMEOUT	8000	/* 8 sec */
#define NFC_DRV_PKT_HDR_SIZE		4
#define NFC_DRV_PKT_HDR_CHNL		12
#define NFC_DRV_PKT_HDR_TYPE		7

struct nfc_drv_st_hdr {
	u8 chnl;
	u8 opcode;
	u16 len;
} __packed__;

/* allocated in nfc_drv_init */
struct nfc_dev {
	atomic_t is_available;		/* for single openness : set the atomic
					variable to 1 in the probe and in open
					if it is already open dec_test will fail
					do not let the device open again, else
					decr is_available and allow device to be
					opened. In release incr the is_available
					variable as open has decr is_available
					variable */
	int major;
	int minor;
	struct class *cls;
	struct cdev cdev;		/* char device structure */
};

/* allocated in nfc_drv_open */
struct nfc_dev_data {
	struct completion st_register_completed;	/* async st_register */
	char st_register_cb_status;		/* result of st_register */

	struct sk_buff_head rx_q;		/* rx queue */
	wait_queue_head_t wait_rx_q;		/* wait queue for rx */

	struct st_proto_s st_proto;		/* ST registration info */
};

static struct nfc_dev *nfc_device;

/* Note : called in IRQ context */
static long nfc_drv_st_recv(void *arg, struct sk_buff *skb)
{
	struct nfc_dev_data *dev = (struct nfc_dev_data *)arg;

	/* strip the ST header (apart for the channel byte,
	   which is not received in the hdr) */
	skb_pull(skb, (NFC_DRV_PKT_HDR_SIZE-1));

	/* append the new packet to the rx q */
	skb_queue_tail(&dev->rx_q, skb);

	/* wake up the user space thread (blocked on read) */
	wake_up_interruptible(&dev->wait_rx_q);

	return 0;
}

/* Note: called in IRQ context */
static void nfc_drv_st_reg_complete_cb(void *arg, char data)
{
	struct nfc_dev_data *dev = (struct nfc_dev_data *)arg;

	dev->st_register_cb_status = data;	/* ST registration status */
	complete_all(&dev->st_register_completed);
}

static int nfc_drv_open(struct inode *inode, struct file *filp)
{
	struct nfc_dev_data *dev;
	int ret;
	unsigned long comp_ret;

	pr_debug("Start");

	/* we permit a device to be opened by only one process
	at a time (single openness) */
	if (unlikely(!atomic_dec_and_test(&nfc_device->is_available))) {
		pr_err("already open");
		ret = -EBUSY;
		goto release_exit;
	}

	dev = kzalloc(sizeof(struct nfc_dev_data), GFP_KERNEL);
	if (unlikely(!dev)) {
		pr_err("unable to allocate nfc_dev_data");
		ret = -ENOMEM;
		goto release_exit;
	}

	/* init rx q and wait rx q */
	skb_queue_head_init(&dev->rx_q);
	init_waitqueue_head(&dev->wait_rx_q);

	/* init the ST registration params and callbacks */
	dev->st_proto.chnl_id = NFC_DRV_PKT_HDR_CHNL;
	dev->st_proto.max_frame_size = 300;
	dev->st_proto.hdr_len = 3;
	dev->st_proto.offset_len_in_hdr = 1;
	dev->st_proto.len_size = 2;
	dev->st_proto.reserve = 0;
	dev->st_proto.recv = nfc_drv_st_recv;
	dev->st_proto.reg_complete_cb = nfc_drv_st_reg_complete_cb;
	dev->st_proto.write = 0;	/* will be filled in st_register */
	dev->st_proto.priv_data = dev;	/* store 'dev' for later usage */

	init_completion(&dev->st_register_completed);

	/* register NFC with ST */
	ret = st_register(&dev->st_proto);
	pr_debug("st_register returned %d", ret);

	if (unlikely(ret < 0)) {
		if (ret == -EINPROGRESS) {
			comp_ret = wait_for_completion_timeout(
				&dev->st_register_completed,
				msecs_to_jiffies(NFC_DRV_REGISTER_TIMEOUT)
				);
			pr_debug("wait_for_completion_timeout returned %ld",
					comp_ret);
			if (comp_ret == 0) {
				/* timeout */
				ret = -ETIMEDOUT;
				goto free_exit;
			} else if (dev->st_register_cb_status != 0) {
				ret = -EAGAIN;
				pr_err("st_register_cb failed %d", ret);
				goto free_exit;
			}
		} else {
			goto free_exit;
		}
	}

	/* st_register must fill the write callback */
	BUG_ON(dev->st_proto.write == 0);

	/* store 'dev' for later usage */
	filp->private_data = dev;

	ret = nonseekable_open(inode, filp);

	goto exit;

free_exit:
	st_unregister(&dev->st_proto);
	kfree(dev);

release_exit:
	atomic_inc(&nfc_device->is_available); /* release the device */

exit:
	return ret;
}

static int nfc_drv_release(struct inode *inode, struct file *filp)
{
	struct nfc_dev_data *dev = (struct nfc_dev_data *)filp->private_data;
	long unreg_ret;

	pr_debug("Start");

	unreg_ret = st_unregister(&dev->st_proto);
	pr_debug("st_unregister returned %ld", unreg_ret);

	/* delete all buffers on the rx queue */
	skb_queue_purge(&dev->rx_q);

	filp->private_data = 0;

	kfree(dev);

	atomic_inc(&nfc_device->is_available); /* release the device */

	return 0;
}

static ssize_t nfc_drv_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	struct nfc_dev_data *dev = (struct nfc_dev_data *)filp->private_data;
	struct sk_buff *skb;
	struct nfc_drv_st_hdr hdr = {
			 NFC_DRV_PKT_HDR_CHNL, NFC_DRV_PKT_HDR_TYPE, 0x0000
			 };
	ssize_t ret = count;
	long write_ret;

	pr_debug("Start");
	/* allocate more bytes for ST header */
	skb = alloc_skb(count + NFC_DRV_PKT_HDR_SIZE, GFP_KERNEL);
	if (unlikely(!skb)) {
		pr_err("unable to allocate skb");
		ret = -ENOMEM;
		goto exit;
	}

	/* put the ST header */
	hdr.len = count;
	memcpy(skb_put(skb, NFC_DRV_PKT_HDR_SIZE), &hdr, NFC_DRV_PKT_HDR_SIZE);

	/* copy the data from user space to kernel space */
	if (unlikely(copy_from_user(skb_put(skb, count), buf, count))) {
		pr_err("cannot copy tx from user space to kernel space");
		ret = -EFAULT;
		goto free_exit;
	}

	/* forward the skb to the ST */
	BUG_ON(dev->st_proto.write == 0);
	write_ret = dev->st_proto.write(skb);
	pr_debug("st_write returned %ld", write_ret);
	if (unlikely(write_ret < 0)) {
		pr_err("st_write failed");
		ret = write_ret;
		goto free_exit;
	}

	goto exit;

free_exit:
	kfree_skb(skb);

exit:
	pr_debug("nfc_drv_write returning %d", ret);
	return ret;
}

static ssize_t nfc_drv_read(struct file *filp, char __user *buf,
			size_t count, loff_t *f_pos)
{
	struct nfc_dev_data *dev = (struct nfc_dev_data *)filp->private_data;
	struct sk_buff *skb;
	ssize_t ret;
	long wait_ret;

	pr_debug("Start");

	/* if the user doesn't want to block and the rx q is empty => exit */
	if (unlikely((filp->f_flags & O_NONBLOCK) &&
		(skb_queue_empty(&dev->rx_q)))) {
		ret = -EAGAIN;
		goto exit;
	}

	/* block the user space thread until timeout or rx */
	wait_ret = wait_event_interruptible_timeout(dev->wait_rx_q,
					!skb_queue_empty(&dev->rx_q),
					msecs_to_jiffies(NFC_DRV_READ_TIMEOUT)
					);
	pr_debug("wait_event_interruptible_timeout returned %ld", wait_ret);
	if (unlikely(wait_ret == 0)) {
		ret = -ETIMEDOUT;
		goto exit;
	} else if (unlikely(wait_ret < 0)) {
		ret = -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		goto exit;
	}

	/* try to extract a new rx pkt */
	skb = skb_dequeue(&dev->rx_q);
	if (unlikely(!skb)) {
		/* another thread could have taken the rx pkt => try again */
		ret = -EAGAIN;
		goto exit;
	}

	pr_debug("recv new rx skb, count %d, skb->len %d", count, skb->len);

	if (likely(skb->len <= count))
		ret = skb->len;
	else
		ret = count;

	/* copy the data from kernel space to user space */
	if (unlikely(copy_to_user(buf, skb->data, ret))) {
		pr_err("cannot copy rx from kernel space to user space");

		/* put back the skb in the head of the rx q */
		skb_queue_head(&dev->rx_q, skb);

		ret = -EFAULT;
		goto exit;
	}

	/* remove the already copied data from the start of the pkt */
	skb_pull(skb, ret);

	if (unlikely(skb->len > 0)) {
		/* more data still in skb => put back the
		skb in the head of the rx q */
		skb_queue_head(&dev->rx_q, skb);
		goto exit;
	}

	kfree_skb(skb);

exit:
	pr_debug("nfc_drv_read returning %d", ret);
	return ret;
}

static unsigned int nfc_drv_poll(struct file *filp, poll_table *wait)
{
	struct nfc_dev_data *dev = (struct nfc_dev_data *)filp->private_data;
	unsigned int mask = (POLLOUT | POLLWRNORM); /* we're always writable */

	pr_debug("Start");

	poll_wait(filp, &dev->wait_rx_q, wait);

	if (!skb_queue_empty(&dev->rx_q))
		mask |= (POLLIN | POLLRDNORM);	/* we're also readable */

	return mask;
}

static const struct file_operations nfc_drv_ops = {
	.owner = THIS_MODULE,
	.open = nfc_drv_open,
	.release = nfc_drv_release,
	.write = nfc_drv_write,
	.read = nfc_drv_read,
	.poll = nfc_drv_poll,
	.llseek = no_llseek
};

/**
 *	Initializes the nfc driver parameters and exposes /dev/nfc node to
 *	user space.
 */
static int nfc_drv_probe(struct platform_device *pdev)
{
	int ret;
	dev_t dev;
	struct device *sysfs_dev;

	pr_debug("Start");

	nfc_device = kzalloc(sizeof(struct nfc_dev), GFP_KERNEL);
	if (unlikely(!nfc_device)) {
		pr_err("unable to allocate nfc_dev");
		ret = -ENOMEM;
		goto exit;
	}

	/* init the counter for single openness */
	atomic_set(&nfc_device->is_available, 1);

	/* create the char device and allocate major */
	ret = alloc_chrdev_region(&dev, 0, NFC_DRV_NUM_DEVICES,
					NFC_DRV_DEVICE_NAME);
	if (unlikely(ret < 0)) {
		pr_err("alloc_chrdev_region failed");
		goto free_exit;
	}

	nfc_device->major = MAJOR(dev);
	nfc_device->minor = MINOR(dev);

	cdev_init(&nfc_device->cdev, &nfc_drv_ops);
	nfc_device->cdev.owner = THIS_MODULE;
	ret = cdev_add(&nfc_device->cdev, dev, NFC_DRV_NUM_DEVICES);
	if (unlikely(ret < 0)) {
		pr_err("cdev_add failed");
		goto unreg_exit;
	}

	/* add the device to sysfs */
	nfc_device->cls = class_create(THIS_MODULE, NFC_DRV_DEVICE_NAME);
	if (unlikely(IS_ERR(nfc_device->cls))) {
		pr_err("class_create failed");
		ret = PTR_ERR(nfc_device->cls);
		goto cdev_exit;
	}

	sysfs_dev = device_create(nfc_device->cls, NULL, dev, NULL,
					NFC_DRV_DEVICE_NAME);
	if (unlikely(IS_ERR(sysfs_dev))) {
		pr_err("device_create failed");
		ret = PTR_ERR(sysfs_dev);
		goto class_exit;
	}

	pr_info("allocated major %d, minor %d", nfc_device->major,
					nfc_device->minor);

	goto exit;

class_exit:
	class_destroy(nfc_device->cls);

cdev_exit:
	cdev_del(&nfc_device->cdev);

unreg_exit:
	unregister_chrdev_region(dev, NFC_DRV_NUM_DEVICES);

free_exit:
	kfree(nfc_device);
	nfc_device = 0;

exit:
	return ret;
}

/**
 *	De-initializes the nfc driver and removes /dev/nfc node from user space.
 */
static int nfc_drv_remove(struct platform_device *pdev)
{
	dev_t dev = MKDEV(nfc_device->major, nfc_device->minor);

	pr_debug("Start");

	/* remove the device from sysfs */
	device_destroy(nfc_device->cls, dev);
	class_destroy(nfc_device->cls);

	/* remove the char device */
	cdev_del(&nfc_device->cdev);
	unregister_chrdev_region(dev, NFC_DRV_NUM_DEVICES);

	kfree(nfc_device);
	nfc_device = 0;
	return 0;
}

static struct platform_driver nfc_driver = {
	.probe = nfc_drv_probe,
	.remove = nfc_drv_remove,
	.driver = {
		.name = "nfcwilink",
		.owner = THIS_MODULE,
	},
};

static int __init nfc_drv_init(void)
{
	printk(KERN_INFO "NFC Driver for TI WiLink");
	return platform_driver_register(&nfc_driver);
}

/**
 *     De-initializes the nfc driver and removes /dev/nfc node from user space.
 */
static void __exit nfc_drv_exit(void)
{
	platform_driver_unregister(&nfc_driver);
}

module_init(nfc_drv_init);
module_exit(nfc_drv_exit);
MODULE_AUTHOR("Ilan Elias <ilane@ti.com>");
MODULE_DESCRIPTION("NFC Char Device Driver for Texas Instrument's Connectivity Chip");
MODULE_LICENSE("GPL");
