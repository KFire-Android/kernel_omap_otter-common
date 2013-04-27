
/*
 * linux/sound/soc/codecs/AIC31XX_tiload.c
 *
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 *
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * History:
 *
 * Rev 0.1	Tiload support		TI	26-07-2012
 *
 *	The Tiload programming support is added to AIC31XX.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/control.h>
#include <sound/jack.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/mfd/tlv320aic3xxx-core.h>
#include "tlv320aic31xx.h"
#include "aic31xx_tiload.h"

/* enable debug prints in the driver */
/* #define DEBUG */
#undef DEBUG

#ifdef DEBUG
#define dprintk(x...)	printk(x)
#else
#define dprintk(x...)
#endif

#ifdef AIC31XX_TiLoad

/* Function prototypes */
#ifdef REG_DUMP_aic31xx
static void aic31xx_dump_page(struct i2c_client *i2c, u8 page);
#endif

/************** Dynamic aic31xx driver, TI LOAD support  ***************/

static struct cdev *aic31xx_cdev;
static aic31xx_reg_union aic_reg;
static int aic31xx_major;	/* Dynamic allocation of Mjr No. */
static int aic31xx_opened;	/* Dynamic allocation of Mjr No. */
static struct snd_soc_codec *aic31xx_codec;
struct class *tiload_class;
static unsigned int magic_num = 0xE0;

/******************************** Debug section *****************************/

#ifdef REG_DUMP_aic31xx
/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_dump_page
 * Purpose  : Read and display one codec register page, for debugging purpose
 *----------------------------------------------------------------------------
 */
static void aic31xx_dump_page(struct i2c_client *i2c, u8 page)
{
	int i;
	u8 data;
	u8 test_page_array[8];

	dprintk("TiLoad DRIVER : %s\n", __func__);
/*	aic31xx_change_page(codec, page); */

	data = 0x0;

	i2c_master_send(i2c, data, 1);
	i2c_master_recv(i2c, test_page_array, 8);

	printk(KERN_INFO"\n------- aic31xx PAGE %d DUMP --------\n", page);
	for (i = 0; i < 8; i++)
		printk(KERN_INFO"[ %d ] = 0x%x\n", i, test_page_array[i]);

}
#endif

/*
 *----------------------------------------------------------------------------
 * Function : tiload_open
 *
 * Purpose  : open method for aic31xx-tiload programming interface
 *----------------------------------------------------------------------------
 */
static int tiload_open(struct inode *in, struct file *filp)
{
	dprintk("TiLoad DRIVER : %s\n", __func__);
	if (aic31xx_opened) {
		printk(KERN_INFO"%s device is already opened\n", "aic31xx");
		printk(KERN_INFO"%s: only one instance of driver is allowed\n",
			"aic31xx");
		return -1;
	}
	aic31xx_opened++;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_release
 *
 * Purpose  : close method for aic31xx_tilaod programming interface
 *----------------------------------------------------------------------------
 */
static int tiload_release(struct inode *in, struct file *filp)
{
	dprintk("TiLoad DRIVER : %s\n", __func__);
	aic31xx_opened--;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_read
 *
 * Purpose  : read method for mini dsp programming interface
 *----------------------------------------------------------------------------
 */
static ssize_t tiload_read(struct file *file, char __user *buf,
			size_t count, loff_t *offset)
{
	static char rd_data[128];
	char reg_addr;
	size_t size;
	#ifdef DEBUG
	int i;
	#endif
	struct aic3xxx *control = aic31xx_codec->control_data;

	dprintk("TiLoad DRIVER : %s\n", __func__);
	if (count > 128) {
		printk(KERN_INFO"Max 128 bytes can be read\n");
		count = 128;
	}

	/* copy register address from user space  */
	size = copy_from_user(&reg_addr, buf, 1);
	if (size != 0) {
		printk(KERN_INFO"read: copy_from_user failure\n");
		return -1;
	}
	/* Send the address to device thats is to be read */

	aic_reg.aic3xxx_register.offset = reg_addr;
	size = aic3xxx_bulk_read(control, aic_reg.aic3xxx_register_int,
				count, rd_data);
/*
	if (i2c_master_send(i2c, &reg_addr, 1) != 1) {
		dprintk("Can not write register address\n");
		return -1;
	}
	size = i2c_master_recv(i2c, rd_data, count);
*/
#ifdef DEBUG
	printk(KERN_ERR"read size = %d, reg_addr= %x , count = %d\n",
			(int)size, reg_addr, (int)count);
	for (i = 0; i < (int)size; i++)
		printk(KERN_ERR"rd_data[%d] = %x\n", i, rd_data[i]);

#endif
	if (size != count)
		printk(KERN_INFO"read %d registers from the codec\n", size);


	if (copy_to_user(buf, rd_data, size) != 0) {
		dprintk("copy_to_user failed\n");
		return -1;
	}

	return size;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_write
 *
 * Purpose  : write method for aic31xx_tiload programming interface
 *----------------------------------------------------------------------------
 */
static ssize_t tiload_write(struct file *file, const char __user *buf,
			size_t count, loff_t *offset)
{
	static char wr_data[128];
//	u8 pg_no;
//	unsigned int reg;
#ifdef DEBUG
	int i;
#endif
	struct aic3xxx *control = aic31xx_codec->control_data;

	dprintk("TiLoad DRIVER : %s\n", __func__);
	/* copy buffer from user space  */
	if (copy_from_user(wr_data, buf, count)) {
		printk(KERN_INFO"copy_from_user failure\n");
		return -1;
	}
#ifdef DEBUG
	printk(KERN_ERR "write size = %d\n", (int)count);
	for (i = 0; i < (int)count; i++)
		printk(KERN_INFO "\nwr_data[%d] = %x\n", i, wr_data[i]);

#endif
	if (wr_data[0] == 0) {
		/* change of page seen, but will only be registered*/
		aic_reg.aic3xxx_register.page = wr_data[1];
		return count;/*  trick */

	} else {
		aic_reg.aic3xxx_register.offset = wr_data[0];
		aic3xxx_bulk_write(control, aic_reg.aic3xxx_register_int,
				count - 1, &wr_data[1]);
		return count;
	}
/*	if (wr_data[0] == 0) {
		aic31xx_change_page(aic31xx_codec, wr_data[1]);
		return count;
	}
	pg_no = aic31xx_private->page_no;

	if ((wr_data[0] == 127) && (pg_no == 0)) {
		aic31xx_change_book(aic31xx_codec, wr_data[1]);
		return count;
	}
	return i2c_master_send(i2c, wr_data, count);*/

}

static long tiload_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int num = 0;
	void __user *argp = (void __user *)arg;
	if (_IOC_TYPE(cmd) != aic31xx_IOC_MAGIC)
		return -ENOTTY;

	dprintk("TiLoad DRIVER : %s\n", __func__);
	switch (cmd) {
	case aic31xx_IOMAGICNUM_GET:
		num = copy_to_user(argp, &magic_num, sizeof(int));
		break;
	case aic31xx_IOMAGICNUM_SET:
		num = copy_from_user(&magic_num, argp, sizeof(int));
		break;
	}
	return num;
}

/*********** File operations structure for aic31xx-tiload programming ********/
static const struct file_operations aic31xx_fops = {
	.owner = THIS_MODULE,
	.open = tiload_open,
	.release = tiload_release,
	.read = tiload_read,
	.write = tiload_write,
	.unlocked_ioctl = tiload_ioctl,
};

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_driver_init
 *
 * Purpose  : Register a char driver for dynamic aic31xx-tiload programming
 *----------------------------------------------------------------------------
 */
int aic31xx_driver_init(struct snd_soc_codec *codec)
{
	int result;

	dev_t dev = MKDEV(aic31xx_major, 0);
	dprintk("TiLoad DRIVER : %s\n", __func__);
	aic31xx_codec = codec;

	dprintk("allocating dynamic major number\n");

	result = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if (result < 0) {
		dprintk("cannot allocate major number %d\n", aic31xx_major);
		return result;
	}

	tiload_class = class_create(THIS_MODULE, DEVICE_NAME);
	aic31xx_major = MAJOR(dev);
	dprintk("allocated Major Number: %d\n", aic31xx_major);

	aic31xx_cdev = cdev_alloc();
	cdev_init(aic31xx_cdev, &aic31xx_fops);
	aic31xx_cdev->owner = THIS_MODULE;
	aic31xx_cdev->ops = &aic31xx_fops;

	aic_reg.aic3xxx_register.page = 0;
	aic_reg.aic3xxx_register.book = 0;

	if (cdev_add(aic31xx_cdev, dev, 1) < 0) {
		dprintk("aic31xx_driver: cdev_add failed\n");
		unregister_chrdev_region(dev, 1);
		aic31xx_cdev = NULL;
		return 1;
	}
	printk(KERN_INFO"Registered aic31xx TiLoad driver, Major number: %d\n",
		aic31xx_major);
	/* class_device_create(tiload_class, NULL, dev, NULL, DEVICE_NAME, 0);*/
	return 0;
}

#endif
