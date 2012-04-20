/*
 * externalSec.h
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

/** \file externalSec.h
 *  \brief station externalSec API
 *
 *  \see externalSec.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  externalSec	                                	            *
 *   PURPOSE: station external security		                                *
 *                                                                          *
 ****************************************************************************/

#ifndef _EXTERNAL_SEC_H
#define _EXTERNAL_SEC_H

#include "paramOut.h"
#include "fsm.h"
#include "rsnApi.h"
#include "keyTypes.h"

/* Constants */
/** number of states in the state machine */
#define	EXTERNAL_SEC_NUM_STATES		2

/** number of events in the state machine */
#define  EXTERNAL_SEC_NUM_EVENTS	3

/* Enumerations */

/** state machine states */
typedef enum 
{
	EXTERNAL_SEC_STATE_IDLE        = 0,
	EXTERNAL_SEC_STATE_WAIT        = 1
} externalSec_smStates;


/** State machine events */
typedef enum 
{
	EXTERNAL_SEC_EVENT_START       = 0,
	EXTERNAL_SEC_EVENT_COMPLETE    = 1,
	EXTERNAL_SEC_EVENT_STOP	       = 2
} externalSec_smSEvents;

/* Typedefs */

/* Structures */
struct externalSec_t
{
    TI_UINT8            currentState;
    fsm_stateMachine_t  *pExternalSecSm;
    mainSec_t           *pParent;
    TI_HANDLE           hOs; 
    TI_HANDLE           hReport;
    TI_BOOL             bPortStatus;
};

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS externalSec_config(mainSec_t *pMainSec);
struct externalSec_t* externalSec_create(TI_HANDLE hOs);
TI_STATUS externalSec_Destroy(struct externalSec_t *pExternalSec);
TI_STATUS externalSec_event(struct externalSec_t *pExternalSec, TI_UINT8 event, void *pData);
TI_STATUS externalSec_rsnComplete(struct externalSec_t *pExternalSec);
#endif /* _EXTERNAL_SEC_H*/
