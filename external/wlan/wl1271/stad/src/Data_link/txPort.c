/*
 * txPort.c
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
 *   MODULE:  txPort.c
 *   
 *   PURPOSE: Multiplexes between the management and data queues.
 * 
 *	 DESCRIPTION:  
 *   ============
 * 		The Tx port state machine multiplexes between the management and data queues 
 *		according to the management queues requests.
 *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_62
#include "commonTypes.h"
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "DataCtrl_Api.h"
#include "DrvMainModules.h"


typedef enum
{
    MUX_MGMT_QUEUES,	/* The management queues have access to the Tx path. */
    MUX_DATA_QUEUES		/* The data queues have access to the Tx path. */
} EQueuesMuxState;

typedef enum
{
	QUEUE_ACTION_NONE,
	QUEUE_ACTION_STOP,
	QUEUE_ACTION_WAKE
} EQueueAction;

/* The txPort module object. */
typedef struct 
{
	TI_HANDLE		hOs;
	TI_HANDLE		hReport;
	TI_HANDLE		hTxDataQ;
	TI_HANDLE		hTxMgmtQ;

	EQueuesMuxState queuesMuxState;
	TI_BOOL			txSuspended;
	TI_BOOL			mgmtQueueEnabled;
	TI_BOOL			dataQueueEnabled;
} TTxPortObj;

/* 
 * The txPort local functions:
 */
static void updateQueuesStates(TTxPortObj *pTxPort);

/****************************************************************************
 *                      txPort_Create()
 ****************************************************************************
 * DESCRIPTION:	Create the txPort module object 
 * 
 * INPUTS:	None
 * 
 * OUTPUT:	None
 * 
 * RETURNS:	The Created object
 ****************************************************************************/
TI_HANDLE txPort_create(TI_HANDLE hOs)
{
	TTxPortObj *pTxPort;

	pTxPort = os_memoryAlloc(hOs, sizeof(TTxPortObj));
	if (pTxPort == NULL)
		return NULL;

	os_memoryZero(hOs, pTxPort, sizeof(TTxPortObj));

	pTxPort->hOs = hOs;

	return( (TI_HANDLE)pTxPort );
}


/****************************************************************************
 *                      txPort_unLoad()
 ****************************************************************************
 * DESCRIPTION:	Unload the txPort module object 
 * 
 * INPUTS:	hTxPort - The object to free
 * 
 * OUTPUT:	None
 * 
 * RETURNS:	TI_OK 
 ****************************************************************************/
TI_STATUS txPort_unLoad(TI_HANDLE hTxPort)
{
	TTxPortObj *pTxPort = (TTxPortObj *)hTxPort;

	if (pTxPort)
		os_memoryFree(pTxPort->hOs, pTxPort, sizeof(TTxPortObj));

	return TI_OK;
}


/****************************************************************************
 *                      txPort_init()
 ****************************************************************************
 * DESCRIPTION:	Configure the txPort module object 
 * 
 * INPUTS:	The needed TI handles
 * 
 * OUTPUT:	None
 * 
 * RETURNS:	void
 ****************************************************************************/
void txPort_init (TStadHandlesList *pStadHandles)
{
	TTxPortObj *pTxPort = (TTxPortObj *)(pStadHandles->hTxPort);

	pTxPort->hReport  = pStadHandles->hReport;
	pTxPort->hTxDataQ = pStadHandles->hTxDataQ;
	pTxPort->hTxMgmtQ = pStadHandles->hTxMgmtQ;

	pTxPort->queuesMuxState	  = MUX_MGMT_QUEUES;
	pTxPort->txSuspended	  = TI_FALSE;
	pTxPort->mgmtQueueEnabled = TI_TRUE;
	pTxPort->dataQueueEnabled = TI_FALSE;
}


/****************************************************************************
 *                      txPort_enableData()
 ****************************************************************************
 * DESCRIPTION:	Called by the txMgmtQueue SM when the Tx path CAN be used by the
 *				  data-queues (i.e. it's not needed for mgmt). Update the queues accordingly.
 ****************************************************************************/
void txPort_enableData(TI_HANDLE hTxPort)
{
	TTxPortObj *pTxPort = (TTxPortObj *)hTxPort;

	pTxPort->queuesMuxState = MUX_DATA_QUEUES;
	updateQueuesStates(pTxPort);
} 


/****************************************************************************
 *                      txPort_enableMgmt()
 ****************************************************************************
 * DESCRIPTION:	Called by the txMgmtQueue SM when the Tx path CAN'T be used by the
 *				  data-queues (i.e. it's needed for mgmt). Update the queues accordingly.
 ****************************************************************************/
void txPort_enableMgmt(TI_HANDLE hTxPort)
{
	TTxPortObj *pTxPort = (TTxPortObj *)hTxPort;

	pTxPort->queuesMuxState = MUX_MGMT_QUEUES;
	updateQueuesStates(pTxPort);
}


/****************************************************************************
 *                      txPort_suspendTx()
 ****************************************************************************
 * DESCRIPTION:	Used by STAD applications (e.g. recovery) to temporarily suspend the Tx path.
 ****************************************************************************/ 
void txPort_suspendTx(TI_HANDLE hTxPort)
{
	TTxPortObj *pTxPort = (TTxPortObj *)hTxPort;

	pTxPort->txSuspended = TI_TRUE;
	updateQueuesStates(pTxPort);
}


/****************************************************************************
 *                      txPort_resumeTx()
 ****************************************************************************
 * DESCRIPTION:	Used by STAD applications (e.g. recovery) to resume Tx path after suspended.
 ****************************************************************************/ 
void txPort_resumeTx(TI_HANDLE hTxPort)
{
	TTxPortObj *pTxPort = (TTxPortObj *)hTxPort;

	pTxPort->txSuspended = TI_FALSE;
	updateQueuesStates(pTxPort);
}


/****************************************************************************
 *                      updateQueuesStates()
 ****************************************************************************
 * DESCRIPTION:	 Switch the Data-Queue and Mgmt-Queue Tx on/off (stop/wake)
 *				   according to the current port conditions.
 ****************************************************************************/ 
static void updateQueuesStates (TTxPortObj *pTxPort)
{
	EQueueAction mgmtQueueAction = QUEUE_ACTION_NONE;
	EQueueAction dataQueueAction = QUEUE_ACTION_NONE;

	/* 
	 * If the Tx path is not suspended:
	 */
	if (!pTxPort->txSuspended)
	{
		/* If mgmt-queues should be enabled, set required actions (awake mgmt and stop data if needed). */
		if (pTxPort->queuesMuxState == MUX_MGMT_QUEUES)
		{
			if ( !pTxPort->mgmtQueueEnabled )
				mgmtQueueAction = QUEUE_ACTION_WAKE;
			if ( pTxPort->dataQueueEnabled )
				dataQueueAction = QUEUE_ACTION_STOP;
		}

		/* If data-queues should be enabled, set required actions (stop mgmt and awake data if needed). */
		else
		{
			if ( pTxPort->mgmtQueueEnabled )
				mgmtQueueAction = QUEUE_ACTION_STOP;
			if ( !pTxPort->dataQueueEnabled )
				dataQueueAction = QUEUE_ACTION_WAKE;
		}
	}

	/* 
	 * If the Tx path is not available (Xfer is busy or suspension is requested),
	 *   set required actions (stop mgmt and data if needed).
	 */
	else
	{
		if ( pTxPort->mgmtQueueEnabled )
			mgmtQueueAction = QUEUE_ACTION_STOP;
		if ( pTxPort->dataQueueEnabled )
			dataQueueAction = QUEUE_ACTION_STOP;
	}


#ifdef TI_DBG
	TRACE1(pTxPort->hReport, REPORT_SEVERITY_INFORMATION, ":  queuesMuxState = , TxSuspend = %d\n", pTxPort->txSuspended);
		
	TRACE2(pTxPort->hReport, REPORT_SEVERITY_INFORMATION, ":  PrevMgmtEnabled = %d,  PrevDataEnabled = %d, MgmtAction = , DataAction = \n", pTxPort->mgmtQueueEnabled, pTxPort->dataQueueEnabled);
#endif /* TI_DBG */

	/* 
	 * Execute the required actions. 
	 * Note: This is done at the end of this function because it may start a sequence that will call it again!!
	 *       Always do WAKE action after STOP action, since WAKE may lead to more activities!!
	 */
	if (mgmtQueueAction == QUEUE_ACTION_STOP)
	{
		pTxPort->mgmtQueueEnabled = TI_FALSE;
		txMgmtQ_StopAll (pTxPort->hTxMgmtQ);
	}
	if (dataQueueAction == QUEUE_ACTION_STOP)
	{
		pTxPort->dataQueueEnabled = TI_FALSE;
		txDataQ_StopAll (pTxPort->hTxDataQ);
	}
	if (mgmtQueueAction == QUEUE_ACTION_WAKE)
	{
		pTxPort->mgmtQueueEnabled = TI_TRUE;
		txMgmtQ_WakeAll (pTxPort->hTxMgmtQ);
	}
	if (dataQueueAction == QUEUE_ACTION_WAKE)
	{
		pTxPort->dataQueueEnabled = TI_TRUE;
		txDataQ_WakeAll (pTxPort->hTxDataQ);
	}
}

