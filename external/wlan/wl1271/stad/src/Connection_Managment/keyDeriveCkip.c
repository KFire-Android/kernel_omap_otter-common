/*
 * keyDeriveCkip.c
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

/** \file wepBroadcastKeyDerivation.c
 * \brief WEP broadcast key derivation implementation.
 *
 * \see wepBroadcastKeyDerivation.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	WEP broadcast key derivation                                *
 *   PURPOSE:   WEP broadcast key derivation                                *
 *                                                                          *
 ****************************************************************************/

#ifdef XCC_MODULE_INCLUDED
#define __FILE_ID__  FILE_ID_30
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"


#include "keyDerive.h"
#include "keyDeriveCkip.h"

#include "mainKeysSm.h"
#include "mainSecSm.h"
#include "admCtrl.h"

/**
*
* keyDeriveCkip_config
*
* \b Description: 
*
* CKIP key derivation init function: 
*							- Initializes the derive & remove callback functions
*							- Resets the key material in the system control block								
*
* \b ARGS: 
*
*  None
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/

TI_STATUS keyDeriveCkip_config(struct _keyDerive_t *pKeyDerive)
{
    pKeyDerive->derive = keyDeriveCkip_derive;
    pKeyDerive->remove = keyDeriveCkip_remove;

    return TI_OK;
}


/**
*
* keyDeriveCkip_derive
*
* \b Description: 
*
* CKIP key derivation function: 
*							- Decodes the key material.
*							- Distribute the decoded key material to the driver.
*
* \b ARGS: 
*
*  I - p - Pointer to the encoded key material.
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/

TI_STATUS keyDeriveCkip_derive(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey)
{
    TI_STATUS           status;
    TSecurityKeys      key;
    TI_UINT8               ckipIndex, keyIndex;
    TI_UINT8               ckipKey[KEY_DERIVE_CKIP_ENC_LEN];

    key.keyType = KEY_XCC;
    key.keyIndex = (TI_UINT8)pEncodedKey->keyId;

    if (pEncodedKey->keyLen != KEY_DERIVE_CKIP_ENC_LEN)
    {
        if ((pEncodedKey->keyLen != KEY_DERIVE_CKIP_5_LEN) && (pEncodedKey->keyLen != KEY_DERIVE_CKIP_13_LEN))
        {
            TRACE1(pKeyDerive->hReport, REPORT_SEVERITY_ERROR, "KEY_DERIVE_CKIP: ERROR: wrong key length %d !!!\n", pEncodedKey->keyLen);
            return TI_NOK;
        }

        keyIndex=0;
        for (ckipIndex=0; ckipIndex<KEY_DERIVE_CKIP_ENC_LEN; ckipIndex++)
        {
           ckipKey[ckipIndex]= pEncodedKey->pData[keyIndex];
		   keyIndex++;
		   if (keyIndex >= pEncodedKey->keyLen)
		   {
				keyIndex = 0;
		   }
           /*keyIndex = ((keyIndex+1) <pEncodedKey->keyLen) ? keyIndex+1 : 0;*/
        }
    }
    else 
    {
        for (ckipIndex=0; ckipIndex<KEY_DERIVE_CKIP_ENC_LEN; ckipIndex++)
        {
           ckipKey[ckipIndex]= pEncodedKey->pData[ckipIndex];
        }
    }

    if (pKeyDerive->pMainKeys->pParent->pParent->pAdmCtrl->encrInSw)
    {
        key.encLen = KEY_DERIVE_CKIP_ENC_LEN;
    }
    else
    {
        key.encLen = pEncodedKey->keyLen;
    }
    
    /* Copy encryption key - not expand */
    os_memoryCopy(pKeyDerive->hOs, (void*)key.encKey, ckipKey, key.encLen);
    /* Copy the MIC keys */
    os_memoryCopy(pKeyDerive->hOs, (void*)key.micRxKey, ckipKey, KEY_DERIVE_CKIP_ENC_LEN);
    os_memoryCopy(pKeyDerive->hOs, (void*)key.micTxKey, ckipKey, KEY_DERIVE_CKIP_ENC_LEN);

    status = pKeyDerive->pMainKeys->setKey(pKeyDerive->pMainKeys, &key);
    if (status == TI_OK)
    {
        os_memoryCopy(pKeyDerive->hOs, &pKeyDerive->key, pEncodedKey, sizeof(encodedKeyMaterial_t));
    }

    return status;
}
 
/**
*
* wepBroadcastKeyDerivationRemove
*
* \b Description: 
*
* WEP broadcast key removal function: 
*							- Remove the key material from the driver.
*
* \b ARGS: 
*
*  None.
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/

TI_STATUS keyDeriveCkip_remove(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey)
{
    TI_STATUS status;
    TSecurityKeys  key;

    os_memoryZero(pKeyDerive->hOs, &key, sizeof(TSecurityKeys));
    key.keyType = KEY_XCC;
    key.keyIndex = (TI_UINT8)pEncodedKey->keyId;
    key.encLen = KEY_DERIVE_CKIP_ENC_LEN;
	MAC_COPY (key.macAddress, pEncodedKey->pData);

    status = pKeyDerive->pMainKeys->removeKey(pKeyDerive->pMainKeys, &key);
    if (status == TI_OK)
    {
        os_memoryZero(pKeyDerive->hOs, &pKeyDerive->key, sizeof(encodedKeyMaterial_t));
    }

    return status;
}

#endif /* XCC_MODULE_INCLUDED */

