/*
 * txCtrl.c
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

/*******************************************************************************/
/*                                                                             */
/*      MODULE: txCtrl.c                                                       */
/*    PURPOSE:  The central Tx path module.                                    */
/*              Prepares Tx packets sent from the data-queue and mgmt-queue    */
/*                for copy to the FW, including building the header and the    */
/*                Tx-descriptor.                                               */
/*                                                                             */
/*******************************************************************************/
#define __FILE_ID__  FILE_ID_56
#include "tidef.h"
#include "paramOut.h"
#include "osApi.h"
#include "TWDriver.h"
#include "DataCtrl_Api.h"
#include "802_11Defs.h"
#include "Ethernet.h"
#include "report.h"
#include "timer.h"
#include "TI_IPC_Api.h"
#include "EvHandler.h"
#include "qosMngr_API.h"
#include "healthMonitor.h"
#include "txCtrl.h"
#include "txCtrl_Api.h"
#include "DrvMainModules.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif
#include "bmtrace_api.h"


/* 
 * Module internal functions prototypes:
 */

/* Note: put here and not in txCtrl.h to avoid warning in the txCtrl submodules that include txCtrl.h */ 
 
static void   txCtrl_TxCompleteCb (TI_HANDLE hTxCtrl, TxResultDescriptor_t *pTxResultInfo);
static void   txCtrl_BuildDataPkt (txCtrl_t *pTxCtrl, TTxCtrlBlk *pPktCtrlBlk,
                                   TI_UINT32 uAc, TI_UINT32 uBackpressure);
static void	  txCtrl_BuildMgmtPkt (txCtrl_t *pTxCtrl, TTxCtrlBlk *pPktCtrlBlk, TI_UINT32 uAc);
static void   txCtrl_UpdateHighestAdmittedAcTable (txCtrl_t *pTxCtrl);
static void   txCtrl_UpdateAcToTidMapping (txCtrl_t *pTxCtrl);
static void   txCtrl_UpdateBackpressure (txCtrl_t *pTxCtrl, TI_UINT32 freedAcBitmap);
static void   txCtrl_UpdateTxCounters (txCtrl_t *pTxCtrl, 
                                       TxResultDescriptor_t *pTxResultInfo,
                                       TTxCtrlBlk *pPktCtrlBlk, 
                                       TI_UINT32 ac, 
                                       TI_BOOL bIsDataPkt);
#ifdef XCC_MODULE_INCLUDED  /* Needed only for XCC-V4 */
static void   txCtrl_SetTxDelayCounters (txCtrl_t *pTxCtrl, 
                                         TI_UINT32 ac, 
                                         TI_UINT32 fwDelay, 
                                         TI_UINT32 driverDelay, 
                                         TI_UINT32 mediumDelay);
#endif /* XCC_MODULE_INCLUDED */


/********************************************************************************
*																				*
*                       MACROS and INLINE FUNCTIONS           					*
*																				*
*********************************************************************************/
/* Get the highest admitted AC equal or below the requested one. */
/* AC to TID translation is bivalent so update TID only if the AC was changed. */
#define SELECT_AC_FOR_TID(ptc,tid,ac)                      \
	ac = ptc->highestAdmittedAc[WMEQosTagToACTable[tid]];  \
    if (ac != WMEQosTagToACTable[tid])                     \
		tid = WMEQosAcToTid[ac]

/* Update packet length in the descriptor according to HW interface requirements */
static inline TI_UINT16 txCtrl_TranslateLengthToFw (TTxCtrlBlk *pPktCtrlBlk)
{
    TI_UINT16 uPktLen = pPktCtrlBlk->tTxDescriptor.length;
    TI_UINT16 uLastWordPad;
    TI_UINT32 uBufNum;

    uPktLen = (uPktLen + 3) & 0xFFFC;                               /* Add alignment bytes if needed */
    uLastWordPad = uPktLen - pPktCtrlBlk->tTxDescriptor.length;     /* Find number of alignment bytes added */
    uPktLen = uPktLen >> 2;                                         /* Convert length to words */
	pPktCtrlBlk->tTxDescriptor.length = ENDIAN_HANDLE_WORD(uPktLen);/* Save FW format length in descriptor */

    /* Find last buffer (last buffer in use is pointed by uBufNum-1) */
    for (uBufNum = 1; uBufNum < MAX_XFER_BUFS; uBufNum++) 
    {
        if (pPktCtrlBlk->tTxnStruct.aLen[uBufNum] == 0)
        {
            break;
        }
    }
    /* Add last word alignment pad also to the last buffer length */
    pPktCtrlBlk->tTxnStruct.aLen[uBufNum - 1] += uLastWordPad;
    
    return uLastWordPad;
}

/* Translate packet timestamp to FW time, and update also lifeTime and uDriverDelay */
static inline void txCtrl_TranslateTimeToFw (txCtrl_t *pTxCtrl, TTxCtrlBlk *pPktCtrlBlk, TI_UINT16 uLifeTime)
{
    TI_UINT32 uPktStartTime = pPktCtrlBlk->tTxDescriptor.startTime;  /* Contains host start time */

    /* Save host packet handling time until this point (for statistics) */
    pPktCtrlBlk->tTxPktParams.uDriverDelay = os_timeStampMs (pTxCtrl->hOs) - uPktStartTime;

    /* Translate packet timestamp to FW time and undate descriptor */
    uPktStartTime = TWD_TranslateToFwTime (pTxCtrl->hTWD, uPktStartTime); 
	pPktCtrlBlk->tTxDescriptor.startTime = ENDIAN_HANDLE_LONG (uPktStartTime);
	pPktCtrlBlk->tTxDescriptor.lifeTime  = ENDIAN_HANDLE_WORD (uLifeTime);
}



/********************************************************************************
*																				*
*                       PUBLIC  FUNCTIONS  IMPLEMENTATION						*
*																				*
*********************************************************************************/

/*************************************************************************
*                        txCtrl_Create                                   *
**************************************************************************
* DESCRIPTION:  This function initializes the Tx module.
*
* INPUT:        hOs - handle to Os Abstraction Layer
*
* OUTPUT:
*
* RETURN:       Handle to the allocated Tx data control block
*************************************************************************/
TI_HANDLE txCtrl_Create (TI_HANDLE hOs)
{
	txCtrl_t *pTxCtrl;

	/* allocate Tx module control block */
	pTxCtrl = os_memoryAlloc(hOs, (sizeof(txCtrl_t)));

	if (!pTxCtrl)
		return NULL;

	/* reset tx control object */
	os_memoryZero(hOs, pTxCtrl, (sizeof(txCtrl_t)));

	pTxCtrl->TxEventDistributor = DistributorMgr_Create(hOs, MAX_TX_NOTIF_REQ_ELMENTS);

	pTxCtrl->hOs = hOs;

	return pTxCtrl;
}


/***************************************************************************
*                           txCtrl_Init                                  *
****************************************************************************
* DESCRIPTION:  This function configures the TxCtrl module.
***************************************************************************/
void txCtrl_Init (TStadHandlesList *pStadHandles)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)(pStadHandles->hTxCtrl);
	TI_UINT32 ac;

	/* Save other modules handles */
	pTxCtrl->hOs			= pStadHandles->hOs;
	pTxCtrl->hReport		= pStadHandles->hReport;
	pTxCtrl->hCtrlData		= pStadHandles->hCtrlData;
	pTxCtrl->hTWD		    = pStadHandles->hTWD;
	pTxCtrl->hTxDataQ	    = pStadHandles->hTxDataQ;
	pTxCtrl->hTxMgmtQ	    = pStadHandles->hTxMgmtQ;
	pTxCtrl->hEvHandler		= pStadHandles->hEvHandler;
	pTxCtrl->hHealthMonitor = pStadHandles->hHealthMonitor;
	pTxCtrl->hTimer         = pStadHandles->hTimer;
	pTxCtrl->hStaCap        = pStadHandles->hStaCap;
	pTxCtrl->hXCCMngr       = pStadHandles->hXCCMngr;
	pTxCtrl->hQosMngr       = pStadHandles->hQosMngr;
	pTxCtrl->hRxData        = pStadHandles->hRxData;

	/* Set Tx parameters to defaults */
	pTxCtrl->headerConverMode = HDR_CONVERT_NONE;
	pTxCtrl->currentPrivacyInvokedMode = DEF_CURRENT_PRIVACY_MODE;
	pTxCtrl->eapolEncryptionStatus = DEF_EAPOL_ENCRYPTION_STATUS;
	pTxCtrl->encryptionFieldSize = 0;
	pTxCtrl->currBssType = BSS_INFRASTRUCTURE;
	pTxCtrl->busyAcBitmap = 0;
	pTxCtrl->dbgPktSeqNum = 0;		
	pTxCtrl->bCreditCalcTimerRunning = TI_FALSE;
	pTxCtrl->genericEthertype = ETHERTYPE_EAPOL;

	for (ac = 0; ac < MAX_NUM_OF_AC; ac++)
	{
		pTxCtrl->aMsduLifeTimeTu[ac] = MGMT_PKT_LIFETIME_TU;
		pTxCtrl->ackPolicy[ac] = ACK_POLICY_LEGACY;
		pTxCtrl->admissionState[ac] = AC_ADMITTED;
		pTxCtrl->admissionRequired[ac] = ADMISSION_NOT_REQUIRED;
		pTxCtrl->useAdmissionAlgo[ac] = TI_FALSE;
		pTxCtrl->mediumTime[ac] = 0;
		pTxCtrl->lastCreditCalcTimeStamp[ac] = 0;
		pTxCtrl->credit[ac] = 0;
	}

	/* Reset counters */
	txCtrlParams_resetCounters (pStadHandles->hTxCtrl);

#ifdef TI_DBG
	txCtrlParams_resetDbgCounters (pStadHandles->hTxCtrl);
#endif

	/* Register the Tx-Complete callback function. */
	TWD_RegisterCb (pTxCtrl->hTWD, 
			TWD_EVENT_TX_RESULT_SEND_PKT_COMPLETE, 
			(void*)txCtrl_TxCompleteCb, 
			pStadHandles->hTxCtrl);

	/* Register the Update-Busy-Map callback function. */
	TWD_RegisterCb (pTxCtrl->hTWD, 
			TWD_EVENT_TX_HW_QUEUE_UPDATE_BUSY_MAP, 
			(void *)txCtrl_UpdateBackpressure, 
			pStadHandles->hTxCtrl);

	TRACE0(pTxCtrl->hReport, REPORT_SEVERITY_INIT, ".....Tx Data configured successfully\n");
}


/*************************************************************************
*                        txCtrl_SetDefaults                                   *
**************************************************************************
* DESCRIPTION:  
*
* INPUT:        
*               txDataInitParams - Tx Data creation parameters
*               
* OUTPUT:
*
* RETURN:       
*************************************************************************/
TI_STATUS txCtrl_SetDefaults (TI_HANDLE hTxCtrl, txDataInitParams_t *txDataInitParams)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->creditCalculationTimeout = txDataInitParams->creditCalculationTimeout;
	pTxCtrl->bCreditCalcTimerEnabled  = txDataInitParams->bCreditCalcTimerEnabled;

	/* Update queues mapping (AC/TID/Backpressure) after module init. */
	txCtrl_UpdateQueuesMapping (hTxCtrl); 

	/* allocate timer for credit calculation */
	pTxCtrl->hCreditTimer = tmr_CreateTimer (pTxCtrl->hTimer);
	if (pTxCtrl->hCreditTimer == NULL)
	{
		TRACE0(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, "txCtrl_SetDefaults(): Failed to create hCreditTimer!\n");
		return TI_NOK;
	}

	return TI_OK;
}


/***************************************************************************
*                           txCtrl_Unload                                  *
****************************************************************************
* DESCRIPTION:  This function unload the tx ctrl module. 
*
***************************************************************************/
TI_STATUS txCtrl_Unload (TI_HANDLE hTxCtrl)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    if (pTxCtrl == NULL)
    {
        return TI_NOK;
    }

    DistributorMgr_Destroy (pTxCtrl->TxEventDistributor);

	if (pTxCtrl->hCreditTimer)
	{
		tmr_DestroyTimer (pTxCtrl->hCreditTimer);
	}

    /* free Tx Data control block */
    os_memoryFree (pTxCtrl->hOs, pTxCtrl, sizeof(txCtrl_t));

    return TI_OK;
}



/*******************************************************************************
*                          txCtrl_XmitData		                               *
********************************************************************************
* DESCRIPTION:  Get a packet from the data-queue, allocate HW resources and CtrlBlk,
*				  build header and descriptor, and send it to HW by TxXfer.
*
* RETURNS:      STATUS_XMIT_SUCCESS - Packet sent succesfully
*				STATUS_XMIT_BUSY    - Packet dropped due to lack of HW resources, retransmit later.
*				STATUS_XMIT_ERROR   - Packet dropped due to an unexpected problem (bug).
********************************************************************************/
TI_STATUS txCtrl_XmitData (TI_HANDLE hTxCtrl, TTxCtrlBlk *pPktCtrlBlk)
{
    txCtrl_t   *pTxCtrl = (txCtrl_t *)hTxCtrl;
	ETxnStatus eStatus;       /* The Xfer return value (different than this function's return values). */
	TI_UINT32  uAc;
	TI_UINT32  uBackpressure = 0; /* HwQueue's indication when the current queue becomes busy. */
    ETxHwQueStatus eHwQueStatus;
	CL_TRACE_START_L3();
    
	/* Get an admitted AC corresponding to the packet TID. 
	 * If downgraded due to admission limitation, the TID is downgraded as well. 
	 */
	SELECT_AC_FOR_TID (pTxCtrl, pPktCtrlBlk->tTxDescriptor.tid, uAc);

#ifdef TI_DBG
TRACE3(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_XmitData(): Pkt Tx, DescID=%d, AC=%d, Len=%d\n", pPktCtrlBlk->tTxDescriptor.descID, uAc, pPktCtrlBlk->tTxDescriptor.length );

	pTxCtrl->dbgCounters.dbgNumPktsSent[uAc]++;
#endif

	/* Call TxHwQueue for Hw resources allocation. */
	{
		CL_TRACE_START_L4();
		eHwQueStatus = TWD_txHwQueue_AllocResources (pTxCtrl->hTWD, pPktCtrlBlk);
		CL_TRACE_END_L4("tiwlan_drv.ko", "INHERIT", "TX", ".allocResources");
	}

	/* If the current AC can't get more packets, stop it in data-queue module. */
	if (eHwQueStatus == TX_HW_QUE_STATUS_STOP_NEXT)
	{
#ifdef TI_DBG
		pTxCtrl->dbgCounters.dbgNumPktsBackpressure[uAc]++;
        TRACE2(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_XmitData(): Backpressure = 0x%x, queue = %d\n", uBackpressure, uAc);
#endif
        uBackpressure = 1 << uAc;
		pTxCtrl->busyAcBitmap |= uBackpressure; /* Set the busy bit of the current AC. */
		txDataQ_StopQueue (pTxCtrl->hTxDataQ, pTxCtrl->admittedAcToTidMap[uAc]);
	}

	/* If current packet can't be transmitted due to lack of resources, return with BUSY value. */
	else if (eHwQueStatus == TX_HW_QUE_STATUS_STOP_CURRENT)
	{
#ifdef TI_DBG
		pTxCtrl->dbgCounters.dbgNumPktsBusy[uAc]++;
        TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_XmitData(): Queue busy - Packet dropped, queue = %d\n", uAc);
#endif
		txDataQ_StopQueue (pTxCtrl->hTxDataQ, pTxCtrl->admittedAcToTidMap[uAc]);
		CL_TRACE_END_L3("tiwlan_drv.ko", "INHERIT", "TX", "");
		return STATUS_XMIT_BUSY;
	}
	
	/* Prepare the packet control-block including the Tx-descriptor. */
	{
		CL_TRACE_START_L4();
		txCtrl_BuildDataPkt(pTxCtrl, pPktCtrlBlk, uAc, uBackpressure);
		CL_TRACE_END_L4("tiwlan_drv.ko", "INHERIT", "TX", ".FillCtrlBlk");
	}

	/* Call the Tx-Xfer to start packet transfer to the FW and return its result. */
	{
		CL_TRACE_START_L4();
		eStatus = TWD_txXfer_SendPacket (pTxCtrl->hTWD, pPktCtrlBlk);
		CL_TRACE_END_L4("tiwlan_drv.ko", "INHERIT", "TX", ".XferSendPacket");
	}

	if (eStatus == TXN_STATUS_ERROR)
	{
#ifdef TI_DBG  
TRACE2(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, "txCtrl_XmitData(): Xfer Error, queue = %d, Status = %d\n", uAc, eStatus);
		pTxCtrl->dbgCounters.dbgNumPktsError[uAc]++;	
#endif

		/* Free the packet resources (packet and CtrlBlk)  */
        txCtrl_FreePacket (pTxCtrl, pPktCtrlBlk, TI_NOK);

		CL_TRACE_END_L3("tiwlan_drv.ko", "INHERIT", "TX", "");
		return STATUS_XMIT_ERROR;
	}

#ifdef TI_DBG  
    pTxCtrl->dbgCounters.dbgNumPktsSuccess[uAc]++;	
#endif
    CL_TRACE_END_L3("tiwlan_drv.ko", "INHERIT", "TX", "");
    return STATUS_XMIT_SUCCESS;
}


/*******************************************************************************
*                          txCtrl_XmitMgmt		                               *
********************************************************************************
* DESCRIPTION:  Get a packet from the Mgmt-Queue (management, EAPOL or IAPP), 
*				  allocate HW resources and CtrlBlk, build header if Ethernet (EAPOL),
*				  build descriptor, and send packet to HW by TxXfer.
*
* RETURNS:      STATUS_XMIT_SUCCESS - Packet sent succesfully.
*				STATUS_XMIT_BUSY - Packet dropped due to lack of HW resources, retransmit later.
*				STATUS_XMIT_ERROR - Packet dropped due to an unexpected problem (bug).
********************************************************************************/
TI_STATUS txCtrl_XmitMgmt (TI_HANDLE hTxCtrl, TTxCtrlBlk *pPktCtrlBlk)
{
    txCtrl_t   *pTxCtrl = (txCtrl_t *)hTxCtrl;
	ETxnStatus eStatus;      /* The Xfer return value (different than this function's return values). */
	TI_UINT32  uAc;  /* The AC selected for the packet transmission. */
	TI_UINT32  uBackpressure = 0;/* HwQueue's indication when the current queue becomes busy. */
    ETxHwQueStatus eHwQueStatus;

	/* Get an admitted AC corresponding to the packet TID. 
	 * If downgraded due to admission limitation, the TID is downgraded as well. 
	 */
	SELECT_AC_FOR_TID (pTxCtrl, pPktCtrlBlk->tTxDescriptor.tid, uAc);

#ifdef TI_DBG
	pTxCtrl->dbgCounters.dbgNumPktsSent[uAc]++;
#endif

	/* Call TxHwQueue for Hw resources allocation. */
   	eHwQueStatus = TWD_txHwQueue_AllocResources (pTxCtrl->hTWD, pPktCtrlBlk);

	/* If the used AC can't get more packets, stop it in mgmt-queue module. */
	if (eHwQueStatus == TX_HW_QUE_STATUS_STOP_NEXT)
	{
#ifdef TI_DBG
		pTxCtrl->dbgCounters.dbgNumPktsBackpressure[uAc]++;
        TRACE2(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_XmitMgmt(): Backpressure = 0x%x, queue = %d\n", uBackpressure, uAc);
#endif
        uBackpressure = 1 << uAc;
		pTxCtrl->busyAcBitmap |= uBackpressure; /* Set the busy bit of the current AC. */
		txMgmtQ_StopQueue (pTxCtrl->hTxMgmtQ, pTxCtrl->admittedAcToTidMap[uAc]);
	}

	/* If current packet can't be transmitted due to lack of resources, return with BUSY value. */
	else if (eHwQueStatus == TX_HW_QUE_STATUS_STOP_CURRENT)
	{
#ifdef TI_DBG
		pTxCtrl->dbgCounters.dbgNumPktsBusy[uAc]++;
        TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_XmitMgmt(): Queue busy - Packet dropped, queue = %d\n", uAc);
#endif
		txMgmtQ_StopQueue (pTxCtrl->hTxMgmtQ, pTxCtrl->admittedAcToTidMap[uAc]);
		return STATUS_XMIT_BUSY;
	}
	
	/* Prepare the packet control-block including the Tx-descriptor. */
	txCtrl_BuildMgmtPkt (pTxCtrl, pPktCtrlBlk, uAc);
    
	/* Call the Tx-Xfer to start packet transfer to the FW and return the result. */
	eStatus = TWD_txXfer_SendPacket (pTxCtrl->hTWD, pPktCtrlBlk);

	if (eStatus == TXN_STATUS_ERROR)
	{
#ifdef TI_DBG  
TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, "txCtrl_XmitMgmt(): Xfer Error, Status = %d\n", eStatus);
		pTxCtrl->dbgCounters.dbgNumPktsError[uAc]++;	
#endif
		/* Free the packet resources (packet and CtrlBlk)  */
        txCtrl_FreePacket (pTxCtrl, pPktCtrlBlk, TI_NOK);
		return STATUS_XMIT_ERROR;
	}

#ifdef TI_DBG  
    pTxCtrl->dbgCounters.dbgNumPktsSuccess[uAc]++;	
#endif
    return STATUS_XMIT_SUCCESS;
}


/***************************************************************************
*                           txCtrl_UpdateQueuesMapping 
****************************************************************************
* DESCRIPTION:  This function should be called upon the following events:
*					1) Init
*					2) ACs admission required change (upon association)
*					3) ACs admission state change (upon association and add/delete Tspec).
*				It updates the following mappings (by this oredr!):
*					1) Update mapping from requested-AC to highest-admitted-AC.
*					2) Update mapping from actual-AC to requested-TID (for backpressure mapping).
*					3) Update TID-backpressure bitmap, and if changed update data-queue and mgmt-queue.
*
***************************************************************************/
void txCtrl_UpdateQueuesMapping (TI_HANDLE hTxCtrl)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
	
	/* Update mapping from requested-AC to highest-admitted-AC. */
	txCtrl_UpdateHighestAdmittedAcTable (pTxCtrl);

	/* Update mapping from actual-AC to requested-TID (for backpressure mapping). */
	txCtrl_UpdateAcToTidMapping (pTxCtrl);

	/* Update TID-backpressure bitmap, and if changed update data-queue and mgmt-queue. */
	txCtrl_UpdateBackpressure (pTxCtrl, 0);
}


/***************************************************************************
*                           txCtrl_AllocPacketBuffer
****************************************************************************
* DESCRIPTION:  Allocate a raw buffer for the whole Tx packet.
                Used for driver generated packets and when the OAL needs to 
                    copy the packet to a new buffer (e.g. to gather multiple buffers). 
***************************************************************************/
void *txCtrl_AllocPacketBuffer (TI_HANDLE hTxCtrl, TTxCtrlBlk *pPktCtrlBlk, TI_UINT32 uPacketLen)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
    void     *pRawBuf = os_memoryAlloc (pTxCtrl->hOs, uPacketLen);

    if (pRawBuf) 
    {
        /* Indicate that the packet is in a raw buffer (i.e. not OS packet) and save its address and length */
        pPktCtrlBlk->tTxPktParams.uFlags |= TX_CTRL_FLAG_PKT_IN_RAW_BUF;
        
        /* Save buffer address and length for the free operation */
        pPktCtrlBlk->tTxPktParams.pInputPkt    = pRawBuf;
        pPktCtrlBlk->tTxPktParams.uInputPktLen = uPacketLen;
        
        TRACE2(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_AllocPacketBuffer(): pRawBuf = 0x%x, uPacketLen = %d\n", pRawBuf, uPacketLen);
    } 
    else 
    {
        TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, "txCtrl_AllocPacketBuffer(): uPacketLen = %d, returning NULL\n", uPacketLen);
    }

    return pRawBuf;
}


/***************************************************************************
*                           txCtrl_FreePacket 
****************************************************************************
* DESCRIPTION:  Free the packet resources, including the packet and the CtrlBlk 
***************************************************************************/
void txCtrl_FreePacket (TI_HANDLE hTxCtrl, TTxCtrlBlk *pPktCtrlBlk, TI_STATUS eStatus)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    TRACE3(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_FreePacket(): RawBufFlag = 0x%x, pBuf = 0x%x, Len = %d\n", (pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_PKT_IN_RAW_BUF), pPktCtrlBlk->tTxPktParams.pInputPkt, pPktCtrlBlk->tTxPktParams.uInputPktLen);

    /* If the packet is in a raw buffer, free its memory */
    if (pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_PKT_IN_RAW_BUF)
    {
        os_memoryFree (pTxCtrl->hOs, 
                       pPktCtrlBlk->tTxPktParams.pInputPkt, 
                       pPktCtrlBlk->tTxPktParams.uInputPktLen);
    }
    /* If the original packet is in OS format, call the OAL to free it */
    else 
    {
        wlanDrvIf_FreeTxPacket (pTxCtrl->hOs, pPktCtrlBlk, eStatus);
    }

    /* Free the CtrlBlk */
    TWD_txCtrlBlk_Free (pTxCtrl->hTWD, pPktCtrlBlk);
}



/********************************************************************************
*																				*
*                       LOCAL  FUNCTIONS  IMPLEMENTATION						*
*																				*
*********************************************************************************/


/*************************************************************************
*                        txCtrl_TxCompleteCb		                             *
**************************************************************************
* DESCRIPTION:  Called by the TWD upon Tx-complete of one packet.
*				Handle packet result:
*				- Update counters (statistics and medium-usage)
*				- Free the packet resources (Wbuf and CtrlBlk)
*
* INPUT:    hTWD -  The Tnetw-Driver handle.
*		    pTxResultInfo - The packet's Tx result information.   
*              
*************************************************************************/
static void txCtrl_TxCompleteCb (TI_HANDLE hTxCtrl, TxResultDescriptor_t *pTxResultInfo)
{
    txCtrl_t    *pTxCtrl = (txCtrl_t *)hTxCtrl;
	TTxCtrlBlk  *pPktCtrlBlk;
	TI_UINT32	ac;
	TI_BOOL	    bIsDataPkt;
	CL_TRACE_START_L3();

	/* Get packet ctrl-block by desc-ID. */
	pPktCtrlBlk = TWD_txCtrlBlk_GetPointer (pTxCtrl->hTWD, pTxResultInfo->descID);
	ac = WMEQosTagToACTable[pPktCtrlBlk->tTxDescriptor.tid];

#ifdef TI_DBG
	/* If the pointed entry is already free, print error and exit (not expected to happen). */
	if (pPktCtrlBlk->pNextFreeEntry != NULL)
	{
TRACE2(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, "txCtrl_TxCompleteCb(): Pkt already free!!, DescID=%d, AC=%d\n", pTxResultInfo->descID, ac);
		CL_TRACE_END_L3("tiwlan_drv.ko", "INHERIT", "TX_Cmplt", "");
		return;
	}
TRACE3(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_TxCompleteCb(): Pkt Tx Complete, DescID=%d, AC=%d, Status=%d\n", pTxResultInfo->descID, ac, pTxResultInfo->status);
#endif
	/* Update the TKIP/AES sequence-number according to the Tx data packet security-seq-num. */
	/* Note: The FW always provides the last used seq-num so no need to check if the current 
			 packet is data and WEP is on. */
	TWD_SetSecuritySeqNum (pTxCtrl->hTWD, pTxResultInfo->lsbSecuritySequenceNumber);

	bIsDataPkt = ( (pPktCtrlBlk->tTxPktParams.uPktType == TX_PKT_TYPE_ETHER) || 
		           (pPktCtrlBlk->tTxPktParams.uPktType == TX_PKT_TYPE_WLAN_DATA) );

#ifdef XCC_MODULE_INCLUDED    
	/* If it's a XCC link-test packet, call its handler. */
	if (pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_LINK_TEST)
    {
        CL_TRACE_START_L4();
        XCCMngr_LinkTestRetriesUpdate (pTxCtrl->hXCCMngr, pTxResultInfo->ackFailures);
        CL_TRACE_END_L4("tiwlan_drv.ko", "INHERIT", "TX_Cmplt", ".XCCLinkTest");
    }
#endif

	/* Add the medium usage time for the specific queue. */
	pTxCtrl->totalUsedTime[ac] += (TI_UINT32)ENDIAN_HANDLE_WORD(pTxResultInfo->mediumUsage);

	/* update TX counters for txDistributer */
    {
        CL_TRACE_START_L4();
        txCtrl_UpdateTxCounters (pTxCtrl, pTxResultInfo, pPktCtrlBlk, ac, bIsDataPkt);
        CL_TRACE_END_L4("tiwlan_drv.ko", "INHERIT", "TX_Cmplt", ".Cntrs");
    }

	/* Free the packet resources (packet and CtrlBlk)  */
    txCtrl_FreePacket (pTxCtrl, pPktCtrlBlk, TI_OK);

	CL_TRACE_END_L3("tiwlan_drv.ko", "INHERIT", "TX_Cmplt", "");
}


/***************************************************************************
*                   txCtrl_BuildDataPktHdr                                 *
****************************************************************************
* DESCRIPTION:  this function builds the WLAN header from ethernet format, 
*               including 802.11-MAC, LLC/SNAP, security padding, alignment padding.
*
* INPUTS:       hTxCtrl - the object
*               pPktCtrlBlk - data packet control block (Ethernet header)
*
* RETURNS:      uHdrAlignPad - Num of bytes (0 or 2) added at the header's beginning for 4-bytes alignment.
***************************************************************************/

TI_UINT32 txCtrl_BuildDataPktHdr (TI_HANDLE hTxCtrl, TTxCtrlBlk *pPktCtrlBlk, AckPolicy_e eAckPolicy)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;    
    TEthernetHeader     *pEthHeader;
    dot11_header_t      *pDot11Header;
    Wlan_LlcHeader_T    *pWlanSnapHeader;
	EHeaderConvertMode	eQosMode = pTxCtrl->headerConverMode;
	TI_UINT32			uHdrLen = 0;
	TI_UINT32			uHdrAlignPad = 0;
	TI_UINT16			uQosControl;
	TI_UINT16			fc = 0;
 	TI_UINT16           typeLength;


	/* 
	 * Handle encryption if needed, for data or EAPOL (decision was done at RSN):
	 *   - Set WEP bit in header.
	 *   - Add padding for FW security overhead: 4 bytes for TKIP, 8 for AES.
	 */

       	if (( (pPktCtrlBlk->tTxPktParams.uPktType == TX_PKT_TYPE_EAPOL)
			  &&
			  pTxCtrl->eapolEncryptionStatus) 
		 ||
			((pPktCtrlBlk->tTxPktParams.uPktType != TX_PKT_TYPE_EAPOL)
			  &&
			  pTxCtrl->currentPrivacyInvokedMode ))
        {
			fc |= DOT11_FC_WEP;
			uHdrLen += pTxCtrl->encryptionFieldSize;
			uHdrAlignPad = pTxCtrl->encryptionFieldSize % 4;
	}

	/*
	 * Handle QoS if needed:
	 */
    if (eQosMode == HDR_CONVERT_QOS)
	{
		uHdrAlignPad = (uHdrAlignPad + HEADER_PAD_SIZE) % 4; /* Add 2 bytes pad at the header beginning for 4 bytes alignment. */
		pDot11Header = (dot11_header_t *)&(pPktCtrlBlk->aPktHdr[uHdrAlignPad]);
		uHdrLen += WLAN_QOS_HDR_LEN;

        /* add empty 4Byte for HT control field set via the FW */
        if (pTxCtrl->tTxCtrlHtControl.bHtEnable == TI_TRUE)
        {
            uHdrLen += WLAN_QOS_HT_CONTROL_FIELD_LEN;
            fc |= DOT11_FC_ORDER;
        }

		/* Set Qos control fields. */
		uQosControl = (TI_UINT16)(pPktCtrlBlk->tTxDescriptor.tid); 
		if ( TI_UNLIKELY(eAckPolicy == ACK_POLICY_NO_ACK) )
			uQosControl |= DOT11_QOS_CONTROL_DONT_ACK;
		COPY_WLAN_WORD(&pDot11Header->qosControl, &uQosControl); /* copy with endianess handling. */
	}
	else  /* No QoS (legacy header, padding is not needed). */
	{
		pDot11Header = (dot11_header_t *)&(pPktCtrlBlk->aPktHdr[uHdrAlignPad]);
		uHdrLen += WLAN_HDR_LEN;
	}
	uHdrLen += uHdrAlignPad;

    /* Before the header translation the first buf-pointer points to the Ethernet header. */	
	pEthHeader = (TEthernetHeader *)(pPktCtrlBlk->tTxnStruct.aBuf[0]);

    if (TI_UNLIKELY(MAC_MULTICAST(pEthHeader->dst)))
    {
        pPktCtrlBlk->tTxPktParams.uFlags |= TX_CTRL_FLAG_MULTICAST;
        if (MAC_BROADCAST(pEthHeader->dst))
        {
            pPktCtrlBlk->tTxPktParams.uFlags |= TX_CTRL_FLAG_BROADCAST;
        }
    }

	/* Set MAC header fields for Independent-BSS case. */
    if ( TI_UNLIKELY(pTxCtrl->currBssType == BSS_INDEPENDENT) )
    {
        MAC_COPY (pDot11Header->address1, pEthHeader->dst);
        MAC_COPY (pDot11Header->address2, pEthHeader->src);
        MAC_COPY (pDot11Header->address3, pTxCtrl->currBssId);

        if (eQosMode == HDR_CONVERT_QOS)
            fc |= DOT11_FC_DATA_QOS;
        else
            fc |= DOT11_FC_DATA;
    }

	/* Set MAC header fields for Infrastructure-BSS case. */
    else
    {
        MAC_COPY (pDot11Header->address1, pTxCtrl->currBssId);
        MAC_COPY (pDot11Header->address2, pEthHeader->src);
        MAC_COPY (pDot11Header->address3, pEthHeader->dst);

        if (eQosMode == HDR_CONVERT_QOS)
            fc |= DOT11_FC_DATA_QOS | DOT11_FC_TO_DS;
        else
            fc |= DOT11_FC_DATA | DOT11_FC_TO_DS;
    }

	COPY_WLAN_WORD(&pDot11Header->fc, &fc); /* copy with endianess handling. */
	
    /* Set the SNAP header pointer right after the other header parts handled above. */
    pWlanSnapHeader = (Wlan_LlcHeader_T *)&(pPktCtrlBlk->aPktHdr[uHdrLen]);
    
	typeLength = HTOWLANS(pEthHeader->type);

    /* Detect the packet type and decide if to create a     */
    /*          new SNAP or leave the original LLC.         */
    /*------------------------------------------------------*/
   	if( typeLength > ETHERNET_MAX_PAYLOAD_SIZE )
    {
        /* Create the SNAP Header:     */
        /*-----------------------------*/
        /*
         * Make a working copy of the SNAP header
         * initialised to zero
         */
		
        pWlanSnapHeader->DSAP = SNAP_CHANNEL_ID;
        pWlanSnapHeader->SSAP = SNAP_CHANNEL_ID;
        pWlanSnapHeader->Control = LLC_CONTROL_UNNUMBERED_INFORMATION;

        /* Check to see if the Ethertype matches anything in the translation     */
        /* table (Appletalk AARP or DixII/IPX).  If so, add the 802.1h           */
        /* SNAP.                                                                 */

        if(( ETHERTYPE_APPLE_AARP == typeLength ) ||
           ( ETHERTYPE_DIX_II_IPX == typeLength ))
        {
            /* Fill out the SNAP Header with 802.1H extention   */
            pWlanSnapHeader->OUI[0] = SNAP_OUI_802_1H_BYTE0;
			pWlanSnapHeader->OUI[1] = SNAP_OUI_802_1H_BYTE1;
			pWlanSnapHeader->OUI[2] = SNAP_OUI_802_1H_BYTE2;

        }
        else
        {
            /* otherwise, add the RFC1042 SNAP   */
    		pWlanSnapHeader->OUI[0] = SNAP_OUI_RFC1042_BYTE0;
			pWlanSnapHeader->OUI[1] = SNAP_OUI_RFC1042_BYTE0;
			pWlanSnapHeader->OUI[2] = SNAP_OUI_RFC1042_BYTE0;
        }

        /* set type length */
        pWlanSnapHeader->Type = pEthHeader->type;
    
        /* Add the SNAP length to the total header length. */
        uHdrLen += sizeof(Wlan_LlcHeader_T);
    }

    /* Replace first buffer pointer and length to the descriptor and WLAN-header (instead of Ether header) */
    pPktCtrlBlk->tTxnStruct.aBuf[0] = (TI_UINT8 *)&(pPktCtrlBlk->tTxDescriptor);
    pPktCtrlBlk->tTxnStruct.aLen[0] = sizeof(TxIfDescriptor_t) + uHdrLen;
    pPktCtrlBlk->tTxDescriptor.length += pPktCtrlBlk->tTxnStruct.aLen[0] - ETHERNET_HDR_LEN;

	/* Return the number of bytes (0 or 2) added at the header's beginning for 4-bytes alignment. */
    return uHdrAlignPad;
}


/***************************************************************************
*                       txCtrl_BuildDataPkt                            
****************************************************************************
* DESCRIPTION:  Prepare the Data packet control-block including the Tx-descriptor.
***************************************************************************/
static void txCtrl_BuildDataPkt (txCtrl_t *pTxCtrl, TTxCtrlBlk *pPktCtrlBlk, 
                                 TI_UINT32 uAc, TI_UINT32 uBackpressure)
{
	TI_UINT32 uHdrAlignPad; /* Num of bytes added between Tx-descriptor and header for 4 bytes alignment (0 or 2). */
    TI_UINT16 uLastWordPad; /* Num of bytes added at the end of the packet for 4 bytes alignment */
	TI_UINT16 uTxDescAttr;
	AckPolicy_e eAckPolicy = pTxCtrl->ackPolicy[uAc];

	/* Build packet header (including MAC, LLC/SNAP, security padding, header alignment padding). */
    uHdrAlignPad = txCtrl_BuildDataPktHdr ((TI_HANDLE)pTxCtrl, pPktCtrlBlk, eAckPolicy);

	/* Update packet length in the descriptor according to HW interface requirements */
    uLastWordPad = txCtrl_TranslateLengthToFw (pPktCtrlBlk);

    /* Set the descriptor attributes */
	uTxDescAttr  = pTxCtrl->dataPktDescAttrib;
    uTxDescAttr |= uLastWordPad << TX_ATTR_OFST_LAST_WORD_PAD;
	uTxDescAttr |= pTxCtrl->dataRatePolicy[uAc] << TX_ATTR_OFST_RATE_POLICY;
	if (uHdrAlignPad)
	{
		uTxDescAttr |= TX_ATTR_HEADER_PAD;
	}
	if (uBackpressure)
    {
		uTxDescAttr |= TX_ATTR_TX_CMPLT_REQ;  /* Request immediate Tx-Complete from FW if the AC is busy */
    }
    if (TI_UNLIKELY(pTxCtrl->currBssType == BSS_INDEPENDENT) &&
        (pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_MULTICAST))
	{
        /* If packet is Broadcast in IBSS, overwrite rate policy with mgmt value. */
		uTxDescAttr &= ~TX_ATTR_RATE_POLICY;
		uTxDescAttr |= pTxCtrl->mgmtRatePolicy[uAc] << TX_ATTR_OFST_RATE_POLICY;
	}
	pPktCtrlBlk->tTxDescriptor.txAttr = ENDIAN_HANDLE_WORD(uTxDescAttr); 

    /* Translate packet timestamp to FW time (also updates lifeTime and driverHandlingTime) */
    txCtrl_TranslateTimeToFw (pTxCtrl, pPktCtrlBlk, pTxCtrl->aMsduLifeTimeTu[uAc]);

    /* Indicate that the packet is transfered to the FW, and the descriptor fields are in FW format! */
    pPktCtrlBlk->tTxPktParams.uFlags |= TX_CTRL_FLAG_SENT_TO_FW;

#ifdef TI_DBG  
	pTxCtrl->dbgPktSeqNum++;
	pTxCtrl->dbgCounters.dbgNumPktsXfered[uAc]++;	/* Count packets sent to Xfer. */
#endif
}


/***************************************************************************
*                       txCtrl_BuildMgmtPkt                            
****************************************************************************
* DESCRIPTION:  Prepare the Mgmt-Queue packet control-block including the Tx-descriptor.
***************************************************************************/
static void txCtrl_BuildMgmtPkt (txCtrl_t *pTxCtrl, TTxCtrlBlk *pPktCtrlBlk, TI_UINT32 uAc)
{
	TI_UINT32 uHdrAlignPad; /* Num of bytes added between Tx-descriptor and header for alignment (0 or 2). */
    TI_UINT16 uLastWordPad; /* Num of bytes added at the end of the packet for 4 bytes alignment */
	TI_UINT16 uTxDescAttr;
	TI_UINT16 uRatePolicy;
	TI_UINT8  uPktType = pPktCtrlBlk->tTxPktParams.uPktType;
	dot11_header_t *pDot11Header;
	
	/* If EAPOL packet (Ethernet), build header (including MAC,SNAP,security pad & alignment pad). */
	if (uPktType == TX_PKT_TYPE_EAPOL)
	{
        uHdrAlignPad = txCtrl_BuildDataPktHdr ((TI_HANDLE)pTxCtrl, pPktCtrlBlk, ACK_POLICY_LEGACY);

		uRatePolicy = pTxCtrl->dataRatePolicy[uAc];
	}

	/*  Other types are already in WLAN format so copy header from Wbuf to Ctrl-Blk. */
	else         
	{
		TI_UINT32 uHdrLen = pPktCtrlBlk->tTxnStruct.aLen[0];
		TI_UINT32 uHdrLenDelta; /* Add the header pad (2 bytes) and Tx-Descriptor length */

		/*  
         * Update the length fields to include the header pad and the Tx-Descriptor.
         * Note: The mgmt-queue provides the header length without the alignment pad, so if 
		 *       it's not 4-byte aligned, a 2-bytes pad was added at the header beginning. 
		 */
		uHdrAlignPad = (uHdrLen & ALIGN_4BYTE_MASK) ? HEADER_PAD_SIZE : 0;  
        uHdrLenDelta = uHdrAlignPad + sizeof(TxIfDescriptor_t);
        pPktCtrlBlk->tTxnStruct.aBuf[0]   -= uHdrLenDelta;
        pPktCtrlBlk->tTxnStruct.aLen[0]   += uHdrLenDelta;
        pPktCtrlBlk->tTxDescriptor.length += uHdrLenDelta;
		
		uRatePolicy = pTxCtrl->mgmtRatePolicy[uAc];

        if (uPktType == TX_PKT_TYPE_WLAN_DATA)  
		{
			/* If QoS mode, update TID in QoS header in case it was downgraded. */
			/* Note: Qos-hdr update for EAPOL is done in txCtrl_BuildDataPktHeader() and doesn't exist in mgmt. */
			if (pTxCtrl->headerConverMode == HDR_CONVERT_QOS)
			{
				TI_UINT16 tidWord = (TI_UINT16)pPktCtrlBlk->tTxDescriptor.tid;
				pDot11Header = (dot11_header_t *)&(pPktCtrlBlk->aPktHdr[uHdrAlignPad]);
				COPY_WLAN_WORD(&pDot11Header->qosControl, &tidWord); /* copy with endianess handling. */
			}
		}
	}

	/* Update packet length in the descriptor according to HW interface requirements */
    uLastWordPad = txCtrl_TranslateLengthToFw (pPktCtrlBlk);

	/* Set fields in the descriptor attributes bitmap. */
	uTxDescAttr  = uRatePolicy << TX_ATTR_OFST_RATE_POLICY;
	uTxDescAttr |= pTxCtrl->txSessionCount << TX_ATTR_OFST_SESSION_COUNTER;
    uTxDescAttr |= uLastWordPad << TX_ATTR_OFST_LAST_WORD_PAD;
	uTxDescAttr |= TX_ATTR_TX_CMPLT_REQ;
	if (uHdrAlignPad)
    {
		uTxDescAttr |= TX_ATTR_HEADER_PAD;
    }
	pPktCtrlBlk->tTxDescriptor.txAttr = ENDIAN_HANDLE_WORD(uTxDescAttr); 

    /* Translate packet timestamp to FW time (also updates lifeTime and driverHandlingTime) */
    txCtrl_TranslateTimeToFw (pTxCtrl, pPktCtrlBlk, MGMT_PKT_LIFETIME_TU);

    /* Indicate that the packet is transfered to the FW, and the descriptor fields are in FW format! */
    pPktCtrlBlk->tTxPktParams.uFlags |= TX_CTRL_FLAG_SENT_TO_FW;

#ifdef TI_DBG  
	pTxCtrl->dbgPktSeqNum++;
	pTxCtrl->dbgCounters.dbgNumPktsXfered[uAc]++;	/* Count packets sent to Xfer. */
#endif
}


/***************************************************************************
*                           txCtrl_UpdateHighestAdmittedAcTable 
****************************************************************************
* DESCRIPTION:  This function updates the table that provides for each requested AC  
*				  the highest AC that can be currently used, as follows:
*					If requested AC is admitted use it.
*					If not, find highest AC below it that doesn't require admission.
*				This function should be called opon the following events:
*					1) Init
*					2) ACs admission required change (upon association)
*					3) ACs admission state change (upon association and add/delete Tspec).
*
***************************************************************************/
static void txCtrl_UpdateHighestAdmittedAcTable (txCtrl_t *pTxCtrl)
{
	int			 inputIdx;
	int			 outputIdx;
	EAcTrfcType inputAc;
	EAcTrfcType outputAc; 
        
	/* Loop over all ACs in priority order (BE is higher priority than BK). */
	for (inputIdx = 0; inputIdx < MAX_NUM_OF_AC; inputIdx++)
	{
		inputAc = priorityOrderedAc[inputIdx];

		/* If input AC is admitted, use it. */
		if(pTxCtrl->admissionState[inputAc] == AC_ADMITTED)
			pTxCtrl->highestAdmittedAc[inputAc] = inputAc;

		/* If input AC is not admitted, find next highest priority AC that doesn't require admission. */
		else	
		{
			/* Loop from input AC downward by priority order. */
			for (outputIdx = inputIdx; outputIdx >= 0; outputIdx--)
			{
				outputAc = priorityOrderedAc[outputIdx]; /* Get priority ordered AC. */

				/* Break with first (highest) AC that doesn't require admission (we don't want to 
				 *   redirect traffic to an AC that requires admission even if admitted for other traffic).
				 */
				if(pTxCtrl->admissionRequired[outputAc] == ADMISSION_NOT_REQUIRED)
					break;
			}

			/* If we've found a valid AC insert it, else use BE as default. */
			if (outputIdx >= 0)
				pTxCtrl->highestAdmittedAc[inputAc] = outputAc;
			else
				pTxCtrl->highestAdmittedAc[inputAc] = QOS_AC_BE; 
		}
	}
}


/***************************************************************************
*                           txCtrl_UpdateAcToTidMapping 
****************************************************************************
* DESCRIPTION:  This function updates the table that provides per each AC  
*				  a bitmap of the TIDs that are mapped to it when transitting packets.
*				Note that this mapping considers the ACs admission states.
*				It is used for mapping ACs backpressure to TIDs (for updating the data/mgmt queues)
*
*				This table is updated after txCtrl_UpdateHighestAdmittedAcTable() is called!
*				It may also effect the backpressure picture seen by the Data-Queue and
*				  Mgmt-Queue, so they should be updated subsequently.
*
***************************************************************************/
static void txCtrl_UpdateAcToTidMapping (txCtrl_t *pTxCtrl)
{
	TI_UINT32	tid;
	EAcTrfcType inputAc;
	EAcTrfcType admittedAc; 
        
    os_memoryZero(pTxCtrl->hOs, (void *)&(pTxCtrl->admittedAcToTidMap[0]), sizeof(pTxCtrl->admittedAcToTidMap));

	/* Loop over all TIDs. */
	for (tid = 0; tid < MAX_NUM_OF_802_1d_TAGS; tid++)
	{
		/* Find the AC that is used for transmitting this TID. */
		inputAc = (EAcTrfcType)WMEQosTagToACTable[tid];					/* Standard translation from TID to AC. */
		admittedAc = pTxCtrl->highestAdmittedAc[inputAc];	/* The actual AC that is used for Tx. */

		/* Set the bit related to the TID in the correlated AC. */
		pTxCtrl->admittedAcToTidMap[admittedAc] |= 1 << tid;		
	}
}


/***************************************************************************
*                           txCtrl_UpdateBackpressure 
****************************************************************************
* DESCRIPTION:  This function is called whenever the busy-TIDs bitmap may change, 
*				  (except on packet-xmit - handled separately for performance). 
*				This includes:
*					1) Init
*					2) ACs admission required change (upon association)
*					3) ACs admission state change (upon association and add/delete Tspec).
*					4) Tx-Complete - provides also freed ACs.
*
*				It updates the local bitmap, and the data-queue and mgmt-queue.
*
***************************************************************************/
static void txCtrl_UpdateBackpressure (txCtrl_t *pTxCtrl, TI_UINT32 freedAcBitmap)
{
	TI_UINT32 busyAcBitmap = pTxCtrl->busyAcBitmap;
	TI_UINT32 busyTidBitmap = 0;
	TI_UINT32 ac = 0;

	
	busyAcBitmap &= ~freedAcBitmap;			/* Clear backpressure bits of freed ACs. */
	pTxCtrl->busyAcBitmap = busyAcBitmap;	/* Save new bitmap before manipulating it. */

	/* Loop while there are busy ACs. */
	while (busyAcBitmap)
	{
		/* If the AC is busy, add its related TIDs to the total busy TIDs bitmap. */
		if (busyAcBitmap & 1)
			busyTidBitmap |= pTxCtrl->admittedAcToTidMap[ac];

		/* Move to next AC. */
		busyAcBitmap = busyAcBitmap >> 1;
		ac++;
	}

    TRACE6(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "txCtrl_UpdateBackpressure(): busyTidBitmap = 0x%x, busyAcBitmap = 0x%x, HighestAdmittedAc[3,2,1,0] = %d, %d, %d, %d\n", busyTidBitmap, pTxCtrl->busyAcBitmap, pTxCtrl->highestAdmittedAc[3], 		pTxCtrl->highestAdmittedAc[2], pTxCtrl->highestAdmittedAc[1], pTxCtrl->highestAdmittedAc[0]);

	/* Save new bitmap and update the data-queue and mgmt-queue. */
	pTxCtrl->busyTidBitmap = busyTidBitmap;
	txDataQ_UpdateBusyMap (pTxCtrl->hTxDataQ, busyTidBitmap);
	txMgmtQ_UpdateBusyMap (pTxCtrl->hTxMgmtQ, busyTidBitmap);
}


/****************************************************************************
 *                      txCtrl_SetTxDelayCounters()
 ****************************************************************************
 * DESCRIPTION:          Update transmission path delay counters.
*
* INPUTS:       hTxCtrl - the object
*               ac - the AC to count delay for
*				fwDelay - the time consumed in FW for packet transmission
*				driverDelay - the time consumed in driver for packet transmission
*
* OUTPUT:
*
* RETURNS:
 ****************************************************************************/

#ifdef XCC_MODULE_INCLUDED  /* Needed only for XCC-V4 */

static void txCtrl_SetTxDelayCounters (txCtrl_t *pTxCtrl, 
                                       TI_UINT32 ac, 
                                       TI_UINT32 fwDelay, 
                                       TI_UINT32 driverDelay, 
                                       TI_UINT32 mediumDelay)
{
	int     rangeIndex;
	TI_UINT32  totalTxDelayUsec = fwDelay + driverDelay;

	/* Increment the delay range counter that the current packet Tx delay falls in. */
	for (rangeIndex = TX_DELAY_RANGE_MIN; rangeIndex <= TX_DELAY_RANGE_MAX; rangeIndex++)
	{
		if ( (totalTxDelayUsec >= txDelayRangeStart[rangeIndex]) &&
			 (totalTxDelayUsec <= txDelayRangeEnd  [rangeIndex]) )
		{
			pTxCtrl->txDataCounters[ac].txDelayHistogram[rangeIndex]++;
			break;
		}
	}
	
	/* Update total delay and FW delay sums and packets number for average delay calculation. */
	/* Note: Accumulate Total-Delay in usec to avoid division per packet (convert to msec 
	         only when results are requested by user). */
	if (pTxCtrl->SumTotalDelayUs[ac] < 0x7FFFFFFF) /* verify we are not close to the edge. */
	{
		pTxCtrl->txDataCounters[ac].NumPackets++; 
		pTxCtrl->SumTotalDelayUs[ac] += totalTxDelayUsec;   
        pTxCtrl->txDataCounters[ac].SumFWDelayUs += fwDelay;
        pTxCtrl->txDataCounters[ac].SumMacDelayUs += mediumDelay;
	}
	else  /* If we get close to overflow, restart average accumulation. */
	{
		pTxCtrl->txDataCounters[ac].NumPackets = 1; 
		pTxCtrl->SumTotalDelayUs[ac] = totalTxDelayUsec;   
        pTxCtrl->txDataCounters[ac].SumFWDelayUs = fwDelay;
        pTxCtrl->txDataCounters[ac].SumMacDelayUs = mediumDelay;
	}
}

#endif /* XCC_MODULE_INCLUDED */



/***************************************************************************
*                       txCtrl_UpdateTxCounters                            
****************************************************************************
* DESCRIPTION:  Update Tx statistics counters according to the transmitted packet.
***************************************************************************/
static void txCtrl_UpdateTxCounters (txCtrl_t *pTxCtrl, 
                                     TxResultDescriptor_t *pTxResultInfo,
                                     TTxCtrlBlk *pPktCtrlBlk, 
                                     TI_UINT32 ac, 
                                     TI_BOOL bIsDataPkt)
{
	TI_UINT32 pktLen;
    TI_UINT32 dataLen;
	TI_UINT32 retryHistogramIndex;
    TI_UINT16 EventMask = 0;

    pktLen = (TI_UINT32)ENDIAN_HANDLE_WORD(pPktCtrlBlk->tTxDescriptor.length);
    pktLen = pktLen << 2;

#ifdef TI_DBG

	/* update debug counters. */
	pTxCtrl->dbgCounters.dbgNumTxCmplt[ac]++;
	if (pTxResultInfo->status == TX_SUCCESS) 
	{
		pTxCtrl->dbgCounters.dbgNumTxCmpltOk[ac]++;
		pTxCtrl->dbgCounters.dbgNumTxCmpltOkBytes[ac] += pktLen;
	}
	else
	{
		pTxCtrl->dbgCounters.dbgNumTxCmpltError[ac]++;

		if (pTxResultInfo->status == TX_HW_ERROR        ||
			pTxResultInfo->status == TX_KEY_NOT_FOUND   ||
			pTxResultInfo->status == TX_PEER_NOT_FOUND)
		{
TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, "txCtrl_UpdateTxCounters(): TxResult = %d !!!\n", pTxResultInfo->status);
		}
        else 
        {
TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_WARNING, "txCtrl_UpdateTxCounters(): TxResult = %d !!!\n", pTxResultInfo->status);
        }
	}

#endif /* TI_DBG */

	/* If it's not a data packet, exit (the formal statistics are only on network stack traffic). */
	if ( !bIsDataPkt )
		return;

	if (pTxResultInfo->status == TX_SUCCESS) 
	{
		/* update the retry histogram */
		retryHistogramIndex = (pTxResultInfo->ackFailures >= TX_RETRY_HISTOGRAM_SIZE) ? 
							   (TX_RETRY_HISTOGRAM_SIZE - 1) : pTxResultInfo->ackFailures;
		pTxCtrl->txDataCounters[ac].RetryHistogram[retryHistogramIndex]++;

#ifdef XCC_MODULE_INCLUDED
		/* update delay histogram */
		txCtrl_SetTxDelayCounters (pTxCtrl, 
        						   ac, 
        						   ENDIAN_HANDLE_LONG(pTxResultInfo->fwHandlingTime), 
        						   pPktCtrlBlk->tTxPktParams.uDriverDelay, 
        						   ENDIAN_HANDLE_LONG(pTxResultInfo->mediumDelay));  
#endif

		if (pTxCtrl->headerConverMode == HDR_CONVERT_QOS)
        {
			dataLen = pktLen - (WLAN_WITH_SNAP_QOS_HEADER_MAX_SIZE - ETHERNET_HDR_LEN);
        }
        else
        {
			dataLen = pktLen - (WLAN_WITH_SNAP_HEADER_MAX_SIZE - ETHERNET_HDR_LEN);
        }

        if (pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_MULTICAST)
        {
            if (pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_BROADCAST)
            {
                /* Broadcast frame */
                pTxCtrl->txDataCounters[ac].BroadcastFramesXmit++;
                pTxCtrl->txDataCounters[ac].BroadcastBytesXmit += dataLen;
                EventMask |= BROADCAST_BYTES_XFER;
                EventMask |= BROADCAST_FRAMES_XFER;
            }
            else 
            {
                /* Multicast Address */
                pTxCtrl->txDataCounters[ac].MulticastFramesXmit++;
                pTxCtrl->txDataCounters[ac].MulticastBytesXmit += dataLen;
                EventMask |= MULTICAST_BYTES_XFER;
                EventMask |= MULTICAST_FRAMES_XFER;
            }
        }
        else 
        {
            /* Save last data Tx rate for applications' query */
            EHwBitRate eHwTxRate = ENDIAN_HANDLE_LONG((EHwBitRate)(pTxResultInfo->rate)); 
            rate_PolicyToDrv (eHwTxRate, &pTxCtrl->eCurrentTxRate);

            /* Directed frame statistics */
            pTxCtrl->txDataCounters[ac].DirectedFramesXmit++;
            pTxCtrl->txDataCounters[ac].DirectedBytesXmit += dataLen;
            EventMask |= DIRECTED_BYTES_XFER;
            EventMask |= DIRECTED_FRAMES_XFER;
        }

        pTxCtrl->txDataCounters[ac].XmitOk++;
        EventMask |= XFER_OK;

		/* update the max consecutive retry failures (if needed) */
		if (pTxCtrl->currentConsecutiveRetryFail > pTxCtrl->txDataCounters[ac].MaxConsecutiveRetryFail)
        {
			pTxCtrl->txDataCounters[ac].MaxConsecutiveRetryFail = pTxCtrl->currentConsecutiveRetryFail;
        }
		pTxCtrl->currentConsecutiveRetryFail = 0;

		if(pTxCtrl->TxEventDistributor)
        {
			DistributorMgr_EventCall(pTxCtrl->TxEventDistributor, EventMask, dataLen);
        }
	}
	else	/* Handle Errors */
	{
		/* 
			NOTE: if the FW sets more then 1 error bit  at a time change the error handling
			code below 
		*/
		if (pTxResultInfo->status == TX_RETRY_EXCEEDED) 
		{
			pTxCtrl->txDataCounters[ac].RetryFailCounter++;
			pTxCtrl->currentConsecutiveRetryFail++;
		}
		else if (pTxResultInfo->status == TX_TIMEOUT) 
		{
			pTxCtrl->txDataCounters[ac].TxTimeoutCounter++;
		}
		else
		{
			pTxCtrl->txDataCounters[ac].OtherFailCounter++;
		}
	}
}


/***************************************************************************
*                           txCtrl_notifyFwReset                           *
****************************************************************************
* DESCRIPTION:  Go over all CtrlBlk entries and free the active ones including the packet.
***************************************************************************/
TI_STATUS txCtrl_NotifyFwReset (TI_HANDLE hTxCtrl)
{
	txCtrl_t   *pTxCtrl = (txCtrl_t *)hTxCtrl;
	TI_UINT32  entry;
	TTxCtrlBlk *pPktCtrlBlk;

	pTxCtrl->busyAcBitmap = 0; /* clean busy bitmap */
	txCtrl_UpdateBackpressure(pTxCtrl, 0);

	for (entry = 0; entry < CTRL_BLK_ENTRIES_NUM-1; entry++)
	{
		/* Get packet ctrl-block by desc-ID. */
		pPktCtrlBlk = TWD_txCtrlBlk_GetPointer(pTxCtrl->hTWD, entry);
		if (pPktCtrlBlk->pNextFreeEntry == 0)
		{
		    /* Don't free if the packet still in tx input queues */
		    if ((pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_SENT_TO_FW))
		    {
		        /* Free the packet resources (packet and CtrlBlk)  */
		        txCtrl_FreePacket (pTxCtrl, pPktCtrlBlk, TI_NOK);
		    }
		}
	}

	return TI_OK;
} /* txCtrl_notifyFwReset */


/***************************************************************************
*                           txCtrl_CheckForTxStuck                         *
****************************************************************************
* DESCRIPTION:  Check if there are stale packets in the TxCtrlTable. 
* The criterion for staleness is function of life time (2 times the longest life time)
* Note that only packets that were not sent to the FW are checked for simplicity!
***************************************************************************/
TI_STATUS txCtrl_CheckForTxStuck (TI_HANDLE hTxCtrl)
{
	txCtrl_t   *pTxCtrl = (txCtrl_t *)hTxCtrl;
	TI_UINT32  entry;
	TTxCtrlBlk *pPktCtrlBlk;
	TI_UINT32  uPktAge;		/* Time in uSec since packet start time. */

	for (entry = 0; entry < CTRL_BLK_ENTRIES_NUM-1; entry++)
	{
		/* Get packet ctrl-block by desc-ID. */
		pPktCtrlBlk = TWD_txCtrlBlk_GetPointer(pTxCtrl->hTWD, entry);

        /* If entry is in use */
		if (pPktCtrlBlk->pNextFreeEntry == 0)
		{
            /* If the packet wasn't sent to the FW yet (time is in host format) */
            if ((pPktCtrlBlk->tTxPktParams.uFlags & TX_CTRL_FLAG_SENT_TO_FW) == 0)
            {
                /* If packet age is more than twice the maximum lifetime, return NOK */
                uPktAge = os_timeStampMs (pTxCtrl->hOs) - pPktCtrlBlk->tTxDescriptor.startTime;
                if (uPktAge > ((MGMT_PKT_LIFETIME_TU << SHIFT_BETWEEN_TU_AND_USEC) * 2))
                {
                    return TI_NOK; /* call for recovery */
                }
            }
		}
	}

	return TI_OK;
} /* txCtrl_FailureTest */



