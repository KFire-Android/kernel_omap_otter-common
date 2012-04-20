/*
 * PowerMgrDebug.c
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

/** \file PowerMgrDebug.c
 *  \brief This is the PowerMgrDebug module implementation.
 *  \
 *  \date 13-Jun-2004
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  PowerMgrDebug                                                      *
 *   PURPOSE: PowerMgrDebug Module implementation.                               *
 *                                                                          *
 ****************************************************************************/

#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "PowerMgrDebug.h"
#include "PowerMgr_API.h"
#include "PowerMgr.h"


/*****************************************************************************
 **         Private Function prototypes                                     **
 *****************************************************************************/
static void PowerMgrDebug_printAllCommands(void);


/*****************************************************************************
 **         Public Function prototypes                                      **
 *****************************************************************************/
void powerMgrDebugFunction(TI_HANDLE thePowerMgrHandle,
                           TI_UINT32 theDebugFunction,
                           void* theParameter)
{
    switch (theDebugFunction)
    {
    case POWER_MGR_DEBUG_PRINT_ALL_COMMANDS:
        PowerMgrDebug_printAllCommands();
        break;

    case POWER_MGR_DEBUG_START_PS:
        PowerMgr_startPS(thePowerMgrHandle);
        break;

    case POWER_MGR_DEBUG_STOP_PS:
        PowerMgr_stopPS(thePowerMgrHandle, TI_TRUE);
        break;

   case POWER_MGR_PRINT_OBJECTS:
        PowerMgr_printObject(thePowerMgrHandle);
        break;

    default:
        WLAN_OS_REPORT(("(%d) - ERROR - Invalid function type in POWER MANAGER DEBUG Function Command: %d\n",
                        __LINE__,theDebugFunction));
        break;
    }
}

/*****************************************************************************
 **         Private Function prototypes                                     **
 *****************************************************************************/
static void PowerMgrDebug_printAllCommands(void)
{
    WLAN_OS_REPORT(("\n\n"));
    WLAN_OS_REPORT(("POWER MGR DEBUG module commands:\n"));
    WLAN_OS_REPORT(("================================\n"));
    WLAN_OS_REPORT(("syntax         description\n"));
    WLAN_OS_REPORT(("%d -  POWER_MGR_DEBUG_PRINT_ALL_COMMANDS\n", POWER_MGR_DEBUG_PRINT_ALL_COMMANDS));
    WLAN_OS_REPORT(("%d -  POWER_MGR_DEBUG_START_PS\n", POWER_MGR_DEBUG_START_PS));
    WLAN_OS_REPORT(("%d -  POWER_MGR_DEBUG_STOP_PS\n", POWER_MGR_DEBUG_STOP_PS));
    WLAN_OS_REPORT(("%d -  POWER_MGR_PRINT_OBJECTS\n", POWER_MGR_PRINT_OBJECTS));
    WLAN_OS_REPORT(("\n\n"));
}

