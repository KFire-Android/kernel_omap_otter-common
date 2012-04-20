/*
 * SoftGemini.c
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

/** \file softGemini.c
 *  \brief BlueTooth-Wlan coexistence module interface
 *
 *  \see softGemini.h
 */

/****************************************************************************************************
*																									*
*		MODULE:		softGemini.c																	*
*		PURPOSE:	BlueTooth-Wlan coexistence module interface.							*
*					This module handles all data base (and Fw setting accordingly)					*
*					for Bluetooth-Wlan coexistence implementation.									*
*																						 			*
****************************************************************************************************/

#define __FILE_ID__  FILE_ID_5
#include "report.h"
#include "osApi.h"
#include "SoftGemini.h"
#include "DataCtrl_Api.h"
#include "scrApi.h"
#include "PowerMgr_API.h"
#include "ScanCncn.h"
#include "currBss.h"
#include "CmdDispatcher.h"
#include "TWDriver.h"
#include "DrvMainModules.h"
#include "bssTypes.h"
#include "sme.h"


#define SENSE_MODE_ENABLE		0x01
#define SENSE_MODE_DISABLE		0x00
#define PROTECTIVE_MODE_ON		0x01
#define PROTECTIVE_MODE_OFF		0x00

/********************************************************************************/
/*						Internal functions prototypes.							*/
/********************************************************************************/

static TI_STATUS SoftGemini_setEnableParam(TI_HANDLE hSoftGemini, ESoftGeminiEnableModes SoftGeminiEnable, TI_BOOL recovery);
static TI_STATUS SoftGemini_setParamsToFW(TI_HANDLE hSoftGemini, TSoftGeminiParams *SoftGeminiParam);
static TI_STATUS SoftGemini_EnableDriver(TI_HANDLE hSoftGemini);
static TI_STATUS SoftGemini_DisableDriver(TI_HANDLE hSoftGemini);
static TI_STATUS SoftGemini_SetPS(SoftGemini_t	 *pSoftGemini);
static TI_STATUS SoftGemini_unSetPS(SoftGemini_t *pSoftGemini);
static void SoftGemini_RemoveProtectiveModeParameters(TI_HANDLE hSoftGemini);
static void SoftGemini_setConfigParam(TI_HANDLE hSoftGemini, TI_UINT32 *param);
static void SoftGemini_EnableProtectiveMode(TI_HANDLE hSoftGemini);
static void SoftGemini_DisableProtectiveMode(TI_HANDLE hSoftGemini);
#ifdef REPORT_LOG
static char* SoftGemini_ConvertModeToString(ESoftGeminiEnableModes SoftGeminiEnable);
#endif

/********************************************************************************/
/*						Interface functions Implementation.						*/
/********************************************************************************/
/************************************************************************
 *                        SoftGemini_SetPSmode									*
 ************************************************************************
DESCRIPTION: SoftGemini module, called by the conn_Infra on connection
				performs the following:
				-	Enables SG if needed
                                -       Enables the SG power mode				                                                                                                   
INPUT:      hSoftGemini -		Handle to SoftGemini		

OUTPUT:		

RETURN:     

************************************************************************/
void SoftGemini_SetPSmode(TI_HANDLE hSoftGemini)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;

	if (pSoftGemini)
	{
		if (pSoftGemini->bDriverEnabled) 
		{
			/* Check if coexAutoPsMode is enabled to enter/exit P.S */
			if ( pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_AUTO_PS_MODE])
			{
				SoftGemini_SetPS(pSoftGemini);
			}
		}
		if (pSoftGemini->bProtectiveMode) 
		{
			SoftGemini_EnableProtectiveMode(hSoftGemini);
		}
	}
	else 
        {
          TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, "  SoftGemini_SetPSmode() - Error hSoftGemini= NULL \n");
        }
}

/************************************************************************
 *                        SoftGemini_unSetPSmode									*
 ************************************************************************
DESCRIPTION: SoftGemini module, called by the conn_Infra after disconnecting 
				performs the following:
				-	Disables the SG
                                -       Disables the SG power mode				                                                                                                   
INPUT:      hSoftGemini -		Handle to SoftGemini		

OUTPUT:		

RETURN:     

************************************************************************/
void SoftGemini_unSetPSmode(TI_HANDLE hSoftGemini)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;

    if (pSoftGemini)
	{
		if (pSoftGemini->bDriverEnabled) 
		{
			/* Check if coexAutoPsMode is enabled to enter/exit P.S */
			if ( pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_AUTO_PS_MODE])
			{
				SoftGemini_unSetPS(pSoftGemini);
			}
		}
		if (pSoftGemini->bProtectiveMode) 
		{
			SoftGemini_RemoveProtectiveModeParameters(hSoftGemini);
		}
	}
    else
    {
		TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, " SoftGemini_unSetPSmode() - Error hSoftGemini= NULL \n");
    }
}

/************************************************************************
 *                        SoftGemini_create									*
 ************************************************************************
DESCRIPTION: SoftGemini module creation function, called by the config mgr in creation phase 
				performs the following:
				-	Allocate the SoftGemini handle
				                                                                                                   
INPUT:      hOs -			Handle to OS		


OUTPUT:		

RETURN:     Handle to the SoftGemini module on success, NULL otherwise

************************************************************************/
TI_HANDLE SoftGemini_create(TI_HANDLE hOs)
{
	SoftGemini_t			*pSoftGemini = NULL;

	/* allocating the SoftGemini object */
	pSoftGemini = os_memoryAlloc(hOs,sizeof(SoftGemini_t));

	if (pSoftGemini == NULL)
		return NULL;				

	pSoftGemini->hOs = hOs;

	return pSoftGemini;
}

/************************************************************************
 *                        SoftGemini_config						*
 ************************************************************************
DESCRIPTION: SoftGemini module init function, called by the rvMain in init phase
				performs the following:
				-	Init local variables
				-	Init the handles to be used by the module
                                                                                                   
INPUT:       pStadHandles  - The driver modules handles		

OUTPUT:		

RETURN:      void
************************************************************************/
void SoftGemini_init (TStadHandlesList *pStadHandles)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)(pStadHandles->hSoftGemini);

	pSoftGemini->hCtrlData    = pStadHandles->hCtrlData;
	pSoftGemini->hTWD	      = pStadHandles->hTWD;
	pSoftGemini->hReport	  = pStadHandles->hReport;
	pSoftGemini->hSCR         = pStadHandles->hSCR;
	pSoftGemini->hPowerMgr    = pStadHandles->hPowerMgr;
	pSoftGemini->hCmdDispatch = pStadHandles->hCmdDispatch;
	pSoftGemini->hScanCncn    = pStadHandles->hScanCncn;
	pSoftGemini->hCurrBss	  = pStadHandles->hCurrBss;
    pSoftGemini->hSme         = pStadHandles->hSme;
}


TI_STATUS SoftGemini_SetDefaults (TI_HANDLE hSoftGemini, SoftGeminiInitParams_t *pSoftGeminiInitParams)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;
	TI_UINT8 i =0;
	TI_STATUS status;
	/*************************************/
	/* Getting SoftGemini init Params */
	/***********************************/

	pSoftGemini->SoftGeminiEnable = pSoftGeminiInitParams->SoftGeminiEnable;

	for (i =0; i< SOFT_GEMINI_PARAMS_MAX ; i++)
	{
		pSoftGemini->SoftGeminiParam.coexParams[i] = pSoftGeminiInitParams->coexParams[i];
	}

	pSoftGemini->SoftGeminiParam.paramIdx = 0xFF; /* signals to FW to config all the paramters */


    /* Send the configuration to the FW */
	status = SoftGemini_setParamsToFW(hSoftGemini, &pSoftGemini->SoftGeminiParam);

	/*******************************/
    /* register Indication interrupts  */
	/*****************************/

    TWD_RegisterEvent (pSoftGemini->hTWD, 
                       TWD_OWN_EVENT_SOFT_GEMINI_SENSE,
                       (void *)SoftGemini_SenseIndicationCB, 
                       hSoftGemini);
	TWD_RegisterEvent (pSoftGemini->hTWD, 
                       TWD_OWN_EVENT_SOFT_GEMINI_PREDIC,
                       (void *)SoftGemini_ProtectiveIndicationCB, 
                       hSoftGemini);

    TWD_EnableEvent (pSoftGemini->hTWD, TWD_OWN_EVENT_SOFT_GEMINI_SENSE);
	TWD_EnableEvent (pSoftGemini->hTWD, TWD_OWN_EVENT_SOFT_GEMINI_PREDIC);

	/* On system initialization SG is disabled but later calls to SoftGemini_setEnableParam() */
	pSoftGemini->bProtectiveMode = TI_FALSE;	
	pSoftGemini->SoftGeminiEnable = SG_DISABLE; 
	pSoftGemini->bDriverEnabled = TI_FALSE;
        pSoftGemini->bPsPollFailureActive = TI_FALSE;

	if ((TI_OK == status) && (pSoftGeminiInitParams->SoftGeminiEnable != SG_DISABLE))
	{	/* called only if different than SG_DISABLE */
		status = SoftGemini_setEnableParam(hSoftGemini, pSoftGeminiInitParams->SoftGeminiEnable, TI_FALSE);
	}

	if (status == TI_OK)
	{
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INIT, "  SoftGemini_config() - configured successfully\n");
	}
	else
	{
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, "  SoftGemini_config() - Error configuring module \n");
	}

	return status;
}

/************************************************************************
 *                        SoftGemini_destroy							*
 ************************************************************************
DESCRIPTION: SoftGemini module destroy function, called by the config mgr in the destroy phase 
				performs the following:
				-	Free all memory aloocated by the module
                                                                                                   
INPUT:      hSoftGemini	-	SoftGemini handle.		


OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS SoftGemini_destroy(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;

	if (pSoftGemini != NULL)
	{
		os_memoryFree( pSoftGemini->hOs, (TI_HANDLE)pSoftGemini , sizeof(SoftGemini_t));
	}
	
	return TI_OK;
}


/***********************************************************************
 *                        SoftGemini_setParam									
 ***********************************************************************
DESCRIPTION: SoftGemini set param function, called by the following:
			-	config mgr in order to set a parameter receiving from the OS abstraction layer.
			-	From inside the driver	
                                                                                                   
INPUT:      hSoftGemini	-	SoftGemini handle.
			pParam	-	Pointer to the parameter		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS SoftGemini_setParam(TI_HANDLE	hSoftGemini,
											paramInfo_t	*pParam)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;
	TI_STATUS return_value = TI_OK;

TRACE1(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "  SoftGemini_setParam() (0x%x)\n", pParam->paramType);
	
	switch(pParam->paramType)
	{

	case SOFT_GEMINI_SET_ENABLE:
		
		return_value = SoftGemini_setEnableParam(hSoftGemini,pParam->content.SoftGeminiEnable, TI_FALSE);
		break;

	case SOFT_GEMINI_SET_CONFIG:

		/* copy new params to SoftGemini module */
		SoftGemini_setConfigParam(hSoftGemini,pParam->content.SoftGeminiParamArray);

		/* set new params to FW */
		return_value = SoftGemini_setParamsToFW(hSoftGemini, &(pSoftGemini->SoftGeminiParam));

		if (pSoftGemini->bProtectiveMode == TI_TRUE)
		{
			/* set new configurations of scan to scancncn */
			scanCncn_SGconfigureScanParams(pSoftGemini->hScanCncn,TI_TRUE,
										   (TI_UINT8)pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_AUTO_SCAN_PROBE_REQ],
										   pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_HV3_MAX_OVERRIDE],
										   pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_ACTIVE_SCAN_DURATION_FACTOR_HV3]);
		}
		break;
		
	default:
TRACE1(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, "  SoftGemini_setParam(), Params is not supported, %d\n\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

	return return_value;
}

/***********************************************************************
 *			      SoftGemini_getParam									
 ***********************************************************************
DESCRIPTION: SoftGemini get param function, called by the following:
			-	config mgr in order to get a parameter from the OS abstraction layer.
			-	From inside the dirver	
                                                                                                   
INPUT:      hSoftGemini	-	SoftGemini handle.
				

OUTPUT:		pParam	-	Pointer to the parameter	

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS SoftGemini_getParam(TI_HANDLE		hSoftGemini,
											paramInfo_t	*pParam)
{ 
		switch (pParam->paramType)
		{
			case SOFT_GEMINI_GET_CONFIG:
				SoftGemini_printParams(hSoftGemini);
				break;
		}

	return TI_OK;
}
 


/***************************************************************************
*					SoftGemini_setEnableParam					    	       *
****************************************************************************
* DESCRIPTION:	The function sets the  appropriate Enable value,
*				configures SCR , POWER MGR , DATA CTRL , FW.   
*
* INPUTS:		pSoftGemini - the object		
***************************************************************************/
static TI_STATUS SoftGemini_setEnableParam(TI_HANDLE hSoftGemini, ESoftGeminiEnableModes SoftGeminiEnable, TI_BOOL recovery)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;
	TTwdParamInfo	param;
	TI_STATUS return_value = TI_OK;

TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "  setSoftGeminiEnableParam() - Old value = , New value = \n");


    /*
     * PsPoll work around is active. Just save the value and configure it later
     */
    if ( pSoftGemini->bPsPollFailureActive )
    {
        TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "  setSoftGeminiEnableParam() - while PsPollFailure is active\n");

        pSoftGemini->PsPollFailureLastEnableValue = SoftGeminiEnable;
        return TI_OK;
    }

	/**********************************/
	/* Sanity check on enable values */
	/********************************/

	/*				Old Value						New Value		    */        
	/*					|							    |			    */        
	/*			  	   \|/							   \|/			    */        

	if ((pSoftGemini->SoftGeminiEnable == SoftGeminiEnable) && !recovery)
	{
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, "   - setting same value \n");
		return TI_NOK;
	}

	/*******************************/
	/* Make the necessary actions */
	/*****************************/
	
	switch (SoftGeminiEnable)
	{
	case SG_PROTECTIVE:
	case SG_OPPORTUNISTIC:
		
		/* set FW with SG_ENABLE */ 
		param.paramType = TWD_SG_ENABLE_PARAM_ID;
		param.content.SoftGeminiEnable = SoftGeminiEnable;
		return_value = TWD_SetParam (pSoftGemini->hTWD, &param);

		break;

	case SG_DISABLE:
		
		/* set FW with SG_DISABLE */
		param.paramType = TWD_SG_ENABLE_PARAM_ID;
		param.content.SoftGeminiEnable = SG_DISABLE;
		return_value = TWD_SetParam (pSoftGemini->hTWD, &param);

		if (pSoftGemini->bDriverEnabled)
		{	
			SoftGemini_DisableDriver(hSoftGemini);
		}
		
		break;

	default:
TRACE1(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, " defualt :%d\n",SoftGeminiEnable);
		return TI_NOK;
	}

	/* Pass to the new enable state */
	pSoftGemini->SoftGeminiEnable = SoftGeminiEnable;

	if (TI_OK != return_value)
	{
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, " can't configure enable param to FW :\n");
	}
	
	return return_value;
}

/***************************************************************************
*					SoftGemini_setConfigParam				    	       *
****************************************************************************
* DESCRIPTION:	The function sets params 
*
* INPUTS:		pSoftGemini - the object
*				param       - params to be configured	
***************************************************************************/
static void SoftGemini_setConfigParam(TI_HANDLE hSoftGemini, TI_UINT32 *param)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;

	/* param[0] - SG parameter index, param[1] - SG parameter value */
	pSoftGemini->SoftGeminiParam.coexParams[(TI_UINT8)param[0]] = (TI_UINT32)param[1];
	pSoftGemini->SoftGeminiParam.paramIdx = (TI_UINT8)param[0];
}

/***************************************************************************
*					SoftGemini_printParams					    	       *
****************************************************************************
* DESCRIPTION:	Print SG Parameters.  
*
* INPUTS:		pSoftGemini - the object	
***************************************************************************/
void SoftGemini_printParams(TI_HANDLE hSoftGemini)
{
#ifdef REPORT_LOG

	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;
	TSoftGeminiParams *SoftGeminiParam = &pSoftGemini->SoftGeminiParam;

	WLAN_OS_REPORT(("[0]:  coexBtPerThreshold = %d\n", SoftGeminiParam->coexParams[SOFT_GEMINI_BT_PER_THRESHOLD]));
	WLAN_OS_REPORT(("[1]:  coexHv3MaxOverride = %d \n", SoftGeminiParam->coexParams[SOFT_GEMINI_HV3_MAX_OVERRIDE])); 
	WLAN_OS_REPORT(("[2]:  coexBtNfsSampleInterval = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_BT_NFS_SAMPLE_INTERVAL]));
	WLAN_OS_REPORT(("[3]:  coexBtLoadRatio = %d (%)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_BT_LOAD_RATIO]));
	WLAN_OS_REPORT(("[4]:  coexAutoPsMode = %s \n", (SoftGeminiParam->coexParams[SOFT_GEMINI_AUTO_PS_MODE]?"Enabled":"Disabled")));
	WLAN_OS_REPORT(("[5]:  coexAutoScanEnlargedNumOfProbeReqPercent = %d (%)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_AUTO_SCAN_PROBE_REQ]));
	WLAN_OS_REPORT(("[6]:  coexHv3AutoScanEnlargedScanWinodowPercent = %d (%)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_ACTIVE_SCAN_DURATION_FACTOR_HV3]));
	WLAN_OS_REPORT(("[7]:  coexAntennaConfiguration = %s (0 = Single, 1 = Dual) \n", (SoftGeminiParam->coexParams[SOFT_GEMINI_ANTENNA_CONFIGURATION]?"Dual":"Single")));
	WLAN_OS_REPORT(("[8]:  coexMaxConsecutiveBeaconMissPrecent = %d (%)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_BEACON_MISS_PERCENT]));
	WLAN_OS_REPORT(("[9]:  coexAPRateAdapationThr = %d\n", SoftGeminiParam->coexParams[SOFT_GEMINI_RATE_ADAPT_THRESH]));
	WLAN_OS_REPORT(("[10]: coexAPRateAdapationSnr = %d\n", SoftGeminiParam->coexParams[SOFT_GEMINI_RATE_ADAPT_SNR]));
	WLAN_OS_REPORT(("[11]: coexWlanPsBtAclMasterMinBR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_BR]));
	WLAN_OS_REPORT(("[12]: coexWlanPsBtAclMasterMaxBR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_BR]));
	WLAN_OS_REPORT(("[13]: coexWlanPsMaxBtAclMasterBR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_BR]));
    WLAN_OS_REPORT(("[14]: coexWlanPsBtAclSlaveMinBR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_BR]));
    WLAN_OS_REPORT(("[15]: coexWlanPsBtAclSlaveMaxBR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_BR]));
    WLAN_OS_REPORT(("[16]: coexWlanPsMaxBtAclSlaveBR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_BR]));
    WLAN_OS_REPORT(("[17]: coexWlanPsBtAclMasterMinEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MIN_EDR]));
    WLAN_OS_REPORT(("[18]: coexWlanPsBtAclMasterMaxEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_MASTER_MAX_EDR]));
    WLAN_OS_REPORT(("[19]: coexWlanPsMaxBtAclMasterEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_MASTER_EDR]));
    WLAN_OS_REPORT(("[20]: coexWlanPsBtAclSlaveMinEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MIN_EDR]));
    WLAN_OS_REPORT(("[21]: coexWlanPsBtAclSlaveMaxEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_BT_ACL_SLAVE_MAX_EDR]));
    WLAN_OS_REPORT(("[22]: coexWlanPsMaxBtAclSlaveEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_PS_MAX_BT_ACL_SLAVE_EDR]));
	WLAN_OS_REPORT(("[23]: coexRxt = %d (usec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_RXT]));
	WLAN_OS_REPORT(("[24]: coexTxt = %d (usec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_TXT]));
	WLAN_OS_REPORT(("[25]: coexAdaptiveRxtTxt = %s \n", (SoftGeminiParam->coexParams[SOFT_GEMINI_ADAPTIVE_RXT_TXT]?"Enabled":"Disabled")));
	WLAN_OS_REPORT(("[26]: coexPsPollTimeout = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_PS_POLL_TIMEOUT]));
	WLAN_OS_REPORT(("[27]: coexUpsdTimeout = %d (msec) \n", SoftGeminiParam->coexParams[SOFT_GEMINI_UPSD_TIMEOUT]));
	WLAN_OS_REPORT(("[28]: coexWlanActiveBtAclMasterMinEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MIN_EDR]));
	WLAN_OS_REPORT(("[29]: coexWlanActiveBtAclMasterMaxEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MASTER_MAX_EDR]));
	WLAN_OS_REPORT(("[30]: coexWlanActiveMaxBtAclMasterEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_MASTER_EDR]));
    WLAN_OS_REPORT(("[31]: coexWlanActiveBtAclSlaveMinEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MIN_EDR]));
	WLAN_OS_REPORT(("[32]: coexWlanActiveBtAclSlaveMaxEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_SLAVE_MAX_EDR]));
    WLAN_OS_REPORT(("[33]: coexWlanActiveMaxBtAclSlaveEDR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_SLAVE_EDR]));
    WLAN_OS_REPORT(("[34]: coexWlanActiveBtAclMinBR = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MIN_BR]));
    WLAN_OS_REPORT(("[35]: coexWlanActiveBtAclMinBr = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_BT_ACL_MAX_BR]));
    WLAN_OS_REPORT(("[36]: coexWlanActiveMaxBtAclBr = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_WLAN_ACTIVE_MAX_BT_ACL_BR]));
    WLAN_OS_REPORT(("[37]: coexHv3AutoEnlargePassiveScanWindowPercent = %d (%)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_HV3]));
    WLAN_OS_REPORT(("[38]: coexA2DPAutoEnlargePassiveScanWindowPercent = %d (%) \n", SoftGeminiParam->coexParams[SOFT_GEMINI_PASSIVE_SCAN_DURATION_FACTOR_A2DP]));
    WLAN_OS_REPORT(("[39]: coexPassiveScanA2dpBtTime  = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_PASSIVE_SCAN_A2DP_BT_TIME]));
    WLAN_OS_REPORT(("[40]: coexPassiveScanA2dpWlanTime = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_PASSIVE_SCAN_A2DP_WLAN_TIME]));
	WLAN_OS_REPORT(("[41]: CoexHv3MaxServed = %d \n", SoftGeminiParam->coexParams[SOFT_GEMINI_HV3_MAX_SERVED]));
	WLAN_OS_REPORT(("[42]: coexDhcpTime = %d (msec)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_DHCP_TIME]));
	WLAN_OS_REPORT(("[43]: coexA2dpAutoScanEnlargedScanWinodowPercent = %d (%)\n", SoftGeminiParam->coexParams[SOFT_GEMINI_ACTIVE_SCAN_DURATION_FACTOR_A2DP]));
	WLAN_OS_REPORT(("[44]: coexTempParam1 = %d \n", SoftGeminiParam->coexParams[SOFT_GEMINI_TEMP_PARAM_1]));
	WLAN_OS_REPORT(("[45]: coexTempParam2 = %d \n", SoftGeminiParam->coexParams[SOFT_GEMINI_TEMP_PARAM_2]));
	WLAN_OS_REPORT(("[46]: coexTempParam3 = %d \n", SoftGeminiParam->coexParams[SOFT_GEMINI_TEMP_PARAM_3]));
	WLAN_OS_REPORT(("[47]: coexTempParam4 = %d \n", SoftGeminiParam->coexParams[SOFT_GEMINI_TEMP_PARAM_4]));
	WLAN_OS_REPORT(("[48]: coexTempParam5 = %d \n", SoftGeminiParam->coexParams[SOFT_GEMINI_TEMP_PARAM_5]));
	WLAN_OS_REPORT(("Enable mode : %s\n", SoftGemini_ConvertModeToString(pSoftGemini->SoftGeminiEnable))); 
	WLAN_OS_REPORT(("Driver Enabled : %s\n",(pSoftGemini->bDriverEnabled ? "YES" : "NO"))); 
	WLAN_OS_REPORT(("Protective mode : %s\n", (pSoftGemini->bProtectiveMode ? "ON" : "OFF"))); 
    WLAN_OS_REPORT(("PsPoll failure active : %s\n", (pSoftGemini->bPsPollFailureActive ? "YES" : "NO"))); 

#endif
}

/***************************************************************************
*					SoftGemini_setParamsToFW					    	       *
****************************************************************************
* DESCRIPTION:	The function sets the FW with the appropriate parameters set.  
*
* INPUTS:		pSoftGemini - the object
*
*
* OUTPUT:			
* 
* RETURNS:		
***************************************************************************/
static TI_STATUS SoftGemini_setParamsToFW(TI_HANDLE hSoftGemini, TSoftGeminiParams *softGeminiParams)
{
	SoftGemini_t *pSoftGemini = (SoftGemini_t *)hSoftGemini;
	TTwdParamInfo param;

	os_memoryCopy(pSoftGemini->hOs,&param.content.SoftGeminiParam, softGeminiParams, sizeof(TSoftGeminiParams));
	param.paramType = TWD_SG_CONFIG_PARAM_ID;
	return TWD_SetParam (pSoftGemini->hTWD, &param);
}


/***************************************************************************
*					SoftGemini_EnableDriver  		    	       *
****************************************************************************
* DESCRIPTION:	Activated when SG is enabled (after CLI or FW command)
*
* INPUTS:		pSoftGemini - the object
*	
***************************************************************************/
static TI_STATUS SoftGemini_EnableDriver(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;
	TI_STATUS return_value = TI_OK;

TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "\n");

	pSoftGemini->bDriverEnabled = TI_TRUE;

	/* Check if coexAutoPsMode - Co-ex is enabled to enter/exit P.S */
	if ( pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_AUTO_PS_MODE])
	{
		SoftGemini_SetPS(pSoftGemini);
	}

	scr_setMode(pSoftGemini->hSCR, SCR_MID_SG);

	return return_value;
}

/***************************************************************************
*					SoftGemini_DisableDriver  		    	       *
****************************************************************************
* DESCRIPTION:	Activated when SG is disabled (after CLI or FW command)
*
* INPUTS:		pSoftGemini - the object
*	
***************************************************************************/
static TI_STATUS SoftGemini_DisableDriver(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;
	TI_STATUS return_value = TI_OK;

TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "\n");

	pSoftGemini->bDriverEnabled = TI_FALSE;

	scr_setMode(pSoftGemini->hSCR, SCR_MID_NORMAL);


	/* Check if coexAutoPsMode - Co-ex is enabled to enter/exit P.S */
	if ( pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_AUTO_PS_MODE])
	{
		SoftGemini_unSetPS(pSoftGemini);
	}

	/* Undo the changes that were made when Protective mode was on */
	if (pSoftGemini->bProtectiveMode)
	{
		SoftGemini_DisableProtectiveMode(hSoftGemini);
	}
	
	return return_value;
}

/***************************************************************************
*					SoftGemini_SetPS  		    						   *
****************************************************************************
* DESCRIPTION:	Set Always PS to PowerMgr
*
* INPUTS:		pSoftGemini - the object
*	
***************************************************************************/
static TI_STATUS SoftGemini_SetPS(SoftGemini_t	*pSoftGemini)
{
	paramInfo_t param;
	bssEntry_t *pBssInfo=NULL;

    if (pSoftGemini->hCurrBss)
	{
		pBssInfo = currBSS_getBssInfo(pSoftGemini->hCurrBss);
	}
    else
    {
		TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_ERROR, "SoftGemini_SetPS: hCurrBss = NULL!!!\n");
    }

    TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "\n");

	if (pBssInfo)
	{
		if ((pBssInfo->band == RADIO_BAND_2_4_GHZ))
		{
            TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, " SG-setPS: band == RADIO_BAND_2_4_GHZ");

	        /* Set Params to Power Mgr for SG priority */
	        param.paramType = POWER_MGR_POWER_MODE;
	        param.content.powerMngPowerMode.PowerMode = POWER_MODE_PS_ONLY;
	        param.content.powerMngPowerMode.PowerMngPriority = POWER_MANAGER_SG_PRIORITY;
	        powerMgr_setParam(pSoftGemini->hPowerMgr,&param);

	        /* enable SG priority for Power Mgr */
	        param.paramType = POWER_MGR_ENABLE_PRIORITY;
	        param.content.powerMngPriority = POWER_MANAGER_SG_PRIORITY;
	        return powerMgr_setParam(pSoftGemini->hPowerMgr,&param);
        }
        else
        {
            TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, " SG-setPS: band == RADIO_BAND_5_GHZ");
        }
	}
	return TI_OK;
}

/***************************************************************************
*					SoftGemini_unSetPS  		    						   *
****************************************************************************
* DESCRIPTION:	unSet Always PS to PowerMgr
*
* INPUTS:		pSoftGemini - the object
*	
***************************************************************************/
static TI_STATUS SoftGemini_unSetPS(SoftGemini_t	*pSoftGemini)
{
	paramInfo_t param;

TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, ", SG-unSetPS \n");

	/* disable SG priority for Power Mgr*/
	param.paramType = POWER_MGR_DISABLE_PRIORITY;
	param.content.powerMngPriority = POWER_MANAGER_SG_PRIORITY;
	return powerMgr_setParam(pSoftGemini->hPowerMgr,&param);
	
}

/***************************************************************************
*					SoftGemini_EnableProtectiveMode  		    	       *
****************************************************************************
* DESCRIPTION:	Activated when FW inform us that protective mode is ON
*				
*
* INPUTS:		pSoftGemini - the object
*	
***************************************************************************/
void SoftGemini_EnableProtectiveMode(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;
	paramInfo_t 	param;

	pSoftGemini->bProtectiveMode = TI_TRUE;

	/* set new configurations of SG roaming parameters */

	/* This code should be removed on SG stage 2 integration
	 currBSS_SGconfigureBSSLoss(pSoftGemini->hCurrBss,pSoftGemini->BSSLossCompensationPercent,TI_TRUE); */

	/* set new configurations of scan to scancncn */
    scanCncn_SGconfigureScanParams(pSoftGemini->hScanCncn,TI_TRUE,
								   (TI_UINT8)pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_AUTO_SCAN_PROBE_REQ],
								   pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_HV3_MAX_OVERRIDE], 
								   pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_ACTIVE_SCAN_DURATION_FACTOR_HV3]);

    /* Call the power manager to enter short doze */
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, " SoftGemini_EnableProtectiveMode set SD");

	/* Set Params to Power Mgr for SG priority */
	param.paramType = POWER_MGR_POWER_MODE;
	param.content.powerMngPowerMode.PowerMode = POWER_MODE_SHORT_DOZE;
	param.content.powerMngPowerMode.PowerMngPriority = POWER_MANAGER_SG_PRIORITY;
	powerMgr_setParam(pSoftGemini->hPowerMgr,&param);
}

/***************************************************************************
*					SoftGemini_DisableProtectiveMode  		    	       *
****************************************************************************
* DESCRIPTION:	Activated when FW inform us that protective mode is OFF or SG is disabled
*
* INPUTS:		pSoftGemini - the object
*	
***************************************************************************/
void SoftGemini_DisableProtectiveMode(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;

TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "\n");

	pSoftGemini->bProtectiveMode = TI_FALSE;

	SoftGemini_RemoveProtectiveModeParameters(hSoftGemini);
}

/***************************************************************************
*					SoftGemini_DisableProtectiveMode  		    	       *
****************************************************************************
* DESCRIPTION:	Called from SoftGemini_DisableProtectiveMode() when FW inform 
*				us that protective mode is OFF or SG is disabled, or from
*				SoftGemini_unSetPSmode() when driver disconnects from AP.
*
* INPUTS:		pSoftGemini - the object
*	
***************************************************************************/

void SoftGemini_RemoveProtectiveModeParameters(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;
	paramInfo_t  	param;

TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "\n");

	/* don't use the SG roaming parameters */
	currBSS_SGconfigureBSSLoss(pSoftGemini->hCurrBss,0,TI_FALSE);

	/* don't use the SG scan parameters */
    scanCncn_SGconfigureScanParams(pSoftGemini->hScanCncn,TI_FALSE,0,0,0);

    /* Call the power manager to exit short doze */
	/* Set Params to Power Mgr for SG priority */
	param.paramType = POWER_MGR_POWER_MODE;
	param.content.powerMngPowerMode.PowerMode = POWER_MODE_PS_ONLY;
	param.content.powerMngPowerMode.PowerMngPriority = POWER_MANAGER_SG_PRIORITY;
	powerMgr_setParam(pSoftGemini->hPowerMgr,&param);
}

/***************************************************************************
*					SoftGemini_SenseIndicationCB  		    	       *
****************************************************************************
* DESCRIPTION:	This is the the function which is called for sense mode indication from FW
*				(i.e. we are in SENSE mode and FW detects BT activity )
*				SENSE_MODE_ENABLE - Indicates that FW detected BT activity
*				SENSE_MODE_DISABLE - Indicates that FW doesn't detect BT activity for a period of time
*
* INPUTS:		pSoftGemini - the object
* NOTE			This function is located in the API for debug purposes
***************************************************************************/

void SoftGemini_SenseIndicationCB( TI_HANDLE hSoftGemini, char* str, TI_UINT32 strLen )
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;

	if (pSoftGemini->SoftGeminiEnable == SG_DISABLE) {
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_WARNING, ": SG is disabled, existing");
		return;
	}

	if ( (SENSE_MODE_ENABLE == *str) && (!pSoftGemini->bDriverEnabled) )
	{
			SoftGemini_EnableDriver(hSoftGemini);
	}
	else if ( (SENSE_MODE_DISABLE == *str) && (pSoftGemini->bDriverEnabled) )
	{
			SoftGemini_DisableDriver(hSoftGemini);
	}
}

/***************************************************************************
*					SoftGemini_ProtectiveIndicationCB  		    	       *
****************************************************************************
* DESCRIPTION:	This is the the function which is called when FW starts Protective mode (i.e BT voice) 
*
*				PROTECTIVE_MODE_ON - FW is activated on protective mode (BT voice is running)
*				PROTECTIVE_MODE_OFF - FW is not activated on protective mode
*
* INPUTS:		pSoftGemini - the object
* NOTE			This function is located in the API for debug purposes
***************************************************************************/

void SoftGemini_ProtectiveIndicationCB( TI_HANDLE hSoftGemini, char* str, TI_UINT32 strLen )
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;
	
TRACE1(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, " with 0x%x\n",*str);

	if (SG_DISABLE != pSoftGemini->SoftGeminiEnable)
	{
		if ((!pSoftGemini->bProtectiveMode) && (PROTECTIVE_MODE_ON == *str))
		{
			SoftGemini_EnableProtectiveMode(hSoftGemini);
		}
		else if ((pSoftGemini->bProtectiveMode) && (PROTECTIVE_MODE_OFF == *str))
		{
			SoftGemini_DisableProtectiveMode(hSoftGemini);
		}
		else
		{
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, " : Protective mode  called when Protective mode is  \n");
		}
	}
	else
	{
TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_WARNING, " : Protective mode  called when SG mode is  ? \n");
	}
}

/***************************************************************************
*					SoftGemini_ConvertModeToString  		    	       *
****************************************************************************/
#ifdef REPORT_LOG

char* SoftGemini_ConvertModeToString(ESoftGeminiEnableModes SoftGeminiEnable)
{
	switch(SoftGeminiEnable)
	{
	case SG_PROTECTIVE:				return "SG_PROTECTIVE";
	case SG_DISABLE:			    return "SG_DISABLE";
	case SG_OPPORTUNISTIC:     return "SG_OPPORTUNISTIC";
	default:
		return "ERROR";
	}
}

#endif

/***************************************************************************
*					SoftGemini_getSGMode						  		    	       *
****************************************************************************/
ESoftGeminiEnableModes SoftGemini_getSGMode(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;
	return pSoftGemini->SoftGeminiEnable;	
}

/***************************************************************************
*					SoftGemini_handleRecovery					    	       *
****************************************************************************
* DESCRIPTION:	The function reconfigures WHAL with the SG parameters.
*
* INPUTS:		pSoftGemini - the object		
***************************************************************************/
TI_STATUS SoftGemini_handleRecovery(TI_HANDLE hSoftGemini)
{
	SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;
	ESoftGeminiEnableModes       realSoftGeminiEnableMode;

	realSoftGeminiEnableMode = pSoftGemini->SoftGeminiEnable;
    /* Disable the SG */
    SoftGemini_setEnableParam(hSoftGemini, SG_DISABLE, TI_TRUE);
    TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "Disable SG \n");

	pSoftGemini->SoftGeminiEnable = realSoftGeminiEnableMode;
	/* Set enable param */

	SoftGemini_setEnableParam(hSoftGemini, pSoftGemini->SoftGeminiEnable, TI_TRUE);
    TRACE1(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "Set SG to-%d\n", pSoftGemini->SoftGeminiEnable);

	/* Config the params to FW */

	SoftGemini_setParamsToFW(hSoftGemini, &pSoftGemini->SoftGeminiParam);
	/*SoftGemini_printParams(hSoftGemini);*/
	return TI_OK;
}
/***************************************************************************
*					SoftGemini_startPsPollFailure					       *
****************************************************************************
* DESCRIPTION:	After Ps-Poll failure we disable the SG
*
* INPUTS:		pSoftGemini - the object		
***************************************************************************/
void SoftGemini_startPsPollFailure(TI_HANDLE hSoftGemini)
	{
    SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;

    TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "\n");

    if ( (!pSoftGemini->bPsPollFailureActive) && (pSoftGemini->SoftGeminiParam.coexParams[SOFT_GEMINI_AUTO_PS_MODE] == TI_TRUE) )
    {
        pSoftGemini->PsPollFailureLastEnableValue = pSoftGemini->SoftGeminiEnable;

        /* Disable SG if needed */
        if ( pSoftGemini->SoftGeminiEnable != SG_DISABLE )
        {	
            SoftGemini_setEnableParam(hSoftGemini, SG_DISABLE, TI_FALSE);
	}

        pSoftGemini->bPsPollFailureActive = TI_TRUE;
    }
    else /* Calling SoftGemini_startPsPollFailure twice ? */
	{
        TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_WARNING, "Calling  SoftGemini_startPsPollFailure while bPsPollFailureActive is TRUE\n");
    }
	}

/***************************************************************************
*					SoftGemini_endPsPollFailure					    	   *
****************************************************************************
* DESCRIPTION:	Return to normal behavior after the PsPoll failure 
*
* INPUTS:		pSoftGemini - the object		
***************************************************************************/
void SoftGemini_endPsPollFailure(TI_HANDLE hSoftGemini)
{
    SoftGemini_t	*pSoftGemini = (SoftGemini_t *)hSoftGemini;

    TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_INFORMATION, "\n");

    if ( pSoftGemini->bPsPollFailureActive ) 
    {
        pSoftGemini->bPsPollFailureActive = TI_FALSE;

        /* return to previous value */
        if ( pSoftGemini->PsPollFailureLastEnableValue != SG_DISABLE )
        {
			SoftGemini_setEnableParam(hSoftGemini, pSoftGemini->PsPollFailureLastEnableValue, TI_FALSE);
        }
    }
    else /* Calling SoftGemini_endPsPollFailure twice ? */
    {
        TRACE0(pSoftGemini->hReport, REPORT_SEVERITY_WARNING, "Calling  SoftGemini_endPsPollFailure while bPsPollFailureActive is FALSE\n");
    }
}


