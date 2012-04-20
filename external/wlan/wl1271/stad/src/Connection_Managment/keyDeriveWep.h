/*
 * keyDeriveWep.h
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

/** \file wepBroadcastKeyDerivation.h
 *  \brief WEP broadcast key derivation API
 *
 *  \see wepBroadcastKeyDerivation.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:	WEP broadcast key derivation                                *
 *   PURPOSE:   WEP broadcast key derivation API                            *
 *                                                                          *
 ****************************************************************************/

#ifndef _KEY_DERIVE_WEP_H
#define _KEY_DERIVE_WEP_H

#include "rsnApi.h"
#include "keyTypes.h"

#include "keyDerive.h"


#define DERIVE_WEP_KEY_LEN_40	 5  /* 40 bit (5 byte) key */
#define DERIVE_WEP_KEY_LEN_104	 13 /* 104 bit (13 byte) key */
#define DERIVE_WEP_KEY_LEN_232	 29 /* 232 bit (29 byte) key */


/* WEP broadcast key derivation init function. */
TI_STATUS keyDeriveWep_config(struct _keyDerive_t *pKeyDerive);

TI_STATUS keyDeriveWep_derive(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);

TI_STATUS keyDeriveWep_remove(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);

TI_STATUS keyDeriveNone_config(struct _keyDerive_t *pKeyDerive);

TI_STATUS keyDeriveNone_derive(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);

TI_STATUS keyDeriveNone_remove(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);

#endif /*  __INCLUDE_WEP_BROADCAST_KEY_DERIVATION_H*/
