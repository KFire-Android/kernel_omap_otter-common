/*
 * conn.h
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

/** \file conn.h
 *  \brief connection module internal header file
 *
 *  \see conn.c
 */

/***************************************************************************/
/*																			*/
/*	  MODULE:	conn.h														*/
/*    PURPOSE:	connection module internal header file						*/
/*																			*/
/***************************************************************************/
#ifndef __CONN_H__
#define __CONN_H__

#include "tidef.h"
#include "paramOut.h"
#include "fsm.h"
#include "802_11Defs.h"
#include "connApi.h"
#include "scrApi.h"

typedef struct
{
	mgmtStatus_e			disAssocEventReason;		/* Disassoc reason to be used for upper layer	*/
	TI_UINT32					disAssocEventStatusCode;	/* Extra information for disConnEventReason		*/
} connSmContext_t;


/*
 *	ibssRandomSsidGen - in IBSS, that flag indicates if the ibss must be randommaly changed. 
 *	TI_FALSE - do not generate random ibss.
 *	TI_TRUE -	generate random ibss.
 *	1.	at Start up that flag is TI_FALSE
 *  2.	after Connection fail in connIbss or connSelf SM, set the flag to TI_TRUE in order to 
 *		generate a new random ibss.
 */

/* Connection handle */
typedef struct 
{
	TI_UINT8				state;	
	connSmContext_t			smContext;
	connectionType_e		currentConnType;
	TI_UINT32				timeout;
	TI_HANDLE               hConnTimer;         /* This timer is used both by IBSS and BSS */
	fsm_stateMachine_t		*ibss_pFsm;
    fsm_stateMachine_t		*infra_pFsm;
	EConnType				connType;
	DisconnectType_e		disConnType;
	mgmtStatus_e			disConnReasonToAP;
	TI_BOOL					disConEraseKeys;
    TI_UINT8                buf[40];            /* For dummy interrogate data (to flush cmdQueue) */
	conn_status_callback_t  pConnStatusCB;
	TI_HANDLE				connStatCbObj;
	TI_BOOL 				scrRequested[ SCR_RESOURCE_NUM_OF_RESOURCES ]; /* wether SCR was requested for the two resources */
    TI_BOOL                 bScrAcquired[ SCR_RESOURCE_NUM_OF_RESOURCES ]; /* wether SCR was acquired for the two resources */
	TI_UINT32               ibssDisconnectCount;

    /* Other modules handles */
	TI_HANDLE				hSiteMgr;
	TI_HANDLE				hSmeSm;
	TI_HANDLE				hMlmeSm;
	TI_HANDLE				hRsn;
	TI_HANDLE				hReport;
	TI_HANDLE				hOs;
	TI_HANDLE				hRxData;
	TI_HANDLE 				hPwrMngr;
	TI_HANDLE				hCtrlData;
	TI_HANDLE 				hQosMngr;
	TI_HANDLE				hTWD;
	TI_HANDLE				hMeasurementMgr;
	TI_HANDLE				hScr;
	TI_HANDLE				hTrafficMonitor;
	TI_HANDLE				hXCCMngr;
    TI_HANDLE               hScanCncn;
	TI_HANDLE				hCurrBss;
	TI_HANDLE				hSwitchChannel;
	TI_HANDLE				hEvHandler;
	TI_HANDLE				hHealthMonitor;
	TI_HANDLE				hTxMgmtQ;
    TI_HANDLE               hRegulatoryDomain;
    TI_HANDLE    			hSoftGemini;
    TI_HANDLE	            hTxCtrl;
    TI_HANDLE	            hTimer;
} conn_t;

#endif /* __CONN_H__*/
