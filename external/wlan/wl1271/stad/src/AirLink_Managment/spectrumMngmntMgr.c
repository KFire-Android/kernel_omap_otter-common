/*
 * spectrumMngmntMgr.c
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

/** \file spectrumMngmntMgr.c
 *  
 *
 *  \see spectrumMngmntMgr.h
 */

/****************************************************************************************************/
/*																									*/
/*		MODULE:		spectrumMngmntMgr.c																*/
/*		PURPOSE:	                                            									*/
/*																						 			*/
/****************************************************************************************************/

#define __FILE_ID__  FILE_ID_6
#include "report.h"
#include "osApi.h"
#include "siteMgrApi.h"
#include "regulatoryDomainApi.h"
#include "mlmeBuilder.h"
#include "Ctrl.h"
#include "spectrumMngmntMgr.h"

#define RADAR_THRESHOLD_IN_PRECENTS		  (5)
#define DOT11H_REQUEST_IE_HDR_LEN          3
#define DOT11H_REQUEST_IE_LEN              7
#define DOT11_MEASUREMENT_REQUEST_ELE_ID (38)


/********************************************************************************/
/*						Internal functions prototypes.							*/
/********************************************************************************/

/* The following function uses features from the old Measurement module. */
/* It will have to be adapted to using the new Measurement Manager. */
#if 0

static void buildMapSubFieldForBasicReport(TI_HANDLE hMeasurementMgr, TI_UINT8* map);

#endif


/********************************************************************************/
/*						Internal Structures.        							*/
/********************************************************************************/
typedef struct
{
    TI_UINT8   dialogToken;
    TI_UINT8   activatioDelay;
    TI_UINT8   measurementOffset;
} dot11hFrameReqHdr_t;

typedef struct
{
    TI_UINT8  IeId;
    TI_UINT8  Length;
    TI_UINT8  Token;
} dot11hReqIEHdr_t;

/********************************************************************************/
/*						Interface functions Implementation.						*/
/********************************************************************************/

/***************************************************************************
 * NOTE: The next 3 functions represent the following situations: Receiving
 *       of TPC request, receving Quite IE and receiving Switch Channel IE.
 *       The Measurement SM should be updated to handle this states.
 ***************************************************************************/


/***********************************************************************
 *                   measurementMgr_getBasicMeasurementParam									
 ***********************************************************************
DESCRIPTION:	
				
				
INPUT:      hMeasurementMgr	    -	MeasurementMgr Handle
			
OUTPUT:		pAcxStatisitics     -   
            pMediumOccupancy    -   

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/

/* The following function uses features from the old Measurement module. */
/* It will have to be adapted to using the new Measurement Manager. */
#if 0

TI_STATUS measurementMgr_getBasicMeasurementParam(TI_HANDLE hMeasurementMgr,
										  acxStatisitcs_t*	pAcxStatisitics,
										  mediumOccupancy_t* pMediumOccupancy)
{

	whalParamInfo_t	whalParam;
    measurementMgr_t *pMeasurementMgr = (measurementMgr_t*)hMeasurementMgr;

	/* getting the ACX statisitc counters by calling the HAL */ 
	whalParam.paramType = HAL_CTRL_ACX_STATISTICS_PARAM;
	if( (status = whalCtrl_GetParam(pMeasurementMgr->hHalCtrl,&whalParam)) == TI_OK)
	{
		pAcxStatisitics->FWpacketReceived = whalParam.content.acxStatisitics.FWpacketReceived;	
		pAcxStatisitics->HALpacketReceived	= whalParam.content.acxStatisitics.HALpacketReceived;
	}
	
	/*FIXME*/
	WLAN_OS_REPORT(("-------------- FW total---------------, %d\n\n", 
                    pAcxStatisitics->FWpacketReceived));
	WLAN_OS_REPORT(("-------------- Driver Total---------------, %d\n\n", 
                    pAcxStatisitics->HALpacketReceived));

    /*******************************************************
    * NOTE: If not using a call back function the required *
    *       information will not be received               *
    *******************************************************/
	/* getting the Medium Occupancy by calling the HAL */ 
	whalParam.paramType = HAL_CTRL_MEDIUM_OCCUPANCY_PARAM;
    whalParam.content.interogateCmdCBParams.CB_Func = NULL;
    whalParam.content.interogateCmdCBParams.CB_handle = hMeasurementMgr;
    whalParam.content.interogateCmdCBParams.CB_buf = NULL;

	if( (status = whalCtrl_GetParam(pMeasurementMgr->hHalCtrl,&whalParam)) == TI_OK)
	{
        return status;
	}

	return status;
}

#endif

/***********************************************************************
 * NOTE: The next 4 functions (dot11h...) should be corrected according 
 *       to the 802.11h standered.
 ***********************************************************************/

/************************************************************************
*					measurementMgr_dot11hParserFrameReq					*
************************************************************************
DESCRIPTION: Frame Request Parser function, called by the Measurement 
             object when a measurement request frame is received. 
				performs the following:
				-	Parsers the received frame request.
					
INPUT:      hMeasurementMgr - MeasurementMgr Handle
			pData			- The frame request
            dataLen         - The frame'sa length

OUTPUT:		fraemReq        - The Parsered Frame Request

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS measurementMgr_dot11hParseFrameReq(TI_HANDLE hMeasurementMgr, 
                                           TI_UINT8 *pData, TI_INT32 dataLen,
                                           TMeasurementFrameRequest *frameReq)
{
    dot11hFrameReqHdr_t     *dot11hFrameReqHdr;

    dot11hFrameReqHdr = (dot11hFrameReqHdr_t*)pData;
    
    frameReq->hdr->dialogToken = (TI_UINT16)dot11hFrameReqHdr->dialogToken;
    frameReq->hdr->activatioDelay = dot11hFrameReqHdr->activatioDelay;
    frameReq->hdr->measurementOffset = dot11hFrameReqHdr->measurementOffset;

    frameReq->requests = pData + DOT11H_REQUEST_IE_HDR_LEN;
    frameReq->requestsLen = dataLen - DOT11H_REQUEST_IE_HDR_LEN;

    return TI_OK;
}

/************************************************************************
 *					measurementMgr_dot11hParserRequestIEHdr				*
 ************************************************************************
DESCRIPTION: Spectrom Manager Request IE Header Parser function, 
             called by the Request Handler object when inserting 
             a request IE to the queue. 
				performs the following:
				-	Parsers the received request IE hdr.
					
INPUT:      hRequestHandler	-	Request Handler handle
			pData			-	The request IE

OUTPUT:		reqestLen           - The Request length
            measurementToken    - The Request IE token

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS measurementMgr_dot11hParseRequestIEHdr(TI_UINT8 *pData, TI_UINT16 *reqestHdrLen,
                                               TI_UINT16 *measurementToken)
{
    dot11hReqIEHdr_t *dot11hReqIEHdr;
    
    dot11hReqIEHdr = (dot11hReqIEHdr_t*)pData;
    
    /* checking if received the correct information element ID */
    if(dot11hReqIEHdr->IeId != DOT11_MEASUREMENT_REQUEST_ELE_ID)
        return TI_NOK;
        
    /* checking that len is valid */
    if(dot11hReqIEHdr->Length != DOT11H_REQUEST_IE_LEN)
        return TI_NOK;
    
    *measurementToken = (TI_UINT16)dot11hReqIEHdr->Token;
    
    *reqestHdrLen = DOT11H_REQUEST_IE_HDR_LEN;
    
    return TI_OK;
}

/************************************************************************
 *					measurementMgr_dot11hIsTypeValid         				*
 ************************************************************************
DESCRIPTION: Spectrom Manager function that checks if the given 
             measurement type is valid. 
					
INPUT:      hMeasurementMgr -	MeasurementMgr Handle
            type			-	The measurement type.
            scanMode        -   The Measurement scan Mode.

OUTPUT:		

RETURN:     TI_TRUE if type is valid, TI_FALSE otherwise

************************************************************************/
TI_BOOL measurementMgr_dot11hIsTypeValid(TI_HANDLE hMeasurementMgr, 
                                         EMeasurementType type, 
                                         EMeasurementScanMode scanMode)
{
    if(type != MSR_TYPE_BASIC_MEASUREMENT)
        return TI_FALSE;

    return TI_TRUE;
}

/***********************************************************************
 *                  measurementMgr_dot11hBuildRejectReport							
 ***********************************************************************
DESCRIPTION:	Send reject measurement report frame Function.
                The function does the following:
				-	checking the reason for reject.
				-	builds measurement report Frame.
				-	Calls the mlmeBuolder to allocate a mangement frame
					and to transmit it.
				
INPUT:      hMeasurementMgr -	MeasurementMgr Handle
			pRequestArr		-	Array of pointer to all measurement 
								request that should be rejected.
			numOfRequestsInParallel - indicates the number of 
								request that should be rejected.
			rejectReason	-	Indicated the rejection reason.

OUTPUT:		None

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS measurementMgr_dot11hBuildRejectReport(TI_HANDLE hMeasurementMgr,
											MeasurementRequest_t* pRequestArr[],
											TI_UINT8	numOfRequestsInParallel,
											EMeasurementRejectReason	rejectReason)
{
	measurementMgr_t	*pMeasurementMgr	 = (measurementMgr_t*)hMeasurementMgr;
	MeasurementReportFrame_t	measurementReport;
	TI_UINT8						measurementMode = 0;

	/* Building the measurement report frame contents */
	measurementReport.actionField[0] = CATAGORY_SPECTRUM_MANAGEMENT;
	measurementReport.actionField[1] = MEASUREMENT_REPORT;
	measurementReport.dialogToken = (TI_UINT8)pRequestArr[0]->frameToken;

	measurementReport.hdr[0] = DOT11_MEASUREMENT_REPORT_ELE_ID;
	measurementReport.hdr[1] = DOT11_MIN_MEASUREMENT_REPORT_IE_LEN;
	measurementReport.measurementToken = (TI_UINT8)pRequestArr[0]->measurementToken;
	measurementReport.measurementType = pRequestArr[0]->Type;
	
	/* Building the measurement mode bit field */

	/* setting Parallel Bit */
	if(numOfRequestsInParallel > 0 )
		measurementMode |= 0x1;
	
	/* setting Incapable and Refused bits */
	switch(rejectReason)
	{
		case MSR_REJECT_DTIM_OVERLAP:
		case MSR_REJECT_DURATION_EXCEED_MAX_DURATION:
		case MSR_REJECT_TRAFFIC_INTENSITY_TOO_HIGH:
		case MSR_REJECT_SCR_UNAVAILABLE:
		case MSR_REJECT_INVALID_CHANNEL:
		case MSR_REJECT_NOISE_HIST_FAIL:
		case MSR_REJECT_CHANNEL_LOAD_FAIL:
		case MSR_REJECT_EMPTY_REPORT:
		case MSR_REJECT_MAX_DELAY_PASSED:
		case MSR_REJECT_INVALID_MEASUREMENT_TYPE:
		{
			/* Setting the Refused bit */
			measurementMode |= 0x4;
			break;
		}
	
		default:
		{
TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "Reject reason is not supported, %d\n\n", rejectReason);

			break;
		}
	}
	
	measurementReport.measurementMode = measurementMode;
TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_WARNING, "Measurement was rejected due to %d,\n\n", rejectReason);

	/* Note: The Measurement report reject frame body includes 8 TI_UINT8 */
	return mlmeBuilder_sendFrame(pMeasurementMgr->hMlme,ACTION,(TI_UINT8*)&measurementReport,8,0);
}


/***********************************************************************
 *                        measurementMgr_dot11hBuildReport							
 ***********************************************************************
DESCRIPTION:	build measurement report Function.
                The function does the following:
				-	builds measurement report IE.
				
INPUT:      hMeasurementMgr -	MeasurementMgr Handle
			pRequestArr		-	Array of pointer to all measurement 
								request that should be reported.
			numOfRequestsInParallel - indicates the number of 
								request that should be reported.
			
OUTPUT:		None

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS measurementMgr_dot11hBuildReport(TI_HANDLE hMeasurementMgr, MeasurementRequest_t request, TMeasurementTypeReply * reply)
{
	/*measurementMgr_t				*pMeasurement	 = (measurementMgr_t*)hMeasurementMgr;*/
	MeasurementReportFrame_t	measurementReport;
	TI_UINT8						totalReportLen = 0;
	TI_UINT8						map = 0;
	TI_UINT8*						pMeasurReport = (TI_UINT8 *)&(measurementReport.measurementReports[0]);

	/* Building the measurement report frame contents */
	measurementReport.actionField[0] = CATAGORY_SPECTRUM_MANAGEMENT;
	measurementReport.actionField[1] = MEASUREMENT_REPORT;
	measurementReport.dialogToken = (TI_UINT8)request.frameToken;

	measurementReport.hdr[0] = DOT11_MEASUREMENT_REPORT_ELE_ID;
	measurementReport.measurementToken = (TI_UINT8)request.measurementToken;
	
	/* setting Parallel Bit in the measurement mode */
	if(request.isParallel)
		measurementReport.measurementMode = 0x1;
	
	measurementReport.measurementType = request.Type;
	
	
	/* Building the reports included in the current measurement report IE */
	/* Note: The current implementation supports only Basic Report */
	if(request.Type == MSR_TYPE_BASIC_MEASUREMENT)
	{		
			*pMeasurReport++ = request.channelNumber;
			*pMeasurReport++ = (TI_UINT8)request.DurationTime;

/* The following function uses features from the old Measurement module. */
/* It will have to be adapted to using the new Measurement Manager. */
#if 0
			/* building the map subfield */
			buildMapSubFieldForBasicReport(hMeasurementMgr,&map);
#endif /* 0 */

			*pMeasurReport++ = map;
			totalReportLen += 3;
	}
	return TI_OK;
}

/***********************************************************************
 *                   measurementMgr_dot11hSendReportAndCleanObject							
 ***********************************************************************
DESCRIPTION:	Send measurement report frame Function.
                The function does the following:
				-	Calls the mlmeBuilder to allocate a mangement frame
					and to transmit it.
				-	Cleans the Measurement Object and reset its Params.
				
INPUT:      hMeasurementMgr -	MeasurementMgr Handle
			
OUTPUT:		None

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS measurementMgr_dot11hSendReportAndCleanObject(TI_HANDLE hMeasurementMgr)
{
    measurementMgr_t	*pMeasurementMgr = (measurementMgr_t*)hMeasurementMgr;
	TI_STATUS		status;

	/* Building the Report Frame Header */
	/* TO DO */

    if(pMeasurementMgr->frameLength != 0)
    {
        /* Setting the IE Length field */
        /* TO DO */

        /* Sending the Rport Frame */
        status =  mlmeBuilder_sendFrame(pMeasurementMgr->hMlme,ACTION,
                                        (TI_UINT8*)&pMeasurementMgr->dot11hFrameReport,
                                        sizeof(dot11_ACTION_FIELD_t) + sizeof(TI_UINT8) + 
                                        sizeof(dot11_eleHdr_t) + 
                                        DOT11_MIN_MEASUREMENT_REPORT_IE_LEN + 
                                        pMeasurementMgr->frameLength,0);
        if(status != TI_OK)
            return status;
        
        /* clearing reports data base */
        os_memoryZero(pMeasurementMgr->hOs,&(pMeasurementMgr->dot11hFrameReport),
            sizeof(MeasurementReportFrame_t));
        pMeasurementMgr->frameLength = 0;
		pMeasurementMgr->nextEmptySpaceInReport = 0;
	}

	/* Reset the Measurement Object Params */
	pMeasurementMgr->currentFrameType = MSR_FRAME_TYPE_NO_ACTIVE;
	requestHandler_clearRequests(pMeasurementMgr->hRequestH);

	return TI_OK;
}


/********************************************************************************/
/*						Internal functions Implementation.						*/
/********************************************************************************/



/***********************************************************************
 *                     buildMapSubFieldForBasicReport									
 ***********************************************************************
DESCRIPTION:	
				
				
INPUT:      hMeasurementMgr -	MeasurementMgr Handle

OUTPUT:		map             -   

RETURN:     None
************************************************************************/

/* The following function uses features from the old Measurement module. */
/* It will have to be adapted to using the new Measurement Manager. */
#if 0

static void buildMapSubFieldForBasicReport(TI_HANDLE hMeasurementMgr,TI_UINT8* map)
{


	TI_INT32	deltaHALReceivedPacked;
	TI_INT32	deltaFWReceivedPacked;
	TI_INT32	deltaFCSError;
	TI_INT32	periodTimeDelta;
	TI_INT32	occupancyDelta;
    measurementMgr_t* pMeasurementMgr = (measurementMgr_t*)hMeasurementMgr;
	
	/* getting the AcxStatisitcs from the FW */
    /* NOTE: The medium occupancy will not be updated - FIX */

/* The following function uses features from the old Measurement module. */
/* It will have to be adapted to using the new Measurement Manager. */
#if 0
	measurementMgr_getBasicMeasurementParam(hMeasurementMgr, &pMeasurementMgr->acxStatisticEnd,
                                         &pMeasurementMgr->mediumOccupancyEnd);
#endif

	/* Calculating the delta for packetReceived from the HAL*/
	deltaHALReceivedPacked = pMeasurementMgr->acxStatisticEnd.HALpacketReceived - 
                             pMeasurementMgr->acxStatisticStart.HALpacketReceived; 
	
	if(deltaHALReceivedPacked < 0)
TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "HAL delta packets is negative , %d\n\n", deltaHALReceivedPacked);
	
	if(deltaHALReceivedPacked != 0 )
		*map = DOT11_BSS_ONLY;
	else
	{
		/* Calculating the delta for FCS Error*/
		deltaFWReceivedPacked = pMeasurementMgr->acxStatisticEnd.FWpacketReceived - 
                                pMeasurementMgr->acxStatisticStart.FWpacketReceived;
		
		if(deltaFWReceivedPacked < 0)
        {
TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "FW delta packets is negative , %d\n\n", deltaFWReceivedPacked);
        }

		deltaFCSError = deltaFWReceivedPacked - deltaHALReceivedPacked;
		
		if(deltaFCSError < 0)
        {
TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "FCS error is negative , %d\n\n", deltaFCSError);
        }

		if(deltaFCSError != 0 )
			*map = DOT11_OFDM_ONLY;
		else
		{
			/* Calculating the delta for Medium occupancy */
			occupancyDelta = pMeasurementMgr->mediumOccupancyEnd.MediumUsage - 
                             pMeasurementMgr->mediumOccupancyStart.MediumUsage;

			if(occupancyDelta < 0)
            {
TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "Medium Occupancy is negative , %d\n\n", occupancyDelta);
            }

			periodTimeDelta = pMeasurementMgr->mediumOccupancyEnd.Period - 
                              pMeasurementMgr->mediumOccupancyStart.Period;
			
			if(periodTimeDelta < 0)
            {
TRACE1(pMeasurementMgr->hReport, REPORT_SEVERITY_ERROR, "Period time delta is negative , %d\n\n", periodTimeDelta);
            }
			
			if( ((occupancyDelta * 100) / periodTimeDelta)  > RADAR_THRESHOLD_IN_PRECENTS )
				*map = DOT11_RADAR_AND_UNIDENTIFIED;
			else
				*map = 0;
		}
	}

	return;
}

#endif /* 0 */
