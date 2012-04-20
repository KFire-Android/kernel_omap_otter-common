/*
 * StaCap.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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


/** \file   StaCap.c 
 *  \brief  STA capabilities module that responsible to publish STA capabilities to all others modules.
 *  
 *  \see    StaCap.c
 */

#define __FILE_ID__  FILE_ID_86
#include "osApi.h"
#include "report.h"
#include "StaCap.h"
#include "TWDriver.h"
#include "802_11Defs.h"
#include "qosMngr_API.h"
#include "Device1273.h"
#include "smeApi.h"

/** 
 * \fn     staCap_Create 
 * \brief  Create the staCap module.
 * 
 * Allocate and clear the staCap module object.
 * 
 * \param  hOs - Handle to Os Abstraction Layer
 * \return Handle of the allocated object 
 * \sa     staCap_Destroy
 */ 
TI_HANDLE StaCap_Create (TI_HANDLE hOs)
{
	TI_HANDLE hStaCap;

	/* allocate module object */
	hStaCap = os_memoryAlloc (hOs, sizeof(TStaCap));
	
	if (!hStaCap)
	{
		WLAN_OS_REPORT (("StaCap_Create():  Allocation failed!!\n"));
		return NULL;
	}
	
    os_memoryZero (hOs, hStaCap, (sizeof(TStaCap)));

	return (hStaCap);
}


/** 
 * \fn     StaCap_Destroy
 * \brief  Destroy the module. 
 * 
 * Free the module's queues and object.
 * 
 * \param  hStaCap - The module object
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     StaCap_Create
 */ 
TI_STATUS StaCap_Destroy (TI_HANDLE hStaCap)
{
    TStaCap *pStaCap = (TStaCap *)hStaCap;

    /* free module object */
	os_memoryFree (pStaCap->hOs, pStaCap, sizeof(TStaCap));
	
    return TI_OK;
}


/** 
 * \fn     StaCap_Init 
 * \brief  Init required handles 
 * 
 * Init required handles and module variables.
 * 
 * \note    
 * \param  pStadHandles  - The driver modules handles
 * \return TI_OK on success or TI_NOK on failure  
 * \sa     
 */ 
TI_STATUS StaCap_Init (TStadHandlesList *pStadHandles)
{
	TStaCap *pStaCap = (TStaCap *)pStadHandles->hStaCap;
    
    pStaCap->hOs       = pStadHandles->hOs;
    pStaCap->hReport   = pStadHandles->hReport;
    pStaCap->hTWD      = pStadHandles->hTWD;
    pStaCap->hQosMngr  = pStadHandles->hQosMngr;
    pStaCap->hSme      = pStadHandles->hSme;
    
	return TI_OK;
}


/** 
 * \fn     StaCap_IsHtEnable
 * \brief  verify if HT enable\disable at the STA according to 11n_Enable init flag and Chip type   
 * 
 * \note   
 * \param  hStaCap - The module object
 * \param  b11nEnable - pointer to enable\disable flag 
 * \return NONE 
 * \sa     
 */ 
void StaCap_IsHtEnable (TI_HANDLE hStaCap, TI_BOOL *b11nEnable)
{
    TStaCap             *pStaCap = (TStaCap *)hStaCap;
    TTwdHtCapabilities  *pTwdHtCapabilities;
    paramInfo_t         tParam;

    tParam.paramType = SME_DESIRED_BSS_TYPE_PARAM;
    sme_GetParam (pStaCap->hSme, &tParam);

    /* If Infra-BSS, get actual HT capabilities from TWD */
    if (tParam.content.smeDesiredBSSType == BSS_INFRASTRUCTURE) 
    {
        TWD_GetTwdHtCapabilities (pStaCap->hTWD, &pTwdHtCapabilities);
        *b11nEnable = pTwdHtCapabilities->b11nEnable;
    }
    /* If IBSS, HT shouldn't be used */
    else 
    {
        *b11nEnable = TI_FALSE;
    }
}


/** 
 * \fn     StaCap_GetHtCapabilitiesIe
 * \brief  Get the desired STA HT capabilities IE. get the physical HT capabilities from TWD 
 * and build HT capabilities IE. 
 * 
 * \note   
 * \param  hStaCap - The module object
 * \param  pRequest - pointer to request buffer\n 
 * \param  len - size of returned IE\n            
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     
 */ 
TI_STATUS StaCap_GetHtCapabilitiesIe (TI_HANDLE hStaCap, TI_UINT8 *pRequest, TI_UINT32 *pLen)
{
    TStaCap                 *pStaCap = (TStaCap *)hStaCap;
    TTwdHtCapabilities      *pTwdHtCapabilities;
    TI_UINT8			    *pDataBuf = pRequest;
    TStaCapHtCapabilities   tHtCapabilities;
    TI_BOOL                 bWmeEnable;

    /* verify that WME flag enable */
    qosMngr_GetWmeEnableFlag (pStaCap->hQosMngr, &bWmeEnable); 
    if (bWmeEnable == TI_FALSE)
    {
        TRACE0(pStaCap->hReport, REPORT_SEVERITY_INFORMATION, "StaCap_GetHtCapabilitiesIe: 802.11n disable due to WME init flag.\n");
   		*pLen = 0;
		return TI_OK;
    }

    TWD_GetTwdHtCapabilities (pStaCap->hTWD, &pTwdHtCapabilities);
    /* verify that 802.11n flag enable */
    if (pTwdHtCapabilities->b11nEnable == TI_FALSE)
    {
        TRACE0(pStaCap->hReport, REPORT_SEVERITY_INFORMATION, "StaCap_GetHtCapabilitiesIe: 802.11n disable due to 11n_Enable init flag.\n");
   		*pLen = 0;
		return TI_OK;
    }

    /* 
     * set TWD values to HT capabilities structure 
     *
     * Note: all numbers after "<<" represent the position of the values in the filed according  
     * to 11n SPEC.
     */
    tHtCapabilities.uHtCapabilitiesInfo = ((pTwdHtCapabilities->uChannelWidth   << 1) |
                                           (pTwdHtCapabilities->uRxSTBC         << 8) |
                                           (pTwdHtCapabilities->uMaxAMSDU       << 11)|
                                           (DSSS_CCK_MODE                       << 12));

    tHtCapabilities.uHtCapabilitiesInfo |= ((((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_LDPC_CODING)                       ? 1 : 0) << 0) |
                                            (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_GREENFIELD_FRAME_FORMAT)           ? 1 : 0) << 4) |
                                            (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_SHORT_GI_FOR_20MHZ_PACKETS)        ? 1 : 0) << 5) |
                                            (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_SHORT_GI_FOR_40MHZ_PACKETS)        ? 1 : 0) << 6) |
                                            (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_SUPPORT_FOR_STBC_IN_TRANSMISSION)  ? 1 : 0) << 7) |
                                            (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_DELAYED_BLOCK_ACK)                 ? 1 : 0) << 10) |
                                            (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_DSSS_CCK_IN_40_MHZ)                ? 1 : 0) << 12) |
                                            (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_LSIG_TXOP_PROTECTION)              ? 1 : 0) << 15));

    tHtCapabilities.uAMpduParam = ((pTwdHtCapabilities->uMaxAMPDU       << 0) |
                                   (pTwdHtCapabilities->uAMPDUSpacing   << 2));

    /* copy RX supported MCS rates */
    os_memoryCopy (pStaCap->hOs, tHtCapabilities.tSuppMcsSet.aRxMscBitmask, pTwdHtCapabilities->aRxMCS, RX_TX_MCS_BITMASK_SIZE);

    tHtCapabilities.tSuppMcsSet.uHighestSupportedDataRate = pTwdHtCapabilities->uRxMaxDataRate;

    /* check if supported MCS rates identical to TX and RX */
    if( 0 == os_memoryCompare(pStaCap->hOs, pTwdHtCapabilities->aRxMCS, pTwdHtCapabilities->aTxMCS, RX_TX_MCS_BITMASK_SIZE))
    {
        tHtCapabilities.tSuppMcsSet.uTxRxSetting = ((TX_MCS_SET_YES     <<  0) |    /* set supported TX MCS rate */
                                                    (TX_RX_NOT_EQUAL_NO <<  1));    /* set TX&RX MCS rate are equal */
    } 
    /* in case supported MCS rates TX different from the RX */
    else
    {
        TI_UINT32 i;

        /* check if there are TX MCS rates supported */
        for (i = 0; i <= (RX_TX_MCS_BITMASK_SIZE - 1); ++i) 
        {
            if (pTwdHtCapabilities->aTxMCS[i] != 0)
            {
                break;
            }
        }

        /* TX MCS supported */
        if(i <= (RX_TX_MCS_BITMASK_SIZE -1))
        {
            tHtCapabilities.tSuppMcsSet.uTxRxSetting = ((TX_MCS_SET_YES         <<  0) |    /* set supported TX MCS rates */    
                                                        (TX_RX_NOT_EQUAL_YES    <<  1));    /* set TX&RX MCS rates different */ 
        }
        /* TX MCS not supported */
        else
        {
            tHtCapabilities.tSuppMcsSet.uTxRxSetting = (TX_MCS_SET_NO          <<  0);      /* set no supported TX MCS rates */
        }
    }

    tHtCapabilities.uExteCapabilities = (((pTwdHtCapabilities->uHTCapabilitiesBitMask & CAP_BIT_MASK_PCO) ? 1 : 0) << 0);

    if (tHtCapabilities.uExteCapabilities != 0)
    {
        tHtCapabilities.uExteCapabilities |= (pTwdHtCapabilities->uPCOTransTime <<  1);  
    }

    tHtCapabilities.uExteCapabilities |= ((pTwdHtCapabilities->uMCSFeedback << 8) |
                                          (HTC_SUPPORT_NO                   << 10));

	tHtCapabilities.uTxBfCapabilities = 0x0;

    tHtCapabilities.uAselCapabilities = 0x0;


    /* build IE */
    *pDataBuf       = HT_CAPABILITIES_IE_ID;
    *(pDataBuf + 1) = DOT11_HT_CAPABILITIES_ELE_LEN;
    COPY_WLAN_WORD(pDataBuf + 2, &(tHtCapabilities.uHtCapabilitiesInfo));
    *(pDataBuf + 4) = tHtCapabilities.uAMpduParam;
    os_memoryCopy (pStaCap->hOs, pDataBuf + 5, tHtCapabilities.tSuppMcsSet.aRxMscBitmask, RX_TX_MCS_BITMASK_SIZE);
    COPY_WLAN_WORD(pDataBuf + 15, &(tHtCapabilities.tSuppMcsSet.uHighestSupportedDataRate));
    *(pDataBuf + 17) = tHtCapabilities.tSuppMcsSet.uTxRxSetting;
    /* clear the reserved bytes */
    os_memoryZero (pStaCap->hOs, (pDataBuf + 18), 3);
    COPY_WLAN_WORD(pDataBuf + 21, &(tHtCapabilities.uExteCapabilities));
    COPY_WLAN_LONG(pDataBuf + 23, &(tHtCapabilities.uTxBfCapabilities));
    *(pDataBuf + 27) = tHtCapabilities.uAselCapabilities;

    *pLen = DOT11_HT_CAPABILITIES_ELE_LEN + sizeof(dot11_eleHdr_t);

    return TI_OK;
}

