/*
 * RxBuf.h
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
/*	  MODULE:	buf.h												       */
/*    PURPOSE:	manages the allocation/free and field access of the BUF    */
/*																		   */
/***************************************************************************/
#ifndef _BUF_H_
#define _BUF_H_

#include "tidef.h"
#include "queue.h"
#include "public_descriptors.h"



#define WSPI_PAD_BYTES          	16     /* Add padding before data buffer for WSPI overhead */
#define PAYLOAD_ALIGN_PAD_BYTES   	4      /* Add an extra word for alignment the MAC payload in case of QoS MSDU */



/**
 * \brief Buffer for Tx/Rx packets 
 */ 
typedef void BUF, *PBUF;

/* Packet types */


/**
 * \def RX_BUF_DATA
 * \brief Macro which gets a pointer to BUF packet header and returns the pointer to the start address of the WLAN packet's data
 */
#define RX_BUF_DATA(pBuf)   ((void*)((TI_UINT8 *)pBuf + sizeof(RxIfDescriptor_t)))
/**
 * \def RX_BUF_LEN
 * \brief Macro which gets a pointer to BUF packet header and returns the buffer length (without Rx Descriptor) of the WLAN packet
 */
#define RX_BUF_LEN(pBuf)    ( (((RxIfDescriptor_t *)(pBuf))->length << 2) -  \
                              ((RxIfDescriptor_t *)(pBuf))->extraBytes -     \
                              sizeof(RxIfDescriptor_t) )

/**
 * \def RX_ETH_PKT_DATA
 * \brief Macro which gets a pointer to BUF packet header and returns the pointer to the start address of the ETH packet's data
 */
#define RX_ETH_PKT_DATA(pBuf)   *((void **)(((TI_UINT32)pBuf + sizeof(RxIfDescriptor_t) + 2) & ~3))
/**
 * \def RX_ETH_PKT_LEN
 * \brief Macro which gets a pointer to BUF packet header and returns the buffer length (without Rx Descriptor) of the ETH packet
 */
#define RX_ETH_PKT_LEN(pBuf)    *((TI_UINT32 *)(((TI_UINT32)pBuf + sizeof(RxIfDescriptor_t) + 6) & ~3))


/** \brief BUF Allocation
 * 
 * \param  hOs		- OS module object handle
 * \param  len		- Length of allocated WBUF
 * \param  ePacketClassTag	- The RX packet type (used only in EMP)
 * \return On success: Pointer to WBUF	;	Otherwise: NULL
 * 
 * \par Description
 * This function allocates BUF element for Tx/Rx packet
 * 
 * \sa
 */ 
BUF* RxBufAlloc         (TI_HANDLE hOs, TI_UINT32 len, PacketClassTag_e ePacketClassTag);


/** \brief BUF Free
 * 
 * \param  hOs		- OS module object handle
 * \param  pWbuf	- Pointer to WBUF which was previously created by user
 * \return void
 * 
 * \par Description
 * This function frees the memory allocated for BUF element
 * 
 * \sa
 */ 
void  RxBufFree          (TI_HANDLE hOs, void* pBuf);


/** \brief BUF Free
 * 
 * \param  hOs		- OS module object handle
 * \param  pWbuf	- Pointer to WBUF which was previously created by user
 * \return void
 * 
 * \par Description
 * This function increment the start address of data held in BUF element in len.
 * 
 * \sa
 */ 
void  RxBufReserve       (TI_HANDLE hOs, void* pBuf, TI_UINT32 len); 

#endif

