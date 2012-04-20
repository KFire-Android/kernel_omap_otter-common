/*
 * admCtrl.h
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

/** \file admCtrl.h
 *  \brief Admission control API
 *
 *  \see admCtrl.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Admission Control                                             *
 *   PURPOSE: Admission Control Module API                                  *
 *                                                                          *
 ****************************************************************************/

#ifndef _ADM_CTRL_H_
#define _ADM_CTRL_H_

#include "rsnApi.h"
#include "TWDriver.h"

/* Constants */

/* Enumerations */

/* Typedefs */

typedef struct _admCtrl_t   admCtrl_t;

/* RSN admission control prototypes */

typedef TI_STATUS (*admCtrl_setAuthSuite_t)(admCtrl_t *pAdmCtrl, EAuthSuite authSuite);

typedef TI_STATUS (*admCtrl_getAuthSuite_t)(admCtrl_t *pAdmCtrl, EAuthSuite *pSuite);

typedef TI_STATUS (*admCtrl_setNetworkMode_t)(admCtrl_t *pAdmCtrl, ERsnNetworkMode mode);

typedef TI_STATUS (*admCtrl_setUcastCipherSuite_t)(admCtrl_t *pAdmCtrl, ECipherSuite suite);

typedef TI_STATUS (*admCtrl_setBcastCipherSuite_t)(admCtrl_t *pAdmCtrl, ECipherSuite suite);

typedef TI_STATUS (*admCtrl_getCipherSuite_t)(admCtrl_t *pAdmCtrl, ECipherSuite *pSuite);

typedef TI_STATUS (*admCtrl_setKeyMngSuite_t)(admCtrl_t *pAdmCtrl, ERsnKeyMngSuite suite);

typedef TI_STATUS (*admCtrl_setExtAuthMode_t)(admCtrl_t *pAdmCtrl, EExternalAuthMode extAuthMode);

typedef TI_STATUS (*admCtrl_getExtAuthMode_t)(admCtrl_t *pAdmCtrl, EExternalAuthMode *pExtAuthMode);

typedef TI_STATUS (*admCtrl_getInfoElement_t)(admCtrl_t *pAdmCtrl, TI_UINT8 *pIe, TI_UINT32 *pLength);

typedef TI_STATUS (*admCtrl_setSite_t)(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen);

typedef TI_STATUS (*admCtrl_evalSite_t)(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pEvaluation);

typedef TI_STATUS (*admCtrl_setMixedMode_t)(admCtrl_t *pAdmCtrl, TI_BOOL mixedMode);

typedef TI_STATUS (*admCtrl_getMixedMode_t)(admCtrl_t *pAdmCtrl, TI_BOOL *mixedMode);

typedef TI_STATUS (*admCtrl_getAuthEncrCapability_t)(admCtrl_t *pAdmCtrl, 
                                        rsnAuthEncrCapability_t   *authEncrCapability);

typedef TI_STATUS (*admCtrl_setPMKIDlist_t)(admCtrl_t *pAdmCtrl, OS_802_11_PMKID  *pmkIdList);

typedef TI_STATUS (*admCtrl_getPMKIDlist_t)(admCtrl_t *pAdmCtrl, OS_802_11_PMKID  *pmkIdList);

typedef TI_STATUS (*admCtrl_resetPMKIDlist_t)(admCtrl_t *pAdmCtrl);

typedef TI_STATUS (*admCtrl_sendPMKIDCandListAfterDelay_t)(admCtrl_t *pAdmCtrl, TI_UINT32 delay);

typedef TI_STATUS (*admCtrl_setPromoteFlags_t)(admCtrl_t *pAdmCtrl, TI_UINT32 flags);

typedef TI_STATUS (*admCtrl_getPromoteFlags_t)(admCtrl_t *pAdmCtrl, TI_UINT32 *flags);

typedef TI_STATUS (*admCtrl_getWPAMixedModeSupport_t)(admCtrl_t *pAdmCtrl, TI_UINT32 *support);

#ifdef XCC_MODULE_INCLUDED
typedef TI_STATUS (*admCtrl_getNetworkEap_t)(admCtrl_t *pAdmCtrl, OS_XCC_NETWORK_EAP *networkEap);

typedef TI_STATUS (*admCtrl_setNetworkEap_t)(admCtrl_t *pAdmCtrl, OS_XCC_NETWORK_EAP networkEap);
#endif


typedef TI_BOOL (*admCtrl_getPreAuthStatus_t)(admCtrl_t *pAdmCtrl, TMacAddr *givenAP, TI_UINT8 *cacheIndex);

typedef TI_STATUS (*admCtrl_startPreAuth_t)(admCtrl_t *pAdmCtrl, TBssidList4PreAuth *pBssidList);

typedef TI_STATUS (*admCtrl_get802_1x_AkmExists_t)(admCtrl_t *pAdmCtrl, TI_BOOL *wpa_802_1x_AkmExists);
/* Constants */

/* Flags for Any-WPA (WPA Mixed) mode) - set by the Supplicant  */
#define ADMCTRL_WPA_OPTION_NONE  							0x00000000
#define ADMCTRL_WPA_OPTION_ENABLE_PROMOTE_AUTH_MODE  	0x00000001
#define ADMCTRL_WPA_OPTION_ENABLE_PROMOTE_CIPHER     	0x00000002

#define ADMCTRL_WPA_OPTION_MAXVALUE                  	0x00000003


/* Structures */

/* PMKID cache structures                        */
/* (PMKID cache used for WPA2 pre-authentication */

#define PMKID_VALUE_SIZE  16
typedef TI_UINT8 pmkidValue_t[PMKID_VALUE_SIZE];

#define PMKID_MAX_NUMBER 16

typedef struct 
{
   TMacAddr         bssId;
   pmkidValue_t     pmkId;
   TI_BOOL          preAuthenticate;

} pmkidEntry_t;

#define ADMCTRL_PMKID_CACHE_SIZE 32

typedef struct 
{
   TSsid               ssid;
   TI_UINT8            entriesNumber;
   TI_UINT8            nextFreeEntry;
   pmkidEntry_t        pmkidTbl[ADMCTRL_PMKID_CACHE_SIZE];
} pmkid_cache_t;



/* Admission control object */
struct _admCtrl_t
{
    ERsnPaeRole             role;
    EAuthSuite              authSuite;
    ERsnNetworkMode         networkMode;
    EExternalAuthMode       externalAuthMode;
    ECipherSuite            unicastSuite;
    ECipherSuite            broadcastSuite;
    ERsnKeyMngSuite         keyMngSuite;
    TI_BOOL                 wpaAkmExists;
    TI_BOOL                 mixedMode;
    TI_UINT8                AP_IP_Address[4];
    TI_UINT16               replayCnt;
    TI_UINT8                aironetIeReserved[8];
    TI_BOOL                 encrInSw;
    TI_BOOL                 micInSw;
    TI_BOOL                 setSiteFirst;
#ifdef XCC_MODULE_INCLUDED
    OS_XCC_NETWORK_EAP      networkEapMode;
#endif
    TI_BOOL                 XCCSupport;
    TI_BOOL                 proxyArpEnabled;

    TI_BOOL                 WPAMixedModeEnable;
    TI_UINT32               WPAPromoteFlags;    

    TI_BOOL                 preAuthSupport;
    TI_UINT32               preAuthTimeout;
    TI_UINT8                MaxNumOfPMKIDs;
    pmkid_cache_t           pmkid_cache;

    struct _rsn_t           *pRsn;
    TI_HANDLE               hMlme;
    TI_HANDLE               hRx;
    TI_HANDLE               hReport;
    TI_HANDLE               hOs;
    TI_HANDLE               hXCCMngr;
    TI_HANDLE               hPowerMgr;
    TI_HANDLE               hEvHandler;
    TI_HANDLE               hTimer;
    TI_HANDLE               hCurrBss;
    TI_HANDLE               hSme;


    admCtrl_setAuthSuite_t              setAuthSuite;
    admCtrl_getAuthSuite_t              getAuthSuite;
    admCtrl_setNetworkMode_t            setNetworkMode;
    admCtrl_setUcastCipherSuite_t       setUcastSuite;
    admCtrl_setBcastCipherSuite_t       setBcastSuite;
    admCtrl_setExtAuthMode_t            setExtAuthMode;
    admCtrl_getExtAuthMode_t            getExtAuthMode;
    admCtrl_getCipherSuite_t            getCipherSuite;
    admCtrl_setKeyMngSuite_t            setKeyMngSuite;
    admCtrl_setMixedMode_t              setMixedMode;
    admCtrl_getMixedMode_t              getMixedMode;
    admCtrl_getInfoElement_t            getInfoElement;
    admCtrl_setSite_t                   setSite;
    admCtrl_evalSite_t                  evalSite;
    admCtrl_getAuthEncrCapability_t     getAuthEncrCap;
    admCtrl_setPMKIDlist_t              setPmkidList;
    admCtrl_getPMKIDlist_t              getPmkidList;
    admCtrl_resetPMKIDlist_t            resetPmkidList;
    admCtrl_setPromoteFlags_t           setPromoteFlags;
    admCtrl_getPromoteFlags_t           getPromoteFlags;
    admCtrl_getWPAMixedModeSupport_t    getWPAMixedModeSupport;
    admCtrl_get802_1x_AkmExists_t       get802_1x_AkmExists;


#ifdef XCC_MODULE_INCLUDED
    admCtrl_getNetworkEap_t         getNetworkEap;
    admCtrl_setNetworkEap_t         setNetworkEap;
#endif

    admCtrl_getPreAuthStatus_t      getPreAuthStatus;
    admCtrl_startPreAuth_t          startPreAuth;

    void                            *hPreAuthTimerWpa2;
    TI_UINT8                           numberOfPreAuthCandidates;
};

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

admCtrl_t* admCtrl_create(TI_HANDLE hOs);

TI_STATUS admCtrl_config (TI_HANDLE hAdmCtrl,
                          TI_HANDLE hMlme,
                          TI_HANDLE hRx,
                          TI_HANDLE hReport,
                          TI_HANDLE hOs,
                          struct _rsn_t *pRsn,
                          TI_HANDLE hXCCMngr,
                          TI_HANDLE hPowerMgr,
                          TI_HANDLE hEvHandler,
                          TI_HANDLE hTimer,
                          TI_HANDLE hCurrBss,
                          TRsnInitParams *pInitParam);

TI_STATUS admCtrl_unload(admCtrl_t *pAdmCtrl);

TI_STATUS admCtrlNone_config(admCtrl_t *pAdmCtrl);

TI_STATUS admCtrlWpa_config(admCtrl_t *pAdmCtrl);

TI_STATUS admCtrl_parseIe(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TI_UINT8 **pIe, TI_UINT8 IeId);

TI_STATUS admCtrl_subConfig(TI_HANDLE hAdmCtrl);

TI_STATUS admCtrl_nullSetPMKIDlist(admCtrl_t *pAdmCtrl, OS_802_11_PMKID  *pmkIdList);

TI_STATUS admCtrl_nullGetPMKIDlist(admCtrl_t *pAdmCtrl, OS_802_11_PMKID  *pmkIdList);

TI_STATUS admCtrl_resetPMKIDlist(admCtrl_t *pAdmCtrl);

TI_BOOL admCtrl_nullGetPreAuthStatus(admCtrl_t *pAdmCtrl, TMacAddr *givenAP, TI_UINT8 *cacheIndex);

TI_STATUS admCtrl_nullStartPreAuth(admCtrl_t *pAdmCtrl, TBssidList4PreAuth *pBssidList);

TI_STATUS admCtrl_nullGet802_1x_AkmExists(admCtrl_t *pAdmCtrl, TI_BOOL *wpa_802_1x_AkmExists);

void admCtrl_notifyPreAuthStatus (admCtrl_t *pAdmCtrl, preAuthStatusEvent_e newStatus);

#endif /*  _ADM_H_*/

