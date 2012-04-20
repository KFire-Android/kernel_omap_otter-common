/*
 * MacServices_api.h
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

/** \file MacServicesApi.h
 *  \brief This file include public definitions for the MacServices module, comprising its API.
 *  \author Yuval Adler
 *  \date 6-Oct-2005
 */

#ifndef __MACSERVICESAPI_H__
#define __MACSERVICESAPI_H__

#include "osApi.h"

/*
 ***********************************************************************
 *	Constant definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Enums.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Typedefs.
 ***********************************************************************
 */



/*
 ***********************************************************************
 *	Structure definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	External data definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	External functions definitions
 ***********************************************************************
 */
/**
 * \author Yuval Adler\n
 * \date 6-Oct-2004\n
 * \brief Creates MacServices module
 *
 * Function Scope \e Public.\n
 * \param hOS - handle to the OS object.\n
 */
TI_HANDLE MacServices_create( TI_HANDLE hOS );

/**
 * \author Yuval Adler\n
 * \date 6-Oct-2004\n
 * \brief Destroys MacServices module
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 */
void MacServices_destroy( TI_HANDLE hMacServices );

/**
 * \author Yuval Adler\n
 * \date  6-Oct-2004\n
 * \brief Initializes the MacServices module
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the Mac Services object.\n
 * \param hReport - handle to the report object.\n
 * \param hTWD - handle to the HAL ctrl object.\n
 * \param hCmdBld - handle to the HAL ctrl object.\n
 * \param hEventMbpx - handle to the HAL ctrl object.\n
 */
void MacServices_init (TI_HANDLE hMacServices, 
                       TI_HANDLE hReport, 
                       TI_HANDLE hTWD, 
                       TI_HANDLE hCmdBld, 
                       TI_HANDLE hEventMbox, 
                       TI_HANDLE hTimer);
void MacServices_config (TI_HANDLE hMacServices, TTwdInitParams *pInitParams);
void MacServices_restart (TI_HANDLE hMacServices);

void MacServices_registerFailureEventCB (TI_HANDLE hMacServices, void * failureEventCB, TI_HANDLE hFailureEventObj);


/***********************************************************************
 *	Scan SRV API functions
 ***********************************************************************/


/**
 * \author Ronen Kalish\n
 * \date 29-Dec-2004\n
 * \brief Registers a complete callback for scan complete notifications.
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 * \param scanCompleteCB - the complete callback function.\n
 * \param hScanCompleteObj - handle to the object passed to the scan complete callback function.\n
 */
void MacServices_scanSRV_registerScanCompleteCB( TI_HANDLE hMacServices, 
                                     TScanSrvCompleteCb scanCompleteCB, TI_HANDLE hScanCompleteObj );



/**
 * \author Ronen Kalish\n
 * \date 29-Dec-2004\n
 * \brief Performs a scan
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 * \param scanParams - the scan specific parameters.\n
 * \param eScanTag - tag used for result and scan complete tracking
 * \param bHighPriority - whether to perform a high priority (overlaps DTIM) scan.\n
 * \param bDriverMode - whether to try to enter driver mode (with PS on) before issuing the scan command.\n
 * \param bScanOnDriverModeError - whether to proceed with the scan if requested to enter driver mode and failed.\n
 * \param bSendNullData - whether to send Null data when exiting driver mode on scan complete.\n
 * \param psRequest - Parameter sent to PowerSaveServer on PS request to indicate PS on or "keep current" 
 * \param commandResponseFunc - CB function which called after downloading the command. \n
 * \param commandResponseObj -  The CB function Obj (Notice : last 2 params are NULL in Legacy run). \n
  * \return TI_OK if successful (various, TBD codes if not).\n
 */
TI_STATUS MacServices_scanSRV_scan( TI_HANDLE hMacServices, TScanParams *scanParams, EScanResultTag eScanTag,
                                    TI_BOOL bHighPriority, TI_BOOL bDriverMode, TI_BOOL bScanOnDriverModeError, 
						E80211PsMode psRequest, TI_BOOL bSendNullData,
						TCmdResponseCb commandResponseFunc, TI_HANDLE commandResponseObj );

/**
 * \author Ronen Kalish\n
 * \date 29-Dec-2004\n
 * \brief Stops a scan in progress
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 * \param eScanTag - scan tag, used for scan complete and result tracking
 * \param bSendNullData - indicates whether to send Null data when exiting driver mode.\n
 * \param commandResponseFunc - CB function which called after downloading the command. \n
 * \param commandResponseObj -  The CB function Obj (Notice : last 2 params are NULL in Legacy run). \n
 * \return TI_OK if successful (various, TBD codes if not).\n
 */
TI_STATUS MacServices_scanSRV_stopScan( TI_HANDLE hMacServices, EScanResultTag eScanTag, TI_BOOL bSendNullData,
                                        TCmdResponseCb commandResponseFunc, TI_HANDLE commandResponseObj );

/**
 * \author Ronen Kalish\n
 * \date 17-Jan-2005\n
 * \brief Notifies the scan SRV of a FW reset (that had originally been reported by a different module).\n
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 * \return TI_OK if successful (various, TBD codes if not).\n
 */
TI_STATUS MacServices_scanSRV_stopOnFWReset( TI_HANDLE hMacServices );

/**
 * \author Ronen Kalish\n
 * \date 29-Dec-2004\n
 * \brief callback function used by the power manager to notify driver mode result
 *
 * Function Scope \e Public.\n
 * \param hScanSRV - handle to the scan SRV object.\n
 * \param psStatus - the power save request status.\n
 */
void MacServices_scanSRV_powerSaveCB( TI_HANDLE hScanSRV, TI_UINT8 PSMode,TI_UINT8 psStatus );

/**
 * \author Ronen Kalish\n
 * \date 29-Dec-2004\n
 * \brief Callback function used by the HAL ctrl to notify scan complete
 *
 * Function Scope \e Public.\n
 * \param hScanSRV - handle to the scan SRV object.\n
 * \param str - pointer to scan result buffer (holding SPS status for SPS scan only!).\n
 * \param strLen - scan result buffer length (should ALWAYS be 2, even for non SPS scans).\n
 */
void MacServices_scanSRV_scanCompleteCB( TI_HANDLE hScanSRV, char* str, TI_UINT32 strLen );

/**
 * \author Ronen Kalish\n
 * \date 29-Dec-2004\n
 * \brief called when a scan timer expires. Completes the scan and starts a recovery process.
 *
 * Function Scope \e Public.\n
 * \param hScanSRV - handle to the scan SRV object.\n
 */
void MacServices_scanSRV_scanTimerExpired (TI_HANDLE hScanSRV, TI_BOOL bTwdInitOccured);

void MacServices_scanSrv_UpdateDtimTbtt (TI_HANDLE hMacServices, 
                                         TI_UINT8  uDtimPeriod, 
                                         TI_UINT16 uBeaconInterval);

#ifdef TI_DBG
/**
 * \author Shirit Brook\n
 * \date God knows when...\n
 * \brief Prints Scan Server SM status.\n
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the Mac Services object.\n
 */
void MacServices_scanSrv_printDebugStatus(TI_HANDLE hMacServices);
#endif

/*Power server API*/


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
TI_STATUS MacServices_powerSrv_SetPsMode(TI_HANDLE 	                hMacServices,
                                		 E80211PsMode	            psMode,
 									     TI_BOOL  						sendNullDataOnExit,
 						        		 void * 					powerSaveCompleteCBObject,
 						        		 TPowerSaveCompleteCb  		powerSaveCompleteCB,
 						        		 TPowerSaveResponseCb	    powerSavecmdResponseCB);


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
TI_STATUS MacServices_powerSrv_ReservePS(	TI_HANDLE 	                hMacServices,
                                            E80211PsMode 	            psMode,
                                            TI_BOOL  						sendNullDataOnExit,
                                            void * 						powerSaveCBObject,
                                            TPowerSaveCompleteCb 		powerSaveCompleteCB);


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
TI_STATUS MacServices_powerSrv_ReleasePS( 	TI_HANDLE 	hMacServices,
									TI_BOOL  						sendNullDataOnExit,
 						 			void *  						powerSaveCBObject,
 									TPowerSaveCompleteCb  			powerSaveCompleteCB);


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
TI_BOOL MacServices_powerSrv_getPsStatus(TI_HANDLE hMacServices);


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
void MacServices_powerSrv_SetRateModulation(TI_HANDLE hMacServices, TI_UINT16  rate);
/**
 * \Return the alrweady seted rate.\n
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * TI_HANDLE 	- handle to the powerSrv object.\n
 * Return Value: TI_UINT16		- desierd rate .\n
 * void.\n
*/
TI_UINT32 MacServices_powerSrv_GetRateModulation(TI_HANDLE hMacServices);




/***********************************************************************
 *	Measurement SRV API functions
 ***********************************************************************/

/**
 * \author Ronen Kalish\n
 * \date 09-November-2005\n
 * \brief Starts a measurement operation.\n
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 * \param pMsrRequest - a structure containing measurement parameters.\n
 * \param timeToRequestExpiryMs - the time (in milliseconds) the measurement SRV has to start the request.\n
 * \param cmdResponseCBFunc - callback function to used for command response.\n
 * \param cmdResponseCBObj - handle to pass to command response CB.\n
 * \param cmdCompleteCBFunc - callback function to be used for command complete.\n
 * \param cmdCompleteCBObj - handle to pass to command complete CB.\n
 * \return TI_OK if successful (various, TBD codes if not).\n
 */ 
TI_STATUS MacServices_measurementSRV_startMeasurement( TI_HANDLE hMacServices, 
                                                       TMeasurementRequest* pMsrRequest,
													   TI_UINT32 timeToRequestExpiryMs,
                                                       TCmdResponseCb cmdResponseCBFunc,
                                                       TI_HANDLE cmdResponseCBObj,
                                                       TMeasurementSrvCompleteCb cmdCompleteCBFunc,
                                                       TI_HANDLE cmdCompleteCBObj );

/**
 * \author Ronen Kalish\n
 * \date 09-November-2005\n
 * \brief Stops a measurement operation in progress.\n
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 * \param bSendNullData - whether to send NULL data when exiting driver mode.\n
 * \param cmdResponseCBFunc - callback function to used for command response.\n
 * \param cmdResponseCBObj - handle to pass to command response CB.\n
 * \return TI_OK if successful (various, TBD codes if not).\n
 */
TI_STATUS MacServices_measurementSRV_stopMeasurement( TI_HANDLE hMacServices,
													  TI_BOOL bSendNullData,
                                                      TCmdResponseCb cmdResponseCBFunc,
                                                      TI_HANDLE cmdResponseCBObj );

/**
 * \author Ronen Kalish\n
 * \date 09-November-2005\n
 * \brief Notifies the measurement SRV of a FW reset (recovery).\n
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 */
void MacServices_measurementSRV_FWReset( TI_HANDLE hMacServices );

/** 
 * \author Ronen Kalish\n
 * \date 09-November-2005\n
 * \brief callback function used by the power manager to notify driver mode result
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 * \param PSMode - the power save mode the STA is currently in.\n
 * \param psStatus - the power save request status.\n
 */
void MacServices_measurementSRV_powerSaveCB( TI_HANDLE hMeasurementSRV, TI_UINT8 PSMode,TI_UINT8 psStatus );

/** 
 * \author Ronen Kalish\n
 * \date 14-November-2005\n
 * \brief callback function used by the HAL for measure start event (sent when the FW 
 * has started measurement operation, i.e. switched channel and changed RX filters).\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 */
void MacServices_measurementSRV_measureStartCB( TI_HANDLE hMeasurementSRV );

/** 
 * \author Ronen Kalish\n
 * \date 14-November-2005\n
 * \brief callback function used by the HAL for measure stop event (sent when the FW 
 * has finished measurement operation, i.e. switched channel to serving channel and changed back RX filters).\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 */
void MacServices_measurementSRV_measureCompleteCB( TI_HANDLE hMeasurementSRV );

/** 
 * \author Ronen Kalish\n
 * \date 14-November-2005\n
 * \brief callback function used by the HAL for AP discovery stop event (sent when the FW 
 * has finished AP discovery operation).\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 */
void MacServices_measurementSRV_apDiscoveryCompleteCB( TI_HANDLE hMeasurementSRV );

/** 
 * \author Ronen Kalish\n
 * \date 16-November-2005\n
 * \brief Callback for channel load get param call.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 * \param status - the get_param call status.\n
 * \param CB_buf - pointer to the results buffer (already on the measurement SRV object)
 */
void MacServices_measurementSRV_channelLoadParamCB( TI_HANDLE hMeasurementSRV, TI_STATUS status, TI_UINT8* CB_buf );

/** 
 * \date 03-January-2005\n
 * \brief Dummy callback for channel load get param call. Used to clear the channel load tracker.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 * \param status - the get_param call status.\n
 * \param CB_buf - pointer to the results buffer (already on the measurement SRV object)
 */
void MacServices_measurementSRV_dummyChannelLoadParamCB( TI_HANDLE hMeasurementSRV, TI_STATUS status, TI_UINT8* CB_buf );

/** 
 * \author Ronen Kalish\n
 * \date 16-November-2005\n
 * \brief Callback for noise histogram get param call.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 * \param status - the get_param call status.\n
 * \param CB_buf - pointer to the results buffer (already on the measurement SRV object)
 */
void MacServices_measurementSRV_noiseHistCallBack(TI_HANDLE hMeasurementSRV, TI_STATUS status, TI_UINT8* CB_buf);

/**
 * \author Ronen Kalish\n
 * \date 14-November-2005\n
 * \brief called when a measurement FW guard timer expires.
 *
 * Function Scope \e Public.\n
 * \param hMeasuremntSRV - handle to the measurement SRV object.\n
 */
void MacServices_measurementSRV_startStopTimerExpired (TI_HANDLE hMeasurementSRV, TI_BOOL bTwdInitOccured);

/**
 * \author Ronen Kalish\n
 * \date 15-November-2005\n
 * \brief called when a measurement type timer expires.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasuremntSRV - handle to the measurement SRV object.\n
 */
void MacServices_measurementSRV_requestTimerExpired (TI_HANDLE hMeasurementSRV, TI_BOOL bTwdInitOccured);

#endif /* __MACSERVICESAPI_H__ */
