/*
 * DrvMainModules.h
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

#ifndef DRVMAIN_MODULES_H
#define DRVMAIN_MODULES_H

#include "tidef.h"

/* STAD Modules Handles List */
typedef struct
{
    TI_HANDLE           hDrvMain;
    TI_HANDLE           hOs;
    TI_HANDLE           hReport;
    TI_HANDLE           hContext;
    TI_HANDLE           hTimer;
    TI_HANDLE           hTWD;
    TI_HANDLE           hCmdHndlr;
    TI_HANDLE           hSme;
    TI_HANDLE           hSiteMgr;
    TI_HANDLE           hConn;
    TI_HANDLE           hMlmeSm;
    TI_HANDLE           hAuth;
    TI_HANDLE           hAssoc;
    TI_HANDLE           hRxData;
    TI_HANDLE           hTxCtrl;
    TI_HANDLE           hTxPort;
    TI_HANDLE           hTxDataQ;
    TI_HANDLE           hTxMgmtQ;
    TI_HANDLE           hCtrlData;
    TI_HANDLE           hTrafficMon;
    TI_HANDLE           hRsn;
    TI_HANDLE           hRegulatoryDomain;
    TI_HANDLE           hMeasurementMgr;
    TI_HANDLE           hSoftGemini;
    TI_HANDLE           hXCCMngr;
    TI_HANDLE           hRoamingMngr;
    TI_HANDLE           hQosMngr;
    TI_HANDLE           hPowerMgr;
    TI_HANDLE           hPowerSrv;
    TI_HANDLE           hEvHandler;
    TI_HANDLE           hAPConnection;
    TI_HANDLE           hCurrBss;
    TI_HANDLE           hSwitchChannel;
    TI_HANDLE           hSCR;               
    TI_HANDLE           hScanCncn;          
    TI_HANDLE           hScanMngr;          
    TI_HANDLE           hHealthMonitor; 
    TI_HANDLE           hCmdDispatch; 
    TI_HANDLE           hStaCap;
    TI_HANDLE           hTxnQ;

} TStadHandlesList;


#endif  /* DRVMAIN_MODULES_H */
