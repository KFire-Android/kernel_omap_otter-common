/*
 * rsn.h
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

/** \file assocSM.h
 *  \brief 802.11 Association SM
 *
 *  \see assocSM.c
 */


/***************************************************************************/
/*                                                                         */
/*      MODULE: assocSM.h                                                  */
/*    PURPOSE:  802.11 Association SM                                      */
/*                                                                         */
/***************************************************************************/

#ifndef _RSN_H
#define _RSN_H

#include "rsnApi.h"
#include "paramOut.h"
#include "siteHash.h"

/* Constants */
#define RSN_MAX_IDENTITY_LEN            64
#define RSN_MAX_PASSWD_LEN              128

#define RSN_MAX_NUMBER_OF_EVENTS        6
#define RSN_MAX_NUMBER_OF_BANNED_SITES  16
#define RSN_MIC_FAILURE_REPORT_TIMEOUT     500
#define RSN_MIC_FAILURE_TIMEOUT         (60000 + RSN_MIC_FAILURE_REPORT_TIMEOUT)
#define RSN_MAIN_KEYS_SESSION_TIMEOUT   RSN_AUTH_FAILURE_TIMEOUT 
#define RSN_MIC_FAILURE_RE_KEY_TIMEOUT  3000

/* Enumerations */
typedef enum 
{
    MIC_FAILURE_FALSE,
    MIC_FAILURE_TRUE,
    MIC_FAILURE_BLOCK
} rsn_micFailureStatus_e;

typedef enum 
{
    GROUP_KEY_UPDATE_FALSE,
    GROUP_KEY_UPDATE_TRUE
} rsn_groupKeyUpdate_e;

typedef enum 
{
	PAIRWISE_KEY_UPDATE_FALSE,
	PAIRWISE_KEY_UPDATE_TRUE
} rsn_pairwiseKeyUpdate_e;


/* Typedefs */
typedef struct _rsn_t   rsn_t;
         
typedef TI_STATUS (*rsn_eventCallback_t)(void* pCtx, void *pData);
typedef TI_STATUS (*rsn_setPaeConfig_t)(rsn_t *pRsn, TRsnPaeConfig *pPaeConfig);
typedef TI_STATUS (*rsn_getNetworkMode_t)(rsn_t *pRsn, ERsnNetworkMode *pNetMode);

typedef TI_STATUS (*rsn_setKey_t)(rsn_t *pMainSec, TSecurityKeys *pKey);
typedef TI_STATUS (*rsn_removeKey_t)(rsn_t *pMainSec, TSecurityKeys *pKey);
typedef TI_STATUS (*rsn_setDefaultKeyId_t)(rsn_t *pMainSec, TI_UINT8 keyId);
typedef TI_STATUS (*rsn_reportStatus_t)(rsn_t *pRsn, TI_STATUS rsnStatus);
typedef TI_STATUS (*rsn_sendEapol_t)(rsn_t *pRsn, TI_UINT8 *pPacket, TI_UINT32 length);
typedef TI_STATUS (*rsn_getSiteEntry_t)(rsn_t *pRsn, TMacAddr *macAddress, siteEntry_t *curSiteEntry);
typedef TI_BOOL (*rsn_getPortStatus_t)(rsn_t *pRsn);
typedef TI_STATUS (*rsn_setPortStatus_t)(TI_HANDLE hRsn, TI_BOOL state);

typedef struct
{
    rsn_eventCallback_t     eventFunc;
    void                    *pCtx;
} rsn_eventStruct_t;

typedef struct
{
    char                 id[RSN_MAX_IDENTITY_LEN];       /**< User identity string */
    TI_UINT8             idLength;                       /**< User identity string length */
    char                 password[RSN_MAX_PASSWD_LEN];   /**< User password string */
    TI_UINT8             pwdLength;                      /**< User password string length */
} authIdentity_t;

typedef struct 
{
    ERsnSiteBanLevel        banLevel;
    TI_UINT32               banStartedMs;
    TI_UINT32               banDurationMs;
    TMacAddr                siteBssid;
} rsn_siteBanEntry_t;

struct _rsn_t
{
    rsn_eventStruct_t      events[RSN_MAX_NUMBER_OF_EVENTS];
    TRsnPaeConfig          paeConfig;
    TI_BOOL                PrivacyOptionImplemented;
    
    TSecurityKeys          keys[MAX_KEYS_NUM];
    TI_BOOL                keys_en [MAX_KEYS_NUM];
    TI_UINT8               defaultKeyId;
    TI_BOOL                defaultKeysOn;
    TI_BOOL                wepDefaultKeys[MAX_KEYS_NUM];
    TI_BOOL                wepStaticKey;
    rsn_groupKeyUpdate_e   eGroupKeyUpdate;
    rsn_pairwiseKeyUpdate_e	ePairwiseKeyUpdate;
    OS_802_11_EAP_TYPES    eapType;

	rsnGenericIE_t         genericIE;
    rsn_siteBanEntry_t     bannedSites[RSN_MAX_NUMBER_OF_BANNED_SITES];
    TI_UINT8               numOfBannedSites;

    TI_HANDLE              hMicFailureReportWaitTimer;
	TI_HANDLE			   hMicFailureGroupReKeyTimer;
	TI_HANDLE              hMicFailurePairwiseReKeyTimer;
    TI_BOOL                bPairwiseMicFailureFilter;

    struct _admCtrl_t      *pAdmCtrl;
    struct _mainSec_t      *pMainSecSm;

    struct _keyParser_t    *pKeyParser;

    TI_HANDLE              hTxCtrl;
    TI_HANDLE              hRx;
    TI_HANDLE              hConn;
    TI_HANDLE              hCtrlData;
    TI_HANDLE              hTWD;
    TI_HANDLE              hSiteMgr;
    TI_HANDLE              hReport;
    TI_HANDLE              hOs;
    TI_HANDLE              hXCCMngr;
    TI_HANDLE              hEvHandler;
    TI_HANDLE              hSmeSm;
    TI_HANDLE              hAPConn;
    TI_HANDLE              hMlme;
    TI_HANDLE              hPowerMgr;
    TI_HANDLE              hTimer;
    TI_HANDLE              hCurrBss;
    
    rsn_setPaeConfig_t     setPaeConfig;
    rsn_getNetworkMode_t   getNetworkMode;
    rsn_setKey_t           setKey;
    rsn_removeKey_t        removeKey;
    rsn_setDefaultKeyId_t  setDefaultKeyId;
    rsn_reportStatus_t     reportStatus;
    rsn_setPortStatus_t    setPortStatus;
    rsn_getPortStatus_t    getPortStatus;

    TI_UINT32              rsnStartedTs;
    TI_UINT32              rsnCompletedTs;
    TI_BOOL                bRsnExternalMode;
};

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS rsn_reportStatus(rsn_t *pRsn, TI_STATUS status);

TI_STATUS rsn_setPaeConfig(rsn_t *pRsn, TRsnPaeConfig *pPaeConfig);

TI_STATUS rsn_getNetworkMode(rsn_t *pRsn, ERsnNetworkMode *pNetMode);

TI_STATUS rsn_setKey(rsn_t *pMainSec, TSecurityKeys *pKey);

TI_STATUS rsn_removeKey(rsn_t *pMainSec, TSecurityKeys *pKey);

TI_STATUS rsn_setDefaultKeyId(rsn_t *pMainSec, TI_UINT8 keyId);

TI_STATUS rsn_setDefaultKeys(rsn_t *pHandle);

TI_BOOL rsn_getPortStatus(rsn_t *pRsn);

TI_STATUS rsn_setPortStatus(TI_HANDLE hRsn, TI_BOOL state);

TI_STATUS rsn_getGenInfoElement(rsn_t *pRsn, TI_UINT8 *out_buff, TI_UINT32 *out_buf_length);

void rsn_clearGenInfoElement(rsn_t *pRsn);

#endif

