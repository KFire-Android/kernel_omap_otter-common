/*
 * context.h
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



/** \file   context.h 
 *  \brief  context module header file.                                  
 *
 *  \see    context.c
 */

#ifndef _CONTEXT_H_
#define _CONTEXT_H_



/* The callback function type for context clients */
typedef void (*TContextCbFunc)(TI_HANDLE hCbHndl);

/* The context init parameters */
typedef struct
{
    /* Indicate if the driver should switch to its own context or not before handling events */
    TI_BOOL   bContextSwitchRequired;  
} TContextInitParams;



/* External Functions Prototypes */
/* ============================= */
TI_HANDLE context_Create          (TI_HANDLE hOs);
TI_STATUS context_Destroy         (TI_HANDLE hContext);
void      context_Init            (TI_HANDLE hContext, TI_HANDLE hOs, TI_HANDLE hReport);
TI_STATUS context_SetDefaults     (TI_HANDLE hContext, TContextInitParams *pContextInitParams);

TI_UINT32 context_RegisterClient (TI_HANDLE       hContext,
                                  TContextCbFunc  fCbFunc,
                                  TI_HANDLE       hCbHndl,
                                  TI_BOOL         bEnable,
                                  char           *sName,
                                  TI_UINT32       uNameSize);

void      context_RequestSchedule (TI_HANDLE hContext, TI_UINT32 uClientId);
void      context_DriverTask      (TI_HANDLE hContext);
void      context_EnableClient    (TI_HANDLE hContext, TI_UINT32 uClientId);
void      context_DisableClient   (TI_HANDLE hContext, TI_UINT32 uClientId);

void      context_EnterCriticalSection (TI_HANDLE hContext);
void      context_LeaveCriticalSection (TI_HANDLE hContext);
void      context_DisableClient   (TI_HANDLE hContext, TI_UINT32 uClientId);
void      context_EnableClient    (TI_HANDLE hContext, TI_UINT32 uClientId);
#ifdef TI_DBG
void      context_Print           (TI_HANDLE hContext);
#endif /* TI_DBG */



#endif  /* _CONTEXT_H_ */


