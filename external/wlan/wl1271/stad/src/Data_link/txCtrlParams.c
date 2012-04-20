/*
 * txCtrlParams.c
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

/*******************************************************************************/
/*                                                                             */
/*      MODULE: txCtrlParams.c                                                 */
/*    PURPOSE:  The txCtrl module parameters handling.                         */
/*              This is a part of the txCtrl module (using the same object).   */
/*                                                                             */
/*******************************************************************************/

#define __FILE_ID__  FILE_ID_57
#include "tidef.h"
#include "report.h"
#include "paramOut.h"
#include "osApi.h"
#include "timer.h"
#include "EvHandler.h"
#include "txCtrl.h"





/***********************************************************************
 *                        calcCreditFromTimer
 ***********************************************************************
DESCRIPTION:    This function is called when credit calculation timer
				is expired. it calculate the credit for the admission ctrl
				credit algorithm


INPUT:	    hTxCtrl         - handle to the ts data object
            bTwdInitOccured - Indicates if TWDriver recovery occured since timer started 

OUTPUT:     None

RETURN:     void
************************************************************************/
static void calcCreditFromTimer(TI_HANDLE hTxCtrl, TI_BOOL bTwdInitOccured)
{
	OS_802_11_THRESHOLD_CROSS_INDICATION_PARAMS	mediumTimeCross;
	txCtrl_t	*pTxCtrl = (txCtrl_t *)hTxCtrl;
	TI_UINT32		ac;
	TI_INT32		prevCredit;
	TI_INT32		highCreditThreshold;
	TI_INT32		lowCreditThreshold;
	TI_INT32		usageRatio;
	TI_INT32		currUsage;
	TI_INT32		prevUsage;
	TI_UINT32		currentTimeStamp = os_timeStampMs(pTxCtrl->hOs);  /* get current time stamp */
	
	/* 
	 *  For each AC under admission control calculate the new usage and credit time,
	 *     and send events if a threshold is crossed. 
	 */
	for(ac = 0 ; ac < MAX_NUM_OF_AC ; ac++)
	{
		/* check if this queue is under admission ctrl operation */
		if(pTxCtrl->mediumTime[ac] == 0)
		{
TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, ": ac = %d mediumTime = 0 \n", ac);
			
			continue;
		}

		/* in case of wraparound */
		if(currentTimeStamp < pTxCtrl->lastCreditCalcTimeStamp[ac])
			pTxCtrl->lastCreditCalcTimeStamp[ac] = 0;
		
		/* store prev credit */
		prevCredit = pTxCtrl->credit[ac];

		/* Calculate the medium usage ratio:    totalUsedTime / mediumTime * 1000
		   Note that since the totalUsedTime is in usec and not msec we don't multiply by 1000.	 */
		usageRatio = pTxCtrl->totalUsedTime[ac] / pTxCtrl->mediumTime[ac];

		/* calculate credit */
		pTxCtrl->credit[ac] += (currentTimeStamp - pTxCtrl->lastCreditCalcTimeStamp[ac]) - usageRatio;
		
		/* update last time stamp */
		pTxCtrl->lastCreditCalcTimeStamp[ac] = currentTimeStamp;

		/* in case credit is bigger than mediumTime -> set credit to medium time */
		if (pTxCtrl->credit[ac] > (TI_INT32)(pTxCtrl->mediumTime[ac]) )
			pTxCtrl->credit[ac] = pTxCtrl->mediumTime[ac];

       TRACE2(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "credit = %d  | TotalUsedTime = %d\n", pTxCtrl->credit[ac], pTxCtrl->totalUsedTime[ac]/1000);

		/* Check medium-usage threshold cross events */
		/*********************************************/
		/*
		 * The medium-usage events are defined as follows:
		 * The high threshold triggers event only when crossed upward (traffic increased above threshold).
		 * The low threshold triggers event only when crossed downward (traffic decreased below threshold).
		 * Thus, the two thresholds provide hysteresis and prevent multiple triggering.
		 * The high threshold should be greater than the low threshold. 
		 * 
		 *   Note:	The driver doesn't delay traffic even if violating the usage limit!
		 *			It only indicates the user application about the thresholds crossing.
		 */

		highCreditThreshold = (TI_INT32)((pTxCtrl->mediumTime[ac])*(pTxCtrl->highMediumUsageThreshold[ac])/100); 
		lowCreditThreshold  = (TI_INT32)((pTxCtrl->mediumTime[ac])*(pTxCtrl->lowMediumUsageThreshold[ac])/100);

		/* The credit is getting more negative as we get closer to the medium usage limit, so we invert
		     it before comparing to the thresholds (lower credit means higher usage). */
		currUsage = -pTxCtrl->credit[ac];
		prevUsage = -prevCredit;

		/* crossing below the low threshold */
		if ( (currUsage < lowCreditThreshold) && (prevUsage >= lowCreditThreshold) )
		{
			/* send event */
			mediumTimeCross.uAC = ac;
			mediumTimeCross.uHighOrLowThresholdFlag = (TI_UINT32)LOW_THRESHOLD_CROSS;
			mediumTimeCross.uAboveOrBelowFlag = (TI_UINT32)CROSS_BELOW;

			EvHandlerSendEvent(pTxCtrl->hEvHandler, IPC_EVENT_MEDIUM_TIME_CROSS, 
				(TI_UINT8 *)&mediumTimeCross, sizeof(OS_802_11_THRESHOLD_CROSS_INDICATION_PARAMS));

            TRACE3(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "crossed below low threshold !!! prevUsage = %d, currUsage = %d, lowCreditThreshold = %d\n",				prevUsage, currUsage, lowCreditThreshold);
		}
		
		/* crossing above the high threshold */
		else if ( (currUsage > highCreditThreshold) && (prevUsage <= highCreditThreshold) )
		{
			/* send event */
			mediumTimeCross.uAC = ac;
			mediumTimeCross.uHighOrLowThresholdFlag = (TI_UINT32)HIGH_THRESHOLD_CROSS;
			mediumTimeCross.uAboveOrBelowFlag = (TI_UINT32)CROSS_ABOVE;

			EvHandlerSendEvent(pTxCtrl->hEvHandler, IPC_EVENT_MEDIUM_TIME_CROSS, 
				(TI_UINT8 *)&mediumTimeCross, sizeof(OS_802_11_THRESHOLD_CROSS_INDICATION_PARAMS));

            TRACE3(pTxCtrl->hReport, REPORT_SEVERITY_INFORMATION, "crossed above high threshold !!! prevUsage = %d, currUsage = %d, highCreditThreshold = %d\n",				prevUsage, currUsage, highCreditThreshold);
		}

		/* reset totalUsedTime */
		pTxCtrl->totalUsedTime[ac] = 0;
	}
}


/****************************************************************************
 *                      updateDataPktPrototype()
 ****************************************************************************
 * DESCRIPTION:	Updates the data packet prototype values according to 
				changed parameters (e.g. rate policy change). 
 ****************************************************************************/
static void updateDataPktPrototype(txCtrl_t *pTxCtrl)
{
	pTxCtrl->dataPktDescAttrib = pTxCtrl->txSessionCount << TX_ATTR_OFST_SESSION_COUNTER;
}


/***************************************************************************
*                       txCtrlParams_resetCounters                               
****************************************************************************
* DESCRIPTION:  Reset the tx data module counters
*
* INPUTS:       hTxCtrl - the object
*
* OUTPUT:
*
* RETURNS:
***************************************************************************/
void txCtrlParams_resetCounters(TI_HANDLE hTxCtrl)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    os_memoryZero(pTxCtrl->hOs, &pTxCtrl->txDataCounters, sizeof(TTxDataCounters) * MAX_NUM_OF_AC);
    os_memoryZero(pTxCtrl->hOs, &pTxCtrl->SumTotalDelayUs, sizeof(pTxCtrl->SumTotalDelayUs));
	pTxCtrl->currentConsecutiveRetryFail = 0;
}


/***************************************************************************
*                       txCtrlParams_RegNotif                                    *
****************************************************************************/
TI_HANDLE txCtrlParams_RegNotif(TI_HANDLE hTxCtrl, TI_UINT16 EventMask, GeneralEventCall_t CallBack,
						  TI_HANDLE context, TI_UINT32 Cookie)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
    if (!hTxCtrl)
        return NULL;
    return DistributorMgr_Reg(pTxCtrl->TxEventDistributor,EventMask,(TI_HANDLE)CallBack,context,Cookie);
}


/***************************************************************************
*                       txCtrlParams_AddToNotifMask                              *
****************************************************************************/
TI_STATUS txCtrlParams_AddToNotifMask(TI_HANDLE hTxCtrl, TI_HANDLE Notifh, TI_UINT16 EventMask)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
    if (!hTxCtrl)
        return TI_NOK;
    return DistributorMgr_AddToMask(pTxCtrl->TxEventDistributor, Notifh, EventMask);
}


/***************************************************************************
*                       txCtrlParams_UnRegNotif                                  *
****************************************************************************/
TI_STATUS txCtrlParams_UnRegNotif(TI_HANDLE hTxCtrl, TI_HANDLE RegEventHandle)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    if (!hTxCtrl)
        return TI_NOK;

    return (DistributorMgr_UnReg(pTxCtrl->TxEventDistributor,RegEventHandle));
}


/***********************************************************************
 *                        txCtrlParams_setAdmissionCtrlParams
 ***********************************************************************
DESCRIPTION:    This function is called for add/delete a tspec in order
				to update parameters.

INPUT:			hTxCtrl - handale to the ts data object
				acId - the AC of the tspec
				mediumTime	- tha alocated medium time for this UP 
				minimumPHYRate - the min phy rate to send a packet of this UP
				admFlag - indicate if the its addition or deletion of tspec 

OUTPUT:     None

RETURN:     void
************************************************************************/
TI_STATUS txCtrlParams_setAdmissionCtrlParams(TI_HANDLE hTxCtrl, TI_UINT8 acId, TI_UINT16 mediumTime, 
											  TI_UINT32 minimumPHYRate, TI_BOOL admFlag)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
	TI_UINT32	i;

	if(admFlag == TI_TRUE) 
	{
		/* tspec added */
		pTxCtrl->mediumTime[acId] = mediumTime;
        pTxCtrl->admissionState[acId] = AC_ADMITTED;
		pTxCtrl->useAdmissionAlgo[acId] = TI_TRUE;
		pTxCtrl->lastCreditCalcTimeStamp[acId] = os_timeStampMs(pTxCtrl->hOs);
		pTxCtrl->credit[acId] = mediumTime;
	}
	else
	{
		/* tspaec deleted */
		pTxCtrl->mediumTime[acId] = 0;
        pTxCtrl->admissionState[acId] = AC_NOT_ADMITTED;
		pTxCtrl->useAdmissionAlgo[acId] = TI_FALSE;
		pTxCtrl->lastCreditCalcTimeStamp[acId] = 0;
		pTxCtrl->credit[acId] = 0;
	}

	/* Update the Tx queues mapping after admission change. */
	txCtrl_UpdateQueuesMapping (hTxCtrl);
	
	/* If the timer was not enabled in registry than we will never set it */
	if (pTxCtrl->bCreditCalcTimerEnabled)
	{
    	/* enable disable credit calculation timer */
    	for (i = 0; i < MAX_NUM_OF_AC; i++)
    	{
    		if (pTxCtrl->useAdmissionAlgo[i])
    		{
    			if (!pTxCtrl->bCreditCalcTimerRunning)
    			{
    				pTxCtrl->bCreditCalcTimerRunning = TI_TRUE;
                    tmr_StartTimer (pTxCtrl->hCreditTimer,
                                    calcCreditFromTimer,
                                    (TI_HANDLE)pTxCtrl,
                                    pTxCtrl->creditCalculationTimeout, 
                                    TI_TRUE);
    			}
    			
    			return TI_OK;
    		}
    	}
    
    	/* in all queues useAdmissionAlgo is not TRUE, so stop timer if running */
        if (pTxCtrl->bCreditCalcTimerRunning)
        {
            tmr_StopTimer (pTxCtrl->hCreditTimer);
            pTxCtrl->bCreditCalcTimerRunning = TI_FALSE;
        }
    }

	return TI_OK;
}


/***************************************************************************
*                           txCtrlParams_getParam                           
****************************************************************************
* DESCRIPTION:  Get a specific parameter by an external user application.
*
* OUTPUT:       pParamInfo - structure which include the value of
*               the requested parameter
***************************************************************************/
TI_STATUS txCtrlParams_getParam(TI_HANDLE hTxCtrl, paramInfo_t *pParamInfo)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
	TI_UINT32 ac;

    if(pTxCtrl == NULL)  /* check handle validity */
    {
        return TI_NOK;
    }

    switch (pParamInfo->paramType)
    {
    case TX_CTRL_COUNTERS_PARAM:
		/* Convert total-delays units from usec to mSec. */
		for(ac = 0 ; ac < MAX_NUM_OF_AC ; ac++)
		{
			pTxCtrl->txDataCounters[ac].SumTotalDelayMs = pTxCtrl->SumTotalDelayUs[ac] / 1000;
		}
        os_memoryCopy( pTxCtrl->hOs, pParamInfo->content.pTxDataCounters, &(pTxCtrl->txDataCounters[0]), 
                       sizeof(TTxDataCounters) * MAX_NUM_OF_AC);
		pParamInfo->paramLength = sizeof(TTxDataCounters) * MAX_NUM_OF_AC;
        break;

	case TX_CTRL_GET_DATA_FRAME_COUNTER:
		pParamInfo->content.txPacketsCount = 0;
		for (ac = 0; ac < MAX_NUM_OF_AC; ac++)
        	pParamInfo->content.txPacketsCount += pTxCtrl->txDataCounters[ac].XmitOk;				
        break;

	case TX_CTRL_REPORT_TS_STATISTICS:
		ac = pParamInfo->content.tsMetricsCounters.acID;
		os_memoryCopy(pTxCtrl->hOs, 
					  pParamInfo->content.tsMetricsCounters.pTxDataCounters, 
					  &(pTxCtrl->txDataCounters[ac]), 
					  sizeof(TTxDataCounters));
		os_memoryZero(pTxCtrl->hOs, &(pTxCtrl->txDataCounters[ac]), sizeof(TTxDataCounters));
		break;

	case TX_CTRL_GENERIC_ETHERTYPE:
		pParamInfo->content.txGenericEthertype = pTxCtrl->genericEthertype;
		break;


    default:
        TRACE0(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, ": PARAMETER NOT SUPPORTED\n");
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}


/***************************************************************************
*                           txCtrlParams_setParam                           
****************************************************************************
* DESCRIPTION:  Set a specific parameter by an external user application.
*
* INPUTS:       hTxCtrl - the object
*               pParamInfo - structure which include the value to set for
*               the requested parameter
***************************************************************************/
TI_STATUS txCtrlParams_setParam(TI_HANDLE hTxCtrl, paramInfo_t *pParamInfo)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
	TI_UINT8 acID;

    if(pTxCtrl == NULL)  /* check handle validity */
    {
        return TI_NOK;
    }
	
    switch (pParamInfo->paramType)
    {
    case TX_CTRL_SET_MEDIUM_USAGE_THRESHOLD:
		acID = (TI_UINT8)pParamInfo->content.txDataMediumUsageThreshold.uAC;
		if(acID < MAX_NUM_OF_AC)
		{
			pTxCtrl->highMediumUsageThreshold[acID] = 
				pParamInfo->content.txDataMediumUsageThreshold.uHighThreshold;
			pTxCtrl->lowMediumUsageThreshold[acID] = 
				pParamInfo->content.txDataMediumUsageThreshold.uLowThreshold;
		}
		else
TRACE1(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, ": Wrong AC (AC=%d > 3)\n", acID);
		break;

    case TX_CTRL_GET_MEDIUM_USAGE_THRESHOLD:
		/* Note: SET operation is used for GET, because AC parameter should be supplied from Utility- 
	         Adapter to driver (copy of user supplied block of data is only performed in SetParam calls). */
		acID = (TI_UINT8)pParamInfo->content.txDataMediumUsageThreshold.uAC;
		pParamInfo->content.txDataMediumUsageThreshold.uHighThreshold = pTxCtrl->highMediumUsageThreshold[acID];
		pParamInfo->content.txDataMediumUsageThreshold.uLowThreshold  = pTxCtrl->lowMediumUsageThreshold[acID];
		break;

    case TX_CTRL_POLL_AP_PACKETS_FROM_AC:
       TRACE0(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, ": Poll-AP is not supported in this version!!\n");
       return PARAM_NOT_SUPPORTED;

	case TX_CTRL_RESET_COUNTERS_PARAM:
		txCtrlParams_resetCounters(hTxCtrl);
		break;

	case TX_CTRL_GENERIC_ETHERTYPE:
        pTxCtrl->genericEthertype = pParamInfo->content.txGenericEthertype;
		{
				paramInfo_t param;
				param.paramType = RX_DATA_GENERIC_ETHERTYPE_PARAM;
				param.content.rxGenericEthertype = pTxCtrl->genericEthertype;
				rxData_setParam(pTxCtrl->hRxData, &param);
		}
		break;


    default:
        TRACE0(pTxCtrl->hReport, REPORT_SEVERITY_ERROR, ": PARAMETER NOT SUPPORTED\n");
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}


/***********************************************************************
 *                        txCtrlParams_setBssId
 ***********************************************************************
DESCRIPTION:    Update the BSS-ID.
************************************************************************/
void txCtrlParams_setBssId (TI_HANDLE hTxCtrl, TMacAddr *pCurrBssId)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	MAC_COPY (pTxCtrl->currBssId, *pCurrBssId);
}


/***********************************************************************
 *                        txCtrlParams_setBssType
 ***********************************************************************
DESCRIPTION:    Update the BSS type.
************************************************************************/
void txCtrlParams_setBssType (TI_HANDLE hTxCtrl, ScanBssType_e currBssType)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->currBssType = currBssType;
}


/***********************************************************************
 *                        txCtrlParams_setQosHeaderConverMode
 ***********************************************************************
DESCRIPTION:    Update the BSS type.
************************************************************************/
void txCtrlParams_setQosHeaderConverMode (TI_HANDLE hTxCtrl, EHeaderConvertMode  headerConverMode)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->headerConverMode = headerConverMode;

	updateDataPktPrototype(pTxCtrl);  /* Needed due to QoS mode change. */
}

/** 
 * \fn     txCtrlParams_SetHtControl() 
 * \brief  Update The HT Control Field on txCtrl module. 
 * 
 * \note    
 * \param  hTxCtrl - the hTxCtrl handle.
 * \param  pHtCapabilitiesIe - input structure.
 * \return TI_OK on success or TI_NOK on failure  
 * \sa     
 */ 
TI_STATUS txCtrlParams_SetHtControl (TI_HANDLE hTxCtrl, TtxCtrlHtControl *pHtControl)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
 
    pTxCtrl->tTxCtrlHtControl.bHtEnable = pHtControl->bHtEnable;

    return TI_OK;
}

/***********************************************************************
 *                        txCtrlParams_setCurrentPrivacyInvokedMode
 ***********************************************************************
DESCRIPTION:    Update the current privacy invoked mode.
************************************************************************/
void txCtrlParams_setCurrentPrivacyInvokedMode (TI_HANDLE hTxCtrl, TI_BOOL currentPrivacyInvokedMode)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->currentPrivacyInvokedMode = currentPrivacyInvokedMode;
}


/***********************************************************************
 *                        txCtrlParams_setEapolEncryptionStatus
 ***********************************************************************
DESCRIPTION:    Update the Eapol Encryption Status.
************************************************************************/
void txCtrlParams_setEapolEncryptionStatus (TI_HANDLE hTxCtrl, TI_BOOL eapolEncryptionStatus)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->eapolEncryptionStatus = eapolEncryptionStatus;
}


/***********************************************************************
 *                        txCtrlParams_setEncryptionFieldSizes
 ***********************************************************************
DESCRIPTION:    Update the encryption field size for the header padding.
************************************************************************/
void txCtrlParams_setEncryptionFieldSizes (TI_HANDLE hTxCtrl, TI_UINT8 encryptionFieldSize)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->encryptionFieldSize = encryptionFieldSize;
}


/***********************************************************************
 *                        txCtrlParams_getCurrentEncryptionInfo
 ***********************************************************************
DESCRIPTION:    Provide the current encryption mode and padding size.
************************************************************************/
void txCtrlParams_getCurrentEncryptionInfo (TI_HANDLE hTxCtrl, 
                                            TI_BOOL    *pCurrentPrivacyInvokedMode,
                                            TI_UINT8   *pEncryptionFieldSize)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    *pCurrentPrivacyInvokedMode = pTxCtrl->currentPrivacyInvokedMode;
    *pEncryptionFieldSize = pTxCtrl->encryptionFieldSize;
}


/***********************************************************************
 *                        txCtrlParams_GetTxRate
 ***********************************************************************
DESCRIPTION:    Provide the last successfull data packet Tx rate.
************************************************************************/
ERate txCtrlParams_GetTxRate (TI_HANDLE hTxCtrl)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    return pTxCtrl->eCurrentTxRate;
}


/***********************************************************************
 *                        txCtrlParams_setAcAdmissionStatus
 ***********************************************************************
DESCRIPTION:    Update the AC admission status - required or not and admitted or not.
				Update also the queues mapping in case it should change.
************************************************************************/
void txCtrlParams_setAcAdmissionStatus (TI_HANDLE hTxCtrl, 
                                        TI_UINT8 ac, 
                                        EAdmissionState admissionRequired,
										ETrafficAdmState admissionState)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->admissionRequired[ac] = admissionRequired;
	pTxCtrl->admissionState[ac]    = admissionState;

	/* Update the Tx queues mapping after admission change. */
	txCtrl_UpdateQueuesMapping (hTxCtrl);
}


/***********************************************************************
 *                        txCtrlParams_setAcMsduLifeTime
 ***********************************************************************
DESCRIPTION:    Update the AC MSDU lifetime. The units are TUs (1024 usec).
************************************************************************/
void txCtrlParams_setAcMsduLifeTime (TI_HANDLE hTxCtrl, TI_UINT8 ac, TI_UINT32 uMsduLifeTimeTu)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->aMsduLifeTimeTu[ac] = (TI_UINT16)uMsduLifeTimeTu;
}


/***********************************************************************
 *                        txCtrlParams_setAcAckPolicy
 ***********************************************************************
DESCRIPTION:    Update the AC Ack policy.
************************************************************************/
void txCtrlParams_setAcAckPolicy (TI_HANDLE hTxCtrl, TI_UINT8 ac, AckPolicy_e ackPolicy)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->ackPolicy[ac] = ackPolicy;
}


/***********************************************************************
 *                     txCtrlParams_updateMgmtRateAttributes
 ***********************************************************************
DESCRIPTION:    Update per AC the rate policy for Mgmnt packets or IBSS BCAST packets.
************************************************************************/
void txCtrlParams_updateMgmtRateAttributes(TI_HANDLE hTxCtrl, TI_UINT8 ratePolicyId, TI_UINT8 ac)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->mgmtRatePolicy[ac] = ratePolicyId;
}


/***********************************************************************
 *                     txCtrlParams_updateDataRateAttributes
 ***********************************************************************
DESCRIPTION:    Update per AC the rate policy for regular data packets (excluding IBSS BCAST packets).
************************************************************************/
void txCtrlParams_updateDataRateAttributes(TI_HANDLE hTxCtrl, TI_UINT8 ratePolicyId, TI_UINT8 ac)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->dataRatePolicy[ac] = ratePolicyId;
}


/***********************************************************************
 *                     txCtrlParams_updateTxSessionCount
 ***********************************************************************
DESCRIPTION:    Update the current Tx-session index configured to FW.
************************************************************************/
void txCtrlParams_updateTxSessionCount(TI_HANDLE hTxCtrl, TI_UINT16 txSessionCount)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

	pTxCtrl->txSessionCount = txSessionCount;

	updateDataPktPrototype(pTxCtrl);
}




/********************************************************************************
*																				*
*                       DEBUG  FUNCTIONS  IMPLEMENTATION						*
*																				*
*********************************************************************************/

#ifdef TI_DBG

/***********************************************************************
 *                     txCtrlParams_printInfo
 ***********************************************************************
DESCRIPTION:    Print module internal information.
************************************************************************/
void txCtrlParams_printInfo(TI_HANDLE hTxCtrl)
{
#ifdef REPORT_LOG
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    WLAN_OS_REPORT(("-------------- Tx-Ctrl Module Information --------------\n"));
    WLAN_OS_REPORT(("========================================================\n"));

    WLAN_OS_REPORT(("ACs Mapping:\n"));
    WLAN_OS_REPORT(("------------\n"));
    WLAN_OS_REPORT(("admissionRequired[3,2,1,0]   =  %d,   %d,   %d,   %d\n", 
		pTxCtrl->admissionRequired[3], pTxCtrl->admissionRequired[2],
		pTxCtrl->admissionRequired[1], pTxCtrl->admissionRequired[0]));
    WLAN_OS_REPORT(("admissionState[3,2,1,0]      =  %d,   %d,   %d,   %d\n", 
		pTxCtrl->admissionState[3], pTxCtrl->admissionState[2],
		pTxCtrl->admissionState[1], pTxCtrl->admissionState[0]));
    WLAN_OS_REPORT(("highestAdmittedAc[3,2,1,0]   =  %d,   %d,   %d,   %d\n", 
		pTxCtrl->highestAdmittedAc[3], pTxCtrl->highestAdmittedAc[2],
		pTxCtrl->highestAdmittedAc[1], pTxCtrl->highestAdmittedAc[0]));
    WLAN_OS_REPORT(("admittedAcToTidMap[3,2,1,0]  =  0x%x, 0x%x, 0x%x, 0x%x\n", 
		pTxCtrl->admittedAcToTidMap[3], pTxCtrl->admittedAcToTidMap[2],
		pTxCtrl->admittedAcToTidMap[1], pTxCtrl->admittedAcToTidMap[0]));
    WLAN_OS_REPORT(("busyAcBitmap                 = 0x%x\n", pTxCtrl->busyAcBitmap));
    WLAN_OS_REPORT(("busyTidBitmap                = 0x%x\n", pTxCtrl->busyTidBitmap));
    WLAN_OS_REPORT(("--------------------------------------------------------\n"));

    WLAN_OS_REPORT(("Tx Attributes:\n"));
    WLAN_OS_REPORT(("--------------\n"));
    WLAN_OS_REPORT(("mgmtRatePolicy[3,2,1,0]      =  %d,   %d,   %d,   %d\n", 
		pTxCtrl->mgmtRatePolicy[3], pTxCtrl->mgmtRatePolicy[2],
		pTxCtrl->mgmtRatePolicy[1], pTxCtrl->mgmtRatePolicy[0]));
    WLAN_OS_REPORT(("dataRatePolicy[3,2,1,0]      =  %d,   %d,   %d,   %d\n", 
		pTxCtrl->dataRatePolicy[3], pTxCtrl->dataRatePolicy[2],
		pTxCtrl->dataRatePolicy[1], pTxCtrl->dataRatePolicy[0]));
    WLAN_OS_REPORT(("dataPktDescAttrib            = 0x%x\n", pTxCtrl->dataPktDescAttrib));
    WLAN_OS_REPORT(("--------------------------------------------------------\n"));

    WLAN_OS_REPORT(("Parameters:\n"));
    WLAN_OS_REPORT(("----------\n"));
    WLAN_OS_REPORT(("headerConverMode             = %d\n", pTxCtrl->headerConverMode));
    WLAN_OS_REPORT(("currentPrivacyInvokedMode    = %d\n", pTxCtrl->currentPrivacyInvokedMode));
    WLAN_OS_REPORT(("eapolEncryptionStatus        = %d\n", pTxCtrl->eapolEncryptionStatus));
    WLAN_OS_REPORT(("encryptionFieldSize          = %d\n", pTxCtrl->encryptionFieldSize));
    WLAN_OS_REPORT(("currBssType                  = %d\n", pTxCtrl->currBssType));
    WLAN_OS_REPORT(("========================================================\n\n"));
#endif
}


/***********************************************************************
 *                     txCtrlParams_printDebugCounters
 ***********************************************************************
DESCRIPTION:    Print Tx statistics debug counters.
************************************************************************/
void txCtrlParams_printDebugCounters(TI_HANDLE hTxCtrl)
{
#ifdef REPORT_LOG
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;
    TI_UINT32 ac;

    WLAN_OS_REPORT(("-------------- Tx-Ctrl Statistics Per AC ---------------\n"));
    WLAN_OS_REPORT(("========================================================\n"));

    WLAN_OS_REPORT(("---------- Packets Sent To Tx-Ctrl ---------------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumPktsSent[ac]));

    WLAN_OS_REPORT(("---------- Number of Queue-Stop (BP) -------------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumPktsBackpressure[ac]));

    WLAN_OS_REPORT(("---------- Number of AC Busy (Requeue pkt) -------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumPktsBusy[ac]));

    WLAN_OS_REPORT(("---------- Packets Sent to Xfer ------------------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumPktsXfered[ac]));

    WLAN_OS_REPORT(("----------- Xfer rc = Success --------------------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumPktsSuccess[ac]));

    WLAN_OS_REPORT(("----------- Xfer rc = Pending --------------------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumPktsPending[ac]));

    WLAN_OS_REPORT(("----------- Xfer rc = Error ----------------------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumPktsError[ac]));

    WLAN_OS_REPORT(("----------- Number of Tx-Complete Packets --------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumTxCmplt[ac]));

    WLAN_OS_REPORT(("----------- Number of Tx-Complete Packets TI_OK -----------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumTxCmpltOk[ac]));

    WLAN_OS_REPORT(("----------- Number of Tx-Complete Packets Fails --------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumTxCmpltError[ac]));

    WLAN_OS_REPORT(("----------- Number of Tx-Complete Bytes TI_OK -------------\n"));
    for(ac = 0; ac < MAX_NUM_OF_AC; ac++)
        WLAN_OS_REPORT(("AC %d = %d\n", ac, pTxCtrl->dbgCounters.dbgNumTxCmpltOkBytes[ac]));
    WLAN_OS_REPORT(("--------------------------------------------------------\n"));

    WLAN_OS_REPORT(("Total Number of Xfer-Complete Events = %d\n", pTxCtrl->dbgCounters.dbgNumXferCmplt));
    WLAN_OS_REPORT(("Total Number of Xfer-Pending  Events = %d\n", 
		pTxCtrl->dbgCounters.dbgNumPktsPending[0] +	pTxCtrl->dbgCounters.dbgNumPktsPending[1] +
		pTxCtrl->dbgCounters.dbgNumPktsPending[2] +	pTxCtrl->dbgCounters.dbgNumPktsPending[3]));
    WLAN_OS_REPORT(("========================================================\n\n"));
#endif
}


/***************************************************************************
*                       txCtrlParams_resetDbgCounters                       
****************************************************************************
* DESCRIPTION:  Reset the tx data module debug counters
***************************************************************************/
void txCtrlParams_resetDbgCounters(TI_HANDLE hTxCtrl)
{
    txCtrl_t *pTxCtrl = (txCtrl_t *)hTxCtrl;

    os_memoryZero(pTxCtrl->hOs, &pTxCtrl->dbgCounters, sizeof(txDataDbgCounters_t));
}



#endif   /* TI_DBG */
