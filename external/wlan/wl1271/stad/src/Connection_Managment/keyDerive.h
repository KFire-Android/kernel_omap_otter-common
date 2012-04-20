/*
 * keyDerive.h
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

#ifndef _KEY_DERIVE_H
#define _KEY_DERIVE_H

#include "paramOut.h"
#include "rsnApi.h"
#include "keyTypes.h"

/* Constants */

/* Enumerations */

/* Typedefs */

typedef struct _keyDerive_t    keyDerive_t;

/* Main Sec SM functions */
typedef TI_STATUS (*keyDeriveDerive_t)(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKeyMaterial);
typedef TI_STATUS (*keyDeriveRemove_t)(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKeyMaterial);

/* Structures */

/* State machine associated data structures. */
struct _keyDerive_t
{
	encodedKeyMaterial_t	key;

	struct _mainKeys_t     	*pMainKeys;
	
	TI_HANDLE				hReport;
	TI_HANDLE			    hOs;
    
	keyDeriveDerive_t		derive;
	keyDeriveRemove_t		remove;
};

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

keyDerive_t* keyDerive_create(TI_HANDLE hOs);

TI_STATUS keyDerive_unload(keyDerive_t *pKeyDerive);

TI_STATUS keyDerive_config(keyDerive_t *pKeyDerive, 
						ECipherSuite cipher, 
						struct _mainKeys_t *pParent,
						TI_HANDLE hReport,
						TI_HANDLE hOs);


#endif /*  _KEY_DERIVE_H*/
