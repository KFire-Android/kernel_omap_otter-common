/*
 * TWDriver.h
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

 
/** \file  TWDriver.h 
 *  \brief TWDriver APIs
 *
 *  \see 
 */

/** @defgroup Control Control group
 * \brief The Control group includes the list of functions which perform TWD Control
 */
/** @defgroup Measurement Measurement group
 * \brief The Measurement group includes the list of functions which gets measurements from FW / TWD
 */
/** @defgroup Data_Path Data Path group
 * \brief The Data Path group includes the list of functions which perform the TWD Data Path
 */
/**	@defgroup Power_Management Power Management group 	
 * \brief The Power Management group includes the list of functions which set the power management mode
 */
/** @defgroup QoS Quality  Of Service group 
 * \brief The Quality of Service group includes the list of functions which perform the TWD QoS
 */
/** @defgroup Radio Radio (PHY) group
 * \brief The Radio group includes the list of functions which handle the Radio
 */
/** @defgroup BSS BSS group
 * \brief The BSS group includes the list of functions which handle the Basic Service Set
 */
/** @defgroup Misc Miscellaneous group
 * \brief The Miscellaneous group includes the list of functions which handle miscellaneous issues
 */
#ifdef TI_DBG
/**	@defgroup Test Debug Test group
 * \brief The Debug Test group includes the list of functions which Test the TWD and FW
 */
#endif

#ifndef TWDRIVER_H
#define TWDRIVER_H


#include "802_11Defs.h"
#include "TWDriverMsr.h"
#include "TWDriverScan.h"
#include "TWDriverRate.h"
#include "fwDebug_api.h"
#include "TwIf.h"
/* 
 * original firmware h-files 
 */
#include "public_commands.h"
#include "public_event_mbox.h"
#include "public_infoele.h"
#include "public_host_int.h"
#include "public_descriptors.h"   
#include "public_radio.h"

/*
 * Firmware types defintions
 */ 
#ifndef uint8
#define uint8   TI_UINT8
#endif
#ifndef uint16
#define uint16  TI_UINT16
#endif
#ifndef uint32
#define uint32  TI_UINT32
#endif
#ifndef int8
#define int8    TI_INT8
#endif
#ifndef int16
#define int16   TI_INT16
#endif
#ifndef int32
#define int32   TI_INT32
#endif



/*
 * --------------------------------------------------------------
 *	Definitions
 * --------------------------------------------------------------
 */

/* PALAU Group Address Default Values */
#define NUM_GROUP_ADDRESS_VALUE_DEF     4   
#define NUM_GROUP_ADDRESS_VALUE_MIN     0
#define NUM_GROUP_ADDRESS_VALUE_MAX     8

/* Early Wakeup Default Values */
#define EARLY_WAKEUP_ENABLE_MIN         (TI_FALSE)
#define EARLY_WAKEUP_ENABLE_MAX         (TI_TRUE)
#define EARLY_WAKEUP_ENABLE_DEF         (TI_TRUE)

/* ARP IP Filter Default Values */
#define MIN_FILTER_ENABLE_VALUE         0
#define MAX_FILTER_ENABLE_VALUE         3
#define DEF_FILTER_ENABLE_VALUE         0
#define FILTER_ENABLE_FLAG_LEN          1

/* Beacon filter Deafult Values */
#define DEF_BEACON_FILTER_ENABLE_VALUE  1
#define DEF_BEACON_FILTER_IE_TABLE_NUM  16
#define MIN_BEACON_FILTER_ENABLE_VALUE  0
#define MAX_BEACON_FILTER_ENABLE_VALUE  1
#define BEACON_FILTER_IE_TABLE_DEF_SIZE 37
#define BEACON_FILTER_IE_TABLE_MAX_SIZE 100
#define BEACON_FILTER_IE_TABLE_MIN_SIZE 0 
#define BEACON_FILTER_IE_TABLE_MAX_NUM  (6+32)
#define BEACON_FILTER_IE_TABLE_MIN_NUM  0 

/* CoexActivity Table Deafult Values */
#define COEX_ACTIVITY_TABLE_DEF_NUM     0
#define COEX_ACTIVITY_TABLE_MIN_NUM     0
#define COEX_ACTIVITY_TABLE_MAX_NUM     24*2
#define COEX_ACTIVITY_TABLE_SIZE        ((2+1)+(2+1)+(2+1)+(2+1)+(4+1)+(4+1)) /* includes spaces between bytes */

#define DEF_NUM_STORED_FILTERS          1
#define MIN_NUM_STORED_FILTERS          1
#define MAX_NUM_STORED_FILTERS          8

#define TWD_HW_ACCESS_METHOD_MIN   0
#define TWD_HW_ACCESS_METHOD_MAX   2
#define TWD_HW_ACCESS_METHOD_DEF   1

#define TWD_SITE_FRAG_COLLECT_MIN  2
#define TWD_SITE_FRAG_COLLECT_MAX  10
#define TWD_SITE_FRAG_COLLECT_DEF  3

#define TWD_TX_MIN_MEM_BLKS_NUM    40   /* The MINIMUM number of Tx memory blocks configured to FW */

#define TWD_RX_BLOCKS_RATIO_MIN    0
#define TWD_RX_BLOCKS_RATIO_MAX    100
#define TWD_RX_BLOCKS_RATIO_DEF    50

#define TWD_TX_FLASH_ENABLE_MIN         TI_FALSE
#define TWD_TX_FLASH_ENABLE_MAX         TI_TRUE
#define TWD_TX_FLASH_ENABLE_DEF         TI_TRUE

#define TWD_USE_INTR_TRHESHOLD_MIN 0
#define TWD_USE_INTR_TRHESHOLD_MAX 1
#define TWD_USE_INTR_TRHESHOLD_DEF 0

#define TWD_USE_TX_DATA_INTR_MIN   0
#define TWD_USE_TX_DATA_INTR_MAX   1

#define NUM_OF_CHANNELS_24              14
#define NUM_OF_CHANNELS_5               180

#define TWD_CALIBRATION_CHANNEL_2_4_MIN 1
#define TWD_CALIBRATION_CHANNEL_2_4_MAX NUM_OF_CHANNELS_24
#define TWD_CALIBRATION_CHANNEL_2_4_DEF 1

#define A_5G_BAND_MIN_CHANNEL       36
#define A_5G_BAND_MAX_CHANNEL       180
#define A_5G_BAND_NUM_CHANNELS  	(A_5G_BAND_MAX_CHANNEL-A_5G_BAND_MIN_CHANNEL+1)

#define TWD_CALIBRATION_CHANNEL_5_0_MIN 34
#define TWD_CALIBRATION_CHANNEL_5_0_MAX  A_5G_BAND_MAX_CHANNEL
#define TWD_CALIBRATION_CHANNEL_5_0_DEF 36

#define TWD_CALIBRATION_CHANNEL_4_9_MIN 8
#define TWD_CALIBRATION_CHANNEL_4_9_MAX 16
#define TWD_CALIBRATION_CHANNEL_4_9_DEF 12

#define TWD_RTS_THRESHOLD_MIN           0
#define TWD_RTS_THRESHOLD_MAX           4096
#define TWD_RTS_THRESHOLD_DEF           4096

#define TWD_BCN_RX_TIME_OUT_MIN         10      /* ms */
#define TWD_BCN_RX_TIME_OUT_MAX         1000    /* ms */
#define TWD_BCN_RX_TIME_OUT_DEF         10      /* ms */

#define TWD_RX_DISABLE_BROADCAST_MIN    TI_FALSE
#define TWD_RX_DISABLE_BROADCAST_MAX    TI_TRUE
#define TWD_RX_DISABLE_BROADCAST_DEF    TI_FALSE

/* Indicate if the recovery process is active or not */
#define TWD_RECOVERY_ENABLE_MIN         TI_FALSE
#define TWD_RECOVERY_ENABLE_MAX         TI_TRUE
#define TWD_RECOVERY_ENABLE_DEF         TI_TRUE

/* Indicate if working with Burst Mode or not */
#define BURST_MODE_ENABLE_MIN         TI_FALSE
#define BURST_MODE_ENABLE_MAX         TI_TRUE
#define BURST_MODE_ENABLE_DEF         TI_FALSE

#define SMART_REFLEX_STATE_MIN        TI_FALSE
#define SMART_REFLEX_STATE_MAX        TI_TRUE
#define SMART_REFLEX_STATE_DEF        TI_TRUE

#define SMART_REFLEX_CONFIG_PARAMS_DEF_TABLE  "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"
#define SMART_REFLEX_CONFIG_PARAMS_DEF_TABLE_SRF1  "07,03,18,10,05,fb,f0,e8, 0,0,0,0,0,0,0f,3f"
#define SMART_REFLEX_CONFIG_PARAMS_DEF_TABLE_SRF2  "07,03,18,10,05,f6,f0,e8"
#define SMART_REFLEX_CONFIG_PARAMS_DEF_TABLE_SRF3  "07,03,18,10,05,fb,f0,e8"

#define TWD_FRAG_THRESHOLD_MIN          256
#define TWD_FRAG_THRESHOLD_MAX          4096
#define TWD_FRAG_THRESHOLD_DEF          4096

#define TWD_MAX_TX_MSDU_LIFETIME_MIN    0
#define TWD_MAX_TX_MSDU_LIFETIME_MAX    3000
#define TWD_MAX_TX_MSDU_LIFETIME_DEF    512

#define TWD_MAX_RX_MSDU_LIFETIME_MIN    0
#define TWD_MAX_RX_MSDU_LIFETIME_MAX    0xFFFFFFFF
#define TWD_MAX_RX_MSDU_LIFETIME_DEF    512000


#define TWD_LISTEN_INTERVAL_MIN         1
#define TWD_LISTEN_INTERVAL_MAX         10
#define TWD_LISTEN_INTERVAL_DEF         3

/* This field indicates the number of transmit retries to attempt at
    the rate specified in the TNETW Tx descriptor before
    falling back to the next lowest rate.
    If this field is set to 0xff, then rate fallback is disabled.
    If this field is 0, then there will be 0 retries before starting fallback.*/
#define TWD_RATE_FB_RETRY_LIMIT_MIN     0   /* => No retries before starting RateFallBack */
#define TWD_RATE_FB_RETRY_LIMIT_MAX     255 /* =>0xff for disabling Rate fallback */
#define TWD_RATE_FB_RETRY_LIMIT_DEF     0

#define TWD_TX_ANTENNA_MIN              TX_ANTENNA_2
#define TWD_TX_ANTENNA_MAX              TX_ANTENNA_1
#define TWD_TX_ANTENNA_DEF              TX_ANTENNA_1

#define TWD_RX_ANTENNA_MIN              RX_ANTENNA_1
#define TWD_RX_ANTENNA_MAX              RX_ANTENNA_PARTIAL
#define TWD_RX_ANTENNA_DEF              RX_ANTENNA_FULL

/*
 * Tx and Rx interrupts pacing (threshold in packets, timeouts in milliseconds)
 */
#define TWD_TX_CMPLT_THRESHOLD_DEF      4   /* 0 means no pacing so send interrupt on every event */
#define TWD_TX_CMPLT_THRESHOLD_MIN      0
#define TWD_TX_CMPLT_THRESHOLD_MAX      30

#define TWD_TX_CMPLT_TIMEOUT_DEF        700 /* The Tx Complete interrupt pacing timeout in microseconds! */
#define TWD_TX_CMPLT_TIMEOUT_MIN        1
#define TWD_TX_CMPLT_TIMEOUT_MAX        50000

#define TWD_RX_INTR_THRESHOLD_DEF       0   /* 0 means no pacing so send interrupt on every event */
#define TWD_RX_INTR_THRESHOLD_MIN       0
#define TWD_RX_INTR_THRESHOLD_MAX       30
#define TWD_RX_INTR_THRESHOLD_DEF_WIFI_MODE  0 /* No Rx interrupt pacing so send interrupt on every event */

#define TWD_RX_INTR_TIMEOUT_DEF         600  /* The Rx interrupt pacing timeout in microseconds! */  
#define TWD_RX_INTR_TIMEOUT_MIN         1 
#define TWD_RX_INTR_TIMEOUT_MAX         50000

/* Rx aggregation packets number limit (max packets in one aggregation) */
#define TWD_RX_AGGREG_PKTS_LIMIT_DEF    4
#define TWD_RX_AGGREG_PKTS_LIMIT_MIN    0 
#define TWD_RX_AGGREG_PKTS_LIMIT_MAX    4

/* Tx aggregation packets number limit (max packets in one aggregation) */
#define TWD_TX_AGGREG_PKTS_LIMIT_DEF    0
#define TWD_TX_AGGREG_PKTS_LIMIT_MIN    0 
#define TWD_TX_AGGREG_PKTS_LIMIT_MAX    32

/*
 * Tx power level 
 */
#define DBM_TO_TX_POWER_FACTOR			10

/* TX_POWER is in Dbm/10 units */
#define MAX_TX_POWER					250 
#define MIN_TX_POWER					0   
#define DEF_TX_POWER					205   


#define MIN_DEFAULT_KEY_ID              0
#define MAX_DEFAULT_KEY_ID              3

#define KEY_RSC_LEN                     8
#define MIN_KEY_LEN                     5
#define MAX_KEY_LEN                     32

#define TWD_RSSI_BEACON_WEIGHT_MIN       0   
#define TWD_RSSI_BEACON_WEIGHT_MAX     100
#define TWD_RSSI_BEACON_WEIGHT_DEF      20

#define TWD_RSSI_PACKET_WEIGHT_MIN       0   
#define TWD_RSSI_PACKET_WEIGHT_MAX     100
#define TWD_RSSI_PACKET_WEIGHT_DEF      10

#define TWD_SNR_BEACON_WEIGHT_MIN        0   
#define TWD_SNR_BEACON_WEIGHT_MAX      100
#define TWD_SNR_BEACON_WEIGHT_DEF       20

#define TWD_SNR_PACKET_WEIGHT_MIN        0   
#define TWD_SNR_PACKET_WEIGHT_MAX      100
#define TWD_SNR_PACKET_WEIGHT_DEF       10

#define TWD_DCO_ITRIM_ENABLE_MIN  TI_FALSE
#define TWD_DCO_ITRIM_ENABLE_MAX  TI_TRUE
#define TWD_DCO_ITRIM_ENABLE_DEF  TI_FALSE

#define TWD_DCO_ITRIM_MODERATION_TIMEOUT_MIN    10000
#define TWD_DCO_ITRIM_MODERATION_TIMEOUT_MAX  1000000
#define TWD_DCO_ITRIM_MODERATION_TIMEOUT_DEF    50000


#define MAX_NUM_OF_AC                   4

/************************************/      
/*      Rates values                */  
/************************************/
/* The next definitions are used to decide which encryption is used by the Rx flags */
#define RX_FLAGS_NO_SECURITY                0  
#define RX_FLAGS_WEP                        1
#define RX_FLAGS_TKIP                       2
#define RX_FLAGS_AES                        3


#define RX_DESC_FLAGS_ENCRYPTION            8
#define RX_PACKET_FLAGS_ENCRYPTION_SHIFT    16
#define RX_PACKET_FLAGS_ENCRYPTION_SHIFT_FROM_DESC      (RX_PACKET_FLAGS_ENCRYPTION_SHIFT - RX_DESC_FLAGS_ENCRYPTION)

/* Tx packet Control-Block flags bit-mask. */
#define TX_CTRL_FLAG_XFER_DONE_ISSUED      0x0001  /* Xfer-Done already issued to upper driver   - for WHA. */
#define TX_CTRL_FLAG_TX_COMPLETE_ISSUED    0x0002  /* Tx-Complete already issued to upper driver - for WHA. */
#define TX_CTRL_FLAG_LINK_TEST             0x0004  /* XCC link test packet */
#define TX_CTRL_FLAG_SENT_TO_FW            0x0008  /* Set after the packet is allowed to be sent to FW (by TxHwQueue) */
#define TX_CTRL_FLAG_PKT_IN_RAW_BUF        0x0010  /* The input packet is in a raw buffer (as opposed to OS packet) */
#define TX_CTRL_FLAG_MULTICAST             0x0020  /* A multicast ethernet packet */
#define TX_CTRL_FLAG_BROADCAST             0x0040  /* A broadcast ethernet packet */

#define TX_PKT_TYPE_MGMT                   1   /* Management Packet						  */
#define TX_PKT_TYPE_EAPOL                  2   /* EAPOL packet (Ethernet)				  */
#define TX_PKT_TYPE_ETHER                  3   /* Data packet from the Network interface  */
#define TX_PKT_TYPE_WLAN_DATA	           4   /* Driver generated WLAN Data Packet (currently used for IAPP packet) */


#define ALIGN_4BYTE_MASK                   0x3 /* Masked LS bits for 4-bytes aligned addresses or lengths. */
#define SHIFT_BETWEEN_TU_AND_USEC          10  /* Shift factor to convert between TU (1024 uSec) and uSec. */

/* Packet header + extensions structure ranges between 24 and 48 bytes as follows:
 * ------------------------------------------------------------------------------
 * Alignment Padding:   0/2 bytes,      added for 4 bytes alignment of this structure.
 * Mac-Header:          24 bytes,       802.11 basic header.
 * Qos header:          0/2 bytes,      for QoS-data or QoS-Null the two QoS bytes are added.
 * Security Pad:        0/0/4/8/18 bytes,  for None/WEP/TKIP/AES/GEM.
 * LLC/SNAP:            0/8 bytes,      added only for data packets.
 * HT control:          0/4             added only for packte support QoS and HT
 */
#define MAX_HEADER_SIZE                 48  

/* Data body max length */
#define MAX_DATA_BODY_LENGTH            4096



/* The weight in % of the new packet relative to the previous average value of RSSI */
#define RSSI_DEFAULT_WEIGHT             20 

#define RSSI_DEFAULT_THRESHOLD          -80 
#define SNR_DEFAULT_THRESHOLD           0 

/* 
 * 'No beacon' roaming trigger configuration
 * Number of consecutive beacons (or DTIM periods) missed before 
 * 'Out of Sync' event is raised 
 */
#define OUT_OF_SYNC_DEFAULT_THRESHOLD   10
/* 
 * IBSS - Number of consecutive beacons (or DTIM periods) missed before 
 * 'Out of Sync' event is raised 
 */
#define OUT_OF_SYNC_IBSS_THRESHOLD      200
/* Period of time between 'Out of sync' and 'No beacon' events */
#define NO_BEACON_DEFAULT_TIMEOUT       100 /* in tu-s*/

/* Consecutive NACK roaming trigger configuration */
#define NO_ACK_DEFAULT_THRESHOLD        20 

/* Low Rx rate roaming trigger configuration */
#define LOW_RATE_DEFAULT_THRESHOLD      2 

#define MEASUREMENT_NOISE_HISTOGRAM_NUM_OF_RANGES       8
#define MAX_CHANNELS_IN_REG_DOMAIN      40

#define CTS_TO_SELF_DISABLE             0
#define CTS_TO_SELF_ENABLE              1

#define MAX_TEMPLATE_SIZE               256

/* Scan constants */
#define MAX_NUMBER_OF_CHANNELS_PER_SCAN                     16
#define SCAN_MAX_NUM_OF_NORMAL_CHANNELS_PER_COMMAND         MAX_NUMBER_OF_CHANNELS_PER_SCAN
#define SCAN_MAX_NUM_OF_SPS_CHANNELS_PER_COMMAND            16
#define SCAN_DEFAULT_MIN_CHANNEL_DWELL_TIME                 30000
#define SCAN_DEFAULT_MAX_CHANNEL_DWELL_TIME                 60000
#define SCAN_DEFAULT_EARLY_TERMINATION_EVENT                SCAN_ET_COND_DISABLE
#define SCAN_DEFAULT_EARLY_TERMINATION_NUM_OF_FRAMES        0

#define NUM_OF_NOISE_HISTOGRAM_COUNTERS 8

#define TX_DESCRIPTOR_SIZE             sizeof(TxIfDescriptor_t)

#define CTRL_BLK_ENTRIES_NUM            160

#define HT_CAP_AMPDU_PARAMETERS_FIELD_OFFSET   2
#define HT_CAP_HT_EXTENDED_FIELD_OFFSET        19
#define HT_CAP_AMPDU_MAX_RX_FACTOR_BITMASK     0x3
#define HT_CAP_AMPDU_MIN_START_SPACING_BITMASK 0x7
#define HT_CAP_GREENFIELD_FRAME_FORMAT_BITMASK 0x0010
#define HT_CAP_SHORT_GI_FOR_20MHZ_BITMASK      0x0020
#define HT_CAP_LSIG_TXOP_PROTECTION_BITMASK    0x8000
#define HT_EXT_HT_CONTROL_FIELDS_BITMASK       0x0400
#define HT_EXT_RD_INITIATION_BITMASK           0x0800
#define HT_INF_RIFS_MOD_BITMASK                0x08
#define HT_INF_OPERATION_MOD_BITMASK           0x03
#define HT_INF_NON_GF_PRES_BITMASK             0x04
#define HT_INF_TX_BURST_LIMIT_BITMASK          0x08
#define HT_INF_DUAL_BEACON_BITMASK             0x40
#define HT_INF_DUAL_CTS_PROTECTION_BITMASK     0x80

/* 
 * TWD HT capabilities, physical capabilities of the STA.
 * The structure is defined like that in order to simplify the interface with WHA layer.
 */
#define RX_TX_MCS_BITMASK_SIZE      10

#define  DSSS_CCK_MODE         					1

#define MCS_HIGHEST_SUPPORTED_RECEPTION_DATA_RATE_IN_MBIT_S 0x48

#define IMPLICIT_TXBF_REC_CAPABLE             	1
#define TRANSMIT_STAGGERED_SOUNDING_CAPABLE   	1

/* Firmware version name length */
#define FW_VERSION_LEN                  		20

/*the max table sized is : ( number of 221 * 8 bytes ) + ( non-221 * 2 bytes )
  Must be synchronized with the size of ACX defined in public_infoele.h interface
  with the FW
*/
#define MIB_MAX_SIZE_OF_IE_TABLE 				112 
#define MIB_TEMPLATE_DATA_MAX_LEN   			256
#define MIB_MAX_MULTICAST_GROUP_ADDRS			8

#define MAX_MULTICAST_GROUP_ADDRS				8

/* Max numver of policies */
#define MAX_NUM_OF_TX_RATE_CLASS_POLICIES   	8 

#define NUM_POWER_LEVELS                		4
#define MAX_POWER_LEVEL                 		1
#define MIN_POWER_LEVEL                 		NUM_POWER_LEVELS

/*
 * --------------------------------------------------------------
 *	Enumerations
 * --------------------------------------------------------------
 */
/** \enum EFileType
 * \brief TWD input/output files
 *
 * \par Description
 * Indicates which File (or part of file) to read or write
 *
 * \sa TFileInfo
 */
typedef enum
{
    FILE_TYPE_INI = 0, 	/**< */
    FILE_TYPE_NVS, 		/**< */
    FILE_TYPE_FW, 		/**< */
    FILE_TYPE_FW_NEXT 	/**< */
} EFileType;

/** \enum EKeepAliveTriggerType
 * \brief Keep Alive Trigger Types
 * 
 * \par Description
 * Indicates when to trigger Keep Alive
 * 
 * \sa TKeepAliveParams
 * 
 */
typedef enum
{
    KEEP_ALIVE_TRIG_TYPE_NO_TX = 0,		/**< */
    KEEP_ALIVE_TRIG_TYPE_PERIOD_ONLY	/**< */

} EKeepAliveTriggerType;

/** \enum ESlotTime
 * \brief Radio (PHY) Slot Time Type
 * 
 * \par Description
 * Used for configuring PHY Slot Time for FW
 * 
 * \sa TWD_CfgPreamble
 */
typedef enum
{
	PHY_SLOT_TIME_LONG		= 0,	/**< 	Long PHY Slot Time  */ 
    PHY_SLOT_TIME_SHORT     = 1		/**< 	Short PHY Slot Time  */ 

} ESlotTime;

/** \enum EMib
 * \brief MIB Element Type
 * 
 * \par Description
 * Used for R/W MIB to FW
 * 
 * \sa TMib
 */
typedef enum
{
/*	0x00	*/	MIB_dot11MaxReceiveLifetime = 0,	/**< */
/*	0x01	*/  MIB_dot11SlotTime,					/**< */
/*	0x02	*/  MIB_dot11GroupAddressesTable,		/**< */
/*	0x03	*/  MIB_dot11WepDefaultKeyId,			/**< */
/*	0x04	*/  MIB_dot11CurrentTxPowerLevel,		/**< */
/*	0x05	*/  MIB_dot11RTSThreshold,				/**< */
/*	0x06	*/  MIB_ctsToSelf,						/**< */
/*	0x07	*/  MIB_arpIpAddressesTable,			/**< */
/*	0x08	*/  MIB_templateFrame,					/**< */
/*	0x09	*/  MIB_rxFilter,						/**< */
/*	0x0A	*/  MIB_beaconFilterIETable,			/**< */
/*	0x0B	*/  MIB_beaconFilterEnable,				/**< */
/*	0x0C	*/  MIB_sleepMode,						/**< */
/*	0x0D	*/  MIB_wlanWakeUpInterval,				/**< */
/*	0x0E	*/  MIB_beaconLostCount,				/**< */
/*	0x0F	*/  MIB_rcpiThreshold,					/**< */
/*	0x10	*/  MIB_statisticsTable,				/**< */
/*	0x11	*/  MIB_ibssPsConfig,					/**< */
/*	0x12	*/  MIB_txRatePolicy,					/**< */
/*	0x13	*/  MIB_countersTable,					/**< */
/*	0x14	*/  MIB_btCoexsitenceMode,				/**< */
/*	0x15	*/  MIB_btCoexistenceParameters,		/**< */

				/* must be last!!! */
				MIB_lastElem	= 0xFFFF			/**< */

} EMib;

/** \enum ETwdParam
 * \brief TWD Control parameter ID
 * 
 * \par Description
 * FW Parmaeter Information Identifier
 * 
 * \sa TWD_SetParam, TWD_GetParam
 */
typedef enum
{
/*	0x01	*/	TWD_RTS_THRESHOLD_PARAM_ID          = 0x01,		/**< */
/*	0x02	*/  TWD_FRAG_THRESHOLD_PARAM_ID,					/**< */
/*	0x03	*/  TWD_COUNTERS_PARAM_ID,							/**< */
/*	0x04	*/  TWD_LISTEN_INTERVAL_PARAM_ID,					/**< */
/*	0x05	*/  TWD_BEACON_INTERVAL_PARAM_ID,					/**< */
/*	0x06	*/  TWD_TX_POWER_PARAM_ID,    						/**< */
/*	0x07	*/  TWD_CLK_RUN_ENABLE_PARAM_ID,					/**< */
/*	0x08	*/  TWD_QUEUES_PARAM_ID, 							/**< */
/*	0x09	*/  TWD_TX_RATE_CLASS_PARAM_ID,						/**< */
/*	0x0A	*/  TWD_MAX_TX_MSDU_LIFE_TIME_PARAM_ID,				/**< */
/*	0x0B	*/  TWD_MAX_RX_MSDU_LIFE_TIME_PARAM_ID,				/**< */
/*	0x0C	*/  TWD_CTS_TO_SELF_PARAM_ID,						/**< */
/*	0x0D	*/  TWD_RX_TIME_OUT_PARAM_ID,						/**< */
/*	0x0E	*/  TWD_BCN_BRC_OPTIONS_PARAM_ID,					/**< */
/*	0x0F	*/	TWD_AID_PARAM_ID,								/**< */
/*	0x10	*/  TWD_RSN_HW_ENC_DEC_ENABLE_PARAM_ID,  			/**< */
/*	0x11	*/  TWD_RSN_KEY_ADD_PARAM_ID,						/**< */
/*	0x12	*/  TWD_RSN_KEY_REMOVE_PARAM_ID,					/**< */
/*	0x13	*/  TWD_RSN_DEFAULT_KEY_ID_PARAM_ID,				/**< */
/*	0x14	*/  TWD_RSN_SECURITY_MODE_PARAM_ID,					/**< */
/*	0x15	*/  TWD_RSN_SECURITY_ALARM_CB_SET_PARAM_ID,			/**< */
/*	0x16	*/  TWD_ACX_STATISTICS_PARAM_ID,					/**< */
/*	0x17	*/  TWD_MEDIUM_OCCUPANCY_PARAM_ID,					/**< */
/*	0x18	*/  TWD_DISABLE_POWER_MANAGEMENT_AUTO_CONFIG_PARAM_ID,	/**< */
/*	0x19	*/  TWD_ENABLE_POWER_MANAGEMENT_AUTO_CONFIG_PARAM_ID,	/**< */
/*	0x1A	*/  TWD_SG_ENABLE_PARAM_ID,							/**< */
/*	0x1B	*/  TWD_SG_CONFIG_PARAM_ID,							/**< */
#ifdef XCC_MODULE_INCLUDED
/*	0x1C	*/  TWD_RSN_XCC_SW_ENC_ENABLE_PARAM_ID,				/**< */
/*	0x1D	*/  TWD_RSN_XCC_MIC_FIELD_ENABLE_PARAM_ID,			/**< */
#endif /* XCC_MODULE_INCLUDED*/
/*	0x1E	*/  TWD_TX_OP_LIMIT_PARAM_ID,						/**< */
/*	0x1F	*/  TWD_NOISE_HISTOGRAM_PARAM_ID,					/**< */
/*	0x20	*/  TWD_TSF_DTIM_MIB_PARAM_ID,						/**< */
/*	0x21	*/  TWD_REVISION_PARAM_ID,							/**< */
/*	0x22	*/  TWD_CURRENT_CHANNEL_PARAM_ID,					/**< */
/*	0x23	*/	TWD_RADIO_TEST_PARAM_ID,						/**< */
/*	0x24	*/	TWD_RSSI_LEVEL_PARAM_ID,						/**< */
/*	0x25	*/	TWD_SNR_RATIO_PARAM_ID,							/**< */
/*	0x26	*/	TWD_COEX_ACTIVITY_PARAM_ID,	    				/**< */
/*	0x27	*/	TWD_FM_COEX_PARAM_ID,	    				    /**< */
/*	0x28	*/	TWD_DCO_ITRIM_PARAMS_ID,    				    /**< */

				/* must be last!!! */
/*	0x29    */	TWD_LAST_PARAM_ID								/**< */
} ETwdParam;

/** \enum ETwdCallbackOwner
 * \brief TWD Callback Module owner ID
 * 
 * \par Description
 * The Owner ID defines a specific TWD Module
 * 
 * \sa ETwdEventId, TWD_RegisterCb
 */
typedef enum
{
    TWD_OWNER_DRIVER_TX_XFER            = 0x0100,	/**< 	TX Xfer Owner ID  		*/    
    TWD_OWNER_RX_XFER                   = 0x0200,	/**< 	RX Xfer Owner ID  		*/    
    TWD_OWNER_SELF                      = 0x0300,	/**< 	Self Owner ID  			*/    
    TWD_OWNER_MAC_SERVICES              = 0x0400,	/**< 	MAC Services Owner ID  	*/    
    TWD_OWNER_TX_RESULT                 = 0x0500,	/**< 	TX Result Owner ID  	*/    
    TWD_OWNER_SELF_CONFIG               = 0x0600,	/**< 	Self configuration of Owner ID  	*/    
    TWD_OWNER_RX_QUEUE                  = 0x0700,	/**< 	RX Queue Owner ID  		*/    
    TWD_OWNER_TX_HW_QUEUE               = 0x0800	/**< 	TX HW Queue Owner ID  	*/    

} ETwdCallbackOwner;

/** \enum ETwdIntCallbackId
 * \brief TWD Internal Callbacks ID
 * 
 * \par Description
 * The Owner ID defines a specific TWD Internal CB
 * 
 * \sa ETwdEventId
 */
typedef enum
{
    TWD_INT_SEND_PACKET_TRANSFER        =  0x00 ,	/**< 	Tx Data Path Send Callback  	*/    
    TWD_INT_SEND_PACKET_COMPLETE                , 	/**< 	Tx Data Path Complete Callback 	*/   
    TWD_INT_UPDATE_BUSY_MAP                     , 	/**< 	Tx Data Path Update-Busy-Map Callback 	*/   

    /* Rx Data Path Callbacks */
    TWD_INT_RECEIVE_PACKET              =  0x10 ,	/**< 	Rx Data Path Receive Packet Callback 	   	*/    
    TWD_INT_REQUEST_FOR_BUFFER                  , 	/**< 	Rx Data Path Request for buffer Callback  	*/   
    
    /* TWD Callbacks */
    TWD_INT_COMMAND_COMPLETE            =  0x20 , 	/**< 	TWD internal Command Complete Callback  	*/   
    TWD_INT_EVENT_FAILURE  							/**< 	TWD internal Event Failure handle Callback 	*/        

} ETwdIntCallbackId;

/** \enum ETwdOwnEventId
 * \brief Event Mail Box ID
 * 
 * \par Description
 * Clients That expects an event should register for it, 
 * and Mask/UnMask Events with this ID
 * 
 * \sa
 */
/* Note: changes here should be reflected also in eventTable in eventMbox.c !!! */
typedef enum 
{
			/*Regular events*/
/*	0	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_0 = 0,       /**< */
/*	1	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_1,           /**< */
/*	2	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_2,           /**< */
/*	3	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_3,           /**< */
/*	4	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_4,          	/**< */
/*	5	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_5,           /**< */
/*	6	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_6,           /**< */
/*	7	*/  TWD_OWN_EVENT_RSSI_SNR_TRIGGER_7,           /**< */
/*	8	*/	TWD_OWN_EVENT_MEASUREMENT_START,            /**< */
/*	9	*/  TWD_OWN_EVENT_MEASUREMENT_COMPLETE,         /**< */
/*	10	*/	TWD_OWN_EVENT_SCAN_CMPLT,                   /**< */
/*	11	*/  TWD_OWN_EVENT_SPS_SCAN_CMPLT,               /**< */
/*	12	*/  TWD_OWN_EVENT_AP_DISCOVERY_COMPLETE,        /**< */
/*	13	*/  TWD_OWN_EVENT_PS_REPORT,                    /**< */
/*	14	*/	TWD_OWN_EVENT_PSPOLL_DELIVERY_FAILURE, 		/**< */
/*	15	*/  TWD_OWN_EVENT_DISCONNECT_COMPLETE,          /**< */
/*	16	*/  TWD_OWN_EVENT_JOIN_CMPLT,                   /**< */
/*	17	*/  TWD_OWN_EVENT_SWITCH_CHANNEL_CMPLT,         /**< */
/*	18	*/  TWD_OWN_EVENT_BSS_LOSE,                     /**< */
/*	19	*/  TWD_OWN_EVENT_BSS_REGAIN,                   /**< */
/*	20	*/  TWD_OWN_EVENT_MAX_TX_RETRY,                 /**< */
/*  21  */  RESERVED21,									/**< */
/*	22	*/  TWD_OWN_EVENT_SOFT_GEMINI_SENSE,            /**< */
/*	23	*/  TWD_OWN_EVENT_SOFT_GEMINI_PREDIC,           /**< */
/*	24	*/  TWD_OWN_EVENT_SOFT_GEMINI_AVALANCHE,        /**< */
/*	25	*/  TWD_OWN_EVENT_PLT_RX_CALIBRATION_COMPLETE,  /**< */
/*  26  */  TWD_DBG_EVENT,								/**< */
/*  27  */  TWD_HEALTH_CHECK_REPLY_EVENT,				/**< */
/*	28	*/  TWD_OWN_EVENT_PERIODIC_SCAN_COMPLETE,       /**< */
/*	29	*/  TWD_OWN_EVENT_PERIODIC_SCAN_REPORT,         /**< */
/*  30  */  TWD_BA_SESSION_TEAR_DOWN_EVENT,				/**< */
/*	31	*/  TWD_OWN_EVENT_ALL,                          /**< */
/*	32	*/  TWD_OWN_EVENT_MAX                          	/**< */ 

} ETwdOwnEventId;   

/** \enum ETwdEventId
 * \brief TNETW Driver Event ID
 * 
 * \par Description
 * The TWD Event ID is used by user for registering a TWD Internal CB 
 * which will handle a TWD Event.
 * Each field in this enum is an ID of TWD Event, and is combined of two IDs: 
 * TWD CB Owner (Module) ID and TWD Internal CB ID. Therefore, the CB is registered accordeing to
 * Module (Owner) and Internal CB Id.
 * 
 * \sa TWD_RegisterCb, ETwdCallbackOwner, ETwdIntCallbackId
 */
typedef enum
{
    /* Internal Failure Event Callbacks */
    TWD_EVENT_FAILURE                   	=  TWD_OWNER_SELF | TWD_INT_EVENT_FAILURE, 					/**< 	Failure Internal Event ID 			*/
    TWD_EVENT_COMMAND_COMPLETE          	=  TWD_OWNER_SELF | TWD_INT_COMMAND_COMPLETE,  				/**< 	Command Complete Internal Event ID */
    
    /* Tx Data Path Callbacks */
    TWD_EVENT_TX_XFER_SEND_PKT_TRANSFER 	=  TWD_OWNER_DRIVER_TX_XFER | TWD_INT_SEND_PACKET_TRANSFER,	/**< 	TX Data Path Send Packet Event ID 			*/
    TWD_EVENT_TX_RESULT_SEND_PKT_COMPLETE	=  TWD_OWNER_TX_RESULT | TWD_INT_SEND_PACKET_COMPLETE,      /**< 	TX Data Path Send Packet Complete Event ID 	*/
    TWD_EVENT_TX_HW_QUEUE_UPDATE_BUSY_MAP   =  TWD_OWNER_TX_HW_QUEUE | TWD_INT_UPDATE_BUSY_MAP,         /**< 	TX Data Path Update-Busy-Map Event ID 	*/

    /* Rx Data Path Callbacks */
    TWD_EVENT_RX_REQUEST_FOR_BUFFER     	=  TWD_OWNER_RX_XFER | TWD_INT_REQUEST_FOR_BUFFER,         	/**< 	RX Data Path Request for Buffer Internal Event ID 	*/
    TWD_EVENT_RX_RECEIVE_PACKET         	=  TWD_OWNER_RX_QUEUE | TWD_INT_RECEIVE_PACKET             	/**< 	RX Data Path Receive Packet Internal Event ID  	*/

} ETwdEventId;

#ifdef TI_DBG
/** \enum ETwdPrintInfoType
 * \brief TWD print functions codes
 * 
 * \par Description
 * Used for Debug - determines which Tx Info to print
 * 
 * \sa TWD_PrintTxInfo
 */
typedef enum
{
/*	0	*/	TWD_PRINT_TX_CTRL_BLK_TBL = 0,	/**< 	Print TX Control Block Information	*/
/*	1	*/  TWD_PRINT_TX_HW_QUEUE_INFO,		/**< 	Print TX HW Queue Information 		*/
/*	2	*/  TWD_PRINT_TX_XFER_INFO,			/**< 	Print TX XFER Information 			*/
/*	3	*/  TWD_PRINT_TX_RESULT_INFO,		/**< 	Print TX Result Information 		*/
/*	4	*/  TWD_CLEAR_TX_RESULT_INFO,		/**< 	Clear TX Result Information			*/
/*	5	*/  TWD_CLEAR_TX_XFER_INFO          /**< 	Clear TX Xfer Information           */

} ETwdPrintInfoType;
#endif

/** \enum EIpVer
 * \brief IP Version
 * 
 * \par Description
 * 
 * \sa TWD_PrintTxInfo
 */
typedef enum
{
/*	0	*/	IP_VER_4 = 0, 	/**< */	
/*	1	*/  IP_VER_6	 	/**< */

} EIpVer;

/** \enum EKeyType
 * \brief Key Type
 * 
 * \par Description
 * Security Key Type
 * 
 * \sa TSecurityKeys
 */
typedef enum
{
/*	0	*/  KEY_NULL = 0,	/**< */
/*	1	*/  KEY_WEP,		/**< */
/*	2	*/  KEY_TKIP,		/**< */
/*	3	*/  KEY_AES,		/**< */
/*	4	*/  KEY_XCC,    	/**< */
#ifdef GEM_SUPPORTED
    /*  5   */  KEY_GEM
#endif

} EKeyType;

/** \enum ERegistryTxRate
 * \brief TX Rate Type
 * 
 * \par Description
 * 
 * \sa
 */
/* Make it same as "rate_e" */
typedef enum
{
/* This value is reserved if this enum is used for MgmtCtrlTxRate - the auto mode is only valid for data packets */  
/*	0	*/	REG_RATE_AUTO_BIT = 0, 		/**< */
/*	1	*/	REG_RATE_1M_BIT,			/**< */
/*	2	*/	REG_RATE_2M_BIT,			/**< */
/*	3	*/	REG_RATE_5_5M_CCK_BIT,		/**< */
/*	4	*/	REG_RATE_11M_CCK_BIT,		/**< */
/*	5	*/	REG_RATE_22M_PBCC_BIT,		/**< */
/*	6	*/	REG_RATE_6M_OFDM_BIT,		/**< */
/*	7	*/	REG_RATE_9M_OFDM_BIT,		/**< */
/*	8	*/	REG_RATE_12M_OFDM_BIT,		/**< */
/*	9	*/	REG_RATE_18M_OFDM_BIT,		/**< */
/*	10	*/	REG_RATE_24M_OFDM_BIT,		/**< */
/*	11	*/	REG_RATE_36M_OFDM_BIT,		/**< */
/*	12	*/	REG_RATE_48M_OFDM_BIT,		/**< */
/*	13	*/	REG_RATE_54M_OFDM_BIT,		/**< */
/*	14	*/	REG_RATE_MCS0_OFDM_BIT,		/**< */
/*	15	*/	REG_RATE_MCS1_OFDM_BIT,		/**< */
/*	16	*/	REG_RATE_MCS2_OFDM_BIT,		/**< */
/*	17	*/	REG_RATE_MCS3_OFDM_BIT,		/**< */
/*	18	*/	REG_RATE_MCS4_OFDM_BIT,		/**< */
/*	19	*/	REG_RATE_MCS5_OFDM_BIT,		/**< */
/*	20	*/	REG_RATE_MCS6_OFDM_BIT,		/**< */
/*	21	*/	REG_RATE_MCS7_OFDM_BIT		/**< */

} ERegistryTxRate;

/** \enum EFailureEvent
 * \brief Failure Event
 * 
 * \par Description
 * Used as a parameter for Failure Event CB - 
 * Inicates Failure Event ID, according which the Failure 
 * Event's data is driven
 * 
 * \sa TWD_RegisterOwnCb, TFailureEventCb
 */
typedef enum
{
/*	-1	*/	NO_FAILURE = -1,				/**< 	No Failure Event					*/
/*	0	*/	NO_SCAN_COMPLETE_FAILURE = 0,	/**< 	No Scan Complete Failure Event		*/
/*	1	*/	MBOX_FAILURE,					/**< 	Mail Box Failure Event				*/
/*	2	*/	HW_AWAKE_FAILURE,				/**< 	HW Awake Failure Event				*/
/*	3	*/	TX_STUCK,						/**< 	TX STUCK Failure Event				*/
/*	4	*/	DISCONNECT_TIMEOUT,				/**< 	Disconnect Timeout Failure Event	*/
/*	5	*/	POWER_SAVE_FAILURE,				/**< 	Power Save Failure Event			*/
/*	6	*/	MEASUREMENT_FAILURE,			/**< 	Measurement Failure Event			*/
/*	7	*/	BUS_FAILURE,					/**< 	Bus Failure Event					*/
/*	8	*/	HW_WD_EXPIRE,					/**< 	HW Watchdog Expire Event			*/
/*	9	*/	RX_XFER_FAILURE,			    /**< 	Rx pkt xfer failure                 */

/* must be last!!! */
/* 10	*/	MAX_FAILURE_EVENTS				/**< 	Maximum number of Failure Events	*/

} EFailureEvent;

/** \enum ETemplateType
 * \brief Template Type
 * 
 * \par Description
 * Used for setting/Getting a Template to/from FW
 * 
 * \sa TWD_CmdTemplate, TWD_WriteMibTemplateFrame, TSetTemplate TWD_GetTemplate
 */
typedef enum
{
/*	0	*/	NULL_DATA_TEMPLATE = 0,		/**< NULL Data Template						*/
/*	1	*/	BEACON_TEMPLATE,        	/**< Beacon Template						*/
/*	2	*/	PROBE_REQUEST_TEMPLATE,     /**< PROBE Request Template					*/
/*	3	*/	PROBE_RESPONSE_TEMPLATE,	/**< PROBE Response Template				*/
/*	4	*/	QOS_NULL_DATA_TEMPLATE,		/**< Quality Of Service NULL Data Template	*/
/*	5	*/	PS_POLL_TEMPLATE,			/**< Power Save Poll Template				*/
/*	6	*/	KEEP_ALIVE_TEMPLATE,		/**< Keep Alive Template 					*/
/*	7	*/	DISCONN_TEMPLATE,			/**< Disconn (Deauth/Disassoc) Template		*/
/*	8	*/	ARP_RSP_TEMPLATE			/**< ARP Ressponse Template		            */
} ETemplateType;




typedef enum
{
    KEY_WEP_DEFAULT       = 0,
    KEY_WEP_ADDR          = 1,
    KEY_AES_GROUP         = 4,
    KEY_AES_PAIRWISE      = 5,
    KEY_WEP_GROUP         = 6,
    KEY_TKIP_MIC_GROUP    = 10,
    KEY_TKIP_MIC_PAIRWISE = 11
} KeyType_enum;


/** \enum ECipherSuite
 * \brief CHIPHER Suite
 * 
 * \par Description
 * Available cipher suites for admission control
 * 
 * \sa 
 */
typedef enum
{
/*	0	*/	TWD_CIPHER_NONE = 0,			/**< no cipher suite 		*/
/*	1	*/	TWD_CIPHER_WEP,        			/**< WEP-40 cipher suite 	*/
/*	2	*/	TWD_CIPHER_TKIP,        		/**< TKIP cipher suite      */
/*	3	*/	TWD_CIPHER_AES_WRAP,    		/**< AES WRAP cipher suite  */
/*	4	*/	TWD_CIPHER_AES_CCMP,    		/**< AES CCMP cipher suite  */
/*	5	*/	TWD_CIPHER_WEP104,      		/**< WEP-104 cipher suite 	*/
/*	6	*/	TWD_CIPHER_CKIP,        		/**< CKIP cipher suite      */
#ifdef GEM_SUPPORTED
    /*	7	*/	TWD_CIPHER_GEM,         		/**< GEM cipher suite       */
#endif
            TWD_CIPHER_MAX,         		

			TWD_CIPHER_UNKNOWN	= 255       /**< UNKNOWN chpiher suite 	*/

} ECipherSuite;

/** \enum E80211PsMode
 * \brief 802.11 Power Save Mode
 * 
 * \par Description
 * 
 * \sa TWD_Scan, TWD_SetPsMode
 */
typedef enum 
{
/*	0	*/	POWER_SAVE_OFF = 0,		/**< 	power save 802.11 OFF   		*/
/*	1	*/	POWER_SAVE_ON,			/**< 	power save 802.11 ON  			*/    
/*	2	*/	POWER_SAVE_KEEP_CURRENT	/**< 	power save 802.11 don't change 	*/

} E80211PsMode;

/** \enum E80211PsStatus
 * \brief Set Power Save mode status
 * 
 * \par Description
 * 
 * \sa 
 */
typedef enum
{
/*	1	*/	POWER_SAVE_802_11_SUCCESS = 1,	/**< 	power save mode Success   	*/
/*	2	*/	POWER_SAVE_802_11_FAIL,			/**< 	power save mode Fail    	*/
/*	3	*/	POWER_SAVE_802_11_NOT_ALLOWED,	/**< 	power save mode Not Allowed	*/
/*	4	*/	POWER_SAVE_802_11_PENDING,		/**< 	power save mode Pending    	*/
/*	5	*/	POWER_SAVE_802_11_IS_CURRENT	/**< 	power save mode Is Current 	*/

} E80211PsStatus;

/** \enum EElpCtrlMode
 * \brief ELP Control Mode
 * 
 * \par Description
 * 
 * \sa 
 */
typedef enum 
{
/*	0	*/	ELPCTRL_MODE_NORMAL = 0,	/**< ALP Control mode Normal   		*/
/*	1	*/	ELPCTRL_MODE_KEEP_AWAKE		/**< ALP Control mode Keep Awake   	*/

} EElpCtrlMode;

/** \enum EPreamble
 * \brief Preamble Type
 * 
 * \par Description
 * 
 * \sa TWD_CfgPreamble
 */
typedef enum
{
    PREAMBLE_LONG       	= 0,	/**< Preamble type Long   			*/
    PREAMBLE_SHORT          = 1,	/**< Preamble type Short   			*/

    PREAMBLE_UNSPECIFIED    = 0xFF	/**< Preamble type Not Specified   	*/

} EPreamble;

/** \enum ENoiseHistogramCmd
 * \brief Noise Histogram Type
 * 
 * \par Description
 * 
 * \sa TNoiseHistogram, TWD_CmdNoiseHistogram
 */
typedef enum
{
     STOP_NOISE_HIST                    = 0,	/**< Stop Noise Histogram	*/
     START_NOISE_HIST                   = 1		/**< Start Noise Histogram	*/

} ENoiseHistogramCmd;

/** \enum ETnetWakeOn
 * \brief ACX Wake Up Condition
 * 
 * \par Description
 * 
 * \sa TPowerMgmtConfig, TWD_CfgWakeUpCondition
 */
typedef enum 
{
    
/*	0	*/	TNET_WAKE_ON_BEACON = 0,       	/**< Indicate the wake on event of the HW - beacon.
											* In this event the HW configure to be awake on every beacon.
											*/

/*	1	*/	TNET_WAKE_ON_DTIM,             /**< Indicate the wake on event of the HW - DTIM. In this event
											* the HW configure to be awake on every DITM (configure by the AP).
											*/

/*	2	*/	TNET_WAKE_ON_N_BEACON,          /**< Indicate the wake on event of the HW - listen interval.
											* In this event the HW configure to be awake on every
											* configured number of beacons.
											*/

/*	3	*/	TNET_WAKE_ON_N_DTIM,            /**< Indicate the wake on event of the HW - listen interval.
											* In this event the HW configure to be awake on every
											* configured number of beacons.
											*/

/*	4	*/	TNET_WAKE_ON_HOST              /**< Indicate the wake on event of the HW - Host access only
											*/
                                
} ETnetWakeOn;

/** \enum ETxAntenna
 * \brief TX Antenna Types
 * 
 * \par Description
 * 
 * \sa TGeneralInitParams, TTwdParamContents
 */
typedef enum
{
    TX_ANTENNA_2	= 0, 	/**< */   
    TX_ANTENNA_1    = 1		/**< */

} ETxAntenna;

/** \enum ERxAntenna
 * \brief RX Antenna Types
 * 
 * \par Description
 * 
 * \sa TGeneralInitParams, TTwdParamContents
 */
typedef enum
{
/*	0	*/	RX_ANTENNA_1 = 0,	/**< */
/*	1	*/	RX_ANTENNA_2,		/**< */
/*	2	*/	RX_ANTENNA_FULL,	/**< */
/*	3	*/	RX_ANTENNA_PARTIAL	/**< */

} ERxAntenna;

/** \enum EPowerPolicy
 * \brief Save Power Level Policy
 * 
 * \par Description
 * 
 * \sa TWD_CfgSleepAuth
 */
typedef enum 
{
/*	0	*/	POWERAUTHO_POLICY_ELP = 0,	/**< */
/*	1	*/	POWERAUTHO_POLICY_PD,		/**< */
/*	2	*/	POWERAUTHO_POLICY_AWAKE,	/**< */
/*	3	*/	POWERAUTHO_POLICY_NUM		/**< */

} EPowerPolicy;

/** \enum ESoftGeminiEnableModes
 * \brief Soft-Gemini Enable Modes
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	SG_DISABLE = 0,			/**< */
/*	1	*/	SG_PROTECTIVE,			    /**< */
/*	2	*/	SG_OPPORTUNISTIC,	/**< */


} ESoftGeminiEnableModes;
/** \enum ESoftGeminiEnableProfile
 * \brief Soft-Gemini Profile Modes for S60 configuration
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{
    BtCoexProfData = 0,
    BtCoexProfDataLowLatency,
    BtCoexProfA2DP
}ESoftGeminiEnableProfile;


/** \enum EMibTemplateType
 * \brief MIB Template type
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{                                       	
/*	0	*/ 	TEMPLATE_TYPE_BEACON = 0,           /**< 	BEACON template 			*/
/*	1	*/  TEMPLATE_TYPE_PROBE_REQUEST,        /**< 	PROB template 				*/
/*	2	*/  TEMPLATE_TYPE_NULL_FRAME,           /**< 	NULL FRAM template 			*/
/*	3	*/  TEMPLATE_TYPE_PROBE_RESPONSE,       /**< 	PROB Response template 		*/
/*	4	*/  TEMPLATE_TYPE_QOS_NULL_FRAME,       /**< 	QOS Null Frame template 	*/
/*	5	*/  TEMPLATE_TYPE_PS_POLL               /**< 	Power Save Poll template	*/

} EMibTemplateType;


/** \enum ERxFailure
 * \brief RX Failure/Error
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	RX_FAILURE_NONE = 0,		/**< No Failure		*/
/*	1	*/	RX_FAILURE_DECRYPT,         /**< DeCrypt Failure	*/
/*	2	*/	RX_FAILURE_MIC_ERROR,		/**< MIC Error		*/
} ERxFailure;

/** \enum ETwdChannelWidth
 * \brief TWD Channel Width
 * 
 * \par Description
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{   
/*	0	*/	CHANNEL_WIDTH_20MHZ = 0,		/**< 20MHZ Channel Width	*/
/*	1	*/  CHANNEL_WIDTH_40MHZ_20MHZ		/**< 40-20MHZ Channel Width	*/
} ETwdChannelWidth;

/** \enum ETwdRxSTBC
 * \brief RX STBC Spatial Stream Supported
 * 
 * \par Description
 * Indicates how many RX STBC Spatial Stream are Supported
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{   
/*	0	*/	RXSTBC_NOT_SUPPORTED  =   0,							/**< No Spatial Stream Supported					*/
/*	1	*/  RXSTBC_SUPPORTED_ONE_SPATIAL_STREAM, 					/**< One Spatial Stream Supported					*/
/*	2	*/  RXSTBC_SUPPORTED_ONE_AND_TWO_SPATIAL_STREAMS,			/**< One and Two Spatial Stream Supported			*/
/*	3	*/  RXSTBC_SUPPORTED_ONE_TWO_AND_THREE_SPATIAL_STREAMS	/**< One, Two and Three Spatial Stream Supported	*/

} ETwdRxSTBC;

/** \enum ETwdMaxAMSDU
 * \brief Maximum MSDU Octets
 * 
 * \par Description
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{   
/*	0	*/	MAX_MSDU_3839_OCTETS = 0,	/**< Maximum MSDU Octets Number: 3839	*/
/*	1	*/  MAX_MSDU_7935_OCTETS		/**< Maximum MSDU Octets Number: 7935	*/

} ETwdMaxAMSDU;

/** \enum ETwdMaxAMPDU
 * \brief Maximum MPDU Octets
 * 
 * \par Description
 * Indicates What is the Maximum MPDU Octets Number
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */

/*
 ==============

 IMPORTANT NOTE - Changes to this enumeration must check weather MIN and MAX values 
                  should be updated
 ==============
*/
typedef enum
{   
            MAX_MPDU_MIN_VALUE = 0,

/*	0	*/	MAX_MPDU_8191_OCTETS = MAX_MPDU_MIN_VALUE,	/**< Maximum MPDU Octets Number: 8191	*/
/*	1	*/  MAX_MPDU_16383_OCTETS,		                /**< Maximum MPDU Octets Number: 16383	*/
/*	2	*/  MAX_MPDU_32767_OCTETS,		                /**< Maximum MPDU Octets Number: 32767	*/
/*	3	*/  MAX_MPDU_65535_OCTETS,		                /**< Maximum MPDU Octets Number: 65535	*/

            MAX_MPDU_MAX_VALUE = MAX_MPDU_65535_OCTETS

} ETwdMaxAMPDU;


/** \enum ETwdAMPDUSpacing
 * \brief TWD AMPDU Spacing
 * 
 * \par Description
 * Indicates What is the Time Spacing of AMPDU
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{   
/*	0	*/	AMPDU_SPC_NO_RESTRCITION =  0,	/**< No Restriction on AMPDU Time Spacing	*/ 	    
/*	1	*/	AMPDU_SPC_1_4_MICROSECONDS, 	/**< 1/4 Microsecond AMPDU Time Spacing   	*/
/*	2	*/	AMPDU_SPC_1_2_MICROSECONDS, 	/**< 1/2 Microsecond AMPDU Time Spacing   	*/
/*	3	*/	AMPDU_SPC_1_MICROSECOND,  		/**< 1 Microsecond AMPDU Time Spacing   	*/
/*	4	*/	AMPDU_SPC_2_MICROSECONDS,		/**< 2 Microsecond AMPDU Time Spacing   	*/
/*	5	*/	AMPDU_SPC_4_MICROSECONDS,		/**< 4 Microsecond AMPDU Time Spacing   	*/
/*	6	*/	AMPDU_SPC_8_MICROSECONDS,		/**< 8 Microsecond AMPDU Time Spacing   	*/
/*	7	*/	AMPDU_SPC_16_MICROSECONDS 		/**< 16 Microsecond AMPDU Time Spacing   	*/

} ETwdAMPDUSpacing;

/** \enum ETwdMcsSupport
 * \brief TWD MCS Support
 * 
 * \par Description
 * BIT Mapp which Indicates What is the Tx/rx MCS Support Enabled
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{   
/*	1	*/	MCS_SUPPORT_MCS_0   =  BIT_0,	/**< BIT 0	*/
/*	2	*/	MCS_SUPPORT_MCS_1   =  BIT_1,	/**< BIT 1	*/
/*	3	*/	MCS_SUPPORT_MCS_2   =  BIT_2,	/**< BIT 2	*/
/*	4	*/	MCS_SUPPORT_MCS_3   =  BIT_3,	/**< BIT 3	*/
/*	5	*/	MCS_SUPPORT_MCS_4   =  BIT_4,	/**< BIT 4	*/
/*	6	*/	MCS_SUPPORT_MCS_5   =  BIT_5,	/**< BIT 5	*/
/*	7	*/	MCS_SUPPORT_MCS_6   =  BIT_6,	/**< BIT 6	*/
/*	8	*/	MCS_SUPPORT_MCS_7   =  BIT_7	/**< BIT 7	*/

} ETwdMcsSupport;

/** \enum ETwdPCOTransTime
 * \brief TWD PCO Transition Time
 * 
 * \par Description
 * Indicates What is the PCO Transition Time
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{   
/*	0	*/	PCO_TRANS_TIME_NO_TRANSITION = 0, 	/**< No PCO Transition Time					*/    
/*	1	*/	PCO_TRANS_TIME_400_MICROSECONDS, 	/**< PCO Transition Time: 400 Microsecond	*/
/*	2	*/	PCO_TRANS_TIME_1_5_MILLISECONDS, 	/**< PCO Transition Time: 1.5 Millisecond	*/
/*	3	*/	PCO_TRANS_TIME_5_MILLISECONDS		/**< PCO Transition Time: 5 Millisecond		*/

} ETwdPCOTransTime;

/** \enum ETwdHTCapabilitiesBitMask
 * \brief TWD HT Capabilities Bit Mask Mapping
 * 
 * \par Description
 * Mapps the Bit Mask which are used for Making (Enabling/Disabling) 
 * HT Capabilities 
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{
/*	1	*/	CAP_BIT_MASK_GREENFIELD_FRAME_FORMAT           =  BIT_0,	/**< BIT 0	*/
/*	2	*/	CAP_BIT_MASK_SHORT_GI_FOR_20MHZ_PACKETS        =  BIT_1,	/**< BIT 1	*/
/*	3	*/	CAP_BIT_MASK_SHORT_GI_FOR_40MHZ_PACKETS        =  BIT_2,	/**< BIT 2	*/
/*	4	*/	CAP_BIT_MASK_SUPPORT_FOR_STBC_IN_TRANSMISSION  =  BIT_3,	/**< BIT 3	*/
/*	5	*/	CAP_BIT_MASK_DELAYED_BLOCK_ACK                 =  BIT_4,	/**< BIT 4	*/
/*	6	*/	CAP_BIT_MASK_DSSS_CCK_IN_40_MHZ                =  BIT_5,	/**< BIT 5	*/
/*	7	*/	CAP_BIT_MASK_LSIG_TXOP_PROTECTION              =  BIT_6,	/**< BIT 6	*/
/*	8	*/	CAP_BIT_MASK_PCO                               =  BIT_7,	/**< BIT 7	*/
/*	9	*/	CAP_BIT_MASK_LDPC_CODING                       =  BIT_8		/**< BIT 8	*/

} ETwdHTCapabilitiesBitMask;

/** \enum ETwdMCSFeedback
 * \brief TWD MCS FeedBack
 * 
 * \par Description
 * Indicates what is the MCS FeedBack Policy 
 * Used for Configure HT Capabilities Settings
 * 
 * \sa TWD_SetDefaults, TTwdHtCapabilities
 */
typedef enum
{   
/*	0	*/	MCS_FEEDBACK_NO = 0,						/**< */
/*	1	*/	MCS_FEEDBACK_RESERVED,						/**< */
/*	2	*/	MCS_FEEDBACK_UNSOLICTED_ONLY,				/**< */
/*	3	*/	MCS_FEEDBACK_BOTH_SOLICTED_AND_UNSOLICTED	/**< */
} ETwdMCSFeedback;

/** \enum ETwdTxMcsSet
 * \brief TWD TX MCS Set
 * 
 * \par Description
 * Indicates Whether to set Tx MCS
 * 
 * \sa
 */
typedef enum
{   
    TX_MCS_SET_NO   =   0, 	/**< Don't Set Tx MCS	*/ 
    TX_MCS_SET_YES  =   1	/**< Set Tx MCS			*/
} ETwdTxMcsSet;

/** \enum ETwdTxRxNotEqual
 * \brief TWD TX RX Not Equal
 * 
 * \par Description
 * Indicates Whether the TX and RX channels are equal
 * 
 * \sa
 */
typedef enum
{   
    TX_RX_NOT_EQUAL_NO   =   0,	/**< TX and RX Channels are not equal	*/ 
    TX_RX_NOT_EQUAL_YES  =   1	/**< TX and RX Channels are equal		*/ 
} ETwdTxRxNotEqual;

/** \enum ETwdHtcSupport
 * \brief TWD HTc Support
 * 
 * \par Description
 * Indicates Whether the HT Capability is Supported
 * 
 * \sa
 */
typedef enum
{   
    HTC_SUPPORT_NO   =   0,	/**< HT Capability is not Supported		*/ 
    HTC_SUPPORT_YES  =   1	/**< HT Capability is Supported			*/ 
} ETwdHtcSupport;

/** \enum ESendCompleteStatus
 * \brief Send complete status
 * 
 * \par Description
 * Indicates the current Success/Failure Status of Completion of Send Operation
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	SEND_COMPLETE_SUCCESS = 0,			/**< Send Complete Success: Completion of Send Operation is OK					
												*/
/*	1	*/	SEND_COMPLETE_RETRY_EXCEEDED,		/**< Send Complete Retry Exceeded: 
												* Completion of Send Operation filed because it Exceeded Allowed retries Number	
												*/
/*	2	*/	SEND_COMPLETE_LIFETIME_EXCEEDED,	/**< Send Complete Lifetiem Exceeded: 
												* Completion of Send Operation failed because it Exceeded Allowed Lifetime	
												*/
/*	3	*/	SEND_COMPLETE_NO_LINK,				/**< Send Complete No Link: 
												* Completion of Send Operation failed because No Link was found  					
												*/
/*	4	*/	SEND_COMPLETE_MAC_CRASHED			/**< Send Complete MAC Crashed: 
												* Completion of Send Operation failed because MAC Crashed							
												*/
} ESendCompleteStatus;

/** \enum EPacketType
 * \brief Packet type
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum 
{ 
/*	0	*/	PACKET_DATA = 0, 	/**< */
/*	1	*/	PACKET_CTRL, 		/**< */
/*	2	*/	PACKET_MGMT 		/**< */

} EPacketType;

/** \enum ETxHwQueStatus
 * \brief Status returned by txHwQueue_AllocResources 
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{   
    TX_HW_QUE_STATUS_SUCCESS,       /* Resources available on current queue */
    TX_HW_QUE_STATUS_STOP_CURRENT,  /* No resources, stop current queue and requeue the packet */
    TX_HW_QUE_STATUS_STOP_NEXT      /* Resources available for this packet but not for another one, 
                                          so just stop the current queue */
} ETxHwQueStatus;

/** \enum ERxBufferStatus
 * \brief Status returned by TRequestForBufferCb 
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{   
    RX_BUF_ALLOC_PENDING,
    RX_BUF_ALLOC_COMPLETE,
    RX_BUF_ALLOC_OUT_OF_MEM

}ERxBufferStatus;


typedef enum
{
    ArpFilterDisabled,
    ArpFilterEnabled,
    ArpFilterEnabledAutoMode = 3
} EArpFilterType;

/*
 * --------------------------------------------------------------
 *	Structures
 * --------------------------------------------------------------
 */
/**
 * \brief Get File Callback
 * 
 * \param  hCbHndl	- Handle to CB Object
 * \return void 
 * 
 * \par Description
 * The callback function type for GetFile users
 * 
 * \sa 	TFileInfo
 */ 
typedef void (*TGetFileCbFunc)(TI_HANDLE hCbHndl);

/** \struct TFileInfo
 * \brief File Information
 * 
 * \par Description
 * Contains all needed information and structures for Getting file
 * 
 * \sa	TWD_InitFw
 */ 
typedef struct
{
    EFileType   	eFileType;  		/**< Requested file type */
    TI_UINT8   		*pBuffer;    		/**< Pointer to Buffer into the file (or file portion) is copied from user space */
    TI_UINT32   	uLength;    		/**< Length of data currently held in pBuffer */
    TI_UINT32   	uOffset;    		/**< Offset in File of data currently held in pBuffer */
    TI_UINT32   	uAddress;    		/**< Offset in File of data currently held in pBuffer */
    TI_BOOL     	bLast;      		/**< TRUE indicates that we reached end of file */
    void       		*hOsFileDesc;		/**< OAL file-descriptor handle for repeated access to same file (FW) */
    TGetFileCbFunc  fCbFunc;			/**< CB function to call if file read is finished in a later context (future option) */
    TI_HANDLE       hCbHndl;			/**< Handle to provide when calling fCbFunc */
    TI_UINT32		uChunksLeft;		/**< Chunks Left to read from File (used if file is read in chunks) */
    TI_UINT32		uChunkBytesLeft;	/**< Number of bytes of Last read chunk, which were not yet handled  */
    TI_UINT32		uCrcCalc;			/**< Current Calculated CRC  */
} TFileInfo;

/** \struct T80211Header
 * \brief 802.11 MAC header
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT16                           fc;			/**< */
    TI_UINT16                           dur;		/**< */
    TMacAddr                            address1;	/**< */
    TMacAddr                            address2;	/**< */
    TMacAddr                            address3;	/**< */
    TI_UINT16                           seq;		/**< */
    TI_UINT16                           qos;		/**< */

}  T80211Header;

/** \struct TKeepAliveParams
 * \brief Keep Alive Parameters
 * 
 * \par Description
 * 
 * \sa	TWD_CfgKeepAlive
 */ 
typedef struct
{
    TI_UINT8                index;		/**< */
    TI_UINT8                enaDisFlag;	/**< */
    TI_UINT32               interval;	/**< */
    EKeepAliveTriggerType   trigType;	/**< */

} TKeepAliveParams;

/** \struct TPsRxStreaming
 * \brief Power Save RX Streaming
 * 
 * \par Description
 * The configuration of Rx streaming delivery in PS mode per TID
 * 
 * \sa	TWD_CfgKeepAlive
 */ 
typedef struct
{
    TI_UINT32               uTid;           /**< The configured TID (0-7) */
    TI_UINT32               uStreamPeriod;  /**< The expected period between two packets of the delivered stream */
    TI_UINT32               uTxTimeout;     /**< Start sending triggers if no Tx traffic triggers arrive for this priod */
    TI_BOOL                 bEnabled;       /**< If TRUE enable this TID streaming, if FALSE disable it. */

} TPsRxStreaming;

/** \struct TDmaParams
 * \brief DMA Parameters
 * 
 * \par Description
 * Struct which holds DMA Rx/Tx Queues and Bufffers params
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT32                           NumRxBlocks;				/**< Allocated RX memory blocks number 	    */
    TI_UINT32                           NumTxBlocks;				/**< Allocated TX memory blocks number      */
    TI_UINT8                            NumStations;				/**< Number of Stations						*/
    void                                *fwTxResultInterface;		/**< RX minimum Memory block number 		*/
    TI_UINT8                            *fwRxCBufPtr;				/**< Pointer to FW RX Control Buffer		*/
    TI_UINT8                            *fwTxCBufPtr;				/**< Pointer to FW TX Control Buffer		*/
	void                                *fwRxControlPtr;			/**< Pointer to FW TX Control 				*/
	void                                *fwTxControlPtr;			/**< Pointer to FW RX Control 				*/
    TI_UINT32                           PacketMemoryPoolStart;      /**< RX Memory block offset 				*/  
} TDmaParams;

/** \struct TSecurityKeys
 * \brief Security Key
 * 
 * \par Description
 * Struct which holds Security Key Parameters
 * Used for handling DMA
 * 
 * \sa
 */ 
typedef struct
{
    EKeyType                            keyType; 				/**< Security Key Type (WEP, TKIP etc.)			*/               
    TI_UINT32                           encLen;					/**< Security Key length in bytes				*/
    TI_UINT8                            encKey[MAX_KEY_LEN];	/**< Security Key Encoding						*/
    TI_UINT8                            micRxKey[MAX_KEY_LEN];	/**< MIC RX Security Key 						*/
    TI_UINT8                            micTxKey[MAX_KEY_LEN];	/**< MIC TX Security Key						*/
    TI_UINT32                           keyIndex;     			/**< Security Key Index (id=0 is broadcast key)	*/                  
    TMacAddr                            macAddress;				/**< Security Key MAC Address					*/
    TI_UINT8                            keyRsc[KEY_RSC_LEN];	/**< Security Key RSC							*/

} TSecurityKeys;

/** \struct TxPktParams_t
 * \brief TX Packet Parameters
 * 
 * \par Description
 * Tx Control-Block Packet parameters that are not included in the Tx-descriptor
 * 
 * \sa
 */ 
typedef struct
{
    void *         pInputPkt;       /**< The input packet to the Tx path, either OS packet or raw buffer (see RAW_BUF_PKT flag) */         
    TI_UINT32      uInputPktLen;    /**< The input packet length in bytes (for freeing it in case of raw buffer)  */         
    TI_UINT32      uDriverDelay;    /**< The time in uSec the pkt was delayed in the driver until Xfer 			  */
    TI_UINT8       uPktType;        /**< See TX_PKT_TYPE_xxxx above                                               */          
    TI_UINT8       uHeadroomSize;   /**< Only for WHA - headroom in bytes before the payload in the packet buffer */          
    TI_UINT16      uFlags;          /**< See TX_CTRL_FLAG__xxxx above 										      */          

} TTxPktParams;


/** \struct TTxCtrlBlk
 * \brief TX Control Block Entry
 * 
 * \par Description
 * Contains the Tx packet parameters required for the Tx process, including
 * the Tx descriptor and the attributes required for HW-queue calculations.
 * TX Control Block Entry is allocated for each packet sent from the upper 
 * driver and freed upon Tx-complete.
 * The entry index is the descriptor-ID. It is written in the descriptor and 
 * copied back into the tx-complete results
 * 
 * \sa	SendPacketTranferCB_t, SendPacketDebugCB_t, TWD_txCtrlBlk_alloc, TWD_txCtrlBlk_free, TWD_txCtrlBlk_GetPointer, TWD_txXfer_sendPacket
 */ 
typedef struct _TTxCtrlBlk
{
    TTxnStruct          tTxnStruct;               /**< The transaction structure for packet queueing and transaction via the bus driver */ 
    TxIfDescriptor_t    tTxDescriptor;            /**< The packet descriptor copied to the FW  */    
    TI_UINT8            aPktHdr[MAX_HEADER_SIZE]; /**< The packet header + extensions (see description of MAX_HEADER_SIZE above) */    
    TTxPktParams        tTxPktParams;             /**< Per packet parameters not included in the descriptor */
    struct _TTxCtrlBlk  *pNextFreeEntry;          /**< Pointer to the next free entry */ 
    struct _TTxCtrlBlk  *pNextAggregEntry;        /**< Pointer to the next aggregated packet entry */

} TTxCtrlBlk;


/** \struct TTemplateParams
 * \brief Template Parameters
 * 
 * \par Description
 * 
 * \sa	TWD_GetTemplate
 */ 
typedef struct
{
    TI_UINT32            Size;		   				/**< Template size					*/  			
    TI_UINT32            uRateMask;                 /**< The rates bitmap for the frame */ 			
    TI_UINT8             Buffer[MAX_TEMPLATE_SIZE];	/**< Buffer which holds Template	*/  			

} TTemplateParams;

/** \struct TFwInfo
 * \brief FW Information
 * 
 * \par Description
 * 
 * \sa	TWD_GetFWInfo
 */ 
typedef struct
{
    TI_UINT8                            fwVer[FW_VERSION_LEN];  /**< Firmware version - null terminated string 	*/
    TMacAddr                            macAddress;				/**< MAC Address								*/
    TI_UINT8                            txPowerTable[NUMBER_OF_SUB_BANDS_E][NUM_OF_POWER_LEVEL]; /**< Maximun Dbm in Dbm/10 units */
    TI_UINT32                           uHardWareVersion;		/**< HW Version									*/

} TFwInfo;  

/** \struct TJoinBss
 * \brief Join BSS Parameters
 * 
 * \par Description
 * 
 * \sa	TWD_CmdJoinBss
 */ 
typedef struct
{
    ScanBssType_e                       bssType;			/**< */
    TI_UINT16                           beaconInterval;		/**< */
    TI_UINT16                           dtimInterval;		/**< */
    TI_UINT8                            channel;			/**< */
    TI_UINT8*                           pBSSID;				/**< */
    TI_UINT8*                           pSSID;				/**< */
    TI_UINT8                            ssidLength;			/**< */
    TI_UINT32                           basicRateSet;      	/**< */ 
    ERadioBand                          radioBand;			/**< */
    /* Current Tx-Session index as configured to FW in last Join command */
    TI_UINT16                           txSessionCount;    	/**< */     

} TJoinBss;

/** \struct TSetTemplate
 * \brief Set Template Parameters
 * 
 * \par Description
 * 
 * \sa	TWD_CmdTemplate, TWD_WriteMibTemplateFrame
 */ 
typedef struct
{
    ETemplateType                       type;	/**< Template Type							*/
    TI_UINT8                            index;  /**< only valid for keep-alive templates	*/
    TI_UINT8*                           ptr;	/**< Pointer to Template Data		  		*/
    TI_UINT32                           len;	/**< Template Length            	  		*/
    ERadioBand                          eBand; 	/**< only valid for probe request templates	*/
    TI_UINT32                           uRateMask;/**< The rate mask to use for this frame  */
    
} TSetTemplate;

/** \struct TNoiseHistogram
 * \brief Noise Histogram Parameters
 * 
 * \par Description
 * 
 * \sa	TWD_CmdNoiseHistogram
 */ 
typedef struct
{
    ENoiseHistogramCmd                  cmd;												/**< Noise Histogram Command (Start/Atop)	*/
    TI_UINT16                           sampleInterval;										/**< Sample Interval (in microsec)			*/
    TI_UINT8                            ranges [MEASUREMENT_NOISE_HISTOGRAM_NUM_OF_RANGES];	/**< Noise Histogram Ranges					*/

} TNoiseHistogram;

/** \struct TInterogateCmdHdr
 * \brief Interrogate Command Header
 * 
 * \par Description
 * 
 * \sa	TNoiseHistogramResults, TMediumOccupancy, TTsfDtim
 */ 
typedef struct
{
    TI_UINT16                           id;		/**< */
    TI_UINT16                           len;	/**< */

} TInterogateCmdHdr;

/** \struct TNoiseHistogramResults
 * \brief Noise Histogram Results
 * 
 * \par Description
 * Used for Getting Noise Histogram Parameters from FW
 * 
 * \sa
 */ 
typedef struct
{
    TInterogateCmdHdr                   noiseHistResCmdHdr;							/**< Results Header						*/
    TI_UINT32                           counters[NUM_OF_NOISE_HISTOGRAM_COUNTERS];	/**< Counters							*/
    TI_UINT32                           numOfLostCycles;							/**< Number of Lost Cycles				*/
    TI_UINT32                           numOfTxHwGenLostCycles;						/**< Number of Tx Hw Gen Lost Cycles	*/
    TI_UINT32                           numOfRxLostCycles;							/**< Number of RX Hw Gen Lost Cycles	*/

} TNoiseHistogramResults;

/** \struct TMediumOccupancy
 * \brief Medium Occupancy Parameters
 * 
 * \par Description
 * Used for Getting Medium Occupancy (Channal Load) from FW
 * or print Medium Occupancy (Channal Load) Debug Information
 * 
 * \sa
 */ 
typedef struct
{
    TInterogateCmdHdr                   mediumOccupCmdHdr;	/**< Command Header						*/
    TI_UINT32                           MediumUsage;		/**< Medium Occupancy Usage Time		*/
    TI_UINT32                           Period;				/**< Medium Occupancy Period Time		*/

} TMediumOccupancy;

/** \struct TTsfDtim
 * \brief Beacon TSF and DTIM count
 * 
 * \par Description
 * Used for Getting updated current TSF and last Beacon TSF and DTIM Count from FW
 * for Scan Purposes
 * 
 * \sa
 */ 
typedef struct
{
    TInterogateCmdHdr                   tsf_dtim_mibCmdHdr;	/**< Command Header						*/
    TI_UINT32                           CurrentTSFHigh;		/**< Current TSF High (of INT64) Value	*/
    TI_UINT32                           CurrentTSFLow;		/**< Current TSF Low (of INT64) Value	*/
    TI_UINT32                           lastTBTTHigh;		/**< Last TBTT High (of INT64) Value	*/
    TI_UINT32                           lastTBTTLow;		/**< Last TBTT Low (of INT64) Value		*/
    TI_UINT8                            LastDTIMCount;		/**< Last DTIM Count			      	*/
    TI_UINT8                            Reserved[3];		/**< Reserved							*/

} TTsfDtim;

/** \struct TBcnBrcOptions
 * \brief Beacon broadcast options
 * 
 * \par Description
 * Used for Getting/Configuring updated Beacon broadcast options from/to FW
 * 
 * \sa	TWD_SetDefaults
 */ 
typedef struct 
{
    TI_UINT16                           BeaconRxTimeout;		/**< Beacon RX Timeout			*/
    TI_UINT16                           BroadcastRxTimeout;		/**< Broadcast RX Timeout		*/
    TI_UINT8                            RxBroadcastInPs;		/**< RX Broadcast In Power Save	*/

} TBcnBrcOptions;

/** \struct TBeaconFilterIeTable
 * \brief Beacon Filter Information Elements Table
 * 
 * \par Description
 * Used for Getting/Configuring Beacon Filter IE Table From/To FW
 * 
 * \sa	TWD_SetDefaults
 */ 
typedef struct
{
    TI_UINT8                            numberOfIEs;							/**< Number of IE Tables 			*/
    TI_UINT8                            IETable[BEACON_FILTER_TABLE_MAX_SIZE];	/**< The IE table					*/
    TI_UINT8                            IETableSize;							/**< number of elements in IE table	*/

} TBeaconFilterIeTable;

/** \struct TBeaconFilterInitParams
 * \brief Beacon Filter Init Parameters
 * 
 * \par Description
 * Used for Init Beacon Filter IE Table in FW
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT8                            desiredState;								/**< Desigred state (required/not required)			*/
    TI_UINT8                            numOfStored;								/**< Number of desigred Beacon Filters stored in FW	*/
    TI_UINT8                            numOfElements;								/**< Number of Beacon Filter Elements stored in FW	*/
    TI_UINT8                            IETableSize;								/**< The IE Table size								*/
    TI_UINT8                            reserve[3];									/**< Reserved										*/
    TI_UINT8                            IETable[BEACON_FILTER_IE_TABLE_MAX_SIZE]; 	/**< The IE table							   		*/

} TBeaconFilterInitParams;

/** \struct TPowerMgmtConfig
 * \brief Power Management Configuration Parameters
 * 
 * \par Description
 * Used for Configuring Wake-Up Conditions or Beacon Broadcast Options to FW
 * 
 * \sa	TWD_CfgWakeUpCondition, TWD_CfgBcnBrcOptions
 */ 
typedef struct
{
	/* power management options */
    TI_UINT8                            beaconListenInterval;		/**< Beacon Listen Interavl:
																	* specify how often the TNET wakes up to listen to beacon frames. 
																	* the value is expressed in units of "beacon interval"	
																	*/
    TI_UINT8                            beaconFiltering;			/**< Beacon Filtering Desigred state (required/not required)			*/
    TI_UINT8                            DTIMListenInterval;			/**< DTIM Listen Interavl:
																	* specify how often the TNET wakes up to listen to DTIM frames. the value 
																	* is expressed in units of "dtim interval"
																	*/
    TI_UINT8                            NConsecutiveBeaconMiss;		/**< Consecutive Beacon Miss											*/
    TI_UINT8                            hangoverPeriod;				/**< Hang Over Period													*/
    TI_UINT8                            HwPsPollResponseTimeout;	/**< Power-Save Polling Response Time Out								*/
    TI_UINT32                           BaseBandWakeUpTime;			/**< Base Band Wakeup Time												*/
    TI_UINT32                           beaconReceiveTime;			/**< Beacon Receive Time												*/
    TI_BOOL                             beaconMissInterruptEnable;	/**< Enable/Disable Beacon Miss Interrupt   							*/
    TI_BOOL                             rxBroadcast;				/**< Enable/Disable receive of broadcast packets in Power-Save mode   	*/
    TI_BOOL                             hwPsPoll;					/**< Enable/Disable Power-Save Polling								   	*/    
    /* Power Management Configuration IE */
    TI_BOOL                             ps802_11Enable;				/**< Enable/Disable 802.11 Power-Save 									*/
    TI_UINT8                            needToSendNullData;  		/**< Indicates if need to send NULL data								*/
    TI_UINT8                            numNullPktRetries; 			/**< Number of NULL Packets allowed retries 							*/
    TI_UINT8                            hangOverPeriod;				/**< HangOver period:
																	* Indicates what is the time in TUs during which the WiLink remains awake 
																	* after sending an MPDU with the Power Save bit set (indicating that the 
																	* station is to go into Power Save mode). Setting bit 0 does not affect 
																	* the hangover period 
																	*/
    TI_UINT16                           NullPktRateModulation; 		/**< Null Packet Rate Modulation										*/
    /* PMConfigStruct */
    TI_BOOL                             ELPEnable;					/**< Enable/Disable ELP				 									*/
    TI_UINT32                           BBWakeUpTime;				/**< Base Band Wakeup Time				 								*/
    TI_UINT32                           PLLlockTime;				/**< PLL Lock Time						 								*/
    /* AcxBcnBrcOptions */						
    TBcnBrcOptions                      BcnBrcOptions;				/**< Beacon broadcast options	 		 								*/
    /* ACXWakeUpCondition */
    ETnetWakeOn                         tnetWakeupOn;  				/**< ACX Wake Up Condition		 		 								*/
    TI_UINT8                            listenInterval;				/**< ACX Listen Interval		 		 								*/
	/* No answer after Ps-Poll work-around */
    TI_UINT8  							ConsecutivePsPollDeliveryFailureThreshold;	/**< Power-Save Polling Delivery Failure Threshold		*/

} TPowerMgmtConfig;

/** \struct TPowerSaveParams
 * \brief Power Save Parameters 
 * 
 * \par Description 
 * 
 * \sa
 */ 
typedef struct
{
    /* powerMgmtConfig IE */
    TI_BOOL                             ps802_11Enable;			/**< Enable/Disable 802.11 Power-Save 									*/
    TI_UINT8                            needToSendNullData;  	/**< Indicates if need to send NULL data								*/
    TI_UINT8                            numNullPktRetries; 		/**< Number of NULL Packets allowed retries 							*/
    TI_UINT8                            hangOverPeriod;			/**< HangOver period:
																* Indicates what is the time in TUs during which the WiLink remains awake 
																* after sending an MPDU with the Power Save bit set (indicating that the 
																* station is to go into Power Save mode). Setting bit 0 does not affect 
																* the hangover period 
																*/
    EHwRateBitFiled                     NullPktRateModulation;	/**< Null Packet Rate Modulation										*/

} TPowerSaveParams;

/** \struct TAcQosParams
 * \brief AC QoS Parameters 
 * 
 * \par Description 
 * Used for Configuring AC Parameters (For Quality Of Service) to FW
 * 
 * \sa	TWD_CfgAcParams
 */ 
typedef struct  
{
    TI_UINT8                            ac;			/**< Access Category - The TX queue's access category	*/
    TI_UINT8                            cwMin;		/**< The contention window minimum size (in slots) 		*/
    TI_UINT16                           cwMax;		/**< The contention window maximum size (in slots)		*/
    TI_UINT8                            aifsn;		/**< The AIF value (in slots)							*/
    TI_UINT16                           txopLimit;	/**< The TX Op Limit (in microseconds)					*/

} TAcQosParams;

/** \struct TMeasurementParams
 * \brief AC Queues Parameters 
 * 
 * \par Description 
 * Used When Send Start Measurment Command to FW
 * 
 * \sa	TWD_CmdMeasurement
 */ 
typedef struct 
{
    TI_UINT32                           ConfigOptions;	/**< RX Filter Configuration Options													*/
    TI_UINT32                           FilterOptions;	/**< RX Filter Options																	*/
    TI_UINT32                           duration;		/**< Specifies the measurement process duration in microseconds. The value of 0 means 
														* infinite duration in which only a STOP_MEASUREMENT command can stop the measurement 
														* process
														*/
    Channel_e                           channel;		/**< Channel number on which the measurement is performed								*/
    RadioBand_e                         band;			/**< Specifies the band to which the channel belongs									*/
    EScanResultTag                      eTag;			/**< Scan Result Tag																	*/

} TMeasurementParams;

/** \struct TApDiscoveryParams
 * \brief AP Discovery Parameters
 * 
 * \par Description 
 * Used When Performing AP Discovery
 * 
 * \sa	TWD_CmdApDiscovery
 */ 
typedef struct 
{
    TI_UINT32                           ConfigOptions;	/**< RX Configuration Options for measurement														*/
    TI_UINT32                           FilterOptions;	/**< RX Filter Configuration Options for measurement												*/
    TI_UINT32                           scanDuration;	/**< This field specifies the amount of time, in time units (TUs), to perform the AP discovery		*/
    TI_UINT16                           scanOptions;	/**< This field specifies whether the AP discovery is performed by an active scan or a passive scan 
														* 0 - ACTIVE, 1 - PASSIVE
														*/
    TI_UINT8                            numOfProbRqst;	/**< This field indicates the number of probe requests to send per channel, when active scan is specified 
														* Note: for XCC measurement this value should be set to 1
														*/
    TI_UINT8                            txPowerDbm;    	/**< TX power level to be used for sending probe requests when active scan is specified.
														* If 0, leave normal TX power level for this channel
														*/
    EHwRateBitFiled                     txdRateSet;		/**< This EHwBitRate format field specifies the rate and modulation to transmit the probe request when 
														* an active scan is specifie
														*/
    ERadioBand                          eBand;			/**< Specifies the band to which the channel belongs												*/
} TApDiscoveryParams;

/** \struct TRroamingTriggerParams
 * \brief Roaming Trigger Parameters
 * 
 * \par Description 
 * 
 * \sa	TWD_CfgMaxTxRetry, TWD_CfgConnMonitParams
 */ 
typedef struct
{
    /* ACXConsNackTriggerParameters */
    TI_UINT8                            maxTxRetry;			/**< The number of frames transmission failures before issuing the "Max Tx Retry" event			*/

    /* ACXBssLossTsfSynchronize */
    TI_UINT16                           TsfMissThreshold;	/**< The number of consecutive beacons that can be lost before the WiLink raises the 
															* SYNCHRONIZATION_TIMEOUT event	
															*/
    TI_UINT16                           BssLossTimeout;		/**< The delay (in time units) between the time at which the device issues the SYNCHRONIZATION_TIMEOUT 
															* event until, if no probe response or beacon is received a BSS_LOSS event is issued
															*/
} TRroamingTriggerParams;

/** \struct TSwitchChannelParams
 * \brief Switch Channel Parameters
 * 
 * \par Description
 * Used for Switch channel Command
 * 
 * \sa	TWD_CmdSwitchChannel
 */ 
typedef struct
{
    TI_UINT8                            channelNumber;		/**< The new serving channel										*/         	
    TI_UINT8                            switchTime;			/**< Relative time of the serving channel switch in TBTT units   	*/
    TI_UINT8                            txFlag;				/**< 1: Suspend TX till switch time; 0: Do not suspend TX			*/
    TI_UINT8                            flush;				/**< 1: Flush TX at switch time; 0: Do not flush  					*/

} TSwitchChannelParams;

/** \struct TRxCounters
 * \brief RX Counters
 * 
 * \par Description
 * Used for Getting RX Counters from FW
 * 
 * \sa
 */ 
typedef struct
{    
    TI_UINT32                           RecvError; 			/**< Number of frames that a NIC receives but does not indicate to the protocols due to errors 	*/
    TI_UINT32                           RecvNoBuffer;   	/**< Number of frames that the NIC cannot receive due to lack of NIC receive buffer space 		*/      
    TI_UINT32                           FragmentsRecv;    	/**< Number of Fragments Received 																*/      
    TI_UINT32                           FrameDuplicates;	/**< Number of Farme Duplicates																	*/
    TI_UINT32                           FcsErrors;			/**< Number of frames that a NIC receives but does not indicate to the protocols due to errors	*/

} TRxCounters;

/** \struct TApPowerConstraint
 * \brief AP Power Constraint
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct  
{
    INFO_ELE_HDR												/**< Information Element Header		*/
    int8                      			powerConstraintOnBss;	/**< The attenuation from the regulatory power constraint as declared by the AP 
																* Units: dBm	;	Range: -20 - 30
																*/
} TApPowerConstraint;

/*
 * TConfigCmdCbParams, TInterrogateCmdCbParams:
 * Note that this structure is used by the GWSI 
 * both for setting (writing to the device) and
 * for retreiving (Reading from the device),
 * while being called with a completion CB
 */
/** \struct TConfigCmdCbParams
 * \brief Config Command CB Parameters
 * 
 * \par Description
 * The CB Parameters (Completino CB, Handle to CB Parameters and buffer of Input/Output Parameters) 
 * are used for Setting Parameters
 * 
 * \sa	TWD_SetParam
 */ 
typedef struct
{
    void*                               fCb;	/**< Completion CB function													*/
    TI_HANDLE                           hCb;	/**< CB handle																*/
    void*                               pCb;	/**< CBuffer contains the content to be written or the retrieved content	*/

} TConfigCmdCbParams;

/** \struct TInterrogateCmdCbParams
 * \brief Interrogate Command Parameters
 * 
 * \par Description
 * Interrogate Command Parameters are the same as configuration Command CB Parameters
 *
 * \sa	TWD_SetParam 
 */ 
typedef TConfigCmdCbParams TInterrogateCmdCbParams;

/** \struct TRxTimeOut
 * \brief RX Time Out
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT16                           psPoll;		/**< The maximum time that the device will wait to receive traffic from the AP after transmission of PS-poll	*/
    TI_UINT16                           UPSD;		/**< The maximum time that the device will wait to receive traffic from the AP after transmission from UPSD 
													* enabled queue
													*/
} TRxTimeOut;

/** \struct TQueueTrafficParams
 * \brief RX Time Out
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT8                            queueID;					/**< The TX queue ID number (0-7)											*/
    TI_UINT8                            channelType;				/**< Channel access type for the queue Refer to ChannelType_enum			*/
    TI_UINT8                            tsid;						/**< for EDCA - the AC Index (0-3, refer to AccessCategory_enum). 
																	* For HCCA - HCCA Traffic Stream ID (TSID) of the queue (8-15)	
																	*/
    TI_UINT32                           dot11EDCATableMSDULifeTime;	/**< 802.11 EDCA Table MSDU Life Time 										*/
    TI_UINT8                            psScheme;					/**< The power save scheme of the specified queue. Refer to PSScheme_enum	*/
    TI_UINT8                            ackPolicy;					/**< ACK policy per AC 														*/
    TI_UINT32                           APSDConf[2];				/**< APSD Configuration 													*/

} TQueueTrafficParams;



/** \struct TFmCoexParams
 * \brief FM Coexistence Parameters
 *
 * \par Description
 * Used for Setting/Printing FM Coexistence Parameters
 * 
 * \sa
 */ 
typedef struct
{	
    TI_UINT8   uEnable;                 /* enable(1) / disable(0) the FM Coex feature */

    TI_UINT8   uSwallowPeriod;          /* Swallow period used in COEX PLL swallowing mechanism,
                                           Range: 0-0xFF,  0xFF = use FW default
                                        */

    TI_UINT8   uNDividerFrefSet1;       /* The N divider used in COEX PLL swallowing mechanism for Fref of 38.4/19.2 Mhz.
                                           Range: 0-0xFF,  0xFF = use FW default
                                        */

    TI_UINT8   uNDividerFrefSet2;       /* The N divider used in COEX PLL swallowing mechanism for Fref of 26/52 Mhz.
                                           Range: 0-0xFF,  0xFF = use FW default
                                        */

    TI_UINT16  uMDividerFrefSet1;       /* The M divider used in COEX PLL swallowing mechanism for Fref of 38.4/19.2 Mhz.
                                           Range: 0-0x1FF,  0xFFFF = use FW default
                                        */

    TI_UINT16  uMDividerFrefSet2;       /* The M divider used in COEX PLL swallowing mechanism for Fref of 26/52 Mhz.
                                           Range: 0-0x1FF,  0xFFFF = use FW default
                                        */

    TI_UINT32  uCoexPllStabilizationTime;/* The time duration in uSec required for COEX PLL to stabilize.
                                           0xFFFFFFFF = use FW default
                                        */

    TI_UINT16  uLdoStabilizationTime;   /* The time duration in uSec required for LDO to stabilize.
                                           0xFFFFFFFF = use FW default
                                        */

    TI_UINT8   uFmDisturbedBandMargin;  /* The disturbed frequency band margin around the disturbed
                                             frequency center (single sided).
                                           For example, if 2 is configured, the following channels
                                             will be considered disturbed channel:
                                             80 +- 0.1 MHz, 91 +- 0.1 MHz, 98 +- 0.1 MHz, 102 +- 0.1 MHz
                                           0xFF = use FW default
                                        */

	TI_UINT8   uSwallowClkDif;          /* The swallow clock difference of the swallowing mechanism.
                                           0xFF = use FW default
                                        */

} TFmCoexParams;


/** \struct TMibBeaconFilterIeTable
 * \brief MIB Beacon Filter IE table
 * 
 * \par Description
 * Used for Read/Write the MIB/IE Beacon Filter
 * NOTE: This struct is only meant to be used as a pointer reference to an actual buffer. 
 * Table size is not a constant and is derived from the buffer size given with the 
 * user command
 * 
 * \sa	TWD_WriteMibBeaconFilterIETable
 */ 
typedef struct 
{
    /* Number of information elements in table  */
    TI_UINT8                            iNumberOfIEs;   					/**< Input Number of IE Tables	*/    
    TI_UINT8                            iIETable[MIB_MAX_SIZE_OF_IE_TABLE]; /**< Input IE Table				*/          

} TMibBeaconFilterIeTable;

/** \struct TMibCounterTable
 * \brief MIB Counter Table
 * 
 * \par Description
 * Used for Getting Counters of MIB Table
 * 
 * \sa
 */ 
typedef struct 
{
    TI_UINT32                           PLCPErrorCount;	  	/**< The number of PLCP errors since the last time this information element was interrogated. 
															* This field is automatically cleared when it is interrogated  
															*/
    TI_UINT32                           FCSErrorCount;		/**< The number of FCS errors since the last time this information element was interrogated. 
															* This field is automatically cleared when it is interrogated
															*/
    TI_UINT32                           SeqNumMissCount;	/**< The number of missed sequence numbers in the squentially values of frames seq numbers	*/
} TMibCounterTable;

/** \struct TMibWlanWakeUpInterval
 * \brief MIB WLAN Wake-Up Interval
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct 
{
    TI_UINT32                           WakeUpInterval;		/**< Wake Up Interval 								*/ 
    TI_UINT8                            ListenInterval; 	/**< Listen interval in unit of the beacon/DTIM 	*/   

} TMibWlanWakeUpInterval;

/** \struct TMibTemplateFrame
 * \brief MIB Template Frame
 * 
 * \par Description
 * Used for Writing MIB Frame Template to FW
 * 
 * \sa	TWD_WriteMibTemplateFrame
 */ 
typedef struct 
{
    EMibTemplateType                 	FrameType;							/**< MIB Farme Template type	*/
    TI_UINT32                           Rate;								/**< Frame Rate					*/
    TI_UINT16                           Length;								/**< Frame Length				*/
    TI_UINT8                            Data [MIB_TEMPLATE_DATA_MAX_LEN];	/**< Frame Template Data		*/

} TMibTemplateFrame;

/** \struct TMibArpIpAddressesTable
 * \brief MIB ARP Address Table
 * 
 * \par Description
 * Used for Writing MIB ARP Table Template to FW
 * 
 * \sa	TWD_WriteMib
 */ 
typedef struct 
{
    TI_UINT32                           FilteringEnable;	/**< Enable/Disable Filtering	*/
    TIpAddr                             addr;				/**< IP Address Table			*/

} TMibArpIpAddressesTable;

/** \struct TMibGroupAdressTable
 * \brief MIB Group Address Table
 * 
 * \par Description
 * Used for Writing MIB Group Table Template to FW
 * 
 * \sa	TWD_WriteMib
 */ 
typedef struct 
{
    TMacAddr                            aGroupTable[MIB_MAX_MULTICAST_GROUP_ADDRS]; 	/**< Table of Multicast Group Addresses */       
    TI_UINT8                            bFilteringEnable;								/**< Enable/Disable Filtering			*/  
    TI_UINT8                            nNumberOfAddresses;								/**< Number of Multicast Addresses		*/

} TMibGroupAdressTable;

/** \struct TTxRateClass
 * \brief TX Rate Class
 * 
 * \par Description
 * Used for Set/Get TX Rate Policy Class to/from FW
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT32                           txEnabledRates;			/**< A Bit Mask which indicates which Rates are enabled */
    TI_UINT8                            shortRetryLimit;		/**< */
    TI_UINT8                            longRetryLimit;			/**< */
    TI_UINT8                            flags;					/**< */
    TI_UINT8                            reserved;				/**< for alignment with the FW API */

} TTxRateClass;

/** \struct TTxRatePolicy
 * \brief TX Rate Policy
 * 
 * \par Description
 * Used for Set/Get TX Rate Policy to/from FW
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT32                           numOfRateClasses;								/**< */
    TTxRateClass                        rateClass[MAX_NUM_OF_TX_RATE_CLASS_POLICIES];	/**< */

} TTxRatePolicy;

/** \struct TCoexActivity
 * \brief CoexActivity Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct {
    uint8  coexIp;           /* 0-BT, 1-WLAN (according to CoexIp_e in FW) */
    uint8  activityId;       /* According to BT/WLAN activity numbering in FW */ 
    uint8  defaultPriority;  /* 0-255, activity default priority */
    uint8  raisedPriority;   /* 0-255, activity raised priority */
    uint16 minService;       /* 0-65535, The minimum service requested either in
                                requests or in milliseconds depending on activity ID */
    uint16 maxService;       /* 0-65535, The maximum service allowed either in
                            requests or in milliseconds depending on activity ID */
} TCoexActivity;

/** \struct THalCoexActivityTable
 * \brief CoexActivity Table Initialization Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    uint32 numOfElements;
    TCoexActivity entry[COEX_ACTIVITY_TABLE_MAX_NUM];
    
} THalCoexActivityTable;

/** \struct DcoItrimParams_t
 * \brief DCO Itrim params structure
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    Bool_e enable;
    uint32 moderationTimeoutUsec;
}DcoItrimParams_t;

/** \union TMibData
 * \brief MIB Data
 * 
 * \par Description
 * Union which defines all MIB Data types
 * Used for write MIB Configuration to FW
 * 
 * \sa	TWD_WriteMib
 */ 
typedef union 
{
    TMacAddr                            StationId;				/**< Station ID MAC Address								*/
    TI_UINT32                           MaxReceiveLifeTime;		/**< HAl Control Max Rx MSDU Life Time, in TUs Units	*/
    TI_UINT32                           SlotTime;				/**< Radio (PHY) Slot Time Type							*/
    TMibGroupAdressTable             	GroupAddressTable;		/**< MIB Group Address Table							*/
    TI_UINT8                            WepDefaultKeyId;		/**< WEP Defualt Security Key ID						*/
    TI_UINT8                            PowerLevel;				/**< */
    TI_UINT16                           RTSThreshold;			/**< */
    TI_UINT32                           CTSToSelfEnable;		/**< Enable/Disable CTS to Self							*/
    TMibArpIpAddressesTable          	ArpIpAddressesTable;	/**< MIB ARP Address Table								*/
    TMibTemplateFrame                	TemplateFrame;			/**< MIB Template Frame		 							*/
    TI_UINT8                            RxFilter;				/**< */
    TMibWlanWakeUpInterval           	WlanWakeUpInterval;		/**< MIB WLAN Wake-Up Interval							*/
    TMibCounterTable                 	CounterTable;			/**< MIB Counter Table									*/
    TMibBeaconFilterIeTable          	BeaconFilter;			/**< MIB Beacon Filter IE table				   			*/
    TTxRatePolicy                       txRatePolicy;			/**< TX Rate Policy			   		 					*/

} TMibData;

/** \struct TMib
 * \brief MIB Structure
 * 
 * \par Description
 * Used for writing MIB Configuration to FW
 * 
 * \sa	TWD_WriteMib, TWD_WriteMibTemplateFrame, TWD_WriteMibBeaconFilterIETable, TWD_WriteMibTxRatePolicy
 */ 
typedef struct
{
    EMib			aMib;  		/**< MIB Element Type	*/
    TI_UINT16       Length;		/**< MIB Data Length	*/
    TMibData     	aData; 		/**< MIB Data			*/

} TMib;

/** \union TTwdParamContents
 * \brief TWD Parameters Content
 * 
 * \par Description
 * All FW Parameters contents
 * 
 * \sa	TWD_SetParam
 */ 
typedef union
{
    TI_UINT16                           halCtrlRtsThreshold;			/**< */
    TI_UINT8                            halCtrlCtsToSelf;				/**< */
    TRxTimeOut                          halCtrlRxTimeOut;				/**< */
    TI_UINT16                           halCtrlFragThreshold;			/**< */
    TI_UINT16                           halCtrlListenInterval;			/**< */
    TI_UINT16                           halCtrlCurrentBeaconInterval;	/**< */
    TI_UINT8                            halCtrlTxPowerDbm;				/**< */
    ETxAntenna                          halCtrlTxAntenna;				/**< */
    ERxAntenna                          halCtrlRxAntenna;				/**< */
    TI_UINT8                            halCtrlAifs;					/**< */
    TI_BOOL                             halCtrlTxMemPoolQosAlgo;		/**< */
    TI_BOOL                             halCtrlClkRunEnable;			/**< */
    TRxCounters                         halCtrlCounters;				/**< */

    TMib*          		                pMib;							/**< */
    TI_UINT8                            halCtrlCurrentChannel;			/**< */
  
    /* AC Qos parameters */
    TQueueTrafficParams                 *pQueueTrafficParams;			/**< */
    
    /* Security related parameters */
#ifdef XCC_MODULE_INCLUDED
    TI_BOOL                             rsnXCCSwEncFlag;				/**< */
    TI_BOOL                             rsnXCCMicFieldFlag;				/**< */
#endif
    ECipherSuite                        rsnEncryptionStatus;			/**< */
    TI_UINT8                            rsnHwEncDecrEnable; 			/**< 0- disable, 1- enable */
    TSecurityKeys                       *pRsnKey;						/**< */
    TI_UINT8                            rsnDefaultKeyID;				/**< */

    /* Measurements section */
    TMediumOccupancy                    mediumOccupancy;				/**< */
    TI_BOOL                             halTxOpContinuation;			/**< */

    TTsfDtim                            fwTsfDtimInfo;					/**< */

    TInterrogateCmdCbParams             interogateCmdCBParams;			/**< */
    TConfigCmdCbParams                  configureCmdCBParams;			/**< */

    TTxRatePolicy                       *pTxRatePlicy;					/**< */

    /* WARNING!!! This section is used to set/get internal params only. */
    TI_UINT16                           halCtrlAid;						/**< */
    
    ESoftGeminiEnableModes              SoftGeminiEnable;				/**< */
    TSoftGeminiParams                   SoftGeminiParam;				/**< */

    TFmCoexParams                       tFmCoexParams;                  /**< */

    TI_UINT32                           halCtrlMaxRxMsduLifetime;		/**< */

    /* Beacon Broadcast options */
    TBcnBrcOptions                      BcnBrcOptions;					/**< */

	/* PLT tests */
	TI_STATUS             				PltRxCalibrationStatus;			/**< */

	/* CoexActivity */
	TCoexActivity                       tTwdParamsCoexActivity;         /**< */

    /* DCO Itrim */
    DcoItrimParams_t                    tDcoItrimParams;                /**< */

} TTwdParamContents;

/** \struct TTwdParamInfo
 * \brief TWD Parameters Information
 * 
 * \par Description
 * 
 * \sa	TWD_SetParam
 */ 
typedef struct
{
    TI_UINT32                           paramType;					/**< FW Parameter Information Identifier	*/
    TI_UINT32                           paramLength;				/**< FW Parameter Length					*/
    TTwdParamContents                   content;					/**< FW Parameter content					*/

} TTwdParamInfo;

/** \struct TRxXferReserved
 * \brief RX Xfer Reserved
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    ProcessIDTag_e                      packetType;		/**< */
    TI_UINT8                            rxLevel;		/**< */
    TI_INT8                             rssi;			/**< */
    TI_UINT8                            SNR;			/**< */
    TI_UINT8                            band;			/**< */
    TI_UINT32                           TimeStamp;		/**< */
    EScanResultTag                      eScanTag;		/**< */

} TRxXferReserved;

/** \struct TRxAttr
 * \brief RX Attributes
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    PacketClassTag_e                    ePacketType;    /**< */
    TI_STATUS                           status;			/**< */
    ERate                               Rate;   		/**< */
    TI_UINT8                            SNR;			/**< */
    TI_INT8                             Rssi;   		/**< */
    TI_UINT8                            channel;		/**< */
    TI_UINT32                           packetInfo;		/**< */
    ERadioBand                          band;			/**< */
    TI_UINT32                           TimeStamp;		/**< */
    EScanResultTag                      eScanTag;		/**< */

} TRxAttr;


/** \struct TGeneralInitParams
 * \brief General Initialization Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT16                           halCtrlListenInterval;				/**< */
    TI_UINT8                            halCtrlCalibrationChannel2_4;		/**< */
    TI_UINT8                            halCtrlCalibrationChannel5_0;	  	/**< */  	
    TI_UINT16                           halCtrlRtsThreshold;				/**< */
    TI_UINT16                           halCtrlFragThreshold;				/**< */
    TI_UINT32                           halCtrlMaxTxMsduLifetime;			/**< */
    TI_UINT32                           halCtrlMaxRxMsduLifetime;			/**< */
    ETxAntenna                          halCtrlTxAntenna;					/**< */
    ERxAntenna                          halCtrlRxAntenna;					/**< */
    TI_UINT8                            halCtrlMacClock;					/**< */
    TI_UINT8                            halCtrlArmClock;					/**< */
    TI_UINT16                           halCtrlBcnRxTime;					/**< */
    TI_BOOL                             halCtrlRxEnergyDetection;    		/**< */
    TI_BOOL                             halCtrlCh14TelecCca;				/**< */
    TI_BOOL                             halCtrlEepromLessEnable;			/**< */
    TI_BOOL                             halCtrlRxDisableBroadcast;			/**< */
    TI_BOOL                             halCtrlRecoveryEnable;				/**< */
    TI_BOOL                             halCtrlFirmwareDebug;				/**< */
    TI_BOOL                             WiFiWmmPS;							/**< */
    TRxTimeOut                          rxTimeOut;							/**< */
    TI_UINT8                            halCtrlRateFallbackRetry;			/**< */
    TI_BOOL                             b11nEnable;							/**< */

    TI_UINT16                           TxCompletePacingThreshold;			/**< */
    TI_UINT16                           TxCompletePacingTimeout;			/**< */
    TI_UINT16                           RxIntrPacingThreshold;			    /**< */
    TI_UINT16                           RxIntrPacingTimeout;			    /**< */

    TI_UINT32                           uRxAggregPktsLimit;					/**< */
    TI_UINT32                           uTxAggregPktsLimit;					/**< */
    TI_UINT8                            hwAccessMethod;						/**< */
    TI_UINT8                            maxSitesFragCollect;				/**< */
    TI_UINT8                            packetDetectionThreshold;			/**< */
    TI_UINT32                           nullTemplateSize;					/**< */
    TI_UINT32                           disconnTemplateSize;				/**< */
    TI_UINT32                           beaconTemplateSize;					/**< */
    TI_UINT32                           probeRequestTemplateSize;			/**< */
    TI_UINT32                           probeResponseTemplateSize;			/**< */
    TI_UINT32                           PsPollTemplateSize;				   	/**< */ 	
    TI_UINT32                           qosNullDataTemplateSize;			/**< */
    TI_UINT32                           ArpRspTemplateSize;                 /**< */
    TI_UINT32                           tddRadioCalTimout;					/**< */
    TI_UINT32                           CrtRadioCalTimout;					/**< */
    TI_UINT32                           UseMboxInterrupt;					/**< */
    TI_UINT32                           TraceBufferSize;					/**< */
    TI_BOOL                             bDoPrint;							/**< */
    TI_UINT8                            StaMacAddress[MAC_ADDR_LEN];		/**< */
    TI_BOOL                             TxFlashEnable;						/**< */
    TI_UINT8                            RxBroadcastInPs;					/**< */
	TI_UINT8       						ConsecutivePsPollDeliveryFailureThreshold;	/**< */
    TI_UINT8                            TxBlocksThresholdPerAc[MAX_NUM_OF_AC];/**< */
    TI_UINT8                            uRxMemBlksNum;                      /**< */
    TI_UINT16                           BeaconRxTimeout;					/**< */
    TI_UINT16                           BroadcastRxTimeout;					/**< */

    TI_UINT8                            uRssiBeaconAverageWeight;			/**< */
    TI_UINT8                            uRssiPacketAverageWeight;			/**< */
    TI_UINT8                            uSnrBeaconAverageWeight;			/**< */
    TI_UINT8                            uSnrPacketAverageWeight;			/**< */

    TI_UINT32                           uHostClkSettlingTime;				/**< */
    TI_UINT8                            uHostFastWakeupSupport;             /**< */
    THalCoexActivityTable               halCoexActivityTable;               /**< */
    TFmCoexParams                       tFmCoexParams;                      /**< */
    TI_UINT8                            uMaxAMPDU;                          /**< */

} TGeneralInitParams;

/** \struct TPowerSrvInitParams
 * \brief Power Service Init Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct 
{
    TI_UINT8                            numNullPktRetries; 			/**< */
    TI_UINT8                            hangOverPeriod;				/**< */
    TI_UINT16                           reserve;					/**< */

} TPowerSrvInitParams;

/** \struct TScanSrvInitParams
 * \brief Scan Service Init Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT32                           numberOfNoScanCompleteToRecovery;	/**< The number of consecutive no scan complete that will trigger a recovery notification 	*/
    TI_UINT32                      		uTriggeredScanTimeOut; 				/**< i.e. split scan. Time out for starting triggered scan between 2 channels 				*/

} TScanSrvInitParams;

/** \struct TArpIpFilterInitParams
 * \brief ARP IP Filter Init Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    EArpFilterType     filterType;	/**< */
    TIpAddr            addr;    			/**< */

} TArpIpFilterInitParams;

/** \struct TMacAddrFilterInitParams
 * \brief AMC Address Filter Init Parameters
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct
{
    TI_UINT8                            isFilterEnabled;							/**< */
    TI_UINT8                            numOfMacAddresses;							/**< */
    TI_UINT16                           reserve;									/**< */
    TMacAddr                            macAddrTable[MAX_MULTICAST_GROUP_ADDRS];	/**< */

} TMacAddrFilterInitParams;

/** \struct RateMangeParams_t
 * \brief Rate Maangement params structure
 *
 * \par Description
 *
 * \sa
 */
typedef struct
{
	rateAdaptParam_e paramIndex;
	uint16 RateRetryScore;
	uint16 PerAdd;
	uint16 PerTh1;
	uint16 PerTh2;
	uint16 MaxPer;
	uint8 InverseCuriosityFactor;
	uint8 TxFailLowTh;
	uint8 TxFailHighTh;
	uint8 PerAlphaShift;
	uint8 PerAddShift;
	uint8 PerBeta1Shift;
	uint8 PerBeta2Shift;
	uint8 RateCheckUp;
	uint8 RateCheckDown;
	uint8 RateRetryPolicy[13];
}RateMangeParams_t;

/*
 * IMPORTANT NOTE:
 * ===============
 * This structure encapsulates the initialization data required by the TnetwDrv layer.
 * All structures in it are arranged so no padding will be added by the compiler!!
 * This is required to avoid missalignment when compiled by customers using GWSI API!!
 */
/** \struct TTwdInitParams
 * \brief TWD Init Parameters
 * 
 * \par Description
 * All TWD Initialization Parameters
 * 
 * \sa	TWD_SetDefaults
 */ 
typedef struct
{
    TGeneralInitParams                  tGeneral;			 /**< General Initialization Parameters			*/
    TPowerSrvInitParams                 tPowerSrv;			 /**< Power Service Initialization Parameters	*/
    TScanSrvInitParams                  tScanSrv;			 /**< Scan Service Initialization Parameters    */
    TArpIpFilterInitParams              tArpIpFilter;		 /**< ARP IP filter Initialization Parameters	*/
    TMacAddrFilterInitParams            tMacAddrFilter;		 /**< MAC Address Initialization Parameters		*/
    IniFileRadioParam                   tIniFileRadioParams; /**< Radio Initialization Parameters   		*/
	IniFileExtendedRadioParam			tIniFileExtRadioParams; /**< Radio Initialization Parameters   		*/
    IniFileGeneralParam                 tPlatformGenParams;  /**< Radio Initialization Parameters           */
	RateMangeParams_t					tRateMngParams;			  
    DcoItrimParams_t                    tDcoItrimParams;     /**< Dco Itrim Parameters                      */
   
} TTwdInitParams;

/** \struct TTwdHtCapabilities
 * \brief TWD HT Capabilities
 * 
 * \par Description
 * 
 * \sa	TWD_SetDefaults, TWD_GetTwdHtCapabilities
 */ 
typedef struct
{
    TI_BOOL     b11nEnable;       					/**< Enable/Disable 802.11n flag	*/
    TI_UINT8    uChannelWidth;						/**< */
    TI_UINT8    uRxSTBC;							/**< */
    TI_UINT8    uMaxAMSDU;						  	/**< */  	
    TI_UINT8    uMaxAMPDU;							/**< */
	TI_UINT8    uAMPDUSpacing;					   	/**< */ 	
	TI_UINT8    aRxMCS[RX_TX_MCS_BITMASK_SIZE];		/**< */    
	TI_UINT8    aTxMCS[RX_TX_MCS_BITMASK_SIZE];		/**< */    
	TI_UINT16   uRxMaxDataRate;						/**< */    
	TI_UINT8    uPCOTransTime;						/**< */    
	TI_UINT32   uHTCapabilitiesBitMask;				/**< */    
	TI_UINT8    uMCSFeedback;						/**< */    
} TTwdHtCapabilities;

typedef struct
{
    int32  SNRCorrectionHighLimit;
    int32  SNRCorrectionLowLimit;
    int32  PERErrorTH;
    int32  attemptEvaluateTH;
    int32  goodAttemptTH;
    int32  curveCorrectionStep;

 }RateMangeReadParams_t;


/*
 * --------------------------------------------------------------
 *	APIs
 * --------------------------------------------------------------
 */
/** @ingroup Control
 * \brief Send Packet Transfer CB
 * 
 * \param  CBObj        - object handle
 * \param  pPktCtrlBlk  - Pointer to Input Packet Control Block
 * \return void
 * 
 * \par Description
 * The Transfer-Done callback
 * User registers the CB for Send Packet Transfer done
 *
 * \sa	TWD_RegisterCb
 */ 
typedef void (* TSendPacketTranferCb)(TI_HANDLE CBObj, TTxCtrlBlk *pPktCtrlBlk);
/** @ingroup Control
 * \brief Send Packet Debug CB
 * 
 * \param  CBObj        - object handle
 * \param  pPktCtrlBlk  - Pointer to Input Packet Control Block
 * uDebugInfo			- Debug Information
 * \return void
 * 
 * \par Description
 * The Transfer-Done Debug callback
 *
 * \sa
 */ 
typedef void (* TSendPacketDebugCb)  (TI_HANDLE CBObj, TTxCtrlBlk *pPktCtrlBlk, TI_UINT32 uDebugInfo);
/** @ingroup Control
 * \brief Send Packet Debug CB
 * 
 * \param  CBObj        - object handle
 * \param  pPktCtrlBlk  - Pointer to Input Packet Control Block
 * uDebugInfo			- Debug Information
 * \return void
 * 
 * \par Description
 *
 * \sa	TWD_RegisterCb
 */ 
typedef ERxBufferStatus (*TRequestForBufferCb) (TI_HANDLE hObj, void **pRxBuffer, TI_UINT16 aLength, TI_UINT32 uEncryptionFlag, PacketClassTag_e ePacketClassTag);
/** @ingroup Control
 * \brief Send Packet Debug CB
 * 
 * \param  hObj        	- object handle
 * \param  pBuffer	    - Pointer to Received buffer frame
 * \return void
 * 
 * \par Description
 * This function CB will be called when Received packet from RX Queue
 * User registers the CB for RX Buffer Request
 *
 * \sa	TWD_RegisterCb
 */ 
typedef void (*TPacketReceiveCb) (TI_HANDLE 	hObj,
                                  const void 	*pBuffer);
/** @ingroup Control
 * \brief Failure Event CB
 * 
 * \param  handle        	- object handle
 * \param  eFailureEvent  	- Failure Event Type
 * \return void
 * 
 * \par Description
 * Callback clled for Failure event
 * User registers the CB for Health-Moitoring
 *
 * \sa	TWD_RegisterCb
 */ 
typedef void (*TFailureEventCb)  (TI_HANDLE handle, EFailureEvent eFailureEvent);

/** \union TTwdCB
 * \brief TWD Callback
 * 
 * \par Description
 * Union which holds all TWD Internal Callbacks which are registered by user
 * per Module and Event IDs
 * 
 * \sa	TWD_RegisterCb
 */ 
typedef union
{
	TSendPacketTranferCb	sendPacketCB;		/**< Transfer-Done callback			*/
	TSendPacketDebugCb		sendPacketDbgCB;	/**< Transfer-Done Debug callback	*/
	TRequestForBufferCb		requestBufferCB;	/**< Transfer-Done Debug callback	*/
	TPacketReceiveCb		recvPacketCB;		/**< RX Buffer Request callback		*/
	TFailureEventCb			failureEventCB;		/**< Failure Event callback			*/
}TTwdCB;


/** @ingroup Control
 * \brief Scan Service complete CB
 * 
 * \param  hCb        	- handle to the scan object
 * \param  eTag  		- the scan results type tag
 * \param  uResultCount - number of results received during this scan
 * \param  SPSStatus  	- bitmap indicating which channels were attempted (if this is an SPS scan)
 * \param  TSFError  	- whether a TSF error occurred (if this is an SPS scan)
 * \param  ScanStatus  	- scan SRV status (OK / NOK)
 * \param  PSMode		- Power Save Mode
 * \return void
 * 
 * \par Description
 * This function CB will be called when Scan Service is complete
 * User registers the Scan Service Complete CB
 * 
 * \sa	TWD_RegisterScanCompleteCb
 */ 
typedef void (*TScanSrvCompleteCb) (TI_HANDLE 		hCb, 
									EScanResultTag 	eTag, 
									TI_UINT32 		uResultCount,
                                    TI_UINT16 		SPSStatus, 
									TI_BOOL 		TSFError, 
									TI_STATUS 		ScanStatus,
                                    TI_STATUS 		PSMode);
/** @ingroup Control
 * \brief TWD Callback
 * 
 * \param  hCb        	- handle to object
 * \param  status  		- completion status
 * \return void
 * 
 * \par Description
 * Initialising Complete Callaback (exapmle: Init HW/FW CB etc.)
 * User can use its own Complete CB which will be called when
 * the suitable module id & event number will arrive
 * 
 * \sa	TWD_Init
 */ 
typedef void (*TTwdCallback) (TI_HANDLE hCb, TI_STATUS status); 
/** @ingroup Control
 * \brief TWD Callback
 * 
 * \param  hCb        	- handle to object
 * \param  msrReply  	- Pointer to input measurement (which ended) Reply
 * \return void
 * 
 * \par Description
 * The function prototype for the measurement complete callback
 * User can use its own measurement complete CB
 * which will be called when measurement end
 * 
 * \sa	TWD_StartMeasurement
 */ 
typedef void (*TMeasurementSrvCompleteCb) (TI_HANDLE hCb, TMeasurementReply* msrReply);
/** @ingroup Control
 * \brief Command Response Callback
 * 
 * \param  hCb        	- handle to object
 * \param  status  		- status of Command ended
 * \return void
 * 
 * \par Description
 * The function prototype for the Command Response CB
 * Enables user to implement and use its own Response CB
 * which will be called when TWD Command end
 * 
 * \sa	TWD_StartMeasurement, TWD_StopMeasurement, TWD_Scan, TWD_StopScan, TWD_StartPeriodicScan, TWD_StopPeriodicScan
 */ 
typedef void (*TCmdResponseCb) (TI_HANDLE hCb, TI_UINT16 status);
/** @ingroup Control
 * \brief Command Response Callback
 * 
 * \param  hCb        	- handle to object
 * \param  status  		- status of Command ended
 * \return void
 * 
 * \par Description
 * The function prototype for the Power Save Set Response CB
 * Enables user to implement and use its own Response CB which 
 * will be called when Power Save Set Command end
 * 
 * \sa	TWD_SetPsMode
 */ 
typedef void (*TPowerSaveResponseCb)(TI_HANDLE hCb, TI_UINT8 status);
/** @ingroup Control
 * \brief Command Complete Callback
 * 
 * \param  hCb        	- handle to object
 * \param  PsMode		- Power Save Mode
 * \param  status  		- status of Command ended
 * \return void
 * 
 * \par Description
 * The function prototype for the Power Save Set Complete CB
 * Enables user to implement and use its own Complete CB which 
 * will be called when Power Save Set Command end (for success/faild notification)
 * 
 * \sa	TWD_SetPsMode
 */ 
typedef void (*TPowerSaveCompleteCb )(TI_HANDLE hCb, TI_UINT8 PsMode, TI_UINT8 status);
/** @ingroup Control
 * \brief  Create TWD Module
 * 
 * \param  hOs   	- OS module object handle
 * \return TWD Module object handle on success or NULL on failure
 * 
 * \par Description
 * Calling this function Creates a TWD object and all its sub-modules. 
 * 
 * \sa     TWD_Destroy, TWD_Init
 */
TI_HANDLE TWD_Create (TI_HANDLE hOs);
/** @ingroup Control
 * \brief  Destroy TWD Module
 * 
 * \param  hTWD   	- hTWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Calling this function Destroys TWD object and all its sub-modules. 
 * 
 * \sa     TWD_Create
 */ 
TI_STATUS TWD_Destroy (TI_HANDLE hTWD);
/** Control
 * \brief  Init TWD module
 * 
 * \param  hTWD         - TWD module object handle
 * \param  hReport      - Report module object handle
 * \param  hUser        - Master (User) module object handle
 * \param  hTimer       - Timer module object handle
 * \param  hContext     - context-engine module object handle
 * \param  hTxnQ        - TxnQueue module object handle
 * \param  fInitHwCb    - Init HW callback called when init HW phase is done
 * \param  fInitFwCb    - Init FW callback called when init FW phase is done
 * \param  fConfigFwCb  - Configuration FW callback called when configuring FW phase is done
 * \param  fStopCb      - Stop callback called when TWD is stopped
 * \param  fInitFailCb  - Fail callback called when TWD is Failed
 * \return void 
 * 
 * \par Description
 * Start hardware Init and Config process. 
 * This is the first function that must be called after TWD_Create.
 * 
 * \sa     TWD_Create, TWD_Stop
 */ 
void TWD_Init (TI_HANDLE    hTWD, 
			   TI_HANDLE 	hReport, 
               TI_HANDLE 	hUser, 
			   TI_HANDLE 	hTimer, 
			   TI_HANDLE 	hContext, 
			   TI_HANDLE 	hTxnQ, 
               TTwdCallback fInitHwCb, 
               TTwdCallback fInitFwCb, 
               TTwdCallback fConfigFwCb,
			   TTwdCallback	fStopCb,
			   TTwdCallback fInitFailCb);
/** @ingroup Control
 * \brief  Init HW module
 * 
 * \param  hTWD         - TWD module object handle
 * \param  pbuf         - Pointer to Input NVS Buffer
 * \param  length       - Length of NVS Buffer
 * \param  uRxDmaBufLen - The Rx DMA buffer length in bytes (needed as a limit for Rx aggregation length)
 * \param  uTxDmaBufLen - The Tx DMA buffer length in bytes (needed as a limit for Tx aggregation length)
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Performs the HW init process. 
 * 
 * \sa
 */ 
TI_STATUS TWD_InitHw (TI_HANDLE hTWD, 
                      TI_UINT8 *pbuf, 
                      TI_UINT32 length, 
                      TI_UINT32 uRxDmaBufLen,
                      TI_UINT32 uTxDmaBufLen);
/** @ingroup Control
 * \brief Set Defults to TWD Init Params
 * 
 * \param  hTWD         - TWD module object handle
 * \param  pInitParams  - Pointer to Input init default parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_SetDefaults (TI_HANDLE hTWD, TTwdInitParams *pInitParams);
/** @ingroup Control
 * \brief  Init FW
 * 
 * \param  hTWD         - TWD module object handle
 * \param  pFileInfo    - Pointer to Input Buffer contains part of FW Image to Download 
 * 							The Following Field should be filled:
 * 							pFileInfo->pBuffer
 * 							pFileInfo->uLength
 * 							pFileInfo->uAddress
 * 							pFileInfo->bLast
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Performs FW Download, and Run FW
 * 
 * \sa
 */ 
TI_STATUS TWD_InitFw (TI_HANDLE hTWD, TFileInfo *pFileInfo);
/** @ingroup Control
 * \brief  Open UART Bus Txn
 * 
 * \param  hTWD         - TWD module object handle
 * \param  pParams      - Pointer to Input parameters 
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_BusOpen (TI_HANDLE hTWD, void* pParams);
/** @ingroup Control
 * \brief  Close UART Bus Txn
 * 
 * \param  hTWD         - TWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_BusClose (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Halt firmware
 * 
 * \param  hTWD         - TWD module object handle
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_Stop (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Enable external events from FW
 * 
 * \param  hTWD         - TWD module object handle
 * \return void
 * 
 * \par Description
 * Enable external events from FW upon driver start or recovery completion
 *
 * \sa
 */ 
void TWD_EnableExternalEvents (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Registers TWD internal callback function
 * 
 * \param  hTWD         - TWD module object handle
 * \param  event        - event on which the registrated CB will be called
 * \param  fCb 	        - Pointer to Registered CB function
 * \param  pData 	    - Pointer to Registered CB data
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * This CB enables user to register TWD Internal CB functions of different Modules,
 * with their data.
 * The function identifies which TWD Module owns the Registered CB, and what the specific Registered CB
 * according to event input parameter.
 * Once the Module and specific CB function are identified, the CB is registerd in the TWD Module 
 * by calling suitable registration CB function
 * 
 * \sa
 */ 
TI_STATUS TWD_RegisterCb (TI_HANDLE hTWD, TI_UINT32 event, TTwdCB *fCb, void *pData);
/** @ingroup Control
 * \brief Exit from init mode
 * 
 * \param  hTWD         - TWD module object handle
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Exit From Init Mode enable sending command to the MboxQueue (which store a command),
 * while the interrupts are masked. 
 * The interrupt would be enable at the end of the init process
 * 
 * \sa
 */ 
TI_STATUS TWD_ExitFromInitMode (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Finalize FW init and download
 * 
 * \param  hTWD         - TWD module object handle
 * \return void 
 * 
 * \par Description
 * Init all the remaining initialization after the FW download has finished
 * 
 * \sa
 */ 
void TWD_FinalizeDownload (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Finalize of faulty FW init and download
 * 
 * \param  hTWD         - TWD module object handle
 * \return void 
 * 
 * \par Description
 * Call the upper layer failure callback after Init or FW download has finished with failure.
 * 
 * \sa
 */ 
void TWD_FinalizeOnFailure (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Perform FW Configuration
 * 
 * \param  hTWD         - TWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure the FW from the TWD DB - after configuring all HW objects
 * 
 * \sa
 */ 
TI_STATUS TWD_ConfigFw (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Handle FW interrupt from ISR context
 * 
 * \param  hTWD         - TWD module object handle
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * This is the FW-interrupt ISR context. The driver task is scheduled to hadnle FW-Events
 * 
 * \sa
 */ 
TI_STATUS TWD_InterruptRequest (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Enable Recovery
 * 
 * \param  hTWD         - TWD module object handle
 * \return TRUE if recovery Enables, FALSE otherwise
 * 
 * \par Description
 * Return Recovery E/D status
 *
 * \sa
 */ 
TI_BOOL TWD_RecoveryEnabled (TI_HANDLE hTWD);
/** @ingroup Measurement
 * \brief Starts a measurement 
 * 
 * \param  hTWD         			- TWD module object handle
 * \param  pMsrRequest         		- Pointer to Input structure which contains the measurement parameters
 * \param  uTimeToRequestExpiryMs   - The time (in milliseconds) the measurement SRV has to start the request
 * \param  fResponseCb         		- The Command response CB Function
 * \param  hResponseCb         		- Handle to Command response CB Function Obj
 * \param  fCompleteCb         		- The Command Complete CB Function
 * \param  hCompleteCb         		- Handle to Command Complete CB Function Obj
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Starts a measurement operation 
 *
 * \sa
 */ 
TI_STATUS TWD_StartMeasurement (TI_HANDLE hTWD, 
								TMeasurementRequest *pMsrRequest, 
								TI_UINT32 uTimeToRequestExpiryMs, 
								TCmdResponseCb fResponseCb, 
								TI_HANDLE hResponseCb, 
								TMeasurementSrvCompleteCb fCompleteCb, 
								TI_HANDLE hCompleteCb);
/** @ingroup Measurement
 * \brief Stops a measurement
 * 
 * \param  hTWD         			- TWD module object handle
 * \param  bSendNullData         	- Indicates whether to send NULL data when exiting driver mode
 * \param  fResponseCb         		- Pointer to Command response CB function
 * \param  hResponseCb         		- Handle to Command response CB parameters
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Stops a measurement operation in progress
 *
 * \sa
 */ 
TI_STATUS TWD_StopMeasurement (TI_HANDLE hTWD, 
							   TI_BOOL bSendNullData, 
							   TCmdResponseCb fResponseCb, 
							   TI_HANDLE hResponseCb);
/** @ingroup Measurement
 * \brief Start scan
 * 
 * \param hTWD                		- TWD module object handle
 * \param pScanParams            	- Pointer to Input Scan specific parameters
 * \param eScanTag               	- Scan tag, used for result and scan complete tracking
 * \param bHighPriority          	- Indicates whether to perform a high priority (overlaps DTIM) scan
 * \param bDriverMode            	- Indicates whether to try to enter driver mode (with PS on) before issuing the scan command
 * \param bScanOnDriverModeError 	- Indicates whether to proceed with the scan if requested to enter driver mode and failed 
 * \param ePsRequest             	- Parameter sent to PowerSaveServer. 
 * 										Should indicates PS ON or "keep current" only when driver mode is requested, 
 * 										Otherwise should indicate OFF
 * \param bSendNullData          	- Indicates whether to send Null data when exiting driver mode on scan complete
 * \param fResponseCb            	- The Response CB Function which called after downloading the command
 * \param hResponseCb            	- Handle to the Response CB Function Obj (Notice : last 2 params are NULL in Legacy run)
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Start scan. enter driver mode (PS) only if station is connected
 *
 * \sa
 */ 
TI_STATUS TWD_Scan (TI_HANDLE hTWD, 
					TScanParams *pScanParams, 
					EScanResultTag eScanTag, 
					TI_BOOL bHighPriority, 
					TI_BOOL bDriverMode, 
					TI_BOOL bScanOnDriverModeError, 
					E80211PsMode ePsRequest, 
					TI_BOOL bSendNullData, 
					TCmdResponseCb fResponseCb, 
					TI_HANDLE hResponseCb);
/** @ingroup Measurement
 * \brief Stop scan
 * 
 * \param hTWD                		- TWD module object handle
 * \param eScanTag               	- Scan tag, used to track scan complete and result
 * \param bSendNullData          	- Indicates whether to send Null data when exiting driver mode
 * \param fScanCommandResponseCb 	- The Response CB Function which called after downloading the command
 * \param hCb                    	- Handle to the Response CB Function Obj (Notice : last 2 params are NULL in Legacy run)
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Sends a Stop Scan command to FW, no matter if we are in scan progress or not
 *
 * \sa
 */ 
TI_STATUS TWD_StopScan (TI_HANDLE hTWD, 
						EScanResultTag eScanTag, 
						TI_BOOL bSendNullData, 
						TCmdResponseCb fScanCommandResponseCb, 
						TI_HANDLE hCb);
/** @ingroup Measurement
 * \brief Stop Scan on FW Reset
 * 
 * \param hTWD		- TWD module object handle
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Stop scan operation when a FW reset (recovery) situation is detected (by all processes 
 * other than scan)
 *
 * \sa
 */ 
TI_STATUS TWD_StopScanOnFWReset (TI_HANDLE hTWD);
/** @ingroup Measurement
 * \brief Start Connection Periodic Scan operation
 * 
 * \param hTWD                			- TWD module object handle
 * \param  pPeriodicScanParams    		- Pointer to Input Parameters Structures for the Periodic Scan operation
 * \param  eScanTag               		- Scan tag, used for scan complete and result tracking
 * \param  uPassiveScanDfsDwellTimeMs 	- Passive dwell time for DFS channels (in milli-secs)
 * \param  fResponseCb            		- Response CB Function which is called after downloading the command
 * \param  hResponseCb            		- Handle to Response CB Function Obj (Notice : last 2 params are NULL in Legacy run)
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Perform Connection Scan periodically
 *
 * \sa
 */ 
TI_STATUS TWD_StartConnectionScan (TI_HANDLE hTWD, 
								 TPeriodicScanParams *pPeriodicScanParams, 
								 EScanResultTag eScanTag, 
								 TI_UINT32 uPassiveScanDfsDwellTimeMs, 
								 TCmdResponseCb fResponseCb, 
								 TI_HANDLE hResponseCb);
/** @ingroup Measurement
 * \brief Stop Periodic Scan operation
 * 
 * \param hTWD 					- TWD module object handle
 * \param eScanTag              - scan tag, used for scan complete and result tracking
 * \param  fResponseCb          - Response CB Function which is called after downloading the command
 * \param  hResponseCb          - Handle to Response CB Function Obj (Notice : last 2 params are NULL in Legacy run)
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Stop Periodic Connection Scan
 *
 * \sa
 */ 
TI_STATUS TWD_StopPeriodicScan (TI_HANDLE hTWD, 
								EScanResultTag eScanTag, 
								TCmdResponseCb fResponseCb, 
								TI_HANDLE hResponseCb);
/** @ingroup Measurement
 * \brief Register CB for Scan Complete
 * 
 * \param  hTWD         		- TWD module object handle
 * \param  fScanCompleteCb     	- The Complete CB Function
 * \param  hScanCompleteCb   	- Handle to the Complete CB Function Obj
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Registers a Complete CB Function for Scan Complete notifications
 *
 * \sa
 */ 
TI_STATUS TWD_RegisterScanCompleteCb (TI_HANDLE hTWD, 
									  TScanSrvCompleteCb fScanCompleteCb, 
									  TI_HANDLE hScanCompleteCb); 
/** @ingroup Misc
 * \brief  Set Parameters in FW
 * 
 * \param hTWD 			- TWD module object handle
 * \param  pParamInfo   - Pointer to Input TWD Parameters Information Structure
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *  Set/Configure Parameters Information in FW via Mail BOX
 * 
 * \sa	TTwdParamInfo
 */ 
TI_STATUS TWD_SetParam (TI_HANDLE hTWD, TTwdParamInfo *pParamInfo);
/** @ingroup Misc
 * \brief  Get Parameters from FW
 * 
 * \param hTWD 			- TWD module object handle
 * \param  pParamInfo   - Pointer to Input TWD Parameters Information Structure
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *  Get Parameters Information from FW  via Mail BOX
 * 
 * \sa
 */ 
TI_STATUS TWD_GetParam (TI_HANDLE hTWD, TTwdParamInfo *pParamInfo);
/** @ingroup Control
 * \brief Callback which Checks MBOX
 * 
 * \param  hTWD         - TWD module object handle
 * \param  uMboxStatus  - Mailbox status
 * \param  pItrParamBuf - Pointer to Interrogate parameters buffer
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Perform FW validation by calling CB function which is used for handling MBOX error.
 * If command MBOX queue identify MBOX error or timeout, it will call 
 * a failure-callback with MBOX_FAILURE type (FW failed)
 * 
 * \sa
 */ 
TI_STATUS TWD_CheckMailboxCb (TI_HANDLE hTWD, TI_UINT16 uMboxStatus, void *pItrParamBuf);
/** @ingroup Control
 * \brief Write MIB
 * 
 * \param hTWD 		- TWD module object handle
 * \param pMib      - Pointer to Input MIB Structure
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Write configuration information to FW
 *
 * \sa TWD_ReadMib
 */ 
TI_STATUS   TWD_WriteMib (TI_HANDLE hTWD, TMib* pMib);
/** @ingroup Control
 * \brief Read MIB
 * 
 * \param hTWD 			- TWD module object handle
 * \param  hCb          - Handle to Request MIB CB Function Obj
 * \param  fCb          - Pinter to Request MIB CB Function
 * \param  pCb          - Pointer to read parameters
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Read configuration information from FW
 *
 * \sa TWD_WriteMib
 */ 
TI_STATUS   TWD_ReadMib                 (TI_HANDLE hTWD, TI_HANDLE hCb, void* fCb, void* pCb);
/** @ingroup Control
 * \brief TWD Debug
 * 
 * \param hTWD 			- TWD module object handle
 * \param  funcType    	- TWD Function Debuged
 * \param  pParam     	- Pointer to Input parameters of Debug function
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Use for Debug TWD
 *
 * \sa
 */ 
TI_STATUS TWD_Debug (TI_HANDLE hTWD, TI_UINT32 funcType, void *pParam);
/** @ingroup Control
 * \brief Register event
 * 
 * \param  hTWD         - TWD module object handle
 * \param  event        - Event ID
 * \param  fCb          - Event Callback function pointer
 * \param  hCb          - Event Callback object handle
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Register FW event callback function
 * 
 * \sa
 */ 
TI_STATUS TWD_RegisterEvent (TI_HANDLE hTWD, TI_UINT32 event, void *fCb, TI_HANDLE hCb);
/** @ingroup Control
 * \brief Disable event
 * 
 * \param  hTWD         - TWD module object handle
 * \param  event        - Event ID
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Disable specific FW event
 * Note: Currently not in use
 * 
 * \sa
 */ 
TI_STATUS TWD_DisableEvent (TI_HANDLE hTWD, TI_UINT32 event);
/** @ingroup Control
 * \brief Enable event
 * 
 * \param  hTWD         - TWD module object handle
 * \param  event        - Event ID
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Enable specific FW event
 * 
 * \sa
 */ 
TI_STATUS TWD_EnableEvent (TI_HANDLE hTWD, TI_UINT32 event);
/** @ingroup Control
 * \brief Convert RSSI to RX Level
 * 
 * \param hTWD 			- TWD module object handle
 * \param  iRssiVal     - RSSI Input Value
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Note: Currently not in use!
 *
 * \sa
 */ 
TI_INT8 TWD_ConvertRSSIToRxLevel (TI_HANDLE hTWD, TI_INT32 iRssiVal);
/** @ingroup Control
 * \brief Complete TWD Stop
 * 
 * \param  hTWD	- TWD module object handle
 * \return void
 * 
 * \par Description
 * 
 * \sa TWD_Stop, TWD_Init
 */ 
void TWD_StopComplete (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Disable Interrupts
 * 
 * \param hTWD	- TWD module object handle
 * \return void 
 * 
 * \par Description
 * Disable the FW Event client of the context thread handler
 *
 * \sa
 */ 
void TWD_DisableInterrupts (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief Enable Interrupts
 * 
 * \param hTWD 		- TWD module object handle
 * \return void 
 * 
 * \par Description
 * Enable the FW Event client of the context thread handler
 *
 * \sa
 */ 
void TWD_EnableInterrupts (TI_HANDLE hTWD);
/** @ingroup Control
 * \brief	Translate host to FW time (Usec)
 * 
 * \param  hTWD 	 - TWD module object handle
 * \param  uHostTime - The host time in MS to translate
 *
 * \return FW Time in Usec
 * 
 * \par Description
 * 
 * \sa
 */
TI_UINT32 TWD_TranslateToFwTime (TI_HANDLE hTWD, TI_UINT32 uHostTime);
/** @ingroup BSS
 * \brief Get TWD HT Capabilities
 * 
 * \param hTWD 					- TWD module object handle
 * \param  pTwdHtCapabilities  	- Pointer read structure Output
 * \return TI_OK
 * 
 * \par Description
 * 
 *
 * \sa
 */ 
void TWD_GetTwdHtCapabilities (TI_HANDLE hTWD, TTwdHtCapabilities **pTwdHtCapabilities);
#ifdef TI_DBG
/** @ingroup Measurement
 * \brief Prints Scan Server Debug status
 * 
 * \param  hTWD         - TWD module object handle
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_STATUS TWD_PrintMacServDebugStatus (TI_HANDLE hTWD);

/** @ingroup Test
 * \brief Prints Tx Info
 * 
 * \param  hTWD         - TWD module object handle
 * \param  ePrintInfo   - Information type
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Call the requested print function - used for Debug Test
 * 
 * \sa 
 */ 
TI_STATUS TWD_PrintTxInfo (TI_HANDLE hTWD, ETwdPrintInfoType ePrintInfo);
#endif

/*-----*/
/* Get */
/*-----*/

/** @ingroup Control
 * \brief Get number of Commands in CMD Queue 
 * 
 * \param  hTWD         - TWD module object handle
 * \return Maximum Number of Commands currently in CMD Queue 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_UINT32 TWD_GetMaxNumberOfCommandsInQueue (TI_HANDLE hTWD);
/** @ingroup Power_Management
 * \brief Get Power Save Status 
 * 
 * \param  hTWD         		- TWD module object handle
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_BOOL TWD_GetPsStatus (TI_HANDLE hTWD);


/** @ingroup Control
 * \brief  Get FW Information
 * 
 * \param  hTWD    	- TWD module object handle
 * \return TFwInfo 	- Pointer to Output FW Information Structure
 * 
 * \par Description
 * Gets the TFwInfo pointer
 * 
 * \sa TFwInfo
 */ 
TFwInfo* TWD_GetFWInfo (TI_HANDLE hTWD);
/** @ingroup BSS
 * \brief	Get Group Address Table
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  pEnabled  		- Pointer to Output Indicatore if MAC Address Filter is Enabled
 * \param  pNumGroupAddrs   - Pointer to Output Number of Group Address
 * \param  pGroupAddr   	- Pointer to Output Group Address Table
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa TWD_WriteMib, TMacAddr
 */
TI_STATUS TWD_GetGroupAddressTable (TI_HANDLE hTWD, TI_UINT8* pEnabled, TI_UINT8* pNumGroupAddrs, TMacAddr *pGroupAddr);
/** @ingroup Control
 * \brief Read Memory
 * 
 * \param hTWD 			- TWD module object handle
 * \param pMemDebug     - Pointer to read Output
 * \param fCb			- Pointer to function Callback
 * \param hCb			- Handle to function Callback Parameters
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_STATUS TWD_readMem (TI_HANDLE hTWD, TFwDebugParams* pMemDebug, void* fCb, TI_HANDLE hCb);
/** @ingroup Control
 * \brief Write Memory
 * 
 * \param hTWD 			- TWD module object handle
 * \param pMemDebug     - Pointer to write Input
 * \param fCb			- Pointer to function Callback
 * \param hCb			- Handle to function Callback Parameters
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_STATUS TWD_writeMem (TI_HANDLE hTWD, TFwDebugParams* pMemDebug, void* fCb, TI_HANDLE hCb);

/** @ingroup Control
 * \brief Check if addr is a valid memory address
 * 
 * \param hTWD 			- TWD module object handle
 * \param pMemDebug     - Pointer to addr & length
 * \return TI_TRUE on success or TI_FALSE on failure 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_BOOL TWD_isValidMemoryAddr (TI_HANDLE hTWD, TFwDebugParams* pMemDebug);

/** @ingroup Control
 * \brief Check if addr is a valid register address
 * 
 * \param hTWD 			- TWD module object handle
 * \param pMemDebug     - Pointer to addr & length
 * \return TI_TRUE on success or TI_FALSE on failure 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_BOOL TWD_isValidRegAddr (TI_HANDLE hTWD, TFwDebugParams* pMemDebug);

/*-----*/
/* Set */
/*-----*/

/** @ingroup Power_Management
 * \brief Set Power Save Mode 
 * 
 * \param  hTWD         		- TWD module object handle
 * \param  ePsMode       		- Power Save Mode
 * \param  bSendNullDataOnExit  - Indicates whether to send NULL data when exiting driver mode
 * \param  hPowerSaveCompleteCb - Handle to PS Complete CB Parameters Obj 
 * \param  fPowerSaveCompleteCb - The PS Complete CB function
 * \param  fPowerSaveResponseCb - The PS Response CB function
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_STATUS TWD_SetPsMode (TI_HANDLE hTWD, 
						 E80211PsMode ePsMode, 
						 TI_BOOL bSendNullDataOnExit, 
						 TI_HANDLE hPowerSaveCompleteCb, 
						 TPowerSaveCompleteCb fPowerSaveCompleteCb, 
						 TPowerSaveResponseCb fPowerSaveResponseCb);
/** @ingroup Radio
 * \brief Set Rate Modulation 
 * 
 * \param  hTWD         - TWD module object handle
 * \param  rate         - Rate Modulation Value
 * \return TRUE if Power Service State is Pwer Save, FALSE otherwise 
 * 
 * \par Description
 *
 * \sa
 */ 
TI_STATUS TWD_SetNullRateModulation (TI_HANDLE hTWD, TI_UINT16 rate);
/** @ingroup Radio
 * \brief	Set Radio Band
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  eRadioBand  		- Radio Band Type
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS TWD_SetRadioBand (TI_HANDLE hTWD, ERadioBand eRadioBand);
/** @ingroup Data_Path
 * \brief	Set Security Sequance Number
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  securitySeqNumLsByte - LS Byte of Security Sequance Number
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Update the TKIP/AES sequence-number according to the Tx data packet security-sequance-number
 * Note: The FW always provides the last used sequance-number so no need to check if the current 
 * packet is data and WEP is on
 *
 * \sa
 */
TI_STATUS TWD_SetSecuritySeqNum (TI_HANDLE hTWD, TI_UINT8 securitySeqNumLsByte);
/** @ingroup BSS
 * \brief Update DTIM & TBTT 
 * 
 * \param  hTWD         	- TWD module object handle
 * \param  uDtimPeriod     	- DTIM period in number of beacons
 * \param  uBeaconInterval 	- Beacon perios in TUs (1024 msec)
 * \return void 
 * 
 * \par Description
 * Update DTIM and Beacon periods for scan timeout calculations
 *
 * \sa
 */ 
void TWD_UpdateDtimTbtt (TI_HANDLE hTWD, TI_UINT8 uDtimPeriod, TI_UINT16 uBeaconInterval);

/*---------*/
/* Command */
/*---------*/


/** @ingroup Measurement
 * \brief  Set Split scan time out
 * 
 * \param hTWD 			- TWD module object handle
 * \param  uTimeOut   	- Scan Time Out
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Set Triggered scan time out per channel
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdSetSplitScanTimeOut (TI_HANDLE hTWD, TI_UINT32 uTimeOut);
/** @ingroup BSS
 * \brief  Join BSS
 * 
 * \param hTWD 				- TWD module object handle
 * \param  pJoinBssParams   - Pointer to Join BSS Input parameters structure
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdJoinBss (TI_HANDLE hTWD, TJoinBss *pJoinBssParams);
/** @ingroup Control
 * \brief  Command Template
 * 
 * \param hTWD 				- TWD module object handle
 * \param  pTemplateParams  - Pointer to Input Template Parameters Structure
 * \param  fCb  			- Pointer to Command Callback Function
 * \param  hCb  			- Handle to Command Callback Function Obj. Parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Sets a template to the FW
 * 
 * \sa
 */ 
/* 6.1.08 - for future WHA measurement command */
TI_STATUS TWD_CmdTemplate (TI_HANDLE hTWD, TSetTemplate *pTemplateParams, void *fCb, TI_HANDLE hCb);
/** @ingroup Data_Path
 * \brief  Enable Tx path
 * 
 * \param  hTWD     	- TWD module object handle
 * \param  channel     	- Channel Number
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Enable tx path on the hardware
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdEnableTx (TI_HANDLE hTWD, TI_UINT8 channel);
/** @ingroup Data_Path
 * \brief  Disable Tx path
 * 
 * \param  hTWD     	- TWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdDisableTx (TI_HANDLE hTWD);
/** @ingroup Measurement
 * \brief  Command Noise Histogram
 * 
 * \param  hTWD     		- TWD module object handle
 * \param  pNoiseHistParams - Pointer Input Noise Histogram Parameters: 
 * 							  holds Start/Stop Noise Histogram Measure Indication, 
 * 							  Sample Interval & Sample Ranges 
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Send a Start/Stop Noise Histogram Measure Command to the FW with measure parameters
 * 
 * \sa	TNoiseHistogram
 */ 
TI_STATUS TWD_CmdNoiseHistogram (TI_HANDLE hTWD, TNoiseHistogram *pNoiseHistParams);

/** @ingroup Radio
 * \brief  Command Switch Channel
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  pSwitchChannelCmd    - Pointer to Switch Channel Parameters Structure
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa TSwitchChannelParams
 */ 
TI_STATUS TWD_CmdSwitchChannel (TI_HANDLE hTWD, TSwitchChannelParams *pSwitchChannelCmd);
/** @ingroup Radio
 * \brief  Command Switch Channel Cancel
 * 
 * \param  hTWD    		- TWD module object handle
 * \param  channel    	- Channek Number
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdSwitchChannelCancel (TI_HANDLE hTWD, TI_UINT8 channel);
/** @ingroup Control
 * \brief  FW Disconnect
 * 
 * \param  hTWD    	- TWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa 
 */ 
TI_STATUS TWD_CmdFwDisconnect (TI_HANDLE hTWD, DisconnectType_e uDisconType, TI_UINT16 uDisconReason);
/** @ingroup Measurement
 * \brief  Start Measurement Command
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  uDisconReason		- 2 bytes of disconnect reason to be use in deauth/disassoc frmaes
 * \param  uDisconType    		- Immediate (dont send frames) or send Deauth or send Disassoc frmae
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * send Command for start measurement to FW
 * 
 * \sa TMeasurementParams
 */ 
TI_STATUS TWD_CmdMeasurement (TI_HANDLE hTWD, 
							  TMeasurementParams *pMeasurementParams, 
							  void *fCommandResponseCb, 
							  TI_HANDLE hCb);
/** @ingroup Measurement
 * \brief  Stop Measurement Command
 * 
 * \param  hTWD    	- TWD module object handle
 * \param  fCb  	- Pointer to Callback Function
 * \param  hCb    	- Handle to Callback Function Object Parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * send Command for stop measurement to FW
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdMeasurementStop (TI_HANDLE hTWD, void* fCb, TI_HANDLE hCb);
/** @ingroup UnKnown
 * \brief  AP Discovery
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  pApDiscoveryParams  	- Pointer to Input AP Discovery Parameters Structure
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdApDiscovery (TI_HANDLE hTWD, TApDiscoveryParams *pApDiscoveryParams);
/** @ingroup UnKnown
 * \brief	AP Discovery Stop
 * 
 * \param  hTWD    				- TWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdApDiscoveryStop (TI_HANDLE hTWD);

/** @ingroup Control
 * \brief	Helth Check
 * 
 * \param  hTWD    			- TWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Trigger the FW health test command and wait for results
 *
 * \sa
 */
TI_STATUS TWD_CmdHealthCheck (TI_HANDLE hTWD);
/** @ingroup UnKnown
 * \brief  AP Discovery
 * 
 * \param  hTWD    		- TWD module object handle
 * \param  staState  	- stat of the station (CONNECTED)
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CmdSetStaState (TI_HANDLE hTWD, TI_UINT8 staState, void *fCb, TI_HANDLE hCb);

/*-----------*/
/* Configure */
/*-----------*/

/** @ingroup UnKnown
 * \brief  Configure ARP table
 * 
 * \param hTWD 			- TWD module object handle
 * \param  tIpAddr   	- IP Address Input Buffer
 * \param  bEnabled   	- Indicates if ARP filtering is Enabled (1) or Disabled (0)
 * \param  eIpVer   	- IP Version
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure ARP IP Address table
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgArpIpAddrTable (TI_HANDLE hTWD, 
								 TIpAddr tIpAddr, 
								 EArpFilterType filterType, 
								 EIpVer eIpVer);

TI_STATUS TWD_CfgArpIpFilter    (TI_HANDLE hTWD, 
                                 TIpAddr tIpAddr);

/** @ingroup BSS
 * \brief	Configure Group Address Table
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  uNumGroupAddrs  	- Number of Group Address
 * \param  pGroupAddr   	- Pointer to Input Group Address Table
 * \param  bEnabled    		- Indicates if MAC Address Filter is Enabled
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa TWD_WriteMib
 */
TI_STATUS TWD_CfgGroupAddressTable (TI_HANDLE hTWD, 
									TI_UINT8 uNumGroupAddrs, 
									TMacAddr *pGroupAddr, 
									TI_BOOL bEnabled);
/** @ingroup Data_Path
 * \brief  Configure RX Filters
 * 
 * \param hTWD 				- TWD module object handle
 * \param  uRxConfigOption  - RX Configuration Option
 * \param  uRxFilterOption 	- RX Filter Option
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa	TWD_WriteMib
 */ 
TI_STATUS TWD_CfgRx (TI_HANDLE hTWD, TI_UINT32 uRxConfigOption, TI_UINT32 uRxFilterOption);
/** @ingroup UnKnown
 * \brief  Configure Packet Detection Threshold
 * 
 * \param hTWD 			- TWD module object handle
 * \param  threshold 	- Threshold Value
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa ERate
 */ 
TI_STATUS TWD_CfgPacketDetectionThreshold (TI_HANDLE hTWD, TI_UINT32 threshold);
/** @ingroup Radio
 * \brief  Configure Slot Time
 * 
 * \param hTWD 				- TWD module object handle
 * \param  eSlotTimeVal 	- Slot Time Value
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgSlotTime (TI_HANDLE hTWD, ESlotTime eSlotTimeVal);
/** @ingroup Radio
 * \brief  Configure Preamble
 * 
 * \param hTWD 			- TWD module object handle
 * \param  ePreamble 	- Preamble Value
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgPreamble (TI_HANDLE hTWD, EPreamble ePreamble);
/** @ingroup Power_Management
 * \brief  Configure Beacon Filter State
 * 
 * \param  hTWD     				- TWD module object handle
 * \param  uBeaconFilteringStatus   - Beacon Filtering Status. Indicates whether the filter is enabled:
 * 									  1 - enabled, 0 - disabled
 * \param  uNumOfBeaconsToBuffer 	- Determines the number of beacons without the unicast TIM bit set,
 * 									  that the firmware buffers before signaling the host about ready frames.
 *									  When thi snumber is set to 0 and the filter is enabled, beacons without 
 *									  the unicast TIM bit set are dropped. 
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure Beacon Filter State to the FW
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgBeaconFilterOpt (TI_HANDLE hTWD, TI_UINT8 uBeaconFilteringStatus, TI_UINT8 uNumOfBeaconsToBuffer);
/** @ingroup Power_Management
 * \brief  Configure Beacon Filter Table
 * 
 * \param  hTWD     	- TWD module object handle
 * \param  uNumOfIe   	- The number of IE's in the table
 * \param  pIeTable 	- Pointer to Input IE Table
 * \param  uIeTableSize - Size of Input IE Table
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure Beacon Filter Table to the FW
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgBeaconFilterTable (TI_HANDLE hTWD, TI_UINT8 uNumOfIe, TI_UINT8 *pIeTable, TI_UINT8 uIeTableSize);
/** @ingroup Power_Management
 * \brief  Configure Wake Up Condition
 * 
 * \param  hTWD     		- TWD module object handle
 * \param  pPowerMgmtConfig	- Pointer to Input Power Management Configuration Parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure Power Manager's Wake Up Condition
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgWakeUpCondition (TI_HANDLE hTWD, TPowerMgmtConfig *pPowerMgmtConfig);
/** @ingroup UnKnown
 * \brief  Configure Beacon Broadcast Options
 * 
 * \param  hTWD     		- TWD module object handle
 * \param  pPowerMgmtConfig	- Pointer to Input Power Management Configuration Parameters Structure
 * 							  The Following Field should be filled: 
 * 							  pPowerMgmtConfig->BcnBrcOptions.BeaconRxTimeout
 * 							  pPowerMgmtConfig->BcnBrcOptions.BroadcastRxTimeout
 * 							  pPowerMgmtConfig->BcnBrcOptions.RxBroadcastInPs - if set, enables receive of broadcast packets in Power-Save mode
 * 							  pPowerMgmtConfig->ConsecutivePsPollDeliveryFailureThreshold - No answer after Ps-Poll work-around 					   	  
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure Power Manager's Beacon Broadcast Options:
 * Beacon RX time Out, Broadcast RX Timeout, RX Broadcast In Power Save, 
 * Consecutive Power Save Poll Delivery Failure Threshold
 * 
 * 
 * \sa TPowerMgmtConfig, TBcnBrcOptions
 */ 
TI_STATUS TWD_CfgBcnBrcOptions (TI_HANDLE hTWD, TPowerMgmtConfig *pPowerMgmtConfig);

/** @ingroup BSS
 * \brief  Configure Max TX Retry
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  pRoamingTriggerCmd   - Pointer to Input Configuration Parameters Structure
 * 							  	  The Following Field should be filled: 
 * 								  pRoamingTriggerCmd->maxTxRetry
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure the Max Tx Retry parameters
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgMaxTxRetry (TI_HANDLE hTWD, TRroamingTriggerParams *pRoamingTriggerCmd);
/** @ingroup BSS
 * \brief  Configure Connection Monitoring
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  pRoamingTriggerCmd   - Pointer to Input Configuration Parameters Structure
 * 							  	  The Following Field should be filled: 
 * 								  pRoamingTriggerCmd->BssLossTimeout
 * 								  pRoamingTriggerCmd->TsfMissThreshold
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure the Bss Lost Timeout & TSF miss threshold Parameters
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgConnMonitParams (TI_HANDLE hTWD, TRroamingTriggerParams *pRoamingTriggerCmd);
/** @ingroup Power_Management
 * \brief	Configure Sleep Auth
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  eMinPowerPolicy  - Minimum Power Policy Type
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure the minimum power policy to the FW
 * 
 * \sa EPowerPolicy
 */
TI_STATUS TWD_CfgSleepAuth (TI_HANDLE hTWD, EPowerPolicy eMinPowerPolicy);
/** @ingroup Control
 * \brief	Configure MAC Clock
 * 
 * \param  hTWD    		- TWD module object handle
 * \param  uMacClock    - MAC Clock value
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_CfgMacClock (TI_HANDLE hTWD, TI_UINT32 uMacClock);
/** @ingroup Control
 * \brief	Configure ARM Clock
 * 
 * \param  hTWD    		- TWD module object handle
 * \param  uArmClock    - ARM Clock value
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_CfgArmClock (TI_HANDLE hTWD, TI_UINT32 uArmClock);
/** @ingroup Data_Path
 * \brief	Configure RX Data Filter
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  index    			- Index of the Rx Data filter
 * \param  command    			- Command: Add/remove the filter
 * \param  eAction    			- Action to take when packets match the pattern
 * \param  uNumFieldPatterns   	- Number of field patterns in the filter
 * \param  uLenFieldPatterns    - Length of the field pattern series
 * \param  pFieldPatterns    	- Series of field patterns
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Add/remove Rx Data filter information element
 *
 * \sa
 */
TI_STATUS TWD_CfgRxDataFilter (TI_HANDLE hTWD, 
							   TI_UINT8 index, 
							   TI_UINT8 command, 
							   filter_e eAction, 
							   TI_UINT8 uNumFieldPatterns, 
							   TI_UINT8 uLenFieldPatterns, 
							   TI_UINT8 *pFieldPatterns);
/** @ingroup Data_Path
 * \brief	Configure Enable RX Data Filter
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  bEnabled      	- Indicates if Rx data filtering is enabled or Disabled
 * 							  (0: data filtering disabled, Otherwise: enabled)
 * \param  eDefaultAction   - The default action taken on non-matching packets
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure Enable/Disable RX Data Filter, and which default action to perform if it is enabled
 *
 * \sa
 */
TI_STATUS TWD_CfgEnableRxDataFilter (TI_HANDLE hTWD, TI_BOOL bEnabled, filter_e eDefaultAction);
/** @ingroup BSS
 * \brief	Configure RRSSI/SNR Trigger parameters
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  pRssiSnrTrigger  - Pointer to RRSSI/SNR Input parameter Structure
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_CfgRssiSnrTrigger (TI_HANDLE hTWD, RssiSnrTriggerCfg_t* pRssiSnrTrigger);
/** @ingroup QoS
 * \brief	Configure AC parameters
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  pAcQosParams  	- Pointer to Input AC Quality Of Service Parameters Structure
 * 								Fields that should be filled:
 * 								pAcQosParams->ac
 * 								pAcQosParams->aifsn
 * 								pAcQosParams->cwMax
 * 								pAcQosParams->cwMin
 * 								pAcQosParams->txopLimit
 * \param  fCb      		- Pointer to Command CB Function
 * \param  hCb      		- Handle to Command CB Function Obj Parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_CfgAcParams (TI_HANDLE hTWD, TAcQosParams *pAcQosParams, void *fCb, TI_HANDLE hCb);
/** @ingroup QoS
 * \brief	Configure Power Save RX Streaming
 * 
 * \param  hTWD    			- TWD module object handle
 * \param  pPsRxStreaming  	- Pointer to Input Power Save RX Straeming Parameters Structure
 * 								Fields that should be filled:
 * 								pPsRxStreaming->uTid
 * 								pPsRxStreaming->bEnabled
 * 								pPsRxStreaming->uStreamPeriod
 * 								pPsRxStreaming->uTxTimeout
 * \param  fCb      		- Pointer to Command CB Function
 * \param  hCb      		- Handle to Command CB Function Obj Parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa TPsRxStreaming
 */
TI_STATUS TWD_CfgPsRxStreaming (TI_HANDLE hTWD, TPsRxStreaming *pPsRxStreaming, void *fCb, TI_HANDLE hCb);
/** @ingroup Power_Management
 * \brief	Configure BET
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  Enable    			- 0: disable BET, Otherwirs: Enable BET
 * \param  MaximumConsecutiveET - Max number of consecutive beacons that may be early terminated
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_CfgBet (TI_HANDLE hTWD, TI_UINT8 Enable, TI_UINT8 MaximumConsecutiveET);
/** @ingroup UnKnown
 * \brief  Configure Keep Alive
 * 
 * \param hTWD 				- TWD module object handle
 * \param  pKeepAliveParams - Pointer to Keep Alive parameters structure
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configures the keep-alive paramters
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgKeepAlive (TI_HANDLE hTWD, TKeepAliveParams *pKeepAliveParams);
/** @ingroup Power_Management
 * \brief  Configure Keep Alive Enable/Disable flag
 * 
 * \param hTWD 			- TWD module object handle
 * \param  enaDisFlag  	- Indicates whether to Enable (TI_TRUE) or Disable Keep Alive
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgKeepAliveEnaDis (TI_HANDLE hTWD, TI_UINT8 enaDisFlag);
/** @ingroup Data_Path
 * \brief	Configure Set BA Initiator
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  uTid 				- TID number
 * \param  uState 				- Policy : Enable / Disable 
 * \param  tRa 					- Mac address of: SA as receiver / RA as initiator
 * \param  uWinSize 			- windows size in number of packet
 * \param  uInactivityTimeout 	- as initiator inactivity timeout in time units(TU) of 1024us / as receiver reserved
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * configure BA session initiator parameters setting in the FW
 * called after join in order to init the parameters for negotiating BA sessions as initiator.
 * Parameters initialized: RA, TID, WinSize, Inactivity Timeout and state = Enable/Disable.
 * In case the host sends a broadcast address as RA the FW is allowed to Set or Deleted BA sessions 
 * to any receiver for that TID.
 * In case of disassociate the FW allowed to establish BA session just after get that command.
 * That command will not need any respond from the FW. In case DELBA send to STA or from the 
 * STA as initiator the FW doesn't send event to the host
 * 
 * \sa
 */
TI_STATUS TWD_CfgSetBaInitiator (TI_HANDLE hTWD, 
								 TI_UINT8 uTid, 
								 TI_UINT8 uState, 
								 TMacAddr tRa, 
								 TI_UINT16 uWinSize, 
								 TI_UINT16 uInactivityTimeout);
/** @ingroup Data_Path
 * \brief	Configure Set BA Receiver
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  uTid 				- TID number
 * \param  uState 				- Policy : Enable / Disable 
 * \param  tRa 					- Mac address of: SA as receiver / RA as initiator
 * \param  uWinSize 			- windows size in number of packet
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * configure BA session receiver parameters setting in the FW
 * called after join in order to init the parameters for incoming BA session, as a responder.
 * Parameters initialized: SA, TID, winSize and state = Enable/Disable.
 * In case the host sends a broadcast address as SA the FW is allowed to Set/Deleted BA sessions 
 * to any sender for that TID.
 * In case of disassociate the FW allowed to establish BA session just after get that command.
 * The events of that command will respond via the RX path from the FW: ADDBA, DELBA, BAR packets. 
 * 
 * \sa
 */
TI_STATUS TWD_CfgSetBaReceiver (TI_HANDLE hTWD, 
								TI_UINT8 uTid, 
								TI_UINT8 uState, 
								TMacAddr tRa, 
								TI_UINT16 uWinSize);

/** @ingroup Data_Path
 * \brief	Close all BA receiver sessions
 * 
 * \param  hTWD    				- TWD module object handle
 * \return None
 * 
 * \par Description
 * Close all BA receiver sessions and pass all packets in the TID queue to upper layer. 
 * 
 * \sa
 */
void TWD_CloseAllBaSessions(TI_HANDLE hTWD); 

/** @ingroup BSS
 * \brief	Set FW HT Capabilities
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  pHtCapabilitiesIe 	- Pointer to string of HT capability IE unparsed
 * \param  bAllowHtOperation 	- TI_TRUE: HT operation allowed, Otherwise: HT operation NOT allowed
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Parse HT capability IE and set the current AP HT Capabilities to the FW
 * 
 * \sa
 */
TI_STATUS TWD_CfgSetFwHtCapabilities (TI_HANDLE hTWD, 
									  Tdot11HtCapabilitiesUnparse *pHtCapabilitiesIe, 
									  TI_BOOL bAllowHtOperation);
/** @ingroup BSS
 * \brief Set FW HT Information
 * 
 * \param  hTWD    				- TWD module object handle
 * \param  pHtInformationIe 	- Pointer to string of HT information IE unparsed
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Parse HT Information IE and set the current AP HT Information to the FW
 * 
 * \sa
 */
TI_STATUS TWD_CfgSetFwHtInformation (TI_HANDLE hTWD, Tdot11HtInformationUnparse *pHtInformationIe);


/** @ingroup UnKnown
 * \brief Enable/Disabel burst mode
 *
 * \param  hTWD    				- TWD module object handle
 * \param  bEnabled 	        - burst mode: Enable/Disable
 * \return TI_OK
 *
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_CfgBurstMode (TI_HANDLE hTWD, TI_BOOL bEnabled);

/*-------------*/
/* Interrogate */
/*-------------*/

/** @ingroup UnKnown
 * \brief  Interrogate Roamming Statistics
 * 
 * \param  hTWD     	- TWD module object handle
 * \param  fCb          - Pointer to Command CB Function
 * \param  hCb          - Handle to Command CB Function Obj Parameters
 * \param  pCb          - Pointer to read parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Interrogate ACX Roamming Statistics
 * 
 * \sa
 */ 
TI_STATUS TWD_ItrRoammingStatisitics (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb);
/** @ingroup UnKnown
 * \brief  Configure/Interrogate RSSI
 * 
 * \param  hTWD    	- TWD module object handle
 * \param  fCb      - Pointer to Command CB Function
 * \param  hCb      - Handle to Command CB Function Obj Parameters
 * \param  pCb      - Pointer to read parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Configure/Interrogate StationId information element to/from FW
 * This information element specifies the MAC Address assigned to the STATION or AP.
 * The RSSI is Configed to default value which is the permanent MAC address which 
 * is stored in the adaptor's non-volatile memory.
 * 
 * \sa 
 */ 
TI_STATUS TWD_ItrRSSI (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb);
/** @ingroup UnKnown
 * \brief	Interrogate Memory Map
 * 
 * \param  hTWD    	- TWD module object handle
 * \param  pMap    	- Pointer to Output Memory Map
 * \param  fCb    	- Pointer to Callback Function
 * \param  hCb    	- Handle to Callback Function Parameters Object 
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Interrogate Memory Map from FW
 *
 * \sa MemoryMap_t
 */TI_STATUS TWD_ItrMemoryMap (TI_HANDLE hTWD, MemoryMap_t *pMap, void *fCb, TI_HANDLE hCb);
/** @ingroup UnKnown
 * \brief	Interrogate Statistics
 * 
 * \param  hTWD    	- TWD module object handle
 * \param  fCb      - Pointer to Command CB Function
 * \param  hCb      - Handle to Command CB Function Obj Parameters
 * \param  pCb      - Pointer to read parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS TWD_ItrStatistics (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb);
/** @ingroup Data_Path
 * \brief	Interrogate Data Filter Statistics
 * 
 * \param  hTWD    	- TWD module object handle
 * \param  fCb      - Pointer to Command CB Function
 * \param  hCb      - Handle to Command CB Function Obj Parameters
 * \param  pCb      - Pointer to read parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 *
 * \sa
 */TI_STATUS TWD_ItrDataFilterStatistics (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void *pCb);

/*
 * --------------------------------------------------------------
 *	TNETW-Driver  Tx  API  Functions
 * --------------------------------------------------------------
 */

/** @ingroup Data_Path
 * \brief  TWD TX Control Block Allocation
 * 
 * \param  hTWD   	- TWD module object handle
 * \return Pointer to Control Block Entry on success or NULL on failure
 * 
 * \par Description
 * Use this function for Allocate a Control-Block for the packet Tx parameters and descriptor
 * 
 * \sa
 */ 
TTxCtrlBlk *TWD_txCtrlBlk_Alloc (TI_HANDLE hTWD);
/** @ingroup Data_Path
 * \brief  TWD TX Control Block Free
 * 
 * \param  hTWD   			- TWD module object handle
 * \param  pCurrentEntry   	- Pointer to TX Control Block Entry to Free
 * \return void
 * 
 * \par Description
 * Use this function for Free a Control-Block of packet Tx parameters and descriptor
 * 
 * \sa
 */ 
void TWD_txCtrlBlk_Free (TI_HANDLE hTWD, TTxCtrlBlk *pCurrentEntry);
/** @ingroup Data_Path
 * \brief  TWD TX Control Get Pointer
 * 
 * \param  hTWD   	- TWD module object handle
 * \param  descId  	- Id of TX Control Block Descriptor
 * \return Pointer to Control Block Entry on success or NULL on failure
 * 
 * \par Description
 * Use this function for Get a Pointer to a Control-Block of packet Tx parameters and descriptor
 * 
 * \sa
 */ 
TTxCtrlBlk *TWD_txCtrlBlk_GetPointer (TI_HANDLE hTWD, TI_UINT8 descId);

/** @ingroup Data_Path
 * \brief  Allocate Resources for TX HW Queue
 * 
 * \param  hTWD   			- TWD module object handle
 * \param  pTxCtrlBlk  		- The Tx packet control block
 * \return see - ETxHwQueStatus
 * 
 * \par Description
 * Allocates Resources (HW-blocks number required) for TX HW Queue 
 * 
 * \sa
 */ 
ETxHwQueStatus TWD_txHwQueue_AllocResources (TI_HANDLE hTWD, TTxCtrlBlk *pTxCtrlBlk);

/** @ingroup Data_Path
 * \brief  TX Xfer Send Packet
 * 
 * \param  hTWD   			- TWD module object handle
 * \param  pPktCtrlBlk   	- Pointer to TX Control Block Entry to Free
 * \return see ETxnStatus
 * 
 * \par Description
 * Send Packet via TX Xfer 
 * 
 * \sa
 */ 
ETxnStatus TWD_txXfer_SendPacket (TI_HANDLE hTWD, TTxCtrlBlk *pPktCtrlBlk);

/** @ingroup Data_Path
 * \brief  Indicates that current packets burst stopped
 * 
 * \param  hTWD   			- TWD module object handle
 * \return void
 * 
 * \par Description
 * Indicates that current packets burst stopped, so the TxXfer will send its aggregated packets to FW. 
 * 
 * \sa
 */ 
void TWD_txXfer_EndOfBurst (TI_HANDLE hTWD);

/** @ingroup Control
 * \brief  Watch Dog Expire Event
 * 
 * \param  hTWD   			- TWD module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * This function handles the Event of Watch Dog Expire (FW stopped)
 * 
 * \sa
 */ 
ETxnStatus TWD_WdExpireEvent (TI_HANDLE hTWD);
/*
 * --------------------------------------------------------------
 *	BIT API Functions
 * --------------------------------------------------------------
 */
/** @ingroup Control
 * \brief TWD Test Command Complete CB
 * 
 * \param  Handle        	- handle to object
 * \param  eStatus			- Status of Driver Test Performed
 * \param  pTestCmdParams  	- Pointer to Output of Test Command Parameters
 * \return void
 * 
 * \par Description
 * The function prototype for the BIT Test Command Complete CB
 * Enables user to implement and use its own BIT Test Command Complete CB 
 * which will be called when Driver Test end
 * 
 * \sa	TWDriverTest
 */ 
typedef void (*TTestCmdCB)(TI_HANDLE Handle, 
						   TI_STATUS eStatus, 
						   TI_HANDLE pTestCmdParams);
/** @ingroup Control
 * \brief TWD Test Command Complete CB
 * 
 * \param  Handle        	- handle to object
 * \param  eStatus			- Status of Driver Test Performed (Complete/Pending/Error)
 * \param  pTestCmdParams  	- Pointer to Output of Test Command Parameters
 * \return void
 * 
 * \par Description
 * The function implementation for the BIT Test Command Complete CB
 * 
 * \sa
 */ 
void TWDriverTestCB(TI_HANDLE Handle, 
					TI_STATUS eStatus, 
					TI_HANDLE pTestCmdParams);
/** @ingroup Control
 * \brief TWD Driver Test
 * 
 * \param  hTWD        		- handle to TWD object
 * \param  eTestCmd			- Identifier of test Command to Perform
 * \param  pTestCmdParams  	- Pointer to Input/Output Test Command Parameters
 * \param  fCb  			- Test Command Complete CB
 * \param  hCb	  			- Handle to Test Command Complete CB Parameters
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * The implementation of the BIT Test Command
 * 
 * \sa
 */ 
TI_STATUS TWDriverTest(TI_HANDLE hTWD, 
					   TestCmdID_enum eTestCmd, 
					   void* pTestCmdParams, 
					   TTestCmdCB fCb, 
					   TI_HANDLE hCb);



/** 
 *  \brief TWD get FEM type
 *  * 
 * \param  Handle        	- handle to object
 * \return uint8 
 * 
 * \par Description
 * The function return the Front end module that was read frm FW register * 
 * \sa
 */ 

TI_UINT8 TWD_GetFEMType (TI_HANDLE hTWD);


/** 
 *  \brief TWD end function of read radio state machine
 *  *  * 
 * \param  Handle        	- handle to object
 * \return void
 * 
 * \par Description
 * The function calling to HwInit call back function, after finish reading FEM registers * 
 * \sa
 */ 

void TWD_FinalizeFEMRead(TI_HANDLE hTWD);
void TWD_FinalizePolarityRead(TI_HANDLE hTWD);

/** @ingroup Data_Path
 * \brief  TWD_CfgBurstMode
 * 
 * \param  hTWD   	- TWD module object handle
 * \param  bEnabled  - is Burst mode enabled
 * \return TI_OK
 * 
 * \par Description
 * Use this function to enable/disbale the burst mode
 * 
 * \sa
 */ 
TI_STATUS TWD_CfgBurstMode (TI_HANDLE hTWD, TI_BOOL bEnabled);
TI_STATUS TWD_SetRateMngDebug(TI_HANDLE hTWD, RateMangeParams_t *pRateMngParams);
TI_STATUS TWD_GetRateMngDebug(TI_HANDLE hTWD, RateMangeReadParams_t  *pParamInfo);

#endif  /* TWDRIVER_H */
