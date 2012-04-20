/*
 * rsnDbg.h
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


#ifndef __RSN_DBG_H__
#define __RSN_DBG_H__

/* definitions for ctrl dbg */

#define DBG_RSN_PRINT_HELP				0

#define DBG_RSN_SET_DEF_KEY_ID			2
#define DBG_RSN_SET_DESIRED_AUTH		3
#define DBG_RSN_SET_DESIRED_CIPHER		4

#define DBG_RSN_GEN_MIC_FAILURE_REPORT	6
#define DBG_RSN_GET_PARAM_802_11_CAPABILITY  7
#define DBG_RSN_GET_PMKID_CACHE              8
#define DBG_RSN_RESET_PMKID_CACHE            9
#define DBG_RSN_PRINT_ROGUE_AP_TABLE         10
#define DBG_RSN_SET_PORT_STATUS              11
#define DBG_RSN_PRINT_PORT_STATUS            12

void rsnDebugFunction(TI_HANDLE hRsn, TI_UINT32 funcType, void *pParam);


#endif /* __RSN_DBG_H__*/

