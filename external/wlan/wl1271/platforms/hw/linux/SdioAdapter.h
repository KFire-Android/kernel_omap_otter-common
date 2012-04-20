/*
 * SdioAdapter.h
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

/** \file   SdioAdapter.h 
 *  \brief  SDIO adapter module API definition                                  
 *
 *  \see    SdioAdapter.c
 */

#ifndef __SDIO_ADAPT_API_H__
#define __SDIO_ADAPT_API_H__


#include "TxnDefs.h"


/************************************************************************
 * Defines
 ************************************************************************/

/************************************************************************
 * Types
 ************************************************************************/

/************************************************************************
 * Functions
 ************************************************************************/
/** \brief	sdioAdapt_ConnectBus: Init SDIO driver and HW
 * 
 * \param  fCbFunc       - The bus driver's callback upon async transaction completion
 * \param  hCbArg        - The CB function handle
 * \param  uBlkSizeShift - In block-mode:   BlkSize = (1 << uBlkSizeShift)
 * \param  uSdioThreadPriority - The SDIO interrupt handler thread priority
 * \param  pRxDmaBufAddr - Pointer for providing the Rx DMA buffer address to the upper layers to use it directly
 * \param  pRxDmaBufLen  - The Rx DMA buffer length in bytes
 * \param  pTxDmaBufAddr - Pointer for providing the Tx DMA buffer address to the upper layers to use it directly
 * \param  pTxDmaBufLen  - The Tx DMA buffer length in bytes
 * \return 0 = OK, otherwise = error
 * 
 * \par Description
 * Called by BusDrv to initialize the SDIO driver and HW.
 *
 * \sa
 */ 
int        sdioAdapt_ConnectBus    (void *        fCbFunc,
                                    void *        hCbArg,
                                    unsigned int  uBlkSizeShift,
                                    unsigned int  uSdioThreadPriority,
                                    unsigned char **pRxDmaBufAddr,
                                    unsigned int  *pRxDmaBufLen,
                                    unsigned char **pTxDmaBufAddr,
                                    unsigned int  *pTxDmaBufLen);

/** \brief	sdioAdapt_DisconnectBus: Disconnect SDIO driver
 * 
 * \param  void
 * \return 0 = OK, otherwise = error
 * 
 * \par Description
 * Called by BusDrv. Disconnect the SDIO driver.
 *
 * \sa
 */ 
int        sdioAdapt_DisconnectBus (void);
/** \brief	sdioAdapt_Transact: Process transaction
 * 
 * \param  uFuncId    - SDIO function ID (1- BT, 2 - WLAN)
 * \param  uHwAddr    - HW address where to write the data
 * \param  pHostAddr  - The data buffer to write from or read into
 * \param  uLength    - The data length in bytes
 * \param  bDirection - TRUE = Read,  FALSE = Write
 * \param  bBlkMode   - If TRUE - use block mode
 * \param  bMore      - If TRUE, more transactions are expected so don't turn off any HW
 * \return COMPLETE if Txn completed in this context, PENDING if not, ERROR if failed
 *
 * \par Description
 * Called by the BusDrv module to issue an SDIO transaction.
 * Call write or read SDIO-driver function according to the direction.
 * Use Sync or Async method according to the transaction length
 * 
 * \note   It's assumed that this function is called only when idle (i.e. previous Txn is done).
 * 
 * \sa
 */ 
ETxnStatus sdioAdapt_Transact      (unsigned int  uFuncId,
                                    unsigned int  uHwAddr,
                                    void *        pHostAddr,
                                    unsigned int  uLength,
                                    unsigned int  bDirection,
                                    unsigned int  bBlkMode,
                                    unsigned int  bFixedAddr,
                                    unsigned int  bMore);
/** \brief	sdioAdapt_TransactBytes: Process bytes transaction
 * 
 * \param  uFuncId    - SDIO function ID (1- BT, 2 - WLAN)
 * \param  uHwAddr    - HW address where to write the data
 * \param  pHostAddr  - The data buffer to write from or read into
 * \param  uLength    - The data length in bytes
 * \param  bDirection - TRUE = Read,  FALSE = Write
 * \param  bMore      - If TRUE, more transactions are expected so don't turn off any HW
 * \return COMPLETE if Txn succeeded, ERROR if failed
 *
 * \par Description
 * Called by the BusDrv module to issue a bytes stream SDIO transaction.
 * Call write or read SDIO-driver Sync function according to the direction.
 * 
 * \note   It's assumed that this function is called only when idle (i.e. previous Txn is done).
 * 
 * \sa
 */ 
ETxnStatus sdioAdapt_TransactBytes (unsigned int  uFuncId,
                                    unsigned int  uHwAddr,
                                    void *        pHostAddr,
                                    unsigned int  uLength,
                                    unsigned int  bDirection,
                                    unsigned int  bMore);



#endif /*__SDIO_ADAPT_API_H__*/
