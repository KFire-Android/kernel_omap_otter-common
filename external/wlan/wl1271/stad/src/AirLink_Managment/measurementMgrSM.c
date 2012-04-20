/*
 * measurementMgrSM.c
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


/**************************************************************************/
/*																		  */
/*		MODULE:		measurementMgrSM.c									  */
/*		PURPOSE:	Measurement Manager State Machine module interface.   */
/*																		  */
/**************************************************************************/



#define __FILE_ID__  FILE_ID_2
#include "measurementMgrSM.h"
#include "measurementMgr.h"
#include "802_11Defs.h"
#ifdef XCC_MODULE_INCLUDED
 #include "XCCMngr.h"
 #include "XCCRMMngr.h"
#endif
#include "spectrumMngmntMgr.h"
#include "siteMgrApi.h"
#include "MacServices_api.h"
#include "regulatoryDomainApi.h"
#include "TWDriver.h"
#include "timer.h"
#include "sme.h"


char * measurementMgr_stateDesc[MEASUREMENTMGR_NUM_STATES] =
{  
	"STATE_IDLE",
    "STATE_PROCESSING_REQUEST",
    "STATE_WAITING_FOR_SCR",
    "STATE_MEASURING"
};

	
char * measurementMgr_eventDesc[MEASUREMENTMGR_NUM_EVENTS] =
{
	"EVENT_CONNECTED",
	"EVENT_DISCONNECTED",
	"EVENT_ENABLE",
	"EVENT_DISABLE",
    "EVENT_FRAME_RECV",
    "EVENT_SEND_REPORT",
	"EVENT_REQUEST_SCR",
    "EVENT_SCR_WAIT",
    "EVENT_SCR_RUN",
    "EVENT_ABORT",
    "EVENT_COMPLETE",
    "EVENT_FW_RESET"
};




/********************************************************************************/
/*						MeasurementMgr SM Action Prototypes						*/
/********************************************************************************/

static TI_STATUS measurementMgrSM_acUnexpected(void * pData);

static TI_STATUS measurementMgrSM_acNop(void * pData);


static TI_STATUS measurementMgrSM_acConnected(void * pData);

static TI_STATUS measurementMgrSM_acDisconnected_fromIdle(void * pData);

static TI_STATUS measurementMgrSM_acEnable(void * pData);

static TI_STATUS measurementMgrSM_acDisable_fromIdle(void * pData);

static TI_STATUS measurementMgrSM_acFrameReceived_fromIdle(void * pData);

static TI_STATUS measurementMgrSM_acSendReportAndCleanObj(void * pData);


static TI_STATUS measurementMgrSM_acDisconnected_fromProcessingRequest(void * pData);

static TI_STATUS measurementMgrSM_acDisable_fromProcessingRequest(void * pData);

static TI_STATUS measurementMgrSM_acFrameReceived_fromProcessingRequest(void * pData);

static TI_STATUS measurementMgrSM_acAbort_fromProcessingRequest(void * pData);

static TI_STATUS measurementMgrSM_acRequestSCR(void * pData);


static TI_STATUS measurementMgrSM_acDisconnected_fromWaitForSCR(void * pData);

static TI_STATUS measurementMgrSM_acDisable_fromWaitForSCR(void * pData);

static TI_STATUS measurementMgrSM_acFrameReceived_fromWaitForSCR(void * pData);

static TI_STATUS measurementMgrSM_acAbort_fromWaitForSCR(void * pData);

static TI_STATUS measurementMgrSM_acStartMeasurement(void * pData);


static TI_STATUS measurementMgrSM_acDisconnected_fromMeasuring(void * pData);

static TI_STATUS measurementMgrSM_acDisable_fromMeasuring(void * pData);

static TI_STATUS measurementMgrSM_acFrameReceived_fromMeasuring(void * pData);

static TI_STATUS measurementMgrSM_acAbort_fromMeasuring(void * pData);

static TI_STATUS measurementMgrSM_acMeasurementComplete(void * pData);

static TI_STATUS measurementMgrSM_acFirmwareReset(void * pData);








/********************************************************************************/
/*						Internal Functions Prototypes							*/
/********************************************************************************/

static void measurementMgrSM_resetParams(measurementMgr_t * pMeasurementMgr);

static void	measurementMgrSM_uponActivationDelayTimeout (TI_HANDLE hMeasurementMgr, TI_BOOL bTwdInitOccured);







/********************************************************************************/
/*						MeasurementMgr SM General Use Functions					*/
/********************************************************************************/


/**
 * Configures the Measurement Manager State Machine.
 * 
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @date 01-Jan-2006
 */
TI_STATUS measurementMgrSM_config(TI_HANDLE hMeasurementMgr)
{
	measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;
    TI_STATUS status;

    /* MeasurementMgr State Machine matrix */
	fsm_actionCell_t measurementMgr_matrix[MEASUREMENTMGR_NUM_STATES][MEASUREMENTMGR_NUM_EVENTS] =
	{
		/* next state and actions for STATE_IDLE state */    
		{
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acConnected},				/* CONNECTED         */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisconnected_fromIdle},	/* DISCONNECTED      */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acEnable},					/* ENABLE            */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisable_fromIdle},		/* DISABLE           */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acFrameReceived_fromIdle},	/* FRAME_RECV        */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acSendReportAndCleanObj},	/* SEND_REPORT       */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acUnexpected},				/* REQUEST_SCR       */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acUnexpected},				/* SCR_WAIT          */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acUnexpected},				/* SCR_RUN           */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acUnexpected},				/* ABORT             */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acUnexpected},				/* COMPLETE          */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acUnexpected}				/* FW_RESET          */
		},

		/* next state and actions for STATE_PROCESSING_REQUEST state */    
		{
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acUnexpected},			/* CONNECTED         */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisconnected_fromProcessingRequest},	/* DISCONNECTED      */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acNop},					/* ENABLE            */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisable_fromProcessingRequest},		/* DISABLE           */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acFrameReceived_fromProcessingRequest},	/* FRAME_RECV        */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acSendReportAndCleanObj},				/* SEND_REPORT       */
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acRequestSCR},				/* REQUEST_SCR       */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acUnexpected},			/* SCR_WAIT          */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acUnexpected},			/* SCR_RUN           */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acAbort_fromProcessingRequest},		/* ABORT             */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acUnexpected},			/* COMPLETE          */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acUnexpected}			/* FW_RESET          */
		},

		/* next state and actions for STATE_WAITING_FOR_SCR state */    
		{
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acUnexpected},						/* CONNECTED         */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisconnected_fromWaitForSCR},				/* DISCONNECTED      */
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acNop},								/* ENABLE            */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisable_fromWaitForSCR},						/* DISABLE           */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acFrameReceived_fromWaitForSCR},	/* FRAME_RECV        */
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acUnexpected},						/* SEND_REPORT       */
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acUnexpected},						/* REQUEST_SCR       */
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acNop},								/* SCR_WAIT          */
			{MEASUREMENTMGR_STATE_MEASURING, measurementMgrSM_acStartMeasurement},						/* SCR_RUN           */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acAbort_fromWaitForSCR},						/* ABORT             */
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acUnexpected},						/* COMPLETE          */
			{MEASUREMENTMGR_STATE_WAITING_FOR_SCR, measurementMgrSM_acUnexpected}						/* FW_RESET          */
		},

		/* next state and actions for STATE_MEASURING state */    
		{
			{MEASUREMENTMGR_STATE_MEASURING, measurementMgrSM_acUnexpected},					/* CONNECTED         */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisconnected_fromMeasuring},			/* DISCONNECTED      */
			{MEASUREMENTMGR_STATE_MEASURING, measurementMgrSM_acNop},							/* ENABLE            */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acDisable_fromMeasuring},				/* DISABLE           */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acFrameReceived_fromMeasuring},	/* FRAME_RECV        */
			{MEASUREMENTMGR_STATE_MEASURING, measurementMgrSM_acUnexpected},					/* SEND_REPORT       */
			{MEASUREMENTMGR_STATE_MEASURING, measurementMgrSM_acUnexpected},					/* REQUEST_SCR       */
			{MEASUREMENTMGR_STATE_MEASURING, measurementMgrSM_acUnexpected},					/* SCR_WAIT          */
			{MEASUREMENTMGR_STATE_MEASURING, measurementMgrSM_acUnexpected},					/* SCR_RUN           */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acAbort_fromMeasuring},				/* ABORT             */
			{MEASUREMENTMGR_STATE_PROCESSING_REQUEST, measurementMgrSM_acMeasurementComplete},	/* COMPLETE          */
			{MEASUREMENTMGR_STATE_IDLE, measurementMgrSM_acFirmwareReset}						/* FW_RESET          */
		}

	};

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Configured MeasurementMgr state machine\n");
	
	status = fsm_Config(pMeasurementMgr->pMeasurementMgrSm, 
						&measurementMgr_matrix[0][0], 
						MEASUREMENTMGR_NUM_STATES, 
						MEASUREMENTMGR_NUM_EVENTS, 
						measurementMgrSM_event, pMeasurementMgr->hOs);

	return status;
}



/**
 * Raises a State Machine event in the Measurement Manager SM.
 * 
 * @param currentState A point to the member holding the SM's current state.
 * @param event The event we want to raise.
 * @param hMeasurementMgr A handle to the Measurement Manager module.
 * 
 * @date 05-Jan-2006
 */
TI_STATUS measurementMgrSM_event(TI_UINT8 * currentState, TI_UINT8 event, TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) hMeasurementMgr;
	TI_STATUS status;
	TI_UINT8 nextState;

	status = fsm_GetNextState(pMeasurementMgr->pMeasurementMgrSm, 
								*currentState, event, &nextState);

	if (status != TI_OK)
	{
		TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": State machine error, failed getting next state\n");

		return(TI_NOK);
	}

	TRACE3(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, "measurementMgrSM_event: <currentState = %d, event = %d> --> nextState = %d\n", currentState, event, nextState);

	status = fsm_Event(pMeasurementMgr->pMeasurementMgrSm, currentState, event, (void *) pMeasurementMgr);

	return status;
}







/********************************************************************************/
/*					MeasurementMgr SM Action Functions							*/
/********************************************************************************/


/********************************************************************************/
/*                            IDLE State Actions                                */
/********************************************************************************/

/**
 * Performs the required action when the Measurement Manager module has
 * been advised that the station has connected to an AP.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acConnected(void * pData)
{
	measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;
	paramInfo_t param;

#ifdef XCC_MODULE_INCLUDED
	iappParsingRegistrationTable_t iappParsingRegistration;
#endif

	/* do nothing if we're already in connected mode */
	if (pMeasurementMgr->Connected)
	{
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Connected flag already set\n");

		return TI_OK;
	}

	pMeasurementMgr->Connected = TI_TRUE;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Connected flag has been set\n");

    /* upon connection to a new AP set the measurment scan flag to FALSE */
    pMeasurementMgr->bMeasurementScanExecuted = TI_FALSE;

	/* get the current serving channel */
	param.paramType = SITE_MGR_CURRENT_CHANNEL_PARAM;
	siteMgr_getParam(pMeasurementMgr->hSiteMgr, &param);
	pMeasurementMgr->servingChannelID = param.content.siteMgrCurrentChannel;
	    
#ifdef XCC_MODULE_INCLUDED
	if(pMeasurementMgr->Mode == MSR_MODE_XCC)
	{
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr set to XCC mode\n");

        if(pMeasurementMgr->isModuleRegistered == TI_FALSE)
        {
            /* Registering to the XCCMngr */
            iappParsingRegistration.handler = pMeasurementMgr;
            iappParsingRegistration.iappParsingRegistrationProcedure = measurementMgr_XCCParse;

            if (XCCMngr_registerForRecvIappPacket(pMeasurementMgr->hXCCMngr,
				iappParsingRegistration, IAPP_RADIO_MEASUREMENT) != TI_OK)
            {
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_WARNING, ": Could not register to receive IAPP packets\n");

                return TI_NOK;
            }

            pMeasurementMgr->isModuleRegistered = TI_TRUE;
        }
        
        pMeasurementMgr->parserFrameReq = measurementMgr_XCCParseFrameReq;
        pMeasurementMgr->isTypeValid = measurementMgr_XCCIsTypeValid;
		pMeasurementMgr->buildReport = measurementMgr_XCCBuildReport;
		pMeasurementMgr->buildRejectReport = measurementMgr_XCCBuildRejectReport;
		pMeasurementMgr->sendReportAndCleanObj = measurementMgr_XCCSendReportAndCleanObject;
        requestHandler_setRequestParserFunction(pMeasurementMgr->hRequestH, 
                                                measurementMgr_XCCParseRequestIEHdr);
	}
	else
#endif
	{
		if(pMeasurementMgr->Mode == MSR_MODE_SPECTRUM_MANAGEMENT)
		{
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": MeasurementMgr set to Spectrum Management mode\n");

            /* NOTE: These 5 functions need to be corrected to fit the 802.11h standered */
            pMeasurementMgr->parserFrameReq = measurementMgr_dot11hParseFrameReq;
            pMeasurementMgr->isTypeValid = measurementMgr_dot11hIsTypeValid;
			pMeasurementMgr->buildReport = measurementMgr_dot11hBuildReport;
			pMeasurementMgr->buildRejectReport = measurementMgr_dot11hBuildRejectReport;
			pMeasurementMgr->sendReportAndCleanObj = measurementMgr_dot11hSendReportAndCleanObject;
            requestHandler_setRequestParserFunction(pMeasurementMgr->hRequestH, 
                                                    measurementMgr_dot11hParseRequestIEHdr);

		}
	}

	return TI_OK;
}



/**
 * Called when the Measurement Manager has been advised that the station
 * has disconnected from the AP.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acDisconnected_fromIdle(void * pData)
{
	measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Connected flag unset\n");

	pMeasurementMgr->Connected = TI_FALSE;

	return TI_OK;
}



/**
 * Called when the Measurement Manager is enabled.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acEnable(void * pData)
{
	measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Measurement Manager has been enabled\n");

	pMeasurementMgr->Enabled = TI_TRUE;

	return TI_OK;
}



/**
 * Called when the Measurement Manager is disabled.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acDisable_fromIdle(void * pData)
{
	measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Measurement Manager has been disabled\n");

	pMeasurementMgr->Enabled = TI_FALSE;

	return TI_OK;
}



/**
 * Called when the SM is in an idle state and we receive a new measurement frame.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acFrameReceived_fromIdle(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;
    TI_UINT16 activationDelay;
    TI_STATUS status;
    paramInfo_t param;
    TI_UINT16 tbtt;

	/* handle frame request only if we're connected and measurement is enabled */
	if (pMeasurementMgr->Connected == TI_FALSE ||
		pMeasurementMgr->Enabled == TI_FALSE)
	{
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": Frame received while SM is in disconnected/disabled state\n");

        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                               MEASUREMENTMGR_EVENT_ABORT, pMeasurementMgr);
	}

	/* Setting the frame Type */
	pMeasurementMgr->currentFrameType = pMeasurementMgr->newFrameRequest.frameType;

TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Frame Type = %d\n", pMeasurementMgr->currentFrameType);

    /* Getting the Beacon Interval from the Site Mgr */
    param.paramType = SITE_MGR_BEACON_INTERVAL_PARAM;
    status = siteMgr_getParam(pMeasurementMgr->hSiteMgr, &param);
    if (status != TI_OK)
    {
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": Failed to retrieve beacon interval - not connected?\n");

        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                               MEASUREMENTMGR_EVENT_ABORT, pMeasurementMgr);
    }

	/* converting beacon interval to msec */
    tbtt = (param.content.beaconInterval * 1024) / 1000;	/* from TU to msec */   

	/* Initializing Activation Delay Time */
	activationDelay	= pMeasurementMgr->newFrameRequest.hdr->activatioDelay;
	activationDelay	*= tbtt;    
    /* Adding the Measurement Offset to the activation delay */
	activationDelay	+= pMeasurementMgr->newFrameRequest.hdr->measurementOffset;

    /* Inserting all received measurement requests into the queue */
	status = requestHandler_insertRequests(pMeasurementMgr->hRequestH, 
                                           pMeasurementMgr->Mode, 
								           pMeasurementMgr->newFrameRequest);

    /* Clean New Frame Params */
    os_memoryZero(pMeasurementMgr->hOs, &pMeasurementMgr->newFrameRequest, 
                      sizeof(TMeasurementFrameRequest));

    if (status != TI_OK)
    {
        pMeasurementMgr->currentFrameType = MSR_FRAME_TYPE_NO_ACTIVE;

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": Could not insert request into the queue\n");

        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
                               MEASUREMENTMGR_EVENT_ABORT, pMeasurementMgr);
    }

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": New frame has been inserted into the queue\n");

	/* If frame type isn't Unicast add to Activation Delay a random delay */
	if ((pMeasurementMgr->currentFrameType != MSR_FRAME_TYPE_UNICAST) && (activationDelay > 0))
	{
		activationDelay	+= ((os_timeStampMs(pMeasurementMgr->hOs) % MSR_ACTIVATION_DELAY_RANDOM)
								+ MSR_ACTIVATION_DELAY_OFFSET);
	}

    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Activation Delay in ms = %d\n", activationDelay);

	if (activationDelay > 0)
	{
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Going to wait for activation delay timer callback\n");

		/* Starting the Activation Delay Timer */
        tmr_StartTimer (pMeasurementMgr->hActivationDelayTimer,
                        measurementMgrSM_uponActivationDelayTimeout,
                        (TI_HANDLE)pMeasurementMgr,
                        activationDelay,
                        TI_FALSE);

		return TI_OK;
	}
	else
	{
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Activating the next request immediately without waiting for callback\n");

		/* Calling to schedule the first waiting request */
		return measurementMgr_activateNextRequest(pData);
	}
}





/********************************************************************************/
/*                      PROCESSING_REQUEST State Actions                        */
/********************************************************************************/

/**
 * Called when the station disconnects from the AP while processing
 * a measurement request.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acDisconnected_fromProcessingRequest(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    /* Stopping the activationDelay Timer */
    tmr_StopTimer (pMeasurementMgr->hActivationDelayTimer);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
           	
	pMeasurementMgr->Connected = TI_FALSE;

	return TI_OK;
}



/**
 * Called when the Measurement Manager module has been disable while
 * processing a measurement request.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acDisable_fromProcessingRequest(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    /* Stopping the activationDelay Timer */
    tmr_StopTimer (pMeasurementMgr->hActivationDelayTimer);

    /* Clear Measurement fields  */
    measurementMgrSM_resetParams(pMeasurementMgr);

	pMeasurementMgr->Enabled = TI_FALSE;

    return TI_OK;
}



/**
 * Called when a frame has been received while we are processing another frame.
 * In this case the older frame is discarded and the new frame is processed.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acFrameReceived_fromProcessingRequest(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    /* Stopping the activationDelay Timer */
    tmr_StopTimer (pMeasurementMgr->hActivationDelayTimer);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
           	
	/* Process New Frame */
	return measurementMgrSM_acFrameReceived_fromIdle(pData);
}



/**
 * Sends measurement reports to the AP and cleans up the module.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acSendReportAndCleanObj(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Sending pending reports and cleaning up...\n");

    return pMeasurementMgr->sendReportAndCleanObj(pData);
}



/**
 * Called when for some reason we abort while processing a request.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acAbort_fromProcessingRequest(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Entered\n");

    /* Stopping the activationDelay Timer */
    tmr_StopTimer (pMeasurementMgr->hActivationDelayTimer);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
    
    return TI_OK;
}



/**
 * Called when we finished processing a request and want to request the SCR.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acRequestSCR(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;
    EScrClientRequestStatus scrStatus;
    EScePendReason scrPendReason;

	/* Request the channel */
    scrStatus = scr_clientRequest(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, 
                                  SCR_RESOURCE_SERVING_CHANNEL, &scrPendReason);
	
    if (scrStatus == SCR_CRS_RUN)
    {	
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Received RUN response from SCR\n");

		/* The channel is allocated for the measurement */
        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
				MEASUREMENTMGR_EVENT_SCR_RUN, pMeasurementMgr);    
    }
    else if ((scrStatus == SCR_CRS_PEND) && (scrPendReason == SCR_PR_DIFFERENT_GROUP_RUNNING))
    {	
TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Received PEND/DIFFGROUP response from SCR\n");

		/* No need to wait for the channel allocation */
        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
				MEASUREMENTMGR_EVENT_ABORT, pMeasurementMgr);  
    }

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Going to wait for SCR callback...\n");

	/* In all other cases wait for the callback function to be called */
    return TI_OK;
}





/********************************************************************************/
/*                        WAIT_FOR_SCR State Actions                            */
/********************************************************************************/


/**
 * Called if the station disconnects from the AP while waiting for a
 * response from the SCR.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acDisconnected_fromWaitForSCR(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* Release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);

	pMeasurementMgr->Connected = TI_FALSE;

    return TI_OK;
}



/**
 * Called if the Measurement Manager module is disabled while we are
 * waiting for a response from the SCR.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acDisable_fromWaitForSCR(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* Release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);

	pMeasurementMgr->Enabled = TI_FALSE;

    return TI_OK;
}



/**
 * Called if a frame is received after we requested the SCR for another frame. 
 * In this case the older frame is discarded and the new frame is processed.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acFrameReceived_fromWaitForSCR(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* Release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);

	/* Process New Frame */
    return measurementMgrSM_acFrameReceived_fromIdle(pData);
}



/**
 * Called if the SCR callbacked with a response other than RUN.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acAbort_fromWaitForSCR(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* Release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

	/* Build a reject report */
	measurementMgr_rejectPendingRequests(pMeasurementMgr, MSR_REJECT_SCR_UNAVAILABLE);

	/* Clear Measurement fields */
    pMeasurementMgr->sendReportAndCleanObj(pMeasurementMgr);

    return TI_OK;
}



/**
 * Called when the SCR callbacks with a RUN response or if the SCR
 * returned a RUN response when we requested it.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acStartMeasurement(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

	/* Cryptic: the first struct is the requestHandler request while */
	/* the second one is the measurementSRV request */
    MeasurementRequest_t * pRequestArr[MAX_NUM_REQ];
	TMeasurementRequest request;
    paramInfo_t	*pParam;
    TI_UINT8 numOfRequestsInParallel;
    TI_UINT8 requestIndex;
	TI_UINT32 timePassed;
	TI_BOOL requestedBeaconMeasurement= TI_FALSE;
	TI_STATUS status;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Starting Measurement operation\n");

    pParam = (paramInfo_t *)os_memoryAlloc(pMeasurementMgr->hOs, sizeof(paramInfo_t));
    if (!pParam)
    {
        return TI_NOK;
    }

	request.channel = pMeasurementMgr->measuredChannelID;
	request.startTime = 0;	/* ignored by MeasurementSRV for now - for .11k */
	request.numberOfTypes = 0;

    TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Measured Channel = %d\n", pMeasurementMgr->measuredChannelID);

	pParam->paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
	pParam->content.channelCapabilityReq.channelNum = pMeasurementMgr->measuredChannelID;
	pParam->content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;

	if (pMeasurementMgr->measuredChannelID <= MAX_CHANNEL_IN_BAND_2_4)
	{
		request.band = RADIO_BAND_2_4_GHZ;
		pParam->content.channelCapabilityReq.band = RADIO_BAND_2_4_GHZ;
	}
	else
	{
		request.band = RADIO_BAND_5_0_GHZ;
		pParam->content.channelCapabilityReq.band = RADIO_BAND_5_0_GHZ;
	}

	regulatoryDomain_getParam(pMeasurementMgr->hRegulatoryDomain, pParam);
	
    request.txPowerDbm = pParam->content.channelCapabilityRet.maxTxPowerDbm;

    request.eTag = SCAN_RESULT_TAG_MEASUREMENT;
    os_memoryFree(pMeasurementMgr->hOs, pParam, sizeof(paramInfo_t));

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Querying Request Handler for the next request in the queue\n");

    /* Getting the next request/requests from the request handler */
    status = requestHandler_getNextReq(pMeasurementMgr->hRequestH, TI_TRUE, pRequestArr,
        &numOfRequestsInParallel);

	if (status != TI_OK)
	{	
        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, ": Failed getting next request from Request Handler\n");
        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
				MEASUREMENTMGR_EVENT_COMPLETE, pMeasurementMgr);  
	}
	
	/* Save the number of requests in parallel so that once the */
	/* measurement operation ends we can get rid of this amount of requests */
	/* from the requestHandler */
	pMeasurementMgr->currentNumOfRequestsInParallel = numOfRequestsInParallel;

	for (requestIndex = 0; requestIndex < numOfRequestsInParallel; requestIndex++)
	{
        if (pRequestArr[requestIndex]->Type == MSR_TYPE_BEACON_MEASUREMENT)
        {
			requestedBeaconMeasurement = TI_TRUE;

			if (pRequestArr[requestIndex]->ScanMode == MSR_SCAN_MODE_BEACON_TABLE)
			{
                TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Beacon Table request encountered, building report now\n");

				/* building Report for beacon table request */
				pMeasurementMgr->buildReport(pMeasurementMgr, *pRequestArr[requestIndex], NULL);

				continue;
			}
        }

        /* save the request so we can reference it when results arrive */
        pMeasurementMgr->currentRequest[request.numberOfTypes] = pRequestArr[requestIndex];

        /* add the measurement type to the request's list */
		request.msrTypes[request.numberOfTypes].duration = pRequestArr[requestIndex]->DurationTime;
		request.msrTypes[request.numberOfTypes].scanMode = pRequestArr[requestIndex]->ScanMode;
		request.msrTypes[request.numberOfTypes].msrType = pRequestArr[requestIndex]->Type;

        TRACE3(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ":\n\nMeasurement Request #%d Details: Type = %d, Duration = %d\n\n",						request.numberOfTypes+1,						request.msrTypes[request.numberOfTypes].msrType,						request.msrTypes[request.numberOfTypes].duration);

		request.numberOfTypes++;
	}

	if (requestedBeaconMeasurement == TI_TRUE)
	{
        /* build a probe request template and send it to the HAL */
        TSetTemplate templateStruct;
		probeReqTemplate_t probeReqTemplate;
		TSsid broadcastSSID;

 		templateStruct.ptr = (TI_UINT8 *) &probeReqTemplate;
		templateStruct.type = PROBE_REQUEST_TEMPLATE;
        templateStruct.eBand = request.band;
        templateStruct.uRateMask = RATE_MASK_UNSPECIFIED;
		broadcastSSID.len = 0;

        TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Sending probe request template...\n");

        buildProbeReqTemplate( pMeasurementMgr->hSiteMgr, &templateStruct, &broadcastSSID, request.band );
#ifdef XCC_MODULE_INCLUDED
  		{	/* Insert Radio Mngt Capability IE according XCC4*/
  			TI_UINT32				len = 0;
  			measurementMgr_radioMngtCapabilityBuild (pMeasurementMgr, 
                                                     templateStruct.ptr + templateStruct.len, 
                                                     (TI_UINT8*)&len);
  			templateStruct.len += len;
  		}
#endif

		TWD_CmdTemplate (pMeasurementMgr->hTWD, &templateStruct, NULL, NULL);
	}

	/* Check if the maximum time to wait for the measurement request to */
	/* finish has already passed */
	timePassed = os_timeStampMs(pMeasurementMgr->hOs) - pMeasurementMgr->currentRequestStartTime;
	if (timePassed > MSR_START_MAX_DELAY)
	{
        TRACE2(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Maximum delay to perform measurement operation has passed (%d / %d)\n",						MSR_START_MAX_DELAY, (os_timeStampMs(pMeasurementMgr->hOs) - pMeasurementMgr->currentRequestStartTime));

		pMeasurementMgr->buildRejectReport(pMeasurementMgr, pRequestArr, numOfRequestsInParallel, MSR_REJECT_MAX_DELAY_PASSED);
        return measurementMgrSM_event((TI_UINT8 *) &(pMeasurementMgr->currentState), 
				MEASUREMENTMGR_EVENT_COMPLETE, pMeasurementMgr);  
	}

    /* set the measurement scan executed flag to TRUE */
    pMeasurementMgr->bMeasurementScanExecuted = TI_TRUE;

	/* Yalla, start measuring */
    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Finished preparing request. Handing over to MeasurementSRV...\n");

	TWD_StartMeasurement (pMeasurementMgr->hTWD,
                               &request, 
                               MSR_START_MAX_DELAY - timePassed,
                               NULL, NULL,
                               measurementMgr_MeasurementCompleteCB, 
                               pMeasurementMgr);
	return TI_OK;
}






/********************************************************************************/
/*                          MEASURING State Actions                             */
/********************************************************************************/


static TI_STATUS measurementMgrSM_acDisconnected_fromMeasuring(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
		
	pMeasurementMgr->Connected = TI_FALSE;

	return TI_OK;
}



static TI_STATUS measurementMgrSM_acDisable_fromMeasuring(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
		
	pMeasurementMgr->Enabled = TI_FALSE;

    return TI_OK;    
}



static TI_STATUS measurementMgrSM_acFrameReceived_fromMeasuring(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
           	
	/* Process New Frame */
	return measurementMgrSM_acFrameReceived_fromIdle(pData);
}



static TI_STATUS measurementMgrSM_acAbort_fromMeasuring(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
		
    TWD_StopMeasurement (pMeasurementMgr->hTWD, TI_TRUE ,NULL, NULL);
    
    return TI_OK;
}



/**
 * Called when we finished a measurement request.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acMeasurementComplete(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;
	requestHandler_t * pRequestH = (requestHandler_t *) pMeasurementMgr->hRequestH;

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Completing measurement operation and resuming normal behavior\n");

	/* advance the activeRequestID variable to get rid of the */
	/* measurement requests we've already executed */
TRACE2(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Increasing activeRequestID from %d to %d.\n", pRequestH->activeRequestID, pRequestH->activeRequestID + pMeasurementMgr->currentNumOfRequestsInParallel);

	pRequestH->activeRequestID += pMeasurementMgr->currentNumOfRequestsInParallel;

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* move the driver result table to stable state and clear it */
    sme_MeansurementScanResult (pMeasurementMgr->hSme, SCAN_CRS_SCAN_COMPLETE_OK, NULL);

    /* release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

	/* Process New Frame */
	return measurementMgr_activateNextRequest(pData);
}



/**
 * Called when a firmware reset has been detected.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acFirmwareReset(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Firmware Reset!!\n");

    setDefaultProbeReqTemplate (pMeasurementMgr->hSiteMgr);

    /* release the SCR */
    scr_clientComplete(pMeasurementMgr->hScr, SCR_CID_XCC_MEASURE, SCR_RESOURCE_SERVING_CHANNEL);

    /* Clear Measurement fields */
    measurementMgrSM_resetParams(pMeasurementMgr);
	
    return TI_OK;
}







/********************************************************************************/
/*						Miscellaneous State Actions								*/
/********************************************************************************/

/**
 * Called when an unexpected event has been triggered.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acUnexpected(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Entered when state is \n");

	return TI_OK;
}

/**
 * A do nothing action.
 * 
 * @date 05-Jan-2006
 */
static TI_STATUS measurementMgrSM_acNop(void * pData)
{
    measurementMgr_t * pMeasurementMgr = (measurementMgr_t *) pData;

    TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Entered when state is \n");

	return TI_OK;
}








/********************************************************************************/
/*						Internal Functions Prototypes							*/
/********************************************************************************/


static void measurementMgrSM_resetParams(measurementMgr_t *pMeasurementMgr)
{  
	/* clear the waiting requests */
	requestHandler_clearRequests(pMeasurementMgr->hRequestH);	

	/* clearing reports data base */
#ifdef XCC_MODULE_INCLUDED
	os_memoryZero(pMeasurementMgr->hOs,&(pMeasurementMgr->XCCFrameReport),
			sizeof(RM_report_frame_t));
#endif
    os_memoryZero(pMeasurementMgr->hOs,&(pMeasurementMgr->dot11hFrameReport),
			sizeof(MeasurementReportFrame_t));

	pMeasurementMgr->frameLength = 0;
	pMeasurementMgr->nextEmptySpaceInReport = 0;
	pMeasurementMgr->measuredChannelID = 0;
	pMeasurementMgr->currentFrameType = MSR_FRAME_TYPE_NO_ACTIVE;
}



/**
 * The callback called when the activation delay timer has ended.
 * 
 * @param hMeasurementMgr - A handle to the Measurement Manager module.
 * @param bTwdInitOccured -   Indicates if TWDriver recovery occured since timer started 
 * 
 * @date 01-Jan-2006
 */
static void	measurementMgrSM_uponActivationDelayTimeout (TI_HANDLE hMeasurementMgr, TI_BOOL bTwdInitOccured)
{
	measurementMgr_t * pMeasurementMgr = (measurementMgr_t *)hMeasurementMgr;

TRACE0(pMeasurementMgr->hReport, REPORT_SEVERITY_INFORMATION, ": Activation delay timeout callback entered\n");

    measurementMgr_activateNextRequest (pMeasurementMgr);
}

