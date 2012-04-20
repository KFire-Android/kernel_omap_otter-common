/*
 * TWDriverCtrl.c
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


/** \file  TWDriver.c 
 *  \brief TI WLAN Hardware Access Driver, Parameters control
 *
 *  \see   TWDriver.h 
 */

#define __FILE_ID__  FILE_ID_118
#include "TWDriver.h"
#include "tidef.h"
#include "report.h"
#include "txHwQueue_api.h"
#include "txXfer_api.h"
#include "txResult_api.h"
#include "eventMbox_api.h" 
#include "TWDriver.h"
#include "TWDriverInternal.h"
#include "FwEvent_api.h"
#include "CmdBld.h"
#include "RxQueue_api.h"


TI_STATUS TWD_SetParam (TI_HANDLE hTWD, TTwdParamInfo *pParamInfo)
{
    TTwd           *pTWD = (TTwd *)hTWD;
    TWlanParams    *pWlanParams = &DB_WLAN(pTWD->hCmdBld);

    TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_SetParam: paramType=0x%X\n", pParamInfo->paramType);

    switch (pParamInfo->paramType)
    {
        case TWD_RTS_THRESHOLD_PARAM_ID:

            if  (pParamInfo->content.halCtrlRtsThreshold > TWD_RTS_THRESHOLD_MAX)
            {
                TRACE1(pTWD->hReport, REPORT_SEVERITY_ERROR, "TWD########TWD_RTS_THRESHOLD_PARAM: Value out of permitted range 0x%x\n", pParamInfo->content.halCtrlRtsThreshold);
                return (PARAM_VALUE_NOT_VALID);
            }

            if (cmdBld_CfgRtsThreshold (pTWD->hCmdBld, pParamInfo->content.halCtrlRtsThreshold, NULL, NULL) == TI_OK)
            {
                TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD########TWD_RTS_THRESHOLD_PARAM 0x%x\n", pParamInfo->content.halCtrlRtsThreshold);
                pWlanParams->RtsThreshold = pParamInfo->content.halCtrlRtsThreshold;
            }
            break;

        case TWD_CTS_TO_SELF_PARAM_ID:
            return cmdBld_CfgCtsProtection (pTWD->hCmdBld, pParamInfo->content.halCtrlCtsToSelf, NULL, NULL);

        case TWD_RX_TIME_OUT_PARAM_ID:
            if (cmdBld_CfgServicePeriodTimeout (pTWD->hCmdBld, &pParamInfo->content.halCtrlRxTimeOut, NULL, NULL) == TI_OK)
            {
                pWlanParams->rxTimeOut.psPoll = pParamInfo->content.halCtrlRxTimeOut.psPoll;
                pWlanParams->rxTimeOut.UPSD   = pParamInfo->content.halCtrlRxTimeOut.UPSD;  
            }
            break;

        case TWD_FRAG_THRESHOLD_PARAM_ID:
            if (pParamInfo->content.halCtrlFragThreshold < TWD_FRAG_THRESHOLD_MIN ||
                pParamInfo->content.halCtrlFragThreshold > TWD_FRAG_THRESHOLD_MAX)
                return PARAM_VALUE_NOT_VALID;

            cmdBld_CfgFragmentThreshold (pTWD->hCmdBld, pParamInfo->content.halCtrlFragThreshold, NULL, NULL);
            break;

        case TWD_MAX_RX_MSDU_LIFE_TIME_PARAM_ID:
            cmdBld_CfgRxMsduLifeTime (pTWD->hCmdBld, pParamInfo->content.halCtrlMaxRxMsduLifetime, NULL, NULL);
            break;

        case TWD_ACX_STATISTICS_PARAM_ID:
            if (cmdBld_CfgStatisitics (pTWD->hCmdBld, NULL, NULL) != TI_OK)
                return TI_NOK;
            break;

        case TWD_LISTEN_INTERVAL_PARAM_ID:
            if (pParamInfo->content.halCtrlListenInterval < TWD_LISTEN_INTERVAL_MIN ||
                pParamInfo->content.halCtrlListenInterval > TWD_LISTEN_INTERVAL_MAX)
                return PARAM_VALUE_NOT_VALID;

            pWlanParams->ListenInterval = (TI_UINT8)pParamInfo->content.halCtrlListenInterval;
            break;

        case TWD_AID_PARAM_ID:
            pWlanParams->Aid = pParamInfo->content.halCtrlAid;

            /* Configure the ACXAID info element*/
            if (cmdBld_CfgAid (pTWD->hCmdBld, pParamInfo->content.halCtrlAid, NULL, NULL) != TI_OK)
               return TI_NOK;
            break;

        case TWD_RSN_HW_ENC_DEC_ENABLE_PARAM_ID:
            TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD########HW_ENC_DEC_ENABLE %d\n", pParamInfo->content.rsnHwEncDecrEnable);

            /* Set the Encryption/Decryption on the HW*/
            if (cmdBld_CfgHwEncDecEnable (pTWD->hCmdBld, pParamInfo->content.rsnHwEncDecrEnable, NULL, NULL) != TI_OK)
                return (TI_NOK);
            break;

        case TWD_RSN_KEY_ADD_PARAM_ID:
            TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD########KEY_ADD\n");

            if (cmdBld_CmdAddKey (pTWD->hCmdBld,
                                  (TSecurityKeys *) pParamInfo->content.configureCmdCBParams.pCb,
                                  TI_FALSE,
                                  pParamInfo->content.configureCmdCBParams.fCb, 
                                  pParamInfo->content.configureCmdCBParams.hCb) != TI_OK)
                return TI_NOK;
            break;

        case TWD_RSN_KEY_REMOVE_PARAM_ID:
            TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD########KEY_REMOVE\n");

            if (cmdBld_CmdRemoveKey (pTWD->hCmdBld, 
                                     (TSecurityKeys *) pParamInfo->content.configureCmdCBParams.pCb,
                                     pParamInfo->content.configureCmdCBParams.fCb, 
                                     pParamInfo->content.configureCmdCBParams.hCb) != TI_OK)
                return TI_NOK;
            break;

        case TWD_RSN_DEFAULT_KEY_ID_PARAM_ID:
            if (*((TI_UINT8 *)pParamInfo->content.configureCmdCBParams.pCb) > MAX_DEFAULT_KEY_ID)
                return PARAM_VALUE_NOT_VALID;

            TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD########DEFAULT_KEY_ID %d\n", (TI_UINT8)pParamInfo->content.rsnDefaultKeyID);

			if (cmdBld_CmdSetWepDefaultKeyId (pTWD->hCmdBld,
									  *((TI_UINT8 *)pParamInfo->content.interogateCmdCBParams.pCb),
									  pParamInfo->content.interogateCmdCBParams.fCb, 
									  pParamInfo->content.interogateCmdCBParams.hCb) != TI_OK) 
                return TI_NOK;

            break;

        case TWD_RSN_SECURITY_MODE_PARAM_ID:
            TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD########SECURITY_MODE_SET %d\n", pParamInfo->content.rsnEncryptionStatus);
            if (cmdBld_CfgSecureMode (pTWD->hCmdBld, (ECipherSuite)pParamInfo->content.rsnEncryptionStatus, NULL, NULL) != TI_OK)
                return TI_NOK;
            break;

#ifdef XCC_MODULE_INCLUDED
        case TWD_RSN_XCC_SW_ENC_ENABLE_PARAM_ID:
        
            TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD: XCC_SW_ENC_ENABLE %d\n", pParamInfo->content.rsnXCCSwEncFlag);

            /* when SW encryption is ON, HW encryption should be turned OFF and vice versa */

            TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD: Set HwEncDecrEnable to %d\n", !pParamInfo->content.rsnXCCSwEncFlag);

            /* Set the Encryption/Decryption on the HW*/
            if (cmdBld_CfgHwEncDecEnable (pTWD->hCmdBld, !pParamInfo->content.rsnXCCSwEncFlag, NULL, NULL) != TI_OK)
                return TI_NOK;
            break;
             /* not supported - CKIP*/
        case TWD_RSN_XCC_MIC_FIELD_ENABLE_PARAM_ID:
            break;
#endif /* XCC_MODULE_INCLUDED*/

        case TWD_TX_POWER_PARAM_ID:

            TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD_TX_POWER_PARAM_ID %d\n", pParamInfo->content.halCtrlTxPowerDbm);

            pWlanParams->TxPowerDbm = pParamInfo->content.halCtrlTxPowerDbm;

            /* Configure the wlan hardware */
            if (cmdBld_CfgTxPowerDbm (pTWD->hCmdBld, pWlanParams->TxPowerDbm, NULL, NULL) != TI_OK)
                return TI_NOK;
            break;

        case TWD_SG_ENABLE_PARAM_ID:
            return cmdBld_CfgSgEnable (pTWD->hCmdBld, 
                                       (ESoftGeminiEnableModes)pParamInfo->content.SoftGeminiEnable, 
                                       NULL, 
                                       NULL);

        case TWD_SG_CONFIG_PARAM_ID:
            return cmdBld_CfgSg (pTWD->hCmdBld, &pParamInfo->content.SoftGeminiParam, NULL, NULL);

        case TWD_FM_COEX_PARAM_ID:
            return cmdBld_CfgFmCoex (pTWD->hCmdBld, &pParamInfo->content.tFmCoexParams, NULL, NULL);

        /*
         *  TX Parameters 
         */ 

        case TWD_TX_RATE_CLASS_PARAM_ID:
            return cmdBld_CfgTxRatePolicy (pTWD->hCmdBld, pParamInfo->content.pTxRatePlicy, NULL, NULL);

        case TWD_QUEUES_PARAM_ID:
            return cmdBld_CfgTid (pTWD->hCmdBld, pParamInfo->content.pQueueTrafficParams, NULL, NULL);

        case TWD_CLK_RUN_ENABLE_PARAM_ID:
            TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION, "TWD_SetParam: CLK_RUN_ENABLE %d\n", pParamInfo->content.halCtrlClkRunEnable);

            /* Set the Encryption/Decryption on the HW*/
            if (cmdBld_CfgClkRun (pTWD->hCmdBld, pParamInfo->content.halCtrlClkRunEnable, NULL, NULL) != TI_OK)
                return TI_NOK;
            break;

        case TWD_COEX_ACTIVITY_PARAM_ID:
            cmdBld_CfgCoexActivity (pTWD->hCmdBld, &pParamInfo->content.tTwdParamsCoexActivity, NULL, NULL);
            break;

        case TWD_DCO_ITRIM_PARAMS_ID:
            cmdBld_CfgDcoItrimParams (pTWD->hCmdBld, pParamInfo->content.tDcoItrimParams.enable, 
                                      pParamInfo->content.tDcoItrimParams.moderationTimeoutUsec, NULL, NULL);
            break;

        default:
            TRACE1(pTWD->hReport, REPORT_SEVERITY_ERROR, "TWD_SetParam - ERROR - Param is not supported, 0x%x\n", pParamInfo->paramType);
            return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}

TI_STATUS TWD_GetParam (TI_HANDLE hTWD, TTwdParamInfo *pParamInfo)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_GetParam: called\n");

    return cmdBld_GetParam (pTWD->hCmdBld, pParamInfo);
}

TI_STATUS TWD_CfgRx (TI_HANDLE hTWD, TI_UINT32 uRxConfigOption, TI_UINT32 uRxFilterOption)
{
    TTwd     *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgRx: called\n");

    return cmdBld_CfgRx (pTWD->hCmdBld, uRxConfigOption, uRxFilterOption, NULL, NULL);
}

TI_STATUS TWD_CfgArpIpAddrTable (TI_HANDLE hTWD, TIpAddr tIpAddr, EArpFilterType filterType, EIpVer eIpVer)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgArpIpAddrTable: called\n");

    return cmdBld_CfgArpIpAddrTable (pTWD->hCmdBld, tIpAddr, (TI_UINT8)filterType, eIpVer, NULL, NULL);
}

/** @ingroup Misc
 * \brief  Configure ARP IP Filter
 * 
 * \param hTWD 			- TWD module object handle
 * \param  bEnabled   	- Indicates if ARP filtering is Enabled (1) or Disabled (0)
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgArpIpFilter (TI_HANDLE hTWD, TIpAddr staIp)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgArpIpFilter: called\n");

    return cmdBld_CfgArpIpFilter (pTWD->hCmdBld, staIp, NULL, NULL);
}

TI_STATUS TWD_CmdSetSplitScanTimeOut  ( TI_HANDLE hTWD, TI_UINT32 uTimeOut )
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdSetSplitScanTimeOut: called\n");

    return cmdBld_CmdSetSplitScanTimeOut (pTWD->hCmdBld, uTimeOut);
}

TI_STATUS TWD_CmdJoinBss (TI_HANDLE hTWD, TJoinBss *pJoinBssParams)
{
    TTwd      *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdJoinBss: called\n");

    return cmdBld_CmdJoinBss (pTWD->hCmdBld, pJoinBssParams, NULL, NULL);
}

TI_STATUS TWD_CfgKeepAlive (TI_HANDLE hTWD, TKeepAliveParams *pKeepAliveParams)
{
    TTwd      *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgKeepAlive: called\n");

    return cmdBld_CfgKeepAlive (pTWD->hCmdBld, pKeepAliveParams, NULL, NULL);
}

TI_STATUS TWD_CfgKeepAliveEnaDis(TI_HANDLE hTWD, TI_UINT8 enaDisFlag)
{
    TTwd      *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgKeepAliveEnaDis: called\n");

    return cmdBld_CfgKeepAliveEnaDis (pTWD->hCmdBld, enaDisFlag, NULL, NULL);
}

TI_STATUS TWD_CmdTemplate (TI_HANDLE hTWD, TSetTemplate *pTemplateParams, void *fCb, TI_HANDLE hCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdTemplate: called\n");

    return cmdBld_CmdTemplate (pTWD->hCmdBld, pTemplateParams, fCb, hCb);
}

TI_STATUS TWD_CfgSlotTime (TI_HANDLE hTWD, ESlotTime eSlotTimeVal)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgSlotTime: called\n");

    return cmdBld_CfgSlotTime (pTWD->hCmdBld, eSlotTimeVal, NULL, NULL);
}

TI_STATUS TWD_CfgPreamble (TI_HANDLE hTWD, EPreamble ePreamble)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgPreamble: called\n");

    return cmdBld_CfgPreamble (pTWD->hCmdBld, (Preamble_e)ePreamble, NULL, NULL);
}

TI_STATUS TWD_CfgPacketDetectionThreshold (TI_HANDLE hTWD, TI_UINT32 threshold)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgPacketDetectionThreshold: called\n");

    return cmdBld_CfgPacketDetectionThreshold (pTWD->hCmdBld, threshold, NULL, NULL);
}

TI_STATUS TWD_CmdDisableTx (TI_HANDLE hTWD)
{
    TTwd   *pTWD = (TTwd *)hTWD;
    
    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdDisableTx: called\n");

    return cmdBld_CmdDisableTx (pTWD->hCmdBld, NULL, NULL);
}
    
TI_STATUS TWD_CmdEnableTx (TI_HANDLE hTWD, TI_UINT8 channel)
{
    TTwd   *pTWD = (TTwd *)hTWD;
        
    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdEnableTx: called\n");

    return cmdBld_CmdEnableTx (pTWD->hCmdBld, channel, NULL, NULL);
}

TI_STATUS TWD_CmdSetStaState (TI_HANDLE hTWD, TI_UINT8 staState, void *fCb, TI_HANDLE hCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE1(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_SetStaState: %d\n", staState);

    return cmdBld_CmdSetStaState (pTWD->hCmdBld, staState, fCb, hCb);
}

TI_STATUS TWD_ItrRoammingStatisitics (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void * pCb)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_ItrRoammingStatisitics: called\n");

    return cmdBld_ItrRoamimgStatisitics (pTWD->hCmdBld, fCb, hCb, pCb);
}

/** @ingroup UnKnown
 * \brief	Interrogate Error Count
 * 
 * \param  hTWD     	- TWD module object handle
 * \param  fCb          - Pointer to Command CB Function
 * \param  hCb          - Handle to Command CB Function Obj Parameters
 * \param  pCb          - Pointer to read parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Interrogate ACX Error counter
 * 
 * \sa
 */ 
TI_STATUS TWD_ItrErrorCnt (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_ItrErrorCnt: called\n");

    return cmdBld_ItrErrorCnt (pTWD->hCmdBld, fCb, hCb, pCb);
}

TI_STATUS TWD_CmdNoiseHistogram (TI_HANDLE hTWD, TNoiseHistogram *pNoiseHistParams)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdNoiseHistogram: called\n");

    return cmdBld_CmdNoiseHistogram (pTWD->hCmdBld, pNoiseHistParams, NULL, NULL);
}

TI_STATUS TWD_CfgBeaconFilterOpt (TI_HANDLE hTWD, TI_UINT8 uBeaconFilteringStatus, TI_UINT8 uNumOfBeaconsToBuffer)
{
    TTwd   *pTWD = (TTwd *)hTWD;
       
    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgBeaconFilterOpt: called\n");

    return cmdBld_CfgBeaconFilterOpt (pTWD->hCmdBld, uBeaconFilteringStatus, uNumOfBeaconsToBuffer, NULL, NULL);
}

TI_STATUS TWD_SetRateMngDebug(TI_HANDLE hTWD, RateMangeParams_t *pRateMngParams)
{
  TTwd   *pTWD = (TTwd *)hTWD;

   TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_SetRateMngDebug: called\n");

   return cmdBld_CfgRateMngDbg (pTWD->hCmdBld, pRateMngParams, NULL, NULL);
}

TI_STATUS TWD_CfgBeaconFilterTable (TI_HANDLE hTWD, TI_UINT8 uNumOfIe, TI_UINT8 *pIeTable, TI_UINT8 uIeTableSize)
{
    TTwd   *pTWD = (TTwd *)hTWD;
      
    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgBeaconFilterTable: called\n");

    return cmdBld_CfgBeaconFilterTable (pTWD->hCmdBld, uNumOfIe, pIeTable, uIeTableSize, NULL, NULL);
}

TI_STATUS TWD_CfgWakeUpCondition (TI_HANDLE hTWD, TPowerMgmtConfig *pPowerMgmtConfig)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgWakeUpCondition: called\n");

    return cmdBld_CfgWakeUpCondition (pTWD->hCmdBld, pPowerMgmtConfig, NULL, NULL);
}

TI_STATUS TWD_CfgBcnBrcOptions (TI_HANDLE hTWD, TPowerMgmtConfig *pPowerMgmtConfig)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgBcnBrcOptions: called\n");

    return cmdBld_CfgBcnBrcOptions (pTWD->hCmdBld, pPowerMgmtConfig, NULL, NULL);
}

TFwInfo * TWD_GetFWInfo (TI_HANDLE hTWD)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_GetFWInfo: called\n");

    return cmdBld_GetFWInfo (pTWD->hCmdBld);
}

TI_STATUS TWD_CmdSwitchChannel (TI_HANDLE hTWD, TSwitchChannelParams *pSwitchChannelCmd)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdSwitchChannel: called\n");

    return cmdBld_CmdSwitchChannel (pTWD->hCmdBld, pSwitchChannelCmd, NULL, NULL);
}

TI_STATUS TWD_CmdSwitchChannelCancel (TI_HANDLE hTWD, TI_UINT8 channel)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdSwitchChannelCancel: called\n");

    return cmdBld_CmdSwitchChannelCancel (pTWD->hCmdBld, channel, NULL, NULL);
}

TI_STATUS TWD_CfgMaxTxRetry (TI_HANDLE hTWD, TRroamingTriggerParams *pRoamingTriggerCmd)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgMaxTxRetry: called\n");

    return cmdBld_CfgMaxTxRetry (pTWD->hCmdBld, pRoamingTriggerCmd, NULL, NULL);
}

TI_STATUS TWD_CfgConnMonitParams (TI_HANDLE hTWD, TRroamingTriggerParams *pRoamingTriggerCmd)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgConnMonitParams: called\n");

    return cmdBld_CfgConnMonitParams (pTWD->hCmdBld, pRoamingTriggerCmd, NULL, NULL);
}

TI_STATUS TWD_ItrRSSI (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_ItrRSSI: called\n");

    return cmdBld_ItrRSSI (pTWD->hCmdBld, fCb, hCb, pCb);
}

TI_STATUS TWD_CmdFwDisconnect (TI_HANDLE hTWD, DisconnectType_e uDisconType, TI_UINT16 uDisconReason)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    return cmdBld_CmdFwDisconnect (pTWD->hCmdBld, 
                                   RX_CONFIG_OPTION_MY_DST_MY_BSS, 
                                   RX_FILTER_OPTION_FILTER_ALL, 
                                   uDisconType, 
                                   uDisconReason, 
                                   NULL, 
                                   NULL);
}

TI_STATUS TWD_CmdMeasurement (TI_HANDLE           hTWD, 
                              TMeasurementParams *pMeasurementParams,
                              void               *fCommandResponseCb, 
                              TI_HANDLE           hCb)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdMeasurement: called\n");

    return cmdBld_CmdMeasurement (pTWD->hCmdBld, pMeasurementParams, fCommandResponseCb, hCb);
}

TI_STATUS TWD_CmdMeasurementStop (TI_HANDLE hTWD, void* fCb, TI_HANDLE hCb)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdMeasurementStop: called\n");

    return cmdBld_CmdMeasurementStop (pTWD->hCmdBld, fCb, hCb);
}

TI_STATUS TWD_CmdApDiscovery (TI_HANDLE hTWD, TApDiscoveryParams *pApDiscoveryParams)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdApDiscovery: called\n");

    return cmdBld_CmdApDiscovery (pTWD->hCmdBld, pApDiscoveryParams, NULL, NULL);
}

TI_STATUS TWD_CmdApDiscoveryStop (TI_HANDLE hTWD)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdApDiscoveryStop: called\n");

    return cmdBld_CmdApDiscoveryStop (pTWD->hCmdBld, NULL, NULL);
}

TI_STATUS TWD_CfgGroupAddressTable (TI_HANDLE     hTWD, 
                                    TI_UINT8      uNumGroupAddrs, 
                                    TMacAddr      *pGroupAddr,
                                    TI_BOOL       bEnabled)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgGroupAddressTable: called\n");

    return cmdBld_CfgGroupAddressTable (pTWD->hCmdBld, uNumGroupAddrs, pGroupAddr, bEnabled, NULL, NULL);
}

TI_STATUS TWD_GetGroupAddressTable (TI_HANDLE hTWD, TI_UINT8* pEnabled, TI_UINT8* pNumGroupAddrs, TMacAddr *pGroupAddr)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_GetGroupAddressTable: called\n");

    return cmdBld_GetGroupAddressTable (pTWD->hCmdBld, pEnabled, pNumGroupAddrs, pGroupAddr);
}

TI_STATUS TWD_SetRadioBand (TI_HANDLE hTWD, ERadioBand eRadioBand)
{
    TTwd   *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_SetRadioBand: called\n");

    return cmdBld_SetRadioBand (pTWD->hCmdBld, eRadioBand);
}

TI_STATUS TWD_CfgSleepAuth (TI_HANDLE hTWD, EPowerPolicy eMinPowerPolicy)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgSleepAuth: called\n");

    /* Configure the new power policy to the FW */
    cmdBld_CfgSleepAuth (pTWD->hCmdBld, eMinPowerPolicy, NULL, NULL);

    return TI_OK;
}

TI_STATUS TWD_CfgBurstMode (TI_HANDLE hTWD, TI_BOOL bEnabled)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "cmdBld_CfgBurstMode: called\n");

    /* Configure the burst mode to the FW */
    cmdBld_CfgBurstMode (pTWD->hCmdBld, bEnabled, NULL, NULL);

    return TI_OK;
}



TI_STATUS TWD_CmdHealthCheck (TI_HANDLE hTWD)
{
    TTwd   *pTWD = (TTwd *)hTWD;
    
    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CmdHealthCheck: called\n");

    return cmdBld_CmdHealthCheck (pTWD->hCmdBld, NULL, NULL);
}

TI_STATUS TWD_CfgMacClock (TI_HANDLE hTWD, TI_UINT32 uMacClock)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgMacClock: called\n");

    return cmdBld_CfgMacClock (pTWD->hCmdBld, uMacClock, NULL, NULL);
}

TI_STATUS TWD_CfgArmClock (TI_HANDLE hTWD, TI_UINT32 uArmClock)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgArmClock: called\n");

    return cmdBld_CfgArmClock (pTWD->hCmdBld, uArmClock, NULL, NULL);
}

TI_STATUS TWD_ItrMemoryMap (TI_HANDLE hTWD, MemoryMap_t *pMap, void *fCb, TI_HANDLE hCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_ItrMemoryMap: called\n");

    return cmdBld_ItrMemoryMap (pTWD->hCmdBld, pMap, fCb, hCb);
}

TI_STATUS TWD_ItrStatistics (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_ItrStatistics: called\n");

    return cmdBld_ItrStatistics (pTWD->hCmdBld, fCb, hCb, pCb);
}

TI_STATUS TWD_ItrDataFilterStatistics (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_ItrDataFilterStatistics: called\n");

    return cmdBld_ItrDataFilterStatistics (pTWD->hCmdBld, fCb, hCb, pCb);
}

TI_STATUS TWD_CfgEnableRxDataFilter (TI_HANDLE hTWD, TI_BOOL bEnabled, filter_e eDefaultAction)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgEnableRxDataFilter: called\n");

    return cmdBld_CfgEnableRxDataFilter (pTWD->hCmdBld, bEnabled, eDefaultAction, NULL, NULL);
}

TI_STATUS TWD_CfgRxDataFilter (TI_HANDLE    hTWD, 
                               TI_UINT8     index, 
                               TI_UINT8     command, 
                               filter_e     eAction, 
                               TI_UINT8     uNumFieldPatterns, 
                               TI_UINT8     uLenFieldPatterns, 
                               TI_UINT8     *pFieldPatterns)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgRxDataFilter: called\n");

    return cmdBld_CfgRxDataFilter (pTWD->hCmdBld, 
                                   index, 
                                   command, 
                                   eAction, 
                                   uNumFieldPatterns, 
                                   uLenFieldPatterns, 
                                   pFieldPatterns, 
                                   NULL, 
                                   NULL);
}

TI_STATUS TWD_CfgRssiSnrTrigger (TI_HANDLE hTWD, RssiSnrTriggerCfg_t* pRssiSnrTrigger)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgRssiSnrTrigger: called\n");

    return cmdBld_CfgRssiSnrTrigger (pTWD->hCmdBld, pRssiSnrTrigger, NULL, NULL);
}

TI_STATUS TWD_CfgAcParams (TI_HANDLE hTWD, TAcQosParams *pAcQosParams, void *fCb, TI_HANDLE hCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgAcParams: called\n");

    return cmdBld_CfgAcParams (pTWD->hCmdBld, pAcQosParams, fCb, hCb);
}

TI_STATUS TWD_CfgPsRxStreaming (TI_HANDLE hTWD, TPsRxStreaming *pPsRxStreaming, void *fCb, TI_HANDLE hCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgPsRxStreaming: called\n");

    return cmdBld_CfgPsRxStreaming (pTWD->hCmdBld, pPsRxStreaming, fCb, hCb);
}

TI_STATUS TWD_CfgBet (TI_HANDLE hTWD, TI_UINT8 Enable, TI_UINT8 MaximumConsecutiveET)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgBet: called\n");

    return cmdBld_CfgBet (pTWD->hCmdBld, Enable, MaximumConsecutiveET, NULL, NULL);
}

TI_STATUS TWD_SetSecuritySeqNum (TI_HANDLE hTWD, TI_UINT8 securitySeqNumLsByte)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_SetSecuritySeqNum: called\n");

    return cmdBld_SetSecuritySeqNum (pTWD->hCmdBld, securitySeqNumLsByte);
}

TI_STATUS TWD_CfgSetFwHtCapabilities (TI_HANDLE hTWD, 
									  Tdot11HtCapabilitiesUnparse *pHtCapabilitiesIe, 
									  TI_BOOL bAllowHtOperation)
{
    TTwd        *pTWD           = (TTwd *)hTWD;
    TI_UINT32   uHtCapabilites;
    TI_UINT8    uAmpduMaxLeng = 0;
    TI_UINT8    uAmpduMinSpac = 0; 
    TI_UINT16   uHtCapabilitesField;

    /* Note, currently this value will be set to FFFFFFFFFFFF to indicate it is relevant for all peers 
       since we only support HT in infrastructure mode. Later on this field will be relevant to IBSS/DLS operation */
    TMacAddr    tMacAddress = {0xff,0xff,0xff,0xff,0xff,0xff};

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgSetFwHtCapabilities: called\n");

    /* Allow HT Operation ? */
    if (bAllowHtOperation == TI_TRUE)
    {
       /* get date from HT capabilities field */
       /* Handle endian for the field */
       COPY_WLAN_WORD(&uHtCapabilitesField, pHtCapabilitiesIe->aHtCapabilitiesIe);
       uHtCapabilites = FW_CAP_BIT_MASK_HT_OPERATION |
                        ((uHtCapabilitesField & HT_CAP_GREENFIELD_FRAME_FORMAT_BITMASK)    ? FW_CAP_BIT_MASK_GREENFIELD_FRAME_FORMAT       : 0) |
                        ((uHtCapabilitesField & HT_CAP_SHORT_GI_FOR_20MHZ_BITMASK)         ? FW_CAP_BIT_MASK_SHORT_GI_FOR_20MHZ_PACKETS    : 0) |
                        ((uHtCapabilitesField & HT_CAP_LSIG_TXOP_PROTECTION_BITMASK)       ? FW_CAP_BIT_MASK_LSIG_TXOP_PROTECTION          : 0);

       /* get date from HT capabilities field */
       uHtCapabilites |= ((uHtCapabilitesField & HT_EXT_HT_CONTROL_FIELDS_BITMASK) ? FW_CAP_BIT_MASK_HT_CONTROL_FIELDS : 0) |
                         ((uHtCapabilitesField & HT_EXT_RD_INITIATION_BITMASK)     ? FW_CAP_BIT_MASK_RD_INITIATION     : 0);

       /* get date from A-MPDU parameters field */
       uAmpduMaxLeng = pHtCapabilitiesIe->aHtCapabilitiesIe[HT_CAP_AMPDU_PARAMETERS_FIELD_OFFSET] & HT_CAP_AMPDU_MAX_RX_FACTOR_BITMASK;
       uAmpduMinSpac = (pHtCapabilitiesIe->aHtCapabilitiesIe[HT_CAP_AMPDU_PARAMETERS_FIELD_OFFSET] >> 2) & HT_CAP_AMPDU_MIN_START_SPACING_BITMASK;
    }
    /* not Allow HT Operation */
    else
    {
        uHtCapabilites = 0;
    }

    return cmdBld_CfgSetFwHtCapabilities (pTWD->hCmdBld,
                                          uHtCapabilites,
                                          tMacAddress,
                                          uAmpduMaxLeng,
                                          uAmpduMinSpac,
                                          NULL, 
                                          NULL);
}

TI_STATUS TWD_CfgSetFwHtInformation (TI_HANDLE hTWD, Tdot11HtInformationUnparse *pHtInformationIe)
{
    TTwd        *pTWD           = (TTwd *)hTWD;
    TI_UINT8    uRifsMode;
    TI_UINT8    uHtProtection; 
    TI_UINT8    uGfProtection;
    TI_UINT8    uHtTxBurstLimit;
    TI_UINT8    uDualCtsProtection;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgSetFwHtInformation: called\n");

    uRifsMode = (pHtInformationIe->aHtInformationIe[1] & HT_INF_RIFS_MOD_BITMASK) >> 3;

    uHtProtection = (pHtInformationIe->aHtInformationIe[2] & HT_INF_OPERATION_MOD_BITMASK);

    uGfProtection = (pHtInformationIe->aHtInformationIe[2] & HT_INF_NON_GF_PRES_BITMASK) >> 2; 

    uHtTxBurstLimit = 0; /* not in use */

    uDualCtsProtection = (pHtInformationIe->aHtInformationIe[4] & HT_INF_DUAL_CTS_PROTECTION_BITMASK) >> 7; 

    return cmdBld_CfgSetFwHtInformation (pTWD->hCmdBld,
                                         uRifsMode,
                                         uHtProtection,
                                         uGfProtection,
                                         uHtTxBurstLimit,
                                         uDualCtsProtection,
                                         NULL, 
                                         NULL);
}

TI_STATUS TWD_CfgSetBaInitiator (TI_HANDLE hTWD,
                                 TI_UINT8 uTid,
                                 TI_UINT8 uState,
                                 TMacAddr tRa,
                                 TI_UINT16 uWinSize,
                                 TI_UINT16 uInactivityTimeout)

{
	TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgSetBaInitiator: called\n");

    return cmdBld_CfgSetBaSession (pTWD->hCmdBld,
                                   ACX_BA_SESSION_INITIATOR_POLICY,
                                   uTid,               
                                   uState,             
                                   tRa,                
                                   uWinSize,          
                                   uInactivityTimeout,
                                   NULL, 
                                   NULL);
}

TI_STATUS TWD_CfgSetBaReceiver (TI_HANDLE hTWD,
                                TI_UINT8 uTid,
                                TI_UINT8 uState,
                                TMacAddr tRa,
                                TI_UINT16 uWinSize)
{
    TTwd *pTWD = (TTwd *)hTWD;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CfgSetBaReceiver: called\n");

    return cmdBld_CfgSetBaSession (pTWD->hCmdBld,
                                   ACX_BA_SESSION_RESPONDER_POLICY,
                                   uTid,               
                                   uState,             
                                   tRa,                
                                   uWinSize,          
                                   0,
                                   (void *)NULL, 
                                   (TI_HANDLE)NULL);
}

void TWD_CloseAllBaSessions(TI_HANDLE hTWD) 
{
    TTwd        *pTWD = (TTwd *)hTWD;
    TI_UINT32    i;

    TRACE0(pTWD->hReport, REPORT_SEVERITY_INFORMATION , "TWD_CloseAllBaSessions: called\n");

    /* close all BA sessions */
    for(i=0; i <MAX_NUM_OF_802_1d_TAGS; ++i)
    {
        RxQueue_CloseBaSession(pTWD->hRxQueue, i);
    }
}

ETxnStatus TWD_WdExpireEvent(TI_HANDLE hTWD)
{
    TTwd *pTWD = (TTwd*)hTWD;

	if ((pTWD->fFailureEventCb != NULL) && (pTWD->hFailureEventCb != NULL))
	{
        pTWD->fFailureEventCb(pTWD->hFailureEventCb, HW_WD_EXPIRE);
	}

    return TXN_STATUS_COMPLETE;
}
