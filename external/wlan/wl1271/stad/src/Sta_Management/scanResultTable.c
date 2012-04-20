/*
 * scanResultTable.c
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

/** \file  scanResultTable.c 
 *  \brief implements a table holding scan results, by BSSID
 *
 *  \see   scanResultTable.h
 */


#define __FILE_ID__  FILE_ID_81
#include "osApi.h"
#include "report.h"
#include "scanResultTable.h"
#include "siteMgrApi.h"
#include "freq.h"


//#define TABLE_ENTRIES_NUMBER    32

#define MILISECONDS(seconds)                            (seconds * 1000)
#define UPDATE_LOCAL_TIMESTAMP(pSite, hOs)              pSite->localTimeStamp = os_timeStampMs(hOs);

#define UPDATE_BSSID(pSite, pFrame)                     MAC_COPY((pSite)->bssid, *((pFrame)->bssId))
#define UPDATE_BAND(pSite, pFrame)                      (pSite)->eBand = (pFrame)->band
#define UPDATE_BEACON_INTERVAL(pSite, pFrame)           pSite->beaconInterval = (pFrame)->parsedIEs->content.iePacket.beaconInerval
#define UPDATE_CAPABILITIES(pSite, pFrame)              pSite->capabilities = (pFrame)->parsedIEs->content.iePacket.capabilities
#define UPDATE_PRIVACY(pSite, pFrame)                   pSite->privacy = (((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_PRIVACY_SHIFT) & CAP_PRIVACY_MASK) ? TI_TRUE : TI_FALSE
#define UPDATE_AGILITY(pSite, pFrame)                   pSite->agility = (((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_AGILE_SHIFT) & CAP_AGILE_MASK) ? TI_TRUE : TI_FALSE
#define UPDATE_SLOT_TIME(pSite, pFrame)                 pSite->newSlotTime = (((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_SLOT_TIME_SHIFT) & CAP_SLOT_TIME_MASK) ? PHY_SLOT_TIME_SHORT : PHY_SLOT_TIME_LONG
#define UPDATE_PROTECTION(pSite, pFrame)                pSite->useProtection = ((pFrame)->parsedIEs->content.iePacket.useProtection)
#define UPDATE_CHANNEL(pSite, pFrame, rxChannel)        if ((pFrame)->parsedIEs->content.iePacket.pDSParamsSet == NULL) \
                                                            pSite->channel = rxChannel; \
                                                        else \
                                                            pSite->channel = (pFrame)->parsedIEs->content.iePacket.pDSParamsSet->currChannel;
#define UPDATE_DTIM_PERIOD(pSite, pFrame)               if ((pFrame)->parsedIEs->content.iePacket.pTIM != NULL) \
                                                            pSite->dtimPeriod = (pFrame)->parsedIEs->content.iePacket.pTIM->dtimPeriod
#define UPDATE_ATIM_WINDOW(pSite, pFrame)               if ((pFrame)->parsedIEs->content.iePacket.pIBSSParamsSet != NULL) \
                                                            pSite->atimWindow = (pFrame)->parsedIEs->content.iePacket.pIBSSParamsSet->atimWindow
#define UPDATE_AP_TX_POWER(pSite, pFrame)               if ((pFrame)->parsedIEs->content.iePacket.TPCReport != NULL) \
                                                            pSite->APTxPower = (pFrame)->parsedIEs->content.iePacket.TPCReport->transmitPower
#define UPDATE_BSS_TYPE(pSite, pFrame)                  pSite->bssType = (((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_ESS_SHIFT) & CAP_ESS_MASK) ? BSS_INFRASTRUCTURE : BSS_INDEPENDENT
#define UPDATE_RSN_IE(pScanResultTable, pSite, pNewRsnIe, newRsnIeLen) if ((pNewRsnIe) != NULL) { \
                                                            TI_UINT8 length=0, index=0;\
                                                            dot11_RSN_t *pTempRsnIe = (pNewRsnIe); \
                                                            (pSite)->rsnIeLen = (newRsnIeLen);\
                                                            while ((length < (pSite)->rsnIeLen) && (index < MAX_RSN_IE)) {\
                                                                (pSite)->pRsnIe[index].hdr[0] = pTempRsnIe->hdr[0];\
                                                                (pSite)->pRsnIe[index].hdr[1] = pTempRsnIe->hdr[1];\
                                                                os_memoryCopy(pScanResultTable->hOS, (void *)(pSite)->pRsnIe[index].rsnIeData, (void *)pTempRsnIe->rsnIeData, pTempRsnIe->hdr[1]);\
                                                                length += (pTempRsnIe->hdr[1]+2); \
                                                                pTempRsnIe += 1; \
                                                                index++;}\
                                                        } \
                                                        else {pSite->rsnIeLen = 0;}
#define UPDATE_BEACON_TIMESTAMP(pScanResultTable, pSite, pFrame)    os_memoryCopy(pScanResultTable->hOS, pSite->tsfTimeStamp, (void *)(pFrame)->parsedIEs->content.iePacket.timestamp, TIME_STAMP_LEN)

/* Updated from beacons */
#define UPDATE_BEACON_MODULATION(pSite, pFrame)         pSite->beaconModulation = (((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_PBCC_SHIFT) & CAP_PBCC_MASK) ? DRV_MODULATION_PBCC : DRV_MODULATION_CCK
#define UPDATE_BEACON_RECV(pSite)                       pSite->beaconRecv = TI_TRUE

/* Updated from probes */
#define UPDATE_PROBE_MODULATION(pSite, pFrame)          pSite->probeModulation = (((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_PBCC_SHIFT) & CAP_PBCC_MASK) ? DRV_MODULATION_PBCC : DRV_MODULATION_CCK
#define UPDATE_PROBE_RECV(pSite)                        pSite->probeRecv = TI_TRUE
#define UPDATE_APSD(pSite, pFrame)                      if ((pFrame)->parsedIEs->content.iePacket.WMEParams == NULL) \
                                                                (pSite)->APSDSupport = ((((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_APSD_SHIFT) & CAP_APSD_MASK) ? TI_TRUE : TI_FALSE); \
                                                        else \
                                                            pSite->APSDSupport = (((((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_APSD_SHIFT) & CAP_APSD_MASK) ? TI_TRUE : TI_FALSE) || \
                                                                                  ((((pFrame)->parsedIEs->content.iePacket.WMEParams->ACInfoField >> AP_QOS_INFO_UAPSD_SHIFT) & AP_QOS_INFO_UAPSD_MASK) ? TI_TRUE : TI_FALSE));
#define UPDATE_PREAMBLE(pSite, pFrame)                  { (pSite)->currentPreambleType = (((pFrame)->parsedIEs->content.iePacket.capabilities >> CAP_PREAMBLE_SHIFT) & CAP_PREAMBLE_MASK) ? PREAMBLE_SHORT : PREAMBLE_LONG; \
                                                          (pSite)->barkerPreambleType = (pFrame)->parsedIEs->content.iePacket.barkerPreambleMode; }
#define UPDATE_QOS(pSite, pFrame)                       if ( ((pFrame)->parsedIEs->content.iePacket.WMEParams  != NULL) && \
                                                             (((((pFrame)->parsedIEs->content.iePacket.WMEParams->ACInfoField) & dot11_WME_ACINFO_MASK) != pSite->lastWMEParameterCnt) || (!pSite->WMESupported)) ) \
                                                            pSite->WMESupported = TI_TRUE; \
                                                        else \
                                                            pSite->WMESupported = TI_FALSE;
#define UPDATE_FRAME_BUFFER(pScanResultTable, pBuffer, uLength, pFrame)  if (pFrame->bufferLength < MAX_BEACON_BODY_LENGTH) \
                                                        { \
                                                           os_memoryCopy (pScanResultTable->hOS, pBuffer, pFrame->buffer, pFrame->bufferLength); \
                                                           uLength = pFrame->bufferLength; \
                                                        }
#define UPDATE_RSSI(pSite, pFrame)                      (pSite)->rssi = (pFrame)->rssi;
#define UPDATE_SNR(pSite, pFrame)                       (pSite)->snr = (pFrame)->snr;
#define UPDATE_RATE(pSite, pFrame)                      if ((DRV_RATE_1M <= (pFrame)->rate) && (DRV_RATE_54M <= (pFrame)->rate)) \
                                                            (pSite)->rxRate = (pFrame)->rate;
#define UPDATE_UNKOWN_IE(pScanResultTable, pSite, pNewUnknwonIe, newUnknwonIeLen) if ((pNewUnknwonIe) != NULL) { \
                                                                                    pSite->unknownIeLen = newUnknwonIeLen; \
                                                                                    os_memoryCopy(pScanResultTable->hOS, \
                                                                                                  (void *)(pSite->pUnknownIe), \
                                                                                                  pNewUnknwonIe, \
                                                                                                  newUnknwonIeLen);	\
                                                                                  } else { \
                                                                                 	pSite->unknownIeLen = 0; \
                                                                                  }


typedef struct
{
    TI_HANDLE       hOS;                    /**< Handle to the OS object */
    TI_HANDLE       hReport;                /**< handle to the report object */
    TI_HANDLE       hSiteMgr;               /**< Handle to the site manager object */
    TSiteEntry      *pTable;                /**< site table */
    TI_UINT32       uCurrentSiteNumber;     /**< number of sites currently in the table */
    TI_UINT32       uEntriesNumber;         /**< max size of the table */
    TI_UINT32       uIterator;              /**< table iterator used for getFirst / getNext */
    TI_UINT32       uSraThreshold;          /**< Rssi threshold for frame filtering */
    TI_BOOL         bStable;                /**< table status (updating / stable) */
    EScanResultTableClear  eClearTable;     /** inicates if table should be cleared at scan */
} TScanResultTable;

static TSiteEntry  *scanResultTbale_AllocateNewEntry (TI_HANDLE hScanResultTable);
static void         scanResultTable_UpdateSiteData (TI_HANDLE hScanResultTable, TSiteEntry *pSite, TScanFrameInfo *pFrame);
static void         scanResultTable_updateRates(TI_HANDLE hScanResultTable, TSiteEntry *pSite, TScanFrameInfo *pFrame);
static void         scanResultTable_UpdateWSCParams (TSiteEntry *pSite, TScanFrameInfo *pFrame);
static TI_STATUS    scanResultTable_CheckRxSignalValidity(TScanResultTable *pScanResultTable, siteEntry_t *pSite, TI_INT8 rxLevel, TI_UINT8 channel);
static void         scanResultTable_RemoveEntry(TI_HANDLE hScanResultTable, TI_UINT32 uIndex);


/** 
 * \fn     scanResultTable_Create 
 * \brief  Create a scan result table object.
 * 
 * Create a scan result table object. Allocate system resources.
 * 
 * \note   
 * \param  hOS - handle to the OS object
 * \return Handle to the newly created scan result table object, NULL if an error occured.
 * \sa     scanResultTable_Init, scanResultTable_Destroy
 */ 
TI_HANDLE scanResultTable_Create (TI_HANDLE hOS, TI_UINT32 uEntriesNumber)
{
    TScanResultTable    *pScanResultTable = NULL;

    /* Allocate object storage */
    pScanResultTable = (TScanResultTable*)os_memoryAlloc (hOS, sizeof(TScanResultTable));
    if (NULL == pScanResultTable)
    {
        /* because the malloc failure here the TRACEx can not be used (no pointer for the 1st parameter to TRACEx) */
        WLAN_OS_REPORT(("scanResultTable_Create: Unable to allocate memory for pScanResultTable of %d bytes\n", 
			  sizeof (TScanResultTable)));
        return NULL;  /* this is done similarly to the next error case */
    }

    pScanResultTable->hOS = hOS;
    /* allocate memory for sites' data */
    pScanResultTable->pTable = 
        (TSiteEntry *)os_memoryAlloc (pScanResultTable->hOS, sizeof (TSiteEntry) * uEntriesNumber);
    if (NULL == pScanResultTable->pTable)
    {
        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_ERROR , 
			   "scanResultTable_Create: Unable to allocate memory for %d entries of %d bytes\n", 
			   uEntriesNumber , sizeof (TSiteEntry));
        os_memoryFree(pScanResultTable->hOS, pScanResultTable, sizeof(TScanResultTable));
        return NULL;
    }
    pScanResultTable->uEntriesNumber = uEntriesNumber;
    os_memoryZero(pScanResultTable->hOS, pScanResultTable->pTable, sizeof(TSiteEntry) * uEntriesNumber);
    return (TI_HANDLE)pScanResultTable;
}

/** 
 * \fn     scanResultTable_Init 
 * \brief  Initializes the scan result table object 
 * 
 * Initializes the scan result table object. Set handles to other objects.
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  pStadHandles - modules handles table
 * \param  eClearTable - indicates if the table should be cleared, used when a frame arrives 
 *                       or setStable is called and the table is in stable state
 * \return None
 * \sa     scanResultTable_Create
 */ 
void        scanResultTable_Init (TI_HANDLE hScanResultTable, TStadHandlesList *pStadHandles, EScanResultTableClear eClearTable)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;

    /* set handles to other modules */
    pScanResultTable->hReport = pStadHandles->hReport;
    pScanResultTable->hSiteMgr = pStadHandles->hSiteMgr;

    /* initialize other parameters */
    pScanResultTable->uCurrentSiteNumber = 0;
    pScanResultTable->bStable = TI_TRUE;
    pScanResultTable->uIterator = 0;
    pScanResultTable->eClearTable = eClearTable;
    /* default Scan Result Aging threshold is 60 second */
    pScanResultTable->uSraThreshold = 60;
}


/** 
 * \fn     scanResultTable_Destroy 
 * \brief  Destroys the scan result table object
 * 
 * Destroys the scan result table object. Release system resources
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \return None
 * \sa     scanResultTable_Create 
 */ 
void        scanResultTable_Destroy (TI_HANDLE hScanResultTable)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;

    /* if the table memory has already been allocated */
    if (NULL != pScanResultTable->pTable)
    {
        /* free table memory */
        os_memoryFree (pScanResultTable->hOS, (void*)pScanResultTable->pTable, 
                       sizeof (TSiteEntry) * pScanResultTable->uEntriesNumber);
    }

    /* free scan result table object memeory */
    os_memoryFree (pScanResultTable->hOS, (void*)hScanResultTable, sizeof (TScanResultTable));
}

/** 
 * \fn     scanResultTable_SetSraThreshold
 * \brief  set Scan Result Aging threshold 
 *  
 * \param  hScanResultTable - handle to the scan result table object
 * \param  uSraThreshold - Scan Result Aging threshold
 * \return None
 * \sa     scanResultTable_performAging
 */ 
void scanResultTable_SetSraThreshold(TI_HANDLE hScanResultTable, TI_UINT32 uSraThreshold)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    pScanResultTable->uSraThreshold = uSraThreshold;
}

/** 
 * \fn     scanResultTable_UpdateEntry
 * \brief  Update or insert a site data. 
 * 
 * Update a site's data in the table if it already exists, or create an entry if the site doesn't exist.
 * If the table is in stable state, will move it to updating state and clear its contents if bClearTable
 * is eClearTable.
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  pBssid - a pointer to the site BSSID
 * \param  pframe - a pointer to the received frame data
 * \return TI_OK if entry was inseretd or updated successfuly, TI_NOK if table is full
 * \sa     scanResultTable_SetStableState
 */ 
TI_STATUS scanResultTable_UpdateEntry (TI_HANDLE hScanResultTable, TMacAddr *pBssid, TScanFrameInfo* pFrame)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    TSiteEntry          *pSite;
    TSsid               tTempSsid;

    TRACE6(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_UpdateEntry: Adding or updating BBSID: %02x:%02x:%02x:%02x:%02x:%02x\n", (*pBssid)[ 0 ], (*pBssid)[ 1 ], (*pBssid)[ 2 ], (*pBssid)[ 3 ], (*pBssid)[ 4 ], (*pBssid)[ 5 ]);

    /* check if the table is in stable state */
    if (TI_TRUE == pScanResultTable->bStable)
    {
        TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_UpdateEntry: table is stable, clearing table and moving to updating state\n");
        /* move the table to updating state */
        pScanResultTable->bStable = TI_FALSE;

        if (SCAN_RESULT_TABLE_CLEAR == pScanResultTable->eClearTable) 
        {
            /* clear table contents */
            pScanResultTable->uCurrentSiteNumber = 0;
        }
    }

    /* Verify that the SSID IE is available (if not return NOK) */
    if (NULL == pFrame->parsedIEs->content.iePacket.pSsid) 
    {
        TRACE6(pScanResultTable->hReport, REPORT_SEVERITY_WARNING, "scanResultTable_UpdateEntry: can't add site %02d:%02d:%02d:%02d:%02d:%02d"                                  " because SSID IE is NULL\n", pBssid[ 0 ], pBssid[ 1 ], pBssid[ 2 ], pBssid[ 3 ], pBssid[ 4 ], pBssid[ 5 ]);
        return TI_NOK;
    }

    /* use temporary SSID structure, and verify SSID length */
    tTempSsid.len = pFrame->parsedIEs->content.iePacket.pSsid->hdr[1];
    if (tTempSsid.len > MAX_SSID_LEN)
    {
        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_WARNING, "scanResultTable_UpdateEntry: SSID len=%d out of range. replaced by %d\n", tTempSsid.len, MAX_SSID_LEN);
        return TI_NOK;
    }
    os_memoryCopy(pScanResultTable->hOS, 
                  (void *)&(tTempSsid.str[ 0 ]), 
                  (void *)&(pFrame->parsedIEs->content.iePacket.pSsid->serviceSetId[ 0 ]),
                  tTempSsid.len);
    if (MAX_SSID_LEN > tTempSsid.len)
        tTempSsid.str[ tTempSsid.len ] ='\0';

    /* check if the SSID:BSSID pair already exists in the table */
    pSite = scanResultTable_GetBySsidBssidPair (hScanResultTable, &tTempSsid ,pBssid);
    if (NULL != pSite)
    {
        if (TI_NOK != scanResultTable_CheckRxSignalValidity(pScanResultTable, pSite, pFrame->rssi, pFrame->channel))
        {
            TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_UpdateEntry: entry already exists, updating\n");
            /* BSSID exists: update its data */
            scanResultTable_UpdateSiteData (hScanResultTable, pSite, pFrame);
        }
    }
    else
    {
        TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_UpdateEntry: entry doesn't exist, allocating a new entry\n");
        /* BSSID doesn't exist: allocate a new entry for it */
        pSite = scanResultTbale_AllocateNewEntry (hScanResultTable);
        if (NULL == pSite)
        {
            TRACE6(pScanResultTable->hReport, REPORT_SEVERITY_WARNING , "scanResultTable_UpdateEntry: can't add site %02d:%02d:%02d:%02d:%02d:%02d"                                  " because table is full\n", pBssid[ 0 ], pBssid[ 1 ], pBssid[ 2 ], pBssid[ 3 ], pBssid[ 4 ], pBssid[ 5 ]);
            return TI_NOK;
        }

        /* and update its data */
        scanResultTable_UpdateSiteData (hScanResultTable, 
                                        pSite,
                                        pFrame);
    }

    return TI_OK;
}

/** 
 * \fn     scanResultTable_SetStableState 
 * \brief  Moves the table to stable state 
 * 
 * Moves the table to stable state. Also clears the tabel contents if required.
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  eClearTable - indicates if the table should be cleared in case the table
 *                       is in stable state (no result where received in last scan).
 * \return None
 * \sa     scanResultTable_UpdateEntry 
 */ 
void    scanResultTable_SetStableState (TI_HANDLE hScanResultTable)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;

    TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_SetStableState: setting stable state\n");

    /* if also asked to clear the table, if it is at Stable mode means that no results were received, clear it! */
    if ((TI_TRUE == pScanResultTable->bStable) && (SCAN_RESULT_TABLE_CLEAR == pScanResultTable->eClearTable))
    {
        TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_SetStableState: also clearing table contents\n");

        pScanResultTable->uCurrentSiteNumber = 0;
    }

    /* set stable state */
    pScanResultTable->bStable = TI_TRUE;

}

/** 
 * \fn     scanResultTable_GetFirst 
 * \brief  Retrieves the first entry in the table 
 * 
 * Retrieves the first entry in the table
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \return A pointer to the first entry in the table, NULL if the table is empty
 * \sa     scanResultTable_GetNext
 */ 
TSiteEntry  *scanResultTable_GetFirst (TI_HANDLE hScanResultTable)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;

    /* initialize the iterator to point at the first site */
    pScanResultTable->uIterator = 0;

    /* and return the next entry... */
    return scanResultTable_GetNext (hScanResultTable);
}

/** 
 * \fn     scanResultTable_GetNext
 * \brief  Retreives the next entry in the table
 * 
 * Retreives the next entry in the table, until table is exhusted. A call to scanResultTable_GetFirst
 * must preceed a sequence of calls to this function.
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \return A pointer to the next entry in the table, NULL if the table is exhsuted
 * \sa     scanResultTable_GetFirst
 */ 
TSiteEntry  *scanResultTable_GetNext (TI_HANDLE hScanResultTable)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;

    /* if the iterator points to a site behind current table storage, return error */
    if (pScanResultTable->uCurrentSiteNumber <= pScanResultTable->uIterator)
    {
        return NULL;
    }

    return &(pScanResultTable->pTable[ pScanResultTable->uIterator++ ]);
}

/** 
 * \fn     scanResultTable_GetByBssid
 * \brief  retreives an entry according to its SSID and BSSID
 * 
 * retreives an entry according to its BSSID
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  pSsid - SSID to search for
 * \param  pBssid - BSSID to search for
 * \return A pointer to the entry with macthing BSSID, NULL if no such entry was found. 
 */ 
TSiteEntry  *scanResultTable_GetBySsidBssidPair (TI_HANDLE hScanResultTable, TSsid *pSsid, TMacAddr *pBssid)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    TI_UINT32           uIndex;

    TRACE6(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBySsidBssidPair: Searching for SSID  BSSID %02x:%02x:%02x:%02x:%02x:%02x\n", (*pBssid)[ 0 ], (*pBssid)[ 1 ], (*pBssid)[ 2 ], (*pBssid)[ 3 ], (*pBssid)[ 4 ], (*pBssid)[ 5 ]);
    
    /* check all entries in the table */
    for (uIndex = 0; uIndex < pScanResultTable->uCurrentSiteNumber; uIndex++)
    {
        /* if the BSSID and SSID match */
        if (MAC_EQUAL (*pBssid, pScanResultTable->pTable[ uIndex ].bssid) &&
            ((pSsid->len == pScanResultTable->pTable[ uIndex ].ssid.len) &&
             (0 == os_memoryCompare (pScanResultTable->hOS, (TI_UINT8 *)(&(pSsid->str[ 0 ])),
                                     (TI_UINT8 *)(&(pScanResultTable->pTable[ uIndex ].ssid.str[ 0 ])),
                                     pSsid->len))))
        {
            TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "Entry found at index %d\n", uIndex);
            return &(pScanResultTable->pTable[ uIndex ]);
        }
    }

    /* site wasn't found: return NULL */
    TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBySsidBssidPair: Entry was not found\n");
    return NULL;
}

/** 
 * \fn     scanResultTable_FindHidden
 * \brief  find entry with hidden SSID anfd return it's index
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  uHiddenSsidIndex - entry index to return
 * \return TI_OK if found, TI_NOT if not. 
 */ 
static TI_STATUS scanResultTable_FindHidden (TScanResultTable *pScanResultTable, TI_UINT32 *uHiddenSsidIndex)
{
    TI_UINT32 uIndex;

    TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_FindHidden: Searching for hidden SSID\n");
    
    /* check all entries in the table */
    for (uIndex = 0; uIndex < pScanResultTable->uCurrentSiteNumber; uIndex++)
    {
        /* check if entry is with hidden SSID */
        if ( (pScanResultTable->pTable[ uIndex ].ssid.len == 0) ||
            ((pScanResultTable->pTable[ uIndex ].ssid.len == 1) && (pScanResultTable->pTable[ uIndex ].ssid.str[0] == 0)))
        {
            TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_FindHidden: Entry found at index %d\n", uIndex);
            *uHiddenSsidIndex = uIndex;
            return TI_OK;
        }
    }

    /* site wasn't found: return NULL */
    TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_FindHidden: Entry was not found\n");
    return TI_NOK;
}

/** 
 * \fn     scanResultTable_performAging 
 * \brief  Deletes from table all entries which are older than the Sra threshold
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \return None
 * \sa     scanResultTable_SetSraThreshold
 */ 
void   scanResultTable_PerformAging(TI_HANDLE hScanResultTable)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    TI_UINT32           uIndex = 0;

    /* check all entries in the table */
    while (uIndex < pScanResultTable->uCurrentSiteNumber)
    {
        /* check if the entry's age is old if it remove it */
        if (pScanResultTable->pTable[uIndex].localTimeStamp < 
            os_timeStampMs(pScanResultTable->hOS) - MILISECONDS(pScanResultTable->uSraThreshold))
        {
            /* The removeEntry places the last entry instead of the deleted entry
             * in order to preserve the table's continuity. For this reason the
             * uIndex is not incremented because we want to check the entry that 
             * was placed instead of the entry deleted */
            scanResultTable_RemoveEntry(hScanResultTable, uIndex);
        }
        else
        {
            /* If the entry was not deleted check the next entry */
            uIndex++;
        }
    }
}

/** 
 * \fn     scanResultTable_removeEntry 
 * \brief  Deletes entry from table
 *         the function keeps a continuty in the table by copying the last
 *         entry in the table to the place the entry was deleted from
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  uIndex           - index of the entry to be deleted
 * \return TI_OK if entry deleted successfully TI_NOK otherwise
 */ 
void   scanResultTable_RemoveEntry(TI_HANDLE hScanResultTable, TI_UINT32 uIndex)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;

    if (uIndex >= pScanResultTable->uCurrentSiteNumber) 
    {
        TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_ERROR , "scanResultTable_removeEntry: %d out of bound entry index\n", uIndex);
        return;
    }

    /* if uIndex is not the last entry, then copy the last entry in the table to the uIndex entry */
    if (uIndex < (pScanResultTable->uCurrentSiteNumber - 1))
    {
        os_memoryCopy(pScanResultTable->hOS, 
                      &(pScanResultTable->pTable[uIndex]), 
                      &(pScanResultTable->pTable[pScanResultTable->uCurrentSiteNumber - 1]),
                      sizeof(TSiteEntry));
    }

    /* clear the last entry */
    os_memoryZero(pScanResultTable->hOS, &(pScanResultTable->pTable[pScanResultTable->uCurrentSiteNumber - 1]), sizeof(TSiteEntry));
    /* decrease the current table size */
    pScanResultTable->uCurrentSiteNumber--;
}

/** 
 * \fn     scanresultTbale_AllocateNewEntry 
 * \brief  Allocates an empty entry for a new site
 * 
 * Function Allocates an empty entry for a new site (and nullfiies required entry fields)
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \return Pointer to the site entry (NULL if the table is full)
 */ 
TSiteEntry *scanResultTbale_AllocateNewEntry (TI_HANDLE hScanResultTable)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    TI_UINT32 uHiddenSsidIndex;

    /* if the table is full */
    if (pScanResultTable->uCurrentSiteNumber >= pScanResultTable->uEntriesNumber)
    {
        /* replace hidden SSID entry with the new result */
        if (scanResultTable_FindHidden(pScanResultTable, &uHiddenSsidIndex) == TI_OK)
        {
            TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTbale_AllocateNewEntry: Table is full, found hidden SSID at index %d to replace with\n", uHiddenSsidIndex);
            
            /* Nullify new site data */
            os_memoryZero(pScanResultTable->hOS, &(pScanResultTable->pTable[ uHiddenSsidIndex ]), sizeof (TSiteEntry));

            /* return the site */
            return &(pScanResultTable->pTable[ uHiddenSsidIndex ]);
        }

        TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTbale_AllocateNewEntry: Table is full, no Hidden SSDI to replace, can't allocate new entry\n");
        return NULL;
    }

    TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTbale_AllocateNewEntry: New entry allocated at index %d\n", pScanResultTable->uCurrentSiteNumber);

    /* Nullify new site data */
    os_memoryZero(pScanResultTable->hOS, &(pScanResultTable->pTable[ pScanResultTable->uCurrentSiteNumber ]), sizeof (TSiteEntry));

    /* return the site (and update site count) */
    pScanResultTable->uCurrentSiteNumber++;
    return &(pScanResultTable->pTable[ pScanResultTable->uCurrentSiteNumber - 1 ]);
}

/** 
 * \fn     scanResultTable_UpdateSiteData 
 * \brief  Update a site entry data from a received frame (beacon or probe response)
 * 
 * Update a site entry data from a received frame (beacon or probe response)
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  pSite - the site entry to update
 * \param  pFrame - the received frame information
 * \return None
 */ 
void scanResultTable_UpdateSiteData (TI_HANDLE hScanResultTable, TSiteEntry *pSite, TScanFrameInfo *pFrame)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    paramInfo_t         param;

    /* Update SSID */
    if (pFrame->parsedIEs->content.iePacket.pSsid != NULL) 
    {
        pSite->ssid.len = pFrame->parsedIEs->content.iePacket.pSsid->hdr[1];
        if (pSite->ssid.len > MAX_SSID_LEN) 
        {
           TRACE2( pScanResultTable->hReport, REPORT_SEVERITY_ERROR, "scanResultTable_UpdateSiteData: pSite->ssid.len=%d exceeds the limit. Set to limit value %d\n", pSite->ssid.len, MAX_SSID_LEN);
           pSite->ssid.len = MAX_SSID_LEN;
        }
        os_memoryCopy(pScanResultTable->hOS,
            (void *)pSite->ssid.str,
            (void *)pFrame->parsedIEs->content.iePacket.pSsid->serviceSetId,
            pSite->ssid.len);
        if (pSite->ssid.len < MAX_SSID_LEN) 
        {
            pSite->ssid.str[pSite->ssid.len] = '\0';
        }
    }

	/* Since a new scan was initiated the entry can be selected again */
	pSite->bConsideredForSelect = TI_FALSE;
    UPDATE_LOCAL_TIMESTAMP(pSite, pScanResultTable->hOS);

    UPDATE_BSSID (pSite, pFrame);
    UPDATE_BAND (pSite, pFrame);
    UPDATE_BEACON_INTERVAL (pSite, pFrame);
    UPDATE_CAPABILITIES (pSite, pFrame);
    UPDATE_PRIVACY (pSite, pFrame);
    UPDATE_RSN_IE (pScanResultTable, pSite, pFrame->parsedIEs->content.iePacket.pRsnIe, pFrame->parsedIEs->content.iePacket.rsnIeLen);
    UPDATE_APSD (pSite, pFrame);
    UPDATE_PREAMBLE (pSite, pFrame);
    UPDATE_AGILITY (pSite, pFrame);
    UPDATE_RSSI (pSite, pFrame);
    UPDATE_SNR (pSite, pFrame);
    UPDATE_RATE (pSite, pFrame);
	UPDATE_UNKOWN_IE(pScanResultTable, pSite, pFrame->parsedIEs->content.iePacket.pUnknownIe, pFrame->parsedIEs->content.iePacket.unknownIeLen );

    param.paramType = SITE_MGR_OPERATIONAL_MODE_PARAM;
    siteMgr_getParam (pScanResultTable->hSiteMgr, &param);
    if (param.content.siteMgrDot11OperationalMode == DOT11_G_MODE)
    {
        UPDATE_SLOT_TIME (pSite, pFrame);
        UPDATE_PROTECTION (pSite, pFrame);
    }

    scanResultTable_updateRates (hScanResultTable, pSite, pFrame);

    if ((pFrame->parsedIEs->content.iePacket.pDSParamsSet != NULL)  &&
        (pFrame->parsedIEs->content.iePacket.pDSParamsSet->currChannel != pFrame->channel))
    {
        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_ERROR , "scanResultTable_UpdateSiteData: wrong channels, radio channel=%d, frame channel=%d\n", pFrame->channel, pFrame->parsedIEs->content.iePacket.pDSParamsSet->currChannel);
    }
    else
        UPDATE_CHANNEL (pSite, pFrame , pFrame->channel);

    UPDATE_BSS_TYPE (pSite, pFrame);
    UPDATE_ATIM_WINDOW (pSite, pFrame);
    UPDATE_AP_TX_POWER (pSite, pFrame);
    UPDATE_QOS (pSite, pFrame);
    UPDATE_BEACON_TIMESTAMP (pScanResultTable, pSite, pFrame);
    scanResultTable_UpdateWSCParams (pSite, pFrame);
    siteMgr_UpdatHtParams (pScanResultTable->hSiteMgr, pSite, pFrame->parsedIEs);

    if (BEACON == pFrame->parsedIEs->subType)
    {
        /* DTIM is only available in beacons */
        if (pSite->bssType == BSS_INFRASTRUCTURE)
        {
            UPDATE_DTIM_PERIOD (pSite, pFrame);
        }

        UPDATE_BEACON_MODULATION (pSite, pFrame);

        /* If the BSS type is independent, the beacon & probe modulation are equal,
            It is important to update this field here for dynamic PBCC algorithm compatibility */
        if (pSite->bssType == BSS_INDEPENDENT)
        {
            UPDATE_PROBE_MODULATION (pSite, pFrame);
        }

        pSite->bChannelSwitchAnnoncIEFound = (pFrame->parsedIEs->content.iePacket.channelSwitch != NULL)?TI_TRUE:TI_FALSE;

        UPDATE_BEACON_RECV (pSite);
        UPDATE_FRAME_BUFFER (pScanResultTable, (pSite->beaconBuffer), (pSite->beaconLength), pFrame); 
    }
    else if (PROBE_RESPONSE == pFrame->parsedIEs->subType)
    {
        UPDATE_PROBE_MODULATION (pSite, pFrame);

        /* If the BSS type is independent, the beacon & probe modulation are equal,
            It is important to update this field here for dynamic PBCC algorithm compatibility */
        if (pSite->bssType == BSS_INDEPENDENT)
            UPDATE_BEACON_MODULATION (pSite, pFrame);

        UPDATE_PROBE_RECV (pSite);
        UPDATE_FRAME_BUFFER (pScanResultTable, (pSite->probeRespBuffer), (pSite->probeRespLength), pFrame);

        pSite->bChannelSwitchAnnoncIEFound = TI_FALSE;
    }
    else
    {
        TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_ERROR , "scanResultTable_UpdateSiteData: unknown frame sub type %d\n", pFrame->parsedIEs->subType);
    }
}

/** 
 * \fn     scanResultTable_updateRates 
 * \brief  Update a scan result table entry with rates information 
 * 
 * Called by the function 'updateSiteInfo()' in order to translate the rates received
 * in the beacon or probe response to rate used by the driver. Perfoms the following:
 *    -   Check the rates. validity. If rates are invalid, return
 *    -   Get the max active rate & max basic rate, if invalid, return
 *    -   Translate the max active rate and max basic rate from network rates to host rates.
 *        The max active & max basic rate are used by the driver from now on in all the processes:
 *        (selection, join, transmission, etc....)
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  pSite - a pointer to the site entry to update 
 * \param  pFrame - a pointer to the received frame
 * \return None
 * \sa     scanResultTable_UpdateSiteData 
 */ 
void scanResultTable_updateRates(TI_HANDLE hScanResultTable, TSiteEntry *pSite, TScanFrameInfo *pFrame)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    TI_UINT8            maxBasicRate = 0, maxActiveRate = 0;
    TI_UINT32           bitMapExtSupp = 0;
	TI_UINT32           uMcsSupportedRateMask = 0, uMcsbasicRateMask = 0;

    if (pFrame->parsedIEs->content.iePacket.pRates == NULL)
    {
        TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_ERROR, "scanResultTable_updateRates, pRates=NULL, beacon & probeResp are:\n");
        TRACE_INFO_HEX(pScanResultTable->hReport, (TI_UINT8*)pFrame->parsedIEs->content.iePacket.pRates, pFrame->parsedIEs->content.iePacket.pRates->hdr[1]+2);
        TRACE_INFO_HEX(pScanResultTable->hReport, (TI_UINT8*)pFrame->parsedIEs->content.iePacket.pRates, pFrame->parsedIEs->content.iePacket.pRates->hdr[1]+2);
        return;
    }

    /* Update the rate elements */
    maxBasicRate = (TI_UINT8)rate_GetMaxBasicFromStr ((TI_UINT8 *)pFrame->parsedIEs->content.iePacket.pRates->rates, 
                                            pFrame->parsedIEs->content.iePacket.pRates->hdr[1], (ENetRate)maxBasicRate);
    maxActiveRate = (TI_UINT8)rate_GetMaxActiveFromStr ((TI_UINT8 *)pFrame->parsedIEs->content.iePacket.pRates->rates,
                                              pFrame->parsedIEs->content.iePacket.pRates->hdr[1], (ENetRate)maxActiveRate);

    if (pFrame->parsedIEs->content.iePacket.pExtRates)
    {
        maxBasicRate = (TI_UINT8)rate_GetMaxBasicFromStr ((TI_UINT8 *)pFrame->parsedIEs->content.iePacket.pExtRates->rates,
                                                pFrame->parsedIEs->content.iePacket.pExtRates->hdr[1], (ENetRate)maxBasicRate);
        maxActiveRate = (TI_UINT8)rate_GetMaxActiveFromStr ((TI_UINT8 *)pFrame->parsedIEs->content.iePacket.pExtRates->rates,
                                                  pFrame->parsedIEs->content.iePacket.pExtRates->hdr[1], (ENetRate)maxActiveRate);
    }

    if (maxActiveRate == 0)
    {
        maxActiveRate = maxBasicRate;
    }

    /* Now update it from network to host rates */
    pSite->maxBasicRate = rate_NetToDrv (maxBasicRate);
    pSite->maxActiveRate = rate_NetToDrv (maxActiveRate);
    if (pSite->maxActiveRate == DRV_RATE_INVALID)
            TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_ERROR, "scanResultTable_updateRates: Network To Host Rate failure, no active network rate\n");

    if (pSite->maxBasicRate != DRV_RATE_INVALID)
    {
        if (pSite->maxActiveRate != DRV_RATE_INVALID)
        {
            pSite->maxActiveRate = TI_MAX (pSite->maxActiveRate, pSite->maxBasicRate);
        }
    } else { /* in case some vendors don't specify basic rates */
        TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_WARNING, "scanResultTable_updateRates: Network To Host Rate failure, no basic network rate");
        pSite->maxBasicRate = pSite->maxActiveRate;
    }

    /* build rates bit map */
    rate_NetStrToDrvBitmap (&pSite->rateMask.supportedRateMask,
                            pFrame->parsedIEs->content.iePacket.pRates->rates,
                            pFrame->parsedIEs->content.iePacket.pRates->hdr[1]);
    rate_NetBasicStrToDrvBitmap (&pSite->rateMask.basicRateMask,
                                 pFrame->parsedIEs->content.iePacket.pRates->rates,
                                 pFrame->parsedIEs->content.iePacket.pRates->hdr[1]);

    if (pFrame->parsedIEs->content.iePacket.pExtRates)
    {
        rate_NetStrToDrvBitmap (&bitMapExtSupp, 
                                pFrame->parsedIEs->content.iePacket.pExtRates->rates,
                                pFrame->parsedIEs->content.iePacket.pExtRates->hdr[1]);

        pSite->rateMask.supportedRateMask |= bitMapExtSupp;

        rate_NetBasicStrToDrvBitmap (&bitMapExtSupp, 
                                     pFrame->parsedIEs->content.iePacket.pExtRates->rates,
                                     pFrame->parsedIEs->content.iePacket.pExtRates->hdr[1]);

        pSite->rateMask.basicRateMask |= bitMapExtSupp;
    }

	if (pFrame->parsedIEs->content.iePacket.pHtCapabilities != NULL)
    {
        /* MCS build rates bit map */
        rate_McsNetStrToDrvBitmap (&uMcsSupportedRateMask,
                                   (pFrame->parsedIEs->content.iePacket.pHtCapabilities->aHtCapabilitiesIe + DOT11_HT_CAPABILITIES_MCS_RATE_OFFSET));

        pSite->rateMask.supportedRateMask |= uMcsSupportedRateMask;
    }

    if (pFrame->parsedIEs->content.iePacket.pHtInformation != NULL)
    {
        /* MCS build rates bit map */
        rate_McsNetStrToDrvBitmap (&uMcsbasicRateMask,
                                   (pFrame->parsedIEs->content.iePacket.pHtInformation->aHtInformationIe + DOT11_HT_INFORMATION_MCS_RATE_OFFSET));

        pSite->rateMask.basicRateMask |= uMcsbasicRateMask;
    }
}

/** 
 * \fn     scanResultTable_UpdateWSCParams 
 * \brief  Update a scan result table entry with WSC information 
 * 
 * Update a scan result table entry with WSC information
 * 
 * \param  pSite - a pointer to the site entry to update 
 * \param  pFrame - a pointer to the received frame
 * \return None
 * \sa     scanResultTable_UpdateSiteData 
 */ 
void scanResultTable_UpdateWSCParams (TSiteEntry *pSite, TScanFrameInfo *pFrame)
{
    /* if the IE is not null => the WSC is on - check which method is supported */
    if (pFrame->parsedIEs->content.iePacket.WSCParams != NULL)
    {
        TI_UINT8    *tlvPtr,*endPtr;
        TI_UINT16   tlvPtrType,tlvPtrLen,selectedMethod=0;
    
        tlvPtr = (TI_UINT8*)pFrame->parsedIEs->content.iePacket.WSCParams->WSCBeaconOrProbIE;
        endPtr = tlvPtr + pFrame->parsedIEs->content.iePacket.WSCParams->hdr[1] - DOT11_OUI_LEN;
    
        do
        {
            tlvPtrType = WLANTOHS (WLAN_WORD(tlvPtr));
    
            if (tlvPtrType == DOT11_WSC_DEVICE_PASSWORD_ID)
            {
                tlvPtr+=2;
                tlvPtr+=2;
                selectedMethod = WLANTOHS (WLAN_WORD(tlvPtr));
                break;
            }
            else
            {
                tlvPtr+=2;
                tlvPtrLen = WLANTOHS (WLAN_WORD(tlvPtr));
                tlvPtr+=tlvPtrLen+2;
            }
        } while ((tlvPtr < endPtr) && (selectedMethod == 0));
       
        if (tlvPtr > endPtr)
        {
            pSite->WSCSiteMode = TIWLN_SIMPLE_CONFIG_OFF;
            return;
        }
    
        if (selectedMethod == DOT11_WSC_DEVICE_PASSWORD_ID_PIN)
            pSite->WSCSiteMode = TIWLN_SIMPLE_CONFIG_PIN_METHOD;
        else if (selectedMethod == DOT11_WSC_DEVICE_PASSWORD_ID_PBC)
            pSite->WSCSiteMode = TIWLN_SIMPLE_CONFIG_PBC_METHOD;
        else 
            pSite->WSCSiteMode = TIWLN_SIMPLE_CONFIG_OFF;
    }
    else
    {
        pSite->WSCSiteMode = TIWLN_SIMPLE_CONFIG_OFF;
    }
}

/** 
 * \fn     scanResultTable_GetNumOfBSSIDInTheList
 * \brief  Returns the number of BSSID's in the scan result list
 * 
 * \param  hScanResultTable - handle to the scan result table
 * \return The number of BSSID's in the list
 * \sa scanResultTable_GetBssidSupportedRatesList
 */ 
TI_UINT32 scanResultTable_GetNumOfBSSIDInTheList (TI_HANDLE hScanResultTable)
{
	return ((TScanResultTable*)hScanResultTable)->uCurrentSiteNumber;
}

/** 
 * \fn     scanResultTable_CalculateBssidListSize
 * \brief  Calculates the size required for BSSID list storage
 * 
 * Calculates the size required for BSSID list storage
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  bAllVarIes - whether to include all variable size IEs
 * \return The total length required
 * \sa     scanResultTable_GetBssidList
 */ 
TI_UINT32 scanResultTable_CalculateBssidListSize (TI_HANDLE hScanResultTable, TI_BOOL bAllVarIes)
{
    TScanResultTable    *pScanResultTable = (TScanResultTable*)hScanResultTable;
    TI_UINT32           uSiteIndex, uSiteLength, uLength = 0;
    TSiteEntry          *pSiteEntry;

    /* set the length of the list header (sites count) */
    uLength = sizeof(OS_802_11_BSSID_LIST_EX) - sizeof(OS_802_11_BSSID_EX);

    /* check lengthes of all sites in the table */
    for (uSiteIndex = 0; uSiteIndex < pScanResultTable->uCurrentSiteNumber; uSiteIndex++)
    {
        pSiteEntry = &(pScanResultTable->pTable[ uSiteIndex ]);
        /* if full list is requested */
        if (bAllVarIes)
        {
            /* set length of all IEs for this site */
            uSiteLength = sizeof(OS_802_11_BSSID_EX) + sizeof(OS_802_11_FIXED_IEs);
            /* and add beacon or probe response length */
            if (TI_TRUE == pSiteEntry->probeRecv)
            {
                uSiteLength += pSiteEntry->probeRespLength;
            }
            else
            {
                uSiteLength += pSiteEntry->beaconLength;
            }
                     
        }
        /* partial list is requested */
        else
        {
            uSiteLength = (sizeof(OS_802_11_BSSID_EX) + sizeof(OS_802_11_FIXED_IEs) +
                           (pSiteEntry->ssid.len + 2) + (DOT11_MAX_SUPPORTED_RATES + 2) +
                           + (DOT11_DS_PARAMS_ELE_LEN +2) + pSiteEntry->rsnIeLen + pSiteEntry->unknownIeLen);

            /* QOS_WME information element */
            if (pSiteEntry->WMESupported)
            {
                /* length of element + header */
                uSiteLength += (DOT11_WME_PARAM_ELE_LEN + 2);
            }
        }

        /* make sure length is 4 bytes aligned */
        if (uSiteLength % 4)
        {
            uSiteLength += (4 - (uSiteLength % 4));
        }

        /* add this site length to the total length */
        uLength += uSiteLength;

        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_calculateBssidListSize: BSSID length=%d on site index %d\n", uSiteLength, uSiteIndex);
    }

    TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_calculateBssidListSize: total length=%d \n", uLength);

    return uLength;
}

/** 
 * \fn     scanResultTable_GetBssidList
 * \brief  Retrieves the site table content
 * 
 * Retrieves the site table content
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  pBssidList - pointer to a buffer large enough to hols the BSSID list
 * \param  plength - length of the supplied buffer, will be overwritten with the actual list length
 * \param  bAllVarIes - whether to include all variable size IEs
 * \return None
 * \sa     scanResultTable_CalculateBssidListSize
 */ 
TI_STATUS scanResultTable_GetBssidList (TI_HANDLE hScanResultTable, 
                                        OS_802_11_BSSID_LIST_EX *pBssidList, 
                                        TI_UINT32 *pLength, 
                                        TI_BOOL bAllVarIes)
{
    TScanResultTable        *pScanResultTable = (TScanResultTable*)hScanResultTable;
    TI_UINT32                uLength, uSiteIndex, rsnIndex, rsnIeLength, len, firstOFDMloc = 0;
    TSiteEntry              *pSiteEntry;
    OS_802_11_BSSID_EX      *pBssid;
    OS_802_11_FIXED_IEs     *pFixedIes;
    OS_802_11_VARIABLE_IEs  *pVarIes;
    TI_UINT8                *pData;

    TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidList called, pBssidList= 0x%p, pLength=%d\n", pBssidList, *pLength);

    /* verify the supplied length is enough */
    uLength = scanResultTable_CalculateBssidListSize (hScanResultTable, bAllVarIes);
    if (uLength > *pLength)
    {
        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_ERROR , "scanResultTable_GetBssidList: received length %d, insufficient to hold list of size %d\n", *pLength, uLength);
        *pLength = uLength;
        return TI_NOK;
    }
#ifdef TI_DBG
    else
    {
        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidList: supplied length: %d, required length: %d\n", *pLength, uLength);
    }
#endif

    /* Nullify number of items in the BSSID list */
    pBssidList->NumberOfItems = 0;

    /* set length to list header size (only list count) */
    uLength = sizeof(OS_802_11_BSSID_LIST_EX) - sizeof(OS_802_11_BSSID_EX);

    /* set data pointer to first item in list */
    pData = (TI_UINT8*)&(pBssidList->Bssid[0]);

    for (uSiteIndex = 0; uSiteIndex < pScanResultTable->uCurrentSiteNumber; uSiteIndex++)
    {
        /* set BSSID entry pointer to current location in buffer */
        pBssid = (OS_802_11_BSSID_EX*)pData;

        /* set pointer to site entry */
        pSiteEntry = &(pScanResultTable->pTable[ uSiteIndex ]);

        TRACE7(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidList: copying entry at index %d, BSSID %02x:%02x:%02x:%02x:%02x:%02x\n", uSiteIndex, pSiteEntry->bssid[ 0 ], pSiteEntry->bssid[ 1 ], pSiteEntry->bssid[ 2 ], pSiteEntry->bssid[ 3 ], pSiteEntry->bssid[ 4 ], pSiteEntry->bssid[ 5 ]);

        /* start copy stuff: */
        /* MacAddress */
        MAC_COPY (pBssid->MacAddress, pSiteEntry->bssid);
        
        /* Capabilities */
        pBssid->Capabilities = pSiteEntry->capabilities;

        /* SSID */
        os_memoryZero (pScanResultTable->hOS, &(pBssid->Ssid.Ssid), MAX_SSID_LEN);
        if (pSiteEntry->ssid.len > MAX_SSID_LEN)
        {
            pSiteEntry->ssid.len = MAX_SSID_LEN;
        }
        os_memoryCopy (pScanResultTable->hOS, 
                       (void *)pBssid->Ssid.Ssid, 
                       (void *)pSiteEntry->ssid.str, 
                       pSiteEntry->ssid.len);
        pBssid->Ssid.SsidLength = pSiteEntry->ssid.len;

        /* privacy */
        pBssid->Privacy = pSiteEntry->privacy;

        /* RSSI */
        pBssid->Rssi = pSiteEntry->rssi;

        pBssid->Configuration.Length = sizeof(OS_802_11_CONFIGURATION);
        pBssid->Configuration.BeaconPeriod = pSiteEntry->beaconInterval;
        pBssid->Configuration.ATIMWindow = pSiteEntry->atimWindow;
        pBssid->Configuration.Union.channel = Chan2Freq(pSiteEntry->channel);

        if  (pSiteEntry->bssType == BSS_INDEPENDENT)
            pBssid->InfrastructureMode = os802_11IBSS;
        else
            pBssid->InfrastructureMode = os802_11Infrastructure;
        /* Supported Rates */
        os_memoryZero (pScanResultTable->hOS, (void *)pBssid->SupportedRates, sizeof(OS_802_11_RATES_EX));
        rate_DrvBitmapToNetStr (pSiteEntry->rateMask.supportedRateMask,
                                pSiteEntry->rateMask.basicRateMask,
                                (TI_UINT8*)pBssid->SupportedRates,
                                &len, 
                                &firstOFDMloc);

        /* set network type acording to band and rates */
        if (RADIO_BAND_2_4_GHZ == pSiteEntry->eBand)
        {
            if (firstOFDMloc == len)
            {
                pBssid->NetworkTypeInUse = os802_11DS;
            } else {
                pBssid->NetworkTypeInUse = os802_11OFDM24;
            }
        }
        else
        {
            pBssid->NetworkTypeInUse = os802_11OFDM5;
        }

        /* start copy IE's: first nullify length */
        pBssid->IELength = 0;

        /* copy fixed IEs from site entry */
        pFixedIes = (OS_802_11_FIXED_IEs*)&(pBssid->IEs[ pBssid->IELength ]);
        os_memoryCopy (pScanResultTable->hOS, (void*)pFixedIes->TimeStamp, 
                       &(pSiteEntry->tsfTimeStamp[ 0 ]), TIME_STAMP_LEN);
        pFixedIes->BeaconInterval = pSiteEntry->beaconInterval;
        pFixedIes->Capabilities = pSiteEntry->capabilities;
        pBssid->IELength += sizeof(OS_802_11_FIXED_IEs);

        /* set pointer for variable length IE's */
        pVarIes = (OS_802_11_VARIABLE_IEs*)&(pBssid->IEs[ pBssid->IELength ]);

        if (!bAllVarIes)
        {   /* copy only some variable IEs */

            /* copy SSID */
            pVarIes->ElementID = SSID_IE_ID;
            pVarIes->Length = pSiteEntry->ssid.len;
            os_memoryCopy (pScanResultTable->hOS, 
                           (void *)pVarIes->data, 
                           (void *)pSiteEntry->ssid.str, 
                           pSiteEntry->ssid.len);
            pBssid->IELength += (pVarIes->Length + 2);

            /* copy RATES */
            pVarIes = (OS_802_11_VARIABLE_IEs*)&(pBssid->IEs[ pBssid->IELength ]);
            pVarIes->ElementID = SUPPORTED_RATES_IE_ID;
            rate_DrvBitmapToNetStr (pSiteEntry->rateMask.supportedRateMask, 
                                    pSiteEntry->rateMask.basicRateMask,
                                    (TI_UINT8 *)pVarIes->data, 
                                    &len, 
                                    &firstOFDMloc);
            pVarIes->Length = len;
            pBssid->IELength += (pVarIes->Length + 2);

            /* copy DS */
            pVarIes = (OS_802_11_VARIABLE_IEs*)&(pBssid->IEs[ pBssid->IELength ]);
            pVarIes->ElementID = DS_PARAMETER_SET_IE_ID;
            pVarIes->Length = DOT11_DS_PARAMS_ELE_LEN;
            os_memoryCopy (pScanResultTable->hOS, (void *)pVarIes->data, 
                           &(pSiteEntry->channel), DOT11_DS_PARAMS_ELE_LEN);
            pBssid->IELength += (pVarIes->Length + 2);

            /* copy RSN information elements */
            if (0 < pSiteEntry->rsnIeLen)
            {
                rsnIeLength = 0;
                for (rsnIndex=0; rsnIndex < MAX_RSN_IE && pSiteEntry->pRsnIe[ rsnIndex ].hdr[1] > 0; rsnIndex++)
                {
                    pVarIes = (OS_802_11_VARIABLE_IEs*)&(pBssid->IEs[ pBssid->IELength + rsnIeLength ]);
                    pVarIes->ElementID = pSiteEntry->pRsnIe[ rsnIndex ].hdr[0];
                    pVarIes->Length = pSiteEntry->pRsnIe[ rsnIndex ].hdr[1];
                    os_memoryCopy (pScanResultTable->hOS, (void *)pVarIes->data, 
                                   (void *)pSiteEntry->pRsnIe[ rsnIndex ].rsnIeData, 
                                   pSiteEntry->pRsnIe[ rsnIndex ].hdr[1]);
                    rsnIeLength += pSiteEntry->pRsnIe[ rsnIndex ].hdr[1] + 2;
                }
                pBssid->IELength += pSiteEntry->rsnIeLen;
            }

            /* QOS_WME/XCC */
            if (TI_TRUE == pSiteEntry->WMESupported)
            {
                /* oui */
                TI_UINT8            ouiWME[3] = {0x50, 0xf2, 0x01};
                dot11_WME_PARAM_t   *pWMEParams; 

                /* fill in the general element  parameters */
                pVarIes =  (OS_802_11_VARIABLE_IEs*)&(pBssid->IEs[ pBssid->IELength ]);
                pVarIes->ElementID = DOT11_WME_ELE_ID;
                pVarIes->Length = DOT11_WME_PARAM_ELE_LEN;

                /* fill in the specific element  parameters */
                pWMEParams = (dot11_WME_PARAM_t*)pVarIes;
                os_memoryCopy (pScanResultTable->hOS, (void *)pWMEParams->OUI, ouiWME, 3);
                pWMEParams->OUIType = dot11_WME_OUI_TYPE;
                pWMEParams->OUISubType = dot11_WME_OUI_SUB_TYPE_PARAMS_IE;
                pWMEParams->version = dot11_WME_VERSION;
                pWMEParams->ACInfoField = dot11_WME_ACINFO_MASK & pSiteEntry->lastWMEParameterCnt;

                /* fill in the data  */
                os_memoryCopy (pScanResultTable->hOS, &(pWMEParams->WME_ACParameteres),
                               &(pSiteEntry->WMEParameters), sizeof(dot11_ACParameters_t));


                /* update the general length */
                pBssid->IELength += (pVarIes->Length + 2);
            }

			/* Copy the unknown IEs */
			if ( 0 < pSiteEntry->unknownIeLen  ) {
					os_memoryCopy (pScanResultTable->hOS, (void *)(&pBssid->IEs[ pBssid->IELength ]),
								   (void *)pSiteEntry->pUnknownIe,
								   pSiteEntry->unknownIeLen );
					pBssid->IELength += pSiteEntry->unknownIeLen;
			}

        }
        else
        {   /* Copy all variable IEs */
            if (pSiteEntry->probeRecv)
            {
                /* It looks like it never happens. Anyway decided to check */
                if ( pSiteEntry->probeRespLength > MAX_BEACON_BODY_LENGTH )
                   /* it may have sense to check the Len here for 0 or MIN_BEACON_BODY_LENGTH also */
                {
                    TRACE2( pScanResultTable->hReport, REPORT_SEVERITY_ERROR,
                         "scanResultTable_GetBssidList. pSiteEntry->probeRespLength=%d exceeds the limit %d\n",
                         pSiteEntry->probeRespLength, MAX_BEACON_BODY_LENGTH);
                    handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
                    return TI_NOK;
                }
                os_memoryCopy (pScanResultTable->hOS, pVarIes, 
                               pSiteEntry->probeRespBuffer, pSiteEntry->probeRespLength);
                pBssid->IELength += pSiteEntry->probeRespLength;
            }
            else
            {
                /* It looks like it never happens. Anyway decided to check */
                if ( pSiteEntry->beaconLength > MAX_BEACON_BODY_LENGTH )
                   /* it may have sense to check the Len here for 0 or MIN_BEACON_BODY_LENGTH also */
                {
                    TRACE2( pScanResultTable->hReport, REPORT_SEVERITY_ERROR,
                         "scanResultTable_GetBssidList. pSiteEntry->beaconLength=%d exceeds the limit %d\n",
                         pSiteEntry->beaconLength, MAX_BEACON_BODY_LENGTH);
                    handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
                    return TI_NOK;
                }
                os_memoryCopy (pScanResultTable->hOS, pVarIes, 
                               pSiteEntry->beaconBuffer, pSiteEntry->beaconLength);
                pBssid->IELength += pSiteEntry->beaconLength;
            }
        }

        /* -1 to remove the IEs[1] placeholder in OS_802_11_BSSID_EX which is taken into account in pBssid->IELength */
        pBssid->Length = sizeof(OS_802_11_BSSID_EX) + pBssid->IELength - 1;

        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidList: before alignment fix, IEs length: %d, BSSID length %d\n", pBssid->IELength, pBssid->Length);

        /* make sure length is 4 bytes aligned */
        if (pBssid->Length % 4)
        {
            pBssid->Length += (4 - (pBssid->Length % 4));
        }

        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidList: after alignment fix, IEs length: %d, BSSID length %d\n", pBssid->IELength, pBssid->Length);

        pData += pBssid->Length;
        uLength += pBssid->Length;

        TRACE1(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidList: current length: %d\n", uLength);
    }

    pBssidList->NumberOfItems = pScanResultTable->uCurrentSiteNumber;
    *pLength = uLength;

    TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidList: total length: %d, number of items: %d\n", uLength, pBssidList->NumberOfItems);

    return TI_OK;
}


/** 
 * \fn     scanResultTable_GetBssidSupportedRatesList
 * \brief  Retrieves the Rate table corresponding with the site
 * table
 * 
 * 
 * \param  hScanResultTable - handle to the scan result table object
 * \param  pRateList - pointer to a buffer large enough to hols
 * the rate list
 * \param  pLength - length of the supplied buffer,
 * \return TI_STATUS
 * \sa scanResultTable_GetBssidSupportedRatesList
 */ 
TI_STATUS scanResultTable_GetBssidSupportedRatesList (TI_HANDLE hScanResultTable,
													  OS_802_11_N_RATES *pRateList,
													  TI_UINT32 *pLength)
{
    TScanResultTable        *pScanResultTable = (TScanResultTable*)hScanResultTable;
	TSiteEntry              *pSiteEntry;
    TI_UINT32                uSiteIndex, firstOFDMloc = 0;
	TI_UINT32                requiredLength;
	OS_802_11_N_RATES	 	*pCurrRateString;

    TRACE0(pScanResultTable->hReport, REPORT_SEVERITY_INFORMATION , "scanResultTable_GetBssidSupportedRatesList called");

    /* Verify the supplied length is enough*/
	requiredLength = pScanResultTable->uCurrentSiteNumber*sizeof(OS_802_11_N_RATES);
	if (requiredLength > *pLength)
    {
        TRACE2(pScanResultTable->hReport, REPORT_SEVERITY_ERROR , "scanResultTable_GetBssidSupportedRatesList: received length %d, insufficient to hold list of size %d\n", *pLength, requiredLength);
        *pLength = requiredLength;
        return TI_NOK;
    }

    /* Create the rate list*/
    for (uSiteIndex = 0; uSiteIndex < pScanResultTable->uCurrentSiteNumber; uSiteIndex++)
    {
		pCurrRateString = &(pRateList[uSiteIndex]);
        pSiteEntry = &(pScanResultTable->pTable[ uSiteIndex ]);

        /* Supported Rates */
        os_memoryZero (pScanResultTable->hOS, (void *)pCurrRateString, sizeof(OS_802_11_N_RATES));
        rate_DrvBitmapToNetStrIncluding11n (pSiteEntry->rateMask.supportedRateMask,
												  pSiteEntry->rateMask.basicRateMask,
												  (TI_UINT8*)pCurrRateString,
												  &firstOFDMloc);
	}

    return TI_OK;
}


/***********************************************************************
 *                        siteMgr_CheckRxSignalValidity
 ***********************************************************************
DESCRIPTION: Called by the scanResultTable_UpdateEntry when receiving managment frame 
                Find the ste in the site table and validate that the 
                RSSI of that site signal is not lower then -80DB + not lower
                then the exising site RSSI


INPUT:      hSiteMgr    -   site mgr handle.
            rxLevel     -   Rx level the frame received in
            bssid       -   BSSID of the frame

OUTPUT:

RETURN:     TI_OK / TI_NOK

************************************************************************/
/** 
 * \fn     scanResultTable_CheckRxSignalValidity
 * \brief  return the state of the table to its state after scan
 * 
 * Called by the scanResultTable_UpdateEntry when receiving managment frame   
 * validate that the RSSI of that site signal is not lower then then the exising site RSSI.
 * validate that the channel in correct.                                             
 * 
 * \param  pScanResultTable - scan result table object
 * \param  pSite - entry from the table
 * \param  rssi - RSSI level at which frame was received 
 * \param  channel - channel on which the frame was received 
 * \return None
 * \sa
 */ 
static TI_STATUS scanResultTable_CheckRxSignalValidity(TScanResultTable *pScanResultTable, TSiteEntry *pSite, TI_INT8 rxLevel, TI_UINT8 channel)
{
     if ((channel != pSite->channel) &&
         (rxLevel < pSite->rssi))
     {   /* Ignore wrong packets with lower RSSI that were detect as
         ripples from different channels */
         TRACE4(pScanResultTable->hReport, REPORT_SEVERITY_WARNING, "scanResultTable_CheckRxSignalValidity:Rx RSSI =%d, on channel=%d, is lower then given RSSI =%d on channel=%d, dropping it.\n", rxLevel, channel, pSite->rssi, pSite->channel);
         return TI_NOK;
     }

     return TI_OK;
}



