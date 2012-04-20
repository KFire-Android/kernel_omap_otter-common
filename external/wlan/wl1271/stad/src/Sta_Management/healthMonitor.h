/*
 * healthMonitor.h
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

/** \file healthMonitor.c
 *  \brief Firmware Recovery Mechanizem
 *
 */

/****************************************************************************/
/*                                                                          */
/*      MODULE:     healthMonitor.h                                         */
/*      PURPOSE:    Driver interface to OS abstraction layer                */
/*                                                                          */
/****************************************************************************/

#ifndef HEALTHMONITOR_H
#define HEALTHMONITOR_H

#include "paramOut.h"
#include "DrvMainModules.h"


/* Config manager state machine defintions */
typedef enum healthMonitorState_e
{
    HEALTH_MONITOR_STATE_DISCONNECTED,
    HEALTH_MONITOR_STATE_SCANNING,
    HEALTH_MONITOR_STATE_CONNECTED 

} healthMonitorState_e;


/* Public Functions Prototypes */
TI_HANDLE healthMonitor_create         (TI_HANDLE hOs);
void healthMonitor_init                (TStadHandlesList *pStadHandles);
TI_STATUS healthMonitor_SetDefaults    (TI_HANDLE hHealthMonitor, healthMonitorInitParams_t *healthMonitorInitParams);
TI_STATUS healthMonitor_unload         (TI_HANDLE hHealthMonitor);
void healthMonitor_PerformTest         (TI_HANDLE hHealthMonitor, TI_BOOL bTwdInitOccured);
void healthMonitor_setState            (TI_HANDLE hHealthMonitor, healthMonitorState_e state);
void healthMonitor_sendFailureEvent    (TI_HANDLE hHealthMonitor, EFailureEvent failureEvent);
void healthMonitor_printFailureEvents  (TI_HANDLE hHealthMonitor);
TI_STATUS healthMonitor_SetParam       (TI_HANDLE hHealthMonitor, paramInfo_t *pParam);
TI_STATUS healthMonitor_GetParam       (TI_HANDLE hHealthMonitor, paramInfo_t *pParam);


#endif
