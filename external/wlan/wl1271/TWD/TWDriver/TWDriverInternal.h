/*
 * TWDriverInternal.h
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



/** \file TWDriverInternal.h
 *  \brief TWD Driver internal common internal declarations
 *
 *  \see TWDriver.h
 */


#ifndef TWDRIVERINTERNAL_H
#define TWDRIVERINTERNAL_H


#include "TWDriver.h"
#include "Device.h"


/* Shift factor to conver between TU (1024 uSec) and uSec. */
#define SHIFT_BETWEEN_TU_AND_USEC       10  

/* keep-alive templates definitions */
#define KLV_MAX_TMPL_NUM                4

/* Definitions for Rx Filter MIB */

/* Set A—Enable: Forward all frames to host driver */
#define MIB_RX_FILTER_PROMISCOUS_SET    0x01       
/* Cleared A— Disable: Do not orward all frames to the host driver */
#define MIB_RX_FILTER_PROMISCOUS_CLEAR  0x00       
/* Set A—filter enabled: receive only those frames that match the BSSID given in the Join command */
#define MIB_RX_FILTER_BSSID_SET         0x02       
/* Cleared A—filter disabled: ignore BSSID in receiving */
#define MIB_RX_FILTER_BSSID_CLEAR       0x00       

/* Asynchronous init mode callback function type */
typedef void (*fnotify_t)(TI_HANDLE module, TI_STATUS status);

        /* Callback function definition for EndOfRecovery */
typedef void (*TEndOfRecoveryCb) (TI_HANDLE handle);


/* TWD Driver Structure */
typedef struct
{
    TI_HANDLE           hOs;
    TI_HANDLE           hUser;
    TI_HANDLE           hReport;
    TI_HANDLE           hTimer;
    TI_HANDLE           hContext;
    TI_HANDLE           hMacServices;
    TI_HANDLE           hTxCtrlBlk;
    TI_HANDLE           hTxHwQueue;
    TI_HANDLE           hHwIntr;
    TI_HANDLE           hHealthMonitor;
    TI_HANDLE           hTwIf;
    TI_HANDLE           hTxnQ;
    TI_HANDLE           hCmdQueue;
    TI_HANDLE           hCmdBld;
    TI_HANDLE           hTxXfer;
    TI_HANDLE           hTxResult;
    TI_HANDLE           hRxXfer;
    TI_HANDLE           hFwEvent;
    TI_HANDLE           hHwInit;
    TI_HANDLE           hCmdMbox;
    TI_HANDLE           hEventMbox;
    TI_HANDLE           hFwDbg;
    TI_HANDLE           hRxQueue;
    
    /* If true it means that we are in recovery process */  
    TI_BOOL             bRecoveryEnabled;

    /* Init success flag */
    TI_BOOL             bInitSuccess;

    ReadWriteCommand_t  tPrintRegsBuf;    

    /* Init/Recovery/Stop callbacks */
    TTwdCallback        fInitHwCb;
    TTwdCallback        fInitFwCb;
    TTwdCallback        fConfigFwCb;
    TTwdCallback        fStopCb;
    TTwdCallback        fInitFailCb;

    TFailureEventCb     fFailureEventCb;
    TI_HANDLE          	hFailureEventCb;

    TI_UINT32           uNumMboxFailures;

#ifdef TI_DBG  /* Just for debug. */
    /* Packets sequence counter (common for all queues). */
    TI_UINT32           dbgPktSeqNum;                               
    /* Tx counters per queue:*/
    /* Count packets sent from upper driver. */
    TI_UINT32           dbgCountSentPackets[MAX_NUM_OF_AC];  
    /* Count packets sent and queue not busy. */
    TI_UINT32           dbgCountQueueAvailable[MAX_NUM_OF_AC];
    /* Count XferDone return values from Xfer. */
    TI_UINT32           dbgCountXferDone[MAX_NUM_OF_AC];     
    /* Count Success return values from Xfer. */
    TI_UINT32           dbgCountXferSuccess[MAX_NUM_OF_AC];  
    /* Count Pending return value from Xfer. */
    TI_UINT32           dbgCountXferPending[MAX_NUM_OF_AC];  
    /* Count Error return value from Xfer. */
    TI_UINT32           dbgCountXferError[MAX_NUM_OF_AC];    
    /* Count XferDone callback calls. */
    TI_UINT32           dbgCountXferDoneCB[MAX_NUM_OF_AC];   
    /* Count TxComplete callback calls. */
    TI_UINT32           dbgCountTxCompleteCB[MAX_NUM_OF_AC]; 

    MemoryMap_t         MemMap;
    ACXStatistics_t     acxStatistic;
#endif

	TTestCmdCB	fRadioCb;
	void		*pRadioCb;
	TI_HANDLE	hRadioCb;
	TTestCmd 	testCmd; 

} TTwd;


/* External Functions Prototypes */

void  SendPacketTransfer (TI_HANDLE         hUser,  
                          TI_UINT32         aPacketId);
void  SendPacketComplete (TI_HANDLE         hUser, 
                          TI_STATUS         aStatus, 
                          TI_UINT32         aPacketId, 
                          TI_UINT32         aRate, 
                          TI_UINT8          aAckFailures, 
                          TI_UINT32         durationInAir, 
                          TI_UINT32         fwHandlingTime, 
                          TI_UINT32         mediumDelay);




#endif /* TNETW_DRIVER_H */

