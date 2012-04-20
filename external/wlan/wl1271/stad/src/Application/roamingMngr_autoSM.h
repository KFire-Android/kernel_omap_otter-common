/*
 * roamingMngr_autoSM.h
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

/** \file roamingMngr_autoSM.h
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

#ifndef _ROAMING_MNGR_AUTO_SM_H_
#define _ROAMING_MNGR_AUTO_SM_H_

#include "osApi.h"
#include "paramOut.h"
#include "scanMngrApi.h"
#include "bssTypes.h"
#include "DrvMainModules.h"


/*-----------*/
/* Constants */
/*-----------*/

/** state machine states */
typedef enum 
{
/*	0	*/	ROAMING_STATE_IDLE,
/*	1	*/	ROAMING_STATE_WAIT_4_TRIGGER,
/*	2	*/  ROAMING_STATE_WAIT_4_CMD,
/*	3	*/  ROAMING_STATE_SCANNING,
/*	4	*/  ROAMING_STATE_SELECTING,
/*	5	*/  ROAMING_STATE_CONNECTING,
/*	6	*/  ROAMING_STATE_LAST

} roamingMngr_smStates;

/** State machine events */
typedef enum 
{
/*	0	*/	ROAMING_EVENT_START = 0, 			/* CONNECTED */
/*	1	*/	ROAMING_EVENT_STOP, 			/* NOT CONNECTED */
/*	2	*/	ROAMING_EVENT_ROAM_TRIGGER,
/*	3	*/	ROAMING_EVENT_SCAN, 
/*	4	*/	ROAMING_EVENT_SELECT, 
/*	5	*/	ROAMING_EVENT_REQ_HANDOVER, 
/*	6	*/	ROAMING_EVENT_ROAM_SUCCESS, 
/*	7	*/	ROAMING_EVENT_FAILURE, 
/*	8	*/	ROAMING_EVENT_LAST

} roamingMngr_smEvents;

#define ROAMING_MNGR_NUM_STATES     	ROAMING_STATE_LAST   
#define ROAMING_MNGR_NUM_EVENTS     	ROAMING_EVENT_LAST

#define INVALID_CANDIDATE_INDEX     	0xFF
#define CURRENT_AP_INDEX            	0xFE


extern TGenSM_actionCell roamingMngrAuto_matrix[ROAMING_MNGR_NUM_STATES][ROAMING_MNGR_NUM_EVENTS];
extern TI_INT8*  AutoRoamStateDescription[];
extern TI_INT8*  AutoRoamEventDescription[];

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


#endif /* _ROAMING_MNGR_AUTO_SM_H_ */

