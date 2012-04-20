/*
 * fwdriverdebug.h
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


/* \file FW_debug.h
 *  \brief This file include private definitions for the FW debug module.
 */

#ifndef __FW_DEBUG_H__
#define __FW_DEBUG_H__


#include "tidef.h"
#include "public_infoele.h"
#include "TWDriver.h"
#include "TWDriverInternal.h"

/*
 ***********************************************************************
 *	Constant definitions.
 ***********************************************************************
 */

/* debug functions */
#define DBG_FW_PRINT_HELP					 	0
#define DBG_FW_SEND_GENERAL_TEST_CMD	    	1
#define DBG_FW_IBSS_CONNECTION	        		2
#define DBG_FW_SEND_MGMT_PACKET                	3
#define DBG_FW_SEND_DATA_PACKET	                4
#define DBG_FW_START_LOOPBACK      				5
#define DBG_FW_STOP_LOOPBACK                    6
#define DBG_FW_INFINIT_SEND	                    7
#define DBG_FW_GENERAL		                    10

/*
 ***********************************************************************
 *	Enums.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Typedefs.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Structure definitions.
 ***********************************************************************
 */
 typedef struct {
 INFO_ELE_HDR  
 unsigned char len;
 unsigned long buf[20];
 }FWDebugTestCmdParamter_t;

 typedef struct {
 INFO_ELE_HDR  
 TTestCmd     Plt; 
 }FWDebugPLT_t;

/*
 ***********************************************************************
 *	External data definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	External functions definitions
 ***********************************************************************
 */

void FWDebugFunction(TI_HANDLE hDrvMain, 
					 TI_HANDLE hOs, 
					 TI_HANDLE hTWD, 
					 TI_HANDLE hMlme, 
					 TI_HANDLE hTxMgmtQ,
					 TI_HANDLE hTxCtrl,
					 unsigned long funcType, 
					 void *pParam 
					 /*yael unsigned long packetNum*/);
void FW_debugSendGeneralTestCmd(TI_HANDLE hTWD, void *pParam);
void FW_DebugSendJoinCommand(TI_HANDLE hTWD, TI_HANDLE hTxMgmtQ);
void FW_DebugSendPacket(TI_HANDLE hDrvMain ,TI_HANDLE hOs, TI_HANDLE hTxMgmtQ, void* pParam);
void FW_DebugInfinitSendPacket(TI_HANDLE hDrvMain ,TI_HANDLE hOs);
void FW_DebugStartLoopBack (TI_HANDLE hDrvMain, TI_HANDLE hTWD);
void FW_DebugStopLoopBack (TI_HANDLE hDrvMain, TI_HANDLE hTWD);
void FW_ComparePacket (char *buf);
void printFWDbgFunctions(void);
void FW_DebugGeneral(TI_HANDLE hTWD, void *pParam);



#endif 

