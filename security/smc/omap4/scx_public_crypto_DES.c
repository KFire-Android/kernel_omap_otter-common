/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scx_public_crypto.h"
#include "scx_public_dma.h"

#include <asm/io.h>
#include <mach/io.h>

/*
 * DES Hardware Accelerator: Base address
 */
#define DES_REGS_HW_ADDR		0x480A5000

/*
 * CTRL register Masks
 */
#define DES_CTRL_OUTPUT_READY_BIT	(1<<0)
#define DES_CTRL_INPUT_READY_BIT	(1<<1)

#define DES_CTRL_GET_DIRECTION(x)	(x&4)
#define DES_CTRL_DIRECTION_DECRYPT	0
#define DES_CTRL_DIRECTION_ENCRYPT	(1<<2)

#define DES_CTRL_GET_TDES(x)		(x&8)
#define DES_CTRL_TDES_DES		0
#define DES_CTRL_TDES_TRIPLE_DES	(1<<3)

#define DES_CTRL_GET_MODE(x)		(x&0x10)
#define DES_CTRL_MODE_ECB		0
#define DES_CTRL_MODE_CBC		(1<<4)

/*
 * SYSCONFIG register masks
 */
#define DES_SYSCONFIG_DMA_REQ_IN_EN_BIT		(1<<5)
#define DES_SYSCONFIG_DMA_REQ_OUT_EN_BIT	(1<<6)

/*------------------------------------------------------------------------*/
/*				DES/DES3 Context			*/
/*------------------------------------------------------------------------*/
/**
 * This structure contains the registers of the DES HW accelerator.
 */
struct Des3DesReg_t {
	u32 DES_KEY3_L;	/* DES Key 3 Low Register		*/
	u32 DES_KEY3_H;	/* DES Key 3 High Register		*/
	u32 DES_KEY2_L;	/* DES Key 2 Low Register		*/
	u32 DES_KEY2_H;	/* DES Key 2 High Register		*/
	u32 DES_KEY1_L;	/* DES Key 1 Low Register		*/
	u32 DES_KEY1_H;	/* DES Key 1 High Register		*/
	u32 DES_IV_L;		/* DES Initialization Vector Low Reg	*/
	u32 DES_IV_H;		/* DES Initialization Vector High Reg	*/
	u32 DES_CTRL;		/* DES Control Register			*/
	u32 DES_LENGTH;	/* DES Length Register			*/
	u32 DES_DATA_L;	/* DES Data Input/Output Low Register	*/
	u32 DES_DATA_H;	/* DES Data Input/Output High Register	*/
	u32 DES_REV;		/* DES Revision Register		*/
	u32 DES_SYSCONFIG;	/* DES Mask and Reset Register		*/
	u32 DES_SYSSTATUS;	/* DES System Status Register		*/
};

static struct Des3DesReg_t *pDESReg_t;

/*------------------------------------------------------------------------
 *Forward declarations
 *------------------------------------------------------------------------ */

static void PDrvCryptoUpdateDESWithDMA(u8 *pSrc, u8 *pDest, u32 nbBlocks);

/*-------------------------------------------------------------------------
 *Save HWA registers into the specified operation state structure
 *-------------------------------------------------------------------------*/
static void PDrvCryptoSaveDESRegisters(u32 DES_CTRL,
	struct PUBLIC_CRYPTO_DES_OPERATION_STATE *pDESState)
{
	dprintk(KERN_INFO
		"PDrvCryptoSaveDESRegisters in pDESState=%p CTRL=0x%08x\n",
		pDESState, DES_CTRL);

	/*Save the IV if we are in CBC mode */
	if (DES_CTRL_GET_MODE(DES_CTRL) == DES_CTRL_MODE_CBC) {
		pDESState->DES_IV_L = INREG32(&pDESReg_t->DES_IV_L);
		pDESState->DES_IV_H = INREG32(&pDESReg_t->DES_IV_H);
	}
}

/*-------------------------------------------------------------------------
 *Restore the HWA registers from the operation state structure
 *-------------------------------------------------------------------------*/
static void PDrvCryptoRestoreDESRegisters(u32 DES_CTRL,
	struct PUBLIC_CRYPTO_DES_OPERATION_STATE *pDESState)
{
	dprintk(KERN_INFO "PDrvCryptoRestoreDESRegisters from \
		pDESState=%p CTRL=0x%08x\n",
		pDESState, DES_CTRL);

	/*Write the IV ctx->reg */
	if (DES_CTRL_GET_MODE(DES_CTRL) == DES_CTRL_MODE_CBC) {
		OUTREG32(&pDESReg_t->DES_IV_L, pDESState->DES_IV_L);
		OUTREG32(&pDESReg_t->DES_IV_H, pDESState->DES_IV_H);
	}

	/*Set the DIRECTION and CBC bits in the CTRL register.
	 *Keep the TDES from the accelerator */
	OUTREG32(&pDESReg_t->DES_CTRL,
		(INREG32(&pDESReg_t->DES_CTRL) & (1 << 3)) |
		(DES_CTRL & ((1 << 2) | (1 << 4))));

	/*Set the SYSCONFIG register to 0 */
	OUTREG32(&pDESReg_t->DES_SYSCONFIG, 0);
}

/*------------------------------------------------------------------------- */

void PDrvCryptoDESInit(void)
{
	pDESReg_t = omap_ioremap(DES_REGS_HW_ADDR, SZ_1M, MT_DEVICE);
	if (pDESReg_t == NULL)
		panic("Unable to remap DES/3DES module");
}

void PDrvCryptoDESExit(void)
{
	omap_iounmap(pDESReg_t);
}

bool PDrvCryptoUpdateDES(u32 DES_CTRL,
	struct PUBLIC_CRYPTO_DES_OPERATION_STATE *pDESState,
	u8 *pSrc, u8 *pDest, u32 nbBlocks)
{
	u32 nbr_of_blocks;
	u32 vTemp;
	u8 *pProcessSrc = pSrc;
	u8 *pProcessDest = pDest;
	u32 dmaUse = PUBLIC_CRYPTO_DMA_USE_NONE;

	/*
	 *Choice of the processing type
	 */
	if (nbBlocks * DES_BLOCK_SIZE >= DMA_TRIGGER_IRQ_DES)
		dmaUse = PUBLIC_CRYPTO_DMA_USE_IRQ;

	dprintk(KERN_INFO "PDrvCryptoUpdateDES: \
		pSrc=0x%08x, pDest=0x%08x, nbBlocks=0x%08x, dmaUse=0x%08x\n",
		(unsigned int)pSrc, (unsigned int)pDest,
		(unsigned int)nbBlocks, (unsigned int)dmaUse);

	if (nbBlocks == 0) {
		dprintk(KERN_INFO "PDrvCryptoUpdateDES: Nothing to process\n");
		return true;
	}

	if (DES_CTRL_GET_DIRECTION(INREG32(&pDESReg_t->DES_CTRL)) !=
		DES_CTRL_GET_DIRECTION(DES_CTRL)) {
		dprintk(KERN_WARNING "HWA configured for another direction\n");
		return false;
	}

	/*Restore the registers of the accelerator from the operation state */
	PDrvCryptoRestoreDESRegisters(DES_CTRL, pDESState);

	OUTREG32(&pDESReg_t->DES_LENGTH, nbBlocks * DES_BLOCK_SIZE);

	if (dmaUse == PUBLIC_CRYPTO_DMA_USE_IRQ) {

		/*perform the update with DMA */
		PDrvCryptoUpdateDESWithDMA(pProcessSrc, pProcessDest,
					nbBlocks);

	} else {

		for (nbr_of_blocks = 0;
			nbr_of_blocks < nbBlocks; nbr_of_blocks++) {

			/*We wait for the input ready */
			/*Crash the system as this should never occur */
			if (SCXPublicCryptoWaitForReadyBit(
				(u32 *)&pDESReg_t->DES_CTRL,
				DES_CTRL_INPUT_READY_BIT) !=
					PUBLIC_CRYPTO_OPERATION_SUCCESS) {
				panic("Wait too long for DES HW \
					accelerator Input data to be ready\n");
			}

			/*We copy the 8 bytes of data src->reg */
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pDESReg_t->DES_DATA_L, vTemp);
			pProcessSrc += 4;
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pDESReg_t->DES_DATA_H, vTemp);
			pProcessSrc += 4;

			/*We wait for the output ready */
			SCXPublicCryptoWaitForReadyBitInfinitely(
						(u32 *)&pDESReg_t->DES_CTRL,
						DES_CTRL_OUTPUT_READY_BIT);

			/*We copy the 8 bytes of data reg->dest */
			vTemp = INREG32(&pDESReg_t->DES_DATA_L);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
			vTemp = INREG32(&pDESReg_t->DES_DATA_H);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
		}
	}

	/*Save the accelerator registers into the operation state */
	PDrvCryptoSaveDESRegisters(DES_CTRL, pDESState);

	dprintk(KERN_INFO "PDrvCryptoUpdateDES: Done\n");
	return true;
}

/*------------------------------------------------------------------------- */
/*
 *Static function, perform DES encryption/decryption using the DMA for data
 *transfer.
 *
 *inputs: pSrc : pointer of the input data to process
 *        nbBlocks : number of block to process
 *        dmaUse : PUBLIC_CRYPTO_DMA_USE_IRQ (use irq to monitor end of DMA)
 *output: pDest : pointer of the output data (can be eq to pSrc)
 */
static void PDrvCryptoUpdateDESWithDMA(u8 *pSrc, u8 *pDest, u32 nbBlocks)
{
	/*
	 *Note: The DMA only sees physical addresses !
	 */

	int dma_ch0;
	int dma_ch1;
	struct omap_dma_channel_params ch0_parameters;
	struct omap_dma_channel_params ch1_parameters;
	u32 nLength = nbBlocks * DES_BLOCK_SIZE;
	u32 nLengthLoop = 0;
	u32 nbBlocksLoop = 0;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	dprintk(KERN_INFO
		"PDrvCryptoUpdateDESWithDMA: In=0x%08x, Out=0x%08x, Len=%u\n",
		(unsigned int)pSrc, (unsigned int)pDest,
		(unsigned int)nLength);

	/*lock the DMA */
	mutex_lock(&pDevice->sm.sDMALock);

	if (scxPublicDMARequest(&dma_ch0) != PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		mutex_unlock(&pDevice->sm.sDMALock);
		return;
	}
	if (scxPublicDMARequest(&dma_ch1) != PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		scxPublicDMARelease(dma_ch0);
		mutex_unlock(&pDevice->sm.sDMALock);
		return;
	}

	while (nLength > 0) {

		/*
		 * At this time, we are sure that the DMAchannels are available
		 * and not used by other public crypto operation
		 */

		/*DMA used for Input and Output */
		OUTREG32(&pDESReg_t->DES_SYSCONFIG,
				INREG32(&pDESReg_t->DES_SYSCONFIG)
			| DES_SYSCONFIG_DMA_REQ_OUT_EN_BIT
			| DES_SYSCONFIG_DMA_REQ_IN_EN_BIT);

		/* Check length */
		if (nLength <= pDevice->nDMABufferLength)
			nLengthLoop = nLength;
		else
			nLengthLoop = pDevice->nDMABufferLength;

		/* The length is always a multiple of the block size */
		nbBlocksLoop = nLengthLoop / DES_BLOCK_SIZE;

		/*
		 * Copy the data from the input buffer into a preallocated
		 * buffer which is aligned on the beginning of a page.  This
		 * may prevent potential issues when flushing/invalidating the
		 * buffer as the cache lines are 64 bytes long.
		 */
		memcpy(pDevice->pDMABuffer, pSrc, nLengthLoop);

		/* DMA1: Mem -> DES */
		scxPublicSetDMAChannelCommonParams(&ch0_parameters,
			nbBlocksLoop,
			DMA_CEN_Elts_per_Frame_DES,
			DES_REGS_HW_ADDR + 0x28,
			pDevice->pDMABufferPhys,
			OMAP44XX_DMA_DES_P_DATA_IN_REQ);

		ch0_parameters.src_amode = OMAP_DMA_AMODE_POST_INC;
		ch0_parameters.dst_amode = OMAP_DMA_AMODE_CONSTANT;
		ch0_parameters.src_or_dst_synch = OMAP_DMA_DST_SYNC;

		dprintk(KERN_INFO
			"PDrvCryptoUpdateDESWithDMA: \
			scxPublicDMASetParams(ch0)\n");
		scxPublicDMASetParams(dma_ch0, &ch0_parameters);

		/* DMA2: DES -> Mem */
		scxPublicSetDMAChannelCommonParams(&ch1_parameters,
			nbBlocksLoop,
			DMA_CEN_Elts_per_Frame_DES,
			pDevice->pDMABufferPhys,
			DES_REGS_HW_ADDR + 0x28,
			OMAP44XX_DMA_DES_P_DATA_OUT_REQ);

		ch1_parameters.src_amode = OMAP_DMA_AMODE_CONSTANT;
		ch1_parameters.dst_amode = OMAP_DMA_AMODE_POST_INC;
		ch1_parameters.src_or_dst_synch = OMAP_DMA_SRC_SYNC;

		dprintk(KERN_INFO "PDrvCryptoUpdateDESWithDMA: \
			scxPublicDMASetParams(ch1)\n");
		scxPublicDMASetParams(dma_ch1, &ch1_parameters);

		wmb();

		dprintk(KERN_INFO
			"PDrvCryptoUpdateDESWithDMA: Start DMA channel %d\n",
			(unsigned int)dma_ch0);
		scxPublicDMAStart(dma_ch0, OMAP_DMA_BLOCK_IRQ);

		dprintk(KERN_INFO
			"PDrvCryptoUpdateDESWithDMA: Start DMA channel %d\n",
			(unsigned int)dma_ch1);
		scxPublicDMAStart(dma_ch1, OMAP_DMA_BLOCK_IRQ);
		scxPublicDMAWait(2);

		/* Unset DMA synchronisation requests */
		OUTREG32(&pDESReg_t->DES_SYSCONFIG,
				INREG32(&pDESReg_t->DES_SYSCONFIG)
			& (~DES_SYSCONFIG_DMA_REQ_OUT_EN_BIT)
			& (~DES_SYSCONFIG_DMA_REQ_IN_EN_BIT));

		scxPublicDMAClearChannel(dma_ch0);
		scxPublicDMAClearChannel(dma_ch1);

		/*
		 * The dma transfer is complete
		 */

		/* The DMA output is in the preallocated aligned buffer
		 * and needs to be copied to the output buffer.*/
		memcpy(pDest, pDevice->pDMABuffer, nLengthLoop);

		pSrc += nLengthLoop;
		pDest += nLengthLoop;
		nLength -= nLengthLoop;
	}

	/* For safety reasons, let's clean the working buffer */
	memset(pDevice->pDMABuffer, 0, nLengthLoop);

	/* Release the DMA */
	scxPublicDMARelease(dma_ch0);
	scxPublicDMARelease(dma_ch1);

	mutex_unlock(&pDevice->sm.sDMALock);

	dprintk(KERN_INFO "PDrvCryptoUpdateDESWithDMA: Success\n");
}
