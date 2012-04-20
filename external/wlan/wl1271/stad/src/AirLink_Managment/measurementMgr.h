/*
 * measurementMgr.h
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


/***************************************************************************/
/*                                                                         */
/*    MODULE:   measurementMgr.h                                           */
/*    PURPOSE:  measurement Manager module header file                     */
/*                                                                         */
/***************************************************************************/




#ifndef __MEASUREMENTMGR_H__
#define __MEASUREMENTMGR_H__


#include "report.h"
#include "timer.h"
#include "paramOut.h"
#include "requestHandler.h"
#include "measurementMgrSM.h"
#ifdef XCC_MODULE_INCLUDED
 #include "XCCRMMngrParam.h"
#endif


/* Functions Pointers Definitions */
typedef TI_STATUS (*parserFrameReq_t)   (TI_HANDLE hMeasurementMgr, 
                                         TI_UINT8 *pData, TI_INT32 dataLen,
                                         TMeasurementFrameRequest *frameReq);

typedef TI_BOOL (*isTypeValid_t)        (TI_HANDLE hMeasurementMgr, 
                                         EMeasurementType type, 
                                         EMeasurementScanMode scanMode);

typedef TI_STATUS (*buildRejectReport_t) (TI_HANDLE hMeasurementMgr,
                                          MeasurementRequest_t *pRequestArr[],
                                          TI_UINT8  numOfRequestsInParallel,
                                          EMeasurementRejectReason rejectReason);

typedef TI_STATUS (*buildReport_t)      (TI_HANDLE hMeasurementMgr,
                                         MeasurementRequest_t request,
                                         TMeasurementTypeReply * reply);

typedef TI_STATUS (*sendReportAndCleanObj_t)(TI_HANDLE hMeasurementMgr);



typedef struct 
{

    /* Timers */
    void *                      hActivationDelayTimer;


    /* Methods */
    parserFrameReq_t            parserFrameReq;
    isTypeValid_t               isTypeValid;
    buildRejectReport_t         buildRejectReport;
    buildReport_t               buildReport;
    sendReportAndCleanObj_t     sendReportAndCleanObj;


    /* Data */
    TI_BOOL                     Enabled;
    TI_BOOL                     Connected;

    TI_UINT8                    servingChannelID;
    TI_UINT8                    measuredChannelID;

    EMeasurementMode            Mode;   
    TI_UINT8                    Capabilities;
    TI_BOOL                     isModuleRegistered;

    TI_UINT16                   trafficIntensityThreshold;
    TI_UINT16                   maxDurationOnNonServingChannel;

    TI_BOOL                     bMeasurementScanExecuted; /* flag indicating if measurment scan was 
                                                      executed by AP after the last connection */

    /* State Machine Params */
    fsm_stateMachine_t *        pMeasurementMgrSm;
    measurementMgrSM_States     currentState;

    
    /* Report Frame Params */
#ifdef XCC_MODULE_INCLUDED
    RM_report_frame_t           XCCFrameReport;
#endif
    MeasurementReportFrame_t    dot11hFrameReport;
    TI_UINT16                   nextEmptySpaceInReport;
    TI_UINT16                   frameLength;


    /* Request Frame Params */
    MeasurementRequest_t *      currentRequest[MAX_NUM_REQ];
    TI_UINT8                    currentNumOfRequestsInParallel;
    EMeasurementFrameType       currentFrameType;
    TI_UINT32                   currentRequestStartTime;
    TMeasurementFrameRequest    newFrameRequest;


    /* XCC Traffic Stream Metrics measurement parameters */
    TI_HANDLE                   hTsMetricsReportTimer[MAX_NUM_OF_AC];
    TI_BOOL                     isTsMetricsEnabled[MAX_NUM_OF_AC];

    /* Handles to other modules */
    TI_HANDLE                   hRequestH;
    TI_HANDLE                   hRegulatoryDomain;
    TI_HANDLE                   hXCCMngr;
    TI_HANDLE                   hSiteMgr;
    TI_HANDLE                   hTWD;
    TI_HANDLE                   hMlme;
    TI_HANDLE                   hTrafficMonitor;
    TI_HANDLE                   hReport;
    TI_HANDLE                   hOs;
    TI_HANDLE                   hScr;
    TI_HANDLE                   hApConn;
    TI_HANDLE                   hTxCtrl;
    TI_HANDLE                   hTimer;
    TI_HANDLE                   hSme;
} measurementMgr_t;




TI_STATUS measurementMgr_activateNextRequest(TI_HANDLE pContext);




#endif /* __MEASUREMENTMGR_H__*/

