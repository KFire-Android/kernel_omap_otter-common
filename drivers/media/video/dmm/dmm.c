/*
 * dmm.c
 *
 * DMM driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>    /* struct cdev */
#include <linux/kdev_t.h>  /* MKDEV() */
#include <linux/fs.h>      /* register_chrdev_region() */
#include <linux/device.h>  /* struct class */
#include <linux/platform_device.h> /* platform_device() */
#include <linux/err.h>     /* IS_ERR() */
#include <linux/io.h> /* ioremap() */
#include <linux/errno.h>

#include "dmm.h"
#include "dmm_mem.h"

static s32 dmm_major;
static s32 dmm_minor;

void __iomem *dmm_base;

struct dmm_dev {
	struct cdev cdev;
};

static struct dmm_dev *dmm_device;
static struct class *dmmdev_class;

static struct platform_driver dmm_driver_ldm = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "dmm",
	},
	.probe = NULL,
	.shutdown = NULL,
	.remove = NULL,
};

static s32 dmm_open(struct inode *ip, struct file *filp)
{
	s32 r = -1;
	r = 0;
	return r;
}

static s32 dmm_release(struct inode *ip, struct file *filp)
{
	s32 r = -1;
	r = 0;
	return r;
}

static const struct file_operations dmm_fops = {
	.open    = dmm_open,
	.release = dmm_release,
};

static s32 __init dmm_init(void)
{
	dev_t dev  = 0;
	s32 r = -1;
	struct device *device = NULL;

	if (dmm_major) {
		dev = MKDEV(dmm_major, dmm_minor);
		r = register_chrdev_region(dev, 1, "dmm");
	} else {
		r = alloc_chrdev_region(&dev, dmm_minor, 1, "dmm");
		dmm_major = MAJOR(dev);
	}

	dmm_device = kmalloc(sizeof(struct dmm_dev), GFP_KERNEL);
	if (!dmm_device) {
		r = -ENOMEM;
		unregister_chrdev_region(dev, 1);
		printk(KERN_ERR "kmalloc():failed\n");
		goto EXIT;
	}
	memset(dmm_device, 0x0, sizeof(struct dmm_dev));
	cdev_init(&dmm_device->cdev, &dmm_fops);
	dmm_device->cdev.owner = THIS_MODULE;
	dmm_device->cdev.ops   = &dmm_fops;

	r = cdev_add(&dmm_device->cdev, dev, 1);
	if (r)
		printk(KERN_ERR "cdev_add():failed\n");

	dmmdev_class = class_create(THIS_MODULE, "dmm");

	if (IS_ERR(dmmdev_class)) {
		printk(KERN_ERR "class_create():failed\n");
		goto EXIT;
	}

	device = device_create(dmmdev_class, NULL, dev, NULL, "dmm");
	if (device == NULL)
		printk(KERN_ERR "device_create() fail\n");

	r = platform_driver_register(&dmm_driver_ldm);

	dmm_base = ioremap(DMM_BASE, DMM_SIZE);
	if (!dmm_base)
		return -ENOMEM;

	__raw_writel(0x88888888, dmm_base + DMM_PAT_VIEW__0);
	__raw_writel(0x88888888, dmm_base + DMM_PAT_VIEW__1);
	__raw_writel(0x80808080, dmm_base + DMM_PAT_VIEW_MAP__0);
	__raw_writel(0x80000000, dmm_base + DMM_PAT_VIEW_MAP_BASE);
	__raw_writel(0x88888888, dmm_base + DMM_TILER_OR__0);
	__raw_writel(0x88888888, dmm_base + DMM_TILER_OR__1);

	if (dmm_init_mem())
		return -ENOMEM;

EXIT:
	return r;
}

static void __exit dmm_exit(void)
{
	dmm_release_mem();
	iounmap(dmm_base);
	platform_driver_unregister(&dmm_driver_ldm);
	cdev_del(&dmm_device->cdev);
	kfree(dmm_device);
	device_destroy(dmmdev_class, MKDEV(dmm_major, dmm_minor));
	class_destroy(dmmdev_class);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("davidsin@ti.com");
module_init(dmm_init);
module_exit(dmm_exit);
