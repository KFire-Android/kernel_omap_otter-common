/*
 * linux/arch/arm/mach-omap2/mcbsp.c
 *
 * Copyright (C) 2008 Instituto Nokia de Tecnologia
 * Contact: Eduardo Valentin <eduardo.valentin@indt.org.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Multichannel mode not supported.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <mach/irqs.h>
#include <plat/dma.h>
#include <plat/mux.h>
#include <plat/cpu.h>
#include <plat/mcbsp.h>
#include <plat/dma.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <linux/pm_runtime.h>

static struct omap_hwmod *oh_st_device[] = {NULL, NULL};
static int no_of_st;

#define OMAP_MCBSP_READ(mcbsp, reg) \
			omap_mcbsp_read(mcbsp, OMAP_MCBSP_REG_##reg, 0)
#define OMAP_MCBSP_WRITE(base, reg, val) \
			omap_mcbsp_write(base, OMAP_MCBSP_REG_##reg, val)
struct omap_mcbsp_reg_cfg mcbsp_cfg = {0};

static void omap2_mcbsp2_mux_setup(void)
{
	omap_cfg_reg(Y15_24XX_MCBSP2_CLKX);
	omap_cfg_reg(R14_24XX_MCBSP2_FSX);
	omap_cfg_reg(W15_24XX_MCBSP2_DR);
	omap_cfg_reg(V15_24XX_MCBSP2_DX);
	omap_cfg_reg(V14_24XX_GPIO117);
	/*
	 * TODO: Need to add MUX settings for OMAP 2430 SDP
	 */
}

static void omap2_mcbsp_request(unsigned int id)
{
	if (cpu_is_omap2420() && (id == OMAP_MCBSP2))
		omap2_mcbsp2_mux_setup();
}

static struct omap_mcbsp_ops omap2_mcbsp_ops = {
	.request	= omap2_mcbsp_request,
};


static __attribute__ ((unused)) void omap2_mcbsp_free(unsigned int id)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];

	if (!cpu_is_omap2420()) {
		if (mcbsp->dma_rx_lch != -1) {
			omap_free_dma_chain(mcbsp->dma_rx_lch);
			 mcbsp->dma_rx_lch = -1;
		}

	if (mcbsp->dma_tx_lch != -1) {
			omap_free_dma_chain(mcbsp->dma_tx_lch);
			mcbsp->dma_tx_lch = -1;
		}
	}
	return;
}
void omap2_mcbsp_config(unsigned int id,
			 const struct omap_mcbsp_reg_cfg *config)
{
	struct omap_mcbsp *mcbsp;
	void __iomem *io_base;
	mcbsp = id_to_mcbsp_ptr(id);
	io_base = mcbsp->io_base;
	OMAP_MCBSP_WRITE(mcbsp, XCCR, config->xccr);
	OMAP_MCBSP_WRITE(mcbsp, RCCR, config->rccr);
}

static void omap2_mcbsp_rx_dma_callback(int lch, u16 ch_status, void *data)
{
	struct omap_mcbsp *mcbsp_dma_rx = data;
	 void __iomem *io_base;
	io_base = mcbsp_dma_rx->io_base;

	/* If we are at the last transfer, Shut down the reciever */
	if ((mcbsp_dma_rx->auto_reset & OMAP_MCBSP_AUTO_RRST)
		&& (omap_dma_chain_status(mcbsp_dma_rx->dma_rx_lch) ==
						 OMAP_DMA_CHAIN_INACTIVE))
		OMAP_MCBSP_WRITE(mcbsp_dma_rx, SPCR1,
			OMAP_MCBSP_READ(mcbsp_dma_rx, SPCR1) & (~RRST));

	if (mcbsp_dma_rx->rx_callback != NULL)
		mcbsp_dma_rx->rx_callback(ch_status, mcbsp_dma_rx->rx_cb_arg);

}

static void omap2_mcbsp_tx_dma_callback(int lch, u16 ch_status, void *data)
{
	struct omap_mcbsp *mcbsp_dma_tx = data;
	 void __iomem *io_base;
	io_base = mcbsp_dma_tx->io_base;

	/* If we are at the last transfer, Shut down the Transmitter */
	if ((mcbsp_dma_tx->auto_reset & OMAP_MCBSP_AUTO_XRST)
		&& (omap_dma_chain_status(mcbsp_dma_tx->dma_tx_lch) ==
						 OMAP_DMA_CHAIN_INACTIVE))
		OMAP_MCBSP_WRITE(mcbsp_dma_tx, SPCR2,
			OMAP_MCBSP_READ(mcbsp_dma_tx, SPCR2) & (~XRST));

	if (mcbsp_dma_tx->tx_callback != NULL)
		mcbsp_dma_tx->tx_callback(ch_status, mcbsp_dma_tx->tx_cb_arg);
}

/*
 * Enable/Disable the sample rate generator
 * id		: McBSP interface ID
 * state	: Enable/Disable
 */
void omap2_mcbsp_set_srg_fsg(unsigned int id, u8 state)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	void __iomem *io_base;

	io_base = mcbsp->io_base;

	if (state == OMAP_MCBSP_DISABLE_FSG_SRG) {
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
			OMAP_MCBSP_READ(mcbsp, SPCR2) & (~GRST));
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
			OMAP_MCBSP_READ(mcbsp, SPCR2) & (~FRST));
	} else {
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
			OMAP_MCBSP_READ(mcbsp, SPCR2) | GRST);
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
			OMAP_MCBSP_READ(mcbsp, SPCR2) | FRST);
	}
	return;
}

/*
 * Stop transmitting data on a McBSP interface
 * id		: McBSP interface ID
 */
int omap2_mcbsp_stop_datatx(unsigned int id)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	void __iomem *io_base;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}

	io_base = mcbsp->io_base;

	if (mcbsp->dma_tx_lch != -1) {
		if (omap_stop_dma_chain_transfers(mcbsp->dma_tx_lch) != 0)
			return -EINVAL;
	}
	mcbsp->tx_dma_chain_state = 0;
	OMAP_MCBSP_WRITE(mcbsp, SPCR2,
		OMAP_MCBSP_READ(mcbsp, SPCR2) & (~XRST));

	if (!mcbsp->rx_dma_chain_state)
		omap2_mcbsp_set_srg_fsg(id, OMAP_MCBSP_DISABLE_FSG_SRG);

	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_stop_datatx);

/*
 * Stop receving data on a McBSP interface
 * id		: McBSP interface ID
 */
int omap2_mcbsp_stop_datarx(u32 id)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	void __iomem *io_base;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}

	io_base = mcbsp->io_base;

	if (mcbsp->dma_rx_lch != -1) {
		if (omap_stop_dma_chain_transfers(mcbsp->dma_rx_lch) != 0)
			return -EINVAL;
	}
	OMAP_MCBSP_WRITE(mcbsp, SPCR1,
		OMAP_MCBSP_READ(mcbsp, SPCR1) & (~RRST));

	mcbsp->rx_dma_chain_state = 0;
	if (!mcbsp->tx_dma_chain_state)
		omap2_mcbsp_set_srg_fsg(id, OMAP_MCBSP_DISABLE_FSG_SRG);

	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_stop_datarx);

/*
 * Get the element index and frame index of transmitter
 * id		: McBSP interface ID
 * ei		: element index
 * fi		: frame index
 */
int omap2_mcbsp_transmitter_index(int id, int *ei, int *fi)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	int eix = 0, fix = 0;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}

	if ((!ei) || (!fi)) {
		printk(KERN_ERR	"OMAP_McBSP: Invalid ei and fi params \n");
		goto txinx_err;
	}

	if (mcbsp->dma_tx_lch == -1) {
		printk(KERN_ERR "OMAP_McBSP: Transmitter not started\n");
		goto txinx_err;
	}

	if (omap_get_dma_chain_index
		(mcbsp->dma_tx_lch, &eix, &fix) != 0) {
		printk(KERN_ERR "OMAP_McBSP: Getting chain index failed\n");
		goto txinx_err;
	}

	*ei = eix;
	*fi = fix;

	return 0;

txinx_err:
	return -EINVAL;
}
EXPORT_SYMBOL(omap2_mcbsp_transmitter_index);
/*
 * Get the element index and frame index of receiver
 * id		: McBSP interface ID
 * ei		: element index
 * fi		: frame index
 */
int omap2_mcbsp_receiver_index(int id, int *ei, int *fi)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	int eix = 0, fix = 0;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}

	if ((!ei) || (!fi)) {
		printk(KERN_ERR	"OMAP_McBSP: Invalid ei and fi params x\n");
		goto rxinx_err;
	}

	/* Check if chain exists */
	if (mcbsp->dma_rx_lch == -1) {
		printk(KERN_ERR "OMAP_McBSP: Receiver not started\n");
		goto rxinx_err;
	}

	/* Get dma_chain_index */
	if (omap_get_dma_chain_index
		(mcbsp->dma_rx_lch, &eix, &fix) != 0) {
		printk(KERN_ERR "OMAP_McBSP: Getting chain index failed\n");
		goto rxinx_err;
	}

	*ei = eix;
	*fi = fix;
	return 0;

rxinx_err:
	return -EINVAL;
}
EXPORT_SYMBOL(omap2_mcbsp_receiver_index);

/*
 * Basic Reset Transmitter
 * id		: McBSP interface number
 * state	: Disable (0)/ Enable (1) the transmitter
 */
int omap2_mcbsp_set_xrst(unsigned int id, u8 state)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	void __iomem *io_base;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}
	io_base = mcbsp->io_base;

	if (state == OMAP_MCBSP_XRST_DISABLE)
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
		      OMAP_MCBSP_READ(mcbsp, SPCR2) & (~XRST));
	else
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
			OMAP_MCBSP_READ(mcbsp, SPCR2) | XRST);
	udelay(10);

	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_set_xrst);

/*
 * Reset Receiver
 * id		: McBSP interface number
 * state	: Disable (0)/ Enable (1) the receiver
 */
int omap2_mcbsp_set_rrst(unsigned int id, u8 state)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	void __iomem *io_base;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}
	io_base = mcbsp->io_base;

	if (state == OMAP_MCBSP_RRST_DISABLE)
		OMAP_MCBSP_WRITE(mcbsp, SPCR1,
			OMAP_MCBSP_READ(mcbsp, SPCR1) & (~RRST));
	else
		OMAP_MCBSP_WRITE(mcbsp, SPCR1,
			OMAP_MCBSP_READ(mcbsp, SPCR1) | RRST);
	udelay(10);
	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_set_rrst);

/*
 * Configure the receiver parameters
 * id		: McBSP Interface ID
 * rp		: DMA Receive parameters
 */
int omap2_mcbsp_dma_recv_params(unsigned int id,
			struct omap_mcbsp_dma_transfer_params *rp)
{
	struct omap_mcbsp *mcbsp;
	 void __iomem *io_base;
	int err, chain_id = -1;
	struct omap_dma_channel_params rx_params;
	u32  dt = 0;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}

	mcbsp = id_to_mcbsp_ptr(id);
	io_base = mcbsp->io_base;
	dt = rp->word_length1;

	if (dt == OMAP_MCBSP_WORD_8)
		rx_params.data_type = OMAP_DMA_DATA_TYPE_S8;
	else if (dt == OMAP_MCBSP_WORD_16)
		rx_params.data_type = OMAP_DMA_DATA_TYPE_S16;
	else if (dt == OMAP_MCBSP_WORD_32)
		rx_params.data_type = OMAP_DMA_DATA_TYPE_S32;
	else
		return -EINVAL;

	rx_params.read_prio = DMA_CH_PRIO_HIGH;
	rx_params.write_prio = DMA_CH_PRIO_HIGH;
	rx_params.sync_mode = OMAP_DMA_SYNC_ELEMENT;
	rx_params.src_fi = 0;
	rx_params.trigger = mcbsp->dma_rx_sync;
	rx_params.src_or_dst_synch = 0x01;
	rx_params.src_amode = OMAP_DMA_AMODE_CONSTANT;
	rx_params.src_ei = 0x0;
	/* Indexing is always in bytes - so multiply with dt */

	dt = (rx_params.data_type == OMAP_DMA_DATA_TYPE_S8) ? 1 :
		(rx_params.data_type == OMAP_DMA_DATA_TYPE_S16) ? 2 : 4;

	/* SKIP_FIRST and SKIP_SECOND- skip alternate data in 24 bit mono */
	if (rp->skip_alt == OMAP_MCBSP_SKIP_SECOND) {
		rx_params.dst_amode = OMAP_DMA_AMODE_DOUBLE_IDX;
		rx_params.dst_ei = (1);
		rx_params.dst_fi = (1) + ((-1) * dt);
	} else if (rp->skip_alt == OMAP_MCBSP_SKIP_FIRST) {
		rx_params.dst_amode = OMAP_DMA_AMODE_DOUBLE_IDX;
		rx_params.dst_ei = 1 + (-2) * dt;
		rx_params.dst_fi = 1 + (2) * dt;
	} else {
		rx_params.dst_amode = OMAP_DMA_AMODE_POST_INC;
		rx_params.dst_ei = 0;
		rx_params.dst_fi = 0;
	}

	mcbsp->rxskip_alt = rp->skip_alt;
	mcbsp->auto_reset &= ~OMAP_MCBSP_AUTO_RRST;
	mcbsp->auto_reset |=	(rp->auto_reset & OMAP_MCBSP_AUTO_RRST);

	mcbsp->rx_word_length = rx_params.data_type << 0x1;
	if (rx_params.data_type == 0)
		mcbsp->rx_word_length = 1;

	mcbsp->rx_callback = rp->callback;
	/* request for a chain of dma channels for data reception */
	if (mcbsp->dma_rx_lch == -1) {
		err = omap_request_dma_chain(id, "McBSP RX",
					 omap2_mcbsp_rx_dma_callback, &chain_id,
					 2, OMAP_DMA_DYNAMIC_CHAIN, rx_params);
		if (err < 0) {
			printk(KERN_ERR "Receive path configuration failed \n");
			return -EINVAL;
		}
		mcbsp->dma_rx_lch = chain_id;
		mcbsp->rx_dma_chain_state = 0;
	} else {
		/* DMA params already set, modify the same!! */
		err = omap_modify_dma_chain_params(mcbsp->dma_rx_lch,
								 rx_params);
		if (err < 0)
			return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_dma_recv_params);

/*
 * Configure the transmitter parameters
 * id		: McBSP Interface ID
 * tp		: DMA Transfer parameters
 */

int omap2_mcbsp_dma_trans_params(unsigned int id,
			struct omap_mcbsp_dma_transfer_params *tp)
{
	struct omap_mcbsp *mcbsp;

	struct omap_dma_channel_params tx_params;
	int err = 0, chain_id = -1;
	 void __iomem *io_base;
	u32 dt = 0;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}

	mcbsp = id_to_mcbsp_ptr(id);
	io_base = mcbsp->io_base;

	dt = tp->word_length1;
	if ((dt != OMAP_MCBSP_WORD_8) && (dt != OMAP_MCBSP_WORD_16)
						 && (dt != OMAP_MCBSP_WORD_32))
		return -EINVAL;
	if (dt == OMAP_MCBSP_WORD_8)
		tx_params.data_type = OMAP_DMA_DATA_TYPE_S8;
	else if (dt == OMAP_MCBSP_WORD_16)
		tx_params.data_type = OMAP_DMA_DATA_TYPE_S16;
	else if (dt == OMAP_MCBSP_WORD_32)
		tx_params.data_type = OMAP_DMA_DATA_TYPE_S32;
	else
		return -EINVAL;

	tx_params.read_prio = DMA_CH_PRIO_HIGH;
	tx_params.write_prio = DMA_CH_PRIO_HIGH;
	tx_params.sync_mode = OMAP_DMA_SYNC_ELEMENT;
	tx_params.dst_fi = 0;
	tx_params.trigger = mcbsp->dma_tx_sync;
	tx_params.src_or_dst_synch = 0;
	tx_params.dst_amode = OMAP_DMA_AMODE_CONSTANT;
	tx_params.dst_ei = 0;
	/* Indexing is always in bytes - so multiply with dt */
	mcbsp->tx_word_length = tx_params.data_type << 0x1;

	if (tx_params.data_type == 0)
		mcbsp->tx_word_length = 1;
	dt = mcbsp->tx_word_length;

	/* SKIP_FIRST and SKIP_SECOND- skip alternate data in 24 bit mono */
	if (tp->skip_alt == OMAP_MCBSP_SKIP_SECOND) {
		tx_params.src_amode = OMAP_DMA_AMODE_DOUBLE_IDX;
		tx_params.src_ei = (1);
		tx_params.src_fi = (1) + ((-1) * dt);
	} else if (tp->skip_alt == OMAP_MCBSP_SKIP_FIRST) {
		tx_params.src_amode = OMAP_DMA_AMODE_DOUBLE_IDX;
		tx_params.src_ei = 1 + (-2) * dt;
		tx_params.src_fi = 1 + (2) * dt;
	} else {
		tx_params.src_amode = OMAP_DMA_AMODE_POST_INC;
		tx_params.src_ei = 0;
		tx_params.src_fi = 0;
	}

	mcbsp->txskip_alt = tp->skip_alt;
	mcbsp->auto_reset &= ~OMAP_MCBSP_AUTO_XRST;
	mcbsp->auto_reset |=
		(tp->auto_reset & OMAP_MCBSP_AUTO_XRST);
	mcbsp->tx_callback = tp->callback;

	/* Based on Rjust we can do double indexing DMA params configuration */
	if (mcbsp->dma_tx_lch == -1) {
		err = omap_request_dma_chain(id, "McBSP TX",
					 omap2_mcbsp_tx_dma_callback, &chain_id,
					 2, OMAP_DMA_DYNAMIC_CHAIN, tx_params);
		if (err < 0) {
			printk(KERN_ERR
				"Transmit path configuration failed \n");
			return -EINVAL;
		}
		mcbsp->tx_dma_chain_state = 0;
		mcbsp->dma_tx_lch = chain_id;
	} else {
		/* DMA params already set, modify the same!! */
		err = omap_modify_dma_chain_params(mcbsp->dma_tx_lch,
								 tx_params);
		if (err < 0)
			return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_dma_trans_params);

/*
 * Start receving data on a McBSP interface
 * id			: McBSP interface ID
 * cbdata		: User data to be returned with callback
 * buf_start_addr	: The destination address [physical address]
 * buf_size		: Buffer size
*/

int omap2_mcbsp_receive_data(unsigned int id, void *cbdata,
			dma_addr_t buf_start_addr, u32 buf_size)
{
	struct omap_mcbsp *mcbsp;
	void __iomem *io_base;
	int enable_rx = 0;
	int e_count = 0;
	int f_count = 0;
	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}

	mcbsp = id_to_mcbsp_ptr(id);

	io_base = mcbsp->io_base;
	mcbsp->rx_cb_arg = cbdata;

	/* Auto RRST handling logic - disable the Reciever before 1st dma */
	if ((mcbsp->auto_reset & OMAP_MCBSP_AUTO_RRST) &&
		(omap_dma_chain_status(mcbsp->dma_rx_lch)
				== OMAP_DMA_CHAIN_INACTIVE)) {
	OMAP_MCBSP_WRITE(mcbsp, SPCR1,
			OMAP_MCBSP_READ(mcbsp, SPCR1) & (~RRST));
		enable_rx = 1;
	}

	/*
	 * for skip_first and second, we need to set e_count =2,
	 * and f_count = number of frames = number of elements/e_count
	 */
	e_count = (buf_size / mcbsp->rx_word_length);

	if (mcbsp->rxskip_alt != OMAP_MCBSP_SKIP_NONE) {
		/*
		 * since the number of frames = total number of elements/element
		 * count, However, with double indexing for data transfers,
		 * double the number of elements need to be transmitted
		 */
		f_count = e_count;
		e_count = 2;
	} else {
		f_count = 1;
	}
	/*
	 * If the DMA is to be configured to skip the first byte, we need
	 * to jump backwards, so we need to move one chunk forward and
	 * ask dma if we dont want the client driver knowing abt this.
	 */
	if (mcbsp->rxskip_alt == OMAP_MCBSP_SKIP_FIRST)
		buf_start_addr += mcbsp->rx_word_length;

	if (omap_dma_chain_a_transfer(mcbsp->dma_rx_lch,
			mcbsp->phys_base + OMAP_MCBSP_REG_DRR, buf_start_addr,
			e_count, f_count, mcbsp) < 0) {
		printk(KERN_ERR " Buffer chaining failed \n");
		return -EINVAL;
	}
	if (mcbsp->rx_dma_chain_state == 0) {
		if (mcbsp->interface_mode == OMAP_MCBSP_MASTER)
			omap2_mcbsp_set_srg_fsg(id, OMAP_MCBSP_ENABLE_FSG_SRG);

		if (omap_start_dma_chain_transfers(mcbsp->dma_rx_lch) < 0)
			return -EINVAL;
		mcbsp->rx_dma_chain_state = 1;
	}
	/* Auto RRST handling logic - Enable the Reciever after 1st dma */
	if (enable_rx &&
		(omap_dma_chain_status(mcbsp->dma_rx_lch)
				== OMAP_DMA_CHAIN_ACTIVE))
		OMAP_MCBSP_WRITE(mcbsp, SPCR1,
			OMAP_MCBSP_READ(mcbsp, SPCR1) | RRST);

	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_receive_data);

/*
 * Start transmitting data through a McBSP interface
 * id			: McBSP interface ID
 * cbdata		: User data to be returned with callback
 * buf_start_addr	: The source address [This should be physical address]
 * buf_size		: Buffer size
 */
int omap2_mcbsp_send_data(unsigned int id, void *cbdata,
			dma_addr_t buf_start_addr, u32 buf_size)
{
	struct omap_mcbsp *mcbsp;
	 void __iomem *io_base;
	u8 enable_tx = 0;
	int e_count = 0;
	int f_count = 0;

	if (!omap_mcbsp_check_valid_id(id)) {
		printk(KERN_ERR "%s: Invalid id (%d)\n", __func__, id + 1);
		return -ENODEV;
	}
	mcbsp = id_to_mcbsp_ptr(id);

	io_base = mcbsp->io_base;

	mcbsp->tx_cb_arg = cbdata;

	/* Auto RRST handling logic - disable the Reciever before 1st dma */
	if ((mcbsp->auto_reset & OMAP_MCBSP_AUTO_XRST) &&
			(omap_dma_chain_status(mcbsp->dma_tx_lch)
				== OMAP_DMA_CHAIN_INACTIVE)) {
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
			OMAP_MCBSP_READ(mcbsp, SPCR2) & (~XRST));
		enable_tx = 1;
	}
	/*
	 * for skip_first and second, we need to set e_count =2, and
	 * f_count = number of frames = number of elements/e_count
	 */
	e_count = (buf_size / mcbsp->tx_word_length);
	if (mcbsp->txskip_alt != OMAP_MCBSP_SKIP_NONE) {
		/*
		 * number of frames = total number of elements/element count,
		 * However, with double indexing for data transfers, double I
		 * the number of elements need to be transmitted
		 */
		f_count = e_count;
		e_count = 2;
	} else {
		f_count = 1;
	}

	/*
	 * If the DMA is to be configured to skip the first byte, we need
	 * to jump backwards, so we need to move one chunk forward and ask
	 * dma if we dont want the client driver knowing abt this.
	 */
	if (mcbsp->txskip_alt == OMAP_MCBSP_SKIP_FIRST)
		buf_start_addr += mcbsp->tx_word_length;

	if (omap_dma_chain_a_transfer(mcbsp->dma_tx_lch,
		buf_start_addr,	mcbsp->phys_base + OMAP_MCBSP_REG_DXR,
		e_count, f_count, mcbsp) < 0)
			return -EINVAL;

	if (mcbsp->tx_dma_chain_state == 0) {
		if (mcbsp->interface_mode == OMAP_MCBSP_MASTER)
			omap2_mcbsp_set_srg_fsg(id, OMAP_MCBSP_ENABLE_FSG_SRG);

		if (omap_start_dma_chain_transfers(mcbsp->dma_tx_lch) < 0)
			return -EINVAL;
		mcbsp->tx_dma_chain_state = 1;
	}

	/* Auto XRST handling logic - Enable the Reciever after 1st dma */
	if (enable_tx &&
		(omap_dma_chain_status(mcbsp->dma_tx_lch)
		== OMAP_DMA_CHAIN_ACTIVE))
		OMAP_MCBSP_WRITE(mcbsp, SPCR2,
			OMAP_MCBSP_READ(mcbsp, SPCR2) | XRST);

	return 0;
}
EXPORT_SYMBOL(omap2_mcbsp_send_data);

void omap2_mcbsp_set_recv_param(struct omap_mcbsp_reg_cfg *mcbsp_cfg,
				struct omap_mcbsp_cfg_param *rp)
{
	mcbsp_cfg->spcr1 = RJUST(rp->justification);
	mcbsp_cfg->rcr2 = RCOMPAND(rp->reverse_compand) |
				RDATDLY(rp->data_delay);
	if (rp->phase == OMAP_MCBSP_FRAME_SINGLEPHASE)
		mcbsp_cfg->rcr2 = mcbsp_cfg->rcr2 & ~(RPHASE);
	else
		mcbsp_cfg->rcr2 = mcbsp_cfg->rcr2  | (RPHASE) |
	RWDLEN2(rp->word_length2) | RFRLEN2(rp->frame_length2);
	mcbsp_cfg->rcr1 = RWDLEN1(rp->word_length1) |
				RFRLEN1(rp->frame_length1);
	if (rp->fsync_src == OMAP_MCBSP_RXFSYNC_INTERNAL)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | FSRM;
	if (rp->clk_mode == OMAP_MCBSP_CLKRXSRC_INTERNAL)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | CLKRM;
	if (rp->clk_polarity == OMAP_MCBSP_CLKR_POLARITY_RISING)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | CLKRP;
	if (rp->fs_polarity == OMAP_MCBSP_FS_ACTIVE_LOW)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | FSRP;
	return;
}

/*
 * Set McBSP transmit parameters
 * id          : McBSP interface ID
 * mcbsp_cfg   : McBSP register configuration
 * tp          : McBSP transmit parameters
 */

void omap2_mcbsp_set_trans_param(struct omap_mcbsp_reg_cfg *mcbsp_cfg,
				struct omap_mcbsp_cfg_param *tp)
{
	mcbsp_cfg->xcr2 = XCOMPAND(tp->reverse_compand) |
				XDATDLY(tp->data_delay);
	if (tp->phase == OMAP_MCBSP_FRAME_SINGLEPHASE)
		mcbsp_cfg->xcr2 = mcbsp_cfg->xcr2 & ~(XPHASE);
	else
		mcbsp_cfg->xcr2 = mcbsp_cfg->xcr2 | (XPHASE) |
			RWDLEN2(tp->word_length2) | RFRLEN2(tp->frame_length2);
	mcbsp_cfg->xcr1 = XWDLEN1(tp->word_length1) |
			XFRLEN1(tp->frame_length1);
	if (tp->fs_polarity == OMAP_MCBSP_FS_ACTIVE_LOW)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | FSXP;
	if (tp->fsync_src == OMAP_MCBSP_TXFSYNC_INTERNAL)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | FSXM;
	if (tp->clk_mode == OMAP_MCBSP_CLKTXSRC_INTERNAL)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | CLKXM;
	if (tp->clk_polarity == OMAP_MCBSP_CLKX_POLARITY_FALLING)
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 | CLKXP;
	return;
}

 /*
  * Set McBSP SRG configuration
  * id                 : McBSP interface ID
  * mcbsp_cfg          : McBSP register configuration
  * interface_mode     : Master/Slave
  * param              : McBSP SRG and FSG configuration
  */

void omap2_mcbsp_set_srg_cfg_param(unsigned int id, int interface_mode,
					struct omap_mcbsp_reg_cfg *mcbsp_cfg,
					struct omap_mcbsp_srg_fsg_cfg *param)
{
	struct omap_mcbsp *mcbsp = mcbsp_ptr[id];
	void __iomem *io_base;
	u32 clk_rate, clkgdv;
	io_base = mcbsp->io_base;

	mcbsp->interface_mode = interface_mode;
	mcbsp_cfg->srgr1 = FWID(param->pulse_width);

	if (interface_mode == OMAP_MCBSP_MASTER) {
		clk_rate = clk_get_rate(mcbsp->fclk);
		clkgdv = clk_rate / (param->sample_rate *
				(param->bits_per_sample - 1));
		if (clkgdv > 0xFF)
			clkgdv = 0xFF;
		mcbsp_cfg->srgr1 = mcbsp_cfg->srgr1 | CLKGDV(clkgdv);
	}

	if (param->dlb)
		mcbsp_cfg->spcr1 = mcbsp_cfg->spcr1 & ~(ALB);

	if (param->sync_mode == OMAP_MCBSP_SRG_FREERUNNING)
		mcbsp_cfg->spcr2 = mcbsp_cfg->spcr2 | FREE;
	mcbsp_cfg->srgr2 = FPER(param->period)|(param->fsgm? FSGM : 0);

	switch (param->srg_src) {

	case OMAP_MCBSP_SRGCLKSRC_CLKS:
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 & ~(SCLKME);
		mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2 & ~(CLKSM);
		/*
		* McBSP master operation at low voltage is only possible if
		* CLKSP=0 In Master mode, if client driver tries to configiure
		* input clock polarity as falling edge, we force it to Rising
		*/

		if ((param->polarity == OMAP_MCBSP_CLKS_POLARITY_RISING) ||
				       (interface_mode == OMAP_MCBSP_MASTER))
			mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2  & ~(CLKSP);
		else
			mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2  |  (CLKSP);
		break;


	case OMAP_MCBSP_SRGCLKSRC_FCLK:
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0 & ~(SCLKME);
		mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2 | (CLKSM);

		break;

	case OMAP_MCBSP_SRGCLKSRC_CLKR:
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0   | (SCLKME);
		mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2 & ~(CLKSM);
		if (param->polarity == OMAP_MCBSP_CLKR_POLARITY_FALLING)
			mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0  & ~(CLKRP);
		else
			mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0  | (CLKRP);

		break;

	case OMAP_MCBSP_SRGCLKSRC_CLKX:
		mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0   | (SCLKME);
		mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2 | (CLKSM);

		if (param->polarity == OMAP_MCBSP_CLKX_POLARITY_RISING)
			mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0  & ~(CLKXP);
		else
			mcbsp_cfg->pcr0 = mcbsp_cfg->pcr0  | (CLKXP);
		break;

	}
	if (param->sync_mode == OMAP_MCBSP_SRG_FREERUNNING)
		mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2 & ~(GSYNC);
	else if (param->sync_mode == OMAP_MCBSP_SRG_RUNNING)
		mcbsp_cfg->srgr2 = mcbsp_cfg->srgr2 | (GSYNC);

	mcbsp_cfg->xccr = OMAP_MCBSP_READ(mcbsp, XCCR) & ~(XDISABLE);
	if (param->dlb)
		mcbsp_cfg->xccr = mcbsp_cfg->xccr | (DILB);
	mcbsp_cfg->rccr = OMAP_MCBSP_READ(mcbsp, RCCR) & ~(RDISABLE);

	return;
	}

/*
 * configure the McBSP registers
 * id                  : McBSP interface ID
 * interface_mode      : Master/Slave
 * rp                  : McBSP recv parameters
 * tp                  : McBSP transmit parameters
 * param               : McBSP SRG and FSG configuration
 */
void omap2_mcbsp_params_cfg(unsigned int id, int interface_mode,
				struct omap_mcbsp_cfg_param *rp,
				struct omap_mcbsp_cfg_param *tp,
				struct omap_mcbsp_srg_fsg_cfg *param)
{
	if (rp)
		omap2_mcbsp_set_recv_param(&mcbsp_cfg, rp);
	if (tp)
		omap2_mcbsp_set_trans_param(&mcbsp_cfg, tp);
	if (param)
		omap2_mcbsp_set_srg_cfg_param(id,
		interface_mode, &mcbsp_cfg, param);
	omap_mcbsp_config(id, &mcbsp_cfg);

	return;
}
EXPORT_SYMBOL(omap2_mcbsp_params_cfg);

struct omap_device_pm_latency omap2_mcbsp_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int omap_init_mcbsp(struct omap_hwmod *oh, void *user)
{
	int id, count = 1, i;
	char *name = "omap-mcbsp";
	char dev_name[16];
	struct omap_hwmod *oh_device[2];
	struct omap_mcbsp_platform_data *pdata;
	struct omap_device *od;

	if (!oh) {
		pr_err("%s:NULL hwmod pointer (oh)\n", __func__);
		return -EINVAL;
	}

	pdata = kzalloc(sizeof(struct omap_mcbsp_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: No memory for mcbsp\n", __func__);
		return -ENOMEM;
	}
	pdata->ops = &omap2_mcbsp_ops;
	pdata->fclk = omap_hwmod_get_clk(oh);

	if (cpu_is_omap34xx()) {
		pdata->dma_op_mode = MCBSP_DMA_MODE_ELEMENT;
		if (id == 2)
			pdata->buffer_size = 0x500;
		else
			pdata->buffer_size = 0x80;
	} else if (cpu_is_omap44xx()) {
		pdata->dma_op_mode = MCBSP_DMA_MODE_ELEMENT;
		pdata->buffer_size = 0x80;
	} else {
		pdata->dma_op_mode = -EINVAL;
		pdata->buffer_size = 0;
	}

	sscanf(oh->name, "mcbsp%d", &id);
	sprintf(dev_name, "mcbsp%d_sidetone", id);
	oh_device[0] = oh;

	for (i = 0; i < no_of_st ; i++) {
		if (!strcmp(dev_name, oh_st_device[i]->name)) {
			oh_device[1] = oh_st_device[i];
			count++;
		}
	}

	pdata->oh = kzalloc(sizeof(struct omap_mcbsp_platform_data) * count,
				GFP_KERNEL);
	if (!pdata->oh) {
		pr_err("%s: No memory for mcbsp pdata oh\n", __func__);
		return -ENOMEM;
	}
	memcpy(pdata->oh, oh_device, sizeof(struct omap_hwmod *) * count);

	od = omap_device_build_ss(name, id - 1, oh_device, count, pdata,
				sizeof(*pdata), omap2_mcbsp_latency,
				ARRAY_SIZE(omap2_mcbsp_latency), false);
	if (IS_ERR(od))  {
		pr_err("%s: Cant build omap_device for %s:%s.\n", __func__,
					name, oh->name);
		kfree(pdata);
		return PTR_ERR(od);
	}
	omap_mcbsp_count++;
	return 0;
}

static int omap_mcbsp_st(struct omap_hwmod *oh, void *user)
{
	if (!oh) {
		pr_err("%s:NULL hwmod pointer (oh)\n", __func__);
		return -EINVAL;
	}
	oh_st_device[no_of_st++] = oh;
	return 0;
}

static int __init omap2_mcbsp_init(void)
{
	omap_hwmod_for_each_by_class("mcbsp_sidetone", omap_mcbsp_st,
					NULL);
	omap_hwmod_for_each_by_class("mcbsp", omap_init_mcbsp, NULL);

	mcbsp_ptr = kzalloc(omap_mcbsp_count * sizeof(struct omap_mcbsp *),
				GFP_KERNEL);
	if (!mcbsp_ptr) {
		pr_err("%s: No memory for mcbsp\n", __func__);
		return -ENOMEM;
	}

	return omap_mcbsp_init();
}
arch_initcall(omap2_mcbsp_init);
