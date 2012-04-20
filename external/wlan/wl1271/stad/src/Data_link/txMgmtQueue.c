/*
 * txMgmtQueue.c
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



/** \file   txMgmtQueue.c 
 *  \brief  The Tx Mgmt Queues module.
 *  
 *	DESCRIPTION:															
 *	============ 													
 *	The Management-Queues module is responsible for the following tasks:	
 *		1.	Queue the driver generated Tx packets, including management,	
 *			EAPOL and null packets until they are transmitted.				
 *			The management packets are buffered in the management-queue,	
 *			and the others in the EAPOL-queue.								
 *		2.	Maintain a state machine that follows the queues state and		
 *			the connection states and enables specific transmission types	
 *			accordingly (e.g. only management).								
 *		3.	Gain access to the Tx path when the management queues are not	
 *			empty, and return the access to the data queues when the		
 *			management queues are empty.									
 *		4.	Schedule packets transmission with strict priority of the		
 *			management queue over the EAPOL queue, and according to the		
 *			backpressure controls from the Port (all queues) and from the	
 *			Tx-Ctrl (per queue). 
 *
 *  \see    txMgmtQueue.h
 */

#define __FILE_ID__  FILE_ID_61
#include "tidef.h"
#include "paramOut.h"
#include "osApi.h"
#include "TWDriver.h"
#include "DataCtrl_Api.h"
#include "report.h"
#include "queue.h"
#include "context.h"
#include "DrvMainModules.h"


#define MGMT_QUEUES_TID		MAX_USER_PRIORITY

typedef enum
{
	QUEUE_TYPE_MGMT,	/* Mgmt-queue  - high-priority, for mgmt packets only. */
	QUEUE_TYPE_EAPOL,	/* EAPOL-queue - low-priority, for other internal packets (EAPOL, NULL, IAPP). */
	NUM_OF_MGMT_QUEUES
} EMgmtQueueTypes;

/* State-Machine Events */
typedef enum
{
	SM_EVENT_CLOSE,			/* All Tx types should be closed. */
	SM_EVENT_MGMT,			/* Allow only mgmt packets. */
	SM_EVENT_EAPOL,			/* Allow mgmt and EAPOL packets. */
	SM_EVENT_OPEN,			/* Allow all packets. */
	SM_EVENT_QUEUES_EMPTY,	/* Mgmt-aQueues are now both empty. */
	SM_EVENT_QUEUES_NOT_EMPTY /* At least one of the Mgmt-aQueues is now not empty. */
} ESmEvent;

/* State-Machine States */
typedef enum
{
	SM_STATE_CLOSE,			/* All Tx path is closed. */
	SM_STATE_MGMT,			/* Only mgmt Tx is permitted. */
	SM_STATE_EAPOL,			/* Only mgmt and EAPOL Tx is permitted. */
	SM_STATE_OPEN_MGMT,		/* All Tx permitted and Mgmt aQueues are currently active (date disabled). */
	SM_STATE_OPEN_DATA		/* All Tx permitted and Data aQueues are currently active (mgmt disabled). */
} ESmState;

/* State-Machine Actions */
typedef enum
{
	SM_ACTION_NULL,				
	SM_ACTION_ENABLE_DATA,		
	SM_ACTION_ENABLE_MGMT,		
	SM_ACTION_RUN_SCHEDULER
} ESmAction;

/* TI_TRUE if both aQueues are empty. */
#define ARE_ALL_MGMT_QUEUES_EMPTY(aQueues)	( (que_Size(aQueues[QUEUE_TYPE_MGMT] ) == 0)  &&  \
											  (que_Size(aQueues[QUEUE_TYPE_EAPOL]) == 0) )

typedef struct
{
	TI_UINT32 aEnqueuePackets[NUM_OF_MGMT_QUEUES];
	TI_UINT32 aDequeuePackets[NUM_OF_MGMT_QUEUES];
	TI_UINT32 aRequeuePackets[NUM_OF_MGMT_QUEUES];
	TI_UINT32 aDroppedPackets[NUM_OF_MGMT_QUEUES];
	TI_UINT32 aXmittedPackets[NUM_OF_MGMT_QUEUES];
} TDbgCount;

/* The module object. */
typedef struct 
{
	/* Handles */
	TI_HANDLE		hOs;
	TI_HANDLE		hReport;
	TI_HANDLE 		hTxCtrl;
	TI_HANDLE 		hTxPort;
	TI_HANDLE 		hContext;
	TI_HANDLE       hTWD;
		
	TI_BOOL			bMgmtPortEnable;/* Port open for mgmt-aQueues or not. */
	ESmState		eSmState;	    /* The current state of the SM. */	
	ETxConnState	eTxConnState;   /* See typedef in module API. */
    TI_UINT32       uContextId;     /* ID allocated to this module on registration to context module */
	
	/* Mgmt aQueues */			
	TI_HANDLE   	aQueues[NUM_OF_MGMT_QUEUES];		   /* The mgmt-aQueues handles. */
    TI_BOOL			aQueueBusy[NUM_OF_MGMT_QUEUES];		   /* Related AC is busy. */
	TI_BOOL			aQueueEnabledBySM[NUM_OF_MGMT_QUEUES]; /* Queue is enabled by the SM. */

	/* Debug Counters */
	TDbgCount		tDbgCounters; /* Save Tx statistics per mgmt-queue. */

} TTxMgmtQ;

/* The module internal functions */
static void mgmtQueuesSM (TTxMgmtQ *pTxMgmtQ, ESmEvent smEvent);
static void runSchedulerNotFromSm (TTxMgmtQ *pTxMgmtQ);
static void runScheduler (TTxMgmtQ *pTxMgmtQ);
static void updateQueuesBusyMap (TTxMgmtQ *pTxMgmtQ, TI_UINT32 tidBitMap);

/*******************************************************************************
*                       PUBLIC  FUNCTIONS  IMPLEMENTATION					   *
********************************************************************************/


/** 
 * \fn     txMgmtQ_Create
 * \brief  Create the module and its queues
 * 
 * Create the Tx Mgmt Queue module and its queues.
 * 
 * \note   
 * \param  hOs - Handle to the Os Abstraction Layer                           
 * \return Handle to the allocated Tx Mgmt Queue module (NULL if failed) 
 * \sa     
 */ 
TI_HANDLE txMgmtQ_Create (TI_HANDLE hOs)
{
    TTxMgmtQ *pTxMgmtQ;

    /* allocate TxMgmtQueue module */
    pTxMgmtQ = os_memoryAlloc (hOs, (sizeof(TTxMgmtQ)));

    if(!pTxMgmtQ)
	{
        WLAN_OS_REPORT(("Error allocating the TxMgmtQueue Module\n"));
		return NULL;
	}

    /* Reset TxMgmtQueue module */
    os_memoryZero (hOs, pTxMgmtQ, (sizeof(TTxMgmtQ)));

    return (TI_HANDLE)pTxMgmtQ;
}


/** 
 * \fn     txMgmtQ_Init
 * \brief  Configure module with default settings
*
 * Get other modules handles.
 * Init the Tx Mgmt queues.
 * Register as the context-engine client.
 * 
 * \note   
 * \param  pStadHandles  - The driver modules handles
 * \return void  
 * \sa     
 */ 
void txMgmtQ_Init (TStadHandlesList *pStadHandles)
{
    TTxMgmtQ  *pTxMgmtQ = (TTxMgmtQ *)(pStadHandles->hTxMgmtQ);
    TI_UINT32  uNodeHeaderOffset = TI_FIELD_OFFSET(TTxnStruct, tTxnQNode);
	int        uQueId;
	
    /* configure modules handles */
    pTxMgmtQ->hOs		= pStadHandles->hOs;
    pTxMgmtQ->hReport	= pStadHandles->hReport;
    pTxMgmtQ->hTxCtrl	= pStadHandles->hTxCtrl;
    pTxMgmtQ->hTxPort	= pStadHandles->hTxPort;
    pTxMgmtQ->hContext	= pStadHandles->hContext;
    pTxMgmtQ->hTWD	    = pStadHandles->hTWD;

	pTxMgmtQ->bMgmtPortEnable = TI_TRUE;	/* Port Default status is open (data-queues are disabled). */
	pTxMgmtQ->eSmState = SM_STATE_CLOSE; /* SM default state is CLOSE. */
	pTxMgmtQ->eTxConnState = TX_CONN_STATE_CLOSE;

    /* initialize tx Mgmt queues */
	for (uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
    {
        pTxMgmtQ->aQueues[uQueId] = que_Create (pTxMgmtQ->hOs, 
                                                pTxMgmtQ->hReport, 
                                                MGMT_QUEUES_DEPTH, 
                                                uNodeHeaderOffset);
		
		/* If any Queues' allocation failed, print error, free TxMgmtQueue module and exit */
		if (pTxMgmtQ->aQueues[uQueId] == NULL)
		{
            TRACE0(pTxMgmtQ->hReport, REPORT_SEVERITY_CONSOLE , "Failed to create queue\n");
			WLAN_OS_REPORT(("Failed to create queue\n"));
			os_memoryFree (pTxMgmtQ->hOs, pTxMgmtQ, sizeof(TTxMgmtQ));
			return;
		}

		pTxMgmtQ->aQueueBusy[uQueId]        = TI_FALSE;	/* aQueueBusy default is not busy. */
		pTxMgmtQ->aQueueEnabledBySM[uQueId] = TI_FALSE; /* Queue is disabled by the SM (state is CLOSE). */
    }

    /* Register to the context engine and get the client ID */
    pTxMgmtQ->uContextId = context_RegisterClient (pTxMgmtQ->hContext,
                                                   txMgmtQ_QueuesNotEmpty,
                                                   (TI_HANDLE)pTxMgmtQ,
                                                   TI_TRUE,
                                                   "TX_MGMT",
                                                   sizeof("TX_MGMT"));

TRACE0(pTxMgmtQ->hReport, REPORT_SEVERITY_INIT, ".....Tx Mgmt Queue configured successfully\n");
}


/** 
 * \fn     txMgmtQ_Destroy
 * \brief  Destroy the module and its queues
 * 
 * Clear and destroy the queues and then destroy the module object.
 * 
 * \note   
 * \param  hTxMgmtQ - The module's object                                          
 * \return TI_OK - Unload succesfull, TI_NOK - Unload unsuccesfull 
 * \sa     
 */ 
TI_STATUS txMgmtQ_Destroy (TI_HANDLE hTxMgmtQ)
{
    TTxMgmtQ  *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;
    TI_STATUS  eStatus = TI_OK;
    int        uQueId;

    /* Dequeue and free all queued packets */
    txMgmtQ_ClearQueues (hTxMgmtQ);

    /* free Mgmt queues */
    for (uQueId = 0 ; uQueId < NUM_OF_MGMT_QUEUES ; uQueId++)
    {
        if (que_Destroy(pTxMgmtQ->aQueues[uQueId]) != TI_OK)
		{
            TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_ERROR, "txMgmtQueue_unLoad: fail to free Mgmt Queue number: %d\n",uQueId);
			eStatus = TI_NOK;
		}
    }

    /* free Tx Mgmt Queue Module */
    os_memoryFree (pTxMgmtQ->hOs, pTxMgmtQ, sizeof(TTxMgmtQ));

    return eStatus;
}


/** 
 * \fn     txMgmtQ_ClearQueues
 * \brief  Clear all queues
 * 
 * Dequeue and free all queued packets.
 * 
 * \note   
 * \param  hTxMgmtQ - The object                                          
 * \return void 
 * \sa     
 */ 
void txMgmtQ_ClearQueues (TI_HANDLE hTxMgmtQ)
{
    TTxMgmtQ   *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;
    TTxCtrlBlk *pPktCtrlBlk;
    TI_UINT32  uQueId;

    /* Dequeue and free all queued packets */
    for (uQueId = 0 ; uQueId < NUM_OF_MGMT_QUEUES ; uQueId++)
    {
        do {
            context_EnterCriticalSection (pTxMgmtQ->hContext);
            pPktCtrlBlk = (TTxCtrlBlk *)que_Dequeue(pTxMgmtQ->aQueues[uQueId]);
            context_LeaveCriticalSection (pTxMgmtQ->hContext);
            if (pPktCtrlBlk != NULL) {
                txCtrl_FreePacket (pTxMgmtQ->hTxCtrl, pPktCtrlBlk, TI_NOK);
            }
        } while (pPktCtrlBlk != NULL);
    }
}


/** 
 * \fn     txMgmtQ_Xmit
 * \brief  Insert non-data packet for transmission
 * 
 * This function is used by the driver applications to send Tx packets other than the 
 *   regular data traffic, including the following packet types:
*				- Management
*				- EAPOL
*				- NULL
*				- IAPP
 * The managment packets are enqueued to the Mgmt-queue and the others to the Eapol-queue.
 * EAPOL packets may be inserted from the network stack context, so it requires switching 
 *   to the driver's context (after the packet is enqueued).
 * If the selected queue was empty before the packet insertion, the SM is called 
 *   with QUEUES_NOT_EMPTY event (in case of external context, only after the context switch).
 *
 * \note   
 * \param  hTxMgmtQ         - The module's object                                          
 * \param  pPktCtrlBlk      - Pointer to the packet CtrlBlk                                         
 * \param  bExternalContext - Indicates if called from non-driver context                                           
 * \return TI_OK - if the packet was queued, TI_NOK - if the packet was dropped. 
 * \sa     txMgmtQ_QueuesNotEmpty
 */ 
TI_STATUS txMgmtQ_Xmit (TI_HANDLE hTxMgmtQ, TTxCtrlBlk *pPktCtrlBlk, TI_BOOL bExternalContext)
{
    TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;
	TI_STATUS eStatus;
	TI_UINT32 uQueId;
    TI_UINT32 uQueSize;

	/* Always set highest TID for mgmt-queues packets. */
	pPktCtrlBlk->tTxDescriptor.tid = MGMT_QUEUES_TID; 

    /* Select queue asccording to the packet type */
	uQueId = (pPktCtrlBlk->tTxPktParams.uPktType == TX_PKT_TYPE_MGMT) ? QUEUE_TYPE_MGMT : QUEUE_TYPE_EAPOL ;

    /* Enter critical section to protect queue access */
    context_EnterCriticalSection (pTxMgmtQ->hContext);

	/* Enqueue the packet in the appropriate Queue */
    eStatus = que_Enqueue (pTxMgmtQ->aQueues[uQueId], (TI_HANDLE)pPktCtrlBlk);

    /* Get number of packets in current queue */
    uQueSize = que_Size (pTxMgmtQ->aQueues[uQueId]);

    /* Leave critical section */
    context_LeaveCriticalSection (pTxMgmtQ->hContext);

	/* If packet enqueued successfully */
	if (eStatus == TI_OK)
	{
		pTxMgmtQ->tDbgCounters.aEnqueuePackets[uQueId]++;

        /* If selected queue was empty before packet insertion */
        if (uQueSize == 1) 
        {
            /* If called from external context (EAPOL from network), request switch to the driver's context. */
            if (bExternalContext) 
            {
                context_RequestSchedule (pTxMgmtQ->hContext, pTxMgmtQ->uContextId);
            }

            /* If already in the driver's context, call the SM with QUEUES_NOT_EMPTY event. */
            else 
            {
                mgmtQueuesSM(pTxMgmtQ, SM_EVENT_QUEUES_NOT_EMPTY);
            }
        }
	}

	else
    {
        /* If the packet can't be queued so drop it */
        txCtrl_FreePacket (pTxMgmtQ->hTxCtrl, pPktCtrlBlk, TI_NOK);
		pTxMgmtQ->tDbgCounters.aDroppedPackets[uQueId]++;
    }

    return eStatus;
}


/** 
 * \fn     txMgmtQ_QueuesNotEmpty
 * \brief  Context-Engine Callback
 * 
 * Context-Engine Callback for processing queues in driver's context.
 * Called after driver's context scheduling was requested in txMgmtQ_Xmit().
 * Calls the SM with QUEUES_NOT_EMPTY event.
 * 
 * \note   
 * \param  hTxMgmtQ - The module's object                                          
 * \return void 
 * \sa     txMgmtQ_Xmit
 */ 
void txMgmtQ_QueuesNotEmpty (TI_HANDLE hTxMgmtQ)
{
    TTxMgmtQ  *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;

    /* Call the SM with QUEUES_NOT_EMPTY event. */
    mgmtQueuesSM(pTxMgmtQ, SM_EVENT_QUEUES_NOT_EMPTY);
}


/** 
 * \fn     txMgmtQ_StopQueue
 * \brief  Context-Engine Callback
 * 
 * This function is called by the txCtrl_xmitMgmt() if the queue's backpressure indication
 *   is set. It sets the internal queue's Busy indication.
 * 
 * \note   
 * \param  hTxMgmtQ   - The module's object                                          
 * \param  uTidBitMap - The busy TIDs bitmap                                          
 * \return void 
 * \sa     txMgmtQ_UpdateBusyMap
 */ 
void txMgmtQ_StopQueue (TI_HANDLE hTxMgmtQ, TI_UINT32 uTidBitMap)
{
	TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;

	/* Update the Queue(s) mode */
	updateQueuesBusyMap (pTxMgmtQ, uTidBitMap);
}


/** 
 * \fn     txMgmtQ_UpdateBusyMap
 * \brief  Update the queues busy map
 * 
 * This function is called by the txCtrl if the backpressure map per TID is changed.
 * This could be as a result of Tx-Complete, admission change or association.
 * The function modifies the internal queues Busy indication and calls the scheduler.
 * 
 * \note   
 * \param  hTxMgmtQ   - The module's object                                          
 * \param  uTidBitMap - The busy TIDs bitmap                                          
 * \return void 
 * \sa     txMgmtQ_StopQueue
 */ 
void txMgmtQ_UpdateBusyMap (TI_HANDLE hTxMgmtQ, TI_UINT32 uTidBitMap)
{
	TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;
	
	/* Update the Queue(s) busy map. */
	updateQueuesBusyMap (pTxMgmtQ, uTidBitMap);

	/* If the queues are not empty, run the scheduler and if they become empty update the SM. */
	runSchedulerNotFromSm (pTxMgmtQ);
}


/** 
 * \fn     txMgmtQ_StopAll
 * \brief  Stop all queues transmission
 * 
 * This function is called by the Tx-Port when the whole Mgmt-queue is stopped.
 * It clears the common queues enable indication.
 * 
 * \note   
 * \param  hTxMgmtQ   - The module's object                                          
 * \return void 
 * \sa     txMgmtQ_WakeAll
 */ 
void txMgmtQ_StopAll (TI_HANDLE hTxMgmtQ)
{
    TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;

	/* Disable the Mgmt Tx port */
	pTxMgmtQ->bMgmtPortEnable = TI_FALSE;
}


/** 
 * \fn     txMgmtQ_WakeAll
 * \brief  Enable all queues transmission
 * 
 * This function is called by the Tx-Port when the whole Mgmt-queue is enabled.
 * It sets the common queues enable indication and calls the scheduler.
 * 
 * \note   
 * \param  hTxMgmtQ   - The module's object                                          
 * \return void 
 * \sa     txMgmtQ_StopAll
 */ 
void txMgmtQ_WakeAll (TI_HANDLE hTxMgmtQ)
{
    TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;

	/* Enable the Mgmt Tx port */
	pTxMgmtQ->bMgmtPortEnable = TI_TRUE;

	/* If the queues are not empty, run the scheduler and if they become empty update the SM. */
	runSchedulerNotFromSm (pTxMgmtQ);
}


/** 
 * \fn     txMgmtQ_SetConnState
 * \brief  Enable all queues transmission
 * 
 * Called by the connection SM and updates the connection state from Tx perspective 
 *   (i.e. which packet types are permitted).
*               Calls the local SM to handle this state change.
*
 * \note   
 * \param  hTxMgmtQ     - The module's object                                          
 * \param  eTxConnState - The new Tx connection state                                          
 * \return void 
 * \sa     mgmtQueuesSM
 */ 
void txMgmtQ_SetConnState (TI_HANDLE hTxMgmtQ, ETxConnState eTxConnState)
{
    TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;

	pTxMgmtQ->eTxConnState = eTxConnState;

	/* Call the SM with the current event. */
	switch (eTxConnState)
	{
		case TX_CONN_STATE_CLOSE:	mgmtQueuesSM(pTxMgmtQ, SM_EVENT_CLOSE);		break;
		case TX_CONN_STATE_MGMT:	mgmtQueuesSM(pTxMgmtQ, SM_EVENT_MGMT);		break;
		case TX_CONN_STATE_EAPOL:	mgmtQueuesSM(pTxMgmtQ, SM_EVENT_EAPOL);		break;
		case TX_CONN_STATE_OPEN:	mgmtQueuesSM(pTxMgmtQ, SM_EVENT_OPEN);		break;

		default:
TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_ERROR, ": Unknown eTxConnState = %d\n", eTxConnState);
	}
}


		
/*******************************************************************************
*                       INTERNAL  FUNCTIONS  IMPLEMENTATION					   *
********************************************************************************/


/** 
 * \fn     mgmtQueuesSM
 * \brief  The module state-machine (static function)
 * 
 * The SM follows the system management states (see ETxConnState) and the Mgmt queues
 *   status (empty or not), and contorls the Tx queues flow accordingly (mgmt and data queues).
 * For detailed explanation, see the Tx-Path LLD document!
 * 
 * \note   To avoid recursion issues, all SM actions are done at the end of the function, 
 *            since some of them may invoke the SM again.
 * \param  pTxMgmtQ - The module's object                                          
 * \param  eSmEvent - The event to act upon                                          
 * \return void 
 * \sa     txMgmtQ_SetConnState
 */ 
static void mgmtQueuesSM (TTxMgmtQ *pTxMgmtQ, ESmEvent eSmEvent)
{
	ESmState  ePrevState = pTxMgmtQ->eSmState;
	ESmAction eSmAction  = SM_ACTION_NULL;

	switch(eSmEvent)
	{
		case SM_EVENT_CLOSE:
			/* 
			 * Tx link is closed (expected in any state), so disable both mgmt queues  
			 *   and if data-queues are active disable them via txPort module.
			 */
			pTxMgmtQ->eSmState = SM_STATE_CLOSE;
			pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_MGMT]  = TI_FALSE;
			pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_EAPOL] = TI_FALSE;
			if (ePrevState == SM_STATE_OPEN_DATA)
				eSmAction = SM_ACTION_ENABLE_MGMT;
			break;

		case SM_EVENT_MGMT:
			/* 
			 * Only Mgmt packets are permitted (expected from any state):
			 *   - Enable the mgmt queue and disable the Eapol queue.
			 *   - If data-queues are active disable them via txPort (this will run the scheduler).
			 *   - Else run the scheduler (to send mgmt packets if waiting).
			 */
			pTxMgmtQ->eSmState = SM_STATE_MGMT;
			pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_MGMT]  = TI_TRUE;
			pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_EAPOL] = TI_FALSE;
			if (ePrevState == SM_STATE_OPEN_DATA)
				eSmAction = SM_ACTION_ENABLE_MGMT;
			else
				eSmAction = SM_ACTION_RUN_SCHEDULER;
			break;

		case SM_EVENT_EAPOL:
			/* 
			 * EAPOL packets are also permitted (expected in MGMT or CLOSE state), so enable the 
			 *   EAPOL queue and run the scheduler (to send packets from EAPOL queue if waiting).
			 */
			if ( (ePrevState != SM_STATE_CLOSE) && (ePrevState != SM_STATE_MGMT) )
			{
TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_ERROR, "mgmtQueuesSM: Got SmEvent=EAPOL when eSmState=%d\n", ePrevState);
			}
			pTxMgmtQ->eSmState = SM_STATE_EAPOL;
			pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_MGMT]  = TI_TRUE;
			pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_EAPOL] = TI_TRUE;
			eSmAction = SM_ACTION_RUN_SCHEDULER;
			break;

		case SM_EVENT_OPEN:
			/* 
			 * All packets are now permitted (expected in EAPOL state), so if the mgmt-queues
			 *   are empty disable them and enable the data queues via txPort module.
			 */
			if (ePrevState != SM_STATE_EAPOL)
			{
TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_ERROR, "mgmtQueuesSM: Got SmEvent=OPEN when eSmState=%d\n", ePrevState);
			}
			if ( ARE_ALL_MGMT_QUEUES_EMPTY(pTxMgmtQ->aQueues) )
			{
				pTxMgmtQ->eSmState = SM_STATE_OPEN_DATA;
				pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_MGMT]  = TI_FALSE;
				pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_EAPOL] = TI_FALSE;
				eSmAction = SM_ACTION_ENABLE_DATA;
			}
			else
			{
				pTxMgmtQ->eSmState = SM_STATE_OPEN_MGMT;
			}
			break;

		case SM_EVENT_QUEUES_EMPTY:
			/* 
			 * The mgmt-queues are empty, so if in OPEN_MGMT state disable the 
			 *   mgmt-queues and enable the data-queues via txPort module.
			 */
			if (ePrevState == SM_STATE_OPEN_MGMT)
			{
				pTxMgmtQ->eSmState = SM_STATE_OPEN_DATA;
				pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_MGMT]  = TI_FALSE;
				pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_EAPOL] = TI_FALSE;
				eSmAction = SM_ACTION_ENABLE_DATA;
			}
			else
			{
				/* This may happen so it's just a warning and not an error. */
TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_WARNING, "mgmtQueuesSM: Got SmEvent=QUEUES_EMPTY when eSmState=%d\n", ePrevState);
			}
			break;

		case SM_EVENT_QUEUES_NOT_EMPTY:

			/* A packet was inserted to the mgmt-queues */

			/* 
			 * If in OPEN_DATA state, enable mgmt-queues and disable data-queues via txPort module.
			 *
			 * Note: The scheduler is not run here because the txPort will call
			 *   txMgmtQueue_wakeAll() which will run the scheduler.
			 */
			if (ePrevState == SM_STATE_OPEN_DATA)
			{
				pTxMgmtQ->eSmState = SM_STATE_OPEN_MGMT;
				pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_MGMT]  = TI_TRUE;
				pTxMgmtQ->aQueueEnabledBySM[QUEUE_TYPE_EAPOL] = TI_TRUE;
				eSmAction = SM_ACTION_ENABLE_MGMT;
			}

			/* 
			 * If in MGMT or EAPOL state, run the scheduler to transmit the packet.
			 */
			else if ( (ePrevState == SM_STATE_MGMT) || (ePrevState == SM_STATE_EAPOL) )
			{
				eSmAction = SM_ACTION_RUN_SCHEDULER;
			}

			else
			{
				/* This may happen so it's just a warning and not an error. */
TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_WARNING, "mgmtQueuesSM: Got SmEvent=QUEUES_NOT_EMPTY when eSmState=%d\n", ePrevState);
			}
			break;

		default:
TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_ERROR, "mgmtQueuesSM: Unknown SmEvent = %d\n", eSmEvent);
			break;
	}

TRACE6( pTxMgmtQ->hReport, REPORT_SEVERITY_INFORMATION, "mgmtQueuesSM: <currentState = %d, event = %d> --> nextState = %d, action = %d, MgmtQueEnbl=%d, EapolQueEnbl=%d\n", ePrevState, eSmEvent, pTxMgmtQ->eSmState, eSmAction, pTxMgmtQ->aQueueEnabledBySM[0], pTxMgmtQ->aQueueEnabledBySM[1]);

	/* 
	 * Execute the required action. 
	 * Note: This is done at the end of the SM because it may start a sequence that will call the SM again!
	 */
	switch (eSmAction)
	{
		case SM_ACTION_NULL:
			break;

		case SM_ACTION_ENABLE_DATA:
			txPort_enableData(pTxMgmtQ->hTxPort);
			break;

		case SM_ACTION_ENABLE_MGMT:
			txPort_enableMgmt(pTxMgmtQ->hTxPort);
			break;

		case SM_ACTION_RUN_SCHEDULER:
			runScheduler(pTxMgmtQ);
			break;

		default:
TRACE1(pTxMgmtQ->hReport, REPORT_SEVERITY_ERROR, ": Unknown SmAction = %d\n", eSmAction);
			break;
	}
}


/** 
 * \fn     runSchedulerNotFromSm
 * \brief  Run scheduler due to other events then from SM (static function)
 * 
 * To comply with the SM behavior, this function is used for any case where the
 *    Mgmt-Queues scheduler may have work to do due to events external to the SM.
 * If the queues are not empty, this function runs the scheduler.
*				If the scheduler emptied the queues, update the SM.
 * 
 * \note   
 * \param  pTxMgmtQ - The module's object                                          
 * \return void 
 * \sa     
 */ 
static void runSchedulerNotFromSm (TTxMgmtQ *pTxMgmtQ)
{
	/* If the queues are not empty, run the scheduler. */
	if ( !ARE_ALL_MGMT_QUEUES_EMPTY(pTxMgmtQ->aQueues) )
	{
		runScheduler (pTxMgmtQ);
		
		/* If the queues are now both empty, call the SM with QUEUES_EMPTY event. */
		if ( ARE_ALL_MGMT_QUEUES_EMPTY(pTxMgmtQ->aQueues) )
        {
			mgmtQueuesSM (pTxMgmtQ, SM_EVENT_QUEUES_EMPTY);
        }
	}
}


/** 
 * \fn     runScheduler
 * \brief  The scheduler processing (static function)
 * 
 * Loops over the mgmt-queues (high priority first) and if queue enabled and
 *   has packets, dequeue a packet and send it to the TxCtrl.
*				Exit if the port level is disabled or if couldn't send anything from both queues.
 * 
 * \note   Protect the queues access against preemption from external context (EAPOL).
 * \param  pTxMgmtQ - The module's object                                          
 * \return void 
 * \sa     
 */ 
static void runScheduler (TTxMgmtQ *pTxMgmtQ)
{
	TI_STATUS  eStatus;
    TTxCtrlBlk *pPktCtrlBlk;
	TI_UINT32  uQueId = 0; /* start from highest priority queue */

	while(1)
	{
		/* If the Mgmt port is closed exit. */
		if ( !pTxMgmtQ->bMgmtPortEnable )
		{
			return;
		}

		/* Check that the current queue is not busy and is enabled by the state-machine. */
		if ( !pTxMgmtQ->aQueueBusy[uQueId]  &&  pTxMgmtQ->aQueueEnabledBySM[uQueId])
		{
            /* Dequeue a packet in a critical section */
            context_EnterCriticalSection (pTxMgmtQ->hContext);
            pPktCtrlBlk = (TTxCtrlBlk *) que_Dequeue (pTxMgmtQ->aQueues[uQueId]);
            context_LeaveCriticalSection (pTxMgmtQ->hContext);

			if (pPktCtrlBlk)
			{
				pTxMgmtQ->tDbgCounters.aDequeuePackets[uQueId]++;

				/* Send the packet */
				eStatus = txCtrl_XmitMgmt (pTxMgmtQ->hTxCtrl, pPktCtrlBlk);

				/* In case the return status is busy it means that the packet wasn't handled
					 so we need to requeue the packet for future try. */
				if(eStatus == STATUS_XMIT_BUSY)
				{
                    /* Requeue the packet in a critical section */
                    context_EnterCriticalSection (pTxMgmtQ->hContext);
                    que_Requeue (pTxMgmtQ->aQueues[uQueId], (TI_HANDLE)pPktCtrlBlk);
                    context_LeaveCriticalSection (pTxMgmtQ->hContext);

                    pTxMgmtQ->tDbgCounters.aRequeuePackets[uQueId]++;
				}

				/* The packet was handled by the lower Tx layers. */
				else
				{
					pTxMgmtQ->tDbgCounters.aXmittedPackets[uQueId]++;

					/* Successful delivery so start next tx from the high priority queue (mgmt),
					 *	 giving it strict priority over the lower queue. 
					 */
					uQueId = 0;
					continue;
				}
			}
		}


		/* If we got here we couldn't deliver a packet from current queue, so progress to lower 
		 *	 priority queue and if already in lowest queue exit. 
		 */
		uQueId++;
		if (uQueId < NUM_OF_MGMT_QUEUES)
        {
			continue;	/* Try sending from next queue (i.e. the EAPOL queue). */
        }
		else
		{
            /* We couldn't send from both queues so indicate end of packets burst and exit. */
            TWD_txXfer_EndOfBurst (pTxMgmtQ->hTWD);
			return;		
		}

	} /* End of while */

	/* Unreachable code */
}


/** 
 * \fn     updateQueuesBusyMap
 * \brief  Update queues busy map (static function)
 * 
 * Set the queues busy indication on or off according to the highest TID bit 
 *    in the tidBitMap (1 = busy). 
*				Note that both Mgmt and Eapol queues are mapped to TID 7.
*
 * \note   
 * \param  pTxMgmtQ   - The module's object                                          
 * \param  uTidBitMap - The TIDs bitmap of the queue(s) to update                                          
 * \return void 
 * \sa     
 */ 
static void updateQueuesBusyMap (TTxMgmtQ *pTxMgmtQ, TI_UINT32 uTidBitMap)
{
	/* Set the queues busy indication on or off according to the highest TID bit (1 = busy). */
	if(uTidBitMap & (1 << MGMT_QUEUES_TID) )
	{
		pTxMgmtQ->aQueueBusy[QUEUE_TYPE_MGMT ] = TI_TRUE;
		pTxMgmtQ->aQueueBusy[QUEUE_TYPE_EAPOL] = TI_TRUE;
	}
	else
	{
		pTxMgmtQ->aQueueBusy[QUEUE_TYPE_MGMT ] = TI_FALSE;
		pTxMgmtQ->aQueueBusy[QUEUE_TYPE_EAPOL] = TI_FALSE;
	}
}



/*******************************************************************************
*                       DEBUG  FUNCTIONS  IMPLEMENTATION					   *
********************************************************************************/

#ifdef TI_DBG

/** 
 * \fn     txMgmtQ_PrintModuleParams
 * \brief  Print module's parameters (debug)
 * 
 * This function prints the module's parameters.
 * 
 * \note   
 * \param  hTxMgmtQ - The module's object                                          
 * \return void 
 * \sa     
 */ 
void txMgmtQ_PrintModuleParams (TI_HANDLE hTxMgmtQ) 
{
	TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;
	TI_UINT32 uQueId;
	
	WLAN_OS_REPORT(("-------------- txMgmtQueue Module Params -----------------\n"));
	WLAN_OS_REPORT(("==========================================================\n"));
	
	WLAN_OS_REPORT(("eSmState        = %d\n", pTxMgmtQ->eSmState));
	WLAN_OS_REPORT(("bMgmtPortEnable = %d\n", pTxMgmtQ->bMgmtPortEnable));
	WLAN_OS_REPORT(("eTxConnState    = %d\n", pTxMgmtQ->eTxConnState));
	WLAN_OS_REPORT(("uContextId      = %d\n", pTxMgmtQ->uContextId));

	WLAN_OS_REPORT(("-------------- Queues Busy (in HW) -----------------------\n"));
	for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
    {
        WLAN_OS_REPORT(("Que[%d]:  %d\n", uQueId, pTxMgmtQ->aQueueBusy[uQueId]));
    }

	WLAN_OS_REPORT(("-------------- Queues Enabled By SM ----------------------\n"));
	for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
    {
        WLAN_OS_REPORT(("Que[%d]:  %d\n", uQueId, pTxMgmtQ->aQueueBusy[uQueId]));
    }

	WLAN_OS_REPORT(("-------------- Queues Info -------------------------------\n"));
	for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
    {
        WLAN_OS_REPORT(("Que %d:\n", uQueId));
        que_Print (pTxMgmtQ->aQueues[uQueId]);
    }

	WLAN_OS_REPORT(("==========================================================\n\n"));
}


/** 
 * \fn     txMgmtQ_PrintQueueStatistics
 * \brief  Print queues statistics (debug)
 * 
 * This function prints the module's Tx statistics per Queue.
 * 
 * \note   
 * \param  hTxMgmtQ - The module's object                                          
 * \return void 
 * \sa     
 */ 
void txMgmtQ_PrintQueueStatistics (TI_HANDLE hTxMgmtQ)
{
#ifdef REPORT_LOG
    TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;
    TI_UINT32 uQueId;

    WLAN_OS_REPORT(("-------------- Mgmt Queues Statistics  -------------------\n"));
    WLAN_OS_REPORT(("==========================================================\n"));

    WLAN_OS_REPORT(("-------------- Enqueue Packets ---------------------------\n"));
    for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
        WLAN_OS_REPORT(("Que[%d]:  %d\n", uQueId, pTxMgmtQ->tDbgCounters.aEnqueuePackets[uQueId]));
	
    WLAN_OS_REPORT(("-------------- Dequeue Packets ---------------------------\n"));
    for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
        WLAN_OS_REPORT(("Que[%d]:  %d\n", uQueId, pTxMgmtQ->tDbgCounters.aDequeuePackets[uQueId]));

    WLAN_OS_REPORT(("-------------- Requeue Packets ---------------------------\n"));
    for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
        WLAN_OS_REPORT(("Que[%d]:  %d\n", uQueId, pTxMgmtQ->tDbgCounters.aRequeuePackets[uQueId]));

    WLAN_OS_REPORT(("-------------- Xmitted Packets ---------------------------\n"));
    for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
        WLAN_OS_REPORT(("Que[%d]:  %d\n", uQueId, pTxMgmtQ->tDbgCounters.aXmittedPackets[uQueId]));

    WLAN_OS_REPORT(("-------------- Dropped Packets (queue full) --------------\n"));
    for(uQueId = 0; uQueId < NUM_OF_MGMT_QUEUES; uQueId++)
        WLAN_OS_REPORT(("Que[%d]:  %d\n", uQueId, pTxMgmtQ->tDbgCounters.aDroppedPackets[uQueId]));

    WLAN_OS_REPORT(("==========================================================\n\n"));
#endif    
}


/** 
 * \fn     txMgmtQ_ResetQueueStatistics
 * \brief  Reset queues statistics (debug)
 * 
 * This function Resets the module's Tx statistics per Queue.
 * 
 * \note   
 * \param  hTxMgmtQ - The module's object                                          
 * \return void 
 * \sa     
 */ 
void txMgmtQ_ResetQueueStatistics (TI_HANDLE hTxMgmtQ)
{
	TTxMgmtQ *pTxMgmtQ = (TTxMgmtQ *)hTxMgmtQ;

    os_memoryZero(pTxMgmtQ->hOs, (void *)&(pTxMgmtQ->tDbgCounters), sizeof(TDbgCount));
}

#endif /* TI_DBG */
	  
