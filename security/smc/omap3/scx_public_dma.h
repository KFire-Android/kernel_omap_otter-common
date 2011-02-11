/*
 *Copyright (c)2006-2008 Trusted Logic S.A.
 *All Rights Reserved.
 *
 *This program is free software; you can redistribute it and/or
 *modify it under the terms of the GNU General Public License
 *version 2 as published by the Free Software Foundation.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program; if not, write to the Free Software
 *Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *MA 02111-1307 USA
 */

#ifndef __SCX_PUBLIC_DMA_H
#define __SCX_PUBLIC_DMA_H

#include <linux/dma-mapping.h>
#include <plat/dma.h>
#include <plat/dma-44xx.h>

#include "scx_public_crypto.h"

/*-------------------------------------------------------------------------- */
/*
 * Public DMA API
 */

/*
 * CEN Masks
 */
#define DMA_CEN_Elts_per_Frame_AES		4
#define DMA_CEN_Elts_per_Frame_DES		2
#define DMA_CEN_Elts_per_Frame_SHA		16

/*
 * CCR Masks
 */
#define DMA_CCR_Channel_Mem2AES1               0x0A
#define DMA_CCR_Channel_AES12Mem               0x09
#define DMA_CCR_Channel_Mem2AES2               0x42
#define DMA_CCR_Channel_AES22Mem               0x41

#define DMA_CCR_Channel_Mem2DES1               0x0C    /*DES1 is the trigger*/
#define DMA_CCR_Channel_DES12Mem               0x0B    /*      "       */
#define DMA_CCR_Channel_Mem2DES2               0x44    /*DES2 is the trigger*/
#define DMA_CCR_Channel_DES22Mem               0x43    /*      "       */
#define DMA_CCR_Channel_Mem2SHA                0x0D


/*
 * Request a DMA channel
 */
u32 scxPublicDMARequest(int *lch);

/*
 * Release a DMA channel
 */
u32 scxPublicDMARelease(int lch);

/**
 * This function waits for the DMA IRQ.
 *
 */
void scxPublicDMAWait(int nr_of_cb);

/*
 * This function starts a DMA operation.
 *
 * lch			DMA channel ID.
 *
 * interruptMask	Configures the Channel Interrupt Control Register
 *			should always contains OMAP_DMA_DROP_IRQ for enabling
 *			the event drop interrupt and might contains
 *			OMAP_DMA_BLOCK_IRQ for enabling the end of
 *			block interrupt.
 */
void scxPublicDMAStart(int lch, int interruptMask);

void scxPublicSetDMAChannelCommonParams(
	struct omap_dma_channel_params *pDMAChannel,
	u32 nbBlocks, u32 nbElements, u32 nDstStart,
	u32 nSrcStart, u32 nTriggerID);
void scxPublicDMASetParams(int lch, struct omap_dma_channel_params *pParams);
void scxPublicDMADisableChannel(int lch);
void scxPublicDMAClearChannel(int lch);

#endif /*__SCX_PUBLIC_DMA_H */
