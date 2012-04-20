/*
 * roamingMngr_manualSM.h
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

/** \file roamingMngr_manualSM.h
 *  \brief Internal Roaming Manager API
 *
 *  \see roamingMngr.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Roaming Manager	    		                                *
 *   PURPOSE: Roaming Manager Module API                              		*
 *                                                                          *
 ****************************************************************************/

#ifndef _ROAMING_MNGR_MANUAL_SM_H_
#define _ROAMING_MNGR_MANUAL_SM_H_

/*-----------*/
/* Constants */
/*-----------*/

/** state machine states */
typedef enum 
{
/*	0	*/  ROAMING_MANUAL_STATE_IDLE = 0,
/*	1	*/  ROAMING_MANUAL_STATE_CONNECTED,
/*	2	*/  ROAMING_MANUAL_STATE_SCAN,
/*	3	*/  ROAMING_MANUAL_STATE_HANDOVER,
/*	4	*/  ROAMING_MANUAL_STATE_LAST   
} ERoamManual_smStates;

/** State machine events */
typedef enum 
{
/*	0	*/	ROAMING_MANUAL_EVENT_START = 0,
/*	1	*/	ROAMING_MANUAL_EVENT_SCAN,
/*	2	*/	ROAMING_MANUAL_EVENT_CONNECT,
/*	3	*/	ROAMING_MANUAL_EVENT_STOP, 
/*	4	*/	ROAMING_MANUAL_EVENT_REJECT, 
/*	5	*/	ROAMING_MANUAL_EVENT_SUCCESS,
/*	6	*/	ROAMING_MANUAL_EVENT_FAIL,
/*	7	*/	ROAMING_MANUAL_EVENT_COMPLETE,
/*	8	*/	ROAMING_MANUAL_EVENT_LAST,
} ERoamManual_smEvents;


#define ROAMING_MANUAL_NUM_STATES    ROAMING_MANUAL_STATE_LAST   
#define ROAMING_MANUAL_NUM_EVENTS    ROAMING_MANUAL_EVENT_LAST


extern TGenSM_actionCell roamingMngrManual_matrix[ROAMING_MANUAL_NUM_STATES][ROAMING_MANUAL_NUM_EVENTS];
extern TI_INT8*  ManualRoamStateDescription[];
extern TI_INT8*  ManualRoamEventDescription[];

/*--------------*/
/* Enumerations */
/*--------------*/

/*----------*/
/* Typedefs */
/*----------*/

/*------------*/
/* Structures */
/*------------*/

/*---------------------------*/
/* External data definitions */
/*---------------------------*/

/*--------------------------------*/
/* External functions definitions */
/*--------------------------------*/

/*----------------------------*/
/* Global Function prototypes */
/*----------------------------*/


#endif /* _ROAMING_MNGR_MANUAL_SM_H_ */

