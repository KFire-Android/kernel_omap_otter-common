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

#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scx_public_crypto.h"
#include "scx_public_dma.h"

#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/algapi.h>
#include <crypto/scatterwalk.h>
#include <crypto/aes.h>

/*
 *AES Hardware Accelerator: Base address
 */
#define AES1_REGS_HW_ADDR		0x480A6000

/*
 *CTRL register Masks
 */
#define AES_CTRL_OUTPUT_READY_BIT	(1<<0)
#define AES_CTRL_INPUT_READY_BIT		(1<<1)

#define AES_CTRL_GET_DIRECTION(x)	(x&4)
#define AES_CTRL_DIRECTION_DECRYPT	0
#define AES_CTRL_DIRECTION_ENCRYPT	(1<<2)

#define AES_CTRL_GET_KEY_SIZE(x)		(x&0x18)
#define AES_CTRL_KEY_SIZE_128			0x08
#define AES_CTRL_KEY_SIZE_192			0x10
#define AES_CTRL_KEY_SIZE_256			0x18

#define AES_CTRL_GET_MODE(x)			((x&0x60) >> 5)
#define AES_CTRL_IS_MODE_CBC(x)		(AES_CTRL_GET_MODE(x) == 1)
#define AES_CTRL_IS_MODE_ECB(x)		(AES_CTRL_GET_MODE(x) == 0)
#define AES_CTRL_IS_MODE_CTR(x)		((AES_CTRL_GET_MODE(x) == 2) || \
					(AES_CTRL_GET_MODE(x) == 3))
#define AES_CTRL_MODE_CBC_BIT			0x20
#define AES_CTRL_MODE_ECB_BIT			0
#define AES_CTRL_MODE_CTR_BIT			0x40

#define AES_CTRL_GET_CTR_WIDTH(x)	(x&0x180)
#define AES_CTRL_CTR_WIDTH_32			0
#define AES_CTRL_CTR_WIDTH_64			0x80
#define AES_CTRL_CTR_WIDTH_96			0x100
#define AES_CTRL_CTR_WIDTH_128		0x180

/*
 *MASK register masks
 */
#define AES_MASK_AUTOIDLE_BIT			(1<<0)
#define AES_MASK_SOFTRESET_BIT		(1<<1)
#define AES_MASK_DMA_REQ_IN_EN_BIT	(1<<2)
#define AES_MASK_DMA_REQ_OUT_EN_BIT	(1<<3)
#define AES_MASK_DIRECT_BUS_EN_BIT	(1<<4)
#define AES_MASK_START_BIT				(1<<5)

#define AES_MASK_GET_SIDLE(x)			((x&0xC0) >> 6)
#define AES_MASK_SIDLE_FORCE_IDLE	0
#define AES_MASK_SIDLE_NO_IDLE		1
#define AES_MASK_SIDLE_SMART_IDLE	2

/*
 *SYSTATUS register masks
 */
#define AES_SYSTATUS_RESET_DONE		(1<<0)

/*----------------------------------------------------------------------*/
/*			 AES Context					*/
/*----------------------------------------------------------------------*/
/**
 *This structure contains the registers of the AES HW accelerator.
 */
typedef struct {
	VU32 AES_KEY4_L;	/*0x00 */
	VU32 AES_KEY4_H;	/*0x04 */
	VU32 AES_KEY3_L;	/*0x08 */
	VU32 AES_KEY3_H;	/*0x0C */
	VU32 AES_KEY2_L;	/*0x10 */
	VU32 AES_KEY2_H;	/*0x14 */
	VU32 AES_KEY1_L;	/*0x18 */
	VU32 AES_KEY1_H;	/*0x1c */

	VU32 AES_IV_1;		/*0x20 */
	VU32 AES_IV_2;		/*0x24 */
	VU32 AES_IV_3;		/*0x28 */
	VU32 AES_IV_4;		/*0x2C */

	VU32 AES_CTRL;		/*0x30 */

	VU32 AES_DATA_1;	/*0x34 */
	VU32 AES_DATA_2;	/*0x38 */
	VU32 AES_DATA_3;	/*0x3C */
	VU32 AES_DATA_4;	/*0x40 */

	VU32 AES_REV;		/*0x44 */
	VU32 AES_MASK;		/*0x48 */

	VU32 AES_SYSSTATUS;	/*0x4C */

} AESReg_t;

#define FLAGS_FAST	BIT(7)
#define FLAGS_BUSY	8

typedef struct {
	unsigned long		flags;

	spinlock_t		lock;
	struct crypto_queue	queue;

	struct tasklet_struct	task;

	struct ablkcipher_request	*req;
	size_t				total;
	struct scatterlist		*in_sg;
	size_t				in_offset;
	struct scatterlist		*out_sg;
	size_t				out_offset;

	size_t			buflen;
	void			*buf_in;
	size_t			dma_size;
	int			dma_in;
	int			dma_lch_in;
	dma_addr_t		dma_addr_in;
	void			*buf_out;
	int			dma_out;
	int			dma_lch_out;
	dma_addr_t		dma_addr_out;

	PUBLIC_CRYPTO_AES_OPERATION_STATE *ctx;
} AES_HWA_CTX;
static AES_HWA_CTX *aes_ctx;

/*---------------------------------------------------------------------------
 *Forward declarations
 *------------------------------------------------------------------------- */

static void PDrvCryptoUpdateAESWithDMA(u8 *pSrc, u8 *pDest,
					u32 nbBlocks, u32 dmaUse);

/*----------------------------------------------------------------------------
 *Save HWA registers into the specified operation state structure
 *--------------------------------------------------------------------------*/
static void PDrvCryptoSaveAESRegisters(
	PUBLIC_CRYPTO_AES_OPERATION_STATE *pAESState)
{
	AESReg_t *pAESReg_t = (AESReg_t *)OMAP2_L4_IO_ADDRESS(AES1_REGS_HW_ADDR);

	dprintk(KERN_INFO "PDrvCryptoSaveAESRegisters: \
		pAESState(%p) <- pAESReg_t(%p): CTRL=0x%08x\n",
		pAESState, pAESReg_t, pAESState->CTRL);

	/*Save the IV if we are in CBC or CTR mode (not required for ECB) */
	if (!AES_CTRL_IS_MODE_ECB(pAESState->CTRL)) {
		pAESState->AES_IV_1 = INREG32(&pAESReg_t->AES_IV_1);
		pAESState->AES_IV_2 = INREG32(&pAESReg_t->AES_IV_2);
		pAESState->AES_IV_3 = INREG32(&pAESReg_t->AES_IV_3);
		pAESState->AES_IV_4 = INREG32(&pAESReg_t->AES_IV_4);
	}
}

/*----------------------------------------------------------------------------
 *Restore the HWA registers from the operation state structure
 *---------------------------------------------------------------------------*/
static void PDrvCryptoRestoreAESRegisters(
	PUBLIC_CRYPTO_AES_OPERATION_STATE *pAESState)
{
	AESReg_t *pAESReg_t = (AESReg_t *)OMAP2_L4_IO_ADDRESS(AES1_REGS_HW_ADDR);

	dprintk(KERN_INFO "PDrvCryptoRestoreAESRegisters: \
		pAESReg_t(%p) <- pAESState(%p): CTRL=0x%08x\n",
		pAESReg_t, pAESState, pAESState->CTRL);

	if (pAESState->key_is_public) {
		OUTREG32(&pAESReg_t->AES_MASK, AES_MASK_SOFTRESET_BIT);

		OUTREG32(&pAESReg_t->AES_KEY1_H, pAESState->KEY1_H);
		OUTREG32(&pAESReg_t->AES_KEY1_L, pAESState->KEY1_L);
		OUTREG32(&pAESReg_t->AES_KEY2_H, pAESState->KEY2_H);
		OUTREG32(&pAESReg_t->AES_KEY2_L, pAESState->KEY2_L);
		OUTREG32(&pAESReg_t->AES_KEY3_H, pAESState->KEY3_H);
		OUTREG32(&pAESReg_t->AES_KEY3_L, pAESState->KEY3_L);
		OUTREG32(&pAESReg_t->AES_KEY4_H, pAESState->KEY4_H);
		OUTREG32(&pAESReg_t->AES_KEY4_L, pAESState->KEY4_L);

		/*
		 * Make sure a potential secure key that has been overwritten by
		 * the previous code is reinstalled before performing other
		 * public crypto operations.
		 */
		g_SCXLNXDeviceMonitor.hAES1SecureKeyContext = 0;
	} else {
		pAESState->CTRL |= INREG32(&pAESReg_t->AES_CTRL);
	}

	/*
	 * Restore the IV first if we are in CBC or CTR mode
	 * (not required for ECB)
	 */
	if (!AES_CTRL_IS_MODE_ECB(pAESState->CTRL)) {
		OUTREG32(&pAESReg_t->AES_IV_1, pAESState->AES_IV_1);
		OUTREG32(&pAESReg_t->AES_IV_2, pAESState->AES_IV_2);
		OUTREG32(&pAESReg_t->AES_IV_3, pAESState->AES_IV_3);
		OUTREG32(&pAESReg_t->AES_IV_4, pAESState->AES_IV_4);
	}

	/*
	 * Then set the CTRL register:
	 * overwrite the CTRL only when needed, because unconditionally
	 * doing it leads to break the HWA process
	 * (observed by experimentation)
	 */
	pAESState->CTRL = (pAESState->CTRL & (3 << 3)) /* key size */
		| (pAESState->CTRL & ((1 << 2) | (1 << 5) | (1 << 6)))
		/* CTR, CBC, DIRECTION */
		| (0x3 << 7)
		/* Always set CTR_WIDTH to 128-bit */;

	if ((pAESState->CTRL & 0x1FC) != (INREG32(&pAESReg_t->AES_CTRL) & 0x1FC))
		OUTREG32(&pAESReg_t->AES_CTRL, pAESState->CTRL & 0x1FC);

	/*Set the MASK register to 0 */
	OUTREG32(&pAESReg_t->AES_MASK, 0);
}

/*-------------------------------------------------------------------------- */

void PDrvCryptoUpdateAES(PUBLIC_CRYPTO_AES_OPERATION_STATE *pAESState,
			u8 *pSrc, u8 *pDest, u32 nbBlocks)
{
	AESReg_t *pAESReg_t = (AESReg_t *)OMAP2_L4_IO_ADDRESS(AES1_REGS_HW_ADDR);

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
	PDrvCryptoRestoreAESRegisters(pAESState);

	if ((dmaUse == PUBLIC_CRYPTO_DMA_USE_POLLING)
		 || (dmaUse == PUBLIC_CRYPTO_DMA_USE_IRQ)) {

		/*perform the update with DMA */
		PDrvCryptoUpdateAESWithDMA(pProcessSrc,
				pProcessDest, nbBlocks, dmaUse);

	} else {
		for (nbr_of_blocks = 0;
			nbr_of_blocks < nbBlocks; nbr_of_blocks++) {

			/*We wait for the input ready */

			/*Crash the system as this should never occur */
			if (scxPublicCryptoWaitForReadyBit(
				(VU32 *)&pAESReg_t->AES_CTRL,
				AES_CTRL_INPUT_READY_BIT) !=
					PUBLIC_CRYPTO_OPERATION_SUCCESS)
					panic("Wait too long for AES hardware \
					accelerator Input data to be ready\n");

			/*We copy the 16 bytes of data src->reg */
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_1, vTemp);
			pProcessSrc += 4;
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_2, vTemp);
			pProcessSrc += 4;
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_3, vTemp);
			pProcessSrc += 4;
			vTemp = (u32) BYTES_TO_LONG(pProcessSrc);
			OUTREG32(&pAESReg_t->AES_DATA_4, vTemp);
			pProcessSrc += 4;

			/*We wait for the output ready */
			scxPublicCryptoWaitForReadyBitInfinitely(
					(VU32 *)&pAESReg_t->AES_CTRL,
					AES_CTRL_OUTPUT_READY_BIT);

			/*We copy the 16 bytes of data reg->dest */
			vTemp = INREG32(&pAESReg_t->AES_DATA_1);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
			vTemp = INREG32(&pAESReg_t->AES_DATA_2);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
			vTemp = INREG32(&pAESReg_t->AES_DATA_3);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
			vTemp = INREG32(&pAESReg_t->AES_DATA_4);
			LONG_TO_BYTE(pProcessDest, vTemp);
			pProcessDest += 4;
		}
	}

	/*Save the accelerator registers into the operation state */
	PDrvCryptoSaveAESRegisters(pAESState);

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
static void PDrvCryptoUpdateAESWithDMA(u8 *pSrc, u8 *pDest,
					u32 nbBlocks, u32 dmaUse)
{
	AESReg_t *pAESReg_t = (AESReg_t *)OMAP2_L4_IO_ADDRESS(AES1_REGS_HW_ADDR);

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

	dprintk(KERN_INFO
		"PDrvCryptoUpdateAESWithDMA: In=0x%08x, Out=0x%08x, Len=%u\n",
		(unsigned int)pSrc,
		(unsigned int)pDest,
		(unsigned int)nLength);

	/*lock the DMA */
	down(&g_SCXLNXDeviceMonitor.sm.sDMALock);

	if (scxPublicDMARequest(&dma_ch0) != PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		up(&g_SCXLNXDeviceMonitor.sm.sDMALock);
		return;
	}
	if (scxPublicDMARequest(&dma_ch1) != PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		scxPublicDMARelease(dma_ch0);
		up(&g_SCXLNXDeviceMonitor.sm.sDMALock);
		return;
	}

	while (nLength > 0) {

		/*
		 * At this time, we are sure that the DMAchannels
		 *are available and not used by other public crypto operation
		 */

		/*DMA used for Input and Output */
		OUTREG32(&pAESReg_t->AES_MASK, INREG32(&pAESReg_t->AES_MASK)
			| AES_MASK_DMA_REQ_OUT_EN_BIT
			| AES_MASK_DMA_REQ_IN_EN_BIT);

		/*check length */
		if (nLength <= g_SCXLNXDeviceMonitor.nDMABufferLength)
			nLengthLoop = nLength;
		else
			nLengthLoop = g_SCXLNXDeviceMonitor.nDMABufferLength;

		/*The length is always a multiple of the block size */
		nbBlocksLoop = nLengthLoop / AES_BLOCK_SIZE;

		/*
		 *Copy the data from the input buffer into a preallocated
		 *buffer which is aligned on the beginning of a page.
		 *This may prevent potential issues when flushing/invalidating
		 *the buffer as the cache lines are 64 bytes long.
		 */
		memcpy(g_SCXLNXDeviceMonitor.pDMABuffer, pSrc, nLengthLoop);

		/*DMA1: Mem -> AES */
		scxPublicSetDMAChannelCommonParams(&ch0_parameters,
			nbBlocksLoop,
			DMA_CEN_Elts_per_Frame_AES,
			AES1_REGS_HW_ADDR + 0x34,
			g_SCXLNXDeviceMonitor.pDMABufferPhys,
			DMA_CCR_Channel_Mem2AES1);

		/*specific for Mem -> HWA */
		ch0_parameters.src_amode = OMAP_DMA_AMODE_POST_INC;
		ch0_parameters.dst_amode = OMAP_DMA_AMODE_CONSTANT;
		ch0_parameters.src_or_dst_synch = OMAP_DMA_DST_SYNC;

		dprintk(KERN_INFO "PDrvCryptoUpdateAESWithDMA: "
				"scxPublicDMASetParams(ch0)\n");
		scxPublicDMASetParams(dma_ch0, &ch0_parameters);

		omap_set_dma_src_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_16);
		omap_set_dma_dest_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_16);

		/*DMA2: AES -> Mem */
		scxPublicSetDMAChannelCommonParams(&ch1_parameters,
			nbBlocksLoop,
			DMA_CEN_Elts_per_Frame_AES,
			g_SCXLNXDeviceMonitor.pDMABufferPhys,
			AES1_REGS_HW_ADDR + 0x34,
			DMA_CCR_Channel_AES12Mem);

		/*specific for HWA -> mem */
		ch1_parameters.src_amode = OMAP_DMA_AMODE_CONSTANT;
		ch1_parameters.dst_amode = OMAP_DMA_AMODE_POST_INC;
		ch1_parameters.src_or_dst_synch = OMAP_DMA_SRC_SYNC;

		omap_set_dma_src_burst_mode(dma_ch1, OMAP_DMA_DATA_BURST_16);
		omap_set_dma_dest_burst_mode(dma_ch1, OMAP_DMA_DATA_BURST_16);

		wmb();

		dprintk(KERN_INFO "PDrvCryptoUpdateAESWithDMA: "
			"scxPublicDMASetParams(ch1)\n");
		scxPublicDMASetParams(dma_ch1, &ch1_parameters);

		dprintk(KERN_INFO
			"PDrvCryptoUpdateAESWithDMA: Start DMA channel %d\n",
			(unsigned int)dma_ch0);
		scxPublicDMAStart(dma_ch0, OMAP_DMA_BLOCK_IRQ);

		dprintk(KERN_INFO "PDrvCryptoUpdateAESWithDMA: \
			Start DMA channel %d\n",
			(unsigned int)dma_ch1);
		scxPublicDMAStart(dma_ch1, OMAP_DMA_BLOCK_IRQ);

		/*Start operation */
		OUTREG32(&pAESReg_t->AES_MASK,
			INREG32(&pAESReg_t->AES_MASK) | AES_MASK_START_BIT);

		/*Suspends the process until the DMA IRQ occurs */
		dprintk(KERN_INFO
			"PDrvCryptoUpdateAESWithDMA: Waiting for IRQ\n");
		scxPublicDMAWait(2);

		/*
		 *The dma transfer is complete
		 */

		/*Stop clocks */
		OUTREG32(&pAESReg_t->AES_MASK, INREG32(&pAESReg_t->AES_MASK)
			& (~AES_MASK_START_BIT));

		/*Unset DMA synchronisation requests */
		OUTREG32(&pAESReg_t->AES_MASK, INREG32(&pAESReg_t->AES_MASK)
			& (~AES_MASK_DMA_REQ_OUT_EN_BIT)
			& (~AES_MASK_DMA_REQ_IN_EN_BIT));

		scxPublicDMAClearChannel(dma_ch0);
		scxPublicDMAClearChannel(dma_ch1);

		/*The DMA output is in the preallocated aligned buffer
		 *and needs to be copied to the output buffer.*/
		memcpy(pDest, g_SCXLNXDeviceMonitor.pDMABuffer, nLengthLoop);

		pSrc += nLengthLoop;
		pDest += nLengthLoop;
		nLength -= nLengthLoop;
	}

	/*For safety reasons, let's clean the working buffer */
	memset(g_SCXLNXDeviceMonitor.pDMABuffer, 0, nLengthLoop);

	/* Release the DMA */
	scxPublicDMARelease(dma_ch0);
	scxPublicDMARelease(dma_ch1);

	up(&g_SCXLNXDeviceMonitor.sm.sDMALock);

	dprintk(KERN_INFO "PDrvCryptoUpdateAESWithDMA: Success\n");
}

/*
 * AES HWA registration into kernel crypto framework
 */

static void sg_copy_buf(void *buf, struct scatterlist *sg,
	unsigned int start, unsigned int nbytes, int out)
{
	struct scatter_walk walk;

	if (!nbytes)
		return;

	scatterwalk_start(&walk, sg);
	scatterwalk_advance(&walk, start);
	scatterwalk_copychunks(buf, &walk, nbytes, out);
	scatterwalk_done(&walk, out, 0);
}

static int sg_copy(struct scatterlist **sg, size_t *offset, void *buf,
	size_t buflen, size_t total, int out)
{
	unsigned int count, off = 0;

	while (buflen && total) {
		count = min((*sg)->length - *offset, total);
		count = min(count, buflen);

		if (!count)
			return off;

		sg_copy_buf(buf + off, *sg, *offset, count, out);

		off += count;
		buflen -= count;
		*offset += count;
		total -= count;

		if (*offset == (*sg)->length) {
			*sg = sg_next(*sg);
			if (*sg)
				*offset = 0;
			else
				total = 0;
		}
	}

	return off;
}

static int aes_dma_start(AES_HWA_CTX *ctx)
{
	AESReg_t *pAESReg_t = (AESReg_t *)OMAP2_L4_IO_ADDRESS(AES1_REGS_HW_ADDR);
	int err, fast = 0, in, out;
	size_t count;
	dma_addr_t addr_in, addr_out;
	struct omap_dma_channel_params dma_params;
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(ctx->req));

	if (sg_is_last(ctx->in_sg) && sg_is_last(ctx->out_sg)) {
		in = IS_ALIGNED((u32)ctx->in_sg->offset, sizeof(u32));
		out = IS_ALIGNED((u32)ctx->out_sg->offset, sizeof(u32));

		fast = in && out;
	}

	if (fast) {
		count = min(ctx->total, sg_dma_len(ctx->in_sg));
		count = min(count, sg_dma_len(ctx->out_sg));

		if (count != ctx->total)
			return -EINVAL;

		err = dma_map_sg(NULL, ctx->in_sg, 1, DMA_TO_DEVICE);
		if (!err)
			return -EINVAL;

		err = dma_map_sg(NULL, ctx->out_sg, 1, DMA_FROM_DEVICE);
		if (!err) {
			dma_unmap_sg(NULL, ctx->in_sg, 1, DMA_TO_DEVICE);
			return -EINVAL;
		}

		addr_in = sg_dma_address(ctx->in_sg);
		addr_out = sg_dma_address(ctx->out_sg);

		ctx->flags |= FLAGS_FAST;
	} else {
		count = sg_copy(&ctx->in_sg, &ctx->in_offset, ctx->buf_in,
			ctx->buflen, ctx->total, 0);

		addr_in = ctx->dma_addr_in;
		addr_out = ctx->dma_addr_out;

		ctx->flags &= ~FLAGS_FAST;
	}

	ctx->total -= count;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&g_smc_wake_lock);
#endif
	down(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);

	/* Configure HWA */
	scxPublicCryptoEnableClock(PUBLIC_CRYPTO_AES1_CLOCK_BIT);

	PDrvCryptoRestoreAESRegisters(state);

	OUTREG32(&pAESReg_t->AES_MASK, INREG32(&pAESReg_t->AES_MASK)
		| AES_MASK_DMA_REQ_OUT_EN_BIT
		| AES_MASK_DMA_REQ_IN_EN_BIT);

	ctx->dma_size = count;
	if (!fast)
		dma_sync_single_for_device(NULL, addr_in, count,
			DMA_TO_DEVICE);

	dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;
	dma_params.frame_count = count / AES_BLOCK_SIZE;
	dma_params.elem_count = DMA_CEN_Elts_per_Frame_AES;
	dma_params.src_ei = 0;
	dma_params.src_fi = 0;
	dma_params.dst_ei = 0;
	dma_params.dst_fi = 0;
	dma_params.sync_mode = OMAP_DMA_SYNC_FRAME;

	/* IN */
	dma_params.trigger = ctx->dma_in;
	dma_params.src_or_dst_synch = OMAP_DMA_DST_SYNC;
	dma_params.dst_start = AES1_REGS_HW_ADDR + 0x34;
	dma_params.dst_amode = OMAP_DMA_AMODE_CONSTANT;
	dma_params.src_start = addr_in;
	dma_params.src_amode = OMAP_DMA_AMODE_POST_INC;

	omap_set_dma_params(ctx->dma_lch_in, &dma_params);

	omap_set_dma_dest_burst_mode(ctx->dma_lch_in, OMAP_DMA_DATA_BURST_16);
	omap_set_dma_src_burst_mode(ctx->dma_lch_in, OMAP_DMA_DATA_BURST_16);

	/* OUT */
	dma_params.trigger = ctx->dma_out;
	dma_params.src_or_dst_synch = OMAP_DMA_SRC_SYNC;
	dma_params.src_start = AES1_REGS_HW_ADDR + 0x34;
	dma_params.src_amode = OMAP_DMA_AMODE_CONSTANT;
	dma_params.dst_start = addr_out;
	dma_params.dst_amode = OMAP_DMA_AMODE_POST_INC;

	omap_set_dma_params(ctx->dma_lch_out, &dma_params);

	omap_set_dma_dest_burst_mode(ctx->dma_lch_out, OMAP_DMA_DATA_BURST_16);
	omap_set_dma_src_burst_mode(ctx->dma_lch_out, OMAP_DMA_DATA_BURST_16);

	/* Is this really needed? */
	omap_disable_dma_irq(ctx->dma_lch_in, OMAP_DMA_DROP_IRQ);
	omap_enable_dma_irq(ctx->dma_lch_in, OMAP_DMA_BLOCK_IRQ);
	omap_disable_dma_irq(ctx->dma_lch_out, OMAP_DMA_DROP_IRQ);
	omap_enable_dma_irq(ctx->dma_lch_out, OMAP_DMA_BLOCK_IRQ);

	wmb();

	omap_start_dma(ctx->dma_lch_in);
	omap_start_dma(ctx->dma_lch_out);

	OUTREG32(&pAESReg_t->AES_MASK,
		INREG32(&pAESReg_t->AES_MASK) | AES_MASK_START_BIT);

	return 0;
}

static int aes_dma_stop(AES_HWA_CTX *ctx)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(ctx->req));
	AESReg_t *pAESReg_t = (AESReg_t *)OMAP2_L4_IO_ADDRESS(AES1_REGS_HW_ADDR);
	int err = 0;
	size_t count;

	dprintk(KERN_INFO "aes_dma_stop(%p)\n", ctx);

	PDrvCryptoSaveAESRegisters(state);

	if (!AES_CTRL_IS_MODE_ECB(state->CTRL)) {
		u32 *ptr = (u32 *) ctx->req->info;

		ptr[0] = state->AES_IV_1;
		ptr[1] = state->AES_IV_2;
		ptr[2] = state->AES_IV_3;
		ptr[3] = state->AES_IV_4;
	}

	OUTREG32(&pAESReg_t->AES_MASK, 0);

	scxPublicCryptoDisableClock(PUBLIC_CRYPTO_AES1_CLOCK_BIT);

#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&g_smc_wake_lock);
#endif
	up(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);

	omap_stop_dma(ctx->dma_lch_in);
	omap_stop_dma(ctx->dma_lch_out);

	if (ctx->flags & FLAGS_FAST) {
		dma_unmap_sg(NULL, ctx->out_sg, 1, DMA_FROM_DEVICE);
		dma_unmap_sg(NULL, ctx->in_sg, 1, DMA_TO_DEVICE);
	} else {
		dma_sync_single_for_device(NULL, ctx->dma_addr_out,
			ctx->dma_size, DMA_FROM_DEVICE);

		/* Copy data */
		count = sg_copy(&ctx->out_sg, &ctx->out_offset, ctx->buf_out,
			ctx->buflen, ctx->dma_size, 1);
		if (count != ctx->dma_size)
			err = -EINVAL;
	}

	if (err || !ctx->total)
		ctx->req->base.complete(&ctx->req->base, err);

	return err;
}

static void aes_dma_callback(int lch, u16 ch_status, void *data)
{
	AES_HWA_CTX *ctx = data;

	if (lch == ctx->dma_lch_out)
		tasklet_schedule(&ctx->task);
}

static int aes_dma_init(AES_HWA_CTX *ctx)
{
	int err = -ENOMEM;

	ctx->dma_lch_out = -1;
	ctx->dma_lch_in = -1;

	ctx->buflen = PAGE_SIZE;
	ctx->buflen &= ~(AES_BLOCK_SIZE - 1);

	dprintk(KERN_INFO "aes_dma_init(%p)\n", ctx);

	/* Allocate and map cache buffers */
	ctx->buf_in = dma_alloc_coherent(NULL, ctx->buflen, &ctx->dma_addr_in,
		GFP_KERNEL);
	if (!ctx->buf_in) {
		dprintk(KERN_ERR "SMC: Unable to alloc AES in cache buffer\n");
		return -ENOMEM;
	}

	ctx->buf_out = dma_alloc_coherent(NULL, ctx->buflen, &ctx->dma_addr_out,
		GFP_KERNEL);
	if (!ctx->buf_out) {
		dprintk(KERN_ERR "SMC: Unable to alloc AES out cache buffer\n");
		dma_free_coherent(NULL, ctx->buflen, ctx->buf_in,
			ctx->dma_addr_in);
		return -ENOMEM;
	}

	/* Request DMA channels */
	err = omap_request_dma(0, "smc-aes-rx", aes_dma_callback, ctx,
		&ctx->dma_lch_in);
	if (err) {
		dprintk(KERN_ERR "SMC: Unable to request AES RX DMA channel\n");
		goto err_dma_in;
	}

	err = omap_request_dma(0, "smc-aes-rx", aes_dma_callback,
		ctx, &ctx->dma_lch_out);
	if (err) {
		dprintk(KERN_ERR "SMC: Unable to request AES TX DMA channel\n");
		goto err_dma_out;
	}

	dprintk(KERN_INFO "aes_dma_init(%p) configured DMA channels"
		"(RX = %d, TX = %d)\n", ctx, ctx->dma_lch_in, ctx->dma_lch_out);

	return 0;

err_dma_out:
	omap_free_dma(ctx->dma_lch_in);
err_dma_in:
	dma_free_coherent(NULL, ctx->buflen, ctx->buf_in, ctx->dma_addr_in);
	dma_free_coherent(NULL, ctx->buflen, ctx->buf_out, ctx->dma_addr_out);

	return err;
}

static void aes_dma_cleanup(AES_HWA_CTX *ctx)
{
	omap_free_dma(ctx->dma_lch_out);
	omap_free_dma(ctx->dma_lch_in);
	dma_free_coherent(NULL, ctx->buflen, ctx->buf_in, ctx->dma_addr_in);
	dma_free_coherent(NULL, ctx->buflen, ctx->buf_out, ctx->dma_addr_out);
}

static int aes_handle_req(AES_HWA_CTX *ctx)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state;
	struct crypto_async_request *async_req, *backlog;
	struct ablkcipher_request *req;
	unsigned long flags;

	if (ctx->total)
		goto start;

	spin_lock_irqsave(&ctx->lock, flags);
	backlog = crypto_get_backlog(&ctx->queue);
	async_req = crypto_dequeue_request(&ctx->queue);
	if (!async_req)
		clear_bit(FLAGS_BUSY, &ctx->flags);
	spin_unlock_irqrestore(&ctx->lock, flags);

	if (!async_req)
		return 0;

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ablkcipher_request_cast(async_req);

	ctx->req = req;
	ctx->total = req->nbytes;
	ctx->in_offset = 0;
	ctx->in_sg = req->src;
	ctx->out_offset = 0;
	ctx->out_sg = req->dst;

	state = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));

	if (!AES_CTRL_IS_MODE_ECB(state->CTRL)) {
		u32 *ptr = (u32 *) req->info;

		state->AES_IV_1 = ptr[0];
		state->AES_IV_2 = ptr[1];
		state->AES_IV_3 = ptr[2];
		state->AES_IV_4 = ptr[3];
	}

start:
	return aes_dma_start(ctx);
}

static void aes_tasklet(unsigned long data)
{
	AES_HWA_CTX *ctx = (AES_HWA_CTX *) data;

	aes_dma_stop(ctx);
	aes_handle_req(ctx);
}

/* Generic */
static int aes_setkey(PUBLIC_CRYPTO_AES_OPERATION_STATE *state,
	const u8 *key, unsigned int keylen)
{
	u32 *ptr = (u32 *)key;

	switch (keylen) {
	case 16:
		state->CTRL |= AES_CTRL_KEY_SIZE_128;
		break;
	case 24:
		state->CTRL |= AES_CTRL_KEY_SIZE_192;
		break;
	case 32:
		state->CTRL |= AES_CTRL_KEY_SIZE_256;
		break;
	default:
		return -EINVAL;
	}

	state->KEY1_L = ptr[0];
	state->KEY1_H = ptr[1];
	state->KEY2_L = ptr[2];
	state->KEY2_H = ptr[3];
	if (keylen >= 24) {
		state->KEY3_L = ptr[4];
		state->KEY3_H = ptr[5];
	}
	if (keylen == 32) {
		state->KEY4_L = ptr[6];
		state->KEY4_H = ptr[7];
	}

	state->key_is_public = 1;

	return 0;
}

static int aes_operate(struct ablkcipher_request *req)
{
	unsigned long flags;
	int err;

	spin_lock_irqsave(&aes_ctx->lock, flags);
	err = ablkcipher_enqueue_request(&aes_ctx->queue, req);
	spin_unlock_irqrestore(&aes_ctx->lock, flags);

	if (!test_and_set_bit(FLAGS_BUSY, &aes_ctx->flags))
		aes_handle_req(aes_ctx);

	return err;
}

static int aes_encrypt(struct ablkcipher_request *req)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));

	state->CTRL |= AES_CTRL_DIRECTION_ENCRYPT;

	return aes_operate(req);
}

static int aes_decrypt(struct ablkcipher_request *req)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));

	state->CTRL &= ~(AES_CTRL_DIRECTION_ENCRYPT);
	state->CTRL |= AES_CTRL_DIRECTION_DECRYPT;

	return aes_operate(req);
}

static int aes_single_setkey(struct crypto_tfm *tfm, const u8 *key,
	unsigned int keylen)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state = crypto_tfm_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_ECB_BIT;

	return aes_setkey(state, key, keylen);
}

static void aes_single_encrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state = crypto_tfm_ctx(tfm);

	state->CTRL |= AES_CTRL_DIRECTION_ENCRYPT;

	down(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&g_smc_wake_lock);
#endif

	scxPublicCryptoEnableClock(PUBLIC_CRYPTO_AES1_CLOCK_BIT);
	PDrvCryptoUpdateAES(state, (u8 *) in, out, 1);
	scxPublicCryptoDisableClock(PUBLIC_CRYPTO_AES1_CLOCK_BIT);

#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&g_smc_wake_lock);
#endif
	up(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);
}

static void aes_single_decrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state = crypto_tfm_ctx(tfm);

	state->CTRL &= ~(AES_CTRL_DIRECTION_ENCRYPT);
	state->CTRL |= AES_CTRL_DIRECTION_DECRYPT;

	down(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&g_smc_wake_lock);
#endif

	scxPublicCryptoEnableClock(PUBLIC_CRYPTO_AES1_CLOCK_BIT);
	PDrvCryptoUpdateAES(state, (u8 *) in, out, 1);
	scxPublicCryptoDisableClock(PUBLIC_CRYPTO_AES1_CLOCK_BIT);

#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&g_smc_wake_lock);
#endif
	up(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);
}

/* AES ECB */
static int aes_ecb_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
	unsigned int keylen)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state = crypto_ablkcipher_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_ECB_BIT;

	return aes_setkey(state, key, keylen);
}

/* AES CBC */
static int aes_cbc_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
	unsigned int keylen)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state = crypto_ablkcipher_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_CBC_BIT;

	return aes_setkey(state, key, keylen);
}

/* AES CTR */
static int aes_ctr_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
	unsigned int keylen)
{
	PUBLIC_CRYPTO_AES_OPERATION_STATE *state = crypto_ablkcipher_ctx(tfm);

	/* Always defaults to 128-bit counter */
	state->CTRL = AES_CTRL_MODE_CTR_BIT | AES_CTRL_CTR_WIDTH_128;

	return aes_setkey(state, key, keylen);
}

static struct crypto_alg smc_aes_alg = {
	.cra_flags		= CRYPTO_ALG_TYPE_CIPHER,
	.cra_priority		= 999,
	.cra_name		= "aes",
	.cra_driver_name	= "aes-smc",
	.cra_module		= THIS_MODULE,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(PUBLIC_CRYPTO_AES_OPERATION_STATE),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_alg.cra_list),
	.cra_u			= {
		.cipher = {
			.cia_min_keysize	= AES_MIN_KEY_SIZE,
			.cia_max_keysize	= AES_MAX_KEY_SIZE,
			.cia_setkey		= aes_single_setkey,
			.cia_encrypt		= aes_single_encrypt,
			.cia_decrypt		= aes_single_decrypt,
		}
	},
};

static struct crypto_alg smc_aes_ecb_alg = {
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_priority		= 999,
	.cra_name		= "ecb(aes)",
	.cra_driver_name	= "aes-ecb-smc",
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(PUBLIC_CRYPTO_AES_OPERATION_STATE),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_ecb_alg.cra_list),
	.cra_u			= {
		.ablkcipher = {
			.min_keysize	= AES_MIN_KEY_SIZE,
			.max_keysize	= AES_MAX_KEY_SIZE,
			.setkey		= aes_ecb_setkey,
			.encrypt	= aes_encrypt,
			.decrypt	= aes_decrypt,
		}
	},
};

static struct crypto_alg smc_aes_cbc_alg = {
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_priority		= 999,
	.cra_name		= "cbc(aes)",
	.cra_driver_name	= "aes-cbc-smc",
	.cra_module		= THIS_MODULE,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(PUBLIC_CRYPTO_AES_OPERATION_STATE),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_cbc_alg.cra_list),
	.cra_u			= {
		.ablkcipher = {
			.min_keysize	= AES_MIN_KEY_SIZE,
			.max_keysize	= AES_MAX_KEY_SIZE,
			.ivsize		= PUBLIC_CRYPTO_IV_MAX_SIZE,
			.setkey		= aes_cbc_setkey,
			.encrypt	= aes_encrypt,
			.decrypt	= aes_decrypt,
		}
	},
};

static struct crypto_alg smc_aes_ctr_alg = {
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_priority		= 999,
	.cra_name		= "ctr(aes)",
	.cra_driver_name	= "aes-ctr-smc",
	.cra_module		= THIS_MODULE,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(PUBLIC_CRYPTO_AES_OPERATION_STATE),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_ctr_alg.cra_list),
	.cra_u			= {
		.ablkcipher = {
			.min_keysize	= AES_MIN_KEY_SIZE,
			.max_keysize	= AES_MAX_KEY_SIZE,
			.ivsize		= PUBLIC_CRYPTO_IV_MAX_SIZE,
			.setkey		= aes_ctr_setkey,
			.encrypt	= aes_encrypt,
			.decrypt	= aes_decrypt,
		}
	},
};

void PDrvCryptoAESRegister(void)
{
	int err;

	aes_ctx = kzalloc(sizeof(AES_HWA_CTX), GFP_KERNEL);
	if (aes_ctx == NULL)
		return;

	crypto_init_queue(&aes_ctx->queue, 1);
	tasklet_init(&aes_ctx->task, aes_tasklet, (unsigned long)aes_ctx);

	aes_ctx->dma_in = DMA_CCR_Channel_Mem2AES1;
	aes_ctx->dma_out = DMA_CCR_Channel_AES12Mem;

	err = aes_dma_init(aes_ctx);
	if (err)
		goto err_dma;

	crypto_register_alg(&smc_aes_alg);
	crypto_register_alg(&smc_aes_ecb_alg);
	crypto_register_alg(&smc_aes_cbc_alg);
	crypto_register_alg(&smc_aes_ctr_alg);

	return;

err_dma:
	tasklet_kill(&aes_ctx->task);
	kfree(aes_ctx);
	aes_ctx = NULL;
}

void PDrvCryptoAESUnregister(void)
{
	if (aes_ctx == NULL)
		return;

	crypto_unregister_alg(&smc_aes_alg);
	crypto_unregister_alg(&smc_aes_ecb_alg);
	crypto_unregister_alg(&smc_aes_cbc_alg);
	crypto_unregister_alg(&smc_aes_ctr_alg);

	tasklet_kill(&aes_ctx->task);

	aes_dma_cleanup(aes_ctx);

	kfree(aes_ctx);
}
