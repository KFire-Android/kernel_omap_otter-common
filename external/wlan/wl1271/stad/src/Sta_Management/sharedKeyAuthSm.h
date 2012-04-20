/*
 * sharedKeyAuthSm.h
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

/** \file sharedKeyAuthSm.h
 *  \brief shared key 802.11 authentication SM
 *
 *  \see sharedKeyAuthSm.c
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	sharedKeyAuthSm.h										   */
/*    PURPOSE:	shared key 802.11 authentication SM						   */
/*																	 	   */
/***************************************************************************/

#ifndef _SHARED_KEY_AUTH_SM_H
#define _SHARED_KEY_AUTH_SM_H

#include "fsm.h"
#include "mlmeApi.h"
#include "authSm.h"

/* Constants */

/* Enumerations */

/* state machine states */
typedef enum 
{
	SHARED_KEY_AUTH_SM_STATE_IDLE		= 0,
	SHARED_KEY_AUTH_SM_STATE_WAIT_1    	= 1,
	SHARED_KEY_AUTH_SM_STATE_WAIT_2    	= 2,
	SHARED_KEY_AUTH_SM_STATE_AUTH		= 3
} sharedKeyAuth_smStates_t;

/* State machine inputs */
typedef enum 
{
	SHARED_KEY_AUTH_SM_EVENT_START			= 0,
	SHARED_KEY_AUTH_SM_EVENT_STOP			= 1,
	SHARED_KEY_AUTH_SM_EVENT_SUCCESS_1  	= 2,
	SHARED_KEY_AUTH_SM_EVENT_FAIL_1			= 3,
	SHARED_KEY_AUTH_SM_EVENT_SUCCESS_2  	= 4,
	SHARED_KEY_AUTH_SM_EVENT_FAIL_2			= 5,
	SHARED_KEY_AUTH_SM_EVENT_TIMEOUT		= 6,
	SHARED_KEY_AUTH_SM_EVENT_MAX_RETRY		= 7
} sharedKeyAuth_smEvents_t;

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS sharedKeyAuth_Config(TI_HANDLE 	hAuth,
							TI_HANDLE 	pOs);

TI_STATUS sharedKeyAuth_Recv(TI_HANDLE pAssoc, mlmeFrameInfo_t *pFrame);

TI_STATUS sharedKey_Timeout(auth_t *pAuth);

TI_STATUS auth_skSMEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hAuth);

/* state machine functions */

TI_STATUS sharedKeyAuth_smStartIdle(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smStopWait(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smSuccess1Wait1(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smFailure1Wait1(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smSuccess2Wait2(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smFailure2Wait2(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smTimeoutWait1(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smTimeoutWait2(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smMaxRetryWait(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smStopAuth(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smActionUnexpected(auth_t *hAuth);

TI_STATUS sharedKeyAuth_smResetRetry(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smIncRetry(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smReportSuccess(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smReportFailure(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smSendAuth1(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smSendAuth2(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smStartTimer(auth_t *hAuth);
TI_STATUS sharedKeyAuth_smStopTimer(auth_t *hAuth);

#endif

