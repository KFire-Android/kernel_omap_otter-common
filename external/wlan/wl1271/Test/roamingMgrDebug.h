/*
 * roamingMgrDebug.h
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

#ifndef __ROAMIN_MGR_DEBUG_H__
#define __ROAMIN_MGR_DEBUG_H__

#include "tidef.h"
#include "paramOut.h"

#define ROAMING_MGR_DEBUG_HELP_MENU 			0
#define PRINT_ROAMING_STATISTICS				1
#define RESET_ROAMING_STATISTICS				2
#define PRINT_ROAMING_CURRENT_STATUS			3
#define PRINT_ROAMING_CANDIDATE_TABLE			4
#define TRIGGER_ROAMING_LOW_QUALITY_EVENT		5
#define TRIGGER_ROAMING_BSS_LOSS_EVENT			6 
#define TRIGGER_ROAMING_SWITCH_CHANNEL_EVENT	7 
#define TRIGGER_ROAMING_AP_DISCONNECT_EVENT		8 
#define TRIGGER_ROAMING_CONNECT_EVENT			9 	
#define TRIGGER_ROAMING_NOT_CONNECTED_EVENT		10
#define TRIGGER_ROAMING_HANDOVER_SUCCESS_EVENT	11
#define TRIGGER_ROAMING_HANDOVER_FAILURE_EVENT	12

/* Added for EMP project */
#define ROAMING_REGISTER_BSS_LOSS_EVENT         13
#define ROAMING_START_IMMEDIATE_SCAN            14
#define ROAMING_CONNECT                         15
#define ROAMING_START_CONT_SCAN_BY_APP          16
#define ROAMING_STOP_CONT_SCAN_BY_APP           17
#define RAOMING_SET_DEFAULT_SCAN_POLICY         18
#define ROAMING_PRINT_MANUAL_MODE               19

void roamingMgrDebugFunction(TI_HANDLE hRoamingMngr, 
					   TI_UINT32	funcType, 
					   void		*pParam);


#endif /* __ROAMIN_MGR_DEBUG_H__ */
