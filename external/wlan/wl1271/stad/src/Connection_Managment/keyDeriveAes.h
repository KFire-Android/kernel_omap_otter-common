/*
 * keyDeriveAes.h
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

/** \file keyDeriveAes.h
 *  \brief AES key derivation API
 *
 *  \see keyDeriveAes.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:	AES encryption key derivation                               *
 *   PURPOSE:   AES encryption key derivation API                           *
 *                                                                          *
 ****************************************************************************/

#ifndef _KEY_DERIVE_AES_H
#define _KEY_DERIVE_AES_H

#include "rsnApi.h"
#include "keyTypes.h"

#include "keyDerive.h"

#define DERIVE_AES_KEY_LEN 16   /* 128 bit key  */


typedef struct
{
    TI_UINT8   macAddress[MAC_ADDR_LEN];
    TI_UINT8   keyRSC[KEY_RSC_LEN]; 
    TI_UINT8   encKey[DERIVE_AES_KEY_LEN];
} keyMaterialAes_t;

/* AES key derivation init function. */
TI_STATUS keyDeriveAes_config(struct _keyDerive_t *pKeyDerive);

TI_STATUS keyDeriveAes_derive(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);
TI_STATUS keyDeriveAes_remove(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);

#endif /*  _KEY_DERIVE_AES_H  */
