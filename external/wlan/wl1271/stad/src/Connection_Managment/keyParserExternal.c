/*
 * keyParserExternal.c
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

/** \file keyParserExternal.c
 * \brief External key parser implementation.
 *
 * \see keyParser.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	External Key Parser                                             *
 *   PURPOSE:   EAP parser implementation                                   *
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_34
#include "tidef.h"
#include "osApi.h"
#include "report.h"

#include "keyTypes.h"

#include "keyParser.h"
#include "keyParserExternal.h"
#include "mainKeysSm.h"
#include "mainSecSm.h"
#include "admCtrl.h"

#include "unicastKeySM.h"
#include "broadcastKeySM.h"
#include "DataCtrl_Api.h"

#define  TKIP_KEY_LEN 32
#define  AES_KEY_LEN  16


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

TI_STATUS keyParserExternal_config(struct _keyParser_t *pKeyParser)
{
	pKeyParser->recv = keyParserExternal_recv;
	pKeyParser->replayReset = keyParser_nop;
	pKeyParser->remove = keyParserExternal_remove;
	return TI_OK;
}


/**
*
* keyParserExternal_recv
*
* \b Description: 
*
* External key Parser receive function:
*							- Called by NDIS (Windows)  upon receiving an External Key.
*							- Filters the following keys:								
*								- Keys with invalid key index
*								- Keys with invalid MAC address
*
* \b ARGS:
*
*  I   - pKeyParser - Pointer to the keyParser context  \n
*  I   - pKeyData - A pointer to the Key Data. \n
*  I   - keyDataLen - The Key Data length. \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
*/

TI_STATUS keyParserExternal_recv(struct _keyParser_t *pKeyParser,
						  TI_UINT8 *pKeyData, TI_UINT32 keyDataLen)
{
	TI_STATUS						status;
	OS_802_11_KEY 	                *pKeyDesc;
	encodedKeyMaterial_t    		encodedKeyMaterial;
    paramInfo_t  					macParam;
	TI_BOOL                         macEqual2Associated=TI_FALSE;
	TI_BOOL							macIsBroadcast=TI_FALSE;
    TI_BOOL                         wepKey = TI_FALSE;
	TI_UINT8						broadcastMacAddr[MAC_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	TI_UINT8						nullMacAddr[MAC_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    TI_UINT8                        keyBuffer[MAC_ADDR_LEN+KEY_RSC_LEN+MAX_EXT_KEY_DATA_LENGTH];
    

    if (pKeyData == NULL)                             
	{                                                 
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: NULL KEY Data\n");
		return TI_NOK;
	}
	
	pKeyDesc = (OS_802_11_KEY*)pKeyData;

    /* copy the key data, mac address and RSC */
	MAC_COPY (keyBuffer, pKeyDesc->BSSID);
	/* configure keyRSC value (if needed) */
    if (pKeyDesc->KeyIndex & EXT_KEY_RSC_KEY_MASK)
	{	/* set key recieve sequence counter */
        os_memoryCopy(pKeyParser->hOs, &keyBuffer[MAC_ADDR_LEN], (TI_UINT8*)&(pKeyDesc->KeyRSC), KEY_RSC_LEN);
	}
    else
    {
        os_memoryZero(pKeyParser->hOs, &keyBuffer[MAC_ADDR_LEN], KEY_RSC_LEN);
    }

    /* check type and validity of keys */
    /* check MAC Address validity */
	macParam.paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
	status = ctrlData_getParam(pKeyParser->hCtrlData, &macParam);

	if (status != TI_OK)
	{
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Cannot get MAC address !!!\n");
        return TI_NOK;
	}

	/* check key length */
	if((pKeyDesc->KeyLength != WEP_KEY_LEN_40) && 
		(pKeyDesc->KeyLength != WEP_KEY_LEN_104) && 
		(pKeyDesc->KeyLength != WEP_KEY_LEN_232) &&
		(pKeyDesc->KeyLength != TKIP_KEY_LEN) && 
		(pKeyDesc->KeyLength != AES_KEY_LEN) )
		
	{
TRACE1(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Incorrect key length - %d \n", pKeyDesc->KeyLength);
		return TI_NOK;
	}
	if (MAC_EQUAL(macParam.content.ctrlDataCurrentBSSID, pKeyDesc->BSSID))
	{	
        macEqual2Associated = TI_TRUE;
   	}
	if (MAC_EQUAL (pKeyDesc->BSSID, broadcastMacAddr))
	{	
        macIsBroadcast = TI_TRUE;
   	}
	if ((pKeyDesc->KeyLength == WEP_KEY_LEN_40) || 
		(pKeyDesc->KeyLength == WEP_KEY_LEN_104) || 
		(pKeyDesc->KeyLength == WEP_KEY_LEN_232))
	{	/* In Add WEP the MAC address is nulled, since it's irrelevant */
        macEqual2Associated = TI_TRUE;
        wepKey = TI_TRUE;
   	}

    if (pKeyDesc->KeyIndex & EXT_KEY_SUPP_AUTHENTICATOR_MASK)
    {  /* The key is being set by an Authenticator - not allowed in IBSS mode */
    	if (pKeyParser->pParent->pParent->pParent->pAdmCtrl->networkMode == RSN_IBSS)
        {	/* in IBSS only Broadcast MAC is allowed */
        TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Authenticator set key in IBSS mode !!!\n");
        	return TI_NOK;
        }

    }

    if (pKeyDesc->KeyIndex & EXT_KEY_REMAIN_BITS_MASK)
    {  /* the reamining bits in the key index are not 0 (when they should be) */
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Key index bits 8-27 should be 0 !!!\n");
		return TI_NOK;
    }
    
    encodedKeyMaterial.pData  = (char *) keyBuffer;
	/* Check key length according to the cipher suite - TKIP, etc...??? */
    if (wepKey)
    {
        if (!((pKeyDesc->KeyLength == WEP_KEY_LEN_40) || (pKeyDesc->KeyLength == WEP_KEY_LEN_104) 
              || (pKeyDesc->KeyLength == WEP_KEY_LEN_232)))
        {	/*Invalid key length*/
            TRACE1(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "WEP_KEY_PARSER: ERROR: Invalid Key length: %d !!!\n", pKeyDesc->KeyLength);
            return TI_NOK;
        }

        os_memoryCopy(pKeyParser->hOs, &keyBuffer[0], pKeyDesc->KeyMaterial, pKeyDesc->KeyLength);
        if (MAC_EQUAL (nullMacAddr, pKeyDesc->BSSID))
        {   
            macIsBroadcast = TI_TRUE;
        } 

        encodedKeyMaterial.keyLen = pKeyDesc->KeyLength;
    }
    else /* this is TKIP or CKIP */
    {   
        if ((pKeyDesc->KeyLength == AES_KEY_LEN) && (pKeyParser->pPaeConfig->unicastSuite == TWD_CIPHER_CKIP))
        {
            os_memoryCopy(pKeyParser->hOs, &keyBuffer[0], pKeyDesc->KeyMaterial, pKeyDesc->KeyLength);
            encodedKeyMaterial.keyLen = pKeyDesc->KeyLength;
        }
        else
        {
            os_memoryCopy(pKeyParser->hOs, 
                          &keyBuffer[MAC_ADDR_LEN+KEY_RSC_LEN],
                          pKeyDesc->KeyMaterial, 
                          pKeyDesc->KeyLength);

            encodedKeyMaterial.keyLen = MAC_ADDR_LEN+KEY_RSC_LEN+pKeyDesc->KeyLength;
        }
    }

    encodedKeyMaterial.keyId  = pKeyDesc->KeyIndex;

TRACE2(pKeyParser->hReport, REPORT_SEVERITY_INFORMATION, "EXT_KEY_PARSER: Key received keyId=%x, keyLen=%d \n",						    pKeyDesc->KeyIndex, pKeyDesc->KeyLength                             );

    if (pKeyDesc->KeyIndex & EXT_KEY_PAIRWISE_GROUP_MASK)
    {	/* Pairwise key */
        /* check that the lower 8 bits of the key index are 0 */
        if (!wepKey && (pKeyDesc->KeyIndex & 0xff))
        {
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_WARNING, "EXT_KEY_PARSER: ERROR: Pairwise key must have index 0 !!!\n");
            return TI_NOK;
        }

		if (macIsBroadcast)
		{
            TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Broadcast MAC address for unicast !!!\n");
			return TI_NOK;
		}
		if (pKeyDesc->KeyIndex & EXT_KEY_TRANSMIT_MASK)
		{	/* tx only pairwase key */
			/* set unicast keys */
        	if (pKeyParser->pUcastKey->recvSuccess!=NULL)
            {
        	status = pKeyParser->pUcastKey->recvSuccess(pKeyParser->pUcastKey, &encodedKeyMaterial);
            }
		} else {
			/* recieve only pairwase keys are not allowed */
            TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: recieve only pairwase keys are not allowed !!!\n");
            return TI_NOK;
		}

    }
    else
    {   /* set broadcast keys */
        if (!macIsBroadcast)
        {	/* not broadcast MAC */
        	if (pKeyParser->pParent->pParent->pParent->pAdmCtrl->networkMode == RSN_IBSS)
        	{	/* in IBSS only Broadcast MAC is allowed */
            TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: not broadcast MAC in IBSS mode !!!\n");
            	return TI_NOK;
        	}
        	else if (!macEqual2Associated)
        	{	/* ESS mode and MAC is different than the associated one */
        		/* save the key for later */
				status = TI_OK; /* pKeyParser->pBcastKey->saveKey(pKeyParser->pBcastKey, &encodedKey);*/
        	}
			else
			{	/* MAC is equal to the associated one - configure immediately */
                if (!wepKey)
				{
					MAC_COPY (keyBuffer, broadcastMacAddr);
				}
        		if (pKeyParser->pBcastKey->recvSuccess!=NULL)
                {
					status =  pKeyParser->pBcastKey->recvSuccess(pKeyParser->pBcastKey, &encodedKeyMaterial);
				}
			}
        }
		else
		{   /* MAC is broadcast - configure immediately */
			if (!wepKey)
			{
				MAC_COPY (keyBuffer, broadcastMacAddr);
			}
		 	
			/* set broadcast key */
			if (pKeyParser->pBcastKey->recvSuccess!=NULL)
			{
				status =  pKeyParser->pBcastKey->recvSuccess(pKeyParser->pBcastKey, &encodedKeyMaterial);
			}

			if (pKeyDesc->KeyIndex & EXT_KEY_TRANSMIT_MASK)
			{	/* Group key used to transmit */
				/* set as unicast key as well */
				if (pKeyParser->pUcastKey->recvSuccess!=NULL)
				{
					status = pKeyParser->pUcastKey->recvSuccess(pKeyParser->pUcastKey, &encodedKeyMaterial);
				}
			}
		}
    }
				  
	return status;
}




TI_STATUS keyParserExternal_remove(struct _keyParser_t *pKeyParser, TI_UINT8 *pKeyData, TI_UINT32 keyDataLen)
{
	TI_STATUS				status;
	OS_802_11_KEY	 		*pKeyDesc;
    paramInfo_t  			macParam;
	encodedKeyMaterial_t    encodedKeyMaterial;
	TI_UINT8				broadcastMacAddr[MAC_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    TI_UINT8                keyBuffer[MAC_ADDR_LEN+KEY_RSC_LEN+MAX_EXT_KEY_DATA_LENGTH];

	if (pKeyData == NULL)
	{
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: NULL KEY Data\n");
		return TI_NOK;
	}
	
	pKeyDesc = (OS_802_11_KEY*)pKeyData;

    if (pKeyDesc->KeyIndex & EXT_KEY_TRANSMIT_MASK)
	{	/* Bit 31 should always be zero */
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Remove TX bit in key index can't be 1\n");
		return TI_NOK;
	}
	if (pKeyDesc->KeyIndex & EXT_KEY_REMAIN_BITS_MASK)
	{	/* Bits 8-29 should always be zero */
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Remove none zero key index\n");
		return TI_NOK;
	}
	
	encodedKeyMaterial.keyId = pKeyDesc->KeyIndex;
	encodedKeyMaterial.keyLen = 0;
    encodedKeyMaterial.pData = (char *) keyBuffer;

	if (pKeyDesc->KeyIndex & EXT_KEY_PAIRWISE_GROUP_MASK)
	{	/* delete all pairwise keys or for the current BSSID */
		if (!MAC_EQUAL(pKeyDesc->BSSID, broadcastMacAddr))
		{
			MAC_COPY (keyBuffer, pKeyDesc->BSSID);
		} 
        else 
        {
			macParam.paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
			status = ctrlData_getParam(pKeyParser->hCtrlData, &macParam);
			if (status != TI_OK)
			{
TRACE0(pKeyParser->hReport, REPORT_SEVERITY_ERROR, "EXT_KEY_PARSER: ERROR: Cannot get MAC address !!!\n");
				return TI_NOK;
			}
			
			MAC_COPY (keyBuffer, macParam.content.ctrlDataCurrentBSSID);
		}

        status =  pKeyParser->pUcastKey->pKeyDerive->remove(pKeyParser->pUcastKey->pKeyDerive, &encodedKeyMaterial);
	}
	else
	{	/* delete all group keys or for the current BSSID */
		MAC_COPY (keyBuffer, broadcastMacAddr);
        status =  pKeyParser->pBcastKey->pKeyDerive->remove(pKeyParser->pUcastKey->pKeyDerive, &encodedKeyMaterial);
	}

	return status;
}
