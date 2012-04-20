/*
 * txMgmtQueue_Api.h
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
/*	  MODULE:	txMgmtQueue_Api.h									       */
/*    PURPOSE:	Tx Mgmt Queue module API Header file					   */
/*																		   */
/***************************************************************************/
#ifndef _TX_MGMT_QUEUE_API_H_
#define _TX_MGMT_QUEUE_API_H_

#include "DrvMainModules.h"


#define MGMT_QUEUES_DEPTH	4	/* Up to 4 packets per queue. */


/* Tx connection states updated by the connection SM. */
typedef enum
{
	TX_CONN_STATE_CLOSE,	/* Tx closed for all packets. */
	TX_CONN_STATE_MGMT,		/* Tx open only for mgmt packets. */
	TX_CONN_STATE_EAPOL,	/* Tx open only for mgmt or EAPOL packets. */
	TX_CONN_STATE_OPEN		/* Tx open for all packets. */
} ETxConnState;


/* 
 *  The module public functions:
 */
TI_HANDLE	txMgmtQ_Create (TI_HANDLE hOs);
void        txMgmtQ_Init (TStadHandlesList *pStadHandles);
TI_STATUS	txMgmtQ_Destroy (TI_HANDLE hTxMgmtQ);
void        txMgmtQ_ClearQueues (TI_HANDLE hTxMgmtQ);
TI_STATUS   txMgmtQ_Xmit (TI_HANDLE hTxMgmtQ, TTxCtrlBlk *pPktCtrlBlk, TI_BOOL bExternalContext);
void        txMgmtQ_QueuesNotEmpty (TI_HANDLE hTxMgmtQ);
void		txMgmtQ_StopQueue (TI_HANDLE hTxMgmtQ, TI_UINT32 tidBitMap);
void		txMgmtQ_UpdateBusyMap (TI_HANDLE hTxMgmtQ, TI_UINT32 tidBitMap);
void		txMgmtQ_StopAll (TI_HANDLE hTxMgmtQ);
void		txMgmtQ_WakeAll (TI_HANDLE hTxMgmtQ);
void		txMgmtQ_SetConnState (TI_HANDLE hTxMgmtQ, ETxConnState txConnState);

#ifdef TI_DBG
void        txMgmtQ_PrintModuleParams (TI_HANDLE hTxMgmtQ);
void        txMgmtQ_PrintQueueStatistics (TI_HANDLE	hTxMgmtQ);
void        txMgmtQ_ResetQueueStatistics (TI_HANDLE	hTxMgmtQ);
#endif


#endif /* _TX_MGMT_QUEUE_API_H_ */
