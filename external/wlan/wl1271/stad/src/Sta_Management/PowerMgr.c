/*
 * PowerMgr.c
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

/** \file PowerMgr.c
 *  \brief This is the PowerMgr module implementation.
 *  \
 *  \date 24-Oct-2005
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  PowerMgr                                                      *
 *   PURPOSE: PowerMgr Module implementation.                               *
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_71
#include "tidef.h"
#include "osApi.h"
#include "timer.h"
#include "paramOut.h"
#include "report.h"
#include "PowerMgr.h"
#include "PowerMgr_API.h"
#include "TrafficMonitorAPI.h"
#include "qosMngr_API.h"
#include "siteMgrApi.h"
#include "TWDriver.h"
#include "SoftGeminiApi.h"
#include "DrvMainModules.h"
#include "PowerMgrKeepAlive.h"
#include "CmdBld.h"


/*****************************************************************************
 **         Defines                                                         **
 *****************************************************************************/
#define DEFAULT_LISTEN_INTERVAL (1)
#define BET_DISABLE 0
#define BET_ENABLE  1


/*****************************************************************************
 **         Private Function prototypes                                      **
 *****************************************************************************/

static void         powerSaveCompleteCB(TI_HANDLE hPowerMgr,TI_UINT8 PSMode,TI_UINT8 transStatus);
static void         PowerMgrTMThresholdCrossCB( TI_HANDLE hPowerMgr, TI_UINT32 cookie );
static void         powerMgrDisableThresholdsIndications(TI_HANDLE hPowerMgr);
static void         powerMgrEnableThresholdsIndications(TI_HANDLE hPowerMgr);
static void         powerMgrStartAutoPowerMode(TI_HANDLE hPowerMgr);
static void         powerMgrRetryPsTimeout(TI_HANDLE hPowerMgr, TI_BOOL bTwdInitOccured);
static void         powerMgrPowerProfileConfiguration(TI_HANDLE hPowerMgr, PowerMgr_PowerMode_e desiredPowerMode);
static void         PowerMgr_setDozeModeInAuto(TI_HANDLE hPowerMgr,PowerMgr_PowerMode_e dozeMode);
static void         PowerMgrConfigBetToFw( TI_HANDLE hPowerMgr, TI_UINT32 cookie );
static void         PowerMgr_PsPollFailureCB( TI_HANDLE hPowerMgr );
static void 		powerMgr_PsPollFailureTimeout( TI_HANDLE hPowerMgr, TI_BOOL bTwdInitOccured );
static void 		powerMgr_SGSetUserDesiredwakeUpCond( TI_HANDLE hPowerMgr );
static TI_STATUS    powerMgrSendMBXWakeUpConditions(TI_HANDLE hPowerMgr,TI_UINT8 listenInterval, ETnetWakeOn tnetWakeupOn);
static TI_STATUS    powerMgrNullPacketRateConfiguration(TI_HANDLE hPowerMgr);
static PowerMgr_PowerMode_e powerMgrGetHighestPriority(TI_HANDLE hPowerMgr);


/*****************************************************************************
 **         Public Function prototypes                                      **
 *****************************************************************************/


/****************************************************************************************
 *                        PowerMgr_create                                                           *
 ****************************************************************************************
DESCRIPTION: Creates the object of the power Manager. 
                performs the following:
                -   Allocate the Power Manager handle
                -   Creates the retry timer
                                                                                                                   
INPUT:          - hOs - Handle to OS        
OUTPUT:     
RETURN:     Handle to the Power Manager module on success, NULL otherwise
****************************************************************************************/
TI_HANDLE PowerMgr_create(TI_HANDLE hOs)
{

    PowerMgr_t * pPowerMgr = NULL;
    pPowerMgr = (PowerMgr_t*) os_memoryAlloc (hOs, sizeof(PowerMgr_t));
    if ( pPowerMgr == NULL )
    {
        WLAN_OS_REPORT(("PowerMgr_create - Memory Allocation Error!\n"));
        return NULL;
    }

    os_memoryZero (hOs, pPowerMgr, sizeof(PowerMgr_t));

    pPowerMgr->hOS = hOs;

    /* create the power manager keep-alive sub module */
    pPowerMgr->hPowerMgrKeepAlive = powerMgrKL_create (hOs);

    return pPowerMgr;

}


/****************************************************************************************
*                        powerSrv_destroy                                                          *
****************************************************************************************
DESCRIPTION: Destroy the object of the power Manager.
               -   delete Power Manager alocation
               -   call the destroy function of the timer
                                                                                                                  
INPUT:          - hPowerMgr - Handle to the Power Manager   
OUTPUT:     
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS PowerMgr_destroy(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    /* destroy the power manager keep-alive sub module */
    powerMgrKL_destroy (pPowerMgr->hPowerMgrKeepAlive);

    if (pPowerMgr->hRetryPsTimer)
    {
        tmr_DestroyTimer (pPowerMgr->hRetryPsTimer);
    }

    if ( pPowerMgr->hPsPollFailureTimer != NULL )
    {
        tmr_DestroyTimer(pPowerMgr->hPsPollFailureTimer);
    }
    os_memoryFree(pPowerMgr->hOS, pPowerMgr, sizeof(PowerMgr_t));

    return TI_OK;
}


/****************************************************************************************
*                        PowerMgr_init                                                         *
****************************************************************************************
DESCRIPTION: Power Manager init function, called in init phase.
                                                                                                                 
INPUT:     pStadHandles  - The driver modules handles

OUTPUT:     

RETURN:    void
****************************************************************************************/
void PowerMgr_init (TStadHandlesList *pStadHandles)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)(pStadHandles->hPowerMgr);

    pPowerMgr->hReport          = pStadHandles->hReport;
    pPowerMgr->hTrafficMonitor  = pStadHandles->hTrafficMon;
    pPowerMgr->hSiteMgr         = pStadHandles->hSiteMgr;
    pPowerMgr->hTWD             = pStadHandles->hTWD;
    pPowerMgr->hSoftGemini      = pStadHandles->hSoftGemini;
    pPowerMgr->hTimer           = pStadHandles->hTimer;
    pPowerMgr->psEnable         = TI_FALSE;

    /* initialize the power manager keep-alive sub module */
    powerMgrKL_init (pPowerMgr->hPowerMgrKeepAlive, pStadHandles);

}


TI_STATUS PowerMgr_SetDefaults (TI_HANDLE hPowerMgr, PowerMgrInitParams_t* pPowerMgrInitParams)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    TI_UINT8 index;
    /* used to initialize the Traffic Monitor for Auto Ps events */
    TrafficAlertRegParm_t tmRegParam;
    TI_STATUS status;

	pPowerMgr->reAuthActivePriority		= pPowerMgrInitParams->reAuthActivePriority;

    /* init power management options */
    pPowerMgr->beaconListenInterval = pPowerMgrInitParams->beaconListenInterval;
    pPowerMgr->dtimListenInterval = pPowerMgrInitParams->dtimListenInterval;
    pPowerMgr->defaultPowerLevel =  pPowerMgrInitParams->defaultPowerLevel;
    pPowerMgr->PowerSavePowerLevel =  pPowerMgrInitParams->PowerSavePowerLevel;
    pPowerMgr->powerMngPriority  = POWER_MANAGER_USER_PRIORITY;
    pPowerMgr->maxFullBeaconInterval = pPowerMgrInitParams->MaximalFullBeaconReceptionInterval;
    pPowerMgr->PsPollDeliveryFailureRecoveryPeriod = pPowerMgrInitParams->PsPollDeliveryFailureRecoveryPeriod;

    /*
     set AUTO PS parameters 
     */
    pPowerMgr->autoModeInterval = pPowerMgrInitParams->autoModeInterval;
    pPowerMgr->autoModeActiveTH = pPowerMgrInitParams->autoModeActiveTH;
    pPowerMgr->autoModeDozeTH = pPowerMgrInitParams->autoModeDozeTH;
    pPowerMgr->autoModeDozeMode = pPowerMgrInitParams->autoModeDozeMode;

    /*
     register threshold in the traffic monitor.
     */
  	pPowerMgr->betEnable = pPowerMgrInitParams->BetEnable; /* save BET enable flag for CLI configuration */
	pPowerMgr->betTrafficEnable = TI_FALSE;                   /* starting without BET */

    /* BET thresholds */
    /* general parameters */
    tmRegParam.Context = pPowerMgr;
    tmRegParam.TimeIntervalMs = BET_INTERVAL_VALUE;
    tmRegParam.Trigger = TRAFF_EDGE;
    tmRegParam.MonitorType = TX_RX_ALL_802_11_DATA_FRAMES;
    tmRegParam.CallBack = PowerMgrConfigBetToFw;

    /* BET enable event */
    tmRegParam.Direction = TRAFF_DOWN;
    tmRegParam.Threshold = pPowerMgrInitParams->BetEnableThreshold;
    pPowerMgr->BetEnableThreshold = pPowerMgrInitParams->BetEnableThreshold;
    tmRegParam.Cookie = (TI_UINT32)BET_ENABLE;
    pPowerMgr->betEnableTMEvent = TrafficMonitor_RegEvent (pPowerMgr->hTrafficMonitor,
                                                             &tmRegParam,
                                                             TI_FALSE);
    /* BET disable event */
    tmRegParam.Direction = TRAFF_UP;
    tmRegParam.Threshold = pPowerMgrInitParams->BetDisableThreshold;
    pPowerMgr->BetDisableThreshold = pPowerMgrInitParams->BetDisableThreshold;
    tmRegParam.Cookie = (TI_UINT32)BET_DISABLE;
    pPowerMgr->betDisableTMEvent = TrafficMonitor_RegEvent (pPowerMgr->hTrafficMonitor,
                                                             &tmRegParam,
                                                             TI_FALSE);

    if ( (pPowerMgr->betDisableTMEvent == NULL) ||
         (pPowerMgr->betEnableTMEvent == NULL))
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INIT, "PowerMgr_init - TM - ERROR registering BET events - ABROTING init!\n");
        return TI_NOK;
    }
    /*
    set the events as resets for one another
    */
    status = TrafficMonitor_SetRstCondition (pPowerMgr->hTrafficMonitor,
                                            pPowerMgr->betDisableTMEvent,
                                            pPowerMgr->betEnableTMEvent,
                                            TI_TRUE);
    if ( status != TI_OK )
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INIT, "PowerMgr_init - PowerMgr_init - ERROR binding BET events - ABROTING init!\n");
        return TI_NOK;
    }

    /* general parameters */
    tmRegParam.Context = pPowerMgr;

    tmRegParam.Cookie = (TI_UINT32)POWER_MODE_ACTIVE;
    tmRegParam.TimeIntervalMs = pPowerMgr->autoModeInterval;
    tmRegParam.Trigger = TRAFF_EDGE;
    tmRegParam.MonitorType = TX_RX_ALL_802_11_DATA_FRAMES;

    /* Active threshold */
    tmRegParam.CallBack = PowerMgrTMThresholdCrossCB;
    tmRegParam.Direction = TRAFF_UP;
    tmRegParam.Threshold = pPowerMgr->autoModeActiveTH;
    pPowerMgr->passToActiveTMEvent = TrafficMonitor_RegEvent (pPowerMgr->hTrafficMonitor,
                                                             &tmRegParam,
                                                             TI_FALSE);
    /* Doze threshold */
    tmRegParam.Direction = TRAFF_DOWN;
    tmRegParam.Threshold = pPowerMgr->autoModeDozeTH;
    tmRegParam.Cookie = (TI_UINT32)POWER_MODE_SHORT_DOZE; /* diffrentiation between long / short doze is done at the 
                                                          CB, according to configuration at time of CB invokation */
    pPowerMgr->passToDozeTMEvent = TrafficMonitor_RegEvent (pPowerMgr->hTrafficMonitor,
                                                           &tmRegParam,
                                                           TI_FALSE);

    if ( (pPowerMgr->passToActiveTMEvent == NULL) ||
         (pPowerMgr->passToDozeTMEvent == NULL))
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INIT, "PowerMgr_init - PowerMgr_init - ERROR registering Auto mode events - ABROTING init!\n");
        return TI_NOK;
    }

    /*
    set the events as resets for one another
    */
    status = TrafficMonitor_SetRstCondition (pPowerMgr->hTrafficMonitor,
                                            pPowerMgr->passToActiveTMEvent,
                                            pPowerMgr->passToDozeTMEvent,
                                            TI_TRUE);
    if ( status != TI_OK )
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INIT, "PowerMgr_init - PowerMgr_init - ERROR binding Auto mode events - ABROTING init!\n");
        return TI_NOK;
    }

    /*
    configure the initialize power mode
    */
    pPowerMgr->desiredPowerModeProfile = pPowerMgrInitParams->powerMode;
    for ( index = 0;index < POWER_MANAGER_MAX_PRIORITY;index++ )
    {
        pPowerMgr->powerMngModePriority[index].powerMode = pPowerMgr->desiredPowerModeProfile;
        pPowerMgr->powerMngModePriority[index].priorityEnable = TI_FALSE;
    }
    pPowerMgr->powerMngModePriority[POWER_MANAGER_USER_PRIORITY].priorityEnable = TI_TRUE;

    if (pPowerMgr->reAuthActivePriority)
		pPowerMgr->powerMngModePriority[POWER_MANAGER_REAUTH_PRIORITY].powerMode = POWER_MODE_ACTIVE;

    /* set the defualt power policy */
    TWD_CfgSleepAuth (pPowerMgr->hTWD, pPowerMgr->defaultPowerLevel);


    /*create the timers */
    pPowerMgr->hRetryPsTimer = tmr_CreateTimer(pPowerMgr->hTimer);

    pPowerMgr->hPsPollFailureTimer = tmr_CreateTimer(pPowerMgr->hTimer);

    if ( (pPowerMgr->hPsPollFailureTimer == NULL) || (pPowerMgr->hRetryPsTimer == NULL))
    {
TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INIT, "PowerMgr_SetDefaults - ERROR creating timers - ABROTING init!\n");
        return TI_NOK;
    }

    /* Register and Enable the PsPoll failure */
    TWD_RegisterEvent (pPowerMgr->hTWD,
        TWD_OWN_EVENT_PSPOLL_DELIVERY_FAILURE,
        (void *)PowerMgr_PsPollFailureCB, 
        hPowerMgr);
    TWD_EnableEvent (pPowerMgr->hTWD, TWD_OWN_EVENT_PSPOLL_DELIVERY_FAILURE);

    TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INIT, "PowerMgr_init - PowerMgr Initialized\n");

    /* set defaults for the power manager keep-alive sub module */
    powerMgrKL_setDefaults (pPowerMgr->hPowerMgrKeepAlive);

    return TI_OK;
}

/****************************************************************************************
 *                        PowerMgr_startPS                                                          *
 ****************************************************************************************
DESCRIPTION: Start the power save algorithm of the driver and also the 802.11 PS.
                                                                                                                   
INPUT:          - hPowerMgr             - Handle to the Power Manager

OUTPUT:     
RETURN:    TI_STATUS - TI_OK or PENDING on success else TI_NOK.\n
****************************************************************************************/
TI_STATUS PowerMgr_startPS(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
	int frameCount;

    
    TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgr_startPS - called\n");

    if ( pPowerMgr->psEnable == TI_TRUE )
    {
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "PowerMgr_startPS - PS mechanism is already Enable! Aborting psEnable=%d !\n", pPowerMgr->psEnable);
        /*
        this is a FATAL ERROR of the power manager!
        already enable power-save! thus return TI_OK, but there is an error in the upper
        layer that call tp PowerMgr_startPS() twice - should know that power-save
        is already enable therefor print the Error message.
        or
        the state machine while NOT in PS can be only in ACTIVE state and in some cases in
        PS_PENDING state. therefore the state machine is out of sync from it logic!
        */
        return TI_OK;
    }

    pPowerMgr->psEnable = TI_TRUE;
    /*set the correct rate after connection*/
    powerMgrNullPacketRateConfiguration(hPowerMgr);
    /*
    if in auto mode then need to refer to the threshold cross indication from the traffic monitor,
    else it need to refer to the configured power mode profile from the user.
    */
    pPowerMgr->desiredPowerModeProfile = powerMgrGetHighestPriority(hPowerMgr);

    if ( pPowerMgr->desiredPowerModeProfile == POWER_MODE_AUTO )
    {
        powerMgrStartAutoPowerMode(hPowerMgr);
    }
    else /*not auto mode - according to the current profle*/
    {
        powerMgrPowerProfileConfiguration(hPowerMgr, pPowerMgr->desiredPowerModeProfile);
    }

    TWD_CfgSleepAuth (pPowerMgr->hTWD, pPowerMgr->PowerSavePowerLevel);

   if ((pPowerMgr->betEnable)&&( pPowerMgr->desiredPowerModeProfile != POWER_MODE_ACTIVE ))
   {
		TrafficMonitor_StartEventNotif(pPowerMgr->hTrafficMonitor,
									   pPowerMgr->betEnableTMEvent);

		TrafficMonitor_StartEventNotif(pPowerMgr->hTrafficMonitor,
									   pPowerMgr->betDisableTMEvent);

	
		frameCount = TrafficMonitor_GetFrameBandwidth(pPowerMgr->hTrafficMonitor);
	
		if (frameCount < pPowerMgr->BetEnableThreshold) 
		{
            pPowerMgr->betTrafficEnable = TI_TRUE;
            
		}
		else if (frameCount > pPowerMgr->BetDisableThreshold) 
		{
			pPowerMgr->betTrafficEnable = TI_FALSE;
        }
       
		PowerMgrConfigBetToFw(hPowerMgr,pPowerMgr->betTrafficEnable);
	}
    
    /* also start the power manager keep-alive sub module */
    powerMgrKL_start (pPowerMgr->hPowerMgrKeepAlive);

    return TI_OK;
}


/****************************************************************************************
 *                        PowerMgr_stopPS                                                           *
 ****************************************************************************************
DESCRIPTION: Stop the power save algorithm of the driver and also the 802.11 PS.
                                                                                                                               
INPUT:          - hPowerMgr             - Handle to the Power Manager

OUTPUT:     
RETURN:    TI_STATUS - TI_OK or PENDING on success else TI_NOK.\n
****************************************************************************************/
TI_STATUS PowerMgr_stopPS(TI_HANDLE hPowerMgr, TI_BOOL bDisconnect)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    /*TI_STATUS status;*/

    TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgr_stopPS - called\n");

    if ( pPowerMgr->psEnable == TI_FALSE )
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgr_stopPS - PS is already Disable! Aborting!\n");
        /*
        Print Info message incase callng PowerMgr_stopPS() more than once in a row, without
        calling to PowerMgr_startPS() in the middle.
        this will return with TI_OK and not doing the Stop action!
        */
        return TI_OK;
    }

    pPowerMgr->psEnable = TI_FALSE;
    tmr_StopTimer (pPowerMgr->hRetryPsTimer);

    /* Check if PsPoll work-around is currently enabled */
    if ( pPowerMgr->powerMngModePriority[POWER_MANAGER_PS_POLL_FAILURE_PRIORITY].priorityEnable == TI_TRUE)
    {
        tmr_StopTimer(pPowerMgr->hPsPollFailureTimer);
        /* Exit the PsPoll work-around */
        powerMgr_PsPollFailureTimeout( hPowerMgr, TI_FALSE );
    }

    if ( pPowerMgr->desiredPowerModeProfile == POWER_MODE_AUTO )
    {
        powerMgrDisableThresholdsIndications(hPowerMgr);
    }

    TWD_SetPsMode (pPowerMgr->hTWD, POWER_SAVE_OFF, TI_FALSE, NULL, NULL, NULL);

    /* set the power policy of the system */
    TWD_CfgSleepAuth (pPowerMgr->hTWD, pPowerMgr->defaultPowerLevel);
    if ((pPowerMgr->betEnable)&&( pPowerMgr->desiredPowerModeProfile != POWER_MODE_ACTIVE ))
	{
		TrafficMonitor_StopEventNotif(pPowerMgr->hTrafficMonitor,
									  pPowerMgr->betEnableTMEvent);

		TrafficMonitor_StopEventNotif(pPowerMgr->hTrafficMonitor,
									  pPowerMgr->betDisableTMEvent);
	}

   /* also stop the power manager keep-alive sub module */
    powerMgrKL_stop (pPowerMgr->hPowerMgrKeepAlive, bDisconnect);

    return TI_OK;
}


/****************************************************************************************
 *                        PowerMgr_getPsStatus                                                          *
 ****************************************************************************************
DESCRIPTION: returns the 802.11 power save status (enable / disable).
                                                                                                                               
INPUT:          - hPowerMgr             - Handle to the Power Manager

OUTPUT:     
RETURN:    TI_BOOL - TI_TRUE if enable else TI_FALSE.\n
****************************************************************************************/
TI_BOOL PowerMgr_getPsStatus(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    return  TWD_GetPsStatus (pPowerMgr->hTWD);
}


/****************************************************************************************
 *                        PowerMgr_setPowerMode                                                         *
 ****************************************************************************************
DESCRIPTION: Configure of the PowerMode profile (auto / active / short doze / long doze).
                                                                                                                               
INPUT:          - hPowerMgr             - Handle to the Power Manager
            - thePowerMode      - the requested power mode (auto / active / short doze / long doze).
OUTPUT:     
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.\n
****************************************************************************************/
TI_STATUS PowerMgr_setPowerMode(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    PowerMgr_PowerMode_e powerMode;

    /*in this way we will run with the highest priority that is enabled*/
    powerMode = powerMgrGetHighestPriority(hPowerMgr);

    /* sanity checking */
    if ( powerMode >= POWER_MODE_MAX)
    {
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "PowerMgr_setPowerMode - unknown parameter: %d\n", powerMode);
        return TI_NOK;
    }

    TRACE1( pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgr_setPowerMode, power mode = %d\n", powerMode);

    if ( pPowerMgr->desiredPowerModeProfile != powerMode )
    {
        PowerMgr_PowerMode_e previousPowerModeProfile;
        previousPowerModeProfile = pPowerMgr->desiredPowerModeProfile;
        pPowerMgr->desiredPowerModeProfile = powerMode;

        if ( pPowerMgr->desiredPowerModeProfile == POWER_MODE_AUTO )
        {
            if ( pPowerMgr->psEnable == TI_TRUE )
            {
                powerMgrStartAutoPowerMode(hPowerMgr);
            }

            /*
            the transitions of state will be done according to the events from the
            traffic monitor - therefor abort and wait event from the traffic monitor.
            */
            return TI_OK;
        }
        else if ( previousPowerModeProfile == POWER_MODE_AUTO )
        {
            /*
            if the old power mode is AUTO and the new power mode is NOT then need
            to disable the thresholds indications from the traffic monitor.
            */
            powerMgrDisableThresholdsIndications(hPowerMgr);
        }
        if ( pPowerMgr->psEnable == TI_TRUE )
        {
            powerMgrPowerProfileConfiguration(hPowerMgr, powerMode);
        }
    }
    else
    {
        /*
        the power mode is already configure to the module - don't need to do anything!
        */
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_WARNING, "PowerMgr_setPowerMode - desiredPowerModeProfile == thePowerMode (=%d), ABORTING!\n", powerMode);
    }

    return TI_OK;
}


/****************************************************************************************
 *                        PowerMgr_setDozeModeInAuto                                    *
 ****************************************************************************************
DESCRIPTION: Configure the doze mode (short-doze / long-doze) that auto mode will toggle between doze vs active.                                                                                        
INPUT:      - hPowerMgr             - Handle to the Power Manager
            - dozeMode      - the requested doze mode when Mgr is in Auto mode (short-doze / long-doze)
OUTPUT:     
RETURN:   
****************************************************************************************/
void PowerMgr_setDozeModeInAuto(TI_HANDLE hPowerMgr, PowerMgr_PowerMode_e dozeMode)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    PowerMgr_PowerMode_e powerMode = powerMgrGetHighestPriority(hPowerMgr);

    /* check if we are trying to configure the same Doze mode */
    if ( dozeMode != pPowerMgr->autoModeDozeMode )
    {
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgr_setDozeModeInAuto - autoModeDozeMode == %d \n", dozeMode);

        pPowerMgr->autoModeDozeMode = dozeMode;

        /* in case we are already in Auto mode, we have to set the wake up condition MIB */
        if ( powerMode == POWER_MODE_AUTO )
        {
            if ( dozeMode == POWER_MODE_SHORT_DOZE )
            {
                if ( pPowerMgr->beaconListenInterval > 1 )
                {
                    powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_N_BEACON);       
                }
                else
                {
                    powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_BEACON);     
                }
            }
            else  /* POWER_MODE_LONG_DOZE */
            {
                if ( pPowerMgr->dtimListenInterval > 1 )
                {
                    powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_N_DTIM);       
                }
                else
                {
                    powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_DTIM);     
                }
            }

            TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgr_setDozeModeInAuto - already in Auto\n");
        }
    }
    else
    {
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_WARNING, "PowerMgr_setDozeModeInAuto - autoModeDozeMode == %d (same same ...)\n", dozeMode);
    }
}

/****************************************************************************************
 *                        PowerMgr_getPowerMode                                                         *
 ****************************************************************************************
DESCRIPTION: Get the current PowerMode of the PowerMgr module. 
                                                                                                                               
INPUT:          - hPowerMgr             - Handle to the Power Manager
OUTPUT:     
RETURN:    PowerMgr_PowerMode_e - (auto / active / short doze / long doze).\n
****************************************************************************************/
PowerMgr_PowerMode_e PowerMgr_getPowerMode(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    return pPowerMgr->desiredPowerModeProfile;
}


TI_STATUS powerMgr_setParam(TI_HANDLE thePowerMgrHandle,
                            paramInfo_t *theParamP)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)thePowerMgrHandle;

    switch ( theParamP->paramType )
    {
    case POWER_MGR_POWER_MODE:
        pPowerMgr->powerMngModePriority[theParamP->content.powerMngPowerMode.PowerMngPriority].powerMode
                        = theParamP->content.powerMngPowerMode.PowerMode;
        PowerMgr_setPowerMode(thePowerMgrHandle);
        if (pPowerMgr->betEnable)
        PowerMgrConfigBetToFw(thePowerMgrHandle, pPowerMgr->betEnable );
        break;

    case POWER_MGR_DISABLE_PRIORITY:
        pPowerMgr->powerMngModePriority[theParamP->content.powerMngPriority].priorityEnable = TI_FALSE;
        PowerMgr_setPowerMode(thePowerMgrHandle);
        break;

    case POWER_MGR_ENABLE_PRIORITY:
        pPowerMgr->powerMngModePriority[theParamP->content.powerMngPriority].priorityEnable = TI_TRUE;
        PowerMgr_setPowerMode(thePowerMgrHandle);
        break;

    case POWER_MGR_POWER_LEVEL_PS:
        pPowerMgr->PowerSavePowerLevel = theParamP->content.PowerSavePowerLevel;
        /* If we are connected, config the new power level (this param is for connected state) */
		if (pPowerMgr->psEnable)
        {
			TWD_CfgSleepAuth (pPowerMgr->hTWD, pPowerMgr->PowerSavePowerLevel);
		}
        break;

    case POWER_MGR_POWER_LEVEL_DEFAULT:
        pPowerMgr->defaultPowerLevel = theParamP->content.DefaultPowerLevel;
        /* If we are NOT connected, config the new power level (this param is for disconnected state) */
		if (!pPowerMgr->psEnable)
		{	
			TWD_CfgSleepAuth (pPowerMgr->hTWD, pPowerMgr->defaultPowerLevel);
		}
        break;

    case POWER_MGR_POWER_LEVEL_DOZE_MODE:
        PowerMgr_setDozeModeInAuto(thePowerMgrHandle,theParamP->content.powerMngDozeMode);
        if (pPowerMgr->betEnable)
        PowerMgrConfigBetToFw(thePowerMgrHandle, pPowerMgr->betEnable );
        break;

    case POWER_MGR_KEEP_ALIVE_ENA_DIS:
    case POWER_MGR_KEEP_ALIVE_ADD_REM:
        return powerMgrKL_setParam (pPowerMgr->hPowerMgrKeepAlive, theParamP);

    default:
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "PowerMgr_setParam - ERROR - Param is not supported, %d\n\n", theParamP->paramType);

        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}



TI_STATUS powerMgr_getParam(TI_HANDLE thePowerMgrHandle,
                            paramInfo_t *theParamP)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)thePowerMgrHandle;

    switch ( theParamP->paramType )
    {
    case POWER_MGR_POWER_MODE:
        theParamP->content.PowerMode = PowerMgr_getPowerMode(thePowerMgrHandle);
        break;

    case POWER_MGR_POWER_LEVEL_PS:
        theParamP->content.PowerSavePowerLevel = pPowerMgr->PowerSavePowerLevel;
        break;

    case POWER_MGR_POWER_LEVEL_DEFAULT:
        theParamP->content.DefaultPowerLevel = pPowerMgr->defaultPowerLevel;
        break;

    case POWER_MGR_POWER_LEVEL_DOZE_MODE:
        theParamP->content.powerMngDozeMode = pPowerMgr->autoModeDozeMode;
        break;

    case POWER_MGR_KEEP_ALIVE_GET_CONFIG:
        return powerMgrKL_getParam (pPowerMgr->hPowerMgrKeepAlive, theParamP);

    case POWER_MGR_GET_POWER_CONSUMPTION_STATISTICS:
       
       return cmdBld_ItrPowerConsumptionstat (pPowerMgr->hTWD,
                             theParamP->content.interogateCmdCBParams.fCb, 
                             theParamP->content.interogateCmdCBParams.hCb,
                             (void*)theParamP->content.interogateCmdCBParams.pCb);





    default:
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "PowerMgr_getParam - ERROR - Param is not supported, %d\n\n", theParamP->paramType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}


/*****************************************************************************
 **         Private Function prototypes                                     **
 *****************************************************************************/


/****************************************************************************************
 *                        powerSaveCompleteCB                                                       *
 ****************************************************************************************
DESCRIPTION: Callback for the Power server complete - gets the result of the request 
              for PS or exit PS.
                                                                                                                               
INPUT:          - hPowerMgr             - Handle to the Power Manager
            - PSMode
            - trasStatus            - result string form the FW.
OUTPUT:     
RETURN:    void.\n
****************************************************************************************/
static void powerSaveCompleteCB(TI_HANDLE hPowerMgr,TI_UINT8 PSMode,TI_UINT8 transStatus)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    TRACE1( pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "powerSaveCompleteCB, statud = %d\n", transStatus);

    /* Handling the event*/
    switch ( (EventsPowerSave_e)transStatus )
    {
    case ENTER_POWER_SAVE_FAIL:
    case EXIT_POWER_SAVE_FAIL:
        pPowerMgr->lastPsTransaction = transStatus;
        tmr_StartTimer (pPowerMgr->hRetryPsTimer,
                        powerMgrRetryPsTimeout,
                        (TI_HANDLE)pPowerMgr,
                      RE_ENTER_PS_TIMEOUT,
                      TI_FALSE);
        break;

    case ENTER_POWER_SAVE_SUCCESS:
    case EXIT_POWER_SAVE_SUCCESS:
        break;

    default:
        TRACE1( pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "powerSaveCompleteCB: invliad status: %d\n", transStatus);
        break;
    }
}

/**
 * \\n
 * \date 30-Aug-2006\n
 * \brief Power manager callback fro TM event notification
 *
 * Function Scope \e Public.\n
 * \param hPowerMgr - handle to the power maanger object.\n
 * \param cookie - values supplied during event registration (active / doze).\n
 */
static void PowerMgrTMThresholdCrossCB( TI_HANDLE hPowerMgr, TI_UINT32 cookie )
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgrTMThresholdCrossCB - TM notified threshold crossed, cookie: %d\n", cookie);

    /* sanity cehcking - TM notifications should only be received when PM is enabled and in auto mode */
    if ( (pPowerMgr->psEnable == TI_TRUE) && (pPowerMgr->desiredPowerModeProfile == POWER_MODE_AUTO))
    {
        switch ((PowerMgr_PowerMode_e)cookie)
        {
        case POWER_MODE_ACTIVE:
            powerMgrPowerProfileConfiguration( hPowerMgr, POWER_MODE_ACTIVE );
            break;

        /* threshold crossed down - need to enter configured doze mode */
        case POWER_MODE_SHORT_DOZE:
            powerMgrPowerProfileConfiguration( hPowerMgr, pPowerMgr->autoModeDozeMode );
            break;

        default:
            TRACE1( pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "PowerMgrTMThresholdCrossCB: TM notification with invalid cookie: %d!\n", cookie);
            break;
        }
    }
    else
    {
        TRACE2( pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "PowerMgrTMThresholdCrossCB: TM motification when psEnable is :%d or desired profile is: %d\n", pPowerMgr->psEnable, pPowerMgr->desiredPowerModeProfile);
    }

}

/****************************************************************************************
*                        powerMgrDisableThresholdsIndications                                           *
*****************************************************************************************
DESCRIPTION: This will send a disable message to the traffic monitor,
                 to stop sending indications on threshold pass.

                                                                                                                              
INPUT:          - hPowerMgr             - Handle to the Power Manager
OUTPUT:     
RETURN:    void.\n
****************************************************************************************/
static void powerMgrDisableThresholdsIndications(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    /*
    auto is not a static/fix state, else its a dynamic state that flows between
    the 3 static/fix states: active, short-doze and long-doze.
    */
    TrafficMonitor_StopEventNotif(pPowerMgr->hTrafficMonitor,
                                  pPowerMgr->passToActiveTMEvent);

    TrafficMonitor_StopEventNotif(pPowerMgr->hTrafficMonitor,
                                  pPowerMgr->passToDozeTMEvent);

}


/****************************************************************************************
*                        powerMgrEnableThresholdsIndications                                            *
*****************************************************************************************
DESCRIPTION: TThis will send an enable message to the traffic monitor,
                to start sending indications on threshold pass.

                                                                                                                              
INPUT:          - hPowerMgr             - Handle to the Power Manager
OUTPUT:     
RETURN:    void.\n
****************************************************************************************/
static void powerMgrEnableThresholdsIndications(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    TRACE0( pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "powerMgrEnableThresholdsIndications called\n");
    /*
    auto is not a static/fix state, but rather a dynamic state that flows between
    the 3 static/fix states: active, short-doze and long-doze.
    */
    TrafficMonitor_StartEventNotif(pPowerMgr->hTrafficMonitor,
                                   pPowerMgr->passToActiveTMEvent);

    TrafficMonitor_StartEventNotif(pPowerMgr->hTrafficMonitor,
                                   pPowerMgr->passToDozeTMEvent);

}


/****************************************************************************************
*                        powerMgrStartAutoPowerMode                                                 *
*****************************************************************************************
DESCRIPTION: configure the power manager to enter into AUTO power mode. 
             The power manager will deside what power level will be applied 
             acording to the traffic monitor.
                                                                                                                              
INPUT:          - hPowerMgr             - Handle to the Power Manager
OUTPUT:     
RETURN:    void.\n
****************************************************************************************/
static void powerMgrStartAutoPowerMode(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    int frameCount;

    frameCount = TrafficMonitor_GetFrameBandwidth(pPowerMgr->hTrafficMonitor);

    TRACE0( pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "powerMgrStartAutoPowerMode: Starting auto power mode,");

    /*Activates the correct profile*/
    if ( frameCount >= pPowerMgr->autoModeActiveTH )
    {
        powerMgrPowerProfileConfiguration(hPowerMgr, POWER_MODE_ACTIVE);
    }
    else
    {
        powerMgrPowerProfileConfiguration(hPowerMgr, pPowerMgr->autoModeDozeMode);

    }
    /* Activates the Trafic monitoe Events*/        
    powerMgrEnableThresholdsIndications(hPowerMgr);
}

/****************************************************************************************
*                        powerMgrRetryPsTimeout                                                     *
*****************************************************************************************
DESCRIPTION: Retry function if a PS/exit PS request failed
                                                                                                                              
INPUT:      hPowerMgr       - Handle to the Power Manager
            bTwdInitOccured - Indicates if TWDriver recovery occured since timer started 

OUTPUT:     

RETURN:    void.\n
****************************************************************************************/
static void powerMgrRetryPsTimeout(TI_HANDLE hPowerMgr, TI_BOOL bTwdInitOccured)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    TRACE0( pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "powerMgrRetryPsTimeout: timer expired.\n");

    if ( pPowerMgr->lastPsTransaction == ENTER_POWER_SAVE_FAIL )
    {
        TWD_SetPsMode (pPowerMgr->hTWD, POWER_SAVE_ON, TI_TRUE, hPowerMgr,powerSaveCompleteCB, NULL);/*NULL as GWSI callback*/
    }
    else
    {
        TWD_SetPsMode (pPowerMgr->hTWD, POWER_SAVE_OFF, TI_TRUE, hPowerMgr, powerSaveCompleteCB, NULL);/*NULL as GWSI callback*/
    }
	return;
}


/****************************************************************************************
*                        powerMgrPowerProfileConfiguration                                          *
*****************************************************************************************
DESCRIPTION: This function is the " builder " of the Power Save profiles. 
             acording to the desired Power mode.
                                                                                                                              
INPUT:          - hPowerMgr             - Handle to the Power Manager
OUTPUT:     
RETURN:    void.\n
****************************************************************************************/
static void powerMgrPowerProfileConfiguration(TI_HANDLE hPowerMgr, PowerMgr_PowerMode_e desiredPowerMode)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    tmr_StopTimer (pPowerMgr->hRetryPsTimer);

	pPowerMgr->lastPowerModeProfile = desiredPowerMode;

    switch ( desiredPowerMode )
    {
    case POWER_MODE_AUTO:
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMode==AUTO - This mode should not be sent to the GWSI - we send AUTO instead\n");
        break;

    case POWER_MODE_ACTIVE:
        /* set AWAKE through */
        TWD_SetPsMode (pPowerMgr->hTWD,
                                          POWER_SAVE_OFF, 
                                          TI_TRUE, 
                                          hPowerMgr,
                                          powerSaveCompleteCB,
                                          NULL);

        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMode==ACTIVE\n");
        break;

    case POWER_MODE_SHORT_DOZE:
        if ( pPowerMgr->beaconListenInterval > 1 )
        {
            powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_N_BEACON);       
        }
        else
        {
            powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_BEACON);     
        }

        TWD_SetPsMode (pPowerMgr->hTWD, 
                                          POWER_SAVE_ON, 
                                          TI_TRUE, 
                                          hPowerMgr,
                                          powerSaveCompleteCB,
                                          NULL);

        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMode==SHORT_DOZE\n");
        break;

    case POWER_MODE_LONG_DOZE:
        if ( pPowerMgr->dtimListenInterval > 1 )
        {
            powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_N_DTIM);       
        }
        else
        {
            powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_DTIM);     
        }
        TWD_SetPsMode (pPowerMgr->hTWD, 
                                          POWER_SAVE_ON, 
                                          TI_TRUE, 
                                          hPowerMgr,
                                          powerSaveCompleteCB,
                                          NULL);

        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMode==LONG_DOZE\n");
        break;

	case POWER_MODE_PS_ONLY:
		/* When in SG PS mode, configure the user desired wake-up condition */
		powerMgr_SGSetUserDesiredwakeUpCond(pPowerMgr);

        TWD_SetPsMode (pPowerMgr->hTWD, 
                                          POWER_SAVE_ON, 
                                          TI_TRUE, 
                                          hPowerMgr,
                                          powerSaveCompleteCB,
                                          NULL);

        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMode==PS_ONLY\n");
        break;

    default:
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "PowerMgr_setWakeUpConfiguration - ERROR - PowerMode - unknown parameter: %d\n", desiredPowerMode);
        return;
    }

}


/****************************************************************************************
*                        powerMgrSendMBXWakeUpConditions                                            *
*****************************************************************************************
DESCRIPTION: Tsend configuration of the power management option that holds in the command
                mailbox inner sturcture.
                                                                                                                              
INPUT:          - hPowerMgr             - Handle to the Power Manager
OUTPUT:     
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.\n
****************************************************************************************/
static TI_STATUS powerMgrSendMBXWakeUpConditions(TI_HANDLE hPowerMgr,
                                                 TI_UINT8 listenInterval,
                                                 ETnetWakeOn tnetWakeupOn)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    TPowerMgmtConfig powerMgmtConfig;
    TI_STATUS status = TI_OK;

    powerMgmtConfig.listenInterval = listenInterval;
    powerMgmtConfig.tnetWakeupOn = tnetWakeupOn;

    TRACE2(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "powerMgrSendMBXWakeUpConditions: listenInterval = %d, tnetWakeupOn = %d\n", listenInterval,tnetWakeupOn);

    status = TWD_CfgWakeUpCondition (pPowerMgr->hTWD, &powerMgmtConfig);
                                      
    if ( status != TI_OK )
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "powerMgrSendMBXWakeUpConditions - Error in wae up condition IE!\n");
    }
    return status;
}




static TI_STATUS powerMgrNullPacketRateConfiguration(TI_HANDLE hPowerMgr)
{
    paramInfo_t     param;
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    param.paramType = SITE_MGR_CURRENT_RATE_PAIR_PARAM;
    if ( siteMgr_getParam(pPowerMgr->hSiteMgr, &param) == TI_OK )
    {
        TWD_SetNullRateModulation (pPowerMgr->hTWD, (TI_UINT16)param.content.siteMgrCurrentRateMask.basicRateMask);
    }
    else
    {
        TWD_SetNullRateModulation (pPowerMgr->hTWD, (DRV_RATE_MASK_1_BARKER | DRV_RATE_MASK_2_BARKER));
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "powerMgrNullPacketRateConfiguration: error - faild to set rate so default was seted!\n");
    }
    return TI_OK;

}


static PowerMgr_PowerMode_e powerMgrGetHighestPriority(TI_HANDLE hPowerMgr)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    int index;
    for ( index = POWER_MANAGER_MAX_PRIORITY-1;index >= 0;index-- )
    {
        if ( pPowerMgr->powerMngModePriority[index].priorityEnable )
        {

            return pPowerMgr->powerMngModePriority[index].powerMode;
        }

    }

    TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, "powerMgrGetHighestPriority - error - faild to get highest priority! sefault deseired mode was returned !!!\n");
    return pPowerMgr->desiredPowerModeProfile;
}


 /****************************************************************************************
 *                        PowerMgr_notifyFWReset															*
 ****************************************************************************************
DESCRIPTION: Notify the object of the power Manager about FW reset (recovery).
			 Calls PowerSrv module to Set Ps Mode
				                                                                                                   
INPUT:      - hPowerMgr - Handle to the Power Manager	
OUTPUT:		
RETURN:    TI_STATUS - TI_OK on success else TI_NOK.
****************************************************************************************/
TI_STATUS PowerMgr_notifyFWReset(TI_HANDLE hPowerMgr)
{
	PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    TRACE2(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgr_notifyFWReset(): psEnable = %d, lastPowerModeProfile = %d\n", pPowerMgr->psEnable, pPowerMgr->lastPowerModeProfile);

	if (pPowerMgr->psEnable)
	{
		powerMgrPowerProfileConfiguration(hPowerMgr, pPowerMgr->lastPowerModeProfile);
	}
	
    return TI_OK;
}


/****************************************************************************************
 *                        PowerMgrConfigBetToFw															*
 ****************************************************************************************
DESCRIPTION: callback from TM event notification.
				-	call PowerSrv module to Set Ps Mode
				                                                                                                   
INPUT:      	- hPowerMgr - Handle to the Power Manager	
                - BetEnable - cookie:values supplied during event registration
OUTPUT:		
RETURN:    None.
****************************************************************************************/
static void PowerMgrConfigBetToFw( TI_HANDLE hPowerMgr, TI_UINT32 BetEnable )
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    TI_UINT8 MaximumConsecutiveET;
    TI_UINT32 listenInterval;
    paramInfo_t param;
    TI_UINT32 beaconInterval;
    TI_UINT32 dtimPeriod;
    PowerMgr_PowerMode_e powerMode;

    param.paramType = SITE_MGR_BEACON_INTERVAL_PARAM;
    siteMgr_getParam(pPowerMgr->hSiteMgr, &param);
    beaconInterval = param.content.beaconInterval;

    param.paramType = SITE_MGR_DTIM_PERIOD_PARAM;
    siteMgr_getParam(pPowerMgr->hSiteMgr, &param);
    dtimPeriod = param.content.siteMgrDtimPeriod;

    /* get actual Power Mode */
    if (pPowerMgr->desiredPowerModeProfile == POWER_MODE_AUTO)
    {
        powerMode = pPowerMgr->autoModeDozeMode;
    }
    else
    {
        powerMode = pPowerMgr->lastPowerModeProfile;
    }

    /* calc ListenInterval */
    if (powerMode == POWER_MODE_SHORT_DOZE)
    {
        listenInterval = beaconInterval * pPowerMgr->beaconListenInterval;
    }
    else if (powerMode == POWER_MODE_LONG_DOZE)
    {
        listenInterval = dtimPeriod * beaconInterval * pPowerMgr->dtimListenInterval;
    }
    else
    {
        listenInterval = beaconInterval;
    }

    if (listenInterval == 0)
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_WARNING, "PowerMgrConfigBetToFw: listenInterval is ZERO\n");
        return;
    }

    /* MaximumConsecutiveET = MaximalFullBeaconReceptionInterval / MAX( BeaconInterval, ListenInterval) */
    MaximumConsecutiveET = pPowerMgr->maxFullBeaconInterval / listenInterval;

    TRACE5(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, "PowerMgrConfigBetToFw:\n                           Power Mode = %d\n                           beaconInterval = %d\n                           listenInterval = %d\n                           Bet Enable = %d\n                           MaximumConsecutiveET = %d\n", powerMode, beaconInterval, listenInterval, BetEnable, MaximumConsecutiveET);

    pPowerMgr->betEnable = BetEnable; /* save BET enable flag for CLI configuration */

    TWD_CfgBet(pPowerMgr->hTWD, BetEnable, MaximumConsecutiveET);
}

/**
 * \date 10-April-2007\n
 * \brief Returns to the configured wakeup condition, when SG protective mode is done
 *
 * Function Scope \e Public.\n
 * Parameters:\n
 * 1) TI_HANDLE - handle to the PowerMgr object.\n
 * Return Value: void.\n
 */
static void powerMgr_SGSetUserDesiredwakeUpCond( TI_HANDLE hPowerMgr )
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

   	if (pPowerMgr->psEnable)
	{
		/* set wakeup condition according to user mode power save profile */
		switch ( pPowerMgr->powerMngModePriority[ POWER_MANAGER_USER_PRIORITY ].powerMode )
		{
		case POWER_MODE_AUTO:
			/*set wakeup condition according to doze mode in auto and wakup interval */
			if ( pPowerMgr->autoModeDozeMode == POWER_MODE_SHORT_DOZE )
			{
				/* short doze */
				if ( pPowerMgr->beaconListenInterval > 1 )
				{
					powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_N_BEACON);       
				}
				else
				{
					powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_BEACON);     
				}
			}
			else
			{
				/* long doze */
				if ( pPowerMgr->dtimListenInterval > 1 )
				{
					powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_N_DTIM);       
				}
				else
				{
					powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_DTIM);     
				}
			}
			break;
	
		case POWER_MODE_ACTIVE:
			break;
	
		case POWER_MODE_SHORT_DOZE:
			if ( pPowerMgr->beaconListenInterval > 1 )
			{
				powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_N_BEACON);       
			}
			else
			{
				powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->beaconListenInterval,TNET_WAKE_ON_BEACON);     
			}
			break;
	
		case POWER_MODE_LONG_DOZE:
			if ( pPowerMgr->dtimListenInterval > 1 )
			{
				powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_N_DTIM);       
			}
			else
			{
				powerMgrSendMBXWakeUpConditions(hPowerMgr,pPowerMgr->dtimListenInterval,TNET_WAKE_ON_DTIM);     
			}
			break;
	
		default:
TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_ERROR, ": ERROR - PowerMode for user prioirty is: %d\n", pPowerMgr->powerMngModePriority[ POWER_MANAGER_USER_PRIORITY ].powerMode);
		}
	}/*end of if (psEnable)*/
}



/****************************************************************************************
*                        PowerMgr_PsPollFailureCB															*
****************************************************************************************
DESCRIPTION: Work around to solve AP bad behavior.
         Some old AP's have trouble with Ps-Poll - The solution will be to exit PS for a 
         period of time
				                                                                                               
INPUT:      	- hPowerMgr - Handle to the Power Manager	
OUTPUT:		
RETURN:    
****************************************************************************************/
static void PowerMgr_PsPollFailureCB( TI_HANDLE hPowerMgr )
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    if ( pPowerMgr->PsPollDeliveryFailureRecoveryPeriod )
    {
        paramInfo_t param;
             
        TRACE1(pPowerMgr->hReport, REPORT_SEVERITY_WARNING, " Oh boy, AP is not answering Ps-Poll's. enter active PS for %d Ms\n", pPowerMgr->PsPollDeliveryFailureRecoveryPeriod);

        /*
         * Set the system to Active power save 
         */
        param.paramType = POWER_MGR_POWER_MODE;
        param.content.powerMngPowerMode.PowerMode = POWER_MODE_ACTIVE;
        param.content.powerMngPowerMode.PowerMngPriority = POWER_MANAGER_PS_POLL_FAILURE_PRIORITY;
        powerMgr_setParam(hPowerMgr,&param);
        
        param.paramType = POWER_MGR_ENABLE_PRIORITY;
        param.content.powerMngPriority = POWER_MANAGER_PS_POLL_FAILURE_PRIORITY;
        powerMgr_setParam(hPowerMgr,&param);

        /*
         * Set timer to exit the active mode
         */
        tmr_StartTimer(pPowerMgr->hPsPollFailureTimer,
					   powerMgr_PsPollFailureTimeout,
					   (TI_HANDLE)pPowerMgr,
					   pPowerMgr->PsPollDeliveryFailureRecoveryPeriod,
					   TI_FALSE);
    } 
    else    /* Work-around is disabled */
    {
        TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_WARNING, " Oh boy, AP is not answering Ps-Poll's !!!\n");
    }
	return;
}

/****************************************************************************************
*                        powerMgr_PsPollFailureTimeout									*
****************************************************************************************
DESCRIPTION: After the timeout of ps-poll failure - return to normal behavior
				                                                                                               
INPUT:      	- hPowerMgr - Handle to the Power Manager	
OUTPUT:		
RETURN:    
****************************************************************************************/
static void powerMgr_PsPollFailureTimeout( TI_HANDLE hPowerMgr, TI_BOOL bTwdInitOccured )
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;
    paramInfo_t param;

    TRACE0(pPowerMgr->hReport, REPORT_SEVERITY_INFORMATION, " \n");
    
    /* disable Ps-Poll priority */
    param.paramType = POWER_MGR_DISABLE_PRIORITY;
    param.content.powerMngPriority = POWER_MANAGER_PS_POLL_FAILURE_PRIORITY;
    powerMgr_setParam(hPowerMgr,&param);

	return;
}

TI_BOOL PowerMgr_getReAuthActivePriority(TI_HANDLE thePowerMgrHandle)
{
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)thePowerMgrHandle;
	return pPowerMgr->reAuthActivePriority;
}
