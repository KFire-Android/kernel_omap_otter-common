/*
 * keyParser.c
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

/** \file keyParser.c
 * \brief KEY parser module implementation.
 *
 * \see keyParser.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	KEY parser                                                  *
 *   PURPOSE:   KEY parser implementation									*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_33
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "keyParser.h" 
#include "keyParserExternal.h"
#include "keyParserWep.h"

TI_STATUS keyParserNone_config(keyParser_t *pKeyParser);

/**
*
* Function  - Init KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_keyParserRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

keyParser_t* keyParser_create(TI_HANDLE hOs)
{
	keyParser_t 		*pKeyParser;

	/* allocate key parser context memory */
	pKeyParser = (keyParser_t*)os_memoryCAlloc(hOs, 1, sizeof(keyParser_t));
	if (pKeyParser == NULL)
	{
		return NULL;
	}

	pKeyParser->hOs = hOs;

	return pKeyParser;
}

/**
*
* Function  - Init KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_keyParserRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

TI_STATUS keyParser_unload(struct _keyParser_t *pKeyParser)
{
	/* free key parser context memory */
	os_memoryFree(pKeyParser->hOs, pKeyParser, sizeof(keyParser_t));

	return TI_OK;
}

/**
*
* Function  - Init KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_keyParserRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

TI_STATUS keyParser_config(struct _keyParser_t *pKeyParser,
						TRsnPaeConfig *pPaeConfig,
                        struct _unicastKey_t *pUcastKey,
                        struct _broadcastKey_t *pBcastKey,
                        struct _mainKeys_t *pParent,
						TI_HANDLE hReport,
						TI_HANDLE hOs,
                        TI_HANDLE hCtrlData)
{
    
    pKeyParser->pParent = pParent;
    pKeyParser->pUcastKey = pUcastKey;
    pKeyParser->pBcastKey = pBcastKey;
    pKeyParser->pPaeConfig = pPaeConfig;

    pKeyParser->hReport = hReport;
	pKeyParser->hOs = hOs;
    pKeyParser->hCtrlData = hCtrlData;

	keyParserExternal_config(pKeyParser);
	
	return TI_OK;
}



TI_STATUS keyParserNone_config(keyParser_t *pKeyParser)
{
	pKeyParser->recv = NULL;
	pKeyParser->replayReset = NULL;
	
	return TI_OK;
	
}


TI_STATUS keyParser_nop(keyParser_t *pKeyParser)
{
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_INFORMATION, "KEY_PARSER: nop \n");
	
	return TI_OK;
}


