/*
 * measurementMgrSM.h
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
/*	  MODULE:	measurementMgrSM.h										   */
/*    PURPOSE:	measurement Manager module State Machine header file       */
/*																		   */
/***************************************************************************/

#ifndef __MEASUREMENTMGRSM_H__
#define __MEASUREMENTMGRSM_H__

#include "fsm.h"



/** State machine states */
typedef enum 
{
	MEASUREMENTMGR_STATE_IDLE					= 0,
    MEASUREMENTMGR_STATE_PROCESSING_REQUEST		= 1,
    MEASUREMENTMGR_STATE_WAITING_FOR_SCR		= 2,
    MEASUREMENTMGR_STATE_MEASURING				= 3,
	MEASUREMENTMGR_STATE_LAST					= 4
} measurementMgrSM_States;


/** State machine events */
typedef enum 
{
	MEASUREMENTMGR_EVENT_CONNECTED				= 0,
	MEASUREMENTMGR_EVENT_DISCONNECTED			= 1,
	MEASUREMENTMGR_EVENT_ENABLE					= 2,
	MEASUREMENTMGR_EVENT_DISABLE				= 3,
    MEASUREMENTMGR_EVENT_FRAME_RECV				= 4,
    MEASUREMENTMGR_EVENT_SEND_REPORT			= 5,
	MEASUREMENTMGR_EVENT_REQUEST_SCR			= 6,
    MEASUREMENTMGR_EVENT_SCR_WAIT				= 7,
    MEASUREMENTMGR_EVENT_SCR_RUN				= 8,
    MEASUREMENTMGR_EVENT_ABORT					= 9,
    MEASUREMENTMGR_EVENT_COMPLETE				= 10,
    MEASUREMENTMGR_EVENT_FW_RESET				= 11,
	MEASUREMENTMGR_EVENT_LAST					= 12
} measurementMgrSM_Events;


#define MEASUREMENTMGR_NUM_STATES		MEASUREMENTMGR_STATE_LAST	 
#define MEASUREMENTMGR_NUM_EVENTS		MEASUREMENTMGR_EVENT_LAST	




TI_STATUS measurementMgrSM_config(TI_HANDLE hMeasurementMgr);

TI_STATUS measurementMgrSM_event(TI_UINT8 * currentState, TI_UINT8 event, TI_HANDLE hMeasurementMgr);


#endif /* __MEASUREMENTMGRSM_H__*/
