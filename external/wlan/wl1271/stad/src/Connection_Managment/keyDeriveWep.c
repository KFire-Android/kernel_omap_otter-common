/*
 * keyDeriveWep.c
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

#define __FILE_ID__  FILE_ID_32
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "keyDerive.h"
#include "keyDeriveWep.h"

#include "mainKeysSm.h"

/**
*
* rsn_wepBroadcastKeyDerivationInit
*
* \b Description: 
*
* WEP broadcast key derivation init function: 
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

TI_STATUS keyDeriveWep_config(struct _keyDerive_t *pKeyDerive)
{
	pKeyDerive->derive = keyDeriveWep_derive;
	pKeyDerive->remove = keyDeriveWep_remove;

	return TI_OK;
}


/**
*
* wepBroadcastKeyDerivationDerive
*
* \b Description: 
*
* WEP broadcast key derivation function: 
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

TI_STATUS keyDeriveWep_derive(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey)
{
	TI_STATUS status;
	TSecurityKeys	key;
	
    if (pEncodedKey==NULL)
    {
        return TI_NOK;
    }
	
	if ((pEncodedKey->keyLen != DERIVE_WEP_KEY_LEN_40) && 
		(pEncodedKey->keyLen != DERIVE_WEP_KEY_LEN_104) && 
		(pEncodedKey->keyLen != DERIVE_WEP_KEY_LEN_232))
	{	
        TRACE1(pKeyDerive->hReport, REPORT_SEVERITY_ERROR, "DeriveWep_derive: ERROR: it is not WEP key lenghth (len=%d) !!!\n", pEncodedKey->keyLen);
        return TI_NOK;
   	}

	key.keyType = KEY_WEP;
	key.keyIndex = (TI_UINT8)pEncodedKey->keyId;
	key.encLen = (TI_UINT16)pEncodedKey->keyLen;
	os_memoryCopy(pKeyDerive->hOs, (void *)key.encKey, pEncodedKey->pData, pEncodedKey->keyLen);

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

TI_STATUS keyDeriveWep_remove(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey)
{
	TI_STATUS status;
	TSecurityKeys	key;
	
    os_memoryZero(pKeyDerive->hOs, &key, sizeof(TSecurityKeys));
	key.keyType = KEY_WEP;
	key.keyIndex = (TI_UINT8)pEncodedKey->keyId;
	key.encLen = (TI_UINT16)pKeyDerive->key.keyLen;
	MAC_COPY (key.macAddress, pEncodedKey->pData);

	status = pKeyDerive->pMainKeys->removeKey(pKeyDerive->pMainKeys, &key);
	if (status == TI_OK)
	{
		os_memoryZero(pKeyDerive->hOs, &pKeyDerive->key, sizeof(encodedKeyMaterial_t));
	}
	
	return status;
}



TI_STATUS keyDeriveNone_config(struct _keyDerive_t *pKeyDerive)
{
	pKeyDerive->derive = keyDeriveNone_derive;
	pKeyDerive->remove = keyDeriveNone_remove;

	return TI_OK;
}


TI_STATUS keyDeriveNone_derive(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey)
{
	TSecurityKeys	key;
	
    if (pEncodedKey==NULL)
    {
        return TI_NOK;
    }

	if ((pEncodedKey->keyLen != DERIVE_WEP_KEY_LEN_40) && 
		(pEncodedKey->keyLen != DERIVE_WEP_KEY_LEN_104) && 
		(pEncodedKey->keyLen != DERIVE_WEP_KEY_LEN_232))
	{	
        return TI_NOK;
   	}

	key.keyType = KEY_WEP;
	key.keyIndex = (TI_UINT8)pEncodedKey->keyId;
	key.encLen = (TI_UINT16)pEncodedKey->keyLen;
	os_memoryCopy(pKeyDerive->hOs, (void *)key.encKey, pEncodedKey->pData, pEncodedKey->keyLen);

	pKeyDerive->pMainKeys->setKey(pKeyDerive->pMainKeys, &key);
	
	return TI_OK;
}


/**
*
* keyDeriveNone_remove
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

TI_STATUS keyDeriveNone_remove(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey)
{
	
    return TI_OK;
}




