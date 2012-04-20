/*
 * apConnApi.h
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

/** \file apConnApi.h
 *  \brief AP Connection Module API
 *
 *  \see apConn.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  AP Connection                                             	*
 *   PURPOSE: AP Connection Module API                                      *
 *                                                                          *
 ****************************************************************************/

#ifndef _AP_CONNECTION_API_H_
#define _AP_CONNECTION_API_H_

#include "paramOut.h"
#include "rsnApi.h"
#include "roamingMngrTypes.h"

/*-----------*/
/* Constants */
/*-----------*/

#define AP_CONNECT_TRIGGER_IGNORED  0x0

/*--------------*/
/* Enumerations */
/*--------------*/

/** \enum apConn_connRequest_e
 * \brief	Connect to AP request types
 * 
 * \par Description
 * Lists the possible types of connection to AP requests 
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	AP_CONNECT_RETAIN_CURR_AP, 	 	/**< Remain connected to the current AP without authentication.
											* Give-up on roaming, return to current AP without performing re-connection 
											*/
/*	1	*/	AP_CONNECT_RECONNECT_CURR_AP,   /**< Reconnect to the current AP with fast authentication.
											* Perform roaming - connect to AP, registered as current AP 
											*/
/*	2	*/	AP_CONNECT_FAST_TO_AP,          /**< Roam to AP with fast authentication.
											* Perform roaming - re-connect to new AP via RE-Assoc, parameters attached 
											*/
/*	3	*/	AP_CONNECT_FULL_TO_AP           /**< Roam to AP with full authentication.
											* Perform full connection - connect to new AP via Assoc, parameters attached 
											*/

} apConn_connRequest_e;

/** \enum apConn_roamingTrigger_e
 * \brief	Roaming Triggers Types
 * 
 * \par Description
 * Lists the possible types of roaming triggers
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	ROAMING_TRIGGER_NONE,							/**< No roaming trigger	*/

/*	1	*/	ROAMING_TRIGGER_LOW_QUALITY_FOR_BG_SCAN,		/**< Low quality trigger for background scan	*/
/*	2	*/	ROAMING_TRIGGER_NORMAL_QUALITY_FOR_BG_SCAN,		/**< Normal quality trigger for background scan	*/

/*	3	*/	ROAMING_TRIGGER_LOW_TX_RATE,					/**< Low TX rate	*/
/*	4	*/	ROAMING_TRIGGER_LOW_SNR,						/**< Low SNR rate 	*/
/*	5	*/	ROAMING_TRIGGER_LOW_QUALITY,					/**< Low quality for roaming	*/

/*	6	*/	ROAMING_TRIGGER_MAX_TX_RETRIES,					/**< Maximum TX retries	*/
/*	7	*/	ROAMING_TRIGGER_BSS_LOSS,						/**< Missed beacon and no ACK on Unicast probe requests	*/
/*	8	*/	ROAMING_TRIGGER_SWITCH_CHANNEL,					/**< Radar detection	*/

/*	9	*/	ROAMING_TRIGGER_AP_DISCONNECT, 					/**< AP disconnect (de-authenticate or disassociate)	*/
/*	10	*/	ROAMING_TRIGGER_SECURITY_ATTACK,				/**< Security attack	*/
/*	11	*/	ROAMING_TRIGGER_TSPEC_REJECTED,	                /**< TSPEC Rejected	*/
    
/*	12	*/	ROAMING_TRIGGER_LAST							/**< Maximum roaming trigger - must be last!!!	*/

} apConn_roamingTrigger_e;

/** \enum apConn_connStatus_e
 * \brief	Connection Status Types
 * 
 * \par Description
 * Lists the possible types of connection status
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	CONN_STATUS_CONNECTED,				/**< */
/*	1	*/	CONN_STATUS_NOT_CONNECTED,			/**< */
/*	2	*/	CONN_STATUS_HANDOVER_FAILURE,		/**< */
/*	3	*/	CONN_STATUS_HANDOVER_SUCCESS,		/**< */

/*	4	*/	CONN_STATUS_LAST					/**< Maximum connection status - must be last!!!	*/

} apConn_connStatus_e;

/** \enum REG_DOMAIN_CAPABILITY
 * \brief	Regulatory Domain Capability  
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	REG_DOMAIN_FIXED,	/**< */
/*	1	*/	REG_DOMAIN_80211D,	/**< */  	  
/*	2	*/	REG_DOMAIN_80211H	/**< */

} REG_DOMAIN_CAPABILITY;


/*----------*/
/* Typedefs */
/*----------*/
/**
 * \brief	Roaming Manager callback type 
 * 
 * \param  hRoamingMngr	- Handle to Roaming Manager Object
 * \param  pData		- Pointer to Data
 * \return void 
 * 
 * \par Description
 *
 * 
 * \sa 	
 */ 
typedef TI_STATUS (*apConn_roamMngrCallb_t) (TI_HANDLE hRoamingMngr, void *pData);

typedef TI_STATUS (*apConn_roamMngrEventCallb_t) (TI_HANDLE hRoamingMngr, void *pData, TI_UINT16 reasonCode);

/*------------*/
/* Structures */
/*------------*/

/** \struct apConn_staCapabilities_t
 * \brief Station Capabilities
 * 
 * \par Description
 * Holds a Station Capabilities.
 * Used by the roaming manager when the capabilities of a new AP 
 * are transferred to the AP connection module
 * 
 * \sa	
 */
typedef struct _apConn_staCapabilities_t
{
    OS_802_11_AUTHENTICATION_MODE   	authMode; 			/**< Authentication Mode: None, Shared, AutoSwitch, WPA, WPAPSK, WPANone, WPA2, WPA2PSK	*/     
    OS_802_11_ENCRYPTION_TYPES      	encryptionType; 	/**< Encription Type: None, WEP, TKIP, AES */    
    OS_802_11_NETWORK_TYPE          	networkType;  		/**< Network Type: 2.4G, 5G or Dual	*/    
    OS_802_11_RATES_EX              	rateMask;  			/**< An array of 16 octets. Each octet contains a preferred data rate in units of 0.5 Mbps */    
    TI_BOOL                            	XCCEnabled; 		/**< TI_TRUE - XCC enabled, TI_FALSE - XCC disabled	*/    
    TI_BOOL                            	qosEnabled; 		/**< TI_TRUE - QOS enabled, TI_FALSE - QOS disabled */   
    REG_DOMAIN_CAPABILITY           	regDomain;  		/**< Fixed, 802.11D, 802.11H */

} apConn_staCapabilities_t;

/** \struct apConn_connStatus_t
 * \brief	Connection Status 
 * 
 * \par Description
 * Holds the Report Status of the Connection and a buffer for the case 
 * of vendor specific IEs in Association Response Packet
 * 
 * \sa	
 */
typedef struct _apConn_connStatus_t
{
    apConn_connStatus_e     		status;         /** Reported status of the connection */
    TI_UINT32                  		dataBufLength;  /** (Optional) length of attached buffer */
    TI_CHAR                    		*dataBuf;       /** (Optional) attached buffer - can be used in case of vendor specific IEs in Association Response Packet */
} apConn_connStatus_t;

/** \struct apConn_connRequest_t
 * \brief	Connection Request 
 * 
 * \par Description
 * Holds the Type of Request needed to establish the Connection, 
 * and a buffer for the case of vendor specific IEs in Association Request Packet
 * 
 * \sa	
 */
typedef struct _apConn_connRequest_t
{
    apConn_connRequest_e    		requestType;    /** Type of Request to establish connection */
    TI_UINT32                  		dataBufLength;  /** (Optional) length of attached buffer */
    TI_CHAR                    		*dataBuf;       /** (Optional) attached buffer - can be used in case of vendor specific IEs in Association Request Packet */
} apConn_connRequest_t;


typedef enum
{
	ReAssoc = 0
	/* The methods below are for 11r support only
	FT_initial_ReAssoc,
    FT_over_the_air,
    FT_over_DS */
} ETransitionMethod;


typedef struct _TargetAP_t
{
    apConn_connRequest_t    connRequest;
	ETransitionMethod 	    transitionMethod;
	bssEntry_t		        newAP;
} TargetAp_t;

/*---------------------------*/
/* External data definitions */
/*---------------------------*/

/*--------------------------------*/
/* External functions definitions */
/*--------------------------------*/

/*---------------------*/
/* Function prototypes */
/*---------------------*/
/**
 * \brief	Configure Roaming Thresholds
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  pParam  			- Pointer to input Roaming event thresholds to configure 
 * \return TI_OK on success
 * 
 * \par Description
 * This function is used for configuring Roaming Thesholds: 
 * it is called by Roaming Manager when the application sets roaming thresholds
 * 
 * \sa
 */
TI_STATUS apConn_setRoamThresholds(TI_HANDLE hAPConnection, roamingMngrThresholdsConfig_t *pParam);
/**
 * \brief	Get Roaming Thresholds configuration
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  pParam  			- Pointer to output Roaming event thresholds configuration to get 
 * \return TI_OK on success
 * 
 * \par Description
 * This function is used for getting current Roaming Thesholds configuration: 
 * it is called by Roaming Manager when the application gets roaming thresholds
 * 
 * \sa
 */
TI_STATUS apConn_getRoamThresholds(TI_HANDLE hAPConnection, roamingMngrThresholdsConfig_t *pParam);
/**
 * \brief	Register Roaming Manager Callbacks
 * 
 * \param  hAPConnection   			- Handle to AP Connection module object
 * \param  roamEventCallb  			- Pointer to input Roaming event callback 
 * \param  reportStatusCallb  		- Pointer to input Connection status callback 
 * \param  returnNeighborApsCallb  	- Pointer to input Neighbor AP callback 
 * \return TI_OK on success
 * 
 * \par Description
 * This function stores the roaming manager methods in the internal database of the AP connection module
 * 
 * \sa
 */
TI_STATUS apConn_registerRoamMngrCallb(TI_HANDLE hAPConnection, 
                                       apConn_roamMngrEventCallb_t  roamEventCallb,
                                       apConn_roamMngrCallb_t       reportStatusCallb,
                                       apConn_roamMngrCallb_t       returnNeighborApsCallb);
/**
 * \brief	Un-Register Roaming Manager Callbacks
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \return TI_OK on success
 * 
 * \par Description
 * This function erases the roaming manager methods from the internal database of the AP connection module. 
 * From the moment this function was called, any roaming event would cause disconnect.
 * 
 * \sa
 */
TI_STATUS apConn_unregisterRoamMngrCallb(TI_HANDLE hAPConnection);
/**
 * \brief	AP-Connection disconnect  
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \return TI_OK on success
 * 
 * \par Description
 * Roaming Manager calls this function when it fails to find a candidate 
 * for roaming and connection with the current AP is not possible
 * 
 * \sa
 */
TI_STATUS apConn_disconnect(TI_HANDLE hAPConnection);
/**
 * \brief	Get Station Capabilities  
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  ie_list   		- Pointer to output list of STA Capabilities
 * \return TI_OK on success
 * 
 * \par Description
 * Roaming Manager calls this function during selection of new candidate for roaming.
 * The local STA capabilities (IEs) of quality and privacy, which are required for 
 * evaluating AP sites suitable for roaming, are returned.
 * 
 * \sa
 */
TI_STATUS apConn_getStaCapabilities(TI_HANDLE hAPConnection,
                                    apConn_staCapabilities_t *ie_list);

/**
 * \brief	Connect to New AP
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  newAP   			- Pointer to input parameters list of AP candidates for roaming
 * \param  request   		- The Connection Request Type: Connect to new AP, current AP (retain to current AP or re-connect)
 * \param  reNegotiateTspec	- Set to TRUE before handover if requested by roaming manager
 * \return TI_OK on success
 * 
 * \par Description
 * Roaming Manager calls this function when it has completed the process of selection a new AP candidate for roaming. 
 * In addition, the Roaming manager informs the AP Connection module whether this is new AP or the current one, 
 * and whether to perform full roaming procedure or just to retain to current AP.
 * Precondition: ready for roaming
 * 
 * \Note
 * Calling this function with the parameter RETAIN_CURR_AP terminates the roaming process at the stage when the driver 
 * resources are occupied for roaming, but the connection with the current AP is still valid
 * \sa
 */
TI_STATUS apConn_connectToAP(TI_HANDLE hAPConnection,
                             bssEntry_t *newAP,
                             apConn_connRequest_t *request,
                             TI_BOOL reNegotiateTspec);

/**
 * \brief	Connect to New AP
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \return Pointer to BSS entry parameters if successful, NULL otherwise
 * 
 * \par Description
 * Roaming Manager calls this function in order to evaluate the quality of new
 * AP candidates for roaming against the quality of current AP.
 * The function returns parameters of current AP.
 * 
 * \sa
 */
bssEntry_t *apConn_getBSSParams(TI_HANDLE hAPConnection);
/**
 * \brief	Check if Site is Banned
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  bssid   			- Pointer to input BSSID (MAC Address) of checked site
 * \return True if the site is banned and Falde otherwise 
 * 
 * \par Description
 * Roaming Manager calls this function during selection of new candidate for roaming.
 * Only APs which are not marked as banned are allowed to be selected. 
 * 
 * \sa
 */
TI_BOOL apConn_isSiteBanned(TI_HANDLE hAPConnection, TMacAddr * bssid);
/**
 * \brief	Get AP Pre-Authentication Status
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  givenAp   		- Pointer to input BSSID (MAC Address) of checked AP
 * \return True if AP is pre-authenticated, False otherwise
 * 
 * \par Description
 * Roaming Manager calls this function during selection of new candidate for roaming.
 * Among all candidate APs of acceptable quality that are not neighbor APs, 
 * pre-authenticated APs are preferred.
 * The function returns the status of the AP, whether or not it is pre-authenticated. 
 * 
 * 
 * \sa
 */
TI_BOOL apConn_getPreAuthAPStatus(TI_HANDLE hAPConnection, TMacAddr *givenAp);
/**
 * \brief	Update Pre-Authentication APs List
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  listAPs   		- Pointer to List of APs to pre-authenticate
 * \return TI_OK if the parameter was set successfully, TI_NOK otherwise
 * 
 * \par Description
 * Roaming Manager calls this function periodically in order to update the list
 * of pre-authenticated APs. The list is managed by WLAN driver and required
 * by Roaming Manager during selection of roaming candidate process.  
 * 
 * \sa
 */
TI_STATUS apConn_preAuthenticate(TI_HANDLE hAPConnection, bssList_t *listAPs);
/**
 * \brief	Prepare to Roaming
 * 
 * \param  hAPConnection   	- Handle to AP Connection module object
 * \param  reason   		- The reason for Roaming trigger
 * \return TI_OK if the parameter was set successfully, TI_NOK otherwise
 * 
 * \par Description
 * This function indicate the driver that Roaming process is starting. 
 * The roaming manager calls this function when roaming event 
 * is received and Roaming Manager wishes to start the roaming process, thus wants to 
 * prevent scan and measurement in the system. 
 * The function will return TI_OK if the allocation is performed, PEND if the allocation is started, 
 * TI_NOK in case the allocation is not possible. In case of successful allocation, 
 * Roaming Manager will be informed via callback about the end of the allocation. 
 * The roaming manager can initiate an immediate scan or call to connect to an AP only 
 * after prepare-to-roam is complete 
 * 
 * \sa
 */

TI_STATUS apConn_prepareToRoaming(TI_HANDLE hAPConnection, apConn_roamingTrigger_e reason);

TI_STATUS apConn_getAssocRoamingTrigger(TI_HANDLE hAPConnection, apConn_roamingTrigger_e *asssocRoamingTrigger);

#endif /*  _AP_CONNECTION_API_H_*/

