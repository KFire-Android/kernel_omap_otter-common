/*
 * conn.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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

/** \file conn.c
 *  \brief connection module interface
 *
 *  \see conn.h
 */

/****************************************************************************************************/
/*																									*/
/*		MODULE:		conn.c																			*/
/*		PURPOSE:	Connection module interface. The connection	itself is implemented in the files	*/
/*					connInfra, connIbss & connSelf. This file distributes the events received to 	*/
/*					one of the modules based on the current connection type.						*/																 	
/*																									*/
/****************************************************************************************************/


#define __FILE_ID__  FILE_ID_25
#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "timer.h"
#include "conn.h"
#include "connApi.h"
#include "connIbss.h"
#include "connInfra.h"
#include "802_11Defs.h"
#include "smeApi.h"
#include "paramOut.h"
#include "siteMgrApi.h"
#include "sme.h"
#include "scrApi.h"
#include "healthMonitor.h"
#include "qosMngr_API.h"
#include "TWDriver.h"
#include "DrvMainModules.h"


/* Local functions prototypes */
static void release_module(conn_t *pConn);

static void conn_DisconnectComplete (conn_t *pConn, TI_UINT8  *data, TI_UINT8   dataLength);

/************************************************************************
 *                        conn_create								*
 ************************************************************************
DESCRIPTION: Connection module creation function, called by the config mgr in creation phase 
				performs the following:
				-	Allocate the connection handle
				-	Create the connection timer
				-	Create the connection state machine
                                                                                                   
INPUT:      hOs -			Handle to OS		


OUTPUT:		

RETURN:     Handle to the connection module on success, NULL otherwise

************************************************************************/
TI_HANDLE conn_create(TI_HANDLE hOs)
{
	conn_t *pConn;
	fsm_stateMachine_t *pFsm;
	TI_STATUS status;

	pConn = os_memoryAlloc(hOs, sizeof(conn_t));
	if (pConn == NULL)
	{
		return NULL;
	}
	os_memoryZero (pConn->hOs, pConn, sizeof(conn_t));

	/* Creating connection Ibss SM */
	status = fsm_Create(hOs, &pFsm, CONN_IBSS_NUM_STATES, CONN_IBSS_NUM_EVENTS);
	if (status != TI_OK)
	{
		release_module(pConn);
		return NULL;
	}
	pConn->ibss_pFsm = pFsm;

	/* Creating connection Infra SM */
   	status = fsm_Create(hOs, &pFsm, CONN_INFRA_NUM_STATES, CONN_INFRA_NUM_EVENTS);
	if (status != TI_OK)
	{
		release_module(pConn);
		return NULL;
	}
	pConn->infra_pFsm = pFsm;

	pConn->hOs = hOs;

	return(pConn);
}


/************************************************************************
 *                        conn_config									*
 ************************************************************************
DESCRIPTION: Connection module configuration function, called by the DrvMain in init phase.
				performs the following:
				-	Reset & initiailzes local variables
				-	Init the handles to be used by the module
                                                                                                   
INPUT:      List of handles to be used by the module

OUTPUT:		

RETURN:     void

************************************************************************/
void conn_init (TStadHandlesList *pStadHandles)
{
	conn_t *pConn = (conn_t *)(pStadHandles->hConn);

	pConn->state = 0;
	os_memoryZero (pConn->hOs, &(pConn->smContext), sizeof(connSmContext_t)); 

	pConn->hSiteMgr			= pStadHandles->hSiteMgr;
	pConn->hSmeSm			= pStadHandles->hSme;
	pConn->hMlmeSm			= pStadHandles->hMlmeSm;
	pConn->hRsn				= pStadHandles->hRsn;
	pConn->hRxData			= pStadHandles->hRxData;
	pConn->hReport			= pStadHandles->hReport;
	pConn->hOs				= pStadHandles->hOs;
	pConn->hPwrMngr			= pStadHandles->hPowerMgr;
	pConn->hCtrlData		= pStadHandles->hCtrlData;
	pConn->hMeasurementMgr	= pStadHandles->hMeasurementMgr;
	pConn->hTrafficMonitor  = pStadHandles->hTrafficMon;
	pConn->hScr				= pStadHandles->hSCR;
	pConn->hXCCMngr			= pStadHandles->hXCCMngr;
	pConn->hQosMngr			= pStadHandles->hQosMngr;
	pConn->hTWD		        = pStadHandles->hTWD;
    pConn->hScanCncn        = pStadHandles->hScanCncn;
	pConn->hCurrBss			= pStadHandles->hCurrBss;
	pConn->hSwitchChannel	= pStadHandles->hSwitchChannel;
	pConn->hEvHandler		= pStadHandles->hEvHandler;
	pConn->hHealthMonitor	= pStadHandles->hHealthMonitor;
	pConn->hTxMgmtQ		    = pStadHandles->hTxMgmtQ;
    pConn->hRegulatoryDomain= pStadHandles->hRegulatoryDomain;
    pConn->hTxCtrl          = pStadHandles->hTxCtrl;
    pConn->hTimer           = pStadHandles->hTimer;
	pConn->hSoftGemini		= pStadHandles->hSoftGemini;

    TRACE0(pConn->hReport, REPORT_SEVERITY_INIT, ".....Connection configured successfully\n");
}


TI_STATUS conn_SetDefaults (TI_HANDLE 	hConn, connInitParams_t		*pConnInitParams)
{
    conn_t *pConn = (conn_t *)hConn;

    pConn->timeout			   = pConnInitParams->connSelfTimeout;
	pConn->connType			   = CONN_TYPE_FIRST_CONN;
    pConn->ibssDisconnectCount = 0;

	/* allocate OS timer memory */
    pConn->hConnTimer = tmr_CreateTimer (pConn->hTimer);
	if (pConn->hConnTimer == NULL)
	{
        TRACE0(pConn->hReport, REPORT_SEVERITY_ERROR, "conn_SetDefaults(): Failed to create hConnTimer!\n");
		release_module (pConn);
		return TI_NOK;
	}
	
	TWD_RegisterEvent (pConn->hTWD,  
                       TWD_OWN_EVENT_JOIN_CMPLT, 
					   (void *)connInfra_JoinCmpltNotification, 
                       pConn);

	TWD_EnableEvent (pConn->hTWD, TWD_OWN_EVENT_JOIN_CMPLT);
	
	 /* Register for 'TWD_OWN_EVENT_DISCONNECT_COMPLETE' event */
    TWD_RegisterEvent (pConn->hTWD, TWD_OWN_EVENT_DISCONNECT_COMPLETE, (void *)conn_DisconnectComplete, pConn);
	TWD_EnableEvent (pConn->hTWD, TWD_OWN_EVENT_DISCONNECT_COMPLETE);

	return TI_OK;
}

/************************************************************************
 *                        conn_unLoad									*
 ************************************************************************
DESCRIPTION: Connection module unload function, called by the config mgr in the unlod phase 
				performs the following:
				-	Free all memory aloocated by the module
                                                                                                   
INPUT:      hConn	-	Connection handle.		


OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_unLoad(TI_HANDLE hConn)
{
	conn_t			*pConn = (conn_t *)hConn;

	if (!pConn)
    {
		return TI_OK;
    }

	release_module(pConn);

	return TI_OK;
}

/***********************************************************************
 *                        conn_setParam									
 ***********************************************************************
DESCRIPTION: Connection set param function, called by the following:
				-	config mgr in order to set a parameter from the OS abstraction layer.
				-	Form inside the driver
				In this fuction, the site manager configures the connection type in the select phase.
				The connection type is used to distribute the connection events to the corresponding connection SM	
                                                                                                   
INPUT:      hConn	-	Connection handle.
			pParam	-	Pointer to the parameter		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_setParam(TI_HANDLE		hConn, 
					 paramInfo_t	*pParam)
{
	conn_t *pConn = (conn_t *)hConn;

	switch(pParam->paramType)
	{
	case CONN_TYPE_PARAM:
		pConn->currentConnType = pParam->content.connType;
		switch (pParam->content.connType)
		{
		case CONNECTION_IBSS:
		case CONNECTION_SELF:
			return conn_ibssConfig(pConn);

		case CONNECTION_INFRA:
			return conn_infraConfig(pConn);

		default:
TRACE1(pConn->hReport, REPORT_SEVERITY_ERROR, "Set connection type, type is not valid, %d\n\n", pParam->content.connType);
			return PARAM_VALUE_NOT_VALID;
		}

	case CONN_SELF_TIMEOUT_PARAM:
		if ((pParam->content.connSelfTimeout < CONN_SELF_TIMEOUT_MIN) || (pParam->content.connSelfTimeout > CONN_SELF_TIMEOUT_MAX))
			return PARAM_VALUE_NOT_VALID;
		pConn->timeout = pParam->content.connSelfTimeout;
		break;

	default:
TRACE1(pConn->hReport, REPORT_SEVERITY_ERROR, "Set param, Params is not supported, %d\n\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

	return TI_OK;
}

/***********************************************************************
 *                        conn_getParam									
 ***********************************************************************
DESCRIPTION: Connection get param function, called by the following:
			-	config mgr in order to get a parameter from the OS abstraction layer.
			-	Fomr inside the dirver	
                                                                                                   
INPUT:      hConn	-	Connection handle.
			pParam	-	Pointer to the parameter		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_getParam(TI_HANDLE		hConn, 
					 paramInfo_t	*pParam)
{
	conn_t *pConn = (conn_t *)hConn;

	switch(pParam->paramType)
	{
	case CONN_TYPE_PARAM:
		pParam->content.connType = pConn->currentConnType;
		break;

	case CONN_SELF_TIMEOUT_PARAM:
		pParam->content.connSelfTimeout = pConn->timeout;
		break;
	
	default:
TRACE1(pConn->hReport, REPORT_SEVERITY_ERROR, "Get param, Params is not supported, %d\n\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

	return TI_OK;
}

/***********************************************************************
 *                        conn_start									
 ***********************************************************************
DESCRIPTION: Called by the SME SM in order to start the connection SM
			 This function start the current connection SM	
                                                                                                   
INPUT:      hConn	-	Connection handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_start(TI_HANDLE hConn, 
                     EConnType connType,
		     conn_status_callback_t  pConnStatusCB,
		     TI_HANDLE connStatCbObj,
   		     TI_BOOL disConEraseKeys,
		     TI_BOOL reNegotiateTspec)
{
	conn_t *pConn = (conn_t *)hConn;
	paramInfo_t param;

	pConn->pConnStatusCB = pConnStatusCB;
	pConn->connStatCbObj = connStatCbObj;

	pConn->connType = connType;
	pConn->disConEraseKeys = disConEraseKeys;
	
	/* Initialize the DISASSOCIATE event parameters to default */ 
	pConn->smContext.disAssocEventReason = STATUS_UNSPECIFIED;
	pConn->smContext.disAssocEventStatusCode  = 0;

	
	/* If requested, re-negotiate voice TSPEC */
	param.paramType = QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC;
	param.content.TspecConfigure.voiceTspecConfigure = reNegotiateTspec; 
    param.content.TspecConfigure.videoTspecConfigure = reNegotiateTspec; 

	qosMngr_setParams(pConn->hQosMngr, &param);

	switch(pConn->currentConnType)
	{
	case CONNECTION_IBSS:
		return conn_ibssSMEvent(&pConn->state, CONN_IBSS_CONNECT, (TI_HANDLE) pConn);

	case CONNECTION_SELF:
		return conn_ibssSMEvent(&pConn->state, CONN_IBSS_CREATE, (TI_HANDLE) pConn);

	case CONNECTION_INFRA:
		return conn_infraSMEvent(&pConn->state, CONN_INFRA_CONNECT, (TI_HANDLE) pConn);

    default:
TRACE1(pConn->hReport, REPORT_SEVERITY_ERROR, "Start connection, invalid type %d\n\n", pConn->currentConnType);
		return TI_NOK;

	}
}

/***********************************************************************
 *                        conn_stop									
 ***********************************************************************
DESCRIPTION: Called by the SME SM in order to stop the connection SM
			 This function stop the current connection SM.	
                                                                                                   
INPUT:      hConn	-	Connection handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_stop(TI_HANDLE 				hConn, 
					DisconnectType_e		disConnType, 
					mgmtStatus_e 			reason,
					TI_BOOL					disConEraseKeys,
					conn_status_callback_t  pConnStatusCB,
					TI_HANDLE 				connStatCbObj  )
{
	conn_t *pConn = (conn_t *)hConn;

	pConn->pConnStatusCB = pConnStatusCB;
	pConn->connStatCbObj = connStatCbObj;

	pConn->disConnType 	 = disConnType;
	pConn->disConnReasonToAP = reason;
	pConn->disConEraseKeys = disConEraseKeys;

	/* 
	 * Mark the disconnection reason as unspecified to indicate that conn module has no information regarding the DISASSOCIATE event to be raised
	 * by the SME
	 */
	pConn->smContext.disAssocEventReason = STATUS_UNSPECIFIED;
	pConn->smContext.disAssocEventStatusCode  = 0;


    TRACE3(pConn->hReport, REPORT_SEVERITY_INFORMATION, "conn_stop, disConnType %d, reason=%d, disConEraseKeys=%d\n\n", disConnType, reason, disConEraseKeys);

	switch(pConn->currentConnType)
	{
	case CONNECTION_IBSS:
	case CONNECTION_SELF:
        pConn->ibssDisconnectCount++;
		return conn_ibssSMEvent(&pConn->state, CONN_IBSS_DISCONNECT, (TI_HANDLE) pConn);

	case CONNECTION_INFRA:
		return conn_infraSMEvent(&pConn->state, CONN_INFRA_DISCONNECT, (TI_HANDLE) pConn);


	default:
TRACE1(pConn->hReport, REPORT_SEVERITY_ERROR, "Stop connection, invalid type %d\n\n", pConn->currentConnType);
		return TI_NOK;
	}
}


/***********************************************************************
 *                        conn_reportMlmeStatus									
 ***********************************************************************
DESCRIPTION:	Called by the MLME SM when MLME status changed. 
				Valid only in the case that the current connection type is infrastructure
				The function calls the connection infra SM with MLME success or MLME failure 
				according to the status
                                                                                                   
INPUT:      hConn	-	Connection handle.
			status	-	MLME status

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_reportMlmeStatus(TI_HANDLE			hConn, 
							mgmtStatus_e		status,
							TI_UINT16				uStatusCode)
{
	conn_t *pConn = (conn_t *)hConn;


	/* Save the reason for the use of the SME when triggering DISASSOCIATE event */ 
	pConn->smContext.disAssocEventReason = status;
	pConn->smContext.disAssocEventStatusCode = uStatusCode;

	if (status == STATUS_SUCCESSFUL)
	{
		conn_infraSMEvent(&pConn->state, CONN_INFRA_MLME_SUCC, pConn);
	}
	else
	{
        TRACE0(pConn->hReport, REPORT_SEVERITY_CONSOLE,"-------------------------------------\n");
        TRACE0(pConn->hReport, REPORT_SEVERITY_CONSOLE,"               CONN LOST             \n");
        TRACE0(pConn->hReport, REPORT_SEVERITY_CONSOLE,"-------------------------------------\n");

#ifdef REPORT_LOG
		WLAN_OS_REPORT(("-------------------------------------\n"));
		WLAN_OS_REPORT(("               CONN LOST             \n"));
		WLAN_OS_REPORT(("-------------------------------------\n"));
#else
		os_printf("%s: *** CONN LOST ***\n", __func__);
#endif
		if( pConn->connType == CONN_TYPE_ROAM )
			pConn->disConnType = DISCONNECT_IMMEDIATE;
		else /* connType == CONN_TYPE_ESS */
			pConn->disConnType = DISCONNECT_DE_AUTH;

        TRACE4(pConn->hReport, REPORT_SEVERITY_INFORMATION, "conn_reportMlmeStatus, disAssocEventReason %d, disAssocEventStatusCode = %d, connType=%d, disConnType=%d \n", pConn->smContext.disAssocEventReason, pConn->smContext.disAssocEventStatusCode, pConn->connType, pConn->disConnType);

		conn_infraSMEvent(&pConn->state, CONN_INFRA_DISCONNECT, pConn);
	}

	return TI_OK;
}

/***********************************************************************
 *                        conn_reportRsnStatus									
 ***********************************************************************
DESCRIPTION:	Called by the RSN SM when RSN status changed. 
				This function calls the current connection SM with RSN success or RSN failure based on the status	
                                                                                                   
INPUT:      hConn	-	Connection handle.
			status	-	RSN status

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_reportRsnStatus(TI_HANDLE			hConn, 
							mgmtStatus_e		status)
{
	conn_t *pConn = (conn_t *)hConn;

	/* Save the reason for the use of the SME when triggering DISASSOCIATE event. For now we just have STATUS_SECURITY_FAILURE */ 
	pConn->smContext.disAssocEventReason = status;
	pConn->smContext.disAssocEventStatusCode = 1; /* we use this status at SME, if != 0 means that assoc frame sent */

	switch(pConn->currentConnType)
	{
	case CONNECTION_IBSS:
	case CONNECTION_SELF:
		if (status == STATUS_SUCCESSFUL)
			return conn_ibssSMEvent(&pConn->state, CONN_IBSS_RSN_SUCC, (TI_HANDLE) pConn);
		else
			return conn_ibssSMEvent(&pConn->state, CONN_IBSS_DISCONNECT, (TI_HANDLE) pConn);



	case CONNECTION_INFRA:
		if (status == STATUS_SUCCESSFUL)
			return conn_infraSMEvent(&pConn->state, CONN_INFRA_RSN_SUCC, (TI_HANDLE) pConn);
		
		else{ /* status == STATUS_SECURITY_FAILURE */
			/*
			 * In infrastructure - if the connection is standard 802.11 connection (ESS) then
			 * need to disassociate. In roaming mode, the connection is stopped without sending
			 * the reassociation frame.
			 */
			if( pConn->connType == CONN_TYPE_ROAM )
				pConn->disConnType = DISCONNECT_IMMEDIATE;
			else /* connType == CONN_TYPE_ESS */
				pConn->disConnType = DISCONNECT_DE_AUTH;

            TRACE3(pConn->hReport, REPORT_SEVERITY_INFORMATION, "conn_reportRsnStatus, disAssocEventReason %d, connType=%d, disConnType=%d \n\n", pConn->smContext.disAssocEventReason, pConn->connType, pConn->disConnType);

			return conn_infraSMEvent(&pConn->state, CONN_INFRA_DISCONNECT, (TI_HANDLE) pConn);
		}
	case CONNECTION_NONE:
		break;
	}
	
	return TI_OK;
}

/***********************************************************************
 *                        conn_timeout									
 ***********************************************************************
DESCRIPTION:	Called by the OS abstraction layer when the self timer expired 
				Valid only if the current connection type is self
				This function calls the self connection SM with timeout event
                                                                                                   
INPUT:      hConn	-	Connection handle.
            bTwdInitOccured -   Indicates if TWDriver recovery occured since timer started 

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
void conn_timeout (TI_HANDLE hConn, TI_BOOL bTwdInitOccured)
{
	conn_t *pConn = (conn_t *)hConn;

	switch(pConn->currentConnType)
	{
	case CONNECTION_IBSS:
	case CONNECTION_SELF:
		conn_ibssSMEvent(&pConn->state, CONN_IBSS_DISCONNECT, pConn);
		break;

	case CONNECTION_INFRA:
		conn_infraSMEvent(&pConn->state, CONN_INFRA_DISCONN_COMPLETE, (TI_HANDLE) pConn);
        /* Initiate recovery only if not already occured. */
        if (!bTwdInitOccured) 
        {
		healthMonitor_sendFailureEvent(pConn->hHealthMonitor, DISCONNECT_TIMEOUT);
        }
		break;

	case CONNECTION_NONE:
		break;
	}

	return;
}


/***********************************************************************
 *                        conn_join									
 ***********************************************************************
DESCRIPTION:	Called by the site manager when detecting that another station joined our own created IBSS 
				Valid only if the current connection type is self
				This function calls the self connection SM with join event
                                                                                                   
INPUT:      hConn	-	Connection handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_ibssStaJoined(TI_HANDLE hConn)
{
	conn_t *pConn = (conn_t *)hConn;

	conn_ibssSMEvent(&pConn->state, CONN_IBSS_STA_JOINED, pConn);
	return TI_OK;
}


TI_STATUS conn_ibssMerge(TI_HANDLE hConn)
{
	conn_t *pConn = (conn_t *)hConn;

	conn_ibssSMEvent(&pConn->state, CONN_IBSS_MERGE, pConn);
	return TI_OK;
}



/***********************************************************************
 *                        release_module									
 ***********************************************************************
DESCRIPTION:	Release all module resources - FSMs, timer and object.
                                                                                                   
INPUT:      hConn	-	Connection handle.

OUTPUT:		

RETURN:     void

************************************************************************/
static void release_module(conn_t *pConn)
{
	if (pConn->ibss_pFsm)
    {
		fsm_Unload (pConn->hOs, pConn->ibss_pFsm);
    }

    if (pConn->infra_pFsm)
    {
		fsm_Unload (pConn->hOs, pConn->infra_pFsm);
    }

	if (pConn->hConnTimer)
    {
		tmr_DestroyTimer (pConn->hConnTimer);
    }

	os_memoryFree(pConn->hOs, pConn, sizeof(conn_t));
}

static void conn_DisconnectComplete (conn_t *pConn, TI_UINT8  *data, TI_UINT8   dataLength)
{
	switch(pConn->currentConnType)
	{
	case CONNECTION_IBSS:
		connIbss_DisconnectComplete(pConn, data, dataLength);
		break;

	case CONNECTION_SELF:
		connIbss_DisconnectComplete(pConn, data, dataLength);
		break;

	case CONNECTION_INFRA:
		connInfra_DisconnectComplete(pConn, data, dataLength);
		break;

    default:
		TRACE1(pConn->hReport, REPORT_SEVERITY_ERROR, "conn_DisconnectComplete, invalid type %d\n\n", pConn->currentConnType);
		
	}
}


#ifdef REPORT_LOG
/**
*
* conn_ibssPrintStatistics
*
* \b Description: 
*
* Called by Site Manager when request to print statistics is requested from CLI  
*
* \b ARGS: Connection handle
*
* \b RETURNS:
*
*  None.
*
* \sa 
*/
void conn_ibssPrintStatistics(TI_HANDLE hConn)
{   
    conn_t *pConn = (conn_t *)hConn;

    WLAN_OS_REPORT(("- IBSS Disconnect = %d\n", pConn->ibssDisconnectCount));
    WLAN_OS_REPORT(("\n"));
}
#endif /*#ifdef REPORT_LOG*/


