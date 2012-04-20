/*
 * GeneralUtilApi.h
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


/***************************************************************************/
/*																		   */
/*	  MODULE:	TrafficMonitor.h												       */
/*    PURPOSE:	TrafficMonitor module Header file		 							   */
/*																		   */
/***************************************************************************/
#ifndef _GENERALUTIL_API_H_
#define _GENERALUTIL_API_H_

#include "tidef.h"
#include "paramOut.h" /* check tis include*/
/**/
/* call back functions prototype.*/
/**/
typedef void (*GeneralEventCall_t)(TI_HANDLE Context,int EventCount,TI_UINT16 Mask,TI_UINT32 Cookie);
               

TI_HANDLE DistributorMgr_Create(TI_HANDLE hOs , int MaxNotifReqElment); 
TI_STATUS DistributorMgr_Destroy(TI_HANDLE hDistributorMgr);
TI_HANDLE DistributorMgr_Reg(TI_HANDLE hDistributorMgr,TI_UINT16 Mask,TI_HANDLE CallBack,TI_HANDLE Context,TI_UINT32 Cookie);
TI_STATUS DistributorMgr_ReReg(TI_HANDLE hDistributorMgr,TI_HANDLE ReqElmenth ,TI_UINT16 Mask,TI_HANDLE CallBack,TI_HANDLE Context,TI_UINT32 Cookie);
TI_STATUS DistributorMgr_AddToMask(TI_HANDLE hDistributorMgr,TI_HANDLE ReqElmenth,TI_UINT16 Mask);
TI_STATUS DistributorMgr_UnReg(TI_HANDLE hDistributorMgr,TI_HANDLE RegEventHandle);
void DistributorMgr_HaltNotif(TI_HANDLE ReqElmenth);
void DistributorMgr_RestartNotif(TI_HANDLE ReqElmenth);
void DistributorMgr_EventCall(TI_HANDLE hDistributorMgr,TI_UINT16 Mask,int EventCount); 


TI_HANDLE List_create(TI_HANDLE hOs,int MaxNumOfElements,int ContainerSize);
TI_STATUS List_Destroy(TI_HANDLE hList)	;
TI_HANDLE List_AllocElement(TI_HANDLE hList);
TI_STATUS List_FreeElement(TI_HANDLE hList,TI_HANDLE Container);
TI_HANDLE List_GetFirst(TI_HANDLE List);
TI_HANDLE List_GetNext(TI_HANDLE List);


#endif
