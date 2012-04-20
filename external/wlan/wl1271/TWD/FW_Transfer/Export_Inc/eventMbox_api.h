/*
 * eventMbox_api.h
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


/****************************************************************************
 *
 *   MODULE:  eventMbox_api.h : event Mail Box API
 *   PURPOSE: Notify EventMbox in case of an incoming event from the FW
 *
 ****************************************************************************/
#ifndef _EVENT_MBOX_API_H
#define _EVENT_MBOX_API_H


#include "TWDriverInternal.h"


/*
 *  TEventMboxDataCb : This Call back is for EventMbox Client 
 *                     that expect an event with Data associated  
 *                     str:    The Data String
 *                     strLen : The Length of the Data
 */
typedef void (*TEventMboxDataCb)(TI_HANDLE hCb, TI_CHAR* str, TI_UINT32 strLen);

/*
 *  TEventMboxEvCb : This Call back is for EventMbox Client 
 *                   that expect an event without any Data
 */
typedef void (*TEventMboxEvCb)(TI_HANDLE hCb);


/*****************************************************************************
 **         API                                                            **
 *****************************************************************************/

TI_HANDLE eventMbox_Create          (TI_HANDLE hOs);
void      eventMbox_Destroy			(TI_HANDLE hEventMbox);
void      eventMbox_Stop			(TI_HANDLE hEventMbox);
void      eventMbox_Config			(TI_HANDLE hEventMbox,
									 TI_HANDLE hTwif,
									 TI_HANDLE hReport,
									 TI_HANDLE hFwEvent,
									 TI_HANDLE hCmdBld);
TI_STATUS eventMbox_InitMboxAddr	(TI_HANDLE hEventMbox, fnotify_t fCb, TI_HANDLE hCb);
void      eventMbox_InitComplete	(TI_HANDLE hEventMbox);
TI_STATUS eventMbox_RegisterEvent	(TI_HANDLE hEventMbox, TI_UINT32 EvID, void *fCb, TI_HANDLE hCb);
TI_STATUS eventMbox_ReplaceEvent	(TI_HANDLE hEventMbox,
                                     TI_UINT32   EvID, 
                                     void       *fNewCb, 
                                     TI_HANDLE   hNewCb,                                    
                                     void      **pPrevCb, 
                                     TI_HANDLE  *pPrevHndl);                                
TI_STATUS eventMbox_UnMaskEvent		(TI_HANDLE hEventMbox, TI_UINT32 EvID, void *fCb, TI_HANDLE hCb);
TI_STATUS eventMbox_MaskEvent		(TI_HANDLE hEventMbox, TI_UINT32 EvID, void *fCb, TI_HANDLE hCb);
ETxnStatus      eventMbox_Handle		   	(TI_HANDLE hEventMbox, FwStatus_t *pFwStatus);
#ifdef TI_DBG
TI_STATUS eventMbox_Print           (TI_HANDLE hEventMbox);
#endif

#endif /* _EVENT_MBOX_API_H */
