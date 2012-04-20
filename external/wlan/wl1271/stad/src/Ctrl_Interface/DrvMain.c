/*
 * DrvMain.c
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


/** \file   DrvMain.c 
 *  \brief  The DrvMain module. Handles driver init, stop and recovery processes.
 *  
 *  \see    DrvMain.h
 */

#define __FILE_ID__  FILE_ID_49
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "context.h"
#include "timer.h"
#include "CmdHndlr.h"
#include "DrvMain.h"
#include "scrApi.h"
#include "EvHandler.h"
#include "connApi.h"
#include "siteMgrApi.h"
#include "sme.h"
#include "SoftGeminiApi.h"
#include "roamingMngrApi.h"
#include "qosMngr_API.h"
#include "TrafficMonitor.h"
#include "PowerMgr_API.h"
#include "EvHandler.h"
#include "apConn.h"
#include "currBss.h"
#include "SwitchChannelApi.h"
#include "ScanCncn.h"
#include "healthMonitor.h"
#include "scanMngrApi.h"
#include "regulatoryDomainApi.h"
#include "measurementMgrApi.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif
#include "TxnQueue.h"
#include "TWDriver.h"
#include "debug.h"
#include "host_platform.h"
#include "StaCap.h"
#include "WlanDrvCommon.h"
#include "DrvMainModules.h"
#include "CmdDispatcher.h"


#define SM_WATCHDOG_TIME_MS     20000  /* SM processes timeout is 20 sec. */

#define SDIO_CONNECT_THRESHOLD  8

/* This is used to prevent endless recovery loops */
#define MAX_NUM_OF_RECOVERY_TRIGGERS 5

/* Handle failure status from the SM callbacks by triggering the SM with FAILURE event */
#define HANDLE_CALLBACKS_FAILURE_STATUS(hDrvMain, eStatus)      \
            if (eStatus != TI_OK) { drvMain_SmEvent (hDrvMain, SM_EVENT_FAILURE);  return; }
    
/* The DrvMain SM states */
typedef enum
{
    /*  0 */ SM_STATE_IDLE,
    /*  1 */ SM_STATE_WAIT_INI_FILE,
    /*  2 */ SM_STATE_WAIT_NVS_FILE,
    /*  3 */ SM_STATE_HW_INIT,
    /*  4 */ SM_STATE_DOWNLOAD_FW_FILE,
    /*  5 */ SM_STATE_WAIT_FW_FILE,
    /*  6 */ SM_STATE_FW_INIT,
    /*  7 */ SM_STATE_FW_CONFIG,
    /*  8 */ SM_STATE_OPERATIONAL,  
    /*  9 */ SM_STATE_DISCONNECTING,
    /* 10 */ SM_STATE_STOPPING,     
    /* 11 */ SM_STATE_STOPPED,      
    /* 12 */ SM_STATE_STOPPING_ON_FAIL,       
    /* 13 */ SM_STATE_FAILED        

} ESmState;

/* The DrvMain SM events */
typedef enum
{
    /*  0 */ SM_EVENT_START,
    /*  1 */ SM_EVENT_INI_FILE_READY,
    /*  2 */ SM_EVENT_NVS_FILE_READY,
    /*  3 */ SM_EVENT_HW_INIT_COMPLETE,
    /*  4 */ SM_EVENT_FW_FILE_READY,
    /*  5 */ SM_EVENT_FW_INIT_COMPLETE,
    /*  6 */ SM_EVENT_FW_CONFIG_COMPLETE,
    /*  7 */ SM_EVENT_STOP,          
    /*  8 */ SM_EVENT_RECOVERY,      
    /*  9 */ SM_EVENT_DISCONNECTED,  
    /* 10 */ SM_EVENT_STOP_COMPLETE, 
    /* 11 */ SM_EVENT_FAILURE

} ESmEvent;

/* The module's object */
typedef struct
{
    TStadHandlesList  tStadHandles; /* All STAD modules handles (distributed in driver init process) */
	TI_BOOL           bRecovery;    /* Indicates if we are during recovery process */
	TI_UINT32         uNumOfRecoveryAttempts;    /* Indicates if we are during recovery process */
	ESmState          eSmState;     /* The DrvMain SM state. */
	ESmEvent          ePendingEvent;/* A pending event issued when the SM is busy */
    TI_UINT32         uPendingEventsCount; /* Counts the number of events pending for SM execution */
    TFileInfo         tFileInfo;    /* Information of last file retrieved by os_GetFile() */
    TI_UINT32         uContextId;   /* ID allocated to this module on registration to context module */
    EActionType       eAction;      /* The last action (start/stop) inserted to the driver */
    void             *hSignalObj;   /* The signal object used for waiting for action completion */
    TBusDrvCfg        tBusDrvCfg;   /* A union (struc per each supported bus type) for the bus driver configuration */
    TI_UINT32         uRxDmaBufLen; /* The bus driver Rx DMA buffer length (needed as a limit for Rx aggregation length) */
    TI_UINT32         uTxDmaBufLen; /* The bus driver Tx DMA buffer length (needed as a limit for Tx aggregation length) */

} TDrvMain;


static void drvMain_Init (TI_HANDLE hDrvMain);
static void drvMain_InitHwCb (TI_HANDLE hDrvMain, TI_STATUS eStatus);
static void drvMain_InitFwCb (TI_HANDLE hDrvMain, TI_STATUS eStatus);
static void drvMain_ConfigFwCb (TI_HANDLE hDrvMain, TI_STATUS eStatus);
static void drvMain_TwdStopCb (TI_HANDLE hDrvMain, TI_STATUS eStatus);
static void drvMain_InitFailCb (TI_HANDLE hDrvMain, TI_STATUS eStatus);
static void drvMain_InitLocals (TDrvMain *pDrvMain);
/* static void drvMain_SmWatchdogTimeout (TI_HANDLE hDrvMain); */
static void drvMain_SmEvent (TI_HANDLE hDrvMain, ESmEvent eEvent);
static void drvMain_Sm (TI_HANDLE hDrvMain, ESmEvent eEvent);

/* External functions prototypes */

/** \brief WLAN Driver I/F Get file
 * 
 * \param  hOs         - OS module object handle
 * \param  pFileInfo   - Pointer to output file information
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * This function provides access to a requested init file:
 * It provides the requested file information and call the requester callback.
 * Note that in Linux the files were previously loaded to driver memory by the loader
 * 
 * \sa
 */ 
extern int  wlanDrvIf_GetFile (TI_HANDLE hOs, TFileInfo *pFileInfo);
/** \brief WLAN Driver I/F Update Driver State
 * 
 * \param  hOs          - OS module object handle
 * \param  eDriverState - New Driver State
 * \return void
 * 
 * \par Description
 * This function Update the driver state (Idle | Running | Stopped |Failed):
 * 
 * \sa
 */
extern void wlanDrvIf_UpdateDriverState (TI_HANDLE hOs, EDriverSteadyState eDriverState);
/** \brief WLAN Driver I/F Set MAC Address
 * 
 * \param  hOs          - OS module object handle
 * \param  pMacAddr     - Pointer to MAC address to set
 * \return void
 * 
 * \par Description
 * This function Update the driver MAC address by copy it to the network interface structure
 * 
 * \sa
 */
extern void wlanDrvIf_SetMacAddress (TI_HANDLE hOs, TI_UINT8 *pMacAddr);
/** \brief OS Init Table INI File
 * 
 * \param  hOs              - OS module object handle
 * \param  InitTable        - Pointer to initialization table
 * \param  file_buf         - Pointer to Input buffer from INI file
 * \param  file_length      - Length of input buffer from INI file
 * \return void
 * 
 * \par Description
 * This function perform Initializing of init table accrding to data from INI file and driver defaults
 * 
 * \sa
 */
extern int  osInitTable_IniFile (TI_HANDLE hOs, TInitTable *InitTable, char *file_buf, int file_length);



/* 
 * \fn     drvMain_Create
 * \brief  Create the driver modules
 * 
 * Create all STAD and TWD modules.
 * Then call all modules init functions which initializes their handles and variables.
 * 
 * \note   
 * \param  hOs          - Handle to the Os Abstraction Layer                           
 * \param  pDrvMainHndl - Pointer for returning the DrvMain handle.                      
 * \param  pCmdHndlr    - Pointer for returning the CmdHndlr handle.                      
 * \param  pContext     - Pointer for returning the Context handle.                      
 * \param  pTxDataQ     - Pointer for returning the TxDataQ handle.                      
 * \param  pTxMgmtQ     - Pointer for returning the TxMgmtQ handle.                      
 * \param  pTxCtrl      - Pointer for returning the TxCtrl handle.                      
 * \param  pTwd         - Pointer for returning the TWD handle.                      
 * \param  pEvHandler   - Pointer for returning the EvHndler handle.                      
 * \return Handle to the DrvMain module (NULL if failed) 
 * \sa     
 */ 
TI_STATUS drvMain_Create (TI_HANDLE  hOs, 
                          TI_HANDLE *pDrvMainHndl, 
                          TI_HANDLE *pCmdHndlr, 
                          TI_HANDLE *pContext, 
                          TI_HANDLE *pTxDataQ, 
                          TI_HANDLE *pTxMgmtQ,
                          TI_HANDLE *pTxCtrl,
                          TI_HANDLE *pTwd,
                          TI_HANDLE *pEvHandler,
		                  TI_HANDLE *pCmdDispatch,
                          TI_HANDLE *pReport)
{
    /* Create the DrvMain module object. */
    TDrvMain *pDrvMain = (TDrvMain *) os_memoryAlloc (hOs, sizeof(TDrvMain));

    if (pDrvMain == NULL) 
    {
        return TI_NOK;
    }

    os_memoryZero (hOs, (void *)pDrvMain, sizeof(TDrvMain));

    pDrvMain->tStadHandles.hDrvMain = (TI_HANDLE)pDrvMain;
    pDrvMain->tStadHandles.hOs = hOs;

/* 
 *   Create all driver modules
 *   =========================
 */

    pDrvMain->tStadHandles.hContext = context_Create (hOs);
    if (pDrvMain->tStadHandles.hContext == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTimer = tmr_Create (hOs);
    if (pDrvMain->tStadHandles.hTimer == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hSCR = scr_create (hOs);
    if (pDrvMain->tStadHandles.hSCR == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTxnQ = txnQ_Create (hOs);
    if (pDrvMain->tStadHandles.hTxnQ == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hEvHandler = EvHandler_Create (hOs);
    if (pDrvMain->tStadHandles.hEvHandler == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hReport = report_Create (hOs);
    if (pDrvMain->tStadHandles.hReport == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hConn = conn_create (hOs);
    if (pDrvMain->tStadHandles.hConn == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hScanCncn = scanCncn_Create (hOs);
    if (pDrvMain->tStadHandles.hScanCncn == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hSme = sme_Create (hOs);
    if (pDrvMain->tStadHandles.hSme == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hSiteMgr = siteMgr_create (hOs);
    if (pDrvMain->tStadHandles.hSiteMgr == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hMlmeSm = mlme_create (hOs);
    if (pDrvMain->tStadHandles.hMlmeSm == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hAuth = auth_create (hOs);
    if (pDrvMain->tStadHandles.hAuth == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hAssoc = assoc_create (hOs);
    if (pDrvMain->tStadHandles.hAssoc == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hRxData = rxData_create (hOs);
    if (pDrvMain->tStadHandles.hRxData == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTxCtrl = txCtrl_Create (hOs);
    if (pDrvMain->tStadHandles.hTxCtrl == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTxDataQ = txDataQ_Create(hOs);
    if (pDrvMain->tStadHandles.hTxDataQ == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTxMgmtQ = txMgmtQ_Create(hOs);
    if (pDrvMain->tStadHandles.hTxMgmtQ == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTxPort = txPort_create (hOs);
    if (pDrvMain->tStadHandles.hTxPort == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hCtrlData = ctrlData_create (hOs);
    if (pDrvMain->tStadHandles.hCtrlData == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTrafficMon = TrafficMonitor_create (hOs);
    if (pDrvMain->tStadHandles.hTrafficMon == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hRsn = rsn_create (hOs);
    if (pDrvMain->tStadHandles.hRsn == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hRegulatoryDomain = regulatoryDomain_create (hOs);
    if (pDrvMain->tStadHandles.hRegulatoryDomain == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hMeasurementMgr = measurementMgr_create (hOs);
    if (pDrvMain->tStadHandles.hMeasurementMgr == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hSoftGemini = SoftGemini_create (hOs);
    if (pDrvMain->tStadHandles.hSoftGemini == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

#ifdef XCC_MODULE_INCLUDED
    pDrvMain->tStadHandles.hXCCMngr = XCCMngr_create (hOs);
    if (pDrvMain->tStadHandles.hXCCMngr == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }
#else
    pDrvMain->tStadHandles.hXCCMngr = NULL;
#endif

    pDrvMain->tStadHandles.hRoamingMngr = roamingMngr_create (hOs);
    if (pDrvMain->tStadHandles.hRoamingMngr == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hAPConnection = apConn_create (hOs);
    if (pDrvMain->tStadHandles.hAPConnection == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hCurrBss = currBSS_create (hOs);
    if (pDrvMain->tStadHandles.hCurrBss == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hQosMngr = qosMngr_create (hOs);
    if (pDrvMain->tStadHandles.hQosMngr == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hPowerMgr = PowerMgr_create (hOs);
    if (pDrvMain->tStadHandles.hPowerMgr == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hSwitchChannel = switchChannel_create (hOs);
    if (pDrvMain->tStadHandles.hSwitchChannel == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hScanMngr = scanMngr_create (hOs);
    if (NULL == pDrvMain->tStadHandles.hScanMngr)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hHealthMonitor = healthMonitor_create (hOs);
    if (NULL == pDrvMain->tStadHandles.hHealthMonitor)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hTWD = TWD_Create (hOs);
    if (pDrvMain->tStadHandles.hTWD == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hCmdHndlr = cmdHndlr_Create (hOs, pDrvMain->tStadHandles.hEvHandler);
    if (pDrvMain->tStadHandles.hCmdHndlr == NULL) 
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hCmdDispatch = cmdDispatch_Create (hOs);
    if (pDrvMain->tStadHandles.hCmdDispatch == NULL) 
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    pDrvMain->tStadHandles.hStaCap = StaCap_Create (hOs);
    if (pDrvMain->tStadHandles.hStaCap == NULL)
    {
        drvMain_Destroy (pDrvMain);
        return TI_NOK;
    }

    /* Bind all modules handles */
    drvMain_Init ((TI_HANDLE)pDrvMain);


    /* Provide required handles to the OAL */
    *pDrvMainHndl = (TI_HANDLE)pDrvMain;
    *pCmdHndlr    = pDrvMain->tStadHandles.hCmdHndlr;
    *pContext     = pDrvMain->tStadHandles.hContext;
    *pTxDataQ     = pDrvMain->tStadHandles.hTxDataQ;
    *pTxMgmtQ     = pDrvMain->tStadHandles.hTxMgmtQ;
    *pTxCtrl      = pDrvMain->tStadHandles.hTxCtrl;
    *pTwd         = pDrvMain->tStadHandles.hTWD;
    *pEvHandler   = pDrvMain->tStadHandles.hEvHandler;
    *pReport      = pDrvMain->tStadHandles.hReport;
    *pCmdDispatch = pDrvMain->tStadHandles.hCmdDispatch;

    WLAN_INIT_REPORT (("drvMain_Create: success\n"));

    return TI_OK;
}

/* 
 * \fn     drvMain_Destroy
 * \brief  Destroy driver
 * 
 * Destroy all STAD and TWD modules and resources.
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \return TI_OK if succeeded, TI_NOK if failed. 
 * \sa     drvMain_Create
 */ 
TI_STATUS drvMain_Destroy (TI_HANDLE  hDrvMain)
{
    TDrvMain *pDrvMain = (TDrvMain *)hDrvMain;

    hPlatform_Wlan_Hardware_DeInit ();

    if (pDrvMain == NULL) 
    {
        return TI_NOK;
    }

    if (pDrvMain->tStadHandles.hScanMngr != NULL)
    {
        scanMngr_unload (pDrvMain->tStadHandles.hScanMngr);
    }

    if (pDrvMain->tStadHandles.hSiteMgr != NULL)
    {
        siteMgr_unLoad (pDrvMain->tStadHandles.hSiteMgr);
    }

    if (pDrvMain->tStadHandles.hSme != NULL)
    {
        sme_Destroy (pDrvMain->tStadHandles.hSme);
    }

    if (pDrvMain->tStadHandles.hConn != NULL)
    {
        conn_unLoad (pDrvMain->tStadHandles.hConn);
    }

    if (pDrvMain->tStadHandles.hTWD != NULL)
    {
        TWD_Destroy (pDrvMain->tStadHandles.hTWD);
    }

    if (pDrvMain->tStadHandles.hScanCncn != NULL)
    {
        scanCncn_Destroy (pDrvMain->tStadHandles.hScanCncn);
    }

    if (pDrvMain->tStadHandles.hTrafficMon != NULL)
    {
        TrafficMonitor_Destroy (pDrvMain->tStadHandles.hTrafficMon);
    }

    if (pDrvMain->tStadHandles.hCtrlData != NULL)
    {
        ctrlData_unLoad (pDrvMain->tStadHandles.hCtrlData);
    }

    if (pDrvMain->tStadHandles.hTxCtrl != NULL)
    {
        txCtrl_Unload (pDrvMain->tStadHandles.hTxCtrl);
    }

    if (pDrvMain->tStadHandles.hTxDataQ != NULL)
    {
        txDataQ_Destroy (pDrvMain->tStadHandles.hTxDataQ);
    }

    if (pDrvMain->tStadHandles.hTxMgmtQ != NULL)
    {
        txMgmtQ_Destroy (pDrvMain->tStadHandles.hTxMgmtQ);
    }

    if (pDrvMain->tStadHandles.hTxPort != NULL)
    {
        txPort_unLoad (pDrvMain->tStadHandles.hTxPort);
    }

    if (pDrvMain->tStadHandles.hRxData != NULL)
    {
        rxData_unLoad (pDrvMain->tStadHandles.hRxData);
    }

    if (pDrvMain->tStadHandles.hAssoc != NULL)
    {
        assoc_unload (pDrvMain->tStadHandles.hAssoc);
    }

    if (pDrvMain->tStadHandles.hAuth != NULL)
    {
        auth_unload (pDrvMain->tStadHandles.hAuth);
    }

    if (pDrvMain->tStadHandles.hMlmeSm != NULL)
    {
        mlme_unload (pDrvMain->tStadHandles.hMlmeSm);
    }

    if (pDrvMain->tStadHandles.hSCR != NULL)
    {
        scr_release (pDrvMain->tStadHandles.hSCR);
    }


    if (pDrvMain->tStadHandles.hRsn != NULL)
    {
        rsn_unload (pDrvMain->tStadHandles.hRsn);
    }

    if (pDrvMain->tStadHandles.hRegulatoryDomain != NULL)
    {
        regulatoryDomain_destroy (pDrvMain->tStadHandles.hRegulatoryDomain);
    }

    if (pDrvMain->tStadHandles.hMeasurementMgr != NULL)
    {
        measurementMgr_destroy (pDrvMain->tStadHandles.hMeasurementMgr);
    }

    if (pDrvMain->tStadHandles.hSoftGemini != NULL)
    {
        SoftGemini_destroy (pDrvMain->tStadHandles.hSoftGemini);
    }

#ifdef XCC_MODULE_INCLUDED
    if (pDrvMain->tStadHandles.hXCCMngr != NULL)
    {
        XCCMngr_unload (pDrvMain->tStadHandles.hXCCMngr);
    }
#endif

    if (pDrvMain->tStadHandles.hRoamingMngr != NULL)
    {
        roamingMngr_unload (pDrvMain->tStadHandles.hRoamingMngr);
    }

    if (pDrvMain->tStadHandles.hQosMngr != NULL)
    {
        qosMngr_destroy (pDrvMain->tStadHandles.hQosMngr);
    }

    if (pDrvMain->tStadHandles.hPowerMgr != NULL)
    {
        PowerMgr_destroy (pDrvMain->tStadHandles.hPowerMgr);
    }

    if (pDrvMain->tStadHandles.hAPConnection != NULL)
    {
        apConn_unload (pDrvMain->tStadHandles.hAPConnection);
    }

    if (pDrvMain->tStadHandles.hCurrBss != NULL)
    {
        currBSS_unload (pDrvMain->tStadHandles.hCurrBss);
    }

    if (pDrvMain->tStadHandles.hSwitchChannel != NULL)
    {
        switchChannel_unload (pDrvMain->tStadHandles.hSwitchChannel);
    }

    if (pDrvMain->tStadHandles.hHealthMonitor != NULL)
    {
        healthMonitor_unload (pDrvMain->tStadHandles.hHealthMonitor);
    }

    if (pDrvMain->tStadHandles.hCmdHndlr && pDrvMain->tStadHandles.hEvHandler)
    {
        cmdHndlr_Destroy (pDrvMain->tStadHandles.hCmdHndlr, pDrvMain->tStadHandles.hEvHandler);
    }

    if (pDrvMain->tStadHandles.hEvHandler != NULL)
    {
         EvHandlerUnload (pDrvMain->tStadHandles.hEvHandler);
    }

    if (pDrvMain->tStadHandles.hCmdDispatch) 
    {
        cmdDispatch_Destroy (pDrvMain->tStadHandles.hCmdDispatch);
    }

    if (pDrvMain->tStadHandles.hTxnQ != NULL)
    {
        txnQ_Destroy (pDrvMain->tStadHandles.hTxnQ);
    }
    /* Note: The Timer module must be destroyed last, so all created timers are already destroyed!! */
    if (pDrvMain->tStadHandles.hTimer != NULL)
    {
        tmr_Destroy (pDrvMain->tStadHandles.hTimer);
    }

    /* Note: Moved after timers for locks */
    if (pDrvMain->tStadHandles.hContext != NULL)
    {
        context_Destroy (pDrvMain->tStadHandles.hContext);
    }

    if (pDrvMain->tStadHandles.hStaCap != NULL)
    {
        StaCap_Destroy (pDrvMain->tStadHandles.hStaCap);
    }

    if (pDrvMain->tStadHandles.hReport != NULL)
    {
        report_Unload (pDrvMain->tStadHandles.hReport);
    }

    /* Destroy the DrvMain object */
    os_memoryFree (pDrvMain->tStadHandles.hOs, hDrvMain, sizeof(TDrvMain));

    return TI_OK;
}

void drvMain_SmeStop (TI_HANDLE hDrvMain)
{
    drvMain_SmEvent (hDrvMain, SM_EVENT_DISCONNECTED);
}


/* 
 * \fn     drvMain_Init
 * \brief  Init driver modules
 * 
 * Called from OS context following the driver creation.
 * Calls all STAD and TWD modules Init functions, which are saving other modules handles,
 *     registering to other modules and initializing their variables.
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \return void 
 * \sa     drvMain_Create
 */ 
static void drvMain_Init (TI_HANDLE hDrvMain)
{
    TDrvMain    *pDrvMain = (TDrvMain *) hDrvMain;
    TStadHandlesList *pModules = &pDrvMain->tStadHandles; /* The STAD modules handles list */

    /* 
     *  Init all modules handles, variables and registries 
     */
    context_Init (pModules->hContext, pModules->hOs, pModules->hReport);
    tmr_Init (pModules->hTimer, pModules->hOs, pModules->hReport, pModules->hContext);
    txnQ_Init (pModules->hTxnQ, pModules->hOs, pModules->hReport, pModules->hContext);
    scr_init (pModules);
    conn_init (pModules);
    ctrlData_init (pModules,
                 #ifdef XCC_MODULE_INCLUDED
                   XCCMngr_LinkTestRetriesUpdate, pModules->hXCCMngr);
                 #else
                   NULL, NULL);
                 #endif                 
    siteMgr_init (pModules);
    regulatoryDomain_init (pModules);
    scanCncn_Init (pModules);
    auth_init (pModules);
    mlme_init (pModules);    
    assoc_init (pModules);
    rxData_init (pModules);
    txCtrl_Init (pModules);
    txDataQ_Init (pModules);
    txMgmtQ_Init (pModules);
    txPort_init (pModules);
    TrafficMonitor_Init (pModules, 1000 /* pInitTable->trafficMonitorMinIntervalPercentage */);
    sme_Init (pModules);
    rsn_init (pModules);
    measurementMgr_init (pModules);
#ifdef XCC_MODULE_INCLUDED
    XCCMngr_init (pModules);
#endif
    scanMngr_init (pModules);
    currBSS_init (pModules); 
    apConn_init (pModules);
    roamingMngr_init (pModules);
    qosMngr_init (pModules);
    switchChannel_init (pModules);
    healthMonitor_init (pModules);
    PowerMgr_init (pModules);
    SoftGemini_init (pModules);                                                                                
    cmdDispatch_Init (pModules);                                                                                
    StaCap_Init (pModules);                                                                 
    cmdHndlr_Init (pModules);

    /* Init TWD component (handles, variables and registries) and provide callbacks for next steps */
    TWD_Init (pModules->hTWD,
              pModules->hReport,
              pModules->hDrvMain,
              pModules->hTimer,
              pModules->hContext,
              pModules->hTxnQ,
              (TTwdCallback)drvMain_InitHwCb,
              (TTwdCallback)drvMain_InitFwCb,
              (TTwdCallback)drvMain_ConfigFwCb,
              (TTwdCallback)drvMain_TwdStopCb,
              (TTwdCallback)drvMain_InitFailCb);

    /* Init DrvMain module local variables */
    drvMain_InitLocals (pDrvMain);
}


/* 
 * \fn     drvMain_SetDefaults
 * \brief  Set driver default configuration
 * 
 * Configure all STAD and TWD modules with their default settings from the ini-file.
 * Timers creation is also done at this stage.
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \param  pBuf     - The ini-file data.
 * \param  uLength  - The ini-file length.
 * \return TI_OK if succeeded, TI_NOK if failed. 
 * \sa     drvMain_Init
 */ 
static TI_STATUS drvMain_SetDefaults (TI_HANDLE hDrvMain, TI_UINT8 *pBuf, TI_UINT32 uLength)
{
    TDrvMain   *pDrvMain = (TDrvMain *) hDrvMain;
    TInitTable *pInitTable;
    TI_STATUS   eStatus;

    pInitTable = os_memoryAlloc (pDrvMain->tStadHandles.hOs, sizeof(TInitTable));

    /* Parse defaults */
    eStatus = osInitTable_IniFile (pDrvMain->tStadHandles.hOs, pInitTable, (char*)pBuf, (int)uLength);

    /*
     *  Configure modules with their default settings
     */
    report_SetDefaults (pDrvMain->tStadHandles.hReport, &pInitTable->tReport);
    context_SetDefaults (pDrvMain->tStadHandles.hContext, &pInitTable->tContextInitParams);
    TWD_SetDefaults (pDrvMain->tStadHandles.hTWD, &pInitTable->twdInitParams);
    conn_SetDefaults (pDrvMain->tStadHandles.hConn, &pInitTable->connInitParams);
    ctrlData_SetDefaults (pDrvMain->tStadHandles.hCtrlData, &pInitTable->ctrlDataInitParams);
    regulatoryDomain_SetDefaults (pDrvMain->tStadHandles.hRegulatoryDomain, &pInitTable->regulatoryDomainInitParams);
    scanCncn_SetDefaults (pDrvMain->tStadHandles.hScanCncn, &pInitTable->tScanCncnInitParams);
    auth_SetDefaults (pDrvMain->tStadHandles.hAuth, &pInitTable->authInitParams);
    assoc_SetDefaults (pDrvMain->tStadHandles.hAssoc, &pInitTable->assocInitParams);
    rxData_SetDefaults (pDrvMain->tStadHandles.hRxData, &pInitTable->rxDataInitParams);
    sme_SetDefaults (pDrvMain->tStadHandles.hSme, &pInitTable->tSmeModifiedInitParams, &pInitTable->tSmeInitParams);
    rsn_SetDefaults (pDrvMain->tStadHandles.hRsn, &pInitTable->rsnInitParams);
    measurementMgr_SetDefaults (pDrvMain->tStadHandles.hMeasurementMgr, &pInitTable->measurementInitParams);
#ifdef XCC_MODULE_INCLUDED
    XCCMngr_SetDefaults (pDrvMain->tStadHandles.hXCCMngr, &pInitTable->XCCMngrParams);
#endif /*XCC_MODULE_INCLUDED*/
    apConn_SetDefaults (pDrvMain->tStadHandles.hAPConnection, &pInitTable->apConnParams);
    qosMngr_SetDefaults (pDrvMain->tStadHandles.hQosMngr, &pInitTable->qosMngrInitParams);
    switchChannel_SetDefaults (pDrvMain->tStadHandles.hSwitchChannel, &pInitTable->SwitchChannelInitParams);
    healthMonitor_SetDefaults (pDrvMain->tStadHandles.hHealthMonitor, &pInitTable->healthMonitorInitParams);
    PowerMgr_SetDefaults (pDrvMain->tStadHandles.hPowerMgr, &pInitTable->PowerMgrInitParams);
    SoftGemini_SetDefaults (pDrvMain->tStadHandles.hSoftGemini, &pInitTable->SoftGeminiInitParams);
    txDataQ_SetDefaults (pDrvMain->tStadHandles.hTxDataQ, &pInitTable->txDataInitParams);
    txCtrl_SetDefaults (pDrvMain->tStadHandles.hTxCtrl, &pInitTable->txDataInitParams);
    currBSS_SetDefaults (pDrvMain->tStadHandles.hCurrBss, &pInitTable->tCurrBssInitParams);
    mlme_SetDefaults (pDrvMain->tStadHandles.hMlmeSm, &pInitTable->tMlmeInitParams);

    scanMngr_SetDefaults(pDrvMain->tStadHandles.hScanMngr, &pInitTable->tRoamScanMngrInitParams);
    roamingMngr_setDefaults(pDrvMain->tStadHandles.hRoamingMngr, &pInitTable->tRoamScanMngrInitParams);

    /* Note: The siteMgr_SetDefaults includes many settings that relate to other modules so keep it last!! */ 
    siteMgr_SetDefaults (pDrvMain->tStadHandles.hSiteMgr, &pInitTable->siteMgrInitParams);

    /* Set DrvMain local defaults */
    pDrvMain->tBusDrvCfg.tSdioCfg.uBlkSizeShift         = pInitTable->tDrvMainParams.uSdioBlkSizeShift;
    pDrvMain->tBusDrvCfg.tSdioCfg.uBusDrvThreadPriority = pInitTable->tDrvMainParams.uBusDrvThreadPriority;
    os_SetDrvThreadPriority (pDrvMain->tStadHandles.hOs, pInitTable->tDrvMainParams.uWlanDrvThreadPriority);

    /* Release the init table memory */
    os_memoryFree (pDrvMain->tStadHandles.hOs, pInitTable, sizeof(TInitTable));

    return eStatus;
}
 

/* 
 * \fn     drvMain_xxx...Cb
 * \brief  Callback functions for the init/stop stages completion
 *
 * The following callback functions are called from other modules (most from TWD)
 *     when the current init/stop step is completed.
 * Note that the callbacks are called anyway, either in the original context (if completed), or
 *     in another context if pending. 
 * The first case (same context) may lead to a recursion of the SM, so a special handling is added
 *     to the SM to prevent recursion (see drvMain_Sm).
 * 
 * drvMain_InitHwCb   - HW init completion callback
 * drvMain_InitFwCb   - FW init (mainly download) completion callback
 * drvMain_ConfigFwCb - FW configuration completion callback
 * drvMain_TwdStopCb  - TWD stopping completion callback
 * drvMain_InitFailCb - FW init faulty completion callback
 * drvMain_SmeStopCb  - SME stopping completion callback
 * drvMain_GetFileCb  - Getting-file completion callback
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \param  eStatus  - The process result (TI_OK if succeeded, TI_NOK if failed)
 * \return void 
 * \sa     drvMain_Create
 */ 
static void drvMain_InitHwCb (TI_HANDLE hDrvMain, TI_STATUS eStatus)
{
    HANDLE_CALLBACKS_FAILURE_STATUS(hDrvMain, eStatus);
    drvMain_SmEvent (hDrvMain, SM_EVENT_HW_INIT_COMPLETE);
}

static void drvMain_InitFwCb (TI_HANDLE hDrvMain, TI_STATUS eStatus)
{
    HANDLE_CALLBACKS_FAILURE_STATUS(hDrvMain, eStatus);
    drvMain_SmEvent (hDrvMain, SM_EVENT_FW_INIT_COMPLETE);
}

static void drvMain_ConfigFwCb (TI_HANDLE hDrvMain, TI_STATUS eStatus)
{
    HANDLE_CALLBACKS_FAILURE_STATUS(hDrvMain, eStatus);
    drvMain_SmEvent (hDrvMain, SM_EVENT_FW_CONFIG_COMPLETE);
}

static void drvMain_TwdStopCb (TI_HANDLE hDrvMain, TI_STATUS eStatus)
{
    HANDLE_CALLBACKS_FAILURE_STATUS(hDrvMain, eStatus);
    drvMain_SmEvent (hDrvMain, SM_EVENT_STOP_COMPLETE);
}

static void drvMain_InitFailCb (TI_HANDLE hDrvMain, TI_STATUS eStatus)
{
    drvMain_SmEvent (hDrvMain, SM_EVENT_FAILURE);
    /* 
     * Note that this call will pass the SM to the FAILED state, since this event 
     *     is not handled by any state.
     */
}

static void drvMain_InvokeAction (TI_HANDLE hDrvMain)
{
    TDrvMain *pDrvMain = (TDrvMain *)hDrvMain;

    switch (pDrvMain->eAction)
    {
    case ACTION_TYPE_START:
        drvMain_SmEvent (hDrvMain, SM_EVENT_START);
        break;
    case ACTION_TYPE_STOP:
        drvMain_SmEvent (hDrvMain, SM_EVENT_STOP);
        break;
        default:    
            TRACE1(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "drvMain_InvokeAction(): Action=%d\n", pDrvMain->eAction);
    }
}

static void drvMain_GetFileCb (TI_HANDLE hDrvMain)
{
    TDrvMain *pDrvMain = (TDrvMain *)hDrvMain;
    ESmEvent  eSmEvent;

    switch (pDrvMain->tFileInfo.eFileType)
    {
        case FILE_TYPE_INI:     eSmEvent = SM_EVENT_INI_FILE_READY;    break;
        case FILE_TYPE_NVS:     eSmEvent = SM_EVENT_NVS_FILE_READY;    break;
        case FILE_TYPE_FW:      eSmEvent = SM_EVENT_FW_FILE_READY;     break;
        case FILE_TYPE_FW_NEXT: eSmEvent = SM_EVENT_FW_FILE_READY;     break;
        default:    
            TRACE1(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "drvMain_GetFileCb(): Unknown eFileType=%d\n", pDrvMain->tFileInfo.eFileType);
            return;
    }
    drvMain_SmEvent (hDrvMain, eSmEvent);
}


/*
 * \fn     drvMain_InitLocals
 * \brief  Init DrvMain module
 * 
 * Init the DrvMain variables, register to other modules and set device power to off.
 * 
 * \note   
 * \param  pDrvMain - The DrvMain object
 * \return void 
 * \sa     drvMain_Init
 */ 
static void drvMain_InitLocals (TDrvMain *pDrvMain)
{
    /* Initialize the module's local varniables to default values */
    pDrvMain->tFileInfo.eFileType   = FILE_TYPE_INI;
    pDrvMain->tFileInfo.fCbFunc     = drvMain_GetFileCb;
    pDrvMain->tFileInfo.hCbHndl     = (TI_HANDLE)pDrvMain;
    pDrvMain->eSmState              = SM_STATE_IDLE;
    pDrvMain->uPendingEventsCount   = 0;
	pDrvMain->bRecovery             = TI_FALSE; 
	pDrvMain->uNumOfRecoveryAttempts = 0;
	pDrvMain->eAction               = ACTION_TYPE_NONE; 

    /* Register the Action callback to the context engine and get the client ID */
    pDrvMain->uContextId = context_RegisterClient (pDrvMain->tStadHandles.hContext,
                                                   drvMain_InvokeAction,
                                                   (TI_HANDLE)pDrvMain,
                                                   TI_TRUE,
                                                   "ACTION",
                                                   sizeof("ACTION"));

    /* Platform specific HW preparations */
	hPlatform_Wlan_Hardware_Init(pDrvMain->tStadHandles.hOs);

    /* Insure that device power is off (expected to be) */
    hPlatform_DevicePowerOff ();
}


/* 
 * \fn     drvMain_InitHw & drvMain_InitFw
 * \brief  Init HW and Init FW sequences
 * 
 * drvMain_InitHw - HW init sequence which writes and reads some HW registers
 *                      that are needed prior to FW download.
 * drvMain_InitFw - FW init sequence which downloads the FW image and waits for 
 *                      FW init-complete indication.
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \param  pBuf     - The file data (NVS for HW-init, FW-Image for FW-init).
 * \param  uLength  - The file length.
 * \return TI_OK if succeeded, TI_NOK if failed. 
 * \sa     
 */ 
static TI_STATUS drvMain_InitHw (TI_HANDLE hDrvMain, TI_UINT8 *pbuf, TI_UINT32 uLength)
{
    TDrvMain   *pDrvMain = (TDrvMain *) hDrvMain;

    return TWD_InitHw (pDrvMain->tStadHandles.hTWD, pbuf, uLength, pDrvMain->uRxDmaBufLen, pDrvMain->uTxDmaBufLen);
}

static TI_STATUS drvMain_InitFw (TI_HANDLE hDrvMain, TFileInfo *pFileInfo)
{
    TDrvMain   *pDrvMain = (TDrvMain *) hDrvMain;

    return TWD_InitFw (pDrvMain->tStadHandles.hTWD, pFileInfo);
}


/* 
 * \fn     drvMain_ConfigFw
 * \brief  Configure the FW
 * 
 * The step that follows the FW Init (mainly FW download).
 * The Command-Mailbox interface is enabled here and the FW is configured.
 * 
 * \note   
 * \param  pDrvMain - The DrvMain object
 * \return TI_OK 
 * \sa     drvMain_Init
 */ 
static TI_STATUS drvMain_ConfigFw (TI_HANDLE hDrvMain)
{
    TDrvMain    *pDrvMain = (TDrvMain *) hDrvMain;

    /* get pointer to FW static info (already in driver memory) */
    TFwInfo     *pFwInfo  = TWD_GetFWInfo (pDrvMain->tStadHandles.hTWD); 
    TI_UINT8    *pMacAddr = (TI_UINT8 *)pFwInfo->macAddress; /* STA MAC address */

    /* Update driver's MAC address */
    wlanDrvIf_SetMacAddress (pDrvMain->tStadHandles.hOs, pMacAddr);

    /*
     *  Exit from init mode should be before smeSM starts. this enable us to send
     *  command to the MboxQueue(that store the command) while the interrupts are masked.
     *  the interrupt would be enable at the end of the init process.
     */
    TWD_ExitFromInitMode (pDrvMain->tStadHandles.hTWD);

    /* Configure the FW from the TWD DB */
    TWD_ConfigFw (pDrvMain->tStadHandles.hTWD);

    TRACE0(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_INIT , "EXIT FROM INIT\n");

    /* Print the driver and firmware version and the mac address */
    os_printf("\n");
    os_printf("-----------------------------------------------------\n");
    os_printf("Driver Version  : %s\n", SW_VERSION_STR);
    os_printf("Firmware Version: %s\n", pFwInfo->fwVer);
    os_printf("Station ID      : %02X-%02X-%02X-%02X-%02X-%02X\n",
              pMacAddr[0], pMacAddr[1], pMacAddr[2], pMacAddr[3], pMacAddr[4], pMacAddr[5]);
    os_printf("-----------------------------------------------------\n");
    os_printf("\n");

    return TI_OK;
}


/* 
 * \fn     drvMain_StopActivities
 * \brief  Freeze driver activities
 * 
 * Freeze all driver activities due to stop command or recovery process.
 * 
 * \note   
 * \param  pDrvMain - The DrvMain object
 * \return TI_OK if succeeded, TI_NOK if failed. 
 * \sa     drvMain_EnableActivities
 */ 
static TI_STATUS drvMain_StopActivities (TDrvMain *pDrvMain)
{
    txPort_suspendTx (pDrvMain->tStadHandles.hTxPort);

    /* Disable External Inputs (IRQs and commands) */
    TWD_DisableInterrupts(pDrvMain->tStadHandles.hTWD);
    cmdHndlr_Disable (pDrvMain->tStadHandles.hCmdHndlr);

    /* Initiate TWD Restart */
    return TWD_Stop (pDrvMain->tStadHandles.hTWD);
}


/* 
 * \fn     drvMain_EnableActivities
 * \brief  Enable driver activities
 * 
 * Enable driver activities after init or recovery process completion.
 * 
 * \note   
 * \param  pDrvMain - The DrvMain object
 * \return void 
 * \sa     drvMain_StopActivities
 */ 
static void drvMain_EnableActivities (TDrvMain *pDrvMain)
{
    txPort_resumeTx (pDrvMain->tStadHandles.hTxPort);

   /* Enable External Inputs (IRQ is enabled elsewhere) */
    cmdHndlr_Enable (pDrvMain->tStadHandles.hCmdHndlr);

    /* Enable external events from FW */
    TWD_EnableExternalEvents (pDrvMain->tStadHandles.hTWD);

    
}


/* 
 * \fn     drvMain_ClearQueuedEvents
 * \brief  Enable driver activities
 * 
 * Clear all external events queues (Tx, commands and timers) upon driver stop.
 * 
 * \note   
 * \param  pDrvMain - The DrvMain object
 * \return void 
 * \sa     
 */ 
static void drvMain_ClearQueuedEvents (TDrvMain *pDrvMain)
{
    txDataQ_ClearQueues (pDrvMain->tStadHandles.hTxDataQ);
    txMgmtQ_ClearQueues (pDrvMain->tStadHandles.hTxMgmtQ);
    cmdHndlr_ClearQueue (pDrvMain->tStadHandles.hCmdHndlr);
    tmr_ClearOperQueue (pDrvMain->tStadHandles.hTimer);
}


/* 
 * \fn     drvMain_InsertAction
 * \brief  Get start/stop action and trigger handling
 * 
 * Get start or stop action command from OAL, save it and trigger driver task
 *     for handling it.
 * Wait on a signal object until the requested process is completed.
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \param  eAction  - The requested action
 * \return void 
 * \sa     
 */ 
TI_STATUS drvMain_InsertAction (TI_HANDLE hDrvMain, EActionType eAction)
{
    TDrvMain *pDrvMain = (TDrvMain *) hDrvMain;

    context_EnterCriticalSection(pDrvMain->tStadHandles.hContext);
    if (pDrvMain->eAction == eAction)
    {
        context_LeaveCriticalSection(pDrvMain->tStadHandles.hContext);
        TRACE0(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_CONSOLE, "Action is identical to last action!\n");
        WLAN_OS_REPORT(("Action %d is identical to last action!\n", eAction));
        return TI_OK;
    }

    /* Save the requested action */
    pDrvMain->eAction = eAction;
    context_LeaveCriticalSection(pDrvMain->tStadHandles.hContext);

    /* Create signal object */
    /* 
     * Notice that we must create the signal object before asking for ReSchedule,
     * because we might receive it immidiatly, and then we will be in a different context 
     * with null signal object.
     */
    pDrvMain->hSignalObj = os_SignalObjectCreate (pDrvMain->tStadHandles.hOs);
    if (pDrvMain->hSignalObj == NULL) 
    {
        TRACE0(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "drvMain_InsertAction(): Couldn't allocate signal object!\n");
        return TI_NOK;
    }

    /* Request driver task schedule for action handling */
    context_RequestSchedule (pDrvMain->tStadHandles.hContext, pDrvMain->uContextId);

    /* Wait for the action processing completion */
    os_SignalObjectWait (pDrvMain->tStadHandles.hOs, pDrvMain->hSignalObj);

    /* After "wait" - the action has already been processed in the driver's context */

    /* Free signalling object */
    os_SignalObjectFree (pDrvMain->tStadHandles.hOs, pDrvMain->hSignalObj);
    pDrvMain->hSignalObj = NULL;

    if (pDrvMain->eSmState == SM_STATE_FAILED)
    return TI_NOK;

    return TI_OK;
}


/* 
 * \fn     drvMain_Recovery
 * \brief  Initiate recovery process
 * 
 * Initiate recovery process upon HW/FW error detection (in the Health-Monitor).
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \return TI_OK if started recovery, TI_NOK if recovery is already in progress. 
 * \sa     
 */ 
TI_STATUS drvMain_Recovery (TI_HANDLE hDrvMain)
{
    TDrvMain         *pDrvMain = (TDrvMain *) hDrvMain;

    if (!pDrvMain->bRecovery)
    {
        TRACE1(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_CONSOLE,".....drvMain_Recovery, ts=%d\n", os_timeStampMs(pDrvMain->tStadHandles.hOs));
#ifdef REPORT_LOG
        WLAN_OS_REPORT((".....drvMain_Recovery, ts=%d\n", os_timeStampMs(pDrvMain->tStadHandles.hOs)));
#else
        printk("%s\n",__func__);
#endif
        pDrvMain->bRecovery = TI_TRUE;
        drvMain_SmEvent (hDrvMain, SM_EVENT_RECOVERY);
        return TI_OK;
    }
    else
    {
        TRACE0(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR, "drvMain_Recovery: ****  Recovery already in progress!  ****\n");

		/* nesting recoveries... Try again */
		drvMain_SmEvent (hDrvMain, SM_EVENT_RECOVERY);
        return TI_NOK;
    }
}


/* 
 * \fn     drvMain_RecoveryNotify
 * \brief  Notify STAD modules about recovery
 * 
 * Notify the relevant STAD modules that recovery took place (after completed).
 * 
 * \note   
 * \param  pDrvMain - The DrvMain object
 * \return void 
 * \sa     
 */ 
static void drvMain_RecoveryNotify (TDrvMain *pDrvMain)
{
    txCtrl_NotifyFwReset (pDrvMain->tStadHandles.hTxCtrl);
    scr_notifyFWReset (pDrvMain->tStadHandles.hSCR);
    PowerMgr_notifyFWReset (pDrvMain->tStadHandles.hPowerMgr);

    TRACE1(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_CONSOLE, ".....drvMain_RecoveryNotify: End Of Recovery, ts=%d\n", os_timeStampMs(pDrvMain->tStadHandles.hOs));
    WLAN_OS_REPORT((".....drvMain_RecoveryNotify: End Of Recovery, ts=%d\n", os_timeStampMs(pDrvMain->tStadHandles.hOs)));
}


/* 
 * \fn     drvMain_SmWatchdogTimeout
 * \brief  SM watchdog timer expiry handler
 * 
 * This is the callback function called upon expiartion of the watchdog timer.
 * It is called by the OS-API in timer expiry context, and it issues a failure event to the SM.
 * Note that we can't switch to the driver task as for other timers, since we are using
 *     this timer to protect the init processes, and anyway we just need to stop the driver.
 * 
 * \note   
 * \param  hDrvMain - The DrvMain object
 * \return void
 * \sa     
 */ 

#if 0
static void drvMain_SmWatchdogTimeout (TI_HANDLE hDrvMain)
{
    TDrvMain *pDrvMain = (TDrvMain *)hDrvMain;

    TRACE1(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "drvMain_SmWatchdogTimeout():  State = %d\n", pDrvMain->eSmState);

    /* Send failure event directly to the SM (so the drvMain_SmEvent won't block it).  */

    drvMain_Sm ((TI_HANDLE)pDrvMain, SM_EVENT_FAILURE);
}
#endif

/* 
 * \fn     drvMain_SmEvent
 * \brief  Issue DrvMain SM event
 * 
 * Each event that is handled by the DrvMain state machine, is introduced through this function.
 * To prevent SM recursion, the SM is invoeked only if it's not already handling the 
 *      previous event. 
 * If the SM is busy, the current event is saved until the previous handling is completed.
 * 
 * \note   Recursion may happen because some SM activities generate SM events in the same context.
 * \param  hDrvMain - The DrvMain object
 * \param  eEvent   - The event issued to the SM
 * \return void 
 * \sa     
 */ 
static void drvMain_SmEvent (TI_HANDLE hDrvMain, ESmEvent eEvent)
{
    TDrvMain *pDrvMain = (TDrvMain *)hDrvMain;

    /* Increment pending events counter and save last event. */
    pDrvMain->uPendingEventsCount++;
    pDrvMain->ePendingEvent = eEvent;

    /* If the SM is busy, save event and exit (will be handled when current event is finished) */
    if (pDrvMain->uPendingEventsCount > 1)
    {
        /* Only one pending event is expected (in addition to the handled one, so two together). */
        if (pDrvMain->uPendingEventsCount > 2)
        {
            TRACE3(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "drvMain_SmEvent():  Multiple pending events (%d), State = %d, Event = %d\n", pDrvMain->uPendingEventsCount, pDrvMain->eSmState, eEvent);
        }

        /* Exit. The current event will be handled by the following while loop of the first instance. */
        return;
    }

    /* Invoke the SM with the current event and further events issued by the last SM invocation. */
    while (pDrvMain->uPendingEventsCount > 0)
    {
        drvMain_Sm (hDrvMain, pDrvMain->ePendingEvent);

        /* 
         * Note: The SM may issue another event by calling this function and incrementing 
         *           the counter. 
         *       In this case, only the upper part of this function is run, and the pending
         *           event is hanlded in the next while loo[.
         */

        pDrvMain->uPendingEventsCount--;
    }
}


/* 
 * \fn     drvMain_Sm
 * \brief  The DrvMain state machine
 * 
 * The DrvMain state machine, which handles all driver init, recovery and stop processes.
 * 
 * \note   Since the SM may be called back from its own context, recursion is prevented 
 *             by postponing the last event.
 * \param  hDrvMain - The DrvMain object
 * \param  eEvent   - The event that triggers the SM
 * \return void 
 * \sa     
 */ 
static void drvMain_Sm (TI_HANDLE hDrvMain, ESmEvent eEvent)
{
    TDrvMain    *pDrvMain   = (TDrvMain *)hDrvMain;
    TI_STATUS    eStatus    = TI_NOK;
    TI_HANDLE    hOs        = pDrvMain->tStadHandles.hOs;
    TI_UINT32    uSdioConIndex = 0;
    TI_BOOL      tmpRecovery;

    TRACE2(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_INFORMATION , "drvMain_Sm():  State = %d, Event = %d\n", pDrvMain->eSmState, eEvent);

    /* 
     *  General explenations:
     *  =====================
     *  1) This SM calls some functions that may complete their processing in another context.
     *     All of these functions (wlanDrvIf_GetFile, drvMain_InitHw, drvMain_InitFw, drvMain_ConfigFw,
     *         drvMain_StopActivities, smeSm_start, smeSm_stop) are provided with a callback which 
     *         they always call upon completion, even if they are completed in the original (SM) context.
     *     Since these callbacks are calling the SM, a simple mechanism is added to prevent
     *         recursion, by postponing the last event if the SM is still in the previous event's context.
     *  2) In any case of unexpected event, the eStatus remains TI_NOK, leading to the FAILED state!
     *     FAILED state is also reached if any of the functions listed in note 1 returns TI_NOK.
     *     Note that if these functions detect a failure in another context, they may call their callback
     *         with the eStatus parameter set to TI_NOK, or call the drvMain_InitFailCb callback.
     *     All these cases lead to FAILED state which terminates all driver activities and wait for destroy.
     *  3) Note that the wlanDrvIf_GetFile is always completed in the original context, and the
     *         option of completion in a later context is only for future use.
     *  4) All processes (Start, Stop, Relcovery) are protected by a watchdog timer to let
     *         the user free the driver in case of deadlock during the process.
     */

    switch (pDrvMain->eSmState) 
    {
    case SM_STATE_IDLE:
        /*
         * We get a START action after all modules are created and linked.
         * Disable further actions, start watchdog timer and request for the ini-file.
         */
        if (eEvent == SM_EVENT_START) 
        {
            pDrvMain->eSmState = SM_STATE_WAIT_INI_FILE;
            context_DisableClient (pDrvMain->tStadHandles.hContext, pDrvMain->uContextId);
            pDrvMain->tFileInfo.eFileType = FILE_TYPE_INI;
            eStatus = wlanDrvIf_GetFile (hOs, &pDrvMain->tFileInfo);
        }
        break;
    case SM_STATE_WAIT_INI_FILE:
        /* 
         * We've got the ini-file.
         * Set STAD and TWD modules defaults according to the ini-file, 
         *     turn on the device and request for the NVS file.
         */
        if (eEvent == SM_EVENT_INI_FILE_READY) 
        {
            pDrvMain->eSmState = SM_STATE_WAIT_NVS_FILE;
            drvMain_SetDefaults (hDrvMain, pDrvMain->tFileInfo.pBuffer, pDrvMain->tFileInfo.uLength);
            hPlatform_DevicePowerOn ();

            pDrvMain->tFileInfo.eFileType = FILE_TYPE_NVS;
            eStatus = wlanDrvIf_GetFile (hOs, &pDrvMain->tFileInfo);
        }
        break;

    case SM_STATE_WAIT_NVS_FILE:

        /* SDBus Connect connection validation  */
        for(uSdioConIndex=0; (uSdioConIndex < SDIO_CONNECT_THRESHOLD) && (eStatus != TI_OK); uSdioConIndex++)
        {
			/* : We should split the call to txnQ_ConnectBus to other state in order to support Async bus connection */
            eStatus = txnQ_ConnectBus(pDrvMain->tStadHandles.hTxnQ, &pDrvMain->tBusDrvCfg, NULL, NULL, &pDrvMain->uRxDmaBufLen, &pDrvMain->uTxDmaBufLen); 

            if((eStatus != TI_OK) &&
			   (uSdioConIndex < (SDIO_CONNECT_THRESHOLD - 1)))
            {
                     TRACE0(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_WARNING , "SDBus Connect Failed\n");
                     WLAN_OS_REPORT(("Try to SDBus Connect again...\n"));
                     if (uSdioConIndex > 1)
						hPlatform_DevicePowerOffSetLongerDelay();
					 else
						hPlatform_DevicePowerOff();
                     hPlatform_DevicePowerOn();
            }
        }

        if(eStatus != TI_OK)
        {
			WLAN_OS_REPORT(("SDBus Connect Failed, Set Object Event !!\r\n"));
			TRACE0(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "SDBus Connect Failed, Set Object Event !!\r\n");
			if (!pDrvMain->bRecovery)
			{
				os_SignalObjectSet(hOs, pDrvMain->hSignalObj);
			}
        }
        else /* SDBus Connect success */
        {
            /*
             * We've got the NVS file.
             * Start HW-Init process providing the NVS file.
             */
            if (eEvent == SM_EVENT_NVS_FILE_READY)
            {
                pDrvMain->eSmState = SM_STATE_HW_INIT;
                eStatus = drvMain_InitHw (hDrvMain, pDrvMain->tFileInfo.pBuffer, pDrvMain->tFileInfo.uLength);
            }
        }
        break;
    case SM_STATE_HW_INIT:
        /*
         * HW-Init process is completed.
         * Request for the FW image file.
         */
        if (eEvent == SM_EVENT_HW_INIT_COMPLETE)
        {
            pDrvMain->tFileInfo.eFileType = FILE_TYPE_FW;
            pDrvMain->eSmState = SM_STATE_DOWNLOAD_FW_FILE;
            eStatus = wlanDrvIf_GetFile (hOs, &pDrvMain->tFileInfo);
        }
        break;
    case SM_STATE_DOWNLOAD_FW_FILE:
        if (eEvent == SM_EVENT_FW_FILE_READY)
        {
            pDrvMain->tFileInfo.eFileType = FILE_TYPE_FW_NEXT;
            if (pDrvMain->tFileInfo.bLast == TI_TRUE)
            {
            pDrvMain->eSmState = SM_STATE_FW_INIT;
            }
            else
            {
                pDrvMain->eSmState = SM_STATE_WAIT_FW_FILE;
            }
            /*
             * We've got the FW image file.
             * Start FW-Init process (mainly FW image download) providing the FW image file.
             */
            eStatus = drvMain_InitFw (hDrvMain, &pDrvMain->tFileInfo);
        }
        break;
    case SM_STATE_WAIT_FW_FILE:
        if (eEvent == SM_EVENT_FW_INIT_COMPLETE)
        {
            pDrvMain->eSmState = SM_STATE_DOWNLOAD_FW_FILE;
            eStatus = wlanDrvIf_GetFile (hOs, &pDrvMain->tFileInfo);
        }
        break;
    case SM_STATE_FW_INIT:
        /*
         * FW-Init process is completed.
         * Free the semaphore of the START action to enable the OS interface.
         * Enable interrupts (or polling for debug).
         * Start FW-Configuration process, and free the semaphore of the START action.
         *
         * Note that in some OSs, the semaphore must be released in order to enable the
         *     interrupts, and the interrupts are needed for the configuration process!
         */
        if (eEvent == SM_EVENT_FW_INIT_COMPLETE)
        {
            pDrvMain->eSmState = SM_STATE_FW_CONFIG;
            TWD_EnableInterrupts(pDrvMain->tStadHandles.hTWD);
          #ifdef PRIODIC_INTERRUPT
            /* Start periodic interrupts. It means that every period of time the FwEvent SM will be called */
            os_periodicIntrTimerStart (hOs);
          #endif
            eStatus = drvMain_ConfigFw (hDrvMain);
        }
        break;
    case SM_STATE_FW_CONFIG:
        /*
         * FW-configuration process is completed.
         * Stop watchdog timer.
         * For recovery, notify the relevant STAD modules.
         * For regular start, start the SME which handles the connection process.
         * Update timer and OAL about entering OPERATIONAL state (OAL ignores recovery)
         * Enable driver activities and external events.
         * Enable STOP action
         * We are now in OPERATIONAL state, i.e. the driver is fully operational!
         */

        tmpRecovery = pDrvMain->bRecovery;
        if (eEvent == SM_EVENT_FW_CONFIG_COMPLETE) 
        {
            pDrvMain->eSmState = SM_STATE_OPERATIONAL;
            if (pDrvMain->bRecovery) 
            {
                pDrvMain->uNumOfRecoveryAttempts = 0;
                drvMain_RecoveryNotify (pDrvMain);
                pDrvMain->bRecovery = TI_FALSE;
            }
            else 
            {
                sme_Start (pDrvMain->tStadHandles.hSme); 
                wlanDrvIf_UpdateDriverState (hOs, DRV_STATE_RUNNING);
            }
            tmr_UpdateDriverState (pDrvMain->tStadHandles.hTimer, TI_TRUE);
            drvMain_EnableActivities (pDrvMain);
            context_EnableClient (pDrvMain->tStadHandles.hContext, pDrvMain->uContextId);
            eStatus = TI_OK;
           
        }
        if (!tmpRecovery)
        {
            os_SignalObjectSet(hOs, pDrvMain->hSignalObj);
        }
        break;
    case SM_STATE_OPERATIONAL:
        /* 
         * Disable start/stop commands and start watchdog timer.
         * Update timer and OAL about exiting OPERATIONAL state (OAL ignores recovery).
         * For STOP, stop SME (handle disconnection) and move to DISCONNECTING state.
         * For recovery, stop driver activities and move to STOPPING state.
         * Note that driver-stop process may be Async if we are during Async bus transaction.
         */
        
        context_DisableClient (pDrvMain->tStadHandles.hContext, pDrvMain->uContextId);
        tmr_UpdateDriverState (pDrvMain->tStadHandles.hTimer, TI_FALSE);
        if (eEvent == SM_EVENT_STOP) 
        {
            pDrvMain->eSmState = SM_STATE_DISCONNECTING;
            wlanDrvIf_UpdateDriverState (hOs, DRV_STATE_STOPING);
            sme_Stop (pDrvMain->tStadHandles.hSme);
            eStatus = TI_OK;
        }
        else if (eEvent == SM_EVENT_RECOVERY) 
        {
            pDrvMain->eSmState = SM_STATE_STOPPING;
            eStatus = drvMain_StopActivities (pDrvMain);
        }
        
        break;
    case SM_STATE_DISCONNECTING:
        /* 
         * Note that this state is not relevant for recovery.
         * SME stop is completed 
         * Stop driver activities and move to STOPPING state.
         * Note that driver stop process may be Async if we are during Async bus transaction.
         */
        
        if (eEvent == SM_EVENT_DISCONNECTED) 
        {
            pDrvMain->eSmState = SM_STATE_STOPPING;
            eStatus = drvMain_StopActivities (pDrvMain);
        }
        break;
    case SM_STATE_STOPPING:
        /* 
         * Driver stopping process is done.
         * Turn device power off.
         * For recovery, turn device power back on, request NVS file and continue with
         *     the init process (recover back all the way to OPERATIONAL state).
         * For STOP process, the driver is now fully stopped (STOPPED state), so stop watchdog timer,
         *     clear all events queues, free the semaphore of the STOP action and enable START action.
         */
        
        if (eEvent == SM_EVENT_STOP_COMPLETE) 
        {
            txnQ_DisconnectBus (pDrvMain->tStadHandles.hTxnQ);
            hPlatform_DevicePowerOff ();
            if (pDrvMain->bRecovery) 
            {
                hPlatform_DevicePowerOn ();
                pDrvMain->eSmState = SM_STATE_WAIT_NVS_FILE;
                pDrvMain->tFileInfo.eFileType = FILE_TYPE_NVS;
                eStatus = wlanDrvIf_GetFile (hOs, &pDrvMain->tFileInfo);
            }
            else 
            {
                pDrvMain->eSmState = SM_STATE_STOPPED;
                drvMain_ClearQueuedEvents (pDrvMain);
                scr_notifyFWReset(pDrvMain->tStadHandles.hSCR);
                os_SignalObjectSet (hOs, pDrvMain->hSignalObj);
                context_EnableClient (pDrvMain->tStadHandles.hContext, pDrvMain->uContextId);
                wlanDrvIf_UpdateDriverState (hOs, DRV_STATE_STOPPED);
                eStatus = TI_OK;
            }
        }
        
        break;
    case SM_STATE_STOPPED:
        /* 
         * A START action command was inserted, so we go through the init process.
         * Disable further actions, start watchdog timer, turn on device and request NVS file.
         */
        
        context_DisableClient (pDrvMain->tStadHandles.hContext, pDrvMain->uContextId);
        if (eEvent == SM_EVENT_START) 
        {
            hPlatform_DevicePowerOn ();
            pDrvMain->eSmState = SM_STATE_WAIT_NVS_FILE;
            pDrvMain->tFileInfo.eFileType = FILE_TYPE_NVS;
            eStatus = wlanDrvIf_GetFile (hOs, &pDrvMain->tFileInfo);
        }
        break;
    case SM_STATE_STOPPING_ON_FAIL:
        /*
         * Driver stopping process upon failure is completed.
         * Turn off the device and move to FAILED state.
         */
        
        pDrvMain->eSmState = SM_STATE_FAILED;
        txnQ_DisconnectBus (pDrvMain->tStadHandles.hTxnQ);
        hPlatform_DevicePowerOff ();
        if (!pDrvMain->bRecovery)
        {
            os_SignalObjectSet (hOs, pDrvMain->hSignalObj);
        }
        else if (pDrvMain->uNumOfRecoveryAttempts < MAX_NUM_OF_RECOVERY_TRIGGERS) 
        {
            pDrvMain->uNumOfRecoveryAttempts++;
            pDrvMain->eSmState = SM_STATE_STOPPING;
            eStatus = drvMain_StopActivities (pDrvMain);
        }
        WLAN_OS_REPORT(("[WLAN] Exit application\n"));
        pDrvMain->bRecovery = TI_FALSE;
        break;
    case SM_STATE_FAILED:
        /* Nothing to do except waiting for Destroy */
        break;
    default:
        TRACE2(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "drvMain_Sm: Unknown state, eEvent=%u at state=%u\n", eEvent, pDrvMain->eSmState);
        /* Note: Handled below as a failure since the status remains TI_NOK */
        break;  
    }

    /* Handle failures (status = NOK) if not handled yet */
    if ((eStatus == TI_NOK) && 
        (pDrvMain->eSmState != SM_STATE_FAILED) &&
        (pDrvMain->eSmState != SM_STATE_STOPPING_ON_FAIL))
    {
        TRACE3(pDrvMain->tStadHandles.hReport, REPORT_SEVERITY_ERROR , "drvMain_Sm: eEvent=%u at state=%u, status=%d\n", eEvent, pDrvMain->eSmState, eStatus);
        pDrvMain->eSmState = SM_STATE_STOPPING_ON_FAIL;
        wlanDrvIf_UpdateDriverState (hOs, DRV_STATE_FAILED);

        /* 
         * Stop all activities. This may be completed in a different context if
         *     we should wait for an Async bus transaction completion.
         * The drvMain_TwdStopCb is called from the TWD in any case to pass
         *     us to the SM_STATE_FAILED state (where we wait for Destroy).
         */
        eStatus = drvMain_StopActivities (pDrvMain);
    }
}
