/*
 * TrafficMonitor.h
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
/*                                                                          */
/*        MODULE:       TrafficMonitor.h                                    */
/*    PURPOSE:  TrafficMonitor module Header file                           */
/*                                                                          */
/***************************************************************************/
#ifndef _TRAFFIC_MONITOR_H
#define _TRAFFIC_MONITOR_H


#include "tidef.h"
#include "GeneralUtilApi.h"
#include "TrafficMonitorAPI.h"

/* time interval to load for the down limit alert Timer *
 *(TrafficMonitor_t->TrafficMonTimer)                   */
#define MIN_MONITOR_INTERVAL  50 /*mSec*/

/*The max number of Alert Element that the traffic monitor will be able to Manage.*/
#define MAX_MONITORED_REQ     32

/* The max number of Alert element that can                *
 * be associated to other alert element for reset condition*/
#define MAX_RST_ELMENT_PER_ALERT 3

/* BW Window in MS. changing this number must take NUM_OF_SLIDING_WINDOWS into consideration  */ 
#define BW_WINDOW_MS			1000

#define NUM_OF_SLIDING_WINDOWS 8							/* Must be power of 2 !!!							 */
#define CYCLIC_COUNTER_ELEMENT (NUM_OF_SLIDING_WINDOWS - 1)	/* Note that it is aligned to NUM_OF_SLIDING_WINDOWS */

#define SIZE_OF_WINDOW_MS	   ( BW_WINDOW_MS / NUM_OF_SLIDING_WINDOWS) /* 125 Ms */

/* BandWidth_t 
	This struct is used for the sliding windows algorithm used to calculate the band width */
typedef struct
{
    TI_UINT32           uCurrentWindow;
    TI_UINT32           auFirstEventsTS[NUM_OF_SLIDING_WINDOWS];
    TI_UINT32           auWindowCounter[NUM_OF_SLIDING_WINDOWS];
}BandWidth_t;


/* The traffic manger class structure */
typedef struct
{
    TI_BOOL             Active;
    TI_HANDLE           NotificationRegList;

    TI_HANDLE           hOs;
    TI_HANDLE           hTimer;
    TI_HANDLE           hRxData;
    TI_HANDLE           hTxCtrl;
    
    TI_HANDLE           TxRegReqHandle;
    TI_HANDLE           RxRegReqHandle;
    
    BandWidth_t         DirectTxFrameBW;
    BandWidth_t         DirectRxFrameBW;
    
    TI_UINT8		    trafficDownTestIntervalPercent;	/* Percentage of max down events test interval     */
                                                        /*to use in our "traffic down" timer               */    
    TI_BOOL             DownTimerEnabled;	/* Indicates whether the "down traffic" timer is active or not */

    TI_HANDLE           hTrafficMonTimer;

}TrafficMonitor_t;


/* Function definition that used for event Aggregation/filtering/etc.. */
typedef void (*TraffActionFunc_t)(TI_HANDLE TraffElem,int Count); 
                
/* This enum holds the event providers that are optional in the system */
typedef enum
{
        TX_TRAFF_MODULE                                 = 0,
        RX_TRAFF_MODULE                         = 1,
    MAX_NUM_MONITORED_MODULES  /* Don't move this enum this index defines the 
                                  number of module that can be monitored.*/                   
}MonModuleTypes_t;



/*
 *      Alert State option enum 
 *  0.  disabled
 *  1.  ON but not active
 *  2.  ON and active
 */
typedef enum {
    ALERT_WAIT_FOR_RESET = 0,          /* Event has been triggered, awaiting reset event to occur */
        ALERT_OFF,
    ALERT_ON
}TraffAlertState_t;


/* Basic Alert element structure */
typedef struct AlertElement_t
{
    /*initial param*/
    TraffAlertState_t   CurrentState; 
    int                             EventCounter;
    int                             Threshold;
    TI_UINT32                              TimeOut;  
    TraffDirection_t    Direction;
    TraffTrigger_t      Trigger;
    TI_BOOL                Enabled;
    int                             LastCounte;
        TraffEevntCall_t        CallBack;
    TI_HANDLE           Context ;
    TI_UINT32              Cookie;    
    TI_UINT32              TimeIntervalMs;               
    TI_BOOL                AutoCreated;
    TI_BOOL                RstWasAssigned;
    TraffActionFunc_t   ActionFunc;
    TI_UINT32              MonitorMask[MAX_NUM_MONITORED_MODULES];
    struct AlertElement_t  *ResetElment[MAX_RST_ELMENT_PER_ALERT];     
}TrafficAlertElement_t;

#endif
