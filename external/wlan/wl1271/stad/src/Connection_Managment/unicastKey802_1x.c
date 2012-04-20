/*
 * unicastKey802_1x.c
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

/** \file unicastKey802_1x.c
 * \brief station unicast key 802_1x implementation
 *
 * \see unicastKey802_1x.h
*/

/****************************************************************************
 *                                                                          *
 *   MODULE:	station unicast key 802_1x		                                *
 *   PURPOSE:   station unicast key 802_1x implementation						*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_44
#include "osApi.h"
#include "report.h"
#include "rsnApi.h"

#include "unicastKey802_1x.h"
#include "mainKeysSm.h"

/** number of states in the state machine */
#define	UCAST_KEY_802_1X_MAX_NUM_STATES		3

/** number of events in the state machine */
#define	UCAST_KEY_802_1X_MAX_NUM_EVENTS		4


TI_STATUS unicastKey802_1x_start(struct _unicastKey_t *pUnicastKey);

TI_STATUS unicastKey802_1x_stop(struct _unicastKey_t *pUnicastKey);

TI_STATUS unicastKey802_1x_recvSuccess(struct _unicastKey_t *pUnicastKey, 
									encodedKeyMaterial_t *pEncodedKeyMaterial);

TI_STATUS unicastKey802_1x_recvFailure(struct _unicastKey_t *pUnicastKey);

TI_STATUS unicastKey802_1x_distribute(struct _unicastKey_t *pUnicastKey);

TI_STATUS unicastKey802_1x_redistribute(struct _unicastKey_t *pUnicastKey);

TI_STATUS unicastKey802_1x_event(struct _unicastKey_t *pUnicastKey,
							  TI_UINT8 event, 
							  void *pData);



/**
*
* Function  - Config KEY Parser module.
*
* \b Description: 
*
* Called by RSN Manager. 
* Registers the function 'rsn_UnicastKeyRecv()' at the distributor to receive KEY frames upon receiving a KEY_RECV event.
*
* \b ARGS:
*
*  
* \b RETURNS:
*
*  TI_STATUS - 0 on success, any other value on failure. 
*
*/

TI_STATUS unicastKey802_1x_config(struct _unicastKey_t *pUnicastKey)
{
	TI_STATUS		status = TI_NOK;

	/** 802.1X Station unicast key State Machine matrix */
	fsm_actionCell_t    unicastKey802_1x_matrix[UCAST_KEY_802_1X_NUM_STATES][UCAST_KEY_802_1X_NUM_EVENTS] =
	{
		/* next state and actions for IDLE state */
		{	{UCAST_KEY_802_1X_STATE_START, (fsm_Action_t)unicastKeySmNop},
			{UCAST_KEY_802_1X_STATE_IDLE, (fsm_Action_t)unicastKeySmNop},
			{UCAST_KEY_802_1X_STATE_IDLE, (fsm_Action_t)unicastKeySmNop},
			{UCAST_KEY_802_1X_STATE_IDLE, (fsm_Action_t)unicastKeySmUnexpected}
		},
	
		/* next state and actions for START state */
		{	{UCAST_KEY_802_1X_STATE_START, (fsm_Action_t)unicastKeySmUnexpected},
			{UCAST_KEY_802_1X_STATE_IDLE, (fsm_Action_t)unicastKeySmNop},
			{UCAST_KEY_802_1X_STATE_COMPLETE, (fsm_Action_t)unicastKey802_1x_distribute},
			{UCAST_KEY_802_1X_STATE_START, (fsm_Action_t)unicastKeySmNop}
		},
	
		/* next state and actions for COMPLETE state */
		{	{UCAST_KEY_802_1X_STATE_COMPLETE, (fsm_Action_t)unicastKeySmUnexpected},
			{UCAST_KEY_802_1X_STATE_IDLE, (fsm_Action_t)unicastKeySmNop},
			{UCAST_KEY_802_1X_STATE_COMPLETE, (fsm_Action_t)unicastKey802_1x_distribute},
			{UCAST_KEY_802_1X_STATE_COMPLETE, (fsm_Action_t)unicastKeySmUnexpected}
		}
	};


	pUnicastKey->start = unicastKey802_1x_start;
	pUnicastKey->stop = unicastKey802_1x_stop;
	pUnicastKey->recvFailure = unicastKey802_1x_recvFailure;
	pUnicastKey->recvSuccess = unicastKey802_1x_recvSuccess;

	pUnicastKey->currentState = UCAST_KEY_802_1X_STATE_IDLE;

	status = fsm_Config(pUnicastKey->pUcastKeySm, 
						&unicastKey802_1x_matrix[0][0], 
						UCAST_KEY_802_1X_NUM_STATES, 
						UCAST_KEY_802_1X_NUM_EVENTS, 
						NULL, pUnicastKey->hOs);

	
	return status;
}



/**
*
* unicastKey802_1x_event
*
* \b Description: 
*
* 802.1x station unicast key state machine transition function
*
* \b ARGS:
*
*  I/O - currentState - current state in the state machine\n
*  I   - event - specific event for the state machine\n
*  I   - pData - Data for state machine action function\n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS unicastKey802_1x_event(struct _unicastKey_t *pUnicastKey, TI_UINT8 event, void *pData)
{
	TI_STATUS 		status;
	TI_UINT8		nextState;

	status = fsm_GetNextState(pUnicastKey->pUcastKeySm, pUnicastKey->currentState, event, &nextState);
	if (status != TI_OK)
	{
TRACE0(pUnicastKey->hReport, REPORT_SEVERITY_ERROR, "UNICAST_KEY_802_1x: ERROR: failed getting next state\n");
		return TI_NOK;
	}

TRACE3(pUnicastKey->hReport, REPORT_SEVERITY_INFORMATION, "STATION_UNICAST_KEY_802_1x: <currentState = %d, event = %d> --> nextState = %d\n", pUnicastKey->currentState, event, nextState);

	status = fsm_Event(pUnicastKey->pUcastKeySm, &pUnicastKey->currentState, event, pData);

	return status;
}


/**
*
* unicastKey802_1x_start
*
* \b Description: 
*
* START event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa unicastKey802_1x_stop()
*/
TI_STATUS unicastKey802_1x_start(struct _unicastKey_t *pUnicastKey)
{
	TI_STATUS  status;
	
	status = unicastKey802_1x_event(pUnicastKey, UCAST_KEY_802_1X_EVENT_START, pUnicastKey);

	return status;
}


/**
*
* unicastKey802_1x_stop
*
* \b Description: 
*
* START event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa unicastKey802_1x_start()
*/
TI_STATUS unicastKey802_1x_stop(struct _unicastKey_t *pUnicastKey)
{
	TI_STATUS  status;

	status = unicastKey802_1x_event(pUnicastKey, UCAST_KEY_802_1X_EVENT_STOP, pUnicastKey);

	return status;
}


/**
*
* unicastKey802_1x_recvSuccess
*
* \b Description: 
*
* SUCCESS event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*  I   - pEncodedKeyMaterial - Encoded key material \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
*/
TI_STATUS unicastKey802_1x_recvSuccess(struct _unicastKey_t *pUnicastKey, encodedKeyMaterial_t *pEncodedKeyMaterial)
{
	TI_STATUS  status;

	pUnicastKey->data.pEncodedKeyMaterial = pEncodedKeyMaterial;

	status = unicastKey802_1x_event(pUnicastKey, UCAST_KEY_802_1X_EVENT_SUCCESS, pUnicastKey);

	return status;
}


/**
*
* unicastKey802_1x_recvFailure
*
* \b Description: 
*
* FAILURE event handler
*
* \b ARGS:
*
*  I   - pCtrlB - station control block  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
*/
TI_STATUS unicastKey802_1x_recvFailure(struct _unicastKey_t *pUnicastKey)
{
	TI_STATUS  status;
	
	status = unicastKey802_1x_event(pUnicastKey, UCAST_KEY_802_1X_EVENT_FAILURE, pUnicastKey);

	return status;
}


/**
*
* unicastKey802_1x_distribute
*
* \b Description: 
*
* Distribute unicast key material to the driver and report the main key SM on unicast complete.
*
* \b ARGS:
*
*  I   - pData - Encoded key material  \n
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*/
TI_STATUS unicastKey802_1x_distribute(struct _unicastKey_t *pUnicastKey)
{
	TI_STATUS  status=TI_NOK;
	
	if (pUnicastKey->pKeyDerive->derive!=NULL)
    {
	status = pUnicastKey->pKeyDerive->derive(pUnicastKey->pKeyDerive, 
												   pUnicastKey->data.pEncodedKeyMaterial);
    }
	if (status != TI_OK)
	{
		return TI_NOK;
	}

	if (pUnicastKey->pParent->setDefaultKeyId!=NULL)
    {
	status = pUnicastKey->pParent->setDefaultKeyId(pUnicastKey->pParent,
												   (TI_UINT8)pUnicastKey->data.pEncodedKeyMaterial->keyId);
    }
	if (status != TI_OK)
	{
		return status;
	}

	if (pUnicastKey->pParent->reportUcastStatus!=NULL)
    {
	status = pUnicastKey->pParent->reportUcastStatus(pUnicastKey->pParent, TI_OK);
    }

	return status;
}
