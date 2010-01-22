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
#include <linux/cdev.h>            /* struct cdev */
#include <linux/kdev_t.h>          /* MKDEV() */
#include <linux/fs.h>              /* register_chrdev_region() */
#include <linux/device.h>          /* struct class */
#include <linux/platform_device.h> /* platform_device() */
#include <linux/err.h>             /* IS_ERR() */
#include <linux/io.h>              /* ioremap() */
#include <linux/errno.h>

#include "dmm.h"
#include "dmm_mem.h"

#undef __DEBUG__
#define BITS_32(in_NbBits) ((((u32)1 << in_NbBits) - 1) | ((u32)1 << in_NbBits))
#define BITFIELD_32(in_UpBit, in_LowBit)\
	(BITS_32(in_UpBit) & ~((BITS_32(in_LowBit)) >> 1))
#define BF BITFIELD_32

#ifdef __DEBUG__
#define DEBUG(x, y) printk(KERN_NOTICE "%s()::%d:%s=(0x%08x)\n", \
				__func__, __LINE__, x, (s32)y);
#else
#define DEBUG(x, y)
#endif

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

s32 dmm_pat_refill(struct pat *pd, enum pat_mode mode)
{
	void __iomem *r = NULL;
	u32 v = -1, w = -1;

	/* Only manual refill supported */
	if (mode != MANUAL)
		return -EFAULT;

	/*
	 * Check that the DMM_PAT_STATUS register
	 * has not reported an error.
	*/
	r = (void __iomem *)((u32)dmm_base | DMM_PAT_STATUS__0);
	v = __raw_readl(r);
	if ((v & 0xFC00) != 0) {
		while (1)
			printk(KERN_ERR "dmm_pat_refill() error.\n");
	}

	/* Set "next" register to NULL */
	r = (void __iomem *)((u32)dmm_base | DMM_PAT_DESCR__0);
	v = __raw_readl(r);
	w = (v & (~(BF(31, 4)))) | ((((u32)NULL) << 4) & BF(31, 4));
	__raw_writel(w, r);

	/* Set area to be refilled */
	r = (void __iomem *)((u32)dmm_base | DMM_PAT_AREA__0);
	v = __raw_readl(r);
	w = (v & (~(BF(30, 24)))) | ((((s8)pd->area.y1) << 24) & BF(30, 24));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(23, 16)))) | ((((s8)pd->area.x1) << 16) & BF(23, 16));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(14, 8)))) | ((((s8)pd->area.y0) << 8) & BF(14, 8));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(7, 0)))) | ((((s8)pd->area.x0) << 0) & BF(7, 0));
	__raw_writel(w, r);
	dsb();

#ifdef __DEBUG__
	printk(KERN_NOTICE "\nx0=(%d),y0=(%d),x1=(%d),y1=(%d)\n",
						(char)pd->area.x0,
						(char)pd->area.y0,
						(char)pd->area.x1,
						(char)pd->area.y1);
#endif

	/* First, clear the DMM_PAT_IRQSTATUS register */
	r = (void __iomem *)((u32)dmm_base | (u32)DMM_PAT_IRQSTATUS);
	__raw_writel(0xFFFFFFFF, r);
	dsb();

	r = (void __iomem *)((u32)dmm_base | (u32)DMM_PAT_IRQSTATUS_RAW);
	v = 0xFFFFFFFF;

	while (v != 0x0) {
		v = __raw_readl(r);
		DEBUG("DMM_PAT_IRQSTATUS_RAW", v);
	}

	/* Fill data register */
	r = (void __iomem *)((u32)dmm_base | DMM_PAT_DATA__0);
	v = __raw_readl(r);

	/* Apply 4 bit left shft to counter the 4 bit right shift */
	w = (v & (~(BF(31, 4)))) | ((((u32)(pd->data >> 4)) << 4) & BF(31, 4));
	__raw_writel(w, r);
	dsb();

	/* Read back PAT_DATA__0 to see if write was successful */
	v = 0x0;
	while (v != pd->data) {
		v = __raw_readl(r);
		DEBUG("DMM_PAT_DATA__0", v);
	}

	r = (void __iomem *)((u32)dmm_base | (u32)DMM_PAT_CTRL__0);
	v = __raw_readl(r);

	w = (v & (~(BF(31, 28)))) | ((((u32)pd->ctrl.ini) << 28) & BF(31, 28));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(16, 16)))) | ((((u32)pd->ctrl.sync) << 16) & BF(16, 16));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(9, 8)))) | ((((u32)pd->ctrl.lut_id) << 8) & BF(9, 8));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(6, 4)))) | ((((u32)pd->ctrl.dir) << 4) & BF(6, 4));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(0, 0)))) | ((((u32)pd->ctrl.start) << 0) & BF(0, 0));
	__raw_writel(w, r);
	dsb();

	/*
	 * Now, check if PAT_IRQSTATUS_RAW has been
	 * set after the PAT has been refilled
	 */
	r = (void __iomem *)((u32)dmm_base | (u32)DMM_PAT_IRQSTATUS_RAW);
	v = 0x0;
	while ((v & 0x3) != 0x3) {
		v = __raw_readl(r);
		DEBUG("DMM_PAT_IRQSTATUS_RAW", v);
	}

	return 0;
}
EXPORT_SYMBOL(dmm_pat_refill);

static s32 dmm_open(struct inode *ip, struct file *filp)
{
	return 0;
}

static s32 dmm_release(struct inode *ip, struct file *filp)
{
	return 0;
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
		unregister_chrdev_region(dev, 1);
		return -ENOMEM;
	}
	memset(dmm_device, 0x0, sizeof(struct dmm_dev));

	cdev_init(&dmm_device->cdev, &dmm_fops);
	dmm_device->cdev.owner = THIS_MODULE;
	dmm_device->cdev.ops = &dmm_fops;

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
