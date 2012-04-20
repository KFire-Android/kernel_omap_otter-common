/*
 * PowerMgr_API.h
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

/** \file PowerMgr_API.h
 *  \brief This is the Power Manager module API.
 *  \
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Power Manager                                                 *
 *   PURPOSE: Power Manager Module API                                      *
 *                                                                          *
 ****************************************************************************/

#ifndef _POWER_MGR_API_H_
#define _POWER_MGR_API_H_

#include "tidef.h"
#include "paramOut.h"
#include "DrvMainModules.h"

/*****************************************************************************
 **         Constants                                                       **
 *****************************************************************************/


/*****************************************************************************
 **         Enumerations                                                    **
 *****************************************************************************/


/*****************************************************************************
 **         Typedefs                                                        **
 *****************************************************************************/


/*****************************************************************************
 **         Structures                                                      **
 *****************************************************************************/


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
 * \
 * \date 24-Oct-2005\n
 * \brief Creates the object of the power Manager.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the OS.\n
 * Return Value: TI_HANDLE - handle to the PowerMgr object.\n
 */
TI_HANDLE PowerMgr_create(TI_HANDLE theOsHandle);

/**
 * \
 * \date 24-Oct-2005\n
 * \brief Destroy the object of the power Manager.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 */
TI_STATUS PowerMgr_destroy(TI_HANDLE thePowerMgrHandle);

/**
 * \
 * \date 24-Oct-2005\n
 * \brief Initialization of the PowerMgr module.
 *
 * Function Scope \e Public.\n
 * Parameters:    pStadHandles - The driver modules handles  \n
 * Return Value:  void  \n
 */
void PowerMgr_init (TStadHandlesList *pStadHandles);

TI_STATUS PowerMgr_SetDefaults (TI_HANDLE hPowerMgr, PowerMgrInitParams_t* pPowerMgrInitParams);

/**
 * \
 * \date 24-Oct-2005\n
 * \brief Start the power save algorithm of the driver and also the 802.11 PS.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 * \b Description:\n
 * PsEnable = true, and decide on the proper power mode.
 */
TI_STATUS PowerMgr_startPS(TI_HANDLE thePowerMgrHandle);

/**
 * \
 * \date 24-Oct-2005\n
 * \brief stop the power save algorithm of the driver and also the 802.11 PS.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * 2) TI_BOOL - indicates if this is roaming (FALSE) or disconnect (TRUE)
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 * \b Description:\n
 * PsEnable = false, and set the power mode to active.
 */
TI_STATUS PowerMgr_stopPS(TI_HANDLE thePowerMgrHandle, TI_BOOL bDisconnect);

/**
 * \
 * \date 24-Oct-2005\n
 * \brief returns the 802.11 power save status (enable / disable).
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * Return Value: TI_BOOL - TI_TRUE if enable else TI_FALSE.\n
*/
TI_BOOL PowerMgr_getPsStatus(TI_HANDLE thePowerMgrHandle);



/**
 * \
 * \date 24-Oct-2005\n
 * \brief Configure of the PowerMode (auto / active / short doze / long doze).
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * 2) PowerMgr_PowerMode_e - the requested power mode (auto / active / short doze / long doze).
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 * \b Description:\n
 * desiredPowerModeProfile = PowerMode input parameter, and set the proper power mode.
*/
TI_STATUS PowerMgr_setPowerMode(TI_HANDLE thePowerMgrHandle);


/**
 * \
 * \date 24-Oct-2005\n
 * \brief Get the current PowerMode of the PowerMgr module.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * Return Value: PowerMgr_PowerMode_e .\n
 */
PowerMgr_PowerMode_e PowerMgr_getPowerMode(TI_HANDLE thePowerMgrHandle);
    
TI_STATUS powerMgr_getParam(TI_HANDLE thePowerMgrHandle,
                            paramInfo_t *theParamP);
TI_STATUS powerMgr_setParam(TI_HANDLE thePowerMgrHandle,
                            paramInfo_t *theParamP);


/**
 * \
 * \date 20-July-2004\n
 * \brief print configuration of the PowerMgr object - use for debug!
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * Return Value: void.\n
 */
void PowerMgr_printObject(TI_HANDLE thePowerMgrHandle);

/**
 * \date 10-April-2007\n
 * \brief reset PM upon recovery event.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * Return Value: void.\n
 */

TI_STATUS PowerMgr_notifyFWReset(TI_HANDLE hPowerMgr);

TI_BOOL PowerMgr_getReAuthActivePriority(TI_HANDLE thePowerMgrHandle);

#endif /*_POWER_MGR_API_H_*/
