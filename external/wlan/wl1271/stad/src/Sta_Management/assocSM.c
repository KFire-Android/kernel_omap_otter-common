/*
 * assocSM.c
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

/** \file assocSM.c
 *  \brief 802.11 association SM source
 *
 *  \see assocSM.h
 */


/***************************************************************************/
/*                                                                         */
/*      MODULE: assocSM.c                                                  */
/*    PURPOSE:  802.11 association SM source                               */
/*                                                                         */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_63
#include "osApi.h"
#include "paramOut.h"
#include "rate.h"
#include "timer.h"
#include "fsm.h"
#include "report.h"
#include "DataCtrl_Api.h"
#include "siteMgrApi.h"
#include "rsnApi.h"
#include "regulatoryDomainApi.h"
#include "mlmeBuilder.h"
#include "mlmeApi.h"
#include "AssocSM.h"
#include "qosMngr_API.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCRMMngr.h"
#include "XCCMngr.h"
#endif
#include "apConn.h"
#include "TWDriver.h"
#include "DrvMainModules.h"
#include "StaCap.h"
#include "smeApi.h"
#include "rsn.h"

/* Constants */

/** number of states in the state machine */
#define ASSOC_SM_NUM_STATES     3

/** number of events in the state machine */
#define ASSOC_SM_NUM_EVENTS     6

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Global variables */

/* Local function prototypes */

/* functions */


/* state machine functions */


TI_STATUS assoc_smEvent(assoc_t *pAssoc, TI_UINT8 event, void *pData);

void assoc_smTimeout(TI_HANDLE hAssoc, TI_BOOL bTwdInitOccured);

TI_STATUS assoc_smStartIdle(assoc_t *pAssoc);
TI_STATUS assoc_smStopWait(assoc_t *pAssoc);
TI_STATUS assoc_smSuccessWait(assoc_t *pAssoc);
TI_STATUS assoc_smFailureWait(assoc_t *pAssoc);
TI_STATUS assoc_smTimeoutWait(assoc_t *pAssoc);
TI_STATUS assoc_smMaxRetryWait(assoc_t *pAssoc);
TI_STATUS assoc_smStopAssoc(assoc_t *pAssoc);
TI_STATUS assoc_smActionUnexpected(assoc_t *pAssoc);

TI_STATUS assoc_smResetRetry(assoc_t *pAssoc);
TI_STATUS assoc_smIncRetry(assoc_t *pAssoc);
TI_STATUS assoc_smReportSuccess(assoc_t *pAssoc);
TI_STATUS assoc_smReportFailure(assoc_t *pAssoc, TI_UINT16 uStatusCode);
TI_STATUS assoc_smSendAssocReq(assoc_t *pAssoc);
TI_STATUS assoc_smStartTimer(assoc_t *pAssoc);
TI_STATUS assoc_smStopTimer(assoc_t *pAssoc);

TI_STATUS assoc_smCapBuild(assoc_t *pCtx, TI_UINT16 *cap);
TI_STATUS assoc_smSSIDBuild(assoc_t *pCtx, TI_UINT8 *pSSID, TI_UINT32 *ssidLen);
TI_STATUS assoc_smRatesBuild(assoc_t *pCtx, TI_UINT8 *pRates, TI_UINT32 *ratesLen);
TI_STATUS assoc_smRequestBuild(assoc_t *pCtx, TI_UINT8* reqBuf, TI_UINT32* reqLen);

TI_STATUS assoc_saveAssocReqMessage(assoc_t *pAssocSm, TI_UINT8 *pAssocBuffer, TI_UINT32 length);
TI_STATUS assoc_sendDisAssoc(assoc_t *pAssocSm, mgmtStatus_e reason);

/**
*
* assoc_create - allocate memory for association SM
*
* \b Description: 
*
* Allocate memory for association SM. \n
*       Allocates memory for Association context. \n
*       Allocates memory for association SM matrix. \n
*
* \b ARGS:
*
*  I   - hOs - OS context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_mainSecSmKeysOnlyStop()
*/
TI_HANDLE assoc_create(TI_HANDLE hOs)
{
    assoc_t     *pHandle;
    TI_STATUS       status;

    /* allocate association context memory */
    pHandle = (assoc_t*)os_memoryAlloc(hOs, sizeof(assoc_t));
    if (pHandle == NULL)
    {
        return NULL;
    }

    os_memoryZero(hOs, pHandle, sizeof(assoc_t));

    pHandle->hOs = hOs;

    /* allocate memory for association state machine */
    status = fsm_Create(hOs, &pHandle->pAssocSm, ASSOC_SM_NUM_STATES, ASSOC_SM_NUM_EVENTS);
    if (status != TI_OK)
    {
        os_memoryFree(hOs, pHandle, sizeof(assoc_t));
        return NULL;
    }

    return pHandle;
}


/**
*
* assocunload - unload association SM from memory
*
* \b Description: 
*
* Unload association SM from memory
*
* \b ARGS:
*
*  I   - hAssoc - association SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_mainSecSmKeysOnlyStop()
*/
TI_STATUS assoc_unload(TI_HANDLE hAssoc)
{
    TI_STATUS       status;
    assoc_t     *pHandle;

    pHandle = (assoc_t*)hAssoc;

    status = fsm_Unload(pHandle->hOs, pHandle->pAssocSm);
    if (status != TI_OK)
    {
        /* report failure but don't stop... */
        TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "ASSOC_SM: Error releasing FSM memory \n");
    }
    
	if (pHandle->hAssocSmTimer)
	{
		tmr_DestroyTimer (pHandle->hAssocSmTimer);
	}
    
    os_memoryFree(pHandle->hOs, hAssoc, sizeof(assoc_t));

    return TI_OK;
}

/**
*
* assoc_config - configure a new association SM
*
* \b Description: 
*
* Configure a new association SM.
*
* \b RETURNS:
*
*  void
*
* \sa assoc_Create, assoc_Unload
*/
void assoc_init (TStadHandlesList *pStadHandles)
{
    assoc_t *pHandle = (assoc_t*)(pStadHandles->hAssoc);
    
    /** Main 802.1X State Machine matrix */
    fsm_actionCell_t    assoc_smMatrix[ASSOC_SM_NUM_STATES][ASSOC_SM_NUM_EVENTS] =
    {
        /* next state and actions for IDLE state */
        {{ASSOC_SM_STATE_WAIT, (fsm_Action_t)assoc_smStartIdle},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smActionUnexpected}
        },
        /* next state and actions for WAIT state */
        {{ASSOC_SM_STATE_WAIT, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smStopWait},
         {ASSOC_SM_STATE_ASSOC, (fsm_Action_t)assoc_smSuccessWait},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smFailureWait},
         {ASSOC_SM_STATE_WAIT, (fsm_Action_t)assoc_smTimeoutWait},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smMaxRetryWait}
        },
        /* next state and actions for ASSOC state */
        {{ASSOC_SM_STATE_ASSOC, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_IDLE, (fsm_Action_t)assoc_smStopAssoc},
         {ASSOC_SM_STATE_ASSOC, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_ASSOC, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_ASSOC, (fsm_Action_t)assoc_smActionUnexpected},
         {ASSOC_SM_STATE_ASSOC, (fsm_Action_t)assoc_smActionUnexpected}
        }};
    
    /* configure state machine */
    fsm_Config (pHandle->pAssocSm, &assoc_smMatrix[0][0], ASSOC_SM_NUM_STATES, ASSOC_SM_NUM_EVENTS, NULL, pStadHandles->hOs);

    pHandle->assocRejectCount = 0;
    pHandle->assocTimeoutCount = 0;
    pHandle->currentState = ASSOC_SM_STATE_IDLE;
    
    pHandle->hMlme             = pStadHandles->hMlmeSm;
    pHandle->hRegulatoryDomain = pStadHandles->hRegulatoryDomain;
    pHandle->hSiteMgr          = pStadHandles->hSiteMgr;
    pHandle->hCtrlData         = pStadHandles->hCtrlData;
    pHandle->hTWD              = pStadHandles->hTWD;
    pHandle->hRsn              = pStadHandles->hRsn;
    pHandle->hReport           = pStadHandles->hReport;
    pHandle->hOs               = pStadHandles->hOs;
    pHandle->hXCCMngr          = pStadHandles->hXCCMngr;
    pHandle->hQosMngr          = pStadHandles->hQosMngr;
    pHandle->hMeasurementMgr   = pStadHandles->hMeasurementMgr;
    pHandle->hApConn           = pStadHandles->hAPConnection;
    pHandle->hTimer            = pStadHandles->hTimer;
    pHandle->hStaCap = pStadHandles->hStaCap;
    pHandle->hSme              = pStadHandles->hSme;

}


TI_STATUS assoc_SetDefaults (TI_HANDLE hAssoc, assocInitParams_t *pAssocInitParams)
{
    assoc_t *pHandle = (assoc_t*)hAssoc;

    pHandle->timeout = pAssocInitParams->assocResponseTimeout;
    pHandle->maxCount = pAssocInitParams->assocMaxRetryCount;

    /* allocate OS timer memory */
    pHandle->hAssocSmTimer = tmr_CreateTimer (pHandle->hTimer);
    if (pHandle->hAssocSmTimer == NULL)
    {
        TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "assoc_SetDefaults(): Failed to create hAssocSmTimer!\n");
        return TI_NOK;
    }

    return TI_OK;
}


/**
*
* assoc_start - Start event for the association SM
*
* \b Description: 
*
* Start event for the association SM
*
* \b ARGS:
*
*  I   - hAssoc - Association SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa assoc_Stop, assoc_Recv
*/
TI_STATUS assoc_start(TI_HANDLE hAssoc)
{
    TI_STATUS       status;
    assoc_t     *pHandle;

    pHandle = (assoc_t*)hAssoc;

    if (pHandle == NULL)
    {
        return TI_NOK;
    }

    pHandle->reAssoc = TI_FALSE;

    pHandle->disAssoc = TI_FALSE;

    status = assoc_smEvent(pHandle, ASSOC_SM_EVENT_START, hAssoc);

    return status;
}


/**
*
* assoc_start - Start event for the association SM
*
* \b Description: 
*
* Start event for the association SM - for Re-assoc request 
*
* \b ARGS:
*
*  I   - hAssoc - Association SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa assoc_Stop, assoc_Recv
*/
TI_STATUS reassoc_start(TI_HANDLE hAssoc)
{
    TI_STATUS       status;
    assoc_t     *pHandle;

    pHandle = (assoc_t*)hAssoc;

    if (pHandle == NULL)
    {
        return TI_NOK;
    }
    pHandle->reAssoc = TI_TRUE;

    status = assoc_smEvent(pHandle, ASSOC_SM_EVENT_START, hAssoc);

    return status;
}

/**
*
* assoc_stop - Stop event for the association SM
*
* \b Description: 
*
* Stop event for the association SM
*
* \b ARGS:
*
*  I   - hAssoc - Association SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa assoc_Start, assoc_Recv
*/
TI_STATUS assoc_stop(TI_HANDLE hAssoc)
{
    TI_STATUS       status;
    assoc_t     *pHandle;

    pHandle = (assoc_t*)hAssoc;

    if (pHandle == NULL)
    {
        return TI_NOK;
    }
    
    status = assoc_smEvent(pHandle, ASSOC_SM_EVENT_STOP, hAssoc);
    
    return status;
}


TI_STATUS assoc_setDisAssocFlag(TI_HANDLE hAssoc, TI_BOOL disAsoccFlag)
{
    assoc_t     *pHandle;
    pHandle = (assoc_t*)hAssoc; 

    pHandle->disAssoc = disAsoccFlag;

    return TI_OK;
}



/**
*
* assoc_recv - Recive a message from the AP
*
* \b Description: 
*
* Parse a message form the AP and perform the appropriate event.
*
* \b ARGS:
*
*  I   - hAssoc - Association SM context  \n
*  I   - pFrame - Frame recieved  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa assoc_Start, assoc_Stop
*/
TI_STATUS assoc_recv(TI_HANDLE hAssoc, mlmeFrameInfo_t *pFrame)
{
    TI_STATUS       status;
    assoc_t         *pHandle = (assoc_t*)hAssoc;
    TTwdParamInfo   tTwdParam;
    TI_UINT16           rspStatus;

    if (pHandle == NULL)
    {
        return TI_NOK;
    }

    /* ensure that the SM is waiting for assoc response */
    if(pHandle->currentState != ASSOC_SM_STATE_WAIT)
        return TI_OK;

    
    if ((pFrame->subType != ASSOC_RESPONSE) && (pFrame->subType != RE_ASSOC_RESPONSE))
    {
        return TI_NOK;
    }

    /* check response status */
    rspStatus  = pFrame->content.assocRsp.status;
    
    if (rspStatus == 0)
    {
        TRsnData        rsnData;
        dot11_RSN_t *pRsnIe;
        TI_UINT8       curRsnData[255];
        TI_UINT8       rsnAssocIeLen;
        TI_UINT8        length = 0;


        TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG Success associating to AP \n");
        
        /* set AID to HAL */
        tTwdParam.paramType = TWD_AID_PARAM_ID;
        tTwdParam.content.halCtrlAid  = pFrame->content.assocRsp.aid;
        TWD_SetParam (pHandle->hTWD, &tTwdParam);
        

        /* Get the RSN IE data */
        pRsnIe = pFrame->content.assocRsp.pRsnIe;
        while (length < pFrame->content.assocRsp.rsnIeLen && (pFrame->content.assocRsp.rsnIeLen < 255))
        {
            curRsnData[0+length] = pRsnIe->hdr[0];
            curRsnData[1+length] = pRsnIe->hdr[1];
            os_memoryCopy(pHandle->hOs, &curRsnData[2+length], (void *)pRsnIe->rsnIeData, pRsnIe->hdr[1]); 
            length += pRsnIe->hdr[1] + 2;
            pRsnIe += 1;
        }
        
        if (pFrame->content.assocRsp.rsnIeLen != 0)
        {
            rsnData.pIe = curRsnData;
            rsnData.ieLen = pFrame->content.assocRsp.rsnIeLen;
            rsnData.privacy =  ((pFrame->content.assocRsp.capabilities >> CAP_PRIVACY_SHIFT) & CAP_PRIVACY_MASK) ? TI_TRUE : TI_FALSE;
            rsn_setSite(pHandle->hRsn, &rsnData, NULL, &rsnAssocIeLen);
        }

        /* update siteMgr with capabilities and whether we are connected to Cisco AP */
        siteMgr_assocReport(pHandle->hSiteMgr,
                            pFrame->content.assocRsp.capabilities, pFrame->content.assocRsp.ciscoIEPresent);

        /* update QoS Manager - it the QOS active protocol is NONE, or no WME IE present, it will return TI_OK */
        /* if configured by AP, update MSDU lifetime */
        status = qosMngr_setSite(pHandle->hQosMngr, &pFrame->content.assocRsp);

        if(status != TI_OK)
        {
            TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "ASSOC_SM: DEBUG - Association failed : qosMngr_setSite error \n");
            /* in case we wanted to work with qosAP and failed to connect to qos AP we want to reassociated again 
               to another one */  
            status = assoc_smEvent(pHandle, ASSOC_SM_EVENT_FAIL, hAssoc);
        }
        else
        {
            status = assoc_smEvent(pHandle, ASSOC_SM_EVENT_SUCCESS, hAssoc);
        }
    } 
    else 
    {
        pHandle->assocRejectCount++;
        
        /* If there was attempt to renegotiate voice settings, update QoS Manager */
        qosMngr_checkTspecRenegResults(pHandle->hQosMngr, &pFrame->content.assocRsp);

        /* check failure reason */
        switch (rspStatus)
        {
        case 0:
            break;
        case 1:
            /* print debug message */
            TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association denied: Unspecified error \n");
            break;
        case 10:
            /* print debug message */
            TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association denied: Cannot support all requested capabilities in the Capability Information field \n");
            break;
        case 11:
            /* print debug message */
            TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association denied: Reassociation denied due to inability to confirm that association exists \n");
            break;
        case 12:
            /* print debug message */
            TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association denied: Association denied due to reason outside the scope of this standard \n");
            rsn_reportAuthFailure(pHandle->hRsn, RSN_AUTH_STATUS_INVALID_TYPE);
            break;
        case 13:
            TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association denied: Association denied due to wrong authentication algorithm \n");
            rsn_reportAuthFailure(pHandle->hRsn, RSN_AUTH_STATUS_INVALID_TYPE);
            break;
        case 17:
            /* print debug message */
            TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association denied: Association denied because AP is unable to handle additional associated stations \n");
            break;
        case 18:
            /* print debug message */
            TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association denied: Association denied due to requesting station not supporting all of the data rates in the BSSBasicRateSet parameter \n");
            break;
        default:
            /* print error message on wrong error code for association response */
            TRACE1(pHandle->hReport, REPORT_SEVERITY_ERROR, "ASSOC_SM: ERROR - Association denied: error code (%d) irrelevant \n", rspStatus);
            break;
        }

        status = assoc_smEvent(pHandle, ASSOC_SM_EVENT_FAIL, hAssoc);
    }

    return status;
}

/**
*
* assoc_getParam - Get a specific parameter from the association SM
*
* \b Description: 
*
* Get a specific parameter from the association SM.
*
* \b ARGS:
*
*  I   - hAssoc - Association SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa assoc_Start, assoc_Stop
*/
TI_STATUS assoc_getParam(TI_HANDLE hAssoc, paramInfo_t *pParam)
{
    assoc_t *pHandle = (assoc_t *)hAssoc;

    if ((pHandle == NULL) || (pParam == NULL))
    {
        return TI_NOK;
    }

    /* serch parameter type */
    switch (pParam->paramType)
    {
    case ASSOC_RESPONSE_TIMEOUT_PARAM:
        pParam->content.assocResponseTimeout = pHandle->timeout;
        break;

    case ASSOC_COUNTERS_PARAM:
        pParam->content.siteMgrTiWlanCounters.AssocRejects = pHandle->assocRejectCount;
        pParam->content.siteMgrTiWlanCounters.AssocTimeouts = pHandle->assocTimeoutCount;
        break;

    case ASSOC_ASSOCIATION_REQ_PARAM:
        pParam->content.assocReqBuffer.buffer = pHandle->assocReqBuffer;
        pParam->content.assocReqBuffer.bufferSize = pHandle->assocReqLen;
		pParam->content.assocReqBuffer.reAssoc = pHandle->reAssoc;
        break;

    case ASSOC_ASSOCIATION_RESP_PARAM:
        pParam->content.assocReqBuffer.buffer = pHandle->assocRespBuffer;
        pParam->content.assocReqBuffer.bufferSize = pHandle->assocRespLen;
		pParam->content.assocReqBuffer.reAssoc = pHandle->reAssocResp;
        break;

    case ASSOC_ASSOCIATION_INFORMATION_PARAM:
       {
           TI_UINT8  reqBuffIEOffset, respBuffIEOffset;
           TI_UINT32 RequestIELength = 0;
           TI_UINT32 ResponseIELength = 0;
           paramInfo_t  *lParam;
           ScanBssType_enum bssType;

           TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "ASSOC_SM: DEBUG - Association Information Get:  \n");
           lParam = (paramInfo_t *)os_memoryAlloc(pHandle->hOs, sizeof(paramInfo_t));
           if (!lParam)
           {
               return TI_NOK;
           }

           /* Assoc exists only in Infrastructure */
           lParam->paramType = CTRL_DATA_CURRENT_BSS_TYPE_PARAM;
           ctrlData_getParam(pHandle->hCtrlData, lParam);
           bssType = lParam->content.ctrlDataCurrentBssType;
           os_memoryFree(pHandle->hOs, lParam, sizeof(paramInfo_t));
           if (bssType != BSS_INFRASTRUCTURE)
           {
               TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "Not in Infrastructure BSS, No ASSOC Info for GET ASSOC_ASSOCIATION_INFORMATION_PARAM\n");
               return TI_NOK;
           }

           /* Init the result buffer to 0 */
           os_memoryZero(pHandle->hOs ,&pParam->content, sizeof(OS_802_11_ASSOCIATION_INFORMATION));

           reqBuffIEOffset  = 4;  /* In Assoc request frame IEs are located from byte 4 */
           respBuffIEOffset = 6;  /* In Assoc response frame the IEs are located from byte 6 */

            /* If the last associate was re-associciation, the current AP MAC address */
            /* is placed before the IEs. Copy it to the result parameters.            */
            if (pHandle->reAssoc)
            {
                MAC_COPY (pParam->content.assocAssociationInformation.RequestFixedIEs.CurrentAPAddress,
                          &pHandle->assocReqBuffer[reqBuffIEOffset]);
                reqBuffIEOffset += MAC_ADDR_LEN;
            }

            /* Calculate length of Info elements in assoc request and response frames */
            if(pHandle->assocReqLen > reqBuffIEOffset)
                RequestIELength = pHandle->assocReqLen - reqBuffIEOffset;

            if(pHandle->assocRespLen > respBuffIEOffset)
                ResponseIELength = pHandle->assocRespLen - respBuffIEOffset;

            /* Copy the association request information */
            pParam->content.assocAssociationInformation.Length = sizeof(OS_802_11_ASSOCIATION_INFORMATION);
            pParam->content.assocAssociationInformation.AvailableRequestFixedIEs = OS_802_11_AI_REQFI_CAPABILITIES | OS_802_11_AI_REQFI_LISTENINTERVAL;
            pParam->content.assocAssociationInformation.RequestFixedIEs.Capabilities = *(TI_UINT16*)&(pHandle->assocReqBuffer[0]);
            pParam->content.assocAssociationInformation.RequestFixedIEs.ListenInterval = *(TI_UINT16*)(&pHandle->assocReqBuffer[2]);

            pParam->content.assocAssociationInformation.RequestIELength = RequestIELength; 
            pParam->content.assocAssociationInformation.OffsetRequestIEs = 0;
            if (RequestIELength > 0)
            {
                pParam->content.assocAssociationInformation.OffsetRequestIEs = (TI_UINT32)&pHandle->assocReqBuffer[reqBuffIEOffset];
            }
            /* Copy the association response information */
            pParam->content.assocAssociationInformation.AvailableResponseFixedIEs = 
                OS_802_11_AI_RESFI_CAPABILITIES | OS_802_11_AI_RESFI_STATUSCODE | OS_802_11_AI_RESFI_ASSOCIATIONID;
            pParam->content.assocAssociationInformation.ResponseFixedIEs.Capabilities = *(TI_UINT16*)&(pHandle->assocRespBuffer[0]);
            pParam->content.assocAssociationInformation.ResponseFixedIEs.StatusCode = *(TI_UINT16*)&(pHandle->assocRespBuffer[2]);
            pParam->content.assocAssociationInformation.ResponseFixedIEs.AssociationId = *(TI_UINT16*)&(pHandle->assocRespBuffer[4]);
            pParam->content.assocAssociationInformation.ResponseIELength = ResponseIELength;
            pParam->content.assocAssociationInformation.OffsetResponseIEs = 0;
            if (ResponseIELength > 0)
            {
                pParam->content.assocAssociationInformation.OffsetResponseIEs = (TI_UINT32)&pHandle->assocRespBuffer[respBuffIEOffset];
            }

       }
        break;
    default:
        return TI_NOK;
    }

    return TI_OK;
}

/**
*
* assoc_setParam - Set a specific parameter to the association SM
*
* \b Description: 
*
* Set a specific parameter to the association SM.
*
* \b ARGS:
*
*  I   - hAssoc - Association SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa assoc_Start, assoc_Stop
*/
TI_STATUS assoc_setParam(TI_HANDLE hAssoc, paramInfo_t *pParam)
{
    assoc_t     *pHandle;

    pHandle = (assoc_t*)hAssoc;

    if ((pHandle == NULL) || (pParam == NULL))
    {
        return TI_NOK;
    }

    switch (pParam->paramType)
    {
    case ASSOC_RESPONSE_TIMEOUT_PARAM:
        /* check bounds */
        if ((pParam->content.assocResponseTimeout >= ASSOC_RESPONSE_TIMEOUT_MIN) &&
            (pParam->content.assocResponseTimeout <= ASSOC_RESPONSE_TIMEOUT_MAX))
        {
            pHandle->timeout = pParam->content.assocResponseTimeout;
        } else {
            return TI_NOK;
        }
        break;
    default:
        return TI_NOK;
    }

    return TI_OK;
}

/**
*
* assoc_smTimeout - Time out event activation function
*
* \b Description: 
*
* Time out event activation function.
*
* \b ARGS:
*
*  I   - hAssoc - Association SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa assoc_Start, assoc_Stop
*/
void assoc_smTimeout(TI_HANDLE hAssoc, TI_BOOL bTwdInitOccured)
{
    assoc_t     *pHandle;

    pHandle = (assoc_t*)hAssoc;


    if (pHandle == NULL)
    {
        return;
    }
    
    pHandle->assocTimeoutCount++;

    assoc_smEvent(pHandle, ASSOC_SM_EVENT_TIMEOUT, hAssoc);
}

/**
*
* assoc_smEvent - Perform an event on the association SM
*
* \b Description: 
*
* Perform an event on the association SM.
*
* \b ARGS:
*
*  I   - pAssoc - Association SM context  \n
*  I   - event - Current event \n
*  I   - pData - event related data
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS assoc_smEvent(assoc_t *pAssoc, TI_UINT8 event, void *pData)
{
    TI_STATUS       status;
    TI_UINT8        nextState;

    status = fsm_GetNextState(pAssoc->pAssocSm, pAssoc->currentState, event, &nextState);
    if (status != TI_OK)
    {
        TRACE0(pAssoc->hReport, REPORT_SEVERITY_ERROR, "ASSOC_SM: ERROR - failed getting next state \n");

        return(TI_NOK);
    }

	TRACE3( pAssoc->hReport, REPORT_SEVERITY_INFORMATION, "assoc_smEvent: <currentState = %d, event = %d> --> nextState = %d\n", pAssoc->currentState, event, nextState);

    status = fsm_Event(pAssoc->pAssocSm, &pAssoc->currentState, event, pData);

    return(status);
}

/* state machine functions */

TI_STATUS assoc_smStartIdle(assoc_t *pAssoc)
{
    TI_STATUS       status;

    status = assoc_smResetRetry(pAssoc);
    status = assoc_smSendAssocReq(pAssoc);
    status = assoc_smStartTimer(pAssoc);
    status = assoc_smIncRetry(pAssoc);

    return status;
}

TI_STATUS assoc_smStopWait(assoc_t *pAssoc)
{
    TI_STATUS       status;

    status = assoc_smStopTimer(pAssoc);

    return status;
}

TI_STATUS assoc_smSuccessWait(assoc_t *pAssoc)
{
    TI_STATUS       status;

    status = assoc_smStopTimer(pAssoc);
    status = assoc_smReportSuccess(pAssoc);

    return status;
}

TI_STATUS assoc_smFailureWait(assoc_t *pAssoc)
{
    TI_STATUS       status;
    TI_UINT16           uRspStatus = *(TI_UINT16*)&(pAssoc->assocRespBuffer[2]);

    status = assoc_smStopTimer(pAssoc);

    /* Sanity check. If the Response status is indeed not 0 */
    if (uRspStatus) 
    {
        status = assoc_smReportFailure(pAssoc, uRspStatus);
    }
    else    /* (uRspStatus == 0) how did we get here ? */ 
    {
        TRACE0(pAssoc->hReport, REPORT_SEVERITY_ERROR, "while Response status is OK (0) !!! \n");

        status = assoc_smReportFailure(pAssoc, (TI_UINT16)TI_NOK);
    }
    return status;
}

TI_STATUS assoc_smTimeoutWait(assoc_t *pAssoc)
{
    TI_STATUS       status;

    status = assoc_smSendAssocReq(pAssoc);
    status = assoc_smStartTimer(pAssoc);
    status = assoc_smIncRetry(pAssoc);

    return status;
}

TI_STATUS assoc_smMaxRetryWait(assoc_t *pAssoc)
{
    TI_STATUS       status;

    status = assoc_smStopTimer(pAssoc);
    status = assoc_smReportFailure(pAssoc, STATUS_PACKET_REJ_TIMEOUT);

    return status;
}

TI_STATUS assoc_smSendAssocReq(assoc_t *pAssoc)
{
    TI_UINT8           *assocMsg;
    TI_UINT32           msgLen;
    TI_STATUS           status;
    dot11MgmtSubType_e  assocType=ASSOC_REQUEST;

    assocMsg = os_memoryAlloc(pAssoc->hOs, MAX_ASSOC_MSG_LENGTH);
    if (!assocMsg)
    {
        return TI_NOK;
    }

    if (pAssoc->reAssoc)
    {
        assocType = RE_ASSOC_REQUEST;
    }
    status = assoc_smRequestBuild(pAssoc, assocMsg, &msgLen);
    if (status == TI_OK) {
        /* Save the association request message */
        assoc_saveAssocReqMessage(pAssoc, assocMsg, msgLen);
        status = mlmeBuilder_sendFrame(pAssoc->hMlme, assocType, assocMsg, msgLen, 0);
    }
    os_memoryFree(pAssoc->hOs, assocMsg, MAX_ASSOC_MSG_LENGTH);
    return status;
}

TI_STATUS assoc_smStopAssoc(assoc_t *pAssoc)
{
    if (pAssoc->disAssoc) {
        assoc_sendDisAssoc(pAssoc, STATUS_UNSPECIFIED);
    }
    return TI_OK;
}

TI_STATUS assoc_smActionUnexpected(assoc_t *pAssoc)
{
    return TI_OK;
}

/* local functions */


TI_STATUS assoc_smResetRetry(assoc_t *pAssoc)
{
    if (pAssoc == NULL)
    {
        return TI_NOK;
    }
    
    pAssoc->retryCount = 0;
    
    return TI_OK;
}

TI_STATUS assoc_smIncRetry(assoc_t *pAssoc)
{
    TI_STATUS       status;

    if (pAssoc == NULL)
    {
        return TI_NOK;
    }
    
    pAssoc->retryCount++;
    
    if (pAssoc->retryCount > pAssoc->maxCount)
    {
        status = assoc_smEvent(pAssoc, ASSOC_SM_EVENT_MAX_RETRY, pAssoc);

        return status;
    }

    return TI_OK;
}

TI_STATUS assoc_smReportSuccess(assoc_t *pAssoc)
{
    TI_STATUS       status;

    if (pAssoc == NULL)
    {
        return TI_NOK;
    }
    status = mlme_reportAssocStatus(pAssoc->hMlme, (TI_UINT16)TI_OK);

    return status;
}

TI_STATUS assoc_smReportFailure(assoc_t *pAssoc, TI_UINT16 uStatusCode)
{
    TI_STATUS       status;

    if (pAssoc == NULL)
    {
        return TI_NOK;
    }
    
    status = mlme_reportAssocStatus(pAssoc->hMlme, uStatusCode);

    return status;
}

TI_STATUS assoc_smStartTimer(assoc_t *pAssoc)
{
    if (pAssoc == NULL)
    {
        return TI_NOK;
    }
    
    tmr_StartTimer (pAssoc->hAssocSmTimer,
                    assoc_smTimeout,
                    (TI_HANDLE)pAssoc,
                    pAssoc->timeout,
                    TI_FALSE);

    return TI_OK;
}

TI_STATUS assoc_smStopTimer(assoc_t *pAssoc)
{
    if (pAssoc == NULL)
    {
        return TI_NOK;
    }
    
    tmr_StopTimer (pAssoc->hAssocSmTimer);

	/* If the timer was stopped it means the association is over,
	   so we can clear the generic IE */
	rsn_clearGenInfoElement(pAssoc->hRsn);

    return TI_OK;
}

/*****************************************************************************
**
** Association messages builder/Parser
**
*****************************************************************************/

TI_STATUS assoc_smCapBuild(assoc_t *pCtx, TI_UINT16 *cap)
{
    paramInfo_t         param;
    TI_STATUS           status;
    EDot11Mode          mode;
    TI_UINT32           rateSuppMask, rateBasicMask;
    TI_UINT8            ratesBuf[DOT11_MAX_SUPPORTED_RATES];
    TI_UINT32           len = 0, ofdmIndex = 0;
    TI_BOOL             b11nEnable, bWmeEnable;

    *cap = 0;

    /* Bss type */
    param.paramType = CTRL_DATA_CURRENT_BSS_TYPE_PARAM;
    status =  ctrlData_getParam(pCtx->hCtrlData, &param);
    if (status == TI_OK)
    {
        if (param.content.ctrlDataCurrentBssType == BSS_INFRASTRUCTURE)
        {
            *cap |= DOT11_CAPS_ESS;
        } else {
            *cap |= DOT11_CAPS_IBSS;
        }
    } else {
        return TI_NOK;
    }

    /* Privacy */
    param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
    status =  rsn_getParam(pCtx->hRsn, &param);
    if (status == TI_OK)
    {
        if (param.content.rsnEncryptionStatus != TWD_CIPHER_NONE)
        {
            *cap |= DOT11_CAPS_PRIVACY;
        }
    } else {
        return TI_NOK;
    }

    /* Preamble */
    param.paramType = SITE_MGR_DESIRED_PREAMBLE_TYPE_PARAM;
    status =  siteMgr_getParam(pCtx->hSiteMgr, &param);
    if (status == TI_OK)
    {
        if (param.content.siteMgrCurrentPreambleType == PREAMBLE_SHORT)
            *cap |= DOT11_CAPS_SHORT_PREAMBLE;
    } else {
        return TI_NOK;
    }

    /* Pbcc */
    param.paramType = SITE_MGR_CURRENT_RATE_PAIR_PARAM;
    status =  siteMgr_getParam(pCtx->hSiteMgr, &param);
    if (status == TI_OK)
    {
        if(param.content.siteMgrCurrentRateMask.supportedRateMask & DRV_RATE_MASK_22_PBCC)
            *cap |= DOT11_CAPS_PBCC;
    } else {
        return TI_NOK;
    }

    
    /* Checking if the station supports Spectrum Management (802.11h) */
    param.paramType = REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM;
    status =  regulatoryDomain_getParam(pCtx->hRegulatoryDomain, &param);
    if (status == TI_OK )
    {
        if( param.content.spectrumManagementEnabled)
            *cap |= DOT11_SPECTRUM_MANAGEMENT;
    } 
    else
    {
        return TI_NOK;
    }
    
    /* slot time */
    param.paramType = SITE_MGR_OPERATIONAL_MODE_PARAM;
    status = siteMgr_getParam(pCtx->hSiteMgr, &param);
    if(status == TI_OK)
    {
        mode = param.content.siteMgrDot11OperationalMode;
    }
    else
        return TI_NOK;

    if(mode == DOT11_G_MODE)
    {
        /* new requirement: the short slot time should be set only
           if the AP's modulation is OFDM (highest rate) */
        
        /* get Rates */
        param.paramType = SITE_MGR_CURRENT_RATE_PAIR_PARAM;
        status =  siteMgr_getParam(pCtx->hSiteMgr, &param);
        if (status == TI_OK)
        {
            rateBasicMask = param.content.siteMgrCurrentRateMask.basicRateMask;
            rateSuppMask  = param.content.siteMgrCurrentRateMask.supportedRateMask;
        } else {
            return TI_NOK;
        }
        
        /* convert the bit map to the rates array */
        rate_DrvBitmapToNetStr (rateSuppMask, rateBasicMask, ratesBuf, &len, &ofdmIndex);

        if(ofdmIndex < len)
            *cap |= DOT11_CAPS_SHORT_SLOT_TIME;

/*      
        param.paramType = SITE_MGR_CURRENT_MODULATION_TYPE_PARAM;
        status = siteMgr_getParam(pCtx->hSiteMgr, &param);
        if(param.content.siteMgrCurrentModulationType == DRV_MODULATION_OFDM)
            *cap |= DOT11_CAPS_SHORT_SLOT_TIME;
*/
    }

    /* Primary Site support HT ? */
    param.paramType = SITE_MGR_PRIMARY_SITE_HT_SUPPORT;
    siteMgr_getParam(pCtx->hSiteMgr, &param);

    if (param.content.bPrimarySiteHtSupport == TI_TRUE)
    {
        /* Immediate Block Ack subfield - (is WME on?) AND (is HT Enable?) */
        /* verify 11n_Enable and Chip type */
        StaCap_IsHtEnable (pCtx->hStaCap, &b11nEnable);
        /* verify that WME flag enable */
        qosMngr_GetWmeEnableFlag (pCtx->hQosMngr, &bWmeEnable); 
    
        if ((b11nEnable != TI_FALSE) && (bWmeEnable != TI_FALSE))
        {
            *cap |= DOT11_CAPS_IMMEDIATE_BA;
        }
    }

    return TI_OK;
}


TI_STATUS assoc_smSSIDBuild(assoc_t *pCtx, TI_UINT8 *pSSID, TI_UINT32 *ssidLen)
{
    paramInfo_t         param;
    TI_STATUS               status;
    dot11_SSID_t        *pDot11Ssid;

    pDot11Ssid = (dot11_SSID_t*)pSSID;
    /* set SSID element id */
    pDot11Ssid->hdr[0] = SSID_IE_ID;

    /* get SSID */
    param.paramType = SME_DESIRED_SSID_ACT_PARAM;
    status =  sme_GetParam(pCtx->hSme, &param);
    if (status != TI_OK)
    {
        return status;
    }
    
    /* check for ANY ssid */
    if (param.content.smeDesiredSSID.len != 0)
    {
        pDot11Ssid->hdr[1] = param.content.smeDesiredSSID.len;
        os_memoryCopy(pCtx->hOs, 
                      (void *)pDot11Ssid->serviceSetId, 
                      (void *)param.content.smeDesiredSSID.str, 
                      param.content.smeDesiredSSID.len);

    } else {
        /* if ANY ssid is configured, use the current SSID */
        param.paramType = SITE_MGR_CURRENT_SSID_PARAM;
        status =  siteMgr_getParam(pCtx->hSiteMgr, &param);
        if (status != TI_OK)
        {
            return status;
        }
        pDot11Ssid->hdr[1] = param.content.siteMgrCurrentSSID.len;
        os_memoryCopy(pCtx->hOs, 
                      (void *)pDot11Ssid->serviceSetId, 
                      (void *)param.content.siteMgrCurrentSSID.str, 
                      param.content.siteMgrCurrentSSID.len);

    }

    *ssidLen = pDot11Ssid->hdr[1] + sizeof(dot11_eleHdr_t);
    
    return TI_OK;
}

TI_STATUS assoc_smRatesBuild(assoc_t *pCtx, TI_UINT8 *pRates, TI_UINT32 *ratesLen)
{
    paramInfo_t         param;
    TI_STATUS           status;
    TI_UINT32           rateSuppMask, rateBasicMask;
    dot11_RATES_t       *pDot11Rates;
    TI_UINT32           len = 0, ofdmIndex = 0;
    TI_UINT8            ratesBuf[DOT11_MAX_SUPPORTED_RATES];
    EDot11Mode          mode;
    TI_UINT32           suppRatesLen, extSuppRatesLen, i;
    pDot11Rates = (dot11_RATES_t*)pRates;


    /* get Rates */
    param.paramType = SITE_MGR_CURRENT_RATE_PAIR_PARAM;
    status =  siteMgr_getParam(pCtx->hSiteMgr, &param);
    if (status == TI_OK)
    {
        rateBasicMask = param.content.siteMgrCurrentRateMask.basicRateMask;
        rateSuppMask  = param.content.siteMgrCurrentRateMask.supportedRateMask;
    } 
    else 
    {
        return TI_NOK;
    }

    /* get operational mode */
    param.paramType = SITE_MGR_OPERATIONAL_MODE_PARAM;
    status = siteMgr_getParam(pCtx->hSiteMgr, &param);
    if(status == TI_OK)
        mode = param.content.siteMgrDot11OperationalMode;
    else
        return TI_NOK;

    /* convert the bit map to the rates array */
    /* remove MCS rates from Extended Supported Rates IE */
    rateSuppMask &= ~(DRV_RATE_MASK_MCS_0_OFDM | 
                      DRV_RATE_MASK_MCS_1_OFDM |
                      DRV_RATE_MASK_MCS_2_OFDM |
                      DRV_RATE_MASK_MCS_3_OFDM |
                      DRV_RATE_MASK_MCS_4_OFDM |
                      DRV_RATE_MASK_MCS_5_OFDM |
                      DRV_RATE_MASK_MCS_6_OFDM |
                      DRV_RATE_MASK_MCS_7_OFDM );

    rate_DrvBitmapToNetStr (rateSuppMask, rateBasicMask, ratesBuf, &len, &ofdmIndex);

    if(mode != DOT11_G_MODE || ofdmIndex == len )
    {
        pDot11Rates->hdr[0] = SUPPORTED_RATES_IE_ID;
        pDot11Rates->hdr[1] = len;
        os_memoryCopy(NULL, (void *)pDot11Rates->rates, ratesBuf, len);
        *ratesLen = pDot11Rates->hdr[1] + sizeof(dot11_eleHdr_t);
    }
    else 
    {
        /* fill in the supported rates */
        pDot11Rates->hdr[0] = SUPPORTED_RATES_IE_ID;
        pDot11Rates->hdr[1] = ofdmIndex;
        os_memoryCopy(NULL, (void *)pDot11Rates->rates, ratesBuf, pDot11Rates->hdr[1]);
        suppRatesLen = pDot11Rates->hdr[1] + sizeof(dot11_eleHdr_t);
        /* fill in the extended supported rates */
        pDot11Rates = (dot11_RATES_t*)(pRates + suppRatesLen);
        pDot11Rates->hdr[0] = EXT_SUPPORTED_RATES_IE_ID;
        pDot11Rates->hdr[1] = len - ofdmIndex;
        os_memoryCopy(NULL, (void *)pDot11Rates->rates, &ratesBuf[ofdmIndex], pDot11Rates->hdr[1]);
        extSuppRatesLen = pDot11Rates->hdr[1] + sizeof(dot11_eleHdr_t);
        *ratesLen = suppRatesLen + extSuppRatesLen;
    }

    TRACE3(pCtx->hReport, REPORT_SEVERITY_INFORMATION, "ASSOC_SM: ASSOC_REQ - bitmapSupp= 0x%X,bitMapBasic = 0x%X, len = %d\n", rateSuppMask,rateBasicMask,len);
    for(i=0; i<len; i++)
    {
        TRACE2(pCtx->hReport, REPORT_SEVERITY_INFORMATION, "ASSOC_SM: ASSOC_REQ - ratesBuf[%d] = 0x%X\n", i, ratesBuf[i]);
    }

    return TI_OK;
}

TI_STATUS assoc_powerCapabilityBuild(assoc_t *pCtx, TI_UINT8 *pPowerCapability, TI_UINT32 *powerCapabilityLen)
{
    paramInfo_t         param;
    TI_STATUS               status;
    dot11_CAPABILITY_t      *pDot11PowerCapability;

    pDot11PowerCapability = (dot11_CAPABILITY_t*)pPowerCapability;
    
    /* set Power Capability element id */
    pDot11PowerCapability->hdr[0] = DOT11_CAPABILITY_ELE_ID;
    pDot11PowerCapability->hdr[1] = DOT11_CAPABILITY_ELE_LEN;

    /* get power capability */
    param.paramType = REGULATORY_DOMAIN_POWER_CAPABILITY_PARAM;
    status =  regulatoryDomain_getParam(pCtx->hRegulatoryDomain, &param);

    if (status == TI_OK)
    {
        pDot11PowerCapability->minTxPower = param.content.powerCapability.minTxPower;
        pDot11PowerCapability->maxTxPower = param.content.powerCapability.maxTxPower;
        *powerCapabilityLen = pDot11PowerCapability->hdr[1] + sizeof(dot11_eleHdr_t);
    }
    else
        *powerCapabilityLen = 0;

    return TI_OK;
}

TI_STATUS assoc_smRequestBuild(assoc_t *pCtx, TI_UINT8* reqBuf, TI_UINT32* reqLen)
{
    TI_STATUS       status;
    TI_UINT8        *pRequest;
    TI_UINT32       len;
    paramInfo_t     param;
    TTwdParamInfo   tTwdParam;
    TI_UINT16       capabilities;
    ECipherSuite    eCipherSuite = TWD_CIPHER_NONE; /* To be used for checking whether 
                                                       AP supports HT rates and TKIP 
                                                     */

    pRequest = reqBuf;
    *reqLen = 0;

    /* insert capabilities */
    status = assoc_smCapBuild(pCtx, &capabilities);
    if (status == TI_OK)
    {
        *(TI_UINT16*)pRequest = capabilities;
    }
    else
        return TI_NOK;

    pRequest += 2;
    *reqLen += 2;

    /* insert listen interval */
    tTwdParam.paramType = TWD_LISTEN_INTERVAL_PARAM_ID;
    status =  TWD_GetParam (pCtx->hTWD, &tTwdParam);
    if (status == TI_OK)
    {
        *(TI_UINT16*)pRequest = ENDIAN_HANDLE_WORD((TI_UINT16)tTwdParam.content.halCtrlListenInterval);
    } else {
        return TI_NOK;
    }
    
    pRequest += 2;
    *reqLen += 2;
    if (pCtx->reAssoc)
    {   /* Insert currentAPAddress element only in reassoc request*/
        param.paramType = SITE_MGR_PREV_SITE_BSSID_PARAM;
        status = siteMgr_getParam(pCtx->hSiteMgr, &param);
        if (status == TI_OK)
        {
            MAC_COPY (pRequest, param.content.siteMgrDesiredBSSID);
            TRACE6(pCtx->hReport, REPORT_SEVERITY_INFORMATION, "ASSOC_SM: ASSOC_REQ - prev AP = %x-%x-%x-%x-%x-%x\n", param.content.siteMgrDesiredBSSID[0], param.content.siteMgrDesiredBSSID[1], param.content.siteMgrDesiredBSSID[2], param.content.siteMgrDesiredBSSID[3], param.content.siteMgrDesiredBSSID[4], param.content.siteMgrDesiredBSSID[5]);


            pRequest += MAC_ADDR_LEN;
            *reqLen += MAC_ADDR_LEN;
        }
        else
        {
            TRACE0(pCtx->hReport, REPORT_SEVERITY_ERROR, "ASSOC_SM: ASSOC_REQ - No prev AP \n");
            return status;

        }
    }

    /* insert SSID element */
    status = assoc_smSSIDBuild(pCtx, pRequest, &len);
    if (status != TI_OK)
    {
        return TI_NOK;
    }

    pRequest += len;
    *reqLen += len;

    /* insert Rates element */
    status = assoc_smRatesBuild(pCtx, pRequest, &len);
    if (status != TI_OK)
    {
        return TI_NOK;
    }
    pRequest += len;
    *reqLen += len;

    /* Checking if the station supports Spectrum Management (802.11h) */
    param.paramType = REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM;
    status = regulatoryDomain_getParam(pCtx->hRegulatoryDomain,&param);
    if( (status == TI_OK) && param.content.spectrumManagementEnabled)
    {
        /* Checking the selected AP capablities */
        param.paramType = SITE_MGR_SITE_CAPABILITY_PARAM;
        status =  siteMgr_getParam(pCtx->hSiteMgr,&param);
        if(status == TI_OK && ((param.content.siteMgrSiteCapability & DOT11_SPECTRUM_MANAGEMENT) != 0)) 
        {
            /* insert Power capability element */
            status = assoc_powerCapabilityBuild(pCtx, pRequest, &len);
            if (status != TI_OK)
            {
                return TI_NOK;
            }
            pRequest += len;
            *reqLen += len;
#if 0
            /* insert Supported Channels element */
            status = assoc_supportedChannelBuild(pCtx, pRequest, &len);
            if (status != TI_OK)
            {
                return TI_NOK;
            }
            pRequest += len;
            *reqLen += len;
#endif
        }


    }

    status = qosMngr_getQosCapabiltyInfeElement(pCtx->hQosMngr,pRequest,&len);
    if (status != TI_OK)
    {
        return TI_NOK;
    }
    pRequest += len;
    *reqLen += len;


#ifdef XCC_MODULE_INCLUDED
    status = rsn_getXCCExtendedInfoElement(pCtx->hRsn, pRequest, (TI_UINT8*)&len);
    if (status != TI_OK)
    {
        return TI_NOK;
    }
    pRequest += len;
    *reqLen += len;

    if (pCtx->reAssoc)
    {   /* insert CCKM information element only in reassoc */
        status = XCCMngr_getCckmInfoElement(pCtx->hXCCMngr, pRequest, (TI_UINT8*)&len);
        
        if (status != TI_OK)
        {
            return TI_NOK;
        }
        pRequest += len;
        *reqLen += len;
    }
    status = XCCMngr_getXCCVersionInfoElement(pCtx->hXCCMngr, pRequest, (TI_UINT8*)&len);
    if (status != TI_OK)
    {
        return TI_NOK;
    }
    pRequest += len;
    *reqLen += len;

    /* Insert Radio Mngt Capability IE */
    status = measurementMgr_radioMngtCapabilityBuild(pCtx->hMeasurementMgr, pRequest, (TI_UINT8*)&len);
    if (status != TI_OK)
    {
        return TI_NOK;
    }
    pRequest += len;
    *reqLen += len;
#endif

     /* Get Simple-Config state */
    param.paramType = SITE_MGR_SIMPLE_CONFIG_MODE;
    status = siteMgr_getParam(pCtx->hSiteMgr, &param);

   if (param.content.siteMgrWSCMode.WSCMode == TIWLN_SIMPLE_CONFIG_OFF)
   {
    /* insert RSN information elements */
    status = rsn_getInfoElement(pCtx->hRsn, pRequest, &len);
	
	if (status != TI_OK)
	{
		return TI_NOK;
	}
	pRequest += len;
	*reqLen += len;
  }

    /* Privacy - Used later on HT */
    param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
    status          = rsn_getParam(pCtx->hRsn, &param);
   
    if(status == TI_OK)
    {
        eCipherSuite = param.content.rsnEncryptionStatus;
    } 


    /* Primary Site support HT ? */
    param.paramType = SITE_MGR_PRIMARY_SITE_HT_SUPPORT;
    siteMgr_getParam(pCtx->hSiteMgr, &param);

    /* Disallow TKIP with HT Rates: If this is the case - discard HT rates from Association Request */
    if((TI_TRUE == param.content.bPrimarySiteHtSupport) && (eCipherSuite != TWD_CIPHER_TKIP))
    {
        status = StaCap_GetHtCapabilitiesIe (pCtx->hStaCap, pRequest, &len);
    	if (status != TI_OK)
    	{
    		return TI_NOK;
    	}
    	pRequest += len;
    	*reqLen += len;
    }

	status = qosMngr_assocReqBuild(pCtx->hQosMngr,pRequest,&len);
	if (status != TI_OK)
	{
		return TI_NOK;
	}
	pRequest += len;
	*reqLen += len;

	status = apConn_getVendorSpecificIE(pCtx->hApConn, pRequest, &len);
	if (status != TI_OK)
	{
		return TI_NOK;
	}
	pRequest += len;
	*reqLen += len;
 
    if (*reqLen>=MAX_ASSOC_MSG_LENGTH)
    {
        return TI_NOK;
    }

    return TI_OK;
}



TI_STATUS assoc_saveAssocRespMessage(assoc_t *pAssocSm, TI_UINT8 *pAssocBuffer, TI_UINT32 length)
{
    if ((pAssocSm==NULL) || (pAssocBuffer==NULL) || (length>=MAX_ASSOC_MSG_LENGTH))
    {
        return TI_NOK;
    }
    os_memoryCopy(pAssocSm->hOs, pAssocSm->assocRespBuffer, pAssocBuffer, length);  
    pAssocSm->assocRespLen = length;
    
    TRACE1(pAssocSm->hReport, REPORT_SEVERITY_INFORMATION, "assoc_saveAssocRespMessage: length=%ld \n",length);
    return TI_OK;
}

TI_STATUS assoc_saveAssocReqMessage(assoc_t *pAssocSm, TI_UINT8 *pAssocBuffer, TI_UINT32 length)
{

    if ((pAssocSm==NULL) || (pAssocBuffer==NULL) || (length>=MAX_ASSOC_MSG_LENGTH))
    {
        return TI_NOK;
    }

    os_memoryCopy(pAssocSm->hOs, pAssocSm->assocReqBuffer, pAssocBuffer, length);  
    pAssocSm->assocReqLen = length;
    
    TRACE1(pAssocSm->hReport, REPORT_SEVERITY_INFORMATION, "assoc_saveAssocReqMessage: length=%ld \n",length);
    return TI_OK;
}


TI_STATUS assoc_sendDisAssoc(assoc_t *pAssocSm, mgmtStatus_e reason)
{
    TI_STATUS       status;
    disAssoc_t      disAssoc;

    if (reason == STATUS_SUCCESSFUL)
    {
        disAssoc.reason = ENDIAN_HANDLE_WORD(STATUS_UNSPECIFIED);
    } else {
        disAssoc.reason = ENDIAN_HANDLE_WORD(reason);
    }

    status = mlmeBuilder_sendFrame(pAssocSm->hMlme, DIS_ASSOC, (TI_UINT8*)&disAssoc, sizeof(disAssoc_t), 0);

    return status;
}


