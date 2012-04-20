/*
 * scanMngrApi.h
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

/** \file  scanMngrApi.h
 *  \brief This file include public definitions for the scan manager module, comprising its API.
 *
 *  \see   scanMngr.c, scanMngr.h, scanMngrTypes.h
 */


#ifndef __SCAN_MNGR_API_H__
#define __SCAN_MNGR_API_H__

#include "scanMngrTypes.h"
#include "bssTypes.h"
#include "ScanCncn.h"
#include "DrvMainModules.h"


/*
 ***********************************************************************
 *  Constant definitions.
 ***********************************************************************
 */

#define SCANNING_OPERATIONAL_MODE_MANUAL   0
#define SCANNING_OPERATIONAL_MODE_AUTO 	   1


/*
 ***********************************************************************
 *  Enums.
 ***********************************************************************
 */
/** \enum scan_mngrResultStatus_e
 *
 * \brief Scan Manager Result Status
 * 
 * \par Description
 * Enumerates the possible Scan result statuses.
 * Returned as a response to an immediate scan request.
 * 
 * \sa
 */
 typedef enum
{
/*	0	*/	SCAN_MRS_SCAN_COMPLETE_OK, 		                	/**< Scan was completed successfully 						*/
/*	1	*/	SCAN_MRS_SCAN_RUNNING,                              /**< Scan was started successfully and is now running 		*/ 
/*	2	*/	SCAN_MRS_SCAN_NOT_ATTEMPTED_ALREADY_RUNNING,        /**< scan was not attempted because it is already running 	*/
/*	3	*/	SCAN_MRS_SCAN_NOT_ATTEMPTED_EMPTY_POLICY,            /**< 
																* Scan was not attempted because the policy defines
																* NULL scan type
																*/
/*	4	*/	SCAN_MRS_SCAN_NOT_ATTEMPTED_NO_CHANNLES_AVAILABLE, 	/**< 
																* Scan was not attempted because no channels are 
																* available for scan, according to the defined policy.
																*/
/*	5	*/	SCAN_MRS_SCAN_FAILED,                             	/**< Scan failed to start 									*/
/*	6	*/	SCAN_MRS_SCAN_STOPPED,                              /**< Scan was stopped by caller 							*/
/*	7	*/	SCAN_MRS_SCAN_ABORTED_FW_RESET,                     /**< Scan was aborted due to recovery 						*/
/*	8	*/	SCAN_MRS_SCAN_ABORTED_HIGHER_PRIORITY               /**< Scan was aborted due to a higher priority client 		*/

} scan_mngrResultStatus_e;

typedef enum {
    CONNECTION_STATUS_CONNECTED =0,
    CONNECTION_STATUS_NOT_CONNECTED
} scanMngr_connStatus_e;




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

typedef struct  {
	TI_UINT8               	numOfChannels;                                  /**< Number of channels to scan */
    TScanChannelEntry      	channelEntry[ MAX_NUMBER_OF_CHANNELS_PER_SCAN ]; /**< Channel data array, actual size according to the above field. */
} channelList_t;

typedef struct _BssListEx_t
{
    bssList_t   *pListOfAPs;
    TI_BOOL      scanIsRunning;
} BssListEx_t;


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
 * \brief  Creates the scan manager object
 * 
 * \param hScanMngr - handle to the OS object
 * \return Handle to the Scan Manager Object on Success, NULL otherwise
 * 
 * \par Description
 * Creates (allocates) the scan manager object and its database
 * 
 * \sa
 */ 
TI_HANDLE scanMngr_create( TI_HANDLE hOS );
/** 
 * \brief  Initializes the scan manager
 * 
 * \param  pStadHandles  - The driver modules handles
 * \return void
 * 
 * \par Description
 * Initializes the scan manager object with other object handlers called by the driver core logic,
 * and creates Scan Manager Timer
 * 
 * \sa	scanMngr_unload
 */ 
void scanMngr_init (TStadHandlesList *pStadHandles);
/** 
 * \brief  Unloads the scan manager object
 * 
 * \param hScanMngr - Handle to the scan manager object to unload
 * \return void
 * 
 * \par Description
 * Frees the memory allocated by the scan manager. 
 * The function is called as part of the unloading process of the driver.
 * 
 * \sa
 */ 
void scanMngr_unload( TI_HANDLE hScanMngr );
/** 
 * \brief  Starts an immediate scan operation
 * 
 * \param hScanMngr 		- handle to the scan manager object
 * \param pNeighborAPsOnly 	- Indicates whether to scan only neighbor APs (or all channels defined by the policy)
 * 							  TI_TRUE if scan for neighbor APs only, TI_FALSE if scan on all channels
 * \return  Scan Result Status
 * 
 * \par Description
 * 
 * \sa	scanMngr_create
 */ 
scan_mngrResultStatus_e scanMngr_startImmediateScan( TI_HANDLE hScanMngr, TI_BOOL bNeighborAPsOnly );
/** 
 * \brief  Stops the immediate scan operation
 * 
 * \param hScanMngr - handle to the scan manager object
 * \return  void
 * 
 * \par Description
 * 
 * \sa
 */ 
void scanMngr_stopImmediateScan( TI_HANDLE hScanMngr );
/** 
 * \brief  Starts the continuous scan timer
 * 
 * \param hScanMngr 		- Handle to the scan manager object
 * \param currentBSS 		- BSSID of the AP to which the STA is connected 
 * \param currentBSSBand 	- Band of the AP to which the STA is connected 
 * \return  void
 * 
 * \par Description
 * Starts the continuous scan process performed by the scan manager. 
 * It is called by the roaming manager when an STA connection is established
 * 
 * \sa
 */ 
void scanMngr_startContScan( TI_HANDLE hScanMngr, TMacAddr* currentBSS, ERadioBand currentBSSBand );
/** 
 * \brief  Stops the continuous scan timer
 * 
 * \param hScanMngr 		- Handle to the scan manager object
 * \return  void
 * 
 * \par Description
 * Stops the continues scan performed by the scan manager. 
 * It is called by the roaming manager when the STA disconnects
 * 
 * \sa
 */ 
void scanMngr_stopContScan( TI_HANDLE hScanMngr );
/** 
 * \brief  Returns the currently available BSS list
 * 
 * \param hScanMngr 		- Handle to the scan manager object
 * \return  Pointer to BSS list
 * 
 * \par Description
 * Used by the roaming manager to obtain the scan manager BSS list.
 * 
 * \sa
 */
bssList_t *scanMngr_getBSSList( TI_HANDLE hScanMngr );
/** 
 * \brief  Sets the neighbor APs
 * 
 * \param hScanMngr 		- Handle to the scan manager object
 * \param neighborAPList 	- Pointer to the neighbor AP list
 * \return  void
 * 
 * \par Description
 * Used by the roaming manager to set a list of neighbor APs for the scan manager, 
 * which are then given priority over policy channels in the discovery phase
 * 
 * \sa
 */
void scanMngr_setNeighborAPs( TI_HANDLE hScanMngr, neighborAPList_t* neighborAPList );
/** 
 * \brief  Change quality level (normal / deteriorating)
 * 
 * \param hScanMngr 	- Handle to the scan manager object
 * \param bLowQuality 	- TI_TRUE if quality is deteriorating, TI_FALSE if quality is normal
 * \return  void
 * 
 * \par Description
 * Used by the roaming manager to set the scan interval used by the scan manager, 
 * according to the current AP quality level
 * 
 * \sa
 */
void scanMngr_qualityChangeTrigger( TI_HANDLE hScanMngr, TI_BOOL bLowQuality );
/** 
 * \brief  Change quality level (normal / deteriorating)
 * 
 * \param hScanMngr 	- Handle to the scan manager object
 * \param macAddress 	- MAC address of the new AP (to which the STA is currently connected to)
 * \param band 			- Band of the new AP (to which the STA is currently connected to) 
 * \return  void
 * 
 * \par Description
 * Used by the roaming manager to notify the scan manager that roaming is complete. 
 * This is done so that the scan manager does not attempt to discover the current AP, 
 * in case it is also a neighbor AP
 * 
 * \sa
 */
void scanMngr_handoverDone( TI_HANDLE hScanMngr, TMacAddr* macAddress, ERadioBand band );
/** 
 * \brief	Get Scan Manager Parameters  
 * 
 * \param hScanMngr - Handle to the scan manager object
 * \param pParam 	- Pointer to get Parameter
 * \return  TI_OK if the parameter got successfully, TI_NOK otherwise
 * 
 * \par Description
 * Parses and executes a get parameter command
 * 
 * \sa
 */
TI_STATUS scanMngr_getParam( TI_HANDLE hScanMngr, paramInfo_t *pParam );
/** 
 * \brief  Set Scan Manager Parameters
 * 
 * \param hScanMngr - Handle to the scan manager object
 * \param pParam 	- Pointer to set Parameter
 * \return  TI_OK if the parameter was set successfully, TI_NOK otherwise
 * 
 * \par Description
 * Called when the user configures scan parameters. It parses and executes a set parameter command
 * 
 * \sa
 */
TI_STATUS scanMngr_setParam( TI_HANDLE hScanMngr, paramInfo_t *pParam );





/********** New APIs added for EMP manual scan support ******/

void scanMngr_startManual(TI_HANDLE hScanMngr);

void scanMngr_stopManual(TI_HANDLE hScanMngr);

TI_STATUS scanMngr_setManualScanChannelList (TI_HANDLE  hScanMngr, channelList_t* pChannelList);

EScanCncnResultStatus scanMngr_Start1ShotScan (TI_HANDLE hScanMngr, EScanCncnClient eClient);

TI_STATUS scanMngr_immediateScanComplete(TI_HANDLE hScanMngr, scan_mngrResultStatus_e scanCmpltStatus);

TI_STATUS scanMngr_reportImmediateScanResults(TI_HANDLE hScanMngr, scan_mngrResultStatus_e scanCmpltStatus);

TI_STATUS scanMngr_startContinuousScanByApp (TI_HANDLE hScanMngr, channelList_t* pChannelList);

TI_STATUS scanMngr_stopContinuousScanByApp (TI_HANDLE hScanMngr);

void scanMngr_SetDefaults (TI_HANDLE hScanMngr, TRoamScanMngrInitParams *pInitParams);

/********** New APIs added for EMP manual scan support ******/


#ifdef TI_DBG
/** 
 * \brief  Print scan policy
 * 
 * \param scanPolicy 	- scan policy to print
 * \return void
 * 
 * \par Description
 * 
 * \sa
 */
void scanMngrTracePrintScanPolicy( TScanPolicy* scanPolicy );
/** 
 * \brief  Print scan manager statistics
 * 
 * \param hScanMngr - handle to the scan manager object.\n
 * \return void
 * 
 * \par Description
 * 
 * \sa
 */
void scanMngr_statsPrint( TI_HANDLE hScanMngr );
/** 
 * \brief  Reset scan manager statistics
 * 
 * \param hScanMngr - handle to the scan manager object.\n
 * \return void
 * 
 * \par Description
 * 
 * \sa
 */
void scanMngr_statsReset( TI_HANDLE hScanMngr );
/** 
 * \brief  Print Neighbor AP list
 * 
 * \param hScanMngr - handle to the scan manager object.\n
 * \return void
 * 
 * \par Description
 * 
 * \sa
 */
void scanMngrDebugPrintNeighborAPList( TI_HANDLE hScanMngr );
/** 
 * \brief  Prints all data in the scan manager object
 * 
 * \param hScanMngr - handle to the scan manager object.\n
 * \return void
 * 
 * \par Description
 * 
 * \sa
 */
void scanMngrDebugPrintObject( TI_HANDLE hScanMngr );

#endif /* TI_DBG */

#endif /* __SCAN_MNGR_API_H__ */
