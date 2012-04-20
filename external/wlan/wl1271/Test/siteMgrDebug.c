/*
 * siteMgrDebug.c
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

/** \file reportReplvl.c
 *  \brief Report level implementation
 * 
 *  \see reportReplvl.h
 */

/** \file   siteMgrDebug.c 
 *  \brief  The siteMgrDebug module.
 *  
 *  \see    siteMgrDebug.h
 */

#include "tidef.h"
#include "osApi.h"
#include "paramOut.h"
#include "siteMgrDebug.h"
#include "siteMgrApi.h"
#include "siteHash.h"
#include "report.h"
#include "CmdDispatcher.h"
#include "DrvMainModules.h"
#include "sme.h"
#include "apConn.h"
#include "healthMonitor.h"
#include "conn.h"
#include "connApi.h"

#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif


static void printPrimarySite(siteMgr_t *pSiteMgr);

void printSiteTable(siteMgr_t *pSiteMgr, char *desiredSsid);

static void printDesiredParams(siteMgr_t *pSiteMgr, TI_HANDLE hCmdDispatch);

static void printPrimarySiteDesc(siteMgr_t *pSiteMgr, OS_802_11_BSSID *pPrimarySiteDesc);

static void setRateSet(TI_UINT8 maxRate, TRates *pRates);

void printSiteMgrHelpMenu(void);

/*	Function implementation */
void siteMgrDebugFunction (TI_HANDLE         hSiteMgr, 
                           TStadHandlesList *pStadHandles,
                           TI_UINT32         funcType, 
                           void             *pParam)
{
	siteMgr_t *pSiteMgr = (siteMgr_t *)hSiteMgr;
	paramInfo_t		param;
	TSsid			newDesiredSsid;
	TI_UINT8		value;
	TI_UINT8		i;
	OS_802_11_BSSID primarySiteDesc;
	TRates			ratesSet;


	newDesiredSsid.len = 5;
	os_memoryCopy(pSiteMgr->hOs, (void *)newDesiredSsid.str, "yaeli", 5);

	
	switch (funcType)
	{
	case SITE_MGR_DEBUG_HELP_MENU:
		printSiteMgrHelpMenu();
		break;

	case PRIMARY_SITE_DBG:
		printPrimarySite(pSiteMgr);
		break;

	case SITE_TABLE_DBG:
		printSiteTable(pSiteMgr, NULL);
		break;

	case DESIRED_PARAMS_DBG:
		printDesiredParams(pSiteMgr, pStadHandles->hCmdDispatch);
		break;

	case GET_PRIMARY_SITE_DESC_DBG:
		param.paramType = SITE_MGR_GET_SELECTED_BSSID_INFO;
		param.content.pSiteMgrPrimarySiteDesc = &primarySiteDesc;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		printPrimarySiteDesc(pSiteMgr, &primarySiteDesc);
		break;

	case SET_RSN_DESIRED_CIPHER_SUITE_DBG:
		param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
		value = *((TI_UINT32 *)pParam);
		param.content.rsnEncryptionStatus = (ECipherSuite)value;
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param); 
		WLAN_OS_REPORT(("\nSetting RSN_DESIRED_CIPHER_SUITE_PARAM : %d\n", value));
		break;

	case GET_RSN_DESIRED_CIPHER_SUITE_DBG:
		param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		WLAN_OS_REPORT(("\nGetting RSN_DESIRED_CIPHER_SUITE_PARAM: %d\n", param.content.rsnEncryptionStatus));
		break;
 
	case SET_RSN_DESIRED_AUTH_TYPE_DBG:
		param.paramType = RSN_EXT_AUTHENTICATION_MODE;
		value = *((TI_UINT32 *)pParam);
		param.content.rsnDesiredAuthType = (EAuthSuite)value;
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param); 
		if (value == RSN_AUTH_OPEN)
			WLAN_OS_REPORT(("\nSetting RSN_DESIRED_AUTH_TYPE_PARAM:	RSN_AUTH_OPEN\n"));
		else if (value == RSN_AUTH_SHARED_KEY)
			WLAN_OS_REPORT(("\nSetting RSN_DESIRED_AUTH_TYPE_PARAM:	RSN_AUTH_SHARED_KEY\n"));
		else if (value == RSN_AUTH_AUTO_SWITCH)
			WLAN_OS_REPORT(("\nSetting RSN_DESIRED_AUTH_TYPE_PARAM:	RSN_AUTH_AUTO_SWITCH\n"));
		else 
			WLAN_OS_REPORT(("\nSetting RSN_DESIRED_AUTH_TYPE_PARAM:	Invalid: %d\n", value));
		break;

	case GET_RSN_DESIRED_AUTH_TYPE_DBG:
		param.paramType = RSN_EXT_AUTHENTICATION_MODE;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		if (param.content.rsnDesiredAuthType == RSN_AUTH_OPEN)
			WLAN_OS_REPORT(("\nGetting RSN_DESIRED_AUTH_TYPE_PARAM:	RSN_AUTH_OPEN\n"));
		else if (param.content.rsnDesiredAuthType == RSN_AUTH_SHARED_KEY)
			WLAN_OS_REPORT(("\nGetting RSN_DESIRED_AUTH_TYPE_PARAM:	RSN_AUTH_SHARED_KEY\n"));
		else if (param.content.rsnDesiredAuthType == RSN_AUTH_AUTO_SWITCH)
			WLAN_OS_REPORT(("\nGetting RSN_DESIRED_AUTH_TYPE_PARAM:	RSN_AUTH_AUTO_SWITCH\n"));
		else 
			WLAN_OS_REPORT(("\nGetting RSN_DESIRED_AUTH_TYPE_PARAM:	Invalid: %d\n", param.content.rsnDesiredAuthType));

		break;

	case GET_CONNECTION_STATUS_DBG:
		param.paramType = SME_CONNECTION_STATUS_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		if (param.content.smeSmConnectionStatus == eDot11Idle)
			WLAN_OS_REPORT(("\nGetting SITE_MGR_CONNECTION_STATUS_PARAM:	STATUS_IDLE\n"));
		else if (param.content.smeSmConnectionStatus == eDot11Scaning)
			WLAN_OS_REPORT(("\nGetting SITE_MGR_CONNECTION_STATUS_PARAM:	STATUS_SCANNING\n"));
		else if (param.content.smeSmConnectionStatus == eDot11Connecting)
			WLAN_OS_REPORT(("\nGetting SITE_MGR_CONNECTION_STATUS_PARAM:	STATUS_CONNECTING\n"));
		else if (param.content.smeSmConnectionStatus == eDot11Associated)
			WLAN_OS_REPORT(("\nGetting SITE_MGR_CONNECTION_STATUS_PARAM:	STATUS_ASSOCIATED\n"));
		else if (param.content.smeSmConnectionStatus == eDot11Disassociated)
			WLAN_OS_REPORT(("\nGetting SITE_MGR_CONNECTION_STATUS_PARAM:	STATUS_DIS_ASSOCIATED\n"));
        else if (param.content.smeSmConnectionStatus == eDot11RadioDisabled)
            WLAN_OS_REPORT(("\nGetting SITE_MGR_CONNECTION_STATUS_PARAM:	STATUS_RADIO_DISABLED\n"));
        else 
			WLAN_OS_REPORT(("\nGetting SITE_MGR_CONNECTION_STATUS_PARAM:	STATUS_ERROR\n"));
		break;

	case SET_SUPPORTED_RATE_SET_DBG:
		param.paramType = SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM;
		value = *((TI_UINT32 *)pParam);
		setRateSet(value, &ratesSet);
		os_memoryCopy(pSiteMgr->hOs, &(param.content.siteMgrDesiredSupportedRateSet), &(ratesSet), sizeof(TRates));
		WLAN_OS_REPORT(("\nSetting SET_SUPPORTED_RATE_SET_DBG\n"));
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param);
		break;

	case GET_SUPPORTED_RATE_SET_DBG:
		param.paramType = SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		WLAN_OS_REPORT(("\nGetting SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM\n"));
		if(param.content.siteMgrDesiredSupportedRateSet.len == 0)
			WLAN_OS_REPORT(("\nNo rates defined\n"));
		else 
		{	
			/* It looks like it never happens. Anyway decided to check */
            if ( param.content.siteMgrDesiredSupportedRateSet.len > DOT11_MAX_SUPPORTED_RATES )
            {
                WLAN_OS_REPORT(("siteMgrDebugFunction. param.content.siteMgrDesiredSupportedRateSet.len=%d exceeds the limit %d\n",
                         param.content.siteMgrDesiredSupportedRateSet.len, DOT11_MAX_SUPPORTED_RATES));
                handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
                param.content.siteMgrDesiredSupportedRateSet.len = DOT11_MAX_SUPPORTED_RATES;
            }
			for (i = 0; i < param.content.siteMgrDesiredSupportedRateSet.len; i++)
				WLAN_OS_REPORT(("\nRate %d is 0x%X\n", i +1, param.content.siteMgrDesiredSupportedRateSet.ratesString[i]));
		}
		break;

	case SET_MLME_LEGACY_AUTH_TYPE_DBG:
		param.paramType = MLME_LEGACY_TYPE_PARAM;
		value = *((TI_UINT32 *)pParam);
		param.content.mlmeLegacyAuthType = (legacyAuthType_e)value;
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param); 
		if (value == AUTH_LEGACY_OPEN_SYSTEM)
			WLAN_OS_REPORT(("\nSetting MLME_LEGACY_TYPE_PARAM:	AUTH_LEGACY_OPEN_SYSTEM\n"));
		else if (value == AUTH_LEGACY_SHARED_KEY)
			WLAN_OS_REPORT(("\nSetting MLME_LEGACY_TYPE_PARAM:	AUTH_LEGACY_SHARED_KEY\n"));
		else if (value == AUTH_LEGACY_AUTO_SWITCH)
			WLAN_OS_REPORT(("\nSetting MLME_LEGACY_TYPE_PARAM:	AUTH_LEGACY_AUTO_SWITCH\n"));
		else 
			WLAN_OS_REPORT(("\nSetting MLME_LEGACY_TYPE_PARAM:	Invalid: %d\n", value));
		break;

	case GET_MLME_LEGACY_AUTH_TYPE_DBG:
		param.paramType = MLME_LEGACY_TYPE_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		if (param.content.mlmeLegacyAuthType == AUTH_LEGACY_OPEN_SYSTEM)
			WLAN_OS_REPORT(("\nGetting MLME_LEGACY_TYPE_PARAM:	AUTH_LEGACY_OPEN_SYSTEM\n"));
		else if (param.content.rsnDesiredAuthType == AUTH_LEGACY_SHARED_KEY)
			WLAN_OS_REPORT(("\nGetting MLME_LEGACY_TYPE_PARAM:	AUTH_LEGACY_SHARED_KEY\n"));
		else if (param.content.rsnDesiredAuthType == AUTH_LEGACY_AUTO_SWITCH)
			WLAN_OS_REPORT(("\nGetting MLME_LEGACY_TYPE_PARAM:	AUTH_AUTO_SWITCH\n"));
		else 
			WLAN_OS_REPORT(("\nGetting MLME_LEGACY_TYPE_PARAM:	Invalid: %d\n", param.content.rsnDesiredAuthType));

		break;


	case RADIO_STAND_BY_CHANGE_STATE:
		WLAN_OS_REPORT(("\nChange GPIO-13 State...\n"));
		break;
		

    case PRINT_FAILURE_EVENTS:
        {

	WLAN_OS_REPORT(("\n PRINT HEALTH MONITOR LOG\n"));
	healthMonitor_printFailureEvents (pStadHandles->hHealthMonitor);
	apConn_printStatistics(pStadHandles->hAPConnection);
#ifdef REPORT_LOG		
        conn_ibssPrintStatistics(pStadHandles->hConn);
#endif
        if (((conn_t*)pStadHandles->hConn)->currentConnType==CONNECTION_INFRA)
        {
            switch (((conn_t*)pStadHandles->hConn)->state)
            {
            case   0:  WLAN_OS_REPORT((" CONN state is IDLE\n")); 
                break;       
             case   1:  WLAN_OS_REPORT((" CONN state is SCR_WAIT\n")); 
                break;             
             case   2:  WLAN_OS_REPORT((" CONN state is WAIT_JOIN_CMPLT\n")); 
                break;            
             case   3:  WLAN_OS_REPORT((" CONN state is MLME_WAIT\n")); 
                break;           
             case   4:  WLAN_OS_REPORT((" CONN state is RSN_WAIT\n")); 
                break;            
             case   5:  WLAN_OS_REPORT((" CONN state is CONFIG_HW\n")); 
                break;        
             case   6:  WLAN_OS_REPORT((" CONN state is CONNECTED\n")); 
                break; 
            case   7:  WLAN_OS_REPORT((" CONN state is DISASSOCC\n")); 
               break; 
            default:
                break;
            }
        }
        }
        break;

	case FORCE_HW_RESET_RECOVERY:
		WLAN_OS_REPORT(("\n Currently not supported!\n"));
		break;

	case FORCE_SOFT_RECOVERY:
		WLAN_OS_REPORT(("\n FORCE Full Recovery (Soft)\n"));
		break;


	case PERFORM_HEALTH_TEST:
		WLAN_OS_REPORT(("\n PERFORM_HEALTH_TEST \n"));
		healthMonitor_PerformTest(pStadHandles->hHealthMonitor, TI_FALSE);	
		break;

	case PRINT_SITE_TABLE_PER_SSID:
		printSiteTable(pSiteMgr, (char*)pParam);
		break;

	case SET_DESIRED_CHANNEL:
		param.paramType = SITE_MGR_DESIRED_CHANNEL_PARAM;
		param.content.siteMgrDesiredChannel = *(TI_UINT8*)pParam;
		siteMgr_setParam(pStadHandles->hSiteMgr, &param);
		break;

	default:
		WLAN_OS_REPORT(("Invalid function type in Debug Site Manager Function Command: %d\n\n", funcType));
		break;
	}
} 

static void printPrimarySite(siteMgr_t *pSiteMgr)
{
	siteEntry_t *pSiteEntry;	
    TI_UINT8	len;
	char	ssid[MAX_SSID_LEN + 1];
	
	pSiteEntry = pSiteMgr->pSitesMgmtParams->pPrimarySite;
	
	if (pSiteEntry == NULL)
	{
		WLAN_OS_REPORT(("\n\n************************	PRIMARY SITE IS NULL	****************************\n\n\n"));
		return;
	}

	WLAN_OS_REPORT(("\n\n************************	PRIMARY SITE	****************************\n\n\n"));
	
	WLAN_OS_REPORT(("BSSID			%2X-%2X-%2X-%2X-%2X-%2X	",	
														pSiteEntry->bssid[0], 
														pSiteEntry->bssid[1], 
														pSiteEntry->bssid[2], 
														pSiteEntry->bssid[3], 
														pSiteEntry->bssid[4], 
														pSiteEntry->bssid[5]));
    len = pSiteEntry->ssid.len;
    /* It looks like it never happens. Anyway decided to check */
    if ( pSiteEntry->ssid.len > MAX_SSID_LEN )
    {
        WLAN_OS_REPORT(("printPrimarySite. pSiteEntry->ssid.len=%d exceeds the limit %d\n",
                   pSiteEntry->ssid.len, MAX_SSID_LEN));
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        len = MAX_SSID_LEN;
    }
	os_memoryCopy(pSiteMgr->hOs, ssid, (void *)pSiteEntry->ssid.str, len);
	ssid[len] = '\0';
	WLAN_OS_REPORT(("SSID			%s\n\n", ssid));

	if (pSiteEntry->bssType == BSS_INFRASTRUCTURE)
		WLAN_OS_REPORT(("BSS Type		INFRASTRUCTURE\n\n"));
	else if (pSiteEntry->bssType == BSS_INDEPENDENT)
		WLAN_OS_REPORT(("BSS Type		IBSS\n\n"));
	else if (pSiteEntry->bssType == BSS_ANY)
		WLAN_OS_REPORT(("BSS Type		ANY\n\n"));
	else
		WLAN_OS_REPORT(("BSS Type		INVALID\n\n"));


	WLAN_OS_REPORT(("Channel			%d\n", pSiteEntry->channel));

	WLAN_OS_REPORT(("\n"));

	switch (pSiteEntry->maxBasicRate)
	{
	case DRV_RATE_1M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_1M_BIT\n"));
		break;

	case DRV_RATE_2M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_2M_BIT\n"));
		break;

	case DRV_RATE_5_5M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_5_5M_BIT\n"));
		break;

	case DRV_RATE_11M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_11M_BIT\n"));
		break;

	case DRV_RATE_6M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_6M_BIT\n"));
		break;

	case DRV_RATE_9M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_9M_BIT\n"));
		break;

	case DRV_RATE_12M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_12M_BIT\n"));
		break;

	case DRV_RATE_18M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_18M_BIT\n"));
		break;

	case DRV_RATE_24M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_24M_BIT\n"));
		break;

	case DRV_RATE_36M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_36M_BIT\n"));
		break;

	case DRV_RATE_48M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_48M_BIT\n"));
		break;

	case DRV_RATE_54M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_54M_BIT\n"));
		break;

	default:
		WLAN_OS_REPORT(("Max Basic Rate		INVALID,  0x%X\n", pSiteEntry->maxBasicRate));
		break;
	}

	switch (pSiteEntry->maxActiveRate)
	{
	case DRV_RATE_1M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_1M_BIT\n"));
		break;

	case DRV_RATE_2M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_2M_BIT\n"));
		break;

	case DRV_RATE_5_5M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_5_5M_BIT\n"));
		break;

	case DRV_RATE_11M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_11M_BIT\n"));
		break;

	case DRV_RATE_22M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_22M_BIT\n"));
		break;

	case DRV_RATE_6M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_6M_BIT\n"));
		break;

	case DRV_RATE_9M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_9M_BIT\n"));
		break;

	case DRV_RATE_12M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_12M_BIT\n"));
		break;

	case DRV_RATE_18M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_18M_BIT\n"));
		break;

	case DRV_RATE_24M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_24M_BIT\n"));
		break;

	case DRV_RATE_36M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_36M_BIT\n"));
		break;

	case DRV_RATE_48M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_48M_BIT\n"));
		break;

	case DRV_RATE_54M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_54M_BIT\n"));
		break;

	default:
		WLAN_OS_REPORT(("Max Active Rate		INVALID,  0x%X\n", pSiteEntry->maxActiveRate));
		break;
	}

	WLAN_OS_REPORT(("\n"));

	if (pSiteEntry->probeModulation == DRV_MODULATION_QPSK)
		WLAN_OS_REPORT(("Probe Modulation	QPSK\n"));
	else if (pSiteEntry->probeModulation == DRV_MODULATION_CCK)
		WLAN_OS_REPORT(("Probe Modulation	CCK\n"));
	else if (pSiteEntry->probeModulation == DRV_MODULATION_PBCC)
		WLAN_OS_REPORT(("Probe Modulation	PBCC\n"));
	else if (pSiteEntry->probeModulation == DRV_MODULATION_OFDM)
		WLAN_OS_REPORT(("Probe Modulation	OFDM\n"));
	else
		WLAN_OS_REPORT(("Probe Modulation	INVALID, %d\n", pSiteEntry->probeModulation));

	if (pSiteEntry->beaconModulation == DRV_MODULATION_QPSK)
		WLAN_OS_REPORT(("Beacon Modulation	QPSK\n"));
	else if (pSiteEntry->beaconModulation == DRV_MODULATION_CCK)
		WLAN_OS_REPORT(("Beacon Modulation	CCK\n"));
	else if (pSiteEntry->beaconModulation == DRV_MODULATION_PBCC)
		WLAN_OS_REPORT(("Beacon Modulation	PBCC\n"));
	else if (pSiteEntry->beaconModulation == DRV_MODULATION_OFDM)
		WLAN_OS_REPORT(("Beacon Modulation	OFDM\n"));
	else
		WLAN_OS_REPORT(("Beacon Modulation	INVALID, %d\n", pSiteEntry->beaconModulation));

	WLAN_OS_REPORT(("\n"));

	if (pSiteEntry->privacy == TI_TRUE)
		WLAN_OS_REPORT(("Privacy							        On\n\n"));
	else
		WLAN_OS_REPORT(("Privacy							        Off\n\n"));

	if (pSiteEntry->currentPreambleType == PREAMBLE_SHORT)
		WLAN_OS_REPORT(("Cap Preamble Type     Short\n"));
	else if (pSiteEntry->currentPreambleType == PREAMBLE_LONG)
		WLAN_OS_REPORT(("Cap Preamble Type     Long\n"));
	else
		WLAN_OS_REPORT(("Preamble	INVALID, %d\n", pSiteEntry->currentPreambleType));


	if(pSiteEntry->barkerPreambleType == PREAMBLE_UNSPECIFIED)
		WLAN_OS_REPORT(("Barker preamble Type		Unspecified\n"));
	else if(pSiteEntry->barkerPreambleType == PREAMBLE_SHORT)
		WLAN_OS_REPORT(("Barker_Preamble Type		Short\n"));
	else
		WLAN_OS_REPORT(("Barker_Preamble Type		Long\n"));

	if(pSiteEntry->currentSlotTime == PHY_SLOT_TIME_SHORT)
		WLAN_OS_REPORT(("Slot time type					   Short\n"));
	else
		WLAN_OS_REPORT(("Slot time type					   Long\n"));


	WLAN_OS_REPORT(("\n"));

	WLAN_OS_REPORT(("Beacon interval		%d\n", pSiteEntry->beaconInterval));

	WLAN_OS_REPORT(("Local Time Stamp	%d\n", pSiteEntry->localTimeStamp));

	WLAN_OS_REPORT(("rssi			%d\n", pSiteEntry->rssi));

	WLAN_OS_REPORT(("\n"));

	WLAN_OS_REPORT(("Fail status		%d\n", pSiteEntry->failStatus));

	WLAN_OS_REPORT(("\n---------------------------------------------------------------\n\n", NULL)); 

}

void printSiteTable(siteMgr_t *pSiteMgr, char *desiredSsid)
{
	TI_UINT8	i, numOfSites = 0;
	siteEntry_t *pSiteEntry;	
	char	ssid[MAX_SSID_LEN + 1];
    siteTablesParams_t      *pCurrentSiteTable = pSiteMgr->pSitesMgmtParams->pCurrentSiteTable;
    TI_UINT8                   tableIndex=2;

    WLAN_OS_REPORT(("\n\n************************	SITE TABLE	****************************\n\n\n"));
	
    /* It looks like it never happens. Anyway decided to check */
    if ( pCurrentSiteTable->maxNumOfSites > MAX_SITES_BG_BAND )
    {
        WLAN_OS_REPORT(("printSiteTable. pCurrentSiteTable->maxNumOfSites=%d exceeds the limit %d\n",
                   pCurrentSiteTable->maxNumOfSites, MAX_SITES_BG_BAND));
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        pCurrentSiteTable->maxNumOfSites = MAX_SITES_BG_BAND;
    }

    do
	{
        tableIndex--;
		for (i = 0; i < pCurrentSiteTable->maxNumOfSites; i++)
		{
			pSiteEntry = &(pCurrentSiteTable->siteTable[i]);
	
			if (pSiteEntry->siteType == SITE_NULL)
				continue;
            /* It looks like it never happens. Anyway decided to check */
            if ( pCurrentSiteTable->siteTable[i].ssid.len > MAX_SSID_LEN )
            {
                WLAN_OS_REPORT(("printSiteTable. pCurrentSiteTable->siteTable[%d].ssid.len=%d exceeds the limit %d\n",
                         i, pCurrentSiteTable->siteTable[i].ssid.len, MAX_SSID_LEN));
                handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
                pCurrentSiteTable->siteTable[i].ssid.len = MAX_SSID_LEN;
            }
			os_memoryCopy(pSiteMgr->hOs ,ssid, (void *)pCurrentSiteTable->siteTable[i].ssid.str, pCurrentSiteTable->siteTable[i].ssid.len);
			ssid[pCurrentSiteTable->siteTable[i].ssid.len] = '\0';
			
			if (desiredSsid != NULL)
			{
				int desiredSsidLength = 0;
				char * tmp = desiredSsid;

				while (tmp != '\0')
				{
					desiredSsidLength++;
					tmp++;
				}

				if (os_memoryCompare(pSiteMgr->hOs, (TI_UINT8 *)ssid, (TI_UINT8 *)desiredSsid, desiredSsidLength))
					continue;
			}
			
			WLAN_OS_REPORT(("SSID	%s\n\n", ssid));
	
			 
 
			if (pSiteEntry->siteType == SITE_PRIMARY)
				WLAN_OS_REPORT( ("	 ENTRY PRIMARY %d \n", numOfSites));
			else
				WLAN_OS_REPORT( ("	ENTRY %d\n", i));
	
			WLAN_OS_REPORT(("BSSID			%2X-%2X-%2X-%2X-%2X-%2X	\n",	
																pCurrentSiteTable->siteTable[i].bssid[0], 
																pCurrentSiteTable->siteTable[i].bssid[1], 
																pCurrentSiteTable->siteTable[i].bssid[2], 
																pCurrentSiteTable->siteTable[i].bssid[3], 
																pCurrentSiteTable->siteTable[i].bssid[4], 
																pCurrentSiteTable->siteTable[i].bssid[5]));
		
		
			if (pCurrentSiteTable->siteTable[i].bssType == BSS_INFRASTRUCTURE)
				WLAN_OS_REPORT(("BSS Type		INFRASTRUCTURE\n\n"));
			else if (pCurrentSiteTable->siteTable[i].bssType == BSS_INDEPENDENT)
				WLAN_OS_REPORT(("BSS Type		IBSS\n\n"));
			else if (pCurrentSiteTable->siteTable[i].bssType == BSS_ANY)
				WLAN_OS_REPORT(("BSS Type		ANY\n\n"));
			else
				WLAN_OS_REPORT(("BSS Type		INVALID\n\n"));
		
		
			WLAN_OS_REPORT(("Channel			%d\n", pCurrentSiteTable->siteTable[i].channel));
		
			WLAN_OS_REPORT(("\n"));
		
			switch (pCurrentSiteTable->siteTable[i].maxBasicRate)
			{
			case DRV_RATE_1M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_1M_BIT\n"));
				break;
	
			case DRV_RATE_2M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_2M_BIT\n"));
				break;
	
			case DRV_RATE_5_5M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_5_5M_BIT\n"));
				break;
	
			case DRV_RATE_11M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_11M_BIT\n"));
				break;
	
			case DRV_RATE_6M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_6M_BIT\n"));
				break;
		
			case DRV_RATE_9M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_9M_BIT\n"));
				break;
		
			case DRV_RATE_12M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_12M_BIT\n"));
				break;
		
			case DRV_RATE_18M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_18M_BIT\n"));
				break;
		
			case DRV_RATE_24M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_24M_BIT\n"));
				break;
		
			case DRV_RATE_36M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_36M_BIT\n"));
				break;
		
			case DRV_RATE_48M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_48M_BIT\n"));
				break;
		
			case DRV_RATE_54M:
				WLAN_OS_REPORT(("Max Basic Rate		RATE_54M_BIT\n"));
				break;

			default:
					WLAN_OS_REPORT(("Max Basic Rate		INVALID,  0x%X\n", pCurrentSiteTable->siteTable[i].maxBasicRate));
				break;
			}
	
				switch (pCurrentSiteTable->siteTable[i].maxActiveRate)
			{
			case DRV_RATE_1M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_1M_BIT\n"));
				break;
	
			case DRV_RATE_2M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_2M_BIT\n"));
				break;
	
			case DRV_RATE_5_5M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_5_5M_BIT\n"));
				break;
	
			case DRV_RATE_11M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_11M_BIT\n"));
				break;
	
			case DRV_RATE_22M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_22M_BIT\n"));
				break;

			case DRV_RATE_6M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_6M_BIT\n"));
				break;
		
			case DRV_RATE_9M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_9M_BIT\n"));
				break;
		
			case DRV_RATE_12M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_12M_BIT\n"));
				break;
		
			case DRV_RATE_18M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_18M_BIT\n"));
				break;
		
			case DRV_RATE_24M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_24M_BIT\n"));
				break;
		
			case DRV_RATE_36M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_36M_BIT\n"));
				break;
		
			case DRV_RATE_48M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_48M_BIT\n"));
				break;
		
			case DRV_RATE_54M:
				WLAN_OS_REPORT(("Max Active Rate		RATE_54M_BIT\n"));
				break;
	
			default:
					WLAN_OS_REPORT(("Max Active Rate		INVALID,  0x%X\n", pCurrentSiteTable->siteTable[i].maxActiveRate));
				break;
			}
	
			WLAN_OS_REPORT(("\n"));
	
				if (pCurrentSiteTable->siteTable[i].probeModulation == DRV_MODULATION_QPSK)
				WLAN_OS_REPORT(("Probe Modulation	QPSK\n"));
				else if (pCurrentSiteTable->siteTable[i].probeModulation == DRV_MODULATION_CCK)
				WLAN_OS_REPORT(("Probe Modulation	CCK\n"));
				else if (pCurrentSiteTable->siteTable[i].probeModulation == DRV_MODULATION_PBCC)
					WLAN_OS_REPORT(("Probe Modulation	PBCC\n"));
				else
					WLAN_OS_REPORT(("Probe Modulation	INVALID, %d\n", pCurrentSiteTable->siteTable[i].probeModulation));
		
				if (pCurrentSiteTable->siteTable[i].beaconModulation == DRV_MODULATION_QPSK)
					WLAN_OS_REPORT(("Beacon Modulation	QPSK\n"));
				else if (pCurrentSiteTable->siteTable[i].beaconModulation == DRV_MODULATION_CCK)
					WLAN_OS_REPORT(("Beacon Modulation	CCK\n"));
				else if (pCurrentSiteTable->siteTable[i].beaconModulation == DRV_MODULATION_PBCC)
					WLAN_OS_REPORT(("Beacon Modulation	PBCC\n"));
				else
					WLAN_OS_REPORT(("Beacon Modulation	INVALID, %d\n", pCurrentSiteTable->siteTable[i].beaconModulation));
		
				WLAN_OS_REPORT(("\n"));
		
				if (pCurrentSiteTable->siteTable[i].privacy == TI_TRUE)
				WLAN_OS_REPORT(("Privacy			On\n"));
			else
				WLAN_OS_REPORT(("Privacy			Off\n"));
	
				if (pCurrentSiteTable->siteTable[i].currentPreambleType == PREAMBLE_SHORT)
				WLAN_OS_REPORT(("Preamble Type		Short\n"));
				else if (pCurrentSiteTable->siteTable[i].currentPreambleType == PREAMBLE_LONG)
				WLAN_OS_REPORT(("Preamble Type		Long\n"));
			else
					WLAN_OS_REPORT(("Preamble	INVALID, %d\n", pCurrentSiteTable->siteTable[i].currentPreambleType));
	
	
			WLAN_OS_REPORT(("\n"));
	
				WLAN_OS_REPORT(("Beacon interval		%d\n", pCurrentSiteTable->siteTable[i].beaconInterval));
	
				WLAN_OS_REPORT(("Local Time Stamp	%d\n", pCurrentSiteTable->siteTable[i].localTimeStamp));
		
				WLAN_OS_REPORT(("rssi			%d\n", pCurrentSiteTable->siteTable[i].rssi));
		
				WLAN_OS_REPORT(("\n"));
		
				WLAN_OS_REPORT(("Fail status		%d\n", pCurrentSiteTable->siteTable[i].failStatus));
		
				WLAN_OS_REPORT(("ATIM Window %d\n", pCurrentSiteTable->siteTable[i].atimWindow));
	
			WLAN_OS_REPORT(("\n---------------------------------------------------------------\n\n", NULL)); 
	
			numOfSites++;
		}
	
		WLAN_OS_REPORT(("\n		Number Of Sites:	%d\n", numOfSites)); 
		WLAN_OS_REPORT(("\n---------------------------------------------------------------\n", NULL)); 
		
		   if ((pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_DUAL_MODE) && (tableIndex==1))
		   {   /* change site table */
			   if (pCurrentSiteTable == &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables)
				  {
                   WLAN_OS_REPORT(("\n		dot11A_sitesTables	\n")); 

                   pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
				  }
			   else
				  {
                   WLAN_OS_REPORT(("\n		dot11BG_sitesTables	\n")); 

                   pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
				  }
		   }

    } while (tableIndex>0);
}

static void printDesiredParams(siteMgr_t *pSiteMgr, TI_HANDLE hCmdDispatch)
{  
	paramInfo_t		param;
	
	WLAN_OS_REPORT(("\n\n*****************************************", NULL));
	WLAN_OS_REPORT(("*****************************************\n\n", NULL));

	WLAN_OS_REPORT(("Channel			%d\n", pSiteMgr->pDesiredParams->siteMgrDesiredChannel));
	
	WLAN_OS_REPORT(("\n*****************************************\n\n", NULL));

	switch (pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxBasic)
	{
	case DRV_RATE_1M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_1M_BIT\n"));
		break;

	case DRV_RATE_2M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_2M_BIT\n"));
		break;

	case DRV_RATE_5_5M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_5_5M_BIT\n"));
		break;

	case DRV_RATE_11M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_11M_BIT\n"));
		break;

	case DRV_RATE_22M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_22M_BIT\n"));
		break;

	case DRV_RATE_6M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_6M_BIT\n"));
		break;

	case DRV_RATE_9M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_9M_BIT\n"));
		break;

	case DRV_RATE_12M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_12M_BIT\n"));
		break;

	case DRV_RATE_18M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_18M_BIT\n"));
		break;

	case DRV_RATE_24M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_24M_BIT\n"));
		break;

	case DRV_RATE_36M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_36M_BIT\n"));
		break;

	case DRV_RATE_48M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_48M_BIT\n"));
		break;

	case DRV_RATE_54M:
		WLAN_OS_REPORT(("Max Basic Rate		RATE_54M_BIT\n"));
		break;

	default:
		WLAN_OS_REPORT(("Invalid basic rate value	0x%X\n", pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxBasic));
		break;
	}

	switch (pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxActive)
	{
	case DRV_RATE_1M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_1M_BIT\n"));
		break;

	case DRV_RATE_2M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_2M_BIT\n"));
		break;

	case DRV_RATE_5_5M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_5_5M_BIT\n"));
		break;

	case DRV_RATE_11M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_11M_BIT\n"));
		break;

	case DRV_RATE_22M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_22M_BIT\n"));
		break;

	case DRV_RATE_6M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_6M_BIT\n"));
		break;

	case DRV_RATE_9M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_9M_BIT\n"));
		break;

	case DRV_RATE_12M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_12M_BIT\n"));
		break;

	case DRV_RATE_18M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_18M_BIT\n"));
		break;

	case DRV_RATE_24M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_24M_BIT\n"));
		break;

	case DRV_RATE_36M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_36M_BIT\n"));
		break;

	case DRV_RATE_48M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_48M_BIT\n"));
		break;

	case DRV_RATE_54M:
		WLAN_OS_REPORT(("Max Active Rate		RATE_54M_BIT\n"));
		break;

	default:
		WLAN_OS_REPORT(("Invalid basic rate value	0x%X\n", pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxActive));
		break;
	}

	if (pSiteMgr->pDesiredParams->siteMgrDesiredModulationType == DRV_MODULATION_QPSK)
		WLAN_OS_REPORT(("Modulation Type		QPSK\n"));
	else if (pSiteMgr->pDesiredParams->siteMgrDesiredModulationType == DRV_MODULATION_CCK)
		WLAN_OS_REPORT(("Modulation Type		CCK\n"));
	else if (pSiteMgr->pDesiredParams->siteMgrDesiredModulationType == DRV_MODULATION_PBCC)
		WLAN_OS_REPORT(("Modulation Type		PBCC\n"));
	else if (pSiteMgr->pDesiredParams->siteMgrDesiredModulationType == DRV_MODULATION_OFDM)
		WLAN_OS_REPORT(("Modulation Type		OFDM\n"));
	else
		WLAN_OS_REPORT(("Invalid Modulation Type	%d\n", pSiteMgr->pDesiredParams->siteMgrDesiredModulationType));


	WLAN_OS_REPORT(("\n*****************************************\n\n", NULL));

	param.paramType = RSN_EXT_AUTHENTICATION_MODE;
	cmdDispatch_GetParam(hCmdDispatch, &param);
	if (param.content.rsnDesiredAuthType == RSN_AUTH_OPEN)
		WLAN_OS_REPORT(("Authentication Type	Open System\n"));
	else if (param.content.rsnDesiredAuthType == RSN_AUTH_SHARED_KEY)
		WLAN_OS_REPORT(("Authentication Type	Shared Key\n"));
	else 
		WLAN_OS_REPORT(("Authentication Type	Invalid: %d\n", param.content.rsnDesiredAuthType));

	param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
	cmdDispatch_GetParam(hCmdDispatch, &param);
	if (param.content.rsnEncryptionStatus == TWD_CIPHER_NONE)
		WLAN_OS_REPORT(("WEP 				Off\n"));
	else if (param.content.rsnEncryptionStatus == TWD_CIPHER_WEP)
		WLAN_OS_REPORT(("WEP 				On\n"));
	else 
		WLAN_OS_REPORT(("WEP 		Invalid: %d\n", param.content.rsnEncryptionStatus));
		
	WLAN_OS_REPORT(("\n"));


	WLAN_OS_REPORT(("\n*****************************************\n\n", NULL));
	if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_B_MODE)
		WLAN_OS_REPORT(("Desired dot11mode		11b\n"));
	else if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_G_MODE)
		WLAN_OS_REPORT(("Desired dot11mode		11g\n"));
	else if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_A_MODE)
		WLAN_OS_REPORT(("Desired dot11mode		11a\n"));
	else if(pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_DUAL_MODE)
		WLAN_OS_REPORT(("Desired dot11mode		dual 11a/g\n"));
	else 
		WLAN_OS_REPORT(("Desired dot11mode		INVALID\n"));

	WLAN_OS_REPORT(("\n*****************************************\n\n", NULL));
	if(pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime == PHY_SLOT_TIME_SHORT)
		WLAN_OS_REPORT(("Desired slot time		short\n"));
	else if(pSiteMgr->pDesiredParams->siteMgrDesiredSlotTime == PHY_SLOT_TIME_LONG)
		WLAN_OS_REPORT(("Desired slot time		long\n"));
	else
		WLAN_OS_REPORT(("Desired slot time		INVALID\n"));


	WLAN_OS_REPORT(("\n*****************************************\n\n", NULL));
	if (pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType == PREAMBLE_SHORT)
		WLAN_OS_REPORT(("Desired Preamble		Short\n"));
	else if (pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType == PREAMBLE_LONG)
		WLAN_OS_REPORT(("Desired Preamble	Long\n"));
	else
		WLAN_OS_REPORT(("Invalid Desired Preamble	%d\n", pSiteMgr->pDesiredParams->siteMgrDesiredPreambleType));

	WLAN_OS_REPORT(("Beacon interval		%d\n", pSiteMgr->pDesiredParams->siteMgrDesiredBeaconInterval));

	WLAN_OS_REPORT(("\n*****************************************", NULL));
	WLAN_OS_REPORT(("*****************************************\n\n", NULL));

}



static void printPrimarySiteDesc(siteMgr_t *pSiteMgr, OS_802_11_BSSID *pPrimarySiteDesc)
{
	TI_UINT8 rateIndex, maxNumOfRates;
	char ssid[MAX_SSID_LEN + 1];

	
	WLAN_OS_REPORT(("\n^^^^^^^^^^^^^^^	PRIMARY SITE DESCRIPTION	^^^^^^^^^^^^^^^^^^^\n\n")); 

	
	/* MacAddress */		
	WLAN_OS_REPORT(("BSSID				0x%X-0x%X-0x%X-0x%X-0x%X-0x%X\n",	pPrimarySiteDesc->MacAddress[0], 
																	pPrimarySiteDesc->MacAddress[1], 
																	pPrimarySiteDesc->MacAddress[2], 
																	pPrimarySiteDesc->MacAddress[3], 
																	pPrimarySiteDesc->MacAddress[4], 
																	pPrimarySiteDesc->MacAddress[5])); 

	/* Capabilities */
	WLAN_OS_REPORT(("Capabilities		0x%X\n",	pPrimarySiteDesc->Capabilities)); 

	/* SSID */
	os_memoryCopy(pSiteMgr->hOs, ssid, (void *)pPrimarySiteDesc->Ssid.Ssid, pPrimarySiteDesc->Ssid.SsidLength);
	ssid[pPrimarySiteDesc->Ssid.SsidLength] = 0;
	WLAN_OS_REPORT(("SSID				%s\n", ssid));

	/* privacy */
	if (pPrimarySiteDesc->Privacy == TI_TRUE)
		WLAN_OS_REPORT(("Privacy				ON\n"));
	else
		WLAN_OS_REPORT(("Privacy				OFF\n"));

	/* RSSI */				
	WLAN_OS_REPORT(("RSSI					%d\n", ((pPrimarySiteDesc->Rssi)>>16)));

	if (pPrimarySiteDesc->InfrastructureMode == os802_11IBSS)
		WLAN_OS_REPORT(("BSS Type				IBSS\n"));
	else
		WLAN_OS_REPORT(("BSS Type				INFRASTRUCTURE\n"));


	maxNumOfRates = sizeof(pPrimarySiteDesc->SupportedRates) / sizeof(pPrimarySiteDesc->SupportedRates[0]);
	/* SupportedRates */
	for (rateIndex = 0; rateIndex < maxNumOfRates; rateIndex++)
	{
		if (pPrimarySiteDesc->SupportedRates[rateIndex] != 0)
			WLAN_OS_REPORT(("Rate					0x%X\n", pPrimarySiteDesc->SupportedRates[rateIndex]));
	}

	WLAN_OS_REPORT(("\n---------------------------------------------------------------\n\n", NULL)); 

}

static void setRateSet(TI_UINT8 maxRate, TRates *pRates)
{
	TI_UINT8 i = 0;

	switch (maxRate)
	{

	case DRV_RATE_54M:
		pRates->ratesString[i] = 108;
		i++;

	case DRV_RATE_48M:
		pRates->ratesString[i] = 96;
		i++;

	case DRV_RATE_36M:
		pRates->ratesString[i] = 72;
		i++;

	case DRV_RATE_24M:
		pRates->ratesString[i] = 48;
		i++;

	case DRV_RATE_18M:
		pRates->ratesString[i] = 36;
		i++;

	case DRV_RATE_12M:
		pRates->ratesString[i] = 24;
		i++;

	case DRV_RATE_9M:
		pRates->ratesString[i] = 18;
		i++;

	case DRV_RATE_6M:
		pRates->ratesString[i] = 12;
		i++;

	case DRV_RATE_22M:
		pRates->ratesString[i] = 44;
		i++;

	case DRV_RATE_11M:
		pRates->ratesString[i] = 22;
		pRates->ratesString[i] |= 0x80;
		i++;

	case DRV_RATE_5_5M:
		pRates->ratesString[i] = 11;
		pRates->ratesString[i] |= 0x80;
		i++;

	case DRV_RATE_2M:
		pRates->ratesString[i] = 4;
		pRates->ratesString[i] |= 0x80;
		i++;

	case DRV_RATE_1M:
		pRates->ratesString[i] = 2;
		pRates->ratesString[i] |= 0x80;
		i++;
		break;

	default:
		WLAN_OS_REPORT(("Set Rate Set, invalid max rate %d\n", maxRate));
		pRates->len = 0;
	}

	pRates->len = i;

}

void printSiteMgrHelpMenu(void)
{
	WLAN_OS_REPORT(("\n\n   Site Manager Debug Menu   \n"));
	WLAN_OS_REPORT(("------------------------\n"));

	WLAN_OS_REPORT(("500 - Help.\n"));
	WLAN_OS_REPORT(("501 - Primary Site Parameters.\n"));
	WLAN_OS_REPORT(("502 - Sites List.\n"));
	WLAN_OS_REPORT(("503 - Desired Parameters.\n"));
	WLAN_OS_REPORT(("507 - Set Power save Mode.\n"));
	WLAN_OS_REPORT(("508 - Get Power save Mode.\n"));
	WLAN_OS_REPORT(("511 - Set Default Key Id.\n"));
	WLAN_OS_REPORT(("512 - Get Default Key Id.\n"));
	WLAN_OS_REPORT(("513 - Set Key.\n"));
	WLAN_OS_REPORT(("514 - Get Key.\n"));
	WLAN_OS_REPORT(("515 - Set Cypher Suite.\n"));
	WLAN_OS_REPORT(("516 - Get Cypher Suite.\n"));
	WLAN_OS_REPORT(("517 - Set Auth Mode.\n"));
	WLAN_OS_REPORT(("518 - Get Auth Mode.\n"));
	WLAN_OS_REPORT(("519 - Get Primary Site Description.\n"));
	WLAN_OS_REPORT(("520 - Get Connection Status.\n"));
	WLAN_OS_REPORT(("522 - Get Current Tx Rate.\n"));
	WLAN_OS_REPORT(("525 - Set Supported Rate Set.\n"));
	WLAN_OS_REPORT(("526 - Get Supported Rate Set.\n"));
	WLAN_OS_REPORT(("527 - Set Auth type.\n"));
	WLAN_OS_REPORT(("528 - Get Auth type.\n"));

	WLAN_OS_REPORT(("        %03d - RADIO_STAND_BY_CHANGE_STATE \n", RADIO_STAND_BY_CHANGE_STATE));
	WLAN_OS_REPORT(("        %03d - CONNECT_TO_BSSID \n", CONNECT_TO_BSSID));

	WLAN_OS_REPORT(("        %03d - SET_START_CLI_SCAN_PARAM \n", SET_START_CLI_SCAN_PARAM));
	WLAN_OS_REPORT(("        %03d - SET_STOP_CLI_SCAN_PARAM \n", SET_STOP_CLI_SCAN_PARAM));

	WLAN_OS_REPORT(("        %03d - SET_BROADCAST_BACKGROUND_SCAN_PARAM \n", SET_BROADCAST_BACKGROUND_SCAN_PARAM));
	WLAN_OS_REPORT(("        %03d - ENABLE_PERIODIC_BROADCAST_BBACKGROUND_SCAN_PARAM \n", ENABLE_PERIODIC_BROADCAST_BACKGROUND_SCAN_PARAM));
	WLAN_OS_REPORT(("        %03d - DISABLE_PERIODIC_BROADCAST_BACKGROUND_SCAN_PARAM \n", DISABLE_PERIODIC_BROADCAST_BACKGROUND_SCAN_PARAM));

	WLAN_OS_REPORT(("        %03d - SET_UNICAST_BACKGROUND_SCAN_PARAM \n", SET_UNICAST_BACKGROUND_SCAN_PARAM));
	WLAN_OS_REPORT(("        %03d - ENABLE_PERIODIC_UNICAST_BACKGROUND_SCAN_PARAM \n", ENABLE_PERIODIC_UNICAST_BACKGROUND_SCAN_PARAM));
	WLAN_OS_REPORT(("        %03d - DISABLE_PERIODIC_UNICAST_BACKGROUND_SCAN_PARAM \n", DISABLE_PERIODIC_UNICAST_BACKGROUND_SCAN_PARAM));

	WLAN_OS_REPORT(("        %03d - SET_FOREGROUND_SCAN_PARAM \n", SET_FOREGROUND_SCAN_PARAM));
	WLAN_OS_REPORT(("        %03d - ENABLE_PERIODIC_FOREGROUND_SCAN_PARAM \n", ENABLE_PERIODIC_FOREGROUND_SCAN_PARAM));
	WLAN_OS_REPORT(("        %03d - DISABLE_PERIODIC_FOREGROUND_SCAN_PARAM \n", DISABLE_PERIODIC_FOREGROUND_SCAN_PARAM));
	
	WLAN_OS_REPORT(("        %03d - SET_CHANNEL_NUMBER \n", SET_CHANNEL_NUMBER));
	WLAN_OS_REPORT(("        %03d - SET_RSSI_GAP_THRSH \n", SET_RSSI_GAP_THRSH));
	WLAN_OS_REPORT(("        %03d - SET_FAST_SCAN_TIMEOUT \n", SET_FAST_SCAN_TIMEOUT));
	WLAN_OS_REPORT(("        %03d - SET_INTERNAL_ROAMING_ENABLE \n", SET_INTERNAL_ROAMING_ENABLE));

	WLAN_OS_REPORT(("        %03d - PERFORM_HEALTH_TEST \n", PERFORM_HEALTH_TEST));
	WLAN_OS_REPORT(("        %03d - PRINT_FAILURE_EVENTS \n", PRINT_FAILURE_EVENTS));
	WLAN_OS_REPORT(("        %03d - FORCE_HW_RESET_RECOVERY \n", FORCE_HW_RESET_RECOVERY));
	WLAN_OS_REPORT(("        %03d - FORCE_SOFT_RECOVERY \n", FORCE_SOFT_RECOVERY));

	WLAN_OS_REPORT(("        %03d - RESET_ROAMING_EVENTS \n", RESET_ROAMING_EVENTS));
	WLAN_OS_REPORT(("        %03d - SET_DESIRED_CONS_TX_ERRORS_THREH\n", SET_DESIRED_CONS_TX_ERRORS_THREH));
	
	WLAN_OS_REPORT(("        %03d - GET_CURRENT_ROAMING_STATUS \n", GET_CURRENT_ROAMING_STATUS));
	

    WLAN_OS_REPORT(("        %03d - TOGGLE_LNA_ON \n", TEST_TOGGLE_LNA_ON));
    WLAN_OS_REPORT(("        %03d - TOGGLE_LNA_OFF \n", TEST_TOGGLE_LNA_OFF));

	WLAN_OS_REPORT(("        %03d - PRINT_SITE_TABLE_PER_SSID\n", PRINT_SITE_TABLE_PER_SSID));
	
	WLAN_OS_REPORT(("        %03d - SET_DESIRED_CHANNEL\n", SET_DESIRED_CHANNEL));
	
	WLAN_OS_REPORT(("        %03d - START_PRE_AUTH\n", START_PRE_AUTH));

	WLAN_OS_REPORT(("\n------------------------\n"));
}



