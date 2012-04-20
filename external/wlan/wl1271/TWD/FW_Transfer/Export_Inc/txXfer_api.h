/*
 * txXfer_api.h
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
 *   MODULE:  txXfer_api.h
 *
 *   PURPOSE: Tx Xfer module API.
 * 
 ****************************************************************************/

#ifndef _TX_XFER_API_H
#define _TX_XFER_API_H


#include "TWDriver.h"


/* 
 *   Public Function Definitions:
 *   ============================
 */


/** 
 * \fn     txXfer_Create
 * \brief  Create module
 * 
 * Create module
 * 
 * \note   
 * \param  hOs - The OS API handle
 * \return The created module handle
 * \sa     
 */ 
TI_HANDLE txXfer_Create (TI_HANDLE hOs);


/** 
 * \fn     txXfer_Destroy
 * \brief  Destroy module
 * 
 * Destroy module
 * 
 * \note   
 * \param  hTxXfer - Module handle
 * \return TI_OK
 * \sa     
 */ 
TI_STATUS txXfer_Destroy (TI_HANDLE hTxXfer);


/** 
 * \fn     txXfer_Init
 * \brief  Initialize module variables
 * 
 * Initialize module variables including saving other modules handles
 * 
 * \note   
 * \param  hTxXfer - Module handle
 * \param  hXXX    - Other modules handles
 * \return TI_OK
 * \sa     
 */ 
TI_STATUS txXfer_Init (TI_HANDLE hTxXfer, TI_HANDLE hReport, TI_HANDLE hTwIf);


/** 
 * \fn     txXfer_Restart
 * \brief  Restart some module variables
 * 
 * Restart some module variables upon init, stop or recovery
 * 
 * \note   
 * \param  hTxXfer - Module handle
 * \return TI_OK
 * \sa     
 */ 
TI_STATUS txXfer_Restart (TI_HANDLE hTxXfer);


/** 
 * \fn     txXfer_SetDefaults
 * \brief  Configure module default settings
 * 
 * Configure module default settings from ini file
 * 
 * \note   
 * \param  hTxXfer     - Module handle
 * \param  pInitParams - The default paremeters structure
 * \return void
 * \sa     
 */ 
void txXfer_SetDefaults (TI_HANDLE hTxXfer, TTwdInitParams *pInitParams);


/** 
 * \fn     txXfer_SetBusParams
 * \brief  Configure bus related parameters
 * 
 * Configure bus driver DMA-able buffer length to be used as a limit to the aggragation length.
 * 
 * \note   
 * \param  hTxXfer     - Module handle
 * \param  uDmaBufLen  - The bus driver DMA-able buffer length
 * \return void
 * \sa     
 */ 
void txXfer_SetBusParams (TI_HANDLE hTxXfer, TI_UINT32 uDmaBufLen);


/** 
 * \fn     txXfer_RegisterCb
 * \brief  Register callback functions
 * 
 * Called by Tx upper layers to register their CB for packet transfer completion.
 * Registered only if needed (currently used only by WHA layer).
 * 
 * \note   
 * \param  hTxXfer    - Module handle
 * \param  CallBackID - Type of CB being registered (currently only transfer completion)
 * \param  CBFunc     - The CB function
 * \param  CBObj      - The parameter to provide when calling the CB
 * \return void
 * \sa     
 */ 
void txXfer_RegisterCb (TI_HANDLE hTxXfer, TI_UINT32 CallBackID, void *CBFunc, TI_HANDLE CBObj);


/** 
 * \fn     txXfer_SendPacket
 * \brief  Send a Tx packet to the FW
 * 
 * Called by the Tx upper layers to send a new Tx packet to the FW (after FW resources were allocated).
 * Aggregate the packet if possible, and if needed call txXfer_SendAggregatedPkts to forward 
 *     the aggregation to the FW.
 * 
 * \note   
 * \param  hTxXfer     - Module handle
 * \param  pPktCtrlBlk - The new packet to send
 * \return COMPLETE if completed in this context, PENDING if not, ERROR if failed
 * \sa     
 */ 
ETxnStatus txXfer_SendPacket (TI_HANDLE hTxXfer, TTxCtrlBlk *pPktCtrlBlk);


/** 
 * \fn     txXfer_EndOfBurst
 * \brief  Indicates that current packets burst stopped
 * 
 * Called by the Tx upper layers to indicate that the current packets burst stopped.
 * Sends the current aggregation of packets to the FW.
 * 
 * \note   
 * \param  hTxXfer - module handle
 * \return void
 * \sa     
 */ 
void txXfer_EndOfBurst (TI_HANDLE hTxXfer);



#ifdef TI_DBG
void txXfer_ClearStats (TI_HANDLE hTxXfer);
void txXfer_PrintStats (TI_HANDLE hTxXfer);
#endif



#endif /* _TX_XFER_API_H */
