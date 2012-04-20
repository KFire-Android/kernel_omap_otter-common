/*
 * measurementMgrApi.h
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
/*    MODULE:   measurementMgrApi.h                                        */
/*    PURPOSE:  Measurement Manager module interface header file           */
/*                                                                         */
/***************************************************************************/





#ifndef __MEASUREMENTMGR_API_H__
#define __MEASUREMENTMGR_API_H__


#include "scrApi.h"
#include "mlmeApi.h"
#include "DrvMainModules.h"


typedef enum
{
    MEASUREMENT_TRAFFIC_THRESHOLD_PARAM = 0x01,
    MEASUREMENT_GET_STATUS_PARAM        = 0x02

} EMeasurementParam;


TI_HANDLE measurementMgr_create(TI_HANDLE hOs);

void      measurementMgr_init (TStadHandlesList *pStadHandles);

TI_STATUS measurementMgr_SetDefaults (TI_HANDLE hMeasurementMgr, measurementInitParams_t * pMeasurementInitParams);

TI_STATUS measurementMgr_destroy(TI_HANDLE hMeasurementMgr);

TI_STATUS measurementMgr_setParam(TI_HANDLE hMeasurementMgr, paramInfo_t * pParam);

TI_STATUS measurementMgr_getParam(TI_HANDLE hMeasurementMgr, paramInfo_t * pParam);

TI_STATUS measurementMgr_connected(TI_HANDLE hMeasurementMgr);

TI_STATUS measurementMgr_disconnected(TI_HANDLE hMeasurementMgr);

TI_STATUS measurementMgr_enable(TI_HANDLE hMeasurementMgr);

TI_STATUS measurementMgr_disable(TI_HANDLE hMeasurementMgr);

TI_STATUS measurementMgr_setMeasurementMode(TI_HANDLE hMeasurementMgr, TI_UINT16 capabilities, 
                                            TI_UINT8 * pIeBuffer, TI_UINT16 length);

TI_STATUS measurementMgr_receiveFrameRequest(TI_HANDLE hMeasurementMgr, EMeasurementFrameType frameType,
                                            TI_INT32 dataLen, TI_UINT8 * pData);

void measurementMgr_rejectPendingRequests(TI_HANDLE hMeasurementMgr, EMeasurementRejectReason rejectReason);

void measurementMgr_MeasurementCompleteCB(TI_HANDLE clientObj, TMeasurementReply * msrReply);

void measurementMgr_scrResponseCB(TI_HANDLE hClient, EScrClientRequestStatus requestStatus,
                                  EScrResourceId eResource, EScePendReason pendReason);

void measurementMgr_mlmeResultCB(TI_HANDLE hMeasurementMgr, TMacAddr * bssid, mlmeFrameInfo_t * frameInfo, 
                                 TRxAttr * pRxAttr, TI_UINT8 * buffer, TI_UINT16 bufferLength);


#endif  /* __MEASUREMENTMGR_API_H__ */
