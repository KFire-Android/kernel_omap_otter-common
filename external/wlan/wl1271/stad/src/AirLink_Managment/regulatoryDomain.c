/*
 * regulatoryDomain.c
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

/** \file regulatoryDomain.c
 *  \brief regulatoryDomain module interface
 *
 *  \see regulatoryDomain.h
 */

/************************************************************************************************/
/*  												*/
/*		MODULE:		regulatoryDomain.c					        */
/*		PURPOSE:	regulatoryDomain module interface.			        */
/*                  This module calculated the channel that should be scanned and that are      */
/*                   supported. Moreover, he set the transmit power level according to the      */
/*                   regulatory domain requirements and the supported channel.                  */
/*								 			        */
/************************************************************************************************/
#define __FILE_ID__  FILE_ID_3
#include "report.h"
#include "osApi.h"
#include "paramOut.h"
#include "regulatoryDomain.h"
#include "regulatoryDomainApi.h"
#include "TWDriver.h"
#include "siteMgrApi.h"
#include "SwitchChannelApi.h"
#include "DrvMainModules.h"
#include "TWDriver.h"


/* Mask for retrieving the TxPower from the Scan Control Table */
#define MASK_TX_POWER					(0x1f) /* bits 0-4 indicates MaxTxPower */ 
#define MASK_ACTIVE_ALLOWED 			(0x40) /* bit 6 indiactes the channel is allowed for Active scan */
#define MASK_FREQ_ALLOWED 				(0x80) /* bit 7 indicates the cahnnel is allowed*/

#define CHANNEL_VALIDITY_TS_THRESHOLD   10000 /* 10 sec */

/* 
* Small macro to convert Dbm units into Dbm/10 units. This macro is important
* in order to avoid over-flow of Dbm units bigger than 25
*/
#define DBM2DBMDIV10(uTxPower) \
	((uTxPower) > (MAX_TX_POWER / DBM_TO_TX_POWER_FACTOR) ? \
		MAX_TX_POWER : (uTxPower) * DBM_TO_TX_POWER_FACTOR)		


/********************************************************************************/
/*						Internal functions prototypes.							*/
/********************************************************************************/
static TI_STATUS regulatoryDomain_updateCurrTxPower(regulatoryDomain_t	*pRegulatoryDomain);

static void regulatoryDomain_setChannelValidity(regulatoryDomain_t *pRegulatoryDomain, 
												TI_UINT16 channelNum, TI_BOOL channelValidity);

static TI_STATUS setSupportedChannelsAccording2CountryIe(regulatoryDomain_t *pRegulatoryDomain, TCountry*	pCountry, TI_BOOL band_2_4);

static void setSupportedChannelsAccording2ScanControlTable(regulatoryDomain_t  *pRegulatoryDomain);

static TI_STATUS regulatoryDomain_getChannelCapability(regulatoryDomain_t *pRegulatoryDomain, 
													   channelCapabilityReq_t channelCapabilityReq, 
													   channelCapabilityRet_t *channelCapabilityRet);

static void regulatoryDomain_updateChannelsTs(regulatoryDomain_t *pRegulatoryDomain, TI_UINT8 channel);

static void regulatoryDomain_buildDefaultListOfChannelsPerBand(regulatoryDomain_t *pRegulatoryDomain, ERadioBand band, TI_UINT8 *listSize);

static void regulatoryDomain_checkCountryCodeExpiry(regulatoryDomain_t *pRegulatoryDomain);

static TI_BOOL regulatoryDomain_isChannelSupprted(regulatoryDomain_t *pRegulatoryDomain, TI_UINT8 channel);

static TI_BOOL regulatoryDomain_isCountryFound(regulatoryDomain_t *pRegulatoryDomain, ERadioBand radioBand);

static void regulatoryDomain_getPowerTableMinMax (regulatoryDomain_t *pRegulatoryDomain,
                                                  powerCapability_t  *pPowerCapability);

static TI_UINT8 regulatoryDomain_getMaxPowerAllowed(regulatoryDomain_t	*pRegulatoryDomain,
												 TI_UINT8				uChannel,
												 ERadioBand		eBand,
												 TI_BOOL				bServingChannel);

/********************************************************************************/
/*						Interface functions Implementation.						*/
/********************************************************************************/


/************************************************************************
 *                        regulatoryDomain_create									*
 ************************************************************************
DESCRIPTION: regulatoryDomain module creation function, called by the config mgr in creation phase 
				performs the following:
				-	Allocate the regulatoryDomain handle
				                                                                                                   
INPUT:      hOs -			Handle to OS		


OUTPUT:		

RETURN:     Handle to the regulatoryDomain module on success, NULL otherwise

************************************************************************/
TI_HANDLE regulatoryDomain_create(TI_HANDLE hOs)
{
	regulatoryDomain_t			*pRegulatoryDomain = NULL;
	
	/* allocating the regulatoryDomain object */
	pRegulatoryDomain = os_memoryAlloc(hOs,sizeof(regulatoryDomain_t));

	if (pRegulatoryDomain == NULL)
		return NULL;

	return(pRegulatoryDomain);
}


/************************************************************************
 *                        regulatoryDomain_init							*
 ************************************************************************
DESCRIPTION: Module init function, Called by the DrvMain in init phase
				performs the following:
				-	Reset & initializes local variables
				-	Init the handles to be used by the module
                                                                                                   
INPUT:      pStadHandles - List of handles to be used by the module

OUTPUT:		

RETURN:     void
************************************************************************/
void regulatoryDomain_init (TStadHandlesList *pStadHandles)
{
	regulatoryDomain_t *pRegulatoryDomain = (regulatoryDomain_t *)(pStadHandles->hRegulatoryDomain);

	/* init variables */
	pRegulatoryDomain->country_2_4_WasFound		= TI_FALSE;
	pRegulatoryDomain->country_5_WasFound		= TI_FALSE;
	pRegulatoryDomain->uExternTxPowerPreferred	= MAX_TX_POWER;	/* i.e. no restriction */
	pRegulatoryDomain->uPowerConstraint			= MIN_TX_POWER;	/* i.e. no restriction */
	 
	/* Init handlers */
	pRegulatoryDomain->hSiteMgr       = pStadHandles->hSiteMgr;
	pRegulatoryDomain->hTWD	          = pStadHandles->hTWD;
	pRegulatoryDomain->hReport	      = pStadHandles->hReport;
	pRegulatoryDomain->hOs	          = pStadHandles->hOs;
    pRegulatoryDomain->hSwitchChannel = pStadHandles->hSwitchChannel;
}


/************************************************************************
 *                        regulatoryDomain_SetDefaults						*
 ************************************************************************
DESCRIPTION: regulatoryDomain module configuration function, called by the config mgr in configuration phase
				performs the following:
				-	Reset & initializes local variables
				-	Init the handles to be used by the module
                                                                                                   
INPUT:      hRegulatoryDomain	-	regulatoryDomain handle
			List of handles to be used by the module
			pRegulatoryDomainInitParams	-	Init table of the module.		


OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS regulatoryDomain_SetDefaults (TI_HANDLE 	hRegulatoryDomain,
                                        regulatoryDomainInitParams_t *pRegulatoryDomainInitParams)
{
	regulatoryDomain_t *pRegulatoryDomain = (regulatoryDomain_t *)hRegulatoryDomain;

	/* User max Tx power for all channels */
	pRegulatoryDomain->uUserMaxTxPower	  = pRegulatoryDomainInitParams->desiredTxPower; 
	/* Temporary Tx Power control to be used */
    pRegulatoryDomain->uTemporaryTxPower  = pRegulatoryDomainInitParams->uTemporaryTxPower;
    pRegulatoryDomain->uDesiredTemporaryTxPower = pRegulatoryDomainInitParams->uTemporaryTxPower;

    /* 
	 * Indicate the time in which the STA didn't receive any country code and was not connected, and therefore
     * will delete its current country code 
	 */
    pRegulatoryDomain->uTimeOutToResetCountryMs = pRegulatoryDomainInitParams->uTimeOutToResetCountryMs;
	pRegulatoryDomain->uLastCountryReceivedTS = 0;

	pRegulatoryDomain->regulatoryDomainEnabled = pRegulatoryDomainInitParams->multiRegulatoryDomainEnabled;
	pRegulatoryDomain->spectrumManagementEnabled = pRegulatoryDomainInitParams->spectrumManagementEnabled;
	if (pRegulatoryDomain->spectrumManagementEnabled == TI_TRUE)
	{
		pRegulatoryDomain->regulatoryDomainEnabled = TI_TRUE;
	}
		
	/* Getting the desired Control Table contents for 2.4 Ghz*/
	os_memoryCopy(pRegulatoryDomain->hOs,
				  (void *)pRegulatoryDomain->scanControlTable.ScanControlTable24.tableString,
				  (void *)pRegulatoryDomainInitParams->desiredScanControlTable.ScanControlTable24.tableString,
					NUM_OF_CHANNELS_24 * sizeof(TI_INT8));

	/* Getting the desired Control Table contents for 5 Ghz*/
	os_memoryCopy(pRegulatoryDomain->hOs,
				  (void *)pRegulatoryDomain->scanControlTable.ScanControlTable5.tableString,
				  (void *)pRegulatoryDomainInitParams->desiredScanControlTable.ScanControlTable5.tableString,
					A_5G_BAND_NUM_CHANNELS * sizeof(TI_INT8));

	setSupportedChannelsAccording2ScanControlTable(pRegulatoryDomain);

    pRegulatoryDomain->minDFS_channelNum = A_5G_BAND_MIN_MIDDLE_BAND_DFS_CHANNEL;
    pRegulatoryDomain->maxDFS_channelNum = A_5G_BAND_MAX_UPPER_BAND_DFS_CHANNEL;

TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_INIT, ".....Regulatory domain configured successfully\n");

	return TI_OK;
}

TI_STATUS regulatoryDomain_setParam(TI_HANDLE hRegulatoryDomain,
									paramInfo_t	*pParam)
{
	regulatoryDomain_t *pRegulatoryDomain = (regulatoryDomain_t *)hRegulatoryDomain;
    
			
	switch(pParam->paramType)
	{
    case REGULATORY_DOMAIN_COUNTRY_PARAM:
        {
            TI_BOOL        bBand_2_4;

            /* Sanity check */
            if (NULL == pParam->content.pCountry)
            {   
                TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR, "regulatoryDomain_setParam, REGULATORY_DOMAIN_COUNTRY_PARAM is set with NULL pointer");

                return TI_NOK;
            }
            else /* Update country code and supported channels */
            {         
                bBand_2_4 = siteMgr_isCurrentBand24(pRegulatoryDomain->hSiteMgr);

			    /* Setting the CountryIE for every Band */
			    setSupportedChannelsAccording2CountryIe(pRegulatoryDomain, pParam->content.pCountry, bBand_2_4);
            }
        }
		break;

	case REGULATORY_DOMAIN_SET_POWER_CONSTRAINT_PARAM:

        /* Update only if 11h enabled */
        if (pRegulatoryDomain->spectrumManagementEnabled)
		{	
            /* Convert to RegDomain units */
            TI_UINT8 uNewPowerConstraint = DBM2DBMDIV10(pParam->content.powerConstraint);

TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "SET_POWER_CONSTRAINT Old= %d New = %d (Only if bigger...)\n", 							  pRegulatoryDomain->uPowerConstraint, uNewPowerConstraint);

			/* Update powerConstraint */
			if ( pRegulatoryDomain->uPowerConstraint != uNewPowerConstraint )
			{
				pRegulatoryDomain->uPowerConstraint = uNewPowerConstraint;
				/* Set new Tx power to TWD - only if needed ! */
				regulatoryDomain_updateCurrTxPower(pRegulatoryDomain);
			}
        }
		break;	
		
	case REGULATORY_DOMAIN_EXTERN_TX_POWER_PREFERRED:
		/* ExternTxPowerPreferred is the TX Power Control (TPC) */
		{
			/* Convert to RegDomain units */
			TI_UINT8 uNewTPC = DBM2DBMDIV10(pParam->content.ExternTxPowerPreferred);

TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "REGULATORY_DOMAIN_EXTERN_TX_POWER_PREFERRED Old= %d New = %d\n", 				pRegulatoryDomain->uExternTxPowerPreferred, uNewTPC);

			if ( uNewTPC != pRegulatoryDomain->uExternTxPowerPreferred )
			{
				pRegulatoryDomain->uExternTxPowerPreferred = uNewTPC;
				/* Set new Tx power to TWD - only if needed ! */
				regulatoryDomain_updateCurrTxPower(pRegulatoryDomain);
			}
		}
		break;	
	
	case REGULATORY_DOMAIN_SET_CHANNEL_VALIDITY:
		/* Set channel as Valid or Invalid for Active SCAN only.
			Mainly used by DFS when Switch Channel is active */
		regulatoryDomain_setChannelValidity(pRegulatoryDomain, pParam->content.channelValidity.channelNum, 
															   pParam->content.channelValidity.channelValidity);
		break;
	
	case REGULATORY_DOMAIN_CURRENT_TX_POWER_IN_DBM_PARAM:
		/* This case is called when the desired Tx Power Level in Dbm is changed by the user */
        if(pRegulatoryDomain->uUserMaxTxPower != pParam->content.desiredTxPower)
        {
            pRegulatoryDomain->uUserMaxTxPower = pParam->content.desiredTxPower;			
			/* Set new Tx power to TWD - only if needed ! */
			regulatoryDomain_updateCurrTxPower(pRegulatoryDomain);
        }

		break;

	case REGULATORY_DOMAIN_TX_POWER_AFTER_SELECTION_PARAM:
		/* Called after joining BSS, set Tx power to TWD */

        TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setParam, REGULATORY_DOMAIN_TX_POWER_AFTER_SELECTION_PARAM \n");

	   /* setting the Tx Power according to the selected channel */
        regulatoryDomain_updateCurrTxPower(pRegulatoryDomain);
        
		break;

    case REGULATORY_DOMAIN_DISCONNECT_PARAM:
        TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setParam, REGULATORY_DOMAIN_DISCONNECT_PARAM\n");

        pRegulatoryDomain->uExternTxPowerPreferred = MAX_TX_POWER;	/* i.e. no restriction */
        pRegulatoryDomain->uPowerConstraint		   = MIN_TX_POWER;	/* i.e. no restriction */

        /* Update the last time a country code was used. 
        After uTimeOutToResetCountryMs the country code will be deleted     */
        if (pRegulatoryDomain->country_2_4_WasFound || pRegulatoryDomain->country_5_WasFound)
        {
            pRegulatoryDomain->uLastCountryReceivedTS = os_timeStampMs(pRegulatoryDomain->hOs);
        }
        break;

	case REGULATORY_DOMAIN_UPDATE_CHANNEL_VALIDITY:
		regulatoryDomain_updateChannelsTs(pRegulatoryDomain, pParam->content.channel);
		break;

    case REGULATORY_DOMAIN_TEMPORARY_TX_ATTENUATION_PARAM:
		/* Temporary Tx Power control */
TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setParam: temporary fix = %d, \n", pParam->content.bActivateTempPowerFix);

        pRegulatoryDomain->bTemporaryTxPowerEnable = pParam->content.bActivateTempPowerFix;

			regulatoryDomain_updateCurrTxPower(pRegulatoryDomain);

        break;

    case REGULATORY_DOMAIN_ENABLE_DISABLE_802_11D:
        TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setParam, REGULATORY_DOMAIN_ENABLE_DISABLE_802_11D = %d, \n", pParam->content.enableDisable_802_11d);

        if ((pRegulatoryDomain->regulatoryDomainEnabled != pParam->content.enableDisable_802_11d) &&
            !pParam->content.enableDisable_802_11d && pRegulatoryDomain->spectrumManagementEnabled)
        {   /* Disable of 802_11d, is not allowed when 802_11h is enabled */
            TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR, "regulatoryDomain_setParam, Disable of 802_11d, is not allowed when 802_11h is enabled  \n");
            return TI_NOK;
            
        }
        pRegulatoryDomain->regulatoryDomainEnabled = pParam->content.enableDisable_802_11d;

		/* Mark that no country was found - applies for both enabling and disabling of 11d */
		pRegulatoryDomain->country_2_4_WasFound = TI_FALSE;
		pRegulatoryDomain->country_5_WasFound = TI_FALSE;

        if (!pRegulatoryDomain->regulatoryDomainEnabled)
        {   /* Set regulatory Domain according to scan control table */
            setSupportedChannelsAccording2ScanControlTable(pRegulatoryDomain);
        }

		break;

    case REGULATORY_DOMAIN_ENABLE_DISABLE_802_11H:
        TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setParam, REGULATORY_DOMAIN_ENABLE_DISABLE_802_11H = %d, \n", pParam->content.enableDisable_802_11h);

        pRegulatoryDomain->spectrumManagementEnabled = pParam->content.enableDisable_802_11h;
        if (pParam->content.enableDisable_802_11h)
        {   /* If 802_11h is enabled, enable 802_11d as well */
            pRegulatoryDomain->regulatoryDomainEnabled = TI_TRUE;
        }
        switchChannel_enableDisableSpectrumMngmt(pRegulatoryDomain->hSwitchChannel, pRegulatoryDomain->spectrumManagementEnabled);
		break;

	case REGULATORY_DOMAIN_COUNTRY_2_4_PARAM:
        /* NOTE !!! use this feature carefully. */
        TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setParam, REGULATORY_DOMAIN_COUNTRY_2_4_PARAM Len = %d, \n", pParam->paramLength);

        TRACE_INFO_HEX(pRegulatoryDomain->hReport, (TI_UINT8*)pParam->content.pCountry, sizeof(TCountry));

        return setSupportedChannelsAccording2CountryIe(pRegulatoryDomain, pParam->content.pCountry, TI_TRUE);

	case REGULATORY_DOMAIN_COUNTRY_5_PARAM:
        /* NOTE !!! use this feature carefully */
        return setSupportedChannelsAccording2CountryIe(pRegulatoryDomain, pParam->content.pCountry, TI_FALSE);


    case REGULATORY_DOMAIN_DFS_CHANNELS_RANGE:
        TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setParam, DFS_CHANNELS_RANGE, min = %d, max = %d, \n", pParam->content.DFS_ChannelRange.minDFS_channelNum, pParam->content.DFS_ChannelRange.maxDFS_channelNum);

        if ((pParam->content.DFS_ChannelRange.minDFS_channelNum<A_5G_BAND_MIN_CHANNEL) ||
            (pParam->content.DFS_ChannelRange.maxDFS_channelNum>A_5G_BAND_MAX_CHANNEL) ||
            pParam->content.DFS_ChannelRange.minDFS_channelNum > pParam->content.DFS_ChannelRange.maxDFS_channelNum)
        {
            TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR, "regulatoryDomain_setParam, Bad DFS_CHANNELS_RANGE, min = %d, max = %d, \n", pParam->content.DFS_ChannelRange.minDFS_channelNum, pParam->content.DFS_ChannelRange.maxDFS_channelNum);
            return TI_NOK;
        }
        pRegulatoryDomain->minDFS_channelNum = (TI_UINT8)pParam->content.DFS_ChannelRange.minDFS_channelNum;
        pRegulatoryDomain->maxDFS_channelNum = (TI_UINT8)pParam->content.DFS_ChannelRange.maxDFS_channelNum;

        break;

	default:
TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR, "Set param, Params is not supported, %d\n\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

	return TI_OK;
}

TI_STATUS regulatoryDomain_getParam(TI_HANDLE hRegulatoryDomain,
									paramInfo_t	*pParam)
{
	regulatoryDomain_t	*pRegulatoryDomain = (regulatoryDomain_t *)hRegulatoryDomain;

    /* Check if country code is still valid */
    regulatoryDomain_checkCountryCodeExpiry(pRegulatoryDomain);

	switch(pParam->paramType)
	{

	case REGULATORY_DOMAIN_TX_POWER_LEVEL_TABLE_PARAM:
        {
            TFwInfo *pFwInfo = TWD_GetFWInfo (pRegulatoryDomain->hTWD);
            os_memoryCopy(pRegulatoryDomain->hOs, 
                          (void *)&pParam->content.powerLevelTable,
                          (void *)pFwInfo->txPowerTable,
                          sizeof(pFwInfo->txPowerTable));
        }
		break;

	case REGULATORY_DOMAIN_MANAGEMENT_CAPABILITY_ENABLED_PARAM:
		pParam->content.spectrumManagementEnabled = pRegulatoryDomain->spectrumManagementEnabled;
		break;
		
	case REGULATORY_DOMAIN_ENABLED_PARAM:
		pParam->content.regulatoryDomainEnabled = pRegulatoryDomain->regulatoryDomainEnabled;
		break;

	case REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES:
		{
			channelCapabilityReq_t	channelCapabilityReq;

			channelCapabilityReq.band = pParam->content.channelCapabilityReq.band;
			channelCapabilityReq.channelNum = pParam->content.channelCapabilityReq.channelNum;
			channelCapabilityReq.scanOption = pParam->content.channelCapabilityReq.scanOption;

            regulatoryDomain_getChannelCapability(pRegulatoryDomain, channelCapabilityReq, &pParam->content.channelCapabilityRet);
		}
		break;

	case REGULATORY_DOMAIN_POWER_CAPABILITY_PARAM:
		/* power capability is only applicable when spectrum management is active (802.11h) */ 
		if(pRegulatoryDomain->spectrumManagementEnabled)
		{
            regulatoryDomain_getPowerTableMinMax (pRegulatoryDomain, &pParam->content.powerCapability);
		}
		else
		{
			return TI_NOK;
		}
		break;

	case REGULATORY_DOMAIN_IS_CHANNEL_SUPPORTED:
		/* checking if the channel is supported */
		pParam->content.bIsChannelSupprted  = 
			regulatoryDomain_isChannelSupprted(pRegulatoryDomain, pParam->content.channel);
			
		break;

	case REGULATORY_DOMAIN_ALL_SUPPORTED_CHANNELS:
		{
			ERadioBand	band = pParam->content.siteMgrRadioBand;
			regulatoryDomain_buildDefaultListOfChannelsPerBand(pRegulatoryDomain, band, &pParam->content.supportedChannels.sizeOfList);
		    pParam->content.supportedChannels.listOfChannels = pRegulatoryDomain->pDefaultChannels;
		}
		break;

	case REGULATORY_DOMAIN_CURRENT_TX_POWER_IN_DBM_PARAM:

            {
			TTwdParamInfo		tparam;
			/* Get last configured Tx power from TWD */
			tparam.paramType = TWD_TX_POWER_PARAM_ID;
			TWD_GetParam(pRegulatoryDomain->hTWD, &tparam);

			pParam->content.desiredTxPower = tparam.content.halCtrlTxPowerDbm;

TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_getParam, CURRENT_TX_POWER_IN_DBM  = %d\n", 							   pParam->content.desiredTxPower);
            }

        break;

    case REGULATORY_DOMAIN_COUNTRY_PARAM:
        {   
            /* This case is used as an inner function of the driver to retrieve the full IE of the country */
            TI_BOOL bBand_2_4 = siteMgr_isCurrentBand24(pRegulatoryDomain->hSiteMgr);

            if (bBand_2_4)
            {
                if (pRegulatoryDomain->country_2_4_WasFound)
                {
                    pParam->content.pCountry = &pRegulatoryDomain->country24;
                }
                else    /* Do not use the Inforamtion */
                {
                    pParam->content.pCountry = NULL;
                }
            }   /* band 5.0 */
            else
            { 
                if (pRegulatoryDomain->country_5_WasFound)
                {
                   pParam->content.pCountry = &pRegulatoryDomain->country5;
                }
                else    /* Do not use the Inforamtion */
                {
                    pParam->content.pCountry = NULL;
                }
            }
        }
        break;
        
	case REGULATORY_DOMAIN_COUNTRY_2_4_PARAM:
		/* Getting only country string */

        if (pRegulatoryDomain->country_2_4_WasFound)
        {
            os_memoryCopy(pRegulatoryDomain->hOs, (void*)pParam->content.pCountryString, (void*)pRegulatoryDomain->country24.countryIE.CountryString, DOT11_COUNTRY_STRING_LEN);
        }
        else
        {
            pParam->content.pCountryString[0] = '\0';
        }
 		break;

	case REGULATORY_DOMAIN_COUNTRY_5_PARAM:
		/* Getting only country string */

        if (pRegulatoryDomain->country_5_WasFound)
        {
            os_memoryCopy(pRegulatoryDomain->hOs, (void*)pParam->content.pCountryString, (void*)pRegulatoryDomain->country5.countryIE.CountryString, DOT11_COUNTRY_STRING_LEN);
        }
        else
        {
            pParam->content.pCountryString[0] = '\0';
        }
		break;

    case REGULATORY_DOMAIN_DFS_CHANNELS_RANGE:
        TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_getParam, DFS_CHANNELS_RANGE, min = %d, max = %d, \n", pRegulatoryDomain->minDFS_channelNum, pRegulatoryDomain->maxDFS_channelNum);
        pParam->content.DFS_ChannelRange.minDFS_channelNum = pRegulatoryDomain->minDFS_channelNum;
        pParam->content.DFS_ChannelRange.maxDFS_channelNum = pRegulatoryDomain->maxDFS_channelNum;

        break;

	case REGULATORY_DOMAIN_IS_COUNTRY_FOUND:

		pParam->content.bIsCountryFound = 
			 regulatoryDomain_isCountryFound(pRegulatoryDomain, pParam->content.eRadioBand);
		
		break;

    case REGULATORY_DOMAIN_IS_DFS_CHANNEL:

        if ((pRegulatoryDomain->spectrumManagementEnabled) && /* 802.11h is enabled */
            (RADIO_BAND_5_0_GHZ == pParam->content.tDfsChannel.eBand) && /* band is 5 GHz */
            (pRegulatoryDomain->minDFS_channelNum <= pParam->content.tDfsChannel.uChannel) && /* channel is within DFS range */
            (pRegulatoryDomain->maxDFS_channelNum >= pParam->content.tDfsChannel.uChannel))
        {
            pParam->content.tDfsChannel.bDfsChannel = TI_TRUE;
        }
        else
        {
            pParam->content.tDfsChannel.bDfsChannel = TI_FALSE;
        }
        break;

    case REGULATORY_DOMAIN_TIME_TO_COUNTRY_EXPIRY:
        /* if a country was found for either band */
        if ((pRegulatoryDomain->country_2_4_WasFound) || (pRegulatoryDomain->country_5_WasFound))
        {
            paramInfo_t *pParam2;
            TI_STATUS   connStatus;
            TI_UINT32   uCurrentTS = os_timeStampMs (pRegulatoryDomain->hOs);

            pParam2 = (paramInfo_t *)os_memoryAlloc(pRegulatoryDomain->hOs, sizeof(paramInfo_t));
            if (!pParam2)
            {
                return TI_NOK;
            }

            /* Get connection status */
            pParam2->paramType = SITE_MGR_CURRENT_SSID_PARAM;
            connStatus = siteMgr_getParam (pRegulatoryDomain->hSiteMgr, pParam2);
            os_memoryFree(pRegulatoryDomain->hOs, pParam2, sizeof(paramInfo_t));

            /* if we are connected, return 0 */
            if (connStatus != NO_SITE_SELECTED_YET)
            {
                pParam->content.uTimeToCountryExpiryMs = 0;
            }
            else
            {
                /* 
                 * if country already expired (shouldn't happen as we are checking it at the top of
                 * get_param, but just in case...
                 */
                if ((uCurrentTS - pRegulatoryDomain->uLastCountryReceivedTS) > pRegulatoryDomain->uTimeOutToResetCountryMs)
                {
                    pParam->content.uTimeToCountryExpiryMs = 0;
                }
                else
                {
                    pParam->content.uTimeToCountryExpiryMs = 
                        pRegulatoryDomain->uTimeOutToResetCountryMs - (uCurrentTS - pRegulatoryDomain->uLastCountryReceivedTS);
                }
            }
        }
        else
        {
            pParam->content.uTimeToCountryExpiryMs = 0;
        }
        break;

	default:
		TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_WARNING, "Get param, Params is not supported, %d\n\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

	return TI_OK;
}

/************************************************************************
 *                        regulatoryDomain_destroy						*
 ************************************************************************
DESCRIPTION: regulatoryDomain module destroy function, called by the config mgr in the destroy phase 
				performs the following:
				-	Free all memory allocated by the module
                                                                                                   
INPUT:      hRegulatoryDomain	-	regulatoryDomain handle.		


OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS regulatoryDomain_destroy(TI_HANDLE hRegulatoryDomain)
{
	regulatoryDomain_t	*pRegulatoryDomain = (regulatoryDomain_t *)hRegulatoryDomain;

	if (pRegulatoryDomain == NULL)
		return TI_OK;

    os_memoryFree(pRegulatoryDomain->hOs, pRegulatoryDomain, sizeof(regulatoryDomain_t));

	return TI_OK;
}

/************************************************************************
 *                        regulatoryDomain_isCountryFound						*
 ************************************************************************
DESCRIPTION: This function returns the validity of Country according to band
                                                                                                   
INPUT:      hRegulatoryDomain	-	regulatoryDomain handle.   
            radioBand           - the desired band 	


OUTPUT:		

RETURN:     TI_TRUE - if country IE was found according to the band.
            TI_FALSE - otherwise.

************************************************************************/
TI_BOOL regulatoryDomain_isCountryFound(regulatoryDomain_t  *pRegulatoryDomain, ERadioBand radioBand)
{

    if(radioBand == RADIO_BAND_2_4_GHZ)
    {
            return pRegulatoryDomain->country_2_4_WasFound;
    }
    else
    {
        return pRegulatoryDomain->country_5_WasFound;
    }

}

/***********************************************************************
 *                       setSupportedChannelsAccording2CountryIe									
 ***********************************************************************
DESCRIPTION:	Called when beacon/Probe Response with Country IE
				is found.
				The function sets the local countryIE per band with the CountryIE
				 that was detected in the last passive scan.
				 It is assumed that only one Country IE per band is allowed.
				 If Country is changed when the TNET is loaded, it should
				 be re-loaded in order to re-config the new Country domain.
                                                                                                   
INPUT:      hRegulatoryDomain	-	RegulatoryDomain handle.
			pCountry	-	pointer to the detected country IE.

OUTPUT:		

RETURN:     TI_OK - New country code was set (or the same one was already configured)
            TI_NOK - The new country code could not be set

************************************************************************/
static TI_STATUS setSupportedChannelsAccording2CountryIe(regulatoryDomain_t *pRegulatoryDomain, TCountry* pCountry, TI_BOOL band_2_4)
{
	channelCapability_t *pSupportedChannels;
	TI_UINT8				channelIndex;
	TI_UINT8				tripletChannelIndex, tripletChannelCnt;
	TI_UINT8				channelStep, numberOfChannels, minChannelNumber, maxChannelNumber;

	
	if (!pRegulatoryDomain->regulatoryDomainEnabled)
	{  /* Ignore the Country IE if 802.11d is disabled */
		return TI_NOK;
	}

    /* Check if the country code should be reset */
    regulatoryDomain_checkCountryCodeExpiry(pRegulatoryDomain);

	if( band_2_4 == TI_TRUE )
	{
		if (pRegulatoryDomain->country_2_4_WasFound)
		{	/* Do not update new Country IE */
			if (os_memoryCompare(pRegulatoryDomain->hOs, (void *)&pCountry->countryIE, (void *)&pRegulatoryDomain->country24.countryIE, sizeof(dot11_countryIE_t)))
			{
TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_WARNING, "setSupportedChannelsAccording2CountryIe different Country, cur=, new=\n");
            	return TI_NOK;
            }
            else    /* Same IE - just mark the TS and return TI_OK */
            {
                /* Mark the time of the received country IE */                
                pRegulatoryDomain->uLastCountryReceivedTS = os_timeStampMs(pRegulatoryDomain->hOs);
                return TI_OK;
            }
		}
		pRegulatoryDomain->country_2_4_WasFound = TI_TRUE;
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_2_4;
		channelStep = BG_24G_BAND_CHANNEL_HOPS;
		maxChannelNumber = NUM_OF_CHANNELS_24;
		minChannelNumber = BG_24G_BAND_MIN_CHANNEL;
		numberOfChannels = NUM_OF_CHANNELS_24;
		/* save the country IE */
		os_memoryCopy(pRegulatoryDomain->hOs, (void*)&pRegulatoryDomain->country24, (void *)pCountry, sizeof(TCountry));

        TRACE3(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "Country 2.4 =%c%c%c\n",pRegulatoryDomain->country24.countryIE.CountryString[0], pRegulatoryDomain->country24.countryIE.CountryString[1], pRegulatoryDomain->country24.countryIE.CountryString[2]);

	}
	else    /* band 5.0 */
	{
		if (pRegulatoryDomain->country_5_WasFound)
		{	/* Do not update new Country IE if the IE is the same*/
			if (os_memoryCompare(pRegulatoryDomain->hOs, (void *)&pCountry->countryIE, (void *)&pRegulatoryDomain->country5.countryIE, sizeof(dot11_countryIE_t)))
			{
TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_WARNING, "setSupportedChannelsAccording2CountryIe different Country, cur=, new=\n");
            	return TI_NOK;
            }
            else    /* Same IE - just mark the TS and return TI_OK */
            {
                /* Mark the time of the received country IE */                
                pRegulatoryDomain->uLastCountryReceivedTS = os_timeStampMs(pRegulatoryDomain->hOs);
                return TI_OK;
            }
		}
		pRegulatoryDomain->country_5_WasFound = TI_TRUE;
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_5;
		channelStep = A_5G_BAND_CHANNEL_HOPS;
		maxChannelNumber = A_5G_BAND_MAX_CHANNEL;
		minChannelNumber = A_5G_BAND_MIN_CHANNEL;
		numberOfChannels = A_5G_BAND_NUM_CHANNELS;
		/* save the country IE */
		os_memoryCopy(pRegulatoryDomain->hOs, (void*)&pRegulatoryDomain->country5, (void*)pCountry, sizeof(TCountry));

        TRACE3(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "Country 5 =%c%c%c\n",pRegulatoryDomain->country5.countryIE.CountryString[0], pRegulatoryDomain->country5.countryIE.CountryString[1], pRegulatoryDomain->country5.countryIE.CountryString[2]);
	}

    /*
     * New Country IE was saved. Now - update the last received TS and ScanControlTable
     */

    /* Mark the time of the received country IE */                
    pRegulatoryDomain->uLastCountryReceivedTS = os_timeStampMs(pRegulatoryDomain->hOs);

	/* First clear the validity of all channels 
		Overwrite the ScanControlTable */
	for (channelIndex=0; channelIndex<numberOfChannels; channelIndex++)
	{
		pSupportedChannels[channelIndex].channelValidityActive = TI_FALSE;
		pSupportedChannels[channelIndex].channelValidityPassive = TI_FALSE;
		pSupportedChannels[channelIndex].bChanneInCountryIe = TI_FALSE;
		pSupportedChannels[channelIndex].uMaxTxPowerDomain = MIN_TX_POWER; 	
	}
    
	tripletChannelCnt = (pCountry->len - DOT11_COUNTRY_STRING_LEN) / 3;
	/* set validity of the channels according to the band (2.4 or 5) */
	for( tripletChannelIndex = 0; tripletChannelIndex < tripletChannelCnt ; tripletChannelIndex++)
	{
		TI_UINT8	firstChannelNumInTriplet;
		
		firstChannelNumInTriplet = pCountry->countryIE.tripletChannels[tripletChannelIndex].firstChannelNumber;
TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "firstChannelNumInTriplet=%d,channelStep=%d\n", firstChannelNumInTriplet, channelStep);
		for (channelIndex=0; channelIndex<pCountry->countryIE.tripletChannels[tripletChannelIndex].numberOfChannels; channelIndex++)
		{
			TI_UINT16	channelNumber;

			channelNumber = firstChannelNumInTriplet+(channelIndex*channelStep);
TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "setSupportedChannelsAccording2CountryIe of channel=%d\n", channelNumber);
			
			if (channelNumber <= maxChannelNumber)
			{
				TI_UINT8 	channelIndex4Band;

				channelIndex4Band = (channelNumber-minChannelNumber);
				pSupportedChannels[channelIndex4Band].bChanneInCountryIe = TI_TRUE;
				pSupportedChannels[channelIndex4Band].channelValidityPassive = TI_TRUE;
				pSupportedChannels[channelIndex4Band].channelValidityActive = TI_TRUE;

				/* set the TX power in DBM/10 units */
			    pSupportedChannels[channelIndex4Band].uMaxTxPowerDomain = 
					DBM2DBMDIV10(pCountry->countryIE.tripletChannels[tripletChannelIndex].maxTxPowerLevel);

TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channel = %d uMaxTxPowerDomain=%d\n", 										channelNumber, pSupportedChannels[channelIndex4Band].uMaxTxPowerDomain);
			}
		}
    }

	return TI_OK;
}


/***********************************************************************
 *                        regulatoryDomain_isChannelSupprted									
 ***********************************************************************
DESCRIPTION:	The function checks if the input channel is supported.
                                                                                                   
INPUT:      pRegulatoryDomain	-	RegulatoryDomain pointer.
			channel				-	Channel number.
			

OUTPUT:		

RETURN:     TI_OK if channel is supported, TI_NOK otherwise.

************************************************************************/
static TI_BOOL regulatoryDomain_isChannelSupprted(regulatoryDomain_t *pRegulatoryDomain, TI_UINT8 channel)
{
	TI_UINT8				channelIndex;
	channelCapability_t *pSupportedChannels;

	if (pRegulatoryDomain==NULL)
	{
		return TI_FALSE;
	}

	if ((channel<BG_24G_BAND_MIN_CHANNEL) || (channel>A_5G_BAND_MAX_CHANNEL))
	{
		return TI_FALSE;
	}
	if (channel>=A_5G_BAND_MIN_CHANNEL)
	{
		channelIndex = (channel-A_5G_BAND_MIN_CHANNEL);
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_5;
	}
	else
	{
		channelIndex = (channel-BG_24G_BAND_MIN_CHANNEL);
		if (channelIndex >= NUM_OF_CHANNELS_24)
		{
			TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR,
				   "regulatoryDomain_isChannelSupprted(): 2.4G invalid channel # %u\n", channel );
			return TI_FALSE;
		}
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_2_4;
	}
	if (pRegulatoryDomain->spectrumManagementEnabled 
		&& (channel >= pRegulatoryDomain->minDFS_channelNum) 
        && (channel <= pRegulatoryDomain->maxDFS_channelNum)
		&& ((os_timeStampMs(pRegulatoryDomain->hOs)-pSupportedChannels[channelIndex].timestamp) >=CHANNEL_VALIDITY_TS_THRESHOLD ))
	{	/* If 802.11h is enabled, a DFS channel is valid only for 10 sec
			from the last Beacon/ProbeResponse */
        pSupportedChannels[channelIndex].channelValidityActive = TI_FALSE;
        TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_isChannelSupprted(): CHANNEL_VALIDITY_TS_THRESHOLD !! Disable channel no %d, DFS channel\n", channel );

	}

	return (pSupportedChannels[channelIndex].channelValidityActive);

}

/************************************************************************
 *                        regulatoryDomain_setChannelValidity			*
 ************************************************************************/
/*
*
*
* \b Description: 
*
* This function sets a channel as invalid or valid in the internal Regulatory Domain
 * database. 
*
* \b ARGS:
*
*  I   - pData - pointer to the regDoamin SM context  \n
*  I   - channelNum - the invalid/valid channel number
*  I   - channelValidity - TI_TRUE if channel is valid, TI_FALSE channel is invalid
*
* \b RETURNS:
*
*  None.
*
* 
*************************************************************************/
static void regulatoryDomain_setChannelValidity(regulatoryDomain_t *pRegulatoryDomain, 
												TI_UINT16 channelNum, TI_BOOL channelValidity)
{
	channelCapability_t		*pSupportedChannels;
	TI_UINT8					channelIndex;


	if (pRegulatoryDomain == NULL)
	{
		return;
	}
	if ((channelNum==0 ) || (channelNum>A_5G_BAND_MAX_CHANNEL))
	{
TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR, "regulatoryDomain_setChannelValidity, invalid channelNum=%d \n", channelNum);
		return;
	}
	
	if (channelNum <= NUM_OF_CHANNELS_24)
	{
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_2_4;
		channelIndex = (channelNum-BG_24G_BAND_MIN_CHANNEL);
	}
	else 
	{
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_5;
		channelIndex = (channelNum - A_5G_BAND_MIN_CHANNEL);
	}
	
	if(channelValidity == TI_TRUE)
		if((pSupportedChannels[channelIndex].bChanneInCountryIe == TI_FALSE) && (pRegulatoryDomain->regulatoryDomainEnabled == TI_TRUE))
		{
TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_WARNING, "regulatoryDomain_setChannelValidity: channelNum = %d isn't supported at the Country. wll not set to active!\n", channelNum);
			return;
		}

    TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_setChannelValidity: channelNum=%d, validity=%d \n", channelNum, channelValidity);


	pSupportedChannels[channelIndex].channelValidityActive = channelValidity;
}


/************************************************************************
 *      setSupportedChannelsAccording2ScanControlTable 					*
 ************************************************************************/
/**
*
*
* \b Description: 
*
* This function is called in config and sets the supported channels according to
* the scan control table read from registry and reg domain read from the chip.
*
* \b ARGS:
*
*  I   - pRegulatoryDomain - pointer to the regDoamin SM context  \n
*
* \b RETURNS:
*
*  None.
*
* 
*************************************************************************/
static void setSupportedChannelsAccording2ScanControlTable(regulatoryDomain_t  *pRegulatoryDomain)
{
	TI_UINT8 	channelIndex;
	TI_UINT8	channelMask;

	if (pRegulatoryDomain==NULL)
	{
		return;
	}

    TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "setSupportedChannelsAccording2ScanControlTable \n");

	for (channelIndex=0; channelIndex<NUM_OF_CHANNELS_24; channelIndex++)
	{
		channelMask = pRegulatoryDomain->scanControlTable.ScanControlTable24.tableString[channelIndex];
		pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].bChanneInCountryIe = TI_FALSE;

		/* Calculate Domain Tx Power - channelMask units are in Dbm. */
		pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].uMaxTxPowerDomain = 
						DBM2DBMDIV10(channelMask & MASK_TX_POWER);
		if (channelMask & (MASK_ACTIVE_ALLOWED | MASK_FREQ_ALLOWED))
		{	/* The channel is allowed for Active & Passive scans */
			if (pRegulatoryDomain->regulatoryDomainEnabled)
			{	/* All channels should be invalid for Active scan */
				pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].channelValidityActive = TI_FALSE;
                TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channelIndex=%d is invalid for Active \n", channelIndex+1);
			}
			else
			{
				pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].channelValidityActive = TI_TRUE;
                TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channelIndex=%d is Active valid \n", channelIndex+1);
			}
			
		}
		
		if (channelMask & MASK_FREQ_ALLOWED)
		{	/* The channel is allowed for Passive scan */
			pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].channelValidityPassive = TI_TRUE;
            TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channelIndex=%d is Passive valid \n", channelIndex+1);
		}
		else
		{	/* The channel is not allowed */
			pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].channelValidityPassive = TI_FALSE;
			pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].channelValidityActive = TI_FALSE;
		}
	}

	for (channelIndex=A_5G_BAND_MIN_CHANNEL; channelIndex<A_5G_BAND_MAX_CHANNEL; channelIndex++)
	{	
		TI_UINT8	channelIndexInBand5;

		channelIndexInBand5 = (channelIndex-A_5G_BAND_MIN_CHANNEL);
		channelMask = pRegulatoryDomain->scanControlTable.ScanControlTable5.tableString[channelIndexInBand5];
        TRACE3(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channelIndex=%d, channelIndexInBand5=%d channelMask=%d\n", channelIndex, channelIndexInBand5, channelMask);

		/* Calculate Domain Tx Power - channelMask units are in Dbm. */
		pRegulatoryDomain->supportedChannels_band_5[channelIndexInBand5].uMaxTxPowerDomain = 
			DBM2DBMDIV10(channelMask & MASK_TX_POWER);

		pRegulatoryDomain->supportedChannels_band_5[channelIndexInBand5].bChanneInCountryIe = TI_FALSE;
		if (channelMask & (MASK_ACTIVE_ALLOWED | MASK_FREQ_ALLOWED))
		{	 /* The channel is allowed for Active & Passive scans */
			if (pRegulatoryDomain->regulatoryDomainEnabled)
			{	/* All channels should be invalid for Active scan */
				pRegulatoryDomain->supportedChannels_band_5[channelIndexInBand5].channelValidityActive = TI_FALSE;
                TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channelIndex=%d is invalid for Active \n", channelIndex);
			}
			else
			{
				pRegulatoryDomain->supportedChannels_band_5[channelIndexInBand5].channelValidityActive = TI_TRUE;
                TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channelIndex=%d, channelIndexInBand5=%d, is Active valid \n", channelIndex, channelIndexInBand5);
			}   		
		}
		
		if (channelMask & MASK_FREQ_ALLOWED)
		{	/* The channel is allowed for Passive scan */
			pRegulatoryDomain->supportedChannels_band_5[channelIndexInBand5].channelValidityPassive = TI_TRUE;
            TRACE2(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "channelIndex=%d, channelIndexInBand5=%d, is Passive valid \n", channelIndex, channelIndexInBand5);
		}
		else
		{	/* The channel is not allowed */
			pRegulatoryDomain->supportedChannels_band_5[channelIndexInBand5].channelValidityPassive = TI_FALSE;
			pRegulatoryDomain->supportedChannels_band_5[channelIndexInBand5].channelValidityActive = TI_FALSE;
		}

	}
}


/***********************************************************************
*                        regulatoryDomain_getChannelCapability									
***********************************************************************
DESCRIPTION:	This function returns the channel capability information

INPUT:      pRegulatoryDomain		-	RegulatoryDomain pointer.
			channelCapabilityReq	-	Channels parameters


OUTPUT:		channelCapabilityRet	-   Channel capability information

RETURN:     TI_OK if information was retrieved, TI_NOK otherwise.

************************************************************************/
static TI_STATUS regulatoryDomain_getChannelCapability(regulatoryDomain_t *pRegulatoryDomain, 
													   channelCapabilityReq_t channelCapabilityReq, 
													   channelCapabilityRet_t *channelCapabilityRet)
{
	channelCapability_t		*pSupportedChannels;
	TI_UINT8				channelIndex;
	TI_BOOL					bCountryWasFound, bServingChannel;

	if ((pRegulatoryDomain == NULL) || (channelCapabilityRet == NULL))
	{
		return TI_NOK;
	}

	channelCapabilityRet->channelValidity = TI_FALSE;
	channelCapabilityRet->maxTxPowerDbm = 0;
	if ((channelCapabilityReq.channelNum==0 ) || (channelCapabilityReq.channelNum > A_5G_BAND_MAX_CHANNEL))
	{
        TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR, "regulatoryDomain_getChannelCapability, invalid channelNum=%d \n", channelCapabilityReq.channelNum);
		return TI_NOK;
	}
	
	if (channelCapabilityReq.band==RADIO_BAND_2_4_GHZ)
	{
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_2_4;
		channelIndex = (channelCapabilityReq.channelNum-BG_24G_BAND_MIN_CHANNEL);
		if (channelIndex >= NUM_OF_CHANNELS_24)
		{
			TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR,
				   "regulatoryDomain_getChannelCapability(): 2.4G invalid channel # %u\n", channelCapabilityReq.channelNum );
			return TI_NOK;
		}
		bCountryWasFound = pRegulatoryDomain->country_2_4_WasFound;
	}
	else if (channelCapabilityReq.band==RADIO_BAND_5_0_GHZ)
	{
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_5;
		channelIndex = (channelCapabilityReq.channelNum - A_5G_BAND_MIN_CHANNEL);
		if (channelIndex >= A_5G_BAND_NUM_CHANNELS)
		{
			TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR,
				   "regulatoryDomain_getChannelCapability(): 5G invalid channel # %u\n", channelCapabilityReq.channelNum);
			return TI_NOK;
		}
		bCountryWasFound = pRegulatoryDomain->country_5_WasFound;
	}
	else
	{
		TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR, "regulatoryDomain_getChannelCapability, invalid band=%d \n", channelCapabilityReq.band);
		return TI_NOK;
	}


	/* 
	 * Set channelValidity according to ScanTable and whether 11d is enabled 
	 */
	if (channelCapabilityReq.scanOption == ACTIVE_SCANNING)
	{
		if ( ( pRegulatoryDomain->regulatoryDomainEnabled ) && ( !bCountryWasFound ) )
		{	/* 11d enabled and no country IE was found - set channel to invalid */
			channelCapabilityRet->channelValidity = TI_FALSE;
		}
		else
		{
            paramInfo_t *pParam = (paramInfo_t *)os_memoryAlloc(pRegulatoryDomain->hOs, sizeof(paramInfo_t));
            if (!pParam)
            {
                return TI_NOK;
            }

            channelCapabilityRet->channelValidity = pSupportedChannels[channelIndex].channelValidityActive;
			/*
			 * Set Maximum Tx power for the channel - only for active scanning
			 */ 

			/* Get current channel and check if we are using the same one */
			pParam->paramType = SITE_MGR_CURRENT_CHANNEL_PARAM;
			siteMgr_getParam(pRegulatoryDomain->hSiteMgr, pParam);

			bServingChannel = ( pParam->content.siteMgrCurrentChannel == channelCapabilityReq.channelNum ?
								TI_TRUE : TI_FALSE );

			channelCapabilityRet->maxTxPowerDbm = regulatoryDomain_getMaxPowerAllowed(pRegulatoryDomain, 
				channelCapabilityReq.channelNum, 
				channelCapabilityReq.band, 
				bServingChannel);
            os_memoryFree(pRegulatoryDomain->hOs, pParam, sizeof(paramInfo_t));
		}
	}
	else	/* Passive scanning */
	{
		if ( ( pRegulatoryDomain->regulatoryDomainEnabled ) && ( !bCountryWasFound ) )
		{	/* 11d enabled and no country IE was found - set channel to valid for passive scan */
			channelCapabilityRet->channelValidity = TI_TRUE;
		}
	else
	{
		channelCapabilityRet->channelValidity = pSupportedChannels[channelIndex].channelValidityPassive;
	}
	}
	
	if (pRegulatoryDomain->spectrumManagementEnabled 
		&& (channelCapabilityReq.scanOption == ACTIVE_SCANNING)
        && (channelCapabilityReq.channelNum >= pRegulatoryDomain->minDFS_channelNum) 
        && (channelCapabilityReq.channelNum <= pRegulatoryDomain->maxDFS_channelNum)
		&& ((os_timeStampMs(pRegulatoryDomain->hOs)-pSupportedChannels[channelIndex].timestamp) >=CHANNEL_VALIDITY_TS_THRESHOLD ))
	{	/* If 802.11h is enabled, a DFS channel is valid only for 10 sec
			from the last Beacon/ProbeResponse */
        pSupportedChannels[channelIndex].channelValidityActive = TI_FALSE;
        channelCapabilityRet->channelValidity = TI_FALSE;
        TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_getChannelCapability(): CHANNEL_VALIDITY_TS_THRESHOLD !!! Disable channel no %d, DFS channel\n", channelCapabilityReq.channelNum  );
    }

    TRACE4(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, " Channel num= %d, scan option=%d validity = %d, TX power = %d \n", 					channelCapabilityReq.channelNum, 					channelCapabilityReq.scanOption,					channelCapabilityRet->channelValidity,					channelCapabilityRet->maxTxPowerDbm);
	return TI_OK;

}


static void regulatoryDomain_updateChannelsTs(regulatoryDomain_t *pRegulatoryDomain, TI_UINT8 channel)
{
	TI_UINT8				channelIndex;
	channelCapability_t *pSupportedChannels;

	if (pRegulatoryDomain==NULL)
	{
		return;
	}

	if ((channel<BG_24G_BAND_MIN_CHANNEL) || (channel>A_5G_BAND_MAX_CHANNEL))
	{
		return;
	}

	if (channel>=A_5G_BAND_MIN_CHANNEL)
	{
		channelIndex = (channel-A_5G_BAND_MIN_CHANNEL);
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_5;
	}
	else
	{
		channelIndex = (channel-BG_24G_BAND_MIN_CHANNEL);
		if (channelIndex >= NUM_OF_CHANNELS_24)
		{
			TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_ERROR,
				   "regulatoryDomain_updateChannelsTs(): 2.4G invalid channel # %u\n", channel );
			return;
		}
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_2_4;
	}
	
	if((pSupportedChannels[channelIndex].bChanneInCountryIe == TI_FALSE) && (pRegulatoryDomain->regulatoryDomainEnabled == TI_TRUE))
  	{
		TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_WARNING, "regulatoryDomain_updateChannelsTs: channelNum = %d isn't supported at the Country. wll not set to active!\n", channel);
  		return;
  	}

	pSupportedChannels[channelIndex].timestamp = os_timeStampMs(pRegulatoryDomain->hOs);
	pSupportedChannels[channelIndex].channelValidityActive = TI_TRUE;

}

/***********************************************************************
 *              regulatoryDomain_updateCurrTxPower								
 ***********************************************************************
DESCRIPTION: Called when new Tx power should be calculated and configured.
			 Check if we are already joined to BSS/IBSS, calculate 
			 new Tx power and configure it to TWD.
				
INPUT:		pRegulatoryDomain	- regulatoryDomain pointer.
			
RETURN:     TI_OK - New value was configured to TWD, TI_NOK - Can't configure value
			TX_POWER_SET_SAME_VALUE - Same value was already configured.

************************************************************************/
static TI_STATUS regulatoryDomain_updateCurrTxPower(regulatoryDomain_t	*pRegulatoryDomain)
{
    paramInfo_t			*pParam;
	TI_STATUS			eStatus;
	TTwdParamInfo		*pTwdParam;
	TI_UINT8			uCurrChannel, uNewTxPower;

    pParam = (paramInfo_t *)os_memoryAlloc(pRegulatoryDomain->hOs, sizeof(paramInfo_t));
    if (!pParam)
    {
        return TI_NOK;
    }

    pTwdParam = (TTwdParamInfo *)os_memoryAlloc(pRegulatoryDomain->hOs, sizeof(TTwdParamInfo));
    if (!pTwdParam)
    {
        os_memoryFree(pRegulatoryDomain->hOs, pParam, sizeof(paramInfo_t));
        return TI_NOK;
    }

	/* Get the current channel, and update TWD with the changed */
	pParam->paramType = SITE_MGR_CURRENT_CHANNEL_PARAM;
	eStatus = siteMgr_getParam(pRegulatoryDomain->hSiteMgr, pParam);

	if ( eStatus != TI_OK )
	{
		/* We are not joined yet - no meaning for new Tx power */
        TRACE0(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_updateCurrTxPower, No site selected yet\n");
        os_memoryFree(pRegulatoryDomain->hOs, pParam, sizeof(paramInfo_t));
        os_memoryFree(pRegulatoryDomain->hOs, pTwdParam, sizeof(TTwdParamInfo));
		return TI_NOK;
	}
	/* Save current channel */
	uCurrChannel = pParam->content.siteMgrCurrentChannel;

	/* Get the current channel, and update TWD with the changed */
	pParam->paramType = 	SITE_MGR_RADIO_BAND_PARAM;
	siteMgr_getParam(pRegulatoryDomain->hSiteMgr, pParam);

	/* Calculate maximum Tx power for the serving channel */
	uNewTxPower = regulatoryDomain_getMaxPowerAllowed(pRegulatoryDomain, uCurrChannel, 
													  pParam->content.siteMgrRadioBand, TI_TRUE);
    os_memoryFree(pRegulatoryDomain->hOs, pParam, sizeof(paramInfo_t));	
	
	/* Verify that the Temporary TX Power Control doesn't violate the TX Power Constraint */
	pRegulatoryDomain->uTemporaryTxPower = TI_MIN(pRegulatoryDomain->uDesiredTemporaryTxPower, uNewTxPower);


    TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "regulatoryDomain_updateCurrTxPower, Write to TWD = %d \n", uNewTxPower);

    pTwdParam->paramType = TWD_TX_POWER_PARAM_ID;

    /* set TWD according to Temporary Tx Power Enable flag */
    if (TI_TRUE == pRegulatoryDomain->bTemporaryTxPowerEnable)
    {
        pTwdParam->content.halCtrlTxPowerDbm = pRegulatoryDomain->uTemporaryTxPower;
    }
    else
    {
        pTwdParam->content.halCtrlTxPowerDbm = uNewTxPower;
    }

    eStatus = TWD_SetParam(pRegulatoryDomain->hTWD, pTwdParam);
    os_memoryFree(pRegulatoryDomain->hOs, pTwdParam, sizeof(TTwdParamInfo));
	return eStatus;
}

/***********************************************************************
 *                        regulatoryDomain_checkCountryCodeExpiry									
 ***********************************************************************
DESCRIPTION: Check & Reset the country code that was detected earlier.
                Reseting country code will be done when the STA was not connected for 
                a certain amount of time, and no country code was received in that period (from the same country).
                This scenario might indicate that the STA has moved to a different country.
                                                                                                   
INPUT:      pRegulatoryDomain	-	Regulatory Domain handle.

OUTPUT:		updating country code if necessary.

RETURN:     

************************************************************************/
void regulatoryDomain_checkCountryCodeExpiry(regulatoryDomain_t *pRegulatoryDomain)
{
    paramInfo_t *pParam;
    TI_STATUS   connStatus;
    TI_UINT32   uCurrentTS = os_timeStampMs(pRegulatoryDomain->hOs);

    if ((pRegulatoryDomain->country_2_4_WasFound) || (pRegulatoryDomain->country_5_WasFound))
    {
        pParam = (paramInfo_t *)os_memoryAlloc(pRegulatoryDomain->hOs, sizeof(paramInfo_t));
        if (!pParam)
        {
            return;
        }
        /* Get connection status */
        pParam->paramType = SITE_MGR_CURRENT_SSID_PARAM;
        connStatus      = siteMgr_getParam(pRegulatoryDomain->hSiteMgr, pParam);

         /* If (uTimeOutToResetCountryMs has elapsed && we are not connected)
                 delete the last country code received */
        if (((uCurrentTS - pRegulatoryDomain->uLastCountryReceivedTS) > pRegulatoryDomain->uTimeOutToResetCountryMs) &&
            (connStatus == NO_SITE_SELECTED_YET))
        {
            TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, ", Reset country code after %d Ms\n",(uCurrentTS - pRegulatoryDomain->uLastCountryReceivedTS));

            /* Reset country codes */
            pRegulatoryDomain->country_2_4_WasFound = TI_FALSE;
            pRegulatoryDomain->country_5_WasFound = TI_FALSE;
            
            /* Restore default values of the scan control table */
            setSupportedChannelsAccording2ScanControlTable(pRegulatoryDomain); 
        }
        os_memoryFree(pRegulatoryDomain->hOs, pParam, sizeof(paramInfo_t));
    }
}

/***********************************************************************
*              regulatoryDomain_getMaxPowerAllowed								
***********************************************************************
DESCRIPTION: Get the maximum tx power allowed for the given channel.
				The final value is constructed by:
				1) User max value
				2) Domain restriction - 11d country code IE
				3) 11h power constraint - only on serving channel
				4) XCC TPC - only on serving channel

RETURN:     Max power in Dbm/10 for the given channel

************************************************************************/
static TI_UINT8 regulatoryDomain_getMaxPowerAllowed(regulatoryDomain_t	*pRegulatoryDomain,
												 TI_UINT8				uChannel,
												 ERadioBand		eBand,
												 TI_BOOL				bServingChannel)
{
	channelCapability_t	*pSupportedChannels;
	TI_UINT8				 uChannelIndex, uTxPower;

	if( eBand == RADIO_BAND_2_4_GHZ)
	{
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_2_4;
		uChannelIndex = uChannel - BG_24G_BAND_MIN_CHANNEL;
	}
	else
	{
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_5;
		uChannelIndex = uChannel - A_5G_BAND_MIN_CHANNEL;
	}

	/* We'll start with the "Domain restriction - 11d country code IE" */
	uTxPower = pSupportedChannels[uChannelIndex].uMaxTxPowerDomain;

	if ( bServingChannel)
	{
		if (pRegulatoryDomain->uPowerConstraint < uTxPower)
		{
			/* When 802.11h is disabled, uPowerConstraint is 0 anyway */
			uTxPower -= pRegulatoryDomain->uPowerConstraint;
		}
        
        /* Take XCC limitation too */
        uTxPower = TI_MIN(uTxPower, pRegulatoryDomain->uExternTxPowerPreferred);

	}

	/* Now make sure we are not exceeding the user maximum */
	uTxPower = TI_MIN(uTxPower, pRegulatoryDomain->uUserMaxTxPower);

TRACE3(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, " uChannel = %d bServingChannel = %d uTxPower = %d \n", uChannel, bServingChannel, uTxPower);

	return uTxPower;
}


static void regulatoryDomain_buildDefaultListOfChannelsPerBand(regulatoryDomain_t *pRegulatoryDomain, ERadioBand band, TI_UINT8 *listSize)
{
	TI_UINT8				channelIndex;
	TI_UINT8				numberOfChannels, minChannelNumber;
	channelCapability_t	*pSupportedChannels;
	TI_UINT8				maxSupportedChannels=0;

	if ( (pRegulatoryDomain==NULL) || (listSize==NULL))
	{
		return;
	}

	if( band == RADIO_BAND_2_4_GHZ)
	{
		minChannelNumber = BG_24G_BAND_MIN_CHANNEL;
		numberOfChannels = NUM_OF_CHANNELS_24;
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_2_4;
	}
	else
	{
		minChannelNumber = A_5G_BAND_MIN_CHANNEL;
		numberOfChannels = A_5G_BAND_NUM_CHANNELS;
		pSupportedChannels = pRegulatoryDomain->supportedChannels_band_5;
	}


	for (channelIndex=0; channelIndex<numberOfChannels; channelIndex++)
	{
		if (pSupportedChannels[channelIndex].channelValidityPassive)
		{
			pRegulatoryDomain->pDefaultChannels[maxSupportedChannels] = channelIndex+minChannelNumber;
TRACE1(pRegulatoryDomain->hReport, REPORT_SEVERITY_INFORMATION, "Channel num %d is supported \n", pRegulatoryDomain->pDefaultChannels[maxSupportedChannels]);
			maxSupportedChannels++;
		}
	}
 
	*listSize = maxSupportedChannels;
    
}

/***********************************************************************
*              regulatoryDomain_getPowerTableMinMax								
***********************************************************************
DESCRIPTION: Find the Tx-power-level table min & max values.
			 The table is made of 4 power levels and 5 bands/sub-bands. 

RETURN:     void
************************************************************************/
static void regulatoryDomain_getPowerTableMinMax (regulatoryDomain_t *pRegulatoryDomain,
                                                  powerCapability_t  *pPowerCapability)
{
    TFwInfo  *pFwInfo = TWD_GetFWInfo (pRegulatoryDomain->hTWD);
	TI_UINT8	i;

    /* Init the min (max) to the opposite edge so the table values are below (above) this edge */ 
	pPowerCapability->minTxPower = MAX_TX_POWER;
	pPowerCapability->maxTxPower = MIN_TX_POWER;

	/* Find Min and Max values of the table */
	for (i = 0; i < NUMBER_OF_SUB_BANDS_E; i++)
	{
		pPowerCapability->minTxPower = TI_MIN (pPowerCapability->minTxPower, 
                                               pFwInfo->txPowerTable[i][NUM_OF_POWER_LEVEL-1]);
		pPowerCapability->maxTxPower = TI_MAX (pPowerCapability->maxTxPower, 
                                               pFwInfo->txPowerTable[i][0]);
	}
}

/* for debug */
void regDomainPrintValidTables(TI_HANDLE hRegulatoryDomain)
{
	regulatoryDomain_t  *pRegulatoryDomain = (regulatoryDomain_t *)hRegulatoryDomain;
	TI_UINT16 channelIndex;

	for (channelIndex=0; channelIndex<NUM_OF_CHANNELS_24; channelIndex++)
	{
		if (pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].channelValidityPassive)
			WLAN_OS_REPORT(("channel num =%d is valid for passive \n", channelIndex+1));
		if (pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].channelValidityActive)
		{
			WLAN_OS_REPORT(("channel =%d is valid for active TX power=%d\n", 
				channelIndex+1, pRegulatoryDomain->supportedChannels_band_2_4[channelIndex].uMaxTxPowerDomain));
		}
	}

	for (channelIndex=0; channelIndex<A_5G_BAND_NUM_CHANNELS; channelIndex++)
	{
		TI_UINT8	channelNum;
		channelNum = channelIndex+A_5G_BAND_MIN_CHANNEL;
		if (pRegulatoryDomain->supportedChannels_band_5[channelIndex].channelValidityPassive)
			WLAN_OS_REPORT(("channel =%d is valid for passive \n", channelNum));
		if (pRegulatoryDomain->supportedChannels_band_5[channelIndex].channelValidityActive)
		{
			WLAN_OS_REPORT(("channel =%d is valid for active TX power=%d\n", 
				channelNum,pRegulatoryDomain->supportedChannels_band_5[channelIndex].uMaxTxPowerDomain));
		}
		}

	WLAN_OS_REPORT(("11h PowerConstraint = %d, XCC TPC = %d, User  = %d\n", 
		pRegulatoryDomain->uPowerConstraint, pRegulatoryDomain->uExternTxPowerPreferred,
		pRegulatoryDomain->uUserMaxTxPower));

}
