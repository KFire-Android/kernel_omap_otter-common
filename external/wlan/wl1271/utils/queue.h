/*
 * queue.h
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



/** \file   queue.h 
 *  \brief  queue module header file.                                  
 *
 *  \see    queue.c
 */


#ifndef _QUEUE_H_
#define _QUEUE_H_


#define QUE_UNLIMITED_SIZE      0xFFFFFFFF

/* A queue node header structure */                        
typedef struct _TQueNodeHdr 
{
    struct _TQueNodeHdr *pNext;
    struct _TQueNodeHdr *pPrev;
} TQueNodeHdr;



/* External Functions Prototypes */
/* ============================= */
TI_HANDLE que_Create  (TI_HANDLE hOs, TI_HANDLE hReport, TI_UINT32 uLimit, TI_UINT32 uNodeHeaderOffset);
TI_STATUS que_Destroy (TI_HANDLE hQue);
TI_STATUS que_Enqueue (TI_HANDLE hQue, TI_HANDLE hItem);
TI_HANDLE que_Dequeue (TI_HANDLE hQue);
TI_STATUS que_Requeue (TI_HANDLE hQue, TI_HANDLE hItem);
TI_UINT32 que_Size    (TI_HANDLE hQue);

#ifdef TI_DBG
void      que_Print   (TI_HANDLE hQue);
#endif /* TI_DBG */



#endif  /* _QUEUE_H_ */
