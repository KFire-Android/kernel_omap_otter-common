/*
 * keyDerive.c
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

/** \file keyDeriveSM.c
 * \brief station unicast key SM implementation
 *
 * \see keyDeriveSM.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	station unicast key SM		                                *
 *   PURPOSE:   station unicast key SM implementation						*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_28
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "keyDerive.h"
#include "keyDeriveWep.h"
#include "keyDeriveTkip.h"
#include "keyDeriveAes.h"
#ifdef XCC_MODULE_INCLUDED
#include "keyDeriveCkip.h"
#endif

/**
*
* Function  - Init KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_KeyDeriveRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

keyDerive_t* keyDerive_create(TI_HANDLE hOs)
{
	keyDerive_t 		*pKeyDerive;

	/* allocate key parser context memory */
	pKeyDerive = (keyDerive_t*)os_memoryAlloc(hOs, sizeof(keyDerive_t));
	if (pKeyDerive == NULL)
	{
		return NULL;
	}

	os_memoryZero(hOs, pKeyDerive, sizeof(keyDerive_t));

	pKeyDerive->hOs = hOs;

	return pKeyDerive;
}

/**
*
* Function  - Init KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_KeyDeriveRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

TI_STATUS keyDerive_unload(struct _keyDerive_t *pKeyDerive)
{
	/* free key parser context memory */
	os_memoryFree(pKeyDerive->hOs, pKeyDerive, sizeof(keyDerive_t));

	return TI_OK;
}

/**
*
* Function  - Init KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_KeyDeriveRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

TI_STATUS keyDerive_config(struct _keyDerive_t *pKeyDerive,
						ECipherSuite cipher,
						struct _mainKeys_t *pMainKeys,
						TI_HANDLE hReport,
						TI_HANDLE hOs)
{

	TI_STATUS		status = TI_NOK;

	pKeyDerive->hReport = hReport;
	pKeyDerive->hOs = hOs;
	pKeyDerive->pMainKeys = pMainKeys;

    switch (cipher)
	{
    case TWD_CIPHER_NONE:
		status = keyDeriveNone_config(pKeyDerive);
        break;
	case TWD_CIPHER_WEP:
    case TWD_CIPHER_WEP104:
		status = keyDeriveWep_config(pKeyDerive);
		break;
	case TWD_CIPHER_TKIP:
		status = keyDeriveTkip_config(pKeyDerive);
		break;
#ifdef XCC_MODULE_INCLUDED
	case TWD_CIPHER_CKIP: 
  	status = keyDeriveCkip_config(pKeyDerive);
		break;
#endif

	case TWD_CIPHER_AES_CCMP:
		status = keyDeriveAes_config(pKeyDerive);
		break;
	default:
		return TI_NOK;
	}

	return status;
}



