/*
 * dma.c - OMAP1/OMAP7xx-specific DMA code
 *
 * Copyright (C) 2003 - 2008 Nokia Corporation
 * Author: Juha Yrjölä <juha.yrjola@nokia.com>
 * DMA channel linking for 1610 by Samuel Ortiz <samuel.ortiz@nokia.com>
 * Graphics DMA and LCD DMA graphics tranformations
 * by Imre Deak <imre.deak@nokia.com>
 * OMAP2/3 support Copyright (C) 2004-2007 Texas Instruments, Inc.
 * Some functions based on earlier dma-omap.c Copyright (C) 2001 RidgeRun, Inc.
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

#include <plat/dma.h>
#include <plat/tc.h>

#define dma_read(reg)							\
({									\
	u32 __val;							\
	__val = __raw_readw(dma_base + OMAP1_DMA_##reg);		\
	__val;								\
})

#define dma_write(val, reg)						\
({									\
	__raw_writew((val), dma_base + OMAP1_DMA_##reg);		\
})

static struct resource res[] __initdata = {
	[0] = {
		.start	= OMAP1_DMA_BASE,
		.end	= OMAP1_DMA_BASE + SZ_2K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name   = "system_dma_0",
		.start  = INT_DMA_CH0_6,
		.flags  = IORESOURCE_IRQ,
	},
	[2] = {
		.name   = "system_dma_1",
		.start  = INT_DMA_CH1_7,
		.flags  = IORESOURCE_IRQ,
	},
	[3] = {
		.name   = "system_dma_2",
		.start  = INT_DMA_CH2_8,
		.flags  = IORESOURCE_IRQ,
	},
	[4] = {
		.name   = "system_dma_3",
		.start  = INT_DMA_CH3,
		.flags  = IORESOURCE_IRQ,
	},
	[5] = {
		.name   = "system_dma_4",
		.start  = INT_DMA_CH4,
		.flags  = IORESOURCE_IRQ,
	},
	[6] = {
		.name   = "system_dma_5",
		.start  = INT_DMA_CH5,
		.flags  = IORESOURCE_IRQ,
	},
	[7] = {
		.name   = "system_dma_6",
		.start  = INT_DMA_LCD,
		.flags  = IORESOURCE_IRQ,
	},
	/* irq's for omap16xx and omap7xx */
	[8] = {
		.name   = "system_dma_7",
		.start  = 53 + IH2_BASE,
		.flags  = IORESOURCE_IRQ,
	},
	[9] = {
		.name   = "system_dma_8",
		.start  = 54 + IH2_BASE,
		.flags  = IORESOURCE_IRQ,
	},
	[10] = {
		.name  = "system_dma_9",
		.start = 55 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
	[11] = {
		.name  = "system_dma_10",
		.start = 56 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
	[12] = {
		.name  = "system_dma_11",
		.start = 57 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
	[13] = {
		.name  = "system_dma_12",
		.start = 58 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
	[14] = {
		.name  = "system_dma_13",
		.start = 59 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
	[15] = {
		.name  = "system_dma_14",
		.start = 60 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
	[16] = {
		.name  = "system_dma_15",
		.start = 61 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
	[17] = {
		.name  = "system_dma_16",
		.start = 62 + IH2_BASE,
		.flags = IORESOURCE_IRQ,
	},
};

static void __iomem *dma_base;

static int __init omap1_system_dma_init(void)
{
	struct platform_device *pdev;
	struct omap_system_dma_plat_info *pdata;
	struct omap_dma_dev_attr *d;
	int ret;

	pdev = platform_device_alloc("system_dma", 0);
	if (!pdev) {
		pr_err("%s: Unable to device alloc for dma\n",
			__func__);
		return -ENOMEM;
	}

	ret = platform_device_add_resources(pdev, res, ARRAY_SIZE(res));
	if (ret) {
		pr_err("%s: Unable to add resources for %s%d\n",
			__func__, pdev->name, pdev->id);
		goto exit_device_put;
	}

	pdata = kzalloc(sizeof(struct omap_system_dma_plat_info), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: Unable to allocate pdata for %s\n",
			__func__, pdev->name);
		ret = -ENOMEM;
		goto exit_device_put;
	}

	/* Errata handling for all omap1 plus processors */
	pdata->errata			= 0;

	if (cpu_class_is_omap1() && !cpu_is_omap15xx())
		pdata->errata		|= OMAP3_3_ERRATUM;

	d = pdata->dma_attr;

	/* Valid attributes for omap1 plus processors */
	d->dma_dev_attr = 0;

	if (cpu_is_omap15xx())
		d->dma_dev_attr = ENABLE_1510_MODE;

	d->dma_dev_attr			|= SRC_PORT;
	d->dma_dev_attr			|= DST_PORT;
	d->dma_dev_attr			|= SRC_INDEX;
	d->dma_dev_attr			|= DST_INDEX;
	d->dma_dev_attr			|= IS_BURST_ONLY4;
	d->dma_dev_attr			|= CLEAR_CSR_ON_READ;
	d->dma_dev_attr			|= IS_WORD_16;

	d->dma_lch_count		= OMAP1_LOGICAL_DMA_CH_COUNT;

	if (cpu_is_omap15xx())
		d->dma_chan_count = 9;
	else if (cpu_is_omap16xx() || cpu_is_omap7xx()) {
		if (!(d->dma_dev_attr & ENABLE_1510_MODE))
			d->dma_chan_count = 16;
		else
			d->dma_chan_count = 9;
	}

	pdata->omap_dma_base		= (void __iomem *)res[0].start;
	dma_base			= pdata->omap_dma_base;

	d->dma_chan = kzalloc(sizeof(struct omap_dma_lch) *
					(d->dma_lch_count), GFP_KERNEL);

	if (!d->dma_chan) {
		dev_err(&pdev->dev, "%s: Memory allcation failed"
					"for dma_chan!!!\n", __func__);
		goto exit_release_pdata;
	}

	ret = platform_device_add_data(pdev, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(&pdev->dev, "%s: Unable to add resources for %s%d\n",
			__func__, pdev->name, pdev->id);
		goto exit_release_dma_chan;
	}
	ret = platform_device_add(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: Unable to add resources for %s%d\n",
			__func__, pdev->name, pdev->id);
		goto exit_release_dma_chan;
	}

exit_release_dma_chan:
	kfree(d->dma_chan);
exit_release_pdata:
	kfree(pdata);
exit_device_put:
	platform_device_put(pdev);

	return ret;
}
arch_initcall(omap1_system_dma_init);
