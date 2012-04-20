/*
 * openAuthSm.c
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

/** \file authSM.c
 *  \brief 802.11 authentication SM source
 *
 *  \see authSM.h
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	authSM.c												   */
/*    PURPOSE:	802.11 authentication SM source							   */
/*																	 	   */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_70
#include "osApi.h"

#include "paramOut.h"
#include "timer.h"
#include "fsm.h"
#include "report.h"
#include "mlmeApi.h"
#include "authSm.h"
#include "openAuthSm.h"
#include "rsnApi.h"

/* Constants */

/** number of states in the state machine */
#define	OPEN_AUTH_SM_NUM_STATES		3

/** number of events in the state machine */
#define	OPEN_AUTH_SM_NUM_EVENTS		6

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Global variables */

/* Local function prototypes */

/* functions */

/**
*
* openAuth_smConfig - configure a new authentication SM
*
* \b Description: 
*
* Configure a new authentication SM.
*
* \b ARGS:
*
*  I   - hAuth - Association SM context  \n
*  I   - hMlme - MLME SM context  \n
*  I   - hSiteMgr - Site manager context  \n
*  I   - hCtrlData - Control data context  \n
*  I   - hTxData - TX data context  \n
*  I   - hHalCtrl - Hal control context  \n
*  I   - hReport - Report context  \n
*  I   - hOs - OS context  \n
*  I   - authTimeout - Association SM timeout \n
*  I   - authMaxCount - Max number of authentication requests to send  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa openAuth_Create, openAuth_Unload
*/
TI_STATUS openAuth_Config(TI_HANDLE hAuth, TI_HANDLE hOs)
{
	auth_t		*pHandle;
	TI_STATUS		status;
	/** Main 802.1X State Machine matrix */
	fsm_actionCell_t	openAuth_smMatrix[OPEN_AUTH_SM_NUM_STATES][OPEN_AUTH_SM_NUM_EVENTS] =
	{
		/* next state and actions for IDLE state */
		{{OPEN_AUTH_SM_STATE_WAIT, (fsm_Action_t)openAuth_smStartIdle},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smActionUnexpected}
		},
		/* next state and actions for WAIT state */
		{{OPEN_AUTH_SM_STATE_WAIT, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smStopWait},
		 {OPEN_AUTH_SM_STATE_AUTH, (fsm_Action_t)openAuth_smSuccessWait},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smFailureWait},
		 {OPEN_AUTH_SM_STATE_WAIT, (fsm_Action_t)openAuth_smTimeoutWait},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smMaxRetryWait}
		},
		/* next state and actions for AUTH state */
		{{OPEN_AUTH_SM_STATE_AUTH, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_IDLE, (fsm_Action_t)openAuth_smStopAuth},
		 {OPEN_AUTH_SM_STATE_AUTH, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_AUTH, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_AUTH, (fsm_Action_t)openAuth_smActionUnexpected},
		 {OPEN_AUTH_SM_STATE_AUTH, (fsm_Action_t)openAuth_smActionUnexpected}
		}};
	

	if (hAuth == NULL)
	{
		return TI_NOK;
	}

	pHandle = (auth_t*)hAuth;
	
	status = fsm_Config(pHandle->pAuthSm, &openAuth_smMatrix[0][0], 
						OPEN_AUTH_SM_NUM_STATES, OPEN_AUTH_SM_NUM_EVENTS, auth_osSMEvent, hOs);
	if (status != TI_OK)
	{ 
		return TI_NOK;
	}

	pHandle->currentState = OPEN_AUTH_SM_STATE_IDLE;
	
	return TI_OK;
}


TI_STATUS auth_osSMEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hAuth)
{
   auth_t *pAuth = (auth_t *)hAuth;
	TI_STATUS 		status;
	TI_UINT8		nextState;

	status = fsm_GetNextState(pAuth->pAuthSm, *currentState, event, &nextState);
	if (status != TI_OK)
	{
		TRACE0(pAuth->hReport, REPORT_SEVERITY_SM, "State machine error, failed getting next state\n");
		return(TI_NOK);
	}

	TRACE3( pAuth->hReport, REPORT_SEVERITY_INFORMATION, "auth_osSMEvent: <currentState = %d, event = %d> --> nextState = %d\n", *currentState, event, nextState);

	status = fsm_Event(pAuth->pAuthSm, currentState, event, (void *)pAuth);

	return status;
}

/**
*
* openAuth_Recv - Recive a message from the AP
*
* \b Description: 
*
* Parse a message form the AP and perform the appropriate event.
*
* \b ARGS:
*
*  I   - hAuth - Association SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa openAuth_Start, openAuth_Stop
*/
TI_STATUS openAuth_Recv(TI_HANDLE hAuth, mlmeFrameInfo_t *pFrame)
{ 
	TI_STATUS 			status;
	auth_t			*pHandle;
	TI_UINT16			authAlgo;

	pHandle = (auth_t*)hAuth;

	if (pHandle == NULL)
	{
		return TI_NOK;
	}
	
	/* check response status */
	authAlgo = ENDIAN_HANDLE_WORD(pFrame->content.auth.authAlgo);
	if ((authAlgo != AUTH_LEGACY_OPEN_SYSTEM) &&
        (authAlgo != AUTH_LEGACY_RESERVED1))
	{
TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "OPEN_AUTH_SM: DEBUG recieved authentication message with wrong algorithm \n");
        rsn_reportAuthFailure(pHandle->hRsn, RSN_AUTH_STATUS_INVALID_TYPE);
        return TI_NOK;
	}
    
    if ((pHandle->authType==AUTH_LEGACY_RESERVED1) && (authAlgo !=AUTH_LEGACY_RESERVED1))
    {
        rsn_reportAuthFailure(pHandle->hRsn, RSN_AUTH_STATUS_INVALID_TYPE);
    }
    TRACE1(pHandle->hReport, REPORT_SEVERITY_SM, "OPEN_AUTH_SM: DEBUG Authentication status is %d \n", pFrame->content.auth.status);

	pHandle->authData.status = pFrame->content.auth.status;

    if (pHandle->authData.status == STATUS_SUCCESSFUL)
    {
        status = auth_osSMEvent(&pHandle->currentState, OPEN_AUTH_SM_EVENT_SUCCESS, pHandle);
    } else {
		rsn_reportAuthFailure(pHandle->hRsn, RSN_AUTH_STATUS_INVALID_TYPE);
		status = auth_osSMEvent(&pHandle->currentState, OPEN_AUTH_SM_EVENT_FAIL, pHandle);
    }

	return status;
}

/* state machine functions */

TI_STATUS openAuth_smStartIdle(auth_t *pAuth)
{
	TI_STATUS		status;

	status = openAuth_smResetRetry(pAuth);
	if (TI_OK != status)
    {
        TRACE0(pAuth->hReport, REPORT_SEVERITY_ERROR, "openAuth_smStartIdle: openAuth_smResetRetry return\n");
        return status;
    }

    status = openAuth_smSendAuthReq(pAuth);
    if (TI_OK != status)
    {
        TRACE0(pAuth->hReport, REPORT_SEVERITY_ERROR, "openAuth_smStartIdle: openAuth_smSendAuthReq return\n");
        return status;
    }

    status = openAuth_smStartTimer(pAuth);
    if (TI_OK != status)
    {
        TRACE0(pAuth->hReport, REPORT_SEVERITY_ERROR, "openAuth_smStartIdle: openAuth_smStartTimer return\n");
        return status;
    }

    status = openAuth_smIncRetry(pAuth);
    if (TI_OK != status)
    {
        TRACE0(pAuth->hReport, REPORT_SEVERITY_ERROR, "openAuth_smStartIdle: openAuth_smIncRetry return\n");
        return status;
    }

	return status;
}

TI_STATUS openAuth_smStopWait(auth_t *hAuth)
{
	TI_STATUS		status;

	status = openAuth_smStopTimer(hAuth);

	return status;
}
 
TI_STATUS openAuth_smSuccessWait(auth_t *hAuth)
{
	TI_STATUS		status;

	status = openAuth_smStopTimer(hAuth);
	status = openAuth_smReportSuccess(hAuth);

	return status;
}
 
TI_STATUS openAuth_smFailureWait(auth_t *hAuth)
{
	TI_STATUS		status;

	status = openAuth_smStopTimer(hAuth);
	status = openAuth_smReportFailure(hAuth);

	return status;
}

TI_STATUS openAuth_smTimeoutWait(auth_t *hAuth)
{
	TI_STATUS		status;

	status = openAuth_smSendAuthReq(hAuth);
	status = openAuth_smStartTimer(hAuth);
	status = openAuth_smIncRetry(hAuth);

	return status;
}

TI_STATUS openAuth_smMaxRetryWait(auth_t *hAuth)
{
	TI_STATUS		status;

    rsn_reportAuthFailure(hAuth->hRsn, RSN_AUTH_STATUS_TIMEOUT);
	status = openAuth_smReportFailure(hAuth);

	return status;
}

TI_STATUS openAuth_smSendAuthReq(auth_t *hAuth)
{
	TI_STATUS		status;

	status = auth_smMsgBuild(hAuth, 1, 0, NULL, 0);

	return status;
}

TI_STATUS openAuth_smStopAuth(auth_t *hAuth)
{
	return TI_OK;
}

TI_STATUS openAuth_smActionUnexpected(auth_t *hAuth)
{
	return TI_OK;
}

/* local functions */


TI_STATUS openAuth_smResetRetry(auth_t *hAuth)
{
	if (hAuth == NULL)
	{
		return TI_NOK;
	}
	
	hAuth->retryCount = 0;
	
	return TI_OK;
}

TI_STATUS openAuth_smIncRetry(auth_t *hAuth)
{
	if (hAuth == NULL)
	{
		return TI_NOK;
	}
	
	hAuth->retryCount++;
	
	return TI_OK;
}

TI_STATUS openAuth_smReportSuccess(auth_t *hAuth)
{
	TI_STATUS 		status;

	if (hAuth == NULL)
	{
		return TI_NOK;
	}
    status = mlme_reportAuthStatus(hAuth->hMlme, hAuth->authData.status);

	return status;
}

TI_STATUS openAuth_smReportFailure(auth_t *hAuth)
{
	TI_STATUS 		status;
    
	if (hAuth == NULL)
	{
		return TI_NOK;
	}
	
    status = mlme_reportAuthStatus(hAuth->hMlme, hAuth->authData.status);

	return status;
}

TI_STATUS openAuth_smStartTimer(auth_t *hAuth)
{
	if (hAuth == NULL)
	{
		return TI_NOK;
	}
	
    tmr_StartTimer (hAuth->hAuthSmTimer,
                    auth_smTimeout,
                    (TI_HANDLE)hAuth,
                    hAuth->timeout,
                    TI_FALSE);

	return TI_OK;
}                        

TI_STATUS openAuth_smStopTimer(auth_t *hAuth)
{
	if (hAuth == NULL)
	{
		return TI_NOK;
	}
	
	tmr_StopTimer (hAuth->hAuthSmTimer);

	return TI_OK;
}

TI_STATUS openAuth_Timeout(auth_t *pAuth)
{
	if (pAuth->retryCount >= pAuth->maxCount)
	{
		pAuth->authData.status = STATUS_PACKET_REJ_TIMEOUT;
		return auth_osSMEvent(&pAuth->currentState, OPEN_AUTH_SM_EVENT_MAX_RETRY, pAuth);
	}

	return auth_osSMEvent(&pAuth->currentState, OPEN_AUTH_SM_EVENT_TIMEOUT, pAuth);
}


