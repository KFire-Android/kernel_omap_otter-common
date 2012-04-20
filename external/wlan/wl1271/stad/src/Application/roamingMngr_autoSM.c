/*
 * roamingMngr_autoSM.c
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

/** \file roamingMngr_autoSM.c
 *  \brief Roaming Manager
 *
 *  \see roamingMngr_autoSM.h
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Roaming Manager                                               *
 *   PURPOSE:                                                               *
 *   Roaming manager is responsible to receive Roaming triggers and try
 *      to select a better AP.
 *      The Roaming triggers are: Low RSSI, PER, consecutive No ACK on TX,
 *      beacon Missed or External request.
 *      In each Internal Roaming request, scan is performed and selection for 
 *      better AP. Better AP is defined as a different AP with better RSSI,
 *      and similar SSID and security settings.
 *      If better AP is found, there is a check for fast-roaming via the
 *      Supplicant. Then connection to the new AP is invoked.
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_135
#include "osApi.h"

#include "paramOut.h"
#include "report.h"
#include "scanMngrApi.h"
#include "roamingMngrApi.h"
#include "apConnApi.h"
#include "roamingMngrTypes.h"
#include "bssTypes.h"
#include "DrvMainModules.h"
#include "TWDriver.h"
#include "siteMgrApi.h"
#include "GenSM.h"
#include "roamingMngr_autoSM.h"


/*****************************************************************************
**         Private Function section                                      **
*****************************************************************************/
/* SM functions */

static void roamingMngr_smStartIdle(TI_HANDLE hRoamingMngr);
static void roamingMngr_smRoamTrigger(TI_HANDLE hRoamingMngr);
static void roamingMngr_smInvokeScan(TI_HANDLE hRoamingMngr);
static void roamingMngr_smSelection(TI_HANDLE hRoamingMngr);
static void roamingMngr_smHandover(TI_HANDLE hRoamingMngr);
static void roamingMngr_smSuccHandover(TI_HANDLE hRoamingMngr);
static void roamingMngr_smFailHandover(TI_HANDLE hRoamingMngr);
static void roamingMngr_smScanFailure(TI_HANDLE hRoamingMngr);
static void roamingMngr_smDisconnectWhileConnecting(TI_HANDLE hRoamingMngr);

/*static TI_STATUS roamingMngr_smUnexpected(TI_HANDLE hRoamingMngr);
static TI_STATUS roamingMngr_smNop(TI_HANDLE hRoamingMngr);
static TI_STATUS roamingMngr_smStopWhileScanning(TI_HANDLE hRoamingMngr);
*/

typedef enum 
{
	REASSOC_RESP_SUCCESS =0,
	REASSOC_RESP_FAILURE,
    REASSOC_RESP_REJECT
} reassociationResp_e;


/*-----------*/
/* Constants */
/*-----------*/

TGenSM_actionCell roamingMngrAuto_matrix[ROAMING_MNGR_NUM_STATES][ROAMING_MNGR_NUM_EVENTS] =
{
    /* next state and actions for IDLE state */
        {   {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smStartIdle},        /* START            */
            {ROAMING_STATE_IDLE, roamingMngr_smNop},                        /* STOP             */ 
            {ROAMING_STATE_IDLE, roamingMngr_smNop},                        /* ROAM_TRIGGER     */
            {ROAMING_STATE_IDLE, roamingMngr_smUnexpected},                 /* SCAN             */
            {ROAMING_STATE_IDLE, roamingMngr_smUnexpected},                 /* SELECT           */
            {ROAMING_STATE_IDLE, roamingMngr_smUnexpected},                 /* REQ_HANDOVER     */
            {ROAMING_STATE_IDLE, roamingMngr_smUnexpected},                 /* ROAM_SUCCESS     */
            {ROAMING_STATE_IDLE, roamingMngr_smUnexpected}                  /* FAILURE          */
        },                                                                              

        /* next state and actions for WAIT_4_TRIGGER state */    
        {   {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smUnexpected},   /* START            */ 
            {ROAMING_STATE_IDLE, roamingMngr_smStop},                   /* STOP             */ 
            {ROAMING_STATE_WAIT_4_CMD, roamingMngr_smRoamTrigger},      /* ROAM_TRIGGER     */ 
            {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smUnexpected},   /* SCAN             */ 
            {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smUnexpected},   /* SELECT           */ 
            {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smUnexpected},   /* REQ_HANDOVER     */ 
            {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smUnexpected},   /* ROAM_SUCCESS     */ 
            {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smUnexpected}    /* FAILURE          */
        },                                                                              
    
        /* next state and actions for WAIT_4_CMD state */    
        {   {ROAMING_STATE_WAIT_4_CMD, roamingMngr_smUnexpected},               /* START            */ 
            {ROAMING_STATE_WAIT_4_CMD, roamingMngr_smUnexpected},               /* STOP             */ 
            {ROAMING_STATE_WAIT_4_CMD, roamingMngr_smUnexpected},               /* ROAM_TRIGGER     */ 
            {ROAMING_STATE_SCANNING, roamingMngr_smInvokeScan},                 /* SCAN             */ 
            {ROAMING_STATE_SELECTING, roamingMngr_smSelection},                 /* SELECT           */ 
            {ROAMING_STATE_WAIT_4_CMD, roamingMngr_smUnexpected},               /* REQ_HANDOVER     */ 
            {ROAMING_STATE_WAIT_4_CMD, roamingMngr_smUnexpected},               /* ROAM_SUCCESS     */ 
            {ROAMING_STATE_WAIT_4_CMD, roamingMngr_smUnexpected}                /* FAILURE          */ 
        },                                                                              

        /* next state and actions for SCANNING state */                      
        {   {ROAMING_STATE_SCANNING, roamingMngr_smUnexpected},              /* START            */
            {ROAMING_STATE_IDLE, roamingMngr_smStopWhileScanning},           /* STOP             */
            {ROAMING_STATE_SCANNING, roamingMngr_smNop},                     /* ROAM_TRIGGER     */
            {ROAMING_STATE_SCANNING, roamingMngr_smInvokeScan},              /* SCAN             */
            {ROAMING_STATE_SELECTING, roamingMngr_smSelection},              /* SELECT           */
            {ROAMING_STATE_SCANNING, roamingMngr_smUnexpected},              /* REQ_HANDOVER     */
            {ROAMING_STATE_SCANNING, roamingMngr_smUnexpected},              /* ROAM_SUCCESS     */
            {ROAMING_STATE_IDLE, roamingMngr_smScanFailure}                  /* FAILURE          */
                                                                                   
        }, 

        /* next state and actions for SELECTING state */                      
        {   {ROAMING_STATE_SELECTING, roamingMngr_smUnexpected},             /* START            */
            {ROAMING_STATE_SELECTING, roamingMngr_smUnexpected},             /* STOP             */
            {ROAMING_STATE_SELECTING, roamingMngr_smUnexpected},             /* ROAM_TRIGGER     */
            {ROAMING_STATE_SELECTING, roamingMngr_smUnexpected},             /* SCAN             */
            {ROAMING_STATE_SELECTING, roamingMngr_smUnexpected},             /* SELECT           */
            {ROAMING_STATE_CONNECTING, roamingMngr_smHandover},              /* REQ_HANDOVER     */
            {ROAMING_STATE_SELECTING, roamingMngr_smUnexpected},             /* ROAM_SUCCESS     */
            {ROAMING_STATE_SELECTING, roamingMngr_smUnexpected}              /* FAILURE          */
                                                                                   
        }, 

        /* next state and actions for CONNECTING state */                      
        {   {ROAMING_STATE_CONNECTING, roamingMngr_smUnexpected},           /* START            */ 
            {ROAMING_STATE_IDLE, roamingMngr_smStop},                       /* STOP             */ 
            {ROAMING_STATE_IDLE, roamingMngr_smDisconnectWhileConnecting},      /* ROAM_TRIGGER     */ 
            {ROAMING_STATE_CONNECTING, roamingMngr_smUnexpected},           /* SCAN,            */ 
            {ROAMING_STATE_CONNECTING, roamingMngr_smUnexpected},           /* SELECT           */ 
            {ROAMING_STATE_CONNECTING, roamingMngr_smHandover},             /* REQ_HANDOVER     */ 
            {ROAMING_STATE_WAIT_4_TRIGGER, roamingMngr_smSuccHandover} ,    /* ROAM_SUCCESS     */ 
            {ROAMING_STATE_IDLE, roamingMngr_smFailHandover}                /* FAILURE          */ 
                                                                                   
        } 
}; 


TI_INT8*  AutoRoamStateDescription[] = 
{
    "IDLE",
    "WAIT_4_TRIGGER",
    "WAIT_4_CMD",
    "SCANNING",
    "SELECTING",
    "CONNECTING"
};

TI_INT8*  AutoRoamEventDescription[] = 
{
    "START",
    "STOP",
    "ROAM_TRIGGER",
    "SCAN",
    "SELECT",
    "REQ_HANDOVER",
    "ROAM_SUCCESS",
    "FAILURE"
};

/**
*
* roamingMngr_smRoamTrigger 
*
* \b Description: 
*
* This procedure is called when an Roaming event occurs: BSS LOSS, LOW Quality etc.
 * Performs the following:
 * - If Roaming is disabled, ignore.
 * - Indicate Driver that Roaming process is starting
 * - Get the BSS list from the Scan Manager.
 * - If the list is not empty, start SELECTION
 * - If the list is empty, start SCANNING. The type of scan is decided
 *      according to the Neigbor APs existence.
*
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smRoamTrigger(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t           *pRoamingMngr;
    roamingMngr_smEvents    roamingEvent;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smRoamTrigger, enableDisable = %d\n",pRoamingMngr->roamingMngrConfig.enableDisable);

    if (!pRoamingMngr->roamingMngrConfig.enableDisable)
    {   
		/* Ignore any other Roaming event when Roaming is disabled */
        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_smRoamTrigger, when Roaming is disabled\n");
        return;
    }
    /* Indicate the driver that Roaming process is starting */
    apConn_prepareToRoaming(pRoamingMngr->hAPConnection, pRoamingMngr->roamingTrigger);

    /* Get the current BSSIDs from ScanMngr */
    pRoamingMngr->pListOfAPs = scanMngr_getBSSList(pRoamingMngr->hScanMngr);
    if ((pRoamingMngr->pListOfAPs != NULL) && (pRoamingMngr->pListOfAPs->numOfEntries > 0))
    {   /* No need to SCAN, start SELECTING */
        roamingEvent = ROAMING_EVENT_SELECT;
    } 
    else
    {   /* check if list of APs exists in order to verify which scan to start */
        roamingEvent = ROAMING_EVENT_SCAN;
        if (pRoamingMngr->neighborApsExist)
        {   /* Scan only Neighbor APs */
            pRoamingMngr->scanType = ROAMING_PARTIAL_SCAN;
        }
        else
        {   /* Scan all channels */
            pRoamingMngr->scanType = ROAMING_FULL_SCAN;
        }
    }
    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smRoamTrigger, scanType = %d\n", pRoamingMngr->scanType);

    roamingMngr_smEvent(roamingEvent, pRoamingMngr);
}

/**
*
* roamingMngr_smInvokeScan 
*
* \b Description: 
*
* This procedure is called when scan should be performed in order
 * to select an AP to roam to.
 * This can be the first scan, a second scan after partail scan,
 * or scan after previous scan was failed.
 * In any case, the scan can either be:
 *  partail, on list of channles or
 *  full on all channels.
*
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smInvokeScan(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t       *pRoamingMngr;
    scan_mngrResultStatus_e     scanResult;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;

    /* check which scan should be performed: Partial on list of channels, or full scan */
    if ((pRoamingMngr->scanType == ROAMING_PARTIAL_SCAN) ||
        (pRoamingMngr->scanType == ROAMING_PARTIAL_SCAN_RETRY))
    {
        scanResult = scanMngr_startImmediateScan (pRoamingMngr->hScanMngr, TI_TRUE);
    }
    else
    {    /* Scan all channels */
        scanResult = scanMngr_startImmediateScan (pRoamingMngr->hScanMngr, TI_FALSE);
    }
   
    if (scanResult != SCAN_MRS_SCAN_RUNNING)
    {   /* the scan failed, immitate scan complete event */
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smInvokeScan, scanResult = %d\n", scanResult);
        roamingMngr_immediateScanComplete(pRoamingMngr, scanResult);
    }
}

/**
*
* roamingMngr_smSelection 
*
* \b Description: 
*
* This procedure is called when selection should be performed.
*   It perform the following:
 * Prepare the candidate APs to roam according to:
 *  - Priority APs
 *  - Pre-Authenticated APs
 * If the candidate AP list is empty, only the current AP can be re-selected
 * Select one AP and trigger REQ_HANDOVER event.
 * 
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smSelection(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t               *pRoamingMngr;
    TI_UINT32                      index;


    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    pRoamingMngr->listOfCandidateAps.numOfNeighborBSS = 0;
    pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS = 0;
    pRoamingMngr->listOfCandidateAps.numOfRegularBSS = 0;

    pRoamingMngr->candidateApIndex = INVALID_CANDIDATE_INDEX;

    if ((pRoamingMngr->pListOfAPs == NULL) || 
        (pRoamingMngr->pListOfAPs->numOfEntries == 0))
    {   /* Error, there cannot be selection  */
        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smSelection pListOfAPs is empty \n");
        roamingMngr_smEvent(ROAMING_EVENT_REQ_HANDOVER, pRoamingMngr);
        return;
    }

    /* Build the candidate AP list */
    for (index=0; index<pRoamingMngr->pListOfAPs->numOfEntries; index++ )
    {
        if ( (pRoamingMngr->roamingTrigger <= ROAMING_TRIGGER_LOW_QUALITY_GROUP) &&
            (pRoamingMngr->pListOfAPs->BSSList[index].RSSI < pRoamingMngr->roamingMngrConfig.apQualityThreshold))
        {   /* Do not insert APs with low quality to the selection table, 
                if the Roaming Trigger was low Quality */
            TRACE8(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "candidate AP %x-%x-%x-%x-%x-%x with RSSI too low =%d, Quality=%d  \n", pRoamingMngr->pListOfAPs->BSSList[index].BSSID[0], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[1], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[2], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[3], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[4], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[5], pRoamingMngr->pListOfAPs->BSSList[index].RSSI, pRoamingMngr->roamingMngrConfig.apQualityThreshold);

            continue;
        }

        if (apConn_isSiteBanned(pRoamingMngr->hAPConnection, &pRoamingMngr->pListOfAPs->BSSList[index].BSSID) == TI_TRUE)
        {
            TRACE6(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, ": Candidate AP %02X-%02X-%02X-%02X-%02X-%02X is banned!\n",									 pRoamingMngr->pListOfAPs->BSSList[index].BSSID[0],	pRoamingMngr->pListOfAPs->BSSList[index].BSSID[1], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[2], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[3], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[4], pRoamingMngr->pListOfAPs->BSSList[index].BSSID[5]);
            continue;
        }

        if (pRoamingMngr->pListOfAPs->BSSList[index].bNeighborAP)
        {   /* The AP is a neighbor AP, insert its index to the neighbor APs list */
            pRoamingMngr->listOfCandidateAps.neighborBSSList[pRoamingMngr->listOfCandidateAps.numOfNeighborBSS] = index; 
            pRoamingMngr->listOfCandidateAps.numOfNeighborBSS++;
        }
        else if (apConn_getPreAuthAPStatus(pRoamingMngr->hAPConnection, 
										   &pRoamingMngr->pListOfAPs->BSSList[index].BSSID))
        {   /* This AP is a pre-auth AP */
            pRoamingMngr->listOfCandidateAps.preAuthBSSList[pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS] = index; 
            pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS++;
        }
        else
        {   /* This AP is not Neighbor nor Pre-Auth */
            pRoamingMngr->listOfCandidateAps.regularBSSList[pRoamingMngr->listOfCandidateAps.numOfRegularBSS] = index; 
            pRoamingMngr->listOfCandidateAps.numOfRegularBSS++;
        }
    }

#ifdef TI_DBG
    {   /* for debug */
        paramInfo_t     param;

        param.paramType = ROAMING_MNGR_PRINT_CANDIDATE_TABLE;
        roamingMngr_getParam(pRoamingMngr, &param);

    }
#endif
    roamingMngr_smEvent(ROAMING_EVENT_REQ_HANDOVER, pRoamingMngr);

}

/**
*
* roamingMngr_smHandover 
*
* \b Description: 
*
* This procedure is called when handover should be invoked.
*   Go over the candidate APs and start handover to each of them. 
 * If there's no candidate APs, disconnect.
 * Handover to the current AP is allowed only if the trigger is
 * low quality.
 * 
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smHandover(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t           *pRoamingMngr;
    bssEntry_t              *pApToConnect;
    apConn_connRequest_t    requestToApConn;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;

    if ((pRoamingMngr->handoverWasPerformed) && (pRoamingMngr->candidateApIndex == CURRENT_AP_INDEX))
    {   /* Handover with the current AP already failed, Disconnect */
        roamingMngr_smEvent(ROAMING_EVENT_FAILURE, pRoamingMngr);
        return;
    }
    if (pRoamingMngr->listOfCandidateAps.numOfNeighborBSS > 0)
    {   /* Neighbor APs are the highest priority to Roam */
        pRoamingMngr->candidateApIndex = 
            pRoamingMngr->listOfCandidateAps.neighborBSSList[pRoamingMngr->listOfCandidateAps.numOfNeighborBSS-1];
        pRoamingMngr->listOfCandidateAps.numOfNeighborBSS--;
    }
    else if (pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS > 0)
    {   /* Pre-Auth APs are the second priority to Roam */
        pRoamingMngr->candidateApIndex = 
            pRoamingMngr->listOfCandidateAps.preAuthBSSList[pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS-1];
        pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS--;
    }
    else if (pRoamingMngr->listOfCandidateAps.numOfRegularBSS > 0)
    {   /* Regular APs are APs that are not pre-authenticated and not Neighbor */
        pRoamingMngr->candidateApIndex = 
            pRoamingMngr->listOfCandidateAps.regularBSSList[pRoamingMngr->listOfCandidateAps.numOfRegularBSS-1];
        pRoamingMngr->listOfCandidateAps.numOfRegularBSS--;
    }
    else
    {   /* No Candidate APs */
        pRoamingMngr->candidateApIndex = INVALID_CANDIDATE_INDEX;
    }

    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smHandover, candidateApIndex=%d \n", pRoamingMngr->candidateApIndex);


    if (pRoamingMngr->candidateApIndex == INVALID_CANDIDATE_INDEX)
    {   /* No cnadidate to Roam to, only the current AP is candidate */
        if (pRoamingMngr->roamingTrigger <= ROAMING_TRIGGER_LOW_QUALITY_GROUP)
        {   /* If the trigger to Roam is low quality, and there are no candidate APs
                to roam to, retain connected to the current AP */
            requestToApConn.requestType = (pRoamingMngr->handoverWasPerformed) ? AP_CONNECT_RECONNECT_CURR_AP : AP_CONNECT_RETAIN_CURR_AP;
            pRoamingMngr->candidateApIndex = CURRENT_AP_INDEX;
        }
        else
        {   /* Disconnect the BSS, there are no more APs to roam to */
            roamingMngr_smEvent(ROAMING_EVENT_FAILURE, pRoamingMngr);
            return;
        }
    }
    else
    {   /* There is a valid candidate AP */
        if (pRoamingMngr->roamingTrigger > ROAMING_TRIGGER_FAST_CONNECT_GROUP)
        {   /* Full re-connection should be perfromed */
            requestToApConn.requestType = AP_CONNECT_FULL_TO_AP; 
        }
        else
        {   /* Fast re-connection should be perfromed */
            requestToApConn.requestType = AP_CONNECT_FAST_TO_AP; 
        }
    }
#ifdef TI_DBG
    /* For debug */
    if (!pRoamingMngr->handoverWasPerformed)
    {   /* Take the time before the first handover started */
        pRoamingMngr->roamingHandoverStartedTimestamp = os_timeStampMs(pRoamingMngr->hOs);
    }
#endif
    
    if (pRoamingMngr->candidateApIndex == CURRENT_AP_INDEX)
    {   /* get the current AP */
        pApToConnect = apConn_getBSSParams(pRoamingMngr->hAPConnection);
    }
    else
    {   /* get the candidate AP */
        pRoamingMngr->handoverWasPerformed = TI_TRUE;
        pApToConnect = &pRoamingMngr->pListOfAPs->BSSList[pRoamingMngr->candidateApIndex];
    }
    TRACE3(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smHandover, candidateApIndex=%d, requestType = %d, channel=%d \n", 							 pRoamingMngr->candidateApIndex, requestToApConn.requestType, pApToConnect->channel);

    requestToApConn.dataBufLength = 0;

#ifdef XCC_MODULE_INCLUDED
    apConn_connectToAP(pRoamingMngr->hAPConnection, pApToConnect, &requestToApConn, pRoamingMngr->bSendTspecInReassPkt);
#else
    apConn_connectToAP(pRoamingMngr->hAPConnection, pApToConnect, &requestToApConn, TI_TRUE);
#endif
}

/**
*
* roamingMngr_smDisconnectWhileConnecting 
*
* \b Description: 
*
* This procedure is called when the Station is in the process of connection,
 * and the AP disconnects the station. 
 * 
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smDisconnectWhileConnecting(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t           *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
 
    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smDisconnectWhileConnecting, candidateApIndex=%d \n", pRoamingMngr->candidateApIndex);

    if (pRoamingMngr->roamingTrigger > ROAMING_TRIGGER_FAST_CONNECT_GROUP)
    {   /* If the trigger is from the Full Connect group, then stop the connection. */
        /* clean intenal variables */
        pRoamingMngr->maskRoamingEvents = TI_TRUE;
        pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;

        scanMngr_stopContScan(pRoamingMngr->hScanMngr);
#ifdef TI_DBG
        pRoamingMngr->roamingFailedHandoverNum++;
#endif
        apConn_disconnect(pRoamingMngr->hAPConnection);
        
    }
}

/**
*
* roamingMngr_smSuccHandover 
*
* \b Description: 
*
* This procedure is called when handover succeeded.
 * Inform Scan Manager about the new AP.    
 * UnMask Roaming Triggers. 
 * 
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smSuccHandover(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t           *pRoamingMngr;
    bssEntry_t              *pNewConnectedAp;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;

    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smSuccHandover, candidateApIndex=%d \n", pRoamingMngr->candidateApIndex);

    if (pRoamingMngr->handoverWasPerformed &&
        (pRoamingMngr->pListOfAPs != NULL) &&
        (pRoamingMngr->pListOfAPs->numOfEntries>0))
    {
        if (pRoamingMngr->candidateApIndex == CURRENT_AP_INDEX)
        {   
			/* get the current AP */
            pNewConnectedAp = apConn_getBSSParams(pRoamingMngr->hAPConnection);
        }
        else
        {   
			/* get the candidate AP */
            pNewConnectedAp = &pRoamingMngr->pListOfAPs->BSSList[pRoamingMngr->candidateApIndex];
        }

        scanMngr_handoverDone(pRoamingMngr->hScanMngr, 
							  &pNewConnectedAp->BSSID,
							  pNewConnectedAp->band);
    }
    pRoamingMngr->maskRoamingEvents = TI_FALSE;
    pRoamingMngr->candidateApIndex = INVALID_CANDIDATE_INDEX;
    pRoamingMngr->handoverWasPerformed = TI_FALSE;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;

    /* Start pre-authentication in order to set PMKID
        for the current AP */
    if (pRoamingMngr->staCapabilities.authMode==os802_11AuthModeWPA2)
    {
		/* No Pre-Auth is required */
        bssList_t           *pBssList;

        pBssList = os_memoryAlloc(pRoamingMngr->hOs, sizeof(bssList_t));
        if (!pBssList)
        {
            return;
        }
        pBssList->numOfEntries = 0;
        apConn_preAuthenticate(pRoamingMngr->hAPConnection, pBssList);
        os_memoryFree(pRoamingMngr->hOs, pBssList, sizeof(bssList_t));
    }
}

/**
*
* roamingMngr_smFailHandover 
*
* \b Description: 
*
* This procedure is called when handover failed and there are no more
 * APs to roam to. Disconnect the BSS and retrun to IDLE state.
* 
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smFailHandover(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t           *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smFailHandover \n");

    /* clean intenal variables */
    pRoamingMngr->maskRoamingEvents = TI_TRUE;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;

    scanMngr_stopContScan(pRoamingMngr->hScanMngr);
#ifdef TI_DBG
    pRoamingMngr->roamingFailedHandoverNum++;
#endif
    apConn_disconnect(pRoamingMngr->hAPConnection);
}

/**
*
* roamingMngr_smScanFailure 
*
* \b Description: 
*
* This procedure is called when all scan attempts failed. 
 * Send Disconnect event and return to IDLE state.
 *
* 
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smScanFailure(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t           *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smScanFailure \n");

    /* clean intenal variables */
    pRoamingMngr->maskRoamingEvents = TI_TRUE;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;

    scanMngr_stopContScan(pRoamingMngr->hScanMngr);

    apConn_disconnect(pRoamingMngr->hAPConnection);
}

#if 0
/**
*
* roamingMngr_smCmdFailure 
*
* \b Description: 
*
* This procedure is called when all the driver failed to prepare to Roaming. 
 * Mask all future Roaming triggers.
 *
* 
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smCmdFailure(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t           *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smCmdFailure \n");

    /* clean intenal variables */
    pRoamingMngr->maskRoamingEvents = TI_TRUE;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE; 
}
#endif

/**
*
* roamingMngr_smStartIdle - Start event when in Idle state
*
* \b Description: 
*
* Start event when in Idle state. 
 * This function is called when the station becomes CONNECTED.
 * Perform the following:
 * - The current state becomes WAIT_4_TRIGGER 
 * - Unmask Roaming events
 * - Set handoverWasPerformed to TI_FALSE
 * - Start the Scan Manager
*
* \b ARGS:
*
*  I   - pData - pointer to the roamingMngr SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void roamingMngr_smStartIdle(void *pData)
{
    roamingMngr_t       *pRoamingMngr;
    bssEntry_t          *pCurBssEntry;

    pRoamingMngr = (roamingMngr_t*)pData;
    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smStartIdle, Unmask Roaming events and start continuos scan \n");

    pRoamingMngr->maskRoamingEvents = TI_FALSE;
    pRoamingMngr->handoverWasPerformed = TI_FALSE;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;

    pCurBssEntry = apConn_getBSSParams(pRoamingMngr->hAPConnection);
    scanMngr_startContScan(pRoamingMngr->hScanMngr, &pCurBssEntry->BSSID, pCurBssEntry->band);

    /* Start pre-authentication in order to set PMKID
        for the current AP */
    if (pRoamingMngr->staCapabilities.authMode==os802_11AuthModeWPA2)
    {   /* No Pre-Auth is required */
        bssList_t           *pBssList;

        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smStartIdle, Pre-Auth to cur AP\n");
        pBssList = os_memoryAlloc(pRoamingMngr->hOs, sizeof(bssList_t));
        if (!pBssList)
        {
            return;
        }

        pBssList->numOfEntries = 0;
        apConn_preAuthenticate(pRoamingMngr->hAPConnection, pBssList);
        os_memoryFree(pRoamingMngr->hOs, pBssList, sizeof(bssList_t));
    }
}

