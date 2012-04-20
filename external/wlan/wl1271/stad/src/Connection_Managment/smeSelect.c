/*
 * smeSelect.c
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

/** \file  smeSelect.c
 *  \brief SME select function implementation
 *
 *  \see   smeSm.h, smeSm.c, sme.c, sme.h, smePrivate.h
 */


#define __FILE_ID__  FILE_ID_42
#include "smePrivate.h"
#include "scanResultTable.h"
#include "rsnApi.h"
#include "siteMgrApi.h"
#include "EvHandler.h"
#include "GenSM.h"
#include "smeSm.h"
#include "tidef.h"

static TI_BOOL sme_SelectSsidMatch (TI_HANDLE hSme, TSsid *pSiteSsid, TSsid *pDesiredSsid, 
                                    ESsidType eDesiredSsidType);
static TI_BOOL sme_SelectBssidMatch (TMacAddr *pSiteBssid, TMacAddr *pDesiredBssid);
static TI_BOOL sme_SelectBssTypeMatch (ScanBssType_e eSiteBssType, ScanBssType_e eDesiredBssType);
static TI_BOOL sme_SelectWscMatch (TI_HANDLE hSme, TSiteEntry *pCurrentSite, 
                                   TI_BOOL *pbWscPbAbort, TI_BOOL *pbWscPbApFound);
static TI_BOOL sme_SelectRsnMatch (TI_HANDLE hSme, TSiteEntry *pCurrentSite);

/** 
 * \fn     sme_Select
 * \brief  Select a connection candidate from the scan result table
 * 
 * Select a connection candidate from the scan result table.
 * 
 * Connection candidate must match SSID, BSSID, BSS type, RSN and WSC settings, has the best
 * RSSI level from all matching sites, and connection was not attempted to it in this SME cycle
 * (since last scan was completed)
 * 
 * \param  hSme - handle to the SME object
 * \return A pointer to the selected site, NULL if no site macthes the selection criteria
 */ 
TSiteEntry *sme_Select (TI_HANDLE hSme)
{
    TSme            *pSme = (TSme*)hSme;
    TSiteEntry      *pCurrentSite, *pSelectedSite = NULL;
    TI_INT8         iSelectedSiteRssi = -127; /* minimum RSSI */
    TI_BOOL         bWscPbAbort, pWscPbApFound = TI_FALSE;
    int             apFoundCtr =0;
    TIWLN_SIMPLE_CONFIG_MODE eWscMode;

    TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select called\n");

    /* on SG avalanche, select is not needed, send connect event automatically */
    if (TI_TRUE == pSme->bReselect)
    {
        paramInfo_t *pParam;

        TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: reselect flag is on, reselecting the current site\n");

        pParam = (paramInfo_t *)os_memoryAlloc(pSme->hOS, sizeof(paramInfo_t));
        if (!pParam)
        {
            return NULL;
        }

        pSme->bReselect = TI_FALSE;

        /* Get Primary Site */
        pParam->paramType = SITE_MGR_GET_PRIMARY_SITE;
        siteMgr_getParam(pSme->hSiteMgr, pParam);
        pCurrentSite = pParam->content.pPrimarySite;
        os_memoryFree(pSme->hOS, pParam, sizeof(paramInfo_t));
        return pCurrentSite;
    }

    /* get the first site from the scan result table */
    pCurrentSite = scanResultTable_GetFirst (pSme->hScanResultTable);

    /* check all sites */
    while (NULL != pCurrentSite)
    {
        TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: considering BSSID: %02x:%02x:%02x:%02x:%02x:%02x for selection\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);

        /* if this site was previously selected in the current SME connection attempt, and conn mode is auto */
        if (TI_TRUE == pCurrentSite->bConsideredForSelect)
        {
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x was selected previously\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            /* get the next site and continue the loop */
            pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
            continue;
        }

        /* check if site matches */
        /* first check SSID match */
        if (TI_FALSE == sme_SelectSsidMatch (hSme, &(pCurrentSite->ssid), &(pSme->tSsid), pSme->eSsidType))
        /* site doesn't match */
        {
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x doesn't match SSID\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            pCurrentSite->bConsideredForSelect = TI_TRUE; /* don't try this site again */
            /* get the next site and continue the loop */
            pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
            continue;
        }

        /* Now check BSSID match */
        if (TI_FALSE == sme_SelectBssidMatch (&(pCurrentSite->bssid), &(pSme->tBssid)))
        /* site doesn't match */
        {
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x doesn't match SSID\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            pCurrentSite->bConsideredForSelect = TI_TRUE; /* don't try this site again */
            /* get the next site and continue the loop */
            pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
            continue;
        }

        /* and BSS type match */
        if (TI_FALSE == sme_SelectBssTypeMatch (pCurrentSite->bssType, pSme->eBssType))
        /* site doesn't match */
        {
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x doesn't match BSS type\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            pCurrentSite->bConsideredForSelect = TI_TRUE; /* don't try this site again */
            /* get the next site and continue the loop */
            pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
            continue;
        }

         if (pCurrentSite->WSCSiteMode == TIWLN_SIMPLE_CONFIG_PBC_METHOD)
         {
           apFoundCtr++;
         }
         if (apFoundCtr > 1)
         {
           pWscPbApFound = TI_TRUE;
         }

        /* and simple config match */
        if (TI_FALSE == sme_SelectWscMatch (hSme, pCurrentSite, &bWscPbAbort, &pWscPbApFound))
        /* site doesn't match */
        {
            /* also check if abort was indicated */
            if (TI_TRUE == bWscPbAbort)
            {
                /* send event to user mode to indicate this */
                EvHandlerSendEvent (pSme->hEvHandler, IPC_EVENT_WPS_SESSION_OVERLAP, NULL, 0);
                /* select failed - will rescan in time */
                return NULL;
            }
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x doesn't match WSC\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            pCurrentSite->bConsideredForSelect = TI_TRUE; /* don't try this site again */
            /* get the next site and continue the loop */
            pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
            continue;
        }

        /* and security match */
        siteMgr_getParamWSC(pSme->hSiteMgr, &eWscMode); 

        /* we don't need to check RSN match while WSC is active */
        if ((pCurrentSite->WSCSiteMode == TIWLN_SIMPLE_CONFIG_OFF) || (pCurrentSite->WSCSiteMode != eWscMode))
        {
            if (TI_FALSE == sme_SelectRsnMatch (hSme, pCurrentSite))
            /* site doesn't match */
            {
                TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x doesn't match RSN\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
                pCurrentSite->bConsideredForSelect = TI_TRUE; /* don't try this site again */
                /* get the next site and continue the loop */
                pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
                continue;
            }
        }

        /* and rate match */
        if (TI_FALSE == siteMgr_SelectRateMatch (pSme->hSiteMgr, pCurrentSite))
        /* site doesn't match */
        {
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x doesn't match rates\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            pCurrentSite->bConsideredForSelect = TI_TRUE; /* don't try this site again */
            /* get the next site and continue the loop */
            pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
            continue;
        }

        if (TI_TRUE == pCurrentSite->bChannelSwitchAnnoncIEFound)
        {
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x has channel switch IE so ignore it \n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            pCurrentSite->bConsideredForSelect = TI_TRUE; /* don't try this site again */
            /* get the next site and continue the loop */
            pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
            continue;
        }

        /* if this site RSSI is higher than current maximum, select it */
        if (pCurrentSite->rssi > iSelectedSiteRssi)
        {
            TRACE6(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_Select: BSSID: %02x:%02x:%02x:%02x:%02x:%02x match and has highest RSSI so far!\n", pCurrentSite->bssid[ 0 ], pCurrentSite->bssid[ 1 ], pCurrentSite->bssid[ 2 ], pCurrentSite->bssid[ 3 ], pCurrentSite->bssid[ 4 ], pCurrentSite->bssid[ 5 ]);
            pSelectedSite = pCurrentSite;
            iSelectedSiteRssi = pCurrentSite->rssi;
        }

        /* and continue to the next site */
        pCurrentSite = scanResultTable_GetNext (pSme->hScanResultTable);
    }

    /* if a matching site was found */
    if (NULL != pSelectedSite)
    {
        /* mark that a connection to this site was (actually is) attempted */
        pSelectedSite->bConsideredForSelect = TI_TRUE;

        /* hope this is the correct place for siteMgr_changeBandParams */
        siteMgr_changeBandParams (pSme->hSiteMgr, pSelectedSite->eBand);

        /*
         * Coordinate between SME module site table and Site module site Table 
         * copy candidate AP to Site module site Table.
         */
        siteMgr_CopyToPrimarySite(pSme->hSiteMgr, pSelectedSite);

		/* copy the result, rather than returning a pointer to the entry in the scan result table.
		 * This is done since the table might change durring the connection process, and the pointer 
		 * will point to the wrong entry in the table, causing connection/disconnection problems */
		os_memoryCopy(pSme->hOS, &(pSme->tCandidate), pSelectedSite, sizeof(TSiteEntry));

		return &(pSme->tCandidate);
    }

    /* return NULL if no site was selected */
    return NULL;
}

/** 
 * \fn     sme_SelectSsidMatch
 * \brief  Check if a site SSID matches the desired SSID for selection
 * 
 * Check if a site SSID matches the desired SSID for selection
 * 
 * \param  hSme - handle to the SME object
 * \param  pSiteSsid - the site SSID
 * \param  pDesiredSsid - the desired SSID
 * \param  edesiredSsidType - the desired SSID type
 * \return TI_TRUE if SSIDs match, TI_FALSE if they don't
 * \sa     sme_Select
 */ 
TI_BOOL sme_SelectSsidMatch (TI_HANDLE hSme, TSsid *pSiteSsid, TSsid *pDesiredSsid, 
                             ESsidType eDesiredSsidType)
{
    TSme        *pSme = (TSme*)hSme;

	/*If SSID length is 0 (hidden SSID)- Discard*/
	if (pSiteSsid->len == 0)
	{
		return TI_FALSE;
	}

    /* if the desired SSID type is any, return TRUE (site matches) */
    if (SSID_TYPE_ANY == eDesiredSsidType)
    {
        return TI_TRUE;
    }
    /* It looks like it never happens. Anyway decided to check */
    if (( pSiteSsid->len > MAX_SSID_LEN ) ||
        ( pDesiredSsid->len > MAX_SSID_LEN ))
    {
        TRACE3( pSme->hReport, REPORT_SEVERITY_ERROR,
               "sme_SelectSsidMatch. pSme->tSsid.len=%d or pDesiredSsid->len =%d exceed the limit %d\n",
                   pSiteSsid->len, pDesiredSsid->len, MAX_SSID_LEN);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return TI_FALSE;
    }
    /* otherwise, check if the SSIDs match */
    if ((pSiteSsid->len == pDesiredSsid->len) && /* lngth match */
        (0 == os_memoryCompare (pSme->hOS, (TI_UINT8 *)&(pSiteSsid->str[ 0 ]), (TI_UINT8 *)&(pDesiredSsid->str[ 0 ]), pSiteSsid->len))) /* content match */
    {
        return TI_TRUE;
    }
    else
    {
        return TI_FALSE;
    }
}

/** 
 * \fn     sme_SelectBssidMatch
 * \brief  Check if a site BSSID matches the desired BSSID for selection
 * 
 * Check if a site BSSID matches the desired BSSID for selection
 * 
 * \param  pSiteBssid - the site BSSID
 * \param  pDesiredBssid - the desired BSSID
 * \return TI_TRUE if BSSIDs match, TI_FALSE if they don't
 * \sa     sme_Select
 */ 
TI_BOOL sme_SelectBssidMatch (TMacAddr *pSiteBssid, TMacAddr *pDesiredBssid)
{
    /* check if the desired BSSID is broadcast (no need to match) */
    if (TI_TRUE == MAC_BROADCAST (*pDesiredBssid))
    {
        return TI_TRUE;
    }

    /* if the desired BSSID is not any BSSID, check if the site BSSID equals the desired BSSID */
    if (TI_TRUE == MAC_EQUAL (*pDesiredBssid, *pSiteBssid))
    {
        return TI_TRUE;
    }

    /* no match */
    return TI_FALSE;
}

/** 
 * \fn     sme_SelectBssTypeMatch
 * \brief  Checks if the desired BSS type match the BSS type of a site
 * 
 * Checks if the desired BSS type match the BSS type of a site
 * 
 * \param  eSiteBssType - the site BSS type
 * \param  edesiredBssType - the desired BSS type
 * \return TI_TRUE if the BSS types matches, TI_FALSE if they don't
 * \sa     sme_Select
 */ 
TI_BOOL sme_SelectBssTypeMatch (ScanBssType_e eSiteBssType, ScanBssType_e eDesiredBssType)
{
    /* if the desired type is any, there is a match */
    if (BSS_ANY == eDesiredBssType)
    {
        return TI_TRUE;
    }

    /* if the BSS types equal, there is a match */
    if (eDesiredBssType == eSiteBssType)
    {
        return TI_TRUE;
    }

    /* no match */
    return TI_FALSE;
}

/** 
 * \fn     sme_SelectWscMatch
 * \brief  checks if the configred WSC mode equals the WSC mode of a site
 * 
 * checks if the configred WSC mode equals the WSC mode of a site
 * 
 * \param  hSme - handle to the SME object
 * \param  pCurrentSite - site to check
 * \return TI_TRUE if site macthes current WSC mode, TI_FALSE if it doesn't match 
 * \sa     sme_Select
 */ 
TI_BOOL sme_SelectWscMatch (TI_HANDLE hSme, TSiteEntry *pCurrentSite, 
                            TI_BOOL *pbWscPbAbort, TI_BOOL *pbWscPbApFound)
{
    TSme            *pSme = (TSme*)hSme;
    TIWLN_SIMPLE_CONFIG_MODE  wscMode;

    /* get the WSC mode from site mgr */
    siteMgr_getParamWSC(pSme->hSiteMgr, &wscMode); /* SITE_MGR_SIMPLE_CONFIG_MODE - get the WSC mode from site mgr */

    /* if simple config is off, site match */
    if (TIWLN_SIMPLE_CONFIG_OFF == wscMode)
    {
        return TI_TRUE;
    }

    /* if WSC is supported, and more than one AP with PB configuration is found - indicate to abort */
    if ((TI_TRUE == *pbWscPbApFound) && (TIWLN_SIMPLE_CONFIG_PBC_METHOD == pCurrentSite->WSCSiteMode))
    {
        TRACE1(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_SelectWscMatch: WSC mode is %d, and more than one AP with WSC PB found - aborting\n", wscMode);
        *pbWscPbAbort = TI_TRUE;
        return TI_FALSE;
    }
    else
    {
        /* indicate NOT to abort the select process */
        *pbWscPbAbort = TI_FALSE;
    }

    /* if configured simple config mode equals site simple config mode, site match */
    if (pCurrentSite->WSCSiteMode == wscMode)
    {
        return TI_TRUE;
    }

    /* site doesn't match */
    return TI_FALSE;
}

/** 
 * \fn     sme_SelectRsnMatch
 * \brief  Checks if the configured scurity settings match those of a site
 * 
 * Checks if the configured scurity settings match those of a site
 * 
 * \param  hSme - handle to the SME object
 * \param  pCurrentSite - the site to check
 * \return TI_TRUE if site matches RSN settings, TI FALSE if it doesn't
 * \sa     sme_Select
 */ 
TI_BOOL sme_SelectRsnMatch (TI_HANDLE hSme, TSiteEntry *pCurrentSite)
{
    TSme            *pSme = (TSme*)hSme;
	TRsnData    	tRsnData;
    dot11_RSN_t     *pRsnIe;
    TI_UINT8        uRsnIECount=0;
    TI_UINT8        uCurRsnData[255];
    TI_UINT32       uLength = 0;
    TI_UINT32       uMetric;
	TRsnSiteParams  tRsnSiteParams;

	tRsnSiteParams.bssType = pCurrentSite->bssType;
	MAC_COPY(tRsnSiteParams.bssid, pCurrentSite->bssid);
	tRsnSiteParams.pHTCapabilities = &pCurrentSite->tHtCapabilities;
	tRsnSiteParams.pHTInfo = &pCurrentSite->tHtInformation;

    /* copy all RSN IE's */
    pRsnIe = pCurrentSite->pRsnIe;
    while ((uLength < pCurrentSite->rsnIeLen) && (uRsnIECount < MAX_RSN_IE))
    {
        if (uLength + 2 + pRsnIe->hdr[ 1 ] > sizeof (uCurRsnData))
        {
           TRACE4( pSme->hReport, REPORT_SEVERITY_ERROR,
                  "sme_SelectRsnMatch. uRsnIECount=%d, uLength=%d; required copy of %d bytes exceeds the buffer limit %d\n",
                   uRsnIECount, uLength, pRsnIe->hdr[ 1 ] +2, sizeof (uCurRsnData));
           handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
           return TI_FALSE;
        }
        uCurRsnData[ 0 + uLength ] = pRsnIe->hdr[ 0 ];
        uCurRsnData[ 1 + uLength ] = pRsnIe->hdr[ 1 ];
        os_memoryCopy (pSme->hOS, &uCurRsnData[ 2 + uLength ], pRsnIe->rsnIeData, pRsnIe->hdr[ 1 ]); 
        uLength += pRsnIe->hdr[ 1 ] + 2;
        pRsnIe += 1;
        uRsnIECount++;
    }
    /* sanity check - make sure RSN IE's size is not too big */
    if (uLength < pCurrentSite->rsnIeLen) 
    {
        TRACE2(pSme->hReport, REPORT_SEVERITY_ERROR , "sme_SelectRsnMatch, RSN IE is too long: rsnIeLen=%d, MAX_RSN_IE=%d\n", pCurrentSite->rsnIeLen, MAX_RSN_IE);
    }

    /* call the RSN to evaluate the site */
    tRsnData.pIe = (pCurrentSite->rsnIeLen == 0) ? NULL : uCurRsnData;
    tRsnData.ieLen = pCurrentSite->rsnIeLen;
    tRsnData.privacy = pCurrentSite->privacy;
    if (rsn_evalSite (pSme->hRsn, &tRsnData, &tRsnSiteParams , &uMetric) != TI_OK)
    {
        /* no match */
        return TI_FALSE;
    }
    else
    {
        /* match! */
        return TI_TRUE;
    }
}

