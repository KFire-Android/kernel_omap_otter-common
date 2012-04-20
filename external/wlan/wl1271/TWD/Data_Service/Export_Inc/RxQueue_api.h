/*
 * RxQueue_api.h
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



/** \file   RxQueue_api.h 
 *  \brief  RxQueue module header file.                                  
 *
 *  \see    RxQueue_api.c
 */

#ifndef _RX_QUEUE_H_
#define _RX_QUEUE_H_

/* 
 * External Functions Prototypes 
 * ============================= 
 */
TI_HANDLE RxQueue_Create        (TI_HANDLE hOs);
TI_STATUS RxQueue_Destroy       (TI_HANDLE hRxQueue);
TI_STATUS RxQueue_Init          (TI_HANDLE hRxQueue, TI_HANDLE hReport, TI_HANDLE hTimerModule);
void      RxQueue_CloseBaSession(TI_HANDLE hRxQueue, TI_UINT8 uFrameTid);
void      RxQueue_ReceivePacket (TI_HANDLE hRxQueue, const void *aFrame);
void      RxQueue_Register_CB   (TI_HANDLE hRxQueue, TI_UINT32 CallBackID, void *CBFunc, TI_HANDLE CBObj);


#endif  /* _STA_CAP_H_ */


