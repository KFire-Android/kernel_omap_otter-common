/*
 * txDataQueue.h
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
/*	  MODULE:	TxDataQueue.h										       */
/*    PURPOSE:	Tx Data Queue module Header file						   */
/*																		   */
/***************************************************************************/
#ifndef _TX_DATA_QUEUE_H_
#define _TX_DATA_QUEUE_H_


#include "TWDriver.h"
#include "DataCtrl_Api.h"


#define TX_SEND_PACE_TIMEOUT_MSEC   1

/* Max number of packets in each queue */
#define DATA_QUEUE_DEPTH_BE  60
#define DATA_QUEUE_DEPTH_BK  10
#define DATA_QUEUE_DEPTH_VI  32
#define DATA_QUEUE_DEPTH_VO  10
#define DATA_QUEUE_DEPTH_TOTAL  (DATA_QUEUE_DEPTH_BE + DATA_QUEUE_DEPTH_BK + DATA_QUEUE_DEPTH_VI + DATA_QUEUE_DEPTH_VO)

/* Verify that there are enough TxCtrlBlks for all users that are queueing packets (driver + FW) */
#if ((DATA_QUEUE_DEPTH_TOTAL + (MGMT_QUEUES_DEPTH * 2) + NUM_TX_DESCRIPTORS) > (CTRL_BLK_ENTRIES_NUM - 2))
    #error  Not enough TxCtrlBlks for all users !!
#endif

/* Tx packets handling statistics */
typedef struct
{
	TI_UINT32 uEnqueuePacket;
	TI_UINT32 uDequeuePacket;
	TI_UINT32 uRequeuePacket;
	TI_UINT32 uXmittedPacket;
	TI_UINT32 uDroppedPacket;
} TTxDataQueueDebugCnt;

/* The module's object */
typedef struct 
{
	TI_HANDLE            hContext;
	TI_HANDLE            hTxCtrl;
	TI_HANDLE            hOs;
	TI_HANDLE            hReport;
	TI_HANDLE            hTxMgmtQ;
	TI_HANDLE            hTWD;
			             
	TClsfrParams		 tClsfrParams;  /* The classifier sub-module parameters */

	TI_BOOL              bDataPortEnable; /* Data port open or not */
    TI_UINT32            uContextId;  /* ID allocated to this module on registration to context module */
    TI_HANDLE            hTxSendPaceTimer; /* If queued packets number doesn't reach threshold within timeout, Tx handling is started */  
						   
	/* Tx aQueues */	    	
	TI_UINT32            uNumQueues; /* Indicates the number of allocated aQueues */
	TI_UINT32            aQueueMaxSize[MAX_NUM_OF_AC]; /* indicates the max size of each Data queue */
	TI_UINT32            aTxSendPaceThresh[MAX_NUM_OF_AC]; /* Number of packets to queue before scheduling Tx handling */
	TI_HANDLE            aQueues[MAX_NUM_OF_AC];  /* The Tx aQueues handles */
    TI_BOOL	             aQueueBusy[MAX_NUM_OF_AC]; /* per queue busy indication */
	TI_UINT32            uLastQueId; /* the last queue processed by the scheduler */				
	TI_BOOL				 aNetStackQueueStopped[MAX_NUM_OF_AC];/*indicate if the current queue was full and caused Tx network stack stop*/
	TI_BOOL				 bStopNetStackTx;/*Flag to enable/disable Tx stop*/

	/* Counters */
	TTxDataQueueDebugCnt aQueueCounters[MAX_NUM_OF_AC]; /* Save Tx statistics per Tx-queue. */
	TI_UINT32			 uClsfrMismatchCount;
	TI_UINT32			 uTxSendPaceTimeoutsCount;

} TTxDataQ;


/* TIDs Mapping to Queues */
static const TI_UINT32 aTidToQueueTable[MAX_NUM_OF_802_1d_TAGS] = {0, 1, 1, 0, 2, 2, 3, 3};


#endif /* _TX_DATA_QUEUE_H_ */
