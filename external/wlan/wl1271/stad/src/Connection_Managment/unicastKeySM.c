/*
 * unicastKeySM.c
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

/** \file unicastKeySM.c
 * \brief station unicast key SM implementation
 *
 * \see unicastKeySM.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	station unicast key SM		                                *
 *   PURPOSE:   station unicast key SM implementation						*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_46
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "unicastKeySM.h"
#include "unicastKey802_1x.h"
#include "unicastKeyNone.h"

/** number of states in the state machine */
#define	UCAST_KEY_MAX_NUM_STATES		3

/** number of events in the state machine */
#define	UCAST_KEY_MAX_NUM_EVENTS		4


/**
*
* Function  - Init KEY Parser module.
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

unicastKey_t* unicastKey_create(TI_HANDLE hOs)
{
	TI_STATUS				status;
	unicastKey_t 		*pUnicastKey;

	/* allocate key parser context memory */
	pUnicastKey = (unicastKey_t*)os_memoryAlloc(hOs, sizeof(unicastKey_t));
	if (pUnicastKey == NULL)
	{
		return NULL;
	}

	os_memoryZero(hOs, pUnicastKey, sizeof(unicastKey_t));

	/* allocate memory for association state machine */
	status = fsm_Create(hOs, &pUnicastKey->pUcastKeySm, UCAST_KEY_MAX_NUM_STATES, UCAST_KEY_MAX_NUM_EVENTS);
	if (status != TI_OK)
	{
		os_memoryFree(hOs, pUnicastKey, sizeof(unicastKey_t));
		return NULL;
	}

	pUnicastKey->pKeyDerive = keyDerive_create(hOs);
	if (pUnicastKey->pKeyDerive == NULL)
	{
		fsm_Unload(hOs, pUnicastKey->pUcastKeySm);
		os_memoryFree(hOs, pUnicastKey, sizeof(unicastKey_t));
		return NULL;
	}

	pUnicastKey->hOs = hOs;

	return pUnicastKey;
}

/**
*
* Function  - Init KEY Parser module.
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

TI_STATUS unicastKey_unload(struct _unicastKey_t *pUnicastKey)
{
	TI_STATUS		status;

	status = keyDerive_unload(pUnicastKey->pKeyDerive);
	if (status != TI_OK)
	{                  
        TRACE0(pUnicastKey->hReport, REPORT_SEVERITY_CONSOLE,"BCAST_KEY_SM: Error in unloading key derivation module\n");
		WLAN_OS_REPORT(("BCAST_KEY_SM: Error in unloading key derivation module\n"));
	}

	status = fsm_Unload(pUnicastKey->hOs, pUnicastKey->pUcastKeySm);
	if (status != TI_OK)
	{
        TRACE0(pUnicastKey->hReport, REPORT_SEVERITY_CONSOLE,"BCAST_KEY_SM: Error in unloading state machine\n");
		WLAN_OS_REPORT(("BCAST_KEY_SM: Error in unloading state machine\n"));
	}

	/* free key parser context memory */
	os_memoryFree(pUnicastKey->hOs, pUnicastKey, sizeof(unicastKey_t));

	return TI_OK;
}

/**
*
* Function  - Init KEY Parser module.
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

TI_STATUS unicastKey_config(struct _unicastKey_t *pUnicastKey,
						 TRsnPaeConfig *pPaeConfig,
						 struct _mainKeys_t *pParent,
						 TI_HANDLE hReport,
						 TI_HANDLE hOs)
{
	TI_STATUS		status = TI_NOK;

	pUnicastKey->hReport = hReport;
	pUnicastKey->hOs = hOs;
	pUnicastKey->pParent = pParent;

	/* configure according to the keyMng suite and cipher suite */
    switch (pPaeConfig->keyExchangeProtocol)
    {
    case RSN_KEY_MNG_NONE:
       status = unicastKeyNone_config(pUnicastKey);
       break;
    case RSN_KEY_MNG_802_1X:
       if (pPaeConfig->unicastSuite == TWD_CIPHER_NONE)
   	    {
   	    	status = unicastKeyNone_config(pUnicastKey);
   	    } else {
   	    	status = unicastKey802_1x_config(pUnicastKey);
   	    }
   	break;
    default:
   	    status = unicastKeyNone_config(pUnicastKey);
   	    break;
    }

	status = keyDerive_config(pUnicastKey->pKeyDerive, pPaeConfig->unicastSuite, pParent, hReport, hOs);
	
	return status;
}


TI_STATUS unicastKeySmUnexpected(struct _unicastKey_t *pUnicastKey)
{
TRACE0(pUnicastKey->hReport, REPORT_SEVERITY_ERROR, "UNICAST_KEY_SM: ERROR: UnExpected Event\n");

	return(TI_NOK);
}

TI_STATUS unicastKeySmNop(struct _unicastKey_t *pUnicastKey)
{
	return(TI_OK);
}

