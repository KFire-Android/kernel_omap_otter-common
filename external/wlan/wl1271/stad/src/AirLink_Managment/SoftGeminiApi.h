/*
 * SoftGeminiApi.h
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

/** \file SoftGeminiApi.h
 *  \brief BlueTooth-Wlan coexistence module interface header file
 *
 *  \see SoftGemini.c & SoftGemini.h
 */

/***************************************************************************/
/*																			*/
/*	  MODULE:	SoftGeminiApi.h												*/
/*    PURPOSE:	BlueTooth-Wlan coexistence module interface header file		*/
/*																			*/
/***************************************************************************/
#ifndef __SOFT_GEMINI_API_H__
#define __SOFT_GEMINI_API_H__

#include "paramOut.h"
#include "DrvMainModules.h"


/* The Soft-Gemini module API functions */
TI_HANDLE SoftGemini_create(TI_HANDLE hOs);
void      SoftGemini_init (TStadHandlesList *pStadHandles);
TI_STATUS SoftGemini_SetDefaults (TI_HANDLE hSoftGemini, SoftGeminiInitParams_t *pSoftGeminiInitParams);
TI_STATUS SoftGemini_destroy(TI_HANDLE hSoftGemini);
TI_STATUS SoftGemini_setParam(TI_HANDLE hSoftGemini, paramInfo_t *pParam);
TI_STATUS SoftGemini_getParam(TI_HANDLE hSoftGemini, paramInfo_t *pParam);
void      SoftGemini_printParams(TI_HANDLE hSoftGemini);
void      SoftGemini_SenseIndicationCB( TI_HANDLE hSoftGemini, char* str, TI_UINT32 strLen );
void      SoftGemini_ProtectiveIndicationCB( TI_HANDLE hSoftGemini, char* str, TI_UINT32 strLen );
void      SoftGemini_startPsPollFailure(TI_HANDLE hSoftGemini);
void      SoftGemini_endPsPollFailure(TI_HANDLE hSoftGemini);
void SoftGemini_SetPSmode(TI_HANDLE hSoftGemini);
void SoftGemini_unSetPSmode(TI_HANDLE hSoftGemini);
ESoftGeminiEnableModes SoftGemini_getSGMode(TI_HANDLE hSoftGemini);

#endif /* __SOFT_GEMINI_API_H__ */
