/*
 * mainSecKeysOnly.c
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

/** \file mainSecSm.c
 *  \brief 802.1X finite state machine header file
 *
 *  \see mainSecSm.h
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	mainSecSm.c											   	   */
/*    PURPOSE:	Main Security State Machine API					   		   */
/*																	 	   */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_37
#include "osApi.h"
#include "paramOut.h"
#include "fsm.h"
#include "report.h"
#include "DataCtrl_Api.h"
#include "smeApi.h"
#include "rsn.h"
#include "rsnApi.h"
#include "mainSecSm.h"
#include "mainSecKeysOnly.h"
#include "mainKeysSm.h"

/* Constants */

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Global variables */

/* functions */
TI_STATUS mainSecKeysOnly_getAuthState(mainSec_t *pMainSec, TIWLN_SECURITY_STATE *supp1XState);

TI_STATUS mainSecKeysOnly_reportAuthFailure(mainSec_t *pMainSec, EAuthStatus authStatus) ;
TI_STATUS mainSecKeysOnly_setAuthIdentity(mainSec_t *pMainSec, authIdentity_t *authIdentity);
/**
*
* rsn_mainSecSmKeysOnlyInit
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
TI_STATUS mainSecKeysOnly_config(mainSec_t *pMainSec, 
						  TRsnPaeConfig *pPaeConfig)
{
	TI_STATUS				status;
	/** Main 802.1X State Machine matrix */
	fsm_actionCell_t	mainSecKeysOnly_matrix[MAIN_SEC_KEYS_ONLY_NUM_STATES][MAIN_SEC_KEYS_ONLY_NUM_EVENTS] =
	{
		/* next state and actions for IDLE state */
		{{MAIN_KO_STATE_START, (fsm_Action_t)mainSecKeysOnly_startIdle},
		 {MAIN_KO_STATE_IDLE, (fsm_Action_t)mainSecKeysOnly_Nop},
		 {MAIN_KO_STATE_IDLE, (fsm_Action_t)mainSecKeysOnly_unexpected},
		 {MAIN_KO_STATE_IDLE, (fsm_Action_t)mainSecKeysOnly_unexpected}
		},
		/* next state and actions for START state */
		{{MAIN_KO_STATE_START, (fsm_Action_t)mainSecKeysOnly_Nop},
		 {MAIN_KO_STATE_IDLE, (fsm_Action_t)mainSecKeysOnly_stopStart},
		 {MAIN_KO_STATE_AUTHORIZED, (fsm_Action_t)mainSecKeysOnly_keysCompleteStart},
		 {MAIN_KO_STATE_NONAUTHORIZED, (fsm_Action_t)mainSecKeysOnly_keysTOStart},
		},
		/* next state and actions for AUTHORIZED state */
		{{MAIN_KO_STATE_AUTHORIZED, (fsm_Action_t)mainSecKeysOnly_unexpected},
		 {MAIN_KO_STATE_IDLE, (fsm_Action_t)mainSecKeysOnly_stopAuthorized},
		 {MAIN_KO_STATE_AUTHORIZED, (fsm_Action_t)mainSecKeysOnly_Nop},
		 {MAIN_KO_STATE_AUTHORIZED, (fsm_Action_t)mainSecKeysOnly_unexpected},
		},
		/* next state and actions for NONAUTORIZED state */
		{{MAIN_KO_STATE_START, (fsm_Action_t)mainSecKeysOnly_unexpected},
		 {MAIN_KO_STATE_IDLE, (fsm_Action_t)mainSecKeysOnly_stopNonAuthorized},
		 {MAIN_KO_STATE_NONAUTHORIZED, (fsm_Action_t)mainSecKeysOnly_unexpected},
		 {MAIN_KO_STATE_NONAUTHORIZED, (fsm_Action_t)mainSecKeysOnly_unexpected},
		}
	};

    status = fsm_Config(pMainSec->pMainSecSm, &mainSecKeysOnly_matrix[0][0], 
						MAIN_SEC_KEYS_ONLY_NUM_STATES, MAIN_SEC_KEYS_ONLY_NUM_EVENTS, NULL, pMainSec->hOs);
	if (status != TI_OK)
	{
TRACE0(pMainSec->hReport, REPORT_SEVERITY_ERROR, "MAIN_SEC_SM: Error in configuring full SM\n");
		return status;
	}

    pMainSec->currentState          = MAIN_KO_STATE_IDLE;
	pMainSec->start 				= mainSecKeysOnly_start;
	pMainSec->stop 					= mainSecKeysOnly_stop;
	pMainSec->reportKeysStatus 		= mainSecKeysOnly_reportKeysStatus;
	pMainSec->getAuthState 			= (mainSecSm_getAuthState_t)mainSecKeysOnly_getAuthState;
	pMainSec->reportReAuthenticate 	= (mainSecSmReportReauthentication_t)mainSecKeysOnly_unexpected;
	pMainSec->setSessionKey 		= (mainSecSmSetSessionKey_t)mainSecKeysOnly_unexpected;
	pMainSec->getSessionKey 		= (mainSecSmGetSessionKey_t)mainSecKeysOnly_unexpected;
	pMainSec->reportAuthStatus 		= (mainSecSmReportAuthStatus_t)mainSecKeysOnly_unexpected;
	pMainSec->getAuthIdentity  		= (mainSecSm_getAuthIdentity_t)mainSecKeysOnly_unexpected;
	pMainSec->setAuthIdentity  		= (mainSecSm_getAuthIdentity_t)mainSecKeysOnly_setAuthIdentity;
    pMainSec->reportAuthFailure     = (mainSecSm_reportAuthFailure_t)mainSecKeysOnly_reportAuthFailure;
    
    TRACE0(pMainSec->hReport, REPORT_SEVERITY_INFORMATION, "mainSecKeysOnly_config\n");

    return TI_OK;
}

/**
*
* mainSecSmNull_Start
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
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
TI_STATUS mainSecKeysOnly_start(mainSec_t *pMainSec)
{
    TI_STATUS status;

    status = fsm_Event(pMainSec->pMainSecSm, &pMainSec->currentState, MAIN_KO_EVENT_START, pMainSec); 
    
    return status;
}

/**
*
* mainSecSmNull_Stop
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
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
TI_STATUS mainSecKeysOnly_stop(mainSec_t *pMainSec)
{
    TI_STATUS status;

    status = fsm_Event(pMainSec->pMainSecSm, &pMainSec->currentState, MAIN_KO_EVENT_STOP, pMainSec); 
    
    return status;
}
 
/**
*
* mainSecSmNull_Stop
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
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
TI_STATUS mainSecKeysOnly_reportKeysStatus(mainSec_t *pMainSec, TI_STATUS keysStatus)
{
	TI_STATUS status;

	pMainSec->data.status = keysStatus;

	if (keysStatus == TI_OK)
	{
		status = fsm_Event(pMainSec->pMainSecSm, &pMainSec->currentState, MAIN_KO_EVENT_KEYS_COMPLETE, pMainSec);
	} else {
TRACE0(pMainSec->hReport, REPORT_SEVERITY_ERROR, "MAIN_SEC_SM: Error in Keys\n");
		status  = TI_OK;
	}

	return status;
}

/**
*
* mainSecSmNull_setKey
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
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
TI_STATUS mainSecKeysOnly_setSessionKey(mainSec_t *pMainSec, TI_UINT8* pKey, TI_UINT8 keyLen)
{
	os_memoryCopy(pMainSec->hOs, pMainSec->sessionKey, pKey, keyLen);
	pMainSec->sessionKeyLen = keyLen;

	return TI_OK;
}

/**
*
* mainSecSmNull_setKey
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
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
TI_STATUS mainSecKeysOnly_getSessionKey(mainSec_t *pMainSec, TI_UINT8* pKey, TI_UINT32* pKeyLen)
{
	os_memoryCopy(pMainSec->hOs, pKey, pMainSec->sessionKey, pMainSec->sessionKeyLen);
	*pKeyLen = pMainSec->sessionKeyLen;

	return TI_OK;
}



/* State machine implementation functions */

TI_STATUS mainSecKeysOnly_startIdle(struct _mainSec_t *pMainSec)
{
    TI_STATUS		status = TI_OK;

TRACE0(pMainSec->hReport, REPORT_SEVERITY_SM, "MAIN_SEC_SM: mainSecKeysOnly_StartIdle...\n");
	status =  pMainSec->pMainKeys->start(pMainSec->pMainKeys);
	
	return status;
}

TI_STATUS mainSecKeysOnly_stopStart(struct _mainSec_t *pMainSec)
{
    TI_STATUS		status = TI_OK;

TRACE0(pMainSec->hReport, REPORT_SEVERITY_SM, "MAIN_SEC_SM: mainSecKeysOnly_StopStart...\n");
	status =  pMainSec->pMainKeys->stop(pMainSec->pMainKeys);

	return status;
}

TI_STATUS mainSecKeysOnly_keysCompleteStart(struct _mainSec_t *pMainSec)
{
    TI_STATUS		status = TI_OK;

TRACE0(pMainSec->hReport, REPORT_SEVERITY_SM, "MAIN_SEC_SM: mainSecKeysOnly_KeysCompleteAuthenticated...\n");
	status =  pMainSec->pParent->reportStatus(pMainSec->pParent, pMainSec->data.status);
	
	return(status);
}

TI_STATUS mainSecKeysOnly_keysTOStart(struct _mainSec_t *pMainSec)
{
    TI_STATUS		status = TI_OK;

	return(status);
}

TI_STATUS mainSecKeysOnly_stopAuthorized(struct _mainSec_t *pMainSec)
{
    TI_STATUS		status = TI_OK;

TRACE0(pMainSec->hReport, REPORT_SEVERITY_SM, "MAIN_SEC_SM: mainSecKeysOnly_StopAuthorized...\n");
	status =  pMainSec->pMainKeys->stop(pMainSec->pMainKeys);
	
	return(status);
}

TI_STATUS mainSecKeysOnly_stopNonAuthorized(struct _mainSec_t *pMainSec)
{
    TI_STATUS		status = TI_OK;

TRACE0(pMainSec->hReport, REPORT_SEVERITY_SM, "MAIN_SEC_SM: mainSecKeysOnly_StopNonAuthorized...\n");
	status =  pMainSec->pMainKeys->stop(pMainSec->pMainKeys);
	
	return(status);
}

/* state machine action functions */

TI_STATUS mainSecKeysOnly_Nop(void* pData)
{
	return(TI_OK);
}

TI_STATUS mainSecKeysOnly_unexpected(void* pData)
{
	return TI_NOK;
}

 /**
*
* mainSecKeysOnly_getAuthState:  \n 
*
* \b Description: 
*
* Get authentication state from supp1x SM.
*
* \b ARGS:
*
*  I   - pMainSec - pMainSec SM context  \n
*  I   - authIdentity - pointer to authentication state \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/

TI_STATUS mainSecKeysOnly_getAuthState(mainSec_t *pMainSec, TIWLN_SECURITY_STATE *secState)
{
	switch (pMainSec->currentState)
	{
	case MAIN_KO_STATE_START:
		*secState = eSecurityStateAuthenticating;
		break;

	case MAIN_KO_STATE_AUTHORIZED:
		*secState = eSecurityStateAuthenticated;
		break;

	case MAIN_KO_STATE_NONAUTHORIZED:
		*secState = eSecurityStateNotAuthenticated ;
		break;

	default:
		*secState = eSecurityStateHalted;
		break;
	}
	
	return TI_OK;

} /*mainSecKeysOnly_getAuthState*/


TI_STATUS mainSecKeysOnly_reportAuthFailure(mainSec_t *pMainSec, EAuthStatus authStatus) 
{

    return TI_OK;
}


TI_STATUS mainSecKeysOnly_setAuthIdentity(mainSec_t *pMainSec, authIdentity_t *authIdentity)
{

	return TI_OK;
} 
