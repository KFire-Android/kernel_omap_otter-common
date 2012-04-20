/*
 * siteMgr.c
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

/** \file siteMgr.c
 *  \brief Site Manager implementation
 *
 *  \see siteMgr.h
 */

/****************************************************************************/
/*                                                                          */
/*      MODULE: siteMgr.c                                                   */
/*    PURPOSE:  Site Manager implementation                                 */
/*                                                                          */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_85
#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "siteMgrApi.h"
#include "siteHash.h"
#include "smeApi.h"
#include "rate.h"
#include "connApi.h"
#include "mlmeSm.h"
#include "sme.h"
#include "DataCtrl_Api.h"
#include "regulatoryDomainApi.h"
#include "rsnApi.h"
#include "measurementMgrApi.h"
#include "qosMngr_API.h"
#include "PowerMgr_API.h"
#include "EvHandler.h"
#include "TI_IPC_Api.h"
#include "MacServices_api.h" 
#include "apConn.h"
#include "currBss.h"
#include "PowerMgr.h"
#include "TWDriver.h"
#include "admCtrl.h"
#include "DrvMainModules.h"
#include "StaCap.h"
#include "freq.h"
#include "currBssApi.h"
#include "CmdBld.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif

/* External function prototypes */
extern TI_STATUS wlanDrvIf_getNetwIpByDevName(TIpAddr devIp, char *devName);

/* definitions */
#define JOIN_RATE_MASK_1M   0x01
#define JOIN_RATE_MASK_2M   0x02
#define JOIN_RATE_MASK_5_5M 0x04
#define JOIN_RATE_MASK_11M  0x08
#define JOIN_RATE_MASK_22M  0x10


#define SITE_MGR_INIT_BIT           1
#define TIMER_INIT_BIT              2
#define DESIRED_PARAMS_INIT_BIT     3
#define MGMT_PARAMS_INIT_BIT        4

#define BUILT_IN_TEST_PERIOD 500

#define KEEP_ALIVE_SEND_NULL_DATA_PERIOD  10000

/* Reconfig constants */
#define SCAN_FAIL_THRESHOLD_FOR_RECONFIG        4  /* After 4 times we reset the 580 register and still no AP found - make recovery */
#define SCAN_FAIL_THRESHOLD_FOR_RESET_REG_580   90  /* After 90 times (45 seconds) and  no AP found - reset the 580 register */
#define SCAN_FAIL_RECONFIG_ENABLED              TI_TRUE
#define SCAN_FAIL_RECONFIG_DISABLED             TI_FALSE

#define TRIGGER_HIGH_TX_PW_PACING 10
#define TRIGGER_LOW_TX_PW_PACING 10
#define TRIGGER_HIGH_TX_PW_HYSTERESIS 3
#define TRIGGER_LOW_TX_PW_HYSTERESIS 3

/* Local Macros */

#define UPDATE_BEACON_INTERVAL(pSite, pFrameInfo)       pSite->beaconInterval = pFrameInfo->content.iePacket.beaconInerval

#define UPDATE_CAPABILITIES(pSite, pFrameInfo)          pSite->capabilities = pFrameInfo->content.iePacket.capabilities

#define UPDATE_PRIVACY(pSite, pFrameInfo)               pSite->privacy = ((pFrameInfo->content.iePacket.capabilities >> CAP_PRIVACY_SHIFT) & CAP_PRIVACY_MASK) ? TI_TRUE : TI_FALSE

#define UPDATE_AGILITY(pSite, pFrameInfo)               pSite->agility = ((pFrameInfo->content.iePacket.capabilities >> CAP_AGILE_SHIFT) & CAP_AGILE_MASK) ? TI_TRUE : TI_FALSE

#define UPDATE_SLOT_TIME(pSite, pFrameInfo)             pSite->newSlotTime = ((pFrameInfo->content.iePacket.capabilities >> CAP_SLOT_TIME_SHIFT) & CAP_SLOT_TIME_MASK) ? PHY_SLOT_TIME_SHORT : PHY_SLOT_TIME_LONG
#define UPDATE_PROTECTION(pSite, pFrameInfo)            pSite->useProtection = (pFrameInfo->content.iePacket.useProtection)

/* 1. no checking Ptrs for NULL; no checking ssid.len for limit & for zero */
/* 2. a similar MACRO is defined in scanResultTable.c
   3. ... but there it contains pSite->ssid.str[pSite->ssid.len] = '\0'; Which of the DEFINEs is correct?*/
#define UPDATE_SSID(pSite, pFrameInfo)                  if (pFrameInfo->content.iePacket.pSsid != NULL) { \
                                                        pSite->ssid.len = pFrameInfo->content.iePacket.pSsid->hdr[1]; \
                                                        if (pSite->ssid.len > MAX_SSID_LEN) { \
                                                            TRACE2( pSiteMgr->hReport, REPORT_SEVERITY_ERROR, \
                                                                  "UPDATE_SSID. pSite->ssid.len=%d exceeds the limit. Set to limit value %d\n", \
                                                                  pSite->ssid.len, MAX_SSID_LEN); \
                                                                  handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION); \
                                                            pSite->ssid.len = MAX_SSID_LEN; \
                                                        } \
                                                        os_memoryCopy(pSiteMgr->hOs, \
                                                        (void *)pSite->ssid.str, \
                                                        (void *)pFrameInfo->content.iePacket.pSsid->serviceSetId, \
                                                        pSite->ssid.len);}

#define UPDATE_CHANNEL(pSite, pFrameInfo, rxChannel)    if (pFrameInfo->content.iePacket.pDSParamsSet == NULL) \
                                                            pSite->channel = rxChannel; \
                                                        else \
                                                            pSite->channel = pFrameInfo->content.iePacket.pDSParamsSet->currChannel;



#define UPDATE_DTIM_PERIOD(pSite, pFrameInfo)           if (pFrameInfo->content.iePacket.pTIM != NULL) \
                                                        pSite->dtimPeriod = pFrameInfo->content.iePacket.pTIM->dtimPeriod

#define UPDATE_ATIM_WINDOW(pSite, pFrameInfo)           if (pFrameInfo->content.iePacket.pIBSSParamsSet != NULL) \
                                                        pSite->atimWindow = pFrameInfo->content.iePacket.pIBSSParamsSet->atimWindow

#define UPDATE_BEACON_AP_TX_POWER(pSite, pFrameInfo)    if (pFrameInfo->content.iePacket.TPCReport != NULL) \
                                                        pSite->APTxPower = pFrameInfo->content.iePacket.TPCReport->transmitPower

#define UPDATE_PROBE_AP_TX_POWER(pSite, pFrameInfo)     if (pFrameInfo->content.iePacket.TPCReport != NULL) \
                                                        pSite->APTxPower = pFrameInfo->content.iePacket.TPCReport->transmitPower

#define UPDATE_BSS_TYPE(pSite, pFrameInfo)              pSite->bssType = ((pFrameInfo->content.iePacket.capabilities >> CAP_ESS_SHIFT) & CAP_ESS_MASK) ? BSS_INFRASTRUCTURE : BSS_INDEPENDENT

#define UPDATE_LOCAL_TIME_STAMP(pSiteMgr, pSite, pFrameInfo)        pSite->localTimeStamp = os_timeStampMs(pSiteMgr->hOs)

/* Updated from beacons */
#define UPDATE_BEACON_MODULATION(pSite, pFrameInfo)     pSite->beaconModulation = ((pFrameInfo->content.iePacket.capabilities >> CAP_PBCC_SHIFT) & CAP_PBCC_MASK) ? DRV_MODULATION_PBCC : DRV_MODULATION_CCK

/* Updated from probes */
#define UPDATE_PROBE_MODULATION(pSite, pFrameInfo)          pSite->probeModulation = ((pFrameInfo->content.iePacket.capabilities >> CAP_PBCC_SHIFT) & CAP_PBCC_MASK) ? DRV_MODULATION_PBCC : DRV_MODULATION_CCK

#define UPDATE_BEACON_RECV(pSite)                       pSite->beaconRecv = TI_TRUE

#define UPDATE_PROBE_RECV(pSite)                        pSite->probeRecv = TI_TRUE


#define UPDATE_RSN_IE(pSite, pRsnIe, rsnIeLen)              if (pRsnIe != NULL) { \
                                                            TI_UINT8 length=0, index=0;\
                                                            pSite->rsnIeLen = rsnIeLen;\
                                                            while ((length < pSite->rsnIeLen) && (index<MAX_RSN_IE)){\
                                                                pSite->pRsnIe[index].hdr[0] = pRsnIe->hdr[0];\
                                                                pSite->pRsnIe[index].hdr[1] = pRsnIe->hdr[1];\
                                                                os_memoryCopy(pSiteMgr->hOs, (void *)pSite->pRsnIe[index].rsnIeData, (void *)pRsnIe->rsnIeData, pRsnIe->hdr[1]);\
                                                                length += (pRsnIe->hdr[1]+2); \
                                                                pRsnIe += 1; \
                                                                index++;}\
                                                        } \
                                                        else {pSite->rsnIeLen = 0;}

#define UPDATE_BEACON_TIMESTAMP(pSiteMgr, pSite, pFrameInfo)    os_memoryCopy(pSiteMgr->hOs, pSite->tsfTimeStamp, (void *)pFrameInfo->content.iePacket.timestamp, TIME_STAMP_LEN)




/* Local  functions definitions*/

static void update_apsd(siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo);

static void release_module(siteMgr_t *pSiteMgr, TI_UINT32 initVec);

static void updateSiteInfo(siteMgr_t *pSiteMgr, mlmeFrameInfo_t *pFrameInfo, siteEntry_t    *pSite, TI_UINT8 rxChannel);

static void updateRates(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo);

static void updateBeaconQosParams(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo);

static void updateProbeQosParams(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo);

static void updateWSCParams(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo);

static void updatePreamble(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo);

static void getPrimarySiteDesc(siteMgr_t *pSiteMgr, OS_802_11_BSSID *pPrimarySiteDesc, TI_BOOL supplyExtendedInfo);

static TI_STATUS getPrimaryBssid(siteMgr_t *pSiteMgr, OS_802_11_BSSID_EX *primaryBssid, TI_UINT32 *pLength);

static ERate translateRateMaskToValue(siteMgr_t *pSiteMgr, TI_UINT32 rateMask);

static void getSupportedRateSet(siteMgr_t *pSiteMgr, TRates *pRatesSet);

static TI_STATUS setSupportedRateSet(siteMgr_t *pSiteMgr, TRates *pRatesSet);

static void siteMgr_externalConfigurationParametersSet(TI_HANDLE hSiteMgr);

static int parseWscMethodFromIE (siteMgr_t *pSiteMgr, dot11_WSC_t *WSCParams, TIWLN_SIMPLE_CONFIG_MODE *pSelectedMethod);

static TI_UINT16 incrementTxSessionCount(siteMgr_t *pSiteMgr);
static void siteMgr_TxPowerAdaptation(TI_HANDLE hSiteMgr, RssiEventDir_e highLowEdge);
static void siteMgr_TxPowerLowThreshold(TI_HANDLE hSiteMgr, TI_UINT8 *data, TI_UINT8 dataLength);
static void siteMgr_TxPowerHighThreshold(TI_HANDLE hSiteMgr, TI_UINT8 *data, TI_UINT8 dataLength);

/************************************************************************
*                        siteMgr_setTemporaryTxPower                    *
*************************************************************************
DESCRIPTION:	This function is used to start the Tx Power Control adjust mechanism
				in regulatoryDomain.

INPUT:      bActivateTempFix             -   Whether the power should be adjusted 
************************************************************************/
void siteMgr_setTemporaryTxPower(siteMgr_t* pSiteMgr, TI_BOOL bActivateTempFix)
{
	paramInfo_t         param;

TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_setTemporaryTxPower is =  \n");

	/* Set the temporary Power Level via the Regulatory Domain*/
	param.paramType = REGULATORY_DOMAIN_TEMPORARY_TX_ATTENUATION_PARAM;
	param.content.bActivateTempPowerFix = bActivateTempFix;
	regulatoryDomain_setParam(pSiteMgr->hRegulatoryDomain,&param);
}

/* Interface functions Implementation */


/*static void UPDATE_RSN_IE (siteMgr_t* pSiteMgr, siteEntry_t   *pSite, dot11_RSN_t *pRsnIe, TI_UINT8 rsnIeLen)
{

    if (pRsnIe != NULL) {
            TI_UINT8 length=0, index=0;
            pSite->rsnIeLen = rsnIeLen;
            while ((length < pSite->rsnIeLen) && (index<MAX_RSN_IE)){
                pSite->pRsnIe[index].hdr = pRsnIe->hdr;
                os_memoryCopy(pSiteMgr->hOs, pSite->pRsnIe[index].rsnIeData, pRsnIe->rsnIeData, pRsnIe->hdr[1]);
                length += (pRsnIe->hdr[1] + 2);
                pRsnIe += 1;
                index++;}
        }
        else {pSite->rsnIeLen = 0;}
}*/
/************************************************************************
 *                        siteMgr_create                                *
 ************************************************************************
DESCRIPTION: Site manager module creation function, called by the config mgr in creation phase
                performs the following:
                -   Allocate the site manager handle
                -   Allocate the desired & mgmt params structure

INPUT:      hOs             -   Handle to OS


OUTPUT:


RETURN:     Handle to the site manager module on success, NULL otherwise
************************************************************************/
TI_HANDLE siteMgr_create(TI_HANDLE hOs)
{
    siteMgr_t       *pSiteMgr;
    TI_UINT32          initVec;

    initVec = 0;

    pSiteMgr = os_memoryAlloc(hOs, sizeof(siteMgr_t));
    if (pSiteMgr == NULL)
    {
        return NULL;
    }

    os_memoryZero(hOs, pSiteMgr, sizeof(siteMgr_t));

    initVec |= (1 << SITE_MGR_INIT_BIT);

    pSiteMgr->pDesiredParams = os_memoryAlloc(hOs, sizeof(siteMgrInitParams_t));
    if (pSiteMgr->pDesiredParams == NULL)
    {
        release_module(pSiteMgr, initVec);
        return NULL;
    }

    initVec |= (1 << DESIRED_PARAMS_INIT_BIT);

    pSiteMgr->pSitesMgmtParams = os_memoryAlloc(hOs, sizeof(sitesMgmtParams_t));
    if (pSiteMgr->pSitesMgmtParams == NULL)
    {
        release_module(pSiteMgr, initVec);
        return NULL;
    }

    initVec |= (1 << MGMT_PARAMS_INIT_BIT);

    pSiteMgr->hOs = hOs;

    return(pSiteMgr);
}



/************************************************************************
 *                        siteMgr_init                                  *
 ************************************************************************
DESCRIPTION: Module init function, called by the DrvMain in init phase
                performs the following:
                -   Reset & initiailzes local variables
                -   Init the handles to be used by the module

INPUT:      pStadHandles -  List of handles to be used by the module

OUTPUT:

RETURN:     void
************************************************************************/
void siteMgr_init (TStadHandlesList *pStadHandles)
{ 
    siteMgr_t *pSiteMgr = (siteMgr_t *)(pStadHandles->hSiteMgr);

    /* Init handles */
    pSiteMgr->hConn                 = pStadHandles->hConn;
    pSiteMgr->hSmeSm                = pStadHandles->hSme;
    pSiteMgr->hTWD                  = pStadHandles->hTWD;
    pSiteMgr->hCtrlData             = pStadHandles->hCtrlData;
    pSiteMgr->hRxData               = pStadHandles->hRxData;
    pSiteMgr->hTxCtrl               = pStadHandles->hTxCtrl;
    pSiteMgr->hRsn                  = pStadHandles->hRsn;
    pSiteMgr->hAuth                 = pStadHandles->hAuth;
    pSiteMgr->hAssoc                = pStadHandles->hAssoc;
    pSiteMgr->hRegulatoryDomain     = pStadHandles->hRegulatoryDomain;
    pSiteMgr->hMeasurementMgr       = pStadHandles->hMeasurementMgr;
    pSiteMgr->hReport               = pStadHandles->hReport;
    pSiteMgr->hOs                   = pStadHandles->hOs;
    pSiteMgr->hMlmeSm               = pStadHandles->hMlmeSm;
    pSiteMgr->hAssoc                = pStadHandles->hAssoc;
    pSiteMgr->hReport               = pStadHandles->hReport;
    pSiteMgr->hXCCMngr              = pStadHandles->hXCCMngr;
    pSiteMgr->hApConn               = pStadHandles->hAPConnection;
    pSiteMgr->hCurrBss              = pStadHandles->hCurrBss;
    pSiteMgr->hQosMngr              = pStadHandles->hQosMngr;
    pSiteMgr->hPowerMgr             = pStadHandles->hPowerMgr;
    pSiteMgr->hScr                  = pStadHandles->hSCR;
    pSiteMgr->hEvHandler            = pStadHandles->hEvHandler;
    pSiteMgr->hStaCap               = pStadHandles->hStaCap;
}


TI_STATUS siteMgr_SetDefaults (TI_HANDLE                hSiteMgr,
                               siteMgrInitParams_t     *pSiteMgrInitParams)
{    
    siteMgr_t       *pSiteMgr = (siteMgr_t *)hSiteMgr;
    TI_UINT32       timestamp;
    ESlotTime       slotTime;
    TMacAddr        saBssid;
    TI_STATUS       status;
    RssiSnrTriggerCfg_t tTriggerCfg;

    pSiteMgr->siteMgrTxPowerCheckTime   = 0;
    pSiteMgr->siteMgrWSCCurrMode        = TIWLN_SIMPLE_CONFIG_OFF;
    pSiteMgr->includeWSCinProbeReq      = pSiteMgrInitParams->includeWSCinProbeReq;

    /* Init desired parameters */
    os_memoryCopy(pSiteMgr->hOs, pSiteMgr->pDesiredParams, pSiteMgrInitParams, sizeof(siteMgrInitParams_t));

    /* Init Beacon Filter Desired State */
    pSiteMgr->beaconFilterParams.desiredState = pSiteMgrInitParams->beaconFilterParams.desiredState;
    /* Init Beacon Filter numOfStored parameter */
    pSiteMgr->beaconFilterParams.numOfStored = pSiteMgrInitParams->beaconFilterParams.numOfStored;

    /* Init management params */
    pSiteMgr->pSitesMgmtParams->dot11A_sitesTables.maxNumOfSites = MAX_SITES_A_BAND;
    siteMgr_resetSiteTable(pSiteMgr,(siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables);
    pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables.maxNumOfSites = MAX_SITES_BG_BAND;
    siteMgr_resetSiteTable(pSiteMgr,&pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables);

    /* calculate random BSSID for usage in IBSS */
    timestamp = os_timeStampMs(pSiteMgr->hOs);
    os_memoryCopy(pSiteMgr->hOs, (void *)&(pSiteMgr->ibssBssid[0]), &timestamp, sizeof(TI_UINT32));

    timestamp = os_timeStampMs(pSiteMgr->hOs);
    os_memoryCopy(pSiteMgr->hOs, (void *)&(pSiteMgr->ibssBssid[2]), &timestamp, sizeof(TI_UINT32));

    /* Get the Source MAC address in order to use it for AD-Hoc BSSID, solving Conexant ST issue for WiFi test */
    status = ctrlData_getParamBssid(pSiteMgr->hCtrlData, CTRL_DATA_MAC_ADDRESS, saBssid);
    if (status != TI_OK)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_CONSOLE ,"\n ERROR !!! : siteMgr_config - Error in getting MAC address\n" );
        WLAN_OS_REPORT(("\n ERROR !!! : siteMgr_config - Error in getting MAC address\n" ));
        return TI_NOK;
    }
    pSiteMgr->ibssBssid[0] = 0x02;
    pSiteMgr->ibssBssid[1] = saBssid[1];
    pSiteMgr->ibssBssid[2] = saBssid[2];

    pSiteMgr->pDesiredParams->siteMgrSupportedBand = RADIO_BAND_DUAL;

    pSiteMgr->beaconSentCount = 0;
    pSiteMgr->pDesiredParams->siteMgrDesiredAtimWindow = 0;
	pSiteMgr->txSessionCount = MIN_TX_SESSION_COUNT;

    if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_DUAL_MODE)
    {
       if(pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_DUAL)
       {
           pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;
           pSiteMgr->radioBand = RADIO_BAND_2_4_GHZ;
           slotTime = pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime;
           pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
       }
       else if(pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_2_4_GHZ)
       {
           pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode = DOT11_G_MODE;
           pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;
           pSiteMgr->radioBand = RADIO_BAND_2_4_GHZ;
           slotTime = pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime;
           pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
       }
       else
       {
           pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode = DOT11_A_MODE;
           pSiteMgr->siteMgrOperationalMode = DOT11_A_MODE;
           pSiteMgr->radioBand = RADIO_BAND_5_0_GHZ;
           slotTime = PHY_SLOT_TIME_SHORT;
           pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
       }
    }
    else if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_G_MODE)
    {
        slotTime = pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime;
        pSiteMgr->radioBand = RADIO_BAND_2_4_GHZ;
        if((pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_DUAL) ||
           (pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_2_4_GHZ))
        {
            pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
            pSiteMgr->siteMgrOperationalMode = pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode;

        }
        else
        {   TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_CONSOLE ,"\nERROR !!!.....The radio doesn't support the desired dot11 mode !!! \n");
            WLAN_OS_REPORT(("\nERROR !!!.....The radio doesn't support the desired dot11 mode !!! \n"));
            return TI_NOK;
        }
    }
    else if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_B_MODE)
    {
        slotTime = PHY_SLOT_TIME_LONG;
        pSiteMgr->radioBand = RADIO_BAND_2_4_GHZ;
        if((pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_DUAL) ||
           (pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_2_4_GHZ))
        {
            pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
            pSiteMgr->siteMgrOperationalMode = pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode;
        }
        else
        {
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_CONSOLE ,"\nERROR !!!.....The radio doesn't support the desired dot11 mode !!! \n");
            WLAN_OS_REPORT(("\nERROR !!!.....The radio doesn't support the desired dot11 mode !!! \n"));
            return TI_NOK;
        }
    }
    else
    {
        slotTime = PHY_SLOT_TIME_SHORT;
        pSiteMgr->radioBand = RADIO_BAND_5_0_GHZ;
        if((pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_DUAL) ||
           (pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_5_0_GHZ))
        {
            pSiteMgr->siteMgrOperationalMode = pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode;
            pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
        }
        else
        {
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_CONSOLE ,"\nERROR !!!.....The radio doesn't support the desired dot11 mode !!! \n");
            WLAN_OS_REPORT(("\nERROR !!!.....The radio doesn't support the desired dot11 mode !!! \n"));
            return TI_NOK;
        }
    }

    /* Configure hal with common core-hal parameters */
    TWD_SetRadioBand(pSiteMgr->hTWD, pSiteMgr->radioBand);
    TWD_CfgSlotTime (pSiteMgr->hTWD, slotTime);
    siteMgr_ConfigRate(hSiteMgr);

    TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INIT, " SiteMgr - numOfElements = %d IETableSize = %d\n" , pSiteMgrInitParams->beaconFilterParams.numOfElements, pSiteMgrInitParams->beaconFilterParams.IETableSize);
    /* Send the table regardless to the state */
    TWD_CfgBeaconFilterTable (pSiteMgr->hTWD,
                              pSiteMgrInitParams->beaconFilterParams.numOfElements, 
                              pSiteMgrInitParams->beaconFilterParams.IETable, 
                              pSiteMgrInitParams->beaconFilterParams.IETableSize);

    /*  At start-up Set the Beacon Filter state as the User required */
    TWD_CfgBeaconFilterOpt (pSiteMgr->hTWD, pSiteMgrInitParams->beaconFilterParams.desiredState, pSiteMgr->beaconFilterParams.numOfStored);
    
    pSiteMgr->pSitesMgmtParams->pPrevPrimarySite = NULL;

    /* Clears the ProbeReqWSC IE */
    os_memoryZero(pSiteMgr->hOs,&pSiteMgr->siteMgrWSCProbeReqParams,sizeof(DOT11_WSC_PROBE_REQ_MAX_LENGTH));

    /* Register the RSSI Trigger events at the currBss RSSI/SNR static table*/
    currBSS_RegisterTriggerEvent(pSiteMgr->hCurrBss, TWD_OWN_EVENT_RSSI_SNR_TRIGGER_2, (TI_UINT8)0,(void*)siteMgr_TxPowerHighThreshold,hSiteMgr);
    currBSS_RegisterTriggerEvent(pSiteMgr->hCurrBss, TWD_OWN_EVENT_RSSI_SNR_TRIGGER_3, (TI_UINT8)0,(void*)siteMgr_TxPowerLowThreshold, hSiteMgr);

    tTriggerCfg.index     = TRIGGER_EVENT_HIGH_TX_PW;
    tTriggerCfg.threshold = (0 - pSiteMgr->pDesiredParams->TxPowerRssiThresh);
    tTriggerCfg.pacing    = TRIGGER_HIGH_TX_PW_PACING;
    tTriggerCfg.metric    = METRIC_EVENT_RSSI_DATA;
    tTriggerCfg.type      = RX_QUALITY_EVENT_EDGE;
    tTriggerCfg.direction = RSSI_EVENT_DIR_HIGH;
    tTriggerCfg.hystersis = TRIGGER_HIGH_TX_PW_HYSTERESIS;
    tTriggerCfg.enable     = TI_TRUE;
    TWD_CfgRssiSnrTrigger (pSiteMgr->hTWD, &tTriggerCfg);

    tTriggerCfg.index     = TRIGGER_EVENT_LOW_TX_PW;
    tTriggerCfg.threshold = (0 - pSiteMgr->pDesiredParams->TxPowerRssiRestoreThresh);
    tTriggerCfg.pacing    = TRIGGER_LOW_TX_PW_PACING;
    tTriggerCfg.metric    = METRIC_EVENT_RSSI_DATA;
    tTriggerCfg.type      = RX_QUALITY_EVENT_EDGE;
    tTriggerCfg.direction = RSSI_EVENT_DIR_LOW;
    tTriggerCfg.hystersis = TRIGGER_LOW_TX_PW_HYSTERESIS;
    tTriggerCfg.enable    = TI_TRUE;
    TWD_CfgRssiSnrTrigger (pSiteMgr->hTWD, &tTriggerCfg);

    TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INIT, ".....Site manager configured successfully\n");

    return TI_OK;
}


/************************************************************************
 *                        siteMgr_unLoad                                    *
 ************************************************************************
DESCRIPTION: site manager module unload function, called by the config mgr in the unlod phase
                performs the following:
                -   Free all memory aloocated by the module

INPUT:      hSiteMgr    -   site mgr handle.


OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS siteMgr_unLoad(TI_HANDLE hSiteMgr)
{
    TI_UINT32          initVec;
    siteMgr_t       *pSiteMgr = (siteMgr_t *)hSiteMgr;

    if (!pSiteMgr)
        return TI_OK;

    initVec = 0xFFFF;
    release_module(pSiteMgr, initVec);

    return TI_OK;
}

/***********************************************************************
 *                        siteMgr_setParam
 ***********************************************************************
DESCRIPTION: site mgr set param function, called by the following:
                -   config mgr in order to set a parameter from the OS abstraction layer.
                In this fuction, the site manager OS abstraction layer configures the site manager to the desired params.
                Sometimes it requires a re scan, depending in the parameter type

INPUT:      hSiteMgr    -   Connection handle.
            pParam  -   Pointer to the parameter

OUTPUT:

RETURN:     TI_OK on success, TI_NOK on failure

************************************************************************/

TI_STATUS siteMgr_setParam(TI_HANDLE        hSiteMgr,
                        paramInfo_t     *pParam)
{
    siteMgr_t *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
    OS_802_11_CONFIGURATION *pConfig;
    TI_UINT32      channel;
    ESlotTime  slotTime;
    paramInfo_t	param;
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)pSiteMgr->hPowerMgr;
    static PowerMgr_PowerMode_e desiredPowerModeProfile;

    switch(pParam->paramType)
    {
    case SITE_MGR_CONFIGURATION_PARAM:
        pConfig = pParam->content.pSiteMgrConfiguration;

/*      for(channel = 0; channel < SITE_MGR_CHANNEL_MAX+1; channel++)
        {
            if(pConfig->channel == pSiteMgr->pDesiredParams->siteMgrFreq2ChannelTable[channel])
                break;
        }*/

        channel = Freq2Chan(pConfig->Union.channel);

        if(channel == 0 || channel > SITE_MGR_CHANNEL_MAX)
            return PARAM_VALUE_NOT_VALID;
        else
            pConfig->Union.channel = channel;

        if((pSiteMgr->pDesiredParams->siteMgrDesiredChannel != pConfig->Union.channel) ||
           (pSiteMgr->pDesiredParams->siteMgrDesiredAtimWindow != pConfig->ATIMWindow))
        {
            pSiteMgr->pDesiredParams->siteMgrDesiredChannel = (TI_UINT8)pConfig->Union.channel;
            pSiteMgr->pDesiredParams->siteMgrDesiredBeaconInterval = (TI_UINT16)pConfig->BeaconPeriod;
            pSiteMgr->pDesiredParams->siteMgrDesiredAtimWindow = pConfig->ATIMWindow;
        }

        return TI_OK;

    case SITE_MGR_DESIRED_CHANNEL_PARAM:
        if (pParam->content.siteMgrDesiredChannel > SITE_MGR_CHANNEL_MAX)
            return PARAM_VALUE_NOT_VALID;

        if (pSiteMgr->pDesiredParams->siteMgrDesiredChannel != pParam->content.siteMgrDesiredChannel)
            pSiteMgr->pDesiredParams->siteMgrDesiredChannel = (TI_UINT8)pParam->content.siteMgrDesiredChannel;
        return TI_OK;

    case SITE_MGR_DESIRED_BSSID_PARAM:
            MAC_COPY (pSiteMgr->pDesiredParams->siteMgrDesiredBSSID, pParam->content.siteMgrDesiredBSSID);
           return TI_OK;

    case SITE_MGR_DESIRED_SSID_PARAM: 

        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "\nSet new SSID= (len=%d)  \n", pParam->content.siteMgrDesiredSSID.len);

        if (pParam->content.siteMgrDesiredSSID.len > MAX_SSID_LEN)
            return PARAM_VALUE_NOT_VALID;

        os_memoryCopy(pSiteMgr->hOs, &pSiteMgr->pDesiredParams->siteMgrDesiredSSID, &pParam->content.siteMgrDesiredSSID, sizeof(TSsid));
        /* only add null at the end of the string if the string length is less than 32 bytes and so we have one char left
        */
        if ( MAX_SSID_LEN > pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len )
        {
            pSiteMgr->pDesiredParams->siteMgrDesiredSSID.str[pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len] = '\0';
        }

        /* increase the random IBSS BSSID calculated during init */
		pSiteMgr->ibssBssid[MAC_ADDR_LEN - 1] ++;

        if (OS_802_11_SSID_JUNK (pSiteMgr->pDesiredParams->siteMgrDesiredSSID.str, pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len))
        {
            rsn_removedDefKeys(pSiteMgr->hRsn);
        }

        /* due to the fact we call to SME_DESIRED_SSID_ACT_PARAM also we not need to call sme_Restart */
        return TI_OK;

    case SITE_MGR_DESIRED_BSS_TYPE_PARAM:

         TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "\nSet BssType = %d\n", pParam->content.siteMgrDesiredBSSType);
        if (pParam->content.siteMgrDesiredBSSType > BSS_ANY)
            return PARAM_VALUE_NOT_VALID;

        if (pSiteMgr->pDesiredParams->siteMgrDesiredBSSType != pParam->content.siteMgrDesiredBSSType)
        {
            pSiteMgr->pDesiredParams->siteMgrDesiredBSSType = pParam->content.siteMgrDesiredBSSType;

			/* If the new BSS type is NOT Ad_Hoc, We make sure that the rate masks are set to G */
			 if(pSiteMgr->pDesiredParams->siteMgrDesiredBSSType != BSS_INDEPENDENT)

            {
				 pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;
                 siteMgr_ConfigRate(pSiteMgr);
            }

            /* If the new BSS type is Ad_Hoc, increase the random BSSID calculated during init */
            if(pSiteMgr->pDesiredParams->siteMgrDesiredBSSType == BSS_INDEPENDENT)
            {
                pSiteMgr->ibssBssid[MAC_ADDR_LEN - 1] ++;
            }

            /* go to B_ONLY Mode only if WiFI bit is Set*/
            if (pSiteMgr->pDesiredParams->siteMgrWiFiAdhoc == TI_TRUE)
            {   /* Configuration For AdHoc when using external configuration */
                if(pSiteMgr->pDesiredParams->siteMgrExternalConfiguration == TI_FALSE)
                {
                    siteMgr_externalConfigurationParametersSet(hSiteMgr);
                }
            }
        }

        return TI_OK;

    case SITE_MGR_SIMPLE_CONFIG_MODE: /* Setting the WiFiSimpleConfig mode */
		
        /* Modify the current mode and IE size */
		pSiteMgr->siteMgrWSCCurrMode = pParam->content.siteMgrWSCMode.WSCMode;
		pSiteMgr->uWscIeSize = pParam->content.siteMgrWSCMode.uWscIeSize;

        TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Setting SimpleConfig Mode to %d, IE Size = %d\n", pSiteMgr->siteMgrWSCCurrMode, pSiteMgr->uWscIeSize);

		/* In case the WSC is on ,the ProbeReq WSC IE need to be updated */
        if(pSiteMgr->siteMgrWSCCurrMode != TIWLN_SIMPLE_CONFIG_OFF)
		{
			os_memoryCopy(pSiteMgr->hOs, &pSiteMgr->siteMgrWSCProbeReqParams, &pParam->content.siteMgrWSCMode.probeReqWSCIE, pSiteMgr->uWscIeSize);

			param.paramType = RSN_WPA_PROMOTE_OPTIONS;	
           	param.content.rsnWPAPromoteFlags = ADMCTRL_WPA_OPTION_ENABLE_PROMOTE_AUTH_MODE;
           	rsn_setParam(pSiteMgr->hRsn, &param);

            /*
		     * Set the system to Active power save
             */
            desiredPowerModeProfile = pPowerMgr->desiredPowerModeProfile;
            param.paramType = POWER_MGR_POWER_MODE;
            param.content.powerMngPowerMode.PowerMode = POWER_MODE_ACTIVE;
            param.content.powerMngPowerMode.PowerMngPriority = POWER_MANAGER_USER_PRIORITY;
            powerMgr_setParam(pSiteMgr->hPowerMgr,&param);
        }
		else
		{
			param.paramType = RSN_WPA_PROMOTE_OPTIONS;	
           	param.content.rsnWPAPromoteFlags = ADMCTRL_WPA_OPTION_NONE;
           	rsn_setParam(pSiteMgr->hRsn, &param);

            /* 
             * Set the system to last power mode
             */
            param.paramType = POWER_MGR_POWER_MODE;
            param.content.powerMngPowerMode.PowerMode = desiredPowerModeProfile;
            param.content.powerMngPowerMode.PowerMngPriority = POWER_MANAGER_USER_PRIORITY;
            powerMgr_setParam(pSiteMgr->hPowerMgr,&param);
		}

        /* Update the FW prob request templates to reflect the new WSC state */
        setDefaultProbeReqTemplate (hSiteMgr);

        /* update the SME on the WPS mode */
        param.paramType = SME_WSC_PB_MODE_PARAM;
        sme_SetParam (pSiteMgr->hSmeSm, &param);

        return TI_OK;

    case SITE_MGR_DESIRED_MODULATION_TYPE_PARAM:
        if ((pParam->content.siteMgrDesiredModulationType < DRV_MODULATION_CCK) ||
            (pParam->content.siteMgrDesiredModulationType > DRV_MODULATION_OFDM))
            return PARAM_VALUE_NOT_VALID;

        if (pSiteMgr->pDesiredParams->siteMgrDesiredModulationType != pParam->content.siteMgrDesiredModulationType)
        {
            pSiteMgr->pDesiredParams->siteMgrDesiredModulationType = pParam->content.siteMgrDesiredModulationType;
            /* means that we are moving from non-pbcc network to pbcc */
            if (pParam->content.siteMgrDesiredModulationType == DRV_MODULATION_PBCC)
                sme_Restart (pSiteMgr->hSmeSm);
            return TI_OK;
        }
        return TI_OK;

    case SITE_MGR_BEACON_RECV:
        if (!pPrimarySite)
        {
            return NO_SITE_SELECTED_YET;
        }
        pPrimarySite->beaconRecv = pParam->content.siteMgrBeaconRecv;
        return TI_OK;


    case SITE_MGR_DESIRED_BEACON_INTERVAL_PARAM:
        if (pParam->content.siteMgrDesiredBeaconInterval < SITE_MGR_BEACON_INTERVAL_MIN)
            return PARAM_VALUE_NOT_VALID;

        if (pSiteMgr->pDesiredParams->siteMgrDesiredBeaconInterval != pParam->content.siteMgrDesiredBeaconInterval)
            pSiteMgr->pDesiredParams->siteMgrDesiredBeaconInterval = pParam->content.siteMgrDesiredBeaconInterval;
        return TI_OK;

    case SITE_MGR_DESIRED_PREAMBLE_TYPE_PARAM:
        if ((pParam->content.siteMgrDesiredPreambleType != PREAMBLE_LONG) &&
            (pParam->content.siteMgrDesiredPreambleType != PREAMBLE_SHORT))
            return PARAM_VALUE_NOT_VALID;

        if (pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType != pParam->content.siteMgrDesiredPreambleType)
        {
            pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType = pParam->content.siteMgrDesiredPreambleType;
        }
        return TI_OK;

    case SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM:
        return setSupportedRateSet(pSiteMgr, &(pParam->content.siteMgrDesiredSupportedRateSet));

    case SITE_MGR_DESIRED_DOT11_MODE_PARAM:
        if(pParam->content.siteMgrDot11Mode > DOT11_MAX_MODE)
            return PARAM_VALUE_NOT_VALID;

        if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode != pParam->content.siteMgrDot11Mode)
        {
            pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode = pParam->content.siteMgrDot11Mode;

            /* since the dot11ABAmode changed, the STA operational mode should be changed */
            if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_DUAL_MODE)
            {
                if(pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_DUAL)
                {
                    pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;
                }
                else if(pSiteMgr->pDesiredParams->siteMgrSupportedBand == RADIO_BAND_2_4_GHZ)
                {
                    pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode = DOT11_G_MODE;
                    pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;
                }
                else
                {
                    pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode = DOT11_G_MODE;
                    pSiteMgr->siteMgrOperationalMode = DOT11_A_MODE;
                }

            }
            else
                pSiteMgr->siteMgrOperationalMode = pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode;

            /* configure HAL with new parameters update rates and select site table */
            pSiteMgr->prevRadioBand = pSiteMgr->radioBand;
            if(pSiteMgr->siteMgrOperationalMode == DOT11_A_MODE)
            {
                pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
                pSiteMgr->radioBand = RADIO_BAND_5_0_GHZ;
                slotTime = PHY_SLOT_TIME_SHORT;
            }
            else if(pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE)
            {
                pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
                pSiteMgr->radioBand = RADIO_BAND_2_4_GHZ;
                slotTime = pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime;
            }
            else
            {
                pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
                pSiteMgr->radioBand = RADIO_BAND_2_4_GHZ;
                slotTime = PHY_SLOT_TIME_LONG;
            }

            if(pSiteMgr->prevRadioBand != pSiteMgr->radioBand)
                siteMgr_bandParamsConfig(pSiteMgr, TI_TRUE);

            /* Configure TWD */
            TWD_SetRadioBand(pSiteMgr->hTWD, pSiteMgr->radioBand);
            TWD_CfgSlotTime (pSiteMgr->hTWD, slotTime);

            /* If the BSS type is Ad_Hoc, increase the random BSSID calculated during init */
            if(pSiteMgr->pDesiredParams->siteMgrDesiredBSSType == BSS_INDEPENDENT)
            {
                pSiteMgr->ibssBssid[MAC_ADDR_LEN - 1] ++;
            }

            /*siteMgr_resetAllSiteTables(pSiteMgr); */
            sme_Restart (pSiteMgr->hSmeSm);
        }
        return TI_OK;

    case SITE_MGR_OPERATIONAL_MODE_PARAM:

        if(pParam->content.siteMgrDot11OperationalMode < DOT11_B_MODE ||
            pParam->content.siteMgrDot11OperationalMode > DOT11_G_MODE )
            return PARAM_VALUE_NOT_VALID;

        pSiteMgr->siteMgrOperationalMode = pParam->content.siteMgrDot11OperationalMode;
        break;


    case SITE_MGR_RADIO_BAND_PARAM:
        if((TI_INT8)pParam->content.siteMgrRadioBand < RADIO_BAND_2_4_GHZ ||
           (TI_INT8)pParam->content.siteMgrRadioBand > RADIO_BAND_DUAL )
            return PARAM_VALUE_NOT_VALID;

        pSiteMgr->prevRadioBand = pSiteMgr->radioBand;
        pSiteMgr->radioBand = pParam->content.siteMgrRadioBand;
        if(pSiteMgr->prevRadioBand != pSiteMgr->radioBand)
            siteMgr_bandParamsConfig(pSiteMgr, TI_FALSE);

        break;

    case SITE_MGR_DESIRED_SLOT_TIME_PARAM:
        if(pParam->content.siteMgrSlotTime != PHY_SLOT_TIME_LONG &&
           pParam->content.siteMgrSlotTime != PHY_SLOT_TIME_SHORT)
            return PARAM_VALUE_NOT_VALID;

        if (pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime != pParam->content.siteMgrSlotTime)
        {
            if (pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE)
            {
                pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime = pParam->content.siteMgrSlotTime;
                if (!pPrimarySite)
                    TWD_CfgSlotTime (pSiteMgr->hTWD, pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime);
                else if (pPrimarySite->bssType != BSS_INFRASTRUCTURE)
                    TWD_CfgSlotTime (pSiteMgr->hTWD, pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime);
            }

        }
        return TI_OK;

    case SITE_MGR_BEACON_FILTER_DESIRED_STATE_PARAM:
        {
            /* Check if the Desired  mode has changed - If not no need to send the MIB to the FW */
            if (pSiteMgr->beaconFilterParams.desiredState == pParam->content.siteMgrDesiredBeaconFilterState)
            {
                TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Beacon Filter already \n");
                break;
            }

            /* Set the New Desired User request of Beacon Filter */
            pSiteMgr->beaconFilterParams.desiredState = pParam->content.siteMgrDesiredBeaconFilterState;
                
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "New Beacon Filter State is: \n");

            /* Send the User required Beacon Filter Configuration to the FW */
            TWD_CfgBeaconFilterOpt (pSiteMgr->hTWD, pSiteMgr->beaconFilterParams.desiredState, pSiteMgr->beaconFilterParams.numOfStored);
        }

        break;

    case SITE_MGR_LAST_RX_RATE_PARAM:
        if (pPrimarySite != NULL)
        {
            pPrimarySite->rxRate = pParam->content.ctrlDataCurrentBasicRate;
        }
        break;

    case SITE_MGR_CURRENT_CHANNEL_PARAM:
        if (!pPrimarySite)
        {
            return NO_SITE_SELECTED_YET;
        }
        pPrimarySite->channel = pParam->content.siteMgrCurrentChannel;
        break;

    case SITE_MGR_CURRENT_SIGNAL_PARAM:
        if (!pPrimarySite)
        {
            return NO_SITE_SELECTED_YET;
        }

        pPrimarySite->rssi = pParam->content.siteMgrCurrentSignal.rssi;
        break;

    case SITE_MGRT_SET_RATE_MANAGMENT:
        TWD_SetRateMngDebug(pSiteMgr->hTWD,&pParam ->content.RateMng);
        break;
    case SITE_MGR_SET_WLAN_IP_PARAM:
        {
            ArpRspTemplate_t      ArpRspTemplate;
            TSetTemplate          templateStruct;
            TIpAddr staIp;
            EArpFilterType        filterType = ArpFilterEnabledAutoMode;

            staIp[0] = pParam->content.StationIP[0];
            staIp[1] = pParam->content.StationIP[1];
            staIp[2] = pParam->content.StationIP[2];
            staIp[3] = pParam->content.StationIP[3];

            templateStruct.type = ARP_RSP_TEMPLATE;
            templateStruct.ptr  = (TI_UINT8 *)&ArpRspTemplate;

            buildArpRspTemplate(pSiteMgr, &templateStruct, staIp);
            TWD_CmdTemplate (pSiteMgr->hTWD, &templateStruct, NULL, NULL);

            /* configuring the IP to 0.0.0.0 by app means disable filtering */
            if ((staIp[0] | staIp[1] | staIp[2] | staIp[3]) == 0) 
            {
                filterType = ArpFilterDisabled;
            }

            TWD_CfgArpIpAddrTable(pSiteMgr->hTWD, staIp , filterType, IP_VER_4);
        }
        
        break;

    default:
        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Set param, Params is not supported, %d\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}

TI_STATUS siteMgr_getParamWSC(TI_HANDLE hSiteMgr, TIWLN_SIMPLE_CONFIG_MODE *wscParam)
{ /* SITE_MGR_SIMPLE_CONFIG_MODE: - Retrieving the WiFiSimpleConfig mode */
    siteMgr_t       *pSiteMgr = (siteMgr_t *)hSiteMgr;

	if (pSiteMgr == NULL)
	{
		return TI_NOK;
	}

    *wscParam = pSiteMgr->siteMgrWSCCurrMode;
    return TI_OK;
}

/***********************************************************************
 *                        siteMgr_getParam
 ***********************************************************************
DESCRIPTION: Site mgr get param function, called by the following:
            -   config mgr in order to get a parameter from the OS abstraction layer.
            -   From inside the dirver

INPUT:      hSiteMgr    -   site mgr handle.
            pParam  -   Pointer to the parameter

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS siteMgr_getParam(TI_HANDLE        hSiteMgr,
                            paramInfo_t     *pParam)
{
    siteMgr_t       *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t     *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
    TI_STATUS       status = TI_OK;
    TI_UINT8           siteEntryIndex;
    TTwdParamInfo   tTwdParam;

	if(pSiteMgr == NULL)
	{
		return TI_NOK;
	}

    switch(pParam->paramType)
    {

    case SITE_MGR_CONFIGURATION_PARAM:
        pParam->content.pSiteMgrConfiguration->Length = sizeof(OS_802_11_CONFIGURATION);
        pParam->content.pSiteMgrConfiguration->ATIMWindow = pSiteMgr->pDesiredParams->siteMgrDesiredAtimWindow;
        pParam->content.pSiteMgrConfiguration->BeaconPeriod = pSiteMgr->pDesiredParams->siteMgrDesiredBeaconInterval;
        pParam->content.pSiteMgrConfiguration->Union.channel =
            Chan2Freq(pSiteMgr->pDesiredParams->siteMgrDesiredChannel);
            /*pSiteMgr->pDesiredParams->siteMgrFreq2ChannelTable[pSiteMgr->pDesiredParams->siteMgrDesiredChannel];*/
        break;

    case SITE_MGR_DESIRED_CHANNEL_PARAM:
        pParam->content.siteMgrDesiredChannel = pSiteMgr->pDesiredParams->siteMgrDesiredChannel;
        break;

	case SITE_MGR_SIMPLE_CONFIG_MODE: /* Retrieving the WiFiSimpleConfig mode */
		pParam->content.siteMgrWSCMode.WSCMode = pSiteMgr->siteMgrWSCCurrMode;
		
        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Retrieving the SimpleConfig Mode (%d) \n", pSiteMgr->siteMgrWSCCurrMode);
		break;

    case SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM:
        getSupportedRateSet(pSiteMgr, &(pParam->content.siteMgrDesiredSupportedRateSet));
        break;

    case SITE_MGR_DESIRED_MODULATION_TYPE_PARAM:
        pParam->content.siteMgrDesiredModulationType = pSiteMgr->pDesiredParams->siteMgrDesiredModulationType;
        break;

    case SITE_MGR_DESIRED_BEACON_INTERVAL_PARAM:
        pParam->content.siteMgrDesiredBeaconInterval = pSiteMgr->pDesiredParams->siteMgrDesiredBeaconInterval;
        break;

    case SITE_MGR_DESIRED_PREAMBLE_TYPE_PARAM:
        pParam->content.siteMgrDesiredPreambleType = pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType;
        break;

    case SITE_MGR_CURRENT_SIGNAL_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrCurrentSignal.rssi = 0;
            pParam->content.siteMgrCurrentSignal.snr = 0;
            return NO_SITE_SELECTED_YET;
        }

        pParam->content.siteMgrCurrentSignal.rssi = pPrimarySite->rssi;
        pParam->content.siteMgrCurrentSignal.snr = pPrimarySite->snr;
        break;

    case SITE_MGR_POWER_CONSTRAINT_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.powerConstraint = 0;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.powerConstraint = pPrimarySite->powerConstraint;
        break;


    case SITE_MGR_DTIM_PERIOD_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrDtimPeriod = 0;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.siteMgrDtimPeriod = pPrimarySite->dtimPeriod;
        break;

    case SITE_MGR_BEACON_RECV:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrBeaconRecv = TI_FALSE;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.siteMgrBeaconRecv = pPrimarySite->beaconRecv;
        break;


    case SITE_MGR_BEACON_INTERVAL_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.beaconInterval = 0;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.beaconInterval = pPrimarySite->beaconInterval;
        break;

    case SITE_MGR_AP_TX_POWER_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.APTxPower = 0;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.APTxPower = pPrimarySite->APTxPower;
        break;

    case SITE_MGR_SITE_CAPABILITY_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrSiteCapability = 0;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.siteMgrSiteCapability = pPrimarySite->capabilities;
        break;

    case SITE_MGR_CURRENT_CHANNEL_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrCurrentChannel = 0;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.siteMgrCurrentChannel = pPrimarySite->channel;
        break;

    case SITE_MGR_CURRENT_SSID_PARAM:
        if (!pPrimarySite)
        {
            os_memoryZero(pSiteMgr->hOs, (void *)pParam->content.siteMgrCurrentSSID.str, MAX_SSID_LEN);
            pParam->content.siteMgrCurrentSSID.len = 0;
            return NO_SITE_SELECTED_YET;
        }
        if(pPrimarySite->ssid.len == 0)
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "siteMgr_getParam: ssid length is zero, while primarySite is selected \n");
        os_memoryCopy(pSiteMgr->hOs, &pParam->content.siteMgrCurrentSSID, &pPrimarySite->ssid, sizeof(TSsid));
        break;


    case SITE_MGR_CURRENT_BSS_TYPE_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrCurrentBSSType = pSiteMgr->pDesiredParams->siteMgrDesiredBSSType;
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Trying to get current BSS Type while no site is selected\n");
            
        }
        else{
            pParam->content.siteMgrCurrentBSSType = pPrimarySite->bssType;
        }

        break;


    case SITE_MGR_CURRENT_RATE_PAIR_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrCurrentRateMask.basicRateMask = 0;
            pParam->content.siteMgrCurrentRateMask.supportedRateMask = 0;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.siteMgrCurrentRateMask.basicRateMask = pSiteMgr->pDesiredParams->siteMgrMatchedBasicRateMask;
        pParam->content.siteMgrCurrentRateMask.supportedRateMask = pSiteMgr->pDesiredParams->siteMgrMatchedSuppRateMask;
        TRACE4(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "SITE_MGR: bitmapBasicPrimary= 0x%X,bitMapBasicDesired = 0x%X,bitMapSuppPrimary = 0x%X, bitMapSuppDesired = 0x%X\n", pPrimarySite->rateMask.basicRateMask,pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask, pPrimarySite->rateMask.supportedRateMask,pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask);
        break;

    case SITE_MGR_CURRENT_MODULATION_TYPE_PARAM:
        if (!pPrimarySite)
        {
            pParam->content.siteMgrCurrentModulationType = DRV_MODULATION_NONE;
            return NO_SITE_SELECTED_YET;
        }
        pParam->content.siteMgrCurrentModulationType = pSiteMgr->chosenModulation;
        break;

    case SITE_MGR_DESIRED_SLOT_TIME_PARAM:
        pParam->content.siteMgrSlotTime = pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime;
        break;

    case SITE_MGR_CURRENT_SLOT_TIME_PARAM:

        if(pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE)
        {
            if(!pPrimarySite)
                pParam->content.siteMgrSlotTime = pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime;
            else
                pParam->content.siteMgrSlotTime = pPrimarySite->currentSlotTime;
        }
        else if(pSiteMgr->siteMgrOperationalMode == DOT11_A_MODE)
            pParam->content.siteMgrSlotTime = PHY_SLOT_TIME_SHORT;
        else
            pParam->content.siteMgrSlotTime = PHY_SLOT_TIME_LONG;

        break;

    case SITE_MGR_LAST_BEACON_BUF_PARAM:
        if (pPrimarySite != NULL)
        {
            if (pPrimarySite->probeRecv)
            {
                pParam->content.siteMgrLastBeacon.isBeacon = TI_FALSE;
                pParam->content.siteMgrLastBeacon.bufLength = pPrimarySite->probeRespLength;
                pParam->content.siteMgrLastBeacon.buffer = pPrimarySite->probeRespBuffer;
            }
            else
            {
                pParam->content.siteMgrLastBeacon.isBeacon = TI_TRUE;
                pParam->content.siteMgrLastBeacon.bufLength = pPrimarySite->beaconLength;
                pParam->content.siteMgrLastBeacon.buffer = pPrimarySite->beaconBuffer;
            }
        }
        break;

    case SITE_MGR_BEACON_FILTER_DESIRED_STATE_PARAM:
        {
            if ( NULL != pSiteMgr )
            {
                pParam->content.siteMgrDesiredBeaconFilterState = pSiteMgr->beaconFilterParams.desiredState;
            }
            else
            {
                TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "pSite = NULL ! No info available");
            }
        }
        break;

    case SITE_MGR_GET_SELECTED_BSSID_INFO:
        getPrimarySiteDesc(pSiteMgr, pParam->content.pSiteMgrPrimarySiteDesc, TI_FALSE);
        break;

	case SITE_MGR_GET_SELECTED_BSSID_INFO_EX:
       getPrimarySiteDesc(pSiteMgr, (OS_802_11_BSSID *)pParam->content.pSiteMgrSelectedSiteInfo,TI_TRUE);
       break;

    case SITE_MGR_PRIMARY_SITE_PARAM:
       status = getPrimaryBssid(pSiteMgr, (OS_802_11_BSSID_EX *)pParam->content.pSiteMgrSelectedSiteInfo, &pParam->paramLength);
       break;


    case SITE_MGR_TI_WLAN_COUNTERS_PARAM:
        pParam->paramType = RX_DATA_COUNTERS_PARAM;
        rxData_getParam(pSiteMgr->hRxData, pParam);

        tTwdParam.paramType = TWD_COUNTERS_PARAM_ID;
        TWD_GetParam (pSiteMgr->hTWD, &tTwdParam);
        pParam->content.siteMgrTiWlanCounters.RecvNoBuffer = tTwdParam.content.halCtrlCounters.RecvNoBuffer;
        pParam->content.siteMgrTiWlanCounters.FragmentsRecv = tTwdParam.content.halCtrlCounters.FragmentsRecv;
        pParam->content.siteMgrTiWlanCounters.FrameDuplicates = tTwdParam.content.halCtrlCounters.FrameDuplicates;
        pParam->content.siteMgrTiWlanCounters.FcsErrors = tTwdParam.content.halCtrlCounters.FcsErrors;
        pParam->content.siteMgrTiWlanCounters.RecvError = tTwdParam.content.halCtrlCounters.RecvError;

        pParam->paramType = AUTH_COUNTERS_PARAM;
        auth_getParam(pSiteMgr->hAuth, pParam);
        
		pParam->paramType = MLME_BEACON_RECV;
        mlme_getParam(pSiteMgr->hMlmeSm, pParam);
        
        pParam->paramType = ASSOC_COUNTERS_PARAM;
        assoc_getParam(pSiteMgr->hAssoc, pParam);
        pParam->content.siteMgrTiWlanCounters.BeaconsXmit = pSiteMgr->beaconSentCount;
        break;
    
    case SITE_MGR_FIRMWARE_VERSION_PARAM:
        {
            TFwInfo *pFwInfo = TWD_GetFWInfo (pSiteMgr->hTWD);
            os_memoryCopy(pSiteMgr->hOs, 
                          pParam->content.siteMgrFwVersion,
                          pFwInfo->fwVer,
                          sizeof(pFwInfo->fwVer));
        }
        break;

    case SITE_MGR_CURRENT_TX_RATE_PARAM:
        {   
            ERate rate = txCtrlParams_GetTxRate (pSiteMgr->hTxCtrl);
            pParam->content.siteMgrCurrentTxRate = rate_DrvToNet (rate);
        }
        break;

    case SITE_MGR_CURRENT_RX_RATE_PARAM:
        {
            pParam->paramType = RX_DATA_RATE_PARAM;
            rxData_getParam (pSiteMgr->hRxData, pParam);
            pParam->content.siteMgrCurrentRxRate =
                (TI_UINT8)rate_DrvToNet ((ERate)pParam->content.siteMgrCurrentRxRate);
        }
        break;

    case SITE_MGR_DESIRED_DOT11_MODE_PARAM:
        pParam->content.siteMgrDot11Mode = pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode;
        break;

	case SITE_MGR_NETWORK_TYPE_IN_USE:
		if (pPrimarySite)
		{ /* Connected - return the current mode */
			pParam->content.siteMgrDot11Mode = pSiteMgr->siteMgrOperationalMode;
		}
		else
		{ /* Disconnected - return the desired mode */
			pParam->content.siteMgrDot11Mode = pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode;
		}
        break;


    case SITE_MGR_OPERATIONAL_MODE_PARAM:
        pParam->content.siteMgrDot11OperationalMode = pSiteMgr->siteMgrOperationalMode;
        break;

    case SITE_MGR_RADIO_BAND_PARAM:
        pParam->content.siteMgrRadioBand = pSiteMgr->radioBand;
        break;

    case SITE_MGR_CURRENT_PREAMBLE_TYPE_PARAM:
        if (!pPrimarySite)
            return NO_SITE_SELECTED_YET;

        pParam->content.siteMgrCurrentPreambleType = pPrimarySite->currentPreambleType;
        break;

    case SITE_MGR_CURRENT_BSSID_PARAM:
        if (pPrimarySite != NULL)
        {
            MAC_COPY (pParam->content.siteMgrDesiredBSSID, pPrimarySite->bssid);
        }
		else
			return NO_SITE_SELECTED_YET;
        break;

    case SITE_MGR_LAST_RX_RATE_PARAM:
        if (pPrimarySite != NULL)
        {
            pParam->content.ctrlDataCurrentBasicRate = pPrimarySite->rxRate;
        }
        break;

	case SITE_MGR_GET_STATS:
        if (pPrimarySite != NULL)
        {
			pParam->content.siteMgrCurrentRssi = pPrimarySite->rssi;
        }
        break;

    case SITE_MGR_PREV_SITE_BSSID_PARAM:
        if (pSiteMgr->pSitesMgmtParams->pPrevPrimarySite==NULL)
        {
            return TI_NOK;
        }
        MAC_COPY (pParam->content.siteMgrDesiredBSSID, pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->bssid);
        break;

    case SITE_MGR_PREV_SITE_SSID_PARAM:
        if (pSiteMgr->pSitesMgmtParams->pPrevPrimarySite==NULL)
        {
            return TI_NOK;
        }
        /* It looks like it never happens. Anyway decided to check */
        if ( pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->ssid.len > MAX_SSID_LEN )
        {
            TRACE2( pSiteMgr->hReport, REPORT_SEVERITY_ERROR,
                   "siteMgr_getParam. pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->ssid.len=%d exceeds the limit %d\n",
                   pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->ssid.len, MAX_SSID_LEN);
            handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
            return TI_NOK;
        }
        pParam->content.siteMgrDesiredSSID.len = pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->ssid.len;
        os_memoryCopy(pSiteMgr->hOs, 
                      (void *)pParam->content.siteMgrDesiredSSID.str, 
                      (void *)pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->ssid.str, 
                      pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->ssid.len);
        break;

    case SITE_MGR_PREV_SITE_CHANNEL_PARAM:
        if (pSiteMgr->pSitesMgmtParams->pPrevPrimarySite==NULL)
        {
            return TI_NOK;
        }
        pParam->content.siteMgrDesiredChannel = pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->channel;
        break;

    case SITE_MGR_SITE_ENTRY_BY_INDEX:
        siteEntryIndex = pParam->content.siteMgrIndexOfDesiredSiteEntry;
        if(siteEntryIndex >= MAX_SITES_BG_BAND)
        {
            return TI_NOK;
        }
        pParam->content.pSiteMgrDesiredSiteEntry =
            (TI_UINT8*)(&(pSiteMgr->pSitesMgmtParams->pCurrentSiteTable->siteTable[siteEntryIndex]));
        break;

    case SITE_MGR_CUR_NUM_OF_SITES:
        pParam->content.siteMgrNumberOfSites = pSiteMgr->pSitesMgmtParams->pCurrentSiteTable->numOfSites;
        break;

    case SITE_MGR_CURRENT_TSF_TIME_STAMP:
        os_memoryCopy(pSiteMgr->hOs, pParam->content.siteMgrCurrentTsfTimeStamp,
                      pSiteMgr->pSitesMgmtParams->pPrimarySite->tsfTimeStamp,
                      TIME_STAMP_LEN);
        break;

    case SITE_MGR_GET_AP_QOS_CAPABILITIES:
       if (!pPrimarySite)
       {
           pParam->content.qosApCapabilities.uQOSFlag = 0;
           pParam->content.qosApCapabilities.uAPSDFlag = 0;
           TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Not connected to an AP...\n");
           return NOT_CONNECTED;
       }
       pParam->content.qosApCapabilities.uQOSFlag = pPrimarySite->WMESupported;
       pParam->content.qosApCapabilities.uAPSDFlag = pPrimarySite->APSDSupport;
       break;

    case SITE_MGR_GET_PRIMARY_SITE:
       if (!pPrimarySite)
       {
           pParam->content.pPrimarySite = (void *)NULL;
           return NOT_CONNECTED;
       }
       else
       {
           pParam->content.pPrimarySite = (void *)pPrimarySite;
       }
       break;

    case SITE_MGR_PRIMARY_SITE_HT_SUPPORT:
       if (!pPrimarySite)
       {
           pParam->content.bPrimarySiteHtSupport = TI_FALSE;
           return NOT_CONNECTED;
       }
       else
       {
           if((pPrimarySite->tHtCapabilities.tHdr[0] != TI_FALSE) && (pPrimarySite->tHtInformation.tHdr[0] != TI_FALSE))
           {
               pParam->content.bPrimarySiteHtSupport = TI_TRUE;
           }
           else
           {
               pParam->content.bPrimarySiteHtSupport = TI_FALSE;
           }
       }
       break;
    case SITE_MGRT_GET_RATE_MANAGMENT:
         return cmdBld_ItrRateParams (pSiteMgr->hTWD,
                                      pParam->content.interogateCmdCBParams.fCb,
                                      pParam->content.interogateCmdCBParams.hCb,
                                      (void*)pParam->content.interogateCmdCBParams.pCb);

    default:
        {
            TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Get param, Params is not supported, 0x%x\n", pParam->paramType);
        }

        return PARAM_NOT_SUPPORTED;
    }

    return status;
}


/***********************************************************************
 *                        siteMgr_join
 ***********************************************************************
DESCRIPTION: Called by the connection state machine in order to join a BSS.
                -   If the BSS is infrastructure, sets a NULL data template to the HAL
                -   If the BSS is IBSS, sets a probe response & beacon template to the HAL
            Call the HAL with the join parameters


INPUT:      hSiteMgr    -   site mgr handle.
            JoinCompleteCB - join command complete callback function ptr
            CB_handle - handle to pass to callback function

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS siteMgr_join(TI_HANDLE    hSiteMgr)
{
    siteMgr_t               *pSiteMgr = (siteMgr_t *)hSiteMgr;
    TJoinBss                joinParams;
    TSetTemplate            templateStruct;
    probeRspTemplate_t      probeRspTemplate;
    nullDataTemplate_t      nullDataTemplate;
    disconnTemplate_t       disconnTemplate;
    psPollTemplate_t        psPollTemplate;
    QosNullDataTemplate_t   QosNullDataTemplate;
    siteEntry_t             *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
    EPreamble               curPreamble;

    if (pPrimarySite == NULL)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Join BSS, Primary Site is NULL\n");
        return TI_OK;
    }

    /* Configure the system according to parameters of Primary Site */
    systemConfig(pSiteMgr);

    joinParams.bssType = pPrimarySite->bssType;
    joinParams.beaconInterval = pPrimarySite->beaconInterval;
    joinParams.dtimInterval = pPrimarySite->dtimPeriod;
    joinParams.pBSSID = (TI_UINT8 *)&pPrimarySite->bssid;
    joinParams.pSSID = (TI_UINT8 *)&pPrimarySite->ssid.str;
    joinParams.ssidLength = pPrimarySite->ssid.len;

    /*  
     * Set the radio band and the HW management Tx rate according to operational mode.
     * The HW management frames includes Beacon and Probe-Response (in IBSS). 
     */
    if(pSiteMgr->siteMgrOperationalMode == DOT11_A_MODE)
    {
        joinParams.radioBand = RADIO_BAND_5_0_GHZ;
    }
    else
    {
        joinParams.radioBand = RADIO_BAND_2_4_GHZ;
    }

    joinParams.channel = pPrimarySite->channel;
    if (joinParams.channel == SPECIAL_BG_CHANNEL)
    {
         joinParams.basicRateSet     = (TI_UINT16)rate_GetDrvBitmapForDefaultBasicSet ();
    }
    else /* != SPECIAL_BG_CHANNEL */
    {
        joinParams.basicRateSet = (TI_UINT16)pSiteMgr->pDesiredParams->siteMgrMatchedBasicRateMask;
    }

    ctrlData_getParamPreamble(pSiteMgr->hCtrlData, &curPreamble); /* CTRL_DATA_CURRENT_PREAMBLE_TYPE_PARAM */
    /* Set the preamble before the join */
    TWD_CfgPreamble (pSiteMgr->hTWD, curPreamble);

    /* Now, Set templates to the HAL */
    templateStruct.uRateMask = RATE_MASK_UNSPECIFIED;
    if (pPrimarySite->bssType == BSS_INDEPENDENT)
    {
        templateStruct.ptr = (TI_UINT8 *)&probeRspTemplate;
        templateStruct.type = PROBE_RESPONSE_TEMPLATE;
        buildProbeRspTemplate(pSiteMgr, &templateStruct);
        TWD_CmdTemplate (pSiteMgr->hTWD, &templateStruct, NULL, NULL);

        /* We don't have to build a beacon template, because it is equal to probe response,
        we only have to change the frame sup type */
        probeRspTemplate.hdr.fc = ENDIAN_HANDLE_WORD(DOT11_FC_BEACON);
        templateStruct.type = BEACON_TEMPLATE;
        TWD_CmdTemplate (pSiteMgr->hTWD, &templateStruct, NULL, NULL);
    }
    else
    {
        templateStruct.ptr = (TI_UINT8 *)&nullDataTemplate;
        templateStruct.type = NULL_DATA_TEMPLATE;
        buildNullTemplate(pSiteMgr, &templateStruct);
        TWD_CmdTemplate (pSiteMgr->hTWD, &templateStruct, NULL, NULL);

        /* Send PsPoll template to HAL */
        templateStruct.ptr = (TI_UINT8 *)&psPollTemplate;
        templateStruct.type = PS_POLL_TEMPLATE;
        buildPsPollTemplate(pSiteMgr, &templateStruct);
        TWD_CmdTemplate (pSiteMgr->hTWD, &templateStruct, NULL, NULL);
            
        /* Set QOS Null data template to the firmware.
            Note:  the AC to use with this template may change in QoS-manager. */
        templateStruct.ptr = (TI_UINT8 *)&QosNullDataTemplate;
        templateStruct.type = QOS_NULL_DATA_TEMPLATE;
        buildQosNullDataTemplate(pSiteMgr, &templateStruct, 0);
        TWD_CmdTemplate (pSiteMgr->hTWD, &templateStruct, NULL, NULL);

        /* Send disconnect (Deauth/Disassoc) template to HAL */
        templateStruct.ptr = (TI_UINT8 *)&disconnTemplate;
        templateStruct.type = DISCONN_TEMPLATE;
        buildDisconnTemplate(pSiteMgr, &templateStruct);
        TWD_CmdTemplate (pSiteMgr->hTWD, &templateStruct, NULL, NULL);
    }

    /* Reset the Tx Power Control adjustment in RegulatoryDomain */
    siteMgr_setTemporaryTxPower(pSiteMgr, TI_FALSE);     
    
	/* Get a new Tx-Session-Count (also updates the TxCtrl module). */
	joinParams.txSessionCount = incrementTxSessionCount(pSiteMgr);

    return TWD_CmdJoinBss (((siteMgr_t *)hSiteMgr)->hTWD, &joinParams);
}


/***********************************************************************
 *                        siteMgr_removeSelfSite
 ***********************************************************************
DESCRIPTION: Called by the Self connection state machine in order to remove the self site from the site table.
                Remove the site entry form the table and reset the primary site pointer


INPUT:      hSiteMgr    -   site mgr handle.

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS siteMgr_removeSelfSite(TI_HANDLE  hSiteMgr)
{
    siteMgr_t           *pSiteMgr  = (siteMgr_t *)hSiteMgr;
    siteTablesParams_t  *currTable = pSiteMgr->pSitesMgmtParams->pCurrentSiteTable;

    if(pSiteMgr->pSitesMgmtParams->pPrimarySite == NULL)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_WARNING, "Remove self site Failure, pointer is NULL\n\n");
        return TI_OK;
    }

    if(pSiteMgr->pSitesMgmtParams->pPrimarySite->siteType != SITE_SELF)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Remove self site Failure, site is not self\n\n");
        return TI_OK;
    }

    removeSiteEntry(pSiteMgr, currTable, pSiteMgr->pSitesMgmtParams->pPrimarySite);
    pSiteMgr->pSitesMgmtParams->pPrimarySite = NULL;

    return TI_OK;
}

/***********************************************************************
 *                        siteMgr_IbssMerge
 ***********************************************************************/
TI_STATUS siteMgr_IbssMerge(TI_HANDLE       hSiteMgr,
                          TMacAddr      	our_bssid,
						  TMacAddr      	new_bssid,
                          mlmeFrameInfo_t   *pFrameInfo,
                          TI_UINT8          rxChannel,
                          ERadioBand        band)
{
	siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t *pSite;
	paramInfo_t Param;
     
	pSite = findAndInsertSiteEntry(pSiteMgr, (TMacAddr*)&our_bssid, band);

	if(!pSite) {
TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "siteMgr_IbssMerge, cannot find our site table entry, our_bssid: %X-%X-%X-%X-%X-%X\n", 						   (our_bssid)[0], (our_bssid)[1], (our_bssid)[2], (our_bssid)[3], 						   (our_bssid)[4], (our_bssid)[5]);
		return TI_NOK;
	}

	updateSiteInfo(pSiteMgr, pFrameInfo, pSite, rxChannel);
    
	pSite->siteType = SITE_PRIMARY;
	pSiteMgr->pSitesMgmtParams->pPrimarySite = pSite;
    
	MAC_COPY(pSite->bssid, new_bssid);
    
	Param.paramType   = SITE_MGR_DESIRED_BSSID_PARAM;
    Param.paramLength = sizeof(TMacAddr);
	MAC_COPY(Param.content.siteMgrDesiredBSSID, new_bssid);

	siteMgr_setParam ( hSiteMgr, &Param );

	conn_ibssMerge(pSiteMgr->hConn);

	return TI_OK;
}



/***********************************************************************
 *                        siteMgr_updateSite
 ***********************************************************************
DESCRIPTION: Called by the MLME parser upon receiving a beacon or probe response.
            Performs the following:
                -   Insert the site entry into the site hash table
                -   Update the site information in the site table
                -   If the site is the primary site, it handles the PBCC algorithm if needed
                -   If the site is NULL (means it is the first frame received from this site)
                    we update the site type to be regular
                -   If the site type is self, we inform the self connection SM
                    that another station joined the network we created


INPUT:      hSiteMgr    -   site mgr handle.
            bssid       -   BSSID received
            pFrameInfo  -   Frame content after the parsing
            rxChannel   -   The channel on which frame was received
            band        -   Band on which frame was received
            measuring   -   Determines whether the beacon or probe response
                            has been received while a beacon measurement
                            took place

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS siteMgr_updateSite(TI_HANDLE          hSiteMgr,
                          TMacAddr      *bssid,
                          mlmeFrameInfo_t   *pFrameInfo,
                          TI_UINT8             rxChannel,
                          ERadioBand       band,
                          TI_BOOL              measuring)
{
    siteEntry_t *pSite;
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    paramInfo_t param;


    /* The following is not required, since the scanCncn is responsible to check
        the channels validity before scanning.
        The problem it caused was that when 802.11d is enabled, 
        channels that are valid for Passive only, will not be updated.*/
    /*if (isChannelSupprted(pSiteMgr->hRegulatoryDomain , rxChannel) == TI_FALSE)
    {
        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_WARNING, "Channel ERROR - try to register a site that its channel (=%d) isn't in the regulatory domain.\n\            registration ABORTED!!!", rxChannel);
        return TI_NOK;
    }*/


    pSite = findAndInsertSiteEntry(pSiteMgr, bssid, band);



    if (pSite == NULL)
    {
        TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Site Update failure, table is full, bssid: %X-%X-%X-%X-%X-%X\n", (*bssid)[0], (*bssid)[1], (*bssid)[2], (*bssid)[3], (*bssid)[4], (*bssid)[5]);
        return TI_OK;
    }

    updateSiteInfo(pSiteMgr, pFrameInfo, pSite, rxChannel);

    switch(pSite->siteType)
    {
    case SITE_PRIMARY:
        TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "PRIMARY site updated, bssid: %X-%X-%X-%X-%X-%X\n\n", (*bssid)[0], (*bssid)[1], (*bssid)[2], (*bssid)[3], (*bssid)[4], (*bssid)[5]);
        if (pSiteMgr->pSitesMgmtParams->pPrimarySite == NULL)
        {
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "siteMgr_updateSite: Primary Site Is NULL\n");
            pSite->siteType = SITE_REGULAR;
            break;
        }
        /* Now, if the following is TI_TRUE we perform the PBCC algorithm: */
        /* If the BSS type is infrastructure, &&
            The chosen modulation is PBCC &&
            The beacon modulation is not NONE &&
            The current data modulation is different than the beacon modulation. */
        if ((pSite->bssType == BSS_INFRASTRUCTURE) &&
            (pSiteMgr->chosenModulation == DRV_MODULATION_PBCC) &&
            (pSite->beaconModulation != DRV_MODULATION_NONE) &&
            (pSiteMgr->currentDataModulation != pSite->beaconModulation))
        {
            pSiteMgr->currentDataModulation = pSite->beaconModulation;
            pbccAlgorithm(pSiteMgr);
        }

        /* Now handle the slot time, first check if the slot time changed since the last
           setting to the HAL ,and if yes set the new value */
        if((pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE) &&
           (pSite->bssType == BSS_INFRASTRUCTURE))
        {
            if (pSite->currentSlotTime != pSite->newSlotTime)
            {
                pSite->currentSlotTime = pSite->newSlotTime;
                TWD_CfgSlotTime (pSiteMgr->hTWD, pSite->currentSlotTime);
            }
        }

        /* Now handle the current protection status */
        if((pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE) && (pSite->bssType == BSS_INFRASTRUCTURE))
        {
            param.paramType = CTRL_DATA_CURRENT_PROTECTION_STATUS_PARAM;
            param.content.ctrlDataProtectionEnabled = pSite->useProtection;
            ctrlData_setParam(pSiteMgr->hCtrlData, &param);
        }

        /* Now handle the current preamble type,
           if desired preamble type is long, the ctrl data param should not be changed */
        if((pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE) &&
           (pSite->bssType == BSS_INFRASTRUCTURE) &&
           (pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType != PREAMBLE_LONG))
        {
            param.paramType = CTRL_DATA_CURRENT_PREAMBLE_TYPE_PARAM;
            if((pSite->preambleAssRspCap == PREAMBLE_LONG) ||
               (pSite->barkerPreambleType == PREAMBLE_LONG))
                  {
                param.content.ctrlDataCurrentPreambleType = PREAMBLE_LONG;
            }
            else
                param.content.ctrlDataCurrentPreambleType = PREAMBLE_SHORT;

            ctrlData_setParam(pSiteMgr->hCtrlData, &param);
        }

        param.paramType = CTRL_DATA_RATE_CONTROL_ENABLE_PARAM;
        ctrlData_setParam(pSiteMgr->hCtrlData, &param);
        break;

    case SITE_NULL:
        pSite->siteType = SITE_REGULAR;
        TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "REGULAR site added, bssid: %X-%X-%X-%X-%X-%X\n\n", (*bssid)[0], (*bssid)[1], (*bssid)[2], (*bssid)[3], (*bssid)[4], (*bssid)[5]);
        break;

    case SITE_SELF:
        pSite->siteType = SITE_PRIMARY;
        TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "SELF ----> PRIMARY site , bssid: %X-%X-%X-%X-%X-%X\n\n", (*bssid)[0], (*bssid)[1], (*bssid)[2], (*bssid)[3], (*bssid)[4], (*bssid)[5]);
        conn_ibssStaJoined(pSiteMgr->hConn);
        break;

    case SITE_REGULAR:
        TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "REGULAR site updated, bssid: %X-%X-%X-%X-%X-%X\n\n", (*bssid)[0], (*bssid)[1], (*bssid)[2], (*bssid)[3], (*bssid)[4], (*bssid)[5]);
        break;

    default:
        TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Setting site type failure, bssid: %X-%X-%X-%X-%X-%X\n\n", (*bssid)[0], (*bssid)[1], (*bssid)[2], (*bssid)[3], (*bssid)[4], (*bssid)[5]);
        break;
    }

    return TI_OK;
}

/***********************************************************************
 *                        siteMgr_start
 ***********************************************************************
DESCRIPTION: Called by the SME SM in order to start the aging timer


INPUT:      hSiteMgr    -   site mgr handle.

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS siteMgr_start(TI_HANDLE   hSiteMgr)
{
    siteMgr_t       *pSiteMgr = (siteMgr_t *)hSiteMgr;

    /* update timestamp each time aging started (needed for quiet scan) */
    if(pSiteMgr->pSitesMgmtParams->pPrimarySite)
        pSiteMgr->pSitesMgmtParams->pPrimarySite->localTimeStamp = os_timeStampMs(pSiteMgr->hOs);

    return TI_OK;
}


/***********************************************************************
 *                        siteMgr_stop
 ***********************************************************************
DESCRIPTION: Called by the SME SM in order to stop site mgr timers


INPUT:      hSiteMgr    -   site mgr handle.

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS siteMgr_stop(TI_HANDLE    hSiteMgr)
{

    return TI_OK;
}


/***********************************************************************
 *                        siteMgr_updatePrimarySiteFailStatus
 ***********************************************************************
DESCRIPTION: Called by the SME SM when the connection with the primary site fails
                If the primary site is NULL, return.


INPUT:      hSiteMgr    -   site mgr handle.
            bRemoveSite -   Whether to remove the site

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS siteMgr_updatePrimarySiteFailStatus(TI_HANDLE hSiteMgr,
                                           TI_BOOL bRemoveSite)
{
    siteMgr_t           *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteTablesParams_t  *currTable = pSiteMgr->pSitesMgmtParams->pCurrentSiteTable;

    if (pSiteMgr->pSitesMgmtParams->pPrimarySite == NULL)
        return TI_OK;

    TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, " SITE MGR: bRemoveSite = %d \n", bRemoveSite);

    if (bRemoveSite) 
    {
        removeSiteEntry(pSiteMgr, currTable, pSiteMgr->pSitesMgmtParams->pPrimarySite);
        pSiteMgr->pSitesMgmtParams->pPrimarySite = NULL;
    }
    else	/* Currently never used */
    {
        pSiteMgr->pSitesMgmtParams->pPrimarySite->failStatus = STATUS_UNSPECIFIED;
    }

    return TI_OK;
}


/***********************************************************************
 *                        siteMgr_isCurrentBand24
 ***********************************************************************
DESCRIPTION: The function checks the current operational mode and
                returns if the current band is 2.4Ghz or 5Ghz.

INPUT:      hSiteMgr    -   site mgr handle.

OUTPUT:

RETURN:     TI_TRUE if current band is 2.4Ghz, TI_FALSE otherwise.

************************************************************************/
TI_BOOL siteMgr_isCurrentBand24(TI_HANDLE  hSiteMgr)
{
    siteMgr_t   *pSiteMgr =     (siteMgr_t *)hSiteMgr;

    if(pSiteMgr->siteMgrOperationalMode == DOT11_A_MODE)
        return TI_FALSE;

    return TI_TRUE; /* 802.11b supports onlty 2.4G band */

}

/***********************************************************************
 *                        removeEldestSite
 ***********************************************************************
DESCRIPTION: Called by the select when trying to create an IBSS and site table is full
                Remove the eldest site from the table

INPUT:      hSiteMgr    -   site mgr handle.

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS removeEldestSite(siteMgr_t *pSiteMgr)
{
    int             i;
    siteEntry_t     *pEldestSite = NULL, *pSiteTable = pSiteMgr->pSitesMgmtParams->pCurrentSiteTable->siteTable;
    siteTablesParams_t  *currTable = pSiteMgr->pSitesMgmtParams->pCurrentSiteTable;
    TI_UINT32          currentTimsStamp = os_timeStampMs(pSiteMgr->hOs);
    TI_UINT32          biggestGap = 0;

    for (i = 0; i < currTable->maxNumOfSites; i++)
    {
        if (biggestGap < ((TI_UINT32)(currentTimsStamp - pSiteTable[i].localTimeStamp)))
        {
            biggestGap = ((TI_UINT32)(currentTimsStamp - pSiteTable[i].localTimeStamp));
            pEldestSite = &(pSiteTable[i]);
        }
    }

    removeSiteEntry(pSiteMgr, currTable, pEldestSite);

    return TI_OK;
}


/***********************************************************************
 *                        update_apsd
 ***********************************************************************
DESCRIPTION:    Sets the site APSD support flag according to the
                beacon's capabilities vector and the WME-params IE if exists.

INPUT:      pSite       -   Pointer to the site entry in the site table
            pFrameInfo  -   Frame information after the parsing
            
OUTPUT:     pSite->APSDSupport flag

RETURN:     

************************************************************************/
static void update_apsd(siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo)
{
    /* If WME-Params IE is not included in the beacon, set the APSD-Support flag
         only by the beacons capabilities bit map. */
    if (pFrameInfo->content.iePacket.WMEParams == NULL)
        pSite->APSDSupport = (((pFrameInfo->content.iePacket.capabilities >> CAP_APSD_SHIFT) & CAP_APSD_MASK) ? TI_TRUE : TI_FALSE);

    /* Else, set the APSD-Support flag if either the capabilities APSD bit or the 
         WME-Params APSD bit indicate so. */
    else
        pSite->APSDSupport = ((((pFrameInfo->content.iePacket.capabilities >> CAP_APSD_SHIFT) & CAP_APSD_MASK) ? TI_TRUE : TI_FALSE) ||
        (((pFrameInfo->content.iePacket.WMEParams->ACInfoField >> AP_QOS_INFO_UAPSD_SHIFT) & AP_QOS_INFO_UAPSD_MASK) ? TI_TRUE : TI_FALSE));
}


/***********************************************************************
 *                        release_module
 ***********************************************************************
DESCRIPTION:    Called by the un load function
                Go over the vector, for each bit that is set, release the corresponding module.

INPUT:      pSiteMgr    -   site mgr handle.
            initVec -   Vector that contains a bit set for each module thah had been initiualized

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static void release_module(siteMgr_t *pSiteMgr, TI_UINT32 initVec)
{
    if ( (initVec & (1 << MGMT_PARAMS_INIT_BIT)) && pSiteMgr->pSitesMgmtParams)
        os_memoryFree(pSiteMgr->hOs, pSiteMgr->pSitesMgmtParams, sizeof(sitesMgmtParams_t));

    if ( (initVec & (1 << DESIRED_PARAMS_INIT_BIT)) && pSiteMgr->pDesiredParams)
        os_memoryFree(pSiteMgr->hOs, pSiteMgr->pDesiredParams, sizeof(siteMgrInitParams_t));

    if (initVec & (1 << SITE_MGR_INIT_BIT))
        os_memoryFree(pSiteMgr->hOs, pSiteMgr, sizeof(siteMgr_t));

    initVec = 0;
}


static TI_BOOL isIeSsidBroadcast (dot11_SSID_t *pIESsid)
{
    if ((pIESsid == NULL) || (pIESsid->hdr[1] == 0))
    {
        return TI_TRUE;
    }

    /* According to 802.11, Broadcast SSID should be with length 0,
        however, different vendors use invalid chanrs for Broadcast SSID. */
    if (pIESsid->serviceSetId[0] < OS_802_11_SSID_FIRST_VALID_CHAR)
    {
        return TI_TRUE;
    }

    return TI_FALSE;
}


/***********************************************************************
 *                        updateSiteInfo
 ***********************************************************************
DESCRIPTION:    Called upon receiving a beacon or probe response
                Go over the vector, for each bit that is set, release the corresponding module.
                Update theaite entry in the site table with the information received in the frame

INPUT:      pSiteMgr    -   site mgr handle.
            pFrameInfo  -   Frame information after the parsing
            pSite       -   Pointer to the site entry in the site table

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static void updateSiteInfo(siteMgr_t *pSiteMgr, mlmeFrameInfo_t *pFrameInfo, siteEntry_t    *pSite, TI_UINT8 rxChannel)
{
    paramInfo_t param;
    TI_BOOL        ssidUpdated = TI_FALSE;

    switch (pFrameInfo->subType)
    {
    case BEACON:

        UPDATE_BEACON_INTERVAL(pSite, pFrameInfo);
        
        UPDATE_CAPABILITIES(pSite, pFrameInfo);
        

        /***********************************/
        /* Must be before UPDATE_PRIVACY and UPDATE_RSN_IE */
        if (pSite->ssid.len==0)
        {   /* Update the SSID only if the site's SSID is NULL */
            if (isIeSsidBroadcast(pFrameInfo->content.iePacket.pSsid) == TI_FALSE)
            {   /* And the SSID is not Broadcast */
                ssidUpdated = TI_TRUE;
                UPDATE_SSID(pSite, pFrameInfo);
            }
        }
        else if (pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len > 0)
        {   /* There is a desired SSID */
            if (pFrameInfo->content.iePacket.pSsid != NULL) 
            {
                if (os_memoryCompare (pSiteMgr->hOs, 
                                      (TI_UINT8 *)pSiteMgr->pDesiredParams->siteMgrDesiredSSID.str,
                                      (TI_UINT8 *)pFrameInfo->content.iePacket.pSsid->serviceSetId, 
                                      pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len)==0)
                {   /* update only SSID that equals the desired SSID */
                    ssidUpdated = TI_TRUE;
                    UPDATE_SSID(pSite, pFrameInfo);
                }
            }
            else
            {
                TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "updateSiteInfo BEACON, pSsid=NULL\n");
            }
        }
        /***********************************/
 
        if (ssidUpdated)
        {

            UPDATE_PRIVACY(pSite, pFrameInfo);
        }

        update_apsd(pSite, pFrameInfo);

        updatePreamble(pSiteMgr, pSite, pFrameInfo);

        UPDATE_AGILITY(pSite, pFrameInfo);


        if(pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE)
        {
            UPDATE_SLOT_TIME(pSite, pFrameInfo);
            UPDATE_PROTECTION(pSite, pFrameInfo);
        }

        /* Updating HT params */
        siteMgr_UpdatHtParams ((TI_HANDLE)pSiteMgr, pSite, pFrameInfo);

        updateRates(pSiteMgr, pSite, pFrameInfo);

        if ((pFrameInfo->content.iePacket.pDSParamsSet != NULL)  &&
            (pFrameInfo->content.iePacket.pDSParamsSet->currChannel!=rxChannel))
        {
            TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "updateSiteInfo, wrong CHANNELS:rxChannel=%d,currChannel=%d\n", rxChannel, pFrameInfo->content.iePacket.pDSParamsSet->currChannel);
        }
        else
            UPDATE_CHANNEL(pSite, pFrameInfo , rxChannel);


        UPDATE_BSS_TYPE(pSite, pFrameInfo);

        if (pSite->bssType == BSS_INFRASTRUCTURE)
            UPDATE_DTIM_PERIOD(pSite, pFrameInfo);

        UPDATE_ATIM_WINDOW(pSite, pFrameInfo);

        UPDATE_BEACON_AP_TX_POWER(pSite, pFrameInfo);

        /* Updating QoS params */
        updateBeaconQosParams(pSiteMgr, pSite, pFrameInfo);


        /* updating CountryIE  */
        if ((pFrameInfo->content.iePacket.country  != NULL) && 
			(pFrameInfo->content.iePacket.country->hdr[1] != 0))
        {
            /* set the country info in the regulatory domain - If a different code was detected earlier
               the regDomain will ignore it */
            param.paramType = REGULATORY_DOMAIN_COUNTRY_PARAM;
            param.content.pCountry = (TCountry *)pFrameInfo->content.iePacket.country;
            regulatoryDomain_setParam(pSiteMgr->hRegulatoryDomain,&param);
        }

        /* Updating WSC params */
        updateWSCParams(pSiteMgr, pSite, pFrameInfo); 
  
        UPDATE_LOCAL_TIME_STAMP(pSiteMgr, pSite, pFrameInfo);
        
        UPDATE_BEACON_MODULATION(pSite, pFrameInfo);

        /* If the BSS type is independent, the beacon & probe modulation are equal,
            It is important to update this field here for dynamic PBCC algorithm compatibility */
        if (pSite->bssType == BSS_INDEPENDENT)
            UPDATE_PROBE_MODULATION(pSite, pFrameInfo);


        if (pSite->siteType == SITE_PRIMARY)
        {
           
            if (pSiteMgr->pSitesMgmtParams->pPrimarySite == NULL)
            {
                TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "updateSiteInfo: Primary Site Is NULL\n");
                pSite->siteType = SITE_REGULAR;
            }
            else
            {
                /*  If the site that we got the beacon on is the primary site - which means we are either trying */
                /*  to connect to it or we are already connected - send the EVENT_GOT_BEACON to the conn module (through the SME module) */
                /*  so the conn module will be aware of the beacon status of the site it's trying to connect to */
                
#ifdef XCC_MODULE_INCLUDED
                TI_INT8 ExternTxPower;

                if (pFrameInfo->content.iePacket.cellTP != NULL)
                {
                    ExternTxPower = pFrameInfo->content.iePacket.cellTP->power;
                }
                else	/* Set to maximum possible. Note that we add +1 so that Dbm = 26 and not 25 */
                {
                    ExternTxPower = MAX_TX_POWER / DBM_TO_TX_POWER_FACTOR + 1;
                }

                param.paramType = REGULATORY_DOMAIN_EXTERN_TX_POWER_PREFERRED;
                param.content.ExternTxPowerPreferred = ExternTxPower;
                regulatoryDomain_setParam(pSiteMgr->hRegulatoryDomain, &param);
#endif

                /* Updating the Tx Power according to the received Power Constraint  */
                if(pFrameInfo->content.iePacket.powerConstraint  != NULL)
                {   /* Checking if the recieved constraint is different from the one that is already known  */
                    if( pFrameInfo->content.iePacket.powerConstraint->powerConstraint != pSite->powerConstraint)
                    {   /* check if Spectrum Management is enabled */
                        param.paramType = REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM;
                        regulatoryDomain_getParam(pSiteMgr->hRegulatoryDomain,&param);
                        if(param.content.spectrumManagementEnabled)
                        {   /* setting power constraint */
                            pSite->powerConstraint = pFrameInfo->content.iePacket.powerConstraint->powerConstraint;
                            param.paramType = REGULATORY_DOMAIN_SET_POWER_CONSTRAINT_PARAM;
                            param.content.powerConstraint = pSite->powerConstraint;
                            regulatoryDomain_setParam(pSiteMgr->hRegulatoryDomain,&param);
                
                        }
                    }
                }
                /* update HT Information IE at the FW whenever any of its relevant fields is changed. */
                if (pSite->bHtInfoUpdate == TI_TRUE)
                {
                    TI_BOOL b11nEnable, bWmeEnable;

                    /* verify 11n_Enable and Chip type */
                    StaCap_IsHtEnable (pSiteMgr->hStaCap, &b11nEnable);

                    /* verify that WME flag enable */
                    qosMngr_GetWmeEnableFlag (pSiteMgr->hQosMngr, &bWmeEnable); 

                    if ((b11nEnable != TI_FALSE) && (bWmeEnable != TI_FALSE))
                    {
                        TWD_CfgSetFwHtInformation (pSiteMgr->hTWD, &pSite->tHtInformation);
                    }
                }
            }
        }

        UPDATE_BEACON_RECV(pSite);

        if (ssidUpdated)
        {
            dot11_RSN_t *pRsnIe = pFrameInfo->content.iePacket.pRsnIe;
            TI_UINT8       rsnIeLen = pFrameInfo->content.iePacket.rsnIeLen;
            UPDATE_RSN_IE(pSite, pRsnIe, rsnIeLen);
        }
        
        UPDATE_BEACON_TIMESTAMP(pSiteMgr, pSite, pFrameInfo);

        
        
        TWD_UpdateDtimTbtt (pSiteMgr->hTWD, pSite->dtimPeriod, pSite->beaconInterval);
       
        break;


    case PROBE_RESPONSE:

        UPDATE_BEACON_INTERVAL(pSite, pFrameInfo);

        UPDATE_CAPABILITIES(pSite, pFrameInfo);

                ssidUpdated = TI_TRUE;
        if (pSite->siteType == SITE_PRIMARY)
        {   /* Primary SITE */
            if (pFrameInfo->content.iePacket.pSsid != NULL && 
                pSiteMgr->pDesiredParams->siteMgrDesiredSSID.len > 0)
            {   /* There's a desired SSID*/
                if (os_memoryCompare (pSiteMgr->hOs, 
                                      (TI_UINT8*)pSiteMgr->pDesiredParams->siteMgrDesiredSSID.str, 
                                      (TI_UINT8*)pFrameInfo->content.iePacket.pSsid->serviceSetId, 
                                      pFrameInfo->content.iePacket.pSsid->hdr[1])!=0)
                {   /* Do not overwrite the primary site's SSID with a different than the desired SSID*/
                    ssidUpdated = TI_FALSE;
                }
                
            }
            else if (pFrameInfo->content.iePacket.pSsid == NULL)
            {
                TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "updateSiteInfo PROBE_RESP, pSsid=NULL\n");
            }
        }

        if (ssidUpdated)
        {
            UPDATE_SSID(pSite, pFrameInfo);
            UPDATE_PRIVACY(pSite, pFrameInfo);
        }

        update_apsd(pSite, pFrameInfo);


        if(pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE)
        {
            UPDATE_PROTECTION(pSite, pFrameInfo);
        }

        updatePreamble(pSiteMgr, pSite, pFrameInfo);

        UPDATE_AGILITY(pSite, pFrameInfo);

        /* Updating HT params */
        siteMgr_UpdatHtParams ((TI_HANDLE)pSiteMgr, pSite, pFrameInfo);

        updateRates(pSiteMgr, pSite, pFrameInfo);

        if ((pFrameInfo->content.iePacket.pDSParamsSet != NULL)  &&
            (pFrameInfo->content.iePacket.pDSParamsSet->currChannel!=rxChannel))
        {
            TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "updateSiteInfo, wrong CHANNELS:rxChannel=%d,currChannel=%d\n", rxChannel, pFrameInfo->content.iePacket.pDSParamsSet->currChannel);
        }
        else
            UPDATE_CHANNEL(pSite, pFrameInfo, rxChannel);


        UPDATE_BSS_TYPE(pSite, pFrameInfo);

        UPDATE_ATIM_WINDOW(pSite, pFrameInfo);

        UPDATE_PROBE_AP_TX_POWER(pSite, pFrameInfo);

        /* Updating WME params */
        updateProbeQosParams(pSiteMgr, pSite, pFrameInfo);


        /* updating CountryIE  */
        if ((pFrameInfo->content.iePacket.country  != NULL) && 
			(pFrameInfo->content.iePacket.country->hdr[1] != 0))
        {
            /* set the country info in the regulatory domain - If a different code was detected earlier
               the regDomain will ignore it */
            param.paramType = REGULATORY_DOMAIN_COUNTRY_PARAM;
            param.content.pCountry = (TCountry *)pFrameInfo->content.iePacket.country;
            regulatoryDomain_setParam(pSiteMgr->hRegulatoryDomain,&param);
        }

        /* Updating WSC params */
        updateWSCParams(pSiteMgr, pSite, pFrameInfo); 
        
        UPDATE_LOCAL_TIME_STAMP(pSiteMgr, pSite, pFrameInfo);

        UPDATE_PROBE_MODULATION(pSite, pFrameInfo);

        /* If the BSS type is independent, the beacon & probe modulation are equal,
            It is important to update this field here for dynamic PBCC algorithm compatibility */
        if (pSite->bssType == BSS_INDEPENDENT)
            UPDATE_BEACON_MODULATION(pSite, pFrameInfo);

        UPDATE_PROBE_RECV(pSite);

        if (ssidUpdated)
        {
            dot11_RSN_t *pRsnIe = pFrameInfo->content.iePacket.pRsnIe;
            TI_UINT8       rsnIeLen = pFrameInfo->content.iePacket.rsnIeLen;
            UPDATE_RSN_IE(pSite, pRsnIe, rsnIeLen);

        }

        UPDATE_BEACON_TIMESTAMP(pSiteMgr, pSite, pFrameInfo);

        break;

    default:
        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Site Update failure, un known frame sub type %d\n\n", pFrameInfo->subType);
        break;
    }
}

/***********************************************************************
 *                        updatePreamble
 ***********************************************************************
DESCRIPTION:    Called by the function 'updateSiteInfo()'

INPUT:      pSiteMgr    -   site mgr handle.
            pFrameInfo  -   Frame information after the parsing
            pSite       -   Pointer to the site entry in the site table

OUTPUT:

RETURN:

************************************************************************/
static void updatePreamble(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo)
{
    pSite->currentPreambleType = ((pFrameInfo->content.iePacket.capabilities >> CAP_PREAMBLE_SHIFT) & CAP_PREAMBLE_MASK) ? PREAMBLE_SHORT : PREAMBLE_LONG;

    pSite->barkerPreambleType = pFrameInfo->content.iePacket.barkerPreambleMode;
}

/***********************************************************************
 *                        updateBeaconQosParams
 ***********************************************************************
DESCRIPTION:    Called by the function 'updateSiteInfo()'

INPUT:      pSiteMgr    -   site mgr handle.
            pFrameInfo  -   Frame information after the parsing
            pSite       -   Pointer to the site entry in the site table

OUTPUT:

RETURN:

************************************************************************/

static void updateBeaconQosParams(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo)
{
    /* Updating WME params */
    if (pFrameInfo->content.iePacket.WMEParams  != NULL)
    {
        /* Checking if this is IE includes new WME Parameters */
        if(( ((pFrameInfo->content.iePacket.WMEParams->ACInfoField) & dot11_WME_ACINFO_MASK ) != pSite->lastWMEParameterCnt) ||
            (!pSite->WMESupported) )
        {
            pSite->WMESupported = TI_TRUE;

            /* Checking if this IE is information only or is a paremeters IE */
            if(pFrameInfo->content.iePacket.WMEParams->OUISubType == dot11_WME_OUI_SUB_TYPE_PARAMS_IE)
            {
                if(pSite->siteType == SITE_PRIMARY)
                {
                    qosMngr_updateIEinfo(pSiteMgr->hQosMngr,(TI_UINT8 *)(pFrameInfo->content.iePacket.WMEParams), QOS_WME);
                }
                /* updating the QOS_WME paraeters into the site table. */
                os_memoryCopy(pSiteMgr->hOs, &pSite->WMEParameters, &(pFrameInfo->content.iePacket.WMEParams->WME_ACParameteres), sizeof( dot11_ACParameters_t));
                pSite->lastWMEParameterCnt = (pFrameInfo->content.iePacket.WMEParams->ACInfoField) & dot11_WME_ACINFO_MASK;
                TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "$$$$$$ QOS_WME parameters were updates according to beacon, cntSeq = %d\n",pSite->lastWMEParameterCnt);
            }
        }
    }else
    {
        pSite->WMESupported = TI_FALSE;
        }

}

/***********************************************************************
 *                        updateProbeQosParams
 ***********************************************************************
DESCRIPTION:    Called by the function 'updateSiteInfo()'

INPUT:      pSiteMgr    -   site mgr handle.
            pFrameInfo  -   Frame information after the parsing
            pSite       -   Pointer to the site entry in the site table

OUTPUT:

RETURN:

************************************************************************/
static void updateProbeQosParams(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo)
{
    /* Updating QOS_WME params */
    if (pFrameInfo->content.iePacket.WMEParams  != NULL)
    {
        /* Checking if this is IE includes new QOS_WME Parameters */
        if(( ((pFrameInfo->content.iePacket.WMEParams->ACInfoField) & dot11_WME_ACINFO_MASK ) != pSite->lastWMEParameterCnt) ||
            (!pSite->WMESupported) )
        {
            pSite->WMESupported = TI_TRUE;

            /* Checking if this IE is information only or is a paremeters IE */
            if(pFrameInfo->content.iePacket.WMEParams->OUISubType == dot11_WME_OUI_SUB_TYPE_PARAMS_IE)
            {
                if(pSite->siteType == SITE_PRIMARY)
                {
                    qosMngr_updateIEinfo(pSiteMgr->hQosMngr,(TI_UINT8 *)(pFrameInfo->content.iePacket.WMEParams),QOS_WME);
                }
                /* updating the QOS_WME paraeters into the site table. */
                os_memoryCopy(pSiteMgr->hOs, &pSite->WMEParameters, &(pFrameInfo->content.iePacket.WMEParams->WME_ACParameteres), sizeof( dot11_ACParameters_t));
                pSite->lastWMEParameterCnt = (pFrameInfo->content.iePacket.WMEParams->ACInfoField) & dot11_WME_ACINFO_MASK;
                TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "$$$$$$ QOS_WME parameters were updates according to  probe response, cntSeq = %d\n",pSite->lastWMEParameterCnt);
            }
        }
    }else
    {
        pSite->WMESupported = TI_FALSE;
        }

}

/***********************************************************************
 *                        siteMgr_UpdatHtParams
 ***********************************************************************
DESCRIPTION:    Called by the function 'updateSiteInfo()' and also from scanResultTable module!

INPUT:      hSiteMgr    -   site mgr handle.
            pSite       -   Pointer to the site entry in the site table
            pFrameInfo  -   Frame information after the parsing

OUTPUT:

RETURN:

************************************************************************/
void siteMgr_UpdatHtParams (TI_HANDLE hSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo)
{                    
    siteMgr_t *pSiteMgr = (siteMgr_t *)hSiteMgr;

    /* Updating HT capabilites IE params */
    if (pFrameInfo->content.iePacket.pHtCapabilities != NULL)
    {
	   /* updating the HT capabilities unparse format into the site table. */
       os_memoryCopy (pSiteMgr->hOs, &pSite->tHtCapabilities, 
                      (TI_UINT8 *)(pFrameInfo->content.iePacket.pHtCapabilities), 
                      sizeof(Tdot11HtCapabilitiesUnparse));

       TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION , "$$$$$$ HT capabilites parameters were updates.\n");
    }
    else
    {
        pSite->tHtCapabilities.tHdr[0] = TI_FALSE;
    }

    /* Updating HT Information IE params */
	if (pFrameInfo->content.iePacket.pHtInformation != NULL)
	{
	   /* update in case different setting vlaue from the last one */
       if (os_memoryCompare (pSiteMgr->hOs, 
                             (TI_UINT8 *)&pSite->tHtInformation,
                             (TI_UINT8 *)pFrameInfo->content.iePacket.pHtInformation, 
                             sizeof(Tdot11HtInformationUnparse)) != 0)
       {
           pSite->bHtInfoUpdate = TI_TRUE;
           /* updating the HT Information unparse pormat into the site table. */
           os_memoryCopy (pSiteMgr->hOs, &pSite->tHtInformation, pFrameInfo->content.iePacket.pHtInformation, sizeof(Tdot11HtInformationUnparse));
           TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION , "$$$$$$ HT Information parameters were updates.\n");
       }
       else
       {
           pSite->bHtInfoUpdate = TI_FALSE;
       }
    }
	else
    {
		pSite->tHtInformation.tHdr[0] = TI_FALSE;
    }
}


/***********************************************************************
 *                        updateWSCParams
 ***********************************************************************
DESCRIPTION:    Called by the function 'updateSiteInfo()'

INPUT:      pSiteMgr    -   site mgr handle.
            pFrameInfo  -   Frame information after the parsing
            pSite       -   Pointer to the site entry in the site table

OUTPUT:

RETURN:

************************************************************************/
static void updateWSCParams(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo)
{
   TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "updateWSCParams called (BSSID: %X-%X-%X-%X-%X-%X)\n",pSite->bssid[0], pSite->bssid[1], pSite->bssid[2], pSite->bssid[3], pSite->bssid[4], pSite->bssid[5]);

	/* if the IE is not null => the WSC is on - check which method is supported */
	if (pFrameInfo->content.iePacket.WSCParams  != NULL)
	{
         parseWscMethodFromIE (pSiteMgr, pFrameInfo->content.iePacket.WSCParams, &pSite->WSCSiteMode);

         TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "pSite->WSCSiteMode = %d\n",pSite->WSCSiteMode);
	}
	else
	{
			pSite->WSCSiteMode = TIWLN_SIMPLE_CONFIG_OFF;
	}

}

static int parseWscMethodFromIE (siteMgr_t *pSiteMgr, dot11_WSC_t *WSCParams, TIWLN_SIMPLE_CONFIG_MODE *pSelectedMethod)
{
   TI_UINT8 *tlvPtr,*endPtr;
   TI_UINT16   tlvPtrType,tlvPtrLen,selectedMethod=0;

   tlvPtr = (TI_UINT8*)WSCParams->WSCBeaconOrProbIE;
   endPtr = tlvPtr + WSCParams->hdr[1] - DOT11_OUI_LEN;

   do
   {
      os_memoryCopy (pSiteMgr->hOs, (void *)&tlvPtrType, (void *)tlvPtr, 2);
      tlvPtrType = WLANTOHS(tlvPtrType);

      /*if (tlvPtrType == DOT11_WSC_SELECTED_REGISTRAR_CONFIG_METHODS)*/
      if (tlvPtrType == DOT11_WSC_DEVICE_PASSWORD_ID)
      {
         tlvPtr+=2;
         tlvPtr+=2;
         os_memoryCopy (pSiteMgr->hOs, (void *)&selectedMethod, (void *)tlvPtr, 2);
         selectedMethod = WLANTOHS (selectedMethod);
         break;
      }
      else
      {
         tlvPtr+=2;
         os_memoryCopy (pSiteMgr->hOs, (void *)&tlvPtrLen, (void *)tlvPtr, 2);
         tlvPtrLen = WLANTOHS (tlvPtrLen);
         tlvPtr+=tlvPtrLen+2;
      }
   } while ((tlvPtr < endPtr) && (selectedMethod == 0));
   
   if (tlvPtr >= endPtr)
   {
      return TI_NOK;
   }

   if (selectedMethod == DOT11_WSC_DEVICE_PASSWORD_ID_PIN)
      *pSelectedMethod = TIWLN_SIMPLE_CONFIG_PIN_METHOD;
   else if (selectedMethod == DOT11_WSC_DEVICE_PASSWORD_ID_PBC)
      *pSelectedMethod = TIWLN_SIMPLE_CONFIG_PBC_METHOD;
   else return TI_NOK;

   return TI_OK;
}


/***********************************************************************
 *                        updateRates
 ***********************************************************************
DESCRIPTION:    Called by the function 'updateSiteInfo()' in order to translate the rates received
                in the beacon or probe response to rate used by the driver. Perfoms the following:
                    -   Check the rates validity. If rates are invalid, return
                    -   Get the max active rate & max basic rate, if invalid, return
                    -   Translate the max active rate and max basic rate from network rates to host rates.
                        The max active & max basic rate are used by the driver from now on in all the processes:
                        (selection, join, transmission, etc....)

INPUT:      pSiteMgr    -   site mgr handle.
            pFrameInfo  -   Frame information after the parsing
            pSite       -   Pointer to the site entry in the site table

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static void updateRates(siteMgr_t *pSiteMgr, siteEntry_t *pSite, mlmeFrameInfo_t *pFrameInfo)
{
    TI_UINT8   maxBasicRate = 0, maxActiveRate = 0;
    TI_UINT32  bitMapExtSupp = 0;
    paramInfo_t param;
    TI_UINT32   uMcsbasicRateMask, uMcsSupportedRateMask;

    if (pFrameInfo->content.iePacket.pRates == NULL)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "updateRates, pRates=NULL, beacon & probeResp are: \n");
        return;
    }

    /* Update the rate elements */
    maxBasicRate = rate_GetMaxBasicFromStr ((TI_UINT8 *)pFrameInfo->content.iePacket.pRates->rates,pFrameInfo->content.iePacket.pRates->hdr[1], maxBasicRate);
    maxActiveRate = rate_GetMaxActiveFromStr ((TI_UINT8 *)pFrameInfo->content.iePacket.pRates->rates,pFrameInfo->content.iePacket.pRates->hdr[1], maxActiveRate);

    if(pFrameInfo->content.iePacket.pExtRates)
    {
        maxBasicRate = rate_GetMaxBasicFromStr ((TI_UINT8 *)pFrameInfo->content.iePacket.pExtRates->rates,pFrameInfo->content.iePacket.pExtRates->hdr[1], maxBasicRate);
        maxActiveRate = rate_GetMaxActiveFromStr ((TI_UINT8 *)pFrameInfo->content.iePacket.pExtRates->rates,pFrameInfo->content.iePacket.pExtRates->hdr[1], maxActiveRate);
    }


    /*
TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "1- maxBasicRate = 0x%X, maxActiveRate = 0x%X \n", maxBasicRate,maxActiveRate);
	*/

    if (maxActiveRate == 0)
        maxActiveRate = maxBasicRate;

    /* Now update it from network to host rates */
    pSite->maxBasicRate = rate_NetToDrv (maxBasicRate);
    pSite->maxActiveRate = rate_NetToDrv (maxActiveRate);

    /* for now we use constat MCS rate */
    if (pFrameInfo->content.iePacket.pHtInformation != NULL)
    {
        pSite->maxBasicRate = DRV_RATE_MCS_7;
        pSite->maxActiveRate = DRV_RATE_MCS_7;
    }

    if (pSite->maxActiveRate == DRV_RATE_INVALID)
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Network To Host Rate failure, no active network rate\n");

    if (pSite->maxBasicRate != DRV_RATE_INVALID)
    {
        if (pSite->maxActiveRate != DRV_RATE_INVALID)
        {
            pSite->maxActiveRate = TI_MAX (pSite->maxActiveRate, pSite->maxBasicRate);
        }
    } else { /* in case some vendors don't specify basic rates */
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_WARNING, "Network To Host Rate failure, no basic network rate");
        pSite->maxBasicRate = pSite->maxActiveRate;
    }

    /* build rates bit map */
    rate_NetStrToDrvBitmap (&pSite->rateMask.supportedRateMask,
                            pFrameInfo->content.iePacket.pRates->rates,
                            pFrameInfo->content.iePacket.pRates->hdr[1]);
    rate_NetBasicStrToDrvBitmap (&pSite->rateMask.basicRateMask,
                                 pFrameInfo->content.iePacket.pRates->rates,
                                 pFrameInfo->content.iePacket.pRates->hdr[1]);

    if(pFrameInfo->content.iePacket.pExtRates)
    {
        rate_NetStrToDrvBitmap (&bitMapExtSupp, 
                                pFrameInfo->content.iePacket.pExtRates->rates,
                                       pFrameInfo->content.iePacket.pExtRates->hdr[1]);

        pSite->rateMask.supportedRateMask |= bitMapExtSupp;

        rate_NetBasicStrToDrvBitmap (&bitMapExtSupp, 
                                     pFrameInfo->content.iePacket.pExtRates->rates,
                                        pFrameInfo->content.iePacket.pExtRates->hdr[1]);

        pSite->rateMask.basicRateMask |= bitMapExtSupp;
    }

    
    if (pFrameInfo->content.iePacket.pHtCapabilities != NULL)
    {
        /* MCS build rates bit map */
        rate_McsNetStrToDrvBitmap (&uMcsSupportedRateMask,
                                   (pFrameInfo->content.iePacket.pHtCapabilities->aHtCapabilitiesIe + DOT11_HT_CAPABILITIES_MCS_RATE_OFFSET));

        pSite->rateMask.supportedRateMask |= uMcsSupportedRateMask;
    }

    if (pFrameInfo->content.iePacket.pHtInformation != NULL)
    {
        /* MCS build rates bit map */
        rate_McsNetStrToDrvBitmap (&uMcsbasicRateMask,
                                   (pFrameInfo->content.iePacket.pHtInformation->aHtInformationIe + DOT11_HT_INFORMATION_MCS_RATE_OFFSET));

        pSite->rateMask.basicRateMask |= uMcsbasicRateMask;
    }


    param.paramType = CTRL_DATA_CURRENT_SUPPORTED_RATE_MASK_PARAM;
    param.content.ctrlDataCurrentRateMask = pSite->rateMask.supportedRateMask;
    /* clear the 22Mbps bit in case the PBCC is not allowed */
    if(pSiteMgr->currentDataModulation != DRV_MODULATION_PBCC && pSiteMgr->currentDataModulation != DRV_MODULATION_OFDM)
    {
      param.content.ctrlDataCurrentRateMask &= ~DRV_RATE_MASK_22_PBCC;
    }
    ctrlData_setParam(pSiteMgr->hCtrlData, &param);
}

/***********************************************************************
 *                        getPrimaryBssid
 ***********************************************************************
DESCRIPTION:    Called by the OS abstraction layer in order to get the BSSID list from the site table

INPUT:      pSiteMgr    -   site mgr handle.

OUTPUT:     bssidList   -   BSSID list pointer

RETURN:

************************************************************************/
static TI_STATUS getPrimaryBssid(siteMgr_t *pSiteMgr, OS_802_11_BSSID_EX *primaryBssid, TI_UINT32 *pLength)
{
    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
    TI_UINT32                  len, firstOFDMloc = 0;
    OS_802_11_FIXED_IEs     *pFixedIes;
    OS_802_11_VARIABLE_IEs  *pVarIes;
    TI_UINT32                  length;


    if (primaryBssid==NULL)
    {
        /* we do not expect to get NULL value for pLength here */
        *pLength = 0;
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "getPrimaryBssid. primaryBssid ptr is NULL\n");
        return TI_NOK;

    }

    if (pPrimarySite==NULL)
    {
        *pLength = 0;
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "getPrimaryBssid, pPrimarySite is NULL \n");
        return TI_NOK;

    }
    length = pPrimarySite->beaconLength + sizeof(OS_802_11_BSSID_EX) + sizeof(OS_802_11_FIXED_IEs);
    if (length > *pLength)
    {
        TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "getPrimaryBssid, insufficient length,  required length=%d, pLength=%d \n", length, *pLength);
        *pLength = length;

        return TI_NOK;
    }

    TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Entering getPrimaryBssid, length = %d\n", *pLength);

    primaryBssid->Length = length; 
    /* MacAddress */
    MAC_COPY (primaryBssid->MacAddress, pPrimarySite->bssid);
        
    /* Capabilities */
    primaryBssid->Capabilities = pPrimarySite->capabilities;

    /* SSID */
    os_memoryZero(pSiteMgr->hOs, primaryBssid->Ssid.Ssid, MAX_SSID_LEN);
    if (pPrimarySite->ssid.len > MAX_SSID_LEN)
    {
        pPrimarySite->ssid.len = MAX_SSID_LEN;
    }
    os_memoryCopy (pSiteMgr->hOs, 
                   (void *)primaryBssid->Ssid.Ssid, 
                   (void *)pPrimarySite->ssid.str, 
                   pPrimarySite->ssid.len);
    primaryBssid->Ssid.SsidLength = pPrimarySite->ssid.len;

    /* privacy */
    primaryBssid->Privacy = pPrimarySite->privacy;

    /* RSSI */
    primaryBssid->Rssi = pPrimarySite->rssi;

    /* NetworkTypeInUse & SupportedRates */
    /* SupportedRates */
    os_memoryZero(pSiteMgr->hOs, (void *)primaryBssid->SupportedRates, sizeof(OS_802_11_RATES_EX));

    rate_DrvBitmapToNetStr (pPrimarySite->rateMask.supportedRateMask,
                            pPrimarySite->rateMask.basicRateMask,
                            (TI_UINT8 *)primaryBssid->SupportedRates,
                            &len, 
                            &firstOFDMloc);

    /* set network type acording to band and rates */
    if (pPrimarySite->channel <= SITE_MGR_CHANNEL_B_G_MAX)
    {
        if (firstOFDMloc == len)
        {
            primaryBssid->NetworkTypeInUse = os802_11DS;
        } else {
            primaryBssid->NetworkTypeInUse = os802_11OFDM24;
        }
    } else {
        primaryBssid->NetworkTypeInUse = os802_11OFDM5;
    }

    /* Configuration */
    primaryBssid->Configuration.Length = sizeof(OS_802_11_CONFIGURATION);
    primaryBssid->Configuration.BeaconPeriod = pPrimarySite->beaconInterval;
    primaryBssid->Configuration.ATIMWindow = pPrimarySite->atimWindow;
    primaryBssid->Configuration.Union.channel = Chan2Freq(pPrimarySite->channel);

    /* InfrastructureMode */
    if  (pPrimarySite->bssType == BSS_INDEPENDENT)
        primaryBssid->InfrastructureMode = os802_11IBSS;
    else
        primaryBssid->InfrastructureMode = os802_11Infrastructure;
    primaryBssid->IELength = 0;

    /* copy fixed IEs from site entry */
    pFixedIes = (OS_802_11_FIXED_IEs*)&primaryBssid->IEs[primaryBssid->IELength];
    os_memoryCopy(pSiteMgr->hOs, (void *)pFixedIes->TimeStamp, (void *)&pPrimarySite->tsfTimeStamp, TIME_STAMP_LEN);
    pFixedIes->BeaconInterval = pPrimarySite->beaconInterval;
    pFixedIes->Capabilities = pPrimarySite->capabilities;
    primaryBssid->IELength += sizeof(OS_802_11_FIXED_IEs);
    pVarIes = (OS_802_11_VARIABLE_IEs*)&primaryBssid->IEs[primaryBssid->IELength];

    /* Copy all variable IEs */
    TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Copy all variable beaconLength=%d, IELength=%d\n", pPrimarySite->beaconLength, primaryBssid->IELength);
    TRACE_INFO_HEX(pSiteMgr->hReport, pPrimarySite->beaconBuffer, pPrimarySite->beaconLength);
    /* It looks like it never happens. Anyway decided to check */
    if ( pPrimarySite->beaconLength > MAX_BEACON_BODY_LENGTH )
    {
        TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_ERROR,
              "getPrimaryBssid. pPrimarySite->beaconLength=%d exceeds the limit %d\n",
              pPrimarySite->beaconLength, MAX_BEACON_BODY_LENGTH);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return TI_NOK;
    }
    os_memoryCopy(pSiteMgr->hOs, pVarIes, pPrimarySite->beaconBuffer, pPrimarySite->beaconLength);

    primaryBssid->IELength += pPrimarySite->beaconLength;


    primaryBssid->Length = sizeof(OS_802_11_BSSID_EX) + primaryBssid->IELength - 1;

    TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "BSSID MAC = %x-%x-%x-%x-%x-%x\n", primaryBssid->MacAddress[0], primaryBssid->MacAddress[1], primaryBssid->MacAddress[2], primaryBssid->MacAddress[3], primaryBssid->MacAddress[4], primaryBssid->MacAddress[5]);


    TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "primaryBssid is\n");
    TRACE_INFO_HEX(pSiteMgr->hReport, (TI_UINT8*)primaryBssid, primaryBssid->Length);


    TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Exiting getBssidList, length =%d, IELength=%d \n", primaryBssid->Length, primaryBssid->IELength);

    *pLength = primaryBssid->Length;

    return TI_OK;
}

#ifdef REPORT_LOG
/***********************************************************************
 *                        siteMgr_printPrimarySiteDesc
 ***********************************************************************
DESCRIPTION:    Called by the OS abstraction layer in order to get the primary site description

INPUT:      pSiteMgr            -   site mgr handle.
            supplyExtendedInfo  - If OS_802_11_BSSID_EX structure should be used (extended info)
                                  (Assuming that if this function is called with TI_TRUE, enough memory was allocated to hold the extended info)

OUTPUT:     pPrimarySiteDesc    -   Primary site description pointer

RETURN:

************************************************************************/
void siteMgr_printPrimarySiteDesc(TI_HANDLE hSiteMgr )
{
    siteMgr_t *pSiteMgr = (siteMgr_t*) hSiteMgr;

    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;

    /* the driver logger can't print %s
     * TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_CONSOLE, "-- SSID  = %s \n",pPrimarySite->ssid.str); 
     */
    TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_CONSOLE,"-- BSSID = %02x-%02x-%02x-%02x-%02x-%02x\n",
                    pPrimarySite->bssid[0], pPrimarySite->bssid[1], pPrimarySite->bssid[2], pPrimarySite->bssid[3], 
                    pPrimarySite->bssid[4], pPrimarySite->bssid[5]);


	WLAN_OS_REPORT(("-- SSID  = %s \n",pPrimarySite->ssid.str));
	WLAN_OS_REPORT(("-- BSSID = %02x-%02x-%02x-%02x-%02x-%02x\n",
					pPrimarySite->bssid[0], pPrimarySite->bssid[1], pPrimarySite->bssid[2], pPrimarySite->bssid[3], 
					pPrimarySite->bssid[4], pPrimarySite->bssid[5]));
}
#endif

/***********************************************************************
 *                        getPrimarySiteDesc
 ***********************************************************************
DESCRIPTION:    Called by the OS abstraction layer in order to get the primary site description

INPUT:      pSiteMgr            - site mgr handle.
            supplyExtendedInfo  - If OS_802_11_BSSID_EX structure should be used (extended info)
                                  (Assuming that if this function is called with TI_TRUE, enough memory was allocated to hold the extended info)

OUTPUT:     pPrimarySiteDesc    - Primary site description pointer

RETURN:

************************************************************************/
static void getPrimarySiteDesc(siteMgr_t *pSiteMgr, OS_802_11_BSSID *pPrimarySiteDesc, TI_BOOL supplyExtendedInfo)
{
    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
    OS_802_11_BSSID_EX *pExPrimarySiteDesc = (OS_802_11_BSSID_EX *) pPrimarySiteDesc;
    TI_UINT32  len, firstOFDMloc = 0;
    OS_802_11_FIXED_IEs     *pFixedIes;
    OS_802_11_VARIABLE_IEs  *pVarIes;
    TI_UINT8 rsnIeLength,index;
    OS_802_11_RATES_EX  SupportedRates;


    if (pPrimarySiteDesc == NULL)
    {
        return;
    }
    if (pPrimarySite == NULL)
    {
        os_memoryZero(pSiteMgr->hOs, pPrimarySiteDesc, sizeof(OS_802_11_BSSID));
        return;
    }

    TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "getPrimarySiteDesc - enter\n");

    
    /* If an "extended" request has been made - update the length accordingly */
   if (supplyExtendedInfo == TI_FALSE)
    pPrimarySiteDesc->Length = sizeof(OS_802_11_BSSID);
   else
    pPrimarySiteDesc->Length = sizeof(OS_802_11_BSSID_EX);

    /* MacAddress */
    MAC_COPY (pPrimarySiteDesc->MacAddress, pPrimarySite->bssid);

    /* Capabilities */
    pPrimarySiteDesc->Capabilities = pPrimarySite->capabilities;

    /* It looks like it never happens. Anyway decided to check */
    if ( pPrimarySite->ssid.len > MAX_SSID_LEN )
    {
        TRACE2( pSiteMgr->hReport, REPORT_SEVERITY_ERROR,
               "getPrimarySiteDesc. pPrimarySite=%d exceeds the limit %d\n",
                   pPrimarySite->ssid.len, MAX_SSID_LEN);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* SSID */
    os_memoryCopy (pSiteMgr->hOs, 
                   (void *)pPrimarySiteDesc->Ssid.Ssid, 
                   (void *)pPrimarySite->ssid.str, 
                   pPrimarySite->ssid.len);
    pPrimarySiteDesc->Ssid.SsidLength = pPrimarySite->ssid.len;

    /* privacy */
    pPrimarySiteDesc->Privacy = pPrimarySite->privacy;

    /* RSSI */

    pPrimarySiteDesc->Rssi = pPrimarySite->rssi;

    pPrimarySiteDesc->NetworkTypeInUse = os802_11DS;

    pPrimarySiteDesc->Configuration.Length = sizeof(OS_802_11_CONFIGURATION);
    pPrimarySiteDesc->Configuration.BeaconPeriod = pPrimarySite->beaconInterval;
    pPrimarySiteDesc->Configuration.ATIMWindow = pPrimarySite->atimWindow;
    pPrimarySiteDesc->Configuration.Union.channel = pPrimarySite->channel;

    if  (pPrimarySite->bssType == BSS_INDEPENDENT)
        pPrimarySiteDesc->InfrastructureMode = os802_11IBSS;
    else
        pPrimarySiteDesc->InfrastructureMode = os802_11Infrastructure;

   /* SupportedRates */
   if (supplyExtendedInfo == TI_FALSE)
      os_memoryZero(pSiteMgr->hOs, (void *)pPrimarySiteDesc->SupportedRates, sizeof(OS_802_11_RATES));
   else
      os_memoryZero(pSiteMgr->hOs, (void *)pExPrimarySiteDesc->SupportedRates, sizeof(OS_802_11_RATES_EX));

   rate_DrvBitmapToNetStr (pPrimarySite->rateMask.supportedRateMask,
                           pPrimarySite->rateMask.basicRateMask,
                           &SupportedRates[0],
                           &len, 
                           &firstOFDMloc);

   if (supplyExtendedInfo == TI_FALSE)
       os_memoryCopy(pSiteMgr->hOs, (void *)pPrimarySiteDesc->SupportedRates, (void *)SupportedRates, sizeof(OS_802_11_RATES));
   else
       os_memoryCopy(pSiteMgr->hOs, (void *)pExPrimarySiteDesc->SupportedRates, (void *)SupportedRates, sizeof(OS_802_11_RATES_EX));


   if (supplyExtendedInfo == TI_TRUE)
   {
      pExPrimarySiteDesc->IELength = 0;

     /* copy fixed IEs from site entry */
     pFixedIes = (OS_802_11_FIXED_IEs*)&pExPrimarySiteDesc->IEs[pExPrimarySiteDesc->IELength];
     os_memoryCopy(pSiteMgr->hOs, (void *)pFixedIes->TimeStamp, (void *)&pPrimarySite->tsfTimeStamp, TIME_STAMP_LEN);
     pFixedIes->BeaconInterval = pPrimarySite->beaconInterval;
     pFixedIes->Capabilities = pPrimarySite->capabilities;
     pExPrimarySiteDesc->IELength += sizeof(OS_802_11_FIXED_IEs);

     /* copy variable IEs */
     /* copy SSID */
     pVarIes = (OS_802_11_VARIABLE_IEs*)&pExPrimarySiteDesc->IEs[pExPrimarySiteDesc->IELength];
     pVarIes->ElementID = SSID_IE_ID;
     pVarIes->Length = pPrimarySite->ssid.len;
     os_memoryCopy (pSiteMgr->hOs, 
                    (void *)pVarIes->data, 
                    (void *)pPrimarySite->ssid.str, 
                    pPrimarySite->ssid.len);
     pExPrimarySiteDesc->IELength += (pVarIes->Length + 2);

     
     /* copy RSN information elements */
     rsnIeLength = 0;
     for (index=0; index<MAX_RSN_IE && pPrimarySite->pRsnIe[index].hdr[1] > 0; index++)
       {
            pVarIes =  (OS_802_11_VARIABLE_IEs*)&pExPrimarySiteDesc->IEs[pExPrimarySiteDesc->IELength+rsnIeLength];
         pVarIes->ElementID = pPrimarySite->pRsnIe[index].hdr[0];
             pVarIes->Length = pPrimarySite->pRsnIe[index].hdr[1];
             os_memoryCopy(pSiteMgr->hOs, (void *)pVarIes->data, (void *)pPrimarySite->pRsnIe[index].rsnIeData, pPrimarySite->pRsnIe[index].hdr[1]);
             rsnIeLength += pPrimarySite->pRsnIe[index].hdr[1] + 2;
            TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "RSN IE ID=%d, Length=%x\n", pVarIes->ElementID, pVarIes->Length);

            TRACE_INFO_HEX(pSiteMgr->hReport, (TI_UINT8 *)pVarIes->data,pVarIes->Length);
         }

         pExPrimarySiteDesc->IELength += pPrimarySite->rsnIeLen;

         pExPrimarySiteDesc->Length = sizeof(OS_802_11_BSSID_EX) + pExPrimarySiteDesc->IELength - 1;

         TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "BSSID MAC = %x-%x-%x-%x-%x-%x\n", pExPrimarySiteDesc->MacAddress[0], pExPrimarySiteDesc->MacAddress[1], pExPrimarySiteDesc->MacAddress[2], pExPrimarySiteDesc->MacAddress[3], pExPrimarySiteDesc->MacAddress[4], pExPrimarySiteDesc->MacAddress[5]);

         TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "pExPrimarySiteDesc length before alignment = %d\n", pExPrimarySiteDesc->Length);

         /* make sure length is 4 bytes aligned */
         if (pExPrimarySiteDesc->Length % 4)
         {
            pExPrimarySiteDesc->Length += (4 - (pExPrimarySiteDesc->Length % 4));
         }

         TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "pExPrimarySiteDesc length after alignment = %d\n", pExPrimarySiteDesc->Length);

        }

}


/***********************************************************************
 *                        translateRateMaskToValue
 ***********************************************************************
DESCRIPTION:    Called by the function 'siteMgr_config()' in order to translate the rate mask read
                from registry to rate value

INPUT:      pSiteMgr            -   site mgr handle.
            rateMask            -   Rate mask

OUTPUT:     The rate after the translation

RETURN:

************************************************************************/
static ERate translateRateMaskToValue(siteMgr_t *pSiteMgr, TI_UINT32 rateMask)
{
	if (rateMask & DRV_RATE_MASK_MCS_7_OFDM)
        return DRV_RATE_MCS_7;
    if (rateMask & DRV_RATE_MASK_MCS_6_OFDM)
        return DRV_RATE_MCS_6;
    if (rateMask & DRV_RATE_MASK_MCS_5_OFDM)
        return DRV_RATE_MCS_5;
    if (rateMask & DRV_RATE_MASK_MCS_4_OFDM)
        return DRV_RATE_MCS_4;
    if (rateMask & DRV_RATE_MASK_MCS_3_OFDM)
        return DRV_RATE_MCS_3;
    if (rateMask & DRV_RATE_MASK_MCS_2_OFDM)
        return DRV_RATE_MCS_2;
    if (rateMask & DRV_RATE_MASK_MCS_1_OFDM)
        return DRV_RATE_MCS_1;
    if (rateMask & DRV_RATE_MASK_MCS_0_OFDM)
        return DRV_RATE_MCS_0;
    if (rateMask & DRV_RATE_MASK_54_OFDM)
        return DRV_RATE_54M;
    if (rateMask & DRV_RATE_MASK_48_OFDM)
        return DRV_RATE_48M;
    if (rateMask & DRV_RATE_MASK_36_OFDM)
        return DRV_RATE_36M;
    if (rateMask & DRV_RATE_MASK_24_OFDM)
        return DRV_RATE_24M;
    if (rateMask & DRV_RATE_MASK_22_PBCC)
        return DRV_RATE_22M;
    if (rateMask & DRV_RATE_MASK_18_OFDM)
        return DRV_RATE_18M;
    if (rateMask & DRV_RATE_MASK_12_OFDM)
        return DRV_RATE_12M;
    if (rateMask & DRV_RATE_MASK_11_CCK)
        return DRV_RATE_11M;
    if (rateMask & DRV_RATE_MASK_9_OFDM)
        return DRV_RATE_9M;
    if (rateMask & DRV_RATE_MASK_6_OFDM)
        return DRV_RATE_6M;
    if (rateMask & DRV_RATE_MASK_5_5_CCK)
        return DRV_RATE_5_5M;
    if (rateMask & DRV_RATE_MASK_2_BARKER)
        return DRV_RATE_2M;
    if (rateMask & DRV_RATE_MASK_1_BARKER)
        return DRV_RATE_1M;

    TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_WARNING, "Translate rate mask to value, mask is 0x%X\n", rateMask);
    if(pSiteMgr->siteMgrOperationalMode != DOT11_A_MODE)
        return DRV_RATE_1M;
    else
        return DRV_RATE_6M;
}


/***********************************************************************
 *                        getSupportedRateSet
 ***********************************************************************
DESCRIPTION:    Called by the function 'siteMgr_getParam()' in order to get the supported rate set
                Build an array of network rates based on the max active & max basic rates

INPUT:      pSiteMgr            -   site mgr handle.

OUTPUT:     pRatesSet           -   The array built

RETURN:

************************************************************************/
static void getSupportedRateSet(siteMgr_t *pSiteMgr, TRates *pRatesSet)
{
    TI_UINT32 dontCareParam;
    TI_UINT32 len = 0;

    rate_DrvBitmapToNetStr (pSiteMgr->pDesiredParams->siteMgrRegstrySuppRateMask,
                               pSiteMgr->pDesiredParams->siteMgrRegstryBasicRateMask,
                            (TI_UINT8 *)pRatesSet->ratesString, 
                            &len, 
                            &dontCareParam);

    pRatesSet->len = (TI_UINT8) len;
}

/***********************************************************************
 *                        setSupportedRateSet
 ***********************************************************************
DESCRIPTION:    Called by the function 'siteMgr_setParam()' in order to set the supported rate set
                Go over the input array and set the max active & max basic rates. (after translation from network
                rates to host rates ofcourse)
                If max basic rate is bigger than the mac active one, print an error.
                If the basic or active rate are different than the ones already stored by the site manager,
                it initiates a reconnection by calling sme_Restart

INPUT:      pSiteMgr            -   site mgr handle.
            pRatesSet           -   The rates array received

OUTPUT:

RETURN:     TI_OK on success

************************************************************************/
static TI_STATUS setSupportedRateSet(siteMgr_t *pSiteMgr, TRates *pRatesSet)
{
    TI_UINT8  i,j, drvRate;
    ERate maxActiveRate = (ERate)0, maxBasicRate = (ERate)0;
    TI_UINT32 suppBitMap, basicBitMap;
    static  ERate basicRates_G[]  = {DRV_RATE_1M,DRV_RATE_2M,DRV_RATE_5_5M,DRV_RATE_11M};
    static ERate basicRates_A[]  = {DRV_RATE_6M,DRV_RATE_12M,DRV_RATE_24M};
    ERate* currentBasicRates;
    TI_UINT32 currentBasicRatesLength;

#ifndef NET_BASIC_MASK
#define NET_BASIC_MASK      0x80    /* defined in common/src/utils/utils.c */
#endif

    if(pSiteMgr->siteMgrOperationalMode == DOT11_A_MODE)
    {
        currentBasicRates = &basicRates_A[0];
        currentBasicRatesLength = sizeof(basicRates_A) / sizeof(basicRates_A[0]);
    }
    else
    {
        currentBasicRates = &basicRates_G[0];
        currentBasicRatesLength = sizeof(basicRates_G) / sizeof(basicRates_G[0]);
    }
    /* It looks like it never happens. Anyway decided to check */
    if ( pRatesSet->len > DOT11_MAX_SUPPORTED_RATES )
    {
        TRACE2( pSiteMgr->hReport, REPORT_SEVERITY_ERROR,
                "setSupportedRateSet. pRatesSet->len=%d exceeds the limit %d\n",
                pRatesSet->len, DOT11_MAX_SUPPORTED_RATES);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        pRatesSet->len = DOT11_MAX_SUPPORTED_RATES;
    }

    /* Basic rates must be supported. If one of 1M,2M,5.5M,11M is not supported fail.*/
    for (j = 0; j < currentBasicRatesLength; j++) 
    {
        for (i = 0; i < pRatesSet->len; i++)
        {
            drvRate = rate_NetToDrv (pRatesSet->ratesString[i]);
            if ((drvRate & ( NET_BASIC_MASK-1)) == currentBasicRates[j])
                break;
        }
        /* not all the basic rates are supported! Failure*/
        if (i == pRatesSet->len) 
        {
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Rates set must contain the basic set! Failing\n");
            return PARAM_VALUE_NOT_VALID;
        }
    }
    
    for (i = 0; i < pRatesSet->len; i++)
    {
        drvRate = rate_NetToDrv (pRatesSet->ratesString[i]);
        if(pSiteMgr->siteMgrOperationalMode == DOT11_A_MODE)
        {
            if(drvRate < DRV_RATE_6M)
            {
                TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Notice, the driver configured in 11a mode, but CCK rate appears\n");
                return PARAM_VALUE_NOT_VALID;
            }
        }
        else if(pSiteMgr->siteMgrOperationalMode == DOT11_B_MODE)
        {
            if(drvRate >= DRV_RATE_6M)
            {
                TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Notice, the driver configured in 11b mode, but OFDM rate appears\n");
                return PARAM_VALUE_NOT_VALID;
            }
        }
        maxActiveRate = TI_MAX ((ERate)drvRate, maxActiveRate);
    }

    for (i = 0; i < pRatesSet->len; i++)
    {
        if (NET_BASIC_RATE(pRatesSet->ratesString[i]))
            maxBasicRate = TI_MAX (rate_NetToDrv (pRatesSet->ratesString[i]), maxBasicRate);
    }

    /* If the basic rate is bigger than the supported one, we print an error */
    if (pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxBasic > pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxActive)
    {
        TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "Notice, the rates configuration is invalid, basic rate is bigger than supported, Max Basic: %d Max Supported: %d\n", pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxBasic, pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxActive);
        return PARAM_VALUE_NOT_VALID;
    }

    pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxActive = maxActiveRate;
    pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxBasic = maxBasicRate;

    /* configure desired modulation */
    if(maxActiveRate == DRV_RATE_22M)
        pSiteMgr->pDesiredParams->siteMgrDesiredModulationType = DRV_MODULATION_PBCC;
    else if(maxActiveRate < DRV_RATE_22M)
        pSiteMgr->pDesiredParams->siteMgrDesiredModulationType = DRV_MODULATION_CCK;
    else
        pSiteMgr->pDesiredParams->siteMgrDesiredModulationType = DRV_MODULATION_OFDM;


    rate_NetStrToDrvBitmap (&suppBitMap, pRatesSet->ratesString, pRatesSet->len);
    rate_NetBasicStrToDrvBitmap (&basicBitMap, pRatesSet->ratesString, pRatesSet->len);

    if((pSiteMgr->pDesiredParams->siteMgrRegstryBasicRateMask != basicBitMap) ||
       (pSiteMgr->pDesiredParams->siteMgrRegstrySuppRateMask != suppBitMap))
    {
        pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask = suppBitMap;
        pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask = basicBitMap;
        pSiteMgr->pDesiredParams->siteMgrRegstryBasicRateMask = basicBitMap;
        pSiteMgr->pDesiredParams->siteMgrRegstrySuppRateMask = suppBitMap;
        /* Initialize Mutual Rates Matching */
        pSiteMgr->pDesiredParams->siteMgrMatchedBasicRateMask = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask;
        pSiteMgr->pDesiredParams->siteMgrMatchedSuppRateMask = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask;

        sme_Restart (pSiteMgr->hSmeSm);
    }

    return TI_OK;
}

/***********************************************************************
 *                        pbccAlgorithm
 ***********************************************************************
DESCRIPTION:    Called by the following functions:
                -   systemConfig(), in the system configuration phase after the selection
                -   siteMgr_updateSite(), in a case of a primary site update & if a PBCC algorithm
                    is needed to be performed
                Performs the PBCC algorithm


INPUT:      hSiteMgr            -   site mgr handle.

OUTPUT:

RETURN:     TI_OK on always

************************************************************************/
TI_STATUS pbccAlgorithm(TI_HANDLE hSiteMgr)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    paramInfo_t param;
    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
    TI_UINT32   supportedRateMask ;


    if (pPrimarySite->channel == SPECIAL_BG_CHANNEL)
        supportedRateMask = (rate_GetDrvBitmapForDefaultSupporteSet() & pSiteMgr->pDesiredParams->siteMgrMatchedSuppRateMask) ;
    else
        supportedRateMask = pSiteMgr->pDesiredParams->siteMgrMatchedSuppRateMask ;

    param.paramType = CTRL_DATA_CURRENT_SUPPORTED_RATE_MASK_PARAM;
    param.content.ctrlDataCurrentRateMask = supportedRateMask;
    /* clear the 22Mbps bit in case the PBCC is not allowed */
    if(pSiteMgr->currentDataModulation != DRV_MODULATION_PBCC &&
       pSiteMgr->currentDataModulation != DRV_MODULATION_OFDM)
            param.content.ctrlDataCurrentRateMask &= ~DRV_RATE_MASK_22_PBCC;
    ctrlData_setParam(pSiteMgr->hCtrlData, &param);

    param.paramType = CTRL_DATA_RATE_CONTROL_ENABLE_PARAM;
    ctrlData_setParam(pSiteMgr->hCtrlData, &param);

    return TI_OK;
}


/***********************************************************************
 *                        siteMgr_assocReport
 ***********************************************************************
DESCRIPTION:    Called by the following functions:
                -   assoc_recv()


INPUT:      hSiteMgr            -   siteMgr handle.
            capabilities        -   assoc rsp capabilities field.
			bCiscoAP			-   whether we are connected to a Cisco AP. Used for Tx Power Control adjustment
OUTPUT:

RETURN:     TI_OK on always

************************************************************************/
TI_STATUS siteMgr_assocReport(TI_HANDLE hSiteMgr, TI_UINT16 capabilities, TI_BOOL bCiscoAP)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;

    /* handle AP's preamble capability */
    pPrimarySite->preambleAssRspCap = ((capabilities >> CAP_PREAMBLE_SHIFT) & CAP_PREAMBLE_MASK)  ? PREAMBLE_SHORT : PREAMBLE_LONG;

	/*
	 * Enable/Disable the Tx Power Control adjustment.
	 * When we are connected to Cisco AP - TX Power Control adjustment is disabled.
	 */
	pSiteMgr->siteMgrTxPowerEnabled = ( !bCiscoAP ) && ( pSiteMgr->pDesiredParams->TxPowerControlOn );

TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Tx Power Control adjustment is %d\n", pSiteMgr->siteMgrTxPowerEnabled);

    return TI_OK;
}

/***********************************************************************
 *                        siteMgr_setWMEParamsSite
 ***********************************************************************
DESCRIPTION:    Set the QOS_WME params as received from the associated
                AP. The function is called by the QoS Mgr
                after receving association response succefully.

INPUT:      hSiteMgr    -   SiteMgr handle.

OUTPUT:     pDot11_WME_PARAM_t - pointer to the QOS_WME Param IE.

RETURN:     TI_OK on always

************************************************************************/
TI_STATUS siteMgr_setWMEParamsSite(TI_HANDLE hSiteMgr,dot11_WME_PARAM_t *pDot11_WME_PARAM)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;

    if( pPrimarySite == NULL )
    {
        return TI_OK;
    }

    if( pDot11_WME_PARAM == NULL )
    {
        pPrimarySite->WMESupported = TI_FALSE;
        return TI_OK;
    }

    /* Update the QOS_WME params */
    os_memoryCopy(pSiteMgr->hOs,&pPrimarySite->WMEParameters,&pDot11_WME_PARAM->WME_ACParameteres,sizeof(dot11_ACParameters_t));
    pPrimarySite->lastWMEParameterCnt = (pDot11_WME_PARAM->ACInfoField & dot11_WME_ACINFO_MASK);
    TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "$$$$$$ QOS_WME parameters were updates according to association response, cntSeq = %d\n",pPrimarySite->lastWMEParameterCnt);

    return TI_OK;
}


/***********************************************************************
 *                        siteMgr_getWMEParamsSite
 ***********************************************************************
DESCRIPTION:    Get the QOS_WME params as recieved from the associated
                AP. The function is called by the Qos Mgr in order
                to set all QOS_WME parameters to the core and Hal

INPUT:      hSiteMgr    -   SiteMgr handle.

OUTPUT:     pWME_ACParameters_t - pointer to the QOS_WME Param.

RETURN:     TI_OK if there are valid QOS_WME parameters , TI_NOK otherwise.

************************************************************************/
TI_STATUS siteMgr_getWMEParamsSite(TI_HANDLE hSiteMgr,dot11_ACParameters_t **pWME_ACParameters_t)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;

    if(pPrimarySite->WMESupported == TI_TRUE)
    {
        *pWME_ACParameters_t = &pPrimarySite->WMEParameters;
        return TI_OK;
    }
    else
    {
        *pWME_ACParameters_t = NULL;
        return TI_NOK;
    }

}

/***********************************************************************
 *                        siteMgr_setCurrentTable
 ***********************************************************************
DESCRIPTION:

INPUT:      hSiteMgr    -   SiteMgr handle.

OUTPUT:

RETURN:

************************************************************************/
void siteMgr_setCurrentTable(TI_HANDLE hSiteMgr, ERadioBand radioBand)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;

    if(radioBand == RADIO_BAND_2_4_GHZ)
        pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
    else
        pSiteMgr->pSitesMgmtParams->pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
}

/***********************************************************************
 *                        siteMgr_updateRates
 ***********************************************************************
DESCRIPTION:

INPUT:      hSiteMgr    -   SiteMgr handle.

OUTPUT:

RETURN:

************************************************************************/

void siteMgr_updateRates(TI_HANDLE hSiteMgr, TI_BOOL dot11a, TI_BOOL updateToOS)
{
    TI_UINT32  statusData;
    TI_UINT32  localSuppRateMask, localBasicRateMask;

    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;

    localSuppRateMask = pSiteMgr->pDesiredParams->siteMgrRegstrySuppRateMask;
    localBasicRateMask = pSiteMgr->pDesiredParams->siteMgrRegstryBasicRateMask;


    rate_ValidateVsBand (&localSuppRateMask, &localBasicRateMask, dot11a);

    pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask = localBasicRateMask;
    pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask = localSuppRateMask;

     /* Initialize Mutual Rates Matching */
    pSiteMgr->pDesiredParams->siteMgrMatchedBasicRateMask = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask;
    pSiteMgr->pDesiredParams->siteMgrMatchedSuppRateMask = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask;

    /*If we are in dual mode and we are only scanning A band we don't have to set the siteMgrCurrentDesiredTxRate.*/
    if (updateToOS == TI_TRUE)
    {
        TI_UINT32 uSupportedRates = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask;
        ERate eMaxRate = rate_GetMaxFromDrvBitmap (uSupportedRates);

        /* Make sure that the basic rate set is included in the supported rate set */
        pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask &= uSupportedRates;

        /* Set desired modulation */
        pSiteMgr->pDesiredParams->siteMgrDesiredModulationType = 
            (eMaxRate < DRV_RATE_22M) ? DRV_MODULATION_CCK : DRV_MODULATION_OFDM;
    }
    pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxBasic = translateRateMaskToValue(pSiteMgr, pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask);
    pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxActive = translateRateMaskToValue(pSiteMgr, pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask);

    if (updateToOS == TI_TRUE) 
	{
        siteEntry_t  *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
        TI_UINT32 commonSupportedRateMask;
        ERate maxRate;

        commonSupportedRateMask = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask;
        if (pPrimarySite)
        {
            commonSupportedRateMask &= pPrimarySite->rateMask.supportedRateMask;
        }

        maxRate = translateRateMaskToValue(pSiteMgr, commonSupportedRateMask);

        /* report the desired rate to OS */
        statusData = rate_DrvToNet(maxRate);
        EvHandlerSendEvent(pSiteMgr->hEvHandler, IPC_EVENT_LINK_SPEED, (TI_UINT8 *)&statusData,sizeof(TI_UINT32));
    }
}


/** 
 * \fn     siteMgr_SelectRateMatch
 * \brief  Checks if the rates settings match those of a site
 * 
 * Checks if the rates settings match those of a site
 * 
 * \param  hSiteMgr - handle to the siteMgr object
 * \param  pCurrentSite - the site to check
 * \return TI_TRUE if site matches rates settings, TI FALSE if it doesn't
 * \sa     sme_Select
 */ 
TI_BOOL siteMgr_SelectRateMatch (TI_HANDLE hSiteMgr, TSiteEntry *pCurrentSite)
{
    siteMgr_t *pSiteMgr = (siteMgr_t *)hSiteMgr;
	TI_UINT32 StaTotalRates;
	TI_UINT32 SiteTotalRates;

	/* If the basic or active rate are invalid (0), return NO_MATCH. */
	if ((pCurrentSite->maxBasicRate == DRV_RATE_INVALID) || (pCurrentSite->maxActiveRate == DRV_RATE_INVALID)) 
	{
        TRACE2(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_SelectRateMatch: basic or active rate are invalid. maxBasic=%d,maxActive=%d \n", pCurrentSite->maxBasicRate ,pCurrentSite->maxActiveRate);
		return TI_FALSE;
	}
	
	if (DRV_RATE_MAX < pCurrentSite->maxBasicRate)
	{
        TRACE1(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_SelectRateMatch: basic rate is too high. maxBasic=%d\n", pCurrentSite->maxBasicRate);
		return TI_FALSE;
	}

	if(pCurrentSite->channel <= SITE_MGR_CHANNEL_B_G_MAX)
		siteMgr_updateRates(pSiteMgr, TI_FALSE, TI_TRUE);
	else
		siteMgr_updateRates(pSiteMgr, TI_TRUE, TI_TRUE);

	StaTotalRates = pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask |
				    pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask;

	SiteTotalRates = pCurrentSite->rateMask.basicRateMask | pCurrentSite->rateMask.supportedRateMask;
    
    if ((StaTotalRates & pCurrentSite->rateMask.basicRateMask) != pCurrentSite->rateMask.basicRateMask)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_SelectRateMatch: Basic or Supported Rates Doesn't Match \n");
        return TI_FALSE;

    }

    return TI_TRUE;
}

/***********************************************************************
 *                        siteMgr_bandParamsConfig
 ***********************************************************************
DESCRIPTION:

INPUT:      hSiteMgr    -   SiteMgr handle.

OUTPUT:

RETURN:

************************************************************************/
void siteMgr_bandParamsConfig(TI_HANDLE hSiteMgr,  TI_BOOL updateToOS)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;

    /* reconfig rates */
    if(pSiteMgr->siteMgrOperationalMode == DOT11_A_MODE)
        siteMgr_updateRates(hSiteMgr, TI_TRUE, updateToOS);
    else
        siteMgr_updateRates(hSiteMgr, TI_FALSE, updateToOS);

    /* go to B_ONLY Mode only if WiFI bit is Set*/
    if (pSiteMgr->pDesiredParams->siteMgrWiFiAdhoc == TI_TRUE)
    {   /* Configuration For AdHoc when using external configuration */
        if (pSiteMgr->pDesiredParams->siteMgrExternalConfiguration == TI_FALSE)
        {
            siteMgr_externalConfigurationParametersSet(hSiteMgr);
        }
    }

}

void siteMgr_ConfigRate(TI_HANDLE hSiteMgr)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    TI_BOOL      dot11a, b11nEnable;
    EDot11Mode   OperationMode;

    OperationMode = pSiteMgr->siteMgrOperationalMode;

    /* It looks like it never happens. Anyway decided to check */
    if ( OperationMode > DOT11_MAX_MODE -1 )
    {
        TRACE2( pSiteMgr->hReport, REPORT_SEVERITY_ERROR,
                "siteMgr_ConfigRate. OperationMode=%d exceeds the limit %d\n",
                OperationMode, DOT11_MAX_MODE -1);
                handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* reconfig rates */
    if (OperationMode == DOT11_A_MODE)
        dot11a = TI_TRUE;
    else
        dot11a = TI_FALSE;

    /*
    ** Specific change to ch 14, that channel is only used in Japan, and is limited
    ** to rates 1,2,5.5,11
    */
    if(pSiteMgr->pDesiredParams->siteMgrDesiredChannel == SPECIAL_BG_CHANNEL)
    {
        if(pSiteMgr->pDesiredParams->siteMgrRegstryBasicRate[OperationMode] > BASIC_RATE_SET_1_2_5_5_11)
            pSiteMgr->pDesiredParams->siteMgrRegstryBasicRate[OperationMode] = BASIC_RATE_SET_1_2_5_5_11;


        if(pSiteMgr->pDesiredParams->siteMgrRegstrySuppRate[OperationMode] > SUPPORTED_RATE_SET_1_2_5_5_11)
            pSiteMgr->pDesiredParams->siteMgrRegstrySuppRate[OperationMode] = SUPPORTED_RATE_SET_1_2_5_5_11;
    }

    /* use HT MCS rates */
    if (pSiteMgr->pDesiredParams->siteMgrDesiredBSSType == BSS_INFRASTRUCTURE)
	{
		b11nEnable = TI_TRUE;
		OperationMode = DOT11_N_MODE;
	}
	else
	{
		b11nEnable = TI_FALSE;
	}
    

    pSiteMgr->pDesiredParams->siteMgrRegstryBasicRateMask =
        rate_BasicToDrvBitmap ((EBasicRateSet)(pSiteMgr->pDesiredParams->siteMgrRegstryBasicRate[OperationMode]), dot11a);

    pSiteMgr->pDesiredParams->siteMgrRegstrySuppRateMask =
        rate_SupportedToDrvBitmap ((EBasicRateSet)(pSiteMgr->pDesiredParams->siteMgrRegstrySuppRate[OperationMode]), dot11a);

    siteMgr_updateRates(pSiteMgr, dot11a, TI_TRUE);

        /* go to B_ONLY Mode only if WiFI bit is Set*/
    if (pSiteMgr->pDesiredParams->siteMgrWiFiAdhoc == TI_TRUE)
    {   /* Configuration For AdHoc when using external configuration */
        if (pSiteMgr->pDesiredParams->siteMgrExternalConfiguration == TI_FALSE)
        {
            siteMgr_externalConfigurationParametersSet(hSiteMgr);
        }
    }
}

static void siteMgr_externalConfigurationParametersSet(TI_HANDLE hSiteMgr)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;

    /* Overwrite the parameters for AdHoc with External Configuration */

    if( ((pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_A_MODE) ||
        (pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_DUAL_MODE)) &&
        !pSiteMgr->pDesiredParams->siteMgrWiFiAdhoc && pSiteMgr->pDesiredParams->siteMgrDesiredBSSType == BSS_INDEPENDENT)
        return;


    if(pSiteMgr->pDesiredParams->siteMgrDesiredBSSType == BSS_INDEPENDENT)
    {
        pSiteMgr->siteMgrOperationalMode = DOT11_B_MODE;
        pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask = rate_BasicToDrvBitmap (BASIC_RATE_SET_1_2_5_5_11, TI_FALSE);
        pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask = rate_SupportedToDrvBitmap (SUPPORTED_RATE_SET_1_2_5_5_11, TI_FALSE);
        pSiteMgr->pDesiredParams->siteMgrRegstryBasicRateMask = rate_BasicToDrvBitmap (BASIC_RATE_SET_1_2_5_5_11, TI_FALSE);
        pSiteMgr->pDesiredParams->siteMgrRegstrySuppRateMask = rate_SupportedToDrvBitmap (SUPPORTED_RATE_SET_1_2_5_5_11, TI_FALSE);
        pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime = PHY_SLOT_TIME_LONG;

        TWD_SetRadioBand(pSiteMgr->hTWD, RADIO_BAND_2_4_GHZ);
        TWD_CfgSlotTime (pSiteMgr->hTWD, PHY_SLOT_TIME_LONG);

    }
    else
    {
        if(pSiteMgr->radioBand == RADIO_BAND_2_4_GHZ)
            pSiteMgr->siteMgrOperationalMode = DOT11_G_MODE;
        else
            pSiteMgr->siteMgrOperationalMode = DOT11_A_MODE;
        pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.basicRateMask = rate_BasicToDrvBitmap (BASIC_RATE_SET_1_2_5_5_11, TI_FALSE);
        pSiteMgr->pDesiredParams->siteMgrCurrentDesiredRateMask.supportedRateMask = rate_SupportedToDrvBitmap (SUPPORTED_RATE_SET_ALL, TI_FALSE);
        pSiteMgr->pDesiredParams->siteMgrRegstryBasicRateMask = rate_BasicToDrvBitmap (BASIC_RATE_SET_1_2_5_5_11, TI_FALSE);
        pSiteMgr->pDesiredParams->siteMgrRegstrySuppRateMask = rate_SupportedToDrvBitmap (SUPPORTED_RATE_SET_ALL, TI_FALSE);

        pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime = PHY_SLOT_TIME_LONG;

        TWD_CfgSlotTime (pSiteMgr->hTWD, PHY_SLOT_TIME_LONG);
    }
}


TI_STATUS siteMgr_saveProbeRespBuffer(TI_HANDLE hSiteMgr, TMacAddr  *bssid, TI_UINT8 *pProbeRespBuffer, TI_UINT32 length)
{
    siteEntry_t *pSite;
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;

    if (pSiteMgr == NULL)
    {
        return TI_NOK;
    }

    TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION , "siteMgr_saveProbeRespBuffer called\n");

    if (pProbeRespBuffer==NULL || length>=MAX_BEACON_BODY_LENGTH)
    {
        return TI_NOK;
    }

    pSite = findSiteEntry(pSiteMgr, bssid);
    if (pSite==NULL)
    {
        return TI_NOK;
    }

    os_memoryCopy(pSiteMgr->hOs, pSite->probeRespBuffer, pProbeRespBuffer, length);
    pSite->probeRespLength = length;

    TRACE17(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_saveProbeRespBuffer: BSSID=%x-%x-%x-%x-%x-%x, TSF=%x-%x-%x-%x-%x-%x-%x-%x, \n ts=%d, rssi=%d\n channel = %d \n", pSite->bssid[0], pSite->bssid[1], pSite->bssid[2], pSite->bssid[3], pSite->bssid[4], pSite->bssid[5], pSite->tsfTimeStamp[0], pSite->tsfTimeStamp[1], pSite->tsfTimeStamp[2], pSite->tsfTimeStamp[3], pSite->tsfTimeStamp[4], pSite->tsfTimeStamp[5], pSite->tsfTimeStamp[6], pSite->tsfTimeStamp[7], os_timeStampMs(pSiteMgr->hOs), pSite->rssi, pSite->channel);

    return TI_OK;
}

TI_STATUS siteMgr_saveBeaconBuffer(TI_HANDLE hSiteMgr, TMacAddr *bssid, TI_UINT8 *pBeaconBuffer, TI_UINT32 length)
{
    siteEntry_t *pSite;
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;

    if (pSiteMgr==NULL)
    {
        return TI_NOK;
    }

	TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION , "siteMgr_saveBeaconBuffer called\n");

    if (pBeaconBuffer==NULL || length>=MAX_BEACON_BODY_LENGTH)
    {
        return TI_NOK;
    }

    pSite = findSiteEntry(pSiteMgr, bssid);
    if (pSite==NULL)
    {
        return TI_NOK;
    }

    os_memoryCopy(pSiteMgr->hOs, pSite->beaconBuffer, pBeaconBuffer, length);
    pSite->beaconLength = length;

    /*if (pSiteMgr->pSitesMgmtParams->pPrimarySite!=NULL)
    {
        if (!os_memoryCompare(pSiteMgr->hOs, pSiteMgr->pSitesMgmtParams->pPrimarySite->ssid.ssidString, pSite->ssid.ssidString, pSiteMgr->pSitesMgmtParams->pPrimarySite->ssid.len))
        {
            TRACE16(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_saveBeaconBuffer: BSSID=%x-%x-%x-%x-%x-%x, TSF=%x-%x-%x-%x-%x-%x-%x-%x, \n ts=%d, rssi=%d \n", pSite->bssid[0], pSite->bssid[1], pSite->bssid[2], pSite->bssid[3], pSite->bssid[4], pSite->bssid[5], pSite->tsfTimeStamp[0], pSite->tsfTimeStamp[1], pSite->tsfTimeStamp[2], pSite->tsfTimeStamp[3], pSite->tsfTimeStamp[4], pSite->tsfTimeStamp[5], pSite->tsfTimeStamp[6], pSite->tsfTimeStamp[7], pSite->osTimeStamp, pSite->rssi);
        }
    }*/
    if ( MAX_SSID_LEN > pSite->ssid.len )
    {
        pSite->ssid.str[pSite->ssid.len] = '\0';
    }

    /*
TRACE9(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_saveBeaconBuffer: BSSID=%x-%x-%x-%x-%x-%x, SSID=, \n ts=%d, rssi=%d\n channel = %d \n", pSite->bssid[0], pSite->bssid[1], pSite->bssid[2], pSite->bssid[3], pSite->bssid[4], pSite->bssid[5], pSite->osTimeStamp, pSite->rssi, pSite->channel);

    */
    return TI_OK;
}

void siteMgr_IsERP_Needed(TI_HANDLE hSiteMgr,TI_BOOL *useProtection,TI_BOOL *NonErpPresent,TI_BOOL *barkerPreambleType)
{
    siteMgr_t       *pSiteMgr = (siteMgr_t*)hSiteMgr;
    paramInfo_t     param;
    siteEntry_t     *pPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;

    *useProtection = TI_FALSE;
    *NonErpPresent = TI_FALSE;
    *barkerPreambleType = TI_FALSE;

    param.paramType = CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM;
    ctrlData_getParam(pSiteMgr->hCtrlData, &param);

    /* On WifiAdhoc (for band B) The STa should not include in the beacon an ERP IE (see WiFi B  clause 2.2, 5.8.2) */
    if (pSiteMgr->pDesiredParams->siteMgrWiFiAdhoc == TI_TRUE)
    {
        /* Return the default => ERP is not needed */
        return;
    }

    /* check if STA is connected */
    if (pPrimarySite)
    {
        if(pSiteMgr->siteMgrOperationalMode == DOT11_G_MODE || pSiteMgr->siteMgrOperationalMode == DOT11_DUAL_MODE)
        {
            if(param.content.ctrlDataIbssProtecionType == ERP_PROTECTION_STANDARD)
            {
                if(pPrimarySite->siteType == SITE_SELF)
                {
                    if(pPrimarySite->channel <= SITE_MGR_CHANNEL_B_G_MAX) /* if channel B&G*/
                    {
                            *useProtection = TI_TRUE;
                            *NonErpPresent = TI_TRUE;
                            *barkerPreambleType = TI_TRUE;
                    }
                }
                else if(pPrimarySite->bssType == BSS_INDEPENDENT)
                {
                    if(pPrimarySite->useProtection == TI_TRUE)
                        *useProtection = TI_TRUE;
                    if(pPrimarySite->NonErpPresent == TI_TRUE)
                        *NonErpPresent = TI_TRUE;
                    if(pPrimarySite->barkerPreambleType == PREAMBLE_SHORT)
                        *barkerPreambleType = TI_TRUE;
                }
            }
        }
    }
}

/** 
 * \fn     siteMgr_CopyToPrimarySite()
 * \brief  Responsible to copy candidate AP from SME table to site Mgr table 
 *
 * \note   
 * \param  hSiteMgr - SiteMgr handle.
 * \param  pCandidate - candidate AP from SME table.
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     
 */ 
TI_STATUS siteMgr_CopyToPrimarySite (TI_HANDLE hSiteMgr, TSiteEntry *pCandidate)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    bssEntry_t  newAP;

    MAC_COPY (newAP.BSSID, pCandidate->bssid);
    newAP.band = pCandidate->eBand;
    newAP.RSSI = pCandidate->rssi;
    newAP.rxRate = pCandidate->rxRate;
    newAP.channel = pCandidate->channel;
    os_memoryCopy(pSiteMgr->hOs, (void *)&newAP.lastRxTSF, (void *)pCandidate->tsfTimeStamp , TIME_STAMP_LEN);
    newAP.beaconInterval = pCandidate->beaconInterval;
    newAP.capabilities = pCandidate->capabilities;
    newAP.pBuffer = NULL;
    /* get frame type */
    if (TI_TRUE == pCandidate->probeRecv)
    {
        newAP.resultType = SCAN_RFT_PROBE_RESPONSE;
        newAP.pBuffer = (TI_UINT8 *)(pCandidate->probeRespBuffer);
        /* length of all IEs of beacon (or probe response) buffer */
        newAP.bufferLength = pCandidate->probeRespLength;
    }
    else
    {
        if (TI_TRUE == pCandidate->beaconRecv)
        {
            newAP.resultType = SCAN_RFT_BEACON;
            newAP.pBuffer = (TI_UINT8 *)(pCandidate->beaconBuffer);
            /* length of all IEs of beacon (or probe response) buffer */
            newAP.bufferLength = pCandidate->beaconLength;

        }
        else
        {
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "siteMgr_CopyToPrimarySite: try to update primary site without updated prob_res\beacon\n\n");
            return TI_NOK;
        }
    }

    if (newAP.pBuffer == NULL)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_ERROR, "siteMgr_CopyToPrimarySite: pBufferBody is NULL !\n");
        return TI_NOK;
    }

    return siteMgr_overwritePrimarySite (hSiteMgr, &newAP, TI_FALSE);
}


/***********************************************************************
 *                        siteMgr_disSelectSite									
 ***********************************************************************
DESCRIPTION: Called by the SME SM in order to dis select the primary site.
			The function set the primary site pointer to NULL and set its type to type regular	
                                                                                                   
INPUT:      hSiteMgr	-	site mgr handle.

OUTPUT:		

RETURN:     TI_OK 

************************************************************************/
TI_STATUS siteMgr_disSelectSite(TI_HANDLE	hSiteMgr)
{
	siteMgr_t	*pSiteMgr = (siteMgr_t *)hSiteMgr;

	/* This protection is because in the case that the BSS was LOST the primary site was removed already. */
	if (pSiteMgr->pSitesMgmtParams->pPrimarySite != NULL)
	{
        TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_disSelectSite REMOVE Primary ssid=, bssid= 0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n\n", pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[0], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[1], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[2], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[3], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[4], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[5] );
		
        pSiteMgr->pSitesMgmtParams->pPrimarySite->siteType = SITE_REGULAR;
		pSiteMgr->pSitesMgmtParams->pPrevPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
		pSiteMgr->pSitesMgmtParams->pPrimarySite = NULL;
	}

	return TI_OK;
}


/**
*
* siteMgr_overwritePrimarySite
*
* \b Description: 
*
* This function sets new AP as a primary site and, if requested, stores previous
* AP's info; called during roaming
*
* \b ARGS:
*
*  I   - pCurrBSS - Current BSS handle \n
*  
* \b RETURNS:
*
*  TI_OK on success, TI_NOK on failure.
*
* \sa 
*/
TI_STATUS siteMgr_overwritePrimarySite(TI_HANDLE hSiteMgr, bssEntry_t *newAP, TI_BOOL requiredToStorePrevSite)
{
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;
    siteEntry_t *newApEntry;
    mlmeIEParsingParams_t *ieListParseParams = mlmeParser_getParseIEsBuffer(pSiteMgr->hMlmeSm);

    TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_overwritePrimarySite: new site bssid= 0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n\n", newAP->BSSID[0], newAP->BSSID[1], newAP->BSSID[2], newAP->BSSID[3], newAP->BSSID[4], newAP->BSSID[5]);

    /* If previous primary site present, and requested to save it - store it */
    if (requiredToStorePrevSite)
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_overwritePrimarySite: required to store prev prim site \n");
        /* Store latest primary site, make ite a regular site */
        pSiteMgr->pSitesMgmtParams->pPrevPrimarySite = pSiteMgr->pSitesMgmtParams->pPrimarySite;
        pSiteMgr->pSitesMgmtParams->pPrevPrimarySite->siteType = SITE_REGULAR;
    }
    else
    {
        TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_overwritePrimarySite: not required to store prev prim site \n");
        if (pSiteMgr->pSitesMgmtParams->pPrimarySite != NULL)
        {
            TRACE6(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "Removing Primary ssid=, bssid= 0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n\n", pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[0], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[1], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[2], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[3], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[4], pSiteMgr->pSitesMgmtParams->pPrimarySite->bssid[5] );

            pSiteMgr->pSitesMgmtParams->pPrimarySite->siteType  = SITE_REGULAR;
            pSiteMgr->pSitesMgmtParams->pPrimarySite->beaconRecv = TI_FALSE;

            pSiteMgr->pSitesMgmtParams->pPrimarySite = NULL;
        }
        else
        {
            TRACE0(pSiteMgr->hReport, REPORT_SEVERITY_INFORMATION, "siteMgr_overwritePrimarySite: primary site is NULL \n");
        }

    }

    /* Find not occupied entry in site table, and store new AP BSSID in */
    /* If pPrimarySite is not set to NULL, store it in pPrevSite before updating */
    newApEntry = findAndInsertSiteEntry(pSiteMgr, &(newAP->BSSID), newAP->band);

    if (newApEntry != NULL)
    {
        /* Zero frame content */
        os_memoryZero(pSiteMgr->hOs, ieListParseParams, sizeof(mlmeIEParsingParams_t));

        /* Update parameters of new AP */
        newApEntry->rssi            = newAP->RSSI;
        newApEntry->bssType         = BSS_INFRASTRUCTURE;
        newApEntry->dtimPeriod      = 1;
        newApEntry->rxRate          = (ERate)newAP->rxRate;
        /* Mark the site as regular in order to prevent from calling Power manager during beacon parsing */
        newApEntry->siteType        = SITE_REGULAR; 

        os_memoryCopy(pSiteMgr->hOs, &newApEntry->ssid, &pSiteMgr->pDesiredParams->siteMgrDesiredSSID, sizeof(TSsid));

        if (newAP->resultType == SCAN_RFT_PROBE_RESPONSE)
        {
            ieListParseParams->frame.subType = PROBE_RESPONSE;
            siteMgr_saveProbeRespBuffer(hSiteMgr, &(newAP->BSSID), newAP->pBuffer, newAP->bufferLength);
        }
        else
        {
            ieListParseParams->frame.subType = BEACON;
            siteMgr_saveBeaconBuffer(hSiteMgr, &(newAP->BSSID), newAP->pBuffer, newAP->bufferLength);
        }
        ieListParseParams->band = newAP->band;
        ieListParseParams->rxChannel = newAP->channel;
        ieListParseParams->myBssid = TI_FALSE;

        ieListParseParams->frame.content.iePacket.pRsnIe = NULL;
        ieListParseParams->frame.content.iePacket.rsnIeLen = 0;
        ieListParseParams->frame.content.iePacket.barkerPreambleMode = PREAMBLE_UNSPECIFIED;
        os_memoryCopy(pSiteMgr->hOs, (void *)ieListParseParams->frame.content.iePacket.timestamp, (void *)&newAP->lastRxTSF, TIME_STAMP_LEN);
        ieListParseParams->frame.content.iePacket.beaconInerval     = newAP->beaconInterval;
        ieListParseParams->frame.content.iePacket.capabilities  = newAP->capabilities;

        if (mlmeParser_parseIEs(pSiteMgr->hMlmeSm, newAP->pBuffer, newAP->bufferLength, ieListParseParams) != TI_OK)
        {
            /* Error in parsing Probe response packet - exit */
            return TI_NOK;
        }

        siteMgr_updateSite(hSiteMgr, &(newAP->BSSID), &ieListParseParams->frame, newAP->channel, newAP->band, TI_FALSE);

        /* Select the entry as primary site */
        newApEntry->siteType = SITE_PRIMARY;
        pSiteMgr->pSitesMgmtParams->pPrimarySite = newApEntry;
        return TI_OK;
    }
    else
    {
        return TI_NOK;
    }
}


/** 
 * \fn     siteMgr_changeBandParams
 * \brief  change band params at the SithMgr and the TWD

 * \param  hSiteMgr - handle to the siteMgr object.
 * \param  radioBand - the set radio band.
 * \return TI_TRUE if site matches RSN settings, TI FALSE if it doesn't
 * \sa     sme_Select
 */ 
void siteMgr_changeBandParams (TI_HANDLE hSiteMgr, ERadioBand radioBand)
{
	paramInfo_t	param;
    siteMgr_t   *pSiteMgr = (siteMgr_t *)hSiteMgr;

	/* change dot11 mode */
	param.paramType = SITE_MGR_OPERATIONAL_MODE_PARAM;
	if(radioBand == RADIO_BAND_2_4_GHZ)
		param.content.siteMgrDot11OperationalMode = DOT11_G_MODE;
	else
		param.content.siteMgrDot11OperationalMode = DOT11_A_MODE;

	siteMgr_setParam(hSiteMgr, &param);
	
	param.paramType = SITE_MGR_RADIO_BAND_PARAM;
	param.content.siteMgrRadioBand = radioBand;
	siteMgr_setParam(hSiteMgr, &param);
	
	siteMgr_setCurrentTable(hSiteMgr, radioBand);
	
	/* configure hal with common core-hal parameters */
	TWD_SetRadioBand (pSiteMgr->hTWD, radioBand);   
}


/***********************************************************************
                incrementTxSessionCount
 ***********************************************************************
DESCRIPTION: Increment the Tx-Session-Counter (cyclic in range 1-7), which
               is configured to the FW in the Join command to distinguish
			   between Tx packets of different AP-connections.
			 Update also the TxCtrl module.

INPUT:      pSiteMgr    -   site mgr handle.

OUTPUT:

RETURN:     TI_UINT16 - the new Tx-Session-Count (range is 1-7).

************************************************************************/
static TI_UINT16 incrementTxSessionCount(siteMgr_t *pSiteMgr)
{
	pSiteMgr->txSessionCount++;
	if(pSiteMgr->txSessionCount > MAX_TX_SESSION_COUNT)
		pSiteMgr->txSessionCount = MIN_TX_SESSION_COUNT;

	txCtrlParams_updateTxSessionCount(pSiteMgr->hTxCtrl, pSiteMgr->txSessionCount);

	return pSiteMgr->txSessionCount;
}


/**
*
* siteMgr_TxPowerHighThreshold
*
* \b Description: 
*
* Called by EventMBox upon TX Power Adaptation High Threshold Trigger.
*
* \b ARGS:
*
*  I   - hSiteMgr - Site Mgr handle \n
*  
* \b RETURNS:
*
*  None
*
* \sa 
*/
void siteMgr_TxPowerHighThreshold(TI_HANDLE hSiteMgr,
                                      TI_UINT8     *data,
                                      TI_UINT8     dataLength)
{
    siteMgr_TxPowerAdaptation(hSiteMgr, RSSI_EVENT_DIR_HIGH);
}

/**
*
* siteMgr_TxPowerLowThreshold
*
* \b Description: 
*
* Called by EventMBox upon TX Power Adaptation Low Threshold Trigger.
*
* \b ARGS:
*
*  I   - hSiteMgr - Site Mgr handle \n
*  
* \b RETURNS:
*
*  None
*
* \sa 
*/
void siteMgr_TxPowerLowThreshold(TI_HANDLE hSiteMgr,
                                      TI_UINT8     *data,
                                      TI_UINT8     dataLength)
{
    siteMgr_TxPowerAdaptation(hSiteMgr, RSSI_EVENT_DIR_LOW);
}

/**
*
* siteMgr_TxPowerAdaptation
*
* \b Description: 
*
* TX Power Adaptation - Used for TX power adjust .
*
* \b ARGS:
*
*  I   - hSiteMgr - Site Mgr handle \n
*      - highLowEdge - Direction of Edge event
*  
* \b RETURNS:
*
*  None
*
* \sa 
*/
static void siteMgr_TxPowerAdaptation(TI_HANDLE hSiteMgr, RssiEventDir_e highLowEdge)
{
    siteMgr_t    *pSiteMgr = (siteMgr_t *)hSiteMgr;

    if (pSiteMgr->siteMgrTxPowerEnabled)
    {
        if (highLowEdge == RSSI_EVENT_DIR_HIGH)
        {
            /* enable the "Temporary TX power" when close to AP */
            siteMgr_setTemporaryTxPower (pSiteMgr, TI_TRUE) ;  
        }
        else
        {
            /* disable the "Temporary TX power" when we are again far from the AP */
            siteMgr_setTemporaryTxPower (pSiteMgr, TI_FALSE);  
        }
    }
}

