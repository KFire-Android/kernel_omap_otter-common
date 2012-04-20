/*
 * ScanSrvSM.h
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

/** \file ScanSrvSM.h
 *  \brief This file include definitions for the scan SRV SM module.
 *  \author Ronen Kalish
 *  \date 10-Jan-2005
 */

#ifndef __SCANSRVSM_H__
#define __SCANSRVSM_H__

#include "fsm.h"

/*
 ***********************************************************************
 *	Constant definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Enums.
 ***********************************************************************
 */

/** \enum scan_SRVSMEvents_e
 * \brief enumerates the different scan SRV SM events
 */
typedef enum
{
    SCAN_SRV_EVENT_REQUEST_PS = 0,
    SCAN_SRV_EVENT_PS_FAIL,
    SCAN_SRV_EVENT_PS_SUCCESS,
    SCAN_SRV_EVENT_PS_PEND,
    SCAN_SRV_EVENT_STOP_SCAN,
    SCAN_SRV_EVENT_FW_RESET,
    SCAN_SRV_EVENT_TIMER_EXPIRED,
    SCAN_SRV_EVENT_SCAN_COMPLETE,
    SCAN_SRV_NUM_OF_EVENTS
} scan_SRVSMEvents_e;

/** \enum scan_SRVSMStates_e
 * \brief enumerates the different scan SRV SM states
 */
typedef enum
{
	SCAN_SRV_STATE_IDLE = 0,
    SCAN_SRV_STATE_PS_WAIT,
	SCAN_SRV_STATE_SCANNING,
	SCAN_SRV_STATE_STOPPING,
	SCAN_SRV_STATE_PS_EXIT,
	SCAN_SRV_NUM_OF_STATES
} scan_SRVSMStates_e;

/*
 ***********************************************************************
 *	Typedefs.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Structure definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	External data definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	External functions definitions
 ***********************************************************************
 */

/**
 * \author Ronen Kalish\n
 * \date 10-Jan-2005\n
 * \brief Initialize the scan SRV SM.
 *
 * Function Scope \e Public.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_init( TI_HANDLE hScanSrv );

/**
 * \author Ronen Kalish\n
 * \date 10-Jan-2005\n
 * \brief Processes an event.
 *
 * Function Scope \e Public.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \param currentState - the current scan SRV SM state.\n
 * \param event - the event to handle.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_SMEvent( TI_HANDLE hScanSrv, scan_SRVSMStates_e* currentState, 
                             scan_SRVSMEvents_e event );

/**
 * \author Ronen Kalish\n
 * \date 10-Jan-2005\n
 * \brief Request to enter driver mode from the power manager module.\n
 *
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_requestPS( TI_HANDLE hScanSrv );

/**
 * \author Yuval Adler\n
 * \date 6-Oct-2005\n
 * \brief Request to release PS mode from the PowerSRV , and wait for answer.\n
 *
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_releasePS( TI_HANDLE hScanSrv );

/**
 * \author Ronen Kalish\n
 * \date 10-Jan-2005\n
 * \brief Send the scan command to the firmware.\n
 *
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_startActualScan( TI_HANDLE hScanSrv );

/**
 * \author Ronen Kalish\n
 * \date 10-Jan-2005\n
 * \brief Send a stop scan command to the firmware.\n
 *
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_stopActualScan( TI_HANDLE hScanSrv );

/**
 * \author Ronen Kalish\n
 * \date 10-Jan-2005\n
 * \brief Notifies scan complete to upper layer.\n
 *
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_notifyScanComplete( TI_HANDLE hScanSrv );

/**
 * \author Ronen Kalish\n
 * \date 10-Jan-2005\n
 * \brief Handles a timer expiry event - starts a recovery process.
 *
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_handleTimerExpiry( TI_HANDLE hScanSrv );

/**
 * \author Ronen Kalish\n
 * \date 17-Jan-2005\n
 * \brief Handles a FW reset event (one that was detected outside the scan SRV) by stopping the timer.
 *
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS scanSRVSM_handleRecovery( TI_HANDLE hScanSrv );

#endif /* __SCANSRVSM_H__ */
