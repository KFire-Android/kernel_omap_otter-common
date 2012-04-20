/*
 * CmdMBox.h
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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


/** \file CmdMBox.h
 *  \brief CmdMbox internal defenitions
 *
 *  \see CmdMBox.c
 */

#ifndef _CMDMBOX_H_
#define _CMDMBOX_H_

#include "TwIf.h"

/*****************************************************************************
 **          Defines                                                        **
 *****************************************************************************/
 /* wait for a Mail box command to complete, ms */
#define CMDMBOX_WAIT_TIMEOUT            15000
#define CMDMBOX_HEADER_LEN              4
#define MAX_CMD_MBOX_CONSECUTIVE_TXN    5



typedef struct
{   
    TTxnStruct          tTxnStruct;
    Command_t           tCmdMbox;
}TCmdTxn;

typedef struct
{   
    TTxnStruct          tTxnStruct;
    TI_UINT32           uRegister;
}TRegTxn;


/*****************************************************************************
 **         Structures                                                      **
 *****************************************************************************/

/** \struct TCmdMbox
 * \brief CmdMbox structure
 * 
 * \par Description
 * 
 * \sa	
 */ 
typedef struct
{   
    /* handles */
    TI_HANDLE           hOs;
    TI_HANDLE           hReport;
    TI_HANDLE           hTwIf;
    TI_HANDLE           hCmdQueue;
    TI_HANDLE           hCmdMboxTimer;
    fnotify_t           fCb;
    TI_HANDLE           hCb;
    TCmdMboxErrorCb     fErrorCb;
    
    /* HW params */
    /* use a struct to read buffers from the bus - used for extra bytes reserving */

    TCmdTxn             aCmdTxn[2];
    TRegTxn             aRegTxn[2];

    /* Holds the module state */
    TI_BOOL             bCmdInProgress;
    TI_UINT32           uFwAddr;
    TI_UINT32           uWriteLen;
    TI_UINT32           uReadLen;

} TCmdMbox;

#endif


