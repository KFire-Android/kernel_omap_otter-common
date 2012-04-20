/*
 * paramOut.h
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
/*    MODULE:   paramOut.h													*/
/*    PURPOSE:																*/
/*																			*/
/****************************************************************************/

#ifndef __PARAM_OUT_H__
#define __PARAM_OUT_H__

#include "tidef.h"
#include "report.h"
#include "context.h"
#include "rate.h"
#include "bssTypes.h"
#include "roamingMngrTypes.h"
#include "scanMngrTypes.h"

#ifdef XCC_MODULE_INCLUDED
#include "paramOutXCC.h"
#else
#define   XCC_PARAM_FIELDS
#endif
 
#include "InternalCmdCodes.h"
#include "commonTypes.h"
#include "coreDefaultParams.h"
#include "rsnApi.h"
#include "TWDriver.h"
#include "tiwlnif.h"


#define DOT11_MAX_DEFAULT_WEP_KEYS          4

#define RX_LEVEL_TABLE_SIZE                 15

#define RX_DATA_FILTER_MAX_FIELD_PATTERNS   8
#define RX_DATA_FILTER_FILTER_BOUNDARY      256

/* Soft gemini  values */

/* Used by UtilInfoCodeQueryInformation , UtilInfoCodeSetInformation */
#define VAL_TX_POWER_VALUE			100
#define VAL_NETWORK_TYPE			101
#define VAL_AP_TX_POWER_LEVEL	    102
/* #define VAL_COUNTRY_CODE    	        103 */ 
/* #define VAL_REG_DOMAIN_BAND_24	    104 */
/* #define VAL_REG_DOMAIN_BAND_50	    105 */
#define VAL_PACKET_BURSTING			106
#define VAL_MIXED_MODE				107
#define VAL_PRIVACY_MODE			108
#define VAL_XCC_SECURITY			109
#define VAL_DEFAULT_KEY_ID			110
#define VAL_AP_SUPPORT_CHANELS 		111

typedef struct
{
    ERate    maxBasic;
    ERate    maxActive;
} ratePair_t;


typedef enum
{
    RTS_CTS_DISABLED = 0,
    RTS_CTS_ENABLED  = 1

} RtsCtsStatus_e;

/* Parameters Structures Definitions per parameter type */
typedef enum
{
    AUTH_LEGACY_OPEN_SYSTEM     = 0,
    AUTH_LEGACY_SHARED_KEY      = 1,
    AUTH_LEGACY_AUTO_SWITCH     = 2,
    AUTH_LEGACY_RESERVED1       = 128,
    AUTH_LEGACY_NONE            = 255
} legacyAuthType_e;

typedef enum
{
    CONNECTION_NONE             = 0,
    CONNECTION_INFRA            = 1,
    CONNECTION_IBSS             = 2,
    CONNECTION_SELF             = 3
} connectionType_e;

typedef enum
{
    RADIO_IN_STAND_BY           = 0,
    RADIO_OUT_OF_STAND_BY       = 1
}radioStandByState_t;

/**** Regulatory Domain module types ****/

/* Scan Control Table for 2.4-G band type */
typedef struct
{
    TI_UINT8       tableString[NUM_OF_CHANNELS_24];
} scanControlTable24_t;

/* Scan Control Table for 5G-band type */
typedef struct
{
    TI_UINT8       tableString[A_5G_BAND_NUM_CHANNELS];
} scanControlTable5_t;

/* Scan Control Table type */
typedef struct
{
    ERadioBand             band;
    scanControlTable5_t     ScanControlTable5;
    scanControlTable24_t    ScanControlTable24;
} scanControlTable_t;

/** \enum regulatoryDomain_scanOption_e
 * \brief Regulatory Domain Scan Options
 *
 * \par Description
 * Enumerates the scan type to used by regulatory domain queries
 *
 * \sa
 */
typedef enum
{
    ACTIVE_SCANNING     = 0,	/**< The query is for active scanning (requires transmission on the channel)	*/
    PASSIVE_SCANNING    = 1		/**< The query is for passive scanning (no transmission is required)			*/
} regulatoryDomain_scanOption_e;

typedef struct
{
    TI_UINT8*      pChannelBitMap;
    TI_UINT8       channelCnt;
    TI_INT8        txPower;
} regulatoryDomainParam_t;

typedef struct
{
    TI_UINT8       minTxPower;
    TI_UINT8       maxTxPower;
} powerCapability_t;


/* SoftGemini module init parameters */
typedef struct
{
    ESoftGeminiEnableModes  SoftGeminiEnable;
	TI_UINT32   coexParams[SOFT_GEMINI_PARAMS_MAX];
 } SoftGeminiInitParams_t;

typedef enum
{
    PHY_UNKNOWN         = 0,
    PHY_FH              = 1,
    PHY_DSS             = 2,
    PHY_UN_USED         = 3,
    PHY_OFDM            = 4,
    PHY_HIGH_RATE_DSS   = 5,
    PHY_ERP             = 6
} phyType_e;


typedef enum
{
    CLOSE           = 0,
    OPEN_NOTIFY     = 1,
    OPEN_EAPOL      = 2,
    OPEN            = 3,
    MAX_NUM_OF_RX_PORT_STATUS
} portStatus_e;


typedef enum
{
    DRIVER_STATUS_IDLE              = 0,
    DRIVER_STATUS_RUNNING           = 1
} driverStatus_e;

typedef enum
{
    OS_ABS_LAYER    = 0,
    RSN             = 1
} eapolDestination_e;

/* enumerator for PRE_AUTH event */
typedef enum
{
   RSN_PRE_AUTH_START,
   RSN_PRE_AUTH_END
} preAuthStatusEvent_e;


typedef enum
{
    STATUS_SCANNING         = 0,
    STATUS_SCAN_COMPLETE    = 1
} scanStatus_e;

typedef enum
{
    SCAN_DISABLED   = 0,	/* TI_FALSE*/
    SCAN_ENABLED    = 1,	/* TI_TRUE*/
	SKIP_NEXT_SCAN	= 2		/* Skip only one next coming scan, then set this parameter to TI_TRUE*/
} scanEnabledOptions_e;




typedef struct
{
    TI_UINT32      RecvOk;                 /* the number of frames that the NIC receives without errors */
    TI_UINT32      DirectedBytesRecv;      /* the number of bytes in directed packets that are received without errors */
    TI_UINT32      DirectedFramesRecv;     /* the number of directed packets that are received without errors */
    TI_UINT32      MulticastBytesRecv;     /* the number of bytes in multicast/functional packets that are received without errors */
    TI_UINT32      MulticastFramesRecv;    /* the number of multicast/functional packets that are received without errors */
    TI_UINT32      BroadcastBytesRecv;     /* the number of bytes in broadcast packets that are received without errors. */
    TI_UINT32      BroadcastFramesRecv;    /* the number of broadcast packets that are received without errors. */
    TI_UINT32      LastSecBytesRecv;       /* the number of bytes received without errors during last second */

} rxDataCounters_t;

typedef struct rxDataFilterFieldPattern_t
{
    TI_UINT16      offset; /*  Offset of the field to compare from the start of the packet*/
    TI_UINT8       length; /* Size of the field pattern */
    TI_UINT8       flag; /* Bit Mask flag */
    TI_UINT8       pattern[RX_DATA_FILTER_MAX_PATTERN_SIZE]; /* expected pattern */
    TI_UINT8       mask[RX_DATA_FILTER_MAX_PATTERN_SIZE]; /* bit-masking of the internal field content */
} rxDataFilterFieldPattern_t;

typedef struct 
{
	void	*handler;
	void	*callback; 
}QoS_renegVoiceTspecReq_t;

/* Authentication/encryption capability */
#define MAX_AUTH_ENCR_PAIR 13

typedef struct 
{
	EExternalAuthMode   authenticationMode;
	ECipherSuite        cipherSuite;

} authEncrPairList_t;

typedef struct 
{
	TI_UINT32              NoOfPMKIDs;
	TI_UINT32              NoOfAuthEncrPairSupported;
	authEncrPairList_t     authEncrPairs[MAX_AUTH_ENCR_PAIR];

} rsnAuthEncrCapability_t;

typedef struct 
{
	TI_UINT32       numOfPreAuthBssids;
	TMacAddr     	*listOfPreAuthBssid;

} rsnPreAuthBssidList_t;


typedef struct
{
    TI_INT32       rssi;
    TI_UINT8       snr;
} signal_t;

typedef struct
{
    TI_UINT32  basicRateMask;
    TI_UINT32  supportedRateMask;
} rateMask_t;

typedef struct
{

    TI_UINT8        *assocRespBuffer;
    TI_UINT32       assocRespLen;
    TI_UINT8        *assocReqBuffer;
    TI_UINT32       assocReqLen;

} assocInformation_t;

typedef struct
{
    TMacAddr    siteMacAddress;
    TI_BOOL     priority;
} siteMgr_prioritySite_t;

typedef struct{
	TI_UINT32 thresholdCross;                /* high or low */
	TI_UINT32 thresholdCrossDirection;       /* direction of crossing */
} trafficIntensityThresholdCross_t;

/************************************/
/*      QOS edcf params             */
/************************************/

/*
#define CW_MIN_DEF                         15
#define CW_MIN_MAX                         31
#define CW_MAX_DEF                         1023
*/
#define CW_MIN_DEF                         4 /* the power of 2 - cwMin = 2^4-1 = 15 */
#define CW_MIN_MAX                         5 /* the power of 2 - cwMax = 2^5-1 = 31 */
#define CW_MAX_DEF                         10

#define AIFS_DEF                            2
#define NO_RX_TIME_OUT                      0
#define NO_RX_ACK_POLICY                    0
#define DATA_DCF                            0    /* MSDUs are sent completely including retrys - normal legacy traffic */
#define QOS_DATA_EDCF                       1    /* MPDUs are sent according to TXOP limits - */
#define RETRY_PREEMPTION_DISABLE            0
#define QOS_CONTROL_TAG_MASK                0x0007
#define QOS_CONTROL_EOSP_MASK                0x0010



typedef enum{
    AC_ACTIVE = 0,
    AC_NOT_ACTIVE
}acActive;


typedef struct
{
	TI_UINT8	*buffer;
	TI_UINT16	bufLength;
	TI_UINT8	isBeacon; 	/* If true, Beacon packet is returned, otherwise it is Probe Response */
} BufferParameters_t;



typedef struct{
	TI_UINT32		trafficAdmCtrlResponseTimeout;
    TI_BOOL        trafficAdmCtrlUseFixedMsduSize;
}trafficAdmCtrlInitParams_t;

typedef struct{
    TI_BOOL       wmeEnable;
    TI_BOOL       trafficAdmCtrlEnable;
    TI_BOOL       qosTagZeroConverHeader;
	TI_UINT8      PacketBurstEnable;
	TI_UINT32     PacketBurstTxOpLimit;
    TI_UINT32     TxOpLimit[MAX_NUM_OF_AC];
    TI_UINT32     MsduLifeTime[MAX_NUM_OF_AC];
    TRxTimeOut    rxTimeOut;
    TI_UINT8      ShortRetryLimit[MAX_NUM_OF_AC];
    TI_UINT8      LongRetryLimit[MAX_NUM_OF_AC];
    TI_UINT8      desiredWmeAcPsMode[MAX_NUM_OF_AC];        /* wme per ac power save mode */
    EQOverflowPolicy QueueOvFlowPolicy[MAX_NUM_OF_AC];
	TI_UINT8      acAckPolicy[MAX_NUM_OF_AC];               /* ack policy per AC */
    trafficAdmCtrlInitParams_t	trafficAdmCtrlInitParams;
	TI_UINT8	  desiredPsMode;						    /* The desired PS mode of the station */
	TI_UINT8	  desiredMaxSpLen;

    TI_BOOL      bCwFromUserEnable;  /* flag to use CwMin & CwMax user setting: 0 disable user setting (values from beacon) , 1 enable user setting (beacon cw ignore)*/
    TI_UINT8     uDesireCwMin;		/**< The contention window minimum size (in slots) from ini file */
    TI_UINT16    uDesireCwMax;		/**< The contention window maximum size (in slots) from ini file */
	TI_BOOL		 bEnableBurstMode;
 /* Enable the Burst mode from ini file */
    /* 802.11n BA session */
    TI_UINT8               aBaPolicy[MAX_NUM_OF_802_1d_TAGS];
    TI_UINT16              aBaInactivityTimeout[MAX_NUM_OF_802_1d_TAGS];
	
}QosMngrInitParams_t;



/*END OF MULTIPLE QUEUES STRUCTURE*/

typedef struct
{
	TI_UINT16		bufferSize;
	TI_UINT8		*buffer;
    TI_BOOL 		reAssoc;
} TAssocReqBuffer;

typedef struct
{
    TMacAddr	bssID;
    TI_UINT16	channel;
} apChannelPair_t;

typedef struct
{
    apChannelPair_t	*apChannelPairs;
    TI_UINT16      	numOfEntries;
} neighbor_AP_t;

typedef struct
{    
    TI_UINT16          maxChannelDuration;		/* One channel max duration time. (time slot 0 - 65000) */    
    TI_UINT16          minChannelDuration;		/* One channel max duration time. (time slot 0 - 65000) */    
    TI_UINT8           earlyTerminationMode;	/**< 0 = Stay until max duration time. 1 = Terminate scan in
												* a channel upon a reception of Prob-Res or Beacon. 2 = Terminate scan
												* in a channel upon a reception of any frame
												*/    
    TI_UINT8           eTMaxNumOfAPframes;		/**< number of AP frames (beacon/probe_resp) to trigger Early termination.
												* Applicable only when EarlyTerminationMode = 1 
												*/
    TI_UINT8           numOfProbeReq;			/* Number of probe request transmitted on each channel */

} periodicScanParams_t;


typedef struct
{	
	TI_UINT16 		channelNum;
	TI_BOOL		channelValidity;
	ERadioBand		band;
} channelValidity_t;

/** \struct channelCapabilityRet_t
 * \brief Channel Capability Response
 * 
 * \par Description
 * Defines scan capabilities information, which is given as a response to a scan capabilities query.
 * 
 * \sa
 */ 
typedef struct
{
	TI_BOOL 	channelValidity;	/**< Indicates whether the channel is valid for the requested scan type. 
									* TRUE: channel is valid; FALSE: not valid 
									*/
	TI_UINT8	maxTxPowerDbm; 		/**< Maximum TX power level allowed on this channel from 1 to 5, 
									* where 1 is the highest and 5 is the lowest. Units: Dbm/10 
									*/
}	channelCapabilityRet_t;

typedef struct
{
	TI_UINT8		*listOfChannels;
	TI_UINT8		sizeOfList;
} supportedChannels_t;

/** \struct channelCapabilityReq_t
 * \brief Channel Capability Resuest
 * 
 * \par Description
 * Defines the regulatory domain scan capability query information
 * 
 * \sa
 */ 
typedef struct
{
	regulatoryDomain_scanOption_e 	scanOption;	/**< Desired scan type (passive or active)		*/
	TI_UINT8						channelNum; /**< Channel on which scan is to be performed	*/		
	ERadioBand                     	band; 		/**< Band on which scan is to be performed		*/	
}	channelCapabilityReq_t;

typedef struct
{
    TI_UINT32   uChannel;
    ERadioBand  eBand;
    TI_BOOL     bDfsChannel;
} TDfsChannel;

typedef struct
{
	TTxDataCounters 			*pTxDataCounters;
	TI_UINT8				acID;
}	reportTsStatisticsReq_t;

/* SME parameters definition */
typedef enum
{
    CONNECT_MODE_AUTO = 0,
    CONNECT_MODE_MANUAL
} EConnectMode;

/* Generic IE */
#define RSN_MAX_GENERIC_IE_LENGTH 255

typedef struct
{
		TI_UINT8      length;
        TI_UINT8      data[255];
} rsnGenericIE_t;


/** \struct paramInfo_t
 * \brief General Parameters Structure
 * 
 * \par Description
 * This structure holds information for the regulatory domain (and other modules 
 * that are outside of the scope of this document) queries
 * 
 * \sa
 */ 
typedef struct{
    TI_UINT32              paramType;		/**< Parameter identification value */
    TI_UINT32              paramLength;		/**< Parameter actual length (or the length allocated in content for parameter value) */

	/* Actual parameter value */
    union
    {
        /* HAL Control section */
		TI_UINT16							halCtrlRtsThreshold;
		TI_UINT16							halCtrlFragThreshold;

        /* site manager section */
        TI_UINT8                			siteMgrDesiredChannel;
        TMacAddr                			siteMgrDesiredBSSID;
        TSsid                   			siteMgrDesiredSSID;
        ScanBssType_e           			siteMgrDesiredBSSType;
        ratePair_t              			siteMgrDesiredRatePair;
        TRates                 				siteMgrDesiredBasicRateSet;
        TRates                 				siteMgrDesiredSupportedRateSet;
        rateMask_t              			siteMgrCurrentRateMask;
        TI_UINT8                			siteMgrCurrentTxRate;
        TI_UINT8                			siteMgrCurrentRxRate;
        EModulationType         			siteMgrDesiredModulationType;
        TI_UINT16               			siteMgrDesiredBeaconInterval;
        EPreamble               			siteMgrDesiredPreambleType;
        EPreamble               			siteMgrCurrentPreambleType;
        ERadioBand              			siteMgrRadioBand;
        OS_802_11_BSSID_EX      			*pSiteMgrSelectedSiteInfo;
        OS_802_11_BSSID         			*pSiteMgrPrimarySiteDesc;
        EDot11Mode              			siteMgrDot11Mode;
        EDot11Mode              			siteMgrDot11OperationalMode;
        EDraftNumber           				siteMgrUseDraftNum;
        TI_UINT8                			siteMgrCurrentChannel;
        TSsid                   			siteMgrCurrentSSID;
		ScanBssType_e						siteMgrCurrentBSSType;
        EModulationType         			siteMgrCurrentModulationType;
        ESlotTime               			siteMgrSlotTime;
        signal_t                			siteMgrCurrentSignal;
        TI_UINT8                			siteMgrNumberOfSites;
        TIWLN_COUNTERS          			siteMgrTiWlanCounters;
        TI_BOOL                 			siteMgrBuiltInTestStatus;
        TI_UINT8                			siteMgrFwVersion[FW_VERSION_LEN]; /* Firmware version - null terminated string*/
        TI_UINT32               			siteMgrDisAssocReason;
        TI_UINT16               			siteMgrSiteCapability;
        TI_UINT16               			beaconInterval;
        TI_UINT8                			APTxPower;
        TI_BOOL                 			siteMgrQuietScanInProcess;
        TI_BOOL                 			siteMgrScanSliceCurrentlyActive;
        TI_UINT8                			siteMgrRoamingRssiGapThreshold;
        TI_UINT8                			timeStamp[8];
        TI_BOOL                 			siteMgrBeaconRecv;
        TI_UINT32               			siteMgrDtimPeriod;
        TI_INT32                			siteMgrCurrentRssi;
        TI_UINT8                			siteMgrIndexOfDesiredSiteEntry;
        TI_UINT8                			*pSiteMgrDesiredSiteEntry;
        TI_UINT8                			siteMgrCurrentTsfTimeStamp[8];
        TI_UINT8                			siteMgrUsrConfigTxPower;

        OS_802_11_CONFIGURATION 			*pSiteMgrConfiguration;
        siteMgr_prioritySite_t  			siteMgrPrioritySite;
		BufferParameters_t					siteMgrLastBeacon;
		TI_UINT8							siteMgrDesiredBeaconFilterState;
		TI_BOOL								siteMgrAllowTxPowerCheck;

        void     							*pPrimarySite;
        TI_BOOL                             bPrimarySiteHtSupport;

        /* WiFI SimpleConfig */
		TWscMode 							siteMgrWSCMode; /* used to set the WiFi Simple Config mode */

        /* SME SM section */
        TMacAddr                			smeDesiredBSSID;
        TSsid                   			smeDesiredSSID;
        ScanBssType_e           			smeDesiredBSSType;
        TI_BOOL                 			smeRadioOn;
        EConnectMode            			smeConnectionMode;
        TIWLN_DOT11_STATUS      			smeSmConnectionStatus;

        /* connection SM section */
        TI_UINT32               			connSelfTimeout;

        /* auth SM section */
        TI_UINT32               			authResponseTimeout;

        /* assoc SM section */
        TI_UINT32               			assocResponseTimeout;

        OS_802_11_ASSOCIATION_INFORMATION  	assocAssociationInformation;
		
        /* RSN section */
        TI_BOOL                 			rsnPrivacyOptionImplemented;
        EAuthSuite              			rsnDesiredAuthType;
        OS_802_11_KEY           			rsnOsKey;
        rsnAuthEncrCapability_t 			*pRsnAuthEncrCapability;
        TI_UINT32               			rsnNoOfPMKIDs;
        OS_802_11_PMKID         			rsnPMKIDList;
        TI_UINT32               			rsnWPAPromoteFlags;
        TI_UINT32               			rsnWPAMixedModeSupport;
        TI_UINT32               			rsnAuthState; /* supp_1XStates */
        ECipherSuite            			rsnEncryptionStatus;
        TI_UINT8                			rsnHwEncDecrEnable; /* 0- disable, 1- enable*/
        TSecurityKeys          				*pRsnKey;
        TI_UINT8                   			rsnDefaultKeyID;

        EExternalAuthMode      	 			rsnExtAuthneticationMode;
        TI_BOOL                    			rsnMixedMode;
		TI_BOOL								rsnPreAuthStatus;
		TMacAddr							rsnApMac;
        OS_802_11_EAP_TYPES     			eapType;
        TI_BOOL                    			wpa_802_1x_AkmExists;
        TI_BOOL                    			rsnPortStatus;
		rsnGenericIE_t                      rsnGenericIE;
		TI_BOOL                             rsnExternalMode;


        /* Rx Data section */
        rxDataCounters_t        			rxDataCounters;
        TI_BOOL                    			rxDataFilterEnableDisable;
        TRxDataFilterRequest    			rxDataFilterRequest;
		TI_UINT16                           rxGenericEthertype;

        /* Tx Data section */
        portStatus_e            			txDataPortStatus;
        TTxDataCounters        				*pTxDataCounters;
		TI_UINT32 							txPacketsCount;
		reportTsStatisticsReq_t 			tsMetricsCounters;
        OS_802_11_THRESHOLD_CROSS_PARAMS  	txDataMediumUsageThreshold;
        TI_UINT8                       		txDataEncryptionFieldSize;
		TI_UINT16                           txGenericEthertype;

        /* Ctrl Data section */
        TI_BOOL                    			ctrlDataPowerSaveEnable;
        TI_BOOL                    			ctrlDataPowerSaveForce;
        TI_BOOL                    			ctrlDatapowerSaveEnhanceAlgorithm;
        erpProtectionType_e     			ctrlDataIbssProtecionType;
        RtsCtsStatus_e          			ctrlDataRtsCtsStatus;
        TI_BOOL                    			ctrlDataProtectionEnabled;

        TMacAddr            				ctrlDataCurrentBSSID;
        ScanBssType_e                		ctrlDataCurrentBssType;
        TI_UINT32                  			ctrlDataCurrentRateMask;
        ERate                  				ctrlDataCurrentBasicRate;
        EPreamble               			ctrlDataCurrentPreambleType;
        ERate                  				ctrlDataCurrentActiveRate;
        TMacAddr            				ctrlDataDeviceMacAddress;
        TStreamTrafficProperties   			ctrlDataUpOfStream;
		TClsfrTableEntry					ctrlDataClsfrInsertTable;
        EClsfrType              			ctrlDataClsfrType;

 		TI_UINT32							ctrlDataTrafficIntensityEventsFlag;
		OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS ctrlDataTrafficIntensityThresholds;

        connectionType_e        			connType;

        /* MLME SM section */
        legacyAuthType_e        			mlmeLegacyAuthType;
        legacyAuthType_e        			authLegacyAuthType;
        TI_BOOL                    			mlmeReAssoc;


        TI_BOOL                    			rxDataExcludeUnencrypted;
        eapolDestination_e         			rxDataEapolDestination;
        portStatus_e               			rxDataPortStatus;

        TI_BOOL                    			txDataCurrentPrivacyInvokedMode;
        TI_BOOL                    			txDataEapolEncryptionStatus;
        TI_UINT32                  			txDataPollApPacketsFromACid;      /* AC to poll AP packets from */

        /* regulatory Domain section */
        regulatoryDomainParam_t 			regulatoryDomainParam;
        TI_UINT8                   			channel;
        TCountry*              				pCountry;
        TI_UINT8*               			pCountryString;
        TI_BOOL                    			spectrumManagementEnabled;
        TI_BOOL                    			regulatoryDomainEnabled;
        powerCapability_t       			powerCapability;
        TI_UINT8*                  			pSupportedChannel;
        TI_INT8                    			powerConstraint;
        TI_INT8                 			desiredTxPower; 		/* The desired Tx power inforced by the User (Utility),
																	or The desired Tx power (in Dbm) as forced by teh OS */
        TI_INT8                    			ExternTxPowerPreferred; /*for other extern elements that want
																	to effect the transmit power*/
		TpowerLevelTable_t					powerLevelTable;
		channelValidity_t					channelValidity;
		channelCapabilityRet_t				channelCapabilityRet;
		channelCapabilityReq_t				channelCapabilityReq;
		supportedChannels_t					supportedChannels;					
        TI_BOOL                    			enableDisable_802_11d;
        TI_BOOL                    			enableDisable_802_11h;
		TI_BOOL								bActivateTempPowerFix;
		TI_BOOL								bIsCountryFound;
		TI_BOOL								bIsChannelSupprted;
        TDfsChannelRange      				DFS_ChannelRange;
        TDfsChannel             			tDfsChannel;
		ERadioBand							eRadioBand;
        TI_UINT32               			uTimeToCountryExpiryMs;


        /* Measurement Manager section */
		TI_UINT32							measurementEnableDisableStatus;
        TI_UINT16							measurementTrafficThreshold;
		TI_UINT16							measurementMaxDuration;
        TInterrogateCmdCbParams 			interogateCmdCBParams;


        /* soft Gemini section */
        ESoftGeminiEnableModes				SoftGeminiEnable;
        TI_UINT32							SoftGeminiParamArray[NUM_OF_CONFIG_PARAMS_IN_SG];
        TI_UINT32							CoexActivityParamArray[NUM_OF_COEX_ACTIVITY_PARAMS_IN_SG];

		/* case XCC MODULE INCLUDED */
		XCC_PARAM_FIELDS

        /* Application Config Parameters Manager */
		TAssocReqBuffer						assocReqBuffer;
        TAssocReqBuffer						assocResBuffer;
		roamingMngrConfigParams_t			roamingConfigBuffer;
		TI_UINT32							roamingTriggerType;
		TI_UINT32							roamingConnStatus;
        bssList_t*              			pScanBssList;
		TScanPolicy*						pScanPolicy;

        /* Scan concnetrator application scan (periodic & one-shot) parameters */
        TScanParams                 		*pScanParams;
        TPeriodicScanParams         		*pPeriodicScanParams;
        TI_UINT32                   		uBssidListSize;
		TI_UINT32                   		uNumBssidInList;
        OS_802_11_BSSID_LIST_EX     		*pBssidList;
		OS_802_11_N_RATES					*pRateList;
        TSsid                   			tScanDesiredSSID;

        TI_UINT32                           uSraThreshold;
        TI_INT32                            nRssiThreshold;

        /* tx data qos related parameters */
        TTxDataQosParams           			txDataQosParams;

        /* QOS Manager */
        EQosProtocol                		qosSiteProtocol;
		TI_UINT8   							qosPacketBurstEnb;     /* Packet Burst Enable */
		EDot11Mode							qosMngrOperationalMode;
		TI_UINT8							desiredPsMode;
		TI_UINT8							currentPsMode;
        TSpecConfigure						TspecConfigure;
        TPsRxStreaming              		tPsRxStreaming;
		OS_802_11_QOS_RX_TIMEOUT_PARAMS		rxTimeOut;
        OS_802_11_QOS_PARAMS        		qosOsParams;
		OS_802_11_AC_QOS_PARAMS				qosApQosParams;
		
        /* AP Qos Capabilities */
        OS_802_11_AP_QOS_CAPABILITIES_PARAMS qosApCapabilities;

        /* Qos current AC status */
        OS_802_11_AC_UPSD_STATUS_PARAMS   	qosCurrentAcStatus;

        OS_802_11_QOS_DELETE_TSPEC_PARAMS   qosDelTspecRequest;
        OS_802_11_QOS_TSPEC_PARAMS     		qosAddTspecRequest;
		QoS_renegVoiceTspecReq_t	   		qosRenegotiateTspecRequest;

        OS_802_11_QOS_TSPEC_PARAMS     		qosTspecParameters;

		OS_802_11_QOS_DESIRED_PS_MODE		qosDesiredPsMode;

        /* Power Manager */
		PowerMgr_PowerMode_e    			PowerMode;
		EPowerPolicy 						PowerSavePowerLevel;
		EPowerPolicy 						DefaultPowerLevel;
		TPowerMgr_PowerMode   				powerMngPowerMode;
		PowerMgr_Priority_e 				powerMngPriority;
		PowerMgr_PowerMode_e				powerMngDozeMode;
        TI_BOOL                 			powerMgrKeepAliveEnaDis;
        TKeepAliveTemplate      			*pPowerMgrKeepAliveTemplate;
        TKeepAliveConfig        			*pPowerMgrKeepAliveConfig;
	 
		/* txRatePolicy params */
		TTxRatePolicy         				TxRatePolicy;
	
		TIWLN_RADIO_RX_QUALITY 				RxRadioQuality ;
		
		/* MIB*/
		TMib 								mib;

        /* Current BSS params - RSSI/SNR User Trigger */
		TUserDefinedQualityTrigger 			rssiSnrTrigger;

		/* debug */
		TDebugRegisterReq					HwRegister;
        RateMangeParams_t                   RateMng;
        RateMangeReadParams_t               RateMngParams;

        TIpAddr    StationIP;
        
    } content;
}paramInfo_t;




/*-----------------------------------------------------*/
/*      EEPROM-less support                            */
/*-----------------------------------------------------*/
#define MAX_CALL_DATA_REG_NUM                30
#define HW_EEPROM_PRESENTED                  1
#define HW_EEPROM_NOT_PRESENTED              0

typedef struct
{
    TI_UINT16  RegAddress;
    TI_UINT16  RegValue;

} PhyRegisters_t;


typedef enum
{
    PS_MODE_ELP         = 0,
    PS_MODE_POWER_DOWN  = 1,
    PS_MODE_ACTIVE      = 2,
    PS_MODE_WAKE_TNET   = 3
} powerSaveModes_e;


/**************************** Beginning of Init Params ************************************/


typedef struct
{
    TI_UINT8                   siteMgr_radioRxLevel[RX_LEVEL_TABLE_SIZE];
    TI_UINT8                   siteMgr_radioLNA[RX_LEVEL_TABLE_SIZE];
    TI_UINT8                   siteMgr_radioRSSI[RX_LEVEL_TABLE_SIZE];
    TI_UINT32                  factorRSSI; /* for RADIA only */
}radioValues_t;

typedef struct
{
    TI_UINT8               	siteMgrDesiredChannel;
    TMacAddr               	siteMgrDesiredBSSID;
    TSsid                  	siteMgrDesiredSSID;
    ScanBssType_e			siteMgrDesiredBSSType;
    EDot11Mode             	siteMgrDesiredDot11Mode;
    ERadioBand             	siteMgrSupportedBand;
    EDraftNumber			siteMgrUseDraftNum;
    TI_UINT32               siteMgrRegstryBasicRate[DOT11_MAX_MODE];
    TI_UINT32               siteMgrRegstrySuppRate[DOT11_MAX_MODE];
    TI_UINT32               siteMgrRegstryBasicRateMask;
    TI_UINT32               siteMgrRegstrySuppRateMask;
    rateMask_t              siteMgrCurrentDesiredRateMask;
    ratePair_t              siteMgrDesiredRatePair;
    TI_UINT32               siteMgrMatchedBasicRateMask;
    TI_UINT32               siteMgrMatchedSuppRateMask;
    EModulationType         siteMgrDesiredModulationType;
    EPreamble               siteMgrDesiredPreambleType;
    ESlotTime               siteMgrDesiredSlotTime;
    TI_UINT16               siteMgrDesiredBeaconInterval;
    TI_UINT32               siteMgrDesiredAtimWindow;
    TI_UINT32               siteMgrFreq2ChannelTable[SITE_MGR_CHANNEL_MAX+1];
    
    TI_UINT8                siteMgrExternalConfiguration;
    TI_UINT8                siteMgrPrivacyMode;
    TI_BOOL                 siteMgrWiFiAdhoc;

	/* TX Power Control parameters */
    TI_UINT32                  TxPowerCheckTime;
    TI_UINT32                  TxPowerControlOn;
    TI_INT32                   TxPowerRssiThresh;
    TI_INT32                   TxPowerRssiRestoreThresh;
    TI_UINT8                   TxPowerRecoverLevel;
    TI_UINT8                   TxPowerDesiredLevel;
	
	TBeaconFilterInitParams	beaconFilterParams; /*contains the desired state*/

	TI_UINT8					includeWSCinProbeReq;
} siteMgrInitParams_t;

typedef struct
{
    ERadioBand  eBand;
    TI_UINT8    uChannel;
} TSmeScanChannel;

typedef struct
{
    TI_BOOL         bRadioOn;
    TSsid           tDesiredSsid;
    TMacAddr        tDesiredBssid;
    ScanBssType_e   eDesiredBssType;
    EConnectMode    eConnectMode;
} TSmeModifiedInitParams;

typedef struct
{
    TI_UINT32       uMinScanDuration;
    TI_UINT32       uMaxScanDuration;
    TI_UINT32       uProbeReqNum;
    TI_INT8         iSnrThreshold;	
    TI_INT8         iRssiThreshold;
    TI_UINT32       uScanIntervals[ PERIODIC_SCAN_MAX_INTERVAL_NUM ];
    TI_UINT32       uCycleNum;
    TI_UINT32       uChannelNum;
    TSmeScanChannel tChannelList[ PERIODIC_SCAN_MAX_CHANNEL_NUM ];
} TSmeInitParams;


typedef struct
{
    TI_BOOL  RoamingScanning_2_4G_enable;
	TI_UINT8 RoamingOperationalMode;
    TI_UINT8 bSendTspecInReassPkt;
}   TRoamScanMngrInitParams;

typedef struct
{
    TI_UINT8                    parseWSCInBeacons;
} TMlmeInitParams;

typedef struct
{
    TI_UINT32                  connSelfTimeout;
} connInitParams_t;

typedef struct
{
    TI_UINT32                  authResponseTimeout;
    TI_UINT32                  authMaxRetryCount;
} authInitParams_t;

typedef struct
{
    TI_UINT32                  assocResponseTimeout;
    TI_UINT32                  assocMaxRetryCount;
} assocInitParams_t;

typedef struct
{
	TI_UINT8				highRateThreshold;
	TI_UINT8				lowRateThreshold;
	TI_BOOL				    enableEvent;
}tspecsRateParameters_t;

typedef struct
{
    TI_BOOL                    ctrlDataPowerSaveEnhanceAlgorithm;
    TI_UINT16                  ctrlDataPowerSaveTimeOut;
    TI_UINT8                   ctrlDataPowerSaveTxThreshold;
    TI_UINT8                   ctrlDataPowerSaveRxThreshold;

}powerSaveInitParams_t;

typedef struct
{
	TI_UINT8 longRetryLimit;
	TI_UINT8 shortRetryLimit;
}txRatePolicyParams;

typedef struct
{
    TI_BOOL                         ctrlDataPowerSaveEnable;
    TI_BOOL                         ctrlDataSoftGeminiEnable;
    TMacAddr                        ctrlDataDeviceMacAddress;
    powerSaveInitParams_t           powerSaveInitParams;
    erpProtectionType_e             ctrlDataDesiredIbssProtection;
/* 0 = CTS protaction disable ; 1 = Standard CTS protaction */
    RtsCtsStatus_e                  ctrlDataDesiredCtsRtsStatus;
    OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS   ctrlDataTrafficThreshold;
    TI_BOOL                         ctrlDataTrafficThresholdEnabled;
    txRatePolicyParams              ctrlDataTxRatePolicy;

	TI_UINT32		                policyEnabledRatesMaskCck;
	TI_UINT32		                policyEnabledRatesMaskOfdm;
	TI_UINT32		                policyEnabledRatesMaskOfdmA;
	TI_UINT32		                policyEnabledRatesMaskOfdmN;

} ctrlDataInitParams_t;

typedef struct
{
    /* TxCtrl Parameters */
	TI_UINT32					creditCalculationTimeout;
	TI_BOOL					    bCreditCalcTimerEnabled;
    /* TxDataQueue Parameters */
	TI_BOOL					    bStopNetStackTx;
	TI_UINT32					uTxSendPaceThresh;
	TClsfrParams				ClsfrInitParam;
} txDataInitParams_t;


typedef enum
{
    RADIO_B_G_INDEX = 0,
    RADIO_A_B_G_INDEX = 1,
    NUM_OF_RADIO_TYPES = 2
} regulatoryDomain_radioIndexType_e;

/* Regulatory Domain module init parameters */
typedef struct
{
    TI_UINT32                      uTimeOutToResetCountryMs;   /* Time after which country code will be reset */
    TI_UINT8                       multiRegulatoryDomainEnabled; /* 802.11d */
    TI_UINT8                       spectrumManagementEnabled; /* 802.11h */
    TI_UINT8                       desiredTxPower;
	TI_UINT8					uTemporaryTxPower;
    scanControlTable_t          desiredScanControlTable;/* for 5 and 2.4 Ghz*/
} regulatoryDomainInitParams_t;

#ifdef XCC_MODULE_INCLUDED
typedef enum
{
    XCC_MODE_DISABLED,
    XCC_MODE_ENABLED,
    XCC_MODE_STANDBY
} XCCMngr_mode_t;

typedef struct
{
    XCCMngr_mode_t  XCCEnabled;
} XCCMngrParams_t;
#endif

/* Measurement module init parameters */
typedef struct
{
    TI_UINT16              trafficIntensityThreshold;
    TI_UINT16              maxDurationOnNonServingChannel;
#ifdef XCC_MODULE_INCLUDED
    XCCMngr_mode_t      XCCEnabled;
#endif
} measurementInitParams_t;

/* Switch Channel Module module init parameters */
typedef struct
{
    TI_BOOL              dot11SpectrumManagementRequired;

} SwitchChannelInitParams_t;

typedef struct
{
  TI_UINT32       qosClassifierTable[MAX_NUM_OF_802_1d_TAGS];
}
clsfrParams_t;


typedef struct
{
    PowerMgr_PowerMode_e        powerMode;
    TI_UINT32                      beaconReceiveTime;
    TI_UINT8                       hangoverPeriod;
    TI_UINT8                       beaconListenInterval;
    TI_UINT8				 dtimListenInterval;
    TI_UINT8                       nConsecutiveBeaconsMissed;
    TI_UINT8                       EnterTo802_11PsRetries;
    TI_UINT8                       HwPsPollResponseTimeout;
    TI_UINT16                      		autoModeInterval;
    TI_UINT16                      		autoModeActiveTH;
    TI_UINT16                      		autoModeDozeTH;
    PowerMgr_PowerMode_e        autoModeDozeMode;

    	EPowerPolicy defaultPowerLevel;
	EPowerPolicy PowerSavePowerLevel;     	

	
	/* powerMgmtConfig IE */
    TI_UINT8						mode;
    TI_UINT8						needToSendNullData;  
    TI_UINT8						numNullPktRetries; 
    TI_UINT8						hangOverPeriod;
    TI_UINT16						NullPktRateModulation; 

	/* PMConfigStruct */
	TI_UINT32						ELPEnable;			/* based on "elpType" */
	TI_UINT32						WakeOnGPIOenable;	/* based on "hwPlatformType" */
	TI_UINT32						BaseBandWakeUpTime;	/* BBWakeUpTime */
	TI_UINT32						PLLlockTime;

	/* ACXWakeUpCondition */
    TI_UINT8						listenInterval;
    /* BET */
    TI_UINT32  						MaximalFullBeaconReceptionInterval; /* maximal time between full beacon reception */
    TI_UINT8   						BetEnableThreshold;
    TI_UINT8   						BetDisableThreshold;
    TI_UINT8   						BetEnable;             
    TI_UINT8   						MaximumConsecutiveET;
    TI_UINT32						PsPollDeliveryFailureRecoveryPeriod;

	TI_BOOL							reAuthActivePriority;
}PowerMgrInitParams_t;

typedef struct
{
	TI_UINT8  FullRecoveryEnable;
	TI_BOOL   recoveryTriggerEnabled[ MAX_FAILURE_EVENTS ];
} healthMonitorInitParams_t;

typedef struct
{
    TI_BOOL   ignoreDeauthReason0;
} apConnParams_t;

typedef struct
{
    TI_UINT32       uMinimumDurationBetweenOsScans;
    TI_UINT32       uDfsPassiveDwellTimeMs;
    TI_BOOL	        bPushMode; /*  True means Push mode. False is the default mode, storing scan results in table. */
    TI_UINT32       uSraThreshold;
    TI_INT32        nRssiThreshold;

} TScanCncnInitParams;

typedef struct
{
    TI_UINT8       uNullDataKeepAlivePeriod;
    TI_UINT8	   RoamingOperationalMode;
} TCurrBssInitParams;

typedef struct
{
	TI_BOOL                rxDataHostPacketProcessing;
    TI_BOOL                rxDataFiltersEnabled;
    filter_e            rxDataFiltersDefaultAction;
    TRxDataFilterRequest    rxDataFilterRequests[MAX_DATA_FILTERS];
	TI_UINT32				reAuthActiveTimeout;
}rxDataInitParams_t;

typedef struct
{
    TI_UINT32       uWlanDrvThreadPriority; /* Default setting of the WLAN driver task priority  */
    TI_UINT32       uBusDrvThreadPriority;  /* Default setting of the bus driver thread priority */
    TI_UINT32       uSdioBlkSizeShift;      /* In block-mode:  uBlkSize = (1 << uBlkSizeShift)   */
}TDrvMainParams;

/* This table is forwarded to the driver upon creation by the OS abstraction layer. */
typedef struct
{
	TTwdInitParams        		    twdInitParams;
    siteMgrInitParams_t             siteMgrInitParams;
    connInitParams_t                connInitParams;
    authInitParams_t                authInitParams;
    assocInitParams_t               assocInitParams;
    txDataInitParams_t              txDataInitParams;
    ctrlDataInitParams_t            ctrlDataInitParams;
    TRsnInitParams                  rsnInitParams;
    regulatoryDomainInitParams_t    regulatoryDomainInitParams;
    measurementInitParams_t         measurementInitParams;
    TSmeModifiedInitParams          tSmeModifiedInitParams;
    TSmeInitParams                  tSmeInitParams;
    SoftGeminiInitParams_t          SoftGeminiInitParams;
    QosMngrInitParams_t             qosMngrInitParams;
    clsfrParams_t                   clsfrParams;
#ifdef XCC_MODULE_INCLUDED
    XCCMngrParams_t                 XCCMngrParams;
#endif
	SwitchChannelInitParams_t		SwitchChannelInitParams;
	healthMonitorInitParams_t		healthMonitorInitParams;
    apConnParams_t                  apConnParams;
    PowerMgrInitParams_t            PowerMgrInitParams;
    TScanCncnInitParams             tScanCncnInitParams;
	rxDataInitParams_t              rxDataInitParams;
	TI_BOOL							SendINIBufferToUser;
    /* Traffic Monitor */
    TI_UINT8                        trafficMonitorMinIntervalPercentage;
    TReportInitParams               tReport;
    TCurrBssInitParams              tCurrBssInitParams;
    TContextInitParams              tContextInitParams;
    TMlmeInitParams                 tMlmeInitParams;
    TDrvMainParams                  tDrvMainParams;
    TRoamScanMngrInitParams         tRoamScanMngrInitParams;
} TInitTable;


#endif /* __PARAM_OUT_H__ */

