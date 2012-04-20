/*
 * CmdBldCfgIE.h
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


/****************************************************************************
 *
 *   MODULE:  CmdBldCfgIE.h
 *   PURPOSE: wlan hardware configuration information elements
 * 
 ****************************************************************************/

#ifndef CMDBLDCFGIE_H
#define CMDBLDCFGIE_H


#include "TWDriverInternal.h"
#include "public_infoele.h"


TI_STATUS cmdBld_CfgIeMemoryMap                	(TI_HANDLE hCmdBld, MemoryMap_t *apMap, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRx                       	(TI_HANDLE hCmdBld, TI_UINT32 apRxConfigOption, TI_UINT32 apRxFilterOption, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeStationId                	(TI_HANDLE hCmdBld, TI_UINT8* apStationId, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeSleepAuth                	(TI_HANDLE hCmdBld, EPowerPolicy minPowerLevel, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeWakeUpCondition          	(TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeFeatureConfig            	(TI_HANDLE hCmdBld, TI_UINT32 uOptions, TI_UINT32 uDataFlowOptions, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeBeaconFilterOpt          	(TI_HANDLE hCmdBld, TI_UINT8 beaconFilteringStatus, TI_UINT8 numOfBeaconsToBuffer, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeBeaconFilterTable        	(TI_HANDLE hCmdBld, TI_UINT8 numberOfIEs, TI_UINT8* pIETable, TI_UINT8 IETableSize, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeArpIpFilter              	(TI_HANDLE hCmdBld, TIpAddr tIpAddr, EArpFilterType filterType, void *fCb, TI_HANDLE hCb); 
TI_STATUS cmdBld_CfgIeGroupAdressTable         	(TI_HANDLE hCmdBld, TI_UINT8 numGroupAddrs, TMacAddr *pGroupAddr, TI_BOOL bEnabled, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeAid                      	(TI_HANDLE hCmdBld, TI_UINT16 apAidVal, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeTxPowerDbm 				(TI_HANDLE hCmdBld, TI_UINT8 uTxPowerDbm , void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeSgEnable                 	(TI_HANDLE hCmdBld, ESoftGeminiEnableModes eSoftGeminiEnableModes, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeSg                       	(TI_HANDLE hCmdBld, TSoftGeminiParams *pSoftGeminiParam, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeFmCoex                    (TI_HANDLE hCmdBld, TFmCoexParams *pFmCoexParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeConnMonitParams          	(TI_HANDLE hCmdBld, TRroamingTriggerParams *pRoamingTriggerCmd, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeConfigMemory             	(TI_HANDLE hCmdBld, TDmaParams *pDmaParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeSlotTime                 	(TI_HANDLE hCmdBld, TI_UINT8 apSlotTime, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIePreamble                 	(TI_HANDLE hCmdBld, TI_UINT8 apPreamble, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeStatisitics              	(TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeEventMask                	(TI_HANDLE hCmdBld, TI_UINT32 mask, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIePacketDetectionThreshold 	(TI_HANDLE hCmdBld, TI_UINT32 pdThreshold, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeCcaThreshold              (TI_HANDLE hCmdBld, TI_UINT16 ccaThreshold, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeTxRatePolicy             	(TI_HANDLE hCmdBld, TTxRatePolicy *pTxRatePolicy, void *fCb, TI_HANDLE hCb);                                        
TI_STATUS cmdBld_CfgIeRtsThreshold             	(TI_HANDLE hCmdBld, TI_UINT16 uRtsThreshold, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeDcoItrimParams            (TI_HANDLE hCmdBld, TI_BOOL enable, TI_UINT32 moderationTimeoutUsec, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeFragmentThreshold         (TI_HANDLE hCmdBld, TI_UINT16 uFragmentThreshold, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIePmConfig                  (TI_HANDLE hCmdBld, TI_UINT32 uHostClkSettlingTime, TI_UINT8 uHostFastWakeupSupport, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeTxCmpltPacing             (TI_HANDLE hCmdBld, TI_UINT16 uTxCompletePacingThreshold, TI_UINT16 uTxCompletePacingTimeout, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRxIntrPacing              (TI_HANDLE hCmdBld, TI_UINT16 uRxIntrPacingThreshold, TI_UINT16 uRxIntrPacingTimeout, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeCtsProtection            	(TI_HANDLE hCmdBld, TI_UINT8 uCtsToSelf, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeBcnBrcOptions            	(TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIePsWmm                    	(TI_HANDLE hCmdBld, TI_BOOL bEnableWA, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRssiSnrTrigger           	(TI_HANDLE hCmdBld, RssiSnrTriggerCfg_t *pTriggerParam, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRssiSnrWeights           	(TI_HANDLE hCmdBld, RssiSnrAverageWeights_t *pWeightsParam, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeTid                      	(TI_HANDLE hCmdBld, TQueueTrafficParams *pQtrafficParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeAcParams                 	(TI_HANDLE hCmdBld, TAcQosParams *pConfigureCommand, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIePsRxStreaming             (TI_HANDLE hCmdBld, TPsRxStreaming *pPsRxStreaming, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeMaxTxRetry               	(TI_HANDLE hCmdBld, TRroamingTriggerParams *pRoamingTriggerCmd, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeServicePeriodTimeout     	(TI_HANDLE hCmdBld, TRxTimeOut* pRxTimeOut, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRxMsduLifeTime           	(TI_HANDLE hCmdBld, TI_UINT32 uRxMsduLifeTime, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeEnableRxDataFilter       	(TI_HANDLE hCmdBld, TI_BOOL enabled, filter_e eDefaultAction, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRxDataFilter             	(TI_HANDLE hCmdBld, TI_UINT8 index, TI_UINT8 command, filter_e action, TI_UINT8 numFieldPatterns, TI_UINT8 lenFieldPatterns, TI_UINT8 *pFieldPatterns, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeBet                      	(TI_HANDLE hCmdBld, TI_UINT8 Enable, TI_UINT8 MaximumConsecutiveET, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeConfigureKeepAliveParams  (TI_HANDLE hCmdBld, TI_UINT8 uIndex, TI_UINT8 uEnaDisFlag, TI_UINT8 trigType, TI_UINT32 interval, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeConfigureKeepAliveEnaDis  (TI_HANDLE hCmdBld, TI_UINT8 enaDisFlag, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeSetBaSession              (TI_HANDLE hCmdBld, InfoElement_e eBaType, TI_UINT8 uTid, TI_UINT8 uState, TMacAddr tRa, TI_UINT16 uWinSize, TI_UINT16 uInactivityTimeout, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeSetFwHtCapabilities       (TI_HANDLE hCmdBld, TI_UINT32 uHtCapabilites, TMacAddr tMacAddress, TI_UINT8 uAmpduMaxLeng, TI_UINT8 uAmpduMinSpac, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeSetFwHtInformation        (TI_HANDLE hCmdBld, TI_UINT8 uRifsMode, TI_UINT8 uHtProtection, TI_UINT8 uGfProtection, TI_UINT8 uHtTxBurstLimit, TI_UINT8 uDualCtsProtection, void *fCb,TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRadioParams               (TI_HANDLE hCmdBld, IniFileRadioParam *pIniFileRadioParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeExtendedRadioParams 		(TI_HANDLE hCmdBld, IniFileExtendedRadioParam *pIniFileExtRadioParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgPlatformGenParams           (TI_HANDLE hCmdBld, IniFileGeneralParam *pGenParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeCoexActivity              (TI_HANDLE hCmdBld, TCoexActivity *pCoexActivity, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeBurstMode 				(TI_HANDLE hCmdBld, TI_BOOL bEnabled, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgIeRateMngDbg 				(TI_HANDLE hCmdBld, RateMangeParams_t *pRateMngParams, void *fCb, TI_HANDLE hCb);
#endif

