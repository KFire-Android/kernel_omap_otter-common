/*
 * broadcastKey802_1x.h
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

/** \file broadcastKeySM.h
 *  \brief station broadcast key SM API
 *
 *  \see broadcastKeySM.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  station broadcast key SM	                                	*
 *   PURPOSE: station broadcast key SM API		                            *
 *                                                                          *
 ****************************************************************************/

#ifndef _BROADCAST_KEY_802_1X_H
#define _BROADCAST_KEY_802_1X_H

#include "paramOut.h"
#include "fsm.h"
#include "rsnApi.h"
#include "keyTypes.h"

#include "broadcastKeySM.h"

/* Constants */
/** number of states in the state machine */
#define	BCAST_KEY_802_1X_NUM_STATES		3

/** number of events in the state machine */
#define	BCAST_KEY_802_1X_NUM_EVENTS		4

/* Enumerations */

/** state machine states */
typedef enum 
{
	BCAST_KEY_802_1X_STATE_IDLE                  = 0,
	BCAST_KEY_802_1X_STATE_START                 = 1,
	BCAST_KEY_802_1X_STATE_COMPLETE              = 2
} broadcastKey802_1x_smStates;

/** State machine events */
typedef enum 
{
	BCAST_KEY_802_1X_EVENT_START                  = 0,
	BCAST_KEY_802_1X_EVENT_STOP                   = 1,
	BCAST_KEY_802_1X_EVENT_SUCCESS                = 2,
	BCAST_KEY_802_1X_EVENT_FAILURE				  = 3
} broadcastKey802_1x_smEvents;

/* Enumerations */

/* Typedefs */


/* Structures */


/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS broadcastKey802_1x_config(broadcastKey_t *pBroadcastKey);


#endif /*  _BROADCAST_KEY_802_1X_H*/
