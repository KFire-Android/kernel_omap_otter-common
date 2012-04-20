/*
 * CmdBldCmd.c
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


/** \file  CmdBldCmd.c 
 *  \brief Command builder. Commands
 *
 *  \see   CmdBld.h 
 */

#define __FILE_ID__  FILE_ID_93
#include "tidef.h"
#include "report.h"
#include "TWDriverInternal.h"
#include "CmdBld.h"
#include "CmdBldCmdIE.h"
#include "CmdBldCfgIE.h"
#include "CmdQueue_api.h"
#include "eventMbox_api.h"

/* 
    Rx filter field is mostly hard-coded.
   This filter value basically pass only valid beacons / probe responses. For exact bit description,
   consult either the DPG or the FPG (or both, and Yoel...)
*/
#define RX_FILTER_CFG_ (CFG_RX_PRSP_EN | CFG_RX_MGMT_EN | CFG_RX_BCN_EN | CFG_RX_RCTS_ACK | CFG_RX_CTL_EN)
#define RX_CONFIG_OPTION (CFG_RX_RAW | CFG_RX_INT_FCS_ERROR | CFG_RX_WR_RX_STATUS | CFG_RX_TIMESTAMP_TSF)



TI_STATUS cmdBld_CmdAddWepMappingKey 	(TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_CmdRemoveWepMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb);
TI_UINT32 cmdBld_BuildPeriodicScanChannles  (TPeriodicScanParams *pPeriodicScanParams, ConnScanChannelInfo_t *pChannelList, EScanType eScanType, ERadioBand eRadioBand, TI_UINT32 uPassiveScanDfsDwellTime);


/** 
 * \fn     cmdBld_CmdStartScan
 * \brief  Build a start scan command and send it to the FW
 * 
 * Build a start scan command and send it to the FW
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  pScanVals - scan parameters
 * \param  eScanTag - scan tag used for scan complete and result tracking
 * \param  fScanCommandResponseCB - command complete CB
 * \param  hCb - command complete CB
 * \return command status (OK / NOK)
 * \sa     cmdBld_CmdStopScan
 */ 
TI_STATUS cmdBld_CmdStartScan (TI_HANDLE hCmdBld, TScanParams *pScanVals, EScanResultTag eScanTag, 
                               TI_BOOL bHighPriority, void* ScanCommandResponseCB, TI_HANDLE hCb)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;
    BasicScanChannelParameters_t* chanPtr;
    ScanParameters_t    tnetScanParams;
    TI_UINT8*              pBSSID;
    TI_INT32 i;

   
    /* Convert general scan data to tnet structure */
    tnetScanParams.basicScanParameters.tidTrigger = pScanVals->Tid;
    tnetScanParams.basicScanParameters.numOfProbRqst = pScanVals->probeReqNumber;
    tnetScanParams.basicScanParameters.ssidLength = pScanVals->desiredSsid.len;
    os_memoryCopy (pCmdBld->hOs, 
                   (void *)tnetScanParams.basicScanParameters.ssidStr, 
                   (void *)pScanVals->desiredSsid.str, 
                   tnetScanParams.basicScanParameters.ssidLength);
    
    /* 
        scan options field is composed of scan type and band selection. 
        First, use the lookup table to convert the scan type 
    */                  

    tnetScanParams.basicScanParameters.scanOptions = 0;

    switch ( pScanVals->scanType )
    {
    case SCAN_TYPE_NORMAL_ACTIVE : 
        tnetScanParams.basicScanParameters.scanOptions = SCAN_ACTIVE;
        break;
    
    case SCAN_TYPE_NORMAL_PASSIVE : 
        tnetScanParams.basicScanParameters.scanOptions = SCAN_PASSIVE;
        break;
    
    case SCAN_TYPE_TRIGGERED_ACTIVE :
        tnetScanParams.basicScanParameters.scanOptions = SCAN_ACTIVE | TRIGGERED_SCAN;
        break;
    
    case SCAN_TYPE_TRIGGERED_PASSIVE :
        tnetScanParams.basicScanParameters.scanOptions = SCAN_PASSIVE | TRIGGERED_SCAN;
        break;

    default:
        TRACE1( pCmdBld->hReport, REPORT_SEVERITY_ERROR, "Invalid scan type:%d\n", pScanVals->scanType);
        return TI_NOK;
    }

    /* Add the band selection */
    if ( RADIO_BAND_5_0_GHZ == pScanVals->band )
    {
        tnetScanParams.basicScanParameters.band = RADIO_BAND_5GHZ;
    }
    else
    {
        tnetScanParams.basicScanParameters.band = RADIO_BAND_2_4_GHZ;
    }

    /* Add high priority bit */
    if ( bHighPriority )
    {
        tnetScanParams.basicScanParameters.scanOptions |= SCAN_PRIORITY_HIGH;
    }

    tnetScanParams.basicScanParameters.scanOptions = ENDIAN_HANDLE_WORD( tnetScanParams.basicScanParameters.scanOptions );

    /* important note: BSSID filter (0x0010) is DISABLED, because the FW sets it according
       to BSSID value (broadcast does not filter, any other value will */
    tnetScanParams.basicScanParameters.rxCfg.ConfigOptions = ENDIAN_HANDLE_LONG(RX_CONFIG_OPTION) ;
    tnetScanParams.basicScanParameters.rxCfg.FilterOptions = ENDIAN_HANDLE_LONG( RX_FILTER_CFG_ );

    /* If the SSID is not broadcast SSID, filter according to SSID and local MAC address */
    if (pScanVals->desiredSsid.len != 0)
    {
        tnetScanParams.basicScanParameters.rxCfg.ConfigOptions = ENDIAN_HANDLE_LONG(RX_CONFIG_OPTION | CFG_SSID_FILTER_EN | CFG_UNI_FILTER_EN) ;
    }
    /* Rate conversion is done in the HAL */
    cmdBld_ConvertAppRatesBitmap (pScanVals->probeRequestRate, 
                                     0, 
                                     &tnetScanParams.basicScanParameters.txdRateSet);

    tnetScanParams.basicScanParameters.txdRateSet = ENDIAN_HANDLE_LONG( tnetScanParams.basicScanParameters.txdRateSet );
    tnetScanParams.basicScanParameters.numChannels = ENDIAN_HANDLE_WORD( pScanVals->numOfChannels );

    /* scan result tag */
    tnetScanParams.basicScanParameters.scanTag = eScanTag;

    /* copy channel specific scan data to HAL structure */
    for ( i = 0; i < pScanVals->numOfChannels; i++ )
    {
        TI_INT32 j;
        TI_UINT8*  macAddr;

        macAddr = (TI_UINT8*)&tnetScanParams.basicScanChannelParameters[ i ].bssIdL;

        /* copy the MAC address, upside down (CHIP structure) */
        for ( j = 0; j < MAC_ADDR_LEN; j++ )
        {
            macAddr[ j ] = pScanVals->channelEntry[ i ].normalChannelEntry.bssId[ MAC_ADDR_LEN - 1 - j ];
        }
        tnetScanParams.basicScanChannelParameters[ i ].scanMinDuration = 
            ENDIAN_HANDLE_LONG( pScanVals->channelEntry[ i ].normalChannelEntry.minChannelDwellTime );
        tnetScanParams.basicScanChannelParameters[ i ].scanMaxDuration = 
            ENDIAN_HANDLE_LONG( pScanVals->channelEntry[ i ].normalChannelEntry.maxChannelDwellTime );
        tnetScanParams.basicScanChannelParameters[ i ].ETCondCount = 
            pScanVals->channelEntry[ i ].normalChannelEntry.ETMaxNumOfAPframes |
            pScanVals->channelEntry[ i ].normalChannelEntry.earlyTerminationEvent;
        tnetScanParams.basicScanChannelParameters[ i ].txPowerAttenuation = 
            pScanVals->channelEntry[ i ].normalChannelEntry.txPowerDbm;
        tnetScanParams.basicScanChannelParameters[ i ].channel = 
            pScanVals->channelEntry[ i ].normalChannelEntry.channel;
    }

    TRACE7(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "RxCfg = 0x%x\n                             RxFilterCfg = 0x%x\n                             scanOptions = 0x%x\n                             numChannels = %d\n                             probeNumber = %d\n                             probeRateModulation = 0x%x\n                             tidTrigger = %d\n" ,                               tnetScanParams.basicScanParameters.rxCfg.ConfigOptions,                               tnetScanParams.basicScanParameters.rxCfg.FilterOptions,                              tnetScanParams.basicScanParameters.scanOptions,                               tnetScanParams.basicScanParameters.numChannels,                               tnetScanParams.basicScanParameters.numOfProbRqst,                              tnetScanParams.basicScanParameters.txdRateSet,                               tnetScanParams.basicScanParameters.tidTrigger);
    
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Channel      BSSID           MinTime     MaxTime     ET     TxPower   probChan\n");
   
    for( i=0; i < pScanVals->numOfChannels; i++)
    {
        chanPtr = &tnetScanParams.basicScanChannelParameters[i];
        pBSSID = (TI_UINT8*)&chanPtr->bssIdL;

 		TRACE12(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "%06d   %02x:%02x:%02x:%02x:%02x:%02x    %05d %05d %02d %05d %05d\n",i, pBSSID[5],pBSSID[4],pBSSID[3],pBSSID[2],pBSSID[1],pBSSID[0], chanPtr->scanMinDuration, chanPtr->scanMaxDuration, chanPtr->ETCondCount, chanPtr->txPowerAttenuation, chanPtr->channel);
    }

    return cmdBld_CmdIeStartScan (hCmdBld, &tnetScanParams, ScanCommandResponseCB, hCb);
}

/** 
 * \fn     cmdBld_CmdStartSPSScan
 * \brief  Build a start SPS scan command and send it to the FW
 * 
 * Build a start SPS scan command and send it to the FW
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  pScanVals - scan parameters
 * \param  eScanTag - scan tag used for scan complete and result tracking
 * \param  fScanCommandResponseCB - command complete CB
 * \param  hCb - command complete CB
 * \return command status (OK / NOK)
 * \sa     cmdBld_CmdStopSPSScan
 */ 
TI_STATUS cmdBld_CmdStartSPSScan (TI_HANDLE hCmdBld, TScanParams *pScanVals, EScanResultTag eScanTag, 
                                  void* fScanCommandResponseCB, TI_HANDLE hCb)
{
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    ScheduledScanParameters_t   tnetSPSScanParams;
    TI_INT32 i;

    /* Convert general scan data to TNET structure */
    tnetSPSScanParams.scheduledGeneralParameters.scanOptions = SCAN_PASSIVE;
    /* Add the band selection */
    if ( RADIO_BAND_5_0_GHZ == pScanVals->band )
    {
        tnetSPSScanParams.scheduledGeneralParameters.band = RADIO_BAND_5GHZ;
    }
    else
    {
        tnetSPSScanParams.scheduledGeneralParameters.band = RADIO_BAND_2_4_GHZ;
    }


    tnetSPSScanParams.scheduledGeneralParameters.scanOptions = ENDIAN_HANDLE_WORD( tnetSPSScanParams.scheduledGeneralParameters.scanOptions );

    /* important note: BSSID filter (0x0010) is DISABLED, because the FW sets it according
       to BSSID value (broadcast does not filter, any other value will */
    /* If the SSID is not broadcast SSID, also filter according to SSID */
    tnetSPSScanParams.scheduledGeneralParameters.rxCfg.ConfigOptions = ENDIAN_HANDLE_LONG(RX_CONFIG_OPTION);
    tnetSPSScanParams.scheduledGeneralParameters.rxCfg.FilterOptions = ENDIAN_HANDLE_LONG( RX_FILTER_CFG_ );
    tnetSPSScanParams.scheduledGeneralParameters.rxCfg.ConfigOptions = ENDIAN_HANDLE_LONG( tnetSPSScanParams.scheduledGeneralParameters.rxCfg.ConfigOptions );

    /* latest TSF value - used to discover TSF error (AP recovery) */
    tnetSPSScanParams.scheduledGeneralParameters.scanCmdTime_h = ENDIAN_HANDLE_LONG( INT64_HIGHER(pScanVals->latestTSFValue) );
    tnetSPSScanParams.scheduledGeneralParameters.scanCmdTime_l = ENDIAN_HANDLE_LONG( INT64_LOWER(pScanVals->latestTSFValue) );

    /* add scan tag */
    tnetSPSScanParams.scheduledGeneralParameters.scanTag = eScanTag;

    tnetSPSScanParams.scheduledGeneralParameters.numChannels = pScanVals->numOfChannels;

    /* copy channel specific scan data to HAL structure */
    for ( i = 0; i < pScanVals->numOfChannels; i++ )
    {
        TI_INT32 j;
        TI_UINT8*  macAddr;

        macAddr = (TI_UINT8*)&tnetSPSScanParams.scheduledChannelParameters[ i ].bssIdL;

        /* copy the MAC address, upside down (CHIP structure) */
        for ( j = 0; j < MAC_ADDR_LEN; j++ )
        {
            macAddr[ j ] = pScanVals->channelEntry[ i ].normalChannelEntry.bssId[ MAC_ADDR_LEN - 1 - j ];
        }
        tnetSPSScanParams.scheduledChannelParameters[ i ].scanMaxDuration = 
            ENDIAN_HANDLE_LONG( pScanVals->channelEntry[ i ].SPSChannelEntry.scanDuration );
        tnetSPSScanParams.scheduledChannelParameters[ i ].scanStartTime = 
            ENDIAN_HANDLE_LONG( pScanVals->channelEntry[ i ].SPSChannelEntry.scanStartTime );
        tnetSPSScanParams.scheduledChannelParameters[ i ].ETCondCount =
            pScanVals->channelEntry[ i ].SPSChannelEntry.ETMaxNumOfAPframes | 
            pScanVals->channelEntry[ i ].SPSChannelEntry.earlyTerminationEvent;
        tnetSPSScanParams.scheduledChannelParameters[ i ].channel = 
            pScanVals->channelEntry[ i ].SPSChannelEntry.channel;
    }

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "RxCfg = 0x%x\n                             RxFilterCfg = 0x%x\n                             scanOptions = 0x%x\n                             numChannels = %d\n", tnetSPSScanParams.scheduledGeneralParameters.rxCfg.ConfigOptions, tnetSPSScanParams.scheduledGeneralParameters.rxCfg.FilterOptions, tnetSPSScanParams.scheduledGeneralParameters.scanOptions, tnetSPSScanParams.scheduledGeneralParameters.numChannels);
    
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Channel      BSSID           StartTime     Duration     ET     probChan\n");
   
#ifdef TI_DBG
    for( i=0; i < tnetSPSScanParams.scheduledGeneralParameters.numChannels; i++)
    {
        ScheduledChannelParameters_t* chanPtr = &tnetSPSScanParams.scheduledChannelParameters[ i ];
        TI_UINT8* pBSSID = (TI_UINT8*)&chanPtr->bssIdL;

        TRACE11(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "%6d   %02x:%02x:%02x:%02x:%02x:%02x    %5d %5d %2d %5d\n",i, pBSSID[5],pBSSID[4],pBSSID[3],pBSSID[2],pBSSID[1],pBSSID[0], chanPtr->scanStartTime, chanPtr->scanMaxDuration, chanPtr->ETCondCount, chanPtr->channel);
    }
#endif /* TI_DBG */

    return cmdBld_CmdIeStartSPSScan (hCmdBld, &tnetSPSScanParams, fScanCommandResponseCB, hCb);
}

/** 
 * \fn     cmdBld_CmdStopScan
 * \brief  Build a stop scan command and send it to FW
 * 
 * Build a stop scan command and send it to FW
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  eScanTag - scan tag, used for scan complete and result tracking
 * \return command status (OK / NOK)
 * \sa     cmdBld_CmdStartSPSScan
 */ 
TI_STATUS cmdBld_CmdStopScan (TI_HANDLE hCmdBld, EScanResultTag eScanTag, 
                              void *fScanCommandResponseCB, TI_HANDLE hCb)
{
    return cmdBld_CmdIeStopScan (hCmdBld, fScanCommandResponseCB, hCb);
}


/** 
 * \fn     cmdBld_CmdStopSPSScan
 * \brief  Build a stop SPS scan command and send it to FW
 * 
 * Build a stop SPS scan command and send it to FW
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  eScanTag - scan tag, used for scan complete and result tracking
 * \return command status (OK / NOK)
 * \sa     cmdBld_CmdStartScan
 */ TI_STATUS cmdBld_CmdStopSPSScan (TI_HANDLE hCmdBld, EScanResultTag eScanTag, 
                                 void* fScanCommandResponseCB, TI_HANDLE hCb)
{
    return cmdBld_CmdIeStopSPSScan (hCmdBld, fScanCommandResponseCB, hCb);
}

TI_STATUS cmdBld_CmdSetSplitScanTimeOut (TI_HANDLE hCmdBld, TI_UINT32 uTimeOut)
{
    DB_WLAN(hCmdBld).uSlicedScanTimeOut = uTimeOut;

	return cmdBld_CmdIeSetSplitScanTimeOut (hCmdBld, uTimeOut, NULL, NULL);
}

/** 
 * \fn     cmdBld_debugPrintPeriodicScanChannles 
 * \brief  Print periodic scan channel list for debug purposes
 * 
 * Print periodic scan channel list for debug purposes
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  pChannel - pointer to the channel list to print
 * \param  uChannelCount - number of channles to print
 * \return None
 * \sa     cmdBld_debugPrintPeriodicScanParams
 */ 
void cmdBld_debugPrintPeriodicScanChannles (TI_HANDLE hCmdBld, ConnScanChannelInfo_t* pChannel,
                                            TI_UINT32 uChannelCount)
{
    TCmdBld                 *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32               uIndex;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Index  Channel  MinTime  MaxTime  DFStime  PowerLevel\n");
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "-------------------------------------------------------------------\n");
    for (uIndex = 0; uIndex <  uChannelCount; uIndex++)
    {
        TRACE6(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "%-10d %-10d %-10d %-10d %-10d %-11d\n", uIndex, pChannel[ uIndex ].channel, pChannel[ uIndex ].scanMinDuration, pChannel[ uIndex ].scanMaxDuration, pChannel[ uIndex ].passiveScanDuration, pChannel[ uIndex ].txPowerLevelDbm);
    }
}

/** 
 * \fn     cmdBld_debugPrintPeriodicScanParams 
 * \brief  Print periodic scan parameters for debug purposes
 * 
 * Print periodic scan parameters for debug purposes
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  pCommand - pointer to the periodic scan command to print
 * \return None
 * \sa     cmdBld_debugPrintPeriodicScanChannles
 */ 
void cmdBld_debugPrintPeriodicScanParams (TI_HANDLE hCmdBld, ConnScanParameters_t* pCommand)
{
    TCmdBld                 *pCmdBld = (TCmdBld *)hCmdBld;

    /* print periodic scan params command */
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Cycle intervals:\n");
    TRACE8(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "0:  %-6d %-6d %-6d %-6d %-6d %-6d %-6d %-6d\n", pCommand->cycleIntervals[ 0 ], pCommand->cycleIntervals[ 1 ], pCommand->cycleIntervals[ 2 ], pCommand->cycleIntervals[ 3 ], pCommand->cycleIntervals[ 4 ], pCommand->cycleIntervals[ 5 ], pCommand->cycleIntervals[ 6 ], pCommand->cycleIntervals[ 7 ]);
    TRACE8(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "8:  %-6d %-6d %-6d %-6d %-6d %-6d %-6d %-6d\n", pCommand->cycleIntervals[ 8 ], pCommand->cycleIntervals[ 9 ], pCommand->cycleIntervals[ 10 ], pCommand->cycleIntervals[ 11 ], pCommand->cycleIntervals[ 12 ], pCommand->cycleIntervals[ 13 ], pCommand->cycleIntervals[ 14 ], pCommand->cycleIntervals[ 15 ]);
    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "RSSI threshold: %d, SNR threshold: %d, number of cycles: %d, reporth threshold: %d\n", pCommand->rssiThreshold, pCommand->snrThreshold, pCommand->maxNumOfCycles, pCommand->reportThreshold);
    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Terminate on report: %d, result tag: %d, BSS type: %d, number of probe requests: %d\n", pCommand->terminateOnReport, pCommand->resultsTag, pCommand->bssType, pCommand->numProbe);
    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "SSID filter type: %d, SSID length: %d, SSID: \n", pCommand->ssidFilterType, pCommand->ssidLength);
    /* print channel info */

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "2.4 GHz Channels:\n");
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "-----------------\n");
    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Number of passive channels: %d, number of active channels: %d\n", pCommand->numOfPassive[ 0 ], pCommand->numOfActive[ 0 ]);
    cmdBld_debugPrintPeriodicScanChannles (hCmdBld, &(pCommand->channelList[ 0 ]),
                                           pCommand->numOfPassive[ 0 ] + 
                                           pCommand->numOfActive[ 0 ]);
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "5.0 GHz Channels:\n");
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "-----------------\n");
    TRACE3(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Number of passive channels: %d, number of DFS channels: %d, number of active channels: %d\n", pCommand->numOfPassive[ 1 ], pCommand->numOfDfs, pCommand->numOfActive[ 2 ]);
    cmdBld_debugPrintPeriodicScanChannles (hCmdBld, &(pCommand->channelList[ CONN_SCAN_MAX_CHANNELS_BG ]),
                                           pCommand->numOfPassive[ 1 ] + 
                                           pCommand->numOfActive[ 1 ] +
                                           pCommand->numOfDfs);
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "4.9 GHz channles:\n");
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "-----------------\n");
    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Number of passive channels: %d, number of active channels: %d\n", pCommand->numOfPassive[ 2 ], pCommand->numOfActive[ 2 ]);
    cmdBld_debugPrintPeriodicScanChannles (hCmdBld, &(pCommand->channelList[ CONN_SCAN_MAX_CHANNELS_BG + CONN_SCAN_MAX_CHANNELS_A ]),
                                           pCommand->numOfPassive[ 2 ] + 
                                           pCommand->numOfActive[ 2 ]);
}

/** 
 * \fn     cmdBld_debugPrintPeriodicScanSsidList 
 * \brief  Print periodic scan SSID list for debug purposes
 * 
 * Print periodic scan SSID list for debug purposes
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  pCommand - pointer to the periodic scan SSID list command to print
 * \return None
 * \sa     cmdBld_debugPrintPeriodicScanParams
 */ 
void cmdBld_debugPrintPeriodicScanSsidList (TI_HANDLE hCmdBld, ConnScanSSIDList_t* pCommand)
{
    TCmdBld                 *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32               uIndex;

    /* print SSID list command */
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "SSID list:\n");
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "---------\n");
    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Num of entries: %d\n", pCommand->numOfSSIDEntries);
    for (uIndex = 0; uIndex < pCommand->numOfSSIDEntries; uIndex++)
    {
        TRACE3(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "index: %d, SSID type: %d, SSID length:% d, SSID string:\n", uIndex, pCommand->SSIDList[ uIndex ].ssidType, pCommand->SSIDList[ uIndex ].ssidLength);
    }

}

/** 
 * \fn     cmdBld_BuildPeriodicScanChannlesn 
 * \brief  Copy channels info for periodic scan to FW structure for a specific band and scan type 
 * 
 * Copy channels info, from driver structure, to FW structure, for periodic scan, for a specific
 * band and scan type.
 * 
 * \param  pPeriodicScanParams - driver priodic scan parameters (source)
 * \param  pChannelList - FW scan channel list (destination)
 * \param  eScanType - scan type (passive or active)
 * \param  eRadioBand - band (G, A or J)
 * \param  uPassiveScanDfsDwellTime - Dwell time for passive scan on DFS channels (in milli-secs)
 * \return Number of channels found for this scan type and band
 * \sa     cmdBld_StartPeriodicScan 
 */ 
TI_UINT32 cmdBld_BuildPeriodicScanChannles (TPeriodicScanParams *pPeriodicScanParams, 
                                            ConnScanChannelInfo_t *pChannelList,
                                            EScanType eScanType, ERadioBand eRadioBand,
                                            TI_UINT32 uPassiveScanDfsDwellTime)
{
    TI_UINT32       uIndex, uNumChannels = 0;

    /* check all channels */
    for (uIndex = 0; uIndex < pPeriodicScanParams->uChannelNum; uIndex++)
    {
        /* if this channel is on the required band and uses the required scan type */
        if ((eRadioBand == pPeriodicScanParams->tChannels[ uIndex ].eBand) &&
            (eScanType == pPeriodicScanParams->tChannels[ uIndex ].eScanType))
        {
            /* update scan parameters */
            pChannelList[ uNumChannels ].channel = (TI_UINT8)pPeriodicScanParams->tChannels[ uIndex ].uChannel;
            pChannelList[ uNumChannels ].scanMaxDuration = 
                ENDIAN_HANDLE_WORD ((TI_UINT16)pPeriodicScanParams->tChannels[ uIndex ].uMaxDwellTimeMs);
            pChannelList[ uNumChannels ].scanMinDuration = 
                ENDIAN_HANDLE_WORD ((TI_UINT16)pPeriodicScanParams->tChannels[ uIndex ].uMinDwellTimeMs);
            pChannelList[ uNumChannels ].txPowerLevelDbm = (TI_UINT8)pPeriodicScanParams->tChannels[ uIndex ].uTxPowerLevelDbm;
            if (SCAN_TYPE_PACTSIVE == eScanType) /* DFS channel */
            {
                pChannelList[ uNumChannels ].passiveScanDuration = ENDIAN_HANDLE_WORD ((TI_UINT16)uPassiveScanDfsDwellTime);
                pChannelList[ uNumChannels ].channelFlags = 1; /* mark as DFS channel */
            }
            else
            {
                pChannelList[ uNumChannels ].passiveScanDuration = ENDIAN_HANDLE_WORD ((TI_UINT16)pPeriodicScanParams->tChannels[ uIndex ].uMaxDwellTimeMs);
                pChannelList[ uNumChannels ].channelFlags = 0; /* mark as not DFS channel */
            }

            /* advance mathcing channel counter */
            uNumChannels++;
        }
    }

    /* return channel count */
    return uNumChannels;
}

/** 
 * \fn     cmdBld_StartPeriodicScan 
 * \brief  Copy driver periodic scan parameters to FW structures and send all commands to FW
 * 
 * Copy driver periodic scan parameters to FW structures (SSID list, parameters including channels
 * and start command) and send all commands to FW.
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  pPeriodicScanParams - periodic scan driver parameters (source)
 * \param  eScanTag - scan tag, used for scan complete and result tracking
 * \param  uPassiveScanDfsDwellTimeUs - Passive dwell time for DFS channels
 * \param  fScanCommandResponseCB - scan command complete CB
 * \param  hCb - scan command response handle
 * \return TI_OK on success, other codes indicate failure
 * \sa     cmdBld_BuildPeriodicScanChannles, cmdBld_StopPeriodicScan
 */ 
TI_STATUS cmdBld_StartPeriodicScan (TI_HANDLE hCmdBld, TPeriodicScanParams *pPeriodicScanParams,
                                    EScanResultTag eScanTag, TI_UINT32 uPassiveScanDfsDwellTimeMs,
                                    void* fScanCommandResponseCB, TI_HANDLE hCb)
{
    TCmdBld                         *pCmdBld = (TCmdBld *)hCmdBld;
    ConnScanParameters_t            tFWPeriodicScanParams;
    ConnScanSSIDList_t              *pFWSsidList;
    PeriodicScanTag                 tScanStart;
    TI_UINT32                       uIndex;
    TI_STATUS                       tStatus;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Building start periodic scan commands:\n");
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "--------------------------------------\n");
    /* copy parameters to FW structure */
    tFWPeriodicScanParams.bssType = (ScanBssType_e)pPeriodicScanParams->eBssType;
    for (uIndex = 0; uIndex < PERIODIC_SCAN_MAX_INTERVAL_NUM; uIndex ++)
    {
        tFWPeriodicScanParams.cycleIntervals[ uIndex ] = 
            ENDIAN_HANDLE_LONG (pPeriodicScanParams->uCycleIntervalMsec[ uIndex ]);
    }
    tFWPeriodicScanParams.maxNumOfCycles = (TI_UINT8)pPeriodicScanParams->uCycleNum;
    tFWPeriodicScanParams.numProbe = (TI_UINT8)pPeriodicScanParams->uProbeRequestNum;
    tFWPeriodicScanParams.reportThreshold = (TI_UINT8)pPeriodicScanParams->uFrameCountReportThreshold;
    tFWPeriodicScanParams.rssiThreshold = (TI_UINT8)pPeriodicScanParams->iRssiThreshold;
    tFWPeriodicScanParams.snrThreshold = (TI_INT8)pPeriodicScanParams->iSnrThreshold;
    tFWPeriodicScanParams.terminateOnReport = (TI_UINT8)pPeriodicScanParams->bTerminateOnReport;
    tFWPeriodicScanParams.resultsTag = (TI_UINT8)eScanTag;
    switch (pPeriodicScanParams->uSsidNum)
    {
    case 0: /* No SSIDs defined - no need to filter according to SSID */
        tFWPeriodicScanParams.ssidFilterType = (ScanSsidFilterType_e)SCAN_SSID_FILTER_TYPE_ANY;
        tFWPeriodicScanParams.ssidLength = 0;
        break;

    default: /* More than one SSID - copy SSIDs to SSID list command */
        pFWSsidList = os_memoryAlloc(pCmdBld->hOs, sizeof(ConnScanSSIDList_t));
        if (!pFWSsidList)
        {
            return TI_NOK;
        }

        if ((TI_UINT8)pPeriodicScanParams->uSsidListFilterEnabled == 1)
	        tFWPeriodicScanParams.ssidFilterType = (ScanSsidFilterType_e)SCAN_SSID_FILTER_TYPE_LIST;
		else
	        tFWPeriodicScanParams.ssidFilterType = (ScanSsidFilterType_e)SCAN_SSID_FILTER_TYPE_LIST_FILTER_DISABLED;
        tFWPeriodicScanParams.ssidLength = 0;
        pFWSsidList->numOfSSIDEntries = (TI_UINT8)pPeriodicScanParams->uSsidNum;
        for (uIndex = 0; uIndex < pPeriodicScanParams->uSsidNum; uIndex++)
        {
            pFWSsidList->SSIDList[ uIndex ].ssidType = 
                (TI_UINT8)pPeriodicScanParams->tDesiredSsid[ uIndex ].eVisability;
            pFWSsidList->SSIDList[ uIndex ].ssidLength = 
                (TI_UINT8)pPeriodicScanParams->tDesiredSsid[ uIndex ].tSsid.len;
            os_memoryCopy (pCmdBld->hOs, (void*)&(pFWSsidList->SSIDList[ uIndex ].ssid[ 0 ]),
                           (void*)&(pPeriodicScanParams->tDesiredSsid[ uIndex ].tSsid.str[ 0 ]),
                           pFWSsidList->SSIDList[ uIndex ].ssidLength);
        }

        /* print the SSID list parameters */
        cmdBld_debugPrintPeriodicScanSsidList (hCmdBld, pFWSsidList);

        /* send the SSID list command */
        tStatus = cmdBld_CmdIeScanSsidList (hCmdBld, pFWSsidList, NULL, NULL);
        os_memoryFree(pCmdBld->hOs, pFWSsidList, sizeof(ConnScanSSIDList_t));
        if (TI_OK != tStatus)
        {
            TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR , "cmdBld_StartPeriodicScan: status %d when configuring SSID list", tStatus);
            return tStatus;
        }
        break;
    }

    /* copy channels */
    tFWPeriodicScanParams.numOfPassive[ 0 ] =  /* build passive B/G channels */
        cmdBld_BuildPeriodicScanChannles (pPeriodicScanParams, &(tFWPeriodicScanParams.channelList[ 0 ]),
                                          SCAN_TYPE_NORMAL_PASSIVE, RADIO_BAND_2_4_GHZ, uPassiveScanDfsDwellTimeMs);
    tFWPeriodicScanParams.numOfActive[ 0 ] = /* build active B/G channels */
        cmdBld_BuildPeriodicScanChannles (pPeriodicScanParams, &(tFWPeriodicScanParams.channelList[ tFWPeriodicScanParams.numOfPassive[ 0 ] ]),
                                          SCAN_TYPE_NORMAL_ACTIVE, RADIO_BAND_2_4_GHZ, uPassiveScanDfsDwellTimeMs);
    tFWPeriodicScanParams.numOfPassive[ 1 ] = /* build passive A channels */
        cmdBld_BuildPeriodicScanChannles (pPeriodicScanParams, &(tFWPeriodicScanParams.channelList[ CONN_SCAN_MAX_CHANNELS_BG ]),
                                          SCAN_TYPE_NORMAL_PASSIVE, RADIO_BAND_5_0_GHZ, uPassiveScanDfsDwellTimeMs);
    tFWPeriodicScanParams.numOfDfs = /* build DFS A channels */
        cmdBld_BuildPeriodicScanChannles (pPeriodicScanParams, &(tFWPeriodicScanParams.channelList[ CONN_SCAN_MAX_CHANNELS_BG + tFWPeriodicScanParams.numOfPassive[ 1 ] ]),
                                          SCAN_TYPE_PACTSIVE, RADIO_BAND_5_0_GHZ, uPassiveScanDfsDwellTimeMs);
    tFWPeriodicScanParams.numOfActive[ 1 ] = /* build active A channels */
        cmdBld_BuildPeriodicScanChannles (pPeriodicScanParams, &(tFWPeriodicScanParams.channelList[ CONN_SCAN_MAX_CHANNELS_BG + tFWPeriodicScanParams.numOfPassive[ 1 ] + tFWPeriodicScanParams.numOfDfs ]),
                                          SCAN_TYPE_NORMAL_ACTIVE, RADIO_BAND_5_0_GHZ, uPassiveScanDfsDwellTimeMs);

    /* until J is supported, mark zero channels for J passive and active */
    tFWPeriodicScanParams.numOfPassive[ 2 ] = 0;
    tFWPeriodicScanParams.numOfActive[ 2 ] = 0;

    /* print the command */
    cmdBld_debugPrintPeriodicScanParams (hCmdBld, &tFWPeriodicScanParams);

    /* Send the periodic scan parameters command */
    tStatus = cmdBld_CmdIePeriodicScanParams (hCmdBld, &tFWPeriodicScanParams, NULL, NULL);
    if (TI_OK != tStatus)
    {
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR , "cmdBld_StartPeriodicScan: status %d when configuring periodic scan parameters", tStatus);
        return tStatus;
    }

    /* send the periodic scan start command */
    tScanStart.scanTag = eScanTag;
    tStatus = cmdBld_CmdIeStartPeriodicScan (hCmdBld, &tScanStart, fScanCommandResponseCB, hCb);
    return tStatus;
}

/** 
 * \fn     cmdBld_StopPeriodicScan 
 * \brief  Stops an on-going periodic scan operation
 * 
 * Stops an on-going periodic scan operation 
 * 
 * \param  hCmdBld - handle to the command builder object
 * \param  eScanTag - scan tag, used for scan complete and result tracking
 * \param  fScanCommandResponseCB - scan command complete CB
 * \param  hCb - scan command response handle
 * \return TI_OK on success, other codes indicate failure
 * \sa     cmdBld_BuildPeriodicScanChannles, cmdBld_StartPeriodicScan
 */ 
TI_STATUS cmdBld_StopPeriodicScan (TI_HANDLE hCmdBld, EScanResultTag eScanTag, 
                                   void* fScanCommandResponseCB, TI_HANDLE hCb)
{
    PeriodicScanTag tScanStop;

    /* send the periodic scan stop command */
    tScanStop.scanTag = eScanTag;
    return cmdBld_CmdIeStopPeriodicScan (hCmdBld, &tScanStop, fScanCommandResponseCB, hCb);
}

/****************************************************************************
 *                      cmdBld_SetBssType()
 ****************************************************************************
 * DESCRIPTION: Set Bss type, set RxFilter 
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
static TI_STATUS cmdBld_CmdSetBssType (TI_HANDLE hCmdBld, ScanBssType_e BssType, TI_UINT8 *HwBssType)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    switch (BssType)
    {
    case BSS_INFRASTRUCTURE:
        DB_BSS(hCmdBld).BssType = BSS_TYPE_STA_BSS;
        cmdBld_SetRxFilter (hCmdBld, RX_CONFIG_OPTION_FOR_JOIN, RX_FILTER_OPTION_JOIN);
        break;

    case BSS_INDEPENDENT:
        DB_BSS(hCmdBld).BssType = BSS_TYPE_IBSS;
        cmdBld_SetRxFilter (hCmdBld, RX_CONFIG_OPTION_FOR_IBSS_JOIN, RX_FILTER_OPTION_DEF);
        break;

    default: 
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_FATAL_ERROR, "cmdBld_SetBssType: FATAL_ERROR, unknown BssType %d\n", BssType);
        return TI_NOK;
    }

    *HwBssType = DB_BSS(hCmdBld).BssType;

    return TI_OK;
}


/****************************************************************************
 *                      cmdBld_StartJoin()
 ****************************************************************************
 * DESCRIPTION: Enable Rx/Tx and send Start/Join command 
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdStartJoin (TI_HANDLE hCmdBld, ScanBssType_e BssType, void *fJoinCompleteCB, TI_HANDLE hCb)
{
    TI_UINT8  HwBssType = 0;
#ifdef TI_DBG  
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8 *pBssId = DB_BSS(hCmdBld).BssId;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INIT, "cmdBld_StartJoin: Enable Tx, Rx and Start the Bss, type=%d\n", BssType);
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INIT, "------------------------------------------------------------\n");
    TRACE7(pCmdBld->hReport, REPORT_SEVERITY_INIT, "START/JOIN, SSID=, BSSID=%02X-%02X-%02X-%02X-%02X-%02X, Chan=%d\n", pBssId[0], pBssId[1], pBssId[2], pBssId[3], pBssId[4], pBssId[5], DB_BSS(hCmdBld).RadioChannel);
    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INIT, "------------------------------------------------------------\n");
#endif /* TI_DBG */

    /*
     * set RxFilter (but don't write it to the FW, this is done in the join command),
     * Configure templates content, ...
     */
    cmdBld_CmdSetBssType (hCmdBld, BssType, &HwBssType);

    return cmdBld_CmdIeStartBss (hCmdBld, HwBssType, fJoinCompleteCB, hCb);
}


TI_STATUS cmdBld_CmdJoinBss (TI_HANDLE hCmdBld, TJoinBss *pJoinBssParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld        *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams    *pWlanParams = &DB_WLAN(hCmdBld);
    TBssInfoParams *pBssInfoParams = &DB_BSS(hCmdBld);
#ifdef TI_DBG
    TI_UINT8 dbgSsidStr[33];
#endif /* TI_DBG */

    /* for debug purpose, can be removed later*/
    if (pJoinBssParams->ssidLength > 32)
        pJoinBssParams->ssidLength = 32;

    /* Update Tx-Session-Counter in the Ctrl field of the Join command. */
    pBssInfoParams->Ctrl &= ~JOIN_CMD_CTRL_TX_SESSION;
    pBssInfoParams->Ctrl |= (TI_UINT8)(pJoinBssParams->txSessionCount << JOIN_CMD_CTRL_OFFSET_TX_SESSION);

#ifdef TI_DBG
    os_memoryCopy (pCmdBld->hOs, (void *)dbgSsidStr, (void *)pJoinBssParams->pSSID, pJoinBssParams->ssidLength);
    dbgSsidStr[pJoinBssParams->ssidLength] = '\0';

    TRACE14(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "TWD_JoinBss : bssType = %d, beaconInterval = %d, dtimInterval = %d, channel = %d, BSSID = %x-%x-%x-%x-%x-%x, ssidLength = %d, basicRateSet = 0x%x, RadioBand = %d, Ctrl = 0x%x", pJoinBssParams->bssType, pJoinBssParams->beaconInterval, pJoinBssParams->dtimInterval, pJoinBssParams->channel, pJoinBssParams->pBSSID[0], pJoinBssParams->pBSSID[1], pJoinBssParams->pBSSID[2], pJoinBssParams->pBSSID[3], pJoinBssParams->pBSSID[4], pJoinBssParams->pBSSID[5], pJoinBssParams->ssidLength, pJoinBssParams->basicRateSet, pJoinBssParams->radioBand, pBssInfoParams->Ctrl);
#endif /* TI_DBG */
    /*
     * save Bss info parameters
     */
    DB_BSS(hCmdBld).ReqBssType = pJoinBssParams->bssType;
    MAC_COPY (DB_BSS(hCmdBld).BssId, pJoinBssParams->pBSSID);
    pBssInfoParams->tSsid.len = pJoinBssParams->ssidLength;
    os_memoryZero (pCmdBld->hOs, (void *)pBssInfoParams->tSsid.str, sizeof (pBssInfoParams->tSsid.str));
    os_memoryCopy (pCmdBld->hOs, (void *)pBssInfoParams->tSsid.str, (void *)pJoinBssParams->pSSID, pJoinBssParams->ssidLength);
    DB_BSS(hCmdBld).BeaconInterval = pJoinBssParams->beaconInterval;
    DB_BSS(hCmdBld).DtimInterval = (TI_UINT8)pJoinBssParams->dtimInterval;
    DB_BSS(hCmdBld).RadioChannel = pJoinBssParams->channel;
    DB_WLAN(hCmdBld).RadioBand = (TI_UINT8)pJoinBssParams->radioBand;
    DB_BSS(hCmdBld).BasicRateSet = pJoinBssParams->basicRateSet;

    /* In case we're joining a new BSS, reset the TKIP/AES sequence counter. */
    /* The firmware resets its own counter - so we won't have mismatch in the following TX complete events */
    pCmdBld->uSecuritySeqNumLow = 0;
    pCmdBld->uSecuritySeqNumHigh = 0;

    pWlanParams->bJoin = TI_TRUE;
    pWlanParams->bStaConnected = TI_FALSE;
    /*
     * call the hardware to start/join the bss
     */
    return cmdBld_CmdStartJoin (hCmdBld, pJoinBssParams->bssType, fCb, hCb);
}


TI_STATUS cmdBld_CmdTemplate (TI_HANDLE hCmdBld, TSetTemplate *pTemplateParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;
    TI_STATUS  Stt;
    TTemplateParams *pTemplate;
    TI_UINT8   uIndex = 0;
    TemplateType_e eType;

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "cmdBld_CmdTemplate: Type=%d, size=%d, index=%d, RateMask=0x%x\n", pTemplateParams->type, pTemplateParams->len, pTemplateParams->index, pTemplateParams->uRateMask);

    switch (pTemplateParams->type)
    {
    case BEACON_TEMPLATE:
        eType = TEMPLATE_BEACON;
        pTemplate = &(DB_TEMP(hCmdBld).Beacon);
        break;

    case PROBE_RESPONSE_TEMPLATE:
        eType = TEMPLATE_PROBE_RESPONSE;
        pTemplate = &(DB_TEMP(hCmdBld).ProbeResp);
        break;

    case PROBE_REQUEST_TEMPLATE:
        if (pTemplateParams->eBand == RADIO_BAND_2_4_GHZ)
        {
            eType = CFG_TEMPLATE_PROBE_REQ_2_4;
            pTemplate = &(DB_TEMP(hCmdBld).ProbeReq24);
        }
        else
        {
            eType = CFG_TEMPLATE_PROBE_REQ_5;
            pTemplate = &(DB_TEMP(hCmdBld).ProbeReq50);
        }
        break;

    case NULL_DATA_TEMPLATE:
        eType = TEMPLATE_NULL_DATA;
        pTemplate = &(DB_TEMP(hCmdBld).NullData);
        break;

    case PS_POLL_TEMPLATE:
        eType = TEMPLATE_PS_POLL;
        pTemplate = &(DB_TEMP(hCmdBld).PsPoll);
        break;

    case QOS_NULL_DATA_TEMPLATE:
        eType = TEMPLATE_QOS_NULL_DATA;
        pTemplate = &(DB_TEMP(hCmdBld).QosNullData);
        break;

    case KEEP_ALIVE_TEMPLATE:
        eType = TEMPLATE_KLV;
        uIndex = pTemplateParams->index;
        pTemplate = &(DB_TEMP(hCmdBld).KeepAlive[uIndex]);
        break;

    case DISCONN_TEMPLATE:
        eType = TEMPLATE_DISCONNECT;
        pTemplate = &(DB_TEMP(hCmdBld).Disconn);
        break;

    case ARP_RSP_TEMPLATE:
        eType = TEMPLATE_ARP_RSP;
        pTemplate = &(DB_TEMP(hCmdBld).ArpRsp); 
        break;

    default:
        TRACE1( pCmdBld->hReport, REPORT_SEVERITY_ERROR,
                 "cmdBld_CmdTemplate. Invalid template type:%d\n", pTemplateParams->type);
        return TI_NOK;
    }

    /* Save template information to DB (for recovery) */
    pTemplate->Size = pTemplateParams->len;
    pTemplate->uRateMask = pTemplateParams->uRateMask;
    os_memoryCopy (pCmdBld->hOs, 
                   (void *)(pTemplate->Buffer), 
                   (void *)(pTemplateParams->ptr), 
                   pTemplateParams->len);
    /* if (eType == TEMPLATE_ARP_RSP)
    {   
       WLAN_OS_REPORT(("cmdBld_CmdTemplate: template (len=%d):\n>>>", pTemplate->Size));
       for (i=0; i<sizeof(ArpRspTemplate_t); i++ )
       {
           WLAN_OS_REPORT((" %2x", *(pTemplate->Buffer+i))); 
           if (i%8 == 7) WLAN_OS_REPORT(("\n>>>"));
       }
         WLAN_OS_REPORT(("\n"));
    }
	*/
    /* Configure template to FW */
    Stt = cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                              pTemplate, 
                                              (TI_UINT16)pTemplateParams->len,
                                              eType,
                                              uIndex, /* index is only relevant for keep-alive template */
                                              fCb,
                                              hCb);

    /* WLAN_OS_REPORT(("cmdBld_CmdTemplate: template %d config rc=%d\n", eType, Stt)); */
    return Stt;
}


/****************************************************************************
 *                      cmdBld_switchChannel()
 ****************************************************************************
 * DESCRIPTION: Switching the serving channel 
 * 
 * INPUTS: channel  -   new channel number  
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdEnableTx (TI_HANDLE hCmdBld, TI_UINT8 channel, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CmdIeEnableTx (hCmdBld, channel, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_DisableTx()
 ****************************************************************************
 * DESCRIPTION: Disable Tx path. 
 * 
 * INPUTS: None
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdDisableTx (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CmdIeDisableTx (hCmdBld, fCb, hCb);
}



/****************************************************************************
 *                      cmdBld_SwitchChannelCmd()
 ****************************************************************************
 * DESCRIPTION: Send Switch Channel command 
 * 
 * INPUTS: None  
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdSwitchChannel (TI_HANDLE hCmdBld, TSwitchChannelParams *pSwitchChannelCmd, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "\n SwitchChannelCmd :\n                             channelNumber = %d\n                             switchTime = %d\n                             txFlag = %d\n                             flush = %d \n ", pSwitchChannelCmd->channelNumber, pSwitchChannelCmd->switchTime, pSwitchChannelCmd->txFlag, pSwitchChannelCmd->flush);

    DB_BSS(hCmdBld).RadioChannel = pSwitchChannelCmd->channelNumber;

    return cmdBld_CmdIeSwitchChannel (hCmdBld, pSwitchChannelCmd, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_SwitchChannelCmd()
 ****************************************************************************
 * DESCRIPTION: Send Switch Channel command 
 * 
 * INPUTS: None  
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdSwitchChannelCancel (TI_HANDLE hCmdBld, TI_UINT8 channel, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "\n TWD_SwitchChannelCancelCmd :\n ");

    DB_BSS(hCmdBld).RadioChannel = channel;

    return cmdBld_CmdIeSwitchChannelCancel (hCmdBld, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_FwDisconnect()
 ****************************************************************************
 * DESCRIPTION: Disconnect. 
 * 
 * INPUTS: None
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdFwDisconnect (TI_HANDLE hCmdBld, TI_UINT32 uConfigOptions, TI_UINT32 uFilterOptions, DisconnectType_e uDisconType, TI_UINT16 uDisconReason, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);

    pWlanParams->bJoin = TI_FALSE;
    pWlanParams->bStaConnected = TI_FALSE;

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Sending FW disconnect, ConfigOptions=%x, FilterOPtions=%x, uDisconType=%d, uDisconReason=%d\n",uConfigOptions, uFilterOptions, uDisconType, uDisconReason);


    return cmdBld_CmdIeFwDisconnect (hCmdBld, uConfigOptions, uFilterOptions, uDisconType, uDisconReason, fCb, hCb);
}


TI_STATUS cmdBld_CmdMeasurement (TI_HANDLE          hCmdBld, 
                                 TMeasurementParams *pMeasurementParams,
                                 void               *fCommandResponseCB, 
                                 TI_HANDLE          hCb)
{
    return cmdBld_CmdIeMeasurement (hCmdBld, pMeasurementParams, fCommandResponseCB, hCb);
}


/****************************************************************************
 *                      cmdBld_measurementStop()
 ****************************************************************************
 * DESCRIPTION: send Command for stoping measurement  
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdMeasurementStop (TI_HANDLE hCmdBld, void* fMeasureCommandResponseCB, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "cmdBld_measurementStop\n");

    return cmdBld_CmdIeMeasurementStop (hCmdBld, fMeasureCommandResponseCB, hCb);
}


/****************************************************************************
 *                      cmdBld_ApDiscovery()
 ****************************************************************************
 * DESCRIPTION: send Command for AP Discovery 
 *              to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdApDiscovery (TI_HANDLE hCmdBld, TApDiscoveryParams *pApDiscoveryParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_ApDiscovery\n");

    return cmdBld_CmdIeApDiscovery (hCmdBld, pApDiscoveryParams, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_ApDiscoveryStop()
 ****************************************************************************
 * DESCRIPTION: send Command for stoping AP Discovery
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdApDiscoveryStop (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_ApDiscoveryStop\n");
    
    return cmdBld_CmdIeApDiscoveryStop (hCmdBld, fCb, hCb);
}


TI_STATUS cmdBld_CmdNoiseHistogram (TI_HANDLE hCmdBld, TNoiseHistogram *pNoiseHistParams, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CmdIeNoiseHistogram (hCmdBld, pNoiseHistParams, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_PowerMgmtConfigurationSet ()
 ****************************************************************************
 * DESCRIPTION: Set the ACX power management option IE
 * 
 * INPUTS: powerSaveParams
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdSetPsMode (TI_HANDLE hCmdBld, TPowerSaveParams* powerSaveParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    /* Rate conversion is done in the HAL */
    cmdBld_ConvertAppRatesBitmap (powerSaveParams->NullPktRateModulation, 
                                  0, 
                                  &powerSaveParams->NullPktRateModulation);

    TRACE5(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, " cmdBld_PowerMgmtConfigurationSet  ps802_11Enable=0x%x hangOverPeriod=%d needToSendNullData=0x%x  numNullPktRetries=%d  NullPktRateModulation=0x%x\n", powerSaveParams->ps802_11Enable, powerSaveParams->hangOverPeriod, powerSaveParams->needToSendNullData, powerSaveParams->numNullPktRetries, powerSaveParams->NullPktRateModulation);

    return cmdBld_CmdIeSetPsMode (hCmdBld, powerSaveParams, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_EnableRx()
 ****************************************************************************
 * DESCRIPTION: Enable Rx and send Start/Join command 
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdEnableRx (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CmdIeEnableRx (hCmdBld, fCb, hCb);
}


TI_STATUS cmdBld_CmdAddKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, TI_BOOL reconfFlag, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8     keyIdx  = (TI_UINT8)pKey->keyIndex;

    /* store the security key for reconfigure phase (FW reload)*/
    if (reconfFlag != TI_TRUE)
    {
        if (keyIdx >= (pCmdBld->tSecurity.uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION + NO_OF_EXTRA_RECONF_SECUR_KEYS))
        {
            TRACE2(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_CmdAddKey: ERROR Key keyIndex field out of range =%d, range is (0 to %d)\n", pKey->keyIndex, pCmdBld->tSecurity.uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION+NO_OF_EXTRA_RECONF_SECUR_KEYS - 1);
            
            return TI_NOK;
        }

        if (pKey->keyType == KEY_NULL)
        {
            TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_CmdAddKey: ERROR KeyType is NULL_KEY\n");
            
            return TI_NOK;
        }

        os_memoryCopy (pCmdBld->hOs, 
                       (void *)(DB_KEYS(pCmdBld).pReconfKeys + keyIdx),
                       (void *)pKey, 
                       sizeof(TSecurityKeys));
    }
    
    switch (pCmdBld->tSecurity.eSecurityMode)
    {
        case TWD_CIPHER_WEP:
        case TWD_CIPHER_WEP104:
				return cmdBld_CmdAddWepDefaultKey (hCmdBld, pKey, fCb, hCb);
    
        case TWD_CIPHER_TKIP:
        case TWD_CIPHER_AES_CCMP:
        #ifdef GEM_SUPPORTED
            case TWD_CIPHER_GEM:
        #endif
            return cmdBld_CmdAddWpaKey (hCmdBld, pKey, fCb, hCb);

        default:
            return TI_NOK;
    }
}


TI_STATUS cmdBld_CmdAddWpaKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
 
    /* Only WEP, TKIP, AES keys are handled*/
    switch (pKey->keyType)
    {
        case KEY_WEP:
            /* Configure the encKeys to the HW - default keys cache*/
            return cmdBld_CmdAddWepDefaultKey (hCmdBld, pKey, fCb, hCb);
        
        case KEY_TKIP:
            /* Set the REAL TKIP key into the TKIP key cache*/
            if (cmdBld_CmdAddTkipMicMappingKey (hCmdBld, pKey, fCb, hCb) != TI_OK)
                return TI_NOK;

            break;
        
        case KEY_AES:
            if (cmdBld_CmdAddAesMappingKey (hCmdBld, pKey, fCb, hCb) != TI_OK)
                return TI_NOK;
            break;
        
        #ifdef GEM_SUPPORTED
            case KEY_GEM:
                if (cmdBld_CmdAddGemMappingKey (hCmdBld, pKey, fCb, hCb) != TI_OK)
                    return TI_NOK;
                break;
        #endif
        
        default:
            return TI_NOK;
    }
    
    /* AES or TKIP key has been successfully added. Store the current */
    /* key type of the unicast (i.e. transmit !) key                  */
    if (!MAC_BROADCAST (pKey->macAddress))
    {
        pCmdBld->tSecurity.eCurTxKeyType = pKey->keyType;
    }

    return TI_OK;
}


TI_STATUS cmdBld_CmdRemoveWpaKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb)
{
    /* Only WEP, TKIP, AES keys are handled*/
    switch (pKey->keyType)
    {
        case KEY_WEP:
            /* Configure the encKeys to the HW - default keys cache*/
            return cmdBld_CmdRemoveWepDefaultKey (hCmdBld, pKey, fCb, hCb);
        
        case KEY_TKIP:
            /* Configure the encKeys to the HW - mapping keys cache*/
            /* configure through SET_KEYS command */

            /* remove the TKIP key from the TKIP key cache*/
            if (cmdBld_CmdRemoveTkipMicMappingKey (hCmdBld, pKey, fCb, hCb) != TI_OK)
                return (TI_NOK);
            break;
        
        case KEY_AES:
            if (cmdBld_CmdRemoveAesMappingKey (hCmdBld, pKey, fCb, hCb) != TI_OK)
                return TI_NOK;
            break;
        
        #ifdef GEM_SUPPORTED
            case KEY_GEM:
                if (cmdBld_CmdRemoveGemMappingKey (hCmdBld, pKey, fCb, hCb) != TI_OK)
                    return TI_NOK;
                break;
        #endif
        
        default:
            return TI_NOK;
    }
    
    return TI_OK;
}


/*
 * ----------------------------------------------------------------------------
 * Function : cmdBld_CmdRemoveKey
 *
 * Input    : 
 * Output   :
 * Process  :
 * Note(s)  :
 * -----------------------------------------------------------------------------
 */
TI_STATUS cmdBld_CmdRemoveKey (TI_HANDLE hCmdBld, TSecurityKeys* pKey, void *fCb, TI_HANDLE hCb)
{
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8  keyIdx  = (TI_UINT8)pKey->keyIndex;

    /* Clear the remove key in the reconfigure data base */
    (DB_KEYS(pCmdBld).pReconfKeys + keyIdx)->keyType = KEY_NULL;

    switch (pCmdBld->tSecurity.eSecurityMode)
    {
        case TWD_CIPHER_WEP:
        case TWD_CIPHER_WEP104:
				return cmdBld_CmdRemoveWepDefaultKey (hCmdBld, pKey, fCb, hCb);
        case TWD_CIPHER_TKIP:
        case TWD_CIPHER_AES_CCMP:
        #ifdef GEM_SUPPORTED
            case TWD_CIPHER_GEM:
        #endif
            return cmdBld_CmdRemoveWpaKey (hCmdBld, pKey, fCb, hCb);
        
        default:
            return TI_NOK;
    }
}


/****************************************************************************
 *                      cmdBld_WepDefaultKeyAdd()
 ****************************************************************************
 * DESCRIPTION: Set the actual default key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdAddWepDefaultKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb) 
{
    TI_STATUS  status;
    TI_UINT8   sMacAddrDummy[6]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    /* Non WEP keys are trashed*/
    if (aSecurityKey->keyType != KEY_WEP)
    {
        return TI_NOK;
    }

    status = cmdBld_CmdIeSetKey (hCmdBld, 
                                 KEY_ADD_OR_REPLACE,
                                 sMacAddrDummy, 
                                 aSecurityKey->encLen, 
                                 CIPHER_SUITE_WEP,
                                 aSecurityKey->keyIndex, 
                                 (TI_UINT8*)aSecurityKey->encKey, 
                                 0,
                                 0,
                                 fCb, 
                                 hCb);
    return status;
}

/****************************************************************************
 *                      cmdBld_WepDefaultKeyRemove()
 ****************************************************************************
 * DESCRIPTION: Set the actual default key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdSetWepDefaultKeyId (TI_HANDLE hCmdBld, TI_UINT8 aKeyIdVal, void *fCb, TI_HANDLE hCb) 
{
    TCmdBld  *pCmdBld          = (TCmdBld *)hCmdBld;
    TI_UINT8 sMacAddrDummy[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    /* Save the deafult key ID for reconfigure phase */
    DB_KEYS(pCmdBld).bDefaultKeyIdValid  = TI_TRUE;
    DB_KEYS(pCmdBld).uReconfDefaultKeyId = aKeyIdVal;

    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_SET_ID,
                               sMacAddrDummy, 
                               0, 
                               CIPHER_SUITE_WEP,
                               aKeyIdVal, 
                               0, 
                               0, 
                               0,
                               fCb, 
                               hCb);
}


/****************************************************************************
 *                      cmdBld_WepDefaultKeyRemove()
 ****************************************************************************
 * DESCRIPTION: Set the actual default key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdRemoveWepDefaultKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb) 
{
    TI_UINT8  sMacAddrDummy[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    /* Non WEP keys are trashed*/
    if (aSecurityKey->keyType != KEY_WEP)
    {
        return TI_NOK;
    }

    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_REMOVE,
                               sMacAddrDummy, 
                               aSecurityKey->encLen, 
                               CIPHER_SUITE_WEP,
                               aSecurityKey->keyIndex, 
                               (TI_UINT8*)aSecurityKey->encKey, 
                               0, 
                               0,
                               fCb, 
                               hCb);
}

/****************************************************************************
 *                      cmdBld_WepMappingKeyAdd()
 ****************************************************************************
 * DESCRIPTION: Set the actual mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdAddWepMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_ADD_OR_REPLACE,
                               (TI_UINT8*)aSecurityKey->macAddress, 
                               aSecurityKey->encLen, 
                               CIPHER_SUITE_WEP,
                               aSecurityKey->keyIndex, 
                               (TI_UINT8*)aSecurityKey->encKey, 
                               0, 
                               0,
                               fCb, 
                               hCb);
}

/****************************************************************************
 *                      cmdBld_WepMappingKeyRemove()
 ****************************************************************************
 * DESCRIPTION: Set the actual mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdRemoveWepMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
	/*In the new security interface it is not allowed to remove uni-cast keys. it will be cleaned on the next join command*/
	if (!MAC_BROADCAST(aSecurityKey->macAddress) ) 
	{
		return TI_OK;
	}
    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_REMOVE,
                               (TI_UINT8*)aSecurityKey->macAddress, 
                               aSecurityKey->encLen, 
                               CIPHER_SUITE_WEP,
                               aSecurityKey->keyIndex, 
                               (TI_UINT8*)aSecurityKey->encKey, 
                               0, 
                               0,
                               fCb, 
                               hCb);
}


/****************************************************************************
 *                      cmdBld_TkipMicMappingKeyAdd()
 ****************************************************************************
 * DESCRIPTION: Set the actual mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdAddTkipMicMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8      keyType;
    TI_UINT8      keyBuffer[KEY_SIZE_TKIP];

    keyType = CIPHER_SUITE_TKIP;

    os_memoryCopy (pCmdBld->hOs, (void*)(&keyBuffer[0]), (void*)aSecurityKey->encKey, 16);
    os_memoryCopy (pCmdBld->hOs, (void*)(&keyBuffer[16]), (void*)aSecurityKey->micRxKey, 8);
    os_memoryCopy (pCmdBld->hOs, (void*)(&keyBuffer[24]), (void*)aSecurityKey->micTxKey, 8);

    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_ADD_OR_REPLACE,
                               (TI_UINT8*)aSecurityKey->macAddress, 
                               KEY_SIZE_TKIP, 
                               keyType,
                               aSecurityKey->keyIndex, 
                               (TI_UINT8*)keyBuffer, 
                               pCmdBld->uSecuritySeqNumLow, 
                               pCmdBld->uSecuritySeqNumHigh,
                               fCb, 
                               hCb);
}

/****************************************************************************
 *                      cmdBld_TkipMappingKeyAdd()
 ****************************************************************************
 * DESCRIPTION: Set the actual mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdRemoveTkipMicMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
    TI_UINT8 keyType;

    keyType = CIPHER_SUITE_TKIP;

	/*In the new security interface it is not allowed to remove uni-cast keys. it will be cleaned on the next join command*/
	if (!MAC_BROADCAST(aSecurityKey->macAddress) ) 
	{
		return TI_OK;
	}


    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_REMOVE,
                               (TI_UINT8*)aSecurityKey->macAddress, 
                               aSecurityKey->encLen, 
                               keyType,
                               aSecurityKey->keyIndex, 
                               (TI_UINT8*)aSecurityKey->encKey,
                               0,
                               0,
                               fCb, 
                               hCb);
}


/****************************************************************************
 *                      cmdBld_AesMappingKeyAdd()
 ****************************************************************************
 * DESCRIPTION: Set the actual Aes mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdAddAesMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8      keyType;

    keyType = CIPHER_SUITE_AES;

    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_AesMappingKeyAdd: uSecuritySeqNumHigh=%ld, pHwCtrl->uSecuritySeqNumLow=%ld \n", pCmdBld->uSecuritySeqNumHigh, pCmdBld->uSecuritySeqNumLow);

    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_ADD_OR_REPLACE,
                               (TI_UINT8*)aSecurityKey->macAddress, 
                               aSecurityKey->encLen, keyType,
                               aSecurityKey->keyIndex, 
                               (TI_UINT8*)aSecurityKey->encKey, 
                               pCmdBld->uSecuritySeqNumLow, 
                               pCmdBld->uSecuritySeqNumHigh,
                               fCb, 
                               hCb);
}


 /****************************************************************************
 *                      cmdBld_AesMappingKeyRemove()
 ****************************************************************************
 * DESCRIPTION: Remove  Aes mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdRemoveAesMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
    TI_UINT8  keyType;

    keyType = CIPHER_SUITE_AES;

	/*In the new security interface it is not allowed to remove uni-cast keys. it will be cleaned on the next join command*/
	if (!MAC_BROADCAST(aSecurityKey->macAddress) ) 
	{
		return TI_OK;
	}

	return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_REMOVE,
                               (TI_UINT8*)aSecurityKey->macAddress, 
                               aSecurityKey->encLen, 
                               keyType,
                               aSecurityKey->keyIndex, 
                               (TI_UINT8*)aSecurityKey->encKey, 
                               0, 
                               0,
                               fCb, 
                               hCb);
 }

/****************************************************************************
 *                      cmdBld_CmdSetStaState()
 ****************************************************************************
 * DESCRIPTION: Set station status . 
 * 
 * INPUTS: None
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdSetStaState (TI_HANDLE hCmdBld, TI_UINT8 staState, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);

    pWlanParams->bStaConnected = TI_TRUE;
    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Sending StaState %d\n",staState);

    return cmdBld_CmdIeSetStaState (hCmdBld, staState, fCb, hCb);
}

#ifdef GEM_SUPPORTED
/****************************************************************************
 *                      cmdBld_CmdAddGemMappingKey()
 ****************************************************************************
 * DESCRIPTION: Set the actual GEM mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdAddGemMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8      keyType;

    keyType = CIPHER_SUITE_GEM;

    return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_ADD_OR_REPLACE,
                               aSecurityKey->macAddress, 
                               MAX_KEY_SIZE, 
                               keyType,
                               aSecurityKey->keyIndex, 
							   aSecurityKey->encKey,
                               pCmdBld->uSecuritySeqNumLow, 
                               pCmdBld->uSecuritySeqNumHigh,
                               fCb, 
                               hCb);
}


/****************************************************************************
 *                      cmdBld_CmdRemoveGemMappingKey()
 ****************************************************************************
 * DESCRIPTION: Remove  GEM mapping key
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdRemoveGemMappingKey (TI_HANDLE hCmdBld, TSecurityKeys* aSecurityKey, void *fCb, TI_HANDLE hCb)
{
    TI_UINT8  keyType;

    keyType = CIPHER_SUITE_GEM;

	/*In the new security interface it is not allowed to remove uni-cast keys. it will be cleaned on the next join command*/
	if (!MAC_BROADCAST(aSecurityKey->macAddress) ) 
	{
		return TI_OK;
	}

	return cmdBld_CmdIeSetKey (hCmdBld, 
                               KEY_REMOVE,
                               aSecurityKey->macAddress, 
                               aSecurityKey->encLen, 
                               keyType,
                               aSecurityKey->keyIndex, 
                               aSecurityKey->encKey, 
                               0, 
                               0,
                               fCb, 
                               hCb);
 }
#endif /*GEM_SUPPORTED*/

/****************************************************************************
 *                      cmdBld_healthCheck()
 ****************************************************************************
 * DESCRIPTION: 
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: 
 ****************************************************************************/
TI_STATUS cmdBld_CmdHealthCheck (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CmdIeHealthCheck\n");
    
    return cmdBld_CmdIeHealthCheck (hCmdBld, fCb, hCb);
}

TI_STATUS cmdBld_CmdTest (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, TTestCmd* pTestCmd)
{
    return cmdBld_CmdIeTest (hCmdBld, fCb, hCb, pTestCmd);
}

