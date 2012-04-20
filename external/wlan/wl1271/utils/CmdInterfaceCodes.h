/*
 * CmdInterfaceCodes.h
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


/****************************************************************************/
/*																			*/
/*    MODULE:   CmdInterfaceCodes.h											*/
/*    PURPOSE:																*/
/*																			*/
/****************************************************************************/
#ifndef _CMD_INTERFACE_CODES_H_
#define _CMD_INTERFACE_CODES_H_

/** \file  CmdInterfaceCodes.h 
 * \brief Command Interface Codes 
 * \n\n
 * This file contains the definitions for the parameters that can be Set/Get from outside user.
 * \n\n
 */

#include "TWDriver.h"

/* defines */
/***********/

/* types */
/*********/


/* This file contains the definitions for the parameters that can be Set/Get from outside.
    The parmeters that can be Set/Get from inside the driver only are defined in the file paramIn.h */

/****************************************************************************
                                PARAMETERS ISSUE
    Each parameter in the system is defined as UINT32. The parameter
    structue is as following:

 bit   31   30 - 24     23    22 - 16    15 - 8       7 - 0
    +-----+----------+-----+----------+-----------+-----------+
    | Set | Reserved | Get | Reserved | Module    | Parameter |
    | bit |          | bit |          | number    | number    |
    +-----+----------+-----+----------+-----------+-----------+

  The 'set' bit indicates whteher this parameter can be set from OS abstraction layer.
  The 'get' bit indicates whteher this parameter can be get from OS abstraction layer.
  (All the parameters can be Get/Set from insied the driver.)
  The module number indicated who is the oner of the parameter.
  The parameter number is the parameter unique number used to identify it.

****************************************************************************/

/** \def SET_BIT
 * \brief Bitmaks of bit which indicates if the Command is SET Command
 */
#define	SET_BIT         			0x08000000
/** \def GET_BIT
 * \brief Bitmaks of bit which indicates if the Command is GET Command
 */
#define	GET_BIT				        0x00800000
/** \def ASYNC_PARAM
 * \brief Bitmaks of bit which indicates if the access to the Command Parameter is Async
 */
#define ASYNC_PARAM					0x00010000
/** \def ALLOC_NEEDED_PARAM
 * \brief Bitmaks of bit which indicates if that the data is not allocated in the paramInfo structure
 */
#define ALLOC_NEEDED_PARAM			0x00020000


/** \def GET_PARAM_MODULE_NUMBER
 * \brief Macro which gets the Parameter's Module Number from the second byte of x \n
 * x should be taken from Module Parameters Enumeration
 * sa EModuleParam
 */
#define GET_PARAM_MODULE_NUMBER(x)  ((x & 0x0000FF00) >> 8)
/** \def IS_PARAM_ASYNC
 * \brief Macro which returns True if access to the Command Parameter is Async \n
 * Otherwise returns False
 */
#define IS_PARAM_ASYNC(x)			(x & ASYNC_PARAM)
/** \def IS_ALLOC_NEEDED_PARAM
 * \brief Macro which returns True if data is not allocated in the paramInfo structure \n
 * (there is a need to allocate memory for data). Otherwise returns False
 */
#define IS_ALLOC_NEEDED_PARAM(x)	(x & ALLOC_NEEDED_PARAM)	
/** \def IS_PARAM_FOR_MODULE
 * \brief Macro which returns True if input param is for input module. \n
 * Otherwise returns False
 */
#define IS_PARAM_FOR_MODULE(param, module)   ((param & 0x0000FF00) == module)

/** \enum EModuleParam
 * \brief Modules Parameters ID
 * 
 * \par Description
 * This Enumeration defines all available Modules numbers. \n
 * Note that the actual number is held in the second byte (E.g. 0x0000FF00). \n
 * According to these numbers it is decided to which Module the Command Parameter is destined
 * 
 * \sa
 */
/* NOTICE! whenever you add a module, you have to increment MAX_PARAM_MODULE_NUMBER as well!!! */
typedef enum
{
    DRIVER_MODULE_PARAM               	= 0x0000,	/**< Driver Module Number							*/
    AUTH_MODULE_PARAM               	= 0x0100,	/**< Authentication Module Number					*/
    ASSOC_MODULE_PARAM              	= 0x0200,	/**< Association Module Number	   					*/
    RX_DATA_MODULE_PARAM            	= 0x0300,	/**< RX Data Module Number							*/
    TX_CTRL_MODULE_PARAM            	= 0x0400,	/**< TX Control Module Number						*/
    CTRL_DATA_MODULE_PARAM          	= 0x0500,	/**< Control Data Module Number						*/
    SITE_MGR_MODULE_PARAM           	= 0x0600,	/**< Site Manager Module Number						*/
    CONN_MODULE_PARAM               	= 0x0700,	/**< Connection Module Number						*/
    RSN_MODULE_PARAM                	= 0x0800,	/**< Robust Security NW (RSN) Module Number			*/
    ADM_CTRL_MODULE_PARAM           	= 0x0900,	/**< ADM Control Module Number						*/
    TWD_MODULE_PARAM                	= 0x0A00,	/**< Report Module Number							*/
    REPORT_MODULE_PARAM             	= 0x0B00,	/**< Report Module Number							*/
    SME_MODULE_PARAM                    = 0x0C00,	/**< SME Module Number								*/
    MLME_SM_MODULE_PARAM            	= 0x0D00,	/**< 802.11 MLME State-Machine Module Number  		*/
    REGULATORY_DOMAIN_MODULE_PARAM  	= 0x0E00,	/**< Regulatory Domain Module Number 				*/
    MEASUREMENT_MODULE_PARAM        	= 0x0F00,	/**< Measurement Module Number						*/
    XCC_MANAGER_MODULE_PARAM        	= 0x1000,	/**< XCC Manager Module Number 						*/
    ROAMING_MANAGER_MODULE_PARAM    	= 0x1100,	/**< Roaming Manager Module Number					*/
    SOFT_GEMINI_PARAM               	= 0x1200,	/**< Soft Gemini Module Number						*/
    QOS_MANAGER_PARAM               	= 0x1300,	/**< Quality Of Service (QoS) Manager Module Number	*/
    POWER_MANAGER_PARAM             	= 0x1400,	/**< Power Manager Module Number					*/
    SCAN_CNCN_PARAM                 	= 0x1500,	/**< Scan Concentrator Module Number				*/
    SCAN_MNGR_PARAM                 	= 0x1600,	/**< Scan Manager Module Number						*/
    MISC_MODULE_PARAM					= 0x1700,	/**< Misc. Module Number							*/
    HEALTH_MONITOR_MODULE_PARAM         = 0x1800,	/**< Health Monitor Module Number					*/
    CURR_BSS_MODULE_PARAM               = 0x1900,   /**< Current Bss Module Number	     		        */
    /*
    Last module - DO NOT TOUCH!
    */
    MODULE_PARAM_LAST_MODULE						/**< LAst Module - Dummy, mast be last				*/

}   EModuleParam;

/** \def MAX_PARAM_MODULE_NUMBER
 * \brief Macro which returns the number of Parameters Modules
 */
#define MAX_PARAM_MODULE_NUMBER             (GET_PARAM_MODULE_NUMBER(MODULE_PARAM_LAST_MODULE))



/** \enum EExternalParam
 * \brief External Parameters
 * 
 * \par Description
 * This Enumeation includes all the eaxternal parameters numbers which are used for Get/Set Commands. 
 * Each module can have 256 parameters	\n
 * PARAMETERS ISSUE:	\n
 * Each parameter in the system is defined as UINT32. The parameter structue is as following:	\n
 * bit 0 - 7: 		Parameter Number - number of parameter inside Module\n
 * bit 8 - 15:		Module number - number of Module\n
 * bit 16:			Async Bit - indicates if command is Async (ON) or Sync (OFF)\n
 * bit 17:			Allocate Bit - indicates if allocation should be done for parameter (ON) or not (OFF)\n
 * bit 18 - 22:		Reserved			\n
 * bit 23:			Get Bit	- indicates if command is Get (ON) or not (OFF)	\n
 * bit 24 - 26:		Reserved	\n
 * bit 27:			Set Bit	- indicates if command is Set (ON) or not (OFF)	\n
 * bit 28 - 31:		Reserved	\n\n
 * The 'set' bit indicates whteher this parameter can be set from OS abstraction layer.
 * The 'get' bit indicates whteher this parameter can be get from OS abstraction layer.
 * (All the parameters can be Get/Set from insied the driver.)
 * The module number indicated who is the oner of the parameter.
 * The parameter number is the parameter unique number used to identify it.
 *
 * \sa
 */
 /* bit | 31 - 28  | 27  | 26 - 24  |  23 | 22 - 18  |    17    |  16   |  15 - 8   |   7 - 0   |
 *      +----------+-----+----------+-----+----------+----------+-------+-----------+-----------+
 *      | Reserved | Set | Reserved | Get | Reserved | Allocate | Async | Module    | Parameter |
 *      |          | bit |          | bit |          |    bit   |  bit  | number    | number    |
 *      +----------+-----+----------+-----+----------+----------+-------+-----------+-----------+
 */
typedef enum
{
	/* Driver General section */
    DRIVER_INIT_PARAM                           =	SET_BIT |           DRIVER_MODULE_PARAM | 0x00,	/**< Driver Init Parameter (Driver General Set Command): \n
																									* Used for setting driver defaults. Done Sync with no memory allocation\n 
																									* Parameter Number:	0x00\n
																									* Module Number: Driver Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
    DRIVER_START_PARAM                          =	SET_BIT |           DRIVER_MODULE_PARAM | 0x01, /**< Driver Start Parameter (Driver General Set Command): \n
																									* Used for Starting Driver. Done Sync with no memory allocation\n 
																									* Parameter Number:	0x01\n
																									* Module Number: Driver Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
    DRIVER_STOP_PARAM                           =	SET_BIT |           DRIVER_MODULE_PARAM | 0x02, /**< Driver Stop Parameter (Driver General Set Command): \n
																									* Used for Stopping Driver. Done Sync with no memory allocation \n
																									* Parameter Number:	0x02\n
																									* Module Number: Driver Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
    DRIVER_STATUS_PARAM                         =	          GET_BIT | DRIVER_MODULE_PARAM | 0x03, /**< Driver Status Parameter (Driver General Get Command): \n
																									* Used for Getting Driver's Status (if running). Done Sync with no memory allocation\n																														Done Sync with no memory allocation\n 
																									* Parameter Number:	0x03\n
																									* Module Number: Driver Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: OFF	\n
																									*/
    DRIVER_THREADID_PARAM                       =	          GET_BIT | DRIVER_MODULE_PARAM | 0x04, /**< Driver Thread ID Parameter (Driver General Get Command): \n
																									* Used for Getting Driver's Thread ID. Done Sync with no memory allocation\n 
																									* Parameter Number:	0x04\n
																									* Module Number: Driver Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: OFF	\n
																									*/

	/* Site manager section */	
	SITE_MGR_DESIRED_CHANNEL_PARAM				=	SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x01,	/**< Site Manager Desired Channel Parameter (Site Manager Module Set/Get Command):\n 
																										* Used for Setting/Getting desired Channel to/from OS abstraction layer\n 
																										* Done Sync with no memory allocation \n
																										* Parameter Number:	0x01\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
	SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM	=	SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x05,	/**< Site Manager Desired Supported Rate Set Parameter (Site Manager Module Set/Get Command):\n 
																										* Used for Setting/Getting Desired Supported Rate to/from OS abstraction layer\n 
																										* Done Sync with no memory allocation \n
																										* Parameter Number:	0x05\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
	SITE_MGR_DESIRED_PREAMBLE_TYPE_PARAM		= 	SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x0A,	/**< Site Manager Desired Preamble Type Parameter (Site Manager Module Set/Get Command): \n 
																										* Used for Setting/Getting Desired Preamble Type to/from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x0A\	n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
    
    SITE_MGRT_SET_RATE_MANAGMENT             = SET_BIT | SITE_MGR_MODULE_PARAM | 0x06 ,                 /**< Site Manager Desired Preamble Type Parameter (Site Manager Module Set/Get Command): \n 
                                                                                                        * Used for Setting/Getting Desired Preamble Type to/from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x06\	n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: OF	\n
																										* SET Bit: ON	\n
*/
 

    SITE_MGRT_GET_RATE_MANAGMENT             = GET_BIT | SITE_MGR_MODULE_PARAM | 0x07| ASYNC_PARAM,     /**< Site Manager Desired Preamble Type Parameter (Site Manager Module Set/Get Command): \n 
                                                                                                        * Used for Setting/Getting Desired Preamble Type to/from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x07\	n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: ON	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
*/


																									   
	SITE_MGR_CURRENT_CHANNEL_PARAM              = 	SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x0E,	/**< Site Manager Current Channel Parameter (Site Manager Module Set/Get Command): \n 
																										* Used for Setting/Getting Current Channel to/from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x0E	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
	SITE_MGR_TI_WLAN_COUNTERS_PARAM            	=			  GET_BIT | SITE_MGR_MODULE_PARAM | 0x14,	/**< Site Manager TI WLAN Counters Parameter (Site Manager Module Get Command): \n 
																										* Used for Getting TI WLAN Statistics Counters from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x14	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/
	SITE_MGR_EEPROM_VERSION_PARAM				=			  GET_BIT | SITE_MGR_MODULE_PARAM | 0x16,  	/**< Site Manager EEPROM Version Parameter (Site Manager Module Get Command): \n 
																										* Used for Getting EEPROM Version from FW\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x16	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/ 
	SITE_MGR_FIRMWARE_VERSION_PARAM				=			  GET_BIT | SITE_MGR_MODULE_PARAM | 0x17,	/**< Site Manager FW Version Parameter (Site Manager Module Get Command): \n 
																										* Used for Getting FW Version from FW\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x17	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/ 
	SITE_MGR_DESIRED_SLOT_TIME_PARAM			=	SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x1B,	/**< Site Manager Desired Slot Time Parameter (Site Manager Module Set/Get Command): \n 
																										* Used for Getting Desired Slot Time from OS abstraction layer and Setting Desired Slot Time to FW\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x1B	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
	SITE_MGR_GET_AP_QOS_CAPABILITIES            =             GET_BIT | SITE_MGR_MODULE_PARAM | 0x2E,	/**< Site Manager Get AP QoS Cpabilities Parameter (Site Manager Module Get Command): \n  
																										* Used for Getting AP QoS Cpabilities from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x2E	\n
																										* Module Number: Site Manager Module Number \n 
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/
	SITE_MGR_CURRENT_TX_RATE_PARAM				=             GET_BIT | SITE_MGR_MODULE_PARAM | 0x32,	/**< Site Manager Current TX Rate Parameter (Site Manager Module Get Command): \n  
																										* Used for Getting Current TX Rate from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x32	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/
	SITE_MGR_BSSID_FULL_LIST_PARAM				=			  GET_BIT | SITE_MGR_MODULE_PARAM | 0x34,	/**< Site Manager BSSID Full List Parameter (Site Manager Module Get Command): \n  
																										* Used for Getting BSSID Full List from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x34	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/
	SITE_MGR_BEACON_FILTER_DESIRED_STATE_PARAM = 	SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x35,	/**< Site Manager Beacon Filter Desired State Parameter (Site Manager Module Set/Get Command): \n  
																										* Used for Getting Beacon Filter Desired State from OS abstraction layer or Setting Beacon Filter Desired State to FW\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x35	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
    SITE_MGR_NETWORK_TYPE_IN_USE				=             GET_BIT | SITE_MGR_MODULE_PARAM | 0x36,	/**< Site Manager NW Type in Use Parameter (Site Manager Module Get Command): \n  
																										* Used for Getting NW Type in Use from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x36	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/

	/* Simple Config module */
	SITE_MGR_SIMPLE_CONFIG_MODE					= 	SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x38,	/**< Site Manager Simple Configuration Mode Parameter (Simple Configuration Module Set/Get Command): \n  
																										* Used for Setting/Getting WiFi Simple Configuration Mode\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x38	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
    SITE_MGR_GET_PRIMARY_SITE					= 	          GET_BIT | SITE_MGR_MODULE_PARAM | 0x40,	/**< Site Manager Get Primary Site Parameter (Simple Configuration Module Get Command): \n  
																										* Used for Getting Primary Site from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x40	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/

    SITE_MGR_PRIMARY_SITE_HT_SUPPORT			= 	          GET_BIT | SITE_MGR_MODULE_PARAM | 0x41,	/**< Site Manager check if the Primary Site support HT: \n  
                                                                                                        * Used for check if the Primary Site support HT \n
                                                                                                        * Done Sync with no memory allocation\n 
                                                                                                        * Parameter Number:	0x41	\n
                                                                                                        * Module Number: Site Manager Module Number \n
                                                                                                        * Async Bit: OFF	\n
                                                                                                        * Allocate Bit: OFF	\n
                                                                                                        * GET Bit: ON	\n
                                                                                                        * SET Bit: OFF	\n
                                                                                                        */
	SITE_MGR_CURRENT_RX_RATE_PARAM				=             GET_BIT | SITE_MGR_MODULE_PARAM | 0x42,	/**< Site Manager Current RX Rate Parameter (Site Manager Module Get Command): \n  
																										* Used for Getting Current RX Rate from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x33	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/
    SITE_MGR_SET_WLAN_IP_PARAM				=             SET_BIT | SITE_MGR_MODULE_PARAM | 0x43,	/**< Site Manager WLAN interface IP Parameter (Site Manager Module Set Command): \n  
																										* Used for Setting the WLAN interface IP from OS abstraction layer\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x43	\n
																										* Module Number: Site Manager Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/

	/* CTRL data section */
	CTRL_DATA_CURRENT_BSS_TYPE_PARAM			=	SET_BIT | GET_BIT | CTRL_DATA_MODULE_PARAM | 0x04,	/**< Control Data Primary BSS Type Parameter (Control Data Module Set/Get Command): \n  
																										* Used for Setting/Getting Primary BSS Type to/form Control Data Parameters\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x04	\n
																										* Module Number: Control Data Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
    CTRL_DATA_MAC_ADDRESS						= 	          GET_BIT | CTRL_DATA_MODULE_PARAM | 0x08,	/**< Control Data MAC Address Parameter (Control Data Module Get Command): \n  
																										* Used for Getting MAC Address form FW\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x08	\n
																										* Module Number: Control Data Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/
    CTRL_DATA_CLSFR_TYPE                        =			  GET_BIT | CTRL_DATA_MODULE_PARAM | 0x0D,	/**< Control Data Classifier Type Parameter (Control Data Module Set/Get Command): \n  
																										* Used for Setting/Getting Classifier Type to/form Control Data (TX Data Queue) Parameters\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x0D	\n
																										* Module Number: Control Data Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: OFF	\n
																										*/
    CTRL_DATA_CLSFR_CONFIG                      =	SET_BIT           | CTRL_DATA_MODULE_PARAM | 0x0E,	/**< Control Data Classifier Configure Parameter (Control Data Module Set Command): \n  
																										* Used for adding Classifier entry to Control Data (TX Data Queue) Parameters\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x0E	\n
																										* Module Number: Control Data Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: OFF	\n
																										* SET Bit: ON	\n
																										*/
    CTRL_DATA_CLSFR_REMOVE_ENTRY                =	SET_BIT           | CTRL_DATA_MODULE_PARAM | 0x0F,	/**< Control Data Classifier Configure Parameter (Control Data Module Set Command): \n  
																										* Used for removing Classifier entry from Control Data (TX Data Queue) Parameters\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x0F	\n
																										* Module Number: Control Data Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: OFF	\n
																										* SET Bit: ON	\n
																										*/
    CTRL_DATA_TRAFFIC_INTENSITY_THRESHOLD		=	SET_BIT | GET_BIT | CTRL_DATA_MODULE_PARAM | 0x15,	/**< Control Data Traffic Intensity Threshold Parameter (Control Data Module Set/Get Command): \n  
																										* Used for Setting/Getting Traffic Intensity Threshold to/from Control Data (Traffic Intensity Threshold) Parameters\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x15	\n
																										* Module Number: Control Data Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
    CTRL_DATA_TOGGLE_TRAFFIC_INTENSITY_EVENTS	=	SET_BIT           | CTRL_DATA_MODULE_PARAM | 0x16,	/**< Control Data Toggle Traffic Intensity Events Parameter (Control Data Module Set Command): \n  
																										* Used for Toggle Traffic Intensity Events (turns ON/OFF traffic intensity notification events)	\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x16	\n
																										* Module Number: Control Data Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: OFF	\n
																										* SET Bit: ON	\n
																										*/

	/* SME SM section */    
    SME_DESIRED_SSID_ACT_PARAM                  = SET_BIT | GET_BIT | SME_MODULE_PARAM	 | 0x01,		/**< SME Set SSID and start connection process (SME Module Set/Get Command): \n  
																										* Used for set SSID and start connection or get current SSID \n 
																										* Parameter Number:	0x01 \n
																										* Module Number: SME Module Number \n
																										* Async Bit: OFF \n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/

    SME_RADIO_ON_PARAM                          = SET_BIT | GET_BIT | SME_MODULE_PARAM	 | 0x03,		/**< SME State-Machine Radio ON Parameter (SME Module Set/Get Command): \n  
																										* Used for Setting new and generating State-Machine Event, or Getting current Radio ON\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x03	\n
																										* Module Number: SME Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
    SME_CONNECTION_MODE_PARAM                   = SET_BIT | GET_BIT | SME_MODULE_PARAM   | 0x04,		/**< SME State-Machine Connection Mode Parameter (SME Module Set/Get Command): \n  
																										* Used for Setting new Connection Mode (and generating disconnect State-Machine event) or Getting current Connection Mode\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x04	\n
																										* Module Number: SME Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: ON	\n
																										* SET Bit: ON	\n
																										*/
    SME_WSC_PB_MODE_PARAM                       = SET_BIT           | SME_MODULE_PARAM   | 0x07,		/**< SME State-Machine SME on the WPS Mode Parameter (SME Module Set Command): \n  
																										* Used for updating the SME on the WPS mode\n
																										* Done Sync with no memory allocation\n 
																										* Parameter Number:	0x07	\n
																										* Module Number: SME Module Number \n
																										* Async Bit: OFF	\n
																										* Allocate Bit: OFF	\n
																										* GET Bit: OFF	\n
																										* SET Bit: ON	\n
																										*/

    SME_DESIRED_SSID_PARAM                      = SET_BIT           | SME_MODULE_PARAM	 | 0x08,		/**< SME Set SSID without start connection process (SME Module Set Command): \n  
                                                                                                        * Used for set SSID without connection \n 
                                                                                                        * Parameter Number:	0x08 \n
                                                                                                        * Module Number: SME Module Number \n
                                                                                                        * Async Bit: OFF \n
                                                                                                        * Allocate Bit: OFF	\n
                                                                                                        * GET Bit: OFF	\n
                                                                                                        * SET Bit: ON	\n
                                                                                                        */

	/* Scan Concentrator section */
    SCAN_CNCN_START_APP_SCAN					= 	SET_BIT | 			SCAN_CNCN_PARAM | 0x01 | ALLOC_NEEDED_PARAM,	/**< Scan Concentrator Start Application Scan Parameter (Scan Concentrator Module Set Command): \n  
																														* Used for start one-shot scan as running application scan client\n
																														* Done Sync with memory allocation\n 
																														* Parameter Number:	0x01	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: ON	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/
    SCAN_CNCN_STOP_APP_SCAN                     =   SET_BIT |           SCAN_CNCN_PARAM | 0x02,							/**< Scan Concentrator Stop Application Scan Parameter (Scan Concentrator Module Set Command): \n  
																														* Used for stop one-shot scan as running application scan client\n
																														* Done Sync with no memory allocation\n 
																														* Parameter Number:	0x02	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/
    SCAN_CNCN_START_PERIODIC_SCAN               =   SET_BIT |           SCAN_CNCN_PARAM | 0x03 | ALLOC_NEEDED_PARAM,	/**< Scan Concentrator Start Periodic Scan Parameter (Scan Concentrator Module Set Command): \n  
																														* Used for start periodic scan as running application scan client\n
																														* Done Sync with memory allocation\n 
																														* Parameter Number:	0x03	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: ON	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/
    SCAN_CNCN_STOP_PERIODIC_SCAN                =   SET_BIT |           SCAN_CNCN_PARAM | 0x04,							/**< Scan Concentrator Stop Periodic Scan Parameter (Scan Concentrator Module Set Command): \n  
																														* Used for stop periodic scan as running application scan client\n
																														* Done Sync with no memory allocation\n 
																														* Parameter Number:	0x04	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/
    SCAN_CNCN_BSSID_LIST_SCAN_PARAM             =   SET_BIT |           SCAN_CNCN_PARAM | 0x05,							/**< Scan Concentrator BSSID List Scon Parameter (Scan Concentrator Module Set Command): \n  
																														* Used for start one-shot scan as running application scan client\n
																														* Done Sync with no memory allocation\n 
																														* Parameter Number:	0x05	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/
	SCAN_CNCN_NUM_BSSID_IN_LIST_PARAM           =   GET_BIT |           SCAN_CNCN_PARAM | 0x06,							/**< Scan Concentrator BSSID List Size Parameter (Scan Concentrator Module Get Command): \n  
																														* Used for retrieving the size to allocate for the application scan result list\n
																														* Done Sync with no memory allocation\n 
																														* Parameter Number:	0x06	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: ON	\n
																														* SET Bit: OFF	\n
																														*/
    SCAN_CNCN_BSSID_LIST_SIZE_PARAM             =   GET_BIT |           SCAN_CNCN_PARAM | 0x07,							/**< Scan Concentrator BSSID List Size Parameter (Scan Concentrator Module Get Command): \n  
																														* Used for retrieving the size to allocate for the application scan result list\n
																														* Done Sync with no memory allocation\n 
																														* Parameter Number:	0x06	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: ON	\n
																														* SET Bit: OFF	\n
																														*/
    SCAN_CNCN_BSSID_LIST_PARAM                  =   GET_BIT |           SCAN_CNCN_PARAM | 0x08,							/**< Scan Concentrator BSSID List Parameter (Scan Concentrator Module Get Command): \n  
																														* Used for retrieving the application scan result table\n
																														* Done Sync with no memory allocation\n 
																														* Parameter Number:	0x07	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: ON	\n
																														* SET Bit: OFF	\n
																														*/
	SCAN_CNCN_BSSID_RATE_LIST_PARAM             =   GET_BIT |           SCAN_CNCN_PARAM | 0x09,							/**< Scan Concentrator Rate List Parameter (Scan Concentrator Module Get Command): \n  
																														* Used for retrieving the application scan rates result table\n
																														* Done Sync with no memory allocation\n 
																														* Parameter Number:	0x08	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: ON	\n
																														* SET Bit: OFF	\n
																														*/
    SCAN_CNCN_SET_SRA                           =   SET_BIT |           SCAN_CNCN_PARAM | 0x0A,                         /**< Scan Concentrator set scan result aging (Scan Concentrator Module Get Command): \n  
																														* Used for aging threshold\n
																														* Parameter Number:	0x09	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/

    SCAN_CNCN_SET_RSSI                          =   SET_BIT |           SCAN_CNCN_PARAM | 0x0B,                         /**< Scan Concentrator set rssi filter threshold (Scan Concentrator Module Get Command): \n  
																														* Used for rssi threshold\n
																														* Parameter Number:	0x0A	\n
																														* Module Number: Scan Concentrator Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: OFF	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/

	/* Scan Manager module */
    SCAN_MNGR_SET_CONFIGURATION                 =	SET_BIT |           SCAN_MNGR_PARAM | 0x01 | ALLOC_NEEDED_PARAM,	/**< Scan Manager Set Configuration Parameter (Scan Manager Module Set Command): \n  
																														* Used for setting the Scan Policy\n
																														* Done Sync with memory allocation\n 
																														* Parameter Number:	0x01	\n
																														* Module Number: Scan Manager Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: ON	\n
																														* GET Bit: OFF	\n
																														* SET Bit: ON	\n
																														*/
    SCAN_MNGR_BSS_LIST_GET						=			  GET_BIT | SCAN_MNGR_PARAM | 0x02 | ALLOC_NEEDED_PARAM,	/**< Scan Manager Get BSS List Parameter (Scan Manager Module Get Command): \n  
																														* Used for getting the currently available BSS list\n
																														* Done Sync with memory allocation\n 
																														* Parameter Number:	0x02	\n
																														* Module Number: Scan Manager Module Number \n
																														* Async Bit: OFF	\n
																														* Allocate Bit: ON	\n
																														* GET Bit: ON	\n
																														* SET Bit: OFF	\n
																														*/

	/* regulatory domain section */
	REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM	=			  GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x02,					   	/**< Regulatory Domain Management Capability Enabled Parameter (Regulatory Domain Module Get Command): \n  
																																				* Used for getting indication if Spectrum Management is enabled\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x02	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: OFF	\n
																																				*/
	REGULATORY_DOMAIN_ENABLED_PARAM							=			  GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x03,						/**< Regulatory Domain Enabled Parameter (Regulatory Domain Module Get Command): \n  
																																				* Used for getting indication if regulatory domain if 802.11d is in use\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x03	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: OFF	\n
																																				*/ 		
    REGULATORY_DOMAIN_CURRENT_TX_POWER_LEVEL_PARAM      	= 	SET_BIT | GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x06,						/**< Regulatory Domain Current TX Power Level Parameter (Regulatory Domain Module Set/Get Command): \n  
																																				* Used for setting/getting current TZ Power Level\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x06	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: ON	\n
																																				*/ 
    REGULATORY_DOMAIN_CURRENT_TX_POWER_IN_DBM_PARAM			= 	SET_BIT | GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x08,						/**< Regulatory Domain Current TX Power in DBM Parameter (Regulatory Domain Module Set/Get Command): \n  
																																				* Used for setting/getting current TX Power Level in DBM\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x08	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: ON	\n
																																				*/
    REGULATORY_DOMAIN_ENABLE_DISABLE_802_11D				=	SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x0C,					 	/**< Regulatory Domain Enable/Disable 802.11d Parameter (Regulatory Domain Module Set Command): \n  
																																				* Used for enabling/disabling 802.11d.\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x0C	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: OFF	\n
																																				* SET Bit: ON	\n
																																				*/   	
    REGULATORY_DOMAIN_ENABLE_DISABLE_802_11H				=	SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x0D,						/**< Regulatory Domain Enable/Disable 802.11h Parameter (Regulatory Domain Module Set Command): \n  
																																				* Used for enabling/disabling 802.11h (If 802_11h is enabled, enable 802_11d as well)\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x0D	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: OFF	\n
																																				* SET Bit: ON	\n
																																				*/ 
    REGULATORY_DOMAIN_COUNTRY_2_4_PARAM						=	SET_BIT | GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x0E | ALLOC_NEEDED_PARAM,	/**< Regulatory Domain Country 2-4 Parameter (Regulatory Domain Module Set/Get Command): \n  
																																				* Used for getting Country String or setting the local country IE per band with the Country IE that was detected in the last passive scan\n
																																				* Done Sync with memory allocation\n 
																																				* Parameter Number:	0x0E	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: ON	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: ON	\n
																																				*/		
    REGULATORY_DOMAIN_COUNTRY_5_PARAM						=	SET_BIT | GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x0F | ALLOC_NEEDED_PARAM,	/**< Regulatory Domain Country 5 Parameter (Regulatory Domain Module Set/Get Command): \n  
																																				* Used for getting Country String or setting the local country IE per band with the Country IE that was detected in the last passive scan\n
																																				* Done Sync with memory allocation\n 
																																				* Parameter Number:	0x0F	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: ON	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: ON	\n
																																				*/	
    REGULATORY_DOMAIN_DFS_CHANNELS_RANGE					=	SET_BIT | GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x10,						/**< Regulatory Domain DFS Channels Parameter (Regulatory Domain Module Set/Get Command): \n  
																																				* Used for config manager in order to set/get a parameter received from the OS abstraction layer\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x10	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: ON	\n
																																				*/	
	REGULATORY_DOMAIN_TX_POWER_LEVEL_TABLE_PARAM			=   		  GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x12,						/**< Regulatory Domain TX Power Level Table Parameter (Regulatory Domain Module Get Command): \n  
																																				* Used for getting TX Power Level Table from FW\n
																																				* Done Sync with no memory allocation\n 
																																				* Parameter Number:	0x12	\n
																																				* Module Number: Regulatory Domain Module Number \n
																																				* Async Bit: OFF	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: OFF	\n
																																				*/		

	/* Power Manager params */
    POWER_MGR_POWER_MODE							= 	SET_BIT | GET_BIT | POWER_MANAGER_PARAM | 0x01,							/**< Power Manager Power Mode Parameter (Power Manager Module Set/Get Command): \n  
																																* Used for setting/getting the Power Mode to/from Power Manager Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x01	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    POWER_MGR_POWER_LEVEL_PS                        =   SET_BIT | GET_BIT | POWER_MANAGER_PARAM | 0x02,							/**< Power Manager Power Level Power-Save Parameter (Power Manager Module Set/Get Command): \n  
																																* Used for getting the Power Level Power-Save from Power Manager Module or setting the Power Level Power-Save to Power Manager Module (and to FW if Power-Save is Enabled)\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x02	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    POWER_MGR_POWER_LEVEL_DEFAULT                   =   SET_BIT | GET_BIT | POWER_MANAGER_PARAM | 0x03,							/**< Power Manager Power Level Default Parameter (Power Manager Module Set/Get Command): \n  
																																* Used for getting the Power Level Default from Power Manager Module or setting the Power Level Default to Power Manager Module (and to FW if Power-Save is Enabled)\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x03	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    POWER_MGR_POWER_LEVEL_DOZE_MODE                 =   SET_BIT | GET_BIT | POWER_MANAGER_PARAM | 0x04,							/**< Power Manager Power Level Doze Mode (short-doze / long-doze) Parameter (Power Manager Module Set/Get Command): \n  
																																* Used for getting the Power Level Doze Mode from Power Manager Module or setting the Power Level Doze Mode to Power Manager Module (and to FW if Power-Save is Enabled)\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x04	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    POWER_MGR_KEEP_ALIVE_ENA_DIS                    =   SET_BIT |           POWER_MANAGER_PARAM | 0x05,							/**< Power Manager Keep Alive Enable/Disable Parameter (Power Manager Module Set Command): \n  
																																* Used for setting the Keep Alive Enable/Disable to Power Manager and FW\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x05	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: OFF	\n
																																* SET Bit: ON	\n
																																*/
    POWER_MGR_KEEP_ALIVE_ADD_REM                    =   SET_BIT |           POWER_MANAGER_PARAM | 0x06 | ALLOC_NEEDED_PARAM,	/**< Power Manager Keep Alive add REM Parameter (Power Manager Module Set Command): \n  
																																* Used for setting addition/removal of a template and global enable/disable flag to Power Manager and FW\n
																																* Done Sync with memory allocation\n 
																																* Parameter Number:	0x06	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: ON	\n
																																* GET Bit: OFF	\n
																																* SET Bit: ON	\n
																																*/
    POWER_MGR_KEEP_ALIVE_GET_CONFIG                 =             GET_BIT | POWER_MANAGER_PARAM | 0x07 | ALLOC_NEEDED_PARAM,	/**< Power Manager Keep Alive Get Configuration Parameter (Power Manager Module Get Command): \n  
																																* Used for getting the Keep Alive current Configuration\n
																																* Done Sync with memory allocation\n 
																																* Parameter Number:	0x07	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: ON	\n
																																* GET Bit: ON	\n
																																* SET Bit: OFF	\n
																																*/


    POWER_MGR_GET_POWER_CONSUMPTION_STATISTICS       =            GET_BIT | POWER_MANAGER_PARAM | 0x09| ASYNC_PARAM,             /**< Power Manager  Get power consumption parmeter (Power Manager Module Get Command): \n  
                                                                                                                                * Used for getting the Keep Alive current Configuration\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x08	\n
																																* Module Number: Power Manager Module Number \n
																																* Async Bit: ON	\n
																																* Allocate Bit: ON	\n
																																* GET Bit: ON	\n
																																* SET Bit: OFF	\n
																																*/




	/* Robust Security NW (RSN) section */
	RSN_ENCRYPTION_STATUS_PARAM						=	SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x04,							/**< Robust Security NW (RSN) Encryption Status Parameter (RSN Module Set/Get Command): \n  
																																* Used for setting/getting Encryption Status to/from RSN Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x04	\n
																																* Module Number: RSN Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
	RSN_ADD_KEY_PARAM								=	SET_BIT | 			RSN_MODULE_PARAM | 0x05,							/**< Robust Security NW (RSN) Add Key Parameter (RSN Module Set Command): \n  
																																* Used for adding RSN Key to FW\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x05	\n
																																* Module Number: RSN Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: OFF	\n
																																* SET Bit: ON	\n
																																*/
	RSN_REMOVE_KEY_PARAM							=	SET_BIT           | RSN_MODULE_PARAM | 0x06,							/**< Robust Security NW (RSN) Remove Key Parameter (RSN Module Set Command): \n  
																																* Used for removing RSN Key from FW\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x06	\n
																																* Module Number: RSN Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: OFF	\n
																																* SET Bit: ON	\n
																																*/
    RSN_EXT_AUTHENTICATION_MODE                 	= 	SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x07,							/**< Robust Security NW (RSN) External Authentication Mode Parameter (RSN Module Set/Get Command): \n  
																																* Used for getting RSN External Authentication Mode from RSN Module or setting RSN External Authentication Mode to FW and RSN Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x07	\n
																																* Module Number: RSN Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
	RSN_MIXED_MODE									=	SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x08,							/**< Robust Security NW (RSN) Mixed Mode Parameter (RSN Module Set/Get Command): \n  
																																* Used for setting/getting RSN Mixed Mode to/from RSN Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x08	\n
																																* Module Number: RSN Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    RSN_DEFAULT_KEY_ID								=	SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x09,							/**< Robust Security NW (RSN) Defualt Key ID Parameter (RSN Module Set/Get Command): \n  
																																* Used for getting RSN defualt Key ID from RSN Module or setting RSN defualt Key ID to FW and RSN Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	0x09	\n
																																* Module Number: RSN Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
	RSN_XCC_NETWORK_EAP								=	SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x0A,							/**< Robust Security NW (RSN) XCC NW EAP Parameter (RSN Module Set/Get Command): \n  
                                                                                                                                * Used for setting/getting RSN XCC NW EAP to/from RSN Module\n
                                                                                                                                * Done Sync with no memory allocation\n 
                                                                                                                                * Parameter Number:	0x0B	\n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: OFF	\n
                                                                                                                                * Allocate Bit: OFF	\n
                                                                                                                                * GET Bit: ON	\n
                                                                                                                                * SET Bit: ON	\n
                                                                                                                                */

    RSN_SET_KEY_PARAM								=	SET_BIT | RSN_MODULE_PARAM | 0x0B,							            /**< Robust Security NW (RSN) Set Key Parameter (RSN Module Set/Get Command): \n  
                                                                                                                                * Used for setting Keys during external RSN mode to RSN Module\n
                                                                                                                                * Done Sync with no memory allocation\n 
                                                                                                                                * Parameter Number:	0x0B	\n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: OFF	\n
                                                                                                                                * Allocate Bit: OFF	\n
                                                                                                                                * GET Bit: ON	\n
                                                                                                                                * SET Bit: ON	\n
                                                                                                                                */



    RSN_SET_PROTECTION_RX_PARAM					=	SET_BIT | RSN_MODULE_PARAM | ASYNC_PARAM | 0x0C,					        /**< Robust Security NW (RSN) Set Protection RX Parameter (RSN Module Set/Get Command): \n  
                                                                                                                                * Used for setting protection for RX during external RSN mode to RSN Module\n
                                                                                                                                * Done ASync with no memory allocation\n 
                                                                                                                                * Parameter Number:	0x0C	\n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: ON	\n
                                                                                                                                * Allocate Bit: OFF	\n
                                                                                                                                * GET Bit: ON	\n
                                                                                                                                * SET Bit: ON	\n
                                                                                                                                */


    RSN_SET_PROTECTION_RX_TX_PARAM					=	SET_BIT | RSN_MODULE_PARAM | 0x0D,							            /**< Robust Security NW (RSN) Set Protection RX TX Parameter (RSN Module Set/Get Command): \n  
                                                                                                                                * Used for setting protection for both RX and TX during external RSN mode to RSN Module\n
                                                                                                                                * Done Sync with no memory allocation\n 
                                                                                                                                * Parameter Number:	0x0C	\n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: OFF	\n
                                                                                                                                * Allocate Bit: OFF	\n
                                                                                                                                * GET Bit: ON	\n
                                                                                                                                * SET Bit: ON	\n
                                                                                                                                */

    RSN_PORT_STATUS_PARAM		 			    =	SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x0D,							    /**< Robust Security NW (RSN)  Port Status (RSN Module Set/Get Command): \n  
                                                                                                                                * Used for setting port status during external RSN mode to RSN Module\n
                                                                                                                                * Done Sync with no memory allocation\n 
                                                                                                                                * Parameter Number:	0x0D	\n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: OFF	\n
                                                                                                                                * Allocate Bit: OFF	\n
                                                                                                                                * GET Bit: ON	\n
                                                                                                                                * SET Bit: ON	\n
                                                                                                                                */

    RSN_GENERIC_IE_PARAM		 			      = SET_BIT           | RSN_MODULE_PARAM | 0x0E,							    /**< Robust Security NW (RSN)  Generic IE (RSN Module Set Command): \n  
                                                                                                                                * Used for setting the Generic IE passed to the AP during association to RSN Module\n
                                                                                                                                * Done Sync with no memory allocation\n 
                                                                                                                                * Parameter Number:	0x0E	\n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: OFF	\n
                                                                                                                                * Allocate Bit: OFF	\n
                                                                                                                                * GET Bit: OFF	\n
                                                                                                                                * SET Bit: ON	\n
                                                                                                                                */

    RSN_EXTERNAL_MODE_PARAM		 			      =          SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x0F,							    /**< Robust Security NW (RSN)  External Mode Parameter: \n  
                                                                                                                                * Used for getting the RSN External Mode\n
                                                                                                                                * Done Sync with no memory allocation\n 
                                                                                                                                * Parameter Number:     0x0F    \n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: OFF        \n
                                                                                                                                * Allocate Bit: OFF     \n
                                                                                                                                * GET Bit: OFF  \n
                                                                                                                                * SET Bit: ON   \n
                                                                                                                                */

    RSN_GEM_DATA_PARAM                                                =                     RSN_MODULE_PARAM | 0x10,                                                        /**< Robust Security NW (RSN)  External Mode Parameter: \n  
                                                                                                                                * Used for setting GEM data\n
                                                                                                                                * Done Sync with no memory allocation\n 
                                                                                                                                * Parameter Number:     0x10    \n
                                                                                                                                * Module Number: RSN Module Number \n
                                                                                                                                * Async Bit: OFF	\n
                                                                                                                                * Allocate Bit: OFF	\n
                                                                                                                                * GET Bit: ON 	\n
                                                                                                                                * SET Bit: OFF	\n
                                                                                                                                */


	/* TWD Control section */
    TWD_RTS_THRESHOLD_PARAM                			=   SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_RTS_THRESHOLD_PARAM_ID,		/**< TWD Control RTS Threshold Parameter (TWD Control Module Set/Get Command): \n  
																																* Used for getting RTS Threshold from TWD Control Module or setting RTS Threshold to FW and TWD Control Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	TWD_RTS_THRESHOLD_PARAM_ID	\n
																																* Module Number: TWD Control Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    TWD_FRAG_THRESHOLD_PARAM               			=   SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_FRAG_THRESHOLD_PARAM_ID,		/**< TWD Control Fragmentation Threshold Parameter (TWD Control Module Set/Get Command): \n  
																																* Used for getting Fragmentation Threshold from TWD Control Module or setting Fragmentation Threshold to FW and TWD Control Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	TWD_FRAG_THRESHOLD_PARAM_ID	\n
																																* Module Number: TWD Control Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    TWD_RSSI_LEVEL_PARAM							=			  GET_BIT | TWD_MODULE_PARAM | TWD_RSSI_LEVEL_PARAM_ID 			| ASYNC_PARAM,	/**< TWD Control RSSI Level Parameter (TWD Control Module Get Command): \n  
																																				* Used for getting RSSI Level From FW\n
																																				* Done Async with no memory allocation\n 
																																				* Parameter Number:	TWD_RSSI_LEVEL_PARAM_ID	\n
																																				* Module Number: TWD Control Module Number \n
																																				* Async Bit: ON	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: OFF	\n
																																				*/
    TWD_SNR_RATIO_PARAM                    			=			  GET_BIT | TWD_MODULE_PARAM | TWD_SNR_RATIO_PARAM_ID			| ASYNC_PARAM,	/**< TWD Control SNR Radio Parameter (TWD Control Module Get Command): \n  
																																				* Used for getting SNR Radio From FW (same outcome as TWD_RSSI_LEVEL_PARAM)\n
																																				* Done Async with no memory allocation\n 
																																				* Parameter Number:	TWD_SNR_RATIO_PARAM_ID	\n
																																				* Module Number: TWD Control Module Number \n
																																				* Async Bit: ON	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: OFF	\n
																																				*/

	/*for BIP/PLT/Radio Debug Tests --> supports Set + GET*/
	TWD_RADIO_TEST_PARAM               						=   SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_RADIO_TEST_PARAM_ID 	| ASYNC_PARAM,	/**< TWD Control SNR Radio Parameter (TWD Control Module Set/Get Command): \n  
																																				* Used for performing BIP/PLT/Radio Debug Tests\n
																																				* Done Async with no memory allocation\n 
																																				* Parameter Number:	TWD_RADIO_TEST_PARAM_ID	\n
																																				* Module Number: TWD Control Module Number \n
																																				* Async Bit: ON	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: ON	\n
																																				* SET Bit: ON	\n
																																				*/
	
	TWD_FM_COEX_PARAM               						=   SET_BIT |    TWD_MODULE_PARAM | TWD_FM_COEX_PARAM_ID,	                        /**< TWD Control FM-Coexistence Parameters (TWD Control Module Set/Get Command): \n  
																																				* Used for setting the FM-Coexistence Parameters\n
																																				* Done Async with no memory allocation\n 
																																				* Parameter Number:	TWD_FM_COEX_PARAM_ID	\n
																																				* Module Number: TWD Control Module Number \n
																																				* Async Bit: ON	\n
																																				* Allocate Bit: OFF	\n
																																				* GET Bit: OFF	\n
																																				* SET Bit: ON	\n
																																				*/

    TWD_DCO_ITRIM_PARAMS                			=   SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_DCO_ITRIM_PARAMS_ID,		/**< TWD Control DCO Itrim Parameters (TWD Control Module Set/Get Command): \n  
																																* Used for getting DCO Itrim Parameters from TWD Control Module or setting DCO Itrim Parameters to FW and TWD Control Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	TWD_DCO_ITRIM_PARAMS_ID	\n
																																* Module Number: TWD Control Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: ON	\n
																																* SET Bit: ON	\n
																																*/
    /* Roaming manager */
    ROAMING_MNGR_APPLICATION_CONFIGURATION		= 	SET_BIT | GET_BIT | ROAMING_MANAGER_MODULE_PARAM | 0x01,	/**< Roaming Manager Application Configuration Parameter (Roaming Manager Module Set/Get Command): \n  
																												* Used for setting/getting Roaming Manager Application Configuration to/from Roaming Manager Module and State-Machine\n
																												* Done Sync with no memory allocation\n 
																												* Parameter Number:	0x01	\n
																												* Module Number: Roaming Manager Module Number \n
																												* Async Bit: OFF	\n
																												* Allocate Bit: OFF	\n
																												* GET Bit: ON	\n
																												* SET Bit: ON	\n
																												*/
    ROAMING_MNGR_USER_DEFINED_TRIGGER     		= 	SET_BIT |           ROAMING_MANAGER_MODULE_PARAM | 0x02,	/**< Roaming Manager User Defined Trigger Parameter (Roaming Manager Module Set Command): \n  
																												* Used for setting user-defined trigger to FW\n
																												* Done Sync with no memory allocation\n 
																												* Parameter Number:	0x02	\n
																												* Module Number: Roaming Manager Module Number \n
																												* Async Bit: OFF	\n
																												* Allocate Bit: OFF	\n
																												* GET Bit: OFF	\n
																												* SET Bit: ON	\n
																												*/

	/* QOS manager params */
    QOS_MNGR_SET_OS_PARAMS						=	SET_BIT |           QOS_MANAGER_PARAM | 0x10,	/**< QoS Manager Set OS Parameter (QoS Module Set Command): \n  
																									* Used for setting Quality Of Service Manager's Parameters\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x10	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
    QOS_MNGR_AP_QOS_PARAMETERS					=			  GET_BIT | QOS_MANAGER_PARAM | 0x11,	/**< QoS Manager AP QoS Parameter (QoS Module Get Command): \n  
																									* Used for getting current AP QoS Parameters from QoS Module\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x11	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: OFF	\n
																									*/
    QOS_MNGR_OS_TSPEC_PARAMS					=             GET_BIT | QOS_MANAGER_PARAM | 0x12,	/**< QoS Manager OS TSPEC Parameter (QoS Module Get Command): \n  
																									* Used for getting current OS 802.11 QoS TSPEC Parameters from QoS Module\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x12	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: OFF	\n
																									*/
    QOS_MNGR_AC_STATUS							=	SET_BIT | GET_BIT | QOS_MANAGER_PARAM | 0x13,	/**< QoS Manager AC Status Parameter (QoS Module Set/Get Command): \n  
																									* Used for setting/getting SC Status\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x13	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/
    QOS_MNGR_ADD_TSPEC_REQUEST					=	SET_BIT | 			QOS_MANAGER_PARAM | 0x14,	/**< QoS Manager Add TSPEC Request Parameter (QoS Module Set Command): \n  
																									* Used for setting TSPEC Parameters to QoS Module\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x14	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
	QOS_MNGR_DEL_TSPEC_REQUEST					=	SET_BIT           | QOS_MANAGER_PARAM | 0x15,	/**< QoS Manager Delete TSPEC Request Parameter (QoS Module Set Command): \n  
																									* Used for deleting TSPEC Parameters from QoS Module\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x15	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
	QOS_MNGR_GET_DESIRED_PS_MODE				=             GET_BIT | QOS_MANAGER_PARAM | 0x17,	/**< QoS Manager Get Desired Power-Save Mode Parameter (QoS Module Get Command): \n  
																									* Used for getting the current desired Power-Save Mode from QoS Module\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x17	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: OFF	\n
																									*/
    QOS_SET_RX_TIME_OUT							=	SET_BIT |			QOS_MANAGER_PARAM | 0x18, 	/**< QoS Manager Get Desired Power-Save Mode Parameter (QoS Module Set Command): \n  
																									* Used for setting RX Time Out (PS poll and UPSD) to FW and in QoS Module\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x18	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
    QOS_MNGR_PS_RX_STREAMING					=	SET_BIT | GET_BIT | QOS_MANAGER_PARAM | 0x19,	/**< QoS Manager Set Power-Save RX Streaming Parameter (QoS Module Set/Get Command): \n  
																									* Used for getting Power-Save RX Streaming or setting Power-Save RX Streaming to FW and in QoS Module\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x19	\n
																									* Module Number: QoS Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/

    /* Soft Gemini params */
	SOFT_GEMINI_SET_ENABLE						=	SET_BIT |           SOFT_GEMINI_PARAM	| 0x01,	/**< Soft Gimini Parameters Set Enable Parameter (Soft Gimini Parameters Module Set Command): \n  
																									* Used for configuring Soft Gimini enable Mode (Enable|Disable|Auto) in FW\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x01	\n
																									* Module Number: Soft Gimini Parameters Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
    SOFT_GEMINI_SET_CONFIG						=	SET_BIT |           SOFT_GEMINI_PARAM   | 0x03,	/**< Soft Gimini Parameters Set Configuration Parameter (Soft Gimini Parameters Module Set Command): \n  
																									* Used for setting Soft Gimini Configuration to FW\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x03	\n
																									* Module Number: Soft Gimini Parameters Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: OFF	\n
																									* SET Bit: ON	\n
																									*/
    SOFT_GEMINI_GET_CONFIG                      =	GET_BIT |           SOFT_GEMINI_PARAM   | 0x04,	/**< Soft Gimini Parameters Get Configuration Parameter (Soft Gimini Parameters Module Get Command): \n  
																									* Used for getting Soft Gimini Configuration\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x04	\n
																									* Module Number: Soft Gimini Parameters Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: OFF	\n
																									*/

	/* REPORT section */
	REPORT_MODULE_TABLE_PARAM                   =	SET_BIT | GET_BIT | REPORT_MODULE_PARAM | 0x01,	/**< Report Module Table Parameter (Report Module Set/Get Command): \n  
																									* Used for setting/getting Report Module Table (Tble of all Logged Modules)\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x01	\n
																									* Module Number: Report Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/
	REPORT_SEVERITY_TABLE_PARAM                 =	SET_BIT | GET_BIT | REPORT_MODULE_PARAM | 0x02,	/**< Report Severity Table Parameter (Report Module Set/Get Command): \n  
																									* Used for setting/getting the Severity Table (holds availble severity Levels of the event which is reported to user)\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x02	\n
																									* Module Number: Report Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/
    REPORT_MODULE_ON_PARAM                      =   SET_BIT | GET_BIT | REPORT_MODULE_PARAM | 0x03,	/**< Report Module ON Parameter (Report Module Set/Get Command): \n  
																									* Used for setting (Enable) ceratin Logged Module in Report Modules Table or getting the Reported Module Status from Table (Enabled/Disabled)\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x03	\n
																									* Module Number: Report Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/
    REPORT_MODULE_OFF_PARAM                     =   SET_BIT | GET_BIT | REPORT_MODULE_PARAM | 0x04,	/**< Report Module OFF Parameter (Report Module Set/Get Command): \n  
																									* Used for setting (Disable) ceratin Logged Module in Report Modules Table or getting the Reported Module Status from Table (Enabled/Disabled)\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x04	\n
																									* Module Number: Report Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/
    REPORT_PPMODE_VALUE_PARAM                   =   SET_BIT | GET_BIT | REPORT_MODULE_PARAM | 0x05,	/**< Report PP MODE Value Parameter (Report Module Set/Get Command): \n  
																									* Used for setting (Enable/Disable) or Getting the Debug Mode flag, which indicates whether debug module should be used or not\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x05	\n
																									* Module Number: Report Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/
	REPORT_OUTPUT_TO_LOGGER_ON                  =   SET_BIT | GET_BIT | REPORT_MODULE_PARAM | 0x06,	/**< Report output ON Parameter (Report Module Set/Get Command): \n  
																									* Used for setting the output of logs to the logger application\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x04	\n
																									* Module Number: Report Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/

	REPORT_OUTPUT_TO_LOGGER_OFF                  =   SET_BIT | GET_BIT | REPORT_MODULE_PARAM | 0x07,/**< Report output OFF Parameter (Report Module Set/Get Command): \n  
																									* Used for setting OFF the output of logs to the logger application\n
																									* Done Sync with no memory allocation\n 
																									* Parameter Number:	0x04	\n
																									* Module Number: Report Module Number \n
																									* Async Bit: OFF	\n
																									* Allocate Bit: OFF	\n
																									* GET Bit: ON	\n
																									* SET Bit: ON	\n
																									*/


	/* TX data section */
    TX_CTRL_COUNTERS_PARAM						=			  GET_BIT | TX_CTRL_MODULE_PARAM | 0x01 | ALLOC_NEEDED_PARAM,	/**< TX Control Counters Parameter (TX Control Module Get Command): \n  
																															* Used for getting TX statistics per Tx-queue\n
																															* Done Sync with memory allocation\n 
																															* Parameter Number:	0x01	\n
																															* Module Number: TX Control Module Number \n
																															* Async Bit: OFF	\n
																															* Allocate Bit: ON	\n
																															* GET Bit: ON	\n
																															* SET Bit: OFF	\n
																															*/
    TX_CTRL_RESET_COUNTERS_PARAM                =	SET_BIT 		  | TX_CTRL_MODULE_PARAM | 0x02,						/**< TX Control Reset Counters Parameter (TX Control Module Set Command): \n  
																															* Used for Reset all TX statistics per Tx-queue\n
																															* Done Sync with no memory allocation\n 
																															* Parameter Number:	0x02	\n
																															* Module Number: TX Control Module Number \n
																															* Async Bit: OFF	\n
																															* Allocate Bit: OFF	\n
																															* GET Bit: OFF	\n
																															* SET Bit: ON	\n
																															*/
    TX_CTRL_SET_MEDIUM_USAGE_THRESHOLD			=	SET_BIT           | TX_CTRL_MODULE_PARAM | 0x03,						/**< TX Control Set Medum Usage Threshold Parameter (TX Control Module Set Command): \n  
																															* Used for setting Medum Usage Threshold of AC\n
																															* Done Sync with no memory allocation\n 
																															* Parameter Number:	0x03	\n
																															* Module Number: TX Control Module Number \n
																															* Async Bit: OFF	\n
																															* Allocate Bit: OFF	\n
																															* GET Bit: OFF	\n
																															* SET Bit: ON	\n
																															*/

    TX_CTRL_GENERIC_ETHERTYPE         			=	SET_BIT | GET_BIT | TX_CTRL_MODULE_PARAM | 0x10,						/**< TX Control Get/Set Generic Ethertype (TX Control Module Get/Set Command): \n  
																															* Used for setting the Generic Ethertype for authentication packets\n
																															* Done Sync with no memory allocation\n 
																															* Parameter Number:	0x10	\n
																															* Module Number: TX Control Module Number \n
																															* Async Bit: OFF	\n
																															* Allocate Bit: OFF	\n
																															* GET Bit: ON	\n
																															* SET Bit: ON	\n
																															*/

    /* RX data section */
    RX_DATA_ENABLE_DISABLE_RX_DATA_FILTERS     	=   SET_BIT | GET_BIT | RX_DATA_MODULE_PARAM | 0x04,						/**< RX Data Enable/Disable Filters Parameter (RX Data Module Set/Get Command): \n  
																															* Used for Enabling/Disabling Filters in FW or getting the  Filters Enabling/Disabling current Status\n
																															* Done Sync with no memory allocation\n 
																															* Parameter Number:	0x04	\n
																															* Module Number: RX Data Module Number \n
																															* Async Bit: OFF	\n
																															* Allocate Bit: OFF	\n
																															* GET Bit: ON	\n
																															* SET Bit: ON	\n
																															*/
    RX_DATA_ADD_RX_DATA_FILTER                 	=   SET_BIT           | RX_DATA_MODULE_PARAM | 0x05,						/**< RX Data Add Filter Parameter (RX Data Module Set Command): \n  
																															* Used for adding RX Data Filter to FW\n
																															* Done Sync with no memory allocation\n 
																															* Parameter Number:	0x05	\n
																															* Module Number: RX Data Module Number \n
																															* Async Bit: OFF	\n
																															* Allocate Bit: OFF	\n
																															* GET Bit: OFF	\n
																															* SET Bit: ON	\n
																															*/
    RX_DATA_REMOVE_RX_DATA_FILTER              	=   SET_BIT           | RX_DATA_MODULE_PARAM | 0x06,						/**< RX Data Remove Filter Parameter (RX Data Module Set Command): \n  
																															* Used for removing RX Data Filter from FW\n
																															* Done Sync with no memory allocation\n 
																															* Parameter Number:	0x06	\n
																															* Module Number: RX Data Module Number \n
																															* Async Bit: OFF	\n
																															* Allocate Bit: OFF	\n
																															* GET Bit: OFF	\n
																															* SET Bit: ON	\n
																															*/
    RX_DATA_GET_RX_DATA_FILTERS_STATISTICS     	=             GET_BIT | RX_DATA_MODULE_PARAM | 0x07 | ASYNC_PARAM,			/**< RX Data Get RX Data Filter Statistics Parameter (RX Data Module Get Command): \n  
																															* Used for getting RX Data Filter Statistics from FW\n
																															* Done Async with no memory allocation\n 
																															* Parameter Number:	0x07	\n
																															* Module Number: RX Data Module Number \n
																															* Async Bit: ON	\n
																															* Allocate Bit: OFF	\n
																															* GET Bit: ON	\n
																															* SET Bit: OFF	\n
																															*/
    

	/* measurement section */
    MEASUREMENT_ENABLE_DISABLE_PARAM			=	SET_BIT |           MEASUREMENT_MODULE_PARAM | 0x01,	/**< Measurement Enable/Disable Parameter (Measurement Module Set Command): \n  
																											* Used for Enabling/Disabling Measurement Management Module\n
																											* Done Sync with no memory allocation\n 
																											* Parameter Number:	0x01	\n
																											* Module Number: Measurement Module Number \n
																											* Async Bit: OFF	\n
																											* Allocate Bit: OFF	\n
																											* GET Bit: OFF	\n
																											* SET Bit: ON	\n
																											*/
	MEASUREMENT_MAX_DURATION_PARAM				=	SET_BIT |           MEASUREMENT_MODULE_PARAM | 0x02,	/**< Measurement Maximum Duration Parameter (Measurement Module Set Command): \n  
																											* Used for updating the Maximum Duration on non serving channel\n
																											* Done Sync with no memory allocation\n 
																											* Parameter Number:	0x02	\n
																											* Module Number: Measurement Module Number \n
																											* Async Bit: OFF	\n
																											* Allocate Bit: OFF	\n
																											* GET Bit: OFF	\n
																											* SET Bit: ON	\n
																											*/

	/* XCC */    
    XCC_CONFIGURATION							=	SET_BIT | GET_BIT | XCC_MANAGER_MODULE_PARAM | 0x01,	/**< XCC Manager Configuration Parameter (XCC Manager Module Set/Get Command): \n  
																											* Used for setting or getting XCC configuration (RogueAP, CCKM, CKIP, All)\n
																											* Done Sync with no memory allocation\n 
																											* Parameter Number:	0x01	\n
																											* Module Number: XCC Manager Module Number \n
																											* Async Bit: OFF	\n
																											* Allocate Bit: OFF	\n
																											* GET Bit: ON	\n
																											* SET Bit: ON	\n
																											*/

	/* MISC section */
	DEBUG_ACTIVATE_FUNCTION						=	SET_BIT | 			MISC_MODULE_PARAM | 0x03,		 	/**< Debug Activate Function Parameter (MISC Module Set Command): \n  
																											* Used for performing debug function\n
																											* Done Sync with no memory allocation\n 
																											* Parameter Number:	0x03	\n
																											* Module Number: MISC Module Number \n
																											* Async Bit: OFF	\n
																											* Allocate Bit: OFF	\n
																											* GET Bit: OFF	\n
																											* SET Bit: ON	\n
																											*/ 

	/* Health Monitoring section */
    HEALTH_MONITOR_CHECK_DEVICE                 =   SET_BIT |           HEALTH_MONITOR_MODULE_PARAM | 0x01,	/**< Health Monitoring Check Device Parameter (Health Monitoring Module Set Command): \n  
																											* Used for sending health check command to FW\n
																											* Done Sync with no memory allocation\n 
																											* Parameter Number:	0x01	\n
																											* Module Number: Health Monitoring Module Number \n
																											* Async Bit: OFF	\n
																											* Allocate Bit: OFF	\n
																											* GET Bit: OFF	\n
																											* SET Bit: ON	\n
																											*/

	/* TWD CoexActivity table */
    TWD_COEX_ACTIVITY_PARAM                			=   SET_BIT | TWD_MODULE_PARAM | TWD_COEX_ACTIVITY_PARAM_ID,		/**< TWD Control CoexActivity Parameter (TWD Control Module Set/Get Command): \n  
																																* Used for getting RTS Threshold from TWD Control Module or setting RTS Threshold to FW and TWD Control Module\n
																																* Done Sync with no memory allocation\n 
																																* Parameter Number:	TWD_COEX_ACTIVITY_PARAM_ID	\n
																																* Module Number: TWD Control Module Number \n
																																* Async Bit: OFF	\n
																																* Allocate Bit: OFF	\n
																																* GET Bit: OFF	\n
																																* SET Bit: ON	\n
																																*/

    /* CurrBss Section */
    CURR_BSS_REGISTER_LINK_QUALITY_EVENT_PARAM     =   SET_BIT | CURR_BSS_MODULE_PARAM | 0x01,	 /**< CurrBss User Defined Trigger Parameter (Roaming Manager Module Set Command): \n  
                                                                                                    * Used for setting user-defined trigger to FW\n
                                                                                                    * Done Sync with no memory allocation\n 
                                                                                                    * Parameter Number:	0x01	\n
                                                                                                    * Module Number: Curr Bss Module Number \n
                                                                                                    * Async Bit: OFF	\n
                                                                                                    * Allocate Bit: OFF	\n
                                                                                                    * GET Bit: OFF	\n
                                                                                                    * SET Bit: ON	\n
                                                                                                    */

	LAST_CMD									=	0x00	/**< Last External Parameter - Dummy, Should always stay Last	*/													

}   EExternalParam;

/* functions */
/*************/

#endif  /* _CMD_INTERFACE_CODES_H_ */
        
