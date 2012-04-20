/*
 * Ctrl.c
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

/***************************************************************************/
/*                                                                         */
/*      MODULE:     Ctrl.c                                                 */
/*      PURPOSE:    Control module functions                               */
/*                                                                         */
/***************************************************************************/
#define __FILE_ID__  FILE_ID_51
#include "Ctrl.h"
#include "802_11Defs.h"
#include "DataCtrl_Api.h"
#include "osApi.h"
#include "report.h" 
#include "smeApi.h"
#include "siteMgrApi.h"
#include "TrafficMonitorAPI.h"
#include "TI_IPC_Api.h"
#include "EvHandler.h"
#include "apConn.h"
#include "rate.h"
#include "TWDriver.h"
#include "DrvMainModules.h"
#include "StaCap.h"

static void selectRateTable(TI_HANDLE hCtrlData, TI_UINT32 rateMask);
static void ctrlData_setTxRatePolicies(ctrlData_t *pCtrlData);
static void ctrlData_UnregisterTrafficIntensityEvents (TI_HANDLE hCtrlData);
static void ctrlData_RegisterTrafficIntensityEvents (TI_HANDLE hCtrlData);
static void ctrlData_storeTSRateSet(ctrlData_t *pCtrlData, TTxDataQosParams *tsrsParams);
static void ctrlData_TrafficThresholdCrossed(TI_HANDLE Context,TI_UINT32 Cookie);


/* definitions for medium usage calculations - in uSec units*/
#define AVERAGE_ACK_TIME   10
#define AVERAGE_CTS_TIME   20
#define B_SIFS             10

#define SHORT_PREAMBLE_TIME   96
#define LONG_PREAMBLE_TIME    192

#define OFDM_PREAMBLE      12
#define OFDM_SIGNAL_EXT    6
#define OFDM_PLCP_HDR      24

#define OFDM_DURATION           (B_SIFS + OFDM_PLCP_HDR + OFDM_SIGNAL_EXT)
#define NONOFDM_SHORT_DURATION  (B_SIFS + SHORT_PREAMBLE_TIME)
#define NONOFDM_LONG_DURATION   (B_SIFS + LONG_PREAMBLE_TIME)

/*************************************************************************
*                        ctrlData_create                                 *
**************************************************************************
* DESCRIPTION:  This function initializes the Ctrl data module.                 
*                                                      
* INPUT:        hOs - handle to Os Abstraction Layer
*               
* RETURN:       Handle to the allocated Ctrl data control block
************************************************************************/
TI_HANDLE ctrlData_create(TI_HANDLE hOs)
{
    TI_HANDLE hCtrlData;

    if( hOs  == NULL )
    {
        WLAN_OS_REPORT(("FATAL ERROR: ctrlData_create(): OS handle Error - Aborting\n"));
        return NULL;
    }

    /* alocate Control module control block */
    hCtrlData = os_memoryAlloc (hOs, sizeof(ctrlData_t));
    if (!hCtrlData)
    {
        return NULL;
    }

    /* reset control module control block */
    os_memoryZero (hOs, hCtrlData, sizeof(ctrlData_t));

    ((ctrlData_t *)hCtrlData)->hOs = hOs;

    return (hCtrlData);
}


/***************************************************************************
*                           ctrlData_config                                *
****************************************************************************
* DESCRIPTION:  This function configures the Ctrl Data module       
* 
* INPUTS:       pStadHandles        - Other modules handles
*               retriesUpdateCBFunc - Link test retries callback function
*               retriesUpdateCBObj  - Link test retries callback handle
*               
* OUTPUT:       
* 
* RETURNS:      void
***************************************************************************/
void ctrlData_init (TStadHandlesList *pStadHandles,                       
                    retriesCB_t       retriesUpdateCBFunc,
                    TI_HANDLE         retriesUpdateCBObj)	
{
    ctrlData_t *pCtrlData = (ctrlData_t *)(pStadHandles->hCtrlData);

    /* set objects handles */
    pCtrlData->hTWD         = pStadHandles->hTWD;  
    pCtrlData->hSiteMgr     = pStadHandles->hSiteMgr;
	pCtrlData->hTxCtrl      = pStadHandles->hTxCtrl;
    pCtrlData->hRxData      = pStadHandles->hRxData;
    pCtrlData->hOs          = pStadHandles->hOs;
    pCtrlData->hReport      = pStadHandles->hReport;
    pCtrlData->hAPConn      = pStadHandles->hAPConnection;
    pCtrlData->hEvHandler   = pStadHandles->hEvHandler;
    pCtrlData->hTrafficMonitor = pStadHandles->hTrafficMon;
    pCtrlData->hTxDataQ     = pStadHandles->hTxDataQ;
    pCtrlData->hStaCap      = pStadHandles->hStaCap;

#ifdef XCC_MODULE_INCLUDED
	/* Register the link test retries CB */
	pCtrlData->retriesUpdateCBFunc = retriesUpdateCBFunc;
	pCtrlData->retriesUpdateCBObj  = retriesUpdateCBObj;
#endif
    
    TRACE0(pCtrlData->hReport, REPORT_SEVERITY_INIT, ".....Ctrl Data configured successfully ...\n");
}


TI_STATUS ctrlData_SetDefaults (TI_HANDLE hCtrlData, ctrlDataInitParams_t *ctrlDataInitParams)
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    TI_UINT32 ac;

    /*  set Control module parameters */
    pCtrlData->ctrlDataDesiredIbssProtection = ctrlDataInitParams->ctrlDataDesiredIbssProtection;
    pCtrlData->ctrlDataIbssProtectionType = ctrlDataInitParams->ctrlDataDesiredIbssProtection;
    pCtrlData->ctrlDataRtsCtsStatus = ctrlDataInitParams->ctrlDataDesiredCtsRtsStatus;

    MAC_COPY (pCtrlData->ctrlDataDeviceMacAddress, 
              ctrlDataInitParams->ctrlDataDeviceMacAddress);

    pCtrlData->ctrlDataCurrentBssType = BSS_INFRASTRUCTURE;

    /* Set short/long retry for all ACs plus one policy for management packets */
    for (ac=0; ac < MAX_NUM_OF_AC + 1; ac++)
    {
        pCtrlData->ctrlDataTxRatePolicy.rateClass[ac].longRetryLimit  = ctrlDataInitParams->ctrlDataTxRatePolicy.longRetryLimit;
        pCtrlData->ctrlDataTxRatePolicy.rateClass[ac].shortRetryLimit = ctrlDataInitParams->ctrlDataTxRatePolicy.shortRetryLimit;
    }

    /* Set enabled rates bitmap for each rates mode */
    pCtrlData->policyEnabledRatesMaskCck   = ctrlDataInitParams->policyEnabledRatesMaskCck;
    pCtrlData->policyEnabledRatesMaskOfdm  = ctrlDataInitParams->policyEnabledRatesMaskOfdm; 
    pCtrlData->policyEnabledRatesMaskOfdmA = ctrlDataInitParams->policyEnabledRatesMaskOfdmA; 
    pCtrlData->policyEnabledRatesMaskOfdmN = ctrlDataInitParams->policyEnabledRatesMaskOfdmN;

    ctrlData_updateTxRateAttributes(hCtrlData); /* Update TxCtrl module with rate change.*/

    /* Initialize traffic intensity threshold parameters */
    pCtrlData->ctrlDataTrafficIntensityEventsEnabled = ctrlDataInitParams->ctrlDataTrafficThresholdEnabled;
    pCtrlData->ctrlDataTrafficIntensityThresholds.uHighThreshold = ctrlDataInitParams->ctrlDataTrafficThreshold.uHighThreshold;
    pCtrlData->ctrlDataTrafficIntensityThresholds.uLowThreshold = ctrlDataInitParams->ctrlDataTrafficThreshold.uLowThreshold;
    pCtrlData->ctrlDataTrafficIntensityThresholds.TestInterval = ctrlDataInitParams->ctrlDataTrafficThreshold.TestInterval;

    TRACE4(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "\nTraffic Intensity parameters:\nEvents enabled = %d\nuHighThreshold = %d\nuLowThreshold = %d\nTestInterval = %d\n\n", pCtrlData->ctrlDataTrafficIntensityEventsEnabled, pCtrlData->ctrlDataTrafficIntensityThresholds.uHighThreshold, pCtrlData->ctrlDataTrafficIntensityThresholds.uLowThreshold, pCtrlData->ctrlDataTrafficIntensityThresholds.TestInterval);

    /* Register the traffic intensity events with the traffic monitor */
    ctrlData_RegisterTrafficIntensityEvents (pCtrlData);

    /* If the events are enabled, start notification, if disabled - then do nothing */
    ctrlData_ToggleTrafficIntensityNotification (pCtrlData, pCtrlData->ctrlDataTrafficIntensityEventsEnabled);

    return TI_OK;
}


/***************************************************************************
*                           ctrlData_unLoad                                *
****************************************************************************
* DESCRIPTION:  This function unload the Ctrl data module. 
* 
* INPUTS:       hCtrlData - the object
*       
* OUTPUT:       
* 
* RETURNS:      TI_OK - Unload succesfull
*               TI_NOK - Unload unsuccesfull
***************************************************************************/
TI_STATUS ctrlData_unLoad(TI_HANDLE hCtrlData)  
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;

    /* check parameters validity */
    if( pCtrlData == NULL )
    {
        return TI_NOK;
    }

    /* free control module object */
    os_memoryFree(pCtrlData->hOs, hCtrlData, sizeof(ctrlData_t));

    return TI_OK;
}

TI_STATUS ctrlData_getParamProtType(TI_HANDLE hCtrlData, erpProtectionType_e *protType)
{ /* CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM */
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;

    *protType = pCtrlData->ctrlDataIbssProtectionType;
    return TI_OK;
}

TI_STATUS ctrlData_getParamPreamble(TI_HANDLE hCtrlData, EPreamble *preamble)
{ /* CTRL_DATA_CURRENT_PREAMBLE_TYPE_PARAM */
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;

    *preamble = pCtrlData->ctrlDataCurrentPreambleType;
    return TI_OK;
}

/***************************************************************************
*                           ctrlData_getParamBssid                         *
****************************************************************************
* DESCRIPTION:  get a specific parameter related to Bssid
* 
* INPUTS:       hCtrlData - the object
*               paramVal  - type of parameter
*               
*       
* OUTPUT:       bssid
* 
* RETURNS:      TI_OK
*               TI_NOK
***************************************************************************/
TI_STATUS ctrlData_getParamBssid(TI_HANDLE hCtrlData, EInternalParam paramVal, TMacAddr bssid)
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;

    if (paramVal == CTRL_DATA_CURRENT_BSSID_PARAM) {
        MAC_COPY (bssid, pCtrlData->ctrlDataCurrentBSSID);
    }
    else if (paramVal == CTRL_DATA_MAC_ADDRESS) {
        TFwInfo *pFwInfo = TWD_GetFWInfo (pCtrlData->hTWD);
        MAC_COPY (bssid, pFwInfo->macAddress); 
    }

    return TI_OK;
}

/***************************************************************************
*                           ctrlData_getParam                              *
****************************************************************************
* DESCRIPTION:  get a specific parameter
* 
* INPUTS:       hCtrlData - the object
*               
*       
* OUTPUT:       pParamInfo - structure which include the value of 
*               the requested parameter
* 
* RETURNS:      TI_OK
*               TI_NOK
***************************************************************************/

TI_STATUS ctrlData_getParam(TI_HANDLE hCtrlData, paramInfo_t *pParamInfo)   
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    
    TRACE1(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "ctrlData_getParam() : param=0x%x \n", pParamInfo->paramType);

    switch (pParamInfo->paramType)
    {
    case CTRL_DATA_CURRENT_BSSID_PARAM:          
		MAC_COPY (pParamInfo->content.ctrlDataCurrentBSSID, 
                  pCtrlData->ctrlDataCurrentBSSID);
        break; 

    case CTRL_DATA_CURRENT_BSS_TYPE_PARAM:  
        pParamInfo->content.ctrlDataCurrentBssType = pCtrlData->ctrlDataCurrentBssType;        
        break; 

    case CTRL_DATA_CURRENT_PREAMBLE_TYPE_PARAM: 
        pParamInfo->content.ctrlDataCurrentPreambleType = pCtrlData->ctrlDataCurrentPreambleType;        
        break; 

    case CTRL_DATA_MAC_ADDRESS:   
        {
            TFwInfo *pFwInfo = TWD_GetFWInfo (pCtrlData->hTWD);
            MAC_COPY (pParamInfo->content.ctrlDataDeviceMacAddress, pFwInfo->macAddress); 
        }
        break;

    case CTRL_DATA_CURRENT_PROTECTION_STATUS_PARAM:
        pParamInfo->content.ctrlDataProtectionEnabled = pCtrlData->ctrlDataProtectionEnabled;
        break;

    case CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM:
        pParamInfo->content.ctrlDataIbssProtecionType = pCtrlData->ctrlDataIbssProtectionType;
        break;

    case CTRL_DATA_CURRENT_RTS_CTS_STATUS_PARAM:
        pParamInfo->content.ctrlDataRtsCtsStatus = pCtrlData->ctrlDataRtsCtsStatus;
        break;

    case CTRL_DATA_CLSFR_TYPE:
        txDataClsfr_GetClsfrType (pCtrlData->hTxDataQ, &pParamInfo->content.ctrlDataClsfrType);
        break;

    case CTRL_DATA_TRAFFIC_INTENSITY_THRESHOLD:
        pParamInfo->content.ctrlDataTrafficIntensityThresholds.uHighThreshold = pCtrlData->ctrlDataTrafficIntensityThresholds.uHighThreshold;
        pParamInfo->content.ctrlDataTrafficIntensityThresholds.uLowThreshold = pCtrlData->ctrlDataTrafficIntensityThresholds.uLowThreshold;
        pParamInfo->content.ctrlDataTrafficIntensityThresholds.TestInterval = pCtrlData->ctrlDataTrafficIntensityThresholds.TestInterval;
        break;

    default:
        TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " ctrlData_getParam() : PARAMETER NOT SUPPORTED \n");
		return (PARAM_NOT_SUPPORTED);
    }

    return (TI_OK);
}

/***************************************************************************
*                           ctrlData_buildSupportedHwRates                 *
****************************************************************************
* DESCRIPTION:  builds HwRatesBitMap (supported rates) for txRatePolicy by anding
*                   the AP support and the Policy rates (Enabled/Disabled rates)
* 
* OUTPUT:       
* 
* RETURNS:      TI_OK
*               TI_NOK
***************************************************************************/
static TI_UINT32 ctrlData_buildSupportedHwRates (TI_UINT32 APsupport,
                                                 TI_UINT32 policySupport)
{
    TI_UINT32  AppRateBitMap = 0;
    TI_UINT32  HwRatesBitMap = 0;

    /* 1. AND all Supported Rates masks */
    AppRateBitMap = APsupport & policySupport;

    /* 2. Incase there are no mutual rates: ignor Policy Rate Settings (use only AP Rates) */
	if ( AppRateBitMap == 0 )
	{
		AppRateBitMap = APsupport;
    }

    /* 3. Set total supported rates bit map for txRatePolicy */
    rate_DrvBitmapToHwBitmap (AppRateBitMap, &HwRatesBitMap);

    return HwRatesBitMap;
}


/***************************************************************************
*                           ctrlData_setTxRatePolicies                     *
****************************************************************************
* DESCRIPTION:  This function sets rate fallback policies to be configured to FW
*               If TSRS is defined to specific AC, the policy is derived from it,
*               otherwise it is derived from pre-defined map
* 
* INPUTS:       pCtrlData - the object
*       
* RETURNS:      -
*               
***************************************************************************/
static void ctrlData_setTxRatePolicies(ctrlData_t *pCtrlData)
{
    TI_UINT32 		ac; 
	TI_UINT32 		uPolicyRateMask;    /* policy rates */
	TI_UINT32 		uSupportedRateMask; /* AP supported rates */
    TI_UINT32       fwPolicyID = 0;
    TI_UINT32       uEnabledHwRatesMask;
    TI_UINT32       uShiftedBit;
    TI_UINT32       i;
    TTwdParamInfo   param;

    for (ac = 0; ac < MAX_NUM_OF_AC; ac++)
    {
        /* 1. If a special rate set is defined for this AC, use its related policy */
        /*    Otherwise use default settings for this class */
        if (pCtrlData->tsrsParameters[ac].supportedRatesMask != 0)
        {
            uPolicyRateMask    = pCtrlData->tsrsParameters[ac].policyClassRateMask;
            uSupportedRateMask = pCtrlData->tsrsParameters[ac].supportedRatesMask;
        }
        else
        {
            uPolicyRateMask    = pCtrlData->uCurrPolicyEnabledRatesMask;
            uSupportedRateMask = pCtrlData->ctrlDataCurrentRateMask;
        }

        /* 2. Build a bitMap for the supported rates */
        uEnabledHwRatesMask = ctrlData_buildSupportedHwRates (uSupportedRateMask, uPolicyRateMask);   
        pCtrlData->ctrlDataTxRatePolicy.rateClass[fwPolicyID].txEnabledRates = uEnabledHwRatesMask;

        TRACE2(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "ctrlData_setTxRatePolicies: AC %d, rate-policy 0x%x", ac, uEnabledHwRatesMask);

        /* Note that Long/Short retries are pre-set during configuration stage */

        /* 3. Finally, increase total number of policies */
        pCtrlData->tsrsParameters[ac].fwPolicyID = fwPolicyID++;

    } /* for (ac = 0; ac < MAX_NUM_OF_AC; ac++) */

    /* Add a specific policy for management packets, which uses only the lowest supported rate */
    pCtrlData->uMgmtPolicyId = fwPolicyID;
    uShiftedBit = 1;
    for (i = 0; i < 32; i++)
    {
        if ((uShiftedBit & uEnabledHwRatesMask) != 0)
        {
            break;
        }
        uShiftedBit = uShiftedBit << 1;
    }
    pCtrlData->ctrlDataTxRatePolicy.rateClass[fwPolicyID].txEnabledRates = uShiftedBit;
    fwPolicyID++;

    /* Download policies to the FW. Num of policies is 8 - one for each AC for every class */
    TRACE1(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "ctrlData_setTxRatePolicies: num of Rate policies: %d\n", fwPolicyID);

    pCtrlData->ctrlDataTxRatePolicy.numOfRateClasses = fwPolicyID;
    param.paramType = TWD_TX_RATE_CLASS_PARAM_ID;
    param.content.pTxRatePlicy = &pCtrlData->ctrlDataTxRatePolicy;

    TWD_SetParam (pCtrlData->hTWD, &param);
}


/***************************************************************************
*                           ctrlData_setParam                              *
****************************************************************************
* DESCRIPTION:  set a specific parameter
* 
* INPUTS:       hCtrlData - the object
*               pParamInfo - structure which include the value to set for 
*               the requested parameter
*       
* OUTPUT:       
* 
* RETURNS:      TI_OK
*               TI_NOK
***************************************************************************/

TI_STATUS ctrlData_setParam(TI_HANDLE hCtrlData, paramInfo_t *pParamInfo)   
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    TTwdParamInfo param;

    TRACE1(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "ctrlData_setParam() : param=0x%x \n", pParamInfo->paramType);

    switch (pParamInfo->paramType)
    {
    case CTRL_DATA_RATE_CONTROL_ENABLE_PARAM:
        selectRateTable(pCtrlData, pCtrlData->ctrlDataCurrentRateMask);

        ctrlData_setTxRatePolicies(pCtrlData);

		ctrlData_updateTxRateAttributes(hCtrlData); /* Update the TxCtrl module with rate changes. */

        break; 
        
    case CTRL_DATA_CURRENT_BSSID_PARAM:          
		MAC_COPY (pCtrlData->ctrlDataCurrentBSSID,
			      pParamInfo->content.ctrlDataCurrentBSSID);
		txCtrlParams_setBssId (pCtrlData->hTxCtrl, &(pCtrlData->ctrlDataCurrentBSSID));
        break; 

    case CTRL_DATA_CURRENT_BSS_TYPE_PARAM:
        if( pParamInfo->content.ctrlDataCurrentBssType != BSS_INFRASTRUCTURE &&
            pParamInfo->content.ctrlDataCurrentBssType != BSS_INDEPENDENT )
            return(PARAM_VALUE_NOT_VALID);

        pCtrlData->ctrlDataCurrentBssType = pParamInfo->content.ctrlDataCurrentBssType;        
		txCtrlParams_setBssType (pCtrlData->hTxCtrl, pCtrlData->ctrlDataCurrentBssType);
        break; 

    case CTRL_DATA_CURRENT_PREAMBLE_TYPE_PARAM: 
        if( pParamInfo->content.ctrlDataCurrentPreambleType != PREAMBLE_LONG &&
            pParamInfo->content.ctrlDataCurrentPreambleType != PREAMBLE_SHORT )
            return(PARAM_VALUE_NOT_VALID);
 pCtrlData->ctrlDataCurrentPreambleType = pParamInfo->content.ctrlDataCurrentPreambleType;        
        break; 

    case CTRL_DATA_CURRENT_SUPPORTED_RATE_MASK_PARAM:
        pCtrlData->ctrlDataCurrentRateMask = pParamInfo->content.ctrlDataCurrentRateMask;
        selectRateTable(pCtrlData, pCtrlData->ctrlDataCurrentRateMask);
		ctrlData_updateTxRateAttributes(hCtrlData); /* Update the TxCtrl module with rate changes. */
        break; 

    case CTRL_DATA_TSRS_PARAM:
        ctrlData_storeTSRateSet(pCtrlData, &pParamInfo->content.txDataQosParams);

        break;

    case CTRL_DATA_CURRENT_PROTECTION_STATUS_PARAM:
        if (pCtrlData->ctrlDataProtectionEnabled != pParamInfo->content.ctrlDataProtectionEnabled)
        {
            pCtrlData->ctrlDataProtectionEnabled = pParamInfo->content.ctrlDataProtectionEnabled;

            /* set indication also to TNET */
            param.paramType = TWD_CTS_TO_SELF_PARAM_ID;
            if(pCtrlData->ctrlDataProtectionEnabled == TI_TRUE)
                param.content.halCtrlCtsToSelf = CTS_TO_SELF_ENABLE;
            else
                param.content.halCtrlCtsToSelf = CTS_TO_SELF_DISABLE;
            
            TWD_SetParam (pCtrlData->hTWD, &param);


            /* In case of using protection fragmentation should be disabled */
            param.paramType = TWD_FRAG_THRESHOLD_PARAM_ID;
            if(pCtrlData->ctrlDataProtectionEnabled == TI_TRUE)
            {
                /* save last non-protection mode fragmentation threshold */
                TWD_GetParam(pCtrlData->hTWD,&param);
                pCtrlData->lastFragmentThreshold = param.content.halCtrlFragThreshold;
                /* set fragmentation threshold to max (disable) */
                param.content.halCtrlFragThreshold = TWD_FRAG_THRESHOLD_MAX;
            }
            else
                param.content.halCtrlFragThreshold = pCtrlData->lastFragmentThreshold;
            
            TWD_SetParam(pCtrlData->hTWD,&param);
        }

        break;

    case CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM:
        if(ERP_PROTECTION_STANDARD == pCtrlData->ctrlDataDesiredIbssProtection) 
        {
            pCtrlData->ctrlDataIbssProtectionType = pParamInfo->content.ctrlDataIbssProtecionType;
        }
        else
        {
            pCtrlData->ctrlDataIbssProtectionType = ERP_PROTECTION_NONE;
        }
                       
        /* set indication also to TNET */
        param.paramType = TWD_CTS_TO_SELF_PARAM_ID;
        if(pCtrlData->ctrlDataIbssProtectionType != ERP_PROTECTION_NONE)
            param.content.halCtrlCtsToSelf = CTS_TO_SELF_ENABLE;
        else
            param.content.halCtrlCtsToSelf = CTS_TO_SELF_DISABLE;

        TWD_SetParam (pCtrlData->hTWD, &param);
        break;

    case CTRL_DATA_CURRENT_RTS_CTS_STATUS_PARAM:
        pCtrlData->ctrlDataRtsCtsStatus = pParamInfo->content.ctrlDataRtsCtsStatus;
        break;
    case CTRL_DATA_CLSFR_TYPE:
        txDataClsfr_SetClsfrType (pCtrlData->hTxDataQ, pParamInfo->content.ctrlDataClsfrType);
        break;

    case CTRL_DATA_CLSFR_CONFIG:
        txDataClsfr_InsertClsfrEntry(pCtrlData->hTxDataQ, &pParamInfo->content.ctrlDataClsfrInsertTable);
        break;

    case CTRL_DATA_CLSFR_REMOVE_ENTRY:
        txDataClsfr_RemoveClsfrEntry(pCtrlData->hTxDataQ, &pParamInfo->content.ctrlDataClsfrInsertTable);
       break;
       
    case CTRL_DATA_TOGGLE_TRAFFIC_INTENSITY_EVENTS:

            /* Enable or disable events according to flag */
            ctrlData_ToggleTrafficIntensityNotification (pCtrlData, (TI_BOOL)pParamInfo->content.ctrlDataTrafficIntensityEventsFlag);
        
        break;

    case CTRL_DATA_TRAFFIC_INTENSITY_THRESHOLD:
        {
            OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS *localParams = &pParamInfo->content.ctrlDataTrafficIntensityThresholds;
            TI_BOOL savedEnableFlag;   /* Used to save previous enable/disable flag - before stopping/starting events for change in params */
        
            /* If any of the parameters has changed, we need to re-register with the Traffic Monitor */
            if ((localParams->uHighThreshold != pCtrlData->ctrlDataTrafficIntensityThresholds.uHighThreshold) ||
                (localParams->uLowThreshold != pCtrlData->ctrlDataTrafficIntensityThresholds.uLowThreshold) ||
                (localParams->TestInterval != pCtrlData->ctrlDataTrafficIntensityThresholds.TestInterval))
            {

                os_memoryCopy(pCtrlData->hOs, &pCtrlData->ctrlDataTrafficIntensityThresholds,
                                localParams,
                                sizeof(OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS));

                savedEnableFlag = pCtrlData->ctrlDataTrafficIntensityEventsEnabled;

                /* Turn off traffic events */
                ctrlData_ToggleTrafficIntensityNotification (pCtrlData, TI_FALSE);

                /* Unregister current events */
                ctrlData_UnregisterTrafficIntensityEvents (pCtrlData);
                
                /* And re-register with new thresholds */
                ctrlData_RegisterTrafficIntensityEvents (pCtrlData);

                /* Enable events if they were enabled  */
                ctrlData_ToggleTrafficIntensityNotification (pCtrlData, savedEnableFlag);

            }
        }
        
        break;

    default:
TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " ctrlData_setParam() : PARAMETER NOT SUPPORTED \n");
        return (PARAM_NOT_SUPPORTED);
    }

    return (TI_OK);
}


/***************************************************************************
*                           selectRateTable                                *
****************************************************************************
* DESCRIPTION:  
*
* INPUTS:       hCtrlData - the object
*               
* OUTPUT:       
*
* RETURNS:      
***************************************************************************/

static void selectRateTable(TI_HANDLE hCtrlData, TI_UINT32 rateMask)
{
    paramInfo_t param;
    ERate       rate;
    TI_BOOL     b11nEnable;
    
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;

    rate = rate_GetMaxFromDrvBitmap (rateMask);

    param.paramType = SITE_MGR_OPERATIONAL_MODE_PARAM;
    siteMgr_getParam(pCtrlData->hSiteMgr, &param);

    switch(param.content.siteMgrDot11OperationalMode)
    {
        case DOT11_B_MODE:
            pCtrlData->uCurrPolicyEnabledRatesMask = pCtrlData->policyEnabledRatesMaskCck;
            break;

        case DOT11_G_MODE:
            if( (rate == DRV_RATE_11M) || 
                (rate == DRV_RATE_5_5M)|| 
                (rate == DRV_RATE_2M)  || 
                (rate == DRV_RATE_1M)    )
            {
                pCtrlData->uCurrPolicyEnabledRatesMask = pCtrlData->policyEnabledRatesMaskCck;
            }
            else
            {
               pCtrlData->uCurrPolicyEnabledRatesMask = pCtrlData->policyEnabledRatesMaskOfdm;
            }
            break;

        case DOT11_A_MODE:
            pCtrlData->uCurrPolicyEnabledRatesMask = pCtrlData->policyEnabledRatesMaskOfdmA;
            break;

        case DOT11_DUAL_MODE:
        case DOT11_MAX_MODE:
        case DOT11_N_MODE:
            TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " uCurrPolicyEnabledRatesMask not configured !!!");
            break;
    }

    /* add HT MCS rates */
    StaCap_IsHtEnable (pCtrlData->hStaCap, &b11nEnable);
    if (b11nEnable == TI_TRUE)
    {
        if ((rate == DRV_RATE_MCS_0) |
            (rate == DRV_RATE_MCS_1) |
            (rate == DRV_RATE_MCS_2) |
            (rate == DRV_RATE_MCS_3) |
            (rate == DRV_RATE_MCS_4) |
            (rate == DRV_RATE_MCS_5) |
            (rate == DRV_RATE_MCS_6) |
            (rate == DRV_RATE_MCS_7))
        {
            pCtrlData->uCurrPolicyEnabledRatesMask = pCtrlData->policyEnabledRatesMaskOfdmN;
        }
    }
}


/***************************************************************************
*                           ctrlData_stop                                  *
****************************************************************************
* DESCRIPTION:  This function stop the link control algorithms. 
*
* INPUTS:       hCtrlData - the object
*       
* OUTPUT:       
* 
* RETURNS:      TI_OK
*               TI_NOK
***************************************************************************/

TI_STATUS ctrlData_stop(TI_HANDLE hCtrlData)    
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;

    /* set Preamble length option to default value*/
    pCtrlData->ctrlDataCurrentPreambleType = DEF_CURRENT_PREAMBLE;

    os_memoryZero(pCtrlData->hOs, 
                  &pCtrlData->tsrsParameters, 
                  sizeof(pCtrlData->tsrsParameters));

    TRACE0(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, " ctrlData_stop() : Link control algorithms stoped \n");

    return TI_OK;
}


/***************************************************************************
*						ctrlData_updateTxRateAttributes		               *
****************************************************************************
* DESCRIPTION:	This function updates the TxCtrl module with all Tx rate attributes 
*				  whenever any of them changes.
*				It is called from ctrlData_setParam() after any rate param update.
***************************************************************************/
void ctrlData_updateTxRateAttributes(TI_HANDLE hCtrlData)	
{
	ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
	TI_UINT8    ac;

	/* For each AC, get current Tx-rate policy for Data and for Mgmt packets and update the TxCtrl module. */
	for (ac = 0; ac < MAX_NUM_OF_AC; ac++) 
	{
		txCtrlParams_updateMgmtRateAttributes(pCtrlData->hTxCtrl, pCtrlData->uMgmtPolicyId, ac);
		txCtrlParams_updateDataRateAttributes(pCtrlData->hTxCtrl, pCtrlData->tsrsParameters[ac].fwPolicyID, ac);
	}
}

/***************************************************************************
*                   ctrlData_getCurrBssTypeAndCurrBssId                    *
****************************************************************************
* DESCRIPTION:  This function return the current BSSID and the 
*               current BSS Type
*
* INPUTS:       hCtrlData - the object
*               
* OUTPUT:       pCurrBssid - pointer to return the current bssid    
*               pCurrBssType - pointer to return the current bss type 
*
* RETURNS:      void
***************************************************************************/
void ctrlData_getCurrBssTypeAndCurrBssId(TI_HANDLE hCtrlData, TMacAddr *pCurrBssid, 
                                           ScanBssType_e *pCurrBssType)
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    
	MAC_COPY (*pCurrBssid, pCtrlData->ctrlDataCurrentBSSID);
    *pCurrBssType = pCtrlData->ctrlDataCurrentBssType;

}


/*-----------------------------------------------------------------------------
Routine Name: ctrlData_ToggleTrafficIntensityNotification
Routine Description: turns ON/OFF traffic intensity notification events
                     from Traffic Monitor module
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
void ctrlData_ToggleTrafficIntensityNotification (TI_HANDLE hCtrlData, TI_BOOL enabledFlag)
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    TI_UINT8 idx;

   if (enabledFlag)
   {
      for (idx=0; idx < CTRL_DATA_TRAFFIC_INTENSITY_MAX_EVENTS; idx++)
      {
         TrafficMonitor_StartEventNotif (pCtrlData->hTrafficMonitor,pCtrlData->ctrlDataTrafficThresholdEvents[idx]);
      }
      TRACE0(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "ctrlData_ToggleTrafficIntensityNotification (TI_TRUE)\n");
   }
   else
   {
      for (idx=0; idx < CTRL_DATA_TRAFFIC_INTENSITY_MAX_EVENTS; idx++)
      {
         TrafficMonitor_StopEventNotif (pCtrlData->hTrafficMonitor,pCtrlData->ctrlDataTrafficThresholdEvents[idx]);
      }
      TRACE0(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "ctrlData_ToggleTrafficIntensityNotification (TI_FALSE)\n");
   }
   pCtrlData->ctrlDataTrafficIntensityEventsEnabled = enabledFlag;

}

/*-----------------------------------------------------------------------------
Routine Name: ctrlData_UnregisterTrafficIntensityEvents
Routine Description: unregisters existing events from traffic monitor
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
static void ctrlData_UnregisterTrafficIntensityEvents (TI_HANDLE hCtrlData)
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    TI_UINT8 idx;

    /* Loop through events and unregister them */
    for (idx=0; idx < CTRL_DATA_TRAFFIC_INTENSITY_MAX_EVENTS; idx++)
    {
       TrafficMonitor_UnregEvent (pCtrlData->hTrafficMonitor,pCtrlData->ctrlDataTrafficThresholdEvents[idx]);
    }

    TRACE0(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, "ctrlData_UnregisterTrafficIntensityEvents: Unregistered all events\n");

}


/*-----------------------------------------------------------------------------
Routine Name: ctrlData_RegisterTrafficIntensityEvents
Routine Description: Registers traffic intensity threshold events through traffic monitor
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
static void ctrlData_RegisterTrafficIntensityEvents (TI_HANDLE hCtrlData)
{
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    TrafficAlertRegParm_t TrafficAlertRegParm;
    TI_STATUS status;

    /* Register high threshold "direction up" event */
    TrafficAlertRegParm.CallBack = ctrlData_TrafficThresholdCrossed;
    TrafficAlertRegParm.Context = hCtrlData; 
    TrafficAlertRegParm.Cookie =  CTRL_DATA_TRAFFIC_INTENSITY_HIGH_CROSSED_ABOVE;    
    TrafficAlertRegParm.Direction = TRAFF_UP;
    TrafficAlertRegParm.Trigger = TRAFF_EDGE;
    TrafficAlertRegParm.TimeIntervalMs = pCtrlData->ctrlDataTrafficIntensityThresholds.TestInterval;
    TrafficAlertRegParm.Threshold = pCtrlData->ctrlDataTrafficIntensityThresholds.uHighThreshold;
    TrafficAlertRegParm.MonitorType = TX_RX_DIRECTED_FRAMES;
    pCtrlData->ctrlDataTrafficThresholdEvents[0] = TrafficMonitor_RegEvent(pCtrlData->hTrafficMonitor,&TrafficAlertRegParm,TI_FALSE); 

    if (pCtrlData->ctrlDataTrafficThresholdEvents[0] == NULL)
    {
         TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " ctrlData_RegisterTrafficIntensityEvents() : Failed to register high treshold event (TRAFF_UP) \n");
         return;
    }

    /* Register high threshold "direction down" event*/
    TrafficAlertRegParm.Cookie =  CTRL_DATA_TRAFFIC_INTENSITY_HIGH_CROSSED_BELOW;    
    TrafficAlertRegParm.Direction = TRAFF_DOWN;
    TrafficAlertRegParm.Trigger = TRAFF_EDGE;
    TrafficAlertRegParm.Threshold = pCtrlData->ctrlDataTrafficIntensityThresholds.uHighThreshold;
    pCtrlData->ctrlDataTrafficThresholdEvents[1] = TrafficMonitor_RegEvent(pCtrlData->hTrafficMonitor,&TrafficAlertRegParm,TI_FALSE); 

    if (pCtrlData->ctrlDataTrafficThresholdEvents[1] == NULL)
    {
         TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " ctrlData_RegisterTrafficIntensityEvents() : Failed to register high treshold event (TRAFF_DOWN) \n");
         return;
    }

    /* Define the "direction below" and "direction above" events as opposites (events that reset eachother)*/
    status = TrafficMonitor_SetRstCondition(pCtrlData->hTrafficMonitor,
                                            pCtrlData->ctrlDataTrafficThresholdEvents[0],
                                            pCtrlData->ctrlDataTrafficThresholdEvents[1],
                                            TI_TRUE);

    if (status != TI_OK)
    {
      TRACE1(pCtrlData->hReport, REPORT_SEVERITY_ERROR , "ctrlData_RegisterTrafficIntensityEvents: TrafficMonitor_SetRstCondition returned status = %d\n",status);
    }

    /* Register low threshold "direction up" event */
    TrafficAlertRegParm.Cookie =  CTRL_DATA_TRAFFIC_INTENSITY_LOW_CROSSED_ABOVE;    
    TrafficAlertRegParm.Direction = TRAFF_UP;
    TrafficAlertRegParm.Trigger = TRAFF_EDGE;
    TrafficAlertRegParm.Threshold = pCtrlData->ctrlDataTrafficIntensityThresholds.uLowThreshold;
    pCtrlData->ctrlDataTrafficThresholdEvents[2] = TrafficMonitor_RegEvent(pCtrlData->hTrafficMonitor,&TrafficAlertRegParm,TI_FALSE); 

    if (pCtrlData->ctrlDataTrafficThresholdEvents[2] == NULL)
    {
         TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " ctrlData_RegisterTrafficIntensityEvents() : Failed to register low treshold event (TRAFF_UP) \n");
         return;
    }

    /* Register low threshold "direction below" event */
    TrafficAlertRegParm.Cookie =  CTRL_DATA_TRAFFIC_INTENSITY_LOW_CROSSED_BELOW;
    TrafficAlertRegParm.Direction = TRAFF_DOWN;
    TrafficAlertRegParm.Trigger = TRAFF_EDGE;
    TrafficAlertRegParm.Threshold = pCtrlData->ctrlDataTrafficIntensityThresholds.uLowThreshold;
    pCtrlData->ctrlDataTrafficThresholdEvents[3] = TrafficMonitor_RegEvent(pCtrlData->hTrafficMonitor,&TrafficAlertRegParm,TI_FALSE); 

    if (pCtrlData->ctrlDataTrafficThresholdEvents[3] == NULL)
    {
         TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " ctrlData_RegisterTrafficIntensityEvents() : Failed to register low treshold event (TRAFF_DOWN) \n");
         return;
    }

    /* Define the "direction below" and "direction above" events as opposites (events that reset eachother)*/
    status = TrafficMonitor_SetRstCondition(pCtrlData->hTrafficMonitor,
                                            pCtrlData->ctrlDataTrafficThresholdEvents[2],
                                            pCtrlData->ctrlDataTrafficThresholdEvents[3],
                                            TI_TRUE);

    if (status != TI_OK)
    {
      TRACE1(pCtrlData->hReport, REPORT_SEVERITY_ERROR , "ctrlData_RegisterTrafficIntensityEvents: TrafficMonitor_SetRstCondition returned status = %d\n",status);
    }
  
    TRACE0(pCtrlData->hReport, REPORT_SEVERITY_INFORMATION, " ctrlData_RegisterTrafficIntensityEvents() : finished registering all events \n");

}


/*-----------------------------------------------------------------------------
Routine Name: ctrlData_TrafficThresholdCrossed
Routine Description: called whenever traffic intensity threshold is crossed. 
                     notifies event handler to send appropriate event with threshold parameters.
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
static void ctrlData_TrafficThresholdCrossed(TI_HANDLE Context,TI_UINT32 Cookie)
{
    ctrlData_t *pCtrlData = (ctrlData_t *)Context;
    trafficIntensityThresholdCross_t crossInfo;

    switch(Cookie) 
    {
    case CTRL_DATA_TRAFFIC_INTENSITY_HIGH_CROSSED_ABOVE:
            crossInfo.thresholdCross = (TI_UINT32)HIGH_THRESHOLD_CROSS;
            crossInfo.thresholdCrossDirection = (TI_UINT32)CROSS_ABOVE;
            EvHandlerSendEvent(pCtrlData->hEvHandler, IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED, (TI_UINT8 *)&crossInfo,sizeof(trafficIntensityThresholdCross_t));
       break;

    case CTRL_DATA_TRAFFIC_INTENSITY_HIGH_CROSSED_BELOW:
            crossInfo.thresholdCross = (TI_UINT32)HIGH_THRESHOLD_CROSS;
            crossInfo.thresholdCrossDirection = (TI_UINT32)CROSS_BELOW;
            EvHandlerSendEvent(pCtrlData->hEvHandler, IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED, (TI_UINT8 *)&crossInfo,sizeof(trafficIntensityThresholdCross_t));
       break;

    case CTRL_DATA_TRAFFIC_INTENSITY_LOW_CROSSED_ABOVE:
            crossInfo.thresholdCross = (TI_UINT32)LOW_THRESHOLD_CROSS;
            crossInfo.thresholdCrossDirection = (TI_UINT32)CROSS_ABOVE;
            EvHandlerSendEvent(pCtrlData->hEvHandler, IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED, (TI_UINT8 *)&crossInfo,sizeof(trafficIntensityThresholdCross_t));
       break;

    case CTRL_DATA_TRAFFIC_INTENSITY_LOW_CROSSED_BELOW:
            crossInfo.thresholdCross = (TI_UINT32)LOW_THRESHOLD_CROSS;
            crossInfo.thresholdCrossDirection = (TI_UINT32)CROSS_BELOW;
            EvHandlerSendEvent(pCtrlData->hEvHandler, IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED, (TI_UINT8 *)&crossInfo,sizeof(trafficIntensityThresholdCross_t));
       break;
    default:
         TRACE0(pCtrlData->hReport, REPORT_SEVERITY_ERROR, " ctrlData_TrafficThresholdCrossed() : Unknown cookie received from traffic monitor !!! \n");
       break;
   }
    
}

/*************************************************************************
 *                                                                       *
 *                          DEBUG FUNCTIONS                              *
 *                                                                       *
 *************************************************************************/

#ifdef TI_DBG

void ctrlData_printTxParameters(TI_HANDLE hCtrlData)
{
#ifdef REPORT_LOG
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;

    WLAN_OS_REPORT(("            Tx Parameters            \n"));
    WLAN_OS_REPORT(("-------------------------------------\n"));
    WLAN_OS_REPORT(("currentPreamble                     = %d\n\n",pCtrlData->ctrlDataCurrentPreambleType));
    WLAN_OS_REPORT(("ctrlDataCurrentRateMask             = 0x%X\n",pCtrlData->ctrlDataCurrentRateMask));
#endif
}  


void ctrlData_printCtrlBlock(TI_HANDLE hCtrlData)
{
#ifdef REPORT_LOG
    ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
    TI_UINT32  i;

    WLAN_OS_REPORT(("    CtrlData BLock    \n"));
    WLAN_OS_REPORT(("----------------------\n"));

    WLAN_OS_REPORT(("hSiteMgr = 0x%X\n",pCtrlData->hSiteMgr));
    WLAN_OS_REPORT(("hTWD = 0x%X\n",pCtrlData->hTWD));
    WLAN_OS_REPORT(("hOs = 0x%X\n",pCtrlData->hOs));
    WLAN_OS_REPORT(("hReport = 0x%X\n",pCtrlData->hReport));

    WLAN_OS_REPORT(("ctrlDataDeviceMacAddress = 0x%X.0x%X.0x%X.0x%X.0x%X.0x%X. \n", pCtrlData->ctrlDataDeviceMacAddress[0],
                                                                                    pCtrlData->ctrlDataDeviceMacAddress[1],
                                                                                    pCtrlData->ctrlDataDeviceMacAddress[2],
                                                                                    pCtrlData->ctrlDataDeviceMacAddress[3],
                                                                                    pCtrlData->ctrlDataDeviceMacAddress[4],
                                                                                    pCtrlData->ctrlDataDeviceMacAddress[5]));

    WLAN_OS_REPORT(("ctrlDataCurrentBSSID = 0x%X.0x%X.0x%X.0x%X.0x%X.0x%X. \n", pCtrlData->ctrlDataCurrentBSSID[0],
                                                                                pCtrlData->ctrlDataCurrentBSSID[1],
                                                                                pCtrlData->ctrlDataCurrentBSSID[2],
                                                                                pCtrlData->ctrlDataCurrentBSSID[3],
                                                                                pCtrlData->ctrlDataCurrentBSSID[4],
                                                                                pCtrlData->ctrlDataCurrentBSSID[5]));

    WLAN_OS_REPORT(("ctrlDataCurrentBssType      = %d\n", pCtrlData->ctrlDataCurrentBssType));
    WLAN_OS_REPORT(("ctrlDataCurrentRateMask     = 0x%X\n", pCtrlData->ctrlDataCurrentRateMask));
    WLAN_OS_REPORT(("ctrlDataCurrentPreambleType = %d\n", pCtrlData->ctrlDataCurrentPreambleType));

    WLAN_OS_REPORT(("Traffic Intensity threshold events status: %s\n", (pCtrlData->ctrlDataTrafficIntensityEventsEnabled ? "Enabled" : "Disabled")));
    WLAN_OS_REPORT(("Traffic Intensity high threshold: %d packets/sec \n", pCtrlData->ctrlDataTrafficIntensityThresholds.uHighThreshold));
    WLAN_OS_REPORT(("Traffic Intensity low threshold: %d packets/sec \n", pCtrlData->ctrlDataTrafficIntensityThresholds.uLowThreshold));
    WLAN_OS_REPORT(("Traffic Intensity test interval: %d ms\n", pCtrlData->ctrlDataTrafficIntensityThresholds.TestInterval));

    for (i=0; i < pCtrlData->ctrlDataTxRatePolicy.numOfRateClasses; i++)
    {
        WLAN_OS_REPORT(("Rate Enable/Disable Mask = 0x%x\n",
						pCtrlData->ctrlDataTxRatePolicy.rateClass[i].txEnabledRates));

        WLAN_OS_REPORT(("Long retry = %d, Short retry = %d\n",
						pCtrlData->ctrlDataTxRatePolicy.rateClass[i].longRetryLimit,
						pCtrlData->ctrlDataTxRatePolicy.rateClass[i].shortRetryLimit));
    }
#endif
}


#endif /*TI_DBG*/


/***************************************************************************
*                           ctrlData_storeTSRateSet                        
****************************************************************************
* DESCRIPTION:  This function translates TSRS rates into map of retransmissions
*               similar to predefined clients rates retransmissions, and stores
*               in the Ctrl database
*
* INPUTS:       pCtrlData - the object
*               acID
*               rates array
*
* RETURNS:      -
****************************************************************************/
static void ctrlData_storeTSRateSet(ctrlData_t *pCtrlData, TTxDataQosParams *tsrsParams)
{
    TI_UINT32 rateCount;
    TI_UINT32 acID;
    TI_UINT32 tsrsRequestedMap;
    ERate rateNumber;

    acID = tsrsParams->acID;
    os_memoryZero(pCtrlData->hOs, 
                  &(pCtrlData->tsrsParameters[acID]), 
                  sizeof(pCtrlData->tsrsParameters[acID]));

    tsrsRequestedMap = 0;

    for (rateCount = 0; rateCount < tsrsParams->tsrsArrLen; rateCount++) 
    {
        /* Erase Most significant bit in case it was raised to mark nominal PHY rates (& 0x7F) */
        /* Convert multiplication of 500kb/sec to ERate and then to ETxRateClassId */
        /* and update retransmission map in accordance to rate definitions */
        rateNumber = rate_NumberToDrv ((tsrsParams->tsrsArr[rateCount] & 0x7F) >> 1);

        pCtrlData->tsrsParameters[acID].policyClassRateMask = pCtrlData->uCurrPolicyEnabledRatesMask;

        tsrsRequestedMap |= 1 << (rateNumber - 1);
    }
    /* Update supportedRatesMask according to TSRS rates and rates supported */
    pCtrlData->tsrsParameters[acID].supportedRatesMask = tsrsRequestedMap;

    /* Check that Rate Fallback policy map is not empty; if this is the case, ignore pre-defined policy */
    if (pCtrlData->tsrsParameters[acID].policyClassRateMask == 0)
    {
        pCtrlData->tsrsParameters[acID].policyClassRateMask = pCtrlData->tsrsParameters[acID].supportedRatesMask;
    }
}




