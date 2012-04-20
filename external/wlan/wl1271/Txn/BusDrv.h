/*
 * BusDrv.h
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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


/** \file   BusDrv.h 
 *  \brief  Bus-Driver module API definition                                  
 *
 *  \see    SdioBusDrv.c, WspiBusDrv.c
 */

#ifndef __BUS_DRV_API_H__
#define __BUS_DRV_API_H__


#include "TxnDefs.h"
#include "queue.h"


/************************************************************************
 * Defines
 ************************************************************************/

#define WSPI_PAD_LEN_WRITE          4
#define WSPI_PAD_LEN_READ           8                    
#define MAX_XFER_BUFS               4

#define TXN_PARAM_STATUS_OK         0
#define TXN_PARAM_STATUS_ERROR      1
#define TXN_PARAM_STATUS_RECOVERY   2

#define TXN_DIRECTION_WRITE         0
#define TXN_DIRECTION_READ          1

#define TXN_HIGH_PRIORITY           0
#define TXN_LOW_PRIORITY            1
#define TXN_NUM_PRIORITYS           2

#define TXN_INC_ADDR                0
#define TXN_FIXED_ADDR              1

#define TXN_AGGREGATE_OFF           0
#define TXN_AGGREGATE_ON            1

#define TXN_NON_SLEEP_ELP           1
#define TXN_SLEEP_ELP               0

#define NUM_OF_PARTITION            4

/************************************************************************
 * Macros
 ************************************************************************/
/* Get field from TTxnStruct->uTxnParams */
#define TXN_PARAM_GET_PRIORITY(pTxn)            ( (pTxn->uTxnParams & 0x00000003) >> 0 )
#define TXN_PARAM_GET_FUNC_ID(pTxn)             ( (pTxn->uTxnParams & 0x0000000C) >> 2 )
#define TXN_PARAM_GET_DIRECTION(pTxn)           ( (pTxn->uTxnParams & 0x00000010) >> 4 )
#define TXN_PARAM_GET_FIXED_ADDR(pTxn)          ( (pTxn->uTxnParams & 0x00000020) >> 5 )
#define TXN_PARAM_GET_MORE(pTxn)                ( (pTxn->uTxnParams & 0x00000040) >> 6 )
#define TXN_PARAM_GET_SINGLE_STEP(pTxn)         ( (pTxn->uTxnParams & 0x00000080) >> 7 )
#define TXN_PARAM_GET_STATUS(pTxn)              ( (pTxn->uTxnParams & 0x00000F00) >> 8 )
#define TXN_PARAM_GET_AGGREGATE(pTxn)           ( (pTxn->uTxnParams & 0x00001000) >> 12 )
#define TXN_PARAM_GET_END_OF_BURST(pTxn)        ( (pTxn->uTxnParams & 0x00002000) >> 13 )



/* Set field in TTxnStruct->uTxnParams */
#define TXN_PARAM_SET_PRIORITY(pTxn, uValue)    ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00000003) | (uValue << 0 ) )
#define TXN_PARAM_SET_FUNC_ID(pTxn, uValue)     ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x0000000C) | (uValue << 2 ) )
#define TXN_PARAM_SET_DIRECTION(pTxn, uValue)   ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00000010) | (uValue << 4 ) )
#define TXN_PARAM_SET_FIXED_ADDR(pTxn, uValue)  ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00000020) | (uValue << 5 ) )
#define TXN_PARAM_SET_MORE(pTxn, uValue)        ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00000040) | (uValue << 6 ) )
#define TXN_PARAM_SET_SINGLE_STEP(pTxn, uValue) ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00000080) | (uValue << 7 ) )
#define TXN_PARAM_SET_STATUS(pTxn, uValue)      ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00000F00) | (uValue << 8 ) )
#define TXN_PARAM_SET_AGGREGATE(pTxn, uValue)   ( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00001000) | (uValue << 12 ) )
#define TXN_PARAM_SET_END_OF_BURST(pTxn, uValue)( pTxn->uTxnParams = (pTxn->uTxnParams & ~0x00002000) | (uValue << 13 ) )


#define TXN_PARAM_SET(pTxn, uPriority, uId, uDirection, uAddrMode) \
        TXN_PARAM_SET_PRIORITY(pTxn, uPriority); \
        TXN_PARAM_SET_FUNC_ID(pTxn, uId); \
        TXN_PARAM_SET_DIRECTION(pTxn, uDirection); \
        TXN_PARAM_SET_FIXED_ADDR(pTxn, uAddrMode);

#define BUILD_TTxnStruct(pTxn, uAddr, pBuf, uLen, fCB, hCB) \
    pTxn->aBuf[0] = (TI_UINT8*)(pBuf); \
    pTxn->aLen[0] = (TI_UINT16)(uLen); \
    pTxn->aLen[1] = 0;                 \
    pTxn->uHwAddr = uAddr;             \
    pTxn->hCbHandle = (void*)hCB;      \
    pTxn->fTxnDoneCb = (TTxnDoneCb)fCB;


/************************************************************************
 * Types
 ************************************************************************/
/* The TxnDone CB called by the bus driver upon Async Txn completion */
typedef void (*TBusDrvTxnDoneCb)(TI_HANDLE hCbHandle, void *pTxn);

/* The TxnDone CB called by the TxnQueue upon Async Txn completion */
typedef void (*TTxnQueueDoneCb)(TI_HANDLE hCbHandle, void *pTxn);

/* The TxnDone CB of the specific Txn originator (Xfer layer) called upon Async Txn completion */
typedef void (*TTxnDoneCb)(TI_HANDLE hCbHandle, void *pTxn);

/* The transactions structure */
typedef struct
{
    TQueNodeHdr  tTxnQNode;                /* Header for queueing */
    TI_UINT32    uTxnParams;               /* Txn attributes (bit fields) - see macros above */
    TI_UINT32    uHwAddr;                  /* Physical (32 bits) HW Address */
    TTxnDoneCb   fTxnDoneCb;               /* CB called by TwIf upon Async Txn completion (may be NULL) */
    TI_HANDLE    hCbHandle;                /* The handle to use when calling fTxnDoneCb */
    TI_UINT16    aLen[MAX_XFER_BUFS];      /* Lengths of the following aBuf data buffers respectively.
                                              Zero length marks last used buffer, or MAX_XFER_BUFS of all are used. */
    TI_UINT8*    aBuf[MAX_XFER_BUFS];      /* Host data buffers to be written to or read from the device */
    TI_UINT8     aWspiPad[WSPI_PAD_LEN_READ]; /* Padding used by WSPI bus driver for its header or fixed-busy bytes */
} TTxnStruct; 

/* Parameters for all bus types configuration in ConnectBus process */

typedef struct
{
    TI_UINT32    uBlkSizeShift;
    TI_UINT32    uBusDrvThreadPriority;
} TSdioCfg; 

typedef struct
{
    TI_UINT32    uDummy;
} TWspiCfg; 

typedef struct
{
    TI_UINT32    uBaudRate;
} TUartCfg; 

typedef union
{
    TSdioCfg    tSdioCfg;       
    TWspiCfg    tWspiCfg;       
    TUartCfg    tUartCfg;       

} TBusDrvCfg;


typedef struct
{
    TI_UINT32   uMemAdrr;
    TI_UINT32   uMemSize;
} TPartition;


/************************************************************************
 * Functions
 ************************************************************************/
TI_HANDLE   busDrv_Create     (TI_HANDLE hOs);
TI_STATUS   busDrv_Destroy    (TI_HANDLE hBusDrv);
void        busDrv_Init       (TI_HANDLE hBusDrv, TI_HANDLE hReport);
TI_STATUS   busDrv_ConnectBus (TI_HANDLE        hBusDrv, 
                               TBusDrvCfg       *pBusDrvCfg,
                               TBusDrvTxnDoneCb fCbFunc,
                               TI_HANDLE        hCbArg,
                               TBusDrvTxnDoneCb fConnectCbFunc,
                               TI_UINT32        *pRxDmaBufLen,
                               TI_UINT32        *pTxDmaBufLen);
TI_STATUS   busDrv_DisconnectBus (TI_HANDLE hBusDrv);
ETxnStatus  busDrv_Transact   (TI_HANDLE hBusDrv, TTxnStruct *pTxn);


#endif /*__BUS_DRV_API_H__*/

