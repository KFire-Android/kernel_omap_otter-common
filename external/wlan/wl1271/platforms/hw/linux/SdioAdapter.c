/*
 * SdioAdapter.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.
 * Copyright(c) 2008 - 2009 Google, Inc. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Texas Instruments nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file   SdioAdapter.c
 *  \brief  The SDIO driver adapter. Platform dependent.
 *
 * An adaptation layer between the lower SDIO driver (in BSP) and the upper Sdio
 * Used for issuing all SDIO transaction types towards the lower SDIO-driver.
 * Makes the decision whether to use Sync or Async transaction, and reflects it
 *     by the return value and calling its callback in case of Async.
 *
 *  \see    SdioAdapter.h, SdioDrv.c & h
 */

#ifdef CONFIG_MMC_EMBEDDED_SDIO
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include "TxnDefs.h"

#define TI_SDIO_DEBUG

#define TIWLAN_MMC_MAX_DMA                 8192

int wifi_set_carddetect( int on );

static struct sdio_func *tiwlan_func = NULL;
static struct completion sdio_wait;

ETxnStatus sdioAdapt_TransactBytes (unsigned int  uFuncId,
                                    unsigned int  uHwAddr,
                                    void *        pHostAddr,
                                    unsigned int  uLength,
                                    unsigned int  bDirection,
                                    unsigned int  bMore);

static int sdio_wifi_probe(struct sdio_func *func,
                           const struct sdio_device_id *id)
{
        int rc;

        printk("%s: %d\n", __FUNCTION__, func->class);

        if (func->class != SDIO_CLASS_WLAN)
                return -EINVAL;

        sdio_claim_host(func);

        rc = sdio_enable_func(func);
        if (rc)
                goto err1;
        rc = sdio_set_block_size(func, 512);

        if (rc) {
                printk("%s: Unable to set blocksize\n", __FUNCTION__);
                goto err2;
        }

        tiwlan_func = func;
        complete(&sdio_wait);
        return 0;
err2:
        sdio_disable_func(func);
err1:
        sdio_release_host(func);
        complete(&sdio_wait);
        return rc;
}

static void sdio_wifi_remove(struct sdio_func *func)
{
}

static const struct sdio_device_id sdio_wifi_ids[] = {
        { SDIO_DEVICE_CLASS(SDIO_CLASS_WLAN)    },
        {                                       },
};

MODULE_DEVICE_TABLE(sdio, sdio_wifi_ids);

static struct sdio_driver sdio_wifi_driver = {
        .probe          = sdio_wifi_probe,
        .remove         = sdio_wifi_remove,
        .name           = "sdio_wifi",
        .id_table       = sdio_wifi_ids,
};

ETxnStatus sdioAdapt_TransactBytes (unsigned int  uFuncId,
                                    unsigned int  uHwAddr,
                                    void *        pHostAddr,
                                    unsigned int  uLength,
                                    unsigned int  bDirection,
                                    unsigned int  bMore);

int sdioAdapt_ConnectBus (void *        fCbFunc,
                          void *        hCbArg,
                          unsigned int  uBlkSizeShift,
                          unsigned int  uSdioThreadPriority,
                          unsigned char **pTxDmaSrcAddr)
{
	int rc;

	init_completion(&sdio_wait);
	wifi_set_carddetect( 1 );
	rc = sdio_register_driver(&sdio_wifi_driver);
	if (rc < 0) {
		printk(KERN_ERR "%s: Fail to register sdio_wifi_driver\n", __func__);
		return rc;
	}
	if (!wait_for_completion_timeout(&sdio_wait, msecs_to_jiffies(10000))) {
		printk(KERN_ERR "%s: Timed out waiting for device detect\n", __func__);
		sdio_unregister_driver(&sdio_wifi_driver);
		return -ENODEV;
	}
	/* Provide the DMA buffer address to the upper layer so it will use it as the transactions host buffer. */
	if (pTxDmaSrcAddr) { /* Dm: check what to do with it */
		*pTxDmaSrcAddr = kmalloc(TIWLAN_MMC_MAX_DMA, GFP_KERNEL | GFP_DMA);
	}
	return 0;
}

int sdioAdapt_DisconnectBus (void)
{
	if (tiwlan_func) {
		sdio_disable_func( tiwlan_func );
		sdio_release_host( tiwlan_func );
	}
	wifi_set_carddetect( 0 );
	sdio_unregister_driver(&sdio_wifi_driver);
	return 0;
}

ETxnStatus sdioAdapt_TransactBytes (unsigned int  uFuncId,
                                    unsigned int  uHwAddr,
                                    void *        pHostAddr,
                                    unsigned int  uLength,
                                    unsigned int  bDirection,
                                    unsigned int  bMore)
{
	unsigned char *pData = pHostAddr;
	unsigned int i;
	int rc = 0, final_rc = 0;

	for (i = 0; i < uLength; i++) {
		if( bDirection ) {
			if (uFuncId == 0)
				*pData = (unsigned char)sdio_f0_readb(tiwlan_func, uHwAddr, &rc);
			else
				*pData = (unsigned char)sdio_readb(tiwlan_func, uHwAddr, &rc);
		}
		else {
			if (uFuncId == 0)
				sdio_f0_writeb(tiwlan_func, *pData, uHwAddr, &rc);
			else
				sdio_writeb(tiwlan_func, *pData, uHwAddr, &rc);
		}
		if( rc ) {
			final_rc = rc;
		}
#ifdef TI_SDIO_DEBUG
		printk(KERN_INFO "%c52: [0x%x](%u) %c 0x%x\n", (bDirection ? 'R' : 'W'), uHwAddr, uLength, (bDirection ? '=' : '<'), (unsigned)*pData);
#endif
		uHwAddr++;
		pData++;
	}
	/* If failed return ERROR, if succeeded return COMPLETE */
	if (final_rc) {
		return TXN_STATUS_ERROR;
	}
	return TXN_STATUS_COMPLETE;
}

ETxnStatus sdioAdapt_Transact (unsigned int  uFuncId,
                               unsigned int  uHwAddr,
                               void *        pHostAddr,
                               unsigned int  uLength,
                               unsigned int  bDirection,
                               unsigned int  bBlkMode,
                               unsigned int  bFixedAddr,
                               unsigned int  bMore)
{
	int rc;

	if (uFuncId == 0)
		return sdioAdapt_TransactBytes (uFuncId, uHwAddr, pHostAddr,
						uLength, bDirection, bMore);
	if (bDirection) {
		if (bFixedAddr)
			rc = sdio_memcpy_fromio(tiwlan_func, pHostAddr, uHwAddr, uLength);
		else
			rc = sdio_readsb(tiwlan_func, pHostAddr, uHwAddr, uLength);

	}
	else {
		if (bFixedAddr)
			rc = sdio_memcpy_toio(tiwlan_func, uHwAddr, pHostAddr, uLength);
		else
			rc = sdio_writesb(tiwlan_func, uHwAddr, pHostAddr, uLength);
	}
#ifdef TI_SDIO_DEBUG
	if (uLength == 1)
	        printk(KERN_INFO "%c53: [0x%x](%u) %c 0x%x\n", (bDirection ? 'R' : 'W'), uHwAddr, uLength, (bDirection ? '=' : '<'), (unsigned)(*(char *)pHostAddr));
	else if (uLength == 2)
	        printk(KERN_INFO "%c53: [0x%x](%u) %c 0x%x\n", (bDirection ? 'R' : 'W'), uHwAddr, uLength, (bDirection ? '=' : '<'), (unsigned)(*(short *)pHostAddr));
	else if (uLength == 4)
	        printk(KERN_INFO "%c53: [0x%x](%u) %c 0x%x\n", (bDirection ? 'R' : 'W'), uHwAddr, uLength, (bDirection ? '=' : '<'), (unsigned)(*(long *)pHostAddr));
	else
		printk(KERN_INFO "%c53: [0x%x](%u) F[%d] B[%d] I[%d] = %d\n", (bDirection ? 'R' : 'W'), uHwAddr, uLength, uFuncId, bBlkMode, bFixedAddr, rc);
#endif
	/* If failed return ERROR, if succeeded return COMPLETE */
	if (rc) {
		return TXN_STATUS_ERROR;
	}
	return TXN_STATUS_COMPLETE;
}

#else

#include "SdioDrvDbg.h"
#include "TxnDefs.h"
#include "SdioAdapter.h"
#include "SdioDrv.h"
#include "bmtrace_api.h"
#include <linux/slab.h>

#ifdef SDIO_1_BIT /* see also in SdioDrv.c */
#define SDIO_BITS_CODE   0x80 /* 1 bits */
#else
#define SDIO_BITS_CODE   0x82 /* 4 bits */
#endif

static unsigned char *pDmaBufAddr = 0;

/************************************************************************
 * Defines
 ************************************************************************/
/* Sync/Async Threshold */
#ifdef FULL_ASYNC_MODE
#define SYNC_ASYNC_LENGTH_THRESH	0     /* Use Async for all transactions */
#else
#define SYNC_ASYNC_LENGTH_THRESH	360   /* Use Async for transactions longer than this threshold (in bytes) */
#endif

#define MAX_RETRIES                 10

#define MAX_BUS_TXN_SIZE            8192  /* Max bus transaction size in bytes (for the DMA buffer allocation) */

/* For block mode configuration */
#define FN0_FBR2_REG_108                    0x210
#define FN0_FBR2_REG_108_BIT_MASK           0xFFF

int sdioAdapt_ConnectBus (void *        fCbFunc,
                          void *        hCbArg,
                          unsigned int  uBlkSizeShift,
                          unsigned int  uSdioThreadPriority,
                          unsigned char **pRxDmaBufAddr,
                          unsigned int  *pRxDmaBufLen,
                          unsigned char **pTxDmaBufAddr,
                          unsigned int  *pTxDmaBufLen)
{
	unsigned char  uByte;
	unsigned long  uLong;
	unsigned long  uCount = 0;
	unsigned int   uBlkSize = 1 << uBlkSizeShift;
	int            iStatus;

	if (uBlkSize < SYNC_ASYNC_LENGTH_THRESH)
	{
		PERR1("%s(): Block-Size should be bigger than SYNC_ASYNC_LENGTH_THRESH!!\n", __FUNCTION__ );
	}

	/* Enabling clocks if thet are not enabled */
	sdioDrv_clk_enable();

	/* Allocate a DMA-able buffer and provide it to the upper layer to be used for all read and write transactions */
	if (pDmaBufAddr == 0) /* allocate only once (in case this function is called multiple times) */
	{
		pDmaBufAddr = kmalloc(MAX_BUS_TXN_SIZE, GFP_KERNEL | GFP_DMA);
		if (pDmaBufAddr == 0)
		{
			iStatus = -1;
			goto fail;
		}
	}
	*pRxDmaBufAddr = *pTxDmaBufAddr = pDmaBufAddr;
	*pRxDmaBufLen  = *pTxDmaBufLen  = MAX_BUS_TXN_SIZE;

	/* Init SDIO driver and HW */
	iStatus = sdioDrv_ConnectBus (fCbFunc, hCbArg, uBlkSizeShift, uSdioThreadPriority);
	if (iStatus) { goto fail; }

	/* Send commands sequence: 0, 5, 3, 7 */
	iStatus = sdioDrv_ExecuteCmd (SD_IO_GO_IDLE_STATE, 0, MMC_RSP_NONE, &uByte, sizeof(uByte));
	if (iStatus)
	{
		printk("%s %d command number: %d failed\n", __FUNCTION__, __LINE__, SD_IO_GO_IDLE_STATE);
		goto fail;
	}

	iStatus = sdioDrv_ExecuteCmd (SDIO_CMD5, VDD_VOLTAGE_WINDOW, MMC_RSP_R4, &uByte, sizeof(uByte));
	if (iStatus) {
		printk("%s %d command number: %d failed\n", __FUNCTION__, __LINE__, SDIO_CMD5);
		goto fail;
	}

	iStatus = sdioDrv_ExecuteCmd (SD_IO_SEND_RELATIVE_ADDR, 0, MMC_RSP_R6, &uLong, sizeof(uLong));
	if (iStatus) {
		printk("%s %d command number: %d failed\n", __FUNCTION__, __LINE__, SD_IO_SEND_RELATIVE_ADDR);
		goto fail;
	}

	iStatus = sdioDrv_ExecuteCmd (SD_IO_SELECT_CARD, uLong, MMC_RSP_R6, &uByte, sizeof(uByte));
	if (iStatus) {
		printk("%s %d command number: %d failed\n", __FUNCTION__, __LINE__, SD_IO_SELECT_CARD);
		goto fail;
	}

    /* NOTE:
     * =====
     * Each of the following loops is a workaround for a HW bug that will be solved in PG1.1 !!
     * Each write of CMD-52 to function-0 should use it as follows:
     * 1) Write the desired byte using CMD-52
     * 2) Read back the byte using CMD-52
     * 3) Write two dummy bytes to address 0xC8 using CMD-53
     * 4) If the byte read in step 2 is different than the written byte repeat the sequence
     */

	/* set device side bus width to 4 bit (for 1 bit write 0x80 instead of 0x82) */
	do
	{
		uByte = SDIO_BITS_CODE;
		iStatus = sdioDrv_WriteSyncBytes (TXN_FUNC_ID_CTRL, CCCR_BUS_INTERFACE_CONTOROL, &uByte, 1, 1);
		if (iStatus) { goto fail; }

		iStatus = sdioDrv_ReadSyncBytes (TXN_FUNC_ID_CTRL, CCCR_BUS_INTERFACE_CONTOROL, &uByte, 1, 1);
		if (iStatus) { goto fail; }
        
		iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, 0xC8, &uLong, 2, 1, 1);
		if (iStatus) { goto fail; }

		uCount++;

	} while ((uByte != SDIO_BITS_CODE) && (uCount < MAX_RETRIES));

	uCount = 0;

	/* allow function 2 */
	do
	{
		uByte = 4;
		iStatus = sdioDrv_WriteSyncBytes (TXN_FUNC_ID_CTRL, CCCR_IO_ENABLE, &uByte, 1, 1);
		if (iStatus) { goto fail; }

		iStatus = sdioDrv_ReadSyncBytes (TXN_FUNC_ID_CTRL, CCCR_IO_ENABLE, &uByte, 1, 1);
		if (iStatus) { goto fail; }
        
		iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, 0xC8, &uLong, 2, 1, 1);
		if (iStatus) { goto fail; }

		uCount++;

	} while ((uByte != 4) && (uCount < MAX_RETRIES));


#ifdef SDIO_IN_BAND_INTERRUPT

	uCount = 0;

	do
	{
		uByte = 3;
		iStatus = sdioDrv_WriteSyncBytes (TXN_FUNC_ID_CTRL, CCCR_INT_ENABLE, &uByte, 1, 1);
		if (iStatus) { goto fail; }

		iStatus = sdioDrv_ReadSyncBytes (TXN_FUNC_ID_CTRL, CCCR_INT_ENABLE, &uByte, 1, 1);
		if (iStatus) { goto fail; }

		iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, 0xC8, &uLong, 2, 1, 1);
		if (iStatus) { goto fail; }

		uCount++;

	} while ((uByte != 3) && (uCount < MAX_RETRIES));


#endif

	uCount = 0;

	/* set block size for SDIO block mode */
	do
	{
		uLong = uBlkSize;
		iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, FN0_FBR2_REG_108, &uLong, 2, 1, 1);
		if (iStatus) { goto fail; }

		iStatus = sdioDrv_ReadSync (TXN_FUNC_ID_CTRL, FN0_FBR2_REG_108, &uLong, 2, 1, 1);
		if (iStatus) { goto fail; }

		iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, 0xC8, &uLong, 2, 1, 1);
		if (iStatus) { goto fail; }

		uCount++;

	} while (((uLong & FN0_FBR2_REG_108_BIT_MASK) != uBlkSize) && (uCount < MAX_RETRIES));

	if (uCount >= MAX_RETRIES)
	{
		/* Failed to write CMD52_WRITE to function 0 */
		iStatus = (int)uCount;
	}

fail:
	/* Disable the clocks for now */
	sdioDrv_clk_disable();

	return iStatus;
}


int sdioAdapt_DisconnectBus (void)
{
	if (pDmaBufAddr)
	{
		kfree (pDmaBufAddr);
		pDmaBufAddr = 0;
	}

	return sdioDrv_DisconnectBus ();
}

ETxnStatus sdioAdapt_Transact (unsigned int  uFuncId,
                               unsigned int  uHwAddr,
                               void *        pHostAddr,
                               unsigned int  uLength,
                               unsigned int  bDirection,
                               unsigned int  bBlkMode,
                               unsigned int  bFixedAddr,
                               unsigned int  bMore)
{
	int iStatus;

	/* If transction length is below threshold, use Sync methods */
	if (uLength < SYNC_ASYNC_LENGTH_THRESH)
	{
		/* Call read or write Sync method */
		if (bDirection)
		{
			CL_TRACE_START_L2();
			iStatus = sdioDrv_ReadSync (uFuncId, uHwAddr, pHostAddr, uLength, bFixedAddr, bMore);
			CL_TRACE_END_L2("tiwlan_drv.ko", "INHERIT", "SDIO", ".ReadSync");
		}
		else
		{
			CL_TRACE_START_L2();
			iStatus = sdioDrv_WriteSync (uFuncId, uHwAddr, pHostAddr, uLength, bFixedAddr, bMore);
			CL_TRACE_END_L2("tiwlan_drv.ko", "INHERIT", "SDIO", ".WriteSync");
		}

		/* If failed return ERROR, if succeeded return COMPLETE */
		if (iStatus)
		{
			return TXN_STATUS_ERROR;
		}
		return TXN_STATUS_COMPLETE;
	}

	/* If transction length is above threshold, use Async methods */
	else
	{
		/* Call read or write Async method */
		if (bDirection)
		{
			CL_TRACE_START_L2();
			iStatus = sdioDrv_ReadAsync (uFuncId, uHwAddr, pHostAddr, uLength, bBlkMode, bFixedAddr, bMore);
			CL_TRACE_END_L2("tiwlan_drv.ko", "INHERIT", "SDIO", ".ReadAsync");
		}
		else
		{
			CL_TRACE_START_L2();
			iStatus = sdioDrv_WriteAsync (uFuncId, uHwAddr, pHostAddr, uLength, bBlkMode, bFixedAddr, bMore);
			CL_TRACE_END_L2("tiwlan_drv.ko", "INHERIT", "SDIO", ".WriteAsync");
		}

		/* If failed return ERROR, if succeeded return PENDING */
		if (iStatus)
		{
			return TXN_STATUS_ERROR;
		}
		return TXN_STATUS_PENDING;
	}
}
         
ETxnStatus sdioAdapt_TransactBytes (unsigned int  uFuncId,
                                    unsigned int  uHwAddr,
                                    void *        pHostAddr,
                                    unsigned int  uLength,
                                    unsigned int  bDirection,
                                    unsigned int  bMore)
{
	static unsigned int lastMore = 0;
	int iStatus;

	if ((bMore == 1) || (lastMore == bMore))
	{
		sdioDrv_cancel_inact_timer();
		sdioDrv_clk_enable();
	}

	/* Call read or write bytes Sync method */
	if (bDirection)
	{
		iStatus = sdioDrv_ReadSyncBytes (uFuncId, uHwAddr, pHostAddr, uLength, bMore);
	}
	else
	{
		iStatus = sdioDrv_WriteSyncBytes (uFuncId, uHwAddr, pHostAddr, uLength, bMore);
	}

	if (bMore == 0)
	{
		sdioDrv_start_inact_timer();
	}
	lastMore = bMore;

	/* If failed return ERROR, if succeeded return COMPLETE */
	if (iStatus)
	{
		return TXN_STATUS_ERROR;
	}
	return TXN_STATUS_COMPLETE;
}
#endif
