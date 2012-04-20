/*
 * admCtrlNone.c
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

/** \file admCtrl.c
 *  \brief Admission control API implimentation
 *
 *  \see admCtrl.h
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Admission Control	    		                                *
 *   PURPOSE: Admission Control Module API                              	*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_17
#include "osApi.h"
#include "paramOut.h"
#include "fsm.h"
#include "report.h"
#include "mlmeApi.h"
#include "DataCtrl_Api.h"
#include "rsn.h"
#include "admCtrl.h"
#include "admCtrlNone.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#include "admCtrlWpa.h"
#include "admCtrlXCC.h"
#endif
#include "TWDriver.h"


/* Constants */

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* Local functions definitions */

/* Global variables */

/* Function prototypes */

/**
*
* admCtrlNone_config  - Configure empty admission control.
*
* \b Description: 
*
* Configure empty admission control.
*
* \b ARGS:
*
*  I   - pAdmCtrl - context \n
*  
* \b RETURNS:
*
*  TI_OK on success, TI_NOK on failure.
*
* \sa 
*/
TI_STATUS admCtrlNone_config(admCtrl_t *pAdmCtrl)
{
	TI_STATUS			status;
	TRsnPaeConfig   	paeConfig;

#ifdef XCC_MODULE_INCLUDED
	TTwdParamInfo   	tTwdParam;
#endif

	if ((pAdmCtrl->authSuite !=	RSN_AUTH_OPEN ) &&
		(pAdmCtrl->authSuite !=	RSN_AUTH_SHARED_KEY) &&
		(pAdmCtrl->authSuite !=	RSN_AUTH_AUTO_SWITCH))	{
		/* The default is OPEN */
		pAdmCtrl->authSuite =	RSN_AUTH_OPEN;
	}
	
	/* set admission control parameters */
    pAdmCtrl->keyMngSuite = RSN_KEY_MNG_NONE;
	pAdmCtrl->externalAuthMode = (EExternalAuthMode)pAdmCtrl->authSuite;
	
	/* set callback functions (API) */
	pAdmCtrl->getInfoElement = admCtrlNone_getInfoElement;
	pAdmCtrl->setSite = admCtrlNone_setSite;
	pAdmCtrl->evalSite = admCtrlNone_evalSite;

	pAdmCtrl->getPmkidList	 = admCtrl_nullGetPMKIDlist;
	pAdmCtrl->setPmkidList	 = admCtrl_nullSetPMKIDlist;
	pAdmCtrl->resetPmkidList = admCtrl_resetPMKIDlist;
	pAdmCtrl->getPreAuthStatus = admCtrl_nullGetPreAuthStatus;
	pAdmCtrl->startPreAuth	= admCtrl_nullStartPreAuth;
    pAdmCtrl->get802_1x_AkmExists = admCtrl_nullGet802_1x_AkmExists;


	
	/* set cipher suite */
	pAdmCtrl->broadcastSuite =  TWD_CIPHER_NONE;
	pAdmCtrl->unicastSuite = TWD_CIPHER_NONE;


	/* set PAE parametrs */
	paeConfig.authProtocol = pAdmCtrl->externalAuthMode;
	paeConfig.unicastSuite = pAdmCtrl->unicastSuite;
	paeConfig.broadcastSuite = pAdmCtrl->broadcastSuite;
	paeConfig.keyExchangeProtocol = pAdmCtrl->keyMngSuite;
	/* set default PAE configuration */
	status = pAdmCtrl->pRsn->setPaeConfig(pAdmCtrl->pRsn, &paeConfig);
						
#ifdef XCC_MODULE_INCLUDED
	/* Clean MIC and KP in HAL and re-send WEP-keys  */
	tTwdParam.paramType = TWD_RSN_XCC_SW_ENC_ENABLE_PARAM_ID; 
	tTwdParam.content.rsnXCCSwEncFlag = TI_FALSE;
	status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);

	tTwdParam.paramType = TWD_RSN_XCC_MIC_FIELD_ENABLE_PARAM_ID; 
	tTwdParam.content.rsnXCCMicFieldFlag = TI_FALSE;
	status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);
#endif /*XCC_MODULE_INCLUDED*/

	return status;
}


/**
*
* admCtrlNone_getInfoElement - Get the current information element.
*
* \b Description: 
*
* Get the current information element.
*
* \b ARGS:
*
*  I   - pAdmCtrl - context \n
*  I   - pIe - IE buffer \n
*  I   - pLength - length of IE \n
*  
* \b RETURNS:
*
*  TI_OK on success, TI_NOK on failure.
*
* \sa 
*/
TI_STATUS admCtrlNone_getInfoElement(admCtrl_t *pAdmCtrl, TI_UINT8 *pIe, TI_UINT32 *pLength)
{
	*pLength = 0;
	pIe = NULL;

	TI_VOIDCAST(pIe);
	return TI_OK;
}
/**
*
* admCtrlNone_setSite  - Set current primary site parameters for registration.
*
* \b Description: 
*
* Set current primary site parameters for registration.
*
* \b ARGS:
*
*  I   - pAdmCtrl - context \n
*  I   - pRsnData - site's RSN data \n
*  O   - pAssocIe - result IE of evaluation \n
*  O   - pAssocIeLen - length of result IE of evaluation \n
*  
* \b RETURNS:
*
*  TI_OK on site is aproved, TI_NOK on site is rejected.
*
* \sa 
*/
TI_STATUS admCtrlNone_setSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen)
{
	TI_STATUS			status;
	paramInfo_t			param;
	TTwdParamInfo		tTwdParam;
	EAuthSuite			authSuite;

	admCtrlNone_config(pAdmCtrl);

	authSuite = pAdmCtrl->authSuite;

  /* Config the default keys */
	if ((authSuite == RSN_AUTH_SHARED_KEY) || (authSuite == RSN_AUTH_AUTO_SWITCH))
	{   /* Configure Security status in HAL */
		tTwdParam.paramType = TWD_RSN_SECURITY_MODE_PARAM_ID;
		tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_WEP;
		status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);
		/* Configure the keys in HAL */
		rsn_setDefaultKeys(pAdmCtrl->pRsn);
	}

#ifdef XCC_MODULE_INCLUDED
	admCtrlXCC_setExtendedParams(pAdmCtrl, pRsnData);
#endif

	/* Now we configure the MLME module with the 802.11 legacy authentication suite, 
		THe MLME will configure later the authentication module */
	param.paramType = MLME_LEGACY_TYPE_PARAM;
	switch (authSuite)
	{
	case RSN_AUTH_OPEN:
		param.content.mlmeLegacyAuthType = AUTH_LEGACY_OPEN_SYSTEM;
		break;

	case RSN_AUTH_SHARED_KEY: 
		param.content.mlmeLegacyAuthType = AUTH_LEGACY_SHARED_KEY;
		break;

	case RSN_AUTH_AUTO_SWITCH:
		param.content.mlmeLegacyAuthType = AUTH_LEGACY_AUTO_SWITCH;
		break;

	default:
		return TI_NOK;
	}
	
	status = mlme_setParam(pAdmCtrl->hMlme, &param);
	if (status != TI_OK)
	{
		return status;
	}

	param.paramType = RX_DATA_EAPOL_DESTINATION_PARAM;
	param.content.rxDataEapolDestination = OS_ABS_LAYER;
	status = rxData_setParam(pAdmCtrl->hRx, &param);
	if (status != TI_OK)
	{
		return status;
	}

	/* Configure privacy status in HAL */
	if (authSuite == RSN_AUTH_OPEN)
	{
		tTwdParam.paramType = TWD_RSN_SECURITY_MODE_PARAM_ID;
		tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)TWD_CIPHER_NONE;
		status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);
	}

	return status;
}

/**
*
* admCtrlNone_evalSite  - Evaluate site for registration.
*
* \b Description: 
*
* evaluate site RSN capabilities against the station's cap.
* If the BSS type is infrastructure, the station matches the site only if it's WEP status is same as the site
* In IBSS, it does not matter
*
* \b ARGS:
*
*  I   - pAdmCtrl - Context \n
*  I   - pRsnData - site's RSN data \n
*  O   - pEvaluation - Result of evaluation \n
*  
* \b RETURNS:
*
*  TI_OK 
*
* \sa 
*/
TI_STATUS admCtrlNone_evalSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pEvaluation)
{
	if ((pAdmCtrl==NULL) || (pEvaluation==NULL)){
		return TI_NOK;
	}
	*pEvaluation = 1;
	
	/* Check privacy bit if not in mixed mode */
	if (!pAdmCtrl->mixedMode)
	{   /* There's no mixed mode, so make sure that the privacy Bit is off*/
		if (pRsnData->privacy)
		{
			*pEvaluation = 0;
			return TI_NOK;
		}
	}

    TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlNone_evalSite:  pEvaluation=%d\n\n", *pEvaluation);

   	return TI_OK;
	
}


