/*
 * measurementDbg.h
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


#ifndef __MEASUREMENT_DBG_H__
#define __MEASUREMENT_DBG_H__

/* definitions for ctrl dbg */

#define DBG_MEASUREMENT_PRINT_HELP		    	0
#define DBG_MEASUREMENT_PRINT_STATUS			1
#define	DBG_MEASUREMENT_CHANNEL_LOAD_START  	2 
#define	DBG_MEASUREMENT_CHANNEL_LOAD_STOP	    3 
#define DBG_MEASUREMENT_SEND_FRAME_REQUEST      4 
#define DBG_MEASUREMENT_START_NOISE_HIST        5 
#define DBG_MEASUREMENT_STOP_NOISE_HIST         6 
#define DBG_MEASUREMENT_GET_NOISE_HIST_RESULTS  7 
#define DBG_MEASUREMENT_SEND_CHANNEL_LOAD_FRAME 8 
#define DBG_MEASUREMENT_SEND_BEACON_TABLE_FRAME 9
#define DBG_MEASUREMENT_SEND_NOISE_HIST_1_FRAME 10
#define DBG_MEASUREMENT_SEND_NOISE_HIST_2_FRAME 11
#define DBG_MEASUREMENT_SET_TRAFFIC_THRSLD      12
#define DBG_SC_PRINT_STATUS					    30
#define DBG_SC_SET_SWITCH_CHANNEL_NUM			31
#define DBG_SC_SET_SWITCH_CHANNEL_TBTT			32
#define DBG_SC_SET_SWITCH_CHANNEL_MODE         33
#define DBG_SC_SET_CHANNEL_AS_VALID			    34
#define DBG_SC_SET_CHANNEL_AS_INVALID			35
#define DBG_SC_SWITCH_CHANNEL_CMD				36
#define DBG_SC_CANCEL_SWITCH_CHANNEL_CMD		37

#define DBG_REG_DOMAIN_PRINT_VALID_CHANNELS		50




void measurementDebugFunction(TI_HANDLE hMeasurementMgr, TI_HANDLE hSwitchChannel, TI_HANDLE hRegulatoryDomain, TI_UINT32 funcType, void *pParam);

void measurement_channelLoadCallBackDbg(TI_HANDLE hMeasurementMgr, TI_STATUS status, 
                                        TI_UINT8* CB_buf);

void measurement_noiseHistCallBackDbg(TI_HANDLE hMeasurementMgr, TI_STATUS status, 
                                      TI_UINT8* CB_buf);

#endif /* __MEASUREMENT_DBG_H__*/
