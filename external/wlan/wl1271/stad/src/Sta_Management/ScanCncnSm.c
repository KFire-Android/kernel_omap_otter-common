/*
 * ScanCncnSm.c
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

/** \file  ScanCncnSm.c
 *  \brief Scan concentrator state machine implementation
 *
 *  \see   ScanCncnSm.h, ScanCncnSmSpecific.c
 */


#define __FILE_ID__  FILE_ID_79
#include "osTIType.h"
#include "GenSM.h"
#include "ScanCncnSm.h"
#include "report.h"

/* state machine action functions */
static void scanCncnSm_RequestScr       (TI_HANDLE hScanCncnClient);
static void scanCncnSm_StartScan        (TI_HANDLE hScanCncnClient);
static void scanCncnSm_StopScan         (TI_HANDLE hScanCncnClient);
static void scanCncnSm_ScanComplete     (TI_HANDLE hScanCncnClient);
static void scanCncnSm_Nop              (TI_HANDLE hScanCncnClient);
static void scanCncnSm_ActionUnexpected (TI_HANDLE hScanCncnClient);
static void scanCncnSm_RejectScan       (TI_HANDLE hScanCncnClient);
static void scanCncnSm_Recovery         (TI_HANDLE hScanCncnClient);


static TGenSM_actionCell tSmMatrix[ SCAN_CNCN_SM_NUMBER_OF_STATES ][ SCAN_CNCN_SM_NUMBER_OF_EVENTS ] = 
    {
        { /* SCAN_CNCN_SM_STATE_IDLE */
            { SCAN_CNCN_SM_STATE_SCR_WAIT, scanCncnSm_RequestScr },         /* SCAN_CNCN_SM_EVENT_START */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ActionUnexpected },       /* SCAN_CNCN_SM_EVENT_RUN */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ActionUnexpected },       /* SCAN_CNCN_SM_EVENT_SCAN_COMPLETE */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ActionUnexpected },       /* SCAN_CNCN_SM_EVENT_STOP */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ActionUnexpected },       /* SCAN_CNCN_SM_EVENT_ABORT */ 
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ActionUnexpected },       /* SCAN_CNCN_SM_EVENT_RECOVERY */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ActionUnexpected }        /* SCAN_CNCN_SM_EVENT_REJECT */
        },
        { /* SCAN_CNCN_SM_STATE_SCR_WAIT */
            { SCAN_CNCN_SM_STATE_SCR_WAIT, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_START */
            { SCAN_CNCN_SM_STATE_SCANNING, scanCncnSm_StartScan },          /* SCAN_CNCN_SM_EVENT_RUN */
            { SCAN_CNCN_SM_STATE_SCR_WAIT, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_SCAN_COMPLETE */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_RejectScan },             /* SCAN_CNCN_SM_EVENT_STOP */
            { SCAN_CNCN_SM_STATE_SCR_WAIT, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_ABORT */ 
            { SCAN_CNCN_SM_STATE_SCR_WAIT, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_RECOVERY */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_RejectScan }              /* SCAN_CNCN_SM_EVENT_REJECT */
        },
        { /* SCAN_CNCN_SM_STATE_SCANNING */
            { SCAN_CNCN_SM_STATE_SCANNING, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_START */
            { SCAN_CNCN_SM_STATE_SCANNING, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_RUN */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ScanComplete },           /* SCAN_CNCN_SM_EVENT_SCAN_COMPLETE */
            { SCAN_CNCN_SM_STATE_STOPPING, scanCncnSm_StopScan },           /* SCAN_CNCN_SM_EVENT_STOP */
            { SCAN_CNCN_SM_STATE_STOPPING, scanCncnSm_StopScan },           /* SCAN_CNCN_SM_EVENT_ABORT */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_Recovery },               /* SCAN_CNCN_SM_EVENT_RECOVERY */
            { SCAN_CNCN_SM_STATE_SCANNING, scanCncnSm_ActionUnexpected }    /* SCAN_CNCN_SM_EVENT_REJECT */
        },
        { /* SCAN_CNCN_SM_STATE_STOPPING */
            { SCAN_CNCN_SM_STATE_STOPPING, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_START */
            { SCAN_CNCN_SM_STATE_STOPPING, scanCncnSm_ActionUnexpected },   /* SCAN_CNCN_SM_EVENT_RUN */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_ScanComplete },           /* SCAN_CNCN_SM_EVENT_SCAN_COMPLETE */
            { SCAN_CNCN_SM_STATE_STOPPING, scanCncnSm_Nop },                /* SCAN_CNCN_SM_EVENT_STOP */
            { SCAN_CNCN_SM_STATE_STOPPING, scanCncnSm_Nop },                /* SCAN_CNCN_SM_EVENT_ABORT */
            { SCAN_CNCN_SM_STATE_IDLE, scanCncnSm_Recovery },               /* SCAN_CNCN_SM_EVENT_RECOVERY */
            { SCAN_CNCN_SM_STATE_STOPPING, scanCncnSm_ActionUnexpected }    /* SCAN_CNCN_SM_EVENT_REJECT */
        }
    };

static TI_INT8*  uStateDescription[] = 
    {
        "IDLE",
        "SCR_WAIT",
        "SCANNING",
        "STOPPING"
    };

static TI_INT8*  uEventDescription[] = 
    {
        "START",
        "RUN",
        "SCAN_COMPLETE",
        "STOP",
        "ABORT",
        "RECOVERY",
        "REJECT",
    };

/** 
 * \fn     scanCncnSm_Create 
 * \brief  Cerates a scan concentrator client object
 * 
 * Cerates a scan concentrator client - allocates object and create a state-machine instance
 * 
 * \param  hOS - handle to the OS object
 * \return Handle to the new scan concentrator client object
 * \sa     scanCncnSm_Init, scanCncnSm_Destroy
 */ 
TI_HANDLE scanCncnSm_Create (TI_HANDLE hOS)
{
    TScanCncnClient *pScanCncnClient;

    /* allocate space for the scan concentartor client object */
    pScanCncnClient = os_memoryAlloc (hOS, sizeof (TScanCncnClient));
    if (NULL == pScanCncnClient)
    {
        WLAN_OS_REPORT (("scanCncnSm_Cretae: not enough space for scan concentrator client object\n"));
        return NULL;
    }

    /* store the OS object handle */
    pScanCncnClient->hOS = hOS;

    /* allocate the state machine object */
    pScanCncnClient->hGenSM = genSM_Create (hOS);
    if (NULL == pScanCncnClient->hGenSM)
    {
        WLAN_OS_REPORT (("scanCncnSm_Cretae: not enough space for scan concentrator client state-machine\n"));
        return NULL;
    }

    /* return the new object */
    return (TI_HANDLE)pScanCncnClient;
}

/** 
 * \fn     scanCncnSm_Init
 * \brief  Initialize a scan concentartor client object
 * 
 * Initialize a scan concentartor client object - store handles and specific SM functions
 * 
 * \note   Some of the values (e.g. scan result CB( are initialized from the main scan concentartor object)
 * \param  hScanCncnClient - handle to the scan concnentrator client object
 * \param  hReport - handle to the report object
 * \param  hTWD - handle to the TWD object
 * \param  hSCR - handle to the SCR object
 * \param  hApConn - handle to the AP connection object
 * \param  hMlme - handle to the MLME object
 * \param  fScrRequest - SM specific SCR request finction
 * \param  fScrRelease - SM specific SCR release finction
 * \param  fStartScan - SM specific scan start finction
 * \param  fStopScan - SM specific scan stop finction
 * \param  fRecovery - SM specific recovery handling function
 * \param  pScanSmName - state machine name
 * \return None
 * \sa     scanCncnSm_Cretae 
 */ 
void scanCncnSm_Init (TI_HANDLE hScanCncnClient, TI_HANDLE hReport, TI_HANDLE hTWD, TI_HANDLE hSCR, 
                      TI_HANDLE hApConn, TI_HANDLE hMlme, TI_HANDLE hScanCncn, TScanPrivateSMFunction fScrRequest, 
                      TScanPrivateSMFunction fScrRelease, TScanPrivateSMFunction fStartScan, 
                      TScanPrivateSMFunction fStopScan, TScanPrivateSMFunction fRecovery, TI_INT8* pScanSmName)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* store handles */
    pScanCncnClient->hReport = hReport;
    pScanCncnClient->hTWD = hTWD;
    pScanCncnClient->hSCR = hSCR;
    pScanCncnClient->hApConn = hApConn;
    pScanCncnClient->hMlme = hMlme;
    pScanCncnClient->hScanCncn = hScanCncn;

    /* store private functions */
    pScanCncnClient->fScrRequest = fScrRequest;
    pScanCncnClient->fScrRelease = fScrRelease;
    pScanCncnClient->fStartScan = fStartScan;
    pScanCncnClient->fStopScan = fStopScan;
    pScanCncnClient->fRecovery = fRecovery;

    /* store SM name */
    pScanCncnClient->pScanSmName = pScanSmName;

    /* initialize the state-machine */
    genSM_Init (pScanCncnClient->hGenSM, hReport);
    genSM_SetDefaults (pScanCncnClient->hGenSM, SCAN_CNCN_SM_NUMBER_OF_STATES, SCAN_CNCN_SM_NUMBER_OF_EVENTS,
                       (TGenSM_matrix)tSmMatrix, SCAN_CNCN_SM_STATE_IDLE, pScanCncnClient->pScanSmName, uStateDescription, 
                       uEventDescription, __FILE_ID__);
}

/** 
 * \fn     scanCncnSm_Destroy 
 * \brief  Destroys a scan concentartor client object
 * 
 * Destroys a scan concentartor client object. destroys the state-machine object and 
 * de-allcoates system resources
 * 
 * \param  hScanCncnClient - handle to the scan concnentrator client object
 * \return None
 * \sa     scanCncnSm_Cretae
 */ 
void scanCncnSm_Destroy (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    /* free the state-machine */
    genSM_Unload (pScanCncnClient->hGenSM);

    /* Free object storage space */
    os_memoryFree (pScanCncnClient->hOS, hScanCncnClient, sizeof (TScanCncnClient));
}

/** 
 * \fn     scanCncnSm_RequestScr 
 * \brief  Scan concentartor SM action function for SCR request
 * 
 * Calls the Sm specific SCR request function
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_RequestScr (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSm_RequestScr: SM  requesting SCR\n");

    /* 
     * just call the specific SCR request function, it will send an event if necessary
     * according to SCR return code by itself
     */
    pScanCncnClient->fScrRequest (hScanCncnClient);
}

/** 
 * \fn     scanCncnSm_StartScan 
 * \brief  Scan concentrator SM action function for starting scan
 * 
 * Register for MLME CB and call the SM specific scan start function 
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_StartScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSm_StartScan: SM  attempting to start scan.\n");

    /* set scan result counter and flag */
    pScanCncnClient->uResultCounter = 0;
    pScanCncnClient->uResultExpectedNumber = 0;
    pScanCncnClient->bScanCompletePending = TI_FALSE;
	pScanCncnClient->bScanRejectedOn2_4 = TI_FALSE;  

    /* call the specific start scan command. It will handle errors by itself */
    pScanCncnClient->fStartScan (hScanCncnClient);
}

/** 
 * \fn     scanCncnSm_StopScan 
 * \brief  Scan concentrator SM action function for stoping scan by outside request
 * 
 * Calls the SM specific stop scan function
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_StopScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSm_StopScan: SM  is attempting to stop scan\n");

    /* call the scan SRV stop scan */
    pScanCncnClient->fStopScan (hScanCncnClient);
}

/** 
 * \fn     scanCncnSm_ScanComplete 
 * \brief  Scan concentrator SM action function for scan complete 
 * 
 * Unregister MLME, release SCR and call client scan complete CB, if not running within a request context
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_ScanComplete (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSm_ScanComplete: SM  received scan complete event\n");

    /* release the SCR */
    pScanCncnClient->fScrRelease (hScanCncnClient);

    /* Call the client scan complete callback */
    if (TI_FALSE == pScanCncnClient->bInRequest)
    {
        pScanCncnClient->tScanResultCB (pScanCncnClient->hScanResultCBObj,
                                        pScanCncnClient->eScanResult, NULL, pScanCncnClient->uSPSScanResult);
    }
}

/** 
 * \fn     scanCncnSm_Nop 
 * \brief  Scan concentrator SM action function for no operation 
 * 
 * Used when no operation is required not due to an error
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_Nop (TI_HANDLE hScanCncnClient)
{
}

/** 
 * \fn     ScanCncnSm_ActionUnexpected 
 * \brief  Scan concentrator SM action function for unexpected events 
 * 
 * Print an error message
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_ActionUnexpected (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_ERROR , "ScanCncnSm_ActionUnexpected: Unexpected event for current state\n");

    /* mark the scan status as failed */
    pScanCncnClient->eScanResult = SCAN_CRS_SCAN_FAILED;
}

/** 
 * \fn     scanCncnSm_RejectScan
 * \brief  Scan concentrator SM action function for rejecting a scan by the SCR
 * 
 * Releases the SCR and calls the client scan complete CB, if not running within a request context
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_RejectScan (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSm_RejectScan: SM  received reject event\n");

     pScanCncnClient->bScanRejectedOn2_4 = TI_TRUE;
    /* release the SCR */
    pScanCncnClient->fScrRelease (hScanCncnClient);

    /* Call the client scan complete CB */
    if (TI_FALSE == pScanCncnClient->bInRequest)
    {
        pScanCncnClient->tScanResultCB (pScanCncnClient->hScanResultCBObj, pScanCncnClient->eScanResult,
                                        NULL, pScanCncnClient->uSPSScanResult); 
    }   
}

/** 
 * \fn     scanCncnSm_Recovery
 * \brief  Scan concentrator SM action function for handling recovery during scan
 * 
 * Calls the SM specific recovery handling function and send a scan complete event. Used to stop timer
 * on one-shot scans
 * 
 * \param  hScanCncnClient - Handle to the scan concentrator client object
 * \return None
 */ 
void scanCncnSm_Recovery (TI_HANDLE hScanCncnClient)
{
    TScanCncnClient *pScanCncnClient = (TScanCncnClient*)hScanCncnClient;

    TRACE0(pScanCncnClient->hReport, REPORT_SEVERITY_INFORMATION , "scanCncnSm_Recovery: SM  received reject event\n");

    /* Call the recovery specific function */
    pScanCncnClient->fRecovery (hScanCncnClient);

    /* Call the client scan complete callback */
    if (TI_FALSE == pScanCncnClient->bInRequest)
    {
        pScanCncnClient->tScanResultCB (pScanCncnClient->hScanResultCBObj,
                                        pScanCncnClient->eScanResult, NULL, pScanCncnClient->uSPSScanResult);
    }
}

