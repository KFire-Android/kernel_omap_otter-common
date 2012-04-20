/*
 * PowerSrv.c
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

/** \file powerSrv.c
 *  \brief This is the powerSrv module implementation.
 *  \author Assaf Azulay
 *  \date 19-Oct-2005
 */

/****************************************************************************
 *                                                                                                           *
 *   MODULE:  powerSrv                                                                                  *
 *   PURPOSE: powerSrv Module implementation.                                                   *
 *                                                                                                              *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_113
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "timer.h"
#include "PowerSrv.h"
#include "PowerSrv_API.h"
#include "PowerSrvSM.h"
#include "eventMbox_api.h"


/*****************************************************************************
 **         Defines                                                         **
 *****************************************************************************/



/****************************************************************************************
**         Private Function prototypes                                                                              **
****************************************************************************************/
static void     powerSrv802_11PsReport  (TI_HANDLE hPowerSrv, char* str , TI_UINT32 strLen);
TI_STATUS   powerSrvProcessRequest  (TI_HANDLE hPowerSrv, powerSrvMode_e requestMode);
void powerSrvCreatePssRequest (TI_HANDLE                    hPowerSrv,
                               powerSrvMode_e              requestMode,
                               powerSrvRequestState_e      requestState,
                               E80211PsMode                psMode,
                               TI_BOOL                        sendNullDataOnExit,
                               void *                      powerSaveCBObject,
                               powerSaveCmpltCB_t          powerSaveCompleteCB,
                               powerSaveCmdResponseCB_t    powerSaveCmdResponseCB);

/***************************************************************************************
**                                 Functions                                                                    **
****************************************************************************************/



/****************************************************************************************
 *                        powerSrv_create                                                           *
 ****************************************************************************************
DESCRIPTION: Power Server module creation function, called by the MAC Services create in creation phase 
                performs the following:
                -   Allocate the Power Server handle
                -   Creates the Power Server State Machine
                                                                                                                   
INPUT:          - hOs - Handle to OS        


OUTPUT:     

RETURN:     Handle to the Power Server module on success, NULL otherwise
****************************************************************************************/
TI_HANDLE powerSrv_create(TI_HANDLE hOs)
{
    powerSrv_t * pPowerSrv = NULL;
    pPowerSrv = (powerSrv_t*) os_memoryAlloc (hOs, sizeof(powerSrv_t));
    if ( pPowerSrv == NULL )
    {
        WLAN_OS_REPORT(("powerSrv_create - Memory Allocation Error!\n"));
        return NULL;
    }

    os_memoryZero (hOs, pPowerSrv, sizeof(powerSrv_t));

    pPowerSrv->hOS = hOs;

    /*creation of the State Machine*/
    pPowerSrv->hPowerSrvSM = powerSrvSM_create(hOs);
    if ( pPowerSrv->hPowerSrvSM == NULL )
    {
        WLAN_OS_REPORT(("powerSrv_create - Error in create PowerSrvSM module!\n"));
        powerSrv_destroy(pPowerSrv);
        return NULL;
    }

    return pPowerSrv;

}


/****************************************************************************************
 *                        powerSrv_destroy                                                          *
 ****************************************************************************************
DESCRIPTION: Power Server module destroy function, c
                -   delete Power Server allocation
                -   call the destroy function of the State machine
                                                                                                                   
INPUT:          - hPowerSrv - Handle to the Power Server    


OUTPUT:     

RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS powerSrv_destroy(TI_HANDLE hPowerSrv)
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;

    if ( pPowerSrv->hPowerSrvSM != NULL )
    {
        powerSrvSM_destroy(pPowerSrv->hPowerSrvSM);
    }

    os_memoryFree(pPowerSrv->hOS , pPowerSrv , sizeof(powerSrv_t));

    return TI_OK;
}


/****************************************************************************************
 *                        powerSrvSM_init                                                           *
 ****************************************************************************************
DESCRIPTION: Power Server module initialize function, called by the MAC Services in initialization phase 
                performs the following:
                -   init the Power server to active state.
                -   call the init function of the state machine.
                                                                                                                   
INPUT:      - hPowerSrv         - handle to the PowerSrv object.        
            - hReport           - handle to the Report object.
            - hEventMbox        - handle to the Event Mbox object.    
            - hCmdBld           - handle to the Command Builder object.    

OUTPUT: 
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS powerSrv_init (TI_HANDLE hPowerSrv,
                         TI_HANDLE hReport,
                         TI_HANDLE hEventMbox,
                         TI_HANDLE hCmdBld,
                         TI_HANDLE hTimer)
{
    powerSrv_t* pPowerSrv = (powerSrv_t*)hPowerSrv;

    pPowerSrv->hReport = hReport;
    pPowerSrv->hEventMbox = hEventMbox;

    /*
    init PowerSrv state machine.
    */
    powerSrvSM_init (pPowerSrv->hPowerSrvSM, hReport, hCmdBld, hTimer);

    pPowerSrv->currentMode = USER_MODE;

    /*init all request with init values*/
    powerSrvCreatePssRequest(hPowerSrv,
                             USER_MODE,
                             HANDLED_REQUEST,
                             POWER_SAVE_OFF,
                             TI_FALSE,
                             NULL,
                             NULL,
                             NULL);
    powerSrvCreatePssRequest(hPowerSrv,
                             DRIVER_MODE,
                             HANDLED_REQUEST,
                             POWER_SAVE_OFF,
                             TI_FALSE,
                             NULL,
                             NULL,
                             NULL);
    pPowerSrv->userLastRequestMode = (powerSrvMode_e)POWER_SAVE_OFF;
    pPowerSrv->pCurrentRequest  = & pPowerSrv->userRequest;

    /*
    register for Event 
    */

	eventMbox_RegisterEvent (hEventMbox,
                             TWD_OWN_EVENT_PS_REPORT,
                             (void *)powerSrv802_11PsReport,
                             hPowerSrv);

    eventMbox_UnMaskEvent (hEventMbox, TWD_OWN_EVENT_PS_REPORT, NULL, NULL);

    TRACE0(pPowerSrv->hReport, REPORT_SEVERITY_INIT, "powerSrv Initialized \n");

    return TI_OK;
}

/****************************************************************************************
 *                        powerSrv_restart															*
 ****************************************************************************************
DESCRIPTION: Restart the scan SRV module upon recovery.
				-	init the Power server to active state.
				-	call the init function of the state machine.
				                                                                                                   
INPUT:     	- hPowerSrv 			- handle to the PowerSrv object.		

OUTPUT:	
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS powerSrv_restart(	TI_HANDLE hPowerSrv)
{
    powerSrv_t* pPowerSrv = (powerSrv_t*)hPowerSrv;
	PowerSrvSM_t *pPowerSrvSM = (PowerSrvSM_t*)pPowerSrv->hPowerSrvSM;
    /*
    init PowerSrv state machine.
    */
	/*
	the PowerSrvSM start in active mode (POWER_SRV_STATE_ACTIVE)
	the PowerSrvSM::currentState must be sync with the PowerSrv::desiredPowerModeProfile (POWER_MODE_ACTIVE).
	*/
	pPowerSrvSM->currentState = POWER_SRV_STATE_ACTIVE;
    pPowerSrv->currentMode = USER_MODE;
    tmr_StopTimer (pPowerSrvSM->hPwrSrvSmTimer);

    /*init all request with init values*/
    powerSrvCreatePssRequest(hPowerSrv,
							USER_MODE,
							HANDLED_REQUEST,
							POWER_SAVE_OFF,
							TI_FALSE,
							NULL,
							NULL,
							NULL);
    powerSrvCreatePssRequest(hPowerSrv,
							DRIVER_MODE,
							HANDLED_REQUEST,
							POWER_SAVE_OFF,
							TI_FALSE,
							NULL,
							NULL,
							NULL);
	pPowerSrv->userLastRequestMode = (powerSrvMode_e)POWER_SAVE_OFF;
    pPowerSrv->pCurrentRequest 	= & pPowerSrv->userRequest;

	/*
	register for Event 
	*/
	eventMbox_RegisterEvent (pPowerSrv->hEventMbox,
							 TWD_OWN_EVENT_PS_REPORT,
							 (void *)powerSrv802_11PsReport,
							 hPowerSrv);

	eventMbox_UnMaskEvent (pPowerSrv->hEventMbox, TWD_OWN_EVENT_PS_REPORT, NULL, NULL);

    return TI_OK;
}

/****************************************************************************************
 *                        powerSrv_config                                                           *
 ****************************************************************************************
DESCRIPTION: Power Server module configuration function, called by the MAC Services in configure phase 
                performs the following:
                -   init the Power server to active state.
                -   call the init function of the state machine.
                                                                                                                   
INPUT:      - hPowerSrv             - handle to the PowerSrv object.            
            - pPowerSrvInitParams   - the Power Server initialize parameters.

OUTPUT: 
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS powerSrv_config(  TI_HANDLE               hPowerSrv,
                            TPowerSrvInitParams    *pPowerSrvInitParams)
{
    powerSrv_t* pPowerSrv = (powerSrv_t*)hPowerSrv;

    /*
    config PowerSrv state machine.
    */
    powerSrvSM_config(   pPowerSrv->hPowerSrvSM,
                         pPowerSrvInitParams);

    return TI_OK;
}
/****************************************************************************************
 *                        powerSrv_SetPsMode                                                            *
 ****************************************************************************************
DESCRIPTION: This function is a user mode request from the Power Save Server.
              it will create a Request from the "USER_REQUEST" and will try to perform the user request for PS/Active.
              this will be done in respect of priority to Driver request.
                                                                                                                   
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.        
            - psMode                            - Power save/Active request
            - sendNullDataOnExit                - 
            - powerSaveCBObject     - handle to the Callback function module.
            - powerSaveCompleteCB           - Callback function - for success/faild notification.
OUTPUT: 
RETURN:    TI_STATUS - TI_OK / PENDING / TI_NOK.
****************************************************************************************/
TI_STATUS powerSrv_SetPsMode(   TI_HANDLE                   hPowerSrv,
                                E80211PsMode                psMode,
                                TI_BOOL                        sendNullDataOnExit,
                                void *                      powerSaveCBObject,
                                powerSaveCmpltCB_t          powerSaveCompleteCB,
                                powerSaveCmdResponseCB_t    powerSavecmdResponseCB)

{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;
    TI_STATUS status;
/*creating the request from type - "user"*/
    powerSrvCreatePssRequest(hPowerSrv,
                             USER_MODE,
                             NEW_REQUEST,
                             psMode,
                             sendNullDataOnExit,
                             powerSaveCBObject,
                             powerSaveCompleteCB,
                             powerSavecmdResponseCB);

/*the request will be handled if the Power server is not in Driver mode.*/
    if ( pPowerSrv->currentMode==USER_MODE )
    {
        status = powerSrvProcessRequest(hPowerSrv,pPowerSrv->userRequest.requestMode);
    }
    else/*driver mode*/
    {
        pPowerSrv->userRequest.requestState = PENDING_REQUEST;
        status = POWER_SAVE_802_11_PENDING;
    }
    return status;

}


/****************************************************************************************
 *                        powerSrv_ReservePS                                                        *
 ****************************************************************************************
DESCRIPTION: This function is a driver mode request to set the 802.11 Power Save state and reserve the module.
              The module should not be in driver mode when this request is made. 
              If this function is called when the module is already in driver mode the result is unexpected. 
              If the request cannot be fulfilled because of currently executing user mode request, 
              then the function will return PENDING and the powerSaveCompleteCB function will be called when the request is fulfilled. 
              If the request can be fulfilled immediately and the Power Save state required is the current state 
              (This is always the case when PSMode = KEEP_CURRENT),
              then the module will be reserved and the function will return TI_OK - the callback function will not be called.?? 
              If the request can be fulfilled immediately and requires a Power Save state transition,
              then the return value will be TI_OK and the powerSaveCompleteCB function will be called by the Power Save Server
              when the request is complete.
                                                                                                                   
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.        
            - psMode                            - Power save/Active request
            - sendNullDataOnExit                - 
            - powerSaveCBObject     - handle to the Callback function module.
            - powerSaveCompleteCB           - Callback function - for success/faild notification.
OUTPUT: 
RETURN:    TI_STATUS - TI_OK / PENDING / TI_NOK.
****************************************************************************************/
TI_STATUS powerSrv_ReservePS(   TI_HANDLE               hPowerSrv,
                                E80211PsMode            psMode,
                                TI_BOOL                    sendNullDataOnExit,
                                void *                  powerSaveCBObject,
                                powerSaveCmpltCB_t      powerSaveCompleteCB)
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;
    TI_STATUS status;

    /*creating the request from type - "driver"*/
    if ( psMode == POWER_SAVE_KEEP_CURRENT )
    {
        psMode = pPowerSrv->userRequest.psMode;
    }

    powerSrvCreatePssRequest(hPowerSrv,
                             DRIVER_MODE,
                             NEW_REQUEST,
                             psMode,
                             sendNullDataOnExit,
                             powerSaveCBObject,
                             powerSaveCompleteCB,
                             NULL);
    /*try to execute the request*/
    status = powerSrvProcessRequest(hPowerSrv,pPowerSrv->driverRequest.requestMode);
    return status;

}


/****************************************************************************************
 *                        powerSrv_ReleasePS                                                        *
 ****************************************************************************************
DESCRIPTION: This function is used to release a previous driver mode request issued with the ReservPS API. 
              it creates a Driver request and the server act like it is a normal driver request.
              the server will send the request with a simple optimization - if there is a pending or 
              new user request - the request will be added in the driver request, in this way when the 
              user request  will be executed there will be nothing to do, in the same manner if there 
              are no user / driver request to execute we will send the last user request in Driver mode. 
               
              
              
                                                                                                                   
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.        
            - sendNullDataOnExit                - 
            - powerSaveCBObject     - handle to the Callback function module.
            - powerSaveCompleteCB           - Callback function - for success/faild notification.
OUTPUT: 
RETURN:    TI_STATUS - TI_OK / PENDING / TI_NOK.
****************************************************************************************/
TI_STATUS powerSrv_ReleasePS(   TI_HANDLE                   hPowerSrv,
                                TI_BOOL                        sendNullDataOnExit,
                                void *                          powerSaveCBObject,
                                powerSaveCmpltCB_t              powerSaveCompleteCB)
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;
    TI_STATUS status;

    /*creating the request from type - "driver"*/

    if (pPowerSrv->driverRequest.requestMode == POWER_SAVE_802_11_PENDING)
    {
        powerSrvCreatePssRequest(hPowerSrv,
                                 DRIVER_MODE,
                                 HANDLED_REQUEST,
                                 POWER_SAVE_OFF,
                                 TI_FALSE,
                                 NULL,
                                 NULL,
                                 NULL);
        return POWER_SAVE_802_11_IS_CURRENT;
    }

    /*creating the request from type - "driver"*/
    powerSrvCreatePssRequest(hPowerSrv,
                             DRIVER_MODE,
                             NEW_REQUEST,
                             POWER_SAVE_KEEP_CURRENT,
                             sendNullDataOnExit,
                             powerSaveCBObject,
                             powerSaveCompleteCB,
                             NULL);
    if ( pPowerSrv->userRequest.requestState == NEW_REQUEST ||
         pPowerSrv->userRequest.requestState == PENDING_REQUEST )
    {
        pPowerSrv->driverRequest.psMode = pPowerSrv->userRequest.psMode;
    }
    else
    {
        pPowerSrv->driverRequest.psMode = (E80211PsMode)pPowerSrv->userLastRequestMode;
    }



    status = powerSrvProcessRequest(hPowerSrv,pPowerSrv->driverRequest.requestMode);
    /*if the request was not executed we should not change the mode*/
    if (status != POWER_SAVE_802_11_PENDING)
    {
        pPowerSrv->currentMode = USER_MODE;
    }
    return status;
}



/****************************************************************************************
 *                        powerSrv_getPsStatus                                                       *
 *****************************************************************************************
DESCRIPTION: This function returns the true state of power.
                                                                                                                                                                       
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.        
            
OUTPUT: 
RETURN:    TI_BOOL - true if the SM is in PS state -  false otherwise
****************************************************************************************/
TI_BOOL powerSrv_getPsStatus(TI_HANDLE hPowerSrv)
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;
    PowerSrvSMStates_e smState;
    smState = powerSrvSM_getCurrentState(pPowerSrv->hPowerSrvSM);
    return(smState == POWER_SRV_STATE_PS );
}


/****************************************************************************************
*                        powerSrv_SetRateModulation                                                         *
*****************************************************************************************
DESCRIPTION: Sets the rate modulation according to the current Radio Mode.
                                                                                                                                                                      
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.
           - dot11mode_e - The current radio mode (A or G)
           
OUTPUT: 
RETURN:    TI_BOOL - true if the SM is in PS state -  false otherwise
****************************************************************************************/
void powerSrv_SetRateModulation(TI_HANDLE hPowerSrv, TI_UINT16  rate)
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;
    powerSrvSM_setRateModulation(pPowerSrv->hPowerSrvSM,rate);

    return;
}

/**
 * \Gets the rate modulation.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n
 * 2) dot11mode_e - The current radio mode (A or G)
 * Return: None.\n
 */
TI_UINT32 powerSrv_GetRateModulation(TI_HANDLE hPowerSrv)
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;
    return powerSrvSM_getRateModulation(pPowerSrv->hPowerSrvSM);
}




/*****************************************************************************
 **         Private Function prototypes                                                             **
 *****************************************************************************/


/****************************************************************************************
 *                        powerSrv802_11PsReport                                                    *
 ****************************************************************************************
DESCRIPTION:  This function is the call back for the TWD control when a PS event triggered
              This function is responsible for the process "keep alive".
              the function handles the event and sends it to the state machine, after sending the events 
              the function handles the next request with respect to driver request priority.
              if a request is already done then we will call the request call back (if exist!).
               
              
              
                                                                                                                   
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.        
            - str                               - Event string   
            - strLen                            - string length

OUTPUT: 
RETURN:    void.
****************************************************************************************/
static void powerSrv802_11PsReport(TI_HANDLE hPowerSrv, char* str , TI_UINT32 strLen)
{
    powerSrv_t *    pPowerSrv = (powerSrv_t*)hPowerSrv;
    TI_UINT8           PowerSaveStatus;
    E80211PsMode    currentPsMode;
    TI_STATUS       status = TI_OK;

    /*copy the event*/
    os_memoryCopy(pPowerSrv->hOS, (void *)&PowerSaveStatus, (void *)str, strLen);

    TRACE1( pPowerSrv->hReport, REPORT_SEVERITY_INFORMATION, "PS callback with status: %d\n", PowerSaveStatus);

    /* Handling the event*/
    switch ( (EventsPowerSave_e)PowerSaveStatus )
    {
    case ENTER_POWER_SAVE_FAIL:
    case EXIT_POWER_SAVE_FAIL:
        TRACE0( pPowerSrv->hReport, REPORT_SEVERITY_WARNING, "Power save enter or exit failed!\n");
        powerSrvSM_SMApi(pPowerSrv->hPowerSrvSM,POWER_SRV_EVENT_FAIL);
        break;

    case ENTER_POWER_SAVE_SUCCESS:
	case EXIT_POWER_SAVE_SUCCESS:
        powerSrvSM_SMApi(pPowerSrv->hPowerSrvSM,POWER_SRV_EVENT_SUCCESS);
        /*update the last user request if the request was a user request*/
        if ( pPowerSrv->currentMode == USER_MODE )
        {
            pPowerSrv->userLastRequestMode= (powerSrvMode_e)pPowerSrv->userRequest.psMode;
        }
        break;

    default:
        TRACE1( pPowerSrv->hReport, REPORT_SEVERITY_ERROR, "Unrecognized status at PS callback %d\n", PowerSaveStatus );
        break;
    }

    /*this reflects the true power save state - power save IFF state machine in PS state.*/
    if ( (EventsPowerSave_e)PowerSaveStatus == ENTER_POWER_SAVE_SUCCESS )
    {
        currentPsMode = POWER_SAVE_ON;
    }
    else
    {
        currentPsMode = POWER_SAVE_OFF;
    }

    /*in case of  request has been already handled - calling the CB*/
    if ( pPowerSrv->pCurrentRequest->requestState == HANDLED_REQUEST )
    {
        if ( pPowerSrv->pCurrentRequest->powerSrvCompleteCB != NULL )
        {
            pPowerSrv->pCurrentRequest->powerSrvCompleteCB( pPowerSrv->pCurrentRequest->powerSaveCBObject,
                                                            currentPsMode,
                                                            (EventsPowerSave_e)PowerSaveStatus);

        }
    }

    /*starting again to handle waiting requests  */
    /*priority to driver request*/
    if ( pPowerSrv->driverRequest.requestState == NEW_REQUEST ||
         pPowerSrv->driverRequest.requestState == PENDING_REQUEST )
    {
        status = powerSrvProcessRequest(hPowerSrv,pPowerSrv->driverRequest.requestMode);
    }
    else/*user request*/
    {
        if ( pPowerSrv->currentMode==USER_MODE )
        {
            if ( pPowerSrv->userRequest.requestState == NEW_REQUEST||
                 pPowerSrv->userRequest.requestState == PENDING_REQUEST )
            {
                status = powerSrvProcessRequest(hPowerSrv,pPowerSrv->userRequest.requestMode);
            }

        }
    }
    if ( status == POWER_SAVE_802_11_IS_CURRENT )/*in case of already or habdled*/
    {
        if ( pPowerSrv->pCurrentRequest->powerSrvCompleteCB != NULL )
        {
            pPowerSrv->pCurrentRequest->powerSrvCompleteCB(pPowerSrv->pCurrentRequest->powerSaveCBObject,
                                                           pPowerSrv->pCurrentRequest->psMode,
                                                           ((pPowerSrv->pCurrentRequest->psMode == POWER_SAVE_ON) ? 
                                                            ENTER_POWER_SAVE_SUCCESS : 
                                                            EXIT_POWER_SAVE_SUCCESS));
        }
    }


}


/****************************************************************************************
 *                        powerSrvProcessRequest                                                    *
 ****************************************************************************************
DESCRIPTION: This function receive the request before sending it to the state machine, checks if it 
              possible to be applied and pass it to the state machine.
              
              
                                                                                                                   
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.        
            - requestMode                   - Driver or User mode


OUTPUT: 
RETURN:    TI_STATUS - TI_OK / PENDING / TI_NOK.
****************************************************************************************/
TI_STATUS powerSrvProcessRequest (TI_HANDLE hPowerSrv, powerSrvMode_e requestMode)
{
    PowerSrvSMStates_e powerSrvSmState;
    powerSrvRequest_t*      pPrcessedRequest;
    TI_STATUS smApiStatus;
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;



/*determine what is the current request*/
    if ( requestMode == DRIVER_MODE )
    {
        pPrcessedRequest = &(pPowerSrv->driverRequest);     
    }
    else
    {
        pPrcessedRequest = &(pPowerSrv->userRequest);
    }

/*in case that the state machine is in a pending state and it is a driver 
   request we will return Pending and not call the SM. the request will 
   be processed in the next event - according to the 802_11_Report.*/
    powerSrvSmState = powerSrvSM_getCurrentState(pPowerSrv->hPowerSrvSM);

    if ( (powerSrvSmState == POWER_SRV_STATE_PEND_ACTIVE || 
          powerSrvSmState == POWER_SRV_STATE_PEND_PS) &&
         pPowerSrv->pCurrentRequest->requestMode == DRIVER_MODE )
    {
        pPrcessedRequest->requestState = PENDING_REQUEST;
        return POWER_SAVE_802_11_PENDING;
    }
    /*Set the correct request to the SM*/
    powerSrvSm_setSmRequest(pPowerSrv->hPowerSrvSM ,pPrcessedRequest);      

    /*call the SM with the correct request*/

    if ( pPrcessedRequest->psMode == POWER_SAVE_ON )
    {
        smApiStatus = powerSrvSM_SMApi(pPowerSrv->hPowerSrvSM,POWER_SRV_EVENT_REQUEST_PS);
    }
    else
    {
        smApiStatus = powerSrvSM_SMApi(pPowerSrv->hPowerSrvSM,POWER_SRV_EVENT_REQUEST_ACTIVE);
    }

    /*if =! pending updating the current request pointer.*/
    if ( pPrcessedRequest->requestState != PENDING_REQUEST )
    {
        pPowerSrv->pCurrentRequest = pPrcessedRequest;
        pPowerSrv->currentMode = pPowerSrv->pCurrentRequest->requestMode;
    }


    return smApiStatus;
}


/****************************************************************************************
 *                        powerSrvCreatePssRequest                                                  *
 ****************************************************************************************
DESCRIPTION: This function create a request acording to it's type:
                                                        - User
                                                        -Driver
                
                                                                                                                   
INPUT:      - hPowerSrv                         - handle to the PowerSrv object.        
            - requestMode                   - request type : Driver/User
            - psMode                            - Power save/Active request
            - sendNullDataOnExit                - 
            - powerSaveCBObject     - handle to the Callback functin module.
            - powerSaveCompleteCB           - Calback function - for success/faild notification.

OUTPUT: 
RETURN:    void.
****************************************************************************************/
void powerSrvCreatePssRequest (TI_HANDLE                    hPowerSrv,
                               powerSrvMode_e              requestMode,
                               powerSrvRequestState_e      requestState,
                               E80211PsMode                psMode,
                               TI_BOOL                        sendNullDataOnExit,
                               void *                      powerSaveCBObject,
                               powerSaveCmpltCB_t          powerSaveCompleteCB,
                               powerSaveCmdResponseCB_t    powerSaveCmdResponseCB)
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;
    if ( requestMode==USER_MODE )
    {
        pPowerSrv->userRequest.requestState             = requestState;
        pPowerSrv->userRequest.requestMode              = requestMode;
        pPowerSrv->userRequest.psMode                   = psMode;
        pPowerSrv->userRequest.sendNullDataOnExit           = sendNullDataOnExit;
        pPowerSrv->userRequest.powerSaveCBObject            = powerSaveCBObject;
        pPowerSrv->userRequest.powerSrvCompleteCB       = powerSaveCompleteCB;
        pPowerSrv->userRequest.powerSaveCmdResponseCB   = powerSaveCmdResponseCB;
    }
    else /*driver request*/
    {
        pPowerSrv->driverRequest.requestState               = requestState;
        pPowerSrv->driverRequest.requestMode                = requestMode;
        pPowerSrv->driverRequest.psMode                 = psMode;
        pPowerSrv->driverRequest.sendNullDataOnExit             = sendNullDataOnExit;
        pPowerSrv->driverRequest.powerSaveCBObject      = powerSaveCBObject;
        pPowerSrv->driverRequest.powerSrvCompleteCB         = powerSaveCompleteCB;
        pPowerSrv->driverRequest.powerSaveCmdResponseCB     = NULL;
    }
}



/****************************************************************************************
 *                        powerSrvRegisterFailureEventCB                                                    *
 ****************************************************************************************
DESCRIPTION: Registers a failure event callback for scan error notifications.
                
                                                                                                                   
INPUT:      - hPowerSrv         - handle to the PowerSrv object.        
            - failureEventCB    - the failure event callback function.\n
            - hFailureEventObj - handle to the object passed to the failure event callback function.

OUTPUT: 
RETURN:    void.
****************************************************************************************/
void powerSrvRegisterFailureEventCB( TI_HANDLE hPowerSrv, 
                                     void * failureEventCB, TI_HANDLE hFailureEventObj )
{
    powerSrv_t *pPowerSrv = (powerSrv_t*)hPowerSrv;

    pPowerSrv->failureEventFunc = (TFailureEventCb)failureEventCB;
    pPowerSrv->failureEventObj  = hFailureEventObj;

    /* register the failure event CB also with the PS SM */
    powerSrvSM_RegisterFailureEventCB( pPowerSrv->hPowerSrvSM, failureEventCB, hFailureEventObj );
}

