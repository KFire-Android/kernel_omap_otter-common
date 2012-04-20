/*
 * unicastKeyNone.c
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

/** \file unicastKeyNone.c
 * \brief station unicast key None implementation
 *
 * \see unicastKeyNone.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	station unicast key None		                            *
 *   PURPOSE:   station unicast key None implementation						*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_45
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "unicastKeyNone.h"
#include "mainKeysSm.h"


TI_STATUS unicastKeyNone_start(struct _unicastKey_t *pUnicastKey);
TI_STATUS unicastKeyNone_distribute(struct _unicastKey_t *pUnicastKey, encodedKeyMaterial_t *pEncodedKeyMaterial);



/**
*
* Function  - Config KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_UnicastKeyRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

TI_STATUS unicastKeyNone_config(struct _unicastKey_t *pUnicastKey)
{

	pUnicastKey->start = unicastKeyNone_start;
	pUnicastKey->stop = unicastKeySmNop;
	pUnicastKey->recvFailure = unicastKeySmNop;
	pUnicastKey->recvSuccess = unicastKeyNone_distribute;

	pUnicastKey->currentState = 0;

	
	return TI_OK;
}

/**
*
* unicastKeyNone_start
*
* \b Description: 
*
* report the main key SM of unicast complete, whithout wating for keys.
*
* \b ARGS:
*
*  I   - pUnicastKey - context  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS unicastKeyNone_start(struct _unicastKey_t *pUnicastKey)
{
	TI_STATUS	status=TI_NOK;

	if (pUnicastKey->pParent->reportUcastStatus!=NULL)
    {
		status = pUnicastKey->pParent->reportUcastStatus(pUnicastKey->pParent, TI_OK);
    }

	return status;
}

/**
*
* unicastKeyNone_distribute
*
* \b Description: 
*
* Distribute unicast key material to the driver and report the main key SM on unicast complete.
*
* \b ARGS:
*
*  I   - pData - Encoded key material  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS unicastKeyNone_distribute(struct _unicastKey_t *pUnicastKey, encodedKeyMaterial_t *pEncodedKeyMaterial)
{
	TI_STATUS  status=TI_NOK;
	
	if ((pUnicastKey==NULL) || (pEncodedKeyMaterial==NULL))
    {
        return TI_NOK;
    }
    
    if (pUnicastKey->pKeyDerive->derive!=NULL)
    {
        status = pUnicastKey->pKeyDerive->derive(pUnicastKey->pKeyDerive, 
                                                       pEncodedKeyMaterial);
    }
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	if (pUnicastKey->pParent->setDefaultKeyId!=NULL)
    {
        status = pUnicastKey->pParent->setDefaultKeyId(pUnicastKey->pParent,
                                                       (TI_UINT8)pEncodedKeyMaterial->keyId);
    }
	if (status != TI_OK)
	{
		return status;
	}

	if (pUnicastKey->pParent->reportUcastStatus!=NULL)
    {
        status = pUnicastKey->pParent->reportUcastStatus(pUnicastKey->pParent, TI_OK);
    }

	return status;
}
