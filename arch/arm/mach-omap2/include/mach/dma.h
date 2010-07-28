/*
 *  OMAP DMA controller register offsets.
 *
 *  Copyright (C) 2003 Nokia Corporation
 *  Author: Juha Yrjölä <juha.yrjola@nokia.com>
 *
 *  Copyright (C) 2009 Texas Instruments
 *  Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 *  Copyright (C) 2010 Texas Instruments
 *  Converted DMA library into platform driver by Manjunatha GK <manjugk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __ASM_ARCH_OMAP2_DMA_H
#define __ASM_ARCH_OMAP2_DMA_H

/* OMAP2 Plus register offset's */
#define OMAP_DMA4_REVISION		0x00
#define OMAP_DMA4_GCR			0x78
#define OMAP_DMA4_IRQSTATUS_L0		0x08
#define OMAP_DMA4_IRQSTATUS_L1		0x0c
#define OMAP_DMA4_IRQSTATUS_L2		0x10
#define OMAP_DMA4_IRQSTATUS_L3		0x14
#define OMAP_DMA4_IRQENABLE_L0		0x18
#define OMAP_DMA4_IRQENABLE_L1		0x1c
#define OMAP_DMA4_IRQENABLE_L2		0x20
#define OMAP_DMA4_IRQENABLE_L3		0x24
#define OMAP_DMA4_SYSSTATUS		0x28
#define OMAP_DMA4_OCP_SYSCONFIG		0x2c
#define OMAP_DMA4_CAPS_0		0x64
#define OMAP_DMA4_CAPS_2		0x6c
#define OMAP_DMA4_CAPS_3		0x70
#define OMAP_DMA4_CAPS_4		0x74

/* Should be part of hwmod data base ? */
#define OMAP_DMA4_LOGICAL_DMA_CH_COUNT	32	/* REVISIT: Is this 32 + 2? */

/* Common channel specific registers for omap2 */
#define OMAP_DMA4_CH_BASE(n)		(0x60 * (n) + 0x80)
#define OMAP_DMA4_CCR(n)		(0x60 * (n) + 0x80)
#define OMAP_DMA4_CLNK_CTRL(n)		(0x60 * (n) + 0x84)
#define OMAP_DMA4_CICR(n)		(0x60 * (n) + 0x88)
#define OMAP_DMA4_CSR(n)		(0x60 * (n) + 0x8c)
#define OMAP_DMA4_CSDP(n)		(0x60 * (n) + 0x90)
#define OMAP_DMA4_CEN(n)		(0x60 * (n) + 0x94)
#define OMAP_DMA4_CFN(n)		(0x60 * (n) + 0x98)
#define OMAP_DMA4_CSEI(n)		(0x60 * (n) + 0xa4)
#define OMAP_DMA4_CSFI(n)		(0x60 * (n) + 0xa8)
#define OMAP_DMA4_CDEI(n)		(0x60 * (n) + 0xac)
#define OMAP_DMA4_CDFI(n)		(0x60 * (n) + 0xb0)
#define OMAP_DMA4_CSAC(n)		(0x60 * (n) + 0xb4)
#define OMAP_DMA4_CDAC(n)		(0x60 * (n) + 0xb8)

/* Channel specific registers only on omap2 */
#define OMAP_DMA4_CSSA(n)		(0x60 * (n) + 0x9c)
#define OMAP_DMA4_CDSA(n)		(0x60 * (n) + 0xa0)
#define OMAP_DMA4_CCEN(n)		(0x60 * (n) + 0xbc)
#define OMAP_DMA4_CCFN(n)		(0x60 * (n) + 0xc0)
#define OMAP_DMA4_COLOR(n)		(0x60 * (n) + 0xc4)

/* Additional registers available on OMAP4 */
#define OMAP_DMA4_CDP(n)		(0x60 * (n) + 0xd0)
#define OMAP_DMA4_CNDP(n)		(0x60 * (n) + 0xd4)
#define OMAP_DMA4_CCDN(n)		(0x60 * (n) + 0xd8)


/* Dummy defines for support multi omap code */
/* Common registers */
#define OMAP1_DMA_GCR				0
#define OMAP1_DMA_HW_ID				0
#define OMAP1_DMA_CAPS_0_U			0
#define OMAP1_DMA_CAPS_0_L			0
#define OMAP1_DMA_CAPS_1_U			0
#define OMAP1_DMA_CAPS_1_L			0
#define OMAP1_DMA_CAPS_2			0
#define OMAP1_DMA_CAPS_3			0
#define OMAP1_DMA_CAPS_4			0
#define OMAP1_DMA_GSCR				0

/* Channel specific registers */
#define OMAP1_DMA_CH_BASE(n)			0
#define OMAP1_DMA_CCR(n)			0
#define OMAP1_DMA_CSDP(n)			0
#define OMAP1_DMA_CCR2(n)			0
#define OMAP1_DMA_CEN(n)			0
#define OMAP1_DMA_CFN(n)			0
#define OMAP1_DMA_LCH_CTRL(n)			0
#define OMAP1_DMA_COLOR_L(n)			0
#define OMAP1_DMA_COLOR_U(n)			0
#define OMAP1_DMA_CSSA_U(n)			0
#define OMAP1_DMA_CSSA_L(n)			0
#define OMAP1_DMA_CSEI(n)			0
#define OMAP1_DMA_CSFI(n)			0
#define OMAP1_DMA_CDSA_U(n)			0
#define OMAP1_DMA_CDSA_L(n)			0
#define OMAP1_DMA_CDEI(n)			0
#define OMAP1_DMA_CDFI(n)			0
#define OMAP1_DMA_CSR(n)			0
#define OMAP1_DMA_CICR(n)			0
#define OMAP1_DMA_CLNK_CTRL(n)			0
#define OMAP1_DMA_CPC(n)			0
#define OMAP1_DMA_CDAC(n)			0
#define OMAP1_DMA_CSAC(n)			0
#define OMAP1_DMA_CCEN(n)			0
#define OMAP1_DMA_CCFN(n)			0

#define OMAP_DMA4_CCR2(n)			0
#define OMAP_DMA4_LCH_CTRL(n)			0
#define OMAP_DMA4_COLOR_L(n)			0
#define OMAP_DMA4_COLOR_U(n)			0
#define OMAP1_DMA_COLOR(n)			0
#define OMAP_DMA4_CSSA_U(n)			0
#define OMAP_DMA4_CSSA_L(n)			0
#define OMAP1_DMA_CSSA(n)			0
#define OMAP_DMA4_CDSA_U(n)			0
#define OMAP_DMA4_CDSA_L(n)			0
#define OMAP1_DMA_CDSA(n)			0
#define OMAP_DMA4_CPC(n)			0

#define OMAP1_DMA_IRQENABLE_L0			0
#define OMAP1_DMA_IRQSTATUS_L0			0
#define OMAP1_DMA_OCP_SYSCONFIG			0
#define OMAP1_DMA_OCP_SYSCONFIG			0
#define OMAP_DMA4_HW_ID				0
#define OMAP_DMA4_CAPS_0_U			0
#define OMAP_DMA4_CAPS_0_L			0
#define OMAP_DMA4_CAPS_1_U			0
#define OMAP_DMA4_CAPS_1_L			0
#define OMAP_DMA4_GSCR				0
#define OMAP1_DMA_REVISION			0

struct omap_dma_lch {
	int next_lch;
	int dev_id;
	u16 saved_csr;
	u16 enabled_irqs;
	const char *dev_name;
	void (*callback)(int lch, u16 ch_status, void *data);
	void *data;
	long flags;
	/* required for Dynamic chaining */
	int prev_linked_ch;
	int next_linked_ch;
	int state;
	int chain_id;
	int status;
};

/* Chaining APIs */
extern int omap_request_dma_chain(int dev_id, const char *dev_name,
				  void (*callback) (int lch, u16 ch_status,
						    void *data),
				  int *chain_id, int no_of_chans,
				  int chain_mode,
				  struct omap_dma_channel_params params);
extern int omap_free_dma_chain(int chain_id);
extern int omap_dma_chain_a_transfer(int chain_id, int src_start,
				     int dest_start, int elem_count,
				     int frame_count, void *callbk_data);
extern int omap_start_dma_chain_transfers(int chain_id);
extern int omap_stop_dma_chain_transfers(int chain_id);
extern int omap_get_dma_chain_index(int chain_id, int *ei, int *fi);
extern int omap_get_dma_chain_dst_pos(int chain_id);
extern int omap_get_dma_chain_src_pos(int chain_id);

extern int omap_modify_dma_chain_params(int chain_id,
					struct omap_dma_channel_params params);
extern int omap_dma_chain_status(int chain_id);

#endif /* __ASM_ARCH_OMAP2_DMA_H */
