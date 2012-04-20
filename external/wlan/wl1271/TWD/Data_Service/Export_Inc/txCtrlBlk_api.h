/*
 * txCtrlBlk_api.h
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


/***************************************************************************/
/*																		   */
/*	  MODULE:	txCtrlBlk_api.h											       */
/*    PURPOSE:	Tx control block module API.							   */
/*																		   */
/***************************************************************************/
#ifndef _TX_CTRL_BLK_API_H_
#define _TX_CTRL_BLK_API_H_


#include "TWDriver.h"

/* Public Function Definitions */
TI_HANDLE	txCtrlBlk_Create    (TI_HANDLE hOs);
TI_STATUS	txCtrlBlk_Destroy   (TI_HANDLE hTxCtrlBlk);
TI_STATUS   txCtrlBlk_Init      (TI_HANDLE hTxCtrlBlk, TI_HANDLE hReport, TI_HANDLE hContext);
TTxCtrlBlk *txCtrlBlk_Alloc     (TI_HANDLE hTxCtrlBlk);
void		txCtrlBlk_Free      (TI_HANDLE hTxCtrlBlk, TTxCtrlBlk *pCurrentEntry);
TTxCtrlBlk *txCtrlBlk_GetPointer(TI_HANDLE hTxCtrlBlk, TI_UINT8 descId);
#ifdef TI_DBG
void		txCtrlBlk_PrintTable(TI_HANDLE hTxCtrlBlk);
#endif /* TI_DBG */


#endif  /* _TX_CTRL_BLK_API_H_ */
		

