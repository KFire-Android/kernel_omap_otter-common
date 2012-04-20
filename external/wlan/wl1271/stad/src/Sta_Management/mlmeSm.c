/*
 * mlmeSm.c
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

/** \file mlmeSM.c
 *  \brief 802.11 MLME SM source
 *
 *  \see mlmeSM.h
 */


/***************************************************************************/
/*                                                                         */
/*      MODULE: mlmeSM.c                                                   */
/*    PURPOSE:  802.11 MLME SM source                                      */
/*                                                                         */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_69
#include "osApi.h"
#include "paramOut.h"
#include "fsm.h"
#include "report.h"
#include "smeApi.h"
#include "AssocSM.h"
#include "authSm.h"
#include "mlmeBuilder.h"
#include "mlmeSm.h"
#include "openAuthSm.h"
#include "sharedKeyAuthSm.h"
#include "connApi.h"
#include "DrvMainModules.h"


#ifdef TI_DBG
#include "siteMgrApi.h"
#endif
/* Constants */

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Global variables */

/* Local function prototypes */
static void mlme_stopAssocAndAuth(mlme_t *pMlme);

/* functions */

/**
*
* mlme_Create - allocate memory for MLME SM
*
* \b Description: 
*
* Allocate memory for MLME SM. \n
*       Allocates memory for MLME context. \n
*       Allocates memory for MLME timer. \n
*       Allocates memory for MLME SM matrix. \n
*
* \b ARGS:
*
*  I   - pOs - OS context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_mainSecSmKeysOnlyStop()
*/
TI_HANDLE mlme_create(TI_HANDLE hOs)
{
    mlme_t  *pHandle;
    TI_STATUS       status;

    /* allocate MLME context memory */
    pHandle = (mlme_t*)os_memoryAlloc(hOs, sizeof(mlme_t));
    if (pHandle == NULL)
    {
        return NULL;
    }

    /* zero all MLME context */
    os_memoryZero(hOs, pHandle, sizeof(mlme_t));

    pHandle->hOs = hOs;

    /* allocate memory for MLME state machine */
    status = fsm_Create(hOs, &pHandle->pMlmeSm, MLME_SM_NUM_STATES, MLME_SM_NUM_EVENTS);
    if (status != TI_OK)
    {
        os_memoryFree(hOs, pHandle, sizeof(mlme_t));
        return NULL;
    }

    return pHandle;
}


/**
*
* mlme_Unload - unload MLME SM from memory
*
* \b Description: 
*
* Unload MLME SM from memory
*
* \b ARGS:
*
*  I   - hMlme - MLME SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_mainSecSmKeysOnlyStop()
*/
TI_STATUS mlme_unload(TI_HANDLE hMlme)
{
    TI_STATUS       status;
    mlme_t      *pHandle;

    pHandle = (mlme_t*)hMlme;

    status = fsm_Unload(pHandle->hOs, pHandle->pMlmeSm);
    if (status != TI_OK)
    {
        /* report failure but don't stop... */
    }
    
    os_memoryFree(pHandle->hOs, hMlme, sizeof(mlme_t));

    return TI_OK;
}

/**
*
* mlme_smConfig - configure a new MLME SM
*
* \b Description: 
*
* Configure a new MLME SM and other modules handles.
*
* \b RETURNS:
*
*  void
*
* \sa mlme_Create, mlme_Unload
*/
void mlme_init (TStadHandlesList *pStadHandles)
{
    mlme_t *pHandle = (mlme_t *)(pStadHandles->hMlmeSm);
    
    /** Main 802.1X State Machine matrix */
    fsm_actionCell_t    mlme_smMatrix[MLME_SM_NUM_STATES][MLME_SM_NUM_EVENTS] =
    {
        /* next state and actions for IDLE state */
        {{MLME_SM_STATE_AUTH_WAIT, (fsm_Action_t)mlme_smStartIdle},             /* MLME_SM_EVENT_START */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smNOP},                     /* MLME_SM_EVENT_STOP  */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smActionUnexpected},       /* MLME_SM_EVENT_AUTH_SUCCESS */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smActionUnexpected},       /* MLME_SM_EVENT_AUTH_FAIL */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smActionUnexpected},       /* MLME_SM_EVENT_ASSOC_SUCCESS */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smActionUnexpected}        /* MLME_SM_EVENT_ASSOC_FAIL */
        },
        /* next state and actions for AUTH_WAIT state */
        {{MLME_SM_STATE_AUTH_WAIT, (fsm_Action_t)mlme_smActionUnexpected},      /* MLME_SM_EVENT_START */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smNOP},                       /* MLME_SM_EVENT_STOP  */
         {MLME_SM_STATE_ASSOC_WAIT, (fsm_Action_t)mlme_smAuthSuccessAuthWait},  /* MLME_SM_EVENT_AUTH_SUCCESS */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smAuthFailAuthWait},           /* MLME_SM_EVENT_AUTH_FAIL */
         {MLME_SM_STATE_AUTH_WAIT, (fsm_Action_t)mlme_smActionUnexpected},      /* MLME_SM_EVENT_ASSOC_SUCCESS */
         {MLME_SM_STATE_AUTH_WAIT, (fsm_Action_t)mlme_smActionUnexpected}       /* MLME_SM_EVENT_ASSOC_FAIL */
        },
        /* next state and actions for ASSOC_WAIT state */
        {{MLME_SM_STATE_ASSOC_WAIT, (fsm_Action_t)mlme_smActionUnexpected}, /* MLME_SM_EVENT_START */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smStopAssocWait},          /* MLME_SM_EVENT_STOP  */
         {MLME_SM_STATE_ASSOC_WAIT, (fsm_Action_t)mlme_smActionUnexpected}, /* MLME_SM_EVENT_AUTH_SUCCESS */
         {MLME_SM_STATE_ASSOC_WAIT, (fsm_Action_t)mlme_smActionUnexpected}, /* MLME_SM_EVENT_AUTH_FAIL */
         {MLME_SM_STATE_ASSOC, (fsm_Action_t)mlme_smAssocSuccessAssocWait}, /* MLME_SM_EVENT_ASSOC_SUCCESS */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smAssocFailAssocWait}      /* MLME_SM_EVENT_ASSOC_FAIL */
        },
        /* next state and actions for ASSOC state */
        {{MLME_SM_STATE_ASSOC, (fsm_Action_t)mlme_smActionUnexpected},  /* MLME_SM_EVENT_START */
         {MLME_SM_STATE_IDLE, (fsm_Action_t)mlme_smStopAssoc},          /* MLME_SM_EVENT_STOP  */
         {MLME_SM_STATE_ASSOC, (fsm_Action_t)mlme_smActionUnexpected},  /* MLME_SM_EVENT_AUTH_SUCCESS */
         {MLME_SM_STATE_ASSOC, (fsm_Action_t)mlme_smActionUnexpected},  /* MLME_SM_EVENT_AUTH_FAIL */
         {MLME_SM_STATE_ASSOC, (fsm_Action_t)mlme_smActionUnexpected},  /* MLME_SM_EVENT_ASSOC_SUCCESS */
         {MLME_SM_STATE_ASSOC, (fsm_Action_t)mlme_smActionUnexpected}   /* MLME_SM_EVENT_ASSOC_FAIL */
        }
    };
    
    fsm_Config(pHandle->pMlmeSm, &mlme_smMatrix[0][0], MLME_SM_NUM_STATES, MLME_SM_NUM_EVENTS, mlme_smEvent, pStadHandles->hOs);
    
    pHandle->currentState = MLME_SM_STATE_IDLE;
    pHandle->legacyAuthType = AUTH_LEGACY_NONE;
    pHandle->reAssoc = TI_FALSE;
    pHandle->disConnType = DISCONNECT_IMMEDIATE;
    pHandle->disConnReason = STATUS_UNSPECIFIED;
    
    pHandle->hAssoc            = pStadHandles->hAssoc;
    pHandle->hAuth             = pStadHandles->hAuth;
    pHandle->hSiteMgr          = pStadHandles->hSiteMgr;
    pHandle->hCtrlData         = pStadHandles->hCtrlData;
    pHandle->hTxMgmtQ          = pStadHandles->hTxMgmtQ;
    pHandle->hMeasurementMgr   = pStadHandles->hMeasurementMgr;
    pHandle->hSwitchChannel    = pStadHandles->hSwitchChannel;
    pHandle->hRegulatoryDomain = pStadHandles->hRegulatoryDomain;
    pHandle->hReport           = pStadHandles->hReport;
    pHandle->hOs               = pStadHandles->hOs;
    pHandle->hConn             = pStadHandles->hConn;
    pHandle->hCurrBss          = pStadHandles->hCurrBss;
    pHandle->hApConn           = pStadHandles->hAPConnection;
    pHandle->hScanCncn         = pStadHandles->hScanCncn;
    pHandle->hQosMngr          = pStadHandles->hQosMngr;
    pHandle->hTWD              = pStadHandles->hTWD;
    pHandle->hTxCtrl           = pStadHandles->hTxCtrl;

    /*
    debug info
    */
    pHandle->debug_lastProbeRspTSFTime = 0;
    pHandle->debug_lastDtimBcnTSFTime = 0;
    pHandle->debug_lastBeaconTSFTime = 0;
    pHandle->debug_isFunctionFirstTime = TI_TRUE;
    pHandle->BeaconsCounterPS = 0;
}

void mlme_SetDefaults (TI_HANDLE hMlmeSm, TMlmeInitParams *pMlmeInitParams)
{
    mlme_t *pMlme = (mlme_t *)(hMlmeSm);

    /* set default values */
    pMlme->bParseBeaconWSC = pMlmeInitParams->parseWSCInBeacons;
}

TI_STATUS mlme_setParam(TI_HANDLE           hMlmeSm,
                        paramInfo_t         *pParam)
{
    mlme_t *pMlmeSm = (mlme_t *)hMlmeSm;

    switch(pParam->paramType)
    {
    case MLME_LEGACY_TYPE_PARAM:

        switch (pParam->content.mlmeLegacyAuthType)
        {
        case AUTH_LEGACY_RESERVED1:
        case AUTH_LEGACY_OPEN_SYSTEM:
            /* First configure the MLME with the new legacy authentication type */
            pMlmeSm->legacyAuthType = pParam->content.mlmeLegacyAuthType;
            /* Now configure the authentication module. */
            pParam->paramType = AUTH_LEGACY_TYPE_PARAM;
            return auth_setParam(pMlmeSm->hAuth, pParam);

        case AUTH_LEGACY_SHARED_KEY:
            /* First configure the MLME with the new legacy authentication type */
            pMlmeSm->legacyAuthType = AUTH_LEGACY_SHARED_KEY;
            /* Now configure the authentication module. */
            pParam->paramType = AUTH_LEGACY_TYPE_PARAM;
            return auth_setParam(pMlmeSm->hAuth, pParam);

        case AUTH_LEGACY_AUTO_SWITCH:
            /* First configure the MLME with the new legacy authentication type */
            pMlmeSm->legacyAuthType = AUTH_LEGACY_AUTO_SWITCH;
            /* Now configure the authentication module, 
                Auto switch mode means start always with shared key, if fail move to open system. */
            pParam->paramType = AUTH_LEGACY_TYPE_PARAM;
            pParam->content.authLegacyAuthType = AUTH_LEGACY_SHARED_KEY;
            return auth_setParam(pMlmeSm->hAuth, pParam);

        default:
            TRACE1(pMlmeSm->hReport, REPORT_SEVERITY_ERROR, "Set param, Params is not supported, 0x%x\n\n", pParam->content.mlmeLegacyAuthType);
            return PARAM_VALUE_NOT_VALID;
        }
/*      break;  - unreachable */

    case MLME_RE_ASSOC_PARAM:
        pMlmeSm->reAssoc = pParam->content.mlmeReAssoc;
        break;

    default:
        TRACE1(pMlmeSm->hReport, REPORT_SEVERITY_ERROR, "Set param, Params is not supported, 0x%x\n\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}

TI_STATUS mlme_getParam(TI_HANDLE           hMlmeSm, 
                        paramInfo_t         *pParam)
{
    mlme_t *pMlmeSm = (mlme_t *)hMlmeSm;

    switch(pParam->paramType)
    {
    case MLME_LEGACY_TYPE_PARAM:
        pParam->content.mlmeLegacyAuthType = pMlmeSm->legacyAuthType;           
        break;

    case MLME_CAPABILITY_PARAM:
        pParam->content.mlmeLegacyAuthType = pMlmeSm->legacyAuthType;
        assoc_smCapBuild(pMlmeSm->hAssoc, &(pParam->content.siteMgrSiteCapability));
        break;

    case MLME_BEACON_RECV:
        pParam->content.siteMgrTiWlanCounters.BeaconsRecv = pMlmeSm->BeaconsCounterPS;
        break;

    default:
        TRACE1(pMlmeSm->hReport, REPORT_SEVERITY_ERROR, "Get param, Params is not supported, %d\n\n", pParam->content.mlmeLegacyAuthType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}

/**
*
* mlme_Start - Start event for the MLME SM
*
* \b Description: 
*
* Start event for the MLME SM
*
* \b ARGS:
*
*  I   - hMlme - MLME SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa mlme_Stop, mlme_Recv
*/
TI_STATUS mlme_start(TI_HANDLE hMlme)
{
    TI_STATUS	status;
    mlme_t		*pHandle = (mlme_t*)hMlme;

    if (pHandle == NULL)
    {
        return TI_NOK;
    }

    if (pHandle->legacyAuthType == AUTH_LEGACY_NONE)
    {
        TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlme_start: pHandle->legacyAuthType == AUTH_LEGACY_NONE\n");
        return TI_NOK;
    }

    status = mlme_smEvent(&pHandle->currentState, MLME_SM_EVENT_START, pHandle);

    return status;
}

/**
*
* mlme_Stop - Stop event for the MLME SM
*
* \b Description: 
*
* Stop event for the MLME SM
*
* \b ARGS:
*
*  I   - hMlme - MLME SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa mlme_Start, mlme_Recv
*/
TI_STATUS mlme_stop(TI_HANDLE hMlme, DisconnectType_e disConnType, mgmtStatus_e reason)
{
    TI_STATUS       status;
    mlme_t      *pHandle;

    pHandle = (mlme_t*)hMlme;

    if (pHandle->legacyAuthType == AUTH_LEGACY_NONE)
        return TI_NOK;

    pHandle->disConnType = disConnType;
    pHandle->disConnReason = reason;

    status = mlme_smEvent(&pHandle->currentState, MLME_SM_EVENT_STOP, pHandle);
    
    return status;
}


/**
*
* mlme_reportAuthStatus - Set a specific parameter to the MLME SM
*
* \b Description: 
*
* Set a specific parameter to the MLME SM.
*
* \b ARGS:
*
*  I   - hMlme - MLME SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa mlme_Start, mlme_Stop
*/
TI_STATUS mlme_reportAuthStatus(TI_HANDLE hMlme, TI_UINT16 status)
{
    mlme_t          *pHandle;
    paramInfo_t     param;
    TI_STATUS       fStatus;

    pHandle = (mlme_t*)hMlme;

    if (pHandle == NULL)
        return TI_NOK;
 
    if (pHandle->legacyAuthType == AUTH_LEGACY_NONE)
        return TI_NOK;

    pHandle->mlmeData.uStatusCode = status;

    /* If status is successful */
    if (status == 0)
    {
        /* Mark a successful status - used for conn.c */
        pHandle->mlmeData.mgmtStatus = STATUS_SUCCESSFUL;
        fStatus = mlme_smEvent(&pHandle->currentState, MLME_SM_EVENT_AUTH_SUCCESS, pHandle);
    } 
    else 
    {
        /* Now, if the MLME legacy auth type is AUTO_SWITCH, and the Auth legacy auth type is shared key, 
            we configure the auth SM to open system, otherwise, this is really an authentication failure. */
        param.paramType = AUTH_LEGACY_TYPE_PARAM;
        auth_getParam(pHandle->hAuth, &param);

        if ((pHandle->legacyAuthType == AUTH_LEGACY_AUTO_SWITCH) && (param.content.authLegacyAuthType ==  AUTH_LEGACY_SHARED_KEY))
        {
            param.content.authLegacyAuthType = AUTH_LEGACY_OPEN_SYSTEM;
            fStatus = auth_setParam(pHandle->hAuth, &param);
            fStatus = auth_start(pHandle->hAuth);
        }

        else
        {
            pHandle->mlmeData.mgmtStatus = STATUS_AUTH_REJECT;
            fStatus = mlme_smEvent(&pHandle->currentState, MLME_SM_EVENT_AUTH_FAIL, pHandle);
        }

    }

    return fStatus;
}

/**
*
* mlme_reportAssocStatus - Set a specific parameter to the MLME SM
*
* \b Description: 
*
* Set a specific parameter to the MLME SM.
*
* \b ARGS:
*
*  I   - hMlme - MLME SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa mlme_Start, mlme_Stop
*/
TI_STATUS mlme_reportAssocStatus(TI_HANDLE hMlme, TI_UINT16 status)
{
    mlme_t      *pHandle;
    TI_STATUS   fStatus;

    pHandle = (mlme_t*)hMlme;

    if (pHandle == NULL)
        return TI_NOK;

    if (pHandle->legacyAuthType == AUTH_LEGACY_NONE)
        return TI_NOK;

    pHandle->mlmeData.uStatusCode = status;

    /* If status is successful */
    if (status == 0)
    {
        pHandle->mlmeData.mgmtStatus = STATUS_SUCCESSFUL;
        fStatus = mlme_smEvent(&pHandle->currentState, MLME_SM_EVENT_ASSOC_SUCCESS, pHandle);
    } else 
    {
        pHandle->mlmeData.mgmtStatus = STATUS_ASSOC_REJECT;
        fStatus = mlme_smEvent(&pHandle->currentState, MLME_SM_EVENT_ASSOC_FAIL, pHandle);
    }

    return fStatus;
}


/**
*
* mlme_SetParam - Set a specific parameter to the MLME SM
*
* \b Description: 
*
* Set a specific parameter to the MLME SM.
*
* \b ARGS:
*
*  I   - hMlme - MLME SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa mlme_Start, mlme_Stop
*/
TI_STATUS mlme_smEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hMlme)
{
   mlme_t *pMlme = (mlme_t *)hMlme;
    TI_STATUS       status;
    TI_UINT8        nextState;

    status = fsm_GetNextState(pMlme->pMlmeSm, *currentState, event, &nextState);
    if (status != TI_OK)
    {
        TRACE0(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_SM: ERROR - failed getting next state \n");

        return(TI_NOK);
    }

	TRACE3( pMlme->hReport, REPORT_SEVERITY_INFORMATION, "mlme_smEvent: <currentState = %d, event = %d> --> nextState = %d\n", *currentState, event, nextState);

    status = fsm_Event(pMlme->pMlmeSm, currentState, event, (void *)pMlme);

    return(status);
}

/* state machine functions */

TI_STATUS mlme_smStartIdle(mlme_t *pMlme)
{
    TI_STATUS       status;

    status = auth_start(pMlme->hAuth);

    return status;
}

TI_STATUS mlme_smClass3Idle(mlme_t *pMlme)
{
    return TI_OK;
}



TI_STATUS mlme_smAuthSuccessAuthWait(mlme_t *pMlme)
{
    TI_STATUS       status;

    if (pMlme->reAssoc)
    {
        status = reassoc_start(pMlme->hAssoc);
    }
    else
    {
        status = assoc_start(pMlme->hAssoc);
    }

    return status;
}

TI_STATUS mlme_smAuthFailAuthWait(mlme_t *pMlme)
{
    TI_STATUS       status;

    status = mlme_smReportStatus(pMlme);

    return status;
}

TI_STATUS mlme_smStopAssocWait(mlme_t *pMlme)
{
    mlme_stopAssocAndAuth(pMlme);
    return TI_OK;
}

TI_STATUS mlme_smAssocSuccessAssocWait(mlme_t *pMlme)
{
    TI_STATUS       status;

    status = mlme_smReportStatus(pMlme);

    return status;
}

TI_STATUS mlme_smAssocFailAssocWait(mlme_t *pMlme)
{
    TI_STATUS       status;
 
    status = mlme_smReportStatus(pMlme);

    return status;
}


TI_STATUS mlme_smStopAssoc(mlme_t *pMlme)
{
    mlme_stopAssocAndAuth(pMlme);
    return TI_OK;
}



TI_STATUS mlme_smNOP(mlme_t *pMlme)
{
    return TI_OK;
}

TI_STATUS mlme_smActionUnexpected(mlme_t *pMlme)
{
    return TI_OK;
}

/* local functions */

TI_STATUS mlme_smReportStatus(mlme_t *pMlme)
{
    TI_STATUS       status;

    if (pMlme == NULL)
    {
        return TI_NOK;
    }

    status = conn_reportMlmeStatus(pMlme->hConn, pMlme->mlmeData.mgmtStatus, pMlme->mlmeData.uStatusCode);
    return status;
}


static void mlme_stopAssocAndAuth(mlme_t *pMlme)
{

    TI_BOOL         sendDeAuth;
    TI_BOOL         sendDisAssoc;

    /* Don't send deauth/disassoc, FW will do it on disconnect command */
    sendDeAuth   = TI_FALSE;
    sendDisAssoc = TI_FALSE;

	TRACE0(pMlme->hReport, REPORT_SEVERITY_INFORMATION, "mlme_stopAssocAndAuth: Auth/assoc stop without sending deauth/disassoc\n");

    assoc_setDisAssocFlag(pMlme->hAssoc, sendDisAssoc);
    assoc_stop(pMlme->hAssoc);
    
    auth_stop(pMlme->hAuth, sendDeAuth, pMlme->disConnReason );
}
/*****************************************************************************
**
** MLME messages builder/Parser
**
*****************************************************************************/







