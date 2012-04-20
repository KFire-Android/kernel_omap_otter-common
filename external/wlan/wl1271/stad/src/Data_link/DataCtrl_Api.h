/*
 * DataCtrl_Api.h
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
/*                                                                         */
/*                                                                         */
/*      PURPOSE:    DataCtrl module api functions header file              */
/*                                                                         */
/***************************************************************************/

#ifndef _DATA_CTRL_API_H_
#define _DATA_CTRL_API_H_

#include "paramOut.h"
#include "rxXfer_api.h"
#include "802_11Defs.h"
#include "GeneralUtilApi.h"
#include "DrvMainModules.h"

/* Include all core Tx modules APIs */
#include "txCtrl_Api.h"
#include "txPort_Api.h"
#include "txDataQueue_Api.h"
#include "txMgmtQueue_Api.h"


typedef enum
{
    RX_DATA_EAPOL_DESTINATION_PARAM = 0x01,
    RX_DATA_PORT_STATUS_PARAM       = 0x02

} ERxDataParam;


/*  RX MODULE   */
/*--------------*/

/* Rx module interface functions */

#define RECV_OK                  0x1
#define DIRECTED_BYTES_RECV      0x2
#define DIRECTED_FRAMES_RECV     0x4
#define MULTICAST_BYTES_RECV     0x8
#define MULTICAST_FRAMES_RECV    0x10   
#define BROADCAST_BYTES_RECV     0x20    
#define BROADCAST_FRAMES_RECV    0x40

#define NO_RX_NOTIFICATION  0x0

#define ALL_RCV_FRAME (DIRECTED_FRAMES_RECV|MULTICAST_FRAMES_RECV|BROADCAST_FRAMES_RECV)

#define  MAX_RX_NOTIF_REQ_ELMENTS 8


/*TI_HANDLE rxData_create (msduReceiveCB_t* msduReceiveCB, TI_HANDLE hOs);  */
TI_HANDLE rxData_create (TI_HANDLE hOs);    

void      rxData_init (TStadHandlesList *pStadHandles);

TI_STATUS rxData_SetDefaults (TI_HANDLE hRxData, rxDataInitParams_t * rxDataInitParams);
                         
void      rxData_receivePacketFromWlan (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);

TI_STATUS rxData_stop(TI_HANDLE hRxData);

TI_STATUS rxData_unLoad(TI_HANDLE hRxData); 

TI_STATUS ctrlData_getParamProtType(TI_HANDLE hCtrlData, erpProtectionType_e *protType);

TI_STATUS ctrlData_getParamPreamble(TI_HANDLE hCtrlData, EPreamble *preamble);

TI_STATUS ctrlData_getParamBssid(TI_HANDLE hCtrlData, EInternalParam paramVal, TMacAddr bssid);

TI_STATUS rxData_getParam(TI_HANDLE hRxData, paramInfo_t *pParamInfo);  

TI_STATUS rxData_setParam(TI_HANDLE hRxData, paramInfo_t *pParamInfo);  

TI_STATUS rxData_getTiwlnCounters(TI_HANDLE hRxData, TIWLN_COUNTERS *pTiwlnCounters);

void      rxData_resetCounters(TI_HANDLE hRxData);

TI_HANDLE rxData_RegNotif(TI_HANDLE hRxData,TI_UINT16 EventMask,GeneralEventCall_t CallBack,TI_HANDLE context,TI_UINT32 Cookie);

TI_STATUS rxData_UnRegNotif(TI_HANDLE hRxData,TI_HANDLE RegEventHandle);

TI_STATUS rxData_AddToNotifMask(TI_HANDLE hRxData,TI_HANDLE Notifh,TI_UINT16 EventMask);

void rxData_SetReAuthInProgress(TI_HANDLE hRxData, TI_BOOL	value);

TI_BOOL rxData_IsReAuthInProgress(TI_HANDLE hRxData);

void rxData_StopReAuthActiveTimer(TI_HANDLE hRxData);
void rxData_ReauthDisablePriority(TI_HANDLE hRxData);

/* debug functions */
void rxData_resetDbgCounters(TI_HANDLE hRxData);
void rxData_printRxBlock(TI_HANDLE hRxData);
void rxData_printRxCounters(TI_HANDLE hRxData);
void rxData_startRxThroughputTimer(TI_HANDLE hRxData); 
void rxData_stopRxThroughputTimer(TI_HANDLE hRxData); 
void rxData_printRxDataFilter(TI_HANDLE hRxData);



/* CONTROL MODULE */
/*----------------*/

#define XFER_OK                 0x1
#define DIRECTED_BYTES_XFER     0x2
#define DIRECTED_FRAMES_XFER    0x4
#define MULTICAST_BYTES_XFER    0x8
#define MULTICAST_FRAMES_XFER   0x10
#define BROADCAST_BYTES_XFER    0x20
#define BROADCAST_FRAMES_XFER   0x40

#define  MAX_TX_NOTIF_REQ_ELMENTS 8


typedef struct
{
    TMacAddr    ctrlDataDeviceMacAddress; 
} ctrlDataConfig_t; 

/* retries for the next link test packet Callback */
typedef void (*retriesCB_t)(TI_HANDLE handle, TI_UINT8 ackFailures);

/*******************************/
/* Control module interface functions */
TI_HANDLE ctrlData_create(TI_HANDLE hOs);

void      ctrlData_init (TStadHandlesList *pStadHandles,                       
                         retriesCB_t       retriesUpdateCBFunc,
                         TI_HANDLE         retriesUpdateCBObj);

TI_STATUS ctrlData_SetDefaults (TI_HANDLE hCtrlData, ctrlDataInitParams_t *ctrlDataInitParams);

TI_STATUS ctrlData_unLoad(TI_HANDLE hCtrlData); 

TI_STATUS ctrlData_getParam(TI_HANDLE hCtrlData, paramInfo_t *pParamInfo);  

TI_STATUS ctrlData_setParam(TI_HANDLE hCtrlData, paramInfo_t *pParamInfo);  

TI_STATUS ctrlData_stop(TI_HANDLE hCtrlData);

TI_STATUS ctrlData_getTiwlnCounters(TI_HANDLE hCtrlData, TIWLN_COUNTERS *pTiwlnCounters);   

void ctrlData_updateTxRateAttributes(TI_HANDLE hCtrlData);	

void ctrlData_getCurrBssTypeAndCurrBssId(TI_HANDLE hCtrlData, TMacAddr *pCurrBssid, 
                                         ScanBssType_e *pCurrBssType);  

void ctrlData_txCompleteStatus(TI_HANDLE hCtrlData, TxResultDescriptor_t *pTxResultInfo,
							   EHwRateBitFiled HwTxRequestRate, TI_UINT8 txPktFlags);

void ctrlData_ToggleTrafficIntensityNotification (TI_HANDLE hCtrlData, TI_BOOL enabledFlag);


/* dbg functions */
/*---------------*/
#ifdef TI_DBG
void ctrlData_printTxParameters(TI_HANDLE hCtrlData);
void ctrlData_printCtrlBlock(TI_HANDLE hCtrlData);
#endif /* TI_DBG */


#endif  /* _DATA_CTRL_API_H_ */

