/*
 * fwDebug_api.h
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


/****************************************************************************
 *
 *   MODULE:  fwDebug_api.h
 *
 *   PURPOSE: FW-Debug module API.
 * 
 ****************************************************************************/

#ifndef _FW_DEBUG_API_H
#define _FW_DEBUG_API_H


#define FW_DEBUG_MAX_BUF		256
#define FW_DBG_CMD_MAX_PARAMS	2


/* FW Debug commands. */
typedef enum
{
	FW_DBG_CMD_READ_MEM,
	FW_DBG_CMD_WRITE_MEM
}fwDbg_dbgCmd_e;

typedef struct
{
	TI_UINT32	addr;
	TI_UINT32 	length;
	union
	{
		TI_UINT8	buf8[FW_DEBUG_MAX_BUF];
		TI_UINT32	buf32[FW_DEBUG_MAX_BUF/4];
	} UBuf;
}TFwDebugParams;

/* for TWD Debug */
typedef struct
{
	TI_UINT32		func_id;
	union
	{
		TI_UINT32		opt_param;
		TFwDebugParams	mem_debug;
	}debug_data;
} TTwdDebug;

typedef void(*TFwDubCallback)(TI_HANDLE hCb);


/* Public Function Definitions */


TI_HANDLE fwDbg_Create      (TI_HANDLE hOs);

void      fwDbg_Destroy     (TI_HANDLE hFwDebug);

void      fwDbg_Init	    (TI_HANDLE hFwDebug,
                             TI_HANDLE hReport,
                             TI_HANDLE hTwif);

TI_STATUS fwDbg_WriteAddr   (TI_HANDLE hFwDebug,
                             TI_UINT32 Address,
                             TI_UINT32 Length,
                             TI_UINT8* Buffer,
                             TFwDubCallback fCb,
                             TI_HANDLE hCb);

TI_STATUS fwDbg_ReadAddr    (TI_HANDLE hFwDebug,
                             TI_UINT32 Address,
                             TI_UINT32 Length,
                             TI_UINT8* Buffer,
                             TFwDubCallback fCb,
						     TI_HANDLE hCb);

TI_BOOL fwDbg_isValidMemoryAddr    (TI_HANDLE hFwDebug,
                             TI_UINT32 Address,
                             TI_UINT32 Length);

TI_BOOL fwDbg_isValidRegAddr    (TI_HANDLE hFwDebug,
                             TI_UINT32 Address,
                             TI_UINT32 Length);


#endif /* _FW_DEBUG_API_H */




