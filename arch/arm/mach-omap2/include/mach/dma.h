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

/* CDP Register bitmaps */
#define DMA_LIST_CDP_DST_VALID	(BIT(0))
#define DMA_LIST_CDP_SRC_VALID	(BIT(2))
#define DMA_LIST_CDP_TYPE1	(BIT(4))
#define DMA_LIST_CDP_TYPE2	(BIT(5))
#define DMA_LIST_CDP_TYPE3	(BIT(4) | BIT(5))
#define DMA_LIST_CDP_PAUSEMODE	(BIT(7))
#define DMA_LIST_CDP_LISTMODE	(BIT(8))
#define DMA_LIST_CDP_FASTMODE	(BIT(10))
/* CAPS register bitmaps */
#define DMA_CAPS_SGLIST_SUPPORT	(BIT(20))

#define DMA_LIST_DESC_PAUSE	(BIT(0))
#define DMA_LIST_DESC_SRC_VALID	(BIT(24))
#define DMA_LIST_DESC_DST_VALID	(BIT(26))
#define DMA_LIST_DESC_BLK_END	(BIT(28))

#define OMAP_DMA_INVALID_FRAME_COUNT	(0xffff)
#define OMAP_DMA_INVALID_ELEM_COUNT	(0xffffff)
#define OMAP_DMA_INVALID_DESCRIPTOR_POINTER	(0xfffffffc)

struct omap_dma_list_config_params {
	unsigned int num_elem;
	struct omap_dma_sglist_node *sghead;
	dma_addr_t sgheadphy;
	unsigned int pausenode;
};

struct omap_dma_sglist_type1_params {
	u32 src_addr;
	u32 dst_addr;
	u16 cfn_fn;
	u16 cicr;
	u16 dst_elem_idx;
	u16 src_elem_idx;
	u32 dst_frame_idx_or_pkt_size;
	u32 src_frame_idx_or_pkt_size;
	u32 color;
	u32 csdp;
	u32 clnk_ctrl;
	u32 ccr;
};

struct omap_dma_sglist_type2a_params {
	u32 src_addr;
	u32 dst_addr;
	u16 cfn_fn;
	u16 cicr;
	u16 dst_elem_idx;
	u16 src_elem_idx;
	u32 dst_frame_idx_or_pkt_size;
	u32 src_frame_idx_or_pkt_size;
};

struct omap_dma_sglist_type2b_params {
	u32 src_or_dest_addr;
	u16 cfn_fn;
	u16 cicr;
	u16 dst_elem_idx;
	u16 src_elem_idx;
	u32 dst_frame_idx_or_pkt_size;
	u32 src_frame_idx_or_pkt_size;
};

struct omap_dma_sglist_type3a_params {
	u32 src_addr;
	u32 dst_addr;
};

struct omap_dma_sglist_type3b_params {
	u32 src_or_dest_addr;
};

enum omap_dma_sglist_descriptor_select {
	OMAP_DMA_SGLIST_DESCRIPTOR_TYPE1,
	OMAP_DMA_SGLIST_DESCRIPTOR_TYPE2a,
	OMAP_DMA_SGLIST_DESCRIPTOR_TYPE2b,
	OMAP_DMA_SGLIST_DESCRIPTOR_TYPE3a,
	OMAP_DMA_SGLIST_DESCRIPTOR_TYPE3b,
};

union omap_dma_sglist_node_type{
	struct omap_dma_sglist_type1_params t1;
	struct omap_dma_sglist_type2a_params t2a;
	struct omap_dma_sglist_type2b_params t2b;
	struct omap_dma_sglist_type3a_params t3a;
	struct omap_dma_sglist_type3b_params t3b;
};

struct omap_dma_sglist_node {

	/* Common elements for all descriptors */
	dma_addr_t next_desc_add_ptr;
	u32 num_of_elem;
	/* Type specific elements */
	union omap_dma_sglist_node_type sg_node;
	/* Control fields */
	unsigned short flags;
	/* Fields that can be set in flags variable */
	#define OMAP_DMA_LIST_SRC_VALID		BIT(0)
	#define OMAP_DMA_LIST_DST_VALID		BIT(1)
	#define OMAP_DMA_LIST_NOTIFY_BLOCK_END	BIT(2)
	enum omap_dma_sglist_descriptor_select desc_type;
	struct omap_dma_sglist_node *next;
};

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
	struct omap_dma_list_config_params list_config;
};

/**
 * omap_set_dma_sglist_mode()	Switch channel to scatter gather mode
 * @lch:	Logical channel to switch to sglist mode
 * @sghead:	Contains the descriptor elements to be executed
 *		Should be allocated using dma_alloc_coherent
 * @padd:	The dma address of sghead, as returned by dma_alloc_coherent
 * @nelem:	Number of elements in sghead
 * @chparams:	DMA channel transfer parameters. Can be NULL
 */
extern int omap_set_dma_sglist_mode(int lch,
	struct omap_dma_sglist_node *sghead, dma_addr_t padd,
	int nelem, struct omap_dma_channel_params *chparams);

/**
 * omap_clear_dma_sglist_mode()	Switch from scatter gather mode
 *				to normal mode
 * @lch:	The logical channel to be switched to normal mode
 *
 * Switches the requested logical channel to normal mode
 * from scatter gather mode
 */
extern void omap_clear_dma_sglist_mode(int lch);

/**
 * omap_start_dma_sglist_transfers()	Starts the sglist transfer
 * @lch:	logical channel on which sglist transfer to be started
 * @pauseafter:	index of the element on which to pause the transfer
 *		set to -1 if no pause is needed till end of transfer
 *
 * Start the dma transfer in list mode
 * The index (in pauseafter) is absolute (from the head of the list)
 * User should have previously called omap_set_dma_sglist_mode()
 */
extern int omap_start_dma_sglist_transfers(int lch, int pauseafter);

/**
 * omap_resume_dma_sglist_transfers()	Resumes a previously paused
 *					sglist transfer
 * @lch:	The logical channel to be resumed
 * @pauseafter:	The index of sglist to be paused again
 *		set to -1 if no pause is needed till end of transfer
 *
 * Resume the previously paused transfer
 * The index (in pauseafter) is absolute (from the head of the list)
 */
extern int omap_resume_dma_sglist_transfers(int lch, int pauseafter);

/**
 * omap_release_dma_sglist()	Releases a previously requested
 *				DMA channel which is in sglist mode
 * @lch:	The logical channel to be released
 */
extern void omap_release_dma_sglist(int lch);

/**
 * omap_get_completed_sglist_nodes()	Returns a list of completed
 *					sglist nodes
 * @lch:	The logical on which the query is to be made
 *
 * Returns the number of completed elements in the linked list
 * The value is transient if the API is invoked for an ongoing transfer
 */
int omap_get_completed_sglist_nodes(int lch);

/**
 * omap_dma_sglist_is_paused()	Query is the logical channel in
 *				sglist mode is paused or note
 * @lch:	The logical on which the query is to be made
 *
 * Returns non zero if the linked list is currently in pause state
 */
int omap_dma_sglist_is_paused(int lch);

/**
 * omap_dma_set_sglist_fastmode() Set the sglist transfer to fastmode
 * @lch:	The logical channel which is to be changed to fastmode
 * @fastmode:	Set or clear the fastmode status
 *		1 = set fastmode
 *		0 = clear fastmode
 *
 * In fastmode, DMA register settings are updated from the first element
 * of the linked list, before initiating the tranfer.
 * In non-fastmode, the first element is used only after completing the
 * transfer as already configured in the registers
 */
void omap_dma_set_sglist_fastmode(int lch, int fastmode);

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

extern void omap2_dma_context_save(void);
extern void omap2_dma_context_restore(void);

#endif /* __ASM_ARCH_OMAP2_DMA_H */
