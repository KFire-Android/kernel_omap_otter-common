/*
 * coreDefaultParams.h
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

#ifndef _CORE_DEFAULT_PARAMS_H
#define _CORE_DEFAULT_PARAMS_H
                  
#if defined(__ARMCC__) 
#include "public_infoele.h"
#endif
/************************************/
/*      Min, Max & Default values   */
/************************************/

/* In this section are defined default, min & max values for parameters, according to the MIB */
/* This values are used as following:
        - By the OS abstraction layer in order to fill the init table with the default values
        if the NVRAM/Registry value for the parameter is invalid
        - By the core modules in order to perform validity check upon setting a parameter. */

/* This is the max possible string length from INI file */
#define BEACON_FILTER_STRING_MAX_LEN            300

#define SITE_MGR_CHANNEL_MIN                    1
/* Band A*/
#define SITE_MGR_CHANNEL_A_MIN                  36
/* Band B&G*/
#define SITE_MGR_CHANNEL_B_G_MAX                14
#define SITE_MGR_CHANNEL_MAX                    201
#define SITE_MGR_CHANNEL_DEF                    11

#define SITE_MGR_DOT_11_MODE_MIN                DOT11_B_MODE
#define SITE_MGR_DOT_11_MODE_MAX                DOT11_DUAL_MODE
#define SITE_MGR_DOT_11_MODE_DEF                DOT11_G_MODE

#define SITE_MGR_BSS_TYPE_DEF                   BSS_INFRASTRUCTURE

#define SITE_MGR_BEACON_INTERVAL_MIN            1
#define SITE_MGR_BEACON_INTERVAL_MAX            65535
#define SITE_MGR_BEACON_INTERVAL_DEF            200

/* number of events to wake up on -
    For WakeOnBeacon- Aging interval =  SITE_MGR_NUMBER_OF_EVENTS_BEFORE_AGING * BeaconInterval
    For WakeOnDtim - Aging interval =  SITE_MGR_NUMBER_OF_EVENTS_BEFORE_AGING * BeaconInterval * DtimPeriod */
#define SITE_MGR_NUMBER_OF_EVENTS_BEFORE_AGING_MIN  2
#define SITE_MGR_NUMBER_OF_EVENTS_BEFORE_AGING_MAX  20
#define SITE_MGR_NUMBER_OF_EVENTS_BEFORE_AGING_DEF  10

#define SITE_MGR_NUMBER_OF_TX_FAILURE_BEFORE_AGING_MIN  3
#define SITE_MGR_NUMBER_OF_TX_FAILURE_BEFORE_AGING_MAX  100
#define SITE_MGR_NUMBER_OF_TX_FAILURE_BEFORE_AGING_DEF  6

/* In seconds */
#define SITE_MGR_ROAMING_STATS_RESET_TIMEOUT_MIN   5
#define SITE_MGR_ROAMING_STATS_RESET_TIMEOUT_MAX   60
#define SITE_MGR_ROAMING_STATS_RESET_TIMEOUT_DEF   10

#define SITE_MGR_LNA_BEACON_INT_COUNT_MIN       2   
#define SITE_MGR_LNA_BEACON_INT_COUNT_MAX       0xFFFF
#define SITE_MGR_LNA_BEACON_INT_COUNT_DEF       3

#define SITE_MGR_LNA_PD_THRESHOLD_LOW_MIN       0x00    
#define SITE_MGR_LNA_PD_THRESHOLD_LOW_MAX       0xff    
#define SITE_MGR_LNA_PD_THRESHOLD_LOW_DEF       0x90    

#define SITE_MGR_LNA_PD_THRESHOLD_HIGH_MIN      0x00    
#define SITE_MGR_LNA_PD_THRESHOLD_HIGH_MAX      0xff    
#define SITE_MGR_LNA_PD_THRESHOLD_HIGH_DEF      0xD5    

#define SITE_MGR_LNA_EN_DINAMYC_TX_ALGO_DEF     0   
#define SITE_MGR_LNA_EN_DINAMYC_TX_ALGO_MAX     1
#define SITE_MGR_LNA_EN_DINAMYC_TX_ALGO_MIN     0

#define SITE_MGR_PREAMBLE_TYPE_DEF              PREAMBLE_SHORT

#define SITE_MGR_EXTERNAL_MODE_MIN              0
#define SITE_MGR_EXTERNAL_MODE_MAX              1
#define SITE_MGR_EXTERNAL_MODE_DEF              0

#define SITE_MGR_PERFORM_BUILD_IN_TEST_RECOVEY_MIN      TI_FALSE
#define SITE_MGR_PERFORM_BUILD_IN_TEST_RECOVEY_MAX      TI_TRUE
#define SITE_MGR_PERFORM_BUILD_IN_TEST_RECOVEY_DEF      TI_FALSE

#define SITE_MGR_WiFiAdHoc_MIN                  0
#define SITE_MGR_WiFiAdHoc_MAX                  1
#define SITE_MGR_WiFiAdHoc_DEF                  0

#define SITE_MGR_RX_LEVEL_TABLE_SIZE_DEF        44

/* due to the fact we use the site table only to connect we need just 2 entries each table */
#define MAX_SITES_BG_BAND   2
#define MAX_SITES_A_BAND    2
#define NUM_OF_SITE_TABLE   2

/* Beacon broadcast options */
#define BCN_RX_TIMEOUT_DEF_VALUE 10000
#define BCN_RX_TIMEOUT_MIN_VALUE 1
#define BCN_RX_TIMEOUT_MAX_VALUE 65535
    
#define BROADCAST_RX_TIMEOUT_DEF_VALUE 20000
#define BROADCAST_RX_TIMEOUT_MIN_VALUE 1 
#define BROADCAST_RX_TIMEOUT_MAX_VALUE 65535

#define RX_BROADCAST_IN_PS_DEF_VALUE 1
#define RX_BROADCAST_IN_PS_MIN_VALUE 0
#define RX_BROADCAST_IN_PS_MAX_VALUE 1

#define CONSECUTIVE_PS_POLL_FAILURE_DEF 20
#define CONSECUTIVE_PS_POLL_FAILURE_MIN 1
#define CONSECUTIVE_PS_POLL_FAILURE_MAX 100

#define PS_POLL_FAILURE_PERIOD_DEF 20
#define PS_POLL_FAILURE_PERIOD_MIN 0       /* '0' is disabled */
#define PS_POLL_FAILURE_PERIOD_MAX 60000

/*---------------------------*/
/*  Classifier parameters    */
/*---------------------------*/

/* 1 - Dtag, 2 - Port, 3 - IP & port */ 
#define CLSFR_TYPE_MIN                          1 
#define CLSFR_TYPE_DEF                      1
#define CLSFR_TYPE_MAX                      3

/* general values of D-tags */
#define CLASSIFIER_DTAG_MIN                 0 
#define CLASSIFIER_DTAG_MAX                 7
#define CLASSIFIER_DTAG_DEF                 0 

/* general values of code points in 
the DSCP classification table*/
#define CLASSIFIER_CODE_POINT_MIN       0 
#define CLASSIFIER_CODE_POINT_MAX       63
#define CLASSIFIER_CODE_POINT_DEF       0

/* general values of port numbers */
#define CLASSIFIER_PORT_MIN                 1 
#define CLASSIFIER_PORT_MAX                 65535
#define CLASSIFIER_PORT_DEF                 1024 

/* General values of IP addresses */
/* TBD according to spec!*/
#define CLASSIFIER_IPADDRESS_MIN                0x0
/* MY IP ... TBD according to spec!*/
#define CLASSIFIER_IPADDRESS_DEF                0x0A030DC4
/* TBD according to spec!*/
#define CLASSIFIER_IPADDRESS_MAX                0xFFFFFFFF

/* DSCP (differentiated services code 
point) classifier parameters  
--------------------------------*/
/* number of classifier entries in the 
   classification table (in case of DSCP classifier) */

#define NUM_OF_CODE_POINTS_MIN              0
#define NUM_OF_CODE_POINTS_MAX              16
#define NUM_OF_CODE_POINTS_DEF              4

/* def values of code points in the DSCP classification table*/
#define DSCP_CLASSIFIER_CODE_POINT_DEF  0x0
#define DSCP_CLASSIFIER_CODE_POINT_00  0
#define DSCP_CLASSIFIER_CODE_POINT_01  8
#define DSCP_CLASSIFIER_CODE_POINT_02  40
#define DSCP_CLASSIFIER_CODE_POINT_03  56

/* def values of D-tags in the DSCP classification table*/
#define DSCP_CLASSIFIER_DTAG_DEF            0
#define DSCP_CLASSIFIER_DTAG_00            0
#define DSCP_CLASSIFIER_DTAG_01            1
#define DSCP_CLASSIFIER_DTAG_02            5
#define DSCP_CLASSIFIER_DTAG_03            6

/* Port Classifier parameters 
--------------------------------*/

/* number of entries in the classification table (in case of destination port classifier) */
#define NUM_OF_PORT_CLASSIFIERS_MIN         0
#define NUM_OF_PORT_CLASSIFIERS_MAX         16
#define NUM_OF_PORT_CLASSIFIERS_DEF         0

/* def values of port numbers in the destination port classification table*/
#define PORT_CLASSIFIER_PORT_DEF            5000

/* def values of D-tags in the destination port classification table*/
#define PORT_CLASSIFIER_DTAG_DEF            7

/* IP&Port Classifier parameters 
--------------------------------*/

/* number of active entries in the 
IP&Port classification table  */
#define NUM_OF_IPPORT_CLASSIFIERS_MIN       0
#define NUM_OF_IPPORT_CLASSIFIERS_MAX       16
#define NUM_OF_IPPORT_CLASSIFIERS_DEF       0

/* def values of IP addresses in the IP&Port classification table*/
#define IPPORT_CLASSIFIER_IPADDRESS_DEF 167972292

/* def values of port numbers in the IP&Port classification table*/
#define IPPORT_CLASSIFIER_PORT_DEF      5004

/* def values of D-tags in the IP&Port classification table*/
#define IPPORT_CLASSIFIER_DTAG_DEF      7

/* end of classifier parameters */

#define MAX_USER_PRIORITY                       7



#define  WME_ENABLED_MIN                        TI_FALSE
#define  WME_ENABLED_MAX                        TI_TRUE
#define  WME_ENABLED_DEF                        TI_TRUE

#define  QOS_TRAFFIC_ADM_CTRL_ENABLED_MIN       TI_FALSE
#define  QOS_TRAFFIC_ADM_CTRL_ENABLED_MAX       TI_TRUE 
#define  QOS_TRAFFIC_ADM_CTRL_ENABLED_DEF       TI_TRUE 

#define  QOS_DESIRED_PS_MODE_MIN            PS_SCHEME_LEGACY
#define  QOS_DESIRED_PS_MODE_MAX            MAX_PS_SCHEME
#define  QOS_DESIRED_PS_MODE_DEF            PS_SCHEME_UPSD_TRIGGER

#define  QOS_TAG_ZERO_PRIO_MIN                  TI_FALSE
#define  QOS_TAG_ZERO_PRIO_MAX                  TI_TRUE
#define  QOS_TAG_ZERO_PRIO_DEF                  TI_TRUE


/* for the AC */
#define  QOS_TX_OP_CONTINUATION_MIN             0
#define  QOS_TX_OP_CONTINUATION_MAX             1
#define  QOS_TX_OP_CONTINUATION_DEF            1

#define  QOS_TX_OP_LIMIT_MIN                   0
#define  QOS_TX_OP_LIMIT_MAX                   32000     
#define  QOS_TX_OP_LIMIT_DEF                   0

/* for packet burst in non-qos protocol */
#define  QOS_PACKET_BURST_ENABLE_MIN             0
#define  QOS_PACKET_BURST_ENABLE_DEF            0
#define  QOS_PACKET_BURST_ENABLE_MAX             1

#define  QOS_PACKET_BURST_TXOP_LIMIT_MIN         0
#define  QOS_PACKET_BURST_TXOP_LIMIT_MAX         1000     
#define  QOS_PACKET_BURST_TXOP_LIMIT_DEF         93

#define  QOS_RX_TIMEOUT_PS_POLL_MIN                0
#define  QOS_RX_TIMEOUT_PS_POLL_MAX             200000
#define  QOS_RX_TIMEOUT_PS_POLL_DEF                15

#define  QOS_RX_TIMEOUT_UPSD_MIN                   0
#define  QOS_RX_TIMEOUT_UPSD_MAX                200000
#define  QOS_RX_TIMEOUT_UPSD_DEF                   15

#define  QOS_MSDU_LIFE_TIME_MIN                0
#define  QOS_MSDU_LIFE_TIME_MAX                1024

#define  QOS_MSDU_LIFE_TIME_BE_MIN              QOS_MSDU_LIFE_TIME_MIN
#define  QOS_MSDU_LIFE_TIME_BE_MAX              QOS_MSDU_LIFE_TIME_MAX
#define  QOS_MSDU_LIFE_TIME_BE_DEF              512
#define  QOS_MSDU_LIFE_TIME_BE_DEF_WIFI_MODE    512

#define  QOS_MSDU_LIFE_TIME_BK_MIN              QOS_MSDU_LIFE_TIME_MIN
#define  QOS_MSDU_LIFE_TIME_BK_MAX              QOS_MSDU_LIFE_TIME_MAX
#define  QOS_MSDU_LIFE_TIME_BK_DEF              100
#define  QOS_MSDU_LIFE_TIME_BK_DEF_WIFI_MODE    512

#define  QOS_MSDU_LIFE_TIME_VI_MIN              QOS_MSDU_LIFE_TIME_MIN
#define  QOS_MSDU_LIFE_TIME_VI_MAX              QOS_MSDU_LIFE_TIME_MAX
#define  QOS_MSDU_LIFE_TIME_VI_DEF              100
#define  QOS_MSDU_LIFE_TIME_VI_DEF_WIFI_MODE    512

#define  QOS_MSDU_LIFE_TIME_VO_MIN              QOS_MSDU_LIFE_TIME_MIN
#define  QOS_MSDU_LIFE_TIME_VO_MAX              QOS_MSDU_LIFE_TIME_MAX
#define  QOS_MSDU_LIFE_TIME_VO_DEF              40
#define  QOS_MSDU_LIFE_TIME_VO_DEF_WIFI_MODE    512

#define  QOS_WME_PS_MODE_BE_MIN                 PS_SCHEME_LEGACY
#define  QOS_WME_PS_MODE_BE_MAX                 MAX_PS_SCHEME
#define  QOS_WME_PS_MODE_BE_DEF                 PS_SCHEME_LEGACY

#define  QOS_WME_PS_MODE_BK_MIN                 PS_SCHEME_LEGACY
#define  QOS_WME_PS_MODE_BK_MAX                 MAX_PS_SCHEME
#define  QOS_WME_PS_MODE_BK_DEF                 PS_SCHEME_LEGACY

#define  QOS_WME_PS_MODE_VI_MIN                 PS_SCHEME_LEGACY
#define  QOS_WME_PS_MODE_VI_MAX                 MAX_PS_SCHEME
#define  QOS_WME_PS_MODE_VI_DEF                 PS_SCHEME_LEGACY

#define  QOS_WME_PS_MODE_VO_MIN                 PS_SCHEME_LEGACY
#define  QOS_WME_PS_MODE_VO_MAX                 MAX_PS_SCHEME
#define  QOS_WME_PS_MODE_VO_DEF                 PS_SCHEME_UPSD_TRIGGER


/* Minimum reserved Tx mem block in FW per AC: */
/***********************************************/
/* By default, partially optimize for Tx QoS on expense of Rx throughput */
#define  QOS_TX_BLKS_THRESHOLD_BK_DEF           0
#define  QOS_TX_BLKS_THRESHOLD_BE_DEF           20
#define  QOS_TX_BLKS_THRESHOLD_VI_DEF           20
#define  QOS_TX_BLKS_THRESHOLD_VO_DEF           20
/* In WiFi mode, fully optimized for Tx QoS on expense of Rx throughput */
#define  QOS_TX_BLKS_THRESHOLD_BK_DEF_WIFI_MODE 0
#define  QOS_TX_BLKS_THRESHOLD_BE_DEF_WIFI_MODE 30
#define  QOS_TX_BLKS_THRESHOLD_VI_DEF_WIFI_MODE 30
#define  QOS_TX_BLKS_THRESHOLD_VO_DEF_WIFI_MODE 30
/* In performance-boost, optimize for Rx throughput on expense of Tx QoS */
#define  QOS_TX_BLKS_THRESHOLD_BK_DEF_BOOST_MODE 0
#define  QOS_TX_BLKS_THRESHOLD_BE_DEF_BOOST_MODE 10
#define  QOS_TX_BLKS_THRESHOLD_VI_DEF_BOOST_MODE 10
#define  QOS_TX_BLKS_THRESHOLD_VO_DEF_BOOST_MODE 10

/* HW Rx mem-blocks Number */
#define  RX_MEM_BLKS_NUM_MIN                    20
#define  RX_MEM_BLKS_NUM_MAX                    120
#define  RX_MEM_BLKS_NUM_DEF                    50   /* By default, partially optimize for Tx QoS on expense of Rx throughput */
#define  RX_MEM_BLKS_NUM_DEF_WIFI_MODE          40   /* In WiFi mode, fully optimize for Tx QoS on expense of Rx throughput */
#define  RX_MEM_BLKS_NUM_DEF_BOOST_MODE         70   /* In performance-boost, optimize for Rx throughput on expense of Tx QoS */

/* WiFi mode on/off */
#define  WIFI_MODE_DEF                          0
#define  WIFI_MODE_MIN                          0
#define  WIFI_MODE_MAX                          1

/* Performance-boost mode (QoS/Speed tradeoff) */
#define  BOOST_MODE_OPTIMIZE_FOR_QOS            0   /* Optimize for Tx QoS on expense of Rx throughput */
#define  BOOST_MODE_OPTIMIZE_FOR_SPEED          1   /* Optimize for Rx throughput on expense of Tx QoS */
#define  PERFORMANCE_BOOST_MODE_DEF             BOOST_MODE_OPTIMIZE_FOR_QOS
#define  PERFORMANCE_BOOST_MODE_MIN             BOOST_MODE_OPTIMIZE_FOR_QOS
#define  PERFORMANCE_BOOST_MODE_MAX             BOOST_MODE_OPTIMIZE_FOR_SPEED

/* Maximum AMPDU Size */
#define MAX_MPDU_DEF                            MAX_MPDU_8191_OCTETS

/* STOP NETWORK STACK TX mode defines */
#define  STOP_NET_STACK_TX_DEF                  0
#define  STOP_NET_STACK_TX_MIN                  0
#define  STOP_NET_STACK_TX_MAX                  1
#define  STOP_NET_STACK_TX_DEF_WIFI_MODE        0

/* Tx send pacing threshold: num of Tx pkts to queue before handling them (excluding voice queue) */
#define  TX_SEND_PACE_THRESH_DEF                1
#define  TX_SEND_PACE_THRESH_MIN                1
#define  TX_SEND_PACE_THRESH_MAX                16
#define  TX_SEND_PACE_THRESH_DEF_WIFI_MODE      1

#define  QOS_TX_BLKS_THRESHOLD_MIN              0
#define  QOS_TX_BLKS_THRESHOLD_MAX              200

#define  QOS_QID_MIN                           0
#define  QOS_QID_MAX                           3

#define  QOS_AC_MIN                            QOS_QID_MIN
#define  QOS_AC_MAX                            QOS_QID_MAX

#define  QOS_AIFS_MIN                          1
#define  QOS_AIFS_MAX                          15

#define QOS_CWMIN_MIN                          0
#define QOS_CWMIN_MAX                          15

#define QOS_CWMAX_MIN                          0
#define QOS_CWMAX_MAX                          15

#define QOS_TIMEOUT_MIN                        0
#define QOS_TIMEOUT_MAX                        65535

#define QOS_ACK_POLICY_MIN                     0
#define QOS_ACK_POLICY_MAX                     1

#define QOS_TRAFFIC_TYPE_MIN                   0
#define QOS_TRAFFIC_TYPE_MAX                   1

#define QOS_SHORT_RETRY_LIMIT_MIN              1
#define QOS_SHORT_RETRY_LIMIT_MAX              255
#define QOS_SHORT_RETRY_LIMIT_DEF              10

#define QOS_SHORT_RETRY_LIMIT_BE_MIN            QOS_SHORT_RETRY_LIMIT_MIN
#define QOS_SHORT_RETRY_LIMIT_BE_MAX            QOS_SHORT_RETRY_LIMIT_MAX
#define QOS_SHORT_RETRY_LIMIT_BE_DEF            QOS_SHORT_RETRY_LIMIT_DEF

#define QOS_SHORT_RETRY_LIMIT_BK_MIN            QOS_SHORT_RETRY_LIMIT_MIN
#define QOS_SHORT_RETRY_LIMIT_BK_MAX            QOS_SHORT_RETRY_LIMIT_MAX
#define QOS_SHORT_RETRY_LIMIT_BK_DEF            QOS_SHORT_RETRY_LIMIT_DEF

#define QOS_SHORT_RETRY_LIMIT_VI_MIN            QOS_SHORT_RETRY_LIMIT_MIN
#define QOS_SHORT_RETRY_LIMIT_VI_MAX            QOS_SHORT_RETRY_LIMIT_MAX
#define QOS_SHORT_RETRY_LIMIT_VI_DEF            QOS_SHORT_RETRY_LIMIT_DEF

#define QOS_SHORT_RETRY_LIMIT_VO_MIN            QOS_SHORT_RETRY_LIMIT_MIN
#define QOS_SHORT_RETRY_LIMIT_VO_MAX            QOS_SHORT_RETRY_LIMIT_MAX
#define QOS_SHORT_RETRY_LIMIT_VO_DEF            4


#define QOS_LONG_RETRY_LIMIT_MIN               1
#define QOS_LONG_RETRY_LIMIT_MAX               255
#define QOS_LONG_RETRY_LIMIT_DEF               4

#define QOS_LONG_RETRY_LIMIT_BE_MIN             QOS_LONG_RETRY_LIMIT_MIN
#define QOS_LONG_RETRY_LIMIT_BE_MAX             QOS_LONG_RETRY_LIMIT_MAX
#define QOS_LONG_RETRY_LIMIT_BE_DEF             QOS_LONG_RETRY_LIMIT_DEF

#define QOS_LONG_RETRY_LIMIT_BK_MIN             QOS_LONG_RETRY_LIMIT_MIN
#define QOS_LONG_RETRY_LIMIT_BK_MAX             QOS_LONG_RETRY_LIMIT_MAX
#define QOS_LONG_RETRY_LIMIT_BK_DEF             QOS_LONG_RETRY_LIMIT_DEF

#define QOS_LONG_RETRY_LIMIT_VI_MIN             QOS_LONG_RETRY_LIMIT_MIN
#define QOS_LONG_RETRY_LIMIT_VI_MAX             QOS_LONG_RETRY_LIMIT_MAX
#define QOS_LONG_RETRY_LIMIT_VI_DEF             QOS_LONG_RETRY_LIMIT_DEF

#define QOS_LONG_RETRY_LIMIT_VO_MIN             QOS_LONG_RETRY_LIMIT_MIN
#define QOS_LONG_RETRY_LIMIT_VO_MAX             QOS_LONG_RETRY_LIMIT_MAX
#define QOS_LONG_RETRY_LIMIT_VO_DEF             QOS_LONG_RETRY_LIMIT_DEF

#define QOS_QUEUE_0_OVFLOW_POLICY_MIN           DROP_NEW_PACKET
#define QOS_QUEUE_0_OVFLOW_POLICY_MAX           DROP_OLD_PACKET
#define QOS_QUEUE_0_OVFLOW_POLICY_DEF           DROP_NEW_PACKET

#define QOS_QUEUE_1_OVFLOW_POLICY_MIN           DROP_NEW_PACKET
#define QOS_QUEUE_1_OVFLOW_POLICY_MAX           DROP_OLD_PACKET
#define QOS_QUEUE_1_OVFLOW_POLICY_DEF           DROP_NEW_PACKET

#define QOS_QUEUE_2_OVFLOW_POLICY_MIN           DROP_NEW_PACKET
#define QOS_QUEUE_2_OVFLOW_POLICY_MAX           DROP_OLD_PACKET
#define QOS_QUEUE_2_OVFLOW_POLICY_DEF           DROP_NEW_PACKET

#define QOS_QUEUE_3_OVFLOW_POLICY_MIN           DROP_NEW_PACKET
#define QOS_QUEUE_3_OVFLOW_POLICY_MAX           DROP_OLD_PACKET
#define QOS_QUEUE_3_OVFLOW_POLICY_DEF           DROP_NEW_PACKET

#define QOS_ACK_POLICY_BE_MIN                   ACK_POLICY_LEGACY
#define QOS_ACK_POLICY_BE_MAX                   MAX_ACK_POLICY
#define QOS_ACK_POLICY_BE_DEF                   ACK_POLICY_LEGACY

#define QOS_ACK_POLICY_BK_MIN                   ACK_POLICY_LEGACY
#define QOS_ACK_POLICY_BK_MAX                   MAX_ACK_POLICY
#define QOS_ACK_POLICY_BK_DEF                   ACK_POLICY_LEGACY

#define QOS_ACK_POLICY_VI_MIN                   ACK_POLICY_LEGACY
#define QOS_ACK_POLICY_VI_MAX                   MAX_ACK_POLICY
#define QOS_ACK_POLICY_VI_DEF                   ACK_POLICY_LEGACY

#define QOS_ACK_POLICY_VO_MIN                   ACK_POLICY_LEGACY
#define QOS_ACK_POLICY_VO_MAX                   MAX_ACK_POLICY
#define QOS_ACK_POLICY_VO_DEF                   ACK_POLICY_LEGACY


/* MAX_SP_LEN_VALUES
  00 - all buffered frames 
  01 - 2
  10 - 4 
  11 - 6
*/

#define QOS_MAX_SP_LEN_MIN                      0
#define QOS_MAX_SP_LEN_MAX                      3
/* Means maxSpLen = 2 (changed for SoftGemini requiremnet) */
#define QOS_MAX_SP_LEN_DEF                      1

/* CW setting from the ini */
#define QOS_CW_USER_ENABLE_MIN                  0
#define QOS_CW_USER_ENABLE_MAX                  1
#define QOS_CW_USER_ENABLE_DEF                  0

#define QOS_CW_CWMIN_MIN                        QOS_CWMIN_MIN
#define QOS_CW_CWMIN_MAX                        QOS_CWMIN_MAX
#define QOS_CW_CWMIN_DEF                        CW_MIN_DEF

#define QOS_CW_CWMAX_MIN                        QOS_CWMAX_MIN
#define QOS_CW_CWMAX_MAX                        QOS_CWMAX_MAX
#define QOS_CW_CWMAX_DEF                        CW_MAX_DEF

/*
 * 802.11n HT settion
 */
#define  HT_11N_ENABLED_MIN                         TI_FALSE
#define  HT_11N_ENABLED_MAX                         TI_TRUE
#define  HT_11N_ENABLED_DEF                         TI_TRUE
                                                    
/* policies: 0 - disable, 1 - initiator, 2 - receiver, 3 -  initiator& receiver. */
#define  HT_BA_POLICY_MIN                           0    
#define  HT_BA_POLICY_MAX                           3
#define  HT_BA_POLICY_DEF                           0

/* In the 11n spec in order to set endless timeout the value is 0 */
#define  HT_BA_INACTIVITY_TIMEOUT_MIN               0
#define  HT_BA_INACTIVITY_TIMEOUT_MAX               0xffff
#define  HT_BA_INACTIVITY_TIMEOUT_DEF               10000

/*---------------------------
      ROAMING parameters
-----------------------------*/
#define ROAMING_MNGR_ENABLE_MIN             0
#define ROAMING_MNGR_ENABLE_MAX             1
#define ROAMING_MNGR_ENABLE_DEF             0

#define ROAMING_MNGR_ENABLE_PERIODIC_SCAN_MIN       0
#define ROAMING_MNGR_ENABLE_PERIODIC_SCAN_MAX       1
#define ROAMING_MNGR_ENABLE_PERIODIC_SCAN_DEF       0

#define ROAMING_MNGR_RSSI_GAP_MIN                   0
#define ROAMING_MNGR_RSSI_GAP_MAX                   50
#define ROAMING_MNGR_RSSI_GAP_DEF                   10

#define ROAMING_MNGR_PERIODIC_SCAN_TIEMOUT_MIN      1000
#define ROAMING_MNGR_PERIODIC_SCAN_TIEMOUT_MAX      10000
#define ROAMING_MNGR_PERIODIC_SCAN_TIEMOUT_DEF      3000

#define ROAMING_MNGR_PERIODIC_SCAN_MIN_CH_MIN       5
#define ROAMING_MNGR_PERIODIC_SCAN_MIN_CH_MAX       60
#define ROAMING_MNGR_PERIODIC_SCAN_MIN_CH_DEF       5

#define ROAMING_MNGR_PERIODIC_SCAN_MAX_CH_MIN       5
#define ROAMING_MNGR_PERIODIC_SCAN_MAX_CH_MAX       60
#define ROAMING_MNGR_PERIODIC_SCAN_MAX_CH_DEF       20

#define ROAMING_MNGR_PERIODIC_SCAN_ET_MODE_MIN      0
#define ROAMING_MNGR_PERIODIC_SCAN_ET_MODE_MAX      3
#define ROAMING_MNGR_PERIODIC_SCAN_ET_MODE_DEF      3

#define ROAMING_MNGR_PERIODIC_SCAN_MAX_NUM_FRAMES_MIN       1
#define ROAMING_MNGR_PERIODIC_SCAN_MAX_NUM_FRAMES_MAX       30
#define ROAMING_MNGR_PERIODIC_SCAN_MAX_NUM_FRAMES_DEF       1

#define ROAMING_MNGR_PERIODIC_SCAN_NUM_PROBE_REQ_MIN        1
#define ROAMING_MNGR_PERIODIC_SCAN_NUM_PROBE_REQ_MAX        10
#define ROAMING_MNGR_PERIODIC_SCAN_NUM_PROBE_REQ_DEF        2

#define ROAMING_MNGR_OPERATIONAL_MODE_MIN       0 /* 0 - manual , 1 - auto*/
#define ROAMING_MNGR_OPERATIONAL_MODE_MAX       1
#define ROAMING_MNGR_OPERATIONAL_MODE_DEF       1

#define ROAMING_MNGR_SEND_TSPEC_IN_REASSO_PKT_MIN       0 /* 0 - do not send , 1 - send */
#define ROAMING_MNGR_SEND_TSPEC_IN_REASSO_PKT_MAX       1
#define ROAMING_MNGR_SEND_TSPEC_IN_REASSO_PKT_DEF       1


/*---------------------------
    Measurement parameters
-----------------------------*/

/* Packets Per Second threshold */
#define MEASUREMENT_TRAFFIC_THRSHLD_MIN         1
#define MEASUREMENT_TRAFFIC_THRSHLD_MAX             1000
#define MEASUREMENT_TRAFFIC_THRSHLD_DEF             400

/* In ms */
#define MEASUREMENT_MAX_DUR_NON_SRV_CHANNEL_MIN 1
#define MEASUREMENT_MAX_DUR_NON_SRV_CHANNEL_MAX             1000
#define MEASUREMENT_MAX_DUR_NON_SRV_CHANNEL_DEF             300


/*---------------------------
      XCC Manager parameters
-----------------------------*/
#define XCC_MNGR_ENABLE_MIN             XCC_MODE_DISABLED
#define XCC_MNGR_ENABLE_MAX             XCC_MODE_STANDBY
#define XCC_MNGR_ENABLE_DEF             XCC_MODE_ENABLED

#define XCC_TEST_IGNORE_DEAUTH_0_DEF            1
#define XCC_TEST_IGNORE_DEAUTH_0_MIN            0
#define XCC_TEST_IGNORE_DEAUTH_0_MAX            1

#define SITE_MGR_ROAMING_TX_RATE_PERCENTAGE_MIN         30
#define SITE_MGR_ROAMING_TX_RATE_PERCENTAGE_MAX         75
#define SITE_MGR_ROAMING_TX_RATE_PERCENTAGE_DEF         40


#define SITE_MGR_ROAMING_RSSI_MIN                       0
#define SITE_MGR_ROAMING_RSSI_MAX                       100
#define SITE_MGR_ROAMING_RSSI_DEF                       80

#define SITE_MGR_ROAMING_CONS_TX_ERRORS_MIN             1
#define SITE_MGR_ROAMING_CONS_TX_ERRORS_MAX             200 
#define SITE_MGR_ROAMING_CONS_TX_ERRORS_DEF             10

/*6 sec*/
#define SITE_MGR_POSTDISCONNECT_TIMEOUT_DEF     6000
#define SITE_MGR_POSTDISCONNECT_TIMEOUT_MIN     1000
#define SITE_MGR_POSTDISCONNECT_TIMEOUT_MAX     10000

/* 1 seconds */                                                                
#define CONN_SELF_TIMEOUT_MIN                   (1 * 1000)
/* 1 minute */
#define CONN_SELF_TIMEOUT_MAX                   (60 * 1000)
/* 10 seconds */
#define CONN_SELF_TIMEOUT_DEF                   (10 * 1000)

#define AUTH_RESPONSE_TIMEOUT_MIN               100
#define AUTH_RESPONSE_TIMEOUT_MAX               5000
#define AUTH_RESPONSE_TIMEOUT_DEF               500

#define AUTH_MAX_RETRY_COUNT_MIN                1
#define AUTH_MAX_RETRY_COUNT_MAX                5
#define AUTH_MAX_RETRY_COUNT_DEF                2

#define ASSOC_RESPONSE_TIMEOUT_MIN              1000
#define ASSOC_RESPONSE_TIMEOUT_MAX              5000
#define ASSOC_RESPONSE_TIMEOUT_DEF              2000

#define ASSOC_MAX_RETRY_COUNT_MIN               1
#define ASSOC_MAX_RETRY_COUNT_MAX               5
#define ASSOC_MAX_RETRY_COUNT_DEF               2

#define RX_DATA_FILTERS_ENABLED_MIN             TI_FALSE
#define RX_DATA_FILTERS_ENABLED_MAX             TI_TRUE
#define RX_DATA_FILTERS_ENABLED_DEF             TI_FALSE

#define RX_DATA_FILTERS_DEFAULT_ACTION_MIN      FILTER_DROP
#define RX_DATA_FILTERS_DEFAULT_ACTION_MAX      FILTER_FW_HANDLE
#define RX_DATA_FILTERS_DEFAULT_ACTION_DEF      FILTER_DROP

#define RX_DATA_FILTERS_FILTER_OFFSET_DEF       0
#define RX_DATA_FILTERS_FILTER_OFFSET_MIN       0
#define RX_DATA_FILTERS_FILTER_OFFSET_MAX       255

#define RX_DATA_FILTERS_FILTER_MASK_DEF         ""
#define RX_DATA_FILTERS_FILTER_MASK_LEN_DEF     0

#define RX_DATA_FILTERS_FILTER_PATTERN_DEF      ""
#define RX_DATA_FILTERS_FILTER_PATTERN_LEN_DEF  0

#define RX_DATA_RE_AUTH_ACTIVE_TIMEOUT_DEF				500
#define RX_DATA_RE_AUTH_ACTIVE_TIMEOUT_MIN				500
#define RX_DATA_RE_AUTH_ACTIVE_TIMEOUT_MAX				800

#define TX_DATA_CREDIT_CALC_TIMOEUT_DEF         100
#define TX_DATA_CREDIT_CALC_TIMOEUT_MIN         20
#define TX_DATA_CREDIT_CALC_TIMOEUT_MAX         1000

#define TRAFFIC_ADM_CONTROL_TIMEOUT_MIN         10
#define TRAFFIC_ADM_CONTROL_TIMEOUT_MAX         10000
#define TRAFFIC_ADM_CONTROL_TIMEOUT_DEF         5000

/* Traffic intensity threshold - Measured in packets */
#define CTRL_DATA_TRAFFIC_THRESHOLD_HIGH_MIN    1
#define CTRL_DATA_TRAFFIC_THRESHOLD_HIGH_MAX    1000
#define CTRL_DATA_TRAFFIC_THRESHOLD_HIGH_DEF    100

/* Traffic intensity threshold - Measured in packets */
#define CTRL_DATA_TRAFFIC_THRESHOLD_LOW_MIN     1
#define CTRL_DATA_TRAFFIC_THRESHOLD_LOW_MAX     1000
#define CTRL_DATA_TRAFFIC_THRESHOLD_LOW_DEF     25

/* Traffic intensity threshold - Traffic test interval - measured in ms */
#define CTRL_DATA_TRAFFIC_THRESHOLD_INTERVAL_MIN   50
#define CTRL_DATA_TRAFFIC_THRESHOLD_INTERVAL_MAX   10000
#define CTRL_DATA_TRAFFIC_THRESHOLD_INTERVAL_DEF   1000

#define CTRL_DATA_TRAFFIC_THRESHOLD_ENABLED_MIN TI_FALSE
#define CTRL_DATA_TRAFFIC_THRESHOLD_ENABLED_MAX TI_TRUE
#define CTRL_DATA_TRAFFIC_THRESHOLD_ENABLED_DEF TI_FALSE

#define TRAFFIC_MONITOR_MIN_INTERVAL_PERCENT_MIN   10
#define TRAFFIC_MONITOR_MIN_INTERVAL_PERCENT_MAX   90
#define TRAFFIC_MONITOR_MIN_INTERVAL_PERCENT_DEF   50

#define CTRL_DATA_CONT_TX_THRESHOLD_MIN  2
#define CTRL_DATA_CONT_TX_THRESHOLD_MAX  256
#define CTRL_DATA_CONT_TX_THRESHOLD_DEF  30

#define CTRL_DATA_STEP_UP_TX_THRESHOLD_MIN    2
#define CTRL_DATA_STEP_UP_TX_THRESHOLD_MAX    256
#define CTRL_DATA_STEP_UP_TX_THRESHOLD_DEF    10

#define CTRL_DATA_FB_SHORT_INTERVAL_MIN         20
#define CTRL_DATA_FB_SHORT_INTERVAL_MAX         2000
#define CTRL_DATA_FB_SHORT_INTERVAL_DEF         50

#define CTRL_DATA_FB_LONG_INTERVAL_MIN          100
#define CTRL_DATA_FB_LONG_INTERVAL_MAX          10000
#define CTRL_DATA_FB_LONG_INTERVAL_DEF          2000

#define RATE_ADAPTATION_TIMEOUT_MIN             1
#define RATE_ADAPTATION_TIMEOUT_MAX             3600
#define RATE_ADAPTATION_TIMEOUT_DEF             300

#define RATE_ADAPT_HIGH_TRSH_AC_VO_MIN          0
#define RATE_ADAPT_HIGH_TRSH_AC_VO_MAX          54  
#define RATE_ADAPT_HIGH_TRSH_AC_VO_DEF          0

#define RATE_ADAPT_HIGH_TRSH_AC_VI_MIN          0
#define RATE_ADAPT_HIGH_TRSH_AC_VI_MAX          54  
#define RATE_ADAPT_HIGH_TRSH_AC_VI_DEF          0

#define RATE_ADAPT_HIGH_TRSH_AC_BE_MIN          0
#define RATE_ADAPT_HIGH_TRSH_AC_BE_MAX          54  
#define RATE_ADAPT_HIGH_TRSH_AC_BE_DEF          0

#define RATE_ADAPT_HIGH_TRSH_AC_BK_MIN          0
#define RATE_ADAPT_HIGH_TRSH_AC_BK_MAX          54  
#define RATE_ADAPT_HIGH_TRSH_AC_BK_DEF          0

#define RATE_ADAPT_LOW_TRSH_AC_VO_MIN           0
#define RATE_ADAPT_LOW_TRSH_AC_VO_MAX           54  
#define RATE_ADAPT_LOW_TRSH_AC_VO_DEF           0

#define RATE_ADAPT_LOW_TRSH_AC_VI_MIN           0
#define RATE_ADAPT_LOW_TRSH_AC_VI_MAX           54  
#define RATE_ADAPT_LOW_TRSH_AC_VI_DEF           0

#define RATE_ADAPT_LOW_TRSH_AC_BE_MIN           0
#define RATE_ADAPT_LOW_TRSH_AC_BE_MAX           54  
#define RATE_ADAPT_LOW_TRSH_AC_BE_DEF           0

#define RATE_ADAPT_LOW_TRSH_AC_BK_MIN           0
#define RATE_ADAPT_LOW_TRSH_AC_BK_MAX           54  
#define RATE_ADAPT_LOW_TRSH_AC_BK_DEF           0

#define CTRL_DATA_RATE_POLICY_USER_SHORT_RETRY_LIMIT_MIN 1
#define CTRL_DATA_RATE_POLICY_USER_SHORT_RETRY_LIMIT_MAX 255
#define CTRL_DATA_RATE_POLICY_USER_SHORT_RETRY_LIMIT_DEF 10

#define CTRL_DATA_RATE_POLICY_USER_LONG_RETRY_LIMIT_MIN 1  
#define CTRL_DATA_RATE_POLICY_USER_LONG_RETRY_LIMIT_MAX 255
#define CTRL_DATA_RATE_POLICY_USER_LONG_RETRY_LIMIT_DEF 10  

#define CTRL_DATA_RATE_POLICY_USER_EN_DIS_MASK_MIN			0
#define CTRL_DATA_RATE_POLICY_USER_EN_DIS_MASK_MAX 			0xFFFFFFFF
#define CTRL_DATA_RATE_POLICY_USER_EN_DIS_MASK_CCK_DEF	 	0x0000001F	   		
#define CTRL_DATA_RATE_POLICY_USER_EN_DIS_MASK_OFDM_DEF	 	0x00001FEF	   		
#define CTRL_DATA_RATE_POLICY_USER_EN_DIS_MASK_OFDMA_DEF	0x00001FE0	   		
#define CTRL_DATA_RATE_POLICY_USER_EN_DIS_MASK_OFDMN_DEF	0x001FFFEF	   		

#define REPORT_SEVERITY_VALUE_MIN               0
#define REPORT_SEVERITY_VALUE_MAX               0xFF
/* WLAN_SEVERITY_WARNING | WLAN_SEVERITY_ERROR | WLAN_SEVERITY_FATAL_ERROR | WLAN_SEVERITY_CONSOLE */
#define REPORT_SEVERITY_VALUE_DEF               0xB8

#define RSN_AUTH_SUITE_MIN                      RSN_AUTH_OPEN
#define RSN_AUTH_SUITE_MAX                      RSN_AUTH_NONE
#define RSN_AUTH_SUITE_DEF                      RSN_AUTH_OPEN

#define RSN_DEFAULT_KEY_ID_MIN                  0
#define RSN_DEFAULT_KEY_ID_MAX                  (DOT11_MAX_DEFAULT_WEP_KEYS - 1)
#define RSN_DEFAULT_KEY_ID_DEF                  0

/* 1 sec */
#define RSN_PMKSA_LIFETIME_MIN                  1
/* 49 days in sec */
#define RSN_PMKSA_LIFETIME_MAX                  4233600
/* 1 day in sec */
#define RSN_PMKSA_LIFETIME_DEF                  86400

#define RSN_WEP_STATUS_MIN                      0
#define RSN_WEP_STATUS_MAX                      1
#define RSN_WEP_STATUS_DEF                      0

#define RSN_WEPMIXEDMODE_ENABLED_MIN                    0
#define RSN_WEPMIXEDMODE_ENABLED_MAX                    1
#define RSN_WEPMIXEDMODE_ENABLED_DEF                    0

#define RSN_WPAMIXEDMODE_ENABLE_MIN             0
#define RSN_WPAMIXEDMODE_ENABLE_MAX             1
#define RSN_WPAMIXEDMODE_ENABLE_DEF             1

#define RSN_PREAUTH_ENABLE_MIN                  0
#define RSN_PREAUTH_ENABLE_MAX                  1
#define RSN_PREAUTH_ENABLE_DEF                  1

/* In mSec units */
#define RSN_PREAUTH_TIMEOUT_MIN                  500
#define RSN_PREAUTH_TIMEOUT_MAX                  60000
#define RSN_PREAUTH_TIMEOUT_DEF                 2000

#define PAIRWISE_MIC_FAIL_FILTER_MIN			0
#define PAIRWISE_MIC_FAIL_FILTER_MAX			1
#define PAIRWISE_MIC_FAIL_FILTER_DEF			1

#define  RSN_PMKIDCANDLIST_DELAY_MIN            3000
#define  RSN_PMKIDCANDLIST_DELAY_MAX            9000
#define  RSN_PMKIDCANDLIST_DELAY_DEF            4000

#define RSN_EXTERNAL_MODE_ENABLE_MIN            0
#define RSN_EXTERNAL_MODE_ENABLE_MAX            1
#define RSN_EXTERNAL_MODE_ENABLE_DEF            0


#define ETHER_MAX_PAYLOAD_SIZE            1500

/* SME Values */
#define SME_SCAN_RSSI_THRESHOLD_MIN             -100
#define SME_SCAN_RSSI_THRESHOLD_MAX             0
#define SME_SCAN_RSSI_THRESHOLD_DEF             "-97"
#define SME_SCAN_RSSI_THRESHOLD_DEF_NUM         -97

#define SME_SCAN_SNR_THRESHOLD_MIN              -5
#define SME_SCAN_SNR_THRESHOLD_MAX              20
#define SME_SCAN_SNR_THRESHOLD_DEF              "0"
#define SME_SCAN_SNR_THRESHOLD_DEF_NUM          0

#define SME_SCAN_CYCLES_MIN                     0
#define SME_SCAN_CYCLES_MAX                     100
#define SME_SCAN_CYCLES_DEF                     50

#define SME_SCAN_MAX_DWELL_MIN                  5     /* in milli-seconds */
#define SME_SCAN_MAX_DWELL_MAX                  500
#define SME_SCAN_MAX_DWELL_DEF                  60

#define SME_SCAN_MIN_DWELL_MIN                  5     /* in milli-seconds */
#define SME_SCAN_MIN_DWELL_MAX                  500
#define SME_SCAN_MIN_DWELL_DEF                  30

#define SME_SCAN_PROBE_REQ_MIN                  1
#define SME_SCAN_PROBE_REQ_MAX                  10
#define SME_SCAN_PROBE_REQ_DEF                  3

#define SME_SCAN_INTERVALS_LIST_VAL_DEF         "0,1000,2000,3000,4000,8000,16000,32000,64000,128000,256000,512000,512000,512000,512000,512000"
#define SME_SCAN_INTERVALS_LIST_STRING_MAX_SIZE 255

#define SME_SCAN_CHANNELS_LIST_G_VAL_DEF        "1,2,3,4,5,6,7,8,9,10,11,12,13,14"
#define SME_SCAN_CHANNELS_LIST_G_STRING_MAX_SIZE 255

#define SME_SCAN_CHANNELS_LIST_A_VAL_DEF        "36,40,44,48,52,56,60,64"
#define SME_SCAN_CHANNELS_LIST_A_STRING_MAX_SIZE 255

/* Scan SRV parameters */
#define SCAN_SRV_NUMBER_OF_NO_SCAN_COMPLETE_TO_RECOVERY_DEF     3
#define SCAN_SRV_NUMBER_OF_NO_SCAN_COMPLETE_TO_RECOVERY_MIN     1
#define SCAN_SRV_NUMBER_OF_NO_SCAN_COMPLETE_TO_RECOVERY_MAX     1000000

#define SCAN_SRV_TRIGGERED_SCAN_TIME_OUT_DEF        50000
#define SCAN_SRV_TRIGGERED_SCAN_TIME_OUT_MIN        0
#define SCAN_SRV_TRIGGERED_SCAN_TIME_OUT_MAX        0xffffffff


/*
  EEPROM-less support
*/
#define REG_MAC_ADDR_STR_LEN                    17
#define REG_ARP_IP_ADDR_STR_LEN                 11
#define REG_MAC_ADDR_PREAMBLE_STR_LEN           9

#define TWD_EEPROMLESS_ENABLE_DEF               1
#define TWD_EEPROMLESS_ENABLE_MIN               0
#define TWD_EEPROMLESS_ENABLE_MAX               1

/* Scanning Channel Values */
#define MAX_CHAN_BITMAP_BYTES                   26

#define MIN_CHANNEL_IN_BAND_2_4                  1
#define MAX_CHANNEL_IN_BAND_2_4                 14

#define SCAN_CONTROL_TABLE_ENTRY_MIN            0x00
#define SCAN_CONTROL_TABLE_ENTRY_MAX            0xff
#define SCAN_CONTROL_TABLE_ENTRY_DEF            0xff

/* country code reset time out */
/* 1 sec   */
#define REGULATORY_DOMAIN_COUNTRY_TIME_RESET_MIN     1000
/* 1 day */
#define REGULATORY_DOMAIN_COUNTRY_TIME_RESET_MAX     86400000
/* 1 minutes  */
#define REGULATORY_DOMAIN_COUNTRY_TIME_RESET_DEF     60000

/* d/h Enabling */

/* 802.11d */
#define MULTI_REGULATORY_DOMAIN_ENABLED_MIN     TI_FALSE
#define MULTI_REGULATORY_DOMAIN_ENABLED_MAX     TI_TRUE
#define MULTI_REGULATORY_DOMAIN_ENABLED_DEF     TI_FALSE

/* 802.11h */
#define SPECTRUM_MANAGEMENT_ENABLED_MIN         TI_FALSE
#define SPECTRUM_MANAGEMENT_ENABLED_MAX         TI_TRUE
#define SPECTRUM_MANAGEMENT_ENABLED_DEF         TI_FALSE

/* Tx Power table (Power level to Dbm)*/
#define TX_POWER_LEVEL_TABLE_24                 "21,13,10,7"
#define TX_POWER_LEVEL_TABLE_5                  "20,12,9,6" 

/* Scan concentrator init parameters - number of entries in app scan result table */
#define SCAN_CNCN_MIN_DURATION_FOR_OS_SCANS_DEF 5
#define SCAN_CNCN_MIN_DURATION_FOR_OS_SCANS_MIN 0
#define SCAN_CNCN_MIN_DURATION_FOR_OS_SCANS_MAX 3600

#define SCAN_CNCN_REGULATORY_DOMAIN_PASSIVE_DWELL_TIME_DEF    150

#define SCAN_CNCN_DFS_PASSIVE_DWELL_TIME_DEF    150
#define SCAN_CNCN_DFS_PASSIVE_DWELL_TIME_MIN    10
#define SCAN_CNCN_DFS_PASSIVE_DWELL_TIME_MAX    500

#define SCAN_CNCN_APP_PUSH_MODE_DEF             TI_FALSE
#define SCAN_CNCN_APP_PUSH_MODE_MIN             TI_FALSE
#define SCAN_CNCN_APP_PUSH_MODE_MAX             TI_TRUE

#define SCAN_CNCN_APP_SRA_DEF                   20
#define SCAN_CNCN_APP_SRA_MIN                   0
#define SCAN_CNCN_APP_SRA_MAX                   1000

#define SCAN_CNCN_RSSI_DEF                      (-100)
#define SCAN_CNCN_RSSI_MIN                      (-100)
#define SCAN_CNCN_RSSI_MAX                      0

/* Current BSS init paramaters - keep alive default interval */
#define NULL_KL_PERIOD_DEF      10
#define NULL_KL_PERIOD_MIN      0
#define NULL_KL_PERIOD_MAX      3600       

/* Context-Engine init paramaters */
#define CONTEXT_SWITCH_REQUIRED_DEF     TI_TRUE
#define CONTEXT_SWITCH_REQUIRED_MIN     TI_FALSE
#define CONTEXT_SWITCH_REQUIRED_MAX     TI_TRUE

/* WSC */
#define WSC_PARSE_IN_BEACON_DEF 1
#define WSC_PARSE_IN_BEACON_MIN 0
#define WSC_PARSE_IN_BEACON_MAX 1

#define WSC_INCLUDE_IN_BEACON_DEF 1
#define WSC_INCLUDE_IN_BEACON_MIN 0
#define WSC_INCLUDE_IN_BEACON_MAX 1


/* Packet Filtering Define */
#define MIN_NUM_OF_BEACONS_IN_BUFFER 1
#define DEF_NUM_OF_BEACONS_IN_BUFFER 5
#define MAX_NUM_OF_BEACONS_IN_BUFFER 10

/* Soft Gemini Enabling */
#define SOFT_GEMINI_ENABLED_MIN                 SG_DISABLE
#define SOFT_GEMINI_ENABLED_MAX                 SG_OPPORTUNISTIC
#define SOFT_GEMINI_ENABLED_DEF                 SG_PROTECTIVE

#define SOFT_GEMINI_PARAMS_INDEX_MIN			0
#define SOFT_GEMINI_PARAMS_INDEX_MAX			(SOFT_GEMINI_PARAMS_MAX-1)
#define SOFT_GEMINI_PARAMS_INDEX_DEF			0

#define SOFT_GEMINI_PARAMS_VALUE_MIN			0
#define SOFT_GEMINI_PARAMS_VALUE_MAX			10000000
#define SOFT_GEMINI_PARAMS_VALUE_DEF			0


#define SOFT_GEMINI_PARAMS_PER_THRESHOLD_MIN			0
#define SOFT_GEMINI_PARAMS_PER_THRESHOLD_MAX			10000000
#define SOFT_GEMINI_PARAMS_PER_THRESHOLD_DEF			7500

#define SOFT_GEMINI_PARAMS_NFS_SAMPLE_INTERVAL_MIN	1
#define SOFT_GEMINI_PARAMS_NFS_SAMPLE_INTERVAL_MAX	65000
#define SOFT_GEMINI_PARAMS_NFS_SAMPLE_INTERVAL_DEF	400

#define SOFT_GEMINI_PARAMS_LOAD_RATIO_MIN				0
#define SOFT_GEMINI_PARAMS_LOAD_RATIO_MAX				65000
#define SOFT_GEMINI_PARAMS_LOAD_RATIO_DEF				200

#define SOFT_GEMINI_PARAMS_AUTO_PS_MODE_MIN				0
#define SOFT_GEMINI_PARAMS_AUTO_PS_MODE_MAX				1
#define SOFT_GEMINI_PARAMS_AUTO_PS_MODE_DEF				1

#define SOFT_GEMINI_PARAMS_AUTO_SCAN_PROBE_REQ_MIN		0
#define SOFT_GEMINI_PARAMS_AUTO_SCAN_PROBE_REQ_MAX		255
#define SOFT_GEMINI_PARAMS_AUTO_SCAN_PROBE_REQ_DEF		170

#define SOFT_GEMINI_PARAMS_ACTIVE_SCAN_DURATION_FACTOR_HV3_MIN		0
#define SOFT_GEMINI_PARAMS_ACTIVE_SCAN_DURATION_FACTOR_HV3_MAX		255
#define SOFT_GEMINI_PARAMS_ACTIVE_SCAN_DURATION_FACTOR_HV3_DEF		50

#define SOFT_GEMINI_PARAMS_ACTIVE_SCAN_DURATION_FACTOR_A2DP_MIN		0
#define SOFT_GEMINI_PARAMS_ACTIVE_SCAN_DURATION_FACTOR_A2DP_MAX		255
#define SOFT_GEMINI_PARAMS_ACTIVE_SCAN_DURATION_FACTOR_A2DP_DEF		100

#define SOFT_GEMINI_HV3_MAX_OVERRIDE_MIN       0
#define SOFT_GEMINI_HV3_MAX_OVERRIDE_MAX       10
#define SOFT_GEMINI_HV3_MAX_OVERRIDE_DEF       0 

#define SOFT_GEMINI_ANTENNA_CONFIGURATION_MIN       0
#define SOFT_GEMINI_ANTENNA_CONFIGURATION_MAX       1
#define SOFT_GEMINI_ANTENNA_CONFIGURATION_DEF       0

#define SOFT_GEMINI_BEACON_MISS_PERCENT_MIN       0 
#define SOFT_GEMINI_BEACON_MISS_PERCENT_MAX       100
#define SOFT_GEMINI_BEACON_MISS_PERCENT_DEF       60

#define SOFT_GEMINI_RATE_ADAPT_THRESH_MIN       1 
#define SOFT_GEMINI_RATE_ADAPT_THRESH_MAX       54
#define SOFT_GEMINI_RATE_ADAPT_THRESH_DEF       12

#define SOFT_GEMINI_RATE_ADAPT_SNR_MIN       0 
#define SOFT_GEMINI_RATE_ADAPT_SNR_MAX       255
#define SOFT_GEMINI_RATE_ADAPT_SNR_DEF       0



#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_BR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_BR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_BR_DEF 		 10

#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_BR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_BR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_BR_DEF 		 30

#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_BR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_BR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_BR_DEF 		 8

#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_BR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_BR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_BR_DEF 		 20

#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_BR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_BR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_BR_DEF 		 50

#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_BR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_BR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_BR_DEF 		 8


#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_EDR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_EDR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_EDR_DEF 		 7

#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_EDR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_EDR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_EDR_DEF 		 25

#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_EDR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_EDR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_EDR_DEF 		 20

#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_EDR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_EDR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_EDR_DEF 		 8

#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_EDR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_EDR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_EDR_DEF 		 40

#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_EDR_MIN 		 0
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_EDR_MAX 		 1000
#define SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_EDR_DEF 		 20





#define SOFT_GEMINI_RXT_MIN 		 		0
#define SOFT_GEMINI_RXT_MAX 		 		65535
#define SOFT_GEMINI_RXT_DEF 		 		1200

#define SOFT_GEMINI_TXT_MIN 		 		0
#define SOFT_GEMINI_TXT_MAX 		 		65535
#define SOFT_GEMINI_TXT_DEF 		 		1000

#define SOFT_GEMINI_ADAPTIVE_RXT_TXT_MIN   	0
#define SOFT_GEMINI_ADAPTIVE_RXT_TXT_MAX 	1
#define SOFT_GEMINI_ADAPTIVE_RXT_TXT_DEF 	1 /* 0 = Disabled, 1= Enabled */

#define SOFT_GEMINI_PS_POLL_TIMEOUT_MIN   	0
#define SOFT_GEMINI_PS_POLL_TIMEOUT_MAX 	1000000
#define SOFT_GEMINI_PS_POLL_TIMEOUT_DEF 	10

#define SOFT_GEMINI_UPSD_TIMEOUT_MIN   		0
#define SOFT_GEMINI_UPSD_TIMEOUT_MAX 		1000000
#define SOFT_GEMINI_UPSD_TIMEOUT_DEF 		10


#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MIN_EDR_MIN   0
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MIN_EDR_MAX 	1000
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MIN_EDR_DEF 	7

#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MAX_EDR_MIN   0
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MAX_EDR_MAX 	1000
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MAX_EDR_DEF 	15

#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_MASTER_EDR_MIN   0
#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_MASTER_EDR_MAX 	1000
#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_MASTER_EDR_DEF 	15


#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_EDR_MIN    0
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_EDR_MAX 	1000
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_EDR_DEF 	8

#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_EDR_MIN    0
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_EDR_MAX 	1000
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_EDR_DEF 	20

#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_EDR_MIN    0
#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_EDR_MAX 	1000
#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_EDR_DEF 	15

#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_BR_MIN    0
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_BR_MAX    1000
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_BR_DEF    20

#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_BR_MIN    0
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_BR_MAX    1000
#define SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_BR_DEF    50

#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_BR_MIN    0
#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_BR_MAX    1000
#define SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_BR_DEF    10

#define SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_HV3_MIN    0
#define SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_HV3_MAX 	1000
#define SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_HV3_DEF 	200

#define SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_A2DP_MIN    0
#define SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_A2DP_MAX 	1000
#define SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_A2DP_DEF 	800

#define SOFT_GEMINI_PASSIVE_SCAN_A2DP_BT_TIME_MIN    0
#define SOFT_GEMINI_PASSIVE_SCAN_A2DP_BT_TIME_MAX 	100
#define SOFT_GEMINI_PASSIVE_SCAN_A2DP_BT_TIME_DEF 	75

#define SOFT_GEMINI_PASSIVE_SCAN_A2DP_WLAN_TIME_MIN  0
#define SOFT_GEMINI_PASSIVE_SCAN_A2DP_WLAN_TIME_MAX 	100
#define SOFT_GEMINI_PASSIVE_SCAN_A2DP_WLAN_TIME_DEF 	15

#define SOFT_GEMINI_DHCP_TIME_MIN  	0
#define SOFT_GEMINI_DHCP_TIME_MAX 	65535
#define SOFT_GEMINI_DHCP_TIME_DEF 	5000

#define SOFT_GEMINI_HV3_MAX_SERVED_MIN	0
#define SOFT_GEMINI_HV3_MAX_SERVED_MAX 	256
#define SOFT_GEMINI_HV3_MAX_SERVED_DEF 	6

#define SOFT_GEMINI_TEMP_PARAM_1_MIN    0
#define SOFT_GEMINI_TEMP_PARAM_1_MAX 	100000
#define SOFT_GEMINI_TEMP_PARAM_1_DEF 	0

#define SOFT_GEMINI_TEMP_PARAM_2_MIN    0
#define SOFT_GEMINI_TEMP_PARAM_2_MAX 	100000
#define SOFT_GEMINI_TEMP_PARAM_2_DEF 	0

#define SOFT_GEMINI_TEMP_PARAM_3_MIN    0
#define SOFT_GEMINI_TEMP_PARAM_3_MAX 	100000
#define SOFT_GEMINI_TEMP_PARAM_3_DEF 	0

#define SOFT_GEMINI_TEMP_PARAM_4_MIN    0
#define SOFT_GEMINI_TEMP_PARAM_4_MAX 	100000
#define SOFT_GEMINI_TEMP_PARAM_4_DEF 	23

#define SOFT_GEMINI_TEMP_PARAM_5_MIN    0
#define SOFT_GEMINI_TEMP_PARAM_5_MAX 	100000
#define SOFT_GEMINI_TEMP_PARAM_5_DEF 	22


#define WIFI_WMM_PS_MIN                         0  
#define WIFI_WMM_PS_MAX                         1
#define WIFI_WMM_PS_DEF                         0 
#define SOFT_GEMINI_PARAMS_ALLOW_PA_SD_MIN                  (0)  
#define SOFT_GEMINI_PARAMS_ALLOW_PA_SD_MAX                  (1)
#define SOFT_GEMINI_PARAMS_ALLOW_PA_SD_DEF                  (1) 

#define SOFT_GEMINI_PARAMS_TIME_BEFORE_BEACON_MIN           (0)  
#define SOFT_GEMINI_PARAMS_TIME_BEFORE_BEACON_MAX           (20000)
#define SOFT_GEMINI_PARAMS_TIME_BEFORE_BEACON_DEF           (6300) 

#define SOFT_GEMINI_PARAMS_HPDM_MAX_GUARD_MIN           (0)  
#define SOFT_GEMINI_PARAMS_HPDM_MAX_GUARD_MAX           (50000)
#define SOFT_GEMINI_PARAMS_HPDM_MAX_GUARD_DEF           (1050) 

#define SOFT_GEMINI_PARAMS_TIME_OUT_NEXT_WLAN_MIN           (100)  
#define SOFT_GEMINI_PARAMS_TIME_OUT_NEXT_WLAN_MAX           (50000)
#define SOFT_GEMINI_PARAMS_TIME_OUT_NEXT_WLAN_DEF           (2550) 

#define SOFT_GEMINI_PARAMS_AUTO_MODE_NO_CTS_MIN         (0)  
#define SOFT_GEMINI_PARAMS_AUTO_MODE_NO_CTS_MAX         (1)
#define SOFT_GEMINI_PARAMS_AUTO_MODE_NO_CTS_DEF         (0) 

#define SOFT_GEMINI_PARAMS_BT_HP_RESPECTED_MIN          (0)  
#define SOFT_GEMINI_PARAMS_BT_HP_RESPECTED_MAX          (20)
#define SOFT_GEMINI_PARAMS_BT_HP_RESPECTED_DEF          (3) 

#define SOFT_GEMINI_PARAMS_WLAN_RX_MIN_RATE_MIN         (0)  
#define SOFT_GEMINI_PARAMS_WLAN_RX_MIN_RATE_MAX         (54)
#define SOFT_GEMINI_PARAMS_WLAN_RX_MIN_RATE_DEF         (24) 

/* CoexActivity table */
#define COEX_ACTIVITY_PARAMS_COEX_IP_MIN               (0)
#define COEX_ACTIVITY_PARAMS_COEX_IP_MAX               (1)
#define COEX_ACTIVITY_PARAMS_COEX_IP_DEF               (1)

#define COEX_ACTIVITY_PARAMS_ACTIVITY_ID_MIN           (0)
#define COEX_ACTIVITY_PARAMS_ACTIVITY_ID_MAX           (24)
#define COEX_ACTIVITY_PARAMS_ACTIVITY_ID_DEF           (0)

#define COEX_ACTIVITY_PARAMS_DEFAULT_PRIO_MIN          (0)
#define COEX_ACTIVITY_PARAMS_DEFAULT_PRIO_MAX          (255)
#define COEX_ACTIVITY_PARAMS_DEFAULT_PRIO_DEF          (0)

#define COEX_ACTIVITY_PARAMS_RAISED_PRIO_MIN           (0)
#define COEX_ACTIVITY_PARAMS_RAISED_PRIO_MAX           (255)
#define COEX_ACTIVITY_PARAMS_RAISED_PRIO_DEF           (0)

#define COEX_ACTIVITY_PARAMS_MIN_SERVICE_MIN           (0)
#define COEX_ACTIVITY_PARAMS_MIN_SERVICE_MAX           (65535)
#define COEX_ACTIVITY_PARAMS_MIN_SERVICE_DEF           (0)

#define COEX_ACTIVITY_PARAMS_MAX_SERVICE_MIN           (0)
#define COEX_ACTIVITY_PARAMS_MAX_SERVICE_MAX           (65535)
#define COEX_ACTIVITY_PARAMS_MAX_SERVICE_DEF           (0)

/* FM Coexistence */
#define FM_COEX_ENABLE_DEF                             TI_TRUE 
#define FM_COEX_ENABLE_MIN                             TI_FALSE  
#define FM_COEX_ENABLE_MAX                             TI_TRUE 

#define FM_COEX_SWALLOW_PERIOD_DEF                     5
#define FM_COEX_SWALLOW_PERIOD_MIN                     0
#define FM_COEX_SWALLOW_PERIOD_MAX                     0xFF

#define FM_COEX_N_DIVIDER_FREF_SET1_DEF                0xFF
#define FM_COEX_N_DIVIDER_FREF_SET1_MIN                0
#define FM_COEX_N_DIVIDER_FREF_SET1_MAX                0xFF

#define FM_COEX_N_DIVIDER_FREF_SET2_DEF                12                  
#define FM_COEX_N_DIVIDER_FREF_SET2_MIN                0   
#define FM_COEX_N_DIVIDER_FREF_SET2_MAX                0xFF

#define FM_COEX_M_DIVIDER_FREF_SET1_DEF                148   
#define FM_COEX_M_DIVIDER_FREF_SET1_MIN                0   
#define FM_COEX_M_DIVIDER_FREF_SET1_MAX                0xFFFF

#define FM_COEX_M_DIVIDER_FREF_SET2_DEF                0xFFFF 
#define FM_COEX_M_DIVIDER_FREF_SET2_MIN                0    
#define FM_COEX_M_DIVIDER_FREF_SET2_MAX                0xFFFF

#define FM_COEX_PLL_STABILIZATION_TIME_DEF             0xFFFFFFFF 
#define FM_COEX_PLL_STABILIZATION_TIME_MIN             0
#define FM_COEX_PLL_STABILIZATION_TIME_MAX             0xFFFFFFFF

#define FM_COEX_LDO_STABILIZATION_TIME_DEF             0xFFFF
#define FM_COEX_LDO_STABILIZATION_TIME_MIN             0
#define FM_COEX_LDO_STABILIZATION_TIME_MAX             0xFFFF

#define FM_COEX_DISTURBED_BAND_MARGIN_DEF              0xFF
#define FM_COEX_DISTURBED_BAND_MARGIN_MIN              0
#define FM_COEX_DISTURBED_BAND_MARGIN_MAX              0xFF

#define FM_COEX_SWALLOW_CLK_DIF_DEF                    0xFF
#define FM_COEX_SWALLOW_CLK_DIF_MIN                    0
#define FM_COEX_SWALLOW_CLK_DIF_MAX                    0xFF


/* Rate Management */

#define RATE_MNG_RATE_RETRY_SCORE_DEF    32000
#define RATE_MNG_RATE_RETRY_SCORE_MIN    0
#define RATE_MNG_RATE_RETRY_SCORE_MAX    65536

#define RATE_MNG_PER_ADD_DEF             8192
#define RATE_MNG_PER_ADD_MIN             0
#define RATE_MNG_PER_ADD_MAX             65536

#define RATE_MNG_PER_TH1_DEF             2048                       
#define RATE_MNG_PER_TH1_MIN             0
#define RATE_MNG_PER_TH1_MAX             65536 

#define RATE_MNG_PER_TH2_DEF             4096
#define RATE_MNG_PER_TH2_MIN             0
#define RATE_MNG_PER_TH2_MAX             65536

#define RATE_MNG_MAX_PER_DEF             8100
#define RATE_MNG_MAX_PER_MIN             0
#define RATE_MNG_MAX_PER_MAX             65536

#define RATE_MNG_INVERSE_CURISITY_FACTOR_DEF 5
#define RATE_MNG_INVERSE_CURISITY_FACTOR_MIN 0
#define RATE_MNG_INVERSE_CURISITY_FACTOR_MAX 256

#define RATE_MNG_TX_FAIL_LOW_TH_DEF       4
#define RATE_MNG_TX_FAIL_LOW_TH_MIN       0
#define RATE_MNG_TX_FAIL_LOW_TH_MAX       256

#define RATE_MNG_TX_FAIL_HIGH_TH_DEF      10
#define RATE_MNG_TX_FAIL_HIGH_TH_MIN      0
#define RATE_MNG_TX_FAIL_HIGH_TH_MAX      256

#define RATE_MNG_PER_ALPHA_SHIFT_DEF       4
#define RATE_MNG_PER_ALPHA_SHIFT_MIN       0
#define RATE_MNG_PER_ALPHA_SHIFT_MAX       256

#define RATE_MNG_PER_ADD_SHIFT_DEF         13
#define RATE_MNG_PER_ADD_SHIFT_MIM         0
#define RATE_MNG_PER_ADD_SHIFT_MAX         256

#define RATE_MNG_PER_BETA1_SHIFT_DEF       10
#define RATE_MNG_PER_BETA1_SHIFT_MIN       0
#define RATE_MNG_PER_BETA1_SHIFT_MAX       256

#define RATE_MNG_PER_BETA2_SHIFT_DEF	   8
#define RATE_MNG_PER_BETA2_SHIFT_MIN       0
#define RATE_MNG_PER_BETA2_SHIFT_MAX       256

#define RATE_MNG_MAX_PER_DEF               8100
#define RATE_MNG_MAX_PER_MIN               0
#define RATE_MNG_MAX_PER_MAX               65536

#define RATE_MNG_RATE_CHECK_UP_DEF         2
#define RATE_MNG_RATE_CHECK_UP_MIN         0
#define RATE_MNG_RATE_CHECK_UP_MAX         256

#define RATE_MNG_RATE_CHECK_DOWN_DEF       12
#define RATE_MNG_RATE_CHECK_DOWN_MIN       0
#define RATE_MNG_RATE_CHECK_DOWN_MAX       256

#define RATE_MNG_RATE_RETRY_POLICY_DEF_TABLE  "1,1,1,3,3,3,6,6,6,6,9,9,9"
#define RATE_MNG_MAX_RETRY_POLICY_PARAMS_LEN  13
#define RATE_MNG_MAX_STR_LEN  255



/* Configurable Scan Rate */
#define SCAN_RATE_MODE_B_MIN                    DRV_RATE_1M
#define SCAN_RATE_MODE_B_MAX                    DRV_RATE_11M
#define SCAN_RATE_MODE_B_DEF                    DRV_RATE_2M

#define SCAN_RATE_MODE_G_MIN                    DRV_RATE_1M
#define SCAN_RATE_MODE_G_MAX                    DRV_RATE_54M
#define SCAN_RATE_MODE_G_DEF                    DRV_RATE_2M

#define SCAN_RATE_MODE_A_MIN                    DRV_RATE_6M
#define SCAN_RATE_MODE_A_MAX                    DRV_RATE_54M
#define SCAN_RATE_MODE_A_DEF                    DRV_RATE_6M

/* Probe request number during scan */
#define SCAN_PROBE_REQ_NUMBER_MIN   1
#define SCAN_PROBE_REQ_NUMBER_MAX   7
#define SCAN_PROBE_REQ_NUMBER_DEF   3


#define ACX_64BITS_WEP_KEY_LENGTH_BYTES         5
#define ACX_128BITS_WEP_KEY_LENGTH_BYTES        13
#define ACX_256BITS_WEP_KEY_LENGTH_BYTES        29
#define ACX_MAX_WEP_KEY_LENGTH_BYTES            29


/* Configurable radio parameters */
#define RADIO_TX_PER_POWER_LIMITS_2_4_NORMAL_DEF_TABLE      "1d,1f,22,26,28,29"
#define RADIO_TX_PER_POWER_LIMITS_2_4_DEGRADED_DEF_TABLE    "1a,1f,22,24,26,28"
#define RADIO_TX_PER_POWER_LIMITS_2_4_EXTREME_DEF_TABLE     "16,1d,1e,20,24,25"


#define RADIO_TX_PER_POWER_LIMITS_2_4_11B_DEF_TABLE         "50,50,50,50,50,50,50,50,50,50,50,50,50,50"
#define RADIO_TX_PER_POWER_LIMITS_2_4_OFDM_DEF_TABLE        "50,50,50,50,50,50,50,50,50,50,50,50,50,50"
#define RADIO_TX_PA_GAIN_VS_BIAS_OFFSET_2_4_DEF_TABLE       "f3,54,55,56,57,58"
#define RADIO_TX_PD_VS_RATE_OFFSET_2_4_DEF_TABLE            "01,02,02,02,02,00"
#define RADIO_TX_BIAS_2_4_DEF_TABLE                         "11,11,15,11,15,0f"
#define RADIO_TX_BIP_REF_VOLTAGE_DEF_TABLE_5G               "173,188,187,18b,18a,186,18c"
#define RADIO_TX_BIP_REF_POWER_DEF_TABLE_5G                 "80,80,80,80,80,80,80"
#define RADIO_TX_BIP_OFF_BD_5G                              "00,00,00,00,00,00"


#define RADIO_TX_PER_POWER_LIMITS_5_NORMAL_DEF_TABLE        "50,50,50,50,50,50"
#define RADIO_TX_PER_POWER_LIMITS_5_DEGRADED_DEF_TABLE      "50,50,50,50,50,50"
#define RADIO_TX_PER_POWER_LIMITS_5_EXTREME_DEF_TABLE       "50,50,50,50,50,50"

#define RADIO_TX_PER_POWER_LIMITS_5_OFDM_DEF_TABLE          "50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50"
#define RADIO_TX_PD_VS_RATE_OFFSET_5_DEF_TABLE              "00,00,00,00,00,00"
#define RADIO_TX_BIAS_5_DEF_TABLE                           "10,10,10,10,10,10"
#define RADIO_RX_FEM_INSERT_LOSS_5_DEF_TABLE                "10,10,10,10,10,10,10"
#define RADIO_RX_TRACE_INSERT_LOSS_5_DEF_TABLE              "00,00,00,00,00,00,00"
#define RADIO_TX_TRACE_LOSS_5_DEF_TABLE                     "00,00,00,00,00,00,00"

#define RADIO_RX_FEM_INSERT_LOSS_2_4_MIN                    0
#define RADIO_RX_FEM_INSERT_LOSS_2_4_DEF                    375  /* 0x15e
 */
#define RADIO_RX_FEM_INSERT_LOSS_2_4_MAX                    0xffff


#define RADIO_DEGRADED_LOW_TO_NORMAL_THR_2_4G_MIN        0
#define RADIO_DEGRADED_LOW_TO_NORMAL_THR_2_4G_DEF        30      /* 0x1e */
#define RADIO_DEGRADED_LOW_TO_NORMAL_THR_2_4G_MAX        255

#define RADIO_NORMAL_TO_DEGRADED_HIGH_THR_2_4G_MIN       0
#define RADIO_NORMAL_TO_DEGRADED_HIGH_THR_2_4G_DEF       45      /* 0x2d */
#define RADIO_NORMAL_TO_DEGRADED_HIGH_THR_2_4G_MAX       255

#define RADIO_DEGRADED_LOW_TO_NORMAL_THR_5G_MIN          0
#define RADIO_DEGRADED_LOW_TO_NORMAL_THR_5G_DEF          30      /* 0x1e */
#define RADIO_DEGRADED_LOW_TO_NORMAL_THR_5G_MAX          255

#define RADIO_NORMAL_TO_DEGRADED_HIGH_THR_5G_MIN         0
#define RADIO_NORMAL_TO_DEGRADED_HIGH_THR_5G_DEF         45      /* 0x2d */
#define RADIO_NORMAL_TO_DEGRADED_HIGH_THR_5G_MAX         255


#define RADIO_RX_TRACE_INSERT_LOSS_2_4_MIN                  0
#define RADIO_RX_TRACE_INSERT_LOSS_2_4_DEF                  0
#define RADIO_RX_TRACE_INSERT_LOSS_2_4_MAX                  255

#define RADIO_RX_RSSI_PROCESS_2_4_DEF_TABLE                 "7a,7b,7c,7d,7e,7f,80,81,82,83,84,85,86,87,88"
#define RADIO_RX_RSSI_PROCESS_5_DEF_TABLE                   "00,00,00,00,00,00,00,00,00,00,00,00,00,00,00"

#define RADIO_TX_PER_CH_POWER_COMPENSATION_2_4_DEF	       "00,00,00,00,00,00,00"
#define RADIO_TX_PER_CH_POWER_COMPENSATION_5_DEF	       "00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00"

#define RADIO_FREF_CLOCK_SETTING_TIME_MIN    				0
#define RADIO_FREF_CLOCK_SETTING_TIME_DEF    				5
#define RADIO_FREF_CLOCK_SETTING_TIME_MAX    				15

#define RADIO_FREF_CLOCK_ENABLED_MIN                        TI_FALSE
#define RADIO_FREF_CLOCK_ENABLED_MAX                        TI_TRUE
#define RADIO_FREF_CLOCK_ENABLED_DEF                        TI_FALSE

#define RADIO_FREF_CLOCK_MIN                        		0	/* 19.2, 26, 38.4, 52 MHz */
#define RADIO_FREF_CLOCK_DEF                        		2
#define RADIO_FREF_CLOCK_MAX                        		31


/* 
 * Driver-Main parameters values
 */
/* Set WLAN driver thread priority - Note that this is an OS dependent parameter! */
#define WLAN_DRV_THREAD_PRIORITY_MIN                        0
#define WLAN_DRV_THREAD_PRIORITY_MAX                        255
#define WLAN_DRV_THREAD_PRIORITY_DEF                        251

/* Set bus driver thread priority - Note that this is an OS dependent parameter! */
#define BUS_DRV_THREAD_PRIORITY_MIN                         0
#define BUS_DRV_THREAD_PRIORITY_MAX                         255
#define BUS_DRV_THREAD_PRIORITY_DEF                         251

/* In SDIO block-mode:  BlkSize = 1 << BlkSizeShift    (current block size is: 1<<9 = 512 bytes) */
#define SDIO_BLK_SIZE_SHIFT_MIN                             0
#define SDIO_BLK_SIZE_SHIFT_MAX                             16
#define SDIO_BLK_SIZE_SHIFT_DEF                             9


/*****************************************************************************
 **         POWER MANAGER MODULE REGISTRY DEFINITIONS                       **
 *****************************************************************************/
/** \enum PowerMode_e */
/* MUST be sync with OS_802_11_POWER_PROFILE */
typedef enum 
{
    POWER_MODE_AUTO,        /**< In this mode the power manager module is toggle states
                             * (ACTIVE, SHORT_DOZE and LONG_DOZE) by its own inner algorithm.
                             */

    POWER_MODE_ACTIVE,      /**< In this mode there is no power save, the host interface & the radio
                             * is always active. The TNET is constantly awake. This mode is used,
                             * for example, when the device is powered from an AC power source,
                             * and provides maximum throughput and minimal latency.
                             */

    POWER_MODE_SHORT_DOZE,  /**< In this mode the system is going to ELP state and awakes (by the
                             * FW) every beacon. The F/W wakes up the host on every Beacon passes
                             * the Beacon to the driver and returns to ELP Doze as soon as possible.
                             */

    POWER_MODE_LONG_DOZE,    /**< In this mode the system is going to ELP state and awakes (by the
                             * FW) every DTIM or listen interval. This mode consumes low power,
                             * while still waking-up for Beacons once in a while. The system spends
                             * a lot of time in ELP-Doze, and the F/W rarely wakes up the host.
                             */

    POWER_MODE_PS_ONLY,     /**< In this mode the system is setting the Ps as ON. 
                             * the ELP state is changing to SHORT or LONG DOZE (According to last configuration). 
                             * Auto mode won't be used here.
                             */

    POWER_MODE_MAX
}PowerMgr_PowerMode_e;


/** \enum PowerMgr_Priority_e */
typedef enum 
{
    POWER_MANAGER_USER_PRIORITY,           /**< indicates the default user priority. */
    POWER_MANAGER_SG_PRIORITY,             /**< Indicate the Soft Gemini priority */
    POWER_MANAGER_REAUTH_PRIORITY,         /**< Indicate 802.1x reauthentication priority */
    POWER_MANAGER_PS_POLL_FAILURE_PRIORITY,/**< After receiving the PsPoll failure event */
    POWER_MANAGER_MAX_PRIORITY                                                      
}PowerMgr_Priority_e;


enum PowerMgr_registryDefinitions
{
    POWER_MODE_MIN_VALUE = POWER_MODE_AUTO,
    POWER_MODE_MAX_VALUE = POWER_MODE_LONG_DOZE,
    POWER_MODE_DEF_VALUE = POWER_MODE_AUTO,

    BEACON_RECEIVE_TIME_MIN_VALUE = 10,
    BEACON_RECEIVE_TIME_MAX_VALUE = 1000,
    BEACON_RECEIVE_TIME_DEF_VALUE = 50,

    BASE_BAND_WAKE_UP_TIME_MIN_VALUE = 100,      /* in micro seconds */
    BASE_BAND_WAKE_UP_TIME_MAX_VALUE = 10000,
    BASE_BAND_WAKE_UP_TIME_DEF_VALUE = 2000,

    PLL_LOCK_TIME_MIN_VALUE = 500,
    PLL_LOCK_TIME_MAX_VALUE = 20000,
    PLL_LOCK_TIME_DEF_VALUE = 4000,

    HANGOVER_PERIOD_MIN_VALUE = 5,
    HANGOVER_PERIOD_MAX_VALUE = 255,
    HANGOVER_PERIOD_DEF_VALUE = 10,

    BEACON_LISTEN_INTERVAL_MIN_VALUE = 1,
    BEACON_LISTEN_INTERVAL_MAX_VALUE = 50,
    BEACON_LISTEN_INTERVAL_DEF_VALUE = 1,

    DTIM_LISTEN_INTERVAL_MIN_VALUE = 1,
    DTIM_LISTEN_INTERVAL_MAX_VALUE = 50,
    DTIM_LISTEN_INTERVAL_DEF_VALUE = 1,

    BEACON_FILTERING_MIN_VALUE = 0,
    BEACON_FILTERING_MAX_VALUE = 30,
    BEACON_FILTERING_DEF_VALUE = 10,

    N_CONSECUTIVE_BEACONS_MISSED_MIN_VALUE = 0,
    N_CONSECUTIVE_BEACONS_MISSED_MAX_VALUE = 50,
    N_CONSECUTIVE_BEACONS_MISSED_DEF_VALUE = 1,

    ENTER_TO_802_11_POWER_SAVE_RETRIES_MIN_VALUE = 0,
    ENTER_TO_802_11_POWER_SAVE_RETRIES_MAX_VALUE = 50,
    ENTER_TO_802_11_POWER_SAVE_RETRIES_DEF_VALUE = 5,

    AUTO_POWER_MODE_INTERVAL_MIN_VALUE = 100,
    AUTO_POWER_MODE_INTERVAL_MAX_VALUE = 30000,
    AUTO_POWER_MODE_INTERVAL_DEF_VALUE = 1000,

    AUTO_POWER_MODE_ACTIVE_TH_MIN_VALUE = 2,
    AUTO_POWER_MODE_ACTIVE_TH_MAX_VALUE = 30000,
    AUTO_POWER_MODE_ACTIVE_TH_DEF_VALUE = 15,

    AUTO_POWER_MODE_DOZE_TH_MIN_VALUE = 1,
    AUTO_POWER_MODE_DOZE_TH_MAX_VALUE = 30000,
    AUTO_POWER_MODE_DOZE_TH_DEF_VALUE = 8,

    AUTO_POWER_MODE_DOZE_MODE_MIN_VALUE = POWER_MODE_SHORT_DOZE,
    AUTO_POWER_MODE_DOZE_MODE_MAX_VALUE = POWER_MODE_LONG_DOZE,
    AUTO_POWER_MODE_DOZE_MODE_DEF_VALUE = POWER_MODE_SHORT_DOZE,

    DCO_ITRIM_ENABLE_MIN = TI_FALSE,
    DCO_ITRIM_ENABLE_MAX = TI_TRUE,
    DCO_ITRIM_ENABLE_DEF = TI_FALSE,

    DCO_ITRIM_MODERATION_TIMEOUT_MIN = 10000,
    DCO_ITRIM_MODERATION_TIMEOUT_MAX = 1000000,
    DCO_ITRIM_MODERATION_TIMEOUT_DEF = 50000,

    POWER_MGMNT_MODE_DEF_VALUE = 1,
    POWER_MGMNT_MODE_MIN_VALUE = 0,
    POWER_MGMNT_MODE_MAX_VALUE = 1,

    POWER_MGMNT_NEED_TO_SEND_NULL_PACKET_DEF_VALUE = 1,
    POWER_MGMNT_NEED_TO_SEND_NULL_PACKET_MIN_VALUE = 0,
    POWER_MGMNT_NEED_TO_SEND_NULL_PACKET_MAX_VALUE = 1,

    /*
     bit14 - "1" send Prob Request in PBCC
     bit15 - "1" short preamble, "0" long preammle
     bit0:bit12  Rates 
     */
    POWER_MGMNT_NULL_PACKET_RATE_MOD_DEF_VALUE =  ((1<<DRV_RATE_1M) | (1<<DRV_RATE_2M)),
    POWER_MGMNT_NULL_PACKET_RATE_MOD_MIN_VALUE = 0,
    POWER_MGMNT_NULL_PACKET_RATE_MOD_MAX_VALUE = 255 ,

    POWER_MGMNT_NUM_NULL_PACKET_RETRY_DEF_VALUE = 5,
    POWER_MGMNT_NUM_NULL_PACKET_RETRY_MIN_VALUE = 1,
    POWER_MGMNT_NUM_NULL_PACKET_RETRY_MAX_VALUE = 255,

	POWER_MGMNT_RE_AUTH_ACTIVE_PRIO_DEF_VALUE = 0,
    POWER_MGMNT_RE_AUTH_ACTIVE_PRIO_MIN_VALUE = 0,
    POWER_MGMNT_RE_AUTH_ACTIVE_PRIO_MAX_VALUE = 1,

    /*BET*/
    POWER_MGMNT_BET_ENABLE_MIN = 0,
    POWER_MGMNT_BET_ENABLE_MAX = 1,
    POWER_MGMNT_BET_ENABLE_DEF = 1,
    POWER_MGMNT_BET_DISABLE_THRESHOLD_MIN = 0,
    POWER_MGMNT_BET_DISABLE_THRESHOLD_MAX = 255,
    POWER_MGMNT_BET_DISABLE_THRESHOLD_DEF = 8,
    POWER_MGMNT_BET_MAX_CONSC_MIN = 1,
    POWER_MGMNT_BET_MAX_CONSC_MAX = 50,
    POWER_MGMNT_BET_MAX_CONSC_DEF = 8,
    HAL_CTRL_BET_DISABLE_THRESHOLD_MIN = 0,
    HAL_CTRL_BET_DISABLE_THRESHOLD_MAX = 255,
    HAL_CTRL_BET_DISABLE_THRESHOLD_DEF = 12,

/*--------------- Maximal time between full beacon reception ------------------*/
    POWER_MGMNT_MAX_FULL_BEACON_MIN = 0,
    POWER_MGMNT_MAX_FULL_BEACON_MAX = 10000,
    POWER_MGMNT_MAX_FULL_BEACON_DEF = 5000,

    HOST_CLK_SETTLE_TIME_USEC_MIN = 0,
    HOST_CLK_SETTLE_TIME_USEC_MAX = 30000,
    HOST_CLK_SETTLE_TIME_USEC_DEF = 5000,

    HOST_FAST_WAKE_SUPPORT_MIN = 0,
    HOST_FAST_WAKE_SUPPORT_MAX = 1,
    HOST_FAST_WAKE_SUPPORT_DEF = 0

};

/*****************************************************************************
 **         END POWER MANAGER MODULE REGISTRY DEFINITIONS                   **
 *****************************************************************************/


typedef enum
{
    ERP_PROTECTION_NONE       = 0,
    ERP_PROTECTION_STANDARD   = 1,
    ERP_PROTECTION_TI_TRICK   = 2
} erpProtectionType_e;


#endif /* _CORE_DEFAULT_PARAMS_H */

