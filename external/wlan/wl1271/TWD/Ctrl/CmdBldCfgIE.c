/*
 * CmdBldCfgIE.c
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


/** \file  CmdBldCfgIE.c 
 *  \brief Command builder. Configuration commands information elements
 *
 *  \see   CmdBld.h 
 */
#define __FILE_ID__  FILE_ID_92
#include "osApi.h"
#include "report.h"
#include "CmdBld.h"
#include "CmdQueue_api.h"
#include "rate.h"
#include "TwIf.h"

/****************************************************************************
 *                      cmdBld_CfgIeConfigMemory()
 ****************************************************************************
 * DESCRIPTION: Configure wlan hardware memory
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeConfigMemory (TI_HANDLE hCmdBld, TDmaParams *pDmaParams, void *fCb, TI_HANDLE hCb)
{   
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXConfigMemory_t AcxElm_ConfigMemory;
    ACXConfigMemory_t *pCfg = &AcxElm_ConfigMemory;
    
    os_memoryZero(pCmdBld->hOs, (void *)pCfg, sizeof(*pCfg));

    /*
     * Set information element header
     */
    pCfg->EleHdr.id = ACX_MEM_CFG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /*
     * Set configuration fields
     */
    pCfg->numStations             = pDmaParams->NumStations;
    pCfg->rxMemblockNumber        = pDmaParams->NumRxBlocks;
    pCfg->txMinimumMemblockNumber = TWD_TX_MIN_MEM_BLKS_NUM;
    pCfg->numSsidProfiles         = 1;
    pCfg->totalTxDescriptors      = ENDIAN_HANDLE_LONG(NUM_TX_DESCRIPTORS);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/* WoneIndex value when running as station */
#define STATION_WONE_INDEX                  0


/****************************************************************************
 *                      cmdBld_CfgIeSlotTime()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the Slot Time
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeSlotTime (TI_HANDLE hCmdBld, TI_UINT8 apSlotTime, void *fCb, TI_HANDLE hCb)                                
{
    TCmdBld    *pCmdBld = (TCmdBld *)hCmdBld;
    ACXSlot_t   AcxElm_SlotTime;
    ACXSlot_t   *pCfg = &AcxElm_SlotTime;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_SLOT;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /* Set configuration fields */
    /* woneIndex is not relevant to station implementation */
    pCfg->woneIndex = STATION_WONE_INDEX;
    pCfg->slotTime = apSlotTime;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": Sending info elem to firmware, Slot Time = %d\n", (TI_UINT8)pCfg->slotTime);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIePreamble()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the Preamble
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIePreamble (TI_HANDLE hCmdBld, TI_UINT8 preamble, void *fCb, TI_HANDLE hCb)
{
    TCmdBld        *pCmdBld = (TCmdBld *)hCmdBld;
    ACXPreamble_t   AcxElm_Preamble;
    ACXPreamble_t   *pCfg = &AcxElm_Preamble;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_PREAMBLE_TYPE;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /* Set configuration fields */
    /* woneIndex is not relevant to station implementation */
    pCfg->preamble = preamble;

    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "ID=%u: preamble=%u\n", pCfg->EleHdr.id, preamble);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeRx()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate RxConfig information element
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeRx (TI_HANDLE hCmdBld, TI_UINT32 apRxConfigOption, TI_UINT32 apRxFilterOption, void *fCb, TI_HANDLE hCb)
{
    TCmdBld       *pCmdBld = (TCmdBld *)hCmdBld;
    ACXRxConfig_t   AcxElm_RxConfig;
    ACXRxConfig_t  *pCfg = &AcxElm_RxConfig;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_RX_CFG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /* Set configuration fields */
    pCfg->ConfigOptions = ENDIAN_HANDLE_LONG(apRxConfigOption);
    pCfg->FilterOptions = ENDIAN_HANDLE_LONG(apRxFilterOption);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}

/****************************************************************************
*                 cmdBld_CfgIeEnableRxDataFilter()
*****************************************************************************
* DESCRIPTION: Enables or disables Rx data filtering.
*
* INPUTS:  enabled             - 0 to disable data filtering, any other value to enable 
*          defaultAction       - The default action to take on non-matching packets.
*
* OUTPUT:  None
*
* RETURNS: TI_OK or TI_NOK
****************************************************************************/
TI_STATUS cmdBld_CfgIeEnableRxDataFilter (TI_HANDLE hCmdBld, TI_BOOL enabled, filter_e defaultAction, void *fCb, TI_HANDLE hCb)
{
    TCmdBld       *pCmdBld = (TCmdBld *)hCmdBld;
    DataFilterDefault_t dataFilterDefault;
    DataFilterDefault_t * pCfg = &dataFilterDefault;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_ENABLE_RX_DATA_FILTER;
    pCfg->EleHdr.len = 0;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": Rx Data Filter configuration:\n");
    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": enabled = %d, defaultAction = %d\n", enabled, defaultAction);

    /* Set information element configuration fields */
    pCfg->enable = enabled;
    pCfg->action = defaultAction;
    pCfg->EleHdr.len += sizeof(pCfg->enable) + sizeof(pCfg->action);

    TRACE_INFO_HEX(pCmdBld->hReport, (TI_UINT8 *) pCfg, sizeof(dataFilterDefault));

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}

/****************************************************************************
*                      cmdBld_CfgIeRxDataFilter()
*****************************************************************************
* DESCRIPTION: Add/remove Rx Data filter information element.
*
* INPUTS:  index               - Index of the Rx Data filter
*          command             - Add or remove the filter
*          action              - Action to take on packets matching the pattern
*          numFieldPatterns    - Number of field patterns in the filter
*          lenFieldPatterns    - Length of the field pattern series
*          fieldPatterns       - Series of field patterns
*
* OUTPUT:  None
*
* RETURNS: TI_OK or TI_NOK
****************************************************************************/
TI_STATUS cmdBld_CfgIeRxDataFilter (TI_HANDLE hCmdBld, 
                                    TI_UINT8 index, 
                                    TI_UINT8 command, 
                                    filter_e action, 
                                    TI_UINT8 numFieldPatterns, 
                                    TI_UINT8 lenFieldPatterns, 
                                    TI_UINT8 *pFieldPatterns, 
                                    void *fCb, 
                                    TI_HANDLE hCb)
{
    TCmdBld       *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8 dataFilterConfig[sizeof(DataFilterConfig_t) + MAX_DATA_FILTER_SIZE];
    DataFilterConfig_t * pCfg = (DataFilterConfig_t *) &dataFilterConfig;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_SET_RX_DATA_FILTER;
    pCfg->EleHdr.len = 0;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": Rx Data Filter configuration:\n");
    TRACE5(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": command = %d, index = %d, action = %d, numFieldPatterns = %d, lenFieldPatterns = %d\n", command, index, action, numFieldPatterns, lenFieldPatterns);

    /* Set information element configuration fields */
    pCfg->command = command;
    pCfg->index = index;
    pCfg->EleHdr.len += sizeof(pCfg->command) + sizeof(pCfg->index);

    /* When removing a filter only the index and command are to be sent */
    if (command == ADD_FILTER)
    {
        pCfg->action = action;
        pCfg->numOfFields = numFieldPatterns;
        pCfg->EleHdr.len += sizeof(pCfg->action) + sizeof(pCfg->numOfFields);

        if (pFieldPatterns == NULL)
        {
            TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, ": Null pattern table argument received!\n");

            return PARAM_VALUE_NOT_VALID;
        }

        os_memoryCopy(pCmdBld->hOs, &pCfg->FPTable, pFieldPatterns, lenFieldPatterns);
        pCfg->EleHdr.len += lenFieldPatterns;
    }

    TRACE_INFO_HEX(pCmdBld->hReport, (TI_UINT8 *) pCfg, sizeof(dataFilterConfig));

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(dataFilterConfig), fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CfgIeArpIpFilter()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate ARP addr table information element for
 *              ipV4 only
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeArpIpFilter (TI_HANDLE hCmdBld, 
                                   TIpAddr   tIpAddr,
                                   EArpFilterType  filterType, 
                                   void      *fCb, 
                                   TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXConfigureIP_t AcxElm_CmdConfigureIP;
    ACXConfigureIP_t *pCfg = &AcxElm_CmdConfigureIP;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_ARP_IP_FILTER;
    pCfg->EleHdr.len = sizeof(ACXConfigureIP_t) - sizeof(EleHdrStruct);

    pCfg->arpFilterEnable = (TI_UINT8)filterType;
        
    /* IP address */
    /* Note that in the case of IPv4 it is assumed that the extra two bytes are zero */
    IP_COPY (pCfg->address, tIpAddr);

    TRACE3(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "ID=%u: ip=%x, enable=%u\n", pCfg->EleHdr.id, *((TI_UINT32*)pCfg->address), filterType);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(ACXConfigureIP_t), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeGroupAdressTable()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate Group addr table information element
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeGroupAdressTable (TI_HANDLE       hCmdBld, 
                                        TI_UINT8        numGroupAddrs, 
                                        TMacAddr        *pGroupAddr, 
                                        TI_BOOL         bEnabled, 
                                        void            *fCb, 
                                        TI_HANDLE       hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32   i = 0;
    TI_UINT8   *tmpLoc = NULL;
    dot11MulticastGroupAddrStart_t  AcxElm_CmdConfigureMulticastIp;
    dot11MulticastGroupAddrStart_t* pCfg = &AcxElm_CmdConfigureMulticastIp;
      
    os_memoryZero (pCmdBld->hOs, (void *)pCfg, sizeof(dot11MulticastGroupAddrStart_t));

    /* Set information element header */
    pCfg->EleHdr.id = DOT11_GROUP_ADDRESS_TBL;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);
     
    pCfg->numOfGroups = numGroupAddrs;
    pCfg->fltrState = bEnabled;
    tmpLoc = pCfg->dataLocation;
        
    if (NULL != pGroupAddr)
    {
        for (i = 0; i < numGroupAddrs; i++) 
        {
            MAC_COPY (&tmpLoc[MAC_ADDR_LEN * i], *(pGroupAddr + i));

            TRACE7(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgIeGroupAdressTable: MAC %x: %x:%x:%x:%x:%x:%x\n", i, tmpLoc[MAC_ADDR_LEN*i+0] , tmpLoc[MAC_ADDR_LEN*i+1] , tmpLoc[MAC_ADDR_LEN*i+2] , tmpLoc[MAC_ADDR_LEN*i+3] , tmpLoc[MAC_ADDR_LEN*i+4] , tmpLoc[MAC_ADDR_LEN*i+5]);
        }
    }

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(dot11MulticastGroupAddrStart_t), fCb, hCb, NULL);
    
}


/****************************************************************************
 *                      cmdBld_CfgIeSgEnable()
 ****************************************************************************
 * DESCRIPTION: Enable/Disable the BTH-WLAN  
 *
 * INPUTS:  Enable flag
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeSgEnable (TI_HANDLE hCmdBld, ESoftGeminiEnableModes SoftGeminiEnableModes, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXBluetoothWlanCoEnableStruct  AcxElm_BluetoothWlanEnable;
    ACXBluetoothWlanCoEnableStruct* pCfg = &AcxElm_BluetoothWlanEnable;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgIeSgEnable: Enable flag = %d\n", SoftGeminiEnableModes);

    /* Set information element header */
    pCfg->EleHdr.id = ACX_SG_ENABLE;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /* Set enable field */
    pCfg->coexOperationMode = (TI_UINT8)SoftGeminiEnableModes;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeSg()
 ****************************************************************************
 * DESCRIPTION: Configure the BTH-WLAN co-exsistance   
 *
 * INPUTS:  Configuration structure pointer 
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeSg (TI_HANDLE hCmdBld, TSoftGeminiParams *pSoftGeminiParam, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXBluetoothWlanCoParamsStruct  AcxElm_BluetoothWlanEnable;
    ACXBluetoothWlanCoParamsStruct *pCfg = &AcxElm_BluetoothWlanEnable;
	int i=0;
    
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgIeSg. \n");

    /* Set information element header */
    pCfg->EleHdr.id      		= ACX_SG_CFG;
    pCfg->EleHdr.len     		= sizeof(*pCfg) - sizeof(EleHdrStruct);

	pCfg->softGeminiParams.paramIdx = pSoftGeminiParam->paramIdx;


	for (i=0; i< SOFT_GEMINI_PARAMS_MAX ; i++)
	{
		pCfg->softGeminiParams.coexParams[i] = pSoftGeminiParam->coexParams[i];
	}

    /* Rate conversion is done in the HAL */
    pCfg->softGeminiParams.coexParams[SOFT_GEMINI_RATE_ADAPT_THRESH] = rateNumberToBitmap((TI_UINT8)pSoftGeminiParam->coexParams[SOFT_GEMINI_RATE_ADAPT_THRESH]);

	if (pCfg->softGeminiParams.coexParams[SOFT_GEMINI_RATE_ADAPT_THRESH] == 0)
    {
        TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "coexAPRateAdapationThr is 0, convert to 1MBPS. \n");
        pCfg->softGeminiParams.coexParams[SOFT_GEMINI_RATE_ADAPT_THRESH] = HW_BIT_RATE_1MBPS;
    }

    /* Send the configuration command */
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeFmCoex()
 ****************************************************************************
 * DESCRIPTION: Configure the FM-WLAN co-exsistance parameters
 *
 * INPUTS:  Configuration structure pointer
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeFmCoex (TI_HANDLE hCmdBld, TFmCoexParams *pFmCoexParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXWlanFmCoexStruct  tFmWlanCoex;
    ACXWlanFmCoexStruct *pCfg = &tFmWlanCoex;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgIeFmCoex\n");

    /* Set information element header */
    pCfg->EleHdr.id  = ACX_FM_COEX_CFG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /* Set parameters with endianess handling */
    pCfg->enable                   = pFmCoexParams->uEnable;
    pCfg->swallowPeriod            = pFmCoexParams->uSwallowPeriod;
    pCfg->nDividerFrefSet1         = pFmCoexParams->uNDividerFrefSet1;
    pCfg->nDividerFrefSet2         = pFmCoexParams->uNDividerFrefSet2;
    pCfg->mDividerFrefSet1         = ENDIAN_HANDLE_WORD(pFmCoexParams->uMDividerFrefSet1);
    pCfg->mDividerFrefSet2         = ENDIAN_HANDLE_WORD(pFmCoexParams->uMDividerFrefSet2);
    pCfg->coexPllStabilizationTime = ENDIAN_HANDLE_LONG(pFmCoexParams->uCoexPllStabilizationTime);
    pCfg->ldoStabilizationTime     = ENDIAN_HANDLE_WORD(pFmCoexParams->uLdoStabilizationTime);
    pCfg->fmDisturbedBandMargin    = pFmCoexParams->uFmDisturbedBandMargin;
    pCfg->swallowClkDif            = pFmCoexParams->uSwallowClkDif;

    /* Send the configuration command */
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeMemoryMap ()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate MemoryMap information element
 *
 * INPUTS:
 *      AcxElm_MemoryMap_T *apMap   pointer to the memory map structure
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeMemoryMap (TI_HANDLE hCmdBld, MemoryMap_t *apMap, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    MemoryMap_t SwapMap;
    TI_UINT32 *pSwap, *pOrig, i, uMemMapNumFields;

    /* Set information element header */
    SwapMap.EleHdr.id  = ACX_MEM_MAP;
    SwapMap.EleHdr.len = sizeof(MemoryMap_t) - sizeof(EleHdrStruct);

    /* Solve endian problem (all fields are 32 bit) */
    pOrig = (TI_UINT32* )&apMap->codeStart;
    pSwap = (TI_UINT32* )&SwapMap.codeStart;
    uMemMapNumFields = (sizeof(MemoryMap_t) - sizeof(EleHdrStruct)) % 4;
    for (i = 0; i < uMemMapNumFields; i++)
        pSwap[i] = ENDIAN_HANDLE_LONG(pOrig[i]);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, &SwapMap, sizeof(SwapMap), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeAid()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the AID info element
 *
 * INPUTS:
 *      TI_UINT16* apAidVal     The AID value
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeAid (TI_HANDLE hCmdBld, TI_UINT16 apAidVal, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXAid_t    WlanElm_AID;
    ACXAid_t    *pCfg = &WlanElm_AID;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_AID;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->Aid = ENDIAN_HANDLE_WORD(apAidVal);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeWakeUpCondition()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the power management option
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeWakeUpCondition (TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    WakeUpCondition_t WakeUpCondition;
    WakeUpCondition_t *pCfg = &WakeUpCondition;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "WakeUpCondition :\n                             listenInterval = 0x%X\n", pPMConfig->listenInterval);

    switch (pPMConfig->tnetWakeupOn)
    {
        case TNET_WAKE_ON_BEACON:
            pCfg->wakeUpConditionBitmap = WAKE_UP_EVENT_BEACON_BITMAP;
            break;
        case TNET_WAKE_ON_DTIM:
            pCfg->wakeUpConditionBitmap = WAKE_UP_EVENT_DTIM_BITMAP;
            break;
        case TNET_WAKE_ON_N_BEACON:
            pCfg->wakeUpConditionBitmap = WAKE_UP_EVENT_N_BEACONS_BITMAP;
            break;
        case TNET_WAKE_ON_N_DTIM:
            pCfg->wakeUpConditionBitmap = WAKE_UP_EVENT_N_DTIM_BITMAP;
            break;
        default:
            pCfg->wakeUpConditionBitmap = WAKE_UP_EVENT_BEACON_BITMAP;
            break;
    }

    pCfg->listenInterval = pPMConfig->listenInterval;

    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, " cmdBld_wakeUpCondition  tnetWakeupOn=0x%x listenInterval=%d\n",pCfg->wakeUpConditionBitmap,pCfg->listenInterval);

    /* Set information element header */
    pCfg->EleHdr.id = ACX_WAKE_UP_CONDITIONS;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeSleepAuth()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the power management option
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeSleepAuth (TI_HANDLE hCmdBld, EPowerPolicy eMinPowerLevel, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXSleepAuth_t ACXSleepAuth;
    ACXSleepAuth_t *pCfg = &ACXSleepAuth;
    EElpCtrlMode eElpCtrlMode;

    /* Set the ELP control according to the new power policy */
    switch (eMinPowerLevel) 
    {
    case POWERAUTHO_POLICY_AWAKE:   eElpCtrlMode = ELPCTRL_MODE_KEEP_AWAKE;  break;
    case POWERAUTHO_POLICY_PD:      eElpCtrlMode = ELPCTRL_MODE_KEEP_AWAKE;  break;
    case POWERAUTHO_POLICY_ELP:     eElpCtrlMode = ELPCTRL_MODE_NORMAL;      break;
    default:
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR, " - Param value is not supported, %d\n", eMinPowerLevel);
        return TI_NOK;

    }

    /* Set the ELP mode only if there is a change */
    if (pCmdBld->uLastElpCtrlMode != eElpCtrlMode)
    {
        pCmdBld->uLastElpCtrlMode = eElpCtrlMode;
		if (eElpCtrlMode == ELPCTRL_MODE_KEEP_AWAKE)
		{
			twIf_Awake(pCmdBld->hTwIf);
		}
		else
		{
			twIf_Sleep(pCmdBld->hTwIf);
		}
    }

    /* In the info element the enums are in reverse */
    switch (eMinPowerLevel)
    {
        case POWERAUTHO_POLICY_ELP:
            pCfg->sleepAuth = 2;
            break;
        case POWERAUTHO_POLICY_AWAKE:
            pCfg->sleepAuth = 0;
            break;
        default:
            pCfg->sleepAuth = eMinPowerLevel;
    }

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, " cmdBld_MinPowerLevelSet  sleepAuth=%d\n", eMinPowerLevel);

    /* Set information element header*/
    pCfg->EleHdr.id = ACX_SLEEP_AUTH;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeBcnBrcOptions()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the power management option
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeBcnBrcOptions (TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXBeaconAndBroadcastOptions_t ACXBeaconAndBroadcastOptions;
    ACXBeaconAndBroadcastOptions_t *pCfg = &ACXBeaconAndBroadcastOptions;

    pCfg->beaconRxTimeOut = pPMConfig->BcnBrcOptions.BeaconRxTimeout;
    pCfg->broadcastTimeOut = pPMConfig->BcnBrcOptions.BroadcastRxTimeout;
    pCfg->rxBroadcastInPS = pPMConfig->BcnBrcOptions.RxBroadcastInPs;
	pCfg->consecutivePsPollDeliveryFailureThr = pPMConfig->ConsecutivePsPollDeliveryFailureThreshold;


    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, " cmdBld_BcnBrcOptions  BeaconRxTimeout=%d BroadcastRxTimeout=%d RxBroadcastInPs=0x%x, consecutivePsPollDeliveryFailureThr=%d\n",							 pCfg->beaconRxTimeOut,pCfg->broadcastTimeOut,							 pCfg->rxBroadcastInPS, pCfg->consecutivePsPollDeliveryFailureThr);

    /* Set information element header */
    pCfg->EleHdr.id = ACX_BCN_DTIM_OPTIONS;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeFeatureConfig()
                                    ACXBeaconAndBroadcastOptions_t* pWlanElm_BcnBrcOptions,
 ****************************************************************************
 * DESCRIPTION: Configure the feature config info element
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeFeatureConfig (TI_HANDLE hCmdBld, TI_UINT32 options, TI_UINT32 uDataFlowOptions, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXFeatureConfig_t  WlanElm_FeatureConfig;
    ACXFeatureConfig_t  *pCfg = &WlanElm_FeatureConfig;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_FEATURE_CFG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /* Set fields */
    pCfg->Options = ENDIAN_HANDLE_LONG(options);
    pCfg->dataflowOptions = ENDIAN_HANDLE_LONG(uDataFlowOptions);

    TRACE3(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "ID=%u: option=0x%x, def.option=0x%x\n", pCfg->EleHdr.id, options, uDataFlowOptions);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeTxPowerDbm ()
 ****************************************************************************
 * DESCRIPTION: Set the Tx power in Dbm units.
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeTxPowerDbm (TI_HANDLE hCmdBld, TI_UINT8 uTxPowerDbm , void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    dot11CurrentTxPowerStruct dot11CurrentTxPower;
    dot11CurrentTxPowerStruct *pCfg = &dot11CurrentTxPower;

   TRACE1( pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, " uTxPowerDbm = %d\n", uTxPowerDbm);


    /* Set information element header*/
    pCfg->EleHdr.id = DOT11_CUR_TX_PWR;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->dot11CurrentTxPower = uTxPowerDbm;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeStatisitics ()
 ****************************************************************************
 * DESCRIPTION: Set the ACX statistics counters to zero.
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeStatisitics (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_STATUS status = TI_NOK;
    ACXStatistics_t  *pCfg;

    pCfg = os_memoryAlloc(pCmdBld->hOs, sizeof(ACXStatistics_t));
    if (!pCfg)
    {
        return status;
    }

    /* Set information element header */
    pCfg->EleHdr.id  = ACX_STATISTICS;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    status = cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
    os_memoryFree(pCmdBld->hOs, pCfg, sizeof(ACXStatistics_t));
    return status;
}


/****************************************************************************
 *                      cmdBld_CfgIeTid()
 ****************************************************************************
 * DESCRIPTION: Write the Queue configuration (For Quality Of Service)
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeTid (TI_HANDLE hCmdBld, TQueueTrafficParams* pQtrafficParams, void *fCb, TI_HANDLE hCb)
                                          
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXTIDConfig_t    TrafficCategoryCfg;
    ACXTIDConfig_t   *pCfg = &TrafficCategoryCfg;

    os_memoryZero (pCmdBld->hOs, (void *)pCfg, sizeof(*pCfg));

    /*
     * Set information element header
     * ==============================
     */
    pCfg->EleHdr.id = ACX_TID_CFG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /*
     * Set information element Data
     * ==============================
     */
    pCfg->queueID       = pQtrafficParams->queueID;
    pCfg->channelType   = pQtrafficParams->channelType;
    pCfg->tsid          = pQtrafficParams->tsid;
    pCfg->psScheme      = pQtrafficParams->psScheme; 
    pCfg->APSDConf[0]   = pQtrafficParams->APSDConf[0];
    pCfg->APSDConf[1]   = pQtrafficParams->APSDConf[1];

    TRACE7(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "ID=%u: queue-id=%u, chan-type=%u, tsid=%u, ps-scheme=%u, apsd-1=0x%x, apsd-2=0x%x\n", pCfg->EleHdr.id, pCfg->queueID, pCfg->channelType, pCfg->tsid, pCfg->psScheme, pCfg->APSDConf[0], pCfg->APSDConf[1]);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeAcParams()
 ****************************************************************************
 * DESCRIPTION: Write the AC configuration (For Quality Of Service)
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeAcParams (TI_HANDLE hCmdBld, TAcQosParams *pAcQosParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXAcCfg_t     AcCfg;
    ACXAcCfg_t    *pCfg  = &AcCfg;

    os_memoryZero (pCmdBld->hOs, (void *)pCfg, sizeof(*pCfg));

    /*
     * Set information element header
     * ==============================
     */
    pCfg->EleHdr.id = ACX_AC_CFG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /*
     * Set information element Data
     * ==============================
     */

    pCfg->ac        = pAcQosParams->ac;
    pCfg->aifsn     = pAcQosParams->aifsn;
    pCfg->cwMax     = ENDIAN_HANDLE_WORD(pAcQosParams->cwMax);
    pCfg->cwMin     = pAcQosParams->cwMin;
    pCfg->txopLimit = ENDIAN_HANDLE_WORD(pAcQosParams->txopLimit);

    TRACE6(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "ID=%u: ac= %u, aifsn=%u, cw-max=%u, cw-min=%u, txop=%u\n", pCfg->EleHdr.id, pAcQosParams->ac, pAcQosParams->aifsn, pAcQosParams->cwMax, pAcQosParams->cwMin, pAcQosParams->txopLimit);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIePsRxStreaming()
 ****************************************************************************
 * DESCRIPTION: Write the AC PS-Rx-Streaming
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIePsRxStreaming (TI_HANDLE hCmdBld, TPsRxStreaming *pPsRxStreaming, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXPsRxStreaming_t  tStreamingCfg;
    ACXPsRxStreaming_t *pCfg  = &tStreamingCfg;

    os_memoryZero (pCmdBld->hOs, (void *)pCfg, sizeof(*pCfg));

    /*
     * Set information element header
     * ==============================
     */
    pCfg->EleHdr.id = ACX_PS_RX_STREAMING;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /*
     * Set information element Data
     * ============================
     */
    pCfg->TID          = (TI_UINT8)pPsRxStreaming->uTid;
    pCfg->rxPSDEnabled = (TI_UINT8)pPsRxStreaming->bEnabled;
    pCfg->streamPeriod = (TI_UINT8)pPsRxStreaming->uStreamPeriod;
    pCfg->txTimeout    = (TI_UINT8)pPsRxStreaming->uTxTimeout;

    TRACE5(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "ID=%u: tid= %u, enable=%u, streamPeriod=%u, txTimeout=%u\n", pCfg->EleHdr.id, pCfg->TID, pCfg->rxPSDEnabled, pCfg->streamPeriod, pCfg->txTimeout);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIePacketDetectionThreshold()
 ****************************************************************************
 * DESCRIPTION:  Set the PacketDetection threshold
 *
 * INPUTS:  
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIePacketDetectionThreshold (TI_HANDLE hCmdBld, TI_UINT32 pdThreshold, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXPacketDetection_t  PacketDetectionThresholdCfg;
    ACXPacketDetection_t *pCfg = &PacketDetectionThresholdCfg;

    /*
     * Set information element header
     * ==============================
     */
    pCfg->EleHdr.id = ACX_PD_THRESHOLD;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /*
     * Set information element Data
     * ==============================
     */
    pCfg->pdThreshold = ENDIAN_HANDLE_LONG(pdThreshold);

    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": pdThreshold = 0x%x , len = 0x%x \n",pCfg->pdThreshold,pCfg->EleHdr.len);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}




/****************************************************************************
 *                      cmdBld_CfgIeBeaconFilterOpt()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the beacon filtering option
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeBeaconFilterOpt (TI_HANDLE hCmdBld, TI_UINT8 beaconFilteringStatus, TI_UINT8 numOfBeaconsToBuffer, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXBeaconFilterOptions_t  ACXBeaconFilterOptions;
    ACXBeaconFilterOptions_t *pCfg = &ACXBeaconFilterOptions;
    
    pCfg->enable = beaconFilteringStatus;
    pCfg->maxNumOfBeaconsStored = numOfBeaconsToBuffer;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_BEACON_FILTER_OPT;
    pCfg->EleHdr.len = sizeof(ACXBeaconFilterOptions_t) - sizeof(EleHdrStruct);

    TRACE3(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "ID=%u: enable=%u, num-stored=%u\n", pCfg->EleHdr.id, beaconFilteringStatus, numOfBeaconsToBuffer);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(ACXBeaconFilterOptions_t), fCb, hCb, NULL);
}
/****************************************************************************
 *                      cmdBld_CfgIeRateMngDbg()
 ****************************************************************************
 * DESCRIPTION: Configure the rate managment params
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/

TI_STATUS cmdBld_CfgIeRateMngDbg (TI_HANDLE hCmdBld, RateMangeParams_t *pRateMngParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    AcxRateMangeParams  RateMng;
    AcxRateMangeParams *pCfg = &RateMng;
	int i;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_SET_RATE_MAMAGEMENT_PARAMS;
    pCfg->EleHdr.len = sizeof(AcxRateMangeParams) - sizeof(EleHdrStruct);


    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "ID=%u, index=%d \n",pCfg->EleHdr.id,pRateMngParams->paramIndex);

	pCfg->paramIndex = pRateMngParams->paramIndex;

	pCfg->InverseCuriosityFactor = pRateMngParams->InverseCuriosityFactor;
    pCfg->MaxPer = pRateMngParams->MaxPer;
	pCfg->PerAdd = pRateMngParams->PerAdd;
	pCfg->PerAddShift = pRateMngParams->PerAddShift;
	pCfg->PerAlphaShift = pRateMngParams->PerAlphaShift;
	pCfg->PerBeta1Shift = pRateMngParams->PerBeta1Shift;
	pCfg->PerBeta2Shift = pRateMngParams->PerBeta2Shift;
	pCfg->PerTh1 = pRateMngParams->PerTh1;
	pCfg->PerTh2 = pRateMngParams->PerTh2;
	pCfg->RateCheckDown = pRateMngParams->RateCheckDown;
	pCfg->RateCheckUp = pRateMngParams->RateCheckUp;
	pCfg->RateRetryScore = pRateMngParams->RateRetryScore;
	pCfg->TxFailHighTh = pRateMngParams->TxFailHighTh;
	pCfg->TxFailLowTh = pRateMngParams->TxFailLowTh;

	for (i=0 ; i< 13 ; i++)
	{
		pCfg->RateRetryPolicy[i] = pRateMngParams->RateRetryPolicy[i];
	}

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(AcxRateMangeParams), fCb, hCb, NULL);
}



/****************************************************************************
 *                     cmdBld_CfgIeBeaconFilterTable
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the beacon filter IE table
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeBeaconFilterTable (TI_HANDLE hCmdBld,
                                         TI_UINT8   uNumberOfIEs, 
                                         TI_UINT8  *pIETable,
                                         TI_UINT8   uIETableSize, 
                                         void      *fCb, 
                                         TI_HANDLE  hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXBeaconFilterIETable_t beaconFilterIETableStruct;
    ACXBeaconFilterIETable_t *pCfg = &beaconFilterIETableStruct;
    TI_UINT32 counter;
    
    if (NULL == pIETable) 
    {
        return PARAM_VALUE_NOT_VALID;
    }

    pCfg->EleHdr.id = ACX_BEACON_FILTER_TABLE;
    pCfg->EleHdr.len = uIETableSize + 1; 
    pCfg->NumberOfIEs = uNumberOfIEs;
        
    os_memoryZero (pCmdBld->hOs, (void *)pCfg->IETable, BEACON_FILTER_TABLE_MAX_SIZE);
    os_memoryCopy (pCmdBld->hOs, (void *)pCfg->IETable, (void *)pIETable, uIETableSize);
        
    TRACE3(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "ID=%u: num-ie=%u, table-size=%u\n", pCfg->EleHdr.id, uNumberOfIEs, uIETableSize);

TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "Beacon IE Table:\n");
    for (counter = 0; counter < uIETableSize; counter++)
    {
TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "%2x ", pIETable[counter]);
	}
TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "\n");
        
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(ACXBeaconFilterIETable_t), fCb, hCb, NULL);
}
 
/****************************************************************************
 *                     cmdBld_CfgCoexActivity
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the Coex activity IE 
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeCoexActivity (TI_HANDLE hCmdBld,
                                         TCoexActivity  *pCoexActivity,
                                         void      *fCb, 
                                         TI_HANDLE  hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXCoexActivityIE_t coexActivityIEStruct;
    ACXCoexActivityIE_t *pCfg = &coexActivityIEStruct;
    
    if (NULL == pCoexActivity) 
    {
        return PARAM_VALUE_NOT_VALID;
    }

    pCfg->EleHdr.id = ACX_COEX_ACTIVITY;
    pCfg->EleHdr.len = sizeof(ACXCoexActivityIE_t) - sizeof(EleHdrStruct);
        
    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "CoexActivity: ID=0x%x\n", pCfg->EleHdr.id);

    pCfg->coexIp          = pCoexActivity->coexIp;
    pCfg->activityId      = pCoexActivity->activityId;
    pCfg->defaultPriority = pCoexActivity->defaultPriority;
    pCfg->raisedPriority  = pCoexActivity->raisedPriority;
    pCfg->minService      = ENDIAN_HANDLE_WORD(pCoexActivity->minService);
    pCfg->maxService      = ENDIAN_HANDLE_WORD(pCoexActivity->maxService);

    TRACE6(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "CoexActivity: 0x%02x 0x%02x - 0x%02x 0x%02x 0x%04x 0x%04x\n", 
            pCfg->coexIp,
            pCfg->activityId,
            pCfg->defaultPriority,
            pCfg->raisedPriority,
            pCfg->minService,
            pCfg->maxService);
 
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CfgIeCcaThreshold()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the Slot Time
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeCcaThreshold (TI_HANDLE hCmdBld, TI_UINT16 ccaThreshold, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXEnergyDetection_t AcxElm_CcaThreshold;
    ACXEnergyDetection_t *pCfg = &AcxElm_CcaThreshold;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_CCA_THRESHOLD;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);
    
    pCfg->rxCCAThreshold = ENDIAN_HANDLE_WORD(ccaThreshold);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeEventMask()
 ****************************************************************************
 * DESCRIPTION: Change the Event Vector Mask in the FW
 * 
 * INPUTS: MaskVector   The Updated Vector Mask
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeEventMask (TI_HANDLE hCmdBld, TI_UINT32 mask, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    ACXEventMboxMask_t EventMboxData;
    ACXEventMboxMask_t *pCfg = &EventMboxData;

    /* Set information element header*/
    pCfg->EleHdr.id = ACX_EVENT_MBOX_MASK; 
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);
    pCfg->lowEventMask = ENDIAN_HANDLE_LONG(mask);
    pCfg->highEventMask = ENDIAN_HANDLE_LONG(0xffffffff); /* Not in Use */

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "cmdBld_CfgIeEventMask:\n");

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeMaxTxRetry()
 ****************************************************************************
 * DESCRIPTION: Configure the Max Tx Retry parameters
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeMaxTxRetry (TI_HANDLE hCmdBld,
                                  TRroamingTriggerParams *pRoamingTriggerCmd,
                                  void      *fCb, 
                                  TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXConsTxFailureTriggerParameters_t AcxElm_SetMaxTxRetry;
    ACXConsTxFailureTriggerParameters_t* pCfg = &AcxElm_SetMaxTxRetry;

    pCfg->maxTxRetry = pRoamingTriggerCmd->maxTxRetry;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_CONS_TX_FAILURE;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);
    
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeConnMonitParams()
 ****************************************************************************
 * DESCRIPTION: Configure the Bss Lost Timeout & TSF miss threshold
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeConnMonitParams (TI_HANDLE hCmdBld, TRroamingTriggerParams *pRoamingTriggerCmd, void *fCb, TI_HANDLE hCb)                                           
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    AcxConnectionMonitorOptions  AcxElm_SetBssLossTsfThreshold;
    AcxConnectionMonitorOptions* pCfg = &AcxElm_SetBssLossTsfThreshold;

    pCfg->BSSLossTimeout     = ENDIAN_HANDLE_LONG(pRoamingTriggerCmd->BssLossTimeout);
    pCfg->TSFMissedThreshold = ENDIAN_HANDLE_LONG(pRoamingTriggerCmd->TsfMissThreshold);

    /* Set information element header */
    pCfg->EleHdr.id  = ACX_CONN_MONIT_PARAMS;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);
    
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeTxRatePolicy()
 ****************************************************************************
 * DESCRIPTION: Write the TxRateClass configuration 
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeTxRatePolicy (TI_HANDLE hCmdBld, TTxRatePolicy *pTxRatePolicy, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXTxAttrClasses_t  TxClassCfg;
    ACXTxAttrClasses_t *pCfg  = &TxClassCfg;
    TI_UINT8 PolicyId;
    
    os_memoryZero (pCmdBld->hOs, (void *)pCfg, sizeof(*pCfg));

    /*
     * Set information element header
     * ==============================
     */
    pCfg->EleHdr.id = ACX_RATE_POLICY;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);
    pCfg->numOfClasses = pTxRatePolicy->numOfRateClasses;

    for (PolicyId = 0; PolicyId < pTxRatePolicy->numOfRateClasses; PolicyId++)
    {
        os_memoryCopy (pCmdBld->hOs,
                       (void *)&(pCfg->rateClasses[PolicyId]),
                       (void *)&(pTxRatePolicy->rateClass[PolicyId]),
                       sizeof(TTxRateClass));
    }
    
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeRtsThreshold()
 ****************************************************************************
 * DESCRIPTION: Configure the RTS threshold
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeRtsThreshold (TI_HANDLE hCmdBld, TI_UINT16 uRtsThreshold, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    dot11RTSThreshold_t AcxElm_RtsThreshold;
    dot11RTSThreshold_t *pCfg = &AcxElm_RtsThreshold;

    /* Set information element header */
    pCfg->EleHdr.id = DOT11_RTS_THRESHOLD;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->RTSThreshold = ENDIAN_HANDLE_WORD(uRtsThreshold);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeRtsThreshold()
 ****************************************************************************
 * DESCRIPTION: Configure the tx fragmentation threshold
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeFragmentThreshold (TI_HANDLE hCmdBld, TI_UINT16 uFragmentThreshold, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXFRAGThreshold_t AcxElm_FragmentThreshold;
    ACXFRAGThreshold_t *pCfg = &AcxElm_FragmentThreshold;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_FRAG_CFG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->fragThreshold = ENDIAN_HANDLE_WORD(uFragmentThreshold);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIePmConfig()
 ****************************************************************************
 * DESCRIPTION: Configure PM parameters
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIePmConfig (TI_HANDLE   hCmdBld, 
                                TI_UINT32   uHostClkSettlingTime,
                                TI_UINT8    uHostFastWakeupSupport, 
                                void *      fCb, 
                                TI_HANDLE   hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXPMConfig_t tPmConfig;
    ACXPMConfig_t *pCfg = &tPmConfig;

    /* Set information element header*/
    pCfg->EleHdr.id  = ACX_PM_CONFIG;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->hostClkSettlingTime   = uHostClkSettlingTime;
    pCfg->hostFastWakeupSupport = uHostFastWakeupSupport;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeTxCmpltPacing()
 ****************************************************************************
 * DESCRIPTION: Configure Tx-Complete interrupt pacing to FW
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeTxCmpltPacing (TI_HANDLE  hCmdBld,
                                     TI_UINT16  uTxCompletePacingThreshold,
                                     TI_UINT16  uTxCompletePacingTimeout,
                                     void *     fCb, 
                                     TI_HANDLE  hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXTxConfigOptions_t  tTxCmpltPacing;
    ACXTxConfigOptions_t  *pCfg = &tTxCmpltPacing;

    /* Set information element header */
    pCfg->EleHdr.id  = ACX_TX_CONFIG_OPT;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->txCompleteThreshold = ENDIAN_HANDLE_WORD(uTxCompletePacingThreshold);
    pCfg->txCompleteTimeout   = ENDIAN_HANDLE_WORD(uTxCompletePacingTimeout);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeRxIntrPacing()
 ****************************************************************************
 * DESCRIPTION: Configure Rx-Complete interrupt pacing to FW
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeRxIntrPacing (TI_HANDLE  hCmdBld,
                                    TI_UINT16  uRxIntrPacingThreshold,
                                    TI_UINT16  uRxIntrPacingTimeout,
                                    void *     fCb, 
                                    TI_HANDLE  hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXRxBufferingConfig_t  tRxIntrPacing;
    ACXRxBufferingConfig_t  *pCfg = &tRxIntrPacing;

    /* Set information element header */
    pCfg->EleHdr.id  = ACX_RX_CONFIG_OPT;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->rxPktThreshold    = ENDIAN_HANDLE_WORD(uRxIntrPacingThreshold);
    pCfg->rxCompleteTimeout = ENDIAN_HANDLE_WORD(uRxIntrPacingTimeout);
    pCfg->rxMblkThreshold   = ENDIAN_HANDLE_WORD(0xFFFF);    /* Set to maximum so it has no effect (only the PktThreshold is used) */
    pCfg->rxQueueType       = RX_QUEUE_TYPE_RX_LOW_PRIORITY; /* Only low priority data packets are buffered */

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
*                      cmdBld_CfgIeCtsProtection()
 ****************************************************************************
 * DESCRIPTION: Configure The Cts to self feature
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeCtsProtection (TI_HANDLE hCmdBld, TI_UINT8 ctsProtection, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXCtsProtection_t AcxElm_CtsToSelf;
    ACXCtsProtection_t *pCfg = &AcxElm_CtsToSelf;

    /* Set information element header*/
    pCfg->EleHdr.id = ACX_CTS_PROTECTION;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->ctsProtectMode = ctsProtection;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeRxMsduLifeTime()
 ****************************************************************************
 * DESCRIPTION: Configure The Cts to self feature
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeRxMsduLifeTime (TI_HANDLE hCmdBld, TI_UINT32 RxMsduLifeTime, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    dot11RxMsduLifeTime_t   AcxElm_RxMsduLifeTime;
    dot11RxMsduLifeTime_t *pCfg = &AcxElm_RxMsduLifeTime;

    /* Set information element header*/
    pCfg->EleHdr.id = DOT11_RX_MSDU_LIFE_TIME;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);
    pCfg->RxMsduLifeTime = RxMsduLifeTime;

    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": RxMsduLifeTime = 0x%x, len = 0x%x\n",pCfg->RxMsduLifeTime,pCfg->EleHdr.len);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeServicePeriodTimeout()
 ****************************************************************************
 * DESCRIPTION: Configure The Rx Time Out
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeServicePeriodTimeout (TI_HANDLE hCmdBld, TRxTimeOut* pRxTimeOut, void *fCb, TI_HANDLE hCb)                            
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXRxTimeout_t AcxElm_rxTimeOut;
    ACXRxTimeout_t *pCfg = &AcxElm_rxTimeOut;

    /* Set information element header*/
    pCfg->EleHdr.id = ACX_SERVICE_PERIOD_TIMEOUT;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->PsPollTimeout = pRxTimeOut->psPoll;
    pCfg->UpsdTimeout   = pRxTimeOut->UPSD;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIePsWmm()
 ****************************************************************************
 * DESCRIPTION: Configure The PS for WMM
 *
 * INPUTS:   TI_TRUE  - Configure PS to work on WMM mode - do not send the NULL/PS_POLL 
 *                   packets even if TIM is set.
 *           TI_FALSE - Configure PS to work on Non-WMM mode - work according to the 
 *                   standard
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIePsWmm (TI_HANDLE hCmdBld, TI_BOOL enableWA, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXConfigPsWmm_t  ConfigPsWmm;
    ACXConfigPsWmm_t *pCfg = &ConfigPsWmm;

    /*
     * Set information element header
     */
    pCfg->EleHdr.id = ACX_CONFIG_PS_WMM;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    pCfg->ConfigPsOnWmmMode = enableWA;

    /* Report the meesage only if we are using the WiFi patch */
    if (enableWA)
    {
        TRACE0(pCmdBld->hReport, REPORT_SEVERITY_CONSOLE, "cmdBld_CfgIePsWmm: PS is on WMM mode\n");
        WLAN_OS_REPORT(("%s PS is on WMM mode\n",__FUNCTION__));
    }
    
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CfgIeRssiSnrTrigger()
 ****************************************************************************
 * DESCRIPTION: Configure the RSSI/SNR Trigger parameters
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: OK or NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeRssiSnrTrigger (TI_HANDLE hCmdBld, RssiSnrTriggerCfg_t *pTriggerParam, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXRssiSnrTriggerCfg_t  tAcxTriggerParameters;
    ACXRssiSnrTriggerCfg_t *pCfg = &tAcxTriggerParameters;

    pCfg->param.index       = pTriggerParam->index    ;        
    pCfg->param.threshold   = pTriggerParam->threshold;        
    pCfg->param.pacing      = pTriggerParam->pacing   ;        
    pCfg->param.metric      = pTriggerParam->metric   ;        
    pCfg->param.type        = pTriggerParam->type     ;        
    pCfg->param.direction   = pTriggerParam->direction;        
    pCfg->param.hystersis   = pTriggerParam->hystersis;        
    pCfg->param.enable      = pTriggerParam->enable   ;        

    /* Set information element header */
    pCfg->EleHdr.id = ACX_RSSI_SNR_TRIGGER;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    TRACE8(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "ID=%u: threshold=%u, pacing=%u, metric=%u, type=%u, dir=%u, hyst=%u, enable=%u\n", pTriggerParam->index, pTriggerParam->threshold, pTriggerParam->pacing, pTriggerParam->metric, pTriggerParam->type, pTriggerParam->direction, pTriggerParam->hystersis, pTriggerParam->enable);
                                             
    /* Send the configuration command */
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CfgIeRssiSnrWeights()
 ****************************************************************************
 * DESCRIPTION: Configure the RSSI/SNR Trigger parameters
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: OK or NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeRssiSnrWeights (TI_HANDLE hCmdBld, RssiSnrAverageWeights_t *pWeightsParam, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXRssiSnrAverageWeights_t  tAcxAverageWeights;
    ACXRssiSnrAverageWeights_t *pCfg = &tAcxAverageWeights;

    pCfg->param.rssiBeaconAverageWeight = pWeightsParam->rssiBeaconAverageWeight;        
    pCfg->param.rssiPacketAverageWeight = pWeightsParam->rssiPacketAverageWeight;        
    pCfg->param.snrBeaconAverageWeight  = pWeightsParam->snrBeaconAverageWeight ;        
    pCfg->param.snrPacketAverageWeight  = pWeightsParam->snrPacketAverageWeight ;        

    /* Set information element header */
    pCfg->EleHdr.id = ACX_RSSI_SNR_WEIGHTS;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "rssi-beacon-avg-weight=%u, rssi-packet-avg-weight=%u, snr-beacon-avg-weight=%u, snr-packet-avg-weight=%u", pWeightsParam->rssiBeaconAverageWeight, pWeightsParam->rssiPacketAverageWeight, pWeightsParam->snrBeaconAverageWeight, pWeightsParam->snrPacketAverageWeight);

    /* Send the configuration command */
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


 /*
 * ----------------------------------------------------------------------------
 * Function : cmdBld_CfgIeBet
 *
 * Input    :   enabled               - 0 to disable BET, 0 to disable BET
 *              MaximumConsecutiveET  - Max number of consecutive beacons
 *                                      that may be early terminated.
 * Output   : TI_STATUS
 * Process  :  Configures Beacon Early Termination information element.
 * Note(s)  :  None
 * -----------------------------------------------------------------------------
 */
TI_STATUS cmdBld_CfgIeBet (TI_HANDLE hCmdBld, TI_UINT8 Enable, TI_UINT8 MaximumConsecutiveET, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    ACXBet_Enable_t ACXBet_Enable;
    ACXBet_Enable_t* pCfg = &ACXBet_Enable;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_BET_ENABLE;
    pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

    /* Set configuration fields */
    pCfg->Enable = Enable;
    pCfg->MaximumConsecutiveET = MaximumConsecutiveET;

    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": Sending info elem to firmware, Enable=%d, MaximumConsecutiveET=%d\n", (TI_UINT32)pCfg->Enable, (TI_UINT32)pCfg->MaximumConsecutiveET);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CmdIeConfigureKeepAliveParams()
 ****************************************************************************
 * DESCRIPTION: Configure keep-alive parameters for a single template
 *
 * INPUTS:  hCmdBld     - handle to command builder object
 *          uIndex      - keep-alive index
 *          uEnaDisFlag - whether keep-alive is enabled or disables
 *          trigType    - send keep alive when TX is idle or always
 *          interval    - keep-alive interval
 *          fCB         - callback function for command complete
 *          hCb         - handle to be apssed to callback function    
 *
 * OUTPUT:  None
 *
 * RETURNS: OK or NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeConfigureKeepAliveParams (TI_HANDLE hCmdBld, TI_UINT8 uIndex,
                                                TI_UINT8 uEnaDisFlag, TI_UINT8 trigType,
                                                TI_UINT32 interval, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    AcxSetKeepAliveConfig_t ACXKeepAlive;

    /* set IE header */
    ACXKeepAlive.EleHdr.id = ACX_SET_KEEP_ALIVE_CONFIG;
    ACXKeepAlive.EleHdr.len = sizeof (AcxSetKeepAliveConfig_t) - sizeof (EleHdrStruct);

    /* set Keep-Alive values */
    ACXKeepAlive.index = uIndex;
    ACXKeepAlive.period = interval;
    ACXKeepAlive.trigger = trigType;
    ACXKeepAlive.valid = uEnaDisFlag;

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": Sending info elem to firmware, index=%d, enaDis=%d, trigType=%d, interval=%d\n", (TI_UINT32)ACXKeepAlive.index, (TI_UINT32)ACXKeepAlive.valid, (TI_UINT32)ACXKeepAlive.trigger, (TI_UINT32)ACXKeepAlive.period);

    /* send the command to the FW */
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, &ACXKeepAlive, sizeof(AcxSetKeepAliveConfig_t), fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CmdIeConfigureKeepAliveParams()
 ****************************************************************************
 * DESCRIPTION: Configure keep-alive global enable / disable flag
 *
 * INPUTS:  enable / disable flag
 *
 * OUTPUT:  None
 *
 * RETURNS: OK or NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeConfigureKeepAliveEnaDis (TI_HANDLE hCmdBld, TI_UINT8 enaDisFlag, 
                                                void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    AcxKeepAliveMode ACXKeepAliveMode;

    /* set IE header */
    ACXKeepAliveMode.EleHdr.id = ACX_KEEP_ALIVE_MODE;
    ACXKeepAliveMode.EleHdr.len = sizeof (AcxKeepAliveMode) - sizeof (EleHdrStruct);

    /* set Keep-Alive mode */
    ACXKeepAliveMode.modeEnabled = enaDisFlag;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, ": Sending info elem to firmware, enaDis=%d\n", (TI_UINT32)ACXKeepAliveMode.modeEnabled);

    /* send the command to the FW */
    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, &ACXKeepAliveMode, sizeof(AcxKeepAliveMode), fCb, hCb, NULL);
}

/** 
 * \fn     cmdBld_CfgIeSetFwHtCapabilities 
 * \brief  set the current AP HT Capabilities to the FW. 
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgIeSetFwHtCapabilities (TI_HANDLE hCmdBld,
                                           TI_UINT32 uHtCapabilites,
                                           TMacAddr  tMacAddress,
                                           TI_UINT8  uAmpduMaxLeng,
                                           TI_UINT8  uAmpduMinSpac,
                                           void      *fCb, 
                                           TI_HANDLE hCb)
{
    TCmdBld                         *pCmdBld = (TCmdBld *)hCmdBld;
    TAxcHtCapabilitiesIeFwInterface tAcxFwHtCap;
    TAxcHtCapabilitiesIeFwInterface *pCfg    = &tAcxFwHtCap;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_PEER_HT_CAP;
    pCfg->EleHdr.len = sizeof(tAcxFwHtCap) - sizeof(EleHdrStruct);

    MAC_COPY (pCfg->aMacAddress, tMacAddress);
    pCfg->uHtCapabilites = uHtCapabilites;
    pCfg->uAmpduMaxLength = uAmpduMaxLeng;
    pCfg->uAmpduMinSpacing = uAmpduMinSpac;
 
    TRACE9(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgIeSetFwHtCapabilities: HtCapabilites=0x%x, AmpduMaxLength=%d, AmpduMinSpac=%d, MAC: %x:%x:%x:%x:%x:%x\n", uHtCapabilites, uAmpduMaxLeng, uAmpduMinSpac, pCfg->aMacAddress[0], pCfg->aMacAddress[1], pCfg->aMacAddress[2], pCfg->aMacAddress[3], pCfg->aMacAddress[4], pCfg->aMacAddress[5]);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(TAxcHtCapabilitiesIeFwInterface), fCb, hCb, NULL);
    
}

/** 
 * \fn     cmdBld_CfgIeSetFwHtInformation 
 * \brief  set the current AP HT Information to the FW. 
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgIeSetFwHtInformation (TI_HANDLE hCmdBld,
                                          TI_UINT8  uRifsMode,           
                                          TI_UINT8  uHtProtection,       
                                          TI_UINT8  uGfProtection,       
                                          TI_UINT8  uHtTxBurstLimit,     
                                          TI_UINT8  uDualCtsProtection,  
                                          void      *fCb, 
                                          TI_HANDLE hCb)
{
    TCmdBld                        *pCmdBld = (TCmdBld *)hCmdBld;
    TAxcHtInformationIeFwInterface tAcxFwHtInf;
    TAxcHtInformationIeFwInterface *pCfg = &tAcxFwHtInf;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_HT_BSS_OPERATION;
    pCfg->EleHdr.len = sizeof(tAcxFwHtInf) - sizeof(EleHdrStruct);

    pCfg->uRifsMode = uRifsMode;
    pCfg->uHtProtection = uHtProtection;
    pCfg->uGfProtection = uGfProtection;
    pCfg->uHtTxBurstLimit = uHtTxBurstLimit;
    pCfg->uDualCtsProtection = uDualCtsProtection;
 
    TRACE5(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgIeSetFwHtInformation: RifsMode=0x%x, HtProtection=0x%x, GfProtection=0x%x, HtTxBurstLimit=0x%x, DualCtsProtection=0x%x\n", uRifsMode, uHtProtection, uGfProtection, uHtTxBurstLimit, uDualCtsProtection);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(TAxcHtInformationIeFwInterface), fCb, hCb, NULL);
}

/** 
 * \fn     cmdBld_CfgIeSetBaSession 
 * \brief  configure BA session initiator\receiver parameters setting in the FW. 
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgIeSetBaSession (TI_HANDLE hCmdBld, 
                                    InfoElement_e eBaType,
                                    TI_UINT8 uTid,               
                                    TI_UINT8 uState,             
                                    TMacAddr tRa,                
                                    TI_UINT16 uWinSize,          
                                    TI_UINT16 uInactivityTimeout,
                                    void *fCb, 
                                    TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TAxcBaSessionInitiatorResponderPolicy tAcxBaSessionPrm;
    TAxcBaSessionInitiatorResponderPolicy *pCfg = &tAcxBaSessionPrm;

    /* Set information element header */
    pCfg->EleHdr.id = eBaType;
    pCfg->EleHdr.len = sizeof(tAcxBaSessionPrm) - sizeof(EleHdrStruct);

    MAC_COPY (pCfg->aMacAddress, tRa);
    pCfg->uTid = uTid;
    pCfg->uPolicy = uState;
    pCfg->uWinSize = uWinSize;

    if (eBaType == ACX_BA_SESSION_INITIATOR_POLICY)
    {
        pCfg->uInactivityTimeout = uInactivityTimeout;
    }
    else
    {
        if (eBaType == ACX_BA_SESSION_RESPONDER_POLICY)
        {
            pCfg->uInactivityTimeout = 0;
        }
        else
        {
            TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_CfgIeSetBaSession: error ID=%u\n", pCfg->EleHdr.id);
            return TI_NOK;
        }
    }

    TRACE10(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgIeSetBaSession: ID=, TID=%u, Policy=%u, MAC: %x:%x:%x:%x:%x:%x, uWinSize=%u, Timeout=%u\n", pCfg->uTid, pCfg->uPolicy, pCfg->aMacAddress[0], pCfg->aMacAddress[1], pCfg->aMacAddress[2], pCfg->aMacAddress[3], pCfg->aMacAddress[4], pCfg->aMacAddress[5], pCfg->uWinSize, pCfg->uInactivityTimeout);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(TAxcBaSessionInitiatorResponderPolicy), fCb, hCb, NULL);
}

/** 
 * \fn     cmdBld_CfgIeRadioParams 
 * \brief  configure radio parameters setting in the FW. 
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgIeRadioParams (TI_HANDLE hCmdBld, IniFileRadioParam *pIniFileRadioParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_STATUS status = TI_NOK;
    TTestCmd *pTestCmd;

    pTestCmd = os_memoryAlloc(pCmdBld->hOs, sizeof(TTestCmd));
    if (!pTestCmd)
    {
        return status;
    }

    pTestCmd->testCmdId = TEST_CMD_INI_FILE_RADIO_PARAM;

    os_memoryCopy(pCmdBld->hOs, &pTestCmd->testCmd_u.IniFileRadioParams, pIniFileRadioParams, sizeof(IniFileRadioParam));

    status = cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_TEST, 
                             (void *)pTestCmd, 
                             sizeof(IniFileRadioParam) + 4,
                             fCb, 
                             hCb, 
                             NULL);
    os_memoryFree(pCmdBld->hOs, pTestCmd, sizeof(TTestCmd));
    return status;
}


/** 
 * \fn     cmdBld_CfgIeExtendedRadioParams 
 * \brief  configure extended radio parameters setting in the
 * FW.
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgIeExtendedRadioParams (TI_HANDLE hCmdBld,
										   IniFileExtendedRadioParam *pIniFileExtRadioParams,
										   void *fCb,
										   TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_STATUS status = TI_NOK;
    TTestCmd *pTestCmd;

    pTestCmd = os_memoryAlloc(pCmdBld->hOs, sizeof(TTestCmd));
    if (!pTestCmd)
    {
        return status;
    }

    pTestCmd->testCmdId = TEST_CMD_INI_FILE_RF_EXTENDED_PARAM;
    
    os_memoryCopy(pCmdBld->hOs, &pTestCmd->testCmd_u.IniFileExtendedRadioParams,
				  pIniFileExtRadioParams, sizeof(IniFileExtendedRadioParam));

    status = cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_TEST, 
                             (void *)pTestCmd, 
                             sizeof(IniFileExtendedRadioParam) + 4,
                             fCb, 
                             hCb, 
                             NULL);
    os_memoryFree(pCmdBld->hOs, pTestCmd, sizeof(TTestCmd));
    return status;
}


TI_STATUS cmdBld_CfgPlatformGenParams (TI_HANDLE hCmdBld, IniFileGeneralParam *pGenParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_STATUS status = TI_NOK;
    TTestCmd *pTestCmd;

    pTestCmd = os_memoryAlloc(pCmdBld->hOs, sizeof(TTestCmd));
    if (!pTestCmd)
    {
        return status;
    }

    pTestCmd->testCmdId = TEST_CMD_INI_FILE_GENERAL_PARAM;
    
    os_memoryCopy(pCmdBld->hOs, &pTestCmd->testCmd_u.IniFileGeneralParams, pGenParams, sizeof(IniFileGeneralParam));

    status = cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                              CMD_TEST, 
                              (void *)pTestCmd, 
                              sizeof(IniFileGeneralParam),
                              fCb, 
                              hCb, 
                              NULL);    
    os_memoryFree(pCmdBld->hOs, pTestCmd, sizeof(TTestCmd));
    return status;
}


/****************************************************************************
 *                      cmdBld_CfgIeBurstMode()
 ****************************************************************************
 * DESCRIPTION: Configure burst mode
 *
 * INPUTS:  hCmdBld     - handle to command builder object
 *          bEnabled    - is enabled flag
 *          fCB         - callback function for command complete
 *          hCb         - handle to be apssed to callback function
 *
 * OUTPUT:  None
 *
 * RETURNS: OK or NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeBurstMode (TI_HANDLE hCmdBld, TI_BOOL bEnabled, void *fCb, TI_HANDLE hCb)
{
	TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
	AcxBurstMode tAcxBurstMode;
	AcxBurstMode *pCfg = &tAcxBurstMode;

	/* set IE header */
	pCfg->EleHdr.id = ACX_BURST_MODE;
	pCfg->EleHdr.len = sizeof(*pCfg) - sizeof(EleHdrStruct);

	/* set burst mode value */
	pCfg->enable = (uint8)bEnabled;

	/* send the command to the FW */
	return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(*pCfg), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CfgIeDcoItrimParams()
 ****************************************************************************
 * DESCRIPTION: Configure/Interrogate the DCO Itrim parameters
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgIeDcoItrimParams (TI_HANDLE hCmdBld, TI_BOOL enable, TI_UINT32 moderationTimeoutUsec, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ACXDCOItrimParams_t ACXBeaconFilterOptions;
    ACXDCOItrimParams_t *pCfg = &ACXBeaconFilterOptions;
    
    pCfg->enable = enable;
    pCfg->moderation_timeout_usec = moderationTimeoutUsec;

    /* Set information element header */
    pCfg->EleHdr.id = ACX_SET_DCO_ITRIM_PARAMS;
    pCfg->EleHdr.len = sizeof(ACXDCOItrimParams_t) - sizeof(EleHdrStruct);

    TRACE3(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "ID=%u: enable=%u, moderation_timeout_usec=%u\n", pCfg->EleHdr.id, enable, moderationTimeoutUsec);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CONFIGURE, pCfg, sizeof(ACXDCOItrimParams_t), fCb, hCb, NULL);
}
