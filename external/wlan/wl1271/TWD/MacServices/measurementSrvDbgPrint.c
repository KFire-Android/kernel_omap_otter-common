/*
 * measurementSrvDbgPrint.c
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

/** \file measurementSrvDbgPrint.c
 *  \brief This file include variuos measurement SRV debug print facilities
 *  \author Ronen Kalish
 *  \date 23-December-2005
 */

#define __FILE_ID__  FILE_ID_111
#include "report.h"
#include "TWDriver.h"
#include "MeasurementSrvSM.h"
#include "MeasurementSrv.h"
#include "measurementSrvDbgPrint.h"


/** 
 * \author Ronen Kalish\n
 * \date 23-December-2005\n
 * \brief Prints a measurement request.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 * \param pMsrRequest - the measurement request.\n
 */
void measurementSRVPrintRequest( TI_HANDLE hMeasurementSRV, TMeasurementRequest *pMsrRequest )
{
#ifdef TI_DBG    
	measurementSRV_t* pMeasurementSRV = (measurementSRV_t*)hMeasurementSRV;
	TI_INT32 i;

TRACE0( pMeasurementSRV->hReport, REPORT_SEVERITY_INFORMATION, "Measurement request:\n");
TRACE5( pMeasurementSRV->hReport, REPORT_SEVERITY_INFORMATION, "band: %d, channel:%d, TX power level: %d, start time: %x-%x\n", pMsrRequest->band, pMsrRequest->channel, pMsrRequest->txPowerDbm, INT64_HIGHER(pMsrRequest->startTime), INT64_LOWER(pMsrRequest->startTime));
	for ( i = 0; i < pMsrRequest->numberOfTypes; i++ )
	{
		measurementSRVPrintTypeRequest( hMeasurementSRV, &(pMsrRequest->msrTypes[ i ]));
	}
#endif /* TI_DBG */

}

/** 
 * \author Ronen Kalish\n
 * \date 23-December-2005\n
 * \brief Prints a measurement type request.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSRV - handle to the measurement SRV object.\n
 * \param pMsrTypeRequest - the measurement type request.\n
 */
void measurementSRVPrintTypeRequest( TI_HANDLE hMeasurementSRV, TMeasurementTypeRequest* pMsrTypeRequest )
{
#ifdef TI_DBG    
	measurementSRV_t* pMeasurementSRV = (measurementSRV_t*)hMeasurementSRV;

TRACE4( pMeasurementSRV->hReport, REPORT_SEVERITY_INFORMATION, "Measurement type request: type: %d, duration:%d, scan mode: %d, reserved: %d", pMsrTypeRequest->msrType, pMsrTypeRequest->duration, pMsrTypeRequest->scanMode, pMsrTypeRequest->reserved);

#endif /* TI_DBG */
}

