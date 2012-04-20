/*
 * HealthMonitorDbg.c
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

/** \file HealthMonitorDbg.c
 *  \brief This file include the HealthMonitordebug module implementation
 *  \
 *  \date 2-Apr-2006
 */

#include "tidef.h"
#include "MacServices.h"
#include "ScanCncn.h"
#include "scanCncnDbg.h"
#include "healthMonitor.h"
#include "ScanSrv.h"
#include "ScanSrvSM.h"
#include "TWDriver.h"
#include "conn.h"
#include "PowerSrv.h"
#include "PowerSrvSM.h"
#include "MacServices_api.h"
#include "HealthMonitorDbg.h"
#include "report.h"
#include "timer.h"
#include "DrvMain.h"
#include "DrvMainModules.h"


static void noScanCompleteTimer (TI_HANDLE hTWD);


/** \file HealthMonitorDbg.c
 *  \
 *  \date 2-Apr-2006
 * \brief Main Recovery debug function
 *
 * Function Scope \e Public.\n
 * \param pStadHandles - modules handles list.\n
 * \param funcType       - the specific debug function.\n
 * \param pParam         - parameters for the debug function.\n
 */
void healthMonitorDebugFunction (TStadHandlesList *pStadHandles, TI_UINT32 funcType, void *pParam)
{
    TI_HANDLE       hHealthMonitor  = pStadHandles->hHealthMonitor;
    TI_HANDLE       hTWD            = pStadHandles->hTWD;
    MacServices_t  *pMacServices    = ((TTwd *)hTWD)->hMacServices;
    TI_HANDLE       hMeasurementSRV = pMacServices->hMeasurementSRV;
    scanSRV_t      *pScanSRV        = (scanSRV_t *)pMacServices->hScanSRV; 
    PowerSrvSM_t   *pPowerSrvSM     = (PowerSrvSM_t*)((powerSrv_t*)pMacServices->hPowerSrv)->hPowerSrvSM;

    switch (funcType)
    {
    case DBG_HM_PRINT_HELP:
        printHealthMonitorDbgFunctions();
        break;

    case DBG_HM_RECOVERY_NO_SCAN_COMPLETE: 
        /* check if the no scan complete timer is running. if so - we can stop the timer and trigger recovery */
        if (pScanSRV->bTimerRunning)
        {
            /* perform the recovery directly */
            noScanCompleteTimer (hTWD);
        }
        else /* scan Srv is probably entering/exiting PowerSave. We won't handle this case since it happens rarely */
        {
            WLAN_OS_REPORT(("Scan isn't running at the moment, so try again (make sure it is enabled)\n"));
        }
        break;
        
    case DBG_HM_RECOVERY_MBOX_FAILURE:  
        TWD_CheckMailboxCb (hTWD, TI_NOK, NULL);
        break;

    case DBG_HM_RECOVERY_HW_AWAKE_FAILURE:
        healthMonitor_sendFailureEvent (hHealthMonitor, HW_AWAKE_FAILURE);
        break;

    case DBG_HM_RECOVERY_TX_STUCK:  
        healthMonitor_sendFailureEvent (hHealthMonitor, TX_STUCK);
        break;
    
    case DBG_HM_DISCONNECT_TIMEOUT:  
        healthMonitor_sendFailureEvent (hHealthMonitor, DISCONNECT_TIMEOUT);
        break;

    case DBG_HM_RECOVERY_POWER_SAVE_FAILURE:  
        tmr_StopTimer (pPowerSrvSM->hPwrSrvSmTimer);  /* stop the PS guard timer */
        healthMonitor_sendFailureEvent (hHealthMonitor, POWER_SAVE_FAILURE);
        break;

    case DBG_HM_RECOVERY_MEASUREMENT_FAILURE:  
        MacServices_measurementSRV_startStopTimerExpired (hMeasurementSRV, TI_FALSE);
        break;

    case DBG_HM_RECOVERY_BUS_FAILURE:  
        healthMonitor_sendFailureEvent (hHealthMonitor, BUS_FAILURE);
        break;

    case DBG_HM_RECOVERY_FROM_CLI:
        drvMain_Recovery (pStadHandles->hDrvMain);
        break;

	case DBG_HM_RECOVERY_FROM_HW_WD_EXPIRE:
        healthMonitor_sendFailureEvent (hHealthMonitor, HW_WD_EXPIRE);
        break;

	case DBG_HM_RECOVERY_RX_XFER_FAILURE:
        healthMonitor_sendFailureEvent (hHealthMonitor, RX_XFER_FAILURE);
        break;

    default:
        WLAN_OS_REPORT(("Invalid function type in health monitor debug function: %d\n", funcType));
        break;
    }
}


/** \file HealthMonitorDbg.c
 *  \
 *  \date 2-Apr-2006
 * \brief Prints Recovery debug menu
 *
 * Function Scope \e Public.\n
 */
void printHealthMonitorDbgFunctions(void)
{
    WLAN_OS_REPORT(("   HealthMonitor Debug Functions       \n"));
    WLAN_OS_REPORT(("---------------------------------------\n"));
    WLAN_OS_REPORT(("2000 - Print HealthMonitor Debug Help  \n"));
    WLAN_OS_REPORT(("2001 - Trigger NO_SCAN_COMPLETE        \n"));
    WLAN_OS_REPORT(("2002 - Trigger MBOX_FAILURE            \n"));
    WLAN_OS_REPORT(("2003 - Trigger HW_AWAKE_FAILURE        \n"));
    WLAN_OS_REPORT(("2004 - Trigger TX_STUCK                \n"));
    WLAN_OS_REPORT(("2005 - Trigger DISCONNECT_TIMEOUT      \n"));
    WLAN_OS_REPORT(("2006 - Trigger POWER_SAVE_FAILURE      \n"));
    WLAN_OS_REPORT(("2007 - Trigger MEASUREMENT_FAILURE     \n"));
    WLAN_OS_REPORT(("2008 - Trigger BUS_FAILURE             \n"));
    WLAN_OS_REPORT(("2009 - Start RECOVERY_FROM_CLI         \n"));
    WLAN_OS_REPORT(("2010 - Trigger HW_WD_EXPIRE            \n"));
    WLAN_OS_REPORT(("2011 - Trigger RX_XFER_FAILURE         \n"));
}

static void noScanCompleteTimer (TI_HANDLE hTWD)
{
    MacServices_t *pMacServices = ((TTwd *)hTWD)->hMacServices;
    scanSRV_t     *pScanSRV     = (scanSRV_t *)pMacServices->hScanSRV; 
    

    /* stop the no scan complete timer in the scan SRV */
    tmr_StopTimer (pScanSRV->hScanSrvTimer);
    pScanSRV->bTimerRunning = TI_FALSE;

    /* simulate the timer expiry */
    MacServices_scanSRV_scanTimerExpired ((TI_HANDLE)pScanSRV, TI_FALSE);
}
