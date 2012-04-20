/*
 * MeasurementSrvSM.h
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

/** \file MeasurementSrv.h
 *  \brief This file include private definitions for the Measurement SRV state machine.
 *  \author Ronen Kalish
 *  \date 08-November-2005
 */

#ifndef __MEASUREMENT_SRV_SM_H__
#define __MEASUREMENT_SRV_SM_H__


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

/** \enum measurement_SRVSMEvents_e
 * \brief enumerates the different measurement SRV state machine events.
 */
typedef enum
{
    MSR_SRV_EVENT_MEASURE_START_REQUEST       = 0,
    MSR_SRV_EVENT_DRIVER_MODE_SUCCESS,
    MSR_SRV_EVENT_DRIVER_MODE_FAILURE,
    MSR_SRV_EVENT_START_SUCCESS,
    MSR_SRV_EVENT_START_FAILURE,
    MSR_SRV_EVENT_ALL_TYPES_COMPLETE,
    MSR_SRV_EVENT_STOP_COMPLETE,
    MSR_SRV_EVENT_MEASURE_STOP_REQUEST,
    MSR_SRV_NUM_OF_EVENTS
} measurement_SRVSMEvents_e;

/** \enum measurement_SRVSMStates_e
 * \brief enumerates the different measurement SRV state machine states.
 */
typedef enum
{
    MSR_SRV_STATE_IDLE                      =0,
    MSR_SRV_STATE_WAIT_FOR_DRIVER_MODE,
    MSR_SRV_STATE_WAIT_FOR_MEASURE_START,
    MSR_SRV_STATE_MEASURE_IN_PROGRESS,
    MSR_SRV_STATE_WAIT_FOR_MEASURE_STOP,
    MSR_SRV_NUM_OF_STATES
} measurements_SRVSMStates_e;


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
 * \date 08-November-2005\n
 * \brief Initialize the measurement SRV SM.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS measurementSRVSM_init( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief Processes an event.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the measurement SRV object.\n
 * \param currentState - the current scan SRV SM state.\n
 * \param event - the event to handle.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
TI_STATUS measurementSRVSM_SMEvent( TI_HANDLE hMeasurementSrv, measurements_SRVSMStates_e* currentState, 
                                    measurement_SRVSMEvents_e event );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief Handle a MEASURE_START_REQUEST event by requesting driver mode.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_requestDriverMode( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief Handle a DRIVER_MODE_SUCCESS event by sending start measure command to the FW.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_requestMeasureStart( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief Handle a START_SUCCESS event by starting different measure types and setting timers.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_startMeasureTypes( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief Handle an ALL_TYPE_COMPLETE event by sending a stop measure command to the FW.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_requestMeasureStop( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief Handle a STOP_COMPLETE event by exiting driver mode and calling the complete CB.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_completeMeasure( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief Handle a STOP_REQUEST event when in WAIT_FOR_DRIVER_MODE state by exiting driver mode.
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_stopFromWaitForDriverMode( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 27-November-2005\n
 * \brief handle a STOP_REQUEST event when in WAIT_FOR_DRIVER_MODE by marking negative result status
 * \brief and callin the ordinary stop function
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_stopFromWaitForMeasureStart( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief handle a STOP_REQUEST event when in MEASURE_IN_PROGRESS by stopping all measure types and
 * \brief requesting measure stop from the FW.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_stopFromMeasureInProgress( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief handle a DRIVER_MODE_FAILURE event by calling the response CB.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_DriverModeFailure( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 08-November-2005\n
 * \brief handle a START_FAILURE event by exiting driver mode and calling the complete CB.\n
 *
 * Function Scope \e Public.\n
 * \param hMeasurementSrv - handle to the Measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSM_measureStartFailure( TI_HANDLE hMeasurementSRV );

/**
 * \author Ronen Kalish\n
 * \date 23-December-2005\n
 * \brief Handles a stop request when no stop is needed (SM is either idle or already send stop command to FW.\n
 *
 * Function Scope \e Private.\n
 * \param hMeasurementSrv - handle to the measurement SRV object.\n
 * \return always TI_OK.\n
 */
TI_STATUS measurementSRVSRVSM_dummyStop( TI_HANDLE hmeasurementSrv );

#endif /* __MEASUREMENT_SRV_SM_H__ */

