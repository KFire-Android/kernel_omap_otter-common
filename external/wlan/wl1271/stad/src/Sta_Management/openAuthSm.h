/*
 * openAuthSm.h
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

/** \file assocSm.h
 *  \brief 802.11 authentication SM
 *
 *  \see assocSm.c
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	assocSm.h												   */
/*    PURPOSE:	802.11 authentication SM									   */
/*																	 	   */
/***************************************************************************/

#ifndef _OPEN_AUTH_SM_H
#define _OPEN_AUTH_SM_H

#include "fsm.h"
#include "mlmeApi.h"
#include "authSm.h"

/* Constants */

/* Enumerations */

/* state machine states */
typedef enum 
{
	OPEN_AUTH_SM_STATE_IDLE		= 0,
	OPEN_AUTH_SM_STATE_WAIT    	= 1,
	OPEN_AUTH_SM_STATE_AUTH		= 2
} openAuth_smStates_t;

/* State machine inputs */
typedef enum 
{
	OPEN_AUTH_SM_EVENT_START		= 0,
	OPEN_AUTH_SM_EVENT_STOP			= 1,
	OPEN_AUTH_SM_EVENT_SUCCESS    	= 2,
	OPEN_AUTH_SM_EVENT_FAIL			= 3,
	OPEN_AUTH_SM_EVENT_TIMEOUT		= 4,
	OPEN_AUTH_SM_EVENT_MAX_RETRY	= 5
} openAuth_smEvents_t;

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS openAuth_Config(TI_HANDLE 	hAuth,
					   TI_HANDLE 	pOs);

TI_STATUS openAuth_Recv(TI_HANDLE pAssoc, mlmeFrameInfo_t *pFrame);

TI_STATUS openAuth_Timeout(auth_t *pAuth);

TI_STATUS auth_osSMEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hAuth);

/* state machine functions */

TI_STATUS openAuth_smStartIdle(auth_t *pAuth);
TI_STATUS openAuth_smStopWait(auth_t *hAuth);
TI_STATUS openAuth_smSuccessWait(auth_t *hAuth);
TI_STATUS openAuth_smFailureWait(auth_t *hAuth);
TI_STATUS openAuth_smTimeoutWait(auth_t *hAuth);
TI_STATUS openAuth_smMaxRetryWait(auth_t *hAuth);
TI_STATUS openAuth_smStopAuth(auth_t *hAuth);
TI_STATUS openAuth_smActionUnexpected(auth_t *hAuth);

TI_STATUS openAuth_smResetRetry(auth_t *hAuth);
TI_STATUS openAuth_smIncRetry(auth_t *hAuth);
TI_STATUS openAuth_smReportSuccess(auth_t *hAuth);
TI_STATUS openAuth_smReportFailure(auth_t *hAuth);
TI_STATUS openAuth_smSendAuthReq(auth_t *hAuth);
TI_STATUS openAuth_smStartTimer(auth_t *hAuth);
TI_STATUS openAuth_smStopTimer(auth_t *hAuth);

#endif

