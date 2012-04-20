/*
 * CmdMBox_api.h
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


/** \file CmdMBox_api.h
 *  \brief CmdMbox api
 *
 *  \see CmdMBox.c
 */

#ifndef _CMDMBOX_API_H_
#define _CMDMBOX_API_H_

#include "TWDriverInternal.h"


/*****************************************************************************
 **              Types                                                      **
 *****************************************************************************/
typedef TI_STATUS (*TCmdMboxErrorCb)     (TI_HANDLE hCmdQueue, TI_UINT32 command, TI_UINT32 status, void *param);


/*****************************************************************************
 **         API functions definitions                                  **
 *****************************************************************************/

/*
 * \brief	Create the mailbox object
 * 
 * \param  hOs  - OS module object handle
 * \return Handle to the created object
 * 
 * \par Description
 * Calling this function creates a CmdMbox object
 * 
 * \sa cmdMbox_Destroy
 */
TI_HANDLE cmdMbox_Create            (TI_HANDLE hOs);


/*
 * \brief	Destroys the mailbox object
 * 
 * \param  hCmdMbox  - The object to free
 * \return TI_OK
 * 
 * \par Description
 * Calling this function destroys a CmdMbox object
 * 
 * \sa cmdMbox_Create
 */
TI_STATUS cmdMbox_Destroy           (TI_HANDLE hCmdMbox);


/*
 * \brief	Configure the CmdMbox object
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  hReport  - Handle to report module
 * \param  hTwIf  - Handle to TwIf
 * \param  hTimer  - Handle to os timer
 * \param  hCmdQueue  - Handle to CmdQueue
 * \param  fErrorCb  - Handle to error handling function
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdMbox_Init              (TI_HANDLE hCmdMbox,
                                     TI_HANDLE hReport,
                                     TI_HANDLE hTwIf,
                                     TI_HANDLE hTimer,
                                     TI_HANDLE hCmdQueue,
                                     TCmdMboxErrorCb fErrorCb);


/*
 * \brief	configure the mailbox address.
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  fCb  - Pointer to the CB
 * \param  hCb  - Cb's handle
 * \return TI_OK or TI_PENDING
 * 
 * \par Description
 * Called from HwInit to read the command mailbox address.
 * 
 * \sa
 */
TI_STATUS cmdMbox_ConfigHw          (TI_HANDLE hCmdMbox, fnotify_t fCb, TI_HANDLE hCb);


/*
 * \brief	Send the Command to the Mailbox
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  cmdType  - 
 * \param  pParamsBuf  - The buffer that will be written to the mailbox
 * \param  uWriteLen  - Length of data to write to the mailbox
 * \param  uReadLen  - Length of data to read from the mailbox (when the result is received)
 * \return TI_PENDING
 * 
 * \par Description
 * Copy the buffer given to a local struct, update the write & read lengths
 * and send to the FW's mailbox.
 *             
 *       ------------------------------------------------------
 *      | CmdMbox Header | Cmd Header    | Command parameters |
 *      ------------------------------------------------------
 *      | ID   | Status  | Type | Length | Command parameters |
 *      ------------------------------------------------------
 *       16bit   16bit    16bit   16bit     
 *
 * \sa cmdMbox_CommandComplete
 */
TI_STATUS cmdMbox_SendCommand       (TI_HANDLE hCmdMbox, Command_e cmdType, TI_UINT8* pParamsBuf, TI_UINT32 uWriteLen, TI_UINT32 uReadLen);


/*
 * \brief	Read the command's result
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return void
 * 
 * \par Description
 * This function is called for FwEvent module uppon receiving command complete interrupt.
 * It issues a read transaction from the mailbox with a CB.
 * 
 * \sa cmdMbox_SendCommand, cmdMbox_TransferComplete
 */
ETxnStatus cmdMbox_CommandComplete (TI_HANDLE hCmdMbox);


/*
 * \brief	Calls the cmdQueue_ResultReceived.
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK
 * 
 * \par Description
 * This function is called from cmdMbox_CommandComplete on a sync read, or from TwIf as a CB on an async read.
 * It calls cmdQueue_ResultReceived to continue the result handling procces & switch the bCmdInProgress flag to TI_FALSE, 
 * meaning other commands can be sent to the FW.
 * 
 * \sa cmdMbox_SendCommand, cmdMbox_TransferComplete
 */
TI_STATUS cmdMbox_TransferComplete  (TI_HANDLE hCmdMbox);


/*
 * \brief	Restart the module upon driver stop or restart
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdMbox_Restart           (TI_HANDLE hCmdMbox);


/*
 * \brief	Return the latest command status
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK or TI_NOK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdMbox_GetStatus         (TI_HANDLE hCmdMbox, CommandStatus_e *cmdStatus);


/*
 * \brief	Return the MBox address
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return MBox address
 * 
 * \par Description
 * 
 * \sa
 */
TI_UINT32 cmdMbox_GetMboxAddress    (TI_HANDLE hCmdMbox);


/*
 * \brief	Return the Command parameters buffer
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  pParamBuf  - Holds the returned buffer
 * \return
 * 
 * \par Description
 * Copying the command's data to pParamBuf
 * 
 * \sa
 */
void      cmdMbox_GetCmdParams           (TI_HANDLE hCmdMbox, TI_UINT8* pParamBuf);


#ifdef TI_DBG
void      cmdMbox_PrintInfo              (TI_HANDLE hCmdMbox);
#endif


#endif
