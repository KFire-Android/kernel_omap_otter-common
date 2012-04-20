/*
 * systemConfig.c
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

/** \file reportReplvl.c
 *  \brief Report level implementation
 *
 *  \see reportReplvl.h 
 */

/***************************************************************************/
/*																			*/
/*		MODULE:	reportReplvl.c												*/
/*    PURPOSE:	Report level implementation	 								*/
/*																			*/
/***************************************************************************/
#define __FILE_ID__  FILE_ID_87
#include "tidef.h"
#include "osApi.h"
#include "siteHash.h"
#include "sme.h"
#include "rate.h"
#include "smeApi.h"
#include "rsnApi.h"
#include "report.h"
#include "TWDriver.h"
#include "connApi.h"
#include "DataCtrl_Api.h"  
#include "siteMgrApi.h"  
#include "EvHandler.h"
#include "TI_IPC_Api.h"
#include "regulatoryDomainApi.h"
#include "measurementMgrApi.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#include "TransmitPowerXCC.h"
#include "XCCRMMngr.h"
#endif

#include "qosMngr_API.h"
#include "StaCap.h"


/****************************************************************************
								MATRIC ISSUE							
	Each function in the select process returns a MATCH, NO_MATCH value in order to 	
	skip non relevant sites. In addition, some of the functions also measures a matching level of a site.
	The matching level is returned as a bit map. The select function 'OR's those bit maps in order to 
	select the site that has the biggest matching level. If a function returns a NO_MATCH value, the matching level of the
	site is reset.
	Following is the site matching level bit map structure.
	Please notice, that if all the match functions returned MATCH for a site, its matric must be different than 0, 
	because of the rates bits.
	

	    31 - 24           23 - 20           20 - 16             15 - 10       9 - 8         7         6           5         4 - 0
	+---------------+---------------+-----------------------+-------------+------------+----------+---------+-----------+-----------+
	| Rx Level      | Privacy       | Attempts              |Rates        | Modulation |Preamble  | Channel | Spectrum  | Reserved  |
	|		        |	    		|		                | 		      |			   |		  |		    | management|		    |	
	+---------------+---------------+-----------------------+-------------+------------+----------+---------+-----------+-----------+
****************************************************************************/

/* Matric bit map definition */
typedef enum 
{
	/* Rx level */
	METRIC_RX_LEVEL_MASK			= 0xFF,
	METRIC_RX_LEVEL_SHIFT			= 24,

	/* Privacy */
	METRIC_PRIVACY_MASK				= 0x0F,
	METRIC_PRIVACY_SHIFT			= 20,

	/* Number of attempts */
	METRIC_ATTEMPTS_NUMBER_MASK		= 0x0F,
	METRIC_ATTEMPTS_NUMBER_SHIFT	= 16,
	
	
	/* Rates */
	METRIC_RATES_MASK				= 0x3F,
	METRIC_RATES_SHIFT				= 10,

	/* PBCC */
	METRIC_MODULATION_MASK			= 0x03,
	METRIC_MODULATION_SHIFT			= 8,
	
	/* Preamble*/
	METRIC_PREAMBLE_MASK			= 0x01,
	METRIC_PREAMBLE_SHIFT			= 7,

	/* Channel */
	METRIC_CHANNEL_MASK				= 0x01,
	METRIC_CHANNEL_SHIFT			= 6,

	/* Spectrum management Capability */
	METRIC_SPECTRUM_MANAGEMENT_MASK	= 0x01,
	METRIC_SPECTRUM_MANAGEMENT_SHIFT= 5,

	/* Priority Site */
	METRIC_PRIORITY_SITE_MASK		= 0x01,
	METRIC_PRIORITY_SITE_SHIFT		= 4

} matric_e;

#define MAX_GB_MODE_CHANEL		14

#define MAX_RSN_DATA_SIZE       256

/* RSSI values boundaries and metric values for best, good, etc  signals */
#define SELECT_RSSI_BEST_LEVEL      (-22)
#define SELECT_RSSI_GOOD_LEVEL      (-38)
#define SELECT_RSSI_NORMAL_LEVEL    (-56)
#define SELECT_RSSI_POOR_LEVEL      (-72)
#define SELECT_RSSI_BAD_LEVEL       (-82)


#define  RSSI_METRIC_BEST      6
#define  RSSI_METRIC_GOOD      5
#define  RSSI_METRIC_NORMAL    4
#define  RSSI_METRIC_POOR      3
#define  RSSI_METRIC_BAD       2
#define  RSSI_METRIC_NOSIGNAL  1

/* Local functions prototypes */

static TI_STATUS sendProbeResponse(siteMgr_t *pSiteMgr, TMacAddr *pBssid);

/* Interface functions Implementation */

/***********************************************************************
 *                        addSelfSite									
 ***********************************************************************
DESCRIPTION: This function is called if the selection fails and desired BSS type is IBSS
			That means we creating our own network and wait for other stations to join us.
			the best site for teh station. 
			Performs the following:
				-	If the desired BSSID is broadcast, we generate a random BSSId, otherwise we use the desired one.
				-	If the site table is full we remove the most old site
				-	We send a probe response with our oiwn desired attributes in order to add the site to the site table
                                                                                                   
INPUT:      pSiteMgr	-	site mgr handle.

OUTPUT:		

RETURN:     Pointer to rthe self site entry in the site table

************************************************************************/
siteEntry_t *addSelfSite(TI_HANDLE hSiteMgr)
{
	siteMgr_t       *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t		*pSite;
	TMacAddr		bssid; 
    TSsid           *pSsid    = &pSiteMgr->pDesiredParams->siteMgrDesiredSSID;

	if (OS_802_11_SSID_JUNK (pSsid->str, pSsid->len))
		return NULL;

	if ((MAC_BROADCAST (pSiteMgr->pDesiredParams->siteMgrDesiredBSSID)) ||
		(BSS_INDEPENDENT == pSiteMgr->pDesiredParams->siteMgrDesiredBSSType))
	{
		MAC_COPY (bssid, pSiteMgr->ibssBssid);
	}
	else
	{
		MAC_COPY (bssid, pSiteMgr->pDesiredParams->siteMgrDesiredBSSID);  
	}

	if(pSiteMgr->pDesiredParams->siteMgrDesiredChannel <= 14)
	{
		pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
        pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;
	}
	else
	{
		pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
		pSiteMgr->siteMgrOperationalMode = DOT11_A_MODE;
	}

    siteMgr_ConfigRate(pSiteMgr);

	/* First make sure that there is a place in the site table, if not, reomve the eldest site. */
	if (pSiteMgr->pSitesMgmtParams->pCurrentSiteTable->numOfSites == pSiteMgr->pSitesMgmtParams->pCurrentSiteTable->maxNumOfSites)
		removeEldestSite(pSiteMgr);

	sendProbeResponse(pSiteMgr, &bssid);

	/* Now find the site in the site table. */
	pSite = findSiteEntry(pSiteMgr, &bssid);
	if (pSite == NULL)
	{
		return NULL;
	}
	pSite->beaconModulation = pSite->probeModulation;
	pSite->barkerPreambleType = PREAMBLE_UNSPECIFIED;

   	pSiteMgr->pSitesMgmtParams->pPrimarySite = pSite;
	pSite->siteType = SITE_SELF;
    pSite->bssType = BSS_INDEPENDENT;

	return pSite;
}

/***********************************************************************
 *                        sendProbeResponse									
 ***********************************************************************
DESCRIPTION: This function is called by the function 'addSelfSite()' in order to send a probe response
			to the site mgr. This will cause the site manager to add a new entry to the site table, the self site entry.

INPUT:      pSiteMgr	-	site mgr handle.
			pBssid		-	Received BSSID

OUTPUT:		

RETURN:     TI_OK

************************************************************************/
static TI_STATUS sendProbeResponse(siteMgr_t *pSiteMgr, TMacAddr *pBssid)
{
	mlmeFrameInfo_t		frame;
    ECipherSuite        rsnStatus;
	dot11_SSID_t 		ssid;	   
	dot11_RATES_t 		rates;	   
	dot11_FH_PARAMS_t 	FHParamsSet;	   
	dot11_DS_PARAMS_t 	DSParamsSet;	   
	dot11_CF_PARAMS_t 	CFParamsSet;	   
	dot11_IBSS_PARAMS_t IBSSParamsSet;
	TI_UINT32			len = 0, ofdmIndex = 0;
    ERadioBand          band;
	dot11_RATES_t 		extRates;
	TI_UINT8			ratesBuf[DOT11_MAX_SUPPORTED_RATES];	
	TI_BOOL				extRatesInd = TI_FALSE;
	
	/* The easiest way to add a site to the site table is to simulate a probe frame. */
	frame.subType = PROBE_RESPONSE;
	os_memoryZero(pSiteMgr->hOs, &frame, sizeof(mlmeFrameInfo_t));
		/* Initialize the frame fields */
	frame.subType = PROBE_RESPONSE;
	os_memoryZero(pSiteMgr->hOs, (void *)frame.content.iePacket.timestamp, TIME_STAMP_LEN);

	/* Build  Beacon interval  */
	frame.content.iePacket.beaconInerval = pSiteMgr->pDesiredParams->siteMgrDesiredBeaconInterval;

	/* Build  capability field */
	frame.content.iePacket.capabilities = 0;
	frame.content.iePacket.capabilities |= (TI_TRUE << CAP_IBSS_SHIFT); /* Bss type must be independent */

	if ((pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType == PREAMBLE_SHORT))
		frame.content.iePacket.capabilities |= (TI_TRUE << CAP_PREAMBLE_SHIFT);

	/* call RSN to get the privacy desired */
    rsn_getParamEncryptionStatus(pSiteMgr->hRsn, &rsnStatus); /* RSN_ENCRYPTION_STATUS_PARAM */
	if (rsnStatus == TWD_CIPHER_NONE)
	{
		frame.content.iePacket.capabilities |= (TI_FALSE << CAP_PRIVACY_SHIFT);
	} 
    else 
    {
		frame.content.iePacket.capabilities |= (TI_TRUE << CAP_PRIVACY_SHIFT);
	}
	
	if (pSiteMgr->pDesiredParams->siteMgrDesiredModulationType == DRV_MODULATION_PBCC)
		frame.content.iePacket.capabilities |= (TI_TRUE << CAP_PBCC_SHIFT);
	
    if (pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE)
    {
        if(pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime == PHY_SLOT_TIME_SHORT)
            frame.content.iePacket.capabilities |= (TI_TRUE << CAP_SLOT_TIME_SHIFT);
    }
	
	/* Build ssid */
	os_memoryZero(pSiteMgr->hOs, (void *)ssid.serviceSetId, MAX_SSID_LEN);

    ssid.hdr[1] = pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len;   
    if (ssid.hdr[1] > MAX_SSID_LEN)
    {
        TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_ERROR,
               "sendProbeResponse. siteMgrDesiredSSID.len=%d exceeds the limit %d\n",
               pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len, MAX_SSID_LEN);
        ssid.hdr[1] = MAX_SSID_LEN;
    }
    os_memoryCopy(pSiteMgr->hOs, (void *)ssid.serviceSetId, (void *)pSiteMgr->pDesiredParams->siteMgrDesiredSSID.str, ssid.hdr[1]);
	
	if(pSiteMgr->pDesiredParams->siteMgrDesiredChannel <= MAX_GB_MODE_CHANEL)
		siteMgr_updateRates(pSiteMgr, TI_FALSE, TI_TRUE);
	else
		siteMgr_updateRates(pSiteMgr, TI_TRUE, TI_TRUE);

	/* Build Rates */
	rate_DrvBitmapToNetStr (pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask,
						    pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask,
                            ratesBuf, 
                            &len, 
                            &ofdmIndex);

	if(pSiteMgr->siteMgrOperationalMode != DOT11_G_MODE ||
       pSiteMgr->pDesiredParams->siteMgrUseDraftNum == DRAFT_5_AND_EARLIER ||
	   ofdmIndex == len)
	{
		rates.hdr[0] = DOT11_SUPPORTED_RATES_ELE_ID;
		rates.hdr[1] = len;
		os_memoryCopy(pSiteMgr->hOs, (void *)rates.rates, ratesBuf, rates.hdr[1]);
	}
	else
	{
		rates.hdr[0] = DOT11_SUPPORTED_RATES_ELE_ID;
		rates.hdr[1] = ofdmIndex;
		os_memoryCopy(pSiteMgr->hOs, (void *)rates.rates, ratesBuf, rates.hdr[1]);

		extRates.hdr[0] = DOT11_EXT_SUPPORTED_RATES_ELE_ID;
		extRates.hdr[1] = len - ofdmIndex;
		os_memoryCopy(pSiteMgr->hOs, (void *)extRates.rates, &ratesBuf[ofdmIndex], extRates.hdr[1]);
		extRatesInd = TI_TRUE;
	}

    if((pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE) || (pSiteMgr->siteMgrOperationalMode == DOT11_DUAL_MODE)) 
    {
        erpProtectionType_e protType;
        ctrlData_getParamProtType(pSiteMgr->hCtrlData, &protType); /* CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM */
        frame.content.iePacket.useProtection = protType;
    }
    else
    {
        frame.content.iePacket.useProtection = ERP_PROTECTION_NONE;
    }

	/* Build FH */
	os_memoryZero(pSiteMgr->hOs, &FHParamsSet, sizeof(dot11_FH_PARAMS_t));

	/* Build DS */
	DSParamsSet.hdr[1] = 1;
	DSParamsSet.currChannel = pSiteMgr->pDesiredParams->siteMgrDesiredChannel;

	/* Build CF */
	os_memoryZero(pSiteMgr->hOs, &CFParamsSet, sizeof(dot11_CF_PARAMS_t));

	/* Build IBSS */
	os_memoryZero(pSiteMgr->hOs, &IBSSParamsSet, sizeof(dot11_IBSS_PARAMS_t));
	IBSSParamsSet.hdr[1] = 2;
	IBSSParamsSet.atimWindow = pSiteMgr->pDesiredParams->siteMgrDesiredAtimWindow;

	frame.content.iePacket.pSsid = &ssid;
	frame.content.iePacket.pRates = &rates;

	if(extRatesInd)
		frame.content.iePacket.pExtRates = &extRates;
	else
		frame.content.iePacket.pExtRates = NULL;

	frame.content.iePacket.pFHParamsSet = &FHParamsSet;
	frame.content.iePacket.pDSParamsSet = &DSParamsSet;
	frame.content.iePacket.pCFParamsSet = &CFParamsSet;
	frame.content.iePacket.pIBSSParamsSet = &IBSSParamsSet;

    band = ( MAX_GB_MODE_CHANEL >= pSiteMgr->pDesiredParams->siteMgrDesiredChannel ? RADIO_BAND_2_4_GHZ : RADIO_BAND_5_0_GHZ );
	/* Update site */
	siteMgr_updateSite(pSiteMgr, pBssid, &frame ,pSiteMgr->pDesiredParams->siteMgrDesiredChannel, band, TI_FALSE);
	
	return TI_OK;
}

/***********************************************************************
 *                        systemConfig									
 ***********************************************************************
DESCRIPTION: This function is called by the function 'siteMgr_selectSite()' in order to configure
			the system with the chosen site attribute.

INPUT:      pSiteMgr	-	site mgr handle.

OUTPUT:		

RETURN:     TI_OK

************************************************************************/
TI_STATUS systemConfig(siteMgr_t *pSiteMgr)
{ 
	paramInfo_t *pParam;
	siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
	TRsnData	rsnData;
	TI_UINT8	rsnAssocIeLen;
    dot11_RSN_t *pRsnIe;
    TI_UINT8    rsnIECount=0;
    TI_UINT8    *curRsnData;
    TI_UINT16   length;
    TI_UINT16   capabilities;
    TI_UINT16   PktLength=0;
    TI_UINT8	*pIeBuffer=NULL;
    TI_BOOL     b11nEnable;
    TI_BOOL     bWmeEnable;

#ifdef XCC_MODULE_INCLUDED
    TI_UINT8     ExternTxPower;
#endif
	TI_STATUS	status;
	ESlotTime	slotTime;
	TI_UINT32	StaTotalRates;
	dot11_ACParameters_t *p_ACParametersDummy = NULL;
    TtxCtrlHtControl tHtControl;

    curRsnData = os_memoryAlloc(pSiteMgr->hOs, MAX_RSN_DATA_SIZE);
    if (!curRsnData)
        return TI_NOK;
    pParam = (paramInfo_t *)os_memoryAlloc(pSiteMgr->hOs, sizeof(paramInfo_t));
    if (!pParam) {
        os_memoryFree(pSiteMgr->hOs, curRsnData, MAX_RSN_DATA_SIZE);
        return TI_NOK;
    }

	if (pPrimarySite->probeRecv)
	{
		pIeBuffer = pPrimarySite->probeRespBuffer;
		PktLength = pPrimarySite->probeRespLength;
	}
    else if (pPrimarySite->beaconRecv)
	{
		pIeBuffer = pPrimarySite->beaconBuffer;
		PktLength = pPrimarySite->beaconLength;
	}
		
	pSiteMgr->prevRadioBand = pSiteMgr->radioBand;
	
    TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Capabilities, Slot Time Bit = %d (capabilities = %d)\n", (pPrimarySite->capabilities >> CAP_SLOT_TIME_SHIFT) & 1, pPrimarySite->capabilities);
	
	if(pPrimarySite->channel <= MAX_GB_MODE_CHANEL)
	{
		if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_B_MODE)
		{
			pSiteMgr->siteMgrOperationalMode = DOT11_B_MODE;
			slotTime = PHY_SLOT_TIME_LONG;

            TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, ": 11b Mode, Slot Time = %d\n", (TI_UINT8)slotTime);
		}
		else
		{
			pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;

			if (((pPrimarySite->capabilities >> CAP_SLOT_TIME_SHIFT) & CAP_SLOT_TIME_MASK) == PHY_SLOT_TIME_SHORT)
			{
			slotTime = pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime;

            TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, ": 11g Mode, Slot Time = %d (desired)\n", (TI_UINT8)slotTime);
			}
			else
			{
				slotTime = PHY_SLOT_TIME_LONG;

                TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, ": 11g Mode, Slot Time = %d\n", (TI_UINT8) slotTime);
			}
		}

		pSiteMgr->radioBand = RADIO_BAND_2_4_GHZ;
		pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
	}
	else
	{
		pSiteMgr->siteMgrOperationalMode = DOT11_A_MODE;
		pSiteMgr->radioBand = RADIO_BAND_5_0_GHZ;
		slotTime = PHY_SLOT_TIME_SHORT;

        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, ": 11a Mode, Slot Time = %d\n", (TI_UINT8)slotTime);

		pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
	}

	/* since we are moving to the different band, the siteMgr should be reconfigured */
	if(pSiteMgr->prevRadioBand != pSiteMgr->radioBand)
		siteMgr_bandParamsConfig(pSiteMgr, TI_TRUE);

	if(pPrimarySite->channel <= MAX_GB_MODE_CHANEL)
		siteMgr_updateRates(pSiteMgr, TI_FALSE, TI_TRUE);
	else
		siteMgr_updateRates(pSiteMgr, TI_TRUE, TI_TRUE);

	/* configure hal with common core-hal parameters */
	TWD_SetRadioBand (pSiteMgr->hTWD, pSiteMgr->radioBand);

	pPrimarySite->currentSlotTime = slotTime;
	TWD_CfgSlotTime (pSiteMgr->hTWD, slotTime);

	/***************** Config Site Manager *************************/
	/* L.M. Should be fixed, should take into account the AP's rates */ 
	if(pSiteMgr->pDesiredParams->siteMgrDesiredModulationType == DRV_MODULATION_CCK)
		pSiteMgr->chosenModulation = DRV_MODULATION_CCK;
	else if(pSiteMgr->pDesiredParams->siteMgrDesiredModulationType == DRV_MODULATION_PBCC)
	{
		if(pPrimarySite->probeModulation != DRV_MODULATION_NONE)
			pSiteMgr->chosenModulation = pPrimarySite->probeModulation;
		else
			pSiteMgr->chosenModulation = pPrimarySite->beaconModulation;
	}
	else
		pSiteMgr->chosenModulation = DRV_MODULATION_OFDM;
	
	/* We use this variable in order tp perform the PBCC algorithm. */
	pSiteMgr->currentDataModulation = pSiteMgr->chosenModulation;
	/***************** Config Data CTRL *************************/
	
	pParam->paramType = CTRL_DATA_CURRENT_BSSID_PARAM;							/* Current BSSID */
	MAC_COPY (pParam->content.ctrlDataCurrentBSSID, pPrimarySite->bssid);
	ctrlData_setParam(pSiteMgr->hCtrlData, pParam);

	pParam->paramType = CTRL_DATA_CURRENT_BSS_TYPE_PARAM;							/* Current BSS Type */
	pParam->content.ctrlDataCurrentBssType = pPrimarySite->bssType;
	ctrlData_setParam(pSiteMgr->hCtrlData, pParam);

	pParam->paramType = CTRL_DATA_CURRENT_PREAMBLE_TYPE_PARAM;					/* Current Preamble Type */
	if ((pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType == PREAMBLE_SHORT) &&
		(pPrimarySite->currentPreambleType == PREAMBLE_SHORT))
		pParam->content.ctrlDataCurrentPreambleType = PREAMBLE_SHORT;
	else
		pParam->content.ctrlDataCurrentPreambleType = PREAMBLE_LONG;
	ctrlData_setParam(pSiteMgr->hCtrlData, pParam);

    /* Mutual Rates Matching */
	StaTotalRates = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask |
					pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask;


    pSiteMgr->pDesiredParams->siteMgrMatchedSuppRateMask = StaTotalRates & 
														   pPrimarySite->rateMask.supportedRateMask;
	
	pSiteMgr->pDesiredParams->siteMgrMatchedBasicRateMask = StaTotalRates &
															pPrimarySite->rateMask.basicRateMask;
	if (pSiteMgr->pDesiredParams->siteMgrMatchedBasicRateMask == 0)
	{
		pSiteMgr->pDesiredParams->siteMgrMatchedBasicRateMask = 
			pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask;
	}

    /* set protection */
    if(BSS_INDEPENDENT == pPrimarySite->bssType)
    {
        pParam->paramType = CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM;
    }
    else
    {   
        pParam->paramType = CTRL_DATA_CURRENT_PROTECTION_STATUS_PARAM;
    }
    pParam->content.ctrlDataProtectionEnabled = pPrimarySite->useProtection;
    ctrlData_setParam(pSiteMgr->hCtrlData, pParam);

	pbccAlgorithm(pSiteMgr);

	/********** Set Site QOS protocol support *************/

	/* Set WME Params */
	 status = siteMgr_getWMEParamsSite(pSiteMgr,&p_ACParametersDummy);
	 if(status == TI_OK)
	 {
		 pParam->content.qosSiteProtocol = QOS_WME;
	 }
	 else
	 {
		 pParam->content.qosSiteProtocol = QOS_NONE;
	 }

     TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, " systemConfigt() : pParam->content.qosSiteProtoco %d\n", pParam->content.qosSiteProtocol);

	 pParam->paramType = QOS_MNGR_SET_SITE_PROTOCOL;
	 qosMngr_setParams(pSiteMgr->hQosMngr, pParam);
	 
     /* Set active protocol in qosMngr according to station desired mode and site capabilities 
	    Must be called BEFORE setting the "CURRENT_PS_MODE" into the QosMngr */
     qosMngr_selectActiveProtocol(pSiteMgr->hQosMngr);

	 /* set PS capability parameter */
	 pParam->paramType = QOS_MNGR_CURRENT_PS_MODE;
	 if(pPrimarySite->APSDSupport == TI_TRUE)
		 pParam->content.currentPsMode = PS_SCHEME_UPSD_TRIGGER;
	 else
		 pParam->content.currentPsMode = PS_SCHEME_LEGACY;
      qosMngr_setParams(pSiteMgr->hQosMngr, pParam);

     /* Set upsd/ps_poll configuration */
     /* Must be done AFTER setting the active Protocol */
     qosMngr_setAcPsDeliveryMode (pSiteMgr->hQosMngr);
	

     /********** Set Site HT setting support *************/
     /* set HT setting to the FW */
     /* verify 11n_Enable and Chip type */
     StaCap_IsHtEnable (pSiteMgr->hStaCap, &b11nEnable);

     /* verify that WME flag enable */
     qosMngr_GetWmeEnableFlag (pSiteMgr->hQosMngr, &bWmeEnable); 

     if ((b11nEnable != TI_FALSE) &&
         (bWmeEnable != TI_FALSE) &&
         (pPrimarySite->tHtCapabilities.tHdr[0] != TI_FALSE) && 
         (pPrimarySite->tHtInformation.tHdr[0] != TI_FALSE))
     {
         TWD_CfgSetFwHtCapabilities (pSiteMgr->hTWD, &pPrimarySite->tHtCapabilities, TI_TRUE);
         TWD_CfgSetFwHtInformation (pSiteMgr->hTWD, &pPrimarySite->tHtInformation);

         /* the FW not supported in HT control field in TX */

        tHtControl.bHtEnable = TI_FALSE;
         txCtrlParams_SetHtControl (pSiteMgr->hTxCtrl, &tHtControl);
     }
     else
     {
         TWD_CfgSetFwHtCapabilities (pSiteMgr->hTWD, &pPrimarySite->tHtCapabilities, TI_FALSE);

         tHtControl.bHtEnable = TI_FALSE;
         txCtrlParams_SetHtControl (pSiteMgr->hTxCtrl, &tHtControl);
     }

	/***************** Config RSN *************************/
    /* Get the RSN IE data */
    pRsnIe = pPrimarySite->pRsnIe;
	length = 0;
    rsnIECount = 0;
    while ((length < pPrimarySite->rsnIeLen) && (pPrimarySite->rsnIeLen < 255) 
           && (rsnIECount < MAX_RSN_IE))
    {
        curRsnData[0+length] = pRsnIe->hdr[0];
        curRsnData[1+length] = pRsnIe->hdr[1];
        os_memoryCopy(pSiteMgr->hOs, &curRsnData[2+length], (void *)pRsnIe->rsnIeData, pRsnIe->hdr[1]); 
        length += pRsnIe->hdr[1]+2;
        pRsnIe += 1;
        rsnIECount++;
    }
    if (length<pPrimarySite->rsnIeLen) 
    {
        TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "siteMgr_selectSiteFromTable, RSN IE is too long: rsnIeLen=%d, MAX_RSN_IE=%d\n", pPrimarySite->rsnIeLen, MAX_RSN_IE);
    }

	rsnData.pIe = (pPrimarySite->rsnIeLen==0) ? NULL : curRsnData;
	rsnData.ieLen = pPrimarySite->rsnIeLen;
    rsnData.privacy = pPrimarySite->privacy; 
    
    rsn_setSite(pSiteMgr->hRsn, &rsnData, NULL, &rsnAssocIeLen);

	/***************** Config RegulatoryDomain **************************/
	
#ifdef XCC_MODULE_INCLUDED
	/* set XCC TPC if present */
	if(XCC_ParseClientTP(pSiteMgr->hOs,pPrimarySite,(TI_INT8 *)&ExternTxPower,pIeBuffer,PktLength) == TI_OK)
    {
        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Select XCC_ParseClientTP == OK: Dbm = %d\n",ExternTxPower);
        pParam->paramType = REGULATORY_DOMAIN_EXTERN_TX_POWER_PREFERRED;
        pParam->content.ExternTxPowerPreferred = ExternTxPower;
        regulatoryDomain_setParam(pSiteMgr->hRegulatoryDomain, pParam);
    }
	/* Parse and save the XCC Version Number if exists */
	XCCMngr_parseXCCVer(pSiteMgr->hXCCMngr, pIeBuffer, PktLength);

#endif

	/* Note: TX Power Control adjustment is now done through siteMgr_assocReport() */
	if (pPrimarySite->powerConstraint>0)
	{	/* setting power constraint */
		pParam->paramType = REGULATORY_DOMAIN_SET_POWER_CONSTRAINT_PARAM;
		pParam->content.powerConstraint = pPrimarySite->powerConstraint;
		regulatoryDomain_setParam(pSiteMgr->hRegulatoryDomain, pParam);
	}


	/***************** Config MeasurementMgr object **************************/
    capabilities = pPrimarySite->capabilities;

    /* Updating the Measurement Module Mode */
    measurementMgr_setMeasurementMode(pSiteMgr->hMeasurementMgr, capabilities, 
									pIeBuffer, PktLength);
    os_memoryFree(pSiteMgr->hOs, curRsnData, MAX_RSN_DATA_SIZE);
    os_memoryFree(pSiteMgr->hOs, pParam, sizeof(paramInfo_t));
	return TI_OK;
}

