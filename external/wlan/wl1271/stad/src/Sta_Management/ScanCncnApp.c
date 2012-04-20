/*
 * ScanCncnApp.c
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

/** \file  ScanCncnApp.c
 *  \brief Scan concentrator application scan module implementation
 *
 *  \see   ScanCncn.h, ScanCncn.c
 */


#define __FILE_ID__  FILE_ID_77
#include "ScanCncnPrivate.h"
#include "ScanCncn.h"
#include "ScanCncnOsSm.h"
#include "EvHandler.h"
#include "report.h"
#include "GenSM.h"
#include "scanResultTable.h"
#include "sme.h"
#include "smeApi.h"

/** 
 * \fn     scanCncnApp_SetParam
 * \brief  Parses and executes a set param command
 * 
 * Parses and executes a set param command (start/stop one-shot/periodic/OS scan)
 * 
 * \param  hScanCncn - handle to the scan concentrator object
 * \param  pParam - the param to set
 * \return operation status (OK / NOK / PARAM_NOT_SUPPORTED)
 * \sa     scanCncnApp_GetParam 
 */ 
TI_STATUS scanCncnApp_SetParam (TI_HANDLE hScanCncn, paramInfo_t *pParam)
{
    TScanCncn   *pScanCncn = (TScanCncn*)hScanCncn;
    TI_UINT32   uCurrentTimeStamp;

    TRACE1(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnApp_SetParam: recevived request of type 0x%x\n", pParam->paramType);

    switch (pParam->paramType)
    {
    case SCAN_CNCN_START_APP_SCAN:

        /* verify that scan is not currently running */
        if (pScanCncn->eCurrentRunningAppScanClient != SCAN_SCC_NO_CLIENT)
        {
            TRACE1(pScanCncn->hReport, REPORT_SEVERITY_WARNING, "scanCncnApp_SetParam: trying to start app one-shot scan when client %d is currently running!\n", pScanCncn->eCurrentRunningAppScanClient);
            /* Scan was not started successfully, send a scan complete event to the user */
            return TI_NOK;
        }

        /* set one-shot scan as running app scan client */
        pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_APP_ONE_SHOT;

        /* Perform aging process before the scan */
        scanResultTable_PerformAging(pScanCncn->hScanResultTable);

        /* start the scan */
        if (SCAN_CRS_SCAN_RUNNING != 
            scanCncn_Start1ShotScan (hScanCncn, SCAN_SCC_APP_ONE_SHOT, pParam->content.pScanParams))
        {
            /* Scan was not started successfully, mark that no app scan is running */
            pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_NO_CLIENT;
            return TI_NOK;
        }
        break;

    case SCAN_CNCN_STOP_APP_SCAN:
        /* verify that scan is currently running */
        if (pScanCncn->eCurrentRunningAppScanClient != SCAN_SCC_NO_CLIENT)
        {
            scanCncn_StopScan (hScanCncn, SCAN_SCC_APP_ONE_SHOT);
        }
        break;

    case SCAN_CNCN_START_PERIODIC_SCAN:
        /* verify that scan is not currently running */
        if (SCAN_SCC_NO_CLIENT != pScanCncn->eCurrentRunningAppScanClient)
        {
            TRACE1(pScanCncn->hReport, REPORT_SEVERITY_WARNING, "scanCncnApp_SetParam: trying to start app periodic scan when client %d is currently running!\n", pScanCncn->eCurrentRunningAppScanClient);
            /* Scan was not started successfully, send a scan complete event to the user */
            return TI_NOK;
        }

        /* set one-shot scan as running app scan client */
        pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_APP_PERIODIC;

        /* Perform aging process before the scan */
        scanResultTable_PerformAging(pScanCncn->hScanResultTable);

        /* start the scan */
        if (SCAN_CRS_SCAN_RUNNING !=
            scanCncn_StartPeriodicScan (hScanCncn, SCAN_SCC_APP_PERIODIC, pParam->content.pPeriodicScanParams))
        {
            TRACE0(pScanCncn->hReport, REPORT_SEVERITY_CONSOLE , "Scan was not started. Verify scan parametrs or SME mode\n");
            WLAN_OS_REPORT (("Scan was not started. Verify scan parametrs or SME mode\n"));
                             
            /* Scan was not started successfully, mark that no app scan is running */
            pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_NO_CLIENT;
            return TI_NOK;
        }
        break;

    case SCAN_CNCN_STOP_PERIODIC_SCAN:
        /* verify that scan is currently running */
        if (pScanCncn->eCurrentRunningAppScanClient != SCAN_SCC_NO_CLIENT)
        {
            scanCncn_StopPeriodicScan (hScanCncn, SCAN_SCC_APP_PERIODIC);
        }
        break;

    case SCAN_CNCN_BSSID_LIST_SCAN_PARAM:
        /* check if OID scans are enabled in the registry */
        if (0 == pScanCncn->tInitParams.uMinimumDurationBetweenOsScans)
        {
            TRACE0(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnApp_SetParam: received OS scan request when OS scans are disabled, quitting...\n");
            return TI_NOK;
        }

        /* check if the last OID scan didn't start at a shorter duration than the configured minimum */
        uCurrentTimeStamp = os_timeStampMs (pScanCncn->hOS);
        if ( (uCurrentTimeStamp - pScanCncn->uOSScanLastTimeStamp) < 
             (pScanCncn->tInitParams.uMinimumDurationBetweenOsScans * 1000) ) /*converted to ms */
        {
            TRACE3(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnApp_SetParam: last OID scan performed at: %d, now is: %d, min duration is: %d, too early for another scan!\n", pScanCncn->uOSScanLastTimeStamp, uCurrentTimeStamp, pScanCncn->tInitParams.uMinimumDurationBetweenOsScans);
            return TI_NOK;
        }

        /* check that no other scan is currently running */
        if (SCAN_SCC_NO_CLIENT != pScanCncn->eCurrentRunningAppScanClient)
        {
            TRACE1(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnApp_SetParam: received OS scan request when client %d is currently running!\n", pScanCncn->eCurrentRunningAppScanClient);
            return TI_NOK;
        }

        /* set one-shot scan as running app scan client */
        pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_APP_ONE_SHOT;

        /* mark that an OID scan process has started */
        pScanCncn->bOSScanRunning = TI_TRUE;
        pScanCncn->uOSScanLastTimeStamp = uCurrentTimeStamp;
        TRACE0(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnApp_SetParam: starting OID scan process...\n");

		if(0 != pParam->content.pScanParams->desiredSsid.len)
        {
			pScanCncn->tOsScanParams.desiredSsid.len = pParam->content.pScanParams->desiredSsid.len;
            os_memoryCopy(pScanCncn->hOS, pScanCncn->tOsScanParams.desiredSsid.str, pParam->content.pScanParams->desiredSsid.str, pParam->content.pScanParams->desiredSsid.len);
        }
        else
        {
            pScanCncn->tOsScanParams.desiredSsid.len = 0;
            pScanCncn->tOsScanParams.desiredSsid.str[ 0 ] = '\0'; /* broadcast scan */
        }

		pScanCncn->tOsScanParams.scanType = pParam->content.pScanParams->scanType;

        /* and actually start the scan */
        genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_START_SCAN, hScanCncn);

        break;

    case SCAN_CNCN_SET_SRA:
        scanResultTable_SetSraThreshold(pScanCncn->hScanResultTable, pParam->content.uSraThreshold);
        break;

    case SCAN_CNCN_SET_RSSI:
        pScanCncn->tInitParams.nRssiThreshold = pParam->content.nRssiThreshold;
        break;

    default:
        TRACE1(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnApp_SetParam: unrecognized param type :%d\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}

/** 
 * \fn     scanCncnApp_GetParam
 * \brief  Parses and executes a get param command
 * 
 * Parses and executes a get param command (get BSSID list)
 * 
 * \param  hScanCncn - handle to the scan concentrator object
 * \param  pParam - the param to get
 * \return operation status (OK / NOK / PARAM_NOT_SUPPORTED)
 * \sa     scanCncnApp_SetParam
 */ 
TI_STATUS scanCncnApp_GetParam (TI_HANDLE hScanCncn, paramInfo_t *pParam)
{
    TScanCncn   *pScanCncn = (TScanCncn*)hScanCncn;

    TRACE1(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnApp_GetParam: received request of type %d\n", pParam->paramType);

    switch (pParam->paramType)
    {

	case SCAN_CNCN_NUM_BSSID_IN_LIST_PARAM:
        /* retrieve the number of BSSID's in the scan result table*/
		pParam->paramLength = sizeof(TI_UINT32);
		pParam->content.uNumBssidInList = scanResultTable_GetNumOfBSSIDInTheList (pScanCncn->hScanResultTable);
        break;
        
    case SCAN_CNCN_BSSID_LIST_SIZE_PARAM:
        /* retrieves the size to allocate for the app scan result taBle BBSID list (see next code) */
        pParam->paramLength = sizeof(TI_UINT32);
		pParam->content.uBssidListSize = scanResultTable_CalculateBssidListSize (pScanCncn->hScanResultTable, TI_TRUE);
        break;

    case SCAN_CNCN_BSSID_LIST_PARAM:
        /* retrieve the app scan result table */
  		return scanResultTable_GetBssidList (pScanCncn->hScanResultTable, pParam->content.pBssidList, 
                                             &pParam->paramLength, TI_TRUE);

	case SCAN_CNCN_BSSID_RATE_LIST_PARAM:
        /* retrieve supported rates list equivalent to the supported rates list
		 in the scan result table, but is extended to include 11n rates as well*/
		return scanResultTable_GetBssidSupportedRatesList (pScanCncn->hScanResultTable, pParam->content.pRateList,
														   &pParam->paramLength);

    default:
        TRACE1(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncnApp_GetParam: unrecognized param type :%d\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}

/** 
 * \fn     scanCncn_AppScanResultCB
 * \brief  Scan result callback for application scan
 * 
 * Scan result callback for application scan
 * 
 * \param  hScanCncn - handle to the scan concentrator object
 * \param  status - the scan result status (scan complete, result received etc.)
 * \param  frameInfo - a pointer to the structure holding all frame related info (in case a frame was received)
 * \param  SPSStatus - a bitmap indicating on which channels scan was attempted (valid for SPS scan only!)
 * \return None
 */ 
void scanCncn_AppScanResultCB (TI_HANDLE hScanCncn, EScanCncnResultStatus status,
                               TScanFrameInfo* frameInfo, TI_UINT16 SPSStatus)
{
    TScanCncn   *pScanCncn = (TScanCncn*)hScanCncn;
    TI_UINT32	statusData;

    /* Since in Manual Mode the app and the SME use the same table
     * there is no need to forward data to SME */

    switch (status)
    {
    case SCAN_CRS_RECEIVED_FRAME:
        /* Save the result in the app scan result table */
        if (TI_OK != scanResultTable_UpdateEntry (pScanCncn->hScanResultTable, frameInfo->bssId, frameInfo))
		{
	        TRACE0(pScanCncn->hReport, REPORT_SEVERITY_WARNING , "scanCncn_AppScanResultCB, scanResultTable_UpdateEntry() failed\n");
		}
		break;

    case SCAN_CRS_SCAN_COMPLETE_OK:

        TRACE1(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncn_AppScanResultCB, received scan complete with status :%d\n", status);

        /* if OS scan is running */
        if (TI_TRUE == pScanCncn->bOSScanRunning)
        {
            /* send a scan complete event to the OS scan SM. It will stabliza the table when needed */
            genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
        }
        else
        {
            /* move the scan result table to stable state */
            scanResultTable_SetStableState (pScanCncn->hScanResultTable);

            /* mark that no app scan is running */
            pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_NO_CLIENT;
            /*
             * The scan was finished, send a scan complete event to the user
             * (regardless of why the scan was completed)
             */
            statusData = SCAN_STATUS_COMPLETE;	/* Completed status */
			EvHandlerSendEvent (pScanCncn->hEvHandler, IPC_EVENT_SCAN_COMPLETE, (TI_UINT8 *)&statusData, sizeof(TI_UINT32));
        }
        break;

    case SCAN_CRS_SCAN_STOPPED:

        TRACE1(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncn_AppScanResultCB, received scan complete with status :%d\n", status);

        /* if OS scan is running */
        if (TI_TRUE == pScanCncn->bOSScanRunning)
        {
            /* send a scan complete event to the OS scan SM. It will stabliza the table when needed */
            genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
        }
        else
        {
            /* move the scan result table to stable state */
            scanResultTable_SetStableState (pScanCncn->hScanResultTable);

            /* mark that no app scan is running */
            pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_NO_CLIENT;
            /*
             * The scan was finished, send a scan complete event to the user
             * (regardless of why the scan was completed)
             */
            statusData = SCAN_STATUS_STOPPED;	/* Stopped status */
            EvHandlerSendEvent (pScanCncn->hEvHandler, IPC_EVENT_SCAN_COMPLETE, (TI_UINT8 *)&statusData, sizeof(TI_UINT32));
        }
        break;

    case SCAN_CRS_TSF_ERROR:
    case SCAN_CRS_SCAN_RUNNING:
    case SCAN_CRS_SCAN_FAILED:
    case SCAN_CRS_SCAN_ABORTED_HIGHER_PRIORITY:
    case SCAN_CRS_SCAN_ABORTED_FW_RESET:

        TRACE1(pScanCncn->hReport, REPORT_SEVERITY_INFORMATION , "scanCncn_AppScanResultCB, received scan complete with status :%d\n", status);

        /* if OS scan is running */
        if (TI_TRUE == pScanCncn->bOSScanRunning)
        {
            /* send a scan complete event to the OS scan SM. It will stabliza the table when needed */
            genSM_Event (pScanCncn->hOSScanSm, SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE, hScanCncn);
        }
        else
        {
            /* move the scan result table to stable state */
            scanResultTable_SetStableState (pScanCncn->hScanResultTable);

            /* mark that no app scan is running */
            pScanCncn->eCurrentRunningAppScanClient = SCAN_SCC_NO_CLIENT;
            /* 
             * The scan was finished, send a scan complete event to the user
             * (regardless of why the scan was completed)
             */
            statusData = SCAN_STATUS_FAILED;		/* Failed status */
            EvHandlerSendEvent (pScanCncn->hEvHandler, IPC_EVENT_SCAN_COMPLETE, (TI_UINT8 *)&statusData, sizeof(TI_UINT32));
        }
        break;

    case SCAN_CRS_NUM_OF_RES_STATUS:
    default:
        TRACE1(pScanCncn->hReport, REPORT_SEVERITY_ERROR , "scanCncn_AppScanResultCB, received erroneuos scan result with status :%d\n", status);
        break;
    }
}

