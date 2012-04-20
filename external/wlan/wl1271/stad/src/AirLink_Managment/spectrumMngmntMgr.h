/*
 * spectrumMngmntMgr.h
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

/** \file spectrumMngmntMgr.h
 *  \brief dot11h spectrum Management Meneger module interface header file
 *
 *  \see spectrumMngmntMgr.c
 */

/***************************************************************************/
/*																			*/
/*	  MODULE:	spectrumMngmntMgr.h											*/
/*    PURPOSE:	dot11h spectrum Management Meneger module interface         */
/*              header file                                        			*/
/*																			*/
/***************************************************************************/
#ifndef __SPECTRUMMNGMNTMGR_H__
#define __SPECTRUMMNGMNTMGR_H__

#include "paramOut.h"
#include "measurementMgr.h"
#include "requestHandler.h"



TI_STATUS measurementMgr_receiveQuietIE(TI_HANDLE hMeasurementMgr,
								  TI_UINT8		quietCount,
								  TI_UINT8		quietPeriod,
								  TI_UINT16	quietDuration,
								  TI_UINT16	quietOffset);


TI_STATUS measurementMgr_receiveTPCRequest(TI_HANDLE hMeasurementMgr,
										TI_UINT8		dataLen,
										TI_UINT8		*pData);

TI_STATUS measurementMgr_dot11hParseFrameReq(TI_HANDLE hMeasurementMgr, 
                                           TI_UINT8 *pData, TI_INT32 dataLen,
                                           TMeasurementFrameRequest *frameReq);

TI_STATUS measurementMgr_dot11hParseRequestIEHdr(TI_UINT8 *pData, TI_UINT16 *reqestHdrLen,
                                               TI_UINT16 *measurementToken);

TI_BOOL      measurementMgr_dot11hIsTypeValid(TI_HANDLE hMeasurementMgr, 
                                              EMeasurementType type, 
                                              EMeasurementScanMode scanMode);

TI_STATUS measurementMgr_dot11hBuildReport(TI_HANDLE hMeasurementMgr, MeasurementRequest_t request, TMeasurementTypeReply * reply);

TI_STATUS measurementMgr_dot11hSendReportAndCleanObject(TI_HANDLE hMeasurementMgr);

TI_STATUS measurementMgr_dot11hBuildRejectReport(TI_HANDLE hMeasurementMgr,
											 MeasurementRequest_t *pRequestArr[],
											 TI_UINT8		numOfRequestsInParallel,
											 EMeasurementRejectReason	rejectReason);


/* The following function uses features from the old Measurement module. */
/* It will have to be adapted to using the new Measurement Manager. */
#if 0

TI_STATUS measurementMgr_getBasicMeasurementParam(TI_HANDLE hMeasurementMgr,
										  acxStatisitcs_t*	pAcxStatisitics,
										  mediumOccupancy_t* pMediumOccupancy);
#endif /* 0 */



#endif /* __SPECTRUMMNGMNTMGR_H__ */
