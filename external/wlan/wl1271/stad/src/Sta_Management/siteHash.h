/*
 * siteHash.h
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

/** \file siteHash.h
 *  \brief Hash & site table internal header file
 *
 *  \see siteHash.c
 */

/***************************************************************************/
/*                                                                          */
/*    MODULE:   siteHash.h                                                  */
/*    PURPOSE:  Hash & site table internal header file                      */
/*                                                                          */
/***************************************************************************/
#ifndef __SITE_MGR_H__
#define __SITE_MGR_H__

#include "tidef.h"
#include "paramOut.h"
#include "802_11Defs.h"
#include "DataCtrl_Api.h"
#include "scanResultTable.h"

#define MIN_TX_SESSION_COUNT    1
#define MAX_TX_SESSION_COUNT    7

/* A site entry contains all the site attribute received in beacon and probes
    and data used to manage the site table and hash table */
typedef TSiteEntry siteEntry_t;

typedef struct
{
    TI_UINT8           numOfSites;
    TI_UINT8           maxNumOfSites;
    siteEntry_t        siteTable[MAX_SITES_BG_BAND];
}siteTablesParams_t;

/* This struct is seperated from the above struct (siteTablesParams_t) for memory optimizations */
typedef struct
{
    TI_UINT8           numOfSites;
    TI_UINT8           maxNumOfSites;
    siteEntry_t        siteTable[MAX_SITES_A_BAND];
}siteTablesParamsBandA_t;

/* Ths following structure is used to manage the sites */
typedef struct
{
    siteTablesParamsBandA_t   dot11A_sitesTables;
    siteTablesParams_t        dot11BG_sitesTables;
    siteTablesParams_t        *pCurrentSiteTable;
    siteEntry_t               *pPrimarySite;
    siteEntry_t               *pPrevPrimarySite;
} sitesMgmtParams_t;


/* Site manager handle */
typedef struct
{
    siteMgrInitParams_t *pDesiredParams;
    sitesMgmtParams_t   *pSitesMgmtParams;

    TI_HANDLE           hConn;
    TI_HANDLE           hSmeSm;
    TI_HANDLE           hCtrlData;
    TI_HANDLE           hRxData;
    TI_HANDLE           hTxCtrl;
    TI_HANDLE           hRsn;
    TI_HANDLE           hAuth;
    TI_HANDLE           hAssoc;
    TI_HANDLE           hRegulatoryDomain;
    TI_HANDLE           hMeasurementMgr;
    TI_HANDLE           hTWD;
    TI_HANDLE           hMlmeSm;
    TI_HANDLE           hReport;
    TI_HANDLE           hOs;
    TI_HANDLE           hXCCMngr;
    TI_HANDLE           hApConn;
    TI_HANDLE           hCurrBss;
    TI_HANDLE           hQosMngr;
    TI_HANDLE           hPowerMgr;
    TI_HANDLE           hEvHandler;
    TI_HANDLE           hScr;
    TI_HANDLE           hStaCap;

    TI_UINT32           beaconSentCount;
    TI_UINT32           rxPacketsCount;
    TI_UINT32           txPacketsCount;
    TI_UINT16           txSessionCount;     /* Current Tx-Session index as configured to FW in last Join command. */

    EModulationType     chosenModulation;
    EModulationType     currentDataModulation;
    EDot11Mode          siteMgrOperationalMode;
    ERadioBand          radioBand;
    ERadioBand          prevRadioBand;
    
    TMacAddr            ibssBssid;
    TI_BOOL             bPostponedDisconnectInProgress;
    TI_BOOL             isAgingEnable;

    /* TX Power Adjust */
    TI_UINT32           siteMgrTxPowerCheckTime;
    TI_BOOL             siteMgrTxPowerEnabled;

    TBeaconFilterInitParams    beaconFilterParams; /*contains the desired state*/


    /*HW Request from Power Ctrl */
    TI_UINT32           DriverTestId;

    /* Wifi Simple Config */
    TIWLN_SIMPLE_CONFIG_MODE  siteMgrWSCCurrMode; /* indicates the current WiFi Simple Config mode */
    TI_UINT32           uWscIeSize; 			  /* Simple Config IE actual size (the part after the OUI) */
    char                siteMgrWSCProbeReqParams[DOT11_WSC_PROBE_REQ_MAX_LENGTH]; /* Contains the params to be used in the ProbeReq - WSC IE */ 

    TI_UINT8            includeWSCinProbeReq;
} siteMgr_t;



siteEntry_t *findAndInsertSiteEntry(siteMgr_t       *pSiteMgr,
                                    TMacAddr    *bssid,
                                    ERadioBand     band);

siteEntry_t *findSiteEntry(siteMgr_t        *pSiteMgr,
                           TMacAddr     *bssid);

void removeSiteEntry(siteMgr_t *pSiteMgr, siteTablesParams_t *pCurrSiteTblParams,
                     siteEntry_t  *hashPtr);

TI_STATUS removeEldestSite(siteMgr_t *pSiteMgr);

TI_STATUS buildProbeReqTemplate(siteMgr_t *pSiteMgr, TSetTemplate *pTemplate, TSsid *pSsid, ERadioBand radioBand);

TI_STATUS buildProbeRspTemplate(siteMgr_t *pSiteMgr, TSetTemplate *pTemplate);

TI_STATUS buildNullTemplate(siteMgr_t *pSiteMgr, TSetTemplate *pTemplate);

TI_STATUS buildArpRspTemplate(siteMgr_t *pSiteMgr, TSetTemplate *pTemplate, TIpAddr staIp);

TI_STATUS buildDisconnTemplate(siteMgr_t *pSiteMgr, TSetTemplate *pTemplate);

TI_STATUS buildPsPollTemplate(siteMgr_t *pSiteMgr, TSetTemplate *pTemplate);

TI_STATUS buildQosNullDataTemplate(siteMgr_t *pSiteMgr, TSetTemplate *pTemplate, TI_UINT8 userPriority);

void setDefaultProbeReqTemplate (TI_HANDLE	hSiteMgr);

#endif /* __SITE_MGR_H__ */
