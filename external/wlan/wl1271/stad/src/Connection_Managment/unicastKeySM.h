/*
 * unicastKeySM.h
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

/** \file unicastKeySM.h
 *  \brief station unicast key SM API
 *
 *  \see unicastKeySM.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  station unicast key SM	                                	*
 *   PURPOSE: station unicast key SM API		                            *
 *                                                                          *
 ****************************************************************************/

#ifndef _UNICAST_KEY_SM_H
#define _UNICAST_KEY_SM_H

#include "paramOut.h"
#include "fsm.h"
#include "rsnApi.h"
#include "keyTypes.h"

#include "keyDerive.h"

/* Constants */

/* Enumerations */

/* Typedefs */

typedef struct _unicastKey_t    unicastKey_t;

/* Main Sec SM functions */
typedef TI_STATUS (*unicastKeySmStart_t)(struct _unicastKey_t *pUnicastKey);
typedef TI_STATUS (*unicastKeySmStop_t)(struct _unicastKey_t *pUnicastKey);
typedef TI_STATUS (*unicastKeySmRecvSuccess_t)(struct _unicastKey_t *pUnicastKey, encodedKeyMaterial_t *pEncodedKeyMaterial);
typedef TI_STATUS (*unicastKeySmRecvFailure_t)(struct _unicastKey_t *pUnicastKey);

/* Structures */

/* State machine associated data structures. */
typedef struct
{
	encodedKeyMaterial_t    *pEncodedKeyMaterial;
} unicastKeyData_t;

struct _unicastKey_t
{
	TI_UINT8                               currentState;
	fsm_stateMachine_t	                *pUcastKeySm;
    unicastKeyData_t					data;

    struct _mainKeys_t                 	*pParent;
	keyDerive_t							*pKeyDerive;
	
	TI_HANDLE			                hReport;
	TI_HANDLE			                hOs;
    
    unicastKeySmStart_t               	start;
    unicastKeySmStop_t                	stop;
    unicastKeySmRecvSuccess_t			recvSuccess;
    unicastKeySmRecvFailure_t			recvFailure;
};

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

unicastKey_t* unicastKey_create(TI_HANDLE hOs);

TI_STATUS unicastKey_unload(unicastKey_t *punicastKey);

TI_STATUS unicastKey_config(struct _unicastKey_t *pUnicastKey, 
						   TRsnPaeConfig *pPaeConfig, 
						   struct _mainKeys_t *pParent,
						   TI_HANDLE hReport,
						   TI_HANDLE hOs);

TI_STATUS unicastKeySmUnexpected(struct _unicastKey_t *pUnicastKey);

TI_STATUS unicastKeySmNop(struct _unicastKey_t *pUnicastKey);

#endif /*  _UNICAST_KEY_SM_H*/
