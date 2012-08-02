/**
 * Copyright (c) 2011 Trusted Logic S.A.
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

#include "tf_defs.h"
#include "tf_util.h"
#include "tf_crypto.h"
#include "tf_dma.h"
#include "tf_zebra.h"

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/mempool.h>
#include <crypto/algapi.h>
#include <crypto/scatterwalk.h>
#include <crypto/aes.h>
#include <mach/io.h>

/*
 *AES Hardware Accelerator: Base address
 */
#define AES1_REGS_HW_ADDR		0x4B501000

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
struct aes_reg {
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
static struct aes_reg *paes_reg;

#ifdef CONFIG_SMC_KERNEL_CRYPTO
#define FLAGS_MODE_MASK		0x000f
#define FLAGS_ENCRYPT		BIT(0)
#define FLAGS_DECRYPT		0

#define FLAGS_FAST	BIT(7)
#define FLAGS_BUSY	8

struct aes_hwa_ctx {
	unsigned long		flags;

	spinlock_t		lock;
	struct crypto_queue	queue;

	struct tasklet_struct	task;

	struct ablkcipher_request	*req;
	struct crypto_async_request     *next_req;
	struct crypto_async_request     *backlog;

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

	struct tf_crypto_aes_operation_state *ctx;
};
static struct aes_hwa_ctx *aes_ctx;

struct aes_reqctx {
	u32 mode;
};
#endif

/*---------------------------------------------------------------------------
 *Forward declarations
 *------------------------------------------------------------------------- */

static bool tf_aes_update_dma(u8 *src, u8 *dest, u32 nb_blocks,
			      u32 ctrl, bool is_kernel);

static bool reconfigure_dma;

/*----------------------------------------------------------------------------
 *Save HWA registers into the specified operation state structure
 *--------------------------------------------------------------------------*/
static void tf_aes_save_registers(
	struct tf_crypto_aes_operation_state *aes_state)
{
	dprintk(KERN_INFO "tf_aes_save_registers: "
		"aes_state(%p) <- paes_reg(%p): CTRL=0x%08x\n",
		aes_state, paes_reg, aes_state->CTRL);

	/*Save the IV if we are in CBC or CTR mode (not required for ECB) */
	if (!AES_CTRL_IS_MODE_ECB(aes_state->CTRL)) {
		aes_state->AES_IV_0 = INREG32(&paes_reg->AES_IV_IN_0);
		aes_state->AES_IV_1 = INREG32(&paes_reg->AES_IV_IN_1);
		aes_state->AES_IV_2 = INREG32(&paes_reg->AES_IV_IN_2);
		aes_state->AES_IV_3 = INREG32(&paes_reg->AES_IV_IN_3);
	}
}

/*----------------------------------------------------------------------------
 *Restore the HWA registers from the operation state structure
 *---------------------------------------------------------------------------*/
static void tf_aes_restore_registers(
	struct tf_crypto_aes_operation_state *aes_state, int encrypt)
{
	struct tf_device *dev = tf_get_device();
	u32 CTRL = aes_state->CTRL;

	dprintk(KERN_INFO "tf_aes_restore_registers: "
		"paes_reg(%p) <- aes_state(%p): CTRL=0x%08x\n",
		paes_reg, aes_state, aes_state->CTRL);

	if (aes_state->key_is_public) {
		OUTREG32(&paes_reg->AES_KEY1_0, aes_state->KEY1_0);
		OUTREG32(&paes_reg->AES_KEY1_1, aes_state->KEY1_1);
		OUTREG32(&paes_reg->AES_KEY1_2, aes_state->KEY1_2);
		OUTREG32(&paes_reg->AES_KEY1_3, aes_state->KEY1_3);
		OUTREG32(&paes_reg->AES_KEY1_4, aes_state->KEY1_4);
		OUTREG32(&paes_reg->AES_KEY1_5, aes_state->KEY1_5);
		OUTREG32(&paes_reg->AES_KEY1_6, aes_state->KEY1_6);
		OUTREG32(&paes_reg->AES_KEY1_7, aes_state->KEY1_7);

		/*
		 * Make sure a potential secure key that has been overwritten by
		 * the previous code is reinstalled before performing other
		 * public crypto operations.
		 */
		dev->aes1_key_context = 0;

		if (encrypt)
			CTRL |= AES_CTRL_DIRECTION_ENCRYPT;
		else
			CTRL = CTRL & ~AES_CTRL_DIRECTION_ENCRYPT;
	} else {
		CTRL |= INREG32(&paes_reg->AES_CTRL);
	}

	/*
	 * Restore the IV first if we are in CBC or CTR mode
	 * (not required for ECB)
	 */
	if (!AES_CTRL_IS_MODE_ECB(CTRL)) {
		OUTREG32(&paes_reg->AES_IV_IN_0, aes_state->AES_IV_0);
		OUTREG32(&paes_reg->AES_IV_IN_1, aes_state->AES_IV_1);
		OUTREG32(&paes_reg->AES_IV_IN_2, aes_state->AES_IV_2);
		OUTREG32(&paes_reg->AES_IV_IN_3, aes_state->AES_IV_3);
	}

	/* Then set the CTRL register:
	 * overwrite the CTRL only when needed, because unconditionally doing
	 * it leads to break the HWA process (observed by experimentation)
	 */

	CTRL = (CTRL & (3 << 3)) /* key size */
		|  (CTRL & ((1 << 2) | (1 << 5) | (1 << 6)))
		|  (0x3 << 7) /* Always set CTR_WIDTH to 128-bit */;

	if ((CTRL & 0x1FC) != (INREG32(&paes_reg->AES_CTRL) & 0x1FC))
		OUTREG32(&paes_reg->AES_CTRL, CTRL & 0x1FC);

	/* Reset the SYSCONFIG register */
	OUTREG32(&paes_reg->AES_SYSCONFIG, 0);
}

/*-------------------------------------------------------------------------- */

void tf_aes_init(void)
{
	paes_reg = omap_ioremap(AES1_REGS_HW_ADDR, SZ_1M, MT_DEVICE);
	if (paes_reg == NULL)
		panic("Unable to remap AES1 module");
}

void tf_aes_exit(void)
{
	omap_iounmap(paes_reg);
}

#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
#define FAULTY(mask, ctrl, chaining, key_size) \
	(((mask) & TF_CRYPTO_ALG_AES_##chaining##_##key_size) && \
	 AES_CTRL_IS_MODE_##chaining((ctrl)) && \
	 AES_CTRL_GET_KEY_SIZE((ctrl)) == AES_CTRL_KEY_SIZE_##key_size)
static void tf_aes_fault_injection(u32 ctrl, u8 *buf)
{
	if (FAULTY(tf_fault_injection_mask, ctrl, ECB, 128) ||
	    FAULTY(tf_fault_injection_mask, ctrl, ECB, 192) ||
	    FAULTY(tf_fault_injection_mask, ctrl, ECB, 256) ||
	    FAULTY(tf_fault_injection_mask, ctrl, CBC, 128) ||
	    FAULTY(tf_fault_injection_mask, ctrl, CBC, 192) ||
	    FAULTY(tf_fault_injection_mask, ctrl, CBC, 256) ||
	    FAULTY(tf_fault_injection_mask, ctrl, CTR, 128) ||
	    FAULTY(tf_fault_injection_mask, ctrl, CTR, 192) ||
	    FAULTY(tf_fault_injection_mask, ctrl, CTR, 256)) {
		pr_warning("TF: injecting fault in AES!\n");
		buf[0] = 0xff;
		buf[1] ^= 0xff;
	} else {
		dprintk(KERN_INFO "%s: no fault "
			"(mask=0x%x ctrl=0x%x mode=%u key_size=%u)\n",
			__func__, tf_fault_injection_mask,
			ctrl, AES_CTRL_GET_MODE(ctrl),
			AES_CTRL_GET_KEY_SIZE(ctrl));
	}
}
#undef FAULTY
#endif

bool tf_aes_update(struct tf_crypto_aes_operation_state *aes_state,
	u8 *src, u8 *dest, u32 nb_blocks)
{
	u32 nbr_of_blocks;
	u32 temp;
	u8 *process_src;
	u8 *process_dest;
	u32 dma_use = PUBLIC_CRYPTO_DMA_USE_NONE;
	bool is_kernel = false;

	/*
	 *Choice of the processing type
	 */
	if (nb_blocks * AES_BLOCK_SIZE >= DMA_TRIGGER_IRQ_AES)
			dma_use = PUBLIC_CRYPTO_DMA_USE_IRQ;

	dprintk(KERN_INFO "tf_aes_update: "
		"src=0x%08x, dest=0x%08x, nb_blocks=0x%08x, dma_use=0x%08x\n",
		(unsigned int)src,
		(unsigned int)dest,
		(unsigned int)nb_blocks,
		(unsigned int)dma_use);

	if (aes_state->key_is_public)
		is_kernel = true;

	if (nb_blocks == 0) {
		dprintk(KERN_INFO "tf_aes_update: Nothing to process\n");
		return true;
	}

	if ((AES_CTRL_GET_DIRECTION(INREG32(&paes_reg->AES_CTRL)) !=
		AES_CTRL_GET_DIRECTION(aes_state->CTRL)) &&
		!aes_state->key_is_public) {
		dprintk(KERN_WARNING "HWA configured for another direction\n");
		return false;
	}

	/*Restore the registers of the accelerator from the operation state */
	tf_aes_restore_registers(aes_state, 0);

	if (dma_use == PUBLIC_CRYPTO_DMA_USE_IRQ) {
		/* Perform the update with DMA */
		if (!tf_aes_update_dma(src, dest, nb_blocks,
				       aes_state->CTRL, is_kernel))
			return false;
	} else {
		u8 buf[DMA_TRIGGER_IRQ_AES];

		/*
		 * Synchronous Linux crypto API buffers are mapped in kernel
		 * space
		 */

		if (is_kernel) {
			process_src = src;
			process_dest = dest;
		} else {
			if (copy_from_user(buf, src,
					nb_blocks * AES_BLOCK_SIZE))
				return false;

			process_src = process_dest = buf;
		}

		for (nbr_of_blocks = 0;
			nbr_of_blocks < nb_blocks; nbr_of_blocks++) {

			/*We wait for the input ready */

			/*Crash the system as this should never occur */
			if (tf_crypto_wait_for_ready_bit(
				(u32 *)&paes_reg->AES_CTRL,
				AES_CTRL_INPUT_READY_BIT) !=
					PUBLIC_CRYPTO_OPERATION_SUCCESS)
					panic("Wait too long for AES hardware "
					"accelerator Input data to be ready\n");

			/* We copy the 16 bytes of data src->reg */
			temp = (u32) BYTES_TO_LONG(process_src);
			OUTREG32(&paes_reg->AES_DATA_IN_0, temp);
			process_src += 4;
			temp = (u32) BYTES_TO_LONG(process_src);
			OUTREG32(&paes_reg->AES_DATA_IN_1, temp);
			process_src += 4;
			temp = (u32) BYTES_TO_LONG(process_src);
			OUTREG32(&paes_reg->AES_DATA_IN_2, temp);
			process_src += 4;
			temp = (u32) BYTES_TO_LONG(process_src);
			OUTREG32(&paes_reg->AES_DATA_IN_3, temp);
			process_src += 4;

			/* We wait for the output ready */
			tf_crypto_wait_for_ready_bit_infinitely(
					(u32 *)&paes_reg->AES_CTRL,
					AES_CTRL_OUTPUT_READY_BIT);

			/* We copy the 16 bytes of data reg->dest */
			temp = INREG32(&paes_reg->AES_DATA_IN_0);
			LONG_TO_BYTE(process_dest, temp);
			process_dest += 4;
			temp = INREG32(&paes_reg->AES_DATA_IN_1);
			LONG_TO_BYTE(process_dest, temp);
			process_dest += 4;
			temp = INREG32(&paes_reg->AES_DATA_IN_2);
			LONG_TO_BYTE(process_dest, temp);
			process_dest += 4;
			temp = INREG32(&paes_reg->AES_DATA_IN_3);
			LONG_TO_BYTE(process_dest, temp);
			process_dest += 4;
		}

#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
		tf_aes_fault_injection(paes_reg->AES_CTRL, buf);
#endif
		if (!is_kernel)
			if (copy_to_user(dest, buf,
					nb_blocks * AES_BLOCK_SIZE))
				return false;
	}

	/* Save the accelerator registers into the operation state */
	tf_aes_save_registers(aes_state);

	dprintk(KERN_INFO "tf_aes_update: Done\n");

	return true;
}

/*-------------------------------------------------------------------------- */
/*
 *Static function, perform AES encryption/decryption using the DMA for data
 *transfer.
 *
 *inputs: src : pointer of the input data to process
 *        nb_blocks : number of block to process
 *        dma_use : PUBLIC_CRYPTO_DMA_USE_IRQ (use irq to monitor end of DMA)
 *                     | PUBLIC_CRYPTO_DMA_USE_POLLING (poll the end of DMA)
 *output: dest : pointer of the output data (can be eq to src)
 */
static bool tf_aes_update_dma(u8 *src, u8 *dest, u32 nb_blocks,
			      u32 ctrl, bool is_kernel)
{
	/*
	 *Note: The DMA only sees physical addresses !
	 */

	int dma_ch0;
	int dma_ch1;
	struct omap_dma_channel_params ch0_parameters;
	struct omap_dma_channel_params ch1_parameters;
	u32 length = nb_blocks * AES_BLOCK_SIZE;
	u32 length_loop = 0;
	u32 nb_blocksLoop = 0;
	struct tf_device *dev = tf_get_device();

	dprintk(KERN_INFO
		"%s: In=0x%08x, Out=0x%08x, Len=%u\n",
		__func__,
		(unsigned int)src,
		(unsigned int)dest,
		(unsigned int)length);

	/*lock the DMA */
	while (!mutex_trylock(&dev->sm.dma_mutex))
		cpu_relax();

	if (tf_dma_request(&dma_ch0) != PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		mutex_unlock(&dev->sm.dma_mutex);
		return false;
	}
	if (tf_dma_request(&dma_ch1) != PUBLIC_CRYPTO_OPERATION_SUCCESS) {
		omap_free_dma(dma_ch0);
		mutex_unlock(&dev->sm.dma_mutex);
		return false;
	}

	while (length > 0) {

		/*
		 * At this time, we are sure that the DMAchannels
		 *are available and not used by other public crypto operation
		 */

		/*DMA used for Input and Output */
		OUTREG32(&paes_reg->AES_SYSCONFIG,
			INREG32(&paes_reg->AES_SYSCONFIG)
			| AES_SYSCONFIG_DMA_REQ_OUT_EN_BIT
			| AES_SYSCONFIG_DMA_REQ_IN_EN_BIT);

		/*check length */
		if (length <= dev->dma_buffer_length)
			length_loop = length;
		else
			length_loop = dev->dma_buffer_length;

		/*The length is always a multiple of the block size */
		nb_blocksLoop = length_loop / AES_BLOCK_SIZE;

		/*
		 * Copy the data from the user input buffer into a preallocated
		 * buffer which has correct properties from efficient DMA
		 * transfers.
		 */
		if (!is_kernel) {
			if (copy_from_user(
				 dev->dma_buffer, src, length_loop)) {
				omap_free_dma(dma_ch0);
				omap_free_dma(dma_ch1);
				mutex_unlock(&dev->sm.dma_mutex);
				return false;
			}
		} else {
			memcpy(dev->dma_buffer, src, length_loop);
		}

		/*DMA1: Mem -> AES */
		tf_dma_set_channel_common_params(&ch0_parameters,
			nb_blocksLoop,
			DMA_CEN_Elts_per_Frame_AES,
			AES1_REGS_HW_ADDR + 0x60,
			(u32)dev->dma_buffer_phys,
			OMAP44XX_DMA_AES1_P_DATA_IN_REQ);

		ch0_parameters.src_amode = OMAP_DMA_AMODE_POST_INC;
		ch0_parameters.dst_amode = OMAP_DMA_AMODE_CONSTANT;
		ch0_parameters.src_or_dst_synch = OMAP_DMA_DST_SYNC;

		dprintk(KERN_INFO "%s: omap_set_dma_params(ch0)\n", __func__);
		omap_set_dma_params(dma_ch0, &ch0_parameters);

		omap_set_dma_src_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_8);
		omap_set_dma_dest_burst_mode(dma_ch0, OMAP_DMA_DATA_BURST_8);
		omap_set_dma_src_data_pack(dma_ch0, 1);

		/*DMA2: AES -> Mem */
		tf_dma_set_channel_common_params(&ch1_parameters,
			nb_blocksLoop,
			DMA_CEN_Elts_per_Frame_AES,
			(u32)dev->dma_buffer_phys,
			AES1_REGS_HW_ADDR + 0x60,
			OMAP44XX_DMA_AES1_P_DATA_OUT_REQ);

		ch1_parameters.src_amode = OMAP_DMA_AMODE_CONSTANT;
		ch1_parameters.dst_amode = OMAP_DMA_AMODE_POST_INC;
		ch1_parameters.src_or_dst_synch = OMAP_DMA_SRC_SYNC;

		dprintk(KERN_INFO "%s: omap_set_dma_params(ch1)\n", __func__);
		omap_set_dma_params(dma_ch1, &ch1_parameters);

		omap_set_dma_src_burst_mode(dma_ch1, OMAP_DMA_DATA_BURST_8);
		omap_set_dma_dest_burst_mode(dma_ch1, OMAP_DMA_DATA_BURST_8);
		omap_set_dma_dest_data_pack(dma_ch1, 1);

		wmb();

		dprintk(KERN_INFO
			"%s: Start DMA channel %d\n",
			__func__, (unsigned int)dma_ch1);
		tf_dma_start(dma_ch1, OMAP_DMA_BLOCK_IRQ);
		dprintk(KERN_INFO
			"%s: Start DMA channel %d\n",
			__func__, (unsigned int)dma_ch0);
		tf_dma_start(dma_ch0, OMAP_DMA_BLOCK_IRQ);

		dprintk(KERN_INFO
			"%s: Waiting for IRQ\n", __func__);
		tf_dma_wait(2);

		/*Unset DMA synchronisation requests */
		OUTREG32(&paes_reg->AES_SYSCONFIG,
				INREG32(&paes_reg->AES_SYSCONFIG)
			& (~AES_SYSCONFIG_DMA_REQ_OUT_EN_BIT)
			& (~AES_SYSCONFIG_DMA_REQ_IN_EN_BIT));

		omap_clear_dma(dma_ch0);
		omap_clear_dma(dma_ch1);

		/*
		 *The dma transfer is complete
		 */

		pr_info("%s completing\n", __func__);
#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
		tf_aes_fault_injection(ctrl, dev->dma_buffer);
#endif

		/*The DMA output is in the preallocated aligned buffer
		 *and needs to be copied to the output buffer.*/
		if (!is_kernel) {
			if (copy_to_user(
				dest, dev->dma_buffer, length_loop)) {
				omap_free_dma(dma_ch0);
				omap_free_dma(dma_ch1);
				mutex_unlock(&dev->sm.dma_mutex);
				return false;
			}
		} else {
			memcpy(dest, dev->dma_buffer, length_loop);
		}

		src += length_loop;
		dest += length_loop;
		length -= length_loop;
	}

	/*For safety reasons, let's clean the working buffer */
	memset(dev->dma_buffer, 0, length_loop);

	/*release the DMA */
	omap_free_dma(dma_ch0);
	omap_free_dma(dma_ch1);

	mutex_unlock(&dev->sm.dma_mutex);

	dprintk(KERN_INFO "%s: Success\n", __func__);

	return true;
}

#ifdef CONFIG_SMC_KERNEL_CRYPTO
/*
 * AES HWA registration into kernel crypto framework
 */

#define CRYPTO_TFM_REQ_DMA_VISIBLE     0x80000000

static void __iomem *omap_dma_base;

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

static int aes_dma_start(struct aes_hwa_ctx *ctx)
{
	int err, fast = 0, in, out;
	size_t count;
	dma_addr_t addr_in, addr_out;
	struct omap_dma_channel_params dma_params;
	struct tf_crypto_aes_operation_state *state =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(ctx->req));
	static size_t last_count;
	unsigned long flags;

	in = IS_ALIGNED((u32)ctx->in_sg->offset, sizeof(u32));
	out = IS_ALIGNED((u32)ctx->out_sg->offset, sizeof(u32));

	fast = in && out;

	if (fast) {
		count = min(ctx->total, sg_dma_len(ctx->in_sg));
		count = min(count, sg_dma_len(ctx->out_sg));

		if (count != ctx->total)
			return -EINVAL;

		/* Only call dma_map_sg if it has not yet been done */
		if (!(ctx->req->base.flags & CRYPTO_TFM_REQ_DMA_VISIBLE)) {
			err = dma_map_sg(NULL, ctx->in_sg, 1, DMA_TO_DEVICE);
			if (!err)
				return -EINVAL;

			err = dma_map_sg(NULL, ctx->out_sg, 1, DMA_FROM_DEVICE);
			if (!err) {
				dma_unmap_sg(
					NULL, ctx->in_sg, 1, DMA_TO_DEVICE);
				return -EINVAL;
			}
		}
		ctx->req->base.flags &= ~CRYPTO_TFM_REQ_DMA_VISIBLE;

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

	/* Configure HWA */
	tf_crypto_enable_clock(PUBLIC_CRYPTO_AES1_CLOCK_REG);

	tf_aes_restore_registers(state, ctx->flags & FLAGS_ENCRYPT ? 1 : 0);

	OUTREG32(&paes_reg->AES_SYSCONFIG, INREG32(&paes_reg->AES_SYSCONFIG)
		| AES_SYSCONFIG_DMA_REQ_OUT_EN_BIT
		| AES_SYSCONFIG_DMA_REQ_IN_EN_BIT);

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
	dma_params.read_prio = 0;
	dma_params.write_prio = 0;

	/* IN */
	dma_params.trigger = ctx->dma_in;
	dma_params.src_or_dst_synch = OMAP_DMA_DST_SYNC;
	dma_params.dst_start = AES1_REGS_HW_ADDR + 0x60;
	dma_params.dst_amode = OMAP_DMA_AMODE_CONSTANT;
	dma_params.src_start = addr_in;
	dma_params.src_amode = OMAP_DMA_AMODE_POST_INC;

	if (reconfigure_dma) {
		omap_set_dma_params(ctx->dma_lch_in, &dma_params);
		omap_set_dma_dest_burst_mode(ctx->dma_lch_in,
			OMAP_DMA_DATA_BURST_8);
		omap_set_dma_src_burst_mode(ctx->dma_lch_in,
			OMAP_DMA_DATA_BURST_8);
		omap_set_dma_src_data_pack(ctx->dma_lch_in, 1);
	} else {
		if (last_count != count)
			omap_set_dma_transfer_params(ctx->dma_lch_in,
				dma_params.data_type,
				dma_params.elem_count, dma_params.frame_count,
				dma_params.sync_mode, dma_params.trigger,
				dma_params.src_or_dst_synch);

		/* Configure input start address */
		__raw_writel(dma_params.src_start,
			omap_dma_base + (0x60 * (ctx->dma_lch_in) + 0x9c));
	}

	/* OUT */
	dma_params.trigger = ctx->dma_out;
	dma_params.src_or_dst_synch = OMAP_DMA_SRC_SYNC;
	dma_params.src_start = AES1_REGS_HW_ADDR + 0x60;
	dma_params.src_amode = OMAP_DMA_AMODE_CONSTANT;
	dma_params.dst_start = addr_out;
	dma_params.dst_amode = OMAP_DMA_AMODE_POST_INC;

	if (reconfigure_dma) {
		omap_set_dma_params(ctx->dma_lch_out, &dma_params);
		omap_set_dma_dest_burst_mode(ctx->dma_lch_out,
			OMAP_DMA_DATA_BURST_8);
		omap_set_dma_src_burst_mode(ctx->dma_lch_out,
			OMAP_DMA_DATA_BURST_8);
		omap_set_dma_dest_data_pack(ctx->dma_lch_out, 1);
		reconfigure_dma = false;
	} else {
		if (last_count != count) {
			omap_set_dma_transfer_params(ctx->dma_lch_out,
				dma_params.data_type,
				dma_params.elem_count, dma_params.frame_count,
				dma_params.sync_mode, dma_params.trigger,
				dma_params.src_or_dst_synch);
			last_count = count;
		}
		/* Configure output start address */
		__raw_writel(dma_params.dst_start,
			omap_dma_base + (0x60 * (ctx->dma_lch_out) + 0xa0));
	}

	/* Is this really needed? */
	omap_enable_dma_irq(ctx->dma_lch_in, OMAP_DMA_BLOCK_IRQ);
	omap_enable_dma_irq(ctx->dma_lch_out, OMAP_DMA_BLOCK_IRQ);

	wmb();

	omap_start_dma(ctx->dma_lch_in);
	omap_start_dma(ctx->dma_lch_out);

	spin_lock_irqsave(&ctx->lock, flags);
	if (ctx->next_req) {
		struct ablkcipher_request *req =
			ablkcipher_request_cast(ctx->next_req);

		if (!(ctx->next_req->flags & CRYPTO_TFM_REQ_DMA_VISIBLE)) {
			err = dma_map_sg(NULL, req->src, 1, DMA_TO_DEVICE);
			if (!err) {
				/* Silently fail for now... */
				spin_unlock_irqrestore(&ctx->lock, flags);
				return 0;
			}

			err = dma_map_sg(NULL, req->dst, 1, DMA_FROM_DEVICE);
			if (!err) {
				dma_unmap_sg(NULL, req->src, 1, DMA_TO_DEVICE);
				/* Silently fail for now... */
				spin_unlock_irqrestore(&ctx->lock, flags);
				return 0;
			}

			ctx->next_req->flags |= CRYPTO_TFM_REQ_DMA_VISIBLE;
			ctx->next_req = NULL;
		}
	}

	if (ctx->backlog) {
		ctx->backlog->complete(ctx->backlog, -EINPROGRESS);
		ctx->backlog = NULL;
	}
	spin_unlock_irqrestore(&ctx->lock, flags);

	return 0;
}

static int aes_dma_stop(struct aes_hwa_ctx *ctx)
{
	struct tf_crypto_aes_operation_state *state =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(ctx->req));
	int err = 0;
	size_t count;

	dprintk(KERN_INFO "aes_dma_stop(%p)\n", ctx);

	tf_aes_save_registers(state);

	if (!AES_CTRL_IS_MODE_ECB(state->CTRL)) {
		u32 *ptr = (u32 *) ctx->req->info;

		ptr[0] = state->AES_IV_0;
		ptr[1] = state->AES_IV_1;
		ptr[2] = state->AES_IV_2;
		ptr[3] = state->AES_IV_3;
	}

	OUTREG32(&paes_reg->AES_SYSCONFIG, 0);

	omap_stop_dma(ctx->dma_lch_in);
	omap_stop_dma(ctx->dma_lch_out);

	tf_crypto_disable_clock(PUBLIC_CRYPTO_AES1_CLOCK_REG);

	if (!(ctx->flags & FLAGS_FAST)) {
		dma_sync_single_for_device(NULL, ctx->dma_addr_out,
			ctx->dma_size, DMA_FROM_DEVICE);

#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
		tf_aes_fault_injection(paes_reg->AES_CTRL, ctx->buf_out);
#endif

		/* Copy data */
		count = sg_copy(&ctx->out_sg, &ctx->out_offset, ctx->buf_out,
			ctx->buflen, ctx->dma_size, 1);
		if (count != ctx->dma_size)
			err = -EINVAL;
	} else {
		dma_unmap_sg(NULL, ctx->out_sg, 1, DMA_FROM_DEVICE);
		dma_unmap_sg(NULL, ctx->in_sg, 1, DMA_TO_DEVICE);

#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
		tf_aes_fault_injection(paes_reg->AES_CTRL,
			sg_virt(ctx->out_sg));
#endif
	}

	if (err || !ctx->total)
		ctx->req->base.complete(&ctx->req->base, err);

	return err;
}

static void aes_dma_callback(int lch, u16 ch_status, void *data)
{
	struct aes_hwa_ctx *ctx = data;

	if (ch_status != OMAP_DMA_BLOCK_IRQ)
		printk(KERN_ERR "%s: DMA error status: 0x%hx\n",
			__func__, ch_status);

	if (lch == ctx->dma_lch_out)
		tasklet_schedule(&ctx->task);
}

static int aes_dma_init(struct aes_hwa_ctx *ctx)
{
	int err = -ENOMEM;
	struct omap_dma_channel_params dma_params;

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

	dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;
	dma_params.elem_count = DMA_CEN_Elts_per_Frame_AES;
	dma_params.src_ei = 0;
	dma_params.src_fi = 0;
	dma_params.dst_ei = 0;
	dma_params.dst_fi = 0;
	dma_params.read_prio = 0;
	dma_params.write_prio = 0;
	dma_params.sync_mode = OMAP_DMA_SYNC_FRAME;

	/* IN */
	dma_params.trigger = ctx->dma_in;
	dma_params.src_or_dst_synch = OMAP_DMA_DST_SYNC;
	dma_params.dst_start = AES1_REGS_HW_ADDR + 0x60;
	dma_params.dst_amode = OMAP_DMA_AMODE_CONSTANT;
	dma_params.src_amode = OMAP_DMA_AMODE_POST_INC;

	omap_set_dma_params(ctx->dma_lch_in, &dma_params);
	omap_set_dma_dest_burst_mode(ctx->dma_lch_in, OMAP_DMA_DATA_BURST_8);
	omap_set_dma_src_burst_mode(ctx->dma_lch_in, OMAP_DMA_DATA_BURST_8);
	omap_set_dma_src_data_pack(ctx->dma_lch_in, 1);

	/* OUT */
	dma_params.trigger = ctx->dma_out;
	dma_params.src_or_dst_synch = OMAP_DMA_SRC_SYNC;
	dma_params.src_start = AES1_REGS_HW_ADDR + 0x60;
	dma_params.src_amode = OMAP_DMA_AMODE_CONSTANT;
	dma_params.dst_amode = OMAP_DMA_AMODE_POST_INC;

	omap_set_dma_params(ctx->dma_lch_out, &dma_params);
	omap_set_dma_dest_burst_mode(ctx->dma_lch_out, OMAP_DMA_DATA_BURST_8);
	omap_set_dma_src_burst_mode(ctx->dma_lch_out, OMAP_DMA_DATA_BURST_8);
	omap_set_dma_dest_data_pack(ctx->dma_lch_out, 1);

	omap_dma_base = ioremap(0x4A056000, 0x1000);
	if (!omap_dma_base) {
		printk(KERN_ERR "SMC: Unable to ioremap DMA registers\n");
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

static void aes_dma_cleanup(struct aes_hwa_ctx *ctx)
{
	omap_free_dma(ctx->dma_lch_out);
	omap_free_dma(ctx->dma_lch_in);
	dma_free_coherent(NULL, ctx->buflen, ctx->buf_in, ctx->dma_addr_in);
	dma_free_coherent(NULL, ctx->buflen, ctx->buf_out, ctx->dma_addr_out);
	iounmap(omap_dma_base);
}

static int aes_handle_req(struct aes_hwa_ctx *ctx)
{
	struct tf_crypto_aes_operation_state *state;
	struct crypto_async_request *async_req;
	struct ablkcipher_request *req;
	struct aes_reqctx *rctx;
	unsigned long flags;

	if (ctx->total)
		goto start;

	spin_lock_irqsave(&ctx->lock, flags);
	ctx->backlog = crypto_get_backlog(&ctx->queue);
	async_req = crypto_dequeue_request(&ctx->queue);
	if (!async_req)
		clear_bit(FLAGS_BUSY, &ctx->flags);
	spin_unlock_irqrestore(&ctx->lock, flags);

	if (!async_req)
		return 0;

	req = ablkcipher_request_cast(async_req);

	ctx->req = req;
	ctx->total = req->nbytes;
	ctx->in_offset = 0;
	ctx->in_sg = req->src;
	ctx->out_offset = 0;
	ctx->out_sg = req->dst;

	rctx = ablkcipher_request_ctx(req);
	rctx->mode &= FLAGS_MODE_MASK;
	ctx->flags = (ctx->flags & ~FLAGS_MODE_MASK) | rctx->mode;

	/*
	 * Try to get the next pending request so it can be prepared while the
	 * first one is being processed.
	 */
	spin_lock_irqsave(&ctx->lock, flags);

	if (likely(ctx->queue.qlen)) {
		struct list_head *next_async_req;

		next_async_req = ctx->queue.list.next;
		ctx->next_req = list_entry(next_async_req,
			struct crypto_async_request, list);
	} else {
		ctx->next_req = NULL;
	}
	spin_unlock_irqrestore(&ctx->lock, flags);

	state = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));

	if (!AES_CTRL_IS_MODE_ECB(state->CTRL)) {
		u32 *ptr = (u32 *) req->info;

		state->AES_IV_0 = ptr[0];
		state->AES_IV_1 = ptr[1];
		state->AES_IV_2 = ptr[2];
		state->AES_IV_3 = ptr[3];
	}

start:
	return aes_dma_start(ctx);
}

static void aes_tasklet(unsigned long data)
{
	struct aes_hwa_ctx *ctx = (struct aes_hwa_ctx *) data;

	aes_dma_stop(ctx);
	aes_handle_req(ctx);
}

/* Generic */
static int aes_setkey(struct tf_crypto_aes_operation_state *state,
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

	state->KEY1_0 = ptr[0];
	state->KEY1_1 = ptr[1];
	state->KEY1_2 = ptr[2];
	state->KEY1_3 = ptr[3];
	if (keylen >= 24) {
		state->KEY1_4 = ptr[4];
		state->KEY1_5 = ptr[5];
	}
	if (keylen == 32) {
		state->KEY1_6 = ptr[6];
		state->KEY1_7 = ptr[7];
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
	struct aes_reqctx *rctx = ablkcipher_request_ctx(req);

	rctx->mode = FLAGS_ENCRYPT;

	return aes_operate(req);
}

static int aes_decrypt(struct ablkcipher_request *req)
{
	struct aes_reqctx *rctx = ablkcipher_request_ctx(req);

	rctx->mode = FLAGS_DECRYPT;

	return aes_operate(req);
}


static void aes_sync_op_complete(
	struct crypto_async_request *async_req, int err)
{
	struct ablkcipher_request *req = ablkcipher_request_cast(async_req);

	/* Notify crypto operation has finished */
	atomic_set((atomic_t *) req->base.data, 0);
}

#define MIN_SYNC_REQ	8
static mempool_t *req_pool;

static int aes_sync_operate(struct blkcipher_desc *desc,
	struct scatterlist *dst, struct scatterlist *src,
	unsigned int nbytes, int encrypt)
{
	struct ablkcipher_request *req;
	struct aes_reqctx *rctx;
	int err = 0;
	atomic_t pending = ATOMIC_INIT(1);

	if (nbytes % AES_BLOCK_SIZE)
		return -EINVAL;

	req = mempool_alloc(req_pool, GFP_ATOMIC);
	if (req == NULL)
		return -ENOMEM;

	req->base.tfm = crypto_blkcipher_tfm(desc->tfm);
	ablkcipher_request_set_crypt(req, src, dst, nbytes, desc->info);
	ablkcipher_request_set_callback(req, desc->flags,
		aes_sync_op_complete, &pending);

	rctx = ablkcipher_request_ctx(req);
	rctx->mode = encrypt ? FLAGS_ENCRYPT : FLAGS_DECRYPT;

	err = aes_operate(req);
	switch (err) {
	case -EINPROGRESS:
	case -EBUSY:
		break;

	default:
		goto out;
	}

	/* Wait for crypto operation to be actually finished */
	while (atomic_read(&pending))
		cpu_relax();

out:
	mempool_free(req, req_pool);

	return err;
}

static int aes_sync_encrypt(struct blkcipher_desc *desc,
	struct scatterlist *dst, struct scatterlist *src,
	unsigned int nbytes)
{
	dprintk(KERN_INFO "aes_sync_encrypt nbytes=0x%x\n", nbytes);

	return aes_sync_operate(desc, dst, src, nbytes, 1);
}

static int aes_sync_decrypt(struct blkcipher_desc *desc,
	struct scatterlist *dst, struct scatterlist *src,
	unsigned int nbytes)
{
	dprintk(KERN_INFO "aes_sync_decrypt\n");

	return aes_sync_operate(desc, dst, src, nbytes, 0);
}

/* AES ECB */
static int aes_ecb_sync_setkey(struct crypto_tfm *tfm, const u8 *key,
	unsigned int keylen)
{
	struct tf_crypto_aes_operation_state *state = crypto_tfm_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_ECB_BIT;

	dprintk(KERN_INFO "aes_ecb_sync_setkey\n");

	return aes_setkey(state, key, keylen);
}

static int aes_ecb_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
	unsigned int keylen)
{
	struct tf_crypto_aes_operation_state *state =
		crypto_ablkcipher_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_ECB_BIT;

	return aes_setkey(state, key, keylen);
}

/* AES CBC */
static int aes_cbc_sync_setkey(struct crypto_tfm *tfm, const u8 *key,
	unsigned int keylen)
{
	struct tf_crypto_aes_operation_state *state = crypto_tfm_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_CBC_BIT;

	dprintk(KERN_INFO "aes_cbc_sync_setkey\n");

	return aes_setkey(state, key, keylen);
}

static int aes_cbc_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
	unsigned int keylen)
{
	struct tf_crypto_aes_operation_state *state =
		crypto_ablkcipher_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_CBC_BIT;

	return aes_setkey(state, key, keylen);
}

/* AES CTR */
static int aes_ctr_sync_setkey(struct crypto_tfm *tfm, const u8 *key,
	unsigned int keylen)
{
	struct tf_crypto_aes_operation_state *state = crypto_tfm_ctx(tfm);

	state->CTRL = AES_CTRL_MODE_CTR_BIT;

	dprintk(KERN_INFO "aes_cbc_sync_setkey\n");

	return aes_setkey(state, key, keylen);
}

static int aes_ctr_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
	unsigned int keylen)
{
	struct tf_crypto_aes_operation_state *state =
		crypto_ablkcipher_ctx(tfm);

	/* Always defaults to 128-bit counter */
	state->CTRL = AES_CTRL_MODE_CTR_BIT | AES_CTRL_CTR_WIDTH_128;

	return aes_setkey(state, key, keylen);
}

static int aes_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct aes_reqctx);

	return 0;
}

static void aes_cra_exit(struct crypto_tfm *tfm)
{
}

static struct crypto_alg smc_aes_ecb_sync_alg = {
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_priority		= 999,
	.cra_name		= "ecb(aes-hw)",
	.cra_driver_name	= "aes-ecb-smc",
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		=
		sizeof(struct tf_crypto_aes_operation_state),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_ecb_sync_alg.cra_list),
	.cra_u			= {
		.blkcipher = {
			.min_keysize	= AES_MIN_KEY_SIZE,
			.max_keysize	= AES_MAX_KEY_SIZE,
			.setkey		= aes_ecb_sync_setkey,
			.encrypt	= aes_sync_encrypt,
			.decrypt	= aes_sync_decrypt,
		}
	},
};

static struct crypto_alg smc_aes_cbc_sync_alg = {
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_priority		= 999,
	.cra_name		= "cbc(aes-hw)",
	.cra_driver_name	= "aes-cbc-smc",
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		=
		sizeof(struct tf_crypto_aes_operation_state),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_cbc_sync_alg.cra_list),
	.cra_u			= {
		.blkcipher = {
			.min_keysize	= AES_MIN_KEY_SIZE,
			.max_keysize	= AES_MAX_KEY_SIZE,
			.ivsize		= PUBLIC_CRYPTO_IV_MAX_SIZE,
			.setkey		= aes_cbc_sync_setkey,
			.encrypt	= aes_sync_encrypt,
			.decrypt	= aes_sync_decrypt,
		}
	},
};

static struct crypto_alg smc_aes_ctr_sync_alg = {
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_priority		= 999,
	.cra_name		= "ctr(aes-hw)",
	.cra_driver_name	= "aes-ctr-smc",
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		=
		sizeof(struct tf_crypto_aes_operation_state),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_ctr_sync_alg.cra_list),
	.cra_u			= {
		.blkcipher = {
			.min_keysize	= AES_MIN_KEY_SIZE,
			.max_keysize	= AES_MAX_KEY_SIZE,
			.ivsize		= PUBLIC_CRYPTO_IV_MAX_SIZE,
			.setkey		= aes_ctr_sync_setkey,
			.encrypt	= aes_sync_encrypt,
			.decrypt	= aes_sync_decrypt,
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
	.cra_ctxsize		=
		sizeof(struct tf_crypto_aes_operation_state),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_ecb_alg.cra_list),
	.cra_init		= aes_cra_init,
	.cra_exit		= aes_cra_exit,
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
	.cra_ctxsize		=
		sizeof(struct tf_crypto_aes_operation_state),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_cbc_alg.cra_list),
	.cra_init		= aes_cra_init,
	.cra_exit		= aes_cra_exit,
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
	.cra_ctxsize		=
		sizeof(struct tf_crypto_aes_operation_state),
	.cra_alignmask		= 3,
	.cra_list		= LIST_HEAD_INIT(smc_aes_ctr_alg.cra_list),
	.cra_init		= aes_cra_init,
	.cra_exit		= aes_cra_exit,
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


int register_smc_public_crypto_aes(void)
{
	int ret;

	tf_crypto_enable_clock(PUBLIC_CRYPTO_AES1_CLOCK_REG);
	OUTREG32(&paes_reg->AES_SYSCONFIG, 0);
	tf_crypto_disable_clock(PUBLIC_CRYPTO_AES1_CLOCK_REG);

	aes_ctx = kzalloc(sizeof(struct aes_hwa_ctx), GFP_KERNEL);
	if (aes_ctx == NULL)
		return -ENOMEM;

	crypto_init_queue(&aes_ctx->queue, 50);
	tasklet_init(&aes_ctx->task, aes_tasklet, (unsigned long)aes_ctx);
	spin_lock_init(&aes_ctx->lock);

	aes_ctx->dma_in = OMAP44XX_DMA_AES1_P_DATA_IN_REQ;
	aes_ctx->dma_out = OMAP44XX_DMA_AES1_P_DATA_OUT_REQ;

	ret = aes_dma_init(aes_ctx);
	if (ret)
		goto err_dma;

	req_pool = mempool_create_kmalloc_pool(MIN_SYNC_REQ,
		sizeof(struct ablkcipher_request) + sizeof(struct aes_reqctx));
	if (req_pool == NULL)
		goto err_dma;

	ret = crypto_register_alg(&smc_aes_ecb_sync_alg);
	if (ret)
		goto err_ecb_sync;

	ret = crypto_register_alg(&smc_aes_cbc_sync_alg);
	if (ret)
		goto err_cbc_sync;

	ret = crypto_register_alg(&smc_aes_ctr_sync_alg);
	if (ret)
		goto err_ctr_sync;

	ret = crypto_register_alg(&smc_aes_ecb_alg);
	if (ret)
		goto err_ecb;

	ret = crypto_register_alg(&smc_aes_cbc_alg);
	if (ret)
		goto err_cbc;

	ret = crypto_register_alg(&smc_aes_ctr_alg);
	if (ret)
		goto err_ctr;

	return 0;

err_ctr:
	crypto_unregister_alg(&smc_aes_cbc_alg);
err_cbc:
	crypto_unregister_alg(&smc_aes_ecb_alg);
err_ecb:
	crypto_unregister_alg(&smc_aes_ctr_sync_alg);
err_ctr_sync:
	crypto_unregister_alg(&smc_aes_cbc_sync_alg);
err_cbc_sync:
	crypto_unregister_alg(&smc_aes_ecb_sync_alg);
err_ecb_sync:
	aes_dma_cleanup(aes_ctx);
err_dma:
	tasklet_kill(&aes_ctx->task);
	kfree(aes_ctx);
	return ret;
}

void unregister_smc_public_crypto_aes(void)
{
	if (aes_ctx == NULL)
		return;

	crypto_unregister_alg(&smc_aes_ecb_sync_alg);
	crypto_unregister_alg(&smc_aes_cbc_sync_alg);
	crypto_unregister_alg(&smc_aes_ctr_sync_alg);

	crypto_unregister_alg(&smc_aes_ecb_alg);
	crypto_unregister_alg(&smc_aes_cbc_alg);
	crypto_unregister_alg(&smc_aes_ctr_alg);

	aes_dma_cleanup(aes_ctx);

	tasklet_kill(&aes_ctx->task);
	kfree(aes_ctx);

	mempool_destroy(req_pool);
}

#endif /* CONFIG_SMC_KERNEL_CRYPTO */

void tf_aes_pm_resume(void)
{
	reconfigure_dma = true;
}
