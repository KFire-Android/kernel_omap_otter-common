/*
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (c) 2008, MontaVista Software, Inc. <source@mvista.com>
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

#ifndef _CPPI41_DMA_H_
#define _CPPI41_DMA_H_

#define USB_GENERIC_RNDIS_EP_SIZE_REG(n) (0x80 + (((n) - 1) << 2))

/* Mode register bits */
#define USB_MODE_SHIFT(n)	((((n) - 1) << 1))
#define USB_MODE_MASK(n)	(3 << USB_MODE_SHIFT(n))
#define USB_RX_MODE_SHIFT(n)	USB_MODE_SHIFT(n)
#define USB_TX_MODE_SHIFT(n)	USB_MODE_SHIFT(n)
#define USB_RX_MODE_MASK(n)	USB_MODE_MASK(n)
#define USB_TX_MODE_MASK(n)	USB_MODE_MASK(n)
#define USB_TRANSPARENT_MODE	0
#define USB_RNDIS_MODE		1
#define USB_CDC_MODE		2
#define USB_GENERIC_RNDIS_MODE	3
#define USB_INFINITE_DMAMODE	4
#define MAX_GRNDIS_PKTSIZE	(64 * 1024)

/* AutoReq register bits */
#define USB_RX_AUTOREQ_SHIFT(n) (((n) - 1) << 1)
#define USB_RX_AUTOREQ_MASK(n)	(3 << USB_RX_AUTOREQ_SHIFT(n))
#define USB_NO_AUTOREQ		0
#define USB_AUTOREQ_ALL_BUT_EOP 1
#define USB_AUTOREQ_ALWAYS	3

/* Teardown register bits */
#define USB_TX_TDOWN_SHIFT(n)	(16 + (n))
#define USB_TX_TDOWN_MASK(n)	(1 << USB_TX_TDOWN_SHIFT(n))
#define USB_RX_TDOWN_SHIFT(n)	(n)
#define USB_RX_TDOWN_MASK(n)	(1 << USB_RX_TDOWN_SHIFT(n))

/* CPPI 4.1 queue manager registers */
#define QMGR_PEND0_REG		0x4090
#define QMGR_PEND1_REG		0x4094
#define QMGR_PEND2_REG		0x4098

#define QMGR_RGN_OFFS		0x4000
#define QMRG_DESCRGN_OFFS	0x5000
#define QMGR_REG_OFFS		0x6000
#define QMGR_STAT_OFFS		0x7000
#define DMA_GLBCTRL_OFFS	0x2000
#define DMA_CHCTRL_OFFS		0x2800
#define DMA_SCHED_OFFS		0x3000
#define DMA_SCHEDTBL_OFFS	0x3800

#define USBSS_INTR_RX_STARV	0x00000001
#define USBSS_INTR_PD_CMPL	0x00000004
#define USBSS_INTR_TX_CMPL	0x00000500
#define USBSS_INTR_RX_CMPL	0x00000A00
#define USBSS_INTR_FLAGS	(USBSS_INTR_PD_CMPL | USBSS_INTR_TX_CMPL \
					| USBSS_INTR_RX_CMPL)

struct cppi41_wrapper_regs {
	/* xmda rndis size register */
	u16	autoreq_reg;
	u16	teardown_reg;
	u16	tx_mode_reg;
	u16	rx_mode_reg;
};

/**
 * struct usb_cppi41_info - CPPI 4.1 USB implementation details
 * @dma_block:	DMA block number
 * @ep_dma_ch:	DMA channel numbers used for EPs 1 .. Max_EP
 * @q_mgr:	queue manager number
 * @num_tx_comp_q: number of the Tx completion queues
 * @num_rx_comp_q: number of the Rx queues
 * @tx_comp_q:	pointer to the list of the Tx completion queue numbers
 * @rx_comp_q:	pointer to the list of the Rx queue numbers
 */
struct usb_cppi41_info {
	u8 dma_block;
	u8 *ep_dma_ch;
	u8 q_mgr;
	u8 num_tx_comp_q;
	u8 num_rx_comp_q;
	u8 max_dma_ch;
	u32 max_pkt_desc;
	u16 *tx_comp_q;
	u16 *rx_comp_q;
	u8 bd_intr_enb;
	u8 rx_dma_mode;
	u8 rx_inf_mode;
	u8 sched_tbl_ctrl;
	u32 version;
	struct cppi41_wrapper_regs wrp;
};

extern struct usb_cppi41_info usb_cppi41_info[];

/**
 * cppi41_completion - Tx/Rx completion queue interrupt handling hook
 * @musb:	the controller
 * @rx:	bitmask having bit N set if Rx queue N is not empty
 * @tx:	bitmask having bit N set if Tx completion queue N is not empty
 */
void cppi41_completion(struct musb *musb, u32 rx, u32 tx);
#endif	/* _CPPI41_DMA_H_ */
