/*
 * TWDriverScan.h
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

#ifndef TWDRIVERSCAN_H
#define TWDRIVERSCAN_H

/** \file  TWDriverScan.h 
 *  \brief TWDriver Scan APIs
 *
 *  \see
 */

#include "tidef.h"
#include "TWDriverRate.h"
#include "public_commands.h"

/*****************************************************************************************
                          Scan Definitions
                          ---------------
This file is included by the TWDriver.h , it should not be included apart. !!!!!!!
*****************************************************************************************/


/*
 ***********************************************************************
 *  Constant definitions.
 ***********************************************************************
 */
#define MAX_NUMBER_OF_CHANNELS_PER_SCAN                     16
#define SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND         MAX_NUMBER_OF_CHANNELS_PER_SCAN
#define SCAN_MAX_NUM_OF_SPS_CHANNELS_PER_COMMAND            16
#define SCAN_DEFAULT_MIN_CHANNEL_DWELL_TIME                 30000
#define SCAN_DEFAULT_MAX_CHANNEL_DWELL_TIME                 60000
#define SCAN_DEFAULT_EARLY_TERMINATION_EVENT                SCAN_ET_COND_DISABLE
#define SCAN_DEFAULT_EARLY_TERMINATION_NUM_OF_FRAMES        0

#define PERIODIC_SCAN_MAX_SSID_NUM      					8
#define PERIODIC_SCAN_MAX_INTERVAL_NUM  					16
#define PERIODIC_SCAN_MAX_CHANNEL_NUM   					37 /* G-14 + A-23 */


 /*
 ***********************************************************************
 *  Enums.
 ***********************************************************************
 */
/** \enum EScanType
 * \brief Scan Type
 *
 * \par Description
 * This Enumeration defines the available scan types.
 *
 * \sa TFileInfo
 */
typedef enum
{
/*	0	*/	SCAN_TYPE_NORMAL_PASSIVE = 0,   /**< Normal passive scan 	*/
/*	1	*/	SCAN_TYPE_NORMAL_ACTIVE,        /**< Normal active scan 	*/
/*	2	*/	SCAN_TYPE_SPS,                  /**< Scheduled Passive scan */
/*	3	*/	SCAN_TYPE_TRIGGERED_PASSIVE,    /**< Triggered Passive scan */
/*	4	*/	SCAN_TYPE_TRIGGERED_ACTIVE,     /**< Triggered Active scan 	*/
/*	5	*/	SCAN_TYPE_NO_SCAN,              /**< No Scan to perform 	*/
/*	6	*/	SCAN_TYPE_PACTSIVE              /**< Passive + Active Scan (used for DFS - driver internal use only!!!) */

} EScanType;

/** \enum EScanEtCondition
 * \brief Scan Early Termonation Condition
 *
 * \par Description
 * This Enumeration defines the different early termination causes.
 *
 * \sa TFileInfo
 */
typedef enum
{
    SCAN_ET_COND_DISABLE     = 0x00,        /**< No early termination is not disabled (Do not perform an early termination scan)*/
    SCAN_ET_COND_BEACON      = 0x10,        /**< Early termination scan on beacon reception 									*/
    SCAN_ET_COND_PROBE_RESP  = 0x20,        /**< Early termination scan on probe response reception 							*/
    SCAN_ET_COND_ANY_FRAME   = 0x30,        /**< Early termination scan on both beacon or probe response reception 				*/
    SCAN_ET_COND_NUM_OF_CONDS= 0x4          /**< Number of early termination conditions 										*/ 

} EScanEtCondition;

/** \enum EScanResultTag
 * \brief Scan Debug Tags
 * 
 * \par Description
 * Enumeration of the differnet Scan Result Tags possible
 * 
 * \sa	
 */
typedef enum
{
/*	0	*/	SCAN_RESULT_TAG_CURENT_BSS = 0,			/**< */
/*	1	*/	SCAN_RESULT_TAG_APPLICATION_ONE_SHOT,	/**< */
/*	2	*/	SCAN_RESULT_TAG_DRIVER_PERIODIC,		/**< */
/*	3	*/	SCAN_RESULT_TAG_APPLICATION_PEIODIC,	/**< */
/*	4	*/	SCAN_RESULT_TAG_MEASUREMENT,			/**< */
/*	5	*/	SCAN_RESULT_TAG_IMMEDIATE,				/**< */
/*	6	*/	SCAN_RESULT_TAG_CONTINUOUS,				/**< */
 /*	7	*/	SCAN_RESULT_TAG_MAX_NUMBER				/**< */

} EScanResultTag;

/** \enum ESsidVisability
 * \brief SSID Visablility Type
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	SCAN_SSID_VISABILITY_PUBLIC = 0,		/**< Visible	*/
/*	1	*/	SCAN_SSID_VISABILITY_HIDDEN				/**< Hidden		*/
} ESsidVisability;

/***********************************************************************
 *  Structure definitions.
 ***********************************************************************
 */
/** \struct TSsid
 * \brief SSID Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT8    len;		  			/**< SSID Length		*/   		
    char        str[ MAX_SSID_LEN ];	/**< SSID string buffer	*/

}  TSsid;

/** \struct TScanNormalChannelEntry
 * \brief Scan Normal Channel Entry
 * 
 * \par Description
 * This structure defines single channel parameters for normal scan operation other than SPS (inc. triggered)
 * 
 * \sa
 */ 
typedef struct
{
    TMacAddr               bssId;                  	/**< BSSID (MAC address) to filter */
    TI_UINT32              maxChannelDwellTime;     /**< Maximum time to stay on the channel if some frames were 
													* received but the early termination limit has not been reached (microseconds) 
													*/
    TI_UINT32              minChannelDwellTime;     /**< Minimum time to stay on the channel if no frames were received (microseconds) */
    EScanEtCondition       earlyTerminationEvent;   /**< Early termination frame type */
    TI_UINT8               ETMaxNumOfAPframes;      /**< Number of frames from the early termination frame types according to the early 
													* Termination Event setting, after which scan is stopped on this channel	
													*/    
    TI_UINT8               txPowerDbm;              /**< Power level used to transmit (for active scan only) (0: no change; 1-5: predefined power level */
    TI_UINT8               channel;                 /**< Channel to scan */

} TScanNormalChannelEntry;

/** \struct TScanSpsChannelEntry
 * \brief Scan SPS Channel Entry
 * 
 * \par Description
 * This structure defines single channel parameters for an SPS scan operation
 * 
 * \sa
 */ 
typedef struct
{
    TMacAddr               bssId;               	/**< BSSID (source is MAC address) to filter */
    TI_UINT32              scanDuration;            /**< Length of time to start scanning the channel (TSF lower 4 bytes) */
    TI_UINT32              scanStartTime;           /**< Exact time to start scanning the channel (TSF lower 4 bytes) */
    EScanEtCondition       earlyTerminationEvent;   /**< Scan early termination frame type */
    TI_UINT8               ETMaxNumOfAPframes;      /**< Number of frames from the early termination frame types according to 
													* the early Termination Event setting, after which scan is stopped on this channel
													*/
    TI_UINT8               channel;                 /**< Channel to scan */

} TScanSpsChannelEntry;

/** \struct TScanChannelEntry
 * \brief Scan Channel Entry
 * 
 * \par Description
 * Holds single channel parameters single-channel parameters for all scan types,
 * either for normal scan or for SPS scan
 * 
 * \sa
 */ 
typedef union
{
    TScanNormalChannelEntry   normalChannelEntry;	/**< Normal scan parameters: channel parameters for all scan types other than SPS 	*/
    TScanSpsChannelEntry      SPSChannelEntry;      /**< SPS scan parameters: channel parameters for SPS type 	*/

} TScanChannelEntry;

/** \struct TScanParams
 * \brief scan operation parameters
 * 
 * \par Description
 * This structure defines parameters for a scan operation
 * 
 * \sa
 */ 
typedef struct
{
    TSsid                  desiredSsid;    		/**< The SSID to search (optional) 												*/
    EScanType              scanType;            /**< Desired scan type (normal - active or passive, SPS, triggered - active or passive)	*/
    ERadioBand             band;             	/**< Band to scan (A / BG) 														*/
    TI_UINT8               probeReqNumber;     	/**< Number of probe requests to send on each channel (for active scan) 		*/
    ERateMask              probeRequestRate;    /**< The rate at which to send the probe requests 								*/
    TI_UINT8               Tid;                 /**< Time at which to trigger the scan (for triggered scan)						*/
    TI_UINT64              latestTSFValue;      /**< For SPS scan: the latest TSF at which a frame was received. Used to detect 
												* TSF error (AP recovery). 
												*/
    TI_UINT32              SPSScanDuration;    	/**< For SPS scan ONLY: the time duration of the scan (in milliseconds), used to 
												* Set timer according to. Used to set scan-complete timer 
												*/
    TI_UINT8               numOfChannels;       /**< Number of channels to scan 														*/
    TScanChannelEntry      channelEntry[ MAX_NUMBER_OF_CHANNELS_PER_SCAN ];	/**< Channel data array, actual size according to the above field. */

} TScanParams;

/** \struct TPeriodicScanSsid
 * \brief Periodic Scan SSID
 * 
 * \par Description
 * This structure defines parameters for Periodic scan for SSID
 * 
 * \sa
 */ 
typedef struct
{
    ESsidVisability eVisability;	/**< Indicates if SSID Visible or not	*/	
    TSsid           tSsid;			/**< The Parameters of Scaned SSID 		*/
} TPeriodicScanSsid;

/** \struct TPeriodicChannelEntry
 * \brief Periodic Channel Entry
 * 
 * \par Description
 * This structure defines a Channel Entry of Periodic scan
 * (each scanned channel has its own Channel Entry)
 * 
 * \sa
 */
typedef struct
{
    ERadioBand      eBand;			  	/**< Channel's Radio Band									*/  	
    TI_UINT32       uChannel;			/**< Channel's Number										*/
    EScanType       eScanType;			/**< The Type of Scan Performed on the channel				*/
    TI_UINT32       uMinDwellTimeMs;	/**< minimum time to dwell on the channel, in microseconds	*/
    TI_UINT32       uMaxDwellTimeMs;	/**< maximum time to dwell on the channel, in microseconds	*/
    TI_UINT32       uTxPowerLevelDbm;	/**< Channel's Power Level In Dbm/10 units 		 			*/
} TPeriodicChannelEntry;

/** \struct TPeriodicScanParams
 * \brief Periodic Scan Parameters
 * 
 * \par Description
 * This structure defines all the parameters of Periodic scan
 * 
 * \sa
 */
typedef struct
{
    TI_UINT32               uSsidNum;												/**< Number of Desired SSID scanned			 					*/
    TI_UINT8				uSsidListFilterEnabled;										/** 1: eneable filtering according to the list; 0: disable  */
    TPeriodicScanSsid       tDesiredSsid[ PERIODIC_SCAN_MAX_SSID_NUM ];				/**< Buffer of size of maximum possible Periodic Scanned SSIDs. 
																					* This buffer holds the Parameters of Desired SSIDs (for each SSID: 
																					* visibility, length, string buffer) --> number of init entries in 
																					* buffer: uSsidNum	
																					*/
    TI_UINT32               uCycleNum;												/**< number of Scan cycles to run 						   		*/ 								
    TI_UINT32               uCycleIntervalMsec[ PERIODIC_SCAN_MAX_INTERVAL_NUM ];	/**< Intervals (in Msec) between two sequential  scan cycle    	*/
    TI_INT8                 iRssiThreshold;											/**< RSSI threshold 											*/
    TI_INT8                 iSnrThreshold;											/**< SNR threshold	 											*/
    TI_UINT32               uFrameCountReportThreshold;								/**< Report after N results are received 						*/
    TI_BOOL                 bTerminateOnReport;										/**< Indicates if to Terminate after report 					*/
    ScanBssType_e           eBssType;												/**< Scan BSS Type												*/
    TI_UINT32               uProbeRequestNum;										/**< Number of probe requests to transmit per SSID				*/
    TI_UINT32               uChannelNum;											/**< Number of Scaned Channels									*/
    TPeriodicChannelEntry   tChannels[ PERIODIC_SCAN_MAX_CHANNEL_NUM ];				/**< Buffer of size of maximum possible Periodic Scanned Channels. 
																					* This buffer holds the Parameters of each Scanned Channel	
																					*/
} TPeriodicScanParams;

#endif /* TWDRIVERSCAN_H */





