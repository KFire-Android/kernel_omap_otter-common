/*
 * RxXfer.h
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
 *   MODULE:  rxXfer.h
 *
 *   PURPOSE: Rx Xfer module header file.
 * 
 ****************************************************************************/

#ifndef _RX_XFER_H
#define _RX_XFER_H

#include "rxXfer_api.h"
#include "TwIf.h"


#define RX_DESCRIPTOR_SIZE (sizeof(RxIfDescriptor_t))
#ifdef PLATFORM_SYMBIAN	/* UMAC is using only one buffer and therefore we can't use consecutive reads */
    #define MAX_CONSECUTIVE_READS    1
#else
    #define MAX_CONSECUTIVE_READS    8
#endif

#define MAX_PACKETS_NUMBER  8
#define MAX_CONSECUTIVE_READ_TXN   16
/* Max Txn size */
#define MAX_PACKET_SIZE     4096

typedef struct 
{
    TI_UINT32      numPacketsRead;
    TI_UINT32      numBytesRead;
    TI_UINT32      numPacketsDroppedNoMem;
    TI_UINT32      numPacketsDroppedPacketIDMismatch;
    TI_UINT32      counters;
    TI_UINT32      numAck0;
} RxXferStats_T;


typedef struct 
{
    TTxnStruct              tTxnStruct;
    TI_UINT32               uRegData; 
    TI_UINT32               uRegAdata; 

} TRegTxn;

typedef struct 
{
    TTxnStruct              tTxnStruct;
    TI_UINT32               uCounter; 
    
} TCounterTxn;

typedef struct 
{
    TTxnStruct              tTxnStruct;
    TI_UINT32               uData; 
    
} TDummyTxn;

typedef struct 
{
    TI_HANDLE               hOs;
    TI_HANDLE               hReport;
    TI_HANDLE               hTwIf;
    TI_HANDLE               hFwEvent;
    TI_HANDLE               hRxQueue;

    TI_UINT32               aRxPktsDesc[NUM_RX_PKT_DESC]; /* Array of Rx packets short descriptors (see RX_DESC_SET/GET...) */
    TI_UINT32               uFwRxCntr;
    TI_UINT32               uDrvRxCntr;

    TI_UINT32               uPacketMemoryPoolStart;

    TRequestForBufferCb     RequestForBufferCB;
    TI_HANDLE               RequestForBufferCB_handle;  

    TRegTxn                 aSlaveRegTxn[MAX_CONSECUTIVE_READ_TXN];
    TTxnStruct              aTxnStruct[MAX_CONSECUTIVE_READ_TXN];
    TCounterTxn             aCounterTxn[MAX_CONSECUTIVE_READ_TXN];
    TDummyTxn               aDummyTxn[MAX_CONSECUTIVE_READ_TXN];
    TI_BOOL                 bChipIs1273Pg10;

    TI_UINT8                aTempBuffer[MAX_PACKET_SIZE];

    TI_UINT32               uAvailableTxn;
    
#ifdef TI_DBG
    RxXferStats_T           DbgStats;
#endif /* TI_DBG */

} RxXfer_t;


#endif /* _RX_XFER_H */
