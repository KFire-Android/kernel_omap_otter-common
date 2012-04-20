/*
 * CmdHndlr.h
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


#ifndef CMD_H
#define CMD_H

#include "queue.h"
#include "DrvMainModules.h"
#include "WlanDrvIf.h"

/* The configuration commands structure */
typedef struct 
{
    TQueNodeHdr tQueNodeHdr;                            /* The header used for queueing the command */
    TI_UINT32   cmd;
    TI_UINT32   flags;
    void       *buffer1;
    TI_UINT32   buffer1_len;
    void       *buffer2;
    TI_UINT32   buffer2_len;
    TI_UINT32  *param3;
    TI_UINT32  *param4;
    void       *pSignalObject;                          /* use to save handle to complete mechanism per OS */
    void       *local_buffer;
    TI_UINT32   local_buffer_len;
    TI_UINT32   return_code;
    TI_STATUS	eCmdStatus;                             /* (PEND / COMPLETE) */
    TI_BOOL	    bWaitFlag; 	                            /* (TRUE / FALSE) */
    /*
     * TCmdRespUnion is defined for each OS:
     * For Linx and WM that defined is empty.
     * For OSE the new typedef includes all "Done" typedefs in union from EMP code (H files).
     */
    TI_UINT8	CmdRespBuffer[sizeof(TCmdRespUnion)];   
} TConfigCommand;


TI_HANDLE cmdHndlr_Create (TI_HANDLE hOs, TI_HANDLE hEvHandler);
TI_STATUS cmdHndlr_Destroy (TI_HANDLE hCmdHndlr, TI_HANDLE hEvHandler);
void      cmdHndlr_ClearQueue (TI_HANDLE hCmdHndlr);
void      cmdHndlr_Init (TStadHandlesList *pStadHandles);
TI_STATUS cmdHndlr_InsertCommand (TI_HANDLE     hCmdHndlr,
                                  TI_UINT32     cmd,
                                  TI_UINT32     flags,
                                  void         *buffer1,
                                  TI_UINT32     buffer1_len,
                                  void         *buffer2,
                                  TI_UINT32     buffer2_len,
                                  TI_UINT32    *param3,
                                  TI_UINT32    *param4);
void      cmdHndlr_HandleCommands (TI_HANDLE hCmdHndlr);
void      cmdHndlr_Complete (TI_HANDLE hCmdHndlr);
void     *cmdHndlr_GetStat (TI_HANDLE hCmdHndlr);
void      cmdHndlr_Enable  (TI_HANDLE hCmdHndlr);
void      cmdHndlr_Disable (TI_HANDLE hCmdHndlr);

#endif
