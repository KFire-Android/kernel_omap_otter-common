/*
 * scanMngr.c
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

/** \file  scanMngr.c
 *  \brief This file include the scan manager module implementation
 *
 *  \see   scanMngr.h, scanMngrApi.h scanMngrTypes.h
 */


#define __FILE_ID__  FILE_ID_9
#include "TWDriver.h"
#include "roamingMngrApi.h"
#include "osApi.h"
#include "timer.h"
#include "ScanCncn.h"
#include "report.h"
#include "regulatoryDomainApi.h"
#include "siteMgrApi.h"
#include "scanMngr.h"
#include "DrvMainModules.h"
#include "EvHandler.h"
#include "apConnApi.h"


/*
 ***********************************************************************
 *  Internal functions
 ***********************************************************************
 */
/***************************************************************************
*                           reminder64                                     *
****************************************************************************
DESCRIPTION:    returns the reminder of a 64 bit number division by a 32
                bit number.
                                                                                                   
INPUT:      The dividee (64 bit number to divide)
            The divider (32 bit number to divide by)

OUTPUT:     
            

RETURN:     The reminder
****************************************************************************/
static TI_UINT32 reminder64( TI_UINT64 dividee, TI_UINT32 divider )
{
    TI_UINT32 divideeHigh, divideeLow, partA, partB, mod28n, mod24n, mod16n, partA8n, mod8n, mod4n;

    divideeHigh = INT64_HIGHER( dividee );
    divideeLow = INT64_LOWER( dividee );

    mod8n = 256 % divider;
    mod4n = 16 % divider;

    partA = (mod4n * (divideeHigh % divider)) % divider;
    partA8n = (partA * mod4n) % divider;
    mod16n = (partA8n * mod8n) % divider;
    mod24n = (mod8n * mod16n) % divider;
    mod28n = (mod4n * mod24n) % divider;

    partB = (mod4n * mod28n) % divider;
    return ( partB + (divideeLow % divider)) % divider;
}



static void scanMngr_setManualScanDefaultParams(TI_HANDLE hScanMngr)
{
    scanMngr_t* 	pScanMngr = (scanMngr_t*)hScanMngr;

    pScanMngr->manualScanParams.desiredSsid.len = 1;        /* will be set by the scan concentrator */
    pScanMngr->manualScanParams.scanType= SCAN_TYPE_NORMAL_ACTIVE;
    pScanMngr->manualScanParams.band = RADIO_BAND_2_4_GHZ;
    pScanMngr->manualScanParams.probeReqNumber =  3;
    pScanMngr->manualScanParams.probeRequestRate = (ERateMask)RATE_MASK_UNSPECIFIED;
}


static void scanMngr_reportContinuousScanResults (TI_HANDLE hScanMngr,	EScanCncnResultStatus resultStatus)
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    BssListEx_t   BssListEx;


    if (resultStatus == SCAN_CRS_SCAN_COMPLETE_OK)
    {
        BssListEx.pListOfAPs = scanMngr_getBSSList(hScanMngr);
        BssListEx.scanIsRunning = pScanMngr->bContinuousScanStarted; /* false = stopped */
        EvHandlerSendEvent(pScanMngr->hEvHandler, IPC_EVENT_CONTINUOUS_SCAN_REPORT, (TI_UINT8*)&BssListEx, sizeof(BssListEx_t));
    }
    else
    {
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_ERROR, "scanMngr_reportContinuousScanResults failed. scan status %d\n", resultStatus);
    }
}



/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Frees scan manager resources.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrFreeMem (TI_HANDLE hScanMngr)
{
    scanMngr_t* pScanMngr = hScanMngr;
    TI_UINT8 i;

    /* free frame storage space */
    for (i = 0; i < MAX_SIZE_OF_BSS_TRACK_LIST; i++)
    {
        if (pScanMngr->BSSList.BSSList[i].pBuffer)
        {
            os_memoryFree (pScanMngr->hOS, pScanMngr->BSSList.BSSList[i].pBuffer, MAX_BEACON_BODY_LENGTH);
        }
    }

    /* free the timer */
    if (pScanMngr->hContinuousScanTimer)
    {
        tmr_DestroyTimer (pScanMngr->hContinuousScanTimer);
    }

    /* free the scan manager object */
    os_memoryFree (pScanMngr->hOS, hScanMngr, sizeof(scanMngr_t));
}

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Callback used by the scan concentrator for immediate scan result.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param resultStatus - reason for calling this function (frame received / scan complete).\n
 * \param frameInfo - frame related information (in case of a frame reception).\n
 * \param SPSStatus - bitmap indicating which channels were scan, in case of an SPS scan.\n
 */
void scanMngr_immedScanCB( TI_HANDLE hScanMngr, EScanCncnResultStatus resultStatus, 
                           TScanFrameInfo* frameInfo, TI_UINT16 SPSStatus )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TScanBandPolicy* aPolicy;
    EScanCncnResultStatus nextResultStatus;

    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_immedScanCB called, hScanMngr=0x%x, resultStatus=%d", hScanMngr, resultStatus);
    
    switch (resultStatus)
    {
    /* if this function is called because a frame was received, update the BSS list accordingly */
    case SCAN_CRS_RECEIVED_FRAME:
        scanMngrUpdateReceivedFrame( hScanMngr, frameInfo );
        break;

    /* scan was completed successfuly */
    case SCAN_CRS_SCAN_COMPLETE_OK:
        /* act according to immediate scan state */
        switch (pScanMngr->immedScanState)
        {
        /* immediate scan on G finished */
        case SCAN_ISS_G_BAND:
#ifdef TI_DBG
        pScanMngr->stats.ImmediateGByStatus[ resultStatus ]++;
#endif
        /* check if another scan is needed (this time on A) */
        aPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_5_0_GHZ );
            if ( (NULL != aPolicy) &&
                 (SCAN_TYPE_NO_SCAN != aPolicy->immediateScanMethod.scanType))
        {
            /* build scan command */
            scanMngrBuildImmediateScanCommand( hScanMngr, aPolicy, pScanMngr->bImmedNeighborAPsOnly );

            /* if no channels are available, report error */
            if ( 0 < pScanMngr->scanParams.numOfChannels )
            {
                /* mark that immediate scan is running on band A */
                pScanMngr->immedScanState = SCAN_ISS_A_BAND;

                /* send scan command to scan concentrator */
                nextResultStatus = 
                    scanCncn_Start1ShotScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_IMMED, &(pScanMngr->scanParams));
                if ( SCAN_CRS_SCAN_RUNNING != nextResultStatus )
                {
                    pScanMngr->immedScanState = SCAN_ISS_IDLE;
                    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start immediate scan on band A, return code %d.\n", resultStatus);
#ifdef TI_DBG
                    pScanMngr->stats.ImmediateAByStatus[ nextResultStatus ]++;
#endif
                    scanMngr_immediateScanComplete(hScanMngr,SCAN_MRS_SCAN_COMPLETE_OK);
                }
            }
            else
            {
                /* mark that immediate scan is not running */
                pScanMngr->immedScanState = SCAN_ISS_IDLE;
                
                /* no channels are actually available for scan - notify the roaming manager of the scan complete */
                scanMngr_immediateScanComplete(hScanMngr,SCAN_MRS_SCAN_COMPLETE_OK);
            }
        }
        else
        {
            /* mark that immediate scan is not running */
            pScanMngr->immedScanState = SCAN_ISS_IDLE;

            /* otherwise, notify the roaming manager of the scan complete */
            scanMngr_immediateScanComplete(hScanMngr,SCAN_MRS_SCAN_COMPLETE_OK);
        }
        break;

        /* stop immediate scan was requested */
        case SCAN_ISS_STOPPING:
            /* mark that immediate scan is not running */
            pScanMngr->immedScanState = SCAN_ISS_IDLE;

            /* notify the roaming manager of the scan complete */
            scanMngr_immediateScanComplete(hScanMngr,SCAN_MRS_SCAN_STOPPED);
            break;

        /* Scan completed on A band */
        case SCAN_ISS_A_BAND:
            /* mark that immediate scan is not running */
            pScanMngr->immedScanState = SCAN_ISS_IDLE;
#ifdef TI_DBG
            pScanMngr->stats.ImmediateAByStatus[ resultStatus ]++;
#endif
            /* otherwise, notify the roaming manager of the scan complete */
            scanMngr_immediateScanComplete(hScanMngr,SCAN_MRS_SCAN_COMPLETE_OK);
            break;
        
        default:
            /* should not be at any other stage when CB is invoked */
            TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Immediate scan CB called with scan complete TI_OK reason in state:%d", pScanMngr->immedScanState);

            /* reset continuous scan to idle */
            pScanMngr->immedScanState = SCAN_ISS_IDLE;
            break;
        }
        break;

    /* scan was completed due to an error! */
    default:
#ifdef TI_DBG
        switch (pScanMngr->immedScanState)
        {
        case SCAN_ISS_G_BAND:
            pScanMngr->stats.ImmediateGByStatus[ resultStatus ]++;
            break;

        case SCAN_ISS_A_BAND:
            pScanMngr->stats.ImmediateAByStatus[ resultStatus ]++;
            break;

        default:
            break;
        }
#endif
        /* mark that immediate scan is not running */
        pScanMngr->immedScanState = SCAN_ISS_IDLE;     
        scanMngr_immediateScanComplete(hScanMngr,scanMngrConvertResultStatus(resultStatus));
        break;
    }
}

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Callback used by the scan concentrator for continuous scan result.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param resultStatus - reason for calling this function (frame received / scan complete).\n
 * \param frameInfo - frame related info (in case of a frame reception).\n
 * \param SPSStatus - bitmap indicating which channels were scan, in case of an SPS scan.\n
 */
void scanMngr_contScanCB( TI_HANDLE hScanMngr, EScanCncnResultStatus resultStatus, 
                         TScanFrameInfo* frameInfo, TI_UINT16 SPSStatus )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TScanBandPolicy *aPolicy;
    EScanCncnResultStatus nextResultStatus;

    TRACE3( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_contScanCB called, hScanMngr=0x%x, resultStatus=%d, SPSStatus=%d\n", hScanMngr, resultStatus, SPSStatus);

    /* It looks like it never happens. Anyway decided to check */
    if ( pScanMngr->scanParams.numOfChannels > SCAN_MAX_NUM_OF_SPS_CHANNELS_PER_COMMAND )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngr_contScanCB. pScanMngr->scanParams.numOfChannels=%d exceeds the limit %d\n",
                pScanMngr->scanParams.numOfChannels, SCAN_MAX_NUM_OF_SPS_CHANNELS_PER_COMMAND);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    switch (resultStatus)
    {
    /* frame received - update BSS list accordingly */
    case SCAN_CRS_RECEIVED_FRAME:
        scanMngrUpdateReceivedFrame( hScanMngr, frameInfo );
        break;

    /* scan was completed successfully - either continue to next stage or simply finish this cycle */
    case SCAN_CRS_SCAN_COMPLETE_OK:
#ifdef SCAN_MNGR_DBG
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Continuous scan completes successfuly.\n");
        scanMngrDebugPrintBSSList( hScanMngr );
#endif
#ifdef TI_DBG
        if ( SCAN_TYPE_SPS == pScanMngr->scanParams.scanType )
        {
            int i;

            /*update SPS channels attendant statistics */
            for ( i = 0; i < pScanMngr->scanParams.numOfChannels; i++ )
            {
                if ( TI_FALSE == WAS_SPS_CHANNEL_ATTENDED( SPSStatus, i ))
                {
                    pScanMngr->stats.SPSChannelsNotAttended[ i ]++;
                }
            }
        }
#endif
    
        /* first, remove APs that were not tracked. Note that this function does NOT
           increase the retry counter, and therefore there's no harm in calling it even if only 
           some of the APs were searched in the previous tracking command, or previous command was
           discovery */
        scanMngrPerformAging( hScanMngr );
        

        /* if new BSS's were found (or enough scan iterations passed w/o finding any), notify the roaming manager */
        if ( ((TI_TRUE == pScanMngr->bNewBSSFound) || 
              (SCAN_MNGR_CONSEC_SCAN_ITER_FOR_PRE_AUTH < pScanMngr->consecNotFound)) &&
             (pScanMngr->BSSList.numOfEntries > 0)) /* in case no AP was found for specified iterations number,
                                                        but no AP is present, and so is pre-auth */
        {
            pScanMngr->bNewBSSFound = TI_FALSE;
            pScanMngr->consecNotFound = 0;
            roamingMngr_updateNewBssList( pScanMngr->hRoamingMngr, (bssList_t*)&(pScanMngr->BSSList));

             if (SCANNING_OPERATIONAL_MODE_MANUAL == pScanMngr->scanningOperationalMode)
             {
                 scanMngr_reportContinuousScanResults(hScanMngr, resultStatus);
             }
        }

        /* act according to continuous scan state */
        switch (pScanMngr->contScanState)
        {
        case SCAN_CSS_TRACKING_G_BAND:
#ifdef TI_DBG
            pScanMngr->stats.TrackingGByStatus[ resultStatus ]++;
#endif    
            TRACE0(pScanMngr->hReport , REPORT_SEVERITY_INFORMATION, "\n Starting SCAN_CSS_TRACKING_G_BAND \n");
          /* if necessary, attempt tracking on A */
            aPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_5_0_GHZ );
            /* if a policy is defined for A band tracking, attempt to perform it */
            if ( (NULL != aPolicy) &&
                 (SCAN_TYPE_NO_SCAN != aPolicy->trackingMethod.scanType))
            {
                /* recalculate current TSF, to adjust the TSF read at the beginning of
                   the continuous scan process with the tracking on G duration */
                pScanMngr->currentTSF += 
                    ((os_timeStampMs( pScanMngr->hOS ) - pScanMngr->currentHostTimeStamp) * 1000);

                /* build scan command */
                scanMngrBuildTrackScanCommand( hScanMngr, aPolicy, RADIO_BAND_5_0_GHZ );

                /* if channels are available for tracking on A */
                if ( 0 < pScanMngr->scanParams.numOfChannels )
                {
                    /* mark that continuous scan is now tracking on A */
                    pScanMngr->contScanState = SCAN_CSS_TRACKING_A_BAND;

                    /* send scan command */
                    nextResultStatus = 
                        scanCncn_Start1ShotScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_CONT, &(pScanMngr->scanParams));
                    if ( SCAN_CRS_SCAN_RUNNING != nextResultStatus )
                    {
                        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start tracking continuous scan on band A, return code %d.\n", resultStatus);
#ifdef TI_DBG
                        pScanMngr->stats.TrackingAByStatus[ nextResultStatus ]++;
#endif
                        pScanMngr->contScanState = SCAN_CSS_IDLE;
                    }
#ifdef SCAN_MNGR_DBG
                    TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Tracking on A started.\n");
#endif
                    return;
                }
            }
            /* in case a TSF error was received on last continuous scan cycle, mark (now, that tracking
               on both bands was attempted), that TSF values are synchronized */
            pScanMngr->bSynchronized = TI_TRUE;
            
            /* the break is missing on purpose: if tracking on A was not successful (or not needed), continue to discovery */

        case SCAN_CSS_TRACKING_A_BAND:
#ifdef TI_DBG
            /* update stats - since there's no break above, we must check that the state is indeed tracking on A */
            if ( SCAN_CSS_TRACKING_A_BAND == pScanMngr->contScanState )
            {
                pScanMngr->stats.TrackingAByStatus[ resultStatus ]++;
            }
#endif
            TRACE0(pScanMngr->hReport , REPORT_SEVERITY_INFORMATION, "\n SCAN_CSS_TRACKING_A_BAND \n");
            /* if necessary and possible, attempt discovery */
            if ( (SCAN_SDP_NO_DISCOVERY != pScanMngr->currentDiscoveryPart) &&
                 (pScanMngr->BSSList.numOfEntries <= pScanMngr->scanPolicy.BSSNumberToStartDiscovery))
            {
                /* build scan command */
                scanMngrBuildDiscoveryScanCommand( hScanMngr );

                /* if channels are available for discovery */
                if ( 0 < pScanMngr->scanParams.numOfChannels )
                {
                    /* mark that continuous scan is now in discovery state */
                    pScanMngr->contScanState = SCAN_CSS_DISCOVERING;

                    /* mark that no new APs were discovered in this discovery operation */
                    pScanMngr->bNewBSSFound = TI_FALSE;

                    /* send scan command */
                    nextResultStatus = 
                        scanCncn_Start1ShotScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_CONT, &(pScanMngr->scanParams));
                    if ( SCAN_CRS_SCAN_RUNNING != nextResultStatus )
                    {
                        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start discovery continuous scan, nextResultStatus %d.\n", nextResultStatus);
                        pScanMngr->contScanState = SCAN_CSS_IDLE;
                    }
#ifdef SCAN_MNGR_DBG
                    TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Disocvery started.\n");
#endif
                    return;
                }
            }

            /* the break is missing on purpose: if discovery was not successful (or not needed), finish scan cycle */

        case SCAN_CSS_DISCOVERING:
#ifdef TI_DBG
            /* update stats - since there's no break above, we must check that the state is indeed discocery */
            if ( SCAN_CSS_DISCOVERING == pScanMngr->contScanState )
            {
                if ( RADIO_BAND_2_4_GHZ == pScanMngr->statsLastDiscoveryBand )
                {
                    pScanMngr->stats.DiscoveryGByStatus[ resultStatus ]++;
                }
                else
                {
                    pScanMngr->stats.DiscoveryAByStatus[ resultStatus ]++;
                }
            }
#endif
            /* continuous scan cycle is complete */
            pScanMngr->contScanState = SCAN_CSS_IDLE;

            break;

        case SCAN_CSS_STOPPING:
            /* continuous scan cycle is complete */
            pScanMngr->contScanState = SCAN_CSS_IDLE;
            break;

        default:
            /* should not be at any other stage when CB is invoked */
            TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Continuous scan CB called with scan complete TI_OK reason in state:%d\n", pScanMngr->contScanState);
            
            /* reset continuous scan to idle */
            pScanMngr->contScanState = SCAN_CSS_IDLE;
            pScanMngr->bNewBSSFound = TI_FALSE;
            break;
        }
        break;

    /* SPS scan was completed with TSF error */
    case SCAN_CRS_TSF_ERROR:
        /* report the recovery event */
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Continuous scan callback called with TSF error indication\n");
        /* mark that the TSF values are no longer valid */
        pScanMngr->bSynchronized = TI_FALSE;
#ifdef TI_DBG
        switch ( pScanMngr->contScanState )
        {
        case SCAN_CSS_TRACKING_G_BAND:
            pScanMngr->stats.TrackingGByStatus[ resultStatus ]++;
            break;

        case SCAN_CSS_TRACKING_A_BAND:
            pScanMngr->stats.TrackingAByStatus[ resultStatus ]++;
            break;

        default:
            break;
        }
#endif
        /* stop continuous scan cycle for this time (to avoid tracking using discovery only on A, thus 
           having mixed results - some are synchronized, some are not */
        pScanMngr->contScanState = SCAN_CSS_IDLE;
        break;

    default:
        /* report the status received */
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Continuous scan CB called with status %d\n", resultStatus);

        /* also perform aging (since it does not increase counter, no harm done if this was not tracking */
        scanMngrPerformAging( hScanMngr );
#ifdef TI_DBG
        switch ( pScanMngr->contScanState )
        {
        case SCAN_CSS_TRACKING_G_BAND:
            pScanMngr->stats.TrackingGByStatus[ resultStatus ]++;
            break;

        case SCAN_CSS_TRACKING_A_BAND:
            pScanMngr->stats.TrackingAByStatus[ resultStatus ]++;
            break;

        case SCAN_CSS_DISCOVERING:
            if ( RADIO_BAND_2_4_GHZ == pScanMngr->statsLastDiscoveryBand )
            {
                pScanMngr->stats.DiscoveryGByStatus[ resultStatus ]++;
            }
            else
            {
                pScanMngr->stats.DiscoveryGByStatus[ resultStatus ]++;
            }
        default:
            break;
        }
#endif  
        /* finish scan for this iteration */
        pScanMngr->contScanState = SCAN_CSS_IDLE;
        break;
    }
}

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Sets the scan policy.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param scanPolicy - a pointer to the policy data.\n
 */
void scanMngr_setScanPolicy( TI_HANDLE hScanMngr, TScanPolicy* scanPolicy )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_setScanPolicy called, hScanMngr=0x%x.\n", hScanMngr);
#ifdef SCAN_MNGR_DBG
    scanMngrTracePrintScanPolicy( scanPolicy );
#endif

    /* if continuous or immediate scan are running, indicate that they shouldn't proceed to next scan (if any),
       and stop the scan operation (in case a triggered scan was in progress and the voice was stopped, the scan
       must be stopped or a recovery will occur */
    if ( pScanMngr->contScanState != SCAN_CSS_IDLE )
    {
        pScanMngr->contScanState = SCAN_CSS_STOPPING;
        scanCncn_StopScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_CONT );
    }
    if ( pScanMngr->immedScanState != SCAN_ISS_IDLE )
    {
        pScanMngr->immedScanState = SCAN_ISS_STOPPING;
        scanCncn_StopScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_IMMED );
    }

    /* set new scan policy */
    os_memoryCopy( pScanMngr->hOS, &(pScanMngr->scanPolicy), scanPolicy, sizeof(TScanPolicy));

    /* remove all tracked APs that are not on a policy defined channel (neighbor APs haven't changed,
       so there's no need to check them */
    scanMngrUpdateBSSList( hScanMngr, TI_FALSE, TI_TRUE );

    /* if continuous scan timer is running, stop it */
    if (pScanMngr->bTimerRunning)
    {
        tmr_StopTimer (pScanMngr->hContinuousScanTimer);
        pScanMngr->bTimerRunning = TI_FALSE;
    }

    /* if continuous scan was started, start the timer using the new intervals */
    if (pScanMngr->bContinuousScanStarted)
    {
        TI_UINT32 uTimeout = pScanMngr->bLowQuality ?
                             pScanMngr->scanPolicy.deterioratingScanInterval :
                             pScanMngr->scanPolicy.normalScanInterval;

        pScanMngr->bTimerRunning = TI_TRUE;

        tmr_StartTimer (pScanMngr->hContinuousScanTimer,
                        scanMngr_GetUpdatedTsfDtimMibForScan,
                        (TI_HANDLE)pScanMngr,
                        uTimeout,
                        TI_TRUE);
    }

    /* reset discovery counters */
    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_2_4_GHZ ] = 0;
    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_5_0_GHZ ] = 0;
    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_2_4_GHZ ] = 0;
    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_5_0_GHZ ] = 0;
    /* set current discovery part to first part */
    pScanMngr->currentDiscoveryPart = SCAN_SDP_NEIGHBOR_G;
    /* now advance discovery part to first valid part */
    scanMngrSetNextDiscoveryPart( hScanMngr );
}

/**
 * \\n
 * \date 06-Feb-2006\n
 * \brief CB function for current TSF and last beacon TSF and DTIM read.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param status - read status (TI_OK / TI_NOK).\n
 * \param CB_buf - a pointer to the data read.\n
 */
void scanMngrGetCurrentTsfDtimMibCB(TI_HANDLE hScanMngr, TI_STATUS status, TI_UINT8* CB_buf) 
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    
    os_memoryCopy(pScanMngr->hOS, (TI_UINT8*)&(pScanMngr->currTsfDtimMib), CB_buf, sizeof(TTsfDtim));

    /* set the current TSF and last beacon TSF and DTIM count */
    INT64_HIGHER( pScanMngr->currentTSF ) = pScanMngr->currTsfDtimMib.CurrentTSFHigh;
    INT64_LOWER( pScanMngr->currentTSF )  = pScanMngr->currTsfDtimMib.CurrentTSFLow;

    INT64_HIGHER( pScanMngr->lastLocalBcnTSF ) = pScanMngr->currTsfDtimMib.lastTBTTHigh;
    INT64_LOWER( pScanMngr->lastLocalBcnTSF )  = pScanMngr->currTsfDtimMib.lastTBTTLow;
    
    pScanMngr->lastLocalBcnDTIMCount = pScanMngr->currTsfDtimMib.LastDTIMCount;
    
    TRACE5( pScanMngr->hReport , REPORT_SEVERITY_INFORMATION, "\n currentTSF = %u-%u lastLocalBcnTSF = %u-%u lastDTIMCount = %d \n", INT64_HIGHER( pScanMngr->currentTSF ), INT64_LOWER( pScanMngr->currentTSF ), INT64_HIGHER( pScanMngr->lastLocalBcnTSF ), INT64_LOWER( pScanMngr->lastLocalBcnTSF ), pScanMngr->lastLocalBcnDTIMCount );

    /* get the current host time stamp */
    pScanMngr->currentHostTimeStamp = os_timeStampMs( pScanMngr->hOS );

    /* now that the current TSF and last beacon TSF had been retrieved from the FW,
       continuous scan may proceed */
    scanMngrPerformContinuousScan(hScanMngr);
}

/**
 * \\n
 * \date 06-Feb-2006\n
 * \brief requests current TSF and last beacon TSF and DTIM from the FW.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bTwdInitOccured - Indicates if TWDriver recovery occured since timer started.\n
 */
void scanMngr_GetUpdatedTsfDtimMibForScan (TI_HANDLE hScanMngr, TI_BOOL bTwdInitOccured)
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TTwdParamInfo param;
    TI_STATUS reqStatus = TI_OK;
    
    TRACE0( pScanMngr->hReport , REPORT_SEVERITY_INFORMATION, "\nscanMngr_GetUpdatedTsfDtimMibForScan called\n");
    
    /* Getting the current TSF and DTIM values */
    param.paramType = TWD_TSF_DTIM_MIB_PARAM_ID;
    param.content.interogateCmdCBParams.fCb = (void *)scanMngrGetCurrentTsfDtimMibCB;
    param.content.interogateCmdCBParams.hCb = hScanMngr;
    param.content.interogateCmdCBParams.pCb = (TI_UINT8*)&pScanMngr->currTsfDtimMib;
    reqStatus = TWD_GetParam (pScanMngr->hTWD, &param);
    if ( TI_OK != reqStatus )
    {
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_ERROR, ": getParam from HAL CTRL failed wih status: %d\n", reqStatus);
    }
}

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Starts a continuous scan operation.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrPerformContinuousScan( TI_HANDLE hScanMngr )
{

    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TScanBandPolicy *gPolicy, *aPolicy;
    EScanCncnResultStatus resultStatus;
    paramInfo_t param;

#ifdef SCAN_MNGR_DBG
    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngrPerformContinuousScan called, hScanMngr=0x%x.\n", hScanMngr);
    scanMngrDebugPrintBSSList( hScanMngr );
#endif
    
    /* this function is called due to continuous scan timer expiry, to start a new continuous scan cycle.
       If the continuous scan is anything but idle, a new cycle is not started. */
    if ( SCAN_CSS_IDLE != pScanMngr->contScanState )
    {
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Continuous scan timer expired and continuous scan state is:%d\n", pScanMngr->contScanState);
        return;
    }

    /* retrieve the current BSS DTIM period and beacon interval, for SPS DTIM avoidance
       calculations later. This is done before the continuous scan process is started, 
       to check that they are not zero (in case the STA disconnected and somehow the 
       scan manager was not notified of the event). If the STA disconnected, the continuous
       scan process is aborted */
    param.paramType = SITE_MGR_BEACON_INTERVAL_PARAM;
    siteMgr_getParam( pScanMngr->hSiteMngr, &param );
    pScanMngr->currentBSSBeaconInterval = param.content.beaconInterval;
    
    param.paramType = SITE_MGR_DTIM_PERIOD_PARAM;
    siteMgr_getParam( pScanMngr->hSiteMngr, &param );
    pScanMngr->currentBSSDtimPeriod = param.content.siteMgrDtimPeriod;

    /* now check that none of the above is zero */
    if ( (0 == pScanMngr->currentBSSBeaconInterval) || (0 == pScanMngr->currentBSSDtimPeriod))
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR, "Trying to start continuous scan cycle but DTIM period=%d and beacon interval=%d\n", pScanMngr->currentBSSDtimPeriod, pScanMngr->currentBSSBeaconInterval);
        return;
    }

    /* increase the consecutive not found counter */
    pScanMngr->consecNotFound++;

    /* first try tracking on G */
    gPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_2_4_GHZ );
    /* if a policy is defined for G band tracking, attempt to perform it */
    if ( (NULL != gPolicy) &&
         (SCAN_TYPE_NO_SCAN != gPolicy->trackingMethod.scanType))
    {
        /* build scan command */
        scanMngrBuildTrackScanCommand( hScanMngr, gPolicy, RADIO_BAND_2_4_GHZ );

        /* if channels are available for tracking on G */
        if ( 0 < pScanMngr->scanParams.numOfChannels )
        {
            /* mark that continuous scan is now tracking on G */
            pScanMngr->contScanState = SCAN_CSS_TRACKING_G_BAND;

            /* send scan command to scan concentrator with the required scan params according to scannig operational  mode */
            resultStatus = scanMngr_Start1ShotScan(hScanMngr, SCAN_SCC_ROAMING_CONT);
            if ( SCAN_CRS_SCAN_RUNNING != resultStatus )
            {
                TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start tracking continuous scan on G, return code %d.\n", resultStatus);
#ifdef TI_DBG
                pScanMngr->stats.TrackingGByStatus[ resultStatus ]++;
#endif
                pScanMngr->contScanState = SCAN_CSS_IDLE;
            }
#ifdef SCAN_MNGR_DBG
            TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Tracking on G started.\n");
#endif            
            return;
        }
    }

    /* if not, try tracking on A */
    aPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_5_0_GHZ );
    /* if a policy is defined for A band tracking, attempt to perform it */
    if ( (NULL != aPolicy) &&
         (SCAN_TYPE_NO_SCAN != aPolicy->trackingMethod.scanType))
    {
        /* build scan command */
        scanMngrBuildTrackScanCommand( hScanMngr, aPolicy, RADIO_BAND_5_0_GHZ );

        /* if channels are available for tracking on A */
        if ( 0 < pScanMngr->scanParams.numOfChannels )
        {
            /* mark that continuous scan is now tracking on A */
            pScanMngr->contScanState = SCAN_CSS_TRACKING_A_BAND;

            /* send scan command to scan concentrator with the required scan params according to scanning operational mode */
            resultStatus = scanMngr_Start1ShotScan(hScanMngr, SCAN_SCC_ROAMING_CONT);
            if ( SCAN_CRS_SCAN_RUNNING != resultStatus )
            {
                TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start tracking continuous scan on A, return code %d.\n", resultStatus);
#ifdef TI_DBG
                pScanMngr->stats.TrackingAByStatus[ resultStatus ]++;
#endif
                pScanMngr->contScanState = SCAN_CSS_IDLE;
            }
#ifdef SCAN_MNGR_DBG
            TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Tracking on A started.\n");
#endif
            return;
        }
    }
    /* in case a TSF error was received on last continuous scan cycle, mark (now, that tracking
       on both bands was attempted), that TSF values are synchronized */
    pScanMngr->bSynchronized = TI_TRUE;                   

    /* if this does not work as well, try discovery */
    /* discovery can be performed if discovery part is valid (this is maintained whenever a new policy or neighbor AP list
       is set, a discovery scan command is built, and a new neighbor AP is discovered) */
    if ( (SCAN_SDP_NO_DISCOVERY != pScanMngr->currentDiscoveryPart) &&
         (pScanMngr->BSSList.numOfEntries <= pScanMngr->scanPolicy.BSSNumberToStartDiscovery))
    {
        /* build scan command */
        scanMngrBuildDiscoveryScanCommand( hScanMngr );

        /* if channels are available for discovery */
        if ( 0 < pScanMngr->scanParams.numOfChannels )
        {
            /* mark that continuous scan is now in discovery state */
            pScanMngr->contScanState = SCAN_CSS_DISCOVERING;

            /* mark that no new BSS's were found (yet) */
            pScanMngr->bNewBSSFound = TI_FALSE;

            /* send scan command to scan concentrator with the required scan params according to scanning operational mode */
            resultStatus = scanMngr_Start1ShotScan(hScanMngr, SCAN_SCC_ROAMING_CONT);
            if ( SCAN_CRS_SCAN_RUNNING != resultStatus )
            {
                TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start discovery continuous scan, resultStatus %d.\n", resultStatus);
#ifdef TI_DBG
                if ( RADIO_BAND_2_4_GHZ == pScanMngr->statsLastDiscoveryBand )
                {
                    pScanMngr->stats.DiscoveryGByStatus[ resultStatus ]++;
                }
                else
                {
                    pScanMngr->stats.DiscoveryAByStatus[ resultStatus ]++;
                }
#endif
                pScanMngr->contScanState = SCAN_CSS_IDLE;
            }
#ifdef SCAN_MNGR_DBG
            TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Discovery started.\n");
#endif
            return;
        }
    }

    /* if we got here, no scan had executed successfully - print a warning */
    TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Unable to perform continuous scan.\n");
}

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Perform aging on the BSS list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrPerformAging( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TI_UINT8 BSSEntryIndex;

#ifdef SCAN_MNGR_DBG
    TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Performing Aging.\n");
#endif
    /* It looks like it never happens. Anyway decided to check */
    if (pScanMngr->BSSList.numOfEntries > MAX_SIZE_OF_BSS_TRACK_LIST)
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                 "scanMngrPerformAging problem. BSSList.numOfEntries=%d exceeds the limit %d\n",
                 pScanMngr->BSSList.numOfEntries, MAX_SIZE_OF_BSS_TRACK_LIST);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* loop on all entries in the BSS list */
    for ( BSSEntryIndex = 0; BSSEntryIndex < pScanMngr->BSSList.numOfEntries; )
    {
        /* if an entry failed enough consecutive track attempts - remove it */
        if ( pScanMngr->BSSList.scanBSSList[ BSSEntryIndex ].trackFailCount > 
             pScanMngr->scanPolicy.maxTrackFailures )
        {
            /* will replace this entry with one further down the array, if any. Therefore, index is not increased
               (because a new entry will be placed in the same index). If this is the last entry - the number of
               BSSes will be decreased, and thus the loop will exit */
#ifdef SCAN_MNGR_DBG
            TRACE7( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Aging: removing BSSID %2x:%2x:%2x:%2x:%2x:%2x from index: %d.\n", pScanMngr->BSSList.BSSList[ BSSEntryIndex ].BSSID[ 0 ], pScanMngr->BSSList.BSSList[ BSSEntryIndex ].BSSID[ 1 ], pScanMngr->BSSList.BSSList[ BSSEntryIndex ].BSSID[ 2 ], pScanMngr->BSSList.BSSList[ BSSEntryIndex ].BSSID[ 3 ], pScanMngr->BSSList.BSSList[ BSSEntryIndex ].BSSID[ 4 ], pScanMngr->BSSList.BSSList[ BSSEntryIndex ].BSSID[ 5 ], pScanMngr->BSSList.numOfEntries);
#endif
            scanMngrRemoveBSSListEntry( hScanMngr, BSSEntryIndex );
        }
        else
        {
            BSSEntryIndex++;
        }
    }
}

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Updates object data according to a received frame.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param frameInfo - pointer to frame related information.\n
 */
void scanMngrUpdateReceivedFrame( TI_HANDLE hScanMngr, TScanFrameInfo* frameInfo )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int BSSListIndex, neighborAPIndex;
    TScanBandPolicy* pBandPolicy;

    /* It looks like it never happens. Anyway decided to check */
    if ( frameInfo->band >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrUpdateReceivedFrame. frameInfo->band=%d exceeds the limit %d\n",
                frameInfo->band, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( pScanMngr->neighborAPsDiscoveryList[ frameInfo->band ].numOfEntries > MAX_NUM_OF_NEIGHBOR_APS )
    {
        TRACE3( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrUpdateReceivedFrame. pScanMngr->neighborAPsDiscoveryList[ %d ].numOfEntries=%d exceeds the limit %d\n",
                frameInfo->band, pScanMngr->neighborAPsDiscoveryList[ frameInfo->band ].numOfEntries, MAX_NUM_OF_NEIGHBOR_APS);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    
#ifdef SCAN_MNGR_DBG
    scanMngrDebugPrintReceivedFrame( hScanMngr, frameInfo );
#endif
#ifdef TI_DBG
    pScanMngr->stats.receivedFrames++;
#endif
    /* first check if the frame pass RSSI threshold. If not discard it and continue */
    pBandPolicy = scanMngrGetPolicyByBand( hScanMngr, frameInfo->band );
    if ( NULL == pBandPolicy ) /* sanity checking */
    {
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_ERROR, "Recieved framed on band %d, for which policy is not defined!\n", frameInfo->band);
#ifdef TI_DBG
        pScanMngr->stats.discardedFramesOther++;
#endif
        return;
    }

    if ( frameInfo->rssi < pBandPolicy->rxRSSIThreshold )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Discarding frame beacuse RSSI %d is lower than threshold %d\n", frameInfo->rssi, pBandPolicy->rxRSSIThreshold);
#ifdef TI_DBG
        pScanMngr->stats.discardedFramesLowRSSI++;
#endif
        return;
    }
    
    /* search for this AP in the tracking list */
    BSSListIndex = scanMngrGetTrackIndexByBssid( hScanMngr, frameInfo->bssId );

    /* if the frame received from an AP in the track list */
    if (( -1 != BSSListIndex ) && (BSSListIndex < MAX_SIZE_OF_BSS_TRACK_LIST ))
    {
        scanMngrUpdateBSSInfo( hScanMngr, BSSListIndex, frameInfo );
    }
    /* otherwise, if the list is not full and AP is either a neighbor AP or on a policy defined channel: */
    else 
    {
        neighborAPIndex = scanMngrGetNeighborAPIndex( hScanMngr, frameInfo->band, frameInfo->bssId );

        if ( (pScanMngr->BSSList.numOfEntries < pScanMngr->scanPolicy.BSSListSize) &&
             ((TI_TRUE == scanMngrIsPolicyChannel( hScanMngr, frameInfo->band, frameInfo->channel )) ||
              (-1 != neighborAPIndex))) 
        {
            /* insert the AP to the list */
            scanMngrInsertNewBSSToTrackingList( hScanMngr, frameInfo );

            /* if this is a neighbor AP */
            if ( -1 != neighborAPIndex )
            {
                /* mark in the neighbor AP list that it's being tracked */
                pScanMngr->neighborAPsDiscoveryList[ frameInfo->band ].trackStatusList[ neighborAPIndex ] = SCAN_NDS_DISCOVERED;

                /* if the discovery index for this neighbor AP band points to this AP, 
                   advance it and advance discovery part if needed */
                if ( pScanMngr->neighborAPsDiscoveryIndex[ frameInfo->band ] == neighborAPIndex )
                {
                    do {
                        pScanMngr->neighborAPsDiscoveryIndex[ frameInfo->band ]++; /* advance discovery index */
                    /* while discovery list is not exhausted and no AP for discovery is found */
                    } while ( (pScanMngr->neighborAPsDiscoveryIndex[ frameInfo->band ] < pScanMngr->neighborAPsDiscoveryList[ frameInfo->band ].numOfEntries) &&
                              (SCAN_NDS_NOT_DISCOVERED != pScanMngr->neighborAPsDiscoveryList[ frameInfo->band ].trackStatusList[ pScanMngr->neighborAPsDiscoveryIndex[ frameInfo->band ] ]));
                    /* if discovery list isexhausted */
                    if ( pScanMngr->neighborAPsDiscoveryIndex[ frameInfo->band ] == pScanMngr->neighborAPsDiscoveryList[ frameInfo->band ].numOfEntries )
                    {
                        /* restart discovery cycle for this band's neighbor APs */
                        pScanMngr->neighborAPsDiscoveryIndex[ frameInfo->band ] = 0;
                        /* set new discovery part (if needed) */
                        scanMngrSetNextDiscoveryPart( hScanMngr );
                    }
                }
            }
        }
#ifdef TI_DBG
        else
        {
            pScanMngr->stats.discardedFramesOther++;
        }
#endif
    }
}

/**
 * \\n
 * \date 17-Mar-2005\n
 * \brief Cerate a new tracking entry and store the newly discovered AP info in it.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param frameInfo - a pointer to the information received from this AP.\n
 */
void scanMngrInsertNewBSSToTrackingList( TI_HANDLE hScanMngr, TScanFrameInfo* frameInfo )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
#ifdef SCAN_SPS_USE_DRIFT_COMPENSATION
    int i;
#endif
    
    /* mark that a new AP was discovered (for discovery stage) */
    pScanMngr->bNewBSSFound = TI_TRUE;

    /* It looks like it never happens. Anyway decided to check */
    if ( pScanMngr->BSSList.numOfEntries > MAX_SIZE_OF_BSS_TRACK_LIST )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
              "scanMngrInsertNewBSSToTrackingList. pScanMngr->BSSList.numOfEntries =%d can not exceed the limit %d\n",
              pScanMngr->BSSList.numOfEntries, MAX_SIZE_OF_BSS_TRACK_LIST);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* insert fields that are not update regulary */
    pScanMngr->BSSList.BSSList[ pScanMngr->BSSList.numOfEntries ].bNeighborAP = 
        ( -1 == scanMngrGetNeighborAPIndex( hScanMngr, frameInfo->band, frameInfo->bssId ) ?
          TI_FALSE :
          TI_TRUE );
    MAC_COPY (pScanMngr->BSSList.BSSList[pScanMngr->BSSList.numOfEntries].BSSID, *(frameInfo->bssId));

    /* initialize average RSSI value */
    pScanMngr->BSSList.BSSList[ pScanMngr->BSSList.numOfEntries ].RSSI = frameInfo->rssi;
    pScanMngr->BSSList.BSSList[ pScanMngr->BSSList.numOfEntries ].lastRSSI = frameInfo->rssi;

#ifdef SCAN_SPS_USE_DRIFT_COMPENSATION
    /* initialize previous delta change array (used for SPS drift compensation) */
    pScanMngr->BSSList.scanBSSList[ pScanMngr->BSSList.numOfEntries ].prevTSFDelta = 0;
    pScanMngr->BSSList.scanBSSList[ pScanMngr->BSSList.numOfEntries ].deltaChangeArrayIndex = 0;
    for ( i = 0; i < SCAN_SPS_NUM_OF_TSF_DELTA_ENTRIES; i++ )
    {
        pScanMngr->BSSList.scanBSSList[ pScanMngr->BSSList.numOfEntries ].deltaChangeArray[ i ] = 0;
    }
#endif

    /* update regular fields */
    pScanMngr->BSSList.scanBSSList[ pScanMngr->BSSList.numOfEntries ].trackFailCount = 0; /* for correct statistics update */
    scanMngrUpdateBSSInfo( hScanMngr, pScanMngr->BSSList.numOfEntries, frameInfo );

    /* increase the number of tracked APs */
    pScanMngr->BSSList.numOfEntries++;
}

/**
 * \\n
 * \date 17-Mar-2005\n
 * \brief Updates tracked AP information.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param BSSListIndex - index to the BSS list where the AP information is stored.\n
 * \param frameInfo - a pointer to the information received from this AP.\n
 */
void scanMngrUpdateBSSInfo( TI_HANDLE hScanMngr, TI_UINT8 BSSListIndex, TScanFrameInfo* frameInfo )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    /* update AP data */
    pScanMngr->BSSList.BSSList[ BSSListIndex ].lastRxHostTimestamp = os_timeStampMs( pScanMngr->hOS );
    pScanMngr->BSSList.BSSList[ BSSListIndex ].resultType = (frameInfo->parsedIEs->subType == BEACON) ? SCAN_RFT_BEACON : SCAN_RFT_PROBE_RESPONSE;
    pScanMngr->BSSList.BSSList[ BSSListIndex ].band = frameInfo->band;
    pScanMngr->BSSList.BSSList[ BSSListIndex ].channel = frameInfo->channel;
    /* if the received TSF (which is the lower 32 bits) is smaller than the lower 32 bits of the last beacon
       TSF, it means the higher 32 bits should be increased by 1 (TSF overflow to higher 32 bits occurred between
       last beacon of current AP and this frame). */
    if ( INT64_LOWER( (pScanMngr->currentTSF))  > frameInfo->staTSF )
    {
        INT64_HIGHER( (pScanMngr->BSSList.scanBSSList[ BSSListIndex ].localTSF)) = 
            INT64_HIGHER( (pScanMngr->currentTSF)) + 1;
    }
    else
    {
        INT64_HIGHER( (pScanMngr->BSSList.scanBSSList[ BSSListIndex ].localTSF)) = 
            INT64_HIGHER( (pScanMngr->currentTSF));
    }
    INT64_LOWER( (pScanMngr->BSSList.scanBSSList[ BSSListIndex ].localTSF)) = frameInfo->staTSF;
    
    if ( BEACON == frameInfo->parsedIEs->subType )
    {
        os_memoryCopy( pScanMngr->hOS, &(pScanMngr->BSSList.BSSList[ BSSListIndex ].lastRxTSF),
                       (void *)frameInfo->parsedIEs->content.iePacket.timestamp, TIME_STAMP_LEN );
        pScanMngr->BSSList.BSSList[ BSSListIndex ].beaconInterval = 
            frameInfo->parsedIEs->content.iePacket.beaconInerval;
        pScanMngr->BSSList.BSSList[ BSSListIndex ].capabilities = 
            frameInfo->parsedIEs->content.iePacket.capabilities;
    }
    else
    {
        os_memoryCopy( pScanMngr->hOS, &(pScanMngr->BSSList.BSSList[ BSSListIndex ].lastRxTSF),
                       (void *)frameInfo->parsedIEs->content.iePacket.timestamp, TIME_STAMP_LEN );
        pScanMngr->BSSList.BSSList[ BSSListIndex ].beaconInterval = 
            frameInfo->parsedIEs->content.iePacket.beaconInerval;
        pScanMngr->BSSList.BSSList[ BSSListIndex ].capabilities = 
            frameInfo->parsedIEs->content.iePacket.capabilities;
    }
#ifdef TI_DBG
    /* 
       update track fail histogram:
       1. only done when tracking (to avoid updating due to "accidental re-discovery"
       2. only done for APs which have their track fail count larger than 0. The reason for that is because
          when tracking is started, the track fail count is increased, and thus if it is 0 tracking was not
          attempted for this AP, or more than one frame was received as a result of tracking operation for the AP.
    */
    if ( ((SCAN_CSS_TRACKING_A_BAND == pScanMngr->contScanState) ||
          (SCAN_CSS_TRACKING_G_BAND == pScanMngr->contScanState)) &&
         (0 < pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount))
    {
        if ( SCAN_MNGR_STAT_MAX_TRACK_FAILURE <= 
            pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount )
        {
            pScanMngr->stats.ConsecutiveTrackFailCountHistogram[ SCAN_MNGR_STAT_MAX_TRACK_FAILURE - 1 ]++;
        }
        else
        {
            pScanMngr->stats.ConsecutiveTrackFailCountHistogram[ pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount - 1 ]++;
        }
    }
#endif
    pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount = 0;

    /* update RSSI value */
    {
        TI_INT8    rssiPrevVal = pScanMngr->BSSList.BSSList[ BSSListIndex ].RSSI;
        TI_INT8    tmpRssiAvg = ((RSSI_PREVIOUS_COEFFICIENT * rssiPrevVal) +
                              ((10-RSSI_PREVIOUS_COEFFICIENT) * frameInfo->rssi)) / 10;

        pScanMngr->BSSList.BSSList[ BSSListIndex ].lastRSSI = frameInfo->rssi;
        
        if (rssiPrevVal!=0)
        {
             /* for faster convergence on RSSI changes use rounding error calculation with latest sample and not 
                on latest average */
            if (frameInfo->rssi > tmpRssiAvg)
                tmpRssiAvg++;
            else
                if (frameInfo->rssi < tmpRssiAvg)
                    tmpRssiAvg--;
        
            pScanMngr->BSSList.BSSList[ BSSListIndex ].RSSI = tmpRssiAvg;
        }
        else
        {
            pScanMngr->BSSList.BSSList[ BSSListIndex ].RSSI = frameInfo->rssi;
        }
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "given RSSI=%d, AVRG RSSI=%d\n", frameInfo->rssi, pScanMngr->BSSList.BSSList[ BSSListIndex ].RSSI);

    }

    pScanMngr->BSSList.BSSList[ BSSListIndex ].rxRate = frameInfo->rate;
    os_memoryCopy( pScanMngr->hOS, pScanMngr->BSSList.BSSList[ BSSListIndex ].pBuffer, 
                   frameInfo->buffer, frameInfo->bufferLength );    
    pScanMngr->BSSList.BSSList[ BSSListIndex ].bufferLength = frameInfo->bufferLength;
}

/**
 * \\n
 * \date 16-Mar-2005\n
 * \brief Search tracking list for an entry matching given BSSID.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bssId - the BSSID to search for.\n
 * \return entry index if found, -1 if no entry matching the BSSID was found.\n
 */
TI_INT8 scanMngrGetTrackIndexByBssid( TI_HANDLE hScanMngr, TMacAddr* bssId )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i;

    for ( i = 0; i < pScanMngr->BSSList.numOfEntries; i++ )
    {
        if (MAC_EQUAL(*bssId, pScanMngr->BSSList.BSSList[ i ].BSSID))
        {
            return i;
        }
    }
    return -1;
}

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Search current policy for band policy
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param band - the band to find policy for.\n
 * \return the policy structure if found, NULL if no policy configured for this band.\n
 */
TScanBandPolicy* scanMngrGetPolicyByBand( TI_HANDLE hScanMngr, ERadioBand band )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i;

    /* loop on all configured policies, and look for the requested band */
    for ( i = 0; i < pScanMngr->scanPolicy.numOfBands; i++ )
    {
        if ( band == pScanMngr->scanPolicy.bandScanPolicy[ i ].band )
        {
            return &(pScanMngr->scanPolicy.bandScanPolicy[ i ]);
        }
    }

    /* if no policy was found, there's no policy configured for the requested band */
    return NULL;
}

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Sets the next discovery part according to current discovery part, policies and neighbor APs availability .\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrSetNextDiscoveryPart( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    scan_discoveryPart_e nextDiscoveryPart, originalDiscoveryPart;
    
    /* sanity check - if discovery part is not valid, restart from first discovery part */
    if ( SCAN_SDP_NO_DISCOVERY <= pScanMngr->currentDiscoveryPart )
    {
        pScanMngr->currentDiscoveryPart = SCAN_SDP_NEIGHBOR_G;
    }

    /* if current discovery part is valid, do nothing */
    if ( TI_TRUE == scanMngrIsDiscoveryValid( hScanMngr, pScanMngr->currentDiscoveryPart ))
    {
        return;
    }

    /* next discovery part is found according to current part, in the following order:
       Neighbor APs on G, Neighbor APs on A, Channel list on G, Channel list on A */
    /* get next discovery part */
    nextDiscoveryPart = pScanMngr->currentDiscoveryPart;
    originalDiscoveryPart = pScanMngr->currentDiscoveryPart;

    do
    {
        nextDiscoveryPart++;
        /* loop back to first discovery part if discovery list end had been reached */
        if ( SCAN_SDP_NO_DISCOVERY == nextDiscoveryPart )
        {
            nextDiscoveryPart = SCAN_SDP_NEIGHBOR_G;
        }
    /* try next discovery part until first one is reached again or a valid part is found */
    } while( (nextDiscoveryPart != originalDiscoveryPart) &&
             (TI_FALSE == scanMngrIsDiscoveryValid( hScanMngr, nextDiscoveryPart )));

    /* if a discovery part for which discovery is valid was found, use it */
    if ( TI_TRUE == scanMngrIsDiscoveryValid( hScanMngr, nextDiscoveryPart ))
    {
        pScanMngr->currentDiscoveryPart = nextDiscoveryPart;
    }
    /* otherwise don't do discovery */
    else
    {
        pScanMngr->currentDiscoveryPart = SCAN_SDP_NO_DISCOVERY;
    }
}

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Checks whether discovery should be performed on the specified discovery part.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param discoveryPart - the discovery part to check.\n
 */
TI_BOOL scanMngrIsDiscoveryValid( TI_HANDLE hScanMngr, scan_discoveryPart_e discoveryPart )
{
    scanMngr_t* pScanMngr = (TI_HANDLE)hScanMngr;
    TScanBandPolicy *gPolicy, *aPolicy;

    gPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_2_4_GHZ );
    aPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_5_0_GHZ );

    switch (discoveryPart)
    {
    case SCAN_SDP_NEIGHBOR_G:
        /* for discovery on G neighbor APs, a policy must be defined for G, discovery scan type should be present,
           number of neighbor APs on G should be greater than zero, and at least one AP should be yet undiscovered */
        if ( (NULL != gPolicy) &&
             (SCAN_TYPE_NO_SCAN != gPolicy->discoveryMethod.scanType) &&
             (0 < pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_2_4_GHZ ].numOfEntries) &&
             (TI_TRUE == scanMngrNeighborAPsAvailableForDiscovery( hScanMngr, RADIO_BAND_2_4_GHZ )))
        {
            return TI_TRUE;
        }
        else
        {
            return TI_FALSE;
        }

    case SCAN_SDP_NEIGHBOR_A:
        /* for discovery on A neighbor APs, a policy must be defined for A, discovery scan type should be present,
           number of neighbor APs on A should be greater than zero, and at least one AP should be yet undiscovered */
        if ( (NULL != aPolicy) &&
             (SCAN_TYPE_NO_SCAN != aPolicy->discoveryMethod.scanType) &&
             (0 < pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_5_0_GHZ ].numOfEntries) &&
             (TI_TRUE == scanMngrNeighborAPsAvailableForDiscovery( hScanMngr, RADIO_BAND_5_0_GHZ )))
        {
            return TI_TRUE;
        }
        else
        {
            return TI_FALSE;
        }

    case SCAN_SDP_CHANNEL_LIST_G:
        /* for discovery on G channel list, a policy must be defined for G, discovery scan type should be present,
           and number of channels in G channel list should be greater than zero */
        if ( (NULL != gPolicy) &&
             (SCAN_TYPE_NO_SCAN != gPolicy->discoveryMethod.scanType) &&
             (0 < gPolicy->numOfChannles))
        {
            return TI_TRUE;
        }
        else
        {
            return TI_FALSE;
        }
    case SCAN_SDP_CHANNEL_LIST_A:
        /* for discovery on A channel list, a policy must be defined for A, discovery scan type should be present,
           and number of channels in A channel list should be greater than zero */
        if ( (NULL != aPolicy) &&
             (SCAN_TYPE_NO_SCAN != aPolicy->discoveryMethod.scanType) &&
             (0 < aPolicy->numOfChannles))
        {
            return TI_TRUE;
        }
        else
        {
            return TI_FALSE;
        }
    default:
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Checking whather discovery is valid for discovery part %d", discoveryPart);
        return TI_FALSE;
    }
}

/**
 * \\n
 * \date 07-Mar-2005\n
 * \brief Check whether there are neighbor APs to track on the given band.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - The scan policy for the requested band.\n
 * \param bNeighborAPsOnly - whether to scan for neighbor APs only or for all policy defined channels.\n
 */
TI_BOOL scanMngrNeighborAPsAvailableForDiscovery( TI_HANDLE hScanMngr, ERadioBand band )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i;


    /* loop on all neighbor APs of the given band */
    for ( i = 0; i < pScanMngr->neighborAPsDiscoveryList[ band ].numOfEntries; i++ )
    {
        /* if a neighbor AP is not being tracked, meaning it yet has to be discovered, return TI_TRUE */
        if ( SCAN_NDS_NOT_DISCOVERED == pScanMngr->neighborAPsDiscoveryList[ band ].trackStatusList[ i ] )
        {
            return TI_TRUE;
        }
    }
    /* if all neighbor APs are being tracked (or no neighbor APs available) return TI_FALSE */
    return TI_FALSE;
}

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Builds a scan command on the object workspace for immediate scan.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - The scan policy for the requested band.\n
 * \param bNeighborAPsOnly - whether to scan for neighbor APs only or for all policy defined channels.\n
 */
void scanMngrBuildImmediateScanCommand( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy, TI_BOOL bNeighborAPsOnly )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int channelIndex;
    paramInfo_t param;
    TMacAddr broadcastAddress;
    int i;

    /* It looks like it never happens. Anyway decided to check */
    if ( bandPolicy->band >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngrBuildImmediateScanCommand. bandPolicy->band=%d exceeds the limit %d\n",
               bandPolicy->band, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].numOfEntries > MAX_NUM_OF_NEIGHBOR_APS )
    {
        TRACE3( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngrBuildImmediateScanCommand. pScanMngr->neighborAPsDiscoveryList[%d].numOfEntries=%d exceeds the limit %d\n",
               bandPolicy->band, pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].numOfEntries, MAX_NUM_OF_NEIGHBOR_APS);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* first, build the command header */
    scanMngrBuildScanCommandHeader( hScanMngr, &(bandPolicy->immediateScanMethod), bandPolicy->band );

    /* if requested to scan on neighbor APs only */
    if ( TI_TRUE == bNeighborAPsOnly )
    {
        /* loop on all neighbor APs */
        channelIndex = 0;
        while ( (channelIndex < pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].numOfEntries) && 
                (pScanMngr->scanParams.numOfChannels < SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND))
        {
            /* verify channel with reg domain */
            param.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
            param.content.channelCapabilityReq.band = bandPolicy->band;
            if ( (bandPolicy->immediateScanMethod.scanType == SCAN_TYPE_NORMAL_PASSIVE) ||
                 (bandPolicy->immediateScanMethod.scanType == SCAN_TYPE_TRIGGERED_PASSIVE) ||
                 (bandPolicy->immediateScanMethod.scanType == SCAN_TYPE_SPS))
            {
                param.content.channelCapabilityReq.scanOption = PASSIVE_SCANNING;
            }
            else
            {
                param.content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
            }
            param.content.channelCapabilityReq.channelNum = 
                pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].APListPtr[ channelIndex ].channel;
            regulatoryDomain_getParam( pScanMngr->hRegulatoryDomain, &param );

            /* if the channel is allowed, insert it to the scan command */
            if (param.content.channelCapabilityRet.channelValidity)
            {
                scanMngrAddNormalChannel( hScanMngr, &(bandPolicy->immediateScanMethod), 
                                          pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].APListPtr[ channelIndex ].channel,
                                          &(pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].APListPtr[ channelIndex ].BSSID),
                                          param.content.channelCapabilityRet.maxTxPowerDbm );
            }
            channelIndex++;
        }
    }
    else
    /* scan on all policy defined channels */
    {
        /* set the broadcast address */
        for ( i = 0; i < MAC_ADDR_LEN; i++ )
        {
            broadcastAddress[ i ] = 0xff;
        }

        /* loop on all channels in the policy */
        channelIndex = 0;
        while ( (channelIndex < bandPolicy->numOfChannles) &&
                (pScanMngr->scanParams.numOfChannels < SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND))
        {
            /* verify channel with reg domain */
            param.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
            param.content.channelCapabilityReq.band = bandPolicy->band;
            if ( (bandPolicy->immediateScanMethod.scanType == SCAN_TYPE_NORMAL_PASSIVE) ||
                 (bandPolicy->immediateScanMethod.scanType == SCAN_TYPE_TRIGGERED_PASSIVE) ||
                 (bandPolicy->immediateScanMethod.scanType == SCAN_TYPE_SPS))
            {
                param.content.channelCapabilityReq.scanOption = PASSIVE_SCANNING;
            }
            else
            {
                param.content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
            }
            param.content.channelCapabilityReq.channelNum = bandPolicy->channelList[ channelIndex ];
            regulatoryDomain_getParam( pScanMngr->hRegulatoryDomain, &param );

            /* if the channel is allowed, insert it to the scan command */
            if (param.content.channelCapabilityRet.channelValidity)
            {
                scanMngrAddNormalChannel( hScanMngr, &(bandPolicy->immediateScanMethod), 
                                          bandPolicy->channelList[ channelIndex ],
                                          &broadcastAddress,
                                          param.content.channelCapabilityRet.maxTxPowerDbm );
            }
            channelIndex++;
        }
    }
}

/**
 * \\n
 * \date 03-Mar-2005\n
 * \brief Builds a scan command on the object workspace for tracking.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - The scan policy for the band to track on.\n
 * \param band - the band to scan.\n
 */
void scanMngrBuildTrackScanCommand( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy, ERadioBand band )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int BSSListIndex;
    paramInfo_t param;
    TScanMethod* scanMethod;

    TRACE0(pScanMngr->hReport , REPORT_SEVERITY_INFORMATION, "\n scanMngrBuildTrackScanCommand \n");


    /* SPS is performed differently from all other scan types, and only if TSF error has not occured */
    if ( (SCAN_TYPE_SPS == bandPolicy->trackingMethod.scanType) && (TI_TRUE == pScanMngr->bSynchronized))
    {
        /* build the command header */
        TRACE0(pScanMngr->hReport , REPORT_SEVERITY_INFORMATION, "\nSPS invoked\n");
        scanMngrBuildScanCommandHeader( hScanMngr, &(bandPolicy->trackingMethod), band );
     
        /* build the channel list */
        scanMngrAddSPSChannels( hScanMngr, &(bandPolicy->trackingMethod), band );
        return;
    }

    /* the scan method to use is the method defined for tracking, unless this is SPS and TSF error occurred,
       in which case we use the discovery method this time. */
    if ( (SCAN_TYPE_SPS == bandPolicy->trackingMethod.scanType) && (TI_FALSE == pScanMngr->bSynchronized))
    {
        /* use discovery scan method */
        scanMethod = &(bandPolicy->discoveryMethod);
    }
    else
    {
        /* use tracking method */
        scanMethod = &(bandPolicy->trackingMethod);
    }

    /* build the command header */
    scanMngrBuildScanCommandHeader( hScanMngr, scanMethod, band );

    /* It looks like it never happens. Anyway decided to check */
    if ( pScanMngr->BSSList.numOfEntries > MAX_SIZE_OF_BSS_TRACK_LIST )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrBuildTrackScanCommand. pScanMngr->BSSList.numOfEntries=%d exceeds the limit %d\n",
                pScanMngr->BSSList.numOfEntries, MAX_SIZE_OF_BSS_TRACK_LIST);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( bandPolicy->numOfChannles > MAX_BAND_POLICY_CHANNLES )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrBuildTrackScanCommand. bandPolicy->numOfChannles=%d exceeds the limit %d\n",
                bandPolicy->numOfChannles, MAX_BAND_POLICY_CHANNLES);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* insert channels from tracking list according to requested band */
    BSSListIndex = 0;
    while ( (BSSListIndex < pScanMngr->BSSList.numOfEntries) &&
            (pScanMngr->scanParams.numOfChannels < SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND))
    {
        /* if BSS is on the right band */
        if ( band == pScanMngr->BSSList.BSSList[ BSSListIndex ].band )
        {
            /* verify the channel with the reg domain */
            param.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
            param.content.channelCapabilityReq.band = band;
            if ( (scanMethod->scanType == SCAN_TYPE_NORMAL_PASSIVE) ||
                 (scanMethod->scanType == SCAN_TYPE_TRIGGERED_PASSIVE))
            {
                param.content.channelCapabilityReq.scanOption = PASSIVE_SCANNING;
            }
            else
            {
                param.content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
            }
            param.content.channelCapabilityReq.channelNum = pScanMngr->BSSList.BSSList[ BSSListIndex ].channel;
            regulatoryDomain_getParam( pScanMngr->hRegulatoryDomain, &param );

            /* if channel is verified for requested scan type */
            if ( param.content.channelCapabilityRet.channelValidity )
            {
                scanMngrAddNormalChannel( hScanMngr, scanMethod, 
                                          pScanMngr->BSSList.BSSList[ BSSListIndex ].channel,
                                          &(pScanMngr->BSSList.BSSList[ BSSListIndex ].BSSID),
                                          param.content.channelCapabilityRet.maxTxPowerDbm );

                /* increase AP track attempts counter */
                if ( (SCAN_TYPE_SPS == bandPolicy->trackingMethod.scanType) && (TI_FALSE == pScanMngr->bSynchronized))
                {
                    pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount = 
                        pScanMngr->scanPolicy.maxTrackFailures + 1;
                }
                else
                {
                    pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount++;
                }
            }
            /* if channel is not verified, there are two options:
               1. we are using the tracking method, and thus the AP should be removed (because we are unable
                  to track it)
               2. we are using the discovery method (because a TSF error occurred and tracking method is SPS).
                  In this case, it seems we do not have to remove the AP (because the channel may not be valid
                  for active scan but it is valid for passive scan), but since we had a TSF error the AP would
                  be removed anyhow if not re-discovered now, so no harm done in removing it as well. */
            else
            {
                /* removing an AP is done by increasing its track failure counter to maximum. Since it is
                   not tracked, it would not be found, and thus would be removed by aging process performed
                   at scan completion */
                pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount = 
                    pScanMngr->scanPolicy.maxTrackFailures + 1;
#ifdef TI_DBG
                /* update statistics */
                pScanMngr->stats.APsRemovedInvalidChannel++;
#endif
            }
        }
        BSSListIndex++;
    }
}

/**
 * \\n
 * \date 03-Mar-2005\n
 * \brief Builds a scan command on the object workspace for discovery.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrBuildDiscoveryScanCommand( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    ERadioBand band;
    TScanBandPolicy* bandPolicy;

    /* find on which band to discover at current cycle */
    if ( (SCAN_SDP_NEIGHBOR_G == pScanMngr->currentDiscoveryPart) ||
         (SCAN_SDP_CHANNEL_LIST_G == pScanMngr->currentDiscoveryPart))
    {
        band = RADIO_BAND_2_4_GHZ;
        bandPolicy = scanMngrGetPolicyByBand( hScanMngr, band );
    }
    else
    {
        band = RADIO_BAND_5_0_GHZ;
        bandPolicy = scanMngrGetPolicyByBand( hScanMngr, band );
    }

	if( NULL == bandPolicy)
	{
		TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "scanMngrGetPolicyByBand() returned NULL.\n");
		return;
	}

    /* first, build the command header */
    scanMngrBuildScanCommandHeader( hScanMngr, &(bandPolicy->discoveryMethod), band );

    /* channels are added according to current discovery part */
    switch ( pScanMngr->currentDiscoveryPart )
    {
    case SCAN_SDP_NEIGHBOR_G:
        /* add channels from neighbor AP discovery list */
        scanMngrAddNeighborAPsForDiscovery( hScanMngr, bandPolicy );

        /* if neighbor AP list is exhausted, proceed to next discovery part */
        if ( 0 == pScanMngr->neighborAPsDiscoveryIndex[ band ] )
        {
            pScanMngr->currentDiscoveryPart++;
            scanMngrSetNextDiscoveryPart( hScanMngr );
        }

        /* if need to discover more APs, (not enough neighbor APs), proceed to G channel list */
        if ( pScanMngr->scanParams.numOfChannels < bandPolicy->numOfChannlesForDiscovery )
        {
            scanMngrAddChannelListForDiscovery( hScanMngr, bandPolicy );
        }

#ifdef TI_DBG
        pScanMngr->statsLastDiscoveryBand = RADIO_BAND_2_4_GHZ;
#endif
        break;

    case SCAN_SDP_NEIGHBOR_A:
        /* add channels from neighbor AP discovery list */
        scanMngrAddNeighborAPsForDiscovery( hScanMngr, bandPolicy );

        /* if neighbor AP list is exhausted, proceed to next discovery part */
        if ( 0 == pScanMngr->neighborAPsDiscoveryIndex[ band ] )
        {
            pScanMngr->currentDiscoveryPart++;
            scanMngrSetNextDiscoveryPart( hScanMngr );
        }

        /* if need to discover more APs, (not enough neighbor APs), proceed to A channel list */
        if ( pScanMngr->scanParams.numOfChannels < bandPolicy->numOfChannlesForDiscovery )
        {
            scanMngrAddChannelListForDiscovery( hScanMngr, bandPolicy );
        }

#ifdef TI_DBG
        pScanMngr->statsLastDiscoveryBand = RADIO_BAND_5_0_GHZ;
#endif
        break;

    case SCAN_SDP_CHANNEL_LIST_G:
        /* add channels from policy channel list */
        scanMngrAddChannelListForDiscovery( hScanMngr, bandPolicy );

        /* if channel list is exhausted, proceed to next discovery part */
        if ( 0 == pScanMngr->channelDiscoveryIndex[ band ] )
        {
            pScanMngr->currentDiscoveryPart++;
            scanMngrSetNextDiscoveryPart( hScanMngr );
        }

        /* if need to discover more APs (not enough channels on channel list), proceed to G neighbor APs */
        if ( pScanMngr->scanParams.numOfChannels < bandPolicy->numOfChannlesForDiscovery )
        {
            scanMngrAddNeighborAPsForDiscovery( hScanMngr, bandPolicy );
        }

#ifdef TI_DBG
        pScanMngr->statsLastDiscoveryBand = RADIO_BAND_2_4_GHZ;
#endif
        break;

    case SCAN_SDP_CHANNEL_LIST_A:
        /* add channels from policy channel list */
        scanMngrAddChannelListForDiscovery( hScanMngr, bandPolicy );

        /* if channel list is exhausted, proceed to next discovery part */
        if ( 0 == pScanMngr->channelDiscoveryIndex[ band ] )
        {
            pScanMngr->currentDiscoveryPart++;
            scanMngrSetNextDiscoveryPart( hScanMngr );
        }

        /* if need to discover more APs (not enough channels on channel list), proceed to A neighbor APs */
        if ( pScanMngr->scanParams.numOfChannels < bandPolicy->numOfChannlesForDiscovery )
        {
            scanMngrAddNeighborAPsForDiscovery( hScanMngr, bandPolicy );
        }
#ifdef TI_DBG
        pScanMngr->statsLastDiscoveryBand = RADIO_BAND_5_0_GHZ;
#endif
        break;

    case SCAN_SDP_NO_DISCOVERY:
    default:
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_ERROR, "scanMngrBuildDiscoveryScanCommand called and current discovery part is %d", pScanMngr->currentDiscoveryPart);
        break;
    }
}

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Builds the scan command header on the object workspace.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param scanMethod - The scan method (and parameters) to use.\n
 * \param band - the band to scan.\n
 */
void scanMngrBuildScanCommandHeader( TI_HANDLE hScanMngr, TScanMethod* scanMethod, ERadioBand band )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;


    /* set general scan parameters */
    /* SSID is not set - scan concentrator will set it for the scan manager to current SSID */
    pScanMngr->scanParams.scanType = scanMethod->scanType;
    pScanMngr->scanParams.band = band;

    switch (scanMethod->scanType)
    {
    case SCAN_TYPE_NORMAL_ACTIVE:
        /* In active scan, the desired SSID is set by the scan concentrator to the current SSID.
           Stting anything not zero triggers this in the scan concentrator */
        pScanMngr->scanParams.desiredSsid.len = 1;
        pScanMngr->scanParams.probeReqNumber = scanMethod->method.basicMethodParams.probReqParams.numOfProbeReqs;
        pScanMngr->scanParams.probeRequestRate = scanMethod->method.basicMethodParams.probReqParams.bitrate;
        break;

    case SCAN_TYPE_TRIGGERED_ACTIVE:
        /* In active scan, the desired SSID is set by the scan concentrator to the current SSID.
           Stting anything not zero triggers this in the scan concentrator */
        pScanMngr->scanParams.desiredSsid.len = 1;
        pScanMngr->scanParams.probeReqNumber = scanMethod->method.TidTriggerdMethodParams.basicMethodParams.probReqParams.numOfProbeReqs;
        pScanMngr->scanParams.probeRequestRate = scanMethod->method.TidTriggerdMethodParams.basicMethodParams.probReqParams.bitrate;
        pScanMngr->scanParams.Tid = scanMethod->method.TidTriggerdMethodParams.triggeringTid;
        break;

    case SCAN_TYPE_TRIGGERED_PASSIVE:
        pScanMngr->scanParams.Tid = scanMethod->method.TidTriggerdMethodParams.triggeringTid;
        /* In Passive scan, Desired SSID length is set to 0 so that the Scan concentrator won't replace
           it with the current SSID (to be able to receive beacons from AP's with multiple or hidden
           SSID) */
        pScanMngr->scanParams.desiredSsid.len = 0;
        break;

    case SCAN_TYPE_NORMAL_PASSIVE:
        /* In Passive scan, Desired SSID length is set to 0 so that the Scan concentrator won't replace
           it with the current SSID (to be able to receive beacons from AP's with multiple or hidden
           SSID) */
        pScanMngr->scanParams.desiredSsid.len = 0;
        break;

    case SCAN_TYPE_SPS:
        /* SPS doesn't have SSID filter, it only uses BSSID filter */
        break;

    default:
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Unrecognized scan type %d when building scan command", scanMethod->scanType);
        break;
    }

    /* set 0 channels - actual channel will be added by caller */
    pScanMngr->scanParams.numOfChannels = 0;
}

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Add neighbor APs to scan command on the object workspace for discovery scan.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - the scan policy for the band to use.\n
 */
void scanMngrAddNeighborAPsForDiscovery( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int neighborAPIndex;
    paramInfo_t param;

    /* It looks like it never happens. Anyway decided to check */
    if ( bandPolicy->band >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrAddNeighborAPsForDiscovery. bandPolicy->band=%d exceeds the limit %d\n",
                bandPolicy->band, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    neighborAPIndex = pScanMngr->neighborAPsDiscoveryIndex[ bandPolicy->band ];
    /* loop while neighbor AP list has not been exhausted, command is not full and not enough APs for discovery had been found */
    while ( (pScanMngr->scanParams.numOfChannels < bandPolicy->numOfChannlesForDiscovery) &&
            (pScanMngr->scanParams.numOfChannels < SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND) &&
            (neighborAPIndex < pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].numOfEntries))
    {
        /* if the AP is not being tracked */
        if ( SCAN_NDS_NOT_DISCOVERED == 
             pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].trackStatusList[ neighborAPIndex ] )
        {
            /* verify channel with reg domain */
            param.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
            param.content.channelCapabilityReq.band = bandPolicy->band;
            if ( (bandPolicy->discoveryMethod.scanType == SCAN_TYPE_NORMAL_PASSIVE) ||
                 (bandPolicy->discoveryMethod.scanType == SCAN_TYPE_TRIGGERED_PASSIVE) ||
                 (bandPolicy->discoveryMethod.scanType == SCAN_TYPE_SPS))
            {
                param.content.channelCapabilityReq.scanOption = PASSIVE_SCANNING;
            }
            else
            {
                param.content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
            }
            param.content.channelCapabilityReq.channelNum = 
                pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].APListPtr[ neighborAPIndex ].channel;
            regulatoryDomain_getParam( pScanMngr->hRegulatoryDomain, &param );

            /* if the channel is allowed, insert it to the scan command */
            if (param.content.channelCapabilityRet.channelValidity)
            {
                scanMngrAddNormalChannel( hScanMngr, &(bandPolicy->discoveryMethod), 
                                          pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].APListPtr[ neighborAPIndex ].channel,
                                          &(pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].APListPtr[ neighborAPIndex ].BSSID),
                                          param.content.channelCapabilityRet.maxTxPowerDbm );
            }
        }
        neighborAPIndex++;
    }

    /* if neighbor AP list has been exhuasted */
    if ( neighborAPIndex == pScanMngr->neighborAPsDiscoveryList[ bandPolicy->band ].numOfEntries )
    {
        /* reset discovery index */
        pScanMngr->neighborAPsDiscoveryIndex[ bandPolicy->band ] = 0;
    }
    else
    {
        /* just update neighbor APs discovery index */
        pScanMngr->neighborAPsDiscoveryIndex[ bandPolicy->band ] = neighborAPIndex;
    }
}

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Add channel from policy channels list to scan command on the object workspace for discovery scan.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - the scan policy for the band to use.\n
 */
void scanMngrAddChannelListForDiscovery( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    paramInfo_t param;
    TMacAddr broadcastAddress;
    int i, channelListIndex;

    /* It looks like it never happens. Anyway decided to check */
    if ( bandPolicy->band >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrAddChannelListForDiscovery. bandPolicy->band=%d exceeds the limit %d\n",
                bandPolicy->band, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( bandPolicy->numOfChannles > MAX_BAND_POLICY_CHANNLES )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrAddChannelListForDiscovery. bandPolicy->numOfChannles=%d exceeds the limit %d\n",
                bandPolicy->numOfChannles, MAX_BAND_POLICY_CHANNLES);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    channelListIndex = pScanMngr->channelDiscoveryIndex[ bandPolicy->band ];

    /* set broadcast MAC address */
    for ( i = 0; i < MAC_ADDR_LEN; i++ )
    {
        broadcastAddress[ i ] = 0xff;
    }

    /* loop while channel list has not been exhausted, command is not full, and not enough APs for discovery had been found */
    while ( (pScanMngr->scanParams.numOfChannels < bandPolicy->numOfChannlesForDiscovery) &&
            (pScanMngr->scanParams.numOfChannels < SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND) &&
            (channelListIndex < bandPolicy->numOfChannles))
    {
        /* verify channel with reg domain */
        param.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
        param.content.channelCapabilityReq.band = bandPolicy->band;
        if ( (bandPolicy->discoveryMethod.scanType == SCAN_TYPE_NORMAL_PASSIVE) ||
             (bandPolicy->discoveryMethod.scanType == SCAN_TYPE_TRIGGERED_PASSIVE) ||
             (bandPolicy->discoveryMethod.scanType == SCAN_TYPE_SPS))
        {
            param.content.channelCapabilityReq.scanOption = PASSIVE_SCANNING;
        }
        else
        {
            param.content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
        }
        param.content.channelCapabilityReq.channelNum = 
            bandPolicy->channelList[ channelListIndex ];
        regulatoryDomain_getParam( pScanMngr->hRegulatoryDomain, &param );

        /* if the channel is allowed, insert it to the scan command */
        if (param.content.channelCapabilityRet.channelValidity)
        {
            scanMngrAddNormalChannel( hScanMngr, &(bandPolicy->discoveryMethod), 
                                      bandPolicy->channelList[ channelListIndex ],
                                      &broadcastAddress,
                                      param.content.channelCapabilityRet.maxTxPowerDbm );
        }
        channelListIndex++;
    }
        
    /* if channel discovery list has been exhuasted */
    if ( channelListIndex == bandPolicy->numOfChannles )
    {
        /* reset discovery index */
        pScanMngr->channelDiscoveryIndex[ bandPolicy->band ] = 0;
    }
    else
    {
        /* just update channel list discovery index */
        pScanMngr->channelDiscoveryIndex[ bandPolicy->band ] = channelListIndex;
    }
}

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Add SPS channels to scan command on the object workspace.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param scanMethod - The scan method (and parameters) to use.\n
 * \param band - the band to scan.\n
 */
void scanMngrAddSPSChannels( TI_HANDLE hScanMngr, TScanMethod* scanMethod, ERadioBand band )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TI_UINT64 EarliestTSFToInsert;
    TI_UINT32 timeToStartInAdvance = scanMethod->method.spsMethodParams.scanDuration /
        SCAN_SPS_DURATION_PART_IN_ADVANCE;
    scan_SPSHelper_t nextEventArray[ MAX_SIZE_OF_BSS_TRACK_LIST ];
    int BSSListIndex, i, j, nextEventArrayHead, nextEventArraySize;
    paramInfo_t param;
#ifdef SCAN_MNGR_SPS_DBG
    TI_UINT32 highValue, lowValue, maxNextEventArraySize;
#endif

    TRACE1(pScanMngr->hReport , REPORT_SEVERITY_INFORMATION, "\nscanMngrAddSPSChannels invoked for band %d\n",band);
    /* initialize latest TSF value */
    pScanMngr->scanParams.latestTSFValue = 0;

    /* initialize the next event arry */
    nextEventArrayHead = -1;
    nextEventArraySize = 0;

#ifdef SCAN_MNGR_SPS_DBG
    highValue = INT64_HIGHER( pScanMngr->currentTSF );
    lowValue = INT64_LOWER( pScanMngr->currentTSF );
    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "current TSF: %u-%u\n", highValue, lowValue);
#endif
    /* It looks like it never happens. Anyway decided to check */
    if ( pScanMngr->BSSList.numOfEntries > MAX_SIZE_OF_BSS_TRACK_LIST )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrAddSPSChannels. pScanMngr->BSSList.numOfEntries=%d exceeds the limit %d\n",
                pScanMngr->BSSList.numOfEntries, MAX_SIZE_OF_BSS_TRACK_LIST);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* insert channels from tracking list to next event array according to requested band */
    for ( BSSListIndex = 0; BSSListIndex < pScanMngr->BSSList.numOfEntries; BSSListIndex++ )
    {
        /* if BSS is on the right band */
        if ( band == pScanMngr->BSSList.BSSList[ BSSListIndex ].band )
        {
            /* verify the channel with the reg domain */
            param.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
            param.content.channelCapabilityReq.band = band;
            param.content.channelCapabilityReq.scanOption = PASSIVE_SCANNING;
            param.content.channelCapabilityReq.channelNum = pScanMngr->BSSList.BSSList[ BSSListIndex ].channel;
            regulatoryDomain_getParam( pScanMngr->hRegulatoryDomain, &param );

            /* if channel is verified for requested scan type */
            if ( param.content.channelCapabilityRet.channelValidity )
            {
                /* if this AP local TSF value is greater that latest TSF value, change it */
                if ( pScanMngr->BSSList.scanBSSList[ BSSListIndex ].localTSF > pScanMngr->scanParams.latestTSFValue )
                {
                    /* the latest TSF value is used by the FW to detect TSF error (an AP recovery). When a TSF
                       error occurs, the latest TSF value should be in the future (because the AP TSF was 
                       reset). */
                    pScanMngr->scanParams.latestTSFValue = pScanMngr->BSSList.scanBSSList[ BSSListIndex ].localTSF;
                }

                /* calculate the TSF of the next event for tracked AP. Scan should start 
                   SCAN_SPS_DURATION_PART_IN_ADVANCE before the calculated event */
                nextEventArray[ nextEventArraySize ].nextEventTSF = 
                    scanMngrCalculateNextEventTSF( hScanMngr, &(pScanMngr->BSSList), BSSListIndex, 
                                                   pScanMngr->currentTSF + SCAN_SPS_GUARD_FROM_CURRENT_TSF +
                                                   timeToStartInAdvance ) - timeToStartInAdvance;
#ifdef SCAN_MNGR_SPS_DBG
                highValue = INT64_HIGHER( nextEventArray[ nextEventArraySize ].nextEventTSF );
                lowValue = INT64_LOWER( nextEventArray[ nextEventArraySize ].nextEventTSF );
                TRACE8( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "BSSID:%02x:%02x:%02x:%02x:%02x:%02x will send frame at TSF:%x-%x\n", pScanMngr->BSSList.BSSList[ BSSListIndex ].BSSID[ 0 ], pScanMngr->BSSList.BSSList[ BSSListIndex ].BSSID[ 1 ], pScanMngr->BSSList.BSSList[ BSSListIndex ].BSSID[ 2 ], pScanMngr->BSSList.BSSList[ BSSListIndex ].BSSID[ 3 ], pScanMngr->BSSList.BSSList[ BSSListIndex ].BSSID[ 4 ], pScanMngr->BSSList.BSSList[ BSSListIndex ].BSSID[ 5 ], highValue, lowValue);
#endif
                nextEventArray[ nextEventArraySize ].trackListIndex = BSSListIndex;

                /* insert it, sorted, to the next event array */
                /* if need to insert as head (either because list is empty or because it has earliest TSF) */
                if ( (-1 == nextEventArrayHead) ||
                     (nextEventArray[ nextEventArraySize ].nextEventTSF < nextEventArray[ nextEventArrayHead ].nextEventTSF))
                {
                    /* link the newly inserted AP to the current head */
                    nextEventArray[ nextEventArraySize ].nextAPIndex = nextEventArrayHead;
                    /* make current head point to newly inserted AP */
                    nextEventArrayHead = nextEventArraySize;
                    nextEventArraySize++;
                }
                /* insert into the list */
                else
                {
                    /* start with list head */
                    i = nextEventArrayHead;
                    /* while the new AP TSF is larger and list end had not been reached */
                    while ( (nextEventArray[ i ].nextAPIndex != -1) && /* list end had not been reached */
                            (nextEventArray[ nextEventArray[ i ].nextAPIndex ].nextEventTSF < nextEventArray[ nextEventArraySize ].nextEventTSF)) /* next event TSF of the next AP in the list is smaller than that of the AP being inserted */
                    {
                        /* proceed to the next AP */
                        i = nextEventArray[ i ].nextAPIndex;
                    }
                    /* insert this AP to the list, right after the next event entry found */
                    nextEventArray[ nextEventArraySize ].nextAPIndex = nextEventArray[ i ].nextAPIndex;
                    nextEventArray[ i ].nextAPIndex = nextEventArraySize;
                    nextEventArraySize++;
                }
            }
            /* if for some reason a channel on which an AP was found is not valid for passive scan,
               the AP should be removed. */
            else
            {
                /* removing the AP is done by increasing its track count to maximum - and since it is
                   not tracked it will not be discovered, and thus will be deleted when the scan is complete */
                pScanMngr->BSSList.scanBSSList[ BSSListIndex ].trackFailCount = 
                    pScanMngr->scanPolicy.maxTrackFailures + 1;
#ifdef TI_DBG
                /*update statistics */
                pScanMngr->stats.APsRemovedInvalidChannel++;
#endif
            }
        }
    }

#ifdef SCAN_MNGR_SPS_DBG
    TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "SPS list after first stage:\n");
    scanMngrDebugPrintSPSHelperList( hScanMngr, nextEventArray, nextEventArrayHead, nextEventArraySize );
    maxNextEventArraySize = nextEventArraySize;
#endif

    /* insert channels from next event array to scan command */
    EarliestTSFToInsert = pScanMngr->currentTSF + SCAN_SPS_GUARD_FROM_CURRENT_TSF;
    /* insert all APs to scan command (as long as command is not full) */
    while ( (nextEventArraySize > 0) &&
            (pScanMngr->scanParams.numOfChannels < SCAN_MAX_NUM_OF_SPS_CHANNELS_PER_COMMAND))
    {
        /* if first list entry fits, and it doesn't collide with current AP DTIM */
        if ( EarliestTSFToInsert < nextEventArray[ nextEventArrayHead ].nextEventTSF )
        {
            if ( TI_FALSE == scanMngrDTIMInRange( hScanMngr, nextEventArray[ nextEventArrayHead ].nextEventTSF,
                                               nextEventArray[ nextEventArrayHead ].nextEventTSF + scanMethod->method.spsMethodParams.scanDuration ))
            {
                /* insert it to scan command */
                pScanMngr->scanParams.channelEntry[ pScanMngr->scanParams.numOfChannels ].SPSChannelEntry.scanStartTime = 
                    INT64_LOWER( (nextEventArray[ nextEventArrayHead ].nextEventTSF));
                pScanMngr->scanParams.channelEntry[ pScanMngr->scanParams.numOfChannels ].SPSChannelEntry.scanDuration = 
                    scanMethod->method.spsMethodParams.scanDuration;
                pScanMngr->scanParams.channelEntry[ pScanMngr->scanParams.numOfChannels ].SPSChannelEntry.ETMaxNumOfAPframes = 
                    scanMethod->method.spsMethodParams.ETMaxNumberOfApFrames;
                pScanMngr->scanParams.channelEntry[ pScanMngr->scanParams.numOfChannels ].SPSChannelEntry.earlyTerminationEvent = 
                    scanMethod->method.spsMethodParams.earlyTerminationEvent;
                pScanMngr->scanParams.channelEntry[ pScanMngr->scanParams.numOfChannels ].SPSChannelEntry.channel = 
                    pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].channel;
                MAC_COPY (pScanMngr->scanParams.channelEntry[ pScanMngr->scanParams.numOfChannels ].SPSChannelEntry.bssId,
                          pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID);
                /* increase the AP track attempts counter */
                pScanMngr->BSSList.scanBSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].trackFailCount++;
                /* increase number of channels in scan command */
                pScanMngr->scanParams.numOfChannels++;
                /* set earliest TSF that would fit in scan command */
                EarliestTSFToInsert = nextEventArray[ nextEventArrayHead ].nextEventTSF + 
                                      scanMethod->method.spsMethodParams.scanDuration + 
                                      SCAN_SPS_GUARD_FROM_LAST_BSS;
                /* remove it from next event array */
                nextEventArrayHead = nextEventArray[ nextEventArrayHead ].nextAPIndex;
                nextEventArraySize--;
            }
            else
            {
                TI_UINT32 beaconIntervalUsec = 
                    pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].beaconInterval * 1024;

                /* if the next beacon also collide with DTIM */
                if ( TI_TRUE == scanMngrDTIMInRange( hScanMngr, nextEventArray[ nextEventArrayHead ].nextEventTSF + beaconIntervalUsec,
                                                  nextEventArray[ nextEventArrayHead ].nextEventTSF + scanMethod->method.spsMethodParams.scanDuration + beaconIntervalUsec ))
                {
                    /* An AP whose two consecutive beacons collide with current AP DTIM is not trackable by SPS!!! 
                       Shouldn't happen at a normal setup, but checked to avoid endless loop.
                       First, remove it from the tracking list (by increasing it's track count above the maximum) */
                    pScanMngr->BSSList.scanBSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].trackFailCount = 
                        pScanMngr->scanPolicy.maxTrackFailures + 1;

                    /* and also remove it from the SPS list */
                    nextEventArrayHead = nextEventArray[ nextEventArrayHead ].nextAPIndex;
                    nextEventArraySize--;

#ifdef TI_DBG
                    /* update statistics */
                    pScanMngr->stats.APsRemovedDTIMOverlap++;
#endif
                }
                else
                {
                    /* calculate next event TSF - will get the next beacon, since timeToStartInAdvance is added to current beacon TSF */
                    nextEventArray[ nextEventArrayHead ].nextEventTSF = 
                        scanMngrCalculateNextEventTSF( hScanMngr, &(pScanMngr->BSSList),
                                                       nextEventArray[ nextEventArrayHead ].trackListIndex,
                                                       nextEventArray[ nextEventArrayHead ].nextEventTSF + timeToStartInAdvance + 1) 
                                                            - timeToStartInAdvance;

#ifdef SCAN_MNGR_SPS_DBG
                    highValue = INT64_HIGHER( nextEventArray[ nextEventArrayHead ].nextEventTSF );
                    lowValue = INT64_LOWER( nextEventArray[ nextEventArrayHead ].nextEventTSF );
                    TRACE8( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "reacalculating next frame for BSSID:%02x:%02x:%02x:%02x:%02x:%02x at TSF:%x-%x, bacause of DTIM collision\n", pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 0 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 1 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 2 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 3 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 4 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 5 ], highValue, lowValue);
#endif

                    /* reinsert to the next event array, sorted */
                    /* if still needs to be head, do nothing (because it's still head). otherwise: */
                    if ( (1 < nextEventArraySize) && /* list has more than one entry */
                         (nextEventArray[ nextEventArrayHead ].nextEventTSF > nextEventArray[ nextEventArray[ nextEventArrayHead ].nextAPIndex ].nextEventTSF)) /* first event in list is earlier */
                    {
                        /* first remove the head from the list */
                        j = nextEventArrayHead;
                        nextEventArrayHead = nextEventArray[ nextEventArrayHead ].nextAPIndex;

                        /* start with list head */
                        i = nextEventArrayHead;
                        /* while the new AP TSF is larger and list end had not been reached */
                        while ( (nextEventArray[ i ].nextAPIndex != -1) && /* list end had not been reached */
                                (nextEventArray[ nextEventArray[ i ].nextAPIndex ].nextEventTSF < nextEventArray[ j ].nextEventTSF)) /* next event TSF of the next AP in the list is smaller than that of the AP being inserted */
                        {
                            /* proceed to the next AP */
                            i = nextEventArray[ i ].nextAPIndex;
                        }
                        /* insert this AP to the list, right after the next event entry found */
                        nextEventArray[ j ].nextAPIndex = nextEventArray[ i ].nextAPIndex;
                        nextEventArray[ i ].nextAPIndex = j;
                    }

#ifdef SCAN_MNGR_SPS_DBG
                    scanMngrDebugPrintSPSHelperList( hScanMngr, nextEventArray, nextEventArrayHead, maxNextEventArraySize );
#endif
#ifdef TI_DBG
                    /* update statistics */
                    pScanMngr->stats.SPSSavedByDTIMCheck++;
#endif
                }
            }
        }
        else
        {
            /* calculate next event TSF */
            nextEventArray[ nextEventArrayHead ].nextEventTSF = 
                scanMngrCalculateNextEventTSF( hScanMngr, &(pScanMngr->BSSList),
                                               nextEventArray[ nextEventArrayHead ].trackListIndex,
                                               EarliestTSFToInsert + timeToStartInAdvance ) - timeToStartInAdvance;

#ifdef SCAN_MNGR_SPS_DBG
            highValue = INT64_HIGHER( nextEventArray[ nextEventArrayHead ].nextEventTSF );
            lowValue = INT64_LOWER( nextEventArray[ nextEventArrayHead ].nextEventTSF );
            TRACE8( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "reacalculating next frame for BSSID:%02x:%02x:%02x:%02x:%02x:%02x at TSF:%x-%x\n", pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 0 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 1 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 2 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 3 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 4 ], pScanMngr->BSSList.BSSList[ nextEventArray[ nextEventArrayHead ].trackListIndex ].BSSID[ 5 ], highValue, lowValue);
#endif

            /* reinsert to the next event array, sorted */
            /* if still needs to be head, do nothing (because it's still head). otherwise: */
            if ( (1 < nextEventArraySize) && /* list has more than one entry */
                 (nextEventArray[ nextEventArrayHead ].nextEventTSF > nextEventArray[ nextEventArray[ nextEventArrayHead ].nextAPIndex ].nextEventTSF)) /* first event in list is earlier */
            {
                /* first remove the head from the list */
                j = nextEventArrayHead;
                nextEventArrayHead = nextEventArray[ nextEventArrayHead ].nextAPIndex;

                /* start with list head */
                i = nextEventArrayHead;
                /* while the new AP TSF is larger and list end had not been reached */
                while ( (nextEventArray[ i ].nextAPIndex != -1) && /* list end had not been reached */
                        (nextEventArray[ nextEventArray[ i ].nextAPIndex ].nextEventTSF < nextEventArray[ j ].nextEventTSF)) /* next event TSF of the next AP in the list is smaller than that of the AP being inserted */
                {
                    /* proceed to the next AP */
                    i = nextEventArray[ i ].nextAPIndex;
                }
                /* insert this AP to the list, right after the next event entry found */
                nextEventArray[ j ].nextAPIndex = nextEventArray[ i ].nextAPIndex;
                nextEventArray[ i ].nextAPIndex = j;
            }

#ifdef SCAN_MNGR_SPS_DBG
            scanMngrDebugPrintSPSHelperList( hScanMngr, nextEventArray, nextEventArrayHead, maxNextEventArraySize );
#endif
        }
    }
    /* For SPS scan, the scan duration is added to the command, since later on current TSF cannot be 
       reevaluated. The scan duration is TSF at end of scan minus current TSF, divided by 1000 (convert
       to milliseconds) plus 1 (for the division reminder). */
    pScanMngr->scanParams.SPSScanDuration = 
        (((TI_UINT32)(EarliestTSFToInsert - SCAN_SPS_GUARD_FROM_LAST_BSS - pScanMngr->currentTSF)) / 1000) + 1;    
}

/**
 * \\n
 * \date 07-Mar-2005\n
 * \brief Calculates local TSF of the next event (beacon or GPR) of the given tracked AP.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param BSSList - a pointer to the track list.\n
 * \param entryIndex - the index of the AP for which calculation is requires in the tracking list.\n
 * \param initialTSFValue - local TSF value AFTER which the next event is to found.\n
 * \return The approximate current TSF
 */
TI_UINT64 scanMngrCalculateNextEventTSF( TI_HANDLE hScanMngr, scan_BSSList_t* BSSList, TI_UINT8 entryIndex, TI_UINT64 initialTSFValue )
{
    TI_UINT64 remoteBeaconTSF, localBeaconTSF;
    TI_INT64 localRemoteTSFDelta;
    TI_UINT32 reminder;
    TI_INT32 averageDeltaChange = 0;
    int i; 
#ifdef SCAN_MNGR_SPS_DBG
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
#endif /* SCAN_MNGR_SPS_DBG */

/* graphical representation:
                               E      E      E      E      E      E      E      E      E
Remote TSF Line:               |      |      |      |      |      |      |      |      |
0                    remoteTSF |      |      |      |      |      |      |      |      | Returned value
+-----------------------+------+------+------+------+------+------+------+------+------+------+----------------...

Local TSF Line:
    0                localTSF                                                   initialTSFValue
    +-------------------+---------------------------------------------------------------+-----+---------------...

  note that:
  1. both lines Don't start at the same time!
  2. remoteTSF and localTSF were measured when last frame was received from the tracked AP. the difference between their
     values is the constant difference between the two lines.
  3. initialTSFValue is the local TSF the first event after which is requested.
  4. returned value is the TSF (in local scale!) of the next event of the tracked AP.
  5. an E represents an occurring event, which is a scheduled frame transmission (beacon or GPR) of the tracked AP.
*/
  
    /*
     * The next event TSF is calculated as follows:
     * first, the difference between the local TSF (that of the AP the STA is currently connected to) and the 
     * remote TSF (that of the AP being tracked) is calculated using the TSF values measured when last scan was
     * performed. Than, the initial TSF value is converted to remote TSF value, using the delta just calculated.
     * The next remote TSF is found (in remote TSF value) by subtracting the reminder of dividing the current
     * remote TSF value by the remote beacon interval (time passed from last beacon) from the current remote TSF
     * (hence amounting to the last beacon remote TSF), and than adding beacon interval. This is finally converted 
     * back to local TSF, which is the requested value.
     *
     * After all this is done, clock drift between current AP and remote AP is compensated. This is done in thr
     * following way: the delte between local TSF and remote TSF is compared to this value at the last scan
     * (if they are equal, the clocks tick at the same rate). This difference is store in an array holding a
     * configured number of such previous differences (currenlty 4). The average value of these previous values
     * is then calculated, and added the the TSF value calculated before. This way, the average drift between 
     * the local AP and the candidate AP is measured, and the next drift value can be estimated and thus
     * taken into account.
     */

#ifdef SCAN_MNGR_SPS_DBG
    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "initial TSF value:%x-%x\n", INT64_HIGHER( initialTSFValue ), INT64_LOWER( initialTSFValue ));
    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "local time stamp:%x-%x\n", INT64_HIGHER( BSSList->scanBSSList[ entryIndex ].localTSF ), INT64_LOWER( BSSList->scanBSSList[ entryIndex ].localTSF ));
#endif
    /* calculate the delta between local and remote TSF */
    localRemoteTSFDelta = BSSList->scanBSSList[ entryIndex ].localTSF - 
        BSSList->BSSList[ entryIndex ].lastRxTSF;
    /* convert initial TSF to remote timeline */
    remoteBeaconTSF = initialTSFValue - localRemoteTSFDelta;
#ifdef SCAN_MNGR_SPS_DBG
    TRACE4( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Local TSF:%u-%u, Remote TSF: %u-%u\n", INT64_HIGHER(BSSList->scanBSSList[ entryIndex ].localTSF), INT64_LOWER(BSSList->scanBSSList[ entryIndex ].localTSF), INT64_HIGHER(BSSList->BSSList[ entryIndex ].lastRxTSF), INT64_LOWER(BSSList->BSSList[ entryIndex ].lastRxTSF)));
    TRACE4( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "TSF delta:%u-%u, current remote TSF:%u-%u\n", INT64_HIGHER(localRemoteTSFDelta), INT64_LOWER(localRemoteTSFDelta), INT64_HIGHER(remoteBeaconTSF ), INT64_LOWER(remoteBeaconTSF ));
#endif
    /* find last remote beacon transmission by subtracting the reminder of current remote TSF divided
       by the beacon interval (indicating how much time passed since last beacon) from current remote
       TSF */
    reminder = reminder64( remoteBeaconTSF, BSSList->BSSList[ entryIndex ].beaconInterval * 1024 );
    remoteBeaconTSF -= reminder;

#ifdef SCAN_MNGR_SPS_DBG
    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "reminder=%d\n",reminder);
    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Last remote beacon TSF:%x-%x\n", INT64_HIGHER(remoteBeaconTSF), INT64_LOWER(remoteBeaconTSF));
#endif
    /* advance from last beacon to next beacon */
    remoteBeaconTSF += BSSList->BSSList[ entryIndex ].beaconInterval * 1024;
#ifdef SCAN_MNGR_SPS_DBG
    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Next remote beacon TSF:%x-%x\n", INT64_HIGHER(remoteBeaconTSF), INT64_LOWER(remoteBeaconTSF));
#endif

#ifdef SCAN_SPS_USE_DRIFT_COMPENSATION
    /* update delta change array with the change between current and last delta (if last delta is valid) */
    if ( 0 != BSSList->scanBSSList[ entryIndex ].prevTSFDelta )
    {
        BSSList->scanBSSList[ entryIndex ].deltaChangeArray[ BSSList->scanBSSList[ entryIndex ].deltaChangeArrayIndex ] = 
            (TI_INT32)(localRemoteTSFDelta - BSSList->scanBSSList[ entryIndex ].prevTSFDelta);
#ifdef SCAN_MNGR_SPS_DBG
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "current delta^2:%d\n", localRemoteTSFDelta - BSSList->scanBSSList[ entryIndex ].prevTSFDelta);
#endif
        if ( SCAN_SPS_NUM_OF_TSF_DELTA_ENTRIES == ++BSSList->scanBSSList[ entryIndex ].deltaChangeArrayIndex )
        {
            BSSList->scanBSSList[ entryIndex ].deltaChangeArrayIndex = 0;
        }
    }
    BSSList->scanBSSList[ entryIndex ].prevTSFDelta = localRemoteTSFDelta;
    
    /* calculate average delta change, and add (or subtract) it from beacon timing */
    for ( i = 0; i < SCAN_SPS_NUM_OF_TSF_DELTA_ENTRIES; i++ )
    {
        averageDeltaChange += BSSList->scanBSSList[ entryIndex ].deltaChangeArray[ i ];
    }
    averageDeltaChange /= SCAN_SPS_NUM_OF_TSF_DELTA_ENTRIES;
#ifdef SCAN_MNGR_SPS_DBG
    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "average delta change: %d\n", averageDeltaChange);
#endif /* SCAN_MNGR_SPS_DBG */
    remoteBeaconTSF += averageDeltaChange;
#endif

    /* convert to local TSF */
    localBeaconTSF = remoteBeaconTSF + localRemoteTSFDelta;

#ifdef SCAN_SPS_USE_DRIFT_COMPENSATION
    /* if beacon (in local TSF) is before initial TSF value (possible due to drift compensation),
       proceed to next beacon */
    if ( localBeaconTSF < initialTSFValue )
    {
        localBeaconTSF += (BSSList->BSSList[ entryIndex ].beaconInterval * 1024);
    }
#endif

    return localBeaconTSF;
}

/**
 * \\n
 * \date 20-September-2005\n
 * \brief Check whether a time range collides with current AP DTIM
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param rangeStart - the time range start TSF.\n
 * \param rangeEnd - the time range end TSF.\n
 * \return Whether the event collides with a DTIM (TRUF if it does, TI_FALSE if it doesn't).\n
 */
TI_BOOL scanMngrDTIMInRange( TI_HANDLE hScanMngr, TI_UINT64 rangeStart, TI_UINT64 rangeEnd )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TI_UINT64 DTIMEventStart, DTIMEventEnd;
    TI_UINT32 DTIMPeriodInUsec; /* DTIM period in micro seconds */

#ifdef SCAN_MNGR_DTIM_DBG
    TRACE4( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "DTIM check: SPS raneg start:%x-%x, end:%x-%x\n", INT64_HIGHER(rangeStart), INT64_LOWER(rangeStart), INT64_HIGHER(rangeEnd), INT64_LOWER(rangeEnd));
#endif

    /* calculate DTIM period */
    DTIMPeriodInUsec = pScanMngr->currentBSSBeaconInterval * 1024 * pScanMngr->currentBSSDtimPeriod;

#ifdef SCAN_MNGR_DTIM_DBG
    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "DTIM period in usec: %d\n", DTIMPeriodInUsec);
#endif

    /* calculate (from DTIM count) the first DTIM after the last seen beacon. The last seen beacon will always
       occur before the SPS - because it already happened, and the SPS is a future event. However, the next DTIM
       is not necessarily prior to the SPS - it is also a future event (if the last beacon was not a DTIM) */
    if ( 0 == pScanMngr->lastLocalBcnDTIMCount )
    {   /* The last beacon was a DTIM */
        DTIMEventStart = pScanMngr->lastLocalBcnTSF;
    }
    else
    {   /* The last beacon was not a DTIM - calculate the next beacon that will be a DTIM */
        DTIMEventStart = pScanMngr->lastLocalBcnTSF + 
            ((pScanMngr->currentBSSDtimPeriod - pScanMngr->lastLocalBcnDTIMCount) * pScanMngr->currentBSSBeaconInterval);
        TRACE6(pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "\n Next DTIM TSF:%u-%u , last beacon TSF:%u-%u, last DTIM count: %d, beacon interval: %d\n", INT64_HIGHER(DTIMEventStart), INT64_LOWER(DTIMEventStart), INT64_HIGHER(pScanMngr->lastLocalBcnTSF), INT64_LOWER(pScanMngr->lastLocalBcnTSF), pScanMngr->lastLocalBcnDTIMCount, pScanMngr->currentBSSBeaconInterval);
    }
#ifdef SCAN_MNGR_DTIM_DBG
    TRACE6( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Next DTIM TSF:%u-%u, last beacon TSF:%u-%u, last DTIM count: %d, beacon interval: %d\n", INT64_HIGHER(DTIMEventStart), INT64_LOWER(DTIMEventStart), INT64_HIGHER(pScanMngr->lastLocalBcnTSF), INT64_LOWER(pScanMngr->lastLocalBcnTSF), pScanMngr->lastLocalBcnDTIMCount, pScanMngr->currentBSSBeaconInterval);
#endif

    /* calculate the DTIM event end (add the DTIM length). Note that broadcast frames after the DTIM are not
       taken into consideration because their availability and length varies. Even if at some point SPS will be
       missed due to broadcast RX frames, it does not mean this AP cannot be tracked. */
    DTIMEventEnd = DTIMEventStart + SCAN_SPS_FW_DTIM_LENGTH;

    /* if this DTIM is after the SPS end - than no collision will occur! */
    if ( DTIMEventStart > rangeEnd )
    {
#ifdef SCAN_MNGR_DTIM_DBG
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "no collision because DTIM is after SPS\n");
#endif
        return TI_FALSE;
    }
    /* if this DTIM end is not before the SPS range start - it means the DTIM is colliding with the SPS, because
       it neither ends before the SPS nor starts after it */
    else if ( DTIMEventEnd >= rangeStart )
    {
#ifdef SCAN_MNGR_DTIM_DBG
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Collision beacuse DTIM is not before SPS\n");
#endif        
        return TI_TRUE;
    }
    /* the DTIM is before the SPS range - find the first DTIM after the SPS start (and check if it's colliding
       with the SPS range */
    else 
    {
        /* get the usec difference from the SPS range start to the last DTIM */
        TI_UINT64 usecDiffFromRangeStartToLastDTIM = rangeStart - DTIMEventStart;
        /* get the reminder from the usec difference divided by the DTIM period - the time (in usec) from last DTIM
           to SPS start */
        TI_UINT32 reminder = reminder64( usecDiffFromRangeStartToLastDTIM, DTIMPeriodInUsec );
        /* get the next DTIM start time by adding DTIM period to the last DTIM before the SPS range start */
        DTIMEventStart = rangeStart - reminder + DTIMPeriodInUsec;
        /* get DTIM end time */
        DTIMEventEnd = DTIMEventStart + SCAN_SPS_FW_DTIM_LENGTH;
#ifdef SCAN_MNGR_DTIM_DBG
        TRACE7( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Diff from range start to last DTIM: %x-%x, reminder:%d, DTIM start:%x-%x, DTIM end: %x-%x\n", INT64_HIGHER(usecDiffFromRangeStartToLastDTIM), INT64_LOWER(usecDiffFromRangeStartToLastDTIM), reminder, INT64_HIGHER(DTIMEventStart), INT64_LOWER(DTIMEventStart), INT64_HIGHER(DTIMEventEnd), INT64_LOWER(DTIMEventEnd));
#endif

        /* if the SPS starts after the DTIM ends or before the DTIM starts - no collision occurs */
        if ( (rangeStart > DTIMEventEnd) || (rangeEnd < DTIMEventStart))
        {
#ifdef SCAN_MNGR_DTIM_DBG
            TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "No collision will occur because DTIM is before or after SPS\n");
#endif
            return TI_FALSE;
        }
        /* otherwise - a collision will occur! */
        {
#ifdef SCAN_MNGR_DTIM_DBG
            TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Collision will occur!\n");
#endif
            return TI_TRUE;
        }
    }
}

/**
 * \\n
 * \date 03-Mar-2005\n
 * \brief Add a normal channel entry to the object workspace scan command.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param scanMethod - The scan method (and parameters) to use.\n
 * \param channel - the channel index.\n
 * \param BSSID - pointer to the BSSID to use (may be broadcast.\n
 * \param txPowerDbm - tx power to transmit probe requests.\n
 */
void scanMngrAddNormalChannel( TI_HANDLE hScanMngr, TScanMethod* scanMethod, TI_UINT8 channel, 
                               TMacAddr* BSSID, TI_UINT8 txPowerDbm )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int commandChannelIndex;
    TScanBasicMethodParams* basicMethodParams;

    /* get next channel in the object workspace */
    commandChannelIndex = pScanMngr->scanParams.numOfChannels;
    pScanMngr->scanParams.numOfChannels++;

    /* get basic method params pointer according to scan type */
    switch ( scanMethod->scanType )
    {
    case SCAN_TYPE_NORMAL_PASSIVE:
    case SCAN_TYPE_NORMAL_ACTIVE:  
        basicMethodParams = &(scanMethod->method.basicMethodParams);
        break;

    case SCAN_TYPE_TRIGGERED_PASSIVE:
    case SCAN_TYPE_TRIGGERED_ACTIVE:
        basicMethodParams = &(scanMethod->method.TidTriggerdMethodParams.basicMethodParams);
        break;

    default:
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Unercognized scan type %d when adding normal channel to scan list.\n", scanMethod->scanType );
        basicMethodParams = NULL;
		return;
    }

    /* set params */
    pScanMngr->scanParams.channelEntry[ commandChannelIndex ].normalChannelEntry.channel = channel;
    pScanMngr->scanParams.channelEntry[ commandChannelIndex ].normalChannelEntry.txPowerDbm = 
        TI_MIN( txPowerDbm, basicMethodParams->probReqParams.txPowerDbm );
    pScanMngr->scanParams.channelEntry[ commandChannelIndex ].normalChannelEntry.maxChannelDwellTime = 
        basicMethodParams->maxChannelDwellTime;
    pScanMngr->scanParams.channelEntry[ commandChannelIndex ].normalChannelEntry.minChannelDwellTime = 
        basicMethodParams->minChannelDwellTime;
    pScanMngr->scanParams.channelEntry[ commandChannelIndex ].normalChannelEntry.earlyTerminationEvent = 
        basicMethodParams->earlyTerminationEvent;
    pScanMngr->scanParams.channelEntry[ commandChannelIndex ].normalChannelEntry.ETMaxNumOfAPframes = 
        basicMethodParams->ETMaxNumberOfApFrames;
    
   MAC_COPY (pScanMngr->scanParams.channelEntry[ commandChannelIndex ].normalChannelEntry.bssId, *BSSID);
}
                                    
/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Removes an entry from the BSS list (by replacing it with another entry, if any).
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param BSSEntryIndex - index of the entry to remove.\n
 */
void scanMngrRemoveBSSListEntry( TI_HANDLE hScanMngr, TI_UINT8 BSSEntryIndex )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TI_UINT8* tempResultBuffer;

#ifdef TI_DBG
    /*update statistics */
    if ( SCAN_MNGR_STAT_MAX_TRACK_FAILURE <= pScanMngr->BSSList.scanBSSList[ BSSEntryIndex ].trackFailCount )
    {
        pScanMngr->stats.ConsecutiveTrackFailCountHistogram[ SCAN_MNGR_STAT_MAX_TRACK_FAILURE - 1 ]++;
    }
    else
    {
        pScanMngr->stats.ConsecutiveTrackFailCountHistogram[ pScanMngr->BSSList.scanBSSList[ BSSEntryIndex ].trackFailCount ]++;
    }
#endif
    /* if no more entries are available, simply reduce the number of entries.
       As this is the last entry, it won't be accessed any more. */
    if ( (pScanMngr->BSSList.numOfEntries-1) == BSSEntryIndex )
    {

        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Removing last entry %d in BSS list\n", pScanMngr->BSSList.numOfEntries);

        pScanMngr->BSSList.numOfEntries--;
    }
    else
    {
#ifdef SCAN_MNGR_DBG
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Removing entry %d of %d\n", BSSEntryIndex, pScanMngr->BSSList.numOfEntries);
#endif
        /* keep the scan result buffer pointer */
        tempResultBuffer = pScanMngr->BSSList.BSSList[ BSSEntryIndex ].pBuffer;
        /* copy the last entry over this one */
        os_memoryCopy( pScanMngr->hOS, &(pScanMngr->BSSList.BSSList[ BSSEntryIndex ]),
                       &(pScanMngr->BSSList.BSSList[ pScanMngr->BSSList.numOfEntries-1 ]),
                       sizeof(bssEntry_t));
        os_memoryCopy( pScanMngr->hOS, &(pScanMngr->BSSList.scanBSSList[ BSSEntryIndex ]),
                       &(pScanMngr->BSSList.scanBSSList[ pScanMngr->BSSList.numOfEntries-1 ]),
                       sizeof(scan_BSSEntry_t));
        /* replace the scan result buffer of the last entry */
        pScanMngr->BSSList.BSSList[ pScanMngr->BSSList.numOfEntries-1 ].pBuffer = tempResultBuffer;
        /* decrease the number of BSS entries */
        pScanMngr->BSSList.numOfEntries--;
    }
}

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Removes all BSS list entries that are neither neighbor APs not on a policy defined channel.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bCheckNeighborAPs - whether to verify that APs marked as neighbor APs are really neighbor APs.\n
 * \param bCheckChannels - whether to verify that APs not marked as neighbor APs are on policy defined channel.\n
 */
void scanMngrUpdateBSSList( TI_HANDLE hScanMngr, TI_BOOL bCheckNeighborAPs, TI_BOOL bCheckChannels )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int BSSEntryIndex;

    /* It looks like it never happens. Anyway decided to check */
    if (pScanMngr->BSSList.numOfEntries > MAX_SIZE_OF_BSS_TRACK_LIST)
    {        
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                 "scanMngrUpdateBSSList problem. BSSList.numOfEntries=%d exceeds the limit %d\n",
                 pScanMngr->BSSList.numOfEntries, MAX_SIZE_OF_BSS_TRACK_LIST);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }

    /* loop on all BSS list entry */    
    for ( BSSEntryIndex = 0; BSSEntryIndex < pScanMngr->BSSList.numOfEntries; )
    {
        /* an AP can be in the BSS list either because it's a neighbor AP or, if not, because it's on a
           policy defined channel. When Neighbor AP list is changed, it is only necessary to check APs that
           are in the list because they are neighbor APs. When the policy is changed, it is only necessary
           to check APs that are in the list because they are on a policy defined channel. */

        /* if a check for neighbor APs is requested, check only APs that are designated as neighbor APs,
           and only check if they still are neighbor APs */
        if ( (TI_TRUE == bCheckNeighborAPs) && 
             (TI_TRUE == pScanMngr->BSSList.BSSList[ BSSEntryIndex ].bNeighborAP) &&
             (-1 == scanMngrGetNeighborAPIndex( hScanMngr, 
                                                pScanMngr->BSSList.BSSList[ BSSEntryIndex ].band,
                                                &(pScanMngr->BSSList.BSSList[ BSSEntryIndex ].BSSID))))
        {
            /* remove it */
            scanMngrRemoveBSSListEntry( hScanMngr, BSSEntryIndex );
            /* repeat the loop with the same index to check the new BSS on this place */
            continue;
        }

        /* if a check for policy defined channels is requested, check only APs that are not designated as 
           neighbor APs */
        if ( (TI_TRUE == bCheckChannels) &&
             (TI_FALSE == pScanMngr->BSSList.BSSList[ BSSEntryIndex ].bNeighborAP) &&
             (TI_FALSE == scanMngrIsPolicyChannel( hScanMngr, 
                                                pScanMngr->BSSList.BSSList[ BSSEntryIndex ].band, 
                                                pScanMngr->BSSList.BSSList[ BSSEntryIndex ].channel )))
        {
            /* remove it */
            scanMngrRemoveBSSListEntry( hScanMngr, BSSEntryIndex );
        }
        else
        {
            BSSEntryIndex++;
        }
    }
}

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief returns the index of a neighbor AP.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param band - the band on which the AP resides.\n
 * \param bssId - the AP's BSSID.\n
 * \return the index into the neighbor AP list for the given address, -1 if AP is not in list.\n
 */
TI_INT8 scanMngrGetNeighborAPIndex( TI_HANDLE hScanMngr, ERadioBand band, TMacAddr* bssId )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i;

    /* loop on all neighbor APS for this AP's band, and compare BSSID's */
    for ( i = 0; i < pScanMngr->neighborAPsDiscoveryList[ band ].numOfEntries; i++ )
    {
        if (MAC_EQUAL (*bssId, pScanMngr->neighborAPsDiscoveryList[ band ].APListPtr[ i ].BSSID))
        {
            return i;
        }
    }

    /* if the AP wasn't found in the list, it's not a neighbor AP... */
    return -1;
}

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Checks whether a channel is defined on a policy.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param band - the band on which the channel is.\n
 * \param channel - the channel number.\n
 * \return TI_TRUE if channel is defined on policy, TI_FALSE otherwise.\n
 */
TI_BOOL scanMngrIsPolicyChannel( TI_HANDLE hScanMngr, ERadioBand band, TI_UINT8 channel )
{
    int i;
    TScanBandPolicy* bandPolicy = scanMngrGetPolicyByBand( hScanMngr, band );

    
    /* check if the AP's band is defined in the policy */
    if ( NULL == bandPolicy )
    {
        return TI_FALSE;
    }

    /* loop on all channels for the AP's band */
    for ( i = 0; i < bandPolicy->numOfChannles; i++ )
    {
        if ( bandPolicy->channelList[ i ] == channel )
        {
            return TI_TRUE;
        }
    }

    /* if no channel was found, the AP is NOT on a policy configured channel */
    return TI_FALSE;
}

/**
 * \\n
 * \date 18-Apr-2005\n
 * \brief Converts scan concentrator result status to scan manager result status, to be returned to roaming manager.\n
 *
 * Function Scope \e Private.\n
 * \param result status - scan concentrator result status.\n
 * \return appropriate scan manager status.\n
 */
scan_mngrResultStatus_e scanMngrConvertResultStatus( EScanCncnResultStatus resultStatus )
{
    switch (resultStatus)
    {
    case SCAN_CRS_SCAN_COMPLETE_OK:
        return  SCAN_MRS_SCAN_COMPLETE_OK;
/*        break; - unreachable */

    case SCAN_CRS_SCAN_RUNNING:
        return SCAN_MRS_SCAN_RUNNING;
/*        break; - unreachable */

    case SCAN_CRS_SCAN_FAILED:
        return SCAN_MRS_SCAN_FAILED;
/*        break; - unreachable */

    case SCAN_CRS_SCAN_STOPPED:
        return SCAN_MRS_SCAN_STOPPED;
/*        break; - unreachable */

    case SCAN_CRS_TSF_ERROR:
        return SCAN_MRS_SCAN_FAILED;
/*        break; - unreachable */

    case SCAN_CRS_SCAN_ABORTED_FW_RESET:
        return SCAN_MRS_SCAN_ABORTED_FW_RESET;
/*        break; - unreachable */

    case SCAN_CRS_SCAN_ABORTED_HIGHER_PRIORITY:
        return SCAN_MRS_SCAN_ABORTED_HIGHER_PRIORITY;
/*        break; - unreachable */

    default:
        return SCAN_MRS_SCAN_FAILED;
/*        break; - unreachable */
    }
}

/************************************************************************/
/*                         Trace functions                             */
/************************************************************************/

#ifdef REPORT_LOG

static char scanTypeDesc[ 6 ][ MAX_DESC_LENGTH ] = 
{
    "passive normal scan",
    "active normal scan",
    "SPS scan",
    "passive triggered scan",
    "active triggered scan",
    "no scan type"
};

static char earlyTerminationConditionDesc[ 4 ][ MAX_DESC_LENGTH ] =
{
    "Early termination disabled",
    "Early termination on beacon",
    "Early termination on probe response",
    "Early termination on both"
};

#ifdef TI_DBG
static char booleanDesc[ 2 ][ MAX_DESC_LENGTH ] =
{
    "No",
    "Yes"
};

static char contScanStatesDesc[ SCAN_CSS_NUM_OF_STATES ][ MAX_DESC_LENGTH ] =
{
    "IDLE",
    "TRACKING ON G",
    "TRACKING ON A",
    "DISCOVERING",
    "STOPPING"
};

static char immedScanStatesDesc[ SCAN_ISS_NUM_OF_STATES ][ MAX_DESC_LENGTH ] = 
{
    "IDLE",
    "IMMEDIATE ON G",
    "IMMEDIATE ON A",
    "STOPPING"
};

static char discoveryPartDesc[ SCAN_SDP_NUMBER_OF_DISCOVERY_PARTS ][ MAX_DESC_LENGTH ] =
{
    "G neighbor APs",
    "A neighbor APs",
    "G channels",
    "A Channels",
    "No discovery"
};

static char neighborDiscovreyStateDesc[ SCAN_NDS_NUMBER_OF_NEIGHBOR_DISCOVERY_STATES ][ MAX_DESC_LENGTH ] =
{
    "Discovered",
    "Not discovered",
    "Current AP"
};

static char earlyTerminationDesc[ SCAN_ET_COND_NUM_OF_CONDS ][ MAX_DESC_LENGTH ] =
{
    "None",
    "Beacon",
    "Prob. resp."
    "Bcn & prob. resp."
};
#endif

#endif

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a neighbor AP list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param neighborAPList - the list of neighbor APs to print.\n
 */
void scanMngrTracePrintNeighborAPsList( TI_HANDLE hScanMngr, neighborAPList_t *neighborAPList )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i;

    /* It looks like it never happens. Anyway decided to check */
    if ( neighborAPList->numOfEntries > MAX_NUM_OF_NEIGHBOR_APS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrTracePrintNeighborAPsList. neighborAPList->numOfEntries=%d exceeds the limit %d\n",
                neighborAPList->numOfEntries, MAX_NUM_OF_NEIGHBOR_APS);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* print number of entries */
    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Neighbor AP list with %d entries.\n\n", neighborAPList->numOfEntries);
    
    /* print all APs in list */
    for ( i = 0; i < neighborAPList->numOfEntries; i++ )
    {
        scanMngrTracePrintNeighborAP( hScanMngr, &(neighborAPList->APListPtr[ i ]));
    }
}

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a neighbor AP.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param neighborAP - the neighbor AP to print.\n
 */
void scanMngrTracePrintNeighborAP( TI_HANDLE hScanMngr, neighborAP_t* neighborAP )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    /* print neighbor AP content */
    TRACE7( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Neighbor AP band: , channel: %d, MAC address (BSSID): %2x:%2x:%2x:%2x:%2x:%2xn", neighborAP->channel, neighborAP->BSSID[ 0 ], neighborAP->BSSID[ 1 ], neighborAP->BSSID[ 2 ], neighborAP->BSSID[ 3 ], neighborAP->BSSID[ 4 ], neighborAP->BSSID[ 5 ]);
}

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print scan policy.\n
 *
 * Function Scope \e Private.\n
 * \param scanPolicy - scan policy to print.\n
 */
void scanMngrTracePrintScanPolicy( TScanPolicy* scanPolicy )
{
    int i;

    /* print general policy parameters */
    WLAN_OS_REPORT(("Global policy parameters:\n"));
    WLAN_OS_REPORT(("Normal scan interval: %d, deteriorating scan interval: %d\n",
                    scanPolicy->normalScanInterval, scanPolicy->deterioratingScanInterval));
    WLAN_OS_REPORT(("BSS list size: %d, numnber of tracked APs to start discovery: %d, "
                    "Max track failures:% d\n", scanPolicy->BSSListSize, 
                    scanPolicy->BSSNumberToStartDiscovery, scanPolicy->maxTrackFailures));
    /* It looks like it never happens. Anyway decided to check */
    if ( scanPolicy->numOfBands > RADIO_BAND_NUM_OF_BANDS )
    {
        WLAN_OS_REPORT(("scanMngrTracePrintScanPolicy. scanPolicy->numOfBands=%d exceeds the limit %d\n",
                scanPolicy->numOfBands, RADIO_BAND_NUM_OF_BANDS));
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* print band policy parameters for all available bands */
    for ( i = 0; i < scanPolicy->numOfBands; i++ )
    {
        scanMngrTracePrintBandScanPolicy( &(scanPolicy->bandScanPolicy[ i ]));
    }
}

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a band scan policy AP.\n
 *
 * Function Scope \e Private.\n
 * \param bandPolicy - the band scan policy to print.\n
 */
void scanMngrTracePrintBandScanPolicy( TScanBandPolicy* bandPolicy )
{
    int i;

    WLAN_OS_REPORT(("Band scan policy for band: %s\n", 
                    (RADIO_BAND_2_4_GHZ == bandPolicy->band ? "2.4 GHz (b/g)" : "5.0 GHz (a)")));
    WLAN_OS_REPORT(("Maximal number of channels to scan at each discovery interval %d:\n", 
                    bandPolicy->numOfChannlesForDiscovery));
    WLAN_OS_REPORT(("RSSI Threshold: %d\n", bandPolicy->rxRSSIThreshold));
    WLAN_OS_REPORT(("Tracking method:\n"));
    scanMngrTracePrintScanMethod( &(bandPolicy->trackingMethod));
    WLAN_OS_REPORT(("Discovery method:\n"));
    scanMngrTracePrintScanMethod( &(bandPolicy->discoveryMethod));
    WLAN_OS_REPORT(("Immediate scan method:\n"));
    scanMngrTracePrintScanMethod( &(bandPolicy->immediateScanMethod));
    /* It looks like it never happens. Anyway decided to check */
    if ( bandPolicy->numOfChannles > MAX_BAND_POLICY_CHANNLES )
    {
        WLAN_OS_REPORT(("scanMngrTracePrintBandScanPolicy. bandPolicy->numOfChannles=%d exceeds the limit %d\n",
                bandPolicy->numOfChannles, MAX_BAND_POLICY_CHANNLES));
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    WLAN_OS_REPORT(("Channels: "));
    for( i = 0; i < bandPolicy->numOfChannles; i++ )
    {
        WLAN_OS_REPORT(("%d ", bandPolicy->channelList[ i ]));
    }
    WLAN_OS_REPORT(("\n"));
}

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a scan method
 *
 * Function Scope \e Private.\n
 * \param scanMethod - the scan method to print.\n
 */
void scanMngrTracePrintScanMethod( TScanMethod* scanMethod )
{
    WLAN_OS_REPORT(("Scan type: %s\n", scanTypeDesc[ scanMethod->scanType ]));

    switch (scanMethod->scanType)
    {
    case SCAN_TYPE_NORMAL_ACTIVE:
    case SCAN_TYPE_NORMAL_PASSIVE:
        scanMngrTracePrintNormalScanMethod( &(scanMethod->method.basicMethodParams));
        break;
    
    case SCAN_TYPE_TRIGGERED_ACTIVE:
    case SCAN_TYPE_TRIGGERED_PASSIVE:
        scanMngrTracePrintTriggeredScanMethod( &(scanMethod->method.TidTriggerdMethodParams));
        break;

    case SCAN_TYPE_SPS:
        scanMngrTracePrintSPSScanMethod( &(scanMethod->method.spsMethodParams));
        break;

    case SCAN_TYPE_NO_SCAN:
    default:
        WLAN_OS_REPORT(("No scan method defined\n"));
        break;
    }
}

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief print a normal scan method
 *
 * Function Scope \e Private.\n
 * \param basicMethodParams - the basic method parameters to print.\n
 */
void scanMngrTracePrintNormalScanMethod( TScanBasicMethodParams* basicMethodParams )
{
    WLAN_OS_REPORT(("Max channel dwell time: %d, min channel dwell time: %d\n",
                    basicMethodParams->maxChannelDwellTime, basicMethodParams->minChannelDwellTime));
    WLAN_OS_REPORT(("Early termination condition: %s, frame number for early termination: %d\n",
                    earlyTerminationConditionDesc[ basicMethodParams->earlyTerminationEvent >> 4 ],
                    basicMethodParams->ETMaxNumberOfApFrames));
    WLAN_OS_REPORT(("Number of probe requests: %d, TX level: %d, probe request rate: %d\n", 
                    basicMethodParams->probReqParams.numOfProbeReqs,
                    basicMethodParams->probReqParams.txPowerDbm,
                    basicMethodParams->probReqParams.bitrate));
}

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief print an AC triggered scan method
 *
 * Function Scope \e Private.\n
 * \param triggeredMethodParams - the AC-triggered method parameters to print.\n
 */
void scanMngrTracePrintTriggeredScanMethod( TScanTidTriggeredMethodParams* triggeredMethodParams )
{
    WLAN_OS_REPORT(("Triggering Tid: %d\n", triggeredMethodParams->triggeringTid));
    scanMngrTracePrintNormalScanMethod( &(triggeredMethodParams->basicMethodParams));
}

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief print a SPS scan method
 *
 * Function Scope \e Private.\n
 * \param SPSMethodParams - the SPS method parameters to print.\n
 */
void scanMngrTracePrintSPSScanMethod( TScanSPSMethodParams* SPSMethodParams )
{
    WLAN_OS_REPORT(("Early termination condition: %s, frame number for early termination: %d\n",
                    earlyTerminationConditionDesc[ SPSMethodParams->earlyTerminationEvent ],
                    SPSMethodParams->ETMaxNumberOfApFrames));
    WLAN_OS_REPORT(("Scan duration: %d\n", SPSMethodParams->scanDuration));
}

/**
 * \\n
 * \date 31-Mar-2005\n
 * \brief print debug information for every received frame.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param frameInfo - holding all frame related information.\n
 */
void scanMngrDebugPrintReceivedFrame( TI_HANDLE hScanMngr, TScanFrameInfo *frameInfo )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "Scan manager received the following frame:\n");
    TRACE8( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "from BSSID: %02x:%02x:%02x:%02x:%02x:%02x, band: %d, channel: %d\n", (*frameInfo->bssId)[ 0 ], (*frameInfo->bssId)[ 1 ], (*frameInfo->bssId)[ 2 ], (*frameInfo->bssId)[ 3 ], (*frameInfo->bssId)[ 4 ], (*frameInfo->bssId)[ 5 ], frameInfo->band, frameInfo->channel);
    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "rate: %d, received at TSF (lower 32 bits): %d\n", frameInfo->rate, frameInfo->staTSF);
    if ( BEACON == frameInfo->parsedIEs->subType )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "remote TSF value: %x-%x\n", INT64_HIGHER( frameInfo->parsedIEs->content.iePacket.timestamp ), INT64_LOWER( frameInfo->parsedIEs->content.iePacket.timestamp ));

    }
    else
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "remote TSF value: %x-%x\n", INT64_HIGHER( frameInfo->parsedIEs->content.iePacket.timestamp ), INT64_LOWER( frameInfo->parsedIEs->content.iePacket.timestamp ));
    }
    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "RSSI: %d\n", frameInfo->rssi);
}
#ifdef TI_DBG
/**
 * \\n
 * \date 31-Mar-2005\n
 * \brief print BSS list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrDebugPrintBSSList( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i, limit;

    if ( 0 == pScanMngr->BSSList.numOfEntries )
    {
        WLAN_OS_REPORT(("BSS list is empty.\n"));
        return;
    }
    limit = pScanMngr->BSSList.numOfEntries;
    /* It looks like it never happens. Anyway decided to check */
	if (pScanMngr->BSSList.numOfEntries > MAX_SIZE_OF_BSS_TRACK_LIST)
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngrDebugPrintBSSList problem. BSSList.numOfEntries=%d Exceeds limit %d\n",
                pScanMngr->BSSList.numOfEntries, MAX_SIZE_OF_BSS_TRACK_LIST);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        limit = MAX_SIZE_OF_BSS_TRACK_LIST;
    }
    
    WLAN_OS_REPORT(("-------------------------------- BSS List--------------------------------\n"));

    for ( i = 0; i < limit; i++ )
    {
        WLAN_OS_REPORT(  ("Entry number: %d\n", i));
        scanMngrDebugPrintBSSEntry( hScanMngr,  i );
    }

    WLAN_OS_REPORT(("--------------------------------------------------------------------------\n"));
}
#endif/*TI_DBG*/
/**
 * \\n
 * \date 31-Mar-2005\n
 * \brief print one entry in the BSS list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param entryIndex - the index of the entry to print.\n
 */
void scanMngrDebugPrintBSSEntry( TI_HANDLE hScanMngr, TI_UINT8 entryIndex )
{
#ifdef REPORT_LOG

    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    bssEntry_t* pBssEntry = &(pScanMngr->BSSList.BSSList[ entryIndex ]);
    scan_BSSEntry_t * pScanBssEntry = &(pScanMngr->BSSList.scanBSSList[ entryIndex ]);

    WLAN_OS_REPORT( ("BSSID: %02x:%02x:%02x:%02x:%02x:%02x, band: %d\n", pBssEntry->BSSID[ 0 ],
                              pBssEntry->BSSID[ 1 ], pBssEntry->BSSID[ 2 ],
                              pBssEntry->BSSID[ 3 ], pBssEntry->BSSID[ 4 ],
                              pBssEntry->BSSID[ 5 ], pBssEntry->band));
    WLAN_OS_REPORT( ("channel: %d, beacon interval: %d, average RSSI: %d dBm\n", 
                              pBssEntry->channel, pBssEntry->beaconInterval, pBssEntry->RSSI));
    WLAN_OS_REPORT(  ("Neighbor AP: %s, track fail count: %d\n", 
                              (TI_TRUE == pBssEntry->bNeighborAP ? "YES" : "NO"),
                              pScanBssEntry->trackFailCount));
    WLAN_OS_REPORT(  ("local TSF: %d-%d, remote TSF: %x-%x\n", 
                              INT64_HIGHER( pScanBssEntry->localTSF ), INT64_LOWER( pScanBssEntry->localTSF ),
                              INT64_HIGHER( pBssEntry->lastRxTSF ), INT64_LOWER( pBssEntry->lastRxTSF )));
    WLAN_OS_REPORT( ("Host Time Stamp: %d, last received rate: %d\n", 
                              pBssEntry->lastRxHostTimestamp, pBssEntry->rxRate));

#endif
}

/**
 * \\n
 * \date 14-Apr-2005\n
 * \brief print SPS helper list
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param spsHelperList - the list to print.\n
 * \param arrayHead - the index of the first element in the list.\n
 * \param arraySize - the size of the array.\n
 */
void scanMngrDebugPrintSPSHelperList( TI_HANDLE hScanMngr, scan_SPSHelper_t* spsHelperList, int arrayHead, int arraySize )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i;

    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "SPS helper list size:%d, list head:%d\n", arraySize, arrayHead);
    for ( i = 0; i < arraySize; i++ )
    {
        TRACE7( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "track list index:%d, BSSID:%02x:%02x:%02x:%02x:%02x:%02x\n", spsHelperList[ i ].trackListIndex, pScanMngr->BSSList.BSSList[ spsHelperList[ i ].trackListIndex ].BSSID[ 0 ], pScanMngr->BSSList.BSSList[ spsHelperList[ i ].trackListIndex ].BSSID[ 1 ], pScanMngr->BSSList.BSSList[ spsHelperList[ i ].trackListIndex ].BSSID[ 2 ], pScanMngr->BSSList.BSSList[ spsHelperList[ i ].trackListIndex ].BSSID[ 3 ], pScanMngr->BSSList.BSSList[ spsHelperList[ i ].trackListIndex ].BSSID[ 4 ], pScanMngr->BSSList.BSSList[ spsHelperList[ i ].trackListIndex ].BSSID[ 5 ]);
        TRACE3( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "TSF:%x-%x, next entry index:%d\n", INT64_HIGHER(spsHelperList[ i ].nextEventTSF), INT64_LOWER(spsHelperList[ i ].nextEventTSF), spsHelperList[ i ].nextAPIndex);
    }
}


/*
 ***********************************************************************
 *  API functions
 ***********************************************************************
 */
TI_HANDLE scanMngr_create( TI_HANDLE hOS )
{
    int i,j = 0;
    scanMngr_t* pScanMngr ;
    
    /* allocate the scan manager object */
    pScanMngr = os_memoryAlloc( hOS, sizeof(scanMngr_t));
    if ( NULL == pScanMngr )
    {
        WLAN_OS_REPORT( ("scanMngr_create: Failed allocating scan manager object storage.\n"));
        return NULL;
    }

    os_memoryZero( pScanMngr->hOS, pScanMngr, sizeof(scanMngr_t));

    pScanMngr->hOS = hOS;

    /* allocate frame storage space for BSS list */
    for (i = 0; i < MAX_SIZE_OF_BSS_TRACK_LIST; i++)
    {
        pScanMngr->BSSList.BSSList[i].pBuffer = os_memoryAlloc (hOS, MAX_BEACON_BODY_LENGTH);
        if (pScanMngr->BSSList.BSSList[i].pBuffer == NULL)
        {
            WLAN_OS_REPORT( ("scanMngr_create: Failed allocating scan result buffer for index %d.\n", i));
            /* failed to allocate a buffer - release all buffers that were allocated by now */
            for (j = i - 1; j >= 0; j--)
            {
                os_memoryFree (hOS, pScanMngr->BSSList.BSSList[j].pBuffer, MAX_BEACON_BODY_LENGTH);
            }
            /* release the rest of the module */
            scanMngrFreeMem ((TI_HANDLE)pScanMngr);
            return NULL;
        }
    }

    return (TI_HANDLE)pScanMngr;
}

void scanMngr_init (TStadHandlesList *pStadHandles)
{
    scanMngr_t *pScanMngr = (scanMngr_t*)(pStadHandles->hScanMngr);
    int i;

    /* store handles */
    pScanMngr->hReport           = pStadHandles->hReport;
    pScanMngr->hRegulatoryDomain = pStadHandles->hRegulatoryDomain;
    pScanMngr->hScanCncn         = pStadHandles->hScanCncn;
    pScanMngr->hRoamingMngr      = pStadHandles->hRoamingMngr;
    pScanMngr->hSiteMngr         = pStadHandles->hSiteMgr;
    pScanMngr->hTWD              = pStadHandles->hTWD;
    pScanMngr->hTimer            = pStadHandles->hTimer;
    pScanMngr->hAPConnection     = pStadHandles->hAPConnection;
    pScanMngr->hEvHandler        = pStadHandles->hEvHandler;

   /* mark the scanning operational mode to be automatic by default */
    pScanMngr->scanningOperationalMode = SCANNING_OPERATIONAL_MODE_AUTO;

    /* mark that continuous scan timer is not running */
    pScanMngr->bTimerRunning = TI_FALSE;

    /* mark that continuous scan process is not running */
    pScanMngr->bContinuousScanStarted = TI_FALSE;

    /* nullify scan policy */
    os_memoryZero( pScanMngr->hOS, &(pScanMngr->scanPolicy), sizeof(TScanPolicy));

    /* initialize the BSS list to empty list */
    pScanMngr->BSSList.numOfEntries = 0;

    /* mark no continuous and immediate scans are currently running */
    pScanMngr->contScanState = SCAN_CSS_IDLE;
    pScanMngr->immedScanState = SCAN_ISS_IDLE;
    pScanMngr->bNewBSSFound = TI_FALSE;
    pScanMngr->consecNotFound = 0;
    
    /* mark no AP recovery occured */
    pScanMngr->bSynchronized = TI_TRUE;
    
    /* mark no neighbor APs */
    pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_2_4_GHZ ].numOfEntries = 0;
    pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_5_0_GHZ ].numOfEntries = 0;

    /* mark no discovery process */
    pScanMngr->currentDiscoveryPart = SCAN_SDP_NO_DISCOVERY;

    /* initialize the low quality indication to indicate that normal quality interval should be used */
    pScanMngr->bLowQuality = TI_FALSE;

    /* clear current BSS field (put broadcast MAC) */
    for (i = 0; i < MAC_ADDR_LEN; i++)
    {
        pScanMngr->currentBSS[i] = 0xff;
    }
    pScanMngr->currentBSSBand = RADIO_BAND_2_4_GHZ;

    /* create timer */
    pScanMngr->hContinuousScanTimer = tmr_CreateTimer (pScanMngr->hTimer);
    if (pScanMngr->hContinuousScanTimer == NULL)
    {
        TRACE0(pScanMngr->hReport, REPORT_SEVERITY_ERROR, "scanMngr_init(): Failed to create hContinuousScanTimer!\n");
    }

    /* register scan concentrator callbacks */
    scanCncn_RegisterScanResultCB( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_CONT,
                                   scanMngr_contScanCB, pStadHandles->hScanMngr );
    scanCncn_RegisterScanResultCB( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_IMMED,
                                   scanMngr_immedScanCB, pStadHandles->hScanMngr );

#ifdef TI_DBG
    /* nullify statistics */
    os_memoryZero( pScanMngr->hOS, &(pScanMngr->stats), sizeof(scan_mngrStat_t));
    /* nullify scan parameters - for debug prints before start */
    os_memoryZero( pScanMngr->hOS, &(pScanMngr->scanParams), sizeof(TScanParams));
    /* initialize other variables for debug print */
    pScanMngr->bImmedNeighborAPsOnly = TI_FALSE;
    pScanMngr->bNewBSSFound = TI_FALSE;
#endif
}

void scanMngr_unload (TI_HANDLE hScanMngr)
{
    scanMngrFreeMem (hScanMngr);
}

scan_mngrResultStatus_e scanMngr_startImmediateScan( TI_HANDLE hScanMngr, TI_BOOL bNeighborAPsOnly )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TScanBandPolicy *gPolicy, *aPolicy;
    EScanCncnResultStatus resultStatus;

    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_startImmediateScan called, hScanMngr=0x%x, bNeighborAPsOnly=.\n", hScanMngr);

    /* sanity check - whether immediate scan is already running */
    if ( SCAN_ISS_IDLE != pScanMngr->immedScanState )
    {
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Immediate scan attempted while it is already running, in state:%d.\n", pScanMngr->immedScanState);
        return SCAN_MRS_SCAN_NOT_ATTEMPTED_ALREADY_RUNNING;
    }

    /* get policies by band */
    gPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_2_4_GHZ );
    aPolicy = scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_5_0_GHZ );

    /* check whether a policy is defined for at least one band */
    if ( ((NULL == gPolicy) || (SCAN_TYPE_NO_SCAN == gPolicy->immediateScanMethod.scanType)) && /* no policy for G band */
         ((NULL == aPolicy) || (SCAN_TYPE_NO_SCAN == aPolicy->immediateScanMethod.scanType))) /* no policy for A band */
    {
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Immediatse scan attempted when no policy is defined.\n");
        return SCAN_MRS_SCAN_NOT_ATTEMPTED_EMPTY_POLICY;
    }

    /* First try to scan on G band - if a policy is defined and channels are available */
    if ( (NULL != gPolicy) && /* policy is defined for G */
         (SCAN_TYPE_NO_SCAN != gPolicy->immediateScanMethod.scanType))
    {
        /* build scan command */
        scanMngrBuildImmediateScanCommand( hScanMngr, gPolicy, bNeighborAPsOnly );
        
        /* if no channels are available, proceed to band A */
        if ( 0 < pScanMngr->scanParams.numOfChannels )
        {
            /* mark that immediate scan is running on band G */
            pScanMngr->immedScanState = SCAN_ISS_G_BAND;
            pScanMngr->bImmedNeighborAPsOnly = bNeighborAPsOnly;

            /* if continuous scan is running, mark that it should quit */
            if ( SCAN_CSS_IDLE != pScanMngr->contScanState )
            {
                TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_startImmediateScan called 1, switched to STOPPING state \n");

                pScanMngr->contScanState = SCAN_CSS_STOPPING;
            }

             /* send scan command to scan concentrator with the required scan params according to scanning operational mode */
            resultStatus = scanMngr_Start1ShotScan(hScanMngr, SCAN_SCC_ROAMING_IMMED);

            if ( SCAN_CRS_SCAN_RUNNING != resultStatus )
            {
                TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start immediate scan on band G, return code %d.\n", resultStatus);
#ifdef TI_DBG
                pScanMngr->stats.ImmediateGByStatus[ resultStatus ]++;
#endif
                return SCAN_MRS_SCAN_FAILED;
            }
            return SCAN_MRS_SCAN_RUNNING;
        }
    }

    /* if G scan did not start (because no policy is configured or no channels are available, try A band */
    if ( (NULL != aPolicy) &&
         (SCAN_TYPE_NO_SCAN != aPolicy->immediateScanMethod.scanType))
    {
        /* build scan command */
        scanMngrBuildImmediateScanCommand( hScanMngr, aPolicy, bNeighborAPsOnly );

        /* if no channels are available, report error */
        if ( 0 == pScanMngr->scanParams.numOfChannels )
        {
            TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "No channels available for scan operation.\n");
            return SCAN_MRS_SCAN_NOT_ATTEMPTED_NO_CHANNLES_AVAILABLE;
        }
        else
        {
            /* mark that immediate scan is running on band A */
            pScanMngr->immedScanState = SCAN_ISS_A_BAND;

            /* if continuous scan is running, mark that it should quit */
            if ( SCAN_CSS_IDLE != pScanMngr->contScanState )
            {
                TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_startImmediateScan called 2, switched to STOPPING state \n");

                pScanMngr->contScanState = SCAN_CSS_STOPPING;
            }

             /* send scan command to scan concentrator with the required scan params according to scanning operational mode */
             resultStatus = scanMngr_Start1ShotScan(hScanMngr, SCAN_SCC_ROAMING_IMMED);
            if ( SCAN_CRS_SCAN_RUNNING != resultStatus )
            {
                TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Failed to start immediate scan on band A, return code %d.\n", resultStatus);
#ifdef TI_DBG
                pScanMngr->stats.ImmediateAByStatus[ resultStatus ]++;
#endif
                return SCAN_MRS_SCAN_FAILED;
            }
            return SCAN_MRS_SCAN_RUNNING;
        }
    }
    else
    {
        /* since we passed the policy check, we arrived here because we didn't had channel on G and policy on A */
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "No channels available for scan operation.\n");
        return SCAN_MRS_SCAN_NOT_ATTEMPTED_NO_CHANNLES_AVAILABLE;
    }
}

void scanMngr_stopImmediateScan( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngrStopImmediateScan called, hScanMngr=0x%x", hScanMngr);

    /* check that immediate scan is running */
    if ( (SCAN_ISS_A_BAND != pScanMngr->immedScanState) && (SCAN_ISS_G_BAND != pScanMngr->immedScanState))
    {
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Immediate scan stop request when immediate scan is in state:%d", pScanMngr->immedScanState);
        return;
    }

#ifdef TI_DBG
    switch ( pScanMngr->immedScanState )
    {
    case SCAN_ISS_G_BAND:
        pScanMngr->stats.ImmediateGByStatus[ SCAN_CRS_SCAN_STOPPED ]++;
        break;

    case SCAN_ISS_A_BAND:
        pScanMngr->stats.ImmediateAByStatus[ SCAN_CRS_SCAN_STOPPED ]++;
        break;

    default:
        break;
    }
#endif
    /* mark immediate scan status as stopping */
    pScanMngr->immedScanState = SCAN_ISS_STOPPING;

    /* send a stop command to scan concentrator */
    scanCncn_StopScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_IMMED );
}

void scanMngr_startContScan( TI_HANDLE hScanMngr, TMacAddr* currentBSS, ERadioBand currentBSSBand )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int currentBSSNeighborIndex;

    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_StartContScan called, hScanMngr=0x%x.\n", hScanMngr);
    /* It looks like it never happens. Anyway decided to check */
    if ( pScanMngr->currentBSSBand >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngr_startContScan. pScanMngr->currentBSSBand=%d exceeds the limit %d\n",
                   pScanMngr->currentBSSBand, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( currentBSSBand >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngr_startContScan. currentBSSBand=%d exceeds the limit %d\n",
                   currentBSSBand, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* if continuous scan is already running, it means we get a start command w/o stop */
    if ( TI_TRUE == pScanMngr->bContinuousScanStarted )
    {
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Start continuous scan requested when continuous scan is running.\n");
        return;
    }

    /* mark that continuous scan was started */
    pScanMngr->bContinuousScanStarted = TI_TRUE;
    
    /* before reading and marking the new BSS - make sure that the old one is marked as NOT DISCOVERED */
    currentBSSNeighborIndex = scanMngrGetNeighborAPIndex( hScanMngr, pScanMngr->currentBSSBand, &(pScanMngr->currentBSS));
    if (( -1 != currentBSSNeighborIndex ) && ( currentBSSNeighborIndex < MAX_NUM_OF_NEIGHBOR_APS ))
    {
        pScanMngr->neighborAPsDiscoveryList[ pScanMngr->currentBSSBand ].trackStatusList[ currentBSSNeighborIndex ] = 
            SCAN_NDS_NOT_DISCOVERED;
    }

    /* Now copy current BSS - to be used when setting neighbor APs */
    pScanMngr->currentBSSBand = currentBSSBand;
    MAC_COPY (pScanMngr->currentBSS, *currentBSS);

    /* if current BSS is in the neighbor AP list, mark it as current BSS */
    currentBSSNeighborIndex = scanMngrGetNeighborAPIndex( hScanMngr, currentBSSBand, currentBSS );
    if (( -1 != currentBSSNeighborIndex ) && ( currentBSSNeighborIndex < MAX_NUM_OF_NEIGHBOR_APS ))
    {
        pScanMngr->neighborAPsDiscoveryList[ currentBSSBand ].trackStatusList[ currentBSSNeighborIndex ] = 
            SCAN_NDS_CURRENT_AP;
    }

    /* reset discovery cycle */
    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_2_4_GHZ ] = 0;
    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_5_0_GHZ ] = 0;
    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_2_4_GHZ ] = 0;
    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_5_0_GHZ ] = 0;
    pScanMngr->currentDiscoveryPart = SCAN_SDP_NEIGHBOR_G;
    scanMngrSetNextDiscoveryPart( hScanMngr );

    /* clear the BSS tracking list */
    pScanMngr->BSSList.numOfEntries = 0;

    /* start timer (if timeout is configured) */
    if ( ((TI_TRUE == pScanMngr->bLowQuality) && (0 < pScanMngr->scanPolicy.normalScanInterval)) ||
         ((TI_FALSE == pScanMngr->bLowQuality) && (0 < pScanMngr->scanPolicy.deterioratingScanInterval)))
    {
        TI_UINT32 uTimeout = pScanMngr->bLowQuality ?
                             pScanMngr->scanPolicy.deterioratingScanInterval :
                             pScanMngr->scanPolicy.normalScanInterval;

        pScanMngr->bTimerRunning = TI_TRUE;

        tmr_StartTimer (pScanMngr->hContinuousScanTimer,
                        scanMngr_GetUpdatedTsfDtimMibForScan,
                        (TI_HANDLE)pScanMngr,
                        uTimeout,
                        TI_TRUE);
    }
}

void scanMngr_stopContScan( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TI_UINT8 i;

    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_stopContScan called, hScanMngr=0x%x, state =%d\n", hScanMngr, pScanMngr->contScanState);
        
    /* if continuous scan is not running, it means we get a stop command w/o start */
    if ( TI_FALSE == pScanMngr->bContinuousScanStarted )
    {
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Stop continuous scan when continuous scan is not running.\n");
        return;
    }

    /* mark that continuous scan is not running */
    pScanMngr->bContinuousScanStarted = TI_FALSE;

    /* stop timer */
    if ( TI_TRUE == pScanMngr->bTimerRunning )
    {
        tmr_StopTimer (pScanMngr->hContinuousScanTimer);
        pScanMngr->bTimerRunning = TI_FALSE;
    }

    /* if continuous scan is currently running */
    if ( (SCAN_CSS_IDLE != pScanMngr->contScanState) &&
         (SCAN_CSS_STOPPING != pScanMngr->contScanState))
    {
        /* send a stop scan command to the scan concentartor */
        scanCncn_StopScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_CONT );
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_stopContScan called, switched to STOPPING state \n");

#ifdef TI_DBG
        switch ( pScanMngr->contScanState )
        {
        case SCAN_CSS_TRACKING_G_BAND:
            pScanMngr->stats.TrackingGByStatus[ SCAN_CRS_SCAN_STOPPED ]++;
            break;

        case SCAN_CSS_TRACKING_A_BAND:
            pScanMngr->stats.TrackingAByStatus[ SCAN_CRS_SCAN_STOPPED ]++;
            break;

        case SCAN_CSS_DISCOVERING:
            if ( RADIO_BAND_2_4_GHZ == pScanMngr->statsLastDiscoveryBand )
            {
                pScanMngr->stats.DiscoveryGByStatus[ SCAN_CRS_SCAN_STOPPED ]++;
            }
            else
            {
                pScanMngr->stats.DiscoveryAByStatus[ SCAN_CRS_SCAN_STOPPED ]++;
            }
            break;

        default:
            break;
        }
#endif
        /* mark that continuous scan is stopping */
        pScanMngr->contScanState = SCAN_CSS_STOPPING;
    }

    /* clear current neighbor APs */
    pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_2_4_GHZ ].numOfEntries = 0;
    pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_5_0_GHZ ].numOfEntries = 0;

    /* clear current BSS field .This is for the case that scanMngr_setNeighborAPs() is called before scanMngr_startcontScan() */
    for ( i = 0; i < MAC_ADDR_LEN; i++ )
    {
        pScanMngr->currentBSS[ i ] = 0xff;
    }

}

bssList_t* scanMngr_getBSSList( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TI_UINT8 BSSIndex;
    paramInfo_t param;

    
    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_getBSSList called, hScanMngr=0x%x.\n", hScanMngr);
    /* It looks like it never happens. Anyway decided to check */
    if (pScanMngr->BSSList.numOfEntries > MAX_SIZE_OF_BSS_TRACK_LIST)
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                 "scanMngr_getBSSList problem. BSSList.numOfEntries=%d exceeds the limit %d\n",
                 pScanMngr->BSSList.numOfEntries, MAX_SIZE_OF_BSS_TRACK_LIST);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        /* Returning here a NULL pointer can cause problems because the calling procedures
         use the returned pointer without checking it for correctness. */
        pScanMngr->BSSList.numOfEntries = MAX_SIZE_OF_BSS_TRACK_LIST;
    }
    /* loop on all BSS'es */    
    for ( BSSIndex = 0; BSSIndex < pScanMngr->BSSList.numOfEntries; )
    {
        /* verify channel validity with the reg domain - for active scan! 
           (because connection will be attempted on the channel... */
        param.paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
        param.content.channelCapabilityReq.band = pScanMngr->BSSList.BSSList[ BSSIndex ].band;
        param.content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
        param.content.channelCapabilityReq.channelNum = pScanMngr->BSSList.BSSList[ BSSIndex ].channel;
        regulatoryDomain_getParam( pScanMngr->hRegulatoryDomain, &param );

        /* if channel is not valid */
        if ( !param.content.channelCapabilityRet.channelValidity )
        {
            /* will replace this entry with one further down the array, if any. Therefore, index is not increased
               (because a new entry will be placed in the same index). If this is the last entry - the number of
               BSSes will be decreased, and thus the loop will exit */
            scanMngrRemoveBSSListEntry( hScanMngr, BSSIndex );
        }
        else
        {
            BSSIndex++;
        }
    }

    /* return the BSS list */
    return (bssList_t*)&(pScanMngr->BSSList);
}

void scanMngr_setNeighborAPs( TI_HANDLE hScanMngr, neighborAPList_t* neighborAPList )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int neighborAPIndex, currentBSSNeighborIndex;

    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_setNeighborAPs called, hScanMngr=0x%x.\n", hScanMngr);
#ifdef TI_DBG
    scanMngrTracePrintNeighborAPsList( hScanMngr, neighborAPList );
#endif
    /* if continuous scan is running, indicate that it shouldn't proceed to next scan (if any) */
    if ( pScanMngr->contScanState != SCAN_CSS_IDLE )
    {
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_setNeighborAPs called, switched to STOPPING state \n");

        pScanMngr->contScanState = SCAN_CSS_STOPPING;
    }
    /* It looks like it never happens. Anyway decided to check */
    if ( neighborAPList->numOfEntries > MAX_NUM_OF_NEIGHBOR_APS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                "scanMngr_setNeighborAPs. neighborAPList->numOfEntries=%d exceeds the limit %d\n",
                neighborAPList->numOfEntries, MAX_NUM_OF_NEIGHBOR_APS);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* clear current neighbor APs */
    pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_2_4_GHZ ].numOfEntries = 0;
    pScanMngr->neighborAPsDiscoveryList[ RADIO_BAND_5_0_GHZ ].numOfEntries = 0;

    /* copy new neighbor APs, according to band */
    for ( neighborAPIndex = 0; neighborAPIndex < neighborAPList->numOfEntries; neighborAPIndex++ )
    {
        if ( neighborAPList->APListPtr[ neighborAPIndex ].band >= RADIO_BAND_NUM_OF_BANDS )
        {
           TRACE3( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
                   "scanMngr_setNeighborAPs. neighborAPList->APListPtr[ %d ].band=%d exceeds the limit %d\n",
                   neighborAPIndex, neighborAPList->APListPtr[ neighborAPIndex ].band, RADIO_BAND_NUM_OF_BANDS-1);
           handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
           return;
        }
        /* insert to appropriate list */
        os_memoryCopy( pScanMngr->hOS,
                       &(pScanMngr->neighborAPsDiscoveryList[ neighborAPList->APListPtr[ neighborAPIndex ].band ].APListPtr[ pScanMngr->neighborAPsDiscoveryList[ neighborAPList->APListPtr[ neighborAPIndex ].band  ].numOfEntries ]),
                       &(neighborAPList->APListPtr[ neighborAPIndex ]),
                       sizeof(neighborAP_t));

        /* if AP is in track list, mark as discovered. This is done only if continuous scan
           has already started, to ensure the roaming canidate list holds valid information */
        if ( TI_TRUE == pScanMngr->bContinuousScanStarted )
        {
        pScanMngr->neighborAPsDiscoveryList[ neighborAPList->APListPtr[ neighborAPIndex ].band  ].trackStatusList[ pScanMngr->neighborAPsDiscoveryList[ neighborAPList->APListPtr[ neighborAPIndex ].band  ].numOfEntries ] =
            ( -1 == scanMngrGetTrackIndexByBssid( hScanMngr, &(neighborAPList->APListPtr[ neighborAPIndex ].BSSID)) ? 
              SCAN_NDS_NOT_DISCOVERED : 
              SCAN_NDS_DISCOVERED );
        }
        else
        {
            /* if continuous scan has not yet started, all AP's are yet to be discovered... */
            pScanMngr->neighborAPsDiscoveryList[ neighborAPList->APListPtr[ neighborAPIndex ].band  ].trackStatusList[ pScanMngr->neighborAPsDiscoveryList[ neighborAPList->APListPtr[ neighborAPIndex ].band  ].numOfEntries ] =
                SCAN_NDS_NOT_DISCOVERED;
        }

        /* increase neighbor AP count */
        pScanMngr->neighborAPsDiscoveryList[ neighborAPList->APListPtr[ neighborAPIndex ].band  ].numOfEntries++;
    }
    
    /* remove all tracked APs that are designated as neighbor APs, but are not anymore. Policy has not
       changed, so there's no need to check APs that are not neighbor APs and were inserted to the BSS
       list because they are on a policy defined channel. */
    scanMngrUpdateBSSList( hScanMngr, TI_TRUE, TI_FALSE );

    /* if current BSS is a neighbor AP, mark it */
    currentBSSNeighborIndex = scanMngrGetNeighborAPIndex( hScanMngr, 
                                                          pScanMngr->currentBSSBand, 
                                                          &(pScanMngr->currentBSS));
    if ( -1 != currentBSSNeighborIndex )
    {
        pScanMngr->neighborAPsDiscoveryList[ pScanMngr->currentBSSBand ].trackStatusList[ currentBSSNeighborIndex ] = 
            SCAN_NDS_CURRENT_AP;
    }

    /* reset discovery counters */
    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_2_4_GHZ ] = 0;
    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_5_0_GHZ ] = 0;
    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_2_4_GHZ ] = 0;
    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_5_0_GHZ ] = 0;
    /* set current discovery part to first part (G neighbor APs) */
    pScanMngr->currentDiscoveryPart = SCAN_SDP_NEIGHBOR_G;
    /* now advance discovery part */
    scanMngrSetNextDiscoveryPart( hScanMngr );
}

void scanMngr_qualityChangeTrigger( TI_HANDLE hScanMngr, TI_BOOL bLowQuality )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    TRACE1( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_qualityChangeTrigger called, hScanMngr=0x%x, bLowQuality=.\n", hScanMngr);

    /* remember the low quality trigger (in case policy changes, to know which timer interval to use) */
    pScanMngr->bLowQuality = bLowQuality;

    /* This function shouldn't be called when continuous scan is not running */
    if ( TI_FALSE == pScanMngr->bContinuousScanStarted )
    {
        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_WARNING, "Quality change trigger when continuous scan is not running.\n");
    }

    /* If the timer is running, stop it and start it again with the new interval */
    if (pScanMngr->bTimerRunning)
    {
        TI_UINT32 uTimeout = pScanMngr->bLowQuality ?
                             pScanMngr->scanPolicy.deterioratingScanInterval :
                             pScanMngr->scanPolicy.normalScanInterval;

        tmr_StopTimer (pScanMngr->hContinuousScanTimer);

        tmr_StartTimer (pScanMngr->hContinuousScanTimer,
                        scanMngr_GetUpdatedTsfDtimMibForScan,
                        (TI_HANDLE)pScanMngr,
                        uTimeout,
                        TI_TRUE);
    }
}

void scanMngr_handoverDone( TI_HANDLE hScanMngr, TMacAddr* macAddress, ERadioBand band )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i, currentBSSNeighborIndex;

    /* mark that TSF values are not synchronized */
    pScanMngr->bSynchronized = TI_FALSE;

    TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_handoverDone called\n");
    /* It looks like it never happens. Anyway decided to check */
    if ( pScanMngr->currentBSSBand >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngr_handoverDone. pScanMngr->currentBSSBand=%d exceeds the limit %d\n",
                   pScanMngr->currentBSSBand, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( pScanMngr->neighborAPsDiscoveryList[ pScanMngr->currentBSSBand ].numOfEntries > MAX_NUM_OF_NEIGHBOR_APS)
    {
        TRACE3( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngr_handoverDone. pScanMngr->neighborAPsDiscoveryList[ %d ].numOfEntries=%d exceeds the limit %d\n",
               pScanMngr->currentBSSBand, pScanMngr->neighborAPsDiscoveryList[ pScanMngr->currentBSSBand ].numOfEntries,
               MAX_NUM_OF_NEIGHBOR_APS);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( band >= RADIO_BAND_NUM_OF_BANDS )
    {
        TRACE2( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngr_handoverDone. band=%d exceeds the limit %d\n",
                   band, RADIO_BAND_NUM_OF_BANDS-1);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    if ( pScanMngr->neighborAPsDiscoveryList[ band ].numOfEntries > MAX_NUM_OF_NEIGHBOR_APS)
    {
        TRACE3( pScanMngr->hReport, REPORT_SEVERITY_ERROR,
               "scanMngr_handoverDone. pScanMngr->neighborAPsDiscoveryList[ %d ].numOfEntries=%d exceeds the limit %d\n",
               band, pScanMngr->neighborAPsDiscoveryList[ band ].numOfEntries, MAX_NUM_OF_NEIGHBOR_APS);
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    /* if previous AP is in neighbor AP list, mark it as not discoverd */
    currentBSSNeighborIndex = scanMngrGetNeighborAPIndex( hScanMngr,
                                                          pScanMngr->currentBSSBand,
                                                          &(pScanMngr->currentBSS));
    if ( -1 != currentBSSNeighborIndex )
    {
        pScanMngr->neighborAPsDiscoveryList[ pScanMngr->currentBSSBand ].trackStatusList[ currentBSSNeighborIndex ] = 
            SCAN_NDS_NOT_DISCOVERED;
    }

    /* copy new current AP info */
    pScanMngr->currentBSSBand = band;
    MAC_COPY (pScanMngr->currentBSS, *macAddress);

    /* if new current AP is a neighbor AP, mark it */
    currentBSSNeighborIndex = scanMngrGetNeighborAPIndex( hScanMngr, band, macAddress );
    if ( -1 != currentBSSNeighborIndex )
    {
        pScanMngr->neighborAPsDiscoveryList[ band ].trackStatusList[ currentBSSNeighborIndex ] = 
            SCAN_NDS_CURRENT_AP;
        /* note - no need to update discovery index - when adding neighbor APs the check (whether discovery should
           be attempted) is done for every channel! */
    }

    /* if a continuous scan is running, mark that it should stop */
    if ( SCAN_CSS_IDLE != pScanMngr->contScanState )
    {

        TRACE0( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_handoverDone called, switched to STOPPING state \n");

        pScanMngr->contScanState = SCAN_CSS_STOPPING;
        scanCncn_StopScan( pScanMngr->hScanCncn, SCAN_SCC_ROAMING_CONT );
    }

    /* if the new AP is in the track list */
    i = scanMngrGetTrackIndexByBssid( hScanMngr, macAddress );
    if (( i != -1 ) && ( i < MAX_SIZE_OF_BSS_TRACK_LIST))
    {
        /* remove it */
        scanMngrRemoveBSSListEntry( hScanMngr, i );
    }
}

TI_STATUS scanMngr_getParam( TI_HANDLE hScanMngr, paramInfo_t *pParam )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    TRACE2( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_getParam called, hScanMngr=0x%x, pParam=0x%x\n", hScanMngr, pParam);

    /* act according to parameter type */
    switch ( pParam->paramType )
    {
    case SCAN_MNGR_BSS_LIST_GET:
        os_memoryCopy(pScanMngr->hOS, pParam->content.pScanBssList, scanMngr_getBSSList( hScanMngr ), sizeof(bssList_t));
        break;

    default:
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_ERROR, "Scan manager getParam called with param type %d.\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
/*        break; - unreachable */
    }

    return TI_OK;
}






TI_STATUS scanMngr_setParam( TI_HANDLE hScanMngr, paramInfo_t *pParam )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    TRACE3( pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_setParam called, hScanMngr=0x%x, pParam=0x%x, pParam->paramType=%d\n", hScanMngr, pParam, pParam->paramType);
    
    /* act according to parameter type */
    switch ( pParam->paramType )
    {
    case SCAN_MNGR_SET_CONFIGURATION:
        scanMngr_setScanPolicy( hScanMngr, pParam->content.pScanPolicy);
        break;
        
    default:
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_ERROR, "Set param, Params is not supported:%d\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}


/**
 * \fn     scanMngr_SetDefaults
 * \brief  Set default values to the Scan Manager
 *
 * \param  hScanMngr - handle to the SME object
 * \param  pInitParams - values read from registry / ini file
 * \return None
 */
void scanMngr_SetDefaults (TI_HANDLE hScanMngr, TRoamScanMngrInitParams *pInitParams)
{
	scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TScanPolicy    defaultScanPolicy;
    paramInfo_t    *pParam;
    int i;

    WLAN_OS_REPORT(("pInitParams->RoamingScanning_2_4G_enable %d \n",pInitParams->RoamingScanning_2_4G_enable ));

    pParam = os_memoryAlloc(pScanMngr->hOS, sizeof(paramInfo_t));
    if (!pParam)
    {
        return;
    }

    if (pInitParams->RoamingScanning_2_4G_enable)
    {
        /* Configure default scan policy for 2.4G  */
        defaultScanPolicy.normalScanInterval = 10000;
        defaultScanPolicy.deterioratingScanInterval = 5000;
        defaultScanPolicy.maxTrackFailures = 3;
        defaultScanPolicy.BSSListSize = 4;
        defaultScanPolicy.BSSNumberToStartDiscovery = 1;
        defaultScanPolicy.numOfBands = 1;

        defaultScanPolicy.bandScanPolicy[0].band = RADIO_BAND_2_4_GHZ;
        defaultScanPolicy.bandScanPolicy[0].rxRSSIThreshold = -80;
        defaultScanPolicy.bandScanPolicy[0].numOfChannlesForDiscovery = 3;
        defaultScanPolicy.bandScanPolicy[0].numOfChannles = 14;

        for ( i = 0; i < 14; i++ )
        {
            defaultScanPolicy.bandScanPolicy[0].channelList[ i ] = i + 1;
        }

        defaultScanPolicy.bandScanPolicy[0].trackingMethod.scanType = SCAN_TYPE_NO_SCAN;
        defaultScanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
        defaultScanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
        defaultScanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.maxChannelDwellTime = 0;
        defaultScanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.minChannelDwellTime = 0;

        defaultScanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.bitrate = (ERateMask)RATE_MASK_UNSPECIFIED; /* Let the FW select */
        defaultScanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 0;
        defaultScanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.txPowerDbm = 0;

        defaultScanPolicy.bandScanPolicy[0].discoveryMethod.scanType = SCAN_TYPE_NO_SCAN;
        defaultScanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
        defaultScanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
        defaultScanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.maxChannelDwellTime = 0;
        defaultScanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.minChannelDwellTime = 0;
        defaultScanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.bitrate = (ERateMask)RATE_MASK_UNSPECIFIED; /* Let the FW select */
        defaultScanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 0;
        defaultScanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.txPowerDbm = 0;

        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.scanType = SCAN_TYPE_NORMAL_ACTIVE;
        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.method.basicMethodParams.maxChannelDwellTime = 30000;
        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.method.basicMethodParams.minChannelDwellTime = 15000;
        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 3;
        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.method.basicMethodParams.probReqParams.bitrate = (ERateMask)4;//RATE_MASK_UNSPECIFIED; /* Let the FW select */
        defaultScanPolicy.bandScanPolicy[0].immediateScanMethod.method.basicMethodParams.probReqParams.txPowerDbm = MAX_TX_POWER;

        pParam->paramType = SCAN_MNGR_SET_CONFIGURATION;

        /* scanMngr_setParam() copy the content and not the pointer */
        pParam->content.pScanPolicy = &defaultScanPolicy;
        pParam->paramLength = sizeof(TScanPolicy);

        scanMngr_setParam (hScanMngr, pParam);
    }

    os_memoryFree(pScanMngr->hOS, pParam, sizeof(paramInfo_t));
}
/**
*
* scanMngr_startManual API
*
* Description:
*
* save the manual scan params later to be used upon the scan concentrator object
* and change the conn status to connected  
*
* ARGS:
*  hScanMngr - Scan manager handle \n
*
* RETURNS:
*  void
*/
void scanMngr_startManual(TI_HANDLE hScanMngr)
{
	scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    pScanMngr->scanningOperationalMode = SCANNING_OPERATIONAL_MODE_MANUAL;
    pScanMngr->connStatus = CONNECTION_STATUS_CONNECTED;

    scanMngr_setManualScanDefaultParams(hScanMngr);
    TRACE0(pScanMngr->hReport,REPORT_SEVERITY_INFORMATION, "scanMngr_startManual() called. \n");

    /* get policies by band */
    scanMngrGetPolicyByBand( hScanMngr, RADIO_BAND_2_4_GHZ ); /* TODO: check if neccessary!!!*/
}

/**
*
* scanMngr_stopManual API
*
* Description:
*
* set the connection status to NOT_CONNECTED
*
* ARGS:
*  hScanMngr - Scan manager handle \n
*  pTargetAp - the target AP to connect with info.
*
* RETURNS:
*  void
*/
void scanMngr_stopManual(TI_HANDLE hScanMngr)
{
	scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    pScanMngr->connStatus = CONNECTION_STATUS_NOT_CONNECTED;
}

/**
*
* scanMngr_setManualScanChannelList API
*
* Description:
*
* save the channel list received form the application.
*
* ARGS:
*  hScanMngr - Scan manager handle \n
*  pTargetAp - the target AP to connect with info.
*
* RETURNS:
*  TI_OK
*/
TI_STATUS scanMngr_setManualScanChannelList (TI_HANDLE  hScanMngr, channelList_t* pChannelList)
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    pScanMngr->manualScanParams.numOfChannels = pChannelList->numOfChannels;
    os_memoryCopy(pScanMngr->hOS,
                  (void*)&pScanMngr->manualScanParams.channelEntry[0],
                  &pChannelList->channelEntry[0],
                  pChannelList->numOfChannels * sizeof(TScanChannelEntry));

    return TI_OK;
}

/**
*
* scanMngr_Start1ShotScan API
*
* Description:
*
* send the required scan params to the scan concentartor module
* according to the scanning manual mode.
*
* ARGS:
*  hScanMngr - scan manager handle \n
*  eClient - the client that requests this scan command.
*
* RETURNS:
*  EScanCncnResultStatus - the scan concentrator result
*/
EScanCncnResultStatus scanMngr_Start1ShotScan (TI_HANDLE hScanMngr, EScanCncnClient eClient)
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    TScanParams* pScanParams;
    EScanCncnResultStatus status;

    TRACE2(pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_Start1ShotScan started... .Operational mode: %d, ScanClient=%d. \n",
                    pScanMngr->scanningOperationalMode, eClient);

    if(SCANNING_OPERATIONAL_MODE_AUTO == pScanMngr->scanningOperationalMode)
    {
        pScanParams = &(pScanMngr->scanParams);
    }
    else
    {
        pScanParams = &(pScanMngr->manualScanParams);  /* the scan params that were previously saved in the scanMngr_startManual()*/
    }

    status = scanCncn_Start1ShotScan(pScanMngr->hScanCncn, eClient, pScanParams);
    return status;
}

/**
*
* scanMngr_immediateScanComplete API
*
* Description:
*
* called upon the immediate scan complete (manual or auto),
  and call the roaming manager to handle this callback.
*
* ARGS:
*  hScanMngr - Scan manager handle \n
*  scanCmpltStatus - the scan complete status
*
* RETURNS:
*  EScanCncnResultStatus - the scan concentrator result
*/
TI_STATUS scanMngr_immediateScanComplete(TI_HANDLE hScanMngr, scan_mngrResultStatus_e scanCmpltStatus)
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    if(SCANNING_OPERATIONAL_MODE_AUTO == pScanMngr->scanningOperationalMode)
    {
        roamingMngr_immediateScanComplete(pScanMngr->hRoamingMngr, scanCmpltStatus);
    }
    else
    {
        scanMngr_reportImmediateScanResults(hScanMngr, SCAN_MRS_SCAN_COMPLETE_OK);
        roamingMngr_immediateScanByAppComplete(pScanMngr->hRoamingMngr, scanCmpltStatus);
    }
    return TI_OK;
}


/**
*
* scanMngr_reportImmediateScanResults API
*
* Description:
*
* report the immediate scan results to the application
*
* ARGS:
*  hScanMngr - Scan manager handle \n
*  scanCmpltStatus - the scan complete status
*
* RETURNS:
*  EScanCncnResultStatus - the scan concentrator result
*/
TI_STATUS scanMngr_reportImmediateScanResults(TI_HANDLE hScanMngr, scan_mngrResultStatus_e scanCmpltStatus)
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    bssList_t   *pListOfAPs;


    if (scanCmpltStatus == SCAN_MRS_SCAN_COMPLETE_OK)
    {
        TRACE0(pScanMngr->hReport, REPORT_SEVERITY_INFORMATION ,"scanMngr_reportImmediateScanResults(): reporting scan results to App \n");
        pListOfAPs  = scanMngr_getBSSList(hScanMngr);
        EvHandlerSendEvent(pScanMngr->hEvHandler, IPC_EVENT_IMMEDIATE_SCAN_REPORT, (TI_UINT8*)pListOfAPs, sizeof(bssList_t));
    }
    else
    {
        TRACE1(pScanMngr->hReport, REPORT_SEVERITY_ERROR, "scanMngr_reportImmediateScanResults was not completed successfully. status: %d\n", scanCmpltStatus);
        return TI_NOK;
    }

    return TI_OK;
}


/**
*
* scanMngr_startContinuousScanByApp API
*
* Description:
*
* start continuous scan by application
*
* ARGS:
*  hScanMngr - Scan manager handle \n
*  pChannelList - the channel list to scan
*
* RETURNS:
*  TI_OK - if connected, if not returns TI_NOK
*/
TI_STATUS scanMngr_startContinuousScanByApp (TI_HANDLE hScanMngr, channelList_t* pChannelList)
{
    scanMngr_t* 	pScanMngr = (scanMngr_t*)hScanMngr;
    bssEntry_t      *pCurBssEntry;

    scanMngr_setManualScanDefaultParams(hScanMngr);

    TRACE1(pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_startContinuousScanByApp().pScanMngr->connStatus =  %d \n", pScanMngr->connStatus);

    if (CONN_STATUS_CONNECTED == pScanMngr->connStatus)
    {
        scanMngr_setManualScanChannelList(hScanMngr,pChannelList);
        pCurBssEntry = apConn_getBSSParams(pScanMngr->hAPConnection);
        scanMngr_startContScan(hScanMngr, &pCurBssEntry->BSSID, pCurBssEntry->band);
    }
    else
    {
        TRACE1( pScanMngr->hReport, REPORT_SEVERITY_ERROR, "scanMngr_startContinuousScanByApp failed. connection status %d\n", pScanMngr->connStatus);
        return TI_NOK;
    }

    return TI_OK;
}

/**
*
* scanMngr_stopContinuousScanByApp API
*
* Description:
*
* stop the continuous scan already started by and reoprt to application
*
* ARGS:
*  hScanMngr - Scan manager handle \n
*
* RETURNS:
*  TI_OK - always
*/
TI_STATUS scanMngr_stopContinuousScanByApp (TI_HANDLE hScanMngr)
{
    scanMngr_t* 	pScanMngr = (scanMngr_t*)hScanMngr;

    TRACE0(pScanMngr->hReport, REPORT_SEVERITY_INFORMATION, "scanMngr_stopContinuousScanByApp(). call scanMngr_stopContScan() \n");
    scanMngr_stopContScan(hScanMngr);
    scanMngr_reportContinuousScanResults(hScanMngr,SCAN_CRS_SCAN_COMPLETE_OK);
    return TI_OK;
}





#ifdef TI_DBG
/**
 * \\n
 * \date 26-May-2005\n
 * \brief Print scan manager statistics.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngr_statsPrint( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    WLAN_OS_REPORT(("-------------- Scan Manager Statistics ---------------\n"));
    WLAN_OS_REPORT(("Discovery scans on G result histogram:\n"));
    scanMngrStatsPrintScanResultHistogram( pScanMngr->stats.DiscoveryGByStatus );
    WLAN_OS_REPORT(("\nDiscovery scans on A result histogram:\n"));
    scanMngrStatsPrintScanResultHistogram( pScanMngr->stats.DiscoveryAByStatus );
    WLAN_OS_REPORT(("\nTracking scans on G result histogram:\n"));
    scanMngrStatsPrintScanResultHistogram( pScanMngr->stats.TrackingGByStatus );    
    WLAN_OS_REPORT(("\nTracking scans on A result histogram:\n"));
    scanMngrStatsPrintScanResultHistogram( pScanMngr->stats.TrackingAByStatus );    
    WLAN_OS_REPORT(("\nImmediate scans on G result histogram:\n"));
    scanMngrStatsPrintScanResultHistogram( pScanMngr->stats.ImmediateGByStatus );    
    WLAN_OS_REPORT(("\nImmediate scans on A result histogram:\n"));
    scanMngrStatsPrintScanResultHistogram( pScanMngr->stats.ImmediateAByStatus );
    WLAN_OS_REPORT(("\nTrack fail count histogram:\n"));
    scanMngrStatsPrintTrackFailHistogrsm( pScanMngr->stats.ConsecutiveTrackFailCountHistogram );
    WLAN_OS_REPORT(("Frames received:%d, frames discarded low RSSI:%d, frames discarded other:%d\n",
                    pScanMngr->stats.receivedFrames, pScanMngr->stats.discardedFramesLowRSSI, 
                    pScanMngr->stats.discardedFramesOther));
    WLAN_OS_REPORT(("\nSPS channels not attened histogram:\n"));
    scanMngrStatsPrintSPSChannelsHistogram( pScanMngr->stats.SPSChannelsNotAttended );
    WLAN_OS_REPORT(("\nSPS attempts changed due to DTIM collision:%d, APs removed due to DTIM overlap: %d\n",
                    pScanMngr->stats.SPSSavedByDTIMCheck, pScanMngr->stats.APsRemovedDTIMOverlap));
    WLAN_OS_REPORT(("APs removed due to invalid channel: %d\n", pScanMngr->stats.APsRemovedInvalidChannel));
}

/**
 * \\n
 * \date 26-May-2005\n
 * \brief Print scan result histogram statistics.\n
 *
 * Function Scope \e Private.\n
 * \param scanResultHistogram - Scan results histogram (by scan complete reason).\n
 */
void scanMngrStatsPrintScanResultHistogram( TI_UINT32 scanResultHistogram[] )
{
    WLAN_OS_REPORT(("Complete TI_OK   failed    stopped    TSF error    FW reset   aborted\n"));
    WLAN_OS_REPORT(("%-6d        %-5d     %-5d      %-5d        %-5d      %-5d\n",
                    scanResultHistogram[ SCAN_CRS_SCAN_COMPLETE_OK ],
                    scanResultHistogram[ SCAN_CRS_SCAN_FAILED ],
                    scanResultHistogram[ SCAN_CRS_SCAN_STOPPED ],
                    scanResultHistogram[ SCAN_CRS_TSF_ERROR ],
                    scanResultHistogram[ SCAN_CRS_SCAN_ABORTED_FW_RESET ],
                    scanResultHistogram[ SCAN_CRS_SCAN_ABORTED_HIGHER_PRIORITY ]));    
}

/**
 * \\n
 * \date 26-May-2005\n
 * \brief Print track fail count histogram statistics.\n
 *
 * Function Scope \e Private.\n
 * \param trackFailHistogram - tracking failure histogram (by tracking retry).\n
 */
void scanMngrStatsPrintTrackFailHistogrsm( TI_UINT32 trackFailHistogram[] )
{
    WLAN_OS_REPORT(("Attempts: 0      1      2      3      4\n"));
    WLAN_OS_REPORT(("          %-6d %-6d %-6d %-6d %-6d\n\n",
                    trackFailHistogram[0], trackFailHistogram[1],trackFailHistogram[2],
                    trackFailHistogram[3], trackFailHistogram[4]));
    WLAN_OS_REPORT(("Attempts: 5      6      7      8      9 or more\n"));
    WLAN_OS_REPORT(("          %-6d %-6d %-6d %-6d %-6d\n\n",
                    trackFailHistogram[5], trackFailHistogram[6],trackFailHistogram[7],
                    trackFailHistogram[8],trackFailHistogram[9]));
}

/**
 * \\n
 * \date 24-July-2005\n
 * \brief Print SPS attendant channel histogram statistics.\n
 *
 * Function Scope \e Private.\n
 * \param SPSChannelsNotAttendedHistogram - SPS channels attendant histogram.\n
 */
void scanMngrStatsPrintSPSChannelsHistogram( TI_UINT32 SPSChannelsNotAttendedHistogram[] )
{
    WLAN_OS_REPORT(("Channel index: 0      1      2      3\n"));
    WLAN_OS_REPORT(("               %-6d %-6d %-6d %-6d\n\n",
                    SPSChannelsNotAttendedHistogram[ 0 ], SPSChannelsNotAttendedHistogram[ 1 ],
                    SPSChannelsNotAttendedHistogram[ 2 ], SPSChannelsNotAttendedHistogram[ 3 ]));
    WLAN_OS_REPORT(("Channel index: 4      5      6      7\n"));
    WLAN_OS_REPORT(("               %-6d %-6d %-6d %-6d\n\n",
                    SPSChannelsNotAttendedHistogram[ 4 ], SPSChannelsNotAttendedHistogram[ 5 ],
                    SPSChannelsNotAttendedHistogram[ 6 ], SPSChannelsNotAttendedHistogram[ 7 ]));
    WLAN_OS_REPORT(("Channel index: 8      9      10     11\n"));
    WLAN_OS_REPORT(("               %-6d %-6d %-6d %-6d\n\n",
                    SPSChannelsNotAttendedHistogram[ 8 ], SPSChannelsNotAttendedHistogram[ 9 ],
                    SPSChannelsNotAttendedHistogram[ 10 ], SPSChannelsNotAttendedHistogram[ 11 ]));
    WLAN_OS_REPORT(("Channel index: 12     13     14     15\n"));
    WLAN_OS_REPORT(("               %-6d %-6d %-6d %-6d\n\n",
                    SPSChannelsNotAttendedHistogram[ 12 ], SPSChannelsNotAttendedHistogram[ 13 ],
                    SPSChannelsNotAttendedHistogram[ 14 ], SPSChannelsNotAttendedHistogram[ 15 ]));
}

/**
 * \\n
 * \date 26-May-2005\n
 * \brief Reset scan manager statistics.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngr_statsReset( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    os_memoryZero( pScanMngr->hOS, &(pScanMngr->stats), sizeof(scan_mngrStat_t));
}

/**
 * \\n
 * \date 25-July-2005\n
 * \brief Print Neighbor AP list.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - Handle to the scan manager object.\n
 */
void scanMngrDebugPrintNeighborAPList( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;
    int i,j;
    
    WLAN_OS_REPORT(("-------------- Scan Manager Neighbor APs List ---------------\n"));
    for ( i = 0; i < RADIO_BAND_NUM_OF_BANDS; i++ )
    {
        WLAN_OS_REPORT(("Neighbor AP list for band:%d\n", i));
        if ( 0 == pScanMngr->neighborAPsDiscoveryList[ i ].numOfEntries )
        {
            WLAN_OS_REPORT(("Neighbor AP list is empty.\n"));
            continue; /* to next band */
        }
        WLAN_OS_REPORT(("%-17s %-4s %-7s %-30s\n", "BSSID", "Band", "Channel", "Discovery state"));
        WLAN_OS_REPORT(("------------------------------------------------------\n"));
        for ( j = 0; j < pScanMngr->neighborAPsDiscoveryList[ i ].numOfEntries; j++ )
        {
            scanMngrDebugPrintNeighborAP( &(pScanMngr->neighborAPsDiscoveryList[ i ].APListPtr[ j ]),
                                          pScanMngr->neighborAPsDiscoveryList[ i ].trackStatusList[ j ] );
        }
    }
}

/**
 * \\n
 * \date 25-July-2005\n
 * \brief Print One neighbor AP entry.\n
 *
 * Function Scope \e Private.\n
 * \param pNeighborAp - pointer to the neighbor AP data.\n
 * \param discovery state - the discovery state of this neighbor AP.\n
 */
void scanMngrDebugPrintNeighborAP( neighborAP_t* pNeighborAp, scan_neighborDiscoveryState_e discoveryState )
{
    WLAN_OS_REPORT(("%02x:%02x:%02x:%02x:%02x:%02x %-4d %-7d %-30s\n", 
                    pNeighborAp->BSSID[ 0 ], pNeighborAp->BSSID[ 1 ], pNeighborAp->BSSID[ 2 ],
                    pNeighborAp->BSSID[ 3 ], pNeighborAp->BSSID[ 4 ], pNeighborAp->BSSID[ 5 ],
                    pNeighborAp->band, pNeighborAp->channel, neighborDiscovreyStateDesc[ discoveryState ]));
}

/**
 * \\n
 * \date 27-July-2005\n
 * \brief Prints a scan command.\n
 *
 * Function Scope \e Private.\n
 * \param pScanParams - a pointer to the scan parameters structure.\n
 */
void scanMngrDebugPrintScanCommand( TScanParams* pScanParams )
{
    int i;

    if ( 0 == pScanParams->numOfChannels )
    {
        WLAN_OS_REPORT(("Invalid scan command.\n"));
        return;
    }
    /* It looks like it never happens. Anyway decided to check */
    if ( pScanParams->numOfChannels > SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND)
    {
        WLAN_OS_REPORT(("scanMngrDebugPrintScanCommand. pScanParams->numOfChannels=%d exceeds the limit %d\n",
                pScanParams->numOfChannels, SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND));
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        return;
    }
    WLAN_OS_REPORT(("Scan type: %s, band: %d\n", scanTypeDesc[ pScanParams->scanType ], pScanParams->band));
    
    switch (pScanParams->scanType)
    {
    case SCAN_TYPE_NORMAL_ACTIVE:
        WLAN_OS_REPORT(("Probe request number:%d, rate:%x, TX level:%d\n",
                        pScanParams->probeReqNumber, pScanParams->probeRequestRate));
        /* break is missing on purpose!!! */

    case SCAN_TYPE_NORMAL_PASSIVE:
        WLAN_OS_REPORT(("SSID: %s\n", pScanParams->desiredSsid));
        WLAN_OS_REPORT(("%-4s %-17s %-17s %-20s %-8s %-14s %-14s\n", "Chnl", "BSSID", "Early ter. event", 
                        "Early ter. frame num", "TX level", "Max dwell time", "Min dwell time"));
        WLAN_OS_REPORT(("------------------------------------------------------------------------------------------------------\n"));
        for ( i = 0; i < pScanParams->numOfChannels; i++ )
        {
            scanMngrDebugPrintNormalChannelParam( &(pScanParams->channelEntry[ i ].normalChannelEntry));
        }
        break;

    case SCAN_TYPE_TRIGGERED_ACTIVE:
        WLAN_OS_REPORT(("Probe request number:%d, rate:%x, TX level:%d\n",
                        pScanParams->probeReqNumber, pScanParams->probeRequestRate ));
        /* break is missing on purpose!!! */

    case SCAN_TYPE_TRIGGERED_PASSIVE:
        WLAN_OS_REPORT(("SSID: %s, Tid: %d\n", pScanParams->desiredSsid, pScanParams->Tid));
        WLAN_OS_REPORT(("%-4s %-17s %-17s %-20s %-8s %-14s %-14s\n", "Chnl", "BSSID", "Early ter. event", 
                        "Early ter. frame num", "TX level", "Max dwell time", " Min dwell time"));
        WLAN_OS_REPORT(("------------------------------------------------------------------------------------------------------\n"));
        for ( i = 0; i < pScanParams->numOfChannels; i++ )
        {
            scanMngrDebugPrintNormalChannelParam( &(pScanParams->channelEntry[ i ].normalChannelEntry));
        }
        break;

    case SCAN_TYPE_SPS:
        WLAN_OS_REPORT(("Total scan duration (for scan timer): %d, latest TSF value: %x-%x\n", 
                        pScanParams->SPSScanDuration, 
                        INT64_HIGHER(pScanParams->latestTSFValue), INT64_LOWER(pScanParams->latestTSFValue)));
        WLAN_OS_REPORT(("%-4s %-17s %-17s %-7s %-16s %-20s\n", "Chnl", "BSSID", "Start time (TSF)", "Duration",
                        "Early ter. event", "Early ter. frame num"));
        WLAN_OS_REPORT(("---------------------------------------------------------------------------------------\n"));
        for ( i = 0; i < pScanParams->numOfChannels; i++ )
        {
            scanMngrDebugPrintSPSChannelParam( &(pScanParams->channelEntry[ i ].SPSChannelEntry));
        }
        break;

    case SCAN_TYPE_NO_SCAN:
    default:
        WLAN_OS_REPORT(("Invalid scan type: %d\n", pScanParams->scanType));
        break;
    }
    
}

/**
 * \\n
 * \date 27-July-2005\n
 * \brief Prints scan command single normal channel.\n
 *
 * Function Scope \e Private.\n
 * \param pNormalChannel - a pointer to the normal channel to print.\n
 */
void scanMngrDebugPrintNormalChannelParam( TScanNormalChannelEntry* pNormalChannel )
{
    WLAN_OS_REPORT(("%-4d %02x:%02x:%02x:%02x:%02x:%02x %-17s %-20d %-8d %-14d %-14d\n", pNormalChannel->channel, 
                    pNormalChannel->bssId[ 0 ], pNormalChannel->bssId[ 1 ], pNormalChannel->bssId[ 2 ],
                    pNormalChannel->bssId[ 3 ], pNormalChannel->bssId[ 4 ], pNormalChannel->bssId[ 5 ],
                    earlyTerminationDesc[ pNormalChannel->earlyTerminationEvent >> 8 ],
                    pNormalChannel->ETMaxNumOfAPframes, pNormalChannel->txPowerDbm,
                    pNormalChannel->minChannelDwellTime, pNormalChannel->maxChannelDwellTime));
}

/**
 * \\n
 * \date 27-July-2005\n
 * \brief Prints scan command single SPS channel.\n
 *
 * Function Scope \e Private.\n
 * \param pSPSChannel - a pointer to the SPS channel to print.\n
 */
void scanMngrDebugPrintSPSChannelParam( TScanSpsChannelEntry* pSPSChannel )
{
    WLAN_OS_REPORT(("%-4d %02x:%02x:%02x:%02x:%02x:%02x %8x-%8x %-7d %-16s %-3d\n", 
                    pSPSChannel->channel, pSPSChannel->bssId[ 0 ], pSPSChannel->bssId[ 1 ], 
                    pSPSChannel->bssId[ 2 ], pSPSChannel->bssId[ 3 ], pSPSChannel->bssId[ 4 ],
                    pSPSChannel->bssId[ 5 ], INT64_HIGHER(pSPSChannel->scanStartTime), 
                    INT64_LOWER(pSPSChannel->scanStartTime), pSPSChannel->scanDuration, 
                    earlyTerminationDesc[ pSPSChannel->earlyTerminationEvent >> 8 ], pSPSChannel->ETMaxNumOfAPframes));
}

/**
 * \\n
 * \date 25-July-2005\n
 * \brief Prints all data in the scan manager object.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrDebugPrintObject( TI_HANDLE hScanMngr )
{
    scanMngr_t* pScanMngr = (scanMngr_t*)hScanMngr;

    WLAN_OS_REPORT(("-------------- Scan Manager Object Dump ---------------\n"));
    WLAN_OS_REPORT(("Continuous scan timer running: %s, Continuous scan started:%s\n",
                    booleanDesc[ pScanMngr->bTimerRunning ], booleanDesc[ pScanMngr->bContinuousScanStarted ]));
    WLAN_OS_REPORT(("Current BSS in low quality: %s, AP TSF synchronized: %s\n",
                    booleanDesc[ pScanMngr->bLowQuality ], booleanDesc[ pScanMngr->bSynchronized ]));
    WLAN_OS_REPORT(("Continuous scan state: %s, Immediate scan state: %s\n",
                    contScanStatesDesc[ pScanMngr->contScanState ], immedScanStatesDesc[ pScanMngr->immedScanState ]));
    WLAN_OS_REPORT(("Discovery part: %s, G channels discovery Index: %d, A channels discovery index: %d\n",
                    discoveryPartDesc[ pScanMngr->currentDiscoveryPart ], 
                    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_2_4_GHZ ],
                    pScanMngr->channelDiscoveryIndex[ RADIO_BAND_5_0_GHZ ]));
    WLAN_OS_REPORT(("G neighbor APs discovery index: %d, A neighbor APs discovery index: %d\n",
                    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_2_4_GHZ ],
                    pScanMngr->neighborAPsDiscoveryIndex[ RADIO_BAND_5_0_GHZ ]));
    WLAN_OS_REPORT(("Current BSS MAC: %02x:%02x:%02x:%02x:%02x:%02x, Current BSS band: %d\n",
                    pScanMngr->currentBSS[ 0 ], pScanMngr->currentBSS[ 1 ], pScanMngr->currentBSS[ 2 ],
                    pScanMngr->currentBSS[ 3 ], pScanMngr->currentBSS[ 4 ], pScanMngr->currentBSS[ 5 ],
                    pScanMngr->currentBSSBand));
    WLAN_OS_REPORT(("Last beacon DTIM count:%d, TSF:%x-%x\n",
                    pScanMngr->lastLocalBcnDTIMCount, 
                    INT64_HIGHER(pScanMngr->currentTSF), INT64_LOWER(pScanMngr->currentTSF)));
    WLAN_OS_REPORT(("-------------- Scan Manager Policy ---------------\n"));
    scanMngrTracePrintScanPolicy( &(pScanMngr->scanPolicy));
    WLAN_OS_REPORT(("-------------- Scan Manager BSS List ---------------\n"));
    scanMngrDebugPrintBSSList( hScanMngr );
    scanMngrDebugPrintNeighborAPList( hScanMngr );
    scanMngr_statsPrint( hScanMngr );
    WLAN_OS_REPORT(("New BSS found during last discovery:%s, Number of scan cycles during which no new AP was found: %d\n",
                    booleanDesc[ pScanMngr->bNewBSSFound ], pScanMngr->consecNotFound));
    WLAN_OS_REPORT(("Scan for neighbor APs only at last immediate scan: %s\n", 
                    booleanDesc[ pScanMngr->bImmedNeighborAPsOnly ]));
    WLAN_OS_REPORT(("-------------- Last issued scan command ---------------\n"));
    scanMngrDebugPrintScanCommand( &(pScanMngr->scanParams));
    WLAN_OS_REPORT(("-------------- Handles ---------------\n"));
    WLAN_OS_REPORT(("Continuous scan timer: %x, OS:% x, Reg. domain: %x\n",
                    pScanMngr->hContinuousScanTimer, pScanMngr->hOS, pScanMngr->hRegulatoryDomain));
    WLAN_OS_REPORT(("Report: %x, Roaming manager: %x, Scan concentrator: %x\n",
                    pScanMngr->hReport, pScanMngr->hRoamingMngr, pScanMngr->hScanCncn));
}

#endif /* TI_DBG */
