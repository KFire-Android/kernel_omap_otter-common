/*
 * CmdQueue_api.h
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



/** \file CmdQueue_api.h
 *  \brief CmdQueue api
 *
 *  \see CmdQueue.c
 */

#ifndef _CMDQUEUE_API_H_
#define _CMDQUEUE_API_H_

#include "public_commands.h"


/*****************************************************************************
 **         Structures                                                      **
 *****************************************************************************/

/* User Callback for Queue */
typedef void (*TCmdQueueInterrogateCb) (TI_HANDLE handle, TI_UINT16 status, void *pItrBuf);


/*****************************************************************************
 **         APIs definitions                                  **
 *****************************************************************************/

/*
 * \brief	Create the TCmdQueue object
 * 
 * \param  hOs  - OS module object handle
 * \return Handle to the created object
 * 
 * \par Description
 * Calling this function creates a CmdQueue object
 * 
 * \sa cmdQueue_Destroy
 */
TI_HANDLE cmdQueue_Create (TI_HANDLE hOS);


/*
 * \brief	Destroys the cmdQueue object
 * 
 * \param  hCmdMbox  - The object to free
 * \return TI_OK
 * 
 * \par Description
 * Calling this function destroys the cmdQueue object
 * 
 * \sa cmdQueue_Create
 */
TI_STATUS cmdQueue_Destroy (TI_HANDLE hCmdQueue);


/*
 * \brief	Configure the CmdQueue object
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  hReport - Handle to report module
 * \param  hTwIf  - Handle to TwIf
 * \param  hTimer  - Handle to os timer
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdQueue_Init   (TI_HANDLE hCmdQueue, 
                           TI_HANDLE hCmdMbox, 
                           TI_HANDLE hReport, 
                           TI_HANDLE hTwIf,
                           TI_HANDLE hTimer);


/*
 * \brief	Prepere the command queue for recovery.
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * Copy the queue nodes to a recovery list, in order handle 
 * the commands CB's after recovery has finished
 * 
 * \sa cmdQueue_EndReconfig
 */
TI_STATUS cmdQueue_Restart (TI_HANDLE hCmdQueue);


/*
 * \brief	Call the stored CB to end the recovery of the MBox queue
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * Call the stored CB's with an error status 
 * 
 * \sa cmdQueue_StartReconfig
 */
TI_STATUS cmdQueue_EndReconfig (TI_HANDLE hCmdQueue);


/*
 * \brief	Register for a call back to be called when Command Complete occured and the CmdMboxCB was NULL
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  fCb - The command's Cb function
 * \param  hCb - The command's Cb handle
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdQueue_RegisterCmdCompleteGenericCb (TI_HANDLE hCmdQueue, void *fCb, TI_HANDLE hCb);


/*
 * \brief	Register for a call back to be called when an Error (Timeout) occurs
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  fCb - The command's Cb function
 * \param  hCb - The command's Cb handle
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdQueue_RegisterForErrorCb (TI_HANDLE hCmdQueue, void *fCb, TI_HANDLE hCb);


/*
 * \brief	Sends the command to the cmdMbox
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  eMboxCmdType - The command type
 * \param  pMboxBuf - The command itself (parameters)
 * \param  uParamsLen - The command's length
 * \param  fCb - The command's Cb function
 * \param  hCb - The command's Cb handle
 * \param  pCb - Pointer to the results buffer (for interrogate commands)
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Pushes the command to the command queue, which triggers the 
 * CmdQueue SM.
 * 
 * \sa cmdQueue_Push
 */
TI_STATUS cmdQueue_SendCommand (TI_HANDLE  hCmdQueue, 
                            Command_e  eMboxCmdType, 
                            void      *pMboxBuf, 
                            TI_UINT32  uParamsLen, 
                            void      *fCb, 
                            TI_HANDLE  hCb, 
                            void      *pCb);


/*
 * \brief	Notify the CmdQueue SM on the result received.
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Call the CmdQueue SM with CMDQUEUE_EVENT_COMPLETE
 * 
 * \sa cmdQueue_SM
 */
TI_STATUS cmdQueue_ResultReceived (TI_HANDLE hCmdQueue);


/*
 * \brief	Returns maximum number of commands (ever) in TCmdQueue queue
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return maximum number of commands (ever) in mailbox queue
 * 
 * \par Description
 * Used for debugging purposes
 *
 * \sa cmdQueue_Error
 */
TI_UINT32 cmdQueue_GetMaxNumberOfCommands (TI_HANDLE hCmdQueue);


/*
 * \brief	Enables the CmdMbox (on exit from init mode)
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa cmdQueue_DisableMbox
 */
TI_STATUS cmdQueue_EnableMbox (TI_HANDLE hCmdQueue);


/*
 * \brief	Disables the CmdMbox (when stopping the driver)
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa cmdQueue_EnableMbox
 */
TI_STATUS cmdQueue_DisableMbox (TI_HANDLE hCmdQueue);


/*
 * \brief	Called when a command timeout occur
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa cmdQueue_Init, cmdMbox_TimeOut
 */
TI_STATUS cmdQueue_Error (TI_HANDLE hCmdQueue, TI_UINT32 command, TI_UINT32 status, void *param);


#ifdef TI_DBG

/*
 * \brief	Print the command queue & statistics
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return void
 * 
 * \par Description
 * Used for debugging purposes
 *
 * \sa cmdQueue_PrintQueue
 */
void      cmdQueue_Print (TI_HANDLE hCmdQueue);


/*
 * \brief	print the last uNumOfCmd commands
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  uNumOfCmd - Number of commands to print
 * \return void
 * 
 * \par Description
 * Used for debugging purposes
 *
 * \sa cmdQueue_Error
 */
void      cmdQueue_PrintHistory (TI_HANDLE hCmdQueue, TI_UINT32 uNumOfCmd);


#endif  /* TI_DBG */


#endif  /* _CMDQUEUE_API_H_ */
