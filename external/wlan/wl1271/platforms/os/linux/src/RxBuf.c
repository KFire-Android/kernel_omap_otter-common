/*
 * RxBuf.c
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


/** \file  buf.c
 *  \brief Linux buf implementation.
 *
 *  \see   
 */

#include "tidef.h"
#include "RxBuf_linux.h"
#include <linux/netdevice.h>
/*--------------------------------------------------------------------------------------*/
/* 
 * Allocate BUF Rx packets.
 * Add 16 bytes before the data buffer for WSPI overhead!
 */
void *RxBufAlloc(TI_HANDLE hOs, TI_UINT32 len,PacketClassTag_e ePacketClassTag)
{
	TI_UINT32 alloc_len = len + WSPI_PAD_BYTES + PAYLOAD_ALIGN_PAD_BYTES + RX_HEAD_LEN_ALIGNED;
	struct sk_buff *skb;
	rx_head_t *rx_head;
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;

	skb = alloc_skb(alloc_len, flags);
	if (!skb)
	{
		return NULL;
	}
	rx_head = (rx_head_t *)skb->head;
	rx_head->skb = skb;
	skb_reserve(skb, RX_HEAD_LEN_ALIGNED + WSPI_PAD_BYTES);
/*
	printk("-->> RxBufAlloc(len=%d)  skb=0x%x skb->data=0x%x skb->head=0x%x skb->len=%d\n",
		   (int)len, (int)skb, (int)skb->data, (int)skb->head, (int)skb->len);
*/
	return skb->data;
    
}

/*--------------------------------------------------------------------------------------*/

inline void RxBufFree(TI_HANDLE hOs, void *pBuf)
{
	unsigned char  *pdata   = (unsigned char *)((TI_UINT32)pBuf & ~(TI_UINT32)0x3);
	rx_head_t      *rx_head = (rx_head_t *)(pdata -  WSPI_PAD_BYTES - RX_HEAD_LEN_ALIGNED);
	struct sk_buff *skb     = rx_head->skb;

#ifdef TI_DBG
	if ((TI_UINT32)pBuf & 0x3)
	{
		if ((TI_UINT32)pBuf - (TI_UINT32)skb->data != 2)
		{
			printk("RxBufFree() address error skb=0x%x skb->data=0x%x pPacket=0x%x !!!\n",(int)skb, (int)skb->data, (int)pBuf);
		}
	}
	else
	{
		if ((TI_UINT32)skb->data != (TI_UINT32)pBuf)
		{
			printk("RxBufFree() address error skb=0x%x skb->data=0x%x pPacket=0x%x !!!\n",(int)skb, (int)skb->data, (int)pBuf);
		}
	}
#endif	  
/*
	printk("-->> RxBufFree()  skb=0x%x skb->data=0x%x skb->head=0x%x skb->len=%d\n",
		   (int)skb, (int)skb->data, (int)skb->head, (int)skb->len);
*/
	dev_kfree_skb(skb);
}
