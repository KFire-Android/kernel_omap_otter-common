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

/* TODO: remove */
#include "../tiler/tiler_def.h"

#define BITS_32(in_NbBits) ((((u32)1 << in_NbBits) - 1) | ((u32)1 << in_NbBits))
#define BITFIELD_32(in_UpBit, in_LowBit)\
	(BITS_32(in_UpBit) & ~((BITS_32(in_LowBit)) >> 1))
#define BITFIELD BITFIELD_32

#if 0
#define regdump(x, y) printk(KERN_NOTICE "%s()::%d:%s=(0x%08x)\n", \
				__func__, __LINE__, x, (s32)y);
#else
#define regdump(x, y)
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

static void pat_ctrl_set(struct pat_ctrl *ctrl, s8 id)
{
	void __iomem *reg = NULL;
	u32 reg_val = 0x0;
	u32 new_val = 0x0;
	u32 bit_field = 0x0;
	u32 field_pos = 0x0;

	/* set DMM_PAT_CTRL register */
	/* TODO: casting as u32 */

	reg = (void __iomem *)(
			(u32)dmm_base |
			(u32)DMM_PAT_CTRL__0);
	reg_val = __raw_readl(reg);

	bit_field = BITFIELD(31, 28);
	field_pos = 28;
	new_val = (reg_val & (~(bit_field))) |
			((((u32)ctrl->initiator) <<
							field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(16, 16);
	field_pos = 16;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->sync) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(9, 8);
	field_pos = 8;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->lut_id) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(6, 4);
	field_pos = 4;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->direction) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(0, 0);
	field_pos = 0;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->start) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	dsb();
}

enum errorCodeT dmm_pat_refill_area_status_get(signed long dmmPatAreaStatSel,
		struct dmmPATStatusT *areaStatus)
{
	enum errorCodeT eCode = DMM_NO_ERROR;

	u32 stat = 0xffffffff;
	void __iomem *statreg = (void __iomem *)
				((u32)dmm_base | 0x4c0ul);

	if (dmmPatAreaStatSel >= 0 && dmmPatAreaStatSel <= 3) {
		stat = __raw_readl(statreg);

		areaStatus->remainingLinesCounter = stat & BITFIELD(24, 16);
		areaStatus->error = (enum dmmPATStatusErrT)
				    (stat & BITFIELD(15, 10));
		areaStatus->linkedReconfig = stat & BITFIELD(4, 4);
		areaStatus->done = stat & BITFIELD(3, 3);
		areaStatus->engineRunning = stat & BITFIELD(2, 2);
		areaStatus->validDescriptor = stat & BITFIELD(1, 1);
		areaStatus->ready = stat & BITFIELD(0, 0);

	} else {
		eCode = DMM_WRONG_PARAM;
		printk(KERN_ALERT "wrong parameter\n");
	}

	return eCode;
}

s32 dmm_pat_refill(struct pat *patDesc, s32 dmmPatAreaSel,
			enum pat_mode refillType, s32 forced_refill)
{
	enum errorCodeT eCode = DMM_NO_ERROR;
	void __iomem *reg = NULL;
	u32 regval = 0xffffffff;
	u32 writeval = 0xffffffff;
	u32 f = 0xffffffff; /* field */
	u32 fp = 0xffffffff; /* field position */
	struct pat pat_desc = {0};

	struct dmmPATStatusT areaStat;
	if (forced_refill == 0) {
		eCode = dmm_pat_refill_area_status_get(
				dmmPatAreaSel, &areaStat);

		if (eCode == DMM_NO_ERROR) {
			if (areaStat.ready == 0 || areaStat.engineRunning) {
				printk(KERN_ALERT "hw not ready\n");
				eCode = DMM_HRDW_NOT_READY;
			}
		}
	}

	if (dmmPatAreaSel < 0 || dmmPatAreaSel > 3) {
		eCode = DMM_WRONG_PARAM;
	} else if (eCode == DMM_NO_ERROR) {
		if (refillType == AUTO) {
			reg = (void __iomem *)
				((u32)dmm_base |
				(0x500ul + 0x0));
			regval = __raw_readl(reg);
			f  = BITFIELD(31, 4);
			fp = 4;
			writeval = (regval & (~(f))) |
				   ((((u32)patDesc) << fp) & f);
			__raw_writel(writeval, reg);
		} else if (refillType == MANUAL) {
			/* check that the DMM_PAT_STATUS register has not
			 * reported an error.
			 */
			reg = (void __iomem *)(
					(u32)dmm_base |
						(u32)DMM_PAT_STATUS__0);
			regval = __raw_readl(reg);
			if ((regval & 0xFC00) != 0) {
				while (1) {
					printk(KERN_ERR "%s()::%d: ERROR\n",
						__func__, __LINE__);
				}
			}

			/* set DESC register to NULL */
			reg = (void __iomem *)
				((u32)dmm_base |
				(0x500ul + 0x0));
			regval = __raw_readl(reg);
			f  = BITFIELD(31, 4);
			fp = 4;
			writeval = (regval & (~(f))) |
				   ((((u32)NULL) << fp) & f);
			__raw_writel(writeval, reg);
			reg = (void __iomem *)
				((u32)dmm_base |
				(0x500ul + 0x4));
			regval = __raw_readl(reg);

			/* set area to be refilled */
			f  = BITFIELD(30, 24);
			fp = 24;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.y1) << fp) & f);
			__raw_writel(writeval, reg);

			regval = __raw_readl(reg);
			f  = BITFIELD(23, 16);
			fp = 16;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.x1) << fp) & f);
			__raw_writel(writeval, reg);

			regval = __raw_readl(reg);
			f  = BITFIELD(14, 8);
			fp = 8;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.y0) << fp) & f);
			__raw_writel(writeval, reg);

			regval = __raw_readl(reg);
			f  = BITFIELD(7, 0);
			fp = 0;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.x0) << fp) & f);
			__raw_writel(writeval, reg);

			printk(KERN_NOTICE
				"\nx0=(%d),y0=(%d),x1=(%d),y1=(%d)\n",
				(char)patDesc->area.x0,
				(char)patDesc->area.y0,
				(char)patDesc->area.x1,
				(char)patDesc->area.y1);
			dsb();

			/* First, clear the DMM_PAT_IRQSTATUS register */
			reg = (void __iomem *)(
					(u32)dmm_base |
					(u32)DMM_PAT_IRQSTATUS);
			__raw_writel(0xFFFFFFFF, reg);
			dsb();

			reg = (void __iomem *)(
					(u32)dmm_base |
					(u32)DMM_PAT_IRQSTATUS_RAW);
			regval = 0xFFFFFFFF;

			while (regval != 0x0) {
				regval = __raw_readl(reg);
				regdump("DMM_PAT_IRQSTATUS_RAW", regval);
			}

			/* fill data register */
			reg = (void __iomem *)
			((u32)dmm_base | (0x500ul + 0xc));
			regval = __raw_readl(reg);

			/* Apply 4 bit lft shft to counter the 4 bit rt shft */
			f  = BITFIELD(31, 4);
			fp = 4;
			writeval = (regval & (~(f))) | ((((u32)
					(patDesc->data >> 4)) << fp) & f);
			__raw_writel(writeval, reg);
			dsb();

			/* read back PAT_DATA__0 to see if it got set */
			regval = 0x0;
			while (regval != patDesc->data) {
				regval = __raw_readl(reg);
				regdump("DMM_PAT_DATA__0", regval);
			}

			pat_desc.ctrl.start = 1;
			pat_desc.ctrl.direction = 0;
			pat_desc.ctrl.lut_id = 0;
			pat_desc.ctrl.sync = 0;
			pat_desc.ctrl.initiator = 0;
			pat_ctrl_set(&pat_desc.ctrl, 0);

			/* Now, check if PAT_IRQSTATUS_RAW has been set after
			the PAT has been refilled */
			reg = (void __iomem *)(
					(u32)dmm_base |
					(u32)DMM_PAT_IRQSTATUS_RAW);
			regval = 0x0;
			while ((regval & 0x3) != 0x3) {
				regval = __raw_readl(reg);
				regdump("DMM_PAT_IRQSTATUS_RAW", regval);
			}
		} else {
			eCode = DMM_WRONG_PARAM;
		}
		if (eCode == DMM_NO_ERROR) {
			eCode = dmm_pat_refill_area_status_get(dmmPatAreaSel,
							       &areaStat);

			if (eCode == DMM_NO_ERROR) {
				if (areaStat.validDescriptor == 0) {
					eCode = DMM_HRDW_CONFIG_FAILED;
					printk(KERN_ALERT "hw config fail\n");
				}
			}

			while (!areaStat.done && !areaStat.ready &&
					eCode == DMM_NO_ERROR) {
				eCode = dmm_pat_refill_area_status_get(
						dmmPatAreaSel,
						&areaStat);
			}

			if (areaStat.error) {
				eCode = DMM_HRDW_CONFIG_FAILED;
				printk(KERN_ALERT "hw config fail\n");
			}
		}
	}

	return eCode;
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
