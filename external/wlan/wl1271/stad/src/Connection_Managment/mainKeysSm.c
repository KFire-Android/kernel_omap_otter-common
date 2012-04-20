/*
 * mainKeysSm.c
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

/** \file mainKeySM.c
 * \brief Main key state machine implementation.
 *
 * \see mainKeySM.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:  Main key SM													*
 *   PURPOSE: Main key SM implmentation				  						*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_36
#include "osApi.h"
#include "paramOut.h"
#include "timer.h"
#include "report.h"
#include "rsn.h"
#include "rsnApi.h"
#include "smeApi.h"
#include "mainSecSm.h"                         
#include "keyParser.h" 
#include "broadcastKeySM.h"
#include "unicastKeySM.h"  
#include "mainKeysSm.h"
#include "mainKeysSmInternal.h"
#include "DataCtrl_Api.h"
#include "admCtrl.h"
#include "EvHandler.h"
#include "TI_IPC_Api.h"
#include "connApi.h"

static TI_STATUS mainKeys_smEvent(struct _mainKeys_t *pMainKeys, TI_UINT8 event, void* pData);

/**
*
* mainKeys_create
*
* \b Description: 
*
* Allocate memory for the main security context, and create all the rest of the needed contexts.
*
* \b ARGS:
*
*  I - hOs - OS handle for OS operations.
*
* \b RETURNS:
*
*  pointer to main security context. If failed, returns NULL.
*
* \sa 
*/
mainKeys_t* mainKeys_create(TI_HANDLE hOs)
{
	mainKeys_t 	*pHandle;
	TI_STATUS		status;

	/* allocate association context memory */
	pHandle = (mainKeys_t*)os_memoryAlloc(hOs, sizeof(mainKeys_t));
	if (pHandle == NULL)
	{
		return NULL;
	}

	os_memoryZero(hOs, pHandle, sizeof(mainKeys_t));

	/* allocate memory for association state machine */
	status = fsm_Create(hOs, &pHandle->pMainKeysSm, MAIN_KEYS_NUM_STATES, MAIN_KEYS_NUM_EVENTS);
	if (status != TI_OK)
	{
		os_memoryFree(hOs, pHandle, sizeof(mainKeys_t));
		return NULL;
	}

	pHandle->pKeyParser = keyParser_create(hOs);
	if (pHandle->pKeyParser == NULL)
	{
		fsm_Unload(hOs, pHandle->pMainKeysSm);
		os_memoryFree(hOs, pHandle, sizeof(mainKeys_t));
		return NULL;
	}

	pHandle->pBcastSm = broadcastKey_create(hOs);
	if (pHandle->pBcastSm == NULL)
	{
		keyParser_unload(pHandle->pKeyParser);
		fsm_Unload(hOs, pHandle->pMainKeysSm);
		os_memoryFree(hOs, pHandle, sizeof(mainKeys_t));
		return NULL;
	}

	pHandle->pUcastSm = unicastKey_create(hOs);
	if (pHandle->pBcastSm == NULL)
	{
		broadcastKey_unload(pHandle->pBcastSm);
		keyParser_unload(pHandle->pKeyParser);
		fsm_Unload(hOs, pHandle->pMainKeysSm);
		os_memoryFree(hOs, pHandle, sizeof(mainKeys_t));
		return NULL;
	}
    
	pHandle->hOs = hOs;
    
	/* At first Timeout we will send MediaSpecific Event   */
	/* At any other Timeout we will send the Timeout Event */

	pHandle->mainKeysTimeoutCounter = TI_FALSE;
    
	return pHandle;
}

/**
*
* mainKeys_config
*
* \b Description: 
*
* Init main security state machine state machine
*
* \b ARGS:
*
*  none
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS mainKeys_config (mainKeys_t    *pMainKeys, 
                           TRsnPaeConfig *pPaeConfig, 
                           void          *pParent,
                           TI_HANDLE      hReport,
                           TI_HANDLE      hOs,
                           TI_HANDLE      hCtrlData,
                           TI_HANDLE      hEvHandler,
                           TI_HANDLE      hConn,
                           TI_HANDLE      hRsn,
                           TI_HANDLE      hTimer)

{
    TI_STATUS      status;

	/** Main key State Machine matrix */
	fsm_actionCell_t    mainKeysSM_matrix[MAIN_KEYS_NUM_STATES][MAIN_KEYS_NUM_EVENTS] =
	{
		/* next state and actions for IDLE state */
		{	{MAIN_KEYS_STATE_START, (fsm_Action_t)mainKeys_startIdle}, 
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeySmNop},
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeySmUnexpected},
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeySmUnexpected},
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeySmUnexpected}
		},

		/* next state and actions for START state */
		{	{MAIN_KEYS_STATE_START, (fsm_Action_t)mainKeySmUnexpected},
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeys_stopStart}, 
			{MAIN_KEYS_STATE_UNICAST_COMPLETE, (fsm_Action_t)mainKeySmNop},
			{MAIN_KEYS_STATE_BROADCAST_COMPLETE, (fsm_Action_t)mainKeySmNop},
			{MAIN_KEYS_STATE_START, (fsm_Action_t)mainKeys_smTimeOut}
		},

		/* next state and actions for UNICAST COMPLETE state */
		{	{MAIN_KEYS_STATE_UNICAST_COMPLETE, (fsm_Action_t)mainKeySmUnexpected},
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeys_stopUcastComplete}, 
			{MAIN_KEYS_STATE_UNICAST_COMPLETE, (fsm_Action_t)mainKeySmNop},
			{MAIN_KEYS_STATE_COMPLETE, (fsm_Action_t)mainKeys_bcastCompleteUcastComplete},
			{MAIN_KEYS_STATE_UNICAST_COMPLETE, (fsm_Action_t)mainKeys_smTimeOut}
		},

		/* next state and actions for BROADCAST COMPLETE state */
		{	{MAIN_KEYS_STATE_BROADCAST_COMPLETE, (fsm_Action_t)mainKeySmUnexpected},
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeys_stopBcastComplete},
			{MAIN_KEYS_STATE_COMPLETE, (fsm_Action_t)mainKeys_ucastCompleteBcastComplete},
			{MAIN_KEYS_STATE_BROADCAST_COMPLETE, (fsm_Action_t)mainKeySmNop},
			{MAIN_KEYS_STATE_BROADCAST_COMPLETE, (fsm_Action_t)mainKeys_smTimeOut}
		},

		/* next state and actions for COMPLETE state */
		{	{MAIN_KEYS_STATE_COMPLETE, (fsm_Action_t)mainKeySmUnexpected},
			{MAIN_KEYS_STATE_IDLE, (fsm_Action_t)mainKeys_stopComplete},
			{MAIN_KEYS_STATE_COMPLETE, (fsm_Action_t)mainKeySmNop},
			{MAIN_KEYS_STATE_COMPLETE, (fsm_Action_t)mainKeySmNop},
			{MAIN_KEYS_STATE_COMPLETE, (fsm_Action_t)mainKeySmUnexpected}
		}
	};

	pMainKeys->hCtrlData = hCtrlData;
	pMainKeys->hOs = hOs;
	pMainKeys->hReport = hReport;
    pMainKeys->hEvHandler = hEvHandler;
    pMainKeys->hConn = hConn;
    pMainKeys->hRsn = hRsn;
    pMainKeys->hTimer = hTimer;

    pMainKeys->pParent = pParent;
	pMainKeys->keysTimeout = MAIN_KEYS_TIMEOUT;

	pMainKeys->start = mainKeys_start;
	pMainKeys->stop = mainKeys_stop;
	pMainKeys->reportUcastStatus = mainKeys_reportUcastStatus;
	pMainKeys->reportBcastStatus = mainKeys_reportBcastStatus;
	pMainKeys->setKey = mainKeys_setKey;
	pMainKeys->removeKey = mainKeys_removeKey;
	pMainKeys->setDefaultKeyId = mainKeys_setDefaultKeyId;
	pMainKeys->getSessionKey = mainKeys_getSessionKey;

	pMainKeys->currentState = MAIN_KEYS_STATE_IDLE;

	/* allocate OS timer memory */
    if (pMainKeys->hSessionTimer == NULL)
    {
        pMainKeys->hSessionTimer = tmr_CreateTimer (pMainKeys->hTimer);
        if (pMainKeys->hSessionTimer == NULL)
        {
            TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "mainKeys_config(): Failed to create hSessionTimer!\n");
            return TI_NOK;
        }
	}

    status = fsm_Config(pMainKeys->pMainKeysSm, &mainKeysSM_matrix[0][0], 
						MAIN_KEYS_NUM_STATES, MAIN_KEYS_NUM_EVENTS, NULL, pMainKeys->hOs);
	if (status != TI_OK)
	{
TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error in configuring SM\n");
		return status;
	}

	status = keyParser_config(pMainKeys->pKeyParser, 
                              pPaeConfig, 
                              pMainKeys->pUcastSm, 
                              pMainKeys->pBcastSm, 
                              pMainKeys, 
                              hReport, 
                              hOs,
                              hCtrlData);
	if (status != TI_OK)
	{
TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error in configuring key parser\n");
		return status;
	}

	status = broadcastKey_config(pMainKeys->pBcastSm, pPaeConfig, pMainKeys, hReport, hOs);
	if (status != TI_OK)
	{
TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error in configuring broadcast key SM\n");
		return status;
	}
    
	status = unicastKey_config(pMainKeys->pUcastSm, pPaeConfig, pMainKeys, hReport, hOs);
	if (status != TI_OK)
	{
TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error in configuring unicast key SM\n");
		return status;
	}
	
	return TI_OK;
}

/**
*
* mainKeys_config
*
* \b Description: 
*
* Init main security state machine state machine
*
* \b ARGS:
*
*  none
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS mainKeys_unload(mainKeys_t *pMainKeys)
{
	TI_STATUS	status;

	if (pMainKeys == NULL)
	{
		return TI_NOK;
	}

	status = fsm_Unload(pMainKeys->hOs, pMainKeys->pMainKeysSm);
    if (status != TI_OK)
	{
		/* report failure but don't stop... */
        TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error releasing FSM memory \n");
	}

	if (pMainKeys->hSessionTimer)
	{
		tmr_DestroyTimer (pMainKeys->hSessionTimer);
	}
    pMainKeys->hSessionTimer = NULL;
	
    status = keyParser_unload(pMainKeys->pKeyParser);
    if (status != TI_OK)
	{
		/* report failure but don't stop... */
        TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error unloading key parser\n");
	}

	status = broadcastKey_unload(pMainKeys->pBcastSm);
    if (status != TI_OK)
	{
		/* report failure but don't stop... */
        TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error unloading broadcast key SM\n");
	}

	status = unicastKey_unload(pMainKeys->pUcastSm);
    if (status != TI_OK)
	{
		/* report failure but don't stop... */
        TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: Error unloading unicast key SM\n");
	}
	
	os_memoryFree(pMainKeys->hOs, pMainKeys, sizeof(mainKeys_t));

    return TI_OK;
}


/**
*
* rsn_mainKeySmStart
*
* \b Description: 
*
* START event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa rsn_mainKeySmStop()
*/

TI_STATUS mainKeys_start(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status;

	status = mainKeys_smEvent(pMainKeys, MAIN_KEYS_EVENT_START, pMainKeys);

	return status;
}

/**
*
* rsn_mainKeySmStop
*
* \b Description: 
*
* STOP event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa rsn_mainKeySmStart()
*/

TI_STATUS mainKeys_stop(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status;

	status = mainKeys_smEvent(pMainKeys, MAIN_KEYS_EVENT_STOP, pMainKeys);

	return status;
}

/**
*
* rsn_mainKeySmReportUnicastComplete
*
* \b Description: 
*
* UNICAST_COMPLETE event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa rsn_mainKeySmReportBroadcastComplete()
*/

TI_STATUS mainKeys_reportUcastStatus(struct _mainKeys_t *pMainKeys, TI_STATUS ucastStatus)
{
	TI_STATUS    status=TI_NOK;
    EExternalAuthMode extAuthMode;
	struct _rsn_t *pRsn = pMainKeys->pParent->pParent;

	if (ucastStatus == TI_OK)
	{

		txCtrlParams_setCurrentPrivacyInvokedMode(pRsn->hTxCtrl, TI_TRUE);
        
		status = pRsn->pAdmCtrl->getExtAuthMode(pRsn->pAdmCtrl, &extAuthMode);
        if (status != TI_OK)
        {
            return status;
        }
        if (extAuthMode >= RSN_EXT_AUTH_MODE_WPA)
        {
			txCtrlParams_setEapolEncryptionStatus(pRsn->hTxCtrl, TI_TRUE);
        }
        else
        {
			txCtrlParams_setEapolEncryptionStatus(pRsn->hTxCtrl, TI_FALSE);
        }
		
        status = mainKeys_smEvent(pMainKeys, MAIN_KEYS_EVENT_UCAST_COMPLETE, pMainKeys);
	}

	return status;
}


/**
*
* rsn_mainKeySmReportBroadcastComplete
*
* \b Description: 
*
* BROADCAST_COMPLETE event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa rsn_mainKeySmReportUnicastComplete()
*/

TI_STATUS mainKeys_reportBcastStatus(struct _mainKeys_t *pMainKeys, TI_STATUS bcastStatus)
{
	TI_STATUS  status=TI_NOK;

	if (bcastStatus == TI_OK)
	{
		status = mainKeys_smEvent(pMainKeys, MAIN_KEYS_EVENT_BCAST_COMPLETE, pMainKeys);
	}

	return status;
}

/**
*
* mainKeySmSessionTimeout
*
* \b Description: 
*
* SESSION_TIMEOUOT event handler
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_setKey(struct _mainKeys_t *pMainKeys, TSecurityKeys *pKey)
{
	return (pMainKeys->pParent->setKey(pMainKeys->pParent, pKey));
}

/**
*
* mainKeySmSessionTimeout
*
* \b Description: 
*
* SESSION_TIMEOUOT event handler
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_removeKey(struct _mainKeys_t *pMainKeys, TSecurityKeys *pKey)
{
	return (pMainKeys->pParent->removeKey(pMainKeys->pParent, pKey));
}

/**
*
* mainKeySmSessionTimeout
*
* \b Description: 
*
* SESSION_TIMEOUOT event handler
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_setDefaultKeyId(struct _mainKeys_t *pMainKeys, TI_UINT8 keyId)
{
	return (pMainKeys->pParent->setDefaultKeyId(pMainKeys->pParent, keyId));
}

/**
*
* mainKeySmSessionTimeout
*
* \b Description: 
*
* SESSION_TIMEOUOT event handler
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_getSessionKey(struct _mainKeys_t *pMainKeys, TI_UINT8 *pKey, TI_UINT32 *pKeyLen)
{
	TI_STATUS		status;

	status = pMainKeys->pParent->getSessionKey(pMainKeys->pParent, pKey, pKeyLen);

	return status;
}

/**
*
* mainKeySmSessionTimeout
*
* \b Description: 
*
* SESSION_TIMEOUOT event handler
*
* \b ARGS:
*
*  I   - pMainKeys - module handle  \n
*  I   - bTwdInitOccured - Indicates if TWDriver recovery occured since timer started   \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/

void mainKeys_sessionTimeout(void *pMainKeys, TI_BOOL bTwdInitOccured)
{

	mainKeys_smEvent(pMainKeys, MAIN_KEYS_EVENT_SESSION_TIMEOUOT, pMainKeys);

}


static TI_STATUS mainKeys_smEvent(struct _mainKeys_t *pMainKeys, TI_UINT8 event, void* pData)
{
	TI_STATUS		status;
	TI_UINT8		nextState;


	status = fsm_GetNextState(pMainKeys->pMainKeysSm, pMainKeys->currentState, event, &nextState);
	if (status != TI_OK)
	{
TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEYS_SM: ERROR - failed getting next state \n");

		return(TI_NOK);
	}

	TRACE3( pMainKeys->hReport, REPORT_SEVERITY_INFORMATION, "mainKeys_smEvent: <currentState = %d, event = %d> --> nextState = %d\n", pMainKeys->currentState, event, nextState);

	status = fsm_Event(pMainKeys->pMainKeysSm, &pMainKeys->currentState, event, pData);

	return status;
}
/**
*
* mainKeySmStartSubKeySmsAndTimer
*
* \b Description: 
*
* Starts unicast & broadcast key SMs and session timer.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/

TI_STATUS mainKeys_startIdle(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status;
	
	status = pMainKeys->pUcastSm->start(pMainKeys->pUcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	status = pMainKeys->pBcastSm->start(pMainKeys->pBcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

    tmr_StartTimer (pMainKeys->hSessionTimer,
                    mainKeys_sessionTimeout,
                    (TI_HANDLE)pMainKeys,
                    pMainKeys->keysTimeout,
                    TI_FALSE);
	
	status = pMainKeys->pKeyParser->replayReset(pMainKeys->pKeyParser);

	return status;
}


/**
*
* mainKeySmStopSubKeySmsAndTimer
*
* \b Description: 
*
* Stops unicast & broadcast key SMs and session timer.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_stopStart(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status = TI_OK;

	status = pMainKeys->pUcastSm->stop(pMainKeys->pUcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	status = pMainKeys->pBcastSm->stop(pMainKeys->pBcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	tmr_StopTimer (pMainKeys->hSessionTimer);
	
	return status;
}


/**
*
* mainKeySmStopSubKeySmsAndTimer
*
* \b Description: 
*
* Stops unicast & broadcast key SMs and session timer.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_stopUcastComplete(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status = TI_OK;

	status = pMainKeys->pUcastSm->stop(pMainKeys->pUcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	status = pMainKeys->pBcastSm->stop(pMainKeys->pBcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	tmr_StopTimer (pMainKeys->hSessionTimer);
	
	return status;
}

/**
*
* mainKeySmReportComplete
*
* \b Description: 
*
* Report key complete to the main security SM.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_bcastCompleteUcastComplete(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status;

    TRACE0(pMainKeys->hReport, REPORT_SEVERITY_INFORMATION, "mainKeys_bcastCompleteUcastComplete - sending Interrogate \n");

    tmr_StopTimer (pMainKeys->hSessionTimer);

	status = pMainKeys->pParent->reportKeysStatus(pMainKeys->pParent, TI_OK);

	return status;
}


/**
*
* mainKeySmStopSubKeySmsAndTimer
*
* \b Description: 
*
* Stops unicast & broadcast key SMs and session timer.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_stopBcastComplete(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status = TI_OK;

	status = pMainKeys->pUcastSm->stop(pMainKeys->pUcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	status = pMainKeys->pBcastSm->stop(pMainKeys->pBcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	tmr_StopTimer (pMainKeys->hSessionTimer);
	
	return status;
}

/**
*
* mainKeySmReportComplete
*
* \b Description: 
*
* Report key complete to the main security SM.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_ucastCompleteBcastComplete(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status;

    TRACE0(pMainKeys->hReport, REPORT_SEVERITY_INFORMATION, "mainKeys_ucastCompleteBcastComplete \n");

	tmr_StopTimer (pMainKeys->hSessionTimer);
	pMainKeys->mainKeysTimeoutCounter = TI_FALSE;
	status = pMainKeys->pParent->reportKeysStatus(pMainKeys->pParent, TI_OK);

	return status;
}

/**
*
* mainKeySmStopSubKeySmsAndTimer
*
* \b Description: 
*
* Stops unicast & broadcast key SMs and session timer.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_stopComplete(struct _mainKeys_t *pMainKeys)
{
	TI_STATUS  status = TI_OK;

	status = pMainKeys->pUcastSm->stop(pMainKeys->pUcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	status = pMainKeys->pBcastSm->stop(pMainKeys->pBcastSm);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	return status;
}

/**
*
* mainKeySmLogMessage
*
* \b Description: 
*
* Prints Log messge.\n
* Start session timer.
*
* \b ARGS:
*
*  I   - pData - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS mainKeys_smTimeOut(void* data)
{
	OS_802_11_AUTHENTICATION_REQUEST   *request;
	TI_UINT8                   AuthBuf[sizeof(TI_UINT32) + sizeof(OS_802_11_AUTHENTICATION_REQUEST)];
	paramInfo_t				param;
	TI_STATUS				status;
	struct _mainKeys_t 		*pMainKeys = (struct _mainKeys_t *)data;


TRACE1(pMainKeys->hReport, REPORT_SEVERITY_INFORMATION, "MAIN_KEY_SM: TRAP: Session Timeout for station , mainKeysTimeoutCounter=%d\n", pMainKeys->mainKeysTimeoutCounter);

	request = (OS_802_11_AUTHENTICATION_REQUEST *)(AuthBuf + sizeof(TI_UINT32));
	request->Length = sizeof(OS_802_11_AUTHENTICATION_REQUEST);

	param.paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
	status = ctrlData_getParam(pMainKeys->hCtrlData, &param);
	if (status != TI_OK)
	{
		return TI_NOK;
	}

    TRACE1(pMainKeys->hReport, REPORT_SEVERITY_INFORMATION, "current station is banned from the roaming candidates list for %d Ms\n", RSN_MAIN_KEYS_SESSION_TIMEOUT);

    rsn_banSite(pMainKeys->hRsn, param.content.ctrlDataCurrentBSSID, RSN_SITE_BAN_LEVEL_FULL, RSN_MAIN_KEYS_SESSION_TIMEOUT);
	

	/* mainKeysTimeoutCounter is a boolean variable, With states:	*/
	/* TI_TRUE - It is a Timeout Association Event						*/ 
	/* TI_FALSE - It is a Media specific Event							*/

	if (!pMainKeys->mainKeysTimeoutCounter)
	{
		/* Fill Media specific indication fields and send to OS/User    */ 
		MAC_COPY (request->BSSID, param.content.ctrlDataCurrentBSSID);
	
		request->Flags = OS_802_11_REQUEST_REAUTH;
	
		*(TI_UINT32*)AuthBuf = os802_11StatusType_Authentication;
	
TRACE1(pMainKeys->hReport, REPORT_SEVERITY_INFORMATION, " %d Ms\n",RSN_MAIN_KEYS_SESSION_TIMEOUT);
	
		EvHandlerSendEvent(pMainKeys->hEvHandler, IPC_EVENT_MEDIA_SPECIFIC, (TI_UINT8*)AuthBuf,
							sizeof(TI_UINT32) + sizeof(OS_802_11_AUTHENTICATION_REQUEST));
	
        tmr_StartTimer (pMainKeys->hSessionTimer,
                        mainKeys_sessionTimeout,
                        (TI_HANDLE)pMainKeys,
                        pMainKeys->keysTimeout,
                        TI_FALSE);

		pMainKeys->mainKeysTimeoutCounter = TI_TRUE;
	}
	else
	{
        pMainKeys->mainKeysTimeoutCounter = TI_FALSE;
        rsn_reportAuthFailure(pMainKeys->hRsn, RSN_AUTH_STATUS_TIMEOUT);
        conn_reportRsnStatus(pMainKeys->hConn, (mgmtStatus_e)STATUS_SECURITY_FAILURE);
	}
	return TI_OK;
}


TI_STATUS mainKeySmUnexpected(struct _mainKeys_t *pMainKeys)
{
TRACE0(pMainKeys->hReport, REPORT_SEVERITY_ERROR, "MAIN_KEY_SM: ERROR UnExpected Event\n");

	return(TI_OK);
}

TI_STATUS mainKeySmNop(struct _mainKeys_t *pMainKeys)
{
	return(TI_OK);
}

void mainKeys_reAuth(TI_HANDLE pHandle)
{
	mainKeys_t 	*pMainKeys = (mainKeys_t *)pHandle;

	rsn_reAuth(pMainKeys->hRsn);
}
