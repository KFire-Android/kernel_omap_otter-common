/*
 * roamingMngrApi.h
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

/** \file roamingMngrApi.h
 *  \brief Internal Roaming Manager API
 *
 *  \see roamingMngr.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Roaming Manager	    		                                *
 *   PURPOSE: Roaming Manager Module API                              		*
 *                                                                          *
 ****************************************************************************/

#ifndef _ROAMING_MNGR_API_H_
#define _ROAMING_MNGR_API_H_

/*#include "802_11Defs.h"*/
#include "osApi.h"
#include "paramOut.h"
#include "scanMngrApi.h"
#include "bssTypes.h"
#include "DrvMainModules.h"
#include "apConnApi.h"
/*-----------*/
/* Constants */
/*-----------*/

#define MAX_ROAMING_TRIGGERS  ROAMING_TRIGGER_LAST


/*--------------*/
/* Enumerations */
/*--------------*/

/* Roaming Trigger groups, according to Roaming Triggers */
typedef enum
{
    ROAMING_TRIGGER_BG_SCAN_GROUP 		= ROAMING_TRIGGER_NORMAL_QUALITY_FOR_BG_SCAN,
    ROAMING_TRIGGER_LOW_QUALITY_GROUP 	= ROAMING_TRIGGER_MAX_TX_RETRIES,
    ROAMING_TRIGGER_FAST_CONNECT_GROUP 	= ROAMING_TRIGGER_SWITCH_CHANNEL,
    ROAMING_TRIGGER_FULL_CONNECT_GROUP 	= ROAMING_TRIGGER_SECURITY_ATTACK
} roamingMngr_connectTypeGroup_e;


/*----------*/
/* Typedefs */
/*----------*/

/* scan types */
typedef enum
{
/*	0	*/	ROAMING_NO_SCAN,
/*	1	*/	ROAMING_PARTIAL_SCAN,
/*	2	*/	ROAMING_PARTIAL_SCAN_RETRY,
/*	3	*/	ROAMING_FULL_SCAN,
/*	4	*/	ROAMING_FULL_SCAN_RETRY

} scan4RoamingType_e;

typedef struct
{
    TI_UINT8   preAuthBSSList[MAX_SIZE_OF_BSS_TRACK_LIST];
    TI_UINT8   numOfPreAuthBSS;
    TI_UINT8   neighborBSSList[MAX_SIZE_OF_BSS_TRACK_LIST];
    TI_UINT8   numOfNeighborBSS;
    TI_UINT8   regularBSSList[MAX_SIZE_OF_BSS_TRACK_LIST];
    TI_UINT8   numOfRegularBSS;
} listOfCandidateAps_t;


struct _roamingMngr_t
{
    /*** Roaming manager parameters that can be configured externally ***/
    roamingMngrConfig_t         	roamingMngrConfig;
    roamingMngrThresholdsConfig_t   roamingMngrThresholdsConfig;
    TI_UINT32                      	lowPassFilterRoamingAttemptInMsec;

    /*** Internal roaming parameters ***/
    apConn_roamingTrigger_e     	roamingTrigger;				/* the roaming trigger type */
    TI_UINT32*                      pCurrentState;				/* pointer to Roaming Generic SM current state */
    TI_BOOL                        	maskRoamingEvents;			/* Indicate if a trigger is already in process, and therefore the
																	other triggers will be ignored */
    TI_UINT32                      	lowQualityTriggerTimestamp;	/* TS to filter Too many low Quality roaming triggers */
    scan4RoamingType_e          	scanType; 					/* the scan type performed for Roaming */
    bssList_t                   	*pListOfAPs;				/* list of BSS received from Scan Manager */
    TI_BOOL                        	neighborApsExist;			/* Indicating if Neighbor APs exist */
    listOfCandidateAps_t        	listOfCandidateAps;			/* a list of the candiadte APs indexes in pListOfAPs according to
																	neighbor APs, pre-auth APs and other APs */
    TI_UINT8                       	candidateApIndex;			/* The current candidate AP's index to Roam to */
    TI_BOOL                        	handoverWasPerformed;		/* Indicates whether at least one handover was performed */
    apConn_staCapabilities_t    	staCapabilities;
    	/* The station capabilities for the current Connection */
    TI_HANDLE          	            hRoamingSm;				    /* Roaming manager SM handle */
    TI_INT8**                       RoamStateDescription;       /* Roaming states index-name keyValue */
    TI_INT8**                       RoamEventDescription;       /* Roaming Events index-name keyValue */


    /* Roaming manager handles to other objects */
    TI_HANDLE                   	hReport;
    TI_HANDLE                   	hOs;
    TI_HANDLE                   	hScanMngr;
    TI_HANDLE                   	hAPConnection;
    TI_HANDLE                   	hTWD;
    TI_HANDLE                   	hEvHandler;
    TI_HANDLE                   	hCurrBss;

#ifdef TI_DBG
    /* Debug trace for Roaming statistics */
    TI_UINT32                      roamingTriggerEvents[MAX_ROAMING_TRIGGERS];
    TI_UINT32                      roamingHandoverEvents[MAX_ROAMING_TRIGGERS];
    TI_UINT32                      roamingSuccesfulHandoverNum;
    TI_UINT32                      roamingFailedHandoverNum;
    TI_UINT32                      roamingTriggerTimestamp;
    TI_UINT32                      roamingHandoverStartedTimestamp;
    TI_UINT32                      roamingHandoverCompletedTimestamp;
    TI_UINT32                      roamingAverageSuccHandoverDuration;
    TI_UINT32                      roamingAverageRoamingDuration;
#endif

    TI_UINT8	                   RoamingOperationalMode; /* 0 - manual, 1 - auto*/
    TI_UINT8                       bSendTspecInReassPkt;   
    TargetAp_t                     targetAP;               /* holds the AP to connect with in manual mode */
}; /* _roamingMngr_t */



typedef struct _roamingMngr_t   roamingMngr_t;

/*------------*/
/* Structures */
/*------------*/

/*---------------------------*/
/* External data definitions */
/*---------------------------*/

/*--------------------------------*/
/* External functions definitions */
/*--------------------------------*/

/*----------------------------*/
/* Global Function prototypes */
/*----------------------------*/

/**
 * \brief Create the Roaming Manager context
 * 
 * \param  hOs	  	- OS handler
 * \return	A pointer to the roaming manager handler on success, 
 * 			NULL on failure (unable to allocate memory or other system resource) 
 * 
 * \par Description
 * Creates the Roaming Manager context: \n
 * Allocate control block for preconfigured parameters and internal variables, create state-machine
 * 
 * \sa	roamingMngr_unload
 */ 
TI_HANDLE roamingMngr_create(TI_HANDLE hOs);
/**
 * \brief Configure the Roaming Manager module
 * 
 * \param  pStadHandles	  	- The driver modules handles
 * \return void 
 * 
 * \par Description
 * Configure the Roaming Manager module to do the following:
 * - Initialize the Station broadcast key State Machine matrix
 * - Store handlers of other modules (report module, Scan Manager, AP connection, TWD)
 * - Initialize the roaming manager internal variables
 * - Configure the roaming manager state-machine
 * 
 * \sa	
 */ 
void roamingMngr_init (TStadHandlesList *pStadHandles);
/**
 * \brief Unloads the Roaming Manager Module
 * 
 * \param  hRoamingMngr - Roaming Manager handler
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Unloads the components of the Roaming Manager module from memory:	\n 
 * releases the allocation for the state-machine and internal variables
 * 
 * \sa	roamingMngr_create
 */ 

TI_STATUS roamingMngr_unload(TI_HANDLE hRoamingMngr);
/**
 * \brief Get Roaming Manager parameters from the roamingMngr SM
 * 
 * \param  hRoamingMngr - Roaming Manager handler
 * \param  pParam 		- Output Parameters
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS roamingMngr_getParam(TI_HANDLE hRoamingMngr, paramInfo_t *pParam);
/**
 * \brief Set Roaming Manager Module parameters to the roamingMngr SM
 * 
 * \param  hRoamingMngr - Roaming Manager handler
 * \param  pParam 		- Input Parameters
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS roamingMngr_setParam(TI_HANDLE hRoamingMngr, paramInfo_t *pParam);
/**
 * \brief  Indicates Roaming Manager that an Immediate Scan was completed 
 *			and provides it with the Scan result
 * 
 * \param  hRoamingMngr  	- Handle to the roaming manager
 * \param  scanCmpltStatus	- Scan complete reason
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * This procedure is called when the Scan Manager completed Immediate Scan for Roaming.
 * It performs the following:
 * - Partial or Full scan
 * - Re-try Partial or Full scan if the previous scan failed
 * - Full scan if the previous partial scan didn't get any APs
 * - Fail event if all the Scans failed
 * 
 * Algorithm description:
 * - If Scan Complete is OK: 
 * -------------------------
 *		- If APs found:
 * 			- starts Selection
 *		- If NO APs found:
 * 			- If Previous Scan was Partial:
 *				- Perform Full Scan
 *			- If Previous Scan was Full:
 *				- Report Error
 *
 * - If Scan Complete is NOT OK: 
 * -----------------------------
 * - Re-Try Scan
 *		- If APs found:
 * 			- starts Selection
 * 		- If NO APs found:
 *			- Re-Try Scan with current Scan Type (Partial/Full Scan Retry or No Scan)
 * 
 * \sa
 */ 
TI_STATUS roamingMngr_immediateScanComplete(TI_HANDLE hRoamingMngr, scan_mngrResultStatus_e scanCmpltStatus);
/**
 * \brief  Indicates that a new BSSID is added to the BSS table
 * 
 * \param  hRoamingMngr  	- Handle to the roaming manager
 * \param  newBss_entry	  	- List of BSSIDs that have been found
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Indicates that a new BSSID is added to the BSS table (Called by the Scan Manager when new BSSID was found). 
 * This function triggers preauthentication to the new BSS.
 * 
 * \sa
 */ 
TI_STATUS roamingMngr_updateNewBssList(TI_HANDLE hRoamingMngr, bssList_t *newBss_entry);



/* All functions below added by Lior*/

TI_STATUS roamingMngr_setDefaults (TI_HANDLE hRoamingMngr, TRoamScanMngrInitParams *pInitParam);
TI_STATUS roamingMngr_setBssLossThreshold (TI_HANDLE hRoamingMngr, TI_UINT32 uNumOfBeacons, TI_UINT16 uClientID);
TI_STATUS roamingMngr_connect(TI_HANDLE hRoamingMngr, TargetAp_t* pTargetAp);
TI_STATUS roamingMngr_startImmediateScan(TI_HANDLE hRoamingMngr, channelList_t *pChannelList);
TI_STATUS roamingMngr_stopImmediateScan(TI_HANDLE hRoamingMngr);
TI_STATUS roamingMngr_immediateScanByAppComplete(TI_HANDLE hRoamingMngr, scan_mngrResultStatus_e scanCmpltStatus);

TI_STATUS roamingMngr_smEvent(TI_UINT8 event, void* data);
void roamingMngr_smStopWhileScanning(void *pData);
void roamingMngr_smStop(void *pData);
void roamingMngr_smUnexpected(void *pData);
void roamingMngr_smNop(void *pData);


#endif /*  _ROAMING_MNGR_API_H_*/

