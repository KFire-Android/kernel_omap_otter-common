/*
 * fsm.h
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

/** \file fsm.h
 *  \brief finite state machine header file
 *
 *  \see fsm.c
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	fsm.h													   */
/*    PURPOSE:	Finite State Machine API								   */
/*																	 	   */
/***************************************************************************/

#ifndef __FSM_H__
#define __FSM_H__

#include "tidef.h"
#include "commonTypes.h"

/* Constants */
#define	MAX_DESC_STRING_LEN		64		


/* Enumerations */

/* Typedefs */

/** state transition function */
typedef	TI_STATUS (*fsm_eventActivation_t)(TI_UINT8 *currState, TI_UINT8 event, void* data);

/** action function type definition */
typedef TI_STATUS (*fsm_Action_t)(void* pData);

/* Structures */

/* State\Event cell */
typedef  struct
{
	TI_UINT8			nextState;		/**< next state in transition */
	fsm_Action_t	actionFunc;		/**< action function */
} fsm_actionCell_t;

/** matrix type */
typedef	fsm_actionCell_t*		fsm_Matrix_t;

/** general FSM structure */
typedef struct
{
	fsm_Matrix_t			stateEventMatrix;		/**< State\Event matrix */
	TI_UINT8					MaxNoOfStates;			/**< Max Number of states in the matrix */
	TI_UINT8					MaxNoOfEvents;			/**< Max Number of events in the matrix */
	TI_UINT8					ActiveNoOfStates;		/**< Active Number of states in the matrix */
	TI_UINT8					ActiveNoOfEvents;		/**< Active Number of events in the matrix */
	fsm_eventActivation_t	transitionFunc;			/**< State transition function */
} fsm_stateMachine_t;

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS fsm_Create(TI_HANDLE				hOs,
				fsm_stateMachine_t		**pFsm,
				TI_UINT8					MaxNoOfStates,
				TI_UINT8					MaxNoOfEvents);

TI_STATUS fsm_Unload(TI_HANDLE				hOs,
				fsm_stateMachine_t		*pFsm);

TI_STATUS fsm_Config(fsm_stateMachine_t	*pFsm,
				  fsm_Matrix_t			pMatrix,
				  TI_UINT8					ActiveNoOfStates,
				  TI_UINT8					ActiveNoOfEvents,
				  fsm_eventActivation_t	transFunc,
				  TI_HANDLE				hOs);

TI_STATUS fsm_Event(fsm_stateMachine_t		*pFsm,
				 TI_UINT8					*currentState,
				 TI_UINT8					event,
				 void					*pData);

TI_STATUS fsm_GetNextState(fsm_stateMachine_t		*pFsm,
						TI_UINT8					currentState,
						TI_UINT8					event,
						TI_UINT8					*nextState);


TI_STATUS action_nop(void *pData);


#endif /* __FSM_H__ */
