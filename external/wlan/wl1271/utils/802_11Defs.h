/*
 * 802_11Defs.h
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


/****************************************************************************
 *
 *   MODULE:  802_11Defs.h
 *   PURPOSE: Contains 802.11 defines/structures
 *
 ****************************************************************************/

#ifndef _802_11_INFO_DEFS_H
#define _802_11_INFO_DEFS_H

#include "tidef.h"
#include "osDot11.h"
#ifdef XCC_MODULE_INCLUDED
#include "osDot11XCC.h"
#endif

#define DOT11_OUI_LEN                       4
#define DOT11_COUNTRY_STRING_LEN            3
#define DOT11_MAX_SUPPORTED_RATES           32

/* Maximum size of beacon or probe-request information element */
#define DOT11_WSC_PROBE_REQ_MAX_LENGTH 80


typedef enum
{
    DOT11_B_MODE    = 1,
    DOT11_A_MODE    = 2,
    DOT11_G_MODE    = 3,
    DOT11_DUAL_MODE = 4,
    DOT11_N_MODE    = 5,

    DOT11_MAX_MODE

} EDot11Mode;


/* FrameControl field of the 802.11 header  */

/**/
/* bit 15    14     13     12     11     10      9     8     7-4     3-2      1-0*/
/* +-------+-----+------+-----+-------+------+------+----+---------+------+----------+*/
/* | Order | WEP | More | Pwr | Retry | More | From | To | Subtype | Type | Protocol |*/
/* |       |     | Data | Mgmt|       | Frag | DS   | DS |         |      | Version  |*/
/* +-------+-----+------+-----+-------+------+------+----+---------+------+----------+*/
/*     1      1      1     1      1       1      1     1       4       2        2*/


#define DOT11_FC_PROT_VERSION_MASK   ( 3 << 0 )
#define DOT11_FC_PROT_VERSION        ( 0 << 0 )

#define DOT11_FC_TYPE_MASK           ( 3 << 2 )
typedef enum
{
  DOT11_FC_TYPE_MGMT         = ( 0 << 2 ),
  DOT11_FC_TYPE_CTRL         = ( 1 << 2 ),
  DOT11_FC_TYPE_DATA         = ( 2 << 2 )
} dot11_Fc_Type_e;

#define DOT11_FC_SUB_MASK           ( 0x0f << 4 )
typedef enum
{
  /* Management subtypes */
  DOT11_FC_SUB_ASSOC_REQ     = (    0 << 4 ),
  DOT11_FC_SUB_ASSOC_RESP    = (    1 << 4 ),
  DOT11_FC_SUB_REASSOC_REQ   = (    2 << 4 ),
  DOT11_FC_SUB_REASSOC_RESP  = (    3 << 4 ),
  DOT11_FC_SUB_PROBE_REQ     = (    4 << 4 ),
  DOT11_FC_SUB_PROBE_RESP    = (    5 << 4 ),
  DOT11_FC_SUB_BEACON        = (    8 << 4 ),
  DOT11_FC_SUB_ATIM          = (    9 << 4 ),
  DOT11_FC_SUB_DISASSOC      = (   10 << 4 ),
  DOT11_FC_SUB_AUTH          = (   11 << 4 ),
  DOT11_FC_SUB_DEAUTH        = (   12 << 4 ),
  DOT11_FC_SUB_ACTION        = (   13 << 4 ),

  /* Control subtypes */
  DOT11_FC_SUB_BAR                    = (    8 << 4 ),
  DOT11_FC_SUB_BA                     = (    9 << 4 ),
  DOT11_FC_SUB_PS_POLL                = (   10 << 4 ),
  DOT11_FC_SUB_RTS                    = (   11 << 4 ),
  DOT11_FC_SUB_CTS                    = (   12 << 4 ),
  DOT11_FC_SUB_ACK                    = (   13 << 4 ),
  DOT11_FC_SUB_CF_END                 = (   14 << 4 ),
  DOT11_FC_SUB_CF_END_CF_ACK          = (   15 << 4 ),

  /* Data subtypes */
  DOT11_FC_SUB_DATA                   = (    0 << 4 ),
  DOT11_FC_SUB_DATA_CF_ACK            = (    1 << 4 ),
  DOT11_FC_SUB_DATA_CF_POLL           = (    2 << 4 ),
  DOT11_FC_SUB_DATA_CF_ACK_CF_POLL    = (    3 << 4 ),
  DOT11_FC_SUB_NULL_FUNCTION          = (    4 << 4 ),
  DOT11_FC_SUB_CF_ACK                 = (    5 << 4 ),
  DOT11_FC_SUB_CF_POLL                = (    6 << 4 ),
  DOT11_FC_SUB_CF_ACK_CF_POLL         = (    7 << 4 ),
  DOT11_FC_SUB_DATA_QOS               = (    8 << 4 ),
  DOT11_FC_SUB_DATA_NULL_QOS          = (   12 << 4 )
} dot11_Fc_Sub_Type_e;

#define  DOT11_FC_TYPESUBTYPE_MASK    ( DOT11_FC_TYPE_MASK | DOT11_FC_SUB_MASK )
typedef enum
{
  DOT11_FC_ASSOC_REQ           = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_ASSOC_REQ           ),
  DOT11_FC_ASSOC_RESP          = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_ASSOC_RESP          ),
  DOT11_FC_REASSOC_REQ         = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_REASSOC_REQ         ),
  DOT11_FC_REASSOC_RESP        = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_REASSOC_RESP        ),
  DOT11_FC_PROBE_REQ           = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_PROBE_REQ           ),
  DOT11_FC_PROBE_RESP          = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_PROBE_RESP          ),
  DOT11_FC_BEACON              = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_BEACON              ),
  DOT11_FC_ATIM                = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_ATIM                ),
  DOT11_FC_DISASSOC            = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_DISASSOC            ),
  DOT11_FC_AUTH                = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_AUTH                ),
  DOT11_FC_DEAUTH              = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_DEAUTH              ),
  DOT11_FC_ACTION              = ( DOT11_FC_TYPE_MGMT | DOT11_FC_SUB_ACTION              ),
  DOT11_FC_PS_POLL             = ( DOT11_FC_TYPE_CTRL | DOT11_FC_SUB_PS_POLL             ),
  DOT11_FC_RTS                 = ( DOT11_FC_TYPE_CTRL | DOT11_FC_SUB_RTS                 ),
  DOT11_FC_CTS                 = ( DOT11_FC_TYPE_CTRL | DOT11_FC_SUB_CTS                 ),
  DOT11_FC_ACK                 = ( DOT11_FC_TYPE_CTRL | DOT11_FC_SUB_ACK                 ),
  DOT11_FC_CF_END              = ( DOT11_FC_TYPE_CTRL | DOT11_FC_SUB_CF_END              ),
  DOT11_FC_CF_END_CF_ACK       = ( DOT11_FC_TYPE_CTRL | DOT11_FC_SUB_CF_END_CF_ACK       ),
  DOT11_FC_DATA                = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_DATA                ),
  DOT11_FC_DATA_CF_ACK         = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_DATA_CF_ACK         ),
  DOT11_FC_DATA_CF_POLL        = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_DATA_CF_POLL        ),
  DOT11_FC_DATA_CF_ACK_CF_POLL = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_DATA_CF_ACK_CF_POLL ),
  DOT11_FC_DATA_NULL_FUNCTION  = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_NULL_FUNCTION       ),
  DOT11_FC_CF_ACK              = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_CF_ACK              ),
  DOT11_FC_CF_POLL             = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_CF_POLL             ),
  DOT11_FC_CF_ACK_CF_POLL      = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_CF_ACK_CF_POLL      ),
  DOT11_FC_DATA_QOS            = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_DATA_QOS            ),
  DOT11_FC_DATA_NULL_QOS       = ( DOT11_FC_TYPE_DATA | DOT11_FC_SUB_DATA_NULL_QOS       )
} dot11_Fc_Type_Sub_Type_e;

typedef enum
{
  DOT11_FC_TO_DS               = ( 1 << 8  ),
  DOT11_FC_FROM_DS             = ( 1 << 9  ),
  DOT11_FC_MORE_FRAG           = ( 1 << 10 ),
  DOT11_FC_RETRY               = ( 1 << 11 ),
  DOT11_FC_PWR_MGMT            = ( 1 << 12 ),
  DOT11_FC_MORE_DATA           = ( 1 << 13 ),
  DOT11_FC_WEP                 = ( 1 << 14 ),
  DOT11_FC_ORDER               = ( 1 << 15 )
} dot11_Fc_Other_e;

typedef enum
{
  DOT11_CAPABILITY_ESS               = ( 1 ),
  DOT11_CAPABILITY_IESS              = ( 1 << 1 ),
  DOT11_CAPABILITY_CF_POLLABE        = ( 1 << 2 ),
  DOT11_CAPABILITY_CF_POLL_REQ       = ( 1 << 3 ),
  DOT11_CAPABILITY_PRIVACY           = ( 1 << 4 ),
  DOT11_CAPABILITY_PREAMBLE          = ( 1 << 5 ),
  DOT11_CAPABILITY_PBCC              = ( 1 << 6 ),
  DOT11_CAPABILITY_AGILE             = ( 1 << 7 )
} dot11_Capability_e;

#define  DOT11_FC_TO_DS_SHIFT        8
#define  DOT11_FC_FROM_DS_SHIFT      9
#define  DOT11_FC_MORE_FRAG_SHIFT   10
#define  DOT11_FC_RETRY_SHIFT       11
#define  DOT11_FC_PWR_MGMT_SHIFT    12
#define  DOT11_FC_MORE_DATA_SHIFT   13
#define  DOT11_FC_WEP_SHIFT         14
#define  DOT11_FC_ORDER_SHIFT       15

#define IS_WEP_ON(fc)       ((1 << DOT11_FC_WEP_SHIFT) & (fc))
#define IS_DATA(fc)         (((DOT11_FC_TYPE_MASK) & (fc)) == DOT11_FC_TYPE_DATA)
#define IS_CTRL(fc)         (((DOT11_FC_TYPE_MASK) & (fc)) == DOT11_FC_TYPE_CTRL)
#define IS_MGMT(fc)         (((DOT11_FC_TYPE_MASK) & (fc)) == DOT11_FC_TYPE_MGMT)
#define IS_LEGACY_DATA(fc)  (((DOT11_FC_TYPESUBTYPE_MASK) & (fc)) == DOT11_FC_DATA)
#define IS_AUTH(fc)         (((DOT11_FC_AUTH) & (fc)) == DOT11_FC_AUTH)
#define IS_QOS_FRAME(fc)    ((((fc) & (DOT11_FC_TYPESUBTYPE_MASK)) == DOT11_FC_DATA_QOS)   ||   \
                             (((fc) & (DOT11_FC_TYPESUBTYPE_MASK)) == DOT11_FC_DATA_NULL_QOS) )
#define IS_HT_FRAME(fc)     ((fc) & (DOT11_FC_ORDER))



#define TUs_TO_MSECs(x)     (((x) << 10) / 1000)

#define TIME_STAMP_LEN  8

/* SequenceControl field of the 802.11 header */
/**/
/* bit    15 - 4           3 - 0*/
/* +-------------------+-----------+*/
/* |  Sequence Number  | Fragment  |*/
/* |                   |  Number   |*/
/* +-------------------+-----------+*/
/*         12                4*/

typedef enum
{
  DOT11_SC_FRAG_NUM_MASK = ( 0xf   << 0 ),
  DOT11_SC_SEQ_NUM_MASK  = ( 0xfff << 4 )
} dot11_Sc_t;

/* QoS Control field of the 802.11 header */
#define DOT11_QOS_CONTROL_FIELD_TID_BITS    0x000f
#define DOT11_QOS_CONTROL_FIELD_A_MSDU_BITS 0x0080

#define DOT11_QOS_CONTROL_ACK        0x0000
#define DOT11_QOS_CONTROL_DONT_ACK   0x0020

typedef struct
{
  TI_UINT16        fc;
  TI_UINT16        duration;
  TMacAddr         address1;
  TMacAddr         address2;
  TMacAddr         address3;
  TI_UINT16        seqCtrl;
  TI_UINT16        qosControl;
/*  TMacAddr    address4;*/
}  dot11_header_t;

typedef struct
{
  TI_UINT16        fc;
  TI_UINT16        duration;
  TMacAddr         address1;
  TMacAddr         address2;
  TMacAddr         address3;
  TI_UINT16        seqCtrl;
} legacy_dot11_header_t;



typedef struct
{
  TI_UINT16        fc;
  TI_UINT16        duration;
  TMacAddr         DA;
  TMacAddr         SA;
  TMacAddr         BSSID;
  TI_UINT16        seqCtrl;
} dot11_mgmtHeader_t;

typedef struct
{
  TI_UINT8     DSAP;
  TI_UINT8     SSAP;
  TI_UINT8     Control;
  TI_UINT8     OUI[3];
  TI_UINT16    Type;
} Wlan_LlcHeader_T;

typedef struct
{
  TI_UINT16        fc;
  TI_UINT16        AID;
  TMacAddr         BSSID;
  TMacAddr         TA;
} dot11_PsPollFrameHeader_t;

typedef struct
{
  TI_UINT16        fc;
  TI_UINT16        duration;
  TMacAddr         RA;
  TMacAddr         TA;
} dot11_BarFrameHeader_t;


#define FCS_SIZE    4

#define WLAN_HDR_LEN                            24
#define WLAN_HT_HDR_LEN                         28
#define WLAN_QOS_HDR_LEN                        26
#define WLAN_QOS_HT_HDR_LEN                     30
#define WLAN_QOS_HT_CONTROL_FIELD_LEN           4
#define WLAN_SNAP_HDR_LEN                       8
#define WLAN_WITH_SNAP_HEADER_MAX_SIZE          (WLAN_HDR_LEN + WLAN_SNAP_HDR_LEN)
#define WLAN_WITH_SNAP_QOS_HEADER_MAX_SIZE      (WLAN_QOS_HDR_LEN + WLAN_SNAP_HDR_LEN)

#define GET_MAX_HEADER_SIZE(macHeaderPointer,headerSize)    \
    if (IS_QOS_FRAME(*(TI_UINT16*)(macHeaderPointer)))      \
    {\
       if (IS_HT_FRAME(*(TI_UINT16*)(macHeaderPointer)))    \
           *headerSize = WLAN_QOS_HT_HDR_LEN;               \
       else                                                 \
           *headerSize = WLAN_QOS_HDR_LEN;                  \
     }\
     else                                                   \
     {\
       if (IS_HT_FRAME(*(TI_UINT16*)(macHeaderPointer)))    \
           *headerSize = WLAN_HT_HDR_LEN;                   \
       else                                                 \
           *headerSize = WLAN_HDR_LEN;                      \
     }

/****************************************************************************************
    The next table is defined in 802.11 spec section 7.2.2 for the address field contents :
    To DS   From DS     Address 1    Address 2  Address 3    Address 4
    -------------------------------------------------------------------
    0           0           DA          SA          BSSID       N/A
    0           1           DA          BSSID       SA          N/A
    1           0           BSSID       SA          DA          N/A
    1           1           RA          TA          DA          SA         
    
NOTE: We only support packets coming from within the DS (i.e. From DS = 0)
*****************************************************************************************/
/* return the destination address used in *dot11_header_t */
#define GET_DA_FROM_DOT11_HEADER_T(pDot11Hdr)   ((pDot11Hdr->fc & DOT11_FC_TO_DS) ? (pDot11Hdr->address3) : (pDot11Hdr->address1)) 


/*
 * MANAGEMENT
 * -----------------
 */

/* mgmt body max length */
#define MAX_MGMT_BODY_LENGTH                2312
/* maximal length of beacon body - note that actual beacons may actually be longer
   than this size, at least according to the spec, but so far no larger beacon was seen 
  Note: 1500 is the recommended size by the Motorola Standard team. TI recommendation is 700*/
#define MAX_BEACON_BODY_LENGTH              1500

/* general mgmt frame structure */
typedef struct
{
    dot11_mgmtHeader_t  hdr;
    TI_UINT8                body[MAX_MGMT_BODY_LENGTH];
} dot11_mgmtFrame_t;

/* Capabilities Information Field - IN THE AIR */
/**/
/*  bit  15      14       13         12        11         10      9      8      7   -   0*/
/* +----------+------+----------+---------+----------+---------+------+-----+---------------+*/
/* |  Channel |      |  Short   | Privacy | CF Poll  |   CF    |      |     |   RESERVED    |   */
/* |  Agility | PBCC | Preamble |         | Request  | Pollable| IBSS | ESS |               |*/
/* +----------+------+----------+---------+----------+---------+------+-----+---------------+   */
/*       1        1       1          1         1          1       1      1*/


/* Capabilities Information Field - IN THE MGMT SOFTWARE AFTER THE SWAP */
/**/
/* bit 15 - 8         7        6       5          4         3          2       1      0*/
/* +------------+----------+------+----------+---------+----------+---------+------+-----+*/
/* |            |  Channel |      |  Short   | Privacy | CF Poll  |   CF    |      |     |*/
/* |  Reserved  |  Agility | PBCC | Preamble |         | Request  | Pollable| IBSS | ESS |*/
/* +------------+----------+------+----------+---------+----------+---------+------+-----+*/
/*       8            1        1       1          1         1          1       1      1*/



typedef enum
{ 
  DOT11_CAPS_ESS             = ( 1 << 0 ),
  DOT11_CAPS_IBSS            = ( 1 << 1 ),
  DOT11_CAPS_CF_POLLABLE     = ( 1 << 2 ),
  DOT11_CAPS_CF_POLL_REQUEST = ( 1 << 3 ),
  DOT11_CAPS_PRIVACY         = ( 1 << 4 ),
  DOT11_CAPS_SHORT_PREAMBLE  = ( 1 << 5 ),
  DOT11_CAPS_PBCC            = ( 1 << 6 ),
  DOT11_CAPS_CHANNEL_AGILITY = ( 1 << 7 ),
  DOT11_SPECTRUM_MANAGEMENT  = ( 1 << 8 ),
  DOT11_CAPS_QOS_SUPPORTED   = ( 1 << 9 ),
  DOT11_CAPS_SHORT_SLOT_TIME = (1  << 10),

  DOT11_CAPS_APSD_SUPPORT    = ( 1 << 11),

  DOT11_CAPS_DELAYED_BA      = ( 1 << 14),
  DOT11_CAPS_IMMEDIATE_BA    = ( 1 << 15)

} dot11_capabilities_e;

typedef enum 
{
    /* ESS */
    CAP_ESS_MASK            = 1,
    CAP_ESS_SHIFT           = 0,

    /* IBSS */
    CAP_IBSS_MASK           = 1,
    CAP_IBSS_SHIFT          = 1,

    /* CF Pollable */
    CAP_CF_POLL_MASK        = 1,
    CAP_CF_POLL_SHIFT       = 2,
    
    /* CF Poll request */
    CAP_CF_REQ_MASK         = 1,
    CAP_CF_REQ_SHIFT        = 3,
    
    /* Privacy */
    CAP_PRIVACY_MASK        = 1,
    CAP_PRIVACY_SHIFT       = 4,

    /* Short Preamble*/
    CAP_PREAMBLE_MASK       = 1,
    CAP_PREAMBLE_SHIFT      = 5,
    
    /* PBCC */
    CAP_PBCC_MASK           = 1,
    CAP_PBCC_SHIFT          = 6,
    
    /* Agile */
    CAP_AGILE_MASK          = 1,
    CAP_AGILE_SHIFT         = 7,

    /* Slot time */
    CAP_SLOT_TIME_MASK      = 1,
    CAP_SLOT_TIME_SHIFT     = 10,

    /* APSD */
    CAP_APSD_MASK           = 1,
    CAP_APSD_SHIFT          = 11


} wdrv_mgmtCapabilities_e;


/*
 * 802.11 Information elements
 * ---------------------------
 */

typedef TI_UINT8 dot11_eleHdr_t[2];  /* Byte-0: IE-ID,  Byte-1: IE-Length  */

/* fixed fields lengths, except of currentAP & timestamp*/
#define FIX_FIELD_LEN       2

/* SSID Information Element */
#define DOT11_SSID_ELE_ID   0

/* Max SSID length */
#define MAX_SSID_LEN        32

typedef struct 
{
    dot11_eleHdr_t    hdr;
    char              serviceSetId[MAX_SSID_LEN];
}  dot11_SSID_t;


/* Supportted rates Information Element */
#define DOT11_SUPPORTED_RATES_ELE_ID        1
#define DOT11_EXT_SUPPORTED_RATES_ELE_ID        50
typedef struct 
{
  dot11_eleHdr_t hdr;
  TI_UINT8 rates[DOT11_MAX_SUPPORTED_RATES];
}  dot11_RATES_t;


#define ERP_IE_NON_ERP_PRESENT_MASK         0x1
#define ERP_IE_USE_PROTECTION_MASK          0x2
#define ERP_IE_BARKER_PREAMBLE_MODE_MASK    0x4
#define DOT11_ERP_IE_ID 42
typedef struct
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           ctrl;
}  dot11_ERP_t;

/* RSN Information Element */
#define MAX_RSN_IE                          3
#define DOT11_RSN_MAX                       255 
typedef struct 
{
  dot11_eleHdr_t hdr;
  TI_UINT8 rsnIeData[DOT11_RSN_MAX];
}  dot11_RSN_t;

/* General definitions needed by rsn.c */
#define IV_FIELD_SIZE   4 
#define ICV_FIELD_SIZE  4
#define MIC_FIELD_SIZE  8 
#define EIV_FIELD_SIZE  4
#define WEP_AFTER_HEADER_FIELD_SIZE  IV_FIELD_SIZE
#define TKIP_AFTER_HEADER_FIELD_SIZE (IV_FIELD_SIZE + EIV_FIELD_SIZE)
#define AES_AFTER_HEADER_FIELD_SIZE  8

/* DS params Information Element */
#define DOT11_DS_PARAMS_ELE_ID      3
#define DOT11_DS_PARAMS_ELE_LEN     1
typedef struct 
{
  dot11_eleHdr_t hdr;
  TI_UINT8  currChannel;
}  dot11_DS_PARAMS_t;


/* DS params Information Element */
#define DOT11_IBSS_PARAMS_ELE_ID    6
#define DOT11_IBSS_PARAMS_ELE_LEN   2
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT16          atimWindow;
} dot11_IBSS_PARAMS_t;

#define DOT11_FH_PARAMS_ELE_ID      2
#define DOT11_FH_PARAMS_ELE_LEN     5
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT16          dwellTime;
    TI_UINT8           hopSet;
    TI_UINT8           hopPattern;
    TI_UINT8           hopIndex;
}  dot11_FH_PARAMS_t;

/* tim Information Element */
#define DOT11_TIM_ELE_ID    5
#define DOT11_PARTIAL_VIRTUAL_BITMAP_MAX    251
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           dtimCount;
    TI_UINT8           dtimPeriod;
    TI_UINT8           bmapControl;
    TI_UINT8           partialVirtualBmap[DOT11_PARTIAL_VIRTUAL_BITMAP_MAX];
}  dot11_TIM_t;

/* tim Information Element */
#define DOT11_CF_ELE_ID             4
#define DOT11_CF_PARAMS_ELE_LEN     6
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           cfpCount;
    TI_UINT8           cfpPeriod;
    TI_UINT16          cfpMaxDuration;
    TI_UINT16          cfpDurRemain;
} dot11_CF_PARAMS_t;

/* Challenge text Information Element */
#define DOT11_CHALLENGE_TEXT_ELE_ID     16
#define DOT11_CHALLENGE_TEXT_MAX        253
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           text[ DOT11_CHALLENGE_TEXT_MAX ];
}  dot11_CHALLENGE_t;


#define DOT11_NUM_OF_MAX_TRIPLET_CHANNEL    37

typedef struct
{
    TI_UINT8           firstChannelNumber;
    TI_UINT8           numberOfChannels;
    TI_UINT8           maxTxPowerLevel;
} dot11_TripletChannel_t;

typedef struct
{
    TI_UINT8                CountryString[DOT11_COUNTRY_STRING_LEN];
    dot11_TripletChannel_t  tripletChannels[DOT11_NUM_OF_MAX_TRIPLET_CHANNEL];
} dot11_countryIE_t;


/* Country Inforamtion Element */
#define DOT11_COUNTRY_ELE_ID        7
#define DOT11_COUNTRY_ELE_LEN_MAX   ( ((DOT11_NUM_OF_MAX_TRIPLET_CHANNEL+1)*3) + !((DOT11_NUM_OF_MAX_TRIPLET_CHANNEL&0x1)))
typedef struct 
{
    dot11_eleHdr_t    hdr;
    dot11_countryIE_t countryIE;
}  dot11_COUNTRY_t;


/* Power Constraint Information Element */
#define DOT11_POWER_CONSTRAINT_ELE_ID       (32)
#define DOT11_POWER_CONSTRAINT_ELE_LEN      (1)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           powerConstraint;
}  dot11_POWER_CONSTRAINT_t;



/* Power Capability Information Element */
#define DOT11_CAPABILITY_ELE_ID         (33)
#define DOT11_CAPABILITY_ELE_LEN        (2)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           minTxPower;
    TI_UINT8           maxTxPower;
} dot11_CAPABILITY_t;

/* TPC request Information Element */
#define DOT11_TPC_REQUEST_ELE_ID        (34)
#define DOT11_TPC_REQUEST_ELE_LEN       (0)
typedef struct 
{
    dot11_eleHdr_t  hdr;
}  dot11_TPC_REQUEST_t;

/* TPC report Information Element */
#define DOT11_TPC_REPORT_ELE_ID         (35)
#define DOT11_TPC_REPORT_ELE_LEN        (2)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           transmitPower;
    TI_UINT8           linkMargin;
} dot11_TPC_REPORT_t;


#ifdef  XCC_MODULE_INCLUDED

/* Cell Transmit Power Information Element */
#define DOT11_CELL_TP_ELE_ID            (150)
#define DOT11_CELL_TP_ELE_LEN           (6)

typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           oui[4];
    TI_UINT8           power;
    TI_UINT8           reserved;
} dot11_CELL_TP_t;

#define   DOT11_CELL_TP \
    dot11_CELL_TP_t         *cellTP;
    
#else   /* XCC_MODULE_INCLUDED */

#define   DOT11_CELL_TP

#endif  /* XCC_MODULE_INCLUDED */


/* Channel Supported Information Element */
#define DOT11_CHANNEL_SUPPORTED_ELE_ID  (36)
#define DOT11_CHANNEL_SUPPORTED_ELE_LEN (26)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           supportedChannel[DOT11_CHANNEL_SUPPORTED_ELE_LEN];

} dot11_CHANNEL_SUPPORTED_t;

/* Channel Switch Announcement Information Element */
#define DOT11_CHANNEL_SWITCH_ELE_ID     (37)
#define DOT11_CHANNEL_SWITCH_ELE_LEN    (3)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           channelSwitchMode;
    TI_UINT8           channelNumber;
    TI_UINT8           channelSwitchCount;
}  dot11_CHANNEL_SWITCH_t;

#define MAX_NUM_REQ (16)

/* Measurement request Information Element */
#define DOT11_MEASUREMENT_REQUEST_ELE_ID        (38)
#define DOT11_MEASUREMENT_REQUEST_LEN           (2)
#define DOT11_MEASUREMENT_REQUEST_ELE_LEN       (3 + DOT11_MEASUREMENT_REQUEST_LEN*MAX_NUM_REQ)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           measurementToken;
    TI_UINT8           measurementMode;
    TI_UINT8           measurementType;
    TI_UINT8           measurementRequests[DOT11_MEASUREMENT_REQUEST_LEN*MAX_NUM_REQ];
}  dot11_MEASUREMENT_REQUEST_t;


/* Quiet Information Element */
#define DOT11_QUIET_ELE_ID              (40)
#define DOT11_QUIET_ELE_LEN             (6)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           quietCount;
    TI_UINT8           quietPeriod;
    TI_UINT16          quietDuration;
    TI_UINT16          quietOffset;
} dot11_QUIET_t;


/* QoS Capability Information Element */
#define DOT11_QOS_CAPABILITY_ELE_ID     (46)
#define DOT11_QOS_CAPABILITY_ELE_LEN    (1)

#define AC_APSD_FLAGS_MASK              (1)
#define Q_ACK_BITG_MASK                 (1)
#define MAX_SP_LENGTH_MASK              (3)
#define MORE_DATA_ACK_MASK              (1)

#define AC_VO_APSD_FLAGS_SHIFT          (0)
#define AC_VI_APSD_FLAGS_SHIFT          (1)
#define AC_BK_APSD_FLAGS_SHIFT          (2)
#define AC_BE_APSD_FLAGS_SHIFT          (3)
#define Q_ACK_FLAGS_SHIFT               (4)
#define MAX_SP_LENGTH_SHIFT             (5)
#define MORE_DATA_ACK_SHIFT             (7)

#define QOS_CONTROL_UP_SHIFT            (0)

#define AP_QOS_INFO_UAPSD_MASK          (1)
#define AP_QOS_INFO_UAPSD_SHIFT         (7)

typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           QosInfoField;
}  dot11_QOS_CAPABILITY_IE_t;

/* WPS Information Element */
#define DOT11_WPS_ELE_ID    (221)
#define DOT11_WPS_OUI       {0x00, 0x50, 0xF2, 0x04}
#define DOT11_WPS_OUI_LEN   4

/* WME Information Element */
#define DOT11_WME_ELE_ID                (221)
#define DOT11_WME_ELE_LEN               (7)
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           OUI[3];
    TI_UINT8           OUIType;
    TI_UINT8           OUISubType;
    TI_UINT8           version;
    TI_UINT8           ACInfoField;
}  dot11_WME_IE_t;


typedef struct
{
    TI_UINT8           ACI_AIFSN;
    TI_UINT8           ECWmin_ECWmax;
    TI_UINT16          TXOPLimit;
} dot11_QOS_AC_IE_ParametersRecord_t;

typedef struct
{
    dot11_QOS_AC_IE_ParametersRecord_t        ACBEParametersRecord;
    dot11_QOS_AC_IE_ParametersRecord_t        ACBKParametersRecord;
    dot11_QOS_AC_IE_ParametersRecord_t        ACVIParametersRecord;
    dot11_QOS_AC_IE_ParametersRecord_t        ACVOParametersRecord;
} dot11_ACParameters_t;


/* WME Parameter Information Element */
#define DOT11_WME_PARAM_ELE_ID          (221)
#define DOT11_WME_PARAM_ELE_LEN         (24)
typedef struct 
{
    dot11_eleHdr_t      hdr;
    TI_UINT8               OUI[3];
    TI_UINT8               OUIType;
    TI_UINT8               OUISubType;
    TI_UINT8               version;
    TI_UINT8               ACInfoField;
    TI_UINT8               reserved;
    dot11_ACParameters_t   WME_ACParameteres;
}  dot11_WME_PARAM_t;


/* 802.11n HT Information Element */
#define DOT11_HT_CAPABILITIES_ELE_LEN    (26)
#define DOT11_HT_INFORMATION_ELE_LEN    (22)

#define DOT11_HT_CAPABILITIES_MCS_RATE_OFFSET   (3)
#define DOT11_HT_INFORMATION_MCS_RATE_OFFSET    (6)

typedef struct 
{
    dot11_eleHdr_t  tHdr;
    TI_UINT8        aHtCapabilitiesIe[DOT11_HT_CAPABILITIES_ELE_LEN];   /* HT capabilities IE unparsed */
}  Tdot11HtCapabilitiesUnparse;

typedef struct 
{
    dot11_eleHdr_t  tHdr;
    TI_UINT8        aHtInformationIe[DOT11_HT_INFORMATION_ELE_LEN];     /* HT Information IE unparsed */
}  Tdot11HtInformationUnparse;

/* BA session bits mask */
#define DOT11_BAR_CONTROL_FIELD_TID_BITS            0xf000
#define DOT11_BA_PARAMETER_SET_FIELD_TID_BITS       0x003C
#define DOT11_BA_PARAMETER_SET_FIELD_WINSIZE_BITS   0xffC0
#define DOT11_DELBA_PARAMETER_FIELD_TID_BITS        0xf000

/* action field BA frames */
typedef enum
{
    DOT11_BA_ACTION_ADDBA           = 0,
    DOT11_BA_ACTION_DELBA           = 2
} Edot11BaAction;


/* WiFi Simple Config Information Element */
#define DOT11_WSC_PARAM_ELE_ID          (221)

#define DOT11_WSC_SELECTED_REGISTRAR_CONFIG_METHODS      0x1053
#define DOT11_WSC_SELECTED_REGISTRAR_CONFIG_METHODS_PIN  0xC
#define DOT11_WSC_SELECTED_REGISTRAR_CONFIG_METHODS_PBC  0x80

#define DOT11_WSC_DEVICE_PASSWORD_ID      0x1012
#define DOT11_WSC_DEVICE_PASSWORD_ID_PIN  0x0000
#define DOT11_WSC_DEVICE_PASSWORD_ID_PBC  0x0004

/* WiFi Simple Config Parameter Information Element */
#define DOT11_WSC_PROBE_REQ_PARAM_ELE_LEN           (22)
#define DOT11_WSC_BEACON_MAX_LENGTH 160

typedef struct 
{
    dot11_eleHdr_t              hdr;
    TI_UINT8                        OUI[3];
    TI_UINT8                        OUIType;
    TI_UINT8 WSCBeaconOrProbIE[DOT11_WSC_BEACON_MAX_LENGTH];
}  dot11_WSC_t;

#define dot11_WPA_OUI_TYPE                  (1)
#define dot11_WME_OUI_TYPE                  (2)
#define dot11_WSC_OUI_TYPE                  (4)
#define dot11_WME_OUI_SUB_TYPE_IE           (0)
#define dot11_WME_OUI_SUB_TYPE_PARAMS_IE    (1)
#define dot11_WME_VERSION                   (1)
#define dot11_WME_ACINFO_MASK               0x0f

/* -------------------- TSPEC ----------------- */

typedef struct
{
    TI_UINT8   tsInfoArr[3];

}  tsInfo_t;



/* This structure is part of the TSPEC structure. It was seperated since there are some cases (such as DEL_TS), which we dont need
to send ALL the TSPEC structure, but only as far as TsInfo. The TSPEC structure contains this smaller structure */
typedef struct
{
    dot11_eleHdr_t  hdr;
    
    TI_UINT8   OUI[3];
    TI_UINT8   oui_type;
    TI_UINT8   oui_subtype;
    TI_UINT8   version;

    tsInfo_t tsInfoField;
}  dot11_WME_TSPEC_IE_hdr_t;

typedef struct
{

    dot11_WME_TSPEC_IE_hdr_t tHdr;

    TI_UINT16  nominalMSDUSize;
    TI_UINT16  maximumMSDUSize;
    TI_UINT32  minimumServiceInterval;
    TI_UINT32  maximumServiceInterval;
    TI_UINT32  inactivityInterval;
    TI_UINT32  suspensionInterval;
    TI_UINT32  serviceStartTime;
    TI_UINT32  minimumDataRate;
    TI_UINT32  meanDataRate;
    TI_UINT32  peakDataRate;
    TI_UINT32  maximumBurstSize;
    TI_UINT32  delayBound;
    TI_UINT32  minimumPHYRate;
    TI_UINT16  surplusBandwidthAllowance;
    TI_UINT16  mediumTime;

} dot11_WME_TSPEC_IE_t;


#define WME_TSPEC_IE_ID                         221
#define WME_TSPEC_IE_LEN                        61
#define WME_TSPEC_IE_TSINFO_LEN                 9                
#define WME_TSPEC_IE_OUI_TYPE                   0x02
#define WME_TSPEC_IE_OUI_SUB_TYPE               0x02
#define WME_TSPEC_IE_VERSION                    0x01

/* OUI TYPE values that can be present in management packets inside Cisco vendor specific IE */
typedef enum
{
    TS_METRIX_OUI_TYPE = 0x07,
    TS_RATE_SET_OUI_TYPE = 0x08,
    EDCA_LIFETIME_OUI_TYPE = 0x09
} XCC_IE_OUI_TYPE_t;

#define ADDTS_REQUEST_ACTION                    0x00
#define ADDTS_RESPONSE_ACTION                   0x01
#define DELTS_ACTION                            0x02

#define ADDTS_STATUS_CODE_SUCCESS               0x00
#define DELTS_CODE_SUCCESS                      0x00
 

#define TS_INFO_0_TRAFFIC_TYPE_MASK             0x01
#define TS_INFO_0_TSID_MASK                     0x1E
#define TS_INFO_0_DIRECTION_MASK                0x60
#define TS_INFO_0_ACCESS_POLICY_MASK            0x80

#define TS_INFO_1_ACCESS_POLICY_MASK            0x01
#define TS_INFO_1_AGGREGATION_MASK              0x02
#define TS_INFO_1_APSD_MASK                     0x04    
#define TS_INFO_1_USER_PRIORITY_MASK            0x38
#define TS_INFO_1_TSINFO_ACK_POLICY_MASK        0xC0

#define TS_INFO_2_SCHEDULE_MASK                 0x01
#define TS_INFO_2_RESERVED_MASK                 0xF7    

#define TRAFFIC_TYPE_SHIFT                      0
#define TSID_SHIFT                              1
#define DIRECTION_SHIFT                         5
#define ACCESS_POLICY_SHIFT                     7
#define AGGREGATION_SHIFT                       1
#define APSD_SHIFT                              2   
#define USER_PRIORITY_SHIFT                     3
#define TSINFO_ACK_POLICY_SHIFT                 6
#define SCHEDULE_SHIFT                          0
#define RESERVED_SHIFT                          1
#define SURPLUS_BANDWIDTH_ALLOW                 13  

#define TS_INFO_0_ACCESS_POLICY_EDCA            0x1                 
#define NORMAL_ACKNOWLEDGEMENT                  0x00        
#define NO_SCHEDULE                             0x00        
#define PS_UPSD                                 0x01
#define EDCA_MODE                               0x08
#define FIX_MSDU_SIZE                           0x8000

#define WPA_IE_OUI                              {0x00, 0x50, 0xf2}
#define XCC_OUI                                 {0x00, 0x40, 0x96}

/* Action field structure
    used for extended management action such as spectrum management */ 
typedef TI_UINT8 dot11_ACTION_FIELD_t[2];


/* Management frames sub types */
typedef enum
{
    ASSOC_REQUEST       = 0,
    ASSOC_RESPONSE      = 1,
    RE_ASSOC_REQUEST    = 2,
    RE_ASSOC_RESPONSE   = 3,
    PROBE_REQUEST       = 4,
    PROBE_RESPONSE      = 5,
    BEACON              = 8,
    ATIM                = 9,
    DIS_ASSOC           = 10,
    AUTH                = 11,
    DE_AUTH             = 12,
    ACTION              = 13
} dot11MgmtSubType_e;

/* Management frames element IDs */
typedef enum
{
    SSID_IE_ID                          = 0,
    SUPPORTED_RATES_IE_ID               = 1,
    FH_PARAMETER_SET_IE_ID              = 2,
    DS_PARAMETER_SET_IE_ID              = 3,
    CF_PARAMETER_SET_IE_ID              = 4,
    TIM_IE_ID                           = 5,
    IBSS_PARAMETER_SET_IE_ID            = 6,
    COUNTRY_IE_ID                       = 7,
    CHALLANGE_TEXT_IE_ID                = 16,
    POWER_CONSTRAINT_IE_ID              = 32,
    TPC_REPORT_IE_ID                    = 35,
    CHANNEL_SWITCH_ANNOUNCEMENT_IE_ID   = 37,
    QUIET_IE_ID                         = 40,
    ERP_IE_ID                           = 42,
    HT_CAPABILITIES_IE_ID               = 45,
    QOS_CAPABILITY_IE_ID                = 46,
    RSN_IE_ID                           = 48,
    EXT_SUPPORTED_RATES_IE_ID           = 50,
    HT_INFORMATION_IE_ID                = 61,
    XCC_EXT_1_IE_ID                     = 133,
    XCC_EXT_2_IE_ID                     = 149,  
    CELL_POWER_IE                       = 150, /*XCC*/
    WPA_IE_ID                           = 221

} dot11MgmtIeId_e;

/* Spectrum Management Action fields */
typedef enum
{
    MEASUREMENT_REQUEST             = 0,
    MEASUREMENT_REPORT              = 1,
    TPC_REQUEST                     = 2,
    TPC_REPORT                      = 3,
    CHANNEL_SWITCH_ANNOUNCEMENT     = 4
} dot11ActionFrameTypes_e;

/* Category fields (such as apectrum management)*/
typedef enum
{
    CATAGORY_SPECTRUM_MANAGEMENT        = 0,
    CATAGORY_QOS                        = 1,
    WME_CATAGORY_QOS                    = 17,
    CATAGORY_SPECTRUM_MANAGEMENT_ERROR  = 128
} dot11CategoryTypes_e;


/* 
 * Management templates to set to the HAL:
 */

#ifdef XCC_MODULE_INCLUDED

    typedef struct  
    {
        dot11_mgmtHeader_t  hdr;
        char                infoElements[sizeof( dot11_SSID_t ) + 
                                         sizeof( dot11_RATES_t ) +
                                         sizeof( dot11_RATES_t ) +
                                         sizeof( Tdot11HtCapabilitiesUnparse ) +
                                         DOT11_WSC_PROBE_REQ_MAX_LENGTH +
                                         sizeof( XCC_radioManagmentCapability_IE_t )
                                        ];
    } probeReqTemplate_t;

#else /* XCC_MODULE_INCLUDED */

    typedef struct  
    {
        dot11_mgmtHeader_t  hdr;
        char                infoElements[sizeof( dot11_SSID_t ) + 
                                         sizeof( dot11_RATES_t ) +
                                         sizeof( dot11_RATES_t ) +
                                         sizeof( Tdot11HtCapabilitiesUnparse ) +
                                         DOT11_WSC_PROBE_REQ_MAX_LENGTH
                                        ];
    } probeReqTemplate_t;

#endif /* XCC_MODULE_INCLUDED */


typedef struct 
{
    dot11_mgmtHeader_t  hdr;
    TI_UINT8               timeStamp[TIME_STAMP_LEN];
    TI_UINT16              beaconInterval;
    TI_UINT16              capabilities;
    char                infoElements[ sizeof( dot11_SSID_t ) + 
                                      sizeof( dot11_RATES_t ) +
                                      sizeof( dot11_RATES_t ) +
                                      sizeof( dot11_DS_PARAMS_t ) +
                                      sizeof( dot11_COUNTRY_t)      ];
}  probeRspTemplate_t;

typedef struct 
{
    dot11_mgmtHeader_t  hdr;
} nullDataTemplate_t;

typedef struct 
{
    dot11_mgmtHeader_t  hdr;
    TI_UINT16 disconnReason;
} disconnTemplate_t; /* Deauth or Disassoc */

typedef struct
{
    dot11_header_t   hdr;
	TI_UINT8 	securityOverhead[AES_AFTER_HEADER_FIELD_SIZE];
    Wlan_LlcHeader_T LLC;
    TI_UINT16 hardType;
    TI_UINT16 protType;
    TI_UINT8  hardSize;
    TI_UINT8  protSize;
    TI_UINT16 op;
    TMacAddr  StaMac;
    TIpAddr   StaIp;
    TMacAddr  TargMac;
    TIpAddr   TargIp;
} ArpRspTemplate_t; /* for auto ArpRsp sending by FW */

typedef struct
{
   dot11_PsPollFrameHeader_t   hdr;
} psPollTemplate_t;

typedef struct
{
   dot11_header_t   hdr;
}  QosNullDataTemplate_t;

/* Traffic Stream Rate Set (TSRS) info-elements */
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           OUI[3];
    TI_UINT8           oui_type;
    TI_UINT8           tsid;
    TI_UINT8           tsNominalRate;
} dot11_TSRS_STA_IE_t;

typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           OUI[3];
    TI_UINT8           oui_type;
    TI_UINT8           tsid;
    TI_UINT8           tsRates[8];
}  dot11_TSRS_IE_t;

/* MSDU lifetime info-element */
typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           OUI[3];
    TI_UINT8           oui_type;
    TI_UINT8           tsid;
    TI_UINT16          msduLifeTime;
}  dot11_MSDU_LIFE_TIME_IE_t;

typedef struct 
{
    dot11_eleHdr_t  hdr;
    TI_UINT8           OUI[3];
    TI_UINT8           oui_type;
    TI_UINT8           tsid;
    TI_UINT8           state;
    TI_UINT16          measureInterval;
}  dot11_TS_METRICS_IE_t;

typedef struct 
{
    dot11_TSRS_IE_t             *trafficStreamParameter;
    dot11_MSDU_LIFE_TIME_IE_t   *edcaLifetimeParameter;
    dot11_TS_METRICS_IE_t       *tsMetrixParameter;
} XCCv4IEs_t;


/* Measurement Report message frame structure */
#define DOT11_MEASUREMENT_REPORT_ELE_ID     (39)
#define DOT11_MAX_MEASUREMENT_REPORT_LEN    (4)
#define DOT11_MIN_MEASUREMENT_REPORT_IE_LEN (3)
#define DOT11_MEASUREMENT_REPORT_ELE_IE_LEN (DOT11_MIN_MEASUREMENT_REPORT_IE_LEN + DOT11_MAX_MEASUREMENT_REPORT_LEN*MAX_NUM_REQ)

typedef struct
{
    dot11_ACTION_FIELD_t    actionField;
    TI_UINT8   dialogToken;

    dot11_eleHdr_t  hdr;
    TI_UINT8            measurementToken;
    TI_UINT8            measurementMode;
    TI_UINT8            measurementType;
    TI_UINT8            measurementReports[DOT11_MAX_MEASUREMENT_REPORT_LEN*MAX_NUM_REQ];
}  MeasurementReportFrame_t;



typedef enum 
{
    STATUS_SUCCESSFUL = 0,
    STATUS_UNSPECIFIED,
    STATUS_AUTH_REJECT,
    STATUS_ASSOC_REJECT,
    STATUS_SECURITY_FAILURE,
    STATUS_AP_DEAUTHENTICATE,
    STATUS_AP_DISASSOCIATE,
    STATUS_ROAMING_TRIGGER,
    STATUS_DISCONNECT_DURING_CONNECT,
    STATUS_SG_RESELECT,
	STATUS_MIC_FAILURE = 14,
    MGMT_STATUS_MAX_NUM = 15
} mgmtStatus_e;

/* Used as a status code in case of STATUS_AUTH_REJECT or STATUS_ASSOC_REJECT that was not received at all */
#define STATUS_PACKET_REJ_TIMEOUT   0xFFFF

/* As defined in 802.11 spec section 7.3.1 - status codes for deAuth packet */
#define STATUS_CODE_802_1X_AUTHENTICATION_FAILED 23

/* map field included in measurement report IE (only in basic report) */
typedef enum
{
  DOT11_BSS_ONLY                    = (0x01),
  DOT11_OFDM_ONLY                   = (0x02),
  DOT11_RADAR_AND_UNIDENTIFIED      = (0x0C)
} dot11_Map_Sub_Field_e;


typedef struct
{
   legacy_dot11_header_t dot11Header;
   Wlan_LlcHeader_T  snapHeader;
}  legacy_dot11_DataMsduHeader_t;


#define WLAN_HEADER_TYPE_CONCATENATION 0x01
#define WLAN_CONCAT_HEADER_LEN 2


#endif   /* _802_11_INFO_DEFS_H */
