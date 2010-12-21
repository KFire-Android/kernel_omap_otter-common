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

#include "pm.h"

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

static struct global_context_registers {
	u32 sysconfig;
	u32 irqenable_l0;
	u32 gcr;
} global_ctx_regs;

struct dma_link_info {
	int *linked_dmach_q;
	int no_of_lchs_linked;

	int q_count;
	int q_tail;
	int q_head;

	int chain_state;
	int chain_mode;

};

static u32 *ch_ctx_regs;
static u8 ch_spec_regs;
static struct dma_link_info *dma_linked_lch;
static struct omap_dma_lch *dma_chan;

static int dma_chan_count;

enum { DMA_CH_ALLOC_DONE, DMA_CH_PARAMS_SET_DONE, DMA_CH_STARTED,
	DMA_CH_QUEUED, DMA_CH_NOTSTARTED, DMA_CH_PAUSED, DMA_CH_LINK_ENABLED
};

enum { DMA_CHAIN_STARTED, DMA_CHAIN_NOTSTARTED };

/* Chain handling macros */
#define OMAP_DMA_CHAIN_QINIT(chain_id)					\
	do {								\
		dma_linked_lch[chain_id].q_head =			\
		dma_linked_lch[chain_id].q_tail =			\
		dma_linked_lch[chain_id].q_count = 0;			\
	} while (0)
#define OMAP_DMA_CHAIN_QFULL(chain_id)					\
		(dma_linked_lch[chain_id].no_of_lchs_linked ==		\
		dma_linked_lch[chain_id].q_count)
#define OMAP_DMA_CHAIN_QLAST(chain_id)					\
	do {								\
		((dma_linked_lch[chain_id].no_of_lchs_linked-1) ==	\
		dma_linked_lch[chain_id].q_count)			\
	} while (0)
#define OMAP_DMA_CHAIN_QEMPTY(chain_id)					\
		(0 == dma_linked_lch[chain_id].q_count)
#define __OMAP_DMA_CHAIN_INCQ(end)					\
	((end) = ((end)+1) % dma_linked_lch[chain_id].no_of_lchs_linked)
#define OMAP_DMA_CHAIN_INCQHEAD(chain_id)				\
	do {								\
		__OMAP_DMA_CHAIN_INCQ(dma_linked_lch[chain_id].q_head);	\
		dma_linked_lch[chain_id].q_count--;			\
	} while (0)

#define OMAP_DMA_CHAIN_INCQTAIL(chain_id)				\
	do {								\
		__OMAP_DMA_CHAIN_INCQ(dma_linked_lch[chain_id].q_tail);	\
		dma_linked_lch[chain_id].q_count++; \
	} while (0)

static struct omap_dma_dev_attr *d;
static void __iomem *dma_base;
static struct omap_system_dma_plat_info *p;
static int dma_caps0_status;

static struct omap_device_pm_latency omap2_dma_latency[] = {
	{
	.deactivate_func = omap_device_idle_hwmods,
	.activate_func	 = omap_device_enable_hwmods,
	.flags		 = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static inline void omap2_enable_irq_lch(int lch)
{
	u32 val;

	val = dma_read(IRQENABLE_L0);
	val |= 1 << lch;
	dma_write(val, IRQENABLE_L0);
}

static inline void omap2_disable_irq_lch(int lch)
{
	u32 val;

	val = dma_read(IRQENABLE_L0);
	val &= ~(1 << lch);
	dma_write(val, IRQENABLE_L0);
}

static inline void omap2_enable_channel_irq(int lch)
{
	/* Clear CSR */
	dma_write(OMAP2_DMA_CSR_CLEAR_MASK, CSR(lch));

	dma_write(dma_chan[lch].enabled_irqs, CICR(lch));
}

static void omap2_disable_channel_irq(int lch)
{
	dma_write(0, CICR(lch));
}

static inline void omap2_enable_lnk(int lch)
{
	u32 l;

	l = dma_read(CLNK_CTRL(lch));

	/* Set the ENABLE_LNK bits */
	if (dma_chan[lch].next_lch != -1)
		l = dma_chan[lch].next_lch | (1 << 15);

	if (dma_chan[lch].next_linked_ch != -1)
		l = dma_chan[lch].next_linked_ch | (1 << 15);

	dma_write(l, CLNK_CTRL(lch));
}

static inline void omap2_disable_lnk(int lch)
{
	u32 l;

	l = dma_read(CLNK_CTRL(lch));

	omap2_disable_channel_irq(lch);
	/* Clear the ENABLE_LNK bit */
	l &= ~(1 << 15);

	dma_write(l, CLNK_CTRL(lch));
	dma_chan[lch].flags &= ~OMAP_DMA_ACTIVE;
}

static void dma_ocpsysconfig_errata(u32 *sys_cf, bool flag)
{
	u32 l;

	/*
	 * DMA Errata:
	 * Special programming model needed to disable DMA before end of block
	 */
	if (!flag) {
		*sys_cf = dma_read(OCP_SYSCONFIG);
		l = *sys_cf;
		/* Middle mode reg set no Standby */
		l &= ~((1 << 12)|(1 << 13));
		dma_write(l, OCP_SYSCONFIG);
	} else
		/* put back old value */
		dma_write(*sys_cf, OCP_SYSCONFIG);
}

static inline void omap_dma_list_set_ntype(struct omap_dma_sglist_node *node,
					   int value)
{
	node->num_of_elem |= ((value) << 29);
}

static void omap_set_dma_sglist_pausebit(
		struct omap_dma_list_config_params *lcfg, int nelem, int set)
{
	struct omap_dma_sglist_node *sgn = lcfg->sghead;

	if (nelem > 0 && nelem < lcfg->num_elem) {
		lcfg->pausenode = nelem;
		sgn += nelem;

		if (set)
			sgn->next_desc_add_ptr |= DMA_LIST_DESC_PAUSE;
		else
			sgn->next_desc_add_ptr &= ~(DMA_LIST_DESC_PAUSE);
	}
}

static int dma_sglist_set_phy_params(struct omap_dma_sglist_node *sghead,
		dma_addr_t phyaddr, int nelem)
{
	struct omap_dma_sglist_node *sgcurr, *sgprev;
	dma_addr_t elem_paddr = phyaddr;

	for (sgprev = sghead;
		sgprev < sghead + nelem;
		sgprev++) {

		sgcurr = sgprev + 1;
		sgprev->next = sgcurr;
		elem_paddr += (int)sizeof(*sgcurr);
		sgprev->next_desc_add_ptr = elem_paddr;

		switch (sgcurr->desc_type) {
		case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE1:
			omap_dma_list_set_ntype(sgprev, 1);
			break;

		case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE2a:
		/* intentional no break */
		case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE2b:
			omap_dma_list_set_ntype(sgprev, 2);
			break;

		case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE3a:
			/* intentional no break */
		case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE3b:
			omap_dma_list_set_ntype(sgprev, 3);
			break;

		default:
			return -EINVAL;

		}
		if (sgcurr->flags & OMAP_DMA_LIST_SRC_VALID)
			sgprev->num_of_elem |= DMA_LIST_DESC_SRC_VALID;
		if (sgcurr->flags & OMAP_DMA_LIST_DST_VALID)
			sgprev->num_of_elem |= DMA_LIST_DESC_DST_VALID;
		if (sgcurr->flags & OMAP_DMA_LIST_NOTIFY_BLOCK_END)
			sgprev->num_of_elem |= DMA_LIST_DESC_BLK_END;
	}
	sgprev--;
	sgprev->next_desc_add_ptr = OMAP_DMA_INVALID_DESCRIPTOR_POINTER;
	return 0;
}

void omap2_dma_context_save(void)
{
	int ch, i, ch_count = 0;

#ifdef CONFIG_PM
	if (!enable_off_mode)
		return;
#endif
	/*
	 * TODO: sysconfig setting should be part of run time pm.
	 * Current pm runtime frame work will not restore this register
	 * if multiple DMA channels are in use.
	 */
	global_ctx_regs.sysconfig        = dma_read(OCP_SYSCONFIG);
	global_ctx_regs.irqenable_l0 = dma_read(IRQENABLE_L0);
	global_ctx_regs.gcr          = dma_read(GCR);

	for (ch = 0; ch < dma_chan_count; ch++) {
		void __iomem *lch_base = dma_base + OMAP_DMA4_CH_BASE(ch);
		if (dma_chan[ch].dev_id == -1)
			continue;
		for (i = 0; i <= ch_spec_regs; i++)
			ch_ctx_regs[ch_count++] = __raw_readl(lch_base + i);
	}
}
EXPORT_SYMBOL(omap2_dma_context_save);

void omap2_dma_context_restore(void)
{
	int ch, i, ch_count = 0;

#ifdef CONFIG_PM
	if (!enable_off_mode)
		return;
#endif
	/*
	 * TODO: sysconfig setting should be part of run time pm.
	 * Current pm runtime frame work will not restore this register
	 * if multiple DMA channels are in use.
	 */
	dma_write(global_ctx_regs.sysconfig, OCP_SYSCONFIG);
	dma_write(global_ctx_regs.gcr, GCR);
	dma_write(global_ctx_regs.irqenable_l0, IRQENABLE_L0);

	for (ch = 0; ch < dma_chan_count; ch++) {
		void __iomem *lch_base = dma_base + OMAP_DMA4_CH_BASE(ch);
		if (dma_chan[ch].dev_id == -1)
			continue;
		for (i = 0; i <= ch_spec_regs; i++)
			__raw_writel(ch_ctx_regs[ch_count++], lch_base + i);
	}

	/*
	 * A bug in ROM code leaves IRQ status for channels 0 and 1 uncleared
	 * after secure sram context save and restore. Hence we need to
	 * manually clear those IRQs to avoid spurious interrupts. This
	 * affects only secure devices.
	 */
	if (cpu_is_omap34xx() && (omap_type() != OMAP2_DEVICE_TYPE_GP))
		dma_write(0x3 , IRQSTATUS_L0);

}
EXPORT_SYMBOL(omap2_dma_context_restore);

/* Create chain of DMA channesls */
static void create_dma_lch_chain(int lch_head, int lch_queue)
{
	u32 l;

	/* Check if this is the first link in chain */
	if (dma_chan[lch_head].next_linked_ch == -1) {
		dma_chan[lch_head].next_linked_ch = lch_queue;
		dma_chan[lch_head].prev_linked_ch = lch_queue;
		dma_chan[lch_queue].next_linked_ch = lch_head;
		dma_chan[lch_queue].prev_linked_ch = lch_head;
	}

	/* a link exists, link the new channel in circular chain */
	else {
		dma_chan[lch_queue].next_linked_ch =
					dma_chan[lch_head].next_linked_ch;
		dma_chan[lch_queue].prev_linked_ch = lch_head;
		dma_chan[lch_head].next_linked_ch = lch_queue;
		dma_chan[dma_chan[lch_queue].next_linked_ch].prev_linked_ch =
					lch_queue;
	}

	l = dma_read(CLNK_CTRL(lch_head));
	l &= ~(0x1f);
	l |= lch_queue;
	dma_write(l, CLNK_CTRL(lch_head));

	l = dma_read(CLNK_CTRL(lch_queue));
	l &= ~(0x1f);
	l |= (dma_chan[lch_queue].next_linked_ch);
	dma_write(l, CLNK_CTRL(lch_queue));
}

static void set_dma_chain_ch(int free_ch)
{
	dma_chan[free_ch].chain_id       = -1;
	dma_chan[free_ch].next_linked_ch = -1;
}

/**
 * @brief omap_request_dma_chain : Request a chain of DMA channels
 *
 * @param dev_id - Device id using the dma channel
 * @param dev_name - Device name
 * @param callback - Call back function
 * @chain_id -
 * @no_of_chans - Number of channels requested
 * @chain_mode - Dynamic or static chaining :	OMAP_DMA_STATIC_CHAIN
 *						OMAP_DMA_DYNAMIC_CHAIN
 * @params - Channel parameters
 *
 * @return -	Success : 0
 *		Failure : -EINVAL/-ENOMEM
 */
int omap_request_dma_chain(int dev_id, const char *dev_name,
			   void (*callback) (int lch, u16 ch_status,
					     void *data),
			   int *chain_id, int no_of_chans, int chain_mode,
			   struct omap_dma_channel_params params)
{
	int *channels;
	int i, err;

	/* Is the chain mode valid ? */
	if (chain_mode != OMAP_DMA_STATIC_CHAIN
			&& chain_mode != OMAP_DMA_DYNAMIC_CHAIN) {
		printk(KERN_ERR "Invalid chain mode requested\n");
		return -EINVAL;
	}

	if (unlikely((no_of_chans < 1
			|| no_of_chans > dma_chan_count))) {
		printk(KERN_ERR "Invalid Number of channels requested\n");
		return -EINVAL;
	}

	/*
	 * Allocate a queue to maintain the status of the channels
	 * in the chain
	 */
	channels = kmalloc(sizeof(*channels) * no_of_chans, GFP_KERNEL);
	if (channels == NULL) {
		printk(KERN_ERR "omap_dma: No memory for channel queue\n");
		return -ENOMEM;
	}

	/* request and reserve DMA channels for the chain */
	for (i = 0; i < no_of_chans; i++) {
		err = omap_request_dma(dev_id, dev_name,
					callback, NULL, &channels[i]);
		if (err < 0) {
			int j;
			for (j = 0; j < i; j++)
				omap_free_dma(channels[j]);
			kfree(channels);
			printk(KERN_ERR "omap_dma: Request failed %d\n", err);
			return err;
		}
		dma_chan[channels[i]].prev_linked_ch = -1;
		dma_chan[channels[i]].state = DMA_CH_NOTSTARTED;

		/*
		 * Allowing client drivers to set common parameters now,
		 * so that later only relevant (src_start, dest_start
		 * and element count) can be set
		 */
		omap_set_dma_params(channels[i], &params);
	}

	*chain_id = channels[0];
	dma_linked_lch[*chain_id].linked_dmach_q = channels;
	dma_linked_lch[*chain_id].chain_mode = chain_mode;
	dma_linked_lch[*chain_id].chain_state = DMA_CHAIN_NOTSTARTED;
	dma_linked_lch[*chain_id].no_of_lchs_linked = no_of_chans;

	for (i = 0; i < no_of_chans; i++)
		dma_chan[channels[i]].chain_id = *chain_id;

	/* Reset the Queue pointers */
	OMAP_DMA_CHAIN_QINIT(*chain_id);

	/* Set up the chain */
	if (no_of_chans == 1)
		create_dma_lch_chain(channels[0], channels[0]);
	else {
		for (i = 0; i < (no_of_chans - 1); i++)
			create_dma_lch_chain(channels[i], channels[i + 1]);
	}

	return 0;
}
EXPORT_SYMBOL(omap_request_dma_chain);

/**
 * @brief omap_modify_dma_chain_param : Modify the chain's params - Modify the
 * params after setting it. Dont do this while dma is running!!
 *
 * @param chain_id - Chained logical channel id.
 * @param params
 *
 * @return -	Success : 0
 *		Failure : -EINVAL
 */
int omap_modify_dma_chain_params(int chain_id,
				struct omap_dma_channel_params params)
{
	int *channels;
	u32 i;

	/* Check for input params */
	if (unlikely((chain_id < 0
			|| chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exists\n");
		return -EINVAL;
	}
	channels = dma_linked_lch[chain_id].linked_dmach_q;

	for (i = 0; i < dma_linked_lch[chain_id].no_of_lchs_linked; i++) {
		/*
		 * Allowing client drivers to set common parameters now,
		 * so that later only relevant (src_start, dest_start
		 * and element count) can be set
		 */
		omap_set_dma_params(channels[i], &params);
	}

	return 0;
}
EXPORT_SYMBOL(omap_modify_dma_chain_params);

/**
 * @brief omap_free_dma_chain - Free all the logical channels in a chain.
 *
 * @param chain_id
 *
 * @return -	Success : 0
 *		Failure : -EINVAL
 */
int omap_free_dma_chain(int chain_id)
{
	int *channels;
	u32 i;

	/* Check for input params */
	if (unlikely((chain_id < 0 || chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exists\n");
		return -EINVAL;
	}

	channels = dma_linked_lch[chain_id].linked_dmach_q;
	for (i = 0; i < dma_linked_lch[chain_id].no_of_lchs_linked; i++) {
		dma_chan[channels[i]].next_linked_ch = -1;
		dma_chan[channels[i]].prev_linked_ch = -1;
		dma_chan[channels[i]].chain_id = -1;
		dma_chan[channels[i]].state = DMA_CH_NOTSTARTED;
		omap_free_dma(channels[i]);
	}

	kfree(channels);

	dma_linked_lch[chain_id].linked_dmach_q = NULL;
	dma_linked_lch[chain_id].chain_mode = -1;
	dma_linked_lch[chain_id].chain_state = -1;

	return 0;
}
EXPORT_SYMBOL(omap_free_dma_chain);

/**
 * @brief omap_dma_chain_status - Check if the chain is in
 * active / inactive state.
 * @param chain_id
 *
 * @return -	Success : OMAP_DMA_CHAIN_ACTIVE/OMAP_DMA_CHAIN_INACTIVE
 *		Failure : -EINVAL
 */
int omap_dma_chain_status(int chain_id)
{
	/* Check for input params */
	if (unlikely((chain_id < 0 || chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exists\n");
		return -EINVAL;
	}
	pr_debug("CHAINID=%d, qcnt=%d\n", chain_id,
			dma_linked_lch[chain_id].q_count);

	if (OMAP_DMA_CHAIN_QEMPTY(chain_id))
		return OMAP_DMA_CHAIN_INACTIVE;

	return OMAP_DMA_CHAIN_ACTIVE;
}
EXPORT_SYMBOL(omap_dma_chain_status);

/**
 * @brief omap_dma_chain_a_transfer - Get a free channel from a chain,
 * set the params and start the transfer.
 *
 * @param chain_id
 * @param src_start - buffer start address
 * @param dest_start - Dest address
 * @param elem_count
 * @param frame_count
 * @param callbk_data - channel callback parameter data.
 *
 * @return -	Success : OMAP_DMA_CHAIN_ACTIVE/OMAP_DMA_CHAIN_INACTIVE
 *		Failure : -EINVAL
 */
int omap_dma_chain_a_transfer(int chain_id, int src_start, int dest_start,
			int elem_count, int frame_count, void *callbk_data)
{
	int *channels;
	u32 l, lch;
	int start_dma = 0;

	/*
	 * if buffer size is less than 1 then there is
	 * no use of starting the chain
	 */
	if (elem_count < 1) {
		printk(KERN_ERR "Invalid buffer size\n");
		return -EINVAL;
	}

	/* Check for input params */
	if (unlikely((chain_id < 0
			|| chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exist\n");
		return -EINVAL;
	}

	/* Check if all the channels in chain are in use */
	if (OMAP_DMA_CHAIN_QFULL(chain_id))
		return -EBUSY;

	/* Frame count may be negative in case of indexed transfers */
	channels = dma_linked_lch[chain_id].linked_dmach_q;

	/* Get a free channel */
	lch = channels[dma_linked_lch[chain_id].q_tail];

	/* Store the callback data */
	dma_chan[lch].data = callbk_data;

	/* Increment the q_tail */
	OMAP_DMA_CHAIN_INCQTAIL(chain_id);

	/* Set the params to the free channel */
	if (src_start != 0)
		dma_write(src_start, CSSA(lch));
	if (dest_start != 0)
		dma_write(dest_start, CDSA(lch));

	/* Write the buffer size */
	dma_write(elem_count, CEN(lch));
	dma_write(frame_count, CFN(lch));

	/*
	 * If the chain is dynamically linked,
	 * then we may have to start the chain if its not active
	 */
	if (dma_linked_lch[chain_id].chain_mode == OMAP_DMA_DYNAMIC_CHAIN) {

		/*
		 * In Dynamic chain, if the chain is not started,
		 * queue the channel
		 */
		if (dma_linked_lch[chain_id].chain_state ==
						DMA_CHAIN_NOTSTARTED) {
			/* Enable the link in previous channel */
			if (dma_chan[dma_chan[lch].prev_linked_ch].state ==
								DMA_CH_QUEUED)
				omap2_enable_lnk(dma_chan[lch].prev_linked_ch);
			dma_chan[lch].state = DMA_CH_QUEUED;
		}

		/*
		 * Chain is already started, make sure its active,
		 * if not then start the chain
		 */
		else {
			start_dma = 1;

			if (dma_chan[dma_chan[lch].prev_linked_ch].state ==
							DMA_CH_STARTED) {
				omap2_enable_lnk(dma_chan[lch].prev_linked_ch);
				dma_chan[lch].state = DMA_CH_QUEUED;
				start_dma = 0;
				if (0 == ((1 << 7) & dma_read(
					CCR(dma_chan[lch].prev_linked_ch)))) {
					omap2_disable_lnk(dma_chan[lch].
						    prev_linked_ch);
					pr_debug("\n prev ch is stopped\n");
					start_dma = 1;
				}
			}

			else if (dma_chan[dma_chan[lch].prev_linked_ch].state
							== DMA_CH_QUEUED) {
				omap2_enable_lnk(dma_chan[lch].prev_linked_ch);
				dma_chan[lch].state = DMA_CH_QUEUED;
				start_dma = 0;
			}
			omap2_enable_channel_irq(lch);

			l = dma_read(CCR(lch));

			if ((0 == (l & (1 << 24))))
				l &= ~(1 << 25);
			else
				l |= (1 << 25);
			if (start_dma == 1) {
				if (0 == (l & (1 << 7))) {
					l |= (1 << 7);
					dma_chan[lch].state = DMA_CH_STARTED;
					pr_debug("starting %d\n", lch);
					dma_write(l, CCR(lch));
				} else
					start_dma = 0;
			} else {
				if (0 == (l & (1 << 7)))
					dma_write(l, CCR(lch));
			}
			dma_chan[lch].flags |= OMAP_DMA_ACTIVE;
		}
	}

	return 0;
}
EXPORT_SYMBOL(omap_dma_chain_a_transfer);

/**
 * @brief omap_start_dma_chain_transfers - Start the chain
 *
 * @param chain_id
 *
 * @return -	Success : OMAP_DMA_CHAIN_ACTIVE/OMAP_DMA_CHAIN_INACTIVE
 *		Failure : -EINVAL
 */
int omap_start_dma_chain_transfers(int chain_id)
{
	int *channels;
	u32 l, i;

	if (unlikely((chain_id < 0 || chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	channels = dma_linked_lch[chain_id].linked_dmach_q;

	if (dma_linked_lch[channels[0]].chain_state == DMA_CHAIN_STARTED) {
		printk(KERN_ERR "Chain is already started\n");
		return -EBUSY;
	}

	if (dma_linked_lch[chain_id].chain_mode == OMAP_DMA_STATIC_CHAIN) {
		for (i = 0; i < dma_linked_lch[chain_id].no_of_lchs_linked;
									i++) {
			omap2_enable_lnk(channels[i]);
			omap2_enable_channel_irq(channels[i]);
		}
	} else {
		omap2_enable_channel_irq(channels[0]);
	}

	l = dma_read(CCR(channels[0]));
	l |= (1 << 7);
	dma_linked_lch[chain_id].chain_state = DMA_CHAIN_STARTED;
	dma_chan[channels[0]].state = DMA_CH_STARTED;

	if ((0 == (l & (1 << 24))))
		l &= ~(1 << 25);
	else
		l |= (1 << 25);
	dma_write(l, CCR(channels[0]));

	dma_chan[channels[0]].flags |= OMAP_DMA_ACTIVE;

	return 0;
}
EXPORT_SYMBOL(omap_start_dma_chain_transfers);

/**
 * @brief omap_stop_dma_chain_transfers - Stop the dma transfer of a chain.
 *
 * @param chain_id
 *
 * @return -	Success : OMAP_DMA_CHAIN_ACTIVE/OMAP_DMA_CHAIN_INACTIVE
 *		Failure : -EINVAL
 */
int omap_stop_dma_chain_transfers(int chain_id)
{
	int *channels;
	u32 l, i;
	u32 get_sysconfig;

	/* Check for input params */
	if (unlikely((chain_id < 0 || chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exists\n");
		return -EINVAL;
	}
	channels = dma_linked_lch[chain_id].linked_dmach_q;

	if (p->errata & DMA_SYSCONFIG_ERRATA)
		dma_ocpsysconfig_errata(&get_sysconfig, false);

	for (i = 0; i < dma_linked_lch[chain_id].no_of_lchs_linked; i++) {

		/* Stop the Channel transmission */
		l = dma_read(CCR(channels[i]));
		l &= ~(1 << 7);
		dma_write(l, CCR(channels[i]));

		/* Disable the link in all the channels */
		omap2_disable_lnk(channels[i]);
		dma_chan[channels[i]].state = DMA_CH_NOTSTARTED;

	}
	dma_linked_lch[chain_id].chain_state = DMA_CHAIN_NOTSTARTED;

	/* Reset the Queue pointers */
	OMAP_DMA_CHAIN_QINIT(chain_id);

	if (p->errata & DMA_SYSCONFIG_ERRATA)
		dma_ocpsysconfig_errata(&get_sysconfig, true);

	return 0;
}
EXPORT_SYMBOL(omap_stop_dma_chain_transfers);

/* Get the index of the ongoing DMA in chain */
/**
 * @brief omap_get_dma_chain_index - Get the element and frame index
 * of the ongoing DMA in chain
 *
 * @param chain_id
 * @param ei - Element index
 * @param fi - Frame index
 *
 * @return -	Success : OMAP_DMA_CHAIN_ACTIVE/OMAP_DMA_CHAIN_INACTIVE
 *		Failure : -EINVAL
 */
int omap_get_dma_chain_index(int chain_id, int *ei, int *fi)
{
	int lch;
	int *channels;

	/* Check for input params */
	if (unlikely((chain_id < 0 || chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exists\n");
		return -EINVAL;
	}
	if ((!ei) || (!fi))
		return -EINVAL;

	channels = dma_linked_lch[chain_id].linked_dmach_q;

	/* Get the current channel */
	lch = channels[dma_linked_lch[chain_id].q_head];

	*ei = dma_read(CCEN(lch));
	*fi = dma_read(CCFN(lch));

	return 0;
}
EXPORT_SYMBOL(omap_get_dma_chain_index);

/**
 * @brief omap_get_dma_chain_dst_pos - Get the destination position of the
 * ongoing DMA in chain
 *
 * @param chain_id
 *
 * @return -	Success : OMAP_DMA_CHAIN_ACTIVE/OMAP_DMA_CHAIN_INACTIVE
 *		Failure : -EINVAL
 */
int omap_get_dma_chain_dst_pos(int chain_id)
{
	int lch;
	int *channels;

	/* Check for input params */
	if (unlikely((chain_id < 0 || chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exists\n");
		return -EINVAL;
	}

	channels = dma_linked_lch[chain_id].linked_dmach_q;

	/* Get the current channel */
	lch = channels[dma_linked_lch[chain_id].q_head];

	return dma_read(CDAC(lch));
}
EXPORT_SYMBOL(omap_get_dma_chain_dst_pos);

/**
 * @brief omap_get_dma_chain_src_pos - Get the source position
 * of the ongoing DMA in chain
 * @param chain_id
 *
 * @return -	Success : OMAP_DMA_CHAIN_ACTIVE/OMAP_DMA_CHAIN_INACTIVE
 *		Failure : -EINVAL
 */
int omap_get_dma_chain_src_pos(int chain_id)
{
	int lch;
	int *channels;

	/* Check for input params */
	if (unlikely((chain_id < 0 || chain_id >= dma_chan_count))) {
		printk(KERN_ERR "Invalid chain id\n");
		return -EINVAL;
	}

	/* Check if the chain exists */
	if (dma_linked_lch[chain_id].linked_dmach_q == NULL) {
		printk(KERN_ERR "Chain doesn't exists\n");
		return -EINVAL;
	}

	channels = dma_linked_lch[chain_id].linked_dmach_q;

	/* Get the current channel */
	lch = channels[dma_linked_lch[chain_id].q_head];

	return dma_read(CSAC(lch));
}
EXPORT_SYMBOL(omap_get_dma_chain_src_pos);

void omap_set_dma_write_mode(int lch, enum omap_dma_write_mode mode)
{
	u32 csdp;

	csdp = dma_read(CSDP(lch));
	csdp &= ~(0x3 << 16);
	csdp |= (mode << 16);
	dma_write(csdp, CSDP(lch));
}
EXPORT_SYMBOL(omap_set_dma_write_mode);

int omap_set_dma_sglist_mode(int lch, struct omap_dma_sglist_node *sgparams,
	dma_addr_t padd, int nelem, struct omap_dma_channel_params *chparams)
{
	struct omap_dma_list_config_params *lcfg;
	int l = DMA_LIST_CDP_LISTMODE; /* Enable Linked list mode in CDP */

	if ((dma_caps0_status & DMA_CAPS_SGLIST_SUPPORT) == 0) {
		printk(KERN_ERR "omap DMA: sglist feature not supported\n");
		return -EPERM;
	}
	if (dma_chan[lch].flags & OMAP_DMA_ACTIVE) {
		printk(KERN_ERR "omap DMA: configuring active DMA channel\n");
		return -EPERM;
	}

	if (padd == 0) {
		printk(KERN_ERR "omap DMA: sglist invalid dma_addr\n");
		return -EINVAL;
	}
	lcfg = &dma_chan[lch].list_config;

	lcfg->sghead = sgparams;
	lcfg->num_elem = nelem;
	lcfg->sgheadphy = padd;
	lcfg->pausenode = -1;


	if (NULL == chparams)
		l |= DMA_LIST_CDP_FASTMODE;
	else
		omap_set_dma_params(lch, chparams);

	dma_write(l, CDP(lch));
	dma_write(0, CCDN(lch)); /* Reset List index numbering */
	/* Initialize frame and element counters to invalid values */
	dma_write(OMAP_DMA_INVALID_FRAME_COUNT, CCFN(lch));
	dma_write(OMAP_DMA_INVALID_ELEM_COUNT, CCEN(lch));

	return dma_sglist_set_phy_params(sgparams, lcfg->sgheadphy, nelem);

}
EXPORT_SYMBOL(omap_set_dma_sglist_mode);

void omap_clear_dma_sglist_mode(int lch)
{
	/* Clear entire CDP which is related to sglist handling */
	dma_write(0, CDP(lch));
	dma_write(0, CCDN(lch));
	/**
	 * Put back the original enabled irqs, which
	 * could have been overwritten by type 1 or type 2
	 * descriptors
	 */
	dma_write(dma_chan[lch].enabled_irqs, CICR(lch));
	return;
}
EXPORT_SYMBOL(omap_clear_dma_sglist_mode);

int omap_start_dma_sglist_transfers(int lch, int pauseafter)
{
	struct omap_dma_list_config_params *lcfg;
	struct omap_dma_sglist_node *sgn;
	unsigned int l, type_id;

	lcfg = &dma_chan[lch].list_config;
	sgn = lcfg->sghead;

	lcfg->pausenode = 0;
	omap_set_dma_sglist_pausebit(lcfg, pauseafter, 1);

	/* Program the head descriptor's properties into CDP */
	switch (lcfg->sghead->desc_type) {
	case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE1:
		type_id = DMA_LIST_CDP_TYPE1;
		break;
	case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE2a:
	case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE2b:
		type_id = DMA_LIST_CDP_TYPE2;
		break;
	case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE3a:
	case OMAP_DMA_SGLIST_DESCRIPTOR_TYPE3b:
		type_id = DMA_LIST_CDP_TYPE3;
		break;
	default:
		return -EINVAL;
	}

	l = dma_read(CDP(lch));
	l |= type_id;
	if (lcfg->sghead->flags & OMAP_DMA_LIST_SRC_VALID)
		l |= DMA_LIST_CDP_SRC_VALID;
	if (lcfg->sghead->flags & OMAP_DMA_LIST_DST_VALID)
		l |= DMA_LIST_CDP_DST_VALID;

	dma_write(l, CDP(lch));
	dma_write((lcfg->sgheadphy), CNDP(lch));
	/**
	 * Barrier needed as writes to the
	 * descriptor memory needs to be flushed
	 * before it's used by DMA controller
	 */
	wmb();
	omap_start_dma(lch);

	return 0;
}
EXPORT_SYMBOL(omap_start_dma_sglist_transfers);

int omap_resume_dma_sglist_transfers(int lch, int pauseafter)
{
	struct omap_dma_list_config_params *lcfg;
	struct omap_dma_sglist_node *sgn;
	int l, sys_cf;

	lcfg = &dma_chan[lch].list_config;
	sgn = lcfg->sghead;

	/* Maintain the pause state in descriptor */
	omap_set_dma_sglist_pausebit(lcfg, lcfg->pausenode, 0);
	omap_set_dma_sglist_pausebit(lcfg, pauseafter, 1);

	/**
	 * Barrier needed as writes to the
	 * descriptor memory needs to be flushed
	 * before it's used by DMA controller
	 */
	wmb();

	/* Errata i557 - pausebit should be cleared in no standby mode */
	sys_cf = dma_read(OCP_SYSCONFIG);
	l = sys_cf;
	/* Middle mode reg set no Standby */
	l &= ~(BIT(12) | BIT(13));
	dma_write(l, OCP_SYSCONFIG);

	/* Clear pause bit in CDP */
	l = dma_read(CDP(lch));
	l &= ~(DMA_LIST_CDP_PAUSEMODE);
	dma_write(l, CDP(lch));

	omap_start_dma(lch);

	/* Errata i557 - put in the old value */
	dma_write(sys_cf, OCP_SYSCONFIG);
	return 0;
}
EXPORT_SYMBOL(omap_resume_dma_sglist_transfers);

void omap_release_dma_sglist(int lch)
{
	omap_clear_dma_sglist_mode(lch);
	omap_free_dma(lch);

	return;
}
EXPORT_SYMBOL(omap_release_dma_sglist);

int omap_get_completed_sglist_nodes(int lch)
{
	int list_count;

	list_count = dma_read(CCDN(lch));
	return list_count & 0xffff; /* only 16 LSB bits are valid */
}
EXPORT_SYMBOL(omap_get_completed_sglist_nodes);

int omap_dma_sglist_is_paused(int lch)
{
	int list_state;
	list_state = dma_read(CDP(lch));
	return (list_state & DMA_LIST_CDP_PAUSEMODE) ? 1 : 0;
}
EXPORT_SYMBOL(omap_dma_sglist_is_paused);

void omap_dma_set_sglist_fastmode(int lch, int fastmode)
{
	int l = dma_read(CDP(lch));

	if (fastmode)
		l |= DMA_LIST_CDP_FASTMODE;
	else
		l &= ~(DMA_LIST_CDP_FASTMODE);
	dma_write(l, CDP(lch));
}
EXPORT_SYMBOL(omap_dma_set_sglist_fastmode);

static int omap2_dma_handle_ch(int ch)
{
	u32 status = dma_read(CSR(ch));

	if (!status) {
		if (printk_ratelimit())
			printk(KERN_WARNING "Spurious DMA IRQ for lch %d\n",
				ch);
		dma_write(1 << ch, IRQSTATUS_L0);
		return 0;
	}
	if (unlikely(dma_chan[ch].dev_id == -1)) {
		if (printk_ratelimit())
			printk(KERN_WARNING "IRQ %04x for non-allocated DMA"
					"channel %d\n", status, ch);
		return 0;
	}
	if (unlikely(status & OMAP_DMA_DROP_IRQ))
		printk(KERN_INFO
		       "DMA synchronization event drop occurred with device "
		       "%d\n", dma_chan[ch].dev_id);
	if (unlikely(status & OMAP2_DMA_TRANS_ERR_IRQ)) {
		printk(KERN_INFO "DMA transaction error with device %d\n",
		       dma_chan[ch].dev_id);
		if (cpu_class_is_omap2()) {
			/*
			 * Errata: sDMA Channel is not disabled
			 * after a transaction error. So we explicitely
			 * disable the channel
			 */
			u32 ccr;

			ccr = dma_read(CCR(ch));
			ccr &= ~OMAP_DMA_CCR_EN;
			dma_write(ccr, CCR(ch));
			dma_chan[ch].flags &= ~OMAP_DMA_ACTIVE;
		}
	}
	if (unlikely(status & OMAP2_DMA_SECURE_ERR_IRQ))
		printk(KERN_INFO "DMA secure error with device %d\n",
		       dma_chan[ch].dev_id);
	if (unlikely(status & OMAP2_DMA_MISALIGNED_ERR_IRQ))
		printk(KERN_INFO "DMA misaligned error with device %d\n",
		       dma_chan[ch].dev_id);

	dma_write(OMAP2_DMA_CSR_CLEAR_MASK, CSR(ch));
	dma_write(1 << ch, IRQSTATUS_L0);

	/* If the ch is not chained then chain_id will be -1 */
	if (dma_chan[ch].chain_id != -1) {
		int chain_id = dma_chan[ch].chain_id;
		dma_chan[ch].state = DMA_CH_NOTSTARTED;
		if (dma_read(CLNK_CTRL(ch)) & (1 << 15))
			dma_chan[dma_chan[ch].next_linked_ch].state =
							DMA_CH_STARTED;
		if (dma_linked_lch[chain_id].chain_mode ==
						OMAP_DMA_DYNAMIC_CHAIN)
			omap2_disable_lnk(ch);

		if (!OMAP_DMA_CHAIN_QEMPTY(chain_id))
			OMAP_DMA_CHAIN_INCQHEAD(chain_id);

		status = dma_read(CSR(ch));
	}

	dma_write(status, CSR(ch));

	if (likely(dma_chan[ch].callback != NULL))
		dma_chan[ch].callback(ch, status, dma_chan[ch].data);

	return 0;
}

/* STATUS register count is from 1-32 while our is 0-31 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	u32 val, enable_reg;
	int i;

	val = dma_read(IRQSTATUS_L0);
	if (val == 0) {
		if (printk_ratelimit())
			printk(KERN_WARNING "Spurious DMA IRQ\n");
		return IRQ_HANDLED;
	}
	enable_reg = dma_read(IRQENABLE_L0);
	val &= enable_reg; /* Dispatch only relevant interrupts */
	for (i = 0; i < dma_chan_count && val != 0; i++) {
		if (val & 1)
			omap2_dma_handle_ch(i);
		val >>= 1;
	}

	return IRQ_HANDLED;
}

static struct irqaction omap24xx_dma_irq = {
	.name = "DMA",
	.handler = irq_handler,
	.flags = IRQF_DISABLED
};

/**
 * @brief omap_dma_set_global_params : Set global priority settings for dma
 *
 * @param arb_rate
 * @param max_fifo_depth
 * @param tparams - Number of threads to reserve:
 * DMA_THREAD_RESERVE_NORM
 * DMA_THREAD_RESERVE_ONET
 * DMA_THREAD_RESERVE_TWOT
 * DMA_THREAD_RESERVE_THREET
 */
void
omap_dma_set_global_params(int arb_rate, int max_fifo_depth, int tparams)
{
	u32 reg;

	if (max_fifo_depth == 0)
		max_fifo_depth = 1;
	if (arb_rate == 0)
		arb_rate = 1;

	reg = 0xff & max_fifo_depth;
	reg |= (0x3 & tparams) << 12;
	reg |= (arb_rate & 0xff) << 16;

	dma_write(reg, GCR);
}
EXPORT_SYMBOL(omap_dma_set_global_params);

/* One time initializations */
static int __init omap2_system_dma_init_dev(struct omap_hwmod *oh, void *user)
{
	struct omap_device *od;
	struct omap_system_dma_plat_info *pdata;
	struct resource *mem;
	char *name = "dma";
	int dma_irq, ret;

	pdata = kzalloc(sizeof(struct omap_system_dma_plat_info), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: Unable to allocate pdata for %s:%s\n",
			__func__, name, oh->name);
		return -ENOMEM;
	}

	pdata->dma_attr		= (struct omap_dma_dev_attr *)oh->dev_attr;

	/* Handling Errata's for all OMAP2PLUS processors */
	pdata->errata			= 0;

	if (cpu_is_omap242x() ||
		(cpu_is_omap243x() &&  omap_type() <= OMAP2430_REV_ES1_0))
		pdata->errata		= DMA_CHAINING_ERRATA;

	/*
	 * Errata: On ES2.0 BUFFERING disable must be set.
	 * This will always fail on ES1.0
	 */
	if (cpu_is_omap24xx())
		pdata->errata		|= DMA_BUFF_DISABLE_ERRATA;

	/*
	 * Errata: OMAP2: sDMA Channel is not disabled
	 * after a transaction error. So we explicitely
	 * disable the channel
	 */
	if (cpu_class_is_omap2())
		pdata->errata		|= DMA_CH_DISABLE_ERRATA;

	/* Errata: OMAP3 :
	 * A bug in ROM code leaves IRQ status for channels 0 and 1 uncleared
	 * after secure sram context save and restore. Hence we need to
	 * manually clear those IRQs to avoid spurious interrupts. This
	 * affects only secure devices.
	 */
	if (cpu_is_omap34xx() && (omap_type() != OMAP2_DEVICE_TYPE_GP))
		pdata->errata		|= DMA_IRQ_STATUS_ERRATA;

	/* Errata3.3: Applicable for all omap2 plus */
	pdata->errata			|= OMAP3_3_ERRATUM;

	p			= pdata;
	p->enable_irq_lch	= omap2_enable_irq_lch;
	p->disable_irq_lch	= omap2_disable_irq_lch;
	p->enable_channel_irq	= omap2_enable_channel_irq;
	p->disable_channel_irq	= omap2_disable_channel_irq;
	p->enable_lnk		= omap2_enable_lnk;
	p->disable_lnk		= omap2_disable_lnk;
	p->set_dma_chain_ch	= set_dma_chain_ch;
	p->dma_context_save	= omap2_dma_context_save;
	p->dma_context_restore	= omap2_dma_context_restore;
	p->clear_lch_regs	= NULL;
	p->get_gdma_dev		= NULL;
	p->set_gdma_dev		= NULL;

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

	dma_irq = platform_get_irq_byname(&od->pdev, "dma_0");
	ret = setup_irq(dma_irq, &omap24xx_dma_irq);
	if (ret) {
		dev_err(&od->pdev.dev, "%s:irq handler setup fail\n", __func__);
		return -EINVAL;
	}

	/* Get DMA device attributes from hwmod data base */
	d = (struct omap_dma_dev_attr *)oh->dev_attr;

	/* OMAP2 Plus: physical and logical channel count is same */
	d->dma_chan_count	= d->dma_lch_count;
	dma_chan_count		= d->dma_chan_count;

	d->dma_chan = kzalloc(sizeof(struct omap_dma_lch) *
					(dma_chan_count), GFP_KERNEL);

	if (!d->dma_chan) {
		dev_err(&od->pdev.dev, "%s: kzalloc fail\n", __func__);
		return -ENOMEM;
	}

	dma_linked_lch = kzalloc(sizeof(struct dma_link_info) *
						dma_chan_count, GFP_KERNEL);
	if (!dma_linked_lch) {
		kfree(d->dma_chan);
		return -ENOMEM;
	}

	/* OMAP4 and OMAP3630 has descrіptor loading registers */
	if (cpu_is_omap3630() || cpu_is_omap4430())
		ch_spec_regs = 22;
	else
		ch_spec_regs = 19;

	ch_ctx_regs = kzalloc(dma_chan_count * (4 * ch_spec_regs), GFP_KERNEL);

	if (!ch_ctx_regs) {
		kfree(d->dma_chan);
		kfree(dma_linked_lch);
		return -ENOMEM;
	}

	dma_chan		= d->dma_chan;

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
