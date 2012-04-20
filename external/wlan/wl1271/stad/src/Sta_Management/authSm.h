/*
 * authSm.h
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

/** \file assocSm.h
 *  \brief 802.11 authentication SM
 *
 *  \see assocSm.c
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	assocSm.h												   */
/*    PURPOSE:	802.11 authentication SM									   */
/*																	 	   */
/***************************************************************************/

#ifndef _AUTH_SM_H
#define _AUTH_SM_H

#include "fsm.h"
#include "mlmeApi.h"

/* Constants */

#define AUTH_MSG_HEADER_LEN		6
#define MAX_CHALLANGE_LEN		256
#define MAX_AUTH_MSG_LEN		MAX_CHALLANGE_LEN + AUTH_MSG_HEADER_LEN

/* Enumerations */

/* Typedefs */

typedef struct
{
	TI_UINT16				status;
	char				*pChalange;
	TI_UINT8				challangeLen;
} authData_t;

typedef struct
{
	legacyAuthType_e	authType;
	fsm_stateMachine_t	*pAuthSm;
	TI_UINT8				currentState;
	TI_UINT32				maxCount;
	TI_UINT8				retryCount;
	TI_UINT32				authRejectCount;
	TI_UINT32				authTimeoutCount;
	TI_UINT32				timeout;
    authData_t          authData;
	TI_HANDLE			hAuthSmTimer;

	TI_HANDLE			hMlme;
	TI_HANDLE			hRsn;
	TI_HANDLE			hReport;
	TI_HANDLE			hOs;
	TI_HANDLE			hTimer;
} auth_t;

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS auth_start(TI_HANDLE hAuth);

TI_STATUS auth_stop(TI_HANDLE hpAuth, TI_BOOL sendDeAuth, mgmtStatus_e reason );

TI_STATUS auth_recv(TI_HANDLE hAuth, mlmeFrameInfo_t *pFrame);

void auth_smTimeout(TI_HANDLE hAssoc, TI_BOOL bTwdInitOccured);

TI_STATUS auth_smMsgBuild(auth_t *pCtx, 
					   TI_UINT16 seq, 
					   TI_UINT16 statusCode, 
					   TI_UINT8* pChallange, 
					   TI_UINT8 challangeLen);

/* local functions */

#endif

