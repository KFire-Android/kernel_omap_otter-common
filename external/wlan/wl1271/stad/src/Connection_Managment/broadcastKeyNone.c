/*
 * broadcastKeyNone.c
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

/** \file broadcastKeyNone.c
 * \brief broadcast key None implementation
 *
 * \see broadcastKeyNone.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	None station broadcast key SM                             *
 *   PURPOSE:   None station broadcast key SM implementation				*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_23
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "keyDerive.h"

#include "broadcastKeyNone.h"
#include "mainKeysSm.h"



TI_STATUS broadcastKeyNone_distribute(struct _broadcastKey_t *pBroadcastKey, encodedKeyMaterial_t *pEncodedKeyMaterial);
TI_STATUS broadcastKeyNone_start(struct _broadcastKey_t *pBroadcastKey);



/**
*
* Function  - Init KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_BroadcastKeyRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

TI_STATUS broadcastKeyNone_config(struct _broadcastKey_t *pBroadcastKey)
{

	pBroadcastKey->start = broadcastKeyNone_start;
	pBroadcastKey->stop = broadcastKeySmNop;
	pBroadcastKey->recvFailure = broadcastKeySmNop;
	pBroadcastKey->recvSuccess = broadcastKeyNone_distribute;

	pBroadcastKey->currentState = 0;

	return TI_OK;
}


/**
*
* broadcastKeyNone_start
*
* \b Description: 
*
* report the main key SM of broadcast complete, whithout wating for keys.
*
* \b ARGS:
*
*  I   - pBroadcastKey - context  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS broadcastKeyNone_start(struct _broadcastKey_t *pBroadcastKey)
{
	TI_STATUS	status=TI_NOK;

	if (pBroadcastKey->pParent->reportBcastStatus!=NULL)
    {
		status = pBroadcastKey->pParent->reportBcastStatus(pBroadcastKey->pParent, TI_OK);
    }

	return status;
}

/**
*
* broadcastKeyNone_distribute
*
* \b Description: 
*
* Distribute broadcast key material to the driver and report the main key SM on broadcast complete.
*
* \b ARGS:
*
*  I   - pData - Encoded key material  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS broadcastKeyNone_distribute(struct _broadcastKey_t *pBroadcastKey, encodedKeyMaterial_t *pEncodedKeyMaterial)
{
	TI_STATUS  status=TI_NOK;
	
	if (pBroadcastKey->pKeyDerive->derive!=NULL)
    {
        status = pBroadcastKey->pKeyDerive->derive(pBroadcastKey->pKeyDerive, 
                                                   pEncodedKeyMaterial);
    }
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	if (pBroadcastKey->pParent->reportBcastStatus!=NULL)
    {
        status = pBroadcastKey->pParent->reportBcastStatus(pBroadcastKey->pParent, TI_OK);
    }

	return status;
}

