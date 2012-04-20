/*
 * roamingMngr.c
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

/** \file roamingMngr.c
 *  \brief Roaming Manager
 *
 *  \see roamingMngrApi.h
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

#define __FILE_ID__  FILE_ID_8
#include "osApi.h"

#include "paramOut.h"
#include "report.h"
#include "fsm.h"
#include "GenSM.h"
#include "scanMngrApi.h"
#include "roamingMngrApi.h"
#include "apConnApi.h"
#include "roamingMngrTypes.h"
#include "bssTypes.h"
#include "DrvMainModules.h"
#include "TWDriver.h"
#include "siteMgrApi.h"
#include "roamingMngr_manualSM.h"
#include "roamingMngr_autoSM.h"
#include "currBss.h"
#include "currBssApi.h"
#include "EvHandler.h"

/*-----------*/
/* Constants */
/*-----------*/

/* Init bits */
#define ROAMING_MNGR_CONTEXT_INIT_BIT       1
#define ROAMING_MNGR_SM_INIT_BIT            2

#define DEFAULT_AP_QUALITY                  (-70)
#define DEFAULT_LOW_PASS_FILTER             (30)
#define DEFAULT_DATA_RETRY_THRESHOLD        (20)
#define DEFAULT_LOW_QUALITY_SCAN_COND       (-60)
#define DEFAULT_NORMAL_QUALITY_SCAN_COND    (-50)
#define DEFAULT_LOW_RSSI                    (-70)
#define DEFAULT_LOW_SNR                     (0)
#define DEFAULT_TBTT_4_BSS_LOSS             (10)
#define DEFAULT_LOW_TX_RATE                 (2)


/*--------------*/
/* Enumerations */
/*--------------*/

/*----------*/
/* Typedefs */
/*----------*/

/*------------*/
/* Structures */
/*------------*/


/************** callback funtions called by AP Connection **************/
/* called when a trigger for Roaming occurs */
TI_STATUS roamingMngr_triggerRoamingCb(TI_HANDLE hRoamingMngr, void *pData, TI_UINT16 reasonCode);
/* called when CONN status event occurs */
TI_STATUS roamingMngr_connStatusCb(TI_HANDLE hRoamingMngr, void *pData);
/* called when Neighbor APs is updated */
TI_STATUS roamingMngr_updateNeighborApListCb(TI_HANDLE hRoamingMngr, void *pData);

/* internal functions */
static void roamingMngr_releaseModule(roamingMngr_t *pRoamingMngr, TI_UINT32 initVec);

#ifdef TI_DBG
/* debug function */
static void roamingMngr_printStatistics(TI_HANDLE hRoamingMngr);
static void roamingMngr_resetStatistics(TI_HANDLE hRoamingMngr);
#endif

/**
*
* roamingMngr_releaseModule
*
* \b Description: 
*
* Called by the un load function
* Go over the vector, for each bit that is set, release the corresponding module.
*
* \b ARGS:
*
*  I   - pRoamingMngr - Roaming Manager context  \n
*  I   - initVec - indicates which modules should be released
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa roamingMngr_create
*/
static void roamingMngr_releaseModule(roamingMngr_t *pRoamingMngr, TI_UINT32 initVec)
{

    if (pRoamingMngr==NULL)
    {
        return;
    }
    if (initVec & (1 << ROAMING_MNGR_SM_INIT_BIT))
    {
        genSM_Unload(pRoamingMngr->hRoamingSm);
    }

    if (initVec & (1 << ROAMING_MNGR_CONTEXT_INIT_BIT))
    {
        os_memoryFree(pRoamingMngr->hOs, pRoamingMngr, sizeof(roamingMngr_t));
    }

    initVec = 0;
}

/**
*
* roamingMngr_triggerRoamingCb 
*
* \b Description: 
*
* This procedure is called when Roaming should be triggered
 * due to one of apConn_roamingTrigger_e Roaming Reasons.
 * Save the trigger and process it only if there's no other Roaming trigger
 * in process.
*
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*  I   - pData - pointer to roaming trigger
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
TI_STATUS roamingMngr_triggerRoamingCb(TI_HANDLE hRoamingMngr, void *pData, TI_UINT16 reasonCode)
{
    roamingMngr_t       *pRoamingMngr;
    apConn_roamingTrigger_e     roamingTrigger;
    TI_UINT32                   curTimestamp;
    TI_UINT16                   disConnReasonCode;


    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    if ((pRoamingMngr == NULL) || (pData == NULL))
    {
        return TI_NOK;
    }

    roamingTrigger = *(apConn_roamingTrigger_e *)pData;

    if ((ROAMING_OPERATIONAL_MODE_MANUAL == pRoamingMngr->RoamingOperationalMode) &&
        (roamingTrigger == ROAMING_TRIGGER_AP_DISCONNECT))
    {
        disConnReasonCode = reasonCode;
        EvHandlerSendEvent(pRoamingMngr->hEvHandler, IPC_EVENT_AP_DISCONNECT, (TI_UINT8*)&disConnReasonCode, sizeof(disConnReasonCode));
    }


    if (roamingTrigger >= ROAMING_TRIGGER_LAST)
    {
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_triggerRoamingCb, bad roaming trigger = %d\n", roamingTrigger);
        return TI_NOK;
    }
#ifdef TI_DBG
    /* save parameters for debug*/
    pRoamingMngr->roamingTriggerEvents[pRoamingMngr->roamingTrigger]++;
#endif
    if (roamingTrigger <= ROAMING_TRIGGER_BG_SCAN_GROUP)
    {
        TI_BOOL    lowQuality = TI_FALSE;
        if (roamingTrigger == ROAMING_TRIGGER_LOW_QUALITY_FOR_BG_SCAN)
        {
            lowQuality = TI_TRUE;
        }
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_triggerRoamingCb, lowQuality = %d \n", lowQuality);
        scanMngr_qualityChangeTrigger(pRoamingMngr->hScanMngr, lowQuality);
    }
    else
    {
        if (roamingTrigger > pRoamingMngr->roamingTrigger)
        {   /* Save the highest priority roaming trigger */
            pRoamingMngr->roamingTrigger = roamingTrigger;
            TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_triggerRoamingCb, higher trigger = %d \n", roamingTrigger);

        }

        curTimestamp = os_timeStampMs(pRoamingMngr->hOs);

        /* If "No BSS" trigger received, disable count of low pass filter timer */
        if (roamingTrigger > ROAMING_TRIGGER_LOW_QUALITY_GROUP)
        {
            pRoamingMngr->lowQualityTriggerTimestamp = 0;
        }

        /* Do not invoke a new Roaming Trigger when a previous one is in process */
        if (pRoamingMngr->maskRoamingEvents == TI_FALSE)
        {   /* No Roaming trigger is in process */
            /* If the trigger is low quality check the low pass filter */
            TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_triggerRoamingCb, trigger = %d \n", roamingTrigger);
            if (roamingTrigger <= ROAMING_TRIGGER_LOW_QUALITY_GROUP)
            {
                TI_UINT32 deltaTs = curTimestamp-pRoamingMngr->lowQualityTriggerTimestamp;

                if ((pRoamingMngr->lowQualityTriggerTimestamp != 0) &&
                    (deltaTs < pRoamingMngr->lowPassFilterRoamingAttemptInMsec))
                {  /* Ignore the low quality events. till the low pass time elapses */
                    TRACE5(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_triggerRoamingCb, trigger = %d Ignored!!,deltaTs=%d, curTimestamp = %d, lowQualityTriggerTimestamp = %d, lowPassFilterRoamingAttempt=%d\n", roamingTrigger, deltaTs, curTimestamp, pRoamingMngr->lowQualityTriggerTimestamp, pRoamingMngr->lowPassFilterRoamingAttemptInMsec);
                    return TI_OK;
                }
                pRoamingMngr->lowQualityTriggerTimestamp = curTimestamp;
            }

            /* Mask all future roaming events */
            pRoamingMngr->maskRoamingEvents = TI_TRUE;

#ifdef TI_DBG
            /* For debug */
            pRoamingMngr->roamingTriggerTimestamp = curTimestamp;
#endif
            return (roamingMngr_smEvent(ROAMING_EVENT_ROAM_TRIGGER, pRoamingMngr));
        }
        else if (roamingTrigger > ROAMING_TRIGGER_FAST_CONNECT_GROUP)
        {   /* If the trigger is from the Full Connect group, then stop the connection. */
            return (roamingMngr_smEvent(ROAMING_EVENT_ROAM_TRIGGER, pRoamingMngr));
            
        }
    }

    return TI_OK;
}

/**
*
* roamingMngr_connStatusCb 
*
* \b Description: 
*
* This procedure is called when the connection status event
 * is triggered.
*
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*  I   - pData - pointer to the connection status.
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
TI_STATUS roamingMngr_connStatusCb(TI_HANDLE hRoamingMngr, void *pData)
{
    roamingMngr_t               *pRoamingMngr;
    apConn_connStatus_e         connStatus;
    roamingMngr_smEvents        roamingEvent;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    if ((pRoamingMngr == NULL) || (pData == NULL))
    {
        return TI_NOK;
    }

    connStatus = ((apConn_connStatus_t *)pData)->status;
    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_connStatusCb, conn status = %d\n", connStatus);

    if (!pRoamingMngr->roamingMngrConfig.enableDisable)
    {
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_connStatusCb, connStatus=%d was received while Roaming is disabled. Stop Roaming \n", 						   connStatus);
        return TI_NOK;
    }

    if (ROAMING_OPERATIONAL_MODE_AUTO == pRoamingMngr->RoamingOperationalMode)
    {
        switch (connStatus)
        {
        case CONN_STATUS_CONNECTED: roamingEvent = ROAMING_EVENT_START;
            /* Get station capabilities */
            apConn_getStaCapabilities(pRoamingMngr->hAPConnection, &pRoamingMngr->staCapabilities); 
            break;     
        case CONN_STATUS_NOT_CONNECTED: roamingEvent = ROAMING_EVENT_STOP;
            break;
        case CONN_STATUS_HANDOVER_SUCCESS: roamingEvent = ROAMING_EVENT_ROAM_SUCCESS;
#ifdef TI_DBG
            /* For debug */
            pRoamingMngr->roamingSuccesfulHandoverNum++;
            pRoamingMngr->roamingHandoverCompletedTimestamp = os_timeStampMs(pRoamingMngr->hOs);
            pRoamingMngr->roamingAverageSuccHandoverDuration += os_timeStampMs(pRoamingMngr->hOs)-pRoamingMngr->roamingHandoverStartedTimestamp;
            pRoamingMngr->roamingAverageRoamingDuration +=  os_timeStampMs(pRoamingMngr->hOs)-pRoamingMngr->roamingTriggerTimestamp;
            pRoamingMngr->roamingHandoverEvents[pRoamingMngr->roamingTrigger]++;
#endif
            break;
        case CONN_STATUS_HANDOVER_FAILURE: roamingEvent = ROAMING_EVENT_REQ_HANDOVER;
#ifdef TI_DBG
            /* For debug */
            pRoamingMngr->roamingFailedHandoverNum++;
#endif
            break;
        default:
            TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_connStatusCb, bad connStatus = %d\n", connStatus);
            return TI_NOK;
        }
    }
    else /* Roaming Manual operational mode*/
    {
         switch (connStatus)
         {
          case CONN_STATUS_CONNECTED: 
        		roamingEvent = (roamingMngr_smEvents)ROAMING_MANUAL_EVENT_START;
                apConn_getStaCapabilities(pRoamingMngr->hAPConnection,&pRoamingMngr->staCapabilities); 
                break;     
          case CONN_STATUS_NOT_CONNECTED:
                roamingEvent = (roamingMngr_smEvents)ROAMING_MANUAL_EVENT_STOP;
                break;
          case CONN_STATUS_HANDOVER_SUCCESS:
                roamingEvent = (roamingMngr_smEvents)ROAMING_MANUAL_EVENT_SUCCESS;
                break;
          case CONN_STATUS_HANDOVER_FAILURE:
                roamingEvent = (roamingMngr_smEvents)ROAMING_MANUAL_EVENT_FAIL;
                break;
          default:
        	return TI_NOK;
        }
    }

    return (roamingMngr_smEvent(roamingEvent, pRoamingMngr));
}

/**
*
* roamingMngr_updateNeighborApListCb 
*
* \b Description: 
*
* This procedure is called when Neighbor AP list is received from the AP.
 * Save the list, and set them in Scan Manager object.
*
* \b ARGS:
*
*  I   - hRoamingMngr - roamingMngr SM context  \n
*  I   - pData - pointer to the list of Neighbor APs.
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
TI_STATUS roamingMngr_updateNeighborApListCb(TI_HANDLE hRoamingMngr, void *pData)
{
    roamingMngr_t           *pRoamingMngr;
    neighborAPList_t        *pNeighborAPList;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    if ((pRoamingMngr == NULL) || (pData == NULL))
    {
        return TI_NOK;
    }

    pNeighborAPList = (neighborAPList_t *)pData;
    if (pNeighborAPList->numOfEntries>0)
    {
        pRoamingMngr->neighborApsExist = TI_TRUE;
    }
    else
    {
        pRoamingMngr->neighborApsExist = TI_FALSE;
    }
    
    if (pRoamingMngr->roamingMngrConfig.enableDisable)
    {
        scanMngr_setNeighborAPs (pRoamingMngr->hScanMngr, pNeighborAPList);
    }
    TRACE2(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_updateNeighborApListCb, numberOfAps = %d, enableDisable=%d\n", 							 pNeighborAPList->numOfEntries, pRoamingMngr->roamingMngrConfig.enableDisable);

    return TI_OK;
}

/**
*
* roamingMngr_smEvent
*
* \b Description: 
*
* Roaming Manager state machine transition function
*
* \b ARGS:
*
*  I/O - currentState - current state in the state machine\n
*  I   - event - specific event for the state machine\n
*  I   - pData - Data for state machine action function\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS roamingMngr_smEvent(TI_UINT8 event, void* data)
{
    roamingMngr_t   *pRoamingMngr = (roamingMngr_t*)data;

    TRACE3(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smEvent(). Mode(%d) ,currentState = %d, event=%d \n",
                    pRoamingMngr->RoamingOperationalMode,
                    *(pRoamingMngr->pCurrentState),
                    event);

    genSM_Event (pRoamingMngr->hRoamingSm, (TI_UINT32)event, data);

    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_smEvent(). new State : %d \n", *(pRoamingMngr->pCurrentState));
    
    return TI_OK;
}


  
#ifdef TI_DBG
/**
*
* roamingMngr_debugTrace 
*
* \b Description: 
*
* This procedure is called for debug only, to trace the roaming triggers and events
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
static void roamingMngr_printStatistics(TI_HANDLE hRoamingMngr)
{


    roamingMngr_t       *pRoamingMngr;
    TI_UINT8               index;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    if (pRoamingMngr == NULL)
    {
        return;
    }

    WLAN_OS_REPORT(("******** ROAMING_TRIGGERS ********\n"));
    for (index=ROAMING_TRIGGER_LOW_TX_RATE; index<ROAMING_TRIGGER_LAST; index++)
    {
        switch (index)
        {
        case ROAMING_TRIGGER_LOW_TX_RATE:
            WLAN_OS_REPORT(("- Low TX rate = %d\n",     pRoamingMngr->roamingTriggerEvents[index]));
            break;
        case ROAMING_TRIGGER_LOW_SNR:
            WLAN_OS_REPORT(("- Low Snr = %d\n",         pRoamingMngr->roamingTriggerEvents[index]));
            break;
        case ROAMING_TRIGGER_LOW_QUALITY:
            WLAN_OS_REPORT(("- Low Quality = %d\n",     pRoamingMngr->roamingTriggerEvents[index]));
            break;
        case ROAMING_TRIGGER_MAX_TX_RETRIES:
            WLAN_OS_REPORT(("- MAX TX retries = %d\n",  pRoamingMngr->roamingTriggerEvents[index]));
            break;
        case ROAMING_TRIGGER_BSS_LOSS:
            WLAN_OS_REPORT(("- BSS Loss TX = %d\n",     pRoamingMngr->roamingTriggerEvents[index]));
            break;
        case ROAMING_TRIGGER_SWITCH_CHANNEL:
            WLAN_OS_REPORT(("- Switch Channel = %d\n",  pRoamingMngr->roamingTriggerEvents[index]));
            break;
        case ROAMING_TRIGGER_AP_DISCONNECT:
            WLAN_OS_REPORT(("- AP Disconnect = %d\n",   pRoamingMngr->roamingTriggerEvents[index]));
            break;
        case ROAMING_TRIGGER_SECURITY_ATTACK:
            WLAN_OS_REPORT(("- SEC attack = %d\n",      pRoamingMngr->roamingTriggerEvents[index]));
            break;  
        default:
            break;
        }
    }

    WLAN_OS_REPORT(("******** Succ ROAMING_HANDOVERS ********\n"));

    for (index=ROAMING_TRIGGER_LOW_QUALITY; index<ROAMING_TRIGGER_LAST; index++)
    {
        switch (index)
        {
        case ROAMING_TRIGGER_LOW_TX_RATE:               
            WLAN_OS_REPORT(("- Low TX rate = %d\n",     pRoamingMngr->roamingHandoverEvents[index]));
            break;                                     
        case ROAMING_TRIGGER_LOW_SNR:               
            WLAN_OS_REPORT(("- Low Snre = %d\n",        pRoamingMngr->roamingHandoverEvents[index]));
            break;                                     
        case ROAMING_TRIGGER_LOW_QUALITY:               
            WLAN_OS_REPORT(("- Low Quality = %d\n",     pRoamingMngr->roamingHandoverEvents[index]));
            break;                                     
        case ROAMING_TRIGGER_MAX_TX_RETRIES:            
            WLAN_OS_REPORT(("- MAX TX retries = %d\n",  pRoamingMngr->roamingHandoverEvents[index]));
            break;                                     
        case ROAMING_TRIGGER_BSS_LOSS:                  
            WLAN_OS_REPORT(("- BSS Loss TX = %d\n",     pRoamingMngr->roamingHandoverEvents[index]));
            break;                                     
        case ROAMING_TRIGGER_SWITCH_CHANNEL:            
            WLAN_OS_REPORT(("- Switch Channel = %d\n",   pRoamingMngr->roamingHandoverEvents[index]));
            break;                                     
        case ROAMING_TRIGGER_AP_DISCONNECT:             
            WLAN_OS_REPORT(("- AP Disconnect = %d\n",   pRoamingMngr->roamingHandoverEvents[index]));
            break;                                     
        case ROAMING_TRIGGER_SECURITY_ATTACK:           
            WLAN_OS_REPORT(("- SEC attack = %d\n",      pRoamingMngr->roamingHandoverEvents[index])); 
            break;                                     
        default:
            break;
        }
    }

    WLAN_OS_REPORT(("******** ROAMING STATISTICS ********\n"));
    WLAN_OS_REPORT(("- Num of succesful handovers = %d\n", pRoamingMngr->roamingSuccesfulHandoverNum)); 
    WLAN_OS_REPORT(("- Num of failed handovers = %d\n", pRoamingMngr->roamingFailedHandoverNum)); 
    if (pRoamingMngr->roamingSuccesfulHandoverNum >0)
    {
        WLAN_OS_REPORT(("- Succesful average succesful handover duration = %d\n", pRoamingMngr->roamingAverageSuccHandoverDuration/pRoamingMngr->roamingSuccesfulHandoverNum)); 
        WLAN_OS_REPORT(("- Succesful average roaming duration = %d\n", pRoamingMngr->roamingAverageRoamingDuration/pRoamingMngr->roamingSuccesfulHandoverNum)); 
    }


}


/**
*
* roamingMngr_resetDebugTrace 
*
* \b Description: 
*
* This procedure is called for debug only, to reset Roaming debug trace 
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
static void roamingMngr_resetStatistics(TI_HANDLE hRoamingMngr)
{

    roamingMngr_t       *pRoamingMngr;
    TI_UINT8               index;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    if (pRoamingMngr == NULL)
    {
        return;
    }
    WLAN_OS_REPORT(("Resetting all ROAMING_EVENTS \n"));

    pRoamingMngr->roamingSuccesfulHandoverNum = 0;    
    pRoamingMngr->roamingHandoverStartedTimestamp = 0;  
    pRoamingMngr->roamingHandoverCompletedTimestamp = 0;
    pRoamingMngr->roamingAverageSuccHandoverDuration = 0; 
    pRoamingMngr->roamingAverageRoamingDuration = 0;  
    pRoamingMngr->roamingFailedHandoverNum = 0;

    for (index=ROAMING_TRIGGER_LOW_QUALITY; index<ROAMING_TRIGGER_LAST; index++)
    {
        pRoamingMngr->roamingHandoverEvents[index] = 0;
        pRoamingMngr->roamingTriggerEvents[index] = 0;
    }
}

#endif /*TI_DBG*/



/**********************************************************************
**         External Function section                                 **
***********************************************************************/
extern TI_STATUS apConn_reportRoamingEvent(TI_HANDLE hAPConnection,
										   apConn_roamingTrigger_e roamingEventType,
										   void *roamingEventData);



/**********************************************************************
**         API Function section                                      **
***********************************************************************/

TI_HANDLE roamingMngr_create(TI_HANDLE hOs)
{
    roamingMngr_t   *pRoamingMngr;
    TI_UINT32          initVec;

    initVec = 0;

    pRoamingMngr = os_memoryAlloc(hOs, sizeof(roamingMngr_t));
    if (pRoamingMngr == NULL)
        return NULL;

    initVec |= (1 << ROAMING_MNGR_CONTEXT_INIT_BIT);
    pRoamingMngr->hOs   = hOs;

    /* allocate the state machine object */
    pRoamingMngr->hRoamingSm = genSM_Create(hOs);

    if (pRoamingMngr->hRoamingSm == NULL)
    {
        roamingMngr_releaseModule(pRoamingMngr, initVec);
        WLAN_OS_REPORT(("FATAL ERROR: roamingMngr_create(): Error Creating pRoamingSm - Aborting\n"));
        return NULL;
    }
    initVec |= (1 << ROAMING_MNGR_SM_INIT_BIT);

    
    return pRoamingMngr;
}

TI_STATUS roamingMngr_unload(TI_HANDLE hRoamingMngr)
{
    TI_UINT32          initVec;

    if (hRoamingMngr == NULL)
    {
        return TI_OK;
    }

    initVec = 0xFFFF;
    roamingMngr_releaseModule(hRoamingMngr, initVec);

    return TI_OK;
}

void roamingMngr_init (TStadHandlesList *pStadHandles)
{
    roamingMngr_t    *pRoamingMngr = (roamingMngr_t*)(pStadHandles->hRoamingMngr);

    /* Update handlers */
    pRoamingMngr->hReport       = pStadHandles->hReport;
    pRoamingMngr->hScanMngr     = pStadHandles->hScanMngr;
    pRoamingMngr->hAPConnection = pStadHandles->hAPConnection;
    pRoamingMngr->hTWD          = pStadHandles->hTWD;
    pRoamingMngr->hEvHandler    = pStadHandles->hEvHandler;
    pRoamingMngr->hCurrBss      = pStadHandles->hCurrBss;

    genSM_Init(pRoamingMngr->hRoamingSm,pRoamingMngr->hReport);
}


TI_STATUS roamingMngr_setDefaults (TI_HANDLE hRoamingMngr, TRoamScanMngrInitParams *pInitParam)
{

    roamingMngr_t  *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
	paramInfo_t     param;

#ifdef TI_DBG
    TI_UINT8 index =0;
#endif
     /* Init intrenal variables */
    //pRoamingMngr->currentState = ROAMING_STATE_IDLE;
    pRoamingMngr->roamingMngrConfig.enableDisable = ROAMING_DISABLED; 
    pRoamingMngr->roamingMngrConfig.apQualityThreshold = DEFAULT_AP_QUALITY;
    pRoamingMngr->roamingMngrConfig.lowPassFilterRoamingAttempt = DEFAULT_LOW_PASS_FILTER;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;
    pRoamingMngr->maskRoamingEvents= TI_TRUE;
    pRoamingMngr->scanType = ROAMING_NO_SCAN;
    pRoamingMngr->candidateApIndex = INVALID_CANDIDATE_INDEX;
    pRoamingMngr->handoverWasPerformed = TI_FALSE;
    pRoamingMngr->lowQualityTriggerTimestamp = 0;
    pRoamingMngr->neighborApsExist = TI_FALSE;
    pRoamingMngr->pListOfAPs = NULL;
    pRoamingMngr->candidateApIndex = INVALID_CANDIDATE_INDEX;
    pRoamingMngr->listOfCandidateAps.numOfNeighborBSS = 0;
    pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS = 0;
    pRoamingMngr->listOfCandidateAps.numOfRegularBSS = 0;
    pRoamingMngr->RoamingOperationalMode =  pInitParam->RoamingOperationalMode;
    pRoamingMngr->bSendTspecInReassPkt = pInitParam->bSendTspecInReassPkt;

	if (pInitParam->RoamingScanning_2_4G_enable)
    {
        param.content.roamingConfigBuffer.roamingMngrConfig.enableDisable =  ROAMING_ENABLED ;
        param.content.roamingConfigBuffer.roamingMngrConfig.lowPassFilterRoamingAttempt = 30;
        param.content.roamingConfigBuffer.roamingMngrConfig.apQualityThreshold = -70;

        param.content.roamingConfigBuffer.roamingMngrThresholdsConfig.dataRetryThreshold = 20;
        param.content.roamingConfigBuffer.roamingMngrThresholdsConfig.numExpectedTbttForBSSLoss = 10;
        param.content.roamingConfigBuffer.roamingMngrThresholdsConfig.txRateThreshold = 2;
        param.content.roamingConfigBuffer.roamingMngrThresholdsConfig.lowRssiThreshold = -80;
        param.content.roamingConfigBuffer.roamingMngrThresholdsConfig.lowSnrThreshold = 0;
        param.content.roamingConfigBuffer.roamingMngrThresholdsConfig.lowQualityForBackgroungScanCondition = -80;
        param.content.roamingConfigBuffer.roamingMngrThresholdsConfig.normalQualityForBackgroungScanCondition = -70;

        param.paramType = ROAMING_MNGR_APPLICATION_CONFIGURATION;
        param.paramLength = sizeof(roamingMngrConfigParams_t);

        roamingMngr_setParam(hRoamingMngr, &param);

    }



    /* config the FSM according to the operational mode*/
    if(ROAMING_OPERATIONAL_MODE_MANUAL==pRoamingMngr->RoamingOperationalMode)
    {
         genSM_SetDefaults(pRoamingMngr->hRoamingSm,
                           ROAMING_MANUAL_NUM_STATES,
                           ROAMING_MANUAL_NUM_EVENTS,
                           &roamingMngrManual_matrix[0][0],
                           ROAMING_MANUAL_STATE_IDLE,
                           "Roaming Manual SM",
                           ManualRoamStateDescription,
                           ManualRoamEventDescription,
                           __FILE_ID__);

         pRoamingMngr->RoamStateDescription = ManualRoamStateDescription;
         pRoamingMngr->RoamEventDescription = ManualRoamEventDescription;
    }
    else
    {
         genSM_SetDefaults(pRoamingMngr->hRoamingSm,
                           ROAMING_MNGR_NUM_STATES,
                           ROAMING_MNGR_NUM_EVENTS,
                           &roamingMngrAuto_matrix[0][0],
                           ROAMING_STATE_IDLE,
                           "Roaming Auto SM",
                           AutoRoamStateDescription,
                           AutoRoamEventDescription,
                           __FILE_ID__);

         pRoamingMngr->RoamStateDescription = AutoRoamStateDescription;
         pRoamingMngr->RoamEventDescription = AutoRoamEventDescription;
    }

    pRoamingMngr->pCurrentState = &((TGenSM*)pRoamingMngr->hRoamingSm)->uCurrentState;

#ifdef TI_DBG
    /* debug counters */
    pRoamingMngr->roamingSuccesfulHandoverNum = 0;    
    pRoamingMngr->roamingHandoverStartedTimestamp = 0;  
    pRoamingMngr->roamingHandoverCompletedTimestamp = 0;
    pRoamingMngr->roamingAverageSuccHandoverDuration = 0; 
    pRoamingMngr->roamingAverageRoamingDuration = 0;  
    pRoamingMngr->roamingFailedHandoverNum = 0;

    for (index=ROAMING_TRIGGER_NONE; index<ROAMING_TRIGGER_LAST; index++)
    {
        pRoamingMngr->roamingTriggerEvents[index] = 0;
        pRoamingMngr->roamingHandoverEvents[index] = 0;
    }
#endif

    return TI_OK;
}

TI_STATUS roamingMngr_setParam(TI_HANDLE hRoamingMngr, paramInfo_t *pParam)
{
    roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    TI_STATUS      status       = TI_OK;

    if (pParam == NULL)
    {
        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR , "roamingMngr_setParam(): pParam is NULL!\n");
        return TI_NOK;
    }

    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION , "roamingMngr_setParam   %X \n", pParam->paramType);

    switch (pParam->paramType)
    {
    
    case ROAMING_MNGR_APPLICATION_CONFIGURATION:
        {
            roamingMngrConfigParams_t   *pRoamingMngrConfigParams;
    
            pRoamingMngrConfigParams = &pParam->content.roamingConfigBuffer;
    
            /* Configure the Roaming Parmeters */
            TRACE3(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_setParam Configuration: \n                                  enableDisable= %d,\n  lowPassFilterRoamingAttempt=%d,\n                                  apQualityThreshold=%d\n", pRoamingMngrConfigParams->roamingMngrConfig.enableDisable, pRoamingMngrConfigParams->roamingMngrConfig.lowPassFilterRoamingAttempt, pRoamingMngrConfigParams->roamingMngrConfig.apQualityThreshold);

            pRoamingMngr->roamingMngrConfig.apQualityThreshold = pRoamingMngrConfigParams->roamingMngrConfig.apQualityThreshold;
            pRoamingMngr->roamingMngrConfig.lowPassFilterRoamingAttempt = pRoamingMngrConfigParams->roamingMngrConfig.lowPassFilterRoamingAttempt;
            pRoamingMngr->lowPassFilterRoamingAttemptInMsec = pRoamingMngrConfigParams->roamingMngrConfig.lowPassFilterRoamingAttempt * 1000;

            /* Configure the Roaming Trigger thresholds */
            TRACE7(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_setParam Thresholds: \n                                  dataRetryThreshold= %d,\n  lowQualityForBackgroungScanCondition=%d,\n                                  lowRssiThreshold=%d,\n lowSNRThreshold=%d,\n                                  normalQualityForBackgroungScanCondition=%d,\n                                  numExpectedTbttForBSSLoss=%d,\n txRateThreshold=%d \n \n", pRoamingMngrConfigParams->roamingMngrThresholdsConfig.dataRetryThreshold, pRoamingMngrConfigParams->roamingMngrThresholdsConfig.lowQualityForBackgroungScanCondition, pRoamingMngrConfigParams->roamingMngrThresholdsConfig.lowRssiThreshold, pRoamingMngrConfigParams->roamingMngrThresholdsConfig.lowSnrThreshold, pRoamingMngrConfigParams->roamingMngrThresholdsConfig.normalQualityForBackgroungScanCondition, pRoamingMngrConfigParams->roamingMngrThresholdsConfig.numExpectedTbttForBSSLoss, pRoamingMngrConfigParams->roamingMngrThresholdsConfig.txRateThreshold);

            os_memoryCopy(pRoamingMngr->hOs, &pRoamingMngr->roamingMngrThresholdsConfig, &pRoamingMngrConfigParams->roamingMngrThresholdsConfig, sizeof(roamingMngrThresholdsConfig_t));
            
            status = apConn_setRoamThresholds(pRoamingMngr->hAPConnection, &pRoamingMngrConfigParams->roamingMngrThresholdsConfig);

            if (pRoamingMngr->roamingMngrConfig.enableDisable && 
                !pRoamingMngrConfigParams->roamingMngrConfig.enableDisable)
            {   /* disable Roaming Manager */
                apConn_unregisterRoamMngrCallb(pRoamingMngr->hAPConnection);
                pRoamingMngr->roamingMngrConfig.enableDisable = ROAMING_DISABLED;
                return (roamingMngr_smEvent(ROAMING_EVENT_STOP, pRoamingMngr));
            }
            else if (!pRoamingMngr->roamingMngrConfig.enableDisable && 
                pRoamingMngrConfigParams->roamingMngrConfig.enableDisable)
            {   /* enable Roaming Manager */
                /* Save the Roaming Configuration parameters */
                pRoamingMngr->roamingMngrConfig.enableDisable = pRoamingMngrConfigParams->roamingMngrConfig.enableDisable;
                /* register Roaming callback */
                apConn_registerRoamMngrCallb(pRoamingMngr->hAPConnection, 
                                             roamingMngr_triggerRoamingCb,
                                             roamingMngr_connStatusCb,
                                             roamingMngr_updateNeighborApListCb);
            }
        }
        break;


    /*********** For Debug Purposes ***********/

    case ROAMING_MNGR_TRIGGER_EVENT:
        /* Enable/disable Internal Roaming */
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_setParam TRIGGER_EVENT=  %d \n", pParam->content.roamingTriggerType);
        apConn_reportRoamingEvent(pRoamingMngr->hAPConnection, (apConn_roamingTrigger_e)pParam->content.roamingTriggerType, NULL);
        break;
    
    case ROAMING_MNGR_CONN_STATUS:
        /* External request to connect to BBSID */
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_setParam CONN_STATUS=  %d \n", pParam->content.roamingConnStatus);
        roamingMngr_connStatusCb(pRoamingMngr, &pParam->content.roamingConnStatus);
        break;

    default:
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_setParam bad param=  %X\n", pParam->paramType);

        break;
    }


    return status;
}

TI_STATUS roamingMngr_getParam(TI_HANDLE hRoamingMngr, paramInfo_t *pParam)
{
    roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;

    if (pParam == NULL)
    {
        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR , "roamingMngr_getParam(): pParam is NULL!\n");
        return TI_NOK;
    }

    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_getParam   %X \n", pParam->paramType);

    switch (pParam->paramType)
    {
    case ROAMING_MNGR_APPLICATION_CONFIGURATION:
        {
            roamingMngrConfigParams_t   *pRoamingMngrConfigParams;
    
            pRoamingMngrConfigParams = &pParam->content.roamingConfigBuffer;
    
            if (pRoamingMngr->roamingMngrConfig.enableDisable == ROAMING_DISABLED) 
            {   
                pRoamingMngrConfigParams->roamingMngrConfig.enableDisable = TI_FALSE;
            }
            else 
            {
                pRoamingMngrConfigParams->roamingMngrConfig.enableDisable = TI_TRUE;
            }
            pRoamingMngrConfigParams->roamingMngrConfig.apQualityThreshold = pRoamingMngr->roamingMngrConfig.apQualityThreshold;
            pRoamingMngrConfigParams->roamingMngrConfig.lowPassFilterRoamingAttempt = pRoamingMngr->roamingMngrConfig.lowPassFilterRoamingAttempt;

            apConn_getRoamThresholds(pRoamingMngr->hAPConnection, &pRoamingMngr->roamingMngrThresholdsConfig);
            os_memoryCopy(pRoamingMngr->hOs, &pRoamingMngrConfigParams->roamingMngrThresholdsConfig, &pRoamingMngr->roamingMngrThresholdsConfig, sizeof(roamingMngrThresholdsConfig_t));
            pParam->paramLength = sizeof(roamingMngrConfigParams_t);
        }
        break;

#ifdef TI_DBG
    case ROAMING_MNGR_PRINT_STATISTICS:
        roamingMngr_printStatistics(pRoamingMngr);
        break;

    case ROAMING_MNGR_RESET_STATISTICS:
        roamingMngr_resetStatistics(pRoamingMngr);
        break;

    case ROAMING_MNGR_PRINT_CURRENT_STATUS:
        WLAN_OS_REPORT(("Roaming Current State = %d, enableDisable=%d\n, maskRoamingEvents = %d, roamingTrigger=%d \n scanType=%d, handoverWasPerformed=%d \n, candidateApIndex=%d, lowQualityTriggerTimestamp=%d \n",
                        *(pRoamingMngr->pCurrentState),
                        pRoamingMngr->roamingMngrConfig.enableDisable,
                        pRoamingMngr->maskRoamingEvents,
                        pRoamingMngr->roamingTrigger,
                        pRoamingMngr->scanType,
                        pRoamingMngr->handoverWasPerformed,
                        pRoamingMngr->candidateApIndex,
                        pRoamingMngr->lowQualityTriggerTimestamp));
        break;
    case ROAMING_MNGR_PRINT_CANDIDATE_TABLE:
        {
            TI_UINT32      index;

            if (pRoamingMngr->pListOfAPs==NULL)
            {
                TRACE0( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "Roaming Mngr the candidate AP list is invalid \n");
                break;
            }
            TRACE1( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "The number of candidates is %d\n", pRoamingMngr->pListOfAPs->numOfEntries);

            TRACE1( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "Roaming Mngr Neighbor AP list, num of candidates = %d\n", pRoamingMngr->listOfCandidateAps.numOfNeighborBSS);

            for (index=0; index<pRoamingMngr->listOfCandidateAps.numOfNeighborBSS; index++)
            {
                TI_UINT32  candidateIndex;
                bssEntry_t  *pBssEntry;

                candidateIndex = pRoamingMngr->listOfCandidateAps.neighborBSSList[index];
                pBssEntry = &pRoamingMngr->pListOfAPs->BSSList[candidateIndex];
                TRACE8( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "candiate %d, BSSID=%x-%x-%x-%x-%x-%x, RSSI =%d \n", candidateIndex, pBssEntry->BSSID[0], pBssEntry->BSSID[1], pBssEntry->BSSID[2], pBssEntry->BSSID[3], pBssEntry->BSSID[4], pBssEntry->BSSID[5], pBssEntry->RSSI);
            }
            TRACE1( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "Roaming Mngr Pre-Auth AP list, num of candidates = %d\n", pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS);

            for (index=0; index<pRoamingMngr->listOfCandidateAps.numOfPreAuthBSS; index++)
            {
                TI_UINT32  candidateIndex;
                bssEntry_t  *pBssEntry;

                candidateIndex = pRoamingMngr->listOfCandidateAps.preAuthBSSList[index];
                pBssEntry = &pRoamingMngr->pListOfAPs->BSSList[candidateIndex];
                TRACE8( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "candiate %d, BSSID=%x-%x-%x-%x-%x-%x, RSSI =%d \n", candidateIndex, pBssEntry->BSSID[0], pBssEntry->BSSID[1], pBssEntry->BSSID[2], pBssEntry->BSSID[3], pBssEntry->BSSID[4], pBssEntry->BSSID[5], pBssEntry->RSSI);
            }
            TRACE1( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "Roaming Mngr Regular AP list, num of candidates = %d\n", pRoamingMngr->listOfCandidateAps.numOfRegularBSS);

            for (index=0; index<pRoamingMngr->listOfCandidateAps.numOfRegularBSS; index++)
            {
                TI_UINT32  candidateIndex;
                bssEntry_t  *pBssEntry;

                candidateIndex = pRoamingMngr->listOfCandidateAps.regularBSSList[index];
                pBssEntry = &pRoamingMngr->pListOfAPs->BSSList[candidateIndex];
                TRACE8( pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "candiate %d, BSSID=%x-%x-%x-%x-%x-%x, RSSI =%d \n", candidateIndex, pBssEntry->BSSID[0], pBssEntry->BSSID[1], pBssEntry->BSSID[2], pBssEntry->BSSID[3], pBssEntry->BSSID[4], pBssEntry->BSSID[5], pBssEntry->RSSI);
            }
        }
        break;

#endif /*TI_DBG*/

    default:
        TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_getParam  bad paramType= %X \n", pParam->paramType);
        return TI_NOK;
    }

    return TI_OK;
}

TI_STATUS roamingMngr_immediateScanComplete(TI_HANDLE hRoamingMngr, scan_mngrResultStatus_e scanCmpltStatus)
{
    roamingMngr_t           *pRoamingMngr;
    roamingMngr_smEvents    roamingEvent;


    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    if (pRoamingMngr == NULL)
    {
        return TI_NOK;
    }

    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_immediateScanComplete, scanCmpltStatus = %d\n", 							 scanCmpltStatus);

    if (scanCmpltStatus == SCAN_MRS_SCAN_COMPLETE_OK)
    {   
		/* The scan completed TI_OK, get the updated list of APs */
        pRoamingMngr->pListOfAPs = scanMngr_getBSSList(pRoamingMngr->hScanMngr);
        if ((pRoamingMngr->pListOfAPs != NULL) && (pRoamingMngr->pListOfAPs->numOfEntries > 0))
        {   
			/* APs were found, start selection */
            pRoamingMngr->scanType = ROAMING_NO_SCAN;
            roamingEvent = ROAMING_EVENT_SELECT;
        }
        else
        {   /* There were no APs, if the scan was partial, retry full scan */
            if ((pRoamingMngr->scanType == ROAMING_PARTIAL_SCAN) ||
                (pRoamingMngr->scanType == ROAMING_PARTIAL_SCAN_RETRY))
            {
                pRoamingMngr->scanType = ROAMING_FULL_SCAN;
                roamingEvent = ROAMING_EVENT_SCAN;
            }
            else
            {   
				/* No APs were found in FULL SCAN, report failure */
                roamingEvent = ROAMING_EVENT_SELECT;
            }
        }
    }
	/* scanCmpltStatus != SCAN_MRS_SCAN_COMPLETE_OK */
    else
    {   
		/* The scan failed, retry scanning according to the current scan type */
        pRoamingMngr->pListOfAPs = scanMngr_getBSSList(pRoamingMngr->hScanMngr);
        if ((pRoamingMngr->pListOfAPs != NULL) && (pRoamingMngr->pListOfAPs->numOfEntries > 0))
        {   
			/* APs were found, start selection */
            pRoamingMngr->scanType = ROAMING_NO_SCAN;
            roamingEvent = ROAMING_EVENT_SELECT;
        }
        else
        {   
			/* The scan failed, and there were no APs found. 
			Retry scanning according to the current scan type */
			switch (pRoamingMngr->scanType)
			{
			case ROAMING_PARTIAL_SCAN:
				roamingEvent = ROAMING_EVENT_SCAN;
				pRoamingMngr->scanType = ROAMING_PARTIAL_SCAN_RETRY;
				break;
			case ROAMING_PARTIAL_SCAN_RETRY:
				roamingEvent = ROAMING_EVENT_SELECT;
				pRoamingMngr->scanType = ROAMING_NO_SCAN;
				break;
			case ROAMING_FULL_SCAN:
				roamingEvent = ROAMING_EVENT_SCAN;
				pRoamingMngr->scanType = ROAMING_FULL_SCAN_RETRY;
				break;
			case ROAMING_FULL_SCAN_RETRY:
					roamingEvent = ROAMING_EVENT_SELECT;
				pRoamingMngr->scanType = ROAMING_NO_SCAN;
				break;
			default:
				roamingEvent = ROAMING_EVENT_SELECT;
TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_immediateScanComplete, pRoamingMngr->scanType = %d\n", 								   pRoamingMngr->scanType);
				pRoamingMngr->scanType = ROAMING_NO_SCAN;       
				break;
			} /* switch (pRoamingMngr->scanType) */
        }
    }

    TRACE2(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_immediateScanComplete, roamingEvent = %d, scanType=%d\n", 							roamingEvent, 							 pRoamingMngr->scanType);

    return (roamingMngr_smEvent(roamingEvent, pRoamingMngr));
    
}

TI_STATUS roamingMngr_updateNewBssList(TI_HANDLE hRoamingMngr, bssList_t *bssList)
{

    roamingMngr_t       *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    if ((pRoamingMngr == NULL) || (bssList == NULL))
    {
        return TI_NOK;
    }

    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_updateNewBssList, number of APs = %d\n", bssList->numOfEntries);

    if (*(pRoamingMngr->pCurrentState) != ROAMING_STATE_WAIT_4_TRIGGER)
    {
        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_WARNING, "roamingMngr_updateNewBssList, ignore APs when not in WAIT_4_TRIGGER state \n");
        return TI_NOK;
    }


    if (pRoamingMngr->staCapabilities.authMode!=os802_11AuthModeWPA2)
    {   /* No Pre-Auth is required */
        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_updateNewBssList, No Pre-Auth is required\n");
        return TI_OK;
    }
    apConn_preAuthenticate(pRoamingMngr->hAPConnection, bssList);

    return TI_OK;

}


void roamingMngr_smNop(void *pData)
{
    roamingMngr_t       *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)pData;
    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, " roamingMngr_smNop\n");
}


void roamingMngr_smUnexpected(void *pData)
{
    roamingMngr_t       *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)pData;
    TRACE1(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, " roamingMngr_smUnexpected, state = %d\n", *(pRoamingMngr->pCurrentState));
}


void roamingMngr_smStop(void *pData)
{
    roamingMngr_t       *pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)pData;

    scanMngr_stopContScan(pRoamingMngr->hScanMngr);
    /* clean intenal variables */
    pRoamingMngr->maskRoamingEvents = TI_TRUE;
    pRoamingMngr->neighborApsExist = TI_FALSE;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;
}
/**
*
* roamingMngr_smStopWhileScanning -
*
* \b Description:
*
* Stop event means that the station is not in Connected State.
 * Stop continuos and immediate scans and clean internal vars.
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
void roamingMngr_smStopWhileScanning(void *pData)
{
    roamingMngr_t* pRoamingMngr;

    pRoamingMngr = (roamingMngr_t*)pData;

    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, " roamingMngr_smStopWhileScanning\n");

    scanMngr_stopImmediateScan(pRoamingMngr->hScanMngr);
    scanMngr_stopContScan(pRoamingMngr->hScanMngr);

    /* clean intenal variables */
    pRoamingMngr->maskRoamingEvents = TI_TRUE;
    pRoamingMngr->neighborApsExist = TI_FALSE;
    pRoamingMngr->roamingTrigger = ROAMING_TRIGGER_NONE;
}




/**
*
* roamingMngr_setBssLossThreshold API
*
* Description:
*
* Set the BSS Loss threshold by EMP and register for the event.
*
* ARGS:
*
*  hRoamingMngr   - Roaming manager handle \n
*  uNumOfBeacons - number of consecutive beacons not received allowed before BssLoss event is issued
*  uClientID - the ID of the client that has registered for this event. will be sent along with the BssLoss event to EMP
* \b RETURNS:
*
*  TI_STATUS - registration status.
 * TI_NOK - registration is not allowed
*
* \sa
*/
TI_STATUS roamingMngr_setBssLossThreshold (TI_HANDLE hRoamingMngr, TI_UINT32 uNumOfBeacons, TI_UINT16 uClientID)
{
    roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;

    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_setBssLossThreshold! \n");

    if(ROAMING_OPERATIONAL_MODE_MANUAL == pRoamingMngr->RoamingOperationalMode)
    {
        return currBss_registerBssLossEvent(pRoamingMngr->hCurrBss, uNumOfBeacons, uClientID);
    }
    else
    {
        TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_setBssLossThreshold is available only in auto mode! \n");
        WLAN_OS_REPORT(("\n roamingMngr_setBssLossThreshold is available only in auto mode! \n "));
        return TI_NOK;
    }
}



/**
*
* roamingMngr_Connect API
*
* Description:
*
* send the Connect event to roaming state machine
*
* ARGS:
*
*  hRoamingMngr   - Roaming manager handle \n
*  pTargetAp - the target AP to connect with info.
* \b RETURNS:
*
*  TI_STATUS - roamingMngr_smEvent status.
*/

TI_STATUS roamingMngr_connect(TI_HANDLE hRoamingMngr, TargetAp_t* pTargetAp)
{
    roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    bssList_t *bssList;
    int i=0;

    TRACE2(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_connect(),"
                                                               "transitionMethod = %d,"
                                                               "requestType = %d,"
                                                               " \n", pTargetAp->transitionMethod,pTargetAp->connRequest.requestType) ;


    TRACE6(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_connect(),"
                                                         " AP to roam BSSID: "
                                                         "%02x-%02x-%02x-%02x-%02x-%02x "
                                                         "\n", pTargetAp->newAP.BSSID[0],pTargetAp->newAP.BSSID[1],pTargetAp->newAP.BSSID[2],pTargetAp->newAP.BSSID[3],pTargetAp->newAP.BSSID[4],pTargetAp->newAP.BSSID[5]);


    /* Search for target AP in the scan manager results table, to get its beacon/ProbResponse buffer  */
    bssList = scanMngr_getBSSList(((roamingMngr_t*)hRoamingMngr)->hScanMngr);
    for (i=0; i< bssList->numOfEntries ; i++)
    {
        if (MAC_EQUAL(bssList->BSSList[i].BSSID, pTargetAp->newAP.BSSID))
        {
            pTargetAp->newAP.pBuffer = bssList->BSSList[i].pBuffer;
            pTargetAp->newAP.bufferLength = bssList->BSSList[i].bufferLength;
            os_memoryCopy(pRoamingMngr->hOs, &(pRoamingMngr->targetAP), (void*)pTargetAp, sizeof(TargetAp_t));
            return roamingMngr_smEvent(ROAMING_MANUAL_EVENT_CONNECT, hRoamingMngr);
        }
    }

    TRACE6(pRoamingMngr->hReport, REPORT_SEVERITY_ERROR, "roamingMngr_connect(),"
                                                         "AP was not found in scan table!! BSSID: "
                                                         "%02x-%02x-%02x-%02x-%02x-%02x "
                                                         "\n", pTargetAp->newAP.BSSID[0],pTargetAp->newAP.BSSID[1],pTargetAp->newAP.BSSID[2],pTargetAp->newAP.BSSID[3],pTargetAp->newAP.BSSID[4],pTargetAp->newAP.BSSID[5]);
    return TI_NOK;
}

/**
*
* roamingMngr_startImmediateScan API
*
* Description:
*
* start the immediate scan with the channel list received by the application
*
* ARGS:
*
*  hRoamingMngr   - Roaming manager handle \n
*  pChannelList -  The channel list to be scanned
* \b RETURNS:
*
*  TI_STATUS - roamingMngr_smEvent status.
*/

TI_STATUS roamingMngr_startImmediateScan(TI_HANDLE hRoamingMngr, channelList_t* pChannelList)
{
    roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;

    TRACE0(pRoamingMngr->hReport, REPORT_SEVERITY_INFORMATION, "roamingMngr_startImmediateScan().\n");

    /* Save the channelList for later usage in the scanMngr_startImmediateScan() */
    scanMngr_setManualScanChannelList (pRoamingMngr-> hScanMngr, pChannelList);
    return roamingMngr_smEvent(ROAMING_MANUAL_EVENT_SCAN, hRoamingMngr);
}



/**
*
* roamingMngr_stopImmediateScan API
*
* Description:
*
* stop the immediate scan, called by the application.
*
* ARGS:
*
*  hRoamingMngr   - Roaming manager handle \n
* \b RETURNS:
*
*  TI_STATUS - TI_OK.
*/
TI_STATUS roamingMngr_stopImmediateScan(TI_HANDLE hRoamingMngr)
{
    roamingMngr_t *pRoamingMngr = (roamingMngr_t*)hRoamingMngr;
    scanMngr_stopImmediateScan(pRoamingMngr->hScanMngr);

    return TI_OK;
}


/**
*
* roamingMngr_stopImmediateScan API
*
* Description:
*
* called upon the immediate scan by application complete
*
* ARGS:
*
*  hRoamingMngr   - Roaming manager handle
*  scanCmpltStatus - scanCmpltStatus
*
* \b RETURNS:
*
*  TI_STATUS - State machine event status.
*/

TI_STATUS  roamingMngr_immediateScanByAppComplete(TI_HANDLE hRoamingMngr, scan_mngrResultStatus_e scanCmpltStatus)
{
    return roamingMngr_smEvent(ROAMING_MANUAL_EVENT_COMPLETE, hRoamingMngr);
}

