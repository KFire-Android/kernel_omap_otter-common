/*
 * connIbss.c
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

/** \file connIbss.c
 *  \brief IBSS connection implementation
 *
 *  \see connIbss.h
 */

/***************************************************************************/
/*                                                                                                  */
/*      MODULE: connIbss.c                                                              */
/*    PURPOSE:  IBSS connection implementation                                          */
/*                                                                                                  */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_26
#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "conn.h"
#include "connIbss.h"
#include "timer.h"
#include "fsm.h"
#include "siteMgrApi.h"
#include "sme.h"
#include "rsnApi.h"
#include "DataCtrl_Api.h"  
#include "paramOut.h"
#include "connApi.h"
#include "EvHandler.h"
#include "currBss.h"
#include "TrafficMonitorAPI.h"
#include "healthMonitor.h"
#include "TWDriver.h"


/* Local functions prototypes */
/* Local functions prototypes */
static TI_STATUS waitDisconnToCmplt_to_idle (void *pData);
static TI_STATUS idle_to_selfWait(void *pData);

static TI_STATUS idle_to_rsnWait(void *pData);
    
static TI_STATUS selfWait_to_waitToDisconnCmplt(void *pData);
static TI_STATUS rsnWait_to_waitToDisconnCmplt(void *pData);
static TI_STATUS connected_to_waitToDisconnCmplt(void *pData);
static TI_STATUS selfWait_to_rsnWait(void *pData);
static TI_STATUS rsnWait_to_connected(void *pData);
static TI_STATUS actionUnexpected(void *pData);
static TI_STATUS actionNop(void *pData);
static TI_STATUS selfw_merge_rsnw(void *pData);
static TI_STATUS rsnw_merge_rsnw(void *pData);
static TI_STATUS conn_merge_conn(void *pData);

/********************************************/
/*      Functions Implementations           */
/********************************************/

/***********************************************************************
 *                        conn_ibssConfig                                   
 ***********************************************************************
DESCRIPTION: IBSS Connection configuration function, called by the conection set param function
                in the selection phase. Configures the connection state machine to IBSS connection mode
                                                                                                   
INPUT:      hConn   -   Connection handle.

OUTPUT:     

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_ibssConfig(conn_t *pConn)
{

    fsm_actionCell_t    smMatrix[CONN_IBSS_NUM_STATES][CONN_IBSS_NUM_EVENTS] =
    {

        /* next state and actions for IDLE state */
        {   {STATE_CONN_IBSS_SELF_WAIT, idle_to_selfWait    },  /* CONN_IBSS_CREATE */
            {STATE_CONN_IBSS_RSN_WAIT,  idle_to_rsnWait     },  /* CONN_IBSS_CONNECT    */
            {STATE_CONN_IBSS_IDLE,      actionNop           },   /* CONN_IBSS_DISCONNECT */
            {STATE_CONN_IBSS_IDLE,      actionUnexpected    },   /* CONN_IBSS_RSN_SUCC */
            {STATE_CONN_IBSS_IDLE,      actionUnexpected    },   /* CONN_IBSS_STA_JOINED */
			{STATE_CONN_IBSS_IDLE,      actionUnexpected    },   /* CONN_IBSS_MERGE */
			{STATE_CONN_IBSS_IDLE,      actionUnexpected    }    /* CONN_IBSS_DISCONN_COMPLETE */
        },

        /* next state and actions for SELF_WAIT state */
        {   {STATE_CONN_IBSS_SELF_WAIT, actionUnexpected    							},  /* CONN_IBSS_CREATE */
            {STATE_CONN_IBSS_SELF_WAIT, actionUnexpected    							},  /* CONN_IBSS_CONNECT    */
            {STATE_CONN_IBSS_WAIT_DISCONN_CMPLT,  selfWait_to_waitToDisconnCmplt 		},  /* CONN_IBSS_DISCONNECT */
            {STATE_CONN_IBSS_SELF_WAIT, actionUnexpected    							},  /* CONN_IBSS_RSN_SUCC */
            {STATE_CONN_IBSS_RSN_WAIT,  selfWait_to_rsnWait 							},  /* CONN_IBSS_STA_JOINED */
			{STATE_CONN_IBSS_RSN_WAIT,  selfw_merge_rsnw    							},  /* CONN_IBSS_MERGE */
			{STATE_CONN_IBSS_SELF_WAIT, actionUnexpected    							}   /* CONN_IBSS_DISCONN_COMPLETE */
        },

        /* next state and actions for RSN_WAIT state */
        {   {STATE_CONN_IBSS_RSN_WAIT,  actionUnexpected    							},  /* CONN_IBSS_CREATE */
            {STATE_CONN_IBSS_RSN_WAIT,  actionUnexpected    							},  /* CONN_IBSS_CONNECT    */
            {STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, rsnWait_to_waitToDisconnCmplt    		},  /* CONN_IBSS_DISCONNECT */
            {STATE_CONN_IBSS_CONNECTED, rsnWait_to_connected							},  /* CONN_IBSS_RSN_SUCC */
			{STATE_CONN_IBSS_RSN_WAIT,  actionUnexpected								}, 	/* CONN_IBSS_STA_JOINED */
			{STATE_CONN_IBSS_RSN_WAIT,  rsnw_merge_rsnw     							},  /* CONN_IBSS_MERGE */
			{STATE_CONN_IBSS_RSN_WAIT, actionUnexpected    								}   /* CONN_IBSS_DISCONN_COMPLETE */
        },
		
        /* next state and actions for CONNECTED state */
        {   {STATE_CONN_IBSS_CONNECTED, actionUnexpected    							},  /* CONN_IBSS_CREATE */
            {STATE_CONN_IBSS_CONNECTED, actionUnexpected    							},  /* CONN_IBSS_CONNECT    */
            {STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, connected_to_waitToDisconnCmplt  		},  /* CONN_IBSS_DISCONNECT */
            {STATE_CONN_IBSS_CONNECTED, actionUnexpected    							},  /* CONN_IBSS_RSN_SUCC */
			{STATE_CONN_IBSS_CONNECTED, actionUnexpected    							},  /* CONN_IBSS_STA_JOINED */
			{STATE_CONN_IBSS_CONNECTED,  conn_merge_conn    							},  /* CONN_IBSS_MERGE */
			{STATE_CONN_IBSS_CONNECTED, actionUnexpected    						   	}   /* CONN_IBSS_DISCONN_COMPLETE */
        },

		 /* next state and actions for STATE_CONN_IBSS_WAIT_DISCONN_CMPLT state */
        {   {STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, actionUnexpected    		},  		/* CONN_IBSS_CREATE */
            {STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, actionUnexpected    		},  		/* CONN_IBSS_CONNECT    */
            {STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, actionUnexpected   		},  		/* CONN_IBSS_DISCONNECT */
            {STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, actionUnexpected    		},  		/* CONN_IBSS_RSN_SUCC */
			{STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, actionUnexpected    		},  		/* CONN_IBSS_STA_JOINED */
			{STATE_CONN_IBSS_WAIT_DISCONN_CMPLT, actionUnexpected     		},  		/* CONN_IBSS_MERGE */
			{STATE_CONN_IBSS_IDLE, 				 waitDisconnToCmplt_to_idle } 			/* CONN_IBSS_DISCONN_COMPLETE */
        }
        
    };

    return fsm_Config(pConn->ibss_pFsm, (fsm_Matrix_t)smMatrix, CONN_IBSS_NUM_STATES, CONN_IBSS_NUM_EVENTS, conn_ibssSMEvent, pConn->hOs);
}


/***********************************************************************
 *                        conn_ibssSMEvent                                  
 ***********************************************************************
DESCRIPTION: IBSS Connection SM event processing function, called by the connection API
                Perform the following:
                -   Print the state movement as a result from the event
                -   Calls the generic state machine event processing function which preform the following:
                    -   Calls the correspoding callback function
                    -   Move to next state
                
INPUT:      currentState    -   Pointer to the connection current state.
            event   -   Received event
            pConn   -   Connection handle

OUTPUT:     

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS conn_ibssSMEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hConn)
{
   conn_t *pConn = (conn_t *)hConn;
    TI_STATUS       status;
    TI_UINT8       nextState;

    status = fsm_GetNextState(pConn->ibss_pFsm, *currentState, event, &nextState);
    if (status != TI_OK)
    {
        TRACE0(pConn->hReport, REPORT_SEVERITY_SM, "IBSS State machine error, failed getting next state\n");
        return(TI_NOK);
    }

	TRACE3( pConn->hReport, REPORT_SEVERITY_INFORMATION, "conn_ibssSMEvent: <currentState = %d, event = %d> --> nextState = %d\n", *currentState, event, nextState);
    status = fsm_Event(pConn->ibss_pFsm, currentState, event, (void *)pConn);

    return status;
}


void connIbss_DisconnectComplete (conn_t *pConn, TI_UINT8  *data, TI_UINT8   dataLength)
{
    /* send an DISCONNECT COMPLETE event to the SM */
    conn_ibssSMEvent(&pConn->state, CONN_IBSS_DISCONN_COMPLETE, (TI_HANDLE) pConn);
}

/************************************************************************************************************/
/*      In the following section are listed the callback function used by the IBSS connection state machine */
/************************************************************************************************************/

/***********************************************************************
 *                        selfWait_to_rsnWait
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS selfWait_to_rsnWait (void *pData)
{
	conn_t      *pConn = (conn_t *)pData;
    paramInfo_t  param;

    tmr_StopTimer (pConn->hConnTimer);

    param.paramType = RX_DATA_PORT_STATUS_PARAM;
    param.content.rxDataPortStatus = OPEN_EAPOL;
    rxData_setParam (pConn->hRxData, &param);

	/* Update TxMgmtQueue SM to enable EAPOL packets. */
	txMgmtQ_SetConnState (pConn->hTxMgmtQ, TX_CONN_STATE_EAPOL);

    return rsn_start (pConn->hRsn);
}


/***********************************************************************
 *                        rsnWait_to_connected
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS rsnWait_to_connected(void *pData)
{
    paramInfo_t param;

    conn_t *pConn=(conn_t *)pData;

    TrafficMonitor_Start( pConn->hTrafficMonitor );

    healthMonitor_setState(pConn->hHealthMonitor, HEALTH_MONITOR_STATE_CONNECTED);

    siteMgr_start(pConn->hSiteMgr);

    param.paramType = RX_DATA_PORT_STATUS_PARAM;
    param.content.rxDataPortStatus = OPEN;
    rxData_setParam(((conn_t *)pData)->hRxData, &param);

	/* Update TxMgmtQueue SM to open Tx path to all packets. */
	txMgmtQ_SetConnState (((conn_t *)pData)->hTxMgmtQ, TX_CONN_STATE_OPEN);
    
    /* Update current BSS connection type and mode */
    currBSS_updateConnectedState(pConn->hCurrBss, TI_TRUE, BSS_INDEPENDENT);

	sme_ReportConnStatus(((conn_t *)pData)->hSmeSm, STATUS_SUCCESSFUL, 0);

    return TI_OK;
}

static TI_STATUS selfw_merge_rsnw(void *pData)
{
    conn_t *pConn=(conn_t *)pData;
    paramInfo_t param;

	os_printf("IBSS selfw_merge_rsnw!!!!!!!!!!\n");

	tmr_StopTimer (pConn->hConnTimer);
	siteMgr_join(pConn->hSiteMgr);

    param.paramType = RX_DATA_PORT_STATUS_PARAM;
    param.content.rxDataPortStatus = OPEN_EAPOL;
    rxData_setParam (pConn->hRxData, &param);

	/* Update TxMgmtQueue SM to enable EAPOL packets. */
	txMgmtQ_SetConnState (pConn->hTxMgmtQ, TX_CONN_STATE_EAPOL);

    return rsn_start (pConn->hRsn);

}


static TI_STATUS rsnw_merge_rsnw(void *pData)
{
    conn_t *pConn=(conn_t *)pData;

	os_printf("IBSS rsnw_merge_rsnw!!!!!!!!!!\n");

	siteMgr_join(pConn->hSiteMgr);

	return TI_OK;
}


static TI_STATUS conn_merge_conn(void *pData)
{
    conn_t *pConn=(conn_t *)pData;

	os_printf("IBSS conn_merge_conn!!!!!!!!!!\n");

	siteMgr_join(pConn->hSiteMgr);

	return TI_OK;
}

static TI_STATUS waitDisconnToCmplt_to_idle (void *pData)
{
	conn_t      *pConn = (conn_t *)pData;

	/* Inform the SME about the connection lost */
    /* we use this status at SME, if != 0 means that assoc frame sent */
	sme_ReportConnStatus(pConn->hSmeSm, STATUS_UNSPECIFIED, 1);
	return TI_OK;
}



/***********************************************************************
 *                        actionUnexpected
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS actionUnexpected(void *pData) 
{
#ifdef TI_DBG
    conn_t *pConn = (conn_t *)pData; 
    
    TRACE0(pConn->hReport, REPORT_SEVERITY_SM, "State machine error, unexpected Event\n\n");
#endif /*TI_DBG*/
    
    return TI_OK;
}

/***********************************************************************
 *                        actionNop
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS actionNop(void *pData) 
{
    return TI_OK;
}


/***********************************************************************
 *                        selfWait_to_waitToDisconnCmplt
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS selfWait_to_waitToDisconnCmplt (void *pData)
{
	conn_t      *pConn = (conn_t *)pData;
    paramInfo_t  param;  

    tmr_StopTimer (pConn->hConnTimer);
    
    siteMgr_removeSelfSite(pConn->hSiteMgr);

    /* Update current BSS connection type and mode */
    currBSS_updateConnectedState(pConn->hCurrBss, TI_FALSE, BSS_INDEPENDENT);
    
    /* stop beacon generation  */
    param.paramType = RX_DATA_PORT_STATUS_PARAM;
    param.content.rxDataPortStatus = CLOSE;
    rxData_setParam(pConn->hRxData, &param);

	/* Update TxMgmtQueue SM to close Tx path. */
	txMgmtQ_SetConnState (pConn->hTxMgmtQ, TX_CONN_STATE_CLOSE);
    
    TWD_CmdFwDisconnect (pConn->hTWD, DISCONNECT_IMMEDIATE, STATUS_UNSPECIFIED);

    return TI_OK;
}



/***********************************************************************
 *                        rsnWait_to_waitToDisconnCmplt
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS rsnWait_to_waitToDisconnCmplt(void *pData)
{
    paramInfo_t     param;
	TI_STATUS		tStatus;

    tStatus = rsn_stop(((conn_t *)pData)->hRsn, TI_FALSE);

    param.paramType = RX_DATA_PORT_STATUS_PARAM;
    param.content.rxDataPortStatus = CLOSE;
    rxData_setParam(((conn_t *)pData)->hRxData, &param);

	/* Update TxMgmtQueue SM to close Tx path. */
	txMgmtQ_SetConnState (((conn_t *)pData)->hTxMgmtQ, TX_CONN_STATE_CLOSE);

    /* Update current BSS connection type and mode */
    currBSS_updateConnectedState(((conn_t *)pData)->hCurrBss, TI_FALSE, BSS_INDEPENDENT);

    /* Stop beacon generation */
    TWD_CmdFwDisconnect (((conn_t *)pData)->hTWD, DISCONNECT_IMMEDIATE, STATUS_UNSPECIFIED); 

    return tStatus;
}


/***********************************************************************
 *                        connected_to_waitToDisconnCmplt
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS connected_to_waitToDisconnCmplt(void *pData)
{
    conn_t *pConn=(conn_t *)pData;

    TrafficMonitor_Stop(pConn->hTrafficMonitor);

    healthMonitor_setState(pConn->hHealthMonitor, HEALTH_MONITOR_STATE_DISCONNECTED);

    /* The logic of this action is identical to rsnWait_to_idle */
    return rsnWait_to_waitToDisconnCmplt(pConn);
}





/***********************************************************************
 *                        idle_to_selfWait
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS idle_to_selfWait (void *pData)
{
	conn_t    *pConn = (conn_t *)pData;
    TI_UINT16  randomTime;

    siteMgr_join (pConn->hSiteMgr);

    /* get a randomTime that is constructed of the lower 13 bits ot the system time to 
       get a MS random time of ~8000 ms */
    randomTime = os_timeStampMs (pConn->hOs) & 0x1FFF;

    /* Update current BSS connection type and mode */
    currBSS_updateConnectedState (pConn->hCurrBss, TI_TRUE, BSS_INDEPENDENT);

    tmr_StartTimer (pConn->hConnTimer,
                    conn_timeout,
                    (TI_HANDLE)pConn,
                    pConn->timeout + randomTime,
                    TI_FALSE);

	/*  Notify that the driver is associated to the supplicant\IP stack. */
    EvHandlerSendEvent (pConn->hEvHandler, IPC_EVENT_ASSOCIATED, NULL, 0);

    return TI_OK;
}



/***********************************************************************
 *                        idle_to_rsnWait
 ***********************************************************************
DESCRIPTION: 


INPUT:   

OUTPUT:

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS idle_to_rsnWait(void *pData)
{
    paramInfo_t param;

    siteMgr_join(((conn_t *)pData)->hSiteMgr);

    param.paramType = RX_DATA_PORT_STATUS_PARAM;
    param.content.rxDataPortStatus = OPEN_EAPOL;
    rxData_setParam(((conn_t *)pData)->hRxData, &param);

	/* Update TxMgmtQueue SM to enable EAPOL packets. */
	txMgmtQ_SetConnState (((conn_t *)pData)->hTxMgmtQ, TX_CONN_STATE_EAPOL);
    
    /*
     *  Notify that the driver is associated to the supplicant\IP stack. 
     */
    EvHandlerSendEvent(((conn_t *)pData)->hEvHandler, IPC_EVENT_ASSOCIATED, NULL,0);

    /* Update current BSS connection type and mode */
    currBSS_updateConnectedState(((conn_t *)pData)->hCurrBss, TI_TRUE, BSS_INDEPENDENT);
    
    return rsn_start(((conn_t *)pData)->hRsn);
}

