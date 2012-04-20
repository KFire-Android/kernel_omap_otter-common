/*
 * SoftGemini.h
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

/** \file SoftGemini.h
 *  \brief BlueTooth-Wlan coexistence module internal header file
 *
 *  \see SoftGemini.c
 */

/***************************************************************************/
/*																			*/
/*	  MODULE:	SoftGemini.h												*/
/*    PURPOSE:	BlueTooth-Wlan coexistence module internal header file			*/
/*																			*/
/***************************************************************************/

#ifndef __SOFT_GEMINI_H__
#define __SOFT_GEMINI_H__

#include "paramOut.h"
#include "SoftGeminiApi.h"


typedef struct 
{
	ESoftGeminiEnableModes  SoftGeminiEnable;						
    ESoftGeminiEnableModes  PsPollFailureLastEnableValue;
	TSoftGeminiParams		SoftGeminiParam;						/* for the FW */
	TI_BOOL					bProtectiveMode;
	TI_BOOL					bDriverEnabled;
    TI_BOOL					bPsPollFailureActive;							/* used to check if we should enable driver when we are switching different enable modes */
	TI_HANDLE				hCtrlData;
	TI_HANDLE				hTWD;
	TI_HANDLE				hReport;
	TI_HANDLE				hOs;
	TI_HANDLE				hSCR;
	TI_HANDLE				hPowerMgr;
	TI_HANDLE               hCmdDispatch;
	TI_HANDLE				hScanCncn;
	TI_HANDLE				hCurrBss;
    TI_HANDLE               hSme;
} SoftGemini_t;

TI_STATUS SoftGemini_handleRecovery(TI_HANDLE hSoftGemini);

#endif /* __SOFT_GEMINI_H__*/


