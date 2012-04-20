/*
 * mainKeysSm.h
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

#ifndef _MAIN_KEYS_SM_H
#define _MAIN_KEYS_SM_H

#include "paramOut.h"
#include "fsm.h"
#include "rsnApi.h"
#include "keyTypes.h"
#include "keyParser.h"
#include "unicastKeySM.h"
#include "broadcastKeySM.h"

/* Constants */

#define MAIN_KEYS_TIMEOUT			20000

/* Enumerations */

/* Typedefs */

typedef struct _mainKeys_t    mainKeys_t;

/* Main Sec SM functions */
typedef TI_STATUS (*mainKeysSmStart_t)(struct _mainKeys_t *pMainKeys);
typedef TI_STATUS (*mainKeysSmStop_t)(struct _mainKeys_t *pMainKeys);
typedef TI_STATUS (*mainKeysSmReportUcastStatus_t)(struct _mainKeys_t *pMainKeys, TI_STATUS authStatus);
typedef TI_STATUS (*mainKeysSmReportBcastStatus_t)(struct _mainKeys_t *pMainKeys, TI_STATUS authStatus);
typedef TI_STATUS (*mainKeysSmReportReKey_t)(struct _mainKeys_t *pMainKeys);
typedef TI_STATUS (*mainKeysSmSetKey_t)(struct _mainKeys_t *pMainKeys, TSecurityKeys *pKey);
typedef TI_STATUS (*mainKeysSmRemoveKey_t)(struct _mainKeys_t *pMainKeys, TSecurityKeys *pKey);
typedef TI_STATUS (*mainKeysSmSetDefaultKeyId_t)(struct _mainKeys_t *pMainKeys, TI_UINT8 keyId);
typedef TI_STATUS (*mainKeysSmGetSessionKey_t)(struct _mainKeys_t *pMainKeys, TI_UINT8 *pKey, TI_UINT32 *pKeyLen);

/* Structures */

typedef struct
{
	TI_STATUS		status;
} mainKeysData_t;

struct _mainKeys_t
{
	TI_UINT8                            currentState;
	TI_UINT32                           keysTimeout;
    mainKeysData_t						data;
	fsm_stateMachine_t	                *pMainKeysSm;
    TI_BOOL			 	                mainKeysTimeoutCounter;

	TI_HANDLE							hCtrlData;
	TI_HANDLE			                hReport;
	TI_HANDLE			                hOs;
    TI_HANDLE                           hEvHandler;
    TI_HANDLE                           hConn;
    TI_HANDLE                           hRsn;
	TI_HANDLE							hTimer;

    TI_HANDLE							hSessionTimer;

	keyParser_t							*pKeyParser;
	unicastKey_t						*pUcastSm;
	broadcastKey_t						*pBcastSm;
    struct _mainSec_t                  	*pParent;

    mainKeysSmStart_t                   start;
    mainKeysSmStop_t                    stop;
    mainKeysSmReportUcastStatus_t		reportUcastStatus;
    mainKeysSmReportBcastStatus_t       reportBcastStatus;
    mainKeysSmReportReKey_t				reportReKey;
	mainKeysSmSetKey_t					setKey; 
	mainKeysSmRemoveKey_t				removeKey;
	mainKeysSmSetDefaultKeyId_t			setDefaultKeyId;
	mainKeysSmGetSessionKey_t			getSessionKey;
};

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

mainKeys_t* mainKeys_create(TI_HANDLE hOs);

TI_STATUS mainKeys_unload(mainKeys_t *pmainKeys);

TI_STATUS mainKeys_config (mainKeys_t    *pMainKeys, 
                           TRsnPaeConfig *pPaeConfig, 
                           void          *pParent,
                           TI_HANDLE      hReport,
                           TI_HANDLE      hOs,
                           TI_HANDLE      hCtrlData,
                           TI_HANDLE      hEvHandler,
                           TI_HANDLE      hConn,
                           TI_HANDLE      hRsn,
                           TI_HANDLE      hTimer);

void mainKeys_reAuth(TI_HANDLE pHandle);

#endif

