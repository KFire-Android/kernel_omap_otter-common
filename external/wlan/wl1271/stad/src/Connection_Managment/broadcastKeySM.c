/*
 * broadcastKeySM.c
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

/** \file broadcastKeySM.c
 * \brief broadcast key SM implementation
 *
 * \see broadcastKeySM.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	broadcast key SM                               *
 *   PURPOSE:   broadcast key SM implementation				*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_24
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "keyDerive.h"

#include "broadcastKeySM.h"
#include "broadcastKey802_1x.h"
#include "broadcastKeyNone.h"

/** number of states in the state machine */
#define	BCAST_KEY_MAX_NUM_STATES		3

/** number of events in the state machine */
#define	BCAST_KEY_MAX_NUM_EVENTS		4


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

broadcastKey_t* broadcastKey_create(TI_HANDLE hOs)
{
	TI_STATUS				status;
	broadcastKey_t 		*pBroadcastKey;

	/* allocate key parser context memory */
	pBroadcastKey = (broadcastKey_t*)os_memoryAlloc(hOs, sizeof(broadcastKey_t));
	if (pBroadcastKey == NULL)
	{
		return NULL;
	}

	os_memoryZero(hOs, pBroadcastKey, sizeof(broadcastKey_t));

	/* allocate memory for association state machine */
	status = fsm_Create(hOs, &pBroadcastKey->pBcastKeySm, BCAST_KEY_MAX_NUM_STATES, BCAST_KEY_MAX_NUM_EVENTS);
	if (status != TI_OK)
	{
		os_memoryFree(hOs, pBroadcastKey, sizeof(broadcastKey_t));
		return NULL;
	}

	pBroadcastKey->pKeyDerive = keyDerive_create(hOs);
	if (pBroadcastKey->pKeyDerive == NULL)
	{
		fsm_Unload(hOs, pBroadcastKey->pBcastKeySm);
		os_memoryFree(hOs, pBroadcastKey, sizeof(broadcastKey_t));
		return NULL;
	}

	pBroadcastKey->hOs = hOs;

	return pBroadcastKey;
}

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

TI_STATUS broadcastKey_unload(struct _broadcastKey_t *pBroadcastKey)
{
	TI_STATUS		status;

	status = keyDerive_unload(pBroadcastKey->pKeyDerive);
                    
	if (status != TI_OK)
	{
        TRACE0(pBroadcastKey->hReport, REPORT_SEVERITY_CONSOLE, "BCAST_KEY_SM: Error in unloading key derivation module\n");
		WLAN_OS_REPORT(("BCAST_KEY_SM: Error in unloading key derivation module\n"));
	}

	status = fsm_Unload(pBroadcastKey->hOs, pBroadcastKey->pBcastKeySm);
	if (status != TI_OK)
	{
        TRACE0(pBroadcastKey->hReport, REPORT_SEVERITY_CONSOLE, "BCAST_KEY_SM: Error in unloading state machine\n");
		WLAN_OS_REPORT(("BCAST_KEY_SM: Error in unloading state machine\n"));
	}

	/* free key parser context memory */
	os_memoryFree(pBroadcastKey->hOs, pBroadcastKey, sizeof(broadcastKey_t));

	return TI_OK;
}

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

TI_STATUS broadcastKey_config(struct _broadcastKey_t *pBroadcastKey,
                              TRsnPaeConfig *pPaeConfig,
                              struct _mainKeys_t *pParent,
                              TI_HANDLE hReport,
                              TI_HANDLE hOs)
{
	TI_STATUS		status = TI_NOK;

	pBroadcastKey->hReport = hReport;
	pBroadcastKey->hOs = hOs;
	pBroadcastKey->pParent = pParent;

	/* configure according to the keyMng suite and cipher suite */
	switch (pPaeConfig->keyExchangeProtocol)
	{
	case RSN_KEY_MNG_NONE:
		status = broadcastKeyNone_config(pBroadcastKey);
		break;
	case RSN_KEY_MNG_802_1X:
		if (pPaeConfig->broadcastSuite == TWD_CIPHER_NONE)
		{
			status = broadcastKeyNone_config(pBroadcastKey);
		} else {
			status = broadcastKey802_1x_config(pBroadcastKey);
		}
		break;
    default:
		status = broadcastKeyNone_config(pBroadcastKey);
		break;
	}

	status = keyDerive_config(pBroadcastKey->pKeyDerive, pPaeConfig->broadcastSuite, pParent, hReport, hOs);

	return status;
}


TI_STATUS broadcastKeySmUnexpected(struct _broadcastKey_t *pBroadcastKey)
{
TRACE0(pBroadcastKey->hReport, REPORT_SEVERITY_ERROR, "BROADCAST_KEY_SM: ERROR: UnExpected Event\n");

	return(TI_NOK);
}

TI_STATUS broadcastKeySmNop(struct _broadcastKey_t *pBroadcastKey)
{
	return(TI_OK);
}

