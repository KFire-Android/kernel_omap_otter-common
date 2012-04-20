/*
 * TrafficMonitorAPI.h
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


/***************************************************************************/
/*                                                                                                                                                 */
/*        MODULE:       TrafficMonitor.h                                                                           */
/*    PURPOSE:  TrafficMonitor module Header file                                              */
/*                                                                                                                                                 */
/***************************************************************************/
#ifndef _TRAFFIC_MONITOR_API_H
#define _TRAFFIC_MONITOR_API_H

#include "DataCtrl_Api.h"
#include "DrvMainModules.h"

/**/
/* call back functions prototype.*/
/**/
typedef void (*TraffEevntCall_t)(TI_HANDLE Context,TI_UINT32 Cookie);


/*
 *      This enum list all the available traffic monitor event 
 *  that a client can register to.
 */
typedef enum
{
    TX_ALL_MSDU_FRAMES =1,                          
    RX_ALL_MSDU_FRAMES,
    TX_RX_DIRECTED_FRAMES,
    TX_RX_ALL_MSDU_FRAMES,
    TX_RX_ALL_MSDU_IN_BYTES,
    TX_RX_DIRECTED_IN_BYTES,
    TX_RX_ALL_802_11_DATA_IN_BYTES,
    TX_RX_ALL_802_11_DATA_FRAMES
} TraffEvntOptNum_t;

 

typedef enum
{
    TRAFF_EDGE = 0,
    TRAFF_LEVEL 
}TraffTrigger_t;

typedef enum
{
    TRAFF_UP = 0,
    TRAFF_DOWN 
}TraffDirection_t;

typedef struct
{
    /*initial param*/
        TraffEevntCall_t    CallBack;
    TI_HANDLE           Context; 
    TI_UINT32              Cookie;    
    TraffDirection_t    Direction;
    TraffTrigger_t      Trigger;
    int                                 Threshold;
    TI_UINT32              TimeIntervalMs;
    TraffEvntOptNum_t   MonitorType;
}TrafficAlertRegParm_t;


/*********************************************************************/


TI_HANDLE TrafficMonitor_create(TI_HANDLE hOs);
void TrafficMonitor_Init (TStadHandlesList *pStadHandles, TI_UINT32 BWwindowMs);
TI_STATUS TrafficMonitor_Destroy(TI_HANDLE hTrafficMonitor);
TI_HANDLE TrafficMonitor_RegEvent(TI_HANDLE hTrafficMonitor, TrafficAlertRegParm_t *TrafficAlertRegParm, TI_BOOL AutoResetCreate);
TI_STATUS TrafficMonitor_SetRstCondition(TI_HANDLE hTrafficMonitor,TI_HANDLE EventHandle,TI_HANDLE ResetEventHandle,TI_BOOL MutualRst);
int TrafficMonitor_GetFrameBandwidth(TI_HANDLE hTrafficMonitor);
void TrafficMonitor_UnregEvent(TI_HANDLE hTrafficMonitor, TI_HANDLE EventHandle);
void TrafficMonitor_Event(TI_HANDLE hTrafficMonitor,int Count,TI_UINT16 Mask,TI_UINT32 MonitorModuleType);
void TrafficMonitor_StopEventNotif(TI_HANDLE hTrafficMonitor,TI_HANDLE EventHandle);
void TrafficMonitor_StartEventNotif(TI_HANDLE hTrafficMonitor,TI_HANDLE EventHandle);
void TrafficMonitor_ResetEvent(TI_HANDLE hTrafficMonitor, TI_HANDLE EventHandle);
TI_STATUS TrafficMonitor_Stop(TI_HANDLE hTrafficMonitor);       
TI_STATUS TrafficMonitor_Start(TI_HANDLE hTrafficMonitor);      
TI_BOOL TrafficMonitor_IsEventOn(TI_HANDLE EventHandle);




#endif


