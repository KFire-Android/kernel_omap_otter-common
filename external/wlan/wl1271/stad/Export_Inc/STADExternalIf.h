/*
 * STADExternalIf.h
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
/*                                                                          */
/*    MODULE:   TiWlnIf.h                                       */
/*    PURPOSE:                                                              */
/*                                                                          */
/****************************************************************************/	
#ifndef __TIWLNIF_NEW_H__
#define __TIWLNIF_NEW_H__

/** \file  STADExternalIf.h 
 *  \brief STAD External APIs
 */

#include "tidef.h"
#include "report.h"
#include "osDot11.h"
#include "TI_IPC_Api.h"
#include "bssTypes.h"
#include "roamingMngrTypes.h"
#include "version.h"
#include "privateCmd.h"
#include "CmdInterfaceCodes.h"
#include "coreDefaultParams.h"
#include "scanMngrTypes.h"
#include "TWDriver.h"

/***********/
/* defines */
/***********/

#define NUM_OF_CONFIG_PARAMS_IN_SG  	    2
#define NUM_OF_STATUS_PARAMS_IN_SG  		11
#define NUM_OF_COEX_ACTIVITY_PARAMS_IN_SG 	6
#define NUM_OF_RATE_MNGT_PARAMS_MAX			14
#define SET_SSID_WITHOUT_SUPPL      		0x8000

#define TX_RETRY_HISTOGRAM_SIZE 			16

#define RX_DATA_FILTER_MAX_MASK_SIZE        8
#define RX_DATA_FILTER_MAX_PATTERN_SIZE     64

#define KEEP_ALIVE_TEMPLATE_MAX_LENGTH      64

#define KEEP_ALIVE_MAX_USER_MESSAGES     	2


/*********************/
/* enumeration types */
/*********************/

/** \enum TxDelayRanges_e
 * \brief TX Delay Ranges
 * 
 * \par Description
 * The Tx path delay histogram (host + MAC)ranges in msec 
 * Used as indexes in tx Delay Histogram Ranges (including Start & End of ranges) Table 
 * 
 * \sa
 */
typedef enum
{
    TX_DELAY_RANGE_MIN        = 0,		/**< */

    TX_DELAY_RANGE_0_TO_1     = 0,		/**< */
    TX_DELAY_RANGE_1_TO_10    = 1,		/**< */
    TX_DELAY_RANGE_10_TO_20   = 2,		/**< */
    TX_DELAY_RANGE_20_TO_40   = 3,		/**< */
    TX_DELAY_RANGE_40_TO_60   = 4,		/**< */
    TX_DELAY_RANGE_60_TO_80   = 5,		/**< */
    TX_DELAY_RANGE_80_TO_100  = 6,		/**< */
    TX_DELAY_RANGE_100_TO_200 = 7,		/**< */
    TX_DELAY_RANGE_ABOVE_200  = 8,		/**< */

    TX_DELAY_RANGE_MAX        = 8,		/**< */
    TX_DELAY_RANGES_NUM       = 9		/**< */
} TxDelayRanges_e;

/** \enum TIWLN_SIMPLE_CONFIG_MODE
 * \brief TI WLAN Simple Configuration Mode
 * 
 * \par Description
 * Used for indicating WiFi Simple Configuration mode 
 * 
 * \sa
 */
typedef enum
{
    TIWLN_SIMPLE_CONFIG_OFF = 0,		/**< Simple Configuration OFF			*/
    TIWLN_SIMPLE_CONFIG_PIN_METHOD,		/**< Simple Configuration PIN Method	*/
    TIWLN_SIMPLE_CONFIG_PBC_METHOD		/**< Simple Configuration PBC Method	*/
} TIWLN_SIMPLE_CONFIG_MODE;

/** \enum EDraftNumber
 * \brief Draft Number
 * 
 * \par Description
 * Site Manager / Exteranl Rate use draft number
 * 
 * \sa
 */
typedef enum
{
    DRAFT_5_AND_EARLIER = 5,	/**< */ 
    DRAFT_6_AND_LATER   = 6		/**< */ 

} EDraftNumber;

/********************/
/* Structures types */
/********************/


/** \struct TTxDataCounters
 * \brief TX Data Counters
 * 
 * \par Description
 * Tx statistics per Tx-queue 
 * 
 * \sa
 */
typedef struct
{
    TI_UINT32      XmitOk;                 /**< The number of frames that were transferred to TNET without errors 						*/
    TI_UINT32      DirectedBytesXmit;      /**< The number of bytes in directed packets that are transmitted without errors 			*/
    TI_UINT32      DirectedFramesXmit;     /**< The number of directed packets that are transmitted without errors 						*/
    TI_UINT32      MulticastBytesXmit;     /**< The number of bytes in multicast/functional packets that are transmitted without errors */
    TI_UINT32      MulticastFramesXmit;    /**< The number of multicast/functional packets that are transmitted without errors			*/
    TI_UINT32      BroadcastBytesXmit;     /**< The number of bytes in broadcast packets that are transmitted without errors 			*/
    TI_UINT32      BroadcastFramesXmit;    /**< The number of broadcast packets that are transmitted without errors 					*/

    TI_UINT32      RetryHistogram[ TX_RETRY_HISTOGRAM_SIZE ];	/**< Histogram counting the number of packets xfered with any retry number	*/
                                        
    TI_UINT32      RetryFailCounter;       /**< Number of packets that failed transmission due to retry number exceeded 				*/
    TI_UINT32      TxTimeoutCounter;       /**< Number of packets that failed transmission due to lifetime expiry 						*/
    TI_UINT32      NoLinkCounter;          /**< Number of packets that failed transmission due to link failure 							*/
    TI_UINT32      OtherFailCounter;       /**< Number of packets that failed transmission due to other reasons 						*/
    TI_UINT32      MaxConsecutiveRetryFail;/**< Maximum consecutive packets that failed transmission due to retry limit exceeded 		*/

    /*  TX path delay statistics  */
    TI_UINT32      txDelayHistogram[TX_DELAY_RANGES_NUM];	/**< Histogram of Tx path delay (host + MAC) 								*/
    TI_UINT32      NumPackets;             /**< For average calculation - Total packets counted 										*/
    TI_UINT32      SumTotalDelayMs;        /**< For average calculation - the sum of packets total delay 								*/
    TI_UINT32      SumFWDelayUs;           /**< For average calculation - The sum of packets FW delay 									*/
    TI_UINT32      SumMacDelayUs;          /**< For average calculation - the sum of packets MAC delay 									*/
} TTxDataCounters;

/** \struct TIWLN_TX_STATISTICS
 * \brief TI WLAN TX Statistics
 * 
 * \par Description
 * All Tx statistics of all Tx Queues Tx-queue 
 * 
 * \sa
 */
typedef struct
{
    TTxDataCounters  txCounters[MAX_NUM_OF_AC];	/**< Table which holds Tx statistics of each Tx-queue */
} TIWLN_TX_STATISTICS;

/** \struct TDfsChannelRange
 * \brief DFS Channel Range
 * 
 * \par Description
 * Range of Dynamic Frequency Selection Channel 
 * 
 * \sa
 */
typedef struct
{
    TI_UINT16   minDFS_channelNum;	/**< Lower limit of DFS Channel Range		*/  	
    TI_UINT16   maxDFS_channelNum;	/**< Higher limit of DFS Channel Range		*/  	
} TDfsChannelRange;

/** \struct TDebugRegisterReq
 * \brief Debug Register Request
 * 
 * \par Description
 * Used for reading HW register (for debug) 
 * 
 * \sa
 */
typedef struct
{
    TI_UINT32 regSize;			/**< Register Size			*/ 	  	  
    TI_UINT32 regAddr;			/**< Register Address  		*/ 
    TI_UINT32 regValue;			/**< Register value read	*/ 
} TDebugRegisterReq;

/** \struct TIWLN_REG_RW
 * \brief TI WLAN Register R/W
 * 
 * \par Description
 * Used for writing HW register (for debug) 
 * 
 * \sa
 */
typedef struct
{
        TI_UINT32 regSize;		/**< Register Size			*/
        TI_UINT32 regAddr;		/**< Register Address  		*/ 
        TI_UINT32 regValue;		/**< Register write value	*/ 
} TIWLN_REG_RW;

/** \struct TCountry
 * \brief Country Parameters
 * 
 * \par Description
 * Parameters of Country Informatino Element
 * 
 * \sa
 */
typedef struct
{
    TI_UINT8            elementId;		/**< Country IE ID 										*/
    TI_UINT8            len;			/**< Country IE data length 							*/
    dot11_countryIE_t   countryIE;	   	/**< Country IE (country string and tripple channel)	*/ 
} TCountry;

/** \struct TRates
 * \brief Rates Parameters
 * 
 * \par Description
 * Site Manager Supported rates parameters
 * 
 * \sa
 */
typedef struct
{
    TI_UINT8       len;											/**< Number of entries in the rates list													*/
    TI_UINT8       ratesString[DOT11_MAX_SUPPORTED_RATES];		/**< Rates List. From each entry - a different bitrate (in bps) can be driven as followed: 
																((ratesString[i] & 0x7F) * 500000). Bits 1-7 are used for the bitrate and bit 8 is MASK used 
																for indicating if NET Basic
																*/
} TRates;

/** \struct TRxDataFilterRequest
 * \brief RX Data Filter Request
 * 
 * \par Description
 * Use for handling RX Data Filter (Add, Remove, parse, usage)
 * 
 * \sa
 */
typedef struct
{
    TI_UINT8       offset; 									/**< Pattern Start Offset (0-255) 										*/
    TI_UINT8       maskLength; 								/**< Byte-Mask Length, 1-8 bytes of mask, 0- match all packets 			*/
    TI_UINT8       patternLength; 							/**< Should correspond to the number of asserted bits in the Byte-Mask 	*/
    TI_UINT8       mask[RX_DATA_FILTER_MAX_MASK_SIZE]; 		/**< Byte-Mask 															*/
    TI_UINT8       pattern[RX_DATA_FILTER_MAX_PATTERN_SIZE];/**< Data Filter PAttern												*/
} TRxDataFilterRequest;

/** \struct TIWLN_COUNTERS
 * \brief TI WLAN Counters
 * 
 * \par Description
 * Use for handling RX Data Filter (Add, Remove, parse, usage)
 * 
 * \sa
 */
typedef struct 
{
    TI_UINT32  RecvOk;              /**< Number of frames that the NIC receives without errors										*/
    TI_UINT32  RecvError;           /**< Number of frames that a NIC receives but does not indicate to the protocols due to errors	*/
    TI_UINT32  RecvNoBuffer;        /**< Number of frames that the NIC cannot receive due to lack of NIC receive buffer space     	*/
    TI_UINT32  DirectedBytesRecv;   /**< Number of bytes in directed packets that are received without errors                     	*/
    TI_UINT32  DirectedFramesRecv;  /**< Number of directed packets that are received without errors                              	*/
    TI_UINT32  MulticastBytesRecv;  /**< Number of bytes in multicast/functional packets that are received without errors         	*/
    TI_UINT32  MulticastFramesRecv; /**< Number of multicast/functional packets that are received without errors                  	*/
    TI_UINT32  BroadcastBytesRecv;  /**< Number of bytes in broadcast packets that are received without errors.                   	*/
    TI_UINT32  BroadcastFramesRecv; /**< Number of broadcast packets that are received without errors.                            	*/

    TI_UINT32  FragmentsRecv;		/**< Number of Fragments Received 															  	*/
    TI_UINT32  FrameDuplicates;		/**< Number of Farme Duplicates						  											*/
    TI_UINT32  FcsErrors;			/**< Number of frames that a NIC receives but does not indicate to the protocols due to errors	*/

    TI_UINT32  BeaconsXmit;			/**< Number of Beacons Sent																		*/
    TI_UINT32  BeaconsRecv;			/**< Number of Beacons Reveived																	*/
    TI_UINT32  AssocRejects;		/**< Number of Rejected Assoc.			  		  												*/
    TI_UINT32  AssocTimeouts;		/**< Number of Assoc. Time Outs																	*/
    TI_UINT32  AuthRejects;			/**< Number of Authentication rejects 															*/
    TI_UINT32  AuthTimeouts;		/**< Number of Authentication Time Outs 														*/

} TIWLN_COUNTERS;

/** \struct TPowerMgr_PowerMode
 * \brief Power Mode Parameters
 * 
 * \par Description
 * 
 * \sa
 */
typedef struct 
{
    PowerMgr_PowerMode_e    PowerMode;			/**< Power Mode	Type		*/
    PowerMgr_Priority_e     PowerMngPriority; 	/**< Power Mode	Priority	*/
} TPowerMgr_PowerMode;

/** \struct TWscMode
 * \brief WSC Mode
 * 
 * \par Description
 * This structure is used whenever the WiFi Simple Configuration Mode is modified between ON and OFF.
 * Upon enabling the Simple Configuration, the user must fill the probeReqWSCIE fields
 * 
 * \sa
 */
typedef struct 
{
    TIWLN_SIMPLE_CONFIG_MODE  WSCMode;						/**< WiFi Simple Configuration mode 			   			*/
    TI_UINT32 uWscIeSize; 						            /**< Simple Config IE actual size (the part after the OUI) */
    TI_UINT8 probeReqWSCIE[DOT11_WSC_PROBE_REQ_MAX_LENGTH];	/**< Buffer which holds the parameters of ProbeReq - WSC IE	*/
}  TWscMode;

/** \struct TKeepAliveTemplate
 * \brief Keep Alive Template
 * 
 * \par Description
 * Used for Add/Remove to/from FW Keep Alive Configuration (Parameters & Template)
 * 
 * \sa
 */
typedef struct
{
    TKeepAliveParams    keepAliveParams;								/**< Keep Alive Parameters						*/
    TI_UINT8            msgBuffer[ KEEP_ALIVE_TEMPLATE_MAX_LENGTH ];	/**< Buffer which holds the Keep Alive Template	*/
    TI_UINT32           msgBufferLength;								/**< Length of Keep Alive Template				*/

} TKeepAliveTemplate;

/** \struct TKeepAliveConfig
 * \brief Keep Alive Configuration
 * 
 * \par Description
 * Used for Get/Set Keep Alive Configuration (Parameters & Template)
 * 
 * \sa
 */
typedef struct
{
    TI_UINT8                enaDisFlag;									/**< Indicates if Keep Alive is Enabled/Disabled	*/
    TKeepAliveTemplate      templates[ KEEP_ALIVE_MAX_USER_MESSAGES ];	/**< Buffer which holds the maximum Keep Alive Template 
																		* possible (according to maximum Keep Alive user messages
																		possible) 
																		*/
} TKeepAliveConfig;

#endif /* __TIWLNIF_H__*/
