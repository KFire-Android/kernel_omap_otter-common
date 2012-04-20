/*
 * PowerSrvSM.h
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

/** \file PowerSrvSM.h
 *  \brief This is the PowerSrv module API.
 *  \author Assaf Azulay
 *  \date 6-Oct-2005
 */

/****************************************************************************
 *                                                                                                        *
 *   MODULE:  PowerSrv                                                                              *
 *   PURPOSE: Power Server State machine API                                                    *
 *                                                                                                              *
 ****************************************************************************/

#ifndef _POWER_SRV_SM_H_
#define _POWER_SRV_SM_H_

#include "PowerSrv_API.h"
#include "PowerSrv.h"



/*****************************************************************************
 **         Constants                                                       **
 *****************************************************************************/

#define POWER_SAVE_GUARD_TIME_MS            5000       /* The gaurd time used to protect from FW stuck */

/*****************************************************************************
 **         Enumerations                                                    **
 *****************************************************************************/

/** \enum PowerSrvSMEvents_e */
typedef enum 
{
        POWER_SRV_EVENT_REQUEST_ACTIVE , 
    POWER_SRV_EVENT_REQUEST_PS ,
    POWER_SRV_EVENT_SUCCESS,
    POWER_SRV_EVENT_FAIL ,
    POWER_SRV_SM_EVENT_NUM
}PowerSrvSMEvents_e;

/** \enum PowerSrvSMStates_e */
typedef enum 
{
    POWER_SRV_STATE_ACTIVE = 0,
    POWER_SRV_STATE_PEND_PS ,
    POWER_SRV_STATE_PS ,
    POWER_SRV_STATE_PEND_ACTIVE ,
    POWER_SRV_STATE_ERROR_ACTIVE,
    POWER_SRV_SM_STATE_NUM
}PowerSrvSMStates_e;



/*****************************************************************************
 **         Structures                                                      **
 *****************************************************************************/

/** \struct PowerSrvSM_t */
typedef struct
{
    TI_HANDLE               hCmdBld;                    /**< 
                                                         * Handle to the power controller object via the command builder.
                                                         * Need for configure the desired power mode policy in the system.
                                                         */

    TI_HANDLE               hOS;                        /**< Handle to the OS object. */

    TI_HANDLE               hReport;                    /**< Handle to the Report module. */

    TI_HANDLE               hFSM;                       /**< Handle to the State machine module. */

    TI_HANDLE               hTimer;                     /**< Handle to the Timer module. */

    TI_HANDLE               hPwrSrvSmTimer;             /**< Guard timer for PS commands sent to the FW */

    PowerSrvSMStates_e      currentState;               /**< the current state of the state machine. */

    powerSrvRequest_t*      pSmRequest;                 /**< pointer to the relevant request in the power server. */

    TI_UINT8                hangOverPeriod;             /**< parameter for the FW */

    TI_UINT8                numNullPktRetries;          /**< parameter for the FW */
    
    EHwRateBitFiled         NullPktRateModulation;      /**< parameter for the FW */

    TFailureEventCb         failureEventCB;             /**< Failure event callback */

    TI_HANDLE               hFailureEventObj;           /**< Failure event object (supplied to the above callback) */
} PowerSrvSM_t;







/*****************************************************************************
 **         External data definitions                                       **
 *****************************************************************************/


/*****************************************************************************
 **         External functions definitions                                  **
 *****************************************************************************/


/*****************************************************************************
 **         Public Function prototypes                                      **
 *****************************************************************************/

/**
 * \author Assaf Azulay
 * \date 6-Oct-2005\n
 * \brief Creates the object of the PowerSrv.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the OS.\n
 * Return Value: TI_HANDLE - handle to the PowerSrv object.\n
 */
TI_HANDLE powerSrvSM_create(TI_HANDLE hOsHandle);

/**
 * \author Assaf Azulay
 * \date 6-Oct-2005\n
 * \brief Destroy the object of the PowerSrvSM.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrv object.\n
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 */
TI_STATUS powerSrvSM_destroy(TI_HANDLE thePowerSrvSMHandle);

/**
 * \author Assaf Azulay
 * \date 6-Oct-2005\n
 * \brief Initialize the PowerSrvSM module.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n
 * 2) TI_HANDLE - handle to the Report object.
 * 3) TI_HANDLE - handle to the Command Builder object.
 * 4) TI_HANDLE - handle to the Timer module object.
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 */
TI_STATUS powerSrvSM_init (TI_HANDLE hPowerSrvSM,
                           TI_HANDLE hReport,
                           TI_HANDLE hCmdBld,
                           TI_HANDLE hTimer);

TI_STATUS powerSrvSM_config(TI_HANDLE hPowerSrvSM,
                            TPowerSrvInitParams *pPowerSrvInitParams);
/**
 * \author Assaf Azulay
 * \date 6-Oct-2005\n
 * \brief return the component version.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n
 * Return Value: TI_UINT32 - component version code.\n
 */

TI_STATUS powerSrvSM_SMApi(TI_HANDLE hPowerSrvSM,
                                            PowerSrvSMEvents_e theSMEvent);


/**
 * \author Assaf Azulay
 * \date 020-Oct-2005\n
 * \brief This function sets the current SM working request.\n
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n
 * 2) powerSrvRequest_t* pSmRequest - desierd request./n
 * Return Value: TI_STATUS -  TI_OK.\n
 */
TI_STATUS powerSrvSm_setSmRequest(TI_HANDLE hPowerSrvSM,powerSrvRequest_t* pSmRequest);



/**
 * \author Assaf Azulay
 * \date 09-Jun-2004\n
 * \brief get the current state of the state machine.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n
 * Return Value: PowerCtrlSMStates_e.\n
 */
PowerSrvSMStates_e powerSrvSM_getCurrentState(TI_HANDLE hPowerSrvSM);


/**
 * \author Assaf Azulay
 * \date 20-July-2004\n
 * \brief sets rate modulation
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n
 * 2) rate_e rate modulation
 * Return Value: void.\n
 */
void powerSrvSM_setRateModulation(TI_HANDLE hPowerSrvSM, TI_UINT16 rateModulation);

/**
 * \brief sets rate modulation
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n* 
 * Return Value: TI_UINT16 Rate.\n
 */
TI_UINT32 powerSrvSM_getRateModulation(TI_HANDLE hPowerSrvSM);

/**
 * \author Assaf Azulay
 * \date 20-July-2004\n
 * \brief print configuration of the PowerSrvSM object - use for debug!
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerSrvSM object.\n
 * Return Value: void.\n
 */
void powerSrvSM_printObject(TI_HANDLE hPowerSrvSM);

/**
 * \author Ronen Kalish
 * \date 21-August-2006\n
 * \brief Registers a failure event callback for power save error notifications (timer expiry).\n
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * - hPowerSrvSM      - handle to the PowerSrv object.        
 * - failureEventCB     - the failure event callback function.
 * - hFailureEventObj   - handle to the object passed to the failure event callback function.
*/
void powerSrvSM_RegisterFailureEventCB( TI_HANDLE hPowerSrvSM, 
                                        void* failureEventCB, TI_HANDLE hFailureEventObj );
#endif /*  _POWER_SRV_SM_H_  */

