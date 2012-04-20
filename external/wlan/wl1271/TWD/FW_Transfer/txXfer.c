/*
 * txXfer.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
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


/** \file   txXfer.c 
 *  \brief  Handle Tx packets transfer to the firmware. 
 *
 *  This module gets the upper driver's Tx packets after FW resources were
 *    allocated for them, aggregates them if possible, and handles their transfer 
 *    to the FW via the host slave (indirect) interface, using the TwIf Transaction API.
 *  The aggregation processing is completed by the BusDrv where the packets are combined
 *    and sent to the FW in one transaction.
 * 
 *  \see    
 */


#define __FILE_ID__  FILE_ID_108
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "TwIf.h"
#include "TWDriver.h"
#include "txXfer_api.h"


#ifdef TI_DBG
#define     DBG_MAX_AGGREG_PKTS     16
#endif

typedef struct 
{
    TTxnStruct              tTxnStruct;
    TI_UINT32               uPktsCntr; 
} TPktsCntrTxn;

/* The TxXfer module object. */
typedef struct 
{
    TI_HANDLE               hOs;
    TI_HANDLE               hReport;
    TI_HANDLE               hTwIf;

    TI_UINT32               uAggregMaxPkts;          /* Max number of packets that may be aggregated */
    TI_UINT32               uAggregMaxLen;           /* Max length in bytes of a single aggregation */ 
    TI_UINT32               uAggregPktsNum;          /* Number of packets in current aggregation */
    TI_UINT32               uAggregPktsLen;          /* Aggregated length of current aggregation */ 
    TTxCtrlBlk *            pAggregFirstPkt;         /* Pointer to the first packet of current aggregation */
    TTxCtrlBlk *            pAggregLastPkt;          /* Pointer to the last packet of current aggregation */
    TSendPacketTranferCb    fSendPacketTransferCb;   /* Upper layer Xfer-Complete callback */
    TI_HANDLE               hSendPacketTransferHndl; /* Upper layer Xfer-Complete callback handle */
    TTxnDoneCb              fXferCompleteLocalCb;    /* TxXfer local CB for pkt transfer completion (NULL is not needed!) */
    TI_UINT32               uPktsCntr;               /* Counts all Tx packets. Written to FW after each packet transaction */
    TI_UINT32               uPktsCntrTxnIndex;       /* The current indext of the aPktsCntrTxn[] used for the counter workaround transactions */
    TPktsCntrTxn            aPktsCntrTxn[CTRL_BLK_ENTRIES_NUM]; /* Transaction structures for sending the packets counter */
#ifdef TI_DBG
    TI_UINT32               aDbgCountPktAggreg[DBG_MAX_AGGREG_PKTS];
#endif

} TTxXferObj;

static ETxnStatus txXfer_SendAggregatedPkts (TTxXferObj *pTxXfer, TI_BOOL bLastPktSentNow);
static void       txXfer_TransferDoneCb     (TI_HANDLE hTxXfer, TTxnStruct *pTxn);


/********************************************************************************
*																				*
*                       PUBLIC  FUNCTIONS  IMPLEMENTATION						*
*																				*
*********************************************************************************/


TI_HANDLE txXfer_Create(TI_HANDLE hOs)
{
    TTxXferObj *pTxXfer;

    pTxXfer = os_memoryAlloc (hOs, sizeof(TTxXferObj));
    if (pTxXfer == NULL)
    {
        return NULL;
    }

    os_memoryZero (hOs, pTxXfer, sizeof(TTxXferObj));

    pTxXfer->hOs = hOs;

    return (TI_HANDLE)pTxXfer;
}


TI_STATUS txXfer_Destroy(TI_HANDLE hTxXfer)
{
    TTxXferObj *pTxXfer = (TTxXferObj *)hTxXfer;

    if (pTxXfer)
    {
        os_memoryFree (pTxXfer->hOs, pTxXfer, sizeof(TTxXferObj));
    }

    return TI_OK;
}


TI_STATUS txXfer_Init (TI_HANDLE hTxXfer, TI_HANDLE hReport, TI_HANDLE hTwIf)
{
    TTxXferObj *pTxXfer = (TTxXferObj *)hTxXfer;
    TTxnStruct *pTxn;
    TI_UINT8    i;

    pTxXfer->hReport = hReport;
    pTxXfer->hTwIf   = hTwIf;
    pTxXfer->fSendPacketTransferCb = NULL;
    pTxXfer->fXferCompleteLocalCb  = NULL;

    for (i = 0; i < CTRL_BLK_ENTRIES_NUM; i++)
    {
        pTxn = &(pTxXfer->aPktsCntrTxn[i].tTxnStruct);
        TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
        BUILD_TTxnStruct(pTxn, HOST_WR_ACCESS_REG, &pTxXfer->aPktsCntrTxn[i].uPktsCntr, REGISTER_SIZE, NULL, NULL)
    }

    return txXfer_Restart(hTxXfer);
}


TI_STATUS txXfer_Restart (TI_HANDLE hTxXfer)
{
    TTxXferObj *pTxXfer = (TTxXferObj *)hTxXfer;

    pTxXfer->uPktsCntr         = 0;
    pTxXfer->uPktsCntrTxnIndex = 0;
    pTxXfer->uAggregPktsNum    = 0;

    return TI_OK;
}


void txXfer_SetDefaults (TI_HANDLE hTxXfer, TTwdInitParams *pInitParams)
{
    TTxXferObj *pTxXfer = (TTxXferObj *)hTxXfer;

    pTxXfer->uAggregMaxPkts = pInitParams->tGeneral.uTxAggregPktsLimit;
}


void txXfer_SetBusParams (TI_HANDLE hTxXfer, TI_UINT32 uDmaBufLen)
{
    TTxXferObj *pTxXfer = (TTxXferObj *)hTxXfer;

    pTxXfer->uAggregMaxLen = uDmaBufLen;
}


void txXfer_RegisterCb (TI_HANDLE hTxXfer, TI_UINT32 CallBackID, void *CBFunc, TI_HANDLE CBObj)
{
    TTxXferObj* pTxXfer = (TTxXferObj*)hTxXfer;

    TRACE3(pTxXfer->hReport, REPORT_SEVERITY_INFORMATION, "txXfer_RegisterCb: CallBackID=%d, CBFunc=0x%x, CBObj=0x%x\n", CallBackID, CBFunc, CBObj);

    switch(CallBackID)
    {
        /* Save upper layers Transfer-Done callback */
        case TWD_INT_SEND_PACKET_TRANSFER:
            pTxXfer->fSendPacketTransferCb   = (TSendPacketTranferCb)CBFunc;
            pTxXfer->hSendPacketTransferHndl = CBObj;
            /* Set also the local CB so we are called upon Async transaction completion to call the upper CB */
            pTxXfer->fXferCompleteLocalCb = (TTxnDoneCb)txXfer_TransferDoneCb;
            break;

        default:
            TRACE0(pTxXfer->hReport, REPORT_SEVERITY_ERROR, " - Illegal value\n");
            break;
    }
}


ETxnStatus txXfer_SendPacket (TI_HANDLE hTxXfer, TTxCtrlBlk *pPktCtrlBlk)
{
    TTxXferObj   *pTxXfer = (TTxXferObj *)hTxXfer;
    TI_UINT32    uPktLen  = ENDIAN_HANDLE_WORD(pPktCtrlBlk->tTxDescriptor.length << 2); /* swap back for endianess if needed */
    ETxnStatus   eStatus; 

    /* If starting a new aggregation, prepare it, and send packet if aggregation is disabled. */
    if (pTxXfer->uAggregPktsNum == 0)
    {
        pTxXfer->uAggregPktsNum  = 1;
        pTxXfer->uAggregPktsLen  = uPktLen;
        pTxXfer->pAggregFirstPkt = pPktCtrlBlk;
        pTxXfer->pAggregLastPkt  = pPktCtrlBlk;
        pPktCtrlBlk->pNextAggregEntry = pPktCtrlBlk;  /* First packet points to itself */
        if (pTxXfer->uAggregMaxPkts <= 1)
        {
            eStatus = txXfer_SendAggregatedPkts (pTxXfer, TI_TRUE);
            pTxXfer->uAggregPktsNum = 0;
        }
        else 
        {
            eStatus = TXN_STATUS_PENDING;
        }
    }

    /* Else, if new packet can be added to aggregation, add it and set status as Pending. */
    else if ((pTxXfer->uAggregPktsNum + 1 <= pTxXfer->uAggregMaxPkts)  && 
             (pTxXfer->uAggregPktsLen + uPktLen <= pTxXfer->uAggregMaxLen))
    {
        pTxXfer->uAggregPktsNum++;
        pTxXfer->uAggregPktsLen += uPktLen;
        pTxXfer->pAggregLastPkt->pNextAggregEntry = pPktCtrlBlk;  /* Link new packet to last */
        pTxXfer->pAggregLastPkt = pPktCtrlBlk;                    /* Save new packet as last */
        pPktCtrlBlk->pNextAggregEntry = pTxXfer->pAggregFirstPkt; /* Point from last to first */
        eStatus = TXN_STATUS_PENDING;
    }

    /* Else, we can't add the new packet, so send current aggregation and start a new one */
    else 
    {
        txXfer_SendAggregatedPkts (pTxXfer, TI_FALSE);
        eStatus = TXN_STATUS_PENDING;  /* The current packet is not sent yet so return Pending */
        pTxXfer->uAggregPktsNum  = 1;
        pTxXfer->uAggregPktsLen  = uPktLen;
        pTxXfer->pAggregFirstPkt = pPktCtrlBlk;
        pTxXfer->pAggregLastPkt  = pPktCtrlBlk;
        pPktCtrlBlk->pNextAggregEntry = pPktCtrlBlk;  /* First packet points to itself */
    }


    /* Return the Txn result - COMPLETE or PENDING. */
    /* Note: For PENDING, a callback function will be called only if registered (needed for WHA) */
    return eStatus;
}


void txXfer_EndOfBurst (TI_HANDLE hTxXfer)
{
    TTxXferObj   *pTxXfer = (TTxXferObj *)hTxXfer;

    if (pTxXfer->uAggregPktsNum > 0) 
    {
        /* No more packets from TxDataQ so send any aggregated packets and clear aggregation */
        txXfer_SendAggregatedPkts (pTxXfer, TI_FALSE);
        pTxXfer->uAggregPktsNum = 0;
    }
}


/********************************************************************************
*																				*
*                       INTERNAL  FUNCTIONS  IMPLEMENTATION						*
*																				*
*********************************************************************************/

/** 
 * \fn     txXfer_SendAggregatedPkts
 * \brief  Send aggregated Tx packets to bus Txn layer
 * 
 * Send aggregated Tx packets to bus Txn layer one by one.
 * Increase the packets counter by the number of packets and send it to the FW (generates an interrupt).
 * If xfer completion CB is registered and status is Complete, call CB for all packets (except last one if inseted now).
 * 
 * \note   The BusDrv combines the packets and sends them in one transaction.
 * \param  pTxXfer         - The module's object
 * \param  bLastPktSentNow - If TRUE, last packet in the aggregation was inserted in current call to txXfer_SendPacket.
 * \return COMPLETE if transaction completed in this context, PENDING if not, ERROR if failed
 * \sa     
 */ 
static ETxnStatus txXfer_SendAggregatedPkts (TTxXferObj *pTxXfer, TI_BOOL bLastPktSentNow)
{
    TTxCtrlBlk   *pCurrPkt;
    TTxnStruct   *pTxn;
    TPktsCntrTxn *pPktsCntrTxn; 
    ETxnStatus   eStatus = TXN_STATUS_COMPLETE; 
    TI_UINT32    i;

    /* Prepare and send all aggregated packets (combined and sent in one transaction by the BusDrv) */
    pCurrPkt = pTxXfer->pAggregFirstPkt;
    for (i = 0; i < pTxXfer->uAggregPktsNum; i++)
    {
        pTxn = (TTxnStruct *)pCurrPkt;

        /* If not last packet, set aggregation flag, clear completion CB and progress to next packet */
        if (i < pTxXfer->uAggregPktsNum - 1)
        {
            TXN_PARAM_SET_AGGREGATE(pTxn, TXN_AGGREGATE_ON);
            pTxn->fTxnDoneCb = NULL;
            pCurrPkt = pCurrPkt->pNextAggregEntry;
        }
        /* If last packet, clear aggregation flag and set completion CB (exist only if registered) */
        else 
        {
            TXN_PARAM_SET_AGGREGATE(pTxn, TXN_AGGREGATE_OFF);
            pTxn->fTxnDoneCb = pTxXfer->fXferCompleteLocalCb;
            pTxn->hCbHandle  = (TI_HANDLE)pTxXfer;
        }

        /* Send packet */
        pTxn->uHwAddr = SLV_MEM_DATA;
        eStatus = twIf_Transact (pTxXfer->hTwIf, pTxn);
    }

#ifdef TI_DBG
    pTxXfer->aDbgCountPktAggreg[pTxXfer->uAggregPktsNum]++;
    TRACE5(pTxXfer->hReport, REPORT_SEVERITY_INFORMATION, "txXfer_SendAggregatedPkts: Status=%d, NumPkts=%d, AggregLen=%d, pFirstPkt=0x%x, pLastPkt=0x%x\n", eStatus, pTxXfer->uAggregPktsNum, pTxXfer->uAggregPktsLen, pTxXfer->pAggregFirstPkt, pTxXfer->pAggregLastPkt);
    if (eStatus == TXN_STATUS_ERROR)
    {
        TRACE5(pTxXfer->hReport, REPORT_SEVERITY_ERROR, "txXfer_SendAggregatedPkts: Status=%d, NumPkts=%d, AggregLen=%d, pFirstPkt=0x%x, pLastPkt=0x%x\n", eStatus, pTxXfer->uAggregPktsNum, pTxXfer->uAggregPktsLen, pTxXfer->pAggregFirstPkt, pTxXfer->pAggregLastPkt);
        return eStatus;
    }
#endif  /* TI_DBG */

    /* Write packet counter to FW (generates an interrupt). 
       Note: This may be removed once the host-slave HW counter functionality is verified */
    pTxXfer->uPktsCntr += pTxXfer->uAggregPktsNum;
    pTxXfer->uPktsCntrTxnIndex++;
    if (pTxXfer->uPktsCntrTxnIndex == CTRL_BLK_ENTRIES_NUM) 
    {
        pTxXfer->uPktsCntrTxnIndex = 0;
    }
    pPktsCntrTxn = &(pTxXfer->aPktsCntrTxn[pTxXfer->uPktsCntrTxnIndex]);
    pPktsCntrTxn->uPktsCntr = ENDIAN_HANDLE_LONG(pTxXfer->uPktsCntr);
    pPktsCntrTxn->tTxnStruct.uHwAddr = HOST_WR_ACCESS_REG;
    twIf_Transact(pTxXfer->hTwIf, &pPktsCntrTxn->tTxnStruct);

    /* If xfer completion CB is registered and last packet status is Complete, call the CB for all 
     *     packets except the input one (covered by the return code). 
     */
    if (pTxXfer->fSendPacketTransferCb && (eStatus == TXN_STATUS_COMPLETE)) 
    {
        /* Don't call CB for last packet if inserted in current Tx */
        TI_UINT32 uNumCbCalls = bLastPktSentNow ? (pTxXfer->uAggregPktsNum - 1) : pTxXfer->uAggregPktsNum;

        pCurrPkt = pTxXfer->pAggregFirstPkt;
        for (i = 0; i < uNumCbCalls; i++)
        {
            pTxXfer->fSendPacketTransferCb (pTxXfer->hSendPacketTransferHndl, pCurrPkt);
            pCurrPkt = pCurrPkt->pNextAggregEntry;
        }
    }

    /* Return the Txn result - COMPLETE or PENDING. */
    /* Note: For PENDING, a callback function will be called only if registered (needed for WHA) */
    return eStatus;
}


/** 
 * \fn     txXfer_TransferDoneCb
 * \brief  Send aggregated Tx packets to bus Txn layer
 * 
 * Call the upper layers TranferDone CB for all packets of the completed aggregation
 * This function is called only if the upper layers registered their CB (used only by WHA)
 * 
 * \note   
 * \param  pTxXfer - The module's object
 * \return COMPLETE if completed in this context, PENDING if not, ERROR if failed
 * \sa     
 */ 
static void txXfer_TransferDoneCb (TI_HANDLE hTxXfer, TTxnStruct *pTxn)
{
    TTxXferObj *pTxXfer   = (TTxXferObj*)hTxXfer;
    TTxCtrlBlk *pInputPkt = (TTxCtrlBlk *)pTxn; /* This is the last packet of the aggregation */
    TTxCtrlBlk *pCurrPkt;
    TI_UINT32   i;

    /* Call the upper layers TranferDone CB for all packets of the completed aggregation */
    /* Note: If this CB was called it means that the upper CB exists */
    pCurrPkt = pInputPkt->pNextAggregEntry;  /* The last packet of the aggregation point to the first one */
    for (i = 0; i < pTxXfer->uAggregMaxPkts; i++)
    {
        pTxXfer->fSendPacketTransferCb (pTxXfer->hSendPacketTransferHndl, pCurrPkt);

        /* If we got back to the input packet we went over all the aggregation */
        if (pCurrPkt == pInputPkt)
        {
            break;   
        }

        pCurrPkt = pCurrPkt->pNextAggregEntry;
    }

    TRACE3(pTxXfer->hReport, REPORT_SEVERITY_INFORMATION, "txXfer_TransferDoneCb: NumPkts=%d, pInputPkt=0x%x, pCurrPkt=0x%x\n", i + 1, pInputPkt, pCurrPkt);
}


#ifdef TI_DBG

void txXfer_ClearStats (TI_HANDLE hTxXfer)
{
    TTxXferObj *pTxXfer = (TTxXferObj*)hTxXfer;

    os_memoryZero (pTxXfer->hOs, &pTxXfer->aDbgCountPktAggreg, sizeof(pTxXfer->aDbgCountPktAggreg));
}

void txXfer_PrintStats (TI_HANDLE hTxXfer)
{
#ifdef REPORT_LOG
    TTxXferObj *pTxXfer = (TTxXferObj*)hTxXfer;
    TI_UINT32   i;
    
    WLAN_OS_REPORT(("Print Tx Xfer module info\n"));
    WLAN_OS_REPORT(("=========================\n"));
    WLAN_OS_REPORT(("uAggregMaxPkts     = %d\n", pTxXfer->uAggregMaxPkts));
    WLAN_OS_REPORT(("uAggregMaxLen      = %d\n", pTxXfer->uAggregMaxLen));
    WLAN_OS_REPORT(("uAggregPktsNum     = %d\n", pTxXfer->uAggregPktsNum));
    WLAN_OS_REPORT(("uAggregPktsLen     = %d\n", pTxXfer->uAggregPktsLen));
    WLAN_OS_REPORT(("uPktsCntr          = %d\n", pTxXfer->uPktsCntr));
    WLAN_OS_REPORT(("uPktsCntrTxnIndex  = %d\n", pTxXfer->uPktsCntrTxnIndex));
    for (i = 1; i < DBG_MAX_AGGREG_PKTS; i++)
    {
        WLAN_OS_REPORT(("uCountPktAggreg-%2d = %d\n", i, pTxXfer->aDbgCountPktAggreg[i]));
    }
#endif
}

#endif /* TI_DBG */
