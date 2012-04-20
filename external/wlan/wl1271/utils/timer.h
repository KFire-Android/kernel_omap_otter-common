/*
 * timer.h
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



/** \file   timer.h 
 *  \brief  timer module header file.                                  
 *
 *  \see    timer.c
 */

#ifndef _TIMER_H_
#define _TIMER_H_


/* The callback function type for timer clients */
typedef void (*TTimerCbFunc)(TI_HANDLE hCbHndl, TI_BOOL bTwdInitOccured);


/* External Functions Prototypes */
/* ============================= */
TI_HANDLE tmr_Create (TI_HANDLE hOs);
TI_STATUS tmr_Destroy (TI_HANDLE hTimerModule);
TI_STATUS tmr_Free (TI_HANDLE hTimerModule);
void      tmr_ClearInitQueue (TI_HANDLE hTimerModule);
void      tmr_ClearOperQueue (TI_HANDLE hTimerModule);
void      tmr_Init (TI_HANDLE hTimerModule, TI_HANDLE hOs, TI_HANDLE hReport, TI_HANDLE hContext);
void      tmr_UpdateDriverState (TI_HANDLE hTimerModule, TI_BOOL bOperState);
TI_HANDLE tmr_CreateTimer (TI_HANDLE hTimerModule);
TI_STATUS tmr_DestroyTimer (TI_HANDLE hTimerInfo);
void      tmr_StartTimer (TI_HANDLE     hTimerInfo,
                          TTimerCbFunc  fExpiryCbFunc,
                          TI_HANDLE     hExpiryCbHndl,
                          TI_UINT32     uIntervalMsec,
                          TI_BOOL       bPeriodic);
void      tmr_StopTimer (TI_HANDLE hTimerInfo);
void      tmr_GetExpiry (TI_HANDLE hTimerInfo);
void      tmr_HandleExpiry (TI_HANDLE hTimerModule);

#ifdef TI_DBG
void      tmr_PrintModule (TI_HANDLE hTimerModule);
void      tmr_PrintTimer (TI_HANDLE hTimerInfo);
#endif /* TI_DBG */


#endif  /* _TIMER_H_ */


