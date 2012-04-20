/*
 * ScanCncn.h
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

/** \file  ScanCncn.h
 *  \brief Scan concentartor module API
 *
 *  \see   ScanCncn.c
 */

#ifndef __SCANCNCN_H__
#define __SCANCNCN_H__

#include "osTIType.h"
#include "TWDriver.h"
#include "scrApi.h"
#include "mlmeApi.h"

#define SCAN_CNCN_APP_SCAN_TABLE_ENTRIES 64

/** \enum EScanCncnClient
 * \brief	Scan Concentrator Client
 * 
 * \par Description
 * Enumerates the different possible clients requesting scan from the scan concentrator
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	SCAN_SCC_ROAMING_IMMED,     	/**< Indicates a scan from the immediate scan for a roaming client */
/*	1	*/	SCAN_SCC_ROAMING_CONT,          /**< Indicates a scan from the continuous scan for a roaming client */
/*	2	*/	SCAN_SCC_DRIVER,                /**< Indicates a scan from the driver client (SME scan) */
/*	3	*/	SCAN_SCC_APP_PERIODIC,          /**< Indicates application (user) periodic scan */
/*	4	*/	SCAN_SCC_APP_ONE_SHOT,          /**< Indicates application (user) one shot scan */
/*	5	*/	SCAN_SCC_NUM_OF_CLIENTS,        /**< Number of scan clients (used internally) */
/*	6	*/	SCAN_SCC_NO_CLIENT              /**< No valid scan clients (used internally) */

} EScanCncnClient;

/** \enum EScanCncnResultStatus
 * \brief	Scan Concentrator Result Status
 * 
 * \par Description
 * Enumerates the possible scan result statuses
 * 
 * \sa
 */
typedef enum 
{
/*	0	*/	SCAN_CRS_RECEIVED_FRAME = 0,            /**< Scan is still running; management frame information is passed. */
/*	1	*/	SCAN_CRS_SCAN_COMPLETE_OK,              /**< Scan completed successfully */
/*	2	*/	SCAN_CRS_SCAN_RUNNING,                  /**< Scan started successfully and now is running */
/*	3	*/	SCAN_CRS_SCAN_FAILED,                   /**< 
													* scan failed due to unexpected situation (SCR reject, no 
													* channels available, scan SRV returned TI_NOK, etc)
													*/
/*	4	*/	SCAN_CRS_SCAN_STOPPED,                  /**< scan stopped by user */
/*	5	*/	SCAN_CRS_TSF_ERROR,                     /**< TSF error (AP recovery) occurred (for SPS only): 
													* SPS was not performed because current TSF value is less than designated TSF value.
													*/
/*	6	*/	SCAN_CRS_SCAN_ABORTED_FW_RESET,         /**< scan aborted due to FW reset */
/*	7	*/	SCAN_CRS_SCAN_ABORTED_HIGHER_PRIORITY,  /**< Scan was aborted because of a higher priority client request of the channel (switched channel) */
/*	8	*/	SCAN_CRS_NUM_OF_RES_STATUS              /**< number of possible result status */

} EScanCncnResultStatus;

/** \struct TScanFrameInfo
 * \brief Scan Fram Information
 * 
 * \par Description
 * This structure contains a single frame information, returned by the result CB when a frame is available.
 * It is used to pass scan results (beacons and probe responses).
 * 
 * \sa
 */ 
typedef struct
{
    TMacAddr*           bssId;              /* BSSID (MAC address) of the AP from which the frame was received */
    mlmeFrameInfo_t*    parsedIEs;          /* Information elements in the frame, which is parsed */
    ERadioBand          band;               /* Band on which the frame was received */
    TI_UINT8            channel;            /* Channel on which the frame was received */
    TI_UINT32           staTSF;             /* TSF of the station when the frame was received */
    TI_INT8             rssi;               /* RSSI level at which frame was received */
    TI_INT8             snr;                /* SNR level at which frame was received */
    ERate               rate;               /* Bitrate at which frame was received */
    TI_UINT8*           buffer;             /* Frame information elements, unparsed */
    TI_UINT16           bufferLength;       /* Length of the frame unparsed information elements */
} TScanFrameInfo;

 /** \typedef TScanResultCB
  * \brief Defines the function prototype for the scan result callback
  * (notification by the scan concentrator to a client of either a scan
  * termination or a result frame received).
  */
/**
 * \brief	Scan Result CB  
 * 
 * \param  clientObj    				- TWD module object handle
 * \param  status 				- TID number
 * \param  frameInfo 				- Policy : Enable / Disable 
 * \param  SPSStatus 					- Mac address of: SA as receiver / RA as initiator
 * \return void
 * 
 * \par Description
 * Defines the function prototype for the scan result callback
 * (notification by the scan concentrator to a client of either a scan termination or a result frame received)
 * This CB is egistered by each client and invoked by the scan concentrator, passes scan results to the caller.
 * 
 * \sa
 */
typedef void (*TScanResultCB) (TI_HANDLE clientObj, EScanCncnResultStatus status,
                               TScanFrameInfo* frameInfo, TI_UINT16 SPSStatus);


TI_HANDLE               scanCncn_Create (TI_HANDLE hOS);
void                    scanCncn_Destroy (TI_HANDLE hScanCncn);
void                    scanCncn_Init (TStadHandlesList *pStadHandles);
void                    scanCncn_SetDefaults (TI_HANDLE hScanCncn, TScanCncnInitParams* pScanCncnInitParams);
void                    scanCncn_SwitchToConnected (TI_HANDLE hScanCncn);
void                    scanCncn_SwitchToNotConnected (TI_HANDLE hScanCncn);
void                    scanCncn_SwitchToIBSS (TI_HANDLE hScanCncn);
/**
 * \brief Starts a one-shot scan operation
 * 
 * \param  hScanCncn 	- Handle to the scan concentrator object
 * \param  eClient 		- The client requesting the scan operation
 * \param  pScanParams 	- Parameters for the requested scan
 * \return Scan Concentrator Result Status: 
 * SCAN_CRS_SCAN_RUNNING - scan started successfully and is now running
 * SCAN_CRS_SCAN_FAILED - scan failed to start due to an unexpected error
 *
 * \par Description
 * Starts a one-shot scan operation:
 *  - copies scan params to scan concentrator object
 *  - copies current SSID for roaming scans
 *  - verifies the requested channels with the reg doamin
 *  - if needed, adjust to SG compensation values
 *  - send an event to the client SM
 * 
 * \sa	scanCncn_StopScan
 */ 
EScanCncnResultStatus   scanCncn_Start1ShotScan (TI_HANDLE hScanCncn, EScanCncnClient eClient, TScanParams* pScanParams);
/**
 * \brief Stop an on-going one-shot scan operation
 * 
 * \param  hScanCncn 	- Handle to the scan concentrator object
 * \param  eClient 		- The client requesting to stop the scan operation
 * \return void
 * 
 * \par Description
 * Set necessary flags and send a stop scan event to the client SM
 * 
 * \sa	scanCncn_Start1ShotScan
 */ 
void                    scanCncn_StopScan (TI_HANDLE hScanCncn, EScanCncnClient eClient);
EScanCncnResultStatus   scanCncn_StartPeriodicScan (TI_HANDLE hScanCncn, EScanCncnClient eClient,
                                                    TPeriodicScanParams *pScanParams);
void                    scanCncn_StopPeriodicScan (TI_HANDLE hScanCncn, EScanCncnClient eClient);
/**
 * \brief Registers a scan complete object
 * 
 * \param  hScanCncn 		- Handle to the scan concentrator object
 * \param  eClient 			- The client requesting to stop the scan operation
 * \param  scanResultCBFunc - Pointer to the resulting callback function
 * \param  scanResultCBObj 	- Object passed to the scan resulting callback function
 * \return void
 * 
 * \par Description
 * Registers a callback function for a client that is called for every scan result and scan complete event. 
 * It is called by each client one time before issuing a scan request.
 * 
 * \sa	
 */ 
void                    scanCncn_RegisterScanResultCB (TI_HANDLE hScanCncn, EScanCncnClient eClient,
                                                       TScanResultCB scanResultCBFunc, TI_HANDLE scanResultCBObj);
void                    scanCncn_ScanCompleteNotificationCB (TI_HANDLE hScanCncn, EScanResultTag eTag,
                                                             TI_UINT32 uResultCount, TI_UINT16 SPSStatus,
                                                             TI_BOOL bTSFError, TI_STATUS scanStatus,
                                                             TI_STATUS PSMode);
void                    scanCncn_MlmeResultCB (TI_HANDLE hScanCncn, TMacAddr* bssid, mlmeFrameInfo_t* frameInfo, 
                                               TRxAttr* pRxAttr, TI_UINT8* buffer, TI_UINT16 bufferLength);
void                    scanCncn_ScrRoamingImmedCB (TI_HANDLE hScanCncn, EScrClientRequestStatus eRequestStatus,
                                                    EScrResourceId eResource, EScePendReason ePendReason);
void                    scanCncn_ScrRoamingContCB (TI_HANDLE hScanCncn, EScrClientRequestStatus eRequestStatus,
                                                   EScrResourceId eResource, EScePendReason ePendReason);
void                    scanCncn_ScrAppCB (TI_HANDLE hScanCncn, EScrClientRequestStatus eRequestStatus,
                                           EScrResourceId eResource, EScePendReason ePendReason );
void                    scanCncn_ScrDriverCB (TI_HANDLE hScanCncn, EScrClientRequestStatus eRequestStatus,
                                              EScrResourceId eResource, EScePendReason ePendReason);
void                    scanCncn_SGconfigureScanParams (TI_HANDLE hScanCncn, TI_BOOL bUseSGParams,
                                                        TI_UINT8 probeReqPercent, TI_UINT32 SGcompensationMaxTime,
                                                        TI_UINT32 SGcompensationPercent);
/* Scan concentrator application functions */
TI_STATUS               scanCncnApp_SetParam (TI_HANDLE hScanCncn, paramInfo_t *pParam);
TI_STATUS               scanCncnApp_GetParam (TI_HANDLE hScanCncn, paramInfo_t *pParam);
void                    scanCncn_AppScanResultCB (TI_HANDLE hScanCncn, EScanCncnResultStatus status,
                                                  TScanFrameInfo* frameInfo, TI_UINT16 SPSStatus);
void                    scanCncn_PeriodicScanCompleteCB (TI_HANDLE hScanCncn, char* str, TI_UINT32 strLen);
void                    scanCncn_PeriodicScanReportCB (TI_HANDLE hScanCncn, char* str, TI_UINT32 strLen);

#endif /* __SCANCNCN_H__ */

