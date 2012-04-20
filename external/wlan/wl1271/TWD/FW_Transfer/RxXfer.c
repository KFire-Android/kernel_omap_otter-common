/*
 * RxXfer.c
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


/****************************************************************************
 *
 *   MODULE:  rxXfer.c
 *
 *   PURPOSE: Rx Xfer module implementation.Responsible for reading Rx from the FW
 *              and forward it to the upper layers.
 * 
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_106
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "rxXfer_api.h"
#include "FwEvent_api.h"
#include "TWDriverInternal.h"
#include "RxQueue_api.h"
#include "TwIf.h"
#include "public_host_int.h"
#include "bmtrace_api.h"

#define RX_DRIVER_COUNTER_ADDRESS 0x300538
#define PLCP_HEADER_LENGTH 8
#define WORD_SIZE   4
#define UNALIGNED_PAYLOAD   0x1
#define RX_DESCRIPTOR_SIZE          (sizeof(RxIfDescriptor_t))
#define MAX_PACKETS_NUMBER          8
#define MAX_CONSECUTIVE_READ_TXN    16
#define MAX_PACKET_SIZE             8192    /* Max Txn size */

#ifdef PLATFORM_SYMBIAN	/* UMAC is using only one buffer and therefore we can't use consecutive reads */
    #define MAX_CONSECUTIVE_READS   1
#else
    #define MAX_CONSECUTIVE_READS   8
#endif

#define SLV_MEM_CP_VALUE(desc, offset)  (((RX_DESC_GET_MEM_BLK(desc) << 8) + offset))
#define ALIGNMENT_SIZE(desc)            ((RX_DESC_GET_UNALIGNED(desc) & UNALIGNED_PAYLOAD) ? 2 : 0)

#if (NUM_RX_PKT_DESC & (NUM_RX_PKT_DESC - 1))
    #error  NUM_RX_PKT_DESC is not a power of 2 which may degrade performance when we calculate modulo!!
#endif


#ifdef TI_DBG
typedef struct
{
    TI_UINT32           uCountFwEvents;
    TI_UINT32           uCountPktsForward;
    TI_UINT32           uCountBufPend;
    TI_UINT32           uCountBufNoMem;
    TI_UINT32           uCountPktAggreg[MAX_XFER_BUFS];

} TRxXferDbgStat;
#endif

typedef struct
{
    TTxnStruct          tTxnStruct;
    TI_UINT32           uRegData;
    TI_UINT32           uRegAdata;

} TRegTxn;

typedef struct
{
    TTxnStruct          tTxnStruct;
    TI_UINT32           uCounter;

} TCounterTxn;

typedef struct
{
    TI_HANDLE           hOs;
    TI_HANDLE           hReport;
    TI_HANDLE           hTwIf;
    TI_HANDLE           hFwEvent;
    TI_HANDLE           hRxQueue;

    TI_UINT32           aRxPktsDesc[NUM_RX_PKT_DESC];           /* Save Rx packets short descriptors from FwStatus */
    TI_UINT32           uFwRxCntr;                              /* Save last FW packets counter from FwStatus */
    TI_UINT32           uDrvRxCntr;                             /* The current driver processed packets counter */
    TI_UINT32           uPacketMemoryPoolStart;                 /* The FW mem-blocks area base address */
    TI_UINT32           uMaxAggregLen;                          /* The max length in bytes of aggregated packets transaction */
    TI_UINT32           uMaxAggregPkts;                         /* The max number of packets that may be aggregated in one transaction */
    TRequestForBufferCb RequestForBufferCB;                     /* Upper layer CB for allocating buffers for packets */
    TI_HANDLE           RequestForBufferCB_handle;              /* The upper later CB handle */
    TI_BOOL             bPendingBuffer;                         /* If TRUE, we exited the Rx handler upon pending-buffer */

    TI_UINT32           uCurrTxnIndex;                          /* The current Txn structures index to use */
    TI_UINT32           uAvailableTxn;                          /* Number of Txn structures currently available */
    TRegTxn             aSlaveRegTxn[MAX_CONSECUTIVE_READ_TXN]; /* Txn structures for writing mem-block address reg */
    TTxnStruct          aTxnStruct[MAX_CONSECUTIVE_READ_TXN];   /* Txn structures for reading the Rx packets */
    TCounterTxn         aCounterTxn[MAX_CONSECUTIVE_READ_TXN];  /* Txn structures for writing the driver counter workaround */

    TI_UINT8            aTempBuffer[MAX_PACKET_SIZE];           /* Dummy buffer to use if we couldn't get a buffer for the packet (so drop the packet) */
    TFailureEventCb     fErrCb;                                 /* The upper layer CB function for error handling */
    TI_HANDLE           hErrCb;                                 /* The CB function handle */

#ifdef TI_DBG
    TRxXferDbgStat      tDbgStat;
#endif

} TRxXfer;


/************************ static function declaration *****************************/
static TI_STATUS rxXfer_Handle(TI_HANDLE hRxXfer);
static void rxXfer_TxnDoneCb (TI_HANDLE hRxXfer, TTxnStruct* pTxn);
static void         rxXfer_PktDropTxnDoneCb (TI_HANDLE hRxXfer, TTxnStruct *pTxn);
static ETxnStatus   rxXfer_IssueTxn (TI_HANDLE hRxXfer, TI_UINT32 uFirstMemBlkAddr);
static void         rxXfer_ForwardPacket (TRxXfer* pRxXfer, TTxnStruct* pTxn);


/****************************************************************************
 *                      RxXfer_Create()
 ****************************************************************************
 * DESCRIPTION: Create the RxXfer module object 
 * 
 * INPUTS:  None
 * 
 * OUTPUT:  None
 * 
 * RETURNS: The Created object
 ****************************************************************************/
TI_HANDLE rxXfer_Create (TI_HANDLE hOs)
{
    TRxXfer *pRxXfer;

    pRxXfer = os_memoryAlloc (hOs, sizeof(TRxXfer));
    if (pRxXfer == NULL)
        return NULL;

    /* For all the counters */
    os_memoryZero (hOs, pRxXfer, sizeof(TRxXfer));

    pRxXfer->hOs = hOs;

    return (TI_HANDLE)pRxXfer;
}


/****************************************************************************
 *                      RxXfer_Destroy()
 ****************************************************************************
 * DESCRIPTION: Destroy the RxXfer module object 
 * 
 * INPUTS:  hRxXfer - The object to free
 * 
 * OUTPUT:  None
 * 
 * RETURNS: 
 ****************************************************************************/
void rxXfer_Destroy (TI_HANDLE hRxXfer)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    if (pRxXfer)
    {
        os_memoryFree (pRxXfer->hOs, pRxXfer, sizeof(TRxXfer));
    }
}


/****************************************************************************
 *                      rxXfer_init()
 ****************************************************************************
 * DESCRIPTION: Init the module object
 * 
 * INPUTS:      hRxXfer - module handle;
 *              other modules handles.
 * 
 * OUTPUT:  None
 * 
 * RETURNS: None
 ****************************************************************************/
void rxXfer_Init(TI_HANDLE hRxXfer,
                 TI_HANDLE hFwEvent, 
                 TI_HANDLE hReport,
                 TI_HANDLE hTwIf,
                 TI_HANDLE hRxQueue)
{
    TRxXfer *pRxXfer        = (TRxXfer *)hRxXfer;
    pRxXfer->hFwEvent       = hFwEvent;
    pRxXfer->hReport        = hReport;
    pRxXfer->hTwIf          = hTwIf;
    pRxXfer->hRxQueue       = hRxQueue;

    rxXfer_Restart (hRxXfer);

#ifdef TI_DBG   
    rxXfer_ClearStats (pRxXfer);
#endif
}


/****************************************************************************
 *                      rxXfer_SetDefaults()
 ****************************************************************************
 * DESCRIPTION: Set module parameters default setting
 *
 * INPUTS:      hRxXfer - module handle;
 *
 * OUTPUT:  None
 *
 * RETURNS: None
 ****************************************************************************/
void rxXfer_SetDefaults (TI_HANDLE hRxXfer, TTwdInitParams *pInitParams)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    pRxXfer->uMaxAggregPkts = pInitParams->tGeneral.uRxAggregPktsLimit;
}


/****************************************************************************
 *                      rxXfer_SetBusParams()
 ****************************************************************************
 * DESCRIPTION: Configure bus driver DMA-able buffer length to be used as a limit to the aggragation length.
 * 
 * INPUTS:      hRxXfer    - module handle
 *              uDmaBufLen - The bus driver DMA-able buffer length
 * 
 * OUTPUT:  None
 * 
 * RETURNS: None
 ****************************************************************************/
void rxXfer_SetBusParams (TI_HANDLE hRxXfer, TI_UINT32 uDmaBufLen)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    pRxXfer->uMaxAggregLen = uDmaBufLen;
}


/****************************************************************************
 *                      rxXfer_Register_CB()
 ****************************************************************************
 * DESCRIPTION: Register the function to be called for request for buffer.
 * 
 * INPUTS:      hRxXfer       - RxXfer handle;
 * 
 * OUTPUT:  None
 * 
 * RETURNS: None
 ****************************************************************************/
void rxXfer_Register_CB (TI_HANDLE hRxXfer, TI_UINT32 CallBackID, void *CBFunc, TI_HANDLE CBObj)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    TRACE1(pRxXfer->hReport, REPORT_SEVERITY_INFORMATION , "rxXfer_Register_CB (Value = 0x%x)\n", CallBackID);

    switch(CallBackID)
    {
    case TWD_INT_REQUEST_FOR_BUFFER:       
        pRxXfer->RequestForBufferCB = (TRequestForBufferCb)CBFunc;
        pRxXfer->RequestForBufferCB_handle = CBObj;
        break;

    default:
        TRACE0(pRxXfer->hReport, REPORT_SEVERITY_ERROR, "rxXfer_Register_CB - Illegal value\n");
        return;
    }
}


/****************************************************************************
 *                      rxXfer_ForwardPacket()
 ****************************************************************************
 * DESCRIPTION:  Forward received packet(s) to the upper layers.
 *
 * INPUTS:      
 * 
 * OUTPUT:      
 * 
 * RETURNS:     
 ****************************************************************************/
static void rxXfer_ForwardPacket (TRxXfer *pRxXfer, TTxnStruct *pTxn)
{
    TI_UINT32 uBufNum;
    RxIfDescriptor_t *pRxInfo  = (RxIfDescriptor_t*)(pTxn->aBuf[0]);
#ifdef TI_DBG   /* for packet sanity check */
    TI_UINT16        uLenFromRxInfo;
#endif

    /* Go over all occupied Txn buffers and forward their Rx packets upward */
    for (uBufNum = 0; uBufNum < MAX_XFER_BUFS; uBufNum++)
    {
        /* If no more buffers, exit the loop */
        if (pTxn->aLen[uBufNum] == 0)
        {
            break;
        }

#ifdef TI_DBG   /* Packet sanity check */
        /* Get length from RxInfo, handle endianess and convert to length in bytes */
        pRxInfo = (RxIfDescriptor_t*)(pTxn->aBuf[uBufNum]);
        uLenFromRxInfo = ENDIAN_HANDLE_WORD(pRxInfo->length) << 2;

        /* If the length in the RxInfo is different than in the short descriptor, set error status */
        if (pTxn->aLen[uBufNum] != uLenFromRxInfo)
        {
            TRACE3(pRxXfer->hReport, REPORT_SEVERITY_ERROR , "rxXfer_ForwardPacket: Bad Length!! RxInfoLength=%d, ShortDescLen=%d, RxInfoStatus=0x%x\n", uLenFromRxInfo, pTxn->aLen[uBufNum], pRxInfo->status);

            pRxInfo->status &= ~RX_DESC_STATUS_MASK;
            pRxInfo->status |= RX_DESC_STATUS_DRIVER_RX_Q_FAIL;
            pRxInfo->length = ENDIAN_HANDLE_WORD(pTxn->aLen[uBufNum] >> 2);

            /* If error CB available, trigger recovery !! */
            if (pRxXfer->fErrCb)
            {
                pRxXfer->fErrCb (pRxXfer->hErrCb, RX_XFER_FAILURE);
            }
        }
        else 
        {
            TRACE2(pRxXfer->hReport, REPORT_SEVERITY_INFORMATION , "rxXfer_ForwardPacket: RxInfoLength=%d, RxInfoStatus=0x%x\n", uLenFromRxInfo, pRxInfo->status);
        }
        pRxXfer->tDbgStat.uCountPktsForward++;
#endif

        /* This is the last packet in the Burst so mark its EndOfBurst flag */
        if (TXN_PARAM_GET_END_OF_BURST(pTxn) && (uBufNum == (MAX_XFER_BUFS - 1) || pTxn->aLen[uBufNum + 1] == 0))
        {
            TXN_PARAM_SET_END_OF_BURST(pTxn, 0);
            pRxInfo->driverFlags |= DRV_RX_FLAG_END_OF_BURST;
        }
        /* Forward received packet to the upper layers */
        RxQueue_ReceivePacket (pRxXfer->hRxQueue, (const void *)pTxn->aBuf[uBufNum]);
    }

    /* reset the aBuf field for clean on recovery purpose */
    pTxn->aBuf[0] = 0;
}


/****************************************************************************
 *                      rxXfer_RxEvent()
 ****************************************************************************
 * DESCRIPTION: Called upon Rx event from the FW.calls the SM  
 * 
 * INPUTS:      hRxXfer       - RxXfer handle;
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TWIF_OK in case of Synch mode, or TWIF_PENDING in case of Asynch mode
 *          (when returning TWIF_PENDING, FwEvent module expects the FwEvent_EventComplete()
 *          function call to finish the Rx Client handling 
 *
 ****************************************************************************/
ETxnStatus rxXfer_RxEvent (TI_HANDLE hRxXfer, FwStatus_t *pFwStatus)
{
    TRxXfer        *pRxXfer = (TRxXfer *)hRxXfer;
    TI_UINT32      uTempCounters;
    FwStatCntrs_t  *pFwStatusCounters;
    TI_UINT32       i;
    TI_STATUS   rc;
    CL_TRACE_START_L2();
  
    uTempCounters = ENDIAN_HANDLE_LONG (pFwStatus->counters);
    pFwStatusCounters = (FwStatCntrs_t*)(&uTempCounters);

    TRACE2(pRxXfer->hReport, REPORT_SEVERITY_INFORMATION , "rxXfer_RxEvent: NewFwCntr=%d, OldFwCntr=%d\n", pFwStatusCounters->fwRxCntr, pRxXfer->uFwRxCntr);

    /* If no new Rx packets - exit */
    if ((pFwStatusCounters->fwRxCntr % NUM_RX_PKT_DESC) == (pRxXfer->uFwRxCntr % NUM_RX_PKT_DESC))
    {
        CL_TRACE_END_L2("tiwlan_drv.ko", "CONTEXT", "RX", "");
        return TXN_STATUS_COMPLETE;
    }

#ifdef TI_DBG
    pRxXfer->tDbgStat.uCountFwEvents++;
#endif

    /* Save current FW counter and Rx packets short descriptors for processing */
    pRxXfer->uFwRxCntr = pFwStatusCounters->fwRxCntr;
    for (i = 0; i < NUM_RX_PKT_DESC; i++)
    {
        pRxXfer->aRxPktsDesc[i] = ENDIAN_HANDLE_LONG (pFwStatus->rxPktsDesc[i]); 
    }

    /* Handle all new Rx packets */
    rc = rxXfer_Handle (pRxXfer);

    CL_TRACE_END_L2("tiwlan_drv.ko", "CONTEXT", "RX", "");
    return TXN_STATUS_COMPLETE;
}


/****************************************************************************
 *                      rxXfer_Handle()
 ****************************************************************************
 * DESCRIPTION: 
 *
 * INPUTS:      hRxXfer       - RxXfer handle;
 * 
 * OUTPUT:      
 * 
 * RETURNS:     
 ****************************************************************************/
static TI_STATUS rxXfer_Handle(TI_HANDLE hRxXfer)
{
#ifndef _VLCT_
    TRxXfer *        pRxXfer          = (TRxXfer *)hRxXfer;
    TI_BOOL          bIssueTxn        = TI_FALSE; /* If TRUE transact current aggregated packets */
    TI_BOOL          bDropLastPkt     = TI_FALSE; /* If TRUE, need to drop last packet (RX_BUF_ALLOC_OUT_OF_MEM) */
    TI_BOOL          bExit            = TI_FALSE; /* If TRUE, can't process further packets so exit (after serving the other flags) */
    TI_UINT32        uAggregPktsNum   = 0;        /* Number of aggregated packets */
    TI_UINT32        uFirstMemBlkAddr = 0;
    TI_UINT32        uRxDesc          = 0;
    TI_UINT32        uBuffSize        = 0;
    TI_UINT32        uTotalAggregLen  = 0;
    TI_UINT32        uDrvIndex;
    TI_UINT32        uFwIndex;
    TI_UINT8 *       pHostBuf;
    TTxnStruct *     pTxn = NULL;
    ETxnStatus       eTxnStatus;
    ERxBufferStatus  eBufStatus;
    PacketClassTag_e eRxPacketType;
    CL_TRACE_START_L2();


    /* If no Txn structures available exit!! (fatal error - not expected to happen) */
    if (pRxXfer->uAvailableTxn == 0 )
    {
        TRACE0(pRxXfer->hReport, REPORT_SEVERITY_ERROR, "rxXfer_Handle: No available Txn structures left!\n");
        CL_TRACE_END_L2("tiwlan_drv.ko", "CONTEXT", "RX", "");
        return TI_NOK;
    }

    uFwIndex = pRxXfer->uFwRxCntr % NUM_RX_PKT_DESC;

    /* Loop while Rx packets can be transfered from the FW */
    while (1)
    {
        uDrvIndex = pRxXfer->uDrvRxCntr % NUM_RX_PKT_DESC;

        /* If there are unprocessed Rx packets */
        if (uDrvIndex != uFwIndex)
        {
            /* Get next packte info */
            uRxDesc       = pRxXfer->aRxPktsDesc[uDrvIndex];
            uBuffSize     = RX_DESC_GET_LENGTH(uRxDesc) << 2;
            eRxPacketType = (PacketClassTag_e)RX_DESC_GET_PACKET_CLASS_TAG (uRxDesc);

            /* If new packet exceeds max aggregation length, set flag to send previous packets (postpone it to next loop) */
            if ((uTotalAggregLen + uBuffSize) > pRxXfer->uMaxAggregLen)
            {
                bIssueTxn = TI_TRUE;
            }

            /* No length limit so try to aggregate new packet */
            else
            {
                /* Allocate host read buffer */
                /* The RxBufAlloc() add an extra word for MAC header alignment in case of QoS MSDU */
                eBufStatus = pRxXfer->RequestForBufferCB(pRxXfer->RequestForBufferCB_handle,
                                                         (void**)&pHostBuf,
                                                         uBuffSize,
                                                         (TI_UINT32)NULL,
                                                         eRxPacketType);

                TRACE6(pRxXfer->hReport, REPORT_SEVERITY_INFORMATION , "rxXfer_Handle: Index=%d, RxDesc=0x%x, DrvCntr=%d, FwCntr=%d, BufStatus=%d, BuffSize=%d\n", uDrvIndex, uRxDesc, pRxXfer->uDrvRxCntr, pRxXfer->uFwRxCntr, eBufStatus, uBuffSize);

                /* If buffer allocated, add it to current Txn (up to 4 packets aggregation) */
                if (eBufStatus == RX_BUF_ALLOC_COMPLETE)
                {
                    /* If first aggregated packet prepare the next Txn struct */
                    if (uAggregPktsNum == 0)
                    {
                        pTxn = (TTxnStruct*)&(pRxXfer->aTxnStruct[pRxXfer->uCurrTxnIndex]);
                        pTxn->uHwAddr = SLV_MEM_DATA;

                        /* Save first mem-block of first aggregated packet! */
                        uFirstMemBlkAddr = SLV_MEM_CP_VALUE(uRxDesc, pRxXfer->uPacketMemoryPoolStart);
                    }
                    pTxn->aBuf[uAggregPktsNum] = pHostBuf + ALIGNMENT_SIZE(uRxDesc);
                    pTxn->aLen[uAggregPktsNum] = uBuffSize;
                    uAggregPktsNum++;
                    uTotalAggregLen += uBuffSize;
                    if (uAggregPktsNum >= pRxXfer->uMaxAggregPkts)
                    {
                        bIssueTxn = TI_TRUE;
                    }
                    pRxXfer->uDrvRxCntr++;
                }

                /* If buffer pending until freeing previous buffer, set Exit flag and if needed set IssueTxn flag. */
                else if (eBufStatus == RX_BUF_ALLOC_PENDING)
                {
                    bExit = TI_TRUE;
                    pRxXfer->bPendingBuffer = TI_TRUE;
                    if (uAggregPktsNum > 0)
                    {
                        bIssueTxn = TI_TRUE;
                    }
#ifdef TI_DBG
                    pRxXfer->tDbgStat.uCountBufPend++;
#endif
                }

                /* If no buffer due to out-of-memory, set DropLastPkt flag and if needed set IssueTxn flag. */
                else
                {
                    bDropLastPkt = TI_TRUE;
                    if (uAggregPktsNum > 0)
                    {
                        bIssueTxn = TI_TRUE;
                    }
#ifdef TI_DBG
                    pRxXfer->tDbgStat.uCountBufNoMem++;
#endif
                }
            }
        }

        /* If no more packets, set Exit flag and if needed set IssueTxn flag. */
        else
        {
            bExit = TI_TRUE;
            if (uAggregPktsNum > 0)
            {
                bIssueTxn = TI_TRUE;
            }
        }


        /* If required to send Rx packet(s) transaction */
        if (bIssueTxn)
        {
            if (bExit)
            {
                TXN_PARAM_SET_END_OF_BURST(pTxn, 1);
            }
            /* If not all 4 Txn buffers are used, reset first unused buffer length for indication */
            if (uAggregPktsNum < MAX_XFER_BUFS)
            {
                pTxn->aLen[uAggregPktsNum] = 0;
            }

            eTxnStatus = rxXfer_IssueTxn (pRxXfer, uFirstMemBlkAddr);

            if (eTxnStatus == TXN_STATUS_COMPLETE)
            {
                /* Forward received packet to the upper layers */
                rxXfer_ForwardPacket (pRxXfer, pTxn);
            }
            else if (eTxnStatus == TXN_STATUS_PENDING)
            {
                /* Decrease the number of available txn structures */
                pRxXfer->uAvailableTxn--;
            }
            else
            {
                TRACE3(pRxXfer->hReport, REPORT_SEVERITY_ERROR , "rxXfer_Handle: Status=%d, DrvCntr=%d, RxDesc=0x%x\n", eTxnStatus, pRxXfer->uDrvRxCntr, uRxDesc);
            }

#ifdef TI_DBG
            pRxXfer->tDbgStat.uCountPktAggreg[uAggregPktsNum - 1]++;
#endif

            uAggregPktsNum  = 0;
            uTotalAggregLen = 0;
            bIssueTxn       = TI_FALSE;
            pRxXfer->uCurrTxnIndex = (pRxXfer->uCurrTxnIndex + 1) % MAX_CONSECUTIVE_READ_TXN;
        }

        /* If last packet should be dropped (no memory for host buffer) */
        if (bDropLastPkt)
        {
            /* Increment driver packets counter before calling rxXfer_IssueTxn() */
            pRxXfer->uDrvRxCntr++;

            /* Read packet to dummy buffer and ignore it (no callback needed) */
            uFirstMemBlkAddr = SLV_MEM_CP_VALUE(uRxDesc, pRxXfer->uPacketMemoryPoolStart);
            pTxn = (TTxnStruct*)&pRxXfer->aTxnStruct[pRxXfer->uCurrTxnIndex];
            BUILD_TTxnStruct(pTxn, SLV_MEM_DATA, pRxXfer->aTempBuffer, uBuffSize, (TTxnDoneCb)rxXfer_PktDropTxnDoneCb, hRxXfer)
            eTxnStatus = rxXfer_IssueTxn (pRxXfer, uFirstMemBlkAddr);
            if (eTxnStatus == TXN_STATUS_PENDING)
            {
                pRxXfer->uAvailableTxn--;
            }
            pRxXfer->uCurrTxnIndex = (pRxXfer->uCurrTxnIndex + 1) % MAX_CONSECUTIVE_READ_TXN;
            bDropLastPkt = TI_FALSE;
        }

        /* Can't process more packets so exit */
        if (bExit)
        {
            CL_TRACE_END_L2("tiwlan_drv.ko", "CONTEXT", "RX", "");
            return TI_OK;
        }

    } /* End of while(1) */

    /* Unreachable code */

#endif
}


/****************************************************************************
 *                      rxXfer_IssueTxn()
 ****************************************************************************
 * DESCRIPTION:
 *
 * INPUTS:
 *
 * OUTPUT:
 *
 * RETURNS:
 ****************************************************************************/
static ETxnStatus rxXfer_IssueTxn (TI_HANDLE hRxXfer, TI_UINT32 uFirstMemBlkAddr)
{
    TRxXfer    *pRxXfer = (TRxXfer *)hRxXfer;
    TI_UINT32   uIndex  = pRxXfer->uCurrTxnIndex;
    TTxnStruct *pTxn;
    ETxnStatus  eStatus;

    /* Write the next mem block that we want to read */
    pTxn = &pRxXfer->aSlaveRegTxn[uIndex].tTxnStruct;
    pTxn->uHwAddr = SLV_REG_DATA;
    pRxXfer->aSlaveRegTxn[uIndex].uRegData  = ENDIAN_HANDLE_LONG(uFirstMemBlkAddr);
    pRxXfer->aSlaveRegTxn[uIndex].uRegAdata = ENDIAN_HANDLE_LONG(uFirstMemBlkAddr + 4);
    twIf_Transact(pRxXfer->hTwIf, pTxn);

    /* Issue the packet(s) read transaction (prepared in rxXfer_Handle) */
    pTxn = &pRxXfer->aTxnStruct[uIndex];
    eStatus = twIf_Transact(pRxXfer->hTwIf, pTxn);

    /* Write driver packets counter to FW. This write automatically generates interrupt to FW */
    /* Note: Workaround for WL6-PG1.0 is still needed for PG2.0   ==>  if (pRxXfer->bChipIs1273Pg10)  */
    pTxn = &pRxXfer->aCounterTxn[uIndex].tTxnStruct;
    pTxn->uHwAddr = RX_DRIVER_COUNTER_ADDRESS;
    pRxXfer->aCounterTxn[uIndex].uCounter = ENDIAN_HANDLE_LONG(pRxXfer->uDrvRxCntr);
    twIf_Transact(pRxXfer->hTwIf, pTxn);

    TRACE5(pRxXfer->hReport, REPORT_SEVERITY_INFORMATION , "rxXfer_IssueTxn: Counter-Txn: HwAddr=0x%x, Len0=%d, Data0=%d, DrvCount=%d, TxnParams=0x%x\n", pTxn->uHwAddr, pTxn->aLen[0], *(TI_UINT32 *)(pTxn->aBuf[0]), pRxXfer->uDrvRxCntr, pTxn->uTxnParams);

    /* Return the status of the packet(s) transaction - COMPLETE, PENDING or ERROR */
    return eStatus;
}
  

/****************************************************************************
 *                      rxXfer_SetRxDirectAccessParams()
 ****************************************************************************
 * DESCRIPTION: 
 *
 * INPUTS:      
 * 
 * OUTPUT:      
 * 
 * RETURNS:     
 ****************************************************************************/
void rxXfer_SetRxDirectAccessParams (TI_HANDLE hRxXfer, TDmaParams *pDmaParams)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    pRxXfer->uPacketMemoryPoolStart = pDmaParams->PacketMemoryPoolStart;
}


/****************************************************************************
 *                      rxXfer_TxnDoneCb()
 ****************************************************************************
 * DESCRIPTION: Forward the packet to the registered CB
 *
 * INPUTS:      
 * 
 * OUTPUT:      
 * 
 * RETURNS:     
 ****************************************************************************/
static void rxXfer_TxnDoneCb (TI_HANDLE hRxXfer, TTxnStruct *pTxn)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;
    CL_TRACE_START_L2();
    
    /* Increase the number of available txn structures */
    pRxXfer->uAvailableTxn++;

    /* Forward received packet to the upper layers */
    rxXfer_ForwardPacket (pRxXfer, pTxn);

    /* If we exited the handler upon pending-buffer, call it again to handle further packets if any */
    if (pRxXfer->bPendingBuffer)
    {
        pRxXfer->bPendingBuffer = TI_FALSE;
        rxXfer_Handle (hRxXfer);
    }

    CL_TRACE_END_L2("tiwlan_drv.ko", "INHERIT", "RX", "");
}


/****************************************************************************
 *                      rxXfer_PktDropTxnDoneCb()
 ****************************************************************************
 * DESCRIPTION: Dummy CB for case of dropping a packet due to out-of-memory.
 *
 * INPUTS:
 *
 * OUTPUT:
 *
 * RETURNS:
 ****************************************************************************/
static void rxXfer_PktDropTxnDoneCb (TI_HANDLE hRxXfer, TTxnStruct *pTxn)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    /* Increase the number of available txn structures */
    pRxXfer->uAvailableTxn++;

    /* Restore the regular TxnDone callback to the used structure */
    pTxn->fTxnDoneCb = (TTxnDoneCb)rxXfer_TxnDoneCb;
    pTxn->hCbHandle  = hRxXfer;
}


/****************************************************************************
 *                      rxXfer_Restart()
 ****************************************************************************
 * DESCRIPTION:	rxXfer_Restart the RxXfer module object (called by the recovery)
 * 
 * INPUTS:	hRxXfer - The object to free
 * 
 * OUTPUT:	None
 * 
 * RETURNS:	NONE 
 ****************************************************************************/
void rxXfer_Restart(TI_HANDLE hRxXfer)
{
	TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;
    TTxnStruct* pTxn;
    TI_UINT8    i;

    pRxXfer->uFwRxCntr     = 0;
    pRxXfer->uDrvRxCntr    = 0;
    pRxXfer->uCurrTxnIndex = 0;
    pRxXfer->uAvailableTxn = MAX_CONSECUTIVE_READ_TXN - 1;

    /* Scan all transaction array and release only pending transaction */
    for (i = 0; i < MAX_CONSECUTIVE_READ_TXN; i++)
    {
        pTxn = &(pRxXfer->aTxnStruct[i]);

        /* Check if buffer allocated and not the dummy one (has a different callback) */
        if ((pTxn->aBuf[0] != 0) && (pTxn->fTxnDoneCb == (TTxnDoneCb)rxXfer_TxnDoneCb))
        {
            TI_UINT32 uBufNum;
            RxIfDescriptor_t *pRxParams;

            /* Go over the Txn occupied  buffers and mark them as TAG_CLASS_UNKNOWN to be freed */
            for (uBufNum = 0; uBufNum < MAX_XFER_BUFS; uBufNum++)
            {
                /* If no more buffers, exit the loop */
                if (pTxn->aLen[uBufNum] == 0)
                {
                    break;
                }

                pRxParams = (RxIfDescriptor_t *)(pTxn->aBuf[uBufNum]);
                pRxParams->packet_class_tag = TAG_CLASS_UNKNOWN;
            }

            /* Call upper layer only to release the allocated buffer */
            rxXfer_ForwardPacket (pRxXfer, pTxn);
        }
    }

    /* Fill the transaction structures fields that have constant values */
    for (i = 0; i < MAX_CONSECUTIVE_READ_TXN; i++)
    {
        /* First mem-block address (two consecutive registers) */
        pTxn = &(pRxXfer->aSlaveRegTxn[i].tTxnStruct);
        TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
        BUILD_TTxnStruct(pTxn, SLV_REG_DATA, &pRxXfer->aSlaveRegTxn[i].uRegData, REGISTER_SIZE*2, NULL, NULL)

        /* The packet(s) read transaction */
        pTxn = &(pRxXfer->aTxnStruct[i]);
        TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_READ, TXN_FIXED_ADDR)
        pTxn->fTxnDoneCb = (TTxnDoneCb)rxXfer_TxnDoneCb;
        pTxn->hCbHandle  = hRxXfer;

        /* The driver packets counter */
        pTxn = &(pRxXfer->aCounterTxn[i].tTxnStruct);
        TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
        BUILD_TTxnStruct(pTxn, RX_DRIVER_COUNTER_ADDRESS, &pRxXfer->aCounterTxn[i].uCounter, REGISTER_SIZE, NULL, NULL)
    }
	
}


/****************************************************************************
 *                      rxXfer_RegisterErrCb()
 ****************************************************************************
 * DESCRIPTION: Register Error CB
 *
 * INPUTS:
 *          hRxXfer - The object
 *          ErrCb   - The upper layer CB function for error handling
 *          hErrCb  - The CB function handle
 *
 * OUTPUT:  None
 *
 * RETURNS: void
 ****************************************************************************/
void rxXfer_RegisterErrCb (TI_HANDLE hRxXfer, void *fErrCb, TI_HANDLE hErrCb)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    /* Save upper layer (health monitor) CB for recovery from fatal error */
    pRxXfer->fErrCb = (TFailureEventCb)fErrCb;
    pRxXfer->hErrCb = hErrCb;
}


#ifdef TI_DBG
/****************************************************************************
 *                      rxXfer_ClearStats()
 ****************************************************************************
 * DESCRIPTION: 
 *
 * INPUTS:  
 *          pRxXfer The object
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK. 
 ****************************************************************************/
void rxXfer_ClearStats (TI_HANDLE hRxXfer)
{
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;

    os_memoryZero (pRxXfer->hOs, &pRxXfer->tDbgStat, sizeof(TRxXferDbgStat));
}


/****************************************************************************
 *                      rxXfer_PrintStats()
 ****************************************************************************
 * DESCRIPTION: .
 *
 * INPUTS:  
 *          pRxXfer The object
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK. 
 ****************************************************************************/
void rxXfer_PrintStats (TI_HANDLE hRxXfer)
{
#ifdef REPORT_LOG
    TRxXfer *pRxXfer = (TRxXfer *)hRxXfer;
    
    WLAN_OS_REPORT(("Print RX Xfer module info\n"));
    WLAN_OS_REPORT(("=========================\n"));
    WLAN_OS_REPORT(("uMaxAggregPkts     = %d\n", pRxXfer->uMaxAggregPkts));
    WLAN_OS_REPORT(("uMaxAggregLen      = %d\n", pRxXfer->uMaxAggregLen));
    WLAN_OS_REPORT(("FW counter         = %d\n", pRxXfer->uFwRxCntr));
    WLAN_OS_REPORT(("Drv counter        = %d\n", pRxXfer->uDrvRxCntr));
    WLAN_OS_REPORT(("AvailableTxn       = %d\n", pRxXfer->uAvailableTxn));
    WLAN_OS_REPORT(("uCountFwEvents     = %d\n", pRxXfer->tDbgStat.uCountFwEvents));
    WLAN_OS_REPORT(("uCountPktsForward  = %d\n", pRxXfer->tDbgStat.uCountPktsForward));
    WLAN_OS_REPORT(("uCountBufPend      = %d\n", pRxXfer->tDbgStat.uCountBufPend));
    WLAN_OS_REPORT(("uCountBufNoMem     = %d\n", pRxXfer->tDbgStat.uCountBufNoMem));
    WLAN_OS_REPORT(("uCountPktAggreg-1  = %d\n", pRxXfer->tDbgStat.uCountPktAggreg[0]));
    WLAN_OS_REPORT(("uCountPktAggreg-2  = %d\n", pRxXfer->tDbgStat.uCountPktAggreg[1]));
    WLAN_OS_REPORT(("uCountPktAggreg-3  = %d\n", pRxXfer->tDbgStat.uCountPktAggreg[2]));
    WLAN_OS_REPORT(("uCountPktAggreg-4  = %d\n", pRxXfer->tDbgStat.uCountPktAggreg[3]));
#endif
}
#endif
