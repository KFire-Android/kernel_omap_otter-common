/*
 * connInfra.h
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

/** \file connInfra.h
 *  \brief Infra connection header file
 *
 *  \see connInfra.c
 */

/***************************************************************************/
/*																									*/
/*	  MODULE:	infraConn.h																*/
/*    PURPOSE:	Infrastructure connection header file			 								*/
/*																									*/
/***************************************************************************/
#ifndef __CONN_INFRA_H__
#define __CONN_INFRA_H__

#include "tidef.h"
#include "paramOut.h"
#include "conn.h"

/* Infra connection SM events */
typedef enum 
{
	CONN_INFRA_CONNECT				= 0,
	CONN_INFRA_SCR_SUCC ,
    CONN_INFRA_JOIN_CMD_CMPLT ,
	CONN_INFRA_DISCONNECT ,
	CONN_INFRA_MLME_SUCC ,
	CONN_INFRA_RSN_SUCC	,
	CONN_INFRA_HW_CONFIGURED ,
	CONN_INFRA_DISCONN_COMPLETE ,
    CONN_INFRA_NUM_EVENTS			
} connInfraEvent_e;

/* Infra connection states */
typedef enum
{
	STATE_CONN_INFRA_IDLE	                 = 0,
	STATE_CONN_INFRA_SCR_WAIT_CONN			 = 1,
    STATE_CONN_INFRA_WAIT_JOIN_CMPLT         = 2,
	STATE_CONN_INFRA_MLME_WAIT			     = 3,
	STATE_CONN_INFRA_RSN_WAIT			     = 4,
	STATE_CONN_INFRA_CONFIG_HW 		         = 5,
    STATE_CONN_INFRA_CONNECTED			     = 6,
    STATE_CONN_INFRA_SCR_WAIT_DISCONN   	 = 7,
    STATE_CONN_INFRA_WAIT_DISCONNECT         = 8,
    CONN_INFRA_NUM_STATES
} infra_state_e;

TI_STATUS conn_infraConfig(conn_t *pConn);

TI_STATUS conn_infraSMEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hConn);

TI_STATUS connInfra_JoinCmpltNotification(TI_HANDLE hconn);

void connInfra_DisconnectComplete (conn_t *pConn, TI_UINT8  *data, TI_UINT8   dataLength);

#endif /* __CONN_INFRA_H__ */
