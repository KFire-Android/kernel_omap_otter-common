/*
 * dma.c - OMAP2 specific DMA code
 *
 * Copyright (C) 2003 - 2008 Nokia Corporation
 * Author: Juha Yrjölä <juha.yrjola@nokia.com>
 * DMA channel linking for 1610 by Samuel Ortiz <samuel.ortiz@nokia.com>
 * Graphics DMA and LCD DMA graphics tranformations
 * by Imre Deak <imre.deak@nokia.com>
 * OMAP2/3 support Copyright (C) 2004-2007 Texas Instruments, Inc.
 * Some functions based on earlier dma-omap.c Copyright (C) 2001 RidgeRun, Inc.
 *
 * Copyright (C) 2009 Texas Instruments
 * Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Converted DMA library into platform driver by Manjunatha GK <manjugk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>

#include <plat/irqs.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/dma.h>

#define dma_read(reg)							\
({									\
	u32 __val;							\
	__val = __raw_readl(dma_base + OMAP_DMA4_##reg);		\
	__val;								\
})

#define dma_write(val, reg)						\
({									\
	__raw_writel((val), dma_base + OMAP_DMA4_##reg);		\
})

static struct omap_dma_dev_attr *d;
static void __iomem *dma_base;
static struct omap_system_dma_plat_info *omap2_pdata;
static int dma_caps0_status;

static struct omap_device_pm_latency omap2_dma_latency[] = {
	{
	.deactivate_func = omap_device_idle_hwmods,
	.activate_func	 = omap_device_enable_hwmods,
	.flags		 = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

/* One time initializations */
static int __init omap2_system_dma_init_dev(struct omap_hwmod *oh, void *user)
{
	struct omap_device *od;
	struct omap_system_dma_plat_info *pdata;
	struct resource *mem;
	char *name = "dma";

	pdata = kzalloc(sizeof(struct omap_system_dma_plat_info), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: Unable to allocate pdata for %s:%s\n",
			__func__, name, oh->name);
		return -ENOMEM;
	}

	pdata->dma_attr		= (struct omap_dma_dev_attr *)oh->dev_attr;

	od = omap_device_build(name, 0, oh, pdata, sizeof(*pdata),
			omap2_dma_latency, ARRAY_SIZE(omap2_dma_latency), 0);

	if (IS_ERR(od)) {
		pr_err("%s: Cant build omap_device for %s:%s.\n",
			__func__, name, oh->name);
		kfree(pdata);
		return 0;
	}

	mem = platform_get_resource(&od->pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&od->pdev.dev, "%s: no mem resource\n", __func__);
		return -EINVAL;
	}

	dma_base = ioremap(mem->start, resource_size(mem));
	if (!dma_base) {
		dev_err(&od->pdev.dev, "%s: ioremap fail\n", __func__);
		return -ENOMEM;
	}

	/* Get DMA device attributes from hwmod data base */
	d = (struct omap_dma_dev_attr *)oh->dev_attr;

	/* OMAP2 Plus: physical and logical channel count is same */
	d->dma_chan_count = d->dma_lch_count;

	d->dma_chan = kzalloc(sizeof(struct omap_dma_lch) *
					(d->dma_lch_count), GFP_KERNEL);

	if (!d->dma_chan) {
		dev_err(&od->pdev.dev, "%s: kzalloc fail\n", __func__);
		return -ENOMEM;
	}

	omap2_pdata		= pdata;
	dma_caps0_status	= dma_read(CAPS_0);

	return 0;
}

static int __init omap2_system_dma_init(void)
{
	int ret;

	ret = omap_hwmod_for_each_by_class("dma",
			omap2_system_dma_init_dev, NULL);

	return ret;
}
arch_initcall(omap2_system_dma_init);
