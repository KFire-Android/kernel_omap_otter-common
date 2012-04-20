/*
 * txCtrlServ.c
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


/****************************************************************************/
/*                                                                          */
/*  MODULE:     txCtrlServ.c                                                */
/*                                                                          */
/*  PURPOSE:    Tx services module, e.g. send-null packet.                  */
/*              A sub-module of TxCtrl module (uses it's object).           */
/*                                                                          */
/****************************************************************************/

#define __FILE_ID__  FILE_ID_58
#include "paramOut.h"
#include "osApi.h"
#include "TWDriver.h"
#include "report.h"
#include "txCtrl.h"
#include "Ethernet.h"
#include "qosMngr_API.h"



/***********************************************************************
 *                        txCtrlServ_buildNullFrame
 ***********************************************************************

DESCRIPTION:    Build Null frame Function.
                The function does the following:
                -   Builds Null Data Frame, considering current QoS mode.

INPUT:      hTxCtrl     - Tx Ctrl module handle (the txServ uses the txCtrl object!!).
            pFrame      - A pointer to a buffer where the frame should be stored
            pLength     - A pointer to a placeholder for the frame length

************************************************************************/
TI_STATUS txCtrlServ_buildNullFrame(TI_HANDLE hTxCtrl, TI_UINT8* pFrame, TI_UINT32* pLength)
{
    txCtrl_t            *pTxCtrl = (txCtrl_t *)hTxCtrl;
    EHeaderConvertMode  qosMode = pTxCtrl->headerConverMode;
    dot11_header_t      *pHeader; /* Note : there is no body for null frame */
    TI_STATUS           status;
    TI_UINT16           fc;

    pHeader = (dot11_header_t*)(pFrame);

    if (qosMode == HDR_CONVERT_QOS)
    {
        *pLength = WLAN_QOS_HDR_LEN;
		SET_WLAN_WORD(&pHeader->qosControl, 0);     /* We are using user priority 0 (BE) so no need for shift and endianess */
    }
    else
    {
        *pLength = WLAN_HDR_LEN;
    }


    /* Set the Frame Control with Null Data type, QoS or non-QoS */
    if (qosMode == HDR_CONVERT_QOS)
        fc = DOT11_FC_DATA_NULL_QOS | DOT11_FC_TO_DS;
    else
        fc = DOT11_FC_DATA_NULL_FUNCTION | DOT11_FC_TO_DS;
    COPY_WLAN_WORD(&pHeader->fc, &fc); /* copy with endianess handling. */

    /* copy destination mac address */
    status = ctrlData_getParamBssid(pTxCtrl->hCtrlData, CTRL_DATA_CURRENT_BSSID_PARAM, pHeader->address3);
    if (status != TI_OK)
    {
        return TI_NOK;
    }

    /* copy source mac address */
    status = ctrlData_getParamBssid(pTxCtrl->hCtrlData, CTRL_DATA_MAC_ADDRESS, pHeader->address2);
    if (status != TI_OK)
    {
        return TI_NOK;
    }

    /* copy BSSID (destination mac address) */
    MAC_COPY (pHeader->address1, pHeader->address3);

    return status;
}


/***********************************************************************
 *                        txCtrlServ_buildWlanHeader
 ***********************************************************************

DESCRIPTION:    Build WLAN header from Ethernet header.

INPUT:      hTxCtrl     - Tx Ctrl module handle (the txServ uses the txCtrl object!!).
            pFrame      - A pointer to a buffer where the frame should be stored
            pLength     - A pointer to a placeholder for the frame length

************************************************************************/
TI_STATUS txCtrlServ_buildWlanHeader(TI_HANDLE hTxCtrl, TI_UINT8* pFrame, TI_UINT32* pLength)
{
    txCtrl_t         *pTxCtrl = (txCtrl_t *)hTxCtrl;
    TI_STATUS        status;
    TMacAddr         daBssid;
    TMacAddr         saBssid;
    EQosProtocol     qosProt;
    ScanBssType_e    currBssType;
    TMacAddr         currBssId;
    TI_UINT32        headerLength;
    TI_UINT16        headerFlags;
    TI_BOOL          currentPrivacyInvokedMode;
    TI_UINT8         encryptionFieldSize;
    TTxCtrlBlk       tPktCtrlBlk;
    dot11_header_t   *pDot11Header = (dot11_header_t*)(tPktCtrlBlk.aPktHdr);
    Wlan_LlcHeader_T *pWlanSnapHeader;

    /* 
     * If QoS is used, add two bytes padding before the header for 4-bytes alignment.
     * Note that the header length doesn't include it, so the txCtrl detects the pad existence
     *   by checking if the header-length is a multiple of 4. 
     */
    qosMngr_getParamsActiveProtocol(pTxCtrl->hQosMngr, &qosProt);

    if (qosProt == QOS_WME)  
    {
        headerLength = WLAN_QOS_HDR_LEN;
        headerFlags  = DOT11_FC_DATA_QOS | DOT11_FC_TO_DS;
        pDot11Header->qosControl = 0;
    }
    else
    {
        headerLength = WLAN_HDR_LEN;
        headerFlags  = DOT11_FC_DATA | DOT11_FC_TO_DS;
    }

    /* 
     * Handle encryption if needed (decision was done at RSN and is provided by TxCtrl):
     *   - Set WEP bit in header.
     *   - Add padding for FW security overhead: 4 bytes for TKIP, 8 for AES.  
     */
    txCtrlParams_getCurrentEncryptionInfo (hTxCtrl, 
                                           &currentPrivacyInvokedMode,
                                           &encryptionFieldSize);
    if (currentPrivacyInvokedMode)
    {
        headerFlags |= DOT11_FC_WEP;
        headerLength += encryptionFieldSize;
    }
    
    COPY_WLAN_WORD (&pDot11Header->fc, &headerFlags); /* copy with endianess handling. */

    /* Get the Destination MAC address */
    status = ctrlData_getParamBssid (pTxCtrl->hCtrlData, CTRL_DATA_CURRENT_BSSID_PARAM, daBssid);
    if (status != TI_OK)
    {
        return TI_NOK;
    }

    /* Get the Source MAC address */
    status = ctrlData_getParamBssid (pTxCtrl->hCtrlData, CTRL_DATA_MAC_ADDRESS, saBssid);
    if (status != TI_OK)
    {
        return TI_NOK;
    }

    /* receive BssId and Bss Type from control module */
    ctrlData_getCurrBssTypeAndCurrBssId (pTxCtrl->hCtrlData, &currBssId, &currBssType);
    if (currBssType != BSS_INFRASTRUCTURE)
    {
        return TI_NOK;
    }

    /* copy BSSID */
    MAC_COPY (pDot11Header->address1, currBssId);
    /* copy source mac address */
    MAC_COPY (pDot11Header->address2, saBssid);
    /* copy destination mac address*/
    MAC_COPY (pDot11Header->address3, daBssid);


    /* Set the SNAP header pointer right after the other header parts handled above. */
    pWlanSnapHeader = (Wlan_LlcHeader_T *)&(tPktCtrlBlk.aPktHdr[headerLength]);
    
   	pWlanSnapHeader->DSAP = SNAP_CHANNEL_ID;
   	pWlanSnapHeader->SSAP = SNAP_CHANNEL_ID;
   	pWlanSnapHeader->Control = LLC_CONTROL_UNNUMBERED_INFORMATION;

    /* add RFC1042. */
	pWlanSnapHeader->OUI[0] = SNAP_OUI_RFC1042_BYTE0;
	pWlanSnapHeader->OUI[1] = SNAP_OUI_RFC1042_BYTE1;
	pWlanSnapHeader->OUI[2] = SNAP_OUI_RFC1042_BYTE2;

    /* set ETH type to IP */
    pWlanSnapHeader->Type = HTOWLANS(ETHERTYPE_IP);

    /* Add the SNAP length to the total header length. */
    headerLength += sizeof(Wlan_LlcHeader_T);

    /* copy WLAN header */
    os_memoryCopy (pTxCtrl->hOs, pFrame, tPktCtrlBlk.aPktHdr, headerLength);
    *pLength = headerLength;

    return TI_OK;
}

