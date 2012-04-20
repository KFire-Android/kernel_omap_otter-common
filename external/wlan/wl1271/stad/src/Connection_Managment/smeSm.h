/*
 * smeSm.h
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

/** \file   smeSm.h
 *  \brief SME SM internal header file
 *
 *  \see smeSm.c
 */

#ifndef __SME_SM_H__
#define __SME_SM_H__

#include "GenSM.h"

void sme_SmEvent(TI_HANDLE hGenSm, TI_UINT32 uEvent, void* pData);

typedef enum
{
    SME_SM_STATE_IDLE = 0,
    SME_SM_STATE_WAIT_CONNECT,
    SME_SM_STATE_SCANNING,
    SME_SM_STATE_CONNECTING,
    SME_SM_STATE_CONNECTED,
    SME_SM_STATE_DISCONNECTING,
    SME_SM_NUMBER_OF_STATES
} ESmeSmStates;

typedef enum
{
    SME_SM_EVENT_START = 0,
    SME_SM_EVENT_STOP,
    SME_SM_EVENT_CONNECT,
    SME_SM_EVENT_CONNECT_SUCCESS,
    SME_SM_EVENT_CONNECT_FAILURE,
    SME_SM_EVENT_DISCONNECT,
    SME_SM_NUMBER_OF_EVENTS
} ESmeSmEvents;

extern TGenSM_actionCell tSmMatrix[ SME_SM_NUMBER_OF_STATES ][ SME_SM_NUMBER_OF_EVENTS ];
extern TI_INT8*  uStateDescription[];
extern TI_INT8*  uEventDescription[];

#endif /* __SME_SM_H__ */


