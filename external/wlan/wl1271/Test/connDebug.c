/*
 * connDebug.c
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

/** \file reportReplvl.c
 *  \brief Report level implementation
 *
 *  \see reportReplvl.h
 */

/***************************************************************************/
/*																									*/
/*		MODULE:	reportReplvl.c																*/
/*    PURPOSE:	Report level implementation	 										*/
/*																									*/
/***************************************************************************/
#include "tidef.h"
#include "osApi.h"
#include "paramOut.h"
#include "connDebug.h"
#include "conn.h"
#include "connApi.h"
#include "report.h"

#ifdef REPORT_LOG
void printConnDbgFunctions(void);
#endif

void connDebugSetParam(TI_HANDLE hConn, TI_UINT32 paramType, TI_UINT32 *value)
{
#ifdef REPORT_LOG
	conn_t	*pConn = (conn_t *)hConn;

	switch (paramType)
	{
	default:
		WLAN_OS_REPORT(("Invalid param type in Set Debug Connection command: %d, curr state %d\n\n", value, pConn->state));
		break;
	}
#endif
}

void connDebugGetParam(TI_HANDLE hConn, TI_UINT32 paramType, TI_UINT32 *value)
{
#ifdef REPORT_LOG
	conn_t	*pConn = (conn_t *)hConn;

	switch (paramType)
	{
	default:
		WLAN_OS_REPORT(("Invalid param type in Get Debug Connection command: %d, curr state %d\n\n", value, pConn->state));
		break;
	}
#endif
}

void connDebugFunction(TI_HANDLE hConn, 
					   TI_UINT32	funcType, 
					   void		*pParam)
{
#ifdef REPORT_LOG
	conn_t	*pConn = (conn_t *)hConn;

	switch (funcType)
	{
	case CONN_PRINT_TEST_FUNCTIONS:
		printConnDbgFunctions();
		break;

	case CONN_PRINT_TEST:
		WLAN_OS_REPORT(("Connection Print Test, param = %d , curr state %d\n\n", *((TI_UINT32 *)pParam), pConn->state));
		break;

	default:
		WLAN_OS_REPORT(("Invalid function type in Debug Connection Function Command: %d\n\n", funcType));
		break;
	}
#endif
} 

void printConnDbgFunctions(void)
{
#ifdef REPORT_LOG
	WLAN_OS_REPORT(("  Conn Dbg Functions  \n"));
	WLAN_OS_REPORT(("----------------------\n"));

	WLAN_OS_REPORT(("601 - Connection Print Test.\n"));
#endif
}
