/*
 * InternalCmdCodes.h
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

#ifndef __PARAM_MGR_H__
#define __PARAM_MGR_H__

#include "CmdInterfaceCodes.h"
#include "TWDriver.h"

/* Following are the parameters numbers. Each module can have 256 parameters */
typedef enum
{    
     /* TWD Control section */
    TWD_COUNTERS_PARAM                     =           GET_BIT | TWD_MODULE_PARAM | TWD_COUNTERS_PARAM_ID,
    TWD_LISTEN_INTERVAL_PARAM              = SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_LISTEN_INTERVAL_PARAM_ID,
    TWD_BEACON_INTERVAL_PARAM              =           GET_BIT | TWD_MODULE_PARAM | TWD_BEACON_INTERVAL_PARAM_ID,
    TWD_TX_POWER_PARAM                     = SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_TX_POWER_PARAM_ID,
    TWD_CLK_RUN_ENABLE_PARAM               = SET_BIT |           TWD_MODULE_PARAM | TWD_CLK_RUN_ENABLE_PARAM_ID,
    TWD_QUEUES_PARAM                       = SET_BIT |           TWD_MODULE_PARAM | TWD_QUEUES_PARAM_ID, 
    TWD_TX_RATE_CLASS_PARAM                = SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_TX_RATE_CLASS_PARAM_ID,
    TWD_MAX_TX_MSDU_LIFE_TIME_PARAM        = SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_MAX_TX_MSDU_LIFE_TIME_PARAM_ID,
    TWD_MAX_RX_MSDU_LIFE_TIME_PARAM        = SET_BIT | GET_BIT | TWD_MODULE_PARAM | TWD_MAX_RX_MSDU_LIFE_TIME_PARAM_ID,
    TWD_RX_TIME_OUT_PARAM                  = SET_BIT |           TWD_MODULE_PARAM | TWD_RX_TIME_OUT_PARAM_ID,
    TWD_BCN_BRC_OPTIONS_PARAM              =           GET_BIT | TWD_MODULE_PARAM | TWD_BCN_BRC_OPTIONS_PARAM_ID,

    /* Site manager section */
    SITE_MGR_DESIRED_BSSID_PARAM                    = SET_BIT           | SITE_MGR_MODULE_PARAM | 0x02, 
    SITE_MGR_DESIRED_SSID_PARAM                     = SET_BIT           | SITE_MGR_MODULE_PARAM | 0x03,
    SITE_MGR_DESIRED_BSS_TYPE_PARAM                 = SET_BIT           | SITE_MGR_MODULE_PARAM | 0x04,
    SITE_MGR_DESIRED_MODULATION_TYPE_PARAM          = SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x08,
    SITE_MGR_DESIRED_BEACON_INTERVAL_PARAM          = SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x09,
    SITE_MGR_CURRENT_RADIO_TYPE_PARAM               =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x0D,

    SITE_MGR_CURRENT_SSID_PARAM                     =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x0F,
    SITE_MGR_CURRENT_RATE_PAIR_PARAM                =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x10,
    SITE_MGR_CURRENT_MODULATION_TYPE_PARAM          =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x11,
    SITE_MGR_CURRENT_SIGNAL_PARAM                   = SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x12,
    SITE_MGR_PRIMARY_SITE_PARAM                     =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x15,
    SITE_MGR_DESIRED_DOT11_MODE_PARAM               = SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x18,
    SITE_MGR_OPERATIONAL_MODE_PARAM                 =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x19,
    SITE_MGR_CURRENT_SLOT_TIME_PARAM                =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x1C,
    SITE_MGR_CURRENT_PREAMBLE_TYPE_PARAM            =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x1D,
    SITE_MGR_BUILT_IN_TEST_STATUS_PARAM             =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x1E,
    SITE_MGR_CONFIGURATION_PARAM                        = SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x1F,
    SITE_MGR_AP_TX_POWER_PARAM                      =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x23,
    SITE_MGR_DESIRED_TX_RATE_PRCT_SET               = SET_BIT           | SITE_MGR_MODULE_PARAM | 0x26,
    SITE_MGR_DESIRED_RSSI_THRESHOLD_SET             = SET_BIT           | SITE_MGR_MODULE_PARAM | 0x27,

    SITE_MGR_SITE_ENTRY_BY_INDEX                        =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x28,
    SITE_MGR_CUR_NUM_OF_SITES                       =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x29,
    SITE_MGR_CURRENT_TSF_TIME_STAMP                 =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x2A,    
    SITE_MGR_GET_SELECTED_BSSID_INFO                =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x2B,
    SITE_MGR_DESIRED_CONS_TX_ERRORS_THREH           = SET_BIT | GET_BIT | SITE_MGR_MODULE_PARAM | 0x2C,
    SITE_MGR_SUPPORTED_NETWORK_TYPES                =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x2D,
    SITE_MGR_CURRENT_BSSID_PARAM                        =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x2F,
    SITE_MGR_LAST_RX_RATE_PARAM                     =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x30,
    SITE_MGR_LAST_BEACON_BUF_PARAM                  =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x31,
    SITE_MGR_CURRENT_BSS_TYPE_PARAM                 =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x33,
    SITE_MGR_GET_SELECTED_BSSID_INFO_EX             =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x37,    
    SITE_MGR_RADIO_BAND_PARAM                       =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x39,
	SITE_MGR_GET_STATS		                        =           GET_BIT | SITE_MGR_MODULE_PARAM | 0x43,

    /* MLME section */
    MLME_BEACON_RECV                                =           GET_BIT | MLME_SM_MODULE_PARAM  | 0x01,

    /* SME SM section */
    SME_DESIRED_BSSID_PARAM                         = SET_BIT | GET_BIT | SME_MODULE_PARAM   | 0x02,

    SME_DESIRED_BSS_TYPE_PARAM                      = SET_BIT | GET_BIT | SME_MODULE_PARAM   | 0x05,
    SME_CONNECTION_STATUS_PARAM                     =           GET_BIT | SME_MODULE_PARAM   | 0x06,

    /* Scan concentrator section */

    /* Scan Manager module */
    
    /* Connection section */
    CONN_SELF_TIMEOUT_PARAM                         = SET_BIT | GET_BIT | CONN_MODULE_PARAM | 0x01,

    /* Auth section */
    AUTH_RESPONSE_TIMEOUT_PARAM                     = SET_BIT | GET_BIT | AUTH_MODULE_PARAM | 0x01,
    AUTH_COUNTERS_PARAM                             =           GET_BIT | AUTH_MODULE_PARAM | 0x02,

    /* Assoc section */
    ASSOC_RESPONSE_TIMEOUT_PARAM                        = SET_BIT | GET_BIT | ASSOC_MODULE_PARAM | 0x01,
    ASSOC_COUNTERS_PARAM                                =           GET_BIT | ASSOC_MODULE_PARAM | 0x02,
    ASSOC_ASSOCIATION_INFORMATION_PARAM                 =           GET_BIT | ASSOC_MODULE_PARAM | 0x03,
    ASSOC_ASSOCIATION_REQ_PARAM                        	=           GET_BIT | ASSOC_MODULE_PARAM | 0x04,
    ASSOC_ASSOCIATION_RESP_PARAM                        =           GET_BIT | ASSOC_MODULE_PARAM | 0x05,

    /* RSN section */
    RSN_PRIVACY_OPTION_IMPLEMENTED_PARAM            =           GET_BIT | RSN_MODULE_PARAM | 0x01,
    RSN_KEY_PARAM                                       = SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x02,
    RSN_SECURITY_STATE_PARAM                            =           GET_BIT | RSN_MODULE_PARAM | 0x03,
    RSN_AUTH_ENCR_CAPABILITY                            =           GET_BIT | RSN_MODULE_PARAM | 0x11,
    RSN_PMKID_LIST                                      = SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x12,
    RSN_WPA_PROMOTE_AVAILABLE_OPTIONS               =           GET_BIT | RSN_MODULE_PARAM | 0x13,
    RSN_WPA_PROMOTE_OPTIONS                         = SET_BIT           | RSN_MODULE_PARAM | 0x14,
    RSN_PRE_AUTH_STATUS                             =           GET_BIT | RSN_MODULE_PARAM | 0x15,
    RSN_EAP_TYPE                                        = SET_BIT | GET_BIT | RSN_MODULE_PARAM | 0x16,
    WPA_801_1X_AKM_EXISTS                               =           GET_BIT | RSN_MODULE_PARAM | 0x17,

    /* RX data section */
    RX_DATA_COUNTERS_PARAM                          =           GET_BIT | RX_DATA_MODULE_PARAM | 0x01,
    RX_DATA_EXCLUDE_UNENCRYPTED_PARAM               = SET_BIT | GET_BIT | RX_DATA_MODULE_PARAM | 0x02,
    RX_DATA_EXCLUDE_BROADCAST_UNENCRYPTED_PARAM     = SET_BIT | GET_BIT | RX_DATA_MODULE_PARAM | 0x03,
    RX_DATA_RATE_PARAM                              =           GET_BIT | RX_DATA_MODULE_PARAM | 0x08,
    RX_DATA_GENERIC_ETHERTYPE_PARAM                 = SET_BIT           | RX_DATA_MODULE_PARAM | 0x09,
     
    /* TX data section */
    TX_CTRL_GET_MEDIUM_USAGE_THRESHOLD              = SET_BIT | GET_BIT | TX_CTRL_MODULE_PARAM | 0x04,
    TX_CTRL_POLL_AP_PACKETS_FROM_AC                     = SET_BIT           | TX_CTRL_MODULE_PARAM | 0x05,
    TX_CTRL_REPORT_TS_STATISTICS                        =           GET_BIT | TX_CTRL_MODULE_PARAM | 0x06,
    TX_CTRL_GET_DATA_FRAME_COUNTER                  =           GET_BIT | TX_CTRL_MODULE_PARAM | 0x07,
    
    /* CTRL data section */
    CTRL_DATA_RATE_CONTROL_ENABLE_PARAM             = SET_BIT           | CTRL_DATA_MODULE_PARAM | 0x02,
    CTRL_DATA_CURRENT_BSSID_PARAM                   =           GET_BIT | CTRL_DATA_MODULE_PARAM | 0x03,
    CTRL_DATA_CURRENT_SUPPORTED_RATE_MASK_PARAM     =           GET_BIT | CTRL_DATA_MODULE_PARAM | 0x05,
    CTRL_DATA_CURRENT_PREAMBLE_TYPE_PARAM           =           GET_BIT | CTRL_DATA_MODULE_PARAM | 0x06,
    CTRL_DATA_CURRENT_PROTECTION_STATUS_PARAM       = SET_BIT | GET_BIT | CTRL_DATA_MODULE_PARAM | 0x07,
    CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM         = SET_BIT | GET_BIT | CTRL_DATA_MODULE_PARAM | 0x09,
    CTRL_DATA_CURRENT_RTS_CTS_STATUS_PARAM          = SET_BIT | GET_BIT | CTRL_DATA_MODULE_PARAM | 0x0A,
    CTRL_DATA_GET_USER_PRIORITY_OF_STREAM           = SET_BIT | GET_BIT | CTRL_DATA_MODULE_PARAM | 0x10,
    CTRL_DATA_TSRS_PARAM                            = SET_BIT           | CTRL_DATA_MODULE_PARAM | 0x11,

    /* regulatory domain section */
    REGULATORY_DOMAIN_UPDATE_CHANNEL_VALIDITY           = SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x09,
    REGULATORY_DOMAIN_CURRENT_TX_ATTENUATION_PARAM      =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x0A,
    REGULATORY_DOMAIN_TEMPORARY_TX_ATTENUATION_PARAM    = SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x0B,
    REGULATORY_DOMAIN_ALL_SUPPORTED_CHANNELS            =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x11,
    REGULATORY_DOMAIN_IS_DFS_CHANNEL                    =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x13,
    REGULATORY_DOMAIN_DISCONNECT_PARAM                  = SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x14,
    REGULATORY_DOMAIN_TX_POWER_AFTER_SELECTION_PARAM    = SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x15,
    REGULATORY_DOMAIN_COUNTRY_PARAM                     = SET_BIT | GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x16,
    REGULATORY_DOMAIN_POWER_CAPABILITY_PARAM            =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x17,
    REGULATORY_DOMAIN_SET_POWER_CONSTRAINT_PARAM        = SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x18,
    REGULATORY_DOMAIN_IS_CHANNEL_SUPPORTED              =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x19,
    REGULATORY_DOMAIN_EXTERN_TX_POWER_PREFERRED         = SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x1A,
    REGULATORY_DOMAIN_SET_CHANNEL_VALIDITY              = SET_BIT |           REGULATORY_DOMAIN_MODULE_PARAM | 0x1B,
    REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES             =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x1C,
    REGULATORY_DOMAIN_IS_COUNTRY_FOUND                  =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x1D,
    REGULATORY_DOMAIN_TIME_TO_COUNTRY_EXPIRY            =           GET_BIT | REGULATORY_DOMAIN_MODULE_PARAM | 0x1E,
    /* measurement section */
     
#ifdef XCC_MODULE_INCLUDED
    /* XCC */    
    XCC_ROGUE_AP_DETECTED                               = SET_BIT           | XCC_MANAGER_MODULE_PARAM | 0x02,
    XCC_REPORT_ROGUE_APS                                = SET_BIT           | XCC_MANAGER_MODULE_PARAM | 0x03,
    XCC_AUTH_SUCCESS                                    = SET_BIT           | XCC_MANAGER_MODULE_PARAM | 0x04,
    XCC_CCKM_REQUEST                                    = SET_BIT           | XCC_MANAGER_MODULE_PARAM | 0x05,
    XCC_CCKM_RESULT                                 = SET_BIT           | XCC_MANAGER_MODULE_PARAM | 0x06,
    XCC_ENABLED                                     = SET_BIT | GET_BIT | XCC_MANAGER_MODULE_PARAM | 0x07,
    XCC_CURRENT_AP_SUPPORTED_VERSION                =           GET_BIT | XCC_MANAGER_MODULE_PARAM | 0x08,
#endif

    /* Roaming manager */
    
    /* Parameters used for DEBUG */
    ROAMING_MNGR_TRIGGER_EVENT                      = SET_BIT           | ROAMING_MANAGER_MODULE_PARAM | 0x03,
    ROAMING_MNGR_CONN_STATUS                        = SET_BIT           | ROAMING_MANAGER_MODULE_PARAM | 0x04, 
#ifdef TI_DBG
    ROAMING_MNGR_PRINT_STATISTICS                   =           GET_BIT | ROAMING_MANAGER_MODULE_PARAM | 0x05,
    ROAMING_MNGR_RESET_STATISTICS                   =           GET_BIT | ROAMING_MANAGER_MODULE_PARAM | 0x06,
    ROAMING_MNGR_PRINT_CURRENT_STATUS               =           GET_BIT | ROAMING_MANAGER_MODULE_PARAM | 0x07,
    ROAMING_MNGR_PRINT_CANDIDATE_TABLE              =           GET_BIT | ROAMING_MANAGER_MODULE_PARAM | 0x08,
#endif

    /* Soft Gemini params */
     
    /* QOS manager params */
    QOS_MNGR_SHORT_RETRY_LIMIT_PARAM            	= SET_BIT | GET_BIT | QOS_MANAGER_PARAM | 0x01,
    QOS_MNGR_LONG_RETRY_LIMIT_PARAM                 = SET_BIT | GET_BIT | QOS_MANAGER_PARAM | 0x02,
    QOS_PACKET_BURST_ENABLE                         = SET_BIT | GET_BIT | QOS_MANAGER_PARAM | 0x03,
    QOS_MNGR_SET_SITE_PROTOCOL            			= SET_BIT |           QOS_MANAGER_PARAM | 0x04,
    QOS_MNGR_SET_802_11_POWER_SAVE_STATUS           = SET_BIT |           QOS_MANAGER_PARAM | 0x05,
    QOS_MNGR_SET_OPERATIONAL_MODE                   = SET_BIT |           QOS_MANAGER_PARAM | 0x08,
    QOS_MNGR_CURRENT_PS_MODE                        = SET_BIT | GET_BIT | QOS_MANAGER_PARAM | 0x09,
    QOS_MNGR_ACTIVE_PROTOCOL                        =           GET_BIT | QOS_MANAGER_PARAM | 0x0A,
    QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC               = SET_BIT | GET_BIT | QOS_MANAGER_PARAM | 0x0C,
    QOS_MNGR_RESEND_TSPEC_REQUEST                   = SET_BIT           | QOS_MANAGER_PARAM | 0x0D,

    /* Power Manager params */
    POWER_MGR_DISABLE_PRIORITY                      = SET_BIT |           POWER_MANAGER_PARAM | 0x02,   
    POWER_MGR_ENABLE_PRIORITY                       = SET_BIT |           POWER_MANAGER_PARAM | 0x03
    
}   EInternalParam;


#endif /* __PARAM_MGR_H__ */
