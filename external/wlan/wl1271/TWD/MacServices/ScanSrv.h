/*
 * ScanSrv.h
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

/** \file ScanSrv.h
 *  \brief This file include private definitions for the scan SRV module.
 *
 *  \see   ScanSrv.c, ScanSrvSM.c, ScanSrvSM.h
 */


#ifndef __SCANSRV_H__
#define __SCANSRV_H__

#include "TWDriverInternal.h"
#include "MacServices_api.h"
#include "fsm.h"
#include "ScanSrvSM.h"

/*
 ***********************************************************************
 *  Constant definitions.
 ***********************************************************************
 */
/* guard time for scan (added to calculated scan duration) */
#define SCAN_SRV_FW_GUARD_TIME_MS                       62000
/* module name for driver mode requests */
#define SCAN_SRV_NAME                                   "SSRV"


/*
 ***********************************************************************
 *  Enums.
 ***********************************************************************
 */


/*
 ***********************************************************************
 *  Typedefs.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *  Structure definitions.
 ***********************************************************************
 */

/** \struct scanSRV_t
 * \brief This structure contains the scan SRV object data
 */
typedef struct
{
    TI_HANDLE           hOS;                           /**< OS object handle */
    TI_HANDLE           hReport;                       /**< report object handle */
    TI_HANDLE           hPowerSrv;                     /**< power Server object handle */
    TI_HANDLE           hTWD;                          /**< TWD layer handle */
    TI_HANDLE           hTimer;                        /**< Timer module object handle */
    TI_HANDLE           hEventMbox;                    /**< EventMbox module object handle */
    TI_HANDLE           hCmdBld;                       /**< Command builder object handle */
    TScanSrvCompleteCb  scanCompleteNotificationFunc;  /**< 
                                                         * upper layer (scan concentrator) scan complete 
                                                         * callback function
                                                         */
    TI_HANDLE           scanCompleteNotificationObj;    /**< 
                                                         * upper layer (scan concentrator) scan complete
                                                         * callback function
                                                         */
    
    TCmdResponseCb      commandResponseFunc;            /**<
                                                         * upper layer command response CB. Passed down into the HAL
                                                         * and called when the scan command has been received by the FW
                                                         */
    TI_HANDLE           commandResponseObj;             /**<
                                                         * object parameter passed to the commandResposeFunc by the HAL
                                                         * when it is called 
                                                         */
    TFailureEventCb     failureEventFunc;               /**<
                                                         * upper layer Failure Event CB.
                                                         * called when the scan command has been Timer Expiry
                                                         */
    TI_HANDLE           failureEventObj;                /**<
                                                         * object parameter passed to the failureEventFunc
                                                         * when it is called 
                                                         */
    TI_UINT16           SPSScanResult;                  /**< 
                                                         * bitmap indicating which channels were scanned
                                                         * in an SPS scan
                                                         */
    TI_BOOL             bTSFError;                      /** indicates whether a TSF error occured */                                                        /**< 
                                                         * scan result: indicates a TSF error event and 
                                                         * which channels were scanned in SPS
                                                         */
    TI_BOOL             bDtimOverlapping;               /**< Indicates whether the scan is overlapping DTIM */
    TI_BOOL             bExitFromDriverMode;            /**< 
                                                         * Indicates whether to exit driver mode once scan 
                                                         * is finished
                                                         */
    TI_BOOL             bSendNullData;                  /**< 
                                                         * Indicates whether to send Null data when exiting driver  
                                                         * mode once scan is finished
                                                         */
    TI_BOOL             bScanOnDriverModeFailure;       /**< 
                                                         * Indicates whether to scan if driver mode entry
                                                         * wasn't successful
                                                         */
    TI_BOOL             bHighPriority;                  /**<
                                                         * Indicates whether to request high priority 
                                                         * (overlapping DTIM) scan
                                                         */
    TI_BOOL             bSPSScan;                       /**< 
                                                         * whether the running scan type is SPS (TI_TRUE)
                                                         * or something else (TI_FALSE). Used to stop a
                                                         * running scan.
                                                         */
    TScanParams*        scanParams;                     /**< scan parameters */
    EScanResultTag      eScanTag;                       /**< scan result tag */
    TI_UINT32           uResultCount;                   /**< number of scan results (received from the FW) */
    TI_HANDLE           hScanSrvTimer;                  /**< scan operation timer */
    TI_BOOL             bTimerRunning;                  /**< whether the above timer is running */
    TI_BOOL             bInRequest;                     /**<
                                                         * Indicates whether the SM is run within
                                                         * the scan request context (if so, to avoid
                                                         * re-entrance, the complete function shouldn't
                                                         * be called on failure, but rather an invalid
                                                         * status should be returned)
                                                         */
    TI_STATUS           returnStatus;                   /**< 
                                                         * Holds the return code to the upper layer
                                                         * Used to save errors during SM operation.
                                                         */
    /* state machine */
    fsm_stateMachine_t* SM;                             /**< 
                                                         * state machines for different
                                                         * scan types
                                                         */
    scan_SRVSMStates_e  SMState;                        /**< 
                                                         * state machine current states 
                                                         * for different scan types
                                                         */
    E80211PsMode        psRequest;                      /**< 
                                                         * Indicates if PS was requested or not
                                                         * for current scan
                                                         */
    TI_UINT32           numberOfNoScanCompleteToRecovery;
                                                        /**< 
                                                         * The number of consecutive no scan complete
                                                         * that will trigger a recovery notification
                                                         */
    TI_UINT32           currentNumberOfConsecutiveNoScanCompleteEvents;
                                                        /**<
                                                         * The number of consecutivre no scan complete 
                                                         * events at present
                                                         */
    TI_BOOL             bNoScanCompleteFlag;            /**<
                                                         * Indicates if the last event was start scan
                                                         * (true) or no scan complete (false) to be able
                                                         * to nullify correctly the above counter */
    TI_UINT32           uTriggeredScanTimeOut;          /**<
                                                         * Time out for starting triggered scan between 
                                                         * 2 channels */
    TI_UINT8                uDtimPeriod;
    TI_UINT16               uBeaconInterval;
} scanSRV_t;

/*
 ***********************************************************************
 *  External data definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *  External functions definitions
 ***********************************************************************
 */

/**
 * \\n
 * \date 16-Oct-2004\n
 * \brief Creates the scan SRV object
 *
 * Function Scope \e Public.\n
 * \param hOS - handle to the OS object.\n
 * \return a handle to the scan SRV object, NULL if an error occurred.\n
 */
TI_HANDLE MacServices_scanSRV_create( TI_HANDLE hOS );

/**
 * \\n
 * \date 29-Dec-2004\n
 * \brief Finalizes the scan SRV module (releasing memory and timer)
 *
 * Function Scope \e Public.\n
 * \param hScanSRV - handle to the scan SRV object.\n
 */
void MacServices_scanSRV_destroy( TI_HANDLE hScanSRV );

/**
 * \\n
 * \date 29-Dec-2004\n
 * \brief Initializes the scan SRV module, registers SCAN_COMPLETE to HAL.
 *
 * Function Scope \e Public.\n
 * \param hScanSRV - handle to the scan SRV object.\n
 * \param Handles of other modules.\n
  */
TI_STATUS MacServices_scanSRV_init (TI_HANDLE hMacServices,
                                    TI_HANDLE hReport,
                                    TI_HANDLE hTWD,
                                    TI_HANDLE hTimer,
                                    TI_HANDLE hEventMbox,
                                    TI_HANDLE hCmdBld);

/**
 * \\n
 * \date 26-July-2006\n
 * \brief Configures the scan SRV module with initialization values
 *
 * Function Scope \e Public.\n
 * \param hScanSRV - handle to the scan SRV object.\n
 * \param hReport - handle to the report object.\n
 * \param hHalCtrl - handle to the HAL ctrl object.\n
  */
void MacServices_scanSrv_config( TI_HANDLE hMacServices, TScanSrvInitParams* pInitParams );

/**
 * \\n
 * \date 29-Dec-2004\n
 * \brief Calculates the maximal time required for a scan operation
 *
 * Function Scope \e Public.\n
 * \param hScanSRV - handle to the scan SRV object.\n
 * \param scanParams - the scan parameters
 * \param bConsiderDTIM - whether this scan overlaps DTIM
 * \return the time (in milliseconds)
 */
TI_UINT32 MacServices_scanSRVcalculateScanTimeout( TI_HANDLE hScanSrv, TScanParams* scanParams, TI_BOOL bConsiderDTIM );

/**
 * \\n
 * \date 16-Jan-2005\n
 * \brief Convert time units (1024 usecs) to millisecs
 *
 * Function Scope \e Private.\n
 * \param tu - the time in time units
 * \return the time in milliseconds
 */
TI_UINT32 MacServices_scanSRVConvertTUToMsec( TI_UINT32 tu );

/**
 * \\n
 * \date 27-Sep-2005\n
 * \brief This function is the CB which is called as response to 'StartScan' or 'StopScan' \n.
 *        here we check if there is a GWSI command response , and call it if necessary .\n
 * Function Scope \e Private.\n
 * \param hScanSrv - handle to the scan SRV object.\n
 * \param MboxStatus - mailbox status. \n
 */

void MacServices_scanSRVCommandMailBoxCB(TI_HANDLE hScanSrv,TI_UINT16 MboxStatus);


/**
 * \brief Registers a failure event callback for scan error notifications.
 *
 * Function Scope \e Public.\n
 * \param hMacServices - handle to the MacServices object.\n
 * \param failureEventCB - the failure event callback function.\n
 * \param hFailureEventObj - handle to the object passed to the failure event callback function.\n
 */
void scanSRV_registerFailureEventCB( TI_HANDLE hScanSRV, 
                                     void * failureEventCB, TI_HANDLE hFailureEventObj );

void scanSRV_restart( TI_HANDLE hScanSRV);


#endif /* __SCANSRV_H__ */
