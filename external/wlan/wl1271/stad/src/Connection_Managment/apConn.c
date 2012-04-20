/*
 * apConn.c
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

/** \file apConn.c
 *  \brief AP Connection
 *
 *  \see apConn.h
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  AP Connection                                                 *
 *   PURPOSE:                                                               *
 *   Roaming ability of eSTA is implemented by Roaming Manager Component and 
 *   described in "Roaming Manager module LLD" document, and by 
 *   AP Connection module. AP Connection module implemented as two sub-modules:
 *   The major one is AP Connection, that is responsible for: 
 *   - providing Roaming Manager with access to other parts of WLAN Driver, 
 *   - implementing low levels of roaming mechanism.
 *   Current BSS sub-module takes care of:
 *   - maintaining database of current AP info,
 *   - providing access to database of current AP info.
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_21
#include "osApi.h"
#include "report.h"
#include "sme.h"
#include "siteMgrApi.h"
#include "smeApi.h"
#include "PowerMgr_API.h"
#include "TrafficMonitorAPI.h"
#include "qosMngr_API.h"
#ifdef XCC_MODULE_INCLUDED
 #include "XCCMngr.h"
#endif
#include "measurementMgrApi.h"
#include "connApi.h"
#include "EvHandler.h"
#include "apConn.h"
#include "currBss.h"
#include "fsm.h"
#include "scrApi.h"
#include "regulatoryDomainApi.h"
#include "TWDriver.h"
#include "DrvMainModules.h"
#include "GenSM.h"

/*----------------------*/
/* Constants and macros */
/*----------------------*/

#ifdef TI_DBG
 #define    AP_CONN_VALIDATE_HANDLE(hAPConnection)  \
    if (hAPConnection == NULL)  \
    {   \
        WLAN_OS_REPORT(("FATAL ERROR: AP Connection context is not initiated\n"));  \
        return TI_NOK; \
    }
#else
 #define    AP_CONN_VALIDATE_HANDLE(hAPConnection)
#endif

#define MAX_ROAMING_TRIGGERS  ROAMING_TRIGGER_LAST

#define UPDATE_SEND_DEAUTH_PACKET_FLAG(roamingEventType) \
            if ((roamingEventType >= ROAMING_TRIGGER_MAX_TX_RETRIES) && \
                (roamingEventType != ROAMING_TRIGGER_SECURITY_ATTACK)) \
            { \
                pAPConnection->sendDeauthPacket = TI_FALSE; \
            } 

/* Init bits */


/*--------------*/
/* Enumerations */
/*--------------*/

/**
* AP Connection state machine states
*/
typedef enum
{
    AP_CONNECT_STATE_IDLE = 0,          /**< Initial state */
    AP_CONNECT_STATE_WAIT_ROAM,         /**< Connected to AP, waiting for start roaming command */
    AP_CONNECT_STATE_SWITCHING_CHANNEL, /**< Connected to AP, switch channel in progress */
    AP_CONNECT_STATE_WAIT_CONNECT_CMD,  /**< SCR allocated, PS mode entered; wait for cmd from Roam Mngr */
    AP_CONNECT_STATE_PREPARE_HAND_OFF,  /**< Request CCKM for new AP, wait for response */
    AP_CONNECT_STATE_CONNECTING,        /**< Performing Connection to new AP; wait for response from Conn SM */
    AP_CONNECT_STATE_DISCONNECTING,     /**< Wait for completion of current link disconnection */
    AP_CONNECT_STATE_REESTABLISH_VOICE, /**< Wait for completion of voice TSPEC re-negotiation */
    AP_CONNECT_STATE_LAST
} apConn_smStates;


/**
* AP Connection state machine events
*/
typedef enum
{
    AP_CONNECT_EVENT_PREPARE_FOR_ROAMING= 0,/**< Sent by Roam MNGR when roaming event occurs */
    AP_CONNECT_EVENT_FINISHED_OK,           /**< Indicates successful completion of request sent to Conn SM */
    AP_CONNECT_EVENT_FINISHED_NOT_OK,       /**< Indicates unsuccessful completion of request sent to Conn SM */
    AP_CONNECT_EVENT_RETAIN_CURRENT_AP,     /**< Sent by Roam MNGR when it wishes to give-up roaming */
    AP_CONNECT_EVENT_START,                 /**< Sent by SME when first time link to AP is established */
    AP_CONNECT_EVENT_START_ROAM,            /**< Sent by Roam MNGR when it wishes to roam to new AP */
    AP_CONNECT_EVENT_START_SWITCH_CHANNEL,  /**< Sent by Switch channel module when starting switch channel process (tx enabled) */
    AP_CONNECT_EVENT_FINISHED_SWITCH_CH,    /**< Sent by Switch channel module when finishing switch channel process (tx enabled) */
    AP_CONNECT_EVENT_FINISHED_HAND_OVER,    /**< Sent by XCC module when finishing hand-over */
    AP_CONNECT_EVENT_STOP,                  /**< Disconnect current link, send stop indication to other modules */
    AP_CONNECT_EVENT_LAST
} apConn_smEvents;

#define AP_CONNECT_NUM_STATES       AP_CONNECT_STATE_LAST    
#define AP_CONNECT_NUM_EVENTS       AP_CONNECT_EVENT_LAST   


/*----------*/
/* Typedefs */
/*----------*/

/*------------*/
/* Structures */
/*------------*/

/**
* AP Connection control block 
* Following structure defines parameters that can be configured externally,
* internal variables, AP Connection state machine and handlers of other modules
* used by AP Connection module
*/
typedef struct _apConn_t
{
    /* AP Connection state machine */
    TI_UINT8                currentState;       /**< AP Connection state machine current state */
    
    /* Internal parameters */
    TI_BOOL                 firstAttempt2Roam;  /**< TI_TRUE if still connected to original AP, TI_FALSE otherwise */
    TI_BOOL                 roamingEnabled;     /**< If TI_FALSE, act like if no roaming callback registered. */
    apConn_roamingTrigger_e roamReason;         /**< The most severe and recent reason for roaming */
	APDisconnect_t			APDisconnect;		/**< The AP disconnect trigger extra information */
    bssEntry_t              *newAP;             /**< Stores parameters of roaming candidate */
    apConn_connRequest_e    requestType;        /**< Stores type of roaming request */
    TI_INT8                 rssiThreshold;      /**< Stores recently configured RSSI threshold */
    TI_UINT8                snrThreshold;       /**< Stores recently configured SNR threshold */
    TI_UINT8                txFailureThreshold; /**< Stores recently configured consec. no ack threshold */
    TI_UINT8                lowRateThreshold;   /**< Stores recently configured consec. no ack threshold */
    TI_UINT32               vsIElength;         /**< Length of vendor specific info-element for assoc req (if defined) */
    char                    *vsIEbuf;           /**< Pointer to vendor specific info-element for assoc req (if defined) */
    TI_BOOL                 isRssiTriggerMaskedOut;
    TI_BOOL                 isSnrTriggerMaskedOut;
    TI_BOOL                 isConsTxFailureMaskedOut;
    TI_BOOL                 islowRateTriggerMaskedOut;
    TI_BOOL                 removeKeys;         /**< Indicates whether keys should be removed after disconnect or not */
    TI_BOOL                 ignoreDeauthReason0;/**< Indicates whether to ignore DeAuth with reason 0, required for Rogue AP test XCC-V2 */
    TI_BOOL                 sendDeauthPacket;   /**< Indicates whether to send DEAUTH packet when discommecting or not */
    TI_UINT8                deauthPacketReasonCode;   /**< Indicates what error code to indicate in the DEAUTH packet  */
    TI_BOOL                 voiceTspecConfigured;/**< Shall be set to TI_TRUE before roaming in case the TSPEC is configured */
    TI_BOOL                 videoTspecConfigured;/**< Shall be set to TRUE before roaming in case the TSPEC is configured */
    TI_BOOL                 reNegotiateTSPEC;   /**< Shall be set to TI_TRUE before hand-over if requested by Roaming Manager */
    TI_BOOL                 resetReportedRoamingStatistics; /**< Shall be set to TI_TRUE if starting to measure traffic */
    TI_UINT16               lastRoamingDelay;
    TI_UINT32               roamingStartedTimestamp;
    TI_UINT8                roamingSuccesfulHandoverNum;
    TI_BOOL					bNonRoamingDisAssocReason; /**< Indicate whether last disconnection was called from outside (SME) */
    
    /** Callback functions, registered by Roaming manager */
    apConn_roamMngrEventCallb_t  roamEventCallb;         /**< roam event triggers */
    apConn_roamMngrCallb_t       reportStatusCallb;      /**< connection status events  */
    apConn_roamMngrCallb_t       returnNeighborApsCallb; /**< neighbor APs list update */
    
    /* Handlers of other modules used by AP Connection */
    TI_HANDLE               hOs;
    TI_HANDLE               hReport;
    TI_HANDLE               hCurrBSS;
    TI_HANDLE               hRoamMng;
    TI_HANDLE               hSme;
    TI_HANDLE               hSiteMgr;
    TI_HANDLE               hXCCMngr;
    TI_HANDLE               hConnSm;
    TI_HANDLE               hPrivacy;
    TI_HANDLE               hQos;
    TI_HANDLE               hEvHandler; 
    TI_HANDLE               hScr;   
    TI_HANDLE               hAssoc;
    TI_HANDLE               hRegulatoryDomain;
    TI_HANDLE               hMlme;
    
    /* Counters for statistics */
    TI_UINT32               roamingTriggerEvents[MAX_ROAMING_TRIGGERS];
    TI_UINT32               roamingSuccesfulHandoverTotalNum;   
    TI_UINT32               roamingFailedHandoverNum;   
    TI_UINT32               retainCurrAPNum;
    TI_UINT32               disconnectFromRoamMngrNum;
    TI_UINT32               stopFromSmeNum;

    TI_HANDLE               hAPConnSM;
	apConn_roamingTrigger_e	assocRoamingTrigger;
} apConn_t;


/*-------------------------------*/
/* Internal functions prototypes */
/*-------------------------------*/

/* SM functions */
static TI_STATUS apConn_smEvent(TI_UINT8 *currState, TI_UINT8 event, void* data);
static void apConn_smNop(void *pData);
static void apConn_smUnexpected(void *pData);
static void apConn_smStartWaitingForTriggers(void *pData);
static void apConn_smConnectedToNewAP(void *pData);
static void apConn_smConfigureDriverBeforeRoaming(void *pData);
static void apConn_smStopConnection(void *pData);
static void apConn_smInvokeConnectionToNewAp(void *pData);
static void apConn_smReportDisconnected(void *pData);
static void apConn_smRetainAP(void *pData);
static void apConn_smRequestCCKM(void *pData);
static void apConn_smReportConnFail(void *pData);
static void apConn_smSwChFinished(void *pData);
static void apConn_smHandleTspecReneg (void *pData);

/* other functions */
#ifdef XCC_MODULE_INCLUDED
static void apConn_calcNewTsf(apConn_t *hAPConnection, TI_UINT8 *tsfTimeStamp, TI_UINT32 newSiteOsTimeStamp, TI_UINT32 beaconInterval);
#endif
static TI_STATUS apConn_qosMngrReportResultCallb (TI_HANDLE hApConn, trafficAdmRequestStatus_e result);
static void		 apConn_reportConnStatusToSME	 (apConn_t *pAPConnection);


/*-------------------------------*/
/* Public functions prototypes 	 */
/*-------------------------------*/

/**
*
* apConn_create
*
* \b Description: 
*
* Create the AP Connection context: 
* allocate memory for internal variables; 
* create state machine.
*
* \b ARGS:
*
*  I   - hOs - OS handler
*  
* \b RETURNS:
*
* Pointer to the AP Connection on success, NULL on failure 
* (unable to allocate memory or other error).
*
* \sa 
*/
TI_HANDLE apConn_create(TI_HANDLE hOs)
{
    apConn_t    *pAPConnection;

    if ((pAPConnection = os_memoryAlloc(hOs, sizeof(apConn_t))) != NULL)
    {
        pAPConnection->hOs = hOs;

        /* allocate the state machine object */
        pAPConnection->hAPConnSM = genSM_Create(hOs);
        if (pAPConnection->hAPConnSM == NULL)
        {
            WLAN_OS_REPORT(("FATAL ERROR: apConn_create(): Error allocating Connection StateMachine! - aborting\n"));
            return NULL;
        }

        /* Succeeded to create AP Connection module context - return pointer to it */
        return pAPConnection; 
    }
    else /* Failed to allocate control block */
    {
        WLAN_OS_REPORT(("FATAL ERROR: apConn_create(): Error allocating cb - aborting\n"));
        os_memoryFree(hOs, pAPConnection, sizeof(apConn_t));
        return NULL;
    }
}

/**
*
* apConn_unload
*
* \b Description: 
*
* Finish AP Connection module work:
* release the allocation for state machine and internal variables.
*
* \b ARGS:
*
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS apConn_unload(TI_HANDLE hAPConnection)
{
    apConn_t *pAPConnection;

    AP_CONN_VALIDATE_HANDLE (hAPConnection);

    pAPConnection = (apConn_t *)hAPConnection;

    /* Unload state machine */
    genSM_Unload(pAPConnection->hAPConnSM);

    /* Free pre-allocated control block */
    os_memoryFree (pAPConnection->hOs, pAPConnection, sizeof(apConn_t));

    return TI_OK;
}

static TGenSM_actionCell apConnSM_matrix[AP_CONNECT_NUM_STATES][AP_CONNECT_NUM_EVENTS] =
{
    /* next state and actions for IDLE state */
        {   {AP_CONNECT_STATE_IDLE, apConn_smUnexpected},                   /* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_IDLE, apConn_smUnexpected},                   /* FINISHED_OK          */
            {AP_CONNECT_STATE_IDLE, apConn_smUnexpected},                   /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_IDLE, apConn_smUnexpected},                   /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_WAIT_ROAM,apConn_smStartWaitingForTriggers},  /* START                */ 
            {AP_CONNECT_STATE_IDLE, apConn_smUnexpected},                   /* START_ROAM           */ 
            {AP_CONNECT_STATE_IDLE, apConn_smUnexpected},                   /* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_IDLE, apConn_smNop},                          /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_IDLE, apConn_smNop},                          /* FINISHED_HAND_OVER   */ 
            {AP_CONNECT_STATE_IDLE, apConn_smUnexpected}                    /* STOP                 */
        },                                                                              
        /* next state and actions for WAIT_ROAM state */
        {   {AP_CONNECT_STATE_WAIT_CONNECT_CMD,apConn_smConfigureDriverBeforeRoaming},/* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smUnexpected},          /* FINISHED_OK          */
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smUnexpected},          /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smUnexpected},          /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smUnexpected},          /* START                */ 
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smUnexpected},          /* START_ROAM           */ 
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smNop},         /* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smUnexpected},          /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smUnexpected},          /* FINISHED_HAND_OVER   */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smStopConnection}   /* STOP                 */
        },                                                                              
        /* next state and actions for SWITCHING_CHANNEL state */
        {   {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smUnexpected},  /* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smUnexpected},  /* FINISHED_OK          */
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smUnexpected},  /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smUnexpected},  /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smUnexpected},  /* START                */ 
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smUnexpected},  /* START_ROAM           */ 
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smSwChFinished},/* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smNop},                 /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_SWITCHING_CHANNEL, apConn_smUnexpected},  /* FINISHED_HAND_OVER   */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smStopConnection}   /* STOP                 */
        },                                                                              
        /* next state and actions for WAIT_CONNECT_CMD state */
        {   {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smUnexpected},   /* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smUnexpected},   /* FINISHED_OK          */
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smUnexpected},   /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_WAIT_ROAM, apConn_smRetainAP},            /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smUnexpected},   /* START                */ 
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smRequestCCKM},  /* START_ROAM           */ 
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smUnexpected},   /* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smUnexpected},   /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smUnexpected},   /* FINISHED_HAND_OVER   */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smStopConnection}   /* STOP                 */
        },                                                                              
        /* next state and actions for PREPARE_HAND_OFF state */
        {   {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* FINISHED_OK          */
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* START                */ 
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* START_ROAM           */ 
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_PREPARE_HAND_OFF, apConn_smUnexpected},   /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_CONNECTING, apConn_smInvokeConnectionToNewAp},/* FINISHED_HAND_OVER */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smStopConnection}       /* STOP               */
        },                                                                              
        /* next state and actions for CONNECTING state */
        {   {AP_CONNECT_STATE_CONNECTING, apConn_smUnexpected},             /* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_REESTABLISH_VOICE,apConn_smHandleTspecReneg}, /* FINISHED_OK             */
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smReportConnFail},   /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_CONNECTING, apConn_smUnexpected},             /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_CONNECTING, apConn_smUnexpected},             /* START                */ 
            {AP_CONNECT_STATE_CONNECTING, apConn_smUnexpected},             /* START_ROAM           */ 
            {AP_CONNECT_STATE_CONNECTING, apConn_smUnexpected},             /* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_CONNECTING, apConn_smUnexpected},             /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_CONNECTING, apConn_smUnexpected},             /* FINISHED_HAND_OVER   */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smStopConnection}       /* STOP                 */
        },                                                                              
        /* next state and actions for DISCONNECTING state */
        {   {AP_CONNECT_STATE_DISCONNECTING, apConn_smNop},             /* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_IDLE, apConn_smReportDisconnected},       /* FINISHED_OK          */
            {AP_CONNECT_STATE_IDLE, apConn_smReportDisconnected},       /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smNop},             /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smUnexpected},      /* START                */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smNop},             /* START_ROAM           */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smNop},             /* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smNop},             /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smNop},             /* FINISHED_HAND_OVER   */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smNop},             /* STOP                 */
        },                                                                             
        /* next state and actions for REESTABLISH_VOICE state */
        {   {AP_CONNECT_STATE_REESTABLISH_VOICE, apConn_smUnexpected},      /* PREPARE_FOR_ROAMING  */ 
            {AP_CONNECT_STATE_WAIT_ROAM,apConn_smConnectedToNewAP},         /* FINISHED_OK          */
            {AP_CONNECT_STATE_WAIT_CONNECT_CMD, apConn_smReportConnFail},   /* FINISHED_NOT_OK      */
            {AP_CONNECT_STATE_REESTABLISH_VOICE, apConn_smUnexpected},      /* RETAIN_CURRENT_AP    */ 
            {AP_CONNECT_STATE_REESTABLISH_VOICE, apConn_smUnexpected},      /* START                */ 
            {AP_CONNECT_STATE_REESTABLISH_VOICE, apConn_smUnexpected},      /* START_ROAM           */ 
            {AP_CONNECT_STATE_REESTABLISH_VOICE, apConn_smUnexpected},      /* START_SWITCH_CHANNEL */ 
            {AP_CONNECT_STATE_REESTABLISH_VOICE, apConn_smUnexpected},      /* FINISHED_SWITCH_CH   */ 
            {AP_CONNECT_STATE_REESTABLISH_VOICE, apConn_smUnexpected},      /* FINISHED_HAND_OVER   */ 
            {AP_CONNECT_STATE_DISCONNECTING, apConn_smStopConnection}       /* STOP                 */
        }                                                                             
}; 


/**
*
* apConn_init
*
* \b Description: 
*
* Prepare AP Connection module to work: initiate internal variables, start state machine
*
* \b ARGS:
*
*  I   - pStadHandles  - The driver modules handles  \n
*  
* \b RETURNS:
*
*  void
*
* \sa 
*/
void apConn_init (TStadHandlesList *pStadHandles)
{
    apConn_t *pAPConnection = (apConn_t *)(pStadHandles->hAPConnection);

    pAPConnection->hReport      = pStadHandles->hReport;
    pAPConnection->hCurrBSS     = pStadHandles->hCurrBss;
    pAPConnection->hRoamMng     = pStadHandles->hRoamingMngr;
    pAPConnection->hSme         = pStadHandles->hSme;
    pAPConnection->hSiteMgr     = pStadHandles->hSiteMgr;
    pAPConnection->hXCCMngr     = pStadHandles->hXCCMngr;
    pAPConnection->hConnSm      = pStadHandles->hConn;
    pAPConnection->hPrivacy     = pStadHandles->hRsn;
    pAPConnection->hQos         = pStadHandles->hQosMngr;
    pAPConnection->hEvHandler   = pStadHandles->hEvHandler;
    pAPConnection->hScr         = pStadHandles->hSCR;
    pAPConnection->hAssoc       = pStadHandles->hAssoc;
    pAPConnection->hMlme        = pStadHandles->hMlmeSm;
    pAPConnection->hRegulatoryDomain = pStadHandles->hRegulatoryDomain;
    
    pAPConnection->currentState = AP_CONNECT_STATE_IDLE;
    pAPConnection->firstAttempt2Roam = TI_TRUE;
    pAPConnection->roamingEnabled = TI_TRUE;
    pAPConnection->reportStatusCallb = NULL;
    pAPConnection->roamEventCallb = NULL;
    pAPConnection->returnNeighborApsCallb = NULL;

    pAPConnection->assocRoamingTrigger = ROAMING_TRIGGER_NONE;

    genSM_Init(pAPConnection->hAPConnSM, pAPConnection->hReport);

}


TI_STATUS apConn_SetDefaults  (TI_HANDLE hAPConnection, apConnParams_t *pApConnParams)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;
    TI_UINT32  index;

    pAPConnection->ignoreDeauthReason0 = pApConnParams->ignoreDeauthReason0;

    for (index=ROAMING_TRIGGER_NONE; index<ROAMING_TRIGGER_LAST; index++)
    {
        pAPConnection->roamingTriggerEvents[index] = 0;
    }
    pAPConnection->roamingSuccesfulHandoverNum = 0;
    pAPConnection->roamingSuccesfulHandoverTotalNum = 0;
    pAPConnection->roamingFailedHandoverNum = 0;
    pAPConnection->retainCurrAPNum = 0;
    pAPConnection->disconnectFromRoamMngrNum = 0;
    pAPConnection->stopFromSmeNum = 0;
    pAPConnection->txFailureThreshold = NO_ACK_DEFAULT_THRESHOLD;
    pAPConnection->lowRateThreshold = LOW_RATE_DEFAULT_THRESHOLD;
    pAPConnection->rssiThreshold = RSSI_DEFAULT_THRESHOLD;
    pAPConnection->snrThreshold = SNR_DEFAULT_THRESHOLD;
    pAPConnection->vsIElength = 0;
    pAPConnection->isRssiTriggerMaskedOut = TI_FALSE;
    pAPConnection->isSnrTriggerMaskedOut = TI_TRUE;
    pAPConnection->islowRateTriggerMaskedOut = TI_FALSE;
    pAPConnection->isConsTxFailureMaskedOut = TI_FALSE;
    pAPConnection->removeKeys = TI_TRUE;
    pAPConnection->sendDeauthPacket = TI_TRUE;       /* Default behavior is radio On - send DISASSOC frame */
	pAPConnection->deauthPacketReasonCode = STATUS_UNSPECIFIED;
    pAPConnection->voiceTspecConfigured = TI_FALSE;
	pAPConnection->videoTspecConfigured = TI_FALSE;
    pAPConnection->resetReportedRoamingStatistics = TI_FALSE;
    pAPConnection->reNegotiateTSPEC = TI_FALSE;
    pAPConnection->bNonRoamingDisAssocReason = TI_FALSE;

    pAPConnection->roamingStartedTimestamp = 0;
    pAPConnection->lastRoamingDelay = 0;
    pAPConnection->roamingSuccesfulHandoverNum = 0;


    genSM_SetDefaults(pAPConnection->hAPConnSM,
                           AP_CONNECT_NUM_STATES,
                           AP_CONNECT_NUM_EVENTS,
                           &apConnSM_matrix[0][0],
                           AP_CONNECT_STATE_IDLE,
                           "AP Connection SM",
                           NULL,
                           NULL,
                           __FILE_ID__);


    return TI_OK;
}


/* apConn_isPsRequiredBeforeScan
*
* \b Description: 
*
* verify if the PS required before scan according if roaming triger is part of ROAMING_TRIGGER_LOW_QUALITY_GROUP 
*
* \b ARGS:
*
*  I   - hAPConnection - pointer to module\n
*
* \b RETURNS:
*
*  TRUE or FALSE.
*
* \sa 
*/
TI_BOOL apConn_isPsRequiredBeforeScan(TI_HANDLE hAPConnection)
{
    apConn_t * pAPConnection = (apConn_t *) hAPConnection;

	/* check if part of ROAMING_TRIGGER_LOW_QUALITY_GROUP */
	if (pAPConnection->roamReason <= ROAMING_TRIGGER_MAX_TX_RETRIES)
		return TI_TRUE;
	else
		return TI_FALSE;
}



/**
*
* apConn_ConnCompleteInd
*
* \b Description: 
*
* Inform AP Connection about successful / unsuccessful completion 
* of link establishing 
*
* \b ARGS:
*
*  I   - result - TI_OK if successfully connected, TI_NOK otherwise  \n
*
* \b RETURNS:
*
*  None.
*
* \sa 
*/
void apConn_ConnCompleteInd(TI_HANDLE hAPConnection, mgmtStatus_e status, TI_UINT32 uStatusCode)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    if (status == STATUS_SUCCESSFUL) 
    {
        apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_OK, pAPConnection);
    }
    else
    {
        apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_NOT_OK, pAPConnection);
    }
}

TI_STATUS apConn_getRoamThresholds(TI_HANDLE hAPConnection, roamingMngrThresholdsConfig_t *pParam)
{
    apConn_t * pAPConnection = (apConn_t *) hAPConnection;

    pParam->lowRssiThreshold = pAPConnection->rssiThreshold;
    pParam->lowSnrThreshold = pAPConnection->snrThreshold;
    pParam->txRateThreshold = pAPConnection->lowRateThreshold;
    pParam->dataRetryThreshold = pAPConnection->txFailureThreshold;

    currBSS_getRoamingParams(pAPConnection->hCurrBSS, 
                             &pParam->numExpectedTbttForBSSLoss, 
                             &pParam->lowQualityForBackgroungScanCondition, 
                             &pParam->normalQualityForBackgroungScanCondition);

    return TI_OK;
}

TI_STATUS apConn_setRoamThresholds(TI_HANDLE hAPConnection, roamingMngrThresholdsConfig_t *pParam)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    /* If low quality trigger threshold is set to 0 - this is the request to ignore this trigger */
    /* Otherwise store it */
    if (pParam->lowRssiThreshold == (TI_INT8)AP_CONNECT_TRIGGER_IGNORED) 
    {
        pAPConnection->isRssiTriggerMaskedOut = TI_TRUE;
        pParam->lowRssiThreshold = pAPConnection->rssiThreshold;
    }
    else
    {
        pAPConnection->isRssiTriggerMaskedOut = TI_FALSE;
        pAPConnection->rssiThreshold = pParam->lowRssiThreshold;
    }

    if (pParam->txRateThreshold == AP_CONNECT_TRIGGER_IGNORED) 
    {
        pAPConnection->islowRateTriggerMaskedOut = TI_TRUE;
        pParam->txRateThreshold = pAPConnection->lowRateThreshold;
    }
    else
    {
        pAPConnection->islowRateTriggerMaskedOut = TI_FALSE;
        pAPConnection->lowRateThreshold = pParam->txRateThreshold;
    }

    if (pParam->dataRetryThreshold == AP_CONNECT_TRIGGER_IGNORED) 
    {
        pAPConnection->isConsTxFailureMaskedOut = TI_TRUE;
        pParam->dataRetryThreshold = pAPConnection->txFailureThreshold;
    }
    else
    {
        pAPConnection->isConsTxFailureMaskedOut = TI_FALSE;
        pAPConnection->txFailureThreshold = pParam->dataRetryThreshold;
    }

    pAPConnection->isSnrTriggerMaskedOut = TI_FALSE;
    pAPConnection->snrThreshold = pParam->lowSnrThreshold;

    currBSS_updateRoamingTriggers(pAPConnection->hCurrBSS, pParam);

    return TI_OK;
}

TI_STATUS apConn_registerRoamMngrCallb(TI_HANDLE hAPConnection, 
                                       apConn_roamMngrEventCallb_t  roamEventCallb,
                                       apConn_roamMngrCallb_t       reportStatusCallb,
                                       apConn_roamMngrCallb_t       returnNeighborApsCallb)
{
    apConn_t *pAPConnection;
    apConn_connStatus_t reportStatus;
    paramInfo_t param;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    pAPConnection = (apConn_t *)hAPConnection;

    pAPConnection->roamEventCallb = roamEventCallb;
    pAPConnection->reportStatusCallb = reportStatusCallb;
    if ((pAPConnection->roamingEnabled) && (pAPConnection->currentState != AP_CONNECT_STATE_IDLE))
    {
        param.paramType   = ASSOC_ASSOCIATION_REQ_PARAM;

        assoc_getParam(pAPConnection->hAssoc, &param);
        reportStatus.dataBuf = (char *)(param.content.assocReqBuffer.buffer);
        reportStatus.dataBufLength = param.content.assocReqBuffer.bufferSize;

        reportStatus.status = CONN_STATUS_CONNECTED;
        reportStatusCallb(pAPConnection->hRoamMng, &reportStatus);  
    }
    ((apConn_t *)hAPConnection)->returnNeighborApsCallb = returnNeighborApsCallb;

    return TI_OK;
}

TI_STATUS apConn_unregisterRoamMngrCallb(TI_HANDLE hAPConnection)
{
    apConn_t *pAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

        pAPConnection = (apConn_t *)hAPConnection;

        pAPConnection->roamEventCallb = NULL;
        pAPConnection->reportStatusCallb = NULL;
        pAPConnection->returnNeighborApsCallb = NULL;

    if ((pAPConnection->currentState != AP_CONNECT_STATE_IDLE) && (pAPConnection->currentState != AP_CONNECT_STATE_WAIT_ROAM))
    {
        /* Roaming Manager is unregistering it's callbacks in the middle of roaming - disconnect */
        apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_STOP, pAPConnection);
    }
    return TI_OK;
}

TI_STATUS apConn_disconnect(TI_HANDLE hAPConnection)
{
    apConn_t *pAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    pAPConnection = (apConn_t *)hAPConnection;
    UPDATE_SEND_DEAUTH_PACKET_FLAG(pAPConnection->roamReason);
	if (pAPConnection->roamReason == ROAMING_TRIGGER_SECURITY_ATTACK)
		pAPConnection->deauthPacketReasonCode = STATUS_MIC_FAILURE;
	else
		pAPConnection->deauthPacketReasonCode = STATUS_UNSPECIFIED;
	apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_STOP, pAPConnection);
    pAPConnection->disconnectFromRoamMngrNum++;

    return TI_OK;
}

TI_STATUS apConn_getStaCapabilities(TI_HANDLE hAPConnection,
                                    apConn_staCapabilities_t *ie_list)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;
    apConn_staCapabilities_t *pList;
    paramInfo_t param;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

        pList = ie_list;

    /* Get authentication suite type */
    param.paramType = RSN_EXT_AUTHENTICATION_MODE;
    rsn_getParam(pAPConnection->hPrivacy, &param);      

    switch (param.content.rsnExtAuthneticationMode)
    {
        case RSN_EXT_AUTH_MODE_OPEN:
            pList->authMode = os802_11AuthModeOpen;
            break;
        case RSN_EXT_AUTH_MODE_SHARED_KEY:
            pList->authMode = os802_11AuthModeShared;
            break;
        case RSN_EXT_AUTH_MODE_AUTO_SWITCH:
            pList->authMode = os802_11AuthModeAutoSwitch;
            break;
        case RSN_EXT_AUTH_MODE_WPA:
            pList->authMode = os802_11AuthModeWPA;
            break;
        case RSN_EXT_AUTH_MODE_WPAPSK:
            pList->authMode = os802_11AuthModeWPAPSK;
            break;
        case RSN_EXT_AUTH_MODE_WPANONE:
            pList->authMode = os802_11AuthModeWPANone;
            break;
        case RSN_EXT_AUTH_MODE_WPA2:
            pList->authMode = os802_11AuthModeWPA2;
            break;
        case RSN_EXT_AUTH_MODE_WPA2PSK:
            pList->authMode = os802_11AuthModeWPA2PSK;
            break;
        default:
            pList->authMode = os802_11AuthModeOpen;
            break;
    }

    /* Get encryption type */
    param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
    rsn_getParam(pAPConnection->hPrivacy, &param);

    switch (param.content.rsnEncryptionStatus)
    {
        case TWD_CIPHER_NONE:
            pList->encryptionType = OS_ENCRYPTION_TYPE_NONE;
            break;
        case TWD_CIPHER_WEP:
        case TWD_CIPHER_WEP104:
            pList->encryptionType = OS_ENCRYPTION_TYPE_WEP;
            break;
        case TWD_CIPHER_TKIP:
        case TWD_CIPHER_CKIP:
            pList->encryptionType = OS_ENCRYPTION_TYPE_TKIP;
            break;
        case TWD_CIPHER_AES_WRAP:
        case TWD_CIPHER_AES_CCMP:
            pList->encryptionType = OS_ENCRYPTION_TYPE_AES;
            break;
        default:
            pList->encryptionType = OS_ENCRYPTION_TYPE_NONE;
            break;
    }

    /* Get supported rates */
    param.paramType = SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM;
    siteMgr_getParam(pAPConnection->hSiteMgr, &param);
    os_memoryCopy(pAPConnection->hOs, (void *)param.content.siteMgrDesiredSupportedRateSet.ratesString, pList->rateMask, sizeof(OS_802_11_RATES_EX));
    
    /* Get mode: 2.4G, 5G or Dual */
    param.paramType = SITE_MGR_DESIRED_DOT11_MODE_PARAM;
    siteMgr_getParam(pAPConnection->hSiteMgr, &param);
    pList->networkType = (OS_802_11_NETWORK_TYPE)param.content.siteMgrDot11Mode;
    switch(param.content.siteMgrDot11Mode) 
    {
        case DOT11_B_MODE:
            pList->networkType = os802_11DS;
            break;
        case DOT11_A_MODE:
            pList->networkType = os802_11OFDM5;
            break;
        case DOT11_G_MODE:
            pList->networkType = os802_11OFDM24;
            break;
        case DOT11_DUAL_MODE:
            pList->networkType = os802_11Automode;
            break;
        default:
            pList->networkType = os802_11DS;
            break;
    }


    /* Get XCC status */
#ifdef XCC_MODULE_INCLUDED
    param.paramType = XCC_ENABLED;
    XCCMngr_getParam(pAPConnection->hXCCMngr, &param);
    pList->XCCEnabled = (param.content.XCCEnabled==XCC_MODE_ENABLED)? TI_TRUE : TI_FALSE;
#else
    pList->XCCEnabled = TI_FALSE;
#endif

    /* Get QoS type */
    param.paramType = QOS_MNGR_ACTIVE_PROTOCOL;
    qosMngr_getParams(pAPConnection->hQos, &param);
    pList->qosEnabled = param.content.qosSiteProtocol != QOS_NONE;

    pList->regDomain = REG_DOMAIN_FIXED;
    /* Get regulatory domain type */
    param.paramType = REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM;
    regulatoryDomain_getParam(pAPConnection->hRegulatoryDomain, &param);
    if (param.content.spectrumManagementEnabled)
    {   /* 802.11h is enabled (802.11h includes 802.11d) */
        pList->regDomain = REG_DOMAIN_80211H;
    }
    else 
    {
    param.paramType = REGULATORY_DOMAIN_ENABLED_PARAM;
    regulatoryDomain_getParam(pAPConnection->hRegulatoryDomain, &param);
        if (param.content.regulatoryDomainEnabled)
        {   /* 802.11d is enabled */
            pList->regDomain = REG_DOMAIN_80211D;
        }
    }
    return TI_OK;
}

TI_STATUS apConn_connectToAP(TI_HANDLE hAPConnection,
                             bssEntry_t *newAP,
                             apConn_connRequest_t *request,
                             TI_BOOL reNegotiateTspec)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    pAPConnection->requestType = request->requestType;

    switch (request->requestType) 
    {
        case AP_CONNECT_RETAIN_CURR_AP:
            apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_RETAIN_CURRENT_AP, pAPConnection);
            break;

        case AP_CONNECT_FULL_TO_AP:
            pAPConnection->removeKeys = TI_TRUE;
            pAPConnection->newAP = newAP;
            pAPConnection->roamingFailedHandoverNum++;
            pAPConnection->reNegotiateTSPEC = reNegotiateTspec;
            apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_START_ROAM, pAPConnection);
            break;

        case AP_CONNECT_FAST_TO_AP:
        case AP_CONNECT_RECONNECT_CURR_AP:
            pAPConnection->removeKeys = TI_FALSE;
            pAPConnection->newAP = newAP;
            pAPConnection->roamingFailedHandoverNum++;
            pAPConnection->reNegotiateTSPEC = reNegotiateTspec;
            apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_START_ROAM, pAPConnection);
            break;

        default:
            break;
    }

    /* If there is vendor specific IE to attach to Assoc req, store it now */
    if (request->dataBufLength > 0)
    {
        pAPConnection->vsIEbuf = request->dataBuf;
        pAPConnection->vsIElength = request->dataBufLength;
    }

    return TI_OK;
}

bssEntry_t *apConn_getBSSParams(TI_HANDLE hAPConnection)
{
#ifdef TI_DBG
    if (hAPConnection == NULL) /* Failed to allocate control block */
    {
        WLAN_OS_REPORT(("FATAL ERROR: apConn_create(): Error allocating cb - aborting\n"));
        return NULL;
    }
#endif
        
    return currBSS_getBssInfo(((apConn_t *)hAPConnection)->hCurrBSS);
}

TI_BOOL apConn_isSiteBanned(TI_HANDLE hAPConnection, TMacAddr * givenAp)
{
    apConn_t * pAPConnection = (apConn_t *) hAPConnection;

    return rsn_isSiteBanned(pAPConnection->hPrivacy, *givenAp);
}

TI_BOOL apConn_getPreAuthAPStatus(TI_HANDLE hAPConnection, TMacAddr *givenAp)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;
    paramInfo_t     param;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    param.paramType = RSN_PRE_AUTH_STATUS;
    MAC_COPY (param.content.rsnApMac, *givenAp);
    rsn_getParam(pAPConnection->hPrivacy, &param);

    return param.content.rsnPreAuthStatus;
}

TI_STATUS apConn_preAuthenticate(TI_HANDLE hAPConnection, bssList_t *listAPs)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;
    TBssidList4PreAuth  apList;
    TI_UINT32           listIndex, apListIndex;
    bssEntry_t          *pCurrentAP;
    TI_UINT8            *pRsnIEs;

#ifdef TI_DBG
    if ((hAPConnection == NULL) || (listAPs == NULL))
    {
        WLAN_OS_REPORT(("FATAL ERROR: AP Connection context is not initiated\n"));
        return TI_NOK;
    }
        
        TRACE0(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_reserveResources \n");
#endif

    for (listIndex=0, apListIndex=0; listIndex<listAPs->numOfEntries; listIndex++)
    {
        MAC_COPY (apList.bssidList[apListIndex].bssId, 
				  listAPs->BSSList[listIndex].BSSID);

        /* search in the buffer pointer to the beginning of the
            RSN IE according to the IE ID */
        if (!mlmeParser_ParseIeBuffer (pAPConnection->hMlme, listAPs->BSSList[listIndex].pBuffer, listAPs->BSSList[listIndex].bufferLength, RSN_IE_ID, &pRsnIEs, NULL, 0))
        {
            TRACE0(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_preAuthenticate, no RSN IE was found \n");
            TRACE_INFO_HEX(pAPConnection->hReport, listAPs->BSSList[listIndex].pBuffer, listAPs->BSSList[listIndex].bufferLength); 
            continue;
        }

        apList.bssidList[apListIndex].pRsnIEs = (dot11_RSN_t*)pRsnIEs;
        apList.bssidList[apListIndex].rsnIeLen = apList.bssidList[apListIndex].pRsnIEs->hdr[1] + 2;
        apListIndex++;
    }

    /* Start pre-auth after any Conn succ (including first), 
    and not only when a New BSSID was added, in order to save/refresh 
    PMKID of the current AP.*/
    {
        /* Add the current BSSID to the list */
        pCurrentAP = apConn_getBSSParams(pAPConnection);
        MAC_COPY (apList.bssidList[apListIndex].bssId, pCurrentAP->BSSID);
        /* search in the buffer pointer to the beginning of the
            RSN IE according to the IE ID */

        if (!mlmeParser_ParseIeBuffer (pAPConnection->hMlme, pCurrentAP->pBuffer, pCurrentAP->bufferLength, RSN_IE_ID, &pRsnIEs, NULL, 0))
        {
            TRACE6(pAPConnection->hReport, REPORT_SEVERITY_ERROR, "apConn_preAuthenticate, no RSN IE was found in the current BSS, BSSID=0x%x-0x%x-0x%x-0x%x-0x%x-0x%x \n", pCurrentAP->BSSID[0], pCurrentAP->BSSID[1], pCurrentAP->BSSID[2], pCurrentAP->BSSID[3], pCurrentAP->BSSID[4], pCurrentAP->BSSID[5]);
            report_PrintDump (pCurrentAP->pBuffer, pCurrentAP->bufferLength);
            apList.bssidList[apListIndex].pRsnIEs = NULL;
            apList.bssidList[apListIndex].rsnIeLen = 0;
        }
        else
        {
            apList.bssidList[apListIndex].pRsnIEs = (dot11_RSN_t*)pRsnIEs;
            apList.bssidList[apListIndex].rsnIeLen = apList.bssidList[apListIndex].pRsnIEs->hdr[1] + 2;
        }
        apList.NumOfItems = apListIndex+1;
        rsn_startPreAuth(pAPConnection->hPrivacy, &apList);
    }
    return TI_OK;
}

TI_STATUS apConn_prepareToRoaming(TI_HANDLE hAPConnection, apConn_roamingTrigger_e reason)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    pAPConnection->roamReason = reason;
    
    apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_PREPARE_FOR_ROAMING, pAPConnection);
    return TI_OK;
}

/**
*
* apConn_indicateSwitchChannelInProgress
*
* \b Description: 
*
* This function is called when switch channel process is started; it will trigger 
* AP Connection state machine from 'Wait for roaming start' to 'Switch channel in progress'
* state.
*
* \b ARGS:
*
*  I   - reason - the reason for roaming \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS apConn_indicateSwitchChannelInProgress(TI_HANDLE hAPConnection)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_START_SWITCH_CHANNEL, pAPConnection);
    return TI_OK;
}


/**
*
* apConn_indicateSwitchChannelFinished
*
* \b Description: 
*
* This function is called when switch channel process is finished
*
* \b ARGS:
*
*  I   - reason - the reason for roaming \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS apConn_indicateSwitchChannelFinished(TI_HANDLE hAPConnection)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_SWITCH_CH, pAPConnection);

    return TI_OK;
}


/**
*
* apConn_start
*
* \b Description: 
*
* Called by SME module when new connection has been successfully established (first time connection)
*
* \b ARGS:
*
*  I   - isValidBSS - if TI_FALSE, no roaming shall be performed, disconnect upon any roaming event; 
*                   other parameters of current AP can be received from Current BSS module
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS apConn_start(TI_HANDLE hAPConnection, TI_BOOL roamingEnabled)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    pAPConnection->roamingEnabled = roamingEnabled;

    apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_START, pAPConnection);
    return TI_OK;
}


/**
*
* apConn_stop
*
* \b Description: 
*
* Called by SME module when current connection must be taken down
* (due to driver download, connection failure or any other reason)
*
* \b ARGS:
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS apConn_stop(TI_HANDLE hAPConnection, TI_BOOL removeKeys)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    pAPConnection->stopFromSmeNum++;
    pAPConnection->removeKeys = removeKeys;
    pAPConnection->sendDeauthPacket = TI_TRUE;
    pAPConnection->reNegotiateTSPEC = TI_FALSE;
    pAPConnection->voiceTspecConfigured = TI_FALSE;
	pAPConnection->videoTspecConfigured = TI_FALSE;

	/* Mark that the connection is stopped due to reason outside the scope of this module */
	if (pAPConnection->roamReason == ROAMING_TRIGGER_SECURITY_ATTACK)
		pAPConnection->bNonRoamingDisAssocReason = TI_FALSE;
	else
		pAPConnection->bNonRoamingDisAssocReason = TI_TRUE;

	apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_STOP, pAPConnection);
    return TI_OK;
}


/**
*
* apConn_reportRoamingEvent
*
* \b Description: 
*
* Called when one of roaming events occur
*
* \b ARGS:
*
*  I   - roamingEventType   
*  I   - pRoamingEventData - in case of 'Tx rate' event, or AP disconnect
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS apConn_reportRoamingEvent(TI_HANDLE hAPConnection,
                                    apConn_roamingTrigger_e roamingEventType,
                                    roamingEventData_u *pRoamingEventData)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection; 
    paramInfo_t param;  /* parameter for retrieving BSSID */
    TI_UINT16 reasonCode = 0;

    AP_CONN_VALIDATE_HANDLE(hAPConnection);

    TRACE4(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_reportRoamingEvent, type=%d, cur_state=%d, roamingEnabled=%d, roamEventCallb=%p  \n", roamingEventType, pAPConnection->currentState, pAPConnection->roamingEnabled, pAPConnection->roamEventCallb);

    pAPConnection->assocRoamingTrigger = roamingEventType;

    /* 1. Check if this is Rogue AP test case */
    if (roamingEventType == ROAMING_TRIGGER_AP_DISCONNECT)
    {   
		if (pRoamingEventData != NULL)
		{	/* Save the disconnect reason for future use */
			pAPConnection->APDisconnect.uStatusCode     = pRoamingEventData->APDisconnect.uStatusCode;
			pAPConnection->APDisconnect.bDeAuthenticate = pRoamingEventData->APDisconnect.bDeAuthenticate;
            reasonCode = pRoamingEventData->APDisconnect.uStatusCode;
		}
        if ((pAPConnection->ignoreDeauthReason0) && (pRoamingEventData!=NULL) &&
               (pAPConnection->APDisconnect.uStatusCode == 0))
        {   /* This is required for Rogue AP test,
                When Rogue AP due to invalid User name, deauth with reason 0 arrives before the Rogue AP,
                and this XCC test fails.*/
            TRACE0(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_reportRoamingEvent, Ignore DeAuth with reason 0 \n");
            return TI_OK;
        }
        TRACE1(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_reportRoamingEvent, DeAuth with reason %d \n", pAPConnection->APDisconnect.uStatusCode);

        if (pAPConnection->APDisconnect.uStatusCode == STATUS_CODE_802_1X_AUTHENTICATION_FAILED)
        {
          #ifdef XCC_MODULE_INCLUDED

            /* Raise the EAP-Failure as event */
            XCCMngr_rogueApDetected (pAPConnection->hXCCMngr, RSN_AUTH_STATUS_CHALLENGE_FROM_AP_FAILED);
          #endif

            
            /* Remove AP from candidate list for a specified amount of time */
            param.paramType = SITE_MGR_CURRENT_BSSID_PARAM;
        	siteMgr_getParam(pAPConnection->hSiteMgr, &param);
            
            TRACE1(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "current station is banned from the roaming candidates list for %d Ms\n", RSN_AUTH_FAILURE_TIMEOUT);

            rsn_banSite(pAPConnection->hPrivacy, param.content.siteMgrDesiredBSSID, RSN_SITE_BAN_LEVEL_FULL, RSN_AUTH_FAILURE_TIMEOUT);
        }

    }

    /* 2. Check if received trigger is masked out */
    if (((pAPConnection->isConsTxFailureMaskedOut) && (roamingEventType == ROAMING_TRIGGER_MAX_TX_RETRIES)) ||
        ((pAPConnection->isRssiTriggerMaskedOut)   && (roamingEventType == ROAMING_TRIGGER_LOW_QUALITY)) ||
        ((pAPConnection->isSnrTriggerMaskedOut)    && (roamingEventType == ROAMING_TRIGGER_LOW_SNR)) ||
        ((pAPConnection->islowRateTriggerMaskedOut)&& (roamingEventType == ROAMING_TRIGGER_LOW_TX_RATE)))
    {
        TRACE0(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_reportRoamingEvent, trigger ignored \n");
        return TI_OK;
    }

    /* 3.  Valid trigger received: */
    /* 3a. Update statistics */
    pAPConnection->roamingTriggerEvents[roamingEventType]++;

	/* 3b. Store the most severe trigger */
	if (pAPConnection->roamReason < roamingEventType)
	{
		pAPConnection->roamReason = roamingEventType;
	}

    /* 3c. Check if Roaming Manager is available */
    if (((!pAPConnection->roamingEnabled) || (pAPConnection->roamEventCallb == NULL) ||
          (pAPConnection->currentState == AP_CONNECT_STATE_IDLE))
        && (roamingEventType > ROAMING_TRIGGER_MAX_TX_RETRIES))
    {
       /* 'Any SSID' configured, meaning Roaming Manager is not allowed to perform roaming, 
           or Roaming Manager is not registered for roaming events;
           unless this is trigger to change parameters of background scan, disconnect the link */
        TRACE1(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "Disconnecting link due to roaming event: ev = %d\n", roamingEventType);

        /*  Handle IBSS case TBD to remove 
            Handle also the case where A first connection is in progress, and 
            de-auth arrived. */
        if (pAPConnection->currentState == AP_CONNECT_STATE_IDLE)
		{
			sme_ReportApConnStatus(pAPConnection->hSme, STATUS_DISCONNECT_DURING_CONNECT, pAPConnection->APDisconnect.uStatusCode);
		}
        else
        {
            /* Infra-structure BSS case - disconnect the link */
            if (roamingEventType >= ROAMING_TRIGGER_AP_DISCONNECT && (roamingEventType != ROAMING_TRIGGER_TSPEC_REJECTED))
            {
                pAPConnection->removeKeys = TI_TRUE;
            }
            else
            {
                pAPConnection->removeKeys = TI_FALSE;
            }
            UPDATE_SEND_DEAUTH_PACKET_FLAG(roamingEventType);
			if (roamingEventType == ROAMING_TRIGGER_SECURITY_ATTACK)
				pAPConnection->deauthPacketReasonCode = STATUS_MIC_FAILURE;
			else
				pAPConnection->deauthPacketReasonCode = STATUS_UNSPECIFIED;
            apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_STOP, pAPConnection);
        }
        return TI_OK;
    }

    /* 4. Check if we are in the middle of switching channel */
    if (pAPConnection->currentState == AP_CONNECT_STATE_SWITCHING_CHANNEL)
    {
        /* Trigger received in the middle of switch channel, continue without reporting Roaming Manager */
        TRACE1(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "Roaming event during switch channel: ev = %d\n", roamingEventType);
        return TI_OK;
    }

    /* 5. Report Roaming Manager */
    if ((pAPConnection->roamingEnabled == TI_TRUE) && (pAPConnection->roamEventCallb != NULL))
    {
        TRACE1(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "Roaming event raised: ev = %d\n", roamingEventType);
        if (roamingEventType == ROAMING_TRIGGER_LOW_QUALITY) 
        {
            EvHandlerSendEvent(pAPConnection->hEvHandler, IPC_EVENT_LOW_RSSI, NULL,0);
        }
        /* Report to Roaming Manager */

#ifdef XCC_MODULE_INCLUDED
        /* For XCC only - if the is reason is TSPEC reject - mark this as BssLoss - To be changed later */
        if (roamingEventType == ROAMING_TRIGGER_TSPEC_REJECTED)
        {
            roamingEventType = ROAMING_TRIGGER_BSS_LOSS;
        }
#endif        
        pAPConnection->roamEventCallb(pAPConnection->hRoamMng, &roamingEventType, reasonCode);
    }

    return TI_OK;
}


/**
*
* apConn_RoamHandoffFinished
*
* \b Description: 
*
* Called when XCC module receives response from the supplicant or recognizes 
* timeout while waiting for the response 
*
* \b ARGS:
*
* \b RETURNS:
*
* \sa 
*/
void apConn_RoamHandoffFinished(TI_HANDLE hAPConnection)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

#ifdef TI_DBG
    if (hAPConnection == NULL) /* Failed to allocate control block */
    {
        WLAN_OS_REPORT(("FATAL ERROR: apConn_create(): Error allocating cb - aborting\n"));
        return;
    }
#endif

    apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_HAND_OVER, pAPConnection);
}


/**
*
* apConn_DisconnCompleteInd
*
* \b Description: 
*
* DISASSOCIATE Packet was sent - proceed with stopping the module
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  None
*
* \sa 
*/
void apConn_DisconnCompleteInd(TI_HANDLE hAPConnection, mgmtStatus_e status, TI_UINT32 uStatusCode)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_OK, pAPConnection);
}


/**
*
* apConn_updateNeighborAPsList
*
* \b Description: 
*
* Called by XCC Manager when Priority APs are found  
*
* \b ARGS:
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
void apConn_updateNeighborAPsList(TI_HANDLE hAPConnection, neighborAPList_t *pListOfpriorityAps)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;
    
    if (pAPConnection->returnNeighborApsCallb != NULL )
    {
        pAPConnection->returnNeighborApsCallb(pAPConnection->hRoamMng, pListOfpriorityAps);
    }
}


/**
*
* apConn_getRoamingStatistics
*
* \b Description: 
*
* Called from Measurement XCC sub-module when preparing TSM report to the AP. 
*
* \b ARGS: AP Connection handle
*
* \b RETURNS:
*
*  total number of successful roams
*  delay of the latest successful roam
*
* \sa 
*/
void apConn_getRoamingStatistics(TI_HANDLE hAPConnection, TI_UINT8 *roamingCount, TI_UINT16 *roamingDelay)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;
    
    /* Get (and clear) total number of successful roams */
    *roamingCount = pAPConnection->roamingSuccesfulHandoverNum;
    pAPConnection->roamingSuccesfulHandoverNum = 0;

    /* Get delay of the latest roam */
    *roamingDelay = pAPConnection->lastRoamingDelay;
    pAPConnection->lastRoamingDelay = 0;
}




/**
*
* apConn_resetRoamingStatistics
*
* \b Description: 
*
* Called from Measurement XCC sub-module in order to re-start roaming statistics. 
*
* \b ARGS: AP Connection handle
*
* \b RETURNS:
*
*  total number of successful roams
*  delay of the latest successful roam
*
* \sa 
*/
void apConn_resetRoamingStatistics(TI_HANDLE hAPConnection)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;
    
    pAPConnection->resetReportedRoamingStatistics = TI_TRUE;
    pAPConnection->roamingSuccesfulHandoverNum = 0;
    pAPConnection->lastRoamingDelay = 0;
}


/**
*
* apConn_printStatistics
*
* \b Description: 
*
* Called by Site Manager when request to print statistics is requested from CLI  
*
* \b ARGS: AP Connection handle
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
void apConn_printStatistics(TI_HANDLE hAPConnection)
{
    WLAN_OS_REPORT(("-------------- Roaming Statistics ---------------\n\n"));
    WLAN_OS_REPORT(("- Low TX rate = %d\n",     ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_LOW_TX_RATE]));
    WLAN_OS_REPORT(("- Low SNR = %d\n",         ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_LOW_SNR]));
    WLAN_OS_REPORT(("- Low Quality = %d\n",     ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_LOW_QUALITY]));
    WLAN_OS_REPORT(("- MAX TX retries = %d\n",  ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_MAX_TX_RETRIES]));
    WLAN_OS_REPORT(("- BSS Loss TX = %d\n",     ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_BSS_LOSS]));
    WLAN_OS_REPORT(("- Switch Channel = %d\n",  ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_SWITCH_CHANNEL]));
    WLAN_OS_REPORT(("- AP Disconnect = %d\n",   ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_AP_DISCONNECT]));
    WLAN_OS_REPORT(("- SEC attack = %d\n",      ((apConn_t *)hAPConnection)->roamingTriggerEvents[ROAMING_TRIGGER_SECURITY_ATTACK]));
    WLAN_OS_REPORT(("\n"));
    WLAN_OS_REPORT(("- Successful roaming = %d\n",                  ((apConn_t *)hAPConnection)->roamingSuccesfulHandoverTotalNum));
    WLAN_OS_REPORT(("- UnSuccessful roaming = %d\n",                ((apConn_t *)hAPConnection)->roamingFailedHandoverNum));
    WLAN_OS_REPORT(("- Giving up roaming = %d\n",                   ((apConn_t *)hAPConnection)->retainCurrAPNum));
    WLAN_OS_REPORT(("- Disconnect cmd from roaming manager = %d\n", ((apConn_t *)hAPConnection)->disconnectFromRoamMngrNum));
    WLAN_OS_REPORT(("- Disconnect cmd from SME = %d\n",             ((apConn_t *)hAPConnection)->stopFromSmeNum));
    WLAN_OS_REPORT(("\n"));
}



/**
*
* apConn_getVendorSpecificIE
*
* \b Description: 
*
* Called by Association SM when request to associate is built and sent to AP;
* returns request updated with vendor specific info-element
*
* \b ARGS: 
*
*  I   - hAPConnection - AP Connection handle\n
*  O   - pRequest - pointer to request buffer\n
*  O   - len - size of returned IE\n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS apConn_getVendorSpecificIE(TI_HANDLE hAPConnection, TI_UINT8 *pRequest, TI_UINT32 *len)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

    if (pAPConnection->vsIElength > 0)
    {
        *len = pAPConnection->vsIElength;
        os_memoryCopy(pAPConnection->hOs, pRequest, pAPConnection->vsIEbuf, pAPConnection->vsIElength);
    }
    else
    {
        *len = 0;
    }
    return TI_OK;
}


/* Internal functions implementation */


/**
*
* apConn_smEvent
*
* \b Description: 
*
* AP Connection state machine transition function
*
* \b ARGS:
*
*  I/O - currentState - current state in the state machine\n
*  I   - event - specific event for the state machine\n
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static TI_STATUS apConn_smEvent(TI_UINT8 *currState, TI_UINT8 event, void* data)
{
    apConn_t  *pAPConnection = (apConn_t *)data;
    TGenSM    *pGenSM = (TGenSM*)pAPConnection->hAPConnSM;

	TRACE2(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_smEvent: currState = %d, event = %d\n", pGenSM->uCurrentState, event);
    genSM_Event (pAPConnection->hAPConnSM, (TI_UINT32)event, data);
    pAPConnection->currentState = pGenSM->uCurrentState;

	TRACE1(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_smEvent: newState = %d\n", pAPConnection->currentState);

    return TI_OK;
}


/**
*
* apConn_smNop - Do nothing
*
* \b Description: 
*
* Do nothing in the SM.
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static void apConn_smNop(void *pData)
{
	apConn_t *hReport = ((apConn_t *)pData)->hReport;
    TRACE0(hReport, REPORT_SEVERITY_INFORMATION, "apConn_smNop\n");
}


/**
*
* apConn_smUnexpected - Unexpected event
*
* \b Description: 
*
* Unexpected event in the SM.
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
void apConn_smUnexpected(void *pData)
{
    TRACE0(((apConn_t *)pData)->hReport, REPORT_SEVERITY_INFORMATION, "apConn_smUnexpected\n");
}


/**
*
* apConn_smStartWaitingForTriggers
*
* \b Description: 
*
* SME informs AP Connection module about successfull link establishment; start wiating for roaming triggers
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smStartWaitingForTriggers(void *pData)
{
    apConn_t    *pAPConnection;
    apConn_connStatus_t reportStatus;  
    paramInfo_t param;

    pAPConnection = (apConn_t *)pData;
    
    if ((pAPConnection->roamingEnabled) && (pAPConnection->reportStatusCallb != NULL))
    {
        param.paramType   = ASSOC_ASSOCIATION_REQ_PARAM;

        assoc_getParam(pAPConnection->hAssoc, &param);
        reportStatus.dataBuf = (char *)(param.content.assocReqBuffer.buffer);
        reportStatus.dataBufLength = param.content.assocReqBuffer.bufferSize;

        reportStatus.status = CONN_STATUS_CONNECTED;
        pAPConnection->reportStatusCallb(pAPConnection->hRoamMng, &reportStatus);   
    }
    pAPConnection->firstAttempt2Roam = TI_TRUE;
    pAPConnection->roamReason = ROAMING_TRIGGER_NONE;
    pAPConnection->removeKeys = TI_TRUE;
    pAPConnection->sendDeauthPacket = TI_TRUE;
    pAPConnection->reNegotiateTSPEC = TI_FALSE;
    pAPConnection->voiceTspecConfigured = TI_FALSE;
	pAPConnection->videoTspecConfigured = TI_FALSE;
}


/**
*
* apConn_smConnectedToNewAP
*
* \b Description: 
*
* After roaming was requested, Connection SM informs AP Connection module about
* successful link re-establishment; start waiting for roaming triggers
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smConnectedToNewAP(void *pData)
{
    apConn_t    *pAPConnection;
    apConn_connStatus_t reportStatus; 
    paramInfo_t param;

    pAPConnection = (apConn_t *)pData;
    
    /* Configure SCR group back to connection */
    scr_setGroup (pAPConnection->hScr, SCR_GID_CONNECTED);

    /* Erase vendor specific info-element if was defined for last AP Assoc request */
    pAPConnection->vsIElength = 0;

    /* TBD Notify Curr BSS module about update of current AP database */

    if (pAPConnection->roamingFailedHandoverNum>0)
    {
        pAPConnection->roamingFailedHandoverNum--;
    }
    /* Report Roaming Manager */
    if (pAPConnection->reportStatusCallb != NULL)
    {
        param.paramType   = ASSOC_ASSOCIATION_REQ_PARAM;

        assoc_getParam(pAPConnection->hAssoc, &param);
        reportStatus.dataBuf = (char *)(param.content.assocReqBuffer.buffer);
        reportStatus.dataBufLength = param.content.assocReqBuffer.bufferSize;

        reportStatus.status = CONN_STATUS_HANDOVER_SUCCESS;

        pAPConnection->reportStatusCallb(pAPConnection->hRoamMng, &reportStatus);   
    }
    pAPConnection->firstAttempt2Roam = TI_TRUE;
    pAPConnection->roamReason = ROAMING_TRIGGER_NONE;
    pAPConnection->roamingSuccesfulHandoverTotalNum++;
    pAPConnection->removeKeys = TI_TRUE;
    pAPConnection->sendDeauthPacket = TI_TRUE;
    pAPConnection->reNegotiateTSPEC = TI_FALSE;
    pAPConnection->voiceTspecConfigured = TI_FALSE;
	pAPConnection->videoTspecConfigured = TI_FALSE;

    
    if (!pAPConnection->resetReportedRoamingStatistics)
    {
        pAPConnection->roamingSuccesfulHandoverNum++;
        pAPConnection->lastRoamingDelay = 
	        (TI_UINT16)os_timeStampMs(pAPConnection->hOs) - pAPConnection->roamingStartedTimestamp;		  
    }
    else
    {
        pAPConnection->resetReportedRoamingStatistics = TI_FALSE;
    }

    /* Raise event of Roaming Completion */
    TRACE0(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "EvHandlerSendEvent -ROAMING_COMPLETE\n");
    EvHandlerSendEvent(pAPConnection->hEvHandler, IPC_EVENT_ROAMING_COMPLETE, NULL, 0);
}


/**
*
* apConn_smStopConnection
*
* \b Description: 
*
* Stop required before roaming was started
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smStopConnection(void *pData)
{
    apConn_t *pAPConnection;
    DisconnectType_e disConnType;
    pAPConnection = (apConn_t *)pData;
    
    TRACE2(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, "apConn_smStopConnection, calls conn_stop, removeKeys=%d, sendDeauthPacket=%d \n", pAPConnection->removeKeys, pAPConnection->sendDeauthPacket);
    
    /* Erase vendor specific info-element if was defined for last AP Assoc request */
    pAPConnection->vsIElength = 0;
    
    /* In case AP connection was stopped by SME, and radioOn is false, meaning immidiate shutdown is required without disassoc frame */
    /* Otherwise, ask for normal disconnection with disassoc frame */
    disConnType = (pAPConnection->sendDeauthPacket == TI_TRUE) ? DISCONNECT_DE_AUTH : DISCONNECT_IMMEDIATE;

    

    /* set the SCr group to connecting */
    scr_setGroup (pAPConnection->hScr, SCR_GID_CONNECT);
    
	/* Stop Connection state machine - always immediate TBD */
	conn_stop(pAPConnection->hConnSm, 
			  disConnType,
			  (mgmtStatus_e)pAPConnection->deauthPacketReasonCode,
			  pAPConnection->removeKeys, /* for Roaming, do not remove the keys */
			  apConn_DisconnCompleteInd,
			  pAPConnection);
	
	pAPConnection->deauthPacketReasonCode = STATUS_UNSPECIFIED;
}


/**
*
* apConn_smReportDisconnected
*
* \b Description: 
*
* Moving from 'Disconnecting' state to 'Idle':
*   RoamMgr.status("not-connected")
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smReportDisconnected(void *pData)
{
    apConn_t    *pAPConnection;
    apConn_connStatus_t reportStatus; 

    pAPConnection = (apConn_t *)pData;

    if (pAPConnection->reportStatusCallb != NULL) 
    {
        reportStatus.status = CONN_STATUS_NOT_CONNECTED;
        pAPConnection->reportStatusCallb(pAPConnection->hRoamMng, &reportStatus);   
    }
    
    pAPConnection->firstAttempt2Roam = TI_TRUE;

    /* Notify SME */
    apConn_reportConnStatusToSME(pAPConnection);
}


/**
*
* apConn_smRetainAP
*
* \b Description: 
*
* Roaming Manager gives up on roaming.
* Moving from 'Wait for connection command' back to 'Wait for roam started.
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smRetainAP(void *data)
{
    apConn_t    *pAPConnection;
    apConn_connStatus_t reportStatus; 
    paramInfo_t param;

    pAPConnection = (apConn_t *)data;

    /* Configure SCR group back to connection */
    scr_setGroup (pAPConnection->hScr, SCR_GID_CONNECTED);

    /* Report Roaming Manager */
    if (pAPConnection->reportStatusCallb != NULL) 
    {
        param.paramType   = ASSOC_ASSOCIATION_REQ_PARAM;

        assoc_getParam(pAPConnection->hAssoc, &param);
        reportStatus.dataBuf = (char *)(param.content.assocReqBuffer.buffer);
        reportStatus.dataBufLength = param.content.assocReqBuffer.bufferSize;

        reportStatus.status = CONN_STATUS_HANDOVER_SUCCESS;

        pAPConnection->reportStatusCallb(pAPConnection->hRoamMng, &reportStatus);   
    }
    pAPConnection->retainCurrAPNum++;

    pAPConnection->roamReason = ROAMING_TRIGGER_NONE;
    pAPConnection->removeKeys = TI_TRUE;
    pAPConnection->sendDeauthPacket = TI_TRUE;
    pAPConnection->reNegotiateTSPEC = TI_FALSE;
    pAPConnection->voiceTspecConfigured = TI_FALSE;
	pAPConnection->videoTspecConfigured = TI_FALSE;

}


/**
*
* apConn_smRequestCCKM
*
* \b Description: 
*
* Roaming Manager requests to roaming.
* Get CCKM (prepare hand-off).
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smRequestCCKM(void *data)
{
    apConn_t    *pAPConnection;
    pAPConnection = (apConn_t *)data;
	
#ifdef XCC_MODULE_INCLUDED
        /* Send request to XCC module */

    apConn_calcNewTsf(pAPConnection, (TI_UINT8 *)&(pAPConnection->newAP->lastRxTSF), pAPConnection->newAP->lastRxHostTimestamp, pAPConnection->newAP->beaconInterval);
    XCCMngr_startCckm(pAPConnection->hXCCMngr, &(pAPConnection->newAP->BSSID), (TI_UINT8 *)&(pAPConnection->newAP->lastRxTSF));
#else
    apConn_RoamHandoffFinished(pAPConnection);
#endif
}


#ifdef XCC_MODULE_INCLUDED
/**
*
* calcNewTsfTimestamp - Calculates the TSF
*
* \b Description: 
*
* Calculates the TSF according to the delta of the TSF from the last Beacon/Probe Resp and the current time.
*
* \b ARGS:
*
*  I   - hRoamingMngr - pointer to the roamingMngr SM context  \n
*  I/O - tsfTimeStamp - the TSF field in the site entry of the roaming candidate AP
*  I   - newSiteOsTimeStamp - the TS field in the site entry of the roaming candidate AP
*
* \b RETURNS:
*
*  Nothing.
*
* 
*/
static void apConn_calcNewTsf(apConn_t *hAPConnection, TI_UINT8 *tsfTimeStamp, TI_UINT32 newSiteOsTimeStamp, TI_UINT32 beaconInterval)
{
    apConn_t    *pAPConnection = hAPConnection;
    TI_UINT32      osTimeStamp = os_timeStampMs(pAPConnection->hOs);
    TI_UINT32      deltaTimeStamp; 
    TI_UINT32      tsfLsdw,tsfMsdw, newOsTimeStamp; 
    TI_UINT32      remainder;
    TI_UINT8       newTsfTimestamp[TIME_STAMP_LEN];

    /* get the delta TS between the TS of the last Beacon/ProbeResp-from the site table
    and the current TS */
    deltaTimeStamp = osTimeStamp - newSiteOsTimeStamp;
    tsfLsdw = *((TI_UINT32*)&tsfTimeStamp[0]); 
    tsfMsdw = *((TI_UINT32*)&tsfTimeStamp[4]);
    
    TRACE2(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, " TSF time LSDW reversed=%x, TSF time MSDW reversed=%x\n", tsfLsdw, tsfMsdw);

    deltaTimeStamp = deltaTimeStamp*1000;/* from mSec to uSec*/
    /* Before adding, save remainder */
    remainder = tsfTimeStamp[3] + ((deltaTimeStamp & 0xff000000) >> 24);

    /* The LS DW of the TS is the TSF taken from the last Beacon/Probe Resp
        + the delta TS from the time the Beacon/Probe Resp arrive till now. */
    newOsTimeStamp = deltaTimeStamp+tsfLsdw;

    /* substracting one beacon interval */
    newOsTimeStamp -= (beaconInterval * 1024); /* im usec */

    /* save just for debug printout */
    deltaTimeStamp +=osTimeStamp; /* uMsec */
    /* update the LSB of the TSF */
    newTsfTimestamp[0] = newOsTimeStamp & 0x000000ff;
    newTsfTimestamp[1] = (newOsTimeStamp & 0x0000ff00) >> 8;
    newTsfTimestamp[2] = (newOsTimeStamp & 0x00ff0000) >> 16;
    newTsfTimestamp[3] = (newOsTimeStamp & 0xff000000) >> 24;

    /* increase the MSB in case of overflow */
    if (remainder > 0xff)
    {
        tsfMsdw++;
        
    }
    /* update the MSB of the TSF */
    newTsfTimestamp[4] = tsfMsdw & 0x000000ff;
    newTsfTimestamp[5] = (tsfMsdw & 0x0000ff00) >> 8;
    newTsfTimestamp[6] = (tsfMsdw & 0x00ff0000) >> 16;
    newTsfTimestamp[7] = (tsfMsdw & 0xff000000) >> 24;

    TRACE11(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, " NEW TSF time: reversedTsfTimeStamp= 0x%x, New deltaTimeStamp= 0x%x, \n remainder=0x%x, new tsfTimeStamp=%x-%x-%x-%x-%x-%x-%x-%x\n", newOsTimeStamp, deltaTimeStamp, remainder, newTsfTimestamp[0], newTsfTimestamp[1], newTsfTimestamp[2], newTsfTimestamp[3], newTsfTimestamp[4], newTsfTimestamp[5], newTsfTimestamp[6], newTsfTimestamp[7]);

    os_memoryCopy(pAPConnection->hOs, tsfTimeStamp, newTsfTimestamp, TIME_STAMP_LEN);
}
#endif


/**
*
* apConn_invokeConnectionToNewAp
*
* \b Description: 
*
* Got CCKM (hand-off), start re-connection to another AP
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smInvokeConnectionToNewAp(void *data)
{
    apConn_t    *pAPConnection;
    EConnType    connType;
    paramInfo_t param;
    TI_UINT8     staPrivacySupported, apPrivacySupported;
    TI_BOOL      renegotiateTspec = TI_FALSE;
    
	pAPConnection = (apConn_t *)data;

	pAPConnection->roamingStartedTimestamp = os_timeStampMs(pAPConnection->hOs);

    /* Check privacy compatibility */
    param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
    rsn_getParam(pAPConnection->hPrivacy, &param);

    staPrivacySupported = (param.content.rsnEncryptionStatus == TWD_CIPHER_NONE) ? TI_FALSE : TI_TRUE;
    apPrivacySupported  = ((pAPConnection->newAP->capabilities >> CAP_PRIVACY_SHIFT) & CAP_PRIVACY_MASK) ? TI_TRUE : TI_FALSE;

#ifdef GEM_SUPPORTED
	// For GEM – ignore the privacy bit of the AP. Some GEM AP’s don’t turn on the privacy
 if ((staPrivacySupported != apPrivacySupported) && (param.content.rsnEncryptionStatus != TWD_CIPHER_GEM))
#else 
    if (staPrivacySupported != apPrivacySupported)
#endif
    {
        param.paramType = RSN_MIXED_MODE;
        rsn_getParam(pAPConnection->hPrivacy, &param);

        if (apPrivacySupported ||
            (!param.content.rsnMixedMode && staPrivacySupported))
        {
            TRACE2(pAPConnection->hReport, REPORT_SEVERITY_WARNING, ": failed privacy comparison %d vs. %d\n", staPrivacySupported, apPrivacySupported);
            apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_NOT_OK, pAPConnection);
            return;
        }
    }

    /* Update data info of desired AP; in case of first attempt to roam,
       store previous primary site info */
    if (siteMgr_overwritePrimarySite(pAPConnection->hSiteMgr, pAPConnection->newAP, pAPConnection->firstAttempt2Roam) != TI_OK)
    {
        TRACE0(pAPConnection->hReport, REPORT_SEVERITY_WARNING, ": failed to ovewrite Primary Site\n");
        apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_NOT_OK, pAPConnection);
        return;
    }

    /* Update re-associate parameter of MLME */
    if (pAPConnection->requestType == AP_CONNECT_FAST_TO_AP || pAPConnection->requestType == AP_CONNECT_RECONNECT_CURR_AP)
    {
        connType = CONN_TYPE_ROAM;
    }
    else
    {
        connType = CONN_TYPE_FIRST_CONN;
    }

#ifdef XCC_MODULE_INCLUDED
    /* Check the need in TSPEC re-negotiation */
    if ( (pAPConnection->voiceTspecConfigured || pAPConnection->videoTspecConfigured) && pAPConnection->reNegotiateTSPEC )
    {
        /* If the candidate AP is at least XCCver4 AP, try to re-negotiate TSPECs */
        if (XCCMngr_parseXCCVer(pAPConnection->hXCCMngr, 
                                pAPConnection->newAP->pBuffer, 
                                pAPConnection->newAP->bufferLength) >= 4)
        {
            renegotiateTspec = TI_TRUE;
        }
    }
#endif

    TRACE2(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, ": calls conn_start, removeKeys=%d, renegotiateTSPEC=%d\n", pAPConnection->removeKeys, renegotiateTspec);

    /* Start Connection state machine */
    conn_start(pAPConnection->hConnSm, 
                      connType,
                      apConn_ConnCompleteInd,
                      pAPConnection,
                      pAPConnection->removeKeys,
                      renegotiateTspec);
}


/**
*
* apConn_smReportConnFail
*
* \b Description: 
*
* Got 'Failed' indication from Connection state machine - inform Roaming Manager Module
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smReportConnFail(void *data)
{
    apConn_t *pAPConnection;
    apConn_connStatus_t reportStatus; 
    paramInfo_t param;

    pAPConnection = (apConn_t *)data;

    pAPConnection->firstAttempt2Roam = TI_FALSE;
    pAPConnection->resetReportedRoamingStatistics = TI_FALSE;

    /* Erase vendor specific info-element if was defined for last AP Assoc request */
    pAPConnection->vsIElength = 0;

    /* Report to Roaming Manager */
    if (pAPConnection->reportStatusCallb != NULL)
    {
        param.paramType   = ASSOC_ASSOCIATION_REQ_PARAM;

        assoc_getParam(pAPConnection->hAssoc, &param);
        reportStatus.dataBuf = (char *)(param.content.assocReqBuffer.buffer);
        reportStatus.dataBufLength = param.content.assocReqBuffer.bufferSize;

        reportStatus.status = CONN_STATUS_HANDOVER_FAILURE;

        pAPConnection->reportStatusCallb(pAPConnection->hRoamMng, &reportStatus);   
    }
}


/**
*
* apConn_smConfigureDriverBeforeRoaming
*
* \b Description: 
*
* Got 'Failed' indication from Connection state machine - inform Roaming Manager Module
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smConfigureDriverBeforeRoaming(void *pData)
{
    apConn_t    *pAPConnection = (apConn_t*)pData;
    paramInfo_t param;
	
    /* Configure SCR group of allowed clients according to 'Roaming' rule */
    scr_setGroup (pAPConnection->hScr, SCR_GID_ROAMING);
    param.paramType = QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC;
    qosMngr_getParams(pAPConnection->hQos, &param);
    pAPConnection->voiceTspecConfigured = param.content.TspecConfigure.voiceTspecConfigure;    
    pAPConnection->videoTspecConfigured = param.content.TspecConfigure.videoTspecConfigure;  
    pAPConnection->resetReportedRoamingStatistics = TI_FALSE;
}

/**
*
* apConn_smSwChFinished
*
* \b Description: 
*
* Switch channel completed; if there were roaming Manager triggers meanwhile, 
* inform Roaming Manager Module
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smSwChFinished(void *pData)
{
    apConn_t *pAPConnection = (apConn_t *)pData;

    /* Inform Current BSS module */
    currBSS_restartRssiCounting(pAPConnection->hCurrBSS);

    /* If there are unreported roaming triggers of 'No AP' type, 
       report them now to roaming manager */
    if (pAPConnection->roamReason >= ROAMING_TRIGGER_MAX_TX_RETRIES)
    {
        if ((pAPConnection->roamingEnabled == TI_TRUE) && 
            (pAPConnection->roamEventCallb != NULL))
        {
            /* Report to Roaming Manager */
            pAPConnection->roamEventCallb(pAPConnection->hRoamMng, &pAPConnection->roamReason, (TI_UINT16)0);
        }
    }
    else
    {
        pAPConnection->roamReason = ROAMING_TRIGGER_NONE;
    }
}


/**
*
* apConn_smHandleTspecReneg
*
* \b Description: 
*
* This function will be called when moving from CONNECTING state to 
* START_TSPEC_RENEGOTIATION state. It checks if TSPEC re-negotiation was requested 
* by roaming manager, if the TSPEC for voice was defined by user application, 
* if the re-negotiation was performed during hand-over. 
* If so, it will trigger moving to WAIT_ROAM state, otherwise it will start 
* TSPEC negotiation, staying in the REESTABLISHING_VOICE state and waiting 
* for results.
*
* \b ARGS:
*
*  I   - pData - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static void apConn_smHandleTspecReneg (void *pData)
{
    apConn_t *pAPConnection = (apConn_t *)pData;
    paramInfo_t param;

    if (pAPConnection->voiceTspecConfigured
#ifndef XCC_MODULE_INCLUDED
          && pAPConnection->reNegotiateTSPEC
#endif
        )
        {
#ifndef XCC_MODULE_INCLUDED        
        param.paramType = QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC;
        qosMngr_getParams(pAPConnection->hQos, &param);

        if (param.content.TspecConfigure.voiceTspecConfigure == TI_TRUE)
        {
            /* TSPEC is already configured, move to CONNECTED */
            apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_OK, pAPConnection);
        }
        else
#endif
        {
            param.paramType = QOS_MNGR_RESEND_TSPEC_REQUEST;
            param.content.qosRenegotiateTspecRequest.callback = (void *)apConn_qosMngrReportResultCallb;
            param.content.qosRenegotiateTspecRequest.handler = pData; 

            if (qosMngr_setParams(pAPConnection->hQos, &param) != TI_OK)
            {
                /* Re-negotiation of TSPEC cannot be performed */
                apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_NOT_OK, pAPConnection);
            }
        }
    }
    else
    {
        /* No need to re-negotiate TSPEC, move to CONNECTED */
        apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_OK, pAPConnection);
    }
	return;
}


/**
*
* apConn_qosMngrReportResultCallb
*
* \b Description: 
*
* This function will be transferred to QoS manager upon request to start negotiation
* of the TSPEC for voice and signaling, and will be called when the voice TSPEC
* renegotiation is completed. The function will generate FINISHED_OK or 
* FINISHED_NOK events to the state machine of AP Connection, triggering change of 
* the current state.
*
* \b ARGS:
*
*  I   - hApConn - pointer to AP Connection context\n
*  I   - result - returned by Traffic admission control\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
static TI_STATUS apConn_qosMngrReportResultCallb (TI_HANDLE hApConn, trafficAdmRequestStatus_e result)
{
    apConn_t *pAPConnection = (apConn_t *)hApConn;

    AP_CONN_VALIDATE_HANDLE(hApConn);

    if (result == STATUS_TRAFFIC_ADM_REQUEST_ACCEPT) 
    {
        apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_OK, pAPConnection);
    }
    else
    {
        apConn_smEvent(&(pAPConnection->currentState), AP_CONNECT_EVENT_FINISHED_NOT_OK, pAPConnection);
    }
    return TI_OK;
}

/**
*
* apConn_reportConnStatusToSME
*
* \b Description: 
*
* Sends report to SME regarding the connection status
*
* \b ARGS:
*
*  I   - pAPConnection  - pointer to AP Connection context\n
*
* \b RETURNS:
*
*  OK on success, NOK otherwise.
*
* \sa 
*/
static void apConn_reportConnStatusToSME (apConn_t *pAPConnection)
{
	
TRACE3(pAPConnection->hReport, REPORT_SEVERITY_INFORMATION, " roamingTrigger = %d, APDisconnectStatusCode = %d, bNonRoamingDisAssocReason = %d\n", pAPConnection->roamReason, pAPConnection->APDisconnect.uStatusCode, 		pAPConnection->bNonRoamingDisAssocReason);

	/* Check if an outside reason caused the disconnection. */
	if (pAPConnection->bNonRoamingDisAssocReason)
	{
		pAPConnection->bNonRoamingDisAssocReason = TI_FALSE;
		sme_ReportApConnStatus(pAPConnection->hSme, STATUS_UNSPECIFIED, 0);
	}
    /* DisAssociation happened due to roaming trigger */
	else if (pAPConnection->roamReason == ROAMING_TRIGGER_AP_DISCONNECT)
	{	/* AP disconnect is a special case of the status delivered to SME */
		mgmtStatus_e mgmtStatus = ( pAPConnection->APDisconnect.bDeAuthenticate ? STATUS_AP_DEAUTHENTICATE : STATUS_AP_DISASSOCIATE );
		sme_ReportApConnStatus(pAPConnection->hSme, mgmtStatus, pAPConnection->APDisconnect.uStatusCode);
	} 
	else	/* Finally, just send the last roaming trigger */
	{	
		sme_ReportApConnStatus(pAPConnection->hSme, STATUS_ROAMING_TRIGGER, (TI_UINT32)pAPConnection->roamReason);
	}
}


void apConn_setDeauthPacketReasonCode(TI_HANDLE hAPConnection, TI_UINT8 deauthReasonCode)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

	pAPConnection->deauthPacketReasonCode = deauthReasonCode;
	pAPConnection->roamReason = ROAMING_TRIGGER_SECURITY_ATTACK;
}

TI_STATUS apConn_getAssocRoamingTrigger(TI_HANDLE hAPConnection, apConn_roamingTrigger_e *asssocRoamingTrigger)
{
    apConn_t *pAPConnection = (apConn_t *)hAPConnection;

	*asssocRoamingTrigger = pAPConnection->assocRoamingTrigger;

	return TI_OK;
}
