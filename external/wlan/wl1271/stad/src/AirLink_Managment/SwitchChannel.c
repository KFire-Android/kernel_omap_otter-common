/*
 * SwitchChannel.c
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

/** \file SwitchChannel.c
 *  \brief SwitchChannel module interface
 *
 *  \see SwitchChannelApi.h
 */

/****************************************************************************************************/
/*                                                                                                  */
/*      MODULE:     SwitchChannel.c                                                                         */
/*      PURPOSE:    SwitchChannel module interface.                                                         */
/*                  This module perform SwitchChannel (Dynamic Frequency Selection)                         */
/*                      according to AP command. The object responsibles for switching channel after*/
/*                      the requires time and quieting the channel for the required duration        */
/*                      time.                                                                       */
/****************************************************************************************************/

#define __FILE_ID__  FILE_ID_7
#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "paramOut.h"
#include "SwitchChannelApi.h"
#include "DataCtrl_Api.h"
#include "regulatoryDomainApi.h"
#include "apConn.h"
#include "siteMgrApi.h"
#include "PowerMgr_API.h"
#include "healthMonitor.h"
#include "TWDriver.h"
#include "DrvMainModules.h"

/* allocation vector */
#define SC_INIT_BIT                     (1)
#define SC_SM_INIT_BIT                  (2)

#define SC_SWITCH_CHANNEL_CMD_LEN               3
#define SC_SWITCH_CHANNEL_MODE_NOT_TX_SUS       0
#define SC_SWITCH_CHANNEL_MODE_TX_SUS           1


#define SC_CHANNEL_INVALID          TI_FALSE
#define SC_CHANNEL_VALID            TI_TRUE

/* Enumerations */

/** state machine states */
typedef enum 
{
    SC_STATE_IDLE           = 0,
    SC_STATE_WAIT_4_CMD     = 1,
    SC_STATE_WAIT_4_SCR     = 2,
    SC_STATE_SC_IN_PROG     = 3,
    SC_STATE_LAST           = 4
} switchChannel_smStates;

/** State machine events */
typedef enum 
{
    SC_EVENT_START          = 0,
    SC_EVENT_STOP           = 1,
    SC_EVENT_SC_CMD         = 2,
    SC_EVENT_SCR_RUN        = 3,
    SC_EVENT_SCR_FAIL       = 4,
    SC_EVENT_SC_CMPLT       = 5,
    SC_EVENT_FW_RESET       = 6,
    SC_EVENT_LAST           = 7
} switchChannel_smEvents;


#define SC_NUM_STATES       SC_STATE_LAST    
#define SC_NUM_EVENTS       SC_EVENT_LAST   


/* Structures */
typedef struct 
{

    /* SwitchChannel parameters that can be configured externally */
    TI_BOOL            dot11SpectrumManagementRequired;

    /* Internal SwitchChannel parameters */
    TI_UINT8                   currentState;
    dot11_CHANNEL_SWITCH_t  curChannelSwitchCmdParams;
    TI_UINT32                   SCRRequestTimestamp;
    TI_UINT8                    currentChannel;
    TI_BOOL                 switchChannelStarted;

#ifdef TI_DBG
    /* switchChannelCmd for debug */
    dot11_CHANNEL_SWITCH_t  debugChannelSwitchCmdParams;
    TI_UINT8                    ignoreCancelSwitchChannelCmd;
#endif
    
    /* SwitchChannel SM */
    fsm_stateMachine_t          *pSwitchChannelSm;

    /* SwitchChannel handles to other objects */                                
    TI_HANDLE       hTWD;
    TI_HANDLE       hSiteMgr;
    TI_HANDLE       hSCR;
    TI_HANDLE       hRegulatoryDomain;
    TI_HANDLE       hPowerMngr;
    TI_HANDLE       hApConn;
    TI_HANDLE       hReport;
    TI_HANDLE       hOs;

} switchChannel_t;




/* External data definitions */

/* Local functions definitions */

/* Global variables */


/********************************************************************************/
/*                      Internal functions prototypes.                          */
/********************************************************************************/


/* SM functions */
static TI_STATUS switchChannel_smStartSwitchChannelCmd(void *pData);
static TI_STATUS switchChannel_smReqSCR_UpdateCmd(void *pData);
static TI_STATUS switchChannel_smSwitchChannelCmplt(void *pData);
static TI_STATUS switchChannel_smFwResetWhileSCInProg(void *pData);
static TI_STATUS switchChannel_smScrFailWhileWait4Scr(void *pData);
static TI_STATUS switchChannel_smNop(void *pData);
static TI_STATUS switchChannel_smUnexpected(void *pData);
static TI_STATUS switchChannel_smStopWhileWait4Cmd(void *pData);
static TI_STATUS switchChannel_smStopWhileWait4Scr(void *pData);
static TI_STATUS switchChannel_smStopWhileSwitchChannelInProg(void *pData);
static TI_STATUS switchChannel_smStart(void *pData);


/* other functions */
static void release_module(switchChannel_t *pSwitchChannel, TI_UINT32 initVec);
static TI_STATUS switchChannel_smEvent(TI_UINT8 *currState, TI_UINT8 event, void* data);
static void switchChannel_zeroDatabase(switchChannel_t *pSwitchChannel);
void switchChannel_SwitchChannelCmdCompleteReturn(TI_HANDLE hSwitchChannel);
void switchChannel_scrStatusCB(TI_HANDLE hSwitchChannel, EScrClientRequestStatus requestStatus, 
                               EScrResourceId eResource, EScePendReason pendReason );
#ifdef TI_DBG
static void switchChannel_recvCmd4Debug(TI_HANDLE hSwitchChannel, dot11_CHANNEL_SWITCH_t *channelSwitch, TI_BOOL BeaconPacket, TI_UINT8 channel);
#endif


/********************************************************************************/
/*                      Interface functions Implementation.                     */
/********************************************************************************/


/************************************************************************
 *                        switchChannel_create                          *
 ************************************************************************/
/**
*
* \b Description: 
*
* This procedure is called by the config manager when the driver is created.
* It creates the SwitchChannel object.
*
* \b ARGS:
*
*  I - hOs - OS context \n
*
* \b RETURNS:
*
*  Handle to the SwitchChannel object.
*
* \sa 
*/
TI_HANDLE switchChannel_create(TI_HANDLE hOs)
{
    switchChannel_t           *pSwitchChannel = NULL;
    TI_UINT32          initVec = 0;
    TI_STATUS       status;

    /* allocating the SwitchChannel object */
    pSwitchChannel = os_memoryAlloc(hOs,sizeof(switchChannel_t));

    if (pSwitchChannel == NULL)
        return NULL;

    initVec |= (1 << SC_INIT_BIT);

    os_memoryZero(hOs, pSwitchChannel, sizeof(switchChannel_t));

    pSwitchChannel->hOs = hOs;

    status = fsm_Create(hOs, &pSwitchChannel->pSwitchChannelSm, SC_NUM_STATES, SC_NUM_EVENTS);
    if (status != TI_OK)
    {
        release_module(pSwitchChannel, initVec);
        WLAN_OS_REPORT(("FATAL ERROR: switchChannel_create(): Error Creating pSwitchChannelSm - Aborting\n"));
        return NULL;
    }
    initVec |= (1 << SC_SM_INIT_BIT);

    return(pSwitchChannel);
}

/************************************************************************
 *                        switchChannel_init                            *
 ************************************************************************/
/**
*
* \b Description: 
*
* This procedure is called by the DrvMain when the driver is initialized.
* It initializes the SwitchChannel object's variables and handlers and creates the SwitchChannel SM.
*
* \b ARGS:
*
*  I - pStadHandles  - The driver modules handles \n
*
* \b RETURNS:
*
*  void
*
* \sa 
*/
void switchChannel_init (TStadHandlesList *pStadHandles)
{
    switchChannel_t *pSwitchChannel = (switchChannel_t *)(pStadHandles->hSwitchChannel);

    /** Roaming State Machine matrix */
    fsm_actionCell_t    switchChannel_SM[SC_NUM_STATES][SC_NUM_EVENTS] =
    {
        /* next state and actions for IDLE state */
        {   {SC_STATE_WAIT_4_CMD, switchChannel_smStart},               /* START        */
            {SC_STATE_IDLE, switchChannel_smNop},                       /* STOP         */ 
            {SC_STATE_IDLE, switchChannel_smNop},                       /* SC_CMD      */
            {SC_STATE_IDLE, switchChannel_smUnexpected},                /* SCR_RUN      */ 
            {SC_STATE_IDLE, switchChannel_smUnexpected},                /* SCR_FAIL     */
            {SC_STATE_IDLE, switchChannel_smUnexpected},                /* SC_CMPLT    */
            {SC_STATE_IDLE, switchChannel_smUnexpected}                 /* FW_RESET     */
        },                                                                              

        /* next state and actions for WAIT_4_CMD state */    
        {   {SC_STATE_WAIT_4_CMD, switchChannel_smNop},                     /* START        */
            {SC_STATE_IDLE, switchChannel_smStopWhileWait4Cmd},             /* STOP         */ 
            {SC_STATE_WAIT_4_SCR, switchChannel_smReqSCR_UpdateCmd},        /* SC_CMD      */
            {SC_STATE_WAIT_4_CMD, switchChannel_smUnexpected},              /* SCR_RUN      */ 
            {SC_STATE_WAIT_4_CMD, switchChannel_smUnexpected},              /* SCR_FAIL     */
            {SC_STATE_WAIT_4_CMD, switchChannel_smUnexpected},              /* SC_CMPLT    */
            {SC_STATE_WAIT_4_CMD, switchChannel_smUnexpected}               /* FW_RESET    */

        },                                                                              

        /* next state and actions for WAIT_4_SCR state */    
        {   {SC_STATE_WAIT_4_SCR, switchChannel_smUnexpected},                  /* START        */
            {SC_STATE_IDLE, switchChannel_smStopWhileWait4Scr},                 /* STOP         */
            {SC_STATE_WAIT_4_SCR, switchChannel_smNop},                         /* SC_CMD      */
            {SC_STATE_SC_IN_PROG, switchChannel_smStartSwitchChannelCmd},       /* SCR_RUN      */
            {SC_STATE_WAIT_4_CMD, switchChannel_smScrFailWhileWait4Scr},        /* SCR_FAIL    */
            {SC_STATE_WAIT_4_SCR, switchChannel_smUnexpected} ,                 /* SC_CMPLT    */
            {SC_STATE_WAIT_4_CMD, switchChannel_smUnexpected}                   /* FW_RESET    */

        },                                                                              

        /* next state and actions for switchChannel_IN_PROG state */
        {   {SC_STATE_SC_IN_PROG, switchChannel_smUnexpected},                   /* START        */
            {SC_STATE_IDLE, switchChannel_smStopWhileSwitchChannelInProg},       /* STOP         */
            {SC_STATE_SC_IN_PROG, switchChannel_smNop},                          /* SC_CMD      */
            {SC_STATE_SC_IN_PROG, switchChannel_smUnexpected},                   /* SCR_RUN      */
            {SC_STATE_WAIT_4_CMD, switchChannel_smUnexpected},                   /* SCR_FAIL    */
            {SC_STATE_WAIT_4_CMD, switchChannel_smSwitchChannelCmplt},           /* SC_CMPLT    */
            {SC_STATE_WAIT_4_CMD, switchChannel_smFwResetWhileSCInProg}          /* FW_RESET    */
        }                                                                             
    }; 

    fsm_Config(pSwitchChannel->pSwitchChannelSm, 
            &switchChannel_SM[0][0], 
            SC_NUM_STATES, 
            SC_NUM_EVENTS, 
            switchChannel_smEvent, pSwitchChannel->hOs);

    /* init handlers */
    pSwitchChannel->hTWD                = pStadHandles->hTWD;
    pSwitchChannel->hSiteMgr            = pStadHandles->hSiteMgr;
    pSwitchChannel->hSCR                = pStadHandles->hSCR;
    pSwitchChannel->hRegulatoryDomain   = pStadHandles->hRegulatoryDomain;
    pSwitchChannel->hApConn             = pStadHandles->hAPConnection;
    pSwitchChannel->hReport             = pStadHandles->hReport;
    pSwitchChannel->hOs                 = pStadHandles->hOs;
}


TI_STATUS switchChannel_SetDefaults (TI_HANDLE hSwitchChannel, SwitchChannelInitParams_t *SwitchChannelInitParams)
{   
    switchChannel_t       *pSwitchChannel = (switchChannel_t *)hSwitchChannel;

    /* init variables */
    pSwitchChannel->dot11SpectrumManagementRequired = SwitchChannelInitParams->dot11SpectrumManagementRequired;
    pSwitchChannel->currentState = SC_STATE_IDLE;
    pSwitchChannel->currentChannel = 0;
    pSwitchChannel->switchChannelStarted = TI_FALSE;

    /* register to SCR */
    scr_registerClientCB(pSwitchChannel->hSCR, SCR_CID_SWITCH_CHANNEL, 
                         switchChannel_scrStatusCB, pSwitchChannel);

    /* register to Switch Channel Complete event in HAL */
    TWD_RegisterEvent (pSwitchChannel->hTWD, 
                       TWD_OWN_EVENT_SWITCH_CHANNEL_CMPLT, 
                       (void *)switchChannel_SwitchChannelCmdCompleteReturn, 
                       pSwitchChannel);

    TWD_EnableEvent (pSwitchChannel->hTWD, TWD_OWN_EVENT_SWITCH_CHANNEL_CMPLT);
#ifdef TI_DBG
    /* for debug */
    pSwitchChannel->debugChannelSwitchCmdParams.hdr[0] = CHANNEL_SWITCH_ANNOUNCEMENT_IE_ID;
    pSwitchChannel->debugChannelSwitchCmdParams.hdr[1] = SC_SWITCH_CHANNEL_CMD_LEN;
    pSwitchChannel->ignoreCancelSwitchChannelCmd = 0;
#endif
    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INIT, ".....SwitchChannel configured successfully\n");

    return TI_OK;
}


/************************************************************************
 *                        switchChannel_stop                            *
 ************************************************************************/
/**
*
* \b Description: 
*
* This procedure is called by the SME when the state is changed from CONNECTED.
* It generates a STOP event to the SwitchChannel SM.
*
* \b ARGS:
*
*  I - hSwitchChannel - SwitchChannel context \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise
*
* \sa 
*/
TI_STATUS switchChannel_stop(TI_HANDLE hSwitchChannel)
{
    switchChannel_t *pSwitchChannel = (switchChannel_t *)hSwitchChannel;

    pSwitchChannel->switchChannelStarted = TI_FALSE;
    return (switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_STOP, pSwitchChannel));

}

/************************************************************************
 *                        switchChannel_start                           *
 ************************************************************************/
/**
*
* \b Description: 
*
* This procedure is called by the SME when the state is changed to CONNECTED.
* It generates a START event to the SwitchChannel SM.
*
* \b ARGS:
*
*  I - hSwitchChannel - SwitchChannel context \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise
*
* \sa 
*/
TI_STATUS switchChannel_start(TI_HANDLE hSwitchChannel)
{
    switchChannel_t *pSwitchChannel = (switchChannel_t *)hSwitchChannel;
    pSwitchChannel->switchChannelStarted = TI_TRUE;

    return (switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_START, pSwitchChannel));

}


/************************************************************************
 *                        switchChannel_unload                          *
 ************************************************************************/
/**
*
* \b Description: 
*
* This procedure is called by the config manager when the driver is unloaded.
* It frees any memory allocated and timers.
*
* \b ARGS:
*
*  I - hSwitchChannel - SwitchChannel context \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise
*
* \sa 
*/
TI_STATUS switchChannel_unload(TI_HANDLE hSwitchChannel)
{
    TI_UINT32              initVec;
    switchChannel_t *pSwitchChannel = (switchChannel_t *)hSwitchChannel;

    if (pSwitchChannel == NULL)
        return TI_OK;

    initVec = 0xff;
    release_module(pSwitchChannel, initVec);

    return TI_OK;
}


/************************************************************************
 *                        switchChannel_recvCmd                         *
 ************************************************************************/
/*DESCRIPTION: This function is called by MLME Parser upon receiving of
                Beacon, Probe Response or Action with Switch Channel command,
                or beacon/
                performs the following:
                -   Initializes the switching channel procedure.
                -   Setting timer to the actual switching time(if needed)
                
INPUT:      hSwitchChannel      - SwitchChannel handle.
            switchMode          - indicates whether to stop transmission
                                    until the scheduled channel switch.
            newChannelNum       - indicates the number of the new channel.
            durationTime        - indicates the time (expressed in ms) until
                                    the scheduled channel switch should accure.

OUTPUT:     None

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

void switchChannel_recvCmd(TI_HANDLE hSwitchChannel, dot11_CHANNEL_SWITCH_t *channelSwitch, TI_UINT8 channel)
{

    switchChannel_t             *pSwitchChannel = (switchChannel_t *)hSwitchChannel;
    paramInfo_t                 param;

    if (pSwitchChannel==NULL)
    {
        return;
    }

    param.paramType = REGULATORY_DOMAIN_DFS_CHANNELS_RANGE;
    regulatoryDomain_getParam(pSwitchChannel->hRegulatoryDomain, &param);

    if (!pSwitchChannel->dot11SpectrumManagementRequired ||
        (channel < param.content.DFS_ChannelRange.minDFS_channelNum) ||
        (channel > param.content.DFS_ChannelRange.maxDFS_channelNum))
    {   /* Do not parse Switch Channel IE, when SpectrumManagement is disabled,
            or the channel is non-DFS channel */
        return;
    }
#ifdef TI_DBG
    /* for debug purposes only */
    if (pSwitchChannel->ignoreCancelSwitchChannelCmd != 0)
    {
        return;
    }
#endif

    if (channelSwitch == NULL)
    {   /* No SC IE, update regDomain */
        param.paramType = REGULATORY_DOMAIN_UPDATE_CHANNEL_VALIDITY;
        param.content.channel = channel;
        regulatoryDomain_setParam(pSwitchChannel->hRegulatoryDomain, &param);
    } 
    else 
    {   /* SC IE exists */
        TRACE3(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_recvFrame, SwitchChannel cmd was found, channel no=%d, mode=%d, TBTT=%d \n", channelSwitch->channelNumber, channelSwitch->channelSwitchMode, channelSwitch->channelSwitchCount);

        /* Checking channel number validity */
        param.content.channel = channelSwitch->channelNumber;
        param.paramType = REGULATORY_DOMAIN_IS_CHANNEL_SUPPORTED;
        regulatoryDomain_getParam(pSwitchChannel->hRegulatoryDomain,&param);
        if ( ( !param.content.bIsChannelSupprted )   || 
            (channelSwitch->channelSwitchCount == 0) || 
            (channelSwitch->channelSwitchMode == SC_SWITCH_CHANNEL_MODE_TX_SUS))
        {   /* Trigger Roaming, if TX mode is disabled, the new channel number is invalid, 
                or the TBTT count is 0 */
            TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "report Roaming trigger\n");
            if (channelSwitch->channelSwitchMode == SC_SWITCH_CHANNEL_MODE_TX_SUS) 
            {
                param.paramType = REGULATORY_DOMAIN_SET_CHANNEL_VALIDITY;
                param.content.channelValidity.channelNum = channel;
                param.content.channelValidity.channelValidity = TI_FALSE;
                regulatoryDomain_setParam(pSwitchChannel->hRegulatoryDomain, &param);
            }
            
            if (TI_TRUE == pSwitchChannel->switchChannelStarted) 
            {
                   apConn_reportRoamingEvent(pSwitchChannel->hApConn, ROAMING_TRIGGER_SWITCH_CHANNEL, NULL); 
            }
            
        }
        else
        {   /* Invoke Switch Channel command */
            /* update the new SCC params */
            pSwitchChannel->curChannelSwitchCmdParams.channelNumber = channelSwitch->channelNumber;
            pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount = channelSwitch->channelSwitchCount;
            pSwitchChannel->curChannelSwitchCmdParams.channelSwitchMode = channelSwitch->channelSwitchMode;
            switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_SC_CMD, pSwitchChannel);
        }

    }

}


/************************************************************************
 *                        switchChannel_powerSaveStatusReturn           *
 ************************************************************************/
/**
*
* \b Description: 
*
* This procedure is called when power save status is returned
*
* \b ARGS:
*
*  I/O - hSwitchChannel - SwitchChannel context \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
void switchChannel_SwitchChannelCmdCompleteReturn(TI_HANDLE hSwitchChannel)
{
    switchChannel_t         *pSwitchChannel = (switchChannel_t*)hSwitchChannel;

    if (pSwitchChannel == NULL)
    {
        return;
    }
    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_SwitchChannelCmdCompleteReturn \n");

    switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_SC_CMPLT, pSwitchChannel);

}

/************************************************************************
 *                        switchChannel_enableDisableSpectrumMngmt          *
 ************************************************************************/
/**
*
* \b Description: 
*
* This procedure enables or disables the spectrum management
*
* \b ARGS:
*
*  I - hSwitchChannel - SwitchChannel context \n
*  I - enableDisable - TI_TRUE - Enable, TI_FALSE - Disable
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
void switchChannel_enableDisableSpectrumMngmt(TI_HANDLE hSwitchChannel, TI_BOOL enableDisable)
{
    switchChannel_t         *pSwitchChannel = (switchChannel_t*)hSwitchChannel;


    if (hSwitchChannel == NULL)
    {
        return;
    }
    TRACE1(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_enableDisableSpectrumMngmt, enableDisable=%d \n", enableDisable);

    pSwitchChannel->dot11SpectrumManagementRequired = enableDisable;

    if (enableDisable)
    {   /* Enable SC, if it was started invoke start event.
            otherwise, wait for a start event */
        if (pSwitchChannel->switchChannelStarted)
        {
            switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_START, pSwitchChannel);
        }
    }
    else
    {   /* Disable SC */
        switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_STOP, pSwitchChannel);
    }

}



/************************************************************************
 *                        SwitchChannel internal functions              *
 ************************************************************************/

/************************************************************************
 *                        switchChannel_smEvent                         *
 ************************************************************************/
/**
*
* \b Description: 
*
* SwitchChannel state machine transition function
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
static TI_STATUS switchChannel_smEvent(TI_UINT8 *currState, TI_UINT8 event, void* data)
{
    TI_STATUS       status;
    TI_UINT8           nextState;
    switchChannel_t           *pSwitchChannel = (switchChannel_t*)data;


    status = fsm_GetNextState(pSwitchChannel->pSwitchChannelSm, *currState, event, &nextState);
    if (status != TI_OK)
    {
        TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_ERROR, "switchChannel_smEvent, fsm_GetNextState error\n");
        return(TI_NOK);
    }

	TRACE3(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smEvent: <currentState = %d, event = %d> --> nextState = %d\n", *currState, event, nextState);

	status = fsm_Event(pSwitchChannel->pSwitchChannelSm, currState, event, (void *)pSwitchChannel);

    if (status != TI_OK)
    {
        TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_ERROR, "switchChannel_smEvent fsm_Event error\n");
		TRACE3(pSwitchChannel->hReport, REPORT_SEVERITY_ERROR, "switchChannel_smEvent: <currentState = %d, event = %d> --> nextState = %d\n", *currState, event, nextState);
    }
    return status;

}


/************************************************************************
 *                        switchChannel_smStart                         *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when the station becomes connected.
 * update the current channel.
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smStart(void *pData)
{
    switchChannel_t                       *pSwitchChannel = (switchChannel_t*)pData;
    paramInfo_t                 param;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }
    
    /* get the current channel number */
    param.paramType = SITE_MGR_CURRENT_CHANNEL_PARAM;
    siteMgr_getParam(pSwitchChannel->hSiteMgr, &param);
    pSwitchChannel->currentChannel = param.content.siteMgrCurrentChannel;
    
    TRACE1(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smStart, channelNo=%d\n", pSwitchChannel->currentChannel);
    return TI_OK;

}

/************************************************************************
 *                        switchChannel_smReqSCR_UpdateCmd              *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* Update the Switch Channel command parameters.
 * Request SCR and wait for SCR return.
 * if tx status suspend
 *  update regulatory Domain
 *  update tx
 *  start periodic timer 
 
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smReqSCR_UpdateCmd(void *pData)
{
    switchChannel_t             *pSwitchChannel = (switchChannel_t*)pData;
    EScrClientRequestStatus     scrStatus;
    EScePendReason              scrPendReason;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }
    TRACE3(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smReqSCR_UpdateCmd, channelNo=%d, TBTT = %d, Mode = %d\n", pSwitchChannel->curChannelSwitchCmdParams.channelNumber, pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount, pSwitchChannel->curChannelSwitchCmdParams.channelSwitchMode);


    /* Save the TS when requesting SCR */
    pSwitchChannel->SCRRequestTimestamp = os_timeStampMs(pSwitchChannel->hOs); 
    
    scrStatus = scr_clientRequest(pSwitchChannel->hSCR, SCR_CID_SWITCH_CHANNEL,
                                  SCR_RESOURCE_SERVING_CHANNEL, &scrPendReason);
    if ((scrStatus != SCR_CRS_RUN) && (scrStatus != SCR_CRS_PEND))
    {   
        TRACE1(pSwitchChannel->hReport, REPORT_SEVERITY_ERROR, "switchChannel_smReqSCR_UpdateCmd():Abort the switch channel, request Roaming, scrStatus=%d\n", scrStatus);
        return (switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_SCR_FAIL, pSwitchChannel));

    }
    if (scrStatus == SCR_CRS_RUN)
    {
        switchChannel_scrStatusCB(pSwitchChannel, scrStatus, SCR_RESOURCE_SERVING_CHANNEL, scrPendReason);
    }
    else if ((scrPendReason==SCR_PR_OTHER_CLIENT_RUNNING) ||
         (scrPendReason==SCR_PR_DIFFERENT_GROUP_RUNNING) )
    {   /* No use to wait for the SCR, invoke FAIL */
        return (switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_SCR_FAIL, pSwitchChannel));
    }
    /* wait for the SCR callback function to be called */
    return TI_OK;

}


/************************************************************************
 *                        switchChannel_scrStatusCB                     *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called by the SCR when:
 * the resource is reserved for the SwitchChannel - SCR_CRS_RUN
 * recovery occurred - SCR_CRS_ABORT
 * other = ERROR
*
* \b ARGS:
*
*  I   - hSwitchChannel - pointer to the SwitchChannel SM context  \n
*  I   - requestStatus - the SCR request status  \n
*  I   - eResource - the resource for which the CB is issued  \n
*  I   - pendReason - The SCR pend status in case of pend reply  \n
*
* \b RETURNS:
*
*  None.
*
* 
*************************************************************************/
void switchChannel_scrStatusCB(TI_HANDLE hSwitchChannel, EScrClientRequestStatus requestStatus, 
                               EScrResourceId eResource, EScePendReason pendReason )
{
    switchChannel_t             *pSwitchChannel = (switchChannel_t*)hSwitchChannel;
    switchChannel_smEvents      scEvent;

    if (pSwitchChannel == NULL)
    {
        return;
    }

    switch (requestStatus)
    {
    case SCR_CRS_RUN: 
        scEvent = SC_EVENT_SCR_RUN;
        break;
    case SCR_CRS_FW_RESET:
        scEvent = SC_EVENT_FW_RESET;
        break;
    case SCR_CRS_PEND:
        scEvent = SC_EVENT_SCR_FAIL;
        break;
    case SCR_CRS_ABORT: 
    default:
        TRACE2(pSwitchChannel->hReport, REPORT_SEVERITY_ERROR, "switchChannel_scrStatusCB scrStatus = %d, pendReason=%d\n", requestStatus, pendReason);
        scEvent = SC_EVENT_SCR_FAIL;
        break;
    }

    switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, scEvent, pSwitchChannel);

}



/************************************************************************
 *                        switchChannel_smStartSwitchChannelCmd         *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called once SwitchChannel command was received and the SCR
 * request returned with reason RUN. 
 * In this case perform the following:
 *  Set CMD to FW


*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smStartSwitchChannelCmd(void *pData)
{
    switchChannel_t         *pSwitchChannel = (switchChannel_t *)pData;
    TSwitchChannelParams    pSwitchChannelCmd;
    TI_UINT32                   switchChannelTimeDuration;
    paramInfo_t             param;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }

    param.paramType = SITE_MGR_BEACON_INTERVAL_PARAM;
    siteMgr_getParam(pSwitchChannel->hSiteMgr, &param);

    switchChannelTimeDuration = pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount * param.content.beaconInterval * 1024 / 1000;
    if (  (switchChannelTimeDuration!=0) &&
        ((os_timeStampMs(pSwitchChannel->hOs) - pSwitchChannel->SCRRequestTimestamp) >= switchChannelTimeDuration ))
    {   /* There's no time to perfrom the SCC, set the Count to 1 */
        pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount = 1;
    }

    apConn_indicateSwitchChannelInProgress(pSwitchChannel->hApConn);

    pSwitchChannelCmd.channelNumber = pSwitchChannel->curChannelSwitchCmdParams.channelNumber;
    pSwitchChannelCmd.switchTime    = pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount;
    pSwitchChannelCmd.txFlag        = pSwitchChannel->curChannelSwitchCmdParams.channelSwitchMode;
    pSwitchChannelCmd.flush         = 0;
    TWD_CmdSwitchChannel (pSwitchChannel->hTWD, &pSwitchChannelCmd);
    
    TRACE4(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "TWD_CmdSwitchChannel:Set the cmd in HAL. Params:\n channelNumber=%d, switchTime=%d, txFlag=%d, flush=%d \n", pSwitchChannelCmd.channelNumber, pSwitchChannelCmd.switchTime, pSwitchChannelCmd.txFlag, pSwitchChannelCmd.flush);

    return TI_OK;

}

/************************************************************************
 *    switchChannel_smFwResetWhileSCInProg          *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when Switch Channel command is cancelled. 
 * In this case update TX nad regulatory Domain adn HAL.
 * Release the SCR and exit PS.
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smFwResetWhileSCInProg(void *pData)
{
    switchChannel_t      *pSwitchChannel = (switchChannel_t *)pData;
    paramInfo_t          param;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }
    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smFwResetWhileSCInProg \n");

    /* Update new current channel */
    param.paramType = SITE_MGR_CURRENT_CHANNEL_PARAM;
    param.content.siteMgrCurrentChannel = pSwitchChannel->curChannelSwitchCmdParams.channelNumber;
    siteMgr_setParam(pSwitchChannel->hSiteMgr, &param);

    apConn_indicateSwitchChannelFinished(pSwitchChannel->hApConn);

    switchChannel_zeroDatabase(pSwitchChannel);

    /* release the SCR */
    scr_clientComplete(pSwitchChannel->hSCR, SCR_CID_SWITCH_CHANNEL, SCR_RESOURCE_SERVING_CHANNEL);
    
    return TI_OK;

}


/************************************************************************
 *                        switchChannel_smSwitchChannelCmplt            *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when SwitchChannel command completed in FW.
 * In this case release SCR and update current channel.
 * If TX was sus, it will be enabled only after first Beacon is recieved.
 * Exit PS.
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smSwitchChannelCmplt(void *pData)
{
    switchChannel_t             *pSwitchChannel = (switchChannel_t *)pData;
    paramInfo_t     param;
    
    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }
    
    /* Update new current channel */
    param.paramType = SITE_MGR_CURRENT_CHANNEL_PARAM;
    param.content.siteMgrCurrentChannel = pSwitchChannel->curChannelSwitchCmdParams.channelNumber;
    siteMgr_setParam(pSwitchChannel->hSiteMgr, &param);

    TRACE1(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smSwitchChannelCmplt, new channelNum = %d\n", pSwitchChannel->currentChannel);

    apConn_indicateSwitchChannelFinished(pSwitchChannel->hApConn);
    switchChannel_zeroDatabase(pSwitchChannel);

    /* release the SCR */
    scr_clientComplete(pSwitchChannel->hSCR, SCR_CID_SWITCH_CHANNEL, SCR_RESOURCE_SERVING_CHANNEL);

    return TI_OK;

}

 

/************************************************************************
 *                        switchChannel_smScrFailWhileWait4Scr          *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when recovery occurred, while waiting for SCR due
 * to previous switch channel command. 
 * Exit PS
 * Release SCR. 
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smScrFailWhileWait4Scr(void *pData)
{
    switchChannel_t                       *pSwitchChannel = (switchChannel_t*)pData;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }

    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smScrFailWhileWait4Scr\n");

    switchChannel_zeroDatabase(pSwitchChannel);

    /* release the SCR is not required !!! */
    scr_clientComplete(pSwitchChannel->hSCR, SCR_CID_SWITCH_CHANNEL, SCR_RESOURCE_SERVING_CHANNEL);

    return TI_OK;
}

/************************************************************************
 *                        switchChannel_smStopWhileWait4Cmd             *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when the station becomes Disconnected and the current
* state is Wait4Cmd. In this case perfrom:
 * Stop the timer
 * Enable TX if it was disabled
 * Zero the current command parameters
 * Stop the timer
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smStopWhileWait4Cmd(void *pData)
{
    switchChannel_t                       *pSwitchChannel = (switchChannel_t*)pData;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }

    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smStopWhileWait4Cmd\n");

    switchChannel_zeroDatabase(pSwitchChannel);
    
    return TI_OK;
}

/************************************************************************
 *                        switchChannel_smStopWhileWait4Scr                 *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when the station becomes Disconnected and the current
* state is Wait4Scr. In this case perfrom:
 * Stop the timer
 * Enable TX if it was disabled
 * Zero the current command parameters
 * Complete SCR
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smStopWhileWait4Scr(void *pData)
{
    switchChannel_t                       *pSwitchChannel = (switchChannel_t*)pData;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }

    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smStopWhileWait4Scr\n");


    switchChannel_zeroDatabase(pSwitchChannel);
    
    /* release the SCR */
    scr_clientComplete(pSwitchChannel->hSCR, SCR_CID_SWITCH_CHANNEL, SCR_RESOURCE_SERVING_CHANNEL);

    return TI_OK;
}

/************************************************************************
 *         switchChannel_smStopWhileSwitchChannelInProg                 *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when the station becomes Disconnected and the current
* state is SwitchChannelInProg. In this case perfrom:
 * Stop the timer
 * Enable TX if it was disabled
 * Zero the current command parameters
 * resume self test
 * Complete SCR
 * Exit PS
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*************************************************************************/
static TI_STATUS switchChannel_smStopWhileSwitchChannelInProg(void *pData)
{
    switchChannel_t                       *pSwitchChannel = (switchChannel_t*)pData;

    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }

    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_smStopWhileSwitchChannelInProg\n");

    /* Exit PS */
    /*PowerMgr_exitFromDriverMode(pSwitchChannel->hPowerMngr, "SwitchChannel");*/

    apConn_indicateSwitchChannelFinished(pSwitchChannel->hApConn);

    TWD_CmdSwitchChannelCancel (pSwitchChannel->hTWD, pSwitchChannel->currentChannel);
    switchChannel_zeroDatabase(pSwitchChannel);
    
    /* release the SCR */
    scr_clientComplete(pSwitchChannel->hSCR, SCR_CID_SWITCH_CHANNEL, SCR_RESOURCE_SERVING_CHANNEL);

    return TI_OK;
}




/************************************************************************
 *                        switchChannel_zeroDatabase                    *
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called when the SwitchChannel internal database should be zero.
 * The following parameters are zerod:
 * SwitchChannelChannelRange - the timestamps and validity state of channels
 * curChannelSwitchCmdParams - the current switch channel command parameters
*
* \b ARGS:
*
*  I   - pSwitchChannel - pointer to the SwitchChannel SM context  \n
*  I   - channelNum - channel number  \n
*  I   - timestamp - required timestamp  \n
*
* \b RETURNS:
*
*  None.
*
* 
*************************************************************************/
static void switchChannel_zeroDatabase(switchChannel_t *pSwitchChannel)
{

    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_zeroDatabase\n");


    pSwitchChannel->curChannelSwitchCmdParams.channelNumber = 0;
    pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount = 0;
    pSwitchChannel->curChannelSwitchCmdParams.channelSwitchMode = SC_SWITCH_CHANNEL_MODE_NOT_TX_SUS;
    pSwitchChannel->currentChannel = 0;

}

/***********************************************************************
 *                        release_module                               
 ***********************************************************************/
/* 
DESCRIPTION:    Called by the destroy function or by the create function (on failure)
                Go over the vector, for each bit that is set, release the corresponding module.
                                                                                                   
INPUT:      pSwitchChannel  -   SwitchChannel pointer.
            initVec -   Vector that contains a bit set for each module thah had been initiualized

OUTPUT:     

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static void release_module(switchChannel_t *pSwitchChannel, TI_UINT32 initVec)
{
    if (pSwitchChannel == NULL)
    {
        return;
    }
    if (initVec & (1 << SC_SM_INIT_BIT))
    {
        fsm_Unload(pSwitchChannel->hOs, pSwitchChannel->pSwitchChannelSm);
    }

    if (initVec & (1 << SC_INIT_BIT))
    {
        os_memoryFree(pSwitchChannel->hOs, pSwitchChannel, sizeof(switchChannel_t));
    }

    initVec = 0;
}


/**
*
* switchChannel_smNop - Do nothing
*
* \b Description: 
*
* Do nothing in the SM.
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static TI_STATUS switchChannel_smNop(void *pData)
{
    switchChannel_t       *pSwitchChannel;

    pSwitchChannel = (switchChannel_t*)pData;
    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }
    TRACE0(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, " switchChannel_smNop\n");

    return TI_OK;
}

/**
*
* switchChannel_smUnexpected - Unexpected event
*
* \b Description: 
*
* Unexpected event in the SM.
*
* \b ARGS:
*
*  I   - pData - pointer to the SwitchChannel SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* 
*/
static TI_STATUS switchChannel_smUnexpected(void *pData)
{
    switchChannel_t       *pSwitchChannel;

    pSwitchChannel = (switchChannel_t*)pData;
    if (pSwitchChannel == NULL)
    {
        return TI_NOK;
    }
    TRACE1(pSwitchChannel->hReport, REPORT_SEVERITY_ERROR, " switchChannel_smUnexpected, state = %d\n", pSwitchChannel->currentState);

    return TI_NOK;
}

/*******************************************************************************
***********                         Debug functions                  ***********               
*******************************************************************************/
#ifdef TI_DBG

/************************************************************************
 *                        switchChannel_recvCmd                         *
 ************************************************************************/
/*DESCRIPTION: This function is called by MLME Parser upon receiving of
                Beacon, Probe Response or Action with Switch Channel command,
                or beacon/
                performs the following:
                -   Initializes the switching channel procedure.
                -   Setting timer to the actual switching time(if needed)
                
INPUT:      hSwitchChannel              -   SwitchChannel handle.
            switchMode          - indicates whether to stop transmission
                                until the scheduled channel switch.
            newChannelNum       - indicates the number of the new channel.
            durationTime        - indicates the time (expressed in ms) until
                                the scheduled channel switch should accure.

OUTPUT:     None

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static void switchChannel_recvCmd4Debug(TI_HANDLE hSwitchChannel, dot11_CHANNEL_SWITCH_t *channelSwitch, TI_BOOL BeaconPacket, TI_UINT8 channel)
{

    switchChannel_t             *pSwitchChannel = (switchChannel_t *)hSwitchChannel;
    paramInfo_t                 param;

    if (pSwitchChannel==NULL)
    {
        return;
    }


    /* The following is for debug purposes only
        It will be operated when the Switch Channel cmd is opertated from debug CLI */
    if (pSwitchChannel->ignoreCancelSwitchChannelCmd != 0)
    {
        if (pSwitchChannel->ignoreCancelSwitchChannelCmd == 1)
        {
            /* update the new SCC params */
            pSwitchChannel->curChannelSwitchCmdParams.channelNumber = channelSwitch->channelNumber;
            pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount = channelSwitch->channelSwitchCount;
            pSwitchChannel->curChannelSwitchCmdParams.channelSwitchMode = channelSwitch->channelSwitchMode;
            
            pSwitchChannel->ignoreCancelSwitchChannelCmd = 2;
        }
        else if (channelSwitch->channelSwitchCount>0)
        {
            channelSwitch->channelSwitchCount --;
        }
        else
        {   
            pSwitchChannel->ignoreCancelSwitchChannelCmd = 0;
        }


        /* search in the buffer pointer to the beginning of the
            Switch Cahnnel Announcement IE according to the IE ID */
        
        /* SC IE exists on the serving channel */
        TRACE3(pSwitchChannel->hReport, REPORT_SEVERITY_INFORMATION, "switchChannel_recvFrame, SwitchChannel cmd was found, channel no=%d, mode=%d, TBTT=%d \n", channelSwitch->channelNumber, channelSwitch->channelSwitchMode, channelSwitch->channelSwitchCount);

        /* Checking channel number validity */
        param.content.channel = channelSwitch->channelNumber;
        param.paramType = REGULATORY_DOMAIN_IS_CHANNEL_SUPPORTED;
        regulatoryDomain_getParam(pSwitchChannel->hRegulatoryDomain,&param);
        if (( !param.content.bIsChannelSupprted ) || 
            (channelSwitch->channelSwitchCount == 0) || 
            (channelSwitch->channelSwitchMode == SC_SWITCH_CHANNEL_MODE_TX_SUS))
        {   /* Trigger Roaming, if TX mode is disabled, the new channel number is invalid, 
                or the TBTT count is 0 */
            apConn_reportRoamingEvent(pSwitchChannel->hApConn, ROAMING_TRIGGER_SWITCH_CHANNEL, NULL);
        }
        else
        {   /* Invoke Switch Channel command */
            /* update the new SCC params */
            pSwitchChannel->curChannelSwitchCmdParams.channelNumber = channelSwitch->channelNumber;
            pSwitchChannel->curChannelSwitchCmdParams.channelSwitchCount = channelSwitch->channelSwitchCount;
            pSwitchChannel->curChannelSwitchCmdParams.channelSwitchMode = channelSwitch->channelSwitchMode;
            switchChannel_smEvent((TI_UINT8*)&pSwitchChannel->currentState, SC_EVENT_SC_CMD, pSwitchChannel);
        }
    }

}

void switchChannelDebug_setCmdParams(TI_HANDLE hSwitchChannel, SC_switchChannelCmdParam_e switchChannelCmdParam, TI_UINT8 param)
{
    switchChannel_t       *pSwitchChannel = (switchChannel_t*)hSwitchChannel;

    if (pSwitchChannel == NULL)
    {
        return;
    }
    
    switch (switchChannelCmdParam)
    {
    case SC_SWITCH_CHANNEL_NUM:
        WLAN_OS_REPORT(("SwitchChannelDebug_setSwitchChannelCmdParams, channelNum=%d \n ", param));
        pSwitchChannel->debugChannelSwitchCmdParams.channelNumber = param;
        break;
    case SC_SWITCH_CHANNEL_TBTT:
        WLAN_OS_REPORT(("SwitchChannelDebug_setSwitchChannelCmdParams, channelSwitchCount=%d \n ", param));
        pSwitchChannel->debugChannelSwitchCmdParams.channelSwitchCount = param;
        break;
    case SC_SWITCH_CHANNEL_MODE:
        WLAN_OS_REPORT(("SwitchChannelDebug_setSwitchChannelCmdParams, channelSwitchMode=%d \n ", param));
        pSwitchChannel->debugChannelSwitchCmdParams.channelSwitchMode = param;
        break;
    default:
        WLAN_OS_REPORT(("ERROR: SwitchChannelDebug_setSwitchChannelCmdParams, wrong switchChannelCmdParam=%d \n ",
                        switchChannelCmdParam));
            break;
    }

}
void switchChannelDebug_SwitchChannelCmdTest(TI_HANDLE hSwitchChannel, TI_BOOL BeaconPacket)
{
    switchChannel_t       *pSwitchChannel = (switchChannel_t*)hSwitchChannel;

    if (pSwitchChannel == NULL)
    {
        return;
    }
    
    WLAN_OS_REPORT(("SwitchChannelDebug_SwitchChannelCmdTest, BeaconPacket=%d \n cmd params: channelNumber=%d, channelSwitchCount=%d, channelSwitchMode=%d \n",
                    BeaconPacket,
                    pSwitchChannel->debugChannelSwitchCmdParams.channelNumber,
                    pSwitchChannel->debugChannelSwitchCmdParams.channelSwitchCount,
                    pSwitchChannel->debugChannelSwitchCmdParams.channelSwitchMode));


    pSwitchChannel->ignoreCancelSwitchChannelCmd= 1;
    switchChannel_recvCmd4Debug(hSwitchChannel, &pSwitchChannel->debugChannelSwitchCmdParams, BeaconPacket, pSwitchChannel->currentChannel);
}

void switchChannelDebug_CancelSwitchChannelCmdTest(TI_HANDLE hSwitchChannel, TI_BOOL BeaconPacket)
{

    switchChannel_t       *pSwitchChannel = (switchChannel_t*)hSwitchChannel;

    if (pSwitchChannel == NULL)
    {
        return;
    }
    
    WLAN_OS_REPORT(("SwitchChannelDebug_SwitchChannelCmdTest, BeaconPacket=%d \n",BeaconPacket));

    pSwitchChannel->ignoreCancelSwitchChannelCmd= 0;
    switchChannel_recvCmd4Debug(hSwitchChannel, NULL, BeaconPacket, pSwitchChannel->currentChannel);
}


void switchChannelDebug_printStatus(TI_HANDLE hSwitchChannel)
{
    switchChannel_t       *pSwitchChannel = (switchChannel_t*)hSwitchChannel;

    if (pSwitchChannel == NULL)
    {
        return;
    }
    
    WLAN_OS_REPORT(("SwitchChannel debug params are: channelNumber=%d, channelSwitchCount=%d , channelSwitchMode=%d \n", 
                    pSwitchChannel->debugChannelSwitchCmdParams.channelNumber, 
                    pSwitchChannel->debugChannelSwitchCmdParams.channelSwitchCount,
                    pSwitchChannel->debugChannelSwitchCmdParams.channelSwitchMode));

    WLAN_OS_REPORT(("SwitchChannel state=%d, currentChannel=%d \n", pSwitchChannel->currentState, pSwitchChannel->currentChannel));


}

void switchChannelDebug_setChannelValidity(TI_HANDLE hSwitchChannel, TI_UINT8 channelNum, TI_BOOL validity)
{
    paramInfo_t param;

    switchChannel_t       *pSwitchChannel = (switchChannel_t*)hSwitchChannel;

    if (pSwitchChannel == NULL)
    {
        return;
    }

    param.paramType = REGULATORY_DOMAIN_SET_CHANNEL_VALIDITY;
    param.content.channelValidity.channelNum = channelNum;
    param.content.channelValidity.channelValidity = validity;
    regulatoryDomain_setParam(pSwitchChannel->hRegulatoryDomain, &param);

}

#endif  /* TI_DBG */






