/*
 * connIbss.h
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

/** \file connIbss.h
 *  \brief IBSS connection header file
 *
 *  \see connIbss.c
 */

/***************************************************************************/
/*																									*/
/*	  MODULE:	ibssConn.h																*/
/*    PURPOSE:	IBSS connection header file			 								*/
/*																									*/
/***************************************************************************/
#ifndef __CONN_IBSS_H__
#define __CONN_IBSS_H__

#include "tidef.h"
#include "paramOut.h"
#include "conn.h"

/* IBSS connection SM events */
typedef enum 
{
	CONN_IBSS_CREATE   		= 0,  /* Sent when establishing new IBSS. */ 
	CONN_IBSS_CONNECT		  ,   /* Sent when trying to join existing IBSS. */
	CONN_IBSS_DISCONNECT	  ,	  /* Stops to connection */
	CONN_IBSS_RSN_SUCC		  ,   /* RSN keys are set to the HW */
	CONN_IBSS_STA_JOINED	  ,   /* Event sent when other STA joined our self IBSS */
	CONN_IBSS_MERGE			  ,
	CONN_IBSS_DISCONN_COMPLETE,   /* Called when disconncet event is done */ 
    CONN_IBSS_NUM_EVENTS            
} connIbssEvent_e;

/* IBSS connection states */
typedef enum
{
	STATE_CONN_IBSS_IDLE 		=0,
 
	STATE_CONN_IBSS_SELF_WAIT	,
	STATE_CONN_IBSS_RSN_WAIT	,
	STATE_CONN_IBSS_CONNECTED 	,
	STATE_CONN_IBSS_WAIT_DISCONN_CMPLT,
    CONN_IBSS_NUM_STATES    
} conn_ibss_state_e;


TI_STATUS conn_ibssConfig(conn_t *pConn);

TI_STATUS conn_ibssSMEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hConn);

void IbssConnSM_ScrCB( TI_HANDLE hConn, EScrClientRequestStatus requestStatus, EScrResourceId eResource, EScePendReason pendReason);

void connIbss_DisconnectComplete (conn_t *pConn, TI_UINT8  *data, TI_UINT8   dataLength);

#endif /* __CONN_IBSS_H__ */
