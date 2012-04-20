/*
 * trafficAdmControl.c
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


/***************************************************************************/
/*																		   */
/*	  MODULE:	admCtrlQos.c											   */
/*    PURPOSE:	WSM/WME admission Control							       */
/*																	 	   */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_89
#include "osApi.h"

#include "paramOut.h"

#include "timer.h"
#include "fsm.h"
#include "report.h"

#include "DataCtrl_Api.h"

#include "trafficAdmControl.h"
#include "qosMngr_API.h"
#include "TWDriver.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif
/* Constants */

/** number of states in the state machine */
#define	TRAFFIC_ADM_CTRL_SM_NUM_STATES		2

/** number of events in the state machine */
#define	TRAFFIC_ADM_CTRL_SM_NUM_EVENTS			5

extern int WMEQosTagToACTable[MAX_NUM_OF_802_1d_TAGS];

typedef struct 
{
	TI_HANDLE hTrafficAdmCtrl;
	tspecInfo_t *pTSpecInfo;
	TI_UINT8		acID;

}fsmTSpecInfo_t;


/* Timer functions */
void trafficAdmCtrl_timeoutAcBE(TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured);
void trafficAdmCtrl_timeoutAcBK(TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured);
void trafficAdmCtrl_timeoutAcVI(TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured);
void trafficAdmCtrl_timeoutAcVO(TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured);


/* SM Functions */
TI_STATUS trafficAdmCtrl_smEvent(trafficAdmCtrl_t *pAdmCtrlQos, TI_UINT8 event, void *pData);

TI_STATUS trafficAdmCtrl_smActionUnexpectedTspecResponse(fsmTSpecInfo_t *fsmTSpecInfo);	/*unxcepted*/
TI_STATUS trafficAdmCtrl_smActionUnexpected(fsmTSpecInfo_t *fsmTSpecInfo);	/*unxcepted*/
TI_STATUS trafficAdmCtrl_smActionNop(fsmTSpecInfo_t *fsmTSpecInfo);			/*NOP*/
TI_STATUS trafficAdmCtrl_smStart(fsmTSpecInfo_t *fsmTSpecInfo);				/*EVENT_START*/
TI_STATUS trafficAdmCtrl_smWaitStop(fsmTSpecInfo_t *fsmTSpecInfo);			/*EVENT_STOP*/
TI_STATUS trafficAdmCtrl_smWaitAccept(fsmTSpecInfo_t *fsmTSpecInfo);		/*EVENT_ACCEPT*/
TI_STATUS trafficAdmCtrl_smWaitReject(fsmTSpecInfo_t *fsmTSpecInfo);		/*EVENT_REJECT*/
TI_STATUS trafficAdmCtrl_smWaitTimeout(fsmTSpecInfo_t *fsmTSpecInfo);		/*EVENT_TIMEOUT*/



TI_STATUS trafficAdmCtrl_sendAdmissionReq(TI_HANDLE hTrafficAdmCtrl, tspecInfo_t *pTSpecInfo);
TI_STATUS trafficAdmCtrl_startTimer(trafficAdmCtrl_t* pTrafficAdmCtrl, TI_UINT8 acID);
TI_STATUS trafficAdmCtrl_stopTimer(trafficAdmCtrl_t* pTrafficAdmCtrl, TI_UINT8 acID);


TI_STATUS trafficAdmCtrl_buildFrameHeader(trafficAdmCtrl_t *pTrafficAdmCtrl, TTxCtrlBlk *pPktCtrlBlk);

static TI_STATUS trafficAdmCtrl_tokenToAc (TI_HANDLE hTrafficAdmCtrl, TI_UINT8 token, TI_UINT8 *acID);

/********************************************************************************
 *							trafficAdmCtrl_create								*
 ********************************************************************************
DESCRIPTION: trafficAdmCtrl module creation function

  INPUT:      hOs -			Handle to OS		


OUTPUT:		

RETURN:     Handle to the trafficAdmCtrl module on success, NULL otherwise

************************************************************************/

TI_HANDLE trafficAdmCtrl_create(TI_HANDLE hOs)
{
	trafficAdmCtrl_t 		*pTrafficAdmCtrl;
	TI_STATUS			status;

	/* allocate admission control context memory */
	pTrafficAdmCtrl = (trafficAdmCtrl_t*)os_memoryAlloc(hOs, sizeof(trafficAdmCtrl_t));
	if (pTrafficAdmCtrl == NULL)
	{
		return NULL;
	}

	os_memoryZero(hOs, pTrafficAdmCtrl, sizeof(trafficAdmCtrl_t));

	pTrafficAdmCtrl->hOs = hOs;

	/* allocate memory for admCtrlQos state machine */
	status = fsm_Create(hOs, &pTrafficAdmCtrl->pTrafficAdmCtrlSm, TRAFFIC_ADM_CTRL_SM_NUM_STATES, TRAFFIC_ADM_CTRL_SM_NUM_EVENTS);
	if (status != TI_OK)
	{
		os_memoryFree(hOs, pTrafficAdmCtrl, sizeof(trafficAdmCtrl_t));
		return NULL;
	}

	return pTrafficAdmCtrl;
}
/************************************************************************
 *                        trafficAdmCtrl_unload						    *
 ************************************************************************
DESCRIPTION: trafficAdmCtrl module destroy function, 
				-	Free all memory alocated by the module
				
INPUT:      hTrafficAdmCtrl	-	trafficAdmCtrl handle.		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS trafficAdmCtrl_unload(TI_HANDLE hTrafficAdmCtrl)
{
	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;
	TI_UINT32         uAcId;

    if (pTrafficAdmCtrl->pTrafficAdmCtrlSm)
	{
        fsm_Unload (pTrafficAdmCtrl->hOs, pTrafficAdmCtrl->pTrafficAdmCtrlSm);
	}
	
	/* free timers */
	for (uAcId = 0; uAcId < MAX_NUM_OF_AC; uAcId++)
    {
        if (pTrafficAdmCtrl->hAdmCtrlTimer[uAcId]) 
        {
            tmr_DestroyTimer (pTrafficAdmCtrl->hAdmCtrlTimer[uAcId]);
        }
    }

	os_memoryFree(pTrafficAdmCtrl->hOs, pTrafficAdmCtrl, sizeof(trafficAdmCtrl_t));

	return TI_OK;
}

/************************************************************************
 *                        trafficAdmCtrl_config							*
 ************************************************************************
DESCRIPTION: trafficAdmCtrl module configuration function, 
				performs the following:
				-	Reset & initiailzes local variables
				-	Init the handles to be used by the module
                                                                                                   
INPUT:      hTrafficAdmCtrl	         -	trafficAdmCtrl handle.
		    List of handles to be used by the module
			pTrafficAdmCtrlInitParams	-	init parameters.		
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS trafficAdmCtrl_config (TI_HANDLE hTrafficAdmCtrl,
    						     TI_HANDLE hTxMgmtQ,
    						     TI_HANDLE hReport,
    						     TI_HANDLE hOs,
    						     TI_HANDLE hQosMngr,
    						     TI_HANDLE hCtrlData,
    						     TI_HANDLE hXCCMgr,
    						     TI_HANDLE hTimer,
    						     TI_HANDLE hTWD,
                                 TI_HANDLE hTxCtrl,
    						     trafficAdmCtrlInitParams_t *pTrafficAdmCtrlInitParams)
{
	trafficAdmCtrl_t	*pTrafficAdmCtrl;
	TI_STATUS			status;
	TI_UINT32      		uAcId;

	fsm_actionCell_t	trafficAdmCtrl_smMatrix[TRAFFIC_ADM_CTRL_SM_NUM_STATES][TRAFFIC_ADM_CTRL_SM_NUM_EVENTS] =
	{
		/* next state and actions for IDLE state */
		{{TRAFFIC_ADM_CTRL_SM_STATE_WAIT, (fsm_Action_t)trafficAdmCtrl_smStart},			/*EVENT_START*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smActionNop},		/*EVENT_STOP*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smActionUnexpectedTspecResponse}, /*EVENT_ACCEPT*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smActionUnexpectedTspecResponse}, /*EVENT_REJECT*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smActionUnexpected}, /*EVENT_TIMEOUT*/
		},
		/* next state and actions for WAIT state */
		{{TRAFFIC_ADM_CTRL_SM_STATE_WAIT, (fsm_Action_t)trafficAdmCtrl_smActionUnexpected},	/*EVENT_START*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smWaitStop},			/*EVENT_STOP*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smWaitAccept},		/*EVENT_ACCEPT*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smWaitReject},		/*EVENT_REJECT*/
		 {TRAFFIC_ADM_CTRL_SM_STATE_IDLE, (fsm_Action_t)trafficAdmCtrl_smWaitTimeout},		/*EVENT_TIMEOUT*/
		},
	};
	
	pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	pTrafficAdmCtrl->hTxMgmtQ = hTxMgmtQ;
	pTrafficAdmCtrl->hReport	= hReport;
	pTrafficAdmCtrl->hOs		= hOs;
	pTrafficAdmCtrl->hQosMngr	= hQosMngr;
	pTrafficAdmCtrl->hCtrlData	= hCtrlData;
	pTrafficAdmCtrl->hXCCMgr	= hXCCMgr;
	pTrafficAdmCtrl->hTimer	    = hTimer;
	pTrafficAdmCtrl->hTWD	    = hTWD;
	pTrafficAdmCtrl->hTxCtrl	= hTxCtrl;

	for (uAcId = 0; uAcId < MAX_NUM_OF_AC; uAcId++)
    {
		pTrafficAdmCtrl->dialogToken[uAcId] = 0;

        /* Create per AC timers */
        pTrafficAdmCtrl->hAdmCtrlTimer[uAcId] = tmr_CreateTimer (pTrafficAdmCtrl->hTimer);
        if (pTrafficAdmCtrl->hAdmCtrlTimer[uAcId] == NULL)
        {
            TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "trafficAdmCtrl_config(): Failed to create hAssocSmTimer!\n");
            return TI_NOK;
        }
    }

    /* configure state machine */
	status = fsm_Config(pTrafficAdmCtrl->pTrafficAdmCtrlSm, &trafficAdmCtrl_smMatrix[0][0], 
						TRAFFIC_ADM_CTRL_SM_NUM_STATES, TRAFFIC_ADM_CTRL_SM_NUM_EVENTS, NULL, hOs);
	if (status != TI_OK)
	{
TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "TRAFFIC_ADM_CTRL_SM: fsm_Config - Error  \n");

		return TI_NOK;
	}

	pTrafficAdmCtrl->timeout =  pTrafficAdmCtrlInitParams->trafficAdmCtrlResponseTimeout;
    pTrafficAdmCtrl->useFixedMsduSize = pTrafficAdmCtrlInitParams->trafficAdmCtrlUseFixedMsduSize;

	pTrafficAdmCtrl->dialogTokenCounter = INITIAL_DIALOG_TOKEN;
	

TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "TRAFFIC ADM CTRL -  configuration completed ..... \n");

	return TI_OK;
}


/************************************************************************
 *                        trafficAdmCtrl_smEvent						*
 ************************************************************************
DESCRIPTION: trafficAdmCtrl SM general function
                                                                                                   
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
			event				-	the event to the SM.
			pData				-	handle to passing parameter			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smEvent(trafficAdmCtrl_t *pTrafficAdmCtrl, TI_UINT8 event, void *pData)
{
	TI_STATUS 		status;
	TI_UINT8			nextState;
	fsmTSpecInfo_t	*fsmTSpecInfo = (fsmTSpecInfo_t*)pData;
	TI_UINT8			acID = fsmTSpecInfo->acID;

    /* It looks like it never happens. Anyway decided to check */
    if ( acID >= MAX_NUM_OF_AC )
    {
        TRACE2( pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR,
               "trafficAdmCtrl_smEvent. fsmTSpecInfo->acID=%d exceeds the limit %d\n",
                   acID, MAX_NUM_OF_AC-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return TI_NOK;
    }

	status = fsm_GetNextState(pTrafficAdmCtrl->pTrafficAdmCtrlSm, pTrafficAdmCtrl->currentState[acID], event, &nextState);
	if (status != TI_OK)
	{
TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "ADM_CTRL: ERROR - failed getting next state \n");

		return(TI_NOK);
	}

	TRACE3(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "trafficAdmCtrl_smEvent: <currentState = %d, event = %d> --> nextState = %d\n", pTrafficAdmCtrl->currentState[acID], event, nextState);

	status = fsm_Event(pTrafficAdmCtrl->pTrafficAdmCtrlSm, &pTrafficAdmCtrl->currentState[acID], event, pData);

	return(status);
}


/************************************************************************
*							state machine functions						*
************************************************************************/
/************************************************************************
 *                        trafficAdmCtrl_smStart						*
 ************************************************************************
DESCRIPTION: the action function when event start ocuured on idle state 
				performs the following:
				-	send admision requestReset 
				-	start timer for the response.
                                                                                                   
INPUT:      fsmTSpecInfo - parameters for the request		
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smStart(fsmTSpecInfo_t *fsmTSpecInfo)		
{
	TI_STATUS				status;
	trafficAdmCtrl_t		*pTrafficAdmCtrl;
	tspecInfo_t				*pTSpecInfo;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);
	pTSpecInfo = fsmTSpecInfo->pTSpecInfo;

	/* send adm request frame */
	status = trafficAdmCtrl_sendAdmissionReq(pTrafficAdmCtrl, pTSpecInfo);
	if(status != TI_OK)
		return status;

	/* init timer */
	trafficAdmCtrl_startTimer(pTrafficAdmCtrl, pTSpecInfo->AC);

	return TI_OK;
}
/************************************************************************
 *                        trafficAdmCtrl_smWaitStop						*
 ************************************************************************
DESCRIPTION: the action function when event stop ocuured on wait state 
				performs the following:
				-	stop timer.
                                                                                                   
INPUT:      fsmTSpecInfo - parameters of the request		
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smWaitStop(fsmTSpecInfo_t *fsmTSpecInfo)
{
	trafficAdmCtrl_t		*pTrafficAdmCtrl;
	tspecInfo_t				*pTSpecInfo;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);
	pTSpecInfo = fsmTSpecInfo->pTSpecInfo;

	/* stop timer */
	trafficAdmCtrl_stopTimer(pTrafficAdmCtrl,fsmTSpecInfo->pTSpecInfo->AC);

TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "TRAFFIC ADM CTRL -  AC = %d,    Stoped ..... \n", pTSpecInfo->AC);

	
	return TI_OK;
}
/************************************************************************
 *                        trafficAdmCtrl_smWaitAccept					*
 ************************************************************************
DESCRIPTION: the action function when event accept ocuured on wait state 
				performs the following:
				-	update the Qos Mngr of the status and the parameters
                                                                                                   
INPUT:      fsmTSpecInfo - parameters of the response		
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smWaitAccept(fsmTSpecInfo_t *fsmTSpecInfo)
{
	trafficAdmCtrl_t		*pTrafficAdmCtrl;
	tspecInfo_t				*pTSpecInfo;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);
	pTSpecInfo = fsmTSpecInfo->pTSpecInfo;

	/* update the QosMngr */
	qosMngr_setAdmissionInfo(pTrafficAdmCtrl->hQosMngr, pTSpecInfo->AC, pTSpecInfo, STATUS_TRAFFIC_ADM_REQUEST_ACCEPT);

	return TI_OK;
}
/************************************************************************
 *                        trafficAdmCtrl_smWaitReject					*
 ************************************************************************
DESCRIPTION: the action function when event reject ocuured on wait state 
				performs the following:
				-	update the Qos Mngr of the status and the parameters
                                                                                                   
INPUT:      fsmTSpecInfo - parameters of the response		
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS trafficAdmCtrl_smWaitReject(fsmTSpecInfo_t *fsmTSpecInfo)
{
	trafficAdmCtrl_t		*pTrafficAdmCtrl;
	tspecInfo_t				*pTSpecInfo;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);
	pTSpecInfo = fsmTSpecInfo->pTSpecInfo;

	/* update the QosMngr */
	qosMngr_setAdmissionInfo(pTrafficAdmCtrl->hQosMngr, pTSpecInfo->AC, pTSpecInfo,	STATUS_TRAFFIC_ADM_REQUEST_REJECT);

	return TI_OK;
}
/************************************************************************
 *                        trafficAdmCtrl_smWaitTimeout					*
 ************************************************************************
DESCRIPTION: the action function when event timeout ocuured on wait state 
				performs the following:
				-	update the Qos Mngr of the status and the parameters
                                                                                                   
INPUT:      fsmTSpecInfo - parameters of the request		
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smWaitTimeout(fsmTSpecInfo_t *fsmTSpecInfo)
{
	trafficAdmCtrl_t		*pTrafficAdmCtrl;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);

	/* update the QosMngr */
	qosMngr_setAdmissionInfo(pTrafficAdmCtrl->hQosMngr, fsmTSpecInfo->acID, NULL, STATUS_TRAFFIC_ADM_REQUEST_TIMEOUT);

	return TI_OK;
}
/************************************************************************
 *               trafficAdmCtrl_smActionUnexpected						*
 ************************************************************************
DESCRIPTION:                                                 
INPUT:      fsmTSpecInfo - tspec parameters 	
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smActionUnexpected(fsmTSpecInfo_t *fsmTSpecInfo)
{
	trafficAdmCtrl_t		*pTrafficAdmCtrl;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);

TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "TRAFFIC ADM CTRL -  AC = %d,    ActionUnexpected ..... \n", fsmTSpecInfo->acID);

	return TI_OK;
}

/************************************************************************
 *               trafficAdmCtrl_smActionUnexpectedTspecResponse			*
 ************************************************************************
DESCRIPTION:                                                 
INPUT:      fsmTSpecInfo - tspec parameters 	
OUTPUT:		
RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smActionUnexpectedTspecResponse(fsmTSpecInfo_t *fsmTSpecInfo)
{
	trafficAdmCtrl_t		*pTrafficAdmCtrl;
	tspecInfo_t				*pTSpecInfo;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);
	pTSpecInfo = fsmTSpecInfo->pTSpecInfo;

	/* Send event to user application - how come TSPEC response arrives without request ? */
	qosMngr_sendUnexpectedTSPECResponseEvent (pTrafficAdmCtrl->hQosMngr,pTSpecInfo);

TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_WARNING, "TRAFFIC ADM CTRL -  AC = %d,    ActionUnexpected ..... \n", fsmTSpecInfo->acID);

	return TI_OK;
}


/************************************************************************
 *                        trafficAdmCtrl_smActionNop					*
 ************************************************************************
DESCRIPTION:                                                 
INPUT:      fsmTSpecInfo - tspec parameters 	
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_smActionNop(fsmTSpecInfo_t *fsmTSpecInfo)
{
	trafficAdmCtrl_t		*pTrafficAdmCtrl;
	tspecInfo_t				*pTSpecInfo;

	pTrafficAdmCtrl = (trafficAdmCtrl_t*)(fsmTSpecInfo->hTrafficAdmCtrl);
	pTSpecInfo = fsmTSpecInfo->pTSpecInfo;

TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "TRAFFIC ADM CTRL -  AC = %d,    Action NOP..... \n", pTSpecInfo->AC);

	return TI_OK;
}
/************************************************************************
 *							API FUNCTIONS						        *
 ************************************************************************
 ************************************************************************/

/************************************************************************
 *                    trafficAdmCtrl_startAdmRequest                    *
 ************************************************************************
DESCRIPTION: start TSPEC signaling
                                                                                                   
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
			pTSpecInfo			-	the TSPEC parameters
	
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS trafficAdmCtrl_startAdmRequest(TI_HANDLE	hTrafficAdmCtrl, tspecInfo_t *pTSpecInfo)
{
	TI_STATUS			status;
	fsmTSpecInfo_t		fsmTSpecInfo;

	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	if (pTrafficAdmCtrl == NULL)
		return TI_NOK;

	fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
	fsmTSpecInfo.pTSpecInfo = pTSpecInfo;
	fsmTSpecInfo.acID = pTSpecInfo->AC;

	/* send event START to SM */
	status = trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_START, &fsmTSpecInfo);

	return status;

}
/************************************************************************
 *                    trafficAdmCtrl_stopAdmRequest                     *
 ************************************************************************
DESCRIPTION: stop specific tspec signaling
                                                                                                   
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
			acID				-	the AC of the tspec to stop
	
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS trafficAdmCtrl_stopAdmRequest(TI_HANDLE hTrafficAdmCtrl, TI_UINT8 acID)
{
	TI_STATUS			status;
	trafficAdmCtrl_t	*pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;
	
	tspecInfo_t			pTSpecInfo;
	fsmTSpecInfo_t		fsmTSpecInfo;

	fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
	fsmTSpecInfo.pTSpecInfo = &pTSpecInfo;

	fsmTSpecInfo.pTSpecInfo->AC = (EAcTrfcType)acID;
	fsmTSpecInfo.acID = acID;

	/* send event STOP to SM */
	status = trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_STOP, &fsmTSpecInfo);
	
	return status;

}
/************************************************************************
 *                    trafficAdmCtrl_stop			                     *
 ************************************************************************
DESCRIPTION: stop all tspecs and reset SM  
			called on disconnect
                                                                                                 
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
	
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS trafficAdmCtrl_stop(TI_HANDLE	hTrafficAdmCtrl)
{
	TI_UINT32  uAcId;

	trafficAdmCtrl_t	*pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;
	
	tspecInfo_t			pTSpecInfo;
	fsmTSpecInfo_t		fsmTSpecInfo;

	fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
	fsmTSpecInfo.pTSpecInfo = &pTSpecInfo;

	/* clean all AC SM  */
	for (uAcId = 0; uAcId < MAX_NUM_OF_AC; uAcId++)
	{
		fsmTSpecInfo.pTSpecInfo->AC = (EAcTrfcType)uAcId;
		fsmTSpecInfo.acID = uAcId;
		trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_STOP, &fsmTSpecInfo);

        pTrafficAdmCtrl->dialogToken[uAcId] = 0;
	}

	pTrafficAdmCtrl->dialogTokenCounter = INITIAL_DIALOG_TOKEN;
	
	return TI_OK;
}

/************************************************************************
 *                    trafficAdmCtrl_recv			                     *
 ************************************************************************
DESCRIPTION: 
                                                                                                 
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
	
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS trafficAdmCtrl_recv(TI_HANDLE hTrafficAdmCtrl, TI_UINT8* pData, TI_UINT8 action)
{
	TI_STATUS 			status = TI_OK;
	TI_UINT8				statusCode;
	TI_UINT8				dialogToken;
	TI_UINT8				tacID;
	tspecInfo_t			tspecInfo;
	fsmTSpecInfo_t		fsmTSpecInfo;

	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	if (action == ADDTS_RESPONSE_ACTION) 
	{
TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "action = 1 - ADDTS RESPONSE ACTION........!! \n");

		/* parsing the dialog token */
		dialogToken = *pData;
		pData++;
		
		/* in WME status code is 1 byte, in WSM is 2 bytes */
		statusCode = *pData;
		pData++;

		tspecInfo.statusCode = statusCode;

TRACE2(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "dialogToken = %d ,  statusCode = %d \n",dialogToken, statusCode);

		trafficAdmCtrl_parseTspecIE(&tspecInfo, pData);

		if (trafficAdmCtrl_tokenToAc (pTrafficAdmCtrl, dialogToken, &tacID) == TI_NOK)
		{
TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_WARNING, "dialog token Not found,  dialogToken = %d , \n",dialogToken);
			
			qosMngr_sendUnexpectedTSPECResponseEvent(pTrafficAdmCtrl->hQosMngr, &tspecInfo);
		
			return TI_NOK;
		}
		
		/* validate dialog token matching */
		if(pTrafficAdmCtrl->dialogToken[tspecInfo.AC] != dialogToken)
		{
TRACE2(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_WARNING, "dialog token mismatch,  dialogToken = %d ,  acID = %d \n",dialogToken, tspecInfo.AC);
			
			qosMngr_sendUnexpectedTSPECResponseEvent(pTrafficAdmCtrl->hQosMngr, &tspecInfo);
		
			return TI_NOK;
		}

		/* Stop the relevant Timer */
		trafficAdmCtrl_stopTimer(pTrafficAdmCtrl, tspecInfo.AC);

		fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
		fsmTSpecInfo.pTSpecInfo = &tspecInfo;

		fsmTSpecInfo.acID = tspecInfo.AC;
		
		if(statusCode != ADDTS_STATUS_CODE_SUCCESS)
		{
			/* admission reject */
			/********************/
TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "***** admCtrlQos_recv: admission reject [ statusCode = %d ]\n",statusCode);
			
			
TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "ADDTS Response (reject) userPriority = %d , \n", tspecInfo.userPriority);
			
			trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_REJECT, &fsmTSpecInfo);
			
		}
		else
		{
			/* admission accept */
			/********************/
			
TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "***** admCtrlQos_recv: admission accept [ statusCode = %d ]\n",statusCode);
			
			
TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "ADDTS Response (accepted) userPriority = %d ,  \n", tspecInfo.userPriority);
	
TRACE2(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "mediumTime = %d ,  surplusBandwidthAllowance = %d \n", tspecInfo.mediumTime, tspecInfo.surplausBwAllowance);
			
			trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_ACCEPT, &fsmTSpecInfo);
		}
	}
	else
	{
		status = TI_NOK;
TRACE1(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "trafficAdmCtrl_recv: unknown action code = %d ,  \n",action);

	}
	return status;
}


/************************************************************************
 *                    trafficAdmCtrl_sendDeltsFrame                     *
 ************************************************************************
DESCRIPTION: 
                                                                                                 
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
	
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS trafficAdmCtrl_sendDeltsFrame(TI_HANDLE hTrafficAdmCtrl, tspecInfo_t *pTSpecInfo, TI_UINT8 reasonCode)
{
	TI_STATUS           status = TI_OK;
    TTxCtrlBlk          *pPktCtrlBlk;
    TI_UINT8            *pPktBuffer;
	TI_UINT32           totalLen = 0;
	tsInfo_t            tsInfo;
	trafficAdmCtrl_t    *pTrafficAdmCtrl = (trafficAdmCtrl_t *)hTrafficAdmCtrl;


    TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlQos_sendDeltsFrame: Enter....!! \n");

	/* Allocate a TxCtrlBlk and data buffer (large enough for the max packet) */
    pPktCtrlBlk = TWD_txCtrlBlk_Alloc (pTrafficAdmCtrl->hTWD);
    pPktBuffer  = txCtrl_AllocPacketBuffer (pTrafficAdmCtrl->hTxCtrl, pPktCtrlBlk, 2000);
    if (pPktBuffer == NULL)
    {
        TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR , ": No memory\n");
        TWD_txCtrlBlk_Free (pTrafficAdmCtrl->hTWD, pPktCtrlBlk);
        return TI_NOK;
    }
	
	status = trafficAdmCtrl_buildFrameHeader (pTrafficAdmCtrl, pPktCtrlBlk);
	if (status != TI_OK)
	{
        TWD_txCtrlBlk_Free (pTrafficAdmCtrl->hTWD, pPktCtrlBlk);
		return TI_NOK;
	}

	*(pPktBuffer + totalLen) = WME_CATAGORY_QOS;    /* CATEGORY_QOS in WME = 17*/
	totalLen++;
	*(pPktBuffer + totalLen) = DELTS_ACTION;        /* DELTS ACTION */
	totalLen++;
	*(pPktBuffer + totalLen) = 0;		/* DIALOG_TOKEN is 0 in DELTS */
	totalLen++;
	*(pPktBuffer + totalLen) = 0;		/* STATUS CODE is 0 in DELTS */
	totalLen++;
	
	/* 
	 * Build tsInfo fields 
	 */

	tsInfo.tsInfoArr[0] = 0;
	tsInfo.tsInfoArr[1] = 0;
	tsInfo.tsInfoArr[2] = 0;

    tsInfo.tsInfoArr[0] |= ( (pTSpecInfo->userPriority) << TSID_SHIFT);

	tsInfo.tsInfoArr[0] |= (BI_DIRECTIONAL << DIRECTION_SHIFT);		/* bidirectional */
	tsInfo.tsInfoArr[0] |= (TS_INFO_0_ACCESS_POLICY_EDCA << ACCESS_POLICY_SHIFT);	/* EDCA */
	
	tsInfo.tsInfoArr[1] |= (0 << AGGREGATION_SHIFT);
	
	tsInfo.tsInfoArr[1] |= (pTSpecInfo->UPSDFlag << APSD_SHIFT);
	
	tsInfo.tsInfoArr[1] |= (pTSpecInfo->userPriority << USER_PRIORITY_SHIFT);
	tsInfo.tsInfoArr[1] |= (NORMAL_ACKNOWLEDGEMENT << TSINFO_ACK_POLICY_SHIFT);
	
	tsInfo.tsInfoArr[2] |= (NO_SCHEDULE << SCHEDULE_SHIFT);

	/*  
	 * Build TSpec IE for DELTS
	 */

    *(pPktBuffer + totalLen    ) = WME_TSPEC_IE_ID;
	*(pPktBuffer + totalLen + 1) = WME_TSPEC_IE_TSINFO_LEN;

	*(pPktBuffer + totalLen + 2) = 0x00;
	*(pPktBuffer + totalLen + 3) = 0x50;
	*(pPktBuffer + totalLen + 4) = 0xf2;
	*(pPktBuffer + totalLen + 5) = WME_TSPEC_IE_OUI_TYPE;
	*(pPktBuffer + totalLen + 6) = WME_TSPEC_IE_OUI_SUB_TYPE;
	*(pPktBuffer + totalLen + 7) = WME_TSPEC_IE_VERSION;

	*(pPktBuffer + totalLen + 8) = tsInfo.tsInfoArr[0];
	*(pPktBuffer + totalLen + 9) = tsInfo.tsInfoArr[1];
	*(pPktBuffer + totalLen +10) = tsInfo.tsInfoArr[2];
	
	totalLen += WME_TSPEC_IE_TSINFO_LEN + 2;

    /* Update packet parameters (start-time, pkt-type and BDL) */
    pPktCtrlBlk->tTxDescriptor.startTime = os_timeStampMs (pTrafficAdmCtrl->hOs);
    pPktCtrlBlk->tTxPktParams.uPktType   = TX_PKT_TYPE_MGMT;
    BUILD_TX_TWO_BUF_PKT_BDL (pPktCtrlBlk, pPktCtrlBlk->aPktHdr, WLAN_HDR_LEN, pPktBuffer, totalLen)

	/* Enqueue packet in the mgmt-queues and run the scheduler. */
	status = txMgmtQ_Xmit (pTrafficAdmCtrl->hTxMgmtQ, pPktCtrlBlk, TI_FALSE);

	return TI_OK;
}


/************************************************************************
 *							INTERNAL FUNCTIONS					        *
 ************************************************************************/
/************************************************************************
 *                    trafficAdmCtrl_startTimer		                    *
 ************************************************************************
DESCRIPTION: start a specific ac timer
                                                                                                   
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
			acID				-	the AC of the timer
	
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS trafficAdmCtrl_startTimer(trafficAdmCtrl_t* pTrafficAdmCtrl, TI_UINT8 acID)
{
    TTimerCbFunc fTimerExpiryFunc = NULL;

    if (pTrafficAdmCtrl == NULL)
    {
		return TI_NOK;
    }

    switch (acID) 
    {
        case QOS_AC_BE:  fTimerExpiryFunc = trafficAdmCtrl_timeoutAcBE;   break;
        case QOS_AC_BK:  fTimerExpiryFunc = trafficAdmCtrl_timeoutAcBK;   break;
        case QOS_AC_VI:  fTimerExpiryFunc = trafficAdmCtrl_timeoutAcVI;   break;
        case QOS_AC_VO:  fTimerExpiryFunc = trafficAdmCtrl_timeoutAcVO;   break;
    }

    tmr_StartTimer (pTrafficAdmCtrl->hAdmCtrlTimer[acID],
                    fTimerExpiryFunc,
                    (TI_HANDLE)pTrafficAdmCtrl,
                    pTrafficAdmCtrl->timeout,
                    TI_FALSE);

	return TI_OK;
}
/************************************************************************
 *                    trafficAdmCtrl_stopTimer		                    *
 ************************************************************************
DESCRIPTION: stop a specific ac timer
                                                                                                   
INPUT:      pTrafficAdmCtrl	    -	trafficAdmCtr handle.
			acID				-	the AC of the timer
	
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS trafficAdmCtrl_stopTimer(trafficAdmCtrl_t* pTrafficAdmCtrl, TI_UINT8 acID)
{
	if (pTrafficAdmCtrl == NULL)
		return TI_NOK;
	
	tmr_StopTimer (pTrafficAdmCtrl->hAdmCtrlTimer[acID]);

	return TI_OK;
}

/************************************************************************
 *						  AC timers functionc		                    *
 ************************************************************************/

/* QOS_AC_BE */
/*********/
void trafficAdmCtrl_timeoutAcBE (TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured)
{
	fsmTSpecInfo_t	fsmTSpecInfo;
	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	
	/* FSM Tspec Info Structure */
	fsmTSpecInfo.acID = QOS_AC_BE;
	fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
	fsmTSpecInfo.pTSpecInfo = NULL;

	trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_TIMEOUT, &fsmTSpecInfo);
}

/* QOS_AC_BK */
/*********/
void trafficAdmCtrl_timeoutAcBK(TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured)
{
	fsmTSpecInfo_t	fsmTSpecInfo;
	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	
	/* FSM Tspec Info Structure */
	fsmTSpecInfo.acID = QOS_AC_BK;
	fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
	fsmTSpecInfo.pTSpecInfo = NULL;

	trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_TIMEOUT, &fsmTSpecInfo);

}
/* QOS_AC_VI */
/*********/
void trafficAdmCtrl_timeoutAcVI(TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured)
{
	fsmTSpecInfo_t	fsmTSpecInfo;
	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	
	/* FSM Tspec Info Structure */
	fsmTSpecInfo.acID = QOS_AC_VI;
	fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
	fsmTSpecInfo.pTSpecInfo = NULL;

	trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_TIMEOUT, &fsmTSpecInfo);

}
/* QOS_AC_VO */
/*********/
void trafficAdmCtrl_timeoutAcVO(TI_HANDLE hTrafficAdmCtrl, TI_BOOL bTwdInitOccured)
{
	fsmTSpecInfo_t	fsmTSpecInfo;
	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	
	/* FSM Tspec Info Structure */
	fsmTSpecInfo.acID = QOS_AC_VO;
	fsmTSpecInfo.hTrafficAdmCtrl = hTrafficAdmCtrl;
	fsmTSpecInfo.pTSpecInfo = NULL;

	trafficAdmCtrl_smEvent(pTrafficAdmCtrl, TRAFFIC_ADM_CTRL_SM_EVENT_TIMEOUT, &fsmTSpecInfo);

}


static TI_STATUS trafficAdmCtrl_tokenToAc (TI_HANDLE hTrafficAdmCtrl, TI_UINT8 token, TI_UINT8 *acID)
{
	TI_UINT8 idx;
	trafficAdmCtrl_t *pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;

	for (idx=0; idx<MAX_NUM_OF_AC; idx++)
	{
		if (pTrafficAdmCtrl->dialogToken[idx] == token)
		{
			*acID = idx;
			return (TI_OK);
		}
	}

	return (TI_NOK);

}



/************************************************************************
 *              trafficAdmCtrl_buildFrameHeader							*
 ************************************************************************
DESCRIPTION: build frame header 
                                                                                                   
INPUT:    	
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS trafficAdmCtrl_buildFrameHeader(trafficAdmCtrl_t *pTrafficAdmCtrl, TTxCtrlBlk *pPktCtrlBlk)
{
   	TI_STATUS			status;
	TMacAddr		    daBssid, saBssid;
	dot11_mgmtHeader_t *pdot11Header;
	ScanBssType_e		currBssType;
	TMacAddr    		currBssId;

	pdot11Header = (dot11_mgmtHeader_t *)(pPktCtrlBlk->aPktHdr);
	
    /* Get the Destination MAC address */
	status = ctrlData_getParamBssid(pTrafficAdmCtrl->hCtrlData, CTRL_DATA_CURRENT_BSSID_PARAM, daBssid);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

    /* Get the Source MAC address */
	status = ctrlData_getParamBssid(pTrafficAdmCtrl->hCtrlData, CTRL_DATA_MAC_ADDRESS, saBssid);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	/* receive BssId and Bss Type from control module */
	ctrlData_getCurrBssTypeAndCurrBssId(pTrafficAdmCtrl->hCtrlData, &currBssId, &currBssType);
	if (currBssType != BSS_INFRASTRUCTURE)
    {
 		/* report failure but don't stop... */
TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "trafficAdmCtrl_buildFrameHeader: Error !! currBssType = BSS_INFRASTRUCTURE \n");

		return TI_NOK;
    }
	/* infrastructure BSS */

	/* copy BSSID */
	MAC_COPY (pdot11Header->BSSID, currBssId);
	/* copy source mac address */
	MAC_COPY (pdot11Header->SA, saBssid);
	/* copy destination mac address */
	MAC_COPY (pdot11Header->DA, daBssid);

	/* set frame ctrl to mgmt action frame an to DS */
	pdot11Header->fc = ENDIAN_HANDLE_WORD(DOT11_FC_ACTION | DOT11_FC_TO_DS);

	return TI_OK;
}

/************************************************************************
 *                  trafficAdmCtrl_sendAdmissionReq						*
 ************************************************************************
DESCRIPTION: send admision request frame
                                                                                                   
INPUT:      hTrafficAdmCtrl	         -	Qos Manager handle.
		    pTSpecInfo				 -  tspec parameters
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

TI_STATUS trafficAdmCtrl_sendAdmissionReq(TI_HANDLE hTrafficAdmCtrl, tspecInfo_t *pTSpecInfo)
{
	TI_STATUS			status = TI_OK;
    TTxCtrlBlk          *pPktCtrlBlk;
    TI_UINT8            *pPktBuffer;
	TI_UINT32			len;
	TI_UINT32			totalLen = 0;
	trafficAdmCtrl_t	*pTrafficAdmCtrl = (trafficAdmCtrl_t*)hTrafficAdmCtrl;


TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlQos_smAdmissionReq: Enter....!! \n");

	/* Allocate a TxCtrlBlk and data buffer (large enough for the max packet) */
    pPktCtrlBlk = TWD_txCtrlBlk_Alloc (pTrafficAdmCtrl->hTWD);
    pPktBuffer  = txCtrl_AllocPacketBuffer (pTrafficAdmCtrl->hTxCtrl, pPktCtrlBlk, 2000);
    if (pPktBuffer == NULL)
    {
        TRACE0(pTrafficAdmCtrl->hReport, REPORT_SEVERITY_ERROR , ": No memory\n");
        TWD_txCtrlBlk_Free (pTrafficAdmCtrl->hTWD, pPktCtrlBlk);
        return TI_NOK;
    }
	
	status = trafficAdmCtrl_buildFrameHeader (pTrafficAdmCtrl, pPktCtrlBlk);
	if (status != TI_OK)
	{
        TWD_txCtrlBlk_Free (pTrafficAdmCtrl->hTWD, pPktCtrlBlk);
		return TI_NOK;
	}

	*(pPktBuffer + totalLen) = WME_CATAGORY_QOS;			/* CATEGORY_QOS WME = 17*/
	totalLen++;
	*(pPktBuffer + totalLen) = ADDTS_REQUEST_ACTION;		/* ADDTS request ACTION */
	totalLen++;

	/* storing the dialog token for response validation */
	pTrafficAdmCtrl->dialogToken[pTSpecInfo->AC] = pTrafficAdmCtrl->dialogTokenCounter++;	/* DIALOG_TOKEN */
	*(pPktBuffer + totalLen) = pTrafficAdmCtrl->dialogToken[pTSpecInfo->AC];
	totalLen++;

	*(pPktBuffer + totalLen) = 0;	 /* STATUS CODE is 0 for ADDTS */
	totalLen++;

	trafficAdmCtrl_buildTSPec (pTrafficAdmCtrl, pTSpecInfo, pPktBuffer + totalLen, (TI_UINT32*)&len);
	totalLen += len;

    /* Update packet parameters (start-time, length, pkt-type) */
    pPktCtrlBlk->tTxDescriptor.startTime = os_timeStampMs (pTrafficAdmCtrl->hOs);
    pPktCtrlBlk->tTxPktParams.uPktType   = TX_PKT_TYPE_MGMT;
    BUILD_TX_TWO_BUF_PKT_BDL (pPktCtrlBlk, pPktCtrlBlk->aPktHdr, WLAN_HDR_LEN, pPktBuffer, totalLen)

	/* Enqueue packet in the mgmt-queues and run the scheduler. */
	status = txMgmtQ_Xmit (pTrafficAdmCtrl->hTxMgmtQ, pPktCtrlBlk, TI_FALSE);

	return TI_OK;
}
/************************************************************************
 *                        trafficAdmCtrl_buildTSPec							*
 ************************************************************************
DESCRIPTION: build a tspec according to the tspec parameters
                                                                                                   
INPUT:      hTrafficAdmCtrl	     -	Qos Manager handle.
		    pTSpecInfo			 -  tspec parameters

OUTPUT:		pPktBuffer             - the Tspec IE to send
			len					 - the tspec frame len				

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

void trafficAdmCtrl_buildTSPec(trafficAdmCtrl_t	*pTrafficAdmCtrl, 
							   tspecInfo_t		*pTSpecInfo, 
							   TI_UINT8			*pDataBuf,
							   TI_UINT32		*len)
{
	tsInfo_t			tsInfo;
	TI_UINT16		nominalMSDUSize, maxMSDUSize;
	TI_UINT32		suspensionInterval = 0;   /* disable */


	os_memoryZero(pTrafficAdmCtrl->hOs, (void *)pDataBuf, WME_TSPEC_IE_LEN + 2);

	*pDataBuf =			WME_TSPEC_IE_ID;
	*(pDataBuf + 1) =	WME_TSPEC_IE_LEN;

	*(pDataBuf + 2) =	0x00;
	*(pDataBuf + 3) =	0x50;
	*(pDataBuf + 4) =	0xf2;
	*(pDataBuf + 5) =	WME_TSPEC_IE_OUI_TYPE;
	*(pDataBuf + 6) =	WME_TSPEC_IE_OUI_SUB_TYPE;
	*(pDataBuf + 7) =	WME_TSPEC_IE_VERSION;

	/* 
	 * Build tsInfo fields 
	 */

	tsInfo.tsInfoArr[0] = 0;
	tsInfo.tsInfoArr[1] = 0;
	tsInfo.tsInfoArr[2] = 0;

	tsInfo.tsInfoArr[0] |=		( (pTSpecInfo->userPriority) << TSID_SHIFT);
	tsInfo.tsInfoArr[0] |=		(pTSpecInfo->streamDirection << DIRECTION_SHIFT);		/* bidirectional */

	tsInfo.tsInfoArr[0] |=		(TS_INFO_0_ACCESS_POLICY_EDCA << ACCESS_POLICY_SHIFT);	/* EDCA */

	tsInfo.tsInfoArr[1] |=		(0 << AGGREGATION_SHIFT);

	tsInfo.tsInfoArr[1] |=		(pTSpecInfo->UPSDFlag << APSD_SHIFT);

	tsInfo.tsInfoArr[1] |=		(pTSpecInfo->userPriority << USER_PRIORITY_SHIFT);
	tsInfo.tsInfoArr[1] |=		(NORMAL_ACKNOWLEDGEMENT << TSINFO_ACK_POLICY_SHIFT);
	
	tsInfo.tsInfoArr[2] |=		(NO_SCHEDULE << SCHEDULE_SHIFT);

	*(pDataBuf + 8) =	tsInfo.tsInfoArr[0];
	*(pDataBuf + 9) =	tsInfo.tsInfoArr[1];
	*(pDataBuf +10) =	tsInfo.tsInfoArr[2];
    
	pDataBuf += 11;  /* Progress the data pointer to the next IE parameters. */
    
	/*
	 * Set all remained parameters 
	 */

    nominalMSDUSize = pTSpecInfo->nominalMsduSize;
    if (pTrafficAdmCtrl->useFixedMsduSize)
		nominalMSDUSize |= FIX_MSDU_SIZE;

    maxMSDUSize = (nominalMSDUSize & (~FIX_MSDU_SIZE));
    
	COPY_WLAN_WORD(pDataBuf,      &nominalMSDUSize);			/* Nominal-MSDU-size. */
	COPY_WLAN_WORD(pDataBuf +  2, &maxMSDUSize);			    /* Maximum-MSDU-size. */
	COPY_WLAN_LONG(pDataBuf +  4, &pTSpecInfo->uMinimumServiceInterval); /* Minimum service interval */
	COPY_WLAN_LONG(pDataBuf +  8, &pTSpecInfo->uMaximumServiceInterval); /* Maximum service interval */
	COPY_WLAN_LONG(pDataBuf + 16, &suspensionInterval);
	COPY_WLAN_LONG(pDataBuf + 24, &pTSpecInfo->meanDataRate);	/* Minimum-data-rate. */
	COPY_WLAN_LONG(pDataBuf + 28, &pTSpecInfo->meanDataRate);	/* Mean-data-rate. */
	COPY_WLAN_LONG(pDataBuf + 32, &pTSpecInfo->meanDataRate);	/* Peak-data-rate. */
	COPY_WLAN_LONG(pDataBuf + 44, &pTSpecInfo->minimumPHYRate);
	COPY_WLAN_WORD(pDataBuf + 48, &pTSpecInfo->surplausBwAllowance);

	*len = WME_TSPEC_IE_LEN + 2;
}



/************************************************************************
 *                        trafficAdmCtrl_parseTspecIE					*
 ************************************************************************
DESCRIPTION: parses a tspec IE according to the tspec parameters
                                                                                                   
INPUT:      pData			-  tspec IE from received frame

OUTPUT:		pTSpecInfo		-  parsed tspec parameters

RETURN:     None
************************************************************************/
void trafficAdmCtrl_parseTspecIE(tspecInfo_t *pTSpecInfo, TI_UINT8 *pData)
{
	tsInfo_t			tsInfo;
	TI_UINT8				userPriority;
	TI_UINT8				acID;
	TI_UINT8				tid;
	TI_UINT8				direction;
	TI_UINT8				APSDbit;

	pData += 8;  /* Skip the WME_TSPEC_IE header */

	/* Get the TS-Info (3 bytes) and parse its fields */
	tsInfo.tsInfoArr[0] = *pData;
	tsInfo.tsInfoArr[1] = *(pData + 1);
	tsInfo.tsInfoArr[2] = *(pData + 2);
	pData += 3;

	userPriority = (((tsInfo.tsInfoArr[1]) & TS_INFO_1_USER_PRIORITY_MASK) >> USER_PRIORITY_SHIFT);
	
	acID = WMEQosTagToACTable[userPriority];

	tid = 	(((tsInfo.tsInfoArr[0]) & TS_INFO_0_TSID_MASK) >> TSID_SHIFT);
	APSDbit = (((tsInfo.tsInfoArr[1]) & TS_INFO_1_APSD_MASK) >> APSD_SHIFT);
	direction = (((tsInfo.tsInfoArr[0]) & TS_INFO_0_DIRECTION_MASK) >> DIRECTION_SHIFT);


	pTSpecInfo->AC = (EAcTrfcType)acID;
	pTSpecInfo->userPriority = userPriority;
	pTSpecInfo->UPSDFlag = APSDbit;
	pTSpecInfo->streamDirection = (EStreamDirection)direction;
	pTSpecInfo->tid = tid;

	/* Get the other Tspec IE parameters (handle WLAN fram endianess if required) */
	COPY_WLAN_WORD(&pTSpecInfo->nominalMsduSize, pData);
	COPY_WLAN_LONG(&pTSpecInfo->uMinimumServiceInterval, pData + 4);
	COPY_WLAN_LONG(&pTSpecInfo->uMaximumServiceInterval, pData + 8);
	COPY_WLAN_LONG(&pTSpecInfo->meanDataRate, pData + 28);
	COPY_WLAN_LONG(&pTSpecInfo->minimumPHYRate, pData + 44);
	COPY_WLAN_WORD(&pTSpecInfo->surplausBwAllowance, pData + 48);
	pTSpecInfo->surplausBwAllowance >>= SURPLUS_BANDWIDTH_ALLOW;  /* Surplus is in 3 MSBits of TI_UINT16 */
	COPY_WLAN_WORD(&pTSpecInfo->mediumTime, pData + 50);
}



/*************************************************************************
 *																		 *
 *							DEBUG FUNCTIONS								 *
 *																		 *
 *************************************************************************/
void trafficAdmCtrl_print(trafficAdmCtrl_t *pTrafficAdmCtr)
{
	TI_UINT32 acID;

	WLAN_OS_REPORT(("     traffic Adm Ctrl  \n"));
	WLAN_OS_REPORT(("-----------------------------------\n\n"));
	WLAN_OS_REPORT(("timeout                   = %d\n",pTrafficAdmCtr->timeout));
	WLAN_OS_REPORT(("dialogTokenCounter        = %d\n",pTrafficAdmCtr->dialogTokenCounter));

	for (acID = 0 ; acID < MAX_NUM_OF_AC ; acID++)
	{
			WLAN_OS_REPORT(("     AC = %d  \n",acID));
			WLAN_OS_REPORT(("----------------------\n"));
			WLAN_OS_REPORT(("currentState   = %d \n",pTrafficAdmCtr->currentState[acID]));
			WLAN_OS_REPORT(("dialogToken    = %d \n",pTrafficAdmCtr->dialogToken[acID]));
	}
}
	
