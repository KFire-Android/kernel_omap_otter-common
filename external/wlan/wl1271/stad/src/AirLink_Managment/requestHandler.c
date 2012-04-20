/*
 * requestHandler.c
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

/** \file RequestHandler.c
 *  \brief RequestHandler module interface
 *
 *  \see RequestHandler.h
 */

/****************************************************************************************************/
/*																									*/
/*		MODULE:		RequestHandler.c																*/
/*		PURPOSE:	RequestHandler module interface.												*/
/*                  This module handle the incoming measurement requests. The object handle			*/
/*					data base that stores all measurement requests from the last incoming.			*/
/*					This module export interface function for sceduling the next requests to be		*/
/*					executed and stores all relevent fields for constructing a measurement report.	*/
/*																									*/
/****************************************************************************************************/
#define __FILE_ID__  FILE_ID_4
#include "report.h"
#include "osApi.h"
#include "paramOut.h"
#include "requestHandler.h"

#ifdef XCC_MODULE_INCLUDED
#include "XCCRMMngrParam.h"
#endif

/* allocation vector */
#define REQUEST_HANDLER_INIT_BIT		(1)

#define DOT11_MEASUREMENT_REQUEST_ELE_ID (38)

/********************************************************************************/
/*						Internal functions prototypes.							*/
/********************************************************************************/
static void release_module(requestHandler_t *pRequestHandler, TI_UINT32 initVec);

static TI_STATUS insertMeasurementIEToQueue(TI_HANDLE           hRequestHandler,
											TI_UINT16			frameToken,
											EMeasurementMode	measurementMode,
											TI_UINT8			*pData,
                                            TI_UINT8            *singelRequestLen);

/********************************************************************************/
/*						Interface functions Implementation.						*/
/********************************************************************************/


/********************************************************************************
 *                        requestHandler_create									*
 ********************************************************************************
DESCRIPTION: RequestHandler module creation function, called by the measurement in 
				creation phase. performs the following:

				-	Allocate the RequestHandler handle
				                                                                                                   
INPUT:      hOs -	Handle to OS		

OUTPUT:		

RETURN:     Handle to the RequestHandler module on success, NULL otherwise
************************************************************************/
TI_HANDLE requestHandler_create(TI_HANDLE hOs)
{
	requestHandler_t			*pRequestHandler = NULL;
	TI_UINT32			initVec = 0;
	
	
	/* allocating the RequestHandler object */
	pRequestHandler = os_memoryAlloc(hOs,sizeof(requestHandler_t));

	if (pRequestHandler == NULL)
		return NULL;

	initVec |= (1 << REQUEST_HANDLER_INIT_BIT);

	return(pRequestHandler);
}

/************************************************************************
 *                        requestHandler_config		    				*
 ************************************************************************
DESCRIPTION: RequestHandler module configuration function, called by the measurement
			  in configuration phase. performs the following:
				-	Reset & initiailzes local variables
				-	Init the handles to be used by the module
                                                                                                   
INPUT:      hRequestHandler	-	RequestHandler handle.
			List of handles to be used by the module
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS RequestHandler_config(TI_HANDLE 	hRequestHandler,
						TI_HANDLE		hReport,
						TI_HANDLE		hOs)
{
	requestHandler_t *pRequestHandler = (requestHandler_t *)hRequestHandler;
	

	/* init variables */
    pRequestHandler->parserRequestIEHdr = NULL;
	pRequestHandler->numOfWaitingRequests = 0;	/*	indicating empty data base	*/
	pRequestHandler->activeRequestID = -1;		/*			   					*/
	pRequestHandler->hReport	= hReport;
	pRequestHandler->hOs		= hOs;

	/* Clearing the Request Array , mostly due to parallel bit */
	os_memoryZero(pRequestHandler->hOs, pRequestHandler->reqArr, MAX_NUM_REQ * sizeof(MeasurementRequest_t));

TRACE0(pRequestHandler->hReport, REPORT_SEVERITY_INIT, ": RequestHandler configured successfully\n");

	return TI_OK;
}

/***********************************************************************
 *                        requestHandler_setParam									
 ***********************************************************************
DESCRIPTION: RequestHandler set param function, called by the following:
			-	config mgr in order to set a parameter receiving from 
				the OS abstraction layer.
			-	From inside the dirver	
                                                                                                   
INPUT:      hRequestHandler	-	RequestHandler handle.
			pParam			-	Pointer to the parameter		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS requestHandler_setParam(TI_HANDLE	hRequestHandler,
								  paramInfo_t	*pParam)
{
	requestHandler_t *pRequestHandler = (requestHandler_t *)hRequestHandler;
	
	switch(pParam->paramType)
	{
/*	case RequestHandler_PARAM_TYPE:*/
		
	/*	break;*/
		
	default:
TRACE1(pRequestHandler->hReport, REPORT_SEVERITY_ERROR, ": Set param, Params is not supported, %d\n\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

/*	return TI_OK; - unreachable */
}

/***********************************************************************
 *                        requestHandler_getParam									
 ***********************************************************************
DESCRIPTION: RequestHandler get param function, called by the following:
			-	config mgr in order to get a parameter from the OS a
				bstraction layer.
			-	From inside the dirver	
                                                                                                   
INPUT:      hRequestHandler	-	RequestHandler handle.
			pParam			-	Pointer to the parameter		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS requestHandler_getParam(TI_HANDLE		hRequestHandler,
								  paramInfo_t	*pParam)
{
	requestHandler_t *pRequestHandler = (requestHandler_t *)hRequestHandler;
/*	TI_STATUS			status;*/
	
	switch(pParam->paramType)
	{
	/*case RequestHandler_PARAM:*/
		
		
		/*return status;*/
	
	default:
TRACE1(pRequestHandler->hReport, REPORT_SEVERITY_ERROR, ": Get param, Params is not supported, %d\n\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

/*	return TI_OK; - unreachable */
}

/************************************************************************
 *                        RequestHandler_destroy						*
 ************************************************************************
DESCRIPTION: RequestHandler module destroy function, called by the config 
			 mgr in the destroy phase 
			 performs the following:
			 -	Free all memory aloocated by the module
                                                                                                   
INPUT:      hRequestHandler	-	RequestHandler handle.		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS requestHandler_destroy(TI_HANDLE hRequestHandler)
{
	requestHandler_t * pRequestHandler = (requestHandler_t *)hRequestHandler;
	TI_UINT32 initVec;

	if (pRequestHandler == NULL)
		return TI_OK;

	initVec = 0xFFFF;
	release_module(pRequestHandler, initVec);

	return TI_OK;
}

/************************************************************************
 *                  requestHandler_insertRequests						*
 ************************************************************************
DESCRIPTION: RequestHandler module parsing function, called by the 
			  measurement object when measuremnt request frame is received.
				performs the following:
				-	Parsers the measurement request frame.
				-	Inserts all requests into the queue.
				-	Initializes each request according to the its frame 
					token, measurement token, measurement type, parallel,
					channel number, duration time and scan mode.
				-	The function updates the numOfWaitingRequests variable
					and set to zero the activeReqId.

			 Note:  The activeReqId contains the index for the first request
					that should be executed or to the current active request.

INPUT:      hRequestHandler	    -	RequestHandler handle.
			measurementMode     -	The MEasurement Object Mode.
			measurementFrameReq -   The New Frame request that was received.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS requestHandler_insertRequests(TI_HANDLE hRequestHandler,
										EMeasurementMode measurementMode,
										TMeasurementFrameRequest measurementFrameReq)
{
	requestHandler_t	*pRequestHandler = (requestHandler_t *)hRequestHandler;
    TI_INT32               requestsLen = measurementFrameReq.requestsLen;
    TI_UINT8               singelRequestLen = 0;
    TI_UINT8               *requests = measurementFrameReq.requests;
		    
	if (requestsLen < 2)
    {
        TRACE0(pRequestHandler->hReport, REPORT_SEVERITY_ERROR, ": Invalid length of the data.\n");

        return TI_NOK;
    }

	/* Inserting all measurement request into the queues */
	while (requestsLen > 0)
	{
		if(insertMeasurementIEToQueue(hRequestHandler, 
                                       measurementFrameReq.hdr->dialogToken, 
                                       measurementMode, 
                                       requests,
                                       &singelRequestLen) != TI_OK )
        {
            requestHandler_clearRequests(hRequestHandler);
			return TI_NOK;
        }
  
		requestsLen -= singelRequestLen;
		requests += singelRequestLen;
      
	}

	pRequestHandler->activeRequestID = 0;
	
TRACE2(pRequestHandler->hReport, REPORT_SEVERITY_INFORMATION, ": Inserted into queue: activeRequestID = %d, numOfWaitingRequests = %d\n",					pRequestHandler->activeRequestID, pRequestHandler->numOfWaitingRequests);

	return TI_OK;
}

/************************************************************************
 *                  requestHandler_getNextReq							*
 ************************************************************************
DESCRIPTION: RequestHandler module function for retrieving the requests that
				should be executed.
				performs the following:
				-	returns pointers to one request/several requests that
					should be performed in parallel.
				Note: The function updates the numOfWaitingRequests internal
				varaible ONLY IF the returned request/s are going to be 
				executed immediatly (isForActivation = TI_TRUE).

INPUT:      hRequestHandler	-	RequestHandler handle.
			
  			isForActivation -	A flag that indicates if the returned 
								request/s are going to be executed immediatly

OUTPUT:		pRequest		-	pointer contains the address in which the 
								next requests for activation should be inserted.
		
			numOfRequests	-	indicates how many requests should be activated
								in parallel.
			
RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS requestHandler_getNextReq(TI_HANDLE hRequestHandler,
									TI_BOOL	  isForActivation,
									MeasurementRequest_t *pRequest[],
									TI_UINT8*	  numOfRequests)
{
	requestHandler_t	*pRequestHandler = (requestHandler_t *)hRequestHandler;
	TI_UINT8				requestIndex = pRequestHandler->activeRequestID;
	TI_UINT8				loopIndex = 0;
	
TRACE2(pRequestHandler->hReport, REPORT_SEVERITY_INFORMATION, ": Looking for requests. activeRequestID = %d, numOfWaitingRequests = %d\n",					pRequestHandler->activeRequestID, pRequestHandler->numOfWaitingRequests);

	if(pRequestHandler->numOfWaitingRequests <= 0)
		return TI_NOK;

	do{
		pRequest[loopIndex] = &(pRequestHandler->reqArr[requestIndex]);
		requestIndex++;
		loopIndex++;
	}
	while ( (loopIndex < pRequestHandler->numOfWaitingRequests) && 
            (pRequestHandler->reqArr[requestIndex].isParallel) );

	*numOfRequests = loopIndex;

TRACE1(pRequestHandler->hReport, REPORT_SEVERITY_INFORMATION, ": Found %d requests to execute in parallel.\n", loopIndex);

	if(isForActivation == TI_TRUE)
	{
		pRequestHandler->numOfWaitingRequests -= loopIndex;

TRACE1(pRequestHandler->hReport, REPORT_SEVERITY_INFORMATION, ": Requests were queried for activation so decreasing numOfWaitingRequests to %d\n", pRequestHandler->numOfWaitingRequests);
	}

	return TI_OK;
}

/************************************************************************
 *                  requestHandler_getCurrentExpiredReq					*
 ************************************************************************
DESCRIPTION: RequestHandler module function for retrieving the request that
				finished its execution.
				performs the following:
				-	returns pointers to the request that
					finished its execution in.

INPUT:      hRequestHandler	-	RequestHandler handle.
			requestIndex	-	Index of request in the queue
			
OUTPUT:		pRequest		-	pointer contains the addresse of the 
								request that finished its execution.

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS requestHandler_getCurrentExpiredReq(TI_HANDLE hRequestHandler,
											  TI_UINT8 requestIndex,
											  MeasurementRequest_t **pRequest)
{
	requestHandler_t	*pRequestHandler = (requestHandler_t *)hRequestHandler;
	
	requestIndex += pRequestHandler->activeRequestID;

	*pRequest = &(pRequestHandler->reqArr[requestIndex]);

	return TI_OK;
}


/************************************************************************
 *                  requestHandler_clearRequests						*
 ************************************************************************
DESCRIPTION: RequestHandler module function for cleaning the data base.
				performs the following:
				-	Clears all requests from the queue by setting
					the activeReqId and numOfWaitingRequests variables.
			Note:	The function does not actually zero all queue 
					variables or destroy the object.

INPUT:      hRequestHandler	-	RequestHandler handle.
			
OUTPUT:		None

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS requestHandler_clearRequests(TI_HANDLE hRequestHandler)
{
	requestHandler_t	*pRequestHandler = (requestHandler_t *)hRequestHandler;

	pRequestHandler->numOfWaitingRequests = 0;
	pRequestHandler->activeRequestID = -1;
	
	/* Clearing the Request Array , mostly due to parallel bit */
	os_memoryZero(pRequestHandler->hOs,pRequestHandler->reqArr,
				  MAX_NUM_REQ * sizeof(MeasurementRequest_t));	
	
TRACE2(pRequestHandler->hReport, REPORT_SEVERITY_INFORMATION, ": Request queue has been cleared. activeRequestID = %d, numOfWaitingRequests = %d\n",					pRequestHandler->activeRequestID, pRequestHandler->numOfWaitingRequests);

	return TI_OK;
}


	
/************************************************************************
 *                  requestHandler_getFrameToken						*
 ************************************************************************
DESCRIPTION: RequestHandler module function for getting the token of the 
				frame that is now being processed.

INPUT:      hRequestHandler	-	RequestHandler handle.
			
			
OUTPUT:		frameToken

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
TI_STATUS requestHandler_getFrameToken(TI_HANDLE hRequestHandler,TI_UINT16 *frameToken )
{
	requestHandler_t	*pRequestHandler = (requestHandler_t *)hRequestHandler;

	if(pRequestHandler->activeRequestID == -1)
		return TI_NOK;

	*frameToken = pRequestHandler->reqArr[0].frameToken;
	
	return TI_OK;
}

/************************************************************************
 *              requestHandler_setRequestParserFunction					*
 ************************************************************************
DESCRIPTION: RequestHandler module function for setting the function that
             parasers a request IE.

INPUT:      hRequestHandler	-	RequestHandler handle.
            parserRequestIE -   A pointer to the function.
			
			
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/	
TI_STATUS requestHandler_setRequestParserFunction(TI_HANDLE hRequestHandler, 
                                                  parserRequestIEHdr_t parserRequestIEHdr)
{
	requestHandler_t	*pRequestHandler = (requestHandler_t *)hRequestHandler;

    pRequestHandler->parserRequestIEHdr = parserRequestIEHdr;

    return TI_OK;
}

/********************************************************************************/
/*						Internal functions Implementation.						*/
/********************************************************************************/

/************************************************************************
 *                  insertMeasurementIEToQueue							*
 ************************************************************************
DESCRIPTION: The function inserts measurement request of one received 
				measurement request information element.

INPUT:      hRequestHandler	-	A Handler to the Request Handler Object.
			frameToken		-	Frame token of the received frame in which
								This current measurement request IE is included.
            measurementObjMode - XCC or SPECTRUM_MNGMNT
			dataLen			-	pointer to the data length that is left.
			pData			-	pointer to the data.
			
OUTPUT:		singelRequestLen - The Length of the request that was inserted 
                               to the queue.

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
static TI_STATUS insertMeasurementIEToQueue(TI_HANDLE           hRequestHandler,
											TI_UINT16				frameToken,
											EMeasurementMode	measurementObjMode,
											TI_UINT8				*pData,
                                            TI_UINT8               *singelRequestLen)
{
   	requestHandler_t	*pRequestHandler = (requestHandler_t *)hRequestHandler;

	TI_UINT16		HeaderLen;
	TI_UINT8		measurementMode;
	TI_UINT8		parallelBit;
	TI_UINT8		enableBit;
    TI_UINT16       measurementToken;
	
	MeasurementRequest_t	*pCurrRequest = &(pRequestHandler->reqArr[pRequestHandler->numOfWaitingRequests]);

    if (pRequestHandler->parserRequestIEHdr(pData, &HeaderLen, &measurementToken) != TI_OK)
    {
        return TI_NOK;
    }

	pCurrRequest->frameToken = frameToken;	
	pCurrRequest->measurementToken = measurementToken;

    pData += HeaderLen;

    /*** Getting the Measurement Mode ***/
	measurementMode		= *pData++;

	/* getting parallel bit */
	parallelBit = measurementMode & 0x1;
	
    /* getting Enable bit */
	enableBit = (measurementMode & 0x2)>>1;
	
    /* checking enable bit, the current implementation does not support 
		enable bit which set to one, so there is no need to check request/report bits	*/
	if(enableBit == 1)
		return TI_OK;
    
    pCurrRequest->isParallel = parallelBit;


    /* Getting the Measurement Mode */
   	pCurrRequest->Type = (EMeasurementType)(*pData++);

	/* Inserting the request that is included in the current measurement request IE. */
	pCurrRequest->channelNumber = *pData++;
    
	pCurrRequest->ScanMode = (EMeasurementScanMode)(*pData++); /* IN dot11h - Spare = 0 */

    COPY_WLAN_WORD(&pCurrRequest->DurationTime, pData);
	
	*singelRequestLen = HeaderLen + 6;

	pRequestHandler->numOfWaitingRequests ++;
	
	return TI_OK;
}


/***********************************************************************
 *                        release_module									
 ***********************************************************************
DESCRIPTION:	Called by the destroy function or by the create function 
				(on failure). Go over the vector, for each bit that is 
				set, release the corresponding module.
                                                                                                   
INPUT:      pRequestHandler	-	RequestHandler pointer.
			initVec			-	Vector that contains a bit set for each 
								module thah had been initiualized

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static void release_module(requestHandler_t *pRequestHandler, TI_UINT32 initVec)
{
	
	if ( initVec & (1 << REQUEST_HANDLER_INIT_BIT) )
		os_memoryFree(pRequestHandler->hOs, pRequestHandler, sizeof(requestHandler_t));
		
	initVec = 0;
}






