/*
 * healthMonitor.c
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

/** \file healthMonitor.c
 *  \brief Firmware Recovery Mechanism
 */

/** \file   healthMonitor.c 
 *  \brief  The health monitor module. Gets failures indications and handle them.
 *
 *  For periodic check, use HW watchdog mechanizem instead of local periodic timer check 
 *
 *  \see    healthMonitor.h
 */

#define __FILE_ID__  FILE_ID_66
#include "healthMonitor.h"
#include "osApi.h"
#include "timer.h"
#include "report.h"
#include "siteMgrApi.h"
#include "PowerMgr_API.h"
#include "currBss.h"
#include "DataCtrl_Api.h"
#include "TWDriver.h"
#include "SoftGeminiApi.h"
#include "currBss.h"
#include "rsnApi.h"
#include "DrvMain.h"
#include "DrvMainModules.h"
#include "TWDriverInternal.h"


typedef struct
{
    /* handles to other modules */
    TI_HANDLE            hOs;                    /* handle to the OS object */
    TI_HANDLE            hReport;                /* handle to the report object */
    TI_HANDLE            hTWD;                   /* handle to the TWD object */
    TI_HANDLE            hSiteMgr;               /* handle to the site manager object */
    TI_HANDLE            hScr;                   /* handle to the SCR object */
    TI_HANDLE            hSoftGemini;            /* handle to the Soft Gemini object */
	TI_HANDLE            hDrvMain;               /* handle to the Recovery Mgr object */
    TI_HANDLE            hTxCtrl;                /* handle to the TX Ctrl object */
    TI_HANDLE            hCurrBss;               /* handle to the currBss object */
	TI_HANDLE            hRsn;                   /* handle to the RSN */
	TI_HANDLE            hTimer;                 /* handle to the Timer module object */
	TI_HANDLE            hContext;               /* handle to the context-engine object */

    /* Timers handles */
    TI_HANDLE            hFailTimer;             /* failure event timer */
    
    /* Management variables */
    TI_UINT32            numOfHealthTests;       /* number of health tests performed counter */
    healthMonitorState_e state;                  /* health monitor state */
    TI_BOOL              bFullRecoveryEnable;    /* full recovery enable flag */
    TI_BOOL              recoveryTriggerEnabled [MAX_FAILURE_EVENTS];
                                                 /* recovery enable flags per trigger type */
    TI_UINT32            failureEvent;           /* current recovery trigger */
    TI_UINT32            keepAliveIntervals;     /* number of health monitor timer intervals at which keep alive should be sent */
    TI_UINT32            currentKeepAliveCounter;/* counting how many timer intervals had passed w/o a keep alive */

    /* Recoveries Statistics */
    TI_UINT32          	 recoveryTriggersNumber [MAX_FAILURE_EVENTS];                                                  
                                                 /* Number of times each recovery trigger occured */
    TI_UINT32            numOfRecoveryPerformed; /* number of recoveries performed */
    
} THealthMonitor;


static void healthMonitor_proccessFailureEvent (TI_HANDLE hHealthMonitor, TI_BOOL bTwdInitOccured);


#ifdef REPORT_LOG

static char* sRecoveryTriggersNames [MAX_FAILURE_EVENTS] = 
{
    "NO_SCAN_COMPLETE_FAILURE",
    "MBOX_FAILURE",
    "HW_AWAKE_FAILURE",
    "TX_STUCK",
    "DISCONNECT_TIMEOUT",
    "POWER_SAVE_FAILURE",
    "MEASUREMENT_FAILURE",
    "BUS_FAILURE",
    "HW_WD_EXPIRE",
    "RX_XFER_FAILURE"
};
#endif


/** 
 * \fn     healthMonitor_create 
 * \brief  Create module object
 * 
 * Create module object.
 * 
 * \note    
 * \param  hOs  - The OS adaptation handle
 * \return The created module handle  
 * \sa     
 */ 
TI_HANDLE healthMonitor_create (TI_HANDLE hOs)
{
    THealthMonitor *pHealthMonitor;
 
    /* Allocate memory for the health monitor object and nullify it */
    pHealthMonitor = (THealthMonitor*)os_memoryAlloc (hOs, sizeof(THealthMonitor));
    if (pHealthMonitor == NULL)
    {
        return NULL;
    }
    os_memoryZero (hOs, pHealthMonitor, sizeof(THealthMonitor));

    /* Store OS object handle */
    pHealthMonitor->hOs = hOs;

    return (TI_HANDLE)pHealthMonitor;
}


/** 
 * \fn     healthMonitor_init 
 * \brief  Init module handles and variables
 * 
 * Init module handles and variables.
 * 
 * \note    
 * \param  pStadHandles  - The driver modules handles
 * \return void  
 * \sa     
 */ 
void healthMonitor_init (TStadHandlesList *pStadHandles)
{
    THealthMonitor *pHealthMonitor = (THealthMonitor *)(pStadHandles->hHealthMonitor);

    pHealthMonitor->hReport         = pStadHandles->hReport;
    pHealthMonitor->hTWD            = pStadHandles->hTWD;
    pHealthMonitor->hSiteMgr        = pStadHandles->hSiteMgr;
    pHealthMonitor->hScr            = pStadHandles->hSCR;
    pHealthMonitor->hSoftGemini     = pStadHandles->hSoftGemini;
    pHealthMonitor->hDrvMain        = pStadHandles->hDrvMain;
	pHealthMonitor->hTxCtrl		    = pStadHandles->hTxCtrl;
    pHealthMonitor->hCurrBss        = pStadHandles->hCurrBss;
    pHealthMonitor->hRsn            = pStadHandles->hRsn;
    pHealthMonitor->hTimer          = pStadHandles->hTimer;
    pHealthMonitor->hContext        = pStadHandles->hContext;

    pHealthMonitor->state           = HEALTH_MONITOR_STATE_DISCONNECTED;
    pHealthMonitor->failureEvent    = (TI_UINT32)NO_FAILURE;

    /* Register the failure event callback */
    TWD_RegisterCb (pHealthMonitor->hTWD, 
                    TWD_EVENT_FAILURE, 
                    (void *)healthMonitor_sendFailureEvent, 
                    (void *)pHealthMonitor);
}


/** 
 * \fn     healthMonitor_SetDefaults 
 * \brief  Set module defaults and create timers
 * 
 * Set module defaults from Ini-file and create timers.
 * 
 * \note    
 * \param  hHealthMonitor  - The module's handle
 * \param  healthMonitorInitParams  - The module's parameters default values (from Ini-file).
 * \return void  
 * \sa     
 */ 
TI_STATUS healthMonitor_SetDefaults (TI_HANDLE    hHealthMonitor, healthMonitorInitParams_t *healthMonitorInitParams)
{
    THealthMonitor *pHealthMonitor = hHealthMonitor;
    int i;

    /* Registry configuration */
    pHealthMonitor->bFullRecoveryEnable   = healthMonitorInitParams->FullRecoveryEnable;

    for (i = 0; i < MAX_FAILURE_EVENTS; i++)
    {
        pHealthMonitor->recoveryTriggerEnabled[i] = healthMonitorInitParams->recoveryTriggerEnabled[i];
    }

    /* Create recovery request timer */
    pHealthMonitor->hFailTimer = tmr_CreateTimer (pHealthMonitor->hTimer);
    if (pHealthMonitor->hFailTimer == NULL)
    {
        TRACE0(pHealthMonitor->hReport, REPORT_SEVERITY_ERROR, "healthMonitor_SetDefaults(): Failed to create hFailTimer!\n");
		return TI_NOK;
    }

    return TI_OK;
}


/***********************************************************************
 *                        healthMonitor_unload
 ***********************************************************************
DESCRIPTION: 
           
INPUT:      

OUTPUT:

RETURN: 

************************************************************************/
TI_STATUS healthMonitor_unload (TI_HANDLE hHealthMonitor)
{
    THealthMonitor *pHealthMonitor;

    pHealthMonitor = (THealthMonitor*)hHealthMonitor;

    if (pHealthMonitor != NULL)
    {
        if (NULL != pHealthMonitor->hFailTimer)
        {
            /* Release the timer */
            tmr_DestroyTimer (pHealthMonitor->hFailTimer);
        }

        /* Freeing the object should be called last !!!!!!!!!!!! */
        os_memoryFree (pHealthMonitor->hOs, pHealthMonitor, sizeof(THealthMonitor));
    }

    return TI_OK;
}


/***********************************************************************
 *                        healthMonitor_setState
 ***********************************************************************
DESCRIPTION: 
           

INPUT:      

OUTPUT:

RETURN: 

************************************************************************/
void healthMonitor_setState (TI_HANDLE hHealthMonitor, healthMonitorState_e state)
{
    THealthMonitor *pHealthMonitor = (THealthMonitor*)hHealthMonitor;

    pHealthMonitor->state = state;
}


/***********************************************************************
 *                        healthMonitor_PerformTest
 ***********************************************************************
DESCRIPTION: Called periodically by timer every few seconds (depends on connection state),
             or optionally by external application.
           
INPUT:      hHealthMonitor	-	Module handle.
            bTwdInitOccured -   Indicates if TWDriver recovery occured since timer started 

OUTPUT:

RETURN: 

************************************************************************/
void healthMonitor_PerformTest (TI_HANDLE hHealthMonitor, TI_BOOL bTwdInitOccured)
{
    THealthMonitor *pHealthMonitor = (THealthMonitor*)hHealthMonitor;

    pHealthMonitor->numOfHealthTests++;

    /* Send health-check command to FW, just to ensure command complete is accepted */
    TWD_CmdHealthCheck (pHealthMonitor->hTWD);
}


/***********************************************************************
 *                        healthMonitor_sendFailureEvent
 ***********************************************************************
DESCRIPTION:    Entry point for all low level modules to send a failure evrnt

INPUT:          handle - health monitor handle
                failureEvent - the error

OUTPUT:

RETURN:    

************************************************************************/
void healthMonitor_sendFailureEvent (TI_HANDLE hHealthMonitor, EFailureEvent failureEvent)
{
    THealthMonitor *pHealthMonitor = (THealthMonitor*)hHealthMonitor;

    /* Check the recovery process is already running */
    if (pHealthMonitor->failureEvent < MAX_FAILURE_EVENTS)
    {
        TRACE0(pHealthMonitor->hReport, REPORT_SEVERITY_WARNING , ": recovery process is already handling , new trigger is \n");
    }

    /* Recovery is performed only if this trigger is enabled in the .INI file */
    else if (pHealthMonitor->recoveryTriggerEnabled[failureEvent])
    {
        pHealthMonitor->failureEvent = failureEvent;
        /* 
         * NOTE: start timer with minimum expiry (1 msec) for recovery will start
         *       from the top of the stack 
         */
        tmr_StartTimer (pHealthMonitor->hFailTimer,
                        healthMonitor_proccessFailureEvent,
                        (TI_HANDLE)pHealthMonitor,
                        1,
                        TI_FALSE);
    }
    else 
    {
        TRACE0(pHealthMonitor->hReport, REPORT_SEVERITY_ERROR , ": Recovery trigger  is disabled!\n");
    }
}


/***********************************************************************
 *                        healthMonitor_proccessFailureEvent
 ***********************************************************************
DESCRIPTION:    this is the central error function - will be passed as call back 
                to the TnetWDriver modules. it will parse the error and dispatch the 
                relevant action (recovery or not) 

INPUT:      hHealthMonitor - health monitor handle
            bTwdInitOccured -   Indicates if TWDriver recovery occured since timer started 

OUTPUT:

RETURN:    

************************************************************************/
void healthMonitor_proccessFailureEvent (TI_HANDLE hHealthMonitor, TI_BOOL bTwdInitOccured)
{
    THealthMonitor *pHealthMonitor = (THealthMonitor*)hHealthMonitor;

    /* Check failure event validity */
    if (pHealthMonitor->failureEvent < MAX_FAILURE_EVENTS)
    {
        pHealthMonitor->recoveryTriggersNumber[pHealthMonitor->failureEvent] ++;

        TRACE2(pHealthMonitor->hReport, REPORT_SEVERITY_CONSOLE, "***** recovery trigger: failureEvent =%d *****, ts=%d\n", pHealthMonitor->failureEvent, os_timeStampMs(pHealthMonitor->hOs));
        WLAN_OS_REPORT (("***** recovery trigger: %s *****, ts=%d\n", sRecoveryTriggersNames[pHealthMonitor->failureEvent], os_timeStampMs(pHealthMonitor->hOs)));

        if (TWD_RecoveryEnabled (pHealthMonitor->hTWD))
        {
            pHealthMonitor->numOfRecoveryPerformed ++;
            drvMain_Recovery (pHealthMonitor->hDrvMain);
        }
        else
        {
            TRACE0(pHealthMonitor->hReport, REPORT_SEVERITY_CONSOLE, "healthMonitor_proccessFailureEvent: Recovery is disabled in tiwlan.ini, abort recovery process\n");
            WLAN_OS_REPORT(("healthMonitor_proccessFailureEvent: Recovery is disabled in tiwlan.ini, abort recovery process\n"));
        }

        pHealthMonitor->failureEvent = (TI_UINT32)NO_FAILURE;
    }
    else
    {
        TRACE1(pHealthMonitor->hReport, REPORT_SEVERITY_ERROR , "unsupported failure event = %d\n", pHealthMonitor->failureEvent);
    }    
}


/***********************************************************************
 *                        healthMonitor_printFailureEvents
 ***********************************************************************
DESCRIPTION:

INPUT:

OUTPUT:

RETURN:
************************************************************************/
void healthMonitor_printFailureEvents(TI_HANDLE hHealthMonitor)
{
#ifdef TI_DBG
#ifdef REPORT_LOG
    THealthMonitor  *pHealthMonitor = (THealthMonitor*)hHealthMonitor;
    int i;

    WLAN_OS_REPORT(("-------------- STA Health Failure Statistics ---------------\n"));
    WLAN_OS_REPORT(("FULL RECOVERY PERFORMED    = %d\n", pHealthMonitor->numOfRecoveryPerformed));
    for (i = 0; i < MAX_FAILURE_EVENTS; i++)
    {
        WLAN_OS_REPORT(("%27s= %d\n", sRecoveryTriggersNames[ i ], pHealthMonitor->recoveryTriggersNumber[ i ]));
    }
    WLAN_OS_REPORT(("Maximum number of commands in mailbox queue = %u\n", TWD_GetMaxNumberOfCommandsInQueue(pHealthMonitor->hTWD)));
    WLAN_OS_REPORT(("Health Test Performed       = %d\n", pHealthMonitor->numOfHealthTests));
    WLAN_OS_REPORT(("\n"));
#endif
#endif /* TI_DBG */
}


/***********************************************************************
 *                        healthMonitor_SetParam									
 ***********************************************************************
DESCRIPTION: Set module parameters from the external command interface	
                                                                                                   
INPUT:      hHealthMonitor	-	module handle.
			pParam	        -	Pointer to the parameter		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS healthMonitor_SetParam (TI_HANDLE hHealthMonitor, paramInfo_t *pParam)
{
    THealthMonitor *pHealthMonitor = (THealthMonitor*)hHealthMonitor;
	TI_STATUS eStatus = TI_OK;

    TRACE1(pHealthMonitor->hReport, REPORT_SEVERITY_INFORMATION, "healthMonitor_SetParam() - %d\n", pParam->paramType);
	
	switch(pParam->paramType)
	{

	case HEALTH_MONITOR_CHECK_DEVICE:
        /* Send health check command to FW if we are not disconnceted */
        if (pHealthMonitor->state != HEALTH_MONITOR_STATE_DISCONNECTED) 
        {
            healthMonitor_PerformTest (hHealthMonitor, TI_FALSE);
        }
		break;

	default:
        TRACE1(pHealthMonitor->hReport, REPORT_SEVERITY_ERROR, "healthMonitor_SetParam(): Params is not supported, %d\n", pParam->paramType);
	}

	return eStatus;
}


/***********************************************************************
 *			      healthMonitor_GetParam									
 ***********************************************************************
DESCRIPTION: Get module parameters from the external command interface
                                                                                                   
INPUT:      hHealthMonitor	-	module handle.
			pParam	        -	Pointer to the parameter		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS healthMonitor_GetParam (TI_HANDLE hHealthMonitor, paramInfo_t *pParam)
{ 
	return TI_OK;
}




