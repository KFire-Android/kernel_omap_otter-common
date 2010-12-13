/*
 * linux/arch/arm/plat-omap/dma.c
 *
 * Copyright (C) 2003 - 2008 Nokia Corporation
 * Author: Juha Yrjölä <juha.yrjola@nokia.com>
 * DMA channel linking for 1610 by Samuel Ortiz <samuel.ortiz@nokia.com>
 * Graphics DMA and LCD DMA graphics tranformations
 * by Imre Deak <imre.deak@nokia.com>
 * OMAP2/3 support Copyright (C) 2004-2007 Texas Instruments, Inc.
 * Merged to support both OMAP1 and OMAP2 by Tony Lindgren <tony@atomide.com>
 * Some functions based on earlier dma-omap.c Copyright (C) 2001 RidgeRun, Inc.
 *
 * Copyright (C) 2009 Texas Instruments
 * Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Support functions for the OMAP internal DMA channels.
 *
 * Copyright (C) 2010 Texas Instruments
 * Converted DMA library into DMA platform driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include <asm/system.h>

#include <mach/hardware.h>

#include <plat/dma.h>
#include <plat/tc.h>

static int enable_1510_mode;
static int dma_lch_count;
static int dma_chan_count;
static int omap_dma_reserve_channels;

static spinlock_t dma_chan_lock;

static void __iomem *omap_dma_base;

static struct omap_dma_lch *dma_chan;
static struct omap_system_dma_plat_info *p;
static struct omap_dma_dev_attr *d;
static struct platform_device           *pd;

#define dma_read(reg)							\
({									\
	u32 __val;							\
	if (cpu_class_is_omap1())					\
		__val = __raw_readw(omap_dma_base + OMAP1_DMA_##reg);	\
	else								\
		__val = __raw_readl(omap_dma_base + OMAP_DMA4_##reg);	\
	__val;								\
})

#define dma_write(val, reg)						\
({									\
	if (cpu_class_is_omap1())					\
		__raw_writew((u16)(val), omap_dma_base + OMAP1_DMA_##reg); \
	else								\
		__raw_writel((val), omap_dma_base + OMAP_DMA4_##reg);	\
})

void omap_set_dma_priority(int lch, int dst_port, int priority)
{
	unsigned long reg;
	u32 l;

	if (d->dma_dev_attr & IS_WORD_16) {
		switch (dst_port) {
		case OMAP_DMA_PORT_OCP_T1:	/* FFFECC00 */
			reg = OMAP_TC_OCPT1_PRIOR;
			break;
		case OMAP_DMA_PORT_OCP_T2:	/* FFFECCD0 */
			reg = OMAP_TC_OCPT2_PRIOR;
			break;
		case OMAP_DMA_PORT_EMIFF:	/* FFFECC08 */
			reg = OMAP_TC_EMIFF_PRIOR;
			break;
		case OMAP_DMA_PORT_EMIFS:	/* FFFECC04 */
			reg = OMAP_TC_EMIFS_PRIOR;
			break;
		default:
			BUG();
			return;
		}
		l = omap_readl(reg);
		l &= ~(0xf << 8);
		l |= (priority & 0xf) << 8;
		omap_writel(l, reg);
	} else {
		u32 ccr;

		ccr = dma_read(CCR(lch));
		if (priority)
			ccr |= (1 << 6);
		else
			ccr &= ~(1 << 6);
		dma_write(ccr, CCR(lch));
	}
}
EXPORT_SYMBOL(omap_set_dma_priority);

void omap_set_dma_transfer_params(int lch, int data_type, int elem_count,
				  int frame_count, int sync_mode,
				  int dma_trigger, int src_or_dst_synch)
{
	u32 l;

	l = dma_read(CSDP(lch));
	l &= ~0x03;
	l |= data_type;
	dma_write(l, CSDP(lch));

	if (d->dma_dev_attr & IS_WORD_16) {
		u16 ccr;

		ccr = dma_read(CCR(lch));
		ccr &= ~(1 << 5);
		if (sync_mode == OMAP_DMA_SYNC_FRAME)
			ccr |= 1 << 5;
		dma_write(ccr, CCR(lch));

		ccr = dma_read(CCR2(lch));
		ccr &= ~(1 << 2);
		if (sync_mode == OMAP_DMA_SYNC_BLOCK)
			ccr |= 1 << 2;
		dma_write(ccr, CCR2(lch));
	} else if (dma_trigger) {
		u32 val;

		val = dma_read(CCR(lch));

		/* DMA_SYNCHRO_CONTROL_UPPER depends on the channel number */
		val &= ~((3 << 19) | 0x1f);
		val |= (dma_trigger & ~0x1f) << 14;
		val |= dma_trigger & 0x1f;

		if (sync_mode & OMAP_DMA_SYNC_FRAME)
			val |= 1 << 5;
		else
			val &= ~(1 << 5);

		if (sync_mode & OMAP_DMA_SYNC_BLOCK)
			val |= 1 << 18;
		else
			val &= ~(1 << 18);

		if (src_or_dst_synch)
			val |= 1 << 24;		/* source synch */
		else
			val &= ~(1 << 24);	/* dest synch */

		dma_write(val, CCR(lch));
	}

	dma_write(elem_count, CEN(lch));
	dma_write(frame_count, CFN(lch));
}
EXPORT_SYMBOL(omap_set_dma_transfer_params);

void omap_set_dma_color_mode(int lch, enum omap_dma_color_mode mode, u32 color)
{
	BUG_ON(enable_1510_mode);

	if (d->dma_dev_attr & IS_WORD_16) {
		u16 w;

		w = dma_read(CCR2(lch));
		w &= ~0x03;

		switch (mode) {
		case OMAP_DMA_CONSTANT_FILL:
			w |= 0x01;
			break;
		case OMAP_DMA_TRANSPARENT_COPY:
			w |= 0x02;
			break;
		case OMAP_DMA_COLOR_DIS:
			break;
		default:
			BUG();
		}
		dma_write(w, CCR2(lch));

		w = dma_read(LCH_CTRL(lch));
		w &= ~0x0f;
		/* Default is channel type 2D */
		if (mode) {
			dma_write((u16)color, COLOR_L(lch));
			dma_write((u16)(color >> 16), COLOR_U(lch));
			w |= 1;		/* Channel type G */
		}
		dma_write(w, LCH_CTRL(lch));
	} else {
		u32 val;

		val = dma_read(CCR(lch));
		val &= ~((1 << 17) | (1 << 16));

		switch (mode) {
		case OMAP_DMA_CONSTANT_FILL:
			val |= 1 << 16;
			break;
		case OMAP_DMA_TRANSPARENT_COPY:
			val |= 1 << 17;
			break;
		case OMAP_DMA_COLOR_DIS:
			break;
		default:
			BUG();
		}
		dma_write(val, CCR(lch));

		color &= 0xffffff;
		dma_write(color, COLOR(lch));
	}
}
EXPORT_SYMBOL(omap_set_dma_color_mode);

/* Note that src_port is only for omap1 */
void omap_set_dma_src_params(int lch, int src_port, int src_amode,
			     unsigned long src_start,
			     int src_ei, int src_fi)
{
	u32 l;

	if (d->dma_dev_attr & SRC_PORT) {
		u16 w;

		w = dma_read(CSDP(lch));
		w &= ~(0x1f << 2);
		w |= src_port << 2;
		dma_write(w, CSDP(lch));
	}

	l = dma_read(CCR(lch));
	l &= ~(0x03 << 12);
	l |= src_amode << 12;
	dma_write(l, CCR(lch));

	if (d->dma_dev_attr & IS_WORD_16) {
		dma_write(src_start >> 16, CSSA_U(lch));
		dma_write((u16)src_start, CSSA_L(lch));
	} else
		dma_write(src_start, CSSA(lch));

	dma_write(src_ei, CSEI(lch));
	dma_write(src_fi, CSFI(lch));
}
EXPORT_SYMBOL(omap_set_dma_src_params);

void omap_set_dma_params(int lch, struct omap_dma_channel_params *params)
{
	omap_set_dma_transfer_params(lch, params->data_type,
				     params->elem_count, params->frame_count,
				     params->sync_mode, params->trigger,
				     params->src_or_dst_synch);
	omap_set_dma_src_params(lch, params->src_port,
				params->src_amode, params->src_start,
				params->src_ei, params->src_fi);

	omap_set_dma_dest_params(lch, params->dst_port,
				 params->dst_amode, params->dst_start,
				 params->dst_ei, params->dst_fi);
	if (params->read_prio || params->write_prio)
		omap_dma_set_prio_lch(lch, params->read_prio,
				      params->write_prio);
}
EXPORT_SYMBOL(omap_set_dma_params);

void omap_set_dma_src_data_pack(int lch, int enable)
{
	u32 l;

	l = dma_read(CSDP(lch));
	l &= ~(1 << 6);
	if (enable)
		l |= (1 << 6);
	dma_write(l, CSDP(lch));
}
EXPORT_SYMBOL(omap_set_dma_src_data_pack);

void omap_set_dma_src_burst_mode(int lch, enum omap_dma_burst_mode burst_mode)
{
	unsigned int burst = 0;
	u32 l;

	l = dma_read(CSDP(lch));
	l &= ~(0x03 << 7);

	switch (burst_mode) {
	case OMAP_DMA_DATA_BURST_DIS:
		break;
	case OMAP_DMA_DATA_BURST_4:
		if (d->dma_dev_attr & IS_BURST_ONLY4)
			burst = 0x2;
		else
			burst = 0x1;
		break;
	case OMAP_DMA_DATA_BURST_8:
		if (!(d->dma_dev_attr & IS_BURST_ONLY4)) {
			burst = 0x2;
			break;
		}
		/*
		 * not supported by current hardware on OMAP1
		 * w |= (0x03 << 7);
		 * fall through
		 */
	case OMAP_DMA_DATA_BURST_16:
		if (!(d->dma_dev_attr & IS_BURST_ONLY4)) {
			burst = 0x3;
			break;
		}
		/*
		 * OMAP1 don't support burst 16
		 * fall through
		 */
	default:
		BUG();
	}

	l |= (burst << 7);
	dma_write(l, CSDP(lch));
}
EXPORT_SYMBOL(omap_set_dma_src_burst_mode);

/* Note that dest_port is only for OMAP1 */
void omap_set_dma_dest_params(int lch, int dest_port, int dest_amode,
			      unsigned long dest_start,
			      int dst_ei, int dst_fi)
{
	u32 l;

	if (d->dma_dev_attr & DST_PORT) {
		l = dma_read(CSDP(lch));
		l &= ~(0x1f << 9);
		l |= dest_port << 9;
		dma_write(l, CSDP(lch));
	}

	l = dma_read(CCR(lch));
	l &= ~(0x03 << 14);
	l |= dest_amode << 14;
	dma_write(l, CCR(lch));

	if (d->dma_dev_attr & IS_WORD_16) {
		dma_write(dest_start >> 16, CDSA_U(lch));
		dma_write(dest_start, CDSA_L(lch));
	} else
		dma_write(dest_start, CDSA(lch));

	dma_write(dst_ei, CDEI(lch));
	dma_write(dst_fi, CDFI(lch));
}
EXPORT_SYMBOL(omap_set_dma_dest_params);

void omap_set_dma_dest_data_pack(int lch, int enable)
{
	u32 l;

	l = dma_read(CSDP(lch));
	l &= ~(1 << 13);
	if (enable)
		l |= 1 << 13;
	dma_write(l, CSDP(lch));
}
EXPORT_SYMBOL(omap_set_dma_dest_data_pack);

void omap_set_dma_dest_burst_mode(int lch, enum omap_dma_burst_mode burst_mode)
{
	unsigned int burst = 0;
	u32 l;

	l = dma_read(CSDP(lch));
	l &= ~(0x03 << 14);

	switch (burst_mode) {
	case OMAP_DMA_DATA_BURST_DIS:
		break;
	case OMAP_DMA_DATA_BURST_4:
		if (d->dma_dev_attr & IS_BURST_ONLY4)
			burst = 0x2;
		else
			burst = 0x1;
		break;
	case OMAP_DMA_DATA_BURST_8:
		if (d->dma_dev_attr & IS_BURST_ONLY4)
			burst = 0x3;
		else
			burst = 0x2;
		break;
	case OMAP_DMA_DATA_BURST_16:
		if (!(d->dma_dev_attr & IS_BURST_ONLY4)) {
			burst = 0x3;
			break;
		}
		/*
		 * OMAP1 don't support burst 16
		 * fall through
		 */
	default:
		printk(KERN_ERR "Invalid DMA burst mode\n");
		BUG();
		return;
	}
	l |= (burst << 14);
	dma_write(l, CSDP(lch));
}
EXPORT_SYMBOL(omap_set_dma_dest_burst_mode);

void omap_enable_dma_irq(int lch, u16 bits)
{
	dma_chan[lch].enabled_irqs |= bits;
}
EXPORT_SYMBOL(omap_enable_dma_irq);

void omap_disable_dma_irq(int lch, u16 bits)
{
	dma_chan[lch].enabled_irqs &= ~bits;
}
EXPORT_SYMBOL(omap_disable_dma_irq);

int omap_request_dma(int dev_id, const char *dev_name,
		     void (*callback)(int lch, u16 ch_status, void *data),
		     void *data, int *dma_ch_out)
{
	int ch, free_ch = -1;
	unsigned long flags;
	struct omap_dma_lch *chan;

	pm_runtime_get_sync(&pd->dev);

	spin_lock_irqsave(&dma_chan_lock, flags);
	for (ch = 0; ch < dma_chan_count; ch++) {
		if (free_ch == -1 && dma_chan[ch].dev_id == -1) {
			free_ch = ch;
			if (dev_id == 0)
				break;
		}
	}
	if (free_ch == -1) {
		spin_unlock_irqrestore(&dma_chan_lock, flags);
		pm_runtime_put(&pd->dev);
		return -EBUSY;
	}
	chan = dma_chan + free_ch;
	chan->dev_id = dev_id;

	if (p->clear_lch_regs)
		p->clear_lch_regs(free_ch);
	else
		omap_clear_dma(free_ch);

	spin_unlock_irqrestore(&dma_chan_lock, flags);

	chan->dev_name = dev_name;
	chan->callback = callback;
	chan->data = data;
	chan->flags = 0;

	if (p->set_dma_chain_ch)
		p->set_dma_chain_ch(free_ch);

	chan->enabled_irqs = OMAP_DMA_DROP_IRQ | OMAP_DMA_BLOCK_IRQ;

	if (d->dma_dev_attr & IS_WORD_16)
		chan->enabled_irqs |= OMAP1_DMA_TOUT_IRQ;
	else
		chan->enabled_irqs |= OMAP2_DMA_MISALIGNED_ERR_IRQ |
						OMAP2_DMA_TRANS_ERR_IRQ;

	if (p->set_gdma_dev) {
		/* If the sync device is set, configure it dynamically. */
		if (dev_id != 0) {
			p->set_gdma_dev(free_ch + 1, dev_id);
			dev_id = free_ch + 1;
		}
		/*
		 * Disable the 1510 compatibility mode and set the sync device
		 * id.
		 */
		dma_write(dev_id | (1 << 10), CCR(free_ch));
	} else if (d->dma_dev_attr & IS_WORD_16)
		dma_write(dev_id, CCR(free_ch));

	if (p->enable_irq_lch) {
		spin_lock_irqsave(&dma_chan_lock, flags);
		p->enable_irq_lch(free_ch);
		spin_unlock_irqrestore(&dma_chan_lock, flags);
		if (p->enable_channel_irq)
			p->enable_channel_irq(free_ch);
		/* Clear the CSR register and IRQ status register */
		dma_write(OMAP2_DMA_CSR_CLEAR_MASK, CSR(free_ch));
		dma_write(1 << free_ch, IRQSTATUS_L0);
	}

	*dma_ch_out = free_ch;

	return 0;
}
EXPORT_SYMBOL(omap_request_dma);

void omap_free_dma(int lch)
{
	unsigned long flags;

	if (dma_chan[lch].dev_id == -1) {
		pr_err("omap_dma: trying to free unallocated DMA channel %d\n",
		       lch);
		return;
	}

	if (d->dma_dev_attr & IS_WORD_16) {
		/* Disable all DMA interrupts for the channel. */
		dma_write(0, CICR(lch));
		/* Make sure the DMA transfer is stopped. */
		dma_write(0, CCR(lch));
	}

	if (p->disable_irq_lch) {
		unsigned long flags;
		spin_lock_irqsave(&dma_chan_lock, flags);
		p->disable_irq_lch(lch);
		spin_unlock_irqrestore(&dma_chan_lock, flags);

		/* Clear the CSR register and IRQ status register */
		dma_write(OMAP2_DMA_CSR_CLEAR_MASK, CSR(lch));
		dma_write(1 << lch, IRQSTATUS_L0);

		/* Disable all DMA interrupts for the channel. */
		dma_write(0, CICR(lch));

		/* Make sure the DMA transfer is stopped. */
		dma_write(0, CCR(lch));
		omap_clear_dma(lch);
		omap_clear_dma_sglist_mode(lch);
	}
	pm_runtime_put(&pd->dev);

	spin_lock_irqsave(&dma_chan_lock, flags);
	dma_chan[lch].dev_id = -1;
	dma_chan[lch].next_lch = -1;
	dma_chan[lch].callback = NULL;
	spin_unlock_irqrestore(&dma_chan_lock, flags);
}
EXPORT_SYMBOL(omap_free_dma);

/**
 * @brief omap_dma_set_prio_lch : Set channel wise priority settings
 *
 * @param lch
 * @param read_prio - Read priority
 * @param write_prio - Write priority
 * Both of the above can be set with one of the following values :
 * 	DMA_CH_PRIO_HIGH/DMA_CH_PRIO_LOW
 */
int
omap_dma_set_prio_lch(int lch, unsigned char read_prio,
		      unsigned char write_prio)
{
	u32 l;

	if (unlikely((lch < 0 || lch >= dma_lch_count))) {
		printk(KERN_ERR "Invalid channel id\n");
		return -EINVAL;
	}
	l = dma_read(CCR(lch));
	l &= ~((1 << 6) | (1 << 26));
	if (d->dma_dev_attr & IS_RW_PRIORIY)
		l |= ((read_prio & 0x1) << 6) | ((write_prio & 0x1) << 26);
	else
		l |= ((read_prio & 0x1) << 6);

	dma_write(l, CCR(lch));

	return 0;
}
EXPORT_SYMBOL(omap_dma_set_prio_lch);

/*
 * Clears any DMA state so the DMA engine is ready to restart with new buffers
 * through omap_start_dma(). Any buffers in flight are discarded.
 */
void omap_clear_dma(int lch)
{
	unsigned long flags;

	local_irq_save(flags);

	if (d->dma_dev_attr & IS_WORD_16) {
		u32 l;

		l = dma_read(CCR(lch));
		l &= ~OMAP_DMA_CCR_EN;
		dma_write(l, CCR(lch));

		/* Clear pending interrupts */
		l = dma_read(CSR(lch));
	} else {
		int i;
		void __iomem *lch_base = omap_dma_base + OMAP_DMA4_CH_BASE(lch);
		for (i = 0; i < 0x44; i += 4)
			__raw_writel(0, lch_base + i);
	}

	local_irq_restore(flags);
}
EXPORT_SYMBOL(omap_clear_dma);

void omap_start_dma(int lch)
{
	u32 l;

	/*
	 * The CPC/CDAC register needs to be initialized to zero
	 * before starting dma transfer.
	 */
	if (d->dma_dev_attr & IS_WORD_16)
		dma_write(0, CPC(lch));
	else
		dma_write(0, CDAC(lch));

	if (!enable_1510_mode && dma_chan[lch].next_lch != -1) {
		int next_lch, cur_lch;
		char dma_chan_link_map[dma_chan_count];

		dma_chan_link_map[lch] = 1;
		/* Set the link register of the first channel */
		if (p->enable_lnk)
			p->enable_lnk(lch);

		memset(dma_chan_link_map, 0, sizeof(dma_chan_link_map));
		cur_lch = dma_chan[lch].next_lch;
		do {
			next_lch = dma_chan[cur_lch].next_lch;

			/* The loop case: we've been here already */
			if (dma_chan_link_map[cur_lch])
				break;
			/* Mark the current channel */
			dma_chan_link_map[cur_lch] = 1;

			if (p->enable_lnk)
				p->enable_lnk(cur_lch);
			if (p->enable_channel_irq)
				p->enable_channel_irq(cur_lch);

			cur_lch = next_lch;
		} while (next_lch != -1);
	}

	if (p->errata & DMA_CHAINING_ERRATA)
		dma_write(lch, CLNK_CTRL(lch));

	if (p->enable_channel_irq)
		p->enable_channel_irq(lch);

	l = dma_read(CCR(lch));

	if (p->errata & DMA_BUFF_DISABLE_ERRATA)
		l |= OMAP_DMA_CCR_EN;

	l |= OMAP_DMA_CCR_EN;
	dma_write(l, CCR(lch));

	dma_chan[lch].flags |= OMAP_DMA_ACTIVE;
}
EXPORT_SYMBOL(omap_start_dma);

void omap_stop_dma(int lch)
{
	u32 l;

	/* Disable all interrupts on the channel */
	if (d->dma_dev_attr & IS_WORD_16)
		dma_write(0, CICR(lch));

	l = dma_read(CCR(lch));
	l &= ~OMAP_DMA_CCR_EN;
	dma_write(l, CCR(lch));

	if (!(enable_1510_mode && (dma_chan[lch].next_lch != -1))) {
		int next_lch, cur_lch = lch;
		char dma_chan_link_map[dma_chan_count];

		memset(dma_chan_link_map, 0, sizeof(dma_chan_link_map));
		do {
			/* The loop case: we've been here already */
			if (dma_chan_link_map[cur_lch])
				break;
			/* Mark the current channel */
			dma_chan_link_map[cur_lch] = 1;

			if (p->disable_lnk)
				p->disable_lnk(cur_lch);

			next_lch = dma_chan[cur_lch].next_lch;
			cur_lch = next_lch;
		} while (next_lch != -1);
	}

	dma_chan[lch].flags &= ~OMAP_DMA_ACTIVE;
}
EXPORT_SYMBOL(omap_stop_dma);

/*
 * Allows changing the DMA callback function or data. This may be needed if
 * the driver shares a single DMA channel for multiple dma triggers.
 */
int omap_set_dma_callback(int lch,
			  void (*callback)(int lch, u16 ch_status, void *data),
			  void *data)
{
	unsigned long flags;

	if (lch < 0)
		return -ENODEV;

	spin_lock_irqsave(&dma_chan_lock, flags);
	if (dma_chan[lch].dev_id == -1) {
		printk(KERN_ERR "DMA callback for not set for free channel\n");
		spin_unlock_irqrestore(&dma_chan_lock, flags);
		return -EINVAL;
	}
	dma_chan[lch].callback = callback;
	dma_chan[lch].data = data;
	spin_unlock_irqrestore(&dma_chan_lock, flags);

	return 0;
}
EXPORT_SYMBOL(omap_set_dma_callback);

/*
 * Returns current physical source address for the given DMA channel.
 * If the channel is running the caller must disable interrupts prior calling
 * this function and process the returned value before re-enabling interrupt to
 * prevent races with the interrupt handler. Note that in continuous mode there
 * is a chance for CSSA_L register overflow inbetween the two reads resulting
 * in incorrect return value.
 */
dma_addr_t omap_get_dma_src_pos(int lch)
{
	dma_addr_t offset = 0;

	if (d->dma_dev_attr & ENABLE_1510_MODE)
		offset = dma_read(CPC(lch));
	else
		offset = dma_read(CSAC(lch));

	if ((p->errata & OMAP3_3_ERRATUM) && !(enable_1510_mode)
				&& (offset == 0))
		offset = dma_read(CSAC(lch));

	if (d->dma_dev_attr & IS_WORD_16)
		offset |= (dma_read(CSSA_U(lch)) << 16);

	return offset;
}
EXPORT_SYMBOL(omap_get_dma_src_pos);

/*
 * Returns current physical destination address for the given DMA channel.
 * If the channel is running the caller must disable interrupts prior calling
 * this function and process the returned value before re-enabling interrupt to
 * prevent races with the interrupt handler. Note that in continuous mode there
 * is a chance for CDSA_L register overflow inbetween the two reads resulting
 * in incorrect return value.
 */
dma_addr_t omap_get_dma_dst_pos(int lch)
{
	dma_addr_t offset = 0;

	if (d->dma_dev_attr & ENABLE_1510_MODE)
		offset = dma_read(CPC(lch));
	else
		offset = dma_read(CDAC(lch));

	if ((p->errata & OMAP3_3_ERRATUM) && !(enable_1510_mode)
				&& (offset == 0))
		offset = dma_read(CDAC(lch));

	if (d->dma_dev_attr & IS_WORD_16)
		offset |= (dma_read(CDSA_U(lch)) << 16);

	return offset;
}
EXPORT_SYMBOL(omap_get_dma_dst_pos);

int omap_get_dma_active_status(int lch)
{
	return (dma_read(CCR(lch)) & OMAP_DMA_CCR_EN) != 0;
}
EXPORT_SYMBOL(omap_get_dma_active_status);

int omap_dma_running(void)
{
	int lch;

	if (d->dma_dev_attr & IS_WORD_16)
		if (omap_lcd_dma_running())
			return 1;

	for (lch = 0; lch < dma_chan_count; lch++)
		if (dma_read(CCR(lch)) & OMAP_DMA_CCR_EN)
			return 1;

	return 0;
}

/*
 * lch_queue DMA will start right after lch_head one is finished.
 * For this DMA link to start, you still need to start (see omap_start_dma)
 * the first one. That will fire up the entire queue.
 */
void omap_dma_link_lch(int lch_head, int lch_queue)
{
	if (enable_1510_mode) {
		if (lch_head == lch_queue) {
			dma_write(dma_read(CCR(lch_head)) | (3 << 8),
								CCR(lch_head));
			return;
		}
		printk(KERN_ERR "DMA linking is not supported in 1510 mode\n");
		BUG();
		return;
	}

	if ((dma_chan[lch_head].dev_id == -1) ||
	    (dma_chan[lch_queue].dev_id == -1)) {
		printk(KERN_ERR "omap_dma: trying to link "
		       "non requested channels\n");
		dump_stack();
	}

	dma_chan[lch_head].next_lch = lch_queue;
}
EXPORT_SYMBOL(omap_dma_link_lch);

/*
 * Once the DMA queue is stopped, we can destroy it.
 */
void omap_dma_unlink_lch(int lch_head, int lch_queue)
{
	if (enable_1510_mode) {
		if (lch_head == lch_queue) {
			dma_write(dma_read(CCR(lch_head)) & ~(3 << 8),
								CCR(lch_head));
			return;
		}
		printk(KERN_ERR "DMA linking is not supported in 1510 mode\n");
		BUG();
		return;
	}

	if (dma_chan[lch_head].next_lch != lch_queue ||
	    dma_chan[lch_head].next_lch == -1) {
		printk(KERN_ERR "omap_dma: trying to unlink "
		       "non linked channels\n");
		dump_stack();
	}

	if ((dma_chan[lch_head].flags & OMAP_DMA_ACTIVE) ||
	    (dma_chan[lch_queue].flags & OMAP_DMA_ACTIVE)) {
		printk(KERN_ERR "omap_dma: You need to stop the DMA channels "
		       "before unlinking\n");
		dump_stack();
	}

	dma_chan[lch_head].next_lch = -1;
}
EXPORT_SYMBOL(omap_dma_unlink_lch);

static int __devinit omap_system_dma_probe(struct platform_device *pdev)
{
	struct omap_system_dma_plat_info *pdata = pdev->dev.platform_data;
	struct resource *mem;
	int ch, ret = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "%s: System DMA initialized without"
			"platform data\n", __func__);
		return -EINVAL;
	}

	pd = pdev;
	p = pdata;
	d = p->dma_attr;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "%s: no mem resource\n", __func__);
		return -EINVAL;
	}

	omap_dma_base = ioremap(mem->start, resource_size(mem));
	if (!omap_dma_base) {
		dev_err(&pdev->dev, "%s: ioremap fail\n", __func__);
		ret = -ENOMEM;
		release_mem_region(mem->start, resource_size(mem));
		return ret;
	}

	if (!(d->dma_dev_attr & IS_WORD_16) && omap_dma_reserve_channels
			&& (omap_dma_reserve_channels <= dma_lch_count))
		d->dma_lch_count = omap_dma_reserve_channels;

	dma_lch_count		= d->dma_lch_count;
	dma_chan_count		= d->dma_chan_count;
	dma_chan		= d->dma_chan;

	pm_runtime_enable(&pd->dev);
	pm_runtime_get_sync(&pd->dev);

	enable_1510_mode = d->dma_dev_attr & ENABLE_1510_MODE;

	if (enable_1510_mode) {
		printk(KERN_INFO "DMA support for OMAP15xx initialized\n");
	} else if (d->dma_dev_attr & IS_WORD_16) {
		printk(KERN_INFO "OMAP DMA hardware version %d\n",
		       dma_read(HW_ID));
		printk(KERN_INFO "DMA capabilities: %08x:%08x:%04x:%04x:%04x\n",
		       (dma_read(CAPS_0_U) << 16) |
		       dma_read(CAPS_0_L),
		       (dma_read(CAPS_1_U) << 16) |
		       dma_read(CAPS_1_L),
		       dma_read(CAPS_2), dma_read(CAPS_3),
		       dma_read(CAPS_4));
		if (!enable_1510_mode) {
			u16 w;

			/* Disable OMAP 3.0/3.1 compatibility mode. */
			w = dma_read(GSCR);
			w |= 1 << 3;
			dma_write(w, GSCR);
		}
	} else {
		u8 revision = dma_read(REVISION) & 0xff;
		printk(KERN_INFO "OMAP DMA hardware revision %d.%d\n",
		       revision >> 4, revision & 0xf);
	}

	spin_lock_init(&dma_chan_lock);
	for (ch = 0; ch < dma_chan_count; ch++) {
		omap_clear_dma(ch);
		if (p->disable_irq_lch) {
			unsigned long flags;
			spin_lock_irqsave(&dma_chan_lock, flags);
			p->disable_irq_lch(ch);
			spin_unlock_irqrestore(&dma_chan_lock, flags);
		}

		dma_chan[ch].dev_id = -1;
		dma_chan[ch].next_lch = -1;
	}

	if (d->dma_dev_attr & GLOBAL_PRIORITY)
		omap_dma_set_global_params(DMA_DEFAULT_ARB_RATE,
				DMA_DEFAULT_FIFO_DEPTH, 0);

	/* reserve dma channels 0 and 1 in high security devices */
	if (d->dma_dev_attr & RESERVE_CHANNEL) {
		printk(KERN_INFO "Reserving DMA channels 0 and 1 for "
				"HS ROM code\n");
		dma_chan[0].dev_id = 0;
		dma_chan[1].dev_id = 1;
	}

	/*
	 * Note: If dma channels are reserved through boot paramters,
	 * then dma device is always enabled.
	 */
	if (!omap_dma_reserve_channels)
		pm_runtime_put(&pd->dev);

	dev_info(&pdev->dev, "System DMA registered\n");
	return 0;
}

static int omap_dma_suspend(struct device *dev)
{
	pm_runtime_put(&pd->dev);

	if (p->dma_context_save)
		p->dma_context_save();

	return 0;

}

static int omap_dma_resume(struct device *dev)
{
	/*
	 * This may not restore sysconfig register if multiple DMA channels
	 * are in use during suspend.
	 * Work around: restroing sysconfig manually in machine specific dma
	 * driver.
	 */
	pm_runtime_get_sync(&pd->dev);

	if (p->dma_context_restore)
		p->dma_context_restore();

	return 0;

}

static const struct dev_pm_ops dma_pm_ops = {
	.suspend	 = omap_dma_suspend,
	.resume		 = omap_dma_resume,
};

static int __devexit omap_system_dma_remove(struct platform_device *pdev)
{
	struct resource *mem;
	iounmap(omap_dma_base);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));
	return 0;
}

static struct platform_driver omap_system_dma_driver = {
	.probe		= omap_system_dma_probe,
	.remove		= omap_system_dma_remove,
	.driver		= {
		.name	= "dma",
		.pm	= &dma_pm_ops,
	},
};

static int __init omap_system_dma_init(void)
{
	return platform_driver_register(&omap_system_dma_driver);
}

arch_initcall(omap_system_dma_init);

static void __exit omap_system_dma_exit(void)
{
	platform_driver_unregister(&omap_system_dma_driver);
}

MODULE_DESCRIPTION("OMAP SYSTEM DMA DRIVER");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");

/*
 * Reserve the omap SDMA channels using cmdline bootarg
 * "omap_dma_reserve_ch=". The valid range is 1 to 32
 */
static int __init omap_dma_cmdline_reserve_ch(char *str)
{
	if (get_option(&str, &omap_dma_reserve_channels) != 1)
		omap_dma_reserve_channels = 0;
	return 1;
}

__setup("omap_dma_reserve_ch=", omap_dma_cmdline_reserve_ch);


