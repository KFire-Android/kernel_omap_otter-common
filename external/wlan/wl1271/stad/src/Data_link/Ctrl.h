/*
 * Ctrl.h
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


/***************************************************************************/
/*                                                                         */
/*    MODULE:   tx.h                                                       */
/*    PURPOSE:  Tx module Header file                                      */
/*                                                                         */
/***************************************************************************/
#ifndef _CTRL_H_
#define _CTRL_H_

#include "tidef.h"
#include "paramOut.h"
#include "rxXfer_api.h"
#include "DataCtrl_Api.h"


#define     DEF_CURRENT_PREAMBLE                        PREAMBLE_LONG
#define     ALL_RATES_AVAILABLE                         0xFFFFFFFF                      

typedef enum
{
    CTRL_DATA_TRAFFIC_INTENSITY_HIGH_CROSSED_ABOVE,
    CTRL_DATA_TRAFFIC_INTENSITY_HIGH_CROSSED_BELOW,
    CTRL_DATA_TRAFFIC_INTENSITY_LOW_CROSSED_ABOVE,
    CTRL_DATA_TRAFFIC_INTENSITY_LOW_CROSSED_BELOW,
    CTRL_DATA_TRAFFIC_INTENSITY_MAX_EVENTS
} ctrlData_trafficIntensityEvents_e;


#define TS_EXCEEDS(currTime,expTime) (currTime > expTime)
#define TS_ADVANCE(currTime,expTime,delta) (expTime = currTime + (delta))

typedef struct 
{
    TI_UINT32 supportedRatesMask;
    TI_UINT32 policyClassRateMask;
    TI_UINT32 fwPolicyID;
}tsrsParameters_t;

typedef struct
{
    TI_HANDLE               hSiteMgr;
    TI_HANDLE               hTxCtrl;
    TI_HANDLE               hRxData;
    TI_HANDLE               hTWD;  
    TI_HANDLE               hOs;
    TI_HANDLE               hReport;
    TI_HANDLE               hAPConn;
    TI_HANDLE               hEvHandler;
    TI_HANDLE               hTrafficMonitor;
    TI_HANDLE               hTxDataQ;
    TI_HANDLE               hStaCap;
    
    TMacAddr                ctrlDataCurrentBSSID; 
    ScanBssType_e           ctrlDataCurrentBssType; 
    TI_UINT32               ctrlDataCurrentRateMask;
    EPreamble               ctrlDataCurrentPreambleType; 
    TMacAddr                ctrlDataDeviceMacAddress; 
    TI_BOOL                 ctrlDataProtectionEnabled;
    RtsCtsStatus_e          ctrlDataRtsCtsStatus;
    erpProtectionType_e     ctrlDataIbssProtectionType;
    erpProtectionType_e     ctrlDataDesiredIbssProtection; /* 0 = CTS protaction disable ; 1 = Standard CTS protaction */

    /*
     * txRatePolicy section
     */

    /* txRatePolicies - here we store the policy and set it to the FW */
    TTxRatePolicy           ctrlDataTxRatePolicy;

    /* number of retries for each rate in each class in the policy that we set to the FW */ 
	TI_UINT32               policyEnabledRatesMaskCck;
	TI_UINT32               policyEnabledRatesMaskOfdm;
	TI_UINT32               policyEnabledRatesMaskOfdmA;
	TI_UINT32               policyEnabledRatesMaskOfdmN;

    TI_UINT32               uCurrPolicyEnabledRatesMask;    /* holds the current used En/Dis Rates Mask */
    TI_UINT32               uMgmtPolicyId;                  /* the management packets policy ID */

#ifdef XCC_MODULE_INCLUDED
    /* Callback for update retries in Link Test */
    retriesCB_t             retriesUpdateCBFunc;
    TI_HANDLE               retriesUpdateCBObj;
#endif

    /* Flag to indicate whether traffic intensity events should be sent or not */
    TI_BOOL                 ctrlDataTrafficIntensityEventsEnabled;
    OS_802_11_TRAFFIC_INTENSITY_THRESHOLD_PARAMS ctrlDataTrafficIntensityThresholds;
    TI_HANDLE               ctrlDataTrafficThresholdEvents[CTRL_DATA_TRAFFIC_INTENSITY_MAX_EVENTS];

    tsrsParameters_t        tsrsParameters[MAX_NUM_OF_AC];

    /* holds last fragmentation threshold */
    TI_UINT16               lastFragmentThreshold;

} ctrlData_t;


#endif
