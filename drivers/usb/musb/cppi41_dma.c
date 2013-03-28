/*
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (c) 2008, MontaVista Software, Inc. <source@mvista.com>
 *
 * This file implements a DMA interface using TI's CPPI 4.1 DMA.
 *
 * This program is free software; you can distribute it and/or modify it
 * under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */

#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "cppi41.h"

#include "musb_core.h"
#include "musb_dma.h"
#include "cppi41_dma.h"

/* Configuration */
#define USB_CPPI41_DESC_SIZE_SHIFT 6
#define USB_CPPI41_DESC_ALIGN	(1 << USB_CPPI41_DESC_SIZE_SHIFT)

#undef DEBUG_CPPI_TD
#undef USBDRV_DEBUG

#ifdef USBDRV_DEBUG
#define dprintk(x, ...) printk(x, ## __VA_ARGS__)
#else
#define dprintk(x, ...)
#endif

/*
 * Data structure definitions
 */

#define USBREQ_DMA_INIT			0
#define USBREQ_DMA_START		1
#define USBREQ_DMA_INPROGRESS		2
#define USBREQ_DMA_COMPLETE		3
#define USBREQ_DMA_SHORTPKT_COMPLETE	4
/*
 * USB Packet Descriptor
 */
struct usb_pkt_desc;

struct usb_pkt_desc {
	/* Hardware descriptor fields from this point */
	struct cppi41_host_pkt_desc hw_desc;	/* 40 bytes */
	/* Protocol specific data */
	dma_addr_t dma_addr;			/* offs:44 byte */
	struct usb_pkt_desc *next_pd_ptr;	/* offs:48 byte*/
	u8 ch_num;
	u8 ep_num;
	u8 eop;
	u8 res1;				/* offs:52 */
	u8 res2[12];				/* offs:64 */
};

/**
 * struct cppi41_channel - DMA Channel Control Structure
 *
 * Using the same for Tx/Rx.
 */
struct cppi41_channel {
	struct dma_channel channel;

	struct cppi41_dma_ch_obj dma_ch_obj; /* DMA channel object */
	struct cppi41_queue src_queue;	/* Tx queue or Rx free descriptor/ */
					/* buffer queue */
	struct cppi41_queue_obj queue_obj; /* Tx queue object or Rx free */
					/* descriptor/buffer queue object */

	u32 tag_info;			/* Tx PD Tag Information field */

	/* Which direction of which endpoint? */
	struct musb_hw_ep *end_pt;
	u8 transmit;
	u8 ch_num;			/* Channel number of Tx/Rx 0..3 */

	/* DMA mode: "transparent", RNDIS, CDC, or Generic RNDIS */
	u8 dma_mode;
	u8 autoreq;

	/* Book keeping for the current transfer request */
	dma_addr_t start_addr;
	u32 length;
	u32 curr_offset;
	u16 pkt_size;
	u8  transfer_mode;
	u8  zlp_queued;
	u8  inf_mode;
	u8  tx_complete;
	u8  rx_complete;
	u8  hb_mult;
	u8 xfer_state;
	u32  count;
	struct usb_pkt_desc *curr_pd;
};


/**
 * struct cppi41 - CPPI 4.1 DMA Controller Object
 *
 * Encapsulates all book keeping and data structures pertaining to
 * the CPPI 1.4 DMA controller.
 */
struct cppi41 {
	struct dma_controller controller;
	struct musb *musb;

	struct cppi41_channel *tx_cppi_ch;
	struct cppi41_channel *rx_cppi_ch;
	struct work_struct      txdma_work;
	struct work_struct      rxdma_work;

	struct usb_pkt_desc *pd_pool_head; /* Free PD pool head */
	dma_addr_t pd_mem_phys;		/* PD memory physical address */
	void *pd_mem;			/* PD memory pointer */
	u8 pd_mem_rgn;			/* PD memory region number */

	u16 teardown_qnum;		/* Teardown completion queue number */
	struct cppi41_queue_obj queue_obj; /* Teardown completion queue */
					/* object */
	u32 pkt_info;			/* Tx PD Packet Information field */
	struct usb_cppi41_info *cppi_info; /* cppi channel information */
	u32 bd_size;
	u8  inf_mode;
	u8  sof_isoc_started;
};

struct usb_cppi41_info usb_cppi41_info[2];
EXPORT_SYMBOL_GPL(usb_cppi41_info);
static void rxdma_completion_work(struct work_struct *data);

#ifdef DEBUG_CPPI_TD
static void print_pd_list(struct usb_pkt_desc *pd_pool_head)
{
	struct usb_pkt_desc *curr_pd = pd_pool_head;
	int cnt = 0;

	while (curr_pd != NULL) {
		if (cnt % 8 == 0)
			dprintk("\n%02x ", cnt);
		cnt++;
		dprintk(" %p", curr_pd);
		curr_pd = curr_pd->next_pd_ptr;
	}
	dprintk("\n");
}
#endif

static struct usb_pkt_desc *usb_get_free_pd(struct cppi41 *cppi)
{
	struct usb_pkt_desc *free_pd = cppi->pd_pool_head;

	if (free_pd != NULL) {
		cppi->pd_pool_head = free_pd->next_pd_ptr;
		free_pd->next_pd_ptr = NULL;
	}
	return free_pd;
}

static void usb_put_free_pd(struct cppi41 *cppi, struct usb_pkt_desc *free_pd)
{
	free_pd->next_pd_ptr = cppi->pd_pool_head;
	cppi->pd_pool_head = free_pd;
}

int get_xfer_type(struct musb_hw_ep *hw_ep, u8 is_tx)
{
	u8 type = musb_readb(hw_ep->regs, is_tx ? MUSB_TXTYPE : MUSB_RXTYPE);
	return (type >> 4) & 3;
}

static void musb_enable_tx_dma(struct musb_hw_ep *hw_ep)
{
	void __iomem *epio = hw_ep->regs;
	u16 csr;

	csr = musb_readw(epio, MUSB_TXCSR);
	csr |= MUSB_TXCSR_DMAENAB | MUSB_TXCSR_DMAMODE;
	if (is_host_active(hw_ep->musb))
		csr |= MUSB_TXCSR_H_WZC_BITS;
	else
		csr |= MUSB_TXCSR_MODE | MUSB_TXCSR_P_WZC_BITS;
	musb_writew(epio, MUSB_TXCSR, csr);
}

/**
 * dma_controller_start - start DMA controller
 * @controller: the controller
 *
 * This function initializes the CPPI 4.1 Tx/Rx channels.
 */
static int dma_controller_start(struct dma_controller *controller)
{
	struct cppi41 *cppi;
	struct cppi41_channel *cppi_ch;
	void __iomem *reg_base;
	struct usb_pkt_desc *curr_pd;
	unsigned long pd_addr;
	int i, max_pkt_desc;
	struct usb_cppi41_info *cppi_info;
	struct musb *musb;
	struct device *dev;

	cppi = container_of(controller, struct cppi41, controller);
	cppi_info = cppi->cppi_info;
	musb = cppi->musb;
	dev = musb->controller;
	max_pkt_desc = cppi_info->max_pkt_desc;

	/*
	 * TODO: We may need to check USB_CPPI41_MAX_PD here since CPPI 4.1
	 * requires the descriptor count to be a multiple of 2 ^ 5 (i.e. 32).
	 * Similarly, the descriptor size should also be a multiple of 32.
	 */

	/*
	 * Allocate free packet descriptor pool for all Tx/Rx endpoints --
	 * dma_alloc_coherent()  will return a page aligned address, so our
	 * alignment requirement will be honored.
	 */
	cppi->bd_size = max_pkt_desc * sizeof(struct usb_pkt_desc);
	cppi->pd_mem = dma_alloc_coherent(cppi->musb->controller,
					  cppi->bd_size,
					  &cppi->pd_mem_phys,
					  GFP_KERNEL | GFP_DMA);
	if (!cppi->pd_mem)
		return -ENOMEM;

	if (cppi41_mem_rgn_alloc(cppi_info->q_mgr, cppi->pd_mem_phys,
				 USB_CPPI41_DESC_SIZE_SHIFT,
				 get_count_order(max_pkt_desc),
				 &cppi->pd_mem_rgn)) {
		dev_dbg(dev, "failed to alloc q_mgr memory region");
		goto free_pds;
	}

	/* Allocate the teardown completion queue */
	if (cppi41_queue_alloc(CPPI41_UNASSIGNED_QUEUE,
			       0, &cppi->teardown_qnum)) {
		dev_dbg(dev, "failed to alloc td compl queue");
		goto free_mem_rgn;
	}
	dev_dbg(dev, "allocated td-compl queue %d q_mgr 0\n",
		cppi->teardown_qnum);

	if (cppi41_queue_init(&cppi->queue_obj, 0, cppi->teardown_qnum)) {
		dev_dbg(dev, "faile to init td completion queue");
		goto free_queue;
	}

	/*
	 * "Slice" PDs one-by-one from the big chunk and
	 * add them to the free pool.
	 */
	curr_pd = (struct usb_pkt_desc *)cppi->pd_mem;
	pd_addr = cppi->pd_mem_phys;
	for (i = 0; i < max_pkt_desc; i++) {
		curr_pd->dma_addr = pd_addr;

		usb_put_free_pd(cppi, curr_pd);
		curr_pd = (struct usb_pkt_desc *)((char *)curr_pd +
						  USB_CPPI41_DESC_ALIGN);
		pd_addr += USB_CPPI41_DESC_ALIGN;
	}

	/* Configure the Tx channels */
	for (i = 0, cppi_ch = cppi->tx_cppi_ch;
	     i < cppi_info->max_dma_ch; ++i, ++cppi_ch) {
		const struct cppi41_tx_ch *tx_info;

		memset(cppi_ch, 0, sizeof(struct cppi41_channel));
		cppi_ch->transmit = 1;
		cppi_ch->ch_num = i;
		cppi_ch->channel.private_data = cppi;

		/*
		 * Extract the CPPI 4.1 DMA Tx channel configuration and
		 * construct/store the Tx PD tag info field for later use...
		 */
		tx_info = cppi41_dma_block[cppi_info->dma_block].tx_ch_info
			  + cppi_info->ep_dma_ch[i];
		cppi_ch->src_queue = tx_info->tx_queue[0];
		cppi_ch->tag_info = (tx_info->port_num <<
				     CPPI41_SRC_TAG_PORT_NUM_SHIFT) |
				    (tx_info->ch_num <<
				     CPPI41_SRC_TAG_CH_NUM_SHIFT) |
				    (tx_info->sub_ch_num <<
				     CPPI41_SRC_TAG_SUB_CH_NUM_SHIFT);
	}

	/* Configure the Rx channels */
	for (i = 0, cppi_ch = cppi->rx_cppi_ch;
	     i < cppi_info->max_dma_ch; ++i, ++cppi_ch) {
		memset(cppi_ch, 0, sizeof(struct cppi41_channel));
		cppi_ch->ch_num = i;
		cppi_ch->channel.private_data = cppi;
	}

	/* Construct/store Tx PD packet info field for later use */
	cppi->pkt_info = (CPPI41_PKT_TYPE_USB << CPPI41_PKT_TYPE_SHIFT) |
			 (CPPI41_RETURN_LINKED << CPPI41_RETURN_POLICY_SHIFT);

	/* Do necessary configuartion in hardware to get started */
	reg_base = cppi->musb->ctrl_base;

	/* Disable auto request mode */
	musb_writel(reg_base, cppi_info->wrp.autoreq_reg, 0);

	/* Disable the CDC/RNDIS modes */
	musb_writel(reg_base, cppi_info->wrp.tx_mode_reg, 0);
	musb_writel(reg_base, cppi_info->wrp.rx_mode_reg, 0);

	return 0;

 free_queue:
	if (cppi41_queue_free(0, cppi->teardown_qnum))
		dev_dbg(dev, "failed to free td-compl queue\n");

 free_mem_rgn:
	if (cppi41_mem_rgn_free(cppi_info->q_mgr, cppi->pd_mem_rgn))
		dev_dbg(dev, "failed to free q_mgr mem-region\n");

 free_pds:
	dma_free_coherent(dev, cppi->bd_size, cppi->pd_mem, cppi->pd_mem_phys);

	return -ENOMEM;
}

/**
 * dma_controller_stop - stop DMA controller
 * @controller: the controller
 *
 * De-initialize the DMA Controller as necessary.
 */
static int dma_controller_stop(struct dma_controller *controller)
{
	struct cppi41 *cppi;
	void __iomem *reg_base;
	struct usb_cppi41_info *cppi_info;
	struct musb *musb;
	struct device *dev;

	cppi = container_of(controller, struct cppi41, controller);
	cppi_info = cppi->cppi_info;
	musb = cppi->musb;
	dev = musb->controller;

	/* Free the teardown completion queue */
	if (cppi41_queue_free(cppi_info->q_mgr, cppi->teardown_qnum))
		dev_dbg(dev, "failed to free td compl queue\n");

	/*
	 * Free the packet descriptor region allocated
	 * for all Tx/Rx channels.
	 */
	if (cppi41_mem_rgn_free(cppi_info->q_mgr, cppi->pd_mem_rgn))
		dev_dbg(dev, "failed to free q_mgr mem-region\n");

	dma_free_coherent(dev, cppi->bd_size, cppi->pd_mem, cppi->pd_mem_phys);

	cppi->pd_mem = 0;
	cppi->pd_mem_phys = 0;
	cppi->pd_pool_head = 0;
	cppi->bd_size = 0;

	reg_base = cppi->musb->ctrl_base;

	/* Disable auto request mode */
	musb_writel(reg_base, cppi_info->wrp.autoreq_reg, 0);

	/* Disable the CDC/RNDIS modes */
	musb_writel(reg_base, cppi_info->wrp.tx_mode_reg, 0);
	musb_writel(reg_base, cppi_info->wrp.rx_mode_reg, 0);

	return 1;
}

/**
 * cppi41_channel_alloc - allocate a CPPI channel for DMA.
 * @controller: the controller
 * @ep:		the endpoint
 * @is_tx:	1 for Tx channel, 0 for Rx channel
 *
 * With CPPI, channels are bound to each transfer direction of a non-control
 * endpoint, so allocating (and deallocating) is mostly a way to notice bad
 * housekeeping on the software side.  We assume the IRQs are always active.
 */
static struct dma_channel *cppi41_channel_alloc(struct dma_controller
						*controller,
						struct musb_hw_ep *ep, u8 is_tx)
{
	struct cppi41 *cppi;
	struct cppi41_channel  *cppi_ch;
	u32 ch_num, ep_num = ep->epnum;
	struct usb_cppi41_info *cppi_info;
	struct musb *musb;
	struct device *dev;

	cppi = container_of(controller, struct cppi41, controller);
	cppi_info = cppi->cppi_info;
	musb = cppi->musb;
	dev = musb->controller;

	/* Remember, ep_num: 1 .. Max_EP, and CPPI ch_num: 0 .. Max_EP - 1 */
	ch_num = ep_num - 1;

	if (ep_num > cppi_info->max_dma_ch) {
		dev_dbg(dev, "No %cx DMA channel for EP%d\n",
		    is_tx ? 'T' : 'R', ep_num);
		return NULL;
	}

	cppi_ch = (is_tx ? cppi->tx_cppi_ch : cppi->rx_cppi_ch) + ch_num;

	/* As of now, just return the corresponding CPPI 4.1 channel handle */
	if (is_tx) {
		/* Initialize the CPPI 4.1 Tx DMA channel */
		if (cppi41_tx_ch_init(&cppi_ch->dma_ch_obj,
				      cppi_info->dma_block,
				      cppi_info->ep_dma_ch[ch_num])) {
			dev_dbg(dev, "failed to init tx_ch %d\n", ch_num);
			return NULL;
		}
		/*
		 * Teardown descriptors will be pushed to the dedicated
		 * completion queue.
		 */
		cppi41_dma_ch_default_queue(&cppi_ch->dma_ch_obj,
					    0, cppi->teardown_qnum);
	} else {
		struct cppi41_rx_ch_cfg rx_cfg;
		u8 q_mgr = cppi_info->q_mgr;
		int i;

		/* Initialize the CPPI 4.1 Rx DMA channel */
		if (cppi41_rx_ch_init(&cppi_ch->dma_ch_obj,
				      cppi_info->dma_block,
				      cppi_info->ep_dma_ch[ch_num])) {
			dev_dbg(dev, "failed to init rx_ch %d\n", ch_num);
			return NULL;
		}

		if (cppi41_queue_alloc(CPPI41_FREE_DESC_BUF_QUEUE |
				       CPPI41_UNASSIGNED_QUEUE,
				       q_mgr, &cppi_ch->src_queue.q_num)) {
			dev_dbg(dev, "fail to alloc free desc queue\n");
			return NULL;
		}

		dev_dbg(dev, "allocated free desc queue %d\n",
			cppi_ch->src_queue.q_num);

		rx_cfg.default_desc_type = cppi41_rx_host_desc;
		rx_cfg.sop_offset = 0;
		rx_cfg.retry_starved = 1;
		rx_cfg.rx_max_buf_cnt = 0;
		rx_cfg.rx_queue.q_mgr = cppi_ch->src_queue.q_mgr = q_mgr;
		rx_cfg.rx_queue.q_num = cppi_info->rx_comp_q[ch_num];
		for (i = 0; i < 4; i++)
			rx_cfg.cfg.host_pkt.fdb_queue[i] = cppi_ch->src_queue;
		cppi41_rx_ch_configure(&cppi_ch->dma_ch_obj, &rx_cfg);
	}

	/* Initialize the CPPI 4.1 DMA source queue */
	if (cppi41_queue_init(&cppi_ch->queue_obj, cppi_ch->src_queue.q_mgr,
			       cppi_ch->src_queue.q_num)) {
		dev_dbg(dev, "failed to init %s queue",
		    is_tx ? "Tx" : "Rx free descriptor/buffer");
		if (is_tx == 0 &&
		    cppi41_queue_free(cppi_ch->src_queue.q_mgr,
				      cppi_ch->src_queue.q_num))
			dev_dbg(dev, "fail to free rx free desc queue %d\n",
				cppi_ch->src_queue.q_num);
		 return NULL;
	}

	/* Enable the DMA channel */
	cppi41_dma_ch_enable(&cppi_ch->dma_ch_obj);

	if (cppi_ch->end_pt)
		dev_dbg(dev, "re-alloc %cx dmach %d (%p)\n",
		    is_tx ? 'T' : 'R', ch_num, cppi_ch);

	cppi_ch->end_pt = ep;
	cppi_ch->ch_num = ch_num;
	cppi_ch->channel.status = MUSB_DMA_STATUS_FREE;
	cppi_ch->channel.max_len = is_tx ?
				CPPI41_TXDMA_MAXLEN : CPPI41_RXDMA_MAXLEN;

	dev_dbg(dev, "allocated %cx dmach %d for ep%d\n",
		is_tx ? 'T' : 'R', ch_num, ep_num);

	return &cppi_ch->channel;
}

/**
 * cppi41_channel_release - release a CPPI DMA channel
 * @channel: the channel
 */
static void cppi41_channel_release(struct dma_channel *channel)
{
	struct cppi41_channel *cppi_ch;
	struct cppi41 *cppi;
	struct device *dev;

	/* REVISIT: for paranoia, check state and abort if needed... */
	cppi_ch = container_of(channel, struct cppi41_channel, channel);
	cppi = cppi_ch->channel.private_data;
	dev = cppi->musb->controller;

	if (cppi_ch->end_pt == NULL)
		dev_dbg(dev, "Releasing idle DMA channel %p\n", cppi_ch);

	/* But for now, not its IRQ */
	cppi_ch->end_pt = NULL;
	channel->status = MUSB_DMA_STATUS_UNKNOWN;

	cppi41_dma_ch_disable(&cppi_ch->dma_ch_obj);

	/* De-allocate Rx free descriptior/buffer queue */
	if (cppi_ch->transmit == 0 &&
	    cppi41_queue_free(cppi_ch->src_queue.q_mgr,
			      cppi_ch->src_queue.q_num))
		dev_err(dev, "failed to free Rx descriptor/buffer queue %d\n",
			cppi_ch->src_queue.q_num);
}

static void cppi41_mode_update(struct cppi41_channel *cppi_ch, u8 mode)
{
	if (mode != cppi_ch->dma_mode) {
		struct cppi41 *cppi = cppi_ch->channel.private_data;
		struct usb_cppi41_info *cppi_info = cppi->cppi_info;
		void *__iomem reg_base = cppi->musb->ctrl_base;
		u32 reg_val;
		u8 ep_num = cppi_ch->ch_num + 1;

		if (cppi_ch->transmit) {
			reg_val = musb_readl(reg_base,
					cppi_info->wrp.tx_mode_reg);
			reg_val &= ~USB_TX_MODE_MASK(ep_num);
			reg_val |= mode << USB_TX_MODE_SHIFT(ep_num);
			musb_writel(reg_base, cppi_info->wrp.tx_mode_reg,
				reg_val);
		} else {
			reg_val = musb_readl(reg_base,
					cppi_info->wrp.rx_mode_reg);
			reg_val &= ~USB_RX_MODE_MASK(ep_num);
			reg_val |= mode << USB_RX_MODE_SHIFT(ep_num);
			musb_writel(reg_base, cppi_info->wrp.rx_mode_reg,
				reg_val);
		}
		cppi_ch->dma_mode = mode;
	}
}

/*
 * CPPI 4.1 Tx:
 * ============
 * Tx is a lot more reasonable than Rx: RNDIS mode seems to behave well except
 * how it handles the exactly-N-packets case. It appears that there's a hiccup
 * in that case (maybe the DMA completes before a ZLP gets written?) boiling
 * down to not being able to rely on the XFER DMA writing any terminating zero
 * length packet before the next transfer is started...
 *
 * The generic RNDIS mode does not have this misfeature, so we prefer using it
 * instead.  We then send the terminating ZLP *explictly* using DMA instead of
 * doing it by PIO after an IRQ.
 *
 */

/**
 * cppi41_next_tx_segment - DMA write for the next chunk of a buffer
 * @tx_ch:	Tx channel
 *
 * Context: controller IRQ-locked
 */
static unsigned cppi41_next_tx_segment(struct cppi41_channel *tx_ch)
{
	struct cppi41 *cppi = tx_ch->channel.private_data;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;
	struct usb_pkt_desc *curr_pd;
	u32 length = tx_ch->length - tx_ch->curr_offset;
	u32 pkt_size = tx_ch->pkt_size;
	unsigned num_pds, n;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	u16 q_mgr = cppi_info->q_mgr;
	u16 tx_comp_q = cppi_info->tx_comp_q[tx_ch->ch_num];
	u32 residue;
	u8 is_isoc = get_xfer_type(tx_ch->end_pt, 1) == USB_ENDPOINT_XFER_ISOC;

	if (!is_isoc && length > 128) {
		residue = length % tx_ch->pkt_size;
		if (residue <= 128)
			length -= residue;
	}

	/*
	 * Tx can use the generic RNDIS mode where we can probably fit this
	 * transfer in one PD and one IRQ.  The only time we would NOT want
	 * to use it is when the hardware constraints prevent it...
	 */
	if ((pkt_size & 0x3f) == 0) {
		num_pds  = length ? 1 : 0;
		cppi41_mode_update(tx_ch, USB_GENERIC_RNDIS_MODE);
	} else {
		num_pds  = (length + pkt_size - 1) / pkt_size;
		cppi41_mode_update(tx_ch, USB_TRANSPARENT_MODE);
	}

	pkt_size = length;
	/*
	 * If length of transmit buffer is 0 or a multiple of the endpoint size,
	 * then send the zero length packet.
	 */
	if (!length || (tx_ch->transfer_mode && length % pkt_size == 0))
		num_pds++;

	dev_dbg(dev, "TX DMA%u, %s, maxpkt %u, %u PDs, addr %#x, len %u\n",
	    tx_ch->ch_num, tx_ch->dma_mode ? "accelerated" : "transparent",
	    pkt_size, num_pds, tx_ch->start_addr + tx_ch->curr_offset, length);

	if (is_isoc)
		tx_ch->xfer_state = USBREQ_DMA_INIT;

	for (n = 0; n < num_pds; n++) {
		struct cppi41_host_pkt_desc *hw_desc;

		/* Get Tx host packet descriptor from the free pool */
		curr_pd = usb_get_free_pd(cppi);
		if (curr_pd == NULL) {
			dev_dbg(dev, "No Tx PDs\n");
			break;
		}

		if (length < pkt_size)
			pkt_size = length;

		hw_desc = &curr_pd->hw_desc;
		hw_desc->desc_info = (CPPI41_DESC_TYPE_HOST <<
				      CPPI41_DESC_TYPE_SHIFT) | pkt_size;
		hw_desc->tag_info = tx_ch->tag_info;
		hw_desc->pkt_info = cppi->pkt_info;
		hw_desc->pkt_info |= ((q_mgr << CPPI41_RETURN_QMGR_SHIFT) |
				(tx_comp_q << CPPI41_RETURN_QNUM_SHIFT));

		hw_desc->buf_ptr = tx_ch->start_addr + tx_ch->curr_offset;
		hw_desc->buf_len = pkt_size;
		hw_desc->next_desc_ptr = 0;
		hw_desc->orig_buf_len = pkt_size;

		curr_pd->ch_num = tx_ch->ch_num;
		curr_pd->ep_num = tx_ch->end_pt->epnum;

		tx_ch->curr_offset += pkt_size;
		length -= pkt_size;

		if (pkt_size == 0)
			tx_ch->zlp_queued = 1;

		if (cppi_info->bd_intr_enb)
			hw_desc->orig_buf_len |= CPPI41_PKT_INTR_FLAG;

		dev_dbg(dev, "TX PD %p: buf %08x, len %08x, pkt info %08x\n",
			curr_pd, hw_desc->buf_ptr, hw_desc->buf_len,
			hw_desc->pkt_info);

		/* make sure descriptor details are updated to memory*/
		dsb();

		cppi41_queue_push(&tx_ch->queue_obj, curr_pd->dma_addr,
				  USB_CPPI41_DESC_ALIGN, pkt_size);

		if (is_isoc && cppi_info->tx_isoc_sched_enab) {
			tx_ch->xfer_state = USBREQ_DMA_START;
			musb_enable_sof(cppi->musb);
			if (cppi->sof_isoc_started) {
				tx_ch->xfer_state = USBREQ_DMA_INPROGRESS;
				musb_enable_tx_dma(tx_ch->end_pt);
			}
		}

	}

	return n;
}

static void cppi41_autoreq_update(struct cppi41_channel *rx_ch, u8 autoreq)
{
	struct cppi41 *cppi = rx_ch->channel.private_data;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;

	if (is_host_active(cppi->musb) &&
	    autoreq != rx_ch->autoreq) {
		void *__iomem reg_base = cppi->musb->ctrl_base;
		u32 reg_val = musb_readl(reg_base,
				cppi_info->wrp.autoreq_reg);
		u8 ep_num = rx_ch->ch_num + 1;

		reg_val &= ~USB_RX_AUTOREQ_MASK(ep_num);
		reg_val |= autoreq << USB_RX_AUTOREQ_SHIFT(ep_num);

		musb_writel(reg_base, cppi_info->wrp.autoreq_reg, reg_val);
		rx_ch->autoreq = autoreq;
	}
}

static void cppi41_set_ep_size(struct cppi41_channel *rx_ch, u32 pkt_size)
{
	struct cppi41 *cppi = rx_ch->channel.private_data;
	void *__iomem reg_base = cppi->musb->ctrl_base;
	u8 ep_num = rx_ch->ch_num + 1;
	u32 res = pkt_size % 64;

	/* epsize register must be multiple of 64 */
	pkt_size += res ? (64 - res) : res;

	musb_writel(reg_base, USB_GENERIC_RNDIS_EP_SIZE_REG(ep_num), pkt_size);
}

/*
 * CPPI 4.1 Rx:
 * ============
 * Consider a 1KB bulk Rx buffer in two scenarios: (a) it's fed two 300 byte
 * packets back-to-back, and (b) it's fed two 512 byte packets back-to-back.
 * (Full speed transfers have similar scenarios.)
 *
 * The correct behavior for Linux is that (a) fills the buffer with 300 bytes,
 * and the next packet goes into a buffer that's queued later; while (b) fills
 * the buffer with 1024 bytes.  How to do that with accelerated DMA modes?
 *
 * Rx queues in RNDIS mode (one single BD) handle (a) correctly but (b) loses
 * BADLY because nothing (!) happens when that second packet fills the buffer,
 * much less when a third one arrives -- which makes it not a "true" RNDIS mode.
 * In the RNDIS protocol short-packet termination is optional, and it's fine if
 * the peripherals (not hosts!) pad the messages out to end of buffer. Standard
 * PCI host controller DMA descriptors implement that mode by default... which
 * is no accident.
 *
 * Generic RNDIS mode is the only way to reliably make both cases work.  This
 * mode is identical to the "normal" RNDIS mode except for the case where the
 * last packet of the segment matches the max USB packet size -- in this case,
 * the packet will be closed when a value (0x10000 max) in the Generic RNDIS
 * EP Size register is reached.  This mode will work for the network drivers
 * (CDC/RNDIS) as well as for the mass storage drivers where there is no short
 * packet.
 *
 * BUT we can only use non-transparent modes when USB packet size is a multiple
 * of 64 bytes. Let's see what happens when  this is not the case...
 *
 * Rx queues (2 BDs with 512 bytes each) have converse problems to RNDIS mode:
 * (b) is handled right but (a) loses badly.  DMA doesn't stop after receiving
 * a short packet and processes both of those PDs; so both packets are loaded
 * into the buffer (with 212 byte gap between them), and the next buffer queued
 * will NOT get its 300 bytes of data.  Even in the case when there should be
 * no short packets (URB_SHORT_NOT_OK is set), queueing several packets in the
 * host mode doesn't win us anything since we have to manually "prod" the Rx
 * process after each packet is received by setting ReqPkt bit in endpoint's
 * RXCSR; in the peripheral mode without short packets, queueing could be used
 * BUT we'll have to *teardown* the channel if a short packet still arrives in
 * the peripheral mode, and to "collect" the left-over packet descriptors from
 * the free descriptor/buffer queue in both cases...
 *
 * One BD at a time is the only way to make make both cases work reliably, with
 * software handling both cases correctly, at the significant penalty of needing
 * an IRQ per packet.  (The lack of I/O overlap can be slightly ameliorated by
 * enabling double buffering.)
 *
 * There seems to be no way to identify for sure the cases where the CDC mode
 * is appropriate...
 *
 */

/**
 * cppi41_next_rx_segment - DMA read for the next chunk of a buffer
 * @rx_ch:	Rx channel
 *
 * Context: controller IRQ-locked
 *
 * NOTE: In the transparent mode, we have to queue one packet at a time since:
 *	 - we must avoid starting reception of another packet after receiving
 *	   a short packet;
 *	 - in host mode we have to set ReqPkt bit in the endpoint's RXCSR after
 *	   receiving each packet but the last one... ugly!
 */
static unsigned cppi41_next_rx_segment(struct cppi41_channel *rx_ch)
{
	struct cppi41 *cppi = rx_ch->channel.private_data;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;
	struct usb_pkt_desc *curr_pd;
	struct cppi41_host_pkt_desc *hw_desc;
	u32 length = rx_ch->length - rx_ch->curr_offset;
	u32 pkt_size = rx_ch->pkt_size;
	u32 max_rx_transfer_size = MAX_GRNDIS_PKTSIZE;
	u32 i, n_bd , pkt_len;
	u8 dma_mode, autoreq;
	u8 rx_dma_mode = cppi_info->rx_dma_mode;
	struct platform_device *pdev = to_platform_device(dev);
	int id = pdev->id;

	pkt_len = rx_ch->length;
	/*
	 * Rx can use the generic RNDIS mode where we can
	 * probably fit this transfer in one PD and one IRQ
	 * (or two with a short packet).
	 */
	dma_mode = USB_TRANSPARENT_MODE;
	autoreq = USB_AUTOREQ_ALL_BUT_EOP;

	if (is_peripheral_active(cppi->musb))
		rx_dma_mode = USB_TRANSPARENT_MODE;

	if (((pkt_size & 0x3f) == 0) &&
		rx_dma_mode == USB_GENERIC_RNDIS_MODE) {
			dma_mode = USB_GENERIC_RNDIS_MODE;
	}

	if (dma_mode == USB_GENERIC_RNDIS_MODE) {
		if (cppi->cppi_info->rx_inf_mode) {
			if (length >= 2 * rx_ch->pkt_size)
				dma_mode = USB_INFINITE_DMAMODE;
			else
				dma_mode = USB_TRANSPARENT_MODE;
		}
	}

	if (length < rx_ch->pkt_size)
		dma_mode = USB_TRANSPARENT_MODE;

	if (dma_mode == USB_INFINITE_DMAMODE) {
		pkt_len = 0;
		length = length - rx_ch->pkt_size;
		cppi41_rx_ch_set_maxbufcnt(
			&rx_ch->dma_ch_obj,
			DMA_CH_RX_MAX_BUF_CNT_1);
			rx_ch->inf_mode = 1;
		dma_mode = USB_GENERIC_RNDIS_MODE;
		autoreq = USB_AUTOREQ_ALWAYS;
	} else {
		if (pkt_len > max_rx_transfer_size)
			pkt_len = max_rx_transfer_size;
	}

	/* update cppi mode */
	cppi41_mode_update(rx_ch, dma_mode);

	if (dma_mode != USB_TRANSPARENT_MODE) {
		if (is_host_active(cppi->musb))
			cppi41_autoreq_update(rx_ch, autoreq);
		cppi41_set_ep_size(rx_ch, pkt_len);
	} else if (is_host_active(cppi->musb)) {
		cppi41_autoreq_update(rx_ch, USB_NO_AUTOREQ);
	}

	dev_dbg(dev, "RX DMA%u, %s, maxpkt %u, addr %#x, rec'd %u/%u\n",
	    rx_ch->ch_num, rx_ch->dma_mode ? "accelerated" : "transparent",
	    pkt_size, rx_ch->start_addr + rx_ch->curr_offset,
	    rx_ch->curr_offset, rx_ch->length);

	/* calculate number of bd required */
	n_bd = (length + max_rx_transfer_size - 1)/max_rx_transfer_size;
	if (dma_mode == USB_TRANSPARENT_MODE) {
		if (!rx_ch->hb_mult)
			max_rx_transfer_size = rx_ch->pkt_size;
		else
			max_rx_transfer_size = rx_ch->hb_mult * rx_ch->pkt_size;
	}

	for (i = 0; i < n_bd ; ++i) {
		/* Get Rx packet descriptor from the free pool */
		curr_pd = usb_get_free_pd(cppi);
		if (curr_pd == NULL) {
			/* Shouldn't ever happen! */
			dev_dbg(dev, "No Rx PDs\n");
			goto sched;
		}

		pkt_len =
		(length > max_rx_transfer_size) ? max_rx_transfer_size : length;

		hw_desc = &curr_pd->hw_desc;
		hw_desc->desc_info = (CPPI41_DESC_TYPE_HOST <<
				      CPPI41_DESC_TYPE_SHIFT);
		hw_desc->orig_buf_ptr = rx_ch->start_addr + rx_ch->curr_offset;
		hw_desc->orig_buf_len = pkt_len;

		/* buf_len field of buffer descriptor updated by dma
		 * after reception of data is completed
		 */
		hw_desc->buf_len = 0;

		curr_pd->ch_num = rx_ch->ch_num;
		curr_pd->ep_num = rx_ch->end_pt->epnum;

		curr_pd->eop = (length -= pkt_len) ? 0 : 1;
		rx_ch->curr_offset += pkt_len;

		if (cppi_info->bd_intr_enb)
			hw_desc->orig_buf_len |= CPPI41_PKT_INTR_FLAG;

		/* make sure descriptor details are updated to memory*/
		dsb();

		/*
		 * Push the free Rx packet descriptor
		 * to the free descriptor/buffer queue.
		 */
		cppi41_queue_push(&rx_ch->queue_obj, curr_pd->dma_addr,
			USB_CPPI41_DESC_ALIGN, 0);
	}

sched:
	/*
	 * HCD arranged ReqPkt for the first packet.
	 * We arrange it for all but the last one.
	 */
	if (is_host_active(cppi->musb) && rx_ch->channel.actual_len &&
		!rx_ch->inf_mode) {
		void __iomem *epio = rx_ch->end_pt->regs;
		u16 csr = musb_readw(epio, MUSB_RXCSR);
		u8 curr_toggle = (csr & MUSB_RXCSR_H_DATATOGGLE) ? 1 : 0;

		/* check if data toggle bit got out of sync */
		if (curr_toggle == rx_ch->end_pt->prev_toggle) {
			dev_dbg(musb->controller,
				"Data toggle same as previous (=%d) on ep%d\n",
					curr_toggle, rx_ch->end_pt->epnum);

			csr |= MUSB_RXCSR_H_DATATOGGLE |
					MUSB_RXCSR_H_WR_DATATOGGLE;
			musb_writew(epio, MUSB_RXCSR, csr);
			rx_ch->end_pt->prev_toggle = !curr_toggle;
		} else
			rx_ch->end_pt->prev_toggle = curr_toggle;

		csr = musb_readw(epio, MUSB_RXCSR);
		csr |= MUSB_RXCSR_H_REQPKT | MUSB_RXCSR_H_WZC_BITS;
		musb_writew(epio, MUSB_RXCSR, csr);
	}

	/* enable schedular if not enabled */
	if (cppi_info->sched_tbl_ctrl &&
		is_peripheral_active(cppi->musb) && (n_bd > 0))
		cppi41_schedtbl_add_dma_ch(0, 0, 15 * id + rx_ch->ch_num, 0);
	return 1;
}

/**
 * cppi41_channel_program - program channel for data transfer
 * @channel:	the channel
 * @maxpacket:	max packet size
 * @mode:	for Rx, 1 unless the USB protocol driver promised to treat
 *		all short reads as errors and kick in high level fault recovery;
 *		for Tx, 0 unless the protocol driver _requires_ short-packet
 *		termination mode
 * @dma_addr:	DMA address of buffer
 * @length:	length of buffer
 *
 * Context: controller IRQ-locked
 */
static int cppi41_channel_program(struct dma_channel *channel,	u16 maxpacket,
				  u8 mode, dma_addr_t dma_addr, u32 length)
{
	struct cppi41_channel *cppi_ch;
	unsigned queued;

	cppi_ch = container_of(channel, struct cppi41_channel, channel);

	switch (channel->status) {
	case MUSB_DMA_STATUS_BUS_ABORT:
	case MUSB_DMA_STATUS_CORE_ABORT:
		/* Fault IRQ handler should have handled cleanup */
		WARNING("%cx DMA%d not cleaned up after abort!\n",
			cppi_ch->transmit ? 'T' : 'R', cppi_ch->ch_num);
		break;
	case MUSB_DMA_STATUS_BUSY:
		WARNING("Program active channel? %cx DMA%d\n",
			cppi_ch->transmit ? 'T' : 'R', cppi_ch->ch_num);
		break;
	case MUSB_DMA_STATUS_UNKNOWN:
		WARNING("%cx DMA%d not allocated!\n",
		    cppi_ch->transmit ? 'T' : 'R', cppi_ch->ch_num);
		return 0;
	case MUSB_DMA_STATUS_FREE:
		break;
	}

	channel->status = MUSB_DMA_STATUS_BUSY;

	/* Set the transfer parameters, then queue up the first segment */
	cppi_ch->start_addr = dma_addr;
	cppi_ch->curr_offset = 0;
	cppi_ch->hb_mult = (maxpacket >> 11) & 0x03;
	cppi_ch->pkt_size = maxpacket & ~(3 << 11);
	cppi_ch->length = length;
	cppi_ch->transfer_mode = mode;
	cppi_ch->zlp_queued = 0;
	cppi_ch->channel.actual_len = 0;

	/* Tx or Rx channel? */
	if (cppi_ch->transmit)
		queued = cppi41_next_tx_segment(cppi_ch);
	else
		queued = cppi41_next_rx_segment(cppi_ch);

	return	queued > 0;
}

static struct usb_pkt_desc *usb_get_pd_ptr(struct cppi41 *cppi,
					   unsigned long pd_addr)
{
	if (pd_addr >= cppi->pd_mem_phys && pd_addr < cppi->pd_mem_phys +
	    cppi->cppi_info->max_pkt_desc * USB_CPPI41_DESC_ALIGN)
		return pd_addr - cppi->pd_mem_phys + cppi->pd_mem;
	else
		return NULL;
}

static int usb_check_teardown(struct cppi41_channel *cppi_ch,
			      unsigned long pd_addr)
{
	u32 info;
	struct cppi41 *cppi = cppi_ch->channel.private_data;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;

	if (cppi41_get_teardown_info(pd_addr, &info)) {
		dev_dbg(dev, "ERROR: not a teardown descriptor\n");
		return 0;
	}

	if ((info & CPPI41_TEARDOWN_TX_RX_MASK) ==
	    (!cppi_ch->transmit << CPPI41_TEARDOWN_TX_RX_SHIFT) &&
	    (info & CPPI41_TEARDOWN_DMA_NUM_MASK) ==
	    (cppi_info->dma_block << CPPI41_TEARDOWN_DMA_NUM_SHIFT) &&
	    (info & CPPI41_TEARDOWN_CHAN_NUM_MASK) ==
	    (cppi_info->ep_dma_ch[cppi_ch->ch_num] <<
	     CPPI41_TEARDOWN_CHAN_NUM_SHIFT))
		return 1;

	dev_dbg(dev, "unexpected values in teardown descriptor\n");
	return 0;
}

/*
 * We can't handle the channel teardown via the default completion queue in
 * context of the controller IRQ-locked, so we use the dedicated teardown
 * completion queue which we can just poll for a teardown descriptor, not
 * interfering with the Tx completion queue processing.
 */
static void usb_tx_ch_teardown(struct cppi41_channel *tx_ch)
{
	struct cppi41 *cppi = tx_ch->channel.private_data;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;
	void __iomem *reg_base = musb->ctrl_base;
	u32 td_reg, timeout = 0xfffff;
	u8 ep_num = tx_ch->ch_num + 1;
	unsigned long pd_addr;
	struct cppi41_queue_obj tx_queue_obj;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;

	/* Initiate teardown for Tx DMA channel */
	cppi41_dma_ch_teardown(&tx_ch->dma_ch_obj);

	/* Wait for a descriptor to be queued and pop it... */
	do {
		td_reg  = musb_readl(reg_base,
				cppi_info->wrp.teardown_reg);
		td_reg |= USB_TX_TDOWN_MASK(ep_num);
		musb_writel(reg_base, cppi_info->wrp.teardown_reg,
				td_reg);

		pd_addr = cppi41_queue_pop(&cppi->queue_obj);
	} while (!pd_addr && timeout--);

	if (pd_addr) {
		dev_dbg(dev, "desc (%lx) popped from td completion queue\n",
				pd_addr);

		if (usb_check_teardown(tx_ch, pd_addr))
			dev_dbg(dev, "Teardown Desc (%lx) rcvd\n", pd_addr);
		else
			dev_dbg(dev, "Invalid PD(%lx)popped from TdCompQueue\n",
				pd_addr);
	} else {
		if (timeout <= 0)
			pr_err("Teardown Desc not rcvd\n");
	}

	/* read the tx completion queue and remove
	 * completion bd if any
	 */
	if (cppi41_queue_init(&tx_queue_obj, cppi_info->q_mgr,
			      cppi_info->tx_comp_q[tx_ch->ch_num])) {
		pr_err("failed to init tx completion queue %d",
			cppi_info->tx_comp_q[tx_ch->ch_num]);
		return;
	}

	while ((pd_addr = cppi41_queue_pop(&tx_queue_obj)) != 0) {
		struct usb_pkt_desc *curr_pd;

		curr_pd = usb_get_pd_ptr(cppi, pd_addr);
		if (curr_pd == NULL) {
			pr_err("Invalid PD popped from Tx completion queue\n");
			continue;
		}

		dev_dbg(dev, "Tx-PD(%p) popped from compl queue\n", curr_pd);
		dev_dbg(dev, "ch(%d) epnum(%d) len(%d)\n", curr_pd->ch_num,
			curr_pd->ep_num, curr_pd->hw_desc.buf_len);

		usb_put_free_pd(cppi, curr_pd);
	}
}

/*
 * For Rx DMA channels, the situation is more complex: there's only a single
 * completion queue for all our needs, so we have to temporarily redirect the
 * completed descriptors to our teardown completion queue, with a possibility
 * of a completed packet landing there as well...
 */
static void usb_rx_ch_teardown(struct cppi41_channel *rx_ch)
{
	struct cppi41 *cppi = rx_ch->channel.private_data;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	u32 timeout = 0xfffff, pd_addr;
	struct cppi41_queue_obj rx_queue_obj;

	cppi41_dma_ch_default_queue(&rx_ch->dma_ch_obj, 0, cppi->teardown_qnum);

	/* Initiate teardown for Rx DMA channel */
	cppi41_dma_ch_teardown(&rx_ch->dma_ch_obj);

	do {
		struct usb_pkt_desc *curr_pd;
		unsigned long pd_addr;

		/* Wait for a descriptor to be queued and pop it... */
		do {
			pd_addr = cppi41_queue_pop(&cppi->queue_obj);
		} while (!pd_addr && timeout--);

		if (timeout <= 0 || !pd_addr) {
			pr_err("teardown Desc not found\n");
			break;
		}

		dev_dbg(dev, "desc (%08lx) popped from td completion queue\n",
			pd_addr);

		/*
		 * We might have popped a completed Rx PD, so check if the
		 * physical address is within the PD region first.  If it's
		 * not the case, it must be a teardown descriptor...
		 * */
		curr_pd = usb_get_pd_ptr(cppi, pd_addr);
		if (curr_pd == NULL) {
			if (usb_check_teardown(rx_ch, pd_addr))
				break;
			continue;
		}

		/* Paranoia: check if PD is from the right channel... */
		if (curr_pd->ch_num != rx_ch->ch_num) {
			pr_err("Unexpected channel %d in Rx PD\n",
			    curr_pd->ch_num);
			continue;
		}

		/* Extract the buffer length from the completed PD */
		rx_ch->channel.actual_len += curr_pd->hw_desc.buf_len;

		/*
		 * Return Rx PDs to the software list --
		 * this is protected by critical section.
		 */
		usb_put_free_pd(cppi, curr_pd);
	} while (0);

	/* read the rx completion queue and remove
	 * completion bd if any
	 */
	if (cppi41_queue_init(&rx_queue_obj, cppi_info->q_mgr,
			      cppi_info->rx_comp_q[rx_ch->ch_num])) {
		pr_err("failed to init rx completion queue %d\n",
			cppi_info->rx_comp_q[rx_ch->ch_num]);
		return;
	}

	while ((pd_addr = cppi41_queue_pop(&rx_queue_obj)) != 0) {
		struct usb_pkt_desc *curr_pd;

		curr_pd = usb_get_pd_ptr(cppi, pd_addr);
		if (curr_pd == NULL) {
			pr_err("Invalid PD popped from Rx completion queue\n");
			continue;
		}

		dev_dbg(dev, "Rx-PD(%p) popped from compl queue\n", curr_pd);
		dev_dbg(dev, "ch(%d)epnum(%d)len(%d)\n", curr_pd->ch_num,
			curr_pd->ep_num, curr_pd->hw_desc.buf_len);

		usb_put_free_pd(cppi, curr_pd);
	}

	/* Now restore the default Rx completion queue... */
	cppi41_dma_ch_default_queue(&rx_ch->dma_ch_obj, cppi_info->q_mgr,
				    cppi_info->rx_comp_q[rx_ch->ch_num]);
}

/*
 * cppi41_channel_abort
 *
 * Context: controller IRQ-locked, endpoint selected.
 */
static int cppi41_channel_abort(struct dma_channel *channel)
{
	struct cppi41 *cppi;
	struct cppi41_channel *cppi_ch;
	struct usb_cppi41_info *cppi_info;
	struct musb  *musb;
	void __iomem *reg_base, *epio;
	struct device *dev;
	unsigned long pd_addr;
	u32 csr, td_reg;
	u8 ch_num, ep_num, i;

	cppi_ch = container_of(channel, struct cppi41_channel, channel);
	ch_num = cppi_ch->ch_num;
	cppi = cppi_ch->channel.private_data;
	cppi_info = cppi->cppi_info;
	musb = cppi->musb;
	dev = musb->controller;

	if (cppi_info->tx_isoc_sched_enab &&
		get_xfer_type(cppi_ch->end_pt, 1) == USB_ENDPOINT_XFER_ISOC) {
		cppi_ch->xfer_state = USBREQ_DMA_INIT;
		musb_disable_sof(musb);
	}

	switch (channel->status) {
	case MUSB_DMA_STATUS_BUS_ABORT:
	case MUSB_DMA_STATUS_CORE_ABORT:
		/* From Rx or Tx fault IRQ handler */
	case MUSB_DMA_STATUS_BUSY:
		/* The hardware needs shutting down... */
		dev_dbg(dev, "%s: DMA busy, status = %x\n",
			__func__, channel->status);
		break;
	case MUSB_DMA_STATUS_UNKNOWN:
		dev_dbg(dev, "%cx DMA%d not allocated\n",
		    cppi_ch->transmit ? 'T' : 'R', ch_num);
		/* FALLTHROUGH */
	case MUSB_DMA_STATUS_FREE:
		return 0;
	}

	reg_base = musb->ctrl_base;
	epio = cppi_ch->end_pt->regs;
	ep_num = ch_num + 1;

#ifdef DEBUG_CPPI_TD
	dprintk("Before teardown:");
	print_pd_list(cppi->pd_pool_head);
#endif

	if (cppi_ch->transmit) {
		dev_dbg(dev, "Tx channel teardown, cppi_ch = %p\n", cppi_ch);

		/* disable the DMAreq before teardown */
		csr  = musb_readw(epio, MUSB_TXCSR);
		csr &= ~MUSB_TXCSR_DMAENAB;
		musb_writew(epio, MUSB_TXCSR, csr);

		/* Tear down Tx DMA channel */
		usb_tx_ch_teardown(cppi_ch);

		/* Issue CPPI FIFO teardown for Tx channel */
		td_reg  = musb_readl(reg_base,
				cppi_info->wrp.teardown_reg);
		td_reg |= USB_TX_TDOWN_MASK(ep_num);
		musb_writel(reg_base, cppi_info->wrp.teardown_reg,
			td_reg);

		/* Flush FIFO of the endpoint */
		for (i = 0; i < 2; ++i) {
			csr  = musb_readw(epio, MUSB_TXCSR);
			if (csr & MUSB_TXCSR_TXPKTRDY) {
				csr |= MUSB_TXCSR_FLUSHFIFO |
					MUSB_TXCSR_H_WZC_BITS;
				musb_writew(epio, MUSB_TXCSR, csr);
			}
		}
		cppi_ch->tx_complete = 0;

	} else { /* Rx */
		dev_dbg(dev, "Rx channel teardown, cppi_ch = %p\n", cppi_ch);

		cppi_ch->rx_complete = 0;
		/* For host, ensure ReqPkt is never set again */
		cppi41_autoreq_update(cppi_ch, USB_NO_AUTOREQ);

		/* disable the DMAreq and remove reqpkt */
		csr  = musb_readw(epio, MUSB_RXCSR);
		dev_dbg(dev, "before rx-teardown: rxcsr %x rxcount %x\n", csr,
			musb_readw(epio, MUSB_RXCOUNT));

		/* 250usec delay to drain to cppi dma pipe line */
		udelay(250);

		/* For host, clear (just) ReqPkt at end of current packet(s) */
		if (is_host_active(cppi->musb))
			csr &= ~MUSB_RXCSR_H_REQPKT;

		csr &= ~MUSB_RXCSR_DMAENAB;
		musb_writew(epio, MUSB_RXCSR, csr);

		/* wait till xdma completes last 64 bytes transfer from
		 * mentor fifo to internal cppi fifo and drain
		 * cppi dma pipe line
		 */
		udelay(250);

		/* Flush FIFO of the endpoint */
		csr  = musb_readw(epio, MUSB_RXCSR);

		if (csr & MUSB_RXCSR_RXPKTRDY)
			csr |= MUSB_RXCSR_FLUSHFIFO;

		csr |= MUSB_RXCSR_H_WZC_BITS;
		musb_writew(epio, MUSB_RXCSR, csr);
		musb_writew(epio, MUSB_RXCSR, csr);
		csr  = musb_readw(epio, MUSB_RXCSR);

		/* Issue CPPI FIFO teardown for Rx channel */
		td_reg  = musb_readl(reg_base,
				cppi_info->wrp.teardown_reg);
		td_reg |= USB_RX_TDOWN_MASK(ep_num);
		musb_writel(reg_base, cppi_info->wrp.teardown_reg,
			td_reg);

		/* Tear down Rx DMA channel */
		usb_rx_ch_teardown(cppi_ch);

		/*
		 * NOTE: docs don't guarantee any of this works...  we expect
		 * that if the USB core stops telling the CPPI core to pull
		 * more data from it, then it'll be safe to flush current Rx
		 * DMA state iff any pending FIFO transfer is done.
		 */

		/* For host, ensure ReqPkt is never set again */
		cppi41_autoreq_update(cppi_ch, USB_NO_AUTOREQ);
	}

	/*
	 * There might be PDs in the Rx/Tx source queue that were not consumed
	 * by the DMA controller -- they need to be recycled properly.
	 */
	while ((pd_addr = cppi41_queue_pop(&cppi_ch->queue_obj)) != 0) {
		struct usb_pkt_desc *curr_pd;

		curr_pd = usb_get_pd_ptr(cppi, pd_addr);
		if (curr_pd == NULL) {
			pr_err("Invalid PD popped from source queue\n");
			continue;
		}

		/*
		 * Return Rx/Tx PDs to the software list --
		 * this is protected by critical section.
		 */
		dev_dbg(dev, "Returning PD %p to the free PD list\n", curr_pd);
		usb_put_free_pd(cppi, curr_pd);
	}

#ifdef DEBUG_CPPI_TD
	dprintk(dev, "After teardown:");
	print_pd_list(cppi->pd_pool_head);
#endif

	/* Re-enable the DMA channel */
	cppi41_dma_ch_enable(&cppi_ch->dma_ch_obj);

	channel->status = MUSB_DMA_STATUS_FREE;

	return 0;
}

int cppi41_isoc_schedular(struct musb *musb)
{
	struct cppi41 *cppi;
	struct cppi41_channel *tx_ch;
	struct usb_cppi41_info *cppi_info;
	int index;

	cppi = container_of(musb->dma_controller, struct cppi41, controller);
	cppi_info = cppi->cppi_info;
	for (index = 0; index < cppi_info->max_dma_ch; index++) {
		void __iomem *epio;
		u16 csr;

		tx_ch = &cppi->tx_cppi_ch[index];

		if (tx_ch->xfer_state == USBREQ_DMA_INIT ||
			tx_ch->xfer_state == USBREQ_DMA_INPROGRESS)
			continue;

		epio = tx_ch->end_pt->regs;
		csr = musb_readw(epio, MUSB_TXCSR);

		switch (tx_ch->xfer_state) {

		case USBREQ_DMA_SHORTPKT_COMPLETE:
			if (cppi->sof_isoc_started) {
				dev_dbg(musb->controller,
				"Invalid state shortpkt complete occur ep%d\n",
					index+1);
				break;
			}

		case USBREQ_DMA_START:
			if (tx_ch->xfer_state == USBREQ_DMA_SHORTPKT_COMPLETE)
				tx_ch->xfer_state = USBREQ_DMA_COMPLETE;
			else
				tx_ch->xfer_state = USBREQ_DMA_INPROGRESS;

			cppi->sof_isoc_started = 1;
			musb_enable_tx_dma(tx_ch->end_pt);
			dev_dbg(musb->controller, "isoc_sched: DMA_INP ep%d\n",
					index+1);
			break;

		case USBREQ_DMA_COMPLETE:
			tx_ch->channel.status = MUSB_DMA_STATUS_FREE;
			tx_ch->xfer_state = USBREQ_DMA_INIT;
			dev_dbg(musb->controller,
				"isoc_sched: gvbk DMA_FREE  ep%d\n", index+1);
			musb_dma_completion(cppi->musb, index+1, 1);
			musb_disable_sof(cppi->musb);
			break;

		default:
			dev_dbg(musb->controller,
				"isoc_sched: invalid state%d ep%d\n",
				tx_ch->xfer_state, index+1);
		}
	}
	return 0;
}
EXPORT_SYMBOL(cppi41_isoc_schedular);

void txdma_completion_work(struct work_struct *data)
{
	struct cppi41 *cppi = container_of(data, struct cppi41, txdma_work);
	struct cppi41_channel *tx_ch;
	struct musb *musb = cppi->musb;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	unsigned index;
	u8 resched = 0;
	unsigned long flags = 0;

	while (1) {
		for (index = 0; index < cppi_info->max_dma_ch; index++) {
			void __iomem *epio;
			u16 csr, len = 0;

			tx_ch = &cppi->tx_cppi_ch[index];
			if (tx_ch->tx_complete) {
				/* Sometimes a EP can unregister from a DMA
				 * channel while the data is still in the FIFO.
				 * Probable reason a proper abort was not
				 * called before taking such a step.
				 * Protect against such cases.
				 */
				if (!tx_ch->end_pt) {
					tx_ch->tx_complete = 0;
					tx_ch->count = 0;
					continue;
				}

				epio = tx_ch->end_pt->regs;
				csr = musb_readw(epio, MUSB_TXCSR);

				if (csr & (MUSB_TXCSR_TXPKTRDY |
					MUSB_TXCSR_FIFONOTEMPTY)) {
					resched = 1;
				} else {
					if (tx_ch->count > 0) {
						tx_ch->count--;
						resched = 1;
						continue;
					}

					len = tx_ch->length -
						tx_ch->curr_offset;
					if (len > 0) {
						tx_ch->tx_complete = 0;
						cppi41_next_tx_segment(tx_ch);
						continue;
					}

					tx_ch->channel.status =
						MUSB_DMA_STATUS_FREE;
					tx_ch->tx_complete = 0;
					spin_lock_irqsave(&musb->lock, flags);
					musb_dma_completion(musb, index+1, 1);
					spin_unlock_irqrestore(&musb->lock,
						flags);
				}
			}

			if (!resched)
				cond_resched();
		}

		if (resched) {
			resched = 0;
			cond_resched();
		} else {
			return ;
		}
	}

}

/**
 * cppi41_dma_controller_create -
 * instantiate an object representing DMA controller.
 */
struct dma_controller *dma_controller_create(struct musb  *musb,
		void __iomem *mregs)
{
	struct cppi41 *cppi;
	struct cppi41_channel *cppi_ch;
	struct platform_device *pdev = to_platform_device(musb->controller);
	int id = pdev->id, max_dma_ch;

	cppi = kzalloc(sizeof(*cppi), GFP_KERNEL);
	if (!cppi)
		goto err;

	max_dma_ch = usb_cppi41_info[id].max_dma_ch;
	cppi_ch = kzalloc(sizeof(*cppi_ch) * max_dma_ch, GFP_KERNEL);
	if (!cppi_ch)
		goto err;
	cppi->tx_cppi_ch = cppi_ch;

	cppi_ch = kzalloc(sizeof(*cppi_ch) * max_dma_ch, GFP_KERNEL);
	if (!cppi_ch)
		goto err;
	cppi->rx_cppi_ch = cppi_ch;

	/* Initialize the CPPI 4.1 DMA controller structure */
	cppi->musb  = musb;
	cppi->controller.start = dma_controller_start;
	cppi->controller.stop  = dma_controller_stop;
	cppi->controller.channel_alloc = cppi41_channel_alloc;
	cppi->controller.channel_release = cppi41_channel_release;
	cppi->controller.channel_program = cppi41_channel_program;
	cppi->controller.channel_abort = cppi41_channel_abort;
	cppi->cppi_info = (struct usb_cppi41_info *)&usb_cppi41_info[id];
	INIT_WORK(&cppi->txdma_work, txdma_completion_work);
	INIT_WORK(&cppi->rxdma_work, rxdma_completion_work);
	if (musb->ops->sof_handler)
		musb->tx_isoc_sched_enable = 1;

	return &cppi->controller;
err:
	dev_err(musb->controller, "memory allocation failed\n");
	return NULL;
}
EXPORT_SYMBOL_GPL(dma_controller_create);

/**
 * dma_controller_destroy -
 * destroy a previously instantiated DMA controller
 * @controller: the controller
 */
void dma_controller_destroy(struct dma_controller *controller)
{
	struct cppi41 *cppi;

	cppi = container_of(controller, struct cppi41, controller);

	/* Free the CPPI object */
	kfree(cppi->tx_cppi_ch);
	kfree(cppi->rx_cppi_ch);
	kfree(cppi);
}
EXPORT_SYMBOL_GPL(dma_controller_destroy);

static void usb_process_tx_queue(struct cppi41 *cppi, unsigned index)
{
	struct cppi41_queue_obj tx_queue_obj;
	unsigned long pd_addr;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;

	if (cppi41_queue_init(&tx_queue_obj, cppi_info->q_mgr,
			      cppi_info->tx_comp_q[index])) {
		pr_err("failed to init tx compl queue %d",
		    cppi_info->tx_comp_q[index]);
		return;
	}

	while ((pd_addr = cppi41_queue_pop(&tx_queue_obj)) != 0) {
		struct usb_pkt_desc *curr_pd;
		struct cppi41_channel *tx_ch;
		u8 ch_num, ep_num, is_isoc;
		u32 length;
		u32 sched_work = 0;

		curr_pd = usb_get_pd_ptr(cppi, pd_addr);
		if (curr_pd == NULL) {
			pr_err("Invalid PD popped from Tx completion queue\n");
			continue;
		}

		/* Extract the data from received packet descriptor */
		ch_num = curr_pd->ch_num;
		ep_num = curr_pd->ep_num;
		length = curr_pd->hw_desc.buf_len;

		tx_ch = &cppi->tx_cppi_ch[ch_num];
		tx_ch->channel.actual_len += length;

		dev_dbg(dev, "Tx complete: dma channel(%d) ep%d len %d\n",
			ch_num, ep_num, length);

		/*
		 * Return Tx PD to the software list --
		 * this is protected by critical section
		 */
		usb_put_free_pd(cppi, curr_pd);

		is_isoc = get_xfer_type(tx_ch->end_pt, 1) ==
				USB_ENDPOINT_XFER_ISOC;
		if (is_isoc && cppi_info->tx_isoc_sched_enab) {
			if (tx_ch->xfer_state == USBREQ_DMA_INPROGRESS)
				tx_ch->xfer_state = USBREQ_DMA_COMPLETE;
			else
				tx_ch->xfer_state =
					USBREQ_DMA_SHORTPKT_COMPLETE;
			dev_dbg(musb->controller, "DMAIsr isoch: state %d ep%d len %d\n",
					tx_ch->xfer_state, ep_num, length);
		} else if ((tx_ch->curr_offset < tx_ch->length) ||
		    (tx_ch->transfer_mode && !tx_ch->zlp_queued)) {
			sched_work = 1;
		} else if (tx_ch->channel.actual_len >= tx_ch->length) {
			void __iomem *epio;
			int residue, musb_completion = 0;
			u16 csr;

			/*
			 * We get Tx DMA completion interrupt even when
			 * data is still in FIFO and not moved out to
			 * USB bus. As we program the next request we
			 * flush out and old data in FIFO which affects
			 * USB functionality. So far, we have obsered
			 * failure with iperf.
			 */

			/* wait for tx fifo empty completion interrupt
			 * if enabled other wise use the workthread
			 * to poll fifo empty status
			 */
			epio = tx_ch->end_pt->regs;
			csr = musb_readw(epio, MUSB_TXCSR);

			residue = tx_ch->channel.actual_len %
					tx_ch->pkt_size;

			if (is_peripheral_active(musb) &&
				csr & MUSB_TXCSR_TXPKTRDY) {
				musb_completion = 1;
			} else if (is_host_active(musb) &&
				tx_ch->pkt_size > 128 && !residue) {
				musb_completion = 1;
			} else
				sched_work = 1;

			if (musb_completion) {
				dev_dbg(dev, "dma complete ep%d csr %x\n",
					ep_num, csr);
				tx_ch->channel.status = MUSB_DMA_STATUS_FREE;
				musb_dma_completion(cppi->musb, ep_num, 1);
			}
		}

		if (sched_work) {
			sched_work = 0;
			tx_ch->tx_complete = 1;
			tx_ch->count = 1;
			schedule_work(&cppi->txdma_work);
		}
	}
}

static void usb_process_rx_bd(struct cppi41 *cppi,
		struct usb_pkt_desc *curr_pd)
{
	struct cppi41_channel *rx_ch;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	u8 ch_num, ep_num;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev);
	int id = pdev->id;
	u32 length = 0, orig_buf_len;

	/* Extract the data from received packet descriptor */
	length = curr_pd->hw_desc.desc_info & CPPI41_PKT_LEN_MASK;
	ch_num = curr_pd->ch_num;
	ep_num = curr_pd->ep_num;

	/* the cppi41 dma will set received byte length as 1 when
	 * zero length packet is received, fix this dummy byte by
	 * setting acutal length received as zero
	 */
	if (curr_pd->hw_desc.pkt_info & CPPI41_ZLP)
		length = 0;

	rx_ch = &cppi->rx_cppi_ch[ch_num];
	dev_dbg(dev, "Rx complete: dma channel(%d) ep%d len %d\n",
		ch_num, ep_num, length);

	rx_ch->channel.actual_len += length;

	if (curr_pd->eop) {
		curr_pd->eop = 0;
		/* disable the rx dma schedular */
		if (cppi_info->sched_tbl_ctrl && !cppi_info->rx_inf_mode
			&& is_peripheral_active(cppi->musb)) {
			cppi41_schedtbl_remove_dma_ch(0, 0,
				ch_num + 15 * id, 0);
		}
	}

	/*
	 * Return Rx PD to the software list --
	 * this is protected by critical section
	 */
	usb_put_free_pd(cppi, curr_pd);

	orig_buf_len = curr_pd->hw_desc.orig_buf_len;
	if (cppi_info->bd_intr_enb)
		orig_buf_len &= ~CPPI41_PKT_INTR_FLAG;

	dev_dbg(dev, "curr_pd=%p, len=%d, origlen=%d,rxch(alen/len)=%d/%d\n",
		curr_pd, length, orig_buf_len, rx_ch->channel.actual_len,
		rx_ch->length);

	if (rx_ch->channel.actual_len >= rx_ch->length ||
		     length < orig_buf_len) {

		struct musb_hw_ep *ep;
		u8 isoc, next_seg = 0;

		/* Workaround for early rx completion of
		 * cppi41 dma in Generic RNDIS mode for ti81xx
		 * the Genric rndis mode does not work reliably
		 * on am33xx platform, so use only transparent mode
		 */
		if (is_host_active(cppi->musb)) {
			u32 pkt_size = rx_ch->pkt_size;
			ep = cppi->musb->endpoints + ep_num;
			isoc = musb_readb(ep->regs, MUSB_RXTYPE);
			isoc = (isoc >> 4) & 0x1;

			if (!isoc
				&& (rx_ch->dma_mode == USB_GENERIC_RNDIS_MODE)
				&& (rx_ch->channel.actual_len < rx_ch->length)
				&& !(rx_ch->channel.actual_len % pkt_size))
					next_seg = 1;
		}
		if (next_seg) {
			rx_ch->curr_offset = rx_ch->channel.actual_len;
			cppi41_next_rx_segment(rx_ch);
		} else {
			rx_ch->channel.status = MUSB_DMA_STATUS_FREE;

			if (rx_ch->inf_mode) {
				cppi41_rx_ch_set_maxbufcnt(
				&rx_ch->dma_ch_obj, 0);
				rx_ch->inf_mode = 0;
			}

			/* Rx completion routine callback */
			musb_dma_completion(cppi->musb, ep_num, 0);
		}
	} else {
			if ((rx_ch->length - rx_ch->curr_offset) > 0)
				cppi41_next_rx_segment(rx_ch);
	}
}

static void rxdma_completion_work(struct work_struct *data)
{
	struct cppi41 *cppi = container_of(data, struct cppi41, rxdma_work);
	struct musb *musb = cppi->musb;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	unsigned index;
	u8 resched = 0;
	unsigned long flags, length;
	struct cppi41_channel *rx_ch;

	while (1) {
		for (index = 0; index < cppi_info->max_dma_ch; index++) {
			rx_ch = &cppi->rx_cppi_ch[index];
			if (rx_ch->rx_complete) {
				/* Sometimes a EP can unregister from a DMA
				 * channel while the data is still in the FIFO.
				 * Probable reason a proper abort was not
				 * called before taking such a step.
				 */
				if (!rx_ch->curr_pd) {
					pr_err("invalid curr_pd chnum%d\n",
						index);
					continue;
				}

				length = rx_ch->curr_pd->hw_desc.desc_info &
						CPPI41_PKT_LEN_MASK;
				if (length == 0) {
					resched = 1;
					continue;
				}
				spin_lock_irqsave(&musb->lock, flags);
				usb_process_rx_bd(cppi, rx_ch->curr_pd);
				rx_ch->rx_complete = 0;
				rx_ch->curr_pd = 0;
				spin_unlock_irqrestore(&musb->lock, flags);
			}
		}

		if (resched) {
			resched = 0;
			cond_resched();
		} else
			return ;
	}
}

static void usb_process_rx_queue(struct cppi41 *cppi, unsigned index)
{
	struct cppi41_queue_obj rx_queue_obj;
	unsigned long pd_addr;
	struct usb_cppi41_info *cppi_info = cppi->cppi_info;
	struct musb *musb = cppi->musb;
	struct device *dev = musb->controller;

	if (cppi41_queue_init(&rx_queue_obj, cppi_info->q_mgr,
			      cppi_info->rx_comp_q[index])) {
		dev_err(dev, "fail to init rx completion queue %d\n",
			cppi_info->rx_comp_q[index]);
		return;
	}

	while ((pd_addr = cppi41_queue_pop(&rx_queue_obj)) != 0) {
		struct usb_pkt_desc *curr_pd;
		struct cppi41_channel *rx_ch;
		u8 ch_num, ep_num;
		u32 length = 0, timeout = 50;

		curr_pd = usb_get_pd_ptr(cppi, pd_addr);
		if (curr_pd == NULL) {
			pr_err("Invalid PD popped from Rx completion queue\n");
			continue;
		}

		/* This delay is required to overcome the dma race condition
		 * where software reads buffer descriptor before being updated
		 * by dma as buffer descriptor's writes by dma still pending in
		 * interconnect bridge.
		 */
		while (timeout--) {
			length = curr_pd->hw_desc.desc_info &
					CPPI41_PKT_LEN_MASK;
			if (length != 0)
				break;
			udelay(1);
		}

		/* Extract the data from received packet descriptor */
		ch_num = curr_pd->ch_num;
		ep_num = curr_pd->ep_num;

		rx_ch = &cppi->rx_cppi_ch[ch_num];

		if (length == 0) {
			dev_dbg(dev, "!Race: delayed rxBD update ch%d ep%d\n",
				ch_num, ep_num);
			rx_ch->rx_complete = 1;
			rx_ch->curr_pd = curr_pd;
			schedule_work(&cppi->rxdma_work);
			continue;
		}
		usb_process_rx_bd(cppi, curr_pd);
	}
}

/*
 * cppi41_completion - handle interrupts from the Tx/Rx completion queues
 *
 * NOTE: since we have to manually prod the Rx process in the transparent mode,
 *	 we certainly want to handle the Rx queues first.
 */
void cppi41_completion(struct musb *musb, u32 rx, u32 tx)
{
	struct cppi41 *cppi;
	unsigned index;

	cppi = container_of(musb->dma_controller, struct cppi41, controller);

	/* Process packet descriptors from the Rx queues */
	for (index = 0; rx != 0; rx >>= 1, index++)
		if (rx & 1)
			usb_process_rx_queue(cppi, index);

	/* Process packet descriptors from the Tx completion queues */
	for (index = 0; tx != 0; tx >>= 1, index++)
		if (tx & 1)
			usb_process_tx_queue(cppi, index);
}
EXPORT_SYMBOL_GPL(cppi41_completion);

MODULE_DESCRIPTION("CPPI4.1 dma controller driver for musb");
MODULE_LICENSE("GPL v2");
