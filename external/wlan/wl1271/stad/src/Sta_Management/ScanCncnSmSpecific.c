/*
 * ScanCncnSmSpecific.c
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

 /** \file  ScanCncnSmSpecific.c
 *  \brief Scan concentrator state machine type-specific functions implemenattion
 *
 *  \see   ScanCncnSm.h, ScanCncnSm.c
 */


#define __FILE_ID__  FILE_ID_80
#include "scrApi.h"
#include "GenSM.h"
#include "TWDriver.h"
#include "ScanCncnSm.h"
#include "ScanCncnPrivate.h"
#include "apConn.h"

/* 
 * Aplication one-shot scan
 */
/** 
 * \fn     scanCncnSmApp1Shot_ScrRequest 
 * \brief  Request the SCR for one-shot application scan
 * 
 * Request the SCR for one-shot application scan. handle different results.
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmApp1Shot_ScrRequest (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient         *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    EScrClientRequestStatus eScrReplyStatus;
    EScePendReason          eScrPendReason;

    /* request the SCR as application scan client, and act according to return status */
    switch (eScrReplyStatus = scr_clientRequest (pScanCncnClient->hSCR, SCR_CID_APP_SCAN, 
                                                 SCR_RESOURCE_SERVING_CHANNEL, &eScrPendReason))
    {
    case SCR_CRS_PEND:
        /* send a reject event to the SM */
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmApp1Shot_ScrRequest: SCR pending, pend reason: %d.\n", eScrPendReason);
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        break;

    case SCR_CRS_RUN:
        /* send a run event to the SM */
        TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmApp1Shot_ScrRequest: SCR acquired.\n");
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_RUN, hScanCncnClient);
        break;

    default:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmApp1Shot_ScrRequest: SCR returned unrecognized status: %d.\n", eScrReplyStatus);
        /* Send a reject event to recover from this error */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);        
        break;
   }
}

/** 
 * \fn     scanCncnSmApp1Shot_ScrRelease
 * \brief  Release the SCR as one-shot application scan
 * 
 * Release the SCR as one-shot application scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmApp1Shot_ScrRelease (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* release the SCR */
    scr_clientComplete (pScanCncnClient->hSCR, SCR_CID_APP_SCAN, SCR_RESOURCE_SERVING_CHANNEL);
}

/** 
 * \fn     scanCncnSmApp1Shot_StartScan 
 * \brief  Request scan start from TWD for one-shot application scan
 * 
 * Request scan start from TWD for one-shot application scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmApp1Shot_StartScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TScanCncn       *pScanCncn = (TScanCncn*)pScanCncnClient->hScanCncn;
    TI_BOOL         bPsRequired;
    TI_STATUS       tStatus;

    /* if the STA is connected, it is rquired to enter PS before scan */
    bPsRequired = (STA_CONNECTED == pScanCncn->eConnectionStatus ? TI_TRUE : TI_FALSE);

    /* call the TWD start scan - enter driver mode (PS) only if station is connected */
    tStatus = TWD_Scan (pScanCncnClient->hTWD, &(pScanCncnClient->uScanParams.tOneShotScanParams), SCAN_RESULT_TAG_APPLICATION_ONE_SHOT,
                       TI_FALSE, bPsRequired, TI_FALSE, POWER_SAVE_ON, /* this parameter is used only when driver mode requested */
                       bPsRequired, NULL, NULL);

    if (TI_OK != tStatus)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmApp1Shot_StartScan: TWD returned status %d, quitting app scan.\n", tStatus);

        /* mark the return status */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;

        /* could not start scan, send a scan complete event to reset the SM */
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmApp1Shot_StopScan
 * \brief  Request scan stop from TWD for one-shot application scan
 * 
 * Request scan stop from TWD for one-shot application scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmApp1Shot_StopScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TScanCncn       *pScanCncn = (TScanCncn*)pScanCncnClient->hScanCncn;
    TI_STATUS       tStatus;

    /* call the TWD stop scan function */
    if (pScanCncn->eConnectionStatus != STA_CONNECTED)
    {
        tStatus = TWD_StopScan (pScanCncnClient->hTWD, SCAN_RESULT_TAG_APPLICATION_ONE_SHOT, TI_FALSE, NULL, NULL);
    }
    else
    {
        tStatus = TWD_StopScan (pScanCncnClient->hTWD, SCAN_RESULT_TAG_APPLICATION_ONE_SHOT, pScanCncnClient->bSendNullDataOnStop, NULL, NULL);
    }

    /* if stop scan operation failed, send a scan complete event to reset the SM */
    if (TI_OK != tStatus)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmApp1Shot_StopScan: status %d from TWD_StopScan, sending scan complete to SM\n", tStatus);
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmApp1Shot_Recovery
 * \brief  Handles recovery during scan for one-shot application scan
 * 
 * Notifies the scan SRV to stop its timer
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmApp1Shot_Recovery (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* Notify scan SRV to stop its timer */
    TWD_StopScanOnFWReset (pScanCncnClient->hTWD);
}

/*
 * Aplication Periodic scan
 */
/** 
 * \fn     scanCncnSmAppP_ScrRequest 
 * \brief  Request the SCR for periodic application scan 
 * 
 * Request the SCR for periodic application scan. Handle different results
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmAppP_ScrRequest (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient         *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    EScrClientRequestStatus eScrReplyStatus;
    EScePendReason          eScrPendReason;

    /* request the SCR as application scan client, and act according to return status */
    switch (eScrReplyStatus = scr_clientRequest (pScanCncnClient->hSCR, SCR_CID_APP_SCAN, 
                                                 SCR_RESOURCE_PERIODIC_SCAN, &eScrPendReason))
    {
    case SCR_CRS_PEND:
        /* send a reject event to the SM */
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmAppP_ScrRequest: SCR pending, pend reason: %d.\n", eScrPendReason);
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        break;

    case SCR_CRS_RUN:
        /* send a run event to the SM */
        TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmAppP_ScrRequest: SCR acquired.\n");
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_RUN, hScanCncnClient);
        break;

    default:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmAppP_ScrRequest: SCR returned unrecognized status: %d.\n", eScrReplyStatus);
        /* Send a reject event to recover from this error */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        break;
   }
}

/** 
 * \fn     scanCncnSmAppP_ScrRelease 
 * \brief  Release the SCR as periodic application scan 
 * 
 * Release the SCR as periodic application scan 
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmAppP_ScrRelease (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* release the SCR */
    scr_clientComplete (pScanCncnClient->hSCR, SCR_CID_APP_SCAN, SCR_RESOURCE_PERIODIC_SCAN);
}

/** 
 * \fn     scanCncnSmAppP_StartScan
 * \brief  Request scan start from TWD for periodic application scan
 * 
 * Request scan start from TWD for periodic application scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmAppP_StartScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TScanCncn       *pScanCncn = (TScanCncn*)pScanCncnClient->hScanCncn;
    TI_STATUS       tStatus;

    /* call the TWD start scan */
    tStatus = TWD_StartConnectionScan (pScanCncnClient->hTWD, &(pScanCncnClient->uScanParams.tPeriodicScanParams),
                                       SCAN_RESULT_TAG_APPLICATION_PEIODIC, 
                                       pScanCncn->tInitParams.uDfsPassiveDwellTimeMs, NULL, NULL);
    if (TI_OK != tStatus)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmAppP_StartScan: TWD returned status %d, quitting app scan.\n", tStatus);

        /* mark the return status */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;

        /* could not start scan, send a scan complete event to reset the SM */
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmAppP_StopScan
 * \brief  Request scan stop from TWD for periodic application scan
 * 
 * Request scan stop from TWD for periodic application scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmAppP_StopScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TI_STATUS       status;

    /* call the TWD stop periodic scan function */
    status = TWD_StopPeriodicScan (pScanCncnClient->hTWD, SCAN_RESULT_TAG_APPLICATION_PEIODIC, NULL, NULL);

    /* if stop scan operation failed, send a scan complete event to reset the SM */
    if (TI_OK != status)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmAppP_StopScan: status %d from TWD_StopPeriodicScan, sending scan complete to SM\n", status);
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmAppP_Recovery
 * \brief  Handles recovery during scan for periodic application scan
 * 
 * Handles recovery during scan for periodic application scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmAppP_Recovery (TI_HANDLE hScanCncnClient)
{
}

/*
 * Driver periodic scan
 */
/** 
 * \fn     scanCncnSmDrvP_ScrRequest
 * \brief  Request the SCR for periodic driver scan
 * 
 * Request the SCR for periodic driver scan. Handle different results
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmDrvP_ScrRequest (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient         *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    EScrClientRequestStatus eScrReplyStatus;
    EScePendReason          eScrPendReason;

    /* request the SCR as driver scan client, and act according to return status */
    switch (eScrReplyStatus = scr_clientRequest( pScanCncnClient->hSCR, SCR_CID_DRIVER_FG_SCAN,
                                                 SCR_RESOURCE_PERIODIC_SCAN, &eScrPendReason ) )
    {
    case SCR_CRS_PEND:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmAppP_ScrRequest: SCR pending, pend reason: %d.\n", eScrPendReason);
        
        /* check the pending reason */
        if (SCR_PR_OTHER_CLIENT_ABORTING != eScrPendReason)
        {
            /* 
             * send a reject event to the SM - would not scan if not in a different group or 
             * another un-abortable client is running
             */
            pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED; 
            genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        }
        /* if the pending reason is another client aborting wait untill it finish abort */
        break;

    case SCR_CRS_RUN:
        /* send a run event to the SM */
        TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmAppP_ScrRequest: SCR acquired.\n");
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_RUN, hScanCncnClient);
        break;

    default:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmAppP_ScrRequest: SCR returned unrecognized status: %d\n", eScrReplyStatus);
        /* Send a reject event to recover from this error */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        break;
    }
}

/** 
 * \fn     scanCncnSmDrvP_ScrRelease
 * \brief  Release the SCR as periodic driver scan
 * 
 * Release the SCR as periodic driver scan 
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmDrvP_ScrRelease (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* release the SCR */
    scr_clientComplete (pScanCncnClient->hSCR, SCR_CID_DRIVER_FG_SCAN, SCR_RESOURCE_PERIODIC_SCAN);
}

/** 
 * \fn     scanCncnSmDrvP_StartScan
 * \brief  Request scan start from TWD for periodic driver scan
 * 
 * Request scan start from TWD for periodic driver scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmDrvP_StartScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TScanCncn       *pScanCncn = (TScanCncn*)pScanCncnClient->hScanCncn;
    TI_STATUS       status;

    /* call the TWD_scan function */
    status = TWD_StartConnectionScan (pScanCncnClient->hTWD, &(pScanCncnClient->uScanParams.tPeriodicScanParams),
                                      SCAN_RESULT_TAG_DRIVER_PERIODIC, 
                                      pScanCncn->tInitParams.uDfsPassiveDwellTimeMs, NULL, NULL);

    if (TI_OK != status)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmDrvP_StartScan: TWD returned status %d, quitting app scan.\n", status);

        /* mark the return status */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;

        /* could not start scan, send a scan complete event to reset the SM */
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmDrvP_StopScan
 * \brief  Request scan stop from TWD for periodic driver scan
 * 
 * Request scan stop from TWD for periodic driver scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmDrvP_StopScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TI_STATUS       status;

    /* call the TWD stop periodic scan */
    status = TWD_StopPeriodicScan (pScanCncnClient->hTWD, SCAN_RESULT_TAG_DRIVER_PERIODIC, NULL, NULL);

    /* if stop scan operation failed, send a scan complete event to reset the SM */
    if (TI_OK != status)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmDrvP_StopScan: status %d from TWD_StopPeriodicScan, sending scan complete to SM\n", status);
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmApp1Shot_Recovery
 * \brief  Handles recovery during scan for periodic driver scan
 * 
 * Handles recovery during scan for periodic driver scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmDrvP_Recovery (TI_HANDLE hScanCncnClient)
{
}

/*
 * Continuous one-shot scan
 */
/** 
 * \fn     scanCncnSmCont1Shot_ScrRequest
 * \brief  Request the SCR for one-shot continuous scan
 * 
 * Request the SCR for one-shot continuous scan. Handle different results
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmCont1Shot_ScrRequest (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient         *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    EScrClientRequestStatus eScrReplyStatus;
    EScePendReason          eScrPendReason;

    /* request the SCR as continuous roaming client, and act according to return status */
    switch (eScrReplyStatus = scr_clientRequest (pScanCncnClient->hSCR, SCR_CID_CONT_SCAN, 
                                                SCR_RESOURCE_SERVING_CHANNEL, &eScrPendReason ) )
    {
    case SCR_CRS_PEND:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmCont1Shot_ScrRequest: SCR pending, pend Reason: %d.\n", eScrPendReason);
        /* check pend reason */
        if (SCR_PR_DIFFERENT_GROUP_RUNNING == eScrPendReason)
        {
            /* send a reject event to the SM - will not scan if not in connected group */
            pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
            genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        }
        /* for other pending reasons wait untill current client finishes */
        break;

    case SCR_CRS_RUN:
        /* send a run event to the SM */
        TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmCont1Shot_ScrRequest: SCR acquired.\n");
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_RUN, hScanCncnClient);
        break;

    default:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmCont1Shot_ScrRequest: SCR returned unrecognized status: %d.\n", eScrReplyStatus);
        /* Send a reject event to recover from this error */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        break;
    }
}

/** 
 * \fn     scanCncnSmCont1Shot_ScrRelease
 * \brief  Release the SCR as one-shot continuous scan
 * 
 * Release the SCR as one-shot continuous scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmCont1Shot_ScrRelease (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* release the SCR */
    scr_clientComplete (pScanCncnClient->hSCR, SCR_CID_CONT_SCAN, SCR_RESOURCE_SERVING_CHANNEL);
}

/** 
 * \fn     scanCncnSmCont1Shot_StartScan 
 * \brief  Request scan start from TWD for one-shot continuous scan
 * 
 * Request scan start from TWD for one-shot continuous scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmCont1Shot_StartScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TI_STATUS       status;

    /* call the TWD start scan function */
    status = TWD_Scan (pScanCncnClient->hTWD, &(pScanCncnClient->uScanParams.tOneShotScanParams), 
                       SCAN_RESULT_TAG_CONTINUOUS, TI_FALSE, TI_TRUE, TI_FALSE, POWER_SAVE_ON, TI_TRUE,
                       NULL, NULL);

    if (TI_OK != status)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmCont1Shot_StartScan: TWD returned status %d, quitting continuous scan.\n", status);

        /* mark the return status */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;

        /* could not start scan, send a scan complete event */
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmCont1Shot_StopScan
 * \brief  Request scan stop from TWD for one-shot continuous scan
 * 
 * Request scan stop from TWD for one-shot continuous scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmCont1Shot_StopScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TI_STATUS       status;

    /* send a stop scan command to FW */
    status = TWD_StopScan (pScanCncnClient->hTWD, SCAN_RESULT_TAG_CONTINUOUS, pScanCncnClient->bSendNullDataOnStop, NULL, NULL);

    /* if stop scan operation failed, send a scan complete event to reset the SM */
    if (TI_OK != status)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmCont1Shot_StopScan: status %d from TWD_StopScan, sending scan complete to SM\n", status);
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmCont1Shot_Recovery
 * \brief  Handles recovery during scan for one-shot continuous scan
 * 
 * Notifies the scan SRV to stop its timer
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmCont1Shot_Recovery (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* Notify scan SRV to stop its timer */
    TWD_StopScanOnFWReset (pScanCncnClient->hTWD);
}

/*
 * Immediate one-shot scan
 */
/** 
 * \fn     scanCncnSmImmed1Shot_ScrRequest
 * \brief  Request the SCR for one-shot immediate scan
 * 
 * Request the SCR for one-shot immediate scan. Handle different results
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmImmed1Shot_ScrRequest (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient         *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    EScrClientRequestStatus eScrReplyStatus;
    EScePendReason          eScrPendReason;
    
    /* request the SCR as immediate roaming client, and act according to return status */
    switch (eScrReplyStatus = scr_clientRequest (pScanCncnClient->hSCR, SCR_CID_IMMED_SCAN,
                                                 SCR_RESOURCE_SERVING_CHANNEL, &eScrPendReason))
    {
    case SCR_CRS_PEND:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmImmed1Shot_ScrRequest: SCR pending, pend Reason: %d.\n", eScrPendReason);

        /* check pend reason */
        if ( SCR_PR_DIFFERENT_GROUP_RUNNING == eScrPendReason )
        {
            /* send a reject event to the SM - will not scan if not in the correct group */
            pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
            genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        }
        /* for other pending reasons wait untill current client finishes */
        break;

    case SCR_CRS_RUN:
        /* send a run event to the SM */
        TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSmImmed1Shot_ScrRequest: SCR acquired.\n");
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_RUN, hScanCncnClient);
        break;

    default:
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmImmed1Shot_ScrRequest: SCR returned unrecognized status: %d.\n", eScrReplyStatus);
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
        /* Send a reject event to recover from this error */
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_REJECT, hScanCncnClient);
        break;
    }
}

/** 
 * \fn     scanCncnSmImmed1Shot_ScrRelease
 * \brief  Release the SCR as one-shot immediate scan
 * 
 * Release the SCR as one-shot immediate scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmImmed1Shot_ScrRelease (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* release the SCR */
    scr_clientComplete (pScanCncnClient->hSCR, SCR_CID_IMMED_SCAN, SCR_RESOURCE_SERVING_CHANNEL);
}

/** 
 * \fn     scanCncnSmImmed1Shot_StartScan 
 * \brief  Request scan start from TWD for one-shot immediate scan
 * 
 * Request scan start from TWD for one-shot immediate scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmImmed1Shot_StartScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TI_BOOL         bPsRequired, bTriggeredScan;
    TI_STATUS       status;

    /* check whether enter PS is required - according to the roaming reason severity */
    bPsRequired = apConn_isPsRequiredBeforeScan (pScanCncnClient->hApConn);
    bTriggeredScan = ((SCAN_TYPE_TRIGGERED_ACTIVE == pScanCncnClient->uScanParams.tOneShotScanParams.scanType) ||
                      (SCAN_TYPE_TRIGGERED_PASSIVE == pScanCncnClient->uScanParams.tOneShotScanParams.scanType) 
                      ? TI_TRUE : TI_FALSE);

    status = TWD_Scan (pScanCncnClient->hTWD, &(pScanCncnClient->uScanParams.tOneShotScanParams),
                       SCAN_RESULT_TAG_IMMEDIATE, !bTriggeredScan, /* Triggered scan cannot be high priority */
                       TI_TRUE, TI_TRUE, (bPsRequired ? POWER_SAVE_ON : POWER_SAVE_KEEP_CURRENT),
                       bPsRequired, NULL, NULL);

    /* call the scan SRV start scan */
    if (TI_OK != status)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmImmed1Shot_StartScan: TWD start scanreturned status %d, quitting immediate scan.\n", status);

        /* mark the return status */
        pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;

        /* could not start scan, send a scan complete event */
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmImmed1Shot_StopScan
 * \brief  Request scan stop from TWD for one-shot immediate scan
 * 
 * Request scan stop from TWD for one-shot immediate scan
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmImmed1Shot_StopScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;
    TI_STATUS       status;

    /* call the TWD stop scan */
    status = TWD_StopScan (pScanCncnClient->hTWD, SCAN_RESULT_TAG_IMMEDIATE, pScanCncnClient->bSendNullDataOnStop, NULL, NULL);

    /* if stop scan operation failed, send a scan complete event to reset the SM */
    if (TI_OK != status)
    {
        TRACE1(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "scanCncnSmImmed1Shot_StopScan: status %d from TWD_StopScan, sending scan complete to SM\n", status);
        genSM_Event (pScanCncnClient->hGenSM, SCAN_CNCN_SM_EVENT_SCAN_COMPLETE, hScanCncnClient);
    }
}

/** 
 * \fn     scanCncnSmImmed1Shot_Recovery
 * \brief  Handles recovery during scan for one-shot immediate scan
 * 
 * Notifies the scan SRV to stop its timer
 * 
 * \param  hScanCncnClient - handle to the specific client object
 * \return None
 */ 
void scanCncnSmImmed1Shot_Recovery (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* Notify scan SRV to stop its timer */
    TWD_StopScanOnFWReset (pScanCncnClient->hTWD);
}

