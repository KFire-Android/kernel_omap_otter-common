/*
 * ScanCncnPrivate.h
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

/** \file  ScanCncnPrivate.h
 *  \brief Scan concentartor module private definitions
 *
 *  \see   ScanCncn.c, ScanCncn.h, ScanCncnApp.c
 */

#ifndef __SCAN_CNCN_PRIVATE_H__
#define __SCAN_CNCN_PRIVATE_H__

#include "osTIType.h"
#include "TWDriver.h"
#include "scrApi.h"
#include "ScanCncnSm.h"

/** \enum EConnectionStatus
 * \brief enumerates the different connection statuses
 */
typedef enum
{
    STA_CONNECTED = 0,              /**< the station is connected to an infrastructure BSS */
    STA_NOT_CONNECTED,              /**< the station is not connected to an infrastructure BSS */
    STA_IBSS                        /**< the station is participating in an IBSS */
} EConnectionStatus;

typedef struct
{
    /* handles to other modules */
    TI_HANDLE               hOS;
    TI_HANDLE               hTWD;
    TI_HANDLE               hReport;
    TI_HANDLE               hRegulatoryDomain;
    TI_HANDLE               hSiteManager;
    TI_HANDLE               hSCR;
    TI_HANDLE               hAPConn;
    TI_HANDLE               hEvHandler;
    TI_HANDLE               hMlme;
    TI_HANDLE               hHealthMonitor;
    TI_HANDLE               hSme;

    /* client specific information */
    TScanCncnClient         *pScanClients[ SCAN_SCC_NUM_OF_CLIENTS ];

    /* SG Flags */
    TI_BOOL                 bUseSGParams;
    TI_UINT32               uSGcompensationPercent;
    TI_UINT32               uSGcompensationMaxTime;
    TI_UINT8                uSGprobeRequestPercent;   
        
    /* connection status */
    EConnectionStatus       eConnectionStatus;
    TScanCncnInitParams     tInitParams;

    /* scan concentrator application sub-module data */
    TI_HANDLE               hScanResultTable; /* application scan result table */
    TI_HANDLE               hOSScanSm; /* OS scan state machine */
    EScanCncnClient         eCurrentRunningAppScanClient; /* to disallow both one-shot and periodic app */
    TI_UINT32               uOSScanLastTimeStamp;
    TI_BOOL                 bOSScanRunning;
    TScanParams             tOsScanParams;

} TScanCncn;

#endif /* __SCAN_CNCN_PRIVATE_H__ */

