/*
 * scanResultTable.h
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

/** \file  scanResultTable.h
 *  \brief declarations for a table holding scan results, by BSSID
 *
 *  \see   scanResultTable.c
 */


#ifndef __SCAN_RESULT_TABLE_H__
#define __SCAN_RESULT_TABLE_H__

#include "osTIType.h"
#include "tidef.h"
#include "ScanCncn.h"
#include "DrvMainModules.h"

/* site types */
typedef enum
{
    SITE_PRIMARY        = 0,
    SITE_SELF           = 1,
    SITE_REGULAR        = 2,
    SITE_NULL           = 3
} siteType_e;

typedef struct
{
    /* The following fields are used for entry management at the SiteMng */
    TI_UINT8                   index;
    siteType_e                 siteType;
    TI_UINT32                  localTimeStamp;
    /* end of fields  are used for entry management at the SiteMng */

    TI_BOOL                    bConsideredForSelect;
    ERadioBand                 eBand;   
    TI_UINT8                   tsfTimeStamp[ TIME_STAMP_LEN ];

    /* The following fields are used for the selection */
    TI_BOOL                    probeRecv;
    TI_BOOL                    beaconRecv;
    TMacAddr                   bssid;
    TSsid                      ssid;
    ScanBssType_e              bssType;
    rateMask_t                 rateMask;
    ERate                      maxBasicRate;
    ERate                      maxActiveRate;
    EModulationType            beaconModulation;
    EModulationType            probeModulation;
    EPreamble                  currentPreambleType;
    EPreamble                  preambleAssRspCap;
    EPreamble                  barkerPreambleType;
    ESlotTime                  currentSlotTime;
    ESlotTime                  newSlotTime;
    TI_BOOL                    useProtection;
    TI_BOOL                    NonErpPresent;
    TI_UINT8                   channel;
    TI_BOOL                    privacy;
    TI_BOOL                    agility;
    TI_UINT16                  capabilities;
    TI_UINT16                  beaconInterval;
    TI_UINT8                   dtimPeriod;
    TI_INT8                    snr;
    ERate                      rxRate;
    TI_INT32                   rssi;

    /* HT capabilites */
	Tdot11HtCapabilitiesUnparse tHtCapabilities;
	/* HT information */
    TI_BOOL                     bHtInfoUpdate;
	Tdot11HtInformationUnparse  tHtInformation;

    /* QOS */
    TI_BOOL                    WMESupported;
    dot11_ACParameters_t       WMEParameters;
    TI_UINT8                   lastWMEParameterCnt;

    /* Power Constraint */
    TI_UINT8                   powerConstraint;

    /* AP Tx Power obtained from TPC Report */
    TI_UINT8                   APTxPower;

    /* UPSD */
    TI_BOOL                    APSDSupport;

    /* WiFi Simple Config */
    TIWLN_SIMPLE_CONFIG_MODE   WSCSiteMode; /* indicates the current WiFi Simple Config mode of the specific site*/

    TI_UINT16                  atimWindow;
    dot11_RSN_t                pRsnIe[MAX_RSN_IE];
    TI_UINT8                   rsnIeLen;

    /* 80211h beacon  - Switch Channel IE included */
    TI_BOOL                    bChannelSwitchAnnoncIEFound;

	TI_UINT8                   pUnknownIe[MAX_BEACON_BODY_LENGTH];
    TI_UINT16                  unknownIeLen;

    mgmtStatus_e               failStatus;
    TI_BOOL                    prioritySite;
    TI_UINT8                   probeRespBuffer[ MAX_BEACON_BODY_LENGTH ];
    TI_UINT16                  probeRespLength;
    TI_UINT8                   beaconBuffer[ MAX_BEACON_BODY_LENGTH ];
    TI_UINT16                  beaconLength;

} TSiteEntry;


typedef enum 
{
    SCAN_RESULT_TABLE_DONT_CLEAR,
    SCAN_RESULT_TABLE_CLEAR

} EScanResultTableClear;

TI_HANDLE   scanResultTable_Create (TI_HANDLE hOS, TI_UINT32 uEntriesNumber);
void        scanResultTable_Init (TI_HANDLE hScanResultTable, TStadHandlesList *pStadHandles, EScanResultTableClear eClearTable);
void        scanResultTable_Destroy (TI_HANDLE hScanResultTable);
TI_STATUS   scanResultTable_UpdateEntry (TI_HANDLE hScanResultTable, TMacAddr *pBssid, TScanFrameInfo* pFrame);
void        scanResultTable_SetStableState (TI_HANDLE hScanResultTable);
TSiteEntry  *scanResultTable_GetFirst (TI_HANDLE hScanResultTable);
TSiteEntry  *scanResultTable_GetNext (TI_HANDLE hScanResultTable);
TSiteEntry  *scanResultTable_GetBySsidBssidPair (TI_HANDLE hScanResultTable, TSsid *pSsid, TMacAddr *pBssid);
TI_UINT32   scanResultTable_CalculateBssidListSize (TI_HANDLE hScanResultTable, TI_BOOL bAllVarIes);
TI_UINT32 scanResultTable_GetNumOfBSSIDInTheList (TI_HANDLE hScanResultTable);
TI_STATUS   scanResultTable_GetBssidList (TI_HANDLE hScanResultTable, OS_802_11_BSSID_LIST_EX *pBssidList, 
                                          TI_UINT32 *pLength, TI_BOOL bAllVarIes);
TI_STATUS scanResultTable_GetBssidSupportedRatesList (TI_HANDLE hScanResultTable, OS_802_11_N_RATES *pRateList, TI_UINT32 *pLength);

void        scanResultTable_PerformAging(TI_HANDLE hScanResultTable);
void        scanResultTable_SetSraThreshold(TI_HANDLE hScanResultTable, TI_UINT32 uSraThreshold);
#endif /* __SCAN_RESULT_TABLE_H__ */

