/*
 * roamingMgrDebug.c
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

/***************************************************************************/
/*																									*/
/*		MODULE:	reportReplvl.c						 										*/
/*    PURPOSE:	Report level implementation	 										*/
/*																									*/
/***************************************************************************/
#include "tidef.h"
#include "report.h"
#include "paramOut.h"
#include "roamingMgrDebug.h"
#include "roamingMngrApi.h"
#include "apConnApi.h"


void printRoamingMgrHelpMenu(void);
void PrintBssListGotAfterImemediateScan(TI_HANDLE hScanMgr);


/*	Function implementation */
void roamingMgrDebugFunction(TI_HANDLE hRoamingMngr, 
					   TI_UINT32	funcType, 
					   void		*pParam)
{
	paramInfo_t			param;

	
	switch (funcType)
	{
	case ROAMING_MGR_DEBUG_HELP_MENU:
		printRoamingMgrHelpMenu();
		break;

	case PRINT_ROAMING_STATISTICS:
		param.paramType = ROAMING_MNGR_PRINT_STATISTICS;
		roamingMngr_getParam(hRoamingMngr, &param);
		break;

	case RESET_ROAMING_STATISTICS:
		param.paramType = ROAMING_MNGR_RESET_STATISTICS;
		roamingMngr_getParam(hRoamingMngr, &param);
		break;

	case PRINT_ROAMING_CURRENT_STATUS:
		param.paramType = ROAMING_MNGR_PRINT_CURRENT_STATUS;
		roamingMngr_getParam(hRoamingMngr, &param);
		break;

	case PRINT_ROAMING_CANDIDATE_TABLE:
		param.paramType = ROAMING_MNGR_PRINT_CANDIDATE_TABLE;
		roamingMngr_getParam(hRoamingMngr, &param);
		break;

	case TRIGGER_ROAMING_LOW_QUALITY_EVENT:
		param.paramType = ROAMING_MNGR_TRIGGER_EVENT;
		param.content.roamingTriggerType = ROAMING_TRIGGER_LOW_QUALITY;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

    case TRIGGER_ROAMING_BSS_LOSS_EVENT:
		param.paramType = ROAMING_MNGR_TRIGGER_EVENT;
		param.content.roamingTriggerType = ROAMING_TRIGGER_BSS_LOSS;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

	case TRIGGER_ROAMING_SWITCH_CHANNEL_EVENT:
		param.paramType = ROAMING_MNGR_TRIGGER_EVENT;
		param.content.roamingTriggerType = ROAMING_TRIGGER_SWITCH_CHANNEL;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

	case TRIGGER_ROAMING_AP_DISCONNECT_EVENT:
		param.paramType = ROAMING_MNGR_TRIGGER_EVENT;
		param.content.roamingTriggerType = ROAMING_TRIGGER_AP_DISCONNECT;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

	case TRIGGER_ROAMING_CONNECT_EVENT:
		param.paramType = ROAMING_MNGR_CONN_STATUS;
		param.content.roamingConnStatus = CONN_STATUS_CONNECTED;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

	case TRIGGER_ROAMING_NOT_CONNECTED_EVENT:
		param.paramType = ROAMING_MNGR_CONN_STATUS;
		param.content.roamingConnStatus = CONN_STATUS_NOT_CONNECTED;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

	case TRIGGER_ROAMING_HANDOVER_SUCCESS_EVENT:
		param.paramType = ROAMING_MNGR_CONN_STATUS;
		param.content.roamingConnStatus = CONN_STATUS_HANDOVER_SUCCESS;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

	case TRIGGER_ROAMING_HANDOVER_FAILURE_EVENT:
		param.paramType = ROAMING_MNGR_CONN_STATUS;
		param.content.roamingConnStatus = CONN_STATUS_HANDOVER_FAILURE;
		roamingMngr_setParam(hRoamingMngr, &param);
		break;

    case ROAMING_REGISTER_BSS_LOSS_EVENT: /* 1613 */
        roamingMngr_setBssLossThreshold(hRoamingMngr, 10, 1);
        break;
    case ROAMING_START_IMMEDIATE_SCAN: /* 1614 */
        {
            int i=0,j =0;
            channelList_t channels;
            channels.numOfChannels = 14;


            for ( i = 0; i < channels.numOfChannels; i++ )
            {
                for ( j = 0; j < 6; j++ )
                {
                    channels.channelEntry[i].normalChannelEntry.bssId[j] = 0xff;
                }

                channels.channelEntry[i].normalChannelEntry.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
                channels.channelEntry[i].normalChannelEntry.ETMaxNumOfAPframes = 0;
                channels.channelEntry[i].normalChannelEntry.maxChannelDwellTime = 60000;
                channels.channelEntry[i].normalChannelEntry.minChannelDwellTime = 30000;
                channels.channelEntry[i].normalChannelEntry.txPowerDbm = DEF_TX_POWER;
                channels.channelEntry[i].normalChannelEntry.channel = i + 1;
            }

            /* upon this call the scanMngr_reportImmediateScanResults() should be invoked and the BssList should be printed */
            roamingMngr_startImmediateScan(hRoamingMngr, &channels); 
        }

        break;
    case ROAMING_CONNECT: /* 1615 */
        {
            TargetAp_t targetAP;
            bssList_t *bssList;
            roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;

            bssList = scanMngr_getBSSList(((roamingMngr_t*)hRoamingMngr)->hScanMngr);

            WLAN_OS_REPORT(("Roaming connect: BSS LIST num of entries=%d \n", bssList->numOfEntries));
            PrintBssListGotAfterImemediateScan(((roamingMngr_t*)hRoamingMngr)->hScanMngr);

            /* The values below must be configured in manual mode */
            targetAP.connRequest.requestType = AP_CONNECT_FULL_TO_AP;
            targetAP.connRequest.dataBufLength = 0;
            targetAP.transitionMethod = ReAssoc;

            os_memoryCopy(((roamingMngr_t*)hRoamingMngr)->hOs, &(targetAP.newAP), &(bssList->BSSList[0]), sizeof(bssEntry_t));

            /* test if no buffer is present */
            targetAP.newAP.bufferLength =0;
            targetAP.newAP.pBuffer = 0;
            /* ----------------------------- */

            os_memoryCopy(pRoamingMngr->hOs, &(pRoamingMngr->targetAP), &targetAP , sizeof(TargetAp_t));

            roamingMngr_connect(hRoamingMngr, &(pRoamingMngr->targetAP));
        }

        break;

    case ROAMING_START_CONT_SCAN_BY_APP: /* 1616 */
        {
            roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
            int i=0,j =0;
            channelList_t channels;
            channels.numOfChannels = 14;

            for ( i = 0; i < channels.numOfChannels; i++ )
            {
                for ( j = 0; j < 6; j++ )
                {
                    channels.channelEntry[i].normalChannelEntry.bssId[j] = 0xff;
                }

                channels.channelEntry[i].normalChannelEntry.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
                channels.channelEntry[i].normalChannelEntry.ETMaxNumOfAPframes = 0;
                channels.channelEntry[i].normalChannelEntry.maxChannelDwellTime = 60000;
                channels.channelEntry[i].normalChannelEntry.minChannelDwellTime = 30000;
                channels.channelEntry[i].normalChannelEntry.txPowerDbm = DEF_TX_POWER;
                channels.channelEntry[i].normalChannelEntry.channel = i + 1;
            }

            scanMngr_startContinuousScanByApp(pRoamingMngr->hScanMngr, &channels);
        }

        break;

    case ROAMING_STOP_CONT_SCAN_BY_APP:
        {
            roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
            scanMngr_stopContinuousScanByApp(pRoamingMngr->hScanMngr);
        }
        break;


    case RAOMING_SET_DEFAULT_SCAN_POLICY: /* 1618 */
        {
            int i=0;
            roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
            TScanPolicy scanPolicy;
            param.paramType = SCAN_MNGR_SET_CONFIGURATION;
            param.content.pScanPolicy = &scanPolicy;

            // init default scan policy
            scanPolicy.normalScanInterval = 10000;
            scanPolicy.deterioratingScanInterval = 5000;
            scanPolicy.maxTrackFailures = 3;
            scanPolicy.BSSListSize = 4;
            scanPolicy.BSSNumberToStartDiscovery = 1;
            scanPolicy.numOfBands = 1;
            scanPolicy.bandScanPolicy[ 0 ].band = RADIO_BAND_2_4_GHZ;
            scanPolicy.bandScanPolicy[ 0 ].rxRSSIThreshold = -80;
            scanPolicy.bandScanPolicy[ 0 ].numOfChannles = 14;
            scanPolicy.bandScanPolicy[ 0 ].numOfChannlesForDiscovery = 3;

            for ( i = 0; i < 14; i++ )
            {
                scanPolicy.bandScanPolicy[ 0 ].channelList[ i ] = i + 1;
            }

            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.scanType = SCAN_TYPE_NORMAL_ACTIVE;
            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.maxChannelDwellTime = 30000;
            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.minChannelDwellTime = 15000;
            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.bitrate = RATE_MASK_UNSPECIFIED; /* Let the FW select */
            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 3;
            scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.txPowerDbm = DEF_TX_POWER;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.scanType = SCAN_TYPE_NORMAL_ACTIVE;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.maxChannelDwellTime = 30000;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.minChannelDwellTime = 15000;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.bitrate = RATE_MASK_UNSPECIFIED; /* Let the FW select */;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 3;
            scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.txPowerDbm = DEF_TX_POWER;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.scanType = SCAN_TYPE_NORMAL_ACTIVE;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.maxChannelDwellTime = 30000;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.minChannelDwellTime = 15000;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.probReqParams.bitrate = RATE_MASK_UNSPECIFIED; /* Let the FW select */;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 3;
            scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.probReqParams.txPowerDbm = DEF_TX_POWER;
             /* we should noe store the scanPolicy now */
            scanMngr_setParam(pRoamingMngr->hScanMngr, &param);


            /* Enable roaming! */
            param.paramType = ROAMING_MNGR_APPLICATION_CONFIGURATION;
            param.content.roamingConfigBuffer.roamingMngrConfig.enableDisable = ROAMING_ENABLED;
            roamingMngr_setParam(hRoamingMngr,&param);
        }

        break;

    case ROAMING_PRINT_MANUAL_MODE: /* 1617 */
        WLAN_OS_REPORT(("\n ROAMING MANUAL MODE IS: %d \n",((roamingMngr_t*)hRoamingMngr)->RoamingOperationalMode));
        break;

	default:
		WLAN_OS_REPORT(("Invalid function type in Debug  Function Command, funcType= %d\n\n", funcType));
		break;
	}
} 


void printRoamingMgrHelpMenu(void)
{
	WLAN_OS_REPORT(("\n\n   Roaming Manager Debug Menu   \n"));
	WLAN_OS_REPORT(("------------------------\n"));


	WLAN_OS_REPORT(("        %02d - ROAMING_MGR_DEBUG_HELP_MENU \n", ROAMING_MGR_DEBUG_HELP_MENU));

	WLAN_OS_REPORT(("        %02d - PRINT_ROAMING_STATISTICS \n", PRINT_ROAMING_STATISTICS));
	WLAN_OS_REPORT(("        %02d - RESET_ROAMING_STATISTICS \n", RESET_ROAMING_STATISTICS));

	WLAN_OS_REPORT(("        %02d - PRINT_ROAMING_CURRENT_STATUS \n", PRINT_ROAMING_CURRENT_STATUS));
	WLAN_OS_REPORT(("        %02d - PRINT_ROAMING_CANDIDATE_TABLE \n", PRINT_ROAMING_CANDIDATE_TABLE));

	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_LOW_QUALITY_EVENT \n", TRIGGER_ROAMING_LOW_QUALITY_EVENT));
	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_BSS_LOSS_EVENT \n", TRIGGER_ROAMING_BSS_LOSS_EVENT));
	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_SWITCH_CHANNEL_EVENT \n", TRIGGER_ROAMING_SWITCH_CHANNEL_EVENT));
	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_AP_DISCONNECT_EVENT \n", TRIGGER_ROAMING_AP_DISCONNECT_EVENT));

	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_CONNECT_EVENT \n", TRIGGER_ROAMING_CONNECT_EVENT));
	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_NOT_CONNECTED_EVENT \n", TRIGGER_ROAMING_NOT_CONNECTED_EVENT));

	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_HANDOVER_SUCCESS_EVENT \n", TRIGGER_ROAMING_HANDOVER_SUCCESS_EVENT));
	WLAN_OS_REPORT(("        %02d - TRIGGER_ROAMING_HANDOVER_FAILURE_EVENT \n", TRIGGER_ROAMING_HANDOVER_FAILURE_EVENT));
	

	WLAN_OS_REPORT(("\n------------------------\n"));
}

void PrintBssListGotAfterImemediateScan(TI_HANDLE hScanMgr)
{
    bssList_t *bssList;
    bssEntry_t* pBssEntry;
    int i=0;

    WLAN_OS_REPORT(("------ PRINTING BSS FOUND AFTER IMMEDIATE SCAN - MANUAL MODE----------\n"));

    bssList = scanMngr_getBSSList(hScanMgr);

    for (i=0 ; i< bssList->numOfEntries ; i++)
    {
        pBssEntry = &(bssList->BSSList[i]);

        WLAN_OS_REPORT( ("BSSID: %02x:%02x:%02x:%02x:%02x:%02x, band: %d\n", pBssEntry->BSSID[ 0 ],
                         pBssEntry->BSSID[ 1 ], pBssEntry->BSSID[ 2 ],
                         pBssEntry->BSSID[ 3 ], pBssEntry->BSSID[ 4 ],
                         pBssEntry->BSSID[ 5 ], pBssEntry->band));
       WLAN_OS_REPORT( ("channel: %d, beacon interval: %d, average RSSI: %d dBm\n",
                                      pBssEntry->channel, pBssEntry->beaconInterval, pBssEntry->RSSI));

    }

    WLAN_OS_REPORT(("-----------------------------------------------------------------------\n"));

}
