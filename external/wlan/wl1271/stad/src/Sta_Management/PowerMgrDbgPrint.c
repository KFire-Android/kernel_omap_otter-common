/*
 * PowerMgrDbgPrint.c
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

/** \file PowerMgrDbgPrint.c
 *  \brief Includes primtoputs for debugging the power manager module.
 *  \
 *  \date 29-Aug-2006
 */

#ifndef __POWER_MGR_DBG_PRINT__
#define __POWER_MGR_DBG_PRINT__

#define __FILE_ID__  FILE_ID_72
#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "PowerMgr.h"

#ifdef TI_DBG

#define MAX_DESC_LENGTH 32

#ifdef REPORT_LOG
void powerMgrPrintPriorities( TI_HANDLE hPowerMgr, powerMngModePriority_t* pPriorities );

static char booleanDesc[ 2 ][ MAX_DESC_LENGTH ] = {"no", "yes"};
static char powerModeDesc[ POWER_MODE_MAX ][ MAX_DESC_LENGTH ] = 
                        { "Auto", "Active", "Short doze", "Long doze", "PS only" };
static char powerPolicyDesc[ POWERAUTHO_POLICY_NUM ][ MAX_DESC_LENGTH ] = 
                        { "ELP", "PD", "AWAKE" };
static char priorityDesc[ POWER_MANAGER_MAX_PRIORITY ][ MAX_DESC_LENGTH ] = 
                        { "User priority", "Soft-Gemini priority" };
static char psStatusDesc[ POWER_SAVE_STATUS_NUMBER ][ MAX_DESC_LENGTH ]=
                        { "Enter fail", "Enter success", "Exit fail", "Exit succes" };
#endif

/****************************************************************************************
*                        PowerMgr_printObject                                                          *
****************************************************************************************
DESCRIPTION: print configuration of the PowerMgr object - use for debug!
                                                                                                                              
INPUT:          - hPowerMgr             - Handle to the Power Manager
OUTPUT:     
RETURN:    void.\n
****************************************************************************************/
void PowerMgr_printObject( TI_HANDLE hPowerMgr )
{
#ifdef REPORT_LOG
    PowerMgr_t *pPowerMgr = (PowerMgr_t*)hPowerMgr;

    WLAN_OS_REPORT(("------------ Power Manager Object ------------\n\n"));
    WLAN_OS_REPORT(("PS enabled: %s, desired power mode profile: %s, last power mode profile: %s\n",
                    booleanDesc[ pPowerMgr->psEnable ], powerModeDesc[ pPowerMgr->desiredPowerModeProfile ],
                    powerModeDesc[ pPowerMgr->lastPowerModeProfile ]));
    WLAN_OS_REPORT(("Default power policy: %ss, PS power policy: %s\n", 
                    powerPolicyDesc[ pPowerMgr->defaultPowerLevel ],
                    powerPolicyDesc[ pPowerMgr->PowerSavePowerLevel ]));
    WLAN_OS_REPORT(("Current priority: %s\n", priorityDesc[ pPowerMgr->powerMngPriority ]));
    powerMgrPrintPriorities( hPowerMgr, pPowerMgr->powerMngModePriority );
    WLAN_OS_REPORT(("\n------------ auto mode parameters ------------\n"));
    WLAN_OS_REPORT(("Interval: %d, active threshold: %d, doze threshold: %d, doze mode: %s\n\n",
                    pPowerMgr->autoModeInterval, pPowerMgr->autoModeActiveTH, pPowerMgr->autoModeDozeTH,
                    powerModeDesc[ pPowerMgr->autoModeDozeMode ]));
    WLAN_OS_REPORT(("Beacon listen interval:%d, DTIM listen interval:%d, last PS status: %s\n\n",
                    pPowerMgr->beaconListenInterval, pPowerMgr->dtimListenInterval,
                    psStatusDesc[ pPowerMgr->lastPsTransaction ]));
    WLAN_OS_REPORT(("------------ Handles ------------\n"));
    WLAN_OS_REPORT(("%-15s: 0x%x, %-15s:0x%x\n","hOS", pPowerMgr->hOS, "hTWD", pPowerMgr->hTWD));
    WLAN_OS_REPORT(("%-15s: 0x%x, %-15s:0x%x\n","hReport", pPowerMgr->hReport, "hTrafficMonitor", pPowerMgr->hTrafficMonitor));
    WLAN_OS_REPORT(("%-15s: 0x%x, %-15s:0x%x\n","hSiteMgr", pPowerMgr->hSiteMgr, "hTWD", pPowerMgr->hTWD));
    WLAN_OS_REPORT(("%-15s: 0x%x, %-15s:0x%x\n","hRetryPsTimer", pPowerMgr->hRetryPsTimer, "hActiveTMEvent", pPowerMgr->passToActiveTMEvent));
    WLAN_OS_REPORT(("%-15s: 0x%x\n", "hDozeTMEvent", pPowerMgr->passToDozeTMEvent));
#endif
}

#ifdef REPORT_LOG
void powerMgrPrintPriorities( TI_HANDLE hPowerMgr, powerMngModePriority_t* pPriorities )
{
    int i;

    for ( i = 0; i < POWER_MANAGER_MAX_PRIORITY; i++ )
    {
        WLAN_OS_REPORT(("Priority: %-15s, enabled: %s, power mode: %s\n",
                        priorityDesc[ i ], booleanDesc[ pPriorities[ i ].priorityEnable ],
                         powerModeDesc[ pPriorities[ i ].powerMode ]));
    }
}
#endif
#endif /* TI_DBG */

#endif /* __POWER_MGR_DBG_PRINT__ */
