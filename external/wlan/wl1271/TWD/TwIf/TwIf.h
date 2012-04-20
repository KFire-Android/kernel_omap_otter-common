/*
 * TwIf.h
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



/** \file   TwIf.h 
 *  \brief  TwIf module API definition                                  
 *
 *  \see    TwIf.c
 */

#ifndef __TWIF_API_H__
#define __TWIF_API_H__


#include "Device.h"
#include "TxnDefs.h"
#include "BusDrv.h"


/************************************************************************
 * Defines
 ************************************************************************/


/************************************************************************
 * Macros
 ************************************************************************/
/* 
 * Defines a TNETWIF read/write field with padding.
 * A low level driver may use this padding internally
 */
#define PADDING(field) \
    TI_UINT8 padding [TNETWIF_READ_OFFSET_BYTES];  \
    field;


/************************************************************************
 * Types
 ************************************************************************/

typedef void (*TTwIfCallback)(TI_HANDLE hCb);
typedef void (*TRecoveryCb)(TI_HANDLE hCb);



/************************************************************************
 * Functions
 ************************************************************************/
TI_HANDLE   twIf_Create (TI_HANDLE hOs);
TI_STATUS   twIf_Destroy (TI_HANDLE hTwIf);
void        twIf_Init (TI_HANDLE hTwIf, 
                       TI_HANDLE hReport, 
                       TI_HANDLE hContext, 
                       TI_HANDLE hTimer, 
                       TI_HANDLE hTxnQ, 
                       TRecoveryCb fRecoveryCb, 
                       TI_HANDLE hRecoveryCb);
void        twIf_RegisterErrCb (TI_HANDLE hTwIf, void *fErrCb, TI_HANDLE hErrCb);
ETxnStatus  twIf_Restart (TI_HANDLE hTwIf);
void        twIf_SetPartition (TI_HANDLE hTwIf,
                               TPartition *pPartition);
void        twIf_Awake (TI_HANDLE hTwIf);
void        twIf_Sleep (TI_HANDLE hTwIf);
void        twIf_HwAvailable (TI_HANDLE hTwIf);
ETxnStatus  twIf_Transact (TI_HANDLE hTwIf, TTxnStruct *pTxn);
ETxnStatus  twIf_TransactReadFWStatus (TI_HANDLE hTwIf, TTxnStruct *pTxn);

TI_BOOL		twIf_isValidMemoryAddr(TI_HANDLE hTwIf, TI_UINT32 Address, TI_UINT32 Length);
TI_BOOL		twIf_isValidRegAddr(TI_HANDLE hTwIf, TI_UINT32 Address, TI_UINT32 Length);

#ifdef TI_DBG
    void        twIf_PrintModuleInfo (TI_HANDLE hTwIf);
    void        twIf_PrintQueues (TI_HANDLE hTwIf);
#endif /* TI_DBG */




#endif /*__TWIF_API_H__*/
