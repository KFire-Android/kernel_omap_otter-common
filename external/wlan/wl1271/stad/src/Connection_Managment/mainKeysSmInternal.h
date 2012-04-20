/*
 * mainKeysSmInternal.h
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

/** \file mainKeysSm.h
 *  \brief RSN main security SM
 *
 *  \see mainKeysSm.c
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	mainKeysSm.h												   */
/*    PURPOSE:	RSN main security SM									   */
/*																	 	   */
/***************************************************************************/

#ifndef _MAIN_KEYS_INTERNAL_H
#define _MAIN_KEYS_INTERNAL_H

#include "paramOut.h"
#include "rsnApi.h"

#include "mainKeysSm.h"

/* Constants */


/* Enumerations */

/* Typedefs */

/** state machine states */
typedef enum 
{
	MAIN_KEYS_STATE_IDLE                  = 0,	
	MAIN_KEYS_STATE_START                 = 1,	
	MAIN_KEYS_STATE_UNICAST_COMPLETE      = 2,		
	MAIN_KEYS_STATE_BROADCAST_COMPLETE    = 3,	
	MAIN_KEYS_STATE_COMPLETE              = 4,
    MAIN_KEYS_NUM_STATES
} mainKeys_smStates;

/** State machine events */
typedef enum 
{
	MAIN_KEYS_EVENT_START					= 0,
	MAIN_KEYS_EVENT_STOP					= 1,
	MAIN_KEYS_EVENT_UCAST_COMPLETE			= 2,
	MAIN_KEYS_EVENT_BCAST_COMPLETE			= 3,
	MAIN_KEYS_EVENT_SESSION_TIMEOUOT		= 4,
    MAIN_KEYS_NUM_EVENTS
} mainKeys_smEvents;


/* Main Sec SM functions */

/* Structures */


/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS mainKeys_start(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_stop(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_reportUcastStatus(struct _mainKeys_t *pMainKeys, TI_STATUS ucastStatus);
TI_STATUS mainKeys_reportBcastStatus(struct _mainKeys_t *pMainKeys, TI_STATUS bcastStatus);
void mainKeys_sessionTimeout(void *pMainKeys, TI_BOOL bTwdInitOccured);
TI_STATUS mainKeys_setKey(struct _mainKeys_t *pMainKeys, TSecurityKeys *pKey);
TI_STATUS mainKeys_removeKey(struct _mainKeys_t *pMainKeys, TSecurityKeys *pKey);
TI_STATUS mainKeys_setDefaultKeyId(struct _mainKeys_t *pMainKeys, TI_UINT8 keyId);
TI_STATUS mainKeys_getSessionKey(struct _mainKeys_t *pMainKeys, TI_UINT8 *pKey, TI_UINT32 *pKeyLen);

TI_STATUS mainKeys_startIdle(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_stopStart(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_stopUcastComplete(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_bcastCompleteUcastComplete(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_smTimeOut(void* data);
TI_STATUS mainKeys_stopBcastComplete(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_ucastCompleteBcastComplete(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeys_stopComplete(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeySmUnexpected(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeySmNop(struct _mainKeys_t *pMainKeys);
TI_STATUS mainKeySmSetKeyCompleted(struct _mainKeys_t *pMainKeys);




#endif

