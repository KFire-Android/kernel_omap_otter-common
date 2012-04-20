/*
 * scanMngr.h
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

/** \file  scanMngr.h
 *  \brief This file include private definitions for the scan manager module.
 *
 *  \see   scanMngr.c, scanMngrApi.h, scanMngrTypes.h
 */


#ifndef __SCAN_MNGR_H__
#define __SCAN_MNGR_H__

#include "scanMngrApi.h"

/*
 ***********************************************************************
 *  Constant definitions.
 ***********************************************************************
 */

/* SPS guard times */
#ifdef TI_DBG
#define SCAN_SPS_GUARD_FROM_CURRENT_TSF         300000 /* 300 msecs - to allow for some debug printouts */
#else
#define SCAN_SPS_GUARD_FROM_CURRENT_TSF         50000 /* 50 msecs */
#endif /* TI_DBG */
#define SCAN_SPS_GUARD_FROM_LAST_BSS            2000 /* 2 msecs */
#define SCAN_SPS_DURATION_PART_IN_ADVANCE       4 /* 1/4 of scan duration in advance */
#define SCAN_SPS_USE_DRIFT_COMPENSATION         1 /* if defined, use drift compensation algorithm */
#define SCAN_SPS_NUM_OF_TSF_DELTA_ENTRIES       4 /* number of TSF delta ^ 2 entries */
#define SCAN_SPS_FW_DTIM_LENGTH                 1000 /* time (in usec) for a DTIM event to complete in the FW */

/* Quality calculation constants */
#define RSSI_PREVIOUS_COEFFICIENT               9

/* scan iteration number after which, if no new AP was found, pre-auth needs to be re-done */
#define SCAN_MNGR_CONSEC_SCAN_ITER_FOR_PRE_AUTH 50

#define MAX_DESC_LENGTH                         50 /* max characters for a description string */
#define SCAN_MNGR_STAT_MAX_TRACK_FAILURE        10 /* max track filures for statistics histogram */

#ifdef TI_DBG
/*#define SCAN_MNGR_DBG 1
#define SCAN_MNGR_SPS_DBG 1
#define SCAN_MNGR_DTIM_DBG 1 */
#endif

 /*
 ***********************************************************************
 *  Enums.
 ***********************************************************************
 */

 /** \enum scan_immedScanState_e 
 * \brief enumerates immediate scan internal status
 */
typedef enum
{
    SCAN_ISS_IDLE = 0,                  /**< immediate scan is not running */
    SCAN_ISS_G_BAND,                    /**< immediate scan is running on G band */
    SCAN_ISS_A_BAND,                    /**< immediate scan is running on A band */
    SCAN_ISS_STOPPING,                  /**< stop was requested and is now pending */
    SCAN_ISS_NUM_OF_STATES              /**< number of available immediate scan states states */
} scan_immedScanState_e;

/** \enum scan_contScanState_e 
 * \brief enumerates continuous scan internal status
 */
typedef enum
{
    SCAN_CSS_IDLE = 0,                  /**< continuous scan is not running */
    SCAN_CSS_TRACKING_G_BAND,           /**< continuous scan is performing tracking scan on G */
    SCAN_CSS_TRACKING_A_BAND,           /**< continuous scan is performing tracking scan on A */
    SCAN_CSS_DISCOVERING,               /**< continuous scan is performing discovery scan */
    SCAN_CSS_STOPPING,                  /**< continuous scan is waiting for scan complete notification */
    SCAN_CSS_NUM_OF_STATES              /**< number of available continuous scan states */
} scan_contScanState_e;

/** \enum scan_discoveryPart_e 
 * \brief enumerates the different parts in the discovery process
 */
typedef enum
{
    SCAN_SDP_NEIGHBOR_G = 0,            /**< attempting to discover neighbor APs in G band */
    SCAN_SDP_NEIGHBOR_A,                /**< attempting to discover neighbor APs in A band */
    SCAN_SDP_CHANNEL_LIST_G,            /**< attempting to discover all APs in G band */
    SCAN_SDP_CHANNEL_LIST_A,            /**< attempting to discover all APs in A band */
    SCAN_SDP_NO_DISCOVERY,              /**< no discovery should be attempted */
    SCAN_SDP_NUMBER_OF_DISCOVERY_PARTS  /**< number of discovery parts available */
} scan_discoveryPart_e;

/** \enum scan_neighborDiscoveryState_e
 * \brief enumerates the different discovery states possible for a neighbor AP
 */
typedef enum
{
    SCAN_NDS_DISCOVERED = 0,            /**< Neighbor AP was discovered and is now being tracked */
    SCAN_NDS_NOT_DISCOVERED,            /**< Neighbor AP was not yet discovered, and should be */
    SCAN_NDS_CURRENT_AP,                /**< 
                                         * Neighbor AP is the AP STA is currently connected to, 
                                         * and shouldn't be discovered
                                         */
    SCAN_NDS_NUMBER_OF_NEIGHBOR_DISCOVERY_STATES
                                        /**< number of available neighbor disocvery states */
} scan_neighborDiscoveryState_e;

/*
 ***********************************************************************
 *  Typedefs.
 ***********************************************************************
 */

#define WAS_SPS_CHANNEL_ATTENDED( SPSStatus, i )    \
    (0 != (SPSStatus & (1<<i)) ? TI_TRUE : TI_FALSE)

/*
 ***********************************************************************
 *  Structure definitions.
 ***********************************************************************
 */

/** \struct scan_neighborAPListDiscovery_t
 * \brief This structure contains Neighbor AP list and their detection status
 */
typedef struct
{
    TI_UINT8                            numOfEntries;                                   /**< number of entries in the list */
    neighborAP_t                    APListPtr[ MAX_NUM_OF_NEIGHBOR_APS ];           /**< neighbor APs list */
    scan_neighborDiscoveryState_e   trackStatusList[ MAX_NUM_OF_NEIGHBOR_APS ];     /**< tracking status list */
} scan_neighborAPListDiscovery_t;

/** \struct scan_BSSEntry
 * \brief This structure contains information relevant only for scan manager module on a BSS
 */
typedef struct
{
    TI_UINT8                    trackFailCount;                 /**< number of consecutive failed track attempts */
    TI_UINT64                   localTSF;                       /**<
                                                             * the TSF of the AP the station is connected to at the
                                                             * reception of the last frame from this AP
                                                             */
#ifdef SCAN_SPS_USE_DRIFT_COMPENSATION
    TI_INT64               prevTSFDelta;                                               /**< Previous TSF delta */
    TI_INT32               deltaChangeArray[ SCAN_SPS_NUM_OF_TSF_DELTA_ENTRIES ];      /**< 
                                                                                     * Array holding deltas 
                                                                                     * between prev. TSF delta
                                                                                     */
    int                 deltaChangeArrayIndex;                                      /**< 
                                                                                     * index to where next entry
                                                                                     * in the delta array should
                                                                                     * be stored
                                                                                     */
#endif
} scan_BSSEntry_t;

/** \struct scan_BSSList
 * \brief This structure contains the BSS tracking list.
 */
typedef struct
{
    TI_UINT8               numOfEntries;                                               /**< Number of entries in the list */
    bssEntry_t          BSSList[ MAX_SIZE_OF_BSS_TRACK_LIST ];                      /**< BSS public information */
    scan_BSSEntry_t     scanBSSList[ MAX_SIZE_OF_BSS_TRACK_LIST ];                  /**< 
                                                                                     * BSS scan manager private 
                                                                                     * information
                                                                                     */
} scan_BSSList_t;

/** \struct scan_SPSHelper_t
 * \brief This structure contains information used for building SPS scan command
 */
typedef struct
{
    TI_UINT64                          nextEventTSF;                                   /**< 
                                                                                     * local TSF value of AP next frame 
                                                                                     * transmission
                                                                                     */
    int                             trackListIndex;                                 /**< index to BSS info in the track list */
    int                             nextAPIndex;                                    /**< index of next AP entry */
} scan_SPSHelper_t;

#ifdef TI_DBG
/** \struct scan_mngrStat_t
 * \brief holds all scan manager statistics
 */
typedef struct
{
    TI_UINT32      receivedFrames;                                     /**< Number of scan results received */
    TI_UINT32      discardedFramesLowRSSI;                             /**< 
                                                                     * Number of frames discarded due 
                                                                     * to RSSI lower than threshold
                                                                     */
    TI_UINT32      discardedFramesOther;                               /**< 
                                                                     * Number of frames discarded due to 
                                                                     * other reasons (invalid band, 
                                                                     * list full)
                                                                     */
    TI_UINT32      SPSSavedByDTIMCheck;                                /**<
                                                                     * Number of SPS scans that were moved
                                                                     * due to the DTIM collision detection mechanism
                                                                     */
    TI_UINT32      APsRemovedDTIMOverlap;                              /**< 
                                                                     * Number of times APs were removed from
                                                                     * tracking list because all their beacons
                                                                     * collide with current AP DTIMs
                                                                     */
    TI_UINT32      APsRemovedInvalidChannel;                           /**<
                                                                     * Number of times APs were removed from
                                                                     * tracking list because their channel was
                                                                     * not valid for tracking scan type
                                                                     */
    TI_UINT32      TrackingGByStatus[ SCAN_CRS_NUM_OF_RES_STATUS ];    /**< 
                                                                     * Number of track scans on G,
                                                                     * according to completion status
                                                                     */
    TI_UINT32      TrackingAByStatus[ SCAN_CRS_NUM_OF_RES_STATUS ];    /**< 
                                                                     * Number of track scans on A,
                                                                     * according to completion status
                                                                     */
    TI_UINT32      DiscoveryGByStatus[ SCAN_CRS_NUM_OF_RES_STATUS ];   /**< 
                                                                     * Number of discovery scans on G,
                                                                     * according to completion status
                                                                     */
    TI_UINT32      DiscoveryAByStatus[ SCAN_CRS_NUM_OF_RES_STATUS ];   /**< 
                                                                     * Number of discovery scans on A,
                                                                     * according to completion status
                                                                     */
    TI_UINT32      ImmediateGByStatus[ SCAN_CRS_NUM_OF_RES_STATUS ];   /**< 
                                                                     * Number of immediate scans on G,
                                                                     * according to completion status
                                                                     */
    TI_UINT32      ImmediateAByStatus[ SCAN_CRS_NUM_OF_RES_STATUS ];   /**< 
                                                                     * Number of immediate scans on A,
                                                                     * according to completion status
                                                                     */
    TI_UINT32      ConsecutiveTrackFailCountHistogram[ SCAN_MNGR_STAT_MAX_TRACK_FAILURE ];
                                                                    /**< 
                                                                     * Number of consecutive track
                                                                     * fail counts */
    TI_UINT32      SPSChannelsNotAttended[ SCAN_MAX_NUM_OF_SPS_CHANNELS_PER_COMMAND ];
                                                                    /**<
                                                                     * Number of times SPS channels were
                                                                     * not scanned by FW, according to
                                                                     * their location in the scan command
                                                                     */
} scan_mngrStat_t;
#endif

/** \struct scanMngr_t
 * \brief This structure contains the scan manager object data
 */
typedef struct
{
    TTsfDtim                        currTsfDtimMib;
    /* handles to other modules */
    TI_HANDLE                       hOS;                                            /**< handle to the OS object */
    TI_HANDLE                       hReport;                                        /**< handle to the report object */
    TI_HANDLE                       hRegulatoryDomain;                              /**< 
                                                                                     * handle to the regulatory domain
                                                                                     * object
                                                                                     */
    TI_HANDLE                       hScanCncn;                                      /**< 
                                                                                     * handle to the scan concentrator
                                                                                     * object
                                                                                     */
    TI_HANDLE                       hRoamingMngr;                                   /**< 
                                                                                     * handle to the roaming manager 
                                                                                     * object
                                                                                     */
    TI_HANDLE                       hSiteMngr;                                      /**< 
                                                                                     * handle to the site manager object*/
    TI_HANDLE                       hTWD; 

    TI_HANDLE                       hTimer;                                         /* handle to the timer module */

    TI_HANDLE                       hEvHandler;                                     /* handle to the eventHandler module */
    TI_HANDLE                       hAPConnection;                                  /* handle to the AP connection module */
    /* start / stop flag */
    TI_BOOL                         bContinuousScanStarted;                         /**<
                                                                                     * Indicates whether continuous scan
                                                                                     * was started
                                                                                     */
    /* Timer */
    TI_HANDLE                       hContinuousScanTimer;                           /**< continuous scan timer object */
    TI_BOOL                         bTimerRunning;                                  /**< 
                                                                                     * indicates whether the timer was 
                                                                                     * started
                                                                                     */

    /* scan policy */
    TScanPolicy                     scanPolicy;                                     /**< scan policy */
    TI_BOOL                         bLowQuality;                                    /**<
                                                                                     * Indicates whether to use the low
                                                                                     * quality time or normal quality
                                                                                     * timer for continuous scan.
                                                                                     */

    /* tracking and discovery information */
    scan_contScanState_e            contScanState;                                  /**< current continuous scan state */
    scan_immedScanState_e           immedScanState;                                 /**< current immediate scan state */
    TI_BOOL                         bImmedNeighborAPsOnly;                          /**< 
                                                                                     * whether immediate scan is to search
                                                                                     * for neighbor AP's only
                                                                                     */
    scan_neighborAPListDiscovery_t  neighborAPsDiscoveryList[ RADIO_BAND_NUM_OF_BANDS ];
                                                                                    /**< 
                                                                                     * List of neighbor APs and their
                                                                                     * discovery status
                                                                                     */
    TI_UINT8                        neighborAPsDiscoveryIndex[ RADIO_BAND_NUM_OF_BANDS ];
                                                                                    /**< 
                                                                                     * Indexes for the neighbor APs
                                                                                     * discovery lists
                                                                                     */
    TI_UINT8                        channelDiscoveryIndex[ RADIO_BAND_NUM_OF_BANDS ];  /**< Indexes for the channels lists */
    scan_discoveryPart_e            currentDiscoveryPart;                           /**< current discovery part */
    TI_BOOL                         bSynchronized;                                  /**< 
                                                                                     * TI_TRUE if SPS data is synchronized
                                                                                     * (no TSF error event occurred)
                                                                                     */
    TI_UINT64                       currentTSF;                                     /**< 
                                                                                     * the local current TSF value 
                                                                                     */
    TI_UINT32                       currentHostTimeStamp;                           /**<
                                                                                     * The current local host time stamp
                                                                                     * (at the time of the above TSF value)
                                                                                     */
    TI_UINT8                        lastLocalBcnDTIMCount;                          /**<
                                                                                     * the DTIM count at the last 
                                                                                     * local beacon reception in the FW
                                                                                     */
    TI_UINT64                       lastLocalBcnTSF;                                /**<
                                                                                     * The local TSF value at the last 
                                                                                     * local beacon reception
                                                                                     */ 
    TMacAddr                        currentBSS;                                     /**< MAC address of current BSS */
    ERadioBand                      currentBSSBand;                                 /**< band of current BSS */
    TI_UINT32                       currentBSSBeaconInterval;                       /**< Beacon interval of current BSS */
    TI_UINT32                       currentBSSDtimPeriod;                           /**< DTIM period of current BSS */
    TI_BOOL                         bNewBSSFound;                                   /**< 
                                                                                     * Indicates whether a new BSS was
                                                                                     * found during the last discovery
                                                                                     * stage
                                                                                     */
    TI_UINT16                       consecNotFound;                                 /**<
                                                                                     * consecutive number of scan 
                                                                                     * cycles in which no new AP was found
                                                                                     * (used to re-pre-auth current APs)
                                                                                     */
    TScanParams                     scanParams;                                     /**< temporary storage for scan command */
    scan_BSSList_t                  BSSList;                                        /**< BSS list (also used for tracking) */

    scanMngr_connStatus_e           connStatus;                                /* save the connection status during manual roaming */
	TI_UINT8                        scanningOperationalMode;                   /* 0 - manual ,  1 - auto */
    TScanParams                     manualScanParams;                          /* temporary storage for manual scan command */



#ifdef TI_DBG
    scan_mngrStat_t                 stats;                                          /**< statistics */
    ERadioBand                      statsLastDiscoveryBand;                         /**< 
                                                                                     * For statistics: the band on which
                                                                                     * discovery was last performed.
                                                                                     */
#endif

} scanMngr_t;

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
 * \date 01-Mar-2005\n
 * \brief Frees scan manager resources.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrFreeMem (TI_HANDLE hScanMngr);

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Starts a continuous scan operation.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrPerformContinuousScan( TI_HANDLE hScanMngr );

void scanMngrGetCurrentTsfDtimMibCB(TI_HANDLE hScanMngr, TI_STATUS status, TI_UINT8* CB_buf) ;
void scanMngr_GetUpdatedTsfDtimMibForScan (TI_HANDLE hScanMngr, TI_BOOL bTwdInitOccured) ;

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Perform aging on the BSS list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrPerformAging( TI_HANDLE hScanMngr );

/**
 * \\n
 * \date 01-Mar-2005\n
 * \brief Updates object data according to a received frame.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param frameInfo - all frame related information.\n
 */
void scanMngrUpdateReceivedFrame( TI_HANDLE hScanMngr, TScanFrameInfo* frameInfo );

/**
 * \\n
 * \date 17-Mar-2005\n
 * \brief Cerate a new tracking entry and store the newly discovered AP info in it.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param frameInfo - a pointer to the information received from this AP.\n
 */
void scanMngrInsertNewBSSToTrackingList( TI_HANDLE hScanMngr, TScanFrameInfo* frameInfo );

/**
 * \\n
 * \date 17-Mar-2005\n
 * \brief Updates tracked AP information.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param BSSListIndex - index to the BSS list where the AP information is stored.\n
 * \param frameInfo - a pointer to the information received from this AP.\n
 */
void scanMngrUpdateBSSInfo( TI_HANDLE hScanMngr, TI_UINT8 BSSListIndex, TScanFrameInfo* frameInfo );

/**
 * \\n
 * \date 16-Mar-2005\n
 * \brief Searched tracking list for an entry matching given BSSID.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bssId - the BSSID to search for.\n
 * \return entry index if found, -1 if no entry matching the BSSID was found.\n
 */
TI_INT8 scanMngrGetTrackIndexByBssid( TI_HANDLE hScanMngr, TMacAddr* bssId );

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Search current policy for band policy
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param band - the band to find policy for.\n
 * \return the policy structure if found, NULL if no policy configured for this band.\n
 */
TScanBandPolicy* scanMngrGetPolicyByBand( TI_HANDLE hScanMngr, ERadioBand band );

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Sets the next discovery part according to current discovery part, policies and neighbor APs availability .\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrSetNextDiscoveryPart( TI_HANDLE hScanMngr );

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Checks whether discovery should be performed on the specified discovery part.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param discoveryPart - the discovery part to check.\n
 */
TI_BOOL scanMngrIsDiscoveryValid( TI_HANDLE hScanMngr, scan_discoveryPart_e discoveryPart );

/**
 * \\n
 * \date 07-Mar-2005\n
 * \brief Check whether there are neighbor APs to track on the given band.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - The scan policy for the requested band.\n
 * \param bNeighborAPsOnly - whether to scan for neighbor APs only or for all policy defined channels.\n
 */
TI_BOOL scanMngrNeighborAPsAvailableForDiscovery( TI_HANDLE hScanMngr, ERadioBand band );

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Builds a scan command on the object workspace for immediate scan.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - The scan policy for the requested band.\n
 * \param bNeighborAPsOnly - whether to scan for neighbor APs only or for all policy defined channels.\n
 */
void scanMngrBuildImmediateScanCommand( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy, TI_BOOL bNeighborAPsOnly );

/**
 * \\n
 * \date 03-Mar-2005\n
 * \brief Builds a scan command on the object workspace for tracking.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - The scan policy for the band to track on.\n
 * \param band - the band to scan.\n
 */
void scanMngrBuildTrackScanCommand( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy, ERadioBand band );

/**
 * \\n
 * \date 03-Mar-2005\n
 * \brief Builds a scan command on the object workspace for discovery.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrBuildDiscoveryScanCommand( TI_HANDLE hScanMngr );

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Builds the scan command header on the object workspace.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param scanMethod - The scan method (and parameters) to use.\n
 * \param band - the band to scan.\n
 */
void scanMngrBuildScanCommandHeader( TI_HANDLE hScanMngr, TScanMethod* scanMethod, ERadioBand band );

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Add neighbor APs to scan command on the object workspace for discovery scan.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - the scan policy for the band to use.\n
 */
void scanMngrAddNeighborAPsForDiscovery( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy );

/**
 * \\n
 * \date 06-Mar-2005\n
 * \brief Add channel from policy channels list to scan command on the object workspace for discovery scan.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bandPolicy - the scan policy for the band to use.\n
 */
void scanMngrAddChannelListForDiscovery( TI_HANDLE hScanMngr, TScanBandPolicy* bandPolicy );

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Add SPS channels to scan command on the object workspace.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param scanMethod - The scan method (and parameters) to use.\n
 * \param band - the band to scan.\n
 */
void scanMngrAddSPSChannels( TI_HANDLE hScanMngr, TScanMethod* scanMethod, ERadioBand band );

/**
 * \\n
 * \date 07-Mar-2005\n
 * \brief Calculates local TSF of the next event (beacon or GPR) of the given tracked AP.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param BSSList - a pointer to the track list.\n
 * \param entryIndex - the index of the AP for which calculation is requires in the tracking list.\n
 * \param initialTSFValue - local TSF value AFTER which the next event is to found.\n
 * \return The approximate current TSF
 */
TI_UINT64 scanMngrCalculateNextEventTSF( TI_HANDLE hScanMngr, scan_BSSList_t* BSSList, TI_UINT8 entryIndex, TI_UINT64 initialTSFValue );

/**
 * \\n
 * \date 20-September-2005\n
 * \brief Check whether a time range collides with current AP DTIM
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param rangeStart - the time range start TSF.\n
 * \param eventEnd - the time range end TSF.\n
 * \return Whether the event collides with a DTIM (TRUF if it does, TI_FALSE if it doesn't).\n
 */
TI_BOOL scanMngrDTIMInRange( TI_HANDLE hScanMngr, TI_UINT64 eventStart, TI_UINT64 eventEnd );

/**
 * \\n
 * \date 03-Mar-2005\n
 * \brief Add a normal channel entry to the object workspace scan command.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param scanMethod - The scan method (and parameters) to use.\n
 * \param channel - the channel index.\n
 * \param BSSID - pointer to the BSSID to use (may be broadcast.\n
 * \param txPowerDbm - Tx power to transmit probe requests.\n
 */
void scanMngrAddNormalChannel( TI_HANDLE hScanMngr, TScanMethod* scanMethod, TI_UINT8 channel, 
                               TMacAddr* BSSID, TI_UINT8 txPowerDbm );
/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Removes an entry from the BSS list (by replacing it with another entry, if any).\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param BSSEntryIndex - index of the entry to remove.\n
 */
void scanMngrRemoveBSSListEntry( TI_HANDLE hScanMngr, TI_UINT8 BSSEntryIndex );

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Removes all BSS list entries that are neither neighbor APs not on a policy defined channel.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param bCheckNeighborAPs - whether to verify that APs marked as neighbor APs are really neighbor APs.\n
 * \param bCheckChannels - whether to verify that APs not marked as neighbor APs are on policy defined channel.\n
 */
void scanMngrUpdateBSSList( TI_HANDLE hScanMngr, TI_BOOL bCheckNeighborAPs, TI_BOOL bCheckChannels );

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief returns the index of a neighbor AP.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param band - the band on which the AP resides.\n
 * \param bssId - the AP's BSSID.\n
 * \return the index into the neighbor AP list for the given address, -1 if AP is not in list.\n
 */
TI_INT8 scanMngrGetNeighborAPIndex( TI_HANDLE hScanMngr, ERadioBand band, TMacAddr* bssId );

/**
 * \\n
 * \date 02-Mar-2005\n
 * \brief Checks whether a channel is defined on a policy.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param band - the band on which the channel is.\n
 * \param channel - the channel number.\n
 * \return TI_TRUE if channel is defined on policy, TI_FALSE otherwise.\n
 */
TI_BOOL scanMngrIsPolicyChannel( TI_HANDLE hScanMngr, ERadioBand band, TI_UINT8 channel );

/**
 * \\n
 * \date 18-Apr-2005\n
 * \brief Converts scan concentrator result status to scan manager result status, to be returned to roaming manager.\n
 *
 * Function Scope \e Private.\n
 * \param result status - scan concentrator result status.\n
 * \return appropriate scan manager status.\n
 */
scan_mngrResultStatus_e scanMngrConvertResultStatus( EScanCncnResultStatus resultStatus );

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a neighbor AP list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param neighborAPList - the list of neighbor APs to print
 */
void scanMngrTracePrintNeighborAPsList( TI_HANDLE hScanMngr, neighborAPList_t *neighborAPList );

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a neighbor AP.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param neighborAP - the neighbor AP to print
 */
void scanMngrTracePrintNeighborAP( TI_HANDLE hScanMngr, neighborAP_t* neighborAP );

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a band scan policy AP.\n
 *
 * Function Scope \e Private.\n
 * \param bandPolicy - the band scan policy to print.\n
 */
void scanMngrTracePrintBandScanPolicy( TScanBandPolicy* bandPolicy );

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief Print a scan method
 *
 * Function Scope \e Private.\n
 * \param scanMethod - the scan method to print.\n
 */
void scanMngrTracePrintScanMethod( TScanMethod* scanMethod );

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief print a normal scan method
 *
 * Function Scope \e Private.\n
 * \param basicMethodParams - the basic method parameters to print.\n
 */
void scanMngrTracePrintNormalScanMethod( TScanBasicMethodParams* basicMethodParams );

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief print an AC triggered scan method
 *
 * Function Scope \e Private.\n
 * \param triggeredMethodParams - the Tid-triggered method parameters to print.\n
 */
void scanMngrTracePrintTriggeredScanMethod( TScanTidTriggeredMethodParams* triggeredMethodParams );

/**
 * \\n
 * \date 09-Mar-2005\n
 * \brief print a SPS scan method
 *
 * Function Scope \e Private.\n
 * \param SPSMethodParams - the SPS method parameters to print.\n
 */
void scanMngrTracePrintSPSScanMethod( TScanSPSMethodParams* SPSMethodParams );

#ifdef TI_DBG
/**
 * \\n
 * \date 31-Mar-2005\n
 * \brief print debug information for every received frame.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param frameInfo - holding all frame related information.\n
 */
void scanMngrDebugPrintReceivedFrame( TI_HANDLE hScanMngr, TScanFrameInfo *frameInfo );

/**
 * \\n
 * \date 31-Mar-2005\n
 * \brief print BSS list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 */
void scanMngrDebugPrintBSSList( TI_HANDLE hScanMngr );

/**
 * \\n
 * \date 31-Mar-2005\n
 * \brief print one entry in the BSS list.\n
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param entryIndex - the index of the entry to print.\n
 */
void scanMngrDebugPrintBSSEntry( TI_HANDLE hScanMngr, TI_UINT8 entryIndex );

/**
 * \\n
 * \date 14-Apr-2005\n
 * \brief print SPS helper list
 *
 * Function Scope \e Private.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param spsHelperList - the list to print.\n
 * \param arrayHead - the index of the first element in the list.\n
 * \param arraySize - the size of the array.\n
 */
void scanMngrDebugPrintSPSHelperList( TI_HANDLE hScanMngr, scan_SPSHelper_t* spsHelperList, int arrayHead, int arraySize );

/**
 * \\n
 * \date 26-May-2005\n
 * \brief Print scan result histogram statistics.\n
 *
 * Function Scope \e Private.\n
 * \param scanResultHistogram - Scan results histogram (by scan complete reason).\n
 */
void scanMngrStatsPrintScanResultHistogram( TI_UINT32 scanResultHistogram[] );

/**
 * \\n
 * \date 26-May-2005\n
 * \brief Print track fail count histogram statistics.\n
 *
 * Function Scope \e Private.\n
 * \param trackFailHistogram - tracking failure histogram (by tracking retry).\n
 */
void scanMngrStatsPrintTrackFailHistogrsm( TI_UINT32 trackFailHistogram[] );

/**
 * \\n
 * \date 26-May-2005\n
 * \brief Print SPS attendant channel histogram statistics.\n
 *
 * Function Scope \e Private.\n
 * \param SPSChannelsNotAttendedHistogram - SPS channels attendant histogram.\n
 */
void scanMngrStatsPrintSPSChannelsHistogram( TI_UINT32 SPSChannelsNotAttendedHistogram[] );

/**
 * \\n
 * \date 25-July-2005\n
 * \brief Print One neighbor AP entry.\n
 *
 * Function Scope \e Private.\n
 * \param pNeighborAp - pointer to the neighbor AP data.\n
 * \param discovery state - the discovery state of this neighbor AP.\n
 */
void scanMngrDebugPrintNeighborAP( neighborAP_t* pNeighborAp, scan_neighborDiscoveryState_e discoveryState );

/**
 * \\n
 * \date 27-July-2005\n
 * \brief Prints a scan command.\n
 *
 * Function Scope \e Private.\n
 * \param pScanParams - a pointer to the scan parameters structure.\n
 */
void scanMngrDebugPrintScanCommand( TScanParams* pScanParams );

/**
 * \\n
 * \date 27-July-2005\n
 * \brief Prints scan command single normal channel.\n
 *
 * Function Scope \e Private.\n
 * \param pNormalChannel - a pointer to the normal channel to print.\n
 */
void scanMngrDebugPrintNormalChannelParam( TScanNormalChannelEntry* pNormalChannel );

/**
 * \\n
 * \date 27-July-2005\n
 * \brief Prints scan command single SPS channel.\n
 *
 * Function Scope \e Private.\n
 * \param pSPSChannel - a pointer to the SPS channel to print.\n
 */
void scanMngrDebugPrintSPSChannelParam( TScanSpsChannelEntry* pSPSChannel );


#endif /* TI_DBG */

#endif /* __SCAN_MNGR_H__ */
