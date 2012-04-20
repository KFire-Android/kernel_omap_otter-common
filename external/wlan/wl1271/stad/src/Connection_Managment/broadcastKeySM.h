/*
 * broadcastKeySM.h
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

/** \file broadcastKeySM.h
 *  \brief station broadcast key SM API
 *
 *  \see broadcastKeySM.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  station broadcast key SM	                                	*
 *   PURPOSE: station broadcast key SM API		                            *
 *                                                                          *
 ****************************************************************************/

#ifndef _BROADCAST_KEY_SM_H
#define _BROADCAST_KEY_SM_H

#include "paramOut.h"
#include "fsm.h"
#include "rsnApi.h"
#include "keyTypes.h"

/* Constants */

/* Enumerations */

/* Typedefs */

typedef struct _broadcastKey_t    broadcastKey_t;

/* Main Sec SM functions */
typedef TI_STATUS (*broadcastKeySmStart_t)(struct _broadcastKey_t *pBroadcastKey);
typedef TI_STATUS (*broadcastKeySmStop_t)(struct _broadcastKey_t *pBroadcastKey);
typedef TI_STATUS (*broadcastKeySmRecvSuccess_t)(struct _broadcastKey_t *pBroadcastKey, encodedKeyMaterial_t *pEncodedKeyMaterial);
typedef TI_STATUS (*broadcastKeySmRecvFailure_t)(struct _broadcastKey_t *pBroadcastKey);

/* Structures */

/* State machine associated data structures. */
typedef struct
{
	encodedKeyMaterial_t    *pEncodedKeyMaterial;
} broadcastKeyData_t;

struct _broadcastKey_t
{
	TI_UINT8                               currentState;
	fsm_stateMachine_t	                *pBcastKeySm;
    broadcastKeyData_t					data;

    struct _mainKeys_t                 	*pParent;
	struct _keyDerive_t					*pKeyDerive;
	
	TI_HANDLE			                hReport;
	TI_HANDLE			                hOs;
    
    broadcastKeySmStart_t               start;
    broadcastKeySmStop_t                stop;
    broadcastKeySmRecvSuccess_t			recvSuccess;
    broadcastKeySmRecvFailure_t			recvFailure;
};

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

broadcastKey_t* broadcastKey_create(TI_HANDLE hOs);

TI_STATUS broadcastKey_unload(broadcastKey_t *pBroadcastKey);

TI_STATUS broadcastKey_config(broadcastKey_t *pBroadcastKey, 
						   TRsnPaeConfig *pPaeConfig, 
						   struct _mainKeys_t *pParent,
						   TI_HANDLE hReport,
						   TI_HANDLE hOs);

TI_STATUS broadcastKeySmUnexpected(struct _broadcastKey_t *pBroadcastKey);

TI_STATUS broadcastKeySmNop(struct _broadcastKey_t *pBroadcastKey);

#endif /*  _BROADCAST_KEY_SM_H*/
