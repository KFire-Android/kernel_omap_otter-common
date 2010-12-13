/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
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
 *AES Hardware Accelerator: Base address
 */
#define AES1_REGS_HW_ADDR		0x4B501000
#define AES2_REGS_HW_ADDR		0x4B701000

/*
 *CTRL register Masks
 */
#define AES_CTRL_OUTPUT_READY_BIT	(1<<0)
#define AES_CTRL_INPUT_READY_BIT	(1<<1)

#define AES_CTRL_GET_DIRECTION(x)	(x&4)
#define AES_CTRL_DIRECTION_DECRYPT	0
#define AES_CTRL_DIRECTION_ENCRYPT	(1<<2)

#define AES_CTRL_GET_KEY_SIZE(x)	(x & 0x18)
#define AES_CTRL_KEY_SIZE_128		0x08
#define AES_CTRL_KEY_SIZE_192		0x10
#define AES_CTRL_KEY_SIZE_256		0x18

#define AES_CTRL_GET_MODE(x)		((x & 0x60) >> 5)
#define AES_CTRL_IS_MODE_CBC(x)		(AES_CTRL_GET_MODE(x) == 1)
#define AES_CTRL_IS_MODE_ECB(x)		(AES_CTRL_GET_MODE(x) == 0)
#define AES_CTRL_IS_MODE_CTR(x)		((AES_CTRL_GET_MODE(x) == 2) || \
					(AES_CTRL_GET_MODE(x) == 3))
#define AES_CTRL_MODE_CBC_BIT		0x20
#define AES_CTRL_MODE_ECB_BIT		0
#define AES_CTRL_MODE_CTR_BIT		0x40

#define AES_CTRL_GET_CTR_WIDTH(x)	(x&0x180)
#define AES_CTRL_CTR_WIDTH_32		0
#define AES_CTRL_CTR_WIDTH_64		0x80
#define AES_CTRL_CTR_WIDTH_96		0x100
#define AES_CTRL_CTR_WIDTH_128		0x180

/*
 * SYSCONFIG register masks
 */
#define AES_SYSCONFIG_DMA_REQ_IN_EN_BIT		(1 << 5)
#define AES_SYSCONFIG_DMA_REQ_OUT_EN_BIT	(1 << 6)


/*----------------------------------------------------------------------*/
/*			 AES Context					*/
/*----------------------------------------------------------------------*/
/**
 *This structure contains the registers of the AES HW accelerator.
 */
struct AESReg_t {
	u32 AES_KEY2_6;	/* 0x00 */
	u32 AES_KEY2_7;	/* 0xO4 */
	u32 AES_KEY2_4;	/* 0x08 */
	u32 AES_KEY2_5;	/* 0x0C */
	u32 AES_KEY2_2;	/* 0x10 */
	u32 AES_KEY2_3;	/* 0x14 */
	u32 AES_KEY2_0;	/* 0x18 */
	u32 AES_KEY2_1;	/* 0x1C */

	u32 AES_KEY1_6;	/* 0x20 */
	u32 AES_KEY1_7;	/* 0x24 */
	u32 AES_KEY1_4;	/* 0x28 */
	u32 AES_KEY1_5;	/* 0x2C */
	u32 AES_KEY1_2;	/* 0x30 */
	u32 AES_KEY1_3;	/* 0x34 */
	u32 AES_KEY1_0;	/* 0x38 */
	u32 AES_KEY1_1;	/* 0x3C */

	u32 AES_IV_IN_0;	/* 0x40 */
	u32 AES_IV_IN_1;	/* 0x44 */
	u32 AES_IV_IN_2;	/* 0x48 */
	u32 AES_IV_IN_3;	/* 0x4C */

	u32 AES_CTRL;		/* 0x50 */

	u32 AES_C_LENGTH_0;	/* 0x54 */
	u32 AES_C_LENGTH_1;	/* 0x58 */
	u32 AES_AUTH_LENGTH;	/* 0x5C */

	u32 AES_DATA_IN_0;	/* 0x60 */
	u32 AES_DATA_IN_1;	/* 0x64 */
	u32 AES_DATA_IN_2;	/* 0x68 */
	u32 AES_DATA_IN_3;	/* 0x6C */

	u32 AES_TAG_OUT_0;	/* 0x70 */
	u32 AES_TAG_OUT_1;	/* 0x74 */
	u32 AES_TAG_OUT_2;	/* 0x78 */
	u32 AES_TAG_OUT_3;	/* 0x7C */

	u32 AES_REVISION;	/* 0x80 */
	u32 AES_SYSCONFIG;	/* 0x84 */

	u32 AES_SYSSTATUS;	/* 0x88 */

};

static struct AESReg_t *pAESReg_t;

/*---------------------------------------------------------------------------
 *Forward declarations
 *------------------------------------------------------------------------- */

static void PDrvCryptoUpdateAESWithDMA(u8 *pSrc, u8 *pDest,
					u32 nbBlocks);

/*----------------------------------------------------------------------------
 *Save HWA registers into the specified operation state structure
 *--------------------------------------------------------------------------*/
static void PDrvCryptoSaveAESRegisters(u32 AES_CTRL,
	struct PUBLIC_CRYPTO_AES_OPERATION_STATE *pAESState)
{
	dprintk(KERN_INFO "PDrvCryptoSaveAESRegisters: \
		pAESState(%p) <- pAESReg_t(%p): CTRL=0x%08x\n",
		pAESState, pAESReg_t, AES_CTRL);

	/*Save the IV if we are in CBC or CTR mode (not required for ECB) */
	if (!AES_CTRL_IS_MODE_ECB(AES_CTRL)) {
		pAESState->AES_IV_0 = INREG32(&pAESReg_t->AES_IV_IN_0);
		pAESState->AES_IV_1 = INREG32(&pAESReg_t->AES_IV_IN_1);
		pAESState->AES_IV_2 = INREG32(&pAESReg_t->AES_IV_IN_2);
		pAESState->AES_IV_3 = INREG32(&pAESReg_t->AES_IV_IN_3);
	}
}

/*----------------------------------------------------------------------------
 *Restore the HWA registers from the operation state structure
 *---------------------------------------------------------------------------*/
void PDrvCryptoRestoreAESRegisters(u32 AES_CTRL,
	struct PUBLIC_CRYPTO_AES_OPERATION_STATE *pAESState)
{
	u32 ctrl = 0;

	dprintk(KERN_INFO "PDrvCryptoRestoreAESRegisters: \
		pAESReg_t(%p) <- pAESState(%p): CTRL=0x%08x\n",
		pAESReg_t, pAESState, AES_CTRL);

	/*
	 * Restore the IV first if we are in CBC or CTR mode
	 * (not required for ECB)
	 */
	if (!AES_CTRL_IS_MODE_ECB(AES_CTRL)) {
		OUTREG32(&pAESReg_t->AES_IV_IN_0, pAESState->AES_IV_0);
		OUTREG32(&pAESReg_t->AES_IV_IN_1, pAESState->AES_IV_1);
		OUTREG32(&pAESReg_t->AES_IV_IN_2, pAESState->AES_IV_2);
		OUTREG32(&pAESReg_t->AES_IV_IN_3, pAESState->AES_IV_3);
	}

	/* Then set the CTRL register:
	 * overwrite the CTRL only when needed, because unconditionally doing
	 * it leads to break the HWA process (observed by experimentation)
	 */

	ctrl = INREG32(&pAESReg_t->AES_CTRL);

	AES_CTRL = (ctrl & (3 << 3)) /* key size */
		|  (AES_CTRL & ((1 << 2) | (1 << 5) | (1 << 6)))
		|  (0x3 << 7) /* Always set CTR_WIDTH to 128-bit */;

	if ((AES_CTRL & 0x1FC) != (ctrl & 0x1FC))
		OUTREG32(&pAESReg_t->AES_CTRL, AES_CTRL & 0x1FC);

	/* Set the SYSCONFIG register to 0 */
	OUTREG32(&pAESReg_t->AES_SYSCONFIG, 0);
}

/*-------------------------------------------------------------------------- */

void PDrvCryptoAESInit(void)
{
	pAESReg_t = omap_ioremap(AES1_REGS_HW_ADDR, SZ_1M, MT_DEVICE);
	if (pAESReg_t == NULL)
		panic("Unable to remap AES1 module");
}

void PDrvCryptoAESExit(void)
{
	omap_iounmap(pAESReg_t);
}

void PDrvCryptoUpdateAES(u32 AES_CTRL,
	struct PUBLIC_CRYPTO_AES_OPERATION_STATE *pAESState,
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
	if (nbBlocks * AES_BLOCK_SIZE >= DMA_TRIGGER_IRQ_AES)
			dmaUse = PUBLIC_CRYPTO_DMA_USE_IRQ;

	dprintk(KERN_INFO "PDrvCryptoUpdateAES: \
		pSrc=0x%08x, pDest=0x%08x, nbBlocks=0x%08x, dmaUse=0x%08x\n",
		(unsigned int)pSrc,
		(unsigned int)pDest,
		(unsigned int)nbBlocks,
		(unsigned int)dmaUse);

	if (nbBlocks == 0) {
		dprintk(KERN_INFO "PDrvCryptoUpdateAES: Nothing to process\n");
		return;
	}

	/*Restore the registers of the accelerator from the operation state */
	PDrvCryptoRestoreAESRegisters(AES_CTRL, pAESState);

	if (dmaUse == PUBLIC_CRYPTO_DMA_USE_IRQ) {

		/*perform the update with DMA */
		PDrvCryptoUpdateAESWithDMA(pProcessSrc,
				pProcessDest, nbBlocks);

	} else {
		for (nbr_of_blocks = 0;
			nbr_of_blocks < nbBlocks; nbr_of_blocks++) {

			/*We wait for the input ready */

			/*Crash the system as this should never occur */
			if (SCXPublicCryptoWaitForReadyBit(
				(u32 *)&pAESReg_t->AES_CTRL,
				AES_CTRL_INPUT_READY_BIT) !=
					PUBLIC_CRYPTO_OPERATION_SUCCESS)
					panic("Wait too long for AES hardware \
					accelerator Input data to be ready\n");

			/* We copy the 16 bytes of data src->reg */
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_IN_0, vTemp);
			pProcessSrc += 4;
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_IN_1, vTemp);
			pProcessSrc += 4;
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_IN_2, vTemp);
			pProcessSrc += 4;
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_IN_3, vTemp);
			pProcessSrc += 4;

			/* We wait for the output ready */
			SCXPublicCryptoWaitForReadyBitInfinitely(
					(u32 *)&pAESReg_t->AES_CTRL,
					AES_CTRL_OUTPUT_READY_BIT);

			/* We copy the 16 bytes of data reg->dest */
			vTemp = INREG32(&pAESReg_t->AES_DATA_IN_0);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
			vTemp = INREG32(&pAESReg_t->AES_DATA_IN_1);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
			vTemp = INREG32(&pAESReg_t->AES_DATA_IN_2);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
			vTemp = INREG32(&pAESReg_t->AES_DATA_IN_3);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
		}
	}

	/* Save the accelerator registers into the operation state */
	PDrvCryptoSaveAESRegisters(AES_CTRL, pAESState);

	dprintk(KERN_INFO "PDrvCryptoUpdateAES: Done\n");
}

/*-------------------------------------------------------------------------- */
/*
 *Static function, perform AES encryption/decryption using the DMA for data
 *transfer.
 *
 *inputs: pSrc : pointer of the input data to process
 *        nbBlocks : number of block to process
 *        dmaUse : PUBLIC_CRYPTO_DMA_USE_IRQ (use irq to monitor end of DMA)
 *                     | PUBLIC_CRYPTO_DMA_USE_POLLING (poll the end of DMA)
 *output: pDest : pointer of the output data (can be eq to pSrc)
 */
static void PDrvCryptoUpdateAESWithDMA(u8 *pSrc, u8 *pDest, u32 nbBlocks)
{
	/*
	 *Note: The DMA only sees physical addresses !
	 */

	int dma_ch0;
	int dma_ch1;
	struct omap_dma_channel_params ch0_parameters;
	struct omap_dma_channel_params ch1_parameters;
	u32 nLength = nbBlocks * AES_BLOCK_SIZE;
	u32 nLengthLoop = 0;
	u32 nbBlocksLoop = 0;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	dprintk(KERN_INFO
		"PDrvCryptoUpdateAESWithDMA: In=0x%08x, Out=0x%08x, Len=%u\n",
		(unsigned int)pSrc,
		(unsigned int)pDest,
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
		 * At this time, we are sure that the DMAchannels
		 *are available and not used by other public crypto operation
		 */

		/*DMA used for Input and Output */
		OUTREG32(&pAESReg_t->AES_SYSCONFIG,
			INREG32(&pAESReg_t->AES_SYSCONFIG)
			| AES_SYSCONFIG_DMA_REQ_OUT_EN_BIT
			| AES_SYSCONFIG_DMA_REQ_IN_EN_BIT);

		/*check length */
		if (nLength <= pDevice->nDMABufferLength)
			nLengthLoop = nLength;
		else
			nLengthLoop = pDevice->nDMABufferLength;

		/*The length is always a multiple of the block size */
		nbBlocksLoop = nLengthLoop / AES_BLOCK_SIZE;

		/*
		 *Copy the data from the input buffer into a preallocated
		 *buffer which is aligned on the beginning of a page.
		 *This may prevent potential issues when flushing/invalidating
		 *the buffer as the cache lines are 64 bytes long.
		 */
		memcpy(pDevice->pDMABuffer, pSrc, nLengthLoop);

		/*DMA1: Mem -> AES */
		scxPublicSetDMAChannelCommonParams(&ch0_parameters,
			nbBlocksLoop,
			DMA_CEN_Elts_per_Frame_AES,
			AES1_REGS_HW_ADDR + 0x60,
			(u32)pDevice->pDMABufferPhys,
			OMAP44XX_DMA_AES1_P_DATA_IN_REQ);

		ch0_parameters.src_amode = OMAP_DMA_AMODE_POST_INC;
		ch0_parameters.dst_amode = OMAP_DMA_AMODE_CONSTANT;
		ch0_parameters.src_or_dst_synch = OMAP_DMA_DST_SYNC;

		dprintk(KERN_INFO "PDrvCryptoUpdateAESWithDMA: \
				scxPublicDMASetParams(ch0)\n");
		scxPublicDMASetParams(dma_ch0, &ch0_parameters);

		omap_set_dma_src_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_16);
		omap_set_dma_dest_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_16);

		/*DMA2: AES -> Mem */
		scxPublicSetDMAChannelCommonParams(&ch1_parameters,
			nbBlocksLoop,
			DMA_CEN_Elts_per_Frame_AES,
			(u32)pDevice->pDMABufferPhys,
			AES1_REGS_HW_ADDR + 0x60,
			OMAP44XX_DMA_AES1_P_DATA_OUT_REQ);

		ch1_parameters.src_amode = OMAP_DMA_AMODE_CONSTANT;
		ch1_parameters.dst_amode = OMAP_DMA_AMODE_POST_INC;
		ch1_parameters.src_or_dst_synch = OMAP_DMA_SRC_SYNC;

		dprintk(KERN_INFO "PDrvCryptoUpdateAESWithDMA: \
			scxPublicDMASetParams(ch1)\n");
		scxPublicDMASetParams(dma_ch1, &ch1_parameters);

		omap_set_dma_src_burst_mode(dma_ch1, OMAP_DMA_DATA_BURST_16);
		omap_set_dma_dest_burst_mode(dma_ch1, OMAP_DMA_DATA_BURST_16);

		wmb();

		dprintk(KERN_INFO
			"PDrvCryptoUpdateAESWithDMA: Start DMA channel %d\n",
			(unsigned int)dma_ch1);
		scxPublicDMAStart(dma_ch1, OMAP_DMA_BLOCK_IRQ);
		dprintk(KERN_INFO
			"PDrvCryptoUpdateAESWithDMA: Start DMA channel %d\n",
			(unsigned int)dma_ch0);
		scxPublicDMAStart(dma_ch0, OMAP_DMA_BLOCK_IRQ);

		dprintk(KERN_INFO
			"PDrvCryptoUpdateAESWithDMA: Waiting for IRQ\n");
		scxPublicDMAWait(2);

		/*Unset DMA synchronisation requests */
		OUTREG32(&pAESReg_t->AES_SYSCONFIG,
				INREG32(&pAESReg_t->AES_SYSCONFIG)
			& (~AES_SYSCONFIG_DMA_REQ_OUT_EN_BIT)
			& (~AES_SYSCONFIG_DMA_REQ_IN_EN_BIT));

		scxPublicDMAClearChannel(dma_ch0);
		scxPublicDMAClearChannel(dma_ch1);

		/*
		 *The dma transfer is complete
		 */

		/*The DMA output is in the preallocated aligned buffer
		 *and needs to be copied to the output buffer.*/
		memcpy(pDest, pDevice->pDMABuffer, nLengthLoop);

		pSrc += nLengthLoop;
		pDest += nLengthLoop;
		nLength -= nLengthLoop;
	}

	/*For safety reasons, let's clean the working buffer */
	memset(pDevice->pDMABuffer, 0, nLengthLoop);

	/*release the DMA */
	scxPublicDMARelease(dma_ch0);
	scxPublicDMARelease(dma_ch1);

	mutex_unlock(&pDevice->sm.sDMALock);

	dprintk(KERN_INFO "PDrvCryptoUpdateAESWithDMA: Success\n");
}
