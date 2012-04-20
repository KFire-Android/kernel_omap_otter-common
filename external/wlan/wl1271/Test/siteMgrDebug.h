/*
 * siteMgrDebug.h
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

#ifndef __SITE_MGR_DEBUG_H__
#define __SITE_MGR_DEBUG_H__

#include "tidef.h"
#include "paramOut.h"
#include "DrvMainModules.h"

#define	SITE_MGR_DEBUG_HELP_MENU			0
#define	PRIMARY_SITE_DBG					1
#define	SITE_TABLE_DBG						2
#define	DESIRED_PARAMS_DBG					3


#define	SET_RSN_DESIRED_CIPHER_SUITE_DBG	15
#define	GET_RSN_DESIRED_CIPHER_SUITE_DBG	16
#define	SET_RSN_DESIRED_AUTH_TYPE_DBG		17
#define	GET_RSN_DESIRED_AUTH_TYPE_DBG		18
#define	GET_PRIMARY_SITE_DESC_DBG			19
#define	GET_CONNECTION_STATUS_DBG			20
#define	GET_CURRENT_TX_RATE_DBG				22
#define	SET_SUPPORTED_RATE_SET_DBG			25
#define	GET_SUPPORTED_RATE_SET_DBG			26
#define	SET_MLME_LEGACY_AUTH_TYPE_DBG		27
#define	GET_MLME_LEGACY_AUTH_TYPE_DBG		28
#define RADIO_STAND_BY_CHANGE_STATE			31
#define CONNECT_TO_BSSID					34


#define SET_START_CLI_SCAN_PARAM							35
#define SET_STOP_CLI_SCAN_PARAM								36

#define SET_BROADCAST_BACKGROUND_SCAN_PARAM					37
#define ENABLE_PERIODIC_BROADCAST_BACKGROUND_SCAN_PARAM		38
#define DISABLE_PERIODIC_BROADCAST_BACKGROUND_SCAN_PARAM	39

#define SET_UNICAST_BACKGROUND_SCAN_PARAM					40
#define ENABLE_PERIODIC_UNICAST_BACKGROUND_SCAN_PARAM		41
#define DISABLE_PERIODIC_UNICAST_BACKGROUND_SCAN_PARAM		42

#define SET_FOREGROUND_SCAN_PARAM							43
#define ENABLE_PERIODIC_FOREGROUND_SCAN_PARAM				44
#define DISABLE_PERIODIC_FOREGROUND_SCAN_PARAM				45

#define SET_CHANNEL_NUMBER									46
#define SET_RSSI_GAP_THRSH									47
#define SET_FAST_SCAN_TIMEOUT								48
#define SET_INTERNAL_ROAMING_ENABLE							49

#define PERFORM_HEALTH_TEST									50
#define PRINT_FAILURE_EVENTS								51
#define FORCE_HW_RESET_RECOVERY								52
#define FORCE_SOFT_RECOVERY									53 


#define RESET_ROAMING_EVENTS								55 
#define SET_DESIRED_CONS_TX_ERRORS_THREH					56
#define GET_CURRENT_ROAMING_STATUS							57
#define SET_DESIRED_CHANNEL									58


#define TEST_TOGGLE_LNA_ON                                  60 
#define TEST_TOGGLE_LNA_OFF   								61 


#define TEST_TOGGLE_LNA_ON                                  60 
#define TEST_TOGGLE_LNA_OFF   								61 

#define PRINT_SITE_TABLE_PER_SSID							70

#define ROAM_TEST1											81
#define ROAM_TEST2											82
#define ROAM_TEST3											83
#define ROAM_TEST4											84
#define ROAM_TEST5											85
#define ROAM_TEST6											86
#define ROAM_TEST7											87


#define START_PRE_AUTH										100


void siteMgrDebugFunction (TI_HANDLE         hSiteMgr, 
                           TStadHandlesList *pStadHandles,
                           TI_UINT32         funcType, 
                           void             *pParam);

#endif /* __SITE_MGR_DEBUG_H__ */
