/*
 * smePrivate.h
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

/** \file  smePrivate.h
 *  \brief SME private declaratnions
 *
 *  \see   sme.c, sme.h, smeSm.c
 */


#ifndef __SME_PRIVATE_H__
#define __SME_PRIVATE_H__

#include "tidef.h"
#include "TWDriver.h"
#include "sme.h"
#include "scanResultTable.h"

typedef struct
{
    mgmtStatus_e    eMgmtStatus;     /* Contains the last DisAssociation reason towards upper layer                  */
    TI_UINT32       uStatusCode;     /* Extra information to the reason. i.e. packet status code or roaming trigger  */
} TDisAssocReason;

typedef struct
{
    /* Handles to other modules */
    TI_HANDLE       hOS;
    TI_HANDLE       hReport;
    TI_HANDLE       hScanCncn;
    TI_HANDLE       hApConn;
    TI_HANDLE       hConn;
    TI_HANDLE       hScr;
    TI_HANDLE       hRegDomain;
    TI_HANDLE       hEvHandler;
    TI_HANDLE       hSiteMgr;
    TI_HANDLE       hRsn;
    TI_HANDLE       hScanResultTable;          /* Working table - points to one of the next two tables */
    TI_HANDLE       hSmeScanResultTable;       /* Sme local table */
    TI_HANDLE       hScanCncnScanResulTable;   /* Scan Cncn table - table used by the application */
    TI_HANDLE       hSmeSm;
    TI_HANDLE       hDrvMain;
    TI_HANDLE       hTwd;

    /* parameters */
    TI_BOOL         bRadioOn;
    EConnectMode    eConnectMode;
    ScanBssType_e   eBssType;
    TMacAddr        tBssid;
    ESsidType       eSsidType;
    TSsid           tSsid;

    /* internal state-machine variables */
    TI_BOOL         bConnectRequired;
    TI_BOOL         bRunning;
    EConnectMode    eLastConnectMode;
    TI_BOOL         bAuthSent;
    TI_UINT32       uScanCount;
    TI_BOOL         bReselect;      /* for SG Avalanche */

    TI_BOOL         bConstantScan;  /* scan constantly, for WSC PB mode */
    TSiteEntry      *pCandidate;
	TSiteEntry      tCandidate;     /* used to store the selected entry of sme_Select*/
    TDisAssocReason tDisAssoc;

    TSmeInitParams  tInitParams;
    TPeriodicScanParams	tScanParams; /* temporary storage for scan command */

} TSme;

TSiteEntry *sme_Select (TI_HANDLE hSme);


#endif /* __SME_PRIVATE_H__ */
