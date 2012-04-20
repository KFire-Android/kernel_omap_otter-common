/*
 * measurementMgr.c
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
/*    MODULE:   measurementMgr.c                                           */
/*    PURPOSE:  measurement Manager module file                            */
/*                                                                         */
/***************************************************************************/




#define __FILE_ID__  FILE_ID_1
#include "measurementMgr.h"
#include "regulatoryDomainApi.h"
#include "healthMonitor.h"
#include "DrvMainModules.h"
#include "siteMgrApi.h"
#include "TrafficMonitorAPI.h"
#include "smeApi.h"
#include "sme.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCTSMngr.h"
#endif
#include "TWDriver.h"

/* default measurement parameters */
#define MEASUREMENT_CAPABILITIES_NONE                   0x00
#define MEASUREMENT_CAPABILITIES_DOT11H                 0x01
#define MEASUREMENT_CAPABILITIES_XCC_RM                 0x02


#define MEASUREMENT_BEACON_INTERVAL_IN_MICRO_SEC        1024
#define MEASUREMENT_MSEC_IN_MICRO                       1000




/********************************************************************************/
/*                      Internal functions prototypes.                          */
/********************************************************************************/

static void measurementMgr_releaseModule(measurementMgr_t *pMeasurementMgr);

static TI_BOOL measurementMgr_isTrafficIntensityHigherThanThreshold(measurementMgr_t * pMeasurementMgr);

static TI_BOOL  measurementMgr_isRequestValid(TI_HANDLE hMeasurementMgr, MeasurementRequest_t *pRequestArr[], TI_UINT8 numOfRequest);

static TI_BOOL measurementMgrSM_measureInProgress(TI_HANDLE hMeasurementMgr);





/********************************************************************************/
/*                      Interface functions Implementation.                     */
/********************************************************************************/


/**
 * Creates the Measurement Manager moodule.
 * 
 * @param hOs A handle to the OS object.
 *
 * @date 16-Dec-2005
 */
TI_HANDLE measurementMgr_create(TI_HANDLE hOs)
{
    measurementMgr_t * pMeasurementMgr = NULL;
    TI_STATUS status;

    /* allocating the MeasurementMgr object */
    pMeasurementMgr = os_memoryAlloc(hOs, sizeof(measurementMgr_t));

    if (pMeasurementMgr == NULL)
        return NULL;

    os_memoryZero(hOs, pMeasurementMgr, sizeof(measurementMgr_t));
    pMeasurementMgr->hOs = hOs;

    /* creating the Measurement SM */
    status = fsm_Create(pMeasurementMgr->hOs, &(pMeasurementMgr->pMeasurementMgrSm), 
                        MEASUREMENTMGR_NUM_STATES , MEASUREMENTMGR_NUM_EVENTS);
    if(status != TI_OK)
    {
        measurementMgr_releaseModule(pMeasurementMgr);
        return NULL;
    }

    /* creating the sub modules of measurement module */
    
    /* creating Request Handler sub module */
    if( (pMeasurementMgr->hRequestH = requestHandler_create(hOs)) == NULL)
    {
        measurementMgr_releaseModule(pMeasurementMgr);
        return NULL;
    }

    return(pMeasurementMgr);
}





/**
 * Configures the Measurement Manager module.
 * 
 * @param pStadHandles Handles to other modules the Measurement Manager needs.
 * 
 * @date 16-Dec-2005
 */
void measurementMgr_init (TStadHandlesList *pStadHandles)
{
    measurementMgr_t *pMeasurementMgr = (measurementMgr_t *)(pStadHandles->hMeasurementMgr);
    paramInfo_t param;
    
    /* Init Handlers */
    pMeasurementMgr->hRegulatoryDomain  = pStadHandles->hRegulatoryDomain;
    pMeasurementMgr->hXCCMngr           = pStadHandles->hXCCMngr;
    pMeasurementMgr->hSiteMgr           = pStadHandles->hSiteMgr;
    pMeasurementMgr->hTWD               = pStadHandles->hTWD;
    pMeasurementMgr->hMlme              = pStadHandles->hMlmeSm;
    pMeasurementMgr->hTrafficMonitor    = pStadHandles->hTrafficMon;
    pMeasurementMgr->hReport            = pStadHandles->hReport;
    pMeasurementMgr->hOs                = pStadHandles->hOs;
    pMeasurementMgr->hScr               = pStadHandles->hSCR;
    pMeasurementMgr->hApConn            = pStadHandles->hAPConnection;
    pMeasurementMgr->hTxCtrl            = pStadHandles->hTxCtrl;
    pMeasurementMgr->hTimer             = pStadHandles->hTimer;
    pMeasurementMgr->hSme               = pStadHandles->hSme;

    /* initialize variables to default values */
    pMeasurementMgr->Enabled = TI_TRUE;
    pMeasurementMgr->Connected = TI_FALSE;
    pMeasurementMgr->Capabilities = MEASUREMENT_CAPABILITIES_NONE;
    pMeasurementMgr->Mode = MSR_MODE_NONE;

    /* Getting management capability status */
    param.paramType = REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM;
    regulatoryDomain_getParam (pMeasurementMgr->hRegulatoryDomain, &param);
    if (param.content.spectrumManagementEnabled)
    {
        pMeasurementMgr->Capabilities |= MEASUREMENT_CAPABILITIES_DOT11H;
    }
    
    /* Init Functions */
    pMeasurementMgr->parserFrameReq = NULL;
    pMeasurementMgr->isTypeValid = NULL;
    pMeasurementMgr->buildReport = NULL;
    pMeasurementMgr->buildRejectReport = NULL;
    pMeasurementMgr->sendReportAndCleanObj = NULL;
    
    /* initialize variables */  
    pMeasurementMgr->currentState = MEASUREMENTMGR_STATE_IDLE;
    pMeasurementMgr->isModuleRegistered = TI_FALSE;
    pMeasurementMgr->currentFrameType = MSR_FRAME_TYPE_NO_ACTIVE;
    pMeasurementMgr->measuredChannelID = 0;
    pMeasurementMgr->currentNumOfRequestsInParallel = 0;
    pMeasurementMgr->bMeasurementScanExecuted = TI_FALSE;
    
    /* config sub modules */
    RequestHandler_config(pMeasurementMgr->hRequestH, pStadHandles->hReport, pStadHandles->hOs);

    /* Register to the SCR module */
    scr_registerClientCB(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, measurementMgr_scrResponseCB, (TI_HANDLE)pMeasurementMgr);

    measurementMgrSM_config ((TI_HANDLE)pMeasurementMgr);   

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INIT , ": Measurement Manager configured successfully\n");
}


TI_STATUS measurementMgr_SetDefaults (TI_HANDLE hMeasurementMgr, measurementInitParams_t * pMeasurementInitParams)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;
#ifdef XCC_MODULE_INCLUDED
    TI_UINT32 currAC;
#endif

    pMeasurementMgr->trafficIntensityThreshold = pMeasurementInitParams->trafficIntensityThreshold;
    pMeasurementMgr->maxDurationOnNonServingChannel = pMeasurementInitParams->maxDurationOnNonServingChannel;

    /* allocating the measurement Activation Delay timer */
    pMeasurementMgr->hActivationDelayTimer = tmr_CreateTimer (pMeasurementMgr->hTimer);
    if (pMeasurementMgr->hActivationDelayTimer == NULL)
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "measurementMgr_SetDefaults(): Failed to create hActivationDelayTimer!\n");
        return TI_NOK;
    }

#ifdef XCC_MODULE_INCLUDED  
    /* allocating the per AC TS Metrics report timers */
    for (currAC = 0; currAC < MAX_NUM_OF_AC; currAC++)
    {
        pMeasurementMgr->isTsMetricsEnabled[currAC] = TI_FALSE;

        pMeasurementMgr->hTsMetricsReportTimer[currAC] = tmr_CreateTimer (pMeasurementMgr->hTimer);
        if (pMeasurementMgr->hTsMetricsReportTimer[currAC] == NULL)
        {
            TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "measurementMgr_SetDefaults(): Failed to create hTsMetricsReportTimer!\n");
            return TI_NOK;
        }
    }

    /* Check in the Registry if the station supports XCC RM */
    if (pMeasurementInitParams->XCCEnabled == XCC_MODE_ENABLED)
    {
        pMeasurementMgr->Capabilities |= MEASUREMENT_CAPABILITIES_XCC_RM;
    }
#endif

    return TI_OK;
}





/**
 * Sets the specified Measurement Manager parameter.
 *
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * @param pParam The parameter to set.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_setParam(TI_HANDLE hMeasurementMgr, paramInfo_t * pParam)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    switch (pParam->paramType)
    {
        case MEASUREMENT_ENABLE_DISABLE_PARAM:
        {
            TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MEASUREMENT_ENABLE_DISABLE_PARAM <- %d\n", pParam->content.measurementEnableDisableStatus);

            if (pParam->content.measurementEnableDisableStatus)
            {
                measurementMgr_enable(pMeasurementMgr);
            }
            else
            {
                measurementMgr_disable(pMeasurementMgr);
            }

            break;
        }

        case MEASUREMENT_TRAFFIC_THRESHOLD_PARAM:
        {
            if ((pParam->content.measurementTrafficThreshold >= MEASUREMENT_TRAFFIC_THRSHLD_MIN) &&
                (pParam->content.measurementTrafficThreshold <= MEASUREMENT_TRAFFIC_THRSHLD_MAX))
            {
                TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MEASUREMENT_TRAFFIC_THRESHOLD_PARAM <- %d\n", pParam->content.measurementTrafficThreshold);

                pMeasurementMgr->trafficIntensityThreshold = pParam->content.measurementTrafficThreshold;
            }
            else
            {
                TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": Invalid value for MEASUREMENT_TRAFFIC_THRESHOLD_PARAM (%d)\n", pParam->content.measurementTrafficThreshold);
            }
        
            break;
        }

        
        case MEASUREMENT_MAX_DURATION_PARAM:
        {
            TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MEASUREMENT_MAX_DURATION_PARAM <- %d\n", pParam->content.measurementMaxDuration);

            pMeasurementMgr->maxDurationOnNonServingChannel = pParam->content.measurementMaxDuration;

            break;
        }
        

        default:
        {
            TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": Specified parameter is not supported (%d)\n", pParam->paramType);

            return PARAM_NOT_SUPPORTED;
        }

    }
    
    return TI_OK;
}





/**
 * Gets the specified parameter from the Measurement Manager.
 *
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * @param pParam The parameter to get.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_getParam(TI_HANDLE hMeasurementMgr, paramInfo_t * pParam)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    switch(pParam->paramType)
    {

        case MEASUREMENT_GET_STATUS_PARAM:
        {
            WLAN_OS_REPORT(("%s: \n\n", __FUNCTION__));
            WLAN_OS_REPORT(("MeasurementMgr Status Report:\n\n"));

            WLAN_OS_REPORT(("Current State: %d\n\n", pMeasurementMgr->currentState));

            WLAN_OS_REPORT(("Connected: %d\n", pMeasurementMgr->Connected));
            WLAN_OS_REPORT(("Enabled: %d\n\n", pMeasurementMgr->Enabled));

            WLAN_OS_REPORT(("Mode: %d\n", pMeasurementMgr->Mode));
            WLAN_OS_REPORT(("Capabilities: %d\n\n", pMeasurementMgr->Capabilities));

            WLAN_OS_REPORT(("current Frame Type: %d\n", pMeasurementMgr->currentFrameType));
            WLAN_OS_REPORT(("Measured Channel: %d\n", pMeasurementMgr->measuredChannelID));
            WLAN_OS_REPORT(("Serving Channel: %d\n", pMeasurementMgr->servingChannelID));
            WLAN_OS_REPORT(("Traffic Intensity Threshold: %d\n", pMeasurementMgr->trafficIntensityThreshold));
            WLAN_OS_REPORT(("Max Duration on Nonserving Channel: %d\n", pMeasurementMgr->maxDurationOnNonServingChannel));

            break;
        }
        

        default:
        {
            TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": Specified parameter is not supported (%d)\n", pParam->paramType);

            return PARAM_NOT_SUPPORTED;
        }

    }

    return TI_OK;
}






/**
 * Signals the Measurement Manager that the STA is connected.
 *
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_connected(TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    /* checking if measurement is enabled */
    if (pMeasurementMgr->Mode == MSR_MODE_NONE)
        return TI_OK;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr set to connected.\n");

    return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                               MEASUREMENTMGR_EVENT_CONNECTED, pMeasurementMgr);
}





/**
 * Signals the Measurement Manager that the STA is disconnected.
 *
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_disconnected(TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr set to disconnected.\n");

    return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                                MEASUREMENTMGR_EVENT_DISCONNECTED, pMeasurementMgr);
}




/**
 * Enables the Measurement Manager module.
 * 
 * @date 10-Jan-2006
 */
TI_STATUS measurementMgr_enable(TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr set to enabled.\n");

    return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                                MEASUREMENTMGR_EVENT_ENABLE, pMeasurementMgr);
}





/**
 * Disables the Measurement Manager module.
 * 
 * @date 10-Jan-2006
 */
TI_STATUS measurementMgr_disable(TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr set to disabled.\n");

    return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                                MEASUREMENTMGR_EVENT_DISABLE, pMeasurementMgr);
}





/**
 * Destroys the Measurement Manager module.
 * 
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_destroy(TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t *pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    if (pMeasurementMgr == NULL)
        return TI_OK;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr is being destroyed\n");

    measurementMgr_releaseModule (pMeasurementMgr);

    return TI_OK;
}






/**
 * Sets the Measurement Mode.
 * 
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * @param capabilities The AP capabilities.
 * @param pIeBuffer Pointer to the list of IEs.
 * @param length Length of the IE list.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_setMeasurementMode(TI_HANDLE hMeasurementMgr, TI_UINT16 capabilities, 
                                         TI_UINT8 * pIeBuffer, TI_UINT16 length)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    /*
     * 11h Measurement is not supported in the current version.
     */
/*  if( (pMeasurementMgr->Capabilities & MEASUREMENT_CAPABILITIES_DOT11H) &&
        (capabilities & DOT11_SPECTRUM_MANAGEMENT) )
    {
        pMeasurementMgr->Mode = MSR_MODE_SPECTRUM_MANAGEMENT;
    }
    else
    {
*/
#ifdef XCC_MODULE_INCLUDED

        if(pMeasurementMgr->Capabilities & MEASUREMENT_CAPABILITIES_XCC_RM)
        {
                    pMeasurementMgr->Mode = MSR_MODE_XCC;
        }
        else
#endif
        {
            pMeasurementMgr->Mode = MSR_MODE_NONE;
        }


    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr mode changed to: %d\n", pMeasurementMgr->Mode);
    
    return TI_OK;
}






/**
 * Called when a frame with type measurement request is received.
 * 
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * @param frameType The frame type.
 * @param dataLen The length of the frame.
 * @param pData A pointer to the frame's content.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_receiveFrameRequest(TI_HANDLE hMeasurementMgr,
                                             EMeasurementFrameType frameType,
                                             TI_INT32 dataLen,
                                             TI_UINT8 * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;

    TMeasurementFrameRequest * frame = &(pMeasurementMgr->newFrameRequest);    
    TI_UINT16 currentFrameToken;
    
    /* checking if measurement is enabled */
    if (pMeasurementMgr->Mode == MSR_MODE_NONE)
        return TI_NOK;

    /* ignore broadcast/multicast request if unicast request is active */
    if (frameType != MSR_FRAME_TYPE_UNICAST && pMeasurementMgr->currentFrameType == MSR_FRAME_TYPE_UNICAST)
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Broadcast/Multicast measurement frame has been ignored\n");

        return TI_NOK;
    }

    /* ignore broadcast request if multicast request is active */
    if (frameType == MSR_FRAME_TYPE_BROADCAST && pMeasurementMgr->currentFrameType == MSR_FRAME_TYPE_MULTICAST)
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Broadcast measurement frame has been ignored\n");
        
        return TI_NOK;
    }

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Measurement frame received\n");

    /* Parsing the Frame Request Header */
    pMeasurementMgr->parserFrameReq(hMeasurementMgr, pData, dataLen, 
                                        frame);

    frame->frameType = frameType;

    /* checking if the received token frame is the same as the one that is being processed */
    if ((requestHandler_getFrameToken(pMeasurementMgr->hRequestH, &currentFrameToken) == TI_OK)
        && (currentFrameToken == frame->hdr->dialogToken))
    {
        os_memoryZero(pMeasurementMgr->hOs, &pMeasurementMgr->newFrameRequest, 
                      sizeof(TMeasurementFrameRequest));

        TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Measurement frame token %d is identical to current frame token - ignoring frame\n", currentFrameToken);

        return TI_NOK;
    }

    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Measurement frame token is %d\n", frame->hdr->dialogToken);

    /* Frame is Received for processing */
    return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                               MEASUREMENTMGR_EVENT_FRAME_RECV, pMeasurementMgr);
}





/**
 * Activates the next measurement request.
 * 
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @date 16-Dec-2005
 */
TI_STATUS measurementMgr_activateNextRequest(TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;
    requestHandler_t * pRequestH = (requestHandler_t *) pMeasurementMgr->hRequestH;
    MeasurementRequest_t * pRequestArr[MAX_NUM_REQ];
    TI_UINT8 numOfRequestsInParallel = 0;
    TI_BOOL valid;
    TI_UINT8 index;

	/* Keep note of the time we started processing the request. this will be used */
    /* to give the measurementSRV a time frame to perform the measurement operation */
    pMeasurementMgr->currentRequestStartTime = os_timeStampMs(pMeasurementMgr->hOs);

    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Timer started at %d, we have 20ms to begin measurement...\n", pMeasurementMgr->currentRequestStartTime);

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Looking for a valid request\n");

    do
    {
        TI_STATUS status;

        if (numOfRequestsInParallel != 0)
        {
            TRACE4(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Changing activeRequestID from %d to %d, and numOfWaitingRequests from %d to %d.\n", pRequestH->activeRequestID, pRequestH->activeRequestID + numOfRequestsInParallel, pRequestH->numOfWaitingRequests, pRequestH->numOfWaitingRequests - numOfRequestsInParallel);
        }

        pRequestH->activeRequestID += numOfRequestsInParallel;
        pRequestH->numOfWaitingRequests -= numOfRequestsInParallel;

        for (index = 0; index < MAX_NUM_REQ; index++)
        {
            pRequestArr[index] = NULL;
        }
        numOfRequestsInParallel = 0;

        /* Getting the next request/requests from the request handler */
        status = requestHandler_getNextReq(pMeasurementMgr->hRequestH, TI_FALSE, pRequestArr, 
                                           &numOfRequestsInParallel);
        
        /* Checking if there are no waiting requests */
        if (status != TI_OK)
        {
            TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": There are no waiting requests in the queue\n");

            return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                               MEASUREMENTMGR_EVENT_SEND_REPORT, pMeasurementMgr);
        }

        /* Checking validity of request/s */
        valid = measurementMgr_isRequestValid(pMeasurementMgr, pRequestArr, 
                                numOfRequestsInParallel);

        /* Checking if the current request is Beacon Table */
        if( (numOfRequestsInParallel == 1) && 
            (pRequestArr[0]->Type == MSR_TYPE_BEACON_MEASUREMENT) &&
            (pRequestArr[0]->ScanMode == MSR_SCAN_MODE_BEACON_TABLE) )
        {
            TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Received Beacon Table request, building a report for it and continuing\n");

            pMeasurementMgr->buildReport(hMeasurementMgr, *(pRequestArr[0]), NULL);
            valid = TI_FALSE; /* In order to get the next request/s*/
        }
        
    } while (valid == TI_FALSE);
    
    
    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Request(s) for activation:\n");

    for (index = 0; index < numOfRequestsInParallel; index++)
    {
        TRACE6(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": \n\nRequest #%d:\n Type: %d\n Measured Channel: %d (Serving Channel: %d)\n Scan Mode: %d\n Duration: %d\n\n", index+1, pRequestArr[index]->Type, pRequestArr[index]->channelNumber, pMeasurementMgr->servingChannelID, pRequestArr[index]->ScanMode, pRequestArr[index]->DurationTime);
    }

    /* Ignore requests if traffic intensity is high */
    if (measurementMgr_isTrafficIntensityHigherThanThreshold(pMeasurementMgr) == TI_TRUE)
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Traffic intensity too high, giving up...\n");

        measurementMgr_rejectPendingRequests(pMeasurementMgr, MSR_REJECT_TRAFFIC_INTENSITY_TOO_HIGH);

        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                               MEASUREMENTMGR_EVENT_SEND_REPORT, pMeasurementMgr);
    }

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Request is Valid, about to start\n");
    
    pMeasurementMgr->measuredChannelID = pRequestArr[0]->channelNumber;
  
    /* Request resource from the SCR */
    return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
        MEASUREMENTMGR_EVENT_REQUEST_SCR, pMeasurementMgr);    
}   



void measurementMgr_rejectPendingRequests(TI_HANDLE hMeasurementMgr, EMeasurementRejectReason rejectReason)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;
    requestHandler_t * pRequestH = (requestHandler_t *) pMeasurementMgr->hRequestH;
    MeasurementRequest_t * pRequestArr[MAX_NUM_REQ];
    TI_UINT8 numOfRequestsInParallel;

    /* reject all pending measurement requests */
    while (requestHandler_getNextReq(pMeasurementMgr->hRequestH, TI_TRUE, 
                pRequestArr, &numOfRequestsInParallel) == TI_OK)
    {
        TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Rejecting pending request (activeRequestID = %d)...\n", pRequestH->activeRequestID);

        pMeasurementMgr->buildRejectReport(pMeasurementMgr, pRequestArr,
                numOfRequestsInParallel, rejectReason);

        pRequestH->activeRequestID += numOfRequestsInParallel;
    }
}





/********************************************************************************/
/*                      Callback functions Implementation.                      */
/********************************************************************************/


/**
 * The callback called by the MeasurementSRV module when then
 * measurement operation has ended.
 * 
 * @param clientObj A handle to the Measurement Manager module.
 * @param msrReply An array of replies sent by the MeasurementSRV module,
 * where each reply contains the result of a single measurement request.
 * 
 * @date 01-Jan-2006
 */
void measurementMgr_MeasurementCompleteCB(TI_HANDLE clientObj, TMeasurementReply * msrReply)
{
    measurementMgr_t    *pMeasurementMgr = (measurementMgr_t *) clientObj;
    TI_UINT8            index;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Building reports for measurement requests\n");

    /* build a report for each measurement request/reply pair */
    for (index = 0; index < msrReply->numberOfTypes; index++)
    {
        pMeasurementMgr->buildReport(pMeasurementMgr, *(pMeasurementMgr->currentRequest[index]), &msrReply->msrTypes[index]);
    }

    measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
            MEASUREMENTMGR_EVENT_COMPLETE, pMeasurementMgr);
}


/**
 * The callback called when the SCR responds to the SCR request.
 * 
 * @param hClient A handle to the Measurement Manager module.
 * @param requestStatus The request's status
 * @param eResource The resource for which the CB is issued
 * @param pendReason The reason of a PEND status.
 * 
 * @date 01-Jan-2006
 */
void measurementMgr_scrResponseCB(TI_HANDLE hClient, EScrClientRequestStatus requestStatus,
                                  EScrResourceId eResource, EScePendReason pendReason )
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hClient;
    measurementMgrSM_Events event;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": SCR callback entered\n");

    /* If the SM is in a state where it waits for the CB, status of RUN */
    /* results in the SM asking the measurementSRV to start measurement; */
    /* otherwise we got an ABORT or a PEND reason worse than the one we */
    /* got when calling the SCR, so the SM aborts the measurement */
    if (pMeasurementMgr->currentState == MEASUREMENTMGR_STATE_WAITING_FOR_SCR)
    {
        if (requestStatus == SCR_CRS_RUN)
        {
            TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Received SCR status RUN, running...\n");

            event = MEASUREMENTMGR_EVENT_SCR_RUN;
        }
        else
        {
            if (requestStatus == SCR_CRS_PEND)
            {
                TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Received SCR status PEND with reason %d, aborting...\n", pendReason);
            }
            else
            {
                TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Received SCR status ABORT/FW_RESET, aborting...\n");
            }

            event = MEASUREMENTMGR_EVENT_ABORT;
        }
    }
    else
    {   
        /* This can only occur if FW reset occurs or when higher priority client is running. */

        if (requestStatus == SCR_CRS_FW_RESET)
        {
            TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Received SCR status FW_RESET\n");

            event = MEASUREMENTMGR_EVENT_FW_RESET;
        }
        else
        {
        TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgrSM current state is %d (which isn't WAITING_FOR_SCR), aborting...\n", pMeasurementMgr->currentState);

        event = MEASUREMENTMGR_EVENT_ABORT;
    }
    }

    measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
        event, pMeasurementMgr);
}






/**
 * The callback called by the MLME.
 * 
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @date 01-Jan-2006
 */
void measurementMgr_mlmeResultCB(TI_HANDLE hMeasurementMgr, TMacAddr * bssid, mlmeFrameInfo_t * frameInfo, 
                                 TRxAttr * pRxAttr, TI_UINT8 * buffer, TI_UINT16 bufferLength)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;
    TScanFrameInfo      tScanFrameInfo;

	if (measurementMgrSM_measureInProgress(pMeasurementMgr) == TI_FALSE)
	{
		TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION , "measurementMgr_mlmeResultCB: measurement not in progress, return\n");
		return;
	}

	
	/* erroneous frames are notifed to the measurmenet manager to update counter 
    (add counter sometimes in the future) Look at: scanCncn_ScanCompleteNotificationCB and
    scanCncn_MlmeResultCB */
    if (NULL == bssid)
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION , "measurementMgr_mlmeResultCB: received an empty frame notification from MLME\n");
        return;
    }

    if (pMeasurementMgr == NULL || pRxAttr == NULL)
    {
		if(pMeasurementMgr != NULL)
		{
			TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": MLME callback called with NULL object\n");
		}
        return;
    }

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MLME callback entered\n");

    /* build the scan frame info object */
    tScanFrameInfo.bssId = bssid;
    tScanFrameInfo.band = (ERadioBand)pRxAttr->band;
    tScanFrameInfo.channel = pRxAttr->channel;
    tScanFrameInfo.parsedIEs = frameInfo;
    tScanFrameInfo.rate = pRxAttr->Rate;
    tScanFrameInfo.rssi = pRxAttr->Rssi;
    tScanFrameInfo.snr = pRxAttr->SNR;
    tScanFrameInfo.staTSF = pRxAttr->TimeStamp;
    tScanFrameInfo.buffer = buffer;
    tScanFrameInfo.bufferLength = bufferLength;

    /* update the driver (SME) result table */
    sme_MeansurementScanResult (pMeasurementMgr->hSme, SCAN_CRS_RECEIVED_FRAME, &tScanFrameInfo);

    TRACE8(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MLME Frame: Subtype = %d, MAC = %x-%x-%x-%x-%x-%x, RSSI = %d\n", frameInfo->subType, (*bssid)[0], (*bssid)[1], (*bssid)[2], (*bssid)[3], (*bssid)[4], (*bssid)[5], pRxAttr->Rssi);
}


/********************************************************************************/
/*                      Internal functions Implementation.                      */
/********************************************************************************/


/**
 * Releases the module's allocated objects according to the given init vector.
 * 
 * @param pMeasurementMgr A handle to the Measurement Manager module.
 * @param initVec The init vector with a bit set for each allocated object.
 * 
 * @date 01-Jan-2006
 */
static void measurementMgr_releaseModule (measurementMgr_t * pMeasurementMgr)
{
#ifdef XCC_MODULE_INCLUDED
    TI_UINT32 currAC;
#endif

    if (pMeasurementMgr->hActivationDelayTimer)
    {
        tmr_DestroyTimer (pMeasurementMgr->hActivationDelayTimer);
    }

#ifdef XCC_MODULE_INCLUDED
    for (currAC = 0; currAC < MAX_NUM_OF_AC; currAC++)
    {
        if (pMeasurementMgr->hTsMetricsReportTimer[currAC])
        {
            tmr_DestroyTimer (pMeasurementMgr->hTsMetricsReportTimer[currAC]);
        }
    }
#endif

    if (pMeasurementMgr->pMeasurementMgrSm)
    {
        fsm_Unload(pMeasurementMgr->hOs, pMeasurementMgr->pMeasurementMgrSm);
    }

    if (pMeasurementMgr->hRequestH)
    {
        requestHandler_destroy(pMeasurementMgr->hRequestH);
    }
    
    os_memoryFree(pMeasurementMgr->hOs, pMeasurementMgr, sizeof(measurementMgr_t));
}



/**
 * Checks whether the traffic intensity, i.e. number of packets per seconds, is higher
 * than the preconfigured threshold.
 * 
 * @param pMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @return True iff the traffic intensity is high
 * 
 * @date 01-Jan-2006
 */
static TI_BOOL measurementMgr_isTrafficIntensityHigherThanThreshold(measurementMgr_t * pMeasurementMgr)
{
    TI_BOOL trafficIntensityHigh = TI_FALSE;
    int pcksPerSec;

    pcksPerSec = TrafficMonitor_GetFrameBandwidth(pMeasurementMgr->hTrafficMonitor);

    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": pcksPerSec = %d\n", pcksPerSec);
    
    if (pcksPerSec >= pMeasurementMgr->trafficIntensityThreshold)
        trafficIntensityHigh = TI_TRUE;
    
    return trafficIntensityHigh;
}




/**
 * Checks whether the given measurement request is valid.
 * 
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * @param pRequestArr The measurement request.
 * @param numOfRequest Number of type requests
 * 
 * @return True iff the request is valid
 * 
 * @date 01-Jan-2006
 */
static TI_BOOL  measurementMgr_isRequestValid(TI_HANDLE hMeasurementMgr, MeasurementRequest_t *pRequestArr[],
                           TI_UINT8 numOfRequest)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;
    TI_UINT8 requestIndex;
    paramInfo_t param;

    /* Checking validity of the measured channel number */
    param.content.channel = pRequestArr[0]->channelNumber;
    param.paramType = REGULATORY_DOMAIN_IS_CHANNEL_SUPPORTED;
    regulatoryDomain_getParam(pMeasurementMgr->hRegulatoryDomain, &param);
    if ( !param.content.bIsChannelSupprted  )
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Request rejected due to invalid channel\n");

        if (pMeasurementMgr->currentFrameType == MSR_FRAME_TYPE_UNICAST)
            pMeasurementMgr->buildRejectReport(pMeasurementMgr, pRequestArr, numOfRequest, 
                                    MSR_REJECT_INVALID_CHANNEL);

        return TI_FALSE;
    }
    else
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Request channel is Valid\n");
    }
    
    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Starting to check each request:\n");

    /* Check Validity of each request */
    for (requestIndex = 0; requestIndex < numOfRequest; requestIndex++)
    {
        TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Checking request #%d:\n", requestIndex+1);

        /* Checking validity of the Request Type */
        if (pMeasurementMgr->isTypeValid(hMeasurementMgr, pRequestArr[requestIndex]->Type, 
                pRequestArr[requestIndex]->ScanMode) == TI_FALSE)
        {
            TRACE2(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Request rejected due to invalid measurement type of request #%d (type = %d)\n", requestIndex+1, pRequestArr[requestIndex]->Type);
            
            if(pMeasurementMgr->currentFrameType == MSR_FRAME_TYPE_UNICAST)
                pMeasurementMgr->buildRejectReport(pMeasurementMgr, pRequestArr, numOfRequest, 
                                        MSR_REJECT_INVALID_MEASUREMENT_TYPE);

            return TI_FALSE;
        }
        else
        {
            TRACE2(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Measurement type of request #%d is supported (type = %d)\n", requestIndex+1, pRequestArr[requestIndex]->Type);
        }

        /* For measurement types different than Beacon Table */
        if ((pRequestArr[requestIndex]->Type != MSR_TYPE_BEACON_MEASUREMENT) || 
            (pRequestArr[requestIndex]->ScanMode != MSR_SCAN_MODE_BEACON_TABLE))
        {
            /* Checking Measurement request's duration only when request is on a non-serving channel */
            if (pMeasurementMgr->servingChannelID != pRequestArr[requestIndex]->channelNumber)
            {
                TI_UINT8 dtimPeriod;
                TI_UINT32 beaconInterval;
                TI_UINT32 dtimDuration;


                /* Checking duration doesn't exceed given max duration */
                if (pRequestArr[requestIndex]->DurationTime > pMeasurementMgr->maxDurationOnNonServingChannel)
                {
                    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Request #%d rejected because duration exceeds maximum duration\n", requestIndex+1);

                    TRACE2(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Duration = %d, MaxDurationOnNonServingChannel = %d\n", pRequestArr[requestIndex]->DurationTime, pMeasurementMgr->maxDurationOnNonServingChannel);
                
                    if (pMeasurementMgr->currentFrameType == MSR_FRAME_TYPE_UNICAST)
                        pMeasurementMgr->buildRejectReport(pMeasurementMgr, pRequestArr, numOfRequest, 
                                MSR_REJECT_DURATION_EXCEED_MAX_DURATION);
                
                    return TI_FALSE;
                }
                else
                {
                    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Duration of request #%d doesn't exceed max duration\n", requestIndex+1);
                }


                /* Checking DTIM */

                /* Getting the DTIM count */
                param.paramType = SITE_MGR_DTIM_PERIOD_PARAM;
                siteMgr_getParam(pMeasurementMgr->hSiteMgr, &param);
                dtimPeriod = param.content.siteMgrDtimPeriod;

                /* Getting the beacon Interval */
                param.paramType = SITE_MGR_BEACON_INTERVAL_PARAM;
                siteMgr_getParam(pMeasurementMgr->hSiteMgr, &param);
                beaconInterval = param.content.beaconInterval;

                dtimDuration = beaconInterval * MEASUREMENT_BEACON_INTERVAL_IN_MICRO_SEC/MEASUREMENT_MSEC_IN_MICRO*dtimPeriod;
                if (pRequestArr[requestIndex]->DurationTime > dtimDuration)
                {
                    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_WARNING, ": Request rejected due to DTIM overlap of request #%d\n", requestIndex+1);
                                    
                    TRACE2(pMeasurementMgr->hReport, REPORT_SEVERITY_WARNING, ": Duration = %d, DTIM Duration = %d\n", pRequestArr[requestIndex]->DurationTime, dtimDuration);
        
                    if (pMeasurementMgr->currentFrameType == MSR_FRAME_TYPE_UNICAST)
                        pMeasurementMgr->buildRejectReport(pMeasurementMgr, pRequestArr, numOfRequest, 
                                                MSR_REJECT_DTIM_OVERLAP);

                    return TI_FALSE;
                }
                else
                {
                    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": DTIM of request #%d doesn't overlap\n", requestIndex+1);
                }
            }
        }
    }

    return TI_TRUE;
}

static TI_BOOL measurementMgrSM_measureInProgress(TI_HANDLE hMeasurementMgr)
{
	measurementMgr_t * pMeasurementMgr = (measurementMgr_t *)hMeasurementMgr;

	if (pMeasurementMgr->currentState == MEASUREMENTMGR_STATE_MEASURING)
		return TI_TRUE;

	else
		return TI_FALSE;
}


