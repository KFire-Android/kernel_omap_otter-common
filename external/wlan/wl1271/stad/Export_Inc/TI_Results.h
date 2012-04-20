/*
 * TI_Results.h
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


/*--------------------------------------------------------------------------*/
/* Module:		TI_Results.h*/
/**/
/* Purpose:		*/
/**/
/*--------------------------------------------------------------------------*/
#ifndef _TI_RESULTS_H
#define _TI_RESULTS_H

#define TI_RESULT_UNBOUND               0x00000001

#define TI_RESULT_OK                0
#define TI_RESULT_FAILED            0xFFFFFFFF  /* -1*/
#define TI_RESULT_INVALIDE_HANDLE   0xFFFFFFFE  /* -2*/
#define TI_RESULT_SM_NOT_FOUND      0xFFFFFFFD  /* -3*/
#define TI_RESULT_INVALID_PARAMETER     0xFFFFFFFC  /* -4*/
#define TI_RESULT_REGISTRY_FAILED       0xFFFFFFFB  /* -5*/
#define TI_RESULT_NOT_ENOUGH_MEMORY     0xFFFFFFFA  /* -6*/
#define TI_RESULT_DRIVER_ERROR          0xFFFFFFF9  /* -7*/
#define TI_RESULT_IPC_ERROR             0xFFFFFFF8  /* -8*/


#define TI_SUCCEEDED(Status)    (Status == TI_RESULT_OK)
#define TI_FAILED(Status)       (Status != TI_RESULT_OK)

#endif

