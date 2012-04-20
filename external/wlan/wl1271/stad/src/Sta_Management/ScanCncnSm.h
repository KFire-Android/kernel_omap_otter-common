/*
 * ScanCncnSm.h
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

/** \file  ScanCncnSm.h 
 *  \brief Scan concentrator state machine declarations
 *
 *  \see   ScanCncnSm.c
 */


#ifndef __SCAN_CNCN_SM_H__
#define __SCAN_CNCN_SM_H__

#include "osTIType.h"
#include "TWDriver.h"
#include "ScanCncn.h"

typedef void (*TScanPrivateSMFunction)(TI_HANDLE hScanCncn);

typedef union
{
    TPeriodicScanParams tPeriodicScanParams;
    TScanParams         tOneShotScanParams;
}UScanParams;

typedef struct
{
    TI_HANDLE               hOS;
    TI_HANDLE               hReport;
    TI_HANDLE               hSCR;
    TI_HANDLE               hTWD;
    TI_HANDLE               hApConn;
    TI_HANDLE               hMlme;
    TI_HANDLE               hGenSM;
    TI_HANDLE               hScanCncn;
    TI_BOOL                 bScanCompletePending; /* TRUE if scan complete event is received
                                                    before all results, for periodic scan */
    TI_BOOL                 bInRequest;
    TI_BOOL                 bSendNullDataOnStop;  /* indicates whether NULL data frame is to be sent wehn
                                                     stopping scan to return to previous PS mode */
    TScanPrivateSMFunction  fScrRequest;
    TScanPrivateSMFunction  fScrRelease;
    TScanPrivateSMFunction  fStartScan;
    TScanPrivateSMFunction  fStopScan;
    TScanPrivateSMFunction  fRecovery;

    /* Scan complete callbacks */ 
    TScanResultCB           tScanResultCB;
    TI_HANDLE               hScanResultCBObj;

    UScanParams             uScanParams;
    EScanCncnResultStatus   eScanResult;
    TI_INT8                 *pScanSmName;
    TI_UINT16               uSPSScanResult;
    TI_UINT32               uResultCounter;
    TI_UINT32               uResultExpectedNumber;
	TI_BOOL					bScanRejectedOn2_4;
} TScanCncnClient;

typedef enum
{
    SCAN_CNCN_SM_STATE_IDLE = 0,
    SCAN_CNCN_SM_STATE_SCR_WAIT,
    SCAN_CNCN_SM_STATE_SCANNING,
    SCAN_CNCN_SM_STATE_STOPPING,
    SCAN_CNCN_SM_NUMBER_OF_STATES
} EScanCncnSmStates;

typedef enum
{
    SCAN_CNCN_SM_EVENT_START = 0,
    SCAN_CNCN_SM_EVENT_RUN,
    SCAN_CNCN_SM_EVENT_SCAN_COMPLETE,
    SCAN_CNCN_SM_EVENT_STOP,
    SCAN_CNCN_SM_EVENT_ABORT,
    SCAN_CNCN_SM_EVENT_RECOVERY,
    SCAN_CNCN_SM_EVENT_REJECT,
    SCAN_CNCN_SM_NUMBER_OF_EVENTS
} EScanCncnSmEvents;

TI_HANDLE   scanCncnSm_Create               (TI_HANDLE hOS);
void        scanCncnSm_Init                 (TI_HANDLE hScanCncnClient, TI_HANDLE hReport, TI_HANDLE hTWD, 
                                             TI_HANDLE hSCR, TI_HANDLE hApConn, TI_HANDLE hMlme, 
                                             TI_HANDLE hScanCncn, TScanPrivateSMFunction fScrRequest, 
                                             TScanPrivateSMFunction fScrRelease, TScanPrivateSMFunction fStartScan, 
                                             TScanPrivateSMFunction fStopScan, TScanPrivateSMFunction fRecovery, 
                                             TI_INT8* pScanSmName);
void        scanCncnSm_Destroy              (TI_HANDLE hScanCncnClient);
void        scanCncnSmApp1Shot_ScrRequest      (TI_HANDLE hScanCncnClient);
void        scanCncnSmApp1Shot_ScrRelease      (TI_HANDLE hScanCncnClient);
void        scanCncnSmApp1Shot_StartScan       (TI_HANDLE hScanCncnClient);
void        scanCncnSmApp1Shot_StopScan        (TI_HANDLE hScanCncnClient);
void        scanCncnSmApp1Shot_Recovery        (TI_HANDLE hScanCncnClient);
void        scanCncnSmAppP_ScrRequest       (TI_HANDLE hScanCncnClient);
void        scanCncnSmAppP_ScrRelease       (TI_HANDLE hScanCncnClient);
void        scanCncnSmAppP_StartScan        (TI_HANDLE hScanCncnClient);
void        scanCncnSmAppP_StopScan         (TI_HANDLE hScanCncnClient);
void        scanCncnSmAppP_Recovery         (TI_HANDLE hScanCncnClient);
void        scanCncnSmDrvP_ScrRequest       (TI_HANDLE hScanCncnClient);
void        scanCncnSmDrvP_ScrRelease       (TI_HANDLE hScanCncnClient);
void        scanCncnSmDrvP_StartScan        (TI_HANDLE hScanCncnClient);
void        scanCncnSmDrvP_StopScan         (TI_HANDLE hScanCncnClient);
void        scanCncnSmDrvP_Recovery         (TI_HANDLE hScanCncnClient);
void        scanCncnSmCont1Shot_ScrRequest     (TI_HANDLE hScanCncnClient);
void        scanCncnSmCont1Shot_ScrRelease     (TI_HANDLE hScanCncnClient);
void        scanCncnSmCont1Shot_StartScan      (TI_HANDLE hScanCncnClient);
void        scanCncnSmCont1Shot_StopScan       (TI_HANDLE hScanCncnClient);
void        scanCncnSmCont1Shot_Recovery       (TI_HANDLE hScanCncnClient);
void        scanCncnSmImmed1Shot_ScrRequest    (TI_HANDLE hScanCncnClient);
void        scanCncnSmImmed1Shot_ScrRelease    (TI_HANDLE hScanCncnClient);
void        scanCncnSmImmed1Shot_StartScan     (TI_HANDLE hScanCncnClient);
void        scanCncnSmImmed1Shot_StopScan      (TI_HANDLE hScanCncnClient);
void        scanCncnSmImmed1Shot_Recovery      (TI_HANDLE hScanCncnClient);

#endif /* __SCAN_CNCN_SM_H__ */

