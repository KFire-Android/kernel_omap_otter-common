/*
 * txDataQueue_Api.h
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
/*																		   */
/*    	PURPOSE:	Tx Data Queue module api functions header file		   */
/*																		   */
/***************************************************************************/

#ifndef _TX_DATA_QUEUE_API_H_
#define _TX_DATA_QUEUE_API_H_

#include "paramOut.h"
#include "TWDriver.h"
#include "DrvMainModules.h"


/* 
 * Tx-Data-Queue functions:
 */
TI_HANDLE txDataQ_Create (TI_HANDLE hOs);
void      txDataQ_Init (TStadHandlesList *pStadHandles);
TI_STATUS txDataQ_SetDefaults (TI_HANDLE  hTxDataQ, txDataInitParams_t *pTxDataInitParams);
TI_STATUS txDataQ_Destroy (TI_HANDLE hTxDataQ);
void      txDataQ_ClearQueues (TI_HANDLE hTxDataQ);
TI_STATUS txDataQ_InsertPacket (TI_HANDLE hTxDataQ, TTxCtrlBlk *pPktCtrlBlk, TI_UINT8 uPacketDtag);
void      txDataQ_StopQueue (TI_HANDLE hTxDataQ, TI_UINT32 tidBitMap);
void      txDataQ_UpdateBusyMap (TI_HANDLE hTxDataQ, TI_UINT32 tidBitMap);
void      txDataQ_StopAll (TI_HANDLE hTxDataQ);
void      txDataQ_WakeAll (TI_HANDLE hTxDataQ);

#ifdef TI_DBG
void      txDataQ_PrintModuleParams    (TI_HANDLE hTxDataQ);
void      txDataQ_PrintQueueStatistics (TI_HANDLE hTxDataQ);
void      txDataQ_ResetQueueStatistics (TI_HANDLE hTxDataQ);
#endif /* TI_DBG */


/* 
 * Tx-Data-Classifier functions:
 */
TI_STATUS txDataClsfr_Config           (TI_HANDLE hTxDataQ, TClsfrParams *pClsfrInitParams);
TI_STATUS txDataClsfr_ClassifyTxPacket (TI_HANDLE hTxDataQ, TTxCtrlBlk *pPktCtrlBlk, TI_UINT8 uPacketDtag);
TI_STATUS txDataClsfr_InsertClsfrEntry (TI_HANDLE hTxDataQ, TClsfrTableEntry *pNewEntry);
TI_STATUS txDataClsfr_RemoveClsfrEntry (TI_HANDLE hTxDataQ, TClsfrTableEntry *pRemEntry);
TI_STATUS txDataClsfr_SetClsfrType     (TI_HANDLE hTxDataQ, EClsfrType eNewClsfrType);
TI_STATUS txDataClsfr_GetClsfrType     (TI_HANDLE hTxDataQ, EClsfrType *pClsfrType);

#ifdef TI_DBG
void      txDataClsfr_PrintClsfrTable  (TI_HANDLE hTxDataQ);
#endif /* TI_DBG */


#endif /* _TX_DATA_QUEUE_API_H_ */

