/*
 * txDataQueue.c
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


/** \file   txDataQueue.c 
 *  \brief  The Tx Data Queues module.
 *  
 *  \see    txDataQueue.h
 */


#define __FILE_ID__  FILE_ID_60
#include "paramOut.h"
#include "osApi.h"
#include "report.h"
#include "timer.h"
#include "queue.h"
#include "context.h"
#include "Ethernet.h"
#include "TWDriver.h"
#include "DataCtrl_Api.h"
#include "txDataQueue.h"
#include "txCtrl.h"
#include "DrvMainModules.h"
#include "bmtrace_api.h"


/* Internal Functions prototypes */
static void txDataQ_RunScheduler (TI_HANDLE hTxDataQ);
static void txDataQ_UpdateQueuesBusyState (TTxDataQ *pTxDataQ, TI_UINT32 uTidBitMap);
static void txDataQ_TxSendPaceTimeout (TI_HANDLE hTxDataQ, TI_BOOL bTwdInitOccured);
extern void wlanDrvIf_StopTx (TI_HANDLE hOs);
extern void wlanDrvIf_ResumeTx (TI_HANDLE hOs);



/***************************************************************************
*                      PUBLIC  FUNCTIONS  IMPLEMENTATION				   *
****************************************************************************/


/** 
 * \fn     txDataQ_Create
 * \brief  Create the module and its queues
 * 
 * Create the Tx Data module and its queues.
 * 
 * \note   
 * \param  hOs - Handle to the Os Abstraction Layer                           
 * \return Handle to the allocated Tx Data Queue module (NULL if failed) 
 * \sa     
 */ 
TI_HANDLE txDataQ_Create(TI_HANDLE hOs)
{
    TTxDataQ *pTxDataQ;

    /* allocate TxDataQueue module */
    pTxDataQ = os_memoryAlloc (hOs, (sizeof(TTxDataQ)));
	
    if (!pTxDataQ)
	{
        WLAN_OS_REPORT(("Error allocating the TxDataQueue Module\n"));
		return NULL;
	}

    /* Reset TxDataQueue module */
    os_memoryZero (hOs, pTxDataQ, (sizeof(TTxDataQ)));

    return (TI_HANDLE)pTxDataQ;
}


/** 
 * \fn     txDataQ_Init
 * \brief  Save required modules handles
 * 
 * Save other modules handles.
 * 
 * \note   
 * \param  pStadHandles  - The driver modules handles
 * \return void  
 * \sa     
 */ 
void txDataQ_Init (TStadHandlesList *pStadHandles)
{
    TTxDataQ  *pTxDataQ = (TTxDataQ *)(pStadHandles->hTxDataQ);
    TI_UINT32  uNodeHeaderOffset = TI_FIELD_OFFSET(TTxnStruct, tTxnQNode);
    TI_UINT8   uQueId;
	
    /* save modules handles */
    pTxDataQ->hContext	= pStadHandles->hContext;
    pTxDataQ->hTxCtrl	= pStadHandles->hTxCtrl;
    pTxDataQ->hOs		= pStadHandles->hOs;
    pTxDataQ->hReport	= pStadHandles->hReport;
    pTxDataQ->hTxMgmtQ	= pStadHandles->hTxMgmtQ;
    pTxDataQ->hTWD	    = pStadHandles->hTWD;

    /* Configures the Port Default status to Close */
	pTxDataQ->bDataPortEnable = TI_FALSE;

	/* Configures the LastQueId to zero => scheduler will strart from Queue 1*/
	pTxDataQ->uLastQueId = 0;
	
	/* init the number of the Data queue to be used */
	pTxDataQ->uNumQueues = MAX_NUM_OF_AC;

	/* init the max size of the Data queues */
	pTxDataQ->aQueueMaxSize[QOS_AC_BE] = DATA_QUEUE_DEPTH_BE;
	pTxDataQ->aQueueMaxSize[QOS_AC_BK] = DATA_QUEUE_DEPTH_BK;
	pTxDataQ->aQueueMaxSize[QOS_AC_VI] = DATA_QUEUE_DEPTH_VI;
	pTxDataQ->aQueueMaxSize[QOS_AC_VO] = DATA_QUEUE_DEPTH_VO;

    /* Create the tx data queues */
	for (uQueId = 0; uQueId < pTxDataQ->uNumQueues; uQueId++)
    {
        pTxDataQ->aQueues[uQueId] = que_Create (pTxDataQ->hOs, 
                                                pTxDataQ->hReport, 
                                                pTxDataQ->aQueueMaxSize[uQueId], 
                                                uNodeHeaderOffset);
		
		/* If any Queues' allocation failed, print error, free TxDataQueue module and exit */
		if (pTxDataQ->aQueues[uQueId] == NULL)
		{
            TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_CONSOLE , "Failed to create queue\n");
			WLAN_OS_REPORT(("Failed to create queue\n"));
			os_memoryFree (pTxDataQ->hOs, pTxDataQ, sizeof(TTxDataQ));
			return;
		}

		/* Configure the Queues default values */
		pTxDataQ->aQueueBusy[uQueId] = TI_FALSE;   
        pTxDataQ->aNetStackQueueStopped[uQueId] = TI_FALSE;  
        pTxDataQ->aTxSendPaceThresh[uQueId] = 1;
    }

    pTxDataQ->hTxSendPaceTimer = tmr_CreateTimer (pStadHandles->hTimer);
	if (pTxDataQ->hTxSendPaceTimer == NULL)
	{
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "txDataQ_Init(): Failed to create hTxSendPaceTimer!\n");
		return;
	}
    
    /* Register to the context engine and get the client ID */
    pTxDataQ->uContextId = context_RegisterClient (pTxDataQ->hContext,
                                                   txDataQ_RunScheduler,
                                                   (TI_HANDLE)pTxDataQ,
                                                   TI_TRUE,
                                                   "TX_DATA",
                                                   sizeof("TX_DATA"));
}


/** 
 * \fn     txDataQ_SetDefaults
 * \brief  Configure module with default settings
 * 
 * Init the Tx Data queues.
 * Register as the context-engine client.
 * 
 * \note   
 * \param  hTxDataQ - The object                                          
 * \param  Other modules handles                              
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     
 */ 
TI_STATUS txDataQ_SetDefaults (TI_HANDLE  hTxDataQ, txDataInitParams_t *pTxDataInitParams)
{
    TTxDataQ  *pTxDataQ = (TTxDataQ *)hTxDataQ;
	TI_STATUS  eStatus;

    /* configure the classifier sub-module */
    eStatus = txDataClsfr_Config (hTxDataQ, &pTxDataInitParams->ClsfrInitParam);
    if (eStatus != TI_OK)
    {  
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_CONSOLE ,"FATAL ERROR: txDataQ_SetDefaults(): txDataClsfr_Config failed - Aborting\n");
        WLAN_OS_REPORT(("FATAL ERROR: txDataQ_SetDefaults(): txDataClsfr_Config failed - Aborting\n"));
        return eStatus;
    }

    /* Save the module's parameters settings */
	pTxDataQ->bStopNetStackTx              = pTxDataInitParams->bStopNetStackTx;
	pTxDataQ->aTxSendPaceThresh[QOS_AC_BE] = pTxDataInitParams->uTxSendPaceThresh;
	pTxDataQ->aTxSendPaceThresh[QOS_AC_BK] = pTxDataInitParams->uTxSendPaceThresh;
	pTxDataQ->aTxSendPaceThresh[QOS_AC_VI] = pTxDataInitParams->uTxSendPaceThresh;
	pTxDataQ->aTxSendPaceThresh[QOS_AC_VO] = 1;     /* Don't delay voice packts! */

    TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_INIT, ".....Tx Data Queue configured successfully\n");
	
    return TI_OK;
}


/** 
 * \fn     txDataQ_Destroy
 * \brief  Destroy the module and its queues
 * 
 * Clear and destroy the queues and then destroy the module object.
 * 
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return TI_OK - Unload succesfull, TI_NOK - Unload unsuccesfull 
 * \sa     
 */ 
TI_STATUS txDataQ_Destroy (TI_HANDLE hTxDataQ)
{
    TTxDataQ  *pTxDataQ = (TTxDataQ *)hTxDataQ;
    TI_STATUS  status = TI_OK;
    TI_UINT32  uQueId;

    /* Dequeue and free all queued packets */
    txDataQ_ClearQueues (hTxDataQ);

    /* Free Data queues */
    for (uQueId = 0 ; uQueId < pTxDataQ->uNumQueues ; uQueId++)
    {
        if (que_Destroy(pTxDataQ->aQueues[uQueId]) != TI_OK)
		{
            TRACE1(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "txDataQueue_unLoad: fail to free Data Queue number: %d\n",uQueId);
			status = TI_NOK;
		}
    }

    /* free timer */
    if (pTxDataQ->hTxSendPaceTimer)
    {
        tmr_DestroyTimer (pTxDataQ->hTxSendPaceTimer);
    }

    /* Free Tx Data Queue Module */
    os_memoryFree (pTxDataQ->hOs, pTxDataQ, sizeof(TTxDataQ));

    return status;
}


/** 
 * \fn     txDataQ_ClearQueues
 * \brief  Clear all queues
 * 
 * Dequeue and free all queued packets.
 * 
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return void 
 * \sa     
 */ 
void txDataQ_ClearQueues (TI_HANDLE hTxDataQ)
{
    TTxDataQ   *pTxDataQ = (TTxDataQ *)hTxDataQ;
    TTxCtrlBlk *pPktCtrlBlk;
    TI_UINT32  uQueId;

    /* Dequeue and free all queued packets */
    for (uQueId = 0 ; uQueId < pTxDataQ->uNumQueues ; uQueId++)
    {
        do {
            context_EnterCriticalSection (pTxDataQ->hContext);
            pPktCtrlBlk = (TTxCtrlBlk *) que_Dequeue(pTxDataQ->aQueues[uQueId]);
            context_LeaveCriticalSection (pTxDataQ->hContext);
            if (pPktCtrlBlk != NULL) {
                txCtrl_FreePacket (pTxDataQ->hTxCtrl, pPktCtrlBlk, TI_NOK);
            }
        } while (pPktCtrlBlk != NULL);
    }
}


/** 
 * \fn     txDataQ_InsertPacket
 * \brief  Insert packet in queue and schedule task
 * 
 * This function is called by the hard_start_xmit() callback function. 
 * If the packet it an EAPOL, forward it to the Mgmt-Queue.
 * Otherwise, classify the packet, enqueue it and request 
 *   context switch for handling it in the driver's context.
 *
 * \note   
 * \param  hTxDataQ    - The object                                          
 * \param  pPktCtrlBlk - Pointer to the packet                                         
 * \param  uPacketDtag - The packet priority optionaly set by the OAL
 * \return TI_OK - if the packet was queued, TI_NOK - if the packet was dropped. 
 * \sa     txDataQ_Run
 */ 
TI_STATUS txDataQ_InsertPacket (TI_HANDLE hTxDataQ, TTxCtrlBlk *pPktCtrlBlk, TI_UINT8 uPacketDtag)
{
    TTxDataQ        *pTxDataQ = (TTxDataQ *)hTxDataQ;
	TEthernetHeader *pEthHead = (TEthernetHeader *)(pPktCtrlBlk->tTxnStruct.aBuf[0]);
	TI_STATUS        eStatus;
    TI_UINT32        uQueId;
    TI_UINT32        uQueSize;
    txCtrl_t         *pTxCtrl = (txCtrl_t *)(pTxDataQ->hTxCtrl);
    TI_BOOL          bRequestSchedule = TI_FALSE;
    TI_BOOL          bStopNetStack = TI_FALSE;
	CL_TRACE_START_L3();

    /* If packet is EAPOL or from the generic Ethertype, forward it to the Mgmt-Queue and exit */
    if ((HTOWLANS(pEthHead->type) == ETHERTYPE_EAPOL) ||
		(HTOWLANS(pEthHead->type) == pTxCtrl->genericEthertype))
    {
		pPktCtrlBlk->tTxPktParams.uPktType = TX_PKT_TYPE_EAPOL;

        return txMgmtQ_Xmit (pTxDataQ->hTxMgmtQ, pPktCtrlBlk, TI_TRUE);
        /* Note: The last parameter indicates that we are running in external context */
    }
    
    pPktCtrlBlk->tTxPktParams.uPktType = TX_PKT_TYPE_ETHER;

    /* Enter critical section to protect classifier data and queue access */
    context_EnterCriticalSection (pTxDataQ->hContext);

	/* Call the Classify function to set the TID field */
	if (txDataClsfr_ClassifyTxPacket (hTxDataQ, pPktCtrlBlk, uPacketDtag) != TI_OK)
	{
#ifdef TI_DBG
		pTxDataQ->uClsfrMismatchCount++;
TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_WARNING, "txDataQueue_xmit: No matching classifier found \n");
#endif /* TI_DBG */
	}

	/* Enqueue the packet in the appropriate Queue */
    uQueId = aTidToQueueTable[pPktCtrlBlk->tTxDescriptor.tid];
    eStatus = que_Enqueue (pTxDataQ->aQueues[uQueId], (TI_HANDLE)pPktCtrlBlk);

    /* Get number of packets in current queue */
    uQueSize = que_Size (pTxDataQ->aQueues[uQueId]);

    /* If the current queue is not stopped */
    if (pTxDataQ->aQueueBusy[uQueId] == TI_FALSE)
    {
        /* If the queue has the desired number of packets, request switch to driver context for handling them */
        if (uQueSize == pTxDataQ->aTxSendPaceThresh[uQueId])
        {
            tmr_StopTimer (pTxDataQ->hTxSendPaceTimer);
            bRequestSchedule = TI_TRUE;
        }
        /* If below Tx-Send pacing threshold, start timer to trigger packets handling if expired */
        else if (uQueSize < pTxDataQ->aTxSendPaceThresh[uQueId]) 
        {
            tmr_StartTimer (pTxDataQ->hTxSendPaceTimer, 
                            txDataQ_TxSendPaceTimeout, 
                            hTxDataQ, 
                            TX_SEND_PACE_TIMEOUT_MSEC, 
                            TI_FALSE);
        }
    }

    /* If allowed to stop network stack and the queue is full, indicate to stop network and 
          to schedule Tx handling (both are executed below, outside the critical section!) */
	if ((pTxDataQ->bStopNetStackTx) && (uQueSize == pTxDataQ->aQueueMaxSize[uQueId]))
	{
		pTxDataQ->aNetStackQueueStopped[uQueId] = TI_TRUE;
        bRequestSchedule = TI_TRUE;
        bStopNetStack = TI_TRUE;
    }

    /* Leave critical section */
    context_LeaveCriticalSection (pTxDataQ->hContext);

    /* If needed, schedule Tx handling */
	if (bRequestSchedule)
    {
        context_RequestSchedule (pTxDataQ->hContext, pTxDataQ->uContextId);
    }

    /* If needed, stop the network stack Tx */
	if (bStopNetStack)
	{
		/* Stop the network stack from sending Tx packets as we have at least one date queue full.
		Note that in some of the OS's (e.g Win Mobile) it is implemented by blocking the thread*/
		wlanDrvIf_StopTx (pTxDataQ->hOs);
    }

	if (eStatus != TI_OK)
    {
        /* If the packet can't be queued drop it */
        txCtrl_FreePacket (pTxDataQ->hTxCtrl, pPktCtrlBlk, TI_NOK);
#ifdef TI_DBG
		pTxDataQ->aQueueCounters[uQueId].uDroppedPacket++;
#endif /* TI_DBG */
    }
	else
    {
#ifdef TI_DBG
		pTxDataQ->aQueueCounters[uQueId].uEnqueuePacket++;
#endif /* TI_DBG */
    }

    CL_TRACE_END_L3 ("tiwlan_drv.ko", "INHERIT", "TX", "");

    return eStatus;
}


/** 
 * \fn     txDataQ_StopQueue
 * \brief  Set queue's busy indication
 * 
 * This function is called by the txCtrl_xmitData() if the queue's backpressure 
 *   indication is set. 
 * It sets the internal queue's Busy indication.
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \param  uTidBitMap   - The changed TIDs busy bitmap                                          
 * \return void 
 * \sa     txDataQ_UpdateBusyMap 
 */ 
void txDataQ_StopQueue (TI_HANDLE hTxDataQ, TI_UINT32 uTidBitMap)
{
	TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;

	/* Set the relevant queue(s) busy flag */
	txDataQ_UpdateQueuesBusyState (pTxDataQ, uTidBitMap);
}


/** 
 * \fn     txDataQ_UpdateBusyMap
 * \brief  Set queue's busy indication
 * 
 * This function is called by the txCtrl if the backpressure map per TID is changed.
 * This could be as a result of Tx-Complete, admission change or association.
 * The function modifies the internal queue's Busy indication and calls the scheduler.
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \param  uTidBitMap   - The changed TIDs busy bitmap                                          
 * \return void 
 * \sa     txDataQ_StopQueue
 */ 
void txDataQ_UpdateBusyMap (TI_HANDLE hTxDataQ, TI_UINT32 tidBitMap)
{
	TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;
	
	/* Update the Queue(s) mode */
	txDataQ_UpdateQueuesBusyState (pTxDataQ, tidBitMap);

	/* Run the scheduler */
	txDataQ_RunScheduler (hTxDataQ);
}


/** 
 * \fn     txDataQ_StopAll
 * \brief  Disable Data-Queue module access to Tx path.
 * 
 * Called by the Tx-Port when the data-queue module can't access the Tx path.
 * Sets stop-all-queues indication.
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return void 
 * \sa     txDataQ_WakeAll  
 */ 
void txDataQ_StopAll (TI_HANDLE hTxDataQ)
{
    TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;

	/* Disable the data Tx port */
	pTxDataQ->bDataPortEnable = TI_FALSE;
}


/** 
 * \fn     txDataQ_WakeAll
 * \brief  Enable Data-Queue module access to Tx path.
 * 
 * Called by the Tx-Port when the data-queue module can access the Tx path.
 * Clears the stop-all-queues indication and calls the scheduler.
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return void 
 * \sa     txDataQ_StopAll
 */ 
void txDataQ_WakeAll (TI_HANDLE hTxDataQ)
{
    TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;

	/* Enable the data Tx port */
	pTxDataQ->bDataPortEnable = TI_TRUE;

	/* Run the scheduler */
	txDataQ_RunScheduler (hTxDataQ);
}


/***************************************************************************
*                       DEBUG  FUNCTIONS  IMPLEMENTATION			       *
****************************************************************************/

#ifdef TI_DBG

/** 
 * \fn     txDataQ_PrintModuleParams
 * \brief  Print Module Parameters
 * 
 * Print Module Parameters
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return void 
 * \sa     
 */ 
void txDataQ_PrintModuleParams (TI_HANDLE hTxDataQ) 
{
	TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;
	TI_UINT32      qIndex;
	
	WLAN_OS_REPORT(("--------- txDataQueue_printModuleParams ----------\n\n"));
	
	WLAN_OS_REPORT(("bStopNetStackTx = %d\n",pTxDataQ->bStopNetStackTx));
	WLAN_OS_REPORT(("bDataPortEnable = %d\n",pTxDataQ->bDataPortEnable));
	WLAN_OS_REPORT(("uNumQueues      = %d\n",pTxDataQ->uNumQueues));
	WLAN_OS_REPORT(("uLastQueId      = %d\n",pTxDataQ->uLastQueId));
	WLAN_OS_REPORT(("uContextId      = %d\n",pTxDataQ->uContextId));

	for (qIndex = 0; qIndex < pTxDataQ->uNumQueues; qIndex++)
    {
		WLAN_OS_REPORT(("aQueueBusy[%d]            = %d\n", qIndex, pTxDataQ->aQueueBusy[qIndex]));
    }
	for (qIndex = 0; qIndex < pTxDataQ->uNumQueues; qIndex++)
    {
        WLAN_OS_REPORT(("aQueueMaxSize[%d]         = %d\n", qIndex, pTxDataQ->aQueueMaxSize[qIndex]));
    }
	for (qIndex = 0; qIndex < pTxDataQ->uNumQueues; qIndex++)
    {
        WLAN_OS_REPORT(("aTxSendPaceThresh[%d]     = %d\n", qIndex, pTxDataQ->aTxSendPaceThresh[qIndex]));
    }
	for (qIndex = 0; qIndex < pTxDataQ->uNumQueues; qIndex++)
    {
        WLAN_OS_REPORT(("aNetStackQueueStopped[%d] = %d\n", qIndex, pTxDataQ->aNetStackQueueStopped[qIndex]));
    }

	WLAN_OS_REPORT(("-------------- Queues Info -----------------------\n"));
	for (qIndex = 0; qIndex < MAX_NUM_OF_AC; qIndex++)
    {
        WLAN_OS_REPORT(("Que %d:\n", qIndex));
        que_Print (pTxDataQ->aQueues[qIndex]);
    }

	WLAN_OS_REPORT(("--------------------------------------------------\n\n"));
}


/** 
 * \fn     txDataQ_PrintQueueStatistics
 * \brief  Print queues statistics
 * 
 * Print queues statistics
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return void 
 * \sa     
 */ 
void txDataQ_PrintQueueStatistics (TI_HANDLE hTxDataQ)
{
#ifdef REPORT_LOG
    TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;
    TI_UINT32      qIndex;

    WLAN_OS_REPORT(("-------------- txDataQueue_printStatistics -------\n\n"));

	WLAN_OS_REPORT(("uClsfrMismatchCount      = %d\n",pTxDataQ->uClsfrMismatchCount));
    WLAN_OS_REPORT(("uTxSendPaceTimeoutsCount = %d\n",pTxDataQ->uTxSendPaceTimeoutsCount));
	
    WLAN_OS_REPORT(("-------------- Enqueue to queues -----------------\n"));
    for(qIndex = 0; qIndex < MAX_NUM_OF_AC; qIndex++)
        WLAN_OS_REPORT(("Que[%d]: = %d\n",qIndex, pTxDataQ->aQueueCounters[qIndex].uEnqueuePacket));
	
    WLAN_OS_REPORT(("-------------- Dequeue from queues ---------------\n"));
    for(qIndex = 0; qIndex < MAX_NUM_OF_AC; qIndex++)
        WLAN_OS_REPORT(("Que[%d]: = %d\n",qIndex, pTxDataQ->aQueueCounters[qIndex].uDequeuePacket));

    WLAN_OS_REPORT(("-------------- Requeue to queues -----------------\n"));
    for(qIndex = 0; qIndex < MAX_NUM_OF_AC; qIndex++)
        WLAN_OS_REPORT(("Que[%d]: = %d\n",qIndex, pTxDataQ->aQueueCounters[qIndex].uRequeuePacket));

    WLAN_OS_REPORT(("-------------- Sent to TxCtrl --------------------\n"));
    for(qIndex = 0; qIndex < MAX_NUM_OF_AC; qIndex++)
        WLAN_OS_REPORT(("Que[%d]: = %d\n",qIndex, pTxDataQ->aQueueCounters[qIndex].uXmittedPacket));

    WLAN_OS_REPORT(("-------------- Dropped - Queue Full --------------\n"));
    for(qIndex = 0; qIndex < MAX_NUM_OF_AC; qIndex++)
        WLAN_OS_REPORT(("Que[%d]: = %d\n",qIndex, pTxDataQ->aQueueCounters[qIndex].uDroppedPacket));

    WLAN_OS_REPORT(("--------------------------------------------------\n\n"));
#endif
}


/** 
 * \fn     txDataQ_ResetQueueStatistics
 * \brief  Reset queues statistics
 * 
 * Reset queues statistics
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return void 
 * \sa     
 */ 
void txDataQ_ResetQueueStatistics (TI_HANDLE hTxDataQ) 
{
	TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;

    os_memoryZero(pTxDataQ->hOs, &pTxDataQ->aQueueCounters, sizeof(pTxDataQ->aQueueCounters));
    pTxDataQ->uTxSendPaceTimeoutsCount = 0;
}


#endif /* TI_DBG */
	  
		
		  
/***************************************************************************
*                      INTERNAL  FUNCTIONS  IMPLEMENTATION				   *
****************************************************************************/


/** 
 * \fn     txDataQ_RunScheduler
 * \brief  The module's Tx scheduler
 * 
 * This function is the Data-Queue scheduler.
 * It selects a packet to transmit from the tx queues and sends it to the TxCtrl.
 * The queues are selected in a round-robin order.
 * The function is called by one of:
 *     txDataQ_Run()
 *     txDataQ_UpdateBusyMap()
 *     txDataQ_WakeAll()
 *
 * \note   
 * \param  hTxDataQ - The object                                          
 * \return void 
 * \sa     
 */ 
static void txDataQ_RunScheduler (TI_HANDLE hTxDataQ)
{
	TTxDataQ   *pTxDataQ = (TTxDataQ *)hTxDataQ;
	TI_UINT32  uIdleIterationsCount = 0;  /* Count iterations without packet transmission (for exit criteria) */
	TI_UINT32  uQueId = pTxDataQ->uLastQueId;  /* The last iteration queue */
	EStatusXmit eStatus;  /* The return status of the txCtrl_xmitData function */
    TTxCtrlBlk *pPktCtrlBlk; /* Pointer to the packet to be dequeued and sent */

	while(1)
	{
		/* If the Data port is closed or the scheduler couldn't send packets from 
		     all queues, indicate end of current packets burst and exit */
		if ( !pTxDataQ->bDataPortEnable  ||  (uIdleIterationsCount >= pTxDataQ->uNumQueues) )
		{
            TWD_txXfer_EndOfBurst (pTxDataQ->hTWD);
			return;
		}

		/* Selecting the next queue */
		uQueId++;
		if (uQueId == pTxDataQ->uNumQueues)
        {
			uQueId = 0;
        }
		pTxDataQ->uLastQueId = uQueId;
		
		/* Increment the idle iterations counter */
		uIdleIterationsCount++;

		/* If the queue is busy (AC is full), continue to next queue. */
		if (pTxDataQ->aQueueBusy[uQueId])
        {
			continue;
        }

		/* Dequeue a packet in a critical section */
        context_EnterCriticalSection (pTxDataQ->hContext);
		pPktCtrlBlk = (TTxCtrlBlk *) que_Dequeue (pTxDataQ->aQueues[uQueId]);
        context_LeaveCriticalSection (pTxDataQ->hContext);

		/* If the queue was empty, continue to the next queue */
		if (pPktCtrlBlk == NULL)
        {
			if ((pTxDataQ->bStopNetStackTx) && pTxDataQ->aNetStackQueueStopped[uQueId])
			{
				pTxDataQ->aNetStackQueueStopped[uQueId] = TI_FALSE;
				/*Resume the TX process as our date queues are empty*/
				wlanDrvIf_ResumeTx (pTxDataQ->hOs);
			}

			continue;
        }

#ifdef TI_DBG
		pTxDataQ->aQueueCounters[uQueId].uDequeuePacket++;
#endif /* TI_DBG */

		/* Send the packet */
		eStatus = txCtrl_XmitData (pTxDataQ->hTxCtrl, pPktCtrlBlk);

		/* 
         * If the return status is busy it means that the packet was not sent
         *   so we need to requeue it for future try.
         */
		if(eStatus == STATUS_XMIT_BUSY)
		{
            TI_STATUS eQueStatus;

            /* Requeue the packet in a critical section */
            context_EnterCriticalSection (pTxDataQ->hContext);
			eQueStatus = que_Requeue (pTxDataQ->aQueues[uQueId], (TI_HANDLE)pPktCtrlBlk);
            if (eQueStatus != TI_OK) 
            {
                /* If the packet can't be queued drop it */
                /* Note: may happen only if this thread was preempted between the   
                   dequeue and requeue and new packets were inserted into this quque */
                txCtrl_FreePacket (pTxDataQ->hTxCtrl, pPktCtrlBlk, TI_NOK);
#ifdef TI_DBG
                pTxDataQ->aQueueCounters[uQueId].uDroppedPacket++;
#endif /* TI_DBG */
            }
            context_LeaveCriticalSection (pTxDataQ->hContext);

#ifdef TI_DBG
			pTxDataQ->aQueueCounters[uQueId].uRequeuePacket++;
#endif /* TI_DBG */

			continue;
		}

		/* If we reach this point, a packet was sent successfully so reset the idle iterations counter. */
		uIdleIterationsCount = 0;

#ifdef TI_DBG
		pTxDataQ->aQueueCounters[uQueId].uXmittedPacket++;
#endif /* TI_DBG */

	} /* End of while */

	/* Unreachable code */
}


/** 
 * \fn     txDataQ_UpdateQueuesBusyState
 * \brief  Update queues' busy state
 * 
 * Update the Queues Mode to Busy according to the input TidBitMap.
*               Each Tid that is set indicates that the related Queue is Busy. 
*
 * \note   
 * \param  hTxDataQ - The object                                          
 * \param  uTidBitMap   - The changed TIDs busy bitmap                                          
 * \return void 
 * \sa     
 */ 
static void txDataQ_UpdateQueuesBusyState (TTxDataQ *pTxDataQ, TI_UINT32 uTidBitMap)
{
	TI_UINT32 uTidIdx;
	
	/* Go over the TidBitMap and update the related queue busy state */
	for (uTidIdx = 0; uTidIdx < MAX_NUM_OF_802_1d_TAGS; uTidIdx++, uTidBitMap >>= 1)
	{
		if (uTidBitMap & 0x1) /* this Tid is busy */
        {
			pTxDataQ->aQueueBusy[aTidToQueueTable[uTidIdx]] = TI_TRUE;
        }
		else
        {
			pTxDataQ->aQueueBusy[aTidToQueueTable[uTidIdx]] = TI_FALSE;
        }
	}
}


/*
 * \brief   Handle Tx-Send-Pacing timeout.
 * 
 * \param  hTxDataQ        - Module handle
 * \param  bTwdInitOccured - Indicate if TWD restart (recovery) occured
 * \return void
 * 
 * \par Description
 * Call the Tx scheduler to handle the queued packets.
 * 
 * \sa 
 */
static void txDataQ_TxSendPaceTimeout (TI_HANDLE hTxDataQ, TI_BOOL bTwdInitOccured)
{
	TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;

    pTxDataQ->uTxSendPaceTimeoutsCount++;

    txDataQ_RunScheduler (hTxDataQ);
}
