/*
 * txHwQueue_api.h
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
 *   MODULE:  txHwQueue_api.h
 *
 *   PURPOSE: HW Tx Queue module API.
 * 
 ****************************************************************************/

#ifndef _TX_HW_QUEUE_API_H
#define _TX_HW_QUEUE_API_H


#include "TWDriver.h"
#include "TWDriverInternal.h"


/* Public Function Definitions */

TI_HANDLE	txHwQueue_Create (TI_HANDLE hOs);
TI_STATUS	txHwQueue_Destroy (TI_HANDLE hTxHwQueue);
TI_STATUS	txHwQueue_Init (TI_HANDLE hTxHwQueue, TI_HANDLE hReport);
TI_STATUS	txHwQueue_Config (TI_HANDLE hTxHwQueue, TTwdInitParams *pInitParams);
TI_STATUS	txHwQueue_SetHwInfo (TI_HANDLE hTxHwQueue, TDmaParams *pDmaParams);
TI_STATUS	txHwQueue_Restart (TI_HANDLE hTxHwQueue);
ETxHwQueStatus txHwQueue_AllocResources (TI_HANDLE hTxHwQueue, TTxCtrlBlk *pTxCtrlBlk);
ETxnStatus         txHwQueue_UpdateFreeResources (TI_HANDLE hTxHwQueue, FwStatus_t *pFwStatus);
void        txHwQueue_RegisterCb (TI_HANDLE hTxHwQueue, TI_UINT32 uCallBackId, void *fCbFunc, TI_HANDLE hCbHndl);
#ifdef TI_DBG
void		txHwQueue_PrintInfo (TI_HANDLE hTxHwQueue);
#endif /* TI_DBG */


#endif /* _TX_HW_QUEUE_API_H */




