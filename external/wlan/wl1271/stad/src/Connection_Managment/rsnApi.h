/*
 * rsnApi.h
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

/** \file utilsReplvl.h
 *  \brief Report level API
 *
 *  \see utilsReplvl.c
 */

/***************************************************************************/
/*                                                                                                  */
/*    MODULE:   utilsReplvl.h                                                               */
/*    PURPOSE:  Report level API                                            */
/*                                                                                                  */
/***************************************************************************/
#ifndef __RSN_API_H__
#define __RSN_API_H__

#include "tidef.h"
#include "TWDriver.h"
#include "802_11Defs.h"
#include "DrvMainModules.h"

/* Constants */

#define RSN_MAC_ADDR_LEN            6
#define MAX_KEY_RSC_LEN             8
#define MAX_SSN_KEY_DATA_LENGTH     32
#define RSN_AUTH_FAILURE_TIMEOUT    30000
#define MAX_NUM_OF_PRE_AUTH_BSSIDS  16
#define MAX_KEYS_NUM                4


/* Enumerations */



/** RSN supported authentication suites */
typedef enum
{
    RSN_AUTH_OPEN           = 0,        /*< Legacy Open authentication suite */
    RSN_AUTH_SHARED_KEY     = 1,        /*< Legacy Shared Key authentication suite */
    RSN_AUTH_AUTO_SWITCH    = 2,        /*< Automatic authentication suite */
    RSN_AUTH_NONE           = 255       /*< no authentication suite */

} EAuthSuite;


/* Available External authentication modes for admission control */
typedef enum
{
    RSN_EXT_AUTH_MODE_OPEN           =   RSN_AUTH_OPEN,
    RSN_EXT_AUTH_MODE_SHARED_KEY     =   RSN_AUTH_SHARED_KEY,
    RSN_EXT_AUTH_MODE_AUTO_SWITCH    =   RSN_AUTH_AUTO_SWITCH,
    RSN_EXT_AUTH_MODE_WPA,
    RSN_EXT_AUTH_MODE_WPAPSK,
    RSN_EXT_AUTH_MODE_WPANONE,
    RSN_EXT_AUTH_MODE_WPA2,
    RSN_EXT_AUTH_MODE_WPA2PSK,
    /* Not a real mode, defined as upper bound */
    RSN_EXT_AUTH_MODEMAX          

} EExternalAuthMode;


typedef enum
{
    RSN_AUTH_STATUS_INVALID_TYPE                = 0x0001,
    RSN_AUTH_STATUS_TIMEOUT                     = 0x0002,
    RSN_AUTH_STATUS_CHALLENGE_FROM_AP_FAILED    = 0x0003,
    RSN_AUTH_STATUS_CHALLENGE_TO_AP_FAILED      = 0x0004

} EAuthStatus;


/** RSN key management suites */
typedef enum 
{
    RSN_KEY_MNG_NONE                = 0,        /**< no key management available */
    RSN_KEY_MNG_802_1X              = 1,        /**< "802.1X" key management */
    RSN_KEY_MNG_WPA                 = 2,        /**< "WPA 4 way handshake" key management */
    RSN_KEY_MNG_XCC                 = 3,        /**< "XCC" key management */
    RSN_KEY_MNG_UNKNOWN             = 255       /**< UNKNOWN key management available */

} ERsnKeyMngSuite;


/** Available cipher suites for admission control */
typedef enum 
{
    RSN_IBSS                = 0,        /**< IBSS mode */
    RSN_INFRASTRUCTURE      = 1         /**< Infrastructure mode */

} ERsnNetworkMode;


/** Port Access Entity role type */
typedef enum
{
    RSN_PAE_AP      = 0,
    RSN_PAE_SUPP    = 1

} ERsnPaeRole;


/** RSN Events */
typedef enum 
{
    RSN_EVENT_EAPOL_RECV            = 0x0,      /**< EAPOL frame received in the RX */
    RSN_EVENT_SEC_ATTACK_DETECT     = 0x1,      /**< Security Attack detection */
    RSN_EVENT_RAW_KEY_RECV          = 0x2,      /**< Raw key recive */
    RSN_EVENT_KEY_REMOVE            = 0x3       /**< Key remove event */

} ERsnEvent;  


/** Site ben levels */
typedef enum 
{
    RSN_SITE_BAN_LEVEL_HALF = 1,
    RSN_SITE_BAN_LEVEL_FULL = 2

} ERsnSiteBanLevel;


/* Typedefs */

/** Port Access Entity structure */
typedef struct
{
    EExternalAuthMode   authProtocol;
    ERsnKeyMngSuite     keyExchangeProtocol;
    ECipherSuite        unicastSuite;
    ECipherSuite        broadcastSuite;

} TRsnPaeConfig;


typedef struct
{
    TI_BOOL             privacy;
    TI_UINT8            *pIe;
    TI_UINT8            ieLen;

} TRsnData;


typedef struct 
{
   TMacAddr             bssId;
   dot11_RSN_t          *pRsnIEs;
   TI_UINT8             rsnIeLen;

} TBssidRsnInfo;


typedef struct 
{
   TI_UINT8             NumOfItems;
   TBssidRsnInfo        bssidList[MAX_NUM_OF_PRE_AUTH_BSSIDS];

} TBssidList4PreAuth;


typedef struct
{
    EAuthSuite          authSuite;
    TI_BOOL             privacyOn;
    TSecurityKeys       keys[MAX_KEYS_NUM];
    TI_UINT8            defaultKeyId;
    EExternalAuthMode   externalAuthMode;
    TI_BOOL             mixedMode;
    TI_BOOL             WPAMixedModeEnable;
    TI_BOOL             preAuthSupport;
    TI_UINT32           preAuthTimeout;
    TI_BOOL             bRsnExternalMode;
    TI_BOOL             bPairwiseMicFailureFilter;

} TRsnInitParams;

typedef struct
{
	ScanBssType_e 				bssType;
	TMacAddr 	  				bssid;
	Tdot11HtInformationUnparse  *pHTInfo;
	Tdot11HtCapabilitiesUnparse *pHTCapabilities;
} TRsnSiteParams;


/* Prototypes */

TI_HANDLE rsn_create(TI_HANDLE hOs);

TI_STATUS rsn_unload(TI_HANDLE hRsn);

void      rsn_init (TStadHandlesList *pStadHandles);

TI_STATUS rsn_SetDefaults (TI_HANDLE hRsn, TRsnInitParams *pInitParam);

TI_STATUS rsn_reconfig(TI_HANDLE hRsn);

TI_STATUS rsn_start(TI_HANDLE hRsn);

TI_STATUS rsn_stop(TI_HANDLE hRsn, TI_BOOL removeKeys);

TI_STATUS rsn_eventRecv(TI_HANDLE hRsn, ERsnEvent event, void* pData);

TI_STATUS rsn_setParam(TI_HANDLE hCtrlData, void *pParam);

TI_STATUS rsn_getParamEncryptionStatus(TI_HANDLE hRsn, ECipherSuite *rsnStatus);
TI_STATUS rsn_getParam(TI_HANDLE hCtrlData, void *pParam);

TI_STATUS rsn_evalSite(TI_HANDLE hRsn, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pMetric);

TI_STATUS rsn_setSite(TI_HANDLE hRsn, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen);

TI_STATUS rsn_getInfoElement(TI_HANDLE hRsn, TI_UINT8 *pRsnIe, TI_UINT32 *pRsnIeLen);

#ifdef XCC_MODULE_INCLUDED
TI_STATUS rsn_getXCCExtendedInfoElement(TI_HANDLE hRsn, TI_UINT8 *pRsnIe, TI_UINT8 *pRsnIeLen);
#endif

TI_STATUS rsn_reportAuthFailure(TI_HANDLE hRsn, EAuthStatus authStatus);

TI_STATUS rsn_reportMicFailure(TI_HANDLE hRsn, TI_UINT8 *pType, TI_UINT32 Length);

TI_STATUS rsn_resetPMKIDList(TI_HANDLE hRsn);

TI_STATUS rsn_removedDefKeys(TI_HANDLE hRsn);

TI_STATUS rsn_startPreAuth(TI_HANDLE hRsn, TBssidList4PreAuth *pBssidList);

ERsnSiteBanLevel rsn_banSite(TI_HANDLE hRsn, TMacAddr siteBssid, ERsnSiteBanLevel banLevel, TI_UINT32 durationMs);

TI_BOOL rsn_isSiteBanned(TI_HANDLE hRsn, TMacAddr siteBssid);

void rsn_MboxFlushFinishCb(TI_HANDLE handle, TI_UINT16 MboxStatus, char *InterrogateParamsBuf);

TI_STATUS rsn_setPortStatus(TI_HANDLE hRsn, TI_BOOL state);

void rsn_reAuth(TI_HANDLE hRsn);

#endif /* __RSN_API_H__*/
