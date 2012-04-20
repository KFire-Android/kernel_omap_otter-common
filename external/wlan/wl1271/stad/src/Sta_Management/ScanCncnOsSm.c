/*
 * ScanCncnOsSm.c
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

/** \file  ScanCncnOsSm.c
 *  \brief Scan concentrator OS scan state machine implementation
 *
 *  \see   ScanCncnApp.c
 */


#define __FILE_ID__  FILE_ID_78
#include "osTIType.h"
#include "GenSM.h"
#include "ScanCncnOsSm.h"
#include "ScanCncn.h"
#include "ScanCncnPrivate.h"
#include "report.h"
#include "osApi.h"
#include "siteMgrApi.h"
#include "regulatoryDomainApi.h"
#include "scanResultTable.h"

#define SCAN_OID_DEFAULT_PROBE_REQUEST_RATE_G                       RATE_MASK_UNSPECIFIED  /* Let the FW select */
#define SCAN_OID_DEFAULT_PROBE_REQUEST_RATE_A                       RATE_MASK_UNSPECIFIED  /* Let the FW select */
#define SCAN_OID_DEFAULT_PROBE_REQUEST_NUMBER_G                     3
#define SCAN_OID_DEFAULT_PROBE_REQUEST_NUMBER_A                     3
#define SCAN_OID_DEFAULT_MAX_DWELL_TIME_PASSIVE_G                   100000
#define SCAN_OID_DEFAULT_MAX_DWELL_TIME_PASSIVE_A                   100000
#define SCAN_OID_DEFAULT_MAX_DWELL_TIME_ACTIVE_G                    30000
#define SCAN_OID_DEFAULT_MAX_DWELL_TIME_ACTIVE_A                    30000
#define SCAN_OID_DEFAULT_MIN_DWELL_TIME_PASSIVE_G                   100000
#define SCAN_OID_DEFAULT_MIN_DWELL_TIME_PASSIVE_A                   100000
#define SCAN_OID_DEFAULT_MIN_DWELL_TIME_ACTIVE_G                    15000
#define SCAN_OID_DEFAULT_MIN_DWELL_TIME_ACTIVE_A                    15000
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_PASSIVE_G          SCAN_ET_COND_BEACON
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_PASSIVE_A          SCAN_ET_COND_BEACON
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_ACTIVE_G           SCAN_ET_COND_ANY_FRAME
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_ACTIVE_A           SCAN_ET_COND_ANY_FRAME

/* For WiFi  WPA OOB scenario, 4 APs need to be configure on the same channel */
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_PASSIVE_G          4 
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_PASSIVE_A          4
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_ACTIVE_G           4
#define SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_ACTIVE_A           4

static void scanCncnOsSm_ActionStartGScan (TI_HANDLE hScanCncn);
static void scanCncnOsSm_ActionStartAScan (TI_HANDLE hScanCncn);
static void scanCncnOsSm_ActionCompleteScan (TI_HANDLE hScanCncn);
static void scanCncnOsSm_ActionUnexpected (TI_HANDLE hScanCncn);
TI_UINT32   scanCncnOsSm_FillAllAvailableChannels (TI_HANDLE hScanCncn, ERadioBand eBand, EScanType eScanType,
                                                   TScanChannelEntry *pChannelArray, TI_UINT32 uMaxDwellTime,
                                                   TI_UINT32 uMinChannelTime, EScanEtCondition eETCondition,
                                                   TI_UINT8 uETFrameNumber);


static TGenSM_actionCell tSmMatrix[ SCAN_CNCN_OS_SM_NUMBER_OF_STATES ][ SCAN_CNCN_OS_SM_NUMBER_OF_EVENTS ] = 
    {
        { /* SCAN_CNCN_OS_SM_STATE_IDLE */
            { SCAN_CNCN_OS_SM_STATE_SCAN_ON_G,  scanCncnOsSm_ActionStartGScan },    /* SCAN_CNCN_OS_SM_EVENT_START_SCAN */
            { SCAN_CNCN_OS_SM_STATE_IDLE,       scanCncnOsSm_ActionUnexpected },    /* SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE */
        },
        { /* SCAN_CNCN_OS_SM_STATE_SCAN_ON_G */
            { SCAN_CNCN_OS_SM_STATE_SCAN_ON_G,  scanCncnOsSm_ActionUnexpected },    /* SCAN_CNCN_OS_SM_EVENT_START_SCAN */
            { SCAN_CNCN_OS_SM_STATE_SCAN_ON_A,  scanCncnOsSm_ActionStartAScan },    /* SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE */
        },
        { /* SCAN_CNCN_OS_SM_STATE_SCAN_ON_A */
            { SCAN_CNCN_OS_SM_STATE_SCAN_ON_A,  scanCncnOsSm_ActionUnexpected },    /* SCAN_CNCN_OS_SM_EVENT_START_SCAN */
            { SCAN_CNCN_OS_SM_STATE_IDLE,       scanCncnOsSm_ActionCompleteScan },  /* SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE */
        }
    };

static TI_INT8  *uStateDescription[] = 
    {
        "IDLE",
        "SCAN_ON_G",
        "SCAN_ON_A"
    };

static TI_INT8  *uEventDescription[] = 
    {
        "START",
        "SCAN_COMPLETE"
    };

/** 
 * \fn     scanCncnOsSm_Create
 * \brief  Creates the OS scan state-machine
 * 
 * creates the OS scan state-machine
 * 
 * \param  hScanCncn - handle to the scan concentrator object
 * \return Handle to the newly created OS san SM, NULL if an error occured
 * \sa     scanCncnOsSm_Create, scanCncnOsSm_Destroy
 */
TI_HANDLE scanCncnOsSm_Create (TI_HANDLE hScanCncn)
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;

    return genSM_Create (pScanCncn->hOS);
}

/** 
 * \fn     scanCncnOsSm_Init
 * \brief  Initialize the OS scan state-machine
 * 
 * Initialize the OS scan state-machine
 * 
 * \param  hScanCncn - handle to the scan concentrator object
 * \return None
 * \sa     scanCncnOsSm_Create
 */
void scanCncnOsSm_Init (TI_HANDLE hScanCncn)
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;

    /* initialize the state-machine */
    genSM_Init (pScanCncn->hOSScanSm, pScanCncn->hReport);
    genSM_SetDefaults (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_NUMBER_OF_STATES, SCAN_CNCN_OS_SM_NUMBER_OF_EVENTS,
                       (TGenSM_matrix)tSmMatrix, SCAN_CNCN_OS_SM_STATE_IDLE, "OS scan SM", uStateDescription, 
                       uEventDescription, __FILE_ID__);
}

/** 
 * \fn     scanCncnOsSm_Destroy
 * \brief  Destroys the OS scan state-machine
 * 
 * Destroys the OS scan state-machine
 * 
 * \param  hScanCncn - handle to the scan concentrator object
 * \return None
 * \sa     scanCncnOsSm_Create
 */
void scanCncnOsSm_Destroy (TI_HANDLE hScanCncn)
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;

    genSM_Unload (pScanCncn->hOSScanSm);
}

/** 
 * \fn     scanCncnOsSm_ActionStartGScan
 * \brief  Scan concentartor OS state machine start scan on G action function
 * 
 * Scan concentartor OS state machine start scan on G action function.
 * Starts a sacn on G using all allowed channels
 * 
 * \param  hScanCncn - handle to the scan concentartor object
 * \return None
 */ 
void scanCncnOsSm_ActionStartGScan (TI_HANDLE hScanCncn)
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;
    paramInfo_t     tParam;
    TI_UINT32       uValidChannelsCount;
    TI_BOOL         bRegulatoryDomainEnabled;

    /* if the STA is not configured for G band or dual band, send a scan complete event to the SM */
    tParam.paramType = SITE_MGR_DESIRED_DOT11_MODE_PARAM;
    siteMgr_getParam (pScanCncn->hSiteManager, &tParam);
    if ((DOT11_G_MODE != tParam.content.siteMgrDot11Mode) && (DOT11_DUAL_MODE != tParam.content.siteMgrDot11Mode))
    {
        TRACE0(pScanCncn->hReport , REPORT_SEVERITY_INFORMATION , "scanCncnOsSm_ActionStartGScan: STA does not work on 2.4 GHz, continuing to 5.0 GHz scan\n");
        genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
        return;
    }

    /* build scan command header */
    pScanCncn->tOsScanParams.band = RADIO_BAND_2_4_GHZ;
    pScanCncn->tOsScanParams.Tid = 255;

    /* query the regulatory domain if 802.11d is in use */
    tParam.paramType = REGULATORY_DOMAIN_ENABLED_PARAM;
    regulatoryDomain_getParam (pScanCncn->hRegulatoryDomain, &tParam );
    bRegulatoryDomainEnabled = tParam.content.regulatoryDomainEnabled;

    /* Get country code status */
    tParam.paramType          = REGULATORY_DOMAIN_IS_COUNTRY_FOUND;
    tParam.content.eRadioBand = RADIO_BAND_2_4_GHZ;
    regulatoryDomain_getParam (pScanCncn->hRegulatoryDomain, &tParam);

    /* scan type is passive if 802.11d is enabled and country IE was not yet found, active otherwise */
    if (((TI_TRUE == bRegulatoryDomainEnabled) && (TI_FALSE == tParam.content.bIsCountryFound)) || SCAN_TYPE_TRIGGERED_PASSIVE == pScanCncn->tOsScanParams.scanType)
    {
		pScanCncn->tOsScanParams.scanType = SCAN_TYPE_TRIGGERED_PASSIVE;
    }
	/* All paramters in the func are hard coded, due to that we set to active if not passive */
    else
    {
        pScanCncn->tOsScanParams.scanType = SCAN_TYPE_TRIGGERED_ACTIVE;
        /* also set number and rate of probe requests */
        pScanCncn->tOsScanParams.probeReqNumber = SCAN_OID_DEFAULT_PROBE_REQUEST_NUMBER_G;
        pScanCncn->tOsScanParams.probeRequestRate = (ERateMask)SCAN_OID_DEFAULT_PROBE_REQUEST_RATE_G;
    }
    
    /* add supported channels on G */
    if (SCAN_TYPE_NORMAL_PASSIVE == pScanCncn->tOsScanParams.scanType )
    {
        uValidChannelsCount = scanCncnOsSm_FillAllAvailableChannels (hScanCncn, RADIO_BAND_2_4_GHZ, 
                                       SCAN_TYPE_NORMAL_PASSIVE, &(pScanCncn->tOsScanParams.channelEntry[0]),
                                       SCAN_OID_DEFAULT_MAX_DWELL_TIME_PASSIVE_G,
                                       SCAN_OID_DEFAULT_MIN_DWELL_TIME_PASSIVE_G,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_PASSIVE_G,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_PASSIVE_G);
    }
    else
    {
        uValidChannelsCount = scanCncnOsSm_FillAllAvailableChannels (hScanCncn, RADIO_BAND_2_4_GHZ, 
                                       SCAN_TYPE_NORMAL_ACTIVE, &(pScanCncn->tOsScanParams.channelEntry[0]),
                                       SCAN_OID_DEFAULT_MAX_DWELL_TIME_ACTIVE_G,
                                       SCAN_OID_DEFAULT_MIN_DWELL_TIME_ACTIVE_G,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_ACTIVE_G,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_ACTIVE_G);
    }
    pScanCncn->tOsScanParams.numOfChannels = uValidChannelsCount;

    /* check that some channels are available */
    if ( uValidChannelsCount > 0 )
    {
        EScanCncnResultStatus   eResult;
        
        /* send command to scan concentrator APP SM */
        eResult = scanCncn_Start1ShotScan (hScanCncn, SCAN_SCC_APP_ONE_SHOT, &(pScanCncn->tOsScanParams));

        /* if scan failed, send scan complete event to the SM */
        if (SCAN_CRS_SCAN_RUNNING != eResult)
        {
            TRACE0(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnOsSm_ActionStartGScan: scan failed on 2.4 GHz, continuing to 5.0 GHz scan\n");
            genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
        }
    }
    else
    {
        TRACE0(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnOsSm_ActionStartGScan: no valid cahnnels on 2.4 GHz, continuing to 5.0 GHz scan\n");
        /* no channels to scan, send a scan complete event */
        genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
    }
}

/** 
 * \fn     scanCncnOsSm_ActionStartAScan
 * \brief  Scan concentartor OS state machine start scan on A action function
 * 
 * Scan concentartor OS state machine start scan on A action function.
 * Starts a sacn on A using all allowed channels
 * 
 * \param  hScanCncn - handle to the scan concentartor object
 * \return None
 */ 
void scanCncnOsSm_ActionStartAScan (TI_HANDLE hScanCncn)
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;
    paramInfo_t     tParam;
    TI_UINT32       uValidChannelsCount;
    TI_BOOL         bRegulatoryDomainEnabled;

    /* if the STA is not configured for G band or dual band, send a scan complete event to the SM */
    tParam.paramType = SITE_MGR_DESIRED_DOT11_MODE_PARAM;
    siteMgr_getParam (pScanCncn->hSiteManager, &tParam);
    if ((DOT11_A_MODE != tParam.content.siteMgrDot11Mode) && (DOT11_DUAL_MODE != tParam.content.siteMgrDot11Mode))
    {
        TRACE0(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnOsSm_ActionStartAScan: STA does not work on 5.0 GHz, quitting\n");
        genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
        return;
    }

    /* build scan command header */
    pScanCncn->tOsScanParams.band = RADIO_BAND_5_0_GHZ;
    pScanCncn->tOsScanParams.Tid = 0;

    /* query the regulatory domain if 802.11d is in use */
    tParam.paramType = REGULATORY_DOMAIN_ENABLED_PARAM;
    regulatoryDomain_getParam (pScanCncn->hRegulatoryDomain, &tParam );
    bRegulatoryDomainEnabled = tParam.content.regulatoryDomainEnabled;

    /* Get country code status */
    tParam.paramType          = REGULATORY_DOMAIN_IS_COUNTRY_FOUND;
    tParam.content.eRadioBand = RADIO_BAND_5_0_GHZ;
    regulatoryDomain_getParam (pScanCncn->hRegulatoryDomain, &tParam);

    /* scan type is passive if 802.11d is enabled and country IE was not yet found, active otherwise */
    if (((TI_TRUE == bRegulatoryDomainEnabled) && (TI_FALSE == tParam.content.bIsCountryFound)) || SCAN_TYPE_TRIGGERED_PASSIVE == pScanCncn->tOsScanParams.scanType)
    {
        pScanCncn->tOsScanParams.scanType = SCAN_TYPE_TRIGGERED_PASSIVE;
    }
    /* All paramters in the func are hard coded, due to that we set to active if not passive */
	else
    {
        pScanCncn->tOsScanParams.scanType = SCAN_TYPE_TRIGGERED_ACTIVE;
        /* also set number and rate of probe requests */
        pScanCncn->tOsScanParams.probeReqNumber = SCAN_OID_DEFAULT_PROBE_REQUEST_NUMBER_A;
        pScanCncn->tOsScanParams.probeRequestRate = (ERateMask)SCAN_OID_DEFAULT_PROBE_REQUEST_RATE_A;
    }
    
    /* add supported channels on G */
    if (SCAN_TYPE_NORMAL_PASSIVE == pScanCncn->tOsScanParams.scanType )
    {
        uValidChannelsCount = scanCncnOsSm_FillAllAvailableChannels (hScanCncn, RADIO_BAND_5_0_GHZ, 
                                       SCAN_TYPE_NORMAL_PASSIVE, &(pScanCncn->tOsScanParams.channelEntry[0]),
                                       SCAN_OID_DEFAULT_MAX_DWELL_TIME_PASSIVE_A,
                                       SCAN_OID_DEFAULT_MIN_DWELL_TIME_PASSIVE_A,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_PASSIVE_A,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_PASSIVE_A );
    }
    else
    {
        uValidChannelsCount = scanCncnOsSm_FillAllAvailableChannels (hScanCncn, RADIO_BAND_5_0_GHZ,
                                       SCAN_TYPE_NORMAL_ACTIVE, &(pScanCncn->tOsScanParams.channelEntry[0]),
                                       SCAN_OID_DEFAULT_MAX_DWELL_TIME_ACTIVE_A,
                                       SCAN_OID_DEFAULT_MIN_DWELL_TIME_ACTIVE_A,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_EVENT_ACTIVE_A,
                                       SCAN_OID_DEFAULT_EARLY_TERMINATION_COUNT_ACTIVE_A );
    }
    pScanCncn->tOsScanParams.numOfChannels = uValidChannelsCount;

    /* check that some channels are available */
    if ( uValidChannelsCount > 0 )
    {
        EScanCncnResultStatus   eResult;

       /* send command to scan concentrator APP SM */
        eResult = scanCncn_Start1ShotScan (hScanCncn, SCAN_SCC_APP_ONE_SHOT, &(pScanCncn->tOsScanParams));

        /* if scan failed, send scan complete event to the SM */
        if (SCAN_CRS_SCAN_RUNNING != eResult)
        {
            TRACE0(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnOsSm_ActionStartAScan: scan failed on 5.0 GHz, quitting\n");
            genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
        }
    }
    else
    {
        TRACE0(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnOsSm_ActionStartGScan: no valid cahnnels on 5.0 GHz, quitting\n");
        /* no channels to scan, send a scan complete event */
        genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
    }
}

/** 
 * \fn     scanCncnOsSm_ActionCompleteScan
 * \brief  Scan concentartor OS state machine complete scan action function
 * 
 * Scan concentartor OS state machine complete scan action function.
 * Cleans up after an OS scan cycle - stabilize the scan result table
 * 
 * \param  hScanCncn - handle to the scan concentartor object
 * \return None
 */ 
void scanCncnOsSm_ActionCompleteScan (TI_HANDLE hScanCncn)
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;


	 /*Update the table only if scan was not rejected*/
	 if ( !pScanCncn->pScanClients[ pScanCncn->eCurrentRunningAppScanClient ]->bScanRejectedOn2_4)
	 {
    /* 
     * set the result table to stable state. Note: OID scans are always done for the application, so the
     * results will always be sent to the scan concentartor app scan result table, regardless of the
     * SME connection mode. However, it is expected that the SME will NOT attempt to connect when an OID
     * scan request will be received
     */
		  scanResultTable_SetStableState (pScanCncn->hScanResultTable);
	 }
	 else
	 {
		  pScanCncn->pScanClients[ pScanCncn->eCurrentRunningAppScanClient ]->bScanRejectedOn2_4 = TI_FALSE;
	 }

    /* mark that OID scan process is no longer running */
    pScanCncn->bOSScanRunning = TI_FALSE;
    /* also mark that no app scan client is running */
    pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_NO_CLIENT;
 

    /* no need to send scan complete event - WZC (or equivalent other OS apps) will query for the results */
}

/** 
 * \fn     scanCncnOsSm_FillAllAvailableChannels
 * \brief  Fills a chhanel array with valid channels (and their params) according to band and scan type
 * 
 * Fills a chhanel array with valid channels (and their params) according to band and scan type
 * 
 * \param  hScanCncn - handle to the scan concentrator object
 * \param  eBand - band to extract channels for
 * \param  eScanType - scan type tp ectract channels for
 * \param  pChannelArray - where to store allowed channels information
 * \param  uMaxDwellTime - maximum dwell time value to be used for each channel
 * \param  uMinDwellTime - minimum dwell time value to be used for each channel
 * \param  eETCondition - early termination condition value to be used for each channel
 * \param  uETFrameNumber - early termination frame number value to be used for each channel
 * \return Number of allowed channels (that were placed in the given channels array)
 */ 
TI_UINT32 scanCncnOsSm_FillAllAvailableChannels (TI_HANDLE hScanCncn, ERadioBand eBand, EScanType eScanType,
                                                 TScanChannelEntry *pChannelArray, TI_UINT32 uMaxDwellTime,
                                                 TI_UINT32 uMinChannelTime, EScanEtCondition eETCondition,
                                                 TI_UINT8 uETFrameNumber)
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;
    TI_UINT32       i, j, uAllowedChannelsCount, uValidChannelsCnt = 0;
    paramInfo_t     tParam;
    TI_UINT8        uTempChannelList[ SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND ];
 
    /* get the numnber of supported channels for this band */
    tParam.paramType = REGULATORY_DOMAIN_ALL_SUPPORTED_CHANNELS;
    tParam.content.siteMgrRadioBand = eBand;
    regulatoryDomain_getParam (pScanCncn->hRegulatoryDomain, &tParam);
    uAllowedChannelsCount = tParam.content.supportedChannels.sizeOfList;

    /* for the time being don't scan more channels than fit in one command */
    if (uAllowedChannelsCount > SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND)
    {
        uAllowedChannelsCount = SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND;
    }

    /* Copy allowed channels to reuse param var */
    os_memoryCopy (pScanCncn->hOS, uTempChannelList,
            tParam.content.supportedChannels.listOfChannels, uAllowedChannelsCount );

    /* preapre the param var to request channel allowance for the requested scan type */
    tParam.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
    tParam.content.channelCapabilityReq.band = eBand;

    /* add default values to channels allowed for the requested scan type and band */
    for (i = 0; i < uAllowedChannelsCount; i++)
    {
        /* get specific channel allowance for scan type */
        if ((eScanType == SCAN_TYPE_NORMAL_PASSIVE) ||
            (eScanType == SCAN_TYPE_TRIGGERED_PASSIVE) ||
            (eScanType == SCAN_TYPE_SPS))
        {
            tParam.content.channelCapabilityReq.scanOption = PASSIVE_SCANNING;
        }
        else
        {
            tParam.content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
        }
        tParam.content.channelCapabilityReq.channelNum = uTempChannelList[ i ];

        regulatoryDomain_getParam( pScanCncn->hRegulatoryDomain, &tParam );
        if (TI_TRUE == tParam.content.channelCapabilityRet.channelValidity)
        {
            /* add the channel ID */
            pChannelArray[ uValidChannelsCnt ].normalChannelEntry.channel = uTempChannelList[ i ];

            /* add other default parameters */
            pChannelArray[ uValidChannelsCnt ].normalChannelEntry.minChannelDwellTime = uMinChannelTime;
            pChannelArray[ uValidChannelsCnt ].normalChannelEntry.maxChannelDwellTime = uMaxDwellTime;
            pChannelArray[ uValidChannelsCnt ].normalChannelEntry.earlyTerminationEvent = eETCondition;
            pChannelArray[ uValidChannelsCnt ].normalChannelEntry.ETMaxNumOfAPframes = uETFrameNumber;
            pChannelArray[ uValidChannelsCnt ].normalChannelEntry.txPowerDbm  = 
                tParam.content.channelCapabilityRet.maxTxPowerDbm;

            /* Fill broadcast BSSID */
            for (j = 0; j < 6; j++)
            {
                pChannelArray[ uValidChannelsCnt ].normalChannelEntry.bssId[ j ] = 0xff;
            }
            uValidChannelsCnt++;
        }
    }

    /* return the number of channels that are actually allowed for the requested scan type on the requested band */
    return uValidChannelsCnt;
}

/** 
 * \fn     Function declaration 
 * \brief  Function brief description goes here 
 * 
 * Function detailed description goes here 
 * 
 * \note   Note is indicated here 
 * \param  Parameter name - parameter description
 * \param  … 
 * \return Return code is detailed here 
 * \sa     Reference to other relevant functions 
 */ 
/**
 * \\n
 * \date 11-Jan-2005\n
 * \brief Handles an unexpected event.\n

 *
 * Function Scope \e Private.\n
 * \param hScanCncn - handle to the scan concentrator object.\n
 * \return always OK.\n
 */
void scanCncnOsSm_ActionUnexpected (TI_HANDLE hScanCncn) 
{
    TScanCncn       *pScanCncn = (TScanCncn*)hScanCncn;

    TRACE0(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnOsSm_ActionUnexpected: Unexpeted action for current state\n");
}

