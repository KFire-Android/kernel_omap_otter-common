/*
 * CmdBldCmdIE.h
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


/** \file  CmdBldCmdIE.h 
 *  \brief Command builder. Command information elements
 *
 *  \see   CmdBld.c 
 */

#ifndef CMDBLDCMDIE_H
#define CMDBLDCMDIE_H


TI_STATUS cmdBld_CmdIeStartBss          (TI_HANDLE hCmdBld, BSS_e BssType, void *fJoinCompleteCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeEnableRx          (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeEnableTx          (TI_HANDLE hCmdBld, TI_UINT8 channel, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeDisableRx         (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeDisableTx         (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeInitMemory        (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeConfigureTemplateFrame (TI_HANDLE hCmdBld, TTemplateParams *pTemplate, TI_UINT16 uFrameSize, TemplateType_e templateType, TI_UINT8 uIndex, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeStartScan         (TI_HANDLE hCmdBld, ScanParameters_t* pScanParams, void* fScanCommandResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeStartSPSScan      (TI_HANDLE hCmdBld, ScheduledScanParameters_t* pScanParams, void* fScanCommandResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeStopScan          (TI_HANDLE hCmdBld, void *fScanCommandResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeStopSPSScan       (TI_HANDLE hCmdBld, void *fScanCommandResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeSetSplitScanTimeOut (TI_HANDLE hCmdBld, TI_UINT32 uTimeOut, void *fCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeScanSsidList      (TI_HANDLE hCmdBld, ConnScanSSIDList_t *pSsidList, void* fScanResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIePeriodicScanParams(TI_HANDLE hCmdBld, ConnScanParameters_t *pPeriodicScanParams, void* fScanResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeStartPeriodicScan (TI_HANDLE hCmdBld, PeriodicScanTag* pPeriodicScanStart, void* fScanResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeStopPeriodicScan  (TI_HANDLE hCmdBld, PeriodicScanTag* pPeriodicScanStop, void* fScanResponseCB, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeNoiseHistogram    (TI_HANDLE hCmdBld, TNoiseHistogram *pNoiseHistParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeSwitchChannel     (TI_HANDLE hCmdBld, TSwitchChannelParams *pSwitchChannelCmd, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeSwitchChannelCancel (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeSetKey            (TI_HANDLE hCmdBld, TI_UINT32 uAction, TI_UINT8 *pMacAddr, TI_UINT32 uKeySize, TI_UINT32 uKeyType, TI_UINT32 uKeyId, TI_UINT8 *pKey, TI_UINT32 uSecuritySeqNumLow, TI_UINT32 SecuritySeqNumHigh, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeSetPsMode         (TI_HANDLE hCmdBld, TPowerSaveParams *pPowerSaveParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeStartNewScan      (TI_HANDLE hCmdBld, TScanParams *pScanParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeFwDisconnect      (TI_HANDLE hCmdBld, TI_UINT32 uConfigOptions, TI_UINT32 uFilterOptions, DisconnectType_e uDisconType, TI_UINT16 uDisconReason, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeMeasurement       (TI_HANDLE hCmdBld, TMeasurementParams *pMeasurementParams, void* fMeasureCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeMeasurementStop   (TI_HANDLE hCmdBld, void *fMeasureCommandResponseCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeApDiscovery       (TI_HANDLE hCmdBld, TApDiscoveryParams* pMeasurementParams, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeApDiscoveryStop   (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeHealthCheck       (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdIeSetStaState       (TI_HANDLE hCmdBld, TI_UINT8 staState, void *fCb, TI_HANDLE hCb);

TI_STATUS cmdBld_CmdIeTest              (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, TTestCmd* pTestCmdBuf);


#endif
