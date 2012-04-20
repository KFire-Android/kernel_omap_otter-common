/*
 * rsn.c
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


/** \file rsn.c
 *  \brief 802.11 rsniation SM source
 *
 *  \see rsnSM.h
 */


/***************************************************************************/
/*                                                                         */
/*      MODULE: rsnSM.c                                                    */
/*    PURPOSE:  802.11 rsniation SM source                                 */
/*                                                                         */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_40
#include "osApi.h"
#include "paramOut.h"
#include "timer.h"
#include "report.h"
#include "DataCtrl_Api.h"
#include "siteMgrApi.h"
#include "smeApi.h"
#include "mainSecSm.h"
#include "admCtrl.h"
#include "rsnApi.h"
#include "rsn.h"
#include "keyParser.h"
#include "EvHandler.h"
#include "TI_IPC_Api.h"
#include "sme.h"
#include "apConn.h"
#include "802_11Defs.h"
#include "externalSec.h"
#include "connApi.h"
#ifdef XCC_MODULE_INCLUDED
#include "admCtrlWpa.h"
#include "XCCMngr.h"
#include "admCtrlXCC.h"
#endif
#include "TWDriver.h"
#include "DrvMainModules.h"
#include "PowerMgr_API.h"

/* Constants */

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Global variables */

/* Local function prototypes */
TI_STATUS rsn_sendKeysNotSet(rsn_t *pRsn);
void rsn_pairwiseReKeyTimeout(TI_HANDLE hRsn, TI_BOOL bTwdInitOccured);
void rsn_groupReKeyTimeout(TI_HANDLE hRsn, TI_BOOL bTwdInitOccured);
void rsn_micFailureReportTimeout (TI_HANDLE hRsn, TI_BOOL bTwdInitOccured);
static rsn_siteBanEntry_t * findEntryForInsert(TI_HANDLE hRsn);
static rsn_siteBanEntry_t * findBannedSiteAndCleanup(TI_HANDLE hRsn, TMacAddr siteBssid);

/* functions */

/**
*
* rsn_Create - allocate memory for rsniation SM
*
* \b Description: 
*
* Allocate memory for rsniation SM. \n
*       Allocates memory for Rsniation context. \n
*       Allocates memory for rsniation timer. \n
*       Allocates memory for rsniation SM matrix. \n
*
* \b ARGS:
*
*  I   - hOs - OS context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_mainSecKeysOnlyStop()
*/
TI_HANDLE rsn_create(TI_HANDLE hOs)
{
    rsn_t  *pRsn;

    /* allocate rsniation context memory */
    pRsn = (rsn_t*)os_memoryAlloc (hOs, sizeof(rsn_t));
    if (pRsn == NULL)
    {
        return NULL;
    }

    os_memoryZero (hOs, pRsn, sizeof(rsn_t));
    
    /* create admission control */
    pRsn->pAdmCtrl = admCtrl_create (hOs);
    if (pRsn->pAdmCtrl == NULL)
    {
        os_memoryFree (hOs, pRsn, sizeof(rsn_t));
        return NULL;
    }

    /* create main security SM */
    pRsn->pMainSecSm = mainSec_create (hOs);
    if (pRsn->pMainSecSm == NULL)
    {
        admCtrl_unload (pRsn->pAdmCtrl);
        os_memoryFree (hOs, pRsn, sizeof(rsn_t));
        return NULL;
    }

    pRsn->pKeyParser = pRsn->pMainSecSm->pKeyParser;
    
    pRsn->hOs = hOs;
    
    return pRsn;
}


/**
*
* rsn_Unload - unload rsniation SM from memory
*
* \b Description: 
*
* Unload rsniation SM from memory
*
* \b ARGS:
*
*  I   - hRsn - rsniation SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_mainSecKeysOnlyStop() 
*/
TI_STATUS rsn_unload (TI_HANDLE hRsn)
{
    rsn_t           *pRsn;
    TI_STATUS       status;

    if (hRsn == NULL)
    {
        return TI_NOK;
    }

    pRsn = (rsn_t*)hRsn;

	if (pRsn->hMicFailureReportWaitTimer)
	{
		tmr_DestroyTimer (pRsn->hMicFailureReportWaitTimer);
	}
	if (pRsn->hMicFailureGroupReKeyTimer)
	{
		tmr_DestroyTimer (pRsn->hMicFailureGroupReKeyTimer);
	}
	if (pRsn->hMicFailurePairwiseReKeyTimer)
	{
		tmr_DestroyTimer (pRsn->hMicFailurePairwiseReKeyTimer);
	}

    status = admCtrl_unload (pRsn->pAdmCtrl);
    status = mainSec_unload (pRsn->pMainSecSm);
    
    os_memoryFree (pRsn->hOs, hRsn, sizeof(rsn_t));

    return status;
}


/**
*
* rsn_init - Init module
*
* \b Description: 
*
* Init module handles and variables.
*
* \b RETURNS:
*
*  void
*
* \sa rsn_Create, rsn_Unload
*/
void rsn_init (TStadHandlesList *pStadHandles)
{
    rsn_t *pRsn = (rsn_t*)(pStadHandles->hRsn);

    pRsn->eGroupKeyUpdate = GROUP_KEY_UPDATE_FALSE;
    pRsn->ePairwiseKeyUpdate = PAIRWISE_KEY_UPDATE_FALSE;
    pRsn->PrivacyOptionImplemented = TI_TRUE;
    
	pRsn->hTxCtrl   = pStadHandles->hTxCtrl;
    pRsn->hRx       = pStadHandles->hRxData;
    pRsn->hConn     = pStadHandles->hConn;
    pRsn->hTWD      = pStadHandles->hTWD;
    pRsn->hCtrlData = pStadHandles->hCtrlData;
    pRsn->hSiteMgr  = pStadHandles->hSiteMgr;
    pRsn->hReport   = pStadHandles->hReport;
    pRsn->hOs       = pStadHandles->hOs;
    pRsn->hXCCMngr  = pStadHandles->hXCCMngr;
    pRsn->hEvHandler= pStadHandles->hEvHandler;
    pRsn->hSmeSm    = pStadHandles->hSme;
    pRsn->hAPConn   = pStadHandles->hAPConnection;
    pRsn->hMlme     = pStadHandles->hMlmeSm;
    pRsn->hPowerMgr = pStadHandles->hPowerMgr;
    pRsn->hTimer    = pStadHandles->hTimer;
    pRsn->hCurrBss  = pStadHandles->hCurrBss;

    pRsn->setPaeConfig = rsn_setPaeConfig;
    pRsn->getNetworkMode = rsn_getNetworkMode;
    pRsn->setKey = rsn_setKey;
    pRsn->removeKey = rsn_removeKey;
    pRsn->reportStatus = rsn_reportStatus;
    pRsn->setDefaultKeyId = rsn_setDefaultKeyId;
	pRsn->getPortStatus = rsn_getPortStatus;
	pRsn->setPortStatus = rsn_setPortStatus;

    pRsn->defaultKeysOn = TI_TRUE;
    pRsn->eapType = OS_EAP_TYPE_NONE;
    pRsn->numOfBannedSites = 0;
	pRsn->genericIE.length = 0;
}


TI_STATUS rsn_SetDefaults (TI_HANDLE hRsn, TRsnInitParams *pInitParam)
{
    rsn_t       *pRsn = (rsn_t*)hRsn;
    TI_UINT8     keyIndex;
    TI_STATUS    status;

    /* Create the module's timers */
    pRsn->hMicFailureReportWaitTimer = tmr_CreateTimer (pRsn->hTimer);
    if (pRsn->hMicFailureReportWaitTimer == NULL)
    {
        TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, "rsn_SetDefaults(): Failed to create hMicFailureReportWaitTimer!\n");
		return TI_NOK;
    }
    pRsn->hMicFailureGroupReKeyTimer = tmr_CreateTimer (pRsn->hTimer);
    if (pRsn->hMicFailureGroupReKeyTimer == NULL)
    {
        TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, "rsn_SetDefaults(): Failed to create hMicFailureGroupReKeyTimer!\n");
		return TI_NOK;
    }

    /* Configure the RSN external mode (by default we're in internal mode) */
    pRsn->bRsnExternalMode = 0;


    pRsn->hMicFailurePairwiseReKeyTimer = tmr_CreateTimer (pRsn->hTimer);
    if (pRsn->hMicFailurePairwiseReKeyTimer == NULL)
    {
        TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, "rsn_SetDefaults(): Failed to create hMicFailurePairwiseReKeyTimer!\n");
		return TI_NOK;
    }

	pRsn->bPairwiseMicFailureFilter = pInitParam->bPairwiseMicFailureFilter;
    /* config the admission control with the authentication suite selected.
       Admission control will configure the main security SM. */
    status = admCtrl_config (pRsn->pAdmCtrl, 
                             pRsn->hMlme, 
                             pRsn->hRx, 
                             pRsn->hReport, 
                             pRsn->hOs, 
                             pRsn, 
                             pRsn->hXCCMngr, 
                             pRsn->hPowerMgr, 
                             pRsn->hEvHandler, 
                             pRsn->hTimer,
                             pRsn->hCurrBss,
                             pInitParam);

    if (status != TI_OK)
    {
        return status;
    }            

    /* Configure keys from registry */
    if (pInitParam->privacyOn)
    {
        pRsn->wepStaticKey = TI_TRUE;
    }

    pRsn->defaultKeyId = pInitParam->defaultKeyId;
    for (keyIndex = 0; keyIndex < MAX_KEYS_NUM; keyIndex++)
    {
        os_memoryCopy (pRsn->hOs, &pRsn->keys[keyIndex], &pInitParam->keys[keyIndex], sizeof(TSecurityKeys));
        if (pRsn->keys[keyIndex].keyType != KEY_NULL)
        {
            pRsn->wepDefaultKeys[keyIndex] = TI_TRUE;
        }
        pRsn->keys_en [keyIndex] = TI_FALSE;
    }

    return status;
}


/**
*
* rsn_reconfig - re-configure a rsniation
*
* \b Description: 
*
* Re-configure rsniation 
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Create, rsn_Unload
*/
TI_STATUS rsn_reconfig (TI_HANDLE hRsn)
{
    rsn_t  *pRsn = (rsn_t *)hRsn;
    TI_UINT8   keyIndex;

    /* Mark all keys as removed */
    for (keyIndex = 0; keyIndex < MAX_KEYS_NUM; keyIndex++)
        pRsn->keys_en [keyIndex] = TI_FALSE;       

    return TI_OK;
}


/** 
*
* rsn_setDefaultKeys - 
*
* \b Description: 
*
* 
*
* \b ARGS:
*
*  I   - hRsn - Rsn SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Stop, rsn_Recv
*/
TI_STATUS rsn_setDefaultKeys(rsn_t *pRsn)
{
    TI_STATUS       status = TI_OK;
    TTwdParamInfo   tTwdParam;
    TI_UINT8           keyIndex;

    for (keyIndex = 0; keyIndex < MAX_KEYS_NUM; keyIndex++)
    {
        /* Set the WEP key to the HAL */
        if (pRsn->wepDefaultKeys[keyIndex] /*pRsn->keys[keyIndex].encLen>0*/)
        {
            /* Change key type to WEP-key before setting*/
            pRsn->keys[keyIndex].keyType = KEY_WEP;

            status = pRsn->pMainSecSm->setKey (pRsn->pMainSecSm, &pRsn->keys[keyIndex]);

            if (status != TI_OK)
            {
                TRACE1(pRsn->hReport, REPORT_SEVERITY_ERROR, "RSN: Setting key #%d failed \n", keyIndex);
                return status;
            }
        }
    }

    /* Now we configure default key ID to the HAL */
    if (pRsn->defaultKeyId < MAX_KEYS_NUM)
    {
        tTwdParam.paramType = TWD_RSN_DEFAULT_KEY_ID_PARAM_ID;
        tTwdParam.content.configureCmdCBParams.pCb = &pRsn->defaultKeyId;
        tTwdParam.content.configureCmdCBParams.fCb = NULL;
        tTwdParam.content.configureCmdCBParams.hCb = NULL;
        status = TWD_SetParam (pRsn->hTWD, &tTwdParam); 

        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: default key ID =%d \n", pRsn->defaultKeyId);
    }

    return status;
}


/** 
*
* rsn_Start - Start event for the rsniation SM
*
* \b Description: 
*
* Start event for the rsniation SM
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Stop, rsn_Recv
*/
TI_STATUS rsn_start(TI_HANDLE hRsn)
{
    TI_STATUS           status;
    rsn_t               *pRsn;
    ECipherSuite        suite;
    EExternalAuthMode   extAuthMode;
    TTwdParamInfo       tTwdParam;

    pRsn = (rsn_t*)hRsn;

    if (pRsn == NULL)
    {
        return TI_NOK;
    }

    TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_start ...\n");
    pRsn->rsnStartedTs = os_timeStampMs (pRsn->hOs);

    status = pRsn->pMainSecSm->start (pRsn->pMainSecSm);
    /* Set keys that need to be set */
    pRsn->defaultKeysOn = TI_FALSE;
    pRsn->pAdmCtrl->getCipherSuite (pRsn->pAdmCtrl, &suite);
    pRsn->pAdmCtrl->getExtAuthMode (pRsn->pAdmCtrl, &extAuthMode);

    if (pRsn->wepStaticKey && ( (suite == TWD_CIPHER_WEP) || (suite == TWD_CIPHER_CKIP) ) )
    {   /* set default WEP keys */
        status = rsn_sendKeysNotSet (pRsn);
        pRsn->eapType = OS_EAP_TYPE_NONE;
    }
    else if (suite == TWD_CIPHER_NONE && extAuthMode != RSN_EXT_AUTH_MODE_OPEN)
    {   /* remove previously WEP key for SHARED */
        pRsn->wepStaticKey = TI_FALSE;
        status = rsn_removedDefKeys (pRsn);

        /* Set None to HAL */
        tTwdParam.paramType = TWD_RSN_SECURITY_MODE_PARAM_ID;
        tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_NONE;
        status = TWD_SetParam (pRsn->hTWD, &tTwdParam);

    }
    else if (suite==TWD_CIPHER_NONE)
    {
        pRsn->eapType = OS_EAP_TYPE_NONE;
    }

    return status;
}


TI_STATUS rsn_sendKeysNotSet(rsn_t *pRsn)
{
    TI_UINT8           keyIndex;
    OS_802_11_KEY   rsnOsKey;
    TI_STATUS       status = TI_OK;
    
    for (keyIndex = 0; keyIndex < MAX_KEYS_NUM; keyIndex++)
    {
        if (pRsn->wepDefaultKeys[keyIndex])
        {
            rsnOsKey.KeyIndex  = pRsn->keys[keyIndex].keyIndex;
            rsnOsKey.KeyLength = pRsn->keys[keyIndex].encLen;
            rsnOsKey.Length    = sizeof(rsnOsKey);

            /* Change key type to WEP-key before setting*/
            pRsn->keys[keyIndex].keyType = KEY_WEP;

            MAC_COPY (rsnOsKey.BSSID, pRsn->keys[keyIndex].macAddress);
            os_memoryCopy (pRsn->hOs, &rsnOsKey.KeyRSC, 
                           (void *)pRsn->keys[keyIndex].keyRsc, 
                           KEY_RSC_LEN);
            os_memoryCopy (pRsn->hOs, rsnOsKey.KeyMaterial, 
                           (void *)pRsn->keys[keyIndex].encKey, 
                           MAX_KEY_LEN /*pRsn->keys[keyIndex].encLen*/);
           
            /* Set WEP transmit key mask on the default key */
            if (keyIndex == pRsn->defaultKeyId)
            {
                rsnOsKey.KeyIndex |= 0x80000000;
            }

            status = pRsn->pKeyParser->recv (pRsn->pKeyParser, (TI_UINT8*)&rsnOsKey, sizeof(rsnOsKey));
        }
    }

    return status;
}


TI_STATUS rsn_removedDefKeys (TI_HANDLE hRsn)
{
    TI_UINT8  keyIndex;
    rsn_t  *pRsn = (rsn_t*)hRsn;
    
    TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_removedDefKeys Enter \n");
    
    for (keyIndex = 0; keyIndex < MAX_KEYS_NUM; keyIndex++)
    {
        TSecurityKeys   key;

        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_removedDefKeys, Remove keyId=%d\n", keyIndex);
       
        pRsn->wepDefaultKeys[keyIndex] = TI_FALSE;
        os_memoryCopy (pRsn->hOs, &key, &pRsn->keys[keyIndex], sizeof(TSecurityKeys));
        pRsn->removeKey (pRsn, &key);
       
        /* Set WEP transmit key mask on the default key */
        if (keyIndex == pRsn->defaultKeyId)
        {
            pRsn->defaultKeyId = 0;
        }
    }

    return TI_OK;
}


/**
*
* rsn_Stop - Stop event for the rsniation SM
*
* \b Description: 
*
* Stop event for the rsniation SM
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Recv
*/
TI_STATUS rsn_stop (TI_HANDLE hRsn, TI_BOOL removeKeys)
{
    TI_STATUS        status;
    rsn_t           *pRsn;
    TI_UINT8            keyIndex;
    TSecurityKeys   key;

    pRsn = (rsn_t*)hRsn;

    if (pRsn == NULL)
    {
        return TI_NOK;
    }
    
    TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: calling STOP... removeKeys=%d\n", removeKeys);

    for (keyIndex = 0; keyIndex < MAX_KEYS_NUM; keyIndex++)
    {
        os_memoryCopy (pRsn->hOs, &key, &pRsn->keys[keyIndex], sizeof(TSecurityKeys));
        key.keyIndex = keyIndex;

        if (!pRsn->wepDefaultKeys[keyIndex])
        {	/* Remove only dynamic keys. Default keys are removed by calling: rsn_removedDefKeys() */
            TRACE2(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_stop, Remove keyIndex=%d, key.keyIndex=%d\n",keyIndex, key.keyIndex);
            
            TRACE_INFO_HEX(pRsn->hReport, (TI_UINT8 *)key.macAddress, 6);

            pRsn->removeKey (pRsn, &key);
        }

    }

    tmr_StopTimer (pRsn->hMicFailureReportWaitTimer);

    /* Stop the pre-authentication timer in case we are disconnecting */
    tmr_StopTimer (pRsn->pAdmCtrl->hPreAuthTimerWpa2);

    status = pRsn->pMainSecSm->stop (pRsn->pMainSecSm);

    pRsn->eGroupKeyUpdate = GROUP_KEY_UPDATE_FALSE;
    pRsn->ePairwiseKeyUpdate = PAIRWISE_KEY_UPDATE_FALSE;
    pRsn->defaultKeysOn = TI_TRUE;

#ifdef XCC_MODULE_INCLUDED
	pRsn->pAdmCtrl->networkEapMode = OS_XCC_NETWORK_EAP_OFF;
#endif

    if (removeKeys)
    {   /* reset PMKID list if exist */
        pRsn->pAdmCtrl->resetPmkidList (pRsn->pAdmCtrl);
        /* reset unicast and broadcast ciphers after disconnect */
        pRsn->pAdmCtrl->unicastSuite = TWD_CIPHER_NONE;
        pRsn->pAdmCtrl->broadcastSuite = TWD_CIPHER_NONE;
    }

    return status;
}

TI_STATUS rsn_getParamEncryptionStatus(TI_HANDLE hRsn, ECipherSuite *rsnStatus)
{ /* RSN_ENCRYPTION_STATUS_PARAM */
    rsn_t       *pRsn = (rsn_t *)hRsn;
    TI_STATUS   status = TI_NOK;

    if ( (NULL == pRsn) || (NULL == rsnStatus) )
    {
        return status;
    }
    status = pRsn->pAdmCtrl->getCipherSuite(pRsn->pAdmCtrl, rsnStatus);
    return status;
}

/**
*
* rsn_GetParam - Get a specific parameter from the rsniation SM
*
* \b Description: 
*
* Get a specific parameter from the rsniation SM.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Stop
*/
TI_STATUS rsn_getParam(TI_HANDLE hRsn, void *param)
{
    rsn_t       *pRsn = (rsn_t *)hRsn;
    paramInfo_t *pParam = (paramInfo_t *)param;
    TI_STATUS   status = TI_OK;

    if ( (NULL == pRsn) || (NULL == pParam) )
    {
        return TI_NOK;
    }

    switch (pParam->paramType)
    {
    case RSN_PRIVACY_OPTION_IMPLEMENTED_PARAM:
        pParam->content.rsnPrivacyOptionImplemented = TI_TRUE;
        break;

    case RSN_KEY_PARAM:
        pParam->content.pRsnKey = &pRsn->keys[pParam->content.pRsnKey->keyIndex];
        if (pParam->content.pRsnKey->keyIndex == pRsn->defaultKeyId)
        {
            pParam->content.pRsnKey->keyIndex |= 0x80000000;
            TRACE1(pRsn->hReport, REPORT_SEVERITY_WARNING, "default Key: %d\n", pRsn->defaultKeyId);
        }
        break;

    case RSN_SECURITY_STATE_PARAM:
        status = pRsn->pMainSecSm->getAuthState (pRsn->pMainSecSm, (TIWLN_SECURITY_STATE*)&(pParam->content.rsnAuthState));
        break;

    case RSN_ENCRYPTION_STATUS_PARAM: 
        status = pRsn->pAdmCtrl->getCipherSuite (pRsn->pAdmCtrl, &pParam->content.rsnEncryptionStatus);
        break;

    case RSN_EXT_AUTHENTICATION_MODE:
        status = pRsn->pAdmCtrl->getExtAuthMode (pRsn->pAdmCtrl, &pParam->content.rsnExtAuthneticationMode);
        break;

    case RSN_MIXED_MODE:
        status = pRsn->pAdmCtrl->getMixedMode (pRsn->pAdmCtrl, &pParam->content.rsnMixedMode);
        break;

    case RSN_AUTH_ENCR_CAPABILITY:
        status = pRsn->pAdmCtrl->getAuthEncrCap(pRsn->pAdmCtrl, pParam->content.pRsnAuthEncrCapability);
        break;

    case RSN_PMKID_LIST:
        pParam->content.rsnPMKIDList.Length = pParam->paramLength;
        status = pRsn->pAdmCtrl->getPmkidList (pRsn->pAdmCtrl, &pParam->content.rsnPMKIDList);
        pParam->paramLength = pParam->content.rsnPMKIDList.Length + 2 * sizeof(TI_UINT32);
        break;

    case RSN_PRE_AUTH_STATUS:
        {
            TI_UINT8 cacheIndex;
        
            pParam->content.rsnPreAuthStatus = pRsn->pAdmCtrl->getPreAuthStatus (pRsn->pAdmCtrl, &pParam->content.rsnApMac, &cacheIndex);
        }
        break;

    case  RSN_WPA_PROMOTE_AVAILABLE_OPTIONS:
        status = pRsn->pAdmCtrl->getWPAMixedModeSupport (pRsn->pAdmCtrl, &pParam->content.rsnWPAMixedModeSupport);
        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Get WPA Mixed MODE support  %d \n",pParam->content.rsnWPAMixedModeSupport);
        break;

    case RSN_WPA_PROMOTE_OPTIONS:
        status = pRsn->pAdmCtrl->getPromoteFlags (pRsn->pAdmCtrl, 
                                                  &pParam->content.rsnWPAPromoteFlags);
                TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Get WPA promote flags  %d \n",pParam->content.rsnWPAPromoteFlags);
        
        break;

#ifdef XCC_MODULE_INCLUDED
    case RSN_XCC_NETWORK_EAP:
        status = pRsn->pAdmCtrl->getNetworkEap (pRsn->pAdmCtrl, &pParam->content.networkEap);
        break;
#endif
    case RSN_EAP_TYPE:
        pParam->content.eapType = pRsn->eapType;
        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Get RSN_EAP_TYPE eapType  %d \n", pParam->content.eapType);
        break;

    case WPA_801_1X_AKM_EXISTS:

        status = pRsn->pAdmCtrl->get802_1x_AkmExists(pRsn->pAdmCtrl, &pParam->content.wpa_802_1x_AkmExists);
        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Get WPA_801_1X_AKM_EXISTS  %d \n", pParam->content.wpa_802_1x_AkmExists);
        break;

    case RSN_DEFAULT_KEY_ID:
        pParam->content.rsnDefaultKeyID = pRsn->defaultKeyId;
        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Get RSN_DEFAULT_KEY_ID  %d \n", pParam->content.rsnDefaultKeyID);
        break;

    case RSN_PORT_STATUS_PARAM:
		TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Get Port Status\n"  );

		if (pRsn->bRsnExternalMode) {
				pParam->content.rsnPortStatus = pRsn->getPortStatus( pRsn );
		} else {
				status = TI_NOK;
		}
        break;

    case RSN_EXTERNAL_MODE_PARAM:
		TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Get External Mode\n"  );

		pParam->content.rsnExternalMode = pRsn->bRsnExternalMode;
        break;

    default:
        return TI_NOK;
    }
    
    return status;
}


/**
*
* rsn_SetParam - Set a specific parameter to the rsniation SM
*
* \b Description: 
*
* Set a specific parameter to the rsniation SM.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Stop
*/
TI_STATUS rsn_setParam (TI_HANDLE hRsn, void *param)
{
    rsn_t               *pRsn;
    paramInfo_t         *pParam = (paramInfo_t*)param;
    TTwdParamInfo       tTwdParam;
    TI_STATUS           status = TI_OK;

    pRsn = (rsn_t*)hRsn;

    if ( (NULL == pRsn) || (NULL == pParam) )
    {
        return TI_NOK;
    }

    TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set rsn_setParam   %X \n", pParam->paramType);

    switch (pParam->paramType)
    {

    case RSN_DEFAULT_KEY_ID:
    {
        TI_UINT8  defKeyId, i;

        defKeyId = pParam->content.rsnDefaultKeyID;
        
        if(defKeyId >= MAX_KEYS_NUM)
        {
            TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, "RSN: Error - the value of the default Key Id  is incorrect \n");
            return TI_NOK;
        }

        /* Clean transmit flag (1 in the bit31) in the previous default key */
        for(i = 0; i < MAX_KEYS_NUM; i++)
        {
            pRsn->keys[i].keyIndex &= 0x7FFFFFFF;
        }

        /* Set the default key ID value in the RSN data structure */ 
        pRsn->defaultKeyId = defKeyId;

        /* Set the default key ID in the HAL */
        tTwdParam.paramType = TWD_RSN_DEFAULT_KEY_ID_PARAM_ID;
        tTwdParam.content.configureCmdCBParams.pCb = &pRsn->defaultKeyId;
        tTwdParam.content.configureCmdCBParams.fCb = NULL;
        tTwdParam.content.configureCmdCBParams.hCb = NULL;
        status = TWD_SetParam (pRsn->hTWD, &tTwdParam); 

        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: default key ID =%d \n", pRsn->defaultKeyId);

        sme_Restart (pRsn->hSmeSm);
        break;
    }

    case RSN_ADD_KEY_PARAM:
    {
        TI_UINT8           keyIndex, i = 0;
        ECipherSuite   cipherSuite;

        status = pRsn->pAdmCtrl->getCipherSuite (pRsn->pAdmCtrl, &cipherSuite);
        if (status !=TI_OK)
        {
            return status;
        }
        
        TRACE2(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_ADD_KEY_PARAM KeyIndex  %x , keyLength=%d\n", pParam->content.rsnOsKey.KeyIndex,pParam->content.rsnOsKey.KeyLength);
        keyIndex = (TI_UINT8)pParam->content.rsnOsKey.KeyIndex;
        if (keyIndex >= MAX_KEYS_NUM)
        {
			return TI_NOK;
        }
       
		status = pRsn->pKeyParser->recv (pRsn->pKeyParser, (TI_UINT8*)&pParam->content.rsnOsKey, sizeof(pParam->content.rsnOsKey));

        if (status != TI_OK)
        {
TRACE1(pRsn->hReport, REPORT_SEVERITY_WARNING, ": pRsn->pKeyParser->recv satus returned with status=%x. returning with NOK\n", status);
            return TI_NOK;
        }

        /* If the Key is not BAD, it may be that WEP key is sent before WEP status is set, 
            save the key, and set it later at rsn_start */

		/* If default Key not cleaned by calling rsn_removedDefKeys for keyIndex, Clean it */
		if (pRsn->wepDefaultKeys[keyIndex] == TI_TRUE) 
		{
			TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "Set RSN_ADD_KEY_PARAM KeyIndex  %x\n", keyIndex);
			TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "Set RSN_ADD_KEY_PARAM wepDefaultKeys=%d\n", pRsn->wepDefaultKeys[keyIndex]);
             
			pRsn->wepDefaultKeys[keyIndex] = TI_FALSE;
			
		}

		pRsn->keys[keyIndex].keyIndex = pParam->content.rsnOsKey.KeyIndex;
		pRsn->keys[keyIndex].encLen = pParam->content.rsnOsKey.KeyLength;
		MAC_COPY (pRsn->keys[keyIndex].macAddress, pParam->content.rsnOsKey.BSSID);
		os_memoryCopy (pRsn->hOs, (void *)pRsn->keys[keyIndex].keyRsc, (TI_UINT8*)&(pParam->content.rsnOsKey.KeyRSC), KEY_RSC_LEN);
		os_memoryCopy (pRsn->hOs, (void *)pRsn->keys[keyIndex].encKey, pParam->content.rsnOsKey.KeyMaterial, MAX_KEY_LEN);
           
        /* Process the transmit flag (31-st bit of keyIndex).        */
        /* If the added key has the TX bit set to TI_TRUE (i.e. the key */
        /* is the new transmit key (default key), update             */
        /* RSN data def.key Id and clean this bit in all other keys  */
        if (pParam->content.rsnOsKey.KeyIndex & 0x80000000)
        {
            pRsn->defaultKeyId = keyIndex;
            
            for (i = 0; i < MAX_KEYS_NUM; i ++)
            {
                if (i != keyIndex)
                {
                    pRsn->keys[i].keyIndex &= 0x7FFFFFFF;
                }
            }
        }
        
        if (pRsn->defaultKeysOn)
        {   /* This is a WEP default key */
            TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN_ADD_KEY_PARAM, Default key configured - keyIndex=%d-TI_TRUE\n", keyIndex);

            pRsn->wepDefaultKeys[keyIndex] = TI_TRUE;
            pRsn->wepStaticKey = TI_TRUE;
            status = TI_OK;
        }
        break;
    }
    case RSN_REMOVE_KEY_PARAM:
    {
        TI_UINT8           keyIndex;
        ECipherSuite   cipherSuite;

        status = pRsn->pAdmCtrl->getCipherSuite (pRsn->pAdmCtrl, &cipherSuite);
        if (status !=TI_OK)
        {
            return status;
        }
        /*if (cipherSuite == RSN_CIPHER_NONE)
        {
            TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, "RSN: Error Remove Wep/Key when no encryption \n");
            return TI_NOK;
        }*/

        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_REMOVE_KEY_PARAM KeyIndex  %x \n", pParam->content.rsnOsKey.KeyIndex);
        keyIndex = (TI_UINT8)pParam->content.rsnOsKey.KeyIndex;
        if (keyIndex >= MAX_KEYS_NUM)
        {
            return TI_NOK;
        }
        
        status = pRsn->pKeyParser->remove (pRsn->pKeyParser, 
                                           (TI_UINT8*)&pParam->content.rsnOsKey, 
                                           sizeof(pParam->content.rsnOsKey));

        if (status == TI_OK)
        {
            pRsn->keys[keyIndex].keyType = KEY_NULL;
            pRsn->keys[keyIndex].keyIndex &= 0x000000FF;
        }

        break;
    }
    
    case RSN_ENCRYPTION_STATUS_PARAM: 
        {
            ECipherSuite   cipherSuite;

            TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_ENCRYPTION_STATUS_PARAM rsnEncryptionStatus  %d \n", pParam->content.rsnEncryptionStatus);

            pRsn->pAdmCtrl->getCipherSuite (pRsn->pAdmCtrl, &cipherSuite);
            if (cipherSuite != pParam->content.rsnEncryptionStatus)
            {
                status = pRsn->pAdmCtrl->setUcastSuite (pRsn->pAdmCtrl, pParam->content.rsnEncryptionStatus);
                status = pRsn->pAdmCtrl->setBcastSuite (pRsn->pAdmCtrl, pParam->content.rsnEncryptionStatus);
                TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, " status = %d \n", status);
            }
            pRsn->defaultKeysOn = TI_TRUE;
        }
        break;

    case RSN_EXT_AUTHENTICATION_MODE:
        {
            EExternalAuthMode  extAuthMode;

            pRsn->pAdmCtrl->getExtAuthMode (pRsn->pAdmCtrl, &extAuthMode);
            if (pParam->content.rsnExtAuthneticationMode!=extAuthMode)
            {
                TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_EXT_AUTHENTICATION_MODE rsnExtAuthneticationMode  %d \n", pParam->content.rsnExtAuthneticationMode);
                
                /*
TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: remove all Keys\n");

                for (keyIndex=0; keyIndex<MAX_KEYS_NUM; keyIndex++)
                {
                    os_memoryCopy(pRsn->hOs, &key, &pRsn->keys[keyIndex], sizeof(TSecurityKeys));
                    pRsn->removeKey(pRsn, &key);

                }*/

                status = pRsn->pAdmCtrl->setExtAuthMode (pRsn->pAdmCtrl, pParam->content.rsnExtAuthneticationMode);
            }
            pRsn->defaultKeysOn = TI_TRUE;
        }
        break;

#ifdef XCC_MODULE_INCLUDED
    case RSN_XCC_NETWORK_EAP:
        {
            OS_XCC_NETWORK_EAP      networkEap;

            pRsn->pAdmCtrl->getNetworkEap (pRsn->pAdmCtrl, &networkEap);
            if (networkEap != pParam->content.networkEap)
            {
                TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_XCC_NETWORK_EAP networkEap  %d \n", pParam->content.networkEap);
                
                status = pRsn->pAdmCtrl->setNetworkEap (pRsn->pAdmCtrl, pParam->content.networkEap);
                if (status == TI_OK) 
                {
                    /*status = RE_SCAN_NEEDED;*/
                }
            }
        }
        break;
#endif
    case RSN_MIXED_MODE:
        {
            TI_BOOL mixedMode;
        
            pRsn->pAdmCtrl->getMixedMode (pRsn->pAdmCtrl, &mixedMode);
            if (mixedMode!=pParam->content.rsnMixedMode)
            {
                status = pRsn->pAdmCtrl->setMixedMode (pRsn->pAdmCtrl, pParam->content.rsnMixedMode);
                
                TRACE2(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_MIXED_MODE mixedMode  %d, status=%d \n", pParam->content.rsnMixedMode, status);
            }
            break;
        }

    case RSN_PMKID_LIST:
        TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_PMKID_LIST \n");

        TRACE_INFO_HEX(pRsn->hReport, (TI_UINT8*)&pParam->content.rsnPMKIDList ,sizeof(OS_802_11_PMKID));
         status = pRsn->pAdmCtrl->setPmkidList (pRsn->pAdmCtrl, 
                                                &pParam->content.rsnPMKIDList);
         if(status == TI_OK)
         {
            TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_PMKID_LIST:   %d PMKID entries has been added to the cache.\n", pParam->content.rsnPMKIDList.BSSIDInfoCount);
         }
         else
         {
            TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_PMKID_LIST failure");
         }
        break;

    case RSN_WPA_PROMOTE_OPTIONS:
         status = pRsn->pAdmCtrl->setPromoteFlags (pRsn->pAdmCtrl, 
                                                   pParam->content.rsnWPAPromoteFlags);
         if(status == TI_OK)
         {
            TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set WPA promote options:  %d \n", pParam->content.rsnWPAPromoteFlags);
         }
         else
         {
            TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set WPA promote options failure");
         }
        break;

    case RSN_EAP_TYPE:
        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set RSN_EAP_TYPE eapType  %d \n", pParam->content.eapType);

        pRsn->eapType = pParam->content.eapType;
	pRsn->defaultKeysOn = TI_TRUE;
        break;

    case RSN_SET_KEY_PARAM:
        {
            TSecurityKeys *pSecurityKey = pParam->content.pRsnKey;
            TI_UINT32     keyIndex;
            TI_UINT8      j=0;

            TRACE2(pRsn->hReport,REPORT_SEVERITY_INFORMATION,"RSN:Set RSN_SET_KEY_PARAM KeyIndex %x,keyLength=%d\n",pSecurityKey->keyIndex,pSecurityKey->encLen);

            if(pSecurityKey->keyIndex >= MAX_KEYS_NUM)
            {
                return TI_NOK;
            }

           keyIndex = (TI_UINT8)pSecurityKey->keyIndex;
		/* Remove the key when the length is 0, or the type is not set */
		   if ( (pSecurityKey->keyType == KEY_NULL) ||
				 (pSecurityKey->encLen == 0))
		   {
				/* Clearing a key */
				status = rsn_removeKey( pRsn, pSecurityKey );
				break;
           }
		   else
		   {
           status = rsn_setKey (pRsn, pSecurityKey);  /* send key to FW*/

           if (status == TI_OK)
           {
               //os_memoryCopy(pKeyDerive->hOs,&pRsn->pKeyParser->pUcastKey/pBcastKey, pEncodedKey, sizeof(encodedKeyMaterial_t));	
           } /* check this copy */


           /* If the Key is not BAD, it may be that WEP key is sent before WEP status is set,
           save the key, and set it later at rsn_start */

           pRsn->keys[keyIndex].keyIndex = pSecurityKey->keyIndex;
           pRsn->keys[keyIndex].encLen = pSecurityKey->encLen;
           MAC_COPY (pRsn->keys[keyIndex].macAddress, pSecurityKey->macAddress);
           os_memoryCopy(pRsn->hOs,(void*)pRsn->keys[keyIndex].keyRsc, (TI_UINT8*)&(pSecurityKey->keyRsc), KEY_RSC_LEN);
           os_memoryCopy (pRsn->hOs, (void *)pRsn->keys[keyIndex].encKey, (void*)pSecurityKey->encKey, MAX_KEY_LEN);

           /* Process the transmit flag (31-st bit of keyIndex).        */
           /* If the added key has the TX bit set to TI_TRUE (i.e. the key */
           /* is the new transmit key (default key), update             */
           /* RSN data def.key Id and clean this bit in all other keys  */
           if (pSecurityKey->keyIndex & 0x80000000)
           {
               pRsn->defaultKeyId = keyIndex;

               for (j = 0; j < MAX_KEYS_NUM; j++)
               {
                   if (j != keyIndex)
                   {
                       pRsn->keys[j].keyIndex &= 0x7FFFFFFF;
                   }
               }
           }

           if (pRsn->defaultKeysOn)
           {   /* This is a WEP default key */
               TRACE1(pRsn->hReport,REPORT_SEVERITY_INFORMATION, "RSN_SET_KEY_PARAM, Default key configured-keyIndex=%d-TI_TRUE\n", keyIndex);

               pRsn->wepDefaultKeys[keyIndex] = TI_TRUE;
               pRsn->wepStaticKey = TI_TRUE;
               status = TI_OK;
           }
           break;
        }
       }
       break;


    case RSN_PORT_STATUS_PARAM:
            TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set Port Status %d \n", pParam->content.rsnPortStatus);

            if (pRsn->bRsnExternalMode) {
                    status = pRsn->setPortStatus( hRsn, pParam->content.rsnPortStatus );
            } else {
                    status = TI_NOK;
            }
            break;

    case RSN_GENERIC_IE_PARAM:
            TRACE4(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set Generic IE: length=%d, IE=%02x%02x%02x... \n",
                   pParam->content.rsnGenericIE.length,
               pParam->content.rsnGenericIE.data[0], pParam->content.rsnGenericIE.data[1],pParam->content.rsnGenericIE.data[2] );

            status = TI_OK;

            /* make sure it's a valid IE: datal-ength > 2 AND a matching length field */
            if ((pParam->content.rsnGenericIE.length > 2) &&
                ((pParam->content.rsnGenericIE.data[1] + 2) == pParam->content.rsnGenericIE.length)) {
                    /* Setting the IE */
                    pRsn->genericIE.length = pParam->content.rsnGenericIE.length;
                    os_memoryCopy(pRsn->hOs,(void*)pRsn->genericIE.data, (TI_UINT8*)pParam->content.rsnGenericIE.data, pParam->content.rsnGenericIE.length);
            } else if ( pParam->content.rsnGenericIE.length == 0 ) {
                    /* Deleting the IE */
                    pRsn->genericIE.length = pParam->content.rsnGenericIE.length;
            } else {
                    TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, "RSN: Set Generic IE: FAILED sanity checks \n" );
                    status = TI_NOK;
            }
	    break;

    case RSN_EXTERNAL_MODE_PARAM:
		TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set External Mode\n"  );

		pRsn->bRsnExternalMode = pParam->content.rsnExternalMode;
            break;

    default:
        return TI_NOK;
    }

    return status;
}


/**
*
* rsn_eventRecv - Set a specific parameter to the rsniation SM
*
* \b Description: 
*
* Set a specific parameter to the rsniation SM.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Stop
*/
TI_STATUS rsn_reportStatus (rsn_t *pRsn, TI_STATUS rsnStatus)
{
    TI_STATUS           status = TI_OK;
    paramInfo_t         param;
    EExternalAuthMode   extAuthMode;

    if (pRsn == NULL)
    {
        return TI_NOK;
    }
    
    if (rsnStatus == TI_OK)
    {
        /* set EAPOL encryption status according to authentication protocol */
        pRsn->rsnCompletedTs = os_timeStampMs (pRsn->hOs);
        
        status = pRsn->pAdmCtrl->getExtAuthMode (pRsn->pAdmCtrl, &extAuthMode);
        if (status != TI_OK)
        {
            return status;
        }

        if (extAuthMode >= RSN_EXT_AUTH_MODE_WPA)
			txCtrlParams_setEapolEncryptionStatus (pRsn->hTxCtrl, TI_TRUE);
		else
			txCtrlParams_setEapolEncryptionStatus (pRsn->hTxCtrl, TI_FALSE);
		
        /* set WEP invoked mode according to cipher suite */
        switch (pRsn->paeConfig.unicastSuite)
        {
        case TWD_CIPHER_NONE:
            param.content.txDataCurrentPrivacyInvokedMode = TI_FALSE;
            break;
        
        default:
            param.content.txDataCurrentPrivacyInvokedMode = TI_TRUE;
            break;
        }

		if (pRsn->bRsnExternalMode) {
				param.content.txDataCurrentPrivacyInvokedMode = TI_TRUE;
                txCtrlParams_setEapolEncryptionStatus (pRsn->hTxCtrl, TI_FALSE);
		}

		txCtrlParams_setCurrentPrivacyInvokedMode(pRsn->hTxCtrl, param.content.txDataCurrentPrivacyInvokedMode);
        /* The value of exclude unencrypted should be as privacy invoked */
        param.paramType = RX_DATA_EXCLUDE_UNENCRYPTED_PARAM;
        rxData_setParam (pRsn->hRx, &param);
        
        param.paramType = RX_DATA_EXCLUDE_BROADCAST_UNENCRYPTED_PARAM;
        if (pRsn->pAdmCtrl->mixedMode)
        {   /* do not exclude Broadcast packets */
            param.content.txDataCurrentPrivacyInvokedMode = TI_FALSE;
        }
        rxData_setParam (pRsn->hRx, &param);
    } 

    else 
        rsnStatus = (TI_STATUS)STATUS_SECURITY_FAILURE;

    status = conn_reportRsnStatus (pRsn->hConn, (mgmtStatus_e)rsnStatus);

    if (status!=TI_OK)
    {
        return status;
    }
    
    if (rsnStatus == TI_OK)
    {
        EvHandlerSendEvent (pRsn->hEvHandler, IPC_EVENT_AUTH_SUCC, NULL, 0);
    }

    TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: rsn_reportStatus \n");

    return TI_OK;
}


/**
*
* rsn_eventRecv - Set a specific parameter to the rsniation SM
*
* \b Description: 
*
* Set a specific parameter to the rsniation SM.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Stop
*/
TI_STATUS rsn_setPaeConfig(rsn_t *pRsn, TRsnPaeConfig *pPaeConfig)
{
    TI_STATUS           status;
    mainSecInitData_t   initData;

    if ( (NULL == pRsn) || (NULL == pPaeConfig) )
    {
        return TI_NOK;
    }

    TRACE2(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Calling set PAE config..., unicastSuite = %d, broadcastSuite = %d \n", pPaeConfig->unicastSuite, pPaeConfig->broadcastSuite);
    
    os_memoryCopy(pRsn->hOs, &pRsn->paeConfig, pPaeConfig, sizeof(TRsnPaeConfig));

    initData.pPaeConfig = &pRsn->paeConfig;

    status = mainSec_config (pRsn->pMainSecSm, 
                             &initData, 
                             pRsn, 
                             pRsn->hReport, 
                             pRsn->hOs, 
                             pRsn->hCtrlData,
                             pRsn->hEvHandler, 
                             pRsn->hConn,
                             pRsn->hTimer);

    return status;
}


/**
*
* rsn_eventRecv - Set a specific parameter to the rsniation SM
*
* \b Description: 
*
* Set a specific parameter to the rsniation SM.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Stop
*/
TI_STATUS rsn_getNetworkMode(rsn_t *pRsn, ERsnNetworkMode *pNetMode)
{
    paramInfo_t     param;
    TI_STATUS       status;

    param.paramType = CTRL_DATA_CURRENT_BSS_TYPE_PARAM;

    status =  ctrlData_getParam (pRsn->hCtrlData, &param);

    if (status == TI_OK)
    {
        if (param.content.ctrlDataCurrentBssType == BSS_INFRASTRUCTURE)
        {
            *pNetMode = RSN_INFRASTRUCTURE;
        } 
        else 
        {
            *pNetMode = RSN_IBSS;
        }
    }
    else 
    {
        return TI_NOK;
    }

    return TI_OK;
}


/**
*
* rsn_eventRecv - Set a specific parameter to the rsniation SM
*
* \b Description: 
*
* Set a specific parameter to the rsniation SM.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Stop
*/
TI_STATUS rsn_evalSite(TI_HANDLE hRsn, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pMetric)
{

    rsn_t       *pRsn;
    TI_STATUS    status;

    if ( (NULL == pRsnData) || (NULL == hRsn) )
    {
        *pMetric = 0;
        return TI_NOK;
    }

    pRsn = (rsn_t*)hRsn;

    if (rsn_isSiteBanned(hRsn, pRsnSiteParams->bssid) == TI_TRUE)
    {
        *pMetric = 0;
        TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Site is banned!\n");
        return TI_NOK;
    }

	if ( pRsn->bRsnExternalMode ) {
			/* In external mode, the supplicant is responsible to make sure that site security matches */
			status = TI_OK;
	} else {
    status = pRsn->pAdmCtrl->evalSite (pRsn->pAdmCtrl, pRsnData, pRsnSiteParams, pMetric);
	}

    TRACE2(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": pMetric=%d status=%d\n", *pMetric, status);

    return status;
}


/**
*
* rsn_getInfoElement - 
*
* \b Description: 
*
* Get the RSN information element.
*
* \b ARGS:
*
*  I   - hRsn - Rsn SM context  \n
*  I/O - pRsnIe - Pointer to the return information element \n
*  I/O - pRsnIeLen - Pointer to the returned IE's length \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS rsn_getInfoElement(TI_HANDLE hRsn, TI_UINT8 *pRsnIe, TI_UINT32 *pRsnIeLen)
{
    rsn_t       *pRsn;
    TI_STATUS   status;
    TI_UINT32   ie_len = 0;

    if ( (NULL == hRsn) || (NULL == pRsnIe) || (NULL == pRsnIeLen) )
    {
        return TI_NOK;
    }

    pRsn = (rsn_t*)hRsn;

    if (!pRsn->bRsnExternalMode) 
		{

    status = pRsn->pAdmCtrl->getInfoElement (pRsn->pAdmCtrl, pRsnIe, &ie_len);

    TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_getInfoElement pRsnIeLen= %d\n",*pRsnIeLen);

    if ( status != TI_OK ) {
    return status;   
    }

    pRsnIe += ie_len;
		}

    status = rsn_getGenInfoElement(hRsn, pRsnIe, pRsnIeLen);

    *pRsnIeLen += ie_len;

    return status;

}


#ifdef XCC_MODULE_INCLUDED
/**
*
* rsn_getXCCExtendedInfoElement - 
*
* \b Description: 
*
* Get the Aironet information element.
*
* \b ARGS:
*
*  I   - hRsn - Rsn SM context  \n
*  I/O - pRsnIe - Pointer to the return information element \n
*  I/O - pRsnIeLen - Pointer to the returned IE's length \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS rsn_getXCCExtendedInfoElement(TI_HANDLE hRsn, TI_UINT8 *pRsnIe, TI_UINT8 *pRsnIeLen)
{
    rsn_t       *pRsn;
    TI_STATUS   status;

    if ( (NULL == hRsn) || (NULL == pRsnIe) || (NULL == pRsnIeLen) )
    {
        return TI_NOK;
    }

    pRsn = (rsn_t*)hRsn;

    status = admCtrlXCC_getInfoElement (pRsn->pAdmCtrl, pRsnIe, pRsnIeLen);

    TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_getXCCExtendedInfoElement pRsnIeLen= %d\n",*pRsnIeLen);

    return status;    
}
#endif


/**
*
* rsn_eventRecv - Set a specific parameter to the rsniation SM
*
* \b Description: 
*
* Set a specific parameter to the rsniation SM.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pParam - Parameter \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa rsn_Start, rsn_Stop
*/
TI_STATUS rsn_setSite(TI_HANDLE hRsn, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen)
{
    rsn_t      *pRsn;
    TI_STATUS   status;

    if ( (NULL == pRsnData) || (NULL == hRsn) )
    {
        *pAssocIeLen = 0;
        return TI_NOK;
    }

    pRsn = (rsn_t*)hRsn;

    status = pRsn->pAdmCtrl->setSite (pRsn->pAdmCtrl, pRsnData, pAssocIe, pAssocIeLen);

    TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_setSite ieLen= %d\n",pRsnData->ieLen);
    return status;
}


TI_STATUS rsn_setKey (rsn_t *pRsn, TSecurityKeys *pKey)
{
    TTwdParamInfo       tTwdParam;
    TI_UINT8            keyIndex;
    TI_BOOL				macIsBroadcast = TI_FALSE;
    TI_STATUS           status = TI_OK;

	if (pRsn == NULL || pKey == NULL)
	{
		return TI_NOK;
	}

	keyIndex = (TI_UINT8)pKey->keyIndex;
    if (keyIndex >= MAX_KEYS_NUM)
    {
        return TI_NOK;
    }

    if (pKey->keyType != KEY_NULL)
    {

			/* If in external mode, set driver's security mode according to the key */
			if (pRsn->bRsnExternalMode) {
					tTwdParam.paramType = TWD_RSN_SECURITY_MODE_PARAM_ID;

					switch (pKey->keyType) {
					case KEY_TKIP:
							tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_TKIP;
							break;
					case KEY_AES:
							tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_AES_CCMP;
							break;
#ifdef GEM_SUPPORTED
					case KEY_GEM:
							tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_GEM;
							status = pRsn->pAdmCtrl->setUcastSuite (pRsn->pAdmCtrl, TWD_CIPHER_GEM);
							status = pRsn->pAdmCtrl->setBcastSuite (pRsn->pAdmCtrl, TWD_CIPHER_GEM);
							break;
#endif
					case KEY_WEP:
							tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_WEP;
							break;
					case KEY_NULL:
					case KEY_XCC:
					default:
							tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_NONE;
							break;
					}
					status = TWD_SetParam(pRsn->hTWD, &tTwdParam);
					if ( status != TI_OK ) {
							return status;
					}
			}

        /* set the size to reserve for encryption to the tx */
        /* update this parameter only in accordance with pairwise key setting */
       if (!MAC_BROADCAST(pKey->macAddress))
        {

        /* set the size to reserve for encryption to the tx */
            switch (pKey->keyType)
            {
                case KEY_TKIP:
			        txCtrlParams_setEncryptionFieldSizes (pRsn->hTxCtrl, IV_FIELD_SIZE);
                    break;
                case KEY_AES:
			        txCtrlParams_setEncryptionFieldSizes (pRsn->hTxCtrl, AES_AFTER_HEADER_FIELD_SIZE);
                    break;
#ifdef GEM_SUPPORTED
			    case KEY_GEM:
#endif
                case KEY_WEP:
                case KEY_NULL:
                case KEY_XCC:
                default:
			        txCtrlParams_setEncryptionFieldSizes (pRsn->hTxCtrl, 0);
                    break;
            }

        }

        pRsn->keys[keyIndex].keyType = pKey->keyType;
        pRsn->keys[keyIndex].keyIndex = keyIndex;

		if (!pRsn->bRsnExternalMode) {
        
        macIsBroadcast = MAC_BROADCAST (pKey->macAddress);
		if ((pRsn->keys[keyIndex].keyType != KEY_NULL )&&
			macIsBroadcast && !MAC_BROADCAST((pRsn->keys[keyIndex].macAddress)))
		{	/* In case a new Group key is set instead of a Unicast key, 
			first remove the UNIcast key from FW */
			rsn_removeKey(pRsn, &pRsn->keys[keyIndex]);
		}

		if (macIsBroadcast)
        {
            TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: rsn_setKey, Group ReKey timer started\n");
            tmr_StopTimer (pRsn->hMicFailureGroupReKeyTimer);
            tmr_StartTimer (pRsn->hMicFailureGroupReKeyTimer,
                            rsn_groupReKeyTimeout,
                            (TI_HANDLE)pRsn,
                            RSN_MIC_FAILURE_RE_KEY_TIMEOUT,
                            TI_FALSE);
            pRsn->eGroupKeyUpdate = GROUP_KEY_UPDATE_TRUE;
        }
		else 
        {
			if (pRsn->bPairwiseMicFailureFilter)	/* the value of this flag is taken from registry */
			{
				TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: rsn_setKey, Pairwise ReKey timer started\n");
				tmr_StopTimer (pRsn->hMicFailurePairwiseReKeyTimer);
				tmr_StartTimer (pRsn->hMicFailurePairwiseReKeyTimer,
								rsn_pairwiseReKeyTimeout,
								(TI_HANDLE)pRsn,
								RSN_MIC_FAILURE_RE_KEY_TIMEOUT,
								TI_FALSE);
				pRsn->ePairwiseKeyUpdate = PAIRWISE_KEY_UPDATE_TRUE;
			}
        }
		}
 
        /* Mark key as added */
        pRsn->keys_en [keyIndex] = TI_TRUE;

        tTwdParam.paramType = TWD_RSN_KEY_ADD_PARAM_ID;
        tTwdParam.content.configureCmdCBParams.pCb = (TI_UINT8*) pKey;
        tTwdParam.content.configureCmdCBParams.fCb = NULL;
        tTwdParam.content.configureCmdCBParams.hCb = NULL;
        status = TWD_SetParam (pRsn->hTWD, &tTwdParam);
    TRACE3(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: rsn_setKey, KeyType=%d, KeyId = 0x%lx,encLen=0x%x\n", pKey->keyType,pKey->keyIndex, pKey->encLen);

    TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "\nEncKey = ");

    TRACE_INFO_HEX(pRsn->hReport, (TI_UINT8 *)pKey->encKey, pKey->encLen);

    if (pKey->keyType != KEY_WEP)
    { 
        TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "\nMac address = ");
        TRACE_INFO_HEX(pRsn->hReport, (TI_UINT8 *)pKey->macAddress, MAC_ADDR_LEN);
        TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "\nRSC = ");
        TRACE_INFO_HEX(pRsn->hReport, (TI_UINT8 *)pKey->keyRsc, KEY_RSC_LEN);
        TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "\nMic RX = ");
        TRACE_INFO_HEX(pRsn->hReport, (TI_UINT8 *)pKey->micRxKey, MAX_KEY_LEN);
        TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "\nMic TX = ");
        TRACE_INFO_HEX(pRsn->hReport, (TI_UINT8 *)pKey->micTxKey, MAX_KEY_LEN);
    }
    }

    return status; 
}


TI_STATUS rsn_removeKey (rsn_t *pRsn, TSecurityKeys *pKey)
{
    TI_STATUS           status = TI_OK;
    TTwdParamInfo       tTwdParam;
    TI_UINT8               keyIndex;

	if (pRsn == NULL || pKey == NULL)
	{
		return TI_NOK;
	}

	keyIndex = (TI_UINT8)pKey->keyIndex;
    if (keyIndex >= MAX_KEYS_NUM)
    {
        return TI_NOK;
    }

    TRACE2(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_removeKey Entry, keyType=%d, keyIndex=0x%lx\n",pKey->keyType, keyIndex);

    /* Now set to the RSN structure. */
    if (pRsn->keys_en[keyIndex])
    {
        tTwdParam.paramType = TWD_RSN_KEY_REMOVE_PARAM_ID;
        /*os_memoryCopy(pRsn->hOs, &tTwdParam.content.rsnKey, pKey, sizeof(TSecurityKeys));*/
        tTwdParam.content.configureCmdCBParams.pCb = (TI_UINT8*) pKey;
        tTwdParam.content.configureCmdCBParams.fCb = NULL;
        tTwdParam.content.configureCmdCBParams.hCb = NULL;

        if ( pKey->keyType == KEY_NULL ) {
				/* If keytype is unknown, retreive it from the RSN context */
				pKey->keyType = pRsn->keys[keyIndex].keyType;
        }

        /* If keyType is TKIP or AES, set the encLen to the KEY enc len - 16 */
        if (pKey->keyType == KEY_TKIP || pKey->keyType == KEY_AES)
        {
            pKey->encLen = 16;
            if (keyIndex != 0)
            {   
                const TI_UINT8 broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
                /* 
                 * if keyType is TKIP or AES, and the key index is broadcast, overwrite the MAC address as broadcast 
                 * for removing the Broadcast key from the FW 
                 */
                MAC_COPY (pKey->macAddress, broadcast);
            }
        }
		else if (pKey->keyType == KEY_WEP)
		{
			/* In full driver we use only WEP default keys. To remove it we make sure that the MAC address is NULL */
			os_memoryZero(pRsn->hOs,(void*)pKey->macAddress,sizeof(TMacAddr)); 
		}

       
        /* Mark key as deleted */
        pRsn->keys_en[keyIndex] = TI_FALSE;

        status = TWD_SetParam (pRsn->hTWD, &tTwdParam);
        
        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_removeKey in whal, status =%d\n", status);

        /* clean the key flags*/
        pRsn->keys[keyIndex].keyIndex &= 0x000000FF;
        pRsn->keys[keyIndex].keyType   = KEY_NULL;
        pRsn->keys[keyIndex].encLen    = 0;
        pRsn->wepDefaultKeys[keyIndex] = TI_FALSE;        
    }

    return status; 
}


TI_STATUS rsn_setDefaultKeyId(rsn_t *pRsn, TI_UINT8 keyId)
{
    TI_STATUS               status = TI_OK;
    TTwdParamInfo           tTwdParam;

    if (pRsn == NULL)
    {
        return TI_NOK;
    }
    pRsn->defaultKeyId = keyId;
    /* Now we configure default key ID to the HAL */
    tTwdParam.paramType = TWD_RSN_DEFAULT_KEY_ID_PARAM_ID;
    tTwdParam.content.configureCmdCBParams.pCb = &keyId;
    tTwdParam.content.configureCmdCBParams.fCb = NULL;
    tTwdParam.content.configureCmdCBParams.hCb = NULL;
    status = TWD_SetParam (pRsn->hTWD, &tTwdParam); 
    
    TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "RSN: rsn_setDefaultKeyId, KeyId = 0x%lx\n", keyId);
    return status;
}


TI_STATUS rsn_reportAuthFailure(TI_HANDLE hRsn, EAuthStatus authStatus) 
{
    TI_STATUS    status = TI_OK;
    rsn_t       *pRsn;
    paramInfo_t param;

    if (hRsn==NULL)
    {
        return TI_NOK;
    }

    pRsn = (rsn_t*)hRsn;

    /* Remove AP from candidate list for a specified amount of time */
	param.paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
	status = ctrlData_getParam(pRsn->hCtrlData, &param);
	if (status != TI_OK)
	{
TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, "rsn_reportAuthFailure, unable to retrieve BSSID \n");
	}
    else
    {
        TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "current station is banned from the roaming candidates list for %d Ms\n", RSN_AUTH_FAILURE_TIMEOUT);

        rsn_banSite(hRsn, param.content.ctrlDataCurrentBSSID, RSN_SITE_BAN_LEVEL_FULL, RSN_AUTH_FAILURE_TIMEOUT);
    }

	
#ifdef XCC_MODULE_INCLUDED
TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "CALLING rougeAP, status= %d \n",authStatus);
    status = XCCMngr_rogueApDetected (pRsn->hXCCMngr, authStatus);
#endif
    TI_VOIDCAST(pRsn);
    return status;
}


/******
This is the CB function for mic failure event from the FW 
*******/
TI_STATUS rsn_reportMicFailure(TI_HANDLE hRsn, TI_UINT8 *pType, TI_UINT32 Length)
{
    rsn_t                               *pRsn = (rsn_t *) hRsn;
    ERsnSiteBanLevel                    banLevel;
    OS_802_11_AUTHENTICATION_REQUEST    *request;
    TI_UINT8 AuthBuf[sizeof(TI_UINT32) + sizeof(OS_802_11_AUTHENTICATION_REQUEST)];
    paramInfo_t                         param;
    TI_UINT8                               failureType;

    failureType = *pType;

   if (((pRsn->paeConfig.unicastSuite == TWD_CIPHER_TKIP) && (failureType == KEY_TKIP_MIC_PAIRWISE)) ||
        ((pRsn->paeConfig.broadcastSuite == TWD_CIPHER_TKIP) && (failureType == KEY_TKIP_MIC_GROUP)))
    {
        /* check if the MIC failure is group and group key update */
        /* was performed during the last 3 seconds */
        if ((failureType == KEY_TKIP_MIC_GROUP) &&
            (pRsn->eGroupKeyUpdate == GROUP_KEY_UPDATE_TRUE))
        {
            TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Group MIC failure ignored, key update was performed within the last 3 seconds.\n");
            return TI_OK;
        }

		/* check if the MIC failure is pairwise and pairwise key update */
        /* was performed during the last 3 seconds */
        if ((failureType == KEY_TKIP_MIC_PAIRWISE) &&
            (pRsn->ePairwiseKeyUpdate == PAIRWISE_KEY_UPDATE_TRUE))
        {	
            TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Pairwise MIC failure ignored, key update was performed within the last 3 seconds.\n");
            return TI_OK;
        }

        /* Prepare the Authentication Request */
        request = (OS_802_11_AUTHENTICATION_REQUEST *)(AuthBuf + sizeof(TI_UINT32));
        request->Length = sizeof(OS_802_11_AUTHENTICATION_REQUEST);

        param.paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
        if (ctrlData_getParam (pRsn->hCtrlData, &param) != TI_OK)
        {
            return TI_NOK;
        }
        
        /* Generate 802 Media specific indication event */
        *(TI_UINT32*)AuthBuf = os802_11StatusType_Authentication;

        MAC_COPY (request->BSSID, param.content.ctrlDataCurrentBSSID);

        if (failureType == KEY_TKIP_MIC_PAIRWISE)
        {
            request->Flags = OS_802_11_REQUEST_PAIRWISE_ERROR;
        }
        else
        {
            request->Flags = OS_802_11_REQUEST_GROUP_ERROR;
        }

		EvHandlerSendEvent (pRsn->hEvHandler, 
                            IPC_EVENT_MEDIA_SPECIFIC, 
                            (TI_UINT8*)AuthBuf,
                            sizeof(TI_UINT32) + sizeof(OS_802_11_AUTHENTICATION_REQUEST));

		if ( pRsn->bRsnExternalMode ) {
			return TI_OK;
		}

        /* Update and check the ban level to decide what actions need to take place */
        banLevel = rsn_banSite (hRsn, param.content.ctrlDataCurrentBSSID, RSN_SITE_BAN_LEVEL_HALF, RSN_MIC_FAILURE_TIMEOUT);
        if (banLevel == RSN_SITE_BAN_LEVEL_FULL)
        {
            /* Site is banned so prepare to disconnect */
            TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Second MIC failure, closing Rx port...\n");

            param.paramType = RX_DATA_PORT_STATUS_PARAM;
            param.content.rxDataPortStatus = CLOSE;
            rxData_setParam(pRsn->hRx, &param);

            /* stop the mic failure Report timer and start a new one for 0.5 seconds */
            tmr_StopTimer (pRsn->hMicFailureReportWaitTimer);
		    apConn_setDeauthPacketReasonCode(pRsn->hAPConn, STATUS_MIC_FAILURE);
            tmr_StartTimer (pRsn->hMicFailureReportWaitTimer,
                            rsn_micFailureReportTimeout,
                            (TI_HANDLE)pRsn,
                            RSN_MIC_FAILURE_REPORT_TIMEOUT,
                            TI_FALSE);
        }
        else
        {
            /* Site is only half banned so nothing needs to be done for now */
            TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": First MIC failure, business as usual for now...\n");
        }
    }

    return TI_OK;
}


void rsn_groupReKeyTimeout(TI_HANDLE hRsn, TI_BOOL bTwdInitOccured)
{
    rsn_t *pRsn;

    pRsn = (rsn_t*)hRsn;

    if (pRsn == NULL)
    {
        return;
    }

    pRsn->eGroupKeyUpdate = GROUP_KEY_UPDATE_FALSE;
}


void rsn_pairwiseReKeyTimeout(TI_HANDLE hRsn, TI_BOOL bTwdInitOccured)
{
    rsn_t *pRsn;

    pRsn = (rsn_t*)hRsn;

    if (pRsn == NULL)
    {
        return;
    }

    pRsn->ePairwiseKeyUpdate = PAIRWISE_KEY_UPDATE_FALSE;
}

void rsn_micFailureReportTimeout (TI_HANDLE hRsn, TI_BOOL bTwdInitOccured)
{
    rsn_t *pRsn;

    pRsn = (rsn_t*)hRsn;

    if (pRsn == NULL)
    {
        return;
    }

    TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": MIC failure reported, disassociating...\n");

    apConn_reportRoamingEvent (pRsn->hAPConn, ROAMING_TRIGGER_SECURITY_ATTACK, NULL);
}


/**
*
* rsn_resetPMKIDList - 
*
* \b Description: 
*   Cleans up the PMKID cache.
*   Called when SSID is being changed.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*/

TI_STATUS rsn_resetPMKIDList(TI_HANDLE hRsn)
{
    rsn_t  *pRsn = (rsn_t*)hRsn;

    if (!pRsn)
        return TI_NOK;

    return (pRsn->pAdmCtrl->resetPmkidList (pRsn->pAdmCtrl));
}


void rsn_debugFunc(TI_HANDLE hRsn)
{
    rsn_t *pRsn;

    if (hRsn == NULL)
    {
        return;
    }
    pRsn = (rsn_t*)hRsn;

    WLAN_OS_REPORT(("rsnStartedTs, ts = %d\n", pRsn->rsnStartedTs));
    WLAN_OS_REPORT(("rsnCompletedTs, ts = %d\n", pRsn->rsnCompletedTs));  
}


/**
*
* rsn_startPreAuth - 
*
* \b Description: 
*
* Start pre-authentication on a list of given BSSIDs.
*
* \b ARGS:
*
*  I   - hRsn - Rsniation SM context  \n
*  I/O - pBssidList - list of BSSIDs that require Pre-Auth \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS rsn_startPreAuth(TI_HANDLE hRsn, TBssidList4PreAuth *pBssidList)
{
    rsn_t       *pRsn;
    TI_STATUS    status;

    if ( (NULL == hRsn) || (NULL == pBssidList) )
    {
        return TI_NOK;
    }

    pRsn = (rsn_t*)hRsn;

    status = pRsn->pAdmCtrl->startPreAuth (pRsn->pAdmCtrl, pBssidList);

    TRACE0(pRsn->hReport, REPORT_SEVERITY_INFORMATION, "rsn_startPreAuth \n");

    return status;
}


/**
 *
 * isSiteBanned - 
 *
 * \b Description: 
 *
 * Returns whether or not the site with the specified Bssid is banned or not. 
 *
 * \b ARGS:
 *
 *  I   - hRsn - RSN module context \n
 *  I   - siteBssid - The desired site's bssid \n
 *
 * \b RETURNS:
 *
 *  TI_NOK iff site is banned.
 *
 */
TI_BOOL rsn_isSiteBanned(TI_HANDLE hRsn, TMacAddr siteBssid)
{
    rsn_t * pRsn = (rsn_t *) hRsn;
    rsn_siteBanEntry_t * entry;

    /* Check if site is in the list */
    if ((entry = findBannedSiteAndCleanup(hRsn, siteBssid)) == NULL)
    {
        return TI_FALSE;
    }

    TRACE7(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Site %02X-%02X-%02X-%02X-%02X-%02X found with ban level %d...\n", siteBssid[0], siteBssid[1], siteBssid[2], siteBssid[3], siteBssid[4], siteBssid[5], entry->banLevel);

    return (entry->banLevel == RSN_SITE_BAN_LEVEL_FULL);
}


/**
 *
 * rsn_PortStatus_Set API implementation-
 *
 * \b Description:
 *
 * set the status port according to the status flag
 *
 * \b ARGS:
 *
 *  I   - hRsn - RSN module context \n
 *  I   - state - The status flag \n
 *
 * \b RETURNS:
 *
 *  TI_STATUS.
 *
 */
TI_STATUS rsn_setPortStatus(TI_HANDLE hRsn, TI_BOOL state)
{
    rsn_t                   *pRsn = (rsn_t *)hRsn;
    struct externalSec_t	*pExtSec;

    pExtSec = pRsn->pMainSecSm->pExternalSec;
    pExtSec->bPortStatus = state;
    if (state)
			pRsn->reportStatus( pRsn, TI_OK );
    return externalSec_rsnComplete(pExtSec);
}


/**
 *
 * rsn_banSite - 
 *
 * \b Description: 
 *
 * Bans the specified site from being associated to for the specified duration.
 * If a ban level of WARNING is given and no previous ban was in effect the
 * warning is marked down but other than that nothing happens. In case a previous
 * warning (or ban of course) is still in effect
 *
 * \b ARGS:
 *
 *  I   - hRsn - RSN module context \n
 *  I   - siteBssid - The desired site's bssid \n
 *  I   - banLevel - The desired level of ban (Warning / Ban)
 *  I   - durationMs - The duration of ban in milliseconds
 *
 * \b RETURNS:
 *
 *  The level of ban (warning / banned).
 *
 */
ERsnSiteBanLevel rsn_banSite(TI_HANDLE hRsn, TMacAddr siteBssid, ERsnSiteBanLevel banLevel, TI_UINT32 durationMs)
{
    rsn_t * pRsn = (rsn_t *) hRsn;
    rsn_siteBanEntry_t * entry;

    /* Try finding the site in the list */
    if ((entry = findBannedSiteAndCleanup(hRsn, siteBssid)) != NULL)
    {
        /* Site found so a previous ban is still in effect */ 
        TRACE6(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Site %02X-%02X-%02X-%02X-%02X-%02X found and has been set to ban level full!\n", siteBssid[0], siteBssid[1], siteBssid[2], siteBssid[3], siteBssid[4], siteBssid[5]);

        entry->banLevel = RSN_SITE_BAN_LEVEL_FULL;
    }
    else
    {
        /* Site doesn't appear in the list, so find a place to insert it */
        TRACE7(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Site %02X-%02X-%02X-%02X-%02X-%02X added with ban level %d!\n", siteBssid[0], siteBssid[1], siteBssid[2], siteBssid[3], siteBssid[4], siteBssid[5], banLevel);

        entry = findEntryForInsert (hRsn);

		MAC_COPY (entry->siteBssid, siteBssid);
        entry->banLevel = banLevel;

        pRsn->numOfBannedSites++;
    }

    entry->banStartedMs = os_timeStampMs (pRsn->hOs);
    entry->banDurationMs = durationMs;

    return entry->banLevel;
}


/**
 *
 * findEntryForInsert - 
 *
 * \b Description: 
 *
 * Returns a place to insert a new banned site. 
 *
 * \b ARGS:
 *
 *  I   - hRsn - RSN module context \n
 *
 * \b RETURNS:
 *
 *  A pointer to a suitable site entry.
 *
 */
static rsn_siteBanEntry_t * findEntryForInsert(TI_HANDLE hRsn)
{
    rsn_t * pRsn = (rsn_t *) hRsn;

    /* In the extreme case that the list is full we overwrite an old entry */
    if (pRsn->numOfBannedSites == RSN_MAX_NUMBER_OF_BANNED_SITES)
    {
        TRACE0(pRsn->hReport, REPORT_SEVERITY_ERROR, ": No room left to insert new banned site, overwriting old one!\n");

        return &(pRsn->bannedSites[0]);
    }

    return &(pRsn->bannedSites[pRsn->numOfBannedSites]);
}


/**
 *
 * findBannedSiteAndCleanup - 
 *
 * \b Description: 
 *
 * Searches the banned sites list for the desired site while cleaning up
 * expired sites found along the way.
 * 
 * Note that this function might change the structure of the banned sites 
 * list so old iterators into the list might be invalidated.
 *
 * \b ARGS:
 *
 *  I   - hRsn - RSN module context \n
 *  I   - siteBssid - The desired site's bssid \n
 *
 * \b RETURNS:
 *
 *  A pointer to the desired site's entry if found,
 *  NULL otherwise.
 *
 */
static rsn_siteBanEntry_t * findBannedSiteAndCleanup(TI_HANDLE hRsn, TMacAddr siteBssid)
{
    rsn_t * pRsn = (rsn_t *) hRsn;
    int iter;

    for (iter = 0; iter < pRsn->numOfBannedSites; iter++)
    {
        /* If this entry has expired we'd like to clean it up */
        if (os_timeStampMs(pRsn->hOs) - pRsn->bannedSites[iter].banStartedMs >= pRsn->bannedSites[iter].banDurationMs)
        {
            TRACE1(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Found expired entry at index %d, cleaning it up...\n", iter);

            /* Replace this entry with the last one */
            pRsn->bannedSites[iter] = pRsn->bannedSites[pRsn->numOfBannedSites - 1];
            pRsn->numOfBannedSites--;

            /* we now repeat the iteration on this entry */
            iter--;

            continue;
        }

        /* Is this the entry for the site we're looking for? */
        if (MAC_EQUAL (siteBssid, pRsn->bannedSites[iter].siteBssid))
        {
            TRACE7(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Site %02X-%02X-%02X-%02X-%02X-%02X found at index %d!\n", siteBssid[0], siteBssid[1], siteBssid[2], siteBssid[3], siteBssid[4], siteBssid[5], iter);

            return &pRsn->bannedSites[iter];
        } 
    }

    /* Entry not found... */
    TRACE6(pRsn->hReport, REPORT_SEVERITY_INFORMATION, ": Site %02X-%02X-%02X-%02X-%02X-%02X not found...\n", siteBssid[0], siteBssid[1], siteBssid[2], siteBssid[3], siteBssid[4], siteBssid[5]);

    return NULL;
}

/**
 *
 * rsn_getPortStatus -
 *
 * \b Description:
 *
 * Returns the extrenalSec port status
 *
 * \b ARGS:
 *
 *  pRsn - pointer to RSN module context \n
 *
 * \b RETURNS:
 *
 *  TI_BOOL - the port status True = Open , False = Close
 *
 */
TI_BOOL rsn_getPortStatus(rsn_t *pRsn)
{
    struct externalSec_t	*pExtSec;

    pExtSec = pRsn->pMainSecSm->pExternalSec;
    return pExtSec->bPortStatus;
}

/**
 *
 * rsn_getGenInfoElement -
 *
 * \b Description:
 *
 * Copies the Generic IE to a given buffer
 *
 * \b ARGS:
 *
 *  I     pRsn           - pointer to RSN module context \n
 *  O     out_buff       - pointer to the output buffer  \n
 *  O     out_buf_length - length of data copied into output buffer \n
 *
 * \b RETURNS:
 *
 *  TI_UINT8 - The amount of bytes copied.
 *
 */
TI_STATUS rsn_getGenInfoElement(rsn_t *pRsn, TI_UINT8 *out_buff, TI_UINT32 *out_buf_length)
{
	if ( !(pRsn && out_buff && out_buf_length) ) {
			return TI_NOK;
	}

	*out_buf_length = pRsn->genericIE.length;
	if (pRsn->genericIE.length > 0) {
			os_memoryCopy(pRsn->hOs, out_buff, pRsn->genericIE.data, pRsn->genericIE.length);
	}

	return TI_OK;
}

/**
 *
 * rsn_clearGenInfoElement -
 *
 * \b Description:
 *
 * Clears the Generic IE
 *
 * \b ARGS:
 *
 *  I     pRsn           - pointer to RSN module context \n
 *
 */
void rsn_clearGenInfoElement(rsn_t *pRsn )
{
    os_memoryZero(pRsn->hOs, &pRsn->genericIE, sizeof(pRsn->genericIE));
}


#ifdef RSN_NOT_USED

static TI_INT16 convertAscii2Unicode(TI_INT8* userPwd, TI_INT16 len)
{
    TI_INT16 i;
    TI_INT8 unsiiPwd[MAX_PASSWD_LEN];
    

    for (i=0; i<len; i++)
    {
        unsiiPwd[i] = userPwd[i];
    }
    for (i=0; i<len; i++)
    {
        userPwd[i*2] = unsiiPwd[i];
        userPwd[i*2+1] = 0;
    }
    return (TI_INT16)(len*2);     
}

#endif

/***************************************************************************
*							rsn_reAuth				                   *
****************************************************************************
* DESCRIPTION:	This is a callback function called by the whalWPA module whenever
*				a broadcast TKIP key was configured to the FW.
*				It does the following:
*					-	resets the ReAuth flag
*					-	stops the ReAuth timer
*					-	restore the PS state
*					-	Send RE_AUTH_COMPLETED event to the upper layer.
*
* INPUTS:		hRsn - the object
*
* OUTPUT:		None
*
* RETURNS:		None
*
***************************************************************************/
void rsn_reAuth(TI_HANDLE hRsn)
{
	rsn_t *pRsn;

	pRsn = (rsn_t*)hRsn;

	if (pRsn == NULL)
	{
		return;
	}

	if (rxData_IsReAuthInProgress(pRsn->hRx))
	{
		rxData_SetReAuthInProgress(pRsn->hRx, TI_FALSE);
		rxData_StopReAuthActiveTimer(pRsn->hRx);
		rxData_ReauthDisablePriority(pRsn->hRx);
		EvHandlerSendEvent(pRsn->hEvHandler, IPC_EVENT_RE_AUTH_COMPLETED, NULL, 0);
	}
}
