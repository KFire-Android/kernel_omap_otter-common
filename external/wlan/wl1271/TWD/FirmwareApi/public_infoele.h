/*
 * public_infoele.h
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

/**********************************************************************************************************************

  FILENAME:       public_infoele.h

  DESCRIPTION:    Contains information element defines/structures used by the FW and host.



***********************************************************************************************************************/
#ifndef PUBLIC_INFOELE_H
#define PUBLIC_INFOELE_H


#include "public_types.h"
#include "public_commands.h"
#include "public_radio.h" 

typedef enum
{
    ACX_WAKE_UP_CONDITIONS      = 0x0002,
    ACX_MEM_CFG                 = 0x0003,
    ACX_SLOT                    = 0x0004,

    ACX_AC_CFG                  = 0x0007,
    ACX_MEM_MAP                 = 0x0008,
    ACX_AID                     = 0x000A,

    ACX_MEDIUM_USAGE            = 0x000F,
    ACX_RX_CFG                  = 0x0010,
    ACX_TX_QUEUE_CFG            = 0x0011,
    ACX_STATISTICS              = 0x0013, /* Debug API*/
    ACX_PWR_CONSUMPTION_STATISTICS       =0x0014,
    ACX_FEATURE_CFG             = 0x0015,
    ACX_TID_CFG                 = 0x001A,
    ACX_PS_RX_STREAMING         = 0x001B, 
    ACX_BEACON_FILTER_OPT       = 0x001F,
    ACX_NOISE_HIST              = 0x0021,
    ACX_HDK_VERSION             = 0x0022, /* ???*/
    ACX_PD_THRESHOLD            = 0x0023,
    ACX_TX_CONFIG_OPT           = 0x0024,
    ACX_CCA_THRESHOLD           = 0x0025,
    ACX_EVENT_MBOX_MASK         = 0x0026,
    ACX_CONN_MONIT_PARAMS       = 0x002D,
    ACX_CONS_TX_FAILURE         = 0x002F,
    ACX_BCN_DTIM_OPTIONS        = 0x0031,
    ACX_SG_ENABLE               = 0x0032,
    ACX_SG_CFG                  = 0x0033,
    ACX_FM_COEX_CFG             = 0x0034,

    ACX_BEACON_FILTER_TABLE     = 0x0038,
    ACX_ARP_IP_FILTER           = 0x0039,
    ACX_ROAMING_STATISTICS_TBL  = 0x003B,
    ACX_RATE_POLICY             = 0x003D, 
    ACX_CTS_PROTECTION          = 0x003E, 
    ACX_SLEEP_AUTH              = 0x003F,
    ACX_PREAMBLE_TYPE           = 0x0040,
    ACX_ERROR_CNT               = 0x0041,
    ACX_IBSS_FILTER             = 0x0044,
    ACX_SERVICE_PERIOD_TIMEOUT  = 0x0045,
    ACX_TSF_INFO                = 0x0046,
    ACX_CONFIG_PS_WMM           = 0x0049,
    ACX_ENABLE_RX_DATA_FILTER   = 0x004A,
    ACX_SET_RX_DATA_FILTER      = 0x004B,
    ACX_GET_DATA_FILTER_STATISTICS = 0x004C,
    ACX_RX_CONFIG_OPT           = 0x004E,
    ACX_FRAG_CFG                = 0x004F,
    ACX_BET_ENABLE              = 0x0050,
	
#ifdef RADIO_SCOPE  /* RADIO MODULE SECTION START */

	ACX_RADIO_MODULE_START      = 0x0500,
	ACX_RS_ENABLE				= ACX_RADIO_MODULE_START,
	ACX_RS_RX					= 0x0501,

    /* Add here ... */

	ACX_RADIO_MODULE_END        = 0x0600,

#endif /* RADIO MODULE SECTION END */

    ACX_RSSI_SNR_TRIGGER        = 0x0051,
    ACX_RSSI_SNR_WEIGHTS        = 0x0052,
    ACX_KEEP_ALIVE_MODE         = 0x0053,
    ACX_SET_KEEP_ALIVE_CONFIG   = 0x0054,
    ACX_BA_SESSION_RESPONDER_POLICY = 0x0055,
    ACX_BA_SESSION_INITIATOR_POLICY = 0x0056,
    ACX_PEER_HT_CAP             = 0x0057,
    ACX_HT_BSS_OPERATION        = 0x0058,
    ACX_COEX_ACTIVITY           = 0x0059,
	ACX_BURST_MODE				= 0x005C,

    ACX_SET_RATE_MAMAGEMENT_PARAMS = 0x005D,
    ACX_GET_RATE_MAMAGEMENT_PARAMS = 0x005E,

    ACX_SET_DCO_ITRIM_PARAMS   = 0x0061,
   
    DOT11_RX_MSDU_LIFE_TIME     = 0x1004,
    DOT11_CUR_TX_PWR            = 0x100D,
    DOT11_RX_DOT11_MODE         = 0x1012,
    DOT11_RTS_THRESHOLD         = 0x1013, 
    DOT11_GROUP_ADDRESS_TBL     = 0x1014,
    ACX_SET_RADIO_PARAMS		= 0x1015,
	ACX_PM_CONFIG               = 0x1016,
     
    MAX_DOT11_IE = ACX_PM_CONFIG,
    
    MAX_IE = 0xFFFF   /*force enumeration to 16bits*/
} InfoElement_enum;


#ifdef HOST_COMPILE
typedef uint16 InfoElement_e;
#else
typedef InfoElement_enum InfoElement_e;
#endif


typedef struct
{
    InfoElement_e id;
    uint16 length;
    uint32 dataLoc; /*use this to point to for following variable-length data*/
} InfoElement_t;


typedef struct 
{
    uint16 id;
    uint16 len;
} EleHdrStruct;

#define MAX_NUM_AID     4 /* max number of STAs in IBSS */  


#ifdef HOST_COMPILE
#define INFO_ELE_HDR    EleHdrStruct    EleHdr;
#else
#define INFO_ELE_HDR
#endif

/******************************************************************************

    Name:   ACX_WAKE_UP_CONDITIONS
    Type:   Configuration
    Access: Write Only
    Length: 2

******************************************************************************/
typedef enum
{
    WAKE_UP_EVENT_BEACON_BITMAP     = 0x01, /* Wake on every Beacon*/
    WAKE_UP_EVENT_DTIM_BITMAP       = 0x02, /* Wake on every DTIM*/
    WAKE_UP_EVENT_N_DTIM_BITMAP     = 0x04, /* Wake on every Nth DTIM (Listen interval)*/
    WAKE_UP_EVENT_N_BEACONS_BITMAP  = 0x08, /* Wake on every Nth Beacon (Nx Beacon)*/
    WAKE_UP_EVENT_BITS_MASK         = 0x0F
} WakeUpEventBitMask_e;

typedef struct
{
    INFO_ELE_HDR
    uint8  wakeUpConditionBitmap;   /* The host can set one bit only. */
                                    /* WakeUpEventBitMask_e describes the Possible */
                                    /* Wakeup configuration bits*/

    uint8  listenInterval;          /* 0 for Beacon and Dtim, */
                                    /* xDtims (1-10) for Listen Interval and */
                                    /* xBeacons (1-255) for NxBeacon*/
    uint8  padding[2];              /* alignment to 32bits boundry   */
}WakeUpCondition_t;

/******************************************************************************

    Name:   ACX_MEM_CFG
    Type:   Configuration
    Access: Write Only
    Length: 12

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint8   rxMemblockNumber;           /* specifies the number of memory buffers that */
                                        /* is allocated to the Rx memory pool. The */
                                        /* actual number allocated may be less than*/
                                        /* this number if there are not enough memory */
                                        /* blocks left over for the Minimum Number of */
                                        /* Tx Blocks. Returns the actual number of RX */
                                        /* buffers allocated in the memory map*/

    uint8   txMinimumMemblockNumber;    /* specifies the minimum number of blocks that */
                                        /* must be allocated to the TX pool. Follows */
                                        /* this limit even if the Number of Rx Memory */
                                        /* Blocks parameter is ignored.*/

    uint8   numStations;                /* The number of STAs supported in IBSS mode. */
                                        /* The FW uses this field to allocate memory */
                                        /* for STA context data such as security keys*/

    uint8   numSsidProfiles;            /* The number of SSID profiles used in IBSS mode */
                                        /* Enables different profiles for different STAs */

    uint32  totalTxDescriptors;         /* Total TX Descriptors - in the past it was configured per AC */
} ACXConfigMemory_t;


/******************************************************************************

    Name:   ACX_SLOT
    Type:   Configuration
    Access: Write Only
    Length: 8

******************************************************************************/

typedef enum
{
    SLOT_TIME_LONG = 0,     /* the WiLink uses long (20 us) slots*/
    SLOT_TIME_SHORT = 1,    /* the WiLink uses short (9 us) slots*/
    DEFAULT_SLOT_TIME = SLOT_TIME_SHORT,
    MAX_SLOT_TIMES = 0xFF
} SlotTime_enum;

#ifdef HOST_COMPILE
typedef uint8 SlotTime_e;
#else
typedef SlotTime_enum SlotTime_e;
#endif


typedef struct
{
    INFO_ELE_HDR
    uint8      woneIndex;   /* reserved*/

    SlotTime_e slotTime;    /* The slot size to be used. refer to SlotTime_enum.    */
    uint8      reserved[6];
} ACXSlot_t;


/******************************************************************************

    Name:   ACX_AC_CFG
    Type:   Configuration
    Access: Write Only
    Length: 8

******************************************************************************/
typedef enum
{
    AC_BE = 0,          /* Best Effort/Legacy*/
    AC_BK = 1,          /* Background*/
    AC_VI = 2,          /* Video*/
    AC_VO = 3,          /* Voice*/
    /* AC_BCAST    = 4, */  /* Broadcast dummy access category      */ 
    AC_CTS2SELF = 4,        /* CTS2Self fictitious AC,              */  
                            /* uses #4 to follow AC_VO, as          */
                            /* AC_BCAST does not seem to be in use. */
        AC_ANY_TID = 0x1F,
	AC_INVALID = 0xFF,  /* used for gTxACconstraint */
    NUM_ACCESS_CATEGORIES = 4
} AccessCategory_enum;

typedef enum
{
	TID0 = 0,			/* Best Effort/Legacy*/
	TID1 = 1,			/* Best Effort/Legacy*/
	TID2 = 2,			/* Background*/
	TID3 = 3,			/* Video*/
	TID4 = 4,			/* Voice*/
	TID5 = 5,		/* Broadcast dummy access category*/
	TID6 = 6,
	TID7 = 7,           /* managment */
	NUM_TRAFFIC_CATEGORIES = 8
} TrafficCategory_enum;


#define AC_REQUEST                      0xfe    /* Special access category type for */
                                                /* requests*/


/* following are defult values for the IE fields*/
#define CWMIN_BK  15
#define CWMIN_BE  15
#define CWMIN_VI  7
#define CWMIN_VO  3
#define CWMAX_BK  1023
#define CWMAX_BE  63
#define CWMAX_VI  15
#define CWMAX_VO  7
#define AIFS_PIFS 1 /* slot number setting to start transmission at PIFS interval */
#define AIFS_DIFS 2 /* slot number setting to start transmission at DIFS interval - */
                    /* normal DCF access */

#define AIFS_MIN AIFS_PIFS

#define AIFSN_BK  7
#define AIFSN_BE  3
#define AIFSN_VI  AIFS_PIFS
#define AIFSN_VO  AIFS_PIFS
#define TXOP_BK   0
#define TXOP_BE   0
#define TXOP_VI   3008
#define TXOP_VO   1504
#define DEFAULT_AC_SHORT_RETRY_LIMIT 7
#define DEFAULT_AC_LONG_RETRY_LIMIT 4

/* rxTimeout values */
#define NO_RX_TIMEOUT 0

typedef struct 
{
    INFO_ELE_HDR
    uint8   ac;         /* Access Category - The TX queue's access category */
                        /* (refer to AccessCategory_enum)*/
    uint8   cwMin;      /* The contention window minimum size (in slots) for */
                        /* the access class.*/
    uint16  cwMax;      /* The contention window maximum size (in slots) for */
                        /* the access class.*/
    uint8   aifsn;      /* The AIF value (in slots) for the access class.*/
    uint8   reserved;
    uint16  txopLimit;  /* The TX Op Limit (in microseconds) for the access class.*/
} ACXAcCfg_t;


/******************************************************************************

    Name:   ACX_MEM_MAP
    Type:   Configuration
    Access: Read Only
    Length: 72
    Note:   Except for the numTxMemBlks, numRxMemBlks fields, this is
            used in MASTER mode only!!!
    
******************************************************************************/
#define MEM_MAP_NUM_FIELDS  24

typedef struct 
{
    uint32 *controlBlock; /* array of two 32-bit entries in the following order:
                            1. Tx-Result entries counter written by the FW
                            2. Tx-Result entries counter read by the host */
    void   *txResultQueueStart; /* points t first descriptor in TRQ */
} TxResultPointers_t;


typedef struct
{
    INFO_ELE_HDR
    void *codeStart;                
    void *codeEnd;                  
    void *wepDefaultKeyStart;
    void *wepDefaultKeyEnd;         
    void *staTableStart;
    void *staTableEnd;              
    void *packetTemplateStart;
    void *packetTemplateEnd;        
    TxResultPointers_t  trqBlock;             
 
    void *queueMemoryStart;
    void *queueMemoryEnd; 
    void *packetMemoryPoolStart;
    void *packetMemoryPoolEnd;
    void *debugBuffer1Start;
    void *debugBuffer1End;
    void *debugBuffer2Start;
    void *debugBuffer2End;

    uint32 numTxMemBlks;    /* Number of blocks that FW allocated for TX packets.*/
    uint32 numRxMemBlks;    /* Number of blocks that FW allocated for RX packets.   */

    /* the following 4 fields are valid in SLAVE mode only */
    uint8   *txCBufPtr;
    uint8   *rxCBufPtr;
    void    *rxControlPtr;
    void    *txControlPtr;

} MemoryMap_t;


/******************************************************************************

    Name:   ACX_AID
    Type:   Configuration
    Access: Write Only
    Length: 2
    
******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint16  Aid;    /* The Association ID to the WiLink. The WiLink uses this */
                    /* field to determine when the STA's AID bit is set in a */
                    /* received beacon and when a PS Poll frame should be */
                    /* transmitted to the AP. The host configures this information */
                    /* element after it has associated with an AP. This information */
                    /* element does not need to be set in Ad Hoc mode.*/
    uint8  padding[2];  /* alignment to 32bits boundry   */
} ACXAid_t;


/******************************************************************************

    Name:   ACX_ERROR_CNT
    Type:   Operation
    Access: Read Only
    Length: 12
    
******************************************************************************/
typedef struct
{
    INFO_ELE_HDR
    uint32 PLCPErrorCount;  /* The number of PLCP errors since the last time this */
                            /* information element was interrogated. This field is */
                            /* automatically cleared when it is interrogated.*/
    
    uint32 FCSErrorCount;   /* The number of FCS errors since the last time this */
                            /* information element was interrogated. This field is */
                            /* automatically cleared when it is interrogated.*/
    
    uint32 validFrameCount; /* The number of MPDUÂ’s without PLCP header errors received*/
                            /* since the last time this information element was interrogated. */
                            /* This field is automatically cleared when it is interrogated.*/

    uint32 seqNumMissCount; /* the number of missed sequence numbers in the squentially */
                            /* values of frames seq numbers */

} ACXErrorCounters_t;

/******************************************************************************

    Name:   ACX_MEDIUM_USAGE
    Type:   Configuration
    Access: Read Only
    Length: 8

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint32 mediumUsage; /* report to the host the value of medium usage registers*/
    uint32 period;      /* report to the host the value of medium period registers*/
} ACXMediumUsage_t;

/******************************************************************************

    Name:   ACX_RX_CFG
    Type:   Filtering Configuration
    Access: Write Only
    Length: 8
    
******************************************************************************/
/*
 * Rx configuration (filter) information element
 * ---------------------------------------------
 */
/*
    RX ConfigOptions Table
    Bit     Definition
    ===     ==========
    31:14   Reserved
    13      Copy RX Status - when set, write three receive status words to top of 
            rx'd MPDU.
            When clear, do not write three status words (added rev 1.5)
    12      Reserved
    11      RX Complete upon FCS error - when set, give rx complete interrupt for 
            FCS errors, after the rx filtering, e.g. unicast frames not to us with 
            FCS error will not generate an interrupt
    10      SSID Filter Enable - When set, the WiLink discards all beacon, 
            probe request, and probe response frames with an SSID that does not 
            match the SSID specified by the host in the START/JOIN command. 
            When clear, the WiLink receives frames with any SSID.
    9       Broadcast Filter Enable - When set, the WiLink discards all broadcast 
            frames. When clear, the WiLink receives all received broadcast frames.
    8:6     Reserved
    5       BSSID Filter Enable - When set, the WiLink discards any frames with a 
            BSSID that does not match the BSSID specified by the host. 
            When clear, the WiLink receives frames from any BSSID.
    4       MAC Addr Filter - When set, the WiLink discards any frames with a 
            destination address that does not match the MAC address of the adaptor. 
            When clear, the WiLink receives frames destined to any MAC address.
    3       Promiscuous - When set, the WiLink receives all valid frames 
            (i.e., all frames that pass the FCS check). 
            When clear, only frames that pass the other filters specified are received.
    2       FCS - When set, the WiLink includes the FCS with the received frame. 
            When clear, the FCS is discarded.
    1       PLCP header - When set, write all data from baseband to frame buffer 
            including PHY header.
    0       Reserved - Always equal to 0.

    RX FilterOptions Table
    Bit     Definition
    ===     ==========
    31:12   Reserved - Always equal to 0.
    11      Association - When set, the WiLink receives all association related frames 
            (association request/response, reassocation request/response, and 
            disassociation). When clear, these frames are discarded.
    10      Auth/De auth - When set, the WiLink receives all authentication and 
            de-authentication frames. When clear, these frames are discarded.
    9       Beacon - When set, the WiLink receives all beacon frames. When clear, 
            these frames are discarded.
    8       Contention Free - When set, the WiLink receives all contention free frames. 
            When clear, these frames are discarded.
    7       Control - When set, the WiLink receives all control frames. 
            When clear, these frames are discarded.
    6       Data - When set, the WiLink receives all data frames.   
            When clear, these frames are discarded.
    5       FCS Error - When set, the WiLink receives frames that have FCS errors. 
            When clear, these frames are discarded.
    4       Management - When set, the WiLink receives all management frames. 
            When clear, these frames are discarded.
    3       Probe Request - When set, the WiLink receives all probe request frames. 
            When clear, these frames are discarded.
    2       Probe Response - When set, the WiLink receives all probe response frames. 
            When clear, these frames are discarded.
    1       RTS/CTS/ACK - When set, the WiLink receives all RTS, CTS and ACK frames. 
            When clear, these frames are discarded.
    0       Rsvd Type/Sub Type - When set, the WiLink receives all frames that 
            have reserved frame types and sub types as defined by the 802.11 
            specification. 
            When clear, these frames are discarded.
*/
typedef struct
{
    INFO_ELE_HDR
    uint32          ConfigOptions;  /* The configuration of the receiver in the WiLink. */
                                    /* "RX ConfigOptions Table" describes the format of */
                                    /* this field.*/
    uint32          FilterOptions;  /* The types of frames that the WiLink can receive. */
                                    /* "RX FilterOptions Table" describes the format of */
                                    /* this field.*/
} ACXRxConfig_t;

/******************************************************************************

    Name:   ACX_BEACON_FILTER_OPT
    Desc:   This information element enables the host to activate beacon filtering. 
            The filter can only be activated when the STA is in PS mode. 
            When activated, either the host is not notified about beacons whose 
            unicast TIM bit is not set, or these beacons are buffered first and 
            the host is notified only after the buffer reaches a predetermined size.
            The host should not activate the filter if it configures the firmware 
            to listen to broadcasts (see the VBM Options field in the 
            ACXPowerMgmtOptions information element). The filter only affects beacons, 
            and not other MSDUs - the firmware notifies the host immediately about 
            their arrival.
    Type:   Filtering Configuration
    Access: Write Only
    Length: 2
 
******************************************************************************/
typedef struct  
{
    INFO_ELE_HDR
    uint8   enable;                /* Indicates whether the filter is enabled. */
                                   /* 1 - enabled, 0 - disabled. */
    uint8   maxNumOfBeaconsStored; /* The number of beacons without the unicast TIM */
                                   /* bit set that the firmware buffers before */
                                   /* signaling the host about ready frames. */
                                   /* When set to 0 and the filter is enabled, beacons */
                                   /* without the unicast TIM bit set are dropped.*/
    uint8  padding[2];             /* alignment to 32bits boundry   */
} ACXBeaconFilterOptions_t;


/******************************************************************************

    Name:   ACX_BEACON_FILTER_TABLE
    Desc:   This information element configures beacon filtering handling for the
            set of information elements. An information element in a beacon can be 
            set to be: ignored (never compared, and changes will not cause beacon 
            transfer), checked (compared, and transferred in case of a change), or 
            transferred (transferred to the host for each appearance or disappearance).
            The table contains all information elements that are subject to monitoring 
            for host transfer. 
            All information elements that are not in the table should be ignored for 
            monitoring.
            This functionality is only enabled when beacon filtering is enabled by 
            ACX_BEACON_FILTER_OPT.
    Type:   Filtering Configuration
    Access: Write Only
    Length: 101
    Notes:  the field measuring the value of received beacons for which the device 
            wakes up the host in ACX_BEACON_FILTER_OPT does not affect 
            this information element.
    
******************************************************************************/

/*
    ACXBeaconFilterEntry (not 221)
    Byte Offset     Size (Bytes)    Definition 
    ===========     ============    ==========
    0               1               IE identifier
    1               1               Treatment bit mask

    ACXBeaconFilterEntry (221)
    Byte Offset     Size (Bytes)    Definition 
    ===========     ============    ==========
    0               1               IE identifier
    1               1               Treatment bit mask
    2               3               OUI
    5               1               Type
    6               2               Version


    Treatment bit mask - The information element handling:
                         bit 0 - The information element is compared and transferred
                                 in case of change.
                         bit 1 - The information element is transferred to the host 
                                 with each appearance or disappearance.
                         Note that both bits can be set at the same time.
*/
#define BEACON_FILTER_TABLE_MAX_IE_NUM                      (32)
#define BEACON_FILTER_TABLE_MAX_VENDOR_SPECIFIC_IE_NUM      (6)
#define BEACON_FILTER_TABLE_IE_ENTRY_SIZE                   (2)
#define BEACON_FILTER_TABLE_EXTRA_VENDOR_SPECIFIC_IE_SIZE   (6)
#define BEACON_FILTER_TABLE_MAX_SIZE    ((BEACON_FILTER_TABLE_MAX_IE_NUM * BEACON_FILTER_TABLE_IE_ENTRY_SIZE) + \
                                         (BEACON_FILTER_TABLE_MAX_VENDOR_SPECIFIC_IE_NUM * BEACON_FILTER_TABLE_EXTRA_VENDOR_SPECIFIC_IE_SIZE))

typedef struct ACXBeaconFilterIETableStruct {
    INFO_ELE_HDR
    uint8 NumberOfIEs;                          /* The number of IE's in the table*/
                                                /* 0 - clears the table.*/

    uint8 padding[3];  /* alignment to 32bits boundry   */
    uint8 IETable[BEACON_FILTER_TABLE_MAX_SIZE];
} ACXBeaconFilterIETable_t;

/******************************************************************************

    Name:   ACX_COEX_ACTIVITY_TABLE
    
******************************************************************************/

typedef enum
{
    COEX_IP_BT = 0,
    COEX_IP_WLAN, 
    COEX_IP_DUAL_MODE,   /* That define isn't valid value in DR&FW interface and use just in the FW */
    MAX_COEX_IP
} CoexIp_enum;

#ifdef HOST_COMPILE
typedef uint8 CoexIp_e;
#else
typedef CoexIp_enum CoexIp_e;
#endif

typedef struct ACXCoexActivityIEStruct {
    INFO_ELE_HDR
    CoexIp_e coexIp;         /* 0-BT, 1-WLAN (according to CoexIp_e in FW) */
    uint8  activityId;       /* According to BT/WLAN activity numbering in FW */ 
    uint8  defaultPriority;  /* 0-255, activity default priority */
    uint8  raisedPriority;   /* 0-255, activity raised priority */
    uint16 minService;       /* 0-65535, The minimum service requested either in
                                requests or in milliseconds depending on activity ID */
    uint16 maxService;       /* 0-65535, The maximum service allowed either in
                            requests or in milliseconds depending on activity ID */
} ACXCoexActivityIE_t;

/******************************************************************************

    Name:   ACX_ARP_IP_FILTER 
    Type:   Filtering Configuration
    Access: Write Only
    Length: 20

******************************************************************************/

#define ARP_FILTER_DISABLED                    (0)
#define ARP_FILTER_ENABLED                  (0x01)
#define ARP_FILTER_AUTO_ARP_ENABLED         (0x03)

typedef struct  
{    
    INFO_ELE_HDR
    uint8     ipVersion;       /* The IP version of the IP address: 4 - IPv4, 6 - IPv6.*/
    uint8     arpFilterEnable; /* 0x00 - No ARP features */
                               /* 0x01 - Only ARP filtering */
                               /* 0x03 - Both ARP filtering and Auto-ARP */
                               /* For IPv6 it MUST be 0 */
    uint8     padding[2];      /* alignment to 32bits boundry   */
    uint8     address[16];     /* The IP address used to filter ARP packets. ARP packets */
                               /* that do not match this address are dropped. */
                               /* When the IP Version is 4, the last 12 bytes of */
                               /* the address are ignored.*/
    
} ACXConfigureIP_t;


/******************************************************************************

  Name:     ACX_IBSS_FILTER
  Type:     Filtering Configuration
  Access:   Write Only
  Length:   1
  
******************************************************************************/
typedef struct  
{
    INFO_ELE_HDR
    uint8   enable; /* if set (i.e. IBSS mode), forward beacons from the same SSID*/
                    /* (also from different BSSID), with bigger TSF then the this of */
                    /* the current BSS.*/
    uint8   padding[3]; /* alignment to 32bits boundry   */
} ACXIBSSFilterOptions_t;


/******************************************************************************

  Name:     ACX_SERVICE_PERIOD_TIMEOUT
  Type:     Configuration
  Access:   Write Only
  Length:   1
  
******************************************************************************/
typedef struct 
{    
    INFO_ELE_HDR
    uint16 PsPollTimeout; /* the maximum time that the device will wait to receive */
                          /* traffic from the AP after transmission of PS-poll.*/
    
    uint16 UpsdTimeout;   /* the maximum time that the device will wait to receive */
                          /* traffic from the AP after transmission from UPSD enabled*/
                          /* queue.*/
} ACXRxTimeout_t;

/******************************************************************************

    Name:   ACX_TX_QUEUE_CFG
    Type:   Configuration
    Access: Write Only
    Length: 8
    
******************************************************************************/
typedef struct
{
    INFO_ELE_HDR
    uint8   qID;                        /* The TX queue ID number.*/
    uint8   padding[3];                 /* alignment to 32bits boundry   */
    uint16  numberOfBlockHighThreshold; /* The maximum memory blocks allowed in the */
                                        /* queue.*/
    uint16  numberOfBlockLowThreshold;  /* The minimum memory blocks that are */
                                        /* guaranteed for this queue.*/
} ACXTxQueueCfg_t;


/******************************************************************************

    Name:   ACX_STATISTICS
    Type:   Statistics
    Access: Write Only
    Length: 
    Note:   Debug API

******************************************************************************/
typedef struct 
{
    uint32  debug1;
    uint32  debug2;
    uint32  debug3;
    uint32  debug4;
    uint32  debug5;
    uint32  debug6;
}DbgStatistics_t;

typedef struct 
{
    uint32  numOfTxProcs;
    uint32  numOfPreparedDescs;
    uint32  numOfTxXfr;
    uint32  numOfTxDma;    
    uint32  numOfTxCmplt;
    uint32  numOfRxProcs;
    uint32  numOfRxData;
}RingStatistics_t;

typedef struct
{
    uint32 numOfTxTemplatePrepared;
    uint32 numOfTxDataPrepared;
    uint32 numOfTxTemplateProgrammed;
    uint32 numOfTxDataProgrammed;
    uint32 numOfTxBurstProgrammed;
    uint32 numOfTxStarts;
    uint32 numOfTxImmResp;
    uint32 numOfTxStartTempaltes;
    uint32 numOfTxStartIntTemplate;
    uint32 numOfTxStartFwGen;
    uint32 numOfTxStartData;
    uint32 numOfTxStartNullFrame;
    uint32 numOfTxExch;
    uint32 numOfTxRetryTemplate;
    uint32 numOfTxRetryData;
    uint32 numOfTxExchPending;
    uint32 numOfTxExchExpiry;
    uint32 numOfTxExchMismatch;
    uint32 numOfTxDoneTemplate;
    uint32 numOfTxDoneData;
    uint32 numOfTxDoneIntTemplate;
    uint32 numOfTxPreXfr;
    uint32 numOfTxXfr;
    uint32 numOfTxXfrOutOfMem;
    uint32 numOfTxDmaProgrammed;
    uint32 numOfTxDmaDone;
} TxStatistics_t;


typedef struct
{
    uint32 RxOutOfMem;
    uint32 RxHdrOverflow;
    uint32 RxHWStuck;
    uint32 RxDroppedFrame;
    uint32 RxCompleteDroppedFrame;
    uint32 RxAllocFrame;
	uint32 RxDoneQueue;
	uint32 RxDone;
	uint32 RxDefrag;
	uint32 RxDefragEnd;
	uint32 RxMic;
	uint32 RxMicEnd;
	uint32 RxXfr;
    uint32 RxXfrEnd;
    uint32 RxCmplt;
    uint32 RxPreCmplt;
    uint32 RxCmpltTask;
	uint32 RxPhyHdr;
    uint32 RxTimeout;
} RxStatistics_t;


typedef struct
{
    uint32 RxDMAErrors;
    uint32 TxDMAErrors;
} DMAStatistics_t;


typedef struct
{
    uint32 IRQs;              /* irqisr() */
} IsrStatistics_t;


typedef struct WepStatistics_t
{
    uint32 WepAddrKeyCount;      /* Count of WEP address keys configured*/
    uint32 WepDefaultKeyCount;   /* Count of default keys configured*/
    uint32 WepKeyNotFound;       /* count of number of times that WEP key not found on lookup*/
    uint32 WepDecryptFail;       /* count of number of times that WEP key decryption failed*/
    uint32 WepEncryptFail;       /* count of number of times that WEP key encryption failed*/
    uint32 WepDecPackets;        /* WEP Packets Decrypted*/
    uint32 WepDecInterrupt;      /* WEP Decrypt Interrupts*/
    uint32 WepEnPackets;         /* WEP Packets Encrypted*/
    uint32 WepEnInterrupt;       /* WEP Encrypt Interrupts*/
} WepStatistics_t;


#define PWR_STAT_MAX_CONT_MISSED_BCNS_SPREAD 10
typedef struct PwrStatistics_t
{
    uint32 MissingBcnsCnt;      /* Count the amount of missing beacon interrupts to the host.*/
    uint32 RcvdBeaconsCnt;      /* Count the number of received beacons.*/
    uint32 ConnectionOutOfSync;         /* Count the number of times TSF Out Of Sync occures, meaning we lost more consecutive beacons that defined by the host's threshold.*/
    uint32 ContMissBcnsSpread[PWR_STAT_MAX_CONT_MISSED_BCNS_SPREAD];  /* Gives statistics about the spread continuous missed beacons.*/
                                    /* The 16 LSB are dedicated for the PS mode.*/
                                    /* The 16 MSB are dedicated for the PS mode.*/
                                    /* ContMissBcnsSpread[0] - single missed beacon.*/
                                    /* ContMissBcnsSpread[1] - two continuous missed beacons.*/
                                    /* ContMissBcnsSpread[2] - three continuous missed beacons.*/
                                    /* ...*/
                                    /* ContMissBcnsSpread[9] - ten and more continuous missed beacons.*/
    uint32 RcvdAwakeBeaconsCnt; /* Count the number of beacons in awake mode.*/
} PwrStatistics_t;


typedef struct MicStatistics_t
{
    uint32 MicRxPkts;
    uint32 MicCalcFailure;
} MicStatistics_t;


typedef struct AesStatisticsStruct
{
    uint32 AesEncryptFail;
    uint32 AesDecryptFail;
    uint32 AesEncryptPackets;
    uint32 AesDecryptPackets;
    uint32 AesEncryptInterrupt;
    uint32 AesDecryptInterrupt;
} AesStatistics_t;

typedef struct GemStatisticsStruct
{
    uint32 GemEncryptFail;
    uint32 GemDecryptFail;
    uint32 GemEncryptPackets;
    uint32 GemDecryptPackets;
    uint32 GemEncryptInterrupt;
    uint32 GemDecryptInterrupt;
} GemStatistics_t;

typedef struct EventStatistics_t
{
    uint32 calibration;
    uint32 rxMismatch;
    uint32 rxMemEmpty;
} EventStatistics_t;


typedef struct PsPollUpsdStatistics_t
{
    uint32 psPollTimeOuts;
    uint32 upsdTimeOuts;
    uint32 upsdMaxAPturn;
    uint32 psPollMaxAPturn;
    uint32 psPollUtilization;
    uint32 upsdUtilization;
} PsPollUpsdStatistics_t;

typedef struct RxFilterStatistics_t
{
    uint32 beaconFilter;
    uint32 arpFilter;
    uint32 MCFilter;
    uint32 dupFilter;
    uint32 dataFilter;
    uint32 ibssFilter;
} RxFilterStatistics_t;

typedef struct ClaibrationFailStatistics_t
{
	uint32 initCalTotal;
	uint32 initRadioBandsFail;
	uint32 initSetParams;
	uint32 initTxClpcFail;
	uint32 initRxIqMmFail;
	uint32 tuneCalTotal;
	uint32 tuneDrpwRTrimFail;
	uint32 tuneDrpwPdBufFail;
	uint32 tuneDrpwTxMixFreqFail;
	uint32 tuneDrpwTaCal;
	uint32 tuneDrpwRxIf2Gain;
	uint32 tuneDrpwRxDac;
	uint32 tuneDrpwChanTune;
	uint32 tuneDrpwRxTxLpf;
	uint32 tuneDrpwLnaTank;
	uint32 tuneTxLOLeakFail;
	uint32 tuneTxIqMmFail;
	uint32 tuneTxPdetFail;
	uint32 tuneTxPPAFail;
	uint32 tuneTxClpcFail;
	uint32 tuneRxAnaDcFail;
	uint32 tuneRxIqMmFail;
	uint32 calStateFail;
}ClaibrationFailStatistics_t;

typedef struct ACXStatisticsStruct
{
    INFO_ELE_HDR
    RingStatistics_t ringStat;
    DbgStatistics_t  debug;
    TxStatistics_t   tx;
    RxStatistics_t   rx;
    DMAStatistics_t  dma;
    IsrStatistics_t  isr;
    WepStatistics_t  wep;
    PwrStatistics_t  pwr;
    AesStatistics_t  aes;
    MicStatistics_t  mic;
    EventStatistics_t event;
    PsPollUpsdStatistics_t ps;
    RxFilterStatistics_t rxFilter;
	ClaibrationFailStatistics_t radioCal;
    GemStatistics_t  gem;
} ACXStatistics_t;

/******************************************************************************

    Name:   ACX_ROAMING_STATISTICS_TBL
    Desc:   This information element reads the current roaming triggers 
            counters/metrics. 
    Type:   Statistics
    Access: Read Only
    Length: 6

******************************************************************************/
typedef struct 
{
    INFO_ELE_HDR
    uint32 MissedBeacons; /* The current number of consecutive lost beacons*/
	uint8  snrData;       /* The current average SNR in db - For Data Packets*/
	uint8  snrBeacon;     /* The current average SNR in db - For Beacon Packets*/
    int8   rssiData;      /* The current average RSSI  - For Data Packets*/
    int8   rssiBeacon;    /* The current average RSSI - For Beacon Packets*/
}ACXRoamingStatisticsTable_t;


/******************************************************************************

    Name:   ACX_FEATURE_CFG
    Desc:   Provides expandability for future features
    Type:   Configuration
    Access: Write Only
    Length: 8
    
******************************************************************************/

/* bit defines for Option: */
#define FEAT_PCI_CLK_RUN_ENABLE     0x00000002  /* Enable CLK_RUN on PCI bus */

/* bit defines for dataflowOptions: */
#define DF_ENCRYPTION_DISABLE       0x00000001  /* When set, enable encription in FW.*/
                                                /* when clear, disable encription. */
#define DF_SNIFF_MODE_ENABLE        0x00000080  /* When set, enable decryption in FW.*/
                                                /* when clear, disable decription. */
typedef struct
{
    INFO_ELE_HDR
    uint32 Options;         /* Data flow options - refer to above definitions*/
    uint32 dataflowOptions; /* Data flow options - refer to above definitions*/
} ACXFeatureConfig_t;



/******************************************************************************

    Name:   ACX_TID_CFG
    Type:   Configuration
    Access: Write Only
    Length: 16
    
******************************************************************************/
typedef enum
{
    CHANNEL_TYPE_DCF = 0,   /* DC/LEGACY*/
    CHANNEL_TYPE_EDCF = 1,  /* EDCA*/
    CHANNEL_TYPE_HCCA = 2,  /* HCCA*/
    MAX_CHANNEL_TYPE = CHANNEL_TYPE_HCCA
} ChannelType_enum;

typedef enum
{
    PS_SCHEME_LEGACY         = 0, /* Regular PS: simple sending of packets*/
    PS_SCHEME_UPSD_TRIGGER   = 1, /* UPSD: sending a packet triggers a UPSD downstream*/
    PS_SCHEME_LEGACY_PSPOLL  = 2, /* Legacy PSPOLL: a PSPOLL packet will be sent before */
                                  /* every data packet transmission in this queue.*/
    PS_SCHEME_SAPSD          = 3, /* Scheduled APSD mode.*/
    MAX_PS_SCHEME = PS_SCHEME_SAPSD
} PSScheme_enum;

typedef enum
{
    ACK_POLICY_LEGACY = 0,   /* ACK immediate policy*/
    ACK_POLICY_NO_ACK = 1,   /* no ACK policy*/
    ACK_POLICY_BLOCK  = 2,   /* block ack policy*/
    MAX_ACK_POLICY = ACK_POLICY_BLOCK
} AckPolicy_enum;


#ifdef HOST_COMPILE
typedef uint8 ChannelType_e;
typedef uint8 PSScheme_e;
typedef uint8 AckPolicy_e;
#else
typedef ChannelType_enum ChannelType_e;
typedef PSScheme_enum PSScheme_e;
typedef AckPolicy_enum AckPolicy_e;
#endif



/* Michal recommendation:
   in the ACXTIDConfig_t structure we need only the fields psScheme, and one other field for AC id (queue? tsid?).
   the rest are obsolete. see IEPsDeliveryTriggerType_t in CE2.0.
   */

typedef struct
{
    INFO_ELE_HDR
    uint8   queueID;        /* The TX queue ID number (0-7).*/
    uint8   channelType;    /* Channel access type for the queue.*/
                            /* Refer to ChannelType_enum.*/
    uint8   tsid;           /* for EDCA - the AC Index (0-3, refer to*/
                            /* AccessCategory_enum).*/
                            /* For HCCA - HCCA Traffic Stream ID (TSID) of */
                            /* the queue (8-15).*/
    PSScheme_e  psScheme;   /* The power save scheme of the specified queue.*/
                            /* Refer to PSScheme_enum.*/
    AckPolicy_e ackPolicy;  /* The TX queue ACK policy. */
    uint8  padding[3];      /* alignment to 32bits boundry   */
    uint32 APSDConf[2];     /* Not supported in this version !!!*/
}ACXTIDConfig_t;



/******************************************************************************

    Name:	ACX_PS_RX_STREAMING
	Type:	Configuration
	Access:	Write Only
	Length: 32
	
******************************************************************************/
typedef struct
{
    INFO_ELE_HDR
    uint8 	TID;            /* The TID index*/
    Bool_e 	rxPSDEnabled;   /* indicates if this traffic stream requires */
                            /* employing an RX Streaming delivery mechanism for the TID*/

    uint8   streamPeriod;   /* the time period for which a trigger needs to be transmitted*/
                            /* in case no data TX triggers are sent by host*/
    uint8   txTimeout;      /* the timeout from last TX trigger after which FW*/
                            /* starts generating triggers by itself*/
}ACXPsRxStreaming_t;

/************************************************************
*      MULTIPLE RSSI AND SNR                                *
*************************************************************/

typedef enum
{
    RX_QUALITY_EVENT_LEVEL = 0,  /* The event is a "Level" indication which keeps */
                               /* triggering as long as the average RSSI is below*/
                               /* the threshold.*/

	RX_QUALITY_EVENT_EDGE = 1    /* The event is an "Edge" indication which triggers*/
                               /* only when the RSSI threshold is crossed from above.*/
}rxQualityEventType_enum;

/* The direction in which the trigger is active */
typedef enum
{
    RSSI_EVENT_DIR_LOW = 0,
    RSSI_EVENT_DIR_HIGH = 1,
    RSSI_EVENT_DIR_BIDIR = 2
}RssiEventDir_e;

/******************************************************************************

    RSSI/SNR trigger configuration:

    ACX_RSSI_SNR_TRIGGER
    ACX_RSSI_SNR_WIGHTS

******************************************************************************/
#define NUM_OF_RSSI_SNR_TRIGGERS 8
typedef struct
{
    int16  threshold;
    uint16 pacing; /* Minimum delay between consecutive triggers in milliseconds (0 - 60000) */
    uint8  metric; /* RSSI Beacon, RSSI Packet, SNR Beacon, SNR Packet */
    uint8  type;   /* Level / Edge */
    uint8  direction; /* Low, High, Bidirectional */
    uint8  hystersis; /* Hysteresis range in dB around the threshold value (0 - 255) */
    uint8  index; /* Index of Event. Values 0 - 7 */
    uint8  enable; /* 1 - Configured, 2 - Not Configured;  (for recovery using) */
    uint8  padding[2];
}RssiSnrTriggerCfg_t;

typedef struct
{
    INFO_ELE_HDR
    RssiSnrTriggerCfg_t param;
}ACXRssiSnrTriggerCfg_t;

/* Filter Weight for every one of 4 RSSI /SNR Trigger Metrics  */
typedef struct
{
    uint8 rssiBeaconAverageWeight;
    uint8 rssiPacketAverageWeight;
    uint8 snrBeaconAverageWeight;
    uint8 snrPacketAverageWeight;
}RssiSnrAverageWeights_t;

typedef struct
{
    INFO_ELE_HDR
    RssiSnrAverageWeights_t param;
}ACXRssiSnrAverageWeights_t;

typedef enum
{
    METRIC_EVENT_RSSI_BEACON = 0,
    METRIC_EVENT_RSSI_DATA   = 1,
    METRIC_EVENT_SNR_BEACON  = 2,
    METRIC_EVENT_SNR_DATA     = 3, 
	METRIC_EVENT_TRIGGER_SIZE = 4
}MetricEvent_e;

/******************************************************************************

    Name:   ACX_NOISE_HIST
    Desc:   Noise Histogram activation is done by special command from host which
            is responsible to read the results using this IE.
    Type:   Configuration
    Access: Read Only
    Length: 48 (NOISE_HIST_LEN=8)
    
******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint32 counters[NOISE_HIST_LEN]; /* This array of eight 32 bit counters describes */
                                     /* the histogram created by the FW noise */
                                     /* histogram engine.*/

    uint32 numOfLostCycles;          /* This field indicates the number of measurement */
                                     /* cycles with failure because Tx was active.*/

    uint32 numOfTxHwGenLostCycles;   /* This field indicates the number of measurement */
                                     /* cycles with failure because Tx (FW Generated)*/
                                     /* was active.*/

    uint32 numOfRxLostCycles;        /* This field indicates the number of measurement */
                                     /* cycles because the Rx CCA was active. */
} NoiseHistResult_t;

/******************************************************************************

    Name:   ACX_PD_THRESHOLD
    Type:   Configuration
    Access: Write Only
    Length: 4

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint32 pdThreshold; /* The packet detection threshold in the PHY.*/
} ACXPacketDetection_t;


/******************************************************************************

    Name:	ACX_RATE_POLICY
	Type:	Configuration
	Access:	Write Only
	Length: 132

******************************************************************************/

#define HOST_MAX_RATE_POLICIES       (8)


typedef struct 
{
    INFO_ELE_HDR
    uint32        numOfClasses;                    /* The number of transmission rate */
                                                   /* fallback policy classes.*/

    txAttrClass_t rateClasses[HOST_MAX_RATE_POLICIES];  /* Rate Policies table*/
}ACXTxAttrClasses_t;



/******************************************************************************

    Name:   ACX_CTS_PROTECTION
    Type:   Configuration
    Access: Write Only
    Length: 1
    
******************************************************************************/

typedef struct 
{
    INFO_ELE_HDR
    uint8   ctsProtectMode; /* This field is a flag enabling or disabling the*/
                                /* CTS-to-self protection mechanism:*/
                                /* 0 - disable, 1 - enable*/
    uint8  padding[3];          /* alignment to 32bits boundry   */
}ACXCtsProtection_t;

/******************************************************************************

    ACX_FRAG_CFG

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint16  fragThreshold;
    uint8   padding[2];          /* alignment toIE_RTS_CTS_CFG 32bits boundry   */
   
} ACXFRAGThreshold_t;


/******************************************************************************

    ACX_RX_CONFIG_OPT

******************************************************************************/
typedef enum  
{
    RX_QUEUE_TYPE_RX_LOW_PRIORITY,    /* All except the high priority */
    RX_QUEUE_TYPE_RX_HIGH_PRIORITY,   /* Management and voice packets */
    RX_QUEUE_TYPE_NUM,
    RX_QUEUE_TYPE_MAX = MAX_POSITIVE8
} RxQueueType_enum;


#ifdef HOST_COMPILE
    typedef uint8 RxQueueType_e;
#else
    typedef RxQueueType_enum RxQueueType_e;
#endif


typedef struct 
{
    INFO_ELE_HDR
    uint16         rxMblkThreshold;   /* Occupied Rx mem-blocks number which requires interrupting the host (0 = no buffering) */
    uint16         rxPktThreshold;    /* Rx packets number which requires interrupting the host  (0 = no buffering) */ 
    uint16         rxCompleteTimeout; /* Max time in msec the FW may delay Rx-Complete interrupt */
    RxQueueType_e  rxQueueType;       /* see above */   
    uint8          reserved;
} ACXRxBufferingConfig_t;


/******************************************************************************

    Name:   ACX_SLEEP_AUTH
    Desc:   configuration of sleep authorization level
    Type:   System Configuration
    Access: Write Only
    Length: 1

******************************************************************************/

typedef struct 
{
    INFO_ELE_HDR
    uint8   sleepAuth; /* The sleep level authorization of the device. */
                       /* 0 - Always active*/
                       /* 1 - Power down mode: light / fast sleep*/
                       /* 2 - ELP mode: Deep / Max sleep*/
        
    uint8  padding[3]; /* alignment to 32bits boundry   */
}ACXSleepAuth_t;

/******************************************************************************

    Name:	ACX_PM_CONFIG
	Desc:   configuration of power management
	Type:	System Configuration
	Access:	Write Only
	Length: 1

******************************************************************************/

typedef struct 
{
    INFO_ELE_HDR
	uint32	hostClkSettlingTime;	/* Host CLK settling time (in uSec units) */
	uint8	hostFastWakeupSupport;	/* 0 - not supported */
									/* 1 - supported */
    uint8  padding[3]; 				/* alignment to 32bits boundry   */
}ACXPMConfig_t;

/******************************************************************************

    Name:   ACX_PREAMBLE_TYPE
    Type:   Configuration
    Access: Write Only
    Length: 1
    
******************************************************************************/

typedef enum
{
	LONG_PREAMBLE			= 0,
	SHORT_PREAMBLE			= 1,
	OFDM_PREAMBLE			= 4,
	N_MIXED_MODE_PREAMBLE	= 6,
	GREENFIELD_PREAMBLE		= 7,
	PREAMBLE_INVALID		= 0xFF
} Preamble_enum;


#ifdef HOST_COMPILE
typedef uint8 Preamble_e;
#else
typedef Preamble_enum Preamble_e;
#endif


typedef struct
{
    INFO_ELE_HDR
    Preamble_e preamble; /* When set, the WiLink transmits beacon, probe response, */
                         /* RTS and PS Poll frames with a short preamble. */
                         /* When clear, the WiLink transmits the frame with a long */
                         /* preamble.*/
    uint8  padding[3];  /* alignment to 32bits boundry   */
} ACXPreamble_t;


/******************************************************************************

    Name:   ACX_CCA_THRESHOLD
    Type:   Configuration
    Access: Write Only
    Length: 2
    
******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint16 rxCCAThreshold; /* The Rx Clear Channel Assessment threshold in the PHY*/
                           /* (the energy threshold).*/
    Bool_e txEnergyDetection;  /* The Tx ED value for TELEC Enable/Disable*/
    uint8  padding;
} ACXEnergyDetection_t;
      

/******************************************************************************

    Name:   ACX_EVENT_MBOX_MASK
    Type:   Operation
    Access: Write Only
    Length: 8
    
******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint32 lowEventMask;   /* Indicates which events are masked and which are not*/
                           /* Refer to EventMBoxId_enum in public_event_mbox.h.*/
    
    uint32 highEventMask;  /* Not in use (should always be set to 0xFFFFFFFF).*/
} ACXEventMboxMask_t;


/******************************************************************************

    Name:   ACX_CONN_MONIT_PARAMS
    Desc:   This information element configures the SYNCHRONIZATION_TIMEOUT 
            interrupt indicator. It configures the number of missed Beacons 
            before issuing the SYNCHRONIZATION_TIMEOUT event.
    Type:   Configuration
    Access: Write Only
    Length: 8

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint32 TSFMissedThreshold; /* The number of consecutive beacons that can be */
                               /* lost before the WiLink raises the */
                               /* SYNCHRONIZATION_TIMEOUT event.*/

    uint32 BSSLossTimeout;     /* The delay (in time units) between the time at */
                               /* which the device issues the SYNCHRONIZATION_TIMEOUT*/
                               /* event until, if no probe response or beacon is */
                               /* received a BSS_LOSS event is issued.*/
} AcxConnectionMonitorOptions;

/******************************************************************************

    Name:   ACX_CONS_TX_FAILURE
    Desc:   This information element configures the number of frames transmission
            failures before issuing the "Max Tx Retry" event. The counter is 
            incremented only for unicast frames or frames that require Ack 
    Type:   Configuration
    Access: Write Only
    Length: 1
    
******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint8 maxTxRetry; /* the number of frames transmission failures before */
                      /* issuing the "Max Tx Retry" event*/
    uint8  padding[3];  /* alignment to 32bits boundry   */
} ACXConsTxFailureTriggerParameters_t;


/******************************************************************************

    Name:   ACX_BCN_DTIM_OPTIONS
    Type:   Configuration
    Access: Write Only
    Length: 5
    
******************************************************************************/

typedef struct 
{    
    INFO_ELE_HDR
    uint16 beaconRxTimeOut;
    uint16 broadcastTimeOut;
    uint8  rxBroadcastInPS;  /* if set, enables receive of broadcast packets */
                             /* in Power-Save mode.*/
    uint8  consecutivePsPollDeliveryFailureThr;         /* Consecutive PS Poll Fail before updating the Driver */
    uint8  padding[2];       /* alignment to 32bits boundry   */
} ACXBeaconAndBroadcastOptions_t;


/******************************************************************************

    Name:   ACX_SG_ENABLE
    Desc:   This command instructs the WiLink to set the Soft Gemini (BT co-existence)
            state to either enable/disable or sense mode. 
    Type:   Configuration
    Access: Write Only
    Length: 1
    
******************************************************************************/
typedef struct
{
    INFO_ELE_HDR
	uint8	coexOperationMode; /* 0- Co-ex operation is Disabled
								  1- Co-ex operation is configured to Protective mode
								  2- Co-ex operation is configured to Opportunistic mode 
			                      
								  Default Value: 0- Co-ex operation is Disabled
								*/

    uint8  padding[3];  /* alignment to 32bits boundry   */

} ACXBluetoothWlanCoEnableStruct;



/** \struct TSoftGeminiParams
 * \brief Soft Gemini Parameters
 *
 * \par Description
 * Used for Setting/Printing Soft Gemini Parameters
 *
 * \sa
 */

typedef enum
{
	SOFT_GEMINI_BT_PER_THRESHOLD = 0,
	SOFT_GEMINI_HV3_MAX_OVERRIDE,
	SOFT_GEMINI_BT_NFS_SAMPLE_INTERVAL,
	SOFT_GEMINI_BT_LOAD_RATIO,
	SOFT_GEMINI_AUTO_PS_MODE,
	SOFT_GEMINI_AUTO_SCAN_PROBE_REQ,
	SOFT_GEMINI_ACTIVE_SCAN_DURATION_FACTOR_HV3,
	SOFT_GEMINI_ANTENNA_CONFIGURATION,
	SOFT_GEMINI_BEACON_MISS_PERCENT,
	SOFT_GEMINI_RATE_ADAPT_THRESH,
	SOFT_GEMINI_RATE_ADAPT_SNR,
    SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_BR,
    SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_BR,
    SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_BR,
    SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_BR,
    SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_BR,
    SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_BR,
    SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_EDR,
	SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_EDR,
	SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_EDR,
    SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_EDR,
	SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_EDR,
	SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_EDR,
	SOFT_GEMINI_RXT,
	SOFT_GEMINI_TXT,
	SOFT_GEMINI_ADAPTIVE_RXT_TXT,
	SOFT_GEMINI_PS_POLL_TIMEOUT,
	SOFT_GEMINI_UPSD_TIMEOUT,
	SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MIN_EDR,
	SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MAX_EDR,
	SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_MASTER_EDR,
    SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_EDR,
	SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_EDR,
	SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_EDR,
    SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MIN_BR,   
    SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MAX_BR, 
    SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_BR, 
    SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_HV3,
    SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_A2DP,
	SOFT_GEMINI_PASSIVE_SCAN_A2DP_BT_TIME,
	SOFT_GEMINI_PASSIVE_SCAN_A2DP_WLAN_TIME,
	SOFT_GEMINI_HV3_MAX_SERVED, 
	SOFT_GEMINI_DHCP_TIME,
    SOFT_GEMINI_ACTIVE_SCAN_DURATION_FACTOR_A2DP,
	SOFT_GEMINI_TEMP_PARAM_1,
	SOFT_GEMINI_TEMP_PARAM_2,
	SOFT_GEMINI_TEMP_PARAM_3,
	SOFT_GEMINI_TEMP_PARAM_4,
	SOFT_GEMINI_TEMP_PARAM_5,
	SOFT_GEMINI_PARAMS_MAX
} softGeminiParams;

typedef struct
{
  uint32   coexParams[SOFT_GEMINI_PARAMS_MAX];
  uint8    paramIdx;       /* the param index which the FW should update, if it equals to 0xFF - update all */ 
  uint8       padding[3];
} TSoftGeminiParams;


/******************************************************************************

    Name:   ACX_SG_CFG
    Desc:   This command instructs the WiLink to set the Soft Gemini (BT co-existence) 
            parameters to the desired values. 
    Type:   Configuration
	Access:	Write (Read For GWSI - disable for now)
    Length: 1
    
******************************************************************************/
typedef struct

{
    INFO_ELE_HDR
	
	TSoftGeminiParams softGeminiParams;
} ACXBluetoothWlanCoParamsStruct;
  
/******************************************************************************

    Name:   ACX_FM_COEX_CFG
    Desc:   This command instructs the WiLink to set the FM co-existence
            parameters to the desired values. 
    Type:   Configuration
	Access:	Write
    Length: 
    
******************************************************************************/
typedef struct

{
    INFO_ELE_HDR
	
    uint8   enable;                     /* enable(1) / disable(0) the FM Coex feature */

    uint8   swallowPeriod;              /* Swallow period used in COEX PLL swallowing mechanism,
                                           Range: 0-0xFF,  0xFF = use FW default
                                        */

    uint8   nDividerFrefSet1;           /* The N divider used in COEX PLL swallowing mechanism for Fref of 38.4/19.2 Mhz.  
                                           Range: 0-0xFF,  0xFF = use FW default
                                        */

    uint8   nDividerFrefSet2;           /* The N divider used in COEX PLL swallowing mechanism for Fref of 26/52 Mhz.
                                           Range: 0-0xFF,  0xFF = use FW default
                                        */

    uint16  mDividerFrefSet1;           /* The M divider used in COEX PLL swallowing mechanism for Fref of 38.4/19.2 Mhz.
                                           Range: 0-0x1FF,  0xFFFF = use FW default
                                        */

    uint16  mDividerFrefSet2;           /* The M divider used in COEX PLL swallowing mechanism for Fref of 26/52 Mhz.
                                           Range: 0-0x1FF,  0xFFFF = use FW default
                                        */

    uint32  coexPllStabilizationTime;   /* The time duration in uSec required for COEX PLL to stabilize.
                                           0xFFFFFFFF = use FW default
                                        */

    uint16  ldoStabilizationTime;       /* The time duration in uSec required for LDO to stabilize.
                                           0xFFFFFFFF = use FW default
                                        */

    uint8   fmDisturbedBandMargin;      /* The disturbed frequency band margin around the disturbed 
                                             frequency center (single sided). 
                                           For example, if 2 is configured, the following channels 
                                             will be considered disturbed channel: 
                                             80 +- 0.1 MHz, 91 +- 0.1 MHz, 98 +- 0.1 MHz, 102 +- 0.1 MHz
                                           0xFF = use FW default
                                        */

	uint8	swallowClkDif;              /* The swallow clock difference of the swallowing mechanism.
                                           0xFF = use FW default
                                        */

} ACXWlanFmCoexStruct;
  


/******************************************************************************

    Name:   ACX_TSF_INFO
    Type:   Operation
    Access: Read Only
    Length: 20

******************************************************************************/
typedef struct ACX_fwTSFInformation
{
    INFO_ELE_HDR
    uint32 CurrentTSFHigh;
    uint32 CurrentTSFLow;
    uint32 lastTBTTHigh;
    uint32 lastTBTTLow;
    uint8 LastDTIMCount;
    uint8  padding[3];  /* alignment to 32bits boundry   */
}ACX_fwTSFInformation_t;

 
/******************************************************************************

Name:   ACX_BET_ENABLE
Desc:   Enable or Disable the Beacon Early Termination module. In addition initialized the
        Max Dropped beacons parameter
Type:   Configuration
Access: Write 
Length: 6
Note:  
******************************************************************************/
typedef struct

{
    INFO_ELE_HDR
    uint8           Enable;                                     /* specifies if beacon early termination procedure is enabled or disabled: 0 – disabled, 1 – enabled */
    uint8           MaximumConsecutiveET;           /* specifies the maximum number of consecutive beacons that may be early terminated. After this number is reached 
                                                       at least one full beacon must be correctly received in FW before beacon ET resumes.  Legal range: 0 – 255 */
    uint8           padding[2];
}ACXBet_Enable_t;


/******************************************************************************

    Name:   DOT11_RX_MSDU_LIFE_TIME
    Type:   Operation
    Access: Write Only
    Length: 4
    
******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint32 RxMsduLifeTime; /* The maximum amount of time, in TU, that the WiLink */
                           /* should attempt to collect fragments of an MSDU before */
                           /* discarding them. */
                           /* The default value for this field is 512.*/
} dot11RxMsduLifeTime_t;


/******************************************************************************

    Name:   DOT11_CUR_TX_PWR
    Desc:   This IE indicates the maximum TxPower in Dbm/10 currently being used to transmit data.
    Type:   Operation
    Access: Write Only
    Length: 1
    
******************************************************************************/

typedef struct
{ 
    INFO_ELE_HDR
    uint8 dot11CurrentTxPower; /* the max Power in Dbm/10 to be used to transmit data.*/
    uint8  padding[3];  /* alignment to 32bits boundry   */
} dot11CurrentTxPowerStruct ;


/******************************************************************************

    Name:   DOT11_RX_DOT11_MODE
    Desc:   This IE indicates the current Rx Mode used by DSSS PHY.
    Type:   Configuration
    Access: Write Only
    Length: 4
    
******************************************************************************/
/*
Possible values for Rx DOT11 Mode are the following:
Value   Description
=====   ===========
3       11g - processing of both a and b packet formats is enabled
2       11b - processing of b packet format is enabled
1       11a - processing of a packet format is enabled
0       undefined
*/

typedef struct
{
    INFO_ELE_HDR
    uint32 dot11RxDot11Mode; /* refer to above table*/
} dot11RxDot11ModeStruct;


/******************************************************************************

    Name:   DOT11_RTS_THRESHOLD 
    Type:   Configuration
    Access: Write Only
    Length: 2

******************************************************************************/

typedef struct 
{
    INFO_ELE_HDR
    uint16  RTSThreshold; /* The number of octets in an MPDU, below which an */
                          /* RTS/CTS handshake is not performed.*/
    
    uint8  padding[2];  /* alignment to 32bits boundry   */
}dot11RTSThreshold_t;


/******************************************************************************

    Name:   DOT11_GROUP_ADDRESS_TBL
    Desc:   The variable lengths of MAC addresses that are define as listening for
            multicast. The field Number of groups identifies how many MAC Addresses 
            are relevant in that information element.
    Type:   Configuration
    Access: Write Only
    Length: up to 50 bytes

******************************************************************************/
#define ADDRESS_GROUP_MAX       (8)
#define ADDRESS_GROUP_MAX_LEN   (6 * ADDRESS_GROUP_MAX)
typedef struct 
{
    INFO_ELE_HDR
    uint8   fltrState;                           /* 1 - multicast filtering is enabled. */
                                                 /* 0 - multicast filtering is disabled.*/

    uint8   numOfGroups;                         /* number of relevant multicast */
                                                 /* addresses.*/

    uint8   padding[2];  /* alignment to 32bits boundary   */
    uint8   dataLocation[ADDRESS_GROUP_MAX_LEN]; /* table of MAC addresses.*/
}dot11MulticastGroupAddrStart_t;

/******************************************************************************

   ACX_CONFIG_PS_WMM (Patch for Wi-Fi Bug)

******************************************************************************/

typedef struct 
{    
    INFO_ELE_HDR
    uint32      ConfigPsOnWmmMode;  /* TRUE  - Configure PS to work on WMM mode - do not send the NULL/PS_POLL 
                                               packets even if TIM is set.
                                       FALSE - Configure PS to work on Non-WMM mode - work according to the 
                                               standard. */
} ACXConfigPsWmm_t;

/******************************************************************************

      
    Name:   ACX_SET_RX_DATA_FILTER
    Desc:   This IE configure one filter in the data filter module. can be used 
            for add / remove / modify filter.
    Type:   Filtering Configuration
    Access: Write Only
    Length: 4 + size of the fields of the filter (can vary between filters)

******************************************************************************/
/* data filter action */

#ifdef HOST_COMPILE

#define FILTER_DROP  0          /* Packet will be dropped by the FW and wont be delivered to the driver. */
#define FILTER_SIGNAL  1        /* Packet will be delivered to the driver. */
#define FILTER_FW_HANDLE  2     /* Packet will be handled by the FW and wont be delivered to the driver. */

#else

typedef enum {
    FILTER_DROP = 0,
    FILTER_SIGNAL  ,
    FILTER_FW_HANDLE, 
    FILTER_MAX  = 0xFF
}filter_enum;

#endif

#ifdef HOST_COMPILE
typedef uint8 filter_e;
#else
typedef filter_enum filter_e;
#endif

/* data filter command */
#define REMOVE_FILTER   0       /* Remove filter */
#define ADD_FILTER      1       /* Add filter */

/* limitation */
#define MAX_DATA_FILTERS 4
#define MAX_DATA_FILTER_SIZE 98

typedef struct 
{
    INFO_ELE_HDR
    uint8                command;   /* 0-remove, 1-add */
    uint8                index;     /* range 0-MAX_DATA_FILTERS */
    filter_e             action;    /* action: FILTER_DROP, FILTER_SIGNAL, FILTER_FW_HANDLE */  
    uint8                numOfFields; /* number of field in specific filter */
    uint8                FPTable;   /* filter fields starts here. variable size. */
} DataFilterConfig_t;

/******************************************************************************
      
    Name:   ACX_ENABLE_RX_DATA_FILTER
    Desc:   This IE disable / enable the data filtering feature. in case the
            featue is enabled - default action should be set as well.
    Type:   Filtering Configuration
    Access: Write Only
    Length: 2

******************************************************************************/

typedef struct  
{
    INFO_ELE_HDR
    uint8       enable;     /* 1 - enable, 0 - disable the data data filtering feature */
    filter_e    action;     /* default action that should be implemented for packets that wont
                               match any of the filters, or in case no filter is configured */
    uint8   padding[2];     /* alignment to 32bits boundary   */        
} DataFilterDefault_t;


/******************************************************************************
      
    Name:   ACX_GET_DATA_FILTER_STATISTICS
    Desc:   get statistics of the data filtering module.
    Type:   Statistics
    Access: Read Only
    Length: 20

******************************************************************************/

typedef struct 
{
    INFO_ELE_HDR
    uint32  unmatchedPacketsCount;                  /* number of packets didn't match any filter (when the feature was enabled). */
    uint32  matchedPacketsCount[MAX_DATA_FILTERS];  /* number of packets matching each of the filters */
} ACXDataFilteringStatistics_t;


#ifdef RADIO_SCOPE
/******************************************************************************

******************************************************************************

    Name:	ACX_RS_ENABLE
	Desc:   This command instructs the WiLink to set the Radio Scope functionality
	        state to either enable/disable. 
	Type:	Configuration
	Access:	Write Only
	Length: 1
	
******************************************************************************/
typedef struct
{
    INFO_ELE_HDR
	uint8   Enable; /* RadioScope feature will be enabled (1) or disabled(0) */
    uint8  padding[3];  /* alignment to 32 bits  */
} ACXRadioScopeEnableStruct;

/******************************************************************************

    Name:	ACX_RS_RX
	Desc:   This command instructs the WiLink to set the Radio Scope 
	        parameters to the desired values. 
	Type:	Configuration
	Access:	Read/Write 
	Length: 1

	We have the following available memory area:
		
			Information Element ID -		2 bytes
			Information Element Length -	2 bytes			
			
				Now the rest is MAX_CMD_PARAMS
				but 4 bytes must be subtracted
				because of the IE in Buffer.
			
	
******************************************************************************/
typedef struct
{	    
	uint16  service;
	uint16	length;
	uint8	channel;
	uint8	band;
	uint8	status;
	uint8   padding[1]; /*32 bit padding */
}RxPacketStruct;

typedef struct 
{		
    /*  We have the following available memory area:        */
    /*                                                      */
    /*  Information Element ID -        2 bytes             */
    /*  Information Element Length -    2 bytes             */
    /*  Number Of Packets in Buffer -    2 bytes            */
    /*                                                      */
    /*        Now the rest is MAX_CMD_PARAMS                */
    /*        but 2 bytes must be subtracted                */
    /*        because of the Number Of Packets in Buffer.   */
	RxPacketStruct packet[(MAX_CMD_PARAMS-2)/sizeof(RxPacketStruct)];
}RxCyclicBufferStruct;

typedef struct

{
    INFO_ELE_HDR
    /*uint8   padding[MAX_CMD_PARAMS-4]; */
	RxCyclicBufferStruct buf;
} ACXRadioScopeRxParamsStruct;

#endif /* RADIO_SCOPE */
/******************************************************************************
    Name:   ACX_KEEP_ALIVE_MODE
    Desc:   Set/Get the Keep Alive feature mode.
    Type:   Configuration
	Access:	Write
    Length: 4 - 1 for the mode + 3 for padding.

******************************************************************************/

typedef struct
{
INFO_ELE_HDR
    Bool_e  modeEnabled;
    uint8 padding [3];
}AcxKeepAliveMode;


/******************************************************************************

    Name:	ACX_SET_KEEP_ALIVE_CONFIG
    Desc:   Configure a KLV template parameters.
    Type:   Configuration
    Access: Write Only
    Length: 8

******************************************************************************/

typedef enum
{
    NO_TX = 0,
    PERIOD_ONLY
} KeepAliveTrigger_enum;

#ifdef HOST_COMPILE
typedef uint8 KeepAliveTrigger_e;
#else
typedef KeepAliveTrigger_enum KeepAliveTrigger_e;
#endif

typedef enum
{
    KLV_TEMPLATE_INVALID = 0,
    KLV_TEMPLATE_VALID,
    KLV_TEMPLATE_PENDING /* this option is FW internal only. host can only configure VALID or INVALID*/
} KeepAliveTemplateValidation_enum;

#ifdef HOST_COMPILE
typedef uint8 KeepAliveTemplateValidation_e;
#else
typedef KeepAliveTemplateValidation_enum KeepAliveTemplateValidation_e;
#endif

typedef struct
{
    INFO_ELE_HDR
	uint32 period; /*at range 1000-3600000 (msec). (To allow better range for debugging)*/
    uint8 index;
    KeepAliveTemplateValidation_e   valid;
    KeepAliveTrigger_e  trigger;
    uint8 padding;
} AcxSetKeepAliveConfig_t;

/*
 * BA sessen interface structure
 */
typedef struct
{
    INFO_ELE_HDR
    uint8 aMacAddress[6];           /* Mac address of: SA as receiver / RA as initiator */
    uint8 uTid;                     /* TID */
    uint8 uPolicy;                  /* Enable / Disable */
    uint16 uWinSize;                /* windows size in num of packet */
    uint16 uInactivityTimeout;      /* as initiator inactivity timeout in time units(TU) of 1024us / 
                                       as receiver reserved */
} TAxcBaSessionInitiatorResponderPolicy;

/******************************************************************************

    Name:	ACX_PEER_HT_CAP
	Desc:   Configure HT capabilities - declare the capabilities of the peer 
            we are connected to.
	Type:	Configuration
	Access:	Write Only
    Length: 

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint32 uHtCapabilites;      /* 
                                 * bit 0 – Allow HT Operation
                                 * bit 1 - Allow Greenfield format in TX
                                 * bit 2 – Allow Short GI in TX 
                                 * bit 3 – Allow L-SIG TXOP Protection in TX
                                 * bit 4 – Allow HT Control fields in TX. 
                                 *         Note, driver will still leave space for HT control in packets regardless 
                                 *         of the value of this field. FW will be responsible to drop the HT field 
                                 *         from any frame when this Bit is set to 0.
                                 * bit 5 - Allow RD initiation in TXOP. FW is allowed to initate RD. Exact policy 
                                 *         setting for this feature is TBD.
                                 *         Note, this bit can only be set to 1 if bit 3 is set to 1.
                                 */

     uint8  aMacAddress[6];     /*
                                 * Indicates to which peer these capabilities are relevant.
                                 * Note, currently this value will be set to FFFFFFFFFFFF to indicate it is 
                                 * relevant for all peers since we only support HT in infrastructure mode. 
                                 * Later on this field will be relevant to IBSS/DLS operation
                                 */

     uint8  uAmpduMaxLength;    /* 
                                 * This the maximum a-mpdu length supported by the AP. The FW may not 
                                 * exceed this length when sending A-MPDUs
                                 */

     uint8  uAmpduMinSpacing;   /* This is the minimal spacing required when sending A-MPDUs to the AP. */
     
} TAxcHtCapabilitiesIeFwInterface;

/* EHtCapabilitesFwBitMask mapping */
typedef enum
{
    FW_CAP_BIT_MASK_HT_OPERATION                      =  BIT_0,
    FW_CAP_BIT_MASK_GREENFIELD_FRAME_FORMAT           =  BIT_1,
    FW_CAP_BIT_MASK_SHORT_GI_FOR_20MHZ_PACKETS        =  BIT_2,
    FW_CAP_BIT_MASK_LSIG_TXOP_PROTECTION              =  BIT_3,
    FW_CAP_BIT_MASK_HT_CONTROL_FIELDS                 =  BIT_4,
    FW_CAP_BIT_MASK_RD_INITIATION                     =  BIT_5
} EHtCapabilitesFwBitMask;


/******************************************************************************

    Name:	ACX_HT_BSS_OPERATION
	Desc:   Configure HT capabilities - AP rules for behavior in the BSS.
	Type:	Configuration
	Access:	Write Only
    Length: 

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    uint8 uRifsMode;            /* Values: 0 – RIFS not allowed, 1 – RIFS allowed */
    uint8 uHtProtection;        /* Values: 0 – 3 like in spec */
    uint8 uGfProtection;        /* Values: 0 - GF protection not required, 1 – GF protection required */
    uint8 uHtTxBurstLimit;      /* Values: 0 – TX Burst limit not required, 1 – TX Burst Limit required */
    uint8 uDualCtsProtection;   /* 
                                 * Values: 0 – Dual CTS protection not required, 1 Dual CTS Protection required
                                 *             Note: When this value is set to 1 FW will protect all TXOP with RTS 
                                 *             frame and will not use CTS-to-self regardless of the value of the 
                                 *             ACX_CTS_PROTECTION information element 
                                 */
    uint8 padding[3];

} TAxcHtInformationIeFwInterface;

/******************************************************************************
 FwStaticData_t - information stored in command mailbox area after the Init 
                  process is complete

 Note:  This structure is passed to the host via the mailbox at Init-Complete
        without host request!!
        The host reads this structure before sending any configuration to the FW.
******************************************************************************/

typedef struct
{
	/* dot11StationIDStruct */
	uint8 dot11StationID[6]; /* The MAC address for the STA.*/
    uint8 padding[2];       /* alignment to 32bits boundry   */
	/* ACXRevision_t */
	char FWVersion[20];		/* The WiLink firmware version, an ASCII string x.x.x.x.x */
							/* that uniquely identifies the current firmware. */
							/* The left most digit is incremented each time a */
							/* significant change is made to the firmware, such as */
							/* WLAN new project.*/
							/* The second and third digit is incremented when major enhancements*/
							/* are added or major fixes are made.*/
							/* The fourth digit is incremented for each SP release */
                            /* and it indicants the costumer private branch */
							/* The fifth digit is incremented for each build.*/
		
    uint32 HardWareVersion; /* This 4 byte field specifies the WiLink hardware version. */
							/* bits 0  - 15: Reserved.*/
							/* bits 16 - 23: Version ID - The WiLink version ID  */
							/*              (1 = first spin, 2 = second spin, and so on).*/
							/* bits 24 - 31: Chip ID - The WiLink chip ID. */
        uint8 txPowerTable[NUMBER_OF_SUB_BANDS_E][NUM_OF_POWER_LEVEL]; /* Maximun Dbm in Dbm/10 units */
} FwStaticData_t;

/******************************************************************************



    ACX_TX_CONFIG_OPT

 

******************************************************************************/

typedef struct 
{
    INFO_ELE_HDR
    uint16  txCompleteTimeout;   /* Max time in msec the FW may delay frame Tx-Complete interrupt */
    uint16  txCompleteThreshold; /* Tx-Complete packets number which requires interrupting the host (0 = no buffering) */
} ACXTxConfigOptions_t;


/******************************************************************************

Name:	ACX_PWR_CONSUMPTION_STATISTICS
Desc:   Retrieve time statistics of the different power states.
Type:	Configuration
Access:	Read Only
Length: 20 

******************************************************************************/

// Power Statistics
typedef struct
{
    INFO_ELE_HDR
    uint32 awakeTimeCnt_Low;
    uint32 awakeTimeCnt_Hi;
    uint32 powerDownTimeCnt_Low;
    uint32 powerDownTimeCnt_Hi;
    uint32 elpTimeCnt_Low;
    uint32 elpTimeCnt_Hi;
    uint32 ListenMode11BTimeCnt_Low;
    uint32 ListenMode11BTimeCnt_Hi;
    uint32 ListenModeOFDMTimeCnt_Low;
    uint32 ListenModeOFDMTimeCnt_Hi;
}ACXPowerConsumptionTimeStat_t;


/******************************************************************************
    Name:   ACX_BURST_MODE
    Desc:   enable/disable burst mode in case TxOp limit != 0.
    Type:   Configuration
    Access:    Write
    Length: 1 - 2 for the mode + 3 for padding.

******************************************************************************/

typedef struct
{
INFO_ELE_HDR
    Bool_e  enable;
    uint8 padding [3];
}AcxBurstMode;


/******************************************************************************
    Name:   ACX_SET_RATE_MAMAGEMENT_PARAMS
    Desc:   configure one of the configurable parameters in rate management module.
    Type:   Configuration
    Access:    Write
    Length: 8 bytes

******************************************************************************/
typedef enum
{
    RATE_MGMT_RETRY_SCORE_PARAM,
	RATE_MGMT_PER_ADD_PARAM,
	RATE_MGMT_PER_TH1_PARAM,
	RATE_MGMT_PER_TH2_PARAM,
	RATE_MGMT_MAX_PER_PARAM,
	RATE_MGMT_INVERSE_CURIOSITY_FACTOR_PARAM,
	RATE_MGMT_TX_FAIL_LOW_TH_PARAM,
	RATE_MGMT_TX_FAIL_HIGH_TH_PARAM,
	RATE_MGMT_PER_ALPHA_SHIFT_PARAM,
	RATE_MGMT_PER_ADD_SHIFT_PARAM,
	RATE_MGMT_PER_BETA1_SHIFT_PARAM,
	RATE_MGMT_PER_BETA2_SHIFT_PARAM,
	RATE_MGMT_RATE_CHECK_UP_PARAM,
	RATE_MGMT_RATE_CHECK_DOWN_PARAM,
	RATE_MGMT_RATE_RETRY_POLICY_PARAM,
	RATE_MGMT_ALL_PARAMS = 0xff
} rateAdaptParam_enum;

#ifdef HOST_COMPILE
typedef uint8 rateAdaptParam_e;
#else
typedef rateAdaptParam_enum rateAdaptParam_e;
#endif

typedef struct
{
    INFO_ELE_HDR
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
}AcxRateMangeParams;

/******************************************************************************
    Name:   ACX_GET_RATE_MAMAGEMENT_PARAMS
    Desc:   read the configurable parameters of rate management module.
    Type:
    Access: read
    Length: 8 bytes

******************************************************************************/
typedef struct
{
    INFO_ELE_HDR
    int32  SNRCorrectionHighLimit;
    int32  SNRCorrectionLowLimit;
    int32  PERErrorTH;
    int32  attemptEvaluateTH;
    int32  goodAttemptTH;
    int32  curveCorrectionStep;
}AcxRateMangeReadParams;


/******************************************************************************

    Name:   ACX_SET_DCO_ITRIM_PARAMS    
    Desc:   Configure DCO Itrim operational parameters:               
            1. Enable/disable of the entire feature.                                     
            2. Moderation timeout (usec) - how much time to wait from last TX
            until DCO Itrim can be set low.
    Type:   Configuration
    Access: Write Only
    Length: 

******************************************************************************/

typedef struct
{
    INFO_ELE_HDR
    Bool_e enable;
    uint32 moderation_timeout_usec;
}ACXDCOItrimParams_t ;

#endif /* PUBLIC_INFOELE_H */

