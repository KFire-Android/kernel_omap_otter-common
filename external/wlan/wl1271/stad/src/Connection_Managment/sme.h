/*
 * sme.h
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

/** \file  sme.h
 *  \brief SME API declarations
 *
 *  \see   sme.c, smeSm.c, smePrivate.h
 */


#ifndef __SME_H__
#define __SME_H__

#include "tidef.h"
#include "DrvMainModules.h"
#include "ScanCncn.h"
#include "paramOut.h"

#define SME_SCAN_TABLE_ENTRIES 32

typedef enum
{
    SSID_TYPE_ANY = 0,
    SSID_TYPE_SPECIFIC,
    SSID_TYPE_INVALID
} ESsidType;

typedef enum
{
    SME_RESTART_REASON_EXTERNAL_PARAM_CHANGED = 0,
    SME_RESTART_REASON_WSC_PB_MODE
} ESmeRestartReason;

typedef void (*TSmeSmCbFunc)(TI_HANDLE hCbHndl); 

TI_HANDLE   sme_Create (TI_HANDLE hOS);
void        sme_Init (TStadHandlesList *pStadHandles);
void        sme_SetDefaults (TI_HANDLE hSme, TSmeModifiedInitParams *pModifiedInitParams, TSmeInitParams *pInitParams);
void        sme_Destroy (TI_HANDLE hSme);
void        sme_Start (TI_HANDLE hSme);
void        sme_Stop (TI_HANDLE hSme);
void        sme_Restart (TI_HANDLE hSme);
void        sme_ScanResultCB (TI_HANDLE hSme, EScanCncnResultStatus eStatus,
                              TScanFrameInfo* pFrameInfo, TI_UINT16 uSPSStatus);
void        sme_MeansurementScanResult (TI_HANDLE hSme, EScanCncnResultStatus eStatus, TScanFrameInfo* pFrameInfo);
void        sme_ReportConnStatus (TI_HANDLE hSme, mgmtStatus_e eStatusType, TI_UINT32 uStatusCode);
void        sme_ReportApConnStatus (TI_HANDLE hSme, mgmtStatus_e eStatusType, TI_UINT32 uStatusCode);

void        sme_ConnectScanReport (TI_HANDLE hSme, TI_HANDLE *hScanResultTable);
void        sme_MeasureScanReport (TI_HANDLE hSme, TI_HANDLE *hScanResultTable);

#endif /* __SME_H__ */

