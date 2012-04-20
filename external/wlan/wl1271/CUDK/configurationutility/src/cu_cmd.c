/*
 * cu_cmd.c
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and  
 * limitations under the License.
 */

/****************************************************************************
*
*   MODULE:  cu_cmd.c
*   
*   PURPOSE: 
* 
*   DESCRIPTION:  
*   ============
*      
*
****************************************************************************/

/* includes */
/************/

#include <stdio.h>

#include "cu_osapi.h"
#include "TWDriver.h"
#include "convert.h"
#include "console.h"
#include "cu_common.h"
#include "cu_os.h"
#include "ipc_event.h"
#include "wpa_core.h"
#include "cu_cmd.h"
#include "oserr.h"

/* defines */
/***********/
#define print_available_values(arr) \
        { \
            S32 i; \
            for(i=0; i<SIZE_ARR(arr); i++) \
                os_error_printf(CU_MSG_INFO2, (PS8)"%d - %s%s", arr[i].value, arr[i].name, (i>=SIZE_ARR(arr)-1) ? (PS8)"\n" : (PS8)", " ); \
        }

#define CU_CMD_FIND_NAME_ARRAY(index, arr, val) \
        for ( index = 0; index < SIZE_ARR(arr); index++ ) \
            if ( arr[ index ].value == (val) ) \
                break; \

#define CHAN_FREQ_TABLE_SIZE        (sizeof(ChanFreq) / sizeof(struct CHAN_FREQ))

#define IS_BASIC_RATE(a)    ((a) & NET_BASIC_MASK)

#define RATE_2_MBPS(a)    ((F32)((a) & (NET_BASIC_MASK-1))/2)

#define NET_BASIC_MASK      0x80    /* defined in common/src/utils/utils.c */

#define BIT_TO_BYTE_FACTOR  8

#define NVS_FILE_TX_PARAMETERS_UPDATE	0
#define NVS_FILE_RX_PARAMETERS_UPDATE	1


/* local types */
/***************/
/* Module control block */
typedef struct CuCmd_t
{
    THandle                 hCuWext;
    THandle                 hCuCommon;
    THandle                 hConsole;
    THandle                 hIpcEvent;
    THandle                 hWpaCore;
    
    U32                     isDeviceRunning;

    scan_Params_t             appScanParams;
    TPeriodicScanParams     tPeriodicAppScanParams;
    scan_Policy_t             scanPolicy;
    
} CuCmd_t;

/* local variables */
/*******************/
struct CHAN_FREQ {
    U8       chan;
    U32      freq;
} ChanFreq[] = {
    {1,2412000}, {2,2417000}, {3,2422000}, {4,2427000},
    {5,2432000}, {6,2437000}, {7,2442000}, {8,2447000},
    {9,2452000},
    {10,2457000}, {11,2462000}, {12,2467000}, {13,2472000},
    {14,2484000}, {36,5180000}, {40,5200000}, {44,5220000},
    {48,5240000}, {52,5260000}, {56,5280000}, {60,5300000},
    {64,5320000},
    {100,5500000}, {104,5520000}, {108,5540000}, {112,5560000},
    {116,5580000}, {120,5600000}, {124,5620000}, {128,5640000},
    {132,5660000}, {136,5680000}, {140,5700000}, {149,5745000},
    {153,5765000}, {157,5785000}, {161,5805000} };

static named_value_t BSS_type[] =
{
    { os802_11IBSS,                  (PS8)"AD-Hoc" },
    { os802_11Infrastructure,        (PS8)"Infr." },
    { os802_11AutoUnknown,           (PS8)"Auto" },
};

static named_value_t Current_mode[] =
{
    { 0,           (PS8)"SME Auto" },
    { 1,           (PS8)"SME Manual" },
};

static named_value_t BeaconFilter_use[] =
{
    { 0,        (PS8)"INACTIVE" },
    { 1,        (PS8)"ACTIVE" },
};

static named_value_t event_type[] = {
    { IPC_EVENT_ASSOCIATED,             (PS8)"Associated" },
    { IPC_EVENT_DISASSOCIATED,          (PS8)"Disassociated"  },
    { IPC_EVENT_LINK_SPEED,             (PS8)"LinkSpeed" },
    { IPC_EVENT_AUTH_SUCC,              (PS8)"Authentication Success" },
    { IPC_EVENT_SCAN_COMPLETE,          (PS8)"ScanComplete" },
    { IPC_EVENT_SCAN_STOPPED,           (PS8)"ScanStopped" },
#ifdef XCC_MODULE_INCLUDED
    { IPC_EVENT_CCKM_START,             (PS8)"CCKM_Start" },
#endif
    { IPC_EVENT_MEDIA_SPECIFIC,         (PS8)"Media_Specific" },
    { IPC_EVENT_EAPOL,                  (PS8)"EAPOL" },
    { IPC_EVENT_RE_AUTH_STARTED,		(PS8)"IPC_EVENT_RE_AUTH_STARTED" },
    { IPC_EVENT_RE_AUTH_COMPLETED,		(PS8)"IPC_EVENT_RE_AUTH_COMPLETED" },
    { IPC_EVENT_RE_AUTH_TERMINATED,     (PS8)"IPC_EVENT_RE_AUTH_TERMINATED" },
    { IPC_EVENT_BOUND,                  (PS8)"Bound" },
    { IPC_EVENT_UNBOUND,                (PS8)"Unbound" },
#ifdef WPA_ENTERPRISE
    { IPC_EVENT_PREAUTH_EAPOL,          (PS8)"PreAuth EAPOL"},
#endif
    { IPC_EVENT_LOW_RSSI,               (PS8)"Low RSSI" },    
    { IPC_EVENT_TSPEC_STATUS,           (PS8)"IPC_EVENT_TSPEC_STATUS" },
    { IPC_EVENT_TSPEC_RATE_STATUS,      (PS8)"IPC_EVENT_TSPEC_RATE_STATUS" },
    { IPC_EVENT_MEDIUM_TIME_CROSS,      (PS8)"IPC_EVENT_MEDIUM_TIME_CROSS" },
    { IPC_EVENT_ROAMING_COMPLETE,       (PS8)"ROAMING_COMPLETE"},
    { IPC_EVENT_EAP_AUTH_FAILURE,       (PS8)"EAP-FAST/LEAP Auth Failed"},
    { IPC_EVENT_WPA2_PREAUTHENTICATION, (PS8)"IPC_EVENT_WPA2_PREAUTHENTICATION" },
    { IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED, (PS8)"IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED" },
    { IPC_EVENT_SCAN_FAILED,			(PS8)"ScanFailed" },
    { IPC_EVENT_WPS_SESSION_OVERLAP,    (PS8)"IPC_EVENT_WPS_SESSION_OVERLAP" },
    { IPC_EVENT_RSSI_SNR_TRIGGER,       (PS8)"IPC_EVENT_RSSI_SNR_TRIGGER" },
    { IPC_EVENT_TIMEOUT,                (PS8)"Timeout" }
};

static named_value_t report_module[] =
{    
    { FILE_ID_0   ,  (PS8)"timer                   " },
    { FILE_ID_1   ,  (PS8)"measurementMgr          " },
    { FILE_ID_2   ,  (PS8)"measurementMgrSM        " },
    { FILE_ID_3   ,  (PS8)"regulatoryDomain        " },
    { FILE_ID_4   ,  (PS8)"requestHandler          " },
    { FILE_ID_5   ,  (PS8)"SoftGemini              " },
    { FILE_ID_6   ,  (PS8)"spectrumMngmntMgr       " },
    { FILE_ID_7   ,  (PS8)"SwitchChannel           " },
    { FILE_ID_8   ,  (PS8)"roamingMngr             " },
    { FILE_ID_9   ,  (PS8)"scanMngr                " },
    { FILE_ID_10  ,  (PS8)"admCtrlXCC              " },
    { FILE_ID_11  ,  (PS8)"XCCMngr                 " },
    { FILE_ID_12  ,  (PS8)"XCCRMMngr               " },
    { FILE_ID_13  ,  (PS8)"XCCTSMngr               " },
    { FILE_ID_14  ,  (PS8)"rogueAp                 " },
    { FILE_ID_15  ,  (PS8)"TransmitPowerXCC        " },
    { FILE_ID_16  ,  (PS8)"admCtrl                 " },
    { FILE_ID_17  ,  (PS8)"admCtrlNone             " },
    { FILE_ID_18  ,  (PS8)"admCtrlWep              " },
    { FILE_ID_19  ,  (PS8)"admCtrlWpa              " },
    { FILE_ID_20  ,  (PS8)"admCtrlWpa2             " },
    { FILE_ID_21  ,  (PS8)"apConn                  " },
    { FILE_ID_22  ,  (PS8)"broadcastKey802_1x      " },
    { FILE_ID_23  ,  (PS8)"broadcastKeyNone        " },
    { FILE_ID_24  ,  (PS8)"broadcastKeySM          " },
    { FILE_ID_25  ,  (PS8)"conn                    " },
    { FILE_ID_26  ,  (PS8)"connIbss                " },
    { FILE_ID_27  ,  (PS8)"connInfra               " },
    { FILE_ID_28  ,  (PS8)"keyDerive               " },
    { FILE_ID_29  ,  (PS8)"keyDeriveAes            " },
    { FILE_ID_30  ,  (PS8)"keyDeriveCkip           " },
    { FILE_ID_31  ,  (PS8)"keyDeriveTkip           " },
    { FILE_ID_32  ,  (PS8)"keyDeriveWep            " },
    { FILE_ID_33  ,  (PS8)"keyParser               " },
    { FILE_ID_34  ,  (PS8)"keyParserExternal       " },
    { FILE_ID_35  ,  (PS8)"keyParserWep            " },
    { FILE_ID_36  ,  (PS8)"mainKeysSm              " },
    { FILE_ID_37  ,  (PS8)"mainSecKeysOnly         " },
    { FILE_ID_38  ,  (PS8)"mainSecNull             " },
    { FILE_ID_39  ,  (PS8)"mainSecSm               " },
    { FILE_ID_40  ,  (PS8)"rsn                     " },
    { FILE_ID_41  ,  (PS8)"sme                     " },
    { FILE_ID_42  ,  (PS8)"smeSelect               " },
    { FILE_ID_43  ,  (PS8)"smeSm                   " },
    { FILE_ID_44  ,  (PS8)"unicastKey802_1x        " },
    { FILE_ID_45  ,  (PS8)"unicastKeyNone          " },
    { FILE_ID_46  ,  (PS8)"unicastKeySM            " },
    { FILE_ID_47  ,  (PS8)"CmdDispatcher           " },
    { FILE_ID_48  ,  (PS8)"CmdHndlr                " },
    { FILE_ID_49  ,  (PS8)"DrvMain                 " },
    { FILE_ID_50  ,  (PS8)"EvHandler               " },
    { FILE_ID_51  ,  (PS8)"Ctrl                    " },
    { FILE_ID_52  ,  (PS8)"GeneralUtil             " },
    { FILE_ID_53  ,  (PS8)"RateAdaptation          " },
    { FILE_ID_54  ,  (PS8)"rx                      " },
    { FILE_ID_55  ,  (PS8)"TrafficMonitor          " },
    { FILE_ID_56  ,  (PS8)"txCtrl                  " },
    { FILE_ID_57  ,  (PS8)"txCtrlParams            " },
    { FILE_ID_58  ,  (PS8)"txCtrlServ              " },
    { FILE_ID_59  ,  (PS8)"TxDataClsfr             " },
    { FILE_ID_60  ,  (PS8)"txDataQueue             " },
    { FILE_ID_61  ,  (PS8)"txMgmtQueue             " },
    { FILE_ID_62  ,  (PS8)"txPort                  " },
    { FILE_ID_63  ,  (PS8)"assocSM                 " },
    { FILE_ID_64  ,  (PS8)"authSm                  " },
    { FILE_ID_65  ,  (PS8)"currBss                 " },
    { FILE_ID_66  ,  (PS8)"healthMonitor           " },
    { FILE_ID_67  ,  (PS8)"mlmeBuilder             " },
    { FILE_ID_68  ,  (PS8)"mlmeParser              " },
    { FILE_ID_69  ,  (PS8)"mlmeSm                  " },
    { FILE_ID_70  ,  (PS8)"openAuthSm              " },
    { FILE_ID_71  ,  (PS8)"PowerMgr                " },
    { FILE_ID_72  ,  (PS8)"PowerMgrDbgPrint        " },
    { FILE_ID_73  ,  (PS8)"PowerMgrKeepAlive       " },
    { FILE_ID_74  ,  (PS8)"qosMngr                 " },
    { FILE_ID_75  ,  (PS8)"roamingInt              " },
    { FILE_ID_76  ,  (PS8)"ScanCncn                " },
    { FILE_ID_77  ,  (PS8)"ScanCncnApp             " },
    { FILE_ID_78  ,  (PS8)"ScanCncnOsSm            " },
    { FILE_ID_79  ,  (PS8)"ScanCncnSm              " },
    { FILE_ID_80  ,  (PS8)"ScanCncnSmSpecific      " },
    { FILE_ID_81  ,  (PS8)"scanResultTable         " },
    { FILE_ID_82  ,  (PS8)"scr                     " },
    { FILE_ID_83  ,  (PS8)"sharedKeyAuthSm         " },
    { FILE_ID_84  ,  (PS8)"siteHash                " },
    { FILE_ID_85  ,  (PS8)"siteMgr                 " },
    { FILE_ID_86  ,  (PS8)"StaCap                  " },
    { FILE_ID_87  ,  (PS8)"systemConfig            " },
    { FILE_ID_88  ,  (PS8)"templates               " },
    { FILE_ID_89  ,  (PS8)"trafficAdmControl       " },
    { FILE_ID_90  ,  (PS8)"CmdBld                  " },
    { FILE_ID_91  ,  (PS8)"CmdBldCfg               " },
    { FILE_ID_92  ,  (PS8)"CmdBldCfgIE             " },
    { FILE_ID_93  ,  (PS8)"CmdBldCmd               " },
    { FILE_ID_94  ,  (PS8)"CmdBldCmdIE             " },
    { FILE_ID_95  ,  (PS8)"CmdBldItr               " },
    { FILE_ID_96  ,  (PS8)"CmdBldItrIE             " },
    { FILE_ID_97  ,  (PS8)"CmdQueue                " },
    { FILE_ID_98  ,  (PS8)"RxQueue                 " },
    { FILE_ID_99  ,  (PS8)"txCtrlBlk               " },
    { FILE_ID_100 ,  (PS8)"txHwQueue               " },
    { FILE_ID_101 ,  (PS8)"CmdMBox                 " },
    { FILE_ID_102 ,  (PS8)"eventMbox               " },
    { FILE_ID_103 ,  (PS8)"fwDebug                 " },
    { FILE_ID_104 ,  (PS8)"FwEvent                 " },
    { FILE_ID_105 ,  (PS8)"HwInit                  " },
    { FILE_ID_106 ,  (PS8)"RxXfer                  " },
    { FILE_ID_107 ,  (PS8)"txResult                " },
    { FILE_ID_108 ,  (PS8)"txXfer                  " },
    { FILE_ID_109 ,  (PS8)"MacServices             " },
    { FILE_ID_110 ,  (PS8)"MeasurementSrv          " },
    { FILE_ID_111 ,  (PS8)"measurementSrvDbgPrint  " },
    { FILE_ID_112 ,  (PS8)"MeasurementSrvSM        " },
    { FILE_ID_113 ,  (PS8)"PowerSrv                " },
    { FILE_ID_114 ,  (PS8)"PowerSrvSM              " },
    { FILE_ID_115 ,  (PS8)"ScanSrv                 " },
    { FILE_ID_116 ,  (PS8)"ScanSrvSM               " },
    { FILE_ID_117 ,  (PS8)"TWDriver                " },
    { FILE_ID_118 ,  (PS8)"TWDriverCtrl            " },
    { FILE_ID_119 ,  (PS8)"TWDriverRadio           " },
    { FILE_ID_120 ,  (PS8)"TWDriverTx              " },
    { FILE_ID_121 ,  (PS8)"TwIf                    " },
    { FILE_ID_122 ,  (PS8)"SdioBusDrv              " },
    { FILE_ID_123 ,  (PS8)"TxnQueue                " },
    { FILE_ID_124 ,  (PS8)"WspiBusDrv              " },
    { FILE_ID_125 ,  (PS8)"context                 " },
    { FILE_ID_126 ,  (PS8)"freq                    " },
    { FILE_ID_127 ,  (PS8)"fsm                     " },
    { FILE_ID_128 ,  (PS8)"GenSM                   " },
    { FILE_ID_129 ,  (PS8)"mem                     " },
    { FILE_ID_130 ,  (PS8)"queue                   " },
    { FILE_ID_131 ,  (PS8)"rate                    " },
    { FILE_ID_132 ,  (PS8)"report                  " },
    { FILE_ID_133 ,  (PS8)"stack                   " },
	{ FILE_ID_134 ,  (PS8)"externalSec             " },
	{ FILE_ID_135 ,  (PS8)"roamingMngr_autoSM      " },
	{ FILE_ID_136 ,  (PS8)"roamingMngr_manualSM    " },
	{ FILE_ID_137 ,  (PS8)"cmdinterpretoid         " },
	{ FILE_ID_138 ,  (PS8)"WlanDrvIf               " }
};

static named_value_t report_severity[] = {
    { 0,                          (PS8)"----"           },
    { REPORT_SEVERITY_INIT,         (PS8)"INIT",          },
    { REPORT_SEVERITY_INFORMATION,  (PS8)"INFORMATION",   },
    { REPORT_SEVERITY_WARNING,      (PS8)"WARNING",       },
    { REPORT_SEVERITY_ERROR,        (PS8)"ERROR",         },
    { REPORT_SEVERITY_FATAL_ERROR,  (PS8)"FATAL_ERROR",   },
    { REPORT_SEVERITY_SM,           (PS8)"SM",            },
    { REPORT_SEVERITY_CONSOLE,      (PS8)"CONSOLE"        }
};

static named_value_t power_level[] = {
        { OS_POWER_LEVEL_ELP,       (PS8)"Extreme Low Power" },
        { OS_POWER_LEVEL_PD,        (PS8)"Power Down" },
        { OS_POWER_LEVEL_AWAKE,     (PS8)"Awake" },
};

static named_value_t band2Str[] = {
        { RADIO_BAND_2_4_GHZ,                   (PS8)"2.4 GHz"                        },
        { RADIO_BAND_5_0_GHZ,                   (PS8)"5.0 GHz"                        },
        { RADIO_BAND_DUAL,                      (PS8)"Both   "                        }
};

static named_value_t EtEvent2Str[] = {
        { SCAN_ET_COND_DISABLE,                 (PS8)"ET disabled  "                     },
        { SCAN_ET_COND_BEACON,                  (PS8)"ET on Beacon "                     },
        { SCAN_ET_COND_PROBE_RESP,              (PS8)"ET on Prb Rsp"                     },
        { SCAN_ET_COND_ANY_FRAME,               (PS8)"ET on both   "                     }
};

static named_value_t rate2Str[] = {
        { DRV_RATE_MASK_AUTO,                   (PS8)"Auto    "                          },
        { DRV_RATE_MASK_1_BARKER,               (PS8)"1 Mbps  "                          },
        { DRV_RATE_MASK_2_BARKER,               (PS8)"2 Mbps  "                          },
        { DRV_RATE_MASK_5_5_CCK,                (PS8)"5.5 Mbps"                          },
        { DRV_RATE_MASK_11_CCK,                 (PS8)"11 Mbps "                          },
        { DRV_RATE_MASK_22_PBCC,                (PS8)"22 Mbps "                          },
        { DRV_RATE_MASK_6_OFDM,                 (PS8)"6 Mbps  "                          },
        { DRV_RATE_MASK_9_OFDM,                 (PS8)"9 Mbps  "                          },
        { DRV_RATE_MASK_12_OFDM,                (PS8)"12 Mbps "                          },
        { DRV_RATE_MASK_18_OFDM,                (PS8)"18 Mbps "                          },
        { DRV_RATE_MASK_24_OFDM,                (PS8)"24 Mbps "                          },
        { DRV_RATE_MASK_36_OFDM,                (PS8)"36 Mbps "                          },
        { DRV_RATE_MASK_48_OFDM,                (PS8)"48 Mbps "                          },
        { DRV_RATE_MASK_54_OFDM,                (PS8)"54 Mbps "                          }
};

static named_value_t scanType2Str[] = {
        { SCAN_TYPE_NORMAL_PASSIVE,             (PS8)"Passive Normal Scan"               },
        { SCAN_TYPE_NORMAL_ACTIVE,              (PS8)"Active Normal Scan"                },
        { SCAN_TYPE_SPS,                        (PS8)"Scheduled Passive Scan (SPS)"      },
        { SCAN_TYPE_TRIGGERED_PASSIVE,          (PS8)"Passive Triggered Scan"            },
        { SCAN_TYPE_TRIGGERED_ACTIVE,           (PS8)"Active Triggered Scan"             }
};

static named_value_t booleanStr[] = {
        { FALSE,                             (PS8)"False" },
        { TRUE,                              (PS8)"True" }
};

static named_value_t ssidVisabilityStr[] = {
        { SCAN_SSID_VISABILITY_PUBLIC,          (PS8)"Public" },
        { SCAN_SSID_VISABILITY_HIDDEN,          (PS8)"Hidden" }
};

static named_value_t bssTypeStr[] = {
        { BSS_INDEPENDENT,                      (PS8)"Independent" },
        { BSS_INFRASTRUCTURE,                   (PS8)"Infrastructure" },
        { BSS_ANY,                              (PS8)"Any" }
};

static named_value_t power_mode_val[] = {
        { OS_POWER_MODE_AUTO,                   (PS8)"AUTO" },
        { OS_POWER_MODE_ACTIVE,                 (PS8)"ACTIVE" },
        { OS_POWER_MODE_SHORT_DOZE,             (PS8)"SHORT_DOZE" },
        { OS_POWER_MODE_LONG_DOZE,              (PS8)"LONG_DOZE" }
};

static named_value_t encrypt_type[] = {
        { OS_ENCRYPTION_TYPE_NONE,              (PS8)"None" },
        { OS_ENCRYPTION_TYPE_WEP,               (PS8)"WEP" },
        { OS_ENCRYPTION_TYPE_TKIP,              (PS8)"TKIP" },
        { OS_ENCRYPTION_TYPE_AES,               (PS8)"AES" }
};

static named_value_t tKeepAliveTriggerTypes[] = {
        { KEEP_ALIVE_TRIG_TYPE_NO_TX,           (PS8)"When Idle" },
        { KEEP_ALIVE_TRIG_TYPE_PERIOD_ONLY,     (PS8)"Always" }
};

#if 0 /* need to create debug logic for CLI */
static named_value_t cli_level_type[] = {
        { CU_MSG_DEBUG,                         (PS8)"CU_MSG_DEBUG" },
        { CU_MSG_INFO1,                         (PS8)"CU_MSG_INFO1" },
        { CU_MSG_WARNING,                       (PS8)"CU_MSG_WARNING" },        
        { CU_MSG_ERROR,                         (PS8)"CU_MSG_ERROR" },
        { CU_MSG_INFO2,                         (PS8)"CU_MSG_INFO2" }
};
#endif


static char *ConnState[] = {
  "IDLE",
  "SCANNING",
  "CONNECTING",
  "CONNECTED",
  "DISCONNECT",
  "IDLE"
};



static char ssidBuf[MAX_SSID_LEN +1];

/* local fucntions */
/*******************/
static S32 CuCmd_Str2MACAddr(PS8 str, PU8 mac)
{
    S32 i;   
    
    for( i=0; i<MAC_ADDR_LEN; i++ )
    {
        mac[i] = (U8) os_strtoul(str, &str, 16);
        str++;
    }    
    return TRUE;
}

/* used in get_bssid_list() */
static U8 CuCmd_Freq2Chan(U32 freq)
{
    U32 i;

    for(i=0; i<CHAN_FREQ_TABLE_SIZE; i++)
        if(ChanFreq[i].freq == freq) 
            return ChanFreq[i].chan;

    return 0;
}

/* Converts a single ASCII character to a hex value (i.e. '0'-'9' = 0-9, 'a'-'f' = a-f, 'A'-'F' = a-f) */
static U8 CuCmd_atox(U8 c)
{
    if (('0' <= c) && ('9' >= c))
    {
        return c - '0';
    }
    else if (('a' <= c) && ('f' >= c))
    {
        return c - 'a' + 10;
    }
    else /* assuming input is valid */
    {
        return c - 'A' + 10;
    }
}

/* converts an ASCII string to a buffer */
static void CuCmd_atox_string (U8* srcString, U8* dstBuffer)
{
    U32 uIndex, uLength;
    
    uLength = os_strlen ((PS8)srcString);

    /* clear the destination buffer */
    os_memset (dstBuffer, 0, (uLength / 2) + 1);

    for (uIndex = 0; uIndex < uLength; uIndex++)
    {
        if (0 == (uIndex % 2))
        {
            dstBuffer[ uIndex / 2 ] |= (CuCmd_atox (srcString[ uIndex ]) << 4);
        }
        else
        {
            dstBuffer[ uIndex / 2 ] |= CuCmd_atox (srcString[ uIndex ]);
        }
    }
}

static void CuCmd_xtoa_string (U8* srcBuffer, U32 srcBufferLength, U8* dstString)
{
    U32 uIndex;

    for (uIndex = 0; uIndex < srcBufferLength; uIndex++)
    {
        os_sprintf ((PS8)&(dstString[ uIndex * 2 ]), (PS8)"%02x", srcBuffer[ uIndex ]);
    }
}

static VOID CuCmd_Init_Scan_Params(CuCmd_t* pCuCmd)
{
    U8 i,j;

    /* init application scan default params */
    pCuCmd->appScanParams.desiredSsid.len = 0;
    pCuCmd->appScanParams.scanType = SCAN_TYPE_NORMAL_ACTIVE;
    pCuCmd->appScanParams.band = RADIO_BAND_2_4_GHZ;
    pCuCmd->appScanParams.probeReqNumber = 3;
    pCuCmd->appScanParams.probeRequestRate = RATE_MASK_UNSPECIFIED; /* Let the FW select */;
    pCuCmd->appScanParams.numOfChannels = 14;
    for ( i = 0; i < 14; i++ )
    {
        for ( j = 0; j < 6; j++ )
        {
            pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry.bssId[ j ] = 0xff;
        }
        pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
        pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry.ETMaxNumOfAPframes = 0;
        pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry.maxChannelDwellTime = 60000;
        pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry.minChannelDwellTime = 30000;
        pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry.txPowerDbm = DEF_TX_POWER;
        pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry.channel = i + 1;
    }

    /* init periodic application scan params */
    pCuCmd->tPeriodicAppScanParams.uSsidNum = 0;
    pCuCmd->tPeriodicAppScanParams.uSsidListFilterEnabled = 1;
    pCuCmd->tPeriodicAppScanParams.uCycleNum = 0; /* forever */
    pCuCmd->tPeriodicAppScanParams.uCycleIntervalMsec[ 0 ] = 3;
    for (i = 1; i < PERIODIC_SCAN_MAX_INTERVAL_NUM; i++)
    {
        pCuCmd->tPeriodicAppScanParams.uCycleIntervalMsec[ i ] = 30000;
    }
    pCuCmd->tPeriodicAppScanParams.iRssiThreshold = -80;
    pCuCmd->tPeriodicAppScanParams.iSnrThreshold = 0;
    pCuCmd->tPeriodicAppScanParams.uFrameCountReportThreshold = 1;
    pCuCmd->tPeriodicAppScanParams.bTerminateOnReport = TRUE;
    pCuCmd->tPeriodicAppScanParams.eBssType = BSS_ANY;
    pCuCmd->tPeriodicAppScanParams.uProbeRequestNum = 3;
    pCuCmd->tPeriodicAppScanParams.uChannelNum = 14;
    for ( i = 0; i < 14; i++ )
    {
        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].eBand = RADIO_BAND_2_4_GHZ;
        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uChannel = i + 1;
        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].eScanType = SCAN_TYPE_NORMAL_ACTIVE;
        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uMinDwellTimeMs = 5;
        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uMaxDwellTimeMs = 20;
        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uTxPowerLevelDbm = DEF_TX_POWER;
    }

    /* init default scan policy */
    pCuCmd->scanPolicy.normalScanInterval = 10000;
    pCuCmd->scanPolicy.deterioratingScanInterval = 5000;
    pCuCmd->scanPolicy.maxTrackFailures = 3;
    pCuCmd->scanPolicy.BSSListSize = 4;
    pCuCmd->scanPolicy.BSSNumberToStartDiscovery = 1;
    pCuCmd->scanPolicy.numOfBands = 1;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].band = RADIO_BAND_2_4_GHZ;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].rxRSSIThreshold = -80;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].numOfChannles = 14;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].numOfChannlesForDiscovery = 3;
    for ( i = 0; i < 14; i++ )
    {
        pCuCmd->scanPolicy.bandScanPolicy[ 0 ].channelList[ i ] = i + 1;
    }
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.scanType = SCAN_TYPE_NORMAL_ACTIVE;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.maxChannelDwellTime = 30000;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.minChannelDwellTime = 15000;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.bitrate = RATE_MASK_UNSPECIFIED; /* Let the FW select */
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 3;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].trackingMethod.method.basicMethodParams.probReqParams.txPowerDbm = DEF_TX_POWER;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.scanType = SCAN_TYPE_NORMAL_ACTIVE;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.maxChannelDwellTime = 30000;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.minChannelDwellTime = 15000;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.bitrate = RATE_MASK_UNSPECIFIED; /* Let the FW select */;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 3;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].discoveryMethod.method.basicMethodParams.probReqParams.txPowerDbm = DEF_TX_POWER;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.scanType = SCAN_TYPE_NORMAL_ACTIVE;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.earlyTerminationEvent = SCAN_ET_COND_DISABLE;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.ETMaxNumberOfApFrames = 0;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.maxChannelDwellTime = 30000;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.minChannelDwellTime = 15000;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.probReqParams.bitrate = RATE_MASK_UNSPECIFIED; /* Let the FW select */;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = 3;
    pCuCmd->scanPolicy.bandScanPolicy[ 0 ].immediateScanMethod.method.basicMethodParams.probReqParams.txPowerDbm = DEF_TX_POWER;
}

char* PrintSSID(OS_802_11_SSID* ssid)
{
    /* It looks like it never happens. Anyway decided to check */
	if(ssid->SsidLength > MAX_SSID_LEN)
    {
        
        os_error_printf(CU_MSG_ERROR, (PS8)"PrintSSID. ssid->SsidLength=%d exceeds the limit %d\n",
                         ssid->SsidLength, MAX_SSID_LEN);
        /*WLAN_OS_REPORT(("PrintSSID. ssid->SsidLength=%d exceeds the limit %d\n",
                   ssid->SsidLength, MAX_SSID_LEN));*/
        ssid->SsidLength = MAX_SSID_LEN;
    }
	os_memcpy((PVOID)ssidBuf, (PVOID) ssid->Ssid, ssid->SsidLength); 
	ssidBuf[ssid->SsidLength] = '\0';
	return ssidBuf;
}

static VOID CuCmd_PrintBssidList(OS_802_11_BSSID_LIST_EX* bssidList, S32 IsFullPrint, TMacAddr CurrentBssid)
{
    U32 i;
    S8  connectionTypeStr[50];
    POS_802_11_BSSID_EX pBssid = &bssidList->Bssid[0];

    os_error_printf(CU_MSG_INFO2, (PS8)"BssId List: Num=%u\n", bssidList->NumberOfItems);
    os_error_printf(CU_MSG_INFO2, (PS8)"         MAC        Privacy Rssi  Mode    Channel    SSID\n");
    for(i=0; i<bssidList->NumberOfItems; i++)
    {            
        switch (pBssid->InfrastructureMode)
        {
            case os802_11IBSS:
                os_strcpy (connectionTypeStr, (PS8)"Adhoc");
                break;
            case os802_11Infrastructure:
                os_strcpy (connectionTypeStr, (PS8)"Infra");
                break;
            case os802_11AutoUnknown:
                os_strcpy (connectionTypeStr, (PS8)"Auto");
                break;
            default:
                os_strcpy (connectionTypeStr, (PS8)" --- ");
                break;
        }
        os_error_printf(CU_MSG_INFO2, (PS8)"%s%02x.%02x.%02x.%02x.%02x.%02x   %3u   %4d  %s %6d        %s\n",
            (!os_memcmp(CurrentBssid, pBssid->MacAddress, MAC_ADDR_LEN))?"*":" ",
            pBssid->MacAddress[0],
            pBssid->MacAddress[1],
            pBssid->MacAddress[2],
            pBssid->MacAddress[3],
            pBssid->MacAddress[4],
            pBssid->MacAddress[5],
            pBssid->Privacy, 
			(char)pBssid->Rssi | 0xffffff00, /* need the 0xffffff00 to get negative value display */
            connectionTypeStr,
            CuCmd_Freq2Chan(pBssid->Configuration.Union.channel),
            (pBssid->Ssid.Ssid[0] == '\0')?(PS8)"****":((PS8)pBssid->Ssid.Ssid) );

        if (IsFullPrint)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"   BeaconInterval %d\n",  pBssid->Configuration.BeaconPeriod);
            os_error_printf(CU_MSG_INFO2, (PS8)"   Capabilities   0x%x\n",  pBssid->Capabilities);
        }
#ifdef _WINDOWS
		pBssid = (POS_802_11_BSSID_EX)((S8*)pBssid + (pBssid->Length ? pBssid->Length : sizeof(OS_802_11_BSSID_EX)));
#else /*for Linux*/
		pBssid = &bssidList->Bssid[i+1];
#endif
    }        
}

static U8 CuCmd_Char2Hex( S8 c )
{
    if( c >= '0' && c <= '9' )
        return c - '0';
    else if( os_tolower(c) >= 'a' && os_tolower(c) <= 'f' )
        return (U8) (os_tolower(c) - 'a' + 0x0a);
    os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Char2Hex - invalid symbol '%c'\n", c );
    return ((U8)-1);
}

static PS8 CuCmd_CreateRateStr(PS8 str, U8 rate)
{
    if( rate == 0 )
        return os_strcpy(str,(PS8)"Auto (0)");

    os_sprintf(str, (PS8)"%.3g Mbps", 
        RATE_2_MBPS(rate));

    return str;
}

static VOID CuCmd_PrintScanMethod(scan_Method_t* scanMethod)
{
    S32 i;
    os_error_printf(CU_MSG_INFO2, (PS8)"Scan type: %s\n", scanType2Str[ scanMethod->scanType ].name);
    switch (scanMethod->scanType)
    {
        case SCAN_TYPE_NORMAL_ACTIVE:
        case SCAN_TYPE_NORMAL_PASSIVE:
            os_error_printf(CU_MSG_INFO2, (PS8)"Max channel dwell time: %d, Min channel dwell time: %d\n",
                scanMethod->method.basicMethodParams.maxChannelDwellTime, 
                scanMethod->method.basicMethodParams.minChannelDwellTime);
            
            CU_CMD_FIND_NAME_ARRAY(i, EtEvent2Str, scanMethod->method.basicMethodParams.earlyTerminationEvent);
            os_error_printf(CU_MSG_INFO2 ,(PS8)"ET condition: %s, ET number of frames: %d\n",
                EtEvent2Str[i].name, 
                scanMethod->method.basicMethodParams.ETMaxNumberOfApFrames);

            CU_CMD_FIND_NAME_ARRAY(i, rate2Str, scanMethod->method.basicMethodParams.probReqParams.bitrate);
            os_error_printf(CU_MSG_INFO2 ,(PS8)"Probe request number: %d, probe request rate: %s, TX level: %d\n",
                scanMethod->method.basicMethodParams.probReqParams.numOfProbeReqs, 
                rate2Str[i].name,
                scanMethod->method.basicMethodParams.probReqParams.txPowerDbm);
            break;

        case SCAN_TYPE_TRIGGERED_ACTIVE:
        case SCAN_TYPE_TRIGGERED_PASSIVE:
            os_error_printf(CU_MSG_INFO2, (PS8)"Triggering Tid: %d\n", scanMethod->method.TidTriggerdMethodParams.triggeringTid);
            os_error_printf(CU_MSG_INFO2, (PS8)"Max channel dwell time: %d, Min channel dwell time: %d\n",
                scanMethod->method.basicMethodParams.maxChannelDwellTime, 
                scanMethod->method.basicMethodParams.minChannelDwellTime);
            
            CU_CMD_FIND_NAME_ARRAY(i, EtEvent2Str, scanMethod->method.basicMethodParams.earlyTerminationEvent);
            os_error_printf(CU_MSG_INFO2, (PS8)"ET condition: %s, ET number of frames: %d\n",
                EtEvent2Str[i].name, 
                scanMethod->method.basicMethodParams.ETMaxNumberOfApFrames);

            CU_CMD_FIND_NAME_ARRAY(i, rate2Str, scanMethod->method.basicMethodParams.probReqParams.bitrate);
            os_error_printf(CU_MSG_INFO2, (PS8)"Probe request number: %d, probe request rate: %s, TX level: %d\n",
                scanMethod->method.basicMethodParams.probReqParams.numOfProbeReqs, 
                rate2Str[i].name,
                scanMethod->method.basicMethodParams.probReqParams.txPowerDbm);
            break;

        case SCAN_TYPE_SPS:
            CU_CMD_FIND_NAME_ARRAY(i, EtEvent2Str, scanMethod->method.spsMethodParams.earlyTerminationEvent);
            os_error_printf(CU_MSG_INFO2, (PS8)"ET condition: %s, ET number of frames: %d\n",
                EtEvent2Str[i].name, 
                scanMethod->method.spsMethodParams.ETMaxNumberOfApFrames);
            os_error_printf(CU_MSG_INFO2, (PS8)"Scan duration: %d\n", scanMethod->method.spsMethodParams.scanDuration);
            break;

        case SCAN_TYPE_NO_SCAN:
        case SCAN_TYPE_PACTSIVE:
            break;
    }
}

static VOID CuCmd_PrintScanBand(scan_bandPolicy_t* pBandPolicy)
{
    S32 j;

    os_error_printf(CU_MSG_INFO2, (PS8)"\nBand: %s\n", band2Str[ pBandPolicy->band ].name);
    os_error_printf(CU_MSG_INFO2, (PS8)"RSSI Threshold: %d dBm\n", pBandPolicy->rxRSSIThreshold);
    os_error_printf(CU_MSG_INFO2, (PS8)"Number of channels for each discovery interval: %d\n", pBandPolicy->numOfChannlesForDiscovery);
    os_error_printf(CU_MSG_INFO2, (PS8)"\nTracking Method:\n");
    CuCmd_PrintScanMethod( &(pBandPolicy->trackingMethod) );
    os_error_printf(CU_MSG_INFO2, (PS8)"\nDiscovery Method:\n");
    CuCmd_PrintScanMethod( &(pBandPolicy->discoveryMethod) );
    os_error_printf(CU_MSG_INFO2, (PS8)"\nImmediate Scan Method:\n");
    CuCmd_PrintScanMethod( &(pBandPolicy->immediateScanMethod) );
    if ( pBandPolicy->numOfChannles > 0 )
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"\nChannel list: ");
        for ( j = 0; j < pBandPolicy->numOfChannles; j++ )
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"%3d ", pBandPolicy->channelList[ j ]);
        }
        os_error_printf(CU_MSG_INFO2, (PS8)"\n");
    }
    else
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"\nNo channels defined.\n");
    }
}

static U32 CuCmd_IsValueRate(U32 rate)
{

    switch (rate)
    {
        case 1:
        case 2:
        case 5:
        case 6:
        case 9:
        case 11:
        case 12:
        case 18:
        case 24:
        case 36:
        case 48:
        case 54:
            return (TRUE);
            
        default:
            return (FALSE);
   }   
}

static VOID CuCmd_ParseMaskString(PS8 pString, PU8 pBuffer, PU8 pLength)
{
    S8  ch;
    S32 iter = 0;
    U8  val;

    while ((ch = pString[iter]))
    {
        val = (ch == '1' ? 1 : 0);

        if (iter % BIT_TO_BYTE_FACTOR)
            pBuffer[iter / BIT_TO_BYTE_FACTOR] |= (val << (iter % BIT_TO_BYTE_FACTOR));
        else
            pBuffer[iter / BIT_TO_BYTE_FACTOR] = val;

        ++iter;
    }

    /* iter = 0 len = 0, iter = 1 len = 1, iter = 8 len = 1, and so on... */
    *pLength = (U8) (iter + BIT_TO_BYTE_FACTOR - 1) / BIT_TO_BYTE_FACTOR;
}

static VOID CuCmd_ParsePatternString(PS8 pString, PU8 pBuffer, PU8 pLength)
{
    S8 ch;
    S32 iter = 0;
    U8 val;

    while ((ch = pString[iter]))
    {
        val = ((ch >= '0' && ch <= '9') ? (ch - '0') : 
                     (ch >= 'A' && ch <= 'F') ? (0xA + ch - 'A') :
                     (ch >= 'a' && ch <= 'f') ? (0xA + ch - 'a') : 0);

        /* even indexes go to the lower nibble, odd indexes push them to the */
        /* higher nibble and then go themselves to the lower nibble. */
        if (iter % 2)
            pBuffer[iter / 2] = ((pBuffer[iter / 2] << (BIT_TO_BYTE_FACTOR / 2)) | val);
        else
            pBuffer[iter / 2] = val;

        ++iter;
    }

    /* iter = 0 len = 0, iter = 1 len = 1, iter = 2 len = 1, and so on... */
    *pLength = (U8) (iter + 1) / 2;
}

/* functions */
/*************/
THandle CuCmd_Create(const PS8 device_name, THandle hConsole, S32 BypassSupplicant, PS8 pSupplIfFile)
{
    THandle hIpcSta;

    CuCmd_t* pCuCmd = (CuCmd_t*)os_MemoryCAlloc(sizeof(CuCmd_t), sizeof(U8));
    if(pCuCmd == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Create - cant allocate control block\n");
        return NULL;
    }

    pCuCmd->isDeviceRunning = FALSE;
    pCuCmd->hConsole = hConsole;
    
    pCuCmd->hCuCommon= CuCommon_Create(&hIpcSta, device_name);
    if(pCuCmd->hCuCommon == NULL)
    {   
        CuCmd_Destroy(pCuCmd);
        return NULL;
    }

    pCuCmd->hCuWext= CuOs_Create(hIpcSta);
    if(pCuCmd->hCuWext == NULL)
    {   
        CuCmd_Destroy(pCuCmd);
        return NULL;
    }

    pCuCmd->hIpcEvent = (THandle) IpcEvent_Create();
    if(pCuCmd->hIpcEvent == NULL)
    {   
        CuCmd_Destroy(pCuCmd);
        return NULL;
    }

    if(BypassSupplicant)
    {
        /* specify that there is no supplicant */
        pCuCmd->hWpaCore = NULL;
    }
    else
    {
#ifndef NO_WPA_SUPPL
        S32 res;

        pCuCmd->hWpaCore = WpaCore_Create(&res, pSupplIfFile);
        if((pCuCmd->hWpaCore == NULL) && (res != EOALERR_IPC_WPA_ERROR_CANT_CONNECT_TO_SUPPL))
        {
            CuCmd_Destroy(pCuCmd);
            return NULL;
        }

        if(res == EOALERR_IPC_WPA_ERROR_CANT_CONNECT_TO_SUPPL)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"******************************************************\n");
            os_error_printf(CU_MSG_ERROR, (PS8)"Connection to supplicant failed\n");
            os_error_printf(CU_MSG_ERROR, (PS8)"******************************************************\n");
        }
        else
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Connection established with supplicant\n");
        }
#endif
    }

    CuCmd_Init_Scan_Params(pCuCmd);

    return pCuCmd;
}

VOID CuCmd_Destroy(THandle hCuCmd)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if(pCuCmd->hCuCommon)
    {
        CuCommon_Destroy(pCuCmd->hCuCommon);
    }

    if(pCuCmd->hCuWext)
    {
        CuOs_Destroy(pCuCmd->hCuWext);
    }

    if(pCuCmd->hIpcEvent)
    {
        IpcEvent_Destroy(pCuCmd->hIpcEvent);
    }

#ifndef NO_WPA_SUPPL
    if(pCuCmd->hWpaCore)
    {
        WpaCore_Destroy(pCuCmd->hWpaCore);
    }
#endif

    os_MemoryFree(pCuCmd);
}

S32 CuCmd_GetDeviceStatus(THandle hCuCmd)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 status;

    status = CuCommon_GetU32(pCuCmd->hCuCommon, DRIVER_STATUS_PARAM, &pCuCmd->isDeviceRunning);

    if ((status == OK) && pCuCmd->isDeviceRunning)
        return OK;  

    return ECUERR_CU_CMD_ERROR_DEVICE_NOT_LOADED;
}

VOID CuCmd_StartDriver(THandle hCuCmd)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 uDummyBuf;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, DRIVER_START_PARAM, &uDummyBuf, sizeof(uDummyBuf))) 
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - Failed to start driver!\n");
    }
}

VOID CuCmd_StopDriver(THandle hCuCmd)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 uDummyBuf;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, DRIVER_STOP_PARAM, &uDummyBuf, sizeof(uDummyBuf))) 
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - Failed to stop driver!\n");
    }
}

#ifdef XCC_MODULE_INCLUDED
THandle CuCmd_GetCuCommonHandle(THandle hCuCmd)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    return pCuCmd->hCuCommon;
}

THandle CuCmd_GetCuWpaHandle (THandle hCuCmd)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    return pCuCmd->hWpaCore;

}
#endif

VOID CuCmd_Show_Status(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    
    TMacAddr Mac;
    OS_802_11_SSID ssid;
    TMacAddr bssid;
    U32 channel, threadid=0;
    U32 status ;

    if(OK != CuCmd_GetDeviceStatus(hCuCmd))
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Driver is stopped!\n");
        return;
    }

    CuOs_GetDriverThreadId(pCuCmd->hCuWext, &threadid);
    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, CTRL_DATA_MAC_ADDRESS, Mac, sizeof(TMacAddr))) return;     
    if(OK != CuOs_Get_SSID(pCuCmd->hCuWext, &ssid)) return;
    if(OK != CuOs_Get_BSSID(pCuCmd->hCuWext, bssid)) return;
    if(OK != CuOs_GetCurrentChannel(pCuCmd->hCuWext, &channel)) return;
    if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, SME_CONNECTION_STATUS_PARAM, &status)) 
      os_error_printf(CU_MSG_INFO2, (PS8)"can't read connection status!\n");
    
    if (threadid != 0)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Thread id: %u (0x%x)\n\n", threadid, threadid);
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"==========================\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"Status   : %s\n",ConnState[status]);
    os_error_printf(CU_MSG_INFO2, (PS8)"MAC      : %02x.%02x.%02x.%02x.%02x.%02x\n",Mac[0],Mac[1],Mac[2],Mac[3],Mac[4],Mac[5]);
    os_error_printf(CU_MSG_INFO2, (PS8)"SSID     : %s\n",(ssid.SsidLength)?PrintSSID(&ssid):"<empty>");
    os_error_printf(CU_MSG_INFO2, (PS8)"BSSID    : %02x.%02x.%02x.%02x.%02x.%02x\n",bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
    if(channel)
        os_error_printf(CU_MSG_INFO2, (PS8)"Channel  : %d\n",channel);  
    else
        os_error_printf(CU_MSG_INFO2, (PS8)"Channel  : <empty>\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"==========================\n");
    
}

VOID CuCmd_BssidList(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 SizeOfBssiList=0;
    OS_802_11_BSSID_LIST_EX* bssidList;
    TMacAddr bssid;

    if(OK != CuCommon_Get_BssidList_Size(pCuCmd->hCuCommon, &SizeOfBssiList))
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Bssid_List - cant get Bssid list size\n");
        return;
    }

    /* allocate the bssidList */
    bssidList = os_MemoryCAlloc(SizeOfBssiList, sizeof(U8));
    if(bssidList == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Bssid_List - cant allocate Bssid list\n");
        return;
    }

    if(SizeOfBssiList == sizeof(U32))
    {
        /* means that bssidList is empty*/
        bssidList->NumberOfItems = 0;
        /* os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Bssid_List - size of list is %d, indicating empty list\n",
                        sizeof(U32)); */
    }
    else
    {
        if (OK != CuOs_GetBssidList(pCuCmd->hCuWext, bssidList))
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Bssid_List - cant get Bssid list\n");
            os_MemoryFree(bssidList);
            return;
        }
    }

    /* get currently connected bssid */
    if(OK != CuOs_Get_BSSID(pCuCmd->hCuWext, bssid))
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Bssid_List - cant get current BSSID\n");
        os_MemoryFree(bssidList);
        return;
    }

    /* print the list to the terminal */
    CuCmd_PrintBssidList(bssidList, FALSE, bssid);    
    
    /* free the bssidList */
    os_MemoryFree(bssidList);
    
}

VOID CuCmd_FullBssidList(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 SizeOfBssiList=0;
    OS_802_11_BSSID_LIST_EX* bssidList;
    TMacAddr bssid;

    if(OK != CuCommon_Get_BssidList_Size(pCuCmd->hCuCommon, &SizeOfBssiList)) return;

    /* allocate the bssidList */
    bssidList = os_MemoryCAlloc(SizeOfBssiList, sizeof(U8));
    if(bssidList == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_Bssid_List - cant allocate Bssid list\n");
        return;
    }

    if(SizeOfBssiList == sizeof(U32))
    {
        /* means that bssidList is empty*/
        bssidList->NumberOfItems = 0;
            
    }
    else
    {
        if(OK != CuOs_GetBssidList(pCuCmd->hCuWext, bssidList) ) return;
    }

    /* get currently connected bssid */
    if(OK != CuOs_Get_BSSID(pCuCmd->hCuWext, bssid)) return;
    
    /* print the list to the terminal */
    CuCmd_PrintBssidList(bssidList, TRUE, bssid);
    
    /* free the bssidList */
    os_MemoryFree(bssidList);
}

#if defined(CONFIG_WPS) && !defined(NO_WPA_SUPPL)
VOID CuCmd_StartEnrolleePIN(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - Cannot start Enrollee without connection to supplicant\n");
        return;         
    }
    
    WpaCore_StartWpsPIN(pCuCmd->hWpaCore);
    os_error_printf(CU_MSG_INFO2, (PS8)"WPS in PIN mode started\n");
}

VOID CuCmd_StartEnrolleePBC(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - Cannot start Enrollee push button without connection to supplicant\n");
        return;         
    }
    
    WpaCore_StartWpsPBC(pCuCmd->hWpaCore);  
    os_error_printf(CU_MSG_INFO2, (PS8)"WPS in PBC mode started\n");
}

VOID CuCmd_StopEnrollee(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - Cannot Stop Enrollee without connection to supplicant\n");
        return;         
    }
    
    WpaCore_StopWps(pCuCmd->hWpaCore);
    os_error_printf(CU_MSG_INFO2, (PS8)"WPS mode stopped\n");
}

VOID CuCmd_SetPin(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - Cannot set PIN without connection to supplicant\n");
        return;         
    }
    
    WpaCore_SetPin(pCuCmd->hWpaCore, (PS8)parm[0].value); 
    os_error_printf(CU_MSG_INFO2, (PS8)"WPS PIN set %s\n", parm[0].value);
}

#endif /* CONFIG_WPS */

VOID CuCmd_Connect(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TMacAddr bssid = { 0xff,0xff,0xff,0xff,0xff,0xff };
    OS_802_11_SSID ssid;
    U32 BssType;

    switch (nParms) 
    {
        case 0 :
            /* 
            *  No SSID & No BSSID are set -
            *  Use Any SSID & Any BSSID.
            */
            ssid.SsidLength = 0;            
            ssid.Ssid[0] = 0;
            break;
        case 1:
            /* 
            *  SSID set & BSSID insn't set  -
            *  Use CLI's SSID & Any BSSID.
            */
            ssid.SsidLength = os_strlen( (PS8)parm[0].value);
            os_memcpy((PVOID)ssid.Ssid, (PVOID) parm[0].value, ssid.SsidLength);
            ssid.Ssid[ssid.SsidLength] = '\0';
            break;
        case 2:
            /* 
             *  Both SSID & BSSID are set -
             *  Use CLI's SSID & BSSID.
             */
            if(!CuCmd_Str2MACAddr( (PS8)parm[1].value, bssid) )
                return;
            ssid.SsidLength = os_strlen((PS8) parm[0].value);
            os_memcpy((PVOID)ssid.Ssid, (PVOID) parm[0].value, ssid.SsidLength);
            ssid.Ssid[ssid.SsidLength] = '\0';
            break;
    }

    if(pCuCmd->hWpaCore == NULL)
    {
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, CTRL_DATA_CURRENT_BSS_TYPE_PARAM, &BssType)) return;

        if((BssType == os802_11IBSS/* Ad-Hoc */) && (ssid.SsidLength == 0))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"SSID string is needed due to fact that BSS Type set to Ad-Hoc.\n");
            return;
        }

        if(OK != CuOs_Set_BSSID(pCuCmd->hCuWext, bssid ) ) return;
        if(OK != CuOs_Set_ESSID(pCuCmd->hCuWext, &ssid) ) return;
    }
    else
    {
#ifndef NO_WPA_SUPPL
        if(OK != WpaCore_GetBssType(pCuCmd->hWpaCore, &BssType)) return;

        if((BssType == os802_11IBSS/* Ad-Hoc */) && (ssid.SsidLength == 0))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"SSID string is needed due to fact BSS Type set to Ad-Hoc\n");
            return;
        }

        WpaCore_SetSsid(pCuCmd->hWpaCore, &ssid, bssid);
#endif
    }
}

VOID CuCmd_Disassociate(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if(pCuCmd->hWpaCore == NULL)
    {
        OS_802_11_SSID ssid;
        
        ssid.SsidLength = MAX_SSID_LEN;
        os_memset(ssid.Ssid, 0, MAX_SSID_LEN);
        
        CuOs_Set_ESSID(pCuCmd->hCuWext, &ssid);
    }
    else
    {
#ifndef NO_WPA_SUPPL
        WpaCore_Disassociate(pCuCmd->hWpaCore);
#endif
    }
}

VOID CuCmd_ModifySsid(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_SSID ssid;
    TMacAddr bssid = { 0xff,0xff,0xff,0xff,0xff,0xff };    
    
    if( nParms == 0 )
    {
        OS_802_11_SSID ssid;
        if(OK != CuOs_Get_SSID(pCuCmd->hCuWext, &ssid)) return;
        os_error_printf(CU_MSG_INFO2, (PS8)"SSID: %s\n",(ssid.SsidLength)?(PS8)PrintSSID(&ssid):(PS8)"<empty>");        
    }
    else
    {
        /* Setting the new SSID, BRCS BSSID is set to clean pre-set BSSID */
        ssid.SsidLength = os_strlen((PS8) parm[0].value);
        os_memcpy((PVOID)ssid.Ssid, (PVOID) parm[0].value, ssid.SsidLength);
        if(ssid.SsidLength < MAX_SSID_LEN)
        {
            ssid.Ssid[ssid.SsidLength] = 0;
        }

        if(OK != CuOs_Set_BSSID(pCuCmd->hCuWext, bssid ) ) return;
        if(OK != CuOs_Set_ESSID(pCuCmd->hCuWext, &ssid) ) return;
    }
}

VOID CuCmd_ModifyConnectMode(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32      i;

    if( nParms == 0 )
    {
        U32 uConnectMode;
        
        if(OK != CuCommon_GetU32 (pCuCmd->hCuCommon, SME_CONNECTION_MODE_PARAM, &uConnectMode)) return;
        CU_CMD_FIND_NAME_ARRAY (i, Current_mode, uConnectMode);
        os_error_printf (CU_MSG_INFO2, (PS8)"Current mode = %s\n", Current_mode[i].name); 
        print_available_values (Current_mode);
    }
    else
    {      
        U32 uConnectMode = parm[0].value;

        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY (i, Current_mode, uConnectMode);
        if(i == SIZE_ARR(Current_mode))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_ModifyConnectMode, Connect Mode %d is not defined!\n", uConnectMode);
            print_available_values (Current_mode);
            return;
        }
        else
        {
            CuCommon_SetU32(pCuCmd->hCuCommon, SME_CONNECTION_MODE_PARAM, uConnectMode);
        }
    }
}

VOID CuCmd_ModifyChannel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if( nParms == 0 )
    {
        U8 desiredChannel = 0;
        U32 currentChannel = 0;
        
        
        if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, SITE_MGR_DESIRED_CHANNEL_PARAM, &desiredChannel)) return;
        if(OK != CuOs_GetCurrentChannel(pCuCmd->hCuWext, &currentChannel)) return;
        
        os_error_printf(CU_MSG_INFO2, (PS8)"Channel=%d (desired: %d)\n",currentChannel,desiredChannel);
    }
    else
    {       
        CuCommon_SetU32(pCuCmd->hCuCommon, SITE_MGR_DESIRED_CHANNEL_PARAM, parm[0].value);
    }
}

VOID CuCmd_GetTxRate(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if( nParms == 0)
    {
        U8 CurrentTxRate;
        S8 CurrentTxRateStr[50];

        if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, TIWLN_802_11_CURRENT_RATES_GET, &CurrentTxRate)) return;

        CuCmd_CreateRateStr(CurrentTxRateStr, CurrentTxRate);
        os_error_printf(CU_MSG_INFO2, (PS8)"Rate: %s\n", CurrentTxRateStr);
    }
}

VOID CuCmd_ModifyBssType(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 BssType = 0;
    S32 i;
    
    if( nParms == 0 )
    {
        if(pCuCmd->hWpaCore == NULL)
        {
            if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, CTRL_DATA_CURRENT_BSS_TYPE_PARAM, &BssType)) return;
        }
        else
        {
#ifndef NO_WPA_SUPPL
            if(OK != WpaCore_GetBssType(pCuCmd->hWpaCore, &BssType)) return;
#endif
        }       

        CU_CMD_FIND_NAME_ARRAY(i, BSS_type, BssType);
        if(i == SIZE_ARR(BSS_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Error getting the current BssType! BssType=0x%x\n",BssType);
            return;
        }
        else
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Current mode = %s\n", BSS_type[i].name);        
        }
        print_available_values(BSS_type);        
    }
    else    
    {
        BssType = parm[0].value;
        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY(i, BSS_type, BssType);
        if(i == SIZE_ARR(BSS_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_ModifyBssType, BssType %d is not defined!\n", BssType);
            return;
        }       
        if(pCuCmd->hWpaCore == NULL)
        {
            CuCommon_SetU32(pCuCmd->hCuCommon, CTRL_DATA_CURRENT_BSS_TYPE_PARAM, BssType);
            CuCommon_SetU32(pCuCmd->hCuCommon, SME_DESIRED_BSS_TYPE_PARAM, BssType);
        }
        else
        {
#ifndef NO_WPA_SUPPL
            WpaCore_SetBssType(pCuCmd->hWpaCore, BssType);
#endif
        }
    }
}

VOID CuCmd_ModifyFragTh(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 FragTh;

    if( nParms == 0 )
    {
        if(OK != CuOs_GetFragTh(pCuCmd->hCuWext, &FragTh)) return;
        os_error_printf(CU_MSG_INFO2, (PS8)"Frag. threshold = %d\n", FragTh);
    }
    else
    {
        FragTh = parm[0].value;
        CuOs_SetFragTh(pCuCmd->hCuWext, FragTh);
    }   
}

VOID CuCmd_ModifyRtsTh(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 RtsTh;

    if( nParms == 0 )
    {
        if(OK != CuOs_GetRtsTh(pCuCmd->hCuWext, &RtsTh)) return;
        os_error_printf(CU_MSG_INFO2, (PS8)"RTS threshold = %d\n", RtsTh);
    }
    else
    {
        RtsTh = parm[0].value;
        CuOs_SetRtsTh(pCuCmd->hCuWext, RtsTh);
    }   
}

VOID CuCmd_ModifyPreamble(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    EPreamble Preamble;
    S32 i;
    named_value_t preamble_type[] = {
        { PREAMBLE_LONG,              (PS8)"PREAMBLE_LONG" },
        { PREAMBLE_SHORT,             (PS8)"PREAMBLE_SHORT" }
    };

    if(nParms)
    {
        Preamble = parm[0].value;
        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY(i, preamble_type, Preamble);
        if(i == SIZE_ARR(preamble_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_ModifyPreamble, Preamble %d is not defined!\n", Preamble);
            return;
        }
        CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_802_11_SHORT_PREAMBLE_SET, Preamble);
    }
    else
    {
        S32 PreambleData;
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_802_11_SHORT_PREAMBLE_GET, (PU32)&PreambleData)) return;
        Preamble = PreambleData;
        os_error_printf(CU_MSG_INFO2, (PS8)"Preamble = %d\n", Preamble);
        print_available_values(preamble_type);
    }
}

VOID CuCmd_ModifyShortSlot(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    slotTime_e SlotTime;
    S32 i;
    named_value_t SlotTime_type[] = {
        { PHY_SLOT_TIME_LONG,              (PS8)"PHY_SLOT_TIME_LONG" },
        { PHY_SLOT_TIME_SHORT,             (PS8)"PHY_SLOT_TIME_SHORT" }
    };

    if(nParms)
    {
        SlotTime = parm[0].value;
        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY(i, SlotTime_type, SlotTime);
        if(i == SIZE_ARR(SlotTime_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_ModifyShortSlot, SlotTime %d is not defined!\n", SlotTime);
            return;
        }
        CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_SHORT_SLOT_SET, SlotTime);
    }
    else
    {
        S32 SlotTimeData;
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_SHORT_SLOT_GET, (PU32)&SlotTimeData)) return;
        SlotTime = SlotTimeData;
        os_error_printf(CU_MSG_INFO2, (PS8)"SlotTime = %d\n", SlotTime);
        print_available_values(SlotTime_type);
    }
}


VOID CuCmd_RadioOnOff(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
	U32      on_off;

	if(nParms) 
    {
		on_off = parm[0].value;
		CuCommon_SetU32(pCuCmd->hCuCommon, SME_RADIO_ON_PARAM, on_off);
	}
	else 
    {
		if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, SME_RADIO_ON_PARAM, &on_off)) {
			os_error_printf(CU_MSG_ERROR, (PS8)"CuCmd_RadioOnOff error, Cannot get radio state!\n");
			return;
	}
		os_error_printf(CU_MSG_ERROR, (PS8)"Radio state = %s\n", on_off ? "ON" : "OFF");
        os_error_printf(CU_MSG_ERROR, (PS8)"Turn radio on/off. 0=OFF, 1=ON\n");
	}
}


VOID CuCmd_GetSelectedBssidInfo(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_SSID ssid;
    TMacAddr bssid;
    
    if(OK != CuOs_Get_SSID(pCuCmd->hCuWext, &ssid)) return;
    if(OK != CuOs_Get_BSSID(pCuCmd->hCuWext, bssid)) return;

    os_error_printf(CU_MSG_INFO2, (PS8)"Selected BSSID Info:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"--------------------\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"SSID : %s\n",(ssid.SsidLength)?(PS8)PrintSSID(&ssid):(PS8)"<empty>");
    os_error_printf(CU_MSG_INFO2, (PS8)"BSSID : %02x.%02x.%02x.%02x.%02x.%02x\n",bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
}

VOID CuCmd_GetRsiiLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S8 dRssi, bRssi;

    if(OK != CuCommon_GetRssi(pCuCmd->hCuCommon, &dRssi, &bRssi)) return;
	/* need the 0xffffff00 to get negative value display */
    os_error_printf(CU_MSG_INFO2, (PS8)"Current dataRSSI=%d  beaconRssi=%d\n", dRssi?dRssi|0xffffff00:dRssi, bRssi?bRssi|0xffffff00:bRssi);
}

VOID CuCmd_GetSnrRatio(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 dSnr, bSnr;
    
    if(OK != CuCommon_GetSnr(pCuCmd->hCuCommon, &dSnr, &bSnr)) return;
    os_error_printf(CU_MSG_INFO2, (PS8)"Current dataSNR=%d   beaconSnr=%d\n", dSnr, bSnr);
}

VOID CuCmd_ShowTxPowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    
    S32 txPowerLevel;
    if(OK != CuOs_GetTxPowerLevel(pCuCmd->hCuWext, &txPowerLevel)) return;
    
    os_error_printf(CU_MSG_INFO2, (PS8)"Tx Power level = %d\n", txPowerLevel);        
}

VOID CuCmd_ShowTxPowerTable(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TpowerLevelTable_t txPowerLevel;
    S32 i, res;

    res = CuCommon_GetBuffer(pCuCmd->hCuCommon, REGULATORY_DOMAIN_TX_POWER_LEVEL_TABLE_PARAM, &txPowerLevel, sizeof(txPowerLevel));

    if( !res )
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Power level table (Dbm/10)\n");        
        for (i = 0; i < NUMBER_OF_SUB_BANDS_E; i++)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"sub-band %i: %d %d %d %d\n", i,
            txPowerLevel.uDbm[i][0],
            txPowerLevel.uDbm[i][1],
            txPowerLevel.uDbm[i][2],
            txPowerLevel.uDbm[i][3]);
        }
    }
    else
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Tx Power level table ERROR !!!\n");
    }
}

VOID CuCmd_TxPowerDbm(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{   
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd; 
    U8 txPowerLevelDbm = (U8)parm[0].value;

    if(nParms == 0)
    {
        if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, TIWLN_802_11_TX_POWER_DBM_GET, (PU8)&txPowerLevelDbm)) return;
    }
    else
    {
        if (parm[0].value > MAX_TX_POWER) 
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Please use values between %d and %d\n", MIN_TX_POWER, MAX_TX_POWER); return;
        }
        else
		{
			if(OK != CuCommon_SetU8(pCuCmd->hCuCommon, TIWLN_802_11_TX_POWER_DBM_GET, (U8)txPowerLevelDbm)) return;
		}
    }

    os_error_printf(CU_MSG_INFO2, (PS8)"Tx Power in DBM = %d\n", txPowerLevelDbm);
}

VOID CuCmd_ModifyState_802_11d(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{   
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 Enabled_802_11d;

    if(nParms == 0)
    {
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_GET_802_11D, &Enabled_802_11d)) return;
        os_error_printf(CU_MSG_INFO2, (PS8)"802_11d enabled = %s\n", (Enabled_802_11d)?"TRUE":"FALSE");     
    }
    else
    {
        Enabled_802_11d = parm[0].value;
        if(OK != CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_ENABLE_DISABLE_802_11D, Enabled_802_11d))
        {
            U32 Enabled_802_11h;
            if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_GET_802_11H, &Enabled_802_11h)) return;
            if(Enabled_802_11h && (!Enabled_802_11d))
                os_error_printf(CU_MSG_INFO2, (PS8)"802_11d cannot be disabled while 802_11h is enabled!!\n" );
                
        }
        else
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"802_11d status is updated to = %s\n", (Enabled_802_11d)?"TRUE":"FALSE" );
        }
    }
}

VOID CuCmd_ModifyState_802_11h(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{   
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 Enabled_802_11h;

    if(nParms == 0)
    {
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_GET_802_11H, &Enabled_802_11h)) return;
        os_error_printf(CU_MSG_INFO2, (PS8)"802_11h enabled = %s\n", (Enabled_802_11h)?"TRUE":"FALSE");     
    }
    else
    {   
        Enabled_802_11h = parm[0].value;
        if(OK != CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_ENABLE_DISABLE_802_11H, Enabled_802_11h)) return;
        if(Enabled_802_11h)
            os_error_printf(CU_MSG_INFO2, (PS8)"802_11h enables automatically 802_11d!!\n" );
                
        os_error_printf(CU_MSG_INFO2, (PS8)"802_11h status is updated to =%d\n", Enabled_802_11h );
    }
}

VOID CuCmd_D_Country_2_4Ie(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{   
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if( nParms == 0 )
    {
        U8   CountryString[DOT11_COUNTRY_STRING_LEN+1];
        if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_GET_COUNTRY_2_4,
            CountryString, DOT11_COUNTRY_STRING_LEN+1)) return;
        CountryString[DOT11_COUNTRY_STRING_LEN] = '\0';
        if (CountryString[0] == '\0')
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"802_11d Country for 2.4 GHz is not found\n");
        }
        else
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"802_11d Country for 2.4 GHz is %s \n", CountryString );
        }
    }
    else
    {
        country_t CountryWorld;

        CountryWorld.elementId = COUNTRY_IE_ID;
        CountryWorld.len = 6;
        os_memcpy( (PVOID)CountryWorld.countryIE.CountryString,(PVOID)"GB ", 3);
        CountryWorld.countryIE.tripletChannels[0].firstChannelNumber = 1;
        CountryWorld.countryIE.tripletChannels[0].maxTxPowerLevel = 23;
        CountryWorld.countryIE.tripletChannels[0].numberOfChannels = 11;        

        if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_SET_COUNTRY_2_4,
            &CountryWorld, sizeof(country_t))) return;                

        os_error_printf(CU_MSG_INFO2, (PS8)"802_11d Start Setting GB Country for 2.4 GHz\n");
    }
}


VOID CuCmd_D_Country_5Ie(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{   
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if( nParms == 0 )
    {
        U8   CountryString[DOT11_COUNTRY_STRING_LEN+1];
        
        if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_GET_COUNTRY_5,
            CountryString, DOT11_COUNTRY_STRING_LEN+1)) return;
        
        CountryString[DOT11_COUNTRY_STRING_LEN] = '\0';
        if (CountryString[0] == '\0')
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"802_11d Country for 5 GHz is not found\n");
        }
        else
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"802_11d Country for 5 GHz is %s\n", CountryString );
        }
    }
    else
    {
        country_t CountryWorld;

        CountryWorld.elementId = COUNTRY_IE_ID;
        CountryWorld.len = 6;
        os_memcpy((PVOID) CountryWorld.countryIE.CountryString,(PVOID)"US ", 3);
        CountryWorld.countryIE.tripletChannels[0].firstChannelNumber = 36;
        CountryWorld.countryIE.tripletChannels[0].maxTxPowerLevel = 13;
        CountryWorld.countryIE.tripletChannels[0].numberOfChannels = 8;        

        if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REG_DOMAIN_SET_COUNTRY_5,
            &CountryWorld, sizeof(country_t))) return;                

        os_error_printf(CU_MSG_INFO2, (PS8)"802_11d Start Setting US Country for 5 GHz\n");
    }
}

VOID CuCmd_ModifyDfsRange(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{   
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U16 minDFS_channelNum;
    U16 maxDFS_channelNum;

    if( nParms == 0 )
    {
        if(OK != CuCommon_GetDfsChannels(pCuCmd->hCuCommon, &minDFS_channelNum, &maxDFS_channelNum)) return;
        
        os_error_printf(CU_MSG_INFO2, (PS8)"DFS min channel is %d, DFS max channel is %d\n", 
                                    minDFS_channelNum, maxDFS_channelNum);
    }
    else
    {
        minDFS_channelNum = (U16) parm[0].value;
        maxDFS_channelNum = (U16) parm[1].value;

        if(OK != CuCommon_SetDfsChannels(pCuCmd->hCuCommon, minDFS_channelNum, maxDFS_channelNum)) return;
        
        os_error_printf(CU_MSG_INFO2, (PS8)"Setting DFS min channel %d, DFS max channel %d\n", 
                                    minDFS_channelNum, maxDFS_channelNum);        
    }
}

VOID CuCmd_SetBeaconFilterDesiredState(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    
    if( nParms == 0 )
    {
        print_available_values(BeaconFilter_use);
    }
    else    
    {
        CuCommon_SetU8(pCuCmd->hCuCommon, TIWLN_802_11_BEACON_FILTER_DESIRED_STATE_SET, (U8)parm[0].value);
    }
}

VOID CuCmd_GetBeaconFilterDesiredState(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U8 beaconFilterDesiredState = 0;

    if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, TIWLN_802_11_BEACON_FILTER_DESIRED_STATE_GET, &beaconFilterDesiredState)) return;
    
    os_error_printf(CU_MSG_INFO2, (PS8)"Desired State is %s\n", (beaconFilterDesiredState == 0)?"FILTER INACTIVE":"FILTER ACTIVE" );
}

VOID CuCmd_ModifySupportedRates(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    rates_t SupportedRates;
    S32 i;

    if( nParms == 0 )
    {
        if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SUPPORTED_RATES,
            &SupportedRates, sizeof(rates_t))) return;   
        
        os_error_printf(CU_MSG_INFO2, (PS8)" Rates: ");
        for( i=0; i < SupportedRates.len; i++ )
        {
            os_error_printf(CU_MSG_INFO2, 
                            (PS8)"%g Mbps(%u%s)%s", 
                            /* patch in order to support NET_RATE_MCS7 values that equal to NET_RATE_1M_BASIC */    
                            (RATE_2_MBPS(SupportedRates.ratesString[i]) == 63.5) ? 65 : RATE_2_MBPS(SupportedRates.ratesString[i]), 
                            /* patch in order to support NET_RATE_MCS7 values that equal to NET_RATE_1M_BASIC */    
                            (SupportedRates.ratesString[i] == 0x7f) ? 0x83 : SupportedRates.ratesString[i],
                IS_BASIC_RATE(SupportedRates.ratesString[i]) ? " - basic" : "",
                (i < SupportedRates.len-1) ? "," : "" );
        }
        os_error_printf(CU_MSG_INFO2, (PS8)"\n");        
    }
    else
    {
        PS8 buf = (PS8) parm[0].value;
        PS8 end_p;
        U32 val;
        U32 MaxVal = ((1 << (sizeof(SupportedRates.ratesString[i]) * 8))-1);

        os_error_printf(CU_MSG_INFO2, (PS8)"param: %s\n", buf );
        
        for( i=0; *buf && i < DOT11_MAX_SUPPORTED_RATES; i++ )
        {
            val = os_strtoul(buf, &end_p, 0);
            if(val == 0)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_ModifySupportedRates: invalid value - %s\n", buf );
                return;
            }
            if(val > MaxVal)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_ModifySupportedRates: out of range %d\n", val );
                return;
            }
            /* patch in order to support NET_RATE_MCS7 values that equal to NET_RATE_1M_BASIC */    
            if (val == 0x83)
            {
                val = 0x7f;
            }
            SupportedRates.ratesString[i] = (U8)(val);
            buf = end_p;
            while( *buf==' ' || *buf == ',' )   buf++;
        }
        if(*buf)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"too many parameters. Max=%d\n", DOT11_MAX_SUPPORTED_RATES );
            return;
        }

        SupportedRates.len = (U8) i;
        CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SUPPORTED_RATES,
            &SupportedRates, sizeof(rates_t));
    }
}

VOID CuCmd_SendHealthCheck(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (OK != CuCommon_SetU32(pCuCmd->hCuCommon, HEALTH_MONITOR_CHECK_DEVICE, TRUE)) return;
    
    os_error_printf(CU_MSG_INFO2, (PS8)"Send health check...\n");
}

VOID CuCmd_EnableRxDataFilters(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (OK != CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_ENABLE_DISABLE_RX_DATA_FILTERS, TRUE)) return;
    
    os_error_printf(CU_MSG_INFO2, (PS8)"Enabling Rx data filtering...\n");
}

VOID CuCmd_DisableRxDataFilters(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (OK != CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_ENABLE_DISABLE_RX_DATA_FILTERS, FALSE)) return;
    
    os_error_printf(CU_MSG_INFO2, (PS8)"Disabling Rx data filtering...\n");
}

VOID CuCmd_AddRxDataFilter(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 res;
    TRxDataFilterRequest request;
    PS8 mask = (PS8) parm[1].value;
    PS8 pattern = (PS8) parm[2].value;

    request.offset = (U8)parm[0].value;
    CuCmd_ParseMaskString(mask, request.mask, &request.maskLength);
    CuCmd_ParsePatternString(pattern, request.pattern, &request.patternLength);

    res = CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_ADD_RX_DATA_FILTER,
        &request, sizeof(TRxDataFilterRequest));

    if(res == OK)
        os_error_printf(CU_MSG_INFO2, (PS8)"Filter added.\n");
    else
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_AddRxDataFilter - Couldn't add Rx data filter...\n");    
        
}

VOID CuCmd_RemoveRxDataFilter(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 res;
    TRxDataFilterRequest request;
    PS8 mask = (PS8) parm[1].value;
    PS8 pattern = (PS8) parm[2].value;

    request.offset = (U8)parm[0].value;
    CuCmd_ParseMaskString(mask, request.mask, &request.maskLength);
    CuCmd_ParsePatternString(pattern, request.pattern, &request.patternLength);

    res = CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REMOVE_RX_DATA_FILTER,
        &request, sizeof(TRxDataFilterRequest));

    if(res == OK)
        os_error_printf(CU_MSG_INFO2, (PS8)"Filter Removed.\n");
    else
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_AddRxDataFilter - Couldn't remove Rx data filter...\n");    
}

VOID CuCmd_GetRxDataFiltersStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 UnmatchedPacketsCount;
    U32 MatchedPacketsCount[4];
    
    if (OK != CuCommon_GetRxDataFiltersStatistics(pCuCmd->hCuCommon, &UnmatchedPacketsCount, MatchedPacketsCount)) return;
    
    os_error_printf(CU_MSG_INFO2, (PS8)"Rx data filtering statistics:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"Unmatched packets: %u\n", UnmatchedPacketsCount);
    os_error_printf(CU_MSG_INFO2, (PS8)"Packets matching filter #1: %u\n", MatchedPacketsCount[0]);
    os_error_printf(CU_MSG_INFO2, (PS8)"Packets matching filter #2: %u\n", MatchedPacketsCount[1]);
    os_error_printf(CU_MSG_INFO2, (PS8)"Packets matching filter #3: %u\n", MatchedPacketsCount[2]);
    os_error_printf(CU_MSG_INFO2, (PS8)"Packets matching filter #4: %u\n", MatchedPacketsCount[3]);
}

VOID CuCmd_ShowStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    U32 powerMode;
    TMacAddr Mac;
    OS_802_11_SSID ssid;
    U8 desiredChannel;
    S32 rtsTh;
    S32 fragTh;
    S32 txPowerLevel;
    U8 bssType;
    U32 desiredPreambleType;
    TIWLN_COUNTERS driverCounters;
    U32 AuthMode;
    U8  CurrentTxRate;
    S8  CurrentTxRateStr[20];
    U8  CurrentRxRate;
    S8  CurrentRxRateStr[20];
    U32 DefaultKeyId;
    U32 WepStatus;
    S8 dRssi, bRssi;
#ifdef XCC_MODULE_INCLUDED
    U32 XCCNetEap;
#endif

    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, CTRL_DATA_MAC_ADDRESS,
            Mac, sizeof(TMacAddr))) return;     
    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_POWER_MODE_GET, &powerMode, sizeof(U32))) return;
    if(OK != CuOs_Get_SSID(pCuCmd->hCuWext, &ssid)) return;
    if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, SITE_MGR_DESIRED_CHANNEL_PARAM, &desiredChannel)) return;
    if(OK != CuOs_GetRtsTh(pCuCmd->hCuWext, &rtsTh)) return;
    if(OK != CuOs_GetFragTh(pCuCmd->hCuWext, &fragTh)) return;
    if(OK != CuOs_GetTxPowerLevel(pCuCmd->hCuWext, &txPowerLevel)) return;
    if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, CTRL_DATA_CURRENT_BSS_TYPE_PARAM, &bssType)) return;
    if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_802_11_SHORT_PREAMBLE_GET, &desiredPreambleType)) return;
    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, SITE_MGR_TI_WLAN_COUNTERS_PARAM, &driverCounters, sizeof(TIWLN_COUNTERS))) return;
    if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_ENCRYPTION_STATUS_PARAM, &WepStatus)) return;
    if(OK != CuCommon_GetRssi(pCuCmd->hCuCommon, &dRssi, &bRssi)) return;
    if (pCuCmd->hWpaCore == NULL)
    {
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_EXT_AUTHENTICATION_MODE, &AuthMode)) return;
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_DEFAULT_KEY_ID, &DefaultKeyId)) return;
    }
    else
    {
#ifndef NO_WPA_SUPPL
        if(OK != WpaCore_GetAuthMode(pCuCmd->hWpaCore, &AuthMode)) return;
        if(OK != WpaCore_GetDefaultKey(pCuCmd->hWpaCore, &DefaultKeyId)) return;    
#endif
    }

    if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, TIWLN_802_11_CURRENT_RATES_GET, &CurrentTxRate)) return;
    CuCmd_CreateRateStr(CurrentTxRateStr, CurrentTxRate);       

    if(OK != CuCommon_GetU8(pCuCmd->hCuCommon, TIWLN_GET_RX_DATA_RATE, &CurrentRxRate)) return;
    CuCmd_CreateRateStr(CurrentRxRateStr, CurrentRxRate);
    
#ifdef XCC_MODULE_INCLUDED
    if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_XCC_NETWORK_EAP, &XCCNetEap)) return;
#endif

    os_error_printf(CU_MSG_INFO2, (PS8)"******************\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"Driver Statistics:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"******************\n");

    os_error_printf(CU_MSG_INFO2, (PS8)"    dot11CurrentTxRate : %s\n", CurrentTxRateStr);
	os_error_printf(CU_MSG_INFO2, (PS8)"         CurrentRxRate : %s\n", CurrentRxRateStr);
    os_error_printf(CU_MSG_INFO2, (PS8)"   dot11DesiredChannel : %d\n", desiredChannel);
    os_error_printf(CU_MSG_INFO2, (PS8)"     currentMACAddress : %02x.%02x.%02x.%02x.%02x.%02x\n",Mac[0],Mac[1],Mac[2],Mac[3],Mac[4],Mac[5]);
    os_error_printf(CU_MSG_INFO2, (PS8)"      dot11DesiredSSID : %s\n", ssid.Ssid);
    os_error_printf(CU_MSG_INFO2, (PS8)"          dot11BSSType : %d\n", bssType);
    os_error_printf(CU_MSG_INFO2, (PS8)"    AuthenticationMode : %d\n", AuthMode );
    os_error_printf(CU_MSG_INFO2, (PS8)"    bShortPreambleUsed : %d\n", desiredPreambleType );
    os_error_printf(CU_MSG_INFO2, (PS8)"          RTSThreshold : %d\n", rtsTh );
    os_error_printf(CU_MSG_INFO2, (PS8)"FragmentationThreshold : %d\n", fragTh );
    os_error_printf(CU_MSG_INFO2, (PS8)" bDefaultWEPKeyDefined : %d\n", DefaultKeyId);
    os_error_printf(CU_MSG_INFO2, (PS8)"             WEPStatus : %d\n", WepStatus);
    os_error_printf(CU_MSG_INFO2, (PS8)"          TxPowerLevel : %d\n", txPowerLevel );
    os_error_printf(CU_MSG_INFO2, (PS8)"             PowerMode : %d\n", powerMode );
    os_error_printf(CU_MSG_INFO2, (PS8)"              dataRssi : %d\n", dRssi);
	os_error_printf(CU_MSG_INFO2, (PS8)"            beaconRssi : %d\n", bRssi);
    /**/
    /* network layer statistics*/
    /**/
    os_error_printf(CU_MSG_INFO2, (PS8)"                RecvOk : %d\n", driverCounters.RecvOk );
    os_error_printf(CU_MSG_INFO2, (PS8)"             RecvError : %d\n", driverCounters.RecvError );
    os_error_printf(CU_MSG_INFO2, (PS8)"     DirectedBytesRecv : %d\n", driverCounters.DirectedBytesRecv );
    os_error_printf(CU_MSG_INFO2, (PS8)"    DirectedFramesRecv : %d\n", driverCounters.DirectedFramesRecv );
    os_error_printf(CU_MSG_INFO2, (PS8)"    MulticastBytesRecv : %d\n", driverCounters.MulticastBytesRecv );
    os_error_printf(CU_MSG_INFO2, (PS8)"   MulticastFramesRecv : %d\n", driverCounters.MulticastFramesRecv );
    os_error_printf(CU_MSG_INFO2, (PS8)"    BroadcastBytesRecv : %d\n", driverCounters.BroadcastBytesRecv );
    os_error_printf(CU_MSG_INFO2, (PS8)"   BroadcastFramesRecv : %d\n", driverCounters.BroadcastFramesRecv );
    os_error_printf(CU_MSG_INFO2, (PS8)"             FcsErrors : %d\n", driverCounters.FcsErrors );
    os_error_printf(CU_MSG_INFO2, (PS8)"           BeaconsRecv : %d\n", driverCounters.BeaconsRecv );
    os_error_printf(CU_MSG_INFO2, (PS8)"          AssocRejects : %d\n", driverCounters.AssocRejects );
    os_error_printf(CU_MSG_INFO2, (PS8)"         AssocTimeouts : %d\n", driverCounters.AssocTimeouts );
    os_error_printf(CU_MSG_INFO2, (PS8)"           AuthRejects : %d\n", driverCounters.AuthRejects );
    os_error_printf(CU_MSG_INFO2, (PS8)"          AuthTimeouts : %d\n", driverCounters.AuthTimeouts );

    /**/
    /* other statistics*/
    /**/
#ifdef XCC_MODULE_INCLUDED
    os_error_printf(CU_MSG_INFO2, (PS8)"        dwSecuritySuit : %d\n", XCCNetEap);
#endif
}

VOID CuCmd_ShowTxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TIWLN_TX_STATISTICS txCounters;
    U32 TxQid;
    U32 AverageDelay;
    U32 AverageFWDelay;
    U32 AverageMacDelay;

    if( nParms == 0 )
    {
        if(OK != CuCommon_GetTxStatistics(pCuCmd->hCuCommon, &txCounters, 0)) return;
    }
    else    
    {
        if(OK != CuCommon_GetTxStatistics(pCuCmd->hCuCommon, &txCounters, parm[0].value)) return;
    }

    os_error_printf(CU_MSG_INFO2, (PS8)"*********************\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"Tx Queues Statistics:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"*********************\n");

    for (TxQid = 0; TxQid < MAX_NUM_OF_AC; TxQid++)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"\nTx Queue %d:\n", TxQid);
        os_error_printf(CU_MSG_INFO2, (PS8)"===========\n");
        
        os_error_printf(CU_MSG_INFO2, (PS8)"  Total Good Frames             : %d\n", txCounters.txCounters[TxQid].XmitOk );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Unicast Bytes                 : %d\n", txCounters.txCounters[TxQid].DirectedBytesXmit );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Unicast Frames                : %d\n", txCounters.txCounters[TxQid].DirectedFramesXmit );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Multicast Bytes               : %d\n", txCounters.txCounters[TxQid].MulticastBytesXmit );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Multicast Frames              : %d\n", txCounters.txCounters[TxQid].MulticastFramesXmit );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Broadcast Bytes               : %d\n", txCounters.txCounters[TxQid].BroadcastBytesXmit );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Broadcast Frames              : %d\n", txCounters.txCounters[TxQid].BroadcastFramesXmit );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Retry Failures                : %d\n", txCounters.txCounters[TxQid].RetryFailCounter );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Tx Timeout Failures           : %d\n", txCounters.txCounters[TxQid].TxTimeoutCounter );
        os_error_printf(CU_MSG_INFO2, (PS8)"  No Link Failures              : %d\n", txCounters.txCounters[TxQid].NoLinkCounter );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Other Failures                : %d\n", txCounters.txCounters[TxQid].OtherFailCounter );
        os_error_printf(CU_MSG_INFO2, (PS8)"  Max Consecutive Retry Failures : %d\n\n", txCounters.txCounters[TxQid].MaxConsecutiveRetryFail );
        
        os_error_printf(CU_MSG_INFO2, (PS8)"  Retry histogram:\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"  ----------------\n\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"  Retries: %8d %8d %8d %8d %8d %8d %8d %8d\n", 0, 1, 2, 3, 4, 5, 6, 7);
        os_error_printf(CU_MSG_INFO2, (PS8)"  packets: %8d %8d %8d %8d %8d %8d %8d %8d\n\n", 
                                txCounters.txCounters[TxQid].RetryHistogram[ 0 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 1 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 2 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 3 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 4 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 5 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 6 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 7 ]);
        os_error_printf(CU_MSG_INFO2, (PS8)"  Retries: %8d %8d %8d %8d %8d %8d %8d %8d\n", 8, 9, 10, 11, 12, 13, 14, 15);
        os_error_printf(CU_MSG_INFO2, (PS8)"  packets: %8d %8d %8d %8d %8d %8d %8d %8d\n\n", 
                                txCounters.txCounters[TxQid].RetryHistogram[ 8 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 9 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 10 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 11 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 12 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 13 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 14 ],
                                txCounters.txCounters[TxQid].RetryHistogram[ 15 ]);

        if (txCounters.txCounters[TxQid].NumPackets)
        {
            AverageDelay = txCounters.txCounters[TxQid].SumTotalDelayMs / txCounters.txCounters[TxQid].NumPackets;
            AverageFWDelay = txCounters.txCounters[TxQid].SumFWDelayUs / txCounters.txCounters[TxQid].NumPackets;
            AverageMacDelay = txCounters.txCounters[TxQid].SumMacDelayUs / txCounters.txCounters[TxQid].NumPackets;
        }
        else
        {
            AverageDelay = 0;
            AverageFWDelay = 0;
            AverageMacDelay = 0;
        }

        os_error_printf(CU_MSG_INFO2, (PS8)"  Total Delay ms (average/sum) : %d / %d\n", AverageDelay, txCounters.txCounters[TxQid].SumTotalDelayMs);
        os_error_printf(CU_MSG_INFO2, (PS8)"  FW Delay us (average/sum) : %d / %d\n", AverageFWDelay, txCounters.txCounters[TxQid].SumFWDelayUs);
        os_error_printf(CU_MSG_INFO2, (PS8)"  MAC Delay us (average/sum)   : %d / %d\n\n", AverageMacDelay, txCounters.txCounters[TxQid].SumMacDelayUs);

        os_error_printf(CU_MSG_INFO2, (PS8)"  Delay Ranges [msec]  : Num of packets\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"  -------------------  : --------------\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"        0   -    1     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_0_TO_1] );
        os_error_printf(CU_MSG_INFO2, (PS8)"        1   -   10     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_1_TO_10] );
        os_error_printf(CU_MSG_INFO2, (PS8)"       10   -   20     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_10_TO_20] );
        os_error_printf(CU_MSG_INFO2, (PS8)"       20   -   40     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_20_TO_40] );
        os_error_printf(CU_MSG_INFO2, (PS8)"       40   -   60     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_40_TO_60] );
        os_error_printf(CU_MSG_INFO2, (PS8)"       60   -   80     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_60_TO_80] );
        os_error_printf(CU_MSG_INFO2, (PS8)"       80   -  100     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_80_TO_100] );
        os_error_printf(CU_MSG_INFO2, (PS8)"      100   -  200     : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_100_TO_200] );
        os_error_printf(CU_MSG_INFO2, (PS8)"        Above 200      : %d\n", txCounters.txCounters[TxQid].txDelayHistogram[TX_DELAY_RANGE_ABOVE_200] );
    }       
}

VOID CuCmd_ShowAdvancedParams(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    U32 AuthMode;
    TPowerMgr_PowerMode Mode;
    S32 txPowerLevel;
#ifndef NO_WPA_SUPPL
    OS_802_11_ENCRYPTION_TYPES EncryptionTypePairwise;
    OS_802_11_ENCRYPTION_TYPES EncryptionTypeGroup;
#endif
    S32 Preamble;       
    S32 FragTh;
    S32 RtsTh;

    if (pCuCmd->hWpaCore == NULL)
    {
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_EXT_AUTHENTICATION_MODE, &AuthMode)) return;        
    }
    else
    {
#ifndef NO_WPA_SUPPL
        if(OK != WpaCore_GetAuthMode(pCuCmd->hWpaCore, &AuthMode)) return;      
#endif
    }
    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_POWER_MODE_GET, &Mode, sizeof(TPowerMgr_PowerMode))) return;
    if(OK != CuOs_GetTxPowerLevel(pCuCmd->hCuWext, &txPowerLevel)) return;    
#ifndef NO_WPA_SUPPL
    if(OK != WpaCore_GetEncryptionPairWise(pCuCmd->hWpaCore, &EncryptionTypePairwise)) return;  
    if(OK != WpaCore_GetEncryptionGroup(pCuCmd->hWpaCore, &EncryptionTypeGroup)) return;    
#endif
    if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_802_11_SHORT_PREAMBLE_GET, (PU32)&Preamble)) return;
    if(OK != CuOs_GetFragTh(pCuCmd->hCuWext, &FragTh)) return;
    if(OK != CuOs_GetRtsTh(pCuCmd->hCuWext, &RtsTh)) return;
            
    
    os_error_printf(CU_MSG_INFO2, (PS8)"********************\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"Advanced Statistics:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"********************\n");
    
    os_error_printf(CU_MSG_INFO2, (PS8)"  Authentication : %u\n", AuthMode );
    os_error_printf(CU_MSG_INFO2, (PS8)"  Power mode : %d\n", Mode.PowerMode );
    os_error_printf(CU_MSG_INFO2, (PS8)"  Tx Power level : %d\n", txPowerLevel );
#ifndef NO_WPA_SUPPL
    os_error_printf(CU_MSG_INFO2, (PS8)"  Encryption Pairwise: %u\n", EncryptionTypePairwise );
    os_error_printf(CU_MSG_INFO2, (PS8)"  Encryption Group: %u\n", EncryptionTypeGroup );
#endif
    os_error_printf(CU_MSG_INFO2, (PS8)"  Preamble : <%s>\n", (Preamble) ? "short" : "long");
    os_error_printf(CU_MSG_INFO2, (PS8)"  Frag. threshold : %u\n", FragTh);
    os_error_printf(CU_MSG_INFO2, (PS8)"  RTS threshold : %u\n", RtsTh );
    os_error_printf(CU_MSG_INFO2, (PS8)"  Power mode: ");
    print_available_values(power_mode_val);
    os_error_printf(CU_MSG_INFO2, (PS8)"  Encryption type: ");
    print_available_values(encrypt_type);
    
}


VOID Cucmd_ShowPowerConsumptionStats(THandle hCuCmd,ConParm_t parm[],U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    ACXPowerConsumptionTimeStat_t tStatistics;
   
    os_memset( &tStatistics, 0, sizeof(ACXPowerConsumptionTimeStat_t) );
    
     if (OK != CuCommon_GetPowerConsumptionStat(pCuCmd->hCuCommon,&tStatistics))
     {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - Failed to read power consumption statistic!\n");
        return;
     }
         

    
    os_error_printf(CU_MSG_INFO2, (PS8)"\nPower Consumption Statistics:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"-----------------------------\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"activeTimeCnt:0x%x%x\n", tStatistics.awakeTimeCnt_Hi,tStatistics.awakeTimeCnt_Low );
    os_error_printf(CU_MSG_INFO2, (PS8)"elpTimeCnt: 0x%x%x\n", tStatistics.elpTimeCnt_Hi,tStatistics.elpTimeCnt_Low);
    os_error_printf(CU_MSG_INFO2, (PS8)"powerDownTimeCnt: 0x%x%x\n", tStatistics.powerDownTimeCnt_Hi,tStatistics.powerDownTimeCnt_Low);
    os_error_printf(CU_MSG_INFO2, (PS8)"ListenMode11BTimeCnt: 0x%x%x\n", tStatistics.ListenMode11BTimeCnt_Hi,tStatistics.ListenMode11BTimeCnt_Low);
    os_error_printf(CU_MSG_INFO2, (PS8)"ListenModeOFDMTimeCnt: 0x%x%x\n", tStatistics.ListenModeOFDMTimeCnt_Hi,tStatistics.ListenModeOFDMTimeCnt_Low);
    
}



VOID CuCmd_ScanAppGlobalConfig(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if ( 0 == os_strcmp( (PS8)"<empty>", (PS8)parm[0].value) )
    {
        pCuCmd->appScanParams.desiredSsid.len = 0;
        pCuCmd->appScanParams.desiredSsid.str[ 0 ] = '\0';
    }
    else
    {    
        pCuCmd->appScanParams.desiredSsid.len = (U8) os_strlen((PS8)parm[0].value);
        os_memcpy( (PVOID)&(pCuCmd->appScanParams.desiredSsid.str), (PVOID)parm[0].value, pCuCmd->appScanParams.desiredSsid.len );
        if(pCuCmd->appScanParams.desiredSsid.len < MAX_SSID_LEN)
        {
            pCuCmd->appScanParams.desiredSsid.str[pCuCmd->appScanParams.desiredSsid.len] = 0;
        }
    }
    pCuCmd->appScanParams.scanType = parm[1].value;
    pCuCmd->appScanParams.band = parm[2].value;
    pCuCmd->appScanParams.probeReqNumber = (U8)parm[3].value;
    pCuCmd->appScanParams.probeRequestRate = parm[4].value;
#ifdef TI_DBG
    pCuCmd->appScanParams.Tid = (U8)parm[5].value;
    pCuCmd->appScanParams.numOfChannels  = (U8)parm[6].value;
#else
    pCuCmd->appScanParams.Tid = 0;
    pCuCmd->appScanParams.numOfChannels = (U8)parm[5].value;
#endif
}

VOID CuCmd_ScanAppChannelConfig(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    
    scan_normalChannelEntry_t* pChannelEntry = 
        &(pCuCmd->appScanParams.channelEntry[ parm[0].value ].normalChannelEntry);
       
    if (parm[2].value < parm[3].value)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Max Dwell Time must be larger than or equal to Min Dwell Time...\n");
        return;
    }

    CuCmd_Str2MACAddr ((PS8)parm[1].value, pChannelEntry->bssId);
    pChannelEntry->maxChannelDwellTime = parm[2].value;
    pChannelEntry->minChannelDwellTime = parm[3].value;
    pChannelEntry->earlyTerminationEvent = parm[4].value;
    pChannelEntry->ETMaxNumOfAPframes = (U8)parm[5].value;
    pChannelEntry->txPowerDbm = (U8)parm[6].value;
    pChannelEntry->channel = (U8)parm[7].value;
}

VOID CuCmd_ScanAppClear(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    os_memset( &pCuCmd->appScanParams, 0, sizeof(scan_Params_t) );
    os_error_printf(CU_MSG_INFO2, (PS8)"Application scan parameters cleared.\n");
}

VOID CuCmd_ScanAppDisplay(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 i,j;
    scan_normalChannelEntry_t* pNormalChannel;

    CU_CMD_FIND_NAME_ARRAY(j, rate2Str, pCuCmd->appScanParams.probeRequestRate);    
    os_error_printf(CU_MSG_INFO2, (PS8)"Application Scan params:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"SSID: %s, Type: %s\n", 
        pCuCmd->appScanParams.desiredSsid.str, 
        scanType2Str[ pCuCmd->appScanParams.scanType ].name);
    os_error_printf(CU_MSG_INFO2, (PS8)"Band: %s, Number of probe req:%d, probe req. rate:%s\n", 
        band2Str[ pCuCmd->appScanParams.band ].name, 
        pCuCmd->appScanParams.probeReqNumber, 
        rate2Str[j].name);
#ifdef TI_DBG
    os_error_printf(CU_MSG_INFO2, (PS8)"Tid :%d\n\n", pCuCmd->appScanParams.Tid);
#else
    os_error_printf(CU_MSG_INFO2, (PS8)"\n");
#endif
    os_error_printf(CU_MSG_INFO2, (PS8)"Channel  BSS ID             Max time  Min time  ET event     ET frame num Power\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"-------------------------------------------------------------------------------\n");
    for ( i = 0; i < pCuCmd->appScanParams.numOfChannels; i++ )
    {
        pNormalChannel = &(pCuCmd->appScanParams.channelEntry[ i ].normalChannelEntry);
        CU_CMD_FIND_NAME_ARRAY(j, EtEvent2Str, pNormalChannel->earlyTerminationEvent);
        os_error_printf(CU_MSG_INFO2, (PS8)"%2d       %02x.%02x.%02x.%02x.%02x.%02x  %7d   %7d   %s%3d          %1d\n",
               pNormalChannel->channel, 
               pNormalChannel->bssId[0],pNormalChannel->bssId[1],pNormalChannel->bssId[2],pNormalChannel->bssId[3],pNormalChannel->bssId[4],pNormalChannel->bssId[5],
               pNormalChannel->maxChannelDwellTime,
               pNormalChannel->minChannelDwellTime, 
               EtEvent2Str[j].name,
               pNormalChannel->ETMaxNumOfAPframes, 
               pNormalChannel->txPowerDbm);
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"\n");
}

VOID CuCmd_ScanSetSra(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (OK != CuCommon_SetU32(pCuCmd->hCuCommon, SCAN_CNCN_SET_SRA, parm[0].value) )
    {
        os_error_printf(CU_MSG_INFO2, (PS8) "Failed setting Scan Result Aging");
    }
    os_error_printf(CU_MSG_INFO2, (PS8) "Scan Result Aging set succesfully to %d seconds", parm[0].value);
}

VOID CuCmd_ScanSetRssi(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (OK != CuCommon_SetU32(pCuCmd->hCuCommon, SCAN_CNCN_SET_RSSI, parm[0].value) )
    {
        os_error_printf(CU_MSG_INFO2, (PS8) "Failed setting Rssi filter threshold");
    }
    os_error_printf(CU_MSG_INFO2, (PS8) "Rssi filter set succesfully to %d", parm[0].value);
}

VOID CuCmd_StartScan(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_START_APP_SCAN_SET, 
        &pCuCmd->appScanParams, sizeof(scan_Params_t))) 
    {
        return;
    }

    os_error_printf(CU_MSG_INFO2, (PS8)"Application scan started\n");

    /*
	 * In order to have ability to set the application scan we are using application scan priver command 
	 * exsample for using supplicant scan command below:
	 * #ifndef NO_WPA_SUPPL
     *       CuOs_Start_Scan(pCuCmd->hCuWext, &ssid);
	 * #endif
	 */
}

VOID CuCmd_WextStartScan(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_SSID ssid;
	U8 scanType =0;

    switch (nParms) 
    {
		case 0:
			ssid.SsidLength = 0;
            ssid.Ssid[0] = 0;
			scanType = 0;
			break;
        case 1 :
            /* 
            *  No SSID & No BSSID are set -
            *  Use Any SSID & Any BSSID.
            */
            ssid.SsidLength = 0;            
            ssid.Ssid[0] = 0;
			scanType = (U8)parm[0].value;
            break;

        case 2:
            /* 
            *  SSID set 
            *  Use CLI's SSID & Any BSSID.
            */
            ssid.SsidLength = os_strlen( (PS8)parm[1].value);
			os_memcpy((PVOID)ssid.Ssid, (PVOID) parm[1].value, ssid.SsidLength);
            ssid.Ssid[ssid.SsidLength] = '\0';
			scanType = (U8)parm[0].value; /* 0 - Active , 1 - Passive*/
            break;

        default:
            os_error_printf(CU_MSG_ERROR, (PS8)"<Scan Type [0=Active, 1=Passive]> <ssid Name As optional>\n");
            return;
    }



#ifndef NO_WPA_SUPPL
    CuOs_Start_Scan(pCuCmd->hCuWext, &ssid, scanType);
#else
    os_error_printf(CU_MSG_INFO2, (PS8)"WEXT not build, Scan Not Started\n");
#endif

}

VOID CuCmd_StopScan (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_STOP_APP_SCAN_SET, NULL, 0))
    {
        return;
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"Application scan stopped\n");
}

VOID CuCmd_ConfigPeriodicScanGlobal (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    pCuCmd->tPeriodicAppScanParams.iRssiThreshold = (S8)parm[ 0 ].value;
    pCuCmd->tPeriodicAppScanParams.iSnrThreshold = (S8)parm[ 1 ].value;
    pCuCmd->tPeriodicAppScanParams.uFrameCountReportThreshold = parm[ 2 ].value;
    pCuCmd->tPeriodicAppScanParams.bTerminateOnReport = parm[ 3 ].value;
    pCuCmd->tPeriodicAppScanParams.eBssType = (ScanBssType_e )parm[ 4 ].value;
    pCuCmd->tPeriodicAppScanParams.uProbeRequestNum = parm[ 5 ].value;
    pCuCmd->tPeriodicAppScanParams.uCycleNum = parm[ 6 ].value;
    pCuCmd->tPeriodicAppScanParams.uSsidNum = parm[ 7 ].value;
    pCuCmd->tPeriodicAppScanParams.uSsidListFilterEnabled = (U8)(parm[ 8 ].value);
    pCuCmd->tPeriodicAppScanParams.uChannelNum = parm[ 9 ].value;
}

VOID CuCmd_ConfigPeriodicScanInterval (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    pCuCmd->tPeriodicAppScanParams.uCycleIntervalMsec[ parm[ 0 ].value ] = parm[ 1 ].value;
}

VOID CuCmd_ConfigurePeriodicScanSsid (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TSsid *pSsid = &pCuCmd->tPeriodicAppScanParams.tDesiredSsid[ parm[ 0 ].value ].tSsid;
     
    pCuCmd->tPeriodicAppScanParams.tDesiredSsid[ parm[ 0 ].value ].eVisability = parm[ 1 ].value;
    pSsid->len = (U8)os_strlen ((PS8)parm[ 2 ].value);
    os_memcpy ((PVOID)&(pSsid->str), 
               (PVOID)parm[ 2 ].value,
               pSsid->len);
    if(pSsid->len < MAX_SSID_LEN)
    {
        pSsid->str[pSsid->len] = 0;
    }
}

VOID CuCmd_ConfigurePeriodicScanChannel (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t                 *pCuCmd = (CuCmd_t*)hCuCmd;
    TPeriodicChannelEntry   *pChannelEnrty = &(pCuCmd->tPeriodicAppScanParams.tChannels[ parm[ 0 ].value ]);

    pChannelEnrty->eBand = parm[ 1 ].value;
    pChannelEnrty->uChannel = parm[ 2 ].value;
    pChannelEnrty->eScanType = parm[ 3 ].value;
    pChannelEnrty->uMinDwellTimeMs = parm[ 4 ].value;;
    pChannelEnrty->uMaxDwellTimeMs = parm[ 5 ].value;
    pChannelEnrty->uTxPowerLevelDbm = parm[ 6 ].value;
}

VOID CuCmd_ClearPeriodicScanConfiguration (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    os_memset (&(pCuCmd->tPeriodicAppScanParams), 0, sizeof (TPeriodicScanParams));
    os_error_printf(CU_MSG_INFO2, (PS8)"Periodic application scan parameters cleared.\n");
}

VOID CuCmd_DisplayPeriodicScanConfiguration (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t*    pCuCmd = (CuCmd_t*)hCuCmd;
    S32         i, j, k;

    os_error_printf(CU_MSG_INFO2, (PS8)"Application Periodic Scan parameters:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"RSSI Threshold: %d, SNR Threshold: %d, Report Threshold: %d  Number of cycles: %d\n",
                    pCuCmd->tPeriodicAppScanParams.iRssiThreshold, pCuCmd->tPeriodicAppScanParams.iSnrThreshold,
                    pCuCmd->tPeriodicAppScanParams.uFrameCountReportThreshold, pCuCmd->tPeriodicAppScanParams.uCycleNum);
    CU_CMD_FIND_NAME_ARRAY (i, booleanStr, pCuCmd->tPeriodicAppScanParams.bTerminateOnReport);
    CU_CMD_FIND_NAME_ARRAY (j, bssTypeStr, pCuCmd->tPeriodicAppScanParams.eBssType);
    os_error_printf(CU_MSG_INFO2, (PS8)"Terminate on Report: %s, BSS type: %s, Probe Request Number: %d\n",
                    booleanStr[ i ].name, bssTypeStr[ j ].name, pCuCmd->tPeriodicAppScanParams.uProbeRequestNum);

    os_error_printf(CU_MSG_INFO2, (PS8)"\nIntervals (msec):\n");
    for (i = 0; i < PERIODIC_SCAN_MAX_INTERVAL_NUM; i++)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"%d ", pCuCmd->tPeriodicAppScanParams.uCycleIntervalMsec[ i ]);
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"\n\nSSIDs:\n");
    for (i = 0; i < (S32)pCuCmd->tPeriodicAppScanParams.uSsidNum; i++)
    {
        CU_CMD_FIND_NAME_ARRAY (j, ssidVisabilityStr, pCuCmd->tPeriodicAppScanParams.tDesiredSsid[ i ].eVisability);
        os_error_printf(CU_MSG_INFO2, (PS8)"%s (%s), ", pCuCmd->tPeriodicAppScanParams.tDesiredSsid[ i ].tSsid.str,
                        ssidVisabilityStr[ j ].name);
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"\n\nSSID List Filter Enabled: %d\n", pCuCmd->tPeriodicAppScanParams.uSsidListFilterEnabled );

    os_error_printf(CU_MSG_INFO2, (PS8)"\n\nChannels:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"%-15s %-10s %-20s %-15s %-15s %-20s\n",
                    (PS8)"Band", (PS8)"Channel", (PS8)"Scan type", (PS8)"Min dwell time", (PS8)"Max dwell time", (PS8)"Power level (dBm*10)");
    os_error_printf(CU_MSG_INFO2, (PS8)"----------------------------------------------------------------------------------------------------\n");
    for (i = 0; i < (S32)pCuCmd->tPeriodicAppScanParams.uChannelNum; i++)
    {
        CU_CMD_FIND_NAME_ARRAY (j, band2Str, pCuCmd->tPeriodicAppScanParams.tChannels[ i ].eBand);
        CU_CMD_FIND_NAME_ARRAY (k, scanType2Str, pCuCmd->tPeriodicAppScanParams.tChannels[ i ].eScanType);
        os_error_printf(CU_MSG_INFO2, (PS8)"%-15s %-10d %-20s %-15d %-15d %-20d\n",
                        band2Str[ j ].name,
                        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uChannel,
                        scanType2Str[ k ].name,
                        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uMinDwellTimeMs,
                        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uMaxDwellTimeMs,
                        pCuCmd->tPeriodicAppScanParams.tChannels[ i ].uTxPowerLevelDbm);   
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"\n");
}

VOID CuCmd_StartPeriodicScan (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, SCAN_CNCN_START_PERIODIC_SCAN,
                                &(pCuCmd->tPeriodicAppScanParams), sizeof(TPeriodicScanParams))) 
    {
        return;
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"Periodic application scan started.\n");
}

VOID CuCmd_StopPeriodicScan (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, SCAN_CNCN_STOP_PERIODIC_SCAN,
                                NULL, 0)) 
    {
        return;
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"Periodic application scan stopped.\n");
}

VOID CuCmd_ConfigScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    pCuCmd->scanPolicy.normalScanInterval =  parm[ 0 ].value;
    pCuCmd->scanPolicy.deterioratingScanInterval = parm[ 1 ].value;
    pCuCmd->scanPolicy.maxTrackFailures = (U8)(parm[ 2 ].value);
    pCuCmd->scanPolicy.BSSListSize = (U8)(parm[ 3 ].value);
    pCuCmd->scanPolicy.BSSNumberToStartDiscovery = (U8)(parm[ 4 ].value);
    pCuCmd->scanPolicy.numOfBands = (U8)(parm[ 5 ].value);
}

VOID CuCmd_ConfigScanBand(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    scan_bandPolicy_t* pBandPolicy = &(pCuCmd->scanPolicy.bandScanPolicy[ parm [ 0 ].value ]);

    pBandPolicy->band = parm[ 1 ].value;
    pBandPolicy->rxRSSIThreshold = (S8)(parm[ 2 ].value);
    pBandPolicy->numOfChannlesForDiscovery = (U8)(parm[ 3 ].value);
    pBandPolicy->numOfChannles = (U8)(parm[ 4 ].value);
}

VOID CuCmd_ConfigScanBandChannel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    scan_bandPolicy_t* pBandPolicy = &(pCuCmd->scanPolicy.bandScanPolicy[ parm [ 0 ].value ]);

    pBandPolicy->channelList[ parm[ 1 ].value ] = (U8)(parm[ 2 ].value);
}

VOID CuCmd_ConfigScanBandTrack(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    scan_bandPolicy_t* pBandPolicy = &(pCuCmd->scanPolicy.bandScanPolicy[ parm [ 0 ].value ]);

    if (parm[6].value < parm[7].value)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Max Dwell Time must be larger than or equal to Min Dwell Time...\n");
        return;
    }

    pBandPolicy->trackingMethod.scanType = parm[ 1 ].value;

    switch (pBandPolicy->trackingMethod.scanType)
    {
        case SCAN_TYPE_NORMAL_ACTIVE:
        case SCAN_TYPE_NORMAL_PASSIVE:
            pBandPolicy->trackingMethod.method.basicMethodParams.maxChannelDwellTime = (parm[ 6 ].value);
            pBandPolicy->trackingMethod.method.basicMethodParams.minChannelDwellTime = (parm[ 7 ].value);
            pBandPolicy->trackingMethod.method.basicMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->trackingMethod.method.basicMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->trackingMethod.method.basicMethodParams.probReqParams.bitrate = parm[ 9 ].value;
            pBandPolicy->trackingMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = (U8)(parm[ 8 ].value);
            pBandPolicy->trackingMethod.method.basicMethodParams.probReqParams.txPowerDbm = (U8)(parm[ 10 ].value);
            break;

        case SCAN_TYPE_TRIGGERED_ACTIVE:
        case SCAN_TYPE_TRIGGERED_PASSIVE:
            /* Check if valid TID */
            if (((parm[ 4 ].value) > 7) && ((parm[ 4 ].value) != 255))
            {
                os_error_printf (CU_MSG_INFO2, (PS8)"ERROR Tid (AC) should be 0..7 or 255 instead = %d (using default = 255)\n",(parm[ 4 ].value));
                parm[ 4 ].value = 255;
            }

            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.triggeringTid = (U8)(parm[ 4 ].value);
            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.basicMethodParams.maxChannelDwellTime = (parm[ 6 ].value);
            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.basicMethodParams.minChannelDwellTime = (parm[ 7 ].value);
            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.basicMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.basicMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.bitrate = parm[ 9 ].value;
            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.numOfProbeReqs = (U8)(parm[ 8 ].value);
            pBandPolicy->trackingMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.txPowerDbm = (U8)(parm[ 10 ].value);
            break;

        case SCAN_TYPE_SPS:
            pBandPolicy->trackingMethod.method.spsMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->trackingMethod.method.spsMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->trackingMethod.method.spsMethodParams.scanDuration = parm[ 5 ].value;
            break;

        default:
            pBandPolicy->trackingMethod.scanType = SCAN_TYPE_NO_SCAN;
            break;
    }
}

VOID CuCmd_ConfigScanBandDiscover(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    scan_bandPolicy_t* pBandPolicy = &(pCuCmd->scanPolicy.bandScanPolicy[ parm [ 0 ].value ]);

    if (parm[6].value < parm[7].value)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Max Dwell Time must be larger than or equal to Min Dwell Time...\n");
        return;
    }

    pBandPolicy->discoveryMethod.scanType = parm[ 1 ].value;

    switch (pBandPolicy->discoveryMethod.scanType)
    {
        case SCAN_TYPE_NORMAL_ACTIVE:
        case SCAN_TYPE_NORMAL_PASSIVE:
            pBandPolicy->discoveryMethod.method.basicMethodParams.maxChannelDwellTime = (parm[ 6 ].value);
            pBandPolicy->discoveryMethod.method.basicMethodParams.minChannelDwellTime = (parm[ 7 ].value);
            pBandPolicy->discoveryMethod.method.basicMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->discoveryMethod.method.basicMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->discoveryMethod.method.basicMethodParams.probReqParams.bitrate = parm[ 9 ].value;
            pBandPolicy->discoveryMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = (U8)(parm[ 8 ].value);
            pBandPolicy->discoveryMethod.method.basicMethodParams.probReqParams.txPowerDbm = (U8)(parm[ 10 ].value);
            break;

        case SCAN_TYPE_TRIGGERED_ACTIVE:
        case SCAN_TYPE_TRIGGERED_PASSIVE:
            /* Check if valid TID */
            if (((parm[ 4 ].value) > 7) && ((parm[ 4 ].value) != 255))
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"ERROR Tid (AC) should be 0..7 or 255 instead = %d (using default = 255)\n",(parm[ 4 ].value));
                parm[ 4 ].value = 255;
            }

            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.triggeringTid = (U8)(parm[ 4 ].value);
            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.basicMethodParams.maxChannelDwellTime = (parm[ 6 ].value);
            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.basicMethodParams.minChannelDwellTime = (parm[ 7 ].value);
            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.basicMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.basicMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.bitrate = parm[ 9 ].value;
            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.numOfProbeReqs = (U8)(parm[ 8 ].value);
            pBandPolicy->discoveryMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.txPowerDbm = (U8)(parm[ 10 ].value);
            break;

        case SCAN_TYPE_SPS:
            pBandPolicy->discoveryMethod.method.spsMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->discoveryMethod.method.spsMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->discoveryMethod.method.spsMethodParams.scanDuration = parm[ 5 ].value;
            break;

        default:
            pBandPolicy->discoveryMethod.scanType = SCAN_TYPE_NO_SCAN;
            break;
    }
}

VOID CuCmd_ConfigScanBandImmed(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    scan_bandPolicy_t* pBandPolicy = &(pCuCmd->scanPolicy.bandScanPolicy[ parm [ 0 ].value ]);

    if (parm[6].value < parm[7].value)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Max Dwell Time must be larger than or equal to Min Dwell Time...\n");
        return;
    }

    pBandPolicy->immediateScanMethod.scanType = parm[ 1 ].value;

    switch (pBandPolicy->immediateScanMethod.scanType)
    {
        case SCAN_TYPE_NORMAL_ACTIVE:
        case SCAN_TYPE_NORMAL_PASSIVE:
            pBandPolicy->immediateScanMethod.method.basicMethodParams.maxChannelDwellTime = (parm[ 6 ].value);
            pBandPolicy->immediateScanMethod.method.basicMethodParams.minChannelDwellTime = (parm[ 7 ].value);
            pBandPolicy->immediateScanMethod.method.basicMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->immediateScanMethod.method.basicMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->immediateScanMethod.method.basicMethodParams.probReqParams.bitrate = parm[ 9 ].value;
            pBandPolicy->immediateScanMethod.method.basicMethodParams.probReqParams.numOfProbeReqs = (U8)(parm[ 8 ].value);
            pBandPolicy->immediateScanMethod.method.basicMethodParams.probReqParams.txPowerDbm = (U8)(parm[ 10 ].value);
            break;

        case SCAN_TYPE_TRIGGERED_ACTIVE:
        case SCAN_TYPE_TRIGGERED_PASSIVE:
            /* Check if valid TID */
            if (((parm[ 4 ].value) > 7) && ((parm[ 4 ].value) != 255))
            {
                os_error_printf (CU_MSG_INFO2, (PS8)"ERROR Tid (AC) should be 0..7 or 255 instead = %d (using default = 255)\n",(parm[ 4 ].value));
                parm[ 4 ].value = 255;
            }

            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.triggeringTid = (U8)(parm[ 4 ].value);
            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.basicMethodParams.maxChannelDwellTime = (parm[ 6 ].value);
            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.basicMethodParams.minChannelDwellTime = (parm[ 7 ].value);
            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.basicMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.basicMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.bitrate = parm[ 9 ].value;
            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.numOfProbeReqs = (U8)(parm[ 8 ].value);
            pBandPolicy->immediateScanMethod.method.TidTriggerdMethodParams.basicMethodParams.probReqParams.txPowerDbm = (U8)(parm[ 10 ].value);
            break;

        case SCAN_TYPE_SPS:
            pBandPolicy->immediateScanMethod.method.spsMethodParams.earlyTerminationEvent = parm[ 2 ].value;
            pBandPolicy->immediateScanMethod.method.spsMethodParams.ETMaxNumberOfApFrames = (U8)(parm[ 3 ].value);
            pBandPolicy->immediateScanMethod.method.spsMethodParams.scanDuration = parm[ 5 ].value;
            break;

        default:
            pBandPolicy->immediateScanMethod.scanType = SCAN_TYPE_NO_SCAN;
            break;
    }
}

VOID CuCmd_DisplayScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 i;  

    os_error_printf(CU_MSG_INFO2, (PS8)"Scan Policy:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"Normal scan interval: %d, deteriorating scan interval: %d\n",
          pCuCmd->scanPolicy.normalScanInterval, pCuCmd->scanPolicy.deterioratingScanInterval);
    os_error_printf(CU_MSG_INFO2, (PS8)"Max track attempt failures: %d\n", pCuCmd->scanPolicy.maxTrackFailures);
    os_error_printf(CU_MSG_INFO2, (PS8)"BSS list size: %d, number of BSSes to start discovery: %d\n",
          pCuCmd->scanPolicy.BSSListSize, pCuCmd->scanPolicy.BSSNumberToStartDiscovery);
    os_error_printf(CU_MSG_INFO2, (PS8)"Number of configured bands: %d\n", pCuCmd->scanPolicy.numOfBands);
    for ( i = 0; i < pCuCmd->scanPolicy.numOfBands; i++ )
    {
        CuCmd_PrintScanBand(&(pCuCmd->scanPolicy.bandScanPolicy[ i ]));       
    }
}

VOID CuCmd_ClearScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    os_memset( &pCuCmd->scanPolicy, 0, sizeof(scan_Policy_t) );
    os_error_printf(CU_MSG_INFO2, (PS8)"Scan policy cleared.\n");
}

VOID CuCmd_SetScanPolicy(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SCAN_POLICY_PARAM_SET,
        &pCuCmd->scanPolicy, sizeof(scan_Policy_t))) return;
    os_error_printf(CU_MSG_INFO2, (PS8)"Scan policy stored.\n");
}

VOID CuCmd_GetScanBssList(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    bssList_t list;
    S32 i;

    if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SCAN_BSS_LIST_GET,
        &list, sizeof(bssList_t))) return;

    /* os_error_printf list */
    os_error_printf(CU_MSG_INFO2, (PS8)"BSS List:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"%-17s  %-7s  %-6s  %-4s  %-10s\n", (PS8)"BSSID", (PS8)"Band", (PS8)"Channel", (PS8)"RSSI", (PS8)"Neighbor?");
    os_error_printf(CU_MSG_INFO2, (PS8)"-----------------------------------------------------\n");
    for  ( i = 0; i < list.numOfEntries; i++ )
    {
        os_error_printf(CU_MSG_INFO2,  (PS8)"%02x.%02x.%02x.%02x.%02x.%02x  %s  %-7d  %-4d  %s\n", 
               list.BSSList[i].BSSID[0], list.BSSList[i].BSSID[1], list.BSSList[i].BSSID[2], list.BSSList[i].BSSID[3], list.BSSList[i].BSSID[4], list.BSSList[i].BSSID[5],
               band2Str[ list.BSSList[ i ].band ].name, 
               list.BSSList[ i ].channel, list.BSSList[ i ].RSSI, 
               (TRUE == list.BSSList[ i ].bNeighborAP ? (PS8)"Yes" : (PS8)"No") );
    }
}

VOID CuCmd_RoamingEnable(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION,
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t))) return;
    roamingMngrConfigParams.roamingMngrConfig.enableDisable = ROAMING_ENABLED;  
    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION,
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    os_error_printf(CU_MSG_INFO2, (PS8)"Roaming is enabled \n");
}

VOID CuCmd_RoamingDisable(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION,
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    roamingMngrConfigParams.roamingMngrConfig.enableDisable = ROAMING_DISABLED; 
    if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION,
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    os_error_printf(CU_MSG_INFO2, (PS8)"Roaming is disabled \n");
}

VOID CuCmd_RoamingLowPassFilter(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION,
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )   
    {       
        roamingMngrConfigParams.roamingMngrConfig.lowPassFilterRoamingAttempt = (U16) parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION,
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;  
    }   
    os_error_printf(CU_MSG_INFO2, (PS8)"Time in sec to wait before low quality Roaming Triggers, \n lowPassFilterRoamingAttempt = %d sec\n", 
           roamingMngrConfigParams.roamingMngrConfig.lowPassFilterRoamingAttempt);    
}

VOID CuCmd_RoamingQualityIndicator(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )   
    {       
        roamingMngrConfigParams.roamingMngrConfig.apQualityThreshold = (S8) parm[0].value;  
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    }       
    os_error_printf(CU_MSG_INFO2, (PS8)"Quality indicator (RSSI) to be used when comparing AP List matching quality, \n apQualityThreshold = %d \n",         
           (roamingMngrConfigParams.roamingMngrConfig.apQualityThreshold));
}

VOID CuCmd_RoamingDataRetryThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms)                                                                     
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )
    {
        roamingMngrConfigParams.roamingMngrThresholdsConfig.dataRetryThreshold =  (S8) parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    }    
    os_error_printf(CU_MSG_INFO2, (PS8)"dataRetryThreshold = %d \n", 
           roamingMngrConfigParams.roamingMngrThresholdsConfig.dataRetryThreshold);

}
VOID CuCmd_RoamingNumExpectedTbttForBSSLoss(THandle hCuCmd, ConParm_t parm[], U16 nParms)                                                    
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )
    {
        roamingMngrConfigParams.roamingMngrThresholdsConfig.numExpectedTbttForBSSLoss =  (S8) parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;    
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"Number of expected TBTTs for BSS Loss event, \n numExpectedTbttForBSSLoss = %d \n", 
           roamingMngrConfigParams.roamingMngrThresholdsConfig.numExpectedTbttForBSSLoss);

}
VOID CuCmd_RoamingTxRateThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms)                                                                  
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )
    {
        roamingMngrConfigParams.roamingMngrThresholdsConfig.txRateThreshold =  (S8 )parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    }    
    os_error_printf(CU_MSG_INFO2, (PS8)"txRateThreshold = %d \n", 
           roamingMngrConfigParams.roamingMngrThresholdsConfig.txRateThreshold);

}

VOID CuCmd_RoamingLowRssiThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms)                                                                           
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )
    {
        roamingMngrConfigParams.roamingMngrThresholdsConfig.lowRssiThreshold =  (S8) parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    }   
    os_error_printf(CU_MSG_INFO2, (PS8)"lowRssiThreshold = %d \n", 
           (roamingMngrConfigParams.roamingMngrThresholdsConfig.lowRssiThreshold));

}

VOID CuCmd_RoamingLowSnrThreshold(THandle hCuCmd, ConParm_t parm[], U16 nParms)                                                                           
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )
    {
        roamingMngrConfigParams.roamingMngrThresholdsConfig.lowSnrThreshold =  (S8)parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    }    
    os_error_printf(CU_MSG_INFO2, (PS8)"lowSnrThreshold = %d \n", roamingMngrConfigParams.roamingMngrThresholdsConfig.lowSnrThreshold);
}

VOID CuCmd_RoamingLowQualityForBackgroungScanCondition(THandle hCuCmd, ConParm_t parm[], U16 nParms) 
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )
    {
        roamingMngrConfigParams.roamingMngrThresholdsConfig.lowQualityForBackgroungScanCondition = (S8) parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    }    
    os_error_printf(CU_MSG_INFO2, (PS8)"Indicator used to increase the background scan period when quality is low, \n lowQualityForBackgroungScanCondition = %d \n", 
           (roamingMngrConfigParams.roamingMngrThresholdsConfig.lowQualityForBackgroungScanCondition));

}

VOID CuCmd_RoamingNormalQualityForBackgroungScanCondition(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;
    if( nParms != 0 )
    {
        roamingMngrConfigParams.roamingMngrThresholdsConfig.normalQualityForBackgroungScanCondition = (S8) parm[0].value;
        if(OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
            &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;    
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"Indicator used to reduce the background scan period when quality is normal, \n normalQualityForBackgroungScanCondition = %d \n", 
           (roamingMngrConfigParams.roamingMngrThresholdsConfig.normalQualityForBackgroungScanCondition));

}

VOID CuCmd_RoamingGetConfParams(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    roamingMngrConfigParams_t   roamingMngrConfigParams;
    
    if(OK != CuCommon_GetBuffer (pCuCmd->hCuCommon, ROAMING_MNGR_APPLICATION_CONFIGURATION, 
        &roamingMngrConfigParams, sizeof(roamingMngrConfigParams_t)) ) return;

    os_error_printf(CU_MSG_INFO2, (PS8)"Roaming is: %s \n", roamingMngrConfigParams.roamingMngrConfig.enableDisable ? "Enabled" : "Disabled\n");
    os_error_printf(CU_MSG_INFO2, (PS8)" lowPassFilterRoamingAttempt = %d sec,\n apQualityThreshold = %d\n", 
        roamingMngrConfigParams.roamingMngrConfig.lowPassFilterRoamingAttempt,
        roamingMngrConfigParams.roamingMngrConfig.apQualityThreshold);
    os_error_printf(CU_MSG_INFO2, (PS8)" Roaming Triggers' thresholds are: \n");
    os_error_printf(CU_MSG_INFO2, (PS8)" dataRetryThreshold = %d,\n lowQualityForBackgroungScanCondition = %d,\n lowRssiThreshold = %d,\n lowSnrThreshold = %d,\n normalQualityForBackgroungScanCondition = %d,\n numExpectedTbttForBSSLoss = %d,\n txRateThreshold = %d \n",
					roamingMngrConfigParams.roamingMngrThresholdsConfig.dataRetryThreshold,
					roamingMngrConfigParams.roamingMngrThresholdsConfig.lowQualityForBackgroungScanCondition,
					roamingMngrConfigParams.roamingMngrThresholdsConfig.lowRssiThreshold,
					roamingMngrConfigParams.roamingMngrThresholdsConfig.lowSnrThreshold,
					roamingMngrConfigParams.roamingMngrThresholdsConfig.normalQualityForBackgroungScanCondition,
					roamingMngrConfigParams.roamingMngrThresholdsConfig.numExpectedTbttForBSSLoss,
					roamingMngrConfigParams.roamingMngrThresholdsConfig.txRateThreshold);
}

VOID CuCmd_CurrBssUserDefinedTrigger(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t *pCuCmd = (CuCmd_t*)hCuCmd;
    TUserDefinedQualityTrigger  userTrigger;
    
    if (nParms == 0)
        return;

    userTrigger.uIndex     = (U8)parm[0].value;
    userTrigger.iThreshold = (U16)parm[1].value;
    userTrigger.uPacing    = (U16)parm[2].value;
    userTrigger.uMetric    = (U8)parm[3].value;
    userTrigger.uType      = (U8)parm[4].value;
    userTrigger.uDirection = (U8)parm[5].value;
    userTrigger.uHystersis = (U8)parm[6].value;
    userTrigger.uEnable    = (U8)parm[7].value;

    userTrigger.uClientID = 0; /* '0' means that external application with no clientId has registered for the event */

    if (OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, CURR_BSS_REGISTER_LINK_QUALITY_EVENT_PARAM,
                                  &userTrigger, sizeof(TUserDefinedQualityTrigger)) ) 
        return;    

    os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_RoamingUserDefinedTrigger: \n \
          index = %d, \n \
          threshold = %d, \n \
          pacing = %d, \n \
          metric = %d, \n \
          type = %d, \n \
          direction = %d, \n \
          hystersis = %d, \n \
          enable = %d \n",
          userTrigger.uIndex,
          userTrigger.iThreshold,
          userTrigger.uPacing,
          userTrigger.uMetric,
          userTrigger.uType,
          userTrigger.uDirection,
          userTrigger.uHystersis,
          userTrigger.uEnable);
}

VOID CuCmd_AddTspec(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_QOS_TSPEC_PARAMS TspecParams;
    
    TspecParams.uUserPriority = parm[0].value;
    TspecParams.uNominalMSDUsize = parm[1].value;
    TspecParams.uMeanDataRate = parm[2].value;
    TspecParams.uMinimumPHYRate = parm[3].value * 1000 * 1000;
    TspecParams.uSurplusBandwidthAllowance = parm[4].value << 13;
    TspecParams.uAPSDFlag = parm[5].value;
    TspecParams.uMinimumServiceInterval = parm[6].value;
    TspecParams.uMaximumServiceInterval = parm[7].value;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_ADD_TSPEC, 
        &TspecParams, sizeof(OS_802_11_QOS_TSPEC_PARAMS))) return;

    os_error_printf(CU_MSG_INFO2, (PS8)"TSpec request sent to driver...\n uUserPriority = %d\n uNominalMSDUsize = %d\n uMeanDataRate = %d\n uMinimumPHYRate = %d\n uSurplusBandwidthAllowance = %d\n uAPSDFlag = %d uMinimumServiceInterval = %d uMaximumServiceInterval = %d\n",
               parm[0].value,
               parm[1].value,
               parm[2].value,
               parm[3].value,
               parm[4].value,
               parm[5].value,
               parm[6].value,
               parm[7].value);
   
}

VOID CuCmd_GetTspec(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_QOS_TSPEC_PARAMS TspecParams;

    TspecParams.uUserPriority = parm[0].value;
    
    if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_TSPEC_PARAMS,
        &TspecParams, sizeof(OS_802_11_QOS_TSPEC_PARAMS))) return;

    os_error_printf(CU_MSG_INFO2, (PS8)"TSpec parameters retrieved:\nuUserPriority = %d\nuNominalMSDUsize = %d\nuMeanDataRate = %d\nuMinimumPHYRate = %d\nuSurplusBandwidthAllowance = %d\nuUAPSD_Flag = %d\nuMinimumServiceInterval = %d\nuMaximumServiceInterval = %d\nuMediumTime = %d\n",
               TspecParams.uUserPriority,
               TspecParams.uNominalMSDUsize,
               TspecParams.uMeanDataRate,
               TspecParams.uMinimumPHYRate,
               TspecParams.uSurplusBandwidthAllowance,
               TspecParams.uAPSDFlag,
               TspecParams.uMinimumServiceInterval,
               TspecParams.uMaximumServiceInterval,
               TspecParams.uMediumTime);
}

VOID CuCmd_DeleteTspec(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_QOS_DELETE_TSPEC_PARAMS TspecParams; 

    TspecParams.uUserPriority = parm[0].value;
    TspecParams.uReasonCode = parm[1].value;

    if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_DELETE_TSPEC, 
        &TspecParams, sizeof(OS_802_11_QOS_TSPEC_PARAMS))) return;

    os_error_printf(CU_MSG_INFO2, (PS8)"TSPEC Delete request sent to driver...\n uUserPriority = %d\n uReasonCode = %d\n",
        TspecParams.uUserPriority,
        TspecParams.uReasonCode);
}

VOID CuCmd_GetApQosParams(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_AC_QOS_PARAMS AcQosParams;
    S32 i = 0;

    /* test if we can get the AC QOS Params */
    AcQosParams.uAC = i;
    if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_AP_QOS_PARAMS,
        &AcQosParams, sizeof(AcQosParams))) return;

    os_error_printf(CU_MSG_INFO2, (PS8)"AP QOS Parameters:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"+----+-------------+----------+-----------+-----------+-----------+\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"| AC | AdmCtrlFlag |   AIFS   |   CwMin   |   CwMax   | TXOPLimit |\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"+----+-------------+----------+-----------+-----------+-----------+\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"| %2d | %11d | %8d | %9d | %9d | %9d |\n",
            i,
            AcQosParams.uAssocAdmissionCtrlFlag,
            AcQosParams.uAIFS,
            AcQosParams.uCwMin,
            AcQosParams.uCwMax,
            AcQosParams.uTXOPLimit);
         
    for (i=1; i<4; i++)
    {
        AcQosParams.uAC = i;
        if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_AP_QOS_PARAMS,
            &AcQosParams, sizeof(AcQosParams))) return;      

        os_error_printf(CU_MSG_INFO2, (PS8)"| %2d | %11d | %8d | %9d | %9d | %9d |\n",
            i,
            AcQosParams.uAssocAdmissionCtrlFlag,
            AcQosParams.uAIFS,
            AcQosParams.uCwMin,
            AcQosParams.uCwMax,
            AcQosParams.uTXOPLimit);
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"+----+-------------+----------+-----------+-----------+-----------+\n");
}

VOID CuCmd_GetPsRxStreamingParams(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TPsRxStreaming tPsRxStreaming;
    S32 i = 0;

    os_error_printf(CU_MSG_INFO2, (PS8)"PS Rx Streaming Parameters:\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"+-----+--------------+------------+---------+\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"| TID | StreamPeriod | uTxTimeout | Enabled |\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"+-----+--------------+------------+---------+\n");
         
    for (i=0; i<8; i++)
    {
        tPsRxStreaming.uTid = i;
        if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, QOS_MNGR_PS_RX_STREAMING,
            &tPsRxStreaming, sizeof(TPsRxStreaming))) return;      

        os_error_printf(CU_MSG_INFO2, (PS8)"| %3d | %12d | %10d | %7d |\n",
            tPsRxStreaming.uTid,         
            tPsRxStreaming.uStreamPeriod, 
            tPsRxStreaming.uTxTimeout,    
            tPsRxStreaming.bEnabled);
    }
    os_error_printf(CU_MSG_INFO2, (PS8)"+-----+--------------+------------+---------+\n");
}

VOID CuCmd_GetApQosCapabilities(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    OS_802_11_AP_QOS_CAPABILITIES_PARAMS ApQosCapabiltiesParams;

    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_AP_QOS_CAPABILITIES,
        &ApQosCapabiltiesParams, sizeof(OS_802_11_AP_QOS_CAPABILITIES_PARAMS))) return;
   
    os_error_printf(CU_MSG_INFO2, (PS8)"AP Qos Capabilities:\n QOSFlag = %d\n APSDFlag = %d\n",
        ApQosCapabiltiesParams.uQOSFlag,
        ApQosCapabiltiesParams.uAPSDFlag);
       
}

VOID CuCmd_GetAcStatus(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_AC_UPSD_STATUS_PARAMS AcStatusParams;

    AcStatusParams.uAC = parm[0].value;

    if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_CURRENT_AC_STATUS,
        &AcStatusParams, sizeof(OS_802_11_AC_UPSD_STATUS_PARAMS))) return;

    os_error_printf(CU_MSG_INFO2, (PS8)"AC %d Status:\n", AcStatusParams.uAC);
    os_error_printf(CU_MSG_INFO2, (PS8)"PS Scheme = %d (0=LEGACY, 1=UPSD)\n", AcStatusParams.uCurrentUAPSDStatus);
    os_error_printf(CU_MSG_INFO2, (PS8)"Admission Status = %d (0=NOT_ADMITTED, 1=WAIT_ADMISSION, 2=ADMITTED)\n", AcStatusParams.pCurrentAdmissionStatus);
}

VOID CuCmd_ModifyMediumUsageTh(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_THRESHOLD_CROSS_PARAMS ThCrossParams;

    if (nParms == 3) /* If user supplied 3 parameters - this is a SET operation */
    {
        ThCrossParams.uAC = parm[0].value;
        ThCrossParams.uHighThreshold = parm[1].value;
        ThCrossParams.uLowThreshold = parm[2].value;

        if (ThCrossParams.uLowThreshold > ThCrossParams.uHighThreshold)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Low threshold cannot be higher than the High threshold...Aborting...\n");
            return;
        }

        if(OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SET_MEDIUM_USAGE_THRESHOLD,
            &ThCrossParams, sizeof(OS_802_11_THRESHOLD_CROSS_PARAMS))) return;      

        os_error_printf(CU_MSG_INFO2, (PS8)"Medium usage threshold for AC %d has been set to:\n LowThreshold = %d\n HighThreshold = %d\n",
            ThCrossParams.uAC,
            ThCrossParams.uLowThreshold,
            ThCrossParams.uHighThreshold);
   }
   else if (nParms == 1) /* Only 1 parameter means a GET operation */
   {
        ThCrossParams.uAC = parm[0].value;
        ThCrossParams.uLowThreshold = 0;
        ThCrossParams.uHighThreshold = 0;

        if(OK != CuCommon_GetSetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_MEDIUM_USAGE_THRESHOLD,
            &ThCrossParams, sizeof(OS_802_11_THRESHOLD_CROSS_PARAMS))) return;      

        os_error_printf(CU_MSG_INFO2, (PS8)"Medium usage threshold for AC %d:\n LowThreshold = %d\n HighThreshold = %d\n",
            ThCrossParams.uAC,
            ThCrossParams.uLowThreshold,
            ThCrossParams.uHighThreshold);
   }
}


VOID CuCmd_GetDesiredPsMode(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_QOS_DESIRED_PS_MODE DesiredPsMode;

    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_DESIRED_PS_MODE,
        &DesiredPsMode, sizeof(OS_802_11_QOS_DESIRED_PS_MODE))) return;     

    os_error_printf(CU_MSG_INFO2, (PS8)"Desired PS Mode (0=PS_POLL, 1=UPSD, 2=PS_NONE):\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"===============================================\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"  +-----------+------+\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"  |    AC     | Mode |\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"  +-----------+------+\n");
    os_error_printf(CU_MSG_INFO2, (PS8)"  |  General  |  %d   |\n", DesiredPsMode.uDesiredPsMode);
    os_error_printf(CU_MSG_INFO2, (PS8)"  |   BE_AC   |  %d   |\n", DesiredPsMode.uDesiredWmeAcPsMode[QOS_AC_BE]);
    os_error_printf(CU_MSG_INFO2, (PS8)"  |   BK_AC   |  %d   |\n", DesiredPsMode.uDesiredWmeAcPsMode[QOS_AC_BK]);
    os_error_printf(CU_MSG_INFO2, (PS8)"  |   VI_AC   |  %d   |\n", DesiredPsMode.uDesiredWmeAcPsMode[QOS_AC_VI]);
    os_error_printf(CU_MSG_INFO2, (PS8)"  |   VO_AC   |  %d   |\n", DesiredPsMode.uDesiredWmeAcPsMode[QOS_AC_VO]);
    os_error_printf(CU_MSG_INFO2, (PS8)"  +-----------+------+\n");
}


VOID CuCmd_InsertClsfrEntry(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    clsfr_tableEntry_t newUserTableEntry;
    S32 i;

    if (nParms >=2)
        newUserTableEntry.DTag = (U8) parm[1].value;

    switch(parm[0].value)
    {
        case D_TAG_CLSFR:
            os_error_printf(CU_MSG_INFO2, (PS8)"Cannot insert D_TAG classifier entry!\n");
            return;        
        case DSCP_CLSFR:
            if (nParms != 3)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"DSCP_CLSFR Entry type, wrong number of parameters(too many?)\n");
                return;
            }
            newUserTableEntry.Dscp.CodePoint = (U8) parm[2].value;
            os_error_printf(CU_MSG_INFO2, (PS8)"Inserting new DSCP_CLSFR classifier entry\nD-Tag = %d\nCodePoint = %d\n",newUserTableEntry.DTag,newUserTableEntry.Dscp.CodePoint);
            break;
        case PORT_CLSFR:
            if (nParms != 3)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"PORT_CLSFR Entry type, wrong number of parameters(too many?)\n");
                return;
            }
            newUserTableEntry.Dscp.DstPortNum = (U16) parm[2].value;
            os_error_printf(CU_MSG_INFO2, (PS8)"Inserting new PORT_CLSFR classifier entry\nD-Tag = %d\nPort = %d\n",newUserTableEntry.DTag,newUserTableEntry.Dscp.DstPortNum);
            break;
        case IPPORT_CLSFR:
            if (nParms != 7)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"PORT_CLSFR Entry type, wrong number of parameters\n");
                return;
            }
            newUserTableEntry.Dscp.DstIPPort.DstPortNum = (U16) parm[2].value;
            newUserTableEntry.Dscp.DstIPPort.DstIPAddress = 0;
            for(i=0; i<4; i++)
            {
                newUserTableEntry.Dscp.DstIPPort.DstIPAddress |= parm[i+3].value << i * 8;
            }
            os_error_printf(CU_MSG_INFO2, (PS8)"Inserting new IPPORT_CLSFR classifier entry\nD-Tag = %d\nPort = %d\nIP = %3d.%d.%d.%d\n",
                newUserTableEntry.DTag,
                newUserTableEntry.Dscp.DstIPPort.DstPortNum,
                (S32)parm[3].value,(S32)parm[4].value,(S32)parm[5].value,(S32)parm[6].value);
            break;
        default:
            os_error_printf(CU_MSG_INFO2, (PS8)"Unknown Classifier Type - Command aborted!\n");
            return;
    }

    if(CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_CONFIG_TX_CLASS,
        &newUserTableEntry, sizeof(clsfr_tableEntry_t)))
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Failed to insert new classifier entry...\n");
    }
}

VOID CuCmd_RemoveClsfrEntry(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    clsfr_tableEntry_t newUserTableEntry;
    S32 i;

    if (nParms >=2)
        newUserTableEntry.DTag = (U8) parm[1].value;

    switch(parm[0].value)
    {
        case D_TAG_CLSFR:
            os_error_printf(CU_MSG_INFO2, (PS8)"Cannot remove D_TAG classifier entry!\n");
            return;
        case DSCP_CLSFR:
            if (nParms != 3)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"DSCP_CLSFR Entry type, wrong number of parameters(too many?)\n");
                return;
            }
            newUserTableEntry.Dscp.CodePoint = (U8) parm[2].value;
            os_error_printf(CU_MSG_INFO2, (PS8)"Removing DSCP_CLSFR classifier entry\nD-Tag = %d\nCodePoint = %d\n",newUserTableEntry.DTag,newUserTableEntry.Dscp.CodePoint);
        break;
        case PORT_CLSFR:
            if (nParms != 3)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"PORT_CLSFR Entry type, wrong number of parameters(too many?)\n");
                return;
            }
            newUserTableEntry.Dscp.DstPortNum = (U16) parm[2].value;
            os_error_printf(CU_MSG_INFO2, (PS8)"Removing PORT_CLSFR classifier entry\nD-Tag = %d\nPort = %d\n",newUserTableEntry.DTag,newUserTableEntry.Dscp.DstPortNum);
        break;
        case IPPORT_CLSFR:
            if (nParms != 7)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"PORT_CLSFR Entry type, wrong number of parameters\n");
                return;
            }
            newUserTableEntry.Dscp.DstIPPort.DstPortNum = (U16) parm[2].value;
            newUserTableEntry.Dscp.DstIPPort.DstIPAddress = 0;
            for(i=0; i<4; i++)
                {
                    newUserTableEntry.Dscp.DstIPPort.DstIPAddress |= parm[i+3].value << i * 8;
                }
            os_error_printf(CU_MSG_INFO2, (PS8)"Removing IPPORT_CLSFR classifier entry\nD-Tag = %d\nPort = %d\nIP = %3d.%d.%d.%d\n",
                    newUserTableEntry.DTag,
                    newUserTableEntry.Dscp.DstIPPort.DstPortNum,
                    (S32)parm[3].value,(S32)parm[4].value,(S32)parm[5].value,(S32)parm[6].value);
            break;
        default:
            os_error_printf(CU_MSG_INFO2, (PS8)"Unknown Classifier Type - Command aborted!\n");
            return;
        break;
    }
   
    if(CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_REMOVE_CLSFR_ENTRY,
        &newUserTableEntry, sizeof(clsfr_tableEntry_t)))
    {
       os_error_printf(CU_MSG_INFO2, (PS8)"Failed to remove classifier entry...\n");
    }
}


VOID CuCmd_SetPsRxDelivery(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TPsRxStreaming tPsRxStreaming;

    tPsRxStreaming.uTid          = parm[0].value;
    tPsRxStreaming.uStreamPeriod = parm[1].value;
    tPsRxStreaming.uTxTimeout    = parm[2].value;
    tPsRxStreaming.bEnabled      = parm[3].value;

    if (CuCommon_SetBuffer(pCuCmd->hCuCommon, QOS_MNGR_PS_RX_STREAMING,
        &tPsRxStreaming, sizeof(TPsRxStreaming)) == OK)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Sent PS Rx Delivery to driver...");
    }
    else
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Error: could not set PS Rx Delivery in driver...\n");
    }
    os_error_printf(CU_MSG_INFO2, 
        (PS8)"TID = %d \n RxPeriod = %d \n TxTimeout = %d\n Enabled = %d\n", 
        tPsRxStreaming.uTid,      
        tPsRxStreaming.uStreamPeriod, 
        tPsRxStreaming.uTxTimeout,
        tPsRxStreaming.bEnabled);
}


VOID CuCmd_SetQosParams(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_QOS_PARAMS QosParams;

    QosParams.acID=parm[0].value;
    QosParams.MaxLifeTime=parm[1].value;
    QosParams.PSDeliveryProtocol = parm[2].value;
   
    if (CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SET_QOS_PARAMS,
        &QosParams, sizeof(OS_802_11_QOS_PARAMS)) == OK)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Sent QOS params to driver...\n AC Number=%d \n MaxLifeTime=%d \n PSDeliveryProtocol = %d\n", 
            QosParams.acID,
            QosParams.MaxLifeTime,
            QosParams.PSDeliveryProtocol);
   }
   else
   {
        os_error_printf(CU_MSG_INFO2, (PS8)"Error: could not set QOS params...\n");
   }
}

VOID CuCmd_SetRxTimeOut(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_QOS_RX_TIMEOUT_PARAMS rxTimeOut;

    rxTimeOut.psPoll = parm[0].value;
    rxTimeOut.UPSD   = parm[1].value;

    if (nParms != 2) 
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Please enter Rx Time Out:\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 0 - psPoll (0 - 65000)\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 1 - UPSD (1 - 65000)\n");
    }
    else
    {
        if(CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SET_RX_TIMEOUT,
            &rxTimeOut, sizeof(OS_802_11_QOS_RX_TIMEOUT_PARAMS)) == OK)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Sent QOS Rx TimeOut params to driver...\n PsPoll = %d\n UPSD = %d\n", 
                rxTimeOut.psPoll,
                rxTimeOut.UPSD);
        }
        else
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Error: could not set Rx TimeOut..\n");
        }
    }
}

VOID CuCmd_RegisterEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if( nParms )
    {
        U32 event; 
        S32 res, i;
        
        event = (U32)parm[0].value;

        CU_CMD_FIND_NAME_ARRAY(i, event_type, event);
        if(i == SIZE_ARR(event_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_RegisterEvents, event %d is not defined!\n", event);
            return;
        }

        res = IpcEvent_EnableEvent(pCuCmd->hIpcEvent, event);       
        if (res == EOALERR_IPC_EVENT_ERROR_EVENT_ALREADY_ENABLED)
        {
            CU_CMD_FIND_NAME_ARRAY(i, event_type, event);
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_RegisterEvents, event %s is already enabled!\n", event_type[i].name);
            return;
        }

    }
    else
    {
        print_available_values(event_type);
    }   
}

VOID CuCmd_UnregisterEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if( nParms )
    {
        U32 event; 
        S32 res, i;
        
        event = (U32)parm[0].value;

        CU_CMD_FIND_NAME_ARRAY(i, event_type, event);
        if(i == SIZE_ARR(event_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_RegisterEvents, event %d is not defined!\n", event);
            return;
        }

        res = IpcEvent_DisableEvent(pCuCmd->hIpcEvent, event);      
        if (res == EOALERR_IPC_EVENT_ERROR_EVENT_ALREADY_DISABLED)
        {
            CU_CMD_FIND_NAME_ARRAY(i, event_type, event);
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_RegisterEvents, event %s is already disabled!\n", event_type[i].name);
            return;
        }

    }
    else
    {
        print_available_values(event_type);
    }   
}

VOID CuCmd_EnableBtCoe(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    ESoftGeminiEnableModes BtMode;
    S32 i;

    named_value_t BtCoe_Mode[] =
    {
        { SG_DISABLE,       (PS8)"Disable" },
        { SG_PROTECTIVE,    (PS8)"Protective" },
        { SG_OPPORTUNISTIC, (PS8)"Opportunistic" },
    };
    

    if(nParms)
    {
        CU_CMD_FIND_NAME_ARRAY(i, BtCoe_Mode, parm[0].value);
        if(i == SIZE_ARR(BtCoe_Mode))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_EnableBtCoe, mode %d is not defined!\n", parm[0].value);
            return;
        }
        BtMode = parm[0].value;
        CuCommon_SetU32(pCuCmd->hCuCommon, SOFT_GEMINI_SET_ENABLE, BtMode);
    }
    else
    {
        print_available_values(BtCoe_Mode);
    }
}

VOID CuCmd_ConfigBtCoe(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 Values[NUM_OF_CONFIG_PARAMS_IN_SG];
    U8 Index;

    if( nParms != NUM_OF_CONFIG_PARAMS_IN_SG )
    {
		os_error_printf(CU_MSG_INFO2, (PS8)"Please enter <index (0,1..)> <value> \n");

		os_error_printf(CU_MSG_INFO2, (PS8)"Param 0  - coexBtPerThreshold (0 - 10000000) PER threshold in PPM of the BT voice \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 1  - coexAutoScanCompensationMaxTime (0 - 10000000 usec)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 2  - coexBtNfsSampleInterval (1 - 65000 msec)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 3  - coexBtLoadRatio (0 - 100 %)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 4  - coexAutoPsMode (0 = Disabled, 1 = Enabled) Auto Power Save \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 5  - coexHv3AutoScanEnlargedNumOfProbeReqPercent (%)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 6  - coexHv3AutoScanEnlargedScanWinodowPercent (%)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 7  - coexAntennaConfiguration (0 = Single, 1 = Dual)\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 8  - coexMaxConsecutiveBeaconMissPrecent (1 - 100 %) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 9  - coexAPRateAdapationThr - rates (1 - 54)\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 10 - coexAPRateAdapationSnr 	  (-128 - 127)\n");

		os_error_printf(CU_MSG_INFO2, (PS8)"Param 11 - coexWlanPsBtAclMasterMinBR      (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 12 - coexWlanPsBtAclMasterMaxBR      (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 13 - coexWlanPsMaxBtAclMasterBR      (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 14 - coexWlanPsBtAclSlaveMinBR   	   (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 15 - coexWlanPsBtAclSlaveMaxBR  	   (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 16 - coexWlanPsMaxBtAclSlaveBR       (msec) \n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 17 - coexWlanPsBtAclMasterMinEDR     (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 18 - coexWlanPsBtAclMasterMaxEDR     (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 19 - coexWlanPsMaxBtAclMasterEDR     (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 20 - coexWlanPsBtAclSlaveMinEDR      (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 21 - coexWlanPsBtAclSlaveMaxEDR  	   (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 22 - coexWlanPsMaxBtAclSlaveEDR      (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 23 - coexRxt                    (usec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 24 - coexTxt                    (usec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 25 - coexAdaptiveRxtTxt    	  (0 = Disable, 1 = Enable) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 26 - coexPsPollTimeout          (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 27 - coexUpsdTimeout       	  (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 28 - coexWlanActiveBtAclMasterMinEDR (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 29 - coexWlanActiveBtAclMasterMaxEDR (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 30 - coexWlanActiveMaxBtAclMasterEDR (msec) \n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 31 - coexWlanActiveBtAclSlaveMinEDR  (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 32 - coexWlanActiveBtAclSlaveMaxEDR  (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 33 - coexWlanActiveMaxBtAclSlaveEDR  (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 34 - coexWlanActiveBtAclMinBR        (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 35 - coexWlanActiveBtAclMaxBR        (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 36 - coexWlanActiveMaxBtAclBR        (msec) \n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 37 - coexHv3AutoEnlargePassiveScanWindowPercent \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 38 - coexA2DPAutoEnlargePassiveScanWindowPercent \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 39 - coexPassiveScanBtTime (msec) \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 40 - coexPassiveScanWlanTime (msec)\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 41 - coexTempParam5 \n");

		return;
    }
    if ((parm[0].value == SOFT_GEMINI_RATE_ADAPT_THRESH) && (CuCmd_IsValueRate(parm[1].value) == FALSE))
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Invalid rate - PHY rate valid values are: 1,2,5,6,9,11,12,18,24,36,48,54\n");
    }
    else
    {
        for (Index = 0; Index < NUM_OF_CONFIG_PARAMS_IN_SG; Index++ )
        {
            Values[Index] = parm[Index].value;
/* value[0] - parmater index, value[1] - parameter value */
        }
        CuCommon_SetBuffer(pCuCmd->hCuCommon, SOFT_GEMINI_SET_CONFIG, Values, sizeof(Values));
    }
}

VOID CuCmd_GetBtCoeStatus(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 uDummyBuf;

    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, SOFT_GEMINI_GET_CONFIG,
            &uDummyBuf, sizeof(U32)))
    {
        return;
    }
}

VOID CuCmd_ConfigCoexActivity(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TCoexActivity tCoexActivity;

    if( nParms != NUM_OF_COEX_ACTIVITY_PARAMS_IN_SG )
    {
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 1 - coexIp          (0 - 1) BT-0, WLAN-1 \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 2 - activityId      (0 - 24)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 3 - defaultPriority (0 - 255)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 4 - raisedPriority  (0 - 255)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 5 - minService      (0 - 65535)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 6 - maxService      (0 - 65535)  \n");
    }
    else
    {
        tCoexActivity.coexIp          = (U8)parm[0].value;
        tCoexActivity.activityId      = (U8)parm[1].value;
        tCoexActivity.defaultPriority = (U8)parm[2].value;
        tCoexActivity.raisedPriority  = (U8)parm[3].value;
        tCoexActivity.minService      = (U16)parm[4].value;
        tCoexActivity.maxService      = (U16)parm[5].value;

        CuCommon_SetBuffer(pCuCmd->hCuCommon, TWD_COEX_ACTIVITY_PARAM,
        &tCoexActivity, sizeof(tCoexActivity));
    }
}

VOID CuCmd_ConfigFmCoex(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TFmCoexParams tFmCoexParams;

    if (nParms != 10)
    {
		os_error_printf(CU_MSG_INFO2, (PS8)"1 - Enable                   (0 - 1)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"2 - SwallowPeriod            (0 - 255)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"3 - NDividerFrefSet1         (0 - 255)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"4 - NDividerFrefSet2         (0 - 255)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"5 - MDividerFrefSet1         (0 - 65535)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"6 - MDividerFrefSet2         (0 - 65535)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"7 - CoexPllStabilizationTime (0 - 4294967295)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"8 - LdoStabilizationTime     (0 - 65535)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"9 - FmDisturbedBandMargin    (0 - 255)  \n");
		os_error_printf(CU_MSG_INFO2, (PS8)"10- SwallowClkDif            (0 - 255)  \n");
    }
    else
    {
        tFmCoexParams.uEnable                    = (TI_UINT8)parm[0].value;
        tFmCoexParams.uSwallowPeriod             = (TI_UINT8)parm[1].value;
        tFmCoexParams.uNDividerFrefSet1          = (TI_UINT8)parm[2].value;
        tFmCoexParams.uNDividerFrefSet2          = (TI_UINT8)parm[3].value;
        tFmCoexParams.uMDividerFrefSet1          = (TI_UINT16)parm[4].value;
        tFmCoexParams.uMDividerFrefSet2          = (TI_UINT16)parm[5].value;
        tFmCoexParams.uCoexPllStabilizationTime  = parm[6].value;
        tFmCoexParams.uLdoStabilizationTime      = (TI_UINT16)parm[7].value;
        tFmCoexParams.uFmDisturbedBandMargin     = (TI_UINT8)parm[8].value;
        tFmCoexParams.uSwallowClkDif             = (TI_UINT8)parm[9].value;

        CuCommon_SetBuffer(pCuCmd->hCuCommon, TWD_FM_COEX_PARAM, &tFmCoexParams, sizeof(TFmCoexParams));
    }
}

VOID CuCmd_SetPowerMode(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TPowerMgr_PowerMode Mode;
    S32 i;
    
    if( nParms )
    {
        CU_CMD_FIND_NAME_ARRAY(i, power_mode_val, parm[0].value);
        if(i == SIZE_ARR(power_mode_val))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetPowerMode, mode %d is not defined!\n", parm[0].value);
            return;
        }
        Mode.PowerMode = parm[0].value;
        Mode.PowerMngPriority = POWER_MANAGER_USER_PRIORITY;
        CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_POWER_MODE_SET,
            &Mode, sizeof(TPowerMgr_PowerMode));
    }
    else
    {
        if(!CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_POWER_MODE_GET, &Mode, sizeof(TPowerMgr_PowerMode)))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Power mode: %d\n", Mode.PowerMode);
            print_available_values(power_mode_val);
        }
    }
}

VOID CuCmd_SetPowerSavePowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 PowerSavePowerLevel;
    S32 i;
    
    if( nParms )
    {
        CU_CMD_FIND_NAME_ARRAY(i, power_level, parm[0].value);
        if(i == SIZE_ARR(power_level))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetPowerSavePowerLevel, level %d is not defined!\n", parm[0].value);
            return;
        }
        PowerSavePowerLevel = parm[0].value;
        CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_802_11_POWER_LEVEL_PS_SET, PowerSavePowerLevel);
    }
    else
    {
        if(!CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_802_11_POWER_LEVEL_PS_GET, &PowerSavePowerLevel))
        {
            CU_CMD_FIND_NAME_ARRAY(i, power_level, PowerSavePowerLevel);
            os_error_printf(CU_MSG_INFO2, (PS8)"Power Level PowerSave is: %s\n", power_level[i].name);
            print_available_values(power_level);
        }
    }
}

VOID CuCmd_SetDefaultPowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 DefaultPowerLevel;
    S32 i;
    
    if( nParms )
    {
        CU_CMD_FIND_NAME_ARRAY(i, power_level, parm[0].value);
        if(i == SIZE_ARR(power_level))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetDefaultPowerLevel, level %d is not defined!\n", parm[0].value);
            return;
        }
        DefaultPowerLevel = parm[0].value;
        CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_802_11_POWER_LEVEL_DEFAULT_SET, DefaultPowerLevel);
    }
    else
    {
        if(!CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_802_11_POWER_LEVEL_DEFAULT_GET, &DefaultPowerLevel))
        {
            CU_CMD_FIND_NAME_ARRAY(i, power_level, DefaultPowerLevel);
            os_error_printf(CU_MSG_INFO2, (PS8)"Power Level Default is: %s\n", power_level[i].name);
            print_available_values(power_level);
        }        
    }
}

VOID CuCmd_SetDozeModeInAutoPowerLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 DozeModeInAutoPowerLevel;
    S32 i;
    
    if( nParms )
    {
        DozeModeInAutoPowerLevel = parm[0].value;
        
        if((DozeModeInAutoPowerLevel > AUTO_POWER_MODE_DOZE_MODE_MAX_VALUE) || (DozeModeInAutoPowerLevel < AUTO_POWER_MODE_DOZE_MODE_MIN_VALUE))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetDozeModeInAutoPowerLevel, level %d is not defined!\n", DozeModeInAutoPowerLevel);
            return;
        }       
        CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_802_11_POWER_LEVEL_DOZE_MODE_SET, DozeModeInAutoPowerLevel);
    }
    else
    {
        /* set Short or Long Doze. no use of other parameters */
        if(!CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_802_11_POWER_LEVEL_DOZE_MODE_GET, &DozeModeInAutoPowerLevel))
        {            
            CU_CMD_FIND_NAME_ARRAY(i, power_mode_val, DozeModeInAutoPowerLevel);
            os_error_printf(CU_MSG_INFO2, (PS8)"Doze power level in auto mode is: %s\n", power_mode_val[i].name);           
        }
    }
}

VOID CuCmd_SetTrafficIntensityTh(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS TrafficIntensityTh;

    if (nParms == 3)
    {
        TrafficIntensityTh.uHighThreshold = parm[0].value;
        TrafficIntensityTh.uLowThreshold = parm[1].value;
        TrafficIntensityTh.TestInterval = parm[2].value;
   
        if (TrafficIntensityTh.uLowThreshold >= TrafficIntensityTh.uHighThreshold)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetTrafficIntensityTh - low threshold equal or greater than the high threshold...aborting...\n");
        }
   
        if(OK == CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_SET_TRAFFIC_INTENSITY_THRESHOLDS, 
            &TrafficIntensityTh, sizeof(OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS)))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Successfully set traffic intensity thresholds...\n");
        }
        else
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetTrafficIntensityTh - cannot set thresholds\n");
        }
    }
    else if (nParms == 0)
    {
        if(OK == CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_GET_TRAFFIC_INTENSITY_THRESHOLDS, 
            &TrafficIntensityTh, sizeof(OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS)))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Traffic intensity thresholds :\n HighThreshold = %d\n LowThreshold = %d\n TestInterval = %d\n",
                  TrafficIntensityTh.uHighThreshold,
                  TrafficIntensityTh.uLowThreshold,
                  TrafficIntensityTh.TestInterval);
        }
        else
        {
            os_error_printf (CU_MSG_ERROR, (PS8)"Error - CuCmd_SetTrafficIntensityTh - cannot get thresholds\n");
        }
    }
}

VOID CuCmd_EnableTrafficEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    if(OK != CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_802_11_TOGGLE_TRAFFIC_INTENSITY_EVENTS, TRUE) ) return;
    os_error_printf(CU_MSG_INFO2, (PS8)"Traffic intensity thresholds enabled...\n");
}

VOID CuCmd_DisableTrafficEvents(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    if(OK != CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_802_11_TOGGLE_TRAFFIC_INTENSITY_EVENTS, FALSE) ) return;
    os_error_printf(CU_MSG_INFO2, (PS8)"Traffic intensity thresholds disabled...\n");
}

VOID CuCmd_SetDcoItrimParams(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    DcoItrimParams_t dcoItrimParams;

    if (nParms == 0)
    {
        if (OK == CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_DCO_ITRIM_PARAMS, &dcoItrimParams, sizeof(DcoItrimParams_t)))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"DCO Itrim Params :\n enable = %d\n moderationTimeoutUsec = %d\n",
                            dcoItrimParams.enable, dcoItrimParams.moderationTimeoutUsec);
        }
        else
        {
            os_error_printf (CU_MSG_ERROR, (PS8)"Error - CuCmd_SetDcoItrimParams - cannot get DCO Itrim Params\n");
        }
    }

    else
    {      
        dcoItrimParams.enable = (Bool_e)parm[0].value;
        dcoItrimParams.moderationTimeoutUsec = parm[1].value;

        if (OK == CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_DCO_ITRIM_PARAMS, &dcoItrimParams, sizeof(DcoItrimParams_t)))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Successfully set DCO Itrim Params...\n");
        }
        else
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetDcoItrimParams - cannot set DCO Itrim Params\n");
        }
    }
}

VOID CuCmd_LogAddReport(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U8 ModuleTable[REPORT_FILES_NUM], ModuleValue[REPORT_FILES_NUM] = {0};
    int index = 0;

    os_memcpy((THandle)ModuleValue, (THandle)(parm[0].value), nParms);

    for (index = 0; index < REPORT_FILES_NUM; index ++)
    {
        if (ModuleValue[index] == '1')
        {
            ModuleTable[index] = '1';
        } 
        else
        {
            ModuleTable[index] = '0';
        }
    }
    CuCommon_SetBuffer(pCuCmd->hCuCommon, REPORT_MODULE_TABLE_PARAM, ModuleTable, REPORT_FILES_NUM);
}

VOID CuCmd_LogReportSeverityLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U8 SeverityTable[REPORT_SEVERITY_MAX];
    S32 index = 0;
    PS8 SeverityValue = (PS8)(parm[0].value);

    /* Get the current report severity */
    if (!CuCommon_GetBuffer(pCuCmd->hCuCommon, REPORT_SEVERITY_TABLE_PARAM, SeverityTable, REPORT_SEVERITY_MAX))    
    {
        if(nParms == 0)
        {            
            S32 i;

            os_error_printf(CU_MSG_INFO2, (PS8)"Severity:\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"-------------------------------\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"%14s\tState\t%s\n", (PS8)"Severity level", (PS8)"Desc");

            for( i=1; i<SIZE_ARR(report_severity); i++ )
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"%d\t%c\t%s\n", report_severity[i].value, (SeverityTable[i] == '1') ? '+' : ' ', report_severity[i].name );
            }

            os_error_printf(CU_MSG_INFO2, (PS8)"* Use '0' to clear all table.\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"* Use '%d' (max index) to set all table.\n", REPORT_SEVERITY_MAX);            
        }
        else
        {
            for (index = 0; index < REPORT_SEVERITY_MAX; index ++)
            {
                if (SeverityValue[index] == '0')
                {
                    SeverityTable[index] = '0';
                } 
                else
                {
                    SeverityTable[index] = '1';
                }
            }
            CuCommon_SetBuffer(pCuCmd->hCuCommon, REPORT_SEVERITY_TABLE_PARAM, SeverityTable, REPORT_SEVERITY_MAX);
        }
    }
    else
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error retriving the severity table from the driver\n");
    }
}

VOID CuCmd_SetReport(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U8 *pModuleTable = (U8 *)parm[0].value;

    if( nParms != 1)
    {
        U8 ModuleTable[REPORT_FILES_NUM];
        S32 i;
    
        if (!CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_GET, ModuleTable, REPORT_FILES_NUM))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"-------------------------------\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"%.5s\tState\t %s\n", (PS8)"Index", (PS8)"Desc");
        
            for( i = 0; i < SIZE_ARR(report_module); i++)
            {
                /* Check if there is string content (the first character is not ZERO) */
                if( report_module[i].name[0] )
                {
                    U8 module_num = (U8) report_module[i].value;
                    os_error_printf(CU_MSG_INFO2, (PS8)"%3d\t%c\t%s\n", 
                         module_num, 
                         (ModuleTable[module_num] == '1') ? '+' : ' ', 
                         report_module[i].name );
                }
            }
        }
        else
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Error reading the report table form the driver\n");
        }
    }
    else
    {
        CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_SET, pModuleTable, REPORT_FILES_NUM);
    }
}

VOID CuCmd_AddReport(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U8 ModuleTable[REPORT_FILES_NUM];
    
    if( nParms != 1)
    {       
        S32 i;    
        if (!CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_GET, ModuleTable, REPORT_FILES_NUM))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"-------------------------------\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"%.5s\tState\t %s\n", (PS8)"Index", (PS8)"Desc");
        
            for( i = 0; i < SIZE_ARR(report_module); i++)
            {
                /* Check if there is string content (the first character is not ZERO) */
                if( report_module[i].name[0] )
                {
                    os_error_printf(CU_MSG_INFO2, (PS8)"%3d\t%c\t%s\n", report_module[i].value, (ModuleTable[i] == '1') ? '+' : ' ', report_module[i].name );
                }
            }
        }
        else
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Error reading the report table form the driver\n");
        }        
        os_error_printf(CU_MSG_INFO2, (PS8)"* Use '%d' (max index) to set all table.\n", REPORT_FILES_NUM);
    }
    else if(!CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_GET, ModuleTable, REPORT_FILES_NUM))
    {
        if (parm[0].value == REPORT_FILES_NUM)
        {
            os_memset(ModuleTable, '1', REPORT_FILES_NUM);
        }
        else if(parm[0].value < REPORT_FILES_NUM)
        {
            ModuleTable[parm[0].value] = '1';
        }
        CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_SET, ModuleTable, REPORT_FILES_NUM);
    }
}

VOID CuCmd_ClearReport(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U8 ModuleTable[REPORT_FILES_NUM];
    
    if( nParms != 1)
    {
        S32 i;    
        if (!CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_GET, ModuleTable, REPORT_FILES_NUM))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"-------------------------------\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"%.5s\tState\t %s\n", (PS8)"Index", (PS8)"Desc");
        
            for( i = 0; i < SIZE_ARR(report_module); i++)
            {
                /* Check if there is string content (the first character is not ZERO) */
                if( report_module[i].name[0] )
                {
                    os_error_printf(CU_MSG_INFO2, (PS8)"%3d\t%c\t%s\n", report_module[i].value, (ModuleTable[i] == '1') ? '+' : ' ', report_module[i].name );
                }
            }
        }
        else
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Error reading the report table form the driver\n");
        }        
        os_error_printf(CU_MSG_INFO2, (PS8)"* Use '%d' (max index) to clear all table.\n", REPORT_FILES_NUM);
    }
    else if(!CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_GET, ModuleTable, REPORT_FILES_NUM))
    {
        if (parm[0].value == REPORT_FILES_NUM)
        {
            os_memset(ModuleTable, '0', REPORT_FILES_NUM);
        }
        else if(parm[0].value < REPORT_FILES_NUM)
        {
            ModuleTable[parm[0].value] = '0';
        }
        CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_MODULE_SET, ModuleTable, REPORT_FILES_NUM);
    }
}

VOID CuCmd_ReportSeverityLevel(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U8 SeverityTable[REPORT_SEVERITY_MAX];

    /* Get the current report severity */
    if (!CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_SEVERITY_GET, SeverityTable, REPORT_SEVERITY_MAX))    
    {
        if(nParms == 0)
        {            
            S32 i;

            os_error_printf(CU_MSG_INFO2, (PS8)"Severity:\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"-------------------------------\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"%14s\tState\t%s\n", (PS8)"Severity level", (PS8)"Desc");

            for( i=1; i<SIZE_ARR(report_severity); i++ )
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"%d\t%c\t%s\n", report_severity[i].value, (SeverityTable[i] == '1') ? '+' : ' ', report_severity[i].name );
            }

            os_error_printf(CU_MSG_INFO2, (PS8)"* Use '0' to clear all table.\n");
            os_error_printf(CU_MSG_INFO2, (PS8)"* Use '%d' (max index) to set all table.\n", REPORT_SEVERITY_MAX);            
        }
        else
        {
            if (parm[0].value == 0)
            {
                /* Disable all severity levels */
                os_memset(SeverityTable, '0', sizeof(SeverityTable));
                CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_SEVERITY_SET, SeverityTable, REPORT_SEVERITY_MAX);
            }
            else if (parm[0].value == REPORT_SEVERITY_MAX)
            {
                /* Enable all severity levels */
                os_memset(SeverityTable, '1', sizeof(SeverityTable));
                CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_SEVERITY_SET, SeverityTable, REPORT_SEVERITY_MAX);
            }
            else if (parm[0].value < REPORT_SEVERITY_MAX)
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"Toggle severity level %#lx\n", parm[0].value);
                if (SeverityTable[parm[0].value] == '1')
                {
                    /* The level is enabled - Disable it */
                    SeverityTable[parm[0].value] = '0';
                }
                else
                {
                    /* The bit is disabled - Enable it */
                    SeverityTable[parm[0].value] = '1';
                }                    
                CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_REPORT_SEVERITY_SET, SeverityTable, REPORT_SEVERITY_MAX);
            }
            else
            {
                os_error_printf(CU_MSG_INFO2, (PS8)"invalid level value: %#lx\n", parm[0].value );
            }
        }
    }
    else
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error retriving the severity table from the driver\n");
    }
}

VOID CuCmd_SetReportLevelCLI(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
#if 0 /* need to create debug logic for CLI */
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S32 i, cli_debug_level;

    if(nParms)
    {
        cli_debug_level = parm[0].value;
        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY(i, cli_level_type, cli_debug_level);
        if(i == SIZE_ARR(cli_level_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetReportLevelCLI, cli_debug_level %d is not defined!\n", cli_debug_level);
            return;
        }

        g_debug_level = cli_debug_level;
        IpcEvent_UpdateDebugLevel(pCuCmd->hIpcEvent, cli_debug_level);
        os_error_printf(CU_MSG_INFO2, (PS8)"set CLI debug value = %s \n", cli_level_type[i].name);   
    }
    else
    {
        cli_debug_level = g_debug_level;
        CU_CMD_FIND_NAME_ARRAY(i, cli_level_type, cli_debug_level);
        os_error_printf(CU_MSG_INFO2, (PS8)"CLI debug value = %s (%d)\n", cli_level_type[i].name, cli_debug_level);
        print_available_values(cli_level_type);     
    }
#endif
}


char* SkipSpaces(char* str)
{
	char* tmp = str;

	while(*tmp == ' ') tmp++;
	return tmp;
}

#define ti_isdigit(c)      ('0' <= (c) && (c) <= '9')
#define ti_islower(c)      ('a' <= (c) && (c) <= 'z')
#define ti_toupper(c)      (ti_islower(c) ? ((c) - 'a' + 'A') : (c))

#define ti_isxdigit(c)   (('0' <= (c) && (c) <= '9') \
                         || ('a' <= (c) && (c) <= 'f') \
                         || ('A' <= (c) && (c) <= 'F'))

#define ti_atol(x) strtoul(x, 0)


unsigned long ti_strtoul(char *cp, char** endp, unsigned int base)
{
	unsigned long result = 0, value;
	
	if (!base) {
		  base = 10;
		  if (*cp == '0') {
				  base = 8;
				  cp++;
				  if ((ti_toupper(*cp) == 'X') && ti_isxdigit(cp[1])) {
						  cp++;
						  base = 16;
				  }
		  }
	} else if (base == 16) {
		  if (cp[0] == '0' && ti_toupper(cp[1]) == 'X')
				  cp += 2;
	}
	while (ti_isxdigit(*cp) &&
			(value = ti_isdigit(*cp) ? *cp-'0' : ti_toupper(*cp)-'A'+10) < base) {
			result = result*base + value;
			 cp++;
	}

	if(endp)
		*endp = (char *)cp;

	return result;
}


VOID CuCmd_FwDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
	U32 *buf_ptr, *pbuf;
	char *pstr = (char *)parm[0].value;
	U32 parm_length;

	os_error_printf(CU_MSG_INFO2, (PS8)"FwDebug parm: %s\n", parm[0].value);

	buf_ptr = (U32*)os_MemoryCAlloc(252, sizeof(U32));
	if(!buf_ptr)
		return;

	pbuf = buf_ptr + 2;

	pstr = SkipSpaces(pstr);
	while(*pstr) {
		*pbuf++ =  ti_strtoul(pstr, &pstr, 0);
		pstr = SkipSpaces(pstr);
	}

	parm_length = (U32)((U8*)pbuf-(U8*)buf_ptr);

	os_error_printf(CU_MSG_INFO2, (PS8)"Parms buf size = %d\n", parm_length);

	*buf_ptr = 2210;
	*(buf_ptr+1) = parm_length - 2*sizeof(U32);

	CuCommon_PrintDriverDebug(pCuCmd->hCuCommon, (PVOID)buf_ptr, parm_length);

	os_MemoryFree(buf_ptr);

}

VOID CuCmd_SetRateMngDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    RateMangeParams_t     RateParams;

	RateParams.paramIndex = (TI_UINT8)parm[0].value;


    if( nParms == 2 )
    {
		switch (RateParams.paramIndex)
		{
		case RATE_MGMT_RETRY_SCORE_PARAM:
			RateParams.RateRetryScore = (TI_UINT16)parm[1].value;
			break;
		case RATE_MGMT_PER_ADD_PARAM:
			RateParams.PerAdd = (TI_UINT16)parm[1].value;
			break;
		case RATE_MGMT_PER_TH1_PARAM:
			RateParams.PerTh1 = (TI_UINT16)parm[1].value;
			break;
		case RATE_MGMT_PER_TH2_PARAM:
			RateParams.PerTh2 = (TI_UINT16)parm[1].value;
			break;
		case RATE_MGMT_MAX_PER_PARAM:
			RateParams.MaxPer = (TI_UINT16)parm[1].value;
			break;
		case RATE_MGMT_INVERSE_CURIOSITY_FACTOR_PARAM:
			RateParams.InverseCuriosityFactor = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_TX_FAIL_LOW_TH_PARAM:
			RateParams.TxFailLowTh = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_TX_FAIL_HIGH_TH_PARAM:
			RateParams.TxFailHighTh = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_PER_ALPHA_SHIFT_PARAM:
			RateParams.PerAlphaShift = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_PER_ADD_SHIFT_PARAM:
			RateParams.PerAddShift = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_PER_BETA1_SHIFT_PARAM:
			RateParams.PerBeta1Shift = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_PER_BETA2_SHIFT_PARAM:
			RateParams.PerBeta2Shift = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_RATE_CHECK_UP_PARAM:
			RateParams.RateCheckUp = (TI_UINT8)parm[1].value;
			break;
		case RATE_MGMT_RATE_CHECK_DOWN_PARAM:
			RateParams.RateCheckDown = (TI_UINT8)parm[1].value;
			break;
		default:
			os_error_printf(CU_MSG_INFO2,"Error: index is not valid! \n");
			return;

		}
	}
	else if ((nParms == NUM_OF_RATE_MNGT_PARAMS_MAX) && (parm[0].value == RATE_MGMT_RATE_RETRY_POLICY_PARAM ))
	{
		int i=0;
		for (i=1; i < NUM_OF_RATE_MNGT_PARAMS_MAX; i++)
		{
			RateParams.RateRetryPolicy[i-1] = (TI_UINT8)parm[i].value;
		}
    }
    else
    {
           os_error_printf(CU_MSG_INFO2,"(0)  RateMngRateRetryScore \n");
           os_error_printf(CU_MSG_INFO2,"(1)  RateMngPerAdd \n");
           os_error_printf(CU_MSG_INFO2,"(2)  RateMngPerTh1 \n");
           os_error_printf(CU_MSG_INFO2,"(3)  RateMngPerTh2 \n");
		   os_error_printf(CU_MSG_INFO2,"(4)  RateMngMaxPer \n");
           os_error_printf(CU_MSG_INFO2,"(5)  RateMngInverseCuriosityFactor \n");
           os_error_printf(CU_MSG_INFO2,"(6)  RateMngTxFailLowTh \n");
		   os_error_printf(CU_MSG_INFO2,"(7)  RateMngTxFailHighTh \n");
           os_error_printf(CU_MSG_INFO2,"(8)  RateMngPerAlphaShift \n");
           os_error_printf(CU_MSG_INFO2,"(9)  RateMngPerAddShift \n");
           os_error_printf(CU_MSG_INFO2,"(10) RateMngPerBeta1Shift \n");
           os_error_printf(CU_MSG_INFO2,"(11) RateMngPerBeta2Shift \n");
           os_error_printf(CU_MSG_INFO2,"(12) RateMngRateCheckUp \n");
		   os_error_printf(CU_MSG_INFO2,"(13) RateMngRateCheckDown \n");
		   os_error_printf(CU_MSG_INFO2,"(14) RateMngRateRetryPolicy[13] \n");
		   return;
    }

	CuCommon_SetBuffer(pCuCmd->hCuCommon, TIWLN_RATE_MNG_SET,&RateParams, sizeof(RateMangeParams_t));
}

VOID CuCmd_GetRateMngDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    AcxRateMangeParams ReadRateParams;
	int i;

    os_memset(&ReadRateParams,0,sizeof(AcxRateMangeParams));

    CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_RATE_MNG_GET, &ReadRateParams, sizeof(AcxRateMangeParams));

	if (0 == nParms)
	{
		parm[0].value =  RATE_MGMT_ALL_PARAMS;
	}

	 switch (parm[0].value)
		{
		case RATE_MGMT_RETRY_SCORE_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngRateRetryScore = %d \n", ReadRateParams.RateRetryScore);
			break;
		case RATE_MGMT_PER_ADD_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngPerAdd = %d\n" , ReadRateParams.PerAdd);
			break;
		case RATE_MGMT_PER_TH1_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngPerTh1 = %d\n" , ReadRateParams.PerTh1);
			break;
		case RATE_MGMT_PER_TH2_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngPerTh2 = %d\n" , ReadRateParams.PerTh2);
			break;
		case RATE_MGMT_MAX_PER_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngMaxPer = %d\n" , ReadRateParams.MaxPer);
			break;
		case RATE_MGMT_INVERSE_CURIOSITY_FACTOR_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngInverseCuriosityFactor = %d \n" , ReadRateParams.InverseCuriosityFactor);
			break;
		case RATE_MGMT_TX_FAIL_LOW_TH_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngTxFailLowTh = %d\n" , ReadRateParams.TxFailLowTh);
			break;
		case RATE_MGMT_TX_FAIL_HIGH_TH_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngTxFailHighTh = %d\n" , ReadRateParams.TxFailHighTh);
			break;
		case RATE_MGMT_PER_ALPHA_SHIFT_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngPerAlphaShift = %d\n" , ReadRateParams.PerAlphaShift);
			break;
		case RATE_MGMT_PER_ADD_SHIFT_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngPerAddShift = %d\n" , ReadRateParams.PerAddShift);
			break;
		case RATE_MGMT_PER_BETA1_SHIFT_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngPerBeta1Shift = %d\n" , ReadRateParams.PerBeta1Shift);
			break;
		case RATE_MGMT_PER_BETA2_SHIFT_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngPerBeta2Shift = %d\n" , ReadRateParams.PerBeta2Shift);
			break;
		case RATE_MGMT_RATE_CHECK_UP_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngRateCheckUp = %d\n" , ReadRateParams.RateCheckUp);
			break;
		case RATE_MGMT_RATE_CHECK_DOWN_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngRateCheckDown = %d\n" , ReadRateParams.RateCheckDown);
			break;
	    case RATE_MGMT_RATE_RETRY_POLICY_PARAM:
			os_error_printf(CU_MSG_INFO2,"RateMngRateRetryPolicy = ");

			for (i=0 ; i< RATE_MNG_MAX_RETRY_POLICY_PARAMS_LEN ; i++)
			{
				os_error_printf(CU_MSG_INFO2,"%d ",ReadRateParams.RateRetryPolicy[i]);
			}

			os_error_printf(CU_MSG_INFO2,"\n");

			break;

	    case RATE_MGMT_ALL_PARAMS:
		   os_error_printf(CU_MSG_INFO2,"RateMngRateRetryScore = %d \n", ReadRateParams.RateRetryScore);
           os_error_printf(CU_MSG_INFO2,"RateMngPerAdd = %d\n" , ReadRateParams.PerAdd);
           os_error_printf(CU_MSG_INFO2,"RateMngPerTh1 = %d\n" , ReadRateParams.PerTh1);
           os_error_printf(CU_MSG_INFO2,"RateMngPerTh2 = %d\n" , ReadRateParams.PerTh2);
		   os_error_printf(CU_MSG_INFO2,"RateMngMaxPer = %d\n" , ReadRateParams.MaxPer);
           os_error_printf(CU_MSG_INFO2,"RateMngInverseCuriosityFactor = %d \n" , ReadRateParams.InverseCuriosityFactor);
           os_error_printf(CU_MSG_INFO2,"RateMngTxFailLowTh = %d\n" , ReadRateParams.TxFailLowTh);
		   os_error_printf(CU_MSG_INFO2,"RateMngTxFailHighTh = %d\n" , ReadRateParams.TxFailHighTh);
           os_error_printf(CU_MSG_INFO2,"RateMngPerAlphaShift = %d\n" , ReadRateParams.PerAlphaShift);
           os_error_printf(CU_MSG_INFO2,"RateMngPerAddShift = %d\n" , ReadRateParams.PerAddShift);
           os_error_printf(CU_MSG_INFO2,"RateMngPerBeta1Shift = %d\n" , ReadRateParams.PerBeta1Shift);
           os_error_printf(CU_MSG_INFO2,"RateMngPerBeta2Shift = %d\n" , ReadRateParams.PerBeta2Shift);
           os_error_printf(CU_MSG_INFO2,"RateMngRateCheckUp = %d\n" , ReadRateParams.RateCheckUp);
		   os_error_printf(CU_MSG_INFO2,"RateMngRateCheckDown = %d\n" , ReadRateParams.RateCheckDown);
		   os_error_printf(CU_MSG_INFO2,"RateMngRateRetryPolicy = ");

			for (i=0 ; i< RATE_MNG_MAX_RETRY_POLICY_PARAMS_LEN ; i++)
			{
				os_error_printf(CU_MSG_INFO2,"%d ",ReadRateParams.RateRetryPolicy[i]);
			}
			os_error_printf(CU_MSG_INFO2,"\n");
		 break;

		default:
			os_error_printf(CU_MSG_INFO2,"Error: index is not valid! \n");
			return;
	 }

}


VOID CuCmd_PrintDriverDebug(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
	U32 size		= 0;
	TTwdDebug data;

	/* check if nParam is invalid */
	if (( nParms == 0 ) || ( nParms > 4 ))
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_PrintDriverDebug: Invalid number of Parameters %d\n", nParms);
		return;
	}

	/* init */
    os_memset( &data.debug_data.mem_debug.UBuf.buf8, 0, sizeof(data.debug_data.mem_debug.UBuf.buf8) );
	data.func_id 						= parm[0].value;
	size								= sizeof(data.func_id);

	os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_PrintDriverDebug: FUN_ID: %u\n", data.func_id);

	/* if R reg request - read data */
    if ( nParms == 2 )
	{
        data.debug_data.opt_param = 4;
		data.debug_data.opt_param = parm[1].value;
		size += sizeof(data.debug_data.opt_param);
	}
    else
	/* if W reg request - read data */
	if ( nParms > 2 )
	{
        data.debug_data.mem_debug.addr 		= 0;

        data.debug_data.mem_debug.length 	= 4;
        size += sizeof(data.debug_data.mem_debug.length);

        data.debug_data.mem_debug.addr = parm[1].value;
        size += sizeof(data.debug_data.mem_debug.addr);

        data.debug_data.mem_debug.UBuf.buf32[0] = parm[2].value;
        size += sizeof(data.debug_data.mem_debug.UBuf.buf32[0]);

		os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_PrintDriverDebug: addr: 0x%x\n", data.debug_data.opt_param);
        os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_PrintDriverDebug: data: 0x%x\n", data.debug_data.mem_debug.UBuf.buf32[0]);
	}
	CuCommon_PrintDriverDebug(pCuCmd->hCuCommon, (PVOID)&data, size);
}

VOID CuCmd_PrintDriverDebugBuffer(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    U32 func_id = ( nParms > 0 ) ? parm[0].value : 0;
    U32 opt_param = ( nParms > 1 ) ? parm[1].value : 0;
    
    os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_PrintDriverDebugBuffer: FUNC:%u, PARAM:%u\n", func_id, opt_param);

    CuCommon_PrintDriverDebugBuffer(pCuCmd->hCuCommon, func_id, opt_param);
}

/*-------------------*/
/* Radio Debug Tests */
/*-------------------*/
/* Set the RX channel --> Radio Tune */
VOID CuCmd_RadioDebug_ChannelTune(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TPowerMgr_PowerMode Mode;
    TTestCmd data;
    
    if ((nParms == 0) || (nParms > 2))
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 0 - Band (0-2.4Ghz, 1-5Ghz, 2-4.9Ghz)\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 1 - Channel\n");
    }
    else
    {
        if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, TIWLN_802_11_POWER_MODE_GET,
            &Mode, sizeof(TPowerMgr_PowerMode))) return;    
        if(Mode.PowerMode != OS_POWER_MODE_ACTIVE)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"Radio tune was not performed becouse Default power-mode is not ACTIVE\n");
        }
        else
        {
			os_memset(&data, 0, sizeof(TTestCmd));
			data.testCmdId 						= TEST_CMD_CHANNEL_TUNE;
			data.testCmd_u.Channel.iChannel 	= (U8)parm[1].value;
			data.testCmd_u.Channel.iBand 		= (U8)parm[0].value;

			if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data))
			{
				os_error_printf(CU_MSG_INFO2, (PS8)"Channel %d tune failed\n",data.testCmd_u.Channel.iChannel);            
				return;
			}
            os_error_printf(CU_MSG_INFO2, (PS8)"Channel tune of channel %d was performed OK\n",(U8)data.testCmd_u.Channel.iChannel);            
        }
    }
}

/* Start CW test (TELEC) */
VOID CuCmd_RadioDebug_StartTxCw(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
	TTestCmd data;

	/* check # of params OK */
    if ((nParms == 0) || (nParms > 2))
    {
		/* print help */
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 0  - Power (0-25000 1/1000 db)\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 1  - Tone Type (1- Carrier Feed Through, 2- Single Tone)\n");

/*        os_error_printf(CU_MSG_INFO2, (PS8)"Param 2  - Band\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 3  - Channel\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 4  - PPA Step\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 5  - Tone no. Single Tones\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 6  - Tone no. Two Tones\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 7  - Use digital DC\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 8  - Invert\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 9  - Eleven N Span\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 10 - Digital DC\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 11 - Analog DC Fine\n");     
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 12 - Analog DC Course\n");*/
    }
    else
    {
		os_memset(&data, 0, sizeof(TTestCmd));
		data.testCmdId 										= TEST_CMD_TELEC;
        data.testCmd_u.TxToneParams.iPower 					= (U16)parm[0].value;
        data.testCmd_u.TxToneParams.iToneType 				= (U8)parm[1].value;
/*		data.testCmd_u.TxToneParams.iPpaStep 				= (U8)parm[4].value;
		data.testCmd_u.TxToneParams.iToneNumberSingleTones 	= (U8)parm[5].value;
		data.testCmd_u.TxToneParams.iToneNumberTwoTones 	= (U8)parm[6].value;
		data.testCmd_u.TxToneParams.iUseDigitalDC 			= (U8)parm[7].value;
		data.testCmd_u.TxToneParams.iInvert 				= (U8)parm[8].value;
		data.testCmd_u.TxToneParams.iElevenNSpan 			= (U8)parm[9].value;
		data.testCmd_u.TxToneParams.iDigitalDC 				= (U8)parm[10].value;
		data.testCmd_u.TxToneParams.iAnalogDCFine 			= (U8)parm[11].value;
		data.testCmd_u.TxToneParams.iAnalogDCCoarse 		= (U8)parm[12].value;*/

		if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data))
		{
			os_error_printf(CU_MSG_INFO2, (PS8)"CW test failed\n");                   
			return;
		}
        os_error_printf(CU_MSG_INFO2, (PS8)"CW test was performed OK\n");                   
    }
}

/* Start TX continues test (FCC) */
VOID CuCmd_RadioDebug_StartContinuousTx(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    TMacAddr mac_addr_mask 	= { 0xff,0xff,0xff,0xff,0xff,0xff };
	CuCmd_t* pCuCmd 		= (CuCmd_t*)hCuCmd;
	TTestCmd data;

    if ((nParms == 0) || (nParms > 15))
    {
		/* print help */
        os_error_printf(CU_MSG_INFO2, (PS8)"\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 0 - Delay\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 1 - Rate\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 2  - Size\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 3  - Amount\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 4  - Power\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 5  - Seed\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 6  - Packet Mode\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 7  - DCF On/Off\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 8  - GI\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 9  - Preamble\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 10 - Type\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 11 - Scrambler\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 12 - Enable CLPC\n");		
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 13 - Sequance no. Mode\n");		
        /* future use. for now the oregenal source address are use.
        os_error_printf(CU_MSG_INFO2, (PS8)"Param 14 - Source MAC Address\n");		
        */
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 14 - Destination MAC Address\n");		
    }
    else
    {
		os_memset(&data, 0, sizeof(TTestCmd));
		data.testCmdId = TEST_CMD_FCC;
		data.testCmd_u.TxPacketParams.iDelay 			= (U32)parm[0].value;
		data.testCmd_u.TxPacketParams.iRate 			= (U32)parm[1].value;
		data.testCmd_u.TxPacketParams.iSize 			= (U16)parm[2].value;
		data.testCmd_u.TxPacketParams.iAmount 			= (U16)parm[3].value;
		data.testCmd_u.TxPacketParams.iPower 			= (U16)parm[4].value;
		data.testCmd_u.TxPacketParams.iSeed 			= (U16)parm[5].value;
		data.testCmd_u.TxPacketParams.iPacketMode 		= (U8)parm[6].value;
		data.testCmd_u.TxPacketParams.iDcfOnOff 		= (U8)parm[7].value;
		data.testCmd_u.TxPacketParams.iGI 				= (U8)parm[8].value;
		data.testCmd_u.TxPacketParams.iPreamble 		= (U8)parm[9].value;
		data.testCmd_u.TxPacketParams.iType 			= (U8)parm[10].value;
		data.testCmd_u.TxPacketParams.iScrambler 		= (U8)parm[11].value;
		data.testCmd_u.TxPacketParams.iEnableCLPC 		= (U8)parm[12].value;
		data.testCmd_u.TxPacketParams.iSeqNumMode 		= (U8)parm[13].value;
        /* future use. for now the oregenal source address are use.
        if(!CuCmd_Str2MACAddr((PS8)parm[16].value, (PU8)mac_addr_mask) )
		{
			os_error_printf(CU_MSG_INFO2, (PS8)"Continuous Tx start has failed to read source MAC Address \n");
			return;
		}
        */
		os_memcpy((PVOID)data.testCmd_u.TxPacketParams.iSrcMacAddr, 
				  (PVOID)mac_addr_mask, 
				  sizeof(mac_addr_mask));
		if(!CuCmd_Str2MACAddr((PS8)parm[14].value, (PU8)mac_addr_mask) )
		{
			os_error_printf(CU_MSG_INFO2, (PS8)"Continuous Tx start has failed to read destination MAC Address \n");
			return;
		}
		os_memcpy((PVOID)data.testCmd_u.TxPacketParams.iDstMacAddr, 
				  (PVOID)mac_addr_mask, 
				  sizeof(mac_addr_mask));

        if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data))
		{
			os_error_printf(CU_MSG_INFO2, (PS8)"Continuous Tx start has failed\n");
			return;
		}
        os_error_printf(CU_MSG_INFO2, (PS8)"Continuous Tx started OK\n");
   }
}

/* Stop FCC/TELEC (Radio Debug) */
VOID CuCmd_RadioDebug_StopTx(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;

    os_memset(&data, 0, sizeof(TTestCmd));
    data.testCmdId = TEST_CMD_STOP_TX;

	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data))
	{
        os_error_printf(CU_MSG_INFO2, (PS8)"Plt Tx Stop has failed\n");
		return; 
	}
	os_error_printf(CU_MSG_INFO2, (PS8)"Plt Tx Stop was OK\n");
}

/* download packet template for transmissions 
	the template shall be set before calling TX Debug */
VOID CuCmd_RadioDebug_Template(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;

    if ((nParms == 0) || (nParms > 3))
    {
		/* print help */
        os_error_printf(CU_MSG_INFO2, (PS8)"\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 1 	- Buffer Offset\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Param 2 	- Buffer Data\n");
	}
	else
	{
		os_memset(&data, 0, sizeof(TTestCmd));
		data.testCmdId 									= TEST_CMD_PLT_TEMPLATE;
		data.testCmd_u.TxTemplateParams.bufferOffset 	= (U16)parm[0].value;
		data.testCmd_u.TxTemplateParams.bufferLength	= (U16)os_strlen((PS8)parm[1].value);
		/* check that length is valid */
		if( data.testCmd_u.TxTemplateParams.bufferOffset + data.testCmd_u.TxTemplateParams.bufferLength > TX_TEMPLATE_MAX_BUF_LEN )
		{
			os_error_printf(CU_MSG_INFO2, (PS8)"Plt Template has failed because of invalid buffer length\n");
			return; 
		}
		/* convert the string to hexadeciaml values, and copy it */
		CuCmd_atox_string ((U8*)parm[1].value,(U8*)data.testCmd_u.TxTemplateParams.buffer);

		if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data))
		{
			os_error_printf(CU_MSG_INFO2, (PS8)"Plt Template has failed\n");
			return; 
		}
		os_error_printf(CU_MSG_INFO2, (PS8)"Plt Template was OK\n");
	}
}


/* Start RX Statistics */
VOID CuCmd_RadioDebug_StartRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;

    os_memset(&data, 0, sizeof(TTestCmd));
    data.testCmdId = TEST_CMD_RX_STAT_START;

	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
    {
		os_error_printf(CU_MSG_INFO2, (PS8)"Start RX Statistics has failed\n");   
		return;
    }
	os_error_printf(CU_MSG_INFO2, (PS8)"Start RX Statistics OK\n");   
}

/* Stop RX Statistics */
VOID CuCmd_RadioDebug_StopRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;

    os_memset(&data, 0, sizeof(TTestCmd));
    data.testCmdId = TEST_CMD_RX_STAT_STOP;

	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
    {
		os_error_printf(CU_MSG_INFO2, (PS8)"Stop RX Statistics has failed\n");   
		return;
    }
	os_error_printf(CU_MSG_INFO2, (PS8)"Stop RX Statistics OK\n");   
}

/* Reset RX Statistics */
VOID CuCmd_RadioDebug_ResetRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;

    os_memset(&data, 0, sizeof(TTestCmd));
    data.testCmdId = TEST_CMD_RX_STAT_RESET;

	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
    {
		os_error_printf(CU_MSG_INFO2, (PS8)"Reset RX Statistics has failed\n");   
		return;
    }
	os_error_printf(CU_MSG_INFO2, (PS8)"Reset RX Statistics OK\n");   
}


/* Get HDK Version*/
VOID CuCmd_RadioDebug_GetHDKVersion(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
	TTestCmd data;

	os_memset(&data, 0, sizeof(TTestCmd));

	data.testCmdId = TEST_CMD_GET_FW_VERSIONS;
	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"Get FW version function has failed\n");   
		return;
	}
	os_error_printf(CU_MSG_INFO2, (PS8)"ProductName:                       %d\n", data.testCmd_u.fwVersions.hdkVersion.ProductName);
	os_error_printf(CU_MSG_INFO2, (PS8)"PgNumber:                          %d\n", data.testCmd_u.fwVersions.hdkVersion.PgNumber);
	os_error_printf(CU_MSG_INFO2, (PS8)"SoftwareVersionLevel:              %d\n", data.testCmd_u.fwVersions.hdkVersion.SoftwareVersionLevel);
	os_error_printf(CU_MSG_INFO2, (PS8)"radioModuleType:                   %d\n", data.testCmd_u.fwVersions.hdkVersion.radioModuleType);
	os_error_printf(CU_MSG_INFO2, (PS8)"SoftwareVersionDelivery:           %d\n", data.testCmd_u.fwVersions.hdkVersion.SoftwareVersionDelivery);
	os_error_printf(CU_MSG_INFO2, (PS8)"numberOfReferenceDesignsSupported: %d\n", data.testCmd_u.fwVersions.hdkVersion.numberOfReferenceDesignsSupported);
#ifdef FIX_HDK_VERSION_API /* HDK version struct should be changed aligned and without pointer */
	os_error_printf(CU_MSG_INFO2, (PS8)"referenceDesignsSupported->referenceDesignId: %d\n",    data.testCmd_u.fwVersions.hdkVersion.referenceDesignsSupported->referenceDesignId);
	os_error_printf(CU_MSG_INFO2, (PS8)"referenceDesignsSupported->nvsMajorVersion: %d\n",      data.testCmd_u.fwVersions.hdkVersion.referenceDesignsSupported->nvsMajorVersion);
	os_error_printf(CU_MSG_INFO2, (PS8)"referenceDesignsSupported->nvsMinorVersion: %d\n",      data.testCmd_u.fwVersions.hdkVersion.referenceDesignsSupported->nvsMinorVersion);
	os_error_printf(CU_MSG_INFO2, (PS8)"referenceDesignsSupported->nvsMinorMinorVersion: %d\n", data.testCmd_u.fwVersions.hdkVersion.referenceDesignsSupported->nvsMinorMinorVersion);
#endif
}

/* Get RX Statistics */
VOID CuCmd_RadioDebug_GetRxStatistics(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
#if 0 /*Temp: currently not supported*/
	U32 i 			= 0;
#endif

    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;

    os_memset(&data, 0, sizeof(TTestCmd));
    data.testCmdId = TEST_CMD_RX_STAT_GET;

	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
    {
		os_error_printf(CU_MSG_INFO2, (PS8)"Get RX Statistics has failed\n");   
		return;
    }
	/* print Statistics Got */
	os_error_printf(CU_MSG_INFO2, (PS8)"\n");
	os_error_printf(CU_MSG_INFO2, (PS8)"Received Valid Packet no.: %d(0x%x)\n", data.testCmd_u.Statistics.oRxPathStatistics.ReceivedValidPacketsNumber,data.testCmd_u.Statistics.oRxPathStatistics.ReceivedValidPacketsNumber);
	os_error_printf(CU_MSG_INFO2, (PS8)"Received FCS Error Packet no.: %d(0x%x)\n", data.testCmd_u.Statistics.oRxPathStatistics.ReceivedFcsErrorPacketsNumber,data.testCmd_u.Statistics.oRxPathStatistics.ReceivedFcsErrorPacketsNumber);
	os_error_printf(CU_MSG_INFO2, (PS8)"Received Address mismatched packet: %d(0x%x)\n", data.testCmd_u.Statistics.oRxPathStatistics.ReceivedPlcpErrorPacketsNumber,data.testCmd_u.Statistics.oRxPathStatistics.ReceivedPlcpErrorPacketsNumber);
	os_error_printf(CU_MSG_INFO2, (PS8)"Sequance Nomber Missing Count: %d(0x%x)\n", data.testCmd_u.Statistics.oRxPathStatistics.SeqNumMissCount,data.testCmd_u.Statistics.oRxPathStatistics.SeqNumMissCount);
	/* The RSSI and SNR are in octal units, the value divided by 8 for the print */  
	os_error_printf(CU_MSG_INFO2, (PS8)"Average SNR: %d(0x%x)\n", data.testCmd_u.Statistics.oRxPathStatistics.AverageSnr/8,data.testCmd_u.Statistics.oRxPathStatistics.AverageSnr/8);
	os_error_printf(CU_MSG_INFO2, (PS8)"Average RSSI: %d(0x%x)\n", (data.testCmd_u.Statistics.oRxPathStatistics.AverageRssi)/8,(data.testCmd_u.Statistics.oRxPathStatistics.AverageRssi)/8);
	os_error_printf(CU_MSG_INFO2, (PS8)"Base Packet ID: %d(0x%x)\n", data.testCmd_u.Statistics.oBasePacketId,data.testCmd_u.Statistics.oBasePacketId);
	os_error_printf(CU_MSG_INFO2, (PS8)"Number of Packets: %d(0x%x)\n", data.testCmd_u.Statistics.ioNumberOfPackets,data.testCmd_u.Statistics.ioNumberOfPackets);
	os_error_printf(CU_MSG_INFO2, (PS8)"Number of Missed Packets: %d(0x%x)\n", data.testCmd_u.Statistics.oNumberOfMissedPackets,data.testCmd_u.Statistics.oNumberOfMissedPackets);
#if 0/*Temp: currently not supported*/
	for ( i = 0 ; i < RX_STAT_PACKETS_PER_MESSAGE ; i++ )
	{ 
		os_error_printf(CU_MSG_INFO2, (PS8)"RX Packet %d Statistics\n",i);
		os_error_printf(CU_MSG_INFO2, (PS8)"Length: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].Length,data.testCmd_u.Statistics.RxPacketStatistics[i].Length);
		os_error_printf(CU_MSG_INFO2, (PS8)"EVM: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].EVM,data.testCmd_u.Statistics.RxPacketStatistics[i].EVM);
		os_error_printf(CU_MSG_INFO2, (PS8)"RSSI: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].RSSI,data.testCmd_u.Statistics.RxPacketStatistics[i].RSSI);
		os_error_printf(CU_MSG_INFO2, (PS8)"Frequency Delta: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].FrequencyDelta,data.testCmd_u.Statistics.RxPacketStatistics[i].FrequencyDelta);
		os_error_printf(CU_MSG_INFO2, (PS8)"Flags: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].Flags,data.testCmd_u.Statistics.RxPacketStatistics[i].Flags);
		os_error_printf(CU_MSG_INFO2, (PS8)"Type: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].Type,data.testCmd_u.Statistics.RxPacketStatistics[i].Type);
		os_error_printf(CU_MSG_INFO2, (PS8)"Rate: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].Rate,data.testCmd_u.Statistics.RxPacketStatistics[i].Rate);
		os_error_printf(CU_MSG_INFO2, (PS8)"Noise: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].Noise,data.testCmd_u.Statistics.RxPacketStatistics[i].Noise);
		os_error_printf(CU_MSG_INFO2, (PS8)"AGC Gain: %d(0x%x)\n", data.testCmd_u.Statistics.RxPacketStatistics[i].AgcGain,data.testCmd_u.Statistics.RxPacketStatistics[i].AgcGain);
	}
#endif
}


/*-----------*/
/* BIP Tests */
/*-----------*/


void nvsFillMACAddress(THandle hCuCmd, FILE *nvsBinFile)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
	TMacAddr Mac;
	U8	lengthToSet;
	U8	addressHigher;
	U8	addressLower;
	U8	valueToSet=0;

	lengthToSet = 0x1;
	

	os_error_printf(CU_MSG_INFO2, (PS8)"Entering FillMACAddressToNVS\n");
	/* param 0 in nvs*/
	os_fwrite(&lengthToSet, sizeof(U8), 1, nvsBinFile);
	
	/* register for MAC Address*/
	addressHigher	= 0x6D;
	addressLower	= 0x54;
	
	/* param 1 in nvs*/
	os_fwrite(&addressHigher, sizeof(U8), 1, nvsBinFile);
	/* param 2 in nvs*/
	os_fwrite(&addressLower, sizeof(U8), 1, nvsBinFile);
	

   /*	read mac address */
	if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, CTRL_DATA_MAC_ADDRESS, Mac, sizeof(TMacAddr))) 
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"Unable  to get Mac address, aborting\n");
		return;
	}
	os_error_printf(CU_MSG_INFO2, (PS8)"Mac[0]=%02x\n", Mac[0]);
	os_error_printf(CU_MSG_INFO2, (PS8)"Mac[1]=%02x\n", Mac[1]);
	os_error_printf(CU_MSG_INFO2, (PS8)"Mac[2]=%02x\n", Mac[2]);
	os_error_printf(CU_MSG_INFO2, (PS8)"Mac[3]=%02x\n", Mac[3]);
	os_error_printf(CU_MSG_INFO2, (PS8)"Mac[4]=%02x\n", Mac[4]);
	os_error_printf(CU_MSG_INFO2, (PS8)"Mac[5]=%02x\n", Mac[5]);

	/* write the lower MAC address starting from the LSB
	   params 3-6 in NVS*/
	os_fwrite(&Mac[5], sizeof(U8), 1, nvsBinFile);
	os_fwrite(&Mac[4], sizeof(U8), 1, nvsBinFile);
	os_fwrite(&Mac[3], sizeof(U8), 1, nvsBinFile);
	os_fwrite(&Mac[2], sizeof(U8), 1, nvsBinFile);

    /* param 7 in NVS*/
	os_fwrite(&lengthToSet, sizeof(U8), 1, nvsBinFile);
	
	addressHigher	= 0x71;
	addressLower	= 0x54;

	/* params 8-9 in NVS*/
	os_fwrite(&addressHigher, sizeof(U8), 1, nvsBinFile);
	os_fwrite(&addressLower, sizeof(U8), 1, nvsBinFile);

	
    /* Write the higher MAC address starting from the LSB
	   params 10-13 in NVS*/
	os_fwrite(&Mac[1], sizeof(U8), 1, nvsBinFile);
	os_fwrite(&Mac[0], sizeof(U8), 1, nvsBinFile);

    os_fwrite(&valueToSet, sizeof(U8), 1, nvsBinFile);
	os_fwrite(&valueToSet, sizeof(U8), 1, nvsBinFile);

    os_error_printf(CU_MSG_INFO2, (PS8)"exiting FillMACAddressToNVS\n");
}

TI_BOOL nvsReadFile(TI_UINT8 *pReadBuffer, TI_UINT16 *length, char* nvsFilePath)
{
	FILE      *nvsBinFile = NULL;
	TI_UINT8  nvsData;
	TI_INT8   nvsFileValid = TRUE;
	TI_UINT32 index =0;

	if (NULL == (nvsBinFile = os_fopen (nvsFilePath, OS_FOPEN_READ_BINARY)))
    {
       nvsFileValid = FALSE; 
	   return TI_FALSE;
    }

	do 
	{
		os_fread(&nvsData, sizeof(TI_UINT8), 1, nvsBinFile);
		pReadBuffer[index++] = nvsData;
	} while((!feof(nvsBinFile)) && (index < NVS_TOTAL_LENGTH)) ;

	*length = index;
	os_fclose(nvsBinFile);

	return TI_TRUE;
}


VOID nvsParsePreviosOne(const uint8 *nvsBuffer, TNvsStruct *nvsTypeTLV, uint32 *nvsVersion)
{
#define BUFFER_INDEX	bufferIndex + START_PARAM_INDEX + infoModeIndex

	uint16		bufferIndex;
	uint8		tlvType;
	uint16		tlvLength;
	uint16		infoModeIndex;
	NVSTypeInfo	nvsTypeInfo;
	uint8		nvsVersionOctetIndex;
	uint8		shift;

	for (bufferIndex = 0; bufferIndex < NVS_TOTAL_LENGTH;)
	{
		tlvType = nvsBuffer[bufferIndex];
		
		/* fill the correct mode to fill the NVS struct buffer */
		/* if the tlvType is the last type break from the loop */
		switch(tlvType)
		{
			case eNVS_RADIO_TX_PARAMETERS:
				nvsTypeInfo = eNVS_RADIO_TX_TYPE_PARAMETERS_INFO;
				break;

			case eNVS_RADIO_RX_PARAMETERS:
				nvsTypeInfo = eNVS_RADIO_RX_TYPE_PARAMETERS_INFO;
				break;

			case eNVS_VERSION:
				for (*nvsVersion = 0, nvsVersionOctetIndex = 0; nvsVersionOctetIndex < NVS_VERSION_PARAMETER_LENGTH; nvsVersionOctetIndex++)
				{
					shift = 8 * (NVS_VERSION_PARAMETER_LENGTH - 1 - nvsVersionOctetIndex);
					*nvsVersion += ((nvsBuffer[bufferIndex + START_PARAM_INDEX + nvsVersionOctetIndex]) << shift);	
				}				
				break;
			
			case eTLV_LAST:
			default:
				return;
		}

		tlvLength = (nvsBuffer[bufferIndex + START_LENGTH_INDEX  + 1] << 8) + nvsBuffer[bufferIndex + START_LENGTH_INDEX];

		/* if TLV type is not NVS version fill the NVS structure according to the mode TX/RX */
		if ((eNVS_RADIO_TX_PARAMETERS == tlvType) || (eNVS_RADIO_RX_PARAMETERS == tlvType)) 
		{
			nvsTypeTLV[nvsTypeInfo].Type = tlvType;
			nvsTypeTLV[nvsTypeInfo].Length = tlvLength;

			for (infoModeIndex = 0; (infoModeIndex < tlvLength) && (BUFFER_INDEX < NVS_TOTAL_LENGTH); infoModeIndex++)
			{
				nvsTypeTLV[nvsTypeInfo].Buffer[infoModeIndex] = nvsBuffer[BUFFER_INDEX];			
			}

		}

		/* increment to the next TLV */
		bufferIndex += START_PARAM_INDEX + tlvLength;
	}
}


VOID nvsFillOldRxParams(FILE *nvsBinFile, const TI_UINT8 *buffer, const uint16 rxLength)
{
	TI_UINT16 	index;
	TI_UINT8	rxTypeValue;
	TI_UINT8	valueToSet;

	/* RX BiP type */
	rxTypeValue = eNVS_RADIO_RX_PARAMETERS;
	fwrite(&rxTypeValue, sizeof(TI_UINT8), 1, nvsBinFile);

	/* RX BIP Length */
	fwrite(&rxLength, sizeof(TI_UINT16), 1, nvsBinFile);

	for (index = 0; index < rxLength; index++)
	{
		valueToSet = buffer[index];
		fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);
	}	
}

VOID nvsFillTXParams(FILE *nvsBinFile, TI_UINT8	*nvsPtr, TI_UINT16 txParamLength)
{
	TI_UINT8			txParamValue;
	TI_UINT8			valueToSet;
	TI_UINT16			index;

	/* TX BiP type */
	txParamValue = eNVS_RADIO_TX_PARAMETERS;
	os_fwrite(&txParamValue, sizeof(TI_UINT8), 1, nvsBinFile);

	/* TX Bip Length */
	os_fwrite(&txParamLength, sizeof(TI_UINT16), 1, nvsBinFile);

	for (index = 0; index < txParamLength; index++)
	{
		valueToSet = nvsPtr[index];
		os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);			
	}
}

VOID nvsFillDefaultRXParams(FILE *nvsBinFile)
{
	TI_UINT8	typeValue = eNVS_RADIO_RX_PARAMETERS;
	TI_UINT16	lengthValue = NVS_RX_PARAM_LENGTH;
	TI_UINT8	valueToSet = DEFAULT_EFUSE_VALUE;
	TI_UINT8	rxParamIndex;

	/* RX type */
	os_fwrite(&typeValue, sizeof(TI_UINT8), 1, nvsBinFile);

	/* RX length */
	os_fwrite(&lengthValue, sizeof(TI_UINT16), 1, nvsBinFile);

	for (rxParamIndex = 0; rxParamIndex < lengthValue; rxParamIndex++)
	{		
		os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);
	}
}


void nvsFillOldTXParams(FILE *nvsBinFile, const TI_UINT8	*buffer, const uint16	txParamLength)
{
	TI_UINT16	index;
	TI_UINT8	rxTypeValue;	
	TI_UINT8	valueToSet;
	TI_UINT16	tlvLength;
	
	/* TX BiP type */
	rxTypeValue = eNVS_RADIO_TX_PARAMETERS;
	os_fwrite(&rxTypeValue, sizeof(TI_UINT8), 1, nvsBinFile);

	/* TX BIP Length */
	tlvLength = txParamLength;
	os_fwrite(&tlvLength, sizeof(TI_UINT16), 1, nvsBinFile);

	for (index = 0; index < txParamLength; index++)
	{
		valueToSet = buffer[index];
		os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);
	}
}


VOID nvsFillDefaultTXParams(FILE *nvsBinFile, const uint16	txParamLength)
{
	TI_UINT32	index;
	TI_UINT32	lengthOfP2Gtable = NVS_TX_P2G_TABLE_LENGTH;
	TI_UINT8	p2GValue = 0;
	TI_UINT32	lengthOfPPASTepTable = NVS_TX_PPA_STEPS_TABLE_LENGTH;
	TI_UINT8	ppaStesValue = 0;
	TI_UINT32	lengthOfPDBufferTable = NVS_TX_PD_TABLE_LENGTH_NVS_V2;
	TI_UINT8	pdStesValue = 0;
	TI_UINT8	typeValue = eNVS_RADIO_TX_PARAMETERS;
	TI_UINT16	lengthValue = txParamLength;
	TI_UINT8	perChannelGainOffsetValue = 0;
	TI_UINT16	lengthOfGainOffsetTable = NUMBER_OF_RADIO_CHANNEL_INDEXS_E;

	/* TX type */
	os_fwrite(&typeValue, sizeof(TI_UINT8), 1, nvsBinFile);

	/* TX length */
	os_fwrite(&lengthValue, sizeof(TI_UINT16), 1, nvsBinFile);

	/* P2G table */
	for (index = 0; index < lengthOfP2Gtable; index++)
	{
		os_fwrite(&p2GValue, sizeof(TI_UINT8), 1, nvsBinFile);
	}

	/* PPA steps table */
	for (index = 0; index < lengthOfPPASTepTable; index++)
	{
		os_fwrite(&ppaStesValue, sizeof(TI_UINT8), 1, nvsBinFile);
	}

	/* Power Detector */
	for (index = 0; index < lengthOfPDBufferTable; index++)
	{
		os_fwrite(&pdStesValue, sizeof(TI_UINT8), 1, nvsBinFile);
	}

	/* Per Channel Gain Offset */
	for (index = 0; index < lengthOfGainOffsetTable; index++)
	{
		os_fwrite(&perChannelGainOffsetValue, sizeof(TI_UINT8), 1, nvsBinFile);
	}
}

void nvsFillRXParams(FILE *nvsBinFile, uint8	*nvsPtr, uint16	rxLength)
{
	TI_UINT8	rxTypeValue;
	TI_UINT8	valueToSet;
	TI_UINT16	index;

	/* RX BiP type */
	rxTypeValue = eNVS_RADIO_RX_PARAMETERS;
	os_fwrite(&rxTypeValue, sizeof(TI_UINT8), 1, nvsBinFile);

	/* RX Bip Length */
	os_fwrite(&rxLength, sizeof(TI_UINT16), 1, nvsBinFile);

	for (index = 0; index < rxLength; index++)
	{
		valueToSet = nvsPtr[index];
		os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);			
	}	
}


VOID  nvsFillVersion(FILE *nvsBinFile, TI_UINT32 version)
{
	char			versionBuffer[NVS_VERSION_PARAMETER_LENGTH];
	TI_UINT8		shift8;
	TI_UINT8		valueToSet;
	TI_UINT32		comparison;
	TI_INT16		index;
	TI_UINT16		lengthToSet;

	/* version type */
	valueToSet = eNVS_VERSION;
	os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);

	/* version length */
	lengthToSet = NVS_VERSION_PARAMETER_LENGTH;
	os_fwrite(&lengthToSet, sizeof(TI_UINT16), 1, nvsBinFile);	

	for (shift8 = 0, comparison = 0xff, index = NVS_VERSION_PARAMETER_LENGTH - 1;index >= 0; index--)
	{
		valueToSet = (version & comparison) >> shift8;
		versionBuffer[index] = valueToSet;		 

		comparison <<= 8;
		shift8 += 8;
	}

	for (index = 0; index < NVS_VERSION_PARAMETER_LENGTH; index++)
	{
		os_fwrite(&versionBuffer[index], sizeof(TI_UINT8), 1, nvsBinFile);	 
	}	
}


VOID nvsWriteEndNVS(FILE *nvsBinFile)
{
	TI_UINT8	valueToSet;
	TI_UINT16	lengthToSet;
	
	/* version type */
	valueToSet = eTLV_LAST;
	os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);

	valueToSet = eTLV_LAST;
	os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);

	/* version length */
	lengthToSet = 0;
	os_fwrite(&lengthToSet, sizeof(TI_UINT16), 1, nvsBinFile);
}

VOID nvsUpdateFile(THandle hCuCmd, TNvsStruct nvsStruct, TI_UINT8 version,  S8 updatedProtocol)
{
#ifdef _WINDOWS
    PS8 nvsFilePath = (PS8)"/windows/nvs_map.bin";
#else
    PS8 nvsFilePath = (PS8)"./nvs_map.bin";
#endif /*_WINDOWS*/
	TI_UINT8		currentNVSbuffer[1500];
	TI_UINT16		lengthOfCurrentNVSBufer;
	TI_BOOL     	prevNVSIsValid;
	TNvsStruct		oldNVSParam[eNUMBER_RADIO_TYPE_PARAMETERS_INFO];
    FILE			*nvsBinFile;
	TI_UINT8    	index;
	TI_UINT8		valueToSet = 0;
	TI_UINT16		nvsTXParamLength = NVS_TX_PARAM_LENGTH_NVS_V2;
	TI_UINT16		oldNVSTXParamLength;
	TI_UINT32		oldNVSVersion;
	TI_UINT32       currentNVSVersion = NVS_VERSION_2; 

	/* read previous NVS if exists */
	prevNVSIsValid = nvsReadFile(currentNVSbuffer, &lengthOfCurrentNVSBufer, nvsFilePath);

	if (prevNVSIsValid)
	{
		/* fill the TLV structure for the mode TX/RX */
		os_memset(oldNVSParam, 0, eNUMBER_RADIO_TYPE_PARAMETERS_INFO * sizeof(TNvsStruct));
		nvsParsePreviosOne(&currentNVSbuffer[NVS_PRE_PARAMETERS_LENGTH], oldNVSParam, &oldNVSVersion);

		if (currentNVSVersion == oldNVSVersion)
		{
			oldNVSTXParamLength = NVS_TX_PARAM_LENGTH_NVS_V2;
		
			/* if read all the parameter (needed) from the previous NVS */
			if ((oldNVSParam[eNVS_RADIO_TX_TYPE_PARAMETERS_INFO].Type != eNVS_RADIO_TX_PARAMETERS)	||
				(oldNVSParam[eNVS_RADIO_TX_TYPE_PARAMETERS_INFO].Length != nvsTXParamLength)		||
				(oldNVSParam[eNVS_RADIO_RX_TYPE_PARAMETERS_INFO].Type != eNVS_RADIO_RX_PARAMETERS)	||
				(oldNVSParam[eNVS_RADIO_RX_TYPE_PARAMETERS_INFO].Length != NVS_RX_PARAM_LENGTH))
			{
				/* the parameters are wrong */
				prevNVSIsValid = TI_FALSE;
			}	
			else
			{
				/* the parameters are right */
				prevNVSIsValid = TI_TRUE;
			}
		}
		else
		{
			/* there isn't NVS */
			prevNVSIsValid = TI_FALSE;
		}		
	}

	nvsBinFile = os_fopen(nvsFilePath, OS_FOPEN_WRITE_BINARY);

	if (NULL == nvsBinFile)
	{
		os_error_printf(CU_MSG_ERROR, (PS8)"\n Could not create FILE!!! !!!! \n");
	}
	
	/* fill MAC Address */
	nvsFillMACAddress(hCuCmd, nvsBinFile);

	/* fill end burst transaction zeros */
	for (index = 0; index < NVS_END_BURST_TRANSACTION_LENGTH; index++)
	{
		os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);
	}

	/* fill zeros to Align TLV start address */
	for (index = 0; index < NVS_ALING_TLV_START_ADDRESS_LENGTH; index++)
	{
		os_fwrite(&valueToSet, sizeof(TI_UINT8), 1, nvsBinFile);
	}

	/* Getting from TX BiP Command */
	if(NVS_FILE_TX_PARAMETERS_UPDATE == updatedProtocol)
	{
		// Fill new TX BiP values
		nvsFillTXParams(nvsBinFile, nvsStruct.Buffer, nvsStruct.Length);

		if (prevNVSIsValid)
		{
			/* set Parameters of RX from the previous file */
			nvsFillOldRxParams(nvsBinFile, oldNVSParam[eNVS_RADIO_RX_TYPE_PARAMETERS_INFO].Buffer,
							   oldNVSParam[eNVS_RADIO_RX_TYPE_PARAMETERS_INFO].Length);
		}
		else
		{
			nvsFillDefaultRXParams(nvsBinFile);
		}
	}
	/* Getting from RX BiP Command */
	else if (NVS_FILE_RX_PARAMETERS_UPDATE == updatedProtocol)
	{
		if (prevNVSIsValid)
		{
			// set Parameters of TX from the previous file
			nvsFillOldTXParams(nvsBinFile, oldNVSParam[eNVS_RADIO_TX_TYPE_PARAMETERS_INFO].Buffer,
									oldNVSParam[eNVS_RADIO_TX_TYPE_PARAMETERS_INFO].Length);
		}
		else
		{
			nvsFillDefaultTXParams(nvsBinFile, nvsTXParamLength);
		}

		/* Fill new RX BiP values */ 
		nvsFillRXParams(nvsBinFile, nvsStruct.Buffer, nvsStruct.Length);
	
	}
	else  /* NVS_FILE_WRONG_UPDATE == updatedProtocol */
	{
		if (prevNVSIsValid)
		{
			/* set Parameters of TX from the previous file */
			nvsFillOldTXParams(nvsBinFile,
									oldNVSParam[eNVS_RADIO_TX_TYPE_PARAMETERS_INFO].Buffer,
				                    oldNVSParam[eNVS_RADIO_TX_TYPE_PARAMETERS_INFO].Length);

			/* set Parameters of RX from the previous file  */
			nvsFillOldRxParams(nvsBinFile,
									oldNVSParam[eNVS_RADIO_RX_TYPE_PARAMETERS_INFO].Buffer,
				                    oldNVSParam[eNVS_RADIO_RX_TYPE_PARAMETERS_INFO].Length);
		}
		else
		{
			/* set default TX param */
			nvsFillDefaultTXParams(nvsBinFile,nvsTXParamLength);

			/* set default RX param */
			nvsFillDefaultRXParams(nvsBinFile);
		}		
	}

	/* Fill the NVS version to the NVS */
	nvsFillVersion(nvsBinFile, version);

	/* End of NVS */
	nvsWriteEndNVS(nvsBinFile);

	/* close the file */
	os_fclose(nvsBinFile);
}

VOID CuCmd_BIP_BufferCalReferencePoint(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
#define NUM_OF_PARAMETERS_REF_POINT 3

	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
	TTestCmd data;

	if(nParms != NUM_OF_PARAMETERS_REF_POINT)
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"Missing Param1: iReferencePointDetectorValue\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Missing Param2: iReferencePointPower\n");
		os_error_printf(CU_MSG_INFO2, (PS8)"Missing Param3: isubBand\n");
		return;
	}
	else
	{
		os_memset(&data, 0, sizeof(TTestCmd));
		data.testCmdId = TEST_CMD_UPDATE_PD_REFERENCE_POINT;
/*		data.testCmd_u.PdBufferCalReferencePoint.iReferencePointDetectorValue = 189;
		data.testCmd_u.PdBufferCalReferencePoint.iReferencePointPower = 12;
		data.testCmd_u.PdBufferCalReferencePoint.isubBand = 0; 1- BG 2-*/

		data.testCmd_u.PdBufferCalReferencePoint.iReferencePointDetectorValue = parm[0].value;
		data.testCmd_u.PdBufferCalReferencePoint.iReferencePointPower = parm[1].value;
		data.testCmd_u.PdBufferCalReferencePoint.isubBand = (U8)parm[2].value;
		
		if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data))
		{
			os_error_printf(CU_MSG_INFO2, (PS8)"BufferCalReferencePoint has failed\n");            
			return;
		}
		
		os_error_printf(CU_MSG_INFO2, (PS8)"BufferCalReferencePoint was configured succesfully\n");
	}
	return;
}


/* P2G Calibration */
VOID CuCmd_BIP_StartBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;
    U32 i;
    
	os_memset(&data, 0, sizeof(TTestCmd));
	
	data.testCmdId = TEST_CMD_P2G_CAL;

    data.testCmd_u.P2GCal.iSubBandMask = 0;
    for (i = 0; i < 8; i++) 
    {
        data.testCmd_u.P2GCal.iSubBandMask |= (U8)parm[i].value << i;
    }
    
	if (data.testCmd_u.P2GCal.iSubBandMask == 0) 
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"At least one sub-band should be enabled\n");        
		return;
	}
	
	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"Tx calibration start has failed\n");        
		return;
	}
	
	if (TI_OK != data.testCmd_u.P2GCal.oRadioStatus) {
		os_error_printf(CU_MSG_INFO2, (PS8)"Tx calibration returned status: %d\n", data.testCmd_u.P2GCal.oRadioStatus);        
		return;
	}

	nvsUpdateFile(hCuCmd,data.testCmd_u.P2GCal.oNvsStruct , (TI_UINT8)data.testCmd_u.P2GCal.oNVSVersion, NVS_FILE_TX_PARAMETERS_UPDATE);

}

VOID CuCmd_BIP_EnterRxBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;
        
	os_memset(&data, 0, sizeof(TTestCmd));
	
	data.testCmdId = TEST_CMD_RX_PLT_ENTER;
      
	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"Enter Rx calibration has failed\n");        
		return;
	}

   
    if (TI_OK != data.testCmd_u.RxPlt.oRadioStatus) {
		os_error_printf(CU_MSG_INFO2, (PS8)"Enter Rx calibration returned status: %d\n", data.testCmd_u.RxPlt.oRadioStatus);        
		return;
	}

}


VOID CuCmd_BIP_StartRxBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;
        
	os_memset(&data, 0, sizeof(TTestCmd));
	
    data.testCmdId = TEST_CMD_RX_PLT_CAL;
    data.testCmd_u.RxPlt.iExternalSignalPowerLevel = (S32)parm[0].value;
   
    if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Rx calibration has failed\n");        
        return;
    }

    if (TI_OK != data.testCmd_u.RxPlt.oRadioStatus) {
		os_error_printf(CU_MSG_INFO2, (PS8)"Rx calibration returned status: %d\n", data.testCmd_u.RxPlt.oRadioStatus);        
		return;
	}

	nvsUpdateFile(hCuCmd, data.testCmd_u.RxPlt.oNvsStruct, (TI_UINT8)data.testCmd_u.RxPlt.oNVSVersion, NVS_FILE_RX_PARAMETERS_UPDATE);
}

VOID CuCmd_BIP_ExitRxBIP(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
	CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    TTestCmd data;
        
	os_memset(&data, 0, sizeof(TTestCmd));
	
	data.testCmdId = TEST_CMD_RX_PLT_EXIT;
      
	if(OK != CuCommon_Radio_Test(pCuCmd->hCuCommon, &data)) 
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"Exit Rx calibration has failed\n");        
		return;
	}

   
    if (TI_OK != data.testCmd_u.RxPlt.oRadioStatus) {
		os_error_printf(CU_MSG_INFO2, (PS8)"Exit Rx calibration returned status: %d\n", data.testCmd_u.RxPlt.oRadioStatus);        
		return;
	}

}



VOID CuCmd_SetPrivacyAuth(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 AuthMode;
    
    if( nParms )
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"Setting privacy authentication to %ld\n", parm[0].value);
        AuthMode = parm[0].value;
        if (pCuCmd->hWpaCore == NULL)
        {
            /* we can only accept WEP or OPEN configurations */
            if((AuthMode >= os802_11AuthModeOpen) && (AuthMode <= os802_11AuthModeAutoSwitch))
            {
                if(OK != CuCommon_SetU32(pCuCmd->hCuCommon, RSN_EXT_AUTHENTICATION_MODE, AuthMode)) return;
            }
            else
            {
                os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetPrivacyAuth - cannot set mode (%ld) when Suppl is not present\n", AuthMode);
                return;
            }
        }
        else
        {
#ifndef NO_WPA_SUPPL
            if(OK != WpaCore_SetAuthMode(pCuCmd->hWpaCore, AuthMode)) return;
#endif
        }
    }
    else
    {
#ifdef WPA_ENTERPRISE
        static named_value_t auth_mode_type[] = {
            { os802_11AuthModeOpen,             (PS8)"Open"      },
            { os802_11AuthModeShared,           (PS8)"Shared"    },
            { os802_11AuthModeAutoSwitch,       (PS8)"AutoSwitch"},
            { os802_11AuthModeWPA,              (PS8)"WPA"       },
            { os802_11AuthModeWPAPSK,           (PS8)"WPAPSK"    },
            { os802_11AuthModeWPANone,          (PS8)"WPANone"   },
            { os802_11AuthModeWPA2,             (PS8)"WPA2"      },
            { os802_11AuthModeWPA2PSK,          (PS8)"WPA2PSK"   },
        };
#else
        static named_value_t auth_mode_type[] = {
            { os802_11AuthModeOpen,             (PS8)"Open"      },
            { os802_11AuthModeShared,           (PS8)"Shared"    },
            { os802_11AuthModeWPAPSK,           (PS8)"WPAPSK"    },
            { os802_11AuthModeWPA2PSK,          (PS8)"WPA2PSK"   },
        };
#endif
        
        if (pCuCmd->hWpaCore == NULL)
        {
            if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_EXT_AUTHENTICATION_MODE, &AuthMode)) return;    
        }
        else 
        {
#ifndef NO_WPA_SUPPL
            if(OK != WpaCore_GetAuthMode(pCuCmd->hWpaCore, &AuthMode)) return;  
#endif
        }

        print_available_values(auth_mode_type);
        os_error_printf(CU_MSG_INFO2, (PS8)"AuthenticationMode=%d\n", AuthMode );
    }   
}

VOID CuCmd_SetPrivacyEap(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd; 
    OS_802_11_EAP_TYPES EapType;
    S32 i;
    named_value_t EapType_type[] = {
        { OS_EAP_TYPE_NONE,                 (PS8)"OS_EAP_TYPE_NONE" },
        { OS_EAP_TYPE_MD5_CHALLENGE,        (PS8)"OS_EAP_TYPE_MD5_CHALLENGE" },
        { OS_EAP_TYPE_GENERIC_TOKEN_CARD,   (PS8)"OS_EAP_TYPE_GENERIC_TOKEN_CARD" },
        { OS_EAP_TYPE_TLS,                  (PS8)"OS_EAP_TYPE_TLS" },
        { OS_EAP_TYPE_LEAP,                 (PS8)"OS_EAP_TYPE_LEAP" },
        { OS_EAP_TYPE_TTLS,                 (PS8)"OS_EAP_TYPE_TTLS" },
        { OS_EAP_TYPE_PEAP,                 (PS8)"OS_EAP_TYPE_PEAP" },
        { OS_EAP_TYPE_MS_CHAP_V2,           (PS8)"OS_EAP_TYPE_MS_CHAP_V2" },
        { OS_EAP_TYPE_FAST,                 (PS8)"OS_EAP_TYPE_FAST" }        
    };
    
    /* check if we have supplicant */
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetPrivacyEap - cannot set EapType when Suppl is not present\n");
        return;                     
    }

    if( nParms )
    {
        EapType = parm[0].value;
        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY(i, EapType_type, EapType);
        if(i == SIZE_ARR(EapType_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetPrivacyEap, EapType %d is not defined!\n", EapType);
            return;
        }       

#ifndef NO_WPA_SUPPL

        if(OK != WpaCore_SetPrivacyEap(pCuCmd->hWpaCore, EapType))
            os_error_printf(CU_MSG_INFO2, (PS8)"Error Setting EapType to %ld\n", EapType);
        else
            os_error_printf(CU_MSG_INFO2, (PS8)"Setting EapType to %ld\n", EapType);

#endif        
               /* 
        WEXT phase I
        TI_SetEAPType( g_id_adapter, (OS_802_11_EAP_TYPES) parm[0].value );
        TI_SetEAPTypeDriver( g_id_adapter, (OS_802_11_EAP_TYPES) parm[0].value );
        */
        
    }
    else
    {       
        print_available_values(EapType_type);        
    }
}


VOID CuCmd_SetPrivacyEncryption(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 EncryptionType;
#ifndef NO_WPA_SUPPL
    OS_802_11_ENCRYPTION_TYPES EncryptionTypePairWise;
    OS_802_11_ENCRYPTION_TYPES EncryptionTypeGroup;
#endif
    S32 i;
   
    if( nParms )
    {
        EncryptionType = parm[0].value;

        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY(i, encrypt_type, EncryptionType);
        if(i == SIZE_ARR(encrypt_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetPrivacyEncryption, EncryptionType %d is not defined!\n", EncryptionType);
            return;
        }

        if (pCuCmd->hWpaCore == NULL)
        {
            if((EncryptionType == OS_ENCRYPTION_TYPE_NONE) || (EncryptionType == OS_ENCRYPTION_TYPE_WEP))
            {
                if(OK != CuCommon_SetU32(pCuCmd->hCuCommon, RSN_ENCRYPTION_STATUS_PARAM, (U32)EncryptionType)) return;              
            }
            else
            {
                os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_SetPrivacyEncryption - can't set EncryptionType %s when not connected to supplicant",encrypt_type[i].name);
                return;
            }
        }
        else
        {
#ifndef NO_WPA_SUPPL
            switch(EncryptionType)
            {
                case OS_ENCRYPTION_TYPE_NONE:
                    EncryptionTypePairWise = OS_ENCRYPTION_TYPE_NONE;
                    EncryptionTypeGroup = OS_ENCRYPTION_TYPE_NONE;
                    break;
                case OS_ENCRYPTION_TYPE_WEP:
                    EncryptionTypePairWise = OS_ENCRYPTION_TYPE_WEP;
                    EncryptionTypeGroup = OS_ENCRYPTION_TYPE_WEP;
                    break;
                case OS_ENCRYPTION_TYPE_TKIP:
                    EncryptionTypePairWise = OS_ENCRYPTION_TYPE_TKIP;
                    EncryptionTypeGroup = OS_ENCRYPTION_TYPE_TKIP;
                    break;
                case OS_ENCRYPTION_TYPE_AES:
                    EncryptionTypePairWise = OS_ENCRYPTION_TYPE_AES;
                    EncryptionTypeGroup = OS_ENCRYPTION_TYPE_AES;
                    break;
            }

            if(OK != WpaCore_SetEncryptionPairWise(pCuCmd->hWpaCore, EncryptionTypePairWise)) return;   
            if(OK != WpaCore_SetEncryptionGroup(pCuCmd->hWpaCore, EncryptionTypeGroup)) return;         
#endif
            
        }

        os_error_printf(CU_MSG_INFO2, (PS8)"Setting privacy encryption to %ld\n", encrypt_type[i]);        
    }
    else
    {   
        if (pCuCmd->hWpaCore == NULL)
        {
            if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_ENCRYPTION_STATUS_PARAM, &EncryptionType)) return;  

            switch (EncryptionType)
            {
            case TWD_CIPHER_NONE:
                EncryptionType = OS_ENCRYPTION_TYPE_NONE;
                break;
            case TWD_CIPHER_WEP:   
            case TWD_CIPHER_WEP104:
                EncryptionType = OS_ENCRYPTION_TYPE_WEP;
                break;
            case TWD_CIPHER_TKIP: 
                EncryptionType = OS_ENCRYPTION_TYPE_TKIP;
                break;
            case TWD_CIPHER_AES_WRAP:
            case TWD_CIPHER_AES_CCMP:
                EncryptionType = OS_ENCRYPTION_TYPE_AES;
                break;
            default:
                os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_SetPrivacyEncryption - unknown encryption type (%d)",EncryptionType);
                break;
            }
        }
        else
        {
#ifndef NO_WPA_SUPPL
            if(OK != WpaCore_GetEncryptionPairWise(pCuCmd->hWpaCore, &EncryptionTypePairWise)) return;  
            if(OK != WpaCore_GetEncryptionGroup(pCuCmd->hWpaCore, &EncryptionTypeGroup)) return;

            if((EncryptionTypePairWise == OS_ENCRYPTION_TYPE_NONE) && (EncryptionTypeGroup == OS_ENCRYPTION_TYPE_NONE))
                EncryptionType = OS_ENCRYPTION_TYPE_NONE;
            else if((EncryptionTypePairWise == OS_ENCRYPTION_TYPE_WEP) && (EncryptionTypeGroup == OS_ENCRYPTION_TYPE_WEP))
                EncryptionType = OS_ENCRYPTION_TYPE_WEP;
            else if((EncryptionTypePairWise == OS_ENCRYPTION_TYPE_TKIP) && (EncryptionTypeGroup == OS_ENCRYPTION_TYPE_TKIP))
                EncryptionType = OS_ENCRYPTION_TYPE_TKIP;
            else if((EncryptionTypePairWise == OS_ENCRYPTION_TYPE_AES) && (EncryptionTypeGroup == OS_ENCRYPTION_TYPE_AES))
                EncryptionType = OS_ENCRYPTION_TYPE_AES;
            else
            {
                os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_SetPrivacyEncryption - unknown encryption type (%d,%d)",EncryptionTypePairWise, EncryptionTypeGroup);
                return;
            }
#endif
        }
        
        print_available_values(encrypt_type);
        os_error_printf(CU_MSG_INFO2, (PS8)"Encryption = %d\n", EncryptionType);
    }
}

VOID CuCmd_SetPrivacyKeyType(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    OS_802_11_KEY_TYPES KeyType;
    S32 i;
    static named_value_t KeyType_type[] = {
            { OS_KEY_TYPE_STATIC,             (PS8)"STATIC" },
            { OS_KEY_TYPE_DYNAMIC,            (PS8)"DYNAMIC"}
    };

    /* check if we have supplicant */
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetPrivacyEncryptGroup - cannot set encryption Group when Suppl is not present\n");
        return;                     
    }

    if( nParms )
    {
        KeyType = parm[0].value;
        /* check if the param is valid */
        CU_CMD_FIND_NAME_ARRAY(i, KeyType_type, KeyType);
        if(i == SIZE_ARR(KeyType_type))
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"CuCmd_SetPrivacyKeyType - KeyType %d is not defined!\n", KeyType);
            return;
        }       
        
        os_error_printf(CU_MSG_INFO2, (PS8)"Setting KeyType to %ld\n", KeyType);

         /* 
        WEXT phase I
        TI_SetKeyType( g_id_adapter, (OS_802_11_KEY_TYPES)parm[0].value );
        */
    }
    else
    {       
        print_available_values(KeyType_type); 
    }
}

VOID CuCmd_SetPrivacyMixedMode(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 MixedMode;

    if( nParms )
    {
        MixedMode = parm[0].value;
        os_error_printf(CU_MSG_INFO2, (PS8)"Setting MixedMode to %s\n", (MixedMode)?"True":"False");

        CuCommon_SetU32(pCuCmd->hCuCommon, TIWLN_802_11_MIXED_MODE_SET, MixedMode);
    }
    else
    {  
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, TIWLN_802_11_MIXED_MODE_GET, &MixedMode)) return;
        
        os_error_printf(CU_MSG_INFO2, (PS8)"Mixed Mode: 0 - FALSE, 1 - TRUE\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Mixed Mode =%s\n", (MixedMode)?"True":"False");
    }
}

VOID CuCmd_SetPrivacyAnyWpaMode(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
#ifndef NO_WPA_SUPPL
    U32 anyWpaMode;

    if( nParms )
    {
        anyWpaMode = parm[0].value;
        os_error_printf(CU_MSG_INFO2, (PS8)"Setting anyWpaMode to %s\n", (anyWpaMode)?"True":"False");

        WpaCore_SetAnyWpaMode(pCuCmd->hWpaCore,(U8)anyWpaMode);
    }
    else
    {  
		WpaCore_GetAnyWpaMode(pCuCmd->hWpaCore,(U8 *)&anyWpaMode);
        os_error_printf(CU_MSG_INFO2, (PS8)"Any WPA Mode: 0 - FALSE, 1 - TRUE\n");
        os_error_printf(CU_MSG_INFO2, (PS8)"Any WPA =%s\n", (anyWpaMode)?"True":"False");
    }
#else
    os_error_printf(CU_MSG_INFO2, (PS8)"Any Wpa Mode support only in Linux supplicants\n");
#endif
}

VOID CuCmd_SetPrivacyCredentials(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    
    if( nParms == 0 )
        return;

   if (pCuCmd->hWpaCore == NULL)
   {
       os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetPrivacyPskPassPhrase - cannot set Credential password phrase when Suppl is not present\n");
       return;                     
   }

#ifndef NO_WPA_SUPPL
      
     if( nParms == 2 )
    {
      WpaCore_SetCredentials(pCuCmd->hWpaCore,(PU8)(parm[0].value),(PU8)(parm[1].value));     
    }
    else if( nParms == 1 )
    {
      WpaCore_SetCredentials(pCuCmd->hWpaCore,(PU8)(parm[0].value),NULL);
    }
#endif    
}

VOID CuCmd_SetPrivacyPskPassPhrase(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 IsHexKey = FALSE;
    U8 buf[WPACORE_MAX_PSK_STRING_LENGTH];
    PS8 pPassphrase;
    S32 len;

    if( nParms == 0 )
        return;

    /* check if we have supplicant */
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetPrivacyPskPassPhrase - cannot set PSK password phrase when Suppl is not present\n");
        return;                     
    }

    len = os_strlen((PS8)(parm[0].value));
    pPassphrase = ((PS8)parm[0].value);
    os_memset(buf, 0, WPACORE_MAX_PSK_STRING_LENGTH);

    
    if( nParms == 2 )
    {       
        if( !os_strcmp( (PS8) parm[1].value, (PS8)"hex") )
            IsHexKey = TRUE;
        else if(!os_strcmp( (PS8) parm[1].value, (PS8)"text"))
            IsHexKey = FALSE;
    }

    if( IsHexKey )
    {
        U8 val_l;
        U8 val_u;
        S32 i;
        
        if( len != WPACORE_PSK_HEX_LENGTH )
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"The hexa PSKPassphrase must be at length of %d hexa digits \n",WPACORE_PSK_HEX_LENGTH);
            return;
        }
        
        for( i=0; *pPassphrase; i++, pPassphrase++ )
        {
            val_u = CuCmd_Char2Hex(*pPassphrase);
            if( val_u == ((U8)-1) ) return;
            val_l = CuCmd_Char2Hex(*(++pPassphrase));
            if( val_l == ((U8)-1) ) return; 
            buf[i] = ((val_u << 4) | val_l);
        }        
    }
    else
    {
        if (len > WPACORE_MAX_PSK_STRING_LENGTH || len < WPACORE_MIN_PSK_STRING_LENGTH)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"The PSKPassphrase must be between %d to  %d chars \n", WPACORE_MIN_PSK_STRING_LENGTH, WPACORE_MAX_PSK_STRING_LENGTH);
            return;
        }

        os_memcpy((PVOID)buf, (PVOID)pPassphrase, len);
    }

    os_error_printf(CU_MSG_INFO2, (PS8)"Setting PSKPassphrase to %s\n", (PS8) parm[0].value);
#ifndef NO_WPA_SUPPL
    WpaCore_SetPskPassPhrase(pCuCmd->hWpaCore, buf);
#endif
    
}

VOID CuCmd_SetPrivacyCertificate(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
  CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
  
   if( nParms == 0 )
        return;

   if (pCuCmd->hWpaCore == NULL)
   {
       os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_SetPrivacyPskPassPhrase - cannot set Certification  when Suppl is not present\n");
       return;                     
   }
#ifndef NO_WPA_SUPPL
   WpaCore_SetCertificate(pCuCmd->hWpaCore,(PU8)(parm[0].value));
#endif     
  
    
}

VOID CuCmd_StopSuppl(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    /* check if we have supplicant */
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_StopSuppl - cannot stop supplicant when Suppl is not present :-)\n");
        return;                     
    }
#ifndef NO_WPA_SUPPL    
    WpaCore_StopSuppl(pCuCmd->hWpaCore);
#endif
}

VOID CuCmd_ChangeSupplDebugLevels(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    /* check if we have supplicant */
    if (pCuCmd->hWpaCore == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error - CuCmd_DebugSuppl - cannot debug supplicant when Suppl is not present :-)\n");
        return;                     
    }
#ifndef NO_WPA_SUPPL
    WpaCore_ChangeSupplDebugLevels(pCuCmd->hWpaCore, parm[0].value, parm[1].value, parm[2].value);
#endif
}

VOID CuCmd_AddPrivacyKey(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    PS8 buf;
    U32 key_id = 0;
    U32 def_flag = 0;
    U32 IsHexKey = TRUE;
    OS_802_11_WEP data;
    U8 val_l, val_u;
    S32 len;
    
    buf =  (PS8)parm[0].value;
    key_id = (U32)parm[1].value;
    if( parm[2].value ) def_flag = 0x80000000;
    os_memset(data.KeyMaterial,0,sizeof(data.KeyMaterial));
    len = os_strlen((PS8)buf);

    if(key_id > 3)
    {
        os_error_printf(CU_MSG_INFO2, (PS8)"the key index must be between 0 and 3\n");
        return;
    }
    
    if( nParms >= 4 )
    {
        if( !os_strcmp( (PS8) parm[3].value, (PS8)"hex") )
            IsHexKey = TRUE;
        else if( !os_strcmp( (PS8) parm[3].value, (PS8)"HEX") )
            IsHexKey = TRUE;
        else if(!os_strcmp( (PS8) parm[3].value, (PS8)"text"))
            IsHexKey = FALSE;
        else if(!os_strcmp( (PS8) parm[3].value, (PS8)"TEXT"))
            IsHexKey = FALSE;
    }

    if( IsHexKey )
    {
        S32 i;

        if( len % 2 )
        {
            os_error_printf(CU_MSG_INFO2, (PS8)"The hexa key should be even length\n");
            return;
        }
        if(len <= 10) /*10 is number of character for key length 40 bit*/
            data.KeyLength = 5;
        else if(len <= 26) /*26 is number of character for key length 128 bit*/
            data.KeyLength = 13;
        else if(len <= 58) /*58 is number of character for key length 256 bit*/
            data.KeyLength = 29;
        else 
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_AddPrivacyKey - key length not valid\n" );
            return;
        }

        for( i=0; *buf; i++, buf++ )
        {
            val_u = CuCmd_Char2Hex(*buf);
            if(val_u == ((U8)-1)) return;
            val_l = CuCmd_Char2Hex(*(++buf));
            if(val_l == ((U8)-1)) return;
            data.KeyMaterial[i] = ((val_u << 4) | val_l);
        }
    }
    else /* for ascii key */
    {
        if(len <= 5) /*10 is number of character for key length 40 bit*/
            data.KeyLength = 5;
        else if(len <= 13) /*26 is number of character for key length 128 bit*/
            data.KeyLength = 13;
        else if(len <= 29) /*58 is number of character for key length 256 bit*/
            data.KeyLength = 29;
        else 
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuCmd_AddPrivacyKey - key length not valid\n" );
            return;
        }
        os_memcpy((PVOID)data.KeyMaterial, (PVOID)buf, len);
    }

    data.KeyIndex = def_flag | key_id;
    data.Length = sizeof(OS_802_11_WEP);

    /* check if we have supplicant */
    if (pCuCmd->hWpaCore == NULL)
    {
        CuCommon_AddKey(pCuCmd->hCuCommon, &data);
    }
    else
    {
#ifndef NO_WPA_SUPPL
        WpaCore_AddKey(pCuCmd->hWpaCore, &data);
#endif
    }
}

VOID CuCmd_RemovePrivacyKey(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    /* check if we have supplicant */
    CuCommon_RemoveKey(pCuCmd->hCuCommon, parm[0].value);
}

VOID CuCmd_GetPrivacyDefaultKey(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    U32 DefaultKeyId;
    
    /* check if we have supplicant */
    if (pCuCmd->hWpaCore == NULL)
    {
        if(OK != CuCommon_GetU32(pCuCmd->hCuCommon, RSN_DEFAULT_KEY_ID, &DefaultKeyId)) return; 
    }
    else
    {
#ifndef NO_WPA_SUPPL
        if(OK != WpaCore_GetDefaultKey(pCuCmd->hWpaCore, &DefaultKeyId)) return;    
#endif
    }

    os_error_printf(CU_MSG_INFO2, (PS8)"WEP default key ID = %d\n", DefaultKeyId );
}

VOID CuCmd_EnableKeepAlive (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (OK != CuCommon_SetU32 (pCuCmd->hCuCommon, POWER_MGR_KEEP_ALIVE_ENA_DIS, 1))
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Unable to enable keep-alive messages\n");
    }
}

VOID CuCmd_DisableKeepAlive (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    if (OK != CuCommon_SetU32 (pCuCmd->hCuCommon, POWER_MGR_KEEP_ALIVE_ENA_DIS, 0))
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Unable to disable keep-alive messages\n");
    }
}

VOID CuCmd_AddKeepAliveMessage (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t                 *pCuCmd = (CuCmd_t*)hCuCmd;
    TKeepAliveTemplate      keepAliveParams;

    if (4 != nParms)
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Number of params to add keep-alive message is %d\n", nParms);
        return;
    }

    /* copy keep-alive params */
    keepAliveParams.keepAliveParams.index = (U8)parm[ 0 ].value;
    keepAliveParams.keepAliveParams.interval = parm[ 1 ].value;
    keepAliveParams.keepAliveParams.trigType = parm[ 2 ].value;
    keepAliveParams.keepAliveParams.enaDisFlag = 1; /* set to enable */
    keepAliveParams.msgBufferLength = os_strlen ((PS8)parm[ 3 ].value);
    if (0 == (keepAliveParams.msgBufferLength %2))
    {
        keepAliveParams.msgBufferLength = keepAliveParams.msgBufferLength / 2;
    }
    else
    {
        keepAliveParams.msgBufferLength = (keepAliveParams.msgBufferLength / 2) + 1;
    }
    /* convert the string to hexadeciaml values, and copy it */
    CuCmd_atox_string ((U8*)parm[ 3 ].value, &keepAliveParams.msgBuffer[ 0 ]);

    if (TI_OK != CuCommon_SetBuffer(pCuCmd->hCuCommon, POWER_MGR_KEEP_ALIVE_ADD_REM, 
                                    &(keepAliveParams), sizeof(TKeepAliveTemplate)))
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Unable to add keep-alive message\n");
    }
}

VOID CuCmd_RemoveKeepAliveMessage (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t                 *pCuCmd = (CuCmd_t*)hCuCmd;
    TKeepAliveTemplate      keepAliveParams;

    if (1 != nParms)
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Number of params to remove keep-alive message is %d\n", nParms);
        return;
    }

    /* copy keep-alive params */
    keepAliveParams.keepAliveParams.index = (U8)parm[ 0 ].value;
    keepAliveParams.keepAliveParams.enaDisFlag = 0; /* set to disable */
    keepAliveParams.keepAliveParams.interval = 1000; /* FW validate all parameters, so some reasonable values must be used */
    keepAliveParams.keepAliveParams.trigType = KEEP_ALIVE_TRIG_TYPE_PERIOD_ONLY;

    if (OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, POWER_MGR_KEEP_ALIVE_ADD_REM, 
                                  &(keepAliveParams), sizeof(TKeepAliveTemplate)))
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Unable to remove keep-alive message\n");
    }
}

VOID CuCmd_ShowKeepAlive (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t                 *pCuCmd = (CuCmd_t*)hCuCmd;
    TKeepAliveConfig        tConfig;
    U32                     uIndex, uNameIndex;
    S8                      msgBuffer[ KEEP_ALIVE_TEMPLATE_MAX_LENGTH * 2 ];

    if (OK != CuCommon_GetSetBuffer (pCuCmd->hCuCommon, POWER_MGR_KEEP_ALIVE_GET_CONFIG, 
                                     &(tConfig), sizeof(TKeepAliveConfig)))
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Unable to read keep-alive configuration\n");
    }

    os_error_printf (CU_MSG_ERROR, (PS8)"Keep-Alive configuration:\n"
                                   "-------------------------\n");
    if (TRUE == tConfig.enaDisFlag)
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Keep-Alive global flag set to enabled\n\n");
    }
    else
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Keep-Alive global flag set to disabled\n\n");
    }

    os_error_printf (CU_MSG_ERROR, (PS8)"%-8s %-8s %-9s %-10s %s\n", (PS8)"Index", (PS8)"Enabled", (PS8)"Trig Type", (PS8)"Interval", (PS8)"Pattern");
    os_error_printf (CU_MSG_ERROR, (PS8)"-----------------------------------------------\n");
    for (uIndex = 0; uIndex < KEEP_ALIVE_MAX_USER_MESSAGES; uIndex++)
    {
        if (TRUE == tConfig.templates[ uIndex ].keepAliveParams.enaDisFlag)
        {
            CU_CMD_FIND_NAME_ARRAY (uNameIndex, tKeepAliveTriggerTypes, 
                                    tConfig.templates[ uIndex ].keepAliveParams.trigType);
            CuCmd_xtoa_string (&(tConfig.templates[ uIndex ].msgBuffer[ 0 ]), 
                               tConfig.templates[ uIndex ].msgBufferLength, (U8*)&(msgBuffer[ 0 ]));
                             os_error_printf (CU_MSG_ERROR, (PS8)"%-8d %-8d %-9s %-10d %s\n", uIndex, 
                             tConfig.templates[ uIndex ].keepAliveParams.enaDisFlag,
                             tKeepAliveTriggerTypes[ uNameIndex ].name,
                             tConfig.templates[ uIndex ].keepAliveParams.interval,
                             &(msgBuffer[ 0 ]));
        }
        else
        {
            os_error_printf (CU_MSG_ERROR, (PS8)"%-8d %-8d %-9s %-10d %s\n", uIndex, 0, (PS8)"N/A", 0, (PS8)"N/A");
        }
    }
}


VOID CuCmd_SetArpIPFilter (THandle hCuCmd, ConParm_t parm[], U16 nParms)
{

    TIpAddr     staIp;
    CuCmd_t     *pCuCmd = (CuCmd_t*)hCuCmd;
    TI_UINT8    length = 4;

    if (length != nParms)
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Error! IP format requires 4 parameters as follows: <Part1> <Part2> <Part3> <Part4>  \n");
        os_error_printf (CU_MSG_ERROR, (PS8)"Please note! IP of 0 0 0 0 will disable the arp filtering feature \n");
        return;
    }

    staIp[0] = (TI_UINT8)parm[0].value;
    staIp[1] = (TI_UINT8)parm[1].value;
    staIp[2] = (TI_UINT8)parm[2].value;
    staIp[3] = (TI_UINT8)parm[3].value;


     if (OK != CuCommon_SetBuffer (pCuCmd->hCuCommon, SITE_MGR_SET_WLAN_IP_PARAM, staIp, length))
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Unable to configure ARP IP filter \n");
    }

}

VOID CuCmd_ShowAbout(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;
    S8 FwVesrion[FW_VERSION_LEN];

    if(OK != CuCommon_GetBuffer(pCuCmd->hCuCommon, SITE_MGR_FIRMWARE_VERSION_PARAM,
        FwVesrion, FW_VERSION_LEN)) return;
    
#ifdef XCC_MODULE_INCLUDED
    os_error_printf(CU_MSG_INFO2, (PS8)"Driver version: %s_XCC\n", 
                                                SW_VERSION_STR);
#elif GEM_SUPPORTED
    os_error_printf(CU_MSG_INFO2, (PS8)"Driver version: %s_GEM\n", 
                                                SW_VERSION_STR);
#else
    os_error_printf(CU_MSG_INFO2, (PS8)"Driver version: %s_NOCCX\n", 
                                                SW_VERSION_STR);
#endif/* XCC_MODULE_INCLUDED*/
    os_error_printf(CU_MSG_INFO2, (PS8)"Firmware version: %s\n", 
                                                FwVesrion);
    
}

VOID CuCmd_Quit(THandle hCuCmd, ConParm_t parm[], U16 nParms)
{
    CuCmd_t* pCuCmd = (CuCmd_t*)hCuCmd;

    Console_Stop(pCuCmd->hConsole);
}


