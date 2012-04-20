/*
 * PowerSrv_API.h
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

/** \file powerSrv_API.h
 *  \brief This is the Power Manager module API.
 *  \author Yaron Menashe
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Power Server                                                 *
 *   PURPOSE: Power Server Module API                                      *
 *                                                                          *
 ****************************************************************************/

#ifndef _POWER_SRV_API_H_
#define _POWER_SRV_API_H_

#include "MacServices_api.h"

/*****************************************************************************
 **         Constants                                                       **
 *****************************************************************************/


/*****************************************************************************
 **         Enumerations                                                    **
 *****************************************************************************/



/*****************************************************************************
 **         Typedefs                                                        **
 *****************************************************************************/
/*typedef void (*powerSaveCmdResponseCB_t )(TI_HANDLE cmdResponseHandle,TI_UINT8 MboxStatus);*/
typedef TPowerSaveResponseCb powerSaveCmdResponseCB_t;
/*typedef void (*powerSaveCmpltCB_t )(TI_HANDLE powerSaveCmpltHandle,TI_UINT8 PSMode,TI_UINT8 transStatus);*/
typedef TPowerSaveCompleteCb powerSaveCmpltCB_t;
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
 * \author Assaf Azulay
 * \date 20-Oct-2005\n
 * \brief Creates the object of the power Server.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the OS.\n
 * Return Value: TI_HANDLE - handle to the powerSrv object.\n
 */
TI_HANDLE powerSrv_create(TI_HANDLE hOs);


/**
 * \author Assaf Azulay
 * \date 27-Apr-2005\n
 * \brief Destroy the object of the power Server.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the powerSrv object.\n
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 */
TI_STATUS powerSrv_destroy(TI_HANDLE hPowerSrv);


/**
  * \author Assaf Azulay
 * \date 24-Oct-2005\n
 * \brief Initialization of the powerSrv module.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the powerSrv object.\n
 * 3) TI_HANDLE - handle to the Report object.\n
 * 2) TI_HANDLE - handle to the EventMbox object.\n
 * 4) TI_HANDLE - handle to the CommandBuilder object.\n
 * 5) TI_HANDLE - handle to the Timer module object.\n
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 */
TI_STATUS powerSrv_init (TI_HANDLE hPowerSrv,
                         TI_HANDLE hReport,
                         TI_HANDLE hEventMbox,
                         TI_HANDLE hCmdBld,
                         TI_HANDLE hTimer);


TI_STATUS powerSrv_config(TI_HANDLE 				hPowerSrv,
				          TPowerSrvInitParams      *pPowerSrvInitParams);

/**
  * \author Assaf Azulay
 * \date 24-Oct-2005\n
 * \brief request PS by User
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) hPowerSrv 						- handle to the PowerSrv object.\n		
 * 2) psMode							- Power save/Active request.\n
 * 3) sendNullDataOnExit				- \n
 * 4) powerSaveCompleteCBObject		- handle to the Callback functin module.\n
 * 5) powerSaveCompleteCB				- Calback function - for success/faild notification.\n
 * 6) powerSavecmdResponseCB			- Calback function - for GWSI success/faild notification.\n
 * Return Value: TI_STATUS - TI_OK / PENDING / TI_NOK.\n
 * \b Description:\n
 * This function is a user mode request from the Power Save Server./n
 * it will create a Request from typ "USER_REQUEST" and will try to perform the user request for PS/Active./n
 * this will be done in respect of priority to Driver request./n
 */
TI_STATUS powerSrv_SetPsMode (TI_HANDLE 					hPowerSrv,
                              E80211PsMode	                psMode,
 							  TI_BOOL  						sendNullDataOnExit,
 						      void * 						powerSaveCompleteCBObject,
 						      powerSaveCmpltCB_t  			powerSaveCompleteCB,
 						      powerSaveCmdResponseCB_t	    powerSavecmdResponseCB);


/**
  * \author Assaf Azulay
 * \date 24-Oct-2005\n
 * \brief SW configure, use to override the current PowerMode (what ever it will be) to
 *        active/PS combined with awake/power-down. use for temporary change the system policy.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the powerSrv object.\n
 * 2) powerSrv_RequestFor_802_11_PS_e - the driver mode obliged to be in 802.11 PS or not change.\n
 * 3) PowerCtrl_PowerLevel_e - the desired driver power level (allowed: AWAKE or POWER DOWN).\n
 * 4) TI_HANDLE theObjectHandle - the handle the object that need the PS success/fail notification.\n
 * 5) ps802_11_NotificationCB_t - the callback function.\n
 * 6) char* - the clinet name that ask for driver mode.\n
 * Return Value: TI_STATUS - if success (already in power save) then TI_OK,\n
 *                           if pend (wait to ACK form AP for the null data frame) then PENDING\n
 *                           if PS isn't enabled then POWER_SAVE_802_11_NOT_ALLOWED\n
 *                           else TI_NOK.\n
 * \b Description:\n
 * enter in to configuration of the driver that in higher priority from the user.\n
 * the configuration is:\n
 *  - to enter to802.11 PS or not (if not this isn't a request to get out from 802.11 PS).\n
 *  - to change the HW power level to awake or power-down if not already there.
 *    this is a must request.\n
*/
TI_STATUS powerSrv_ReservePS (TI_HANDLE 				hPowerSrv,
							  E80211PsMode              psMode,
 						 	  TI_BOOL  					sendNullDataOnExit,
 							  void * 					powerSaveCBObject,
							  powerSaveCmpltCB_t 		powerSaveCompleteCB);


/**
 * \author Assaf Azulay
 * \date 24-Oct-2005\n
 * \brief end the temporary change of system policy, and returns to the user system policy.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the powerSrv object.\n
 * 2) char* - the clinet name that ask for driver mode.\n
 * Return Value: TI_STATUS - TI_OK on success else TI_NOK.\n
 * \b Description:\n
 * enter in to configuration of the driver that in higher priority from the user.\n
 * the configuration is:\n
 * end the user mode configuration (driver mode priority) and returns the user configuration
 * (user mode priority).
*/
TI_STATUS powerSrv_ReleasePS( 	TI_HANDLE 					hPowerSrv,
									TI_BOOL  						sendNullDataOnExit,
 						 			void *  						powerSaveCBObject,
 									powerSaveCmpltCB_t  			powerSaveCompleteCB);


/**
 * \author Assaf Azulay
 * \date 24-Oct-2005\n
 * \brief reflects the actual state of the state machine
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the powerSrv object.\n
 * Return Value:\n 
 * TI_BOOL - thre is in PS false otherwise.\n
*/
TI_BOOL powerSrv_getPsStatus(TI_HANDLE hPowerSrv);


/**
 * \author Assaf Azulay
 * \date 24-Oct-2005\n
 * \sets the rate as got from user else sets default value.\n
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE 	- handle to the powerSrv object.\n
 * 2) TI_UINT16		- desierd rate .\n
 * Return Value:\n 
 * void.\n
*/
void powerSrv_SetRateModulation(TI_HANDLE hPowerSrv, TI_UINT16  rate);



/**
 * \author Assaf Azulay
 * \date 9-Mar-2006\n
 * \brief Registers a failure event callback for scan error notifications.
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * - hPowerSrv 		- handle to the PowerSrv object.		
 * - failureEventCB 	- the failure event callback function.
 * - hFailureEventObj 	- handle to the object passed to the failure event callback function.
*/
void powerSrvRegisterFailureEventCB( TI_HANDLE hPowerSrv, 
                                     void * failureEventCB, TI_HANDLE hFailureEventObj );


/**
 * \date 03-Jul-2006\n
 * \return the rate as it was seted by powerSrv_SetRateModulation.\n
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE 	- handle to the powerSrv object.\n
 * Return Value: TI_UINT16		- desierd rate .\n
 * void.\n
*/
TI_UINT32 powerSrv_GetRateModulation(TI_HANDLE hPowerSrv);

TI_STATUS powerSrv_restart(	TI_HANDLE hPowerSrv);

#endif /*  _POWER_SRV_API_H_  */
