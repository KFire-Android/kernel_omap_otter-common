/*
 * CmdBld.h
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



#ifndef CMDBLD_H
#define CMDBLD_H


#include "TWDriver.h"
#include "CmdBldDb.h"

TI_HANDLE cmdBld_Create                 (TI_HANDLE hOs);
TI_STATUS cmdBld_Destroy                (TI_HANDLE hCmdBld);
TI_STATUS cmdBld_Restart               (TI_HANDLE hCmdBld);
TI_STATUS cmdBld_Config                 (TI_HANDLE  hCmdBld, 
                                         TI_HANDLE  hReport, 
                                         void      *fFinalizeDownload, 
                                         TI_HANDLE  hFinalizeDownload, 
                                         TI_HANDLE  hEventMbox, 
                                         TI_HANDLE  hCmdQueue,
                                         TI_HANDLE  hTwIf);
TI_STATUS cmdBld_ConfigFw               (TI_HANDLE hCmdBld, void *fConfigFwCb, TI_HANDLE hConfigFwCb);
TI_STATUS cmdBld_CheckMboxCb            (TI_HANDLE hCmdBld, void *fFailureEvCb, TI_HANDLE hFailureEv);
TI_STATUS cmdBld_GetParam               (TI_HANDLE hCmdBld, TTwdParamInfo *pParamInfo);
TI_STATUS cmdBld_ReadMib                (TI_HANDLE hCmdBld, TI_HANDLE hCb, void* fCb, void* pCb);
TI_STATUS cmdBld_ConvertAppRatesBitmap  (TI_UINT32 uAppRatesBitmap, TI_UINT32 uAppModulation, EHwRateBitFiled *pHwRatesBitmap);
TI_STATUS cmdBld_ConvertAppRate (ERate AppRate, TI_UINT8 *pHwRate);
EHwRateBitFiled rateNumberToBitmap(TI_UINT8 uRate);

/* Commands */
TI_STATUS cmdBld_CmdNoiseHistogram      (TI_HANDLE hCmdBld, TNoiseHistogram* pNoiseHistParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdMeasurement         (TI_HANDLE hCmdBld, TMeasurementParams *pMeasurementParams, void *fCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdMeasurementStop     (TI_HANDLE hCmdBld, void *fCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdApDiscovery         (TI_HANDLE hCmdBld, TApDiscoveryParams* pApDiscoveryParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdApDiscoveryStop     (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdStartScan           (TI_HANDLE hCmdBld, TScanParams *pScanVals, EScanResultTag eScanTag, TI_BOOL bHighPriority, void *fScanCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdStartSPSScan        (TI_HANDLE hCmdBld, TScanParams *pScanVals, EScanResultTag eScanTag, void *fScanCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdStopScan            (TI_HANDLE hCmdBld, EScanResultTag eScanTag, void *fScanCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdStopSPSScan         (TI_HANDLE hCmdBld, EScanResultTag eScanTag, void *fScanCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdSetSplitScanTimeOut (TI_HANDLE hCmdBld, TI_UINT32 uTimeOut);
TI_STATUS cmdBld_StartPeriodicScan      (TI_HANDLE hCmdBld, TPeriodicScanParams *pPeriodicScanParams, EScanResultTag eScanTag, TI_UINT32 uPassiveScanDfsDwellTimeMs, void* fScanCommandResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_StopPeriodicScan       (TI_HANDLE hCmdBld, EScanResultTag eScanTag, void* fScanCommandResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdStartJoin           (TI_HANDLE hCmdBld, ScanBssType_e eBssType, void *fJoinCompleteCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdJoinBss             (TI_HANDLE hCmdBld, TJoinBss *pJoinBssParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdTemplate            (TI_HANDLE hCmdBld, TSetTemplate *pTemplateParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdSwitchChannel       (TI_HANDLE hCmdBld, TSwitchChannelParams *pSwitchChannelCmdvoid, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdSwitchChannelCancel (TI_HANDLE hCmdBld, TI_UINT8 channel, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdEnableTx            (TI_HANDLE hCmdBld, TI_UINT8 channel, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdDisableTx           (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdFwDisconnect        (TI_HANDLE hCmdBld, TI_UINT32 uConfigOptions, TI_UINT32 uFilterOptions, DisconnectType_e uDisconType, TI_UINT16 uDisconReason, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdHealthCheck         (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdSetPsMode           (TI_HANDLE hCmdBld, TPowerSaveParams* pPowerSaveParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdEnableRx            (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdAddWepDefaultKey    (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdRemoveWepDefaultKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdSetWepDefaultKeyId  (TI_HANDLE hCmdBld, TI_UINT8 aKeyIdVal, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdAddWpaKey           (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdRemoveWpaKey        (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdAddTkipMicMappingKey(TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdRemoveTkipMicMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdAddAesMappingKey    (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdRemoveAesMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
#ifdef GEM_SUPPORTED
    TI_STATUS cmdBld_CmdAddGemMappingKey    (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
    TI_STATUS cmdBld_CmdRemoveGemMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
#endif
TI_STATUS cmdBld_CmdAddKey              (TI_HANDLE hCmdBld, TSecurityKeys* pKey, TI_BOOL bReconfFlag, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdRemoveKey           (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdConfigureTemplates  (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdSetStaState         (TI_HANDLE hCmdBld, TI_UINT8 staState, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdTest                (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, TTestCmd* pTestCmd);


/* Config */
TI_STATUS cmdBld_CfgEventMask           (TI_HANDLE hCmdBld, TI_UINT32 uEventMask, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgConnMonitParams     (TI_HANDLE hCmdBld, TRroamingTriggerParams *pRoamingTriggerCmd, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgMaxTxRetry          (TI_HANDLE hCmdBld, TRroamingTriggerParams *pRoamingTriggerCmd, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgAid                 (TI_HANDLE hCmdBld, TI_UINT16 aid, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSlotTime            (TI_HANDLE hCmdBld, ESlotTime eSlotTimeVal, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgArpIpAddrTable      (TI_HANDLE hCmdBld, TIpAddr tIpAddr, TI_UINT8 bEnabled, EIpVer eIpVer, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgArpIpFilter         (TI_HANDLE hCmdBld, TIpAddr tIpAddr, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgRx                  (TI_HANDLE hCmdBld, TI_UINT32 uRxConfigOption, TI_UINT32 uRxFilterOption, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgPreamble            (TI_HANDLE hCmdBld, Preamble_e ePreamble, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgPacketDetectionThreshold (TI_HANDLE hCmdBld, TI_UINT32 threshold, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgBeaconFilterOpt     (TI_HANDLE hCmdBld, TI_UINT8 uBeaconFilteringStatus, TI_UINT8 uNumOfBeaconsToBuffer, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgBeaconFilterTable   (TI_HANDLE hCmdBld, TI_UINT8 uNumberOfIEs, TI_UINT8 *pIETable, TI_UINT8 uIeTableSize, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgWakeUpCondition     (TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgBcnBrcOptions       (TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgGroupAddressTable   (TI_HANDLE hCmdBld, TI_UINT8 numGroupAddrs, TMacAddr *pGroupAddr, TI_BOOL bEnabled, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSleepAuth           (TI_HANDLE hCmdBld, EPowerPolicy eMinPowerLevel, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSgEnable            (TI_HANDLE hCmdBld, ESoftGeminiEnableModes eSgEnable, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSg                  (TI_HANDLE hCmdBld, TSoftGeminiParams *pSgParam, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgTxRatePolicy        (TI_HANDLE hCmdBld, TTxRatePolicy *pTxRatePolicy, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgTid                 (TI_HANDLE hCmdBld, TQueueTrafficParams *pQtrafficParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgAcParams            (TI_HANDLE hCmdBld, TAcQosParams *pAcQosParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgPsRxStreaming       (TI_HANDLE hCmdBld, TPsRxStreaming *pPsRxStreaming, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgClkRun              (TI_HANDLE hCmdBld, TI_BOOL bClkRunEnable, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgHwEncEnable         (TI_HANDLE hCmdBld, TI_BOOL aHwEncEnable, TI_BOOL bHwDecEnable, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgHwEncDecEnable      (TI_HANDLE hCmdBld, TI_BOOL bHwEncEnable, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgRxMsduFormat        (TI_HANDLE hCmdBld, TI_BOOL bRxMsduForamtEnable, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgRtsThreshold        (TI_HANDLE hCmdBld, TI_UINT16 threshold, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgDcoItrimParams      (TI_HANDLE hCmdBld, TI_BOOL enable, TI_UINT32 moderationTimeoutUsec, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgFragmentThreshold   (TI_HANDLE hCmdBld, TI_UINT16 uFragmentThreshold, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSecureMode          (TI_HANDLE hCmdBld, ECipherSuite eSecurMode, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgMacClock            (TI_HANDLE hCmdBld, TI_UINT32 uMacClock, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgArmClock            (TI_HANDLE hCmdBld, TI_UINT32 uArmClock, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgEnableRxDataFilter  (TI_HANDLE hCmdBld, TI_BOOL bEnabled, filter_e eDefaultAction, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgRxDataFilter        (TI_HANDLE hCmdBld, TI_UINT8 index, TI_UINT8 command, filter_e eAction, TI_UINT8 uNumFieldPatterns, TI_UINT8 uLenFieldPatterns, TI_UINT8 *pFieldPatterns, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgCtsProtection       (TI_HANDLE hCmdBld, TI_UINT8 uCtsProtection, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgServicePeriodTimeout(TI_HANDLE hCmdBld, TRxTimeOut *pRxTimeOut, void *fCb, TI_HANDLE hCb);                            
TI_STATUS cmdBld_CfgRxMsduLifeTime      (TI_HANDLE hCmdBld, TI_UINT32 uRxMsduLifeTime, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgStatisitics         (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgTxPowerDbm          (TI_HANDLE hCmdBld, TI_UINT8 uTxPowerDbm, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgRssiSnrTrigger      (TI_HANDLE hCmdBld, RssiSnrTriggerCfg_t *pTriggerParam, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgRssiSnrWeights      (TI_HANDLE hCmdBld, RssiSnrAverageWeights_t *pWeightsParam, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgBet                 (TI_HANDLE hCmdBld, TI_UINT8 Enable, TI_UINT8 MaximumConsecutiveET, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgKeepAlive           (TI_HANDLE hCmdBld, TKeepAliveParams *pKeepAliveParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgKeepAliveEnaDis     (TI_HANDLE hCmdBld, TI_UINT8 enaDisFlag, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSetBaSession        (TI_HANDLE hCmdBld, InfoElement_e eBaType, TI_UINT8 uTid, TI_UINT8 uState, TMacAddr tRa, TI_UINT16 uWinSize, TI_UINT16 uInactivityTimeout, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSetFwHtCapabilities (TI_HANDLE hCmdBld, TI_UINT32 uHtCapabilites, TMacAddr tMacAddress, TI_UINT8 uAmpduMaxLeng, TI_UINT8 uAmpduMinSpac, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgSetFwHtInformation  (TI_HANDLE hCmdBld, TI_UINT8 uRifsMode,TI_UINT8 uHtProtection, TI_UINT8 uGfProtection, TI_UINT8 uHtTxBurstLimit, TI_UINT8 uDualCtsProtection, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgCoexActivity        (TI_HANDLE hCmdBld, TCoexActivity *pCoexActivity, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgBurstMode 			(TI_HANDLE hCmdBld, TI_BOOL bEnabled, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CfgFmCoex              (TI_HANDLE hCmdBld, TFmCoexParams *pFmCoexParams, void *fCb, TI_HANDLE hCb);

/* Interrogate */
TI_STATUS cmdBld_ItrRSSI                    (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrErrorCnt                (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrRoamimgStatisitics      (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrSg                      (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrMemoryMap               (TI_HANDLE hCmdBld, MemoryMap_t *pMap, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_ItrStatistics              (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrDataFilterStatistics    (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrPowerConsumptionstat    (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void* pCb);
TI_STATUS cmdBld_ItrRateParams              (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void* pCb);


/* Get */
TI_STATUS cmdBld_GetGroupAddressTable       (TI_HANDLE hCmdBld, TI_UINT8* pEnabled, TI_UINT8* pNumGroupAddrs, TMacAddr *pGroupAddr);
TI_UINT8  cmdBld_GetDtimCount               (TI_HANDLE hCmdBld);
TI_UINT16 cmdBld_GetBeaconInterval          (TI_HANDLE hCmdBld);
TFwInfo * cmdBld_GetFWInfo                  (TI_HANDLE hCmdBld);
TI_STATUS cmdBld_GetRxFilters               (TI_HANDLE hCmdBld, TI_UINT32 *pRxConfigOption, TI_UINT32 *pRxFilterOption);
TI_UINT8  cmdBld_GetBssType                 (TI_HANDLE hCmdBld); 
TI_UINT32 cmdBld_GetAckPolicy               (TI_HANDLE hCmdBld, TI_UINT32 uQueueId); 
TI_STATUS cmdBld_GetPltRxCalibrationStatus  ( TI_HANDLE hCmdBld, TI_STATUS *pLastStatus );

/* Set */
TI_STATUS cmdBld_SetRadioBand           (TI_HANDLE hCmdBld, ERadioBand eRadioBand);
TI_STATUS cmdBld_SetRxFilter            (TI_HANDLE hCmdBld, TI_UINT32 uRxConfigOption, TI_UINT32 uRxFilterOption);
TI_STATUS cmdBld_SetSecuritySeqNum      (TI_HANDLE hCmdBld, TI_UINT8 securitySeqNumLsByte);
TI_STATUS cmdBld_CfgRateMngDbg 			(TI_HANDLE hCmdBld, RateMangeParams_t *pRateMngParams ,void *fCb, TI_HANDLE  hCb);


/* this is a solution for the EMP project Enable/Disable Rx Data*/
TI_STATUS cmdBld_SetDownlinkData  (TI_HANDLE hCmdBld, TI_BOOL bDownlinkFlag);


#ifdef TI_DBG
void cmdBld_DbgForceTemplatesRates (TI_HANDLE hCmdBld, TI_UINT32 uRateMask);
#endif


typedef struct
{
    TI_UINT32                  uNumOfStations;
    ECipherSuite               eSecurityMode;
    EKeyType                   eCurTxKeyType;  /* Key type of current configured unicast key */ 

} TSecurity;


typedef struct 
{
    TI_HANDLE                  hOs;
    TI_HANDLE                  hReport;
    TI_HANDLE                  hCmdQueue;
    TI_HANDLE                  hEventMbox;
    TI_HANDLE                  hTwIf;

    TCmdBldDb                  tDb;
    TSecurity                  tSecurity;
    MemoryMap_t                tMemMap;
    TI_UINT16                  uSecuritySeqNumLow;
    TI_UINT32                  uSecuritySeqNumHigh;

    void                       *fFinalizeDownload;
    TI_HANDLE                  hFinalizeDownload;

    void                       *fConfigFwCb;
    TI_HANDLE                  hConfigFwCb;

    void                       *fJoinCmpltOriginalCbFunc;
    TI_HANDLE                  hJoinCmpltOriginalCbHndl;

    TI_UINT32                  uIniSeq;         /* Init sequence counter */
    TI_BOOL                    bReconfigInProgress;

    TI_UINT32                  uLastElpCtrlMode;/* Init sleep mode */

#ifdef TI_DBG
    TI_UINT32                  uDbgTemplatesRateMask;
#endif

} TCmdBld;


/* 
 * Define the number of keys allocated on reconfigure 
 * data structure for each station
 */
#define NO_OF_RECONF_SECUR_KEYS_PER_STATION     1 
#define NO_OF_EXTRA_RECONF_SECUR_KEYS           3 


#define DB_QUEUES(HCMDBLD) (((TCmdBld *)HCMDBLD)->tDb.queues)
#define DB_AC(HCMDBLD)     (((TCmdBld *)HCMDBLD)->tDb.ac)
#define DB_PS_STREAM(HCMDBLD) (((TCmdBld *)HCMDBLD)->tDb.psStream)
#define DB_WLAN(HCMDBLD)   (((TCmdBld *)HCMDBLD)->tDb.wlan)
#define DB_DMA(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.dma)
#define DB_BSS(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.bss)
#define DB_HW(HCMDBLD)     (((TCmdBld *)HCMDBLD)->tDb.hw)
#define DB_CNT(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.counters)
#define DB_TEMP(HCMDBLD)   (((TCmdBld *)HCMDBLD)->tDb.templateList)
#define DB_KLV(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.klvList)
#define DB_KEYS(HCMDBLD)            (((TCmdBld *)HCMDBLD)->tDb.keys)
#define DB_RX_DATA_FLTR(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.rxDataFilters)
#define DB_RADIO(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.tRadioIniParams)
#define DB_EXT_RADIO(HCMDBLD)		(((TCmdBld *)HCMDBLD)->tDb.tExtRadioIniParams)
#define DB_GEN(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.tPlatformGenParams)
#define DB_RM(HCMDBLD)    (((TCmdBld *)HCMDBLD)->tDb.tRateMngParams)


#define DB_DEFAULT_CHANNEL(HCMDBLD)                     \
    (RADIO_BAND_5_0_GHZ == DB_WLAN(HCMDBLD).RadioBand)  \
        ? DB_WLAN(HCMDBLD).calibrationChannel5_0        \
        : DB_WLAN(HCMDBLD).calibrationChannel2_4

#endif

