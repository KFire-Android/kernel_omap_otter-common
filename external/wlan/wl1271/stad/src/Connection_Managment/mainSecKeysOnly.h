/*
 * mainSecKeysOnly.h
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

/** \file MainSecSm.h
 *  \brief RSN main security SM
 *
 *  \see MainSecSm.c
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	MainSecSm.h												   */
/*    PURPOSE:	RSN main security SM									   */
/*																	 	   */
/***************************************************************************/

#ifndef _MAIN_SEC_KEYS_ONLY_H
#define _MAIN_SEC_KEYS_ONLY_H

#include "fsm.h"
#include "mainSecSm.h"

/* Constants */

/** number of events in the state machine */
#define	MAIN_SEC_KEYS_ONLY_NUM_EVENTS		4

/** number of states in the state machine */
#define	MAIN_SEC_KEYS_ONLY_NUM_STATES		4

/* Enumerations */

/* state machine states */
typedef enum 
{
	MAIN_KO_STATE_IDLE				= 0,
	MAIN_KO_STATE_START				= 1,
	MAIN_KO_STATE_AUTHORIZED		= 2,
	MAIN_KO_STATE_NONAUTHORIZED		= 3
} main_keysOnlyStates;

/* State machine inputs */
typedef enum 
{
	MAIN_KO_EVENT_START				= 0,
	MAIN_KO_EVENT_STOP				= 1,
	MAIN_KO_EVENT_KEYS_COMPLETE		= 2,
	MAIN_KO_EVENT_SEC_ATTACK		= 3
} main_keysOnlyEvents;


/* Typedefs */

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS mainSecKeysOnly_config(mainSec_t *pMainSec, 
                            TRsnPaeConfig *pPaeConfig);

TI_STATUS mainSecKeysOnly_start(mainSec_t *pMainSec);

TI_STATUS mainSecKeysOnly_stop(mainSec_t *pMainSec);

TI_STATUS mainSecKeysOnly_setSessionKey(mainSec_t *pMainSec, TI_UINT8* pKey, TI_UINT8 keyLen);

TI_STATUS mainSecKeysOnly_getSessionKey(mainSec_t *pMainSec, TI_UINT8* pKey, TI_UINT32* pKeyLen);

TI_STATUS mainSecKeysOnly_reportKeysStatus(mainSec_t *pMainSec, TI_STATUS keyStatus);

/* state machine functions */

TI_STATUS mainSecKeysOnly_startIdle(struct _mainSec_t *pMainSec);

TI_STATUS mainSecKeysOnly_stopStart(struct _mainSec_t *pMainSec);

TI_STATUS mainSecKeysOnly_keysCompleteStart(struct _mainSec_t *pMainSec);

TI_STATUS mainSecKeysOnly_keysTOStart(struct _mainSec_t *pMainSec);

TI_STATUS mainSecKeysOnly_stopAuthorized(struct _mainSec_t *pMainSec);

TI_STATUS mainSecKeysOnly_stopNonAuthorized(struct _mainSec_t *pMainSec);

/* state machine action functions */

TI_STATUS mainSecKeysOnly_StartMainKeySm(void* pData);

TI_STATUS mainSecKeysOnly_StopMainKeySm(void* pData);

TI_STATUS mainSecKeysOnly_ReportAuthSuccess(void* pData);

TI_STATUS mainSecKeysOnly_ReportAuthFailure(void* pData);

TI_STATUS mainSecKeysOnly_Nop(void* pData);

TI_STATUS mainSecKeysOnly_unexpected(void* pData);

#endif

