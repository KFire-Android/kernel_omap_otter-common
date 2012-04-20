/*
 * PowerSrvSM.c
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

/** \file PowerSrvSM.c
 *  \brief This is the PowerSrvSM module implementation.
 *  \author Assaf Azulay
 *  \date 19-OCT-2005
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  PowerSrvSM                                                    *
 *   PURPOSE: PowerSrvSM Module implementation.                             *
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_114
#include "tidef.h"
#include "osApi.h"
#include "timer.h"
#include "fsm.h"
#include "report.h"
#include "TWDriver.h"
#include "PowerSrvSM.h"
#include "CmdBld.h"


/*****************************************************************************
 **         Defines                                                         **
 *****************************************************************************/


/*****************************************************************************
 **         structs                                                         **
 *****************************************************************************/


/*****************************************************************************
 **         Private Function prototypes                                     **
 *****************************************************************************/

static TI_STATUS powerSrvSmSMEvent(TI_UINT8* pCurrentState,
                                   TI_UINT8 event,
                                   TI_HANDLE hPowerSrvSM);
static TI_STATUS powerSrvSmDoUpdateRequest(TI_HANDLE hPowerSrvSM);
static TI_STATUS powerSrvSmDoEnterPowerSave(TI_HANDLE hPowerSrvSM);
static TI_STATUS powerSrvSmDoExitPowerSave(TI_HANDLE hPowerSrvSM);
static TI_STATUS powerSrvSmDoPending(TI_HANDLE hPowerSrvSM);
static TI_STATUS powerSrvSmDoAllready(TI_HANDLE hPowerSrvSM);
static TI_STATUS powerSrvSMActionUnexpected(TI_HANDLE hPowerSrvSM);
static TI_STATUS powerSrvSMSendMBXConfiguration(TI_HANDLE hPowerSrvSM, TI_BOOL PS_disableEnable);
static void      powerSrvSMTimerExpired (TI_HANDLE hPowerSrvSM, TI_BOOL bTwdInitOccured);

/***************************************************************************************
 **         Public Function prototypes                                      **
 ****************************************************************************************/


/****************************************************************************************
 *                        powerSrvSM_create                                                         *
 ****************************************************************************************
DESCRIPTION: Power Server SM module creation function, called by the Power Server create in creation phase 
                performs the following:
                -   Allocate the Power Server SM handle
                -   Creates the fsm.
                                                                                                                   
INPUT:          - hOs - Handle to OS        


OUTPUT:     

RETURN:     Handle to the Power Server SM module on success, NULL otherwise
****************************************************************************************/
TI_HANDLE powerSrvSM_create(TI_HANDLE hOsHandle)
{
    PowerSrvSM_t *pPowerSrvSM = NULL;
    fsm_stateMachine_t *pFsm = NULL;
    TI_STATUS status;

    pPowerSrvSM = (PowerSrvSM_t*) os_memoryAlloc (hOsHandle, sizeof(PowerSrvSM_t));
    if ( pPowerSrvSM == NULL )
    {
        WLAN_OS_REPORT(("%s(%d) - Memory Allocation Error!\n",__FUNCTION__,__LINE__));
        return NULL;
    }

    os_memoryZero (hOsHandle, pPowerSrvSM, sizeof(PowerSrvSM_t));

    pPowerSrvSM->hOS = hOsHandle;

    /* create the generic state-machine */
    status = fsm_Create(hOsHandle,
                        &pFsm,
                        (TI_UINT8)POWER_SRV_SM_STATE_NUM,
                        (TI_UINT8)POWER_SRV_SM_EVENT_NUM);
    if ( status != TI_OK )
    {
        WLAN_OS_REPORT(("%s(%d) - Error in create FSM!\n",__FUNCTION__,__LINE__));
        powerSrvSM_destroy(pPowerSrvSM);
        return NULL;
    }

    pPowerSrvSM->hFSM = (TI_HANDLE)pFsm;

    return pPowerSrvSM;
}

 
/****************************************************************************************
 *                        powerSrvSM_destroy                                                            *
 ****************************************************************************************
DESCRIPTION: Power Server SM module destroy function, 
                -   delete Power Server SM allocation
                
                                                                                                                   
INPUT:          - hPowerSrvSM - Handle to the Power Server  SM


OUTPUT:     

RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS powerSrvSM_destroy(TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    TI_HANDLE osHandle = pPowerSrvSM->hOS;

    /* free the timer */
    if (pPowerSrvSM->hPwrSrvSmTimer)
    {
        tmr_DestroyTimer (pPowerSrvSM->hPwrSrvSmTimer);
    }

    /* free the generic SM */
    if ( pPowerSrvSM->hFSM != NULL )
    {
        fsm_Unload(osHandle, (fsm_stateMachine_t*)pPowerSrvSM->hFSM);
    }

    /* free the Power Save SRV object */
    os_memoryFree(osHandle , pPowerSrvSM , sizeof(PowerSrvSM_t));

    return TI_OK;
}


/****************************************************************************************
*                        powerSrvSM_init                                                           *
****************************************************************************************
DESCRIPTION: Power Server SM module initialize function, called by the Power Server init in configure phase 
               performs the following:
               -   init the Stet machine states.
               -   set Active as start state.
                                                                                                                  
INPUT:     - hPowerSrvSM       - handle to the PowerSrvSM object.
           - hReport           - handle to the Report object.
           - hCmdBld           - handle to the Command Builder object.    
           - hTimer            - handle to the Timer module object.    

OUTPUT: 
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS powerSrvSM_init (TI_HANDLE hPowerSrvSM,
                           TI_HANDLE hReport,
                           TI_HANDLE hCmdBld,
                           TI_HANDLE hTimer)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    fsm_actionCell_t smMatrix[POWER_SRV_SM_STATE_NUM][POWER_SRV_SM_EVENT_NUM] =
    {
        /*
        next state and transition action for POWER_SRV_STATE_ACTIVE state
        */
        {
            /* POWER_SRV_EVENT_REQUEST_ACTIVE */
            {POWER_SRV_STATE_ACTIVE             , powerSrvSmDoAllready},

            /* POWER_SRV_EVENT_REQUEST_PS */
            {POWER_SRV_STATE_PEND_PS                , powerSrvSmDoEnterPowerSave},

            /* POWER_SRV_EVENT_SUCCESS */
            {POWER_SRV_STATE_ACTIVE                 , powerSrvSMActionUnexpected},

            /* POWER_SRV_EVENT_FAIL */
            {POWER_SRV_STATE_ACTIVE                 , powerSrvSMActionUnexpected}

        },

        /*
        next state and transition action for POWER_SRV_STATE_PEND_PS state
        */
        {
            /* POWER_SRV_EVENT_REQUEST_ACTIVE */
            {POWER_SRV_STATE_PEND_PS            , powerSrvSmDoPending},

            /* POWER_SRV_EVENT_REQUEST_PS */
            {POWER_SRV_STATE_PEND_PS        , powerSrvSmDoPending},

            /* POWER_SRV_EVENT_SUCCESS */
            {POWER_SRV_STATE_PS                 , powerSrvSmDoUpdateRequest},

            /* POWER_SRV_EVENT_FAIL */
            {POWER_SRV_STATE_ACTIVE             , powerSrvSmDoUpdateRequest}

        },
        /*
        next state and transition action for POWER_SRV_STATE_PS state
        */
        {
            /* POWER_SRV_EVENT_REQUEST_ACTIVE */
            {POWER_SRV_STATE_PEND_ACTIVE        , powerSrvSmDoExitPowerSave},

            /* POWER_SRV_EVENT_REQUEST_PS */
            {POWER_SRV_STATE_PS                 , powerSrvSmDoAllready},

            /* POWER_SRV_EVENT_SUCCESS */
            {POWER_SRV_STATE_PS                 , powerSrvSMActionUnexpected},

            /* POWER_SRV_EVENT_FAIL */
            {POWER_SRV_STATE_PS                 , powerSrvSMActionUnexpected}

        },
        /*
        next state and transition action for POWER_SRV_STATE_PEND_ACTIVE state
        */
        {
            /* POWER_SRV_EVENT_REQUEST_ACTIVE */
            {POWER_SRV_STATE_PEND_ACTIVE            , powerSrvSmDoPending},

            /* POWER_SRV_EVENT_REQUEST_PS */
            {POWER_SRV_STATE_PEND_ACTIVE        , powerSrvSmDoPending},

            /* POWER_SRV_EVENT_SUCCESS */
            {POWER_SRV_STATE_ACTIVE             , powerSrvSmDoUpdateRequest},

            /* POWER_SRV_EVENT_FAIL */
            {POWER_SRV_STATE_ERROR_ACTIVE       , powerSrvSmDoUpdateRequest}

        },
        /*
        next state and transition action for POWER_SRV_STATE_ERROR_ACTIVE state
        */
        {
            /* POWER_SRV_EVENT_REQUEST_ACTIVE */
            {POWER_SRV_STATE_PEND_ACTIVE            , powerSrvSmDoExitPowerSave},

            /* POWER_SRV_EVENT_REQUEST_PS */
            {POWER_SRV_STATE_PEND_PS        , powerSrvSmDoEnterPowerSave},

            /* POWER_SRV_EVENT_SUCCESS */
            {POWER_SRV_STATE_ERROR_ACTIVE       , powerSrvSMActionUnexpected},

            /* POWER_SRV_EVENT_FAIL */
            {POWER_SRV_STATE_ERROR_ACTIVE       , powerSrvSMActionUnexpected}

        },

    };

    pPowerSrvSM->hReport = hReport;
    pPowerSrvSM->hCmdBld = hCmdBld;
    pPowerSrvSM->hTimer  = hTimer;

    /* create the timer */
    pPowerSrvSM->hPwrSrvSmTimer = tmr_CreateTimer (pPowerSrvSM->hTimer);
	if (pPowerSrvSM->hPwrSrvSmTimer == NULL)
	{
        TRACE0(pPowerSrvSM->hReport, REPORT_SEVERITY_ERROR, "powerSrvSM_init(): Failed to create hPwrSrvSmTimer!\n");
		return TI_NOK;
	}

    fsm_Config(pPowerSrvSM->hFSM,
               (fsm_Matrix_t)smMatrix,
               POWER_SRV_SM_STATE_NUM,
               POWER_SRV_SM_EVENT_NUM,
               powerSrvSmSMEvent,
               pPowerSrvSM->hOS);

    /*
    the PowerSrvSM start in active mode (POWER_SRV_STATE_ACTIVE)
    the PowerSrvSM::currentState must be sync with the PowerSrv::desiredPowerModeProfile (POWER_MODE_ACTIVE).
    */
    pPowerSrvSM->currentState = POWER_SRV_STATE_ACTIVE;


    /*
    Null packet rate : 2,5.5 M
    Probe Request : Not PBCC modulation, Long Preamble */
    pPowerSrvSM->NullPktRateModulation= (DRV_RATE_MASK_1_BARKER | DRV_RATE_MASK_2_BARKER); 

    TRACE0(pPowerSrvSM->hReport, REPORT_SEVERITY_INIT, "PowerSrvSM Initialized\n");

    return TI_OK;
}

/****************************************************************************************
*                        powerSrvSM_config                                                         *
****************************************************************************************
DESCRIPTION: Power Server SM module configuration function, called by the Power Server init in configure phase 
               performs the following:
               -   init the Stet machine states.
               -   set Active as start state.
                                                                                                                  
INPUT:      - hPowerSrvSM       - handle to the PowerSrvSM object.  
           - pPowerSrvInitParams   - the Power Server initialize parameters.

OUTPUT: 
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS powerSrvSM_config(TI_HANDLE hPowerSrvSM,
                            TPowerSrvInitParams *pPowerSrvInitParams)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    /*
    init PowerMgmtConfigration
    */
    pPowerSrvSM->hangOverPeriod =   pPowerSrvInitParams->hangOverPeriod;
    pPowerSrvSM->numNullPktRetries =    pPowerSrvInitParams->numNullPktRetries;

    return TI_OK;
}
/****************************************************************************************
 *                        powerSrvSM_SMApi                                                           *
 *****************************************************************************************
DESCRIPTION: This function triggers events from the outside of the module into the state machine.
              
                                                                                                                                                                       
INPUT:      - hPowerSrvSM                   - handle to the PowerSrvSM object.  
            - theSMEvent                    - event from TWD.
            

OUTPUT: 
RETURN:    TI_STATUS TI_OK / PENDING / TI_NOK
****************************************************************************************/
TI_STATUS powerSrvSM_SMApi(TI_HANDLE hPowerSrvSM,
                           PowerSrvSMEvents_e theSMEvent)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    TI_STATUS status;

    switch ( theSMEvent )
    {
    case POWER_SRV_EVENT_REQUEST_ACTIVE :
    case POWER_SRV_EVENT_REQUEST_PS :
    case POWER_SRV_EVENT_FAIL :
    case POWER_SRV_EVENT_SUCCESS :

        TRACE1(pPowerSrvSM->hReport, REPORT_SEVERITY_INFORMATION, "powerSrvSM_SMApi(%d) called - legal input parameter.\n",theSMEvent);
        break;

    default:
        TRACE1(pPowerSrvSM->hReport, REPORT_SEVERITY_WARNING, "powerSrvSM_SMApi(%d) called,                            input parameter is illegal.",theSMEvent);
        return TI_NOK;
    }


    status = powerSrvSmSMEvent((TI_UINT8*)&pPowerSrvSM->currentState,
                               (TI_UINT8)theSMEvent,
                               hPowerSrvSM);

    return status;
}


/****************************************************************************************
 *                        powerSrvSm_setSmRequest                                                    *
 *****************************************************************************************
DESCRIPTION: This function sets the current SM working request.
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.
            -powerSrvRequest_t*                 - pointer to the correct request in the Power server.

OUTPUT: 
RETURN:    TI_STATUS -  TI_OK
****************************************************************************************/
TI_STATUS powerSrvSm_setSmRequest(TI_HANDLE hPowerSrvSM,powerSrvRequest_t* pSmRequest)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    pPowerSrvSM->pSmRequest = pSmRequest;
    return TI_OK;
}


/****************************************************************************************
 *                        powerSrvSM_getCurrentState                                                         *
 *****************************************************************************************
DESCRIPTION: This function returns the current state of the SM.
                                                       
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.      
            

OUTPUT: 
RETURN:    PowerSrvSMStates_e current state
****************************************************************************************/
PowerSrvSMStates_e powerSrvSM_getCurrentState(TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    return pPowerSrvSM->currentState; 
}

/****************************************************************************************
 *                        powerSrvSM_setRateModulation                                               *
 *****************************************************************************************
DESCRIPTION: This function sets the Rate Modulation
                                                       
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.      
            - rateModulation                        - desired rate

OUTPUT: 
RETURN:      void
****************************************************************************************/

void powerSrvSM_setRateModulation(TI_HANDLE hPowerSrvSM, TI_UINT16 rateModulation)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    pPowerSrvSM->NullPktRateModulation= rateModulation; 
}

/****************************************************************************************
 *                        powerSrvSM_getRateModulation                                               *
 *****************************************************************************************
DESCRIPTION: This function sets the Rate Modulation
                                                       
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.      

OUTPUT: 
RETURN:      -  desired rate
****************************************************************************************/

TI_UINT32 powerSrvSM_getRateModulation(TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    return pPowerSrvSM->NullPktRateModulation;
}

/****************************************************************************************
 *                        powerSrvSM_printObject                                                         *
 *****************************************************************************************
DESCRIPTION: This function prints the SM object
                                                       
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.      
            

OUTPUT: 
RETURN:   void
****************************************************************************************/
void powerSrvSM_printObject(TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    char *pString;

    WLAN_OS_REPORT(("\n+++++ powerSrvSM_printObject +++++\n"));
    WLAN_OS_REPORT(("Handle to the CmdBld is 0x%08X\n", pPowerSrvSM->hCmdBld));
    WLAN_OS_REPORT(("Handle to the OS is 0x%08X\n", pPowerSrvSM->hOS));
    WLAN_OS_REPORT(("Handle to the Report is 0x%08X\n", pPowerSrvSM->hReport));
    WLAN_OS_REPORT(("Handle to the FSM is 0x%08X\n", pPowerSrvSM->hFSM));

    switch ( pPowerSrvSM->currentState )
    {
    case POWER_SRV_STATE_ACTIVE:
        pString = "POWER_SRV_STATE_ACTIVE";
        break;

    case POWER_SRV_STATE_PEND_PS:
        pString = "POWER_SRV_STATE_PEND_PS";
        break;

    case POWER_SRV_STATE_PS:
        pString = "POWER_SRV_STATE_PS";
        break;

    case POWER_SRV_STATE_PEND_ACTIVE:
        pString = "POWER_SRV_STATE_PEND_ACTIVE";
        break;

    case POWER_SRV_STATE_ERROR_ACTIVE:
        pString = "POWER_SRV_STATE_ERROR_ACTIVE";
        break;


    default:
        pString = "UNKWON PARAMETER";
        break;
    }
    WLAN_OS_REPORT(("The current state of the state machine is %s (=%d)\n",
                    pString,
                    pPowerSrvSM->currentState));

}




/*****************************************************************************
 **         Private Function prototypes                                                             **
 *****************************************************************************/






/****************************************************************************************
 *                        powerSrvSmDoEnterPowerSave                                                 *
 *****************************************************************************************
DESCRIPTION: This function is an action of the state machine to move from active state to PS
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.

OUTPUT: 
RETURN:    TI_STATUS - TI_OK / TI_NOK
****************************************************************************************/

static TI_STATUS powerSrvSmDoEnterPowerSave(TI_HANDLE hPowerSrvSM)
{
    TI_STATUS status;
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    pPowerSrvSM->pSmRequest->requestState = RUNNING_REQUEST;
    status = powerSrvSMSendMBXConfiguration(hPowerSrvSM, TI_TRUE);
    return status;
}


/****************************************************************************************
 *                        powerSrvSmDoExitPowerSave                                              *
 *****************************************************************************************
DESCRIPTION: This function is an action of the state machine to move from PS state to Active
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.

OUTPUT: 
RETURN:    TI_STATUS - TI_OK / TI_NOK
****************************************************************************************/
static TI_STATUS powerSrvSmDoExitPowerSave(TI_HANDLE hPowerSrvSM)
{
    TI_STATUS status;
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    pPowerSrvSM->pSmRequest->requestState = RUNNING_REQUEST;
    status = powerSrvSMSendMBXConfiguration(hPowerSrvSM, TI_FALSE);
    return status;
}


/****************************************************************************************
 *                        powerSrvSmDoUpdateRequest                                                  *
 *****************************************************************************************
DESCRIPTION: This function is an action of the state machine to update a request when the SM 
              is already in the requested state is already 
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.

OUTPUT: 
RETURN:    TI_STATUS - TI_OK / TI_NOK
****************************************************************************************/

static TI_STATUS powerSrvSmDoUpdateRequest(TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    /* request has completed - stop the guard timer */
    tmr_StopTimer (pPowerSrvSM->hPwrSrvSmTimer);

    /*powerSrv_SetRequestState  will update the correct request (acording to the current active request)*/
    if ( pPowerSrvSM->pSmRequest->requestState == RUNNING_REQUEST )
    {
        pPowerSrvSM->pSmRequest->requestState = HANDLED_REQUEST;
    }

    return TI_OK;
}


/****************************************************************************************
 *                        powerSrvSmDoPending                                                        *
 *****************************************************************************************
DESCRIPTION: This function is an action of the state machine returns Pending in case that there is a request 
              waiting to be finished (already sent to FW)
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.

OUTPUT: 
RETURN:    TI_STATUS - PENDING
****************************************************************************************/

static TI_STATUS powerSrvSmDoPending(TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    /*powerSrv_SetRequestState will check the mode and will update the correct request (Driver of user)*/
    pPowerSrvSM->pSmRequest->requestState = PENDING_REQUEST;
    return POWER_SAVE_802_11_PENDING;
}



/****************************************************************************************
 *                        powerSrvSmDoAllready                                                       *
 *****************************************************************************************
DESCRIPTION: This function is an action of the state machine stays in the same state since it the requested
              one in the request
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.

OUTPUT: 
RETURN:    TI_STATUS - TI_OK
****************************************************************************************/
static TI_STATUS powerSrvSmDoAllready(TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    /*powerSrv_SetRequestState will check the mode and will update the correct request (Driver of user)*/
    pPowerSrvSM->pSmRequest->requestState = HANDLED_REQUEST;
    return POWER_SAVE_802_11_IS_CURRENT;
}


/****************************************************************************************
 *                        powerSrvSMActionUnexpected                                                 *
 *****************************************************************************************
DESCRIPTION: This function is an action of the state machine stays in the same state and return that action
              was not expected
                                                                                                                   
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.

OUTPUT: 
RETURN:    TI_STATUS - TI_OK
****************************************************************************************/
static TI_STATUS powerSrvSMActionUnexpected(TI_HANDLE hPowerSrvSM)
{
#ifdef TI_DBG
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    TRACE0(pPowerSrvSM->hReport, REPORT_SEVERITY_ERROR, "called: powerSrvSMActionUnexpected");
#endif /* TI_DBG */

    return TI_OK;
}


/****************************************************************************************
 *                        powerSrvSmSMEvent                                                      *
 *****************************************************************************************
DESCRIPTION: This function is the manager of the state macine. its move the state machine
              from one state to the other depend on the receive event, and call to the appropriate
              action (function) for the move between the states.
                                                                                                                   
INPUT:      - pCurrentState
            - event
            - hPowerSrvSM                       - handle to the PowerSrvSM object.

OUTPUT: 
RETURN:    TI_STATUS 
****************************************************************************************/
static TI_STATUS powerSrvSmSMEvent(TI_UINT8* pCurrentState,
                                   TI_UINT8 event,
                                   TI_HANDLE hPowerSrvSM)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    TI_STATUS status = TI_OK;
    TI_UINT8 nextState;

    status = fsm_GetNextState((fsm_stateMachine_t*)pPowerSrvSM->hFSM,
                              (TI_UINT8)pPowerSrvSM->currentState,
                              event,
                              &nextState);
    if ( status != TI_OK )
    {
        TRACE0(pPowerSrvSM->hReport, REPORT_SEVERITY_SM, "PowerSrvSM - State machine error, failed getting next state\n");
        return(status);
    }


	TRACE3( pPowerSrvSM->hReport, REPORT_SEVERITY_INFORMATION, "powerSrvSmSMEvent: <currentState = %d, event = %d> --> nextState = %d\n", *pCurrentState, event, nextState);

    status = fsm_Event(pPowerSrvSM->hFSM,
                       pCurrentState,
                       event,
                       (void*)pPowerSrvSM);

    return status;
}


/****************************************************************************************
*                        powerSrvSMSendMBXConfiguration                                             *
*****************************************************************************************
DESCRIPTION: This function send configuration of the power save option that holds in the command
                mailbox inner sturcture.
                                                                                                                  
INPUT:      - hPowerSrvSM                       - handle to the PowerSrvSM object.
           - PS_disableEnable                      - true = PS , false = active

OUTPUT: 
RETURN:    TI_STATUS 
****************************************************************************************/
static TI_STATUS    powerSrvSMSendMBXConfiguration(TI_HANDLE hPowerSrvSM, TI_BOOL PS_disableEnable)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;
    TPowerSaveParams powerSaveParams;
    TI_STATUS status;

    /*setting the params for the Hal*/
    powerSaveParams.hangOverPeriod          = pPowerSrvSM->hangOverPeriod;
    powerSaveParams.numNullPktRetries       = pPowerSrvSM->numNullPktRetries;
    powerSaveParams.NullPktRateModulation   = pPowerSrvSM->NullPktRateModulation;
    powerSaveParams.needToSendNullData      = pPowerSrvSM->pSmRequest->sendNullDataOnExit;
    powerSaveParams.ps802_11Enable          = PS_disableEnable;

    /* start the FW guard timer, which is used to protect from FW stuck */
    tmr_StartTimer (pPowerSrvSM->hPwrSrvSmTimer,
                    powerSrvSMTimerExpired,
                    (TI_HANDLE)pPowerSrvSM,
                    POWER_SAVE_GUARD_TIME_MS,
                    TI_FALSE);

    /* that command should be sent to FW just in case we moved from Active to one of the PS modes
     * and vice versa, it shoul not be sent when moving between different PS modes */
    status = cmdBld_CmdSetPsMode (pPowerSrvSM->hCmdBld, 
                                  &powerSaveParams,
                                  (void *)pPowerSrvSM->pSmRequest->powerSaveCmdResponseCB,
                                  (pPowerSrvSM->pSmRequest->powerSaveCmdResponseCB == NULL) ? NULL : pPowerSrvSM->pSmRequest->powerSaveCBObject);
                                      
    if ( status != TI_OK )
    {
        TRACE0(pPowerSrvSM->hReport, REPORT_SEVERITY_ERROR, "Error in configuring Power Manager paramters!\n");
    }

    return status;
}

/****************************************************************************************
*                               powerSrvSMTimerExpired                                  *
*****************************************************************************************
DESCRIPTION: This function is called upon timer expiry - when the FW has not returned
             a response within the defined tme (50 ms)
                                                                                                                  
INPUT:      hPowerSrvSM     - handle to the PowerSrvSM object.
            bTwdInitOccured - Indicates if TWDriver recovery occured since timer started 

OUTPUT:    None

RETURN:    None
****************************************************************************************/
static void powerSrvSMTimerExpired (TI_HANDLE hPowerSrvSM, TI_BOOL bTwdInitOccured)
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    /* Print an error message */
    TRACE0(pPowerSrvSM->hReport, REPORT_SEVERITY_ERROR, "PS guard timer expired!\n");

    /* Call the error notification callback (triggering recovery) */
    pPowerSrvSM->failureEventCB( pPowerSrvSM->hFailureEventObj ,POWER_SAVE_FAILURE );
}

/****************************************************************************************
 *                        powerSrvRegisterFailureEventCB                                                    *
 ****************************************************************************************
DESCRIPTION: Registers a failure event callback for PS SM error notifications.
                
                                                                                                                   
INPUT:      - hPowerSrv         - handle to the PowerSrv object.        
            - failureEventCB    - the failure event callback function.\n
            - hFailureEventObj - handle to the object passed to the failure event callback function.

OUTPUT: 
RETURN:    void.
****************************************************************************************/
void powerSrvSM_RegisterFailureEventCB( TI_HANDLE hPowerSrvSM, 
                                        void *failureEventCB, TI_HANDLE hFailureEventObj )
{
    PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)hPowerSrvSM;

    pPowerSrvSM->failureEventCB = (TFailureEventCb)failureEventCB;
    pPowerSrvSM->hFailureEventObj = hFailureEventObj;
}

