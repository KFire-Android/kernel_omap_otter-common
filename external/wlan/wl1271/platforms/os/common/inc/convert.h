/*
 * convert.h
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

#if !defined _CONVERT_H
#define _CONVERT_H

#include "TWDriver.h"
#include "STADExternalIf.h"
#include "InternalCmdCodes.h"
#include "cu_common.h"
#include "TWDriverScan.h"

/***********************/
/* General definitions */
/***********************/

#define TIWLN_802_11_SUPPORTED_RATES                    SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM
#define TIWLN_802_11_SUPPORTED_RATES_SET                SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM
#define TIWLN_802_11_CURRENT_RATES_GET                  SITE_MGR_CURRENT_TX_RATE_PARAM
#define TIWLN_802_11_CHANNEL_GET                        SITE_MGR_CURRENT_CHANNEL_PARAM /* wext in linux */
#define TIWLN_REG_DOMAIN_ENABLE_DISABLE_802_11D         REGULATORY_DOMAIN_ENABLE_DISABLE_802_11D
#define TIWLN_REG_DOMAIN_ENABLE_DISABLE_802_11H         REGULATORY_DOMAIN_ENABLE_DISABLE_802_11H
#define TIWLN_REG_DOMAIN_GET_802_11D                    REGULATORY_DOMAIN_ENABLED_PARAM
#define TIWLN_REG_DOMAIN_GET_802_11H                    REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM
#define TIWLN_REG_DOMAIN_GET_COUNTRY_2_4	            REGULATORY_DOMAIN_COUNTRY_2_4_PARAM
#define TIWLN_REG_DOMAIN_SET_COUNTRY_2_4	            REGULATORY_DOMAIN_COUNTRY_2_4_PARAM
#define TIWLN_REG_DOMAIN_GET_COUNTRY_5	                REGULATORY_DOMAIN_COUNTRY_5_PARAM
#define TIWLN_REG_DOMAIN_SET_COUNTRY_5	                REGULATORY_DOMAIN_COUNTRY_5_PARAM
#define TIWLN_REG_DOMAIN_SET_DFS_RANGE	                REGULATORY_DOMAIN_DFS_CHANNELS_RANGE
#define TIWLN_REG_DOMAIN_GET_DFS_RANGE	                REGULATORY_DOMAIN_DFS_CHANNELS_RANGE
#define TIWLN_802_11_POWER_MODE_GET	                    POWER_MGR_POWER_MODE
#define TIWLN_802_11_POWER_MODE_SET	                    POWER_MGR_POWER_MODE
#define TIWLN_802_11_RSSI                       	    TWD_RSSI_LEVEL_PARAM
#define TIWLN_802_11_TX_POWER_DBM_GET           	    REGULATORY_DOMAIN_CURRENT_TX_POWER_IN_DBM_PARAM
#define TIWLN_802_11_POWER_MGR_PROFILE          	    POWER_MGR_POWER_MODE
                                                        
#define TIWLN_SHORT_SLOT_GET	                        SITE_MGR_DESIRED_SLOT_TIME_PARAM
#define TIWLN_SHORT_SLOT_SET	                        SITE_MGR_DESIRED_SLOT_TIME_PARAM
#define TIWLN_IBSS_PROTECTION_GET               	    CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM /* not implemented in CUDK */
#define TIWLN_IBSS_PROTECTION_SET	                    CTRL_DATA_CURRENT_IBSS_PROTECTION_PARAM /* not implemented in CUDK */
#define TIWLN_802_11_MIXED_MODE_SET	                    RSN_MIXED_MODE
#define TIWLN_802_11_MIXED_MODE_GET	                    RSN_MIXED_MODE
                                                        
#define TIWLN_802_11_GET_AP_QOS_PARAMS	                QOS_MNGR_AP_QOS_PARAMETERS
#define TIWLN_802_11_GET_AP_QOS_CAPABILITIES	        SITE_MGR_GET_AP_QOS_CAPABILITIES
#define TIWLN_802_11_ADD_TSPEC	                        QOS_MNGR_ADD_TSPEC_REQUEST
#define TIWLN_802_11_GET_TSPEC_PARAMS	                QOS_MNGR_OS_TSPEC_PARAMS
#define TIWLN_802_11_DELETE_TSPEC               	    QOS_MNGR_DEL_TSPEC_REQUEST
#define TIWLN_802_11_GET_CURRENT_AC_STATUS	            QOS_MNGR_AC_STATUS
#define TIWLN_802_11_SET_MEDIUM_USAGE_THRESHOLD 	    TX_CTRL_SET_MEDIUM_USAGE_THRESHOLD
#define TIWLN_802_11_GET_MEDIUM_USAGE_THRESHOLD 	    TX_CTRL_GET_MEDIUM_USAGE_THRESHOLD
#define TIWLN_802_11_GET_DESIRED_PS_MODE                QOS_MNGR_GET_DESIRED_PS_MODE
#define TIWLN_802_11_SET_RX_TIMEOUT                     QOS_SET_RX_TIME_OUT
#define TIWLN_802_11_POWER_LEVEL_DEFAULT_GET            POWER_MGR_POWER_LEVEL_DEFAULT
#define TIWLN_802_11_POWER_LEVEL_DEFAULT_SET            POWER_MGR_POWER_LEVEL_DEFAULT
#define TIWLN_802_11_POWER_LEVEL_PS_SET                 POWER_MGR_POWER_LEVEL_PS
#define TIWLN_802_11_POWER_LEVEL_PS_GET                 POWER_MGR_POWER_LEVEL_PS
#define TIWLN_GET_POWER_CONSUMPTION_STATISTICS          POWER_MGR_GET_POWER_CONSUMPTION_STATISTICS
#define TIWLN_802_11_BEACON_FILTER_DESIRED_STATE_SET    SITE_MGR_BEACON_FILTER_DESIRED_STATE_PARAM
#define TIWLN_802_11_BEACON_FILTER_DESIRED_STATE_GET    SITE_MGR_BEACON_FILTER_DESIRED_STATE_PARAM
#define TIWLN_802_11_POWER_LEVEL_DOZE_MODE_GET          POWER_MGR_POWER_LEVEL_DOZE_MODE
#define TIWLN_802_11_POWER_LEVEL_DOZE_MODE_SET          POWER_MGR_POWER_LEVEL_DOZE_MODE
#define TIWLN_802_11_SHORT_PREAMBLE_GET                 SITE_MGR_DESIRED_PREAMBLE_TYPE_PARAM
#define TIWLN_802_11_SHORT_PREAMBLE_SET                 SITE_MGR_DESIRED_PREAMBLE_TYPE_PARAM
#define TIWLN_ENABLE_DISABLE_RX_DATA_FILTERS            RX_DATA_ENABLE_DISABLE_RX_DATA_FILTERS
#define TIWLN_ADD_RX_DATA_FILTER                        RX_DATA_ADD_RX_DATA_FILTER
#define TIWLN_REMOVE_RX_DATA_FILTER                     RX_DATA_REMOVE_RX_DATA_FILTER
#define TIWLN_GET_RX_DATA_FILTERS_STATISTICS            RX_DATA_GET_RX_DATA_FILTERS_STATISTICS
#define TIWLN_GET_RX_DATA_RATE                          SITE_MGR_CURRENT_RX_RATE_PARAM
#define TIWLN_REPORT_MODULE_SET                         REPORT_MODULE_TABLE_PARAM
#define TIWLN_REPORT_MODULE_GET                         REPORT_MODULE_TABLE_PARAM
#define TIWLN_REPORT_SEVERITY_SET                       REPORT_SEVERITY_TABLE_PARAM
#define TIWLN_REPORT_SEVERITY_GET                       REPORT_SEVERITY_TABLE_PARAM
#define TIWLN_DISPLAY_STATS                             DEBUG_ACTIVATE_FUNCTION
#define TIWLN_RATE_MNG_SET                              SITE_MGRT_SET_RATE_MANAGMENT
#define TIWLN_RATE_MNG_GET                              SITE_MGRT_GET_RATE_MANAGMENT
#define TIWLN_802_11_GET_SELECTED_BSSID_INFO            SITE_MGR_GET_SELECTED_BSSID_INFO
#define TIWLN_802_11_TX_STATISTICS                      TX_CTRL_COUNTERS_PARAM
#define TIWLN_802_11_SET_TRAFFIC_INTENSITY_THRESHOLDS   CTRL_DATA_TRAFFIC_INTENSITY_THRESHOLD
#define TIWLN_802_11_GET_TRAFFIC_INTENSITY_THRESHOLDS   CTRL_DATA_TRAFFIC_INTENSITY_THRESHOLD
#define TIWLN_802_11_TOGGLE_TRAFFIC_INTENSITY_EVENTS    CTRL_DATA_TOGGLE_TRAFFIC_INTENSITY_EVENTS
#define TIWLN_802_11_GET_PRIMARY_BSSID_INFO             SITE_MGR_PRIMARY_SITE_PARAM /* not implemented in CUDK */
#define TIWLN_ENABLE_DISABLE_RX_DATA_FILTERS            RX_DATA_ENABLE_DISABLE_RX_DATA_FILTERS
#define TIWLN_ADD_RX_DATA_FILTER                        RX_DATA_ADD_RX_DATA_FILTER
#define TIWLN_REMOVE_RX_DATA_FILTER                     RX_DATA_REMOVE_RX_DATA_FILTER
#define TIWLN_GET_RX_DATA_FILTERS_STATISTICS            RX_DATA_GET_RX_DATA_FILTERS_STATISTICS /* not implemented in CUDK */
#define TIWLN_802_11_START_APP_SCAN_SET                 SCAN_CNCN_START_APP_SCAN           
#define TIWLN_802_11_STOP_APP_SCAN_SET                  SCAN_CNCN_STOP_APP_SCAN
#define TIWLN_802_11_SCAN_POLICY_PARAM_SET              SCAN_MNGR_SET_CONFIGURATION
#define TIWLN_802_11_SCAN_BSS_LIST_GET                  SCAN_MNGR_BSS_LIST_GET
#define TIWLN_802_11_SET_QOS_PARAMS                     QOS_MNGR_SET_OS_PARAMS
#define TIWLN_802_11_CONFIG_TX_CLASS                    CTRL_DATA_CLSFR_CONFIG
#define TIWLN_802_11_REMOVE_CLSFR_ENTRY                 CTRL_DATA_CLSFR_REMOVE_ENTRY
#define TIWLN_DCO_ITRIM_PARAMS                          TWD_DCO_ITRIM_PARAMS


/********************/
/* Type definitions */
/********************/

typedef TRates                              rates_t;
typedef EDraftNumber                        draftNumber_t;
typedef TCountry                            country_t;
typedef TDfsChannelRange                    DFS_ChannelRange_t;
typedef ESlotTime                           slotTime_e;
typedef TRxDataFilterRequest                TIWLAN_DATA_FILTER_REQUEST;
typedef TCuCommon_RxDataFilteringStatistics TIWLAN_DATA_FILTER_STATISTICS;
typedef TScanParams                         scan_Params_t;
typedef TScanNormalChannelEntry             scan_normalChannelEntry_t;
typedef EScanEtCondition                    scan_ETCondition_e;
typedef TScanSpsChannelEntry                scan_SPSChannelEntry_t;
typedef EScanType                           scan_Type_e;
typedef ERadioBand                          radioBand_e;
typedef ERateMask                           rateMask_e;
typedef TSsid                               ssid_t;
typedef TScanPolicy                         scan_Policy_t;
typedef TScanBandPolicy                     scan_bandPolicy_t;
typedef TScanProbReqParams                  scan_probReqParams_t;
typedef TScanBasicMethodParams              scan_basicMethodParams_t;
typedef TScanTidTriggeredMethodParams       scan_TidTriggeredMethodParams_t;
typedef TScanSPSMethodParams                scan_SPSMethodParams_t;
typedef TScanMethod                         scan_Method_t;
typedef TClsfrTableEntry                    clsfr_tableEntry_t;
typedef TIpPort                             IP_Port_t;

#endif /* _CONVERT_H */
