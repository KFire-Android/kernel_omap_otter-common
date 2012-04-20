/*
 * public_commands.h
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

/**********************************************************************************************************************

  FILENAME:       public_commands.h

  DESCRIPTION:    Command definitions shared with host



***********************************************************************************************************************/
#ifndef PUBLIC_COMMANDS_H
#define PUBLIC_COMMANDS_H


#include "public_types.h"
#include "public_radio.h"

typedef enum
{
    CMD_INTERROGATE     = 1,    /*use this to read information elements*/
    CMD_CONFIGURE       = 2,    /*use this to write information elements*/
    CMD_ENABLE_RX       = 3,
    CMD_ENABLE_TX       = 4,
    CMD_DISABLE_RX      = 5,
    CMD_DISABLE_TX      = 6,
    CMD_SCAN            = 8,
    CMD_STOP_SCAN       = 9,

    CMD_START_JOIN      = 11,
    CMD_SET_KEYS        = 12,
    CMD_READ_MEMORY     = 13,
    CMD_WRITE_MEMORY    = 14,

    CMD_SET_TEMPLATE    = 19,
    CMD_TEST            = 23,

    CMD_NOISE_HIST      = 28,
    
    CMD_LNA_CONTROL     = 32,
    CMD_SET_BCN_MODE    = 33,

    CMD_MEASUREMENT      = 34,
    CMD_STOP_MEASUREMENT = 35,
    CMD_DISCONNECT       = 36,
    CMD_SET_PS_MODE      = 37,

    CMD_CHANNEL_SWITCH   = 38,
    CMD_STOP_CHANNEL_SWICTH = 39,

    CMD_AP_DISCOVERY     = 40,
    CMD_STOP_AP_DISCOVERY = 41,

    CMD_SPS_SCAN = 42,
    CMD_STOP_SPS_SCAN = 43,
    
    CMD_HEALTH_CHECK     = 45,     
    CMD_DEBUG            = 46, 
    CMD_TRIGGER_SCAN_TO  = 47,

    CMD_CONNECTION_SCAN_CFG        = 48,
    CMD_CONNECTION_SCAN_SSID_CFG   = 49,
    CMD_START_PERIODIC_SCAN        = 50,
    CMD_STOP_PERIODIC_SCAN         = 51,
    CMD_SET_STA_STATE              = 52,

NUM_COMMANDS,
    MAX_COMMAND_ID = 0xFFFF
} Command_enum;

#ifdef HOST_COMPILE
typedef uint16 Command_e;
#else
typedef Command_enum Command_e;
#endif


#ifdef HOST_COMPILE

#define     CMD_MAILBOX_IDLE               0
#define     CMD_STATUS_SUCCESS             1
#define     CMD_STATUS_UNKNOWN_CMD         2
#define     CMD_STATUS_UNKNOWN_IE          3
#define     CMD_STATUS_REJECT_MEAS_SG_ACTIVE    11
#define     CMD_STATUS_RX_BUSY             13
#define     CMD_STATUS_INVALID_PARAM       14
#define     CMD_STATUS_TEMPLATE_TOO_LARGE  15 
#define     CMD_STATUS_OUT_OF_MEMORY       16 
#define     CMD_STATUS_STA_TABLE_FULL      17
#define     CMD_STATUS_RADIO_ERROR         18
#define     CMD_STATUS_WRONG_NESTING       19
#define     CMD_STATUS_TIMEOUT             21 /* Driver internal use.*/
#define     CMD_STATUS_FW_RESET            22 /* Driver internal use.*/
#define     MAX_COMMAND_STATUS             MAX_POSITIVE16

#else

typedef enum
{
    CMD_MAILBOX_IDLE              =  0,
    CMD_STATUS_SUCCESS            =  1,
    CMD_STATUS_UNKNOWN_CMD        =  2,
    CMD_STATUS_UNKNOWN_IE         =  3,
    CMD_STATUS_REJECT_MEAS_SG_ACTIVE =  11,
    CMD_STATUS_RX_BUSY            = 13,
    CMD_STATUS_INVALID_PARAM      = 14,
    CMD_STATUS_TEMPLATE_TOO_LARGE = 15, 
    CMD_STATUS_OUT_OF_MEMORY      = 16, 
    CMD_STATUS_STA_TABLE_FULL     = 17,
    CMD_STATUS_RADIO_ERROR        = 18,
    CMD_STATUS_WRONG_NESTING      = 19,
    CMD_STATUS_TIMEOUT            = 21, /* Driver internal use.*/
    CMD_STATUS_FW_RESET           = 22, /* Driver internal use.*/
    MAX_COMMAND_STATUS            = MAX_POSITIVE16
} CommandStatus_enum;

#endif

#ifdef HOST_COMPILE
typedef uint16 CommandStatus_e;
#else
typedef CommandStatus_enum CommandStatus_e;
#endif

#define MAX_CMD_PARAMS 610

#define DEBUG_INDICATOR      0x8000    

typedef struct
{
    Command_e cmdID;
    CommandStatus_e cmdStatus;
    uint8 parameters[MAX_CMD_PARAMS];
} Command_t;


/******************************************************************************

    ID:       CMD_INTERROGATE
    Desc:     This command requests an information element from the WiLink. The 
              interface for this command is somewhat different from other commands 
              since the interface is bi-directional and asymmetric. 
              The host structure consists of the Command ID, a Command Status 
              (returned by WiLink) place holder, and the Information Element Heading
              (ID and expected length).
              The response to that command is a buffer of the information element's 
              actual values returned by the WiLink just after the command is issued.
              The response to that command is a buffer of the information element's 
              actual values returned by the WiLink just after the command is issued.
    Params:   InfoElement_t - see below.
              
    
******************************************************************************/
/*
Description of InfoElement structure - defined in "public_infoele.h"
offset  length  source  description
======  ======  ======  ===========
0       2       host    Information Element ID - contains the ID of the requested 
                        information element (refer to InfoElement_enum in 
                        pblic_infoele.h). In response to this command, the WiLink 
                        writes the requested information element to the response area 
                        for the command mailbox.
2       4       wilink  Length - the length of the response (different for each IE
                        according to definitions in public_infoele.h).
4       Length  wilink  IE payload according to definition in public_infoele.h.
*/



/******************************************************************************

    ID:       CMD_CONFIGURE
    Desc:     This command configures an information element in the WiLink. 
    Params:   InfoElement_t - see below.

******************************************************************************/
/*
Description of InfoElement structure - defined in "public_infoele.h"
offset  length  source  description
======  ======  ======  ===========
0       2       host    Information Element ID - contains the ID of the requested 
                        information element (refer to InfoElement_enum in 
                        pblic_infoele.h). In response to this command, the WiLink 
                        writes the requested information element to the response area 
                        for the command mailbox.
2       4       host    Length - the length of the response (different for each IE
                        according to definitions in public_infoele.h).
4       Length  host    IE payload according to definition in public_infoele.h.
*/


/******************************************************************************

    ID:       CMD_ENABLE_RX
    Desc:     This command enables the normal reception of frames. 
    Params:   Channel Number - this field indicates the radio channel on which to
                               receive data. This parameter also sets the channel on
                               which to transmit. The last channel number used, 
                               regardless of the order in which the ENABLE_RX and 
                               ENABLE_TX commands are issued, is the channel number 
                               for both RX and TX. This command must be issued after 
                               the host has set all necessary configuration elements 
                               appropriately.
    
******************************************************************************/



/******************************************************************************

    ID:       CMD_ENABLE_TX
    Desc:     This command enables the normal transmission of frames.  
    Params:   Channel Number - this field indicates the radio channel on which to
                               transmit data. This parameter also sets the channel on
                               which to receive. The last channel number used, 
                               regardless of the order in which the ENABLE_RX and 
                               ENABLE_TX commands are issued, is the channel number 
                               for both RX and TX. This command must be issued after
                               the host has set all necessary configuration elements
                               appropriately.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_DISABLE_RX
    Desc:     This command disables the normal reception of packets over the 
              Baseband interface. 
    Params:   None
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_DISABLE_TX
    Desc:     This command disables the normal transmission of frames. 
    Params:   None.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_SCAN
    Desc:     This command instructs the WiLink to scan for BSS/IBSSs. The host 
              may perform either an active scan or a passive scan. During an active
              scan, the WiLink transmits a probe request on the specified channel(s) 
              and then listens for beacon/probe responses. During a passive scan, the 
              WiLink monitors the specified channel(s) for beacons.
              The WiLink sends SCAN_COMPLETE event to notify the host when it has 
              completed a scan.
    Params:   ScanParameters_t - see below
    
******************************************************************************/
/*
Offset  Length  Definition
0       8       RX filters for Scan (refer to ACXRxConfigStruct)
8       2       Scan options (Band select, Voice mode and Scan type = Active/Passive)
10      1       NumChannels 
11      1       Number of Probe requests (used for Active scan)
12      2       Probe request rate & modulation
14      1       AC trigger (for Voice mode only)
15      1       SSID length
16      32      SSID string (Null terminated)
48      2       Channel [0] ScanMinDuration
50      2       Channel [0] ScanMaxDuration 
52      6       Channel [0] BSSID (4 bytes LOW and 2 bytes HIGH)
58      1       Channel [0].bit0-3: Early Termination count. Bit 4-5: Condition
59      1       Channel [0] TX power level for Scan (0 means do not change - other values:1-5)
60      1       Channel [0] Channel  
61      3       Channel [0] Reserved
64-404  340     Optional Channel [1] - Channel [15] - same format as Channel [0] fields above.
*/

/* Defines for Rx "ConfigOptions".*/
/* Only bits 2-10 can be configured by the Driver".*/
#define CFG_RX_SERIAL           BIT_0    /* 0 = use parallel interface,         1 = use serial interface from ACX101- not valid.*/
#define CFG_RX_RAW              BIT_1    /* 1 = write all data from baseband to frame buffer including PHY header.*/
#define CFG_RX_FCS              BIT_2    /* 1 = write FCS to end of frame in memory, 0 = do not write FCS to memory.*/
#define CFG_RX_ALL_GOOD         BIT_3    /* promiscuous mode, receive all good frames.*/
#define CFG_UNI_FILTER_EN       BIT_4    /* local MAC address filter enable.*/
#define CFG_BSSID_FILTER_EN     BIT_5    /* BSSID filter enable.*/
#define CFG_MC_FILTER_EN        BIT_6    /* 0 = receive all multicast,          1 = use one or both multicast address filters.*/
#define CFG_MC_ADDR0_EN         BIT_7    /* 1 = receive frames from mc_addr0,   0 = do not use this filter.*/
#define CFG_MC_ADDR1_EN         BIT_8    /* 1 = receive frames from mc_addr1,   0 = do not use this filter .*/
#define CFG_BC_REJECT_EN        BIT_9    /* 0 = receive all broadcast,          1 = filter all broadcast.*/
#define CFG_SSID_FILTER_EN      BIT_10   /* SSID Filter Enable.*/
#define CFG_RX_INT_FCS_ERROR    BIT_11   /* 1 = give rx complete interrupt for FCS errors.*/
#define CFG_RX_INT_ENCRYPTED    BIT_12   /* 1 = only give rx header interrupt if frame is encrypted.*/
#define CFG_RX_WR_RX_STATUS     BIT_13   /* 0 = do not write three status words, 1 = write three receive status words to top of rx'd MPDU.*/
#define CFG_RX_FILTER_NULTI     BIT_14   /* 1 = filter multicast/broadcast frame if SA matchs local MAC addr->.*/
#define CFG_RX_RESERVE          BIT_15   /* reserve.*/
#define CFG_RX_TIMESTAMP_TSF    BIT_16   /* 1 = sample frame's' arrival time in 32bits TSF, 0 = write it in MAC time stamp.*/


/* Defines for Rx "FilterOptions".*/
/* The rx filter enables control what type of receive frames will be rejected or received by the rx hardware*/
/* 1 = frame is written to memory,*/
/* 0 = not written to memory, rejected.*/
#define CFG_RX_RSV_EN       BIT_0  /* reserved types and subtypes.*/
#define CFG_RX_RCTS_ACK     BIT_1  /* rts, cts, ack frames.*/
#define CFG_RX_PRSP_EN      BIT_2  /* probe response.*/
#define CFG_RX_PREQ_EN      BIT_3  /* probe request.*/
#define CFG_RX_MGMT_EN      BIT_4  /* type = management.*/
#define CFG_RX_FCS_ERROR    BIT_5  /* frames with FCS errors.*/
#define CFG_RX_DATA_EN      BIT_6  /* type = data.*/
#define CFG_RX_CTL_EN       BIT_7  /* type = control.*/
#define CFG_RX_CF_EN        BIT_8  /* contention free frames.*/
#define CFG_RX_BCN_EN       BIT_9  /* beacons.*/
#define CFG_RX_AUTH_EN      BIT_10 /* authentication, deauthentication.*/
#define CFG_RX_ASSOC_EN     BIT_11 /* association related frames (all 5 subtypes  assoc req/resp,*/


typedef struct
{
    uint32          ConfigOptions;
    uint32          FilterOptions;
} ACXRxConfigStruct;


/* ScanOptions bit mask field.*/
#define SCAN_ACTIVE         0
#define SCAN_PASSIVE        1   /* 1 = passive scan, 0 = active scan*/
/* #define SCAN_5GHZ_BAND      2  */  /* 1 = scan channel list in 5 Ghz band, 0 = scan channel list in 2.4 Ghz band*/ 
#define TRIGGERED_SCAN      2   /* 1 = Triggered scan, 0 = Normal scan*/
#define SCAN_PRIORITY_HIGH  4   /* 1 = High priority scan, 0 = Low priority scan*/
 
typedef uint8 TidTrigger_t;

/* General scan parameters.*/
typedef struct
{
    ACXRxConfigStruct  rxCfg;         /* Rx filter to be used for each channel scan. */
                                      /* The BSSID filter enable will be set (by the */
                                      /* scan process) to ON for a specific channel if*/
                                      /* the BSSID of this channel is a unicast address.*/
                                      /* Otherwise it will be set to OFF (Refer to */
                                      /* ACXRxConfig IE in public_infoele.h).*/

    uint16             scanOptions;   /* This bitwise field indicates the scan options. */
                                      /* Bits [4:15] are reserved. */
                                      /* Bits [0:3] are defined as follows: */
                                      /* Scan Type (bit 0) - When this bit is set, the */
                                      /*  WiLink performs a passive scan. When this bit*/
                                      /*  is cleared, the WiLink performs an active scan. */
                                      /* Band Select (bit 1) - When this bit is set, the*/
                                      /*  WiLink scans the specified channels in the */
                                      /*  5GHz band. When this bit is cleared, the */
                                      /*  WiLink scans the specified channels in the */
                                      /*  2.4GHz band. */
                                      /* Voice mode (bit 2) - When this bit is set, */
                                      /*  the request is for a voice scan. When this bit*/
                                      /*  is cleared, the request is for a normal scan. */
                                      /* Scan priority (bit 3) - When this bit is set, */
                                      /*  the request is for a high priority scan. When*/
                                      /*  this bit is cleared, the request is for a low*/
                                      /*  priority scan.*/
        
    uint8              numChannels;   /* Number of scan channels in the list (minimum is*/
                                      /* 1, maximum is 30).*/

    uint8              numOfProbRqst; /* This field indicates the number of probe */
                                      /* requests to send per channel, in active scan. */
    
    EHwRateBitFiled    txdRateSet;    /* This EHwRateBitFiled format field specifies the rate and */
                                      /* modulation to transmit the probe request during*/
	                                  /* an active scan. It is not used for passive scans.*/
   
    TidTrigger_t       tidTrigger;    /* used for TidTriggered scan only.*/

    uint8              ssidLength;    /* This field specifies the size of the SSID, */
                                      /* which can be up to 32 bytes long. If this field*/
                                      /* equals to zero, SSID filter is not applied. */
    
    uint8              padding1[2];    /* in order to align */ 


    uint32             ssidStr[8];    /* This field specifies the SSID packets from that*/
                                      /* are relevant for the Scan result. The WiLink*/
                                      /* uses this information to filter beacon, probe*/
                                      /* response frames (if the SSID length field of */
                                      /* this command structure is not zero) */

    RadioBand_e         band;         /* Band to scan */

    uint8               useSsidList;  /* Whether to use SSID list (configured seperately)
                                         when transmitting probe-requests */

    uint8               scanTag;      /* Scan results tag */

    uint8               padding2;     /* in order to align */ 

} BasicScanParameters_t;



#define SCAN_ET_COND_MASK  0x30
#define SCAN_ET_COUNT_MASK 0x0F

#define SCAN_MAX_NUM_OF_CHANNELS 24

/* Early Termination condition (bits 4-5) - This field can have one of the following */
/* values (note that bits 0-3 indicates Early Termination count): */
typedef enum 
{
    ET_COND_DISABLE = 0x00,          /* Disable - No early termination condition.*/

    ET_COND_BEACON  = 0x10,          /* Beacon only. When this value is selected, the */
                                     /* Early Termination count field specifies the */
                                     /* maximum number of beacons to collect before */
                                     /* ending a scan. */

    ET_COND_PROBE_RESP = 0x20,       /* Probe responses only. When this value is */
                                     /* selected, the Early Termination count field */
                                     /* specifies the maximum number of probe responses*/
                                     /* to collect before ending a scan. */

    ET_COND_BEACON_PROBE_RESP = 0x30,/* Beacon/probe response. When this value is */
                                     /* selected, the Early Termination count field */
                                     /* specifies the maximum number of beacons or probe*/
                                     /* responses to collect before ending a scan. */
                                     
    ET_COND_INVALID = 0xFF
} ETCondition_enum;

#ifdef HOST_COMPILE
typedef uint8 ETCondition_e;
#else
typedef ETCondition_enum ETCondition_e;
#endif


typedef uint8 ETCondCount_t;



#define PROCESS_SCAN_IS_HIGH(pScanParameters) ((pScanParameters)->basicScanParameters.scanOptions & SCAN_PRIORITY_HIGH)
#define PROCESS_SCAN_IS_LOW(pScanParameters) ((PROCESS_SCAN_IS_HIGH(pScanParameters)) == 0)


/* Per-Channel scan parameters.*/
typedef struct
{

    uint32        scanMinDuration;    /* For active scans, this field specifies the */
                                      /* minimum amount of time, in time units (TUs), */
                                      /* to wait for a frame on a channel. This */
                                      /* parameter is not used for passive scans. The*/
                                      /*  value can range from 0 to 65535 TUs */
                                      /* (67.1 seconds). */

    uint32        scanMaxDuration;    /* For active scans, this field specifies the */
                                      /* maximum amount of time, in time units (TUs), */
                                      /* to wait for a probe response on a channel.*/
                                      /* For passive scans, this field specifies the */
                                      /* amount of time, in time units (TUs), to listen*/
                                      /* on a channel. The value can range from 0 to */
                                      /* 65535 TUs (67.1 seconds). */
    

    uint32        bssIdL;             /* 32 LSBits of BSSID of the AP to scan for. */
                                      /* If scanning on this channel any BSSID, this */
                                      /* field shall be set to broadcast BSSID. */

    uint16        bssIdH;             /* 16 MSBits of BSSID of the AP to scan for.*/

    ETCondCount_t ETCondCount;        /* bit 0-3: Early Termination count - This field */
                                      /*          defines the maximum number of beacons*/
                                      /*          or probe responses or both (according*/
                                      /*          to condition) to collect before ending*/
                                      /*          a scan.*/
    
                                      /* Bit 4-5: Early Termination Condition (refer */
                                      /*          to ETCondition_enum).*/
                
    uint8         txPowerAttenuation; /* TX power level to be used per channel scanned. */
                                      /* If 0, leave normal TX power level for this */
                                      /* channel. Range: 0 - 20 [dB].*/
    
    Channel_e     channel;            /* Channel number to scan, valid range 0-255 */
                                      /* (1-14 for 802.11b). */

    Bool_e        dfsCandidate;       /* FW internal use only! */
    Bool_e        activityDetected;   /* FW internal use only! */

    uint8         padding[1];             /* for alignment to 32 bits boundry*/
} BasicScanChannelParameters_t;

/* The Scan command structure.*/
typedef struct
{
    BasicScanParameters_t basicScanParameters; /* refer to BasicScanParameters_t */
                                               /* definition*/

    BasicScanChannelParameters_t basicScanChannelParameters[SCAN_MAX_NUM_OF_CHANNELS];
} ScanParameters_t;

/*****************************************************************************

    ID:       CMD_TRIGGER_SCAN_TO
    Desc:     This Command will configure the enhanced Trigger Scan Timeout 
                information.
              To use legacy Trigger Scan, configure the parameter to 0
    Params:   None
    
******************************************************************************/
typedef struct 
{
    uint32  slicedScanTimeOut;          /* 0 - Split Scan Disable
                                           any other value will represent the timeout 
                                           for each channel "mini scan" in uSec */
}enhancedTriggerTO_t;



/*****************************************************************************

    ID:       CMD_STOP_SCAN
    Desc:     This command instructs the WiLink to terminate any scan in progress. 
              After processing this command, the WiLink returns to its previous state
              (the state before the scan was started) and generates the SCAN_COMPLETE
              information message. 
    Params:   None
    
******************************************************************************/



/******************************************************************************

    ID:       CMD_START_JOIN
    Desc:     This command instructs the WiLink to either join a BSS or IBSS, or 
              start an IBSS. When the device has joined the BSS or IBSS the Join 
              Complete event is raised to the host.  
    Params:   StartJoinRequest_t - see below.

******************************************************************************/

/* 
 * Join command  control bit mask field:
 */



/* Cyclic counter in range 1-7, advanced on every Join command to enable the FW distinguish 
     between connection sessions and drop obsolete Tx packets. */
#define JOIN_CMD_CTRL_TX_SESSION       (BIT_3 | BIT_2 | BIT_1)

/* When this bit is set,the firmware will flush all Tx frames in the pipe and will not transmit them. */
#define JOIN_CMD_CTRL_TX_FLUSH         BIT_7 

/* Offsets of the above fields */
#define JOIN_CMD_CTRL_OFFSET_TX_SESSION             1
#define JOIN_CMD_CTRL_OFFSET_TX_FLUSH                      7 


typedef enum
{
    BSS_TYPE_IBSS = 0,
    BSS_TYPE_STA_BSS = 2,
    BSS_TYPE_AP_BSS = 3,
    MAX_BSS_TYPE = 0xFF
} BssType_enum;

#ifdef HOST_COMPILE
typedef uint8 BSS_e;
#else
typedef BssType_enum BSS_e;
#endif

#define MAX_SSID_STR_LEN_BYTESX4 8

typedef struct
{
    uint32            bssIdL;        /* This field indicates the 32 LSBits of the MAC*/
                                     /* address of the BSS to join. */
                                     /* Note: To correctly generate beacon frames, the */
                                     /* byte order of the BSS ID field must be */
                                     /* reversed. for example, if the MAC address of */
                                     /* the AP is 00 7E 99 11 22 33, program the BSS */
                                     /* ID field as 33 22 11 99 7E 00.*/
        
    uint16            bssIdH;        /* This field indicates the 16 MSBits of the MAC*/
                                     /* address of the BSS to join. */

    uint16            beaconInterval;/* This field specifies the time between target */
                                     /* beacon transmission times (TBTTs), in time */
                                     /* units (TUs). Valid values are 1 to 1024.*/
    
    ACXRxConfigStruct rxFilter;      /* This filed is the Rx filter configuration for*/
                                     /* the device while connected to the BSS or IBSS.*/
                                     /* This setting is overridden in case of a */
                                     /* measurement or a scan activity and is reset */
                                     /* after these activities end.*/

    EHwRateBitFiled   basicRateSet;  /* For 802.11b, this field specifies the control*/
                                     /* response frame rate for the BSS or IBSS (that*/
                                     /* is, the BSSBasicRateSet parameter in the */
                                     /* 802.11 Specification). The WiLink uses this */
                                     /* field to determine the rate at which to */
                                     /* transmit control frame responses (such as ACK */
	                                 /* or CTS frames). */
    
    uint8             dtimInterval;  /* This field specifies the number of beacon */
                                     /* intervals between DTIM beacon frames. The host*/
                                     /* is only required to set this field when the */
                                     /* BSS Type is infrastructure BSS (STA) or AP. */
                                     /* For an independent BSS, the host should set */
                                     /* this field to 1.*/
    
    BSS_e             bssType;       /* bits 0-2: This bitwise field specifies the type */
                                     /*  of BSS to start or join (Refer to BssType_enum). */
                                     /* bit 4: Band - The radio band in which to join*/
                                     /*  or start.*/
                                     /*  0 - 2.4GHz band    */
                                     /*  1 - 5GHz band*/
                                     /* bits 3, 5-7: Reserved*/

    Channel_e         channelNumber; /* This field specifies the channel number of the*/
                                     /* BSS to join or start. Valid values are 1 to 14. */
                                     /* If the specified channel is not allowed in the*/
                                     /* regulatory domain, the command is rejected and*/
                                     /* the status code 0x0005 is returned in the */
                                     /* Command Status field.*/
    
    uint8             ssidLength;    /* This field specifies the size of the SSID, which*/
                                     /* can be up to 32 bytes long.*/
    
    uint32              ssidStr[MAX_SSID_STR_LEN_BYTESX4];
                                     /* This field specifies the SSID of the BSS to */
                                     /* start or join. The WiLink uses this information*/
                                     /* to filter beacon, probe response and probe */
                                     /* request frames (if configured to do so in bit 10*/
                                     /* in the Receive Configuration field of the */
                                     /* ACXRxConfig information element). */
                                     /* It also uses this information to determine if a*/
                                     /* probe response should be transmitted in */
                                     /* response to a received probe request.*/
    
    uint8             ctrl;          /* Join command control field (refer to */
                                     /* the JOIN_CMD_CTRL... specified above).*/

    uint8             reserved[3];
} StartJoinRequest_t;


/******************************************************************************

    ID:       CMD_SET_KEYS
    Desc:     The host issues this command to manage the WEP key cache in the WiLink. 
              The host can issue this command during the configuration or operation 
              phase.  
    Params:   SetKey_t - see below.

******************************************************************************/

#define NUM_ACCESS_CATEGORIES_COPY 4 

#define MAX_KEY_SIZE 32

typedef enum
{
    KEY_ADD_OR_REPLACE = 1,             /* Add or replace a key in the WEP cache*/
    KEY_REMOVE         = 2,             /* Remove a key from the WEP cache*/
    KEY_SET_ID         = 3,             /* Set Key ID*/
    MAX_KEY_ACTION     = MAX_POSITIVE16 /* force this enum to be uint16*/
} KeyAction_enum;

#ifdef HOST_COMPILE
typedef uint16 KeyAction_e;
#else
typedef KeyAction_enum KeyAction_e;
#endif


/*
Key Size+Key Data table (valid value)
KeyType_enum  Key Type                    Valid Key Size    Key Data Field Format
============  ========                    ==============    =====================
0x00          WEP default key             5, 13, 29         Key Size bytes of key data

0x01          WEP key mapping key         5, 13, 29         Key Size bytes of key data

0x04          AES Group Key               16                16 bytes of key data

0x05          AES Pairwise Key            16                16 bytes of key data

0x0A          TKIP and MIC Group Key      32                16 bytes of TKIP key data
                                                        8 bytes of Rx MIC key data
                                                        8 bytes of Tx MIC key data

0x0B          TKIP and MIC Pairwise Key   32                16 bytes of TKIP key data
                                                        8 bytes of Rx MIC key data
                                                        8 bytes of Tx MIC key data

0x0C          GEM and MIC Group Key      32                 16 bytes of GEM key data
                                                            16 bytes of MIC key data

0x0D          GEM and MIC Pairwise Key   32                 16 bytes of GEM key data
                                                            16 bytes of MIC key data
                                                            
*/


typedef enum
{
  CIPHER_SUITE_NONE = 0,
  CIPHER_SUITE_WEP  = 1,
  CIPHER_SUITE_TKIP = 2,
  CIPHER_SUITE_AES  = 3,
  CIPHER_SUITE_GEM  = 4,
  MAX_KEY_TYPE
} CipherSuite_enum;

#ifdef HOST_COMPILE
typedef uint8 CipherSuite_e;
#else
typedef CipherSuite_enum CipherSuite_e;
#endif

typedef enum
{
    NO_KEY            =  0,
    KEY_SIZE_WEP_64   =  5,
    KEY_SIZE_WEP_128  = 13,
    KEY_SIZE_WEP_256  = 29,
    KEY_SIZE_TKIP     = MAX_KEY_SIZE
} KeySize_enum;                      /* WEP keysizes reflect 3 bytes appended from IV.*/

#ifdef HOST_COMPILE
typedef uint8 KeySize_e;
#else
typedef KeySize_enum KeySize_e;
#endif


typedef struct
{
    uint8 addr[MAC_ADDR_SIZE]; /* This field specifies the MAC address of the station to*/
                               /* add or remove from the WEP key cache. This field is */
                               /* ignored if a WEP default key is being added or removed.*/
    
    KeyAction_e action;        /* This field specifies the action to be performed.*/
                               /* Refer to KeyAction_enum.*/

    uint16      reserved;
    KeySize_e   keySize;       /* This field indicates the size of the key in bytes */
                               /* being added. Valid values are listed in the Valid Key */
                               /* Size column in the above "Key Size+Key Data table". */

    CipherSuite_e   type;          /* This field indicates the type of key being added.*/
                               /* Valid values are listed in the Value column in the*/
                               /* KeyType_enum.*/

    uint8       ssidProfile;   /* This field indicates the SSID profile for which the */
                               /* key is set.*/
    uint8       id;            /* Key ID - For TKIP and AES key types, this field */
                               /* indicates the value that should be inserted into the*/
                               /* KeyID field of frames transmitted using this key */
                               /* entry. For WEP default key types, this field indicates*/
                               /* the ID of the key to add or remove. */
                               /* For WEP key mapping key types, this field is ignored.*/
                               /* Valid values for this field are 0 to 3.*/
    
    uint8       reserved2[6];  
    uint8       key[MAX_KEY_SIZE];
                               /* This field holds the security key data to add to the*/
                               /* STA table. The format of this field varies depending*/
                               /* on the type field. The format of this field for each*/
                               /* key type is described in the Key Data Field Format */
                               /* column in the "Key Size+Key Data table", above.*/

    uint16      AcSeqNum16[NUM_ACCESS_CATEGORIES_COPY]; 
                               /* This field indicates the lower part of the PN\IV */
                               /* sequence number that is used, for the four Access*/
                               /* Categories.*/

    uint32      AcSeqNum32[NUM_ACCESS_CATEGORIES_COPY]; 
                               /* This field indicates the higher part of the PN\IV */
                               /* sequence number that is used, for four Access */
                               /* Categories.*/
    
} SetKey_t;


/******************************************************************************

    ID:       CMD_READ_MEMORY
    Desc:     The host issues this command to read the WiLink device 
              memory/registers. 
    Params:   ReadWriteCommand_t - see below.
    Note:     The Base Band address has special handling (16 bits registers and
              addresses). For more information, see the hardware specification.

******************************************************************************/
/******************************************************************************

    ID:       CMD_WRITE_MEMORY
    Desc:     The host issues this command to write the WiLink device memory/registers. 
    Params:   ReadWriteCommand_t - see below.
    Note:     The Base Band address has special handling (16 bits registers and
              addresses). For more information, see the hardware specification.

******************************************************************************/

#define MAX_READ_SIZE 256

typedef struct
{
    uint32 addr;                 /* The address of the memory to read from or write to.*/
    uint32 size;                 /* The amount of data in bytes to read from or write */
                                 /* to the WiLink device.*/
    uint8  value[MAX_READ_SIZE]; /* The actual value read from or written to the Wilink.*/
                                 /* The source of this field is the Host in WRITE */
                                 /* command or the Wilink in READ command.*/
} ReadWriteCommand_t;


/******************************************************************************

    ID:       CMD_BEACON
    Desc:     This command specifies the contents of the beacon template stored in 
              the WiLink. 
    Params:   PktTemplate_t - see below.

******************************************************************************/

/******************************************************************************

    ID:       CMD_PROBE_RESP
    Desc:     This command specifies the contents of the probe response template
              stored in the WiLink.  
    Params:   PktTemplate_t - see below.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_NULL_DATA
    Desc:     This command specifies the contents of the Null data template
              stored in the WiLink. 
    Params:   PktTemplate_t - see below.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_PROBE_REQ
    Desc:     This command specifies the contents of the probe request template
              stored in the WiLink.  
    Params:   PktTemplate_t - see below.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_PS_POLL
    Desc:     This command specifies the contents of the PS-poll template
              stored in the WiLink.  
    Params:   PktTemplate_t - see below.
    
******************************************************************************/


/******************************************************************************

    ID:       CMD_QOS_NULL_DATA
    Desc:     This command specifies the contents of the QOS-Null template
              stored in the WiLink.  
    Params:   PktTemplate_t - see below.
    
******************************************************************************/
/* Template command data structure.*/
typedef enum
{
    TEMPLATE_NULL_DATA = 0,
    TEMPLATE_BEACON,        
    CFG_TEMPLATE_PROBE_REQ_2_4,
    CFG_TEMPLATE_PROBE_REQ_5,
    TEMPLATE_PROBE_RESPONSE,
    TEMPLATE_QOS_NULL_DATA,
    TEMPLATE_PS_POLL,
    TEMPLATE_KLV,
	TEMPLATE_DISCONNECT, 
    TEMPLATE_PROBE_REQ_2_4, /*for firmware internal use only*/
    TEMPLATE_PROBE_REQ_5, /*for firmware internal use only*/
	TEMPLATE_BAR, /*for firmware internal use only*/
    TEMPLATE_CTS, /* For CTS-to-self (FastCTS) mechanism for BT/WLAN coexistence (SoftGemini). */
    TEMPLATE_ARP_RSP, /* Template for Automatic ARP reply by FW */

    MAX_NUM_OF_TEMPLATES = 0xff
} TemplateType_enum;

#ifdef HOST_COMPILE
typedef uint8 TemplateType_e;
#else
typedef TemplateType_enum TemplateType_e;
#endif

#define MAX_TEMPLATES_SIZE 252  /* according to max size of FW memblock */
#define RATE_MASK_UNSPECIFIED   0    /* Let the FW select the rates mask internally */

/* 
aflag definition:
bit field       description
=========       ===========
0               Truncate - If set, then attempts to send a frame stop when the total 
                valid per-rate attempts have been exhausted; 
                otherwise transmissions will continue at the lowest available rate 
                until the appropriate one of the Short Retry Limit, Long Retry Limit, 
                dot11MaxTransmitMsduLifetime, or MAX TX Life Time (in ACXTIDConfig), 
                if supported and supplied, is exhausted.
1               Preamble Override - Indicates if the preamble type should be used in TX. 
2               Preamble Type - The type of the preamble to be used by the policy. 
                0 - long preamble, 
                1 - short preamble. 
3-7             Reserved
*/

/* definition of single rate policy*/
typedef struct 
{

    uint32              enabledRates;  				  /* A Bit Mask which indicates which Rates are enabled */
                                                      /* The rates bitmap is defined in EHwBitRate. */
                                                      /* If set to RATE_MASK_UNSPECIFIED, the FW sets it internally */

    uint8               shortRetryLimit;              /* The dot11ShortRetryLimit used */
                                                      /* for Tx retries.*/

    uint8               longRetryLimit;               /* The dot11LongRetryLimit used */
                                                      /* for Tx retries.  */
    
    uint8               aflags;                       /* Flags controlling attributes */
                                                      /* of the transmission. */
                                                      /* see above description for the */
                                                      /* structure of this field.*/

    uint8               reserved;
}txAttrClass_t;
     
/* Template command data structure.*/
typedef struct 
{
    uint16 len;
    TemplateType_e templateType;
    uint8 index;    /* relevant only for KLV_TEMPLATE type */
    txAttrClass_t templateTxAttribute;
    uint8  templateStart[MAX_TEMPLATES_SIZE]; 
} PktTemplate_t;


/******************************************************************************

    ID:       CMD_NOISE_HIST
    Desc:     This command starts/stops the noise histogram measurements.
    Params:   NoiseHistRequest_t - see below.

******************************************************************************/
#define NOISE_HIST_LEN 8

typedef enum 
{
    NOISE_HIST_STOP,
    NOISE_HIST_START,
    NOISE_HIST_INVALID = MAX_POSITIVE16 /* Force to be 16 bits enum*/
} NoiseHistMode_enum;

#ifdef HOST_COMPILE
typedef uint16 NoiseHistMode_e;
#else
typedef NoiseHistMode_enum NoiseHistMode_e;
#endif

typedef struct
{
    NoiseHistMode_e mode;             /* Start or stop the FW engine. */
                                      /* Possible values are 1 (Start) and 0 (Stop).*/

    uint16 sampleIntervalUSec;        /* The time interval in usec between measurements.*/
                                      /* Valid values are between 100us (default) and */
                                      /* 2ms (with 100us jumps). This parameter is */
                                      /* relevant only when Mode is Start (1).*/
        
    uint8  thresholds[NOISE_HIST_LEN];/* An array of eight 8 bit thresholds. The FW */
                                      /* takes noise measurements, once every */
                                      /* SampleIntervalUSec interval. If the measured */
                                      /* noise level is between the threshold[X] and */
                                      /* threshold[X 1], then the FW increments the */
                                      /* noise histogram counter[X]. */
                                      /* The counters are read via the */
                                      /* ACXNoiseHistogramResults IE. This parameter is */
                                      /* relevant only when Mode is Start (1).*/
} NoiseHistRequest_t;


/******************************************************************************

    ID:       CMD_LNA_CONTROL
    Desc:     This command controls the LNA state. 
    Params:   LNAControl_t - see below.

******************************************************************************/

typedef enum
{
    LNA_MODE_MANUAL,    /* 0: The LNA is set to manual mode and is turned off.*/
    LNA_MODE_AUTO,      /* 1: The LNA is set to automatic mode.*/
    LNA_MODE_INVALID = 0xFF
} LnaMode_enum;

#ifdef HOST_COMPILE
typedef uint8 LnaMode_e;
#else
typedef LnaMode_enum LnaMode_e;
#endif

typedef struct
{
    LnaMode_e LNAControlField; /* refer to LnaMode_enum*/
    uint8     padding[3];      /* for alignment to 32 bits boundry*/
} LNAControl_t;


/******************************************************************************

    ID:       CMD_MEASUREMENT
    Desc:     This command instructs the WiLink device to begin a basic channel 
              load measurement on the specified channel. When the measurement 
              process actually starts running the WilLink device will raise the 
              Measurement Started event. When the measurement process completes as
              a result of the end of the measurement duration or a STOP_MEASUREMENT
              command, the WilLink device will raise a Measurement Complete event. 
    Params:   MeasurementParameters_t - see below.

******************************************************************************/
typedef struct 
{
    ACXRxConfigStruct rxFilter; /* This field is the Rx filter configuration for the */
                                /* device while the measurement process is running. */
                                /* When the process ends the previous Rx filter */
                                /* configuration is reset. The filter configuration is*/
                                /* composed of two 32 bit registers. When they are set*/
                                /* to 0xFFFFFFFF the Rx filter configuration is not*/
                                /* changed.*/
    
    uint32 duration;            /* Specifies the measurement process duration in */
                                /* microseconds. The value of 0 means infinite duration*/
                                /* in which only a STOP_MEASUREMENT command can*/
                                /* stop the measurement process.*/
    
    Channel_e channel;          /* Channel number on which the measurement is performed,*/
                                /* valid range 0-255 (1-14 for 802.11b).*/
    
    RadioBand_e band;           /* Specifies the band to which the channel belongs. */
                                /* 0 - 2.4GHz */
                                /* 1 - 5GHz , */
                                /* 0xFF - Current band.*/

    uint8 scanTag;              /* results tag */

    uint8 padding[1];           /* for alignment to 32 bits boundry*/
} MeasurementParameters_t;

/******************************************************************************

    ID:       CMD_STOP_MEASUREMENT
    Desc:     This command instructs the WiLink to terminate any measurement in 
              progress. After processing this command, the WiLink returns to its 
              previous state (the state before the measurement was started) and 
              generates the Measurment Complete event. 
    Params:   None.

******************************************************************************/

/******************************************************************************

    ID:       CMD_DISCONNECT
    Desc:     This command instructs the WiLink device to stop all BSS or IBSS activity.
              The device will cancel all of its TSF dependent events and activities. 
              Power Save dependent activities are an exception to this, therefore 
              the host must exit Power Save mode by issuing the SET_PS_MODE command 
              before calling this command. When this command is complete the Disconnect
              Complete event is raised to the host and the WiLink device is allowed 
              to enter the configured low power state. 
    Params:   DisconnectParameters_t - see below.
    
******************************************************************************/

typedef enum
{
    DISCONNECT_IMMEDIATE,    /* Disconnect without sending any frame */
    DISCONNECT_DE_AUTH,      /* Send deauthentication frame upon disconnecting */
    DISCONNECT_DIS_ASSOC     /* Send disassociation frame upon disconnecting */
} DisconnectType_enum;

#ifdef HOST_COMPILE
typedef uint8 DisconnectType_e;
#else
typedef DisconnectType_enum DisconnectType_e;
#endif



typedef struct
{
    ACXRxConfigStruct rxFilter;
    uint16 disconnectReason;
    DisconnectType_e   disconnectType; 
    uint8 padding[1];  /* for alignment to 32 bits boundry */
} DisconnectParameters_t;




/******************************************************************************

    ID:       CMD_SET_PS_MODE
    Desc:     This command turns ON/OFF Power save protocol on the WiLink. 
              After HW configuration, FW sends Null data packet to the AP with Power 
              Management bit set accordingly to the field "Mode" of this command 
              structure. After processing this command, the FW generates the 
              PS_COMPLETE event.
    Params:   PSModeParameters_t - see below.

******************************************************************************/

typedef enum
{
    STATION_ACTIVE_MODE,
    STATION_POWER_SAVE_MODE
} StationPSMode_enum;

#ifdef HOST_COMPILE
typedef uint8 StationPowerSaveMode_e;
#else
typedef StationPSMode_enum StationPowerSaveMode_e;
#endif


typedef struct
{
    StationPowerSaveMode_e mode;         /* This field specifies the future Power save*/
                                         /* protocol mode of the system. */
                                         /* When set, Power save protocol is enabled. */
                                         /* When cleared, Power save protocol is */
                                         /* disabled (refer to StationPSMode_enum).*/
    
    uint8                  needToSendNullData;
    uint8 numberOfRetries;               /* This field specifies the maximum allowed */
                                         /* number of retries of the Null data packet */
                                         /* that FW will send after switching the */
                                         /* Power Save Protocol mode.*/

    uint8 hangOverPeriod;                /* This field specifies the hangover period, */
                                         /* which is the time in TUs during which the */
                                         /* WiLink remains awake after sending an MPDU */
                                         /* with the Power Save bit set, indicating that*/
                                         /* the station is to go into Power Save mode. */
                                         /* Setting bit 0 does not affect the hangover */
                                         /* period.*/
    
    EHwRateBitFiled rateToTransmitNullData; /* This EHwBitRate format field specifies the rate and */
                                         /* modulation to transmit the Null data packet*/
	                                     /* to the AP. */
    
} PSModeParameters_t;

/******************************************************************************

    ID:       CMD_CHANNEL_SWITCH
    Desc:     This command instructs the WiLink to switch serving channel at the given
              time. Once the channel switch is performed, the Channel Switch Complete
              event is raised to the host.  
    Params:   ChannelSwitchParameters_t - see below.

******************************************************************************/
typedef struct
{
    Channel_e channel;  /* The new serving channel.*/
    uint8 switchTime;   /* Relative time of the serving channel switch in TBTT units.*/
    Bool_e txSuspend;   /* 1: Suspend TX till switch time; */
                        /* 0: Do not suspend TX*/
    Bool_e flush;       /* 1: Flush TX at switch time; */
                        /* 0: Do not flush*/
    
} ChannelSwitchParameters_t;

/******************************************************************************

    ID:       CMD_STOP_CHANNEL_SWICTH
    Desc:     This command instructs the WiLink device to cancel performing a 
              pending channel switch event command. 
    Params:   None.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_AP_DISCOVERY
    Desc:     This command instructs the WiLink device to perform an AP discovery 
              measurement on a single channel. This command can only be issued after 
              a measurement process has been started by the WiLink device as a result
              of a previous Measurement command. The Measurement command specifies the 
              channel on which the AP discovery is performed. Once the "AP discovery" 
              measurement is completed either by a STOP_AP_DISCOVERY command or when 
              the duration has expired, it will send an "AP discovery complete event" 
              to the host.  
    Params:   ApDiscoveryParameters_t - see below.

******************************************************************************/


typedef struct
{
    ACXRxConfigStruct rxFilter; /* This field is the Rx filter configuration for the */
                                /* device while the AP Discovery process is running. */
                                /* When the process ends the previous Rx filter */
                                /* configuration is reset. The filter configuration is*/
                                /* composed of two 32 bit registers. When they are set */
                                /* to 0xFFFFFFFF the Rx filter configuration is not */
                                /* changed.*/
    
    uint32 scanDuration;        /* This field specifies the amount of time, in time*/
                                /* units (TUs), to perform the AP discovery. The value*/
                                /* can range from 0 to 65535 TUs (67.1 seconds). */
    
    uint16 scanOptions;         /* This field specifies whether the AP discovery is */
                                /* performed by an active scan or a passive scan. */
                                /* 0 - ACTIVE, 1 - PASSIVE.*/
    
    uint8  numOfProbRqst;       /* This field indicates the number of probe requests to*/
                                /* send per channel, when active scan is specified. */
                                /* Note: for XCC measurement this value should be set */
                                /*       to 1.*/
    
    uint8 txPowerAttenuation;   /* TX power level to be used for sending probe requests*/
                                /* when active scan is specified. */
                                /* If 0, leave normal TX power level for this channel. */

    EHwRateBitFiled txdRateSet; /* This EHwBitRate format field specifies the rate and modulation*/
                                /* to transmit the probe request when an active scan is*/
	                            /* specified. */

} ApDiscoveryParameters_t;

/******************************************************************************

    ID:       CMD_STOP_AP_DISCOVERY
    Desc:     This command instructs the WiLink to terminate the AP Discovery 
              measurement in progress. After processing this command, the WiLink 
              returns to its previous state  and generates the AP Discovery Complete
              Event. 
    Params:   None.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_SPS_SCAN
    Desc:     This command instructs the WiLink to perform a scheduled passive 
              scan for BSS/IBSSs. The WiLink monitors the specified channel(s) 
              for beacons. The WiLink sends Scheduled Scan Complete event to notify
              the host when it has completed a scan. 
    Params:   InfoElement_t - see below.

******************************************************************************/
/* Scheduled - General scan parameters.*/
typedef struct
{
    ACXRxConfigStruct rxCfg;         /* Rx filter to be used for each channel scan. */
                                     /* The BSSID filter enable will be set (by the scan*/
                                     /* process) to ON for a specific channel if the*/
                                     /* BSSID of this channel is a unicast address. */
                                     /* Otherwise it will be set to OFF (Refer to */
                                     /* ACXRxConfigStruct).*/
    
    uint32            scanCmdTime_h; /* This filed is the latest 32 MSBits of TSF known*/
                                     /* at the time the SPS command was issued. When the*/
                                     /* scan process is about to begin, this value is */
                                     /* used to determine if the AP has performed a */
                                     /* recovery by comparing this value to the current*/
                                     /* TSF. (An AP that has performed a recovery should*/
                                     /* have a lower TSF then the one that was saved).*/
    
    uint32            scanCmdTime_l; /* This filed is the latest 32 LSBits of TSF known*/
                                     /* at the time the SPS command was issued. */

    uint16            scanOptions;   /* This bitwise field indicates the scan options. */
                                     /* 0 - ACTIVE, 1 - PASSIVE.*/
    
    uint8             numChannels;   /* Number of scan channels in the list (minimum */
                                     /* (minimumis 1, maximum is 30).*/
    
    RadioBand_e       band;          /* Band to scan */
    uint8             scanTag;       /* results tag */
} ScheduledGeneralParameters_t;


/* Scheduled - Per-Channel scan parameters.*/
typedef struct
{
    uint32 scanStartTime;      /* Duration in microseconds of the scan on this channel */
                               /* (Scan could be aborted before this duration in case of*/
                               /* early termination condition met on the channel). */
    
    uint32 scanMaxDuration;    /* Lower 4 bytes of TSF time in microseconds when the */
                               /* scan should start listening on the desired channel. */

    uint32 bssIdL;             /* 32 LSBits of BSSID of the AP to scan for. If scanning */
                               /* on this  channel any BSSID, this field shall be set */
                               /* to broadcast BSSID. */
    
    uint16 bssIdH;             /* 16 MSBits of BSSID of the AP to scan for. */

    ETCondCount_t ETCondCount; /* bit 0-3: Early Termination count - This field */
                               /*          defines the maximum number of beacons*/
                               /*          or probe responses or both (according*/
                               /*          to condition) to collect before ending*/
                               /*          a scan.*/
    
                               /* Bit 4-5: Early Termination Condition (refer */
                               /*          to ETCondition_enum).*/
    Channel_e     channel;     /* Channel number to scan, valid range 0-255 */
                               /* (1-14 for 802.11b).*/
} ScheduledChannelParameters_t;


/* The Scheduled Scan command structure.*/
typedef struct
{
    ScheduledGeneralParameters_t scheduledGeneralParameters;
    ScheduledChannelParameters_t scheduledChannelParameters[SCAN_MAX_NUM_OF_CHANNELS];
} ScheduledScanParameters_t;

/******************************************************************************

    ID:       CMD_STOP_SPS_SCAN
    Desc:     This command instructs the WiLink to terminate a currently running 
              SPS or a pending SPS.. After processing this command, the WiLink 
              returns to its previous state and generates the Scheduled Scan 
              Complete Event. 
    Params:   None.

******************************************************************************/

/******************************************************************************

    ID:       CMD_HEALTH_CHECK
    Desc:     This command instructs the WiLink to raise a MAC status event 
              which contains the current FCS Error counter. 
    Params:   None.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_DEBUG
    Desc:      
    Params:   debugCommnad_t - see below.

******************************************************************************/
#define MAX_DEBUG_PARAMETERS 10

typedef struct
{
    uint32  id;
    uint32  params[MAX_DEBUG_PARAMETERS];
} debugCommnad_t;


/******************************************************************************

    ID:       CMD_CONNECTION_SCAN_CFG
    Desc:     This command configures the periodic connection scan feature 
    Params:   ConnectionScanParameters_t followed by active and passive channel
              lists (variable size structure)
    
******************************************************************************/

typedef enum
{
    BSS_INDEPENDENT                     = 0,
    BSS_INFRASTRUCTURE                  = 1,
    BSS_ANY                             = 2,
    BSS_MAX = 0xFF
} ScanBssType_enum;

#ifdef HOST_COMPILE
typedef uint8 ScanBssType_e;
#else
typedef ScanBssType_enum ScanBssType_e;
#endif

typedef enum
{
    SCAN_SSID_FILTER_TYPE_ANY = 0,
    SCAN_SSID_FILTER_TYPE_SPECIFIC = 1,
    SCAN_SSID_FILTER_TYPE_LIST = 2,
    SCAN_SSID_FILTER_TYPE_LIST_FILTER_DISABLED = 3,
    MAX_SCAN_SSID_FILTER_TYPE = 0xFF
} ScanSsidFilterType_enum;

#ifdef HOST_COMPILE
typedef uint8 ScanSsidFilterType_e;
#else
typedef ScanSsidFilterType_enum ScanSsidFilterType_e;
#endif

#define CONN_SCAN_MAX_CHANNELS_BG  14
#define CONN_SCAN_MAX_CHANNELS_J    4
#define CONN_SCAN_MAX_CHANNELS_A   23
#define CONN_SCAN_MAX_CHANNELS_ALL_BANDS        ((CONN_SCAN_MAX_CHANNELS_BG) + (CONN_SCAN_MAX_CHANNELS_A) + (CONN_SCAN_MAX_CHANNELS_J))
#define CONN_SCAN_MAX_NUM_OF_CYCLES_INTERVALS   (16)        /* Maximum number of configured inter-cycle intervals */

#define CONN_SCAN_MAX_BAND                      (3)         /* Maximum number of bands in scan configuration */

#define CONN_SCAN_DFS_CANDIDATE_CHANNEL     BIT_0
#define CONN_SCAN_DFS_ENABLED_CHANNEL       BIT_1

typedef struct
{
    uint16       scanMinDuration;     /* Min duration (in ms)*/
    uint16       scanMaxDuration;     /* Max duration (in ms)*/
    uint16       passiveScanDuration; /* Duration to use for passive scans (in ms)*/
    uint8        channel;             /* channel number (channel_e) */
    uint8        txPowerLevelDbm;    /* TX power level in dbm */
    uint8        channelFlags;        /* BIT 0: DFS channel */
                                      /* BIT 1: DFS enabled (to be included in active scan) */
    uint8        padding[3];
} ConnScanChannelInfo_t;

typedef struct
{
    uint32                  cycleIntervals[CONN_SCAN_MAX_NUM_OF_CYCLES_INTERVALS];      /* Intervals between each scan cycle */
    int8                    rssiThreshold;          /* RSSI threshold */
    int8                    snrThreshold;           /* SNR threshold */
    uint8                   maxNumOfCycles;         /* number of cycles to run */
    uint8                   reportThreshold;        /* Report after N results are received */
    uint8                   terminateOnReport;      /* Terminate after report */
    uint8                   resultsTag;             /* Tag for filtered scan results */
    ScanBssType_e           bssType;                /* BSS type to filter (0 - Infra, 1 - IBSS, 2 - Any) */
    ScanSsidFilterType_e    ssidFilterType;         /* SSID filter Type (0 - Any, 1 - specified in command, 2 - use SSID list, 3 - use SSID list no filter) */
    uint8                   ssidLength;             /* SSID Length (if ssidType is specified in command) */
    uint8                   ssid[32];               /* SSID (if ssidType is specified in command) */
    uint8                   numProbe;               /* Number of probe requests to transmit per (hidden) SSID per channel */

    uint8                   numOfPassive[CONN_SCAN_MAX_BAND];
    uint8                   numOfActive[CONN_SCAN_MAX_BAND];
    uint8                   numOfDfs;
    uint8                   padding[ 3 ];
    ConnScanChannelInfo_t   channelList[CONN_SCAN_MAX_CHANNELS_ALL_BANDS];
} ConnScanParameters_t;

/******************************************************************************

    ID:       CMD_CONNECTION_SCAN_SSID_CFG
    Desc:     This command configures the SSID filter list to be used in the
              periodic connection scan
    Params:   None.
    
******************************************************************************/

#define CONN_SCAN_MAX_NUMBER_OF_SSID_ENTRIES        (8)

typedef enum
{
    SCAN_SSID_TYPE_PUBLIC = 0,
    SCAN_SSID_TYPE_HIDDEN = 1,
    MAX_SCAN_SSID_TYPE = 0xFF
} ScanSsidType_enum;

#ifdef HOST_COMPILE
typedef uint8 ScanSsidType_e;
#else
typedef ScanSsidType_enum ScanSsidType_e;
#endif

typedef struct
{
    ScanSsidType_e      ssidType;
    uint8               ssidLength; 
    uint8               ssid[32];
} ConnScanSSID_t;


typedef struct
{
    uint8               numOfSSIDEntries;
    ConnScanSSID_t      SSIDList[CONN_SCAN_MAX_NUMBER_OF_SSID_ENTRIES];
} ConnScanSSIDList_t;


/******************************************************************************

    ID:       CMD_START_PERIODIC_SCAN
    Desc:     This command instructs the WiLink to start a periodic scan
              process (either connection-scan or discovery)
    Params:   None.
    
******************************************************************************/

typedef struct
{
    uint8   scanTag;
    uint8   uReserved[3];
} PeriodicScanTag;

/******************************************************************************

    ID:       CMD_STOP_PERIODIC_SCAN
    Desc:     This command instructs the WiLink to stop a periodic scan
              process (either connection-scan or discovery)
    Params:   None.
    
******************************************************************************/

/******************************************************************************

    ID:       CMD_SET_STATUS
    Desc:     This command instructs the station state
    Params:   None.
    
******************************************************************************/

#define STA_STATE_CONNECTED 1

typedef struct
{
    uint8   staState;
    uint8   pad[3];
} SetStaState_t; 

/* PLT public definitions*/
#define TEST_CONTINUOUS 0x04       /* transmit frames contimuously */


/*This typedef corresponds to the Fig 1 Frame Format for PER test given in PER test doc.*/
#define PER_MODE_TX 1
#define PER_MODE_RX 2
#define INFRA   0x2         /* BSS is in Infrastructure mode */
#define LOCAL_MEM_LAST      0x02000000
#define PBCC_MODULATION_MASK 0x80
#define OFDM_MODULATION_MASK 0x40
#define BAND_SELECT_5GHZ     0x10
#define CTL_PREAMBLE         0x01



#endif /* PUBLIC_COMMANDS_H*/
