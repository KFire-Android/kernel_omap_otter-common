/*
 * PowerMgr.h
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

/** \file PowerMgr.h
 *  \brief This is the Power Manager module private (internal).
 *  \
 *  \date 24-Oct-2005
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Power Manager                                                 *
 *   PURPOSE: Power Manager Module private                                      *
 *                                                                          *
 ****************************************************************************/

#ifndef _POWER_MGR_H_
#define _POWER_MGR_H_

#include "tidef.h"
#include "paramOut.h"

/*****************************************************************************
 **         Constants                                                       **
 *****************************************************************************/
#define RE_ENTER_PS_TIMEOUT 10 /* mSec */

#define BET_INTERVAL_VALUE 1000 /* mSec */

/*****************************************************************************
 **         Enumerations                                                    **
 *****************************************************************************/

/*****************************************************************************
 **         Typedefs                                                        **
 *****************************************************************************/

/*****************************************************************************
 **         Structures                                                      **
 *****************************************************************************/

typedef struct
{
    PowerMgr_PowerMode_e powerMode;
    TI_BOOL priorityEnable;
} powerMngModePriority_t;



/** \struct powerMgr_t
 * this structure contain the data of the PowerMgr object.
 */
typedef struct
{
    TI_HANDLE                   hOS;                            /**< Handle to the OS object */
    TI_HANDLE                   hReport;                        /**< Handle to the Report module */
    TI_HANDLE                   hTrafficMonitor;                /**< Handle to the Traffic Monitor object */
    TI_HANDLE                   hSiteMgr;                       /**< Handle to the Site Mgr object */
    TI_HANDLE                   hTWD;                           /**< Handle to the TWD object */
    TI_HANDLE                   hSoftGemini;                    /**< Handle to the SG object */
    TI_HANDLE                   hTimer;                         /**< Handle to the Timer module object */
    TI_HANDLE                   hRetryPsTimer;                  /**< Handle to the retry timer */
    TI_HANDLE                   hPsPollFailureTimer;            /**< Handle to ps-poll failure timer */
    TI_HANDLE                   hPowerMgrKeepAlive;             /**< Handle to the keep-alive sub module */
    PowerMgr_PowerMode_e        desiredPowerModeProfile;        /**< 
                                                                 * The configure power mode to the system in the
                                                                 * initialization function. This parameters is Saved
                                                                 * for restart the module.
                                                                 */
    powerMngModePriority_t      powerMngModePriority[POWER_MANAGER_MAX_PRIORITY];                                                                           
    PowerMgr_PowerMode_e        lastPowerModeProfile;           /**< 
                                                                 * The last configured power mode. 
                                                                 */
    TI_BOOL                     psEnable;                       /**<
                                                                 * The parameter holds the enable of the power save
                                                                 * mechanism.
                                                                 */
    TI_UINT16                   autoModeInterval;               /**<
                                                                 * Time period (in ms) to test the current TP before
                                                                 * changing the current power mode.
                                                                 */
    TI_UINT16                   autoModeActiveTH;               /**< Threshold (in Bytes) for moving to Active mode */
    TI_UINT16                   autoModeDozeTH;                 /**< 
                                                                 * Threshold (in Bytes) for moving to Short-Doze from
                                                                 * active mode.
                                                                 */
    PowerMgr_PowerMode_e        autoModeDozeMode;               /**< 
                                                                 * The power mode of doze (short-doze / long-doze) that
                                                                 * auto mode will be toggle between doze vs active.
                                                                 */
    PowerMgr_Priority_e         powerMngPriority;               /**<
                                                                 * the priority of the power manager - canbe - regular user (cli) or
                                                                 * special user i.e Soft Gemini.
                                                                 */
    TI_HANDLE                   passToActiveTMEvent;            /**< 
                                                                 * Traffic Monitor event (TrafficAlertElement) for
                                                                 * the pass to active event from the traffic monitor.
                                                                 */
    TI_HANDLE                   passToDozeTMEvent;              /**<
                                                                 * Traffic Monitor event (TrafficAlertElement) for
                                                                 * the pass to short doze from active event from the
                                                                 * traffic monitor.
                                                                 */
    TI_HANDLE                   betEnableTMEvent;               /**<
                                                                 * Traffic Monitor event (TrafficAlertElement) for
                                                                 * enabling BET.
                                                                 */
    TI_HANDLE                   betDisableTMEvent;               /**<
                                                                 * Traffic Monitor event (TrafficAlertElement) for
                                                                 * disabling BET.
                                                                 */
    TI_UINT8                    beaconListenInterval;           /**< 
                                                                 * specify how often the TNET wakes up to listen to
                                                                 * beacon frames. the value is expressed in units of
                                                                 * "beacon interval".
                                                                 */
    TI_UINT8                    dtimListenInterval;             /**< specify how often the TNET wakes up to listen to
                                                                 * dtim frames. the value is expressed in units of
                                                                 * "dtim interval".
                                                                 */                                                                          
    EPowerPolicy    			defaultPowerLevel;              /**< Power level when PS not active */
    EPowerPolicy    			PowerSavePowerLevel;            /**< Power level when PS active */
    EventsPowerSave_e           lastPsTransaction;              /**< Last result of PS request */
    TI_UINT32                   maxFullBeaconInterval;          /**< Maximal time between full beacon reception */
    TI_UINT8	                betEnable;                      /**< last configuration of BET enable/disable */
	TI_UINT8					betTrafficEnable;
	TI_UINT8					BetEnableThreshold;
	TI_UINT8					BetDisableThreshold;
    TI_UINT32                   PsPollDeliveryFailureRecoveryPeriod; /* Time to exit PS after receiving PsPoll failure event */

	TI_BOOL						reAuthActivePriority;
} PowerMgr_t;


#endif /*  _POWER_MGR_H_  */

