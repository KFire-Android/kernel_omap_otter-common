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
static struct omap_dma_lch *dma_chan;
struct omap_dma_dev_attr *d;

u32 enable_1510_mode;

#define OMAP_FUNC_MUX_ARM_BASE		(0xfffe1000 + 0xec)
static inline int omap1_get_gdma_dev(int req)
{
	u32 reg = OMAP_FUNC_MUX_ARM_BASE + ((req - 1) / 5) * 4;
	int shift = ((req - 1) % 5) * 6;

	return ((omap_readl(reg) >> shift) & 0x3f) + 1;
}

static inline void omap1_set_gdma_dev(int req, int dev)
{
	u32 reg = OMAP_FUNC_MUX_ARM_BASE + ((req - 1) / 5) * 4;
	int shift = ((req - 1) % 5) * 6;
	u32 l;

	l = omap_readl(reg);
	l &= ~(0x3f << shift);
	l |= (dev - 1) << shift;
	omap_writel(l, reg);
}

static void omap1_clear_lch_regs(int lch)
{
	int i;
	void __iomem *lch_base = dma_base + OMAP1_DMA_CH_BASE(lch);

	for (i = 0; i < 0x2c; i += 2)
		__raw_writew(0, lch_base + i);
}

static inline void omap1_enable_channel_irq(int lch)
{
	u32 status;

	status = dma_read(CSR(lch));
	dma_write(dma_chan[lch].enabled_irqs, CICR(lch));
}

static void omap1_disable_channel_irq(int lch)
{
	return;
}

static inline void omap1_enable_lnk(int lch)
{
	u32 l;

	l = dma_read(CLNK_CTRL(lch));

	l &= ~(1 << 14);

	/* Set the ENABLE_LNK bits */
	if (dma_chan[lch].next_lch != -1)
		l = dma_chan[lch].next_lch | (1 << 15);

	dma_write(l, CLNK_CTRL(lch));
}

static inline void omap1_disable_lnk(int lch)
{
	u32 l;

	l = dma_read(CLNK_CTRL(lch));

	/* Disable interrupts */
	dma_write(0, CICR(lch));
	/* Set the STOP_LNK bit */
	l |= 1 << 14;

	dma_write(l, CLNK_CTRL(lch));
	dma_chan[lch].flags &= ~OMAP_DMA_ACTIVE;
}

static int omap1_dma_handle_ch(int ch)
{
	u32 csr;

	if (enable_1510_mode && ch >= 6) {
		csr = dma_chan[ch].saved_csr;
		dma_chan[ch].saved_csr = 0;
	} else
		csr = dma_read(CSR(ch));
	if (enable_1510_mode && ch <= 2 && (csr >> 7) != 0) {
		dma_chan[ch + 6].saved_csr = csr >> 7;
		csr &= 0x7f;
	}
	if ((csr & 0x3f) == 0)
		return 0;
	if (unlikely(dma_chan[ch].dev_id == -1)) {
		printk(KERN_WARNING "Spurious interrupt from DMA channel "
		       "%d (CSR %04x)\n", ch, csr);
		return 0;
	}
	if (unlikely(csr & OMAP1_DMA_TOUT_IRQ))
		printk(KERN_WARNING "DMA timeout with device %d\n",
		       dma_chan[ch].dev_id);
	if (unlikely(csr & OMAP_DMA_DROP_IRQ))
		printk(KERN_WARNING "DMA synchronization event drop occurred "
		       "with device %d\n", dma_chan[ch].dev_id);
	if (likely(csr & OMAP_DMA_BLOCK_IRQ))
		dma_chan[ch].flags &= ~OMAP_DMA_ACTIVE;
	if (likely(dma_chan[ch].callback != NULL))
		dma_chan[ch].callback(ch, csr, dma_chan[ch].data);

	return 1;
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	int ch = ((int) dev_id) - 1;
	int handled = 0;

	for (;;) {
		int handled_now = 0;

		handled_now += omap1_dma_handle_ch(ch);
		if (enable_1510_mode && dma_chan[ch + 6].saved_csr)
			handled_now += omap1_dma_handle_ch(ch + 6);
		if (!handled_now)
			break;
		handled += handled_now;
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

void omap_set_dma_channel_mode(int lch, enum omap_dma_channel_mode mode)
{
	if (!(d->dma_dev_attr & ENABLE_1510_MODE)) {
		u32 l;

		l = dma_read(LCH_CTRL(lch));
		l &= ~0x7;
		l |= mode;
		dma_write(l, LCH_CTRL(lch));
	}
}
EXPORT_SYMBOL(omap_set_dma_channel_mode);

void omap_set_dma_src_index(int lch, int eidx, int fidx)
{
	dma_write(eidx, CSEI(lch));
	dma_write(fidx, CSFI(lch));
}
EXPORT_SYMBOL(omap_set_dma_src_index);

void omap_set_dma_dest_index(int lch, int eidx, int fidx)
{
	dma_write(eidx, CDEI(lch));
	dma_write(fidx, CDFI(lch));
}
EXPORT_SYMBOL(omap_set_dma_dest_index);

void omap_dma_set_global_params(int arb_rate, int max_fifo_depth, int tparams)
{
	return;
}
EXPORT_SYMBOL(omap_dma_set_global_params);

void omap_clear_dma_sglist_mode(int lch)
{
	return;
}

static int __init omap1_system_dma_init(void)
{
	struct platform_device *pdev;
	struct omap_system_dma_plat_info *pdata;
	int dma_irq, ret, ch;
	char irq_name[14];
	int irq_rel;

	pdev = platform_device_alloc("system_dma", 0);
	if (!pdev) {
		pr_err("%s: Unable to device alloc for dma\n",
			__func__);
		return -ENOMEM;
	}

	dma_irq = platform_get_irq_byname(pdev, irq_name);
	if (dma_irq < 0) {
		dev_err(&pdev->dev, "%s:unable to get irq\n",
							__func__);
		ret = dma_irq;
		goto exit_pdev;
	}

	ret = platform_device_add_resources(pdev, res, ARRAY_SIZE(res));
	if (ret) {
		pr_err("%s: Unable to add resources for %s%d\n",
			__func__, pdev->name, pdev->id);
		goto exit_pdev;
	}

	pdata = kzalloc(sizeof(struct omap_system_dma_plat_info), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: Unable to allocate pdata for %s\n",
			__func__, pdev->name);
		ret = -ENOMEM;
		goto exit_pdev;
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
	enable_1510_mode		= d->dma_dev_attr & ENABLE_1510_MODE;

	d->dma_dev_attr			|= SRC_PORT;
	d->dma_dev_attr			|= DST_PORT;
	d->dma_dev_attr			|= SRC_INDEX;
	d->dma_dev_attr			|= DST_INDEX;
	d->dma_dev_attr			|= IS_BURST_ONLY4;
	d->dma_dev_attr			|= CLEAR_CSR_ON_READ;
	d->dma_dev_attr			|= IS_WORD_16;

	d->dma_lch_count		= OMAP1_LOGICAL_DMA_CH_COUNT;

	pdata->enable_channel_irq		= omap1_enable_channel_irq;
	pdata->disable_channel_irq		= omap1_disable_channel_irq;
	pdata->enable_lnk			= omap1_enable_lnk;
	pdata->disable_lnk			= omap1_disable_lnk;
	pdata->clear_lch_regs		= omap1_clear_lch_regs;
	if (cpu_is_omap16xx()) {
		pdata->get_gdma_dev		= omap1_get_gdma_dev;
		pdata->set_gdma_dev		= omap1_set_gdma_dev;
	}
	pdata->enable_irq_lch		= NULL;
	pdata->disable_irq_lch		= NULL;
	pdata->set_dma_chain_ch		= NULL;
	p->dma_context_save		= NULL;
	p->dma_context_restore		= NULL;

	if (cpu_is_omap15xx())
		d->dma_chan_count = 9;
	else if (cpu_is_omap16xx() || cpu_is_omap7xx()) {
		if (!(d->dma_dev_attr & ENABLE_1510_MODE))
			d->dma_chan_count = 16;
		else
			d->dma_chan_count = 9;
	}

	for (ch = 0; ch < d->dma_chan_count; ch++) {
		if (ch >= 6 && enable_1510_mode)
			continue;
		ret = request_irq(dma_irq, irq_handler, 0, "DMA",
							(void *) (ch + 1));
		if (ret != 0)
			goto exit_pdata;
	}

	pdata->omap_dma_base		= (void __iomem *)res[0].start;
	dma_base			= pdata->omap_dma_base;
	d->dma_chan = kzalloc(sizeof(struct omap_dma_lch) *
					(d->dma_lch_count), GFP_KERNEL);
	if (!d->dma_chan) {
		dev_err(&pdev->dev, "%s: Memory allcation failed"
					"for dma_chan!!!\n", __func__);
		goto exit_pdata;
	}
	dma_chan = d->dma_chan;

	ret = platform_device_add_data(pdev, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(&pdev->dev, "%s: Unable to add resources for %s%d\n",
			__func__, pdev->name, pdev->id);
		goto exit_pdata;
	}
	ret = platform_device_add(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: Unable to add resources for %s%d\n",
			__func__, pdev->name, pdev->id);
		goto exit_dma_chan;
	}

exit_dma_chan:
	kfree(d->dma_chan);
exit_pdata:
	printk(KERN_ERR "unable to request IRQ %d"
				"for DMA (error %d)\n", dma_irq, ret);
	if (enable_1510_mode)
		ch = 6;
	for (irq_rel = 0; irq_rel < ch; irq_rel++) {
		dma_irq = platform_get_irq(pdev, irq_rel);
		free_irq(dma_irq, (void *)(irq_rel + 1));
	}
	kfree(pdata);
exit_pdev:
	platform_device_put(pdev);
	return ret;
}
arch_initcall(omap1_system_dma_init);
