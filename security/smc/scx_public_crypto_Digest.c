/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scx_public_crypto.h"
#include "scx_public_dma.h"

#include <asm/io.h>
#include <mach/io.h>

/*
 * SHA2/MD5 Hardware Accelerator: Base address for SHA2/MD5 HIB2
 * This is referenced as the SHA2MD5 module in the Crypto TRM
 */
#define DIGEST1_REGS_HW_ADDR			0x4B101000

/*
 * IRQSTATUS register Masks
 */
#define DIGEST_IRQSTATUS_OUTPUT_READY_BIT	(1 << 0)
#define DIGEST_IRQSTATUS_INPUT_READY_BIT	(1 << 1)
#define DIGEST_IRQSTATUS_PARTHASH_READY_BIT	(1 << 2)
#define DIGEST_IRQSTATUS_CONTEXT_READY_BIT	(1 << 3)

/*
 * MODE register Masks
 */
#define DIGEST_MODE_GET_ALGO(x)			((x & 0x6) >> 1)
#define DIGEST_MODE_SET_ALGO(x, a)		((a << 1) | (x & 0xFFFFFFF9))

#define DIGEST_MODE_ALGO_CONST_BIT		(1 << 3)
#define DIGEST_MODE_CLOSE_HASH_BIT		(1 << 4)

/*
 * SYSCONFIG register masks
 */
#define DIGEST_SYSCONFIG_PIT_EN_BIT		(1 << 2)
#define DIGEST_SYSCONFIG_PDMA_EN_BIT		(1 << 3)
#define DIGEST_SYSCONFIG_PCONT_SWT_BIT		(1 << 6)
#define DIGEST_SYSCONFIG_PADVANCED_BIT		(1 << 7)

/*-------------------------------------------------------------------------*/
/*				 Digest Context				*/
/*-------------------------------------------------------------------------*/
/**
 * This structure contains the registers of the SHA1/MD5 HW accelerator.
 */
struct Sha1Md5Reg_t {
	u32 ODIGEST_A;		/* 0x00 Outer Digest A      */
	u32 ODIGEST_B;		/* 0x04 Outer Digest B      */
	u32 ODIGEST_C;		/* 0x08 Outer Digest C      */
	u32 ODIGEST_D;		/* 0x0C Outer Digest D      */
	u32 ODIGEST_E;		/* 0x10 Outer Digest E      */
	u32 ODIGEST_F;		/* 0x14 Outer Digest F      */
	u32 ODIGEST_G;		/* 0x18 Outer Digest G      */
	u32 ODIGEST_H;		/* 0x1C Outer Digest H      */
	u32 IDIGEST_A;		/* 0x20 Inner Digest A      */
	u32 IDIGEST_B;		/* 0x24 Inner Digest B      */
	u32 IDIGEST_C;		/* 0x28 Inner Digest C      */
	u32 IDIGEST_D;		/* 0x2C Inner Digest D      */
	u32 IDIGEST_E;		/* 0x30 Inner Digest E      */
	u32 IDIGEST_F;		/* 0x34 Inner Digest F      */
	u32 IDIGEST_G;		/* 0x38 Inner Digest G      */
	u32 IDIGEST_H;		/* 0x3C Inner Digest H      */
	u32 DIGEST_COUNT;	/* 0x40 Digest count        */
	u32 MODE;		/* 0x44 Digest mode         */
	u32 LENGTH;		/* 0x48 Data length         */

	u32 reserved0[13];

	u32 DIN_0;		/* 0x80 Data 0              */
	u32 DIN_1;		/* 0x84 Data 1              */
	u32 DIN_2;		/* 0x88 Data 2              */
	u32 DIN_3;		/* 0x8C Data 3              */
	u32 DIN_4;		/* 0x90 Data 4              */
	u32 DIN_5;		/* 0x94 Data 5              */
	u32 DIN_6;		/* 0x98 Data 6              */
	u32 DIN_7;		/* 0x9C Data 7              */
	u32 DIN_8;		/* 0xA0 Data 8              */
	u32 DIN_9;		/* 0xA4 Data 9              */
	u32 DIN_10;		/* 0xA8 Data 10             */
	u32 DIN_11;		/* 0xAC Data 11             */
	u32 DIN_12;		/* 0xB0 Data 12             */
	u32 DIN_13;		/* 0xB4 Data 13             */
	u32 DIN_14;		/* 0xB8 Data 14             */
	u32 DIN_15;		/* 0xBC Data 15             */

	u32 reserved1[16];

	u32 REVISION;		/* 0x100 Revision           */

	u32 reserved2[3];

	u32 SYSCONFIG;		/* 0x110 Config             */
	u32 SYSSTATUS;		/* 0x114 Status             */
	u32 IRQSTATUS;		/* 0x118 IRQ Status         */
	u32 IRQENABLE;		/* 0x11C IRQ Enable         */
};

static struct Sha1Md5Reg_t *pSha1Md5Reg_t;

/*------------------------------------------------------------------------
 *Forward declarations
 *------------------------------------------------------------------------- */

static void static_Hash_HwPerform64bDigest(u32 *pData,
				u32 nAlgo, u32 nBytesProcessed);
static void static_Hash_HwPerformDmaDigest(u8 *pData, u32 nDataLength,
				u32 nAlgo, u32 nBytesProcessed);

static void PDrvCryptoUpdateHashWithDMA(u32 SHA_CTRL,
	struct PUBLIC_CRYPTO_SHA_OPERATION_STATE *pSHAState,
	u8 *pData, u32 dataLength);


/*-------------------------------------------------------------------------
 *Save HWA registers into the specified operation state structure
 *------------------------------------------------------------------------*/
static void PDrvCryptoSaveHashRegisters(
	struct PUBLIC_CRYPTO_SHA_OPERATION_STATE *pSHAState)
{
	dprintk(KERN_INFO "PDrvCryptoSaveHashRegisters: State=%p\n",
		pSHAState);

	pSHAState->SHA_DIGEST_A = INREG32(&pSha1Md5Reg_t->IDIGEST_A);
	pSHAState->SHA_DIGEST_B = INREG32(&pSha1Md5Reg_t->IDIGEST_B);
	pSHAState->SHA_DIGEST_C = INREG32(&pSha1Md5Reg_t->IDIGEST_C);
	pSHAState->SHA_DIGEST_D = INREG32(&pSha1Md5Reg_t->IDIGEST_D);
	pSHAState->SHA_DIGEST_E = INREG32(&pSha1Md5Reg_t->IDIGEST_E);
	pSHAState->SHA_DIGEST_F = INREG32(&pSha1Md5Reg_t->IDIGEST_F);
	pSHAState->SHA_DIGEST_G = INREG32(&pSha1Md5Reg_t->IDIGEST_G);
	pSHAState->SHA_DIGEST_H = INREG32(&pSha1Md5Reg_t->IDIGEST_H);
}

/*-------------------------------------------------------------------------
 *Restore the HWA registers from the operation state structure
 *-------------------------------------------------------------------------*/
static void PDrvCryptoRestoreHashRegisters(
	struct PUBLIC_CRYPTO_SHA_OPERATION_STATE *pSHAState)
{
	dprintk(KERN_INFO "PDrvCryptoRestoreHashRegisters: State=%p\n",
		pSHAState);

	if (pSHAState->nBytesProcessed != 0) {
		/*
		 * Some bytes were already processed. Initialize
		 * previous digest
		 */
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_A, pSHAState->SHA_DIGEST_A);
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_B, pSHAState->SHA_DIGEST_B);
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_C, pSHAState->SHA_DIGEST_C);
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_D, pSHAState->SHA_DIGEST_D);
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_E, pSHAState->SHA_DIGEST_E);
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_F, pSHAState->SHA_DIGEST_F);
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_G, pSHAState->SHA_DIGEST_G);
		OUTREG32(&pSha1Md5Reg_t->IDIGEST_H, pSHAState->SHA_DIGEST_H);
	}

	OUTREG32(&pSha1Md5Reg_t->SYSCONFIG, 0);
}

/*------------------------------------------------------------------------- */

void PDrvCryptoDigestInit(void)
{
	pSha1Md5Reg_t = omap_ioremap(DIGEST1_REGS_HW_ADDR, SZ_1M, MT_DEVICE);
	if (pSha1Md5Reg_t == NULL)
		panic("Unable to remap SHA2/MD5 module");
}

void PDrvCryptoDigestExit(void)
{
	omap_iounmap(pSha1Md5Reg_t);
}

void PDrvCryptoUpdateHash(u32 SHA_CTRL,
	struct PUBLIC_CRYPTO_SHA_OPERATION_STATE *pSHAState,
	u8 *pData, u32 dataLength)
{
	u32 dmaUse = PUBLIC_CRYPTO_DMA_USE_NONE;

	/*
	 *Choice of the processing type
	 */
	if (dataLength >= DMA_TRIGGER_IRQ_DIGEST)
		dmaUse = PUBLIC_CRYPTO_DMA_USE_IRQ;

	dprintk(KERN_INFO "PDrvCryptoUpdateHash : \
		Data=0x%08x/%u, Chunck=%u, Processed=%u, dmaUse=0x%08x\n",
		(u32)pData, (u32)dataLength,
		pSHAState->nChunkLength, pSHAState->nBytesProcessed,
		dmaUse);

	if (dataLength == 0) {
		dprintk(KERN_INFO "PDrvCryptoUpdateHash: \
				Nothing to process\n");
		return;
	}

	if (dmaUse != PUBLIC_CRYPTO_DMA_USE_NONE) {
		/*
		 * Restore the registers of the accelerator from the operation
		 * state
		 */
		PDrvCryptoRestoreHashRegisters(pSHAState);

		/*perform the updates with DMA */
		PDrvCryptoUpdateHashWithDMA(SHA_CTRL,
				pSHAState, pData, dataLength);

		/* Save the accelerator registers into the operation state */
		PDrvCryptoSaveHashRegisters(pSHAState);
	} else {
		/*Non-DMA transfer */

		/*(1)We take the chunk buffer wich contains the last saved
		 *data that could not be yet processed because we had not
		 *enough data to make a 64B buffer. Then we try to make a
		 *64B buffer by concatenating it with the new passed data
		 */

		/*Is there any data in the chunk? If yes is it possible to
		 *make a 64B buffer with the new data passed ? */
		if ((pSHAState->nChunkLength != 0)
			&& (pSHAState->nChunkLength + dataLength >=
						HASH_BLOCK_BYTES_LENGTH)) {

			u8 vLengthToComplete =
			HASH_BLOCK_BYTES_LENGTH - pSHAState->nChunkLength;

			/*So we fill the chunk buffer with the new data to
			 *complete to 64B */
			memcpy(pSHAState->pChunkBuffer + pSHAState->
				nChunkLength, pData, vLengthToComplete);

			if (pSHAState->nChunkLength + dataLength ==
				HASH_BLOCK_BYTES_LENGTH) {
				/*We'll keep some data for the final */
				pSHAState->nChunkLength =
					HASH_BLOCK_BYTES_LENGTH;
				dprintk(KERN_INFO "PDrvCryptoUpdateHash: \
					Done: Chunck=%u; Processed=%u\n",
					pSHAState->nChunkLength,
					pSHAState->nBytesProcessed);
				return;
			}

			/*
			 * Restore the registers of the accelerator from the
			 * operation state
			 */
			PDrvCryptoRestoreHashRegisters(pSHAState);

			/*Then we send this buffer to the HWA */
			static_Hash_HwPerform64bDigest(
				(u32 *)pSHAState->pChunkBuffer, SHA_CTRL,
				pSHAState->nBytesProcessed);

			/*
			 * Save the accelerator registers into the operation
			 * state
			 */
			PDrvCryptoSaveHashRegisters(pSHAState);

			pSHAState->nBytesProcessed =
				INREG32(&pSha1Md5Reg_t->DIGEST_COUNT);

			/*We have flushed the chunk so it is empty now */
			pSHAState->nChunkLength = 0;

			/*Then we have less data to process */
			pData += vLengthToComplete;
			dataLength -= vLengthToComplete;
		}

		/*(2)We process all the 64B buffer that we can */
		if (pSHAState->nChunkLength + dataLength >=
					HASH_BLOCK_BYTES_LENGTH) {

			while (dataLength > HASH_BLOCK_BYTES_LENGTH) {
				u8 pTempAlignedBuffer[HASH_BLOCK_BYTES_LENGTH];

				/*
				 *We process a 64B buffer
				 */
				/*We copy the data to process to an aligned
				 *buffer */
				memcpy(pTempAlignedBuffer, pData,
					HASH_BLOCK_BYTES_LENGTH);

				/*Then we send this buffer to the hash
				 *hardware */
				PDrvCryptoRestoreHashRegisters(pSHAState);
				static_Hash_HwPerform64bDigest(
					(u32 *)pTempAlignedBuffer, SHA_CTRL,
						pSHAState->nBytesProcessed);
				PDrvCryptoSaveHashRegisters(pSHAState);

				pSHAState->nBytesProcessed =
					INREG32(&pSha1Md5Reg_t->DIGEST_COUNT);

				/*Then we decrease the remaining data of 64B */
				pData += HASH_BLOCK_BYTES_LENGTH;
				dataLength -= HASH_BLOCK_BYTES_LENGTH;
			}
		}

		/*(3)We look if we have some data that could not be processed
		 *yet because it is not large enough to fill a buffer of 64B */
		if (dataLength > 0) {
			if (pSHAState->nChunkLength + dataLength >
					HASH_BLOCK_BYTES_LENGTH) {
				/*Should never be in this case !!! */
			panic("PDrvCryptoUpdateHash: nChunkLength + \
				dataLength > HASH_BLOCK_BYTES_LENGTH\n");
			}

			/*So we fill the chunk buffer with the new data to
			 *complete to 64B */
			memcpy(pSHAState->pChunkBuffer + pSHAState->
				nChunkLength, pData, dataLength);
			pSHAState->nChunkLength += dataLength;
		}
	}

	dprintk(KERN_INFO "PDrvCryptoUpdateHash: Done: \
		Chunck=%u; Processed=%u\n",
		pSHAState->nChunkLength, pSHAState->nBytesProcessed);
}

/*------------------------------------------------------------------------- */

static void static_Hash_HwPerform64bDigest(u32 *pData,
					u32 nAlgo, u32 nBytesProcessed)
{
	u32 nAlgoConstant = 0;

	OUTREG32(&pSha1Md5Reg_t->DIGEST_COUNT, nBytesProcessed);

	if (nBytesProcessed == 0) {
		/* No bytes processed so far. Will use the algo constant instead
			of previous digest */
		nAlgoConstant = 1 << 3;
	}

	OUTREG32(&pSha1Md5Reg_t->MODE,
		nAlgoConstant | (nAlgo & 0x6));
	OUTREG32(&pSha1Md5Reg_t->LENGTH, HASH_BLOCK_BYTES_LENGTH);

	if (SCXPublicCryptoWaitForReadyBit(
		(u32 *)&pSha1Md5Reg_t->IRQSTATUS,
		DIGEST_IRQSTATUS_INPUT_READY_BIT)
			!= PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		/* Crash the system as this should never occur */
		panic("Wait too long for DIGEST HW accelerator" \
		      "Input data to be ready\n");
	}

	/*
	 *The pData buffer is a buffer of 64 bytes.
	 */
	OUTREG32(&pSha1Md5Reg_t->DIN_0, pData[0]);
	OUTREG32(&pSha1Md5Reg_t->DIN_1, pData[1]);
	OUTREG32(&pSha1Md5Reg_t->DIN_2, pData[2]);
	OUTREG32(&pSha1Md5Reg_t->DIN_3, pData[3]);
	OUTREG32(&pSha1Md5Reg_t->DIN_4, pData[4]);
	OUTREG32(&pSha1Md5Reg_t->DIN_5, pData[5]);
	OUTREG32(&pSha1Md5Reg_t->DIN_6, pData[6]);
	OUTREG32(&pSha1Md5Reg_t->DIN_7, pData[7]);
	OUTREG32(&pSha1Md5Reg_t->DIN_8, pData[8]);
	OUTREG32(&pSha1Md5Reg_t->DIN_9, pData[9]);
	OUTREG32(&pSha1Md5Reg_t->DIN_10, pData[10]);
	OUTREG32(&pSha1Md5Reg_t->DIN_11, pData[11]);
	OUTREG32(&pSha1Md5Reg_t->DIN_12, pData[12]);
	OUTREG32(&pSha1Md5Reg_t->DIN_13, pData[13]);
	OUTREG32(&pSha1Md5Reg_t->DIN_14, pData[14]);
	OUTREG32(&pSha1Md5Reg_t->DIN_15, pData[15]);

	/*
	 *Wait until the hash operation is finished.
	 */
	SCXPublicCryptoWaitForReadyBitInfinitely(
		(u32 *)&pSha1Md5Reg_t->IRQSTATUS,
		DIGEST_IRQSTATUS_OUTPUT_READY_BIT);
}

/*------------------------------------------------------------------------- */

static void static_Hash_HwPerformDmaDigest(u8 *pData, u32 nDataLength,
			u32 nAlgo, u32 nBytesProcessed)
{
	/*
	 *Note: The DMA only sees physical addresses !
	 */

	int dma_ch0;
	struct omap_dma_channel_params ch0_parameters;
	u32 nLengthLoop = 0;
	u32 nAlgoConstant;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	dprintk(KERN_INFO
		"static_Hash_HwPerformDmaDigest: Buffer=0x%08x/%u\n",
		(u32)pData, (u32)nDataLength);

	/*lock the DMA */
	mutex_lock(&pDevice->sm.sDMALock);
	if (scxPublicDMARequest(&dma_ch0) != PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		mutex_unlock(&pDevice->sm.sDMALock);
		return;
	}

	while (nDataLength > 0) {

		nAlgoConstant = 0;
		if (nBytesProcessed == 0) {
			/*No bytes processed so far. Will use the algo
			 *constant instead of previous digest */
			nAlgoConstant = 1 << 3;
		}

		/*check length */
		if (nDataLength <= pDevice->nDMABufferLength)
			nLengthLoop = nDataLength;
		else
			nLengthLoop = pDevice->nDMABufferLength;

		/*
		 *Copy the data from the input buffer into a preallocated
		 *buffer which is aligned on the beginning of a page.
		 *This may prevent potential issues when flushing/invalidating
		 *the buffer as the cache lines are 64 bytes long.
		 */
		memcpy(pDevice->pDMABuffer, pData, nLengthLoop);

		/*DMA1: Mem -> HASH */
		scxPublicSetDMAChannelCommonParams(&ch0_parameters,
			nLengthLoop / HASH_BLOCK_BYTES_LENGTH,
			DMA_CEN_Elts_per_Frame_SHA,
			DIGEST1_REGS_HW_ADDR + 0x80,
			pDevice->pDMABufferPhys,
			OMAP44XX_DMA_SHA2_DIN_P);

		/*specific for Mem -> HWA */
		ch0_parameters.src_amode = OMAP_DMA_AMODE_POST_INC;
		ch0_parameters.dst_amode = OMAP_DMA_AMODE_CONSTANT;
		ch0_parameters.src_or_dst_synch = OMAP_DMA_DST_SYNC;

		scxPublicDMASetParams(dma_ch0, &ch0_parameters);

		omap_set_dma_src_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_16);
		omap_set_dma_dest_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_16);

		OUTREG32(&pSha1Md5Reg_t->DIGEST_COUNT, nBytesProcessed);
		OUTREG32(&pSha1Md5Reg_t->MODE,
			nAlgoConstant | (nAlgo & 0x6));

		/*
		 * Triggers operation
		 * Interrupt, Free Running + GO (DMA on)
		 */
		OUTREG32(&pSha1Md5Reg_t->SYSCONFIG,
			INREG32(&pSha1Md5Reg_t->SYSCONFIG) |
			DIGEST_SYSCONFIG_PDMA_EN_BIT);
		OUTREG32(&pSha1Md5Reg_t->LENGTH, nLengthLoop);

		wmb();

		scxPublicDMAStart(dma_ch0, OMAP_DMA_BLOCK_IRQ);

		scxPublicDMAWait(1);

		OUTREG32(&pSha1Md5Reg_t->SYSCONFIG, 0);

		scxPublicDMAClearChannel(dma_ch0);

		pData += nLengthLoop;
		nDataLength -= nLengthLoop;
		nBytesProcessed =
			INREG32(&pSha1Md5Reg_t->DIGEST_COUNT);
	}

	/*For safety reasons, let's clean the working buffer */
	memset(pDevice->pDMABuffer, 0, nLengthLoop);

	/*release the DMA */
	scxPublicDMARelease(dma_ch0);

	mutex_unlock(&pDevice->sm.sDMALock);

	/*
	 * The dma transfert is finished, now wait until the hash
	 * operation is finished.
	 */
	SCXPublicCryptoWaitForReadyBitInfinitely(
		(u32 *)&pSha1Md5Reg_t->IRQSTATUS,
		DIGEST_IRQSTATUS_CONTEXT_READY_BIT);
}

/*------------------------------------------------------------------------- */
/*
 *Static function, perform data digest using the DMA for data transfer.
 *
 *inputs:
 *        pData : pointer of the input data to process
 *        dataLength : number of byte to process
 */
static void PDrvCryptoUpdateHashWithDMA(u32 nSHA_CTRL,
	struct PUBLIC_CRYPTO_SHA_OPERATION_STATE *pSHAState,
	u8 *pData, u32 dataLength)
{
	dprintk(KERN_INFO "PDrvCryptoUpdateHashWithDMA\n");

	if (pSHAState->nChunkLength != 0) {

		u32 vLengthToComplete;

		/*Fill the chunk first */
		if (pSHAState->
			nChunkLength + dataLength <= HASH_BLOCK_BYTES_LENGTH) {

			/*So we fill the chunk buffer with the new data */
			memcpy(pSHAState->
				pChunkBuffer + pSHAState->nChunkLength,
				pData, dataLength);
			pSHAState->nChunkLength += dataLength;

			/*We'll keep some data for the final */
			return;
		}

		vLengthToComplete = HASH_BLOCK_BYTES_LENGTH - pSHAState->
								nChunkLength;

		if (vLengthToComplete != 0) {
			/*So we fill the chunk buffer with the new data to
			 *complete to 64B */
			memcpy(pSHAState->pChunkBuffer + pSHAState->
				nChunkLength, pData, vLengthToComplete);
		}

		/*Then we send this buffer to the HWA (no DMA) */
		static_Hash_HwPerform64bDigest(
			(u32 *)pSHAState->pChunkBuffer, nSHA_CTRL,
					pSHAState->nBytesProcessed);

		pSHAState->nBytesProcessed =
			INREG32(&pSha1Md5Reg_t->DIGEST_COUNT);

		/*We have flushed the chunk so it is empty now */
		pSHAState->nChunkLength = 0;

		/*Update the data buffer depending of the data already
		 *processed */
		pData += vLengthToComplete;
		dataLength -= vLengthToComplete;
	}

	if (dataLength > HASH_BLOCK_BYTES_LENGTH) {

		/*DMA only manages data length that is multiple of 64b */
		u32 vDmaProcessSize = dataLength & 0xFFFFFFC0;

		if (vDmaProcessSize == dataLength) {
			/*We keep one block for the final */
			vDmaProcessSize -= HASH_BLOCK_BYTES_LENGTH;
		}

		static_Hash_HwPerformDmaDigest(pData, vDmaProcessSize,
			nSHA_CTRL, pSHAState->nBytesProcessed);

		pSHAState->nBytesProcessed =
			INREG32(&pSha1Md5Reg_t->DIGEST_COUNT);
		pData += vDmaProcessSize;
		dataLength -= vDmaProcessSize;
	}

	/*At that point, there is less than 64b left to process*/
	if ((dataLength == 0) || (dataLength > HASH_BLOCK_BYTES_LENGTH)) {
		/*Should never be in this case !!! */
		panic("PDrvCryptoUpdateHASHWithDMA: \
			Remaining dataLength=%u\n", dataLength);
	}

	/*We now fill the chunk buffer with the remaining data */
	memcpy(pSHAState->pChunkBuffer, pData, dataLength);
	pSHAState->nChunkLength = dataLength;
}
