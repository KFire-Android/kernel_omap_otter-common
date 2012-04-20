/*
 * SdioBusDrv.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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

 
/** \file   SdioBusDrv.c 
 *  \brief  The SDIO bus driver upper layer. Platform independent. 
 *          Uses the SdioAdapter API. 
 *          Introduces a generic bus-independent API upwards.
 *  
 *  \see    BusDrv.h, SdioAdapter.h, SdioAdapter.c
 */

#define __FILE_ID__  FILE_ID_122
#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "TxnDefs.h"
#include "SdioAdapter.h"
#include "BusDrv.h"
#include "bmtrace_api.h"



/************************************************************************
 * Defines
 ************************************************************************/
#define MAX_TXN_PARTS     MAX_XFER_BUFS * 5   /* for aggregation we may need a few parts for each buffer */


/************************************************************************
 * Types
 ************************************************************************/

/* A single SDIO bus transaction which is a part of a complete transaction (TTxnStruct) */ 
typedef struct
{
    TI_BOOL          bBlkMode;           /* If TRUE this is a block-mode SDIO transaction */
    TI_UINT32        uLength;            /* Length in byte */
    TI_UINT32        uHwAddr;            /* The device address to write to or read from */
    void *           pHostAddr;          /* The host buffer address to write from or read into */
    TI_BOOL          bMore;              /* If TRUE, indicates the lower driver to keep awake for more transactions */
} TTxnPart; 


/* The busDrv module Object */
typedef struct _TBusDrvObj
{
    TI_HANDLE	     hOs;		   	 
    TI_HANDLE	     hReport;

	TBusDrvTxnDoneCb fTxnDoneCb;         /* The callback to call upon full transaction completion. */
	TI_HANDLE        hCbHandle;          /* The callback handle */
    TTxnStruct *     pCurrTxn;           /* The transaction currently being processed */
    ETxnStatus       eCurrTxnStatus;     /* COMPLETE, PENDING or ERROR */
    TTxnPart         aTxnParts[MAX_TXN_PARTS]; /* The actual bus transactions of current transaction */
    TI_UINT32        uCurrTxnPartsNum;   /* Number of transaction parts composing the current transaction */
    TI_UINT32        uCurrTxnPartsCount; /* Number of transaction parts already executed */
    TI_UINT32        uCurrTxnPartsCountSync; /* Number of transaction parts completed in Sync mode (returned COMPLETE) */
    TI_UINT32        uBlkSizeShift;      /* In block-mode:  uBlkSize = (1 << uBlkSizeShift) = 512 bytes */
    TI_UINT32        uBlkSize;           /* In block-mode:  uBlkSize = (1 << uBlkSizeShift) = 512 bytes */
    TI_UINT32        uBlkSizeMask;       /* In block-mode:  uBlkSizeMask = uBlkSize - 1 = 0x1FF*/
    TI_UINT8 *       pRxDmaBuf;          /* The Rx DMA-able buffer for buffering all write transactions */
    TI_UINT32        uRxDmaBufLen;       /* The Rx DMA-able buffer length in bytes */
    TI_UINT8 *       pTxDmaBuf;          /* The Tx DMA-able buffer for buffering all write transactions */
    TI_UINT32        uTxDmaBufLen;       /* The Tx DMA-able buffer length in bytes */
    TI_UINT32        uTxnLength;         /* The current transaction accumulated length (including Tx aggregation case) */

} TBusDrvObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/
static TI_BOOL  busDrv_PrepareTxnParts  (TBusDrvObj *pBusDrv, TTxnStruct *pTxn);
static void     busDrv_SendTxnParts     (TBusDrvObj *pBusDrv);
static void     busDrv_TxnDoneCb        (TI_HANDLE hBusDrv, TI_INT32 status);
 


/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     busDrv_Create 
 * \brief  Create the module
 * 
 * Create and clear the bus driver's object, and the SDIO-adapter.
 * 
 * \note   
 * \param  hOs - Handle to Os Abstraction Layer
 * \return Handle of the allocated object, NULL if allocation failed 
 * \sa     busDrv_Destroy
 */ 
TI_HANDLE busDrv_Create (TI_HANDLE hOs)
{
    TI_HANDLE   hBusDrv;
    TBusDrvObj *pBusDrv;

    hBusDrv = os_memoryAlloc(hOs, sizeof(TBusDrvObj));
    if (hBusDrv == NULL)
    {
        return NULL;
    }
    
    pBusDrv = (TBusDrvObj *)hBusDrv;

    os_memoryZero(hOs, hBusDrv, sizeof(TBusDrvObj));
    
    pBusDrv->hOs = hOs;

    return pBusDrv;
}


/** 
 * \fn     busDrv_Destroy
 * \brief  Destroy the module. 
 * 
 * Close SDIO lower bus driver and free the module's object.
 * 
 * \note   
 * \param  The module's object
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     busDrv_Create
 */ 
TI_STATUS busDrv_Destroy (TI_HANDLE hBusDrv)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;

    if (pBusDrv)
    {
        os_memoryFree (pBusDrv->hOs, pBusDrv, sizeof(TBusDrvObj));     
    }
    return TI_OK;
}


/** 
 * \fn     busDrv_Init
 * \brief  Init bus driver 
 * 
 * Init module parameters.

 * \note   
 * \param  hBusDrv - The module's handle
 * \param  hReport - report module handle
 * \return void
 * \sa     
 */ 
void busDrv_Init (TI_HANDLE hBusDrv, TI_HANDLE hReport)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*) hBusDrv;

    pBusDrv->hReport = hReport;
}


/** 
 * \fn     busDrv_ConnectBus
 * \brief  Configure bus driver
 * 
 * Called by TxnQ.
 * Configure the bus driver with its connection configuration (such as baud-rate, bus width etc) 
 *     and establish the physical connection. 
 * Done once upon init (and not per functional driver startup). 
 * 
 * \note   
 * \param  hBusDrv    - The module's object
 * \param  pBusDrvCfg - A union used for per-bus specific configuration. 
 * \param  fCbFunc    - CB function for Async transaction completion (after all txn parts are completed).
 * \param  hCbArg     - The CB function handle
 * \param  fConnectCbFunc - The CB function for the connect bus competion (if returned Pending)
 * \param  pRxDmaBufLen - The Rx DMA buffer length in bytes (needed as a limit of the Tx/Rx aggregation length)
 * \param  pTxDmaBufLen - The Tx DMA buffer length in bytes (needed as a limit of the Tx/Rx aggregation length)
 * \return TI_OK / TI_NOK
 * \sa     
 */ 
TI_STATUS busDrv_ConnectBus (TI_HANDLE        hBusDrv, 
                             TBusDrvCfg       *pBusDrvCfg,
                             TBusDrvTxnDoneCb fCbFunc,
                             TI_HANDLE        hCbArg,
                             TBusDrvTxnDoneCb fConnectCbFunc,
                             TI_UINT32        *pRxDmaBufLen,
                             TI_UINT32        *pTxDmaBufLen)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    int         iStatus;

    /* Save the parameters (TxnQ callback for TxnDone events, and block-size) */
    pBusDrv->fTxnDoneCb    = fCbFunc;
    pBusDrv->hCbHandle     = hCbArg;
    pBusDrv->uBlkSizeShift = pBusDrvCfg->tSdioCfg.uBlkSizeShift;
    pBusDrv->uBlkSize      = 1 << pBusDrv->uBlkSizeShift;
    pBusDrv->uBlkSizeMask  = pBusDrv->uBlkSize - 1;
    /* This should cover stop send Txn parts in recovery */
    pBusDrv->uCurrTxnPartsCount = 0;
    pBusDrv->uCurrTxnPartsNum = 0;
    pBusDrv->uCurrTxnPartsCountSync = 0;
    pBusDrv->uTxnLength = 0;
	
    /*
     * Configure the SDIO driver parameters and handle SDIO enumeration.
     *
     * Note: The DMA-able buffer address to use for write transactions is provided from the 
     *           SDIO driver into pBusDrv->pDmaBuffer.
     */

    iStatus = sdioAdapt_ConnectBus ((void *)busDrv_TxnDoneCb, 
                                    hBusDrv, 
                                    pBusDrv->uBlkSizeShift, 
                                    pBusDrvCfg->tSdioCfg.uBusDrvThreadPriority,
                                    &pBusDrv->pRxDmaBuf,
                                    &pBusDrv->uRxDmaBufLen,
                                    &pBusDrv->pTxDmaBuf,
                                    &pBusDrv->uTxDmaBufLen);

    *pRxDmaBufLen = pBusDrv->uRxDmaBufLen;
    *pTxDmaBufLen = pBusDrv->uTxDmaBufLen;

    if ((pBusDrv->pRxDmaBuf == NULL) || (pBusDrv->pTxDmaBuf == NULL))
    {
        TRACE0(pBusDrv->hReport, REPORT_SEVERITY_ERROR, "busDrv_ConnectBus: Didn't get DMA buffer from SDIO driver!!");
        return TI_NOK;
    }

    if (iStatus == 0) 
    {
        return TI_OK;
    }
    else 
    {
        TRACE2(pBusDrv->hReport, REPORT_SEVERITY_ERROR, "busDrv_ConnectBus: Status = 0x%x, BlkSize = %d\n", iStatus, pBusDrv->uBlkSize);
        return TI_NOK;
    }
}


/** 
 * \fn     busDrv_DisconnectBus
 * \brief  Disconnect SDIO driver
 * 
 * Called by TxnQ. Disconnect the SDIO driver.
 *  
 * \note   
 * \param  hBusDrv - The module's object
 * \return TI_OK / TI_NOK
 * \sa     
 */ 
TI_STATUS busDrv_DisconnectBus (TI_HANDLE hBusDrv)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;

    TRACE0(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_DisconnectBus()\n");

    /* Disconnect SDIO driver */
    return sdioAdapt_DisconnectBus ();
}


/** 
 * \fn     busDrv_Transact
 * \brief  Process transaction 
 * 
 * Called by the TxnQ module to initiate a new transaction.
 * Prepare the transaction parts (lower layer single transactions),
 *      and send them one by one to the lower layer.
 * 
 * \note   It's assumed that this function is called only when idle (i.e. previous Txn is done).
 * \param  hBusDrv - The module's object
 * \param  pTxn    - The transaction object 
 * \return COMPLETE if Txn completed in this context, PENDING if not, ERROR if failed
 * \sa     busDrv_PrepareTxnParts, busDrv_SendTxnParts
 */ 
ETxnStatus busDrv_Transact (TI_HANDLE hBusDrv, TTxnStruct *pTxn)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    TI_BOOL     bWithinAggregation;
    CL_TRACE_START_L4();

    pBusDrv->pCurrTxn               = pTxn;
    pBusDrv->uCurrTxnPartsCount     = 0;
    pBusDrv->uCurrTxnPartsCountSync = 0;

    /* Prepare the transaction parts in a table. */
    bWithinAggregation = busDrv_PrepareTxnParts (pBusDrv, pTxn);

    /* If in the middle of Tx aggregation, return Complete (current Txn was coppied to buffer but not sent) */
    if (bWithinAggregation)
    {
        TRACE1(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_Transact: In aggregation so exit, uTxnLength=%d\n", pBusDrv->uTxnLength);
        CL_TRACE_END_L4("tiwlan_drv.ko", "INHERIT", "TXN", ".Transact");
        return TXN_STATUS_COMPLETE;
    }

    /* Send the prepared transaction parts. */
    busDrv_SendTxnParts (pBusDrv);

    TRACE1(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_Transact: Status = %d\n", pBusDrv->eCurrTxnStatus);

    CL_TRACE_END_L4("tiwlan_drv.ko", "INHERIT", "TXN", ".Transact");

    /* return transaction status - COMPLETE, PENDING or ERROR */
    /* The status is updated in busDrv_SendTxnParts(). It is Async (pending) if not completed in this context */
    return pBusDrv->eCurrTxnStatus;
}


/** 
 * \fn     busDrv_PrepareTxnParts
 * \brief  Prepare write or read transaction parts
 * 
 * Called by busDrv_Transact().
 * Prepares the actual sequence of SDIO bus transactions in a table.
 * Use a DMA-able buffer for the bus transaction, so all data is copied
 *     to it from the host buffer(s) before write transactions,
 *     or copied from it to the host buffers after read transactions.
 * 
 * \note   
 * \param  pBusDrv - The module's object
 * \param  pTxn    - The transaction object 
 * \return TRUE if we are in the middle of a Tx aggregation
 * \sa     busDrv_Transact, busDrv_SendTxnParts, 
 */ 
static TI_BOOL busDrv_PrepareTxnParts (TBusDrvObj *pBusDrv, TTxnStruct *pTxn)
{
    TI_UINT32 uPartNum     = 0;
    TI_UINT32 uCurrHwAddr  = pTxn->uHwAddr;
    TI_BOOL   bFixedHwAddr = TXN_PARAM_GET_FIXED_ADDR(pTxn);
    TI_BOOL   bWrite       = (TXN_PARAM_GET_DIRECTION(pTxn) == TXN_DIRECTION_WRITE) ? TI_TRUE : TI_FALSE;
    TI_UINT8 *pHostBuf     = bWrite ? pBusDrv->pTxDmaBuf : pBusDrv->pRxDmaBuf; /* Use DMA buffer (Rx or Tx) for actual transaction */
    TI_UINT32 uBufNum;
    TI_UINT32 uBufLen;
    TI_UINT32 uRemainderLen;

    /* Go over the transaction buffers */
    for (uBufNum = 0; uBufNum < MAX_XFER_BUFS; uBufNum++) 
    {
        uBufLen = pTxn->aLen[uBufNum];

        /* If no more buffers, exit the loop */
        if (uBufLen == 0)
        {
            break;
        }

        /* For write transaction, copy the data to the DMA buffer */
        if (bWrite) 
        {
            os_memoryCopy (pBusDrv->hOs, pHostBuf + pBusDrv->uTxnLength, pTxn->aBuf[uBufNum], uBufLen);
        }

        /* Add buffer length to total transaction length */
        pBusDrv->uTxnLength += uBufLen;
    }

    /* If in a Tx aggregation, return TRUE (need to accumulate all parts before sending the transaction) */
    if (TXN_PARAM_GET_AGGREGATE(pTxn) == TXN_AGGREGATE_ON)
    {
        TRACE6(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_PrepareTxnParts: In aggregation so exit, uTxnLength=%d, bWrite=%d, Len0=%d, Len1=%d, Len2=%d, Len3=%d\n", pBusDrv->uTxnLength, bWrite, pTxn->aLen[0], pTxn->aLen[1], pTxn->aLen[2], pTxn->aLen[3]);
        return TI_TRUE;
    }

    /* If current buffer has a remainder, prepare its transaction part */
    uRemainderLen = pBusDrv->uTxnLength & pBusDrv->uBlkSizeMask;
    if (uRemainderLen > 0)
    {
        pBusDrv->aTxnParts[uPartNum].bBlkMode  = TI_FALSE;
        pBusDrv->aTxnParts[uPartNum].uLength   = uRemainderLen;
        pBusDrv->aTxnParts[uPartNum].uHwAddr   = uCurrHwAddr;
        pBusDrv->aTxnParts[uPartNum].pHostAddr = (void *)pHostBuf;
        pBusDrv->aTxnParts[uPartNum].bMore     = TI_TRUE;

        /* If not fixed HW address, increment it by this part's size */
        if (!bFixedHwAddr)
        {
            uCurrHwAddr += uRemainderLen;
        }

        uPartNum++;
    }

#ifdef  DISABLE_SDIO_MULTI_BLK_MODE

    /* SDIO multi-block mode is disabled so split to 512 bytes blocks */
    {
        TI_UINT32 uLen;

        for (uLen = uRemainderLen; uLen < pBusDrv->uTxnLength; uLen += pBusDrv->uBlkSize)
        {
            pBusDrv->aTxnParts[uPartNum].bBlkMode  = TI_FALSE;
            pBusDrv->aTxnParts[uPartNum].uLength   = pBusDrv->uBlkSize;
            pBusDrv->aTxnParts[uPartNum].uHwAddr   = uCurrHwAddr;
            pBusDrv->aTxnParts[uPartNum].pHostAddr = (void *)(pHostBuf + uLen);
            pBusDrv->aTxnParts[uPartNum].bMore     = TI_TRUE;

            /* If not fixed HW address, increment it by this part's size */
            if (!bFixedHwAddr)
            {
                uCurrHwAddr += pBusDrv->uBlkSize;
            }

            uPartNum++;
        }
    }

#else  /* Use SDIO block mode (this is the default behavior) */

    /* If current buffer has full SDIO blocks, prepare a block-mode transaction part */
    if (pBusDrv->uTxnLength >= pBusDrv->uBlkSize)
    {
        pBusDrv->aTxnParts[uPartNum].bBlkMode  = TI_TRUE;
        pBusDrv->aTxnParts[uPartNum].uLength   = pBusDrv->uTxnLength - uRemainderLen;
        pBusDrv->aTxnParts[uPartNum].uHwAddr   = uCurrHwAddr;
        pBusDrv->aTxnParts[uPartNum].pHostAddr = (void *)(pHostBuf + uRemainderLen);
        pBusDrv->aTxnParts[uPartNum].bMore     = TI_TRUE;

        uPartNum++;
    }

#endif /* DISABLE_SDIO_MULTI_BLK_MODE */

    /* Set last More flag as specified for the whole Txn */
    pBusDrv->aTxnParts[uPartNum - 1].bMore = TXN_PARAM_GET_MORE(pTxn);
    pBusDrv->uCurrTxnPartsNum = uPartNum;

    TRACE9(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_PrepareTxnParts: Txn prepared, PartsNum=%d, bWrite=%d, uTxnLength=%d, uRemainderLen=%d, uHwAddr=0x%x, Len0=%d, Len1=%d, Len2=%d, Len3=%d\n", uPartNum, bWrite, pBusDrv->uTxnLength, uRemainderLen, pTxn->uHwAddr, pTxn->aLen[0], pTxn->aLen[1], pTxn->aLen[2], pTxn->aLen[3]);

    pBusDrv->uTxnLength = 0;

    /* Return FALSE to indicate that we are not in the middle of a Tx aggregation so the Txn is ready to send */
    return TI_FALSE;
}


/** 
 * \fn     busDrv_SendTxnParts
 * \brief  Send prepared transaction parts
 * 
 * Called first by busDrv_Transact(), and also from TxnDone CB after Async completion.
 * Sends the prepared transaction parts in a loop.
 * If a transaction part is Async, the loop continues later in the TxnDone ISR context.
 * When all parts are done, the upper layer TxnDone CB is called.
 * 
 * \note   
 * \param  pBusDrv - The module's object
 * \return void
 * \sa     busDrv_Transact, busDrv_PrepareTxnParts
 */ 
static void busDrv_SendTxnParts (TBusDrvObj *pBusDrv)
{
    ETxnStatus  eStatus;
    TTxnPart   *pTxnPart;
    TTxnStruct *pTxn = pBusDrv->pCurrTxn;

    /* While there are transaction parts to send */
    while (pBusDrv->uCurrTxnPartsCount < pBusDrv->uCurrTxnPartsNum)
    {
        pTxnPart = &(pBusDrv->aTxnParts[pBusDrv->uCurrTxnPartsCount]);
        pBusDrv->uCurrTxnPartsCount++;

        /* Assume pending to be ready in case we are preempted by the TxnDon CB !! */
        pBusDrv->eCurrTxnStatus = TXN_STATUS_PENDING;   

        /* If single step, send ELP byte */
        if (TXN_PARAM_GET_SINGLE_STEP(pTxn)) 
        {
            /* Overwrite the function id with function 0 - for ELP register !!!! */
            eStatus = sdioAdapt_TransactBytes (TXN_FUNC_ID_CTRL,
                                               pTxnPart->uHwAddr,
                                               pTxnPart->pHostAddr,
                                               pTxnPart->uLength,
                                               TXN_PARAM_GET_DIRECTION(pTxn),
                                               pTxnPart->bMore);

            /* If first write failed try once again (may happen once upon chip wakeup) */
            if (eStatus == TXN_STATUS_ERROR)
            {
                /* Overwrite the function id with function 0 - for ELP register !!!! */
                eStatus = sdioAdapt_TransactBytes (TXN_FUNC_ID_CTRL,
                                                   pTxnPart->uHwAddr,
                                                   pTxnPart->pHostAddr,
                                                   pTxnPart->uLength,
                                                   TXN_PARAM_GET_DIRECTION(pTxn),
                                                   pTxnPart->bMore);
                TRACE0(pBusDrv->hReport, REPORT_SEVERITY_WARNING, "busDrv_SendTxnParts: SDIO Single-Step transaction failed once so try again");
            }
        }
        else
        {
            eStatus = sdioAdapt_Transact (TXN_PARAM_GET_FUNC_ID(pTxn),
                                          pTxnPart->uHwAddr,
                                          pTxnPart->pHostAddr,
                                          pTxnPart->uLength,
                                          TXN_PARAM_GET_DIRECTION(pTxn),
                                          pTxnPart->bBlkMode,
                                          ((TXN_PARAM_GET_FIXED_ADDR(pTxn) == 1) ? 0 : 1),
                                          pTxnPart->bMore);
        }

        TRACE7(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_SendTxnParts: PartNum = %d, SingleStep = %d, Direction = %d, HwAddr = 0x%x, HostAddr = 0x%x, Length = %d, BlkMode = %d\n", pBusDrv->uCurrTxnPartsCount-1, TXN_PARAM_GET_SINGLE_STEP(pTxn), TXN_PARAM_GET_DIRECTION(pTxn), pTxnPart->uHwAddr, pTxnPart->pHostAddr, pTxnPart->uLength, pTxnPart->bBlkMode);

        /* If pending TxnDone (Async), continue this loop in the next TxnDone interrupt */
        if (eStatus == TXN_STATUS_PENDING)
        {
            return; 
        }

        /* Update current transaction status to deduce if it is all finished in the original context (Sync) or not. */
        pBusDrv->eCurrTxnStatus = eStatus;
        pBusDrv->uCurrTxnPartsCountSync++;

        /* If error, set error in Txn struct, call TxnDone CB if not fully sync, and exit */
        if (eStatus == TXN_STATUS_ERROR)
        {
            TXN_PARAM_SET_STATUS(pTxn, TXN_PARAM_STATUS_ERROR);
            if (pBusDrv->uCurrTxnPartsCountSync != pBusDrv->uCurrTxnPartsCount)
            {
                pBusDrv->fTxnDoneCb (pBusDrv->hCbHandle, pTxn);
            }
        	return;
        }
    }

    /* If we got here we sent all buffers and we don't pend transaction end */
    TRACE3(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_SendTxnParts: Txn finished successfully, Status = %d, PartsCount = %d, SyncCount = %d\n", pBusDrv->eCurrTxnStatus, pBusDrv->uCurrTxnPartsCount, pBusDrv->uCurrTxnPartsCountSync);

    /* For read transaction, copy the data from the DMA-able buffer to the host buffer(s) */
    if (TXN_PARAM_GET_DIRECTION(pTxn) == TXN_DIRECTION_READ)
    {
        TI_UINT32 uBufNum;
        TI_UINT32 uBufLen;
        TI_UINT8 *pDmaBuf = pBusDrv->pRxDmaBuf; /* After the read transaction the data is in the Rx DMA buffer */

        for (uBufNum = 0; uBufNum < MAX_XFER_BUFS; uBufNum++)
        {
            uBufLen = pTxn->aLen[uBufNum];

            /* If no more buffers, exit the loop */
            if (uBufLen == 0)
            {
                break;
            }

            os_memoryCopy (pBusDrv->hOs, pTxn->aBuf[uBufNum], pDmaBuf, uBufLen);
            pDmaBuf += uBufLen;
        }
    }

    /* Set status OK in Txn struct, and call TxnDone CB if not fully sync */
    TXN_PARAM_SET_STATUS(pTxn, TXN_PARAM_STATUS_OK);
    if (pBusDrv->uCurrTxnPartsCountSync != pBusDrv->uCurrTxnPartsCount)
    {
        pBusDrv->fTxnDoneCb (pBusDrv->hCbHandle, pTxn);
    }
}


/** 
 * \fn     busDrv_TxnDoneCb
 * \brief  Continue async transaction processing (CB)
 * 
 * Called back by the lower (BSP) bus-driver upon Async transaction completion (TxnDone ISR).
 * Call busDrv_SendTxnParts to continue sending the remained transaction parts.
 * 
 * \note   
 * \param  hBusDrv - The module's object
 * \param  status  - The last transaction result - 0 = OK, else Error
 * \return void
 * \sa     busDrv_SendTxnParts
 */ 
static void busDrv_TxnDoneCb (TI_HANDLE hBusDrv, int iStatus)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    CL_TRACE_START_L1();

    /* If last transaction part failed, set error in Txn struct, call TxnDone CB and exit. */
    if (iStatus != 0)
    {
        TRACE1(pBusDrv->hReport, REPORT_SEVERITY_ERROR, "busDrv_TxnDoneCb: Status = 0x%x\n", iStatus);

        TXN_PARAM_SET_STATUS(pBusDrv->pCurrTxn, TXN_PARAM_STATUS_ERROR);
        pBusDrv->fTxnDoneCb (pBusDrv->hCbHandle, pBusDrv->pCurrTxn);
        CL_TRACE_END_L1("tiwlan_drv.ko", "TXN_DONE", "BusDrvCB", "");
        return;
    }

    TRACE0(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_TxnDoneCb()\n");

    /* Continue sending the remained transaction parts. */
    busDrv_SendTxnParts (pBusDrv);

    CL_TRACE_END_L1("tiwlan_drv.ko", "TXN_DONE", "BusDrvCB", "");
}
