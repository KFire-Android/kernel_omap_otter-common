/*
 * TI_IPC_Api.h
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

/*---------------------------------------------------------------*/
/**/
/*  TI IPC header file*/
/**/
/*  Author :    Texas Instruments*/
/**/
/*  Filename: TI_IPC_Api.h*/
/**/
/*  Version  :1.0*/
/**/
/*  Last update date : 15/03/2004*/
/**/
/*---------------------------------------------------------------*/

#ifndef _TI_IPC_API_H
#define _TI_IPC_API_H

#include "tidef.h"


#define MAX_REGISTERED_MODULES 5
#define MAX_EVENT_DATA_SIZE 128
#define MAX_SEND_EVENTS 4

#ifdef  __cplusplus
extern "C" {
#endif

/*******************Defines*********************/

/* WARNING! DON'T CHANGE THE ORDER OF EVENTS! */
/* OS EVENTS MUST COME FIRST!*/

enum
{
    IPC_EVENT_ASSOCIATED = 0,
    IPC_EVENT_DISASSOCIATED,
    IPC_EVENT_LINK_SPEED,
    IPC_EVENT_AUTH_SUCC,
    IPC_EVENT_SCAN_REPORT,
    IPC_EVENT_SCAN_COMPLETE,
    IPC_EVENT_SCAN_STOPPED,
    IPC_EVENT_CCKM_START,
    IPC_EVENT_MEDIA_SPECIFIC,
    IPC_EVENT_MAX_OS_EVENT = IPC_EVENT_MEDIA_SPECIFIC,
    IPC_EVENT_EAPOL,
    IPC_EVENT_BOUND,
    IPC_EVENT_UNBOUND,
    IPC_EVENT_PREAUTH_EAPOL,
    IPC_EVENT_RESERVED2,
    IPC_EVENT_LOW_RSSI,
    IPC_EVENT_TSPEC_STATUS,
    IPC_EVENT_TSPEC_RATE_STATUS,
    IPC_EVENT_MEDIUM_TIME_CROSS,
    IPC_EVENT_ROAMING_COMPLETE,
    IPC_EVENT_EAP_AUTH_FAILURE,
    IPC_EVENT_WPA2_PREAUTHENTICATION,
    IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED,
    IPC_EVENT_SCAN_FAILED,
    IPC_EVENT_WPS_SESSION_OVERLAP,
    IPC_EVENT_RSSI_SNR_TRIGGER,
    IPC_EVENT_RSSI_SNR_TRIGGER_0,
	IPC_EVENT_LOGGER,
    IPC_EVENT_NOT_ASSOCIATED,
    IPC_EVENT_BSS_LOSS,
    IPC_EVENT_REASSOCIATION_RESP,
    IPC_EVENT_IMMEDIATE_SCAN_REPORT,
    IPC_EVENT_CONTINUOUS_SCAN_REPORT,
    IPC_EVENT_RSSI_SNR_TRIGGER_1,
    IPC_EVENT_AP_DISCONNECT,
    IPC_EVENT_TX_RETRY_FALIURE,
    IPC_EVENT_RE_AUTH_STARTED,
    IPC_EVENT_RE_AUTH_COMPLETED,
    IPC_EVENT_RE_AUTH_TERMINATED,
    IPC_EVENT_TIMEOUT,
    IPC_EVENT_GWSI,
    IPC_EVENT_MAX
};

enum
{
    DELIVERY_PUSH =0,
    DELIVERY_GET_DATA
};

enum
{
    SCAN_STATUS_COMPLETE = 0,
    SCAN_STATUS_STOPPED = 1,
    SCAN_STATUS_FAILED = 2
};

/************************* IOCTLs Functions *******************************/

TI_HANDLE   IPC_Init(void);

TI_INT32     IPC_DeInit(void);

TI_HANDLE   IPC_DeviceOpen(void* AdapterName); /* get hDevice Handle*/

TI_INT32     IPC_DeviceClose(TI_HANDLE hDevice);

TI_INT32     IPC_DeviceIoControl(TI_HANDLE   hDevice,
                            TI_UINT32    IoControlCode, 
                            void*     pInBuffer,
                            TI_UINT32    InBufferSize,
                            void*     pOutBuffer,
                            TI_UINT32    pOutBufferSize,
                            TI_UINT32*   pBytesReturned);

/************************* Events Functions *******************************/

typedef struct _IPC_EV_DATA * PIPC_EV_DATA;  

typedef TI_INT32 (*TI_EVENT_CALLBACK) (PIPC_EV_DATA  pData);

typedef struct _IPC_EVENT_PARAMS
{
    TI_UINT32            uEventType;
    TI_HANDLE           uEventID;
    TI_UINT32            uProcessID;
    TI_UINT32            uDeliveryType;
    TI_HANDLE           hUserParam;            /* Handle to back reference*/
    TI_EVENT_CALLBACK   pfEventCallback;
}IPC_EVENT_PARAMS;

/* EvParams are assumed to be the first field. Any addtions shoild be made 
    afterwards
 */
typedef struct _IPC_EV_DATA
{
    IPC_EVENT_PARAMS    EvParams;
    TI_UINT32            uBufferSize;
    TI_UINT8             uBuffer[MAX_EVENT_DATA_SIZE];
}IPC_EV_DATA;


/*this function will also enable event and pass all the parameters about it*/
/* returns unique ID of registered event, to be passed later for unregister*/
TI_INT32 IPC_RegisterEvent(TI_HANDLE             hDevice,    /* Driver Handle*/
                          IPC_EVENT_PARAMS*     pEvParams);  /* size of the structure + size of the params*/

TI_INT32 IPC_UnRegisterEvent(TI_HANDLE   hDevice,
                            IPC_EVENT_PARAMS*   pEvParams); /* returned by IPC_RegisterEvent*/

/***************************************************************************/

#ifdef  __cplusplus
}
#endif

#endif /*_IPC_UTIL_H*/

