/*
 * cu_cmd.h
 *
 * Copyright 2001-2009 Texas Instruments, Inc. - http://www.ti.com/
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and  
 * limitations under the License.
 */

/****************************************************************************/
/*                                                                          */
/*    MODULE:   cu_cmd.h                                                    */
/*    PURPOSE:                                                              */
/*                                                                          */
/****************************************************************************/
#ifndef _CU_CMD_H_
#define _CU_CMD_H_

/* defines */
/***********/

/* types */
/*********/
typedef struct
{
    U32 value;
    PS8 name;
} named_value_t;

/* functions */
/*************/
THandle CuCmd_Create(const PS8 device_name, THandle hConsole, S32 BypassSupplicant, PS8 pSupplIfFile);
VOID CuCmd_Destroy(THandle hCuCmd);
S32  CuCmd_GetDeviceStatus(THandle hCuCmd);
VOID CuCmd_StartDriver(THandle hCuCmd);
VOID CuCmd_StopDriver(THandle hCuCmd);

#ifdef XCC_MODULE_INCLUDED
THandle CuCmd_GetCuCommonHandle(THandle hCuCmd);
THandle CuCmd_GetCuWpaHandle (THandle hCuCmd);
#endif


VOID CuCmd_Show_Status(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_BssidList(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_FullBssidList(THandle hCuCmd, ConParm_t parm[], U16 nParms);
#ifdef CONFIG_WPS
VOID CuCmd_StartEnrolleePIN(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_StartEnrolleePBC(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_StopEnrollee(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPin(THandle hCuCmd, ConParm_t parm[], U16 nParms);
#endif
VOID CuCmd_Connect(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_Disassociate(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_ModifySsid(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyChannel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyConnectMode(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetTxRate(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyBssType(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyFragTh(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyRtsTh(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyPreamble(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyShortSlot(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioOnOff(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetSelectedBssidInfo(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetDriverState(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetRsiiLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetSnrRatio(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ShowTxPowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ShowTxPowerTable(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_TxPowerDbm(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyState_802_11d(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyState_802_11h(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_D_Country_2_4Ie(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_D_Country_5Ie(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyDfsRange(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetBeaconFilterDesiredState(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetBeaconFilterDesiredState(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifySupportedRates(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SendHealthCheck(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_EnableRxDataFilters(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_DisableRxDataFilters(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_AddRxDataFilter(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RemoveRxDataFilter(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetRxDataFiltersStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_ShowStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ShowTxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ShowAdvancedParams(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_ScanAppGlobalConfig(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ScanAppChannelConfig(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ScanAppClear(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ScanAppDisplay(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ScanSetSra(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ScanSetRssi(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_StartScan(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_StopScan (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_WextStartScan(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigPeriodicScanGlobal (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigPeriodicScanInterval (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigurePeriodicScanSsid (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigurePeriodicScanChannel (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ClearPeriodicScanConfiguration (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_DisplayPeriodicScanConfiguration (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_StartPeriodicScan (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_StopPeriodicScan (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigScanBand(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigScanBandChannel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigScanBandTrack(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigScanBandDiscover(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigScanBandImmed(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_DisplayScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ClearScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetScanBssList(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_RoamingEnable(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingDisable(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingLowPassFilter(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingQualityIndicator(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingDataRetryThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingNumExpectedTbttForBSSLoss(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingTxRateThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingLowRssiThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingLowSnrThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms);                                                                           
VOID CuCmd_RoamingLowQualityForBackgroungScanCondition(THandle hCuCmd, ConParm_t parm[], U16 nParms); 
VOID CuCmd_RoamingNormalQualityForBackgroungScanCondition(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RoamingGetConfParams(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_CurrBssUserDefinedTrigger(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_AddTspec(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetTspec(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_DeleteTspec(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetApQosParams(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetPsRxStreamingParams(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetApQosCapabilities(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetAcStatus(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ModifyMediumUsageTh(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetDesiredPsMode(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_InsertClsfrEntry(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RemoveClsfrEntry(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPsRxDelivery(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetQosParams(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetRxTimeOut(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_RegisterEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_UnregisterEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_EnableBtCoe(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigBtCoe(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetBtCoeStatus(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigCoexActivity(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ConfigFmCoex(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_SetPowerMode(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPowerSavePowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetDefaultPowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetDozeModeInAutoPowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetTrafficIntensityTh(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_EnableTrafficEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_DisableTrafficEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetDcoItrimParams(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_LogAddReport(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_LogReportSeverityLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_SetReport(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_AddReport(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ClearReport(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ReportSeverityLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetReportLevelCLI(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_PrintDriverDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_PrintDriverDebugBuffer(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_FwDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetRateMngDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetRateMngDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms);


VOID CuCmd_SetArpIPFilter (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ShowAbout(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_Quit(THandle hCuCmd, ConParm_t parm[], U16 nParms);

/* Radio Debug Tests */
VOID CuCmd_RadioDebug_ChannelTune(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_StartTxCw(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_StartContinuousTx(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_StopTx(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_Template(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_StartRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_StopRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_ResetRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_GetRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RadioDebug_GetHDKVersion(THandle hCuCmd, ConParm_t parm[], U16 nParms);

/* BIP Tests */
VOID CuCmd_BIP_StartBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_BIP_EnterRxBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms); 
VOID CuCmd_BIP_StartRxBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_BIP_ExitRxBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_BIP_BufferCalReferencePoint(THandle hCuCmd, ConParm_t parm[], U16 nParms);

VOID CuCmd_SetPrivacyAuth(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyEap(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyEncryption(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyKeyType(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyMixedMode(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyAnyWpaMode(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyCredentials(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyPskPassPhrase(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_SetPrivacyCertificate(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_StopSuppl(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ChangeSupplDebugLevels(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_AddPrivacyKey(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RemovePrivacyKey(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_GetPrivacyDefaultKey(THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_EnableKeepAlive (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_DisableKeepAlive (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_AddKeepAliveMessage (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_RemoveKeepAliveMessage (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID CuCmd_ShowKeepAlive (THandle hCuCmd, ConParm_t parm[], U16 nParms);
VOID Cucmd_ShowPowerConsumptionStats(THandle hCuCmd,ConParm_t parm[],U16 nParms);

#endif  /* _CU_CMD_H_ */

